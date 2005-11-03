/*
 * CIM Provider - provider for LinuxHA_ClusterResourceInstance
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


#include <hb_api.h> 
#include "cmpi_utils.h"
#include "linuxha_info.h"


#define PROVIDER_ID "cim-res-inst"

static CMPIBroker * Broker          = NULL;
static char ClassName []            = "LinuxHA_ClusterResourceInstance"; 

static char attr_ref []             = "InstanceAttribute";
static char resource_ref []         = "ClusterResource"; 
static char attr_class_name []      = "LinuxHA_ClusterResourceInstanceAttr"; 
static char resource_class_name []  = "LinuxHA_ClusterResource";




/***************** instance interfaces *******************/
CMPIStatus 
ClusterResourceInstance_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
ClusterResourceInstance_EnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
        
CMPIStatus 
ClusterResourceInstance_EnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties);

CMPIStatus 
ClusterResourceInstance_GetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
ClusterResourceInstance_CreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);

CMPIStatus 
ClusterResourceInstance_SetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties);

CMPIStatus 
ClusterResourceInstance_DeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);

CMPIStatus 
ClusterResourceInstance_ExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char * lang, char * query);

/*********************** association interfaces ***********************/

CMPIStatus 
ClusterResourceInstance_AssociationCleanup(CMPIAssociationMI * mi, 
                        CMPIContext * ctx);

CMPIStatus
ClusterResourceInstance_Associators(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass,
                const char * role, const char * resultRole, char ** properties);

CMPIStatus
ClusterResourceInstance_AssociatorNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass,
                const char * role, const char * resultRole);

CMPIStatus
ClusterResourceInstance_References(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * resultClass,
                const char * role, char ** properties);

CMPIStatus
ClusterResourceInstance_ReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * assocClass, const char * role);


CMPIAssociationMI * 
LinuxHA_ClusterResourceInstanceProvider_Create_AssociationMI(CMPIBroker* brkr, 
                                CMPIContext *ctx); 
CMPIInstanceMI * 
LinuxHA_ClusterResourceInstanceProvider_Create_InstanceMI(CMPIBroker * brkr, 
                                CMPIContext * ctx); 

/**********************************************
 * Instance 
 **********************************************/

CMPIStatus 
ClusterResourceInstance_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterResourceInstance_EnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult * rslt, CMPIObjectPath * ref)
{

        CMReturn(CMPI_RC_OK); 
}


CMPIStatus 
ClusterResourceInstance_EnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties)
{
       
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterResourceInstance_GetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties)
{
        
       CMReturn(CMPI_RC_OK);

}

CMPIStatus 
ClusterResourceInstance_CreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
ClusterResourceInstance_SetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
ClusterResourceInstance_DeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
ClusterResourceInstance_ExecQuery(CMPIInstanceMI * mi,
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
ClusterResourceInstance_AssociationCleanup(
                 CMPIAssociationMI * mi, 
                 CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
ClusterResourceInstance_Associators(
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
                        attr_ref, resource_ref,
                        attr_class_name, resource_class_name, 
                        ctx, rslt, cop, assocClass,
                        resultClass, role, resultRole, 
                        NULL, 1, &rc); 
        
        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus
ClusterResourceInstance_AssociatorNames(
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
                        attr_ref, resource_ref,
                        attr_class_name, resource_class_name, 
                        ctx, rslt, cop, assocClass,
                        resultClass, role, resultRole, 
                        NULL, 0, &rc); 


        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

CMPIStatus
ClusterResourceInstance_References(
                CMPIAssociationMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * resultClass,
                const char * role, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_references(Broker, ClassName, 
                        attr_ref, resource_ref, 
                        attr_class_name, resource_class_name,
                        ctx, rslt, cop, resultClass, role,
                        NULL, 1, &rc);


        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

CMPIStatus
ClusterResourceInstance_ReferenceNames(
                CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * cop,
                const char * resultClass, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = assoc_enumerate_references(Broker, ClassName, 
                        attr_ref, resource_ref, 
                        attr_class_name, resource_class_name,
                        ctx, rslt, cop, resultClass, role,
                        NULL, 0, &rc);
        

        DEBUG_LEAVE();

        if ( ret != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                

/**************************************************************
 *      install stub
 *************************************************************/


static char inst_provider_name[] = "instanceClusterRscInstance_";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        ClusterResourceInstance_Cleanup,
        ClusterResourceInstance_EnumInstanceNames,
        ClusterResourceInstance_EnumInstances,
        ClusterResourceInstance_GetInstance,
        ClusterResourceInstance_CreateInstance,
        ClusterResourceInstance_SetInstance, 
        ClusterResourceInstance_DeleteInstance,
        ClusterResourceInstance_ExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterResourceInstanceProvider_Create_InstanceMI(CMPIBroker * brkr, 
                CMPIContext * ctx)
{
        static CMPIInstanceMI mi = {
                NULL,
                &instMIFT
        };
        Broker = brkr;
        return &mi;
}


/******************************************************************************/

static char assoc_provider_name[] = "assocationClusterResourceInstance_";

static CMPIAssociationMIFT assocMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        assoc_provider_name,
        ClusterResourceInstance_AssociationCleanup,
        ClusterResourceInstance_Associators,
        ClusterResourceInstance_AssociatorNames,
        ClusterResourceInstance_References,
        ClusterResourceInstance_ReferenceNames

};

CMPIAssociationMI *
LinuxHA_ClusterResourceInstanceProvider_Create_AssociationMI(CMPIBroker* brkr,CMPIContext *ctx)
{
        static CMPIAssociationMI mi = {
                NULL,
                &assocMIFT
        };

        Broker = brkr;
        return &mi;
}


