/* 
 * asynchronize.c: Test data checkpoint API : saCkptCheckpointSynchronizeAsync
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <saf/ais.h>
#include <time.h>
#include "ckpt_test.h"

#define INVOCATION 1024
void ckpt_open_callback (SaInvocationT invocation,
			 const SaCkptCheckpointHandleT *checkpointHandle,
			 SaErrorT error);
void ckpt_async_callback (SaInvocationT invocation, SaErrorT error);
int async_pre_open(void);
int async_null_handle(void);
int async_after_close(void);
int async_after_finalize(void);
int async_err_access(void);
int async_normal_call(void);

void ckpt_open_callback (SaInvocationT invocation,
			 const SaCkptCheckpointHandleT *checkpointHandle,
			 SaErrorT error)
{
	return ;
}

void ckpt_async_callback (SaInvocationT invocation, SaErrorT error)
{
		ckpt_error = error ;
		if (invocation != INVOCATION  && (ckpt_error == SA_OK))
			{
				ckpt_error = SA_ERR_INVALID_PARAM ;
			}
		return ;
}

/*
case description :
        Invoke with invalid checkpoint handle or before open.  
returns :
               o : indicate success
              -1: fail
*/
int async_pre_open(void)
{
	checkpoint_handle = -1 ;
	ckpt_error  = saCkptCheckpointSynchronizeAsync (&ckpt_handle , INVOCATION, & checkpoint_handle) ;

	if ( ckpt_error == SA_OK)
		return -1 ;

	return 0;
}

int async_null_handle(void)
{
	ckpt_error = saCkptCheckpointSynchronizeAsync (&ckpt_handle , INVOCATION, NULL) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	return 0 ;
}


/*
case description :
        Invoke with invalid checkpoint handle. Two cases will be included:
        1. after close operation

        2. after finalization operation

        

returns :

               o : indicate success
              -1: fail
*/
int async_after_close(void)
{
	ckpt_error = saCkptInitialize(&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                        &ckpt_name,  
                                        &ckpt_create_attri , 
                                        SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 
				   	open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error  = saCkptCheckpointSynchronizeAsync (&ckpt_handle , INVOCATION, & checkpoint_handle) ;

	if( ckpt_error == SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* sychronize after finalization */
	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	 
	return 0;
}

int async_after_finalize(void)
{
	ckpt_error = saCkptInitialize(&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 
					   open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* sychronize after finalization */
	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	 
	ckpt_error  = saCkptCheckpointSynchronizeAsync (&ckpt_handle , INVOCATION, & checkpoint_handle) ;
	if( ckpt_error == SA_OK)
		return -1;
	return 0;
}

/*
case description :
        Checkpoint not opened in write mode. SA_ERR_ACCESS will be returned. 

returns :

               o : indicate success
              -1: fail
*/
int async_err_access(void)
{	
	fd_set  fs ;
	int maxfd ;
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                        &ckpt_name,  
                                        &ckpt_create_attri , SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_READ, 
					open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error=saCkptCheckpointSynchronizeAsync (&ckpt_handle , INVOCATION, & checkpoint_handle) ;
	if (ckpt_error != SA_OK)
		{
				saCkptCheckpointClose (&checkpoint_handle) ;
				saCkptFinalize (& ckpt_handle) ;
				return -1 ;
		}

	FD_ZERO (&fs);
	FD_SET (ckpt_select_obj, &fs) ;
	maxfd = ckpt_select_obj + 1 ;
	
	if ( select (maxfd, &fs, NULL, NULL, NULL ) == -1)
		{			
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;		
		}

	if ( !FD_ISSET (ckpt_select_obj, &fs))
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;	
			return -1 ;
		}

	if ( saCkptDispatch (&ckpt_handle , SA_DISPATCH_ALL) != SA_OK)		
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}	

	if ( ckpt_error != SA_OK )  /* error  */
		{			
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
	{
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}

	if ((ckpt_error=saCkptFinalize(&ckpt_handle)) != SA_OK )
	{
		return -1 ;
	}
	return 0 ;
}



/*
case description :
        Invocation with correct steps and parameters. SA_OK will be returned.

returns :

               o : indicate success
              -1: fail
*/
int async_normal_call(void)
{

	fd_set  fs ;
	int maxfd ;


	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

	ckpt_error = saCkptSelectionObjectGet (&ckpt_handle, &ckpt_select_obj);
	if (ckpt_error != SA_OK)
		{		
			saCkptFinalize (& ckpt_handle) ;	
			return -1;	
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                           		&ckpt_name, 
					&ckpt_create_attri , 
        	                     	SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
				  	open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptCheckpointSynchronizeAsync(&ckpt_handle , INVOCATION, & checkpoint_handle) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	FD_ZERO (&fs);
	FD_SET (ckpt_select_obj, &fs) ;
	maxfd = ckpt_select_obj + 1 ;
	
	if ( select (maxfd, &fs, NULL, NULL, NULL ) == -1)
		{			
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;		
		}

	if ( !FD_ISSET (ckpt_select_obj, &fs))
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;	
			return -1 ;
		}

	if ( saCkptDispatch (&ckpt_handle , SA_DISPATCH_ALL) != SA_OK)		
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}	

	if ( ckpt_error != SA_OK )  /* error  */
		{			
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;

	return 0 ;
}



int main(int argc, char* argv[])
{
	char name_async[]="checkpoint_async";
	char case_name[] = "saCkptCheckpointSynchronizeAsync";
	int case_index ;
		
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_async_callback;

	ckpt_name.length = sizeof (name_async) ;

	memcpy (ckpt_name.value, name_async, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	case_index = 0 ;

	if ( !async_pre_open())
		printf ("%s async_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s async_pre_open %d FAIL\n", case_name, case_index++);

	if ( !async_null_handle())
		printf ("%s async_null_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s async_null_handle %d FAIL\n", case_name, case_index++);

	if ( !async_after_close())
		printf ("%s async_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s async_after_close %d FAIL\n", case_name, case_index++);

	if ( !async_after_finalize())
		printf ("%s async_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s async_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !async_normal_call())
		printf ("%s async_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s async_normal_call %d FAIL\n", case_name, case_index++);

	if ( !async_err_access())
		printf ("%s async_err_access %d OK\n", case_name, case_index++);
	else
		printf ("%s async_err_access %d FAIL\n", case_name, case_index++);

	return 0 ;

}
