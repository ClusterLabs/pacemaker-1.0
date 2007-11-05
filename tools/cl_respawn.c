/*
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ******************************************************************************
 * TODO: 
 *	1) Man page
 *	2) Add the "cl_respawn recover" function, for combining with recovery
 *	   manager. But what's its strategy ?
 * 	   The pid will passed by environment
 *	3) Add the function for "-l" option ?
 ******************************************************************************
 *
 * File: cl_respawn.c
 * Description: 
 * 	A small respawn tool which will start a program as a child process, and
 * unless it exits with the "magic" exit code, will restart the program again 
 * if it exits(dies).  It is intended that this respawn program should be usable
 * in resource agent scripts and other places.  The respawn tool should properly
 * log all restarts, and all exits which it doesn't respawn, and run itself as a
 * client of the apphb application heartbeating program, so that it can be 
 * restarted if the program it is monitoring dies.  
 * 
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
 */ 

#include <lha_internal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <glib.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/uids.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/GSource.h>
#include <clplumbing/proctrack.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/cl_pidfile.h>
#include <apphb.h>

static const char * Simple_helpscreen =
"Usage cl_respawn [<options>] <monitored_program> [<arg1>] [<arg2>] ...\n"
"Options are as below:\n"
"-m magic_exit_code\n"
"	When monitored_program exit as this magic_exit_code, then cl_respawn\n"
"	will not try to respawn it.\n"
"-i interval\n"
"	Set the interval(ms) of application hearbeat or plumbing its client.\n" 
"-w warntime\n"
"	Set the warning time (ms) of application heartbeat.\n"
"-p pidfile\n"
"	Set the name of a pid file to use.\n"
"-r	Recover itself from crash. Only called by other monitor programs like"
"	recovery manager.\n"
"-l	List the program monitored by cl_respawn.\n"
"	Notice: donnot support yet.\n"
"-h	Display this simple help.\n";


static void become_daemon(void);
static int  run_client_as_child(char * client_argv[]);
static gboolean plumb_client_and_emit_apphb(gpointer data);
static gboolean cl_respawn_quit(int signo, gpointer user_data);
static void separate_argv(int * argc_p, char *** argv_p, char *** client_argv);
static int cmd_str_to_argv(char * cmd_str, char *** argv);
static void free_argv(char ** argv);

/* Functions for handling the child quit/abort event
 */
static void monitoredProcessDied(ProcTrack* p, int status, int signo
            ,	int exitcode, int waslogged);
static void monitoredProcessRegistered(ProcTrack* p);
static const char * monitoredProcessName(ProcTrack* p);

static ProcTrack_ops MonitoredProcessTrackOps = {
        monitoredProcessDied,
        monitoredProcessRegistered,
        monitoredProcessName
};

static const int
	INSTANCE_NAME_LEN = 20,
	APPHB_INTVL_DETLA = 30;    /* Avoid the incorrect warning message */ 
	
static const unsigned long 
	DEFAULT_APPHB_INTERVAL 	= 2000, /* MS */
	DEFAULT_APPHB_WARNTIME  = 6000; /* MS */

static int MAGIC_EXIT_CODE = 100;

static const char * app_name = "cl_respawn";
static gboolean	REGTO_APPHBD = FALSE;
static char * pidfile = NULL;

/* 
 * This pid will equal to the PID of the process who was ever the child of 
 * that dead cl_respawn.
 */
static pid_t monitored_PID = 0;

static const char * optstr = "rm:i:w:p:lh";
static GMainLoop * mainloop = NULL;
static gboolean IS_RECOVERY = FALSE;

static gboolean shutting_down = FALSE;

