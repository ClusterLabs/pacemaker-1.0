/* 
 * fun6node2.c: Funtion Test Case 6 for Event Service Test
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

#include "func.h"

static int nCmpResult=1,nTimes=0;
#define EVENT_DATA  "Publish 1:N Function Test Case event"
#define Event_Priority 1
#define Event_retentionTime 10000
#define PublishName "f6node1"
#define Pattern1 "func06"

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{
	nTimes++;
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = sizeof(Pattern1);
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(pattern_array.patterns[0].pattern, Pattern1, sizeof(Pattern1));
	publisher_name.length = sizeof(PublishName);
	memcpy(publisher_name.value, PublishName, sizeof(PublishName));
	void *c;
	data_size = sizeof(EVENT_DATA);
	c = g_malloc0(data_size);
	memcpy(c, EVENT_DATA, data_size);
	
	if(receivecmp(event_handle,eventDataSize,c,pattern_array,Event_Priority,
						Event_retentionTime,publisher_name,-1,-1) <0)
	{
			nCmpResult=-1;
	}
	
	free(c);
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	saEvtEventFree(event_handle);
	return;
}

int main(int argc, char **argv)
{
	int count =0,i ;	
	SaEvtChannelHandleT channel_handle1,channel_handle2;	
	
	if (inittest () != 0)
	{
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*initialize */
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){		
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);

	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d\n", Start_message, getpid ()) ;
	
	/* wait for node 1 ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();

	/*Step 1 */
	ch_name.length = sizeof("fun06");
	memcpy(ch_name.value, "fun06", sizeof("fun06"));

	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle2)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();	

	
	/*step 3 */
	/*subscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = sizeof(Pattern1);
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(filter_array.filters[0].filter.pattern, Pattern1, sizeof(Pattern1));

	for (i=0;i<3;i++)	
	{
		if(saEvtEventSubscribe(channel_handle1, &filter_array, i)
					!= SA_OK){
			saEvtChannelClose(channel_handle1);
			saEvtChannelClose(channel_handle2);
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
		if(saEvtEventSubscribe(channel_handle2, &filter_array, i+3)
					!= SA_OK){
			saEvtChannelClose(channel_handle1);
			saEvtChannelClose(channel_handle2);
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
	}
	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);	

	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	
	/*step 4: receive event 1 on node 2 */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
	if((select_ret == -1) || (select_ret == 0))
		{
			printf("select error!\n");
			saEvtChannelClose(channel_handle1);
			saEvtChannelClose(channel_handle2);
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}else{
			if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK || nCmpResult<0||nTimes!=6)
			{
				saEvtChannelClose(channel_handle1);
				saEvtChannelClose(channel_handle2);
				saEvtFinalize(evt_handle);
				syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
				return -1;
			}
		}	
		
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		
	
	
	if(saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtChannelClose(channel_handle2);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	

	if(saEvtChannelClose(channel_handle2) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
		
	saEvtFinalize(evt_handle);	
	
	/* wait for node 1 ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d \n",Signal_message, count++, SIGUSR1) ;
	pausepause();	
		
	return 0 ; 
	
}


