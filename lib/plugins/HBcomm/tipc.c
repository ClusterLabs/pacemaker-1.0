/*
 * tipc for heartbeat
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>

#include <heartbeat.h>
#include <HBcomm.h>

#include <net/tipc/tipc.h>


#define PIL_PLUGINTYPE          HB_COMM_TYPE
#define PIL_PLUGINTYPE_S        HB_COMM_TYPE_S
#define PIL_PLUGIN              tipc
#define PIL_PLUGIN_S            "tipc"
#define PIL_PLUGINLICENSE       LICENSE_LGPL
#define PIL_PLUGINLICENSEURL    URL_LGPL
#include <pils/plugin.h>

struct tipc_private {
        struct sockaddr_tipc maddr;
        int recvfd;
        int sendfd;
        unsigned int name_type;
        unsigned int seq_lower;
        unsigned int seq_upper;
};

static struct hb_media *
tipc_new(unsigned int name_type, 
            unsigned int seq_lower, unsigned int seq_upper);

static int tipc_parse(const char * line);
static int tipc_open(struct hb_media * mp);
static int tipc_close(struct hb_media * mp);
static void * tipc_read(struct hb_media * mp, int * lenp);
static int tipc_write(struct hb_media * mp, void * msg, int len);
static int tipc_make_receive_sock(struct hb_media * mp);
static int tipc_make_send_sock(struct hb_media * mp);
static int tipc_descr(char ** buffer);
static int tipc_mtype(char ** buffer);
static int tipc_isping(void);

static struct hb_media_fns tipcOps;

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
static const PILPluginImports *  PluginImports;
static PILPlugin *               OurPlugin;
static PILInterface *            OurInterface;
static struct hb_media_imports * OurImports;
static void *                    interfprivate;

#define LOG         PluginImports->log
#define MALLOC      PluginImports->alloc
#define STRDUP      PluginImports->mstrdup
#define FREE        PluginImports->mfree


#define IS_TIPC_OBJECT(mp) ((mp) && ((mp)->vf == (void*)&tipcOps))
#define TIPC_ASSERT(mp)    g_assert(IS_TIPC_OBJECT(mp))

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports);

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports)
{
        /* Force the compiler to do a little type checking */
        (void)(PILPluginInitFun)PIL_PLUGIN_INIT;

        PluginImports = imports;
        OurPlugin = us;

        /* Register ourself as a plugin */
        imports->register_plugin(us, &OurPIExports);  

        /*  Register our interface implementation */
         return imports->register_interface(us, PIL_PLUGINTYPE_S
        ,        PIL_PLUGIN_S
        ,        &tipcOps
        ,        NULL                /*close */
        ,        &OurInterface
        ,        (void*)&OurImports
        ,        interfprivate); 
}


static struct hb_media *
tipc_new(unsigned int name_type, 
         unsigned int seq_lower, unsigned int seq_upper)
{
        struct tipc_private * tipc = NULL;
        struct hb_media * mp = NULL;
        char * name;


        mp = MALLOC(sizeof(struct hb_media));
        
        if ( mp == NULL ){
                PILCallLog(LOG, PIL_CRIT, 
                           "%s: malloc failed for hb_media", __FUNCTION__);
                return NULL;
        }

        mp->name = name;

        tipc = MALLOC(sizeof(struct tipc_private));

        if ( tipc == NULL ){
                PILCallLog(LOG, PIL_CRIT,
                           "%s: malloc failed for tipc_private", __FUNCTION__);
                FREE(mp);
                return NULL;
        }

        tipc->name_type = name_type;

        tipc->seq_lower = seq_lower;
        tipc->seq_upper = seq_upper;

         /* setting mcast addr */
        tipc->maddr.addrtype = TIPC_ADDR_MCAST;
        tipc->maddr.addr.name.domain = 0;
        tipc->maddr.addr.nameseq.type = name_type;

        tipc->maddr.addr.nameseq.lower = seq_lower;
        tipc->maddr.addr.nameseq.upper = seq_upper;

        mp->pd = (void *)tipc;

        return mp;

}

