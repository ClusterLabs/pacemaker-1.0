%module "heartbeat::cl_raw"
%{
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_poll.h>
#include <clplumbing/GSource.h>
#include <clplumbing/ipc.h>
#include <ipc_wrappers.h>
%}

%include <clplumbing/ipc.h>
%include <ipc_wrappers.h>
%include <cl_log_wrappers.h>

