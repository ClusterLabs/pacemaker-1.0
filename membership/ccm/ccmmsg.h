/*
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _CCMMSG_H_
#define _CCMMSG_H_

#include <ccm.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>

int		ccm_send_cluster_msg(ll_cluster_t* hb, struct ha_msg* msg);
int		ccm_send_node_msg(ll_cluster_t* hb, struct ha_msg* msg, 
				  const char* node);
struct ha_msg*	ccm_create_msg(ccm_info_t * info, int type);
int		ccm_send_protoversion(ll_cluster_t *hb, ccm_info_t *info);
int		ccm_send_one_join_reply(ll_cluster_t *hb, ccm_info_t *info, 
					const char *joiner);
int		ccm_send_standard_clustermsg(ll_cluster_t* hb, ccm_info_t* info, 
					     int type);
int		ccm_send_join(ll_cluster_t *hb, ccm_info_t *info);
int		ccm_send_memlist_request(ll_cluster_t *hb, ccm_info_t *info);
int		ccm_send_memlist_res(ll_cluster_t *hb, 
				     ccm_info_t *info,
				     const char *nodename, 
				     const char *memlist);
int		ccm_send_final_memlist(ll_cluster_t *hb, 
				       ccm_info_t *info, 
				       char *newcookie, 
				       char *finallist,
				       uint32_t max_tran);
int		ccm_send_abort(ll_cluster_t *hb, ccm_info_t *info, 
			       const char *dest, 
			       const int major, 
			       const int minor);
struct ha_msg *	ccm_create_leave_msg(ccm_info_t *info, int uuid);

int		timeout_msg_init(ccm_info_t *info);
struct ha_msg*  timeout_msg_mod(ccm_info_t *info);	
int		ccm_bcast_node_leave_notice(ll_cluster_t* hb, 
					    ccm_info_t* info,
					    const char* node);
int		send_node_leave_to_leader(ll_cluster_t *hb, 
					  ccm_info_t *info, 
					  const char *node);
int		ccm_send_to_all(ll_cluster_t *hb, ccm_info_t *info, 
				char *memlist, char *newcookie,
				int *uptime_list, size_t uptime_size);
int		ccm_send_alive_msg(ll_cluster_t *hb, ccm_info_t *info);
int		ccm_send_newnode_to_leader(ll_cluster_t *hb, 
					   ccm_info_t *info, 
					   const char *node);
int		ccm_send_state_info(ll_cluster_t* hb, 
				    ccm_info_t* info,
				    const char* node);
int		ccm_send_restart_msg(ll_cluster_t* hb, ccm_info_t* info);




#endif
