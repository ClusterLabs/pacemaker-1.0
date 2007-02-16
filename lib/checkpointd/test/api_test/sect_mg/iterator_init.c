/* 
 * iterator_init.c: data checkpoint API test: saCkptSectionIteratorInitialize
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
int  init_pre_open(void);
int init_null_handle(void);
int init_after_close(void);
int init_after_finalize(void);
int init_null_iterator(void);
int init_choosen_flag_forever(void);
int init_choosen_flag_any(void);
int init_choosen_flag_leq(void);
int init_choosen_flag_geq(void);



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
int  init_pre_open(void)
{
	checkpoint_handle = -1 ;
	ckpt_error = saCkptSectionIteratorInitialize (& checkpoint_handle, SA_CKPT_SECTIONS_ANY, cur_time, & ckpt_iterator) ;
		
	if ( ckpt_error == SA_OK)
		return -1 ;

	return 0 ;
}

int init_null_handle(void)
{
	ckpt_error = saCkptSectionIteratorInitialize (NULL, SA_CKPT_SECTIONS_ANY, cur_time, & ckpt_iterator) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	return 0;	
}

/*
case description :
        Invoke with invalid checkpoint handle. Two cases will be included:

         after close operation


returns :
               o : indicate success
              -1: fail
*/
int init_after_close(void)
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


	/* call after close operation */
	if (saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptSectionIteratorInitialize (& checkpoint_handle, SA_CKPT_SECTIONS_ANY, cur_time, & ckpt_iterator) ;
	if( ckpt_error == SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	
	return 0;
}

int init_after_finalize(void)
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
	
	ckpt_error = saCkptSectionIteratorInitialize (&checkpoint_handle, 
		SA_CKPT_SECTIONS_ANY, cur_time, & ckpt_iterator) ;

	if( ckpt_error == SA_OK)
		{
			return -1 ;
		}
	
	return 0;
}

/* 
ase description :
        Invoke with null pointer of SaCkptSectionIteratorT.  

returns :
               o : indicate success
              -1: fail
*/
int init_null_iterator(void)
{
	time_t cur_time = 0;	

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

	ckpt_error = saCkptSectionIteratorInitialize (& checkpoint_handle, 
						SA_CKPT_SECTIONS_ANY, 
						cur_time, 
						NULL) ;
	if (ckpt_error == SA_OK)
		{
			saCkptSectionIteratorFinalize (& ckpt_iterator) ;
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
				saCkptFinalize (& ckpt_handle) ;
				return -1 ;
		}
	
	if (saCkptFinalize (& ckpt_handle) != SA_OK)
		return -1 ;
	
	return 0 ;
	
}


int init_choosen_flag_forever(void)
{
	SaUint8T tmp_id_array[1];
	int  i, j ;
	SaUint8T	id_value ;
	time_t cur_time = 0;		
	sect_id.id = tmp_id_array ;
	sect_id.idLen = sizeof (tmp_id_array) ;
	sect_create_attri.sectionId = &sect_id ;
	time (&cur_time) ;

	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_WRITE ,
					   open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* start of creation sections */
	i = 1 ;
	tmp_id_array[0]=i++;
	/* expiration time set to SA_TIME_END */
	sect_create_attri.expirationTime= SA_TIME_END ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  	(void *) init_data, sizeof (init_data));
	if (ckpt_error != SA_OK)
	{
		for (j=0 ; j< i ; j++)
			{
			        tmp_id_array[0]= j ;
				saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
			}
		saCkptCheckpointClose (&checkpoint_handle) ;
		saCkptFinalize (&ckpt_handle) ;
		return -1 ;
	}

	/* expiration time set to SA_TIME_END */
	tmp_id_array[0] = i++ ;
	sect_create_attri.expirationTime= SA_TIME_END ;
	
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  	(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	/* expiration time set to current time plus two hours */
	time (&cur_time) ;
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time + 3600*2)*1000000000LL);
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					 	 (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
			       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  	(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
				tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId);
			}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus 24 hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*24)*1000000000LL);
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
				{
					tmp_id_array[0]= j ;
					saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId);
					saCkptCheckpointClose ( & checkpoint_handle) ;
					saCkptFinalize (& ckpt_handle) ;
					return -1 ;
				}
		}
	/* end  of creation sections*/

	
