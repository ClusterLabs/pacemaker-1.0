/* 
 * iterator_finalize.c: Test data checkpoint API : saCkptSectionIteratorNext
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
	const SaCkptCheckpointHandleT *checkpointHandle,SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int finalize_null_iterator(void);
int finalize_invalid_iterator(void);
int finalize_after_finalize(void);


void ckpt_open_callback (SaInvocationT invocation,
	const SaCkptCheckpointHandleT *checkpointHandle,SaErrorT error)
{
		return ;
}


void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error)
{
		return ;
}

/* 
case description :
        Invoke with invalid  SaCkptSectionIteratorT parameter. 

        1. null

        2. invocation after iterator finalization

        3. invalid value ( -1 ).

returns :
               o : indicate success
              -1: fail
*/
int finalize_null_iterator(void)
{
	if (saCkptSectionIteratorFinalize (NULL) == SA_OK)
		return -1 ;
	return 0;
}


int finalize_invalid_iterator(void)
{
	ckpt_iterator = -1 ;
	if (saCkptSectionIteratorFinalize (&ckpt_iterator) == SA_OK)
		return -1 ;
	return 0;
}

int finalize_after_finalize(void)
{
	/* invocation after iterator finalization */
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
       	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                         &ckpt_name, 
                                         &ckpt_create_attri , 
                                         SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_WRITE, 
				   	 open_timeout, 
                                         &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptSectionIteratorInitialize (& checkpoint_handle, SA_CKPT_SECTIONS_ANY, 0, & ckpt_iterator) ;

	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionIteratorFinalize (& ckpt_iterator) != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionIteratorFinalize (& ckpt_iterator) == SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose ( & checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (& ckpt_handle) != SA_OK) 
		return -1 ;

	return 0 ;

}

int main(void)
{
	char case_name[] = "saCkptSectionIteratorFinalize";
	char name_ite_fin[]="checkpoint_ite_fin";
	int case_index ;
	
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_ite_fin) ;

	memcpy (ckpt_name.value, name_ite_fin, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	case_index = 0 ;
	if ( !finalize_invalid_iterator())
		printf ("%s finalize_invalid_iterator %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_invalid_iterator %d FAIL\n", case_name, case_index++);

	if ( !finalize_null_iterator())
		printf ("%s finalize_null_iterator %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_null_iterator %d FAIL\n", case_name, case_index++);
	#if 0
	if ( !finalize_after_finalize())
		printf ("%s finalize_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s finalize_after_finalize %d FAIL\n", case_name, case_index++);
	#endif

	return 0 ;
}
