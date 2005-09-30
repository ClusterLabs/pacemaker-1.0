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


#include <hb_api.h> 
#include "cmpi_utils.h"
#include "linuxha_info.h"


#define PROVIDER_ID "cim-provider-ra"

static CMPIBroker * Broker          = NULL;
static char ClassName []            = "LinuxHA_ClusterResourceInstanceAttr"; 


/***************** instance interfaces *******************/
CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
        
CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties);

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties);

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char * lang, char * query);

/************************** entry *************************/
CMPIInstanceMI * 
LinuxHA_ClusterResourceInstanceAttrProvider_Create_InstanceMI(CMPIBroker * brkr, 
                                CMPIContext * ctx); 

/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderCleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult * rslt, CMPIObjectPath * ref)
{

        cl_log(LOG_INFO,"%s", ClassName);
        CMReturn(CMPI_RC_OK);        
}


CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties)
{
        CMReturn(CMPI_RC_OK);

}

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
LinuxHA_ClusterResourceInstanceAttrProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath *ref,
                char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}
                

/**************************************************************
 *      install stub
 *************************************************************/


static char inst_provider_name[] = 
                "instanceLinuxHA_ClusterRscInstAttrProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        LinuxHA_ClusterResourceInstanceAttrProviderCleanup,
        LinuxHA_ClusterResourceInstanceAttrProviderEnumInstanceNames,
        LinuxHA_ClusterResourceInstanceAttrProviderEnumInstances,
        LinuxHA_ClusterResourceInstanceAttrProviderGetInstance,
        LinuxHA_ClusterResourceInstanceAttrProviderCreateInstance,
        LinuxHA_ClusterResourceInstanceAttrProviderSetInstance, 
        LinuxHA_ClusterResourceInstanceAttrProviderDeleteInstance,
        LinuxHA_ClusterResourceInstanceAttrProviderExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterResourceInstanceAttrProvider_Create_InstanceMI(
                CMPIBroker * brkr, 
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