/* start of Forever flag */
	ckpt_error = saCkptSectionIteratorInitialize (&checkpoint_handle, 
                                                      SA_CKPT_SECTIONS_FOREVER, 
                                                      0, 
                                                      &ckpt_iterator) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* two sections will be chosen if correct */
	j=0 ;
	while ( saCkptSectionIteratorNext (&ckpt_iterator, &ckpt_desc) == SA_OK)
		{
			if (ckpt_desc.sectionId.id == NULL) {
				continue;
			}
			id_value = ckpt_desc.sectionId.id[0] ;
			if ( ckpt_desc.expirationTime != SA_TIME_END)
				break; 
			j++ ;				
		}
	/* Chosen error */
	if ( j != 2)  
		{
			saCkptSectionIteratorFinalize (& ckpt_iterator) ;
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
				}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionIteratorFinalize (& ckpt_iterator) != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
					saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
				}

			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	for (j=1 ; j<= 5 ; j++)
		{
			tmp_id_array[0]= j ;
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
		}

	if ( saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
				saCkptFinalize (& ckpt_handle) ;
				return -1 ;
		}
	
	if (saCkptFinalize (& ckpt_handle) != SA_OK)
		return -1 ;

	return 0;

}
	  
/* end of Forever flag */
int init_choosen_flag_any(void)
{
	SaUint8T tmp_id_array[1];
	int  i, j ;
	SaUint8T	id_value ;
	time_t cur_time;	

	sect_id.id = tmp_id_array ;
	sect_id.idLen = sizeof (tmp_id_array) ;
	sect_create_attri.sectionId = &sect_id ;

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                        &ckpt_name,  
                                        &ckpt_create_attri , 
                                        SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_WRITE ,
				   	open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* start o f creation sections */
	i = 1 ;
	tmp_id_array[0] = i++ ;
	/* expiration time set to SA_TIME_END */
	sect_create_attri.expirationTime= SA_TIME_END ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data));
	if (ckpt_error != SA_OK)
	{
		for (j=0 ; j< i ; j++)
			{
			        tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
		saCkptCheckpointClose (&checkpoint_handle) ;
		saCkptFinalize(&ckpt_handle) ;
		return -1 ;
	}

	/* expiration time set to SA_TIME_END */
	tmp_id_array[0] = i++ ;
	sect_create_attri.expirationTime= SA_TIME_END ;
	
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;
		}
	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	time (&cur_time) ;
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
			       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
				tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId);
			}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus 24 hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*24)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
				{
					tmp_id_array[0]= j ;
					saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
					saCkptCheckpointClose(&checkpoint_handle) ;
					saCkptFinalize (&ckpt_handle) ;
					return -1 ;
				}
		}
	/* end  of creation sections*/

	ckpt_error = saCkptSectionIteratorInitialize (&checkpoint_handle, 
                                                      SA_CKPT_SECTIONS_ANY, 
                                                      0, 
                                                      &ckpt_iterator) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* Five sections will be chosen if correct */
	j=0 ;
	while ( saCkptSectionIteratorNext (&ckpt_iterator, &ckpt_desc) == SA_OK)
		{
			if (ckpt_desc.sectionId.id == NULL) {
				continue;
			}
			id_value = ckpt_desc.sectionId.id[0] ;
			j++ ;				
		}

	/* Chosen error */
	if ( j != 5)  
		{
			saCkptSectionIteratorFinalize (&ckpt_iterator) ;
			for (j=0 ; j< 5 ; j++)
				{
					tmp_id_array[0]= j ;
					saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId);

				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionIteratorFinalize (& ckpt_iterator) != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
					tmp_id_array[0]= j ;
					saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
				}

			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	for (j=1 ; j<= 5 ; j++)
		{
			tmp_id_array[0]= j ;
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
		}

	if ( saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
				saCkptFinalize (& ckpt_handle) ;
				return -1 ;
		}
	
	if (saCkptFinalize (& ckpt_handle) != SA_OK)
		return -1 ;
	
	return 0;

}

