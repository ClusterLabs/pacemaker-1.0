/* 
 * d.c: Event Service API test case for:saEvtEventAttributesSet
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

/*#include <clplumbing/cl_signal.h> */
/*#include "event.h" */
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
	SaEvtEventHandleT event_hd;
	SaNameT publisher_name;
	SaEvtEventPatternArrayT pattern_array, pattern_array_out;

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
		printf("Event channel open fail\n");
		return 1;
	}
	
	/*event allocate */
	if(saEvtEventAllocate(channel_handle, &event_hd) != SA_OK){
		printf("Event allocate fail\n");
		return 1;
	}

	/*attributes set */
	publisher_name.length = 7;
	memcpy(publisher_name.value, "forrest", 7); 
	if(saEvtEventAttributesSet(event_hd, NULL, 1, 1000, &publisher_name) ==
		SA_ERR_INVALID_PARAM){
		printf("Event attributes set(1) success\n");
	}else{
		printf("Event attributes set(1) fail\n");
	}

	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 6;
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	memcpy(pattern_array.patterns[0].pattern, "abcxyz", 6);
	
	if(saEvtEventAttributesSet(event_hd, &pattern_array, 1, 1000, NULL) ==
		SA_ERR_INVALID_PARAM){
		printf("Event attributes set(2) success\n");
	}else{
		printf("Event attributes set(2) fail\n");
	}

	if(saEvtEventAttributesSet(event_hd, &pattern_array, 4, 1000, &publisher_name) ==
		SA_ERR_INVALID_PARAM){
		printf("Event attributes set(3) success\n");
	}else{
		printf("Event attributes set(3) fail\n");
	}

	if(saEvtEventAttributesSet(event_hd, &pattern_array, 1, 1000, &publisher_name) ==
		SA_OK){
		printf("Event attributes set(4) success\n");
	}else{
		printf("Event attributes set(4) fail\n");
	}

	/*event attributes get */
	SaUint8T priority_out;
	SaTimeT time_out, publish_out;
	SaNameT name_out;
	SaEvtEventIdT id_out;
	pattern_array_out.patternsNumber = 1;
	pattern_array_out.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array_out.patterns[0].patternSize = 6;
	pattern_array_out.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	
	if(saEvtEventAttributesGet(event_hd, &pattern_array_out, &priority_out,
		 &time_out, &name_out, &publish_out, &id_out) == SA_OK){
		printf("Event attributes get(1) success\n");
	}else{
		printf("Event attributes get(1) fail\n");
	}

	/*event free */
	if(saEvtEventFree(event_hd) != SA_OK){
		printf("Event free fail\n");
		return 1;
	}
	if(saEvtEventAttributesSet(event_hd, &pattern_array, 1, 1000, &publisher_name) ==
		SA_ERR_BAD_HANDLE){
		printf("Event attributes set(5) success\n");
	}else{
		printf("Event attributes set(5) fail\n");
	}

	if(saEvtEventAttributesGet(event_hd, &pattern_array_out, &priority_out,
		 &time_out, &name_out, &publish_out, &id_out) == SA_ERR_BAD_HANDLE){
		printf("Event attributes get(2) success\n");
	}else{
		printf("Event attributes get(2) fail\n");
	}

	/*channel close */
	if(saEvtChannelClose(channel_handle) != SA_OK){
		printf("Event channel close fail\n");
		return 1;
	}

	/*finalize */
	saEvtFinalize(evt_handle);
	return 0;
}
