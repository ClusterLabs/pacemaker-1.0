/* 
 * pf1node1.c: Performance Test case 1 for Event Service Test
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

#define EVENT_DATA  "Performance Test Case 1 event"
#define Event_Priority 1
#define Event_retentionTime 1000000
#define PublishName "pf1node1"
#define Pattern1 "pf01"
#define run_time 10

SaVersionT version;
SaNameT ch_name;
SaEvtHandleT evt_handle;
SaEvtChannelHandleT channel_handle1;	
SaEvtCallbacksT callbacks;
SaErrorT Evt_Error;
SaEvtEventHandleT event_handle;
SaEvtEventPatternArrayT pattern_array;
SaNameT publisher_name;
SaEvtEventIdT event_id;
int data_size;
void *event_data;
time_t cur_time,wait_time;
int nTimes=0;

int main(int argc, char **argv)
{


	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;
	
	/*initialize */
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		return -1;
	}
	syslog (LOG_INFO|LOG_LOCAL7, "%s \n", "node1 init" ) ;		
	
	/*Open Channel */
	ch_name.length = sizeof(Pattern1);
	memcpy(ch_name.value, Pattern1, sizeof(Pattern1));	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		return -1;
	}

	/*event allocate */
	if(saEvtEventAllocate(channel_handle1, &event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		return -1;
	}

	/* attributes set */
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = sizeof(Pattern1);
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(pattern_array.patterns[0].pattern, Pattern1, sizeof(Pattern1));
	publisher_name.length = sizeof(PublishName);
	memcpy(publisher_name.value, PublishName, sizeof(PublishName));
	if(saEvtEventAttributesSet(event_handle, &pattern_array, Event_Priority, Event_retentionTime, &publisher_name)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		return -1;
	}

	/*set data */
	data_size = sizeof(EVENT_DATA);
	event_data = g_malloc0(data_size);
	memcpy(event_data, EVENT_DATA, data_size);
	
	/*set the current time */
	time(&cur_time);
	wait_time=cur_time;
	
	/*publish within run_time */
	while(wait_time+run_time>cur_time){
		if(saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
		{
			saEvtChannelClose(channel_handle1);
			saEvtFinalize(evt_handle);
			return -1;
		}
		time(&cur_time);
		nTimes++;
		syslog (LOG_INFO|LOG_LOCAL7, "##Publish Events: %d \n", (int)event_id) ;
	}
	syslog (LOG_INFO|LOG_LOCAL7, "##Publish Events: %d \n", nTimes) ;
	printf("##Publish Events: %d \n", nTimes);
	printf("##time: %d \n", (int)(cur_time-wait_time));
	
	/*event free */
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	free(event_data);
	if(saEvtEventFree(event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		return -1;
	}	
	
	if(saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		return -1;
	}
	
	if(saEvtFinalize(evt_handle)!= SA_OK){
		return -1;
	}	
	
	return 0;	
}



