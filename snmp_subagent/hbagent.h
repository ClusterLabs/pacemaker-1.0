#ifndef __hasubagent_h__
#define __hasubagent_h__

#include <glib.h>
#include "saf/ais.h"

/* funcs and structs used by the SNMP agent */

#define CACHE_TIME_OUT 5

typedef enum lha_group {
	LHA_CLUSTERINFO,
	LHA_NODEINFO,
	LHA_IFINFO,
	LHA_RESOURCEINFO,
	LHA_MEMBERSHIPINFO,
	LHA_HBCONFIGINFO,
} lha_group_t;

typedef enum lha_attribute {
	/* LHA_CLUSTERINFO stats */
	TOTAL_NODE_COUNT,
	LIVE_NODE_COUNT,

	/* LHA_RESOURCEINFO stats */
	RESOURCE_STATUS,
} lha_attribute_t;


int init_heartbeat(void);
int get_heartbeat_fd(void);
int handle_heartbeat_msg(void);

int init_membership(void);
int get_membership_fd(void);
int handle_membership_msg(void);

int init_resource_table(void);

int get_int_value(lha_group_t group, lha_attribute_t attr, size_t index, int32_t * value);

int get_str_value(lha_group_t group, lha_attribute_t attr, size_t index, char * * value);

int clusterinfo_get_int_value(lha_attribute_t attr, size_t index, int32_t * value);

int rsinfo_get_int_value(lha_attribute_t attr, size_t index, int32_t * value);

int hbconfig_get_str_value(const char * attr, char * * value);

GArray * get_hb_info(lha_group_t group);

/* funcs and structs used by the heartbeat client */

int walk_node_table(void);
int walk_if_table(void);

struct hb_nodeinfo {
	char * name;
	char * type;
	char * status;
	size_t ifcount;
};

struct hb_ifinfo {
	size_t nodeid;
	size_t id;
	char * name;
	char * node;
	char * status;
};

struct hb_rsinfo {
    	char * master;
	char * resource;
	int    status;
};

#endif /* __hasubagent_h__ */



