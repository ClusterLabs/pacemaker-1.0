/* $Id: ipc_wrappers.c,v 1.3 2004/11/22 19:03:00 gshi Exp $ */
/*
 * Some helpers for wrapping the ocf_ipc functionality for Perl.
 *
 * Copyright (c) 2004 Lars Marowsky-Brée <lmb@suse.de>
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
#include <clplumbing/ipc.h>
#include <glib.h>
#include <ipc_wrappers.h>
#include <string.h>

GHashTable *simple_hash_new(void) {
	GHashTable * attrs = g_hash_table_new(g_str_hash,g_str_equal);
	return attrs;
}

void simple_hash_insert(GHashTable *foo, char *name, char *value) {
	g_hash_table_insert(foo, name, value);
}

void simple_hash_destroy(GHashTable *foo) {
	g_hash_table_destroy(foo);
}

IPC_Auth *helper_create_auth(void) {
	IPC_Auth *auth = g_new(struct IPC_AUTH, 1);

	auth->gid = NULL;
	auth->uid = NULL;

	return auth;
}

void helper_add_auth_uid(IPC_Auth *auth, int a_uid) {
	static int v = 1;

	if (auth->uid == NULL) {
		auth->uid = g_hash_table_new(g_direct_hash, g_direct_equal);
	}
	g_hash_table_insert(auth->uid, GINT_TO_POINTER((gint)a_uid), &v);
}

void helper_add_auth_gid(IPC_Auth *auth, int a_gid) {
	static int v = 1;

	if (auth->gid == NULL) {
		auth->gid = g_hash_table_new(g_direct_hash, g_direct_equal);
	}
	g_hash_table_insert(auth->gid, GINT_TO_POINTER((gint)a_gid), &v);
}

void ipc_ch_destroy(IPC_Channel *ch) {
	ch->ops->destroy(ch);
}

int ipc_ch_initiate_connection(IPC_Channel *ch) {
	return ch->ops->initiate_connection(ch);
}

int ipc_ch_verify_auth(IPC_Channel *ch, IPC_Auth *auth) {
	return ch->ops->verify_auth(ch,auth);
}

int ipc_ch_send(IPC_Channel *ch, IPC_Message *msg) {
	return ch->ops->send(ch,msg);
}

IPC_Message_with_rc *ipc_ch_recv(IPC_Channel *ch) {
	IPC_Message_with_rc *rc = g_malloc(sizeof(IPC_Message_with_rc));
	rc->msg = NULL;
	rc->rc = ch->ops->recv(ch,&rc->msg);
	return rc;
}

void ipc_ch_msg_with_rc_destroy(IPC_Message_with_rc *msg) {
	g_free(msg);
}

int ipc_ch_waitin(IPC_Channel *ch) {
	return ch->ops->waitin(ch);
}

int ipc_ch_waitout(IPC_Channel *ch) {
	return ch->ops->waitout(ch);
}

int ipc_ch_is_message_pending(IPC_Channel *ch) {
	return ch->ops->is_message_pending(ch);
}

int ipc_ch_is_sending_blocked(IPC_Channel *ch) {
	return ch->ops->is_sending_blocked(ch);
}

int ipc_ch_resume_io(IPC_Channel *ch) {
	return ch->ops->resume_io(ch);
}

int ipc_ch_get_send_select_fd(IPC_Channel *ch) {
	return ch->ops->get_send_select_fd(ch);
}

int ipc_ch_get_recv_select_fd(IPC_Channel *ch) {
	return ch->ops->get_recv_select_fd(ch);
}

int ipc_ch_set_send_qlen(IPC_Channel *ch, int q_len) {
	return ch->ops->set_send_qlen(ch,q_len);
}

int ipc_ch_set_recv_qlen(IPC_Channel *ch, int q_len) {
	return ch->ops->set_recv_qlen(ch,q_len);
}

int ipc_ch_isrconn(IPC_Channel *ch) {
	return IPC_ISRCONN(ch);
}

int ipc_ch_iswconn(IPC_Channel *ch) {
	return IPC_ISWCONN(ch);
}

void ipc_wc_destroy(IPC_WaitConnection *wc) {
	wc->ops->destroy(wc);
}

int ipc_wc_get_select_fd(IPC_WaitConnection *wc) {
	return wc->ops->get_select_fd(wc);
}

IPC_Channel *ipc_wc_accept_connection(IPC_WaitConnection *wc, IPC_Auth *auth) {
	return wc->ops->accept_connection(wc,auth);
}

void ipc_msg_done(IPC_Message *msg) {
	msg->msg_done(msg);
}

char *ipc_msg_get_body(IPC_Message *msg) {
	return msg->msg_body;
}

void ipc_msg_free(IPC_Message *msg) {
	g_free(msg->msg_body);
	g_free(msg);
}

IPC_Message *ipc_msg_constructor(IPC_Channel *ch, size_t s, char *data) {
	IPC_Message *msg = (IPC_Message *)g_malloc(sizeof(IPC_Message));
	
	memset(msg, 0, sizeof(IPC_Message));
	
	msg->msg_private = NULL;
	msg->msg_done = ipc_msg_free;
	msg->msg_ch = ch;
	msg->msg_len = s;
	
	msg->msg_body = g_malloc(s+1);
	memcpy(msg->msg_body, data, s);

	return msg;
}

