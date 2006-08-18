# Linux-HA: TSA plugin 
# 
# Author: Jia Ming Pan <jmltc@cn.ibm.com>
# Copyright (c) 2006 International Business Machines
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


TSA_LIBDIR=$(TSA_DIR)/sam/lib
CLASSPATH=.:${JAVA_HOME}/lib/dt.jar:${JAVA_HOME}/lib/tools.jar:${TSA_LIBDIR}/eezadapter.jar:${TSA_LIBDIR}/jlanclient.jar:${TSA_LIBDIR}/eezcfghelp.jar:${TSA_LIBDIR}/jlog.jar:${TSA_LIBDIR}/eezmsgs.jar:${TSA_LIBDIR}/log.jar:${TSA_LIBDIR}/eezmsgs_lang.jar:${TSA_LIBDIR}/remoteaccess.jar:${TSA_LIBDIR}/eezpolldr.jar:${TSA_LIBDIR}/samfla.jar:${TSA_LIBDIR}/eezsdk.jar:${TSA_LIBDIR}/sammsgs.jar:${TSA_LIBDIR}/eezutils.jar:${TSA_LIBDIR}/sampolicy.jar:${TSA_LIBDIR}/evd.jar:${TSA_LIBDIR}/samrmc.jar:${TSA_LIBDIR}/jffdc.jar:${TSA_LIBDIR}/ssh.jar:${TSA_LIBDIR}/jhall.jar:${TSA_LIBDIR}/tivolicommondir.jar

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include    \
                          -I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
                          -I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl    \
                          -I$(CMPI_HEADER_PATH)

 
COMMONLIBS              = $(top_builddir)/lib/clplumbing/libplumb.la      \
                          $(top_builddir)/lib/hbclient/libhbclient.la     \
			  $(top_builddir)/membership/ccm/libccmclient.la  \
			  $(top_builddir)/membership/ccm/libclm.la        \
			  $(GLIBLIB)

LRM_DIR                 =  lrm
MGMTLIBS                =  $(top_builddir)/mgmt/daemon/libhbmgmt.la     \
                           $(top_builddir)/lib/mgmt/libhbmgmtcommon.la  \
                           $(top_builddir)/lib/$(LRM_DIR)/liblrm.la

hamgmtlibdir         	= /usr/lib
hamgmtlib_LTLIBRARIES 	= liblhamgmt.la

jardir 			= $(TSA_LIBDIR)
jar_DATA 		= linuxhafla.jar

swig_gen_files		= ${JAVA_PACKAGE_DIR}/LHAMgmtLibJNI.java 	\
			  ${JAVA_PACKAGE_DIR}/LHAMgmtLib.java 		\
			  ha_mgmt_client_wrap.c

JAVA_PACKAGE_DIR	= org/linuxha/sam/plugin
java_sources		= ${JAVA_PACKAGE_DIR}/LHAMgmtLibJNI.java	\
			  ${JAVA_PACKAGE_DIR}/LHANativeResource.java	\
			  ${JAVA_PACKAGE_DIR}/LHANodeResource.java	\
			  ${JAVA_PACKAGE_DIR}/LHAClusterManager.java	\
			  ${JAVA_PACKAGE_DIR}/LHAOrderConstraint.java	\
			  ${JAVA_PACKAGE_DIR}/LHAColocationConstraint.java\
			  ${JAVA_PACKAGE_DIR}/LHAResource.java		\
			  ${JAVA_PACKAGE_DIR}/LHAConstraint.java	\
			  ${JAVA_PACKAGE_DIR}/LHAResourceGroup.java	\
			  ${JAVA_PACKAGE_DIR}/LHALocationConstraint.java\
			  ${JAVA_PACKAGE_DIR}/LinuxHAFLA.java		\
			  ${JAVA_PACKAGE_DIR}/LHANativeClient.java	\
			  ${JAVA_PACKAGE_DIR}/LHACLIClient.java		\
			  ${JAVA_PACKAGE_DIR}/LHAEventdClient.java	\
			  ${JAVA_PACKAGE_DIR}/LinuxHAPlugin.java	\
			  ${JAVA_PACKAGE_DIR}/LHAMgmtLib.java		\
			  ${JAVA_PACKAGE_DIR}/Utilities.java


${swig_gen_files}: ha_mgmt_client.i
	$(SWIG) -java -package org.linuxha.sam.plugin -outdir org/linuxha/sam/plugin/ $^

linuxhafla.jar:${java_sources}
	${JAVA_HOME}/bin/javac -classpath ${CLASSPATH} ${JAVA_PACKAGE_DIR}/*.java 
	${JAVA_HOME}/bin/jar cf linuxhafla.jar ${JAVA_PACKAGE_DIR}/*.class

liblhamgmt_la_SOURCES	= ha_mgmt_client_wrap.c ha_mgmt_client.c ha_mgmt_client.h	\
			  ha_tsa_common.h ha_tsa_common.c
liblhamgmt_la_CFLAGS	= $(INCLUDES) -I$(top_builddir)/include -w
liblhamgmt_la_LIBADD 	= $(COMMONLIBS) $(MGMTLIBS)

tsadir			=  $(libdir)/@HB_PKG@
tsa_PROGRAMS 		= tsa_eventd
tsa_eventd_SOURCES	= tsa_eventd.c ha_tsa_common.c
tsa_eventd_LDADD	= $(COMMONLIBS) $(MGMTLIBS)

cleancore:
	rm -rf core.* javacore*

CLEANFILES=org/linuxha/sam/plugin/*.class *.jar ${swig_gen_files} 

clidir			= $(libdir)/@HB_PKG@
cli_PROGRAMS		= tsa_hacli 
tsa_hacli_SOURCES	= tsa_hacli.c ha_tsa_common.c
tsa_hacli_LDADD		= $(COMMONLIBS) $(MGMTLIBS)

adapterdir		= $(libdir)/@HB_PKG@
adapter_SCRIPTS 	= linuxha-adapter

noinst_PROGRAMS		= dummy
dummy_SOURCES		= dummy.c
dummy_LDADD		= liblhamgmt.la

