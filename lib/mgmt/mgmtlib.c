#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mgmt/tls.h>
#include <mgmt/mgmt.h>

#define INIT_SIZE	1024
#define INC_SIZE	512
				
#define ISCONNECTED()	(session)


int 		sock = 0;
void*		session = NULL;
malloc_t 	malloc_f = NULL;
realloc_t 	realloc_f = NULL;
free_t 		free_f = NULL;

static void* mgmt_malloc(size_t size);
static void* mgmt_realloc(void* oldval, size_t newsize);
static void mgmt_free(void *ptr);

int
mgmt_connect(const char* server, const char* user, const char*  passwd)
{
	struct sockaddr_in addr;
	char* msg;
	char* ret;
	
	/* if it has already connected, return fail */
	if (ISCONNECTED()) {
		return -1;
	}

	/* create socket */
	sock=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1 ) {
		return -1;
	}

	/* connect to server*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(server);
	addr.sin_port = htons(PORT);
	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(sock);
		return -1;
	}

	/* initialize GnuTls lib*/
	if (tls_init_client() == -1) {
		return -1;
	}

	/* bind the socket to GnuTls lib */
	session = tls_attach_client(sock);
	if (session == NULL) {
		close(sock);
		tls_close_client();
		return -1;
	}

	/* login to server */
	msg = mgmt_new_msg(MSG_LOGIN, user, passwd, NULL);
	ret = mgmt_sendmsg(msg);
	if (ret == NULL || STRNCMP_CONST(ret,MSG_OK) != 0) {
		mgmt_del_msg(msg);
		mgmt_del_msg(ret);
		close(sock);
		tls_close_client();
		return -1;
	}
	
	mgmt_del_msg(msg);
	mgmt_del_msg(ret);
	return 0;
}

char* 
mgmt_sendmsg(const char* msg)
{
	/* send the msg */
	if (-1 == mgmt_session_sendmsg(session, msg)) {
		return NULL;
	}
	/* get the result msg */
	return mgmt_session_recvmsg(session);
}

char*
mgmt_recvmsg(void)
{
	return mgmt_session_recvmsg(session);
}

int
mgmt_inputfd(void)
{
	if (!ISCONNECTED()) {
		return -1;
	}
	return sock;
}


int
mgmt_disconnect(void)
{
	if (!ISCONNECTED()) {
		return -1;
	}
	
	if (session != NULL) {
		mgmt_session_sendmsg(session, MSG_LOGOUT);
		tls_detach(session);
		session = NULL;
	}
	if (sock != 0) {
		close(sock);
		sock = 0;
	}
	tls_close_client();
	return 0;
}

