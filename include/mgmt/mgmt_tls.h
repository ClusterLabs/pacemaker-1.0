/*
 * GnuTls wrapper
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (C) 2005 International Business Machines
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
#ifndef __MGMT_TLS_H
#define __MGMT_TLS_H 1

extern int tls_init_client(void);
extern void* tls_attach_client(int sock);
extern int tls_close_client(void);

extern int tls_init_server(void);
extern void* tls_attach_server(int sock);
extern int tls_close_server(void);
		
extern ssize_t tls_send(void* s, const void *buf, size_t len);
extern ssize_t tls_recv(void* s, void* buf, size_t len);
extern int tls_detach(void* s);

#endif /* __MGMT_TLS_H */