int main(int argc, char * argv[])
{
	char app_instance[INSTANCE_NAME_LEN];
	int option_char;
	int interval = DEFAULT_APPHB_INTERVAL;
	int apphb_warntime = DEFAULT_APPHB_WARNTIME;
	char ** client_argv = NULL;
	pid_t child_tmp = 0;

	cl_log_set_entity(app_name);
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(HA_LOG_FACILITY);

	if (argc == 1) { /* no arguments */
		printf("%s\n", Simple_helpscreen);
		exit(LSB_EXIT_EINVAL);
	}

	/* 
	 * Try to separate the option parameter between myself and the client.
	 * Maybe rewrite the argc and argv. 
	 */
	separate_argv(&argc, &argv, &client_argv);
	
	/* code for debug */
#if 0
	{
		int j;
		cl_log(LOG_INFO, "client_argv: 0x%08lx", (unsigned long) client_argv);
		cl_log(LOG_INFO, "Called arg");

		for (j=0; argv[j] != NULL; ++j) {
			cl_log(LOG_INFO, "argv[%d]: %s", j, argv[j]);
		}

		for (j=0; client_argv && client_argv[j] != NULL; ++j) {
			if (ANYDEBUG) {
				cl_log(LOG_INFO, "client_argv[%d]: %s", j, client_argv[j]);
			}
		}
	}
#endif

	do {
		option_char = getopt(argc, argv, optstr);

		if (option_char == -1) {
			break;
		}

		switch (option_char) {
			case 'r':
				IS_RECOVERY = TRUE;
				break;

			case 'm':
				if (optarg) {
					MAGIC_EXIT_CODE = atoi(optarg); 
				}
				break;

			case 'i':
				if (optarg) {
					interval = atoi(optarg);
				} else {
					printf("error.\n");
				}
				break;

			case 'p':
				if (optarg) {
					pidfile = optarg;
				}
				break;
			case 'w':
				if (optarg) {
					apphb_warntime = atoi(optarg);
				}
				break;

			case 'l':
				break;
				/* information */
				return LSB_EXIT_OK;

			case 'h':
				printf("%s\n",Simple_helpscreen);
				return LSB_EXIT_OK;

			default:
				cl_log(LOG_ERR, "getopt returned" 
					"character code %c.", option_char);
				printf("%s\n",Simple_helpscreen);
				return LSB_EXIT_EINVAL;
		}
	} while (1);


	/* 
	 * Now I suppose recovery program only pass the client name via 
	 * environment variables.
	 */
	if ( (IS_RECOVERY == FALSE) && (client_argv == NULL) ) {
		cl_log(LOG_ERR, "Please give the program name which will be " 
			"run as a child process of cl_respawn.");
		printf("%s\n", Simple_helpscreen);
		exit(LSB_EXIT_EINVAL);
	}

	if ((IS_RECOVERY == TRUE ) && ( client_argv == NULL)) {
		/*
		 * Here the client_argv must be NULL. At least now just 
		 * suppose so.
		 */
		/* 
		 * From the environment variables to acquire the necessary
		 * information set by other daemons like recovery manager.
		 * RSP_PID:  the PID of the process which need to be monitored.
		 * RSP_CMD:  the command line to restart the program, which is
		 * the same as the input in command line as above. 
		 */
		if ( getenv("RSP_PID") == NULL ) {
			cl_log(LOG_ERR, "cannot get monitored PID from the "
				"environment variable which should be set by "
				"the recovery program.");
			exit(LSB_EXIT_EINVAL);
		} else {
			monitored_PID = atoi(getenv("RSP_PID"));
		}

		/* 
		 * client_argv == NULL" indicates no client program passed as 
		 * a parameter by others such as a recovery manager, so expect 
		 * it will be passed by environment variable RSP_CMD, see as 
		 * below. If cannot get it, quit.
		 */
		if (client_argv == NULL) {
			if (getenv("RSP_CMD") == NULL) {
				cl_log(LOG_ERR, "cannot get the argument of the "
					"monitored program from the environment "
					"variable, which should be set by the "
					"recovery program.");
			}

			if (0!=cmd_str_to_argv(getenv("RSP_CMD"), &client_argv)) {
				cl_log(LOG_ERR, "Failed to transfer the CLI "
					"string to the argv[] style.");
				exit(LSB_EXIT_EINVAL);
			}	
		}
	}
	
	/* Not use the API 'daemon' since it's not a POSIX's */
	become_daemon();

	/* Code for debug
	int k = 0;
	do {
		cl_log(LOG_INFO,"%s", execv_argv[k]);
	} while (execv_argv[++k] != NULL); 
	*/

	set_sigchld_proctrack(G_PRIORITY_HIGH,DEFAULT_MAXDISPATCHTIME);

	if (( IS_RECOVERY == FALSE )) {
		child_tmp = run_client_as_child(client_argv);
		if (child_tmp > 0 ) {
			cl_log(LOG_NOTICE, "started the monitored program %s, "
			   "whose PID is %d", client_argv[0], child_tmp); 
		} else {
			exit(LSB_EXIT_GENERIC);
		}
	}

	snprintf(app_instance, INSTANCE_NAME_LEN, "%s_%ldd"
		, app_name, (long)getpid());

	if (apphb_register(app_name, app_instance) != 0) {
		cl_log(LOG_WARNING, "Failed to register with apphbd.");
		cl_log(LOG_WARNING, "Maybe apphd isn't running.");
		REGTO_APPHBD = FALSE;
	} else {
		REGTO_APPHBD = TRUE;
		cl_log(LOG_INFO, "Registered with apphbd.");
		apphb_setinterval(interval);
		apphb_setwarn(apphb_warntime);
		/* To avoid the warning when app_interval is very small. */
		apphb_hb();
	}
	Gmain_timeout_add(interval - APPHB_INTVL_DETLA
		, 	  plumb_client_and_emit_apphb, client_argv);

	mainloop = g_main_new(FALSE);
	g_main_run(mainloop);

	if ( REGTO_APPHBD == TRUE ) {
		apphb_hb();
		apphb_unregister();
	}
	
	return LSB_EXIT_OK;
}

