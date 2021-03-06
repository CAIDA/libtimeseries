/*
 * libtimeseries
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "khash.h"
#include "utils.h"

#include "timeseries_kp_int.h"

#include "timeseries_backend_int.h"
#include "timeseries_int.h"
#include "timeseries_log_int.h"

/* ========== PRIVATE DATA STRUCTURES/FUNCTIONS ========== */

KHASH_MAP_INIT_STR(strint, int);

struct timeseries_kp_ki {
  /** Key string */
  char *key;

  /** Value */
  uint64_t value;

  /** Backend-specific state
   * @note index of backend is given by (timeseries_backend_id_t - 1)
   */
  void *backend_state[TIMESERIES_BACKEND_ID_LAST];

  /** Should this KI be skipped? */
  uint8_t disabled;
};

/** Structure which holds state for a Key Package */
struct timeseries_kp {
  /** Timeseries instance that this key package is associated with */
  timeseries_t *timeseries;

  /** Dynamically allocated array of Key Info objects */
  timeseries_kp_ki_t *key_infos;

  /** Hash of key names -> key ids */
  khash_t(strint) * key_id_hash;

  /** Number of keys in the Key Package */
  uint32_t key_infos_cnt;

  /** Number of enabled keys in the Key Package */
  uint32_t key_infos_enabled_cnt;

  /** Per-backend state about this key package
   *
   *  Backends may use this to store any information they require.
   *  At present this is unused.
   */
  void *backend_state[TIMESERIES_BACKEND_ID_LAST];

  /** Should the values be explicitly reset after a flush? */
  int reset;

