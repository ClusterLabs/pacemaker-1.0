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

#include <clplumbing/cl_malloc.h>

#include "linuxha_info.h"
#include "cmpi_resource.h"
#include "cmpi_utils.h"

/* temporary implementaion */        


typedef struct cluster_resource_info_s {
        char *  name;
        char *  type;
        char *  provider;
        char *  class;
} cluster_resource_info;

static char * get_hosting_node(const char * name);

static int
get_resource_infos(GPtrArray * info_array)
{
        char ** std_out = NULL;
        char cmnd[] = HA_LIBDIR"/heartbeat/crmadmin -R";
        int exit_code;
        int ret, i;

        DEBUG_ENTER();
        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);

        for (i = 0; std_out[i]; i++){
               char ** match = NULL;

                ret = regex_search("primitive: (.*) \\((.*)::(.*)\\)", 
                                        std_out[i], &match);

                if (ret == HA_OK && match[0]) {
                        cluster_resource_info * info = NULL;
                        info = malloc(sizeof(cluster_resource_info));

                        /* parse the result */
                        info->class = strdup(match[3]);
                        info->type = strdup(match[2]);
                        info->name = strdup(match[1]);

                        cl_log(LOG_INFO, "%s: add resource info for %s", 
                                        __FUNCTION__, info->name);
                        g_ptr_array_add(info_array, info);

                        /* free memory allocated */
                        free_2d_array(match);
                         
                }
        }

        free_2d_array(std_out);

        DEBUG_LEAVE();

        return HA_OK;
}


static CMPIInstance *
make_resource_instance(char * classname, CMPIBroker * broker, 
                CMPIObjectPath * op, char * rsc_name, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;
        char * hosting_node = NULL;
        GPtrArray * info_array = NULL;
        cluster_resource_info * info = NULL;
        int ret = 0;
        int i = 0;


        DEBUG_ENTER();

        cl_log(LOG_INFO, "%s: make instance for %s", __FUNCTION__, rsc_name);

        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                ci = NULL;
                goto out;
        }
         
        info_array = g_ptr_array_new();
        if ( info_array == NULL ) {
                ci = NULL;
                goto out;
        } 

        cl_log(LOG_INFO, "%s: get_resource_infos", __FUNCTION__);
        ret = get_resource_infos(info_array);

        if ( ret != HA_OK ) {
                ci = NULL;
                goto out;
        }

        cl_log(LOG_INFO, "%s: got resource infos", __FUNCTION__);

        ASSERT(info_array);

        for ( i = 0; i < info_array->len; i++ ) {

                info = (cluster_resource_info *)
                        g_ptr_array_index(info_array, i); 

                
                cl_log(LOG_INFO, "%s: looking for [%s], rsc @ %d = [%s]",
                        __FUNCTION__, rsc_name, i, info->name);
                
                if ( strcmp(info->name, rsc_name) == 0 ) {
                        cl_log(LOG_INFO, "%s: found!", __FUNCTION__);
                        break;
                } 
        }


        if ( i == info_array->len ) {
                cl_log(LOG_WARNING, "%s: can not find resource: %s",
                                __FUNCTION__, rsc_name);
                ci = NULL;
                goto out;
        }

        ASSERT(info);

        cl_log(LOG_INFO, "%s: setting properties", __FUNCTION__);

        /* setting properties */

        CMSetProperty(ci, "Type", info->type, CMPI_chars);

        CMSetProperty(ci, "ResourceClass", info->class, CMPI_chars);
        CMSetProperty(ci, "Name", info->name, CMPI_chars);
       
        hosting_node = get_hosting_node(rsc_name);

        cl_log(LOG_INFO, "%s: hosting_node of %s is %s", __FUNCTION__, 
                                        rsc_name, hosting_node);

        if (hosting_node){
                char status [] = "Running";
                cl_log(LOG_INFO, "Hosting node is %s", hosting_node);

                CMSetProperty(ci, "Status", status, CMPI_chars);
                CMSetProperty(ci, "HostingNode", hosting_node, CMPI_chars);
        }else{
                char status [] = "Not running";
                CMSetProperty(ci, "Status", status, CMPI_chars);
        }

        g_ptr_array_free(info_array, 0);

