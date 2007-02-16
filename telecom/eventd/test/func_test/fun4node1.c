/* 
 * fun4node1.c: Funtion Test Case 4 for Event Service Test
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

#define EVENT_DATA  "Allocate Function Test Case node1"
#define Event_Priority 1
#define Event_retentionTime 1000000

int main(int argc, char **argv)
{
	int count =0,i;	
	SaEvtChannelHandleT channel_handle;	

	if (inittest () != 0)
	{
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	
	/*initialize */
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d\n", Start_message, getpid ()) ;	
	
	ch_name.length = sizeof("fun04");
	memcpy(ch_name.value, "fun04", sizeof("fun04"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	

	/*step 1:event allocate */
	if(saEvtEventAllocate(channel_handle, &event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/* Step3: attributes set */
	pattern_array.patternsNumber = 1;
	pattern_array.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array.patterns[0].patternSize = 6;
	pattern_array.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	memcpy(pattern_array.patterns[0].pattern, "func04", 6);
	publisher_name.length = 7;
	memcpy(publisher_name.value, "f4node1", 7);
	if(saEvtEventAttributesSet(event_handle, &pattern_array, Event_Priority, Event_retentionTime, &publisher_name)!= SA_OK)
	{
		saEvtChannelClose(channel_handle);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/* Step5: attributes get */
	pattern_array_out.patternsNumber = 1;
	pattern_array_out.patterns = (SaEvtEventPatternT *)g_malloc(sizeof(SaEvtEventPatternT));
	pattern_array_out.patterns[0].patternSize = 6;
	pattern_array_out.patterns[0].pattern = (SaUint8T *)g_malloc(6);
	if(saEvtEventAttributesGet(event_handle, &pattern_array_out, &priority, &retention_time, &publisher_name_out, &publish_time, &event_id)
			!= SA_OK)
	{
		saEvtChannelClose(channel_handle);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	if(patterncmp(pattern_array, pattern_array_out)<0)
	{		
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n pattern", Fail_message) ;
		return -1;
	}



	if(retention_time!=Event_retentionTime)
	{		
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n retentiontime", Fail_message) ;
		return -1;
	}

	if(priority!=Event_Priority)
	{		
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n prority", Fail_message) ;
		return -1;
	}

	if(publisher_name.length!=publisher_name_out.length)	
	{		
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n publishnamelength", Fail_message) ;
		return -1;
	}

	for(i=0;i<publisher_name.length;i++)
	{
		if(publisher_name.value[i]!=publisher_name_out.value[i])
		{		
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n publishnamevalue", Fail_message) ;
			return -1;
		}
	}

	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	

	/*Step 7: event free */
	free(pattern_array.patterns[0].pattern);
	free(pattern_array.patterns);
	free(pattern_array_out.patterns[0].pattern);
	free(pattern_array_out.patterns);
	if(saEvtEventFree(event_handle)!= SA_OK)
	{
		saEvtChannelClose(channel_handle);
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	


	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;	
	

	if(saEvtChannelClose(channel_handle) != SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}

	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause();	
	
	/*finalize */
	saEvtFinalize(evt_handle);

	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;	
	
	return 0;

	
}

