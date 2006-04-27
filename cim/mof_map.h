/*
 * mof_map.h: map class properties to msg attributes
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


#ifndef _MOF_MAP_H
#define _MOF_MAP_H

typedef struct {
	const char * key;
        const char * name;
        int	type;
} map_entry_t;

struct map_t{
	int	id;
	int	len;
	const 	map_entry_t *entry;
};

const struct map_t *	cim_query_map(int mapid); 

#define HA_CLUSTER			1
#define HA_CLUSTER_NODE			2
#define HA_PRIMITIVE_RESOURCE		3
#define HA_RESOURCE_CLONE		4
#define HA_MASTERSLAVE_RESOURCE		5
#define HA_OPERATION			6
#define HA_ORDER_CONSTRAINT		7
#define HA_COLOCATION_CONSTRAINT	8
#define HA_LOCATION_CONSTRAINT		9
#define HA_RESOURCE_GROUP		10
#define HA_INSTANCE_ATTRIBUTES		11
#define HA_LOCATION_CONSTRAINT_RULE	12

#define METHOD_ADD_RESOURCE		"AddResource"
#define METHOD_ADD_PRIMITIVE_RESOURCE	"AddPrimitiveResource"


#define CLASS_HA_CLUSTER		"HA_Cluster"
#define CLASS_HA_CLUSTER_NODE		"HA_ClusterNode"
#define CLASS_HA_CLUSTER_RESOURCE	"HA_ClusterResource"

#define CLASS_HA_SOFTWARE_IDENTITY	"HA_SoftwareIdentity"
#define CLASS_HA_INSTALLED_SOFTWARE_IDENTITY	"HA_InstalledSoftwareIdentity"
#define CLASS_HA_PRIMITIVE_RESOURCE	"HA_PrimitiveResource"
#define CLASS_HA_RESOURCE_GROUP		"HA_ResourceGroup"
#define CLASS_HA_RESOURCE_CLONE		"HA_ResourceClone"
#define CLASS_HA_MSTERSLAVE_RESOURCE	"HA_MasterSlaveResource"
#define CLASS_HA_INSTANCE_ATTRIBUTES	"HA_InstanceAttributes"
#define CLASS_HA_ATTRIBUTES_OF_RESOURCE	"HA_AttributesOfResource"
#define CLASS_HA_OPERATION		"HA_Operation"
#define CLASS_HA_OPERATION_ON_RESOURCE	"HA_OperationOnResource"
#define CLASS_HA_RESOURCE_CONSTRAINT	"HA_ResourceConstraint"
#define CLASS_HA_ORDER_CONSTRAINT	"HA_OrderConstraint"
#define CLASS_HA_COLOCATION_CONSTRAINT	"HA_ColocationConstraint"
#define CLASS_HA_LOCATION_CONSTRAINT	"HA_LocationConstraint"

#define CLASS_HA_SUBRESOURCE		"HA_SubResource"
#define CLASS_HA_PARTICIPATING_NODE	"HA_ParticipatingNode"
#define CLASS_HA_HOSTED_RESOURCE	"HA_HostedResource"
#define CLASS_HA_INDICATION		"HA_Indication"
#define CLASS_HA_CLUSTERING_SERVICE	"HA_ClusteringService"


#endif
