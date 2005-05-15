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
	LHAResourceGroupTable.c	

test_scrs	=	SNMPAgentSanityCheck


mibsdir			= @MIBS_DIR@
mibs_DATA		= LINUX-HA-MIB.mib

if SNMP_SUBAGENT_BUILD
EXTRA_DIST		= SNMPAgentSanityCheck.in $(mibs_DATA)


INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

halibdir		= $(libdir)/@HB_PKG@

NET_SNMP_CONFIG		= @NET_SNMP_CONFIG@

# binary programs
halib_PROGRAMS		= hbagent 

hbagent_SOURCES		= $(hbagent_srces)



hbagent_LDADD	 	= \
                        $(top_builddir)/lib/clplumbing/libplumb.la \
			$(top_builddir)/lib/hbclient/libhbclient.la \
			$(top_builddir)/membership/ccm/libccmclient.la \
			$(top_builddir)/membership/ccm/libclm.la \
			$(GLIBLIB)

hbagent_LDFLAGS		= @SNMP_LIBS@ 

# test cases
testdir			= $(libdir)/heartbeat
test_SCRIPTS		= $(test_scrs)

else
EXTRA_DIST = $(hbagent_srces) $(test_scrs)
endif
