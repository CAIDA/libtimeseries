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

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct tsmq tsmq_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */



/** @} */

/**
 * @name Public Enums
 *
 * @{ */



/** @} */

/** Initialize a new instance of tsmq
 *
 * @return a pointer to a tsmq structure if successful, NULL if an error
 * occurred
 */
tsmq_t *tsmq_init();

/** Free a tsmq instance
 *
 * @param tsmq          pointer to a tsmq instance to free
 */
void tsmq_free(tsmq_t *tsmq);

#endif /* __TSMQ_H */
