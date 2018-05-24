/*
 * libtimeseries
 *
 * Philipp Winter, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2018 The Regents of the University of California.
 *
 * This file is part of libtimeseries.
 *
 * libtimeseries is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libtimeseries is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtimeseries.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

#include <librdkafka/rdkafka.h>

#include "timeseries.h"

/** Convenience macro to deserialize a simple variable from a byte array.
 *
 * @param buf           pointer to the buffer (will be updated)
 * @param len           total length of the buffer
 * @param read          the number of bytes already read from the buffer
 *                      (will be updated)
 * @param to            the variable to deserialize
 */
#define DESERIALIZE_VAL(buf, len, read, to)                                    \
  do {                                                                         \
    if (((len) - (read)) < sizeof(to)) {                                       \
      LOG_ERROR("Not enough bytes left to read.");                             \
      break;                                                                   \
    }                                                                          \
    memcpy(&(to), (buf), sizeof(to));                                          \
    read += sizeof(to);                                                        \
    buf += sizeof(to);                                                         \
} while (0)

/** Convenience macro for INFO log messages.
 */
#define LOG_INFO(...)                                                          \
  do {                                                                         \
    fprintf(stderr, "INFO ");                                                  \
    log_msg(__VA_ARGS__);                                                      \
  } while (0)

/** Convenience macro for ERROR log messages.
 */
#define LOG_ERROR(...)                                                         \
  do {                                                                         \
    fprintf(stderr, "ERROR ");                                                 \
    log_msg(__VA_ARGS__);                                                      \
  } while (0)

// Macros related to our key package statistics.
#define STATS_METRIC_PREFIX      "systems.services.tsk"
#define STATS_TIMESERIES_BACKEND "kafka"
#define STATS_INTERVAL_NOW       ((time(NULL) / stats_interval) * stats_interval)

// When passed as an argument to maybe_flush(), it forces the function to flush.
#define FORCE_FLUSH 0

// Our time series backend.
#define TIMESERIES_BACKEND "dbats"

// The protocol version that we expect.
#define TSKBATCH_VERSION 0

// Number of header bytes that we skip when parsing an rskmessage.
#define HEADER_MAGIC_LEN 8

// Buffer length to hold key package keys.
#define KEY_BUF_LEN 200
#define MIN_KEY_LEN 90
#define MAX_KEY_LEN 110

// Default values if nothing else is provided over the command line.
#define DEFAULT_STATS_INTERVAL    60
#define DEFAULT_OFFSET            "earliest"
#define DEFAULT_CONSUMER_GROUP_ID "c-tsk-proxy-test"
#define DEFAULT_CHANNEL           "active.ping-slash24.team-1.slash24"
#define DEFAULT_TOPIC_PREFIX      "tsk-production"
#define DEFAULT_KAFKA_ARGS        "-b loki.caida.org:9092,"                    \
                                  "riddler.caida.org:9092,"                    \
                                  "penguin.caida.org:9092 "                    \
                                  "-p tsk-production -c systems.1min"

typedef struct kafka_config {

  char *broker;
  char *group_id;
  char *topic_prefix;

  // The channel to subscribe, e.g., "active.ping-slash24.team-1.slash24".
  char *channel;

  // Either "latest" or "earliest".
  char *offset;
} kafka_config_t;

// References to our two timeseries objects.
static timeseries_t *timeseries = NULL;
static timeseries_t *stats_timeseries = NULL;

// Key packages for our active probing data and for statistics.
static timeseries_kp_t *kp = NULL;
static timeseries_kp_t *stats_kp = NULL;

// Statistics-related variables.
static char *stats_key_prefix = NULL;
static int stats_interval = 0;
static int stats_time = 0;

// Set to 1 when we catch a SIGINT.
volatile sig_atomic_t shutdown_proxy = 0;

/** Handles SIGINT gracefully and shuts down */
static void catch_sigint(int sig)
{
  shutdown_proxy++;
  signal(sig, catch_sigint);
}

