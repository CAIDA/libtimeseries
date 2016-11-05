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

#ifndef __TIMESERIES_INT_H
#define __TIMESERIES_INT_H

#include <inttypes.h>

#include "timeseries_pub.h"

/** @file
 *
 * @brief Header file that contains the protected interface to a timeseries
 * object
 *
 * @author Alistair King
 *
 */

/* Provides a way for internal classes to iterate over the set of possible
   enabled backend IDs */
#define TIMESERIES_FOREACH_BACKEND_ID(id)                                      \
  for (id = TIMESERIES_BACKEND_ID_FIRST; id <= TIMESERIES_BACKEND_ID_LAST; id++)

#define TIMESERIES_FOREACH_ENABLED_BACKEND(timeseries, backend, id)            \
  TIMESERIES_FOREACH_BACKEND_ID(id)                                            \
  if ((backend = timeseries_get_backend_by_id(timeseries, id)) == NULL ||      \
      timeseries_backend_is_enabled(backend) == 0)                             \
    continue;                                                                  \
  else

/**
 * @name Protected Datastructures
 *
 * These data structures are available only to libtimeseries classes. Some may
 * be exposed as opaque structures by libtimeseries.h (e.g timeseries_t)
 *
 * @{ */

/** @} */

#endif /* __TIMESERIES_INT_H */
