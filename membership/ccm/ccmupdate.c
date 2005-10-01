/* $Id: ccmupdate.c,v 1.16 2005/10/01 02:01:56 gshi Exp $ */
/* 
 * update.c: functions that track the votes during the voting protocol
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
#include "ccmmisc.h"

/* generic leader info */
typedef struct leader_info_s {
	int		index;
	int		trans;
} leader_info_t;

/* */
/* BEGIN of Functions that keeps track of the memlist request messages from */
/* cluster leaders. */
/* */

/* */
/* add the node 'node' to the list of cluster leaders requesting */
/* for membership information. */
/* */
void
update_add_memlist_request(ccm_update_t *tab, 
			llm_info_t *llm, 
			const char *node,
			const int trans)
{
	int idx = llm_get_index(llm, node);
	leader_info_t *obj;
	int i=0;

	while((obj = (leader_info_t *)
	       g_slist_nth_data(UPDATE_GET_CLHEAD(tab),i++)) != NULL){
		if(idx == obj->index) {
			if(trans > obj->trans) {
				cl_log(LOG_WARNING
				       ,	"WARNING:update_add_memlist_request"
				" %s already added(updating)", node);
				obj->trans = trans;
			}
			return;
		}
	}
	obj = g_malloc(sizeof(leader_info_t));
	obj->index = idx;
	obj->trans = trans;
	UPDATE_SET_CLHEAD(tab, g_slist_append(UPDATE_GET_CLHEAD(tab), obj));
	return;
}

/* */
/* free all the members in the list. */
/* */
void
update_free_memlist_request(ccm_update_t *tab) 
{
	uint i;
	leader_info_t *obj;

	for (i = 0; i < g_slist_length(UPDATE_GET_CLHEAD(tab)); i++) {
		obj = (leader_info_t *)g_slist_nth_data(
				UPDATE_GET_CLHEAD(tab),i);
		if(obj) {
			g_free(obj);
		}

	}
	g_slist_free(UPDATE_GET_CLHEAD(tab));
	UPDATE_SET_CLHEAD(tab, NULL);
}

/* */
/* set up the context to traverse the list of  */
/* cluster leaders. */
/* */
void *
update_initlink(ccm_update_t *tab)
{
	GSList **track  = (GSList **)g_malloc(sizeof(GSList *));
	*track  = UPDATE_GET_CLHEAD(tab);
	return (void *)track;
}


/* */
/* return name of the cluster leader in the next element in the list. */
/* */
char *
update_next_link(ccm_update_t *tab, llm_info_t *llm, void *tr, uint *trans)
{
	leader_info_t *node;
	GSList **track = (GSList **)tr;

	node = (leader_info_t *)g_slist_nth_data((*track),0);
	if(node==NULL) {
		return NULL;
	}

	*trans = node->trans;
	*track = g_slist_next(*track);
	return (LLM_GET_NODEID(llm, node->index));
}

/* */
/* free the context used for cluster leader link traversal. */
/* */
void
update_freelink(ccm_update_t *tab, void *track)
{
	g_free(track);
	return;
}
/* */
/* END of Functions that keeps track of the memlist request messages from */
/* cluster leaders. */
/* */


/* */
/* clear all the information that we are tracking. */
/* */
void
update_reset(ccm_update_t *tab)
{
	int i;

	UPDATE_SET_LEADER(tab, -1);
	UPDATE_SET_NODECOUNT(tab, 0);
	for ( i = 0 ; i < MAXNODE; i++ ) {
		UPDATE_SET_INDEX(tab, i, -1);
		UPDATE_SET_UPTIME(tab, i, -1);
	}
	/* also note down the time. this should help us in 
	 *  determining when to timeout
 	 */
	UPDATE_SET_INITTIME(tab, ccm_get_time());
	update_free_memlist_request(tab);
}

/* */
/* initialize our datastructures. */
/* */
void
update_init(ccm_update_t *tab)
{
	UPDATE_SET_CLHEAD(tab, NULL);
	update_reset(tab);
}

/* */
/* return TRUE if sufficient time has expired since update messages */
/* were exchanged. */
/* */
int
update_timeout_expired(ccm_update_t *tab, unsigned long timeout)
{
	return(ccm_timeout(UPDATE_GET_INITTIME(tab), ccm_get_time(), timeout));
}


/* */
/* return TRUE if we have received any update messages at all. */
/* */
int
update_any(ccm_update_t *tab)
{
	if (UPDATE_GET_LEADER(tab) == -1) {
		return FALSE;
	}

	return TRUE;
}