  /** Should the the keys be disabled after a flush? */
  int disable;

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
 * and/or deactivate all keys if kp.disable is true.
 *
 * @param kp            pointer to a Key Package to reset/disable
 */
static void kp_reset_disable(timeseries_kp_t *kp);

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

static void kp_reset_disable(timeseries_kp_t *kp)
{
  int i;
  if (kp->reset == 0 && kp->disable == 0) {
    return;
  }

  for (i = 0; i < kp->key_infos_cnt; i++) {
    if (kp->reset != 0) {
      kp_ki_set(&kp->key_infos[i], 0);
    }
    if (kp->disable != 0) {
      timeseries_kp_disable_key(kp, i);
    }
  }
}

static int kp_ki_init(timeseries_kp_ki_t *ki, timeseries_kp_t *kp,
                      const char *key)
{
  assert(ki != NULL);

  if ((ki->key = strdup(key)) == NULL) {
    kp_ki_free(ki, kp);
    return -1;
  }

  /* zero out the structure */
  ki->value = 0;
  ki->disabled = 0;
  memset(&ki->backend_state, 0, sizeof(void *) * TIMESERIES_BACKEND_ID_LAST);

  return 0;
}

static void kp_ki_free(timeseries_kp_ki_t *ki, timeseries_kp_t *kp)
{
  timeseries_t *timeseries = kp_get_timeseries(kp);
  assert(timeseries != NULL);
  timeseries_backend_t *backend;
  int id;

  if (ki == NULL) {
    return;
  }

  free(ki->key);
  ki->key = NULL;

  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
  {
    backend->kp_ki_free(backend, kp, ki, ki->backend_state[id - 1]);
    ki->backend_state[id - 1] = NULL;
  }

  return;
}

static void kp_ki_set(timeseries_kp_ki_t *ki, uint64_t value)
{
  ki->value = value;
}

static uint64_t kp_ki_get(timeseries_kp_ki_t *ki)
{
  return ki->value;
}

/* ========== PROTECTED FUNCTIONS ========== */

int timeseries_kp_size(timeseries_kp_t *kp)
{
  assert(kp != NULL);
  return kp->key_infos_cnt;
}

int timeseries_kp_enabled_size(timeseries_kp_t *kp)
{
  return kp->key_infos_enabled_cnt;
}

timeseries_kp_ki_t *timeseries_kp_get_ki(timeseries_kp_t *kp, int id)
{
  assert(kp != NULL);
  if (id >= 0 && id < kp->key_infos_cnt) {
    return &kp->key_infos[id];
  }
  return NULL;
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

int timeseries_kp_ki_enabled(timeseries_kp_ki_t *ki)
{
  return !ki->disabled;
}

void *timeseries_kp_ki_get_backend_state(timeseries_kp_ki_t *ki,
                                         timeseries_backend_id_t id)
{
  assert(ki != NULL);
  return ki->backend_state[id - 1];
}

void timeseries_kp_ki_set_backend_state(timeseries_kp_ki_t *ki,
                                        timeseries_backend_id_t id,
                                        void *ki_state)
{
  assert(ki != NULL);
  ki->backend_state[id - 1] = ki_state;
}

/* ========== PUBLIC FUNCTIONS ========== */

timeseries_kp_t *timeseries_kp_init(timeseries_t *timeseries, int flags)
{
  assert(timeseries != NULL);
  timeseries_kp_t *kp = NULL;
  int id;
  timeseries_backend_t *backend;

  /* we only need to malloc the Package, keys will be malloc'd on the fly */
  if ((kp = malloc_zero(sizeof(timeseries_kp_t))) == NULL) {
    timeseries_log(__func__, "could not malloc key package");
    return NULL;
  }

  /* prep the key hash */
  if ((kp->key_id_hash = kh_init(strint)) == NULL) {
    timeseries_log(__func__, "could not init key hash");
    return NULL;
  }

  /* save the timeseries pointer */
  kp->timeseries = timeseries;

  /* check the flags */
  kp->reset = flags & TIMESERIES_KP_RESET;
  kp->disable = flags & TIMESERIES_KP_DISABLE;

  /* let each backend store some state about this kp, if they like */
  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
  {
    if (backend->kp_init(backend, kp, &kp->backend_state[id - 1]) != 0) {
      return NULL;
    }
  }

  return kp;
}

void timeseries_kp_free(timeseries_kp_t **kp_p)
{
  timeseries_kp_t *kp;
  timeseries_t *timeseries;
  timeseries_backend_t *backend;
  int i, id;

  assert(kp_p != NULL);
  kp = *kp_p;
  if (kp == NULL) {
    return;
  }
  *kp_p = NULL;

  /* destroy the key hash */
  kh_destroy(strint, kp->key_id_hash);

  for (i = 0; i < kp->key_infos_cnt; i++) {
    kp_ki_free(&kp->key_infos[i], kp);
  }

  free(kp->key_infos);
  kp->key_infos = NULL;
  kp->key_infos_cnt = 0;

  timeseries = kp_get_timeseries(kp);
  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
  {
    backend->kp_free(backend, kp, kp->backend_state[id - 1]);
    kp->backend_state[id - 1] = NULL;
  }

  /* free the actual key package structure */
  free(kp);

  return;
}

int timeseries_kp_add_key(timeseries_kp_t *kp, const char *key)
{
  assert(kp != NULL);
  assert(key != NULL);
  int ret;
  khiter_t k;
  int this_id = kp->key_infos_cnt;
  timeseries_kp_ki_t *ki = NULL;

  /* first we need to realloc the array of keys */
  if ((kp->key_infos = realloc(kp->key_infos, sizeof(timeseries_kp_ki_t) *
                                                (this_id + 1))) == NULL) {
    timeseries_log(__func__, "could not realloc KP KI array");
    return -1;
  }

  ki = &kp->key_infos[this_id];
  assert(ki != NULL);

  if (kp_ki_init(ki, kp, key) != 0) {
    return -1;
  }

  /* now add a lookup in the hash */
  k = kh_put(strint, kp->key_id_hash, timeseries_kp_ki_get_key(ki), &ret);
  if (ret == -1) {
    timeseries_log(__func__, "could not add key to hash");
    return -1;
  }
  kh_val(kp->key_id_hash, k) = this_id;

  kp->key_infos_cnt++;
  kp->key_infos_enabled_cnt++;

  /* backends will need to update their state */
  kp->dirty = 1;

  return this_id;
}

int timeseries_kp_get_key(timeseries_kp_t *kp, const char *key)
{
  khiter_t k;
  assert(kp != NULL);

  /* just check the hash */
  if ((k = kh_get(strint, kp->key_id_hash, key)) == kh_end(kp->key_id_hash)) {
    return -1;
  }
  return kh_val(kp->key_id_hash, k);
}

const char *timeseries_kp_get_key_name(timeseries_kp_t *kp, uint32_t key)
{
  if (key >= kp->key_infos_cnt) {
    return NULL;
  }
  return kp->key_infos[key].key;
}

void timeseries_kp_disable_key(timeseries_kp_t *kp, uint32_t key)
{
  if (kp->key_infos[key].disabled == 0) {
    kp->key_infos[key].disabled = 1;
    kp->key_infos_enabled_cnt--;
  }
}

void timeseries_kp_enable_key(timeseries_kp_t *kp, uint32_t key)
{
  if (kp->key_infos[key].disabled != 0) {
    kp->key_infos[key].disabled = 0;
    kp->key_infos_enabled_cnt++;
  }
}

uint64_t timeseries_kp_get(timeseries_kp_t *kp, uint32_t key)
{
  return kp_ki_get(&kp->key_infos[key]);
}

void timeseries_kp_set(timeseries_kp_t *kp, uint32_t key, uint64_t value)
{
  assert(kp != NULL);
  assert(key < kp->key_infos_cnt);

  kp_ki_set(&kp->key_infos[key], value);
}

int timeseries_kp_resolve(timeseries_kp_t *kp)
{
  int id;
  timeseries_backend_t *backend;
  timeseries_t *timeseries = kp_get_timeseries(kp);
  assert(timeseries != NULL);

  /* this is a forced resolve, so we always resolve, but we mark the KP as not
     dirty */
  kp->dirty = 0;

  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
  {
    if (backend->kp_ki_update(backend, kp) != 0) {
      return -1;
    }
  }

  return 0;
}

int timeseries_kp_flush(timeseries_kp_t *kp, uint32_t time)
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
    if (dirty != 0 && backend->kp_ki_update(backend, kp) != 0) {
      kp->dirty = 1; /* otherwise the next call won't resolve keys */
      return -1;
    }

    if (backend->kp_flush(backend, kp, time) != 0) {
      return -1;
    }
  }

  kp_reset_disable(kp);
  return 0;
}
