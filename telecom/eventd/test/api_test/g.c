/* 
 * g.c: Event Service API test case for:SaEvtChannelUnlink
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
	SaUint8T tmp_char[4];
	SaEvtChannelHandleT ch_hd;

	memcpy(&ch_hd, tmp_char, 4);
	
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
	ch_name.length = 6;
	memcpy(ch_name.value, "unlink", 6);
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle)
			!= SA_OK){
		printf("Event service open failed\n");
		return 1;		
	}

	if(saEvtChannelUnlink(evt_handle,&ch_name) != SA_OK){
		printf("Event channel unlink(1) fail\n");
	}else{
		if(saEvtChannelOpen(evt_handle, &ch_name, 3, 1000000, &channel_handle)!= SA_OK)
		{
			printf("Event channel unlink(1) success\n");
		}else{
			printf("Event channel unlink(1) fail\n");
		
		}
	}
	
	if(saEvtChannelUnlink(evt_handle,NULL) != SA_ERR_INVALID_PARAM){
		printf("Event channel unlink(2) fail\n");
	}else{
		printf("Event channel unlink(2) success\n");
	}
	
#if 0
	if(saEvtChannelClose(channel_handle)==SA_OK)
	{
		printf("Event channel close after unlink fail\n");
		/*return 1; */
	}
#endif
	
	/*finalize */
	saEvtFinalize(evt_handle);
	
	if(saEvtChannelUnlink(evt_handle,&ch_name) != SA_ERR_BAD_HANDLE){
		printf("Event channel unlink(3) fail\n");
	}else{
		printf("Event channel unlink(3) success\n");
	}	
	
	return 0;
	
}
