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

static const char * PROVIDER_ID = "cim-node";
static CMPIBroker * G_broker    = NULL;
static char G_classname []      = "HA_ClusterNode";

static CMPIInstance *
make_node_instance(CMPIObjectPath * op, char * uname, CMPIStatus * rc);
static int
get_inst_node(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
              char ** properties,  CMPIStatus * rc);
static int
enum_inst_node(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
               CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc);
static int cleanup_node(void);
DeclareInstanceFunctions(ClusterNode);

static CMPIInstance *
make_node_instance(CMPIObjectPath * op, char * uname, CMPIStatus * rc)
{
        struct ci_table * nodeinfo = NULL;
        CMPIInstance * ci = NULL;
        char * active_status, * dc = NULL, * uuid = NULL;
        char caption [256];

        ci = CMNewInstance(G_broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                CMSetStatusWithChars(G_broker, rc, CMPI_RC_ERR_FAILED, 
                                     "Create instance failed");

                return NULL;
        }

        if ( ( nodeinfo = ci_get_nodeinfo(uname))== NULL ) {
                cl_log(LOG_ERR, "%s: could not get node info", __FUNCTION__);
                CMSetStatusWithChars(G_broker, rc, CMPI_RC_ERR_FAILED, 
                                     "Could not get node information");
                return NULL;
        }       

        active_status = CITableGet(nodeinfo, "active_status").value.string;
        if(active_status) CMSetProperty(ci, "ActiveStatus", active_status, CMPI_chars);

        if ( active_status && strcmp (active_status, "True") == 0) {
                char * standby, * unclean, * shutdown, * expected_up, * node_ping;
                standby = CITableGet(nodeinfo, "standby").value.string;
                unclean = CITableGet(nodeinfo, "unclean").value.string;
                shutdown = CITableGet(nodeinfo, "shutdown").value.string;
                expected_up = CITableGet(nodeinfo, "expected_up").value.string;
                node_ping = CITableGet(nodeinfo, "node_ping").value.string;

                if (standby) CMSetProperty(ci, "Standby", standby, CMPI_chars);
                if (unclean) CMSetProperty(ci, "Unclean", unclean, CMPI_chars);
                if (shutdown) CMSetProperty(ci, "Shutdown", shutdown, CMPI_chars);
                if (expected_up) CMSetProperty(ci, "ExpectedUp", expected_up, CMPI_chars);
                if (node_ping) CMSetProperty(ci, "NodePing", node_ping, CMPI_chars);
        }

        dc = ci_get_cluster_dc();
        uuid = CIM_STRDUP("N/A");
        
        sprintf(caption, "Node.%s", uname);
        /* setting properties */
        CMSetProperty(ci, "CreationClassName", G_classname, CMPI_chars);
        CMSetProperty(ci, "Name", uname, CMPI_chars);
        CMSetProperty(ci, "UUID", uuid, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);

        if ( dc ) {
                if ( strncmp(dc, uname, strlen(uname)) == 0){
                        char dc_status[] = "True";
                        CMSetProperty(ci, "IsDC", dc_status, CMPI_chars); 
                } else {
                        char dc_status[] = "False";
                        CMSetProperty(ci, "IsDC", dc_status, CMPI_chars); 
                
                }
        }

        if ( dc ) {
                CIM_FREE(dc);
        }
        CIM_FREE(uuid);
        return ci;

}

static int
get_inst_node(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
              char ** properties,  CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIData data_uname;
        char * uname = NULL;
        CMPIInstance * ci = NULL;
        int ret = 0;

        data_uname = CMGetKey(cop, "Name", rc);
        uname = CMGetCharPtr(data_uname.value.string);

        op = CMNewObjectPath(G_broker, 
                CMGetCharPtr(CMGetNameSpace(cop, rc)), G_classname, rc);

        if ( CMIsNullObject(op) ){
                ret = HA_FAIL;
                goto out;
        }
        
        ci = make_node_instance(op, uname, rc);
        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                goto out;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        ret = HA_OK;
        
out:
        return ret; 
}

static int
enum_inst_node(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
               CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        GPtrArray * nodeinfo_table = NULL;

        int i = 0;

        nodeinfo_table = ci_get_node_name_table ();
        
        if ( nodeinfo_table == NULL ) {
                cl_log(LOG_ERR, 
                       "%s: could not get node information", 
                       __FUNCTION__);

                CMSetStatusWithChars(G_broker, rc,
                       CMPI_RC_ERR_FAILED, "Could not get node information");

                return HA_FAIL;
        }

        for (i = 0; i < nodeinfo_table->len; i++) {
                char * uname = NULL;
                char * space = NULL;
                uname = (char *) g_ptr_array_index(nodeinfo_table, i);
                
                space = CMGetCharPtr(CMGetNameSpace(ref, rc));
                if ( space == NULL ) {
                        ci_free_ptr_array(nodeinfo_table, CIM_FREE);
                        return HA_FAIL;
                }

                /* create an object */
                op = CMNewObjectPath(G_broker, space, G_classname, rc);
                if ( enum_inst ) {
                        /* enumerate instances */
                        CMPIInstance * ci = NULL;
                        ci = make_node_instance(op, uname, rc);
                        if ( CMIsNullObject(ci) ) {
                                ci_free_ptr_array(nodeinfo_table, CIM_FREE);
                                return HA_FAIL;
                        }

                        CMReturnInstance(rslt, ci);

                } else { /* enumerate instance names */
                        CMAddKey(op, "CreationClassName", G_classname, CMPI_chars);
                        CMAddKey(op, "Name", uname, CMPI_chars);
                        /* add object path to rslt */
                        CMReturnObjectPath(rslt, op);
                }

        }

        CMReturnDone(rslt);
        ci_free_ptr_array(nodeinfo_table, CIM_FREE);

        return HA_OK;
}

static int
cleanup_node () {

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
	
        init_logger( PROVIDER_ID );
	if ( enum_inst_node(mi, ctx, rslt, ref, 0, &rc) == HA_OK ) {
	        CMReturn(CMPI_RC_OK);	
        } else {
                return rc;
        }
}


static CMPIStatus 
ClusterNodeEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                          CMPIResult * rslt, CMPIObjectPath * ref,
                          char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	
        init_logger( PROVIDER_ID );
        if ( enum_inst_node(mi, ctx, rslt, ref, 1, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);	
        } else {
                return rc;
        }
}

static CMPIStatus 
ClusterNodeGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger( PROVIDER_ID );

	if ( get_inst_node(ctx, rslt, cop, properties, &rc) != HA_OK ) {
                cl_log(LOG_WARNING, "%s: NULL instance", __FUNCTION__);
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ClusterNodeCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop,
                           CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
ClusterNodeSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                        CMPIResult * rslt, CMPIObjectPath * cop,
                        CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
ClusterNodeDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
ClusterNodeExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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
ClusterNodeInvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx,
                         CMPIResult * rslt, CMPIObjectPath * ref,
                         const char * method, CMPIArgs * in, CMPIArgs * out)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
	return rc;    
}


static CMPIStatus 
ClusterNodeMethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install provider
 ****************************************************/

DeclareInstanceMI(ClusterNode, HA_ClusterNodeProvider, G_broker);
DeclareMethodMI(ClusterNode, HA_ClusterNodeProvider, G_broker);

