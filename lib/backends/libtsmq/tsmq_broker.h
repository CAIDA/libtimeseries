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

#ifndef __TSMQ_BROKER_H
#define __TSMQ_BROKER_H

#include <tsmq_common.h>

/** @file
 *
 * @brief Header file that contains the public components of the tsmq metadata
 * broker
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** Default URI for the broker to listen for client requests on */
#define TSMQ_BROKER_CLIENT_URI_DEFAULT "tcp://*:7300"

/** Default URI for the broker to listen for server connections on */
#define TSMQ_BROKER_SERVER_URI_DEFAULT "tcp://*:7400"

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** tsmq metadata broker */
typedef struct tsmq_broker tsmq_broker_t;

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
tsmq_broker_t *tsmq_broker_init();

/** Start a given tsmq metadata broker
 *
 * @param broker        pointer to the broker instance to start
 * @return 0 if the broker was started successfully, -1 otherwise
 */
int tsmq_broker_start(tsmq_broker_t *broker);

/** Free a tsmq md broker instance
 *
 * @param broker        pointer to a tsmq md broker instance to free
 */
void tsmq_broker_free(tsmq_broker_t *broker);

/** Set the URI for the broker to listen for client connections on
 *
 * @param server        pointer to a tsmq md broker instance to update
 * @param uri           pointer to a uri string
 *
 * @note defaults to TSMQ_BROKER_CLIENT_URI_DEFAULT
 */
void tsmq_broker_set_client_uri(tsmq_broker_t *broker, const char *uri);

/** Set the URI for the broker to listen for server connections on
 *
 * @param server        pointer to a tsmq md broker instance to update
 * @param uri           pointer to a uri string
 *
 * @note defaults to TSMQ_BROKER_SERVER_URI_DEFAULT
 */
void tsmq_broker_set_server_uri(tsmq_broker_t *broker, const char *uri);

/** Set the heartbeat interval
 *
 * @param broker        pointer to a tsmq md broker instance to update
 * @param interval_ms   time in ms between heartbeats
 *
 * @note defaults to TSMQ_HEARTBEAT_INTERVAL
 */
void tsmq_broker_set_heartbeat_interval(tsmq_broker_t *broker,
					uint64_t interval_ms);

/** Set the heartbeat liveness
 *
 * @param broker        pointer to a tsmq md broker instance to update
 * @param beats         number of heartbeats that can go by before a server is
 *                      declared dead
 *
 * @note defaults to TSMQ_HEARTBEAT_LIVENESS
 */
void tsmq_broker_set_heartbeat_liveness(tsmq_broker_t *broker,
					int beats);

/** Publish the error API for the metadata broker */
TSMQ_ERR_PROTOS(broker)

/** @} */

#endif /* __TSMQ_BROKER_H */
