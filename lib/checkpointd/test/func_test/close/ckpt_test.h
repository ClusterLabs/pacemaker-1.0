/* 
 * ckpt_test.h: Data checkpoint Test Head File 
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
 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#define VERSION_MAJOR '0' 
#define VERSION_MINOR '1'
#define VERSION_RELEASCODE 'A'

#define DEBUG_PORT 8000
#define TEST_PORT 9000

#define INVOCATION_BASE  100

#define CKPTFIFO HA_VARLIBDIR "heartbeat/ckpt/ckpt.sock"
#define RECVSIZE 102400
#define SEL_TIMEOUT 10

#ifndef LLONG_MAX
#define LLONG_MAX	9223372036854775807LL
#endif

char name[] = "chekckpoint";
char node1[] = "aelanpr1" ;
char node2[] = "aelanpr2" ;


SaErrorT  ckpt_error;

SaCkptHandleT ckpt_handle ;

SaVersionT ckpt_version ;

SaCkptCallbacksT ckpt_callback ;

SaInvocationT ckpt_invocation;

SaNameT ckpt_name;

SaCkptCheckpointCreationAttributesT ckpt_create_attri ;

SaCkptCheckpointHandleT checkpoint_handle;
SaSelectionObjectT ckpt_select_obj;
SaCkptSectionCreationAttributesT sect_create_attri ;
SaCkptSectionIdT sect_id ;
time_t cur_time ;
char init_data[] = "Hello world !" ;
char over_data[] = "Overwrite data here" ;
SaUint8T sect_id_array[] = "12";

SaTimeT  open_timeout = LLONG_MAX ;
SaUint64T del_timeout = LLONG_MAX ;
SaCkptSectionIteratorT ckpt_iterator ;
SaCkptSectionDescriptorT ckpt_desc ;
SaCkptIOVectorElementT ckpt_io ;
SaCkptIOVectorElementT io_read ;
SaUint32T ckpt_error_index ;

