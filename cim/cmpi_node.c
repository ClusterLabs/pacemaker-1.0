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
#include <clplumbing/cl_malloc.h>

#include "linuxha_info.h"
#include "cmpi_node.h"
#include "cmpi_utils.h"


#define HB_CLIENT_ID "cim-provider-node"

static char * get_cluster_dc(void);
static char * get_clusternode_status(const char * node);

static CMPIInstance * make_clusternode_instance(char * classname, 
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
get_clusternode_status(const char * node)
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


static CMPIInstance *
make_clusternode_instance(char * classname, CMPIBroker * broker, 
                CMPIObjectPath * op, char * uname, CMPIStatus * rc)
{
        GPtrArray * gNodeTable = NULL;
        struct hb_nodeinfo * nodeinfo = NULL;
        CMPIInstance* ci = NULL;
        char * dc = NULL;
        char * status = NULL;
        char * active_status = NULL;
        char * uuid = NULL;
        int i;


        DEBUG_ENTER();

        /*** linuxha should be init by the caller ***/
        ASSERT( get_hb_initialized() );

        gNodeTable = get_hb_info(LHA_NODEINFO);

        for (i = 0; i < gNodeTable->len; i++) {
                int length = 0;
                nodeinfo = (struct hb_nodeinfo *) 
                                g_ptr_array_index(gNodeTable, i);
                length = strlen ( nodeinfo->name) + 1;
                if ( strncmp(nodeinfo->name, uname, length) == 0 ){
                        break;
                }
        }

        if ( i == gNodeTable->len){
                cl_log(LOG_WARNING, 
                        "%s: %s is not a valid cluster node", 
                        __FUNCTION__, uname); 
                goto out;
        }


        
        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                goto out;
        }

 
        cl_log(LOG_INFO, "%s: get DC", __FUNCTION__);
        dc = get_cluster_dc();

        cl_log(LOG_INFO, "%s: get node's status", __FUNCTION__);
        status = get_clusternode_status(uname);

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
        DEBUG_LEAVE();
        return ci;

}

int
get_clusternode_instance(char * classname, CMPIBroker * broker,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties, CMPIStatus * rc)
{
        CMPIObjectPath* op = NULL;
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
        
        if ( ! get_hb_initialized() ) {
                ret = linuxha_initialize(HB_CLIENT_ID, 0);
                if (ret != HA_OK ) {
                        char err_info [] = "Can't initialized LinuxHA";

	                CMSetStatusWithChars(broker, rc, 
			                CMPI_RC_ERR_FAILED, err_info);
                        cl_log(LOG_INFO, 
                                "%s: failed to initialize LinuxHA",
                                __FUNCTION__);
                        goto out;
                }
        }

        cl_log(LOG_INFO, "%s: make instance", __FUNCTION__);

        ci = make_clusternode_instance(classname, broker,  op, uname, rc);

        if ( get_hb_initialized () ) {
                linuxha_finalize();
        }

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
        GPtrArray * gNodeTable = NULL;

        char key_name[] = "Name";
        char key_creation[] = "CreationClassName";
        int ret = 0;
        int i = 0;

        DEBUG_PID();
        if ( ! get_hb_initialized() ) {
                ret = linuxha_initialize(HB_CLIENT_ID, 0);
                if (ret != HA_OK ) {

                        char err_info [] = "Can't initialized heartbeat";

	                CMSetStatusWithChars(broker, rc, 
			                CMPI_RC_ERR_FAILED, err_info);

                        cl_log(LOG_INFO, 
                                "%s: failed to initialize heartbeat", 
                                __FUNCTION__);
                        return ret;
                }
        }


        gNodeTable = get_hb_info(LHA_NODEINFO); 

        for (i = 0; i < gNodeTable->len; i++) {
                struct hb_nodeinfo * nodeinfo = NULL;
                char * uname = NULL;
                char * uuid = NULL;

                nodeinfo = (struct hb_nodeinfo *) 
                                g_ptr_array_index(gNodeTable, i);

                uname = strdup(nodeinfo->name);
                uuid = malloc(17);
                
                /* create an object */
                op = CMNewObjectPath(broker, 
                        (char *)CMGetNameSpace(ref, rc)->hdl, classname, rc);

                CMAddKey(op, key_creation, classname, CMPI_chars);
                CMAddKey(op, key_name, uname, CMPI_chars);

                if ( enum_inst ) {
                        CMPIInstance * ci = NULL;
                        ci = make_clusternode_instance(classname, 
                                broker, op, uname, rc);
                        if ( CMIsNullObject(ci) ) {
                                return HA_FAIL;
                        }
                        CMReturnInstance(rslt, ci);

                }else {
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);
                }
                free(uname);
                free(uuid);
        }

        CMReturnDone(rslt);
        
        if ( get_hb_initialized() ) {
                linuxha_finalize();
        }

        return HA_OK;
}

int
cleanup_node () {

        cl_log(LOG_INFO, "%s: clean up", __FUNCTION__);
        return HA_OK;
}
