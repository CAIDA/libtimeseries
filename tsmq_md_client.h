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

#ifndef __TSMQ_MD_CLIENT_H
#define __TSMQ_MD_CLIENT_H

#include "tsmq_int.h"

#include <czmq.h>

/** @file
 *
 * @brief Header file that contains the private components of the tsmq metadata
 * client.
 *
 * @author Alistair King
 *
 */

struct tsmq_md_client {
  /** Common tsmq state */
  tsmq_t *tsmq;

  /** URI to connect to the broker on */
  char *broker_uri;

  /** Socket used to connect to the broker */
  void *broker_socket;

  /** Request sequence number */
  uint64_t sequence_num;

  /** Request timeout in msec */
  uint64_t request_timeout;

  /** Request retries */
  int request_retries;
};

/** Structure that represents a single metric key.
 * It contains information about which backend issued the key id to allow for
 * subsequent writes
 */
struct tsmq_md_client_key {
  /** Byte array of the key 'name' */
  uint8_t *key;
  /** Length of the key array */
  size_t key_len;

  /** Backend server ID */
  uint8_t *server_id;
  /** Length of the server id */
  size_t server_id_len;

  /** Backend key ID
   * FYI: for dbats this will be a uint64, for 'ascii' this will be a copy
   * of the string key
   * @note this is only unique in combination with the server id
   */
  uint8_t *server_key_id;
  /** Length of the backend key id */
  size_t server_key_id_len;
};

#endif /* __TSMQ_MD_CLIENT_H */