/*
  tipc name_type seq_lower seq_upper 
 */

#define GET_NEXT_TOKEN(bp, token) do {           \
        int toklen;                              \
        bp += strspn(bp, WHITESPACE);            \
        toklen = strcspn(bp, WHITESPACE);        \
        strncpy(token, bp, toklen);              \
        bp += toklen;                            \
        token[toklen] = EOS;                     \
}while(0)

static int
tipc_parse(const char * line)
{
        const char * bp = NULL;
        struct hb_media * media = NULL;
        char token[MAXLINE];
        unsigned int name_type;

        unsigned int seq_lower;
        unsigned int seq_upper;

        bp = line;

        /* name_type */
        GET_NEXT_TOKEN(bp, token);
        name_type = (unsigned int)atoi(token);

        /* seq_lower */
        GET_NEXT_TOKEN(bp, token);
        seq_lower = (unsigned int)atoi(token);

        /* seq_upper */
        GET_NEXT_TOKEN(bp, token);
        seq_upper = (unsigned int)atoi(token);

        PILCallLog(LOG, PIL_INFO, 
                   "%s: name type: %u, sequence lower: %u, sequence upper: %u", 
                   __FUNCTION__, name_type, seq_lower, seq_upper);
        
        media = tipc_new (name_type, seq_lower, seq_upper );

        if ( media == NULL ) {
                PILCallLog(LOG, PIL_CRIT,
                           "%s: alloc media failed", __FUNCTION__);
                return HA_FAIL;
        }

        sprintf(token, "TIPC:<%u>", name_type); 
        media->name = STRDUP(token);

        if ( media->name == NULL ) {
                PILCallLog(LOG, PIL_CRIT,
                           "%s: alloc media's name failed", __FUNCTION__);
                FREE(media);
                return HA_FAIL;
        }

        OurImports->RegisterNewMedium(media);
        PILCallLog(LOG, PIL_INFO, "%s: register new medium OK", __FUNCTION__);

        return HA_OK;
}

static int
tipc_open(struct hb_media * mp)
{
        struct tipc_private * tipc = NULL;
        tipc = (struct tipc_private *) mp->pd;

        PILCallLog(LOG, PIL_INFO, "%s: tipc_open called", __FUNCTION__);
        
        tipc->recvfd = tipc_make_receive_sock(mp);
        if ( tipc->recvfd < 0 ) {
                PILCallLog(LOG, PIL_CRIT, "%s: Open receive socket failed",
                           __FUNCTION__);
                return HA_FAIL;
        }

        tipc->sendfd = tipc_make_send_sock(mp);
        if ( tipc->sendfd < 0 ) {
                close(tipc->recvfd);

                PILCallLog(LOG, PIL_CRIT, "%s: Open send socket failed", 
                           __FUNCTION__);
                return HA_FAIL;
        }

        PILCallLog(LOG, PIL_INFO, "%s: tipc_open OK", __FUNCTION__);

        return HA_OK;
}

static int
tipc_close(struct hb_media * mp)
{
        struct tipc_private * tipc;

        tipc = (struct tipc_private *) mp->pd;

        if ( tipc->recvfd >= 0 ) {
                close(tipc->recvfd);
        }

        if ( tipc->sendfd >= 0 ) {
                close(tipc->sendfd);
        }

        FREE(tipc);
        FREE(mp);

        return HA_OK;
}

char tipc_pkt[MAXMSG];

static void *
tipc_read(struct hb_media * mp, int * len)
{
        struct sockaddr_tipc client_addr;
        struct tipc_private * tipc;
        int sock_len;
        int numbytes;

        TIPC_ASSERT(mp);

        /*
        PILCallLog(LOG, PIL_INFO, "%s: reading msg", __FUNCTION__);
        */

        tipc = (struct tipc_private *) mp->pd;

        sock_len = sizeof(struct sockaddr_tipc);
        if (( numbytes = recvfrom(tipc->recvfd, tipc_pkt, MAXMSG, 0,
                     (struct sockaddr*)&client_addr, &sock_len)) < 0) {
                PILCallLog(LOG, PIL_WARN, "%s: unable to read message", __FUNCTION__);
                return NULL;
        }

        tipc_pkt[numbytes] = EOS;
        *len = numbytes + 1;

        if ( Debug >= PKTTRACE ) {
                PILCallLog(LOG, PIL_INFO, "%s: Got %d bytes", __FUNCTION__, numbytes);
        }

        return tipc_pkt;
}


