/*
 * cms_common.h: cms daemon common functions header
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
#ifndef __CMS_COMMON_H__
#define __CMS_COMMON_H__

#include "cms_data.h"
#include "cms_cluster.h"
#include "cms_client.h"
#include "cms_membership.h"

#define dprintf(arg...)	do {if (option_debug) fprintf(stderr, ##arg);} while(0)
#define CMS_TRACE()	dprintf("TRACE: In function %s\n", __FUNCTION__)

#define TYPESTRSIZE 40

const char * mqname_type2string(enum mqname_type type);
enum mqname_type mqname_string2type(const char *str);
const char * saerror_type2string(SaErrorT type);
SaErrorT saerror_string2type(const char * str);
const char * cmsrequest_type2string(size_t type);
size_t cmsrequest_string2type(const char * str);
long long get_current_satime(void);
gboolean is_host_local(const char * host, cms_data_t * cmsdata);
int str2saname(SaNameT * name, const char * str);
char * saname2str(SaNameT name);

#endif	/* __CMS_COMMON_H__ */
