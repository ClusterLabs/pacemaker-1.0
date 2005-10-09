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

#include <sys/types.h>
#include <unistd.h>
#include <glib.h>

#include <clplumbing/cl_malloc.h>

#include "linuxha_info.h"
#include "cmpi_node.h"
#include "cmpi_utils.h"


#define HB_CLIENT_ID "cim-provider-node"

static char * get_cluster_dc(void);
static char * get_node_status(const char * node);
static GPtrArray * get_nodeinfo_table(void);
static int free_nodeinfo_table(GPtrArray * nodeinfo_table);
static struct hb_nodeinfo * hb_nodeinfo_dup(const struct hb_nodeinfo * info);
static int hb_nodeinfo_free(struct hb_nodeinfo * node_info);


static CMPIInstance * make_node_instance(char * classname, 
        CMPIBroker * broker, CMPIObjectPath * op, 
        char * uname, CMPIStatus * rc);


static char *
get_cluster_dc ()
{
        char ** std_out = NULL;
        char cmnd [] = HA_LIBDIR"/heartbeat/crmadmin -D";
        int ret, i;
        int exit_code;        
        char *dc = NULL;

        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);

        for (i = 0; std_out[i]; i++){

                char ** match;        
                ret = regex_search(": (.*)", std_out[i], &match);
                if (ret == HA_OK){
                        dc = strdup(match[1]);               
                        break;
                }
        }

        free_2d_array(std_out);

        cl_log(LOG_INFO, "%s: DC is %s", __FUNCTION__, dc);
        return dc;
}

static char *
get_node_status(const char * node)
{
        char ** std_out = NULL;
        char cmnd_pat [] = HA_LIBDIR"/heartbeat/crmadmin -S";
        char *cmnd = NULL;
        int ret, i;
        int length = 0;
        int exit_code = 0;        
        char * status = NULL;

        length = strlen(cmnd_pat) + strlen(node) + 1;

        cmnd = malloc(length);
        sprintf(cmnd, "%s %s", cmnd_pat, node);

        cl_log(LOG_INFO, "%s: run %s", __FUNCTION__, cmnd);
        
        /*
        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);
        */

        if ( std_out == NULL ) {
                exit_code = 0;
                free(cmnd);
                return NULL;
        }

        cl_log(LOG_INFO, "%s: looking for status", __FUNCTION__);

        for (i = 0; std_out[i]; i++){
                char ** match = NULL;        

                ret = regex_search(": (.*) ", std_out[i], &match);
                if (ret == HA_OK){
                        status = strdup(match[1]);               
                        break;
                }
        }

        cl_log(LOG_INFO, "%s: status of %s is %s", 
                                __FUNCTION__, node, status);

        free_2d_array(std_out);
        free(cmnd);

        return status;
}

static struct hb_nodeinfo *
hb_nodeinfo_dup(const struct hb_nodeinfo * info)
{
        struct hb_nodeinfo * dup_info = NULL;

        dup_info = (struct hb_nodeinfo *)malloc(sizeof(struct hb_nodeinfo));

        if ( dup_info == NULL ) {
                return NULL;
        }

        dup_info->id = info->id;
        dup_info->type = info->type;
        dup_info->status = info->status;
        dup_info->ifcount = info->ifcount;
        dup_info->uuid = info->uuid;
        
        dup_info->name = strdup(info->name);
        
        return dup_info;        
}

static int
hb_nodeinfo_free(struct hb_nodeinfo * node_info)
{
        free(node_info->name);
        free(node_info);
        
        return HA_OK;
}


