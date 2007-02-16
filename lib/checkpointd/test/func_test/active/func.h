/* 
 * func.h: Test data checkpoint head file: saCkptActiveCheckpointSet 
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

#define PIDNUMS 1 
#define NAMEPREFIX "checkpoint name:"

SaCkptCheckpointHandleT cphandle = -1;
SaCkptHandleT libhandle =-1 ;

int debugsock= -1 ;
pid_t pidparent ;
pid_t pid[PIDNUMS] ;

void finalize(void);
void termhandler (int signumber);
void usrhandler (int signumber);
void initparam(void);
int inittest(void);
int opensync (void);
int openasync (void);
void opencallback (SaInvocationT invocation,const SaCkptCheckpointHandleT *checkpointHandle,	SaErrorT error);
int checkreplica (char *activenode);
int checkpid (int pid);

/*
 *Description: 
 *   Finalization related work: close debug socket, close checkpoint service
 */
void finalize(void)
{
	
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

	return;

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
	ckpt_name.length = sizeof(CaseName);
	memcpy (ckpt_name.value, CaseName, sizeof(CaseName));

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
	
	cphandle = libhandle = -1 ;
	
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

	/*return 1; */
	/* library initialize */
	if ( saCkptInitialize (&libhandle, &ckpt_callback, 
						   &ckpt_version) != SA_OK)
		{
			return -1;
		}
	
	ckpt_error = 
		saCkptCheckpointOpen (&libhandle , &ckpt_name, &ckpt_create_attri, 
			SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 
			open_timeout, &cphandle) ;

	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}

	return  0 ;

}


/*
 * Description: 
 *  	Only one node.This case setup the initial testing model in synchronous 
 * mode. App on node1 or node 2  create one new checkpoint. 
 *  	We compare the checkpoint and crossponding replica list before and 
 * after checkpoint creation. New checkpoint name should exist in 
 * checkpoint list after creation.
 * returns : 
 * 		0: success
 * 		-1 : fail
 */
int openasync (void)
{
	
	int maxfd ;
	fd_set fs ;
	struct timeval tv ;
	SaSelectionObjectT selobj;
	
	maxfd = selobj = 0 ;
	FD_ZERO (&fs) ;
   	libhandle = cphandle = -1 ;
	tv.tv_sec = SEL_TIMEOUT ;
	tv.tv_usec = 0 ;

	initparam () ;

	
	/* library initialize */
	if ( saCkptInitialize (&libhandle, &ckpt_callback, 
						   &ckpt_version) != SA_OK)
		return -1 ;
	
	if ( saCkptSelectionObjectGet (&libhandle, &selobj) != SA_OK)
		{
			return -1;
		}
	
	/* create the checkpoint */
	ckpt_error = saCkptCheckpointOpenAsync(&libhandle, 
						INVOCATION_BASE,
						&ckpt_name,
						&ckpt_create_attri ,
						SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_COLOCATED) ;
									  		
	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}
	
	FD_SET (selobj, &fs) ;
	maxfd = selobj + 1 ;
	
	if (select (maxfd, &fs, NULL, NULL, &tv) == -1)
		{
			return -1 ;
		}

	if (!FD_ISSET (selobj, &fs))
		{
			return -1 ;
		}

	if (saCkptDispatch (&libhandle , SA_DISPATCH_ALL) != SA_OK)
		{
					return -1 ;
		}

	if ( ckpt_error != SA_OK || cphandle == -1)
		{	
			return -1 ; 
		}

	

	return 0 ;

}


/*
 *Description:
 *	Callback function.Only set checkpoint handle and error number.
 */
void opencallback (SaInvocationT invocation,
	  	const SaCkptCheckpointHandleT *checkpointHandle,
		SaErrorT error)
{
		ckpt_error = error ;

		if (invocation != INVOCATION_BASE) 
			{
				ckpt_error = SA_ERR_INVALID_PARAM ;
				return ;
			}
		cphandle = * checkpointHandle ;

}



