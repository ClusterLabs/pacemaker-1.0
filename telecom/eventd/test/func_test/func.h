/* 
 * func.h:  Head File for Event Service Funtion Test
 * saEvtInitialize, saEvtFinalize, saEvtSelectionObjectGet
 *
 * Copyright (C) 2004 Wilna, Wei <willna.wei@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <saf/ais.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>

#define INVOCATION_BASE  100
#define Start_message " evt_start "
#define Success_message " evt_success "
#define Fail_message " evt_fail "
#define Signal_message " evt_signal "

struct timeval tv;
SaVersionT version;
SaEvtHandleT evt_handle;
SaEvtCallbacksT callbacks;
SaNameT ch_name;
int select_ret;
SaSelectionObjectT fd;
fd_set rset;	
SaErrorT Evt_Error;
SaInvocationT evt_invocation;
int data_size;
SaEvtEventHandleT event_handle,event_handle1;
SaEvtEventPatternArrayT pattern_array, pattern_array_out,pattern_mode[6];
SaNameT publisher_name, publisher_name_out;
SaUint8T priority;
SaTimeT retention_time, publish_time;
SaEvtEventIdT event_id,eventid2clear;
void *event_data;
SaEvtEventFilterArrayT filter_array,filter_mode[4];
time_t cur_time,wait_time;

int patterncmp (SaEvtEventPatternArrayT pattern_array, SaEvtEventPatternArrayT pattern_array_out);
int receivecmp (const SaEvtEventHandleT eventHandlein,
				const SaSizeT Datasizein,
				void* Datain,
				const SaEvtEventPatternArrayT patternArrayin,
				const SaUint8T priorityin,
				const SaTimeT retentionTimein,
				const SaNameT publisherNamein,
				const SaTimeT publishTimein,
				const SaEvtEventIdT eventIdin);

void pausepause (void); 
void termhandler (int signumber);
void usrhandler (int signumber);
int inittest(void);
void initparam (void);
int setmode (void);
int freemode (void);
int Publish_Event_mode (SaEvtChannelHandleT channel_handle_in);

/* For debug reason, use the function to avoid pause funtion*/
void pausepause (void)
{
	pause ();
}

/* 
 * Description: 
 * 		This hanlder is for exception use. When monitor machine sends SIGTERM 
 * signal to node, node app should close the event service it used. 
 * AIS specifies that event daemon should do this for process when process 
 * exits. However, we add this handler. 
 */

void termhandler (int signumber)
{
	
/*	finalize () ; */
	exit (0) ;
}

/*
 *Description:
 *	 for SIGUSR1 handler. Wait and Go mechanism. Nothing will done here.
 *
 */

void usrhandler (int signumber)
{
	return ;
}

/*
 *Description: 
 *	Make some prepation work for test case.For example, sigal handler seting up, socket
 *creating, etc. 
 *Returns :
 * 	0 : success
 * -1 : failure
 */
 
int inittest (void)
{
	
	
	/* setup SIGTERM handler for exception */
	if ( signal (SIGTERM, termhandler) == SIG_ERR)
		{
			return -1;
		}

  	
	/* setup synchronous signal handler for nodes sychronization */
    	if ( signal (SIGUSR1, usrhandler) == SIG_ERR)
		{
			return -1;
		}
	initparam();
	return 0 ;
}

/*
 function description: 
	initailize checkpiont parameters for library "init" and "open ".
 returns :
 	none 
*/

void initparam(void)
{
	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;
	evt_invocation = INVOCATION_BASE ;	
	
	/*timeout use in select */
	tv.tv_sec=2;
	tv.tv_usec=0;
}

/*
 Compare two patterns if they are the same
 
 Returns;
 
 	Same: 0
 	different: -1 
*/
int patterncmp(SaEvtEventPatternArrayT pattern_array, SaEvtEventPatternArrayT pattern_array_out)
{
	int i,j;

	if(pattern_array_out.patternsNumber!=pattern_array.patternsNumber) 
	{
		return -1;
	}

	for(i=0;i<pattern_array.patternsNumber;i++)
	{
		if(pattern_array_out.patterns[i].patternSize!=pattern_array.patterns[i].patternSize)
		{
			return -1;
		}
		for(j=0;j<pattern_array.patterns[i].patternSize;j++)
		{
			if(pattern_array_out.patterns[i].pattern[j]!=pattern_array.patterns[i].pattern[j])
			{
					return -1;
			}
		}
	}

	return 0;

}

