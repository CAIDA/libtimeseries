/*
 * tsmq
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of tsmq.
 *
 * tsmq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tsmq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tsmq.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __TSMQ_SERVER_H
#define __TSMQ_SERVER_H

#include <tsmq_common.h>

/** @file
 *
 * @brief Header file that contains the public components of the tsmq metadata
 * server.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** Default URI for the server -> broker connection */
#define TSMQ_SERVER_BROKER_URI_DEFAULT "tcp://127.0.0.1:7400"

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** tsmq metadata server */
typedef struct tsmq_server tsmq_server_t;

/** @} */

/**
 * @name Server callback definitions and setter functions
 *
 * @{ */

/** Uses the timeseries API to resolve the given key to a backend-specific ID
 *
 * @param server          pointer to the server instance that received the request
 * @param key             pointer to a string key
 * @param server_key[out] set to point to a byte array of the server key id
 * @return the length of the server_key array if successful, 0 otherwise
 */
typedef size_t (tsmq_server_cb_key_lookup_t)(tsmq_server_t *server,
					     char *key,
					     uint8_t **server_key,
					     void *user);

/** Uses the timeseries API to set the value for the given backend-specific ID
 *
 * @param server          pointer to the server instance that received the request
 * @param id              pointer to a backend-specific byte array
 * @param id_len          length of the ID byte array
 * @param value           value to set
 * @param time            time for which to set the value for
 * @return 0 if the value was successfully set, -1 otherwise
 */
typedef int (tsmq_server_cb_set_single_t)(tsmq_server_t *server,
					  uint8_t *id, size_t id_len,
					  tsmq_val_t value,
					  tsmq_time_t time,
					  void *user);

/** Register a function to be called to handle key lookups
 *
 * @param client        pointer to a client instance to set callback for
 * @param cb            pointer to a handle_reply callback function
 */
void tsmq_server_set_cb_key_lookup(tsmq_server_t *server,
				   tsmq_server_cb_key_lookup_t *cb);

/** Register a function to be called to handle insertion of single key/value
*
* @param client        pointer to a client instance to set callback for
* @param cb            pointer to a set_single callback function
*/
void tsmq_server_set_cb_set_single(tsmq_server_t *server,
				   tsmq_server_cb_set_single_t *cb);

/** Set the user data that will provided to each callback function */
void tsmq_server_set_cb_userdata(tsmq_server_t *server,
				 void *user);

/** @} */

/**
* @name Public Metadata Server API
*
* @{ */

/** Initialize a new instance of a tsmq metadata server
*
* @todo add callback structure here
* @return a pointer to a tsmq md server structure if successful, NULL if an
* error occurred
*
* @note Currently we will only consider one timeseries backend for writing.  If
* more than one is provided, the one with the lowest id will be used. As of the
* time of writing this, this would be the ASCII backend.
*/
tsmq_server_t *tsmq_server_init();

/** Start a given tsmq metadata server
*
* @param server        pointer to the server instance to start
* @return 0 if the server was started successfully, -1 otherwise
*/
int tsmq_server_start(tsmq_server_t *server);

/** Free a tsmq md server instance
*
* @param server        pointer to a tsmq md server instance to free
*/
void tsmq_server_free(tsmq_server_t *server);

/** Set the URI for the server to connect to the broker on
*
* @param server        pointer to a tsmq md server instance to update
* @param uri           pointer to a uri string
*/
void tsmq_server_set_broker_uri(tsmq_server_t *server, const char *uri);

/** Set the heartbeat interval
*
* @param server        pointer to a tsmq md server instance to update
* @param interval_ms   time in ms between heartbeats
*
* @note defaults to TSMQ_HEARTBEAT_INTERVAL
*/
void tsmq_server_set_heartbeat_interval(tsmq_server_t *server,
					uint64_t interval_ms);

/** Set the heartbeat liveness
*
* @param server        pointer to a tsmq md server instance to update
* @param beats         number of heartbeats that can go by before a server is
*                      declared dead
*
* @note defaults to TSMQ_HEARTBEAT_LIVENESS
*/
void tsmq_server_set_heartbeat_liveness(tsmq_server_t *server,
					int beats);

/** Set the minimum reconnect time
*
* @param server        pointer to a tsmq md server instance to update
* @param time          min time in ms to wait before reconnecting to broker
*
* @note defaults to TSMQ_RECONNECT_INTERVAL_MIN
*/
void tsmq_server_set_reconnect_interval_min(tsmq_server_t *server,
					    uint64_t reconnect_interval_min);

/** Set the maximum reconnect time
*
* @param server        pointer to a tsmq md server instance to update
* @param time          max time in ms to wait before reconnecting to broker
*
* @note defaults to TSMQ_RECONNECT_INTERVAL_MAX
*/
void tsmq_server_set_reconnect_interval_max(tsmq_server_t *server,
					    uint64_t reconnect_interval_max);

/** Publish the error API for the metadata server */
TSMQ_ERR_PROTOS(server)

/** @} */

#endif /* __TSMQ_SERVER_H */
