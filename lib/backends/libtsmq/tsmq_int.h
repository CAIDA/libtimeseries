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

#ifndef __TSMQ_INT_H
#define __TSMQ_INT_H

#include <czmq.h>

#include "klist.h"

#include "tsmq_common.h"

/** @file
 *
 * @brief Header file that contains the private components of tsmq.
 *
 * @author Alistair King
 *
 */

/**
 * @name Internal Macros
 *
 */

#define TSMQ_GET_ERR(type)				\
  tsmq_err_t tsmq_##type##_get_err(tsmq_##type##_t *t)	\
  {							\
   return tsmq_get_err(t->tsmq);			\
  }

#define TSMQ_IS_ERR(type)				\
  int tsmq_##type##_is_err(tsmq_##type##_t *t)		\
  {							\
    return tsmq_is_err(t->tsmq);			\
  }

#define TSMQ_PERR(type)					\
  void tsmq_##type##_perr(tsmq_##type##_t *t)		\
  {							\
    return tsmq_perr(t->tsmq);				\
  }

#define TSMQ_ERR_FUNCS(type)			\
  TSMQ_GET_ERR(type)				\
  TSMQ_IS_ERR(type)				\
  TSMQ_PERR(type)

/** @} */

/**
 * @name Internal Enums
 *
 * @{ */

/** Enumeration of tsmq message types
 *
 * @note these will be cast to a uint8_t, so be sure that there are fewer than
 * 2^8 values
 *
 * @todo ensure that these names still make sense when we implement the ts
 * classes
 */
typedef enum {
  /** Invalid message */
  TSMQ_MSG_TYPE_UNKNOWN   = 0,

  /** Server is ready to do work */
  TSMQ_MSG_TYPE_READY     = 1,

  /** Server is still alive */
  TSMQ_MSG_TYPE_HEARTBEAT = 2,

  /** A request for a server to process */
  TSMQ_MSG_TYPE_REQUEST   = 3,

  /** Server is sending a response to a client */
  TSMQ_MSG_TYPE_REPLY     = 4,

  /** Highest message number in use */
  TSMQ_MSG_TYPE_MAX      = TSMQ_MSG_TYPE_REPLY,
} tsmq_msg_type_t;

#define tsmq_msg_type_size_t sizeof(uint8_t)

typedef enum {
  /** Invalid request type */
  TSMQ_REQUEST_MSG_TYPE_UNKNOWN         = 0,

  /** Request ACK type (only sent *to* clients) */
  TSMQ_REQUEST_MSG_TYPE_ACK             = 1,

  /** Key lookup request */
  TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP      = 2,

  /** Bulk Key lookup request */
  TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP_BULK = 3,

  /** Key storage request */
  TSMQ_REQUEST_MSG_TYPE_KEY_SET_SINGLE  = 4,

  /** Bulk Key storage request */
  TSMQ_REQUEST_MSG_TYPE_KEY_SET_BULK    = 5,

  /** Highest request type in use */
  TSMQ_REQUEST_MSG_TYPE_MAX = TSMQ_REQUEST_MSG_TYPE_KEY_SET_BULK,
} tsmq_request_msg_type_t;

#define tsmq_request_msg_type_size_t sizeof(uint8_t)

/** @} */

/**
 * @name Internal Datastructures
 *
 * These datastructures are internal to tsmq. Some may be exposed as opaque
 * structures by tsmq.h
 *
 * @{ */

/** State common to all tsmq subclasses */
typedef struct tsmq {

  /** 0MQ context pointer */
  zctx_t *ctx;

  /** Error information */
  struct tsmq_err err;

} tsmq_t;

/** @} */

/**
 * @name Internal API
 *
 * These functions are only exposed to libtsmq code
 */

/** Initialize a new tsmq instance
 *
 * @return pointer to an initialized tsmq object if successful, NULL otherwise
 */
tsmq_t *tsmq_init();

/** Start a tsmq instance
 *
 * @param tsmq          pointer to tsmq instance to start
 * @return 0 if successful, -1 otherwise
 */
int tsmq_start(tsmq_t *tsmq);

/** Free a tsmq instance
 *
 * @param tsmq          pointer to a tsmq instance to free
 */
void tsmq_free(tsmq_t *tsmq);

/** Sets the error status for a tsmq subclass
 *
 * @param errcode       either an Econstant from libc, or a TSMQ_ERR
 * @param msg           a plaintext error message
 */
void tsmq_set_err(tsmq_t *tsmq, int errcode, const char *msg, ...);

/** Gets the errno for a tsmq subclass and clears the error state
 *
 * @param tsmq          pointer to tsmq object to get the error for
 * @return tsmq error structure representing the error (may be no error)
 */
tsmq_err_t tsmq_get_err(tsmq_t *tsmq);

/** Checks if an error has occurred
 *
 * @param tsmq          pointer to tsmq object to check for an error
 * @return 1 if an error has occurred, 0 otherwise
 */
int tsmq_is_err(tsmq_t *tsmq);

/** Prints the error status to standard error and clears the error state
 *
 * @param tsmq          pointer to tsmq object to print error for
 */
void tsmq_perr(tsmq_t *tsmq);

/** Receives a single message and decodes as a message type
 *
 * @param src           socket to receive message on
 * @return the type of the message, or TSMQ_MSG_TYPE_UNKNOWN if an error occurred
 */
tsmq_msg_type_t tsmq_recv_type(void *src);

/** Decodes the request type for the given message
 *
 * @param msg           zmsg object to inspect
 * @return the type of the message, or TSMQ_MSG_TYPE_UNKNOWN if an error occurred
 *
 * This function will pop the type frame from the beginning of the message
 */
tsmq_request_msg_type_t tsmq_request_msg_type(zmsg_t *msg);

/** Receives a single message and decodes as a request message type
 *
 * @param src           socket to receive message on
 * @return the type of the message, or TSMQ_REQUEST_MSG_TYPE_UNKNOWN if an
 *         error occurred
 */
tsmq_request_msg_type_t tsmq_recv_request_type(void *src);

/** Receive a string from the given socket
 *
 * @param src           socket to recieve on
 * @return pointer to a string if successful, NULL otherwise
 */
char *tsmq_recv_str(void *src);

/** @} */

#endif /* __TSMQ_INT_H */
