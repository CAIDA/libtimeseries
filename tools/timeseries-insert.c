/*
 * libtimeseries
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wandio.h>

#include "timeseries.h"

#include "config.h"

#define BUFFER_LEN 1024

static timeseries_t *timeseries = NULL;
static timeseries_kp_t *kp = NULL;
static int points_pending = 0;

static int batch_mode = 0;
static int gtime = 0;

static int insert(char *line)
{
  char *end = NULL;
  char *key = NULL;
  char *value_str = NULL;
  char *time_str = NULL;
  uint64_t value = 0;
  uint32_t time = 0;

  int key_id = -1;

  if (line == NULL) {
    return 0;
  }

  /* line format is "<key> <value> <time>" */

  /* get the key string */
  if ((key = strsep(&line, " ")) == NULL) {
    /* malformed line */
    fprintf(stderr, "ERROR: Malformed metric record (missing key): %s\n", key);
    return 0;
  }

  /* get the value string */
  if ((value_str = strsep(&line, " ")) == NULL) {
    /* malformed line */
    fprintf(stderr, "ERROR: Malformed metric record (missing value): %s\n",
            key);
    return 0;
  }
  /* parse the value */
  value = strtoull(value_str, &end, 10);
  if (end == value_str || *end != '\0' || errno == ERANGE) {
    fprintf(stderr, "ERROR: Invalid metric value for '%s': '%s'\n", key,
            value_str);
    return 0;
  }

  /* get the time string */
  if ((time_str = strsep(&line, " ")) == NULL) {
    /* malformed line */
    fprintf(stderr, "ERROR: Malformed metric record (missing time): '%s %s'\n",
            key, value_str);
    return 0;
  }
  /* parse the time */
  time = strtoul(time_str, &end, 10);
  if (end == time_str || *end != '\0' || errno == ERANGE) {
    fprintf(stderr, "ERROR: Invalid metric time for '%s %s': '%s'\n", key,
            value_str, time_str);
    return 0;
  }

  if (batch_mode == 0) {
    if (timeseries_set_single(timeseries, key, value, time) != 0) {
      return -1;
    }
  } else {
    /* use kp */
    if (gtime == 0) {
      gtime = time;
    }
    if (gtime != time) {
      fprintf(stderr, "Flushing table at time %d\n", gtime);
      if (timeseries_kp_flush(kp, gtime) != 0) {
        fprintf(stderr, "ERROR: Could not flush table\n");
        return -1;
      }
      gtime = time;
      points_pending = 0;
    }

    /* attempt to get id for this key */
    if ((key_id = timeseries_kp_get_key(kp, key)) == -1 &&
        (key_id = timeseries_kp_add_key(kp, key)) == -1) {
      fprintf(stderr, "ERROR: Could not add key (%s) to KP\n", key);
      return -1;
    }
    assert(key_id >= 0);

    timeseries_kp_set(kp, key_id, value);
    points_pending++;
  }

  return 0;
}

static void backend_usage()
{
  assert(timeseries != NULL);
  timeseries_backend_t **avail_backends = NULL;
  int i;

  /* get the available backends from libtimeseries */
  avail_backends = timeseries_get_all_backends(timeseries);

  fprintf(stderr, "                            available backends:\n");
  for (i = 0; i < TIMESERIES_BACKEND_ID_LAST; i++) {
    /* skip unavailable backends */
    if (avail_backends[i] == NULL) {
      continue;
    }

    assert(timeseries_backend_get_name(avail_backends[i]));
    fprintf(stderr, "                            - %s\n",
            timeseries_backend_get_name(avail_backends[i]));
  }
}

static void usage(const char *name)
{
  fprintf(
    stderr,
    "usage: %s -t <ts-backend> [<options>]\n"
    "       -b                 Simulate batch insert mode (may be slower)\n"
    "       -f <input-file>    File to read time series data from (default: "
    "stdin)\n"
    "       -t <ts-backend>    Timeseries backend to use for writing\n",
    name);
  backend_usage();
}

