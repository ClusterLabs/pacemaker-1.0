/* quorumd.c: quorum module
 * policy --- connect to quorumd for asking whether we have quorum.
 *
 * Copyright (C) 2006 Huang Zhen
 *
 * SECURITY NOTE:  It would be very easy for someone to masquerade as the
 * device that you're pinging.  If they don't know the password, all they can
 * do is echo back the packets that you're sending out, or send out old ones.
 * This does mean that if you're using such an approach, that someone could
 * make you think you have quorum when you don't during a cluster partition.
 * The danger in that seems small, but you never know ;-)
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


#include <portability.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <pils/plugin.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_quorum.h>



#define PIL_PLUGINTYPE          HB_QUORUM_TYPE
#define PIL_PLUGINTYPE_S        HB_QUORUM_TYPE_S
#define PIL_PLUGIN              quorumd
#define PIL_PLUGIN_S            "quorumd"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL

static struct hb_quorum_fns Ops;

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
     
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate = NULL;

#define LOG	PluginImports->log
#define MALLOC	PluginImports->alloc
#define STRDUP  PluginImports->mstrdup
#define FREE	PluginImports->mfree

#define DH_BITS 1024
#define MAX_BUF 1024
#define CACERT HB_HA_DIR"/ca-cert.pem"
#define CLIENTKEY HB_HA_DIR"/client-key.pem"
#define CLIENTCERT HB_HA_DIR"/client-cert.pem"

static int verify_certificate (gnutls_session session);
static gnutls_session initialize_tls_session (int sd);
static void initialize_tls_global(void);
static gboolean query_quorum(gpointer data);
static gboolean connect_quorum_server(gpointer data);

static void quorumd_stop(void);
static gnutls_certificate_credentials xcred;


PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports);

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports)
{
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;

	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);  

	/*  Register our interface implementation */
 	return imports->register_interface(us, PIL_PLUGINTYPE_S,
					   PIL_PLUGIN_S,
					   &Ops,
					   NULL,
					   &OurInterface,
					   (void*)&OurImports,
					   interfprivate); 
}

static int sock = 0;
static gnutls_session session = NULL;
static guint repeat_timer = 0;
static int nodenum = 0;
static int weight = 0;
static int cur_quorum = -1;
static callback_t callback = NULL;
static const char* cluster = NULL;
static const char* quorum_server = NULL;
static int interval = 0;
static int
quorumd_getquorum(const char* cluster
,		int member_count, int member_quorum_votes
,		int total_node_count, int total_quorum_votes)
{
	cl_log(LOG_DEBUG, "quorum plugin: quorumd");
 	cl_log(LOG_DEBUG, "cluster:%s, member_count=%d, member_quorum_votes=%d",
 	       cluster, member_count, member_quorum_votes);
 	cl_log(LOG_DEBUG, "total_node_count=%d, total_quorum_votes=%d",
 	       total_node_count, total_quorum_votes);

	nodenum = member_count;
	weight = member_quorum_votes;
	if (cur_quorum == -1) {
		connect_quorum_server(NULL);
	}
	cl_log(LOG_DEBUG,"zhenh: return cur_quorum  %d\n", cur_quorum);
	
	return cur_quorum==1? QUORUM_YES:QUORUM_NO;
}
static int
quorumd_init(callback_t notify, const char* cl_name, const char* qs_name)
{
	cl_log(LOG_DEBUG, "quorum plugin: quorumd, quorumd_init()");
	cl_log(LOG_DEBUG, "quorum plugin: cluster:%s, quorum_server:%s", cl_name, qs_name);
	callback = notify;
	cluster = cl_name;
	quorum_server = qs_name;
	return 0;
}
static void
quorumd_stop(void)
{
	cl_log(LOG_DEBUG, "quorum plugin: quorumd, quorumd_stop()");
	if (repeat_timer != 0) {
		g_source_remove(repeat_timer);
		repeat_timer = 0;
	}
	if (session != NULL) {
		gnutls_bye (session, GNUTLS_SHUT_WR);
		gnutls_deinit (session);
		close(sock);
		session = NULL;
	}
	cur_quorum = -1;
}
static struct hb_quorum_fns Ops ={
	quorumd_getquorum,
	quorumd_init,
	quorumd_stop
};
gboolean
connect_quorum_server(gpointer data)
{
	struct sockaddr_in addr;
	struct ha_msg* msg = NULL;
	struct ha_msg* ret = NULL;
	const char* version = "2_0_8";
	struct hostent* hp;
	int quorum;
	size_t	len;
	char* s = NULL;
	char buf[MAXMSG];
	
	cl_log(LOG_DEBUG, "quorum plugin: quorumd, connect_quorum_server");
	/* initialize gnutls */
	initialize_tls_global();

	/* create socket */
	sock=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1 ) {
		return FALSE;
	}

	/* connect to server*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	hp = gethostbyname(quorum_server);
	if (hp == NULL) {
		return FALSE;
	}
	memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));
	addr.sin_port = htons(5561);
	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		return FALSE;
	}
	session = initialize_tls_session(sock);
	if (session == NULL) {
		close(sock);
		session = NULL;
		return FALSE;
	}
	/* send the version */
	gnutls_record_send(session, version, strlen(version)+1);

	/* send initialize msg */
	msg = ha_msg_new(10);
	ha_msg_add(msg, "t","init");
	ha_msg_add(msg, "cl_name", cluster);

	s  = msg2wirefmt(msg, &len);
	gnutls_record_send(session, s, len);
	cl_free(s);
	len = gnutls_record_recv(session, buf, MAXMSG);
	if ((ssize_t)len <=0) {
		close(sock);
		session = NULL;
		return FALSE;
	}
	ret = wirefmt2msg(buf, len, FALSE);
	if (STRNCMP_CONST(ha_msg_value(ret, "result"), "ok") != 0) {
		close(sock);
		session = NULL;
		return FALSE;
	}
	if (ha_msg_value_int(ret, "interval", &interval)!= HA_OK) {
		close(sock);
		session = NULL;
		return FALSE;
	}
	ha_msg_del(ret);
	ha_msg_del(msg);

	/* send quorum query msg */
	msg = ha_msg_new(10);
	ha_msg_add(msg, "t","quorum");
	ha_msg_add_int(msg, "nodenum", nodenum);
	ha_msg_add_int(msg, "weight", weight);

	s  = msg2wirefmt(msg, &len);
	gnutls_record_send(session, s, len);
	cl_free(s);
	len = gnutls_record_recv(session, buf, MAXMSG);
	ret = wirefmt2msg(buf, len, FALSE);
	ha_msg_value_int(ret, "quorum", &quorum);
	LOG(LOG_DEBUG,"quorum:%d\n", quorum);
	cur_quorum = quorum;
	
	ha_msg_del(ret);
	ha_msg_del(msg);

	/* set the repeatly query */
	repeat_timer = g_timeout_add(interval, query_quorum, NULL);
	return FALSE;
}
gboolean
query_quorum(gpointer data)
{
	int quorum;
	size_t	len;
	char* s = NULL;
	char buf[MAXMSG];
	struct ha_msg* msg = NULL;
	struct ha_msg* ret = NULL;

	if(session != NULL) {
		msg = ha_msg_new(10);
		ha_msg_add(msg, "t","quorum");
		ha_msg_add_int(msg, "nodenum", nodenum);
		ha_msg_add_int(msg, "weight", weight);
	
		s  = msg2wirefmt(msg, &len);
		gnutls_record_send(session, s, len);
		cl_free(s);
		len = gnutls_record_recv(session, buf, MAXMSG);
		if ((ssize_t)len < 0) {
			gnutls_bye (session, GNUTLS_SHUT_WR);
			gnutls_deinit (session);
			close(sock);
			session = NULL;
			cur_quorum = -1;
			ha_msg_del(msg);
			return TRUE;
		}
		ret = wirefmt2msg(buf, len, FALSE);
		ha_msg_value_int(ret, "quorum", &quorum);
			
		ha_msg_del(ret);
		ha_msg_del(msg);
		
		if (cur_quorum!=-1 && cur_quorum!=quorum && callback!=NULL){
			cur_quorum = quorum;
			callback();
		}
		cur_quorum = quorum;
	}
	else {
		connect_quorum_server(NULL);
	}
	return TRUE;
}

