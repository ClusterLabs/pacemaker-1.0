/* 
 * libclm.c: SAForum AIS Membership Service library
 *
 * Copyright (c) 2003 Intel Corp.
 * Author: Zhu Yi (yi.zhu@intel.com)
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
#include <portability.h>
#include <strings.h>
#include <stdio.h>

#include <unistd.h>
#include <glib.h>
#include <errno.h>
#include <assert.h>
#include <ocf/oc_event.h>
#include <clplumbing/cl_log.h>
#include <saf/ais.h>
#ifdef POSIX_THREADS
#  include <pthread.h>
#endif
#include <sys/time.h>
#include <string.h>

#define CLM_TRACK_STOP 0
#define CLM_DEBUG 0

#define GET_CLM_HANDLE(x) (__clm_handle_t *)g_hash_table_lookup(__handle_hash,x)

typedef struct __clm_handle_s {
	oc_ev_t *ev_token;
	SaClmCallbacksT callbacks;
	SaSelectionObjectT fd;
	SaUint8T trackflags;
	SaUint32T itemnum;
	SaClmClusterNotificationT *nbuf;
	SaSelectionObjectT st;
} __clm_handle_t;

static GHashTable *__handle_hash = NULL;
static guint __handle_counter = 0;
static const oc_ev_membership_t *__ccm_data = NULL;
static oc_ev_t __ccm_event = OC_EV_MS_INVALID;
static void *__ccm_cookie = NULL;
#ifdef POSIX_THREADS
	static pthread_mutex_t __clmlib_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void pthread_lock(void);
static void pthread_unlock(void);
static void clm_init(void);
extern void oc_ev_special(const oc_ev_t *, oc_ev_class_t , int );
static void retrieve_current_buffer(__clm_handle_t *hd);
static void retrieve_changes_buffer(__clm_handle_t *hd);
static void retrieve_changes_only_buffer(__clm_handle_t *hd);
static SaErrorT retrieve_node_buffer(SaClmNodeIdT nodeId
,		SaClmClusterNodeT *clusterNode);

static void pthread_lock()
{
#ifdef POSIX_THREADS
	pthread_mutex_lock(&__clmlib_mutex);
#endif
}

static void pthread_unlock()
{
#ifdef POSIX_THREADS
	pthread_mutex_unlock(&__clmlib_mutex);
#endif
}

static void
clm_init()
{
	static gboolean clminit_flag = FALSE;

	if (clminit_flag == FALSE) {
		__handle_hash = g_hash_table_new(g_int_hash
		,	g_int_equal);
		clminit_flag = TRUE;
	}

	return;
}

static void
ccm_events(oc_ed_t event, void *cookie, size_t size, const void *data)
{
	pthread_lock();

	/* dereference old cache */
	if (__ccm_cookie)
		oc_ev_callback_done(__ccm_cookie);

	__ccm_cookie = cookie;
	__ccm_event = event;
	__ccm_data = (const oc_ev_membership_t *)data;

#if CLM_DEBUG
	cl_log(LOG_DEBUG, "__ccm_data = <0x%x>"
	,	(unsigned int)data);
#endif
	pthread_unlock();

	if (event == OC_EV_MS_EVICTED || event == OC_EV_MS_NOT_PRIMARY
	||	event == OC_EV_MS_PRIMARY_RESTORED) {
		/* We do not care about this info */
		return;
	}

	if (!data) {
		cl_log(LOG_ERR, "CCM event callback return NULL data");
		return;
	}

	/*
	 * Note: No need to worry about the buffer free problem, OCF
	 * callback mechanism did this for us.
	 */
}

