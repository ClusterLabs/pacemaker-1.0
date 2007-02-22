/* 
 * write.c: Test data checkpoint API : saCkptCheckpointWrite
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

#include <stdio.h>
#include <unistd.h>
#include <saf/ais.h>
#include "ckpt_test.h"
#include <time.h>

void ckpt_open_callback (SaInvocationT invocation,
                         const SaCkptCheckpointHandleT *checkpointHandle,
                         SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int write_pre_open(void);
int write_pre_null_handle(void);
int write_after_delete(void);
int write_after_close(void);
int write_after_finalize(void);
int write_null_error_index(void);
int write_normal_call(void);
int write_default_section(void);
int write_err_access(void);
int  write_null_io_vector(void);
int write_invalid_sectid(void);
int write_invalid_databuff(void);
int write_negative_datasize(void);
int write_big_datasize(void);
int write_big_offset(void);

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
int write_pre_open(void)
{
        checkpoint_handle = -1 ;
        ckpt_error  = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,  NULL) ;

        if ( ckpt_error == SA_OK)
                return -1 ;
        return 0;
}

int write_pre_null_handle(void)
{
        ckpt_error = saCkptCheckpointWrite (NULL, &ckpt_io, 1,  NULL) ;
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
int write_after_delete(void)
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
                                          NULL,0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }
        
        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }
        
        /* write after section delete */
        ckpt_error  = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL) ;
        if (ckpt_error == SA_OK)
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        /* write  after close operation */
        if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
                {
                        saCkptFinalize (& ckpt_handle) ;
                        return -1 ;
                }

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;
        
        return 0;        
                
}

int write_after_close(void)
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
                                          NULL,0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }
        
        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        /* write after section delete */
        ckpt_error  = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL) ;
        if (ckpt_error == SA_OK)
                {
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;
        
        return 0;        
                
}

int write_after_finalize(void)
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
        
        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        ckpt_error  = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL) ;
        if (ckpt_error == SA_OK)
                {
                        return -1 ;        
                }


        return 0;        
                
}


/*
case description :
        Invoke with erroneousVectorIndex set to null. AIS support this case. SA_OK will be returned.
returns :
               o : indicate success
              -1: fail
*/

int write_null_error_index(void)
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

        if (saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,  NULL) != SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;
}

/*
case description :
        Invoke with correct steps and parameters.  SA_OK will be returned.
returns :
               o : indicate success
              -1: fail
*/
int write_normal_call(void)
{
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                      &sect_create_attri,  
                                          NULL, 0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        ckpt_error_index = 121 ;
        ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);
        if(ckpt_error != SA_OK || ckpt_error_index != 121)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;
}



/*
case description :
        Try to write to default section.  AIS supports this case. SA_OK will be returned.
returns :
               o : indicate success
              -1: fail
*/

