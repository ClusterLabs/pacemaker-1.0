#include <saf/ais.h>
#include <stdio.h>
#include <unistd.h>
#include "ckpt_test.h"
#include <time.h>


void ckpt_open_callback (SaInvocationT invocation,
			const SaCkptCheckpointHandleT *checkpointHandle,
			SaErrorT error);

void ckpt_open_callback (SaInvocationT invocation,
			const SaCkptCheckpointHandleT *checkpointHandle,
			SaErrorT error)
{
	return ;
}

void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
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



int sync_pre_open(void);
int sync_pre_open(void)
{

	checkpoint_handle = -1 ;
	ckpt_error  = saCkptCheckpointSynchronize (&checkpoint_handle, 1000) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	return 0;
}

int sync_null_handle(void);
int sync_null_handle(void)
{
	ckpt_error = saCkptCheckpointSynchronize (NULL, 1000) ;
	if ( ckpt_error == SA_OK)
		return -1 ;
	return 0 ;
}



/*
case description :
        Invoke with invalid checkpoint handle. Two cases will be included:
        1. after close operation

        2. after finalization operation

        

returns :

               o : indicate success
              -1: fail
*/

int sync_after_close(void);
int sync_after_close(void)
{
	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 

				           open_timeout, 
                                           &checkpoint_handle) ;
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

	ckpt_error  = saCkptCheckpointSynchronize (&checkpoint_handle, 1000) ;
	if( ckpt_error == SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;

	return 0;
	
}


int sync_after_finalize(void);
int sync_after_finalize(void)
{
	time_t now;
	SaTimeT expire_time;

	time(&now);
	expire_time = (SaTimeT)((now+3600*24)*1000000000LL);

	ckpt_error = saCkptInitialize (&ckpt_handle, &ckpt_callback, &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 

				           open_timeout, 
                                           &checkpoint_handle) ;
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

	if ( saCkptFinalize (& ckpt_handle) != SA_OK )
		return -1 ;

	ckpt_error  = saCkptCheckpointSynchronize (&checkpoint_handle, expire_time) ;
	if( ckpt_error == SA_OK)
		{
			return -1 ;
		}
	return 0;
}

/*
case description :
        Invoke with timeout set to negative value.

        

returns :

               o : indicate success
              -1: fail
*/

int sync_invalid_timeout(void);
int sync_invalid_timeout(void)
{

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ, 

					   open_timeout, 
                                           &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error  = saCkptCheckpointSynchronize (&checkpoint_handle, -10) ;
	if (ckpt_error == SA_OK )
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
        Checkpoint not opened in write mode. SA_ERR_ACCESS will be returned. 

returns :

               o : indicate success
              -1: fail
*/



int sync_err_access(void);
int sync_err_access(void)
{


	time_t now;
	SaTimeT expire_time;

	time(&now);
	expire_time = (SaTimeT)((now+3600*24)*1000000000LL);

	ckpt_error = saCkptInitialize ( &ckpt_handle, & ckpt_callback,  &ckpt_version) ;
	if (ckpt_error != SA_OK)
        	{
			return -1 ;
		}
 	ckpt_error = saCkptCheckpointOpen (&ckpt_handle, 
                                           &ckpt_name,  
                                           &ckpt_create_attri , 
                                           SA_CKPT_CHECKPOINT_COLOCATED| SA_CKPT_CHECKPOINT_READ, 
					   open_timeout, &checkpoint_handle) ;
	if ( ckpt_error != SA_OK)
		{
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		}

	ckpt_error  = saCkptCheckpointSynchronize (&checkpoint_handle,  expire_time) ;
	if (ckpt_error != SA_OK)
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

	if ( saCkptFinalize (&ckpt_handle) != SA_OK )
		return -1 ;

	return 0 ;
}



/*
case description :
        Invocation with correct steps and parameters. SA_OK will be returned.

returns :

               o : indicate success
              -1: fail
*/



int sync_normal_call(void);
int sync_normal_call(void)
{
	time_t now;
	SaTimeT expire_time;

	time(&now);
	expire_time = (SaTimeT)((now+3600*24)*1000000000LL);
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

	ckpt_error = saCkptCheckpointSynchronize (&checkpoint_handle, expire_time) ;
	if (ckpt_error != SA_OK)
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

int main(int argc, char * argv[])
{
	char case_name[] = "saCkptCheckpointSynchronize";
	char name_sync[]="checkpoint_sync";
	int case_index ;
		
	ckpt_version.major = VERSION_MAJOR;
	ckpt_version.minor = VERSION_MINOR;
	ckpt_version.releaseCode = VERSION_RELEASCODE ;

	ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	ckpt_name.length = sizeof (name_sync) ;

	memcpy (ckpt_name.value, name_sync, ckpt_name.length);

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = 10 ;

	case_index = 0 ;
	if ( !sync_pre_open())
		printf ("%s sync_pre_open %d OK\n", case_name, case_index++);
	else
		printf ("%s sync_pre_open %d FAIL\n", case_name, case_index++);
	
	if ( !sync_after_close())
		printf ("%s sync_after_close %d OK\n", case_name, case_index++);
	else
		printf ("%s sync_after_close %d FAIL\n", case_name, case_index++);


	if ( !sync_after_finalize())
		printf ("%s sync_after_finalize %d OK\n", case_name, case_index++);
	else
		printf ("%s sync_after_finalize %d FAIL\n", case_name, case_index++);

	if ( !sync_normal_call())
		printf ("%s sync_normal_call %d OK\n", case_name, case_index++);
	else
		printf ("%s sync_normal_call %d FAIL\n", case_name, case_index++);

	if ( !sync_err_access())
		printf ("%s sync_err_access %d OK\n", case_name, case_index++);
	else
		printf ("%s sync_err_access %d FAIL\n", case_name, case_index++);

	if ( !sync_invalid_timeout())
		printf ("%s sync_invalid_timeout %d OK\n", case_name, case_index++);
	else
		printf ("%s sync_invalid_timeout %d FAIL\n", case_name, case_index++);

	return 0 ;
}
