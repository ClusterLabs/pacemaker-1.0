/* $Id: func.h,v 1.1 2004/06/30 03:44:20 deng.pan Exp $ */
/* 
 * func.h: Head File for data checkpoint Test : saCkptSectionDelete 
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

#define PIDNUMS 1 
#define NAMEPREFIX "checkpoint name:"

SaCkptCheckpointHandleT cphandle = -1;
SaCkptHandleT libhandle =-1 ;


SaCkptSectionCreationAttributesT sectattri ;
SaCkptSectionIdT sectid ;

/* flag indicates section created or not */
int createflag = 0 ;


void finalize(void);
void finalize1(void);
void termhandler (int signumber);
void usrhandler (int signumber);
void initparam(void);
int inittest(void);
void initparam(void);
int createsect (void);
int opensync (void);
int deletesect(void);

/*
 *Description: 
 *   Finalization related work: close debug socket, close checkpoint service
 */
void finalize(void)
{
	if (createflag)
		{
			saCkptSectionDelete(&cphandle,&sectid );
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
	if (createflag)
		{
			saCkptSectionDelete(&cphandle,&sectid );
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
//	signal (SIGUSR1, usrhandler) ;
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
	
	//memcpy (ckpt_name.value, name, ckpt_name.length);
	
	memcpy (ckpt_name.value, CaseName, sizeof(CaseName));

//	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ;
//	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.creationFlags = CkptCreationFlag;
	ckpt_create_attri.retentionDuration = 0;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	sectid.id = sect_id_array ;
	sectid.idLen = sizeof (sect_id_array) ;

	sectattri.sectionId = &sectid ;
	time(&cur_time) ;
	sectattri.expirationTime = (SaTimeT)((cur_time + 3600*24)*1000000000LL);
	

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


/*Description:
 *	Create one section in local  replica. Check local status.
 *Returns:
 *  -1 : fail 
 *  0  : success
 */
int createsect (void)
{
	ckpt_error = saCkptSectionCreate (&cphandle, &sectattri ,  (void *)init_data, sizeof(init_data)) ;
	if (ckpt_error != SA_OK)
		return -1 ;
	createflag = 1 ;
	return 0;	
}

/* Description: 
 * 	Delete "remote" section.
 * Returns :
 * 	fail : -1 ;
 * 	success : 0
 */
int deletesect(void)
{

	if (saCkptSectionDelete(&cphandle,&sectid ) != SA_OK)
		return -1 ;
	return 0; 
}