static int
run_client_as_child(char * execv_argv[])
{
	long	pid;
	int	i;

	if (execv_argv[0] == NULL) {
		cl_log(LOG_ERR, "Null pointer to program name which need to" 
			"be executed.");
		return LSB_EXIT_EINVAL;
	}

	pid = fork();

	if (pid < 0) {
		cl_log(LOG_ERR, "cannot start monitor program %s.", 
			execv_argv[0]);
		return -1;
	} else if (pid > 0) { /* in the parent process */
		NewTrackedProc( pid, 1, PT_LOGVERBOSE
			, execv_argv, &MonitoredProcessTrackOps);
		monitored_PID = pid;
		return pid;
	}
	
 	/* Now in child process */
	execvp(execv_argv[0], execv_argv);
	/* if go here, there must be something wrong */
	cl_log(LOG_ERR, "%s",strerror(errno));
	cl_log(LOG_ERR, "execving monitored program %s failed.", execv_argv[0]);

	i = 0;
	do {
		free(execv_argv[i]);
	} while (execv_argv[++i] != NULL); 

	/* Since parameter error, donnot need to be respawned */
	exit(MAGIC_EXIT_CODE);
}

/* 
 * Notes: Since the work dir is changed to "/", the client name should include
 * pathname or it's located in the system PATH
*/
static void
become_daemon(void)
{

	int j;

	if (pidfile) {
		int	runningpid;
		if ((runningpid=cl_read_pidfile(pidfile)) > 0) {
			cl_log(LOG_WARNING, "pidfile [%s] says we're already running as pid [%d]"
			,	pidfile, runningpid);
			exit(LSB_EXIT_OK);
		}
		if (cl_lock_pidfile(pidfile) != 0) {
			cl_log(LOG_ERR, "Cannot create pidfile [%s]"
			,	pidfile);
			exit(LSB_EXIT_GENERIC);
		}
	}
#if 0
	pid_t pid;

	pid = fork();

	if (pid < 0) {
		cl_log(LOG_ERR, "cannot start daemon.");
		exit(LSB_EXIT_GENERIC);
	} else if (pid > 0) {
		exit(LSB_EXIT_OK);
	}
#endif

	if (chdir("/") < 0) {
		cl_log(LOG_ERR, "cannot chroot to /.");
		exit(LSB_EXIT_GENERIC);
	}
	
	umask(022);
	setsid();

	for (j=0; j < 3; ++j) {
		close(j);
		(void)open("/dev/null", j == 0 ? O_RDONLY : O_RDWR);
	}

	CL_IGNORE_SIG(SIGINT);
	CL_IGNORE_SIG(SIGHUP);
	
	G_main_add_SignalHandler(G_PRIORITY_DEFAULT, SIGTERM, cl_respawn_quit, NULL, NULL);
}

