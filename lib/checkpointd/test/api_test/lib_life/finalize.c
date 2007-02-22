/* 
 * finalize.c: data checkpoint API test: saCkptFinalize
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
int  finalize_pre_init(void);
int finalize_after_finalize(void);
int  finalize_noraml_call(void);
int  finalize_invalid_handle(void);
int  finalize_ckpt_exist(void);

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
        Invoke before initialization. 
returns :
               o : indicate success
              -1: fail
*/
int  finalize_pre_init(void)
{
	ckpt_handle = -1;
	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) == SA_OK)
		return -1 ;
	if ( (ckpt_error = saCkptFinalize (NULL)) == SA_OK)
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
int finalize_after_finalize(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  &ckpt_version)) 
		!=SA_OK)
			return -1 ;
	if (saCkptFinalize ( & ckpt_handle) != SA_OK)
		return -1 ;

	if ( (ckpt_error = saCkptFinalize (&ckpt_handle)) == SA_OK)
		return -1 ;
	return 0 ;
	
}

/*
case description :
        Invoke with correct parameter. The case will return SA_OK.
returns :
               o : indicate success
              -1: fail
*/
	
int  finalize_noraml_call(void)
{


	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  &ckpt_version)) 
		!=SA_OK)
			return -1 ;
	if (saCkptFinalize ( & ckpt_handle) != SA_OK)
		return -1 ;
	return 0 ;
}


/*
case description :
        Set the value of SaCkptHandleT  to -1. The invoke returned value 
        will be SA_ERR_BAD_HANDLE.
returns :
               0 : indicate success
              -1: fail
*/
int  finalize_invalid_handle(void)
{
	ckpt_handle = -1;
	if ( (ckpt_error == saCkptFinalize (&ckpt_handle))== SA_OK)
		return -1;
	return 0;
}


/*
case description :
        Fialize library when the open checkpoint exists. The invoke returned valude will be SA_ERR_BUSY.
returns :
               o : indicate success
              -1: fail
*/
int  finalize_ckpt_exist(void)
{
	if ( (ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version)) != SA_OK)
		{
				return -1 ;
		}
	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
		& ckpt_name,  
		& ckpt_create_attri , 
		SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
		open_timeout, 
		& checkpoint_handle) ;
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;			
			return -1 ;
		}
	if ( (ckpt_error = saCkptFinalize(& ckpt_handle))  != SA_OK)
		{
			return -1 ;
		}
	return 0 ;
}


int main(void)
{	
	char case_name[] = "saCkptFinalize";
	char name_fin[]="checkpoint_finalize";
	int case_index ;	 
	 
	ckpt_version.major = VERSION_MAJOR ;
	ckpt_version.minor = VERSION_MINOR ;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_invocation = INVOCATION_BASE ;

	ckpt_name.length = sizeof (name_fin) ;
	memcpy (ckpt_name.value, name_fin, ckpt_name.length);

	/*ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;*/
	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;
	
	case_index = 0 ;

	if ( !finalize_pre_init())
		printf ("%s finalize_pre_init %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_pre_init %d FAIL\n", case_name, case_index++);

	if ( !finalize_after_finalize())
		printf ("%s finalize_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !finalize_noraml_call())
		printf ("%s finalize_noraml_call %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_noraml_call %d FAIL\n", case_name, case_index++);

	if ( !finalize_ckpt_exist())
		printf ("%s finalize_ckpt_exist %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_ckpt_exist %d FAIL\n", case_name, case_index++);
	
	if ( !finalize_invalid_handle())
		printf ("%s finalize_invalid_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_invalid_handle %d FAIL\n", case_name, case_index++);/**/

	return 0 ;	
}


