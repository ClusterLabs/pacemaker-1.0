/*
 * cmpi_rsc_group.c: helper file for LinuxHA_ClusterResourceGroup provider
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

int 
enum_inst_res_group(char * classname, CMPIBroker * broker,
                    CMPIContext * ctx, CMPIResult * rslt,
                    CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc)
{
        CMPIObjectPath* op = NULL;
        GNode * root = NULL;
        GList * list = NULL;
        GList * current = NULL;

        root = get_res_tree ();

        if ( root == NULL ) {
                cl_log(LOG_ERR, "%s: can't get resource tree", __FUNCTION__);
                return HA_FAIL;
        }

        list = build_res_list ( root );

        if ( list == NULL ) {
                cl_log(LOG_ERR, "%s: failed to build list", __FUNCTION__);
        }

        for ( current = list ; current; ) {
                char * group_id = NULL;
                CMPIString * nsp = NULL;
                GNode * node = NULL;
                char caption [] = "LinuxHA Resource Group";                

                node = (GNode *) current->data;
                if ( node == NULL || GetResNodeData(node) == NULL ||
                     GetResType(GetResNodeData(node)) == RESOURCE){
                        goto next;
                }

                group_id = GetGroupInfo(GetResNodeData(node))->id;

                if ( group_id == NULL ) {
                        goto next;
                }

                /* create an object */
                nsp = CMGetNameSpace(ref, rc);
                op = CMNewObjectPath(broker, (char *)nsp->hdl, classname, rc);

                if ( CMIsNullObject(op) ){
                        free_res_tree ( root);
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
                                free_res_tree (root);
                                return HA_FAIL;
                        }
                        
                        CMSetProperty(inst, "GroupId", group_id, CMPI_chars);
                        CMSetProperty(inst, "Caption", caption, CMPI_chars);
                        CMReturnInstance(rslt, inst);
 
                }
        next:
                current = current->next;
                if ( current == list ) {
                        break;
                }
        }
        
        CMReturnDone(rslt);

        free_res_tree (root);
        /* FIXME: free list */

        return HA_OK;
}


int 
get_inst_res_group(char * classname, CMPIBroker * broker,
                   CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * ref, CMPIStatus * rc)
{
        CMPIData key_data;
        CMPIObjectPath * op = NULL;
        CMPIInstance * inst = NULL;

        char * group_id = NULL;
        char * nsp = NULL;
        char caption [] = "LinuxHA Resource Group";

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
        CMSetProperty(inst, "Caption", caption, CMPI_chars);
        CMReturnInstance(rslt, inst);
        CMReturnDone(rslt);

        return HA_OK;
}
