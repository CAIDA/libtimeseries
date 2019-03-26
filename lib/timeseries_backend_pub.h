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

#ifndef __TIMESERIES_BACKEND_PUB_H
#define __TIMESERIES_BACKEND_PUB_H

#include <stdint.h>
#include <stdlib.h>

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
typedef enum timeseries_backend_id {
  /** Writes timeseries metrics out in ASCII format (either to stdout or to
      file) */
  TIMESERIES_BACKEND_ID_ASCII = 1,

  /** Write timeseries metrics into a DBATS database */
  TIMESERIES_BACKEND_ID_DBATS = 2,

  /** Write timeseries metrics to an Apache Kafka cluster */
  TIMESERIES_BACKEND_ID_KAFKA = 3,

  /** Lowest numbered timeseries backend ID */
  TIMESERIES_BACKEND_ID_FIRST = TIMESERIES_BACKEND_ID_ASCII,
  /** Highest numbered timeseries backend ID */
  TIMESERIES_BACKEND_ID_LAST = TIMESERIES_BACKEND_ID_KAFKA,

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
timeseries_backend_id_t
timeseries_backend_get_id(timeseries_backend_t *backend);

/** Get the backend name for the given ID
 *
 * @param id            The backend ID to retrieve the name for
 * @return the name of the backend, NULL if an invalid backend was provided
 */
const char *timeseries_backend_get_name(timeseries_backend_t *backend);

#endif /* __TIMESERIES_BACKEND_PUB_H */
