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

#include "utils.h"

#include "timeseries_kp_int.h"

#include "timeseries_int.h"
#include "timeseries_backend_int.h"
#include "timeseries_log_int.h"

/* ========== PRIVATE DATA STRUCTURES/FUNCTIONS ========== */

struct timeseries_kp_ki
{
  /** Key string */
  char *key;

  /** Value */
  uint64_t value;

  /** Backend-specific state
   * @note index of backend is given by (timeseries_backend_id_t - 1)
   */
  void *backend_state[TIMESERIES_BACKEND_ID_LAST];

};

/** Structure which holds state for a Key Package */
struct timeseries_kp
{
  /** Timeseries instance that this key package is associated with */
  timeseries_t *timeseries;

  /** Dynamically allocated array of Key Info objects */
  timeseries_kp_ki_t *key_infos;

  /** Number of keys in the Key Package */
  uint32_t key_infos_cnt;

  /** Per-backend state about this key package
   *
   *  Backends may use this to store any information they require.
   *  At present this is unused.
   */
  void *backend_state[TIMESERIES_BACKEND_ID_LAST];

  /** Should the values be explicitly reset after a flush? */
  int reset;

  /** Have keys been added since the last call to [backend]->kp_ki_update? */
  int dirty;
};

/** Get the timeseries object associated with the given Key Package
 *
 * @param kp            pointer to a Key Package
 * @return pointer to a timeseries object
 */
static timeseries_t *kp_get_timeseries(timeseries_kp_t *kp);

/** Reset all the values in the given Key Package to 0 (if kp.reset is true)
 *
 * @param kp            pointer to a Key Package to reset
 */
static void kp_reset(timeseries_kp_t *kp);

/** Initialize the given Key Info object
 *
 * @param ki            Pointer to the Key Info object to initialize
 * @param kp            Pointer to the Key Package this KI is associated with
 * @param key           Pointer to a key string
 * @return 0 if the KI was initialized successfully, -1 otherwise
 */
static int kp_ki_init(timeseries_kp_ki_t *ki, timeseries_kp_t *kp,
		      const char *key);

/** Free the given Key Info object
 *
 * @param ki_p          Pointer to a KI object to free
 * @param kp            Pointer to the KP object this KI is associated with
 *
 * @note does NOT free the memory for the actual KI structure
 */
static void kp_ki_free(timeseries_kp_ki_t *ki, timeseries_kp_t *kp);

/** Set the value of the given KI to the given value
 *
 * @param ki            Pointer to the KI object to set the value of
 * @param value         Value to set
 */
static void kp_ki_set(timeseries_kp_ki_t *ki, uint64_t value);


static timeseries_t *kp_get_timeseries(timeseries_kp_t *kp)
{
  assert(kp != NULL);
  return kp->timeseries;
}

static void kp_reset(timeseries_kp_t *kp)
{
  int i;
  if(kp->reset == 0)
    {
      return;
    }

  for(i=0; i<kp->key_infos_cnt; i++)
    {
      kp_ki_set(&kp->key_infos[i], 0);
    }
}

static int kp_ki_init(timeseries_kp_ki_t *ki, timeseries_kp_t *kp,
		      const char *key)
{
  assert(ki != NULL);

  if((ki->key = strdup(key)) == NULL)
    {
      kp_ki_free(ki, kp);
      return -1;
    }

  return 0;
}

static void kp_ki_free(timeseries_kp_ki_t *ki, timeseries_kp_t *kp)
{
  timeseries_t *timeseries = kp_get_timeseries(kp);
  assert(timeseries != NULL);
  timeseries_backend_t *backend;
  int id;

  if(ki == NULL)
    {
      return;
    }

  free(ki->key);
  ki->key = NULL;

  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
    {
      backend->kp_ki_free(backend, kp, ki, ki->backend_state[id-1]);
      ki->backend_state[id-1] = NULL;
    }

  free(ki);

  return;
}

static void kp_ki_set(timeseries_kp_ki_t *ki, uint64_t value)
{
  assert(ki != NULL);
  ki->value = value;
}


