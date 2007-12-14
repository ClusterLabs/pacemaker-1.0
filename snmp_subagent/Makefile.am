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

hbagent_srces  = snmp-config-resolve.h	\
	hbagent.h			\
	hbagent.c			\
	hbagentv2.h			\
	hbagentv2.c			\
	LHANodeTable.h			\
	LHANodeTable_columns.h		\
	LHANodeTable_enums.h		\
	LHANodeTable.c			\
	LHAClusterInfo.h		\
	LHAClusterInfo.c		\
	LHAIFStatusTable.h		\
	LHAIFStatusTable_enums.h	\
	LHAIFStatusTable_columns.h	\
	LHAIFStatusTable.c		\
	LHAMembershipTable_columns.h	\
	LHAMembershipTable_enums.h	\
	LHAMembershipTable.h		\
	LHAMembershipTable.c		\
	LHAHeartbeatConfigInfo.c	\
	LHAHeartbeatConfigInfo.h	\
	LHAResourceGroupTable_columns.h	\
	LHAResourceGroupTable_enums.h	\
	LHAResourceGroupTable.h		\
	LHAResourceGroupTable.c		\
	LHAResourceTable_columns.h \
	LHAResourceTable_enums.h \
	LHAResourceTable.h		\
	LHAResourceTable.c		\
	LHAResourceStatusUpdate.h \
	LHAResourceStatusUpdate.c 

test_scrs	=	SNMPAgentSanityCheck


mibsdir			= @MIBS_DIR@
mibs_DATA		= LINUX-HA-MIB.mib
DIST_LIST		= SNMPAgentSanityCheck.in $(mibs_DATA)

EXTRA_DIST		= $(DIST_LIST)


INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

halibdir		= $(libdir)/@HB_PKG@

# binary programs
halib_PROGRAMS		= hbagent 

hbagent_SOURCES		= $(hbagent_srces)



hbagent_LDADD	 	= \
                        $(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la \
			$(top_builddir)/lib/crm/common/libcrmcommon.la \
			$(top_builddir)/lib/crm/cib/libcib.la \
			$(top_builddir)/lib//crm/pengine/libpe_status.la \
			$(top_builddir)/membership/ccm/libccmclient.la \
			$(top_builddir)/membership/ccm/libclm.la \
			$(CURSESLIBS) \
			$(GLIBLIB)

hbagent_LDFLAGS		= @SNMPAGENTLIB@ 

# test cases
testdir			= $(HA_NOARCHDATAHBDIR)
test_SCRIPTS		= $(test_scrs)

hadir                   = $(sysconfdir)/ha.d

AM_CFLAGS               = @CFLAGS@
