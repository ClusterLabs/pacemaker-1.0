/* $Id: hb_proc.h,v 1.18 2005/07/16 15:04:58 alan Exp $ */
/*
 * hb_proc.h: definitions of heartbeat child process info
 *
 * These are the things that let us manage our child processes well.
 *
 * Copyright (C) 2001 Alan Robertson <alanr@unix.sh>
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

#ifndef _HB_PROC_H
#	define _HB_PROC_H 1

#include <clplumbing/cl_malloc.h>
#include <ha_msg.h>
#include <clplumbing/longclock.h>
#include <heartbeat.h>

enum process_type {
	PROC_UNDEF=0,		/* OOPS! ;-) */
	PROC_MST_CONTROL,	/* Master control process */
	PROC_HBREAD,		/* Read process */
	PROC_HBWRITE,		/* Write process */
	PROC_HBFIFO,		/* FIFO process */
	PROC_PPP		/* (Obsolete) PPP process */
};

enum process_status { 
	FORKED=1,	/* This process is forked, but not yet really running */
	RUNNING=2	/* This process is fully active, and open for business*/
};


struct process_info {
	enum process_type	type;		/* Type of process */
	enum process_status	pstat;		/* Is it running yet? */
	pid_t			pid;		/* Process' PID */
	hb_msg_stats_t		msgstats;
	cl_mem_stats_t		memstats;
};


struct pstat_shm {
	int	nprocs;
	int	restart_after_shutdown;
	int	giveup_resources;
	int	i_hold_resources;
	struct process_info info [MAXPROCS];
};

/* These are volatile because they're in shared memory */
volatile extern struct pstat_shm *	procinfo;
volatile extern struct process_info *	curproc;

#endif /*_HB_PROC_H*/
