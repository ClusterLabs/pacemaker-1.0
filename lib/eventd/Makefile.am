#
# lib_event: AIS event service library
#
# Copyright (C) 2002 Alan Robertson, International Business Machines
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
			-I$(top_builddir)/lib/checkpointd \
			-I$(top_srcdir)/lib/checkpointd  \
			-I$(top_builddir)/linux-ha \
			-I$(top_srcdir)/linux-ha  \
			-I$(top_builddir)/libltdl \
			-I$(top_srcdir)/libltdl \
			-I$(top_builddir)/telecom/eventd \
			-I$(top_srcdir)/telecom/eventd \
			-I$(top_builddir)/include/clplumbing \
			-I$(top_srcdir)/include/clplumbing \
			-I$(top_builddir)/heartbeat \
			-I$(top_srcdir)/heartbeat

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
evtvarlibdir		= $(havarlibdir)/evt
havarrundir		= $(localstatedir)/run/

AM_CFLAGS		= @CFLAGS@ \
			-DEVTVARLIBDIR='"$(evtvarlibdir)"'

## libraries
lib_LTLIBRARIES		= libevent.la

libevent_la_SOURCES	= event_lib.c	
libevent_la_CFLAGS	= $(AM_CFLAGS)
libevent_la_LDFLAGS	= -version-info 0:0:0 
