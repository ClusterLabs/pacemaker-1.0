/* 
 * h.c: Event Service API test case for:SaEvtChannelOpenSync, SaEvtDispatch
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
#include <unistd.h>

#define INVOCATION_BASE  100

SaInvocationT evt_invocation;
SaEvtChannelHandleT channel_handle;
static int nOpenTimes;

/*event data get */
static void callback_event_open(SaInvocationT invocation,const SaEvtChannelHandleT ChannelHandle,SaErrorT error)
{
	/*for Openasync1: Openasync first time without CREAT falg */
	nOpenTimes++;
	if(nOpenTimes==1)
		{
			if(error== SA_ERR_NOT_EXIST){
				printf("Event service opensync(1) success\n");
			}else{
				printf("Event service opensync(1)%d fail\n", error);	
			}
		}
	
	if (invocation != evt_invocation)
		return ;
	
	/*callback_match = 1 ; */
	channel_handle =  ChannelHandle ;
	return ;
}


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
	SaNameT ch_name;
	int select_ret;
	SaSelectionObjectT fd;
	fd_set rset;	
	evt_invocation = INVOCATION_BASE ;
	SaErrorT Evt_Error;
	
	/*initialize */
	version.releaseCode = 'A';
	version.major = 1;
	version.minor = 0;
	callbacks.saEvtChannelOpenCallback = callback_event_open;
	callbacks.saEvtEventDeliverCallback = callback_event_deliver;
	nOpenTimes=0;
	
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		printf("Event service initialize failed\n");
		return 1;
	}

	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);

	/*channel open */
	ch_name.length = 8;
	memcpy(ch_name.value, "opensync", 8);


	/*Openasync1: openasync first time without Creat flag */
	Evt_Error = saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 3);
	if(Evt_Error!=SA_OK){
		printf("Event service opensync(1) fail\n");
	}else{
		/*dispatch, event_data_get(in callback function) */
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
		if((select_ret == -1) || (select_ret == 0)){
			printf("select error!\n");
			return 1;
		}else{
			(saEvtDispatch(evt_handle, SA_DISPATCH_ONE)== SA_OK);	
		}
	}



	Evt_Error = saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 7);
	
	if(Evt_Error== SA_OK){
		printf("Event service opensync(2) success\n");
	}else{
		printf("Event service opensync(2) fail\n");	
	}

	
	
	/*dispatch, event_data_get(in callback function) */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
	if((select_ret == -1) || (select_ret == 0)){
		printf("select error!\n");
		return 1;
	}else{

		
		/*dispatch 1 */
		if(saEvtDispatch(evt_handle, SA_DISPATCH_ONE)== SA_OK){
			printf("Event service dispatch(1) success\n");
			
		}else{
			printf("Event service dispatch(1) fail\n");	
		}
	}

	
	/*channel close */
	saEvtChannelClose(channel_handle);

	/*opensync 2 */
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, -1)== SA_ERR_BAD_FLAGS){
		printf("Event service opensync(3) success\n");
	}else{
		printf("Event service opensync(3) fail\n");	
	}

	/*finalize */
	saEvtFinalize(evt_handle);
	 
	/*dispatch 2 */
	if(saEvtDispatch(evt_handle, SA_DISPATCH_ONE)== SA_ERR_BAD_HANDLE){
		printf("Event service dispatch(2) success\n");			
	}else{
		printf("Event service dispatch(2) fail\n");	
	}
	
	/*opensync 3 */
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 5)== SA_ERR_BAD_HANDLE){
		printf("Event service opensync(4) success\n");
	}else{
		printf("Event service opensync(4) fail\n");	
	}
	
	/*dispatch 3 */
	saEvtInitialize(&evt_handle, &callbacks, &version);
	if(saEvtDispatch(evt_handle, -1)== SA_ERR_BAD_FLAGS){
		printf("Event service dispatch(3) success\n");
	}else{
		printf("Event service dispatch(3) fail\n");	
	}	
	
	saEvtFinalize(evt_handle);

	
	return 0;
}
