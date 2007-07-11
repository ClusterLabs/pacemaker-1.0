/*
 *
 * multi-clients NFS lock test code
 *
 * Copyright (C) 2004 Guochun Shi<gshi@ncsa.uiuc.edu>
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


#include <lha_internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <signal.h>

#define MYPORT   5000
#define PASS 0
#define WARNING 1
#define FATAL 2
#define END 0

#define output printf

struct test_task{	
	int num;
	int sec;
	int func;
	off_t offset;
	off_t length;
	int pass;
	int fail;	
};

#define MAX_FN_LEN 32

struct global_info{
	char filename[MAX_FN_LEN]; 
	int clientID;
	int sockfd;	
	int fd;
	int fd_save;
	int testpass;
	int testwarn;
	int testfail;
	pid_t forkpid; 
};


enum{
	OPEN,
	MANOPEN,
	CLOSE,
	CLOSE_SAVEFD,
	WAIT,
	SEND,
	RUN,
	READ,
	WRITE,
	SLEEP,
	PUSHFD,
	POPFD,
	TRUNC,
	FORK,
	KILL,
	OUTPUT
};

struct run_param{
	int num;
	int sec;
	int func;
	off_t offset;
	off_t length;
	int pass;
	int fail;
};

struct io_param{
	int num;
	int sec;
	int start;
	int whichmsg;
};
struct sleep_param{
	int time;
};
struct task{
	int op;
	struct run_param run_param;
	struct io_param io_param;
	struct sleep_param sleep_param;	
};

	
struct global_info gi;


static void 
pushfd(void)
{
	if ((gi.fd_save = dup(gi.fd)) == -1 ){
		perror("dup");
		exit(1);
	}
	
	return;
	
}
static void
popfd(void)
{
	if (dup2(gi.fd_save, gi.fd) == -1){
		perror("dup2");
		exit(1);
	}
	
	return;
	
}
static void
close_savefd(void)
{
	if (close(gi.fd_save) == -1){
		perror("close");
		exit(1);
	}
	return;
}

static void
hb_trunc(void )
{
	if (ftruncate(gi.fd, 0) == -1){
		perror("ftruncate");
		exit(1);
	}
	
	return;
	
}

static int
report (int result, struct test_task *tt)
{
	
	if (tt->pass == result){
		output("PASSED.\n");
		gi.testpass++;
	} 
	else if ((tt->pass == EAGAIN && result == EACCES)
		 || (tt->pass == EACCES && result == EAGAIN)
		 || tt->fail == WARNING){
		output("warning\n");
		gi.testwarn++;
	}
	else {
		output("test failed, result =%d\n", result);
		gi.testfail++;
		exit(1);
	}	
	
	return 0;
}

static void 
test_exit(void )
{
	if( gi.testwarn == 0 && gi.testfail == 0 ){
		output("All tests finished successfully!\n");
		exit(1);
	}
	else {
		output(" ********There are %d warnings, %d errors******", 
		       gi.testwarn, gi.testfail);
		exit(1);
	}
}


static void
manopen_testfile(void)
{
	
	if ((gi.fd = open(gi.filename,O_RDWR|O_CREAT, 02666)) == -1){
		perror("open");
		exit(1);
	}
	
	return ;
}


static void
open_testfile(void)
{
	
	if ((gi.fd = open(gi.filename,O_RDWR|O_CREAT, 0666)) == -1){
		perror("open");
		exit(1);
	}
	
	return ;
}

static void 
close_testfile(void)
{
	if( close(gi.fd) == -1){
		perror("close");
		exit(1);
	}
	/* unlink(gi.filename); */
	return;
}

#define MSGONE 1
#define MSGTWO 2 
#define MSGA "abcde"
#define MSGB "edcba"
#define WR_START 4*1024 - 4
#define LOCKLEN 6 
static void  
write_testfile(struct task* task)
{
	
	const char* msg;
	int result;

	output("%d-%d:\t", task->io_param.num, task->io_param.sec);
	
	if (task->io_param.whichmsg == 1){
		msg = MSGA;
	} else {
		msg = MSGB;
	}
	
	if (lseek(gi.fd, task->io_param.start, SEEK_SET) == -1){
		perror("lseek");
		exit(1);
	}
	if ((result = write(gi.fd, msg, LOCKLEN)) == -1){
		perror("write");
		exit(1);
	}
	
	output("PASSED.\n");
}

