'''CTS: Cluster Testing System: AIS dependent modules...
'''

__copyright__='''
Copyright (C) 2007 Andrew Beekhof <andrew@suse.de>

'''

#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

import os, sys, warnings
from CTSvars import *
from CM_lha  import crm_lha
from CTS     import Process

#######################################################################
#
#  LinuxHA v2 dependent modules
#
#######################################################################

class crm_ais(crm_lha):
    '''
    The crm version 3 cluster manager class.
    It implements the things we need to talk to and manipulate
    crm clusters running on top of openais
    '''
    def __init__(self, Environment, randseed=None):
        crm_lha.__init__(self, Environment, randseed=randseed)

        self.update({
            "Name"           : "crm-ais",

            "UUIDQueryCmd"   : "crmadmin -N --openais",
            "EpocheCmd"      : "crm_node -e --openais",
            "QuorumCmd"      : "crm_node -q --openais",
            "ParitionCmd"    : "crm_node -p --openais",

            "Pat:They_stopped" : "%s crmd:.*Node %s: .* state=lost .new",            
            "Pat:ChildExit"    : "Child process .* exited",

            # Bad news Regexes.  Should never occur.
            "BadRegexes"   : (
                r"ERROR:",
                r"CRIT:",
                r"TRACE:",
                r"Shutting down\.",
                r"Forcing shutdown\.",
                r"Timer I_TERMINATE just popped",
                r"input=I_ERROR",
                r"input=I_FAIL",
                r"input=I_INTEGRATED cause=C_TIMER_POPPED",
                r"input=I_FINALIZED cause=C_TIMER_POPPED",
                r"input=I_ERROR",
                r", exiting\.",
                r"WARN.*Ignoring HA message.*vote.*not in our membership list",
                r"pengine.*Attempting recovery of resource",
                r"is taking more than 2x its timeout",
                r"Confirm not received from",
                r"Welcome reply not received from",
                r"Attempting to schedule .* after a stop",
                r"Resource .* was active at shutdown",
                r"duplicate entries for call_id",
                r"Search terminated:",
                r"No need to invoke the TE",
                r":global_timer_callback",
                r"Faking parameter digest creation",
                r"Parameters to .* action changed:",
                r"Parameters to .* changed",
                r"Child process .* terminated with signal 11",
                r"Executing .* fencing operation",
                r"ping.*: DEBUG: Updated connected = 0",
                r"Digest mis-match:",
            ),
        })

    def errorstoignore(self):
        # At some point implement a more elegant solution that 
        #   also produces a report at the end
        '''Return list of errors which are known and very noisey should be ignored'''
        if 1:
            return [ 
                "crm_mon:",
                "crmadmin:",
                "update_trace_data",
                "async_notify: strange, client not found",
                "ERROR: Message hist queue is filling up"
                ]
        return []

    def NodeUUID(self, node):
        return node

    def ais_components(self):   
        fullcomplist = {}
        self.complist = []
        self.common_ignore = [
                    "Pending action:",
                    "ERROR: crm_log_message_adv:",
                    "ERROR: MSG: No message to dump",
                    "pending LRM operations at shutdown",
                    "Lost connection to the CIB service",
                    "Connection to the CIB terminated...",
                    "Sending message to CIB service FAILED",
                    "apply_xml_diff: Diff application failed!",
                    "crmd: .*Action A_RECOVER .* not supported",
                    "pingd: .*ERROR: send_update: Could not send update",
                    "send_ipc_message: IPC Channel to .* is not connected",
                    "unconfirmed_actions: Waiting on .* unconfirmed actions",
                    "cib_native_msgready: Message pending on command channel",
                    "crmd:.*do_exit: Performing A_EXIT_1 - forcefully exiting the CRMd",
                    "verify_stopped: Resource .* was active at shutdown.  You may ignore this error if it is unmanaged.",
                    "ERROR: attrd_connection_destroy: Lost connection to attrd",
                    "nfo: te_fence_node: Executing .* fencing operation",
            ]

        fullcomplist["cib"] = Process(self, "cib", pats = [
                    "State transition .* S_RECOVERY",
                    "Respawning .* crmd",
                    "Respawning .* attrd",
                    "Lost connection to the CIB service",
                    "Connection to the CIB terminated...",
                    "Child process crmd exited .* rc=2",
                    "Child process attrd exited .* rc=1",
                    "crmd: .*Input I_TERMINATE from do_recover",
                    "crmd: .*I_ERROR.*crmd_cib_connection_destroy",
                    "crmd:.*do_exit: Could not recover from internal error",
                    ], badnews_ignore = self.common_ignore)

        fullcomplist["lrmd"] = Process(self, "lrmd", pats = [
                    "State transition .* S_RECOVERY",
                    "LRM Connection failed",
                    "Respawning .* crmd",
                    "crmd: .*I_ERROR.*lrm_connection_destroy",
                    "Child process crmd exited .* rc=2",
                    "crmd: .*Input I_TERMINATE from do_recover",
                    "crmd:.*do_exit: Could not recover from internal error",
                    ], badnews_ignore = self.common_ignore)

        fullcomplist["crmd"] = Process(self, "crmd", pats = [
#                    "WARN: determine_online_status: Node .* is unclean",
#                    "Scheduling Node .* for STONITH",
#                    "Executing .* fencing operation",
# Only if the node wasn't the DC:  "State transition S_IDLE",
                    "State transition .* -> S_IDLE",
                    ], badnews_ignore = self.common_ignore)

        fullcomplist["attrd"] = Process(self, "attrd", pats = [
                    "crmd: .*ERROR: attrd_connection_destroy: Lost connection to attrd"
                    ], badnews_ignore = self.common_ignore)

        fullcomplist["pengine"] = Process(self, "pengine", dc_pats = [
                    "State transition .* S_RECOVERY",
                    "Respawning .* crmd",
                    "Child process crmd exited .* rc=2",
                    "crmd: .*pe_connection_destroy: Connection to the Policy Engine failed",
                    "crmd: .*I_ERROR.*save_cib_contents",
                    "crmd: .*Input I_TERMINATE from do_recover",
                    "crmd:.*do_exit: Could not recover from internal error",
                    ], badnews_ignore = self.common_ignore)

        stonith_ignore = [
            "ERROR: stonithd_signon: ",
            "update_failcount: Updating failcount for child_DoFencing",
            "ERROR: te_connect_stonith: Sign-in failed: triggered a retry",
            ]
        
        stonith_ignore.extend(self.common_ignore)
        
        fullcomplist["stonithd"] = Process(self, "stonithd", process="stonithd", dc_only=1, pats = [
                "tengine_stonith_connection_destroy: Fencing daemon connection failed",
                "Attempting connection to fencing daemon",
                "te_connect_stonith: Connected",
                ], badnews_ignore = stonith_ignore)
        
        vgrind = self.Env["valgrind-procs"].split()
        for key in fullcomplist.keys():
            if self.Env["valgrind-tests"]:
                if key in vgrind:
                    # Processes running under valgrind can't be shot with "killall -9 processname"
                    self.log("Filtering %s from the component list as it is being profiled by valgrind" % key)
                    continue
            if key == "stonithd" and not self.Env["DoFencing"]:
                continue
                
            self.complist.append(fullcomplist[key])

        #self.complist = [ fullcomplist["pengine"] ]
        return self.complist

