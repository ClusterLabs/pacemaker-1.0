/* $Id: apphbtest.c,v 1.25 2005/07/29 06:21:10 sunjd Exp $ */
/*
 * apphbtest:	application heartbeat test program
 *
 * This program tests apphbd. It registers with the application heartbeat 
 * server and issues heartbeats from time to time...
 *
 * Copyright(c) 2002 Alan Robertson <alanr@unix.sh>
 *
 *********************************************************************
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
#include <portability.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <clplumbing/cl_log.h>
#include <apphb.h>

int debug = 0;

void doafailtest(void);

void multi_hb_test(int child_proc_num, int hb_intvl_ms, int hb_num
, int delaysecs, int dofailuretests);

void hb_normal(int hb_intvl_ms, int delaysecs, int hb_num);

void apphb_setwarn_test(int warnhb_ms, int hb_ms);

void dup_reg_test(void);

#define APPNAME_LEN 256
#define OPTARGS "n:p:i:l:dFh"
#define USAGE_STR "Usage: [-n heartbeat number] \
[-p process number] \
[-l delay seconds] \
[-i heartbeat interval(ms)] \
[-d](debug information) \
[-F](enable failure cases) \
[-h](print help message)"

int
main(int argc,char ** argv)
{
	int flag;
	int hb_num = 10;
	int child_proc_num = 1;
	int hb_intvl_ms = 1000;
	int dofailuretests = FALSE;
	int delaysecs = -1;
	
	while (( flag = getopt(argc, argv, OPTARGS)) != EOF) {
		switch(flag) {
			case 'n':	/* Number of heartbeat */
				hb_num = atoi(optarg);
				break;
			case 'p':	/* Number of heartbeat processes */
				child_proc_num = atoi(optarg);
				break;
			case 'i':	/* Heartbeat interval */
				hb_intvl_ms = atoi(optarg);
				break;
			case 'l':	/* Delay before starting multiple clients */
				delaysecs = atoi(optarg);
				break;
			case 'd':	/* Debug */
				debug += 1;
				break;
			case 'F':	/* Enable failure cases */
				dofailuretests = TRUE;
				break;
			case 'h':
			default:
				fprintf(stderr
				,	"%s "USAGE_STR"\n", argv[0]);
				return(1);	
		}
	}

	cl_log_set_entity(argv[0]);
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_USER);
	
	if (delaysecs < 0) 
		delaysecs = child_proc_num;
	multi_hb_test(child_proc_num, hb_intvl_ms, hb_num, delaysecs
			, dofailuretests);
	
	if (dofailuretests) {
		/*  run these fail cases if you want */
		/*
		apphb_setwarn_test(2000, 1000);
		apphb_setwarn_test(1000, 2000);
		dup_reg_test();		
		*/
	}
	
	return(0);
}

void
doafailtest(void)
{
	int	j;
	int	rc;
	char	app_name[] = "failtest";
	char	app_instance[APPNAME_LEN];
	
	snprintf(app_instance, sizeof(app_instance)
	,	"%s_%ld", app_name, (long)getpid());
	
	cl_log(LOG_INFO, "Client %s registering", app_instance);
	
	rc = apphb_register(app_name, app_instance);
	if (rc < 0) {
		cl_perror("%s registration failure", app_instance);
		exit(1);
	}
	if (debug) {
		cl_log(LOG_INFO, "Client %s registered", app_instance);
	}
	
	cl_log(LOG_INFO, "Client %s setting 2 second heartbeat period"
			, app_instance);
	rc = apphb_setinterval(2000);
	if (rc < 0) {
		cl_perror("%s setinterval failure", app_instance);
		exit(2);
	}

	for (j=0; j < 10; ++j) {
		sleep(1);
		if (debug) 
			fprintf(stderr, "+");
		if (j == 8) {
			apphb_setwarn(500);
		}
		rc = apphb_hb();
		if (rc < 0) {
			cl_perror("%s apphb_hb failure", app_instance);
			exit(3);
		}

	}
	if (debug) {
		fprintf(stderr, "\n");
	}
	sleep(3);
	if (debug) 
		fprintf(stderr, "!");
	rc = apphb_hb();
	if (rc < 0) {
		cl_perror("%s late apphb_hb failure", app_instance);
		exit(4);
	}

	cl_log(LOG_INFO, "Client %s unregistering", app_instance);
	rc = apphb_unregister();
	if (rc < 0) {
		cl_perror("%s apphb_unregister failure", app_instance);
		exit(5);
	}
	rc = apphb_register(app_instance, "HANGUP");
	if (rc < 0) {
		cl_perror("%s second registration failure", app_instance);
		exit(1);
	}
	/* Now we leave without further adieu -- HANGUP */
	cl_log(LOG_INFO, "Client %s HANGUP!", app_instance);
}

