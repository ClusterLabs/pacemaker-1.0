/*
 * resource_clone_provider.c: HA_ResourceClone provider
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

#include <crm_internal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <hb_api.h>
#include "utils.h"
#include "cmpi_utils.h"
#include "cluster_info.h"
#include "resource_common.h"

static const char * 	PROVIDER_ID 	= "cim-clone";
static char 		ClassName []	= "HA_ResourceClone";
static CMPIBroker * 	Broker    	= NULL;

DeclareInstanceFunctions(ResourceClone);

/**********************************************
 * Instance provider functions
 **********************************************/

static CMPIStatus 
ResourceCloneCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMPIStatus rc;
        resource_cleanup(Broker, ClassName, mi, ctx, TID_RES_CLONE, &rc);
	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ResourceCloneEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
        resource_enum_insts(Broker, ClassName, ctx, rslt, ref, FALSE, 
                                TID_RES_CLONE, &rc);
	return rc;
}


static CMPIStatus 
ResourceCloneEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char ** properties)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
	resource_enum_insts(Broker, ClassName, ctx, rslt, ref, TRUE,
                                TID_RES_CLONE, &rc);
	return rc;
}

static CMPIStatus 
ResourceCloneGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        char ** properties)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
        resource_get_inst(Broker, ClassName, ctx, rslt, cop, properties, 
                            TID_RES_CLONE, &rc);
	return rc;
}

static CMPIStatus 
ResourceCloneCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop,
                           CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	resource_create_inst(Broker, ClassName, ctx, rslt, cop, ci, 
				TID_RES_CLONE, &rc);
	return rc;
}


static CMPIStatus 
ResourceCloneSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();	
        resource_update_inst(Broker, ClassName, ctx, rslt, cop, ci, 
			properties, TID_RES_CLONE, &rc);
        return rc;
}


static CMPIStatus 
ResourceCloneDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	resource_del_inst(Broker, ClassName, ctx, rslt, cop, &rc);
	return rc;
}

static CMPIStatus 
ResourceCloneExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                      CMPIResult * rslt, CMPIObjectPath * ref,
                      char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


/************************************************
 * method
 ***********************************************/

static CMPIStatus 
ResourceCloneInvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, 
		const char * method_name, CMPIArgs * in, CMPIArgs * out)
{
        CMPIString * 	classname = NULL;
        CMPIStatus 	rc = {CMPI_RC_OK, NULL};
	int 		ret = 0;
       
	PROVIDER_INIT_LOGGER(); 
	DEBUG_ENTER();
        classname = CMGetClassName(ref, &rc);
        if(strcasecmp(CMGetCharPtr(classname), ClassName) == 0 &&
           strcasecmp(METHOD_ADD_RESOURCE, method_name) == 0 ){
		ret = resource_add_subrsc(Broker, ClassName, ctx, rslt, 
			ref, TID_RES_CLONE, in, out, &rc); 
        }

        CMReturnData(rslt, &ret, CMPI_uint32);
        CMReturnDone(rslt);
	DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
ResourceCloneMethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}
/*****************************************************
 * install provider
 ****************************************************/

DeclareInstanceMI(ResourceClone, HA_ResourceCloneProvider, Broker);
DeclareMethodMI(ResourceClone, HA_ResourceCloneProvider, Broker);
