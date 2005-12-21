 /*
 * cluster_info.c
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

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <hb_api.h>
#include <mgmt/mgmt.h>
#include <heartbeat.h>
#include "cluster_info.h"

const char * nodeinfo_keys   [] = {"name", "type", "status", 
                                   "active_status", "uuid"};

const char * exp_keys        [] = { "id", "attribute", "operation", 
                                    "value", "type" };
const char * date_exp_keys   [] = {"id", "operation", "start", "end" };
const char * rule_keys       [] = {"id", "operation"};

const char * nvpair_keys     [] = {"id", "name", "value" };

const char * op_keys         [] = {"id", "name", "interval", "prereq", 
                                   "on_fail", "description" };

const char * primitive_keys  [] = {"id", "is_managed", "on_stopfail", 
                                   "restart_type", "multiple_active", 
                                   "resource_stickness", "start_prereq",
                                   "class", "type", "provider"};

const char * group_keys      [] = {"id", "is_managed", "on_stopfail", 
                                   "restart_type", "multiple_active",
                                   "resource_stickness", "start_prereq"};

const char * clone_keys      [] = {"id", "is_managed", "on_stopfail", 
                                   "restart_type", "multiple_active",
                                   "resource_stickness", "start_prereq",
                                   "notify", "ordered", "interleave"};

const char * master_keys      [] = {"id", "is_managed", "on_stopfail", 
                                   "restart_type", "multiple_active",
                                   "resource_stickness", "start_prereq",
                                   "notify", "ordered", "interleave",
                                   "max_masters", "max_node_masters"};


const char * order_keys      [] = {"id", "from", "action", "type", "to", 
                                   "symtrical"};
const char * localtion_keys  [] = {"id", "rsc"};
const char * colocation_keys [] = {"id", "from", "to", "score"};

typedef unsigned int POINTER_t;
/*************************************
 * mgmt lib
 *************************************/

/* this make mgmt lib happy */
const char * module_name = "cim";

/* this may save time, but :
   when to destroy it? how to invalidate it?
   and threadsafe? */
GHashTable * G_result_cache = NULL;

#define ClientGetAt(x,i)  (x)->get_nth_value(x, i)
#define ClientSize(x)     (x)->rlen

struct mgmt_client {
        char ** rdata;
        int rlen;

        int (* process_cmnd)(struct mgmt_client * client, const char * type, ...);
        char * (* get_nth_value)(struct mgmt_client * client, uint32_t index);
        char * (* get_nth_key)(struct mgmt_client * client, uint32_t index);
        uint32_t  (* get_size)(struct mgmt_client * client);
        void (* free)(struct mgmt_client * client);
};

static int lib_ref_count = 0;
int 
ci_lib_initialize(void)
{
        if ( lib_ref_count == 0 ) {
                /* not init yet, init it  */
                init_mgmt_lib(module_name, LIB_INIT_ALL);
        }
        lib_ref_count ++;
        return HA_OK;
}

void 
ci_lib_finalize(void)
{
        lib_ref_count --;
        if ( lib_ref_count == 0) {
                /* nobody use it, free it now */
                final_mgmt_lib();
        }
        return;
}

static int
mgmt_client_process_cmnd(struct mgmt_client * client,  const char * type, ... ) 
{
        char * msg = NULL;
        char * result = NULL;
	char ** args = NULL;
        va_list ap;
        int n, rc;
        
        rc = HA_FAIL;

        /* init mgmt lib */
        ci_lib_initialize();

        /* alloc msg */
        if ( type == NULL || ( msg = mgmt_new_msg(type, NULL)) == NULL ) {
        	goto out;
	}
        
	/* set up msg */
        va_start(ap, type);
        while (1) {
                char * arg = va_arg(ap, char *);
                if ( arg == NULL ) break;
                msg = mgmt_msg_append(msg, arg);
        }
        va_end(ap);
        
        /*
        if ( G_result_cache ) { 
                result = (char *)g_hash_table_lookup(G_result_cache, msg);
        } else {
                G_result_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                       CIM_FREE, NULL); 
        }
        */

        if ( result == NULL) {
        	/* send */
                if ( ( result = process_msg(msg) ) == NULL ) {
		        goto out;
                }
                /* insert the result to cache */ 
                /*
                g_hash_table_insert(G_result_cache, CIM_STRDUP(msg), result);
                */
        } 

        cl_log(LOG_INFO, "%s: msg = [%s], result = [%s]", __FUNCTION__, msg, result);
	if ( ! mgmt_result_ok(result) )  {
                cl_log(LOG_INFO, "result failed.");
                goto out;
        }
        /* parse args */
        if ( ( args = mgmt_msg_args(result, &n) ) == NULL ) {
                goto out;
	}

	client->rlen = n - 1 > 0? n - 1 : 0;
        client->rdata = ++args;

        rc = HA_OK;
out:	
        if (msg) mgmt_del_msg(msg);
	if (result) mgmt_del_msg(result);

        ci_lib_finalize();
        return rc;
}

static char *
mgmt_client_get_nth_value(struct mgmt_client * client, uint32_t index) 
{
        if ( client == NULL ) return NULL;

	if ( index >= client->rlen ) {
		return NULL;
	}

        return client->rdata[index];
}

static void
mgmt_client_free(struct mgmt_client * client)
{
        char ** args;
        if (client == NULL ) return ;
        if ( client->rdata ) {
                args = --client->rdata;
                if ( args ) mgmt_del_args(args);
        }

        CIM_FREE(client);
}

