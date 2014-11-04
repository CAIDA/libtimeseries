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

/** @file
 *
 * @brief Header file that exposes the public interface of tsmq.
 *
 * @author Alistair King
 *
 */

/** Shared header */
#include "tsmq_common.h"

/** Component-specific includes */
#include "tsmq_md_server.h"
/** @todo consider splitting these into separate public headers */
#include "tsmq_md_client.h"
#include "tsmq_md_broker.h"

#endif /* __TSMQ_H */
