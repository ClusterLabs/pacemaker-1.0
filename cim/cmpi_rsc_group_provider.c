/*
 * CIM Provider - provider for LinuxHA_ClusterResourceGroup
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

#include "cmpi_rsc_group.h"

#define PROVIDER_ID "cim-res-grp"

static CMPIBroker * Broker          = NULL;
static char ClassName []            = "LinuxHA_ClusterResourceGroup"; 


/***************** instance interfaces *******************/
CMPIStatus 
ClusterResourceGroup_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx);

CMPIStatus 
ClusterResourceGroup_EnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
        
CMPIStatus 
ClusterResourceGroup_EnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties);

CMPIStatus 
ClusterResourceGroup_GetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
ClusterResourceGroup_CreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);

CMPIStatus 
ClusterResourceGroup_SetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties);

CMPIStatus 
ClusterResourceGroup_DeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);

CMPIStatus 
ClusterResourceGroup_ExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char * lang, char * query);


/***************** instance entry ********************************/
CMPIInstanceMI * 
LinuxHA_ClusterResourceGroupProvider_Create_InstanceMI(
                CMPIBroker * brkr, 
                CMPIContext * ctx);

/**********************************************
 * Instance _ Interface
 **********************************************/

CMPIStatus 
ClusterResourceGroup_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterResourceGroup_EnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc;
        int ret = 0;
        
        init_logger( PROVIDER_ID );
        cl_log(LOG_INFO,"%s", ClassName);

        ret = enumerate_resource_groups(ClassName, Broker, 
                                        ctx, rslt, ref, 0, &rc);

        if ( ret != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);        
}


CMPIStatus 
ClusterResourceGroup_EnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc;
        int ret = 0;
        
        init_logger( PROVIDER_ID );
        cl_log(LOG_INFO,"%s", ClassName);

        ret = enumerate_resource_groups(ClassName, Broker, 
                                        ctx, rslt, ref, 1, &rc);

        if ( ret != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterResourceGroup_GetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc;
        int ret = 0;

        ret = get_resource_group_instance(ClassName, Broker, 
                                 ctx, rslt, cop, &rc);

        if ( ret != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
ClusterResourceGroup_CreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
ClusterResourceGroup_SetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
ClusterResourceGroup_DeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
ClusterResourceGroup_ExecQuery(CMPIInstanceMI * mi,
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
                "instanceClusterRscInstAttr_";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        ClusterResourceGroup_Cleanup,
        ClusterResourceGroup_EnumInstanceNames,
        ClusterResourceGroup_EnumInstances,
        ClusterResourceGroup_GetInstance,
        ClusterResourceGroup_CreateInstance,
        ClusterResourceGroup_SetInstance, 
        ClusterResourceGroup_DeleteInstance,
        ClusterResourceGroup_ExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterResourceGroupProvider_Create_InstanceMI(
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
