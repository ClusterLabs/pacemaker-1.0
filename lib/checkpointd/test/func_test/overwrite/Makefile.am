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
			-I$(top_builddir)/lib/checkpointd/test

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
ckptvarlibdir		= $(havarlibdir)/ckpt
havarrundir		= $(localstatedir)/run/

test_linkfiles		= $(GLIBLIB) \
	 		$(top_builddir)/lib/checkpointd/libckpt.la \
			$(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la 
## binary progs 
noinst_PROGRAMS	= overwrite1node1 overwrite1node2 overwrite2node1 overwrite2node2 overwrite3node1 overwrite3node2  overwrite4node1 overwrite4node2 overwrite5node1 overwrite5node2 overwrite6node1 overwrite6node2

overwrite1node1_SOURCES	= overwrite1node1.c
overwrite1node1_LDADD	= $(test_linkfiles) 
overwrite1node1_LDFLAGS	= 

overwrite1node2_SOURCES	= overwrite1node2.c
overwrite1node2_LDADD	= $(test_linkfiles) 
overwrite1node2_LDFLAGS	= 

overwrite2node1_SOURCES	= overwrite2node1.c
overwrite2node1_LDADD	= $(test_linkfiles) 
overwrite2node1_LDFLAGS	= 

overwrite2node2_SOURCES	= overwrite2node2.c
overwrite2node2_LDADD	= $(test_linkfiles) 
overwrite2node2_LDFLAGS	= 


overwrite3node1_SOURCES	= overwrite3node1.c
overwrite3node1_LDADD	= $(test_linkfiles) 
overwrite3node1_LDFLAGS	= 

overwrite3node2_SOURCES	= overwrite3node2.c
overwrite3node2_LDADD	= $(test_linkfiles) 
overwrite3node2_LDFLAGS	= 


overwrite4node1_SOURCES	= overwrite4node1.c
overwrite4node1_LDADD	= $(test_linkfiles) 
overwrite4node1_LDFLAGS	= 

overwrite4node2_SOURCES	= overwrite4node2.c
overwrite4node2_LDADD	= $(test_linkfiles) 
overwrite4node2_LDFLAGS	= 


overwrite5node1_SOURCES	= overwrite5node1.c
overwrite5node1_LDADD	= $(test_linkfiles) 
overwrite5node1_LDFLAGS	= 

overwrite5node2_SOURCES	= overwrite5node2.c
overwrite5node2_LDADD	= $(test_linkfiles) 
overwrite5node2_LDFLAGS	= 


overwrite6node1_SOURCES	= overwrite6node1.c
overwrite6node1_LDADD	= $(test_linkfiles) 
overwrite6node1_LDFLAGS	= 

overwrite6node2_SOURCES	= overwrite6node2.c
overwrite6node2_LDADD	= $(test_linkfiles) 
overwrite6node2_LDFLAGS	= 

