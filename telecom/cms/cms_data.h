/*
 * cms_data.h: cms daemon data and struct header
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#ifndef __CMS_DATA_H__
#define __CMS_DATA_H__ 

#include <hb_api.h>
#include <saf/ais.h>
#include <clplumbing/ipc.h>


#define CMSID 		"cms"

#define DEBUG_MEMORY	0
#define DEBUG_CLUSTER 	0
#define DEBUG_EXIT	0

struct cms_data_s {
	ll_cluster_t * 	hb_handle;
	IPC_Channel *	hb_channel;
	size_t		node_count;
	char *		my_nodeid;

	SaClmHandleT	clm_handle;
	int		clm_fd;
	SaClmClusterNotificationT * clm_nbuf;

	GHashTable * 	client_table;
	IPC_WaitConnection * wait_ch;

	GMainLoop *	mainloop;

	int 		cms_ready;
};

typedef struct cms_data_s cms_data_t;

extern cms_data_t cms_data;
extern GList *mqmember_list;
extern int option_debug;


int cms_client_init(cms_data_t * cms_data);
gboolean cluster_input_dispatch(IPC_Channel * channel, gpointer user_data);
gboolean client_input_dispatch(IPC_Channel * client, gpointer user_data);

#endif /* __CMS_DATA_H__ */
