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

#include "clplumbing/cl_log.h"
#include "cmpi_utils.h"

#include "cmpi_sub_resource.h"


int group_contain_resource(CMPIInstance * group_inst,
                                CMPIInstance * resource_inst, CMPIStatus * rc)
{
        CMPIString * rsc_name = NULL;
        CMPIArray * sub_rsc_names = NULL;
        int count = 0;
        int contain = 0;
        int i = 0;

        DEBUG_ENTER();

        /* resource's name */
        rsc_name = CMGetProperty(resource_inst, "Name", rc).value.string;

        if ( rsc_name == NULL ) {
                cl_log(LOG_INFO, "%s: resource_name =  NULL", __FUNCTION__);
                contain = 0;
                goto out;
        }

        /* sub_resource's names in group */
        sub_rsc_names = CMGetProperty(group_inst,
                                    "SubResourceNames", rc).value.array;

        if ( sub_rsc_names == NULL ){
                cl_log(LOG_INFO, 
                        "%s: sub_resource_names = NULL", __FUNCTION__);
                contain = 0;
                goto out;
        }

        count = CMGetArrayCount(sub_rsc_names, rc);

        for ( i = 0; i < count; i++ ){
                CMPIString * this_rsc_name = NULL;
                this_rsc_name = 
                        CMGetArrayElementAt(sub_rsc_names, i, rc).value.string;
                if ( this_rsc_name == NULL ) {
                        continue;
                }
                cl_log(LOG_INFO, "%s: got element at %d: %s",
                                __FUNCTION__, i, (char *)this_rsc_name->hdl);

                if ( strcmp((char *)this_rsc_name->hdl,
                            (char *)rsc_name->hdl) == 0){
                        contain = 1;
                        break;
                }
        }
out:
        DEBUG_LEAVE();

        return contain;

}
