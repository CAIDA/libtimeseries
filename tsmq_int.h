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

/* always include our public header so that internal users don't need to */
#include "tsmq.h"

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

struct tsmq_md_server {
  /** Common tsmq state */
  tsmq_t *tsmq;

  /** URI to connect to broker on */
  char *uri;
};

struct tsmq_md_broker {
  /** Common tsmq state */
  tsmq_t *tsmq;

  /** URI to listen for clients on */
  char *client_uri;

  /** URI to listen for servers on */
  char *server_uri;
};

struct tsmq_md_client {
  /** Common tsmq state */
  tsmq_t *tsmq;
};

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

/** @} */

#endif /* __TSMQ_INT_H */
