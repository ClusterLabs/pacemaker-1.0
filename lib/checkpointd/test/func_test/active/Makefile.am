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

EXTRA_DIST		= func.h

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
			-I$(top_builddir)/lib/checkpointd/test\
			-I$/..

hadir			= $(sysconfdir)/ha.d
halibdir			= $(libdir)/@HB_PKG@
commmoddir	= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
ckptvarlibdir	= $(havarlibdir)/ckpt
havarrundir		= $(localstatedir)/run/

test_linkfiles	= $(GLIBLIB) \
	 		$(top_builddir)/lib/checkpointd/libckpt.la \
			$(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la 

## binary progs
noinst_PROGRAMS	= active1node1 active1node2 active2node1 active2node2 active3node1 active3node2 active4node1 active4node2
halib_PROGRAMS		= active1node1 active1node2 active2node1 active2node2 active3node1 active3node2 active4node1 active4node2

active1node1_SOURCES	= active1node1.c
active1node1_LDADD	= $(test_linkfiles) 
active1node1_LDFLAGS	= 

active1node2_SOURCES	= active1node2.c
active1node2_LDADD	= $(test_linkfiles) 
active1node2_LDFLAGS		= 

active2node1_SOURCES	= active2node1.c
active2node1_LDADD	= $(test_linkfiles) 
active2node1_LDFLAGS	= 

active2node2_SOURCES	= active2node2.c
active2node2_LDADD	= $(test_linkfiles) 
active2node2_LDFLAGS		= 

active3node1_SOURCES	= active3node1.c
active3node1_LDADD	= $(test_linkfiles) 
active3node1_LDFLAGS	= 

active3node2_SOURCES	= active3node2.c
active3node2_LDADD	= $(test_linkfiles) 
active3node2_LDFLAGS	= 


active4node1_SOURCES	= active4node1.c
active4node1_LDADD	= $(test_linkfiles) 
active4node1_LDFLAGS	= 

active4node2_SOURCES	= active4node2.c
active4node2_LDADD	= $(test_linkfiles) 
active4node2_LDFLAGS	= 


