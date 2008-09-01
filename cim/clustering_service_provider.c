/*
 * clustering_service_provider.c: HA_ClusteringService provider
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
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <hb_api.h>
#include "cmpi_utils.h"
#include "cluster_info.h"

static const char * PROVIDER_ID = "cim_soft"; 
static CMPIBroker * Broker	= NULL;
static char ClassName []	= "HA_ClusteringService"; 
static char DEFAULT_ID []	= "default_service_id";
DeclareInstanceFunctions(ClusteringService);


static CMPIStatus 
ClusteringServiceCleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ClusteringServiceEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *namespace = NULL;
	CMPIObjectPath * op = NULL;
	PROVIDER_INIT_LOGGER();
        namespace = CMGetCharPtr(CMGetNameSpace(ref, &rc));
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        CMAddKey(op, "Id", DEFAULT_ID, CMPI_chars);
        CMReturnObjectPath(rslt, op);
	return rc;
}


static CMPIStatus 
ClusteringServiceEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * ref, 
                              char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *namespace = NULL;
	CMPIObjectPath *op;
	CMPIEnumeration *en;

	PROVIDER_INIT_LOGGER();
        namespace = CMGetCharPtr(CMGetNameSpace(ref, &rc));
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        en = CBEnumInstanceNames(Broker, ctx, ref, &rc);
        if ( CMIsNullObject(en) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        while ( CMHasNext(en, &rc) ){
                CMPIData ref_data;
                CMPIInstance * inst = NULL;

                ref_data = CMGetNext(en, &rc);
                if (ref_data.value.ref == NULL) {
                        cl_log(LOG_INFO, "failed to get ref");
                        CMReturn(CMPI_RC_ERR_FAILED);
                }

                inst = CBGetInstance(Broker, ctx,
                                ref_data.value.ref, properties, &rc);
                if ( CMIsNullObject(inst) ) {
                        return rc;
                }
                CMReturnInstance(rslt, inst);
        }
        CMReturnDone(rslt);

	return rc;
}

static CMPIStatus 
ClusteringServiceGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            char ** properties)
{
	CMPIObjectPath *op;
	CMPIInstance *ci;
	char *namespace;

	CMPIStatus rc = {CMPI_RC_OK, NULL};

	PROVIDER_INIT_LOGGER();
        namespace = CMGetCharPtr(CMGetNameSpace(cop, &rc));
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        ci = CMNewInstance(Broker, op, &rc);
        if ( CMIsNullObject(ci) ) {
                CMReturn(CMPI_RC_ERR_FAILED);
        }
	CMSetProperty(ci, "Id", DEFAULT_ID, CMPI_chars);
	CMSetProperty(ci, "Caption", DEFAULT_ID, CMPI_chars);
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

	return rc;
}


static CMPIStatus 
ClusteringServiceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
ClusteringServiceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             CMPIInstance * ci,	char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


static CMPIStatus 
ClusteringServiceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                               CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
ClusteringServiceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char * lang, char * query)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


/***********************************************
 * method 
 ***********************************************/

static CMPIStatus 
ClusteringServiceInvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, 
		const char * method_name, CMPIArgs * in, CMPIArgs * out)
{
        CMPIString * 	classname = NULL;
        CMPIStatus 	rc = {CMPI_RC_OK, NULL};
	CMPIObjectPath*	rscop = NULL;
	int 		ret = 0;
        
	PROVIDER_INIT_LOGGER();
        classname = CMGetClassName(ref, &rc);

        if((rscop = CMGetArg(in, "Resource", &rc).value.ref) == NULL ) {
                cl_log(LOG_ERR, "%s: can't get Resource ObjectPath.",
			__FUNCTION__);
                return rc;
        }

        if(strcasecmp(CMGetCharPtr(classname), ClassName) == 0 &&
           strcasecmp(METHOD_ADD_RESOURCE, method_name) == 0 ){
		char * rscid;
		rscid = CMGetKeyString(rscop, "Id", &rc);
		if ( ( ret = cim_rsc_submit(rscid)) == HA_FAIL ) {
			cl_log(LOG_ERR, "%s: failed to add resource %s.",
				__FUNCTION__, rscid);
			rc.rc = CMPI_RC_ERR_FAILED;
		}
        }

        CMReturnData(rslt, &ret, CMPI_uint32);
        CMReturnDone(rslt);
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
ClusteringServiceMethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}



/*****************************************************
 * instance MI
 ****************************************************/
DeclareInstanceMI(ClusteringService, HA_ClusteringServiceProvider, Broker);
DeclareMethodMI(ClusteringService, HA_ClusteringServiceProvider, Broker);


