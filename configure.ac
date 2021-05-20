#
# libtimeseries
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

AC_PREREQ([2.68])

AC_INIT([libtimeseries], [1.0.3], [corsaro-info@caida.org])
AM_INIT_AUTOMAKE([foreign])

LIBTIMESERIES_MAJOR_VERSION=1
LIBTIMESERIES_MID_VERSION=0
LIBTIMESERIES_MINOR_VERSION=3

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
AC_CHECK_LIB([wandio], [wandio_fgets], ,[AC_MSG_ERROR(
                [libwandio (>=4.1.0) required])])

AC_SEARCH_LIBS([yaml_parser_initialize], [yaml], ,[AC_MSG_ERROR(
		[libyaml required]
		)])

# shall we build with the dbats backend?
# -- installing DBATS is not trivial, so we don't want to make it required
AC_MSG_CHECKING([whether to build the DBATS backend])
AC_ARG_WITH([dbats],
	[AS_HELP_STRING([--without-dbats],
	  [do not compile the DBATS backend])],
	  [],
	  [with_dbats=no])
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

# shall we build with support for kafka?
AC_MSG_CHECKING([whether to build kafka backend])
AC_ARG_WITH([kafka],
	[AS_HELP_STRING([--without-kafka],
	  [do not compile the kafka backend])],
	  [],
	  [with_kafka=yes])
AC_MSG_RESULT([$with_kafka])

AM_CONDITIONAL([WITH_KAFKA], [test "x$with_kafka" != xno])

if test x"$with_kafka" = xyes; then
AC_CHECK_LIB([rdkafka], [rd_kafka_query_watermark_offsets], ,
               [AC_MSG_ERROR(
                 [librdkafka is required for kafka (--without-kafka to disable)])])

AC_DEFINE([WITH_KAFKA],[1],[Building kafka backend])
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
		common/libpatricia/Makefile
		common/libinterval3/Makefile
		common/libinterval3/rb_tree/Makefile
		common/libcsv/Makefile
                common/libjsmn/Makefile
		lib/Makefile
		lib/backends/Makefile
		tools/Makefile
		])
AC_OUTPUT
