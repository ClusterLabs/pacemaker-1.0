/* 
 * init.c: data checkpoint API test: saCkptInitialize
 *
 * Copyright (C) 2003 Wilna Wei <willna.wei@intel.com>
 * 
 * This program  is free software; you can redistribute it and/or
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
#include <saf/ais.h>
#include "ckpt_test.h"

void ckpt_open_callback (SaInvocationT invocation,
 		        const SaCkptCheckpointHandleT *checkpointHandle,
                   	SaErrorT error);
void ckpt_sync_callback (SaInvocationT invocation, SaErrorT error);
int  init_null_handle(void);
int  init_null_callback(void);
int  init_null_version(void);
int  init_invalid_version(void);
int  init_noraml_call(void);
int  init_repeat_call(void);

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
        Invoke with invalid library handle pointer(NULL).The error returned shoude be 
        SA_ERR_INVALIDE_PARAM
        
returns :
               o : indicate success
              -1: fail
*/
int  init_null_handle(void)
{
	if ( (ckpt_error = saCkptInitialize (NULL, &ckpt_callback,  &ckpt_version)) 
		!=SA_OK)
		return 0;
	return -1;
			
}


/*
case description :
        Set the callback parameter to null.  AIS support this kind of call. SA_OK will be returned. 
returns :
               o : indicate success
              -1: fail
*/
int  init_null_callback(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, NULL,  &ckpt_version)) 
		!=SA_OK)
			return -1 ;	
	return 0 ;
}


/*
case description :
        This case test version parameter. Two subcase will be included:
         				1.Set the version  parameter to null. 
         				2. error version value.
        Both case will return SA_ERR_VERSION.

returns :
               o : indicate success
              -1: fail
*/
int  init_null_version(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback, NULL)) 
		!=SA_OK)
		return 0;
	else
		return -1 ;
}

int  init_invalid_version(void)
{
	SaVersionT tmp_version ; 
	tmp_version.major = VERSION_MAJOR +1;
	tmp_version.minor = VERSION_MINOR +1 ;
	tmp_version.releaseCode = VERSION_RELEASCODE +1 ;
	
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback, &tmp_version)) 
		!=SA_OK)
		return 0 ;
	else	
		return -1 ;
}

/*
case description :
        Invoke with correct parameter. The case will return SA_OK.
returns :
               o : indicate success
              -1: fail
*/
int  init_noraml_call(void)
{
	if ( (ckpt_error = saCkptInitialize ( &ckpt_handle, &ckpt_callback,  &ckpt_version)) 
		!=SA_OK)
			return -1 ;
	if (saCkptFinalize (&ckpt_handle) != SA_OK)
		return -1 ;
	return 0 ;
}

/*
case description :
        Repeat initialization library.  Each call returns a different handle.
returns :
               o : indicate success
              -1: fail
*/
int  init_repeat_call(void)
{
	SaCkptHandleT ckpt_tmp_handle[5];
	 int i, j ;
	 int ret_err = 0 ;

	for ( i = 0; i < 5 ; i++)
		{
			if ( (ckpt_error = saCkptInitialize ( &ckpt_tmp_handle[i], 
						&ckpt_callback,  &ckpt_version))!=SA_OK)
				 break ;
			for (j=0; j < i ; j++)
				{
					if (ckpt_tmp_handle[i] == ckpt_tmp_handle[j])
						{
							break;
						}
				}
		}

	for (j = 0; j < i; j++)
		{
			if (saCkptFinalize (& ckpt_tmp_handle[j]) != SA_OK)
				ret_err = -1;
			
		}
	if (i != 5)
		ret_err = -1 ;

	return ret_err ;
	
}



int  main(void)
{

	
	char case_name[] = "saCkptInitialize";
	int case_index ;
	 
	 
	 ckpt_version.major = VERSION_MAJOR;
	 ckpt_version.minor = VERSION_MINOR;
	 ckpt_version.releaseCode = VERSION_RELEASCODE;

	 ckpt_callback.saCkptCheckpointOpenCallback = ckpt_open_callback ;
	 ckpt_callback.saCkptCheckpointSynchronizeCallback = ckpt_sync_callback;

	case_index = 0 ;

	if ( !init_null_handle())
		printf ("%s init_null_handle %d OK\n\n", case_name, case_index++);
	else
		printf ("%s init_null_handle %d FAIL\n\n", case_name, case_index++);

	if ( !init_null_callback())
		printf ("%s init_null_callback %d OK\n\n", case_name, case_index++);
	else
		printf ("%s init_null_callback %d FAIL\n\n", case_name, case_index++);

	if ( !init_invalid_version())
		printf ("%s init_invalid_version %d OK\n\n", case_name, case_index++);
	else
		printf ("%s init_invalid_version %d FAIL\n\n", case_name, case_index++);

	if ( !init_null_version())
		printf ("%s init_null_version %d OK\n\n", case_name, case_index++);
	else
		printf ("%s init_null_version %d FAIL\n\n", case_name, case_index++);

	if ( !init_noraml_call())
		printf ("%s init_noraml_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s init_noraml_call %d FAIL\n\n", case_name, case_index++);

	if ( !init_repeat_call())
		printf ("%s init_repeat_call %d OK\n\n", case_name, case_index++);
	else
		printf ("%s init_repeat_call %d FAIL\n\n", case_name, case_index++);

	return  0;

}
