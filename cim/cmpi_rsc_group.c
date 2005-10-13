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
                        CMPIInstance * inst = NULL;
                        inst = CMNewInstance(broker, op, rc);

                        if ( inst == NULL ) {
                                return HA_FAIL;
                        }
                        cl_log(LOG_INFO, 
                               "%s: ready to set instance", __FUNCTION__);
                        
                        CMSetProperty(inst, "GroupId", group_id, CMPI_chars);
                        
                        CMReturnInstance(rslt, inst);
 
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
