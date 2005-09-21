#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <mgmt/tls.h>

#define DH_BITS 1024

static gnutls_dh_params dh_params;
gnutls_anon_server_credentials anoncred_server;
gnutls_anon_client_credentials anoncred_client;

const int kx_prio[] =
{
	GNUTLS_KX_ANON_DH,
	0
};

int
tls_init_client(void)
{
	gnutls_global_init();
	gnutls_anon_allocate_client_credentials(&anoncred_client);
	return 0;
}

void* 
tls_attach_client(int sock)
{
	int ret;
	gnutls_session* session = (gnutls_session*)gnutls_malloc(sizeof(gnutls_session));
	gnutls_init(session, GNUTLS_CLIENT);
	gnutls_set_default_priority(*session);
	gnutls_kx_set_priority (*session, kx_prio);
	gnutls_credentials_set(*session, GNUTLS_CRD_ANON, anoncred_client);
	gnutls_transport_set_ptr(*session, (gnutls_transport_ptr) sock);
	ret = gnutls_handshake(*session);
	if (ret < 0) {
		fprintf(stderr, "*** Handshake failed\n");
		gnutls_perror(ret);
		gnutls_deinit(*session);
		gnutls_free(session);
		return NULL;
	}
	return session;
}

ssize_t
tls_send(void* s, const void *buf, size_t len)
{
	gnutls_session* session = (gnutls_session*)s;
	while (1) {
		int ret = gnutls_record_send(*session, buf, len);
		if (ret != GNUTLS_E_INTERRUPTED && ret != GNUTLS_E_AGAIN) {
			return ret;
		}
	}
	return 0;
}
ssize_t
tls_recv(void* s, void* buf, size_t len)
{
	gnutls_session* session = (gnutls_session*)s;
	while (1) {
		int ret = gnutls_record_recv(*session, buf, len);
		if (ret != GNUTLS_E_INTERRUPTED && ret != GNUTLS_E_AGAIN) {
			return ret;
		}
	}
	return 0;
}
int
tls_detach(void* s)
{

	gnutls_session* session = (gnutls_session*)s;
	gnutls_bye(*session, GNUTLS_SHUT_RDWR);
	gnutls_deinit(*session);
	gnutls_free(session);
	return 0;
}

int
tls_close_client(void)
{
	gnutls_anon_free_client_credentials (anoncred_client);
	gnutls_global_deinit();
	return 0;
}

int
tls_init_server(void)
{
	gnutls_global_init();
	gnutls_anon_allocate_server_credentials (&anoncred_server);
	gnutls_dh_params_init(&dh_params);
	gnutls_dh_params_generate2(dh_params, DH_BITS);
	gnutls_anon_set_server_dh_params (anoncred_server, dh_params);
	return 0;
}

void* 
tls_attach_server(int sock)
{
	int ret;
	gnutls_session* session = (gnutls_session*)gnutls_malloc(sizeof(gnutls_session));
	gnutls_init(session, GNUTLS_SERVER);
	gnutls_set_default_priority(*session);
	gnutls_kx_set_priority (*session, kx_prio);
	gnutls_credentials_set(*session, GNUTLS_CRD_ANON, anoncred_server);
	gnutls_dh_set_prime_bits(*session, DH_BITS);
	gnutls_transport_set_ptr(*session, (gnutls_transport_ptr) sock);
	ret = gnutls_handshake(*session);
	if (ret < 0) {
		fprintf(stderr, "*** Handshake has failed (%s)\n\n",
			gnutls_strerror(ret));
		gnutls_deinit(*session);
		gnutls_free(session);
		return NULL;
	}
	return session;
}

int
tls_close_server(void)
{
	gnutls_anon_free_server_credentials (anoncred_server);
	gnutls_global_deinit();
	return 0;
}

