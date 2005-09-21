#include <unistd.h>
#include <stdio.h>
#include <mgmt/mgmt.h>

int main (int argc, char* argv[])
{
	char* ret;
	mgmt_connect("127.0.0.1", "hacluster","hacluster");
	ret = mgmt_sendmsg(MSG_ECHO);
	printf("%s\n",ret);
	mgmt_del_msg(ret);
	
	ret = mgmt_sendmsg(MSG_ACTIVENODES);
	printf("%s\n",ret);
	mgmt_del_msg(ret);

	ret = mgmt_sendmsg(MSG_ALLNODES);
	printf("%s\n",ret);
	mgmt_del_msg(ret);
	
	mgmt_disconnect();
	return 0;
}
