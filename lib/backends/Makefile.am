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

SUBDIRS =

AM_CPPFLAGS = 	-I$(top_srcdir)		\
		-I$(top_srcdir)/common 	\
		-I$(top_srcdir)/lib

noinst_LTLIBRARIES = libtimeseries_backends.la

include_HEADERS =

BACKEND_SRCS=
BACKEND_LIBS=

# ASCII Backend
BACKEND_SRCS += \
	timeseries_backend_ascii.c \
	timeseries_backend_ascii.h

# DBATS Backend
if WITH_DBATS
BACKEND_SRCS += \
	timeseries_backend_dbats.c \
	timeseries_backend_dbats.h
endif

# TSMQ Backend
if WITH_TSMQ
SUBDIRS += libtsmq
AM_CPPFLAGS += -I$(top_srcdir)/lib/backends/libtsmq
BACKEND_SRCS += \
	timeseries_backend_tsmq.c \
	timeseries_backend_tsmq.h
BACKEND_LIBS += $(top_builddir)/lib/backends/libtsmq/libtsmq.la
endif

libtimeseries_backends_la_SOURCES = $(BACKEND_SRCS)

libtimeseries_backends_la_LIBADD = $(BACKEND_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
