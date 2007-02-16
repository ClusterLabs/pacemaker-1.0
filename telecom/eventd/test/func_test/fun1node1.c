/* 
 * fun1node1.c: Funtion Test Case 1 for Event Service Test
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

int main(int argc, char **argv)
{
	int count =0,i ;	

	SaEvtChannelHandleT channel_handle[5];	

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
		
	/*Step 1. */
	ch_name.length = sizeof("fun01");
	memcpy(ch_name.value, "fun01", sizeof("fun01"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle[0])
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle[1])
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	 	
	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;
	
	/*step 3 */
	ch_name.length = sizeof("fun011");
	memcpy(ch_name.value, "fun011", sizeof("fun011"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 7, 1000000, &channel_handle[2])
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	ch_name.length = sizeof("fun012");
	memcpy(ch_name.value, "fun012", sizeof("fun012"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 6, 1000000, &channel_handle[3])
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	ch_name.length = sizeof("fun013");
	memcpy(ch_name.value, "fun013", sizeof("fun013"));
	
	if(saEvtChannelOpen(evt_handle, &ch_name, 5, 1000000, &channel_handle[4])
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
	
	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;
	
	/*5. close channels on node1 */
	for(i=0; i<5; i++){
		if(saEvtChannelClose(channel_handle[i]) != SA_OK){
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}	
	}	

	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;			

	/*finalize */
	saEvtFinalize(evt_handle);
	
	syslog (LOG_INFO|LOG_LOCAL7, "%s", Success_message) ;
	

	return 0 ; 
}



