/*
 * HBcomm.h: Communication functions for Linux-HA
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

#ifndef HBCOMM_H
#	define HBCOMM_H 1

#define HB_COMM_TYPE	HBcomm
#define HB_COMM_TYPE_S	"HBcomm"

/*
 *	List of functions provided by implementations of the heartbeat media
 *	interface.
 */
struct hb_media_fns {
	struct hb_media*(*new)		(const char * token);
	int		(*parse)	(const char * options);
	int		(*mopen)	(struct hb_media *mp);
	int		(*close)	(struct hb_media *mp);
	void*		(*read)		(struct hb_media *mp, int *len );
	int		(*write)	(struct hb_media *mp
					 ,	void *msg, int len);
	int		(*mtype)	(char **buffer);
	int		(*descr)	(char **buffer);
	int		(*isping)	(void);
};

/* Functions imported by heartbeat media plugins */
struct hb_media_imports {
	const char *	(*ParamValue)(const char * ParamName);
	void		(*RegisterNewMedium)(struct hb_media* mp);
	int		(*devlock)(const char *);	/* Lock a device */
	int		(*devunlock)(const char *);	/* Unlock a device */
	int		(*StrToBaud)(const char *);	/* Convert baudrate */
	void		(*RegisterCleanup)(void(*)(void));
	void		(*CheckForEvents)(void);	/* Check for signals */
	/* Actually there are lots of other dependencies that ought to
	 * be handled, but this is a start ;-)
	 */
};

#define	PKTTRACE	4
#define	PKTCONTTRACE	5

#endif /*HBCOMM_H*/
