#ifndef __libhasubagent_h__
#define __libhasubagent_h__

#include <clplumbing/cl_log.h>
#include "hb_api.h"

#include <sys/types.h>

typedef enum ha_group {
	CLUSTERINFO,
	NODEINFO,
	IFINFO,
	RESOURCEINFO,
} ha_group_t;

typedef enum ha_attribute {
	/* heartbeat parameter */
	NODE_COUNT, 

	/* node parameter */
	NODE_NAME,
	NODE_TYPE,
	NODE_STATUS,
	NODE_IF_COUNT,

	/* if parameter */
	IF_NAME,
	IF_STATUS,
	IF_NODE_ID,
	IF_ID,

	/* resource parameter */

} ha_attribute_t;


int init_heartbeat(void);
int get_heartbeat_fd(void);
int handle_heartbeat_msg(void);

/* functions specific for snmp request */
int get_count(ha_group_t group, size_t * count);

int get_int32_value(ha_group_t group, ha_attribute_t attrib, size_t index, int32_t * value);

int get_str_value(ha_group_t group, ha_attribute_t attrib, size_t index, const char * * value);

#endif // __libhasubagent_h__
