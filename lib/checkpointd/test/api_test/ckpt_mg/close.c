/* 
 * close.c: data checkpoint API test:saCkptCheckpointClose
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
#include <stdio.h>
#include <unistd.h>
#include "ckpt_test.h"

void ckpt_open_callback (SaInvocationT invocation,
			const SaCkptCheckpointHandleT *checkpointHandle,
			SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int close_pre_init(void);
int close_null_handle(void);
int close_after_finalize(void);
int close_after_close(void);
int close_normal_call (void);
int close_after_unlink(void);


void ckpt_open_callback (SaInvocationT invocation,
			const SaCkptCheckpointHandleT *checkpointHandle,
			SaErrorT error)
{
	return ;
}

void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error)
{
	return ;
}

/*
case description :
        Invoke with invalid library handle or before initialization.  
returns :
               o : indicate success
              -1: fail
*/
int close_pre_init(void)
{
	checkpoint_handle = -1 ;
	ckpt_error = saCkptCheckpointClose (&checkpoint_handle) ;
	if ( ckpt_error == SA_OK)
		return -1 ;	
	return 0;
}

int close_null_handle(void)
{	
	ckpt_error = saCkptCheckpointClose (NULL) ;
	if ( ckpt_error == SA_OK)
		return -1 ;	
	return 0 ;
}

/*
case description :
        Invoke with old  library handle after finalization or after successfully close invoking.

returns :
               o : indicate success
              -1: fail
*/
int close_after_finalize(void)
{
	ckpt_error = saCkptInitialize ( & ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) == SA_OK)
		return -1;

	return 0;
}

int close_after_close(void)
{
	ckpt_error = saCkptInitialize ( & ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
	  	{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) == SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	

	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}

	return 0;
}



/*

description: 
		This function tests whether "close" with correct parameter succeeds or not. In this case , no existing process is

		associated with the checkpoint. 

returns :
               o : indicate success
              -1: fail
*/
int close_normal_call (void)
{
	ckpt_error = saCkptInitialize ( & ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}
	
	return 0 ;
}



/*
description: 
		This function tests whether close can succeed after unlink invoking. 
returns :
               o : indicate success
              -1: fail
*/

int close_after_unlink(void)
{
	ckpt_error = saCkptInitialize ( & ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
		{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointUnlink(&ckpt_handle, &ckpt_name);
	if(ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}
	
	return 0 ;
}

int  main(void)
{
	char case_name[] = "saCkptCheckpointClose";
	char name_close[]="checkpoint_close";
	int case_index ;
	 
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_close) ;

	memcpy (ckpt_name.value, name_close, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !close_pre_init())
		printf ("%s close_pre_init %d OK\n\n", case_name, case_index++);
	else
		printf ("%s close_pre_init %d FAIL\n\n", case_name, case_index++);

	if ( !close_after_finalize())
		printf ("%s close_after_finalize %d OK\n\n", case_name, case_index++);
	else
		printf ("%s close_after_finaliz %d FAIL\n\n", case_name, case_index++);

	if ( !close_normal_call())
		printf ("%s close_normal_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s close_normal_call %d FAIL\n\n", case_name, case_index++);

	if ( !close_after_unlink())
		printf ("%s close_after_unlink %d OK\n\n", case_name, case_index++);
	else
		printf ("%s close_after_unlink %d FAIL\n\n", case_name, case_index++);

	return 0 ;

}

