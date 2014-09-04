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

#ifndef __TSMQ_H
#define __TSMQ_H

#include <stdint.h>
#include <wandio.h>

/** @file
 *
 * @brief Header file that exposes the public interface of tsmq.
 *
 * @author Alistair King
 *
 */

#define TSMQ_GET_ERR_PROTO(type)			\
  tsmq_err_t tsmq_##type##_get_err(tsmq_##type##_t *t);

#define TSMQ_IS_ERR_PROTO(type)				\
  int tsmq_##type##_is_err(tsmq_##type##_t *t);

#define TSMQ_PERR_PROTO(type)				\
  void tsmq_##type##_perr(tsmq_##type##_t *t);

#define TSMQ_ERR_PROTOS(type)			\
  TSMQ_GET_ERR_PROTO(type)			\
  TSMQ_IS_ERR_PROTO(type)			\
  TSMQ_PERR_PROTO(type)

/**
 * @name Public Constants
 *
 * @{ */

/** Default URI for the server -> broker connection */
#define TSMQ_MD_SERVER_BROKER_URI_DEFAULT "tcp://127.0.0.1:7400"

/** Default URI for the client -> broker connection */
#define TSMQ_MD_CLIENT_BROKER_URI_DEFAULT "tcp://127.0.0.1:7300"

/** Default URI for the broker to listen for client requests on */
#define TSMQ_MD_BROKER_CLIENT_URI_DEFAULT "tcp://*:7300"

/** Default URI for the broker to listen for server connections on */
#define TSMQ_MD_BROKER_SERVER_URI_DEFAULT "tcp://*:7400"

/** Default the client request timeout to 2.5 seconds */
#define TSMQ_MD_CLIENT_REQUEST_TIMEOUT 2500

/** Default the client request retry count to 3 */
#define TSMQ_MD_CLIENT_REQUEST_RETRIES 3

/** Default the broker/server heartbeat interval to 1 second */
#define TSMQ_MD_HEARTBEAT_INTERVAL_DEFAULT 1000

/** Default the broker/server heartbeat liveness to 3 beats */
#define TSMQ_MD_HEARTBEAT_LIVENESS_DEFAULT 3

/** Default the server reconnect minimum interval to 1 second */
#define TSMQ_MD_RECONNECT_INTERVAL_MIN 1000

/** Default the server reconnect maximum interval to 32 seconds */
#define TSMQ_MD_RECONNECT_INTERVAL_MAX 32000


/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** tsmq metadata server */
typedef struct tsmq_md_server tsmq_md_server_t;

/** tsmq metadata broker */
typedef struct tsmq_md_broker tsmq_md_broker_t;

/** tsmq metadata client */
typedef struct tsmq_md_client tsmq_md_client_t;

/** tsmq metadata key */
typedef struct tsmq_md_client_key tsmq_md_client_key_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** tsmq error information */
typedef struct tsmq_err {
  /** Error code */
  int err_num;

  /** String representation of the error that occurred */
  char problem[255];
} tsmq_err_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** Enumeration of error codes
 *
 * @note these error codes MUST be <= 0
 */
typedef enum {

  /** No error has occured */
  TSMQ_ERR_NONE         = 0,

  /** tsmq failed to initialize */
  TSMQ_ERR_INIT_FAILED  = -1,

  /** tsmq failed to start */
  TSMQ_ERR_START_FAILED = -2,

  /** tsmq was interrupted */
  TSMQ_ERR_INTERRUPT    = -3,

  /** unhandled error */
  TSMQ_ERR_UNHANDLED    = -4,

  /** protocol error */
  TSMQ_ERR_PROTOCOL     = -5,

  /** malloc error */
  TSMQ_ERR_MALLOC       = -6,

} tsmq_err_code_t;

/** @} */

/**
 * @name Public Metadata Server API
 *
 * @{ */

/** Initialize a new instance of a tsmq metadata server
 *
 * @param id            pointer to an id byte array
 * @paran id_len        length of the id byte array
 * @return a pointer to a tsmq md server structure if successful, NULL if an
 * error occurred
 *
 * @note The id must be globally unique amongst servers. The broker will not
 * accept connections from duplicate ids.
 * @note The id will be used by the broker to route metrics to the appropriate
 * server. The server with the longest match to the beginning of a key will be
 * routed a metric.
 * @todo figure out how to have a catch-all server
 */
tsmq_md_server_t *tsmq_md_server_init();

/** Start a given tsmq metadata server
 *
 * @param server        pointer to the server instance to start
 * @return 0 if the server was started successfully, -1 otherwise
 */
int tsmq_md_server_start(tsmq_md_server_t *server);

/** Free a tsmq md server instance
 *
 * @param server        pointer to a tsmq md server instance to free
 */
void tsmq_md_server_free(tsmq_md_server_t *server);

/** Set the URI for the server to connect to the broker on
 *
 * @param server        pointer to a tsmq md server instance to update
 * @param uri           pointer to a uri string
 */
void tsmq_md_server_set_broker_uri(tsmq_md_server_t *server, const char *uri);

/** Set the heartbeat interval
 *
 * @param server        pointer to a tsmq md server instance to update
 * @param interval_ms   time in ms between heartbeats
 *
 * @note defaults to TSMQ_MD_HEARTBEAT_INTERVAL
 */
void tsmq_md_server_set_heartbeat_interval(tsmq_md_server_t *server,
					   uint64_t interval_ms);

/** Set the heartbeat liveness
 *
 * @param server        pointer to a tsmq md server instance to update
 * @param beats         number of heartbeats that can go by before a server is
 *                      declared dead
 *
 * @note defaults to TSMQ_MD_HEARTBEAT_LIVENESS
 */
