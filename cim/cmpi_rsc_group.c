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

/* temporary implementaion */


static int 
enumerate_sub_resource_names(const char * group_id, 
                                GPtrArray * rsc_name_array)
{
        char ** std_out = NULL;
        char cmnd [] = HA_LIBDIR"/heartbeat/crmadmin -R";
        int exit_code = 0;
        int ret = 0;
        int i = 0;
        int in_group = 0;

        char * rsc_name = NULL;

        DEBUG_ENTER();

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
        return HA_OK;
} 

static int
set_rg_instance_properties(CMPIBroker * broker, CMPIInstance *inst, 
       char * group_id, const GPtrArray * rsc_name_array, CMPIStatus * rc)
{
        int array_length = 0;
        CMPIArray * cmpi_array = NULL;
        int i = 0;

        DEBUG_ENTER();

        array_length = rsc_name_array->len;

        cmpi_array = CMNewArray(broker, array_length, CMPI_chars, rc);    

        if ( CMIsNullObject(cmpi_array) ) {
                DEBUG_LEAVE();
                return HA_FAIL;
        }


        for ( i = 0; i < rsc_name_array->len ; i++ ) {
                char * rsc_name = NULL;

                rsc_name = (char *) g_ptr_array_index(rsc_name_array, i);

                if ( rsc_name == NULL ) {
                        cl_log(LOG_WARNING, 
                              "%s: got a NULL value, continue", __FUNCTION__);
                        continue;
                }
                cl_log(LOG_INFO, "%s: add %s to cmpi array", 
                                                __FUNCTION__, rsc_name);

                CMSetArrayElementAt(cmpi_array, i, rsc_name, CMPI_chars);
        }


        cl_log(LOG_INFO, "%s: cmpi array count: %d", 
                        __FUNCTION__, CMGetArrayCount(cmpi_array, rc));

        CMSetProperty(inst, "GroupId", group_id, CMPI_chars);

        cl_log(LOG_INFO, 
               "%s: setting array property, OpenWBEM segment fault here?", 
                __FUNCTION__);

        CMSetProperty(inst, "SubResourceNames", &cmpi_array, CMPI_charsA);

        DEBUG_LEAVE();

        return HA_OK;
}


int 
enumerate_resource_groups(char * classname, CMPIBroker * broker,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, int enum_inst, CMPIStatus * rc)
{
        CMPIObjectPath* op = NULL;

        char ** std_out = NULL;
        char cmnd [] = HA_LIBDIR"/heartbeat/crmadmin -R";
        int exit_code = 0;
        int ret = 0, i = 0;

        ret = run_shell_command(cmnd, &exit_code, &std_out, NULL);

        for (i = 0; std_out[i]; i++){
                char * group_id = NULL;
                char ** match = NULL;
                CMPIString * nsp = NULL;
 
                /* create an object */
                nsp = CMGetNameSpace(ref, rc);
                op = CMNewObjectPath(broker, (char *)nsp->hdl, classname, rc);

                if ( CMIsNullObject(op) ){
                        return HA_FAIL;
                }

                ret = regex_search("group: (.*) \\((.*)\\)", 
                                                std_out[i], &match);

                if ( ret != HA_OK ) {
                        continue;
                }

                /* parse the result */
                group_id = match[1];
                cl_log(LOG_INFO, "%s: setting keys: group_id = [%s] ", 
                                                __FUNCTION__, group_id);
                        
                if ( ! enum_inst ) { /* just enumerate names */
                        /* add keys */
                        CMAddKey(op, "GroupId", group_id, CMPI_chars);
       
                        /* add object to rslt */
                        CMReturnObjectPath(rslt, op);

                        /* free memory allocated */
                        free_2d_array(match);

                }else{  /* return instance */
                        GPtrArray * rsc_name_array = NULL;
                        rsc_name_array = g_ptr_array_new();
                
                        ret = enumerate_sub_resource_names(group_id, 
                                                           rsc_name_array);
                        if ( ret == HA_OK ) {
                                CMPIInstance * inst = NULL;
                                inst = CMNewInstance(broker, op, rc);

                                if ( inst == NULL ) {
                                        return HA_FAIL;
                                }
                                cl_log(LOG_INFO, 
                                        "%s: ready to set instance", __FUNCTION__);

                                set_rg_instance_properties(broker, inst, 
                                                group_id, rsc_name_array, rc);

                                CMReturnInstance(rslt, inst);
                                g_ptr_array_free(rsc_name_array, 0);
                        }        
                }

        }
        
        free_2d_array(std_out);
        CMReturnDone(rslt);

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

        GPtrArray * rsc_name_array = NULL;
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

        rsc_name_array = g_ptr_array_new();
        enumerate_sub_resource_names(group_id, rsc_name_array);

        cl_log(LOG_INFO, "%s: ready to set instance", __FUNCTION__);
        set_rg_instance_properties(broker, inst,
                        group_id, rsc_name_array, rc);

        CMReturnInstance(rslt, inst);
        CMReturnDone(rslt);

        g_ptr_array_free(rsc_name_array, 0);

        
        return HA_OK;
}
