/*
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 *         Jia Ming Pan (jmltc@cn.ibm.com)
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

#ifndef _LINUXHA_INFO_H
#define _LINUXHA_INFO_H

#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <clplumbing/cl_uuid.h>
#include <hb_api.h>
#include "saf/ais.h"


#define CACHE_TIME_OUT 5

typedef enum lha_group {
	LHA_CLUSTERINFO,
	LHA_NODEINFO,
	LHA_IFSTATUSINFO,
	LHA_RESOURCEINFO,
	LHA_MEMBERSHIPINFO,
	LHA_HBCONFIGINFO,
} lha_group_t;

typedef enum lha_attribute {
	/* LHA_CLUSTERINFO stats */
	TOTAL_NODE_COUNT,
	LIVE_NODE_COUNT,
	RESOURCE_GROUP_COUNT,
	CURRENT_NODE_ID,

	/* LHA_RESOURCEINFO stats */
	RESOURCE_STATUS,
} lha_attribute_t;

extern unsigned long clm_initialized;

const char * get_status(uint32_t status_value);

int get_int_value(lha_group_t group, lha_attribute_t attr, size_t index, uint32_t * value);
int get_str_value(lha_group_t group, lha_attribute_t attr, size_t index, char * * value);
int clusterinfo_get_int_value(lha_attribute_t attr, size_t index, uint32_t * value);
int rsinfo_get_int_value(lha_attribute_t attr, size_t index, uint32_t * value);
int hbconfig_get_str_value(const char * attr, char * * value);

int get_hb_initialized (void);
char * get_hb_client_id (void);

/*************************************************
 * event handler
 ************************************************/

typedef int (*nodestatus_event_handler_t)(const char * node, const char * status);
typedef int (*ifstatus_event_handler_t)(const char * node, const char * lnk, const char * status);
typedef int (*membership_event_handler_t)(const char * node, SaClmClusterChangesT status);

typedef struct event_handler_set_s{
        nodestatus_event_handler_t nodestatus_event;
        ifstatus_event_handler_t ifstatus_event;
        membership_event_handler_t membership_event; 
        
} ha_event_handler_set_t;

int linuxha_register_event_handler(nodestatus_event_handler_t, 
                ifstatus_event_handler_t, membership_event_handler_t);
	
int linuxha_unregister_event_handler(void);


/*****************************************
 * infomation
 ****************************************/

GPtrArray * get_hb_info(lha_group_t group);

struct hb_nodeinfo {
	size_t id;
	char * name;
	uint32_t type;
	uint32_t status;
	size_t ifcount;
	cl_uuid_t uuid;
};

struct hb_ifinfo {
	size_t id;
	size_t nodeid;
	char * name;
	char * node;
	uint32_t status;
};

struct hb_rsinfo {
	size_t id;
	size_t masternodeid;
	char * resource;
	uint32_t status;
};


int linuxha_initialize(const char * client_id, int force);
int linuxha_finalize(void);


int init_logger(const char * entity);

int init_heartbeat(void);
int free_heartbeat(void);

int hb_set_callbacks(void);

int get_heartbeat_fd(void);
int handle_heartbeat_msg(void);

int init_membership(void);
int get_membership_fd(void);
int handle_membership_msg(void);

int init_resource_table(void);

int init_storage(void);
void free_storage(void);

int walk_nodetable(void);
int walk_iftable(void);

int ping_membership(int * mem_fd);



#endif