class crm_whitetank(crm_ais):
    '''
    The crm version 3 cluster manager class.
    It implements the things we need to talk to and manipulate
    crm clusters running on top of openais
    '''
    def __init__(self, Environment, randseed=None):
        crm_ais.__init__(self, Environment, randseed=randseed)

        self.update({
            "Name"           : "crm-whitetank",
            "StartCmd"       : CTSvars.INITDIR+"/openais start",
            "StopCmd"        : CTSvars.INITDIR+"/openais stop",

            "Pat:We_stopped"   : "%s.*openais.*pcmk_shutdown: Shutdown complete",
            "Pat:They_stopped" : "%s crmd:.*Node %s: .* state=lost .new",
            "Pat:They_dead"    : "openais:.*Node %s is now: lost",
            
            "Pat:ChildKilled"  : "%s openais.*Child process %s terminated with signal 9",
            "Pat:ChildRespawn" : "%s openais.*Respawning failed child process: %s",
            "Pat:ChildExit"    : "Child process .* exited",
        })

    def Components(self):    
        self.ais_components()

        aisexec_ignore = [
                    "ERROR: ais_dispatch: Receiving message .* failed",
                    "crmd: .*I_ERROR.*crmd_cib_connection_destroy",
                    "cib: .*ERROR: cib_ais_destroy: AIS connection terminated",
                    #"crmd: .*ERROR: crm_ais_destroy: AIS connection terminated",
                    "crmd:.*do_exit: Could not recover from internal error",
                    "crmd: .*I_TERMINATE.*do_recover",
                    "attrd: .*CRIT: attrd_ais_destroy: Lost connection to OpenAIS service!",
                    "stonithd: .*ERROR: AIS connection terminated",
            ]

        aisexec_ignore.extend(self.common_ignore)

        self.complist.append(Process(self, "aisexec", pats = [
                    "ERROR: ais_dispatch: AIS connection failed",
                    "crmd: .*ERROR: do_exit: Could not recover from internal error",
                    "pengine: .*Scheduling Node .* for STONITH",
                    "stonithd: .*requests a STONITH operation RESET on node",
                    "stonithd: .*Succeeded to STONITH the node",
                    ], badnews_ignore = aisexec_ignore))
        
