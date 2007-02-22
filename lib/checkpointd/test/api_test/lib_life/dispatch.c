/* 
 * dispatch.c: data checkpoint API test:saCkptDispatch
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
#include "ckpt_test.h"
#include <stdio.h>
#include <sys/time.h>


int callback_match = 0 ;

void ckpt_open_callback (SaInvocationT invocation,const SaCkptCheckpointHandleT *checkpointHandle,SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int  dispatch_pre_init(void);
int dispatch_after_finalize(void);
int  dispatch_invalid_handle(void);
int  dispatch_invalide_flag(void);
int  dispatch_normal_call(void);





void ckpt_open_callback (SaInvocationT invocation,
		const SaCkptCheckpointHandleT *checkpointHandle,SaErrorT error)
{
		if (invocation != ckpt_invocation)
				return ;
		callback_match = 1 ;
		checkpoint_handle = * checkpointHandle ;
		
		return ;
}


void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error)
{
		return ;
}


/*
case description :
        Invoke before initialization. 
returns :
               o : indicate success
              -1: fail
*/	
int  dispatch_pre_init(void)
{


	ckpt_handle = -1;
	if ( (ckpt_error = saCkptDispatch(&ckpt_handle,  SA_DISPATCH_ALL)) == SA_OK)
		return -1 ;
	if ( (ckpt_error = saCkptDispatch (NULL,  SA_DISPATCH_ALL)) == SA_OK)
		return -1 ;
	return 0 ;
}


/*
case description :
        Invoke after finalization . 
returns :
               o : indicate success
              -1: fail
*/


int dispatch_after_finalize(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  &ckpt_version)) 
		!=SA_OK)
			return -1 ;
	if (saCkptFinalize ( & ckpt_handle) != SA_OK)
		return -1 ;

	if ( (ckpt_error = saCkptDispatch(&ckpt_handle,  SA_DISPATCH_ALL)) == SA_OK)
		return -1; 
	return 0 ;
	
}


/*
case description :
        Set the value of SaCkptHandleT  to -1. The invoke returned value 
        will be SA_ERR_BAD_HANDLE.
returns :
               o : indicate success
              -1: fail
*/
int  dispatch_invalid_handle(void)
{

	ckpt_handle = -1;

	if ( (ckpt_error = saCkptDispatch(&ckpt_handle,  SA_DISPATCH_ALL)) == SA_OK)
		{
			
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	return 0;     

}


/*
case description :
        Invoke with invalid flags.  
returns :
               o : indicate success
              -1: fail
*/

int  dispatch_invalide_flag(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  &ckpt_version)) 
		!=SA_OK)
			return -1 ;

	if ( (ckpt_error = saCkptDispatch(&ckpt_handle,  -1)) == SA_OK)
		return -1 ;
	if ( (ckpt_error = saCkptDispatch(&ckpt_handle,  4)) == SA_OK)
		return -1 ;
	if (saCkptFinalize (&ckpt_handle) != SA_OK)
		return -1 ;
	
	return 0 ;
	
}

/*
case description :
        Invoke with invalid flags.  
returns :
               o : indicate success
              -1: fail
*/
int  dispatch_normal_call(void)
{
	fd_set fs;
	int maxfd ;
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  
						&ckpt_version))!=SA_OK)
			return -1 ;

	if ( (ckpt_error = saCkptSelectionObjectGet (& ckpt_handle, & ckpt_select_obj)) 
				!= SA_OK)
		{
			 
			saCkptFinalize (& ckpt_handle) ;
			return -1;
		}
       
			
	ckpt_error = saCkptCheckpointOpenAsync(& ckpt_handle, ckpt_invocation, & ckpt_name,
			& ckpt_create_attri ,SA_CKPT_CHECKPOINT_READ|SA_CKPT_CHECKPOINT_COLOCATED) ;
									  		
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}	
	
	FD_ZERO (&fs);
	FD_SET (ckpt_select_obj, &fs) ;
	maxfd = ckpt_select_obj + 1 ;	
	if ( select (maxfd, &fs, NULL, NULL, NULL ) == -1)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;				
		}	

	if ( !FD_ISSET (ckpt_select_obj, & fs))
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (  saCkptDispatch (&ckpt_handle , SA_DISPATCH_ALL) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( !callback_match )  /*invocatation mismatch  */
		{	
		
			saCkptFinalize (& ckpt_handle) ;
			return -1 ; 
	       }
	
	if (saCkptCheckpointClose(&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return  -1 ;
		}

		
	if ( (ckpt_error = saCkptFinalize (& ckpt_handle)) != SA_OK) 
		{
			return -1 ;
		}

	
	return 0 ;
	
}



int  main(void) {
	char case_name[] = "saCkptDispatch";
	int case_index ;
	char name_dispatch[]="checkpoint_dispatch";
	
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_invocation = INVOCATION_BASE ;

	
	ckpt_name.length = sizeof (name_dispatch) ;
	memcpy (ckpt_name.value, name_dispatch, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	case_index = 0 ;

	if ( !dispatch_pre_init())
		printf ("%s dispatch_pre_init %d OK\n", case_name, case_index++);
	else
		printf ("%s dispatch_pre_init %d FAIL\n", case_name, case_index++);

	if ( !dispatch_after_finalize())
		printf ("%s dispatch_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s dispatch_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !dispatch_normal_call())
		printf ("%s dispatch_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s dispatch_normal_call %d FAIL\n", case_name, case_index++);

	if ( !dispatch_invalide_flag())
		printf ("%s dispatch_invalide_flag %d OK\n", case_name, case_index++);
	else
		printf ("%s dispatch_invalide_flag %d FAIL\n", case_name, case_index++);
 
	if ( !dispatch_invalid_handle())
		printf ("%s dispatch_invalid_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s dispatch_invalid_handle %d FAIL\n", case_name, case_index++);
 
       return 0;
 

	
}
