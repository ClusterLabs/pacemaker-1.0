/*
 * mgmt_client.c: mgmt library client
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
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

#ifndef _MGMT_CLIENT_H
#define _MGMT_CLIENT_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <hb_api.h>
#include <saf/ais.h>
#include <clplumbing/cl_uuid.h>
#include <clplumbing/cl_log.h>
#include <mgmt/mgmt.h>
#include "utils.h"

#define Arg1(a)               a, NULL
#define Arg2(a,b)             a, b, NULL
#define Arg3(a,b,c)           a, b, c, NULL
#define Arg4(a,b,c,d)         a, b, c, d, NULL
#define Arg5(a,b,c,d,e)       a, b, c, d, e, NULL
#define Arg6(a,b,c,d,e,f)     a, b, c, d, e, f, NULL

#define mclient_makeup_param(a,b)			\
	({	char param[MAXLEN] = "";		\
		strncat(param, a, 			\
			sizeof(param)-strlen(param)-1);	\
		strncat(param, "\n",			\
			sizeof(param)-strlen(param)-1);	\
		strncat(param, b, 			\
			sizeof(param)-strlen(param)-1);	\
		param;					\
	})

#define mclient_new_and_process(cmnd, arg...) 				\
	({ 	MClient * client = mclient_new_with_cmnd(cmnd, ##arg);	\
		if ( client ){						\
			if ( mclient_process(client) != HA_OK 		\
					|| client->rlen == 0){		\
				mclient_free(client);			\
				client = NULL;				\
				cl_log(LOG_ERR,"mclient_new_and_process"\
						":process error.");	\
			}						\
		}					        	\
		client;							\
	})

typedef struct mgmt_client {
	char *		cmnd;		/* cmnd */
        char **		rdata;		/* result */
        int		rlen;		/* result length */
} MClient;


#define		MC_OK		0
#define		MC_FAIL		-1
#define		MC_ERROR	-2

MClient *	mclient_new(void);
MClient * 	mclient_new_with_cmnd(const char * type, ...);
int		mclient_cmnd_new(MClient * client, const char * type, ...);
int		mclient_cmnd_append(MClient * client, const char * cmnd);
int		mclient_process(MClient * client);
char *		mclient_nth_value(MClient * client, uint32_t index);
char *		mclient_nth_key(MClient * client, uint32_t index);
void		mclient_free(void * client);

int		mgmt_lib_initialize(void);
void		mgmt_lib_finalize(void);
#endif
