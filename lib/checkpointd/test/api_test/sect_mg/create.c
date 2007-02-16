/* 
 * creat.c: data checkpoint API test: saCkptSectionCreate
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
int  create_pre_open(void);
int create_null_handle(void);
int create_after_close(void);
int create_after_finalize(void);
int create_err_access(void);
int  create_null_initial_data(void);
int create_invalid_initial_size(void);
int create_null_section_id(void);
int create_null_id(void);
int create_too_long_idlen(void);
int create_expire_time(void);
int create_normal_call_user_spec_id(void);
int create_normal_call_generate_id(void);
int create_duplicate_call(void);




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
int  create_pre_open(void)
{
	checkpoint_handle = -1 ;

 	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & sect_create_attri,  
					  (void *) init_data, sizeof(init_data)) ;
	
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0 ;
}

int create_null_handle(void)
{
	ckpt_error = saCkptSectionCreate (NULL, & sect_create_attri,  
					  (void *) init_data, sizeof(init_data)) ;

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
int create_after_close(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name, & ckpt_create_attri, 				       SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE,
				    open_timeout, & checkpoint_handle) ;

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
	
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & sect_create_attri,  
					  (void *) init_data, sizeof(init_data)) ;
	if( ckpt_error == SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;

			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
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
int create_after_finalize(void)
{

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

	/* call after finalization */
	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & sect_create_attri,  
					  (void *) init_data, sizeof(init_data)) ;
	if( ckpt_error == SA_OK)
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
int create_err_access(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name, &ckpt_create_attri, 					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_READ, 					   open_timeout, & checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & sect_create_attri,  
					(void *) init_data, sizeof(init_data)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete( & checkpoint_handle,  
			sect_create_attri.sectionId);
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
		 Invalid initialData parameter(Null). The is one normal case. SA_OK should be returned.
returns :
               o : indicate success
              -1: fail
*/
int  create_null_initial_data(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, &ckpt_name, &ckpt_create_attri,
				   SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
				   open_timeout, & checkpoint_handle) ;
	if ( ckpt_error != SA_OK)

		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & sect_create_attri,  
					  NULL, 0) ;
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
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
		 Invalid initialDataSize parameter. The size exceeds the maxsection size specified in 
		 checkpoint creating.
returns :
               o : indicate success
              -1: fail
*/
int create_invalid_initial_size(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  & ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)

		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & sect_create_attri,  
			(void *)init_data, ckpt_create_attri.maxSectionSize + 1) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
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
		 Invalid SaCkptSectionIdT parameter. Two  subcases will be included:
		  null pointer of  SaCkptSectionIdT
returns :
               o : indicate success
              -1: fail
*/
int create_null_section_id(void)
{
	SaCkptSectionCreationAttributesT tmp_sect_attri ;
	/*SaCkptSectionIdT tmp_sect_id ;*/
	
	
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
				& ckpt_create_attri , 
				SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
				open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)

		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
	
      /* null pointer of sectionId */
	tmp_sect_attri.sectionId = NULL ;
	tmp_sect_attri.expirationTime = sect_create_attri.expirationTime ;
	
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & tmp_sect_attri,
			  	(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error == SA_OK)
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
		 Invalid SaCkptSectionIdT parameter. Two  subcases will be included:
		  Null pointer of id , compont varible of SaCkptSectionIdT
returns :
               o : indicate success
              -1: fail
*/
int create_null_id(void)
{
	SaCkptSectionCreationAttributesT tmp_sect_attri ;
	SaCkptSectionIdT tmp_sect_id ;	
	
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
			& ckpt_create_attri , 
			SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
			open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
	
	
	/*  Null pointer of id , compont varible of SaCkptSectionIdT*/
	tmp_sect_attri.sectionId =&tmp_sect_id;
	tmp_sect_attri.expirationTime = sect_create_attri.expirationTime ;
	
	tmp_sect_id.id = NULL ;
	tmp_sect_id.idLen=0;
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & tmp_sect_attri,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error == SA_OK)
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
		 Invalid SaCkptSectionIdT parameter. Two  subcases will be included:
		  too long idLen which exceeds the maximum  id lengh.
returns :
               o : indicate success
              -1: fail
