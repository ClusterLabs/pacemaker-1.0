/* $Id: ipc_wrappers.h,v 1.1 2004/03/04 11:33:12 lars Exp $ */

/* Wrapping all of glib would suck, all we need is a simple way to
 * construct GHashTables */

GHashTable *simple_hash_new(void);
void simple_hash_insert(GHashTable *foo, char *name, char *value);
void simple_hash_destroy(GHashTable *foo);

IPC_Auth *helper_create_auth(void);
void helper_add_auth_uid(IPC_Auth *auth, int uid);
void helper_add_auth_gid(IPC_Auth *auth, int gid);

/* The following helper wrappers prevent us from having to deal with
 * function pointers. The constructors don't need to be wrapped.
 */

/* Wrappers for the waitconnection functions */
void ipc_wc_destroy(IPC_WaitConnection *wc);
int ipc_wc_get_select_fd(IPC_WaitConnection *wc);
IPC_Channel *ipc_wc_accept_connection(IPC_WaitConnection *wc, IPC_Auth *auth);

/* Wrappers for the channel functions */
void ipc_ch_destroy(IPC_Channel *ch);
int ipc_ch_initiate_connection(IPC_Channel *ch);
int ipc_ch_verify_auth(IPC_Channel *ch, IPC_Auth *auth);
int ipc_ch_send(IPC_Channel *ch, IPC_Message *msg);

/* Don't ask and I won't explain, m'kay? */
typedef struct IPC_MESSAGE_WITH_RC IPC_Message_with_rc;
struct IPC_MESSAGE_WITH_RC{
	IPC_Message *msg;
	int rc;
};

IPC_Message_with_rc *ipc_ch_recv(IPC_Channel *ch);
void ipc_ch_msg_with_rc_destroy(IPC_Message_with_rc *msg);

int ipc_ch_waitin(IPC_Channel *ch);
int ipc_ch_waitout(IPC_Channel *ch);
int ipc_ch_is_message_pending(IPC_Channel *ch);
int ipc_ch_is_sending_blocked(IPC_Channel *ch);
int ipc_ch_resume_io(IPC_Channel *ch);
int ipc_ch_get_send_select_fd(IPC_Channel *ch);
int ipc_ch_get_recv_select_fd(IPC_Channel *ch);
int ipc_ch_set_send_qlen(IPC_Channel *ch, int q_len);
int ipc_ch_set_recv_qlen(IPC_Channel *ch, int q_len);
/* Macros are annoying to call from within scripting languages */
int ipc_ch_isrconn(IPC_Channel *ch);
int ipc_ch_iswconn(IPC_Channel *ch);

/* Dealing with the messages themselves */
void ipc_msg_done(IPC_Message *msg);
char *ipc_msg_get_body(IPC_Message *msg);
IPC_Message *ipc_msg_constructor(IPC_Channel *ch, size_t s, char *data);


