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

#ifndef __TIMESERIES_BACKEND_INT_H
#define __TIMESERIES_BACKEND_INT_H

#include <inttypes.h>

#include "timeseries_kp_int.h"
#include "timeseries_backend_pub.h"

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
#define TIMESERIES_BACKEND_STATE(type, backend)                                \
  ((timeseries_backend_##type##_state_t *)(backend)->state)

/** Convenience macro that defines all the function prototypes for the
 * timeseries
 * backend API
 */
#define TIMESERIES_BACKEND_GENERATE_PROTOS(provname)                           \
  timeseries_backend_t *timeseries_backend_##provname##_alloc();               \
  int timeseries_backend_##provname##_init(timeseries_backend_t *ds, int argc, \
                                           char **argv);                       \
  void timeseries_backend_##provname##_free(timeseries_backend_t *ds);         \
  int timeseries_backend_##provname##_kp_init(                                 \
    timeseries_backend_t *backend, timeseries_kp_t *kp, void **kp_state_p);    \
  void timeseries_backend_##provname##_kp_free(                                \
    timeseries_backend_t *backend, timeseries_kp_t *kp, void *kp_state);       \
  int timeseries_backend_##provname##_kp_ki_update(                            \
    timeseries_backend_t *backend, timeseries_kp_t *kp);                       \
  void timeseries_backend_##provname##_kp_ki_free(                             \
    timeseries_backend_t *backend, timeseries_kp_t *kp,                        \
    timeseries_kp_ki_t *ki, void *ki_state);                                   \
  int timeseries_backend_##provname##_kp_flush(                                \
    timeseries_backend_t *backend, timeseries_kp_t *kp, uint32_t time);        \
  int timeseries_backend_##provname##_set_single(                              \
    timeseries_backend_t *backend, const char *key, uint64_t value,            \
    uint32_t time);                                                            \
  int timeseries_backend_##provname##_set_single_by_id(                        \
    timeseries_backend_t *backend, uint8_t *id, size_t id_len, uint64_t value, \
    uint32_t time);                                                            \
  int timeseries_backend_##provname##_set_bulk_init(                           \
    timeseries_backend_t *backend, uint32_t key_cnt, uint32_t time);           \
  int timeseries_backend_##provname##_set_bulk_by_id(                          \
    timeseries_backend_t *backend, uint8_t *id, size_t id_len,                 \
    uint64_t value);                                                           \
  size_t timeseries_backend_##provname##_resolve_key(                          \
    timeseries_backend_t *backend, const char *key, uint8_t **backend_key);    \
  int timeseries_backend_##provname##_resolve_key_bulk(                        \
    timeseries_backend_t *backend, uint32_t keys_cnt, const char *const *keys, \
    uint8_t **backend_keys, size_t *backend_key_lens, int *contig_alloc);

/** Convenience macro that defines all the function pointers for the timeseries
 * backend API
 */
#define TIMESERIES_BACKEND_GENERATE_PTRS(provname)                             \
  timeseries_backend_##provname##_init, timeseries_backend_##provname##_free,  \
    timeseries_backend_##provname##_kp_init,                                   \
    timeseries_backend_##provname##_kp_free,                                   \
    timeseries_backend_##provname##_kp_ki_update,                              \
    timeseries_backend_##provname##_kp_ki_free,                                \
    timeseries_backend_##provname##_kp_flush,                                  \
    timeseries_backend_##provname##_set_single,                                \
    timeseries_backend_##provname##_set_single_by_id,                          \
    timeseries_backend_##provname##_set_bulk_init,                             \
    timeseries_backend_##provname##_set_bulk_by_id,                            \
    timeseries_backend_##provname##_resolve_key,                               \
    timeseries_backend_##provname##_resolve_key_bulk, 0, NULL

/** Structure which represents a metadata backend */
struct timeseries_backend {
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
  int (*init)(struct timeseries_backend *backend, int argc, char **argv);

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
   * @param[out] state  Set to pointer to the state allocated, or NULL if no
   *                    state is needed by the backend
   * @return 0 if state was allocated successfully, -1 otherwise
   */
  int (*kp_init)(timeseries_backend_t *backend, timeseries_kp_t *kp,
                 void **kp_state_p);

  /** Free the backend-specific state in the given Key Package.
   *
   * @param backend     Pointer to a backend instance
   * @param kp          Pointer to the KP to free state for
   * @param state       Pointer to the state to free
   *
   * @note This function should free the state created by kp_init.
   */
  void (*kp_free)(timeseries_backend_t *backend, timeseries_kp_t *kp,
                  void *kp_state);

  /** Update backend-specific Key Info state in the given Key Package object
   *
   * @param      backend     Pointer to a backend instance
   * @param      kp          Pointer to the KP to update
   * @return 0 if state was updated successfully, -1 otherwise
   *
   * For example: the DBATS backend needs to ask DBATS what the internal key id
   * is for the string key.
   *
   * Backends should use the TIMESERIES_KP_FOREACH_KI macro to iterate over all
   * KIs in the KP and then use the timeseries_kp_get_ki and
   * timeseries_kp_ki_get_backend_state functions to get a pointer to the state
   * to update.
   */
  int (*kp_ki_update)(timeseries_backend_t *backend, timeseries_kp_t *kp);

  /** Free the backend-specific state in the given Key Info object.
   *
   * @param backend    Pointer to a backend instance
   * @param kp         Pointer to the KP the KI is a member of
   * @param kv         Pointer to the KI to free state for
   * @param ki_state   Pointer to the state to free
   *
   * @note This function should free the state created by kp_kv_add.
   */
  void (*kp_ki_free)(timeseries_backend_t *backend, timeseries_kp_t *kp,
                     timeseries_kp_ki_t *ki, void *ki_state);