/*
 Compare if the attributes get from event if the same with the input values
 
 Returns;
 
 	Same: 0
 	different: -1 
*/
int receivecmp (const SaEvtEventHandleT eventHandlein,
				const SaSizeT Datasizein,
				void* Datain,
				const SaEvtEventPatternArrayT patternArrayin,
				const SaUint8T priorityin,
				const SaTimeT retentionTimein,
				const SaNameT publisherNamein,
				const SaTimeT publishTimein,
				const SaEvtEventIdT eventIdin)
{
	int i;
	SaSizeT data_size = 100;
	void *event_data = g_malloc0 (data_size);	
	saEvtEventDataGet (event_handle, event_data, &data_size);	
	SaEvtEventPatternArrayT pattern_array;
	SaNameT publisher_name;
	SaUint8T priority;
	SaTimeT retention_time, publish_time;
	SaEvtEventIdT event_id;
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *) g_malloc (sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 7;
	pattern_array.patterns[0].pattern = (SaUint8T *) g_malloc (7);
	saEvtEventAttributesGet (eventHandlein, &pattern_array, &priority,&retention_time, &publisher_name, &publish_time, &event_id);
	printf ("the data size == %d\n", data_size);
	printf ("%s\n", (char *)event_data);
	printf ("%d\n", (int)retention_time);
	
	if (data_size!=Datasizein)
	{
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n datasize", Fail_message) ;
			return -1;
	}


	char *c1=event_data;
	char *c2=Datain;
	for (i=0;i<data_size;i++)
	{
		/*if(((char *)event_data)[i]!=((char *)Datain)[i]) */
		if(c1[i]!=c2[i])
		{		
			return -1;
		}
	}
	
	if (patterncmp(pattern_array, patternArrayin)<0)
	{
		printf("\npattern_array don't compare\n");		
		return -1;
	}

	if(retention_time!=retentionTimein)
	{	
		printf("\nretention_time don't compare: %d,%d\n",(int)retention_time,(int)retentionTimein);	
		return -1;
	}

	if(priority!=priorityin)
	{		
		printf("\npriority don't compare\n");	
		return -1;
	}

	if(publisher_name.length!=publisherNamein.length)	
	{		
		printf("\npublisher_name.length don't compare\n");	
		return -1;
	}

	for(i=0;i<publisher_name.length;i++)
	{
		if(publisher_name.value[i]!=publisherNamein.value[i])
		{		
			printf("\npublisher_name.value don't compare\n");	
			return -1;
		}
	}
	if (publishTimein!=-1)
	{
		if(publishTimein!=publish_time)		
			{		
				printf("\npublishTimein don't compare\n");	
				return -1;
			}
	}

	if (eventIdin!=-1)
	{
		if(eventIdin!=event_id)		
			{		
				printf("\neventIdin don't compare\n");	
				return -1;
			}
	}  
	free(event_data);

	return 0;
}

