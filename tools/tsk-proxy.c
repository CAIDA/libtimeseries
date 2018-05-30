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
#include <yaml.h>

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
#define STATS_INTERVAL_NOW       ((time(NULL) / stats_interval) *              \
                                  stats_interval)

// When passed as an argument to maybe_flush(), it forces the function to flush.
#define FORCE_FLUSH 0

// Buffer size for the channel in kafka messages.
#define MSG_CHAN_BUF_SIZE 512

// The protocol version that we expect.
#define TSKBATCH_VERSION 0

// Number of header bytes that we skip when parsing an rskmessage.
#define HEADER_MAGIC_LEN 8

// Timeout for kafka consumer poll in milliseconds.
#define KAFKA_POLL_TIMEOUT 10 * 1000

// Buffer length to hold key package keys.
#define KEY_BUF_LEN 1024

typedef struct tsk_config {

  int log_level;

  char *timeseries_backend;
  char *timeseries_dbats_opts;

  char *kafka_brokers;
  char *kafka_topic_prefix;
  char *kafka_channel;
  char *kafka_consumer_group;
  char *kafka_offset;

  char *stats_ts_backend;
  char *stats_ts_opts;

  rd_kafka_topic_partition_list_t *partition_list;
} tsk_config_t;

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

/** Handles SIGINT gracefully and shuts down */
static void catch_sigint(int sig)
{
  shutdown_proxy++;
  if (shutdown_proxy >= 3) {
    LOG_INFO("Caught %d SIGINTs.  Shutting down now.\n", shutdown_proxy);
    exit(1);
  }
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

void inc_stat(const char *stats_key_suffix, const int value)
{
  int key_id = 0;
  int old_value = 0;

  char *stats_key = NULL;

  asprintf(&stats_key, "%s.%s", stats_key_prefix, stats_key_suffix);

  if ((key_id = timeseries_kp_get_key(stats_kp, stats_key)) == -1) {
    key_id = timeseries_kp_add_key(stats_kp, stats_key);
  }

  old_value = timeseries_kp_get(stats_kp, key_id);
  timeseries_kp_set(stats_kp, key_id, value + old_value);

  free(stats_key);
}

int parse_key_value(char **buf, size_t *len_read, const int buflen)
{
  uint16_t keylen = 0;
  uint64_t value = 0;
  int key_id = 0;
  static char key[KEY_BUF_LEN];

  // Get 2-byte key length.
  DESERIALIZE_VAL(*buf, buflen, *len_read, keylen);
  keylen = ntohs(keylen);

  // Make sure that there are enough bytes left to read.
  if ((buflen - *len_read) < keylen) {
    LOG_ERROR("Not enough bytes left to read.");
    return 1;
  }

  // Get variable-length key.  We have to 0-terminate the key ourselves.
  memcpy(key, *buf, keylen);
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

  timeseries_kp_set(kp, key_id, value);

  return 0;
}

static void maybe_flush(const int flush_time)
{
  static int current_time = 0;

  if (current_time == 0) {
    current_time = flush_time;
  }

  if (flush_time == 0 || flush_time != current_time) {
    LOG_INFO("%sFlushing key packages at %d with %d keys enabled (%d total).\n",
        (flush_time == FORCE_FLUSH) ? "(Force-)" : "",
        current_time, timeseries_kp_enabled_size(kp), timeseries_kp_size(kp));
    inc_stat("flush_cnt", 1);
    inc_stat("flushed_key_cnt", timeseries_kp_enabled_size(kp));

    if (timeseries_kp_flush(kp, current_time) == -1) {
      LOG_ERROR("Could not flush key packages.\n");
      return;
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
      return;
    }
    stats_time = now;
  }
}

void handle_message(const rd_kafka_message_t *rkmessage,
                    const tsk_config_t *cfg)
{
  size_t len_read = 0;
  uint8_t version = 0;
  uint32_t time = 0;
  uint16_t chanlen = 0;
  char msg_chan[MSG_CHAN_BUF_SIZE] = {0};
  char *buf = rkmessage->payload;

  // Skip the string "TSKBATCH".
  len_read += HEADER_MAGIC_LEN;
  buf += HEADER_MAGIC_LEN;

  // Extract version (1 byte), time (4 bytes), and chanlen (2 bytes), and
  // msg_chan (variable-length).
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, version);
  if (version != TSKBATCH_VERSION) {
    LOG_ERROR("Expected version %d but got %d.\n", TSKBATCH_VERSION, version);
    return;
  }
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, time);
  time = ntohl(time);
  DESERIALIZE_VAL(buf, rkmessage->len, len_read, chanlen);
  chanlen = ntohs(chanlen);
  assert(chanlen < MSG_CHAN_BUF_SIZE);

  // Make sure that there are enough bytes left to read.
  if ((rkmessage->len - len_read) < chanlen) {
    LOG_ERROR("Not enough bytes left to read.");
    return;
  }
  memcpy(msg_chan, buf, chanlen);
  buf += chanlen;
  len_read += chanlen;

  if (strncmp(cfg->kafka_channel, msg_chan, strlen(cfg->kafka_channel)) != 0) {
    LOG_ERROR("Message with unknown channel.  Expected %s but got %s.\n",
              cfg->kafka_channel, msg_chan);
    return;
  }

  maybe_flush(time);
  inc_stat("messages_cnt", 1);
  inc_stat("messages_bytes", rkmessage->len);

  while (len_read < rkmessage->len) {
    if (parse_key_value(&buf, &len_read, rkmessage->len) != 0) {
      return;
    }
  }
}

