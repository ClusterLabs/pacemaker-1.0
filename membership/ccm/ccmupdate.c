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
#include <lha_internal.h>
#include "ccm.h"
#include "ccmmisc.h"


/* 
 * add the node 'node' to the list of cluster leaders requesting 
 * for membership information. 
 */

void
update_add_memlist_request(ccm_update_t *tab, 
			llm_info_t *llm, 
			const char *node,
			const int uptime)
{
	int idx = llm_get_index(llm, node);
	update_t *obj;
	int i=0;
	
	while((obj = (update_t *)
	       g_slist_nth_data(UPDATE_GET_CLHEAD(tab),i++)) != NULL){
		if(idx == obj->index) {
			if(uptime > obj->uptime) {
				ccm_debug(LOG_WARNING
				       ,	"WARNING:update_add_memlist_request"
				       " %s already added(updating)", node);
				obj->uptime = uptime;
			}
			return;
		}
	}
	obj = g_malloc(sizeof(update_t));
	obj->index = idx;
	obj->uptime = uptime;
	UPDATE_SET_CLHEAD(tab, g_slist_append(UPDATE_GET_CLHEAD(tab), obj));
	return;
}

/* 
 * free all the members in the list. 
 */
void
update_free_memlist_request(ccm_update_t *tab) 
{
	uint i;
	update_t *obj;
	
	for (i = 0; i < g_slist_length(UPDATE_GET_CLHEAD(tab)); i++) {
		obj = (update_t *)g_slist_nth_data(UPDATE_GET_CLHEAD(tab),i);
		if(obj) {
			g_free(obj);
		}
		
	}
	g_slist_free(UPDATE_GET_CLHEAD(tab));
	UPDATE_SET_CLHEAD(tab, NULL);
}

/* 
 * set up the context to traverse the list of  
 * cluster leaders. 
 */
void *
update_initlink(ccm_update_t *tab)
{
	GSList **track  = (GSList **)g_malloc(sizeof(GSList *));
	*track  = UPDATE_GET_CLHEAD(tab);
	return (void *)track;
}


/* 
 * return name of the cluster leader in the next element in the list. 
 */
const char *
update_next_link(ccm_update_t *tab, llm_info_t *llm, void *tr, uint *uptime)
{
	update_t *node;
	GSList **track = (GSList **)tr;

	node = (update_t *)g_slist_nth_data((*track),0);
	if(node==NULL) {
		return NULL;
	}

	*uptime = node->uptime;
	*track = g_slist_next(*track);
	return (llm_get_nodename(llm, node->index));
}

/* 
 * free the context used for cluster leader link traversal. 
 */
void
update_freelink(ccm_update_t *tab, void *track)
{
	g_free(track);
	return;
}


/*
 * clear all the information that we are tracking. 
 */
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
	UPDATE_SET_INITTIME(tab, ccm_get_time());
	update_free_memlist_request(tab);
}

/* 
 * initialize our datastructures. 
 */

void
update_init(ccm_update_t *tab)
{
	UPDATE_SET_CLHEAD(tab, NULL);
	update_reset(tab);
}

/* 
 * return TRUE if sufficient time has expired since update messages 
 * were exchanged. 
 */
int
update_timeout_expired(ccm_update_t *tab, unsigned long timeout)
{
	return(ccm_timeout(UPDATE_GET_INITTIME(tab), ccm_get_time(), timeout));
}


/*
 * given two members return the leader. 
 */
static uint
update_compute_leader(ccm_update_t *tab, uint j, llm_info_t *llm)
{
	update_t *entry;
	update_t *leader_entry;
	int value;
	
	int leader = tab->leader;

	if(leader == -1) {
		return j;
	}
	
	entry = &(tab->update[j]);	
	leader_entry = &(tab->update[leader]);
	
	
	if (leader_entry->uptime == entry->uptime){
		goto namecompare;
	}

	if (leader_entry->uptime ==0){
		return j;
	}
	if (entry->uptime == 0){
		return leader;
	}
	
	if (leader_entry->uptime < entry->uptime) {
		return leader;
	}

	if (leader_entry->uptime > entry->uptime) {
		return j;
	}

 namecompare:
	value =  llm_node_cmp(llm, leader_entry->index, 
				entry->index);

	if (value == 0){
		ccm_log(LOG_ERR, "update_compute_leader:same id comparsion?");
		abort();
	}
	
	if (value > 0) {
		return leader;
	}

	return j;
}

void
update_display(int pri,llm_info_t* llm, ccm_update_t* tab)
{
	int i; 
	
	ccm_debug(pri, "diplaying update information: ");
	ccm_debug(pri, "leader=%d(%s) nodeCount=%d", 
	       tab -> leader,
	       (tab->leader<0 || tab->leader >= (int)llm_get_nodecount(llm))?
	       "":llm_get_nodename(llm, tab->update[tab->leader].index),
	       tab->nodeCount);
	
 	for ( i = 0; i < llm_get_nodecount(llm); i++){
		if (tab->update[i].index >=0){
			ccm_debug(pri, "%d:%s uptime=%d", 
			       i,
			       llm_get_nodename(llm, tab->update[i].index),
			       tab->update[i].uptime);		
		}
	}
}

/* 
 * given the current members, choose the leader. 
 * set the leader and return the leader as well
 *
 */
