/* 
 * get_status.c: data checkpoint API test:saCkptCheckpointStatusGet
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
int  get_status_pre_open(void);
int get_status_null(void);
int get_status_after_finalize(void);
int get_status_after_close(void);
int get_status_null_status(void);
int get_status_normal_call(void);


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
int  get_status_pre_open(void)
{

	SaCkptCheckpointStatusT  checkpoint_stauts ;

	checkpoint_handle = -1 ;

	ckpt_error = saCkptCheckpointStatusGet(&checkpoint_handle, & checkpoint_stauts) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0;

}

int get_status_null(void)
{
	SaCkptCheckpointStatusT  checkpoint_stauts ;
	ckpt_error = saCkptCheckpointStatusGet(NULL, & checkpoint_stauts) ;
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
int get_status_after_finalize(void)
{

	SaCkptCheckpointStatusT  checkpoint_stauts ;
	
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
	
	ckpt_error = saCkptCheckpointStatusGet(&checkpoint_handle, &checkpoint_stauts);
	if (ckpt_error == SA_OK)
			return -1 ;
	
	return 0;

}

int get_status_after_close(void)
{

	SaCkptCheckpointStatusT  checkpoint_stauts ;
	
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
	
	
	ckpt_error = saCkptCheckpointStatusGet(&checkpoint_handle, &checkpoint_stauts);
	if (ckpt_error == SA_OK)
		{
			saCkptFinalize (&ckpt_handle);
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
        Invoke with invalid checkpoint status parameter. Pass null to the point of SaCkptCheckpointStautsT.

returns :

               o : indicate success
              -1: fail
*/
int get_status_null_status(void)
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

	ckpt_error = saCkptCheckpointStatusGet(&checkpoint_handle, NULL);
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle);
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
        Invoke with correct steps and parameter. We ensure only the test case opens the checkpoint.

        After successfully getting the checkpoint status, compare the creation atrributes. The number of 

	 sections should be one for default section open. 

returns :

               o : indicate success
              -1: fail
*/
int get_status_normal_call(void)
{
	SaCkptCheckpointStatusT  checkpoint_stauts ;
	int mismatch = 0 ;

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

	ckpt_error = saCkptCheckpointStatusGet(&checkpoint_handle, &checkpoint_stauts);
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle);
			return -1 ;
		}

	/* compare each componts of attributes */

	if ( !mismatch && checkpoint_stauts.checkpointCreationAttributes.checkpointSize != ckpt_create_attri.checkpointSize)
		mismatch = 1 ;

	if ( !mismatch && checkpoint_stauts.checkpointCreationAttributes.retentionDuration != ckpt_create_attri.retentionDuration)
		mismatch = 1 ;

	if ( !mismatch && checkpoint_stauts.checkpointCreationAttributes.creationFlags!= ckpt_create_attri.creationFlags)
		mismatch = 1 ;	

	if ( !mismatch && checkpoint_stauts.checkpointCreationAttributes.maxSections!= ckpt_create_attri.maxSections)
		mismatch = 1 ;

	if ( !mismatch && checkpoint_stauts.checkpointCreationAttributes.maxSectionIdSize!= ckpt_create_attri.maxSectionIdSize)
		mismatch = 1 ;

	if ( !mismatch && checkpoint_stauts.checkpointCreationAttributes.maxSectionSize!= ckpt_create_attri.maxSectionSize)
		mismatch = 1 ;

	if ( mismatch )
		{
			saCkptCheckpointClose (& checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* the value of numberOfSections should be 1 */
	if ( !mismatch && checkpoint_stauts.numberOfSections != 1)
			mismatch=1;

	if (saCkptCheckpointClose (& checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return mismatch;

}



int main(int argc, char* argv[])
{
	char case_name[] = "saCkptCheckpointStatusGet";
	char name_get_status[]="checkpoint_get_status";
	int case_index ;
	 
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_get_status) ;

	memcpy (ckpt_name.value, name_get_status, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !get_status_pre_open())
		printf ("%s get_status_pre_open %d OK\n\n", case_name, case_index++);
	else
		printf ("%s get_status_pre_open %d FAIL\n\n", case_name, case_index++);
	
	if ( !get_status_null())
		printf ("%s get_status_null %d OK\n\n", case_name, case_index++);
	else
		printf ("%s get_status_null %d FAIL\n\n", case_name, case_index++);


	if ( !get_status_after_finalize())
		printf ("%s get_status_after_finalize %d OK\n\n", case_name, case_index++);
	else
		printf ("%s get_status_after_finalize %d FAIL\n\n", case_name, case_index++);
	
	if ( !get_status_after_close())
		printf ("%s get_status_after_close %d OK\n\n", case_name, case_index++);
	else
		printf ("%s get_status_after_close %d FAIL\n\n", case_name, case_index++);


	if ( !get_status_null_status())
		printf ("%s get_status_null_status%d OK\n\n", case_name, case_index++);
	else
		printf ("%s get_status_null_status%d FAIL\n\n", case_name, case_index++);

	if ( !get_status_normal_call())
		printf ("%s get_status_normal_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s get_status_normal_call %d FAIL\n\n", case_name, case_index++);

	return 0 ;

}



