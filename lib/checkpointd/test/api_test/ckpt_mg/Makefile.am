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
noinst_PROGRAMS		= open close unlink set_duration get_status set_active
halib_PROGRAMS		= open close unlink set_duration get_status set_active

open_SOURCES		= open.c
open_LDADD		= $(test_LDADD)
open_LDFLAGS		= 

close_SOURCES		= close.c
close_LDADD		= $(test_LDADD)
close_LDFLAGS		= 

unlink_SOURCES		= unlink.c
unlink_LDADD		= $(test_LDADD)
unlink_LDFLAGS		= 

set_duration_SOURCES	= set_duration.c
set_duration_LDADD	= $(test_LDADD)
set_duration_LDFLAGS	= 

get_status_SOURCES = get_status.c
get_status_LDADD = $(test_LDADD)
get_status_LDFLAGS = 

set_active_SOURCES = set_active.c
set_active_LDADD = $(test_LDADD) 
set_active_LDFLAGS = 
