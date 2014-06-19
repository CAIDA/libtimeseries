#
# tsmq
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# This file is part of tsmq.
#
# tsmq is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# tsmq is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with tsmq.  If not, see <http://www.gnu.org/licenses/>.
#

SUBDIRS =
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common

lib_LTLIBRARIES = libtsmq.la

include_HEADERS = tsmq.h

libtsmq_la_SOURCES = 		\
	tsmq.c 			\
	tsmq.h			\
	tsmq_int.h

libtsmq_la_LIBADD = $(top_builddir)/common/libcccommon.la

libtsmq_la_LDFLAGS = -version-info @TSMQ_MAJOR_VERSION@:@TSMQ_MID_VERSION@:@TSMQ_MINOR_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
