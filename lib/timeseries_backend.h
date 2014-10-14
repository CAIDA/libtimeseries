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


#ifndef __TIMESERIES_BACKEND_H
#define __TIMESERIES_BACKEND_H

#include <inttypes.h>

#include "libtimeseries.h"

/** @file
 *
 * @brief Header file that exposes the timeseries backend API
 *
 * @author Alistair King
 *
 */

/** Convenience macro to allow backend implementations to retrieve their state
 *  object
 */
#define TIMESERIES_BACKEND_STATE(type, backend) \
  ((timeseries_backend_##type##_state_t*)(backend)->state)

/** Convenience macro that defines all the function prototypes for the timeseries
 * backend API
 */
#define TIMESERIES_BACKEND_GENERATE_PROTOS(provname)			\
  timeseries_backend_t * timeseries_backend_##provname##_alloc();	\
  int timeseries_backend_##provname##_init(timeseries_backend_t *ds,	\
					   int argc, char **argv);	\
  void timeseries_backend_##provname##_free(timeseries_backend_t *ds);	\
  int timeseries_backend_##provname##_kp_init(timeseries_backend_t *backend, \
					      timeseries_kp_t *kp,	\
					      void **state);		\
  void timeseries_backend_##provname##_kp_free(timeseries_backend_t *backend, \
					       timeseries_kp_t *kp,	\
					       void *state);		\
  int timeseries_backend_##provname##_kp_flush(timeseries_backend_t *backend, \
					       timeseries_kp_t *kp,	\
					       uint32_t time);		\
  int timeseries_backend_##provname##_set_single(timeseries_backend_t *backend,	\
						 const char *key,	\
						 uint64_t value,	\
						 uint32_t time);

/** Convenience macro that defines all the function pointers for the timeseries
 * backend API
 */
#define TIMESERIES_BACKEND_GENERATE_PTRS(provname)	\
  timeseries_backend_##provname##_init,			\
    timeseries_backend_##provname##_free,		\
    timeseries_backend_##provname##_kp_init,		\
    timeseries_backend_##provname##_kp_free,		\
    timeseries_backend_##provname##_kp_flush,		\
    timeseries_backend_##provname##_set_single,		\
    0, NULL

/** Structure which represents a metadata backend */
struct timeseries_backend
{
  /**
   * @name Backend information fields
   *
   * These fields are always filled, even if a backend is not enabled.
   *
   * @{ */

  /** The ID of the backend */
  timeseries_backend_id_t id;

  /** The name of the backend */
  const char *name;

  /** }@ */

  /**
   * @name Backend function pointers
   *
   * These pointers are always filled, even if a backend is not enabled.
   * Until the backend is enabled, only the init function can be called.
   *
   * @{ */

  /** Initialize and enable this backend
   *
   * @param backend     The backend object to allocate
   * @param argc        The number of tokens in argv
   * @param argv        An array of strings parsed from the command line
   * @return 0 if the backend is successfully initialized, -1 otherwise
   *
   * @note the most common reason for returning -1 will likely be incorrect
   * command line arguments.
   *
   * @warning the strings contained in argv will be free'd once this function
   * returns. Ensure you make appropriate copies as needed.
   */
  int (*init)(struct timeseries_backend *backend, int argc, char ** argv);

  /** Shutdown and free backend-specific state for this backend
   *
   * @param backend    The backend object to free
   *
   * @note backends should *only* free backend-specific state. All other state
   * will be free'd for them by the backend manager.
   */
  void (*free)(struct timeseries_backend *backend);

  /** Allocate any backend-specific state in the given Key Package
   *
   * @param backend     Pointer to a backend instance
   * @param kp          Pointer to the KP to free state for
   * @param[out] state  Pointer to the state allocated, or NULL if no state is
   *                    needed by the backend
   */
  int (*kp_init)(timeseries_backend_t *backend,
		 timeseries_kp_t *kp,
		 void **state);

