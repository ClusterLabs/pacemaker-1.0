/* 
 * unlink.c: data checkpoint API test:saCkptCheckpointUnlink
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
int  unlink_pre_init(void);
int unlink_pre_null_handle(void);
int  unlink_after_finalize(void);
int  unlink_after_close(void);
int unlink_null_name(void);
int unlink_wrong_name(void);
int unlink_error_access(void);
int unlink_normal_call(void);
int unlink_open_after_unlink(void);


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
        Invoke with invalid library handle or before initialization.  In this case, ckpt_name parameter is crossponding 

        to one open checkpoint name.

returns :
               o : indicate success
              -1: fail
*/
int  unlink_pre_init(void)
{
	checkpoint_handle = -1;
	ckpt_error = saCkptCheckpointUnlink (&checkpoint_handle, &ckpt_name);
	if (ckpt_handle == SA_OK)
			return -1 ;
	return 0;
}	

int unlink_pre_null_handle(void)
{
	ckpt_error = saCkptCheckpointUnlink (NULL, &ckpt_name);
	if (ckpt_handle == SA_OK)
		return -1;
	return 0;
}

int  unlink_after_finalize(void)
{

	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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

	ckpt_error = saCkptCheckpointUnlink (&checkpoint_handle, &ckpt_name);
	if (ckpt_error == SA_OK)
		{
			return -1 ;
		}

	return 0 ;

}

int  unlink_after_close(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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

	ckpt_error = saCkptCheckpointUnlink (&checkpoint_handle, &ckpt_name);
	if (ckpt_error == SA_OK)
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
case description :
        Invoke with invalide checkpoint name. Unlink call will return SA_ERR_NAME_NOT_FOUND 

        or SA_ERR_NAME_TOO_LONG.


returns :
               o : indicate success
              -1: fail
*/
int unlink_null_name(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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

	ckpt_error = saCkptCheckpointUnlink (&checkpoint_handle, NULL);
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
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

	return 0;

}

int unlink_wrong_name(void)
{
	SaNameT tmp_name ;
	
	tmp_name.length= ckpt_name.length;
	memcpy( tmp_name.value, ckpt_name.value, ckpt_name.length) ;
	(tmp_name.value)[0]='z';

	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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

	ckpt_error = saCkptCheckpointUnlink (&checkpoint_handle, &tmp_name);
	if (ckpt_error == SA_OK)
		{		
			saCkptCheckpointClose (&checkpoint_handle);
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
case description :
        Invoke with correct steps and parameters. Two cases will be included here:

        1. firstly  unlink after successfully open.

        2. Re-open to see the checkpoint name is still available.        

returns :
               o : indicate success
              -1: fail
*/
int unlink_error_access(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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
	
	if (saCkptCheckpointUnlink( &ckpt_handle, &ckpt_name) != SA_OK)
		{
			saCkptCheckpointClose ( &checkpoint_handle);
			saCkptFinalize (& ckpt_handle) ;
			return -1;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle);
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK)
		return -1 ;

	return 0;


}


int unlink_normal_call(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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
	if (saCkptCheckpointUnlink( &ckpt_handle, &ckpt_name) != SA_OK)
		{
			saCkptCheckpointClose ( &checkpoint_handle);
			saCkptFinalize (& ckpt_handle) ;
			return -1;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle);
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK)
		return -1 ;

	return 0;
}



int unlink_open_after_unlink(void)
{
	SaCkptCheckpointHandleT tmp_handle ;
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
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
	
	if (saCkptCheckpointUnlink( &ckpt_handle, &ckpt_name) != SA_OK)
		{
			saCkptCheckpointClose ( &checkpoint_handle);
			saCkptFinalize (& ckpt_handle) ;
			return -1;
		}
	
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                           		&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout,
					&tmp_handle);
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&tmp_handle);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle);
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle);
			return -1 ;
		}
	if (saCkptFinalize (&ckpt_handle) != SA_OK)
		return -1 ;

	return 0;

}





int  main(void)
{			
	char case_name[] = "saCkptCheckpointUnlink";
	char name_unlink[]="checkpoint_unlink";
	int case_index ;
	 
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_unlink) ;

	memcpy (ckpt_name.value, name_unlink, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !unlink_pre_init())
		printf ("%s unlink_pre_init %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_pre_init %d FAIL\n\n", case_name, case_index++);

	if ( !unlink_pre_null_handle())
		printf ("%s unlink_pre_null_handle %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_pre_null_handle %d FAIL\n\n", case_name, case_index++);


	if ( !unlink_after_finalize())
		printf ("%s unlink_after_finalize %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_after_finalize %d FAIL\n\n", case_name, case_index++);
	
	if ( !unlink_after_close())
		printf ("%s unlink_after_close %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_after_close %d FAIL\n\n", case_name, case_index++);

	if ( !unlink_wrong_name())
		printf ("%s unlink_wrong_name %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_wrong_name %d FAIL\n\n", case_name, case_index++);
	
	if ( !unlink_null_name())
		printf ("%s unlink_null_name %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_null_name %d FAIL\n\n", case_name, case_index++);

	if ( !unlink_error_access())
		printf ("%s unlink_error_access %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_error_access %d FAIL\n\n", case_name, case_index++);

	if ( !unlink_normal_call())
		printf ("%s unlink_normal_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_normal_call %d FAIL\n\n", case_name, case_index++);

	if ( !unlink_open_after_unlink())
		printf ("%s unlink_open_after_unlink %d OK\n\n", case_name, case_index++);
	else
		printf ("%s unlink_open_after_unlink %d FAIL\n\n", case_name, case_index++);

	return 0;

}




