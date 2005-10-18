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


#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include <hb_api.h>
#include <clplumbing/cl_log.h>

#include "cmpi_utils.h"
#include "ha_resource.h"
#include "cmpi_sub_resource.h"

static GPtrArray * get_sub_resource_names(const char * group_id);
static int free_sub_resource_name_array(GPtrArray * rsc_name_array);
static int group_has_resource(const char * group_id, const char * rsc_name);
static struct cluster_resource_group_info *
       search_group_in_list (GList * list, const char * group_id);

static struct cluster_resource_group_info *
search_group_in_list (GList * list, const char * group_id)
{
        struct cluster_resource_group_info * info = NULL;
        GList * p = NULL;

        for ( p = list; ; p = g_list_next(p) ) {
                struct res_node * node = NULL;
                node = (struct res_node *)p->data;

                if ( node == NULL || node->type == RESOURCE ) {
                        continue;
                }
                
                info = (struct cluster_resource_group_info *) node->res;

                cl_log(LOG_INFO, "%s: Got group %s", __FUNCTION__, 
                       info->id);

                if ( strcmp (info->id, group_id) == 0 ) {
                        return info;
                } else {
                        return search_group_in_list ( info->res_list, 
                                                      group_id);
                }

                if ( p == g_list_last(list)) {
                        break;
                }
        }

        return info;
}


static GPtrArray *
get_sub_resource_names(const char * group_id)
{
        GPtrArray * rsc_name_array = NULL;
        GList * list = NULL;
        GList * p = NULL;
        struct cluster_resource_group_info * info = NULL;


        DEBUG_ENTER();
        
        rsc_name_array = g_ptr_array_new ();

        if ( rsc_name_array == NULL ) {
                cl_log(LOG_ERR, "%s: can't alloc array", __FUNCTION__);
                return NULL;
        }


        list = get_res_list ();

        if ( list == NULL ) {
                return rsc_name_array;
        }

        info = search_group_in_list ( list, group_id );
        if ( info == NULL ) {
                return rsc_name_array;
        }

        /* Attention, only add direct sub resource */
        for ( p = info->res_list; ; p = g_list_next(p) ) {
                struct res_node * node = NULL;
                struct cluster_resource_info * res_info = NULL;

                node = (struct res_node *)p->data;

                if ( node == NULL || node->type == GROUP ) {
                        continue;
                }

                res_info = (struct cluster_resource_info *) node->res;
                g_ptr_array_add ( rsc_name_array, res_info_dup(res_info) );

                if ( p == g_list_last(p) ) {
                        break;
                }
        }

        free_res_list (list);

        DEBUG_LEAVE();
        return rsc_name_array;
} 


static int
free_sub_resource_name_array(GPtrArray * rsc_name_array)
{
        while ( rsc_name_array->len ) {
                char * rsc_name = NULL;
                rsc_name = (char *)
                        g_ptr_array_remove_index_fast(rsc_name_array, 0);
                free(rsc_name);
        }

        g_ptr_array_free(rsc_name_array, 0);
      
        return HA_OK;
}

static int
group_has_resource(const char * group_id, const char * rsc_name)
{
        GPtrArray * rsc_name_array = NULL;
        int i = 0;

        cl_log(LOG_INFO, "%s: looking for %s in group %s",
               __FUNCTION__, rsc_name, group_id);

        rsc_name_array = get_sub_resource_names (group_id);

        if ( rsc_name_array == NULL ) {
                return 0;
        }

        for ( i = 0; i < rsc_name_array->len; i++ ) {
                char * sub_rsc_name = NULL;

                sub_rsc_name = (char *) g_ptr_array_index(rsc_name_array, i);

                if ( sub_rsc_name == NULL ) {
                        cl_log(LOG_WARNING, 
                               "%s: got NULL, continue",__FUNCTION__);
                        continue;

                }

                if ( strcmp ( rsc_name, sub_rsc_name) == 0 ) {
                        free_sub_resource_name_array(rsc_name_array);
                        cl_log(LOG_INFO, "%s: we got it", __FUNCTION__);
                        return 1;
                }

        }         

        return 0;
}

int is_sub_resource_of(CMPIInstance * resource_inst, 
                       CMPIInstance * group_inst, CMPIStatus * rc)
{
        CMPIString * rsc_name = NULL;
        /* CMPIArray * sub_rsc_names = NULL; */
        /* int count = 0; */
        /* int i = 0; */

        CMPIString * group_id = NULL;
        int contain = 0;


        DEBUG_ENTER();

        /* resource's name */
        rsc_name = CMGetProperty(resource_inst, "Name", rc).value.string;

        if ( rsc_name == NULL ) {
                cl_log(LOG_INFO, "%s: resource_name =  NULL", __FUNCTION__);
                contain = 0;
                goto out;
        }

        group_id = CMGetProperty(group_inst, "GroupId", rc).value.string;

        if ( CMIsNullObject( group_id ) ) {
                cl_log(LOG_INFO, "%s: invalid GroupId", __FUNCTION__);
                contain = 1;
                goto out;
        }

        contain = group_has_resource(CMGetCharPtr(group_id), 
                                     CMGetCharPtr(rsc_name));
out:
        DEBUG_LEAVE();

        return contain;

}
