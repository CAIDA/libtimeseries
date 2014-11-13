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

#ifndef __TSMQ_CLIENT_H
#define __TSMQ_CLIENT_H

#include <timeseries.h>

#include <tsmq_common.h>

/** @file
 *
 * @brief Header file that contains the public components of the tsmq metadata
 * client.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** Default URI for the client -> broker connection */
#define TSMQ_CLIENT_BROKER_URI_DEFAULT "tcp://127.0.0.1:7300"

/** Default the client request timeout to 2.5 seconds */
#define TSMQ_CLIENT_REQUEST_TIMEOUT 2500

/** Default the client request retry count to 3 */
#define TSMQ_CLIENT_REQUEST_RETRIES 3

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** tsmq metadata client */
typedef struct tsmq_client tsmq_client_t;

/** tsmq metadata key */
typedef struct tsmq_client_key tsmq_client_key_t;

/** @} */

/**
 * @name Public Metadata Client API
 *
 * @{ */

/** Initialize a new instance of a tsmq metadata client
 *
 * @return a pointer to a tsmq md client structure if successful, NULL if an
 * error occurred
 */
tsmq_client_t *tsmq_client_init();

/** Start a given tsmq metadata client
 *
 * @param client        pointer to the client instance to start
 * @return 0 if the client was started successfully, -1 otherwise
 */
int tsmq_client_start(tsmq_client_t *client);

/** Free a tsmq md client instance
 *
 * @param client        pointer to a tsmq md client instance to free
 */
void tsmq_client_free(tsmq_client_t **client_p);

/** Set the URI for the client to connect to the broker on
 *
 * @param client        pointer to a tsmq md client instance to update
 * @param uri           pointer to a uri string
 */
void tsmq_client_set_broker_uri(tsmq_client_t *client, const char *uri);

/** Set the request timeout
 *
 * @param client        pointer to a tsmq md client instance to update
 * @param timeout_ms    time in ms before request is retried
 *
 * @note defaults to TSMQ_CLIENT_REQUEST_TIMEOUT
 */
void tsmq_client_set_request_timeout(tsmq_client_t *client,
				     uint64_t timeout_ms);

/** Set the number of request retries before a request is abandoned
 *
 * @param client        pointer to a tsmq md client instance to update
 * @param retry_cnt     number of times to retry a request before giving up
 *
 * @note defaults to TSMQ_CLIENT_REQUEST_RETRIES
 */
void tsmq_client_set_request_retries(tsmq_client_t *client,
				     int retry_cnt);

/** Given an array of bytes that represent a metric key (probably a string),
 *  issue a request to find the appropriate server and corresponding key id.
 *
 * @param client        pointer to a tsmq md client instance to query
 * @param key           pointer to a string key
 * @return a pointer to a key info structure if successful, NULL otherwise
 *
 * @note this key info structure must be used when issuing subsequent write
 * requests. The broker will batch write requests into a single write for each
 * backend server based on the backend id in this structure.
 */
tsmq_client_key_t *tsmq_client_key_lookup(tsmq_client_t *client,
					  const char *key);

/** Given an key package, issue a request to resolve string keys to
 * backend-specific ids.
 *
 * @param client        pointer to a tsmq md client instance to query
 * @param kp            pointer to a Key Package
 * @param force         if set to 1, all keys will be looked up, even if they
 *                      are already resolved
 * @return 0 if all keys were resolved successfully, -1 otherwise
 */
int tsmq_client_key_lookup_bulk(tsmq_client_t *client,
                                timeseries_kp_t *kp,
                                int force);

/** Write the value for a single key to the database(s)
 *
 * @param client        pointer to a tsmq md client instance
 * @param key           pointer to a key structure (returned by `_key_lookup`)
 * @param value         value to set for the given key
 * @param time          time slot to set value for
 * @return 0 if the command was successfully queued, -1 otherwise
 */
int tsmq_client_key_set_single(tsmq_client_t *client,
			       tsmq_client_key_t *key,
			       tsmq_val_t value, tsmq_time_t time);

/** Free a key info structure
 *
 * @param key           double pointer to the key info structure to free
 */
void tsmq_client_key_free(tsmq_client_key_t **key_p);

/** Publish the error API for the metadata client */
TSMQ_ERR_PROTOS(client)

/** @} */

#endif /* __TSMQ_CLIENT_H */






