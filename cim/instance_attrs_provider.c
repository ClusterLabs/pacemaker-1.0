/*
 * instance_attr_provider.c: HA_InstanceAttributes provider
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
#include "cmpi_utils.h"
#include "cluster_info.h"

static const char * 	PROVIDER_ID 	= "cim-attr";
static char 		ClassName []  	= "HA_InstanceAttributes";
static CMPIBroker * 	Broker    	= NULL;
DeclareInstanceFunctions(InstanceAttributes);

static struct ha_msg * 
find_instattrs_nvpair(const char * rscid, const char *nvid)
{
	struct ha_msg *instattrss = NULL, *nvpair;

	if ((instattrss = cim_rscattrs_get(rscid)) == NULL ) {
		return NULL;
	}
	if ((nvpair = cim_msg_find_child(instattrss, nvid))) {
		nvpair = ha_msg_copy(nvpair);
	}
	ha_msg_del(instattrss);
	return nvpair;
}


static CMPIInstance *
instattrs_make_instance(CMPIObjectPath * op, char* rscid, 
		const char * nvid, struct ha_msg *nvpair, CMPIStatus * rc)
{
        char caption[MAXLEN], id[MAXLEN];
        CMPIInstance * ci = NULL;
	
        ci = CMNewInstance(Broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: couldn't create instance", __FUNCTION__);
	        CMSetStatusWithChars(Broker, rc, 
		       CMPI_RC_ERR_FAILED, "Could not create instance.");
                return NULL;
        }
	/* set properties */	
	cmpi_msg2inst(Broker, ci, HA_INSTANCE_ATTRIBUTES, nvpair, rc); 

	cim_debug2(LOG_INFO, "%s: rscid: %s, nvid: %s.", __FUNCTION__, rscid, nvid);
        snprintf(caption, MAXLEN, "InstanceAttributes.%s-%s", rscid, nvid);
	strncpy(id, nvid, MAXLEN);
	CMSetProperty(ci, "Id", id, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
	CMSetProperty(ci, "ResourceId", rscid, CMPI_chars);
	return ci;
}

static int
instattrs_enum_insts(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
               CMPIObjectPath * ref, int EnumInst, CMPIStatus * rc)
{
	char *namespace;
        CMPIObjectPath * op = NULL;
	char *nvid, *rscid;
	struct ha_msg *msg, *rsclist, *instattrs, *nvpair;
	int i, j, rsccount, nvcount;

	namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));

	/* all resources, including the sub resources */
	if ((msg = cim_get_all_rsc_list())== NULL 
		|| (rsclist = cim_traverse_allrsc(msg)) == NULL ) {
		cl_log(LOG_WARNING, "%s: no resource found.", __FUNCTION__);
		goto done;
	}
	ha_msg_del(msg);

	/* for each resource */	
	rsccount = cim_list_length(rsclist);
	for (i = 0; i < rsccount; i++){
		rscid = cim_list_index(rsclist, i);
		if (!rscid || cim_get_rsctype(rscid) != TID_RES_PRIMITIVE) {
			continue;
		}

		/* get the resource's instance attributes */
		if ((instattrs = cim_rscattrs_get(rscid)) == NULL ){
			continue;
		}

		/* for each nvpair in the attirubtes */
		nvcount = cim_msg_children_count(instattrs);
		for (j = 0; j < nvcount; j++) {	
			const char *const_id;
 			if(( nvpair = cim_msg_child_index(instattrs, j)) == NULL
			 ||(const_id = cl_get_string(nvpair, "id"))== NULL){
				continue;
			}

			nvid = cim_strdup(const_id);

			/* create objectpath or instance */
	                op = CMNewObjectPath(Broker, namespace, ClassName, rc);
	                if ( EnumInst ) {
	                        CMPIInstance * ci = NULL;
				ci = instattrs_make_instance(op, rscid, nvid, nvpair, rc);
                	        CMReturnInstance(rslt, ci);
	                } else { /* enumerate instance names */
        	                CMAddKey(op, "ResourceId", rscid, CMPI_chars);
        	                CMAddKey(op, "Id", nvid, CMPI_chars); 
                	        CMReturnObjectPath(rslt, op);
                	}
			cim_free(nvid);
        	}
	}
done:
	rc->rc = CMPI_RC_OK;
        CMReturnDone(rslt);
        return HA_OK;
}


