/* 
 * j.c:Event Service API test case for:saEvtEventDateGet
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
#include <saf/ais_base.h>
#include <saf/ais_event.h>

#define EVENT_DATA  "DataGet Test Case"

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{
	
	/*dataget1 normal call */
	SaSizeT data_size = eventDataSize;
	void *event_data = g_malloc0(data_size);
	
	if(saEvtEventDataGet(event_handle, event_data, &data_size)== SA_OK){
	
		if(data_size==sizeof(EVENT_DATA) && !strcmp(event_data,EVENT_DATA))
			printf("Event service DataGet(1) success\n");
		else
			printf("Event service DataGet(1) fail\n");
			
	}else{
		printf("Event service DataGet(1) fail\n");
	}
	
	/*dataget4 invalid event_data or data_size */
	if(saEvtEventDataGet(event_handle, event_data, NULL)== SA_ERR_INVALID_PARAM &&
	   saEvtEventDataGet(event_handle, NULL, &data_size)== SA_ERR_INVALID_PARAM){	
		printf("Event service DataGet(2) success\n");
	}else{
		printf("Event service DataGet(2) fail\n");
	}
	
	/*dataget3 too small data buffer to hold event data */
	data_size = eventDataSize-5;
	event_data = g_malloc0(data_size);
	
	if(saEvtEventDataGet(event_handle, event_data, &data_size)== SA_ERR_NO_SPACE 
		&& data_size==sizeof(EVENT_DATA)){
	
		printf("Event service DataGet(3) success\n");
	}else{
		printf("the data size == %d\n", data_size);
		printf("%s\n", (char *)event_data);
		printf("Event service DataGet(3) fail\n");
	}

	#if 0
	/*dataget5 Datasize bigger than real data buffer size */
	data_size = eventDataSize;
	event_data = g_malloc0(data_size-1);
	
	if(saEvtEventDataGet(event_handle, event_data, &data_size)!=SA_OK){
	
		printf("Event service DataGet(5) success\n");
	}else{
		printf("Event service DataGet(5) fail\n");
	}	
	#endif
			
	saEvtEventFree(event_handle);
	
	/*dataget2 invalid event hadle */
	if(saEvtEventDataGet(event_handle, event_data, &data_size)== SA_ERR_BAD_HANDLE){
		printf("Event service DataGet(4) success\n");
	}else{
		printf("Event service DataGet(4) fail\n");
	}	
	
	
	
	return;
}

int main(int argc, char **argv)
{

	SaVersionT version;
	SaEvtHandleT evt_handle;
	SaEvtCallbacksT callbacks;
	SaEvtChannelHandleT channel_handle;
	SaNameT ch_name;
	int data_size, select_ret;
	SaEvtEventHandleT event_handle;
	SaEvtEventPatternArrayT pattern_array;
	SaNameT publisher_name;
	SaEvtEventIdT event_id;
	void *event_data;
	SaEvtEventFilterArrayT filter_array;
	SaSelectionObjectT fd;
	fd_set rset;
	
	/*initialize */
	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		printf("Event service initialize failed\n");
		return 1;
	}

	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);

	/*channel open */
	ch_name.length = 3;
	memcpy(ch_name.value, "aaa", 3);
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle)
			!= SA_OK){
		printf("Event channel open failed\n");
		return 1;
	}


	/*subscribe/unsubscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = 6;
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(6);
	memcpy(filter_array.filters[0].filter.pattern, "abcxyz", 6);
	saEvtEventSubscribe(channel_handle, &filter_array, 1);

	/*event allocate */
	saEvtEventAllocate(channel_handle, &event_handle);
	printf("the event handle == %d\n", (unsigned int)event_handle);

	/* attributes set */
	saEvtEventAttributesSet(event_handle, NULL, 1, 1000, NULL);
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 6;
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	memcpy(pattern_array.patterns[0].pattern, "abcxyz", 6);
	publisher_name.length = 7;
	memcpy(publisher_name.value, "forrest", 7);
	saEvtEventAttributesSet(event_handle, &pattern_array, 1, 2000, &publisher_name);

	/*publish */
	data_size = sizeof(EVENT_DATA);
	event_data = g_malloc0(data_size);
	memcpy(event_data, EVENT_DATA, data_size);
	saEvtEventPublish(event_handle, event_data, data_size, &event_id);
	printf("the event id of published event is: %Ld\n", event_id);
	
	/*dispatch, event_data_get(in callback function) */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
	if((select_ret == -1) || (select_ret == 0)){
		printf("select error!\n");
		return 1;
	}else{
		saEvtDispatch(evt_handle, SA_DISPATCH_ONE);
	}

	/*event free */
	saEvtEventFree(event_handle);

	/*channel close */
	saEvtChannelClose(channel_handle);

	/*finalize */
	saEvtFinalize(evt_handle);
	return 0;
}
