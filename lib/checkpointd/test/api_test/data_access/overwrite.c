/* 
 * overwrite.c: Test data checkpoint API : saCkptSectionOverwrite
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


SaCkptIOVectorElementT sectread ;
char buffer[256];

void ckpt_open_callback (SaInvocationT invocation,
		 	 const SaCkptCheckpointHandleT *checkpointHandle,
			 SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int overwrite_pre_open(void);
int overwrite_null_handle(void);
int overwrite_after_finalize(void);
int overwrite_after_delete(void);
int overwrite_after_close(void);
int overwrite_normal_call(void);
int overwrite_default_section(void);
int overwrite_err_access(void);
int overwrite_null_databuffer(void);
int  overwrite_negative_datasize(void);
int  overwrite_big_datasize(void);


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
int overwrite_pre_open(void)
{
	checkpoint_handle = -1 ;
	ckpt_error = saCkptSectionOverwrite (& checkpoint_handle,  &sect_id, sect_id_array, sizeof (sect_id_array)) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0;
}

int overwrite_null_handle(void)
{	
	ckpt_error = saCkptSectionOverwrite (NULL, &sect_id, sect_id_array, sizeof (sect_id_array)) ;

	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0 ;

}



/*
case description :
        Invoke with invalid checkpoint handle. Two cases will be included:
        1. after close operation

        2. after finalization operation

        3. after section delete

returns :

               o : indicate success
              -1: fail
*/
int overwrite_after_finalize(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                &sect_create_attri,  
			  	(void *) init_data, 
                                sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      sizeof (sect_id_array)) ;
	if (ckpt_error == SA_OK)
		{
			return -1 ;	
		}

	return 0;
}

int overwrite_after_delete(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                &sect_create_attri,  
				(void *) init_data, 
                                sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      sizeof (sect_id_array)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptCheckpointClose(&checkpoint_handle) ;
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;

}

int overwrite_after_close(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                &sect_create_attri,  
				(void *) init_data, 
                                sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      sizeof (sect_id_array)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;

}

/*
case description :
        Invoke with correct steps and parameters.  SA_OK will be returned.

returns :

               o : indicate success
              -1: fail
*/
int overwrite_normal_call(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                          		&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&checkpoint_handle);
  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                  &sect_create_attri,  
				  	(void *) init_data, 
                                  sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      sizeof (sect_id_array)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
		
	/*check if the overwrite data has been write into the section	 */
	sectread.sectionId.id = sect_id_array ;
	sectread.sectionId.idLen = sizeof(sect_id_array) ;
	sectread.dataBuffer = buffer ;
	sectread.dataSize = sizeof (sect_id_array) ;
	sectread.dataOffset = 0 ;
	sectread.readSize = 0 ; 	
				
	if (saCkptCheckpointRead (&checkpoint_handle, &sectread, 1 , NULL )!= SA_OK)
		return -1;

	if (sectread.readSize != sizeof (sect_id_array))
		return -1 ;
	
	if (strcmp (buffer, (char*)sect_id_array))
		return -1 ;

	
	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;
}

/*
case description :
        Try to overwrite to default section.  AIS supports this case. SA_OK will be returned.

returns :

               o : indicate success
              -1: fail
*/
int overwrite_default_section(void)
{

	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                           		&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&checkpoint_handle);

  
	if (ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	sect_id.id = NULL ;
	sect_id.idLen = 0 ;

	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      sizeof (sect_id_array)) ;

	if (ckpt_error != SA_OK )	
		{
			sect_id.id = sect_id_array ;
			sect_id.idLen = sizeof ( sect_id_array) ;

			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	/*check if the overwrite data has been write into the default section	 */
	sectread.sectionId.id =NULL ;
	sectread.sectionId.idLen = 0 ;
	sectread.dataBuffer = buffer ;
	sectread.dataSize = sizeof (sect_id_array) ;
	sectread.dataOffset = 0 ;
	sectread.readSize = 0 ; 	
				
	if (saCkptCheckpointRead (&checkpoint_handle, &sectread, 1 , NULL )!= SA_OK)
		return -1;

	if (sectread.readSize != sizeof (sect_id_array))
		return -1 ;
	
	if (strcmp (buffer, (char*)sect_id_array))
		return -1 ;
	

	sect_id.id = sect_id_array ;
	sect_id.idLen = sizeof ( sect_id_array) ;

	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;
 	
}

