/* $Id: hb_signal.c,v 1.16 2005/07/29 07:03:46 sunjd Exp $ */
/*
 * hb_signal.c: signal handling routines to be used by Heartbeat
 *
 * Copyright (C) 2002 Horms <horms@verge.net.au>
 *
 * Derived from code in heartbeat.c in this tree
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *
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

#define _USE_BSD
#include <portability.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <hb_config.h>
#include <hb_signal.h>
#include <clplumbing/proctrack.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/realtime.h>
#include <clplumbing/uids.h>
#include <hb_proc.h>
#include <heartbeat_private.h>
#include <heartbeat.h>
#include <clplumbing/setproctitle.h>
#include <clplumbing/GSource.h>
#include <pils/plugin.h>
#include <test.h>


static volatile unsigned int __hb_signal_pending = 0;

#define HB_SIG_REAPER_SIG                      0x0001UL
#define HB_SIG_TERM_SIG                        0x0002UL
#define HB_SIG_DEBUG_USR1_SIG                  0x0004UL
#define HB_SIG_DEBUG_USR2_SIG                  0x0008UL
#define HB_SIG_PARENT_DEBUG_USR1_SIG           0x0010UL
#define HB_SIG_PARENT_DEBUG_USR2_SIG           0x0020UL
#define HB_SIG_REREAD_CONFIG_SIG               0x0040UL
#define HB_SIG_FALSE_ALARM_SIG                 0x0080UL


/*
 * This function does NOT have the same semantics as setting SIG_IGN.
 * Signals set to SIG_IGN never interrupt system calls.
 * Setting this signal handler and calling siginterrupt(nsig, TRUE)
 * will result in the signal interrupting system calls but otherwise
 * being ignored.  This is nice for interrupting writes to serial
 * ports that might otherwise hang forever (for example).
 */
static void
hb_ignoresig(int sig)
{
}

void
hb_signal_signal_all(int sig)
{
	int us = getpid();
	int j;

	extern pid_t processes[MAXPROCS];

	if (ANYDEBUG) {
		ha_log(LOG_DEBUG, "pid %d: received signal %d", us, sig);
		if (curproc) {
			ha_log(LOG_DEBUG, "pid %d: type is %d", us
			,	curproc->type);
		}
	}

	if (sig == SIGTERM) {
		CL_IGNORE_SIG(SIGTERM);
		cl_make_normaltime();
	}

	for (j=0; j < procinfo->nprocs; ++j) {
		if (processes[j] != us && processes[j] != 0) {
			if (ANYDEBUG) {
				ha_log(LOG_DEBUG
				,	"%d: Signalling process %d [%d]"
				,	us, (int) processes[j], (int) sig);
			}
			return_to_orig_privs();
			CL_KILL(processes[j], sig);
			return_to_dropped_privs();
		}
	}
	switch (sig) {
		case SIGTERM:
			/* Shouldn't happen... */
			if (curproc && curproc->type == PROC_MST_CONTROL) {
				return;
			}
			cleanexit(1);
			break;
	}

	return;
}


/* Signal handler to use with SIGCHLD to free the
 * resources of any exited children using wait3(2).
 * This stops zombie processes from hanging around
 */

void
hb_signal_reaper_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_REAPER_SIG;
}


/*
 * We need to handle the case of the exiting process is one of our
 * client children that we spawn as requested when we started up.
 */
void
hb_signal_reaper_action(int waitflags)
{
	int status;
	pid_t	pid;

	while((pid=wait3(&status, waitflags, NULL)) > 0
	||	(pid == -1 && errno == EINTR)) {

		if (pid > 0) {
			/* If they're in the API client table, 
			 * remove them... */
			api_remove_client_pid(pid, "died");
			ReportProcHasDied(pid, status);
		}

	}
}


void
hb_signal_term_handler(int sig)
{
	__hb_signal_pending |= HB_SIG_TERM_SIG;
}


void
hb_signal_term_action(void)
{
	extern volatile struct process_info *curproc;

	return_to_orig_privs();
	cl_make_normaltime();
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG, "Process %d processing SIGTERM"
		, 	(int) getpid());
	}
	if (curproc->type == PROC_MST_CONTROL) {
		hb_initiate_shutdown(FALSE);
	}else{
		cleanexit(SIGTERM);
	}
}


