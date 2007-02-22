/* 
 * func.h: Test Head file for data checkpoint : saCkptCheckpointClose 
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
#include <sys/wait.h>
#include <string.h>
#include "ckpt_test.h"
#include <stdio.h>
#include <sys/time.h>

#define PIDNUMS 3 
#define NAMEPREFIX "checkpoint name:"

SaCkptCheckpointHandleT cphandle = -1;
SaCkptHandleT libhandle =-1 ;


pid_t pidparent ;
pid_t pid[PIDNUMS] ;


void finalize(void);
void finalize1(void);
void termhandler (int signumber);
void usrhandler (int signumber);
void initparam(void);
int inittest(void);
int opensync (void);

/*
 *Description: 
 *   Finalization related work: close debug socket, close checkpoint service
 */
void finalize(void)
{
	int i ;
	for (i =0 ; i < PIDNUMS ; i++)
		{
			if (pid[i] > 0)
				kill (pid[i], SIGTERM) ;
		}

	if (cphandle > 0)
		{
			saCkptCheckpointClose (&cphandle) ;
		}
	cphandle = -1 ;

		
	if ( libhandle > 0)
		{
			saCkptFinalize (&libhandle) ;
		}
	libhandle = -1 ;
	

}

/*
 *Description: 
 *   Finalization related work: close debug socket, close checkpoint service
 *   Finalize1 use to node1 final without finalize checkpoint
 */
void finalize1(void)
{
	int i ;
	for (i =0 ; i < PIDNUMS ; i++)
		{
			if (pid[i] > 0)
				kill (pid[i], SIGTERM) ;
		}

	if (cphandle > 0)
		{
			saCkptCheckpointClose (&cphandle) ;
		}
	cphandle = -1 ;



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
	
	if (getpid () == pidparent)	
		finalize () ;

	else if (cphandle > 0)
		{
			saCkptCheckpointClose (&cphandle) ;
			saCkptFinalize (&libhandle) ;
		}

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
	ckpt_name.length = sizeof(CaseName);
	memcpy (ckpt_name.value, CaseName, sizeof(CaseName));

/*	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ; */
/*	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ; */
	ckpt_create_attri.creationFlags = CkptCreationFlag;
	ckpt_create_attri.retentionDuration = 0;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	

}

/*
 *Description: 
 *	Make some prepation work for test case.For example, sigal handler seting up, socket
 *creating, etc. 
 *Returns :
 * 	0 : success
 * -1 : failure
 */
 
int inittest(void)
{
	int i;
	
	cphandle = libhandle = -1 ;
	
	pidparent = getpid () ;
	for (i=0; i < PIDNUMS ; i++)
		{
			pid[i] = -1 ;
		}
	/* setup SIGTERM handler for exception */
	if ( signal (SIGTERM, termhandler) == SIG_ERR)
		{
			return -1;
		}

   
	
	/* setup synchronous signal handler for nodes sychronization */
	if ( signal (SIGUSR1, usrhandler) == SIG_ERR)
		{
			return -1;
		}
	initparam () ;
	return 0 ;
}


/*
 * Description: 
 * 		Create the local replica with "colocated " flag.
 * returns : 
 * 		0: success
 * 		-1 : fail
 */
int opensync (void)
{
	
	libhandle = cphandle = -1 ;	
	/* library initialize */
	if ( (ckpt_error=saCkptInitialize (&libhandle, &ckpt_callback, 
					&ckpt_version) )!= SA_OK)
		{
			return -1 ;
		}
	
	ckpt_error = 
		saCkptCheckpointOpen (&libhandle, &ckpt_name, &ckpt_create_attri, 
					CkptOpenFlag, open_timeout, &cphandle) ;

	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}
	
	return 0 ; 

}

