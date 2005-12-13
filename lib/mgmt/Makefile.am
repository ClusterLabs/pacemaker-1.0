#
# Author: Sun Jiang Dong <sunjd@cn.ibm.com>
# Copyright (c) 2005 International Business Machines
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
MAINTAINERCLEANFILES = Makefile.in

INCLUDES                	=	-I$(top_builddir)/include -I$(top_srcdir)/include \
                       			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha \
					-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl \
					-I$(top_builddir)          -I$(top_srcdir)

halibdir			=	$(libdir)/@HB_PKG@
havarlibdir			=	$(localstatedir)/lib/@HB_PKG@
COMMONLIBS			=	$(GLIBLIB)
MGMT_DIR 			=	mgmt
mgmtdir				=	$(HA_VARLIBDIR)/$(HB_PKG)/$(MGMT_DIR)
apigid                          =       @HA_APIGID@

lib_LTLIBRARIES = libmgmttls.la libmgmtclient.la libmgmtcommon.la
libmgmttls_la_SOURCES = mgmt_tls_lib.c
libmgmttls_la_LIBADD  = $(COMMONLIBS)
libmgmttls_la_LDFLAGS = `libgnutls-config --libs`
libmgmttls_la_CFLAGS = $(INCLUDES) `libgnutls-config --cflags`

libmgmtclient_la_SOURCES = mgmt_client_lib.c
libmgmtclient_la_LIBADD = $(COMMONLIBS) libmgmttls.la
libmgmtclient_la_CFLAGS = $(INCLUDES)

libmgmtcommon_la_SOURCES = mgmt_common_lib.c
libmgmtcommon_la_LIBADD = $(COMMONLIBS) 
libmgmtcommon_la_CFLAGS = $(INCLUDES)

halib_LTLIBRARIES = _pymgmt.la
_pymgmt_la_SOURCES = pymgmt_wrap.c mgmt_client_lib.c mgmt_tls_lib.c mgmt_common_lib.c
_pymgmt_la_CFLAGS = -I$(includedir)/@PYTHON_HEADER_DIR@ -I$(top_builddir)/include -w
_pymgmt_la_LDFLAGS = -module `libgnutls-config --libs`

halib_SCRIPTS = pymgmt.py

pymgmt_wrap.c : pymgmt.i
	$(SWIG) -python -o $@ $^

pymgmt.py: pymgmt_wrap.c

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(mgmtdir)
	-chgrp $(apigid) $(DESTDIR)/$(mgmtdir)
	chmod 770 $(DESTDIR)/$(mgmtdir)
