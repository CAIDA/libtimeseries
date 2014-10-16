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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_cmd.h"
#include "utils.h"

#include "libtimeseries_int.h"
#include "timeseries_backend.h"

#define MAXOPTS 1024

#define SEPARATOR "|"

timeseries_t *timeseries_init()
{
  timeseries_t *timeseries;

  timeseries_log(__func__, "initializing libtimeseries");

  /* allocate some memory for our state */
  if((timeseries = malloc_zero(sizeof(timeseries_t))) == NULL)
    {
      timeseries_log(__func__, "could not malloc timeseries_t");
      return NULL;
    }

  /* allocate the backends */
  if(timeseries_backend_alloc_all(timeseries) != 0)
    {
      free(timeseries);
      return NULL;
    }

  return timeseries;
}

void timeseries_free(timeseries_t *timeseries)
{
  int i;

  /* no mercy for double frees */
  assert(timeseries != NULL);

  /* loop across all backends and free each one */
  for(i = 0; i < TIMESERIES_BACKEND_ID_LAST; i++)
    {
      timeseries_backend_free(timeseries, timeseries->backends[i]);
    }

  free(timeseries);
  return;
}

int timeseries_enable_backend(timeseries_t *timeseries,
			      timeseries_backend_t *backend,
			      const char *options)
{
  char *local_args = NULL;
  char *process_argv[MAXOPTS];
  int len;
  int process_argc = 0;
  int rc;

  timeseries_log(__func__, "enabling backend (%s)", backend->name);

  /* first we need to parse the options */
  if(options != NULL && (len = strlen(options)) > 0)
    {
      local_args = strndup(options, len);
      parse_cmd(local_args, &process_argc, process_argv, MAXOPTS,
		backend->name);
    }
  else
    {
      process_argv[process_argc++] = (char*)backend->name;
    }

  /* we just need to pass this along to the backend framework */
  rc = timeseries_backend_init(timeseries, backend, process_argc, process_argv);

  if(local_args != NULL)
    {
      free(local_args);
    }

  return rc;
}

inline timeseries_backend_t *timeseries_get_backend_by_id(
					     timeseries_t *timeseries,
					     timeseries_backend_id_t id)
{
  assert(timeseries != NULL);
  if(id < TIMESERIES_BACKEND_ID_FIRST || id > TIMESERIES_BACKEND_ID_LAST)
    {
      return NULL;
    }
  return timeseries->backends[id - 1];
}

timeseries_backend_t *timeseries_get_backend_by_name(timeseries_t *timeseries,
						     const char *name)
{
  timeseries_backend_t *backend;
  int i;

  for(i = TIMESERIES_BACKEND_ID_FIRST; i <= TIMESERIES_BACKEND_ID_LAST; i++)
    {
      if((backend = timeseries_get_backend_by_id(timeseries, i)) != NULL &&
	 strncasecmp(backend->name, name, strlen(backend->name)) == 0)
	{
	  return backend;
	}
    }

  return NULL;
}

inline int timeseries_is_backend_enabled(timeseries_backend_t *backend)
{
  assert(backend != NULL);
  return backend->enabled;
}

inline timeseries_backend_id_t timeseries_get_backend_id(
					      timeseries_backend_t *backend)
{
  assert(backend != NULL);

  return backend->id;
}

inline const char *timeseries_get_backend_name(timeseries_backend_t *backend)
{
  assert(backend != NULL);

  return backend->name;
}

timeseries_backend_t **timeseries_get_all_backends(timeseries_t *timeseries)
{
  return timeseries->backends;
}

timeseries_kp_t *timeseries_kp_init(timeseries_t *timeseries, int reset)
{
  assert(timeseries != NULL);
  timeseries_kp_t *kp = NULL;

  /* we only need to malloc the struct, space for keys will be malloc'd on the
     fly */
  if((kp = malloc_zero(sizeof(timeseries_kp_t))) == NULL)
    {
      timeseries_log(__func__, "could not malloc key package");
      return NULL;
    }

  /* give the backends a chance to do any init they need */
  if(timeseries_backend_kp_init(kp) != 0)
    {
      free(kp);
      return NULL;
    }

  /* save the timeseries pointer */
  kp->timeseries = timeseries;

  /* set the reset flag */
  kp->reset = reset;

  return kp;
}

void timeseries_kp_free(timeseries_kp_t *kp)
{
  int i;

  if(kp != NULL)
    {
      /* free each of the key strings */
      for(i = 0; i < kp->kvs_cnt; i++)
	{
          free(kp->kvs[i].key);
          kp->kvs[i].key = NULL;
	}
      free(kp->kvs);
      kp->kvs = NULL;
      kp->kvs_cnt = 0;

      /* ask each backend that has data here to free it */
      timeseries_backend_kp_free(kp);

      /* free the actual key package structure */
      free(kp);
    }

  return;
}

int timeseries_kp_add_key(timeseries_kp_t *kp, const char *key)
{
  assert(kp != NULL);
  assert(key != NULL);

  /* first we need to realloc the array of keys */
  if((kp->kvs =
      realloc(kp->kvs,
              sizeof(timeseries_kp_kv_t) * (kp->kvs_cnt+1))) == NULL)
    {
      timeseries_log(__func__, "could not realloc key package (keys)");
      return -1;
    }

  /* we promised we would zero the values on addition */
  kp->kvs[kp->kvs_cnt].value = 0;

  /* now add the key (and increment the count) */
  if((kp->kvs[kp->kvs_cnt++].key = strdup(key)) == NULL)
    {
      return -1;
    }

  /* mark the package as dirty so that the backend key ids are updated */
  kp->dirty = 1;

  return -1;
}

void timeseries_kp_set(timeseries_kp_t *kp, uint32_t key, uint64_t value)
{
  assert(kp != NULL);
  assert(key < kp->kvs_cnt);

  kp->kvs[key].value = value;
}

int timeseries_kp_flush(timeseries_backend_t *backend,
			timeseries_kp_t *kp, uint32_t time)
{
  int rc, i;
  assert(backend != NULL && backend->enabled != 0);

  /* if a key has been added since the last flush, then we invite the backends
     to update their kp state */
  if(kp->dirty != 0 && (timeseries_backend_kp_update(kp) != 0))
    {
      return -1;
    }

  rc = backend->kp_flush(backend, kp, time);

  /** @todo is there some way we could reset while flushing? pass the reset
      param to the backend? hmm */
  if(rc == 0 && kp->reset != 0)
    {
      for(i = 0; i < kp->kvs_cnt; i++)
	{
	  kp->kvs[i].value = 0;
	}
    }
  return rc;
}

int timeseries_set_single(timeseries_backend_t *backend, const char *key,
			  uint64_t value, uint32_t time)
{
  assert(backend != NULL && backend->enabled != 0);

  return backend->set_single(backend, key, value, time);
}

size_t timeseries_resolve_key(timeseries_backend_t *backend,
                              const char *key,
                              uint8_t **backend_key)
{
  assert(backend != NULL && backend->enabled != 0);

  return backend->resolve_key(backend, key, backend_key);
}
