/*
 * cms_cmstools.h: cmstools utility header
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
#define DEFAULT_MQUEUE_NAME		"cmstools_mqueue"
#define DEFAULT_MQGROUP_NAME		"cmstools_mqgroup"

#define DEFAULT_MQUEUE_SIZE		15000
#define DEFAULT_MESSAGE_SIZE		1024
#define DEFAULT_MESSAGE_PRIORITY	SA_MSG_MESSAGE_LOWEST_PRIORITY
#define DEFAULT_MESSAGE_STRING		"cmstools testing message"
#define DEFAULT_RETENTION_TIME		150000000000LL	/* 150 seconds */
#define DEFAULT_CREATION_FLAGS		0
#define DEFAULT_TIMEOUT			SA_TIME_END

#define DEFAULT_MQGROUP_TRACK_FLAG	SA_TRACK_CHANGES_ONLY
