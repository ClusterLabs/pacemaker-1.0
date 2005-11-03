/*
 * Tests 
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
#include <regex.h>
#include <glib.h>
#include <clplumbing/cl_malloc.h>

#include "cmpi_cluster.h"
#include "linuxha_info.h"
#include "cmpi_utils.h"
#include "ha_resource.h"


void print_for_each (gpointer data, gpointer user);
int tree_print_for_each ( GNode * node, gpointer user);

int tree_print_for_each ( GNode * node, gpointer user) {
        print_for_each(node->data, user );
        return 0;
}

void 
print_for_each (gpointer data, gpointer user)
{
        struct res_node_data * node = NULL;
        node = (struct res_node_data *) data;

        if ( node == NULL ) {
                return;
        }

                
        if ( node->type == GROUP ) {
                struct cluster_resource_group_info * info = NULL;
                info = (struct cluster_resource_group_info *)
                        node->res;
                cl_log(LOG_INFO, "---- %d: %s", node->type, info->id);
              /*  g_list_foreach(info->res_list, print_for_each, NULL);
                cl_log(LOG_INFO, "---- %s END", info->id); */

        } else {
                struct cluster_resource_info * info = NULL;
                info = (struct cluster_resource_info *)
                        node->res;
                cl_log(LOG_INFO, "---- %d: %s", node->type, info->name);
        
        }
}

int main(void)
{
/*
        GList * list = NULL;
        GList * p = NULL;
*/
        GNode * root = NULL;

        init_logger ("cim-demo");
/*
        list = get_res_list ();

        g_list_foreach(list, print_for_each, NULL);

        for ( p = list; ; p = g_list_next(p) ){
                cl_log(LOG_INFO, "p @ 0x%0x", (unsigned int)p);
                if ( p == g_list_last(list) ) {
                        break;
                }
        }

        free_res_list(list);
*/
        root = get_res_tree ();
        g_node_traverse( root, G_POST_ORDER, 0x3, 10, tree_print_for_each, NULL);   
        return 0;

}
