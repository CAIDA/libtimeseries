/*
 * libtimeseries
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#include "timeseries_backend_kafka.h"
#include "timeseries_backend_int.h"
#include "timeseries_kp_int.h"
#include "timeseries_log_int.h"
#include "config.h"
#include "utils.h"
#include "wandio_utils.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <librdkafka/rdkafka.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wandio.h>

#define BACKEND_NAME "kafka"

#define DEFAULT_TOPIC "tsk-production"

#define HEADER_MAGIC "TSKBATCH"
#define HEADER_MAGIC_LEN 8

/** use "unassigned" partition to automatically round-robin amongst
    partitions */
#define DEFAULT_PARTITION RD_KAFKA_PARTITION_UA

#define CONNECT_MAX_RETRIES 8

/** 32K buffer. Approx half will be used, hence the x2 */
#define BUFFER_LEN ((1024 * 32) * 2)

#define IDENTITY_MAX_LEN 1024

#define STATE(provname) (TIMESERIES_BACKEND_STATE(kafka, provname))

#define SERIALIZE_VAL(buf, len, written, from)                                 \
  do {                                                                         \
    size_t s;                                                                  \
    assert(((len) - (written)) >= sizeof((from)));                             \
    memcpy((buf), &(from), sizeof(from));                                      \
    s = sizeof(from);                                                          \
    written += s;                                                              \
    buf += s;                                                                  \
  } while (0)

#define SEND_MSG(partition, buf, written, time, ptr, len)                      \
  do {                                                                         \
    int success = 0;                                                           \
    while (success == 0) {                                                     \
      if (rd_kafka_produce(state->rkt, (partition), RD_KAFKA_MSG_F_COPY,       \
                           (buf), (written), &(time), sizeof(time),            \
                           NULL) == -1) {                                      \
        if (rd_kafka_errno2err(errno) == RD_KAFKA_RESP_ERR__QUEUE_FULL) {      \
          timeseries_log(__func__, "WARN: producer queue full, retrying...");  \
          if (sleep(1) != 0) {                                                 \
            goto err;                                                          \
          }                                                                    \
        } else {                                                               \
          timeseries_log(                                                      \
            __func__, "ERROR: Failed to produce to topic %s partition %i: %s", \
            rd_kafka_topic_name(state->rkt), (partition),                      \
            rd_kafka_err2str(rd_kafka_errno2err(errno)));                      \
          rd_kafka_poll(state->rdk_conn, 0);                                   \
          goto err;                                                            \
        }                                                                      \
      } else {                                                                 \
        success = 1;                                                           \
      }                                                                        \
    }                                                                          \
    rd_kafka_poll(state->rdk_conn, 0);                                         \
    RESET_BUF(buf, ptr, written);                                              \
  } while (0)

#define RESET_BUF(buf, ptr, written)                                           \
  do {                                                                         \
    (ptr) = (buf);                                                             \
    (written) = 0;                                                             \
  } while (0)

#define SEND_IF_FULL(partition, buf, written, time, ptr, len)                  \
  do {                                                                         \
    if (written > ((len) / 2)) {                                               \
      SEND_MSG(partition, buf, written, time, ptr, len);                       \
    }                                                                          \
  } while (0)

/** The basic fields that every instance of this backend have in common */
static timeseries_backend_t timeseries_backend_kafka = {
  TIMESERIES_BACKEND_ID_KAFKA,            //
  BACKEND_NAME,                           //
  TIMESERIES_BACKEND_GENERATE_PTRS(kafka) //
};

