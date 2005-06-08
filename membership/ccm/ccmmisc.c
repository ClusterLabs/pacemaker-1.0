/* $Id: ccmmisc.c,v 1.18 2005/06/08 08:17:39 sunjd Exp $ */
/* 
 * ccmmisc.c: Miscellaneous Consensus Cluster Service functions
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
#include <portability.h>
#include <ccm.h>
#include <stdlib.h>
#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif

/* */
/* Convert a given string to a bitmap. */
/* */
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


/* */
/* Convert a given bitmap to a string. */
/* */
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

/*  */
/* */
/* END OF GENERIC FUNCTION FOR BITMAP AND STRING CONVERSION. */
/* */


							
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
