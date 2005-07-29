/* $Id: ccmlib_eventapi.c,v 1.10 2005/07/29 23:02:03 alan Exp $ */
/* 
 * ccmlib_eventapi.c: OCF event API.
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/* 
 * implements 0.2 version of proposed event api 
 */
#define __CCM_LIBRARY__
#include <ccmlib.h>

void *cookie_construct(void (*f)(void *), void (*free_f)(void *), void *);
void * cookie_get_data(void *ck);
void * cookie_get_func(void *ck);
void cookie_ref(void *ck);
void cookie_unref(void *ck);

static GHashTable  *tokenhash = NULL;

typedef struct oc_ev_cookie_s {
	void		 	(*func) (void *);
	void		 	(*freefunc) (void *);
	void			*data;
	int			refcount;
} oc_ev_cookie_t;

static guint token_counter=0;

typedef struct oc_ev_s {
	int		oc_flag;
	GHashTable 	*oc_eventclass;
} __oc_ev_t;

#define EVENT_INIT 1



/* 
 * BEGIN OF FUNCTIONS DEALING WITH COOKIES
 */

void *
cookie_construct(void (*f)(void *), void (*free_f)(void *), void *data)
{
	oc_ev_cookie_t *cookie = g_malloc(sizeof(oc_ev_cookie_t));
	cookie->func = f;
	cookie->data = data;
	cookie->freefunc = free_f;
	cookie->refcount = 1;
	return (void *)cookie;
}

void *
cookie_get_data(void *ck)
{
	oc_ev_cookie_t *cookie = (oc_ev_cookie_t *)ck;
	if (!cookie) {
		return NULL;
	}

	return cookie->data;
}
void *
cookie_get_func(void *ck)
{
	oc_ev_cookie_t *cookie = (oc_ev_cookie_t *)ck;
	if(!cookie) {
		return NULL;
	}

	return cookie->func;
}

void
cookie_unref(void *ck)
{
	oc_ev_cookie_t *cookie = (oc_ev_cookie_t *)ck;
	if(!cookie) {
		return;
	}
	if(--cookie->refcount == 0) {
		 if(cookie->freefunc){
			 cookie->freefunc(cookie->data);
		 }
		g_free(cookie);
	}
	return;
}

void
cookie_ref(void *ck)
{
	oc_ev_cookie_t *cookie = (oc_ev_cookie_t *)ck;
	if(!cookie) {
		return;
	}
	++cookie->refcount;
	return;
}

/* 
 * END OF FUNCTIONS DEALING WITH COOKIES
 */




static class_t *
class_construct(oc_ev_class_t class_type, oc_ev_callback_t  *fn)
{
	class_t *t_class = NULL;

	switch(class_type) {
	case OC_EV_MEMB_CLASS:  
		t_class = oc_ev_memb_class(fn);
		break;

	case OC_EV_CONN_CLASS: 
	case OC_EV_GROUP_CLASS:  
	default :
		break;
	}

	return t_class;
}


static void
oc_ev_init(void)
{
	static gboolean ocinit_flag = FALSE;

	if(ocinit_flag==FALSE) {
		tokenhash =  g_hash_table_new(g_direct_hash, 
				g_direct_equal);
		ocinit_flag = TRUE;
	}
	return;
}

static gboolean 
eventclass_remove_func(gpointer key, 
		gpointer value, 
		gpointer user_data)
{
	class_t  *class = (class_t *)value;
	class->unregister(class);
	g_free(class);

	return TRUE;
}

static gboolean
token_invalid(const __oc_ev_t *token)
{
	if(!token){
		return TRUE;
	}

	if(token->oc_flag!= EVENT_INIT) {
		return TRUE;
	}

	return FALSE;
}

static void 
activate_func(gpointer key, 
		gpointer value, 
		gpointer user_data)
{
	class_t  *class = (class_t *)value;
	oc_ev_class_t class_type = (oc_ev_class_t) GPOINTER_TO_SIZE(key);
	int	*fd = (int *) user_data;
	int	tmp;

	tmp = class->activate(class);
	/* NOTE: the event API 0.2 is broken. 
	 * since membership class is the only event
	 * class that is supported with this API, we
	 * just return its file descriptor
	 */ 
	if(class_type == OC_EV_MEMB_CLASS) {
		*fd = tmp;
	}
	return;
}

static gboolean 
handle_func(gpointer key, 
		gpointer value, 
		gpointer user_data)
{
	class_t  *class = (class_t *)value;

	/* if handle event fails, remove this class */
	if(!class->handle_event((void *)class))
		return TRUE;

	/*do not remove this class*/
	return FALSE;
}