static void 
read_testfile( struct task* task)
{
	
	const char* msg;
	char buf[LOCKLEN];
	
	output("%d-%d:\t", task->io_param.num, task->io_param.sec);
	
	if (task->io_param.whichmsg == 1){
		msg = MSGA;
	} else {
		msg = MSGB;
	}
	
	if (lseek(gi.fd, task->io_param.start, SEEK_SET) == -1){
		perror("lseek");
		exit(1);
	}	
	
	if (read(gi.fd, buf, LOCKLEN) == -1){
		perror("read");
		exit(1);
	}
	
	if(memcmp(buf, msg, LOCKLEN) != 0){
		output("\nread content is not matched\n");
		output("conent in file is =%s,\n"
		       "the msg compared with is %s\n", buf, msg);
		exit(1);
	}
	
	output("PASSED.\n");
}

static int
do_test( struct test_task* tt)
{
	int result = PASS;
	struct flock flock;
	int cmd;
	
	
	flock.l_whence = SEEK_SET;
	flock.l_start = tt->offset;
	flock.l_len = tt->length;
	flock.l_type = F_WRLCK;
	switch(tt->func){
		
	case F_TEST:
		cmd = F_GETLK;
		break;
	case F_TLOCK:
		cmd = F_SETLK;
		break;
	case F_LOCK:
		cmd = F_SETLKW;		
		break;
	case F_ULOCK:
		flock.l_type = F_UNLCK;
		cmd = F_SETLK;
		break;
	default:
		output("wrong func in task! \n");
		exit(1);
	}
	output("%d-%d:\t", tt->num, tt->sec);
	
	if(lseek(gi.fd, tt->offset, 0) < 0) {
		result = errno;
	}
	
	if (result == 0) {
		/* if (result = lockf(gi.fd, tt->func, tt->length) != 0){ */
		if ((result = fcntl(gi.fd, cmd, &flock)) != 0 ){
			result = errno;
		}else if ( cmd == F_GETLK && flock.l_type != F_UNLCK){
			result = EACCES;
		}
		
		
		
	}
	
	return report(result, tt);
}


static int
test(int num, int sec, int func, off_t offset,
     off_t length, int pass, int fail){
  
	struct test_task tt ;
	tt.num = num;
	tt.sec = sec;
	tt.func = func;
	tt.offset = offset;
	tt.length = length;
	tt.pass = pass;
	tt.fail = fail;
	return do_test(&tt);	
}

static int 
waitnotice(void)     
{
	int numbytes;
	char buf;
	
	if ((numbytes = recv(gi.sockfd, &buf, 1, 0)) == -1){
		perror("recv");
		close(gi.sockfd);
		exit(1);
	} else if (numbytes == 0){
		output("socket broken\n");
		exit(1);
	}
	
	return 0;
}

static int
sendnotice(void)
{
	int numbytes;
	char buf;
	
	if ((numbytes = send(gi.sockfd, &buf, 1, 0)) == -1){
		perror("send");
		close(gi.sockfd);
		exit(1);
	}
	
	return 0;
	
}
#if 0

static int 
send_msg(void* msg, int len)
{	
	if (send(gi.sockfd, msg, len, 0) != len){
		perror("send");
		close(gi.sockfd);
		exit(1);
	}
	
	return 0;
}


static int
recv_msg(void* buf, int len)
{
	int numbytes = 0;
	
	while (len > 0 ){
		
		if (( numbytes = recv(gi.sockfd, buf, len, 0)) == -1){		
			perror("recv");
			close(gi.sockfd);
			exit(1);
		}
		len -= numbytes;
	}
	
	return 0;
}

#endif