char *graphite_safe_node(char *str)
{
  int i = 0;
  int str_len = strlen(str);

  for (i = 0; i < str_len; i++) {
    if (str[i] == '.') {
      str[i] = '-';
    }
  }

  return str;
}

void log_msg(const char *format, ...)
{
  va_list args;
  char time_str[30];
  struct tm *tmp;
  time_t t;

  time(&t);
  tmp = localtime(&t);
  strftime(time_str, 30, "%F %T", tmp);

  va_start(args, format);
  fprintf(stderr, "[%s] ", time_str);
  vfprintf(stderr, format, args);
  va_end(args);
}

void inc_stat(char *stats_key_suffix, int value)
{
  int key_id = 0;

  char *stats_key = NULL;

  asprintf(&stats_key, "%s.%s", stats_key_prefix, stats_key_suffix);

  if ((key_id = timeseries_kp_get_key(stats_kp, stats_key)) == -1) {
    key_id = timeseries_kp_add_key(stats_kp, stats_key);
  } else {
    timeseries_kp_enable_key(stats_kp, key_id);
  }
  timeseries_kp_set(stats_kp, key_id, value);

  free(stats_key);
}

void parse_key_value(char **buf, size_t *len_read, const int buflen)
{
  uint16_t keylen = 0;
  uint64_t value = 0;
  int key_id = 0;
  static char key[KEY_BUF_LEN];

  // Get 2-byte key length.
  DESERIALIZE_VAL(*buf, buflen, *len_read, keylen);
  keylen = ntohs(keylen);
  assert(keylen >= MIN_KEY_LEN && keylen <= MAX_KEY_LEN);

  // Get variable-length key.
  strncpy(key, *buf, keylen);
  // We have to 0-terminate the string ourselves.
  key[keylen] = 0;
  *buf += keylen;
  *len_read += keylen;

  // Get value.
  DESERIALIZE_VAL(*buf, buflen, *len_read, value);
  value = be64toh(value);

  // Write key:val pair to key package.
  if ((key_id = timeseries_kp_get_key(kp, key)) == -1) {
    key_id = timeseries_kp_add_key(kp, key);
  } else {
    timeseries_kp_enable_key(kp, key_id);
  }
  LOG_INFO("setting key %s (key_id=%d) to %d\n", key, key_id, value);
  timeseries_kp_set(kp, key_id, value);
}

static void maybe_flush(int flush_time)
{
  static int current_time = 0;

  if (current_time == 0) {
    current_time = flush_time;
  }

  if (flush_time == 0 || flush_time != current_time) {
    if (flush_time == 0) {
      LOG_INFO("Forcing a flush.\n");
    }
    LOG_INFO("Flushing key packages at %d with %d keys enabled (%d total).\n",
        current_time, timeseries_kp_enabled_size(kp), timeseries_kp_size(kp));
    inc_stat("flush_cnt", 1);
    inc_stat("flushed_key_cnt", timeseries_kp_enabled_size(kp));

    if (timeseries_kp_flush(kp, current_time) == -1) {
      LOG_ERROR("Could not flush key packages.\n");
    }

    assert(timeseries_kp_enabled_size(kp) == 0);
    current_time = flush_time;
  }
}

static void maybe_flush_stats()
{
  int now = STATS_INTERVAL_NOW;

  if (now >= (stats_time + stats_interval)) {
    LOG_INFO("Flushing stats at %d.\n", stats_time);
    if (timeseries_kp_flush(stats_kp, stats_time) == -1) {
      LOG_ERROR("Could not flush stats key packages.\n");
    }
    stats_time = now;
  }
}

void process_message(rd_kafka_message_t *rkmessage)
{
  size_t len_read = 0;
  uint8_t version = 0;
  uint32_t time = 0;
  uint16_t chanlen = 0;
  char chanbuf[34] = {0};
  char *buf = rkmessage->payload;

  // Skip the string "TSKBATCH".
  len_read += HEADER_MAGIC_LEN;
  buf += HEADER_MAGIC_LEN;

  // Extract version (1 byte), time (4 bytes), and chanlen (2 bytes), and
  // chanbuf (variable-length).
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, version);
  if (version != TSKBATCH_VERSION) {
    LOG_ERROR("Expected version %d but got %d.\n", TSKBATCH_VERSION, version);
    return;
  }
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, time);
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, chanlen);
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, chanbuf);

  maybe_flush(ntohl(time));
  inc_stat("messages_cnt", 1);
  inc_stat("messages_bytes", rkmessage->len);

  while (len_read < rkmessage->len) {
    parse_key_value(&buf, &len_read, rkmessage->len);
  }
}

