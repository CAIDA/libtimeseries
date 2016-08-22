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

#ifndef __TSMQ_CLIENT_INT_H
#define __TSMQ_CLIENT_INT_H

#include "tsmq_client.h"
#include "tsmq_int.h"

/** @file
 *
 * @brief Header file that contains the private components of the tsmq metadata
 * client.
 *
 * @author Alistair King
 *
 */

struct tsmq_client {
  /** Common tsmq state */
  tsmq_t *tsmq;

  /** URI to connect to the broker on */
  char *broker_uri;

  /** Socket used to connect to the broker */
  void *broker_socket;

  /** Request sequence number */
  uint64_t sequence_num;

  /** Request ACK timeout in msec */
  uint64_t request_ack_timeout;

  /** Key resolution request timeout in msec */
  uint64_t key_lookup_timeout;

  /** Key set request timeout in msec */
  uint64_t key_set_timeout;

  /** Request retries */
  int request_retries;

  /** Time that we will give up waiting for the current request */
  uint64_t give_up_at;
};

/** Structure that represents a single metric key.
 * It contains information about which backend issued the key id to allow for
 * subsequent writes
 */
struct tsmq_client_key {
#if 0
  /** Backend server ID */
  zmq_msg_t server_id;
#endif
  /** Backend key ID
   * FYI: for dbats this will be a uint64, for 'ascii' this will be a copy
   * of the string key
   * @note this is only unique in combination with the server id
   */
  uint8_t *server_key_id;

  /** Length of the backend key ID */
  uint16_t server_key_id_len;
};

#endif /* __TSMQ_CLIENT_INT_H */






