#
# heartbeat: Linux-HA telecom code: AIS message service daemon
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

## Subdirectories...
SUBDIRS			= . utils test doc

INCLUDES                = -I$(top_builddir)/include \
			-I$(top_srcdir)/include \
			-I$(top_builddir)/libltdl \
			-I$(top_srcdir)/libltdl \
			-I$(top_builddir)/linux-ha \
			-I$(top_srcdir)/linux-ha \
			-I$(top_builddir)/heartbeat \
			-I$(top_srcdir)/heartbeat \
			-I$(top_builddir)/include/clplumbing
			-I$(top_srcdir)/include/clplumbing

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
cmsvarlibdir		= $(havarlibdir)/cms
havarrundir		= $(localstatedir)/run/

AM_CFLAGS		= @CFLAGS@ \
			-DCMSVARLIBDIR='"$(cmsvarlibdir)"'

noinst_HEADERS		= 

## binary progs
halib_PROGRAMS		= cms

#cms
cms_SOURCES	= cms_main.c cms_data.h cms_membership.c cms_membership.h \
			cms_cluster.h cms_cluster.c cms_client.h \
			cms_client.c cms_mqueue.c cms_mqueue.h \
			cms_entry.c cms_common.h cms_common.c \
			cms_client_types.h

cms_CFLAGS	= $(AM_CFLAGS)
cms_LDADD	= $(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la \
			$(top_builddir)/membership/ccm/libccmclient.la \
			$(top_builddir)/membership/ccm/libclm.la \
			$(GLIBLIB)
cms_LDFLAGS	=


install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(cmsvarlibdir)
	-chgrp @HA_APIGID@ $(DESTDIR)$(cmsvarlibdir)
	-chmod 770 $(DESTDIR)/$(cmsvarlibdir)
	-test -p $(DESTDIR)$(cmsvarlibdir) || mkdir -p $(DESTDIR)$(cmsvarlibdir)

uninstall-local:
	-rm -fr $(DESTDIR)$(cmsvarlibdir)