  /** Flush the current values in the given Key Package to the database
   *
   * @param backend       Pointer to a backend instance to flush to
   * @param kp            Pointer to the KP to flush values for
   * @param time          The timestamp to associate the values with in the DB
   * @return 0 if the data was written successfully, -1 otherwise.
   */
  int (*kp_flush)(timeseries_backend_t *backend, timeseries_kp_t *kp,
                  uint32_t time);

  /** Write the value for a single key to the database
   *
   * @param backend     Pointer to a backend instance to write to
   * @param key         String key name
   * @param value       Value to set the key to
   * @param time        The time slot to set the key's value for
   * @return 0 if the value was set successfully, -1 otherwise
   *
   * @warning this function may perform much worse than using the Key Package
   * functions above (or the set_single_by_id function below), use with caution
   */
  int (*set_single)(timeseries_backend_t *backend, const char *key,
                    uint64_t value, uint32_t time);

  /** Write the value for a single key ID (retrieved using resolve_key) to the
   * database
   *
   * @param backend     Pointer to the backend instance to write to
   * @param id          Pointer to the backend-specific key ID byte array
   * @param id_len      Length of the key ID byte array
   * @param value       Value to set
   * @param time        The time slot to set the value for
   * @return 0 if the value was set successfully, -1 otherwise
   */
  int (*set_single_by_id)(timeseries_backend_t *backend, uint8_t *id,
                          size_t id_len, uint64_t value, uint32_t time);

  /** Prepare to write a bulk set of values to the database
   *
   * @param backend     Pointer to the backend instance to write to
   * @param key_cnt     Number of keys (expect key_cnt set_bulk_by_id calls
   *                    before flushing)
   * @param time        The time slot to set the values for
   * @return 0 if the values were set successfully, -1 otherwise
   *
   * @note the backend only guarantees to flush the values once key_cnt calls to
   * set_bulk_by_id have been received after calling this function. Users should
   * call set_bulk_init with the number of values they will store, and then call
   * set_bulk_by_id for each of them.
   */
  int (*set_bulk_init)(timeseries_backend_t *backend, uint32_t key_cnt,
                       uint32_t time);

  /** Queue the value for a key ID (retrieved using resolve_key) for writing to
   * the database. Must follow a call to set_bulk_init
   *
   * @param backend     Pointer to the backend instance to write to
   * @param id          Pointer to the backend-specific key ID byte array
   * @param id_len      Length of the key ID byte array
   * @param value       Value to set
   * @return 0 if the value was set successfully, -1 otherwise
   */
  int (*set_bulk_by_id)(timeseries_backend_t *backend, uint8_t *id,
                        size_t id_len, uint64_t value);

  /** Resolve the given key into a backend-specific opaque ID.
   *
   * @param backend           Pointer to the backend to resolve the key for
   * @param key               String key name
   * @param backend_key[out]  Set to a pointer to a byte array containing the
   *                            backend key (memory owned by the caller)
   * @return the number of bytes in the returned key, 0 if an error occurred
   *
   * @note if no key exists, the backend should dynamically create it and return
   * the id.
   */
  size_t (*resolve_key)(timeseries_backend_t *backend, const char *key,
                        uint8_t **backend_key);

  /** Resolve the given set of keys into backend-specific opaque IDs.
   *
   * @param backend           Pointer to the backend to resolve the key for
   * @param keys_cnt          Number of keys to resolve
   * @param keys              Array of string key names
   * @param backend_keys[out] Array to store results of lookups in
   * @param backend_key_lens[out]  Array to store length of each key result
   * @param contig_alloc[out] Set to 1 if all backend_key arrays are allocated
   *                          contiguously, 0 otherwise
   * @return 0 if all keys were successfully resolved, -1 otherwise
   *
   * Each key in the `keys` array will be looked up, and a backend-specific byte
   * array will be stored in the corresponding element of the backend_keys
   * array. Memory for this byte array is owned by the caller of this function
   * (see note below). The length of each byte array is to be stored in the
   * corresponding element of the backend_key_lens array. If no key exists, the
   * backend should dynamically create it and return the id.
   *
   * @note if contig_alloc is set, only the first backend key byte array should
   * be freed by the caller, as all other arrays are stored in the same memory
   * allocation. However, if contig_alloc is 0, then each backend key byte array
   * must be freed individually.
   */
  int (*resolve_key_bulk)(timeseries_backend_t *backend, uint32_t keys_cnt,
                          const char *const *keys, uint8_t **backend_keys,
                          size_t *backend_key_lens, int *contig_alloc);

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
 * These functions are to be used solely by the timeseries framework
 * initializing
 * and freeing backend plugins.
 *
 * @{ */

/** Allocate all backend objects
 *
 * @param id            ID of the backend to allocate
 * @return pointer to the backend allocated, NULL if an error occurred, or the
 * backend is
 *         disabled.
 */
timeseries_backend_t *timeseries_backend_alloc(timeseries_backend_id_t id);

/** Initialize a backend object
 *
 * @param backend       Pointer to an alloc'd backend structure
 * @param argc          Number of elements in the argument array
 * @param argv          Pointer to the argument array
 * @return 0 if the backend was initialized successfully, -1 otherwise
 */
int timeseries_backend_init(timeseries_backend_t *backend, int argc,
                            char **argv);

/** Free the given backend object
 *
 * @param timeseries    The timeseries object to remove the backend from
 * @param backend       Double-pointer to the backend object to free
 */
void timeseries_backend_free(timeseries_backend_t **backend_p);

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
