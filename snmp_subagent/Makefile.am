# Linux-HA: SNMP Subagent
# 
# Copyright (C) 2002 Yixiong Zou (yixiong.zou@intel.com)
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

mibsdir			= @MIBS_DIR@
mibs_DATA		= LINUX-HA-MIB.mib

EXTRA_DIST		= $(mibs_DATA)

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

halibdir		= $(libdir)/@HB_PKG@

NET_SNMP_CONFIG		= @NET_SNMP_CONFIG@

# binary programs
halib_PROGRAMS		= hbagent

hbagent_SOURCES		= snmp-config-resolve.h hbagent.h hbagent.c \
			  LHAClusterInfo.h LHAClusterInfo.c \
			  LHANodeTable.h LHANodeTable_enums.h LHANodeTable_columns.h LHANodeTable.c \
			  LHAIFStatusTable.h LHAIFStatusTable_enums.h LHAIFStatusTable_columns.h LHAIFStatusTable.c \
			  LHAMembershipTable_columns.h LHAMembershipTable_enums.h LHAMembershipTable.h LHAMembershipTable.c \
			  LHAHeartbeatConfigInfo.c LHAHeartbeatConfigInfo.h \
			  LHAResourceGroupTable.c LHAResourceGroupTable_columns.h LHAResourceGroupTable_enums.h LHAResourceGroupTable.h



hbagent_LDADD	 	= \
                        $(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la \
			$(top_builddir)/membership/ccm/libccmclient.la \
			$(top_builddir)/membership/ccm/libclm.la \
			$(GLIBLIB)

hbagent_LDFLAGS		= @SNMP_LIBS@ 

## test cases
#noinst_PROGRAMS		= testclient
#testclient_SOURCES	= testclient.c
#testclient_LDADD        = $(HASUBAGENT_LDADD_EXTRA)

