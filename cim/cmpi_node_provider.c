/*
 * CIM Provider - provider for LinuxHA_ClusterNode
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
#include "cmpi_node.h"


#define PROVIDER_ID "cim-provider-node"

static CMPIBroker * Broker = NULL;
static char ClassName []   = "LinuxHA_ClusterNode";


CMPIStatus 
ClusterNode_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx);

CMPIStatus 
ClusterNode_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * ref);
CMPIStatus 
ClusterNode_EnumInstances(CMPIInstanceMI * mi, CMPIContext* ctx,
                CMPIResult * rslt, CMPIObjectPath * ref,
                char ** properties);

CMPIStatus 
ClusterNode_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * cop,
                char ** properties);

CMPIStatus 
ClusterNode_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * cop,
                CMPIInstance * ci);


CMPIStatus 
ClusterNode_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * cop,
                CMPIInstance * ci, char ** properties);

CMPIStatus 
ClusterNode_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * cop);
CMPIStatus 
ClusterNode_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * ref,
                char * lang, char * query);

CMPIStatus 
ClusterNode_InvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx,
                CMPIResult * rslt, CMPIObjectPath * ref,
                char * method_name, CMPIArgs * in, CMPIArgs * out);

CMPIStatus 
ClusterNode_MethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx);


/******************************************************************/
CMPIInstanceMI * 
LinuxHA_ClusterNodeProvider_Create_InstanceMI(CMPIBroker * brkr, 
                CMPIContext * ctx);

CMPIAssociationMI *
LinuxHA_ClusterNodeProvider_Create_AssociationMI(CMPIBroker * brkr,
                 CMPIContext * ctx);


/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
ClusterNode_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{

        cleanup_node();
	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterNode_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	
        init_logger( PROVIDER_ID );
	if ( enumerate_clusternode_instances(ClassName, Broker, mi, ctx, 
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
ClusterNode_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	
        init_logger( PROVIDER_ID );
	if ( enumerate_clusternode_instances(ClassName, Broker, mi, ctx, 
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
ClusterNode_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger( PROVIDER_ID );

        DEBUG_ENTER();

	if ( get_clusternode_instance(ClassName, Broker, ctx, rslt, cop, 
                                      properties, &rc) != HA_OK ) {
                cl_log(LOG_WARNING, "%s: NULL instance", __FUNCTION__);
                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }

        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
ClusterNode_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop,
                           CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


CMPIStatus 
ClusterNode_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
ClusterNode_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

CMPIStatus 
ClusterNode_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                      CMPIResult * rslt, CMPIObjectPath * ref,
                      char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


/**************************************************
 * Method Provider 
 *************************************************/
CMPIStatus 
ClusterNode_InvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx,
                         CMPIResult * rslt, CMPIObjectPath * ref,
                         char * method_name, CMPIArgs * in, CMPIArgs * out)
{

        CMPIString* class_name = NULL;
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMPIData arg_data;
        CMPIValue valrc;
	
        class_name = CMGetClassName(ref, &rc);
        arg_data = CMGetArg(in, "DirPathName", &rc);
	
        if(strcasecmp(CMGetCharPtr(class_name), ClassName) == 0 &&
           strcasecmp("Send_message_to_all", method_name) == 0 ){
                char* strArg = CMGetCharPtr(arg_data.value.string);
                char cmd[] = "wall ";

                strcat(cmd, strArg);
                valrc.uint32 = system(cmd);
        }

        CMReturnData(rslt, &valrc, CMPI_uint32);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ClusterNode_MethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install provider
 ****************************************************/

static char inst_provider_name[] = "instanceLinuxHA_ClusterNodeProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        ClusterNode_Cleanup,
        ClusterNode_EnumInstanceNames,
        ClusterNode_EnumInstances,
        ClusterNode_GetInstance,
        ClusterNode_CreateInstance,
        ClusterNode_SetInstance, 
        ClusterNode_DeleteInstance,
        ClusterNode_ExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterNodeProvider_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx)
{
        static CMPIInstanceMI mi = {
                NULL,
                &instMIFT
        };
        Broker = brkr;
        return &mi;
}


/*----------------------------------------------------------------*/

