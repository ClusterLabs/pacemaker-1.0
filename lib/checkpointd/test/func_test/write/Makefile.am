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

test_linkfiles		= $(GLIBLIB) \
	 		$(top_builddir)/lib/checkpointd/libckpt.la \
			$(top_builddir)/replace/libreplace.la \
			$(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la 
## binary progs 
noinst_PROGRAMS	= write1node1 write1node2  write2node1 write2node2 write3node1 write3node2 write4node1 write4node2 write5node1 write5node2 write6node1 write6node2 write7node1 write7node2

write1node1_SOURCES	= write1node1.c
write1node1_LDADD	= $(test_linkfiles) 
write1node1_LDFLAGS	= 

write1node2_SOURCES	= write1node2.c
write1node2_LDADD	= $(test_linkfiles) 
write1node2_LDFLAGS	= 

write2node1_SOURCES	= write2node1.c
write2node1_LDADD	= $(test_linkfiles) 
write2node1_LDFLAGS	= 

write2node2_SOURCES	= write2node2.c
write2node2_LDADD	= $(test_linkfiles) 
write2node2_LDFLAGS	= 

write3node1_LDADD	= $(test_linkfiles) 
write3node1_LDFLAGS	= 

write3node2_SOURCES	= write3node2.c
write3node2_LDADD	= $(test_linkfiles) 
write3node2_LDFLAGS	= 

write4node1_SOURCES	= write4node1.c
write4node1_LDADD	= $(test_linkfiles) 
write4node1_LDFLAGS	= 

write4node2_SOURCES	= write4node2.c
write4node2_LDADD	= $(test_linkfiles) 
write4node2_LDFLAGS	= 

write5node1_SOURCES	= write5node1.c
write5node1_LDADD	= $(test_linkfiles) 
write5node1_LDFLAGS	= 

write5node2_SOURCES	= write5node2.c
write5node2_LDADD	= $(test_linkfiles) 
write5node2_LDFLAGS	= 

write6node1_SOURCES	= write6node1.c
write6node1_LDADD	= $(test_linkfiles) 
write6node1_LDFLAGS	= 

write6node2_SOURCES	= write6node2.c
write6node2_LDADD	= $(test_linkfiles) 
write6node2_LDFLAGS	= 

write7node1_SOURCES	= write7node1.c
write7node1_LDADD	= $(test_linkfiles) 
write7node1_LDFLAGS	= 

write7node2_SOURCES	= write7node2.c
write7node2_LDADD	= $(test_linkfiles) 
write7node2_LDFLAGS	= 

