/*
 * CIM Provider
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

#include <glib.h>
#include <clplumbing/cl_malloc.h>

#include "linuxha_info.h"
#include "cmpi_resource.h"
#include "cmpi_utils.h"
#include "ha_resource.h"

static char system_name []     = "LinuxHACluster";
static char system_creation [] = "LinuxHA_Cluster";

static CMPIInstance *
make_resource_instance(char * classname, CMPIBroker * broker, 
                       CMPIObjectPath * op, char * rsc_name, 
                       CMPIStatus * rc, GNode * node);

/* crm_resource -W -r requires group id */
static char * get_res_whole_name ( GNode * node, const char * rsc_name);


static char *
get_res_whole_name ( GNode * node, const char * rsc_name)
{
        int max_len = 256;
        char long_name [max_len];
        char tmp [max_len];
        GNode * parent = NULL;
        struct res_node_data * node_data = NULL;
        struct cluster_resource_group_info * info = NULL;


        cl_log(LOG_INFO, "%s: rsc_name = %s, rsc_name in node = %s",
               __FUNCTION__, rsc_name, GetResourceInfo(GetResNodeData(node))->name);

        parent = node->parent;
        strcpy(long_name, rsc_name);

        while ( parent != NULL && parent->data != NULL ) {
                node_data = (struct res_node_data *) parent->data;
                 
                ASSERT (node_data->type == GROUP );

                info = (struct cluster_resource_group_info *)node_data->res;

                if ( strlen(long_name) + strlen(info->id) + 1 >= max_len ) {
                        cl_log(LOG_INFO, "%s: exceed max length", __FUNCTION__);
                        return NULL;
                }
                
                strcpy(tmp, long_name);
                strcpy(long_name, info->id);
                strcat(long_name, ":");
                strcat(long_name, tmp);

                parent = parent->parent;
        }

        return strdup(long_name);
}


static CMPIInstance *
make_resource_instance(char * classname, CMPIBroker * broker, 
                       CMPIObjectPath * op, char * rsc_name, 
                       CMPIStatus * rc, GNode * node)
{
        CMPIInstance * ci = NULL;
        char * hosting_node = NULL;
        struct cluster_resource_info * info = NULL;
        struct res_node_data * node_data = NULL;
        char * whole_name = NULL;
        
        DEBUG_ENTER();

        cl_log(LOG_INFO, "%s: make instance for %s", __FUNCTION__, rsc_name);

        
        node_data = (struct res_node_data *) node->data;

        if ( node_data == NULL || node_data->res == NULL ) {
                return NULL;
        }

        info = (struct cluster_resource_info *)node_data->res;

        ASSERT(info);

        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can't create instance", __FUNCTION__);

	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get create instance");
                goto out;
        }
 
        cl_log(LOG_INFO, "%s: setting properties", __FUNCTION__);

        /* setting properties */

        CMSetProperty(ci, "Type", info->type, CMPI_chars);
        CMSetProperty(ci, "ResourceClass", info->class, CMPI_chars);
        CMSetProperty(ci, "Name", info->name, CMPI_chars);
       

        whole_name = get_res_whole_name( node, rsc_name );
        hosting_node = whole_name ? 
                get_hosting_node(whole_name) : NULL;

        cl_log(LOG_INFO, "%s: hosting_node of %s is %s", __FUNCTION__, 
               rsc_name, hosting_node? hosting_node: "(NULL)");

        if ( hosting_node ){
                char status [] = "Running";
                cl_log(LOG_INFO, "Hosting node is %s", hosting_node);

                CMSetProperty(ci, "Status", status, CMPI_chars);
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        } else {
                char status [] = "Not running";
                
               /* OpenWBEM will segment fault in HostedResource provider 
                  if "HostingNode" not set */

                hosting_node = strdup ("Unknown");
                CMSetProperty(ci, "Status", status, CMPI_chars);
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        }


        /* set other key properties inherited from super classes */
        CMSetProperty(ci, "SystemCreationClassName", system_creation, CMPI_chars);
        CMSetProperty(ci, "SystemName", system_name, CMPI_chars);
        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);
        
        free (whole_name);
        free (hosting_node);

out:
        DEBUG_LEAVE();
        return ci;
}

