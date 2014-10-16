/*
 * tsmq
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of tsmq.
 *
 * tsmq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tsmq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tsmq.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tsmq.h>

#include <libtimeseries.h>

#include "config.h"

timeseries_t *timeseries = NULL;
timeseries_backend_t *backend = NULL;

static size_t handle_key_lookup(tsmq_md_server_t *server,
                                char *key, uint8_t **server_key,
                                void *user)
{
  return timeseries_resolve_key(backend, key, server_key);
}

static void backend_usage()
{
  assert(timeseries != NULL);
  timeseries_backend_t **backends = NULL;
  int i;

  /* get the available backends from libtimeseries */
  backends = timeseries_get_all_backends(timeseries);

  fprintf(stderr,
	  "                            available backends:\n");
  for(i = 0; i < TIMESERIES_BACKEND_ID_LAST; i++)
    {
      /* skip unavailable backends */
      if(backends[i] == NULL)
	{
	  continue;
	}

      assert(timeseries_get_backend_name(backends[i]));
      fprintf(stderr,
	      "                            - %s\n",
	      timeseries_get_backend_name(backends[i]));
    }
}

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>] -t <ts-backend>\n"
	  "       -b <broker-uri>    0MQ-style URI to connect to broker on\n"
	  "                          (default: %s)\n"
	  "       -i <interval-ms>   Time in ms between heartbeats to broker\n"
	  "                          (default: %d)\n"
	  "       -l <beats>         Number of heartbeats that can go by before \n"
	  "                          the broker is declared dead (default: %d)\n"
	  "       -r <retry-min>     Min time in ms to wait before reconnecting to broker\n"

	  "                          (default: %d)\n"
	  "       -R <retry-max>     Max time in ms to wait before reconnecting to broker\n"
	  "                          (default: %d)\n"
	  "       -t <ts-backend>    Timeseries backend to use for writing\n",
	  name,
	  TSMQ_MD_SERVER_BROKER_URI_DEFAULT,
	  TSMQ_MD_HEARTBEAT_INTERVAL_DEFAULT,
	  TSMQ_MD_HEARTBEAT_LIVENESS_DEFAULT,
	  TSMQ_MD_RECONNECT_INTERVAL_MIN,
	  TSMQ_MD_RECONNECT_INTERVAL_MAX);
  backend_usage();
}

static int init_timeseries(const char *ts_backend)
{
  char *strcpy = NULL;
  char *args = NULL;

  if((strcpy = strdup(ts_backend)) == NULL)
    {
      goto err;
    }

  if((args = strchr(ts_backend, ' ')) != NULL)
    {
      /* set the space to a nul, which allows ts_backend to be used
	 for the backend name, and then increment args ptr to
	 point to the next character, which will be the start of the
	 arg string (or at worst case, the terminating \0 */
      *args = '\0';
      args++;
    }

  if((backend = timeseries_get_backend_by_name(timeseries, ts_backend)) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid backend name (%s)\n",
	      ts_backend);
      goto err;
    }

  if(timeseries_enable_backend(timeseries, backend, args) != 0)
    {
      fprintf(stderr, "ERROR: Failed to initialized backend (%s)\n",
	      ts_backend);
      goto err;
    }

  free(strcpy);

  return 0;

 err:
  if(strcpy != NULL)
    {
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
  const char *broker_uri = NULL;
  const char *ts_backend = NULL;

  uint64_t heartbeat_interval = TSMQ_MD_HEARTBEAT_INTERVAL_DEFAULT;
  int heartbeat_liveness      = TSMQ_MD_HEARTBEAT_LIVENESS_DEFAULT;
  uint64_t reconnect_interval_min = TSMQ_MD_RECONNECT_INTERVAL_MIN;
  uint64_t reconnect_interval_max = TSMQ_MD_RECONNECT_INTERVAL_MAX;

  tsmq_md_server_t *server = NULL;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":b:i:l:r:R:t:v?")) >= 0)
    {
      if (optind == prevoptind + 2 && *optarg == '-' ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage(argv[0]);
	  return -1;
	  break;

	case 'b':
	  broker_uri = optarg;
	  break;

	case 'i':
	  heartbeat_interval = atoi(optarg);
	  break;

	case 'l':
	  heartbeat_liveness = atoi(optarg);
	  break;

	case 'r':
	  reconnect_interval_min = atoi(optarg);
	  break;

	case 'R':
	  reconnect_interval_max = atoi(optarg);
	  break;

	case 't':
	  ts_backend = optarg;
	  break;

	case '?':
	case 'v':
	  fprintf(stderr, "libtimeseries version %d.%d.%d\n",
		  LIBTIMESERIES_MAJOR_VERSION,
		  LIBTIMESERIES_MID_VERSION,
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

  /* better just grab a pointer to lts before anybody goes crazy and starts
     dumping usage strings */
  if((timeseries = timeseries_init()) == NULL)
    {
      fprintf(stderr, "ERROR: Could not initialize libtimeseries\n");
      return -1;
    }

  if(ts_backend == NULL)
    {
      fprintf(stderr,
	      "ERROR: Timeseries backend must be specified\n");
      usage(argv[0]);
      return -1;
    }

  if(init_timeseries(ts_backend) != 0)
    {
      usage(argv[0]);
      goto err;
    }
  assert(timeseries != NULL && backend != NULL);

  if((server = tsmq_md_server_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize tsmq server\n");
      usage(argv[0]);
      goto err;
    }

  if(broker_uri != NULL)
    {
      tsmq_md_server_set_broker_uri(server, broker_uri);
    }

  tsmq_md_server_set_heartbeat_interval(server, heartbeat_interval);

  tsmq_md_server_set_heartbeat_liveness(server, heartbeat_liveness);

  tsmq_md_server_set_reconnect_interval_min(server, reconnect_interval_min);

  tsmq_md_server_set_reconnect_interval_max(server, reconnect_interval_max);

  tsmq_md_server_set_cb_key_lookup(server, handle_key_lookup);

  /* do work */
  /* this function will block until the broker shuts down */
  /* @todo add a structure of callback functions for the server to call when
     messages are received */
  tsmq_md_server_start(server);

  /* this will always be set, normally to a SIGINT-caught message */
  tsmq_md_server_perr(server);

  /* cleanup */
  tsmq_md_server_free(server);

  /* complete successfully */
  return 0;

 err:
  if(server != NULL) {
    tsmq_md_server_free(server);
  }
  return -1;
}