/* */
/* given two members return the leader. */
/* */
/* */
static uint
update_compute_leader(ccm_update_t *tab, uint j, llm_info_t *llm)
{
	update_t *entry1, *entry2;
	int value;

	int leader = tab->leader;

	if(leader == -1) {
		return j;
	}

	entry1 = &(tab->update[j]);

	entry2 = &(tab->update[leader]);


	if ((entry2->uptime == 0)  
	    &&  (entry1->uptime == 0)) {
		goto leader_str;
	}

	if (entry1->uptime == 0) {
		return leader;
	}

	if (entry2->uptime == 0) {
		return j;
	}

	if (entry2->uptime < entry1->uptime) {
		return leader;
	}

	if (entry2->uptime > entry1->uptime) {
		return j;
	}

leader_str :
	value =  llm_nodeid_cmp(llm, entry2->index, entry1->index);

	assert(value != 0);

	if (value < 0) {
		return leader;
	}

	return j;
}

void
update_display(int pri,llm_info_t* llm, ccm_update_t* tab)
{
	unsigned i; 
	
	cl_log(pri, "diplaying update information: ");
	cl_log(pri, "leader=%d(%s) nodeCount=%d", 
	       tab -> leader,
	       (tab->leader<0 || tab->leader >= (int)LLM_GET_NODECOUNT(llm))?"":LLM_GET_NODEID(llm, tab->update[tab->leader].index),
	       tab->nodeCount);
	
 	for ( i = 0; i < LLM_GET_NODECOUNT(llm); i++){
		if (tab->update[i].index >=0){
			cl_log(pri, "%d:%s uptime=%d", 
			       i,
			       LLM_GET_NODEID(llm, tab->update[i].index),
			       tab->update[i].uptime);		
		}
	}
}

/* */
/* given the current members, choose the leader. 
* set the leader and return the leader as well
*
 */
static int
update_find_leader(ccm_update_t *tab, llm_info_t *llm) 
{
	uint i, leader, j;

	for ( i = 0 ; i < LLM_GET_NODECOUNT(llm); i++ ){
		if (UPDATE_GET_INDEX(tab, i) != -1) {
			break;
		}
	}

	if (i == LLM_GET_NODECOUNT(llm)){
		UPDATE_SET_LEADER(tab,-1);
		return -1;
	}

	leader = i;
        UPDATE_SET_LEADER(tab,leader);

	for ( j = i+1 ; j < LLM_GET_NODECOUNT(llm); j++ ){

		if (UPDATE_GET_INDEX(tab, j) == -1){
			continue;
		}

		if(update_compute_leader(tab, j, llm) == j){
			UPDATE_SET_LEADER(tab,j);
			leader = j;
		}
	}

	return leader;
}


/* return the index of the 'orig' node in the update table. */
static int
update_get_index(ccm_update_t *tab,
		llm_info_t *llm,
		const char *orig)
{
	int i;
	uint j;

	i = llm_get_index(llm, orig);
	if ( i == -1 ){
		return -1;
	}

	/* search for the index in the update table */
	for ( j = 0 ; j < LLM_GET_NODECOUNT(llm); j++ ){
		if (UPDATE_GET_INDEX(tab,j) == i ){
			break;
		}
	}

	if ( j == LLM_GET_NODECOUNT(llm)){
		return -1;
	}

	return j;
}

int
update_get_uptime(ccm_update_t *tab,
		llm_info_t *llm,
		int idx)
{
	uint count=0, j;
	int i;

	for ( j = 0 ; j < LLM_GET_NODECOUNT(llm); j++ ){
		i = UPDATE_GET_INDEX(tab,j);
		if (i == -1){
			continue;
		}
		if (i == idx) {
			return UPDATE_GET_UPTIME(tab,j);
		}
		count++;
		if(count >= UPDATE_GET_NODECOUNT(tab)){
			return -1;
		}
	}
	return -1;
}

/* */
/* return TRUE if 'orig' had participated in the update voting round. */
/* */
int
update_is_member(ccm_update_t *tab,
		llm_info_t *llm,
		const char *orig)
{
	if(update_get_index(tab, llm, orig) == -1 ){
		return FALSE;
	}
	return TRUE;
}
	