static gboolean
plumb_client_and_emit_apphb(gpointer data)
{
	pid_t new_pid;
	char ** client_argv = (char **) data;

	if ( REGTO_APPHBD == TRUE ) {
		apphb_hb();
	}
	if (shutting_down) {
		return TRUE;
	}
	/* cl_log(LOG_NOTICE,"donnot emit hb for test."); */
	if ( IS_RECOVERY == TRUE  && !(CL_PID_EXISTS(monitored_PID)) ) {
		cl_log(LOG_INFO, "process %d exited.", monitored_PID);

		new_pid = run_client_as_child(client_argv);
		if (new_pid > 0 ) {
			cl_log(LOG_NOTICE, "restart the monitored program %s,"
				" whose PID is %d", client_argv[0], new_pid); 
		} else { 
			/* 
			 * donnot let recovery manager restart me again, avoid
			 * infinite loop 
			*/
			cl_log(LOG_ERR, "Failed to restart the monitored "
				"program %s, will exit.", client_argv[0]);
			cl_respawn_quit(SIGTERM, NULL);
		}
	}

	return TRUE;
}

static gboolean
cl_respawn_quit(int signo, gpointer user_data)
{
	shutting_down = TRUE;
	if (monitored_PID != 0) {
		cl_log(LOG_INFO, "Killing pid [%d] with SIGTERM"
		,	monitored_PID);
		/* DisableProcLogging(); */
		if (kill(monitored_PID, SIGTERM) < 0) {
			monitored_PID=0;
		}else{
			return TRUE;
		}
	}
	
	if (mainloop != NULL && g_main_is_running(mainloop)) {
		DisableProcLogging();
		g_main_quit(mainloop);
	} else {
		apphb_unregister();
		DisableProcLogging();
		exit(LSB_EXIT_OK);
	}
	return TRUE;
}

static void 
separate_argv(int * argc_p, char *** argv_p, char *** client_argv_p)
{
	/* Search the first no-option parameter */
	int i,j;
	struct stat buf;
	*client_argv_p = NULL;

	for (i=1; i < *argc_p; i++) {
		if (    ((*argv_p)[i][0] != '-') 
		     && (0 == stat((*argv_p)[i], &buf)) ) {
			if (   S_ISREG(buf.st_mode)
			    && ((S_IXUSR| S_IXGRP | S_IXOTH) & buf.st_mode) ) {
				break;
			}
		}
	}

	/* 
	 * Cannot find a valid program name which will be run as a child
	 * process of cl_respawn, may be a recovery.
	 */
	if (*argc_p == i) {
		return;
	}

	*client_argv_p = calloc(*argc_p - i + 1, sizeof(char*));
	if (*client_argv_p == NULL) {
		cl_perror("separate_argv:calloc: ");
		exit(1);
	}

	for (j=i; j < *argc_p; j++) {
		(*client_argv_p)[j-i] = (*argv_p)[j];	
	}

	(*argv_p)[i] = NULL;
	*argc_p = i;

	return;
}

