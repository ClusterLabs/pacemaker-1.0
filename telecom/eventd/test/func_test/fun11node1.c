/* 
 * fun11node1.c: Funtion Test Case 11 for Event Service Test
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

static int count =0,nTimes=0;


#define EVENT_DATA  "RetetionTimeClear 1 Function Test Case event"
#define Event_Priority 1
#define Event_retentionTime 1000000
#define PublishName "f11node1"
#define Pattern1 "func11"

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

	SaEvtChannelHandleT channel_handle1;	

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
	
	
	/*Step 1. */
	ch_name.length = sizeof("fun11");
	memcpy(ch_name.value, "fun11", sizeof("fun11"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*count=0 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	


	/*step 3: publish event 1on node 1 with retentiontime set to 1000000 */
	/*event allocate */
	if(saEvtEventAllocate(channel_handle1, &event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
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
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*publish */
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
	eventid2clear=event_id;
	
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

	/*step 3 */
	/*RetetionTimeClear */
	Evt_Error=saEvtEventRetentionTimeClear(channel_handle1, eventid2clear);
	if(Evt_Error!=SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s retentiontime \n", Fail_message) ;
		return -1;
	}		

	/*step 4 */
	/*subscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = sizeof(Pattern1);
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(filter_array.filters[0].filter.pattern, Pattern1, sizeof(Pattern1));
	
	if(saEvtEventSubscribe(channel_handle1, &filter_array, 1)
				!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);

	
	/* receive */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if(select_ret != 0){
		printf("select error!\n");
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s select : %d\n", Fail_message,select_ret ) ;
		return -1;
	}

	/*count=2 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	
	
	if(saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*count=3 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*finalize */
	if(saEvtFinalize(evt_handle)!= SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;	
	
	return 0;

	
}



