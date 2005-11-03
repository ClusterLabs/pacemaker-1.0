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


#ifndef _HA_RESOURCE_H
#define _HA_RESOURCE_H

typedef int res_type_t;
#define RESOURCE 1
#define GROUP    2

struct res_node_data {
        res_type_t type;
        void * res;     /* cluster_resource_group_info 
                           or cluster_resource_info */
};

struct cluster_resource_group_info {
        char  * id;
        GList * res_list;
};

struct cluster_resource_info {
        char *  name;
        char *  type;
        char *  provider;
        char *  class;
        
};

#define GetResNodeData(node)  ((struct res_node_data *)node->data)
#define GetResType(data)      (((struct res_node_data *)data)->type)
#define GetResourceInfo(data) ((struct cluster_resource_info *)       \
                                      ((struct res_node_data *)data)->res)
#define GetGroupInfo(data)    ((struct cluster_resource_group_info *) \
                                      ((struct res_node_data *)data)->res)


GList * get_res_list (void);
int free_res_list (GList * res_list);

GNode * get_res_tree (void);
int free_res_tree (GNode * root);
GList * build_res_list ( GNode * root);

struct cluster_resource_info * 
res_info_dup (const struct cluster_resource_info * info);
int free_res_info(struct cluster_resource_info * info);
GNode * search_res_in_tree(GNode * root, char * name, res_type_t type);

char * get_hosting_node(const char * name);

#endif
