/* 
 * fun14node1.c: Funtion Test Case 14 for Event Service Test
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

static int count =0,nTimes=0,nCmpResult=1;
SaEvtChannelHandleT channel_handle1;	


#define EVENT_DATA  "RetetionTimeClear 2 Function Test Case event"
#define Event_Priority 1
#define Event_retentionTime 8000
#define PublishName "f14node1"
#define Pattern1 "func141"
#define Pattern2 "func142"

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{		
	nTimes++;
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;

	/*step 3 */
	/*RetetionTimeClear */
	Evt_Error=saEvtEventRetentionTimeClear(channel_handle1, eventid2clear);
	if (Evt_Error!=SA_OK){
		nCmpResult=-1;
		return;
	}

	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
   	pausepause () ;

	/*step5:process and free event1 on node1 */
	saEvtEventFree(event_handle);	
	return;
}

int main(int argc, char **argv)
{

	SaEvtChannelHandleT channel_handle1;	

	if (inittest () != 0)
	{
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	
	/*initialize */
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	if (saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);

	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d\n", Start_message, getpid ()) ;	
	
	/*open channel */
	ch_name.length = sizeof("fun14");
	memcpy(ch_name.value, "fun14", sizeof("fun14"));
	
	if (saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*count = 0 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	


	/*step 1: publish event 1,2 on node 1 with retentiontime set to 10000 */
	/*event allocate */
	if (saEvtEventAllocate(channel_handle1, &event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/* attributes set/get */
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = sizeof(Pattern1);
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(pattern_array.patterns[0].pattern, Pattern1, sizeof(Pattern1));
	publisher_name.length = sizeof(PublishName);
	memcpy(publisher_name.value, PublishName, sizeof(PublishName));
	if (saEvtEventAttributesSet(event_handle, &pattern_array, Event_Priority, Event_retentionTime, &publisher_name)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*publish event1 */
	data_size = sizeof(EVENT_DATA);
	event_data = g_malloc0(data_size);
	memcpy(event_data, EVENT_DATA, data_size);
	
	if (saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	


	/*publish event2 */
	memcpy(pattern_array.patterns[0].pattern, Pattern2, sizeof(Pattern2));
	if (saEvtEventAttributesSet(event_handle, &pattern_array, Event_Priority, Event_retentionTime, &publisher_name)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	if (saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*set the current time */
	time(&cur_time);
	wait_time=cur_time;

	
	/*event free */
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	free(event_data);
	if (saEvtEventFree(event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	

	/*count =1 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		

	/*step 4:Subscribe and try to receive event1 on node2 */
	/*subscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = sizeof(Pattern1);
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(filter_array.filters[0].filter.pattern, Pattern1, sizeof(Pattern1));

	if (saEvtEventSubscribe(channel_handle1, &filter_array, 1)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);

	/*receive */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if (select_ret !=0){
		printf("select error!\n");
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s ,%d\n", Fail_message,select_ret ) ;
		return -1;
	}



	/*count = 2 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*step6:wait retentionTime on node1 and node2 */
	do{		
		time(&cur_time);	
		sleep(1);
	}while (wait_time+Event_retentionTime/1000>cur_time);

	/*step 7:Subscribe and try to receive event2 on node1,2 */
	/*subscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = sizeof(Pattern2);
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof(Pattern2));
	memcpy(filter_array.filters[0].filter.pattern, Pattern2, sizeof(Pattern2));

	if (saEvtEventSubscribe(channel_handle1, &filter_array, 2)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);

	/*receive */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if (select_ret != 0){
		printf("select error!\n");
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s select %d \n", Fail_message,select_ret) ;
		return -1;
	}

	/*count = 3 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	
	if (saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}		

	/*count = 4 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		

	/*finalize */
	if (saEvtFinalize(evt_handle)!= SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;	
	
	return 0;

	
}




