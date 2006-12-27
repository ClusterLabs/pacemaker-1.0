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

const char * sa_errortype_str[SA_ERR_BAD_FLAGS + 2] = {
	"",
	"sa_ok",
	"sa_err_library",
	"sa_err_version",
	"sa_err_init",
	"sa_err_timeout",
	"sa_err_try_again",
	"sa_err_invalid_param",
	"sa_err_no_memory",
	"sa_err_bad_handle",
	"sa_err_busy",
	"sa_err_access",
	"sa_err_not_exist",
	"sa_err_name_too_long",
	"sa_err_exist",
	"sa_err_no_space",
	"sa_err_interrupt",
	"sa_err_system",
	"sa_err_name_not_found",
	"sa_err_no_resources",
	"sa_err_not_supported",
	"sa_err_bad_operation",
	"sa_err_failed_operation",
	"sa_err_message_error",
	"sa_err_no_message",
	"sa_err_queue_full",
	"sa_err_queue_not_available",
	"sa_err_bad_checkpoint",
	"sa_err_bad_flags",
	""
};

const char * mqname_type_str[MQNAME_TYPE_LAST] = {
	"",
	"mqinit",
	"mqrequest",
	"mqgranted",
	"mqreopen",
	"mqdenied",
	"mqclose",
	"mqunlink",
	"mqsend",
	"mqinsert",
	"mqremove",
	"mqmsgack",
	"mqinfoupdate",
	"mqreopenmsgfeed",
	"mqmsgfeedend",
	"mqstatusrequest",
	"mqstatusreply",
	"mqinfoupdaterequest",
	"mqsendreceive",
	"mqreceivereply",
	""
};

const char * cmsrequest_type_str[CMS_TYPE_TOTAL] = {
	"",
	"cms_qstatus",
	"cms_qopen",
	"cms_qopenasync",
	"cms_qclose",
	"cms_qunlink",
	"cms_msend",
	"cms_msendasync",
	"cms_mack",
	"cms_mreceivedget",
	"cms_qg_creat",
	"cms_qg_delete",
	"cms_qg_insert",
	"cms_qg_remove",
	"cms_qg_track_start",
	"cms_qg_track_stop",
	"cms_qg_notify",
	"cms_msg_request",
	"cms_msg_sendreceive",
	"cms_msg_receive",
	"cms_msg_reply",
	"cms_msg_replyasync",
	/* below are msg types that can not go across network */
	"cms_mget",
	"cms_msg_notify",
	""
};


const char *
mqname_type2string(enum mqname_type type)
{
	if (type > MQNAME_TYPE_LAST) 
		return NULL;

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
	return 0;
}

const char *
saerror_type2string(SaErrorT type)
{
	if (type > SA_ERR_BAD_FLAGS)
		return NULL;

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
	if (type > CMS_TYPE_TOTAL) 
		return NULL;

	return cmsrequest_type_str[type];
}

enum cms_client_msg_type
cmsrequest_string2type(const char * str)
{
	int i;

	for (i = 0; i < CMS_TYPE_TOTAL; i++) {
		if (strncmp(str, cmsrequest_type_str[i], TYPESTRSIZE) == 0)
			return i;
	}
	cl_log(LOG_CRIT, "Invalid request string [%s]", str);
	return 0;
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

	if ((str = (char *)cl_malloc(name.length + 1)) == NULL)
		return NULL;

	strncpy(str, name.value, name.length);
	str[name.length] = '\0';

	return str;
}

