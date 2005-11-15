/*
 * ha_resource.c: resource information routines
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

#include "cmpi_utils.h"
#include "linuxha_info.h"
#include "ha_resource.h"

static struct cluster_resource_group_info *
res_parse_group (const char * line, int depth);
static struct cluster_resource_info *
res_parse_resource (const char * line, int depth);

static void free_each_node (gpointer data, gpointer user);
static int res_parse (char ** lines, GList * list, int ln, int depth);

static int res_parse_tree (char ** lines, GNode * parent, int ln, int depth);
static int add_tree_node_to_list( GNode * node, gpointer user);
static int free_each_tree_node ( GNode * node, gpointer user);
static int search_in_tree ( GNode * node, gpointer user);


/* temporary implementation */
struct cluster_resource_info * 
res_info_dup (const struct cluster_resource_info * info)
{
        struct cluster_resource_info * new_info = NULL;

        new_info = (struct cluster_resource_info *)
                malloc(sizeof(struct cluster_resource_info));

        if ( new_info == NULL ) {
                cl_log(LOG_ERR, "%s: alloc info failed", __FUNCTION__);
                return NULL;
        }

        new_info->name = strdup(info->name);
        new_info->type = strdup(info->type);
        new_info->provider = strdup(info->provider);
        new_info->class = strdup(info->class);
        
        return new_info;
        
}

int 
free_res_info(struct cluster_resource_info * info)
{
        free(info->name);
        free(info->type);
        free(info->provider);
        free(info->class);
        return HA_OK;
}


char * 
get_hosting_node(const char * name)
{
        char cmnd_pat[] = "crm_resource -W -r";
        int lenth, i;
        char * cmnd = NULL;
        char ** std_out = NULL;
        int exit_code, ret;
        char * node = NULL;

        cl_log(LOG_WARNING, "%s: !! TEMPORARY IMPLEMENTATION", __FUNCTION__);
        cl_log(LOG_INFO, "%s: looking for hosting node for %s", 
               __FUNCTION__, name);

        lenth = strlen(cmnd_pat) + strlen(name) + 1;

        if ( (cmnd = malloc(lenth)) == NULL ) {
                return NULL;
        }
        
        sprintf(cmnd, "%s %s", cmnd_pat, name);
        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);
        if ( ret != HA_OK ) {
                free(cmnd);
                return NULL;
        }

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

/*
Resource Group: group_1
    group_1:IPaddr_1 (heartbeat::ocf:IPaddr)
    group_1:IPaddr_2 (heartbeat::ocf:IPaddr)
IPaddr_4 (heartbeat::ocf:IPaddr)
*/

static struct cluster_resource_group_info *
res_parse_group (const char * line, int depth)
{
        int ret = 0;
        char ** match = NULL;
        const char * regex_str = NULL;

        struct cluster_resource_group_info * info = NULL;

        cl_log(LOG_INFO, "%s: match %s at depth: %d", 
               __FUNCTION__, line, depth);

        info = (struct cluster_resource_group_info *)
                malloc(sizeof(struct cluster_resource_group_info));

        if ( info == NULL ) {
                cl_log(LOG_ERR, "%s: alloc info failed", __FUNCTION__);
                return NULL;
        }

        if ( depth ) {
                regex_str = "\\W+Resource Group: (.*)";
        } else {
                regex_str = "Resource Group: (.*)";
        }

        ret = regex_search(regex_str, line, &match);
        if ( ret != HA_OK ) {
                cl_log(LOG_WARNING, "%s: no match", __FUNCTION__);
                return NULL;
        }

        if ( (info->id = strdup(match[1])) != NULL ) {
                info->id[strlen(info->id) - 1] = '\0';
                cl_log(LOG_INFO, "got Group: %s", info->id);
        }

        free_2d_array(match);

        return info;
}