/** Holds the state for an instance of this backend */
typedef struct timeseries_backend_kafka_state {

  /** Comma-separated list of Kafka brokers to connect to */
  char *broker_uri;

  /** Name of the channel (DBATS server) to publish metrics to */
  char *channel_name;

  /** Name of the kafka topic to produce to */
  char *topic_prefix;

  /** Reusable message buffer */
  uint8_t buffer[BUFFER_LEN];

  /** Number of bytes written to the buffer */
  int buffer_written;

  /** The number of values received for the current bulk set */
  uint32_t bulk_cnt;

  /** The expected number of values in the current bulk set */
  uint32_t bulk_expect;

  /* Kafka connection state: */

  /** Are we connected to Kafka? */
  int connected;

  /** Have we encountered a fatal error? */
  int fatal_error;

  /** RD Kafka connection handle */
  rd_kafka_t *rdk_conn;

  /** Fully-qualified name of the topic (<topic_prefix>.<channel_name>) */
  char topic_name[IDENTITY_MAX_LEN];

  /** RD Kafka topic handle */
  rd_kafka_topic_t *rkt;

} timeseries_backend_kafka_state_t;

/** Print usage information to stderr */
static void usage(timeseries_backend_t *backend)
{
  fprintf(stderr,
          "backend usage: %s [-p topic] -b broker-uri -c channel \n"
          "       -b <broker-uri>    kafka broker URI (required)\n"
          "       -c <channel>       metric channel to publish to (required)\n"
          "       -p <topic-prefix>  topic prefix to use (default: %s)\n",
          backend->name, //
          DEFAULT_TOPIC);
}

/** Parse the arguments given to the backend */
static int parse_args(timeseries_backend_t *backend, int argc, char **argv)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  int opt;

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */

  while ((opt = getopt(argc, argv, ":b:c:p:?")) >= 0) {
    switch (opt) {
    case 'b':
      state->broker_uri = strdup(optarg);
      break;

    case 'c':
      state->channel_name = strdup(optarg);
      break;

    case 'p':
      state->topic_prefix = strdup(optarg);
      break;

    case '?':
    case ':':
    default:
      usage(backend);
      return -1;
    }
  }

  if (state->broker_uri == NULL) {
    fprintf(stderr, "ERROR: Kafka Broker URI(s) must be specified using -b\n");
    usage(backend);
    return -1;
  }

  if (state->channel_name == NULL) {
    fprintf(stderr, "ERROR: Metric channel name must be specified using -c\n");
    usage(backend);
    return -1;
  }

  return 0;
}

static void kafka_error_callback(rd_kafka_t *rk, int err, const char *reason,
                                 void *opaque)
{
  timeseries_backend_t *backend = (timeseries_backend_t *)opaque;

  switch (err) {
  // fatal errors:
  case RD_KAFKA_RESP_ERR__BAD_COMPRESSION:
  case RD_KAFKA_RESP_ERR__RESOLVE:
    STATE(backend)->fatal_error = 1;
  // fall through

  // recoverable? errors:
  case RD_KAFKA_RESP_ERR__DESTROY:
  case RD_KAFKA_RESP_ERR__FAIL:
  case RD_KAFKA_RESP_ERR__TRANSPORT:
  case RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN:
    STATE(backend)->connected = 0;
    break;
  }

  timeseries_log(__func__, "ERROR: %s (%d): %s", rd_kafka_err2str(err), err,
                 reason);

  // TODO: handle other errors
}

static void kafka_delivery_callback(rd_kafka_t *rk,
                                    const rd_kafka_message_t *rkmessage,
                                    void *opaque)
{
  if (rkmessage->err) {
    timeseries_log(__func__,
                   "ERROR: Message delivery failed: %s [%" PRId32 "]: %s\n",
                   rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition,
                   rd_kafka_err2str(rkmessage->err));
  }
}

static int topic_connect(timeseries_backend_t *backend)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();
  assert(topic_conf != NULL);

  timeseries_log(__func__, "INFO: Checking topic connection...");
  assert(state->topic_name != NULL);

  // build the topic name
  if (snprintf(state->topic_name, IDENTITY_MAX_LEN, "%s.%s",
               state->topic_prefix, state->channel_name) >= IDENTITY_MAX_LEN) {
    return -1;
  }

  // use the random partitioner since our messages are self-contained
  rd_kafka_topic_conf_set_partitioner_cb(topic_conf,
                                         rd_kafka_msg_partitioner_random);

  // connect to kafka
  if (state->rkt == NULL) {
    timeseries_log(__func__, "DEBUG: Connecting to %s", state->topic_name);
    if ((state->rkt = rd_kafka_topic_new(state->rdk_conn, state->topic_name,
                                         topic_conf)) == NULL) {
      return -1;
    }
  }

  return 0;
}