static int
update_find_leader(ccm_update_t *tab, llm_info_t *llm) 
{
	int i, leader, j;

	for ( i = 0 ; i < llm_get_nodecount(llm); i++ ){
		if (UPDATE_GET_INDEX(tab, i) != -1) {
			break;
		}
	}

	if (i == llm_get_nodecount(llm)){
		UPDATE_SET_LEADER(tab,-1);
		return -1;
	}

	leader = i;
        UPDATE_SET_LEADER(tab,leader);

	for ( j = i+1 ; j < llm_get_nodecount(llm); j++ ){

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


/* return the index of the node in the update table. */
static int
update_get_position(ccm_update_t *tab,
		 llm_info_t *llm,
		 const char *nodename)
{
	int i;
	uint j;
	
	i = llm_get_index(llm, nodename);
	if ( i == -1 ){
		return -1;
	}
	
	/* search for the index in the update table */
	for ( j = 0 ; j < llm_get_nodecount(llm); j++ ){
		if (UPDATE_GET_INDEX(tab,j) == i ){
			break;
		}
	}

	if ( j == llm_get_nodecount(llm)){
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

	for ( j = 0 ; j < llm_get_nodecount(llm); j++ ){
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

/* 
 * return TRUE if the node had participated in the update voting round. 
 *
 */
int
update_is_node_updated(ccm_update_t *tab,
		       llm_info_t *llm,
		       const char *node)
{
	if(update_get_position(tab, llm, node) == -1 ){
		return FALSE;
	}
	return TRUE;
}


/* 
 * Update the vote of the node in the update table. 
 */
void
update_add(ccm_update_t *tab, 
	   llm_info_t *llm, 
	   const char *nodename,
	   int  uptime,
	   gboolean leader_flag)
{
	int i;
	uint j;
	
	i = llm_get_index(llm, nodename);
	
	if( i == -1 ) {
		ccm_log(LOG_ERR, "ccm_update_table:Internal Logic error i=%d",
				i);
		exit(1);
	}

	/* find a free location in the 'table' table to fill the new
	 * entry. A free entry should be found within llm_get_nodecount
	 * entries.
	 */
	for ( j = 0 ; j < llm_get_nodecount(llm); j++ ){
		if (UPDATE_GET_INDEX(tab,j) == -1 ){
			break;
		}
		
		if(i == UPDATE_GET_INDEX(tab,j)){
			ccm_log(LOG_ERR, "ccm_update_table:duplicate entry %s",
			       nodename);
			return;
		}
	}
	
	if( j == llm_get_nodecount(llm) ) {
		ccm_log(LOG_ERR, "ccm_update_table:Internal Logic error j=%d",
		       j);
		exit(1);
	}
	
	UPDATE_SET_INDEX(tab,j,i);
	UPDATE_SET_UPTIME(tab,j,uptime);
	UPDATE_INCR_NODECOUNT(tab);

	if(leader_flag) {
		UPDATE_SET_LEADER(tab, update_compute_leader(tab, j, llm));
	}
	return;
}



/* 
 * remove the vote of a node from the update table. 
 */
void
update_remove(ccm_update_t *tab, 
	      llm_info_t *llm, 
		const char *nodename)
{
	int j, idx;
	update_t *obj;
	int i=0;

	j = update_get_position(tab, llm, nodename);
	if( j == -1 ) {
		return;
	}
	
	UPDATE_SET_UPTIME(tab, j, 0);
	UPDATE_SET_INDEX(tab, j, -1);
	UPDATE_DECR_NODECOUNT(tab);
	/* remove any request cached in our queue from this node */
	idx =  llm_get_index(llm, nodename);
	while((obj = (update_t *)g_slist_nth_data(tab->cl_head,i)) 
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



/* 
 * return TRUE if I am the leader among the members that have 
 * voted in this round of update exchanges. 
 */
int
update_am_i_leader(ccm_update_t *tab, 
		llm_info_t *llm)
{
	int leader = UPDATE_GET_LEADER(tab);
	if (llm_get_myindex(llm) == UPDATE_GET_INDEX(tab,leader)) {
		return TRUE;
	}
	return FALSE;
}


/*
 * return the name of the cluster leader. 
 */
const char *
update_get_cl_name(ccm_update_t *tab,
		   llm_info_t *llm)
{
	int leader = UPDATE_GET_LEADER(tab);
	return(llm_get_nodename(llm,UPDATE_GET_INDEX(tab,leader)));
}




/*
 * return the uuid of the next member who has voted in the update
 * message transfer round. 
 */
int
update_get_next_index(ccm_update_t *tab, llm_info_t *llm, int *nextposition)
{
	uint pos;
	
	if (*nextposition < -1 || *nextposition >= (int)llm_get_nodecount(llm)) {
			return -1;
	}
	
	pos = (*nextposition == -1 ? 0 : *nextposition);
	*nextposition = pos + 1;
	
	while (UPDATE_GET_INDEX(tab,pos) == -1 && 
	       pos < llm_get_nodecount(llm)){ 
		pos++;
	}
	if (pos == llm_get_nodecount(llm)) {
		return -1;
	}
	
	return UPDATE_GET_INDEX(tab, pos);
}

/* 
 * create a string that represents the members of the update 
 * round, and return it through the memlist parameter. 
 * also return the size of the string. 
 */
int 
update_strcreate(ccm_update_t *tab,
		 char *memlist,
		 llm_info_t *llm)
{
	uint i;
	int	indx;
	char *bitmap;
	int str_len;

	bitmap_create(&bitmap, MAXNODE);

	for ( i = 0 ; i < llm_get_nodecount(llm); i ++ ) {
		indx = UPDATE_GET_INDEX(tab,i);
		if (indx == -1){
			continue;
		}

		bitmap_mark(indx, bitmap, MAXNODE);
	}

	str_len = ccm_bitmap2str(bitmap, memlist, MAX_MEMLIST_STRING);
	bitmap_delete(bitmap);
	return str_len;
}
