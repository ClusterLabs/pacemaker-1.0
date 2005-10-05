/*
 * Client-side Linux HA Manager API.
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (C) 2005 International Business Machines
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __HAMD_H
#define __HAMD_H 1

#include <mgmt/mgmt.h>

#define OPTARGS		"skrhv"
#define PID_FILE 	HA_VARRUNDIR"/mgmtd.pid"
#define ENV_PREFIX 	"HA_"
#define KEY_LOGDAEMON   "use_logd"
#define HADEBUGVAL	"HA_DEBUG"

#define mgmtd_log(priority, fmt...); \
        if ( debug_level == 0 && priority == LOG_DEBUG ) { \
                ; \
} else { \
                cl_log(priority, fmt); \
}

#define mgmtd_log2(priority, fmt...); \
        if ( debug_level == 2 && priority == LOG_DEBUG ) { \
                cl_log(priority, fmt); \
}

#define ARGC_CHECK(n);		\
if (argc != (n)) {					\
	mgmtd_log(LOG_DEBUG, "%s msg should have %d params, but %d given",argv[0],n,argc);	\
	return cl_strdup(MSG_FAIL);			\
}

typedef char* (*msg_handler)(char* argv[], int argc, int client_id);
extern int reg_msg(const char* type, msg_handler fun);
extern int reg_evt(const char* type, int client_id);
extern int fire_evt(const char* evt);
extern const char* mgmtd_name;
extern int debug_level;
#endif /* __HAMD_H */