void tsmq_md_server_set_heartbeat_liveness(tsmq_md_server_t *server,
					   int beats);

/** Set the minimum reconnect time
 *
 * @param server        pointer to a tsmq md server instance to update
 * @param time          min time in ms to wait before reconnecting to broker
 *
 * @note defaults to TSMQ_MD_RECONNECT_INTERVAL_MIN
 */
void tsmq_md_server_set_reconnect_interval_min(tsmq_md_server_t *server,
					       uint64_t reconnect_interval_min);

/** Set the maximum reconnect time
 *
 * @param server        pointer to a tsmq md server instance to update
 * @param time          max time in ms to wait before reconnecting to broker
 *
 * @note defaults to TSMQ_MD_RECONNECT_INTERVAL_MAX
 */
void tsmq_md_server_set_reconnect_interval_max(tsmq_md_server_t *server,
					       uint64_t reconnect_interval_max);

/** Publish the error API for the metadata server */
TSMQ_ERR_PROTOS(md_server)

/** @} */



/**
 * @name Public Metadata Broker API
 *
 * @{ */

/** Initialize a new instance of a tsmq metadata broker
 *
 * @return a pointer to a tsmq md broker structure if successful, NULL if an
 * error occurred
 */
tsmq_md_broker_t *tsmq_md_broker_init();

/** Start a given tsmq metadata broker
 *
 * @param broker        pointer to the broker instance to start
 * @return 0 if the broker was started successfully, -1 otherwise
 */
int tsmq_md_broker_start(tsmq_md_broker_t *broker);

/** Free a tsmq md broker instance
 *
 * @param broker        pointer to a tsmq md broker instance to free
 */
void tsmq_md_broker_free(tsmq_md_broker_t *broker);

/** Set the URI for the broker to listen for client connections on
 *
 * @param server        pointer to a tsmq md broker instance to update
 * @param uri           pointer to a uri string
 *
 * @note defaults to TSMQ_MD_BROKER_CLIENT_URI_DEFAULT
 */
void tsmq_md_broker_set_client_uri(tsmq_md_broker_t *broker, const char *uri);

/** Set the URI for the broker to listen for server connections on
 *
 * @param server        pointer to a tsmq md broker instance to update
 * @param uri           pointer to a uri string
 *
 * @note defaults to TSMQ_MD_BROKER_SERVER_URI_DEFAULT
 */
void tsmq_md_broker_set_server_uri(tsmq_md_broker_t *broker, const char *uri);

/** Set the heartbeat interval
 *
 * @param broker        pointer to a tsmq md broker instance to update
 * @param interval_ms   time in ms between heartbeats
 *
 * @note defaults to TSMQ_MD_HEARTBEAT_INTERVAL
 */
void tsmq_md_broker_set_heartbeat_interval(tsmq_md_broker_t *broker,
					   uint64_t interval_ms);

/** Set the heartbeat liveness
 *
 * @param broker        pointer to a tsmq md broker instance to update
 * @param beats         number of heartbeats that can go by before a server is
 *                      declared dead
 *
 * @note defaults to TSMQ_MD_HEARTBEAT_LIVENESS
 */
void tsmq_md_broker_set_heartbeat_liveness(tsmq_md_broker_t *broker,
					   int beats);

/** Publish the error API for the metadata broker */
TSMQ_ERR_PROTOS(md_broker)

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
tsmq_md_client_t *tsmq_md_client_init();

/** Start a given tsmq metadata client
 *
 * @param client        pointer to the client instance to start
 * @return 0 if the client was started successfully, -1 otherwise
 */
int tsmq_md_client_start(tsmq_md_client_t *client);

/** Free a tsmq md client instance
 *
 * @param client        pointer to a tsmq md client instance to free
 */
void tsmq_md_client_free(tsmq_md_client_t *client);

/** Set the URI for the client to connect to the broker on
 *
 * @param client        pointer to a tsmq md client instance to update
 * @param uri           pointer to a uri string
 */
void tsmq_md_client_set_broker_uri(tsmq_md_client_t *client, const char *uri);

/** Set the request timeout
 *
 * @param client        pointer to a tsmq md client instance to update
 * @param timeout_ms    time in ms before request is retried
 *
 * @note defaults to TSMQ_MD_CLIENT_REQUEST_TIMEOUT
 */
void tsmq_md_client_set_request_timeout(tsmq_md_client_t *client,
					uint64_t timeout_ms);

/** Set the number of request retries before a request is abandoned
 *
 * @param client        pointer to a tsmq md client instance to update
 * @param retry_cnt     number of times to retry a request before giving up
 *
 * @note defaults to TSMQ_MD_CLIENT_REQUEST_RETRIES
 */
void tsmq_md_client_set_request_retries(tsmq_md_client_t *client,
					int retry_cnt);

/** Given an array of bytes that represent a metric key (probably a string),
 *  issue a request to find the appropriate server and corresponding key id.
 *
 * @param client        pointer to a tsmq md client instance to query
 * @param key           pointer to a byte array
 * @param len           length of the byte array
 * @return a pointer to a key info structure if successful, NULL otherwise
 *
 * @note this key info structure must be used when issuing subsequent write
 * requests. The broker will batch write requests into a single write for each
 * backend server based on the backend id in this structure.
 */
tsmq_md_client_key_t *tsmq_md_client_key_lookup(tsmq_md_client_t *client,
						const uint8_t *key, size_t len);

/** Free a key info structure
 *
 * @param key           pointer to the key info structure to free
 */
void tsmq_md_client_key_free(tsmq_md_client_key_t *key);

/** Publish the error API for the metadata client */
TSMQ_ERR_PROTOS(md_client)

/** @} */

#endif /* __TSMQ_H */
