/* 
 * open1node2.c: Test data checkpoint function : saCkptCheckpointOpen 
 *
 * Copyright (C) 2003 Wilna Wei <willna.wei@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <saf/ais.h>
#include <stdlib.h>
#include "ckpt_test.h"
#include <stdio.h>
#include <sys/time.h>


SaCkptCheckpointHandleT cphandle =-1 ;
SaCkptHandleT libhandle =-1 ;

void finalize(void);
void termhandler (int signumber);
void usrhandler (int signumber);
int opensync (void);
void initparam(void);

/*
 *Description: 
 *   Finalization related work: close debug socket, close checkpoint service
 */
void finalize(void)
{
	if (cphandle != -1)
		{
			saCkptCheckpointClose (&cphandle) ;
		}
	cphandle = -1 ;
	
	if (libhandle != -1)
		{
			saCkptFinalize (&libhandle) ;
		}
	libhandle = -1 ;
	
	return ;
}

/* 
 * Description: 
 * 		This hanlder is for exception use. When monitor machine sends SIGTERM 
 * signal to node, node app should close the checkpoint service it used. 
 * AIS specifies that checkpoint daemon should do this for process when process 
 * exits. However, we add this handler. 
 */

void termhandler (int signumber)
{
	finalize () ;
	exit (0) ;
}

/*
 *Description:
 *	 for SIGUSR1 handler. Wait and Go mechanism. Nothing will done here.
 *
 */

void usrhandler (int signumber)
{
/*	signal (SIGUSR1, usrhandler) ; */
	return ;
}

/*
 function description: 
	initailize checkpiont parameters for library "init" and "open ".
 returns :
 	none 
*/

void initparam(void)
{
	
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE;

	ckpt_callback.saCkptCheckpointOpenCallback = NULL ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = NULL ;
	ckpt_invocation = INVOCATION_BASE ;
	ckpt_name.length = sizeof (name) ;
	
	memcpy (ckpt_name.value, name, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = 0;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
}

/*
 * Description: 
 * 		This function sets up active replica for active node (node 2).
 * 	After setup finishes, we should check whether creation really effects.
 * 
 * returns : 
 * 		0: success
 * 		-1 : fail
 */
int opensync (void)
{
	initparam () ;
	/* library initialize */
	if ( saCkptInitialize (&libhandle, &ckpt_callback, 
				&ckpt_version) != SA_OK)
	return -1 ;

	/* create the checkpoint */
	ckpt_error = saCkptCheckpointOpen (&libhandle, &ckpt_name, &ckpt_create_attri, 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, &cphandle) ;
	if (ckpt_error != SA_OK)
		return -1 ;

	return 0 ;

}

int main(int argc, char **argv)
{
	int count =0 ;	
	cphandle = libhandle = -1 ;
	

	/* setup SIGTERM handler for exception */
	if ( signal (SIGTERM, termhandler) == SIG_ERR)
		{
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1;
		}
	
	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_start %d\n", getpid()) ;

 	/* setup synchronous signal handler for nodes sychronization */
   	 if ( signal (SIGUSR1, usrhandler) == SIG_ERR)
		{
			finalize () ;
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1;
		}
   
	/* create local replica for active node */
	if (opensync () != 0)
		{
			finalize () ;
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1 ;
		}
	
	/* active node ready, wait for slave node (node 1) action*/
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_signal %d %d\n", count++, SIGUSR1) ;
	pause () ;
 
	finalize () ;
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_success\n") ;
	return 0 ; 

}



