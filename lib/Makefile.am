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

SUBDIRS = backends

AM_CPPFLAGS = 	-I$(top_srcdir) 	\
		-I$(top_srcdir)/common 	\
		-I$(top_srcdir)/lib/backends

lib_LTLIBRARIES = libtimeseries.la

include_HEADERS = 	timeseries.h			\
			timeseries_pub.h		\
			timeseries_backend_pub.h	\
			timeseries_kp_pub.h

libtimeseries_la_SOURCES = 		\
	timeseries.h			\
					\
	timeseries_pub.h		\
	timeseries_int.h		\
	timeseries.c	 		\
					\
	timeseries_log_int.h		\
	timeseries_log.c		\
					\
	timeseries_backend_pub.h	\
	timeseries_backend_int.h	\
	timeseries_backend.c		\
					\
	timeseries_kp_pub.h		\
	timeseries_kp_int.h		\
	timeseries_kp.c

libtimeseries_la_LIBADD = 			\
	$(top_builddir)/common/libcccommon.la 	\
	$(top_builddir)/lib/backends/libtimeseries_backends.la

libtimeseries_la_LDFLAGS = -version-info @LIBTIMESERIES_MAJOR_VERSION@:@LIBTIMESERIES_MID_VERSION@:@LIBTIMESERIES_MINOR_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