static GPtrArray *
get_nodeinfo_table ()
{
        GPtrArray * nodeinfo_table = NULL;
        GPtrArray * hb_info = NULL;
        struct hb_nodeinfo * node_info = NULL;

        int ret = 0;
        int i = 0;

        DEBUG_ENTER();

        nodeinfo_table = g_ptr_array_new ();

        if ( nodeinfo_table == NULL ) {
                cl_log(LOG_ERR, "%s: can not create array", __FUNCTION__);

                DEBUG_LEAVE();
                return NULL;
        }


        if ( ! get_hb_initialized() ) {
                ret = linuxha_initialize(HB_CLIENT_ID, 0);

                if (ret != HA_OK ) {
                        cl_log(LOG_ERR, 
                           "%s: can not initialize heartbeat", __FUNCTION__);

                        DEBUG_LEAVE();
                        return NULL;
                }
        }

        /* get info from linuxha_info.c */
        hb_info = get_hb_info(LHA_NODEINFO); 

        for ( i = 0; i < hb_info->len; i++ ) {
                node_info = hb_nodeinfo_dup ( (struct hb_nodeinfo *) 
                                        g_ptr_array_index(hb_info, i) );
                if ( node_info == NULL ) {
                        cl_log(LOG_WARNING, 
                                "%s: get NULL, continue", __FUNCTION__);
                        continue;
                }
                g_ptr_array_add(nodeinfo_table, node_info);
        }

        if ( get_hb_initialized() ) {
                linuxha_finalize();
        }

        DEBUG_LEAVE();
        return nodeinfo_table;
}

static int 
free_nodeinfo_table(GPtrArray * nodeinfo_table)
{
        struct hb_nodeinfo * node_info = NULL;

        while (nodeinfo_table->len) {

                node_info = (struct hb_nodeinfo *)
                        g_ptr_array_remove_index_fast(nodeinfo_table, 0);
                hb_nodeinfo_free(node_info);
                node_info = NULL;

        }

        g_ptr_array_free(nodeinfo_table, 0);

        return HA_OK;
}       


static CMPIInstance *
make_node_instance(char * classname, CMPIBroker * broker, 
                CMPIObjectPath * op, char * uname, CMPIStatus * rc)
{
        GPtrArray * nodeinfo_table = NULL;
        struct hb_nodeinfo * nodeinfo = NULL;
        CMPIInstance* ci = NULL;
        char * dc = NULL;
        char * status = NULL;
        char * active_status = NULL;
        char * uuid = NULL;
        int i;


        DEBUG_ENTER();


        nodeinfo_table = get_nodeinfo_table();
        
        if ( nodeinfo_table == NULL ) {

                cl_log(LOG_ERR, "%s: can not get node info", __FUNCTION__);

                CMSetStatusWithChars(broker, rc,
                       CMPI_RC_ERR_FAILED, "Can't get node info");

                return NULL;
        }       

        for (i = 0; i < nodeinfo_table->len; i++) {
                int length = 0;

                nodeinfo = (struct hb_nodeinfo *) 
                                g_ptr_array_index(nodeinfo_table, i);

                length = strlen ( nodeinfo->name ) + 1;

                if ( strncmp(nodeinfo->name, uname, length) == 0 ){
                        break;
                }
        }

        if ( i == nodeinfo_table->len){

                cl_log(LOG_WARNING, 
                        "%s: %s is not a valid cluster node", 
                        __FUNCTION__, uname); 

                CMSetStatusWithChars(broker, rc,
                       CMPI_RC_ERR_NOT_FOUND, "Node not found");

                goto out;
        }

        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                CMSetStatusWithChars(broker, rc,
                       CMPI_RC_ERR_FAILED, "Can't create instance");

                goto out;
        }

        dc = get_cluster_dc();
        status = get_node_status(uname);

        active_status = strdup(get_status(nodeinfo->status));
        uuid = uuid_to_str(&nodeinfo->uuid);

        /* setting properties */

        cl_log(LOG_INFO, "%s: setting properties", __FUNCTION__);

        CMSetProperty(ci, "CreationClassName", classname, CMPI_chars);
        CMSetProperty(ci, "Name", uname, CMPI_chars);

        CMSetProperty(ci, "UUID", uuid, CMPI_chars);
        CMSetProperty(ci, "Status", status, CMPI_chars);
        CMSetProperty(ci, "ActiveStatus", active_status, CMPI_chars);

        if ( dc ) {
                if ( strncmp(dc, uname, strlen(uname)) == 0){
                        char dc_status[] = "Yes";
                        CMSetProperty(ci, "IsDC", dc_status, CMPI_chars); 
                } else {
                        char dc_status[] = "No";
                        CMSetProperty(ci, "IsDC", dc_status, CMPI_chars); 
                
                }

        }

        if ( dc ) {
                free(dc);
        }

        free(status);
        free(active_status);
        free(uuid);

