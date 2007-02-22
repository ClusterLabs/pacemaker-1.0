/*
 * ccmlib.h: internal definations for ccm library files.
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __CCMLIB_H_
#define __CCMLIB_H_
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <ipc.h>
#include <clplumbing/realtime.h>
#include <sys/time.h>

#ifdef __CCM_LIBRARY__
#include <ocf/oc_event.h>
void oc_ev_special(const oc_ev_t *, oc_ev_class_t , int );
#endif

#define NODEIDSIZE 255 /* if this value is changed change it 
			  	also in ccm.h */
#define CCMFIFO HA_VARRUNDIR "/heartbeat/ccm/ccm" /* if this value
			is changed change it also in ccm.h */

size_t strnlen(const char *, size_t); /*TOBEDONE*/
typedef struct born_s {
		int index;
		int bornon;
} born_t;
/* to be include by the client side of ccm */
typedef struct ccm_meminfo_s {
	int 		ev;
	int		n;
	int		trans;
	int		quorum;
	born_t		member[0];
} ccm_meminfo_t;

/* bornon structure sent to the client */
typedef struct ccm_born_s {
	int 	   n;
	born_t	   born[0];
} ccm_born_t;

typedef struct ccm_llm_s { /* information about low level membership info */
	int	   ev;
	uint	   n; 		/* number of nodes in the cluster  */
	int	   mynode;	 /* index of mynode */
	struct  node_s {
		uint  Uuid;  /* a cluster unique id for the node */
		char Id[NODEIDSIZE];
	} node[0];
} ccm_llm_t;
#define CLLM_GET_MYNODE(cllm) cllm->mynode
#define CLLM_GET_NODECOUNT(cllm) cllm->n
#define CLLM_GET_UUID(cllm,i) cllm->node[i].Uuid
#define CLLM_GET_MYUUID(cllm) CLLM_GET_UUID(cllm, CLLM_GET_MYNODE(cllm))
#define CLLM_GET_NODEID(cllm,i) cllm->node[i].Id
#define CLLM_GET_MYNODEID(cllm) CLLM_GET_NODEID(cllm, CLLM_GET_MYNODE(cllm))
#define CLLM_SET_MYNODE(cllm,indx) cllm->mynode = indx
#define CLLM_SET_NODECOUNT(cllm, count) cllm->n = count
#define CLLM_SET_UUID(cllm,i, uuid) cllm->node[i].Uuid = uuid
#define CLLM_SET_MYUUID(cllm, uuid) CLLM_SET_UUID(cllm, CLLM_GET_MYNODE(cllm), uuid)
#define CLLM_SET_NODEID(cllm, i, name)  \
			(strncpy(cllm->node[i].Id,name,NODEIDSIZE))
#define CLLM_SET_MYNODEID(cllm, name) \
			CLLM_SET_NODEID(cllm, CLLM_GET_MYNODE(cllm), name)

#ifdef __CCM_LIBRARY__
typedef struct class_s {
	int	type;
	oc_ev_callback_t *(*set_callback)(struct class_s *, 
						oc_ev_callback_t(*));
	gboolean	 (*handle_event) (struct class_s *);
	int		 (*activate) (struct class_s *);
	void		 (*unregister) (struct class_s *);
	gboolean	 (*is_my_nodeid) (struct class_s *, const oc_node_t *);
	void	 	 (*special) (struct class_s *, int);
	void		  *private;
} class_t;
class_t *oc_ev_memb_class(oc_ev_callback_t  *);
#endif

#define CCM_EVICTED 		1
#define CCM_NEW_MEMBERSHIP 	2
#define CCM_INFLUX 		3
#define CCM_LLM 		4


#endif  /* __CCMLIB_H_ */
