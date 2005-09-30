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


#ifndef _CMPI_HOSTED_RESOURCE_H
#define _CMPI_HOSTED_RESOURCE_H

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>


#define REF_NODE                "Antecedent"
#define REF_RESOURCE            "Dependent"
#define NODE_CLASSNAME          "LinuxHA_ClusterNode"
#define RESOURCE_CLASSNAME      "LinuxHA_ClusterResource"


int node_host_resource(CMPIInstance * node_inst, 
                       CMPIInstance * resource_inst, CMPIStatus * rc);



#endif