static void
__hb_signal_debug_action(int sig)
{
	extern PILPluginUniv *PluginLoadingSystem;

	switch(sig) {
		case SIGUSR1:
			++debug;
			break;

		case SIGUSR2:
			if (debug > 0) {
				--debug;
			}else{
				debug=0;
			}
			break;
	}

 	PILSetDebugLevel(PluginLoadingSystem, NULL, NULL , debug);
	{
		static char cdebug[8];
		snprintf(cdebug, sizeof(debug), "%d", debug);
		setenv(HADEBUGVAL, cdebug, TRUE);
	}
	if (debug <= 0) {
		unsetenv(HADEBUGVAL);
	}
}


void
hb_signal_debug_usr1_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_DEBUG_USR1_SIG;
}


void
hb_signal_debug_usr1_action(void)
{
	__hb_signal_debug_action(SIGUSR1);
}


void
hb_signal_debug_usr2_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_DEBUG_USR2_SIG;
}


void
hb_signal_debug_usr2_action(void)
{
	__hb_signal_debug_action(SIGUSR2);
}


static void
__parent_hb_signal_debug_action(int sig)
{
	int	olddebug = debug;

	__hb_signal_debug_action(sig);
	hb_signal_signal_all(sig);

	ha_log(LOG_DEBUG, "debug now set to %d [pid %d]", debug
	,	(int) getpid());
	if (debug == 1 && olddebug == 0) {
		hb_versioninfo();
		hb_dump_proc_stats(curproc);
	}
}


void
parent_hb_signal_debug_usr1_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_PARENT_DEBUG_USR1_SIG;
}


void
parent_hb_signal_debug_usr1_action(void)
{
	__parent_hb_signal_debug_action(SIGUSR1);
}


void
parent_hb_signal_debug_usr2_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_PARENT_DEBUG_USR2_SIG;
}


void
parent_hb_signal_debug_usr2_action(void)
{
	__parent_hb_signal_debug_action(SIGUSR2);
}


void
hb_signal_reread_config_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_REREAD_CONFIG_SIG;
}


void
hb_signal_reread_config_action(void)
{
	int	j;
	int	signal_children = 0;

	/* If we're the master control process, tell our children */
	if (curproc->type == PROC_MST_CONTROL) {
		struct	stat	buf;
		if (stat(CONFIG_NAME, &buf) < 0) {
			ha_perror("Cannot stat " CONFIG_NAME);
			return;
		}
		if (ANYDEBUG) {
			ha_log(LOG_DEBUG
			,	"stat of %s: %lu versus old %lu"
			,	CONFIG_NAME
			,	(unsigned long)buf.st_mtime
			,	(unsigned long)config->cfg_time);
		}
		if ((TIME_T)buf.st_mtime != config->cfg_time) {
			procinfo->giveup_resources = FALSE;
			procinfo->restart_after_shutdown = TRUE;
			hb_initiate_shutdown(TRUE);
			return;
		}
		if (stat(KEYFILE, &buf) < 0) {
			ha_perror("Cannot stat " KEYFILE);
		}else if ((TIME_T)buf.st_mtime != config->auth_time) {
			config->rereadauth = TRUE;
			ha_log(LOG_INFO, "Rereading authentication file.");
			signal_children = TRUE;
		}else{
			ha_log(LOG_INFO, "Configuration unchanged.");
		}
	}else{
		/*
		 * We are not the control process, and we received a SIGHUP
		 * signal.  This means the authentication file has changed.
		 */
		ha_log(LOG_INFO, "Child rereading authentication file.");
		config->rereadauth = TRUE;
		check_auth_change(config);
	}

	if (ParseTestOpts() && curproc->type == PROC_MST_CONTROL) {
		signal_children = 1;
	}
	if (signal_children) {
		return_to_orig_privs();
		for (j=0; j < procinfo->nprocs; ++j) {
			if (procinfo->info+j != curproc) {
				CL_KILL(procinfo->info[j].pid, SIGHUP);
			}
		}
		return_to_dropped_privs();
	}
}


void
hb_signal_false_alarm_handler(int sig)
{
	__hb_signal_pending|=HB_SIG_FALSE_ALARM_SIG;
}