static void
init_comm(const char* servername)
{
	struct sockaddr_in their_addr;
	socklen_t sin_size;
	
	if (gi.clientID == 0 ){
		
		int sockfd;
		struct sockaddr_in my_addr;
		int yes = 1;
		
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(1);
		}
		
		
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons(MYPORT);
		my_addr.sin_addr.s_addr = INADDR_ANY;
		memset(&(my_addr.sin_zero), '\0', 8);
		
		if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		
		if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))
		    == -1){
			perror("bind");
			exit(1);
		}
		
		if (listen(sockfd, 10) == -1){
			perror("listen");
			exit(1);
		}
		
		sin_size = sizeof(their_addr);
		if(( gi.sockfd = accept(sockfd, (struct sockaddr *)&their_addr, 
					 (socklen_t *)&sin_size)) == -1) {
			perror("accept");
			exit(1);
		}
		
		close(sockfd);
		
	} else{
		struct hostent *he;
		
		if (!servername){
			printf("servername is NULL\n");
			exit(1);

		}
		if ((he=gethostbyname(servername)) == NULL){
			output("gethostbyname: Error, servername =%s \n", servername);
			exit(1);
		}
		
		if(( gi.sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(1);
		}

		their_addr.sin_family = AF_INET;
		their_addr.sin_port = htons(MYPORT);
		memcpy(&their_addr.sin_addr, he->h_addr, sizeof(struct in_addr));
		memset(&(their_addr.sin_zero), '\0', 8);
		
		if(connect(gi.sockfd, (struct sockaddr *) &their_addr,
			   sizeof(struct sockaddr)) == -1){
			perror("connect");
			exit(1);
		}				
	}
}



static GSList * 
generate_task_list(void)
{
	GSList* task_list = NULL;
	size_t i;
	
  	int  task_table[][9] =
		{
			/*test 1:
			  testing lock function in only one machine*/
			{0, OPEN},
			{0, WAIT},
			{0, RUN, 1, 1, F_TEST, 0, 1, PASS},
			{0, RUN, 1, 2, F_TEST, 0, END, PASS},
			{0, RUN, 1, 3, F_TEST, 1, 1, PASS},
			{0, RUN, 1, 4, F_TEST, 1, END, PASS},
			{0, SEND},
			{0, CLOSE},
			
			{1, SEND},
			{1, WAIT},

			/*test 2: 
			  node 1 locks the whole file
			  node 2 tries to lock different regions of 
			  the same file*/

			{0, OPEN},
			{0 ,WAIT},
			{0, RUN, 2, 0, F_TLOCK, 0, END, PASS},
			{0, SEND},
			{0, WAIT},
			{0, CLOSE},

			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 2, 1, F_TEST, 0, 1, EACCES},
			{1, RUN, 2, 2, F_TEST, 0, END, EACCES},
			{1, RUN, 2, 3, F_TEST, 1, 1, EACCES},
			{1, RUN, 2, 4, F_TEST, 1, END, EACCES},
			{1, SEND},
			{1, CLOSE},

			/* test 3:
			   node 1 locks the 1st byte.
			   node 2 tries to lock different regions*/

			{0, OPEN},
			{0, WAIT},
			{0, RUN, 3, 0, F_TLOCK, 0, 1, PASS},
			{0, SEND},
			{0, WAIT},
			{0, RUN, 3, 5, F_ULOCK, 0, 1, PASS},
			{0, CLOSE},
			
			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 3, 1, F_TEST, 0, 1, EACCES},
			{1, RUN, 3, 2, F_TEST, 0, END, EACCES},
			{1, RUN, 3, 3, F_TEST, 1, 1, PASS},
			{1, RUN, 3, 4, F_TEST, 1, END, PASS},
			{1, SEND},
			{1, CLOSE},
			
			/* test 4:
			   node 1 locks the second byte
			   node 2 tries to lock different regions
			*/
			
			{0, OPEN},
			{0, WAIT},
			{0, RUN, 4, 0, F_TLOCK, 1, 1, PASS},
			{0, SEND},
			{0, WAIT},
			{0, RUN, 4, 10, F_ULOCK, 1, 1, PASS},
			{0, CLOSE},
			
			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 4, 1, F_TEST, 0, 1, PASS},
			{1, RUN, 4, 2, F_TEST, 0, 2, EACCES},
			{1, RUN, 4, 3, F_TEST, 0, END, EACCES},
			{1, RUN, 4, 4, F_TEST, 1, 1, EACCES},
			{1, RUN, 4, 5, F_TEST, 1, 2, EACCES},
			{1, RUN, 4, 6, F_TEST, 1, END, EACCES},
			{1, RUN, 4, 7, F_TEST, 2, 1, PASS},
			{1, RUN, 4, 8, F_TEST, 2, 2, PASS},
			{1, RUN, 4, 9, F_TEST, 2, END, PASS},
			{1, SEND},
			{1, CLOSE},
			
			/* test 5:
			   node 1 locks the 1st and 3rd bytes, 
			   node 2 tries to lock different regions
			*/

			{0, OPEN},
			{0, WAIT},
			{0, RUN, 5, 0, F_TLOCK, 0, 1, PASS},
			{0, RUN, 5, 1, F_TLOCK, 2, 1, PASS},
			{0, SEND},
			{0, WAIT},
			{0, RUN, 5, 14, F_ULOCK, 0, 1, PASS},
			{0, RUN, 5, 15, F_ULOCK, 2, 1, PASS},
			{0, CLOSE},
			
			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 5, 2, F_TEST, 0, 1, EACCES},
			{1, RUN, 5, 3, F_TEST, 0, 2, EACCES},
			{1, RUN, 5, 4, F_TEST, 0, END, EACCES},
			{1, RUN, 5, 5, F_TEST, 1, 1, PASS},
			{1, RUN, 5, 6, F_TEST, 1, 2, EACCES},
			{1, RUN, 5, 7, F_TEST, 1, END, EACCES},
			{1, RUN, 5, 8, F_TEST, 2, 1, EACCES},
			{1, RUN, 5, 9, F_TEST, 2, 2, EACCES},
			{1, RUN, 5, 10, F_TEST, 2, END, EACCES},
			{1, RUN, 5, 11, F_TEST, 3, 1, PASS},
			{1, RUN, 5, 12, F_TEST, 3, 2, PASS},
			{1, RUN, 5, 13, F_TEST, 3, END, PASS},
			{1, SEND},
			{1, CLOSE},

			/*test 6 : about maxof , ignored now
			 */


			/*test 7: test nodes' mutual exclusion.
			 */

			{0, OPEN},
			{0, WAIT},
			{0, RUN, 7, 0, F_TLOCK, WR_START, LOCKLEN, PASS},
			{0, WRITE, 7, 1, WR_START, MSGONE},
			{0, SEND},
			{0, READ, 7, 2, WR_START, MSGONE},
			{0, RUN, 7, 3, F_ULOCK, WR_START, LOCKLEN, PASS},
			{0, WAIT},
			{0, RUN, 7, 7, F_LOCK, WR_START, LOCKLEN, PASS},
			{0, READ, 7, 8, WR_START, MSGTWO},
			{0, RUN, 7, 9, F_ULOCK, WR_START, LOCKLEN, PASS},
			{0, SEND},
			{0, CLOSE},
			

			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 7, 4, F_LOCK, WR_START, LOCKLEN, PASS},
			{1, SEND},
			{1, WRITE, 7, 5, WR_START, MSGTWO},
			{1, RUN, 7, 6, F_ULOCK, WR_START, LOCKLEN, PASS},
			{1, WAIT},
			{1, CLOSE},

			/*test 8: rate test, ignored now
			 */


			/*test 9: Test mandatory locking.
			  FIXME: this testing cannot work yet
			 */