SaErrorT 
saClmInitialize(SaClmHandleT *clmHandle, const SaClmCallbacksT *clmCallbacks,
                const SaVersionT *version)
{
	int ret;
	oc_ev_t *ev_token;
	__clm_handle_t *hd;
	SaClmHandleT *hash_key;
	fd_set rset;
	struct timeval tv;
        SaErrorT rc;

	oc_ev_register(&ev_token);
	if ((ret = oc_ev_set_callback(ev_token, OC_EV_MEMB_CLASS
	,	ccm_events, NULL)) != 0) {
		if (ret == ENOMEM){
			rc = SA_ERR_NO_MEMORY;
                        goto err_nomem_exit;
		}
		else{
			assert(0);	/* Never runs here */
		}
	}
	/* We must call it to get non-quorum partition info */
	oc_ev_special(ev_token, OC_EV_MEMB_CLASS, 0);

	clm_init();


	hash_key = (SaClmHandleT *)g_malloc(sizeof(SaClmHandleT));
	if (!hash_key){
		rc = SA_ERR_NO_MEMORY;
                goto err_nomem_exit;
	}

	hd = (__clm_handle_t *)g_malloc(sizeof(__clm_handle_t));
	if (!hd){
                g_free(hash_key);
		rc = SA_ERR_NO_MEMORY;
                goto err_nomem_exit;
	}

	*clmHandle = __handle_counter++;
	*hash_key = *clmHandle;
	hd->ev_token = ev_token;
	hd->callbacks = *clmCallbacks;
	hd->trackflags = CLM_TRACK_STOP;
	cl_log(LOG_INFO, "g_hash_table_insert hd = [%p]", hd);
	g_hash_table_insert(__handle_hash, hash_key, hd);

	if ((ret = oc_ev_activate(hd->ev_token, &hd->fd)) != 0) {
		cl_log(LOG_ERR, "oc_ev_activate error [%d]", ret);
		rc = SA_ERR_LIBRARY;
                goto err_lib_exit;
	}

	/* Prepare information for saClmClusterNodeGet() series calls */
	while (!__ccm_data) {

		FD_ZERO(&rset);
		FD_SET(hd->fd, &rset);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		if ((ret = select(hd->fd + 1, &rset, NULL, NULL, &tv)) == -1) {
			cl_log(LOG_ERR, "%s: select error [%d]"
			,	__FUNCTION__, ret);
			rc = SA_ERR_LIBRARY;
                        goto err_lib_exit;

		} else if (ret == 0) {
			cl_log(LOG_WARNING, "%s: select timeout", __FUNCTION__);
			rc = SA_ERR_TIMEOUT;
                        goto err_lib_exit;
		}

		if ((ret = oc_ev_handle_event(hd->ev_token) != 0)) {
			cl_log(LOG_ERR, "%s: oc_ev_handle_event error [%d]"
			,	__FUNCTION__, ret);
			rc = SA_ERR_LIBRARY;
                        goto err_lib_exit;
		}

	}
	return SA_OK;

 err_nomem_exit:
        g_hash_table_remove(__handle_hash, hash_key);
        g_free(hd);
        g_free(hash_key);

 err_lib_exit:
        oc_ev_unregister(ev_token);
        return rc;
}

SaErrorT 
saClmSelectionObjectGet(const SaClmHandleT *clmHandle, 
                        SaSelectionObjectT *selectionObject)
{
	__clm_handle_t *hd = GET_CLM_HANDLE(clmHandle);

	if (!hd){
		return SA_ERR_BAD_HANDLE;
	}

	*selectionObject = hd->fd;

	return SA_OK;
}


#define MEMCHANGE(x)	hd->nbuf[x].clusterChanges
#define MEMNODE(x)	hd->nbuf[x].clusterNode

static void
set_misc_node_info(SaClmClusterNodeT *cn)
{
	cn->nodeAddress.length = 0;
	cn->nodeAddress.value[0] = '\0';
	cn->nodeName.length = strlen((char*)cn->nodeName.value);
	cn->clusterName.length = 0;
	cn->clusterName.value[0] = '\0';
	cn->bootTimestamp = 0;
}

static void
retrieve_current_buffer(__clm_handle_t *hd)
{
	uint i;
	char *p;
	const oc_ev_membership_t *oc = __ccm_data;

	for (i = 0; i < oc->m_n_member; i++) {
		MEMCHANGE(i) = SA_CLM_NODE_NO_CHANGE;
		MEMNODE(i).nodeId = oc->m_array[oc->m_memb_idx+i].node_id;
		MEMNODE(i).member = 1;
		p = oc->m_array[oc->m_memb_idx+i].node_uname;
		if (p) {
			strncpy((char *)MEMNODE(i).nodeName.value, p, 
					SA_MAX_NAME_LENGTH - 1);
			MEMNODE(i).nodeName.value[SA_MAX_NAME_LENGTH-1] = '\0';
		} else {
			MEMNODE(i).nodeName.value[0] = '\0';
		}
		set_misc_node_info(&MEMNODE(i));
	}
}

