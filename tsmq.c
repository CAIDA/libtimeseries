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

/* ---------- PRIVATE API FUNCTIONS BELOW HERE ---------- */
/* ---------- see tsmq_int.h for declarations ---------- */

tsmq_t *tsmq_init()
{
  tsmq_t *tsmq;

  if((tsmq = malloc_zero(sizeof(tsmq_t))) == NULL)
    {
      return NULL;
    }

  if((tsmq->ctx = zctx_new()) == NULL)
    {
      tsmq_set_err(tsmq, TSMQ_ERR_INIT_FAILED, "Failed to create 0MQ context");
      return NULL;
    }

  return tsmq;
}

int tsmq_start(tsmq_t *tsmq)
{
  /* nothing to be done */
  return 0;
}

void tsmq_free(tsmq_t *tsmq)
{
  return;
}

void tsmq_set_err(tsmq_t *tsmq, int errcode, const char *msg, ...)
{
  char buf[256];
  va_list va;

  va_start(va,msg);

  assert(errcode != 0 && "An error occurred, but it is unknown what it is");

  tsmq->err.err_num=errcode;

  if (errcode>0) {
    vsnprintf(buf,sizeof(buf),msg,va);
    snprintf(tsmq->err.problem,sizeof(tsmq->err.problem),
	     "%s: %s",buf,strerror(errcode));
  } else {
    vsnprintf(tsmq->err.problem,sizeof(tsmq->err.problem),
	      msg,va);
  }

  va_end(va);
}

tsmq_err_t tsmq_get_err(tsmq_t *tsmq)
{
  tsmq_err_t err = tsmq->err;
  tsmq->err.err_num = 0; /* "OK" */
  tsmq->err.problem[0]='\0';
  return err;
}

int tsmq_is_err(tsmq_t *tsmq)
{
  return tsmq->err.err_num != 0;
}

void tsmq_perr(tsmq_t *tsmq)
{
  if(tsmq->err.err_num) {
    fprintf(stderr,"%s (%d)\n", tsmq->err.problem, tsmq->err.err_num);
  } else {
    fprintf(stderr,"No error\n");
  }
  tsmq->err.err_num = 0; /* "OK" */
  tsmq->err.problem[0]='\0';
}
