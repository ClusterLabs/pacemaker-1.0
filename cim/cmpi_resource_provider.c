/*
 * CIM Provider - provider for LinuxHA_ClusterResource
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

#define PROVIDER_ID     "cim-res"

static CMPIBroker * Broker = NULL;
static char ClassName []   = "LinuxHA_ClusterResource";


CMPIStatus 
ClusterResource_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx);

CMPIStatus 
ClusterResource_EnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult* rslt, CMPIObjectPath* ref);
                
CMPIStatus 
ClusterResource_EnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath* ref, char ** properties);

CMPIStatus 
ClusterResource_GetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);

CMPIStatus 
ClusterResource_CreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance* ci);


CMPIStatus 
ClusterResource_SetInstance(CMPIInstanceMI * mi,
                CMPIContext* ctx, CMPIResult * rslt, CMPIObjectPath* cop,
                CMPIInstance* ci, char ** properties);

CMPIStatus 
ClusterResource_DeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath* cop);
CMPIStatus  
ClusterResource_ExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
                char * lang, char * query);

CMPIInstanceMI * 
LinuxHA_ClusterResourceProvider_Create_InstanceMI(CMPIBroker * brkr, 
                CMPIContext * ctx);


/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
ClusterResource_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterResource_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
         
        init_logger(PROVIDER_ID);
        if ( enumerate_resource_instances(ClassName, Broker, ctx, 
                                          rslt, ref, 0, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);        
        } else {
                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }
}


CMPIStatus 
ClusterResource_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult* rslt, CMPIObjectPath* ref, 
                              char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        init_logger(PROVIDER_ID);
        if ( enumerate_resource_instances(ClassName, Broker, ctx, 
                                          rslt, ref, 1, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);        
        } else {
                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }

}


CMPIStatus 
ClusterResource_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult* rslt, CMPIObjectPath * cop, 
                            char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        init_logger(PROVIDER_ID);
        if ( get_resource_instance(ClassName, Broker, ctx, rslt, 
                                   cop, properties, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);
        } else {
                return rc;
        }
}

CMPIStatus 
ClusterResource_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop, 
                               CMPIInstance* ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
ClusterResource_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop,
                            CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
ClusterResource_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
ClusterResource_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


/*****************************************************
 * install interface
 ****************************************************/

static char provider_name[] = "instanceClusterResourceProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        provider_name,
        ClusterResource_Cleanup,
        ClusterResource_EnumInstanceNames,
        ClusterResource_EnumInstances,
        ClusterResource_GetInstance,
        ClusterResource_CreateInstance,
        ClusterResource_SetInstance, 
        ClusterResource_DeleteInstance,
        ClusterResource_ExecQuery
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
        return &mi;
}

