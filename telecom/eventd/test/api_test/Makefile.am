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
halib_PROGRAMS		= a b c d e f g h i j

a_SOURCES	= a.c ./../../event.h 

a_CFLAGS	= $(AM_CFLAGS)
a_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
a_LDFLAGS	= 

b_SOURCES	= b.c ./../../event.h 

b_CFLAGS	= $(AM_CFLAGS)
b_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
b_LDFLAGS	= 

c_SOURCES	= c.c ./../../event.h 

c_CFLAGS	= $(AM_CFLAGS)
c_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
c_LDFLAGS	= 

d_SOURCES	= d.c ./../../event.h 

d_CFLAGS	= $(AM_CFLAGS)
d_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
d_LDFLAGS	= 

e_SOURCES	= e.c ./../../event.h 

e_CFLAGS	= $(AM_CFLAGS)
e_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
e_LDFLAGS	= 

f_SOURCES	= f.c ./../../event.h 

f_CFLAGS	= $(AM_CFLAGS)
f_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
f_LDFLAGS	= 

g_SOURCES	= g.c ./../../event.h 

g_CFLAGS	= $(AM_CFLAGS)
g_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
g_LDFLAGS	= 

h_SOURCES	= h.c 

h_CFLAGS	= $(AM_CFLAGS)

h_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
h_LDFLAGS	= 

i_SOURCES	= i.c ./../../event.h 

i_CFLAGS	= $(AM_CFLAGS)
i_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
i_LDFLAGS	= 

j_SOURCES	= j.c ./../../event.h 

j_CFLAGS	= $(AM_CFLAGS)
j_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
j_LDFLAGS	= 


install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(evtvarlibdir)
	chmod 770 $(DESTDIR)/$(evtvarlibdir)
	-test -p $(DESTDIR)$(evtvarlibdir) || mkdir -p $(DESTDIR)$(evtvarlibdir)

uninstall-local:
	-rm -fr $(DESTDIR)$(evtvarlibdir)

