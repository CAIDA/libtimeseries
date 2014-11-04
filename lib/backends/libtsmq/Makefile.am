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
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common

noinst_LTLIBRARIES = libtsmq.la

include_HEADERS = 		\
	tsmq.h 			\
	tsmq_common.h		\
	tsmq_md_server.h

libtsmq_la_SOURCES = 		\
	tsmq_int.h		\
	tsmq.h			\
	tsmq.c			\
	tsmq_common.h		\
	tsmq_md_server.c	\
	tsmq_md_server.h	\
	tsmq_md_server_int.h	\
	tsmq_md_broker.c	\
	tsmq_md_broker.h	\
	tsmq_md_broker_int.h	\
	tsmq_md_client.c	\
	tsmq_md_client.h	\
	tsmq_md_client_int.h

libtsmq_la_LIBADD = 

libtsmq_la_LDFLAGS = -version-info @TSMQ_MAJOR_VERSION@:@TSMQ_MID_VERSION@:@TSMQ_MINOR_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