int
get_resource_instance(char * classname, CMPIBroker * broker, CMPIContext * ctx,
                      CMPIResult * rslt, CMPIObjectPath * cop,
                      char ** properties, CMPIStatus * rc)
{
        CMPIInstance* ci = NULL;
        CMPIObjectPath* op = NULL;
        CMPIData data_name;
        char * rsc_name = NULL;
        int ret = 0;
        GNode * root = NULL;
        GNode * node_found = NULL;
        
        DEBUG_ENTER();

        data_name = CMGetKey(cop, "Name", rc);

        if ( data_name.value.string == NULL ) {
                cl_log(LOG_WARNING, "key %s is NULL", "Name");
                ret = HA_FAIL;
                goto out;
        }

        rsc_name = CMGetCharPtr(data_name.value.string);

        cl_log(LOG_INFO, "rsc_name = %s", rsc_name);
 
        op = CMNewObjectPath(broker, 
                        CMGetCharPtr(CMGetNameSpace(cop, rc)), classname, rc);

        if ( CMIsNullObject(op) ){
                ret = HA_FAIL;
                cl_log(LOG_WARNING, 
                        "%s: can not create object path.", __FUNCTION__);
                goto out;
        }

        /* get resource tree */
        root = get_res_tree ();

        if ( root  == NULL ) {
                cl_log(LOG_ERR, "%s: failed to get resource tree", __FUNCTION__);

	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Failed to get resource tree");
                goto out;
        }


        /* search for node */
        node_found = search_res_in_tree(root, rsc_name, RESOURCE);

        if ( node_found == NULL ) {
                cl_log(LOG_WARNING, "%s: resource %s not found!",
                                __FUNCTION__, rsc_name);

	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_NOT_FOUND, "Resource not found");

                goto out;
        }

        /* make instance according to the node found */
        ci = make_resource_instance(classname, broker, op, rsc_name, 
                                    rc, node_found);

        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                cl_log(LOG_WARNING, 
                        "%s: can not create instance.", __FUNCTION__);
                goto out;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        ret = HA_OK;
out:
        if ( root ) {
                free_res_tree ( root );
        }

        DEBUG_LEAVE();
        return ret;
}

int 
enumerate_resource_instances(char * classname, CMPIBroker * broker,
                             CMPIContext * ctx, CMPIResult * rslt,
                             CMPIObjectPath * ref, int enum_inst, 
                             CMPIStatus * rc)
{

        char * namespace = NULL;
        GNode * root = NULL;

        CMPIObjectPath * op = NULL;
        GList * list = NULL;
        GList * current = NULL;

        /* get resource tree */
        root = get_res_tree ();
        
        if ( root == NULL ) {
                cl_log(LOG_ERR, "%s: can't get resource tree", __FUNCTION__);
                return HA_FAIL;
        }
        
        /* build list on tree */
        list = build_res_list ( root );

        if ( list == NULL ) {
                cl_log(LOG_ERR, "%s: failed to build resource list", 
                       __FUNCTION__);
                free_res_tree ( root );
                return HA_FAIL;
        } 
                
        cl_log(LOG_INFO, "%s: resource list built, %d elements",
               __FUNCTION__, g_list_length(list) );

        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));

        op = CMNewObjectPath(broker, namespace, classname, rc);

        if ( CMIsNullObject(op) ){
                free_res_tree ( root );
                return HA_FAIL;
        }
        

        /* go through the list for enumerating resource */
        for ( current = list; current;) {
                GNode * node = NULL;
                struct res_node_data * node_data = NULL;
                struct cluster_resource_info * info = NULL;
                

                node = (GNode *)current->data;
                if ( node == NULL ) {
                        goto next;
                }

                node_data = (struct res_node_data *) node->data;
                if ( node_data == NULL || node_data->type == GROUP) {
                        goto next;
                }

                cl_log(LOG_INFO, "%s: res type %d", __FUNCTION__, node_data->type);
                info = (struct cluster_resource_info *)node_data->res;

                /* create an object */
                CMAddKey(op, "Name", info->name, CMPI_chars);

                /* add keys inherited from super classes */
                CMAddKey(op, "SystemName", system_name, CMPI_chars);
                CMAddKey(op, "SystemCreationClassName", system_creation, CMPI_chars);
                CMAddKey(op, "CreationClassName", classname, CMPI_chars);


                if ( enum_inst ) {
                        CMPIInstance * ci = NULL;
                        ci = make_resource_instance(classname, broker, op, 
                                                    info->name, rc, node); 

                        if ( CMIsNullObject(ci) ){
                                cl_log(LOG_WARNING, 
                                   "%s: can not make instance", __FUNCTION__);
                                return HA_FAIL;
                        }
                        
                        cl_log(LOG_INFO, "%s: return instance", __FUNCTION__);
                        CMReturnInstance(rslt, ci);
                } else {
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }

        next:
                current = current->next;
                if ( current == list ) {
                        break;
                }

        }

        if ( root ) {
                free_res_tree ( root );
        }

        /* TODO: free list */

        CMReturnDone(rslt);
        return HA_OK;
}


