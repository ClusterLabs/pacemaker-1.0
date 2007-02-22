/*
 * attributes_of_resource.c: HA_AttributesOfResource Provider
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
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

#include <lha_internal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <hb_api.h> 
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID       = "cim-attrof";
static CMPIBroker * Broker            = NULL;
static char 	    ClassName      [] = "HA_AttributesOfResource"; 
static char 	    Left           [] = "Resource"; 
static char 	    Right          [] = "InstanceAttributes";
static char 	    LeftClassName  [] = "HA_PrimitiveResource";
static char 	    RightClassName [] = "HA_InstanceAttributes"; 

DeclareInstanceFunctions   (AttributesOfResource);
DeclareAssociationFunctions(AttributesOfResource);

static int 
resource_has_attr(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * rscop, CMPIObjectPath * attrop,  CMPIStatus * rc)
{
        char *rscid, *nvrscid = NULL;
        rscid = CMGetKeyString(rscop, "Id", rc);
	nvrscid = CMGetKeyString(attrop, "ResourceId", rc);
	if ( strncmp(rscid, nvrscid, MAXLEN) == 0 ) {
		return TRUE;
	}
	return FALSE;
}


/**********************************************
 * Instance 
 **********************************************/

static CMPIStatus 
AttributesOfResourceCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
AttributesOfResourceEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, resource_has_attr, NULL, 
			FALSE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
AttributesOfResourceEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath * cop, 
                         char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, resource_has_attr, NULL, 
			TRUE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus 
AttributesOfResourceGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_get_inst(Broker, ClassName, ctx, rslt, cop, Left, Right, &rc)
		!= HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
        
}

static CMPIStatus 
AttributesOfResourceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
AttributesOfResourceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
AttributesOfResourceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        return rc;
}

static CMPIStatus 
AttributesOfResourceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                     CMPIResult * rslt, CMPIObjectPath * cop,
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
static CMPIStatus 
AttributesOfResourceAssociationCleanup(CMPIAssociationMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
AttributesOfResourceAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * assoc_class, const char * result_class,
                            const char * role, const char * result_role, 
                            char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName, 
			assoc_class, result_class, role, result_role, 
			resource_has_attr, NULL, TRUE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
AttributesOfResourceAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                CMPIResult * rslt, CMPIObjectPath * cop, 
                                const char * assoc_class, const char * result_class,
                                const char * role, const char * result_role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			assoc_class, result_class, role, result_role, 
			resource_has_attr, NULL, FALSE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
AttributesOfResourceReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           const char * result_class, const char * role, 
                           char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			result_class, role, NULL, NULL, 
			TRUE, &rc) != HA_OK ) {
                return rc;
        
	}
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
AttributesOfResourceReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop,
                          const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "AttributesOf: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			result_class, role, resource_has_attr, NULL, 
			FALSE, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}                

/**************************************************************
 *      MI stub
 *************************************************************/
DeclareInstanceMI   (AttributesOfResource, HA_AttributesOfResourceProvider, Broker);
DeclareAssociationMI(AttributesOfResource, HA_AttributesOfResourceProvider, Broker);