static int
tipc_write(struct hb_media * mp, void * msg, int len)
{
        struct tipc_private * tipc;

        int numbytes;
        int sock_len;

        TIPC_ASSERT(mp);

        tipc = (struct tipc_private *) mp->pd;        

        sock_len = sizeof(struct sockaddr_tipc);

        if ( (numbytes = sendto(tipc->sendfd, msg, len, 0, 
                                (struct sockaddr *)&tipc->maddr, 
                                sock_len)) < 0 ){
                PILCallLog(LOG, PIL_INFO, "%s: unable to send message", __FUNCTION__);
                return HA_FAIL;
        }

        if ( numbytes != len ) {
                PILCallLog(LOG, PIL_WARN, "%s: Sent %d bytes, message length is %d", 
                           __FUNCTION__, numbytes, len);
                return HA_FAIL;
        }
        
        if ( Debug >= PKTTRACE ) {
                PILCallLog(LOG, PIL_INFO, "%s: Sent %d bytes", __FUNCTION__, numbytes);
        } 

        return HA_OK;
}


static int 
tipc_mtype(char ** buffer) { 
        *buffer = STRDUP(PIL_PLUGIN_S);
        if (!*buffer) {
                return 0;
        }

        return STRLEN_CONST(PIL_PLUGIN_S);
}



static int
tipc_descr(char ** buffer) { 
        const char constret[] = "tipc communication module";
        *buffer = STRDUP(constret);
        if (!*buffer) {
                return 0;
        }

        return STRLEN_CONST(constret);
}

static int
tipc_isping(void) {
    return 0;
}


static int 
tipc_make_receive_sock(struct hb_media * mp)
{
        struct sockaddr_tipc server_addr;
        struct tipc_private * tipc = NULL;
        int sock_len;
        int sd;        
        
        sock_len = sizeof (struct sockaddr_tipc);
        sd = socket (AF_TIPC, SOCK_RDM, 0);

        tipc = (struct tipc_private *) mp->pd;
        
        server_addr.family = AF_TIPC;
        server_addr.addrtype = TIPC_ADDR_NAMESEQ;
        server_addr.scope = TIPC_CLUSTER_SCOPE;
        server_addr.addr.nameseq.type = tipc->name_type;
        server_addr.addr.nameseq.lower = tipc->seq_lower;
        server_addr.addr.nameseq.upper = tipc->seq_upper;

        /* Bind  port to sequence */

        if (bind (sd, (struct sockaddr*)&server_addr, sock_len) != 0){
                PILCallLog(LOG, PIL_CRIT, 
                           "%s: Could not bind to sequence <%u,%u,%u> scope %u",
                           __FUNCTION__, 
                           server_addr.addr.nameseq.type,
                           server_addr.addr.nameseq.lower,
                           server_addr.addr.nameseq.upper,
                           server_addr.scope);
                return -1;
        }

        PILCallLog(LOG, PIL_INFO, 
                   "%s: Bound to name sequence <%u,%u,%u> scope %u", 
                   __FUNCTION__,
                   server_addr.addr.nameseq.type, 
                   server_addr.addr.nameseq.lower,
                   server_addr.addr.nameseq.upper,
                   server_addr.scope);


        return sd;
}

static int 
tipc_make_send_sock(struct hb_media * mp)
{
        int sd = socket (AF_TIPC, SOCK_RDM, 0);
        return sd;
}

static struct hb_media_fns tipcOps ={
        NULL,
        tipc_parse,                
        tipc_open,
        tipc_close,
        tipc_read,
        tipc_write,
        tipc_mtype,
        tipc_descr,
        tipc_isping,
};
