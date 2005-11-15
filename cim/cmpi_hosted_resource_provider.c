/*
 * cmpi_hosted_resource_provider.c: LinuxHA_HostedResource provider
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cmpi_utils.h"
#include "cmpi_cluster.h"
#include "cmpi_node.h"
#include "cmpi_hosted_resource.h"
#include "linuxha_info.h"

#define PROVIDER_ID                "cim-hosted-res"
static CMPIBroker * Broker         = NULL;
static char ClassName           [] = "LinuxHA_HostedResource"; 
static char node_ref            [] = REF_NODE;
static char resource_ref        [] = REF_RESOURCE; 
static char node_class_name     [] = NODE_CLASSNAME; 
static char resource_class_name [] = RESOURCE_CLASSNAME;

/***************** instance interfaces *******************/
CMPIStatus 
HostedResource_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx);
CMPIStatus 
HostedResource_EnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
CMPIStatus 
HostedResource_EnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties);
CMPIStatus 
HostedResource_GetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);
CMPIStatus 
HostedResource_CreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);
CMPIStatus 
HostedResource_SetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties);
CMPIStatus 
HostedResource_DeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);
CMPIStatus 
HostedResource_ExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char * lang, char * query);

/*********************** association interfaces ***********************/
CMPIStatus 
HostedResource_AssociationCleanup(CMPIAssociationMI * mi, 
                CMPIContext * ctx);

CMPIStatus
HostedResource_Associators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, 
                const char * asscClass, const char * resultClass,
                const char * role, const char * resultRole, char ** properties);
CMPIStatus
HostedResource_AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, 
                const char * asscClass, const char * resultClass,
                const char * role, const char * resultRole);
CMPIStatus
HostedResource_References(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt,CMPIObjectPath * op, const char * resultClass,
                const char * role, char ** properties);
CMPIStatus
HostedResource_ReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * assocClass, const char * role);

/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
HostedResource_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
HostedResource_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                                 CMPIResult * rslt, CMPIObjectPath * ref)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = enum_inst_assoc(Broker, ClassName, node_ref, resource_ref, 
                              node_class_name, resource_class_name,
                              ctx, rslt, ref, &node_host_resource, 0, &rc);
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
HostedResource_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * ref, 
                             char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = enum_inst_assoc(Broker, ClassName, node_ref, resource_ref, 
                              node_class_name, resource_class_name,
                              ctx, rslt, ref, &node_host_resource, 1, &rc);
        
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
HostedResource_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = get_inst_assoc(Broker, ClassName, node_ref, resource_ref, 
                             node_class_name, resource_class_name,
                             ctx, rslt, cop, &rc);

        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
HostedResource_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
HostedResource_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
HostedResource_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
HostedResource_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath *ref,
                         char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


/****************************************************
 * Association
 ****************************************************/
CMPIStatus 
HostedResource_AssociationCleanup(CMPIAssociationMI * mi, 
                                  CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
HostedResource_Associators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           const char * assocClass,  const char * resultClass,
                           const char * role, const char * resultRole, 
                           char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = enum_associators(Broker, ClassName, node_ref, resource_ref,
                               node_class_name, resource_class_name, 
                               ctx, rslt, cop, assocClass,
                               resultClass, role, resultRole, 
                               &node_host_resource, 1, &rc); 
        
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


CMPIStatus
HostedResource_AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop, 
                               const char * assocClass, const char * resultClass,
                               const char * role, const char * resultRole)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = enum_associators(Broker, ClassName, node_ref, resource_ref,
                               node_class_name, resource_class_name, 
                               ctx, rslt, cop, assocClass,
                               resultClass, role, resultRole, 
                               &node_host_resource, 0, &rc); 

        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
HostedResource_References(CMPIAssociationMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          const char * resultClass, const char * role, 
                          char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = enum_references(Broker, ClassName, node_ref, resource_ref, 
                              node_class_name, resource_class_name,
                              ctx, rslt, cop, resultClass, role,
                              &node_host_resource, 1, &rc);

        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
HostedResource_ReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              const char * resultClass, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = enum_references(Broker, ClassName, node_ref, resource_ref, 
                              node_class_name, resource_class_name,
                              ctx, rslt, cop, resultClass, role,
                              &node_host_resource, 0, &rc);
        
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                


DeclareInstanceMI(HostedResource_, LinuxHA_HostedResourceProvider, Broker);
DeclareAssociationMI(HostedResource_, LinuxHA_HostedResourceProvider, Broker);