*/
int create_too_long_idlen(void)
{
	SaCkptSectionCreationAttributesT tmp_sect_attri ;
	SaCkptSectionIdT tmp_sect_id ;
	
	
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
			& ckpt_create_attri , 
			SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
			open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
	

	/* too long idLen which exceeds the maximum  id lengh.*/	
	tmp_sect_attri.sectionId = &tmp_sect_id;
	tmp_sect_id.id = sect_create_attri.sectionId->id ;
	tmp_sect_id.idLen = ckpt_create_attri.maxSectionIdSize + 1 ;	
	tmp_sect_attri.expirationTime=sect_create_attri.expirationTime;

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & tmp_sect_attri,  
				  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
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
		 Test section expire time  parameter. Two cases will be included:
		 1. The absolute time is before current time. 
		 2. Check SA_TIME_END support.
returns :
               o : indicate success
              -1: fail
*/
int create_expire_time(void)
{
	SaCkptSectionCreationAttributesT tmp_sect_attri ;

	time_t tmp_time ;
	
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
			& ckpt_create_attri , 
			SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
			open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
	
	
	tmp_sect_attri.sectionId = sect_create_attri.sectionId ;
	
	/* The absolute time is before current time */
	time (& tmp_time ) ;
	tmp_sect_attri.expirationTime = (SaTimeT) tmp_time * 1000000000LL/2;
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, & tmp_sect_attri,(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;				
		}

	/* Check SA_TIME_END support */
	tmp_sect_attri.expirationTime = SA_TIME_END ;

	ckpt_error = saCkptSectionCreate(& checkpoint_handle, & tmp_sect_attri,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptSectionDelete( & checkpoint_handle,  
		sect_create_attri.sectionId) != SA_OK) {
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
		Invocation with normal steps and parameters. It will include two cases:
		 user specified sectionId
returns :
               o : indicate success
              -1: fail
*/
int create_normal_call_user_spec_id(void)
{
	
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
			& ckpt_create_attri , 
			SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE,  
			open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}

	/* user specified secton Id */
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri,  
					(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if ( saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
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
		Invocation with normal steps and parameters. It will include two cases:
		 SA_CKPT_GENERATED_SECTINON_ID support. A new section ID will be generated.
returns :
               o : indicate success
              -1: fail
*/

int create_normal_call_generate_id(void)
{
	
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}


	/*SA_CKPT_GENERATED_SECTINON_ID support. A new section ID will be generated  */

	sect_create_attri.sectionId->id = NULL ;
	sect_create_attri.sectionId->idLen = 0 ;
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,
					(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
		/*	saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);*/
			sect_id.id = sect_id_array ;
			sect_id.idLen = sizeof (sect_id_array) ;
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;

			return -1 ;
		}
	if (!( sect_create_attri.sectionId->id && sect_create_attri.sectionId->idLen ))
		{
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
			sect_id.id = sect_id_array ;
			sect_id.idLen = sizeof (sect_id_array) ;
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{ 
			sect_id.id = sect_id_array ;
			sect_id.idLen = sizeof (sect_id_array) ;
			saCkptCheckpointClose (&checkpoint_handle) ;
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
	
	return 0 ;
}

/*
case description :
	Recall saCkptSectionCreate with  same creation parameter. SA_ERR_EXIST will be returned.
returns :
               o : indicate success
              -1: fail
*/

int create_duplicate_call(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;

	if (ckpt_error != SA_OK)
        {
		return -1 ;
	}

 	ckpt_error = saCkptCheckpointOpen (& ckpt_handle, & ckpt_name,  
					& ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, & checkpoint_handle) ;

	if ( ckpt_error != SA_OK)
	{
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;

	}

	saCkptSectionDelete(&checkpoint_handle, &sect_id);
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,
				(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
	{
		/*saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);*/
		saCkptCheckpointClose ( & checkpoint_handle) ;
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}

	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri , 
					(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error == SA_OK)
	{
		saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
		saCkptCheckpointClose ( & checkpoint_handle) ;
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}
	
	if ( saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
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


int main(int argc, char* argv[])
{

	char case_name[] = "saCkptSectionCreate";
	char name_creat[]="checkpoint_creat";
	int case_index ;
		
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_creat) ;

	memcpy (ckpt_name.value, name_creat, ckpt_name.length);

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
	
	if ( !create_pre_open())
		printf ("%s create_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s create_pre_open %d FAIL\n", case_name, case_index++);

	if ( !create_after_close())

		printf ("%s create_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s create_after_close %d FAIL\n", case_name, case_index++);

	if ( !create_after_finalize())

		printf ("%s create_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s create_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !create_err_access())

		printf ("%s create_err_access %d OK\n", case_name, case_index++);
	else
		printf ("%s create_err_access %d FAIL\n", case_name, case_index++);

	if ( !create_null_section_id())
		printf ("%s create_null_section_id %d OK\n", case_name, case_index++);
	else
		printf ("%s create_null_section_id %d FAIL\n", case_name, case_index++);
	
	#if 0	
	if ( !create_null_id())
		printf ("%s create_null_id %d OK\n", case_name, case_index++);
	else
		printf ("%s create_null_id %d FAIL\n", case_name, case_index++);
	#endif	
	
	if ( !create_too_long_idlen())
		printf ("%s create_too_long_idlen %d OK\n", case_name, case_index++);
	else
		printf ("%s create_too_long_idlen %d FAIL\n", case_name, case_index++);

	if ( !create_expire_time())
		printf ("%s create_expire_time %d OK\n", case_name, case_index++);
	else
		printf ("%s create_expire_time %d FAIL\n", case_name, case_index++);

	if ( !create_invalid_initial_size())
		printf ("%s create_invalid_initial_size %d OK\n", case_name, case_index++);
	else
		printf ("%s create_invalid_initial_size %d FAIL\n", case_name, case_index++);

	if ( !create_normal_call_user_spec_id())

		printf ("%s create_normal_call_user_spec_id %d OK\n", case_name, case_index++);
	else
		printf ("%s create_normal_call_user_spec_id %d FAIL\n", case_name, case_index++);

	if ( !create_normal_call_generate_id())

		printf ("%s create_normal_call_generate_id %d OK\n", case_name, case_index++);
	else
		printf ("%s create_normal_call_generate_id %d FAIL\n", case_name, case_index++);

	if ( !create_null_initial_data())
		printf ("%s create_null_initial_data %d OK\n", case_name, case_index++);
	else
		printf ("%s create_null_initial_data %d FAIL\n", case_name, case_index++);
	
	if ( !create_duplicate_call())
		printf ("%s create_duplicate_call %d OK\n", case_name, case_index++);
	else
		printf ("%s create_duplicate_call %d FAIL\n", case_name, case_index++);

	return 0 ;
}