static int producer_connect(timeseries_backend_t *backend)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  rd_kafka_conf_t *conf = rd_kafka_conf_new();
  char errstr[512];

  // Set the opaque pointer that will be passed to callbacks
  rd_kafka_conf_set_opaque(conf, backend);

  // Set our error handler
  rd_kafka_conf_set_error_cb(conf, kafka_error_callback);

  // ask for delivery reports
  rd_kafka_conf_set_dr_msg_cb(conf, kafka_delivery_callback);

  // Disable logging of connection close/idle timeouts caused by Kafka 0.9.x
  //   See https://github.com/edenhill/librdkafka/issues/437 for more details.
  // TODO: change this when librdkafka has better handling of idle disconnects
  if (rd_kafka_conf_set(conf, "log.connection.close", "false", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    timeseries_log(__func__, "ERROR: %s", errstr);
    goto err;
  }

  if (rd_kafka_conf_set(conf, "compression.codec", "snappy", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    timeseries_log(__func__, "ERROR: %s", errstr);
    goto err;
  }

  // Disable logging of connection close/idle timeouts caused by Kafka 0.9.x
  //   See https://github.com/edenhill/librdkafka/issues/437 for more details.
  // TODO: change this when librdkafka has better handling of idle disconnects
  if (rd_kafka_conf_set(conf, "log.connection.close", "false", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    timeseries_log(__func__, "ERROR: %s", errstr);
    goto err;
  }
  if (rd_kafka_conf_set(conf, "batch.num.messages", "100", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    timeseries_log(__func__, "ERROR: %s", errstr);
    goto err;
  }
  // But don't wait very long before sending a partial batch (0.5s)
  if (rd_kafka_conf_set(conf, "queue.buffering.max.ms", "500", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    timeseries_log(__func__, "ERROR: %s", errstr);
    goto err;
  }
  if (rd_kafka_conf_set(conf, "queue.buffering.max.messages", "2000", errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    timeseries_log(__func__, "ERROR: %s", errstr);
    goto err;
  }

  if ((state->rdk_conn = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr,
                                      sizeof(errstr))) == NULL) {
    timeseries_log(__func__, "ERROR: Failed to create new producer: %s",
                   errstr);
    goto err;
  }

  if (rd_kafka_brokers_add(state->rdk_conn, state->broker_uri) == 0) {
    timeseries_log(__func__, "ERROR: No valid brokers specified");
    goto err;
  }

  state->connected = 1;

  // poll for errors
  rd_kafka_poll(state->rdk_conn, 5000);

  return state->fatal_error;

err:
  return -1;
}

static int kafka_connect(timeseries_backend_t *backend)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  int wait = 10;
  int connect_retries = CONNECT_MAX_RETRIES;

  while (state->connected == 0 && connect_retries > 0) {
    if (producer_connect(backend) != 0) {
      return -1;
    }

    connect_retries--;
    if (state->connected == 0 && connect_retries > 0) {
      timeseries_log(__func__,
                     "WARN: Failed to connect to Kafka. Retrying in %d seconds",
                     wait);
      sleep(wait);
      wait *= 2;
      if (wait > 180) {
        wait = 180;
      }
    }
  }

  if (state->connected == 0) {
    timeseries_log(
      __func__, "ERROR: Failed to connect to Kafka after %d retries. Giving up",
      CONNECT_MAX_RETRIES);
    return -1;
  }

  // connect to topics
  if (topic_connect(backend) != 0) {
    return -1;
  }

  return 0;
}

static int write_header(uint8_t *buf, size_t len, uint32_t time)
{
  // this function can be a bit sub-optimal because it isn't called a zillion
  // times
  size_t written = 0;

  // use a string as the magic number (for easier debugging)
  assert(HEADER_MAGIC_LEN < len);
  memcpy(buf, HEADER_MAGIC, HEADER_MAGIC_LEN);
  buf += HEADER_MAGIC_LEN;
  written += HEADER_MAGIC_LEN;

  // the time of this batch
  SERIALIZE_VAL(buf, len, written, time);

  return written;
}

static int write_kv(uint8_t *buf, size_t len, const char *key, uint64_t value)
{
  size_t written = 0;
  size_t key_len = strlen(key);
  assert(key_len < UINT16_MAX);

  // now we know the size of the message we will write
  assert((key_len + sizeof(uint16_t) + sizeof(value)) <= len);

  // write the key length (network byte order)
  uint16_t tmp16 = htons(key_len);
  SERIALIZE_VAL(buf, len, written, tmp16);

  // copy the string in
  memcpy(buf, key, key_len);
  buf += key_len;
  written += key_len;

  // and then append the value (in network byte order)
  value = htonll(value);
  SERIALIZE_VAL(buf, len, written, value);

  return written;
}

/* ===== PUBLIC FUNCTIONS BELOW THIS POINT ===== */

timeseries_backend_t *timeseries_backend_kafka_alloc()
{
  return &timeseries_backend_kafka;
}

int timeseries_backend_kafka_init(timeseries_backend_t *backend, int argc,
                                  char **argv)
{
  timeseries_backend_kafka_state_t *state;

  /* allocate our state */
  if ((state = malloc_zero(sizeof(timeseries_backend_kafka_state_t))) == NULL) {
    timeseries_log(__func__,
                   "could not malloc timeseries_backend_kafka_state_t");
    return -1;
  }
  timeseries_backend_register_state(backend, state);

  /* parse the command line args */
  if (parse_args(backend, argc, argv) != 0) {
    return -1;
  }

  /* connect to kafka and create producer */
  if (kafka_connect(backend) != 0) {
    goto err;
  }

  /* ready to rock n roll */
  return 0;

err:
  timeseries_backend_kafka_free(backend);
  return -1;
}

void timeseries_backend_kafka_free(timeseries_backend_t *backend)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);

  if (state == NULL) {
    return;
  }

  if (state->rdk_conn != NULL) {
    int drain_wait_cnt = 12;
    while (rd_kafka_outq_len(state->rdk_conn) > 0 && drain_wait_cnt > 0) {
      timeseries_log(
        __func__,
        "INFO: Waiting for Kafka queue to drain (currently %d messages)",
        rd_kafka_outq_len(state->rdk_conn));
      rd_kafka_poll(state->rdk_conn, 5000);
      drain_wait_cnt--;
    }
  }

  free(state->broker_uri);
  state->broker_uri = NULL;

  free(state->channel_name);
  state->channel_name = NULL;

  free(state->topic_prefix);
  state->topic_prefix = NULL;

  if (state->rkt != NULL) {
    rd_kafka_topic_destroy(state->rkt);
    state->rkt = NULL;
  }

  timeseries_log(__func__, "INFO: Shutting down rdkafka");
  if (state->rdk_conn != NULL) {
    rd_kafka_destroy(state->rdk_conn);
    state->rdk_conn = NULL;
  }

  timeseries_backend_free_state(backend);
  return;
}

int timeseries_backend_kafka_kp_init(timeseries_backend_t *backend,
                                     timeseries_kp_t *kp, void **kp_state_p)
{
  /* we do not need any state */
  assert(kp_state_p != NULL);
  *kp_state_p = NULL;
  return 0;
}

void timeseries_backend_kafka_kp_free(timeseries_backend_t *backend,
                                      timeseries_kp_t *kp, void *kp_state)
{
  /* we did not allocate any state */
  assert(kp_state == NULL);
  return;
}

int timeseries_backend_kafka_kp_ki_update(timeseries_backend_t *backend,
                                          timeseries_kp_t *kp)
{
  /* we don't need to do anything */
  return 0;
}

void timeseries_backend_kafka_kp_ki_free(timeseries_backend_t *backend,
                                         timeseries_kp_t *kp,
                                         timeseries_kp_ki_t *ki, void *ki_state)
{
  /* we did not allocate any state */
  assert(ki_state == NULL);
  return;
}

int timeseries_backend_kafka_kp_flush(timeseries_backend_t *backend,
                                      timeseries_kp_t *kp, uint32_t time)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  timeseries_kp_ki_t *ki = NULL;
  int id;

  uint8_t *ptr = state->buffer;
  size_t len = BUFFER_LEN;
  ssize_t s;
  assert(state->buffer_written == 0);

  // flip the time around
  time = htonl(time);

  TIMESERIES_KP_FOREACH_KI(kp, ki, id)
  {
    if (timeseries_kp_ki_enabled(ki) == 0) {
      continue;
    }

    if (state->buffer_written == 0) {
      // new message, so write the header
      if ((s = write_header(ptr, (len - state->buffer_written), time)) <= 0) {
        goto err;
      }
      state->buffer_written += s;
      ptr += s;
    }

    if ((s = write_kv(ptr, (len - state->buffer_written),
                      timeseries_kp_ki_get_key(ki),
                      timeseries_kp_ki_get_value(ki))) <= 0) {
      goto err;
    }
    state->buffer_written += s;
    ptr += s;

    SEND_IF_FULL(DEFAULT_PARTITION, state->buffer, state->buffer_written, time,
                 ptr, len);
  }

  SEND_MSG(DEFAULT_PARTITION, state->buffer, state->buffer_written, time, ptr,
           len);

  return 0;

err:
  return -1;
}