static uint32_t
mgmt_client_get_size(struct mgmt_client * client)
{
        if ( client == NULL  ) {
                return 0;
        }
        return client->rlen;
}

static struct mgmt_client *
mgmt_client_new (void)
{
        struct mgmt_client * client;
        client = (struct mgmt_client *)CIM_MALLOC(sizeof(struct mgmt_client));
        memset(client, 0, sizeof(struct mgmt_client));
        
        client->free = mgmt_client_free;
        client->get_nth_value = mgmt_client_get_nth_value;
        client->get_nth_key = NULL;
        client->process_cmnd = mgmt_client_process_cmnd;
        client->get_size = mgmt_client_get_size;
        client->free = mgmt_client_free;

        return client;
}

/*******************************************
 * Data Table
 ********************************************/

#define CITablePrivate(x) ((struct ci_table_private *)x->private)
#define CITableInsert(x, key, value, type)                   \
        CITablePrivate(x)->insert_value(CITablePrivate(x), key, value, type)

#define CITableAdd(x, value, type)                  \
        CITablePrivate(x)->add_value(CITablePrivate(x), value, type)

struct ci_table_private {

        GPtrArray * array;
        GHashTable * map;

        int withkey;

        int (* insert_value)(struct ci_table_private *, 
                             const char * key, const void * value, int type);
        int (* add_value)(struct ci_table_private *, const void * value, 
                          int type);

        void (* free) (struct ci_table_private *);
};


static const struct ci_data zero_data = {NULL, {0}, 0};

/* data */
static struct ci_data *
make_table_data(const char * key, const void * value, int type)
{
        struct ci_data * data;
        
        if ( value == NULL ) return NULL;
        if ( ( data = (struct ci_data *)
               CIM_MALLOC(sizeof(struct ci_data))) == NULL ) {
                return NULL;
        }
        memset(data, 0, sizeof(struct ci_data));

        if ( key != NULL ) {
                data->key = CIM_STRDUP(key);
                if ( data->key == NULL ) {
                        CIM_FREE(data);
                        return NULL;
                }
        } else {
                data->key = NULL;
        }

        if ( type == CI_uint32 ) {
                data->value.uint32 = *(const uint32_t *)value;
                data->type = CI_uint32;
        } else if ( type == CI_string ) {
                data->value.string = CIM_STRDUP((const char *)value);
                data->type = CI_string;
        } else if ( type == CI_table ) {
		typedef struct ci_table * ci_p_table_t;
		const ci_p_table_t * p_table= (const ci_p_table_t *) value;
		data->value.table = (struct ci_table *)*p_table;
		data->type = CI_table;
	} else {
                cl_log(LOG_ERR, "make_table_data: unknown data type: %d", type);
                return NULL;
        }
        return data;
}


/* private */
/* with key, table as a dict */
static int 
ci_table_private_insert_value(struct ci_table_private * private, 
                              const char * key, const void * value, int type)
{
        struct ci_data * data;
        if ( key == NULL || value == NULL ) return HA_FAIL;   

        if ( ! private->withkey ) {
                cl_log(LOG_INFO, "add: without key, please use add instead");
                return HA_FAIL;
        }

        if ( ( data = make_table_data(key, value, type)) == NULL ) {
                return HA_FAIL;
        }

        /* should be atomic */
        cl_log(LOG_INFO, "insert %s to (private: 0x%0x, array: 0x%0x), type: %d", 
               key, (POINTER_t)private, (POINTER_t)private->array, type);
        g_ptr_array_add(private->array, data);

        cl_log(LOG_INFO, "insert %s to (private: 0x%0x, map: 0x%0x), type: %d", 
               key, (POINTER_t)private, (POINTER_t)private->map, type);
        g_hash_table_insert(private->map, CIM_STRDUP(key), data);

        return HA_OK;
}

/* without key, table as an array */
static int
ci_table_private_add_value(struct ci_table_private * private, 
                             const void * value, int type)
{
        struct ci_data * data;
        if (  value == NULL ) return HA_FAIL;   
        
        if ( private->withkey ) {
                cl_log(LOG_INFO, "add: withkey, please use insert instead");
                return HA_FAIL;
        }

        if ( ( data = make_table_data(NULL, value, type)) == NULL ) {
                return HA_FAIL;
        }

        g_ptr_array_add(private->array, data);
        return HA_OK;
}

static void
ci_table_private_free(struct ci_table_private * private)
{
        int i;
        if ( private == NULL ) return;
        if ( private->array == NULL ) {
                CIM_FREE(private);
                return;
        }

        cl_log(LOG_INFO, "freeing (private: 0x%0x)", (POINTER_t)private);
        for ( i = 0; i < private->array->len; i ++ ) {
                struct ci_data * data;
                data = (struct ci_data *) 
                        g_ptr_array_index(private->array, i);
                if ( data == NULL ) continue;
                if (data->key) CIM_FREE(data->key);
                switch(data->type){
                case CI_uint32:
                        break;
                case CI_string:
                        if (data->value.string)CIM_FREE(data->value.string);
                        break;
                case CI_table:
                        if ( data->value.table){
                                data->value.table->free(data->value.table);
                        }
                        break;
                }
                CIM_FREE(data);
        }
        g_ptr_array_free(private->array, 0);
        if ( private->withkey ) {
                g_hash_table_destroy(private->map);
        }
        CIM_FREE(private);
        cl_log(LOG_INFO, "(private: 0x%0x) freed", (POINTER_t)private);
}

