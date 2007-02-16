/* 
 * fun9node1.c: Funtion Test Case 9 for Event Service Test
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

static int nCmpResult=1, nTimes=0;

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{
	nTimes++;
	/*syslog (LOG_INFO|LOG_LOCAL7, "\n$$$$node 1 :sub: %d, Event: %d$$$$$\n",(int)sub_id,eventDataSize); */
	switch(sub_id)
	{
		case 1: 
			if(eventDataSize!=1) nCmpResult=-1;
			break;
		case 2: 
			if(eventDataSize!=2) nCmpResult=-1;
			break;
		case 3: 
			if(eventDataSize!=2 && eventDataSize!=3 && eventDataSize!=5) nCmpResult=-1;
			break;
		case 4: 
			if(eventDataSize!=4 && eventDataSize!=6) nCmpResult=-1;
			break;
		default: 
			nCmpResult=-1;
			break;	
	}
	saEvtEventFree(event_handle);
	return;
}

int main(int argc, char **argv)
{
	int count =0, i;	

	SaEvtChannelHandleT channel_handle[4];	

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
	for(i=0;i<4;i++){		
		ch_name.length = i+1;
		memcpy(ch_name.value, "99999", i+1);
		if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle[i])
				!= SA_OK){
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
	}
	
	/*count =0 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		

	/*step 2 */
	/*subscribe filter1~4 on channel 1 node 1 */
	/*setmode() will set the 6 event patterns and 4 filter arrays, see in func.h */
	if(setmode()<0){
		for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	for(i=0;i<4;i++){
		if(saEvtEventSubscribe(channel_handle[0], &filter_mode[i], i+1)
					!= SA_OK){
			for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
			freemode();
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
	}

	/*count=1 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	
	/*count=2 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*step 5 */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	tv.tv_sec=2;
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if((select_ret == -1) || (select_ret == 0))
		{
			printf("select error!\n");
			for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
			freemode();
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}else{
			sleep(1);
			if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK || nCmpResult<0 ||nTimes!=7 )
			{
				for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
				freemode();
				saEvtFinalize(evt_handle);
				syslog (LOG_INFO|LOG_LOCAL7, "%s, %d, %d \n", Fail_message, nCmpResult,nTimes) ;
				return -1;
			}
		}

	/*count=3 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*step 6 : unsubscribe filter 1,3 on channel 1 */
	if(saEvtEventUnsubscribe(channel_handle[0], 1)!= SA_OK){
		for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
		freemode();
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	if(saEvtEventUnsubscribe(channel_handle[0], 3)!= SA_OK){
		for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
		freemode();
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/*count=4 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*step 8 */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	tv.tv_sec=2;
	select_ret = select(fd + 1, &rset, NULL,NULL, &tv);
	if((select_ret == -1) || (select_ret == 0))
		{
			printf("select error!\n");
			for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
			freemode();
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}else{
			sleep(1);
			/*if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK ) */
			if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK || nCmpResult<0 ||nTimes!=10)
			{
				for(i=0;i<4;i++) saEvtChannelClose(channel_handle[i]);
				freemode();
				saEvtFinalize(evt_handle);
				syslog (LOG_INFO|LOG_LOCAL7, "%s, %d, %d \n", Fail_message, nCmpResult,nTimes) ;
				return -1;
			}
		}
		
	/*count =5 */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;
	
	for(i=0;i<4;i++) 
	{
		if(saEvtChannelClose(channel_handle[i])!=SA_OK){
			freemode();
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
	}
		
	freemode();
	
	/*finalize */
	saEvtFinalize(evt_handle);
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;	
	
	return 0;
	
}



