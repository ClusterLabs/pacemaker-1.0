/* 
 * ckpttest.c: data checkpoint service test program
 *
 * Copyright (C) 2003 Deng Pan <deng.pan@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <glib.h>

#include <saf/ais.h>

int requestNumber = 0;

#undef TESTLOOP
// #define TESTLOOP 1

#ifndef LLONG_MAX
#define	LLONG_MAX	9223372036854775807LL
#endif

int main(void)
{

	SaErrorT  		ckpt_error;
	SaCkptHandleT 		ckpt_handle ;
	SaCkptCheckpointHandleT	checkpoint_handle;
	SaVersionT 		ckpt_version = {'A', '0', '1'};
	SaNameT 		ckpt_name = {9, "testckpt"};
	SaTimeT  		open_timeout = ((SaTimeT)LLONG_MAX);

	SaCkptCheckpointCreationAttributesT ckpt_create_attri ;
	SaCkptIOVectorElementT	io_write;
	SaCkptIOVectorElementT	io_read;

	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ;
//	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ;
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 10 ;
	ckpt_create_attri.maxSectionIdSize = SA_MAX_ID_LENGTH ;
	
	io_write.sectionId.id[0] = 0;
	io_write.sectionId.idLen = 0;
	io_write.dataBuffer = &requestNumber;
	io_write.dataSize = sizeof(int);
	io_write.dataOffset = 0 ;

	io_read.sectionId.id[0] = 0;
	io_read.sectionId.idLen = 0;
	io_read.dataBuffer = &requestNumber;
	io_read.dataOffset = 0 ;
	io_read.dataSize = sizeof(int);
	io_read.readSize = 0 ;

#ifdef TESTLOOP
while (1) {
#endif
	ckpt_error = saCkptInitialize(&ckpt_handle, 
			NULL, 
			&ckpt_version) ;
	if (ckpt_error != SA_OK) {
		printf("saCkptInitialize error\n");
		return -1 ;
	}
	printf("Client %d initialized\n", ckpt_handle);
      	
	ckpt_error = saCkptCheckpointOpen(&ckpt_handle, 
			&ckpt_name, 
			&ckpt_create_attri, 
			SA_CKPT_CHECKPOINT_COLOCATED, 
			open_timeout, 
			&checkpoint_handle) ;
	if (ckpt_error != SA_OK) {
		printf("saCkptCheckpointOpen error\n");
		saCkptFinalize(&ckpt_handle) ;
		return -1 ;
	}
	printf("checkpoint %d opened\n", checkpoint_handle);

	ckpt_error = saCkptCheckpointRead (&checkpoint_handle, 
			&io_read, 
			1,  
			NULL) ;
	if( (ckpt_error != SA_OK) || 
		(io_read.readSize != sizeof(requestNumber))){
		printf("saCkptCheckpointRead error\n");
		requestNumber = 0;
	}

	printf("Accepted %d requests before\n", requestNumber);

	/* update the request number */

	requestNumber++;
	ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, 
			&io_write, 
			1,  
			NULL) ;
	if( ckpt_error != SA_OK) {
		printf("saCkptCheckpointWrite error\n");
		saCkptCheckpointClose ( & checkpoint_handle) ;
		saCkptFinalize (& ckpt_handle) ;
		return -1 ;
	}

	if (saCkptCheckpointClose(&checkpoint_handle) != SA_OK ) {
		printf("saCkptCheckpointClose error\n");
		saCkptFinalize (&ckpt_handle) ;
		return -1 ;
	}
	printf("checkpoint %d closed\n", checkpoint_handle);
	
	if ((ckpt_error = saCkptFinalize(&ckpt_handle)) != SA_OK) {
		printf("saCkptFinalize error\n");
		return -1 ;
	}
	printf("Client %d finalized\n", ckpt_handle);

#ifdef TESTLOOP
}
#endif
	
	return 0 ;
}

