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

#include "tsmq_broker.h"

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -c <client-uri>    0MQ-style URI to listen for clients on\n"
	  "                          (default: %s)\n"
	  "       -i <interval-ms>   Time in ms between heartbeats to servers\n"
	  "                          (default: %d)\n"
	  "       -l <beats>         Number of heartbeats that can go by before \n"
	  "                          a server is declared dead (default: %d)\n"
	  "       -s <server-uri>    0MQ-style URI to listen for servers on\n"
	  "                          (default: %s)\n",
	  name,
	  TSMQ_BROKER_CLIENT_URI_DEFAULT,
	  TSMQ_HEARTBEAT_INTERVAL_DEFAULT,
	  TSMQ_HEARTBEAT_LIVENESS_DEFAULT,
	  TSMQ_BROKER_SERVER_URI_DEFAULT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *client_uri = NULL;
  const char *server_uri = NULL;

  uint64_t heartbeat_interval = TSMQ_HEARTBEAT_INTERVAL_DEFAULT;
  int heartbeat_liveness      = TSMQ_HEARTBEAT_LIVENESS_DEFAULT;

  tsmq_broker_t *broker;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":c:i:l:s:v?")) >= 0)
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

	case 'c':
	  client_uri = optarg;
	  break;

	case 'i':
	  heartbeat_interval = atoi(optarg);
	  break;

	case 'l':
	  heartbeat_liveness = atoi(optarg);
	  break;

	case 's':
	  server_uri = optarg;
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

  if((broker = tsmq_broker_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize tsmq metadata broker\n");
      goto err;
    }

  if(client_uri != NULL)
    {
      tsmq_broker_set_client_uri(broker, client_uri);
    }

  if(server_uri != NULL)
    {
      tsmq_broker_set_server_uri(broker, server_uri);
    }

  tsmq_broker_set_heartbeat_interval(broker, heartbeat_interval);

  tsmq_broker_set_heartbeat_liveness(broker, heartbeat_liveness);

  /* do work */
  /* this function will block until the broker shuts down */
  tsmq_broker_start(broker);

  /* this will always be set, normally to a SIGINT-caught message */
  tsmq_broker_perr(broker);

  /* cleanup */
  tsmq_broker_free(&broker);

  /* complete successfully */
  return 0;

 err:
  tsmq_broker_free(&broker);
  return -1;
}