void
hb_signal_false_alarm_action(void)
{
	ha_log(LOG_ERR, "Unexpected alarm in process %d", (int) getpid());
}


static sigset_t
__hb_signal_process_pending_mask;
int
__hb_signal_process_pending_mask_set = 0;


void 
hb_signal_process_pending_set_mask_set(const sigset_t *set)
{
	if (!set) {
		return;
	}

	memcpy(&__hb_signal_process_pending_mask, set, sizeof(sigset_t));
	__hb_signal_process_pending_mask_set = 1;
}


unsigned int
hb_signal_pending(void)
{
	return(__hb_signal_pending);
}


void
hb_signal_process_pending(void)
{
	while (__hb_signal_pending) {
		unsigned long	handlers;

		if (__hb_signal_process_pending_mask_set &&
			cl_signal_block_set(SIG_BLOCK
		,	&__hb_signal_process_pending_mask, NULL) < 0) {
			ha_log(LOG_ERR, "hb_signal_process_pending(): "
				"cl_signal_block_set(): "
				"Could not block signals");
		}

		handlers = __hb_signal_pending;
		__hb_signal_pending=0;
	
		/* Allow signals */
		if (__hb_signal_process_pending_mask_set &&
			cl_signal_block_set(SIG_UNBLOCK
		,	&__hb_signal_process_pending_mask, NULL) < 0) {
			ha_log(LOG_ERR, "hb_signal_process_pending(): "
				"cl_signal_block_set(): "
				"Could not unblock signals");
		}

		if (handlers&HB_SIG_TERM_SIG) {
			hb_signal_term_action();
		}

		if (handlers&HB_SIG_DEBUG_USR1_SIG) {
			hb_signal_debug_usr1_action();
		}

		if (handlers&HB_SIG_DEBUG_USR2_SIG) {
			hb_signal_debug_usr2_action();
		}

		if (handlers&HB_SIG_PARENT_DEBUG_USR1_SIG) {
			parent_hb_signal_debug_usr1_action();
		}

		if (handlers&HB_SIG_PARENT_DEBUG_USR2_SIG) {
			parent_hb_signal_debug_usr2_action();
		}

		if (handlers&HB_SIG_REREAD_CONFIG_SIG) {
			hb_signal_reread_config_action();
		}

		if (handlers&HB_SIG_FALSE_ALARM_SIG) {
			hb_signal_false_alarm_action();
		}

		if (handlers&HB_SIG_REAPER_SIG) {
			hb_signal_reaper_action(WNOHANG);
		}
	}
}


int
hb_signal_set_common(sigset_t *set)
{
	sigset_t our_set;
	sigset_t *use_set;

	const cl_signal_mode_t mode [] =
	{	{SIGHUP,	hb_signal_reread_config_handler,1}
	,	{SIGPIPE,	SIG_IGN,			0}
#ifdef  SIGSTP
	,	{SIGSTP,	SIG_IGN,			0}
#endif
#ifdef  SIGTTOU
	,	{SIGTTOU,	SIG_IGN,			0}
#endif
#ifdef  SIGTTIN
	,	{SIGTTIN,	SIG_IGN,			0}
#endif
	,	{SIGINT,	SIG_IGN,			0}
	,	{SIGTERM,	hb_signal_term_handler,		1}
	,	{SIGALRM,	hb_signal_false_alarm_handler,	1}
	,	{SIGUSR1,	hb_signal_debug_usr1_handler,	1}
	,	{SIGUSR2,	hb_signal_debug_usr2_handler,	1}
	,	{0,		0,				0}
	};

	if (set) {
		use_set = set;
	}else{
		use_set = &our_set;

		if (CL_SIGEMPTYSET(use_set) < 0) {
			ha_log(LOG_ERR, "hb_signal_set_common(): "
				"CL_SIGEMPTYSET(): %s", strerror(errno));
			return(-1);
		}
	}

	if (cl_signal_set_handler_mode(mode, use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_common(): "
			"cl_signal_set_handler_mode()");
		return(-1);
	}

	hb_signal_process_pending_set_mask_set(use_set);

	/*
	 * This signal is generated by our ttys in order to cause output
	 * flushing, but we don't want to see it in our software.
	 * I don't think this next function call is needed any more because
	 * it's covered by the cl_signal_mode_t above.
         */
	if (cl_signal_set_interrupt(SIGINT, 0) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_common(): "
			"cl_signal_set_interrupt()");
		return(-1);
	}
	if (cl_signal_block(SIG_BLOCK, SIGINT, NULL) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_common(): "
			"cl_signal_block()");
		return(-1);
	}

	return(0);
}



