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

#ifndef __TSMQ_COMMON_H
#define __TSMQ_COMMON_H

#include <stdint.h>

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

/** Default the broker/server heartbeat interval to 1 second */
#define TSMQ_HEARTBEAT_INTERVAL_DEFAULT 1000

/** Default the broker/server heartbeat liveness to 3 beats */
#define TSMQ_HEARTBEAT_LIVENESS_DEFAULT 3

/** Default the reconnect minimum interval to 1 second */
#define TSMQ_RECONNECT_INTERVAL_MIN 1000

/** Default the reconnect maximum interval to 32 seconds */
#define TSMQ_RECONNECT_INTERVAL_MAX 32000

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Type of a time value */
typedef uint32_t tsmq_time_t;

/** Type of a TS value */
typedef uint64_t tsmq_val_t;

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

  /** callback error */
  TSMQ_ERR_CALLBACK     = -7,

} tsmq_err_code_t;

/** @} */

#endif /* __TSMQ_COMMON_H */
