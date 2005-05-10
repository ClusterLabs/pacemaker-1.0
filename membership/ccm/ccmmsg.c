
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

