#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#define SAME 1
#define TRUE 1
#define FALSE 0
int childflag = 0;
int grandchildflag = 0;
int   pidconsistent = TRUE;
void *
grandchild_func(void * data)
{
        pid_t pid = (pid_t) data;

        if (pid ==  getpid()){
                grandchildflag = SAME;
        }

        if (grandchildflag ^ childflag){
		pidconsistent = FALSE;
                printf("Inconsistency detected\n");
        }
        return NULL;
}

void *
child_func(void * data)
{
        pid_t pid = (pid_t) data;
        pthread_t thread_id;

        if (pid ==  getpid()){
                childflag = SAME;
        }

        pthread_create(&thread_id, NULL, grandchild_func, (void*)getpid());
	
}

int
main()
{
        pthread_t thread_id;
        pthread_attr_t tattr;
        int  firsttime = 1;
	pid_t	pid;
	int	status;
	
        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

again:
	pid = fork();
	if ( pid == 0 ) { 
                childflag = 0; 
                grandchildflag =0;
                if (pthread_create(&thread_id, &tattr, child_func, (void*)getpid()) != 0){
                        printf("%s: creating thread failed", __FUNCTION__);
                }
		usleep(500000);
                if (firsttime){
                        firsttime=0;
                        goto again;
                }
		if (pidconsistent){
			return 0;
		}else{
			return 1;
		}
        }
	if (waitpid(pid, &status, 0) <= 0){
		printf("ERROR: wait for child %d failed\n",pid);
	}
	if (WIFEXITED(status)){
		return (WEXITSTATUS(status));
	}else{
		printf("child process %d does not exit normally\n",pid);
	}
	
	return 0;
}

