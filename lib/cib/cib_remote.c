/*
 * Copyright (c) 2008 Andrew Beekhof
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
#include <crm_internal.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <termios.h>
#include <sys/socket.h>

#include <glib.h>

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/ipc.h>
#include <cib_private.h>


#ifdef HAVE_GNUTLS_GNUTLS_H
#  undef KEYFILE
#  include <gnutls/gnutls.h>
extern gnutls_anon_client_credentials anon_cred_c;
extern gnutls_session *create_tls_session(int csock, int type);

const int kx_prio[] =
{
	GNUTLS_KX_ANON_DH,
	0
};

#else
typedef void gnutls_session;
#endif

#include <arpa/inet.h>
#include <sgtty.h>

#define DH_BITS 1024

struct remote_connection_s 
{
	int socket;
	gboolean encrypted;
	gnutls_session *session;
	GFDSource *source;
	char *token;
};

typedef struct cib_remote_opaque_s 
{
	int flags;
	int socket;
	int port;
	char *server;
	char *user;
	char *passwd;
	struct remote_connection_s command;
	struct remote_connection_s callback;
	
} cib_remote_opaque_t;

void cib_remote_connection_destroy(gpointer user_data);
gboolean cib_remote_dispatch(int fd, gpointer user_data);
int cib_remote_signon(cib_t* cib, const char *name, enum cib_conn_type type);
int cib_remote_signoff(cib_t* cib);
int cib_remote_free(cib_t* cib);

int cib_remote_perform_op(
    cib_t *cib, const char *op, const char *host, const char *section,
    xmlNode *data, xmlNode **output_data, int call_options);


static int cib_remote_inputfd(cib_t* cib) {
    cib_remote_opaque_t *private = cib->variant_opaque;
    return private->callback.socket;
}

static int cib_remote_set_connection_dnotify(
    cib_t *cib, void (*dnotify)(gpointer user_data))
{
    return cib_NOTSUPPORTED;
}

static int
cib_remote_register_notification(cib_t* cib, const char *callback, int enabled) 
{
    xmlNode *notify_msg = create_xml_node(NULL, "cib_command");
    cib_remote_opaque_t *private = cib->variant_opaque;
    
    crm_xml_add(notify_msg, F_CIB_OPERATION, T_CIB_NOTIFY);
    crm_xml_add(notify_msg, F_CIB_NOTIFY_TYPE, callback);
    crm_xml_add_int(notify_msg, F_CIB_NOTIFY_ACTIVATE, enabled);
    cib_send_remote_msg(private->callback.session, notify_msg, private->callback.encrypted);
    free_xml(notify_msg);
    return cib_ok;
}

cib_t*
cib_remote_new (const char *server, const char *user, const char *passwd, int port, gboolean encrypted)
{
    cib_remote_opaque_t *private = NULL;
    cib_t *cib = cib_new_variant();

    crm_malloc0(private, sizeof(cib_remote_opaque_t));

    cib->variant = cib_remote;
    cib->variant_opaque = private;

    if(server) {
	private->server = crm_strdup(server);
    }

    if(user) {
	private->user = crm_strdup(user);
    }

    if(passwd) {
	private->passwd = crm_strdup(passwd);
    }

    private->port = port;
    private->command.encrypted = encrypted;
    private->callback.encrypted = encrypted;

    /* assign variant specific ops*/
    cib->cmds->variant_op = cib_remote_perform_op;
    cib->cmds->signon     = cib_remote_signon;
    cib->cmds->signoff    = cib_remote_signoff;
    cib->cmds->free       = cib_remote_free;
    cib->cmds->inputfd    = cib_remote_inputfd;

    cib->cmds->register_notification = cib_remote_register_notification;
    cib->cmds->set_connection_dnotify = cib_remote_set_connection_dnotify;

    return cib;
} 

static int
cib_tls_close(cib_t *cib)
{
    cib_remote_opaque_t *private = cib->variant_opaque;
    shutdown (private->command.socket, SHUT_RDWR);     /* no more receptions */
    shutdown (private->callback.socket, SHUT_RDWR);     /* no more receptions */
    close(private->command.socket);
    close(private->callback.socket);

#ifdef HAVE_GNUTLS_GNUTLS_H
    if(private->command.encrypted) {
	gnutls_bye(*(private->command.session), GNUTLS_SHUT_RDWR);
	gnutls_deinit(*(private->command.session));
	gnutls_free(private->command.session);

	gnutls_bye(*(private->callback.session), GNUTLS_SHUT_RDWR);
	gnutls_deinit(*(private->callback.session));
	gnutls_free(private->callback.session);

	gnutls_anon_free_client_credentials (anon_cred_c);
	gnutls_global_deinit();
    }
#endif
    return 0;
}

