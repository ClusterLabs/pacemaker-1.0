# Linux-HA: CIM Provider 
# 
# Author: Jia Ming Pan <jmltc@cn.ibm.com>
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

MAINTAINERCLEANFILES    = Makefile.in

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include    \
                          -I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
                          -I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

 
COMMONLIBS              = $(top_builddir)/lib/clplumbing/libplumb.la      \
                          $(top_builddir)/lib/hbclient/libhbclient.la     \
			  $(top_builddir)/membership/ccm/libccmclient.la  \
			  $(top_builddir)/membership/ccm/libclm.la        \
			  $(GLIBLIB)

#using pthread, or gthread?
#THREADLIB              = `pkg-config --libs gthread-2.0` 
THREADLIB               = lpthread

CMPIHEADERS	        = /usr/local/include/cmpi/	

cimprovidersdir         = /usr/local/lib/cmpi/
cimproviders_LTLIBRARIES = liblinuxha_info.la                                   \
                           libcmpiLinuxHA_ClusterProvider.la  	                \
			   libcmpiLinuxHA_ClusterResourceProvider.la            \
			   libcmpiLinuxHA_ClusterNodeProvider.la                \
			   libcmpiLinuxHA_ParticipatingNodeProvider.la          \
			   libcmpiLinuxHA_HostedResourceProvider.la             \
			   libcmpiLinuxHA_SoftwareIdentityProvider.la           \
			   libcmpiLinuxHA_InstalledSoftwareIdentityProvider.la  \
                           libcmpiLinuxHA_ClusterResourceInstanceProvider.la    \
                           libcmpiLinuxHA_ClusterResourceInstanceAttrProvider.la\
                           libcmpiLinuxHA_ClusterResourceGroupProvider.la       \
                           libcmpiLinuxHA_SubResourceProvider.la		\
			   libcmpiLinuxHA_IndicationProvider.la


#--------------------------------------------------------------
liblinuxha_info_la_SOURCES  	=	linuxha_info.c
liblinuxha_info_la_CFLAGS    	= 	-I$(CMPIHEADERS)
liblinuxha_info_la_LDFLAGS 	= 	$(COMMONLIBS) 

#--------------------------------------------------------------
libcmpiLinuxHA_ClusterProvider_la_SOURCES = cmpi_utils.c cmpi_cluster.c cmpi_cluster_provider.c
libcmpiLinuxHA_ClusterProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_ClusterProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la

#--------------------------------------------------------------
libcmpiLinuxHA_SoftwareIdentityProvider_la_SOURCES = cmpi_utils.c cmpi_software_identity_provider.c
libcmpiLinuxHA_SoftwareIdentityProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_SoftwareIdentityProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------
libcmpiLinuxHA_ClusterResourceProvider_la_SOURCES  = cmpi_utils.c cmpi_resource.c cmpi_resource_provider.c
libcmpiLinuxHA_ClusterResourceProvider_la_CFLAGS   = -I$(CMPIHEADERS)
libcmpiLinuxHA_ClusterResourceProvider_la_LDFLAGS   = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------
libcmpiLinuxHA_ClusterNodeProvider_la_SOURCES	= cmpi_utils.c cmpi_node.c cmpi_node_provider.c
libcmpiLinuxHA_ClusterNodeProvider_la_CFLAGS	= -I$(CMPIHEADERS)
libcmpiLinuxHA_ClusterNodeProvider_la_LDFLAGS	= $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------
libcmpiLinuxHA_ParticipatingNodeProvider_la_SOURCES = cmpi_utils.c \
                                                      cmpi_participating_node_provider.c
libcmpiLinuxHA_ParticipatingNodeProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_ParticipatingNodeProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 


#--------------------------------------------------------------
libcmpiLinuxHA_HostedResourceProvider_la_SOURCES = cmpi_utils.c                   \
						   cmpi_hosted_resource.c         \
						   cmpi_hosted_resource_provider.c
libcmpiLinuxHA_HostedResourceProvider_la_CFLAGS	 = -I$(CMPIHEADERS)
libcmpiLinuxHA_HostedResourceProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------
libcmpiLinuxHA_InstalledSoftwareIdentityProvider_la_SOURCES = cmpi_utils.c    \
                                                              cmpi_installed_software_provider.c
libcmpiLinuxHA_InstalledSoftwareIdentityProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_InstalledSoftwareIdentityProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------

libcmpiLinuxHA_ClusterResourceInstanceProvider_la_SOURCES = cmpi_utils.c    \
							    cmpi_rsc_instance_provider.c
libcmpiLinuxHA_ClusterResourceInstanceProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_ClusterResourceInstanceProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------

libcmpiLinuxHA_ClusterResourceInstanceAttrProvider_la_SOURCES = cmpi_utils.c \
                                                                cmpi_rsc_attribute_provider.c
libcmpiLinuxHA_ClusterResourceInstanceAttrProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_ClusterResourceInstanceAttrProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------
libcmpiLinuxHA_ClusterResourceGroupProvider_la_SOURCES = cmpi_utils.c     \
                                                         cmpi_rsc_group.c \
                                                         cmpi_rsc_group_provider.c
libcmpiLinuxHA_ClusterResourceGroupProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_ClusterResourceGroupProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la 

#--------------------------------------------------------------
libcmpiLinuxHA_SubResourceProvider_la_SOURCES =	cmpi_utils.c        \
						cmpi_sub_resource.c \
                                                cmpi_sub_resource_provider.c
libcmpiLinuxHA_SubResourceProvider_la_CFLAGS  =	-I$(CMPIHEADERS)
libcmpiLinuxHA_SubResourceProvider_la_LDFLAGS =	$(COMMONLIBS) liblinuxha_info.la 
#--------------------------------------------------------------
libcmpiLinuxHA_IndicationProvider_la_SOURCES = cmpi_utils.c cmpi_ha_indication.c \
                                                      cmpi_ha_indication_provider.c
libcmpiLinuxHA_IndicationProvider_la_CFLAGS  = -I$(CMPIHEADERS)
libcmpiLinuxHA_IndicationProvider_la_LDFLAGS = $(COMMONLIBS) liblinuxha_info.la

#--------------------------------------------------------------
noinst_PROGRAMS	    = 	simpledemo
simpledemo_SOURCES  = 	cmpi_cluster.c cmpi_utils.c simple_demo.c
simpledemo_CFLAGS   =	-I$(CMPIHEADERS)
simpledemo_LDFLAGS  =	$(COMMONLIBS) liblinuxha_info.la 
