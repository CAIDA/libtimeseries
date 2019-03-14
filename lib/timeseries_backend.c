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

#include "timeseries_backend_int.h" /* timeseries_backend_t */
#include "timeseries_log_int.h"     /* timeseries_log */
#include "timeseries_pub.h"         /* timeseries_t */

/* now include the backends */

/* ascii */
#include "timeseries_backend_ascii.h"

/* DBATS */
#ifdef WITH_DBATS
#include "timeseries_backend_dbats.h"
#endif

/* Kafka */
#ifdef WITH_KAFKA
#include "timeseries_backend_kafka.h"
#endif

/* ========== PRIVATE DATA STRUCTURES/FUNCTIONS ========== */

/** Convenience typedef for the backend alloc function type */
typedef timeseries_backend_t *(*backend_alloc_func_t)();

/** Array of backend allocation functions.
 *
 * @note the indexes of these functions must exactly match the ID in
 * timeseries_backend_id_t. The element at index 0 MUST be NULL.
 */
static const backend_alloc_func_t backend_alloc_functions[] = {
  /** Pointer to ASCII backend alloc function */
  timeseries_backend_ascii_alloc,

/** If we are building with DBATS support, point to the dbats alloc function,
    otherwise a NULL pointer to indicate the backend is unavailable */
#ifdef WITH_DBATS
  timeseries_backend_dbats_alloc,
#else
  NULL,
#endif

/** If we are building with Kafka support, point to the kafka alloc function,
    otherwise a NULL pointer to indicate the backend is unavailable */
#ifdef WITH_KAFKA
  timeseries_backend_kafka_alloc,
#else
  NULL,
#endif

};

/* ========== PROTECTED FUNCTIONS ========== */

timeseries_backend_t *timeseries_backend_alloc(timeseries_backend_id_t id)
{
  timeseries_backend_t *backend;
  assert(ARR_CNT(backend_alloc_functions) == TIMESERIES_BACKEND_ID_LAST);

  if (backend_alloc_functions[id - 1] == NULL) {
    return NULL;
  }

  /* first, create the struct */
  if ((backend = malloc_zero(sizeof(timeseries_backend_t))) == NULL) {
    timeseries_log(__func__, "could not malloc timeseries_backend_t");
    return NULL;
  }

  /* get the core backend details (id, name) from the backend plugin */
  memcpy(backend, backend_alloc_functions[id - 1](),
         sizeof(timeseries_backend_t));

  return backend;
}

int timeseries_backend_init(timeseries_backend_t *backend, int argc,
                            char **argv)
{
  assert(backend != NULL);

  /* if it has already been initialized, then we simply return */
  if (backend->enabled != 0) {
    timeseries_log(__func__, "WARNING: backend (%s) is already initialized, "
                             "ignoring new settings",
                   backend->name);
    return 0;
  }

  /* otherwise, we need to init this plugin */

  /* ask the backend to initialize. this will normally mean that it connects to
     some database and prepares state */
  if (backend->init(backend, argc, argv) != 0) {
    return -1;
  }

  backend->enabled = 1;

  return 0;
}

void timeseries_backend_free(timeseries_backend_t **backend_p)
{
  assert(backend_p != NULL);
  timeseries_backend_t *backend = *backend_p;
  *backend_p = NULL;

  if (backend == NULL) {
    return;
  }

  /* only free everything if we were enabled */
  if (backend->enabled != 0) {
    /* ask the backend to free it's own state */
    backend->free(backend);
  }

  /* finally, free the actual backend structure */
  free(backend);

  return;
}

void timeseries_backend_register_state(timeseries_backend_t *backend,
                                       void *state)
{
  assert(backend != NULL);
  assert(state != NULL);

  backend->state = state;
}

void timeseries_backend_free_state(timeseries_backend_t *backend)
{
  assert(backend != NULL);

  free(backend->state);
  backend->state = NULL;
}

/* ========== PUBLIC FUNCTIONS ========== */

inline int timeseries_backend_is_enabled(timeseries_backend_t *backend)
{
  assert(backend != NULL);
  return backend->enabled;
}

inline timeseries_backend_id_t
timeseries_backend_get_id(timeseries_backend_t *backend)
{
  assert(backend != NULL);

  return backend->id;
}

inline const char *timeseries_backend_get_name(timeseries_backend_t *backend)
{
  assert(backend != NULL);

  return backend->name;
}
