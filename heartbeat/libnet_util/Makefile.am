#
# heartbeat: Linux-HA heartbeat code
#
# Copyright (C) 2001 Michael Moerz
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
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

halibdir                = $(libdir)/@HB_PKG@
gliblib			= @GLIBLIB@

LIBRT 			= @LIBRT@
AM_CFLAGS			= @CFLAGS@ \
-DVAR_RUN_D='"$(havarrundir)"'	\
-DVAR_LIB_D='"$(havarlibdir)"'		\
-DHA_D='"$(hadir)"'			\
-DHALIB='"$(halibdir)"' \
@LIBNETDEFINES@

halib_PROGRAMS		= send_arp

send_arp_SOURCES	= send_arp.c
send_arp_LDADD	 	= $(top_builddir)/lib/clplumbing/libplumb.la	\
			  $(gliblib) @LIBNETLIBS@	\
			  $(top_builddir)/lib/pils/libpils.la