int
hb_signal_set_write_child(sigset_t *set)
{
	sigset_t our_set;
	sigset_t *use_set;

	const cl_signal_mode_t mode [] = { 
		{SIGALRM,	hb_ignoresig,	1}
	,	{0,		0,		0}
	};

	if (set) {
		use_set = set;
	}else{
		use_set = &our_set;

		if (CL_SIGEMPTYSET(use_set) < 0) {
			ha_log(LOG_ERR, "hb_signal_set_write_child(): "
				"CL_SIGEMPTYSET(): %s", strerror(errno));
			return(-1);
		}
	}

	if (hb_signal_set_common(use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_write_child(): "
			"hb_signal_set_common()");
		return(-1);
	}

	if (cl_signal_set_handler_mode(mode, use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_write_child(): "
			"cl_signal_set_handler_mode()");
		return(-1);
	}

	hb_signal_process_pending_set_mask_set(use_set);

	return(0);
}


int
hb_signal_set_read_child(sigset_t *set)
{
	if (hb_signal_set_common(set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_read_child(): "
			"hb_signal_set_common()");
		return(-1);
	}

	hb_signal_process_pending_set_mask_set(set);

	return(0);
}

int
hb_signal_set_fifo_child(sigset_t *set)
{
	sigset_t *use_set;
	sigset_t our_set;

	const cl_signal_mode_t mode [] = { 
		{SIGALRM,	hb_ignoresig,	1}
	,	{0,		0,		0}
	};

	if (set) {
		use_set = set;
	}else{
		use_set = &our_set;

		if (CL_SIGEMPTYSET(use_set) < 0) {
			ha_log(LOG_ERR, "hb_signal_set_write_child(): "
				"CL_SIGEMPTYSET(): %s", strerror(errno));
			return(-1);
		}
	}

	if (hb_signal_set_common(use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_fifo_child(): "
			"hb_signal_set_common()");
		return(-1);
	}

	if (cl_signal_set_handler_mode(mode, use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_fifo_child(): "
			"cl_signal_set_handler_mode()");
		return(-1);
	}

	if (cl_signal_set_handler_mode(mode, use_set) < 0) {
		ha_log(LOG_ERR, "%s(): cl_signal_set_handler_mode() failed."
		,	__FUNCTION__);
		return(-1);
	}

	hb_signal_process_pending_set_mask_set(set);

	return(0);
}

int
hb_signal_set_master_control_process(sigset_t *set)
{
	sigset_t our_set;
	sigset_t *use_set;

	const cl_signal_mode_t mode [] =
	{	{SIGTERM,	hb_signal_term_handler,	1}
	,	{SIGUSR1,	parent_hb_signal_debug_usr1_handler,	1}
	,	{SIGUSR2,	parent_hb_signal_debug_usr2_handler,	1}
	,	{SIGALRM,       hb_signal_false_alarm_handler,        	1}
	,	{0,		0,					0}
	};

	if (set) {
		use_set = set;
	}else{
		use_set = &our_set;

		if (CL_SIGEMPTYSET(use_set) < 0) {
			ha_log(LOG_ERR, 
				"hb_signal_set_master_control_process(): "
				"CL_SIGEMPTYSET(): %s", strerror(errno));
			return(-1);
		}
	}

	if (hb_signal_set_common(use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_master_control_process(): "
			"hb_signal_set_common()");
		return(-1);
	}

	if (cl_signal_set_handler_mode(mode, use_set) < 0) {
		ha_log(LOG_ERR, "hb_signal_set_master_control_process(): "
			"cl_signal_set_handler_mode()");
		return(-1);
	}
	
	set_sigchld_proctrack(G_PRIORITY_HIGH);
	hb_signal_process_pending_set_mask_set(use_set);

	return(0);
}
