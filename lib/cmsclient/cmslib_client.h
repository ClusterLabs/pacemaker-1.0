/*
 * cmslib_client.h: SAForum AIS Message Service client library header
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 */
#ifndef __CMSLIB_CLIENT_H__
#define __CMSLIB_CLIENT_H__

#include <clplumbing/ipc.h>

#define CMS_LIBRARY_TRACE()	dprintf("TRACE: %s\n", __FUNCTION__)

typedef struct {
	IPC_Channel 		* ch;		/* client daemon channel */
	int			active_fd;	/* make select always returns */
	int			backup_fd;	/* backup fd vs. active_fd */
	SaMsgHandleT 		service_handle;
	SaMsgCallbacksT 	callbacks;	/* client's callback func */
	GList * 		dispatch_queue; /* client's dispatch queue */
	GHashTable 		* queue_handle_hash;
} __cms_handle_t;

typedef struct {
	SaMsgQueueHandleT	queue_handle;
	SaNameT			queue_name;
	__cms_handle_t		* cms_handle;
} __cms_queue_handle_t;

typedef struct {
	const SaNameT * name;
	SaUint8T flag;
	SaMsgQueueGroupPolicyT policy;
	SaMsgQueueGroupNotificationBufferT buf;
} __mqgroup_track_t;

#endif	/* __CMSLIB_CLIENT_H__ */
