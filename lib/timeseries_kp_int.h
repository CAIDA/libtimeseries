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


#ifndef __TIMESERIES_KP_INT_H
#define __TIMESERIES_KP_INT_H

#include <inttypes.h>

#include <timeseries_kp_pub.h>

/** @file
 *
 * @brief Header file that contains the private components of libtimeseries.
 *
 * @author Alistair King
 *
 */

/**
 * @name Protected Opaque Data Structures
 *
 * @{ */

/** Opaque struct holding state for a timeseries Key Package Key */
typedef struct timeseries_kp_ki timeseries_kp_ki_t;

/** @} */

/**
 * @name Protected Data Structures
 *
 * @{ */

/** @} */

#define TIMESERIES_KP_FOREACH_KI(kp, ki, id)			\
  int cnt = timeseries_kp_size(kp);				\
  for(id=0, (ki=timeseries_kp_get_ki(kp, id));			\
      id<cnt;							\
      ++id, (ki=timeseries_kp_get_ki(kp, id)))


/** Get the Key Info object with the given ID
 *
 * @param kp            Pointer to the KP to retrieve the Key from
 * @param id            ID of the Key Info object to retrieve
 * @return pointer to the Key Info object with the given ID, NULL if none exists
 */
timeseries_kp_ki_t *timeseries_kp_get_ki(timeseries_kp_t *kp, int id);

/** Get the string key from a Key Info object
 *
 * @param key           pointer to a Key Package Key Info object
 * @return a pointer to the string representation of the Key Info
 */
const char *timeseries_kp_ki_get_key(timeseries_kp_ki_t *ki);

/** Get the value of the given Key Info object
 *
 * @param key           pointer to a Key Package Key Info object
 * @return current value for the given Key Info
 */
uint64_t timeseries_kp_ki_get_value(timeseries_kp_ki_t *ki);

/** Get the backend state of the given Key Info object
 *
 * @param ki            pointer to a Key Package Key Info object
 * @param backend_id    ID of the backend state to retrieve
 * @return double-pointer to the state for this backend/info pair
 */
void **timeseries_kp_ki_get_backend_state(timeseries_kp_ki_t *ki,
					  timeseries_backend_id_t id);


#endif /* __TIMESERIES_KP_INT_H */
