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

#define METHOD_ADD_RESOURCE		"AddResource"
#define METHOD_ADD_PRIMITIVE_RESOURCE	"AddPrimitiveResource"

#endif
