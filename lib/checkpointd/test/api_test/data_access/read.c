/* 
 * read.c: Test data checkpoint API : saCkptCheckpointRead
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
			 const SaCkptCheckpointHandleT *checkpointHandle,
			 SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int read_pre_open(void);
int read_null_handle(void);
int read_after_finalize(void);
int read_after_close(void);
int read_after_delete(void);
int read_error_index(void);
int read_normal_call(void);
int read_default_section(void);
int read_err_access(void);
int  read_null_io_vector(void);
int read_invalid_number(void);
int read_invalid_sectid(void);
int read_null_databuf(void);
int read_negative_datasize(void);
int read_big_datasize(void);
int read_negative_offset(void);
int read_big_offset(void);


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
int read_pre_open(void)
{	
	checkpoint_handle = -1 ;
	memset(io_read.dataBuffer, 0, 20) ;	
	ckpt_error  = saCkptCheckpointRead(& checkpoint_handle, & io_read, 1,  &ckpt_error_index) ;

	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0;	
}

int read_null_handle(void)
{
	memset(io_read.dataBuffer, 0, 20) ;
	ckpt_error = saCkptCheckpointRead (NULL, & io_read, 1,  &ckpt_error_index) ;
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
int read_after_finalize(void)
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
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

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            &io_read, 
                                            1, 
                                            &ckpt_error_index) ;
	if (ckpt_error== SA_OK)
		return -1;	

	return 0;

}

int read_after_close(void)
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            &io_read, 
                                            1, 
                                            &ckpt_error_index) ;
	if (ckpt_error== SA_OK)
		{	
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
	
}

int read_after_delete(void)
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
				  	  NULL,0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
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
	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            &io_read, 
                                            1, 
                                            &ckpt_error_index) ;
	if (ckpt_error== SA_OK)
		{	
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}


	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
	
}

/*
case description :
        Invoke with erroneousVectorIndex set to null. AIS support this case. SA_OK will be returned.
returns :
               o : indicate success
              -1: fail
*/

int read_error_index(void)
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            &io_read, 
                                            1, 
                                            NULL) ;
	if (ckpt_error!= SA_OK)
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
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
int read_normal_call(void)
{
	memset(io_read.dataBuffer, 20, 0) ;
	
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            &io_read, 
                                            1, 
                                            &ckpt_error_index) ;
	if (ckpt_error!= SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (io_read.readSize != io_read.dataSize || memcmp(io_read.dataBuffer, ckpt_io.dataBuffer, io_read.dataSize))
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;

}



/*
case description :
        Try to read to default section.  AIS supports this case. SA_OK will be returned.
returns :
               o : indicate success
              -1: fail
*/

int read_default_section(void)
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
	 ckpt_io.sectionId.id = NULL ;
	 ckpt_io.sectionId.idLen = 0 ;
	
	if (saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL) != SA_OK)
		{ 
				ckpt_io.sectionId.id = sect_id_array ;
				ckpt_io.sectionId.idLen = sizeof (sect_id_array) ;
				
				saCkptCheckpointClose ( & checkpoint_handle) ;
				saCkptFinalize (& ckpt_handle) ;
				return -1 ;	
		}
	ckpt_io.sectionId.id = sect_id_array ;
	ckpt_io.sectionId.idLen = sizeof (sect_id_array) ;

	io_read.sectionId.id = NULL ;
	io_read.sectionId.idLen = 0 ;
	
	if ((ckpt_error  = saCkptCheckpointRead (&checkpoint_handle, &io_read, 1, NULL)) != SA_OK)
		{
			io_read.sectionId.id = sect_id_array ;
			io_read.sectionId.idLen = ckpt_io.sectionId.idLen ;
			
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	io_read.sectionId.id = sect_id_array ;
	io_read.sectionId.idLen = ckpt_io.sectionId.idLen ;
			
	/* compare datasize and content */

	if (io_read.readSize != io_read.dataSize || memcmp(io_read.dataBuffer, ckpt_io.dataBuffer, io_read.dataSize))
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
        Checkpoint not opened in read mode. SA_ERR_ACCESS will be returned. 
returns :
               o : indicate success
              -1: fail
*/
int read_err_access(void)
{
	memset(io_read.dataBuffer, 20, 0) ;
	
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            &io_read, 
                                            1, 
                                            &ckpt_error_index) ;
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;

}

