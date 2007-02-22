/* 
 * set_duration.c: data checkpoint API test:saCkptCheckpointRetentionDurationSet
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
int set_duration_pre_open(void);
int set_duration_pre_open(void);
int set_duration_null(void);
int set_duration_after_finalize(void);
int set_duration_after_close(void);
int set_duration_normal_call(void);
int set_duration_timeout(void);

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
        Invoke with invalid checkpoint handle or before open.  

returns :
               o : indicate success
              -1: fail
*/
int set_duration_pre_open()
{

	SaCkptCheckpointHandleT tmp_handle ;
	SaTimeT tmp_time ;

	tmp_time = 0 ;
	tmp_handle = -1;

	ckpt_error = saCkptCheckpointRetentionDurationSet(NULL,  tmp_time);
	if (ckpt_error == SA_OK) 
	       return -1;
	return 0;
}

int set_duration_null()
{
	ckpt_error = saCkptCheckpointRetentionDurationSet(NULL, 1000);
	if(ckpt_error ==SA_OK) 
	       return -1;

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
int set_duration_after_finalize()
{
	SaTimeT tmp_time ;
	tmp_time = 100 ;

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

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}

	ckpt_error = saCkptCheckpointRetentionDurationSet (&checkpoint_handle, tmp_time) ;

	if ( ckpt_error == SA_OK)
			return -1 ;

	return 0 ;

}

int set_duration_after_close()
{
	SaTimeT tmp_time ;
	tmp_time = 100 ;

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

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointRetentionDurationSet (&checkpoint_handle, tmp_time) ;

	if ( ckpt_error == SA_OK)
		{
			saCkptFinalize (&ckpt_handle);
			return -1;
		}
	
	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}
	return 0 ;
}



/*
case description :
        Invoke with correct steps and parameters. After successfully invoking,  we use open function withou colocated

        flag to check whether the duration effects. If duration effects , open will failed after duration timer fires and succeed 

        before duration timer fires. Two kind values will be checked:zero and non-zero.

        In this case , only one process is assiocated with the checkpoint .

       
returns :

               o : indicate success
              -1: fail
*/
int set_duration_normal_call()
{

	SaTimeT tmp_time ;
	tmp_time = 0 ;

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

       if ( SA_OK != saCkptCheckpointRetentionDurationSet (&checkpoint_handle, tmp_time) )
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


/* Step1: Create checkpoint with colocated flag.
     Stpe2: Set short duration. The Checkpoint will be deleted after short time. 
     Step3: Open the checkpoint without colocated flag. Because the checkpiont has been deleted,
     So error will be returned.
*/
int set_duration_timeout()
{
	SaTimeT tmp_time ;
	tmp_time = 100 ;

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

       if ( SA_OK != saCkptCheckpointRetentionDurationSet (&checkpoint_handle, tmp_time) )
       	{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
	   	}

	if ( saCkptCheckpointClose(&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	sleep(1) ;
	
	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                           		&ckpt_name,
                         	  	&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout,
					&checkpoint_handle);
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}

	return 0 ;

}


int main(int argc, char* argv[])
{
	char case_name[] = "saCkptCheckpointRetentionDurationSet";
	char name_set_duration[]="checkpoint_set_duration";
	int case_index ;
	 
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_set_duration) ;
	memcpy (ckpt_name.value, name_set_duration, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !set_duration_pre_open())
		printf ("%s set_duration_pre_open %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_duration_pre_open %d FAIL\n\n", case_name, case_index++);
	
	if ( !set_duration_null())
		printf ("%s set_duration_null %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_duration_null %d FAIL\n\n", case_name, case_index++);

	if ( !set_duration_after_finalize())
		printf ("%s set_duration_after_finalize %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_duration_after_finalize %d FAIL\n\n", case_name, case_index++);
	
	if ( !set_duration_after_close())
		printf ("%s set_duration_after_close %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_duration_after_close %d FAIL\n\n", case_name, case_index++);

	if ( !set_duration_normal_call())
		printf ("%s set_duration_normal_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_duration_normal_call %d FAIL\n\n", case_name, case_index++);

	if ( !set_duration_timeout())
		printf ("%s set_duration_timeout %d OK\n\n", case_name, case_index++);
	else
		printf ("%s set_duration_timeout %d FAIL\n\n", case_name, case_index++);

	return 0 ;
}
