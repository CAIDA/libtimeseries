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
	  "usage: %s -c client-uri -s server-uri\n"
	  "       -c <client-uri>    0MQ-style URI to listen for clients on\n"
	  "                          (default: %s)\n"
	  "       -s <server-uri>    0MQ-style URI to listen for servers on\n"
	  "                          (default: %s)\n",
	  name,
	  TSMQ_MD_BROKER_CLIENT_URI_DEFAULT,
	  TSMQ_MD_BROKER_SERVER_URI_DEFAULT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *client_uri = NULL;
  const char *server_uri = NULL;

  tsmq_md_broker_t *broker;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":c:s:v?")) >= 0)
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

	case 's':
	  server_uri = optarg;
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

  if((broker = tsmq_md_broker_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize tsmq metadata broker\n");
      goto err;
    }

  if(client_uri != NULL)
    {
      tsmq_md_broker_set_client_uri(broker, client_uri);
    }

  if(server_uri != NULL)
    {
      tsmq_md_broker_set_server_uri(broker, server_uri);
    }

  /* do work */
  if(tsmq_md_broker_start(broker) != 0)
    {
      fprintf(stderr, "INFO: Broker is exiting\n");
      tsmq_md_broker_perr(broker);
      goto err;
    }

  /* cleanup */
  tsmq_md_broker_free(broker);

  /* complete successfully */
  return 0;

 err:
  if(broker != NULL) {
    tsmq_md_broker_free(broker);
  }
  return -1;
}