void 
hb_normal(int hb_intvl_ms, int delaysecs, int hb_num)
{
	int	j;
	int 	rc;
	struct 	timespec time_spec;
	char	app_name[] = "apphb_normal";
	char	app_instance[APPNAME_LEN];
	struct	timeval tmp;

	snprintf(app_instance, sizeof(app_instance)
	,	"%s_%ld", app_name, (long)getpid());
	
	if (delaysecs) {
		/* sleep randomly for a while */
		gettimeofday(&tmp, NULL);
		srandom((unsigned int)tmp.tv_usec);
		delaysecs = random() % delaysecs;
		if (delaysecs) {
			cl_log(LOG_INFO, "%s sleep randomly for %d secs"
				, app_instance, delaysecs);
			time_spec.tv_sec = delaysecs;
			time_spec.tv_nsec = 0;
			nanosleep(&time_spec, NULL);
		}
	}

	
	cl_log(LOG_INFO, "Client %s registering", app_instance);
	rc = apphb_register(app_name, app_instance);
	if (rc < 0) {
		cl_perror("%s registration failure", app_instance);
		exit(1);
	}
	if (debug) {
		cl_log(LOG_INFO, "Client %s registered", app_instance);
	}
	
	cl_log(LOG_INFO, "Client %s setting %d ms heartbeat interval"
			, app_instance, hb_intvl_ms);
	rc = apphb_setinterval(hb_intvl_ms);
	if (rc < 0) {
		cl_perror("%s setinterval failure", app_instance);
		exit(2);
	}

	/* Sleep for half of the heartbeat interval */
	time_spec.tv_sec = hb_intvl_ms / 2000;
	time_spec.tv_nsec = (hb_intvl_ms % 2000) * 500000;
	for (j=0; j < hb_num; ++j) {
		nanosleep(&time_spec, NULL);
		if(debug >= 1) 
			fprintf(stderr, "%ld:+\n", (long)getpid());
		rc = apphb_hb();
		if (rc < 0) {
			cl_perror("%s apphb_hb failure", app_instance);
			exit(3);
		}
	}
	
	cl_log(LOG_INFO, "Client %s unregistering", app_instance);
	rc = apphb_unregister();
	if (rc < 0) {
		cl_perror("%s apphb_unregister failure", app_instance);
		exit(4);
	}
	if (debug) {
		cl_log(LOG_INFO, "Client %s unregistered", app_instance);
	}
}

void 
multi_hb_test(int child_proc_num, int hb_intvl_ms, int hb_num, int delaysecs
,	int dofailuretests)
{
	int j;

	cl_log(LOG_INFO, "----Start %d client(s) with hb interval %d ms----"
			, child_proc_num, hb_intvl_ms);	
	for (j=0; j < child_proc_num; ++j) {
		switch(fork()){
		case 0:
			hb_normal(hb_intvl_ms, delaysecs ,hb_num);
			exit(0);
			break;
		case -1:
			cl_perror("Can't fork!");
			exit(1);
			break;
		default:
			/* In the parent. */
			break;
		}
	}	
	/* Wait for all our child processes to exit*/
	while(wait(NULL) > 0);
	errno = 0;
	
	if (dofailuretests) {
		cl_log(LOG_INFO, "----Start %d client(s) doing fail test----"
				, child_proc_num);
		for (j = 0; j < child_proc_num; ++j) {
			switch(fork()){
			case 0:
				doafailtest();
				exit(0);
				break;
			case -1:
				cl_perror("Can't fork!");
				exit(1);
				break;
			default:
				break;	
			}
		}
		/* Wait for all our child processes to exit*/
		while(wait(NULL) > 0);
		errno = 0;
	}
}

void
apphb_setwarn_test(int warnhb_ms, int hb_ms)
{
	/* apphb_setwarn() sets the warning period.
	 * if interval between two heartbeats is longer than the
	 * warning period, apphbd will warn: 'late heartbeat' */

	int	rc;
	struct	timespec time_spec;
	char	app_name[] = "apphb_setwarn_test";
	char	app_instance[APPNAME_LEN];

	snprintf(app_instance, sizeof(app_instance)
	,	"%s_%ld", app_name, (long)getpid());
	cl_log(LOG_INFO, "----Start test apphb_setwarn----");
	cl_log(LOG_INFO, "Client %s registering", app_instance);
	rc = apphb_register(app_name, app_instance);
	if (rc < 0) {
		cl_perror("%s register failure", app_instance);
		exit(1);
	}
	
	cl_log(LOG_INFO, "Client %s setwarn for %d ms", app_instance, warnhb_ms);
	rc = apphb_setwarn(warnhb_ms);
	if (rc < 0) {
		cl_perror("%s setwarn failure", app_instance);
		exit(3);
	}
	
	cl_log(LOG_INFO, "Client %s setinterval for %d ms", app_instance, hb_ms);
	rc = apphb_setinterval(hb_ms);
	if (rc < 0) {
		cl_perror("%s setinterval failure", app_instance);
		exit(2);
	}
	
	rc = apphb_hb();
	if (rc < 0) {
		cl_perror("%s first apphb_hb failure", app_instance);
		exit(4);
	}

	time_spec.tv_sec = hb_ms/1000;
	time_spec.tv_nsec = (hb_ms % 1000) * 1000000;
	nanosleep(&time_spec, NULL);

	rc = apphb_hb();
	if (rc < 0) {
		cl_perror("%s second apphb_hb failure", app_instance);
		exit(4);
	}

	cl_log(LOG_INFO, "Client %s unregistering", app_instance);
	rc = apphb_unregister();
	if (rc < 0) {
		cl_perror("%s apphb_unregister failure", app_instance);
		exit(5);
	}
	
	errno = 0;
}

void dup_reg_test()
{
	/* apphbd should not allow a process register two times */
	int	rc;
	char	app_instance[APPNAME_LEN];
	char	app_name[] = "dup_reg_test";

	snprintf(app_instance, sizeof(app_instance)
	,	"%s_%ld", app_name, (long)getpid());
	
	cl_log(LOG_INFO, "----Client %s trying to register twice----"
			, app_instance);
	
	cl_log(LOG_INFO, "Client %s registering", app_instance);
	rc = apphb_register(app_name, app_instance);
	if (rc < 0) {
		cl_perror("%s first register fail", app_instance);
		exit(1);
	}

	sleep(3);

	cl_log(LOG_INFO, "Client %s registering again", app_instance);
	rc = apphb_register(app_name, app_instance);
	if (rc < 0) {
		cl_perror("%s second register fail", app_instance);
		exit(1);
	}
	errno = 0;
}