/*
case description :
        Checkpoint not opened in write mode. SA_ERR_ACCESS will be returned. 

returns :

               o : indicate success
              -1: fail
*/
int overwrite_err_access(void)
{
	SaCkptCheckpointHandleT tmp_handle;

	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                &sect_create_attri,  
				(void *) init_data, 
                                sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                          		&ckpt_name,
					&ckpt_create_attri , 
					SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ, 
					open_timeout,
					&tmp_handle);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	ckpt_error  = saCkptSectionOverwrite (&tmp_handle, 
					      &sect_id, sect_id_array, 
					      sizeof (sect_id_array)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptCheckpointClose (&tmp_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&tmp_handle);
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose (&tmp_handle) != SA_OK)
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
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;
}



/*
case description :
        Invocation with dataBuffer set to null. SA_INVALID_PARAM will be returned. 

returns :
               o : indicate success
              -1: fail
*/
int overwrite_null_databuffer(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                  &sect_create_attri,  
				  	(void *) init_data, 
                                  sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, NULL, 
					      sizeof (sect_id_array)) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;

}

/*

case description :
        Invocation with invalid dataSize. SA_INVALID_PARAM will be returned.

        1. negative dataSize

        2. dataSize exceeds maximum section size

returns :

               o : indicate success
              -1: fail
*/
int  overwrite_negative_datasize(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                  &sect_create_attri,  
				  	(void *) init_data, 
                                  sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      -1) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;

}

int  overwrite_big_datasize(void)
{
	/* dataSize exceeds maximum section size*/
	ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
      	
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                  &sect_create_attri,  
				 	(void *) init_data, 
                                  sizeof (init_data));
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error  = saCkptSectionOverwrite (&checkpoint_handle, 
					      &sect_id, sect_id_array, 
					      ckpt_create_attri.maxSectionSize + 1) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0;
}





int main(int argc, char* argv[])
{

	char case_name[] = "saCkptSectionOverwrite";
	char name_overwr[]="checkpoint_overwr";
	int case_index ;
		
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_overwr) ;
	memcpy (ckpt_name.value, name_overwr, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ;
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

	ckpt_io.sectionId.id = sect_id_array ;
	ckpt_io.sectionId.idLen = sect_id.idLen ;
	ckpt_io.dataBuffer = (void *)init_data ;
	ckpt_io.dataSize = sizeof (init_data) ;
	ckpt_io.dataOffset = 0 ;

	case_index = 0 ;

	if ( !overwrite_pre_open())
		printf ("%s overwrite_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_pre_open %d FAIL\n", case_name, case_index++);

	if ( !overwrite_null_handle())
		printf ("%s overwrite_null_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_null_handle %d FAIL\n", case_name, case_index++);

	if ( !overwrite_after_finalize())
		printf ("%s overwrite_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_after_finalize %d FAIL\n", case_name, case_index++);
	
	if ( !overwrite_after_close())
		printf ("%s overwrite_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_after_close %d FAIL\n", case_name, case_index++);
	
	if ( !overwrite_after_delete())
		printf ("%s overwrite_after_delete %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_after_delete %d FAIL\n", case_name, case_index++);

	if ( !overwrite_normal_call())
		printf ("%s overwrite_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_normal_call %d FAIL\n", case_name, case_index++);

	if ( !overwrite_default_section())
		printf ("%s overwrite_default_section %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_default_section %d FAIL\n", case_name, case_index++);

	if ( !overwrite_err_access())
		printf ("%s overwrite_err_access %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_err_access %d FAIL\n", case_name, case_index++);

	if ( !overwrite_null_databuffer())
		printf ("%s overwrite_null_databuffer %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_null_databuffer %d FAIL\n", case_name, case_index++);
	
	#if 0
	if ( !overwrite_negative_datasize())
		printf ("%s overwrite_negative_datasize %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_negative_datasize %d FAIL\n", case_name, case_index++);
	#endif
	
	if ( !overwrite_big_datasize())
		printf ("%s overwrite_big_datasize %d OK\n", case_name, case_index++);
	else
		printf ("%s overwrite_big_datasize %d FAIL\n", case_name, case_index++);

	return 0 ;

}

