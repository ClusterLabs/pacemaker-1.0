/* 
 * open2node1.c: Test data checkpoint function : saCkptCheckpointOpen 
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

#define PIDNUMS 0 

SaCkptCheckpointHandleT cphandle = -1;
SaCkptHandleT libhandle =-1 ;

pid_t pidparent ;
pid_t pid[PIDNUMS] ;

void finalize(void);
void termhandler (int signumber);
void usrhandler (int signumber);
void initparam(void);
int  inittest(void);
int opensync(int flag);

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
	
	if (libhandle > 0)
		{
			saCkptFinalize (&libhandle) ;
		}
	libhandle = -1 ;
	
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
	saCkptCheckpointClose (&cphandle) ;

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
 *		Test environment: node 2(active node) has been set up already. Node 1
 * repeats creating local replica in synchronous mode (two times). 
 *	  Two puroses:
 *	  1. colocated flag test when recalling open function.
 *	  2. pid list test
 *	  If pid list does't miss any local pid and replica list remains,
 * test succeeds; otherwhile, it fails.
 * 
 * returns : 
 * 		0: success
 * 		-1 : fail
 */
int opensync (int flag)
{
	if (flag < 0) /*parent creation */
	{
		/* library initialize */
		if ( saCkptInitialize (&libhandle, &ckpt_callback, 
							   &ckpt_version) != SA_OK)
			return -1 ;
	
		/* create the checkpoint local replica */
		ckpt_error = 
			saCkptCheckpointOpen (&libhandle, 
						&ckpt_name, 
						&ckpt_create_attri,
						SA_CKPT_CHECKPOINT_WRITE, 
						open_timeout, &cphandle) ;
	}
	else{  /*child process open */
		ckpt_error = 
			saCkptCheckpointOpen (&libhandle, 
						&ckpt_name, 
						&ckpt_create_attri,
						SA_CKPT_CHECKPOINT_WRITE, 
						open_timeout, &cphandle) ;
	}
	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}
	
	return 0;	
	
}

int main(int argc, char **argv)
{
	int count =0 ;	
	
	if (inittest () != 0)
		{
			finalize () ;
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1 ;
		}

	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_start %d\n", pidparent) ;
  
	/* wait for node 2 checkpoit open ready */
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_signal %d %d\n", count++, SIGUSR1) ;
	pause () ;
	
	/* create local replica for slave node */
	if (opensync (-1) < 0)
		{
			finalize () ;
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1 ;
		}
	
	/* wait for node 2 processing replica check */
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_signal %d %d\n", count++, SIGUSR1) ;
	pause () ;
	
	finalize () ;

	return 0 ; 
}