/*
case description :
        Invocation with ioVector set to null. SA_ERR_INVALID_PARAM will be returned. 
returns :
               o : indicate success
              -1: fail
*/

int  read_null_io_vector(void)
{
	memset(io_read.dataBuffer, 20, 0) ;
	
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
				  	  NULL,0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            NULL, 
                                            1, 
                                            NULL) ;
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;

}

/*
case description :
        Parameter erroneousVectorIndex exceeds the exact number of the elements. SA_ERR_INVALID_PARAM will be returned. 
returns :
               o : indicate success
              -1: fail
*/
int read_invalid_number(void)
{
	memset(io_read.dataBuffer, 20, 0) ;
	
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}

	ckpt_error  = saCkptCheckpointRead (&checkpoint_handle,
                                            NULL, 
                                            1, 
                                            NULL) ;
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;

}

/*
case description :
        Invocation with invalid ioVector parameter. SA_ERR_INVALID_PARAM will be returned.
        Four sub-cases will be included:
        1. sectionId not exist
        2. null databuffer 
        3. negative datasize , or datasize value exceeds the maximum section size
        4. a. negative dataoffet
            b. dataOffset value exceeds the maximum section size.
            c. dataOffset + dataSize>  maximum section size
returns :
               o : indicate success
              -1: fail
*/

int read_invalid_sectid(void)
{
	SaUint8T sect_tmp_array[] = "12345679";
	memset(io_read.dataBuffer, 20, 0) ;
	
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}

	io_read.sectionId.id = sect_tmp_array ;
	ckpt_error = saCkptCheckpointRead (& checkpoint_handle, &io_read, 1 ,  &ckpt_error_index) ;
	
	io_read.sectionId.id = sect_id_array ;
	
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;

}

int read_null_databuf(void)
{	

	char *ptr_tmp ;
	
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	ptr_tmp = (char *) io_read.dataBuffer ;
	io_read.dataBuffer = NULL ;
	ckpt_error = saCkptCheckpointRead (& checkpoint_handle, &io_read, 1 ,  &ckpt_error_index) ;
	ckpt_io.dataBuffer = (void *) ptr_tmp ;
	
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
}