static struct ci_table_private *
ci_table_private_new(int withkey)
{
        GPtrArray * array;
        struct ci_table_private * private;

        if ((private = 
             CIM_MALLOC(sizeof(struct ci_table_private))) == NULL ) {
                return NULL;
        }
        memset(private, 0, sizeof(struct ci_table_private));

        if (( array = g_ptr_array_new()) == NULL ) {
                CIM_FREE(private);
                return NULL;
        }

        private->insert_value = ci_table_private_insert_value;
        private->add_value = ci_table_private_add_value;

        private->free = ci_table_private_free;
        private->withkey = withkey;
        private->array = array;
        private->map = NULL;

        if ( withkey ) {
                GHashTable * map;
                map = g_hash_table_new_full(g_str_hash, g_str_equal, 
                                            CIM_FREE, NULL);
                if ( map == NULL ) {
                        g_ptr_array_free(array, 0);
                        CIM_FREE(private);
                        return NULL;
                }
                private->map = map;
        }

        return private;
}

/* table */
static uint32_t 
ci_table_get_data_size(const struct ci_table * table)
{
        struct ci_table_private * private;
        
        if (table == NULL ) return 0;
        if (( private = CITablePrivate(table)) == NULL ) {
                return 0;
        }
        if ( private->array == NULL ) {
                return 0;
        }
        cl_log(LOG_INFO, "table size is %d", private->array->len);
        return private->array->len;
}


static struct ci_data
ci_table_get_data(const struct ci_table * table, const char * key)
{
        struct ci_table_private * private;
        struct ci_data * data;
        if (table == NULL ){
                return zero_data;
        }

        if (( private = CITablePrivate(table)) == NULL ) {
                return zero_data;
        }
        if ( private->map == NULL ) {
                return zero_data;
        }

        data =(struct ci_data *)g_hash_table_lookup(private->map, key);
        if ( data == NULL ) return zero_data;
        cl_log(LOG_INFO, "got %s from (private: 0x%0x, map: 0x%0x), type: %d", 
               key, (POINTER_t)private, (POINTER_t)private->map, data->type);
        return * data;
}
        
static struct ci_data
ci_table_get_data_at(const struct ci_table * table, int index)
{
        struct ci_table_private * private;
        struct ci_data * data;

        if (table == NULL ) return zero_data;
        if (( private = CITablePrivate(table)) == NULL ) {
                return zero_data;
        }
        if ( private->array == NULL ) {
                return zero_data;
        }
        
        if ( private->array->len == 0 || index >= private->array->len) {
                return zero_data;
        }

        data = (struct ci_data *)g_ptr_array_index(private->array, index);
        if ( data == NULL ) return zero_data;

        cl_log(LOG_INFO, "got data %d from (private: 0x%0x, array: 0x%0x), type: %d", 
               index, (POINTER_t)private, (POINTER_t)private->array, data->type);
        
        return * data;
}


static GPtrArray * 
ci_table_get_keys(const struct ci_table * table)
{
        cl_log(LOG_WARNING, "%s: not implemented yet", __FUNCTION__);
        return NULL;
}


static void 
ci_table_free(struct ci_table * data)
{
        struct ci_table_private * private;

        if ( data == NULL ) return;
        private = (struct ci_table_private *) data->private;
        if (private) {
                private->free(private);
        }

        CIM_FREE(data);
}

struct ci_table *
ci_table_new (int withkey)
{
        struct ci_table * table;
        struct ci_table_private * private;

        if ( (table = CIM_MALLOC(sizeof(struct ci_table))) == NULL ) {
                return NULL;
        }
        memset(table, 0, sizeof(struct ci_table));
        if ( (private = ci_table_private_new(withkey)) == NULL ) {
                CIM_FREE(table);
                return NULL;
        }
        
        table->free = ci_table_free;
        table->get_keys = ci_table_get_keys;
        table->get_data_at = ci_table_get_data_at;
        table->get_data = ci_table_get_data;
        table->get_data_size = ci_table_get_data_size;

        table->private = (void *) private;
        
        return table;
}

/**********************************************
 * CRM
 *********************************************/

/*
description:
	return CRM configuration
format:
	MSG_CRM_CONFIG
return:
	MSG_OK transition_idle_timeout symmetric_cluster(True|False)
 	  stonith_enabled(True|False) no_quorum_policy(freeze|stop|ignore)
 	  default_resource_stickiness have_quorum(True|False)
or
	MSG_FAIL
*/
struct ci_table * 
ci_get_crm_config (void)
{
        struct ci_table * table;
        struct mgmt_client * client;
        const char * keys [] = {"transition_idle_timeout",
                                "symmetric_cluster",
                                "stonith_enabled",
                                "no_quorum_policy",
                                "default_resource_stickiness",
                                "have_quorum"};

        int i = 0;

        if (( client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_CRM_CONFIG, NULL) == HA_FAIL ){
                client->free(client);
                cl_log(LOG_ERR, "crm_config: process msg failed");
                return NULL;
        }

        if ( ( table = ci_table_new(TRUE)) == NULL ) {
                cl_log(LOG_ERR, "crm_config: couldn't alloc table");
                return NULL;
        }

        for ( i = 0; i < client->rlen; i++) {
                CITableInsert(table, keys[i], client->get_nth_value(client, i),
                              CI_string);
        }

        client->free(client);
        return table;
}


