/* $Id: notify.h,v 1.2 2004/12/10 20:07:07 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>

#include <crm/crm.h>
#include <crm/common/xml.h>

extern FILE *msg_cib_strm;


void cib_pre_notify(
	const char *op, xmlNodePtr existing, xmlNodePtr update);

void cib_post_notify(
	const char *op, xmlNodePtr update, enum cib_errors result, xmlNodePtr new_obj);