#if 0
			{0, MANOPEN},
			{0, SLEEP, 20},
			{0, WAIT},
			{0, RUN,   9, 0, F_TLOCK, 0, LOCKLEN, PASS},
			{0, WRITE, 9, 1, 0, MSGONE},
			{0, SEND},
			{0, READ,  9, 2, 0, MSGONE},
			{0, RUN,   9, 3, F_ULOCK, 0, LOCKLEN, PASS},
			{0, WAIT},
			{0, READ,  9, 5, 0, MSGTWO},
			{0, SEND},
			{0, CLOSE},
			
			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 9, 4, F_TEST, 0, LOCKLEN, EACCES},
			{1, WRITE, 9, 4, 0, MSGTWO},
			{1, SEND},
			{1, WAIT},
			{1, CLOSE},
#endif 
			/* test 10:
			   Make sure a locked region is split properly
			*/
			
			{0, OPEN},
			{0, WAIT},
			{0, RUN, 10, 0, F_TLOCK, 0, 3, PASS},
			{0, RUN, 10, 1, F_ULOCK, 1, 1, PASS},
			{0, SEND},
			{0, WAIT},
			{0, RUN, 10, 6, F_ULOCK, 0, 1, PASS},
			{0, RUN, 10, 7, F_ULOCK, 2, 1, PASS},
			{0, SEND},
			{0, WAIT},
			/* {0, RUN, 10, 9, F_ULOCK, 0, 1, PASS}, */
			{0, RUN, 10, 10, F_TLOCK, 1, 3, PASS},
			{0, RUN, 10, 11, F_ULOCK, 2, 1, PASS},
			{0, SEND},
			{0, WAIT},
			{0, CLOSE},
			
			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 10, 2, F_TEST, 0, 1, EACCES},
			{1, RUN, 10, 3, F_TEST, 2, 1, EACCES},
			{1, RUN, 10, 4, F_TEST, 3, END, PASS},
			{1, RUN, 10, 5, F_TEST, 1, 1, PASS},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 10, 8, F_TEST, 0, 3, PASS},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 10, 12, F_TEST, 1, 1, EACCES},
			{1, RUN, 10, 13, F_TEST, 3, 1, EACCES},
			{1, RUN, 10, 14, F_TEST, 4, END, PASS},
			{1, RUN, 10, 15, F_TEST, 2, 1, PASS},
			{1, RUN, 10, 16, F_TEST, 0, 1, PASS},
			{1, SEND},
			{1, CLOSE},

			/* test 11:
			   make sure close() releases the process's locks
			*/
			{0, OPEN},
			{0, WAIT},
			{0, PUSHFD},
			{0, RUN, 11, 0, F_TLOCK, 0, 0, PASS},
			{0, CLOSE},
			{0, SEND},

			{0, WAIT},
			{0, POPFD},
			{0, RUN, 11, 3, F_TLOCK, 29, 1463, PASS},
			{0, RUN, 11, 4, F_TLOCK, 0X2000, 87, PASS},
			{0, CLOSE},
			{0, SEND},
		

			{0, WAIT},
			{0, POPFD},
			{0, WRITE, 11, 7, 0, MSGONE},
			{0, RUN, 11, 8, F_TLOCK, 0, 0, PASS},
			{0, CLOSE},
			{0, SEND},
			
			{0, WAIT},
			{0, POPFD},
			{0, WRITE, 11, 11, 0, MSGTWO},
			{0, RUN, 11, 12, F_TLOCK, 0, 0, PASS},
			{0, TRUNC},
			{0, CLOSE},
			{0, SEND},
			{0, WAIT},
			{0, CLOSE_SAVEFD},

			{1, OPEN},
			{1, SEND},
			{1, WAIT},
			{1, RUN, 11, 1, F_TLOCK, 0, 0, PASS},
			{1, RUN, 11, 2, F_ULOCK, 0, 0, PASS},			
			{1, SEND},
			
			{1, WAIT},
			{1, RUN, 11, 5, F_TLOCK, 0, 0, PASS},
			{1, RUN, 11, 6, F_ULOCK, 0, 0, PASS},
			{1, SEND},
			
			{1, WAIT},
			{1, RUN, 11, 9, F_TLOCK, 0, 0, PASS},
			{1, RUN, 11, 10, F_ULOCK, 0, 0, PASS},
			{1, SEND},
			
			{1, WAIT},
			{1, RUN, 11, 13, F_TLOCK, 0, 0, PASS},
			{1, RUN, 11, 14, F_ULOCK, 0, 0, PASS},
			{1, SEND},
			{1, CLOSE},
			
			/* test 12:
			   Signalled process should release locks.
			*/
			
	  		{0, OPEN},
			{0, WAIT},
			{0, SLEEP, 1},
			{0, RUN, 12, 1, F_TLOCK, 0, 0, PASS},
			{0, SEND},
			{0, CLOSE},
			
			{1, OPEN},
			{1, FORK, RUN, 12, 0, F_TLOCK, 0, 0, PASS},
			{1, SLEEP, 1},
			{1, KILL},
			{1, SEND},
			{1, WAIT},
			{1, CLOSE},
			
			
			
		};
	
	for (i = 0; i < sizeof(task_table)/(sizeof(task_table[0])); i++ ){
		if ( gi.clientID == task_table[i][0]){
			struct task* task  = g_malloc( sizeof(struct task));
			int j = 2;

			task->op = task_table[i][1];			
			
			switch(task->op){
		

			case WRITE:
			case READ:
				task->io_param.num = task_table[i][j++];
				task->io_param.sec = task_table[i][j++];	
				task->io_param.start = task_table[i][j++];
				task->io_param.whichmsg = task_table[i][j++];
				break;
				
			case SLEEP:
				task->sleep_param.time = task_table[i][j++];
				break;
				
			case FORK:
				j++;
				/* fall through */
			case RUN:
				task->run_param.num = task_table[i][j++];
				task->run_param.sec = task_table[i][j++];
				task->run_param.func = task_table[i][j++];
				task->run_param.offset = task_table[i][j++];				     
				task->run_param.length = task_table[i][j++];
				task->run_param.pass = task_table[i][j++];				     
				break;						
				
			}
			task_list = g_slist_append(task_list, task);
		}
	}     		
	return task_list;
}