static int
cib_tls_signon(cib_t *cib, struct remote_connection_s *connection)
{
    int sock;
    cib_remote_opaque_t *private = cib->variant_opaque;
    struct sockaddr_in addr;
    int rc = 0;
    char *server = private->server;

    int ret_ga;
    struct addrinfo *res;
    struct addrinfo hints;

    xmlNode *answer = NULL;
    xmlNode *login = NULL;

    connection->socket  = 0;
    connection->session = NULL;
    
    /* create socket */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1 ) {
	crm_perror(LOG_ERR,"Socket creation failed");
	return -1;
    }
    
    /* getaddrinfo */
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    if(hints.ai_family == AF_INET6) {
	hints.ai_protocol = IPPROTO_ICMPV6;
    } else {
	hints.ai_protocol = IPPROTO_ICMP;
    }

    crm_debug("Looking up %s", server);
    ret_ga = getaddrinfo(server, NULL, &hints, &res);
    if (ret_ga) {
	crm_err("getaddrinfo: %s", gai_strerror(ret_ga));
	return -1;
    }
	
    if (res->ai_canonname) {
	server = res->ai_canonname;
    }

    crm_debug("Got address %s for %s", server, private->server);
    
    if (!res->ai_addr) {
	fprintf(stderr, "getaddrinfo failed");
	exit(1);
    }

#if 1
    memcpy(&addr, res->ai_addr, res->ai_addrlen);
#else
    /* connect to server*/
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server);
#endif
    addr.sin_port = htons(private->port);
    
    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
	crm_perror(LOG_ERR,"Connection to %s:%d failed", server, private->port);
	close(sock);
	return -1;
    }

    if(connection->encrypted) {
    /* initialize GnuTls lib*/
#ifdef HAVE_GNUTLS_GNUTLS_H
	gnutls_global_init();
	gnutls_anon_allocate_client_credentials(&anon_cred_c);
    
	/* bind the socket to GnuTls lib */
	connection->session = create_tls_session(sock, GNUTLS_CLIENT);
	if (connection->session == NULL) {
	    crm_perror(LOG_ERR,"Session creation for %s:%d failed", server, private->port);
	    close(sock);
	    cib_tls_close(cib);
	    return -1;
	}
#else
	return cib_NOTSUPPORTED;
#endif
    } else {
	connection->session = GUINT_TO_POINTER(sock);
    }
    
    /* login to server */
    login = create_xml_node(NULL, "cib_command");
    crm_xml_add(login, "op", "authenticate");
    crm_xml_add(login, "user", private->user);
    crm_xml_add(login, "password", private->passwd);
    crm_xml_add(login, "hidden", "password");
    
    cib_send_remote_msg(connection->session, login, connection->encrypted);
    free_xml(login);

    answer = cib_recv_remote_msg(connection->session, connection->encrypted);
    crm_log_xml_debug_3(answer, "Reply");
    if(answer == NULL) {
	rc = cib_authentication;

    } else {
	/* grab the token */
	const char *msg_type = crm_element_value(answer, F_CIB_OPERATION);
	const char *tmp_ticket = crm_element_value(answer, F_CIB_CLIENTID);
	
	if(safe_str_neq(msg_type, CRM_OP_REGISTER) ) {
	    crm_err("Invalid registration message: %s", msg_type);
	    rc = cib_registration_msg;
	    
	} else if(tmp_ticket == NULL) {
	    rc = cib_callback_token;
	    
	} else {
	    connection->token = crm_strdup(tmp_ticket);
	}    
    }
    
    if (rc != 0) {
	cib_tls_close(cib);
    }
    
    connection->socket = sock;
    connection->source = G_main_add_fd(
	G_PRIORITY_HIGH, connection->socket, FALSE,
	cib_remote_dispatch, cib, cib_remote_connection_destroy);	

    return rc;
}

void
cib_remote_connection_destroy(gpointer user_data)
{
    crm_err("Connection destroyed");
#ifdef HAVE_GNUTLS_GNUTLS_H
    cib_tls_close(user_data);
#endif
    return;
}