char*
mgmt_new_msg(const char* type, ...)
{
	va_list ap;
	int len;
	char* buf;
	
	/* count the total len of fields */	
	len = strnlen(type, MAX_STRLEN)+1;
	va_start(ap,type);
	while(1) {
		char* arg = va_arg(ap, char*);
		if (arg == NULL) {
			break;
		}
		len += strnlen(arg, MAX_STRLEN)+1;
	}
	va_end(ap);
	
	/* alloc memory */
	buf = (char*)mgmt_malloc(len+1);
	if (buf == NULL) {
		return NULL;
	}

	/* assign the first field */
	snprintf(buf,len,"%s", type);
	
	/* then the others */
	va_start(ap, type);
	while(1) {
		char* arg = va_arg(ap, char*);
		if (arg == NULL) {
			break;
		}
		strncat(buf, "\n", len);
		strncat(buf, arg, len);
	}
	va_end(ap);
	
	return buf;
}
char*
mgmt_msg_append(char* msg, const char* append)
{
	int msg_len;
	int append_len;
	int len;

	msg_len = strnlen(msg, MAX_MSGLEN);
	append_len = strnlen(append, MAX_STRLEN);
	/* +2: one is the '\n', other is the end 0*/
	len = msg_len+append_len+2;
	msg = (char*)mgmt_realloc(msg, len);
	strncat(msg, "\n", len);
	strncat(msg, append, len);
	return msg;
}
char**
mgmt_msg_args(const char* msg, int* num)
{
	char* p;
	char* buf;
	char** ret = NULL;
	int i,n;
	int len;
	
	if (msg == NULL) {
		return NULL;
	}
	
	/* alloc memory */
	len = strnlen(msg, MAX_MSGLEN);
	buf = (char*)mgmt_malloc(len+1);
	if (buf == NULL) {
		return NULL;
	}
	
	strncpy(buf, msg, len);
	buf[len] = 0;
	
	/* find out how many fields first */
	p = buf;
	n = 1;
	while(1) {
		p=strchr(p,'\n');
		if (p != NULL) {
			p++;
			n++;
		}
		else {
			break;
		}
	}

	/* malloc the array for args */
	ret = (char**)mgmt_malloc(sizeof(char*)*n);
	if (ret == NULL) {
		mgmt_free(p);
		return NULL;
	}

	/* splite the string to fields */
	ret[0] = buf;
	for (i = 1; i < n; i++) {
		ret[i] = strchr(ret[i-1],'\n');
		*ret[i] = 0;
		ret[i]++;
	}
	if (num != NULL) {
		*num = n;
	}
	return ret;
}

void
mgmt_del_msg(char* msg)
{
	if (msg != NULL) {
		mgmt_free(msg);
	}
}
void
mgmt_del_args(char** args)
{
	if (args != NULL) {
		if (args[0] != NULL) {
			mgmt_free(args[0]);
		}
		mgmt_free(args);
	}
}

int 
mgmt_session_sendmsg(void* session, const char* msg)
{
	int len;
	if (session == NULL) {
		return -1;
	}
	/* send the msg, with the last zero */
	len = strnlen(msg, MAX_MSGLEN)+1;
	if (len != tls_send(session, msg, len)) {
		return -1;
	}
	/* get the bytes sent */
	return len;
}

char*
mgmt_session_recvmsg(void* session)
{
	char c;
	int cur = 0;
	int len = 0;
	char* buf = NULL;
	if (session == NULL) {
		return NULL;
	}

	while(1) {
		int rd = tls_recv(session, &c, 1);
		if (rd == 0 && buf == NULL) {
			/* no msg or something wrong */
			return NULL;
		}
		if (rd == 1) {
			/* read one char */
			if (buf == NULL) {
				/* malloc buf */
				buf = (char*)mgmt_malloc(INIT_SIZE);
				len = INIT_SIZE;
			}
			if (buf == NULL) {
				return NULL;
			}
			/* the buf is full, enlarge it */
			if (cur == len) {
				buf = mgmt_realloc(buf, len+INC_SIZE);
				if (buf == NULL) {
					return NULL;
				}
				len += INC_SIZE;
			}
			
			buf[cur] = c;
			cur++;
			if (c == 0) {
				return buf;
			}
		}
		/* something wrong */
		if (rd == -1) {
			if(errno == EINTR) {
				continue;
			}
			mgmt_free(buf);
			return NULL;
		}
	}
	return NULL;
}

void
mgmt_set_mem_funcs(malloc_t m, realloc_t r, free_t f)
{
	malloc_f = m;
	realloc_f = r;
	free_f = f;
}

void*
mgmt_malloc(size_t size)
{
	if (malloc_f == NULL) {
		return malloc(size);
	}
	return (*malloc_f)(size);
}

void*
mgmt_realloc(void* oldval, size_t newsize)
{
	if (realloc_f == NULL) {
		return realloc(oldval, newsize);
	}
	return (*realloc_f)(oldval, newsize);
}

void
mgmt_free(void *ptr)
{
	if (free_f == NULL) {
		free(ptr);
		return;
	}
	(*free_f)(ptr);
}