/*
	set patterns and filters used in subscribe test
	Set 6 Patterns and 4 filters
	Refer: Test Spec
*/
int setmode(void)
{

	/*/////////////////PATTERN MODE SET////////////////////// */
	
	/*pattern 1.	{abd, xyz, 123} */
	pattern_mode[0].patternsNumber = 3;
	pattern_mode[0].patterns = (SaEvtEventPatternT *)g_malloc(3*sizeof(SaEvtEventPatternT));
	
	pattern_mode[0].patterns[0].patternSize = sizeof("abd")-1;
	pattern_mode[0].patterns[0].pattern = (SaUint8T *)g_malloc(sizeof("abd")-1);
	memcpy(pattern_mode[0].patterns[0].pattern, "abd", sizeof("abd")-1);
	
	pattern_mode[0].patterns[1].patternSize = sizeof("xyz")-1;
	pattern_mode[0].patterns[1].pattern = (SaUint8T *)g_malloc(sizeof("xyz")-1);
	memcpy(pattern_mode[0].patterns[1].pattern, "xyz", sizeof("xyz")-1);
	
	pattern_mode[0].patterns[2].patternSize = sizeof("123")-1;
	pattern_mode[0].patterns[2].pattern = (SaUint8T *)g_malloc(sizeof("123")-1);
	memcpy(pattern_mode[0].patterns[2].pattern, "123", sizeof("123")-1);
	
	/*pattern 2.	{abc, xyz} */
	pattern_mode[1].patternsNumber = 2;
	pattern_mode[1].patterns = (SaEvtEventPatternT *)g_malloc(2*sizeof(SaEvtEventPatternT));
	
	pattern_mode[1].patterns[0].patternSize = sizeof("abc")-1;
	pattern_mode[1].patterns[0].pattern = (SaUint8T *)g_malloc(sizeof("abc")-1);
	memcpy(pattern_mode[1].patterns[0].pattern, "abc", sizeof("abc")-1);
	
	pattern_mode[1].patterns[1].patternSize = sizeof("xyz")-1;
	pattern_mode[1].patterns[1].pattern = (SaUint8T *)g_malloc(sizeof("xyz")-1);
	memcpy(pattern_mode[1].patterns[1].pattern, "xyz", sizeof("xyz")-1);

	/*pattern 3.	{abc, 12, 23} */
	pattern_mode[2].patternsNumber = 3;
	pattern_mode[2].patterns = (SaEvtEventPatternT *)g_malloc(3*sizeof(SaEvtEventPatternT));
	
	pattern_mode[2].patterns[0].patternSize = sizeof("abc")-1;
	pattern_mode[2].patterns[0].pattern = (SaUint8T *)g_malloc(sizeof("abc")-1);
	memcpy(pattern_mode[2].patterns[0].pattern, "abc", sizeof("abc")-1);
	
	pattern_mode[2].patterns[1].patternSize = sizeof("12")-1;
	pattern_mode[2].patterns[1].pattern = (SaUint8T *)g_malloc(sizeof("12")-1);
	memcpy(pattern_mode[2].patterns[1].pattern, "12", sizeof("12")-1);
	
	pattern_mode[2].patterns[2].patternSize = sizeof("23")-1;
	pattern_mode[2].patterns[2].pattern = (SaUint8T *)g_malloc(sizeof("23")-1);
	memcpy(pattern_mode[2].patterns[2].pattern, "23", sizeof("23")-1);	

	
	/*pattern 4.	{abcd, 12} */
	pattern_mode[3].patternsNumber = 2;
	pattern_mode[3].patterns = (SaEvtEventPatternT *)g_malloc(2*sizeof(SaEvtEventPatternT));
	
	pattern_mode[3].patterns[0].patternSize = sizeof("abcd")-1;
	pattern_mode[3].patterns[0].pattern = (SaUint8T *)g_malloc(sizeof("abcd")-1);
	memcpy(pattern_mode[3].patterns[0].pattern, "abcd", sizeof("abcd")-1);
	
	pattern_mode[3].patterns[1].patternSize = sizeof("12")-1;
	pattern_mode[3].patterns[1].pattern = (SaUint8T *)g_malloc(sizeof("12")-1);
	memcpy(pattern_mode[3].patterns[1].pattern, "12", sizeof("12")-1);

	/*pattern 5.	{abc} */
	pattern_mode[4].patternsNumber = 1;
	pattern_mode[4].patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	
	pattern_mode[4].patterns[0].patternSize = sizeof("abc")-1;
	pattern_mode[4].patterns[0].pattern = (SaUint8T *)g_malloc(sizeof("abc")-1);
	memcpy(pattern_mode[4].patterns[0].pattern, "abc", sizeof("abc")-1);
	
	
	/*pattern 6.	{abcde} */
	pattern_mode[5].patternsNumber = 1;
	pattern_mode[5].patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	
	pattern_mode[5].patterns[0].patternSize = sizeof("abcde")-1;
	pattern_mode[5].patterns[0].pattern = (SaUint8T *)g_malloc(sizeof("abcde")-1);
	memcpy(pattern_mode[5].patterns[0].pattern, "abcde", sizeof("abcde")-1);	

	
	/*/////////////////FILTER MODE SET////////////////////// */

	/*Filter 1.	{[ab, PRE], [-, ALL], [123, SUB]} */
	filter_mode[0].filtersNumber = 3;
	filter_mode[0].filters = g_malloc0(3*sizeof(SaEvtEventFilterT));
	
	filter_mode[0].filters[0].filterType = SA_EVT_PREFIX_FILTER;
	filter_mode[0].filters[0].filter.patternSize = sizeof("ab")-1;
	filter_mode[0].filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof("ab")-1);
	memcpy(filter_mode[0].filters[0].filter.pattern, "ab", sizeof("ab")-1);
		
	filter_mode[0].filters[1].filterType = SA_EVT_PASS_ALL_FILTER;

	filter_mode[0].filters[2].filterType = SA_EVT_SUFFIX_FILTER;
	filter_mode[0].filters[2].filter.patternSize = sizeof("123")-1;
	filter_mode[0].filters[2].filter.pattern = (SaUint8T *)g_malloc(sizeof("23")-1);
	memcpy(filter_mode[0].filters[2].filter.pattern, "123", sizeof("123")-1);

	
	/*Filter 2.	{[bc, SUB], [xyz, EXACT], [-, ALL]} */
	filter_mode[1].filtersNumber = 3;
	filter_mode[1].filters = g_malloc0(3*sizeof(SaEvtEventFilterT));
	
	filter_mode[1].filters[0].filterType = SA_EVT_SUFFIX_FILTER;
	filter_mode[1].filters[0].filter.patternSize = sizeof("bc")-1;
	filter_mode[1].filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof("bc")-1);
	memcpy(filter_mode[1].filters[0].filter.pattern, "bc", sizeof("bc")-1);
		
	filter_mode[1].filters[1].filterType = SA_EVT_EXACT_FILTER;
	filter_mode[1].filters[1].filter.patternSize = sizeof("xyz")-1;
	filter_mode[1].filters[1].filter.pattern = (SaUint8T *)g_malloc(sizeof("xyz")-1);
	memcpy(filter_mode[1].filters[1].filter.pattern, "xyz", sizeof("xyz")-1);

	filter_mode[1].filters[2].filterType = SA_EVT_PASS_ALL_FILTER;

	/*Filter 3.	{[abc, EXACT], [(size=0), -]} */
	filter_mode[2].filtersNumber = 2;
	filter_mode[2].filters = g_malloc0(2*sizeof(SaEvtEventFilterT));
	
	filter_mode[2].filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_mode[2].filters[0].filter.patternSize = sizeof("abc")-1;
	filter_mode[2].filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof("abc")-1);
	memcpy(filter_mode[2].filters[0].filter.pattern, "abc", sizeof("abc")-1);
		
	filter_mode[2].filters[1].filter.patternSize = 0;

	/*Filter 4.	{[abcd, PRE]} */
	filter_mode[3].filtersNumber = 1;
	filter_mode[3].filters = g_malloc0(sizeof(SaEvtEventFilterT));
	
	filter_mode[3].filters[0].filterType = SA_EVT_PREFIX_FILTER;
	filter_mode[3].filters[0].filter.patternSize = sizeof("abcd")-1;
	filter_mode[3].filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof("abcd")-1);
	memcpy(filter_mode[3].filters[0].filter.pattern, "abcd", sizeof("abcd")-1);

	return 0;
}

