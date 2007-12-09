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


#include <hb_config.h>
#include <pthread.h>
#include "mgmt_client.h"

#define LIB_INIT_ALL (ENABLE_LRM|ENABLE_CRM)


#undef DEBUG_ENTER
#undef DEBUG_LEAVE

#define DEBUG_ENTER() 
#define DEBUG_LEAVE() 

const char *     module_name = "cim";

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

int
mgmt_lib_initialize(void)
{
        init_mgmt_lib(module_name, LIB_INIT_ALL);
        return HA_OK;
}

void 
mgmt_lib_finalize(void)
{
        final_mgmt_lib(); 
        return;
}

MClient *
mclient_new (void)
{
        MClient * client;

        /* lib must be init before every thing,
		otherwise mgmt_malloc ... will not be
		set to cl_malloc... */
        mgmt_lib_initialize();
        client = (MClient *)cim_malloc(sizeof(MClient));
        if ( client == NULL ) { 
		cl_log(LOG_ERR, "mclient_new: failed to malloc client.");
		return NULL; 
	}
        memset(client, 0, sizeof(MClient));

	client->cmnd = NULL;
	client->rdata = NULL;
	client->rlen  = 0;        
	
        return client;
}

MClient *
mclient_new_with_cmnd(const char * type, ... ) 
{
	MClient *	client;
        va_list 	ap;

	if ( ( client = mclient_new()) == NULL ) {
		cl_log(LOG_ERR, "mclient_new_with_cmnd: can't alloc client.");
		return NULL;
	}

        /* alloc msg */
	mclient_cmnd_append(client, type);
	va_start(ap, type);
        while (1) {
                char * arg = va_arg(ap, char *);
                if ( arg == NULL ) { break; }
		mclient_cmnd_append(client, arg);
        }
        va_end(ap);
        return client;
}

void
mclient_free(void * c)
{
	MClient * client = (MClient *)c;

	if ( client == NULL ) {
		return;
	}
        if ( client->rdata ) {
                mgmt_del_args(client->rdata);
        }
	if (client->cmnd ) {
		mgmt_del_msg(client->cmnd);
	}
        cim_free(client);

       	/* cleanup lib */ 
	mgmt_lib_finalize();
}

int
mclient_cmnd_new(MClient * client, const char * type, ...) 
{
        va_list  ap;
	int      rc = HA_FAIL;

	if ( client->cmnd ) {
		mgmt_del_msg(client->cmnd);
		client->cmnd = NULL;
	}
        /* alloc msg */
	mclient_cmnd_append(client, type);

	va_start(ap, type);
        while (1) {
                char * arg = va_arg(ap, char *);
                if ( arg == NULL ) { break; }
                mclient_cmnd_append(client, arg);
        }
        va_end(ap);
        return rc;
}

int
mclient_cmnd_append(MClient * client, const char * cmnd)
{
	if (client->cmnd == NULL ) {
		client->cmnd = mgmt_new_msg(cmnd, NULL);
	} else {
		client->cmnd = mgmt_msg_append(client->cmnd, cmnd);
	}
	return HA_OK;
}


int
mclient_process(MClient * client)
{
	char *   result;
	int      n, rc;
	char **  args = NULL;


	pthread_mutex_lock(&client_mutex);
	result = process_msg(client->cmnd);
	pthread_mutex_unlock(&client_mutex);

        if ( result == NULL ) {
		cl_log(LOG_ERR, "mclient_process: failed to process: %s", 
			client->cmnd);
		rc = MC_ERROR;
		goto exit2;
	}
        cl_log(LOG_INFO, "%s: cmnd: [%s], result: [%s].", 
			__FUNCTION__, client->cmnd, result);

	if ( ! mgmt_result_ok(result) )  {
                cl_log(LOG_WARNING, "mclient_process: client return \'failed\'.");
		cl_log(LOG_WARNING, "mclient_process: cmnd %s", client->cmnd);
		cl_log(LOG_WARNING, "mclient_process: %s", result);
		rc = MC_FAIL;
                goto exit1;
        }

	/* free rdata if not NULL */
	if ( client->rdata ) {
                mgmt_del_args(client->rdata);
        }
	client->rlen = 0;
	client->rdata = NULL;

        /* parse args */
        if ( ( args = mgmt_msg_args(result, &n) ) == NULL ) {
		cl_log(LOG_ERR, "do_process_cmnd: parse args failed.");
		rc = MC_ERROR;
		goto exit1;
	}

	client->rlen = n - 1;
        client->rdata = args;
        rc = MC_OK;
exit1:
	mgmt_del_msg(result);
exit2:
	return rc;
}

char *
mclient_nth_value(MClient * client, uint32_t index) 
{
	if ( client == NULL ||client->rdata == NULL) {
		cl_log(LOG_ERR, "mclient_nth_value: parameter error.");
		return NULL;
	}
	if ( index >= client->rlen ) {
		cl_log(LOG_ERR, "mclient_nth_value: index:%d, len:%d.",
			index, client->rlen);
		return NULL;
	}
	/*
	cl_log(LOG_INFO, "mclient_nth_value: got value %u:%s", 
			index, client->rdata[index + 1]);
	*/
        return client->rdata[index + 1];
}