int write_default_section(void)
{

        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_io.sectionId.id = NULL ;
        ckpt_io.sectionId.idLen = 0 ;

        ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1,&ckpt_error_index);

        if(ckpt_error != SA_OK )
                {
                        ckpt_io.sectionId.id = sect_id_array ;
                        ckpt_io.sectionId.idLen = sizeof (sect_id_array) ;
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        ckpt_io.sectionId.id = sect_id_array ;
        ckpt_io.sectionId.idLen = sizeof (sect_id_array) ;

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
        Checkpoint not opened in write mode. SA_ERR_ACCESS will be returned. 
returns :
               o : indicate success
              -1: fail
*/

int write_err_access(void)
{
        SaCkptCheckpointHandleT tmp_handle;

        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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
        
        ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_READ,
                                           open_timeout,
                                           &tmp_handle);
  
        if (ckpt_error != SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        ckpt_error = saCkptCheckpointWrite (&tmp_handle, &ckpt_io, 1,&ckpt_error_index);
        if(ckpt_error == SA_OK)
                {
                        saCkptCheckpointClose (&tmp_handle) ;
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if (saCkptCheckpointClose (&tmp_handle) != SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;

}

/*
case description :
        Invocation with ioVector set to null. SA_ERR_INVALID_PARAM will be returned. 
returns :
               o : indicate success
              -1: fail
*/
int  write_null_io_vector(void)
{
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                         &sect_create_attri,  
                                          NULL, 0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, NULL, 1, NULL);
        if(ckpt_error == SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;

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

int write_invalid_sectid(void)
{
        SaUint8T sect_tmp_array[] = "12345679";
        /*int tmp ; */
        
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                      &sect_create_attri,  
                                        NULL,0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        /* sectionId not exist*/
        ckpt_io.sectionId.id = sect_tmp_array ;
        ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL);
        ckpt_io.sectionId.id = sect_id_array ;
        if(ckpt_error == SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;

}

int write_invalid_databuff(void)
{
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri,  
                                          NULL, 0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        /* sectionId not exist*/
        ckpt_io.dataBuffer = NULL ;
        ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL);
        ckpt_io.dataBuffer = (void *) init_data ;
        if(ckpt_error == SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;
}

int write_negative_datasize(void)
{
        int tmp;
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri,  
                                          NULL,0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        /* sectionId not exist*/
        tmp = ckpt_io.dataSize ;
        ckpt_io.dataSize = -1 ;
        ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, &ckpt_io, 1, NULL);
        ckpt_io.dataSize = tmp ;
        if(ckpt_error == SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;

}

int write_big_datasize(void)
{
        int tmp;
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri,  
                                          NULL, 0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        /* datasize value exceeds the maximum section size*/
        tmp = ckpt_io.dataSize ;
        ckpt_io.dataSize = ckpt_create_attri.maxSectionSize+1;
        ckpt_error = saCkptCheckpointWrite (& checkpoint_handle, &ckpt_io, 1 , &ckpt_error_index) ;
        
        ckpt_io.dataSize=tmp ;
        if(ckpt_error == SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;

}

int write_big_offset(void)
{
        int tmp;
        ckpt_error = saCkptInitialize (&ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

        ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri,  
                                          NULL,0);
        if (ckpt_error != SA_OK )
                {
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;
                }

        /*dataOffset + dataSize>  maximum section size */
        tmp = ckpt_io.dataOffset;
        ckpt_io.dataOffset= ckpt_create_attri.maxSectionSize -ckpt_io.dataSize + 1;
        ckpt_error = saCkptCheckpointWrite (& checkpoint_handle, &ckpt_io, 1 ,  &ckpt_error_index) ;
        
        ckpt_io.dataOffset=tmp ;
        if(ckpt_error == SA_OK)
                {
                        saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
                        saCkptCheckpointClose (&checkpoint_handle) ;
                        saCkptFinalize (&ckpt_handle) ;
                        return -1 ;        
                }

        if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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

        if ( saCkptFinalize (& ckpt_handle) != SA_OK )
                return -1 ;

        return 0 ;


}

int main(int argc, char* argv[])
{

        char case_name[] = "saCkptCheckpointWrite";
        char name_wr[]="checkpoint_wr";
        int case_index ;
                
        ckpt_version.major = VERSION_MAJOR;
        ckpt_version.minor = VERSION_MINOR;
        ckpt_version.releaseCode = VERSION_RELEASCODE ;

        ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
        ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

        ckpt_name.length = sizeof (name_wr) ;

        memcpy (ckpt_name.value, name_wr, ckpt_name.length);

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
        
        case_index = 0 ;

        if ( !write_pre_open())
                printf ("%s write_pre_open %d OK\n", case_name, case_index++);
        else
                printf ("%s write_pre_open %d FAIL\n", case_name, case_index++);

        if ( !write_pre_null_handle())
                printf ("%s write_pre_null_handle %d OK\n", case_name, case_index++);
        else
                printf ("%s write_pre_null_handle %d FAIL\n", case_name, case_index++);

        if ( !write_after_delete())
                printf ("%s write_after_delete %d OK\n", case_name, case_index++);
        else
                printf ("%s write_after_delete %d FAIL\n", case_name, case_index++);

        if ( !write_after_close())
                printf ("%s write_after_close %d OK\n", case_name, case_index++);
        else
                printf ("%s write_after_close %d FAIL\n", case_name, case_index++);

        if ( !write_after_finalize())
                printf ("%s write_after_finalize %d OK\n", case_name, case_index++);
        else
                printf ("%s write_after_finalize %d FAIL\n", case_name, case_index++);

        if ( !write_null_error_index())
                printf ("%s write_null_error_index %d OK\n", case_name, case_index++);
        else
                printf ("%s write_null_error_index %d FAIL\n", case_name, case_index++);

        if ( !write_normal_call())
                printf ("%s write_normal_call %d OK\n", case_name, case_index++);
        else
                printf ("%s write_normal_call %d FAIL\n", case_name, case_index++);

        if ( !write_default_section())
                printf ("%s write_default_section %d OK\n", case_name, case_index++);
        else
                printf ("%s write_default_section %d FAIL\n", case_name, case_index++);

        if (!write_err_access())
                printf ("%s write_err_access %d OK\n", case_name, case_index++);
        else
                printf ("%s write_err_access %d FAIL\n", case_name, case_index++);

        if (!write_null_io_vector())
                printf ("%s write_null_io_vector %d OK\n", case_name, case_index++);
        else
                printf ("%s write_null_io_vector %d FAIL\n", case_name, case_index++);

        if (!write_invalid_sectid())
                printf ("%s write_invalid_sectid %d OK\n", case_name, case_index++);
        else
                printf ("%s write_invalid_sectid %d FAIL\n", case_name, case_index++);

        if (!write_invalid_databuff())
                printf ("%s write_invalid_databuff %d OK\n", case_name, case_index++);
        else
                printf ("%s write_invalid_databuff %d FAIL\n", case_name, case_index++);

        #if 0
        if (!write_negative_datasize())
                printf ("%s write_negative_datasize %d OK\n", case_name, case_index++);
        else
                printf ("%s write_negative_datasize %d FAIL\n", case_name, case_index++);
        #endif
        
        if (!write_big_datasize())
                printf ("%s write_big_datasize %d OK\n", case_name, case_index++);
        else
                printf ("%s write_big_datasize %d FAIL\n", case_name, case_index++);

        if (!write_big_offset())
                printf ("%s write_big_offset %d OK\n", case_name, case_index++);
        else
                printf ("%s write_big_offset %d FAIL\n", case_name, case_index++);

        return 0 ;
        
}
