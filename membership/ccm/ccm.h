/* $Id: ccm.h,v 1.23 2004/09/20 18:59:16 msoffen Exp $ */
/*
 * ccm.h: definitions Consensus Cluster Manager internal header
 *				file
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _CLUSTER_MANAGER_H_
#define _CLUSTER_MANAGER_H_
 
/* MUST BE INCLUDED for configure time ifdef vars */
#include <portability.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <assert.h>
#include <glib.h>
#include <ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/GSource.h>
#include <clplumbing/realtime.h>
#include <base64.h>

#include <ha_config.h>
#include <heartbeat.h>
#include <hb_api.h>


/* BEGINNING OF version request tracking interfaces */
typedef struct ccm_version_s {
	longclock_t time;
	int	numtries;
	int	n_resp; /* keeps track of the number of version */
				/* responses recevied from other nodes */
				/* after we received the first response. */
} ccm_version_t;
void version_reset(ccm_version_t *);
void version_some_activity(ccm_version_t *);
int version_retry(ccm_version_t *, longclock_t);
void version_inc_nresp(ccm_version_t *);
void version_set_nresp(ccm_version_t *, int);
unsigned int version_get_nresp(ccm_version_t *);
#define VER_TRY_AGAIN 1
#define VER_NO_CHANGE 2
#define VER_TRY_END   3

/* END OF version request tracking interfaces */



/* BEGINING OF Low Level Membership interfaces */
#define NODEIDSIZE 255 /* if this value is changed, change it
			  	also in ccmlib.h */
#define STATUSSIZE 15
#define CCMFIFO    "/var/lib/heartbeat/ccm/ccm" /* if this value is
			changed change it also in ccmlib.h */

typedef struct NodeList_s {
		uint  NodeUuid;  /* a cluster unique id for the node */
		char NodeID[NODEIDSIZE];
		char Status[STATUSSIZE];
		uint received_change_msg;
} NodeList_t;
typedef struct llm_info_s { /* information about low level membership info */
	uint	   llm_nodeCount; /*number of nodes in the cluster  */
	int	   llm_mynode;	 /*index of mynode */
	NodeList_t llm_nodes[MAXNODE];  /*information of each node */
} llm_info_t;
#define CLUST_INACTIVE  "inctv"
#define LLM_GET_MYNODE(llm) llm->llm_mynode
#define LLM_GET_NODECOUNT(llm) llm->llm_nodeCount
#define LLM_GET_UUID(llm,i) llm->llm_nodes[i].NodeUuid
#define LLM_GET_MYUUID(llm) LLM_GET_UUID(llm, LLM_GET_MYNODE(llm))
#define LLM_GET_NODEID(llm,i) llm->llm_nodes[i].NodeID
#define LLM_GET_MYNODEID(llm) LLM_GET_NODEID(llm, LLM_GET_MYNODE(llm))
#define LLM_GET_STATUS(llm,i) llm->llm_nodes[i].Status
#define LLM_SET_MYNODE(llm,indx) llm->llm_mynode = indx
#define LLM_SET_NODECOUNT(llm, count) llm->llm_nodeCount = count
#define LLM_INC_NODECOUNT(llm) (llm->llm_nodeCount)++
#define LLM_SET_UUID(llm,i, uuid) llm->llm_nodes[i].NodeUuid = uuid
#define LLM_SET_MYUUID(llm, uuid) LLM_SET_UUID(llm, LLM_GET_MYNODE(llm), uuid)
#define LLM_SET_NODEID(llm, i, name)  \
			(strncpy(llm->llm_nodes[i].NodeID,name,NODEIDSIZE))
#define LLM_SET_MYNODEID(llm, name) \
			LLM_SET_NODEID(llm, LLM_GET_MYNODE(llm), name)
#define LLM_SET_STATUS(llm,i,status) \
			(strncpy(llm->llm_nodes[i].Status,status,STATUSSIZE))
#define LLM_COPY(llm,dst,src) (llm->llm_nodes[dst] = llm->llm_nodes[src])
#define LLM_GET_NODEIDSIZE(llm) NODEIDSIZE
int llm_get_active_nodecount(llm_info_t *);
gboolean llm_only_active_node(llm_info_t *);
int llm_get_uuid(llm_info_t *, const char *);
char *llm_get_nodeid_from_uuid(llm_info_t *, const int );
int llm_nodeid_cmp(llm_info_t *, int , int );
int llm_status_update(llm_info_t *, const char *, const char *);
void llm_init(llm_info_t *);
void llm_end(llm_info_t *);
int llm_is_valid_node(llm_info_t *, const char *);
void llm_add(llm_info_t *, const char *, const char *, const char *);
int llm_get_index(llm_info_t *, const char *);
/* END OF Low Level Membership interfaces */


/* ccm prototypes */
int ccm_str2bitmap(const char *, unsigned char **);
int ccm_bitmap2str(const unsigned char *, int , char **);
longclock_t ccm_get_time(void);
int ccm_timeout(longclock_t, longclock_t, unsigned long);
int ccm_need_control(void *);
int ccm_take_control(void *);
void* ccm_initialize(void);
int ccm_get_fd(void *);
void ccm_send_init_state(void *);
void ccm_check_memoryleak(void);

