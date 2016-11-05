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

#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wandio.h>

#include "utils.h"
#include "wandio_utils.h"

#include "timeseries_backend_int.h"
#include "timeseries_kp_int.h"
#include "timeseries_log_int.h"
#include "timeseries_backend_kafka.h"

#define BACKEND_NAME "kafka"

#define DEFAULT_COMPRESS_LEVEL 6

#define STATE(provname) (TIMESERIES_BACKEND_STATE(kafka, provname))

/** The basic fields that every instance of this backend have in common */
static timeseries_backend_t timeseries_backend_kafka = {
  TIMESERIES_BACKEND_ID_KAFKA, BACKEND_NAME,
  TIMESERIES_BACKEND_GENERATE_PTRS(kafka)};

/** Holds the state for an instance of this backend */
typedef struct timeseries_backend_kafka_state {
  /** The filename to write metrics out to */
  char *kafka_file;

  /** A wandio output file pointer to write metrics to */
  iow_t *outfile;

  /** The compression level to use of the outfile is compressed */
  int compress_level;

  /** The number of values received for the current bulk set */
  uint32_t bulk_cnt;

  /** The time for the current bulk set */
  uint32_t bulk_time;

  /** The expected number of values in the current bulk set */
  uint32_t bulk_expect;

} timeseries_backend_kafka_state_t;

/** Print usage information to stderr */
static void usage(timeseries_backend_t *backend)
{
  fprintf(stderr,
          "backend usage: %s [-c compress-level] [-f output-file]\n"
          "       -c <level>    output compression level to use (default: %d)\n"
          "       -f            file to write KAFKA timeseries metrics to\n",
          backend->name, DEFAULT_COMPRESS_LEVEL);
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

  while ((opt = getopt(argc, argv, ":c:f:?")) >= 0) {
    switch (opt) {
    case 'c':
      state->compress_level = atoi(optarg);
      break;

    case 'f':
      state->kafka_file = strdup(optarg);
      break;

    case '?':
    case ':':
    default:
      usage(backend);
      return -1;
    }
  }

  /*
   * TODO:
   *  - topic prefix (default: ???)
   *  - channel??? (i.e. the server to send metrics to)
   *    prefix + channel = kafka topic
   *
   *  - brokers uri
   *
   *  - connect to kafka
   *
   *  - set_single, send single datapoint as message (but same format as bulk)
   *  - set_bulk, serialize kp into single* message
   *    * use standard message batching to split big KPs
   *
   *  - header: channel, key count (for x-check), time
   *  - row: key (in ascii), value
   */

  return 0;
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

  /* set initial default values (that can be overridden on the command line) */
  state->compress_level = DEFAULT_COMPRESS_LEVEL;

  /* parse the command line args */
  if (parse_args(backend, argc, argv) != 0) {
    return -1;
  }

  /* if specified, open the output file */
  if (state->kafka_file != NULL &&
      (state->outfile = wandio_wcreate(
         state->kafka_file, wandio_detect_compression_type(state->kafka_file),
         state->compress_level, O_CREAT)) == NULL) {
    timeseries_log(__func__, "failed to open output file '%s'",
                   state->kafka_file);
    return -1;
  }

  /* ready to rock n roll */

  return 0;
}

void timeseries_backend_kafka_free(timeseries_backend_t *backend)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  if (state != NULL) {
    if (state->kafka_file != NULL) {
      free(state->kafka_file);
      state->kafka_file = NULL;
    }

    if (state->outfile != NULL) {
      wandio_wdestroy(state->outfile);
      state->outfile = NULL;
    }

    timeseries_backend_free_state(backend);
  }
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

  /* there are at most 10 digits in a 32bit unix time value, plus the nul */
  char time_buffer[11];

  /* we really only need to convert the time value to a string once */
  snprintf(time_buffer, 11, "%" PRIu32, time);

  TIMESERIES_KP_FOREACH_KI(kp, ki, id)
  {
    if (timeseries_kp_ki_enabled(ki) != 0) {
      DUMP_METRIC(state, timeseries_kp_ki_get_key(ki),
                  timeseries_kp_ki_get_value(ki), time_buffer);
    }
  }

  return 0;
}

int timeseries_backend_kafka_set_single(timeseries_backend_t *backend,
                                        const char *key, uint64_t value,
                                        uint32_t time)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);

  /* there are at most 10 digits in a 32bit unix time value, plus the nul */
  char time_buffer[11];

  /* we really only need to convert the time value to a string once */
  snprintf(time_buffer, 11, "%" PRIu32, time);

  DUMP_METRIC(state, key, value, time_buffer);
  return 0;
}

int timeseries_backend_kafka_set_single_by_id(timeseries_backend_t *backend,
                                              uint8_t *id, size_t id_len,
                                              uint64_t value, uint32_t time)
{
  /* the kafka backend ID is just the key, decode and call set single */
  return timeseries_backend_kafka_set_single(backend, (char *)id, value, time);
}

int timeseries_backend_kafka_set_bulk_init(timeseries_backend_t *backend,
                                           uint32_t key_cnt, uint32_t time)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);

  assert(state->bulk_expect == 0 && state->bulk_cnt == 0);
  state->bulk_expect = key_cnt;
  state->bulk_time = time;
  return 0;
}

int timeseries_backend_kafka_set_bulk_by_id(timeseries_backend_t *backend,
                                            uint8_t *id, size_t id_len,
                                            uint64_t value)
{
  timeseries_backend_kafka_state_t *state = STATE(backend);
  assert(state->bulk_expect > 0);

  if (timeseries_backend_kafka_set_single_by_id(backend, id, id_len, value,
                                                state->bulk_time) != 0) {
    return -1;
  }

  if (++state->bulk_cnt == state->bulk_expect) {
    state->bulk_cnt = 0;
    state->bulk_time = 0;
    state->bulk_expect = 0;
  }
  return 0;
}

size_t timeseries_backend_kafka_resolve_key(timeseries_backend_t *backend,
                                            const char *key,
                                            uint8_t **backend_key)
{
  if ((*backend_key = (uint8_t *)strdup(key)) == NULL) {
    return 0;
  }
  return strlen(key) + 1;
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
