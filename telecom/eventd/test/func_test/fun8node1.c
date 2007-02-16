/* 
 * fun8node1.c: Funtion Test Case 8 for Event Service Test
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


#define EVENT_DATA  "Priority Function Test Case event"
#define Event_retentionTime 10000
#define PublishName "f8node1"
#define Pattern1 "func08"

/*event data get */
static void callback_event_deliver(SaEvtSubscriptionIdT sub_id,
				SaEvtEventHandleT event_handle,
				const SaSizeT eventDataSize)
{

	saEvtEventFree(event_handle);
	return;
}

int main(int argc, char **argv)
{
	int count =0,i;	

	SaEvtChannelHandleT channel_handle1;	

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
	
	
	/*Step 1. */
	ch_name.length = sizeof("fun08");
	memcpy(ch_name.value, "fun08", sizeof("fun08"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle1)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		

	
	/*step 2: publish event 1 on node 1 */
	/*event allocate */
	if(saEvtEventAllocate(channel_handle1, &event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = sizeof(Pattern1);
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(sizeof(Pattern1));
	memcpy(pattern_array.patterns[0].pattern, Pattern1, sizeof(Pattern1));
	publisher_name.length = sizeof(PublishName);
	memcpy(publisher_name.value, PublishName, sizeof(PublishName));
	data_size = sizeof(EVENT_DATA);
	event_data = g_malloc0(data_size);
	memcpy(event_data, EVENT_DATA, data_size);

	/*publish */
	for (i=3;i>-1;i--)
	{
		if(saEvtEventAttributesSet(event_handle, &pattern_array,i, Event_retentionTime, &publisher_name)!= SA_OK)
		{
			saEvtChannelClose(channel_handle1);
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
		if(saEvtEventPublish(event_handle, event_data, data_size, &event_id)!= SA_OK)
		{
			saEvtChannelClose(channel_handle1);
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}
	}
	
	/*event free */
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	free(event_data);
	if(saEvtEventFree(event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle1);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	

	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	


	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	
	
	if(saEvtChannelClose(channel_handle1) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;			

	/*finalize */
	saEvtFinalize(evt_handle);
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;	
	
	return 0;

	
}




