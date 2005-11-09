#
# InterfaceMgr: Interface manager plugins for Linux-HA
#
# Copyright (C) 2001 Alan Robertson
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

if HBAAPI_BUILD
HBAPING = hbaping.la
endif

if BUILD_OPENAIS_MODULE
OPENAIS=openais.la
endif

if BUILD_TIPC_MODULE
TIPC=tipc.la
endif

SUBDIRS                 = 

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include   \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha   \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl     \
			-I$(top_builddir)/lib/upmls -I$(top_srcdir)/lib/upmls \
                        @TIPC_INCLUDE@

AM_CFLAGS		= @CFLAGS@

## libraries

halibdir		= $(libdir)/@HB_PKG@
plugindir		= $(halibdir)/plugins/HBcomm
plugin_LTLIBRARIES	= bcast.la mcast.la ping.la serial.la ucast.la \
			  ping_group.la  $(HBAPING) $(OPENAIS) $(TIPC)

bcast_la_SOURCES	= bcast.c
bcast_la_LDFLAGS	= -export-dynamic -module -avoid-version

ucast_la_SOURCES	= ucast.c
ucast_la_LDFLAGS	= -export-dynamic -module -avoid-version

mcast_la_SOURCES	= mcast.c
mcast_la_LDFLAGS	= -export-dynamic -module -avoid-version 
mcast_la_LIBADD		= $(top_builddir)/replace/libreplace.la

serial_la_SOURCES	= serial.c
serial_la_LDFLAGS	= -export-dynamic -module -avoid-version

ping_la_SOURCES		= ping.c
ping_la_LDFLAGS		= -export-dynamic -module -avoid-version
ping_la_LIBADD		= $(top_builddir)/replace/libreplace.la

ping_group_la_SOURCES	= ping_group.c
ping_group_la_LDFLAGS	= -export-dynamic -module -avoid-version
ping_group_la_LIBADD	= $(top_builddir)/replace/libreplace.la

hbaping_la_SOURCES   	= hbaping.c
hbaping_la_LDFLAGS  	 = -export-dynamic -module -avoid-version
hbaping_la_LIBADD   	 = $(top_builddir)/replace/libreplace.la

openais_la_SOURCES   	= openais.c
openais_la_LDFLAGS  	 = -export-dynamic -module -avoid-version
openais_la_LIBADD   	 = $(top_builddir)/replace/libreplace.la -levs

tipc_la_SOURCES   	= tipc.c
tipc_la_LDFLAGS  	= -export-dynamic -module -avoid-version
tipc_la_LIBADD   	= $(top_builddir)/replace/libreplace.la
