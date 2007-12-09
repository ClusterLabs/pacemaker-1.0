/*
 * constraint_common.c: common functions for constraint providers
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

#include <hb_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cluster_info.h"
#include "constraint_common.h"
#include "cmpi_utils.h"

static char SystemName		[] = "LinuxHACluster";
static char SystemCrName	[] = "HA_Cluster";

static CMPIInstance *	make_instance_byid(CMPIBroker * broker, char * classname,
				CMPIObjectPath * op, char * id, uint32_t type, 
				CMPIStatus * rc);
static CMPIInstance *	make_instance(CMPIBroker * broker, char * classname, 
				CMPIObjectPath *op, struct ha_msg *cons, 
				int type, CMPIStatus * rc);
static CMPIInstance *
make_instance(CMPIBroker *broker, char *classname, CMPIObjectPath *op, 
               struct ha_msg* constraint, int type, CMPIStatus *rc)
{
        CMPIInstance * ci = NULL;
        char *id;
	char caption[MAXLEN];

	DEBUG_ENTER();
        ci = CMNewInstance(broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get create instance");
                goto out;
        }
        
        id = cim_strdup(cl_get_string(constraint, "id"));
        if ( id == NULL ) { return NULL; }

        /* setting properties */
        CMSetProperty(ci, "Id", id, CMPI_chars);
        CMSetProperty(ci, "SystemCreationClassName", SystemCrName, CMPI_chars);
        CMSetProperty(ci, "SystemName", SystemName, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);

        if ( type == TID_CONS_ORDER ) {
		cmpi_msg2inst(broker, ci, 
				HA_ORDER_CONSTRAINT, constraint, rc); 
        } else if ( type == TID_CONS_LOCATION ) {
		cmpi_msg2inst(broker, ci, 
				HA_LOCATION_CONSTRAINT, constraint, rc);
        } else if ( type == TID_CONS_COLOCATION ) {
		cmpi_msg2inst(broker, ci, 
				HA_COLOCATION_CONSTRAINT, constraint, rc);
        }
        snprintf(caption, MAXLEN, "Constraint.%s", id);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
	cim_free(id);
out:
	DEBUG_LEAVE();
        return ci; 
}

static CMPIInstance *
make_instance_byid(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
                     char * id, uint32_t type, CMPIStatus * rc) 
{
	CMPIInstance * ci;
	struct ha_msg *constraint;
	int funcid = 0;

	DEBUG_ENTER();
        switch(type) {
        case TID_CONS_LOCATION: funcid=GET_LOCATION_CONSTRAINT; break;
        case TID_CONS_COLOCATION: funcid=GET_COLOCATION_CONSTRAINT; break;
        case TID_CONS_ORDER: funcid=GET_ORDER_CONSTRAINT; break;
        default:break;
        }

        if ( (constraint= cim_query_dispatch(funcid, id, NULL)) == NULL ) { 
		return NULL; 
	}
	ci = make_instance(broker, classname, op, constraint, type, rc);
	ha_msg_del(constraint);
	DEBUG_LEAVE();
	return ci;
}

int
constraing_get_inst(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
              CMPIResult * rslt, CMPIObjectPath * cop,
              char ** properties, uint32_t type, CMPIStatus * rc)
{
        CMPIInstance* ci = NULL;
        CMPIObjectPath * op = NULL;
        char * consid = NULL;
                
	DEBUG_ENTER();
        /* get the key from the object path */
	if (( consid = CMGetKeyString(cop, "Id", rc)) == NULL ) {
                cl_log(LOG_ERR, "%s: Id is missing.", __FUNCTION__);
		return HA_FAIL;
	}

        /* create a object path */
        op = CMNewObjectPath(broker, 
		CMGetCharPtr(CMGetNameSpace(cop, rc)), classname, rc);
        if ( CMIsNullObject(op) ){
                cl_log(LOG_ERR, "%s: Could not create object path.", 
			__FUNCTION__);
		return HA_FAIL;
        }

        /* make an instance */
        ci = make_instance_byid(broker, classname, op, consid, type, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: Could not create instance.", 
			__FUNCTION__);
		return HA_FAIL;
        }

        /* add the instance to result */
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        return HA_OK;
}

int 
constraint_enum_insts(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref, 
               int need_inst, uint32_t type, CMPIStatus * rc)
{
        char * namespace = NULL;
        CMPIObjectPath * op = NULL;
        int i, funcid = 0, len;
	struct ha_msg * cons;

        /* create object path */
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ){
                return HA_FAIL;
        }
       
	switch(type){
		case TID_CONS_ORDER: 
			funcid = GET_ORDER_CONS_LIST;break;
		case TID_CONS_LOCATION: 
			funcid = GET_LOCATION_CONS_LIST; break;
		case TID_CONS_COLOCATION: 
			funcid = GET_COLOCATION_CONS_LIST; break;
		default: break;
	} 

        if ( ( cons = cim_query_dispatch(funcid, NULL, NULL) ) == NULL ) {
                return HA_FAIL;
        }

	len = cim_list_length(cons);
        /* for each constraint */
        for ( i = 0; i < len; i++) {
                char * consid = cim_list_index(cons, i);
                if ( need_inst ) {
                        /* if need instance, make instance an return it */
                        CMPIInstance * ci = NULL;
                        ci = make_instance_byid(broker, classname, op, 
                        		consid, type, rc); 
                        if ( CMIsNullObject(ci) ){
                                cl_log(LOG_WARNING, 
                                   "%s: can not make instance", __FUNCTION__);
                                return HA_FAIL;
                        }
                        
                        cl_log(LOG_INFO, "%s: return instance", __FUNCTION__);
                        CMReturnInstance(rslt, ci);
                } else {
                        /* otherwise, just add keys to objectpath and return it */
                        CMAddKey(op, "Id", consid, CMPI_chars);      
                        CMAddKey(op, "SystemName", SystemName, CMPI_chars);
                        CMAddKey(op, "SystemCreationClassName", SystemCrName, CMPI_chars);
                        CMAddKey(op, "CreationClassName", classname, CMPI_chars);

                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }
        }
        CMReturnDone(rslt);
	rc->rc = CMPI_RC_OK;
	ha_msg_del(cons);
        return HA_OK;
}

