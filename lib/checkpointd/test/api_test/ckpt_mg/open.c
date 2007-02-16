/* 
 * open.c: data checkpoint API test:saCkptCheckpointOpen
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
void ckpt_sync_callback (SaInvocationT invocation, 
	SaErrorT error);
int  open_pre_init(void);
int open_null_ckpt_handle(void);
int open_after_finalize(void);
int open_normal_call(void);
int open_normal_call_read(void);
int open_normal_call_write(void);
int open_null_create_attribute(void);
int open_invalid_flag(void);
int open_error_create_attribute(void);
int open_null_handle(void);
int open_null_name(void);
int open_repeat_call(void);


void ckpt_open_callback (SaInvocationT invocation,
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaErrorT error)
{
	return ;
}

void ckpt_sync_callback (SaInvocationT invocation, 
	SaErrorT error)
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
int  open_pre_init()
{

	ckpt_handle = -1 ;

	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           &ckpt_create_attri , 
				           SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
				      	   open_timeout,
					   &checkpoint_handle);
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0 ;
}

int open_null_ckpt_handle()
{
	ckpt_error = saCkptCheckpointOpen (NULL, 
                                          &ckpt_name,
					  &ckpt_create_attri , 
					  SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
					  open_timeout,
					  &checkpoint_handle) ;

	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0 ;
}

/*
case description :
        Invoke with old  library handle after finalization.   
returns :
               o : indicate success
              -1: fail
*/
int open_after_finalize()
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
       	{
		return -1 ;
	}
	
 	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
 		{
			return -1 ;
		}

	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           &ckpt_create_attri , 
				           SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
				      	   open_timeout,
					   &checkpoint_handle);

	if ( ckpt_error == SA_OK)
		return -1;

	return 0 ;
}

/*

description: 
		This function tests whether "open" with correct parameter succeeds or not. Four sub cases are tested:
		1.colocated open mode
		2. read mode .
		3. write mode.
		4. The creationAttributes parameter is null and the checkpoint does exist.
		
returns :
               o : indicate success
              -1: fail
*/
int open_normal_call()
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

int open_normal_call_read()
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
				           SA_CKPT_CHECKPOINT_COLOCATED, 
				      	   open_timeout,
					   &checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
		
	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           &ckpt_create_attri , 
				           SA_CKPT_CHECKPOINT_READ, 
				      	   open_timeout,
					   &tmp_handle);

  
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&tmp_handle) != SA_OK )
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

int open_normal_call_write()
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
				           SA_CKPT_CHECKPOINT_COLOCATED, 
				      	   open_timeout,
					   &checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
		
	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           &ckpt_create_attri , 
				           SA_CKPT_CHECKPOINT_WRITE, 
				      	   open_timeout,
					   &tmp_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&tmp_handle) != SA_OK )
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

int open_null_create_attribute()
{	
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           NULL , 
				           SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
				      	   open_timeout,
					   &checkpoint_handle);
  
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) != SA_OK)
		{
			return -1 ;
		}
	
	return 0 ;

}

int open_invalid_flag()
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           &ckpt_create_attri , 
				           5, 
				      	   open_timeout,
					   &checkpoint_handle);
  
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
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
          	In this case, the parameter "SaCkptCheckpointCreationAtrributesT" are different from the ones used at creation time.
          	The checkpoint does exist. 
		Invocaiton should return error code "SA_ERR_EXIST".  
returns :
               o : indicate success
              -1: fail


*/
int open_error_create_attribute()
{
	 /* open the checkpiont in colocated mode .  */	   
	SaCkptCheckpointCreationAttributesT attri_tmp ;
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
				           SA_CKPT_CHECKPOINT_COLOCATED, 
				      	   open_timeout,
					   &checkpoint_handle);  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	memset(&attri_tmp,0,sizeof(attri_tmp));
	memcpy (&attri_tmp, &ckpt_create_attri, sizeof(ckpt_create_attri)) ;
	
	attri_tmp.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA_WEAK ;

	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
				           &attri_tmp, 
				           SA_CKPT_CHECKPOINT_WRITE, 
				      	   open_timeout,
					   &tmp_handle);  
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&tmp_handle);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK )
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize(&ckpt_handle) != SA_OK)
		return -1 ;

	return 0 ;	
}

