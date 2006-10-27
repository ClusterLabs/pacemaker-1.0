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
COMMONLIBS		= $(top_builddir)/lib/clplumbing/libplumb.la   \
			$(GLIBLIB)

AM_CFLAGS		= @CFLAGS@

## libraries
if QUORUMD_BUILD
QUORUMD_PLUGIN = quorumd.la
endif

halibdir		= $(libdir)/@HB_PKG@
plugindir		= $(halibdir)/plugins/quorum
plugin_LTLIBRARIES	= majority.la twonodes.la weight.la $(QUORUMD_PLUGIN)

majority_la_SOURCES		= majority.c
majority_la_LDFLAGS		= -export-dynamic -module -avoid-version -lz
majority_la_LIBADD		= $(top_builddir)/replace/libreplace.la 

twonodes_la_SOURCES		= twonodes.c
twonodes_la_LDFLAGS		= -export-dynamic -module -avoid-version -lz
twonodes_la_LIBADD		= $(top_builddir)/replace/libreplace.la 

weight_la_SOURCES		= weight.c
weight_la_LDFLAGS		= -export-dynamic -module -avoid-version -lz
weight_la_LIBADD		= $(top_builddir)/replace/libreplace.la 

if QUORUMD_BUILD
quorumd_la_SOURCES		= quorumd.c
quorumd_la_LDFLAGS		= -export-dynamic -module -avoid-version -lz `libgnutls-config --libs`
quorumd_la_LIBADD		= $(top_builddir)/replace/libreplace.la $(COMMONLIBS)
quorumd_la_CFLAGS		= $(INCLUDES) `libgnutls-config --cflags`
endif

