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
noinst_PROGRAMS	= 	sect_delete1node1 sect_delete1node2 sect_delete2node1 sect_delete2node2\
					sect_delete3node1 sect_delete3node2 sect_delete4node1 sect_delete4node2\
					sect_delete5node1 sect_delete5node2 sect_delete6node1 sect_delete6node2 
					
sect_delete1node1_SOURCES	= sect_delete1node1.c
sect_delete1node1_LDADD	= $(test_linkfiles) 
sect_delete1node1_LDFLAGS	= 

sect_delete1node2_SOURCES	= sect_delete1node2.c
sect_delete1node2_LDADD	= $(test_linkfiles) 
sect_delete1node2_LDFLAGS	= 

sect_delete2node1_SOURCES	= sect_delete2node1.c
sect_delete2node1_LDADD	= $(test_linkfiles) 
sect_delete2node1_LDFLAGS	= 

sect_delete2node2_SOURCES	= sect_delete2node2.c
sect_delete2node2_LDADD	= $(test_linkfiles) 
sect_delete2node2_LDFLAGS	= 

sect_delete3node1_SOURCES	= sect_delete3node1.c
sect_delete3node1_LDADD	= $(test_linkfiles) 
sect_delete3node1_LDFLAGS	= 

sect_delete3node2_SOURCES	= sect_delete3node2.c
sect_delete3node2_LDADD	= $(test_linkfiles) 
sect_delete3node2_LDFLAGS	= 

sect_delete4node1_SOURCES	= sect_delete4node1.c
sect_delete4node1_LDADD	= $(test_linkfiles) 
sect_delete4node1_LDFLAGS	= 

sect_delete4node2_SOURCES	= sect_delete4node2.c
sect_delete4node2_LDADD	= $(test_linkfiles) 
sect_delete4node2_LDFLAGS	= 

sect_delete5node1_SOURCES	= sect_delete5node1.c
sect_delete5node1_LDADD	= $(test_linkfiles) 
sect_delete5node1_LDFLAGS	= 

sect_delete5node2_SOURCES	= sect_delete5node2.c
sect_delete5node2_LDADD	= $(test_linkfiles) 
sect_delete5node2_LDFLAGS	= 

sect_delete6node1_SOURCES	= sect_delete6node1.c
sect_delete6node1_LDADD	= $(test_linkfiles) 
sect_delete6node1_LDFLAGS	= 

sect_delete6node2_SOURCES	= sect_delete6node2.c
sect_delete6node2_LDADD	= $(test_linkfiles) 
sect_delete6node2_LDFLAGS	= 
