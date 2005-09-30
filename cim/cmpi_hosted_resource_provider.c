/*
 * CIM Provider
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


#define PROVIDER_ID "cim-provider-hr"

static CMPIBroker * Broker      = NULL;
static char ClassName []        = "LinuxHA_HostedResource"; 

static char node_ref []                 = REF_NODE;
static char resource_ref []             = REF_RESOURCE; 
static char node_class_name []          = NODE_CLASSNAME; 
static char resource_class_name []      = RESOURCE_CLASSNAME;




/***************** instance interfaces *******************/
CMPIStatus 
LinuxHA_HostedResourceProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
LinuxHA_HostedResourceProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
        
CMPIStatus 
LinuxHA_HostedResourceProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties);

CMPIStatus 
LinuxHA_HostedResourceProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
LinuxHA_HostedResourceProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);

CMPIStatus 
LinuxHA_HostedResourceProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties);

CMPIStatus 
LinuxHA_HostedResourceProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);

CMPIStatus 
LinuxHA_HostedResourceProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char * lang, char * query);

/*********************** association interfaces ***********************/

CMPIStatus 
LinuxHA_HostedResourceProviderAssociationCleanup(CMPIAssociationMI * mi, 
                        CMPIContext * ctx);

CMPIStatus
LinuxHA_HostedResourceProviderAssociators(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass,
                const char * role, const char * resultRole, char ** properties);

CMPIStatus
LinuxHA_HostedResourceProviderAssociatorNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass,
                const char * role, const char * resultRole);

CMPIStatus
LinuxHA_HostedResourceProviderReferences(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * resultClass,
                const char * role, char ** properties);

CMPIStatus
LinuxHA_HostedResourceProviderReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * assocClass, const char * role);


CMPIAssociationMI * 
LinuxHA_HostedResourceProvider_Create_AssociationMI(CMPIBroker* brkr, 
                                CMPIContext *ctx); 
CMPIInstanceMI * 
LinuxHA_HostedResourceProvider_Create_InstanceMI(CMPIBroker * brkr, 
                                CMPIContext * ctx); 

/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
LinuxHA_HostedResourceProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_HostedResourceProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult * rslt, CMPIObjectPath * ref)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_instances(Broker, ClassName, 
                        node_ref, resource_ref, 
                        node_class_name, resource_class_name,
                        ctx, rslt, ref, 
                        &node_host_resource, 0, &rc);

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_HostedResourceProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();

        ret = assoc_enumerate_instances(Broker, ClassName, 
                        node_ref, resource_ref, 
                        node_class_name, resource_class_name,
                        ctx, rslt, ref, 
                        &node_host_resource, 1, &rc);

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
LinuxHA_HostedResourceProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_get_instance(Broker, ClassName, 
                        node_ref, resource_ref, 
                        node_class_name, resource_class_name,
                        ctx, rslt, cop, &rc);

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_HostedResourceProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
LinuxHA_HostedResourceProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
LinuxHA_HostedResourceProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
LinuxHA_HostedResourceProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath *ref,
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
LinuxHA_HostedResourceProviderAssociationCleanup(CMPIAssociationMI * mi, 
                        CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
LinuxHA_HostedResourceProviderAssociators(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * assocClass, 
                const char * resultClass,
                const char * role, const char * resultRole, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();

        ret = assoc_enumerate_associators(Broker, ClassName, 
                        node_ref, resource_ref,
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
LinuxHA_HostedResourceProviderAssociatorNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * assocClass, 
                const char * resultClass,
                const char * role, const char * resultRole)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        
        init_logger(PROVIDER_ID);

        DEBUG_ENTER();

        ret = assoc_enumerate_associators(Broker, ClassName, 
                        node_ref, resource_ref,
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
LinuxHA_HostedResourceProviderReferences(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * resultClass,
                const char * role, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_references(Broker, ClassName, 
                        node_ref, resource_ref, 
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
LinuxHA_HostedResourceProviderReferenceNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * cop,
                const char * resultClass, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_references(Broker, ClassName, 
                        node_ref, resource_ref, 
                        node_class_name, resource_class_name,
                        ctx, rslt, cop, resultClass, role,
                        &node_host_resource, 0, &rc);
        

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                

/**************************************************************
 *      install stub
 *************************************************************/


static char inst_provider_name[] = "instanceLinuxHA_HostedResourceProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        LinuxHA_HostedResourceProviderCleanup,
        LinuxHA_HostedResourceProviderEnumInstanceNames,
        LinuxHA_HostedResourceProviderEnumInstances,
        LinuxHA_HostedResourceProviderGetInstance,
        LinuxHA_HostedResourceProviderCreateInstance,
        LinuxHA_HostedResourceProviderSetInstance, 
        LinuxHA_HostedResourceProviderDeleteInstance,
        LinuxHA_HostedResourceProviderExecQuery
};

CMPIInstanceMI * 
LinuxHA_HostedResourceProvider_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx)
{
        static CMPIInstanceMI mi = {
                NULL,
                &instMIFT
        };
        Broker = brkr;
        CMNoHook;
        return &mi;
}


/******************************************************************************/

static char assoc_provider_name[] = "assocationLinuxHA_HostedResourceProvider";

static CMPIAssociationMIFT assocMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        assoc_provider_name,
        LinuxHA_HostedResourceProviderAssociationCleanup,
        LinuxHA_HostedResourceProviderAssociators,
        LinuxHA_HostedResourceProviderAssociatorNames,
        LinuxHA_HostedResourceProviderReferences,
        LinuxHA_HostedResourceProviderReferenceNames

};

CMPIAssociationMI *
LinuxHA_HostedResourceProvider_Create_AssociationMI(CMPIBroker* brkr,CMPIContext *ctx)
{
        static CMPIAssociationMI mi = {
                NULL,
                &assocMIFT
        };

        Broker = brkr;
        CMNoHook;
        return &mi;
}


