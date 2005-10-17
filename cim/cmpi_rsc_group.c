/*
 * CIM Provider Header File
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include <hb_api.h> 
#include "cmpi_utils.h"
#include "linuxha_info.h"

#include "cmpi_rsc_group.h"
#include "ha_resource.h"

static void add_groupid_for_each (gpointer data, gpointer user);
static GPtrArray * get_groupid_table (void);
static int free_groupid_table ( GPtrArray * groupid_table);


static void 
add_groupid_for_each (gpointer data, gpointer user)
{
        struct res_node * node = NULL;
        GPtrArray * array = NULL;

        node = (struct res_node *) data;
   
        if ( node == NULL ) {
                return;
        }

        array = (GPtrArray *) user;
                
        if ( node->type == GROUP ) {
                struct cluster_resource_group_info * info = NULL;
                info = (struct cluster_resource_group_info *) node->res;

                cl_log(LOG_INFO, 
                       "%s: add group %s", __FUNCTION__, info->id);

                g_ptr_array_add(array, strdup(info->id));

                g_list_foreach(info->res_list, add_groupid_for_each, array);

        } else {
        }
}


static GPtrArray * 
get_groupid_table ()
{
        
        GPtrArray * groupid_table = NULL;
        GList * list = NULL;
        
        DEBUG_ENTER();

        groupid_table = g_ptr_array_new ();

        if ( groupid_table == NULL ) {
                cl_log(LOG_ERR, "%s: failed to alloc array", __FUNCTION__);
                return NULL;
        }

        list = get_res_list ();
        
        g_list_foreach(list, add_groupid_for_each, groupid_table);

        free_res_list (list);
        
        DEBUG_LEAVE();

        return groupid_table;
}

static int 
free_groupid_table ( GPtrArray * groupid_table)
{
        while (groupid_table->len) {
                char * group_id = NULL;
                group_id = (char *)
                        g_ptr_array_remove_index_fast(groupid_table, 0);
                free (group_id);
        }

        g_ptr_array_free(groupid_table, 0);
        return HA_OK;

}

int 
enumerate_resource_groups(char * classname, CMPIBroker * broker,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc)
{
        CMPIObjectPath* op = NULL;
        GPtrArray * groupid_table = NULL;
        int i = 0;

        groupid_table = get_groupid_table ();

        if (groupid_table == NULL ) {
                cl_log(LOG_ERR, "%s: can't get group ids", __FUNCTION__);
                return HA_FAIL;
        }

        for ( i = 0 ; i < groupid_table->len; i++ ) {
                char * group_id = NULL;

                CMPIString * nsp = NULL;

                group_id = (char *) g_ptr_array_index(groupid_table, i);

                if ( group_id == NULL ) {
                        continue;
                }

                /* create an object */
                nsp = CMGetNameSpace(ref, rc);
                op = CMNewObjectPath(broker, (char *)nsp->hdl, classname, rc);

                if ( CMIsNullObject(op) ){
                        free_groupid_table (groupid_table);
                        return HA_FAIL;
                }

                        
                if ( ! enum_inst ) { /* just enumerate names */
                        /* add keys */
                        CMAddKey(op, "GroupId", group_id, CMPI_chars);
       
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);


                }else{  /* return instance */
                        CMPIInstance * inst = NULL;
                        inst = CMNewInstance(broker, op, rc);

                        if ( inst == NULL ) {
                                free_groupid_table (groupid_table);
                                return HA_FAIL;
                        }
                        cl_log(LOG_INFO, 
                               "%s: ready to set instance", __FUNCTION__);
                        
                        CMSetProperty(inst, "GroupId", group_id, CMPI_chars);
                        
                        CMReturnInstance(rslt, inst);
 
                }

        }
        
        CMReturnDone(rslt);

        free_groupid_table (groupid_table);
        return HA_OK;
}


int 
get_resource_group_instance(char * classname, CMPIBroker * broker,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, CMPIStatus * rc)
{
        CMPIData key_data;
        CMPIObjectPath * op = NULL;
        CMPIInstance * inst = NULL;

        char * group_id = NULL;
        char * nsp = NULL;

        key_data = CMGetKey(ref, "GroupId", rc);

        if ( key_data.value.string == NULL ) {
                return HA_FAIL;
        }

        group_id = (char *)key_data.value.string->hdl;

        nsp = (char *)CMGetNameSpace(ref, rc)->hdl;
        op = CMNewObjectPath(broker, nsp, classname, rc);

        inst = CMNewInstance(broker, op, rc);

        if ( inst == NULL ) {
                return HA_FAIL;
        }

        cl_log(LOG_INFO, "%s: ready to set instance", __FUNCTION__);

        CMSetProperty(inst, "GroupId", group_id, CMPI_chars);
        CMReturnInstance(rslt, inst);
        CMReturnDone(rslt);

        return HA_OK;
}
