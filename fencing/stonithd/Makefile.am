#  File: Makefile.am
#  Author: Sun Jiang Dong <sunjd@cn.ibm.com>
#  Copyright (c) 2004 International Business Machines
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

INCLUDES         = -I$(top_builddir)/include  -I$(top_srcdir)/include     \
                   -I$(top_builddir)/libltdl  -I$(top_srcdir)/libltdl     \
		   -I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha    \
		   -I$(top_builddir)          -I$(top_srcdir) \
		   -I$(AISPREFIX)/include/openais

halibdir	 =  $(libdir)/@HB_PKG@
havarlibdir      =  $(localstatedir)/lib/@HB_PKG@

halib_PROGRAMS   =  stonithd
stonithd_SOURCES =  stonithd.c
stonithd_LDFLAGS = @LIBLTDL@ -lpils \
           $(top_builddir)/lib/fencing/libstonithd.la \
           $(top_builddir)/lib/crm/common/libcrmcluster.la \
           $(top_builddir)/lib/crm/common/libcrmcommon.la
stonithd_LDADD = $(GLIBLIB) $(CLUSTERLIBS) -lplumb -lpils -lapphb -lstonith

AM_CFLAGS = $(INCLUDES)

# need to make stonithd's owner as hacluster:haclient?
install-exec-local:
	$(mkinstalldirs) $(DESTDIR)/$(havarlibdir)/fencing
	-chgrp $(HA_APIGROUP) $(DESTDIR)/$(havarlibdir)/fencing
	chmod 750 $(DESTDIR)/$(havarlibdir)/fencing

