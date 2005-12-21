/*
 * rule_provider.c: HA_Rule provider
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

static const char * PROVIDER_ID = "cim-rule";
static CMPIBroker * G_broker    = NULL;
static char G_classname []      = "HA_Rule";

static CMPIInstance *
make_rule_instance(CMPIObjectPath * op, char * id, char * sys_name,
                   char * sys_cr_name, CMPIStatus * rc);
static int
get_inst_rule(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
              char ** properties, CMPIStatus * rc);

DeclareInstanceFunctions(Rule);
DeclareMethodFunctions(Rule);

static CMPIInstance *
make_rule_instance(CMPIObjectPath * op, char * id, char * sys_name, 
                   char * sys_cr_name, CMPIStatus * rc)
{
        struct ci_table * rules;
        struct ci_table * rule;
        char * name, * interval, * timeout ;
        CMPIInstance * ci = NULL;

        if ((rules = ci_get_inst_rules(sys_name) ) == NULL ) {
                return NULL;
        }

        if ((rule = rules->get_data(rules, id).value.table) == NULL ) {
                return NULL;
        }

        ci = CMNewInstance(G_broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);
                rules->free(rules);
                return NULL;
        }
        
        name = rule->get_data(rule, "name").value.string;
        interval = rule->get_data(rule, "interval").value.string;
        timeout = rule->get_data(rule, "timeout").value.string;

        if (name) CMSetProperty(ci, "Name", name, CMPI_chars);
        if (interval) CMSetProperty(ci, "Interval", interval, CMPI_chars);
        if (timeout) CMSetProperty(ci, "Timeout", timeout, CMPI_chars);
        
        CMSetProperty(ci, "Id", id, CMPI_chars);
        CMSetProperty(ci, "SystemName", sys_name, CMPI_chars);
        CMSetProperty(ci, "SystemCreationClassName", sys_cr_name, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", G_classname, CMPI_chars);
        CMSetProperty(ci, "Caption", id, CMPI_chars);

        rules->free(rules);
        return ci;
}

static int
get_inst_rule(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
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

        /* rule's id */
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

        ci = make_rule_instance(op, id, sys_name, sys_cr_name, rc);

        if ( CMIsNullObject(ci) ) {
                return HA_FAIL;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        return HA_OK;
}

/**********************************************
 * Instance provider functions
 **********************************************/

static CMPIStatus 
RuleCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{

	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
RuleEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
RuleEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                       CMPIResult * rslt, CMPIObjectPath * ref,
                       char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
RuleGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                     CMPIResult * rslt, CMPIObjectPath * cop,
                     char ** properties)
{

        CMPIStatus rc;
        if ( get_inst_rule(ctx, rslt, cop, properties, &rc) == HA_FAIL ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);


}

static CMPIStatus 
RuleCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
RuleSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                     CMPIResult * rslt, CMPIObjectPath * cop,
                     CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
RuleDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
RuleExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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
RuleInvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx,
                      CMPIResult * rslt, CMPIObjectPath * ref,
                      const char * method, CMPIArgs * in, CMPIArgs * out)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;    
}


static CMPIStatus 
RuleMethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install provider
 ****************************************************/

DeclareInstanceMI(Rule, HA_RuleProvider, G_broker);
DeclareMethodMI(Rule, HA_RuleProvider, G_broker);

