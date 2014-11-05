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


#ifndef __TIMESERIES_BACKEND_PUB_H
#define __TIMESERIES_BACKEND_PUB_H

#include <stdlib.h>
#include <stdint.h>


/** @file
 *
 * @brief Header file that exposes the public interface of a timeseries backend
 * object
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque struct holding state for a timeseries backend */
typedef struct timeseries_backend timeseries_backend_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */


/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** A unique identifier for each timeseries backend that libtimeseries supports
 */
typedef enum timeseries_backend_id
  {
    /** Writes timeseries metrics out in ASCII format (either to stdout or to
	file) */
    TIMESERIES_BACKEND_ID_ASCII      = 1,

    /** Write timeseries metrics into a DBATS database */
    TIMESERIES_BACKEND_ID_DBATS      = 2,

    /** Write timeseries metrics to a remote libtimeseries database */
    TIMESERIES_BACKEND_ID_TSMQ       = 3,

    /** Lowest numbered timeseries backend ID */
    TIMESERIES_BACKEND_ID_FIRST      = TIMESERIES_BACKEND_ID_ASCII,
    /** Highest numbered timeseries backend ID */
    TIMESERIES_BACKEND_ID_LAST       = TIMESERIES_BACKEND_ID_TSMQ,

  } timeseries_backend_id_t;

/** @} */

/** Check if the given backend is enabled already
 *
 * @param backend       The backend to check the status of
 * @return 1 if the backend is enabled, 0 otherwise
 */
int timeseries_backend_is_enabled(timeseries_backend_t *backend);

/** Get the ID for the given backend
 *
 * @param backend      The backend object to retrieve the ID from
 * @return the ID of the given backend
 */
timeseries_backend_id_t timeseries_backend_get_id(timeseries_backend_t *backend);

/** Get the backend name for the given ID
 *
 * @param id            The backend ID to retrieve the name for
 * @return the name of the backend, NULL if an invalid backend was provided
 */
const char *timeseries_backend_get_name(timeseries_backend_t *backend);

#endif /* __TIMESERIES_BACKEND_PUB_H */
