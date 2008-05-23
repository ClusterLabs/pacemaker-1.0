/* File: stonithd_msg.h
 * Description: Head file which define message related used in stonithd and
 * its client library.
 *
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _STONITHD_MSG_H_
#define _STONITHD_MSG_H_

#define STONITHD_SOCK HA_VARRUNDIR"/heartbeat/stonithd"
#define STONITHD_CALLBACK_SOCK HA_VARRUNDIR"/heartbeat/stonithd_callback"
/* define the field name for messages stonithd used */
#define F_STONITHD_TYPE   "stonithd"

#define F_STONITHD_APIREQ "reqest"	/* api request */
#define F_STONITHD_APIRPL "reply"	/* api reply */
#define F_STONITHD_APIRET "apiret"	/* api return code */
#define F_STONITHD_CNAME  "cname"	/* client name */
#define F_STONITHD_CPID   "cpid"     	/* client pid */
#define F_STONITHD_CEUID  "ceuid"     	/* client executing uid */
#define F_STONITHD_CEGID  "cegid"     	/* client executing gid */
#define F_STONITHD_OPTYPE "optype"     	/* stonith op type */
#define F_STONITHD_NODE   "node"     	/* the name of node which is rquired
					   to stonith */
#define F_STONITHD_NODE_UUID "node_uuid"/* the uuid of the node which is rquired
					   to be stonith'd */
#define F_STONITHD_TIMEOUT  "timeout"  	/* the timeout of a stonith operation */
#define F_STONITHD_RSCID    "rscid" 	/* stonith resource id */
#define F_STONITHD_RANAME   "raname" 	/* stonith RA name */
#define F_STONITHD_RAOPTYPE "raoptype" 	/* stonith RA op type */
#define F_STONITHD_PARAMS   "params" 	/* parameters for stonith RA  */
#define F_STONITHD_CALLID   "callid" 	/* RA executing call_id==pid */
#define F_STONITHD_STTYPES  "sttypes" 	/* stonith device types */
#define F_STONITHD_FRC	    "frc" 	/* final return code */
#define F_STONITHD_PDATA    "pdata" 	/* private data for callback */
#define F_STONITHD_NLIST    "nlist" 	/* node name list for final return */
#define F_STONITHD_COOKIE   "cookie"    /* cookie to identify a client */
#define F_STONITHD_ERROR    "error"     /* error message of operation */
#define F_STONITHD_OP       "stonithdop" /* stonithd operation */

/* Maximum length for stonithd message type */
#define MAXLEN_SMTYPE  18
/* define the message type (F_STONITHD_TYPE) value used by stonithd */
#define ST_APIREQ	"apireq"
#define ST_APIRPL	"apirpl"

/* define the message type (F_STONITHD_APIREQ) value used by stonithd */
#define ST_SIGNON	"signon"
#define ST_SIGNOFF	"signoff"
#define ST_STONITH	"stonith"
#define ST_RAOP		"raop"		/* stonith resource agent operation */
#define ST_LTYPES	"ltypes"	/* List the stonith device types */

/* define the message type (F_STONITHD_APIRPL) value used by stonithd */
#define ST_RSIGNON	"rsignon"
#define ST_RSIGNOFF	"rsignoff"
#define ST_RSTONITH	"rstonith"	/* stonith ops sync result */
#define ST_STRET	"stret"		/* stonith ops final result */
#define ST_RRAOP	"rraop"		/* stonithRA ops sync result */
#define ST_RAOPRET	"raopret"	/* stonithRA ops final result */
#define ST_RLTYPES	"rltypes"	/* return the stonith device types */

/* define the message type (F_STONITHD_APIRET) value used by stonithd */
#define ST_APIOK	"apiok"
#define ST_APIFAIL	"apifail"	/* Generic error */
#define	ST_BADREQ	"badreq"
#define ST_COOKIE	"cookieauth"	/* cookie authentication required */

#define ZAPMSG(m)       { ha_msg_del(m); (m) = NULL; }

/* free the object allocated by g_new, g_strdup and etc. */
#define ZAPGDOBJ(m)					\
			if ( (m) != NULL ) {		\
				g_free(m);		\
				(m) = NULL;		\
			}

/* Some message handle funtions used internally */
int ha_msg_addhash(struct ha_msg * msg, const char * name, GHashTable * htable);
struct ha_msg * hashtable_to_hamsg(GHashTable * htable);
void insert_data_pairs(gpointer key, gpointer value, gpointer user_data);
GHashTable * cl_get_hashtable(struct ha_msg *request, const char * name);
void print_str_hashtable(GHashTable * htable);
void print_str_item(gpointer key, gpointer value, gpointer user_data);

#endif /* _STONITHD_MSG_H_ */