/*

case description :
          	The parameter  checkpointHandle' value is set to null. 
          	Invocaiton should return error code "SA_ERR_INVALID_PARAM".  
returns :
               o : indicate successp
              -1: fail


*/
int open_null_handle()
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
					   NULL);  
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
          	The parameter  checkpointName' value is set to null. 
          	Invocaiton should return error code "SA_ERR_INVALID_PARAM".  
returns :
               o : indicate successp
              -1: fail

*/
int open_null_name()
{
	ckpt_error = saCkptInitialize ( & ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	 /* open the checkpiont in colocated mode .  */
	 ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           NULL,
				           &ckpt_create_attri , 
				           SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
				      	   open_timeout,
					   &checkpoint_handle);  
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
          	This case repeats calling open function. Each call should return differrent handel.

returns :
               o : indicate successp
              -1: fail


*/
int open_repeat_call()
{
	SaCkptCheckpointHandleT handle_array[5] ;
	int i, j ;
	int ret_err = 0 ;	

	ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback, &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	for ( i = 0; i < 5; i++)
	{
		ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
						&ckpt_name,
				         	&ckpt_create_attri , 
						SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
						open_timeout,
						&handle_array[i]);
		if (ckpt_error != SA_OK) break;
		
		for ( j = 0; j < i ; j ++)				 
			{
				if (handle_array[i] == handle_array[j])
					break;
			}
	}

	for (j = 0; j < i; j++)
	{
		if (saCkptCheckpointClose (&handle_array[j]) != SA_OK)
			ret_err = -1;
			
	}
	
	if (i != 5) ret_err = -1 ;
	
	if (saCkptFinalize(&ckpt_handle) != SA_OK)
		ret_err = -1;

	return ret_err ;
}


int  main(int argc, char* argv[])
{
	char case_name[] = "saCkptCheckpointOpen";
	char name_open[]=("checkpoint_open");
	int case_index ;
	 
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_invocation = INVOCATION_BASE ;

	ckpt_name.length = sizeof(name_open) ;
	memcpy (ckpt_name.value, name_open, ckpt_name.length);
	
	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !open_pre_init())
		printf ("%s open_pre_init %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_pre_init %d FAIL\n\n", case_name, case_index++);

	if ( !open_null_ckpt_handle())
		printf ("%s open_null_ckpt_handle %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_null_ckpt_handle %d FAIL\n\n", case_name, case_index++);

	if ( !open_after_finalize())
		printf ("%s open_after_finalize %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_after_finalize %d FAIL\n\n", case_name, case_index++);
	
	if ( !open_normal_call())
		printf ("%s open_normal_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_normal_call %d FAIL\n\n", case_name, case_index++);
	
	if ( !open_normal_call_write())
		printf ("%s open_normal_call_write %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_normal_call_write %d FAIL\n\n", case_name, case_index++);

	if ( !open_normal_call_read())
		printf ("%s open_normal_call_read %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_normal_call_read %d FAIL\n\n", case_name, case_index++);

	#if 0	
	if ( !open_invalid_flag())
		printf ("%s open_invalid_flag %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_invalid_flag %d FAIL\n\n", case_name, case_index++);
	#endif
	
	if ( !open_repeat_call())
		printf ("%s open_repeat_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_repeat_call %d FAIL\n\n", case_name, case_index++);

	if ( !open_null_create_attribute())
		printf ("%s open_null_create_attribute %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_null_create_attribute %d FAIL\n\n", case_name, case_index++);
		
	if ( !open_null_handle())
		printf ("%s open_null_handle %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_null_handle %d FAIL\n\n", case_name, case_index++);
		
	if ( !open_null_name())
		printf ("%s open_null_name %d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_null_name %d FAIL\n\n", case_name, case_index++);
	
	if ( !open_error_create_attribute())
		printf ("%s open_error_create_attribute%d OK\n\n", case_name, case_index++);
	else
		printf ("%s open_error_create_attribute%d FAIL\n\n", case_name, case_index++);

	return 0 ;
}