/* end  of ANY flag */	
int init_choosen_flag_leq(void)
{
	SaUint8T tmp_id_array[1];
	int  i, j ;
	SaUint8T	id_value ;
	time_t cur_time;	
	
	sect_id.id = tmp_id_array ;
	sect_id.idLen = sizeof (tmp_id_array) ;
	sect_create_attri.sectionId = &sect_id ;

	time (&cur_time) ;

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_WRITE ,
					  	 open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* start o f creation sections */
	i = 1 ;
	tmp_id_array[0] = i++ ;

	/* expiration time set to SA_TIME_END */
	sect_create_attri.expirationTime= SA_TIME_END ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data));
	if (ckpt_error != SA_OK)
	{
		for (j=0 ; j< i ; j++)
			{
			        tmp_id_array[0]= j ;
				saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
			}
		saCkptCheckpointClose ( & checkpoint_handle) ;
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}

	/* expiration time set to SA_TIME_END */
	tmp_id_array[0] = i++ ;
	sect_create_attri.expirationTime= SA_TIME_END ;
	
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
			       saCkptSectionDelete( & checkpoint_handle,sect_create_attri.sectionId);
			}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
			saCkptCheckpointClose(&checkpoint_handle) ;
			saCkptFinalize(&ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus 24 hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*24)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				saCkptCheckpointClose (&checkpoint_handle) ;
				saCkptFinalize (&ckpt_handle) ;
				return -1 ;
			}
		}
	/* end  of creation sections*/

/* start of LEQ flag */
	ckpt_error = saCkptSectionIteratorInitialize (&checkpoint_handle, 
                                                      SA_CKPT_SECTIONS_LEQ_EXPIRATION_TIME,
                                                      (SaTimeT)((cur_time + 3600 *3)*1000000000LL), 
                                                      &ckpt_iterator) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
					saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* Two sections will be chosen if correct */
	j=0 ;
	while ( saCkptSectionIteratorNext (& ckpt_iterator, & ckpt_desc) == SA_OK)
		{
			if (ckpt_desc.sectionId.id == NULL) {
				continue;
			}
			id_value = ckpt_desc.sectionId.id[0] ;
			if ( ckpt_desc.expirationTime != (SaTimeT)((cur_time + 3600*2)*1000000000LL))
				break; 
			j++ ;				
		}

	/* Chosen error */
	if ( j != 2)  
		{
			saCkptSectionIteratorFinalize(&ckpt_iterator) ;
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionIteratorFinalize (& ckpt_iterator) != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
		   	       	tmp_id_array[0]= j ;
					saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
				}

			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	for (j=1 ; j<= 5 ; j++)
		{
		       tmp_id_array[0]= j ;
			saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
		}

	if ( saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
				saCkptFinalize (& ckpt_handle) ;
				return -1 ;
		}
	
	if (saCkptFinalize (& ckpt_handle) != SA_OK)
		return -1 ;

	return 0;
}

/* end of LEQ flag*/
int init_choosen_flag_geq(void)
{
	SaUint8T tmp_id_array[1];
	int  i, j ;
	SaUint8T	id_value ;
	time_t cur_time;	
	
	sect_id.id = tmp_id_array ;
	sect_id.idLen = sizeof (tmp_id_array) ;
	sect_create_attri.sectionId = &sect_id ;

	time(&cur_time) ;

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_WRITE ,
					   	open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* start o f creation sections */
	i = 1 ;
	tmp_id_array[0] = i++ ;

	/* expiration time set to SA_TIME_END */
	sect_create_attri.expirationTime= SA_TIME_END ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data));
	if (ckpt_error != SA_OK)
	{
		for (j=0 ; j< i ; j++)
			{
			        tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
		saCkptCheckpointClose (&checkpoint_handle) ;
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}

	/* expiration time set to SA_TIME_END */
	tmp_id_array[0] = i++ ;
	sect_create_attri.expirationTime= SA_TIME_END ;
	
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
			       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus two hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*2)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
				saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
			}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* expiration time set to current time plus 24 hours */
	tmp_id_array[0] = i++ ;
	
	sect_create_attri.expirationTime= (SaTimeT)((cur_time+ 3600*24)*1000000000LL) ;
	ckpt_error = saCkptSectionCreate (& checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< i ; j++)
			{
			       tmp_id_array[0]= j ;
				saCkptSectionDelete( & checkpoint_handle, sect_create_attri.sectionId);
				saCkptCheckpointClose(&checkpoint_handle) ;
				saCkptFinalize (&ckpt_handle) ;
				return -1 ;
			}
		}
	/* end  of creation sections*/