static void
remove_task_list(GSList* task_list)
{
	size_t i;
	
	for (i = 0; i < g_slist_length(task_list); i++){
		g_free(g_slist_nth_data(task_list, i));
	}
	
	g_slist_free(task_list);
	
}

static void
runtests(GSList* task_list)
{
	size_t i;
	
	for (i = 0; i < g_slist_length(task_list); i++){
		struct task* task; 
		
		task =  g_slist_nth_data(task_list, i);
		switch(task->op){

		case OPEN:
			open_testfile();
			break;
		case MANOPEN:
			manopen_testfile();
			break;
		case CLOSE:
			close_testfile();
			break;
		case CLOSE_SAVEFD:
			close_savefd();
			break;
		case WAIT:
			waitnotice();
			break;
		case SEND:
			sendnotice();
			break;
		case WRITE:
			write_testfile(task);			
			break;
		case READ:
			read_testfile(task);			
			break;
		case RUN:
			test( task->run_param.num,
			      task->run_param.sec,
			      task->run_param.func,
			      task->run_param.offset,
			      task->run_param.length,
			      task->run_param.pass, 
			      FATAL);
			break;
		case FORK:{
			int subpid = fork();
			if (subpid < 0){
				perror("can't fork off child");
				exit(1);
			}
			if (subpid == 0){
				test( task->run_param.num,
				      task->run_param.sec,
				      task->run_param.func,
				      task->run_param.offset,
				      task->run_param.length,
				      task->run_param.pass, 
				      FATAL);
				while(1) {
					sleep(1);
				}
			}else{
				gi.forkpid = subpid;
			}
			break;
		}
		case KILL:
			if(kill(gi.forkpid, SIGINT) == -1){
				perror("kill");
				exit(1);

			}
			break;
		case SLEEP:
			sleep(task->sleep_param.time);
			break;
		case PUSHFD:
			pushfd();
			break;
		case POPFD:
			popfd();
			break;
		case TRUNC:
			hb_trunc();
			break;
		}
		
	}
	
}

