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
AM_CPPFLAGS = 	-I$(top_srcdir) 	\
		-I$(top_srcdir)/common 	\
		-I$(top_srcdir)/lib

noinst_LTLIBRARIES = libtsmq.la

include_HEADERS = 		\
	tsmq_common.h		\
	tsmq_broker.h		\
	tsmq_client.h		\
	tsmq_server.h

libtsmq_la_SOURCES = 		\
	tsmq_int.h		\
	tsmq.c			\
	tsmq_common.h		\
	tsmq_server.c	\
	tsmq_server.h	\
	tsmq_server_int.h	\
	tsmq_broker.c	\
	tsmq_broker.h	\
	tsmq_broker_int.h	\
	tsmq_client.c	\
	tsmq_client.h	\
	tsmq_client_int.h

libtsmq_la_LIBADD =

libtsmq_la_LDFLAGS =

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
