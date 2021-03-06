#
# Copyright (C) 2004 Andrew Beekhof
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
MAINTAINERCLEANFILES    = Makefile.in

INCLUDES        = -I$(top_builddir)/include -I$(top_srcdir)/include	\
		-I$(top_builddir)           -I$(top_srcdir)		\
		-I$(top_builddir)/libltdl   -I$(top_srcdir)/libltdl

halibdir	= $(CRM_DAEMON_DIR)

PE_TESTS	= $(wildcard test10/*.xml)

testdir			= $(datadir)/$(PACKAGE)/tests/pengine
test_SCRIPTS		= regression.sh
test_DATA		= regression.core.sh

test10dir		= $(datadir)/$(PACKAGE)/tests/pengine/test10
test10_DATA		= $(PE_TESTS) $(PE_TESTS:%.xml=%.dot) $(PE_TESTS:%.xml=%.exp) $(PE_TESTS:%.xml=%.scores)

COMMONLIBS	= 					\
		$(top_builddir)/lib/common/libcrmcommon.la		\
		$(top_builddir)/lib/pengine/libpe_status.la		\
		libpengine.la $(CURSESLIBS) $(CLUSTERLIBS) 

## libraries
lib_LTLIBRARIES	= libpengine.la

## binary progs
halib_PROGRAMS	= pengine
sbin_PROGRAMS	= ptest

if BUILD_HELP
man8_MANS =	ptest.8
%.8:	%
	help2man --output $@ --no-info --section 8 --name "Part of the Pacemaker cluster resource manager" $(top_builddir)/pengine/$<
endif

## SOURCES
noinst_HEADERS	= allocate.h utils.h pengine.h
#utils.h pengine.h

libpengine_la_LDFLAGS	= -version-info 3:0:0
# -L$(top_builddir)/lib/pils -lpils -export-dynamic -module -avoid-version 
libpengine_la_SOURCES	= pengine.c allocate.c utils.c constraints.c \
			native.c group.c clone.c master.c graph.c
libpengine_la_LIBADD    = $(top_builddir)/lib/pengine/libpe_status.la

pengine_SOURCES	= main.c
pengine_LDADD	= $(COMMONLIBS)	$(top_builddir)/lib/cib/libcib.la
# libcib for get_object_root()
#		$(top_builddir)/lib/hbclient/libhbclient.la

ptest_SOURCES	= ptest.c 
ptest_LDADD	= $(COMMONLIBS)						\
		$(top_builddir)/lib/cib/libcib.la			\
		$(top_builddir)/lib/transition/libtransitioner.la

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)/$(PE_STATE_DIR)
	-chown $(CRM_DAEMON_USER) $(DESTDIR)/$(PE_STATE_DIR)
	-chgrp $(CRM_DAEMON_GROUP) $(DESTDIR)/$(PE_STATE_DIR)
	-chmod 750 $(DESTDIR)/$(PE_STATE_DIR)

uninstall-local:
