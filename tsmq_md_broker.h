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

#ifndef __TSMQ_MD_BROKER_H
#define __TSMQ_MD_BROKER_H

#include "tsmq_int.h"

#include <czmq.h>

/** @file
 *
 * @brief Header file that contains the private components of tsmq.
 *
 * @author Alistair King
 *
 */

struct tsmq_md_broker {
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

  /** List of servers that are connected */
  zlist_t *servers;

  /** Time (in ms) between heartbeats sent to servers */
  uint64_t heartbeat_interval;

  /** Time (in ms) to send the next heartbeat to servers */
  uint64_t heartbeat_next;

  /** The number of heartbeats that can go by before a server is declared
      dead */
  int heartbeat_liveness;
};

#endif /* __TSMQ_MD_BROKER_H */