/*
	Free patterns and filters used in subscribe test
	Free 6 Patterns and 4 filters
	
*/
int freemode(void)
{
	int i,j;
	
	for(i=0;i<6;i++)
	{
		for(j=0;j<pattern_mode[i].patternsNumber;j++)
		{
			free(pattern_mode[i].patterns[j].pattern);		
		}
		free(pattern_mode[i].patterns);
	}

	for(i=0;i<4;i++)
	{
		for(j=0;j<filter_mode[i].filtersNumber;j++)
		{
			free(filter_mode[i].filters[j].filter.pattern);		
		}
		free(filter_mode[i].filters);
	}


	return 0;
}

/*Publish 6 events on a given channel, used in subscribe test */
int Publish_Event_mode(SaEvtChannelHandleT channel_handle_in)
{
	int i;
	
	/*event allocate */
	if(saEvtEventAllocate(channel_handle_in, &event_handle)!= SA_OK)
	{
		return -1;
	}

	publisher_name.length = sizeof("subscribe test");
	memcpy(publisher_name.value, "subscribe test", sizeof("subscribe test"));

	/*publish */
	for(i=0;i<6;i++)
	{

		data_size = i+1;
		event_data = g_malloc0(data_size);
		memcpy(event_data, "ABCDEFG", data_size);
		
		if(saEvtEventAttributesSet(event_handle, &pattern_mode[i],1, 50000, &publisher_name)!= SA_OK)
		{
			free(event_data);
			saEvtEventFree(event_handle);
			return -1;
		}
		if(saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
		{
			free(event_data);
			saEvtEventFree(event_handle);
			return -1;
		}
			syslog (LOG_INFO|LOG_LOCAL7, "event %d published\n",i) ;
	}
	
	/*event free */
	free(event_data);
	if(saEvtEventFree(event_handle)!= SA_OK)
	{
		return -1;
	}

	return 0;
}


