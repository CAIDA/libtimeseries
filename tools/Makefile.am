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

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common -I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/backends

dist_bin_SCRIPTS =

bin_PROGRAMS = 

if WITH_TSMQ
bin_PROGRAMS += tsmq-md-server

tsmq_md_server_SOURCES = \
	tsmq-md-server.c
tsmq_md_server_LDADD = -ltimeseries
tsmq_md_server_LDFLAGS = -L$(top_builddir)/lib
endif

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
