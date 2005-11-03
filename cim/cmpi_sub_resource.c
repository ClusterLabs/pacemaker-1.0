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


static int group_has_resource(const char * group_id, char * rsc_name);


static int
group_has_resource(const char * group_id, char * rsc_name)
{
        GNode * root = NULL;
        GNode * node = NULL;

        cl_log(LOG_INFO, "%s: looking for %s in group %s",
               __FUNCTION__, rsc_name, group_id);

        /* TODO: optimize this, avoid to rebuild tree frequently */

        root = get_res_tree ();
        if ( root == NULL ) {
                cl_log (LOG_ERR, "%s: failed to get resource tree", 
                        __FUNCTION__);
                return 0;
        }

        node = search_res_in_tree(root, rsc_name, RESOURCE);
        if ( node == NULL ) {
                cl_log(LOG_WARNING, "%s: resource %s not found in tree",
                       __FUNCTION__, rsc_name);
                return 0;
        }

        /* go through its parents */

        while ( node && node->parent) {
                node = node->parent;
                if ( node->data == NULL ) break;

                cl_log(LOG_INFO, "%s: found %s's parent %s", __FUNCTION__,
                       rsc_name, GetGroupInfo(GetResNodeData(node))->id);

                if ( strcmp (group_id, 
                             GetGroupInfo(GetResNodeData(node))->id) == 0) {
                        free_res_tree ( root );
                        return 1;
                }
                     
        }

        free_res_tree (root);
        return 0;
}

int is_sub_resource_of(CMPIInstance * resource_inst, 
                       CMPIInstance * group_inst, CMPIStatus * rc)
{
        CMPIString * rsc_name = NULL;

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


