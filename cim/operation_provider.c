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

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
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
static const mapping_t	OperationMap[]	= {MAPPING_HA_Operation};

static CMPIInstance * 	make_operation_instance(CMPIObjectPath * op, char * opid,
				char * sys_name, char * sys_cr_name, 
				CMPIStatus * rc);
static int		operation_set_instance(CMPIContext * ctx, CMPIResult * rslt, 
				CMPIObjectPath * cop, CMPIInstance * ci, 
				char ** properties, CMPIStatus * rc);
DeclareInstanceFunctions(Operation);

static CMPIInstance *
make_operation_instance(CMPIObjectPath * op, char * opid, char * rscid, 
		char * crname, CMPIStatus * rc)
{
        char caption[MAXLEN];
        CMPIInstance * ci = NULL;
	CIMArray * ops = NULL;
	CIMTable * operation = NULL;
	int count, i;
	
	DEBUG_ENTER();
        ci = CMNewInstance(Broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: couldn't create instance", __FUNCTION__);
	        CMSetStatusWithChars(Broker, rc, 
		       CMPI_RC_ERR_FAILED, "Could not create instance.");
                return NULL;
        }

        if ((ops = cim_get_array(GET_RSC_OPERATIONS, rscid, NULL)) == NULL ) {
                cl_log(LOG_ERR, "%s: ops is NULL.", __FUNCTION__);
		DEBUG_LEAVE();
		return NULL;
	}

	for (i = 0; i < cim_array_len(ops); i++) {
		char * id;
		operation = cim_array_index_v(ops,i).v.table;
		id = cim_table_lookup_v(operation, "id").v.str;
		if ( strcmp(id, opid) == 0 ) {
			break;
		}
	}

	if ( i == cim_array_len(ops) ) {
		cl_log(LOG_ERR, "Operation %s not found.", opid);
		DEBUG_LEAVE();
		return NULL;
	}

	count = MAPDIM(OperationMap);
	cmpi_set_properties(Broker, ci, operation, OperationMap, count, rc);

        CMSetProperty(ci, "SystemName", rscid, CMPI_chars);
        CMSetProperty(ci, "SystemCreationClassName", crname, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", ClassName, CMPI_chars);
        snprintf(caption, MAXLEN, "Operation.%s", opid);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);

	cim_array_free(ops);
        DEBUG_LEAVE();
	return ci;
}


static int
operation_set_instance(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
		CMPIInstance * ci, char ** properties, CMPIStatus * rc)
{
	CIMArray * ops = NULL;
	int i, len;
	CIMTable * operation = NULL;
	char *id, *sysname, *syscrname;
	const char * key[] = {"Id", "SystemName", "SystemCreationClassName"};

        DEBUG_ENTER();
	id = CMGetKeyString(cop, key[0], rc);
        sysname = CMGetKeyString(cop, key[1], rc);
        syscrname = CMGetKeyString(cop, key[2], rc);

        if ((ops = cim_get_array(GET_RSC_OPERATIONS, sysname,NULL)) == NULL ) {
                cl_log(LOG_ERR, "%s: ops is NULL.", __FUNCTION__);
                return HA_FAIL;
        }
        for (i = 0; i < cim_array_len(ops); i++) {
		char * lid;
                operation = cim_array_index_v(ops,i).v.table;
                if ((lid = cim_table_lookup_v(operation, "id").v.str)==NULL){
			continue;
		}
                if ( strcmp(lid, id) == 0 ) {
                        break;
                }
        }

        if ( i == len || operation == NULL ) {
                cl_log(LOG_ERR, "Operation %s not found.", id);
                DEBUG_LEAVE();
                return HA_FAIL;
        }

	for ( i=0; i<MAPDIM(OperationMap); i++) {
		CMPIData data;
		char * v = NULL;
		data = CMGetProperty(ci, OperationMap[i].name, rc);
		if ( data.value.string && rc->rc == CMPI_RC_OK) {
			v = CMGetCharPtr(data.value.string);
			cim_table_strdup_replace(operation, 
					OperationMap[i].key, v);
		}
	}

	cim_update(UPDATE_OPERATIONS, id, ops, &len);
	cim_array_free(ops);

	rc->rc = CMPI_RC_OK;

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
	CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
OperationEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
OperationGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * cop, char ** properties)
{
        CMPIObjectPath * op;
        CMPIInstance * ci;
        CMPIStatus rc;
	const char * key[] = {"Id", "SystemName", "SystemCreationClassName"};
	char *id, *sysname, *syscrname;

	PROVIDER_INIT_LOGGER();

        DEBUG_ENTER();
	id = CMGetKeyString(cop, key[0], &rc);
        sysname = CMGetKeyString(cop, key[1], &rc);
        syscrname = CMGetKeyString(cop, key[2], &rc);

	op = CMNewObjectPath(Broker, 
		CMGetCharPtr(CMGetNameSpace(cop, &rc)), ClassName, &rc);
        if ( CMIsNullObject(op) ){
                cl_log(LOG_WARNING, "inst_attr: can not create object path.");
        	CMReturnDone(rslt);
        	return rc;
	}

        ci = make_operation_instance(op, id, sysname, syscrname, &rc);
        if ( CMIsNullObject(ci) ) {
        	CMReturnDone(rslt);
        	return rc;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
	DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
OperationCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
OperationSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * cop, 
		CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	operation_set_instance(ctx, rslt, cop, ci, properties, &rc);
        return rc;
}


static CMPIStatus 
OperationDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		 CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
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


/**************************************************
 * Method Provider 
 *************************************************/
static CMPIStatus 
OperationInvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx,
                      CMPIResult * rslt, CMPIObjectPath * ref,
                      const char * method, CMPIArgs * in, CMPIArgs * out)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;    
}


static CMPIStatus 
OperationMethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install provider
 ****************************************************/

DeclareInstanceMI(Operation, HA_OperationProvider, Broker);
DeclareMethodMI(Operation, HA_OperationProvider, Broker);