static void
retrieve_changes_buffer(__clm_handle_t *hd)
{
	uint i, j;
	int n;
	char *p;
	const oc_ev_membership_t *oc = __ccm_data;

	retrieve_current_buffer(hd);

	for (i = 0; i < oc->m_n_in; i++) {
		for (j = 0; j < oc->m_n_member; j++) {
			if (MEMNODE(j).nodeId
			==	oc->m_array[oc->m_in_idx+i].node_id) {
				MEMCHANGE(j) = SA_CLM_NODE_JOINED;
				p = oc->m_array[oc->m_in_idx+i].node_uname;
				if (p) {
					strncpy((char*)MEMNODE(j).nodeName.value, p, 
							SA_MAX_NAME_LENGTH-1);
					MEMNODE(j).nodeName.value \
						[SA_MAX_NAME_LENGTH-1] = '\0';
				} else {
					MEMNODE(j).nodeName.value[0] = '\0';
				}
				break;
			}
		}
		assert(j < oc->m_n_member); /* must find new in all */
	}
	for (j = 0, n = oc->m_n_member; j < oc->m_n_out; j++, n++) {
		MEMCHANGE(n) = SA_CLM_NODE_LEFT;
		MEMNODE(n).nodeId = oc->m_array[oc->m_out_idx+j].node_id;
		MEMNODE(n).member = 0;
		p = oc->m_array[oc->m_out_idx+j].node_uname;
		if (p) {
			strncpy((char*)MEMNODE(n).nodeName.value, p,
					SA_MAX_NAME_LENGTH - 1);
			MEMNODE(n).nodeName.value[SA_MAX_NAME_LENGTH-1] = '\0';
		} else {
			MEMNODE(n).nodeName.value[0] = '\0';
		}
		set_misc_node_info(&MEMNODE(n));
	}
}

static void
retrieve_changes_only_buffer(__clm_handle_t *hd)
{
	uint i;
	int n;
	char *p;
	const oc_ev_membership_t *oc = __ccm_data;

	for (i = 0, n = 0; i < oc->m_n_in; i++, n++) {
		MEMCHANGE(n) = SA_CLM_NODE_JOINED;
		MEMNODE(n).nodeId = oc->m_array[oc->m_in_idx+i].node_id;
		MEMNODE(n).member = 1;
		p = oc->m_array[oc->m_in_idx+i].node_uname;
		if (p) {
			strncpy((char*)MEMNODE(n).nodeName.value, p,
					SA_MAX_NAME_LENGTH - 1);
			MEMNODE(n).nodeName.value[SA_MAX_NAME_LENGTH-1] = '\0';
		} else {
			MEMNODE(n).nodeName.value[0] = '\0';
		}
		set_misc_node_info(&MEMNODE(n));
	}
	for (i = 0; i < oc->m_n_out; i++, n++) {
		MEMCHANGE(n) = SA_CLM_NODE_LEFT;
		MEMNODE(n).nodeId = oc->m_array[oc->m_out_idx+i].node_id;
		MEMNODE(n).member = 0;
		p = oc->m_array[oc->m_out_idx+i].node_uname;
		if (p) {
			strncpy((char *)MEMNODE(n).nodeName.value, p,
					SA_MAX_NAME_LENGTH - 1);
			MEMNODE(n).nodeName.value[SA_MAX_NAME_LENGTH-1] = '\0';
		} else {
			MEMNODE(n).nodeName.value[0] = '\0';
		}
		set_misc_node_info(&MEMNODE(n));
	}
}