rd_kafka_t *init_kafka(tsk_config_t *cfg)
{
  rd_kafka_t *kafka = NULL;
  rd_kafka_topic_partition_list_t *topics = NULL;
  rd_kafka_resp_err_t err;
  rd_kafka_conf_t *conf;
  char errstr[512];
  char *group_id = NULL;
  char *topic_name = NULL;

  LOG_INFO("Initializing kafka.\n");

  asprintf(&topic_name, "%s.%s", cfg->kafka_topic_prefix, cfg->kafka_channel);
  asprintf(&group_id, "%s.%s", cfg->kafka_consumer_group, topic_name);

  topics = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(topics, topic_name, -1);
  cfg->partition_list = topics;

  LOG_INFO("Kafka topic name: %s\n", topic_name);
  LOG_INFO("Kafka group id: %s\n", group_id);

  // Configure the initial log offset.
  conf = rd_kafka_conf_new();
  if (rd_kafka_conf_set(conf, "auto.offset.reset", cfg->kafka_offset,
                        errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    LOG_ERROR("Could not set log offset because: %s\n", errstr);
    goto error;
  }

  // Set our group ID.
  if (rd_kafka_conf_set(conf, "group.id", group_id,
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
  if (rd_kafka_brokers_add(kafka, cfg->kafka_brokers) == 0) {
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

  free(topic_name);
  free(group_id);

  return kafka;

error:
  LOG_ERROR("Could not initialize kafka.\n");
  free(topic_name);
  free(group_id);
  return NULL;
}

int init_timeseries(const tsk_config_t *cfg)
{
  timeseries_backend_t *backend = NULL;

  LOG_INFO("Initializing timeseries.\n");

  // Initialize where we are going to write data to.
  if ((timeseries = timeseries_init()) == NULL) {
    LOG_ERROR("Could not initialize libtimeseries.\n");
    return 1;
  }

  if ((backend = timeseries_get_backend_by_name(timeseries,
                                                cfg->timeseries_backend)) ==
                                                NULL) {
    LOG_ERROR("Invalid timeseries backend name.\n");
    return 1;
  }

  LOG_INFO("Using DBATS options \"%s\".\n", cfg->timeseries_dbats_opts);
  if (timeseries_enable_backend(backend, cfg->timeseries_dbats_opts) != 0) {
    LOG_ERROR("Failed to initialize backend.\n");
    return 1;
  }

  if ((kp = timeseries_kp_init(timeseries, TIMESERIES_KP_DISABLE)) == NULL) {
    LOG_ERROR("Could not create key packages.\n");
    return 1;
  }

  return 0;
}

int init_stats_timeseries(const tsk_config_t *cfg)
{
  timeseries_backend_t *backend = NULL;

  LOG_INFO("Initializing stats timeseries.\n");

  // Initialize where we are going to write data to.
  if ((stats_timeseries = timeseries_init()) == NULL) {
    LOG_ERROR("Could not initialize libtimeseries.\n");
    return 1;
  }

  if ((backend = timeseries_get_backend_by_name(stats_timeseries,
                                                cfg->stats_ts_backend))
                                                == NULL) {
    LOG_ERROR("Invalid stats timeseries backend name.\n");
    return 1;
  }

  LOG_INFO("kafka args: %s\n", cfg->stats_ts_opts);
  if (timeseries_enable_backend(backend, cfg->stats_ts_opts) != 0) {
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

void run(rd_kafka_t *kafka, const tsk_config_t *cfg)
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

    rkmessage = rd_kafka_consumer_poll(kafka, KAFKA_POLL_TIMEOUT);
    eof_since_data = 0;
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
        handle_message(rkmessage, cfg);
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
      rkmessage = rd_kafka_consumer_poll(kafka, KAFKA_POLL_TIMEOUT);
      maybe_flush_stats();
    }
  }

cleanup:
  maybe_flush(FORCE_FLUSH);
  LOG_INFO("Shutdown complete.\n");
}

void create_stats_prefix(tsk_config_t *cfg)
{

  char *consumer_group = graphite_safe_node(strdup(cfg->kafka_consumer_group));
  char *topic_prefix = graphite_safe_node(strdup(cfg->kafka_topic_prefix));
  char *channel = graphite_safe_node(strdup(cfg->kafka_channel));

  // Create key prefix for our kp statistics.
  asprintf(&stats_key_prefix, "%s.%s.%s.%s", STATS_METRIC_PREFIX,
           consumer_group, topic_prefix, channel);

  free(consumer_group);
  free(topic_prefix);
  free(channel);
}

tsk_config_t *parse_config_file(const char *filename)
{
  FILE *fh = NULL;
  char* tk;
  char **textp = NULL;
  int *intp = NULL;
  int state = 0;
  yaml_parser_t parser;
  yaml_token_t token;
  tsk_config_t *tsk_cfg = NULL;

  LOG_INFO("Parsing config file \"%s\".\n", filename);

  if ((tsk_cfg = calloc(1, sizeof(tsk_config_t))) == NULL) {
    LOG_ERROR("Could not allocate tsk_config_t object.\n");
    return NULL;
  }

  if (!yaml_parser_initialize(&parser)) {
    LOG_ERROR("Failed to initialize YAML parser.\n");
    return NULL;
  }

  if ((fh = fopen(filename, "r")) == NULL) {
    LOG_ERROR("Failed to open config file \"%s\".\n", filename);
    return NULL;
  }

  yaml_parser_set_input_file(&parser, fh);

  /* YAML supports mappings in its data format which would allow us to add
   * sections to our configuration file format.  While more elegant, it would
   * require more complicated parsing code, which is why we only support a flat
   * configuration file format for now, consisting of key:value variables.
   */
  do {
    yaml_parser_scan(&parser, &token);
    switch(token.type) {
      case YAML_KEY_TOKEN:
        state = 0;
        break;
      case YAML_VALUE_TOKEN:
        state = 1;
        break;
      case YAML_SCALAR_TOKEN:
        tk = (char *) token.data.scalar.value;
        if (state == 0) {
          // General section.
          if (strcmp(tk, "log-level") == 0) {
            intp = &(tsk_cfg->log_level);
          // Timeseries section.
          } else if (strcmp(tk, "timeseries-backend") == 0) {
            textp = &(tsk_cfg->timeseries_backend);
          } else if (strcmp(tk, "timeseries-dbats-opts") == 0) {
            textp = &(tsk_cfg->timeseries_dbats_opts);
          // Kafka section.
          } else if (strcmp(tk, "kafka-brokers") == 0) {
            textp = &(tsk_cfg->kafka_brokers);
          } else if (strcmp(tk, "kafka-topic-prefix") == 0) {
            textp = &(tsk_cfg->kafka_topic_prefix);
          } else if (strcmp(tk, "kafka-channel") == 0) {
            textp = &(tsk_cfg->kafka_channel);
          } else if (strcmp(tk, "kafka-consumer-group") == 0) {
            textp = &(tsk_cfg->kafka_consumer_group);
          } else if (strcmp(tk, "kafka-offset") == 0) {
            textp = &(tsk_cfg->kafka_offset);
          // Stats section.
          } else if (strcmp(tk, "stats-interval") == 0) {
            intp = &stats_interval;
          } else if (strcmp(tk, "stats-ts-backend") == 0) {
            textp = &(tsk_cfg->stats_ts_backend);
          } else if (strcmp(tk, "stats-ts-opts") == 0) {
            textp = &(tsk_cfg->stats_ts_opts);
          } else {
            LOG_ERROR("Ignoring unsupported config key \"%s\".\n", tk);
          }
        } else if (textp && (state == 1)) {
          *textp = strdup(tk);
          textp = NULL;
        } else if (intp && (state == 1)) {
          *intp = atoi(tk);
          intp = NULL;
        }
        break;
      default:
        break;
    }
    if (token.type != YAML_STREAM_END_TOKEN) {
      yaml_token_delete(&token);
    }
  } while(token.type != YAML_STREAM_END_TOKEN);
  yaml_token_delete(&token);

  yaml_parser_delete(&parser);
  fclose(fh);

  return tsk_cfg;
}

int is_valid_config(const tsk_config_t *c) {

  // todo loglevel?

  if (c->timeseries_backend == NULL) {
    LOG_ERROR("Config option \"timeseries-backend\" not provided.\n");
    return 1;
  }
  if (c->timeseries_dbats_opts == NULL) {
    LOG_ERROR("Config option \"timeseries-dbats-opts\" not provided.\n");
    return 1;
  }

  if (c->kafka_brokers == NULL) {
    LOG_ERROR("Config option \"kafka-brokers\" not provided.\n");
    return 1;
  }
  if (c->kafka_topic_prefix == NULL) {
    LOG_ERROR("Config option \"kafka-topic-prefix\" not provided.\n");
    return 1;
  }
  if (c->kafka_channel == NULL) {
    LOG_ERROR("Config option \"kafka-channel\" not provided.\n");
    return 1;
  }
  if (c->kafka_consumer_group == NULL) {
    LOG_ERROR("Config option \"kafka-consumer-group\" not provided.\n");
    return 1;
  }
  if (c->kafka_offset == NULL) {
    LOG_ERROR("Config option \"kafka-offset\" not provided.\n");
    return 1;
  }

  if (c->stats_ts_backend == NULL) {
    LOG_ERROR("Config option \"stats-ts-backend\" not provided.\n");
    return 1;
  }
  if (c->stats_ts_opts == NULL) {
    LOG_ERROR("Config option \"stats-ts-opts\" not provided.\n");
    return 1;
  }

  return 0;
}

void destroy_tsk_config(tsk_config_t *c)
{
  LOG_INFO("Freeing tsk_config_t object.\n");

  free(c->timeseries_backend);
  free(c->timeseries_dbats_opts);

  free(c->kafka_brokers);
  free(c->kafka_topic_prefix);
  free(c->kafka_channel);
  free(c->kafka_consumer_group);
  free(c->kafka_offset);

  free(c->stats_ts_backend);
  free(c->stats_ts_opts);

  rd_kafka_topic_partition_list_destroy(c->partition_list);

  free(c);
}

int main(int argc, char **argv)
{
  rd_kafka_t *kafka = NULL;
  tsk_config_t *cfg = NULL;

  signal(SIGINT, catch_sigint);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
    return 1;
  }

  if ((cfg = parse_config_file(argv[1])) == NULL) {
    LOG_ERROR("Could not parse config file.\n");
    return 1;
  }
  if (is_valid_config(cfg) != 0) {
    LOG_ERROR("Missing keys in configuration file.\n");
    return 1;
  }
  create_stats_prefix(cfg);

  // Initialize kafka, our data source.
  if ((kafka = init_kafka(cfg)) == NULL) {
    return 1;
  }

  // Initialize our two timeseries.
  if (init_timeseries(cfg) != 0) {
    LOG_ERROR("Could not initialize timeseries.\n");
    return 1;
  }
  if (init_stats_timeseries(cfg) != 0) {
    LOG_ERROR("Could not initialize stats timeseries.\n");
    return 1;
  }

  // Start main processing loop.
  run(kafka, cfg);

  LOG_INFO("Freeing resources.\n");
  rd_kafka_destroy(kafka);
  timeseries_kp_free(&kp);
  timeseries_kp_free(&stats_kp);
  timeseries_free(&timeseries);
  timeseries_free(&stats_timeseries);
  destroy_tsk_config(cfg);
  free(stats_key_prefix);

  return 0;
}
