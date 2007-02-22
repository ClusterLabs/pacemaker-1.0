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
#include <lha_internal.h>
#include <ccm.h>
#include <stdlib.h>
#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif
#include "ccmmisc.h"

#if 0
int
ccm_bitmap2str(const char *bitmap, char* memlist, int size)
{
	int	num_member = 0;
	char*	p;
	int	i;
	
	if (bitmap == NULL ||
	    memlist == NULL ||
	    size <= 0){
		ccm_log(LOG_ERR, "invalid arguments");
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
ccm_str2bitmap(const char *_memlist, int size, char *bitmap)
{
	char	memlist[MAX_MEMLIST_STRING];
	char*	p;
	int	num_members = 0;
	
	if (memlist == NULL
	    || size <= 0
	    || size >= MAX_MEMLIST_STRING
	    || bitmap == NULL){
		ccm_log(LOG_ERR, "invalid arguments");
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
ccm_str2bitmap(const char *memlist, int size, char *bitmap)
{
	int    outbytes = B64_maxbytelen(size);
	
	if (size == 0) {
		return 0;
	}
	
	outbytes = base64_to_binary(memlist, size, bitmap, outbytes);

	return outbytes;
}

int
ccm_bitmap2str(const char *bitmap, char *memlist, int size)
{
	int maxstrsize;
	int bytes;
	
	bytes = MAXNODE / BitsInByte;
	if (MAXNODE%BitsInByte != 0){
		bytes++;
	}
	maxstrsize = B64_stringlen(bytes)+1;

	if (maxstrsize > MAX_MEMLIST_STRING){
		ccm_log(LOG_ERR, "MAX_MEMLIST_STRING is too small(%d), sized required %d",
		       MAX_MEMLIST_STRING, maxstrsize);
		return -1;
	}
	return binary_to_base64(bitmap, bytes, memlist, size);
}


#endif

							
longclock_t 
ccm_get_time(void)
{
	return time_longclock();
}


/* 
* given two times, and a timeout interval(in milliseconds),  
* return true if the timeout has occured, else return 
* false. 
* NOTE: 'timeout' is in milliseconds. 
*/
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
	static int	count = 0;

	++count;
	/* Mallinfo is surprisingly expensive */
	if (count >= 60) {
		count = 0;
		i = mallinfo();
		if(arena==0) {
			arena = i.arena;
		} else if(arena < i.arena) {
			ccm_debug(LOG_WARNING, 
				"leaking memory? previous arena=%d "
				"present arena=%d", 
				arena, i.arena);
			arena=i.arena;
		}
	}
#endif
}


/* 
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

static  char *leave_bitmap=NULL;

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



gboolean 
part_of_cluster(int state)
{
	if (state >= CCM_STATE_END 
	    || state < 0){
		ccm_log(LOG_ERR, "part_of_cluster:wrong state(%d)", state);
		return FALSE;
	}
	
	if (state == CCM_STATE_VERSION_REQUEST
	    || state == CCM_STATE_NONE){
		return FALSE;
	}
	
	return TRUE;
	
	
}


/* the ccm strings tokens communicated aross the wire. 
 * these are the values for the F_TYPE names. 
 */

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
	int i;
	
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