SaErrorT
saClmDispatch(const SaClmHandleT *clmHandle, 
              SaDispatchFlagsT dispatchFlags)
{
	int ret;
	const oc_ev_membership_t *oc;
	uint itemnum;
	__clm_handle_t *hd = GET_CLM_HANDLE(clmHandle);

	if (!hd){
		return SA_ERR_BAD_HANDLE;
	}

	if ((ret = oc_ev_handle_event(hd->ev_token)) != 0) {
		if (ret == EINVAL){
			return SA_ERR_BAD_HANDLE;
		}

		/* else we must be evicted */
	}

	/* We did not lock for read here because other writers will set it
	 * with the same value (if there really exist some). Otherwise we
	 * need to lock here.
	 */
	if (__ccm_event == OC_EV_MS_EVICTED) {
		cl_log(LOG_WARNING
		,	"This node is evicted from the current partition!");
		return SA_ERR_LIBRARY;
	}
	if (__ccm_event == OC_EV_MS_NOT_PRIMARY
	||	__ccm_event == OC_EV_MS_PRIMARY_RESTORED) {
		cl_log(LOG_DEBUG, "Received not interested event [%d]"
		,	__ccm_event);
		return SA_OK;
	}
	if (!__ccm_data){
		return SA_ERR_INIT;
	}

	oc = __ccm_data;

	if(CLM_TRACK_STOP == hd->trackflags){
		return SA_OK;
	}

	/* SA_TRACK_CURRENT is cleared in saClmClusterTrackStart, hence we 
	 * needn't to deal with it now*/
	if (hd->trackflags & SA_TRACK_CHANGES) {
		itemnum = oc->m_n_member + oc->m_n_out;
		if (itemnum > hd->itemnum) {
			hd->callbacks.saClmClusterTrackCallback(hd->nbuf
			,	hd->itemnum, oc->m_n_member, oc->m_instance
			,	SA_ERR_NO_SPACE);
			return SA_OK;
		}
		pthread_lock();
		retrieve_changes_buffer(hd);
		pthread_unlock();
		hd->callbacks.saClmClusterTrackCallback(hd->nbuf, itemnum
		,	oc->m_n_member, oc->m_instance, SA_OK);
	} else if (hd->trackflags & SA_TRACK_CHANGES_ONLY) {
		itemnum = oc->m_n_in + oc->m_n_out;
		if (itemnum > hd->itemnum) {
			hd->callbacks.saClmClusterTrackCallback(hd->nbuf
			,	hd->itemnum, oc->m_n_member, oc->m_instance
			,	SA_ERR_NO_SPACE);
			return SA_OK;
		}
		pthread_lock();
		retrieve_changes_only_buffer(hd);
		pthread_unlock();
		hd->callbacks.saClmClusterTrackCallback(hd->nbuf, itemnum
		,	oc->m_n_member, oc->m_instance, SA_OK);

	} else {
		assert(0);
	}
	/* unlock */

	return SA_OK;
}

SaErrorT 
saClmFinalize(SaClmHandleT *clmHandle)
{
	gpointer hd, oldkey;

	if (g_hash_table_lookup_extended(__handle_hash, clmHandle
	,	&oldkey, &hd) == FALSE) {
		return SA_ERR_BAD_HANDLE;
	}
 
	oc_ev_unregister(((__clm_handle_t *)hd)->ev_token);
	/* TODO: unregister saClmClusterNodeGetCall here */

	g_free(hd);
	g_free(oldkey);

	return SA_OK;
}

SaErrorT 
saClmClusterTrackStart(const SaClmHandleT *clmHandle,
                       SaUint8T trackFlags,
                       SaClmClusterNotificationT *notificationBuffer,
                       SaUint32T numberOfItems)
{
	__clm_handle_t *hd = GET_CLM_HANDLE(clmHandle);

	if (!hd){
		return SA_ERR_BAD_HANDLE;
	}

	hd->trackflags = trackFlags;
	hd->itemnum = numberOfItems;
	hd->nbuf = notificationBuffer;

	if (trackFlags & SA_TRACK_CURRENT) {
		const oc_ev_membership_t *oc;
		SaUint32T itemnum;
		
		/* Clear SA_TRACK_CURRENT, it's no use since now. */
		hd->trackflags &= ~SA_TRACK_CURRENT;

		if (__ccm_data == NULL) {
			return SA_ERR_LIBRARY;
		}
		
		oc = __ccm_data;
		itemnum = oc->m_n_member;
		if (itemnum > numberOfItems) {
			hd->callbacks.saClmClusterTrackCallback(hd->nbuf
			,	hd->itemnum, oc->m_n_member, oc->m_instance
			,	SA_ERR_NO_SPACE);
			return SA_OK;
		}
		pthread_lock();
		retrieve_current_buffer(hd);
		pthread_unlock();
		hd->callbacks.saClmClusterTrackCallback(hd->nbuf, itemnum
		,	oc->m_n_member, oc->m_instance, SA_OK);
		return SA_OK;
	}

	return SA_OK;
}

