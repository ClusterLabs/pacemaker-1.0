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

#include "cmpi_resource.h"
#include "cmpi_utils.h"
#include "linuxha_info.h"

#define PROVIDER_ID     "cim-provider-rsc"


static CMPIBroker * Broker;
static char ClassName[] = "LinuxHA_ClusterResource";



/*****         prototype declare  ***/

CMPIStatus 
LinuxHA_ClusterResourceProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
LinuxHA_ClusterResourceProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt, CMPIObjectPath* ref);
                
CMPIStatus 
LinuxHA_ClusterResourceProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt,
                CMPIObjectPath* ref, char ** properties);

CMPIStatus 
LinuxHA_ClusterResourceProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
LinuxHA_ClusterResourceProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance* ci);


CMPIStatus 
LinuxHA_ClusterResourceProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult* rslt, CMPIObjectPath* cop,
                CMPIInstance* ci, char ** properties);

CMPIStatus 
LinuxHA_ClusterResourceProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt, CMPIObjectPath* cop);
CMPIStatus 
LinuxHA_ClusterResourceProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
                char * lang, char * query);

CMPIStatus 
LinuxHA_ClusterResourceProviderIndicationCleanup(CMPIInstanceMI * mi, 
                CMPIContext * ctx);

CMPIStatus 
LinuxHA_ClusterResourceProviderAuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, 
                CMPISelectExp * filter, const char * type, 
                CMPIObjectPath * classPath, const char * owner);

CMPIStatus 
LinuxHA_ClusterResourceProviderMustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter,
                const char * indType, CMPIObjectPath* classPath);

CMPIStatus 
LinuxHA_ClusterResourceProviderActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation);

CMPIStatus 
LinuxHA_ClusterResourceProviderDeActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean lastActivation);

CMPIInstanceMI * 
LinuxHA_ClusterResourceProvider_Create_InstanceMI(CMPIBroker * brkr, 
                CMPIContext * ctx);


/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
LinuxHA_ClusterResourceProviderCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterResourceProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret ;
        
        init_logger(PROVIDER_ID);
        ret = enumerate_resource_instances(ClassName, 
                                Broker, ctx, rslt, ref, 0, &rc);

        if ( ret == HA_OK ) {
                CMReturn(CMPI_RC_OK);        
        } else {
                CMReturn(CMPI_RC_ERR_FAILED);
        }
}


CMPIStatus 
LinuxHA_ClusterResourceProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt,
                CMPIObjectPath* ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret ;
        
        init_logger(PROVIDER_ID);
        ret = enumerate_resource_instances(ClassName, 
                                Broker, ctx, rslt, ref, 1, &rc);

        if ( ret == HA_OK ) {
                CMReturn(CMPI_RC_OK);        
        } else {
                CMReturn(CMPI_RC_ERR_FAILED);
        }

}


CMPIStatus 
LinuxHA_ClusterResourceProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt,
                CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);
        ret = get_resource_instance(ClassName, Broker, ctx, rslt, 
                        cop, properties, &rc);

        if ( ret == HA_OK ){
                CMReturn(CMPI_RC_OK);
        } else {
                CMReturn(CMPI_RC_ERR_FAILED);
        }
}

CMPIStatus 
LinuxHA_ClusterResourceProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance* ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
LinuxHA_ClusterResourceProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
LinuxHA_ClusterResourceProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
LinuxHA_ClusterResourceProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
                char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}



/**************************************************
 * Indication Interface Implementaion
 *************************************************/


CMPIStatus 
LinuxHA_ClusterResourceProviderIndicationCleanup(CMPIInstanceMI * mi, 
        CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterResourceProviderAuthorizeFilter(CMPIIndicationMI * mi,
        CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
        const char * type, CMPIObjectPath * classPath, const char * owner)
{

        CMPIValue valrc;
        valrc.boolean = 1;

        DEBUG_ENTER();

        CMReturnData(rslt, &valrc, CMPI_boolean);
        CMReturnDone(rslt);


        DEBUG_LEAVE();        
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterResourceProviderMustPoll(CMPIIndicationMI * mi,
        CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
        const char * indType, CMPIObjectPath* classPath)
{
        
        CMPIValue valrc;
        valrc.boolean = 1;

        DEBUG_ENTER();
        CMReturnData(rslt, &valrc, CMPI_boolean);
        CMReturnDone(rslt);
        
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterResourceProviderActivateFilter(CMPIIndicationMI * mi,
        CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
        const char * type, CMPIObjectPath * classPath, 
        CMPIBoolean firstActivation)
{

        DEBUG_ENTER();
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterResourceProviderDeActivateFilter(CMPIIndicationMI * mi,
        CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
        const char * type, CMPIObjectPath * classPath,
        CMPIBoolean lastActivation)
{

        DEBUG_ENTER();
        DEBUG_LEAVE();

        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install interface
 ****************************************************/

static char provider_name[] = "instanceLinuxHA_ClusterResourceProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        provider_name,
        LinuxHA_ClusterResourceProviderCleanup,
        LinuxHA_ClusterResourceProviderEnumInstanceNames,
        LinuxHA_ClusterResourceProviderEnumInstances,
        LinuxHA_ClusterResourceProviderGetInstance,
        LinuxHA_ClusterResourceProviderCreateInstance,
        LinuxHA_ClusterResourceProviderSetInstance, 
        LinuxHA_ClusterResourceProviderDeleteInstance,
        LinuxHA_ClusterResourceProviderExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterResourceProvider_Create_InstanceMI(CMPIBroker * brkr, 
        CMPIContext * ctx)
{
        static CMPIInstanceMI mi = {
                NULL,
                &instMIFT
        };
        Broker = brkr;
        CMNoHook;
        return &mi;
}

