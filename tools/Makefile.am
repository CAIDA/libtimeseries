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

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common -I$(top_srcdir)/lib

dist_bin_SCRIPTS =

bin_PROGRAMS = tsmq-md-broker tsmq-md-client

tsmq_md_broker_SOURCES = \
	tsmq-md-broker.c
tsmq_md_broker_LDADD = -ltsmq
tsmq_md_broker_LDFLAGS = -L$(top_builddir)/lib

tsmq_md_client_SOURCES = \
	tsmq-md-client.c
tsmq_md_client_LDADD = -ltsmq
tsmq_md_client_LDFLAGS = -L$(top_builddir)/lib

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
