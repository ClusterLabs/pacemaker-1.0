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
noinst_PROGRAMS	= close1node1 close1node2 close2node1 close2node2 close3node1 close3node2
halib_PROGRAMS		= close1node1 close1node2 close2node1 close2node2 close3node1 close3node2

close1node1_SOURCES	= close1node1.c
close1node1_LDADD	= $(test_linkfiles)
close1node1_LDFLAGS	= 

close1node2_SOURCES	= close1node2.c
close1node2_LDADD	= $(test_linkfiles)
close1node2_LDFLAGS	= 


close2node1_SOURCES	= close2node1.c
close2node1_LDADD	= $(test_linkfiles)
close2node1_LDFLAGS	= 

close2node2_SOURCES	= close2node2.c
close2node2_LDADD	= $(test_linkfiles)
close2node2_LDFLAGS	= 


close3node1_SOURCES	= close3node1.c
close3node1_LDADD	= $(test_linkfiles)
close3node1_LDFLAGS	= 

close3node2_SOURCES	= close3node2.c
close3node2_LDADD	= $(test_linkfiles)
close3node2_LDFLAGS	= 




