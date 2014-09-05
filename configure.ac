#
# libtimeseries
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# This file is part of libtimeseries.
#
# libtimeseries is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# libtimeseries is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with libtimeseries.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ([2.68])

AC_INIT([libtimeseries], [0.1.0], [corsaro-info@caida.org])

LIBTIMESERIES_MAJOR_VERSION=0
LIBTIMESERIES_MID_VERSION=1
LIBTIMESERIES_MINOR_VERSION=0

# since libtimeseries is only a library, the version numbers are only used to
# set the libtool library verson numbers. This means that the numbering is
# slightly different than semantic versioning. For more info, see
# https://www.sourceware.org/autobook/autobook/autobook_91.html

AC_DEFINE_UNQUOTED([LIBTIMESERIES_MAJOR_VERSION],$LIBTIMESERIES_MAJOR_VERSION,
	[libtimeseries major version])
AC_DEFINE_UNQUOTED([LIBTIMESERIES_MID_VERSION],$LIBTIMESERIES_MID_VERSION,
	[libtimeseries mid version])
AC_DEFINE_UNQUOTED([LIBTIMESERIES_MINOR_VERSION],$LIBTIMESERIES_MINOR_VERSION,
	[libtimeseries minor version])

LT_INIT

AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([lib/timeseries.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CXX
AM_PROG_LIBTOOL
AC_PROG_CC_C99

AC_SYS_LARGEFILE

AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif])

AC_CHECK_FUNCS([gettimeofday memset strdup strstr strsep vasprintf])

# should we dump debug output to stderr and not optmize the build?

AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [enable debug data generation (def=no)])],
    [debugit="$enableval"],
    [debugit=no])
AC_MSG_RESULT([$debugit])

if test x"$debugit" = x"yes"; then
    AC_DEFINE([DEBUG],[],[Debug Mode])
else
    AC_DEFINE([NDEBUG],[],[No-debug Mode])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

# Checks for libraries.
# we use libwandio for threaded IO
AC_SEARCH_LIBS([wandio_create], [wandio trace], ,[AC_MSG_ERROR(
		 [libwandio required (included with libtrace since 3.0.15)]
		 )])


# shall we build with the dbats backend?
# -- installing DBATS is not trivial, so we don't want to make it required
AC_MSG_CHECKING([whether to build the DBATS backend])
AC_ARG_WITH([dbats],
	[AS_HELP_STRING([--without-dbats],
	  [do not compile the DBATS backend])],
	  [],
	  [with_dbats=yes])
AC_MSG_RESULT([$with_dbats])

AM_CONDITIONAL([WITH_DBATS], [test "x$with_dbats" != xno])

if test x"$with_dbats" = xyes; then
AC_CHECK_LIB([dbats], [dbats_open], ,[AC_MSG_ERROR(
		[libdbats is required by DBATS (--without-dbats to disable)]
		)])
AC_CHECK_DECLS([DB_LOCK_DEADLOCK], , AC_MSG_ERROR(
				   [db.h is missing DB_LOCK_DEADLOCK]
				   ), [[#include <db.h>]])

AC_DEFINE([WITH_DBATS],[1],[Building with DBATS backend])
fi

# shall we build with support for tsmq?
#  -- this will enable both the tsmq-*server tools and the tsmq backend
AC_MSG_CHECKING([whether to build with tsmq support])
AC_ARG_WITH([tsmq],
	[AS_HELP_STRING([--without-tsmq],
	  [do not compile the tsmq backend])],
	  [],
	  [with_tsmq=yes])
AC_MSG_RESULT([$with_tsmq])

if test x"$with_tsmq" = xyes; then
AC_CHECK_LIB([tsmq], [tsmq_md_server_init], ,[AC_MSG_ERROR(
		[libtsmq is required for tsmq support (--without-tsmq to disable)]
		)])

AC_DEFINE([WITH_TSMQ],[1],[Building with tsmq support])
fi

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h inttypes.h limits.h math.h stdlib.h string.h \
			      time.h sys/time.h])

# we may want to come back later and add compile-time configuration for things
# like timeseries backends, but for now it will all get compiled

AC_HEADER_ASSERT

AC_SUBST([LIBTIMESERIES_MAJOR_VERSION])
AC_SUBST([LIBTIMESERIES_MID_VERSION])
AC_SUBST([LIBTIMESERIES_MINOR_VERSION])

AC_HEADER_ASSERT

AC_CONFIG_FILES([Makefile
		common/Makefile
		common/libcsv/Makefile
		common/libpatricia/Makefile
		lib/Makefile
		lib/backends/Makefile
		tools/Makefile
		])
AC_OUTPUT
