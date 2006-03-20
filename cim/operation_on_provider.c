/*
 * operation_on_provider.c: HA_OperationOn provider
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
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID       = "cim-op-on";
static CMPIBroker * Broker            = NULL;
static char 	    ClassName      [] = "HA_OperationOnResource"; 
static char 	    Left           [] = "Resource"; 
static char 	    Right          [] = "Operation";
static char 	    LeftClassName  [] = "HA_PrimitiveResource";
static char 	    RightClassName [] = "HA_Operation"; 

static CMPIArray *  enumerate_right(CMPIBroker * broker, 
			char * classname, CMPIContext * ctx, char * namespace, 
			char * target_name, char * target_role,
                    	CMPIObjectPath * source_op, CMPIStatus * rc);
static CMPIArray *  enumerate_func(CMPIBroker * broker, char * classname, 
			CMPIContext * ctx, char * namespace, 
			char * target_name, char * target_role,
               		CMPIObjectPath * source_op, CMPIStatus * rc);

DeclareInstanceFunctions   (OperationOn);
DeclareAssociationFunctions(OperationOn);

static CMPIArray *
enumerate_right(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role,
                    CMPIObjectPath * source_op, CMPIStatus * rc)
{
        CMPIArray * array;
        uint32_t i, len;
	CIMArray * operations;
	char * rscid, * crname;

	DEBUG_ENTER();
	rscid = CMGetKeyString(source_op, "Id", rc);
	crname = CMGetKeyString(source_op, "CreationClassName", rc);

        if ((operations = cim_get_array(GET_RSC_OPERATIONS, rscid, NULL))==NULL){
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_ERR, "OperationOn: can't get operations");
                return NULL;
        }
      
	len = cim_array_len(operations);
        if ((array = CMNewArray(broker, len, CMPI_ref, rc)) == NULL ) {
		cl_log(LOG_ERR, "%s: create array failed.", __FUNCTION__);
        	goto out;
	}
	
	/* make ObjPath, add it to array */
        for (i = 0; i < cim_array_len(operations); i ++ ) {
                CMPIObjectPath * op;
		CIMTable * operation;
                char * id;
             
		operation = cim_array_index_v(operations, i).v.table; 
		if(operation== NULL ) {
			cl_log(LOG_ERR, "%s: NULL at %d.", __FUNCTION__, i);
			continue;
		} 
		/* create a ObjectPath */ 
                op = CMNewObjectPath(broker, namespace, RightClassName, rc);
                id = cim_table_lookup_v(operation, "id").v.str;
                if ( CMIsNullObject(op) ||  id == NULL ) {
			continue; 
		}

                CMAddKey(op, "Id", id, CMPI_chars);
                CMAddKey(op, "SystemName", rscid, CMPI_chars);
                CMAddKey(op, "SystemCreationClassName", crname, CMPI_chars);
                CMAddKey(op, "CreationClassName", ClassName, CMPI_chars);

                /* add to array */
                CMSetArrayElementAt(array, i, &op, CMPI_ref);
        }
out:
	cim_array_free(operations);
	DEBUG_LEAVE();
        return array;
}

static CMPIArray *
enumerate_func(CMPIBroker * broker, char * classname, CMPIContext * ctx,
               char * namespace, char * target_name, char * target_role,
               CMPIObjectPath * source_op, CMPIStatus * rc)
{
        if ( strcmp(target_name, LeftClassName) == 0 ) {
        	/* if target is a Resource, enumerate its instance names */
                if ( source_op ){
			/* Operation is the source. This is not allowed, just
			return an empty array. */
                        return CMNewArray(broker, 0, CMPI_ref, rc);
                }
                return  cmpi_instance_names(broker, namespace, 
						target_name, ctx, rc);
        } else if ( strcmp(target_name, RightClassName) == 0 ) {
                return enumerate_right(broker, classname, ctx, namespace, 
				target_name, target_role, source_op, rc);
        }
        return NULL;
}

/**********************************************
 * Instance 
 **********************************************/

static CMPIStatus 
OperationOnCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
OperationOnEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, NULL, enumerate_func, 
			FALSE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
OperationOnEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath * cop, 
                         char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, NULL, enumerate_func, 
			TRUE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus 
OperationOnGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_get_inst(Broker, ClassName, ctx, rslt, cop, Left, Right, &rc)
		!= HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
        
}

static CMPIStatus 
OperationOnCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
OperationOnSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
OperationOnDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        return rc;
}

static CMPIStatus 
OperationOnExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
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
OperationOnAssociationCleanup(CMPIAssociationMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
OperationOnAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * assoc_class, const char * result_class,
                            const char * role, const char * result_role, 
                            char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName, 
			assoc_class, result_class, role, result_role, 
			NULL, enumerate_func, TRUE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
OperationOnAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                CMPIResult * rslt, CMPIObjectPath * cop, 
                                const char * assoc_class, const char * result_class,
                                const char * role, const char * result_role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			assoc_class, result_class, role, result_role, 
			NULL, enumerate_func, FALSE, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
OperationOnReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           const char * result_class, const char * role, 
                           char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			result_class, role, NULL, enumerate_func, 
			TRUE, &rc) != HA_OK ) {
                return rc;
        
	}
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
OperationOnReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop,
                          const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	if ( cim_get_hb_status() != HB_RUNNING ) {
		cl_log(LOG_ERR, "OperationsOn: heartbeat not running");
		CMReturn(CMPI_RC_ERR_FAILED);
	}
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
			Left, Right, LeftClassName, RightClassName,
			result_class, role, NULL, enumerate_func, 
			FALSE, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}                

/**************************************************************
 *      MI stub
 *************************************************************/
DeclareInstanceMI   (OperationOn, HA_OperationOnProvider, Broker);
DeclareAssociationMI(OperationOn, HA_OperationOnProvider, Broker);

