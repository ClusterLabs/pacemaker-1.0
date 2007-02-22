/* 
 * event_testclient.c: demo for event service
 *
 * Copyright (C) 2004 Forrest,Zhao <forrest.zhao@intel.com>
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

#include <clplumbing/cl_signal.h>
#include "event.h"

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{
	SaSizeT data_size = 40;
	void *event_data = g_malloc0(data_size);
	
	saEvtEventDataGet(event_handle, event_data, &data_size);
	printf("the data size == %d\n", data_size);
	printf("%s\n", (char *)event_data);
	saEvtEventFree(event_handle);
	return;
}

int main(int argc, char **argv)
{

	SaVersionT version;
	SaEvtHandleT evt_handle;
	SaEvtCallbacksT callbacks;
	SaEvtChannelHandleT channel_handle, channel_handle1;
	SaNameT ch_name;
	int data_size, select_ret;
	SaEvtEventHandleT event_handle, event_handle1;
	SaEvtEventPatternArrayT pattern_array, pattern_array_out;
	SaNameT publisher_name, publisher_name_out;
	SaUint8T priority;
	SaTimeT retention_time, publish_time;
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
	printf("the fd for selection is: %d\n", fd);

	/*channel open */
	ch_name.length = 3;
	memcpy(ch_name.value, "aaa", 3);
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle)
			!= SA_OK){
		printf("Event channel open failed\n");
		return 1;
	}
	if(saEvtChannelOpen(evt_handle, &ch_name, 5, 1000000, &channel_handle1)
			!= SA_OK){
		printf("Event channel open failed");
		return 1;
	}
	printf("the channel handle == %d\n", (unsigned int)channel_handle);
	printf("the channel handle1 == %d\n", (unsigned int)channel_handle1);

	/*subscribe/unsubscribe */
	filter_array.filtersNumber = 1;
	filter_array.filters = g_malloc0(sizeof(SaEvtEventFilterT));
	filter_array.filters[0].filterType = SA_EVT_EXACT_FILTER;
	filter_array.filters[0].filter.patternSize = 6;
	filter_array.filters[0].filter.pattern = (SaUint8T *)g_malloc(6);
	memcpy(filter_array.filters[0].filter.pattern, "abcxyz", 6);
	saEvtEventSubscribe(channel_handle, &filter_array, 1);
/*	saEvtEventUnsubscribe(channel_handle, 1); */

	/*event allocate */
	saEvtEventAllocate(channel_handle, &event_handle);
	saEvtEventAllocate(channel_handle1, &event_handle1);
	printf("the event handle == %d\n", (unsigned int)event_handle);
	printf("the event handle1 == %d\n", (unsigned int)event_handle1);

	/* attributes set/get */
	saEvtEventAttributesSet(event_handle, NULL, 1, 1000, NULL);
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 6;
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	memcpy(pattern_array.patterns[0].pattern, "abcxyz", 6);
	publisher_name.length = 7;
	memcpy(publisher_name.value, "forrest", 7);
	saEvtEventAttributesSet(event_handle, &pattern_array, 1, 2000, &publisher_name);

	pattern_array_out.patternsNumber = 1;
	pattern_array_out.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array_out.patterns[0].patternSize = 6;
	pattern_array_out.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	saEvtEventAttributesGet(event_handle, &pattern_array_out, &priority, &retention_time, &publisher_name_out, &publish_time, &event_id);
	printf("the priority == %d\n",priority);
	printf("retention_time == %Ld\n",retention_time); 
	printf("publish_time == %Ld\n", publish_time);
	printf("event_id == %Ld\n", event_id);

	/*publish */
	data_size = 20;
	event_data = g_malloc0(data_size);
	memcpy(event_data, "first event", data_size);
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
		printf("***begin dispatch***\n");
		saEvtDispatch(evt_handle, SA_DISPATCH_ONE);
		printf("*** end dispatch ***\n");
	}

	/*retention time */
	saEvtEventSubscribe(channel_handle, &filter_array, 2);
	for(;;){
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
		if((select_ret == -1) || (select_ret == 0)){
			printf("select error!\n");
			return 1;
		}else{
			printf("***received event within retetion time***\n");
			saEvtDispatch(evt_handle, SA_DISPATCH_ONE);
		}
	}

	/*event free */
	saEvtEventFree(event_handle);
	saEvtEventFree(event_handle1);

	/*channel close */
	saEvtChannelClose(channel_handle);
	saEvtChannelClose(channel_handle1);

	/*finalize */
	saEvtFinalize(evt_handle);
	return 0;
}
