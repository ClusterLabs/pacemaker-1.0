#
#  Author: Sun Jiang Dong <sunjd@cn.ibm.com>
#  Copyright (c) 2002 International Business Machines
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

include_HEADERS = mgmtd.h

INCLUDES        = -I$(top_builddir)/include  -I$(top_srcdir)/include     \
                  -I$(top_builddir)/libltdl  -I$(top_srcdir)/libltdl     \
		  -I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha    \
		  -I$(top_builddir)          -I$(top_srcdir)

halibdir	=  $(libdir)/@HB_PKG@

COMMONLIBS	=  $(top_builddir)/lib/clplumbing/libplumb.la   \
		   $(top_builddir)/lib/crm/cib/libcib.la	\
		   $(top_builddir)/lib/crm/common/libcrmcommon.la	\
		   $(top_builddir)/lib/apphb/libapphb.la		\
		   $(top_builddir)/crm/pengine/libpengine.la		\
		   $(top_builddir)/lib/hbclient/libhbclient.la \
		   $(CURSESLIBS)					\
		   $(GLIBLIB)
		   
LRM_DIR		= lrm

pamdir 		= /etc/pam.d
pam_DATA	= mgmtd.pam

install-data-hook:
		mv /etc/pam.d/mgmtd.pam /etc/pam.d/mgmtd


halib_PROGRAMS 	=  mgmtd

mgmtd_SOURCES 	=  mgmtd.c mgmtd_gen.c mgmtd_crm.c mgmtd_hb.c mgmtd_lrm.c

mgmtd_LDFLAGS 	=  $(top_builddir)/lib/mgmt/libmgmt.la \
		   $(top_builddir)/lib/mgmt/libtls.la \
		   $(top_builddir)/lib/$(LRM_DIR)/liblrm.la \
		   $(COMMONLIBS) `libgnutls-config --libs` \
		   -lpam -ldl

mgmtd_DEPENDENCIES  = 	$(top_builddir)/lib/mgmt/libmgmt.la \
		     	$(top_builddir)/lib/mgmt/libtls.la \
			$(top_builddir)/lib/$(LRM_DIR)/liblrm.la
