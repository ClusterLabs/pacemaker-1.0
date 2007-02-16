/* 
 * fun13node1.c: Funtion Test Case 13 for Event Service Test
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

static int count =0,nTimes=0,SubIDCount=1;
SaEvtChannelHandleT channel_handle1,channel_handle2;	


#define EVENT_DATA  "RetetionTimeClear 2 Function Test Case event"
#define Event_Priority 1
#define Event_retentionTime 5000
#define PublishName "f13node1"
#define Pattern1 "func131"
#define Pattern2 "func132"


int SubscribeReceive(SaEvtChannelHandleT channel_handlein);

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{		
	nTimes++;
	saEvtEventFree(event_handle);
	return;
}

/*Subscribe and Receive event 1,2 on some channel */
int SubscribeReceive(SaEvtChannelHandleT channel_handlein)
{
	/*subscribe 1, 2 */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = sizeof(Pattern1);
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(filter_array.filters[0].filter.pattern, Pattern1, sizeof(Pattern1));

	if(saEvtEventSubscribe(channel_handlein, &filter_array, SubIDCount++)!= SA_OK){

		syslog (LOG_INFO|LOG_LOCAL7, "sub1 error\n") ;
		return -1;
	}	

	memcpy(filter_array.filters[0].filter.pattern, Pattern2, sizeof(Pattern2));

	if(saEvtEventSubscribe(channel_handlein, &filter_array, SubIDCount++)!= SA_OK){
		
		syslog (LOG_INFO|LOG_LOCAL7, "sub2 error\n") ;
		return -1;
	}

	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);

	/*receive */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	tv.tv_sec=2;
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if((select_ret == -1) || (select_ret == 0)){
		syslog (LOG_INFO|LOG_LOCAL7, "select error\n") ;
		return -1;
	}else{
		sleep(1);
		if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK ){
			syslog (LOG_INFO|LOG_LOCAL7, "dispatch error\n") ;
			return -1;
		}
	}

	switch(SubIDCount)
	{
		case 3: 
			if(nTimes!=2) return -1;
			break;
		case 5: 
			if(nTimes!=4) return -1;
			break;
		case 7: 
			if(nTimes!=5) return -1;
			break;		
		case 9: 
			if(nTimes!=6) return -1;
			break;
		default:
			return -1;
	}
	
	return 0;
}

int main(int argc, char **argv)
{


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
	
	
	/*open channel */
	ch_name.length = sizeof("fun13");
	memcpy(ch_name.value, "fun13", sizeof("fun13"));
	
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

	/*count =0 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
		pausepause () ;	


	/*step 1: Publish event1 (retention time: 10000) and event2 (retention time: 20000) on node1 channel1~2 */
	/*event allocate */
	if(saEvtEventAllocate(channel_handle1, &event_handle)!= SA_OK)
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
	if(saEvtEventAttributesSet(event_handle, &pattern_array, Event_Priority, Event_retentionTime*2, &publisher_name)!= SA_OK)
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
	if(saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*set the current time */
	time(&cur_time);
	wait_time=cur_time;

	/*publish event2 */
	memcpy(pattern_array.patterns[0].pattern, Pattern2, sizeof(Pattern2));
	if(saEvtEventAttributesSet(event_handle, &pattern_array, Event_Priority, Event_retentionTime*4, &publisher_name)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	if(saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*event free */
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	free(event_data);
	if(saEvtEventFree(event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	

	/*count=1 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
   	pausepause () ;		

		

	/*step 2 */
	/*subscribe and receive event1,2 on both nodes channel1 */
	if(SubscribeReceive(channel_handle1)<0){
		saEvtChannelClose(channel_handle1);
		saEvtChannelClose(channel_handle2);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*step3:wait 5 second on node1 and node2 */
	do{
		time(&cur_time);
		sleep(1);
	}while(wait_time+Event_retentionTime/1000>cur_time);
	wait_time+=Event_retentionTime/1000;

	/*count=2 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		
		
	/*step 4 */
	/*subscribe and receive event1,2 on both nodes channel2 */
	if(SubscribeReceive(channel_handle2)<0){
		saEvtChannelClose(channel_handle1);
		saEvtChannelClose(channel_handle2);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*step5:wait 5 second on node1 and node2 */
	do{
		sleep(1);
		time(&cur_time);
	}while(wait_time+Event_retentionTime/1000>cur_time);
	wait_time+=Event_retentionTime/1000;	

	/*count=3 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*step 6 */
	/*subscribe and receive event1,2 on both nodes channel1 */
	if(SubscribeReceive(channel_handle1)<0){
		saEvtChannelClose(channel_handle1);
		saEvtChannelClose(channel_handle2);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	/*step7:wait 5 second on node1 and node2 */
	do{
		sleep(1);
		time(&cur_time);
	}while(wait_time+Event_retentionTime/1000>cur_time);
	wait_time+=Event_retentionTime/1000;

	/*count=4 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*step 8 */
	/*subscribe and receive event1,2 on both nodes channel2 */
	if(SubscribeReceive(channel_handle2)<0){
		saEvtChannelClose(channel_handle1);
		saEvtChannelClose(channel_handle2);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*count=5 */
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
		

	/*finalize */
	if(saEvtFinalize(evt_handle)!= SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;	
	
	return 0;

	
}