void
initialize_tls_global(void)
{
	gnutls_global_init ();
	gnutls_certificate_allocate_credentials (&xcred);
	gnutls_certificate_set_x509_trust_file (xcred, CACERT, GNUTLS_X509_FMT_PEM);
	gnutls_certificate_set_x509_key_file (xcred, CLIENTCERT, CLIENTKEY, GNUTLS_X509_FMT_PEM);
}

gnutls_session
initialize_tls_session (int sd)
{
	int ret;
	gnutls_session session;
	const int cert_type_priority[2] = { GNUTLS_CRT_X509,0};
	
	gnutls_init (&session, GNUTLS_CLIENT);
	gnutls_set_default_priority (session);
	gnutls_certificate_type_set_priority (session, cert_type_priority);
	gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, xcred);
	gnutls_transport_set_ptr (session, (gnutls_transport_ptr) GINT_TO_POINTER(sd));
	ret = gnutls_handshake (session);
	if (ret < 0)
	{
		close (sd);
		gnutls_deinit (session);
		fprintf (stderr, "*** Handshake failed\n");
		gnutls_perror (ret);
		return NULL;
	}
	verify_certificate(session);
	return session;
}

int
verify_certificate (gnutls_session session)
{
	unsigned int cert_list_size;
	const gnutls_datum *cert_list;
	int ret;
	gnutls_x509_crt cert;

	ret = gnutls_certificate_verify_peers (session);

	if (ret < 0)
	{
		printf("gnutls_certificate_verify_peers2 returns error.\n");
		return -1;
	}
	if (gnutls_certificate_type_get (session) != GNUTLS_CRT_X509) {
		printf("The certificate is not a x.509 cert\n");
    		return -1;
	}
	if (gnutls_x509_crt_init (&cert) < 0)
	{
		printf("error in gnutls_x509_crt_init\n");
		return -1;
	}

	cert_list = gnutls_certificate_get_peers (session, &cert_list_size);
	if (cert_list == NULL)
	{
		printf("No certificate was found!\n");
		return -1;
	}

	if (gnutls_x509_crt_import (cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
	{
		printf("error parsing certificate\n");
		return -1;
	}

	if (gnutls_x509_crt_get_expiration_time (cert) < time (0))
	{
		printf("The certificate has expired\n");
		return -1;
	}

	if (gnutls_x509_crt_get_activation_time (cert) > time (0))
	{
		printf("The certificate is not yet activated\n");
		return -1;
	}
	
	gnutls_x509_crt_deinit (cert);

	return 0;
}
