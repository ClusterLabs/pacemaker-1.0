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
halib_PROGRAMS		=fun1node1 fun1node2 fun2node1 fun2node2 fun3node1 fun3node2 fun4node1 fun4node2 fun5node1 fun5node2 \
					fun6node1 fun6node2 fun7node1 fun7node2 fun8node1 fun8node2 fun9node1 fun9node2  fun10node1 fun10node2\
					fun11node1 fun11node2 fun12node1 fun12node2 fun13node1 fun13node2 fun14node1 fun14node2\
					pf1node1 pf1node2 

fun1node1_SOURCES	= fun1node1.c ./../../event.h 
fun1node1_CFLAGS	= $(AM_CFLAGS)
fun1node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun1node1_LDFLAGS	= 

fun1node2_SOURCES	= fun1node2.c ./../../event.h 

fun1node2_CFLAGS	= $(AM_CFLAGS)
fun1node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun1node2_LDFLAGS	= 


fun2node1_SOURCES	= fun2node1.c ./../../event.h 
fun2node1_CFLAGS	= $(AM_CFLAGS)
fun2node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun2node1_LDFLAGS	= 

fun2node2_SOURCES	= fun2node2.c ./../../event.h 
fun2node2_CFLAGS	= $(AM_CFLAGS)
fun2node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun2node2_LDFLAGS	= 



fun3node1_SOURCES	= fun3node1.c ./../../event.h 
fun3node1_CFLAGS	= $(AM_CFLAGS)
fun3node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun3node1_LDFLAGS	= 

fun3node2_SOURCES	= fun3node2.c ./../../event.h 
fun3node2_CFLAGS	= $(AM_CFLAGS)
fun3node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun3node2_LDFLAGS	= 




fun4node1_SOURCES	= fun4node1.c ./../../event.h 
fun4node1_CFLAGS	= $(AM_CFLAGS)
fun4node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun4node1_LDFLAGS	= 

fun4node2_SOURCES	= fun4node2.c ./../../event.h 
fun4node2_CFLAGS	= $(AM_CFLAGS)
fun4node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun4node2_LDFLAGS	= 


fun5node1_SOURCES	= fun5node1.c ./../../event.h 
fun5node1_CFLAGS	= $(AM_CFLAGS)
fun5node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun5node1_LDFLAGS	= 

fun5node2_SOURCES	= fun5node2.c ./../../event.h 
fun5node2_CFLAGS	= $(AM_CFLAGS)
fun5node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun5node2_LDFLAGS	= 


fun6node1_SOURCES	= fun6node1.c ./../../event.h 
fun6node1_CFLAGS	= $(AM_CFLAGS)
fun6node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun6node1_LDFLAGS	= 

fun6node2_SOURCES	= fun6node2.c ./../../event.h 
fun6node2_CFLAGS	= $(AM_CFLAGS)
fun6node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun6node2_LDFLAGS	= 


fun7node1_SOURCES	= fun7node1.c ./../../event.h 
fun7node1_CFLAGS	= $(AM_CFLAGS)
fun7node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun7node1_LDFLAGS	= 

fun7node2_SOURCES	= fun7node2.c ./../../event.h 
fun7node2_CFLAGS	= $(AM_CFLAGS)
fun7node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun7node2_LDFLAGS	= 


fun8node1_SOURCES	= fun8node1.c ./../../event.h 
fun8node1_CFLAGS	= $(AM_CFLAGS)
fun8node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun8node1_LDFLAGS	= 

fun8node2_SOURCES	= fun8node2.c ./../../event.h 
fun8node2_CFLAGS	= $(AM_CFLAGS)
fun8node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun8node2_LDFLAGS	= 

fun9node1_SOURCES	= fun9node1.c ./../../event.h 
fun9node1_CFLAGS	= $(AM_CFLAGS)
fun9node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun9node1_LDFLAGS	= 

fun9node2_SOURCES	= fun9node2.c ./../../event.h 
fun9node2_CFLAGS	= $(AM_CFLAGS)
fun9node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun9node2_LDFLAGS	= 


fun10node1_SOURCES	= fun10node1.c ./../../event.h 
fun10node1_CFLAGS	= $(AM_CFLAGS)
fun10node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun10node1_LDFLAGS	= 

fun10node2_SOURCES	= fun10node2.c ./../../event.h 
fun10node2_CFLAGS	= $(AM_CFLAGS)
fun10node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun10node2_LDFLAGS	= 


fun11node1_SOURCES	= fun11node1.c ./../../event.h 
fun11node1_CFLAGS	= $(AM_CFLAGS)
fun11node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun11node1_LDFLAGS	= 

fun11node2_SOURCES	= fun11node2.c ./../../event.h 
fun11node2_CFLAGS	= $(AM_CFLAGS)
fun11node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun11node2_LDFLAGS	= 



fun12node1_SOURCES	= fun12node1.c ./../../event.h 
fun12node1_CFLAGS	= $(AM_CFLAGS)
fun12node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun12node1_LDFLAGS	= 

fun12node2_SOURCES	= fun12node2.c ./../../event.h 
fun12node2_CFLAGS	= $(AM_CFLAGS)
fun12node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun12node2_LDFLAGS	= 



fun13node1_SOURCES	= fun13node1.c ./../../event.h 
fun13node1_CFLAGS	= $(AM_CFLAGS)
fun13node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun13node1_LDFLAGS	= 

fun13node2_SOURCES	= fun13node2.c ./../../event.h 
fun13node2_CFLAGS	= $(AM_CFLAGS)
fun13node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun13node2_LDFLAGS	= 



fun14node1_SOURCES	= fun14node1.c ./../../event.h 
fun14node1_CFLAGS	= $(AM_CFLAGS)
fun14node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun14node1_LDFLAGS	= 

fun14node2_SOURCES	= fun14node2.c ./../../event.h 
fun14node2_CFLAGS	= $(AM_CFLAGS)
fun14node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
fun14node2_LDFLAGS	= 


pf1node1_SOURCES	= pf1node1.c ./../../event.h 
pf1node1_CFLAGS	= $(AM_CFLAGS)
pf1node1_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
pf1node1_LDFLAGS	= 

pf1node2_SOURCES	= pf1node2.c ./../../event.h 
pf1node2_CFLAGS	= $(AM_CFLAGS)
pf1node2_LDADD	= \
		$(top_builddir)/lib/clplumbing/libplumb.la \
		$(top_builddir)/lib/hbclient/libhbclient.la \
		$(top_builddir)/lib/eventd/libevent.la	\
		$(GLIBLIB)
pf1node2_LDFLAGS	= 

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(evtvarlibdir)
	chmod 770 $(DESTDIR)/$(evtvarlibdir)
	-test -p $(DESTDIR)$(evtvarlibdir) || mkdir -p $(DESTDIR)$(evtvarlibdir)

uninstall-local:
	-rm -fr $(DESTDIR)$(evtvarlibdir)


