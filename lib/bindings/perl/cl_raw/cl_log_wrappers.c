/* $Id: cl_log_wrappers.c,v 1.1 2004/03/04 11:33:12 lars Exp $ */
#include <cl_log_wrappers.h>
#include <clplumbing/cl_log.h>

void cl_log_helper(int priority, const char *s) {
	cl_log(priority, "%s", s);
}

void cl_perror_helper(const char * s) {
	cl_perror("%s", s);
}

