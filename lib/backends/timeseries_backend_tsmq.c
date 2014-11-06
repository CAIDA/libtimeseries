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

#include <tsmq_client.h>

#include "utils.h"

#include "timeseries_backend_int.h"
#include "timeseries_kp_int.h"
#include "timeseries_log_int.h"
#include "timeseries_backend_tsmq.h"

#define BACKEND_NAME "tsmq"

#define STATE(provname)				\
  (TIMESERIES_BACKEND_STATE(tsmq, provname))

/** The basic fields that every instance of this backend have in common */
static timeseries_backend_t timeseries_backend_tsmq = {
  TIMESERIES_BACKEND_ID_TSMQ,
  BACKEND_NAME,
  TIMESERIES_BACKEND_GENERATE_PTRS(tsmq)
};

/** Holds the state for an instance of this backend */
typedef struct timeseries_backend_tsmq_state {
  /** TSMQ client instance to send metrics to */
  tsmq_client_t *client;

} timeseries_backend_tsmq_state_t;

/** Print usage information to stderr */
static void usage(timeseries_backend_t *backend)
{
  fprintf(stderr,
	  "backend usage: %s [<options>]\n"
	  "       -b <broker-uri>    0MQ-style URI to connect to broker on\n"
	  "                          (default: %s)\n"
	  "       -r <retries>       Number of times to resend a request\n"
	  "                          (default: %d)\n"
	  "       -t <timeout>       Time to wait before resending a request\n"
	  "                          (default: %d)\n",
	  backend->name,
	  TSMQ_CLIENT_BROKER_URI_DEFAULT,
	  TSMQ_CLIENT_REQUEST_RETRIES,
	  TSMQ_CLIENT_REQUEST_TIMEOUT);
}


/** Parse the arguments given to the backend */
static int parse_args(timeseries_backend_t *backend, int argc, char **argv)
{
  timeseries_backend_tsmq_state_t *state = STATE(backend);
  int opt;

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */

  assert(state->client != NULL);

  while((opt = getopt(argc, argv, ":b:r:t:?")) >= 0)
    {
      switch(opt)
	{
	case 'b':
	  tsmq_client_set_broker_uri(state->client, optarg);
	  break;

	case 'r':
	  tsmq_client_set_request_retries(state->client, atoi(optarg));
	  break;

	case 't':
	  tsmq_client_set_request_timeout(state->client, atoi(optarg));
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

timeseries_backend_t *timeseries_backend_tsmq_alloc()
{
  return &timeseries_backend_tsmq;
}

int timeseries_backend_tsmq_init(timeseries_backend_t *backend,
				 int argc, char ** argv)
{
  timeseries_backend_tsmq_state_t *state;

  /* allocate our state */
  if((state = malloc_zero(sizeof(timeseries_backend_tsmq_state_t)))
     == NULL)
    {
      timeseries_log(__func__,
		  "could not malloc timeseries_backend_tsmq_state_t");
      return -1;
    }
  timeseries_backend_register_state(backend, state);

  /* create a tsmq client instance (MUST be init before parse_args) */
  if((state->client = tsmq_client_init()) == NULL)
    {
      timeseries_log(__func__, "could not init tsmq client");
      return -1;
    }

  /* parse the command line args */
  if(parse_args(backend, argc, argv) != 0)
    {
      return -1;
    }

  if(tsmq_client_start(state->client) != 0)
    {
      tsmq_client_perr(state->client);
      return -1;
    }

  /* ready to rock n roll */

  return 0;
}

void timeseries_backend_tsmq_free(timeseries_backend_t *backend)
{
  timeseries_backend_tsmq_state_t *state = STATE(backend);
  if(state != NULL)
    {
      tsmq_client_free(&state->client);

      timeseries_backend_free_state(backend);
    }
  return;
}

int timeseries_backend_tsmq_kp_init(timeseries_backend_t *backend,
				    timeseries_kp_t *kp,
				    void **kp_state_p)
{
  /* we don't need any kp-specific state */
  assert(kp_state_p != NULL);
  *kp_state_p = NULL;
  return 0;
}

void timeseries_backend_tsmq_kp_free(timeseries_backend_t *backend,
				     timeseries_kp_t *kp,
				     void *kp_state)
{
  /* we stored no state in the kp */
  assert(kp_state == NULL);
  return;
}

int timeseries_backend_tsmq_kp_ki_update(timeseries_backend_t *backend,
					 timeseries_kp_t *kp)
{
  assert(0);
  return 0;
}

void timeseries_backend_tsmq_kp_ki_free(timeseries_backend_t *backend,
                                       timeseries_kp_t *kp,
				       timeseries_kp_ki_t *ki,
                                       void *ki_state)
{
  assert(0);
  return;
}

int timeseries_backend_tsmq_kp_flush(timeseries_backend_t *backend,
				      timeseries_kp_t *kp,
				      uint32_t time)
{
  assert(0);
  return 0;
}

int timeseries_backend_tsmq_set_single(timeseries_backend_t *backend,
					const char *key,
					uint64_t value,
					uint32_t time)
{
  timeseries_backend_tsmq_state_t *state = STATE(backend);

  int rc;
  tsmq_client_key_t *ck = NULL;

  if((ck = tsmq_client_key_lookup(state->client, key)) == NULL)
    {
      return -1;
    }

  rc = tsmq_client_key_set_single(state->client, ck, value, time);

  tsmq_client_key_free(&ck);
  return rc;
}

int timeseries_backend_tsmq_set_single_by_id(timeseries_backend_t *backend,
                                              uint8_t *id, size_t id_len,
                                              uint64_t value, uint32_t time)
{
  /* this would happen when chaining tsmq instances.
     think some more about what this means */
  assert(0 && "unimplemented");
  return -1;
}

size_t timeseries_backend_tsmq_resolve_key(timeseries_backend_t *backend,
                                            const char *key,
                                            uint8_t **backend_key)
{
  /* as with above */
  assert(0 && "unimplemented");
  return -1;
}
