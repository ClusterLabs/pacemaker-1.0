#
# heartbeat: Linux-HA telecom code: event daemon
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
			-I$(top_builddir)/heartbeat \
			-I$(top_srcdir)/heartbeat \
			-I$(top_builddir)/include/clplumbing
			-I$(top_srcdir)/include/clplumbing

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
evtvarlibdir		= $(havarlibdir)/evt
havarrundir		= $(localstatedir)/run/

AM_CFLAGS		= @CFLAGS@ \
			-DEVTVARLIBDIR='"$(evtvarlibdir)"'

noinst_HEADERS		= 

## binary progs
halib_PROGRAMS		= eventd event_testclient

#eventd
eventd_SOURCES	= eventd.c event.h 

eventd_CFLAGS	= $(AM_CFLAGS)
eventd_LDADD	= $(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/heartbeat/libhbclient.la \
			$(GLIBLIB)
eventd_LDFLAGS	= 


#event_testclient
event_testclient_SOURCES	= event_testclient.c event.h 
event_testclient_LDADD	= $(GLIBLIB) \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/eventd/libevent.la 
event_testclient_LDFLAGS	= 


install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(evtvarlibdir)
	chmod 770 $(DESTDIR)/$(evtvarlibdir)
	-test -p $(DESTDIR)$(evtvarlibdir) || mkdir -p $(DESTDIR)$(evtvarlibdir)

uninstall-local:
	-rm -fr $(DESTDIR)$(evtvarlibdir)

