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

#include <hb_api.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include "cmpi_utils.h"
#include "linuxha_info.h"
#include "cmpi_sub_resource.h"


#define PROVIDER_ID "cim-provider-sr"

static CMPIBroker * Broker         = NULL;
static char ClassName []           = "LinuxHA_SubResource"; 

static char group_ref []            = "Group";
static char group_class_name []     = "LinuxHA_ClusterResourceGroup"; 
static char resource_ref []         = "Resource"; 
static char resource_class_name []  = "LinuxHA_ClusterResource";




/***************** instance interfaces *******************/
CMPIStatus 
LinuxHA_SubResourceProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
LinuxHA_SubResourceProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
        
CMPIStatus 
LinuxHA_SubResourceProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties);

CMPIStatus 
LinuxHA_SubResourceProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
LinuxHA_SubResourceProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);

CMPIStatus 
LinuxHA_SubResourceProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties);

CMPIStatus 
LinuxHA_SubResourceProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);

CMPIStatus 
LinuxHA_SubResourceProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char * lang, char * query);

/*********************** association interfaces ***********************/

CMPIStatus 
LinuxHA_SubResourceProviderAssociationCleanup(CMPIAssociationMI * mi, 
                        CMPIContext * ctx);

CMPIStatus
LinuxHA_SubResourceProviderAssociators(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass,
                const char * role, const char * resultRole, char ** properties);

CMPIStatus
LinuxHA_SubResourceProviderAssociatorNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass,
                const char * role, const char * resultRole);

CMPIStatus
LinuxHA_SubResourceProviderReferences(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * resultClass,
                const char * role, char ** properties);

CMPIStatus
LinuxHA_SubResourceProviderReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * assocClass, const char * role);


CMPIAssociationMI * 
LinuxHA_SubResourceProvider_Create_AssociationMI(CMPIBroker* brkr, 
                                CMPIContext *ctx); 
CMPIInstanceMI * 
LinuxHA_SubResourceProvider_Create_InstanceMI(CMPIBroker * brkr, 
                                CMPIContext * ctx); 



/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
LinuxHA_SubResourceProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_SubResourceProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_instances(Broker, ClassName, 
                        group_ref, resource_ref, 
                        group_class_name, resource_class_name,
                        ctx, rslt, ref, 
                        &group_contain_resource, 0, &rc);


        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_SubResourceProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_instances(Broker, ClassName, 
                        group_ref, resource_ref, 
                        group_class_name, resource_class_name,
                        ctx, rslt, ref, 
                        &group_contain_resource, 1, &rc);


        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_SubResourceProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_get_instance(Broker, ClassName, 
                        group_ref, resource_ref, 
                        group_class_name, resource_class_name,
                        ctx, rslt, cop, &rc);

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

        
}

CMPIStatus 
LinuxHA_SubResourceProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
LinuxHA_SubResourceProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
LinuxHA_SubResourceProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
LinuxHA_SubResourceProviderExecQuery(CMPIInstanceMI * mi,
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
LinuxHA_SubResourceProviderAssociationCleanup(CMPIAssociationMI * mi, 
                        CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
LinuxHA_SubResourceProviderAssociators(
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
                        group_ref, resource_ref,
                        group_class_name, resource_class_name, 
                        ctx, rslt, cop, assocClass,
                        resultClass, role, resultRole, 
                        &group_contain_resource, 1, &rc); 
        
        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus
LinuxHA_SubResourceProviderAssociatorNames(
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
                        group_ref, resource_ref,
                        group_class_name, resource_class_name, 
                        ctx, rslt, cop, assocClass,
                        resultClass, role, resultRole, 
                        &group_contain_resource, 0, &rc); 


        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

CMPIStatus
LinuxHA_SubResourceProviderReferences(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * resultClass,
                const char * role, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_references(Broker, ClassName, 
                        group_ref, resource_ref, 
                        group_class_name, resource_class_name,
                        ctx, rslt, cop, resultClass, role,
                        &group_contain_resource, 1, &rc);


        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

CMPIStatus
LinuxHA_SubResourceProviderReferenceNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * cop,
                const char * resultClass, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_references(Broker, ClassName, 
                        group_ref, resource_ref, 
                        group_class_name, resource_class_name,
                        ctx, rslt, cop, resultClass, role,
                        &group_contain_resource, 0, &rc);
        

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                

/**************************************************************
 *      install stub
 *************************************************************/


static char inst_provider_name[] = "instanceLinuxHA_SubResourceProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        LinuxHA_SubResourceProviderCleanup,
        LinuxHA_SubResourceProviderEnumInstanceNames,
        LinuxHA_SubResourceProviderEnumInstances,
        LinuxHA_SubResourceProviderGetInstance,
        LinuxHA_SubResourceProviderCreateInstance,
        LinuxHA_SubResourceProviderSetInstance, 
        LinuxHA_SubResourceProviderDeleteInstance,
        LinuxHA_SubResourceProviderExecQuery
};

CMPIInstanceMI * 
LinuxHA_SubResourceProvider_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx)
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

static char assoc_provider_name[] = "assocationLinuxHA_SubResourceProvider";

static CMPIAssociationMIFT assocMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        assoc_provider_name,
        LinuxHA_SubResourceProviderAssociationCleanup,
        LinuxHA_SubResourceProviderAssociators,
        LinuxHA_SubResourceProviderAssociatorNames,
        LinuxHA_SubResourceProviderReferences,
        LinuxHA_SubResourceProviderReferenceNames

};

CMPIAssociationMI *
LinuxHA_SubResourceProvider_Create_AssociationMI(
                        CMPIBroker* brkr,CMPIContext *ctx)
{
        static CMPIAssociationMI mi = {
                NULL,
                &assocMIFT
        };

        Broker = brkr;
        CMNoHook;
        return &mi;
}


