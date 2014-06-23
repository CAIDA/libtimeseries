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

#include "tsmq_int.h"

#include "utils.h"

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(md_broker)

tsmq_md_broker_t *tsmq_md_broker_init()
{
  tsmq_md_broker_t *broker;
  if((broker = malloc_zero(sizeof(tsmq_md_broker_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }

  if((broker->tsmq = tsmq_init()) == NULL)
    {
      /* still cannot set an error */
      return NULL;
    }

  broker->client_uri = strdup(TSMQ_MD_BROKER_CLIENT_URI_DEFAULT);

  broker->server_uri = strdup(TSMQ_MD_BROKER_CLIENT_URI_DEFAULT);

  /* now we are ready to set errors... */

  return broker;
}

int tsmq_md_broker_start(tsmq_md_broker_t *broker)
{
  tsmq_set_err(broker->tsmq, TSMQ_ERR_INTERRUPT, "SIGINT caught, exiting");
  return -1;
}

void tsmq_md_broker_free(tsmq_md_broker_t *broker)
{
  assert(broker != NULL);
  assert(broker->tsmq != NULL);

  if(broker->client_uri != NULL)
    {
      free(broker->client_uri);
    }

  if(broker->server_uri != NULL)
    {
      free(broker->server_uri);
    }

  tsmq_free(broker->tsmq);
  free(broker);

  return;
}

void tsmq_md_broker_set_client_uri(tsmq_md_broker_t *broker, const char *uri)
{
  assert(broker != NULL);

  /* remember, we set one by default */
  assert(broker->cient_uri != NULL);
  free(broker->client_uri);

  broker->client_uri = strdup(uri);
}

void tsmq_md_broker_set_server_uri(tsmq_md_broker_t *broker, const char *uri)
{
  assert(broker != NULL);

  /* remember, we set one by default */
  assert(broker->server_uri != NULL);
  free(broker->server_uri);

  broker->server_uri = strdup(uri);
}
