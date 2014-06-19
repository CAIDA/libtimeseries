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
 * @name Internal Datastructures
 *
 * These datastructures are internal to tsmq. Some may be exposed as opaque
 * structures by tsmq.h
 *
 * @{ */

struct tsmq {
  /* general tsmq state goes here */
};

/** @} */

#endif /* __TSMQ_INT_H */
