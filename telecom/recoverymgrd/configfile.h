/* $Id: configfile.h,v 1.4 2005/07/27 09:03:24 panjiam Exp $ */
/*
 *
 *
 * Copyright (c) 2002 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CONFIGFILE_H_
#define CONFIGFILE_H_

#define CONFIGSTRINGLENGTH	256
#define CONFIGUIDLENGTH		10
#define CONFIGGIDLENGTH		10
#define MAXEVENTS		6 /* event values start at 1 */


typedef struct
{
        gboolean	inuse;
        char    	args[CONFIGSTRINGLENGTH];
} EventAction;

typedef struct
{
        char            appname[CONFIGSTRINGLENGTH];
	uid_t		uid;
	gid_t		gid;
	char            scriptname[CONFIGSTRINGLENGTH];
        EventAction     event[MAXEVENTS];

} RecoveryInfo;

extern GHashTable *scripts;
extern RecoveryInfo *current;
extern int eventindex;
extern char *tempname;
extern int length;

#define YYSTYPE char *

extern void yyerror(const char *str);
extern int yywrap(void);
extern int yyparse(void);

#endif /* CONFIGFILE_H_ */
