/*
 * auth.h: Authentication functions for Linux-HA
 *
 * Copyright (C) 2000, 2001 Alan Robertson <alanr@unix.sh>
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

#ifndef HBAUTH_H
#	define HBAUTH_H 1


struct HBauth_info {
	struct HBAuthOps *	auth;
	const char *		authname;
	char *			key;
};

/* Authentication interfaces */
struct HBAuthOps {
	int (*auth)
	(	const struct HBauth_info * authinfo, const void *data
	,	size_t data_len, char * result, int resultlen);
	int		(*needskey) (void); 
};

#define HB_AUTH_TYPE	HBauth
#define HB_AUTH_TYPE_S	"HBauth"

#endif /*HBAUTH_H*/
