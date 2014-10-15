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

#define KEY_LOOKUP_CNT 1

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -b <broker-uri>    0MQ-style URI to connect to broker on\n"
	  "                          (default: %s)\n"
          "       -n <key-cnt>       Number of keys to lookup and insert fake data for (default: %d)\n"
	  "       -r <retries>       Number of times to resend a request\n"
	  "                          (default: %d)\n"
	  "       -t <timeout>       Time to wait before resending a request\n"
	  "                          (default: %d)\n",
	  name,
	  TSMQ_MD_CLIENT_BROKER_URI_DEFAULT,
          KEY_LOOKUP_CNT,
	  TSMQ_MD_CLIENT_REQUEST_RETRIES,
	  TSMQ_MD_CLIENT_REQUEST_TIMEOUT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *broker_uri = NULL;

  int retries = TSMQ_MD_CLIENT_REQUEST_RETRIES;
  int timeout = TSMQ_MD_CLIENT_REQUEST_TIMEOUT;

  int key_cnt = KEY_LOOKUP_CNT;

  tsmq_md_client_t *client;

  int i;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":b:n:r:t:v?")) >= 0)
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

        case 'n':
          key_cnt = atoi(optarg);
          break;

	case 'r':
	  retries = atoi(optarg);
	  break;

	case 't':
	  timeout = atoi(optarg);
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

  if((client = tsmq_md_client_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize tsmq metadata client\n");
      goto err;
    }

  if(broker_uri != NULL)
    {
      tsmq_md_client_set_broker_uri(client, broker_uri);
    }

  tsmq_md_client_set_request_timeout(client, timeout);

  tsmq_md_client_set_request_retries(client, retries);

  if(tsmq_md_client_start(client) != 0)
    {
      tsmq_md_client_perr(client);
      return -1;
    }

  /* debug !! */
  char *key = "a.test.key";
  size_t len = strlen(key);
  tsmq_md_client_key_t *response;

  fprintf(stdout, "Resolving server id for %d keys (%s)\n",
          KEY_LOOKUP_CNT, key);

  for(i=0; i<key_cnt; i++)
    {
      if((response =
          tsmq_md_client_key_lookup(client, (uint8_t*)key, len)) == NULL)
        {
          tsmq_md_client_perr(client);
          return -1;
        }
      tsmq_md_client_key_free(&response);
    }

  fprintf(stdout, "Key lookup successful for %d keys (%s)\n",
          KEY_LOOKUP_CNT, key);

  /* cleanup */
  tsmq_md_client_free(client);

  /* complete successfully */
  return 0;

 err:
  if(client != NULL)
    {
      tsmq_md_client_free(client);
    }
  return -1;
}
