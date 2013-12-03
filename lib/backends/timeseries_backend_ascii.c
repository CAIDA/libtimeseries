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
#include "libtimeseries_int.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "wandio.h"

#include "utils.h"

#include "timeseries_backend_ascii.h"

#define BACKEND_NAME "ascii"

#define DEFAULT_COMPRESS_LEVEL 6

#define STATE(provname)				\
  (TIMESERIES_BACKEND_STATE(ascii, provname))

/** The basic fields that every instance of this backend have in common */
static timeseries_backend_t timeseries_backend_ascii = {
  TIMESERIES_BACKEND_ASCII,
  BACKEND_NAME,
  TIMESERIES_BACKEND_GENERATE_PTRS(ascii)
};

/** Holds the state for an instance of this backend */
typedef struct timeseries_backend_ascii_state {
  /** The filename to write metrics out to */
  char *ascii_file;

  /** A wandio output file pointer to write metrics to */
  iow_t *outfile;

  /** The compression level to use of the outfile is compressed */
  int compress_level;

} timeseries_backend_ascii_state_t;

/** Print usage information to stderr */
static void usage(timeseries_backend_t *backend)
{
  fprintf(stderr,
	  "backend usage: %s [-c compress-level] [-f output-file]\n"
	  "       -c <level>    output compression level to use (default: %d)\n"
	  "       -f            file to write ASCII timeseries metrics to\n",
	  backend->name,
	  DEFAULT_COMPRESS_LEVEL);
}


/** Parse the arguments given to the backend */
static int parse_args(timeseries_backend_t *backend, int argc, char **argv)
{
  timeseries_backend_ascii_state_t *state = STATE(backend);
  int opt;

  assert(argc > 0 && argv != NULL);

  if(argc == 1)
    {
      usage(backend);
      return -1;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */

  while((opt = getopt(argc, argv, ":c:f:?")) >= 0)
    {
      switch(opt)
	{

	case 'c':
	  state->compress_level = atoi(optarg);
	  break;

	case 'f':
	  state->ascii_file = strdup(optarg);
	  break;

	case '?':
	case ':':
	default:
	  usage(backend);
	  return -1;
	}
    }

  return 0;
}

/* ===== PUBLIC FUNCTIONS BELOW THIS POINT ===== */

timeseries_backend_t *timeseries_backend_ascii_alloc()
{
  return &timeseries_backend_ascii;
}

int timeseries_backend_ascii_init(timeseries_backend_t *backend,
				 int argc, char ** argv)
{
  timeseries_backend_ascii_state_t *state;

  /* allocate our state */
  if((state = malloc_zero(sizeof(timeseries_backend_ascii_state_t)))
     == NULL)
    {
      timeseries_log(__func__,
		  "could not malloc timeseries_backend_ascii_state_t");
      return -1;
    }
  timeseries_backend_register_state(backend, state);

  /* set initial default values (that can be overridden on the command line) */
  state->compress_level = DEFAULT_COMPRESS_LEVEL;

  /* parse the command line args */
  if(parse_args(backend, argc, argv) != 0)
    {
      usage(backend);
      return -1;
    }

  /* if specified, open the output file */
  if(state->ascii_file != NULL &&
     (state->outfile =
      wandio_wcreate(state->ascii_file,
		    wandio_detect_compression_type(state->ascii_file),
		    state->compress_level,
		    O_CREAT)) == NULL)
    {
      timeseries_log(__func__,
		 "failed to open output file '%s'", state->ascii_file);
      return -1;
    }

  /* ready to rock n roll */

  return 0;
}

void timeseries_backend_ascii_free(timeseries_backend_t *backend)
{
  timeseries_backend_ascii_state_t *state = STATE(backend);
  if(state != NULL)
    {
      if(state->ascii_file != NULL)
	{
	  free(state->ascii_file);
	  state->ascii_file = NULL;
	}

      if(state->outfile != NULL)
	{
	  wandio_wdestroy(state->outfile);
	  state->outfile = NULL;
	}

      timeseries_backend_free_state(backend);
    }
  return;
}

#define PRINT_METRIC(func, file, key, value, time)		\
  do {								\
    func(file, "%s %"PRIu64" %"PRIu32"\n", key, value, time);	\
  } while(0)

#define DUMP_METRIC(state, key, value, time)				\
  do {									\
  if(state->outfile != NULL)						\
    {									\
      PRINT_METRIC(wandio_printf, state->outfile, key, value, time);	\
    }									\
  else									\
    {									\
      PRINT_METRIC(fprintf, stdout, key, value, time);			\
    }									\
  } while(0)


int timeseries_backend_ascii_kp_flush(timeseries_backend_t *backend,
				      timeseries_kp_t *kp,
				      uint32_t time)
{
  timeseries_backend_ascii_state_t *state = STATE(backend);
  int i;

  for(i = 0; i < kp->keys_cnt; i++)
    {
      DUMP_METRIC(state, kp->keys[i], kp->values[i], time);
    }

  return 0;
}

int timeseries_backend_ascii_set_single(timeseries_backend_t *backend,
					const char *key,
					uint64_t value,
					uint32_t time)
{
  timeseries_backend_ascii_state_t *state = STATE(backend);
  DUMP_METRIC(state, key, value, time);
  return 0;
}
