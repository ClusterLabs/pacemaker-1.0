/* $Id: atest.c,v 1.3 2005/02/10 11:08:11 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
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
#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <apphb.h>

#include <clplumbing/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/uids.h>
#include <clplumbing/realtime.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_poll.h>
#include <clplumbing/coredumps.h>

#include <crm/crm.h>
#include <crm/common/ctrl.h>
#include <crm/common/ipc.h>
#include <crm/common/xml.h>

#include <crmd.h>
#include <crmd_fsa.h>
#include <crmd_messages.h>
#include <crm/cib.h>

#include <crm/dmalloc_wrapper.h>

const char* crm_system_name = "core";

gboolean process_atest_message(
	HA_Message *msg, crm_data_t *xml_data, IPC_Channel *sender);


GMainLoop*  mainloop = NULL;

int
main(int argc, char ** argv)
{
	IPC_Channel* channels[2];
	crm_data_t *a_cib = NULL;
	HA_Message *cmd = NULL;
	
	crm_log_init(crm_system_name);
	set_crm_log_level(LOG_INSANE-1);
	
	a_cib = string2xml("<cib_fragment section=\"status\"><cib timestamp=\"1107940665\" generated=\"true\" cib_feature_revision=\"1\" debug_source=\"do_lrm_query\"><configuration timestamp=\"1107940665\"><crm_config/><nodes/><resources/><constraints/></configuration><status timestamp=\"1107940665\"><node_state replace=\"lrm\" id=\"f67904e0-4dfc-4db1-83a2-e930fc1d20f4\" uname=\"c001n09\"><lrm><lrm_resources/><lrm_agents><lrm_agent class=\"stonith\" type=\"apcmaster\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"apcsmart\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"baytech\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"external\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"ibmhmc\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"meatware\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"null\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"nw_rpc100s\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"rcd_serial\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"riloe\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"rps10\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"ssh\" version=\"1\"/><lrm_agent class=\"stonith\" type=\"wti_nps\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"portblock\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"ldirectord\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"db2\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"apache\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"Xinetd\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"WinPopup\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"WAS\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"ServeRAID\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"SendArp\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"Raid1\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"OCF\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"MailTo\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"LinuxSCSI\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"LVM\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"IPv6addr\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"IPsrcaddr\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"IPaddr2\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"IPaddr\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"ICP\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"Filesystem\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"Delay\" version=\"1\"/><lrm_agent class=\"heartbeat\" type=\"AudibleAlarm\" version=\"1\"/></lrm_agents><metatdata/></lrm></node_state></status></cib></cib_fragment>");
	/* createEmptyCib(); */
	cl_log_message(LOG_DEBUG, (HA_Message*)a_cib);

	if (ipc_channel_pair(channels) != IPC_OK) {
		cl_perror("Can't create ipc channel pair");
		exit(1);
	}
	G_main_add_IPC_Channel(G_PRIORITY_LOW,
			       channels[1], FALSE,
			       subsystem_msg_dispatch,
			       (void*)process_atest_message, 
			       default_ipc_connection_destroy);

	/* send transition graph over IPC instead */
	cmd = create_request(CRM_OP_TRANSITION, a_cib, NULL,
			     CRM_SYSTEM_TENGINE, CRM_SYSTEM_TENGINE, NULL);
	
	send_ipc_message(channels[0], cmd);

	mainloop = g_main_new(FALSE);
	crm_debug("Starting mainloop");
	g_main_run(mainloop);
	
#if 0
    /* read local config file */
    crm_debug("Enabling coredumps");
    if(cl_set_corerootdir(HA_COREDIR) < 0){
	    cl_perror("cannot set corerootdir");
    }
    if(cl_enable_coredumps(1) != 0) {
	    crm_err("Cannot enable coredumps");
    }
    if(cl_cdtocoredir() != 0) {
	    crm_err("Cannot cd to coredump dir");
    }

    crm_warn("Calling abort()");
    abort();
    crm_err("We just dumped core");
#endif
    return 0;
}

gboolean
process_atest_message(HA_Message *msg, crm_data_t *xml_data, IPC_Channel *sender)
{
	crm_debug("made it");
	cl_log_message(LOG_DEBUG, msg);
	return TRUE;
}