/* BEGINING OF update interfaces */
/* structure that keeps track of new joining requests. */
typedef struct update_s {
	int 	index; /* index of the node in the ccm_llm table */
	int	uptime;/* uptime as specified by the node */
} update_t;

typedef struct ccm_update_s {
	int	leader;
	uint	nodeCount;
	longclock_t  inittime;
	update_t update[MAXNODE];
	GSList *cl_head; /* a linked list of cached cluster leader  */
				  /*  requests  */
} ccm_update_t;
#define UPDATE_GET_LEADER(updt) updt->leader
#define UPDATE_GET_NODECOUNT(updt) updt->nodeCount
#define UPDATE_GET_INITTIME(updt) updt->inittime
#define UPDATE_GET_INDEX(updt, i) updt->update[i].index
#define UPDATE_GET_UPTIME(updt, i) updt->update[i].uptime
#define UPDATE_GET_CLHEAD(updt) (updt)->cl_head
#define UPDATE_SET_LEADER(updt, lead) updt->leader = lead
#define UPDATE_SET_NODECOUNT(updt, count) updt->nodeCount = count
#define UPDATE_SET_INITTIME(updt, time) updt->inittime = time
#define UPDATE_SET_INDEX(updt, i, value) updt->update[i].index = value
#define UPDATE_SET_UPTIME(updt, i, value) updt->update[i].uptime = value
#define UPDATE_SET_CLHEAD(updt, ptr) (updt)->cl_head = ptr
#define UPDATE_INCR_NODECOUNT(updt) (updt->nodeCount)++
#define UPDATE_DECR_NODECOUNT(updt) (updt->nodeCount)--


void update_add_memlist_request(ccm_update_t *, llm_info_t *, const char *, const int);
void update_free_memlist_request(ccm_update_t *);
void update_reset(ccm_update_t *);
void update_init(ccm_update_t *);
int update_timeout_expired(ccm_update_t *, unsigned long);
int update_any(ccm_update_t *);
void update_add(ccm_update_t *, llm_info_t *, const char *, int, gboolean);
void update_remove(ccm_update_t *, llm_info_t *, const char *);
int update_am_i_leader(ccm_update_t *, llm_info_t *);
int update_can_be_leader(ccm_update_t *,  llm_info_t *llm, const char *, int );
char * update_get_cl_name(ccm_update_t *, llm_info_t *);
void * update_initlink(ccm_update_t *);
char * update_next_link(ccm_update_t *, llm_info_t *, void *, uint *);
void update_freelink(ccm_update_t *, void *);
int update_get_next_uuid(ccm_update_t *, llm_info_t *, int *);
int update_strcreate(ccm_update_t *, char **, llm_info_t *);
void update_strdelete(char *memlist);
int update_is_member(ccm_update_t *, llm_info_t *, const char *);
int update_get_uptime(ccm_update_t *, llm_info_t *, int );
/* END OF update interfaces */



/* BEGINNING OF graph interfaces */

typedef struct vertex_s {
                unsigned char  *bitmap; /* bitmap sent by each node */
                int    count;   /* connectivity number for each node */
                int    uuid;   /* the uuid of the node */
} vertex_t;

typedef struct graph_s {
        vertex_t  *graph_node[MAXNODE];
        int        graph_nodes;/* no of nodes that had sent the join message */
                                /*  whose bitmaps we are now expecting */
        int        graph_rcvd; /* no of nodes that have sent a memlistbitmap */
} graph_t;  
graph_t * graph_init(void);
void graph_free(graph_t *);
void graph_add_uuid(graph_t *, int );
void graph_update_membership(graph_t *, int , unsigned char *);
int  graph_filled_all(graph_t *);
int graph_get_maxclique(graph_t *, unsigned char **);
void graph_add_to_membership(graph_t *, int, int);
/* END OF graph interfaces */


/* BEGINNING OF bitmap interfaces */
int bitmap_create(unsigned char **, int);
void bitmap_delete(unsigned char *);
void bitmap_mark(int, unsigned char *, int);
void bitmap_clear(int, unsigned char *, int);
int bitmap_test(int, const unsigned char *, int);
int bitmap_count(const unsigned char *, int);
void bitmap_print(unsigned char *, int, char *);
void bitmap_reset(unsigned char *, int);
int  bitmap_size(int);
int  bitmap_copy(unsigned char *, unsigned char *);
/* END OF bitmap interfaces */


size_t strnlen(const char *, size_t); /*TOBEDONE*/
/* end ccm */

/* BEGINNING OF client management interfaces */
void client_init(void);
int  client_add(struct IPC_CHANNEL *);
void client_delete(struct IPC_CHANNEL *);
void client_delete_all(void);
void client_new_mbrship(int n, int , int *, gboolean, void *);
void client_llm_init(llm_info_t *);
void client_influx(void);
void client_evicted(void);
/* END OF client management interfaces */

#endif /*  _CLUSTER_MANAGER_H_ */
