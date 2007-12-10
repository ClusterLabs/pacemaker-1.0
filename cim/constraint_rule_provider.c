/*
 * constraint_rule_provider.c: HA_LocationConstraintRule provider
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
#include "constraint_common.h"
#include "utils.h"

static const char * 	PROVIDER_ID 	= "cim-rule";
static char 		ClassName []  	= "HA_LocationConstraintRule";
static CMPIBroker * 	Broker    	= NULL;

DeclareInstanceFunctions(LocationConstraintRule);

static struct ha_msg * 
find_constraint_rule(const char * consid, const char *ruleid)
{
	struct ha_msg * constraint;
	int i, size = 0;
	struct ha_msg *rule;

	constraint = cim_query_dispatch(GET_LOCATION_CONSTRAINT, 
			consid, NULL);
	if ( constraint == NULL ) {
		return NULL;
	}

        size = cim_msg_children_count(constraint);
        for ( i = 0; i < size; i++ ) {
                const char *id;
		rule = cim_msg_child_index(constraint, i);
                id = cl_get_string(rule, "id");
		if ( strncmp(id, ruleid, MAXLEN) == 0 ) {
			rule = ha_msg_copy(rule);
			ha_msg_del(constraint);
			return rule;
		} 
	}

	ha_msg_del(constraint);
	return NULL;
}

static CMPIInstance *
constraint_rule_make_instance(CMPIObjectPath * op, char* consid, 
		const char * ruleid, struct ha_msg *rule, CMPIStatus * rc)
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
	
        snprintf(caption, MAXLEN, "LocationConstraintRule.%s", ruleid);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
        CMSetProperty(ci, "ConstraintId", consid, CMPI_chars);
	cmpi_msg2inst(Broker, ci, HA_LOCATION_CONSTRAINT_RULE, rule, rc); 
	return ci;
}

static int
constraint_rule_enum_insts(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
               CMPIObjectPath * ref, int EnumInst, CMPIStatus * rc)
{
	char *namespace;
        CMPIObjectPath * op = NULL;
	int i, j, size = 0, rulecount = 0;
	char *consid = NULL, *ruleid = NULL;
	struct ha_msg *list, *constraint, *rule = NULL;

	namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
	if ( namespace == NULL ) {
		return HA_FAIL;
	}
	list = cim_query_dispatch(GET_LOCATION_CONS_LIST, NULL, NULL);
	if ( list == NULL ) {
		goto done;
	}
	size = cim_list_length(list);
	for (i = 0; i < size; i++) {
		consid = cim_list_index(list, i);
		constraint = cim_query_dispatch(GET_LOCATION_CONSTRAINT, 
				consid, NULL);
		if ( constraint == NULL ) {
			continue;
		}
        	rulecount = cim_msg_children_count(constraint);
		for (j = 0; j < rulecount; j++){
			ruleid = cim_strdup(cim_msg_child_name(constraint, j));
	                op = CMNewObjectPath(Broker, namespace, ClassName, rc);
        	        if ( EnumInst ) {
                	        CMPIInstance * ci = NULL;
				rule = cim_msg_child_index(constraint, j);
				ci = constraint_rule_make_instance(op, consid, 
						ruleid, rule, rc);
	               	        CMReturnInstance(rslt, ci);
        	        } else { /* enumerate instance names */
       	        	        CMAddKey(op, "ConstraintId", consid, CMPI_chars);
       	                	CMAddKey(op, "Id", ruleid, CMPI_chars); 
	               	        CMReturnObjectPath(rslt, op);
        	       	}
			cim_free(ruleid);
       		}
		ha_msg_del(constraint);
	}
	ha_msg_del(list);
done:
	rc->rc = CMPI_RC_OK;
        CMReturnDone(rslt);
        return HA_OK;
}


/**********************************************
 * Instance provider functions
 **********************************************/

static CMPIStatus 
LocationConstraintRuleCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{

	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
LocationConstraintRuleEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	constraint_rule_enum_insts(mi, ctx, rslt, ref, FALSE, &rc);
	return rc;
}


static CMPIStatus 
LocationConstraintRuleEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
	constraint_rule_enum_insts(mi, ctx, rslt, ref, TRUE, &rc);
	return rc;
}

