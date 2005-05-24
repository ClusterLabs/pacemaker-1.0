/* $Id: ccm.h,v 1.35 2005/05/24 18:59:47 gshi Exp $ */
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

#include <ocf/oc_event.h>

#define BitsInByte CHAR_BIT

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
#define CCMFIFO    HA_VARLIBDIR "/heartbeat/ccm/ccm" /* if this value is
			changed change it also in ccmlib.h */

typedef struct llm_node_s {
	uint  uuid;  /* a cluster unique id for the node */
	gboolean join_request;
	char nodename[NODEIDSIZE];
	char status[STATUSSIZE];
	uint received_change_msg;
}llm_node_t;

typedef struct llm_info_s { /* information about low level membership info */
	uint	   nodecount; /*total number of nodes in the cluster  */
	int	   myindex;	 /*index of mynode */
	llm_node_t nodes[MAXNODE];  /*information of each node */
} llm_info_t;

#define CLUST_INACTIVE  "inctv"
#define LLM_GET_MYNODE(llm) llm->myindex
#define LLM_GET_NODECOUNT(llm) llm->nodecount
#define LLM_GET_UUID(llm,i) llm->nodes[i].uuid
#define LLM_GET_MYUUID(llm) LLM_GET_UUID(llm, LLM_GET_MYNODE(llm))
#define LLM_GET_NODEID(llm,i) llm->nodes[i].nodename
#define LLM_GET_MYNODEID(llm) LLM_GET_NODEID(llm, LLM_GET_MYNODE(llm))
#define LLM_GET_STATUS(llm,i) llm->nodes[i].status
#define LLM_SET_MYNODE(llm,indx) llm->myindex = indx
#define LLM_SET_NODECOUNT(llm, count) llm->nodecount = count
#define LLM_INC_NODECOUNT(llm) (llm->nodecount)++
#define LLM_SET_UUID(llm,i, _uuid) llm->nodes[i].uuid = _uuid
#define LLM_SET_MYUUID(llm, _uuid) LLM_SET_UUID(llm, LLM_GET_MYNODE(llm), _uuid)
#define LLM_SET_NODEID(llm, i, name)  \
			(strncpy(llm->nodes[i].nodename,name,NODEIDSIZE))
#define LLM_SET_MYNODEID(llm, name) \
			LLM_SET_NODEID(llm, LLM_GET_MYNODE(llm), name)
#define LLM_SET_STATUS(llm,i,status) \
			(strncpy(llm->nodes[i].status,status,STATUSSIZE))
#define LLM_COPY(llm,dst,src) (llm->nodes[dst] = llm->nodes[src])
#define LLM_GET_NODEIDSIZE(llm) NODEIDSIZE
int llm_get_live_nodecount(llm_info_t *);
gboolean llm_only_active_node(llm_info_t *);
int llm_get_uuid(llm_info_t *, const char *);
char *llm_get_nodeid_from_uuid(llm_info_t *, const int );
int llm_nodeid_cmp(llm_info_t *, int , int );
int llm_status_update(llm_info_t *, const char *, const char *, char*);

/*	Get the number of nodes that are inactive 
 *	inactive means this node is STONITHed
 *	therefore we don't count that in quorum computation
 */
int	llm_get_inactive_node_count(llm_info_t *llm);

void	display_llm(llm_info_t *llm);
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
IPC_Channel * ccm_get_ipcchan(void *);
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
void client_llm_init(llm_info_t *);
void client_influx(void);
void client_evicted(void);
/* END OF client management interfaces */





/* */
/* the various states of the CCM state machine. */
/* */
enum ccm_state  {
	CCM_STATE_NONE=0,		/* is in NULL state  */
	CCM_STATE_VERSION_REQUEST,	/* sent a request for protocol version */
	CCM_STATE_JOINING,  		/* has initiated a join protocol  */
	CCM_STATE_RCVD_UPDATE,	/* has recevied the updates from other nodes */
	CCM_STATE_SENT_MEMLISTREQ,	/* CL has sent a request for member list  */
					/* this state is applicable only on CL */
	CCM_STATE_REQ_MEMLIST,	/* CL has requested member list */
				  	/* this state is applicable only on non-CL */
	CCM_STATE_MEMLIST_RES,	/* Responded member list to the Cluster  */
				 	/* Leader */
	CCM_STATE_JOINED,    /* PART of the CCM cluster membership! */
	CCM_STATE_WAIT_FOR_MEM_LIST,
	CCM_STATE_WAIT_FOR_CHANGE,
	CCM_STATE_NEW_NODE_WAIT_FOR_MEM_LIST,
	CCM_STATE_END
};