/* */
/* Update the vote of 'orig' node in the update table. */
/* */
void
update_add(ccm_update_t *tab, 
		llm_info_t *llm, 
		const char *orig,
		int  uptime /*incidently its the transition number 
			      when the node
			     joined*/,
		gboolean leader_flag /* should the leader be recomputed? */)
{
	int i;
	uint j;

	/* find the location of the hostname in the llm table */
	i = llm_get_index(llm, orig);

	if( i == -1 ) {
		/* something wrong. Better printout a error and exit 
	 	* Lets catch this bug
	 	*/
		cl_log(LOG_ERR, "ccm_update_table:Internal Logic error i=%d",
				i);
		exit(1);
	}

	/* find a free location in the 'table' table to fill the new
	 * entry. A free entry should be found within LLM_GET_NODECOUNT
	 * entries.
	 */
	for ( j = 0 ; j < LLM_GET_NODECOUNT(llm); j++ ){
		if (UPDATE_GET_INDEX(tab,j) == -1 ){
			break;
		}

		/* check if this update is a duplicate update from the same node
		 * This should not happen. But never know! 
		*/
		if(i == UPDATE_GET_INDEX(tab,j)){
			cl_log(LOG_ERR, "ccm_update_table:duplicate entry %s",
						orig);
			return;
		}
	}

	if( j == LLM_GET_NODECOUNT(llm) ) {
		/* something wrong. Better printout a error and exit 
	 	* Lets catch this bug
	 	*/
		cl_log(LOG_ERR, "ccm_update_table:Internal Logic error j=%d",
				j);
		exit(1);
	}

	UPDATE_SET_INDEX(tab,j,i);
	UPDATE_SET_UPTIME(tab,j,uptime);
	/* increment the nodecount */
	UPDATE_INCR_NODECOUNT(tab);

	if(leader_flag) {
		UPDATE_SET_LEADER(tab, update_compute_leader(tab, j, llm));
	}
	return;
}



/* */
/* remove the vote of 'orig' from the update table. */
/* */
void
update_remove(ccm_update_t *tab, 
	      llm_info_t *llm, 
		const char *orig)
{
	int j, idx;
	leader_info_t *obj;
	int i=0;

	/* find this entry's location in the update table */
	j = update_get_index(tab, llm, orig);
	if( j == -1 ) {
		/* dont worry. Just return */
		return;
	}

	UPDATE_SET_UPTIME(tab, j, 0);
	UPDATE_SET_INDEX(tab, j, -1);
	UPDATE_DECR_NODECOUNT(tab);
	/* remove any request cached in our queue from this node */
	idx =  llm_get_index(llm, orig);
	while((obj = (leader_info_t *)g_slist_nth_data(tab->cl_head,i)) 
			!= NULL) {
		if(obj->index == idx){
			tab->cl_head = g_slist_remove(tab->cl_head, obj);
		} else {
			i++;
		}
	}

	/* recalculate the new leader if leader's entry is being removed*/
	if (UPDATE_GET_LEADER(tab) != j) {
		return;
	}

	UPDATE_SET_LEADER(tab,update_find_leader(tab, llm));
	return;
}



/* */
/* return TRUE if I am the leader among the members that have */
/* voted in this round of update exchanges. */
/* */
int
update_am_i_leader(ccm_update_t *tab, 
		llm_info_t *llm)
{
	int leader_slot = UPDATE_GET_LEADER(tab);
	if (LLM_GET_MYNODE(llm) == UPDATE_GET_INDEX(tab,leader_slot)) {
		return TRUE;
	}
	return FALSE;
}


/* */
/* return the name of the cluster leader. */
/* */
char *
update_get_cl_name(ccm_update_t *tab,
		llm_info_t *llm)
{
	int leader_slot = UPDATE_GET_LEADER(tab);
	return(LLM_GET_NODEID(llm,UPDATE_GET_INDEX(tab,leader_slot)));
}




/* */
/* return the uuid of the next member who has voted in the update */
/* message transfer round. */
/* */
int
update_get_next_uuid(ccm_update_t *tab, llm_info_t *llm, int *lastindex)
{
	uint indx;

	if (*lastindex < -1 || *lastindex >= (int)LLM_GET_NODECOUNT(llm)) {
			return -1;
	}

	indx = (*lastindex == -1 ? 0 : *lastindex);

	while (UPDATE_GET_INDEX(tab,indx) == -1 && 
	       indx < LLM_GET_NODECOUNT(llm)){ 
		indx++;
	}
	if (indx == LLM_GET_NODECOUNT(llm)) {
		return -1;
	}
	
	*lastindex = indx+1;
	
	return UPDATE_GET_INDEX(tab,indx);
}

/* */
/* create a string that represents the members of the update */
/* round, and return it through the memlist parameter. */
/* also return the size of the string. */
/* */
int 
update_strcreate(ccm_update_t *tab,
		 char *memlist,
		 llm_info_t *llm)
{
	uint i;
	int	indx;
	unsigned char *bitmap;
	int str_len;

	/* create a bitmap that can accomodate MAXNODE bits */
	bitmap_create(&bitmap, MAXNODE);


	/* for each node in the update list, find its uuid and
	 * correspondingly set its bit in the bitmap 
	 */
	for ( i = 0 ; i < LLM_GET_NODECOUNT(llm); i ++ ) {
		/* get the index of the node in the llm table */
		indx = UPDATE_GET_INDEX(tab,i);
		if (indx == -1){
			continue;
		}

		/* set this bit in the bitmap */
		bitmap_mark(indx, bitmap, MAXNODE);
	}

	str_len = ccm_bitmap2str(bitmap, memlist, MAX_MEMLIST_STRING);
	bitmap_delete(bitmap);
	return str_len;
}
