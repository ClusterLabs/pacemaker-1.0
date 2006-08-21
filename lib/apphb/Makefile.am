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

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl


AM_CFLAGS		= @CFLAGS@

## libraries

lib_LTLIBRARIES		= libapphb.la

libapphb_la_SOURCES	= apphb.c	

if ENABLE_THREAD_SAFE
libapphb_la_CFLAGS	= -DTHREAD_SAFE -D_REENTRANT -DHA_VARLIBDIR='"@HA_VARLIBDIR@"'
else
libapphb_la_CFLAGS	= -lpthread -DHA_VARLIBDIR='"@HA_VARLIBDIR@"'
endif

libapphb_la_LDFLAGS	= -version-info 0:0:0 