/* start of GEQ flag */
	ckpt_error = saCkptSectionIteratorInitialize (&checkpoint_handle, 
						      SA_CKPT_SECTIONS_GEQ_EXPIRATION_TIME, 
						      (SaTimeT)((cur_time + 3600 *3)*1000000000LL), 
						      &ckpt_iterator) ;
	if (ckpt_error != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
				       saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId);
				}
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	/* One sections will be chosen if correct */
	j=0 ;
	while ( saCkptSectionIteratorNext (&ckpt_iterator, &ckpt_desc) == SA_OK)
		{
			if (ckpt_desc.sectionId.id == NULL) {
				continue;
			}
			
			id_value = ckpt_desc.sectionId.id[0] ;
			if ( ckpt_desc.expirationTime != (SaTimeT)((cur_time + 3600*24)*1000000000LL))
				break; 
			j++ ;				
		}

	/* Chosen error */
	if ( j != 1)  
		{
			saCkptSectionIteratorFinalize (&ckpt_iterator) ;
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
					saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);

				}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptSectionIteratorFinalize (& ckpt_iterator) != SA_OK)
		{
			for (j=0 ; j< 5 ; j++)
				{
				       tmp_id_array[0]= j ;
					saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId);
				}
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;

		}
	
/* end of GEQ flag */
	for (j=1 ; j<= 5 ; j++)
		{
		       tmp_id_array[0]= j ;
			saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId);

		}
	if ( saCkptCheckpointClose (&checkpoint_handle) != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	
	if (saCkptFinalize (& ckpt_handle) != SA_OK)
		return -1 ;

	return 0 ;
}

int main(int argc, char* argv[])
{
	char case_name[] = "saCkptSectionIteratorInitialize";
	char name_ite_in[]="checkpoint_ite_in";
	int case_index ;
	
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_ite_in) ;

	memcpy (ckpt_name.value, name_ite_in, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	case_index = 0 ;
	if ( !init_pre_open())
		printf ("%s init_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s init_pre_open %d FAIL\n", case_name, case_index++);

	if ( !init_null_handle())
		printf ("%s init_null_handle %d OK\n", case_name, case_index++);
	else
		printf ("%s init_null_handle %d FAIL\n", case_name, case_index++);

	if ( !init_after_finalize())
		printf ("%s init_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s init_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !init_null_iterator())
		printf ("%s init_null_iterator %d OK\n", case_name, case_index++);
	else
		printf ("%s init_null_iterator %d FAIL\n", case_name, case_index++);

	if ( !init_choosen_flag_forever())
		printf ("%s init_choosen_flag_forever %d OK\n", case_name, case_index++);
	else
		printf ("%s init_choosen_flag_forever %d FAIL\n", case_name, case_index++);

	if ( !init_choosen_flag_any())
		printf ("%s init_choosen_flag_any %d OK\n", case_name, case_index++);
	else
		printf ("%s init_choosen_flag_any %d FAIL\n", case_name, case_index++);

	if ( !init_choosen_flag_leq())
		printf ("%s init_choosen_flag_leq %d OK\n", case_name, case_index++);
	else
		printf ("%s init_choosen_flag_leq %d FAIL\n", case_name, case_index++);

	if ( !init_choosen_flag_geq())
		printf ("%s init_choosen_flag_geq %d OK\n", case_name, case_index++);
	else
		printf ("%s init_choosen_flag_geq %d FAIL\n", case_name, case_index++);

	return 0 ;
}
