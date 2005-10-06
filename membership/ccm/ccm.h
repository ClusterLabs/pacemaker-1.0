/* $Id: ccm.h,v 1.47 2005/10/06 20:03:16 gshi Exp $ */
/*
 * ccm.h: definitions Consensus Cluster Manager internal header
 *				file
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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



/* */
/* ccm defined new type tokens used by the CCM protocol. */
/* */
#define CCM_VERSIONVAL  "ccmpverval" 	  /* version value token */
#define CCM_UPTIME      "ccmuptime"       /* Uptime for Consensus  */
#define CCM_MEMLIST     "ccmmemlist"      /* bitmap for membership */
#define CCM_PROTOCOL    "ccmproto"        /* protocol version */
#define CCM_MAJORTRANS  "ccmmajor"        /* major transition version*/
#define CCM_MINORTRANS  "ccmminor"        /* minor transition version */
#define CCM_MAXTRANS    "ccmmaxt"        /* minor transition version */
#define CCM_COOKIE      "ccmcookie"       /* communication context */
#define CCM_NEWCOOKIE   "ccmnewcookie"    /* new communication context */
#define CCM_CLSIZE   	"ccmclsize"       /* new cluster size */
#define CCM_UPTIMELIST "ccmuptimelist" /*uptime list*/


/* ccm_types for easier processing. */
enum ccm_type {
	CCM_TYPE_PROTOVERSION,
	CCM_TYPE_PROTOVERSION_RESP,
	CCM_TYPE_JOIN,
	CCM_TYPE_REQ_MEMLIST,
	CCM_TYPE_RES_MEMLIST,
	CCM_TYPE_FINAL_MEMLIST,
	CCM_TYPE_ABORT,
	CCM_TYPE_LEAVE,
	CCM_TYPE_TIMEOUT,
	CCM_TYPE_NODE_LEAVE_NOTICE,
	CCM_TYPE_NODE_LEAVE,
	CCM_TYPE_MEM_LIST,
	CCM_TYPE_ALIVE,
	CCM_TYPE_NEW_NODE,
	CCM_TYPE_STATE_INFO, 
	CCM_TYPE_RESTART,
	CCM_TYPE_LAST
};


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
#define CCMFIFO    HA_VARRUNDIR "/heartbeat/ccm/ccm" /* if this value is
			changed change it also in ccmlib.h */

typedef struct llm_node_s {
	char	nodename[NODEIDSIZE];
	char	status[STATUSSIZE];
	int	uptime;
	gboolean join_request;
	gboolean receive_change_msg;
}llm_node_t;

typedef struct llm_info_s { 
	int	   nodecount;
	int	   myindex;	
	llm_node_t nodes[MAXNODE];
} llm_info_t;

int		llm_get_live_nodecount(llm_info_t *);
int		llm_node_cmp(llm_info_t *llm, int indx1, int indx2);
char*		llm_get_nodename(llm_info_t *, const int );
int		llm_status_update(llm_info_t *, const char *, 
				  const char *, char*);
void		llm_display(llm_info_t *llm);
int		llm_init(llm_info_t *);
int		llm_is_valid_node(llm_info_t *, const char *);
int		llm_add(llm_info_t *, const char *, const char *, const char *);
int		llm_get_index(llm_info_t *, const char *);
int		llm_get_myindex(llm_info_t *);
int		llm_get_nodecount(llm_info_t* llm);
const char*	llm_get_mynodename(llm_info_t* llm);
char*		llm_get_nodestatus(llm_info_t* llm, const int index);
int		llm_set_joinrequest(llm_info_t* llm, int index, gboolean value);
gboolean	llm_get_joinrequest(llm_info_t* llm, int index);
int		llm_set_change(llm_info_t* llm, int index, gboolean value);
gboolean	llm_get_change(llm_info_t* llm, int index);
int		llm_set_uptime(llm_info_t* llm, int index, int uptime);
int		llm_get_uptime(llm_info_t* llm, int index);

/* ccm prototypes */
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
void update_add(ccm_update_t *, llm_info_t *, const char *, int, gboolean);
void update_remove(ccm_update_t *, llm_info_t *, const char *);
int update_am_i_leader(ccm_update_t *, llm_info_t *);
int update_can_be_leader(ccm_update_t *,  llm_info_t *llm, const char *, int );
char * update_get_cl_name(ccm_update_t *, llm_info_t *);
void * update_initlink(ccm_update_t *);
char * update_next_link(ccm_update_t *, llm_info_t *, void *, uint *);
void update_freelink(ccm_update_t *, void *);
int update_get_next_index(ccm_update_t *, llm_info_t *, int *);
int update_strcreate(ccm_update_t *tab, char *memlist,llm_info_t *llm);
int update_is_node_updated(ccm_update_t *, llm_info_t *, const char *);
int update_get_uptime(ccm_update_t *, llm_info_t *, int );
void	update_display(int pri,llm_info_t* llm, ccm_update_t* tab);
/* END OF update interfaces */



/* BEGINNING OF graph interfaces */