rd_kafka_t *init_kafka(kafka_config_t *cfg)
{
  rd_kafka_t *kafka = NULL;
  rd_kafka_conf_t *conf = NULL;
  rd_kafka_topic_partition_list_t *topics = NULL;
  rd_kafka_resp_err_t err;
  char errstr[512];
  char *topic_name = NULL;
  char *consumer_group = NULL;

  // todo: free these vars
  asprintf(&topic_name, "%s.%s", cfg->topic_prefix, cfg->channel);
  asprintf(&consumer_group, "%s.%s", cfg->group_id, topic_name);

  LOG_INFO("Attempting to initialize kafka.\n");

  LOG_INFO("topic name: %s\n", topic_name);
  topics = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(topics, topic_name, -1);

  // Configure the initial log offset.
  conf = rd_kafka_conf_new();
  if (rd_kafka_conf_set(conf, "auto.offset.reset", cfg->offset,
                        errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    LOG_ERROR("Could not set log offset because: %s\n", errstr);
    goto error;
  }

  // Set our group ID.
  if (rd_kafka_conf_set(conf, "group.id", consumer_group,
                        errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    LOG_ERROR("Could not set group ID because: %s\n", errstr);
    goto error;
  }

  // Create our kafka instance.
  if ((kafka = rd_kafka_new(RD_KAFKA_CONSUMER, conf,
                            errstr, sizeof(errstr))) == NULL) {
    LOG_ERROR("Could not create handle because: %s\n", errstr);
    goto error;
  }

  // Add kafka brokers.
  if (rd_kafka_brokers_add(kafka, cfg->broker) == 0) {
    LOG_ERROR("Kafka brokers could not be added.\n");
    goto error;
  }

  // Subscribe to topics.
  if ((err = rd_kafka_subscribe(kafka, topics)) != 0) {
    LOG_ERROR("Could not subscribe to kafka topic because: %s\n",
              rd_kafka_err2str(err));
    goto error;
  }

  LOG_INFO("Successfully initialized kafka.\n");

  return kafka;

error:
  LOG_ERROR("Could not initialize kafka.\n");
  return NULL;
}

int init_timeseries(char *dbats_db)
{
  timeseries_backend_t *backend = NULL;
  char *dbats_args = NULL;

  LOG_INFO("Initializing timeseries.\n");

  // Initialize where we are going to write data to.
  if ((timeseries = timeseries_init()) == NULL) {
    LOG_ERROR("Could not initialize libtimeseries.\n");
    return 1;
  }

  if ((backend = timeseries_get_backend_by_name(timeseries,
                                                TIMESERIES_BACKEND)) == NULL) {
    LOG_ERROR("Invalid timeseries backend name.\n");
    return 1;
  }

  asprintf(&dbats_args, "-p %s", dbats_db);
  if (timeseries_enable_backend(backend, dbats_args) != 0) {
    LOG_ERROR("Failed to initialize backend.\n");
    return 1;
  }
  free(dbats_args);

  if ((kp = timeseries_kp_init(timeseries, TIMESERIES_KP_DISABLE)) == NULL) {
    LOG_ERROR("Could not create key packages.\n");
    return 1;
  }

  return 0;
}

int init_stats_timeseries(char *kafka_args)
{
  timeseries_backend_t *backend = NULL;

  LOG_INFO("Initializing stats timeseries.\n");

  // Initialize where we are going to write data to.
  if ((stats_timeseries = timeseries_init()) == NULL) {
    LOG_ERROR("Could not initialize libtimeseries.\n");
    return 1;
  }

  if ((backend = timeseries_get_backend_by_name(stats_timeseries,
                                                STATS_TIMESERIES_BACKEND))
                                                == NULL) {
    LOG_ERROR("Invalid stats timeseries backend name.\n");
    return 1;
  }

  LOG_INFO("kafka args: %s\n", kafka_args);
  if (timeseries_enable_backend(backend, kafka_args) != 0) {
    LOG_ERROR("Failed to initialize stats timeseries backend.\n");
    return 1;
  }

  if ((stats_kp = timeseries_kp_init(stats_timeseries,
                                     TIMESERIES_KP_RESET)) == NULL) {
    LOG_ERROR("Could not create stats key packages.\n");
    return 1;
  }

  stats_time = STATS_INTERVAL_NOW;

  return 0;
}

int run(rd_kafka_t *kafka)
{
  int unix_ts = 0;
  uint32_t msg_cnt = 0;
  int eof_since_data = 0;
  rd_kafka_message_t *rkmessage = NULL;

  LOG_INFO("Starting C TSK Proxy.\n");
  unix_ts = time(NULL);
  while (1) {
    maybe_flush(FORCE_FLUSH);
    maybe_flush_stats();

    if (shutdown_proxy) {
      LOG_INFO("Shutting down C TSK Proxy.\n");
      goto cleanup;
    }

    rkmessage = rd_kafka_consumer_poll(kafka, 1000);
    // Process a burst of messages.
    while (rkmessage) {

      // Print msgs/sec to give us an idea of how fast we are.
      msg_cnt++;
      if (time(NULL) != unix_ts) {
        LOG_INFO("Processed %d msgs/s.\n", msg_cnt);
        msg_cnt = 0;
        unix_ts = time(NULL);
      }

      if (!rkmessage->err) {
        process_message(rkmessage);
        eof_since_data = 0;
      } else if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
        LOG_INFO("Reached end of partition.\n");
        if (++eof_since_data >= 10) {
          rd_kafka_message_destroy(rkmessage);
          break;
        }
      } else {
        LOG_ERROR("%s\n", rd_kafka_message_errstr(rkmessage));
        shutdown_proxy++;
      }
      rd_kafka_message_destroy(rkmessage);

      if (shutdown_proxy) {
        break;
      }
      rkmessage = rd_kafka_consumer_poll(kafka, 1000);
      maybe_flush_stats();
    }
  }

cleanup:
  maybe_flush(FORCE_FLUSH);
  LOG_INFO("Shutdown complete.\n");

  return 0;
}

static void usage(const char *name)
{
  fprintf(stderr,
    "Usage: %s [-h] [-a KAFKA_ARGS] [-b BROKER] [-c CHANNEL] "
    "[-i STATS_INTERVAL] [-o OFFSET] [-p TOPIC_PREFIX] "
    "-d DBATS_PATH -g GROUP_ID\n"
    "  -a KAFKA_ARGS      Arguments passed to the Kafka backend.\n"
    "  -b BROKER          Kafka broker host(s) in the format address:port"
                          "[,address:port,...].\n"
    "  -c CHANNEL         Kafka channel.  (default=%s)\n"
    "  -d DBATS_PATH      Path to DBATS database.\n"
    "  -g GROUP_ID        Kafka consumer group ID.  (default=%s)\n"
    "  -h                 Show this help text.\n"
    "  -i STATS_INTERVAL  Statistics interval.\n"
    "  -o OFFSET          Kafka offset.  Must be \"earliest\" or \"latest\".  "
                          "(default=\"%s\")\n"
    "  -p TOPIC_PREFIX Kafka topic prefix.  (default=%s)\n",
    name, DEFAULT_CONSUMER_GROUP_ID, DEFAULT_CHANNEL, DEFAULT_OFFSET,
    DEFAULT_TOPIC_PREFIX);
}

int main(int argc, char **argv)
{
  int ret = 0;
  int opt = 0;
  int prevoptind = 0;
  char *broker = NULL;
  char *group_id = NULL;
  char *dbats_db = NULL;
  char *kafka_args = NULL;
  char *channel = NULL;
  char *topic_prefix = NULL;
  char *offset = NULL;
  rd_kafka_t *kafka = NULL;
  kafka_config_t *kafka_config = NULL;

  signal(SIGINT, catch_sigint);

  while (prevoptind = optind, (opt = getopt(argc, argv,
                                            "o:a:p:i:c:b:g:d:h")) >= 0) {
    switch (opt) {
      case 'a':
        kafka_args = optarg;
        break;
      case 'b':
        broker = optarg;
        break;
      case 'c':
        channel = optarg;
        break;
      case 'd':
        dbats_db = optarg;
        break;
      case 'g':
        group_id = optarg;
        break;
      case 'h':
        usage(argv[0]);
        return 0;
      case 'i':
        stats_interval = atoi(optarg);
        break;
      case 'o':
        offset = optarg;
        break;
      case 'p':
        topic_prefix = optarg;
        break;
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if ((kafka_config = calloc(1, sizeof(kafka_config_t))) == NULL) {
    LOG_ERROR("Could not allocate kafka_config_t.\n");
    return 1;
  }

  // Check mandatory arguments.
  if (dbats_db == NULL) {
    LOG_ERROR("DBATS database not provided.  Use \"-d\".\n");
    return 1;
  }

  if (broker == NULL) {
    LOG_ERROR("Broker not provided.  Use \"-b\".\n");
    return 2;
  }
  kafka_config->broker = broker;

  // Check optional arguments.
  if (group_id == NULL) {
    LOG_INFO("No group id given.  Using default \"%s\".\n",
             DEFAULT_CONSUMER_GROUP_ID);
    group_id = DEFAULT_CONSUMER_GROUP_ID;
  }
  kafka_config->group_id = group_id;

  if (channel == NULL) {
    LOG_INFO("No channel given.  Using default \"%s\".\n", DEFAULT_CHANNEL);
    channel = DEFAULT_CHANNEL;
  }
  kafka_config->channel = channel;

  if (stats_interval == 0) {
    LOG_INFO("No stats interval given.  Using default %d.\n",
             DEFAULT_STATS_INTERVAL);
    stats_interval = DEFAULT_STATS_INTERVAL;
  }

  if (topic_prefix == NULL) {
    LOG_INFO("No topic prefix given.  Using default %s.\n",
             DEFAULT_TOPIC_PREFIX);
    topic_prefix = DEFAULT_TOPIC_PREFIX;
  }
  kafka_config->topic_prefix = topic_prefix;

  if (kafka_args == NULL) {
    LOG_INFO("No kafka arguments given.  Using default %s.\n",
             DEFAULT_KAFKA_ARGS);
    kafka_args = DEFAULT_KAFKA_ARGS;
  }

  if (offset == NULL) {
    LOG_INFO("No offset given.  Using default \"%s\".\n", DEFAULT_OFFSET);
    offset = DEFAULT_OFFSET;
  } else if ((strcmp(offset, "earliest") != 0) &&
             (strcmp(offset, "latest") != 0)) {
      LOG_ERROR("Offset parameter (-o) must either be \"earliest\" or "
                "\"latest\".\n");
      return 1;
  }
  kafka_config->offset = offset;

  // Create key prefix for our kp statistics.
  asprintf(&stats_key_prefix, "%s.%s.%s.%s", STATS_METRIC_PREFIX,
                              graphite_safe_node(strdup(group_id)),
                              graphite_safe_node(strdup(topic_prefix)),
                              graphite_safe_node(strdup(channel)));

  // Initialize kafka, our data source.
  if ((kafka = init_kafka(kafka_config)) == NULL) {
    return 3;
  }

  // Initialize our two timeseries.
  if ((ret = init_timeseries(dbats_db)) != 0) {
    return ret;
  }
  if ((ret = init_stats_timeseries(kafka_args)) != 0) {
    LOG_ERROR("init_stats_timeseries() failed\n");
    return ret;
  }

  // Start main processing loop.
  run(kafka);

  LOG_INFO("Freeing resources.\n");
  free(kafka_config);
  timeseries_kp_free(&kp);
  timeseries_free(&timeseries);

  return 0;
}
