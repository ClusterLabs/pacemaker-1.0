/* $Id: testclient.c,v 1.3 2004/02/17 22:12:01 lars Exp $ */
#include "haclient.h"

int main(void)
{
	size_t count;
	const char * str;
	int i;

	if (init_heartbeat() != HA_OK) {
		printf("init_heartbeat error\n");
		return -1;
	}
	if (get_count(NODEINFO, &count) != HA_OK) {
		printf("get_node_count error\n");
	}
	for (i = 0; i < count; i++) {
		if (get_str_value(NODEINFO, NODE_NAME, i, &str) != HA_OK) {
			printf("get_node_info error, i = %d\n", 
					i);
		}
                printf("b4 getting node info. node = %s\n", str);
	}
	return 0;
}
