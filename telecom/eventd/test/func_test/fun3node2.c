/* 
 * fun3node2.c: Funtion Test Case 3 for Event Service Test
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

#define EVENT_DATA  "Unlink Function Test Case node2"
int nTimes=0; /*indicate how many events have been received */
/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{
	SaSizeT data_size = 40;
	void *event_data = g_malloc0(data_size);

	nTimes++;	
	saEvtEventDataGet(event_handle, event_data, &data_size);
	
	saEvtEventFree(event_handle);
	free(event_data);
	return;
}

int main(int argc, char **argv)
{
	int count =0 ;	
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
	
	/*count=0 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();

	/*Step 2 */
	ch_name.length = sizeof("fun03");
	memcpy(ch_name.value, "fun03", sizeof("fun03"));
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	
	/*step 4 */
	/*subscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = 6;
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(6);
	memcpy(filter_array.filters[0].filter.pattern, "func03", 6);

	if(saEvtEventSubscribe(channel_handle1, &filter_array, 1)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	
	/*count=1 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();	

	/*event allocate */
	if(saEvtEventAllocate(channel_handle1, &event_handle)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/* attributes set/get */
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 6;
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	memcpy(pattern_array.patterns[0].pattern, "func03", 6);
	publisher_name.length = 7;
	memcpy(publisher_name.value, "f3node2", 7);

	if(saEvtEventAttributesSet(event_handle, &pattern_array, 1, 20000, &publisher_name)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*publish */
	data_size = sizeof(EVENT_DATA);
	event_data = g_malloc0(data_size);
	memcpy(event_data, EVENT_DATA, data_size);
	if(saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*event free */
	free(filter_array.filters[0].filter.pattern);
	free(filter_array.filters);
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	free(event_data);
	if(saEvtEventFree(event_handle)!= SA_OK){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
		
	/*count=2 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if((select_ret == -1) || (select_ret == 0)){
		printf("select error!\n");
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}else{
		if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK||nTimes!=2){
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
	}
	
	/*count=3 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 3, 1000000, &channel_handle2)
			!= SA_ERR_NOT_EXIST){
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*count=4 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		
	
	if(saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	if((Evt_Error=saEvtFinalize(evt_handle))!=SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	/*count=5 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();	
		
	return 0 ; 
	
}
