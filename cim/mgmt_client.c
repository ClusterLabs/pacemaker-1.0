#include "mgmt_client.h"

#define LIB_INIT_ALL (ENABLE_HB|ENABLE_LRM|ENABLE_CRM)


/* #define	MCLIENT_DEBUG_CLASS (1<<10) */
 #define	MCLIENT_DEBUG_CLASS (0) 
#define mclient_debug(p,fmt...) \
		cim_debug(MCLIENT_DEBUG_CLASS,p,##fmt)

#undef DEBUG_ENTER
#undef DEBUG_LEAVE

#define DEBUG_ENTER() \
	mclient_debug(LOG_INFO, "%s: --- ENTER ---", __FUNCTION__)
#define DEBUG_LEAVE() \
	mclient_debug(LOG_INFO, "%s: --- LEAVE ---", __FUNCTION__)

const char *     module_name = "cim";
static int       mgmt_lib_initialize(void);
static void      mgmt_lib_finalize(void);


int
mgmt_lib_initialize(void)
{
        init_mgmt_lib(module_name, LIB_INIT_ALL);
        return HA_OK;
}

static void 
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

	cim_debug_set(MCLIENT_DEBUG_CLASS);
	DEBUG_ENTER();
        client = (MClient *)cim_malloc(sizeof(MClient));
        if ( client == NULL ) { 
		return NULL; 
		cl_log(LOG_ERR, "mclient_new: failed to malloc client.");
	}
        memset(client, 0, sizeof(MClient));

	client->cmnd = NULL;
	client->rdata = NULL;
	client->rlen  = 0;        
	DEBUG_LEAVE();
	
        return client;
}

MClient *
mclient_new_with_cmnd(const char * type, ... ) 
{
	MClient *	client;
        va_list 	ap;

       	DEBUG_ENTER(); 
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
	DEBUG_LEAVE();
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
       
	DEBUG_ENTER(); 
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
	DEBUG_LEAVE();
        return rc;
}

int
mclient_cmnd_append(MClient * client, const char * cmnd)
{
	DEBUG_ENTER();
	if (client->cmnd == NULL ) {
		client->cmnd = mgmt_new_msg(cmnd, NULL);
	} else {
		client->cmnd = mgmt_msg_append(client->cmnd, cmnd);
	}
	DEBUG_LEAVE();
	return HA_OK;
}


int
mclient_process(MClient * client)
{
	char *   result;
	int      n;
	int      rc = HA_FAIL;
	char **  args = NULL;

	DEBUG_ENTER();        


        if ( ( result = process_msg(client->cmnd) ) == NULL ) {
		cl_log(LOG_ERR, "do_process_cmnd: failed to process: %s", 
			client->cmnd);
		goto exit2;
	}
        cl_log(LOG_INFO, "%s: cmnd: [%s], result: [%s].", 
			__FUNCTION__, client->cmnd, result);

	if ( ! mgmt_result_ok(result) )  {
                cl_log(LOG_ERR, "do_process_cmnd: result error.");
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
		goto exit1;
	}

	client->rlen = n - 1;
        client->rdata = args;
        rc = HA_OK;
exit1:
	mgmt_del_msg(result);
exit2:
	DEBUG_LEAVE();
	return rc;
}

char *
mclient_nth_value(MClient * client, uint32_t index) 
{
	DEBUG_ENTER();
	if ( client == NULL ||client->rdata == NULL) {
		cl_log(LOG_ERR, "mclient_nth_value: parameter error.");
		return NULL;
	}
	if ( index >= client->rlen ) {
		cl_log(LOG_ERR, "mclient_nth_value: index:%d, len:%d.",
			index, client->rlen);
		return NULL;
	}
	cl_log(LOG_INFO, "mclient_nth_value: got value %u:%s", 
			index, client->rdata[index + 1]);
	DEBUG_LEAVE();
        return client->rdata[index + 1];
}

