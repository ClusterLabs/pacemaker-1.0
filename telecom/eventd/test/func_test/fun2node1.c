/* $Id: fun2node1.c,v 1.2 2004/10/09 01:49:43 lge Exp $ */
/* 
 * fun2node1.c: Funtion Test Case 2 for Event Service Test
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

static int nOpenTimes=0;
static SaEvtChannelHandleT channel_handle[5];	

static void callback_event_open(SaInvocationT invocation,
	const SaEvtChannelHandleT ChannelHandle,SaErrorT error)
{	
	if(error== SA_OK && invocation== evt_invocation){
		channel_handle[nOpenTimes]=ChannelHandle;
		nOpenTimes++;
		return;
	}else{
		saEvtChannelClose(ChannelHandle);
		saEvtFinalize(evt_handle);
		return ;
	}
}


int main(int argc, char **argv)
{	
   	int count =0,i ;	

	if (inittest () != 0)
	{
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return 1;
	}	
	
	/*initialize */
	callbacks.saEvtChannelOpenCallback = callback_event_open;
	if(saEvtInitialize(&evt_handle, &callbacks, &version) != SA_OK){
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return 1;
	}

	/*get selection object */
	saEvtSelectionObjectGet(evt_handle, &fd);
	
	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d\n", Start_message, getpid ()) ;
		
	/*Step 1. */
	ch_name.length = sizeof("fun02");
	memcpy(ch_name.value, "fun02", sizeof("fun02"));
	
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 7)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 7)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}	
 	
	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;
	
	/*Step 3. */
	while(nOpenTimes<2)
	{
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
		if((select_ret == -1) || (select_ret == 0)){
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}else{
			if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK)
			{			
				saEvtFinalize(evt_handle);
				syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
				return -1;
			}
	
		}
	}
	
	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;
	
	
	/*step 5 */
	ch_name.length = sizeof("fun021");
	memcpy(ch_name.value, "fun021", sizeof("fun021"));
	
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 7)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	ch_name.length = sizeof("fun022");
	memcpy(ch_name.value, "fun022", sizeof("fun022"));
	
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 7)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	ch_name.length = sizeof("fun023");
	memcpy(ch_name.value, "fun023", sizeof("fun023"));
	
	if(saEvtChannelOpenAsync(evt_handle,evt_invocation , &ch_name, 7)
			!= SA_OK){
		saEvtFinalize(evt_handle);
		syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
		return -1;
	}
	
	
	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;
	
	/*Step 3. */
	while(nOpenTimes<5)
	{
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
		if((select_ret == -1) || (select_ret == 0)){
			saEvtFinalize(evt_handle);
			syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
			return -1;
		}else{
			if(saEvtDispatch(evt_handle, SA_DISPATCH_ALL)!= SA_OK)
			{			
				saEvtFinalize(evt_handle);
				syslog (LOG_INFO|LOG_LOCAL7, "%s \n", Fail_message) ;
				return -1;
			}	
		}

	}
		
	/* wait for node 2  ready */
	syslog (LOG_INFO|LOG_LOCAL7, "%s %d %d\n",Signal_message, count++, SIGUSR1) ;
	pausepause () ;		
	
	/*9. close channels on node1 */
	for(i=0; i<5; i++){
		if((Evt_Error=saEvtChannelClose(channel_handle[i]))!= SA_OK ){
		
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



