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

#include "khash.h"

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
  struct timeseries_backend *backends[TIMESERIES_BACKEND_MAX];

};

/** @} */

#endif /* __LIBTIMESERIES_INT_H */
