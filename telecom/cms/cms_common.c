/*
 * cms_common.c: cms daemon common functions
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

#include <portability.h>
#include <string.h>	/* strncmp */
#include <sys/time.h>	/* gettimeofday */

#include "cms_common.h"


const char *
mqname_type2string(enum mqname_type type)
{
	return mqname_type_str[type];
}

enum mqname_type
mqname_string2type(const char *str)
{
	int i;

	for (i = 0; i < MQNAME_TYPE_LAST; i++) {
		if (strncmp(str, mqname_type_str[i], TYPESTRSIZE) == 0)
			return i;
	}
	return MQNAME_TYPE_LAST;
}

const char *
saerror_type2string(SaErrorT type)
{
	return sa_errortype_str[type];
}

SaErrorT
saerror_string2type(const char * str)
{
	int i;

	for (i = 0; i < SA_ERR_BAD_FLAGS + 2; i++) {
		if (strncmp(str, sa_errortype_str[i], TYPESTRSIZE) == 0) 
			return i;
	}
	return 0;
}

const char *
cmsrequest_type2string(size_t type) 
{
	int head = 0;
	int tail = CMS_TYPE_TOTAL - 1;
	int index = CMS_TYPE_TOTAL / 2;

	/* binary search */
	while ((index >= head) && (index <= tail)) {

		if (type == (1 << index))
			return cmsrequest_type_str[index];

		else if (type > (1 << index))
			head = index + 1;

		else
			tail = index - 1;

		index = (head + tail) / 2;
		// dprintf("index is %d, type is 0x%x\n", index, type);
	}

	cl_log(LOG_CRIT, "Invalid request type [%d]", (int)type);
	return NULL;
}

size_t
cmsrequest_string2type(const char * str)
{
	int i;

	for (i = 0; i < CMS_TYPE_TOTAL; i++) {
		if (strncmp(str, cmsrequest_type_str[i], TYPESTRSIZE) == 0)
			return (1 << i);
	}
	cl_log(LOG_CRIT, "Invalid request string [%s]", str);
	return -1;
}

long long
get_current_satime()
{
	struct timeval tv;
	long long ret;

	/* We don't have the nanosecond granularity, do we? */
	gettimeofday(&tv, 0);
	ret = tv.tv_sec;
	ret *= 1000000;
	ret += tv.tv_usec;
	ret *= 1000;
	return ret;
}

gboolean
is_host_local(const char * host, cms_data_t * cmsdata)
{
	if (strcmp(host, cmsdata->my_nodeid) == 0)
		return TRUE;
	else
		return FALSE;
}

int
str2saname(SaNameT * name, const char * str)
{
	name->length = strlen(str);
	strncpy(name->value, str, SA_MAX_NAME_LENGTH);
	name->value[name->length] = '\0';

	return name->length;
}

char *
saname2str(SaNameT name)
{
	char * str;

	if (name.length <= 0)
		return NULL;

	if (name.length > SA_MAX_NAME_LENGTH - 1)
		name.length = SA_MAX_NAME_LENGTH - 1;

	if ((str = (char *)ha_malloc(name.length + 1)) == NULL)
		return NULL;

	strncpy(str, name.value, name.length);
	str[name.length] = '\0';

	return str;
}

