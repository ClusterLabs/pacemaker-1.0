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

#include <hb_api.h>
#include <clplumbing/cl_log.h>

#include "cmpi_hosted_resource.h"
#include "cmpi_utils.h"

int
node_host_resource(CMPIInstance * node_inst, 
                   CMPIInstance * resource_inst, CMPIStatus * rc)
{
        CMPIString * node_name = NULL;
        CMPIString * hosting_node = NULL;
        int hosted = 0;

        DEBUG_ENTER();

        if ( CMIsNullObject( node_inst) || CMIsNullObject( resource_inst) ) {
                cl_log(LOG_INFO, "%s: NULL instance", __FUNCTION__);
                hosted = 0;
                goto out;

        }        

        node_name = CMGetProperty(node_inst, "Name", rc).value.string;

        if ( CMIsNullObject(node_name) ) {
                cl_log(LOG_INFO, "%s: node_name is NULL", __FUNCTION__);
                hosted = 0;
                goto out;
        }

        cl_log(LOG_INFO, "%s: OpenWBEM segment fault here", __FUNCTION__);
        hosting_node = CMGetProperty(resource_inst, 
                                     "HostingNode", rc).value.string;
        cl_log(LOG_INFO, "%s: OpenWBEM never get here", __FUNCTION__);

        if ( CMIsNullObject(hosting_node) ){
                cl_log(LOG_INFO, "%s: hosting node is NULL", __FUNCTION__);
                hosted = 0;
                goto out;
        }

        if (strcmp( CMGetCharPtr(hosting_node), CMGetCharPtr(node_name)) == 0){
                cl_log(LOG_INFO, "%s: host resource", __FUNCTION__);
                hosted = 1;
                goto out;
        }
out:
        DEBUG_LEAVE();

        return hosted;
}


