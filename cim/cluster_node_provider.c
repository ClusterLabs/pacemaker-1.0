/*
 * cluster_node_provider.c: HA_ClusterNode provider
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
#include <hb_api.h>
#include "cmpi_utils.h"
#include "cluster_info.h"

static const char * 	PROVIDER_ID  = "cim-node";
static CMPIBroker * 	Broker       = NULL;
static char 		ClassName [] = "HA_ClusterNode";

static CMPIInstance *   make_node_instance(CMPIObjectPath * op, char * uname, 
				CMPIStatus * rc);
static int              node_get_inst(CMPIContext * ctx, CMPIResult * rslt, 
				CMPIObjectPath * cop, char ** properties,  
				CMPIStatus * rc);
static int              node_enum_insts(CMPIInstanceMI * mi, CMPIContext * ctx, 
				CMPIResult * rslt, CMPIObjectPath * ref, 
				int EnumInst, CMPIStatus * rc);
static int 		cleanup_node(void);

DeclareInstanceFunctions(ClusterNode);

static CMPIInstance *
make_node_instance(CMPIObjectPath * op, char * uname, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;
        char caption[MAXLEN];
	struct ha_msg *nodeinfo;

        ci = CMNewInstance(Broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                CMSetStatusWithChars(Broker, rc, 
			CMPI_RC_ERR_FAILED, "Create instance failed");
                return NULL;
        }

        snprintf(caption, MAXLEN, "Node.%s", uname);
        CMSetProperty(ci, "CreationClassName", ClassName, CMPI_chars);
        CMSetProperty(ci, "Name", uname, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);

	nodeinfo = cim_query_dispatch(GET_NODE_INFO, uname, NULL);
        if(nodeinfo){	
		cmpi_msg2inst(Broker, ci, HA_CLUSTER_NODE, nodeinfo, rc); 
		ha_msg_del(nodeinfo);
	}
        return ci;
}

static int
node_get_inst(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
              char ** properties,  CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIString * tmp;
        char * uname = NULL;
        CMPIInstance * ci = NULL;

        tmp = CMGetKey(cop, "Name", rc).value.string;
	if ( tmp == NULL ) {
		cl_log(LOG_ERR, "node_get_inst: Failed to get key:Name");
		return HA_FAIL;
	}
        uname = CMGetCharPtr(tmp);
        op = CMNewObjectPath(Broker, CMGetCharPtr(CMGetNameSpace(cop, rc)), 
			ClassName, rc);

        if ( CMIsNullObject(op) ){
		cl_log(LOG_ERR, "node_get_inst: Alloc ObjectPath failed.");
        	return HA_FAIL;
	}
        
        ci = make_node_instance(op, uname, rc);
        if ( CMIsNullObject(ci) ) {
        	return HA_FAIL;
	}

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
       	return HA_OK; 
}

static int
node_enum_insts(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
               CMPIObjectPath * ref, int EnumInst, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
	int hbstatus;
        int i = 0, len;
	struct ha_msg *nodes;
	char *nspace;

	if ((hbstatus = cim_get_hb_status()) != HB_RUNNING ) {
		CMReturnDone(rslt);
		return HA_OK;
	}
	nspace = CMGetCharPtr(CMGetNameSpace(ref, rc));
	if ( nspace == NULL ) {
		return HA_FAIL;
	}

	nodes = cim_query_dispatch(GET_NODE_LIST, NULL, NULL); 
        if ( nodes == NULL ) {
                cl_log(LOG_ERR, "%s: Can not get node list.", __FUNCTION__); 
                CMSetStatusWithChars(Broker, rc,
                       CMPI_RC_ERR_FAILED, "Could not get node list.");
                return HA_FAIL;
        }
	len = cim_list_length(nodes);
        for (i = 0; i < len; i++) {
                char *uname;
		if ((uname = cim_list_index(nodes, i)) == NULL ) {
			continue;
		}
                /* create an object */
                op = CMNewObjectPath(Broker, nspace, ClassName, rc);
		if (op == NULL ){
			ha_msg_del(nodes);
			return HA_FAIL;
		}
                if ( EnumInst ) {
                        /* enumerate instances */
                        CMPIInstance * ci = NULL;
                        if ((ci = make_node_instance(op, uname, rc))==NULL) {
				rc->rc = CMPI_RC_ERR_FAILED;
				ha_msg_del(nodes);
                                return HA_FAIL;
                        }
                        CMReturnInstance(rslt, ci);
                } else { /* enumerate instance names */
                        CMAddKey(op, "CreationClassName", ClassName, CMPI_chars);
                        CMAddKey(op, "Name", uname, CMPI_chars);
                        CMReturnObjectPath(rslt, op);
                }
        }

        CMReturnDone(rslt);
	ha_msg_del(nodes);
        return HA_OK;
}

static int
cleanup_node () 
{
        cl_log(LOG_INFO, "%s: clean up", __FUNCTION__);
        return HA_OK;
}

/**********************************************
 * Instance provider functions
 **********************************************/

static CMPIStatus 
ClusterNodeCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        cleanup_node();
	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ClusterNodeEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * ref)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        node_enum_insts(mi, ctx, rslt, ref, FALSE, &rc);
	return rc;
}


static CMPIStatus 
ClusterNodeEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        node_enum_insts(mi, ctx, rslt, ref, TRUE, &rc);
	return rc;
}

static CMPIStatus 
ClusterNodeGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
	node_get_inst(ctx, rslt, cop, properties, &rc);
	return rc;
}

static CMPIStatus 
ClusterNodeCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop,
                           CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
ClusterNodeSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
ClusterNodeDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
ClusterNodeExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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

DeclareInstanceMI(ClusterNode, HA_ClusterNodeProvider, Broker);
