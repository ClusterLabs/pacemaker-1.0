/* 
 * delete.c: data checkpoint API test: saCkptSectionDelete
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
#include <time.h>

void ckpt_open_callback (SaInvocationT invocation,
	const SaCkptCheckpointHandleT *checkpointHandle,SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int  delete_pre_open(void);
int delete_null_handle(void);
int delete_after_close(void);
int delete_after_finalize(void);
int delete_normal_call(void);
int delete_non_exist(void);
int delete_null_section_id(void);
int delete_duplicate(void);
int delete_err_access(void);
int delete_defaul_section(void);


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
        Invoke with invalid checkpoint handle or before open.  
returns :
               o : indicate success
              -1: fail
*/
int  delete_pre_open(void)
{
	checkpoint_handle = -1 ;
 	ckpt_error = saCkptSectionDelete (& checkpoint_handle, & sect_id );
	
	if ( ckpt_error == SA_OK)
		return -1 ;
	return 0;	
}

int delete_null_handle(void)
{
	ckpt_error = saCkptSectionDelete (NULL , &sect_id );
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0 ;
}

/*
case description :
        Invoke with invalid checkpoint handle. Two cases will be included:

         after close operation


 

returns :
               o : indicate success
              -1: fail
*/
int delete_after_close(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,  
                          		&ckpt_create_attri , 
                         		SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE,  
					open_timeout, 
                         		&checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* call after close operation */
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptSectionDelete (&checkpoint_handle, & sect_id) ;

	if( ckpt_error == SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;
	
}
/*
case description :
        Invoke with invalid checkpoint handle. Two cases will be included:

         after finalization operation 

returns :
               o : indicate success
              -1: fail
*/

int delete_after_finalize(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE,  
					open_timeout, 
					& checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* call after finalization */
	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;

	ckpt_error = saCkptSectionDelete (& checkpoint_handle, & sect_id) ;
	if( ckpt_error == SA_OK)
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
int delete_normal_call(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE,  
					open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ; 
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;

	return 0 ; 
}


/*

case description :
        The section to be deleted , identified by sectionId, does not exist . 

        1. Delete with non exist section id

returns :
               o : indicate success
              -1: fail
*/
int delete_non_exist(void)
{
	SaUint8T sect_tmp_id[] = "12345679";

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, 
					& checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
	
	/*Delete with non exist section id*/
	sect_id.id = sect_tmp_id ;

	if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) == SA_OK)
		{
			
			sect_id.id = sect_id_array ;
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	sect_id.id = sect_id_array ;
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0 ;
}

/*

case description :
        The section to be deleted , identified by sectionId, does not exist . 
         duplicate delete

returns :
               o : indicate success
              -1: fail
*/
int delete_duplicate(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, 
					& checkpoint_handle) ;

	if ( ckpt_error != SA_OK)

		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	/* duplicate delete */
	sect_id.id = sect_id_array ;
	if ( saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0 ;
}

/*
case description :
        The section to be deleted , identified by sectionId, does not exist . 

         null sectionId pointer

returns :
               o : indicate success
              -1: fail
*/
int delete_null_section_id(void)
{

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, 
					& checkpoint_handle) ;

	if ( ckpt_error != SA_OK)

		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	/* null pointer of sectionId*/
	if ( saCkptSectionDelete( & checkpoint_handle, NULL) == SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0 ;
}




/*
case description :
        Open not in write mode. SA_ERR_ACCESS will be returned.


returns :
               o : indicate success
              -1: fail
*/
int delete_err_access(void)
{
	SaCkptHandleT tmp_handle ;
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, 
					& checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
       
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_READ, 
					open_timeout, & tmp_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptCheckpointClose (&tmp_handle ) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionDelete(&tmp_handle,  sect_create_attri.sectionId) == SA_OK )
		{
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptCheckpointClose (& tmp_handle ) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	if ( saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptCheckpointClose ( & tmp_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&tmp_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;

	return 0 ;

}



/*
case description :
        Try to delete the defaut section: SA_CKPT_DEFAULT_SECTION_ID. SA_ERR_INVALID_PARAM 

        will be returned.

returns :

               o : indicate success
              -1: fail
*/

int delete_defaul_section(void)
{
	SaCkptHandleT tmp_handle ;

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, 
					& ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, 
					& checkpoint_handle) ;

	if ( ckpt_error != SA_OK)

		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

     	 sect_id.id = NULL ;
	 sect_id.idLen = 0 ; 

	if (saCkptSectionDelete (& ckpt_handle, &sect_id) == SA_OK)
		{
            		sect_id.id = sect_id_array ;
			sect_id.idLen = sizeof (sect_id_array) ;
			saCkptCheckpointClose ( & tmp_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	sect_id.id = sect_id_array ;
	sect_id.idLen = sizeof (sect_id_array) ;

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	return 0;

}



int main(void)
{
	char case_name[] = "saCkptSectionDelete";	
	char name_del[]="checkpoint_del";
	int case_index ;	
	
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_del) ;

	memcpy (ckpt_name.value, name_del, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	sect_id.id = sect_id_array ;
	sect_id.idLen = sizeof (sect_id_array) ;

	sect_create_attri.sectionId = & sect_id ;
	time (&cur_time) ;
	sect_create_attri.expirationTime = (SaTimeT)((cur_time + 3600*24)*1000000000LL);
		
	case_index = 0 ;

	if ( !delete_pre_open())
		printf ("%s delete_pre_open%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_pre_open%d FAIL\n", case_name, case_index++);

	if ( !delete_null_handle())
		printf ("%s delete_null_handle%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_null_handle%d FAIL\n", case_name, case_index++);

	if ( !delete_after_finalize())
		printf ("%s delete_after_finalize%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_after_finalize%d FAIL\n", case_name, case_index++);

	if ( !delete_after_close())
		printf ("%s delete_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s delete_after_close %d FAIL\n", case_name, case_index++);

	if ( !delete_defaul_section())
		printf ("%s delete_defaul_section%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_defaul_section%d FAIL\n", case_name, case_index++);

	if ( !delete_err_access())
		printf ("%s delete_err_access%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_err_access%d FAIL\n", case_name, case_index++);

	if ( !delete_non_exist())
		printf ("%s delete_non_exist%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_non_exist%d FAIL\n", case_name, case_index++);

	if ( !delete_duplicate())
		printf ("%s delete_duplicate%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_duplicate%d FAIL\n", case_name, case_index++);

	if ( !delete_null_section_id())
		printf ("%s delete_null_section_id%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_null_section_id%d FAIL\n", case_name, case_index++);

	if ( !delete_normal_call())
		printf ("%s delete_normal_call%d OK\n", case_name, case_index++);
	else
		printf ("%s delete_normal_call%d FAIL\n", case_name, case_index++);
	
	return 0;
}