int
oc_ev_register(oc_ev_t **token)
{
	__oc_ev_t *rettoken;

	oc_ev_init();


	rettoken = (__oc_ev_t *)g_malloc(sizeof(__oc_ev_t));
	*token = (oc_ev_t *)GUINT_TO_POINTER(token_counter++);

	if(!rettoken) {
		return ENOMEM;
	}

	rettoken->oc_flag = EVENT_INIT;
	rettoken->oc_eventclass = g_hash_table_new(g_direct_hash, 
					g_direct_equal);

	g_hash_table_insert(tokenhash, *token, rettoken);

	return 0;
}



int 
oc_ev_unregister(oc_ev_t *tok)
{
	__oc_ev_t *token = (__oc_ev_t *)g_hash_table_lookup(tokenhash, 
			tok);
	
	if(token == NULL || token_invalid(token)){
		return EINVAL;
	}
	
	
	/*
	 * delete all the event classes associated within
	 * this handle
	 */
	g_hash_table_foreach_remove(token->oc_eventclass, 
			eventclass_remove_func, NULL);

	g_hash_table_remove(tokenhash, tok);
	g_free(token);
	return 0;
}


/* a to configure any special parameters for 
 * any of the classes. This function is not 
 * part of the 0.2 event API. Is been added
 * to support setup of any special behaviour.
 */
void
oc_ev_special(const oc_ev_t *tok, 
		oc_ev_class_t class_type, 
		int type)
{
	class_t *class;
	const __oc_ev_t *token =  (__oc_ev_t *)
		g_hash_table_lookup(tokenhash, tok);
	
	if(token == NULL || token_invalid(token)){
		return;
	}
	
	/* if structure for the class already exists 
 	 *  just update the callback. Else allocate
	 *  a structure and update the callback
	 */
	if((class = g_hash_table_lookup(token->oc_eventclass, 
				(void *)class_type)) == NULL){
		return;
	}
	class->special(class, type);
	return;
}

int 
oc_ev_set_callback(const oc_ev_t *tok,
		oc_ev_class_t class_type,
		oc_ev_callback_t *fn,
		oc_ev_callback_t **prev_fn)
{
	class_t *class;
	oc_ev_callback_t *pre_callback;

	const __oc_ev_t *token =  (__oc_ev_t *)
		g_hash_table_lookup(tokenhash, tok);

	if(token == NULL || token_invalid(token)){
		return EINVAL;
	}
	

	/* if structure for the class already exists 
 	 *  just update the callback. Else allocate
	 *  a structure and update the callback
	 */
	if((class = g_hash_table_lookup(token->oc_eventclass, 
				(void *)class_type)) == NULL){
		class = class_construct(class_type, NULL);
		g_hash_table_insert(token->oc_eventclass, 
				(void *)GINT_TO_POINTER(class_type),
				class);
	}
	
	assert(class && class->set_callback);

	pre_callback  =  class->set_callback(class, fn);
	if(prev_fn)
		*prev_fn = pre_callback;

	return 0;
}


int 
oc_ev_activate(const oc_ev_t *tok, int *fd)
{
	const __oc_ev_t *token =  (__oc_ev_t *)
		g_hash_table_lookup(tokenhash, tok);

	*fd = -1;

	if(token == NULL || token_invalid(token)){
		return EINVAL;
	}
	
	if(!g_hash_table_size(token->oc_eventclass)){
		return EMFILE;
	}

	g_hash_table_foreach(token->oc_eventclass, 
				activate_func, fd);
	if(*fd == -1){
		return 1;
	}

	return 0;
}


int
oc_ev_handle_event(const oc_ev_t *tok)
{
	const __oc_ev_t *token =  (__oc_ev_t *)
		g_hash_table_lookup(tokenhash, tok);

	if(token == NULL || token_invalid(token)){
		return EINVAL;
	}
	

	if(!g_hash_table_size(token->oc_eventclass)){
		return -1;
	}
	
	if(g_hash_table_size(token->oc_eventclass)) {
		g_hash_table_foreach_remove(token->oc_eventclass, 
					    handle_func, 
					    NULL);
	}
	return 0;
}



int 
oc_ev_callback_done(void *ck)
{
	void (*f)(void *);
	oc_ev_cookie_t *cookie = (oc_ev_cookie_t *)ck;
	f = cookie_get_func(cookie);
	f(ck);
	return 0;
}


int 
oc_ev_is_my_nodeid(const oc_ev_t *tok, const oc_node_t *node)
{
	class_t  *class;
	const __oc_ev_t *token =  (__oc_ev_t *)
		g_hash_table_lookup(tokenhash, tok);

	if(token == NULL || token_invalid(token) || !node){
		return EINVAL;
	}

	class = g_hash_table_lookup(token->oc_eventclass, 
			(void *)OC_EV_MEMB_CLASS);

	return class->is_my_nodeid(class, node);
}
