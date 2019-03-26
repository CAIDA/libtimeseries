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

#ifndef __TIMESERIES_KP_INT_H
#define __TIMESERIES_KP_INT_H

#include <inttypes.h>

#include "timeseries_kp_pub.h"

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

#define TIMESERIES_KP_FOREACH_KI(kp, ki, id)                                   \
  int cnt = timeseries_kp_size(kp);                                            \
  for (id = 0, (ki = timeseries_kp_get_ki(kp, id)); id < cnt;                  \
       id++, (ki = timeseries_kp_get_ki(kp, id)))

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

/** Is this KI enabled?
 *
 * @param ki            pointer to a Key Package Key Info object
 * @return 1 if the KI is enabled (should be dumped), 0 otherwise
 */
int timeseries_kp_ki_enabled(timeseries_kp_ki_t *ki);

/** Get the backend state of the given Key Info object
 *
 * @param ki            pointer to a Key Package Key Info object
 * @param backend_id    ID of the backend state to retrieve
 * @return pointer to the state for this backend/info pair
 */
void *timeseries_kp_ki_get_backend_state(timeseries_kp_ki_t *ki,
                                         timeseries_backend_id_t id);

/** Set the backend state of the given Key Info object
 *
 * @param ki            pointer to a Key Package Key Info object
 * @param backend_id    ID of the backend state to store
 * @param ki_state      pointer to the state to store in the KI object
 */
void timeseries_kp_ki_set_backend_state(timeseries_kp_ki_t *ki,
                                        timeseries_backend_id_t id,
                                        void *ki_state);

#endif /* __TIMESERIES_KP_INT_H */
