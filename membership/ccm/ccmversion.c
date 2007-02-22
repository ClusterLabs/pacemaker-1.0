/* 
 * ccmversion.c: routines that handle information while in the version 
 * request state
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <lha_internal.h>
#include <ccm.h>

#define MAXTRIES 3

#define VERSION_GET_TIMER(ver) (ver->time)
#define VERSION_SET_TIMER(ver, t) ver->time = t
#define VERSION_GET_TRIES(ver) ver->numtries
#define VERSION_RESET_TRIES(ver) ver->numtries = 0
#define VERSION_INC_TRIES(ver) (ver->numtries)++
#define VERSION_INC_NRESP(ver) (ver->n_resp)++
#define VERSION_SET_NRESP(ver,val) ver->n_resp = val
#define VERSION_GET_NRESP(ver) ver->n_resp


/* */
/* return true if we have waited long enough for a response */
/* for our version request. */
/* */
static int
version_timeout_expired(ccm_version_t *ver, longclock_t timeout)
{
	return(ccm_timeout(VERSION_GET_TIMER(ver), ccm_get_time(), 
				timeout));
}

/* */
/* reset all the data structures used to track the version request */
/* state. */
/* */
void
version_reset(ccm_version_t *ver)
{
	VERSION_SET_TIMER(ver,ccm_get_time());
	VERSION_RESET_TRIES(ver);
	VERSION_SET_NRESP(ver,0);
}

/* */
/* return true if version request has message has to be resent. */
/* else return false. */
/* */
int
version_retry(ccm_version_t *ver, longclock_t timeout)
{
	if(version_timeout_expired(ver, timeout)) {
		ccm_debug2(LOG_DEBUG, "version_retry:%d tries left" 
			,	3-VERSION_GET_TRIES(ver));
		if(VERSION_GET_TRIES(ver) == MAXTRIES) {
			return VER_TRY_END;
		} else {
			VERSION_INC_TRIES(ver);
			VERSION_SET_TIMER(ver,ccm_get_time());
			return VER_TRY_AGAIN;
		}
	}
	return VER_NO_CHANGE;
}

/* */
/* The caller informs us: */
/* "please note that there is some activity going on in the cluster. */
/* Probably you may want to try for some more time" */
/* */
void
version_some_activity(ccm_version_t *ver)
{
	VERSION_RESET_TRIES(ver);
}


void
version_inc_nresp(ccm_version_t *ver)
{
	VERSION_INC_NRESP(ver);
}

void
version_set_nresp(ccm_version_t *ver, int val)
{
	VERSION_SET_NRESP(ver, val);
}

unsigned int
version_get_nresp(ccm_version_t *ver)
{
	return VERSION_GET_NRESP(ver);
}
