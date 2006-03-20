#ifndef _MGMT_CLIENT_H
#define _MGMT_CLIENT_H

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <hb_api.h>
#include <saf/ais.h>
#include <clplumbing/cl_uuid.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <mgmt/mgmt.h>
#include "utils.h"

#define Arg1(a)               a, NULL
#define Arg2(a,b)             a, b, NULL
#define Arg3(a,b,c)           a, b, c, NULL
#define Arg4(a,b,c,d)         a, b, c, d, NULL
#define Arg5(a,b,c,d,e)       a, b, c, d, e, NULL
#define Arg6(a,b,c,d,e,f)     a, b, c, d, e, f, NULL

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

MClient *	mclient_new(void);
MClient * 	mclient_new_with_cmnd(const char * type, ...);
int		mclient_cmnd_new(MClient * client, const char * type, ...);
int		mclient_cmnd_append(MClient * client, const char * cmnd);
int		mclient_process(MClient * client);
char *		mclient_nth_value(MClient * client, uint32_t index);
char *		mclient_nth_key(MClient * client, uint32_t index);
void		mclient_free(void * client);
#endif
