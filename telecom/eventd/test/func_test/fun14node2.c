/* 
 * fun14node2.c: Funtion Test Case 14 for Event Service Test
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

#define EVENT_DATA  "RetetionTimeClear 2 Function Test Case event"
#define Event_Priority 1
#define Event_retentionTime 8000
#define PublishName "f14node2"
#define Pattern1 "func141"
#define Pattern2 "func142"

static int count =0,nTimes=0,nCmpResult=1;
SaEvtChannelHandleT channel_handle1;	

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{	
	
	nTimes++;

	SaEvtEventPatternArrayT pattern_array;
	SaNameT publisher_name;
	SaUint8T priority;
	SaTimeT retention_time, publish_time;
	SaEvtEventIdT event_id;
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 7;
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(7);
	saEvtEventAttributesGet(event_handle, &pattern_array, &priority,&retention_time, &publisher_name, &publish_time, &event_id);

	/*step 3 */
	/*RetetionTimeClear */
syslog (LOG_INFO|LOG_LOCAL7, "%s \n", "before timeclear") ;
	Evt_Error=saEvtEventRetentionTimeClear(channel_handle1, event_id);
syslog (LOG_INFO|LOG_LOCAL7, "%s \n", "after timeclear") ;
	if (Evt_Error!=SA_OK){
syslog (LOG_INFO|LOG_LOCAL7, "%d \n", Evt_Error) ;
		nCmpResult=-1;
		return;
	}

	/*count=2 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;

	/*step5:process and free event1 on node1 */
	saEvtEventFree(event_handle);	
	return;
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
	if (saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){		
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);

	
	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d\n", Start_message, getpid ()) ;
	
	/*count=0 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();

	ch_name.length = sizeof("fun14");
	memcpy(ch_name.value, "fun14", sizeof("fun14"));

	if (saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*wait for node 1 do step 1: publish event 1,2 */
	/*count=1 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();	


	/*step 2 */
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
	if ((select_ret == -1) || (select_ret == 0)){
		printf("select error!\n");
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}else{
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", "dispatch begin") ;
		if (saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK || nCmpResult<0)
		{
			saEvtChannelClose(channel_handle1);
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
			syslog (LOG_INFO|LOG_LOCAL7, "dispatch end\n" ) ;

	}

	/*count=3 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();	


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
	tv.tv_sec=2;
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if (select_ret != 0){
		printf("select error!\n");
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s , %d\n", Fail_message,select_ret ) ;
		return -1;
	}
		
	/*count=4 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		
	
	if (saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	
		
	if (saEvtFinalize(evt_handle)!= SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	
	/*count=5 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d \n",Signal_message, count++, SIGUSR1) ;
	pausepause();	
		
	return 0 ; 
	
}


