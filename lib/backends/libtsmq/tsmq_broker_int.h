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

#ifndef __TSMQ_BROKER_INT_H
#define __TSMQ_BROKER_INT_H

#include <czmq.h>

#include "khash.h"

#include <tsmq_broker.h>
#include "tsmq_int.h"

/** @file
 *
 * @brief Header file that contains the private components of tsmq.
 *
 * @author Alistair King
 *
 */

typedef struct tsmq_broker_server tsmq_broker_server_t;

KHASH_INIT(str_server, char*, tsmq_broker_server_t*, 1,
	   kh_str_hash_func, kh_str_hash_equal);

struct tsmq_broker {
  /** Common tsmq state */
  tsmq_t *tsmq;

  /** URI to listen for clients on */
  char *client_uri;

  /** Socket to bind to for client connections */
  void *client_socket;

  /** URI to listen for servers on */
  char *server_uri;

  /** Socket to bind to for client connections */
  void *server_socket;

  /** Hash of servers that are connected */
  khash_t(str_server) *servers;

  /** Time (in ms) between heartbeats sent to servers */
  uint64_t heartbeat_interval;

  /** The number of heartbeats that can go by before a server is declared
      dead */
  int heartbeat_liveness;

  /** Event loop */
  zloop_t *loop;

  /** Heartbeat timer ID */
  int timer_id;
};

#endif /* __TSMQ_BROKER_INT_H */
