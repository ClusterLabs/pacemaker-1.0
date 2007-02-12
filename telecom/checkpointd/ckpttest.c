/* $Id: ckpttest.c,v 1.11 2004/11/18 01:56:59 yixiong Exp $ */
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

#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>

#include <glib.h>

#include <saf/ais.h>

int requestNumber = 0;

#undef TESTLOOP
/* #define TESTLOOP 1 */

int main(void)
{

	SaErrorT  		ckpt_error;
	SaCkptHandleT 		ckpt_handle ;
	SaCkptCheckpointHandleT	checkpoint_handle;
	SaVersionT 		ckpt_version = {'A', 1, 1};
	SaNameT 		ckpt_name = {9, "testckpt"};
	SaTimeT  		open_timeout = SA_TIME_END;

	SaCkptCheckpointCreationAttributesT ckpt_create_attri ;
	SaCkptSectionCreationAttributesT sect_create_attri ;
	SaCkptSectionIdT sect_id ;
	SaUint8T sectionid = 0;
	unsigned char sectionName[6]="ABCDE\0";
	SaCkptIOVectorElementT	io_write;
	SaCkptIOVectorElementT	io_read;

	int i = 0; /* section test loop count */
	int data_buffer = 0;
	unsigned char dataBuf[4]="abc\0";
	time_t cur_time;

	/* library initialize */
	ckpt_error = saCkptInitialize(&ckpt_handle, 
			NULL, 
			&ckpt_version) ;
	if (ckpt_error != SA_OK) {
		printf("saCkptInitialize error\n");
		return -1 ;
	}
	printf("Client %d initialized\n", ckpt_handle);

	/* checkpoint open */
	ckpt_create_attri.creationFlags = SA_CKPT_WR_ALL_REPLICAS ;
/*	ckpt_create_attri.creationFlags = SA_CKPT_WR_ACTIVE_REPLICA ; */
	ckpt_create_attri.retentionDuration = SA_TIME_END;
	ckpt_create_attri.checkpointSize = 1000 ;
	ckpt_create_attri.maxSectionSize = 100;
	ckpt_create_attri.maxSections = 11 ;
	ckpt_create_attri.maxSectionIdSize = SA_MAX_ID_LENGTH ;
	
	ckpt_error = saCkptCheckpointOpen(&ckpt_handle, 
			&ckpt_name, 
			&ckpt_create_attri, 
			SA_CKPT_CHECKPOINT_COLOCATED |
			SA_CKPT_CHECKPOINT_READ |
			SA_CKPT_CHECKPOINT_WRITE, 
			open_timeout, 
			&checkpoint_handle) ;
	if (ckpt_error != SA_OK) {
		printf("saCkptCheckpointOpen error\n");
		saCkptFinalize(&ckpt_handle) ;
		return -1 ;
	}
	printf("checkpoint %d opened\n", checkpoint_handle);

	/* read from the default section */
	io_read.sectionId.id = NULL;
	io_read.sectionId.idLen = 0;
	io_read.dataBuffer = &requestNumber;
	io_read.dataOffset = 0 ;
	io_read.dataSize = sizeof(int);
	io_read.readSize = 0 ;

	ckpt_error = saCkptCheckpointRead(&checkpoint_handle, 
			&io_read, 
			1,  
			NULL) ;
	if((ckpt_error != SA_OK) || 
		(io_read.readSize != sizeof(requestNumber))){
		printf("saCkptCheckpointRead error\n");
		requestNumber = 0;
	} else {
		printf("Read number %d from default section\n", 
			requestNumber);
	}

	/* write to the default section */
	requestNumber++;
	
	io_write.sectionId.id = NULL;
	io_write.sectionId.idLen = 0;
	io_write.dataBuffer = &requestNumber;
	io_write.dataSize = sizeof(int);
	io_write.dataOffset = 0 ;

	ckpt_error = saCkptCheckpointWrite(&checkpoint_handle, 
			&io_write, 
			1,  
			NULL) ;
	if( ckpt_error != SA_OK) {
		printf("saCkptCheckpointWrite error\n");
		saCkptCheckpointClose(&checkpoint_handle) ;
		saCkptFinalize(&ckpt_handle) ;
		return -1 ;
	} else {
		printf("Write number %d into default section\n", 
			requestNumber);
	}

	for (i=0; i<9; i++) {
		/* section create */
		sectionid = 'A' + i;
		sectionName[0] += 1;
		sect_id.id = sectionName;
		sect_id.idLen = sizeof(sectionName)/sizeof(unsigned char);
		sect_create_attri.sectionId = &sect_id ;

		time(&cur_time) ;
		sect_create_attri.expirationTime = 
			(SaTimeT)(cur_time + 3600*24) * 1000000000LL;

		/* initial data is the section id */
		data_buffer = sectionid;
		
		ckpt_error = saCkptSectionCreate(&checkpoint_handle, 
						&sect_create_attri,  
						dataBuf, 
						sizeof(dataBuf)/sizeof(unsigned char)) ;
		if( ckpt_error != SA_OK && ckpt_error != SA_ERR_EXIST) {
			printf("\tsaCkptSectionCreate error\n");
			saCkptCheckpointClose(&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		} else {
			if(ckpt_error == SA_ERR_EXIST){
				printf("\tsection %c existed already\n", *(SaUint8T*)sect_id.id);
			}
			else printf("\tsection %c created\n", *(SaUint8T*)sect_id.id);
		}

		/* read init data from the section */
		data_buffer = 0;
		
		io_read.sectionId.id = sectionName;
		io_read.sectionId.idLen = sizeof(sectionName)/sizeof(unsigned char);
		io_read.dataBuffer = dataBuf;
		io_read.dataSize = sizeof(dataBuf)/sizeof(unsigned char);
		io_read.dataOffset = 0;
		ckpt_error = saCkptCheckpointRead (&checkpoint_handle, 
				&io_read, 
				1,  
				NULL) ;
		if(ckpt_error != SA_OK) {
			printf("\tsaCkptCheckpointRead error\n");
			saCkptCheckpointClose(&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		} else {
			printf("\tRead number %d from section %c\n", 
				dataBuf[0],
				*(SaUint8T*)(io_read.sectionId.id));
		}
		
		/* write to the section */
		data_buffer = '*';
		io_write.sectionId.id = sectionName;
		io_write.sectionId.idLen = sizeof(sectionName);
		io_write.dataBuffer = &data_buffer;
		io_write.dataOffset = 0;
		io_write.dataSize = sizeof(data_buffer);
		ckpt_error = saCkptCheckpointWrite (&checkpoint_handle, 
				&io_write, 
				1,  
				NULL) ;
		if( ckpt_error != SA_OK) {
			printf("saCkptCheckpointWrite error\n");
			saCkptCheckpointClose(&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		} else {
			printf("\tWrite number %d into section %c\n", 
				data_buffer, 
				*(SaUint8T*)(io_write.sectionId.id));
		}
	}
	sectionName[0]='A';
	/* section delete */
	for (i=0; i<9; i++) {
		/* section create */
		sectionid = 'A' + i;
		sectionName[0] += 1;
		sect_id.id = sectionName ;
		sect_id.idLen = sizeof(sectionName)/sizeof(unsigned char);

		ckpt_error = saCkptSectionDelete(&checkpoint_handle,
						&sect_id);
		if( ckpt_error != SA_OK) {
			printf("\tsaCkptSectionDelete error\n");
			saCkptCheckpointClose(&checkpoint_handle) ;
			saCkptFinalize (& ckpt_handle) ;
			return -1 ;
		} else {
			printf("\tsection %c deleted\n", 
				*(SaUint8T*)sect_id.id);
		}
	}
	
	/* checkpoint close */
	if (saCkptCheckpointClose(&checkpoint_handle) != SA_OK) {
		printf("saCkptCheckpointClose error\n");
		saCkptFinalize (&ckpt_handle) ;
		return -1 ;
	}
	printf("checkpoint %d closed\n", checkpoint_handle);

	/* library finalize */
	if ((ckpt_error = saCkptFinalize(&ckpt_handle)) != SA_OK) {
		printf("saCkptFinalize error\n");
		return -1 ;
	}
	printf("Client %d finalized\n", ckpt_handle);
	
	return 0 ;
}

