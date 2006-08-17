/*
 * dummy.c: dummy 
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_pidfile.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>
#include <ha_msg.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <mgmt/mgmt.h>
#include "ha_mgmt_client.h"

#define MAX_CMD_LEN 1024

int main(int argc, char *argv[])
{
	char *result;

	init_logger("dummy");
	result = process_cmnd_eventd("hb_config");
	printf("%s", result);
	return 0;
}

