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

AM_CPPFLAGS = 	-I$(top_srcdir) 	\
		-I$(top_srcdir)/common 	\
		-I$(top_srcdir)/lib 	\
		-I$(top_srcdir)/lib/backends

dist_bin_SCRIPTS =

bin_PROGRAMS = timeseries-insert
if WITH_KAFKA
bin_PROGRAMS += tsk-proxy
endif

tsk_proxy_SOURCES = \
	tsk-proxy.c
tsk_proxy_LDADD = -ltimeseries
tsk_proxy_LDFLAGS = -L$(top_builddir)/lib

timeseries_insert_SOURCES = \
	timeseries-insert.c
timeseries_insert_LDADD = -ltimeseries
timeseries_insert_LDFLAGS = -L$(top_builddir)/lib

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
