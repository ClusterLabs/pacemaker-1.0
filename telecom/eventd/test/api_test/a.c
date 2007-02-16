/* 
 * a.c: Event Service API test case for:
 * saEvtInitialize, saEvtFinalize, saEvtSelectionObjectGet
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
	SaSelectionObjectT select_obj;
	
	/*initialize, version */
	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	if(saEvtInitialize(&evt_handle, &callbacks, &version) == SA_OK){
		printf("Event service initialize(1) success\n");
	}else{
		printf("Event service initialize(1) fail\n");
	}
	if(saEvtFinalize(evt_handle) == SA_OK){
		printf("Event service finalize(1) success\n");
	}else{
		printf("Event service finalize(1) fail\n");
	}

	/*initialize, version */
	version.major = 2;
	if((saEvtInitialize(&evt_handle, &callbacks, &version) 
		== SA_ERR_VERSION) && (version.major == 1)){
		printf("Event service initialize(2) success\n");
	}else{
		printf("Event service initialize(2) fail\n");
	}

	/*finalize */
	if(saEvtFinalize(evt_handle) == SA_ERR_BAD_HANDLE){
		printf("Event service finalize(2) success\n");
	}else{
		printf("Event service finalize(2) fail\n");
	}
	version.releaseCode = 'B';
	if((saEvtInitialize(&evt_handle, &callbacks, &version)
		== SA_ERR_VERSION) && (version.releaseCode == 'A')){
		printf("Event service initialize(3) success\n");
	}else{
		printf("Event service initialize(3) fail\n");
	}
	
	/*selectobjectget */
	if(saEvtSelectionObjectGet(evt_handle, &select_obj) == SA_OK){
		printf("Event service get selection object(1) fail\n");
	}else{
		printf("Event service get selection object(1) success\n");
	}

	/*selectobjectget */
	saEvtInitialize(&evt_handle, &callbacks, &version); 
	if(saEvtSelectionObjectGet(evt_handle, NULL) == SA_ERR_INVALID_PARAM){
		printf("Event service get selection object(2) success\n");
	}else{
		printf("Event service get selection object(2) fail\n");
	}
	if(saEvtSelectionObjectGet(evt_handle, &select_obj) == SA_OK){
		printf("Event service get selection object(3) success\n");
	}else{
		printf("Event service get selection object(3) fail\n");
	}
	saEvtFinalize(evt_handle);
	
	
	
	return 0;
}