  /** Free the backend-specific state in the given Key Package
   *
   * @param backend     Pointer to a backend instance
   * @param kp          Pointer to the KP to free state for
   * @param state       Pointer to the state to free
   */
  void (*kp_free)(timeseries_backend_t *backend,
		  timeseries_kp_t *kp,
		  void *state);

  /** Update the backend-specific state in the given Key Package
   *
   * @param backend     Pointer to a backend instance
   * @param kp          Pointer to the KP to free state for
   * @param state       Pointer to the state to update
   *
   * @note for example, the DBATS backend needs to ask DBATS what the internal
   * key id is for each string key. The tsmq backend needs to send a query over
   * the network to find the server and key ids for each key. This is only done
   * once per flush, and only when a key has been added to the Key Package since
   * the last flush
   */
  void (*kp_update)(timeseries_backend_t *backend,
		    timeseries_kp_t *kp,
		    void *state);

  /** Flush the current values in the given Key Package to the database
   *
   * @param backend       Pointer to a backend instance to flush to
   * @param kp            Pointer to the KP to flush values for
   * @param time          The timestamp to associate the values with in the DB
   * @return 0 if the data was written successfully, -1 otherwise.
   */
  int (*kp_flush)(timeseries_backend_t *backend,
		  timeseries_kp_t *kp, uint32_t time);

  /** Write the value for a single key to the database
   *
   * @param backend     Pointer to a backend instance to write to
   * @param key         String key name
   * @param value       Value to set the key to
   * @param time        The time slot to set the key's value for
   *
   * @warning this function will perform much worse than using the Key Package
   * functions above, use with caution
   */
  int (*set_single)(timeseries_backend_t *backend, const char *key,
		    uint64_t value, uint32_t time);

  /** }@ */

  /**
   * @name Backend state fields
   *
   * These fields are only set if the backend is enabled (and initialized)
   * @note These fields should *not* be directly manipulated by
   * backends. Instead they should use accessor functions provided by the
   * backend manager.
   *
   * @{ */

  int enabled;

  /** An opaque pointer to backend-specific state if needed by the backend */
  void *state;

  /** }@ */
};

/**
 * @name Backend setup functions
 *
 * These functions are to be used solely by the timeseries framework initializing
 * and freeing backend plugins.
 *
 * @{ */

/** Allocate all backend objects
 *
 * @param timeseries    The timeseries object to allocate backends for
 * @return 0 if all backends were successfully allocated, -1 otherwise
 */
int timeseries_backend_alloc_all(timeseries_t *timeseries);

/** Initialize a backend object
 *
 * @param timeseries    The timeseries object to initialize the backend for
 * @param backend_id    The unique ID of the metadata backend
 * @return the backend object created, NULL if an error occurred
 */
int timeseries_backend_init(timeseries_t *timeseries,
			    timeseries_backend_t *backend,
			    int argc, char **argv);

/** Free the given backend object
 *
 * @param timeseries    The timeseries object to remove the backend from
 * @param backend       The backend object to free
 *
 * @note if this backend was the default, there will be *no* default backend set
 * after this function returns
 */
void timeseries_backend_free(timeseries_t *timeseries,
			     timeseries_backend_t *backend);

/** Ask all backend providers to free their state for the given Key Package
 *
 * @param */
void timeseries_backend_kp_free(timeseries_kp_t *kp);

/** }@ */

/**
 * @name Backend convenience functions
 *
 * These functions are to be used solely by backend implementations to access
 * the backend opaque structure
 *
 * @{ */

/** Register the state for a backend
 *
 * @param backend       The backend to register state for
 * @param state         A pointer to the state object to register
 */
void timeseries_backend_register_state(timeseries_backend_t *backend,
				       void *state);

/** Free the state for a backend
 *
 * @param backend       The backend to free state for
 */
void timeseries_backend_free_state(timeseries_backend_t *backend);

 /** }@ */

#endif /* __TIMESERIES_BACKEND_H */
