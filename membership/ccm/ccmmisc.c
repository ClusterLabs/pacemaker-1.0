/* $Id: ccmmisc.c,v 1.22 2005/10/04 09:23:38 horms Exp $ */
/* 
 * ccmmisc.c: Miscellaneous Consensus Cluster Service functions
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
 */
#include <portability.h>
#include <ccm.h>
#include <stdlib.h>
#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif
#include "ccmmisc.h"

#if 1
int
ccm_bitmap2str(const unsigned char *bitmap, char* memlist, int size)
{
	int	num_member = 0;
	char*	p;
	int	i;
	
	if (bitmap == NULL ||
	    memlist == NULL ||
	    size <= 0){
		cl_log(LOG_ERR, "invalid arguments");
		return -1;
	}
	
	p =memlist;
	for ( i = 0 ; i < MAXNODE; i++ ) {
		if(bitmap_test(i, bitmap, MAXNODE)){
			num_member++;
			p += sprintf(p, "%d ", i);
		}
	}
	
	return  strnlen(memlist, size);
}


int
ccm_str2bitmap(const unsigned char *_memlist, int size, unsigned char *bitmap)
{
	char	memlist[MAX_MEMLIST_STRING];
	char*	p;
	int	num_members = 0;
	
	if (memlist == NULL
	    || size <= 0
	    || size >= MAX_MEMLIST_STRING
	    || bitmap == NULL){
		cl_log(LOG_ERR, "invalid arguments");
		return -1;
	}
	
	memset(memlist, 0, MAX_MEMLIST_STRING);
	memcpy(memlist, _memlist, size);

	p = strtok(memlist, " ");
	while ( p != NULL){
		int i = atoi(p);
		bitmap_mark(i, bitmap, MAXNODE);		
		num_members ++;
		p = strtok(NULL, " ");
	}
	
	return num_members;
	
}	
#else

int
ccm_str2bitmap(const char *memlist, unsigned char **bitlist)
{
	size_t str_len =  strlen(memlist);
	int    outbytes = B64_maxbytelen(str_len);

	if (str_len == 0) {
	   	return bitmap_create(bitlist, MAXNODE);
	}

	while ((*bitlist = (unsigned  char *)g_malloc(outbytes)) == NULL) {
		cl_shortsleep();
	}
	memset(*bitlist,0,outbytes);

	outbytes = base64_to_binary(memlist, str_len, *bitlist, outbytes);

	return outbytes;
}

int
ccm_bitmap2str(const unsigned char *bitmap, int numBytes, char **memlist)
{
	int maxstrsize;

	maxstrsize = B64_stringlen(numBytes)+1;
	/* we want memory and we want it now */
	while ((*memlist = (char *)g_malloc(maxstrsize)) == NULL) {
		cl_shortsleep();
	}

	return binary_to_base64(bitmap, numBytes, *memlist, maxstrsize);
}


#endif

							
/* */
/* BEGIN OF FUNCTIONS THAT FACILITATE A MONOTONICALLY INCREASING */
/* LOCAL CLOCK. Useful for timeout manipulations. */
/* */
/* NOTE: gettimeofday() is generally helpful, but has the disadvantage */
/* of resetting to a earlier value(in case system administrator resets */
/* the clock) */
/* Similarly times() is a monotonically increasing clock, but has the */
/* disadvantage a of wrapping back on overflow. */
/* */
/* */

/* */
/* return the current time  */
/*  */
longclock_t 
ccm_get_time(void)
{
	return time_longclock();
}


/* */
/* given two times, and a timeout interval(in milliseconds),  */
/* return true if the timeout has occured, else return */
/* false. */
/* NOTE: 'timeout' is in milliseconds. */
int
ccm_timeout(longclock_t t1, longclock_t t2, unsigned long timeout)
{
	longclock_t t1cl;

	t1cl = add_longclock(t1 , msto_longclock(timeout));

	if(cmp_longclock(t1cl, t2) < 0) {
		return TRUE;
	}
	return FALSE;
}

