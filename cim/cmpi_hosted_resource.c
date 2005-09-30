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
#include "cmpi_hosted_resource.h"
#include "cmpi_utils.h"

int
node_host_resource(CMPIInstance * node_inst, 
                   CMPIInstance * resource_inst, CMPIStatus * rc)
{
        CMPIData node_name;
        CMPIData hosting_node;
        int hosted = 0;

        DEBUG_ENTER();
        node_name = CMGetProperty(node_inst, "Name", rc);

        if ( node_name.value.string == NULL ) {
                cl_log(LOG_INFO, "node_name is NULL");
                hosted = 0;
                goto out;
        }

        hosting_node = CMGetProperty(resource_inst, 
                                    "HostingNode", rc);                        

        if ( hosting_node.value.string == NULL ){
                cl_log(LOG_INFO, "hosting node is NULL");
                hosted = 0;
                goto out;
        }

        if ( strcmp((char *)hosting_node.value.string->hdl, 
                        (char *)node_name.value.string->hdl) == 0){
                hosted = 1;                                             
        }
out:
        DEBUG_LEAVE();

        return hosted;
}