static struct cluster_resource_info *
res_parse_resource (const char * line, int depth)
{
        int ret = 0;
        char ** match = NULL;
        const char * regex_str = NULL;
        struct cluster_resource_info * info = NULL;


        cl_log(LOG_INFO, "%s: match %s at depth: %d", 
               __FUNCTION__, line, depth);

        info = (struct cluster_resource_info *)
                malloc(sizeof(struct cluster_resource_info));

        if ( info == NULL ) {
                cl_log(LOG_ERR, "%s: alloc info failed", __FUNCTION__);
                return NULL;
        }

        if ( depth ) {
                regex_str = "\\W+([^:]*) \\((.*)::(.*):(.*)\\)";
        } else {
                regex_str = "([^:]*) \\((.*)::(.*):(.*)\\)";
        }

        ret = regex_search(regex_str, line, &match);
        if ( ret != HA_OK ) {
                cl_log(LOG_WARNING, "%s: no match", __FUNCTION__);
                return NULL;
        }

        info->name = strdup (match[1]);
        info->provider = strdup (match[2]);
        info->type = strdup (match[3]);
        info->class = strdup (match[4]);
        
        if ( info->name ){
                cl_log(LOG_INFO, "got Resource: %s", info->name);
        }

        free_2d_array(match);

        return info;
}

/*******************************************
 * store resource information in list
 ******************************************/

static int
res_parse (char ** lines, GList * list, int ln, int depth)
{
        struct cluster_resource_info * r_info = NULL;
        struct cluster_resource_group_info * g_info = NULL;
        int i = 0;
        
        i = ln;

        cl_log(LOG_INFO, "%s: begin to parse ln: %d, depth: %d", 
               __FUNCTION__, ln, depth);

        while ( lines [i] ) {
                if ( (g_info = res_parse_group (lines[i], depth)) != NULL ) {
                        struct res_node_data * node = NULL;
                        GList * sub_list = NULL;

                        /* necessary  */
                        if ( (sub_list = g_list_alloc () ) == NULL ){
                                continue;
                        }
                        
                        /* parse begin from i + 1 */
                        i = res_parse(lines, sub_list, i + 1, depth + 1);
                        g_info->res_list = sub_list;
                        
                        cl_log(LOG_INFO, 
                               "%s: CREATE group node", __FUNCTION__);

                        node = (struct res_node_data *)
                                malloc(sizeof(struct res_node_data));
                        if ( node ) {
                                node->type = GROUP;
                                node->res = (void *) g_info;
                                g_list_append (list, (gpointer)node);
                        }
                } else if ( ( r_info = res_parse_resource (lines[i], depth) ) 
                            != NULL) {

                        struct res_node_data * node = NULL;

                        cl_log(LOG_INFO, 
                               "%s: CREATE resource node", __FUNCTION__);
                        
                        node = (struct res_node_data *)
                                malloc(sizeof(struct res_node_data));
                        if ( node ) {
                                node->type = RESOURCE;
                                node->res = (void *) r_info;
                                g_list_append (list, (gpointer)node);
                        }
                        /* move to next line */
                        i ++; 

                } else {
                        cl_log(LOG_INFO, "%s: END of a group", __FUNCTION__);
                        break;
                }
        }
        
        return i;
}

GList *
get_res_list ()
{
        char cmnd [] = "crm_resource -L";
        char ** std_out = NULL;
        GList * list = NULL;

        int exit_code = 0;
        int ret = 0;
        
        DEBUG_ENTER();

        cl_log(LOG_WARNING, "%s: !! TEMPORARY IMPLEMENTATION", __FUNCTION__);
        
        list = g_list_alloc ();
        if ( list == NULL ) {
                cl_log(LOG_ERR, "%s: can't alloc list", __FUNCTION__);
                return NULL;
        }

        cl_log(LOG_INFO, "%s: exec command %s", __FUNCTION__, cmnd);

        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);
        if ( ret != HA_OK ) {
                cl_log(LOG_ERR, "%s: error exec command %s", 
                       __FUNCTION__, cmnd);

                return list;
        }

        res_parse ( std_out, list, 0, 0);
        free_2d_array ( std_out );
        DEBUG_LEAVE();

        return list;
};

