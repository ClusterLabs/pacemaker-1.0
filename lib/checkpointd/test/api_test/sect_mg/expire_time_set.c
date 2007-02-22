/* 
 * expire_time_set.c: data checkpoint API test: saCkptSectionExpirationTimeSet
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

SaTimeT expire_time;

void ckpt_open_callback (SaInvocationT invocation,
			const SaCkptCheckpointHandleT *checkpointHandle,SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int  set_pre_open(void);
int set_null_handle(void);
int set_after_finalize(void);
int set_after_close(void);
int set_after_delete(void);
int set_err_access(void);
int set_not_exist(void);
int set_default_section(void);
int set_invalid_time(void);
int set_normal_call(void);


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
int  set_pre_open(void)
{
	checkpoint_handle = -1 ;
 	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, &sect_id,expire_time);
	if ( ckpt_error == SA_OK)
		return -1 ;
	return 0;
}

int set_null_handle(void)
{
	ckpt_error = saCkptSectionExpirationTimeSet (NULL, &sect_id, expire_time);
	if ( ckpt_error == SA_OK)
		return -1 ;
	
	return 0 ;
}

int set_after_finalize(void)
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
					open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, &sect_create_attri ,  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
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

	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, 
						&sect_id, 
						 expire_time);
	if( ckpt_error == SA_OK)
		{
			return -1 ;
		}
	return 0;
}

int set_after_close(void)
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
					open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, &sect_create_attri ,  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
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
	
	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, 
                                                     &sect_id, 
                                                     expire_time);
	if( ckpt_error == SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;
	return 0;

}

int set_after_delete(void)
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
					open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, &sect_create_attri ,  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	if ( saCkptSectionDelete( & checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, 
                                                     &sect_id, 
                                                     expire_time);
	if( ckpt_error == SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle);
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
	
	return 0;

}


/*
case description :
        Open not in write mode. SA_ERR_ACCESS will be returned.

returns :
               o : indicate success
              -1: fail
*/
int set_err_access(void)
{

	SaCkptHandleT tmp_handle ;
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, &sect_create_attri ,  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					   	&ckpt_name, 
					   	&ckpt_create_attri , 
                                     	SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_READ,  
					  	 open_timeout, 
                                           &tmp_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId);
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptCheckpointClose (&tmp_handle ) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionExpirationTimeSet (&tmp_handle, &sect_id, expire_time) != SA_OK)
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

        The section to be set , identified by sectionId, does not exist . Three cases will be included:

        1. Delete with non exist section id
        2. set after delete
        3. null sectionId pointer

returns :
               o : indicate success
              -1: fail
*/
int set_not_exist(void)
{
	SaUint8T sect_tmp_id[] = "12345679";

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					   	&ckpt_name,  
                             		&ckpt_create_attri , 
                                     	SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					   	open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}
	ckpt_error = saCkptSectionCreate (&checkpoint_handle, &sect_create_attri ,  
					  (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	
	sect_id.id = sect_tmp_id ;
	ckpt_error= saCkptSectionExpirationTimeSet (& checkpoint_handle, & sect_id, expire_time);
	if (ckpt_error == SA_OK) 
		{
			sect_id.id = sect_id_array ;
			saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId) ;
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	sect_id.id = sect_id_array ;
	if ( saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId) != SA_OK)
		{
			saCkptCheckpointClose ( & checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
		}
	
	if (saCkptCheckpointClose(&checkpoint_handle) != SA_OK)
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
        Try to set expiration time to  the defaut section: SA_CKPT_DEFAULT_SECTION_ID. 

        SA_ERR_INVALID_PARAM will be returned.

returns :

               o : indicate success
              -1: fail
*/
int set_default_section(void)
{
	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					   &ckpt_name,  
                                      &ckpt_create_attri , SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					   open_timeout, 
                                      &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

     	sect_id.id = NULL ;
    	sect_id.idLen = 0 ; 

	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, & sect_id, expire_time);
	if (ckpt_error == SA_OK)
		{
            		sect_id.id = sect_id_array ;
			sect_id.idLen = sizeof (sect_id_array) ;
			saCkptSectionDelete(&checkpoint_handle,  sect_create_attri.sectionId) ;
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

	return 0;
}


/*

case description :

        	Expiration parameter value  is beyond current time. 

returns :
               o : indicate success
              -1: fail
*/

int set_invalid_time(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}

 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
					&ckpt_name,  
                             	&ckpt_create_attri , 
                          		SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE, 
					open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                     	&sect_create_attri ,  
					 	 (void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, 
						     	&sect_id, 
                                    		expire_time-(SaTimeT)3600*96*1000000000);
	if(ckpt_error == SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) ;
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}
	if ( saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
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
          Invocation with correct steps and parameters. 


returns :
               o : indicate success
              -1: fail
*/
int set_normal_call(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
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

	ckpt_error = saCkptSectionCreate (&checkpoint_handle, 
                                          &sect_create_attri ,  
					  	(void *) init_data, sizeof (init_data)) ;
	if (ckpt_error != SA_OK)
		{
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	ckpt_error = saCkptSectionExpirationTimeSet (&checkpoint_handle, &sect_id, expire_time);
	if(ckpt_error != SA_OK)
		{
			saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) ;
			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (&ckpt_handle) ;
			return -1 ;
		}

	if (saCkptSectionDelete(&checkpoint_handle, sect_create_attri.sectionId) != SA_OK)
		{

			saCkptCheckpointClose (&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if (saCkptCheckpointClose(&checkpoint_handle) != SA_OK)
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
	char case_name[] = "saCkptSectionExpirationTimeSet";
	char name_expire[]="checkpoint_expire";
	int case_index ;
        time_t now;
		
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_expire) ;

	memcpy (ckpt_name.value, name_expire, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	sect_id.id = sect_id_array ;
	sect_id.idLen = sizeof (sect_id_array) ;

	time (&now) ;
	expire_time =(SaTimeT)(now+3600*24)*1000000000LL;

	sect_create_attri.sectionId = & sect_id ;
	sect_create_attri.expirationTime = expire_time;
	
	case_index = 0 ;

	if ( !set_pre_open())
		printf ("%s set_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s set_pre_open %d FAIL\n", case_name, case_index++);

	if ( !set_after_close())
		printf ("%s set_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s set_after_close %d FAIL\n", case_name, case_index++);

	if ( !set_after_finalize())
		printf ("%s set_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s set_after_finalize %d FAIL\n", case_name, case_index++);
	
	if ( !set_after_delete())
		printf ("%s set_after_delete %d OK\n", case_name, case_index++);
	else
		printf ("%s set_after_delete %d FAIL\n", case_name, case_index++);

	if ( !set_err_access())
		printf ("%s set_err_access %d OK\n", case_name, case_index++);
	else
		printf ("%s set_err_access %d FAIL\n", case_name, case_index++);
	
	if ( !set_normal_call())
		printf ("%s set_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s set_normal_call %d FAIL\n", case_name, case_index++);

	if ( !set_invalid_time())
		printf ("%s set_invalid_time %d OK\n", case_name, case_index++);
	else
		printf ("%s set_invalid_time %d FAIL\n", case_name, case_index++);

	if ( !set_default_section())
		printf ("%s set_default_section %d OK\n", case_name, case_index++);
	else
		printf ("%s set_default_section %d FAIL\n", case_name, case_index++);

	if ( !set_not_exist())
		printf ("%s set_not_exist %d OK\n", case_name, case_index++);
	else
		printf ("%s set_not_exist %d FAIL\n", case_name, case_index++);

	return 0;
}