static int 
cmd_str_to_argv(char * cmd_str, char *** client_argv_p)
{
	const int MAX_NUM_OF_PARAMETER = 80;
	char *pre, *next;
	int index = 0;
	int i, len_tmp;

	if (cmd_str == NULL) {
		return LSB_EXIT_EINVAL;
	}
	
	*client_argv_p = calloc(MAX_NUM_OF_PARAMETER, sizeof(char *));
	if (*client_argv_p == NULL) {
		cl_perror("cmd_str_to_argv:calloc: ");
		return LSB_EXIT_GENERIC;
	}

	pre = cmd_str;
	do {
		next = strchr(pre,' ');

		if (next == NULL) {
			len_tmp = strnlen(pre, 80);	
			(*client_argv_p)[index] = calloc(len_tmp+1, sizeof(char));
			if (((*client_argv_p)[index]) == NULL ) {
				cl_perror("cmd_str_to_argv:calloc: ");
				return LSB_EXIT_GENERIC;
			}
			strncpy((*client_argv_p)[index], pre, len_tmp);
			break;
		}

		(*client_argv_p)[index] = calloc(next-pre+1, sizeof(char));
		if (((*client_argv_p)[index]) == NULL ) {
			cl_perror("cmd_str_to_argv:calloc: ");
			return LSB_EXIT_GENERIC;
		}
		strncpy((*client_argv_p)[index], pre, next-pre);

		/* remove redundant spaces between parametes */
		while ((char)(*next)==' ') {
			next++;
		}

		pre = next;
		if (++index >= MAX_NUM_OF_PARAMETER - 1) {
			break; 
		}
	} while (1==1);
	
	if (index >= MAX_NUM_OF_PARAMETER - 1) {
		for (i = 0; i < MAX_NUM_OF_PARAMETER; i++) {
			free((*client_argv_p)[i]);
		} 
		free(*client_argv_p);
		return LSB_EXIT_EINVAL; 
	}

	(*client_argv_p)[index+1] = NULL;

	return 0;
}

static void
monitoredProcessDied(ProcTrack* p, int status, int signo
			, int exitcode, int waslogged)
{
	pid_t new_pid;
	char ** client_argv = (char **) p->privatedata;
	const char * pname = p->ops->proctype(p);

	if (shutting_down) {
		cl_respawn_quit(SIGTERM, NULL);
		p->privatedata = NULL;
		return;
	}

	if ( exitcode == MAGIC_EXIT_CODE) {
		cl_log(LOG_INFO, "Don't restart the monitored program"
			" %s [%d], since we got the magic exit code."
			, pname, p->pid);
		free_argv(client_argv);
		cl_respawn_quit(SIGTERM, NULL);	/* Does NOT always exit */
		return;
	}

	cl_log(LOG_INFO, "process %s[%d] exited, and its exit code is %d"
		, pname, p->pid, exitcode);
	if ( 0 < (new_pid = run_client_as_child(client_argv)) ) {
		cl_log(LOG_NOTICE, "restarted the monitored program, whose PID "
			" is %d", new_pid); 
	} else {
		cl_log(LOG_ERR, "Failed to restart the monitored program %s ,"
			"will exit.", pname );
		free_argv(client_argv);
		cl_respawn_quit(SIGTERM, NULL);	/* Does NOT always exit */
		return;
	}

	p->privatedata = NULL;
}

static void
monitoredProcessRegistered(ProcTrack* p)
{
	cl_log(LOG_INFO, "Child process [%s] started [ pid: %d ]."
			, p->ops->proctype(p), p->pid);
}

static const char *
monitoredProcessName(ProcTrack* p)
{
	char ** argv = p->privatedata;
	return  argv[0];
}

static void
free_argv(char ** argv)
{
	int i = 0;

	if ( argv == NULL ) {
		return;
	}

	do {
		if (argv[i] != NULL) {
			free(argv[i++]);
		} else {
			free(argv);
			return;
		}
	} while (1==1);
}