gboolean cib_remote_dispatch(int fd, gpointer user_data)
{
    cib_t *cib = user_data;
    cib_remote_opaque_t *private = cib->variant_opaque;
    if(fd == private->callback.socket) {
	xmlNode *msg = NULL;
	const char *type = NULL;

	crm_info("Message on callback channel");
	msg = cib_recv_remote_msg(private->callback.session, private->callback.encrypted);

	type = crm_element_value(msg, F_TYPE);
	crm_debug_4("Activating %s callbacks...", type);

	if(safe_str_eq(type, T_CIB)) {
	    cib_native_callback(cib, msg, 0, 0);
		
	} else if(safe_str_eq(type, T_CIB_NOTIFY)) {
		g_list_foreach(cib->notify_list, cib_native_notify, msg);

	} else {
		crm_err("Unknown message type: %s", type);
	}
	
	if(msg != NULL) {
	    free_xml(msg);
	    return TRUE;
	}
	
    } else if(fd == private->command.socket) {
	crm_err("Message on command channel");

    } else {
	crm_err("Unknown fd");
    }
    
    return FALSE;
}

int
cib_remote_signon(cib_t* cib, const char *name, enum cib_conn_type type)
{
    int rc = cib_ok;
    cib_remote_opaque_t *private = cib->variant_opaque;

    if(private->passwd == NULL) {
	struct termios settings;
	int rc;
	
	rc = tcgetattr (0, &settings);
	settings.c_lflag &= ~ECHO;
	rc = tcsetattr (0, TCSANOW, &settings);

	fprintf(stderr, "Password: ");
	crm_malloc0(private->passwd, 1024);
	scanf("%s", private->passwd);
	fprintf(stdout, "\n");
	/* fprintf(stderr, "entered: '%s'\n", buffer); */

	settings.c_lflag |= ECHO;
	rc = tcsetattr (0, TCSANOW, &settings);
    }
    
    if(private->server == NULL || private->user == NULL) {
	rc = cib_missing;
    }
    
    if(rc == cib_ok) {
	rc = cib_tls_signon(cib, &(private->command));
    }
    
    if(rc == cib_ok) {
	rc = cib_tls_signon(cib, &(private->callback));
    }
    
    if(rc == cib_ok) {
	xmlNode *hello = cib_create_op(0, private->callback.token, CRM_OP_REGISTER, NULL, NULL, NULL, 0);
	crm_xml_add(hello, F_CIB_CLIENTNAME, name);
	cib_send_remote_msg(private->command.session, hello, private->command.encrypted);
	free_xml(hello);
    }    

    if(rc == cib_ok) {
	fprintf(stderr, "%s: Opened connection to %s:%d\n", name, private->server, private->port);
	cib->state = cib_connected_command;
	cib->type  = cib_command;

    } else {
	fprintf(stderr, "%s: Connection to %s:%d failed: %s\n",
		name, private->server, private->port, cib_error2string(rc));
    }
    
    return rc;
}
	
int
cib_remote_signoff(cib_t* cib)
{
    int rc = cib_ok;
    /* cib_remote_opaque_t *private = cib->variant_opaque; */

    crm_debug("Signing out of the CIB Service");
#ifdef HAVE_GNUTLS_GNUTLS_H
    cib_tls_close(cib);
#endif
    
    cib->state = cib_disconnected;
    cib->type  = cib_none;
    
    return rc;
}

int
cib_remote_free (cib_t* cib)
{
    int rc = cib_ok;

    crm_warn("Freeing CIB");
    if(cib->state != cib_disconnected) {
	rc = cib_remote_signoff(cib);
	if(rc == cib_ok) {
	    cib_remote_opaque_t *private = cib->variant_opaque;
	    crm_free(private->server);
	    crm_free(private->user);
	    crm_free(private->passwd);
	    crm_free(cib->cmds);
	    crm_free(private);
	    crm_free(cib);
	}
    }
	
    return rc;
}

static gboolean timer_expired = FALSE;
static struct timer_rec_s *sync_timer = NULL;
static gboolean cib_timeout_handler(gpointer data)
{
    struct timer_rec_s *timer = data;
    timer_expired = TRUE;
    crm_err("Call %d timed out after %ds", timer->call_id, timer->timeout);

    /* Always return TRUE, never remove the handler
     * We do that after the while-loop in cib_native_perform_op()
     */
    return TRUE;
}

