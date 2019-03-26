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

#ifndef __TIMESERIES_PUB_H
#define __TIMESERIES_PUB_H

#include <stdint.h>
#include <stdlib.h>

#include "timeseries_backend_pub.h"

/** @file
 *
 * @brief Header file that exposes the public interface of a timeseries object
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
 * @param               Double-pointer to timeseries instance to free
 */
void timeseries_free(timeseries_t **timeseries);

/** Enable the given backend unless it is already enabled
 *
 * @param backend       Pointer to the backend to be enabled
 * @param options       A string of options to configure the backend
 * @return 0 if the backend was initialized, -1 if an error occurred
 *
 * Once timeseries_init is called, timeseries_enable_backend should be called
 * once for each backend that is to be used.
 *
 * To obtain a pointer to a backend, use the timeseries_get_backend_by_name or
 * timeseries_get_backend_by_id functions. To enumerate a list of available
 * backends, the timeseries_get_all_backends function can be used to get a list
 * of
 * all backends and then timeseries_backend_get_name can be used on each to get
 * their name.
 */
int timeseries_enable_backend(timeseries_backend_t *backend,
                              const char *options);

/** Retrieve the backend object for the given backend ID
 *
 * @param timeseries    The timeseries object to retrieve the backend object
 * from
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

/** Get an array of available backends
 *
 * @param timeseries    The timeseries object to get all the backends for
 * @return an array of backend objects
 *
 * @note the number of elements in the array will be exactly
 * TIMESERIES_BACKEND_ID_LAST.
 *
 * @note not all backends in the list may be present * (i.e. there may be null
 * pointers), or some may not be enabled. use * timeseries_is_backend_enabled to
 * check.
 */
timeseries_backend_t **timeseries_get_all_backends(timeseries_t *timeseries);

/** Write the value for a single key to all enabled backends
 *
 * @param timeseries    Pointer to the timeseries object to write to
 * @param key           String key name
 * @param value         Value to set the key to
 * @param time          The time slot to set the key's value for
 *
 * @warning this function will perform much worse than using a Key Package.
 * Use with caution
 */
int timeseries_set_single(timeseries_t *timeseries, const char *key,
                          uint64_t value, uint32_t time);

#endif /* __TIMESERIES_PUB_H */
