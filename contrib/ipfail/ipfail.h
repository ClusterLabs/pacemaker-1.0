/* ipfail.h: ipfail header file
 *
 * Copyright (C) 2003 Kevin Dwyer <kevin@pheared.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/* Various defualts */
#define F_NUMPING               "num_ping"

#define HB_LOCAL_RESOURCES      "local"
#define HB_FOREIGN_RESOURCES    "foreign"
#define HB_ALL_RESOURCES        "all"


/* Data structures */
struct giveup_data {
	ll_cluster_t *hb;
	const char *res_type;
};

/* Prototypes */
void node_walk(ll_cluster_t *);
void set_signals(ll_cluster_t *);
void NodeStatus(const char *, const char *, void *);
void LinkStatus(const char *, const char *, const char *, void *);
void msg_ipfail_join(struct ha_msg *, void *);
void msg_ping_nodes(struct ha_msg *, void *);
void i_am_dead(struct ha_msg *, void *);
void msg_resources(struct ha_msg *, void *);
void gotsig(int);
gboolean giveup(gpointer);
void you_are_dead(ll_cluster_t *);
int ping_node_status(ll_cluster_t *);
void ask_ping_nodes(ll_cluster_t *, int);
void set_callbacks(ll_cluster_t *);
void open_api(ll_cluster_t *);
void close_api(ll_cluster_t *);
gboolean ipfail_dispatch(IPC_Channel *, gpointer);
void ipfail_dispatch_destroy(gpointer);
gboolean ipfail_timeout_dispatch(gpointer);
void delay_giveup(ll_cluster_t *, const char *, int);
void giveup_destroy(gpointer);
void abort_giveup(void);
void send_abort_giveup(ll_cluster_t *);
void msg_abort_giveup(struct ha_msg *, void *);
int is_stable(ll_cluster_t *);
