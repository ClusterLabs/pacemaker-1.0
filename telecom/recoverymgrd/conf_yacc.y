%{

/*
 *
 * Copyright 2002 Intel Corporation
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

#undef _GNU_SOURCE  /* in case it was defined on the command line */
#define _GNU_SOURCE /* Needed for strn* functions */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <apphb_notify.h>
#include <glib.h>

#include "configfile.h"
#include "libgen.h" /* for basename() */

extern int yylex(void);
/* #define DEBUG */

/* This conditional may not be a truly adequate test */

#ifdef YYBYACC
#	define	MAKE_WARNINGS_GO_AWAY	{			\
						(void)yyrcsid;	\
					}
#else
#	define	MAKE_WARNINGS_GO_AWAY	/* Nothing */
#endif

int yyget_lineno(void);


%}

%token PID APPHB_HUP_L APPHB_NOHB_L APPHB_HBAGAIN_L APPHB_HBWARN_L 
%token APPHB_HBUNREG_L STRING FILENAME OPEN_CURLY CLOSE_CURLY
%token COLON EQUALS WORD


%start commands
%%
commands: command commands 
	| command
	{
		MAKE_WARNINGS_GO_AWAY
	};

command: userinfo FILENAME OPEN_CURLY events 
	{
#ifdef DEBUG
		int i;
#endif
		strncpy(current->scriptname, $2, CONFIGSTRINGLENGTH);

		tempname = strdup(current->scriptname);
		strncpy(current->appname,basename(tempname), CONFIGSTRINGLENGTH);
		free(tempname);
#ifdef DEBUG
		printf("insert to hash table: [%s][%s]\n", current->appname
			, current);
#endif
		g_hash_table_insert(scripts, current->appname, current);

#ifdef DEBUG
		printf("uid=%ld\ngid=%ld\n", current->uid, current->gid);
		printf("scriptname = %s\n", current->scriptname);
		printf("appname = %s\n", current->appname);
		
		for (i=1;i<MAXEVENTS;i++) {
		   printf("event (%i) = %s\n", i, current->event[i].args);
		}
#endif
		if (0) {
			YYERROR;
		}
	};


userinfo: WORD COLON WORD
	{
		int i;
		struct passwd* mypasswd = NULL;
		struct group* mygroup = NULL;
		current = (RecoveryInfo *) malloc(sizeof(RecoveryInfo));
		if (!current) {
		   printf("out of memory. Failed to parse config file\n");
		   return(5);
		}else{
		   for (i=1;i<MAXEVENTS;i++) {
		      current->event[i].inuse = FALSE;
		      current->event[i].args[0] = '\0';
	 	   }
		   /*
		   strncpy(current->uid, $1, CONFIGUIDLENGTH);
		   strncpy(current->gid, $3, CONFIGGIDLENGTH);			
		   */
		   mypasswd = getpwnam($1);
		   mygroup = getgrnam($3);
		   if(mypasswd){   
			current->uid = mypasswd->pw_uid;
			current->gid = mypasswd->pw_gid;
			if(mygroup){
				if(mygroup->gr_gid != current->gid){
					printf("User [%s] is not in group [%s]\n"
					, $1, $3);
					return(1);
				}
			}else{
		   		printf("Cannot find group id for group:[%s]\n"
					, $3);
				return(2);
			}
		   }else{
		   	printf("Cannot find user id for user:[%s]\n", $1);
			return(3);
		   } 	
		 }
	};

events: eventdef events
	| eventdef CLOSE_CURLY
	{
	};

eventdef: event EQUALS STRING
	{
#ifdef DEBUG
		printf("eventindex = %d\n", eventindex);
		printf("string = %s\n", $3);
		printf("string(yylval) = %s\n", yylval);
		printf("strlen = %d\n", strlen($3));
#endif
		if (!current || eventindex >= MAXEVENTS){ return(4);}

		current->event[eventindex].inuse = TRUE;

		/* remove quotes */
		length = strlen($3)-2;
		if (length <= 2) {
			current->event[eventindex].args[0] = '\0';
		}else{	
			if (length > CONFIGSTRINGLENGTH)
				length = CONFIGSTRINGLENGTH;
			strncpy(current->event[eventindex].args,
                       		 &yylval[1], length);
			current->event[eventindex].args[length] = '\0';
		}
	}
	| event EQUALS WORD
	{
		current->event[eventindex].inuse = TRUE;
		strncpy(current->event[eventindex].args,$3, CONFIGSTRINGLENGTH);
		current->event[eventindex].args[CONFIGSTRINGLENGTH-1] = '\0';
	}
	| event
	{
		current->event[eventindex].inuse = TRUE;
		current->event[eventindex].args[0] = '\0';
	};

event: APPHB_HUP_L 		{ eventindex = (int) APPHB_HUP; }
	| APPHB_NOHB_L  	{ eventindex = (int) APPHB_NOHB; }
	| APPHB_HBAGAIN_L	{ eventindex =(int) APPHB_HBAGAIN; }
	| APPHB_HBWARN_L	{ eventindex = (int) APPHB_HBWARN; }
	| APPHB_HBUNREG_L	{ eventindex = (int) APPHB_HBUNREG; };
%%