/* ========== PROTECTED FUNCTIONS ========== */

int timeseries_kp_size(timeseries_kp_t *kp)
{
  assert(kp != NULL);
  return kp->key_infos_cnt;
}

timeseries_kp_ki_t *timeseries_kp_get_ki(timeseries_kp_t *kp, int id)
{
  assert(kp != NULL);
  assert(id >= 0 && id < kp->key_infos_cnt);
  return &kp->key_infos[id];
}

const char *timeseries_kp_ki_get_key(timeseries_kp_ki_t *ki)
{
  assert(ki != NULL);
  return ki->key;
}

uint64_t timeseries_kp_ki_get_value(timeseries_kp_ki_t *ki)
{
  assert(ki != NULL);
  return ki->value;
}

void **timeseries_kp_ki_get_backend_state(timeseries_kp_ki_t *ki,
					  timeseries_backend_id_t id)
{
  assert(ki != NULL);
  return ki->backend_state[id-1];
}

/* ========== PUBLIC FUNCTIONS ========== */

timeseries_kp_t *timeseries_kp_init(timeseries_t *timeseries, int reset)
{
  assert(timeseries != NULL);
  timeseries_kp_t *kp = NULL;
  int id;
  timeseries_backend_t *backend;

  /* we only need to malloc the Package, keys will be malloc'd on the fly */
  if((kp = malloc_zero(sizeof(timeseries_kp_t))) == NULL)
    {
      timeseries_log(__func__, "could not malloc key package");
      return NULL;
    }

  /* save the timeseries pointer */
  kp->timeseries = timeseries;

  /* set the reset flag */
  kp->reset = reset;

  /* let each backend store some state about this kp, if they like */
  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
  {
    if(backend->kp_init(backend, kp, &kp->backend_state[id-1]) != 0)
      {
	return NULL;
      }
  }

  return kp;
}

void timeseries_kp_free(timeseries_kp_t **kp_p)
{
  int i;
  assert(kp_p != NULL);
  timeseries_kp_t *kp = *kp_p;
  *kp_p = NULL;

  timeseries_t *timeseries = kp_get_timeseries(kp);
  timeseries_backend_t *backend;
  int id;

  if(kp != NULL)
    {
      for(i=0; i < kp->key_infos_cnt; i++)
	{
	  kp_ki_free(&kp->key_infos[i], kp);
	}

      free(kp->key_infos);
      kp->key_infos = NULL;
      kp->key_infos_cnt = 0;

      TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
	{
	  backend->kp_free(backend, kp, kp->backend_state[id-1]);
	  kp->backend_state[id-1] = NULL;
	}

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
  if((kp->key_infos =
      realloc(kp->key_infos,
              sizeof(timeseries_kp_ki_t) * (kp->key_infos_cnt+1))) == NULL)
    {
      timeseries_log(__func__, "could not realloc KP KI array");
      return -1;
    }

  if(kp_ki_init(&kp->key_infos[kp->key_infos_cnt], kp, key) != 0)
    {
      return -1;
    }

  kp->key_infos_cnt++;

  /* backends will need to update their state */
  kp->dirty = 1;

  return 0;
}

void timeseries_kp_set(timeseries_kp_t *kp, uint32_t key, uint64_t value)
{
  assert(kp != NULL);
  assert(key < kp->key_infos_cnt);

  kp_ki_set(&kp->key_infos[key], value);
}

int timeseries_kp_flush(timeseries_kp_t *kp,
			uint32_t time)
{
  int id;
  timeseries_backend_t *backend;
  int dirty;
  timeseries_t *timeseries = kp_get_timeseries(kp);
  assert(timeseries != NULL);

  dirty = kp->dirty;
  kp->dirty = 0;

  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
    {
      if(dirty != 0 && backend->kp_ki_update(backend, kp) != 0)
	{
	  return -1;
	}

      if(backend->kp_flush(backend, kp, time) != 0)
	{
	  return -1;
	}
    }

  kp_reset(kp);
  return 0;
}
