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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define  YYSTYPE char *
#include "conf_yacc.h"
extern int yylex(void);
extern YYSTYPE yylval;

extern void *yy_flex_realloc( void *ptr, yy_size_t size );
extern int yy_flex_strlen( yyconst char *s );

#ifdef FLEX_SCANNER
#	define	MAKE_WARNINGS_GO_AWAY	(void)yy_flex_realloc; 	\
					(void)yy_flex_strlen;
#else
#	define	MAKE_WARNINGS_GO_AWAY	;
#endif

%}

%{
int yyget_lineno(void);
%}
%{
FILE *yyget_in(void);
%}
%{
FILE *yyget_out(void);
%}
%{
int yyget_leng(void);
%}
%{
char *yyget_text(void);
%}
%{
void yyset_lineno(int);
%}
%{
void yyset_in(FILE *);
%}
%{
void yyset_out(FILE *);
%}
%{
int yyget_debug(void);
%}
%{
void yyset_debug(int);
%}
%{
int yylex_destroy(void);
%}

%option nounput

%%

PID			{return PID;};
APPHB_HUP		{return APPHB_HUP_L;};
APPHB_NOHB		{return APPHB_NOHB_L;};
APPHB_HBAGAIN		{return APPHB_HBAGAIN_L;};
APPHB_HBWARN		{return APPHB_HBWARN_L;};
APPHB_HBUNREG		{return APPHB_HBUNREG_L;};
\".+\"			{yylval=(char *)strdup(yytext); return STRING;}
[a-zA-Z][a-zA-Z0-9]*	{yylval=(char *)strdup(yytext); return WORD;}
[_a-zA-Z0-9\/.-]+	{yylval=(char *)strdup(yytext); return FILENAME;}
[ \t]+			/* ignore whitespace */;
\n			/* ignore */ MAKE_WARNINGS_GO_AWAY
\{			{return OPEN_CURLY;}
\}			{return CLOSE_CURLY;}
:			{return COLON;}
=			{return EQUALS;}
#+.*\n			/* ignore lines starting with # */

#			{REJECT;}/* This makes unused label warning go away */
			/* This rule is actually never matched because
			 * of the rule above.
			 */

%%
