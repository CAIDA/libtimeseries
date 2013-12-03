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


#ifndef __LIBTIMESERIES_H
#define __LIBTIMESERIES_H

#include <stdint.h>

/** @file
 *
 * @brief Header file that exposes the public interface of libtimeseries.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque struct holding timeseries state */
typedef struct timeseries timeseries_t;

/** Opaque struct holding state for a timeseries backend */
typedef struct timeseries_backend timeseries_backend_t;

/** Opaque struct holding state for a timeseries key package */
typedef struct timeseries_kp timeseries_kp_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/* none right now */

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
    TIMESERIES_BACKEND_ASCII      =  1,

    /** Write timeseries metrics into a DBATS database */
    TIMESERIES_BACKEND_DBATS  =  2,

    /** Highest numbered timeseries backend ID */
    TIMESERIES_BACKEND_MAX          = TIMESERIES_BACKEND_DBATS,

  } timeseries_backend_id_t;

/** @} */

/** Initialize a new libtimeseries instance
 *
 * @param backend_id    ID of the timeseries backend to use
 * @param options       String of options used to configure the backend
 *
 * @return the timeseries instance created, NULL if an error occurs
 */
timeseries_t *timeseries_init();

/** Free a libtimeseries instance
 *
 * @param               The timeseries instance to free
 */
void timeseries_free(timeseries_t *timeseries);

/** Enable the given backend unless it is already enabled
 *
 * @param timeseries    The timeseries object to enable the backend for
 * @param backend       The backend to be enabled
 * @param options       A string of options to configure the backend
 * @return 0 if the backend was initialized, -1 if an error occurred
 *
 * Once timeseries_init is called, timeseries_enable_backend should be called
 * once for each backend that is to be used.
 *
 * To obtain a pointer to a backend, use the timeseries_get_backend_by_name or
 * timeseries_get_backend_by_id functions. To enumerate a list of available
 * backends, the timeseries_get_all_backends function can be used to get a list of
 * all backends and then timeseries_get_backend_name can be used on each to get
 * their name.
 */
int timeseries_enable_backend(timeseries_t *timeseries,
			      timeseries_backend_t *backend,
			      const char *options);

/** Retrieve the backend object for the given backend ID
 *
 * @param timeseries    The timeseries object to retrieve the backend object from
 * @param id            The backend ID to retrieve
 * @return the backend object for the given ID, NULL if there are no matches
 */
timeseries_backend_t *timeseries_get_backend_by_id(timeseries_t *timeseries,
						   timeseries_backend_id_t id);


/** Retrieve the backend id for the given backend name
 *
 * @param timeseries    Timeseries object to retrieve the backend from
 * @param name          The backend name to retrieve
 * @return the backend id for the given name, 0 if there are no matches
 */
timeseries_backend_t *timeseries_get_backend_by_name(timeseries_t *timeseries,
						     const char *name);

/** Check if the given backend is enabled already
 *
 * @param backend       The backend to check the status of
 * @return 1 if the backend is enabled, 0 otherwise
 */
int timeseries_is_backend_enabled(timeseries_backend_t *backend);

/** Get the ID for the given backend
 *
 * @param backend      The backend object to retrieve the ID from
 * @return the ID of the given backend
 */
timeseries_backend_id_t timeseries_get_backend_id(timeseries_backend_t *backend);

/** Get the backend name for the given ID
 *
 * @param id            The backend ID to retrieve the name for
 * @return the name of the backend, NULL if an invalid backend was provided
 */
const char *timeseries_get_backend_name(timeseries_backend_t *backend);

/** Get an array of available backends
 *
 * @param timeseries    The timeseries object to get all the backends for
 * @return an array of backend objects
 *
 * @note the number of elements in the array will be exactly
 * TIMESERIES_BACKEND_MAX.
 * @note not all backends in the list may be enabled. use
 * timeseries_is_backend_enabled to check.
 */
timeseries_backend_t **timeseries_get_all_backends(timeseries_t *timeseries);

/** Initialize a Key Package
 *
 * @param reset         Should the values be reset upon a flush.
 * @return a pointer to a corsaro dbats key package structure, NULL if an error
 *         occurs
 *
 * DBATS supports highly-efficient writes if the key names are known a priori,
 * and all values are inserted simultaneously. A Key Package is an easy way to
 * define a set of keys that will be updated in concert.
 *
 * Given that Corsaro operates in intervals, the most common use case will be
 * to establish a Key Package during initialization, and then either set
 * values on a per-packet basis, or, more likely, set values for all keys at
 * the end of an interval, after which the kp_flush function can
 * be used to write the key package out.
 *
 * You may safely assume that values for all keys will be initialized to 0.
 * If you know you will set a value for every key every interval then setting
 * the _reset_ parameter to 0 will improve performance slightly.
 *
 * @note If not all key names are known during initialization, then the
 * corsaro_dbats_insert function can be used to insert the value for a single
 * arbitrary key.
 */
timeseries_kp_t *timeseries_kp_init(int reset);

/** Free a Key Package
 *
 * @param kp          Pointer to the Key Package to free
 */
void timeseries_kp_free(timeseries_kp_t *kp);

/** Add a key to an existing Key Package
 *
 * @param kp          The Key Package to add the key to
 * @param key         String containing the name of the key to add
 * @return the index of the key that was added, -1 if an error occurred
 */
int timeseries_kp_add_key(timeseries_kp_t *kp, const char *key);

/** Set the current value for the given key in a Key Package
 *
 * @param kp            Pointer to the KP to set the value on
 * @param key           Index of the key (as returned by kp_add_key) to
 *                      set the value for
 * @param value         Value to set the key to
 */
void timeseries_kp_set(timeseries_kp_t *kp, uint32_t key, uint64_t value);

/** Flush the current values in the given Key Package to the database
 *
 * @param backend       Pointer to the backend to flush values to
 * @param kp            Pointer to the KP to flush values for
 * @param time          The timestamp to associate the values with in the DB
 * @return 0 if the data was written successfully, -1 otherwise.
 */
int timeseries_kp_flush(timeseries_backend_t *backend,
			timeseries_kp_t *kp, uint32_t time);

/** Write the value for a single key to the DBATS database
 *
 * @param backend       Pointer to the backend to write value to
 * @param key           String key name
 * @param value         Value to set the key to
 * @param time          The time slot to set the key's value for
 *
 * @warning this function will perform much worse than using the Key Package
 * functions above, use with caution
 */
int timeseries_set_single(timeseries_backend_t *backend, const char *key,
			  uint64_t value, uint32_t time);

/**
 * @name Logging functions
 *
 * Collection of convenience functions that allow libtimeseries to log events
 * For now we just log to stderr, but this should be extended in future.
 *
 * @todo find (or write) good C logging library (that can also log to syslog)
 *
 * @{ */

void timeseries_log(const char *func, const char *format, ...);

/** @} */

#endif /* __LIBTIMESERIES_H */
