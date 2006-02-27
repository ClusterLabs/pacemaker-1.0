/* $Id: ccmllm.c,v 1.30 2006/02/27 14:05:12 alan Exp $ */
/* 
 * ccmllm.c: Low Level membership routines.
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

#include "ccm.h"


int
llm_get_nodecount(llm_info_t* llm){
	
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return -1;
	}
	
	return llm->nodecount;
}

int
llm_get_live_nodecount(llm_info_t *llm)
{
	int	count = 0;
	int	i;
	
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return -1;
	}
	
	for ( i = 0 ; i < llm->nodecount; i++ ) {
		const char* status = llm->nodes[i].status;
		
		if (STRNCMP_CONST(status, DEADSTATUS) != 0){
			count++;
		}
	}
	
	return count;
}


char *
llm_get_nodename(llm_info_t *llm, const int index)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  NULL;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return NULL;
	}
	
	return llm->nodes[index].nodename;
	
}

char *
llm_get_nodestatus(llm_info_t* llm, const int index)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  NULL;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return NULL;
	}
	
	return llm->nodes[index].status;	
	
}

int
llm_node_cmp(llm_info_t *llm, int indx1, int indx2)
{
	return strncmp(llm_get_nodename(llm, indx1),
			llm_get_nodename(llm, indx2), NODEIDSIZE);
}



void
llm_display(llm_info_t *llm)
{
	unsigned int	i;
	ccm_debug2(LOG_DEBUG, "total node number is %d", llm->nodecount);
	for (i = 0 ;i < llm->nodecount; i++){
		ccm_debug2(LOG_DEBUG, "node %d =%s, status=%s", 
		       i,    llm->nodes[i].nodename, llm->nodes[i].status);		
	}
	
}

int 
llm_get_myindex(llm_info_t* llm)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "NULL pointer");
		return -1;
	}
	return llm->myindex;
}

const char*
llm_get_mynodename(llm_info_t* llm)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return NULL;
	}
	
	if (llm->myindex < 0){
		ccm_log(LOG_ERR, "%s: mynode is not set",
		       __FUNCTION__);
		return NULL;
	}
	
	return llm->nodes[llm->myindex].nodename;
	
}

int
llm_get_index(llm_info_t *llm, const char *node)
{
	int low,high,mid;
	int value;

	/*binary search */
	low = 0;
	high = llm->nodecount - 1;
	do {
		mid = (low+high+1)/2;
		value = strncmp(llm_get_nodename(llm, mid), node, NODEIDSIZE);
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

	return -1;
}

int
llm_status_update(llm_info_t *llm, const char *node, 
		  const char *status, char* oldstatus)
{
	int i;
	
	i = llm_get_index(llm, node);
	if(i == -1){
		return HA_FAIL;
	}
	
	if (oldstatus){
		strncpy(oldstatus, llm->nodes[i].status, STATUSSIZE);
	}
	
	strncpy(llm->nodes[i].status, status, STATUSSIZE);

	return HA_OK;
}


int
llm_is_valid_node(llm_info_t *llm, 
		  const char *node)
{
	if(llm_get_index(llm, node) == -1 ) {
		return FALSE;
	}
	return TRUE;
}

int
llm_init(llm_info_t *llm)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		
		return HA_FAIL;
	}
	
	llm->nodecount = 0;
	llm->myindex = -1;
	
	return HA_OK;
}


int 
llm_del(llm_info_t* llm,
	const char* node)
{
	int i;
	int j;

	for ( i = 0 ;i < llm->nodecount; i++){
		if (strncmp(llm->nodes[i].nodename, node, NODEIDSIZE)==0){
			break;
		}
	}
	
	if (i == llm->nodecount){
		ccm_log(LOG_ERR, "%s: Node %s not found in llm",
		       __FUNCTION__,
		       node);
		return HA_FAIL;
	}
	
	if (llm->myindex > i){
		llm->myindex --;
	}else if (llm->myindex ==i){
		ccm_log(LOG_ERR, "%s: deleing myself in ccm is not allowed",
		       __FUNCTION__);
		return HA_FAIL;
	}


	for ( j = i; j< llm->nodecount - 1; j++){
		strncpy(llm->nodes[j].nodename, llm->nodes[j+1].nodename, NODEIDSIZE);
		strncpy(llm->nodes[j].status, llm->nodes[j+1].status, STATUSSIZE);
		
	}
		
	llm->nodecount --;

	return HA_OK;
}



int
llm_add(llm_info_t *llm, 
	const char *node,
	const char *status, 
	const char *mynode)
{
	int	nodecount;
	int	i, j;

	nodecount = llm->nodecount;
	if (nodecount < 0 || nodecount > MAXNODE ){
		ccm_log(LOG_ERR, "nodecount out of range(%d)",
		       nodecount);
		return HA_FAIL;
	}
	


	for ( i = 0 ; i < nodecount ; i++ ) {
		int value = strncmp(llm_get_nodename(llm, i), 
				    node, NODEIDSIZE);
		if (value == 0){
			ccm_log(LOG_ERR, "%s: adding same node(%s) twice(?)",
			       __FUNCTION__, node);
			return HA_FAIL;
		}
		if (value > 0) {
			break;
		}
	}
	
	for ( j = nodecount; j > i; j-- ) {
		llm->nodes[j] = llm->nodes[j - 1];
	}
		
	llm->nodes[i].join_request = FALSE;
	strncpy(llm->nodes[i].nodename, node,NODEIDSIZE);
	strncpy(llm->nodes[i].status, status, STATUSSIZE);
	llm->nodecount++;
	
	if (llm->myindex >= i) {		
		llm->myindex++;		
	}
		
	if (llm->myindex < 0 
	    && strncmp(mynode, node, NODEIDSIZE) == 0) {
		llm->myindex = i;
	} 

	if (llm->myindex >= llm->nodecount){
		ccm_log(LOG_ERR, "%s: myindex(%d) out of range,"
		       "llm->nodecount =%d",
		       __FUNCTION__, llm->myindex, llm->nodecount);
		return HA_FAIL;
	}

	return HA_OK;
}

int
llm_set_joinrequest(llm_info_t* llm, int index, gboolean value)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  HA_FAIL;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return HA_FAIL;
	}	
	
	llm->nodes[index].join_request = value;
	
	return HA_OK;
}

gboolean
llm_get_joinrequest(llm_info_t* llm, int index)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  FALSE;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return FALSE;
	}	
	
	return llm->nodes[index].join_request;
	
}


int
llm_set_change(llm_info_t* llm, int index, gboolean value)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  HA_FAIL;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return HA_FAIL;
	}	
	
	llm->nodes[index].receive_change_msg = value;
	
	return HA_OK;
}

gboolean
llm_get_change(llm_info_t* llm, int index)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  FALSE;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return FALSE;
	}	
	
	return llm->nodes[index].receive_change_msg;	
}

int
llm_set_uptime(llm_info_t* llm, int index, int uptime)
{
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  FALSE;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return FALSE;
	}
	
	if (uptime < 0){
		ccm_log(LOG_ERR, "%s: Negative uptime %d",
		       __FUNCTION__, uptime);
		return FALSE;
		       
	}

	llm->nodes[index].uptime = uptime;
	
	return HA_OK;
}

int
llm_get_uptime(llm_info_t* llm, int index)
{
	
	if (llm == NULL){
		ccm_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return  -1;
	}
	
	if (index < 0 || index > MAXNODE){
		ccm_log(LOG_ERR, "%s: index(%d) out of range",
		       __FUNCTION__, index);
		return -1;
	}
	
	return llm->nodes[index].uptime;

	
}
