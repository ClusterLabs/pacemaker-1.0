/* 
 * i.c:Event Service API test case for:
 * saEvtEventSubscribe, saEvtEventUnsubscribe
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
	SaEvtChannelHandleT channel_handle;
	SaNameT ch_name;
	SaEvtEventFilterArrayT filter_array;
	
	/*initialize */
	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		printf("Event service initialize failed\n");
		return 1;
	}

 
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

	/*subscribe normally */
	if(saEvtEventSubscribe(channel_handle, &filter_array, 1)== SA_OK){
		printf("Event service subscribe(1) success\n");
	}else{
		printf("Event service subscribe(1) fail\n");
	}
	
	if(saEvtEventSubscribe(channel_handle, &filter_array, 1)== SA_ERR_EXIST){
		printf("Event service subscribe(2) success\n");
	}else{
		printf("Event service subscribe(2) fail\n");
	}
	
	/*no such subscribe id */
	if(saEvtEventUnsubscribe(channel_handle, 2)== SA_ERR_NAME_NOT_FOUND){
		printf("Event service unsubscribe(1) success\n");
	}else{
		printf("Event service unsubscribe(1) fail\n");
	}
	
	/*unsubscribe normally */
	if(saEvtEventUnsubscribe(channel_handle, 1)== SA_OK){
		printf("Event service unsubscribe(2) success\n");
	}else{
		printf("Event service unsubscribe(2) fail\n");
	}
	
	/*after unsbscribe, subscribe with the same id */
	if(saEvtEventSubscribe(channel_handle, &filter_array, 1)== SA_OK){
		printf("Event service unsubscribe(3) success\n");
	}else{
		printf("Event service unsubscribe(3) fail\n");
	}
	saEvtEventUnsubscribe(channel_handle, 1);
	
	/*invalid filter */
	if(saEvtEventSubscribe(channel_handle, NULL , 2)== SA_ERR_INVALID_PARAM){
		printf("Event service subscribe(3) success\n");
	}else{
		printf("Event service subscribe(3) fail\n");
	}

	saEvtChannelClose(channel_handle);
	
	/*invalid channal handle */
	if(saEvtEventSubscribe(channel_handle, &filter_array, 1)== SA_ERR_BAD_HANDLE){
		printf("Event service subscribe(4) success\n");
	}else{
		printf("Event service subscribe(4) fail\n");
	}
	
	
	/*invalid channal handle */
	if(saEvtEventUnsubscribe(channel_handle, 1)== SA_ERR_BAD_HANDLE){
		printf("Event service unsubscribe(4) success\n");
	}else{
		printf("Event service unsubscribe(4) fail\n");
	}

	/*not open for subscribe */
	if(saEvtChannelOpen(evt_handle, &ch_name, 5, 1000000, &channel_handle)
			!= SA_OK){
		printf("Event channel open2 failed\n");
		return 1;
	}
	if(saEvtEventSubscribe(channel_handle, &filter_array, 1)== SA_ERR_INVALID_PARAM){
		printf("Event service subscribe(5) success\n");
	}else{
		printf("Event service subscribe(5) fail\n");
	}
	
	saEvtChannelClose(channel_handle);
		
	/*finalize */
	saEvtFinalize(evt_handle);
	return 0;
}