static void
free_each_node (gpointer data, gpointer user)
{
        struct res_node_data * node = NULL;
        node = (struct res_node_data *)data;
        if ( node == NULL ) {
                return;
        }
        
        if ( node->type == RESOURCE ) {
                struct cluster_resource_info * res_info = NULL;
                res_info =
                        (struct cluster_resource_info *)node->res;
                free(res_info->name);
                free(res_info->class);
                free(res_info->type);
                free(res_info);

        } else {
                struct cluster_resource_group_info * info = NULL;
                info = (struct cluster_resource_group_info *)node->res;
                free_res_list(info->res_list);
        }

}

int
free_res_list (GList * res_list) 
{
        
        if ( res_list == NULL ) {
                return HA_FAIL;
        }

        g_list_foreach(res_list, free_each_node, NULL);
        g_list_free (res_list);

        return HA_OK;
}



/***********************************************
 * store resource information in tree
 **********************************************/

static int
res_parse_tree (char ** lines, GNode * parent, int ln, int depth)
{
        struct cluster_resource_info * r_info = NULL;
        struct cluster_resource_group_info * g_info = NULL;
        GNode * current = NULL;
        int i = 0;
        
        i = ln;
        cl_log(LOG_INFO, "%s: begin to parse ln: %d, depth: %d", 
               __FUNCTION__, ln, depth);

        while ( lines [i] ) {
                if ( (g_info = res_parse_group (lines[i], depth)) != NULL ) {
                        /* is a resource group */
                        struct res_node_data * node_data = NULL;
                        GNode * child = NULL;
                        
                        node_data = (struct res_node_data *)
                                    malloc(sizeof(struct res_node_data));
                        
                        if ( node_data == NULL ) {
                                cl_log(LOG_ERR, "%s: alloc node data failed", 
                                       __FUNCTION__);
                                return HA_FAIL;
                        }

                        node_data->type = GROUP;
                        node_data->res = (void *) g_info;
 
                        child = g_node_new ( node_data );

                        /* insert child to tree */
                        cl_log(LOG_INFO, "%s: insert group to tree", __FUNCTION__);
                        g_node_insert_after ( parent, current, child );
                        current = child;

                        /* parse begin from i + 1 */
                        i = res_parse_tree(lines, child, i + 1, depth + 1);
                        
                } else if ( ( r_info = res_parse_resource (lines[i], depth) ) 
                            != NULL) { /* is a resource */

                        struct res_node_data * node_data = NULL;
                        GNode * child = NULL;

                        node_data = (struct res_node_data *)
                                malloc(sizeof(struct res_node_data));
                        
                        if ( node_data == NULL ) {
                                cl_log(LOG_ERR, "%s: alloc node data failed", 
                                       __FUNCTION__);
                                return HA_FAIL;
                        }
                        node_data->type = RESOURCE;
                        node_data->res = (void *) r_info;

                        child = g_node_new ( node_data );

                        /* insert child to tree */
                        cl_log(LOG_INFO, "%s: insert resource to tree",
                               __FUNCTION__);
                        g_node_insert_after ( parent, current, child );
                        current = child;

                        /* move to next line */
                        i ++; 

                } else {
                        cl_log(LOG_INFO, "%s: END of a group", __FUNCTION__);
                        break;
                }
        }
        
        return i;
}


GNode *
get_res_tree ()
{
        char cmnd [] = "crm_resource -L";
        char ** std_out = NULL;
        GNode * root = NULL;

        int exit_code = 0;
        int ret = 0;
        
        DEBUG_ENTER();

        cl_log(LOG_WARNING, "%s: !! TEMPORARY IMPLEMENTATION", __FUNCTION__);

        if ( (root = g_node_new ( NULL )) == NULL ) {
                return NULL;
        }
        
        root->parent = NULL;

        cl_log(LOG_INFO, "%s: exec command %s", __FUNCTION__, cmnd);
        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);
        if ( ret != HA_OK ) {
                cl_log(LOG_ERR, "%s: error exec command %s", 
                       __FUNCTION__, cmnd);
                return NULL;
        }

        res_parse_tree ( std_out, root, 0, 0);

        free_2d_array ( std_out );
        DEBUG_LEAVE();
        return root;
}

