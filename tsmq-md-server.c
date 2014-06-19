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
#include <unistd.h>

/* include tsmq's public interface */
/* @@ never include the _int.h file from tools. */
#include "tsmq.h"

#define COUNT_DEFAULT 1
#define COUNT_MIN     0
#define COUNT_MAX     100

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [-c count] name\n"
	  "       -c <count>    the number of times to repeat (default: %d)\n",
	  name,
	  COUNT_DEFAULT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int lastopt;
  int prevoptind;

  /* to store command line argument values */
  int count = COUNT_DEFAULT;
  const char *name;

  int i;

  tsmq_t *tsmq;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":c:?")) >= 0)
    {
      if (optind == prevoptind + 2 && *optarg == '-' ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case 'c':
	  count = atoi(optarg);
	  break;

	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage(argv[0]);
	  return -1;
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

  if(count < COUNT_MIN || count > COUNT_MAX)
    {
      fprintf(stderr, "ERROR: -c argument must be between %d and %d (%d given)\n",
	      COUNT_MIN, COUNT_MAX, count);
      return -1;
    }

  /* once getopt completes, optind points to the first non-option argument */
  if(optind >= argc)
    {
      fprintf(stderr, "ERROR: a name must be given\n");
      usage(argv[0]);
      return -1;
    }

  name = argv[optind];

  for(i = 0; i < count; i++)
    {
      if((tsmq = tsmq_init()) == NULL)
	{
	  fprintf(stderr, "ERROR: could not initialize tsmq\n");
	  return -1;
	}
    }

  /* complete successfully */
  return 0;
}
