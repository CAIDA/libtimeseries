/*
 * libtimeseries
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of libtimeseries.
 *
 * libtimeseries is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libtimeseries is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtimeseries.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __TIMESERIES_LOG_INT_H
#define __TIMESERIES_LOG_INT_H

/** @file
 *
 * @brief Header file that contains the protected interface to the timeseries
 * log class
 *
 *
 * @author Alistair King
 *
 */

/**
 * @name Logging functions
 *
 * Collection of convenience functions that allow libtimeseries to log events
 * For now we just log to stderr, but this should be extended in future.
 *
 * @todo find (or write) good C logging library (that can also log to syslog)
 *
 * @{ */

void timeseries_log(const char *func, const char *format, ...);

/** @} */

#endif /* __TIMESERIES_LOG_INT_H */
