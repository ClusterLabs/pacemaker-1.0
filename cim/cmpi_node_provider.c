/*
 * cmpi_node_provider.c: LinuxHA_ClusterNode provider
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
#include <hb_api.h>
#include "cmpi_utils.h"
#include "cmpi_node.h"

#define PROVIDER_ID          "cim-node"
static CMPIBroker * Broker = NULL;
static char ClassName []   = "LinuxHA_ClusterNode";

#define ReturnErrRC(rc)                       \
do{                                           \
        if (rc.rc == CMPI_RC_OK ) {           \
                CMReturn(CMPI_RC_ERR_FAILED); \
        } else {                              \
                return rc;                    \
        }                                     \
} while(0)

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
                const char * method, CMPIArgs * in, CMPIArgs * out);
CMPIStatus 
ClusterNode_MethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx);

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
	if ( enum_node_instances(ClassName, Broker, mi, ctx, 
                                 rslt, ref, 0, &rc) == HA_OK ) {
	        CMReturn(CMPI_RC_OK);	
        } else {
                ReturnErrRC(rc);
        }
}


CMPIStatus 
ClusterNode_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	
        init_logger( PROVIDER_ID );
        if ( enum_node_instances(ClassName, Broker, mi, ctx, 
                                 rslt, ref, 1, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);	
        } else {
                ReturnErrRC(rc);
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
	if ( get_node_instance(ClassName, Broker, ctx, rslt, cop, 
                               properties, &rc) != HA_OK ) {
                cl_log(LOG_WARNING, "%s: NULL instance", __FUNCTION__);
                ReturnErrRC(rc);
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
                         const char * method, CMPIArgs * in, CMPIArgs * out)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;    
}


CMPIStatus 
ClusterNode_MethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install provider
 ****************************************************/

DeclareInstanceMI(ClusterNode_, LinuxHA_ClusterNodeProvider, Broker);
DeclareMethodMI(ClusterNode_, LinuxHA_ClusterNodeProvider, Broker);

