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


#ifndef __LIBTIMESERIES_INT_H
#define __LIBTIMESERIES_INT_H

#include <inttypes.h>

#include "libtimeseries.h"

/** @file
 *
 * @brief Header file that contains the private components of libtimeseries.
 *
 * @author Alistair King
 *
 */

/**
 * @name Internal Datastructures
 *
 * These datastructures are internal to libtimeseries. Some may be exposed as
 * opaque structures by libtimeseries.h (e.g timeseries_t)
 *
 * @{ */

/** Structure which holds state for a libtimeseries instance */
struct timeseries
{

  /** Array of backends
   * @note index of backend is given by (timeseries_backend_id_t - 1)
   */
  struct timeseries_backend *backends[TIMESERIES_BACKEND_ID_LAST];

};

typedef struct timeseries_kp_kv
{
  /** Key string */
  char *key;

  /** Value */
  uint64_t value;

} timeseries_kp_kv_t;

/** Structure which holds state for a Key Package */
struct timeseries_kp
{
  /** Timeseries instance that this key package is associated with */
  timeseries_t *timeseries;

  /** Dynamically allocated array of Key/Value pairs */
  timeseries_kp_kv_t *kvs;

  /** Number of keys in the Key Package */
  uint32_t kvs_cnt;

  /** Flag marking the key package as dirty (i.e. a key has been added since the
      last flush) */
  int dirty;

  /** Per-backend state about this key package
   *
   *  Backends may use this to store any information they require, but most
   *  commonly it will be used to store an array of backend-specific id's that
   *  correspond to the keys in the key package
   */
  void *backend_state[TIMESERIES_BACKEND_ID_LAST];

  /** Should the values be explicitly reset after a flush? */
  int reset;
};

/** @} */

#endif /* __LIBTIMESERIES_INT_H */