typedef struct vertex_s {
	char  *bitmap; /* bitmap sent by each node */
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
void graph_update_membership(graph_t *, int , char *);
int  graph_filled_all(graph_t *);
int graph_get_maxclique(graph_t *, char **);
void graph_add_to_membership(graph_t *, int, int);
/* END OF graph interfaces */


/* BEGINNING OF bitmap interfaces */
int bitmap_create(char **, int);
void bitmap_delete(char *);
void bitmap_mark(int, char *, int);
void bitmap_clear(int, char *, int);
int bitmap_test(int, const char *, int);
int bitmap_count(const char *, int);
void bitmap_print(char *, int, char *);
void bitmap_reset(char *, int);
int  bitmap_size(int);
int  bitmap_copy(char *, char *);
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
	CCM_STATE_SENT_MEMLISTREQ,	/* CL has sent a request for member list  */
					/* this state is applicable only on CL */
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

#define		CCM_SET_ACTIVEPROTO(info, val) \
					info->ccm_active_proto = val
#define		CCM_SET_MAJORTRANS(info, val) 	\
		{	\
			info->ccm_transition_major = val; \
			info->ccm_max_transition = \
				(info->ccm_max_transition < val ? \
				val: info->ccm_max_transition); \
		}
#define		CCM_SET_MINORTRANS(info, val) 	\
					info->ccm_transition_minor = val
#define		CCM_INIT_MAXTRANS(info) 	\
					info->ccm_max_transition = 0

/* 	NOTE the reason the increment for majortrans is done */
/* 	as below is to force recomputation of  ccm_max_transition  */
#define		CCM_INCREMENT_MAJORTRANS(info) 	\
				CCM_SET_MAJORTRANS(info, \
					CCM_GET_MAJORTRANS(info)+1)

#define		CCM_INCREMENT_MINORTRANS(info) 	\
					info->ccm_transition_minor++
#define		CCM_RESET_MAJORTRANS(info) 	\
					info->ccm_transition_major = 0
#define		CCM_RESET_MINORTRANS(info) 	\
					info->ccm_transition_minor = 0

#define 	CCM_SET_JOINED_TRANSITION(info, trans) \
					info->ccm_joined_transition = trans
#define 	CCM_SET_COOKIE(info, val) \
				strncpy(info->ccm_cookie, val, COOKIESIZE)
#define 	CCM_SET_CL(info, index)	info->ccm_cluster_leader = index


#define		CCM_GET_ACTIVEPROTO(info) info->ccm_active_proto
#define		CCM_GET_MAJORTRANS(info) info->ccm_transition_major
#define		CCM_GET_MINORTRANS(info) info->ccm_transition_minor
#define 	CCM_GET_MAXTRANS(info)   info->ccm_max_transition
#define		CCM_GET_STATE(info) 	info->state 
#define		CCM_GET_HIPROTO(info) 	info->ccm_hiproto 
#define 	CCM_GET_LLM(info) 	(&(info->llm))
#define 	CCM_GET_UPDATETABLE(info) (&(info->ccm_update))
#define 	CCM_GET_MEMCOMP(info) (&(info->ccm_memcomp))
#define 	CCM_GET_JOINED_TRANSITION(info) info->ccm_joined_transition
#define  	CCM_GET_LLM_NODECOUNT(info) llm_get_nodecount(&info->llm)
#define  	CCM_GET_MY_HOSTNAME(info)  ccm_get_my_hostname(info)
#define 	CCM_GET_COOKIE(info) info->ccm_cookie

#define 	CCM_GET_MEMINDEX(info, i)	info->ccm_member[i]
#define 	CCM_GET_MEMTABLE(info)		info->ccm_member
#define 	CCM_GET_CL(info)  		info->ccm_cluster_leader
#define		CCM_TRANS_EARLIER(trans1, trans2) (trans1 < trans2) /*TOBEDONE*/
#define 	CCM_GET_VERSION(info)	&(info->ccm_version)


#define 	CCM_TMOUT_SET_U(info,t) info->tmout.u=t
#define 	CCM_TMOUT_SET_LU(info,t) info->tmout.lu=t
#define 	CCM_TMOUT_SET_VRS(info,t) info->tmout.vrs=t
#define 	CCM_TMOUT_SET_ITF(info,t) info->tmout.itf=t
#define 	CCM_TMOUT_SET_IFF(info,t) info->tmout.iff=t
#define 	CCM_TMOUT_SET_FL(info,t) info->tmout.fl=t
#define 	CCM_TMOUT_GET_U(info) info->tmout.u
#define 	CCM_TMOUT_GET_LU(info) info->tmout.lu
#define 	CCM_TMOUT_GET_VRS(info) info->tmout.vrs
#define 	CCM_TMOUT_GET_ITF(info) info->tmout.itf
#define 	CCM_TMOUT_GET_IFF(info) info->tmout.iff
#define 	CCM_TMOUT_GET_FL(info) info->tmout.fl

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
	
	int		memcount;	/*  number of nodes in the ccm cluster */
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
	enum ccm_state 	state;	/* cluster state of this node  */
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
const char*	state2string(int state);
int ccm_control_process(ccm_info_t *info, ll_cluster_t * hb);
int	jump_to_joining_state(ll_cluster_t* hb, 
			      ccm_info_t* info, 
			      struct ha_msg* msg);
typedef void (*state_msg_handler_t)(enum ccm_type ccm_msg_type, 
				    struct ha_msg *reply, 
				    ll_cluster_t *hb, 
				    ccm_info_t *info);

#endif 