static CMPIStatus 
LocationConstraintRuleGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, char ** properties)
{
        CMPIObjectPath * op;
        CMPIInstance * ci;
        CMPIStatus rc;
	char *consid, *id;
	struct ha_msg * rule;

	PROVIDER_INIT_LOGGER();

	id	= CMGetKeyString(cop, "Id", &rc);
	consid	= CMGetKeyString(cop, "ConstraintId", &rc);

	op = CMNewObjectPath(Broker, 
		CMGetCharPtr(CMGetNameSpace(cop, &rc)), ClassName, &rc);
        if ( CMIsNullObject(op) ){
                cl_log(LOG_WARNING, "%s: can not create object path.",
			 __FUNCTION__);
        	CMReturnDone(rslt);
        	return rc;
	}

	if ((rule = find_constraint_rule(consid, id)) == NULL ) {
		cl_log(LOG_WARNING, "%s: rule %s not found.", __FUNCTION__, id);
		CMReturnDone(rslt);
		return rc;
	}
	
        ci = constraint_rule_make_instance(op, consid, id, rule, &rc);
        if ( CMIsNullObject(ci) ) {
        	CMReturnDone(rslt);
        	return rc;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
LocationConstraintRuleCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *id, *consid; 
        struct ha_msg * constraint, *rule;
        int ret;

	id	= CMGetKeyString(cop, "Id", &rc);
        consid	= CMGetKeyString(cop, "ConstraintId", &rc);
        constraint = cim_query_dispatch(GET_LOCATION_CONSTRAINT, consid, NULL);
	if (constraint == NULL ) {
		goto done;
	}

	if ( cl_get_struct(constraint, id) != NULL ) {
		cl_log(LOG_ERR, "%s: rule with id %s already exists.",
				__FUNCTION__, id);
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}	
	if ( (rule = ha_msg_new(1)) == NULL ) {
		cl_log(LOG_ERR, "%s: alloc rule failed.", __FUNCTION__);
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}

	if (cmpi_inst2msg(ci, HA_LOCATION_CONSTRAINT_RULE, rule, &rc) != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}
	ha_msg_addstruct(constraint, id, rule);			
	ret = cim_update_dispatch(UPDATE_LOCATION_CONSTRAINT, 
			NULL, constraint, NULL);
	ha_msg_del(constraint);
	if ( ret != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
	}
done:
	return rc;
}


static CMPIStatus 
LocationConstraintRuleSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		CMPIResult * rslt, CMPIObjectPath * cop, 
		CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *id, *consid;
        struct ha_msg * constraint, *rule;
        int ret;

	id	= CMGetKeyString(cop, "Id", &rc);
	consid	= CMGetKeyString(cop, "ConstraintId", &rc);

        constraint = cim_query_dispatch(GET_LOCATION_CONSTRAINT, consid, NULL);
	if (constraint == NULL ) {
		goto done;
	}
	
	if ( (rule = ha_msg_new(1)) == NULL ) {
		cl_log(LOG_ERR, "%s: alloc rule failed.", __FUNCTION__);
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}

	if (cmpi_inst2msg(ci, HA_LOCATION_CONSTRAINT_RULE, rule, &rc) != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	}
	cl_msg_remove(constraint, id);
	ha_msg_addstruct(constraint, id, rule);			
	ret = cim_update_dispatch(UPDATE_LOCATION_CONSTRAINT, 
			NULL, constraint, NULL);
	ha_msg_del(constraint);
	if ( ret != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
	}
done:
	return rc;
}


static CMPIStatus 
LocationConstraintRuleDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
		 CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	char *id, *consid; 
	struct ha_msg * constraint;
	int ret;

	id	= CMGetKeyString(cop, "Id", &rc);
        consid	= CMGetKeyString(cop, "ConstraintId", &rc);
	
	constraint = cim_query_dispatch(GET_LOCATION_CONSTRAINT, consid, NULL);
	if (constraint == NULL ) {
		goto done;
	}
	
	ret = cim_update_dispatch(DEL_LOCATION_CONSTRAINT, consid, NULL,NULL);
	if ( ret != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
		goto done;
	} 
	cl_msg_remove(constraint, id);
	ret = cim_update_dispatch(UPDATE_LOCATION_CONSTRAINT, 
			NULL, constraint, NULL);
	ha_msg_del(constraint);
	if ( ret != HA_OK ) {
		rc.rc = CMPI_RC_ERR_FAILED;
	}
done:
	return rc;
}

static CMPIStatus 
LocationConstraintRuleExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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

DeclareInstanceMI(LocationConstraintRule, HA_LocationConstraintRuleProvider, Broker);

