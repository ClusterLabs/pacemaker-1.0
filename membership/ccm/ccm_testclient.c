/* $Id: ccm_testclient.c,v 1.17 2005/03/16 17:11:15 lars Exp $ */
/* 
 * ccm.c: A consensus cluster membership sample client
 *
 * Copyright (c) International Business Machines  Corp., 2000
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
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
#include <ocf/oc_event.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <clplumbing/cl_log.h>


oc_ev_t *ev_token;

extern void oc_ev_special(const oc_ev_t *, oc_ev_class_t , int );


static void 
my_ms_events(oc_ed_t event, void *cookie, 
		size_t size, const void *data)
{
	const oc_ev_membership_t *oc = (const oc_ev_membership_t *)data;
	uint i;
	int i_am_in;

 	cl_log(LOG_INFO,"event=%s:", 
	       event==OC_EV_MS_NEW_MEMBERSHIP?"NEW MEMBERSHIP":
	       event==OC_EV_MS_NOT_PRIMARY?"NOT PRIMARY":
	       event==OC_EV_MS_PRIMARY_RESTORED?"PRIMARY RESTORED":
	       event==OC_EV_MS_EVICTED?"EVICTED":
	       "NO QUORUM MEMBERSHIP"
	       );

	if(OC_EV_MS_EVICTED == event) {
		oc_ev_callback_done(cookie);
		return;
	}

	cl_log(LOG_INFO,"instance=%d\n"
	       "# ttl members=%d, ttl_idx=%d\n"
	       "# new members=%d, new_idx=%d\n"
	       "# out members=%d, out_idx=%d",
	       oc->m_instance,
	       oc->m_n_member,
	       oc->m_memb_idx,
	       oc->m_n_in,
	       oc->m_in_idx,
	       oc->m_n_out,	       
	       oc->m_out_idx);
	
	i_am_in=0;
	cl_log(LOG_INFO, "NODES IN THE PRIMARY MEMBERSHIP");
	for(i=0; i<oc->m_n_member; i++) {
		cl_log(LOG_INFO,"\tnodeid=%d, uname=%s, born=%d",
			oc->m_array[oc->m_memb_idx+i].node_id,
			oc->m_array[oc->m_memb_idx+i].node_uname,
			oc->m_array[oc->m_memb_idx+i].node_born_on);
		if(oc_ev_is_my_nodeid(ev_token, &(oc->m_array[i]))){
			i_am_in=1;
		}
	}
	if(i_am_in) {
		cl_log(LOG_INFO,"MY NODE IS A MEMBER OF THE MEMBERSHIP LIST");
	}

	cl_log(LOG_INFO, "NEW MEMBERS");
	if(oc->m_n_in==0) 
		cl_log(LOG_INFO, "\tNONE");
	for(i=0; i<oc->m_n_in; i++) {
		cl_log(LOG_INFO,"\tnodeid=%d, uname=%s, born=%d",
			oc->m_array[oc->m_in_idx+i].node_id,
			oc->m_array[oc->m_in_idx+i].node_uname,
			oc->m_array[oc->m_in_idx+i].node_born_on);
	}
	cl_log(LOG_INFO, "MEMBERS LOST");
	if(oc->m_n_out==0) 
		cl_log(LOG_INFO, "\tNONE");
	for(i=0; i<oc->m_n_out; i++) {
		cl_log(LOG_INFO,"\tnodeid=%d, uname=%s, born=%d",
			oc->m_array[oc->m_out_idx+i].node_id,
			oc->m_array[oc->m_out_idx+i].node_uname,
			oc->m_array[oc->m_out_idx+i].node_born_on);
	}
	cl_log(LOG_INFO, "-----------------------");
	oc_ev_callback_done(cookie);
}

int
main(int argc, char *argv[])
{
	int ret;
	fd_set rset;
	int	my_ev_fd;

	cl_log_set_entity(argv[0]);
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_USER);
	oc_ev_register(&ev_token);

	oc_ev_set_callback(ev_token, OC_EV_MEMB_CLASS, my_ms_events, NULL);
	oc_ev_special(ev_token, OC_EV_MEMB_CLASS, 0/*don't care*/);

	ret = oc_ev_activate(ev_token, &my_ev_fd);
	if(ret){
		oc_ev_unregister(ev_token);
		return(1);
	}

	for (;;) {

		FD_ZERO(&rset);
		FD_SET(my_ev_fd, &rset);

		if(select(my_ev_fd + 1, &rset, NULL,NULL,NULL) == -1){
			perror("select");
			return(1);
		}
		if(oc_ev_handle_event(ev_token)){
			cl_log(LOG_ERR,"terminating");
			return(1);
		}
	}
	return 0;
}
