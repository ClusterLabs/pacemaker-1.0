/* 
 * pf1node2.c: Performance Test case 1 for Event Service Test
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
#include <time.h>

#define INVOCATION_BASE  100
#define EVENT_DATA  "Performance Test Case 1 event"
#define Event_Priority 1
#define Event_retentionTime 1000000
#define PublishName "pf1node2"
#define Pattern1 "pf01"
#define run_time 10

struct timeval tv;
SaVersionT version;
SaEvtHandleT evt_handle;
SaEvtChannelHandleT channel_handle1;	
SaEvtCallbacksT callbacks;
SaNameT ch_name;
int select_ret;
SaSelectionObjectT fd;
fd_set rset;	
SaEvtEventFilterArrayT filter_array;
time_t cur_time,wait_time;
int nTimes=0;

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{	
	nTimes++;
	saEvtEventFree(event_handle);
	return;
}

int main(int argc, char **argv)
{
	
	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;	
	
	/*initialize */
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){	
		return -1;
	}
	
	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);

	/*Open Channel */
	ch_name.length = sizeof(Pattern1);
	memcpy(ch_name.value, Pattern1, sizeof(Pattern1));

	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		return -1;
	}

	/*subscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = sizeof(Pattern1);
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(filter_array.filters[0].filter.pattern, Pattern1, sizeof(Pattern1));
	if(saEvtEventSubscribe(channel_handle1, &filter_array, 1)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		return -1;
	}
	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);
	
	/*set the current time */
	time(&cur_time);
	wait_time=cur_time;

	/*receive within runtime */
	while(wait_time+run_time>cur_time){
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		
		/*timeout used in select */
		tv.tv_sec=2;
		tv.tv_usec=0;
		
		select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
		if((select_ret == -1) || (select_ret == 0)){
			printf("select error!: %d\n",select_ret);
			printf("##Receive Events: %d \n", nTimes);
			saEvtChannelClose(channel_handle1);
			saEvtFinalize(evt_handle);
			return -1;
		}else{
			if(saEvtDispatch(evt_handle, SA_DISPATCH_ONE)!= SA_OK )
			{
				saEvtChannelClose(channel_handle1);
				saEvtFinalize(evt_handle);
				return -1;
			}

		}
		time(&cur_time);
		syslog (LOG_INFO|LOG_LOCAL7, "##Receive Event number: %d \n", nTimes) ;
	}
		
	printf("##Receive Events: %d \n", nTimes);
	printf("##time: %d \n", (int)(cur_time-wait_time));	
	
	if(saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		return -1;
	}	
	
	if(saEvtFinalize(evt_handle)!= SA_OK){
		return -1;
	}	
			
	return 0 ; 
}