int timeseries_backend_kafka_set_single(timeseries_backend_t *backend,
                                        const char *key, uint64_t value,
                                        uint32_t time)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);

  uint8_t *ptr = state->buffer;
  size_t len = BUFFER_LEN;
  ssize_t s;
  assert(state->buffer_written == 0);

  // flip the time around
  time = htonl(time);

  if ((s = write_header(ptr, (len - state->buffer_written), time)) <= 0) {
    goto err;
  }
  state->buffer_written += s;
  ptr += s;

  if ((s = write_kv(ptr, (len - state->buffer_written), key, value)) <= 0) {
    goto err;
  }
  state->buffer_written += s;
  ptr += s;

  SEND_MSG(DEFAULT_PARTITION, state->buffer, state->buffer_written, time, ptr,
           len);

  return 0;

err:
  return -1;
}

int timeseries_backend_kafka_set_single_by_id(timeseries_backend_t *backend,
                                              uint8_t *id, size_t id_len,
                                              uint64_t value, uint32_t time)
{
  /* we chose not to implement this for memory efficiency */
  assert(0 && "Not implemented");
  return -1;
}

int timeseries_backend_kafka_set_bulk_init(timeseries_backend_t *backend,
                                           uint32_t key_cnt, uint32_t time)
{
  /* we chose not to implement this for memory efficiency */
  assert(0 && "Not implemented");
  return -1;
}

int timeseries_backend_kafka_set_bulk_by_id(timeseries_backend_t *backend,
                                            uint8_t *id, size_t id_len,
                                            uint64_t value)
{
  /* we chose not to implement this for memory efficiency */
  assert(0 && "Not implemented");
  return -1;
}

size_t timeseries_backend_kafka_resolve_key(timeseries_backend_t *backend,
                                            const char *key,
                                            uint8_t **backend_key)
{
  *backend_key = NULL;
  return 0;
}

int timeseries_backend_kafka_resolve_key_bulk(
  timeseries_backend_t *backend, uint32_t keys_cnt, const char *const *keys,
  uint8_t **backend_keys, size_t *backend_key_lens, int *contig_alloc)
{
  int i;

  for (i = 0; i < keys_cnt; i++) {
    if ((backend_key_lens[i] = timeseries_backend_kafka_resolve_key(
           backend, keys[i], &(backend_keys[i]))) == 0) {
      timeseries_log(__func__, "Could not resolve key ID");
      return -1;
    }
  }

  assert(contig_alloc != NULL);
  *contig_alloc = 0;

  return 0;
}
