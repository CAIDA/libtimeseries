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

# Kafka Backend
if WITH_KAFKA
BACKEND_SRCS += \
	timeseries_backend_kafka.c \
	timeseries_backend_kafka.h
endif

libtimeseries_backends_la_SOURCES = $(BACKEND_SRCS)

libtimeseries_backends_la_LIBADD = $(BACKEND_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