out:
        DEBUG_LEAVE();
        return ci;
}

int 
enumerate_resource_instances(char * classname, CMPIBroker * broker,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc)
{

        CMPIObjectPath * op = NULL;
        char * namespace = NULL;
        int ret, i;
        GPtrArray * info_array = NULL;

        
        info_array = g_ptr_array_new(); 
        if ( info_array == NULL ) {
                return HA_FAIL;
        } 

        cl_log(LOG_INFO, "%s: get_resource_infos", __FUNCTION__);
        ret = get_resource_infos(info_array);

        if ( ret != HA_OK ) {
                return HA_FAIL;
        }
        
        namespace = CMGetCharPtr(CMGetNameSpace(ref, rc));
        op = CMNewObjectPath(broker, namespace, classname, rc);

        if ( CMIsNullObject(op) ){
                return HA_FAIL;
        }

        cl_log(LOG_INFO, "%s: begin to enumerate", __FUNCTION__);

        for (i = 0; i < info_array->len; i++){
                cluster_resource_info * info = NULL;
           
                info = (cluster_resource_info *)
                        g_ptr_array_index(info_array, i); 

                /* create an object */
                CMAddKey(op, "Name", info->name, CMPI_chars);
                if ( enum_inst ) {
                        CMPIInstance * ci = NULL;
                        ci = make_resource_instance(classname, 
                                        broker, op, info->name, rc); 

                        if ( CMIsNullObject(ci) ){
                                cl_log(LOG_WARNING, 
                                        "%s: can not make instance", 
                                        __FUNCTION__);
                                return HA_FAIL;
                        }
                        
                        CMReturnInstance(rslt, ci);
                } else {
        
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                
                }
        }
        
        cl_log(LOG_INFO, "%s: return done!", __FUNCTION__);
        CMReturnDone(rslt);

        g_ptr_array_free(info_array, 0);
        return HA_OK;
}


static char * 
get_hosting_node(const char * name)
{
        char cmnd_pat[] = HA_LIBDIR"/heartbeat/crmadmin -W";
        int lenth, i;
        char * cmnd = NULL;
        char ** std_out = NULL;
        int exit_code, ret;
        char * node = NULL;

        lenth = strlen(cmnd_pat) + strlen(name) + 1;

        cmnd = malloc(lenth);
        sprintf(cmnd, "%s %s", cmnd_pat, name);

        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);

        for (i = 0; std_out[i]; i++){
                char ** match;
                ret = regex_search("running on: (\\w*)", std_out[i], &match);

                if (ret == HA_OK && match[0]) {
                        node = strdup(match[1]);
                        free_2d_array(match);
                } else{
                        node = NULL;
                }
        }

        free_2d_array(std_out);
        free(cmnd);        
        return node;

}

int
get_resource_instance(char * classname,
                CMPIBroker * broker,
                CMPIContext * ctx,
                CMPIResult * rslt,
                CMPIObjectPath * cop,
                char ** properties,
                CMPIStatus * rc)
{
        CMPIInstance* ci = NULL;
        CMPIObjectPath* op = NULL;
        CMPIData data_name;
        char * rsc_name = NULL;
        int ret = 0;

        
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

        ci = make_resource_instance(classname, broker, op, rsc_name, rc);

        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                cl_log(LOG_WARNING, 
                        "%s: can not create instance.", __FUNCTION__);
                goto out;
        }

        CMReturnInstance(rslt, ci);

        cl_log(LOG_INFO, "%s: return done!", __FUNCTION__);
        CMReturnDone(rslt);

        ret = HA_OK;
out:
        DEBUG_LEAVE();
        return ret;
}


