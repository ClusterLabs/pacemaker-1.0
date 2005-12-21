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

# Subdirectories
SUBDIRS                 = mof

CMPIHEADERS	        = $(CMPI_HEADER_PATH)
INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include    \
                          -I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
                          -I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl    \
                          -I$(CMPI_HEADER_PATH)

 
COMMONLIBS              = $(top_builddir)/lib/clplumbing/libplumb.la      \
                          $(top_builddir)/lib/hbclient/libhbclient.la     \
			  $(top_builddir)/membership/ccm/libccmclient.la  \
			  $(top_builddir)/membership/ccm/libclm.la        \
			  $(GLIBLIB)

#using pthread, or gthread?
#THREADLIB              = `pkg-config --libs gthread-2.0` 
THREADLIB               = lpthread

cimprovidersdir         = $(CMPI_PROVIDER_DIR)

noinst_HEADERS          =  assoc_utils.h cluster_info.h cmpi_utils.h  \
                           resource_common.h constraint_common.h

LRM_DIR                 =  lrm
MGMTLIBS                =  $(top_builddir)/mgmt/daemon/libmgmt.la     \
                           $(top_builddir)/lib/mgmt/libmgmtcommon.la  \
                           $(top_builddir)/lib/$(LRM_DIR)/liblrm.la

cimproviders_LTLIBRARIES = libHAClusterInfo.la                             \
			   libHA_ClusterProvider.la                        \
			   libHA_ClusterNodeProvider.la                    \
                           libHA_SoftwareIdentityProvider.la               \
                           libHA_PrimitiveResourceProvider.la              \
                           libHA_ResourceCloneProvider.la                  \
                           libHA_MasterSlaveResourceProvider.la            \
                           libHA_ResourceGroupProvider.la                  \
                           libHA_LocationConstraintProvider.la             \
                           libHA_ColocationConstraintProvider.la           \
                           libHA_OrderConstraintProvider.la                \
                           libHA_HostedResourceProvider.la                 \
                           libHA_SubResourceProvider.la                    \
                           libHA_InstalledSoftwareIdentityProvider.la      \
			   libHA_ParticipatingNodeProvider.la              \
                           libHA_InstanceAttributesProvider.la             \
                           libHA_OperationProvider.la                      \
                           libHA_ResourceInstanceProvider.la               \
                           libHA_OperationOnProvider.la                    \
                           libHA_IndicationProvider.la                     

#--------------------------------------------------------------
libHAClusterInfo_la_SOURCES                 = cluster_info.c
libHAClusterInfo_la_LIBADD                  = $(COMMONLIBS) $(MGMTLIBS)
#--------------------------------------------------------------
libHA_ClusterProvider_la_SOURCES            = cluster_provider.c
libHA_ClusterProvider_la_LIBADD             = libHAClusterInfo.la
#--------------------------------------------------------------
libHA_SoftwareIdentityProvider_la_SOURCES   = software_identity_provider.c
libHA_SoftwareIdentityProvider_la_LIBADD    = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_ClusterNodeProvider_la_SOURCES	    = cluster_node_provider.c
libHA_ClusterNodeProvider_la_LIBADD	    = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_PrimitiveResourceProvider_la_SOURCES  = resource_common.c primitive_resource_provider.c
libHA_PrimitiveResourceProvider_la_LIBADD   = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_ResourceCloneProvider_la_SOURCES      = resource_common.c resource_clone_provider.c
libHA_ResourceCloneProvider_la_LIBADD       = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_MasterSlaveResourceProvider_la_SOURCES= resource_common.c masterslave_resource_provider.c
libHA_MasterSlaveResourceProvider_la_LIBADD = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_ResourceGroupProvider_la_SOURCES      = resource_common.c resource_group_provider.c
libHA_ResourceGroupProvider_la_LIBADD       = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_ParticipatingNodeProvider_la_SOURCES  = assoc_utils.c participating_node_provider.c
libHA_ParticipatingNodeProvider_la_LIBADD   = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_HostedResourceProvider_la_SOURCES     = assoc_utils.c hosted_resource_provider.c
libHA_HostedResourceProvider_la_LIBADD      = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_InstalledSoftwareIdentityProvider_la_SOURCES = assoc_utils.c installed_software_provider.c
libHA_InstalledSoftwareIdentityProvider_la_LIBADD  = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_ResourceInstanceProvider_la_SOURCES   = assoc_utils.c resource_instance_provider.c
libHA_ResourceInstanceProvider_la_LIBADD    = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_InstanceAttributesProvider_la_SOURCES = inst_attribute_provider.c
libHA_InstanceAttributesProvider_la_LIBADD  = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_SubResourceProvider_la_SOURCES        = assoc_utils.c sub_resource_provider.c
libHA_SubResourceProvider_la_LIBADD         = libHAClusterInfo.la 
#--------------------------------------------------------------
libHA_OperationProvider_la_SOURCES          = operation_provider.c
libHA_OperationProvider_la_LIBADD           = libHAClusterInfo.la
#--------------------------------------------------------------
libHA_LocationConstraintProvider_la_SOURCES = constraint_common.c location_constraint_provider.c
libHA_LocationConstraintProvider_la_LIBADD  = libHAClusterInfo.la
#--------------------------------------------------------------
libHA_ColocationConstraintProvider_la_SOURCES = constraint_common.c colocation_constraint_provider.c
libHA_ColocationConstraintProvider_la_LIBADD  = libHAClusterInfo.la
#--------------------------------------------------------------
libHA_OrderConstraintProvider_la_SOURCES    = constraint_common.c order_constraint_provider.c
libHA_OrderConstraintProvider_la_LIBADD     = libHAClusterInfo.la
#--------------------------------------------------------------
libHA_IndicationProvider_la_SOURCES         = cluster_indication_provider.c
libHA_IndicationProvider_la_LIBADD          = libHAClusterInfo.la
#--------------------------------------------------------------
libHA_OperationOnProvider_la_SOURCES        = assoc_utils.c operation_on_provider.c
libHA_OperationOnProvider_la_LIBADD         = libHAClusterInfo.la
#--------------------------------------------------------------
#noinst_PROGRAMS	    = simpledemo
#simpledemo_SOURCES  = simple_demo.c
#simpledemo_LDADD    = libHAClusterInfo.la 

