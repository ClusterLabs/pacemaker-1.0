#
# heartbeat: Linux-HA telecom code: data checkpoint daemon
#
# Copyright (C) 2002 Alan Robertson
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
			-I$(top_builddir)/include/clplumbing
			-I$(top_srcdir)/include/clplumbing

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
ckptvarlibdir		= $(havarlibdir)/ckpt
havarrundir		= $(localstatedir)/run/

## AM_CFLAGS		= @CFLAGS@ -fprofile-arcs -ftest-coverage 
AM_CFLAGS		= @CFLAGS@ \
			-DCKPTVARLIBDIR='"$(ckptvarlibdir)"'

noinst_HEADERS		= checkpointd.h message.h request.h replica.h \
		  	utils.h client.h response.h operation.h

## binary progs
halib_PROGRAMS		= checkpointd ckpttest 

#checkpointd
checkpointd_SOURCES	= checkpointd.c message.c request.c replica.c \
			utils.c client.c response.c operation.c
checkpointd_CFLAGS	= $(AM_CFLAGS)
checkpointd_LDADD	= $(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/hbclient/libhbclient.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(GLIBLIB)
checkpointd_LDFLAGS	= 

#ckpttest
ckpttest_SOURCES	= ckpttest.c
ckpttest_LDADD		= $(GLIBLIB) \
	 		$(top_builddir)/lib/checkpointd/libckpt.la \
			$(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/hbclient/libhbclient.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(GLIBLIB)
ckpttest_LDFLAGS	= 

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)/$(ckptvarlibdir)
	chmod 770 $(DESTDIR)/$(ckptvarlibdir)
	-test -p $(DESTDIR)/$(ckptvarlibdir) || mkdir -p $(DESTDIR)/$(ckptvarlibdir)

uninstall-local:
	-rm -fr $(DESTDIR)/$(ckptvarlibdir)

