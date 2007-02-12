/* $Id: event.h,v 1.3 2004/04/06 19:15:57 msoffen Exp $ */
/* 
 * event.h: header file for event service
 *
 * Copyright (C) 2004 Forrest,Zhao <forrest.zhao@intel.com>
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

#ifndef _EVENT_H
#define _EVENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <assert.h>
#include <glib.h>
#include <sys/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/GSource.h>
#include <clplumbing/realtime.h>

#include <portability.h>
#include <heartbeat.h>
#include <hb_api_core.h>
#include <hb_api.h>
#include <hb_proc.h>
#include <saf/ais_base.h>
#include <saf/ais_event.h>

#define NODEIDSIZE 255
#define STATUSSIZE 15

enum evt_type{
	EVT_EVENT_MSG = 1,
	EVT_CH_OPEN_REQUEST,
	EVT_CH_OPEN_REPLY,
	EVT_NEW_SUBSCRIBE,
	EVT_NEW_SUBSCRIBE_REPLY,
	EVT_RETENTION_CLEAR_REQUEST,
	EVT_RETENTION_CLEAR_REPLY,
	EVT_CHANNEL_UNLINK_NOTIFY,
	EVT_TIMEOUT,
	EVT_INITIALIZE,
	EVT_FINALIZE,
	EVT_PUBLISH,
	EVT_SUBSCRIBE,
	EVT_UNSUBSCRIBE,
	EVT_OPEN_EVENT_CHANNEL,
	EVT_CLOSE_EVENT_CHANNEL,
	EVT_CLEAR_RETENTION_TIME,
	EVT_CHANNEL_UNLINK,
	EVT_CLEAR_RETENTION_TIME_REPLY,
	EVT_CH_OPEN_REPLY_FROM_DAEMON,
	EVT_ASYN_CH_OPEN_REPLY_FROM_DAEMON,
	EVT_PUBLISH_REPLY,
	EVT_NORMAL_EVENT
};

#define EVTFIFO    EVTVARLIBDIR "/evt.so" 

struct sa_handle {
	int valid;	
};

struct sa_handle_database {
		SaUint32T handle_count;
		struct sa_handle *handles;
};

SaErrorT get_handle(struct sa_handle_database *handle_database,
				SaUint32T *handle);

SaErrorT put_handle(struct sa_handle_database *handle_database,
				SaUint32T handle);

SaErrorT get_handle(struct sa_handle_database *handle_database,
				SaUint32T *handle)
{
	SaUint32T i;
	int found = 0;
	void *new_handles;
	for (i = 0; i < handle_database->handle_count; i++) {
		if (handle_database->handles[i].valid == 0) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		handle_database->handle_count += 1;
		new_handles = (struct sa_handle *)realloc(
					handle_database->handles,
					sizeof(struct sa_handle)* handle_database->handle_count);
		if (new_handles == 0) {
			return (SA_ERR_NO_MEMORY);
		}
		handle_database->handles = new_handles;
	}
	handle_database->handles[i].valid = 1;
	*handle = i;
	return (SA_OK);
}

SaErrorT put_handle(struct sa_handle_database *handle_database,
				SaUint32T handle)
{
	handle_database->handles[handle].valid = 0;
	return (SA_OK);
}

#endif