class crm_flatiron(crm_ais):
    '''
    The crm version 3 cluster manager class.
    It implements the things we need to talk to and manipulate
    crm clusters running on top of openais
    '''
    def __init__(self, Environment, randseed=None):
        crm_ais.__init__(self, Environment, randseed=randseed)

        self.update({
            "Name"           : "crm-flatiron",
            "StartCmd"       : "service corosync start",
            "StopCmd"        : "service corosync stop",

# The next pattern is too early
#            "Pat:We_stopped"   : "%s.*Service engine unloaded: Pacemaker Cluster Manager",
# The next pattern would be preferred, but it doesn't always come out
#            "Pat:We_stopped"   : "%s.*Corosync Cluster Engine exiting with status",
            "Pat:We_stopped"  : "%s.*Service engine unloaded: corosync cluster quorum service",
            "Pat:They_stopped" : "%s crmd:.*Node %s: .* state=lost .new",
            "Pat:They_dead"    : "corosync:.*Node %s is now: lost",
            
            "Pat:ChildKilled"  : "%s corosync.*Child process %s terminated with signal 9",
            "Pat:ChildRespawn" : "%s corosync.*Respawning failed child process: %s",
        })

    def Components(self):    
        self.ais_components()

        corosync_ignore = [
                    "ERROR: ais_dispatch: Receiving message .* failed",
                    "crmd: .*I_ERROR.*crmd_cib_connection_destroy",
                    "cib: .*ERROR: cib_ais_destroy: AIS connection terminated",
                    #"crmd: .*ERROR: crm_ais_destroy: AIS connection terminated",
                    "crmd:.*do_exit: Could not recover from internal error",
                    "crmd: .*I_TERMINATE.*do_recover",
                    "attrd: .*CRIT: attrd_ais_destroy: Lost connection to Corosync service!",
                    "stonithd: .*ERROR: AIS connection terminated",
            ]

#        corosync_ignore.extend(self.common_ignore)

#        self.complist.append(Process(self, "corosync", pats = [
#                    "ERROR: ais_dispatch: AIS connection failed",
#                    "crmd: .*ERROR: do_exit: Could not recover from internal error",
#                    "pengine: .*Scheduling Node .* for STONITH",
#                    "stonithd: .*requests a STONITH operation RESET on node",
#                    "stonithd: .*Succeeded to STONITH the node",
#                    ], badnews_ignore = corosync_ignore))
        
    
        return self.complist
