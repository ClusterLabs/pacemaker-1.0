#
# apphb: OCF application heartbeat library
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
			-I$(top_srcdir)/libltdl

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
ckptvarlibdir		= $(havarlibdir)/ckpt
havarrundir		= $(localstatedir)/run/

AM_CFLAGS		= @CFLAGS@ \
			-DCKPTVARLIBDIR='"$(ckptvarlibdir)"'

## libraries
lib_LTLIBRARIES		= libckpt.la

libckpt_la_SOURCES	= libckpt.c	

libckpt_la_CFLAGS	= $(AM_CFLAGS)
libckpt_la_LDFLAGS	= -version-info 0:0:0 

install-exec-local:
	test -p $(ckptvarlibdir) || mkdir $(ckptvarlibdir)

uninstall-local:
	-test -p $(ckptvarlibdir) || mkdir $(ckptvarlibdir)