int	
constraint_delete_inst(CMPIBroker * broker, char * classname, 
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, uint32_t type, CMPIStatus * rc)
{
	const char * key [] = {"Id", "CreationClassName", "SystemName",
			"SystemCreationClassName"};
	int funcid = 0;
	char * id;

	DEBUG_ENTER();
	if ((id = CMGetKeyString(cop, key[0], rc)) == NULL ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_ERR, "del_cons: can't get constraint id.");
		return HA_FAIL;
	}
	
	switch(type){
		case TID_CONS_ORDER: 
			funcid = DEL_ORDER_CONSTRAINT;
			break;
		case TID_CONS_LOCATION: 
			funcid = DEL_LOCATION_CONSTRAINT; 
			break;
		case TID_CONS_COLOCATION: 
			funcid = DEL_COLOCATION_CONSTRAINT; 
			break;
		default: 
			cl_log(LOG_WARNING, "del_cons: Unknown type");
			break;
	} 
	if ( cim_update_dispatch(funcid, id, NULL, NULL) == HA_OK ) {
		rc->rc = CMPI_RC_OK;
	} else {
		rc->rc = CMPI_RC_ERR_FAILED;
		cl_log(LOG_ERR, "del_cons: cim_update return error.");
	}
	DEBUG_LEAVE();

	return HA_OK;
}

int	
constraint_update_inst(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, char ** properties,
		uint32_t type, CMPIStatus * rc)
{
	struct ha_msg * t=NULL;
	const char * key [] = {"Id", "CreationClassName", "SystemName",
				"SystemCreationClassName"};
	char * id, *crname, *sysname, *syscrname;
	int ret = HA_FAIL;

	DEBUG_ENTER();
	id        = CMGetKeyString(cop, key[0], rc);
	crname    = CMGetKeyString(cop, key[1], rc);
	sysname   = CMGetKeyString(cop, key[2], rc);
	syscrname = CMGetKeyString(cop, key[3], rc);
	
	switch(type){
	case TID_CONS_ORDER:
		t = cim_query_dispatch(GET_ORDER_CONSTRAINT, id, NULL); 
		cmpi_inst2msg(ci, HA_ORDER_CONSTRAINT, t, rc); 
		ret = cim_update_dispatch(UPDATE_ORDER_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_LOCATION:
		t = cim_query_dispatch(GET_LOCATION_CONSTRAINT, id, NULL); 
		cmpi_inst2msg(ci, HA_LOCATION_CONSTRAINT, t, rc); 
		ret = cim_update_dispatch(UPDATE_LOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_COLOCATION:
		t = cim_query_dispatch(GET_COLOCATION_CONSTRAINT, id, NULL); 
		cmpi_inst2msg(ci, HA_COLOCATION_CONSTRAINT, t, rc); 
		ret = cim_update_dispatch(UPDATE_COLOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	default: break;
	}

	ha_msg_del(t);
	/* if update OK, return CMPI_RC_OK */
	rc->rc = (ret == HA_OK)? CMPI_RC_OK : CMPI_RC_ERR_FAILED;
	DEBUG_ENTER();
	return HA_OK;
}

int
constraint_create_inst(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, uint32_t type, 
		CMPIStatus * rc)
{
	struct ha_msg *t;

	int ret = HA_FAIL;
	DEBUG_ENTER();
	if (( t = ha_msg_new(16)) == NULL ) {
		rc->rc = CMPI_RC_ERR_FAILED;
		DEBUG_LEAVE();
		return HA_FAIL;
	}
	switch(type){
	case TID_CONS_ORDER:
		cmpi_inst2msg(ci, HA_ORDER_CONSTRAINT, t, rc); 
		ret = cim_update_dispatch(UPDATE_ORDER_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_LOCATION:
		cmpi_inst2msg(ci, HA_LOCATION_CONSTRAINT, t, rc); 
		ret = cim_update_dispatch(UPDATE_LOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	case TID_CONS_COLOCATION:
		cmpi_inst2msg(ci, HA_COLOCATION_CONSTRAINT, t, rc); 
		ret = cim_update_dispatch(UPDATE_COLOCATION_CONSTRAINT, NULL, t, NULL); 
		break;
	default: break;
	}
	rc->rc = (ret==HA_OK)? CMPI_RC_OK : CMPI_RC_ERR_FAILED;
	ha_msg_del(t);
	DEBUG_LEAVE();
	return ret;
}

