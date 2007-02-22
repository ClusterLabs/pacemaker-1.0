/* 
 * set_active.c: data checkpoint API test:saCkptActiveCheckpointSet
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
void ckpt_sync_callback(SaInvocationT invocation, SaErrorT error);
int  set_active_pre_open(void);
int set_null_value(void);
int set_active_after_finalize(void);
int set_active_after_close(void);
int set_active_err_access(void);
int set_active_normal_call(void);


void ckpt_open_callback (SaInvocationT invocation,
		         const SaCkptCheckpointHandleT *checkpointHandle,
		         SaErrorT error)
{
	return ;
}

void ckpt_sync_callback(SaInvocationT invocation, SaErrorT error)
{
	return ;
}

/*
case description :
        Invoke with invalid checkpoint handle or before open.  
returns :
               o : indicate success
              -1: fail
*/
int  set_active_pre_open(void)
{
	checkpoint_handle = -1 ;
	
	ckpt_error = saCkptActiveCheckpointSet (&checkpoint_handle) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0;

}

int set_null_value(void)
{
	ckpt_error = saCkptActiveCheckpointSet (NULL) ;
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
int set_active_after_finalize(void)
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
				           SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE, 
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

	ckpt_error = saCkptActiveCheckpointSet(&checkpoint_handle);
	if (ckpt_error ==SA_OK)
	{
		return -1 ;
	}

	return 0;
	
}

int set_active_after_close(void)
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
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE, 
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
	

	ckpt_error = saCkptActiveCheckpointSet(&checkpoint_handle);
	if (ckpt_error ==SA_OK)
		{
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;
		}

	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}
	
	return 0;
	
}


/*
case description :
        In this case, the checkpoint is not opened in WRITE mode. So the invokation will return SA_ERR_ACCESS error code .
returns :
               o : indicate success
              -1: fail
*/
int set_active_err_access(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

	ckpt_error = saCkptActiveCheckpointSet(&checkpoint_handle);
	if (ckpt_error !=SA_OK)
		{
			saCkptFinalize(&ckpt_handle) ;
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
	
	return 0;
	
}

/*
case description :
        Invocation with correct steps and parameters. 
returns :
               o : indicate success
              -1: fail
*/

int set_active_normal_call(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout,
					&checkpoint_handle);

	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptActiveCheckpointSet(&checkpoint_handle);
	if (ckpt_error !=SA_OK)
		{
			saCkptFinalize(&ckpt_handle) ;
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
	
	return 0;

}

int main(int argc, char* argv[])
{
	char case_name[] = "saCkptActiveCheckpointSet";
	char name_set_active[]=("checkpoint_set_active");
	int case_index ;
	 
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof(name_set_active) ;

	memcpy (ckpt_name.value, name_set_active, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !set_active_pre_open())
		printf ("%s set_active_pre_open %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_active_pre_open %d FAIL\n\n", case_name, case_index++);
	
	if ( !set_null_value())
		printf ("%s set_null_value %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_null_value %d FAIL\n\n", case_name, case_index++);

	if ( !set_active_after_finalize())
		printf ("%s set_active_after_finalize %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_active_after_finalize %d FAIL\n\n", case_name, case_index++);
	
	if ( !set_active_after_close())
		printf ("%s set_active_after_close %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_active_after_close %d FAIL\n\n", case_name, case_index++);

	if ( !set_active_err_access())
		printf ("%s set_active_err_access %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_active_err_access %d FAIL\n\n", case_name, case_index++);

	if ( !set_active_normal_call())
		printf ("%s set_active_normal_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_active_normal_call %d FAIL\n\n", case_name, case_index++);
	
	return 0 ;
	
}
