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

/* include tsmq's public interface */
/* @@ never include the _int.h file from tools. */
#include "tsmq.h"

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

  tsmq_md_server_t *server;

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
	  fprintf(stderr, "tsmq version %d.%d.%d\n",
		  TSMQ_MAJOR_VERSION,
		  TSMQ_MID_VERSION,
		  TSMQ_MINOR_VERSION);
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

  if(ts_backend == NULL)
    {
      fprintf(stderr,
	      "Timeseries backend must be specified "
	      "(use `-t ?` to get a list)\n");
      usage(argv[0]);
      return -1;
    }

  if((server = tsmq_md_server_init(ts_backend)) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize tsmq metadata server\n");
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

  /* do work */
  /* this function will block until the broker shuts down */
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