out:

        if ( nodeinfo_table ) {
                free_nodeinfo_table(nodeinfo_table);
        }

        DEBUG_LEAVE();
        return ci;

}


int
get_clusternode_instance(char * classname, CMPIBroker * broker,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIData data_uname;
        char * uname = NULL;
        CMPIInstance * ci = NULL;
        int ret = 0;


        DEBUG_ENTER();
        DEBUG_PID();

        data_uname = CMGetKey(cop, "Name", rc);
        uname = CMGetCharPtr(data_uname.value.string);

        op = CMNewObjectPath(broker, 
                CMGetCharPtr(CMGetNameSpace(cop, rc)), classname, rc);

        if ( CMIsNullObject(op) ){
                ret = HA_FAIL;
                goto out;
        }
        

        cl_log(LOG_INFO, "%s: make instance", __FUNCTION__);

        ci = make_node_instance(classname, broker,  op, uname, rc);

        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                goto out;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        ret = HA_OK;
        
out:

        DEBUG_LEAVE();
        return ret; 
}


int 
enumerate_clusternode_instances(char * classname, CMPIBroker * broker,
                CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc)
{

        CMPIObjectPath * op = NULL;
        GPtrArray * nodeinfo_table = NULL;

        char key_name[] = "Name";
        char key_creation[] = "CreationClassName";
        int i = 0;

        DEBUG_PID();

        nodeinfo_table = get_nodeinfo_table ();
        
        if ( nodeinfo_table == NULL ) {
                cl_log(LOG_ERR, "%s: can not get node info", __FUNCTION__);

                CMSetStatusWithChars(broker, rc,
                       CMPI_RC_ERR_FAILED, "Can't get node info");

                return HA_FAIL;
        }

        for (i = 0; i < nodeinfo_table->len; i++) {
                struct hb_nodeinfo * nodeinfo = NULL;
                char * uname = NULL;

                nodeinfo = (struct hb_nodeinfo *) 
                                g_ptr_array_index(nodeinfo_table, i);

                uname = strdup(nodeinfo->name);
                
                /* create an object */
                op = CMNewObjectPath(broker, 
                        (char *)CMGetNameSpace(ref, rc)->hdl, classname, rc);

                CMAddKey(op, key_creation, classname, CMPI_chars);
                CMAddKey(op, key_name, uname, CMPI_chars);

                if ( enum_inst ) {
                        /* enumerate instances */
                        CMPIInstance * ci = NULL;

                        ci = make_node_instance(classname, 
                                                broker, op, uname, rc);

                        if ( CMIsNullObject(ci) ) {
                                free(uname);

                                if ( nodeinfo_table ) {
                                        free_nodeinfo_table(nodeinfo_table);
                                }

                                return HA_FAIL;
                        }

                        CMReturnInstance(rslt, ci);

                }else { /* enumerate instance names */
                        /* add object path to rslt */
                        CMReturnObjectPath(rslt, op);
                }

                free(uname);
        }

        CMReturnDone(rslt);

        if ( nodeinfo_table ) {
                free_nodeinfo_table(nodeinfo_table);
        }

        return HA_OK;
}

int
cleanup_node () {

        cl_log(LOG_INFO, "%s: clean up", __FUNCTION__);
        return HA_OK;
}