int free_each_tree_node ( GNode * node, gpointer user )
{
        struct res_node_data * node_data = NULL;

        node_data = (struct res_node_data *) node->data;
        if ( node_data == NULL ) {
                return 0;
        }
        
        if ( node_data->type == RESOURCE ) {
                struct cluster_resource_info * res_info = NULL;
                res_info =
                        (struct cluster_resource_info *)node_data->res;

                cl_log(LOG_INFO, "%s: free resource %s", __FUNCTION__, 
                       res_info->name);

                free(res_info->name);
                free(res_info->class);
                free(res_info->type);
                free(res_info);

        } else {
                struct cluster_resource_group_info * info = NULL;
                info = (struct cluster_resource_group_info *)node_data->res;

                cl_log(LOG_INFO, "%s: free resource group %s", 
                       __FUNCTION__, info->id);

                free (info->id);
        }
       
        free ( node_data);
        return 0;
}

int
free_res_tree ( GNode * root )
{
        GTraverseFlags flag = G_TRAVERSE_LEAFS | G_TRAVERSE_NON_LEAFS;
        g_node_traverse(root, G_POST_ORDER, flag, G_MAXINT, 
                        free_each_tree_node, NULL);
        g_node_destroy ( root );
        return HA_OK;
}

static
int add_tree_node_to_list ( GNode * node, gpointer user)
{
        GList * list = NULL;
        struct res_node_data * node_data = NULL;
        
        list = *(GList **) user;

        node_data = (struct res_node_data *) node->data;
        if ( node_data ==NULL ) {
                cl_log(LOG_INFO, "%s: added a node to list", __FUNCTION__);
        } else {
                cl_log(LOG_INFO, "%s: added node type %d to list",
                       __FUNCTION__, node_data->type);
        }

        *(GList **)user = g_list_append(list, node);

        return 0;
        
}

GList *
build_res_list ( GNode * root )
{
        GList * list = NULL;
        GTraverseFlags traverse_flag = G_TRAVERSE_LEAFS | G_TRAVERSE_NON_LEAFS;
        g_node_traverse(root, G_IN_ORDER, traverse_flag, G_MAXINT, 
                        add_tree_node_to_list, &list);

        return list;
}



static int
search_in_tree ( GNode * node, gpointer user)
{
        char * name = NULL;
        struct res_node_data * node_data = NULL;
        void ** user_data = NULL;
        res_type_t type;

        user_data = (void **) user;
        type = *( (res_type_t *) user_data[0]);
        name = (char *) user_data [1];

        node_data = (struct res_node_data *) node->data;
        if ( node_data == NULL ) {
                return 0;
        }
        
        if ( !( type & node_data->type ) ) {
                return 0;
        }

        if ( node_data->type == RESOURCE ) {
                struct cluster_resource_info * res_info = NULL;
                res_info = (struct cluster_resource_info *)node_data->res;

                if ( strcmp (res_info->name, name ) == 0 ) {
                        cl_log(LOG_INFO, 
                               "%s: resource found %s", __FUNCTION__, name);
                        user_data [2] = node;
                        return 1;
                }
        } else {
                struct cluster_resource_group_info * info = NULL;
                info = (struct cluster_resource_group_info *)node_data->res;

                if ( strcmp(info->id, name ) == 0 ) {
                        cl_log(LOG_INFO, "%s: resource group found %s", 
                               __FUNCTION__, name);
                        return 1;
                }
        }

        user_data [2] = NULL;
        return 0;
}

GNode *
search_res_in_tree (GNode * root, char * name, res_type_t type)
{
        void * user_data [3];
        GTraverseFlags traverse_flag;

        /* traverse tree looking for resource */
        user_data [0] = &type;
        user_data [1] = name;
        user_data [2] = NULL;
        traverse_flag = G_TRAVERSE_LEAFS | G_TRAVERSE_NON_LEAFS;

        g_node_traverse(root, G_POST_ORDER, traverse_flag, G_MAXINT, 
                        search_in_tree, user_data);

        return user_data [2]; 
}
