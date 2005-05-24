/* $Id: ccmllm.c,v 1.17 2005/05/24 18:39:33 gshi Exp $ */
/* 
 * ccmllm.c: Low Level membership routines.
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
#include <ccm.h>


/* */
/* return the number of nodes in the cluster that are in a active state. */
/* */
int
llm_get_live_nodecount(llm_info_t *llm)
{
	uint count=0, i;
	for ( i = 0 ; i < LLM_GET_NODECOUNT(llm) ; i++ ) {
		const char* status = LLM_GET_STATUS(llm,i);
		if (STRNCMP_CONST(status, DEADSTATUS) != 0
		    && STRNCMP_CONST(status, CLUST_INACTIVE) != 0) {
			    count++;
		}
	}
	return count;
}


/* */
/* return the nodename of the node with the specified uuid. */
/* */
char *
llm_get_nodeid_from_uuid(llm_info_t *llm, const int uuid)
{
	return LLM_GET_NODEID(llm, uuid);
}

/* */
/* return >0 if name of the node with indx1 is lexically */
/* higher than the name of the node with indx2. */
/* return 0 if the node names of both the nodes are the */
/* same */
/* return <0 if name of the node with indx1 is lexically */
/* lower than the name of the node with indx2. */
/* */
int
llm_nodeid_cmp(llm_info_t *llm, int indx1, int indx2)
{
	return strncmp(LLM_GET_NODEID(llm, indx1),
			LLM_GET_NODEID(llm, indx2), NODEIDSIZE);
}



void
display_llm(llm_info_t *llm)
{
	int i;
	cl_log(LOG_INFO, "total node number is %d", LLM_GET_NODECOUNT(llm));
	for (i = 0 ;i < LLM_GET_NODECOUNT(llm) ; i++){
		cl_log(LOG_INFO, "node %d =%s, status=%s", 
		       i,    llm->nodes[i].nodename, llm->nodes[i].status);		
	}
	
}

/* */
/* Get the index of the node with the given nodename  */
/* */
int
llm_get_index(llm_info_t *llm, const char *node)
{
	int low,high,mid;
	int value;

	/* lets do a binary search */
	low = 0;
	high = LLM_GET_NODECOUNT(llm)-1;
	do {
		mid = (low+high+1)/2;
		value = strncmp(LLM_GET_NODEID(llm, mid), node, NODEIDSIZE);
		if(value==0) {
			return mid;
		}

		if(high == low) {
			break;
		}
		
		if(value > 0) {
			high=mid-1;
		}
		else {
			low=mid+1;
		}
	} while(high>=low);

	display_llm(llm);
	return -1;
}

/* */
/* Update the status of node 'nodename'. */
/* return TRUE if the node transitioned to DEADSTATUS or CLUST_INACTIVE */
/* */
/* NOTE: CLUST_INACTIVE carries more information then DEADSTATUS */
/* DEADSTATUS just means the node is assumed to be dead, probably because */
/* of loss of connectivity or because of real death. */
/* BUT CLUST_INACTIVE confirms that the node is really cluster inactive. */
/* */
int
llm_status_update(llm_info_t *llm, const char *node, const char *status)
{
	int i;

	i = llm_get_index(llm, node);
	if(i == -1){
		return FALSE;
	}

	/* if there is no status change for this node just return */
	/*  FALSE 						  */
	if(strncmp(LLM_GET_STATUS(llm,i), status, 
			STATUSSIZE) == 0) {
		return FALSE;
	}

	LLM_SET_STATUS(llm,i,status);
	if ((STRNCMP_CONST(status, DEADSTATUS) == 0) || 
		STRNCMP_CONST(status, CLUST_INACTIVE) == 0) {
		return TRUE;
	}
	return FALSE;
}


int
llm_get_inactive_node_count(llm_info_t *llm)
{
	int		count = 0 ;
	unsigned	i;
	
	cl_log(LOG_INFO, "Counting nodes(dead nodes are not shown):");
	for (i = 0; i< llm->nodecount; i++){
		if (STRNCMP_CONST(llm->nodes[i].status, DEADSTATUS) != 0){
			cl_log(LOG_INFO, "node=%s  status=%s",
			       llm->nodes[i].nodename, 
			       llm->nodes[i].status);
		}
		if (STRNCMP_CONST(llm->nodes[i].status, 
			    CLUST_INACTIVE) == 0){
			count ++;
		}
	}
	
	return count;
}

/* */
/* Get uuid of the node with given nodename. */
/* */
int 
llm_get_uuid(llm_info_t *llm, const char *orig)
{
	int i;
	i = llm_get_index(llm, orig);
	if( i == -1 ) {
		return i;
	}

	return LLM_GET_UUID(llm,i);
}

/* */
/* return true if the node 'node' is a member of the */
/* low level membership. */
/* */
int
llm_is_valid_node(llm_info_t *llm, 
	const char *node)
{
	if(llm_get_index(llm, node) == -1 ) {
		return FALSE;
	}
	return TRUE;
}

/* */
/* set the context to fill in the low membership information. */
/* */
void 
llm_init(llm_info_t *llm)
{
	LLM_SET_NODECOUNT(llm,0);
	LLM_SET_MYNODE(llm,-1);
	return;
}

/* */
/* done filling in the low level membership.  */
/* */
void 
llm_end(llm_info_t *llm)
{
	assert(LLM_GET_NODECOUNT(llm) > 0);
	assert(LLM_GET_MYNODE(llm) != -1);
	return;
}

/* */
/* add a node to the low level membership with its */
/* coresspoding attributes. */
/* */
void
llm_add(llm_info_t *llm, 
	const char *node, 
	const char *status, 
	const char *mynode)
{
	int nodecount, mynode_idx, i, j;
	int value;


	/* Since this function is called only once, don't bother to
	 * program a great insert algorithm. Something that works
	 * correctly is good enough 
	 */
	nodecount = LLM_GET_NODECOUNT(llm);
	assert(nodecount < MAXNODE && nodecount >= 0);

	if (nodecount == 0) {
		mynode_idx = -1;
	} else {
		mynode_idx = LLM_GET_MYNODE(llm);
	}
	/* locate the position of the node */
	for ( i = 0 ; i < nodecount ; i++ ) {
		value = strncmp(LLM_GET_NODEID(llm, i), node, NODEIDSIZE);
		assert(value!=0);
		if(value > 0) {
			break;
		}
	}

	for ( j = nodecount; j>i; j-- ) {
		LLM_COPY(llm, j, j-1);
		LLM_SET_UUID(llm, j, j);
	}
		
	llm->nodes[i].join_request = FALSE;
	LLM_SET_NODEID(llm, i, node);
	LLM_SET_STATUS(llm, i, status);
	LLM_SET_UUID(llm, i, i);
	LLM_INC_NODECOUNT(llm);
	if (strncmp(mynode, node, NODEIDSIZE) == 0) {
		LLM_SET_MYNODE(llm, i);
	} else if (mynode_idx != -1 && i <= mynode_idx) {
		LLM_SET_MYNODE(llm, mynode_idx+1);
	}
	return;
}
