%module pymgmt
%{
#include "../../include/mgmt/mgmt_client.h"
%}

int mgmt_connect(const char* server, const char* user, const char*  passwd);
char* mgmt_sendmsg(const char* msg);
char* mgmt_recvmsg(void);
int mgmt_inputfd(void);
int mgmt_disconnect(void);
