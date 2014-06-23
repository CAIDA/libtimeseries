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
#include <string.h>

#include "tsmq_int.h"

#include "utils.h"

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(md_server)

tsmq_md_server_t *tsmq_md_server_init()
{
  tsmq_md_server_t *server;
  if((server = malloc_zero(sizeof(tsmq_md_server_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }

  if((server->tsmq = tsmq_init()) == NULL)
    {
      /* still cannot set an error */
      return NULL;
    }

  /* now we are ready to set errors... */

  return server;
}

int tsmq_md_server_start(tsmq_md_server_t *server)
{
  return -1;
}

void tsmq_md_server_free(tsmq_md_server_t *server)
{
  return;
}

void tsmq_md_server_set_broker_uri(tsmq_md_server_t *server, const char *uri)
{
  assert(server != NULL);

  server->uri = strdup(uri);
}