/* the times for repeating sending message */
#define REPEAT_TIMES 10

/* add new enums to this structure as and when new protocols are added */
enum ccm_protocol {
	CCM_VER_NONE = 0,
	CCM_VER_1,
	CCM_VER_LAST
};

typedef struct ccm_proto_s {
	enum ccm_protocol  com_hiproto;/* highest protocol version that  */
				/* this node can handle */
	int	com_active_proto;/* protocol version */
} ccm_proto_t;


typedef struct memcomp_s {
	graph_t		*mem_graph;  /* memlist calculation graph */

	GSList 		*mem_maxt; 	    /* the maxtrans of each node */
				    /* participating in the computation . */
				    /* NOTE: the transition number of the */
				    /* next transition is always 1 higher */
				    /* than that of all transitions seen  */
				    /* by each node participating in the  */
				    /* membership */
	longclock_t  	mem_inittime; /* the time got intialized */
} memcomp_t;
#define 	MEMCOMP_GET_GRAPH(memc)  	memc->mem_graph
#define 	MEMCOMP_GET_MAXT(memc)  	memc->mem_maxt
#define 	MEMCOMP_GET_INITTIME(memc)  	memc->mem_inittime
#define 	MEMCOMP_SET_GRAPH(memc, gr)  	memc->mem_graph=gr
#define 	MEMCOMP_SET_MAXT(memc, list)  	memc->mem_maxt=list
#define 	MEMCOMP_SET_INITTIME(memc,time)	memc->mem_inittime=time


typedef struct ccm_tmout_s {
	long	iff;  /* membership_Info_From_Followers_timeout */
	long	itf;  /* membership_Info_To_Followers_timeout */
	long	fl;  /* membership_Final_List_timeout */
	long	u;  /* update timeout */
	long	lu;  /* long update timeout */
	long	vrs;  /* version timeout */
} ccm_tmout_t;

enum change_event_type{
    TYPE_NONE,	
    NODE_LEAVE,
    NEW_NODE
};

#define COOKIESIZE 15
typedef struct ccm_info_s {
	llm_info_t 	llm;	/*  low level membership info */
	
	int		ccm_nodeCount;	/*  number of nodes in the ccm cluster */
	int		ccm_member[MAXNODE];/* members of the ccm cluster */
	memcomp_t	ccm_memcomp;	/* the datastructure to compute the  */
					/* final membership for each membership */
	 				/* computation instance of the ccm protocol. */
	 				/* used by the leader only. */

	ccm_proto_t  	ccm_proto;	/* protocol version information */
#define ccm_active_proto ccm_proto.com_active_proto
#define ccm_hiproto	  ccm_proto.com_hiproto

	char		ccm_cookie[COOKIESIZE];/* context identification string. */
	uint32_t	ccm_transition_major;/* transition number of the cluster */
	int		ccm_cluster_leader; /* cluster leader of the last major */
				/* transition. index of cl in ccm_member table */
	int		ccm_joined_transition;
					/* this indicates the major transition  */
					/* number during which this node became */
					/* a member of the cluster. */
					/* A sideeffect of this is it also */
					/* is used to figure out if this node */
					/* was ever a part of the cluster. */
					/* Should be intially set to 0 */
	uint32_t	ccm_max_transition;	/* the maximum transition number seen */
					/* by this node ever since it was born. */
	enum ccm_state 	ccm_node_state;	/* cluster state of this node  */
	uint32_t	ccm_transition_minor;/* minor transition number of the  */
					/* cluster */

	ccm_update_t   ccm_update; 	/* structure that keeps track */
					/* of uptime of each member */
	ccm_version_t  ccm_version;     /* keeps track of version request  */
					/* related info */
	ccm_tmout_t	tmout;
	uint32_t change_event_remaining_count; 		
	enum change_event_type change_type;
	char change_node_id[NODEIDSIZE];

} ccm_info_t;

/*
 * datastructure passed to the event loop.
 * This acts a handle, and should not be interpreted
 * by the event loop.
 */
typedef struct  ccm_s {
	ll_cluster_t    *hbfd;
	void    	*info;
} ccm_t;


void client_new_mbrship(ccm_info_t*, void*);
void ccm_reset(ccm_info_t *info);
#endif /*  _CLUSTER_MANAGER_H_ */
