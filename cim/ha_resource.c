/*
 * HA resource info
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

static void free_each_node (gpointer data, gpointer user);
static struct cluster_resource_group_info *
       res_parse_group (const char * line, int depth);
static struct cluster_resource_info *
       res_parse_resource (const char * line, int depth);
static int res_parse (char ** lines, GList * list, int ln, int depth);

/* temporary implementation */

struct cluster_resource_info * 
res_info_dup (const struct cluster_resource_info * info)
{
        struct cluster_resource_info * new_info = NULL;

        new_info = (struct cluster_resource_info *)
                malloc(sizeof(struct cluster_resource_info));

        new_info->name = strdup(info->name);
        new_info->type = strdup(info->type);
        new_info->provider = strdup(info->provider);
        new_info->class = strdup(info->class);
        
        return new_info;
        
}

static void
free_each_node (gpointer data, gpointer user)
{
        struct res_node * node = NULL;
        node = (struct res_node *)data;
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


        info = (struct cluster_resource_group_info *)
                malloc(sizeof(struct cluster_resource_group_info));

        info->id = strdup(match[1]);
        info->id[strlen(info->id) - 1] = '\0';

        cl_log(LOG_INFO, "got Group: %s", info->id);
        
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

        info = (struct cluster_resource_info *)
                malloc(sizeof(struct cluster_resource_info));


        info->name = strdup (match[1]);
        info->provider = strdup (match[2]);
        info->type = strdup (match[3]);
        info->class = strdup (match[4]);

        cl_log(LOG_INFO, "got Resource: %s", info->name);
        free_2d_array(match);

        return info;
}

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
                        struct res_node * node = NULL;
                        GList * sub_list = NULL;

                        /* necessary  */
                        sub_list = g_list_alloc (); 

                        /* parse begin from i + 1 */
                        i = res_parse(lines, sub_list, i + 1, depth + 1);


                        g_info->res_list = sub_list;
                        
                        cl_log(LOG_INFO, 
                               "%s: CREATE group node", __FUNCTION__);

                        node = (struct res_node *)
                                malloc(sizeof(struct res_node));

                        node->type = GROUP;
                        node->res = (void *) g_info;
                        
                        g_list_append (list, (gpointer)node);

                } else if ( ( r_info = res_parse_resource (lines[i], depth) ) 
                            != NULL) {

                        struct res_node * node = NULL;

                        cl_log(LOG_INFO, 
                               "%s: CREATE resource node", __FUNCTION__);
                        
                        node = (struct res_node *)
                                malloc(sizeof(struct res_node));
                        
                        node->type = RESOURCE;
                        node->res = (void *) r_info;

                        g_list_append (list, (gpointer)node);

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
        char cmnd [] = HA_LIBDIR"/heartbeat/crm_resource -L";
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
                return NULL;
        }

        res_parse ( std_out, list, 0, 0);

        
        free_2d_array ( std_out );

        DEBUG_LEAVE();

        return list;
};

char * 
get_hosting_node(const char * name)
{
        char cmnd_pat[] = HA_LIBDIR"/heartbeat/crmadmin -W";
        int lenth, i;
        char * cmnd = NULL;
        char ** std_out = NULL;
        int exit_code, ret;
        char * node = NULL;

        cl_log(LOG_WARNING, "%s: !! TEMPORARY IMPLEMENTATION", __FUNCTION__);

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

