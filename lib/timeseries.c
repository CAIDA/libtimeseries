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
#include "parse_cmd.h"

#include "timeseries_backend_int.h" /* timeseries_backend_t */
#include "timeseries_int.h"         /* timeseries_t */
#include "timeseries_log_int.h"     /* timeseries_log */

/* ========== PRIVATE DATA STRUCTURES/FUNCTIONS ========== */

#define MAXOPTS 1024

#define SEPARATOR "|"

/** Structure which holds state for a libtimeseries instance */
struct timeseries {

  /** Array of backends
   * @note index of backend is given by (timeseries_backend_id_t - 1)
   */
  struct timeseries_backend *backends[TIMESERIES_BACKEND_ID_LAST];
};

/* ========== PROTECTED FUNCTIONS ========== */

/* ========== PUBLIC FUNCTIONS ========== */

timeseries_t *timeseries_init()
{
  timeseries_t *timeseries;
  int id;

  timeseries_log(__func__, "initializing libtimeseries");

  /* allocate some memory for our state */
  if ((timeseries = malloc_zero(sizeof(timeseries_t))) == NULL) {
    timeseries_log(__func__, "could not malloc timeseries_t");
    return NULL;
  }

  /* allocate the backends (some may/will be NULL) */
  TIMESERIES_FOREACH_BACKEND_ID(id)
  {
    timeseries->backends[id - 1] = timeseries_backend_alloc(id);
  }

  return timeseries;
}

void timeseries_free(timeseries_t **timeseries_p)
{
  assert(timeseries_p != NULL);
  timeseries_t *timeseries = *timeseries_p;
  *timeseries_p = NULL;
  int id;

  /* loop across all backends and free each one */
  TIMESERIES_FOREACH_BACKEND_ID(id)
  {
    timeseries_backend_free(&timeseries->backends[id - 1]);
  }

  free(timeseries);
  return;
}

int timeseries_enable_backend(timeseries_backend_t *backend,
                              const char *options)
{
  char *local_args = NULL;
  char *process_argv[MAXOPTS];
  int len;
  int process_argc = 0;
  int rc;

  timeseries_log(__func__, "enabling backend (%s)", backend->name);

  /* first we need to parse the options */
  if (options != NULL && (len = strlen(options)) > 0) {
    local_args = strndup(options, len);
    parse_cmd(local_args, &process_argc, process_argv, MAXOPTS, backend->name);
  } else {
    process_argv[process_argc++] = (char *)backend->name;
  }

  /* we just need to pass this along to the backend framework */
  rc = timeseries_backend_init(backend, process_argc, process_argv);

  if (local_args != NULL) {
    free(local_args);
  }

  return rc;
}

inline timeseries_backend_t *
timeseries_get_backend_by_id(timeseries_t *timeseries,
                             timeseries_backend_id_t id)
{
  assert(timeseries != NULL);
  if (id < TIMESERIES_BACKEND_ID_FIRST || id > TIMESERIES_BACKEND_ID_LAST) {
    return NULL;
  }
  return timeseries->backends[id - 1];
}

timeseries_backend_t *timeseries_get_backend_by_name(timeseries_t *timeseries,
                                                     const char *name)
{
  timeseries_backend_t *backend;
  int id;

  TIMESERIES_FOREACH_BACKEND_ID(id)
  {
    if ((backend = timeseries_get_backend_by_id(timeseries, id)) != NULL &&
        strncasecmp(backend->name, name, strlen(backend->name)) == 0) {
      return backend;
    }
  }

  return NULL;
}

timeseries_backend_t **timeseries_get_all_backends(timeseries_t *timeseries)
{
  return timeseries->backends;
}

int timeseries_set_single(timeseries_t *timeseries, const char *key,
                          uint64_t value, uint32_t time)
{
  int id;
  timeseries_backend_t *backend;
  assert(timeseries != NULL);

  TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)
  {
    if (backend->set_single(backend, key, value, time) != 0) {
      return -1;
    }
  }

  return 0;
}
