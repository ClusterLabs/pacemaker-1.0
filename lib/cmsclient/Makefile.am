#
# Linux-HA telecom code: ais message service client library
#
# Copyright (C) 2004 Intel Corp.
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

INCLUDES                = -I$(top_builddir)/include \
			-I$(top_srcdir)/include \
			-I$(top_builddir)/libltdl \
			-I$(top_srcdir)/libltdl \
			-I$(top_builddir)/linux-ha \
			-I$(top_srcdir)/linux-ha \
			-I$(top_builddir)/heartbeat \
			-I$(top_srcdir)/heartbeat \
			-I$(top_builddir)/include/clplumbing \
			-I$(top_srcdir)/include/clplumbing \
			-I$(top_builddir)/telecom/cms \
			-I$(top_srcdir)/telecom/cms

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
cmsvarlibdir		= $(havarlibdir)/cms
havarrundir		= $(localstatedir)/run/

AM_CFLAGS		= @CFLAGS@ \
			-DCMSVARLIBDIR='"$(cmsvarlibdir)"'

noinst_HEADERS		= 

## libraries

lib_LTLIBRARIES		= libcms.la

## binary progs
halib_PROGRAMS		=

libcmsdir		= $(libdir)/@HB_PKG@
libcms_PROGRAMS		= 
libcms_la_SOURCES	= cmslib_client.c cmslib_client.h
libcms_la_CFLAGS	= -D$(DEBUG)_LIBRARY
libcms_la_LDFLAGS	=


install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(cmsvarlibdir)
	chmod 770 $(DESTDIR)/$(cmsvarlibdir)
	-test -p $(DESTDIR)$(cmsvarlibdir) || mkdir -p $(DESTDIR)$(cmsvarlibdir)

uninstall-local:
	-rm -fr $(DESTDIR)$(cmsvarlibdir)