void
ccm_check_memoryleak(void)
{
#ifdef HAVE_MALLINFO
	/* check for memory leaks */
	struct mallinfo i;
	static int arena=0;
	i = mallinfo();
	if(arena==0) {
		arena = i.arena;
	} else if(arena < i.arena) {
		cl_log(LOG_WARNING, 
			"leaking memory? previous arena=%d "
			"present arena=%d", 
			arena, i.arena);
		arena=i.arena;
	}
#endif
}


/* BEGINE of the functions that track asynchronous leave 
 * 
 * When ccm running on a  node leaves the cluster voluntarily it  
 * sends  a  leave  message  to  the  other nodes in the cluster.
 * Similarly  whenever  ccm  running on some node of the cluster, 
 * dies  the  local  heartbeat   delivers a leave message to ccm. 
 * And  whenever  some node in the cluster dies, local heartbeat  
 * informs  the  death  through  a  callback.  
 * In all these cases, ccm is informed about the loss of the node, 
 * asynchronously, in  some context where immidiate processing of  
 * the message is not possible.  
 * The  following  set of routines act as a cache that keep track  
 * of  message  leaves  and  facilitates  the  delivery  of these  
 * messages at a convinient time. 
 * 
 */

static unsigned char *leave_bitmap=NULL;

void
leave_init(void)
{
	int numBytes;
	
	assert(!leave_bitmap);
	numBytes = bitmap_create(&leave_bitmap, MAXNODE);
	memset(leave_bitmap, 0, numBytes);
}

void
leave_reset(void)
{
	int numBytes = bitmap_size(MAXNODE);
	if(!leave_bitmap) {
		return;
	}
	memset(leave_bitmap, 0, numBytes);
	return;
}

void
leave_cache(int i)
{
	assert(leave_bitmap);
	bitmap_mark(i, leave_bitmap, MAXNODE);
}

int
leave_get_next(void)
{
	int i;

	assert(leave_bitmap);
	for ( i = 0 ; i < MAXNODE; i++ ) {
		if(bitmap_test(i,leave_bitmap,MAXNODE)) {
			bitmap_clear(i,leave_bitmap,MAXNODE);
			return i;
		}
	}
	return -1;
}

int
leave_any(void)
{
	if(bitmap_count(leave_bitmap,MAXNODE)){
		return TRUE;
	}
	return FALSE;
}



/* */
/* BEGIN  OF  FUNCTIONS  that  keep track of stablized membership list */
/*  */
/* These  function  keep track of consensus membership once a instance */
/* of the  ccm algorithm terminates and decided on the final consensus  */
/* members of the cluster. */
/* */
int 
ccm_memlist_changed(ccm_info_t *info, 
		  char *bitmap /* the bitmap string containing bits */)
{
	int nodeCount, i;
	llm_info_t *llm;
	uint indx;
		
		
	/* go through the membership list */
	nodeCount = CCM_GET_MEMCOUNT(info);
	llm = CCM_GET_LLM(info);
	for ( i = 0 ; i < nodeCount; i++ ) {
		indx = CCM_GET_MEMINDEX(info, i);
		assert(indx >=0 && indx < LLM_GET_NODECOUNT(llm));
		if (!bitmap_test(indx, (unsigned char *)bitmap, MAXNODE)){
			return TRUE;
		}
	}
	return FALSE;
} 

int 
ccm_fill_memlist(ccm_info_t *info, 
	const unsigned char *bitmap)
{
	llm_info_t *llm;
	uint i;

	llm = CCM_GET_LLM(info);
	CCM_RESET_MEMBERSHIP(info);
	for ( i = 0 ; i < LLM_GET_NODECOUNT(llm); i++ ) {
		if(bitmap_test(i, bitmap, MAXNODE)){
			/*update the membership list with this member*/
			CCM_ADD_MEMBERSHIP(info, i);
		}
	}
	
	return HA_OK;
}

