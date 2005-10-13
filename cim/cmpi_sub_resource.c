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

#include "cmpi_sub_resource.h"


static GPtrArray * get_sub_resource_names(const char * group_id);
static int free_sub_resource_name_array(GPtrArray * rsc_name_array);
static int group_has_resource(const char * group_id, const char * rsc_name);



static GPtrArray *
get_sub_resource_names(const char * group_id)
{
        char ** std_out = NULL;
        char cmnd [] = HA_LIBDIR"/heartbeat/crmadmin -R";
        int exit_code = 0;
        int ret = 0;
        int i = 0;
        int in_group = 0;
        char * rsc_name = NULL;
        GPtrArray * rsc_name_array = NULL;



        DEBUG_ENTER();
        
        rsc_name_array = g_ptr_array_new ();

        if ( rsc_name_array == NULL ) {
                cl_log(LOG_ERR, "%s: can't alloc array", __FUNCTION__);
        }

        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);

        for (i = 0; std_out[i]; i++){
                char ** match = NULL;
                if ( !in_group ) {
                        ret = regex_search("group: (.*) \\((.*)\\)", 
                                                std_out[i], &match);
                        if ( ret != HA_OK ) {
                                continue;
                        }
                } else {        /* in group */
                        ret = regex_search("\\Wprimitive: (.*) \\((.*)::(.*)\\)",
                                                std_out[i], &match);
                        if ( ret != HA_OK ) {
                                in_group = 0;
                                continue;
                        }
                }

                if ( !in_group && strcmp ( group_id, match[1] ) == 0 ){
                        cl_log(LOG_INFO, "%s: found group_id: %s", 
                                                __FUNCTION__, group_id);
                        in_group = 1;
                        continue;
                }
                
                if ( in_group ) {
                        cl_log(LOG_INFO, "%s: found sub resource: %s", 
                                                __FUNCTION__, match[1]); 
                        rsc_name = strdup(match[1]); 
                        g_ptr_array_add ( rsc_name_array, 
                                                (gpointer *)rsc_name);
                        continue;
                }
        }


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
