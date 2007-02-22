#
# InterfaceMgr: Interface manager plugins for Linux-HA
#
# Copyright (C) 2005 Guochun Shi <gshi@ncsa.uiuc.edu>
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


SUBDIRS                 = 

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl  \
			-I$(top_builddir)/lib/upmls -I$(top_srcdir)/lib/upmls

AM_CFLAGS		= @CFLAGS@

## libraries

halibdir		= $(libdir)/@HB_PKG@
plugindir		= $(halibdir)/plugins/quorumd
plugin_LTLIBRARIES	= 2_0_8.la

2_0_8_la_SOURCES	= 2_0_8.c
2_0_8_la_LDFLAGS	= -export-dynamic -module -avoid-version -lz $(GNUTLSLIBS)
2_0_8_la_LIBADD		= $(top_builddir)/replace/libreplace.la 
2_0_8_la_CFLAGS 	= $(INCLUDES) $(GNUTLSHEAD)
