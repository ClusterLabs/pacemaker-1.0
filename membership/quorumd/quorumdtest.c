/*
 * Test client for Linux HA quormd daemon test client
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include <ha_msg.h>


#define DH_BITS 1024
#define MAX_BUF 1024
#define CACERT "/etc/ha.d/ca-cert.pem"
#define CLIENTKEY "/etc/ha.d/client-key.pem"
#define CLIENTCERT "/etc/ha.d/client-cert.pem"

static int verify_certificate (gnutls_session session);
static gnutls_session initialize_tls_session (int sd);
static void initialize_tls_global(void);

static gnutls_certificate_credentials xcred;

int sock = 0;

int main (int argc, char* argv[])
{
	struct sockaddr_in addr;
	struct ha_msg* msg = NULL;
	struct ha_msg* ret = NULL;
	const char* version = "2_0_8";
	struct hostent* hp;
	int i;
	int quorum;
	size_t	len;
	char* s = NULL;
	char buf[MAXMSG];
	gnutls_session session;
	int t_interval;
	/* initialize gnutls */
	initialize_tls_global();
	
	/* create socket */
	sock=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1 ) {
		return -1;
	}

	/* connect to server*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	hp = gethostbyname("pluto");
	memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));
	addr.sin_port = htons(5561);
	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(sock);
		return -1;
	}
	session = initialize_tls_session(sock);
	if (session == NULL) {
		return -1;
	}
	gnutls_record_send(session, version, strlen(version)+1);

	msg = ha_msg_new(10);
	ha_msg_add(msg, "t","init");
	ha_msg_add(msg, "cl_name","mycluster");
	
	s  = msg2wirefmt(msg, &len);
	gnutls_record_send(session, s, len);
	len = gnutls_record_recv(session, buf, MAXMSG);
	ret = wirefmt2msg(buf, len, FALSE);

	printf("result:%s\n",ha_msg_value(ret, "result"));
	ha_msg_value_int(ret, "interval", &t_interval);
	for (i = 0; i < 20; i++ )
	{
		msg = ha_msg_new(10);
		ha_msg_add(msg, "t","quorum");
		ha_msg_add_int(msg, "nodenum", 2);
		ha_msg_add_int(msg, "weight", 200);
	
		s  = msg2wirefmt(msg, &len);
		gnutls_record_send(session, s, len);
		len = gnutls_record_recv(session, buf, MAXMSG);
		ret = wirefmt2msg(buf, len, FALSE);
		printf("result:%s\n",ha_msg_value(ret, "result"));
		ha_msg_value_int(ret, "quorum", &quorum);
		printf("quorum:%d\n",quorum);
	
		ha_msg_del(ret);
		ha_msg_del(msg);
		sleep(t_interval/1000);
	}
	gnutls_bye (session, GNUTLS_SHUT_WR);
	gnutls_deinit (session);
	close(sock);
	return 0;
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
/*	
	if (status & GNUTLS_CERT_INVALID) {
		printf("The certificate is not trusted.\n");
		return -1;
	}
	if (status & GNUTLS_CERT_SIGNER_NOT_FOUND) {
		printf("The certificate hasn't got a known issuer.\n");
		return -1;
	}
	if (status & GNUTLS_CERT_REVOKED) {
		printf("The certificate has been revoked.\n");
		return -1;
	}
*/	
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
