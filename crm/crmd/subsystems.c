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

#include <lha_internal.h>

#include <sys/param.h>
#include <crm/crm.h>
#include <crmd_fsa.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>			/* for access */
#include <clplumbing/cl_signal.h>
#include <clplumbing/realtime.h>
#include <clplumbing/proctrack.h>
#include <sys/types.h>	/* for calls to open */
#include <sys/stat.h>	/* for calls to open */
#include <fcntl.h>	/* for calls to open */
#include <pwd.h>	/* for getpwuid */
#include <grp.h>	/* for initgroups */

#include <sys/time.h>	/* for getrlimit */
#include <sys/resource.h>/* for getrlimit */

#include <errno.h>

#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crmd_messages.h>
#include <crmd_callbacks.h>

#include <crm/cib.h>
#include <crmd.h>


static void
crmdManagedChildRegistered(ProcTrack* p)
{
	struct crm_subsystem_s *the_subsystem = p->privatedata;
	the_subsystem->pid = p->pid;
}


static void
crmdManagedChildDied(
	ProcTrack* p, int status, int signo, int exitcode, int waslogged)
{
	struct crm_subsystem_s *the_subsystem = p->privatedata;
	
	crm_info("Process %s:[%d] exited (signal=%d, exitcode=%d)",
		 the_subsystem->name, the_subsystem->pid, signo, exitcode);
	
	the_subsystem->pid = -1;
	the_subsystem->ipc = NULL;	
	clear_bit_inplace(fsa_input_register, the_subsystem->flag_connected);

	crm_debug_3("Triggering FSA: %s", __FUNCTION__);
	G_main_set_trigger(fsa_source);

	if(is_set(fsa_input_register, the_subsystem->flag_required)) {
		/* this wasnt supposed to happen */
		crm_err("The %s subsystem terminated unexpectedly",
			the_subsystem->name);
		
		register_fsa_input_before(C_FSA_INTERNAL, I_ERROR, NULL);
	}

	p->privatedata = NULL;
}

static const char *
crmdManagedChildName(ProcTrack* p)
{
	struct crm_subsystem_s *the_subsystem = p->privatedata;
	return the_subsystem->name;
}

static ProcTrack_ops crmd_managed_child_ops = {
	crmdManagedChildDied,
	crmdManagedChildRegistered,
	crmdManagedChildName
};

gboolean
stop_subsystem(struct crm_subsystem_s *the_subsystem, gboolean force_quit)
{
	int quit_signal = SIGTERM;
	crm_debug_2("Stopping sub-system \"%s\"", the_subsystem->name);
	clear_bit_inplace(fsa_input_register, the_subsystem->flag_required);
	
	if (the_subsystem->pid <= 0) {
		crm_debug_2("Client %s not running", the_subsystem->name);
		return FALSE;
		
	}

	if(is_set(fsa_input_register, the_subsystem->flag_connected) == FALSE) {
		/* running but not yet connected */
		crm_debug("Stopping %s before it had connected",
			  the_subsystem->name);
	}
/*
	if(force_quit && the_subsystem->sent_kill == FALSE) {
		quit_signal = SIGKILL;

	} else if(force_quit) {
		crm_debug("Already sent -KILL to %s: [%d]",
			  the_subsystem->name, the_subsystem->pid);
	}
*/
	errno = 0;
	if(CL_KILL(the_subsystem->pid, quit_signal) == 0) {
		crm_info("Sent -TERM to %s: [%d]",
			 the_subsystem->name, the_subsystem->pid);
		the_subsystem->sent_kill = TRUE;
		
	} else {
		cl_perror("Sent -TERM to %s: [%d]",
			  the_subsystem->name, the_subsystem->pid);
	}
	
	return TRUE;
}


gboolean
start_subsystem(struct crm_subsystem_s*	the_subsystem)
{
	pid_t       pid;
	struct stat buf;
	int         s_res;
	unsigned int	j;
	struct rlimit	oflimits;
	const char 	*devnull = "/dev/null";

	crm_info("Starting sub-system \"%s\"", the_subsystem->name);
	set_bit_inplace(fsa_input_register, the_subsystem->flag_required);

	if (the_subsystem->pid > 0) {
		crm_warn("Client %s already running as pid %d",
			the_subsystem->name, (int) the_subsystem->pid);

		/* starting a started X is not an error */
		return TRUE;
	}

	/*
	 * We want to ensure that the exec will succeed before
	 * we bother forking.
	 */

	if (access(the_subsystem->path, F_OK|X_OK) != 0) {
		cl_perror("Cannot (access) exec %s", the_subsystem->path);
		return FALSE;
	}

	s_res = stat(the_subsystem->command, &buf);
	if(s_res != 0) {
		cl_perror("Cannot (stat) exec %s", the_subsystem->command);
		return FALSE;
	}
	
	/* We need to fork so we can make child procs not real time */
	switch(pid=fork()) {
		case -1:
			crm_err("Cannot fork.");
			return FALSE;

		default:	/* Parent */
			NewTrackedProc(pid, 0, PT_LOGNORMAL,
				       the_subsystem, &crmd_managed_child_ops);
			crm_debug_2("Client %s is has pid: %d",
				    the_subsystem->name, pid);
			the_subsystem->pid = pid;
			return TRUE;

		case 0:		/* Child */
			/* create a new process group to avoid
			 * being interupted by heartbeat
			 */
			setpgid(0, 0);
			break;
	}

	crm_debug("Executing \"%s (%s)\" (pid %d)",
		  the_subsystem->command, the_subsystem->name, (int) getpid());

	/* A precautionary measure */
	getrlimit(RLIMIT_NOFILE, &oflimits);
	for (j=0; j < oflimits.rlim_cur; ++j) {
		close(j);
	}
	
	(void)open(devnull, O_RDONLY);	/* Stdin:  fd 0 */
	(void)open(devnull, O_WRONLY);	/* Stdout: fd 1 */
	(void)open(devnull, O_WRONLY);	/* Stderr: fd 2 */
	
	if(getenv("HA_VALGRIND_ENABLED") != NULL) {
		char *opts[] = { crm_strdup(VALGRIND_BIN),
 				 crm_strdup("--show-reachable=yes"),
				 crm_strdup("--leak-check=full"),
				 crm_strdup("--time-stamp=yes"),
				 crm_strdup("--suppressions="VALGRIND_SUPP),
/* 				 crm_strdup("--gen-suppressions=all"), */
				 crm_strdup(VALGRIND_LOG),
				 crm_strdup(the_subsystem->command),
				 NULL
		};
		(void)execvp(VALGRIND_BIN, opts);
	} else {
		char *opts[] = { crm_strdup(the_subsystem->command), NULL };
		(void)execvp(the_subsystem->command, opts);
	}
	
	/* Should not happen */
	cl_perror("FATAL: Cannot exec %s", the_subsystem->command);

	exit(100); /* Suppress respawning */
	return TRUE; /* never reached */
}