static int init_timeseries(char *ts_backend)
{
  char *strcpy = NULL;
  char *args = NULL;

  timeseries_backend_t *backend;

  if ((strcpy = strdup(ts_backend)) == NULL) {
    goto err;
  }

  if ((args = strchr(ts_backend, ' ')) != NULL) {
    /* set the space to a nul, which allows ts_backend to be used
       for the backend name, and then increment args ptr to
       point to the next character, which will be the start of the
       arg string (or at worst case, the terminating \0 */
    *args = '\0';
    args++;
  }

  if ((backend = timeseries_get_backend_by_name(timeseries, ts_backend)) ==
      NULL) {
    fprintf(stderr, "ERROR: Invalid backend name (%s)\n", ts_backend);
    goto err;
  }

  if (timeseries_enable_backend(backend, args) != 0) {
    fprintf(stderr, "ERROR: Failed to initialize backend (%s)\n", ts_backend);
    goto err;
  }

  free(strcpy);

  return 0;

err:
  if (strcpy != NULL) {
    free(strcpy);
  }
  return -1;
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  char *ts_backend[TIMESERIES_BACKEND_ID_LAST];
  int ts_backend_cnt = 0;

  int i;

  char *input_file = "-";
  io_t *infile = NULL;
  char buffer[BUFFER_LEN];

  /* better just grab a pointer to lts before anybody goes crazy and starts
     dumping usage strings */
  if ((timeseries = timeseries_init()) == NULL) {
    fprintf(stderr, "ERROR: Could not initialize libtimeseries\n");
    return -1;
  }

  while (prevoptind = optind, (opt = getopt(argc, argv, ":bf:t:v?")) >= 0) {
    if (optind == prevoptind + 2 && (optarg == NULL || *optarg == '-')) {
      opt = ':';
      --optind;
    }
    switch (opt) {
    case ':':
      fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
      usage(argv[0]);
      return -1;
      break;

    case 'b':
      batch_mode = 1;
      break;

    case 'f':
      input_file = optarg;
      break;

    case 't':
      if (ts_backend_cnt >= TIMESERIES_BACKEND_ID_LAST - 1) {
        fprintf(stderr, "ERROR: At most %d backends can be enabled\n",
                TIMESERIES_BACKEND_ID_LAST);
        usage(argv[0]);
        return -1;
      }
      ts_backend[ts_backend_cnt++] = optarg;
      break;

    case '?':
    case 'v':
      fprintf(stderr, "libtimeseries version %d.%d.%d\n",
              LIBTIMESERIES_MAJOR_VERSION, LIBTIMESERIES_MID_VERSION,
              LIBTIMESERIES_MINOR_VERSION);
      usage(argv[0]);
      return 0;
      break;

    default:
      usage(argv[0]);
      return -1;
      break;
    }
  }

  /* NB: once getopt completes, optind points to the first non-option
     argument */

  if (ts_backend_cnt == 0) {
    fprintf(stderr, "ERROR: Timeseries backend(s) must be specified\n");
    usage(argv[0]);
    return -1;
  }

  for (i = 0; i < ts_backend_cnt; i++) {
    assert(ts_backend[i] != NULL);
    if (init_timeseries(ts_backend[i]) != 0) {
      usage(argv[0]);
      goto err;
    }
  }

  assert(timeseries != NULL);

  if (batch_mode != 0) {
    fprintf(stderr, "INFO: Using batch mode (Key Package)\n");
    if ((kp = timeseries_kp_init(timeseries, 1)) == NULL) {
      fprintf(stderr, "ERROR: Could not create Key Package\n");
    }
  }

  fprintf(stderr, "INFO: Reading metrics from %s\n", input_file);
  /* open the input file, (defaults to stdin) */
  if ((infile = wandio_create(input_file)) == NULL) {
    fprintf(stderr, "ERROR: Could not open %s for reading\n", input_file);
    usage(argv[0]);
    goto err;
  }

  /* read from the input file (chomp off the newlines) */
  while (wandio_fgets(infile, &buffer, BUFFER_LEN, 1) > 0) {
    /* treat # as comment line, and ignore empty lines */
    if (buffer[0] == '#' || buffer[0] == '\0') {
      continue;
    }

    if (insert(buffer) != 0) {
      goto err;
    }
  }

  if (batch_mode != 0 && points_pending > 0) {
    fprintf(stderr, "Flushing final table at time %d\n", gtime);
    if (timeseries_kp_flush(kp, gtime) != 0) {
      fprintf(stderr, "ERROR: Could not flush table\n");
      return -1;
    }
  }

  /* free the kp */
  timeseries_kp_free(&kp);
  /* free timeseries, backends will be free'd */
  timeseries_free(&timeseries);

  wandio_destroy(infile);

  /* complete successfully */
  return 0;

err:
  timeseries_kp_free(&kp);
  timeseries_free(&timeseries);
  return -1;
}