/********************************************
 * cluster
 *******************************************/

/*
format:
	MSG_HB_CONFIG
return:
	MSG_OK apiauth auto_failback baud debug debugfile deadping deadtime
	  hbversion hopfudge initdead keepalive logfacility logfile msgfmt
	  nice_failback node normalpoll stonith udpport warntime watchdog
or
	MSG_FAIL
*/


struct ci_table *
ci_get_cluster_config ()
{
        int i = 0;
        struct ci_table * table;
        struct mgmt_client * client;

        const char * keys [] = {"apiauth", "auto_failback", "baud", "debug", 
                                "debugfile", "deadping", "deadtime",
                                "hbversion", "hopfudge", "initdead", "keepalive", 
                                "logfacility", "logfile", "msgfmt", 
                                "nice_failback", "node", "normalpoll", "stonith", 
                                "udpport", "warntime", "watchdog"};

        if (( client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_HB_CONFIG, NULL) == HA_FAIL ){
                client->free(client);
                cl_log(LOG_ERR, "cluster_config: process msg failed");
                return NULL;
        }

        if ( ( table = ci_table_new(TRUE)) == NULL ) {
                cl_log(LOG_ERR, "cluster_config: couldn't alloc table");
                return NULL;
        }
 
        for ( i = 0; i < client->get_size(client); i ++ ) {
                char * value;
		if ( (value = client->get_nth_value(client, i)) == NULL ) {
                	continue;
		}
                CITableInsert(table, keys[i], value, CI_string);
        }
        
        client->free(client);
        return table;
}


