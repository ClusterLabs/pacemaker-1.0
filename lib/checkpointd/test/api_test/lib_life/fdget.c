/* 
 * fdget.c: data checkpoint API test: saCkptSelectionObjectGet
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
			const SaCkptCheckpointHandleT *checkpointHandle, SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int  fdget_pre_init(void);
int fdget_after_finalize(void);
int fdget_null_select_obj(void);
int  fdget_invalid_handle(void);
int  fdget_normal_call(void);

void ckpt_open_callback (SaInvocationT invocation, 
			const SaCkptCheckpointHandleT *checkpointHandle, SaErrorT error)
{

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
int  fdget_pre_init(void)
{


	ckpt_handle = -1;
	if ( (ckpt_error = saCkptSelectionObjectGet (&ckpt_handle,  &ckpt_select_obj)) == SA_OK)
		return -1 ;
	if ( (ckpt_error = saCkptSelectionObjectGet (NULL,  &ckpt_select_obj)) == SA_OK)
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
int fdget_after_finalize(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  &ckpt_version))!=SA_OK)
		return -1 ;
	if (saCkptFinalize ( & ckpt_handle) != SA_OK)
		return -1 ;
	if ( (ckpt_error = saCkptSelectionObjectGet (&ckpt_handle,  &ckpt_select_obj)) == SA_OK)
		return -1; 

	return 0 ;
	
}

/*
case description :
        Set the value of parameter saCkptSelectionObjectT to null . The invoke returned value 
        will be SA_ERR_INVALIDE_PARAM.
returns :
               o : indicate success
              -1: fail
*/
int fdget_null_select_obj(void)
{
	if (saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) != SA_OK)
		return -1 ;
	if ( (ckpt_error = saCkptSelectionObjectGet (&ckpt_handle, NULL))== SA_OK)
	{
			
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}       
	if ( saCkptFinalize (& ckpt_handle) != SA_OK) 
		return -1 ;
	return 0 ;
}


/*
case description :
        Set the value of SaCkptHandleT  to -1. The invoke returned value 
        will be SA_BAD_HANDLE.
returns :
               o : indicate success
              -1: fail
*/
int  fdget_invalid_handle(void)
{

	ckpt_handle = -1;
	ckpt_error = saCkptSelectionObjectGet (&ckpt_handle,&ckpt_select_obj);
	if(ckpt_error== SA_OK)
		{
			
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	return 0;       

}

/*

case description :
        Call with correct steps and parameters . The invoke returned value 
        will be SA_OK.
returns :
               o : indicate success
              -1: fail

*/
int  fdget_normal_call(void)
{

	if (saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) != SA_OK)
		return -1 ;

	if ( (ckpt_error = saCkptSelectionObjectGet (& ckpt_handle, &ckpt_select_obj)) 
		!= SA_OK)
		{
			
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
       
	if ( saCkptFinalize (& ckpt_handle) != SA_OK) 
		return -1 ;
	return 0 ;

}

int main(void)
{

	char case_name[] = "saCkptSelectionObjectGet";
	int case_index ;
		
	 
	ckpt_version.major = VERSION_MAJOR ;
	ckpt_version.minor = VERSION_MINOR ;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	case_index = 0 ;

	if ( !fdget_pre_init())
		printf ("%s fdget_pre_init %d OK\n", case_name, case_index++);
	else
		printf ("%s fdget_pre_init %d FAIL\n", case_name, case_index++);
	
	if ( !fdget_after_finalize())
		printf ("%s fdget_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s fdget_after_finalize %d FAIL\n", case_name, case_index++);
	
	if ( !fdget_null_select_obj())
		printf ("%s fdget_null_select_obj %d OK\n", case_name, case_index++);
	else
		printf ("%s fdget_null_select_obj %d FAIL\n", case_name, case_index++);

	if ( !fdget_invalid_handle())
		printf ("%s fdget_invalid_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s fdget_invalid_handle %d FAIL\n", case_name, case_index++);

	if ( !fdget_normal_call())
		printf ("%s fdget_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s fdget_normal_call %d FAIL\n", case_name, case_index++);
	

	return 0;
}