int 
ccm_fill_memlist_from_str(ccm_info_t *info, 
			  const char *memlist)
{
	unsigned char *bitmap = NULL;
	int ret;
	
	bitmap_create(&bitmap, MAXNODE);
	if (bitmap == NULL){
		cl_log(LOG_ERR, "bitmap creation failure");
		return HA_FAIL;
	}
	if (ccm_str2bitmap((const unsigned char *) memlist, strlen(memlist), 
			   bitmap) < 0){
		return HA_FAIL;
	}
	
	ret = ccm_fill_memlist(info, bitmap);
	bitmap_delete(bitmap);
	return ret;
}

									
int 
ccm_fill_memlist_from_bitmap(ccm_info_t *info, 
	const unsigned char *bitmap)
{
	return ccm_fill_memlist(info, bitmap);
}


int
ccm_get_membership_index(ccm_info_t *info, const char *node)
{
	int i,indx;
	llm_info_t *llm = CCM_GET_LLM(info);
	for ( i = 0 ; i < CCM_GET_MEMCOUNT(info) ; i++ ) {
		indx =  CCM_GET_MEMINDEX(info, i);
		if(strncmp(LLM_GET_NODEID(llm, indx), node, 
			   LLM_GET_NODEIDSIZE(llm)) == 0){
			return i;
		}
	}
	return -1;
}

gboolean
node_is_member(ccm_info_t* info, const char* node)
{
	int i,indx;
	llm_info_t *llm = CCM_GET_LLM(info);
	for ( i = 0 ; i < CCM_GET_MEMCOUNT(info) ; i++ ) {
		indx =  CCM_GET_MEMINDEX(info, i);
		if(strncmp(LLM_GET_NODEID(llm, indx), node, 
			   LLM_GET_NODEIDSIZE(llm)) == 0){
			return TRUE;
		}
	}	
	
	return FALSE;
}



gboolean 
part_of_cluster(int state)
{
	if (state >= CCM_STATE_END 
	    || state < 0){
		cl_log(LOG_ERR, "wrong state(%d)", state);
		return FALSE;
	}
	
	if (state == CCM_STATE_VERSION_REQUEST
	    || state == CCM_STATE_NONE){
		return FALSE;
	}
	
	return TRUE;
	
	
}


/* the ccm strings tokens communicated aross the wire. */
/* these are the values for the F_TYPE names. */
#define TYPESTRSIZE 32
char  ccm_type_str[CCM_TYPE_LAST + 1][TYPESTRSIZE] = {
	"CCM_TYPE_PROTOVERSION",
	"CCM_TYPE_PROTOVERSION_RESP",
	"CCM_TYPE_JOIN",
	"CCM_TYPE_REQ_MEMLIST",
	"CCM_TYPE_RES_MEMLIST",
	"CCM_TYPE_FINAL_MEMLIST",
	"CCM_TYPE_ABORT",
	"CCM_TYPE_LEAVE",
	"CCM_TYPE_TIMEOUT",
	"CCM_TYPE_NODE_LEAVE_NOTICE",
	"CCM_TYPE_NODE_LEAVE",
	"CCM_TYPE_MEM_LIST",
	"CCM_TYPE_ALIVE",
	"CCM_TYPE_NEW_NODE",
	"CCM_TYPE_STATE_INFO", 
	"CCM_TYPE_RESTART",
	"CCM_TYPE_LAST"
};


int
ccm_string2type(const char *type)
{
	enum ccm_type i;
	
	for ( i = CCM_TYPE_PROTOVERSION; i <= CCM_TYPE_LAST; i++ ) {
		if (strncmp(ccm_type_str[i], type, TYPESTRSIZE) == 0){
			return i;
		}
	}
	
	/* this message is not any type of ccm state messages
	 * but some other message from heartbeat
	 */
	
	return -1;
}

char *
ccm_type2string(enum ccm_type type)
{
        return ccm_type_str[type];
}

