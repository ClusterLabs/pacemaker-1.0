#
#  Author: Sun Jiang Dong <sunjd@cn.ibm.com>
#  Copyright (c) 2005 International Business Machines
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

INCLUDES        = -I$(top_builddir)/include  -I$(top_srcdir)/include     \
                  -I$(top_builddir)/libltdl  -I$(top_srcdir)/libltdl     \
		  -I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha    \
		  -I$(top_builddir)          -I$(top_srcdir)

halibdir	=  $(libdir)/@HB_PKG@

COMMONLIBS	=  $(top_builddir)/lib/clplumbing/libplumb.la   \
		   $(AISLIBS)					\
		   $(GLIBLIB)
noinst_HEADERS	=  mgmt_internal.h
EXTRA_DIST	=  mgmtd.pam

LRM_DIR		= lrm

pamdir 		= /etc/pam.d
pam_DATA	= mgmtd.pam

install-data-hook:
		mv $(DESTDIR)$(pamdir)/mgmtd.pam $(DESTDIR)$(pamdir)/hbmgmtd; \
		if [ -f /etc/SuSE-release ]; then \
		sed -i 's/pam_unix/&2/' $(DESTDIR)$(pamdir)/hbmgmtd; \
		fi


halib_PROGRAMS 	=  mgmtd
mgmtd_SOURCES 	=  mgmtd.c
mgmtd_LDFLAGS	= -lpam
mgmtd_LDADD 	=  $(top_builddir)/lib/mgmt/libhbmgmtclient.la \
		   $(top_builddir)/lib/mgmt/libhbmgmttls.la \
		   libhbmgmt.la \
		   $(top_builddir)/lib/$(LRM_DIR)/liblrm.la \
		   $(top_builddir)/lib/mgmt/libhbmgmtcommon.la \
		   $(COMMONLIBS)

lib_LTLIBRARIES = libhbmgmt.la
libhbmgmt_la_SOURCES = mgmt_lib.c mgmt_crm.c mgmt_lrm.c mgmt_hb.c
libhbmgmt_la_CFLAGS = $(INCLUDES)
libhbmgmt_la_LDFLAGS = $(GNUTLSLIBS)
libhbmgmt_la_LIBADD = $(top_builddir)/lib/mgmt/libhbmgmttls.la 		\
		  	$(top_builddir)/lib/crm/cib/libcib.la		\
		   	$(top_builddir)/lib/crm/common/libcrmcommon.la	\
		   	$(top_builddir)/lib/apphb/libapphb.la		\
		   	$(top_builddir)/lib/crm/pengine/libpe_status.la	\
		   	$(top_builddir)/lib/hbclient/libhbclient.la	\
		   	$(top_builddir)/lib/crm/pengine/libpe_status.la	\
			$(COMMONLIBS) $(CURSESLIBS)