char *
ci_get_cluster_dc ()
{
        struct mgmt_client * client;
	char * dc;

        if ( ( client  =mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_DC, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

	if ( ( dc = client->get_nth_value(client, 0)) == NULL ) {
                client->free(client);
		return NULL;
	}

        dc = CIM_STRDUP(dc);
        client->free(client);
        return dc;
}

/*
	MSG_NODE_CONFIG NODENAME
return:
	MSG_OK uname online(True|False) standby(True|False) unclean(True|False)
 	  shutdown(True|False) expected_up(True|False) is_dc(True|False)
	  node_ping("ping|member")
*/
struct ci_table *
ci_get_nodeinfo (const char * id)
{
        struct ci_table * nodeinfo = NULL;
        char * value;
	int i;
        struct mgmt_client * client;
        const char * keys [] = {"uname", "online", "standby", "unclean",
                                "shutdown", "expected_up", "is_dc", "node_ping"};

        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( (nodeinfo = ci_table_new(TRUE)) == NULL ) {
                return NULL;
        }
        
        CITableInsert(nodeinfo, "name", id, CI_string);

        /* get node's config info */
        if ( client->process_cmnd(client, MSG_NODE_CONFIG, id, NULL) == HA_FAIL ) { 
                CITableInsert(nodeinfo, "active_status", "False", CI_string);
                client->free(client);
                return nodeinfo;
        }
        
        /* if go here, this node must be active */
        CITableInsert(nodeinfo, "active_status", "True", CI_string);

        for ( i = 0; i < client->rlen; i++ ) {
		if ( (value = client->get_nth_value(client, i)) == NULL ) {
			continue;
		}
                CITableInsert(nodeinfo, keys[i], value, CI_string);
        }

        client->free(client);
        return nodeinfo;
}

GPtrArray *
ci_get_node_name_table ()
{
        int i = 0;
        GPtrArray * node_table = NULL;
        struct mgmt_client * client;

        if ( ( client = mgmt_client_new()) == NULL ) {
                return NULL;
        }
        if ( client->process_cmnd(client, MSG_ALLNODES, NULL)  == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        if ( ( node_table = g_ptr_array_new ()) == NULL ) {
                client->free(client);
                return NULL;
        }

        for( i = 0; i < client->get_size(client); i++ ) {
        	char * node = client->get_nth_value(client, i);
                if ( node == NULL ) continue;
                node = CIM_STRDUP(node);
                if ( node == NULL ) continue;
                g_ptr_array_add(node_table, node);
        };
        
        client->free(client);
        return node_table;
}


/***************************************************
 * resources
 **************************************************/


/* get operations of an resource instance,
   id: resource id,
   DTD1.0 says op may contain instance attributes,
   we just ignore this.
*/
struct ci_table *
ci_get_inst_operations (const char * id) {

	struct ci_table * ops;
        struct mgmt_client * client;
	int i;
        
        if ( ( client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

	if ( client->process_cmnd(client, MSG_RSC_OPS, id, NULL) == HA_FAIL ) {
                client->free(client);
		return NULL;
	}
	
        if ( ( ops = ci_table_new(TRUE)) == NULL ){
                client->free(client);
                return NULL;
        } 

	for ( i = 0; i < client->rlen/4; i++ ) {
		struct ci_table * op = NULL;
		char * id, * name, * interval, * timeout;
		if ( (op = ci_table_new (TRUE) ) == NULL ) {
			continue;
		}

		id = client->get_nth_value(client, i*4);
		name = client->get_nth_value(client, i*4 + 1);
		interval = client->get_nth_value(client, i*4 + 2);
		timeout = client->get_nth_value(client, i*4 + 3);

                CITableInsert(op, "id", id, CI_string);
                CITableInsert(op, "name", name, CI_string);
                CITableInsert(op, "interval", interval, CI_string);
                CITableInsert(op, "timeout", timeout, CI_string);
                
                /* op's id as key in ops */
                CITableInsert(ops, id, &op, CI_table);
	}	
	
        client->free(client);
	return ops; 	
}

/* get nvpairs of a instance attributes,
   id: resource id 
*/
struct ci_table *
ci_get_attributes(const char * id)
{
	int i;
        struct ci_table * attributes;
        struct mgmt_client * client;

        if ( ( client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

	if ( client->process_cmnd(client, MSG_RSC_PARAMS, id, NULL) == HA_FAIL ) {
                client->free(client);
		return NULL;
	}
        
	if ( ( attributes = ci_table_new (FALSE)) == NULL ) {
		return NULL;
	}
	
	for ( i = 0; i < client->get_size(client)/3; i++ ) {
		struct ci_table * pair = NULL;
		char * id, * name, * value;
		
		if ( (pair = ci_table_new(TRUE)) == NULL ) {
			continue;
		}

		id = client->get_nth_value(client, i);
		name = client->get_nth_value(client, i*3+1);
		value = client->get_nth_value(client, i*3+2);
                
                cl_log(LOG_INFO, "get_attibutes: id, name, value = %s, %s, %s",
                       id, name, value);
                CITableInsert(pair, "id", id, CI_string);
                CITableInsert(pair, "name", name, CI_string);
                CITableInsert(pair, "value", value, CI_string);

                CITableAdd(attributes, &pair, CI_table);
	}	
	
        client->free(client);
        return attributes;
}

/* get resource instance attributes, 
   although DTD1.0 says a resource section may contain 
   multiple instance_attributes, but we assume only one
   instance_attributes in a section for the moment.
*/
   
struct ci_table * 
ci_get_resource_instattrs_table(const char * id)
{
	struct ci_table * attr = NULL;
	struct ci_table * instattr = NULL;
        struct ci_table * array;

        if ( ( array = ci_table_new(FALSE)) == NULL ) {
                return NULL;
        }
        
        /* instattr->atrr->nvpairs */
	if ( ( instattr = ci_table_new (TRUE)) == NULL ) {
                array->free(array);
		return NULL;
	}

	attr = ci_get_attributes (id);
	if ( attr == NULL ) {
                attr->free(instattr);
		return NULL;
	}

        /* FIXME: instance attribute's id    */
        CITableInsert(instattr, "N/A", &attr, CI_table);
        CITableAdd(array, &instattr, CI_table);

	return array;
}

uint32_t
ci_get_resource_type(const char * id)
{
        struct mgmt_client * client;
        char * type = NULL;
        uint32_t rc = 0;

        if ( id == NULL) return 0;
        
        if ((client = mgmt_client_new()) == NULL ) {
                return 0;
        }

        if (client->process_cmnd(client, MSG_RSC_TYPE, id, NULL) == HA_FAIL ){
                client->free(client);
                return 0;
        }

        if ((type = client->get_nth_value(client, 0)) == NULL ) {
                client->free(client);
                return 0;
        }

        if ( strcmp(type, "native") == 0) {
                rc = TID_RES_PRIMITIVE;
        } else if (strcmp(type, "group") == 0 ) {
                rc = TID_RES_GROUP;
        } else if (strcmp(type, "clone") == 0 ) {
                rc = TID_RES_CLONE;
        } else if (strcmp(type, "master") == 0 ) {
                rc = TID_RES_CLONE;
        }

        client->free(client);
        return rc;
}


/* primitive resource */
struct ci_table *
ci_get_primitive_resource (const char * id)
{
        struct mgmt_client * client;
        struct ci_table * primitive;
        char * lid, * class, * provider, * type;

        if ((client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_RSC_ATTRS, id, NULL ) == HA_FAIL) {
                client->free(client);
                return NULL;
        }

        if ((primitive = ci_table_new(TRUE)) == NULL ) {
                client->free(client);
                return NULL;
        }

        lid = client->get_nth_value(client, 0);
        class = client->get_nth_value(client, 1);
        provider = client->get_nth_value(client, 2);
        type = client->get_nth_value(client, 3);

        CITableInsert(primitive, "id", id, CI_string);
        CITableInsert(primitive, "class", class, CI_string);
        CITableInsert(primitive, "provider", class, CI_string);
        CITableInsert(primitive, "type", type, CI_string);

        client->free(client);
        return primitive;
}


/* resource clone */
struct ci_table *
ci_get_resource_clone (const char * id) 
{
        struct ci_table * clone;
        char * clone_id, * clone_max, * clone_node_max;
        struct mgmt_client * client;

        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_GET_CLONE, id, NULL ) == HA_FAIL) {
                client->free(client);
                return NULL;
        }

	if ( (clone = ci_table_new (TRUE)) == NULL ) {
                client->free(client);
                return NULL;
        }

        /* so this is duplicated */
        clone_id = client->get_nth_value(client, 0);
        clone_max = client->get_nth_value(client, 1);
        clone_node_max = client->get_nth_value(client, 2);

        CITableInsert(clone, "id", id, CI_string);
        CITableInsert(clone, "clone_id", clone_id, CI_string);
        CITableInsert(clone, "clone_max", clone_max, CI_string);
        CITableInsert(clone, "clone_node_max", clone_node_max, CI_string);

        return clone;
}

/* master-slave resource */
/*
	MSG_OK master_id  clone_max clone_node_max
		master_max master_max_node
*/
struct ci_table *
ci_get_master_resource (const char * id)
{
        struct ci_table * master;
        char * master_max, * master_max_node;
        struct mgmt_client * client;

        if (( client = mgmt_client_new()) == NULL ) {
                return NULL;
        }
        if ( client->process_cmnd(client, MSG_GET_MASTER, id, NULL ) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        if ( (master = ci_table_new (TRUE)) == NULL ) {
                client->free(client);
                return NULL;
        }

        master_max = client->get_nth_value(client, 3);
        master_max_node = client->get_nth_value(client, 4);

        CITableInsert(master, "id", id, CI_string);
        CITableInsert(master, "master_max", master_max, CI_string);
        CITableInsert(master, "master_node_node", master_max_node, CI_string);
        
        client->free(client);
        return master;
}


/* get primitive resource name table for a given resource group */
GPtrArray *
ci_get_sub_resource_name_table ( const char * id)
{
        struct mgmt_client * client;
        GPtrArray * array;
        int i = 0;
        
        if ( (client = mgmt_client_new ()) == NULL ) {
                return NULL;
        }

        /* get all sub resources for this group */
        if ( client->process_cmnd(client, MSG_SUB_RSC, id, NULL ) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        if ( (array = g_ptr_array_new()) == NULL ) {
                client->free(client);
                return NULL;
        }


        for ( i = 0; i < client->get_size(client); i++ ) {
                char * sub_rsc_id;
                if ( (sub_rsc_id = client->get_nth_value(client, i)) == NULL ) {
                        continue;
                }
                
                if ( (sub_rsc_id = CIM_STRDUP(sub_rsc_id)) == NULL ) {
                        continue;
                }
                /* and add to the primitive table */
                g_ptr_array_add(array, sub_rsc_id);
        }

        client->free(client);
        return array;
}


/* resource group */
struct ci_table *
ci_get_resource_group ( const char * id)
{
	struct ci_table * group;

	if ((group = ci_table_new (TRUE)) == NULL ) {
                return NULL;
        }
        
        CITableInsert(group, "id", id, CI_string);
	return group;
}

struct ci_table *
ci_get_resource_name_table ()
{
	struct ci_table * name_table = NULL;
        struct mgmt_client * client;
        int i;
        

        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        /* get all resources names */
        if (client->process_cmnd(client, MSG_ALL_RSC, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        if ((name_table = ci_table_new(FALSE)) == NULL ) {
                client->free(client);
                return NULL;
        }
        
        for ( i = 0; i < client->get_size(client); i++ ) {
                char * rsc_id = NULL;
                if ((rsc_id = client->get_nth_value(client, i )) == NULL ) {
                        continue;
                }

                CITableAdd(name_table, rsc_id, CI_string);
        }
        
        client->free(client);
	return name_table;
}

/* return the node that host the resource */
char *
ci_get_res_running_node(const char * id)
{
        char * node;
        struct mgmt_client * client;
        
        if ( id == NULL ) return NULL;
        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_RSC_RUNNING_ON, id, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        if ( ( node = client->get_nth_value(client, 0)) == NULL ) {
                client->free(client);
                return NULL;
        }
        node = CIM_STRDUP(node);

        client->free(client);
        return node;
}


char * 
ci_get_resource_status(const char * id)
{
        char * status;
        struct mgmt_client * client;
        
        if ( id == NULL ) return NULL;
        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_RSC_STATUS, id, NULL) == HA_FAIL) {
                client->free(client);
                return NULL;
        }
        
        if ( (status = client->get_nth_value(client, 0)) == NULL ) {
                client->free(client);
                return NULL;
        }
        status = CIM_STRDUP(status);
        client->free(client);
        return status;

}
/****************************************************
 * constraints 
 ***************************************************/


/* get a constraint by constaint id */
struct ci_table *
ci_get_order_constraint (const char * id)
{
        struct ci_table * rsc_order = NULL;
	char * lid, * from, * type, * to;
        struct mgmt_client * client;
        
        if ( id == NULL ) return NULL;
        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_GET_CONSTRAINT, STR_CONS_ORDER, 
                                  id, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        rsc_order = ci_table_new (TRUE);
        if ( rsc_order == NULL ) {
                return NULL;
        }

	lid = client->get_nth_value(client, 0);
	from = client->get_nth_value(client, 1);
	type = client->get_nth_value(client, 2);
	to = client->get_nth_value(client, 3);

        CITableInsert(rsc_order, "id", id, CI_string);
        CITableInsert(rsc_order, "from", from, CI_string);
        CITableInsert(rsc_order, "type", type, CI_string);
        CITableInsert(rsc_order, "to", to, CI_string);

        client->free(client);
	return rsc_order;
}

/* due to the limitation of Huan Zhen's lib, 
   there is only on rule for the moment */
/*
	rsc_location:
		MSG_OK id resource score 
                        expr_id1 attribute1 operation1 value1
			expr_id2 attribute2 operation2 value2 ...
			expr_idn attributen operationn valuen
*/
struct ci_table *
ci_get_location_constraint (const char * id)
{
        struct ci_table * rsc_location = NULL;
        char * rsc_id, * score;
        struct ci_table * exp;
        struct ci_table * rule;
        struct mgmt_client * client;
        int i;

        if ( id == NULL ) return NULL;
        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_GET_CONSTRAINT, STR_CONS_LOCATION, 
                                  id, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        if ((rsc_location = ci_table_new(TRUE)) == NULL ) {
                return NULL;
        }
        
        rsc_id = client->get_nth_value(client, 1);
        score = client->get_nth_value(client, 2);

        CITableInsert(rsc_location, "id", id, CI_string);
        CITableInsert(rsc_location, "resource", rsc_id, CI_string);
        CITableInsert(rsc_location, "score", score, CI_string);

        /* create a rule */
        if ((rule = ci_table_new(TRUE)) == NULL ) {
                goto out;
        }

        for ( i = 0; i < (client->rlen - 3) / 4; i ++ ) {
                /* create a expression */
                if ((exp = ci_table_new(TRUE)) == NULL ) {
                        continue;
                }
                CITableInsert(exp, "id", client->get_nth_value(client, i*4 + 3), CI_string);
                CITableInsert(exp, "attribute", client->get_nth_value(client, i*4 + 4), CI_string);
                CITableInsert(exp, "operation", client->get_nth_value(client, i*4 + 5), CI_string);
                CITableInsert(exp, "value", client->get_nth_value(client, i*4 + 6), CI_string);
        
                /* id as key */
                CITableInsert(rule, id, &exp, CI_table);
        }

        CITableInsert(rsc_location, "rule", &rule, CI_table);
 out:
        client->free(client);
        return rsc_location;
}


struct ci_table *
ci_get_colocation_constraint (const char * id)
{
        struct ci_table * rsc_colocation = NULL;
	char * lid, * from, * to, * score;
        struct mgmt_client * client;

        if ( id == NULL ) {
                return NULL;
        }
        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_GET_CONSTRAINT, 
                                  STR_CONS_COLOCATION, id, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }


        rsc_colocation = ci_table_new (TRUE);
        if ( rsc_colocation == NULL ) {
                client->free(client);
                return NULL;
        }
        
	lid = client->get_nth_value(client, 0);
	from = client->get_nth_value(client, 1);
	to = client->get_nth_value(client, 2);
	score = client->get_nth_value(client, 3);

        CITableInsert(rsc_colocation, "id", id, CI_string);
        CITableInsert(rsc_colocation, "from", from, CI_string);
        CITableInsert(rsc_colocation, "to", to, CI_string);
        CITableInsert(rsc_colocation, "score", score, CI_string);

        client->free(client);
        return rsc_colocation;

}

GPtrArray *
ci_get_constraint_name_table (uint32_t type)
{
        GPtrArray * table = NULL;
        int i = 0;
        const char * arg;
        struct mgmt_client * client;
	
        switch(type) {
        case TID_CONS_ORDER:
                arg = STR_CONS_ORDER;
                break;
        case TID_CONS_LOCATION:
                arg = STR_CONS_LOCATION;
                break;
        case TID_CONS_COLOCATION:
                arg = STR_CONS_COLOCATION;
                break;
        default:
                return NULL;
        }

        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }
        if ( client->process_cmnd(client, MSG_GET_CONSTRAINTS, arg, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

	if ( (table = g_ptr_array_new () ) == NULL ) {
                client->free(client);
		return NULL;
	}

        for ( i = 0; i < client->get_size(client); i++ ) {
                char * consid = client->get_nth_value(client, i);
                if ( consid == NULL ) continue;
                if ( (consid = CIM_STRDUP(consid)) == NULL ) {
                        continue;
                }
                g_ptr_array_add(table, consid);
        }

        client->free(client);
        return table;
}

/***************************************************
 * metadata
 **************************************************/

/* do we need a tree to maintain the relationship of 
   classes, types and providers ? */

GPtrArray *
ci_get_res_class_table ()
{
	GPtrArray * classes;
	int i;
        struct mgmt_client * client;

        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

	if ( ( classes = g_ptr_array_new ()) == NULL ) {
                client->free(client);
		return NULL;
	}

	if ( client->process_cmnd(client, MSG_RSC_CLASSES, NULL) == HA_FAIL ) {
                client->free(client);
		return NULL;
	}

	for ( i = 0; i < client->get_size(client); i ++ ) {
		char * class = client->get_nth_value(client, i);
		if ( class == NULL ) continue;
		g_ptr_array_add(classes, CIM_STRDUP(class));
	} 
        client->free(client);
	return classes;
}

GPtrArray *
ci_get_res_type_table (const char * class)
{
        GPtrArray * types;
        int i;
        struct mgmt_client * client;

        if ( class == NULL ) return NULL;
        if ( (client = mgmt_client_new()) == NULL ) {
                return NULL;
        }
        if (( types = g_ptr_array_new ()) == NULL  ) {
                client->free(client);
                return NULL;
        }
        
        if ( client->process_cmnd(client, MSG_RSC_TYPES, class, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }

        for ( i = 0; i < client->get_size(client); i++ ) {
                char * type = client->get_nth_value(client, i);
                if ( type == NULL ) continue;
                
                g_ptr_array_add(types, CIM_STRDUP(type));
        }

        client->free(client);
	return types;
}

GPtrArray *
ci_get_res_provider_table(const char * class, const char * type)
{
        GPtrArray * providers;
        int i;
        struct mgmt_client * client;

        if ( class == NULL || type == NULL ) return NULL;
        if ((client = mgmt_client_new()) == NULL ) {
                return NULL;
        }

        if ( client->process_cmnd(client, MSG_RSC_PROVIDERS, class, NULL) == HA_FAIL ) {
                client->free(client);
                return NULL;
        }
        if (( providers = g_ptr_array_new ()) == NULL  ) {
                client->free(client);
                return NULL;
        }

        for ( i = 0; i < client->get_size(client); i++ ) {
                char * provider = client->get_nth_value(client, i);
                if ( provider == NULL ) continue;
                g_ptr_array_add(providers, CIM_STRDUP(provider));
        }

        client->free(client);
        return providers;

}

GPtrArray *
ci_get_metadata_table(const char * class, const char * type, const char * provider)
{
	GPtrArray * lines = NULL;

	return lines;
}


/***************************************
 * utils
 **************************************/
int 
init_logger(const char * entity)
{
        char * inherit_debuglevel;
        int debug_level = 2;
 
	inherit_debuglevel = getenv(HADEBUGVAL);
	if (inherit_debuglevel != NULL) {
		debug_level = atoi(inherit_debuglevel);
		if (debug_level > 2) {
			debug_level = 2;
		}
	}

	cl_log_set_entity(entity);
	cl_log_enable_stderr(debug_level?TRUE:FALSE);
	cl_log_set_facility(LOG_DAEMON);
        return HA_OK;
}

int 
run_shell_command(const char * cmnd, int * ret, 
			char *** std_out, char *** std_err)
				/* std_err not used currently */
{
	FILE * fstream = NULL;
	char buffer [4096];
	int cmnd_rc, rc, i;

	if ( (fstream = popen(cmnd, "r")) == NULL ){
		return HA_FAIL;
	}

	if ( (*std_out = CIM_MALLOC(sizeof(char*)) ) == NULL ) {
                return HA_FAIL;
        }

	(*std_out)[0] = NULL;

	i = 0;
	while (!feof(fstream)) {

		if ( fgets(buffer, 4096, fstream) != NULL ){
			/** add buffer to std_out **/
			*std_out = realloc(*std_out, (i+2) * sizeof(char*));	
                        if ( *std_out == NULL ) {
                                rc = HA_FAIL;
                                goto exit;
                        }
                        (*std_out)[i] = CIM_STRDUP(buffer);
			(*std_out)[i+1] = NULL;		
		}else{
			continue;
		}
		i++;

	}
	
	rc = HA_OK;
exit:
	if ( (cmnd_rc = pclose(fstream)) == -1 ){
		/*** WARNING log ***/
                cl_log(LOG_WARNING, "failed to close pipe.");
	}
	*ret = cmnd_rc;
	return rc;
}

int
regex_search(const char * reg, const char * str, char *** match)
{
	regex_t regexp;
	const size_t nmatch = 16;	/* max match times */
	regmatch_t pm[16];
	int i;
	int ret;

	ret = regcomp(&regexp, reg, REG_EXTENDED);
	if ( ret != 0) {
		cl_log(LOG_ERR, "Error regcomp regex %s", reg);
		return HA_FAIL;
	}

	ret = regexec(&regexp, str, nmatch, pm, 0);
	if ( ret == REG_NOMATCH ){
		regfree(&regexp);
		return HA_FAIL;
	}else if (ret != 0){
        	cl_log(LOG_ERR, "Error regexec\n");
		regfree(&regexp);
		return HA_FAIL;
	}


	*match = CIM_MALLOC(sizeof(char*));
        if ( *match == NULL ) {
                regfree(&regexp);
                return HA_FAIL;
        }

	(*match)[0] = NULL;


	for(i = 0; i < nmatch && pm[i].rm_so != -1; i++){
		int str_len = pm[i].rm_eo - pm[i].rm_so;

		*match = CIM_REALLOC(*match, (i+2) * sizeof(char*));
                if ( *match == NULL ) {
                        regfree(&regexp);
                        return HA_FAIL;
                }

		(*match)[i] = CIM_MALLOC(str_len + 1);
                if ( (*match)[i] == NULL ) {
                        free_2d_array(*match);
                        regfree(&regexp);
                        return HA_FAIL;
                }

		strncpy( (*match)[i], str + pm[i].rm_so, str_len);
		(*match)[i][str_len] = '\0';
		(*match)[i+1] = NULL;
	}

	regfree(&regexp);
	return HA_OK;
} 


int free_2d_array(char ** array){
	if (array) {
		int i = 0;
		while ( array[i] ){
			ci_safe_free(array[i]);
			i++;
		}
		
		ci_safe_free(array);
	}

	return HA_OK;
}

char * 
uuid_to_str(const cl_uuid_t * uuid){
        int i, len = 0;
        char * str = CIM_MALLOC(256);

        if ( str == NULL ) {
                return NULL;
        }

        memset(str, 0, 256);

        for ( i = 0; i < sizeof(cl_uuid_t); i++){
                len += sprintf(str + len, "%.2X", uuid->uuid[i]);
        }
        return str;
}



void
ci_assert(const char * assertion, int line, const char * file)
{
        cl_log(LOG_ERR, "Assertion \"%s\" failed on line %d in file \"%s\""
        ,       assertion, line, file);
        exit(1);
}

void
ci_free_ptr_array(GPtrArray * table, void (* free_ptr)(void * ptr))
{
        void * ptr;
        if ( table == NULL ) {
                return;
        }
        
        while ( table->len ) {
                ptr = (void *)g_ptr_array_remove_index_fast(table, 0);
                free_ptr(ptr);
        }
        g_ptr_array_free(table, 0);
}


void
ci_safe_free(void * ptr) {
        if (ptr) CIM_FREE(ptr);
}
