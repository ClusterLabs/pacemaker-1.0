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
			-I$(top_builddir)/include/clplumbing \
			-I$(top_srcdir)/include/clplumbing \
			-I$(top_srcdir)/lib/checkpointd/test \
			-I$(top_builddir)/lib/checkpointd/test

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
ckptvarlibdir		= $(havarlibdir)/ckpt
havarrundir		= $(localstatedir)/run/

test_LDADD		= $(GLIBLIB) \
	 		$(top_builddir)/lib/checkpointd/libckpt.la \
			$(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la 
## binary progs
noinst_PROGRAMS	= sync1node1 sync1node2 sync2node1 sync2node2 sync3node1 sync3node2  sync4node1 sync4node2 sync5node1 sync5node2
halib_PROGRAMS		= sync1node1 sync1node2  sync2node1 sync2node2 sync3node1 sync3node2   sync4node1 sync4node2 sync5node1 sync5node2

sync1node1_SOURCES	= sync1node1.c
sync1node1_LDADD	= $(test_LDADD) 
sync1node1_LDFLAGS	= 

sync1node2_SOURCES	= sync1node2.c
sync1node2_LDADD	= $(test_LDADD) 
sync1node2_LDFLAGS	=

sync2node1_SOURCES	= sync2node1.c
sync2node1_LDADD	= $(test_LDADD) 
sync2node1_LDFLAGS	= 

sync2node2_SOURCES	= sync2node2.c
sync2node2_LDADD	= $(test_LDADD) 
sync2node2_LDFLAGS	= 


sync3node1_SOURCES	= sync3node1.c
sync3node1_LDADD	= $(test_LDADD) 
sync3node1_LDFLAGS	= 

sync3node2_SOURCES	= sync3node2.c
sync3node2_LDADD	= $(test_LDADD) 
sync3node2_LDFLAGS	= 

sync4node1_SOURCES	= sync4node1.c
sync4node1_LDADD	= $(test_LDADD) 
sync4node1_LDFLAGS	= 

sync4node2_SOURCES	= sync4node2.c
sync4node2_LDADD	= $(test_LDADD) 
sync4node2_LDFLAGS	= 

sync5node1_SOURCES	= sync5node1.c
sync5node1_LDADD	= $(test_LDADD) 
sync5node1_LDFLAGS	= 

sync5node2_SOURCES	= sync5node2.c
sync5node2_LDADD	= $(test_LDADD) 
sync5node2_LDFLAGS	= 
