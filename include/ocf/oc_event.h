/* $Id: oc_event.h,v 1.5 2005/11/09 22:22:56 gshi Exp $ */
/*
 * oc_event.h
 *
 * Definition of the Open Cluster Framework event notification API
 *
 *  Copyright (C) 2002 Mark Haverkamp, Joe DiMartino
 *                2002 Open Source Development Lab
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef OC_EVENT_H
#define OC_EVENT_H
#include <sys/types.h>
#include <inttypes.h>

/*
 * An opaque token into the membership service is
 * defined as an int for portability.
 */
typedef int oc_ev_t;


/*
 * oc_ed_t is the event descriptor for a callback event.  An event
 * descriptor is unique for all events across all event classes.
 */

typedef uint32_t  oc_ed_t;

/*
 * Event descriptors:
 *	upper 10 bits for Class
 *	lower 22 bits for Event
 */

#define OC_EV_CLASS_SHIFT	22
#define OC_EV_EVENT_SHIFT	10
#define	OC_EV_EVENT_MASK	(~ (~((uint)0) << OC_EV_CLASS_SHIFT))

#define	OC_EV_GET_CLASS(ed)	((uint)(ed) >> OC_EV_CLASS_SHIFT)
#define	OC_EV_GET_EVENT(ed)	((uint)(ed) & OC_EV_EVENT_MASK)
#define	OC_EV_SET_CLASS(cl,ev)	(cl << OC_EV_CLASS_SHIFT | \
				(ev & OC_EV_EVENT_MASK))


/*
 * The following event classes are defined:
 */

typedef enum {
	OC_EV_CONN_CLASS = 1,	/* Connectivity Event Class */
	OC_EV_MEMB_CLASS,	/* Node Membership Event Class */
	OC_EV_GROUP_CLASS	/* Group Messaging Event Class */
} oc_ev_class_t;


/*
 * Within each event class, event types are defined.
 */

/*
 * Connectivity Events
 */
typedef enum {
	OC_EV_CS_INVALID = OC_EV_SET_CLASS(OC_EV_CONN_CLASS, 0),
	OC_EV_CS_INTERFACE,
	OC_EV_CS_ELIGIBLE,
	OC_EV_CS_CONNECT
} oc_conn_event_t;


/* Node Membership Events
 * (see http://wiki.linux-ha.org/CCM/MembershipCallback for more info)
 *
 *OC_EV_MS_NEW_MEMBERSHIP 
 *	CCM: membership with quorum 
 *
 *OC_EV_MS_MS_INVALID 
 *	CCM: membership without quorum 
 *
 *OC_EV_MS_NOT_PRIMARY 
 *	CCM: old membership (not valid any longer) 
 *
 *OC_EV_MS_PRIMARY_RESTORED 
 *	This event mean the cluster restores to a stable state that has the same membership as before. 
 *	It also implies it has the same quorum as before. 
 *	CCM: old membership restored (same membership as before) 
 *
 *OC_EV_MS_EVICTED 
 *	CCM: the client is evicted from ccm. 
 *
 */

typedef enum {
	OC_EV_MS_INVALID = OC_EV_SET_CLASS(OC_EV_MEMB_CLASS, 0),
	OC_EV_MS_NEW_MEMBERSHIP,
	OC_EV_MS_NOT_PRIMARY,
	OC_EV_MS_PRIMARY_RESTORED,
	OC_EV_MS_EVICTED
} oc_memb_event_t;

/*
 * For events OC_EV_MS_NEW_MEMBERSHIP, OC_EV_MS_NOT_PRIMARY, and
 * OC_EV_MS_PRIMARY_RESTORED, the event handlers 'data' member points 
 * to an oc_ev_mebership_t structure.  For OC_EV_MS_EVICTED, 'data' is 
 * NULL.
 */

/*
 * member node information
 */
typedef struct oc_node_s {
	char   *node_uname;     /* unique */
	uint    node_id;        /* unique */
	uint    node_born_on;   /* membership instance number */
} oc_node_t;

/*
 * membership event information
 */
typedef struct oc_ev_membership_s {
	uint    m_instance;     /* instance # of current membership */

	uint    m_n_member;     /* # of current members */
	uint    m_memb_idx;     /* index into m_array for members */

	uint    m_n_out;        /* # of previous members lost */
	uint    m_out_idx;      /* index into m_array for lost */

	uint    m_n_in;         /* # of new members in this instance */
	uint    m_in_idx;       /* index into m_array for new */

	oc_node_t m_array[1];   /* array of members (see above) */
} oc_ev_membership_t;

/*
 * Group Events
 */
typedef enum {
	OC_EV_GS_INVALID = OC_EV_SET_CLASS(OC_EV_GROUP_CLASS, 0),
	OC_EV_GS_JOIN,
	OC_EV_GS_LEAVE,
	OC_EV_GS_CAST,
	OC_EV_GS_REPLY
} oc_group_event_t;


/*
 * This is the initial call to register for cluster event
 * notification service.  Callers receive an opaque token.
 * Implementations define the contents of the opaque token.
 * Failure returns an appropriate value.
 */

int oc_ev_register(oc_ev_t **token);

/*
 * Event service will terminate after calling oc_ev_unregister().
 * This routine can be safely called from a callback routine.
 * Pending events may be dropped at the discression of the cluster
 * implementation.
 */

int oc_ev_unregister(oc_ev_t *token);

/*
 * callback function definition
 */

typedef void oc_ev_callback_t(oc_ed_t event,
				void *cookie,
				size_t size,
				const void *data);

/*
 * Event notification is performed through callbacks.  Events are
 * delivered only for those event classes in which a callback has
 * been registered.  The callback function is registered using
 * oc_ev_set_callback().  A callback is delivered when an event in
 * the corresponding event class occurs.
 */

int oc_ev_set_callback(const oc_ev_t *token,
			oc_ev_class_t class,
			oc_ev_callback_t *fn,
			oc_ev_callback_t **prev_fn);


/*
 * For calls within the kernel only the event service token is
 * used and all other arguments are ignored.  After activation,
 * kernel callbacks may be delivered immediately.  All kernel
 * callbacks will be performed in a process context supplied by the
 * kernel compliant event notification service.
 */

int oc_ev_activate(const oc_ev_t *token, int *fd);


/* 
 * A user-level process determines that an event is pending using
 * select/poll on the file descriptor returned by oc_ev_activate().
 * A callback will deliver the event in the context of this process
 * after calling oc_ev_handle_event().
 */

int oc_ev_handle_event(const oc_ev_t *token);


/* 
 * It is necessary to inform the notification service that callback
 * processing is complete.  Any data associated with this completed
 * callback is no longer valid upon successful return.
 */

int oc_ev_callback_done(void *cookie);

/*
 * This is a synchronous call to return the event notification
 * service version number.  It is safe to call anytime.
int oc_ev_get_version(const oc_ev_t *token, oc_ver_t *ver);
 */

/* 
 * This is a synchronous call to determine the local node identifier.
 */

int oc_ev_is_my_nodeid(const oc_ev_t *token, const oc_node_t *node);

#endif  /* OC_EVENT_H */