int
cib_remote_perform_op(
    cib_t *cib, const char *op, const char *host, const char *section,
    xmlNode *data, xmlNode **output_data, int call_options) 
{
	int  rc = HA_OK;
	
	xmlNode *op_msg   = NULL;
	xmlNode *op_reply = NULL;

	cib_remote_opaque_t *private = cib->variant_opaque;
	if(sync_timer == NULL) {
	    crm_malloc0(sync_timer, sizeof(struct timer_rec_s));
	}
	
	if(cib->state == cib_disconnected) {
		return cib_not_connected;
	}

	if(output_data != NULL) {
		*output_data = NULL;
	}
	
	if(op == NULL) {
		crm_err("No operation specified");
		return cib_operation;
	}

	cib->call_id++;
	/* prevent call_id from being negative (or zero) and conflicting
	 *    with the cib_errors enum
	 * use 2 because we use it as (cib->call_id - 1) below
	 */
	if(cib->call_id < 1) {
	    cib->call_id = 1;
	}
	
	op_msg = cib_create_op(
	    cib->call_id, private->callback.token, op, host, section, data, call_options);
	if(op_msg == NULL) {
		return cib_create_msg;
	}
	
	crm_debug_3("Sending %s message to CIB service", op);
	cib_send_remote_msg(private->command.session, op_msg, private->command.encrypted);
	free_xml(op_msg);

	if((call_options & cib_discard_reply)) {
		crm_debug_3("Discarding reply");
		return cib_ok;

	} else if(!(call_options & cib_sync_call)) {
		return cib->call_id;
	}
	
	rc = IPC_OK;
	crm_debug_3("Waiting for a syncronous reply");

	if(cib->call_timeout > 0) {
	    /* We need this, even with msgfromIPC_timeout(), because we might
	     * get other/older replies that don't match the active request
	     */
	    timer_expired = FALSE;
	    sync_timer->call_id = cib->call_id;
	    sync_timer->timeout = cib->call_timeout*1000;
	    sync_timer->ref = g_timeout_add(
		sync_timer->timeout, cib_timeout_handler, sync_timer);
	}

	while(timer_expired == FALSE) {
		int reply_id = -1;
		int msg_id = cib->call_id;

		op_reply = cib_recv_remote_msg(private->command.session, private->command.encrypted);
		if(op_reply == NULL) {
			break;
		}

		crm_element_value_int(op_reply, F_CIB_CALLID, &reply_id);
		CRM_CHECK(reply_id > 0,
			  free_xml(op_reply);
			  if(sync_timer->ref > 0) {
			      g_source_remove(sync_timer->ref);
			      sync_timer->ref = 0;
			  }
			  return cib_reply_failed);

		if(reply_id == msg_id) {
			break;
			
		} else if(reply_id < msg_id) {
			crm_debug("Received old reply: %d (wanted %d)",
				  reply_id, msg_id);
			crm_log_xml(
				LOG_MSG, "Old reply", op_reply);

		} else if((reply_id - 10000) > msg_id) {
			/* wrap-around case */
			crm_debug("Received old reply: %d (wanted %d)",
				  reply_id, msg_id);
			crm_log_xml(
				LOG_MSG, "Old reply", op_reply);
		} else {
			crm_err("Received a __future__ reply:"
				" %d (wanted %d)", reply_id, msg_id);
		}

		free_xml(op_reply);
		op_reply = NULL;
	}

	if(sync_timer->ref > 0) {
	    g_source_remove(sync_timer->ref);
	    sync_timer->ref = 0;
	}
	
	if(timer_expired) {
	    return cib_remote_timeout;
	}

	/* if(IPC_ISRCONN(native->command_channel) == FALSE) { */
	/* 	crm_err("CIB disconnected: %d",  */
	/* 		native->command_channel->ch_status); */
	/* 	cib->state = cib_disconnected; */
	/* } */
	
	if(op_reply == NULL) {
		crm_err("No reply message - empty - %d", rc);
		return cib_reply_failed;
	}
	
	crm_debug_3("Syncronous reply received");
	rc = cib_ok;
	
	/* Start processing the reply... */
	if(crm_element_value_int(op_reply, F_CIB_RC, &rc) != 0) {
		rc = cib_return_code;
	}	

	if(rc == cib_diff_resync) {
	    /* This is an internal value that clients do not and should not care about */
	    rc = cib_ok;
	}
	
	if(rc == cib_ok || rc == cib_not_master || rc == cib_master_timeout) {
	    crm_log_xml(LOG_DEBUG, "passed", op_reply);

	} else {
/* 	} else if(rc == cib_remote_timeout) { */
		crm_err("Call failed: %s", cib_error2string(rc));
		crm_log_xml(LOG_WARNING, "failed", op_reply);
	}
	
	if(output_data == NULL) {
		/* do nothing more */
		
	} else if(!(call_options & cib_discard_reply)) {
		xmlNode *tmp = get_message_xml(op_reply, F_CIB_CALLDATA);
		if(tmp == NULL) {
			crm_debug_3("No output in reply to \"%s\" command %d",
				  op, cib->call_id - 1);
		} else {
		    *output_data = copy_xml(tmp);
		}
	}
	
	free_xml(op_reply);

	return rc;
}
