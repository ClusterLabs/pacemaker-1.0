/*
 * gXMLscan.h - gXMLscan header file
 *
 * Copyright (C) 2001   Tom Darrow <cdarrow@mines.edu>
 *                      Mike Martinez-Schiferl <mike@starfyter.com>
 *                      Bryan Weatherly <bryanweatherly@yahoo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <glib.h>

#ifndef gXML_S
#define gXML_S
#define MAXCHAR 256


/* types for returning tokens */
typedef enum {
        T_EOF=0,        /* 0 - end-of-file */
        T_LT,           /* 1 - '<' */
        T_GT,           /* 2 - '>' */
        T_LTSL,         /* 3 - '</' */
        T_SLGT,         /* 4 - '/>' */
        T_QSTR,         /* 5 - "quoted string" */
        T_WORD,         /* 6 - a word */
        T_XMLSTR,       /* 7 - non-quoted string outside of < > delimiters */
        T_EQ,           /* 8 - '=' */
	T_BAD,		/* 9 - bad token */
        T_MAXTYPE       /*  - Not a real type */
}TokenType;

/* gXML_token def */
/* this structure is inherited by the scanner and is */
/* used to keep track of the state machine and cuurent */
/* and peek tokens for the parser */
/* gXML_token def */
typedef struct _gXML_token{
        gint            state;
        TokenType       peek;
        gint            c_CurChar; /* The current character */
        gint            c_NextChar; /* The next character */
	GString*	TokString;
}gXML_token;


/* gXML_scanner def */
/* this structure inherits a gXML_token, it keeps track */
/* of the state the next and current char tokens as well */
/* as the NexToken and the peek variables used by the gXML_parse */
/* function */
typedef struct _gXML_scanner{
        gXML_token n_token;
        GString *input_stream;
	GString* tokstr;
	GString* escseq;
	gchar	savedws;
	gboolean	saved_char;
        char current_char;
        char next_char;
        gint cur_pos;
        gint nex_pos;
}gXML_scanner;
/* prototypes */

gXML_token
gXML_scan(gXML_scanner *scanner);
/*  inputs:     passed a gXML_scanner that has already been initialized */
/*  outputs:    each token is returned, one at a time, in the form */
/*              of a gXML_token variable */


gXML_scanner* gXML_new_scanner(GString *input_stream);
/*  inputs:     passed a GString that is the XML stream to be scanned */
/*  outputs:    initializes a new scanner and returns it */

gint gXML_scanner_get_next_char(gXML_scanner *scanner);
/*  inputs:     passed a valid gXML_scanner */
/*  outputs:    scanner increments scanner token position varibles */
/*              and changes the current and next token variables */
/*              accordingly.  It returns a gint that is the integer */
/*              representation of the next_token */

#endif