SaErrorT 
saClmClusterTrackStop(const SaClmHandleT *clmHandle)
{
	__clm_handle_t *hd = GET_CLM_HANDLE(clmHandle);

	if (!hd){
		return SA_ERR_BAD_HANDLE;
	}

	/* This is ugly. But we currently depends on OCF interface, we have
	 * no choice. This should be fixed in the next version after we remove
	 * the dependency with OCF.
	 */
	hd->trackflags = CLM_TRACK_STOP;

	return SA_OK;
}

static SaErrorT
retrieve_node_buffer(SaClmNodeIdT nodeId, SaClmClusterNodeT *clusterNode)
{
	const oc_ev_membership_t *oc;
	uint i;
	char *p;

	oc = (const oc_ev_membership_t *)__ccm_data;

	for (i = 0; i < oc->m_n_member; i++) {
		if (oc->m_array[oc->m_memb_idx+i].node_id == nodeId) {
			clusterNode->nodeId = nodeId;
			clusterNode->member = 1;
			p = oc->m_array[oc->m_memb_idx+i].node_uname;
			if (p) {
				strncpy((char *)clusterNode->nodeName.value, p,
						SA_MAX_NAME_LENGTH - 1);
				clusterNode->nodeName.value \
					[SA_MAX_NAME_LENGTH-1] = '\0';
			} else {
				clusterNode->nodeName.value[0] = '\0';
			}
			goto found;
		}
	}
	for (i = 0; i < oc->m_n_out; i++) {
		if (oc->m_array[oc->m_out_idx+i].node_id == nodeId) {
			clusterNode->nodeId = nodeId;
			clusterNode->member = 0;
			p = oc->m_array[oc->m_out_idx+i].node_uname;
			if (p) {
				strncpy((char *)clusterNode->nodeName.value, p,
						SA_MAX_NAME_LENGTH - 1);
				clusterNode->nodeName.value \
					[SA_MAX_NAME_LENGTH-1] = '\0';
			} else {
				clusterNode->nodeName.value[0] = '\0';
			}
			goto found;
		}
	}
	cl_log(LOG_WARNING, "%s: no record for nodeId [%lu]"
	,	__FUNCTION__, nodeId);
	return SA_ERR_INVALID_PARAM;

found:
	set_misc_node_info(clusterNode);

	return SA_OK;
}

SaErrorT 
saClmClusterNodeGet(SaClmNodeIdT nodeId, SaTimeT timeout,
                    SaClmClusterNodeT *clusterNode)
{
	int i;
	SaErrorT ret;

	if (!clusterNode) {
		cl_log(LOG_ERR, "Invalid parameter clusterNode <%p>"
		,	clusterNode);
		return SA_ERR_INVALID_PARAM;
	}
	for (i = 0; i < timeout; i++) {
		if (__ccm_data){
			break;
		}
		sleep(1);
	}
	if (i == timeout){
		return SA_ERR_TIMEOUT;
	}

	pthread_lock();
	ret = retrieve_node_buffer(nodeId, clusterNode);
	pthread_unlock();
	return ret;
}

/*
 * This API is highly deprecated in version 1 implementation base on OCF.
 * It is actually _not_ an asynchronous call. TODO fix in version 2.
 */
SaErrorT
saClmClusterNodeGetAsync(const SaClmHandleT *clmHandle,
                         SaInvocationT invocation,
                         SaClmNodeIdT nodeId,
                         SaClmClusterNodeT *clusterNode)
{
	int ret;
	__clm_handle_t *hd = GET_CLM_HANDLE(clmHandle);

	if (!hd){
		return SA_ERR_BAD_HANDLE;
	}

	if (!clusterNode) {
		cl_log(LOG_ERR, "Invalid parameter clusterNode <%p>"
		,	clusterNode);
		return SA_ERR_INVALID_PARAM;
	}
	if (!__ccm_data) {
		cl_log(LOG_ERR, "__ccm_data is NULL");
		return SA_ERR_INIT;
	}
	pthread_lock();
	if ((ret = retrieve_node_buffer(nodeId, clusterNode)) != SA_OK) {
		cl_log(LOG_ERR, "retrieve_node_buffer error [%d]", ret);
		pthread_unlock();
		return ret;
	}
	pthread_unlock();

	hd->callbacks.saClmClusterNodeGetCallback(invocation, clusterNode
	,	SA_OK);

	return SA_OK;
}
