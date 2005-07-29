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

#include <ccm.h>
#include <config.h>
#include <ha_config.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>

int ccm_send_cluster_msg(ll_cluster_t* hb, struct ha_msg* msg);
int ccm_send_node_msg(ll_cluster_t* hb, struct ha_msg* msg, const char* node);
int
ccm_send_cluster_msg(ll_cluster_t* hb, struct ha_msg* msg)
{
	int rc;
	
	rc = hb->llc_ops->sendclustermsg(hb, msg);
	if (rc != HA_OK){
		cl_log(LOG_INFO, "sending out message failed");
		cl_log_message(LOG_INFO, msg);
		return rc;
	}
	
	return HA_OK;
}


int
ccm_send_node_msg(ll_cluster_t* hb, 
		  struct ha_msg* msg, 
		  const char* node)
{
	int rc;
	
	rc = hb->llc_ops->sendclustermsg(hb, msg);
	if (rc != HA_OK){
		cl_log(LOG_INFO, "sending out message failed");
		cl_log_message(LOG_INFO, msg);
		return rc;
	}
	
	return HA_OK;
	
	
}