/**********************************************
 * Instance provider functions
 **********************************************/

static CMPIStatus 
InstanceAttributesCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
InstanceAttributesEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	instattrs_enum_insts(mi, ctx, rslt, ref, FALSE, &rc);
	return rc;
}


static CMPIStatus 
InstanceAttributesEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	instattrs_enum_insts(mi, ctx, rslt, ref, TRUE, &rc);
	return rc;
}

static CMPIStatus 
InstanceAttributesGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, char ** properties)
{
        CMPIObjectPath * op;
        CMPIInstance * ci;
        CMPIStatus rc;
	char *rscid, *id, *namespace;
	struct ha_msg *nvpair = NULL;

	PROVIDER_INIT_LOGGER();
	id    = CMGetKeyString(cop, "Id", &rc);
	rscid = CMGetKeyString(cop, "ResourceId", &rc);

	/* create objectpath for the instance */
	namespace = CMGetCharPtr(CMGetNameSpace(cop, &rc));
	op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                cl_log(LOG_WARNING, "inst_attr: can not create object path.");
        	CMReturnDone(rslt);
        	return rc;
	}

	/* get the nvpair */
	if ((nvpair = find_instattrs_nvpair(rscid, id)) == NULL ) {
		cl_log(LOG_ERR, "%s: can't find instattrs for %s", 
			__FUNCTION__, id);
        	CMReturnDone(rslt);
		rc.rc = CMPI_RC_ERR_FAILED;
        	return rc;
	}

	/* make instance */
        ci = instattrs_make_instance(op, rscid, id, nvpair, &rc);
        if ( CMIsNullObject(ci) ) {
        	CMReturnDone(rslt);
        	return rc;
        }
	ha_msg_del(nvpair);
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
InstanceAttributesCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	struct ha_msg *nvpair;
	char *id, *rscid; 

	id    = CMGetKeyString(cop, "Id", &rc);
        rscid = CMGetKeyString(cop, "ResourceId", &rc);

	/* create an empty operatoin */
	if ((nvpair = ha_msg_new(16)) == NULL ) {
                cl_log(LOG_ERR, "%s: nvpair alloc failed.", __FUNCTION__);
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}

	cmpi_inst2msg(ci, HA_INSTANCE_ATTRIBUTES, nvpair, &rc); 
	/* add attr to resource */
	if (cim_update_attrnvpair(rscid, id, nvpair) != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
		ha_msg_del(nvpair);
		goto done;
	} 
	
	ha_msg_del(nvpair);
	rc.rc = CMPI_RC_OK;
done:
	return rc;
}


static CMPIStatus 
InstanceAttributesSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * cop, 
		CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	struct ha_msg *nvpair;
	char *id, *rscid;
	int ret;

	id    = CMGetKeyString(cop, "Id", &rc);
	rscid = CMGetKeyString(cop, "ResourceId", &rc);

	if ( (nvpair = find_instattrs_nvpair(rscid, id)) == NULL ) {
		cl_log(LOG_ERR, "%s: failed to get nvpair.", __FUNCTION__);
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}

	cmpi_inst2msg(ci, HA_INSTANCE_ATTRIBUTES, nvpair, &rc); 
	cim_debug_msg(nvpair, "%s: nvpair from inputs:", __FUNCTION__);
	if ( (ret = cim_update_attrnvpair(rscid, id, nvpair)) == HA_OK ) { 
		rc.rc = CMPI_RC_OK;
	} else {
		rc.rc = CMPI_RC_ERR_FAILED;
	}
	ha_msg_del(nvpair);
done:
        return rc;
}


static CMPIStatus 
InstanceAttributesDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		 CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *id, *rscid; 

	id    = CMGetKeyString(cop, "Id", &rc);
        rscid = CMGetKeyString(cop, "ResourceId", &rc);
	
	if ( cim_remove_attrnvpair(rscid, id) == HA_OK ) {
		rc.rc = CMPI_RC_OK;
	} else {
		rc.rc = CMPI_RC_ERR_FAILED;
	}

	return rc;
}

static CMPIStatus 
InstanceAttributesExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
		 CMPIResult * rslt, CMPIObjectPath * ref, 
		char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

/*****************************************************
 * install provider
 ****************************************************/

DeclareInstanceMI(InstanceAttributes, HA_InstanceAttributesProvider, Broker);

