/* 
 * close1node2.c: Test data checkpoint function : saCkptCheckpointClose 
 *
 * Copyright (C) 2003 Wilna Wei <willna.wei@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define CkptOpenFlag SA_CKPT_CHECKPOINT_COLOCATED|SA_CKPT_CHECKPOINT_WRITE|SA_CKPT_CHECKPOINT_READ
#define CaseName "close1"
#define CkptCreationFlag SA_CKPT_WR_ACTIVE_REPLICA 
/* #define CkptCreationFlag SA_CKPT_WR_ALL_REPLICAS  */
#include "func.h"

int main(int argc, char **argv)
{
	int count =0 ;	
	/* char slavenode[50] ; */
	int i ;

	if (inittest () != 0)
		{
			finalize () ;
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1 ;
		}

	/* tell monitor machine "I'm up now"*/ 	
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_start %d\n", pidparent) ;
 	
	/* create local replica for active node */
	if (opensync () == -1)
		{
			finalize () ;
			syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
			return -1 ;
		}
	
	/* multile re-open. Check pid list in opensync call */
	for ( i=0 ; i < PIDNUMS ; i++)
		{
			if ( (pid[i] = fork ()) < 0)
				{
					finalize () ;
					syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
					return -1 ;
				}
			else if (pid[i] == 0) /* child process */
				{
						if (opensync() < 0)
							{
								syslog (LOG_INFO|LOG_LOCAL7, "ckpt_fail\n") ;
							
								/* wait for monitor machine SIGTERM signal */
								for (; ;)
									pause() ;
							}
						/* parent process will send SIGUSR1	signal */
						pause () ;

						saCkptCheckpointClose (&cphandle) ;
						saCkptFinalize (&libhandle) ;
						cphandle = -1;
						libhandle = -1 ;
						exit (0);
				}
		}

	/* wait for node 1 create and fork open ready */
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_signal %d %d\n", count++, SIGUSR1) ;
	pause () ;
		
	/* child process exits one bye one.After exiting, check that the pid 
	 * still remains in pid list
	 */
	for (i=0 ; i< PIDNUMS ; i++)
		{
			kill (pid[i], SIGUSR1) ;
			waitpid (pid[i], NULL, 0) ;

			
			pid[i] = -1 ;
		}
	
	/* wait for node 1 quiting process ready */
	syslog (LOG_INFO|LOG_LOCAL7, "ckpt_signal %d %d\n", count++, SIGUSR1) ;
	pause () ;

	finalize () ;
	return 0 ; 
}



