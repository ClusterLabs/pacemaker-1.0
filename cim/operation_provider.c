/*
 * operation_provider.c: HA_Operation provider
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

static const char * 	PROVIDER_ID 	= "cim-op";
static char 		ClassName []  	= "HA_Operation";
static CMPIBroker * 	Broker    	= NULL;

DeclareInstanceFunctions(Operation);

static struct ha_msg * 
find_operation(const char * rscid, const char *opid)
{
	struct ha_msg *ops, *operation = NULL;
	if ((ops = cim_get_rscops(rscid)) == NULL ) {
		cl_log(LOG_WARNING, "%s: none ops of %s found.",
			__FUNCTION__, rscid);
		return NULL;
	}
	if ((operation = cim_msg_find_child(ops, opid)) ){
		operation = ha_msg_copy(operation);
	}
	ha_msg_del(ops);
	return operation;
}

static CMPIInstance *
operation_make_instance(CMPIObjectPath * op, char* rscid, 
		const char * opid, struct ha_msg *operation, CMPIStatus * rc)
{
        char caption[MAXLEN];
        CMPIInstance * ci = NULL;
	
        ci = CMNewInstance(Broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: couldn't create instance", __FUNCTION__);
	        CMSetStatusWithChars(Broker, rc, 
		       CMPI_RC_ERR_FAILED, "Could not create instance.");
                return NULL;
        }
	
	cmpi_msg2inst(Broker, ci, HA_OPERATION, operation, rc); 
        snprintf(caption, MAXLEN, "Operation.%s", opid);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
	CMSetProperty(ci, "ResourceId", rscid, CMPI_chars);
	return ci;
}

static int
operation_enum_insts(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
               CMPIObjectPath * ref, int EnumInst, CMPIStatus * rc)
{
	char *namespace;
        CMPIObjectPath * op = NULL;
	char *opid, *rscid;
	struct ha_msg *msg, *rsclist, *ops, *operation;
	int i, j, rsccount, opcount;

	DEBUG_ENTER();
	namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
	if ( namespace == NULL ) {
		return HA_FAIL;
	}

	if ((msg = cim_get_all_rsc_list())== NULL 
		|| (rsclist = cim_traverse_allrsc(msg)) == NULL ) {
		goto done;
	}
	ha_msg_del(msg);
	
	rsccount = cim_list_length(rsclist);
	for (i = 0; i < rsccount; i++){
		rscid = cim_list_index(rsclist, i);
		if (rscid == NULL || (ops = cim_get_rscops(rscid)) == NULL ){
			continue;
		}
		opcount = cim_msg_children_count(ops);
		for (j = 0; j < opcount; j++) {	
			const char *const_opid;
 			if(( operation = cim_msg_child_index(ops, j)) == NULL
			 ||(const_opid=cl_get_string(operation, "id"))== NULL){
				continue;
			}
			opid = cim_strdup(const_opid);
	                op = CMNewObjectPath(Broker, namespace, ClassName, rc);
	                if ( EnumInst ) {
	                        CMPIInstance * ci = NULL;
				ci = operation_make_instance(op, rscid, 
							opid, operation, rc);
                	        CMReturnInstance(rslt, ci);
	                } else { /* enumerate instance names */
        	                CMAddKey(op, "ResourceId", rscid, CMPI_chars);
        	                CMAddKey(op, "Id", opid, CMPI_chars); 
                	        CMReturnObjectPath(rslt, op);
                	}
			cim_free(opid);
        	}
	}
	ha_msg_del(rsclist);
done:
	rc->rc = CMPI_RC_OK;
        CMReturnDone(rslt);
	DEBUG_LEAVE();
        return HA_OK;
}


/**********************************************
 * Instance provider functions
 **********************************************/

static CMPIStatus 
OperationCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{

	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
OperationEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	operation_enum_insts(mi, ctx, rslt, ref, FALSE, &rc);
	return rc;
}


static CMPIStatus 
OperationEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	operation_enum_insts(mi, ctx, rslt, ref, TRUE, &rc);
	return rc;
}

static CMPIStatus 
OperationGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * cop, char ** properties)
{
        CMPIObjectPath * op;
        CMPIInstance * ci;
        CMPIStatus rc;
	char *rscid, *id;
	struct ha_msg *operation = NULL;

	PROVIDER_INIT_LOGGER();

	id    = CMGetKeyString(cop, "Id", &rc);
	rscid = CMGetKeyString(cop, "ResourceId", &rc);

	op = CMNewObjectPath(Broker, CMGetCharPtr(CMGetNameSpace(cop, &rc)), 
			ClassName, &rc);
        if ( CMIsNullObject(op) ){
                cl_log(LOG_WARNING, "inst_attr: can not create object path.");
        	CMReturnDone(rslt);
        	return rc;
	}

	operation = find_operation(rscid, id);
	if ( operation == NULL) {
		cl_log(LOG_ERR, "%s: can't find operation for %s", 
			__FUNCTION__, id);
        	CMReturnDone(rslt);
		rc.rc = CMPI_RC_ERR_FAILED;
        	return rc;
	}

        ci = operation_make_instance(op, rscid, id, operation, &rc);
        if ( CMIsNullObject(ci) ) {
        	CMReturnDone(rslt);
        	return rc;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
OperationCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	struct ha_msg *operation;
	char *id, *rscid; 

	id    = CMGetKeyString(cop, "Id", &rc);
        rscid = CMGetKeyString(cop, "ResourceId", &rc);

	/* create an empty operatoin */
	if ((operation = ha_msg_new(16)) == NULL ) {
                cl_log(LOG_ERR, "%s: operation alloc failed.", __FUNCTION__);
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}

	/* fill operation */
	cmpi_inst2msg(ci, HA_OPERATION, operation, &rc);

	/* add op to resource */
	if (cim_add_rscop(rscid, operation) != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
		ha_msg_del(operation);
		goto done;
	} 
	
	ha_msg_del(operation);
	rc.rc = CMPI_RC_OK;
done:
	return rc;
}


static CMPIStatus 
OperationSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * cop, 
		CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	struct ha_msg *operation;
	char *id, *rscid;

	id    = CMGetKeyString(cop, "Id", &rc);
	rscid = CMGetKeyString(cop, "ResourceId", &rc);
	if ((operation = find_operation(rscid, id)) == NULL ) {
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}
	cmpi_inst2msg(ci, HA_OPERATION, operation, &rc);
	cim_update_rscop(rscid, id, operation);
	rc.rc = CMPI_RC_OK;
done:
        return rc;
}


static CMPIStatus 
OperationDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		 CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *id, *rscid; 
	id    = CMGetKeyString(cop, "Id", &rc);
        rscid = CMGetKeyString(cop, "ResourceId", &rc);
	
	if ( cim_del_rscop(rscid, id) == HA_OK ) {
		rc.rc = CMPI_RC_OK;
	} else {
		rc.rc = CMPI_RC_ERR_FAILED;
	}
	return rc;
}

static CMPIStatus 
OperationExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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

DeclareInstanceMI(Operation, HA_OperationProvider, Broker);