static void
usage(const char* pgm)
{
 	output("Usage: this test need to run in two machines, the filesname and num have to be the same in both nodes\n");
	output("node1:\n %s [-N num] <filename>\n", pgm);
	output("node2:\n %s [-N num] <filename> <node1_hostname>\n", pgm);
	return;
}

int
main (int argc, char**  argv)
{
	char* servername = NULL;
	char* filename = NULL;
	GSList* task_list = NULL;
	int option;
	int num_ites;
	int i;

	extern char *optarg;

	/*default number of phase is 1*/
	num_ites = 1; 
	while ((option = getopt(argc, argv, "N:h")) != -1){
		switch(option){
		case 'N':
			if (sscanf(optarg, "%d", &num_ites) <= 0) {
				usage(argv[0]);
				exit(1);
			}
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(1);
		}
	}
	
	gi.clientID = 0;
	for (i = optind ; i <  argc; i++)
		{
			if (filename == NULL){
                                filename = argv[i];
			} else {
	                        servername = argv[i];
                                gi.clientID = 1;
			}
		}
	if (filename == NULL){
		usage(argv[0]);
		exit(1);
	}
	gi.testpass = 0;
	gi.testwarn = 0;
	gi.testfail = 0;
	
	strlcpy(gi.filename, filename, sizeof(gi.filename));
	
	init_comm(servername);
	
	for (i = 0; i < num_ites; i++){
		
		output("Iteration %d: \n", i);
		task_list = generate_task_list();
		
		runtests(task_list);
		
		remove_task_list(task_list);
	}
	
	test_exit();

	return 0;
}

