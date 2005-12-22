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

static const char * PROVIDER_ID = "cim-op";
static CMPIBroker * G_broker    = NULL;
static char G_classname []      = "HA_Operation";

static CMPIInstance *
make_operation_instance(CMPIObjectPath * op, char * id, char * sys_name,
                        char * sys_cr_name, CMPIStatus * rc);
static int
get_inst_operation(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                   char ** properties, CMPIStatus * rc);
DeclareInstanceFunctions(Operation);

static CMPIInstance *
make_operation_instance(CMPIObjectPath * op, char * id, char * sys_name, 
                        char * sys_cr_name, CMPIStatus * rc)
{
        struct ci_table * operations;
        struct ci_table * operation;
        char * name, * interval, * timeout ;
        CMPIInstance * ci = NULL;
        char caption[256];

        if ((operations = ci_get_inst_operations(sys_name) ) == NULL ) {
                return NULL;
        }

        if ((operation = CITableGet(operations, id).value.table) == NULL ) {
                return NULL;
        }

        ci = CMNewInstance(G_broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
                CITableFree(operations);
                return NULL;
        }
        
        name = CITableGet(operation, "name").value.string;
        interval = CITableGet(operation, "interval").value.string;
        timeout = CITableGet(operation, "timeout").value.string;

        if (interval) { CMSetProperty(ci, "Interval", interval, CMPI_chars); }
        if (timeout)  { CMSetProperty(ci, "Timeout", timeout, CMPI_chars);   }
        if (name)     { CMSetProperty(ci, "Name", name, CMPI_chars);         }
        
        sprintf(caption, "Operation.%s", id);
        CMSetProperty(ci, "Id", id, CMPI_chars);
        CMSetProperty(ci, "SystemName", sys_name, CMPI_chars);
        CMSetProperty(ci, "SystemCreationClassName", sys_cr_name, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", G_classname, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);

        CITableFree(operations);
        return ci;
}

static int
get_inst_operation(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                   char ** properties, CMPIStatus * rc)
{
        CMPIObjectPath * op;
        CMPIString * cmpi_id, * cmpi_sys_name, * cmpi_sys_cr_name;
        char * id, * sys_name, * sys_cr_name;
        CMPIInstance * ci;


        if ((cmpi_id = CMGetKey(cop, "Id", rc).value.string) == NULL ) {
                cl_log(LOG_WARNING, "key Id is NULL");
                return HA_FAIL;
        }

        /* operation's id */
        id = CMGetCharPtr(cmpi_id);

        if ((cmpi_sys_name = CMGetKey(cop, "SystemName", 
                                      rc).value.string) == NULL ) {
                cl_log(LOG_WARNING, "key SystemName is NULL");
                return HA_FAIL;
        }
        
        /* this is the resource's name */
        sys_name = CMGetCharPtr(cmpi_sys_name);

        if ((cmpi_sys_cr_name =  CMGetKey(cop, "SystemCreationClassName", 
                                          rc).value.string)==NULL){
                cl_log(LOG_WARNING, "key SystemCreationClassName is NULL");
                return HA_FAIL;
        }

        sys_cr_name = CMGetCharPtr(cmpi_sys_cr_name);
        op = CMNewObjectPath(G_broker, CMGetCharPtr(CMGetNameSpace(cop, rc)), 
                             G_classname, rc);

        if ( CMIsNullObject(op) ){
                cl_log(LOG_WARNING, "inst_attr: can not create object path.");
                return HA_FAIL;
        }

        ci = make_operation_instance(op, id, sys_name, sys_cr_name, rc);

        if ( CMIsNullObject(ci) ) {
                return HA_FAIL;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        CMRelease(op);
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
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
OperationEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                       CMPIResult * rslt, CMPIObjectPath * ref,
                       char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
OperationGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                     CMPIResult * rslt, CMPIObjectPath * cop,
                     char ** properties)
{

        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        if ( get_inst_operation(ctx, rslt, cop, properties, &rc) == HA_FAIL ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);


}

static CMPIStatus 
OperationCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
OperationSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                     CMPIResult * rslt, CMPIObjectPath * cop,
                     CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
OperationDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
OperationExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                      CMPIResult * rslt, CMPIObjectPath * ref,
                      char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
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
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
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

DeclareInstanceMI(Operation, HA_OperationProvider, G_broker);
DeclareMethodMI(Operation, HA_OperationProvider, G_broker);