int read_negative_datasize(void)
{
	int tmp;
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	tmp = io_read.dataSize ;
	io_read.dataSize = -1 ;
	ckpt_error = saCkptCheckpointRead (& checkpoint_handle, &io_read, 1 ,  &ckpt_error_index) ;
	io_read.dataSize = tmp ;
	if (ckpt_error!= SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;

}

int read_big_datasize(void)
{
	int tmp;
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
				  	  NULL, 
                                          0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	tmp = io_read.dataSize ;
	io_read.dataSize = ckpt_create_attri.maxSectionSize+1;
	ckpt_error = saCkptCheckpointRead (& checkpoint_handle, &io_read, 1 ,  &ckpt_error_index) ;
	io_read.dataSize = tmp ;
	if (ckpt_error!= SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
}

int read_negative_offset(void)
{
	int tmp;
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	tmp = io_read.dataOffset;
	io_read.dataOffset = -1;
	ckpt_error = saCkptCheckpointRead (& checkpoint_handle, &io_read, 1 ,  &ckpt_error_index) ;
	io_read.dataOffset=tmp ;
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
}

int read_big_offset(void)
{
	int tmp;
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
				  	  NULL, 0);
	if (ckpt_error != SA_OK )
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
	if (ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle);
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;	
		}
	
	tmp = io_read.dataOffset;
	io_read.dataOffset = ckpt_create_attri.maxSectionSize+1;
	ckpt_error = saCkptCheckpointRead (& checkpoint_handle, &io_read, 1 ,  &ckpt_error_index) ;
	io_read.dataOffset=tmp ;
	if (ckpt_error== SA_OK )
		{	
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
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
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
}

int main(int argc, char* argv[]) 
{
	char case_name[] = "saCkptCheckpointRead";
	char name_rd[]="checkpoint_rd";
	char buffer[20] ;
	int case_index ;
		
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_rd) ;

	memcpy (ckpt_name.value,name_rd, ckpt_name.length);

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

	ckpt_io.sectionId.id = sect_id_array ;
	ckpt_io.sectionId.idLen = sect_id.idLen ;
	ckpt_io.dataBuffer = (void *)init_data ;
	ckpt_io.dataSize = sizeof (init_data) ;
	ckpt_io.dataOffset = 0 ;

	io_read.sectionId.id = ckpt_io.sectionId.id ;
	io_read.sectionId.idLen = ckpt_io.sectionId.idLen ;
	io_read.dataBuffer = (void *) buffer ;
	io_read.dataOffset = 0 ;
	io_read.dataSize = sizeof (init_data) ;
	io_read.readSize = 0 ;
	
	case_index = 0 ;

	if ( !read_pre_open())
		printf ("%s read_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s read_pre_open %d FAIL\n", case_name, case_index++);

	if ( !read_null_handle())
		printf ("%s read_null_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s read_null_handle %d FAIL\n", case_name, case_index++);

	if ( !read_after_finalize())
		printf ("%s read_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s read_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !read_after_close())
		printf ("%s read_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s read_after_close %d FAIL\n", case_name, case_index++);

	if ( !read_after_delete())
		printf ("%s read_after_delete %d OK\n", case_name, case_index++);
	else
		printf ("%s read_after_delete %d FAIL\n", case_name, case_index++);
	
	if ( !read_error_index())
		printf ("%s read_error_index %d OK\n", case_name, case_index++);
	else
		printf ("%s read_error_index %d FAIL\n", case_name, case_index++);

	if ( !read_normal_call())
		printf ("%s read_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s read_normal_call %d FAIL\n", case_name, case_index++);

	if ( !read_default_section())
		printf ("%s read_default_section %d OK\n", case_name, case_index++);
	else
		printf ("%s read_default_section %d FAIL\n", case_name, case_index++);

	if ( !read_err_access())
		printf ("%s read_err_access %d OK\n", case_name, case_index++);
	else
		printf ("%s read_err_access %d FAIL\n", case_name, case_index++);

	if ( !read_invalid_number())
		printf ("%s read_invalid_number %d OK\n", case_name, case_index++);
	else
		printf ("%s read_invalid_number %d FAIL\n", case_name, case_index++);

	if ( !read_null_io_vector())
		printf ("%s read_null_io_vector %d OK\n", case_name, case_index++);
	else
		printf ("%s read_null_io_vector %d FAIL\n", case_name, case_index++);
	
#if 0	
	if ( !read_null_databuf())
		printf ("%s read_null_databuf %d OK\n", case_name, case_index++);
	else
		printf ("%s read_null_databuf %d FAIL\n", case_name, case_index++);
#endif
	if ( !read_negative_datasize())
		printf ("%s read_negative_datasize %d OK\n", case_name, case_index++);
	else
		printf ("%s read_negative_datasize %d FAIL\n", case_name, case_index++);
	
	if ( !read_big_datasize())
		printf ("%s read_big_datasize %d OK\n", case_name, case_index++);
	else
		printf ("%s read_big_datasize %d FAIL\n", case_name, case_index++);

	if ( !read_negative_offset())
		printf ("%s read_negative_offset %d OK\n", case_name, case_index++);
	else
		printf ("%s read_negative_offset %d FAIL\n", case_name, case_index++);

	if ( !read_big_offset())
		printf ("%s read_big_offset %d OK\n", case_name, case_index++);
	else
		printf ("%s read_big_offset %d FAIL\n", case_name, case_index++);

	return 0 ;
}
