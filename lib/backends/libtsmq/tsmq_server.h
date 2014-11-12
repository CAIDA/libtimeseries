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

#include <timeseries.h>

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
* @name Public Server API
*
* @{ */

/** Initialize a new instance of a tsmq metadata server
*
* @param ts_backend     pointer to an initialized and configured libtimeseries
*                       backend instance to write to
* @return a pointer to a tsmq md server structure if successful, NULL if an
* error occurred
*/
tsmq_server_t *tsmq_server_init(timeseries_backend_t *ts_backend);

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
