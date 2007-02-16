/*
 * gXMLscan.c - gXMLscan implementation file
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
#include "gXMLscan.h"
#include "glibconfig.h"
#include "XMLchars.h"

/* States */
/* this state enumeration is used for the state and action */
/* tables */
enum state {
        INIT,
        TOTAG,	/* < received - no tag chars yet */
        TOTG2,	/* < received - no tag chars yet - '<' or other emitted */
	INTAG,	/* Inside tag - tag chars found */
	INTG2,	/* Inside tag - tag chars found delim char processed */
        IN_QT,	/* Inside quotes inside a tag */
	INSTR,	/* In XML string - last char not white space */
	INSTW,	/* In XML string - last char was white space */
        SLASH,	/* Received a slash character inside of a tag */
	INAMP,	/* Received an ampersand character */
        FINAL,
        MAXSTATE /* Not a real state */
};

/* Character classes  (inputs) */
enum CharClass {
        CL_EOF,		/* End Of File */
        QUOTE,		/* " */
        LT,		/* < */
        GT,		/* > */
        EQUAL,		/* = */
        SLSH,		/* / */
        STRING,		/* Most other characters */
	WHSP,		/* white space: blank, tab, etc */
	AMP,		/* Ampersand */
	SEMI,		/* Semicolon */
        MAXINP		/* Not a real input */
};

/* Actions */
#define A_EOF           0x0001L		/* Emit EOF */
#define A_LT            0x0002L		/* Emit < */
#define A_LTS		0x0004L		/* Emit </ */
#define A_GT            0x0008L		/* Emit > */
#define A_SGT		0x0010L		/* Emit /> */
#define A_EQ            0x0020L		/* Emit = */
#define A_STR		0x0040L		/* Emit XML string */
#define A_QST		0x0080L		/* Emit quoted string */
#define A_WD		0x0100L		/* Emit word */
#define A_SVE		0x0200L		/* Save escape char seq */
#define A_ESC		0x0400L		/* Append escaped char */
#define A_SVW		0x0800L		/* Save whitespace char */
#define A_APW		0x1000L		/* append whitespace char */
#define A_AP		0x2000L		/* append char to token */
#define A_ER1		0x4000L		/* emit Invalid char token */
#define A_BK		0x8000L		/* unget (backup) a character */

/* STATE-NEXT STATE TABLE */
/* this is the state vs next state table */
/* the current state is along the left and the character */
/* received is at the top */
static int FSA[MAXSTATE][MAXINP] = {
/*STATE:   EOF   QUOTE  LT     GT    EQUAL  SLSH   STRING WHSP    AMP   SEMI */
/*INIT*/ {FINAL, INSTR, TOTAG, INIT,  INIT,  INSTR, INSTR, INIT,  INAMP, INSTR},
/*TOTAG*/{FINAL, IN_QT, TOTG2, INTG2, INTG2, TOTG2, INTAG, TOTG2, TOTG2, INTAG},
/*TOTG2*/{FINAL, IN_QT, TOTAG, INIT,  TOTG2, SLASH, INTAG, TOTAG, TOTAG, INTAG},
/*INTAG*/{FINAL, IN_QT, INIT,  INIT,  INTG2, INTG2, INTG2, TOTAG, INTAG, INTAG},
/*INTG2*/{FINAL, IN_QT, TOTAG, INIT,  TOTG2, SLASH, INTG2, INTG2, INTG2, INTG2},
/*IN_QT*/{FINAL, TOTG2, IN_QT, IN_QT, IN_QT, IN_QT, IN_QT, IN_QT, IN_QT, IN_QT},
/*INSTR*/{FINAL, INSTR, TOTAG, INIT,  INSTR, INSTR, INSTR, INSTW, INAMP, INSTR},
/*INSTW*/{FINAL, INSTR, TOTAG, INIT,  INSTR, INSTR, INSTR, INSTW, INAMP, INSTW},
/*SLASH*/{FINAL, IN_QT, TOTAG, INIT,  INSTR, SLASH, INSTR, SLASH, INAMP, SLASH},
/*INAMP*/{FINAL, INAMP, TOTAG, INIT,  INAMP, INAMP, INAMP, INAMP, INAMP, INSTR},
/*FINAL*/{FINAL, FINAL, FINAL, FINAL, FINAL, FINAL, FINAL, FINAL, FINAL, FINAL},
};

/* STATE-ACTION TABLE */
/* this is the state vs action table */
/* the current state is along the left (which row) and the character */
/* received is at the top (which column) */
/*
 * Normally, I try really hard to get the code to be readable in 80 columns.
 * This table really needs ~ 91 columns for readability.  Sorry...
 */

#define	APPW	(A_APW|A_AP)
static unsigned long Actions[MAXSTATE][MAXINP] = {
	/* These actions need revising to match new state table */
/*STATE:  EOF   QUOTE   LT      GT          EQUAL      SLSH   STRING   WHSP   AMP    SEMI*/
/*INIT*/ {A_EOF, 0,     0,     A_GT,       A_AP,       A_AP,     A_AP,   0,    0,    A_AP},
/*TOTAG*/{A_EOF, 0,   A_LT,    A_LT|A_BK,  A_GT|A_BK,  A_LTS, A_LT|A_BK, 0,    0,    A_AP},
/*TOTG2*/{A_EOF, 0,     0,     A_GT,       A_EQ,       A_ER1,   A_AP,    0,    0,    A_AP},
/*INTAG*/{A_EOF, A_WD, A_WD,   A_WD|A_BK,  A_WD|A_BK,  A_WD,    A_AP,   A_WD,  0,    A_AP},
/*INTG2*/{A_EOF, 0,  A_WD|A_BK, A_WD|A_BK, A_WD|A_BK, A_WD|A_BK, A_AP, A_WD,   0,    A_AP},
/*IN_QT*/{A_EOF, A_QST, A_AP,  A_AP,       A_AP,       A_AP,    A_AP,   A_AP,  0,    A_AP},
/*INSTR*/{A_EOF, A_AP, A_STR,  A_STR,      A_AP,       A_AP,    A_AP,   A_SVW, 0,    A_AP},
/*INSTW*/{A_EOF, APPW,A_STR,A_SVW|A_STR|A_BK,APPW,     APPW,    APPW,    0,   A_APW, APPW},
/*SLASH*/{A_EOF, A_ER1, A_ER1, A_SGT,      A_ER1,      A_ER1,   A_ER1,   0,   A_ER1, A_ER1},
/*INAMP*/{A_EOF, A_SVE, A_SVE, A_SVE,      A_SVE,      A_SVE,   A_SVE,   0,   A_SVE, A_ESC},
/*FINAL*/{A_EOF, A_EOF, A_EOF, A_EOF,      A_EOF,      A_EOF,   A_EOF, A_EOF, A_EOF, A_EOF}
};

enum  CharClass gXML_find_chartype(gint token);
void gXML_scanner_unget_char(gXML_scanner *scanner);
/*  inputs:     passed an integer value for a char */
/*  outputs:    it returns the correct enumerated value for each */
/*              char */

/*implementation///////////// */

static gchar gXMLLookupEscape(const char * s);
gboolean XMLDEBUGSCANNER=FALSE;
gXML_token
gXML_scan(gXML_scanner *scanner)
{
        /*variables */
        gint this_char;
        enum CharClass chartype;
        enum state curstate=INIT, nextstate;
        unsigned long action;
        /*/variables */

        /* Get the next character */

        while((this_char = gXML_scanner_get_next_char(scanner))){
		gboolean emittoken = FALSE;

                /* How can this be EOS, when you checked in loop "while"? */
                /* ??? */

                if(this_char != EOS){
                        scanner->n_token.c_NextChar = this_char;
                }/*end else if(this_char != EOS.. */

		if (XMLDEBUGSCANNER) {
			printf("l %d char: '%c' %d\n"
			,	__LINE__, this_char, this_char);
		}
                
                chartype = gXML_find_chartype(scanner->n_token.c_NextChar);
		if (XMLDEBUGSCANNER) {
			printf("l %d type: %d char: '%c' %d\n"
			,	__LINE__
			,	chartype
			,	scanner->n_token.c_NextChar
			,	scanner->n_token.c_NextChar
			);
		}
		g_assert (chartype < MAXINP);
		g_assert (chartype >= 0);
		g_assert (curstate >= 0);
		g_assert (curstate < MAXSTATE);
                curstate = scanner->n_token.state;
                nextstate = FSA[curstate][chartype];
		g_assert (nextstate >= 0);
		g_assert (nextstate < MAXSTATE);
                scanner->n_token.state = nextstate;
                action = Actions[curstate][chartype];

		if (XMLDEBUGSCANNER) {
			printf("curstate:%d - chartype:%d"
		       "	=> nextstate:%d- action:0x%03lx %d\n"
			,	curstate
			,	chartype
			,	nextstate
			,	action
			,	scanner->n_token.state);
		}

                /* ACTION DEFS */
		/*
		 * The order in which these actions occur is significant...
		 * Generally the first order of business is appending characters
		 * The last order of business is emitting tokens.
		 *
		 */
#define EMIT(type) {							\
		scanner->n_token.peek = (type);				\
		g_string_assign(scanner->n_token.TokString		\
		,	scanner->tokstr->str);				\
		g_string_truncate(scanner->tokstr, 0);			\
		g_string_truncate(scanner->escseq, 0);			\
		emittoken = TRUE;					\
	}

#define EMITCONST(type, str) {						\
		g_string_assign(scanner->tokstr, (str));	\
		EMIT(type);						\
	}

#define EMITVAR(type) {							\
		EMIT(type);						\
	}

		if (action&A_EOF) {	/* Emit an EOF... */
			EMITCONST(T_EOF, "<EOF>");
                }

                if (action&A_LT) {	/* Emit a '<' ... */
			EMITCONST(T_LT, "<");
                }

                if (action&A_LTS) {	/* Emit a '</' ... */
			EMITCONST(T_LTSL, "</");
                }

                if (action&A_GT) {	/* Emit a '>' ... */
			EMITCONST(T_GT, ">");
                }

                if (action&A_SGT) {	/* Emit a '/>' ... */
			EMITCONST(T_SLGT, "/>");
                }

                if (action&A_EQ) {	/* Emit an '=' ... */
			EMITCONST(T_EQ, "=");
                }

		if (action&A_APW) {	/* Append saved w.s. char to token */
			g_string_append_c(scanner->tokstr, scanner->savedws);
		}

		if (action&A_AP) {	/* append char to token */
			g_string_append_c(scanner->tokstr
			,	scanner->n_token.c_NextChar);
		}

		if (action&A_SVW) {	/* Save white space char */
			scanner->savedws =  scanner->n_token.c_NextChar;
		}

		if (action&A_STR) {	/* Emit saved token as XML string */
			EMITVAR(T_XMLSTR);
		}

		if (action&A_QST) {	/* Emit saved token as "string" */
			EMITVAR(T_QSTR);
		}

		if (action&A_WD) {	/* Emit saved token as a word (tag) */
			/* Tags and attributes aren't case sensitive in XML */
			g_strdown(scanner->tokstr->str);				\
			EMITVAR(T_WORD);
		}

                if (action&A_ER1) {	/* Emit "bad" token */
			EMITCONST(T_BAD, "<badtoken>");
                }


		/* Add character to saved escape sequence... */
		if (action&A_SVE) {
			g_string_append_c(scanner->escseq
			,	scanner->n_token.c_NextChar);
		}

		/* Translate saved escape sequence to a character */
		if (action&A_ESC) {
			gchar	escchar
			=	gXMLLookupEscape(scanner->escseq->str);


			if (escchar == EOS) {
				EMITCONST(T_BAD, "<bad escape sequence>");
			}else{
				g_string_append_c(scanner->tokstr, escchar);
			}
			g_string_truncate(scanner->escseq, 0);
		}

		/* Unget character so we can see it again... */
		if (action&A_BK) {
			gXML_scanner_unget_char(scanner);
		}

		/*
		 * Actually return the token here...
		 * This has to happen after all the other actions...
		 */
		if (emittoken) {
			if (XMLDEBUGSCANNER) {
				fprintf(stderr
				,	"********EMIT: TOK: [%s] [%d]\n"
				,	scanner->n_token.TokString->str
				,	scanner->n_token.peek);
			}
			return scanner->n_token;
		}

        }/*end while(this_char = gXML_scanner_get_next_char... */
        
        scanner->n_token.c_CurChar = scanner->n_token.c_NextChar;
        scanner->n_token.peek = T_EOF;                        
	/*tell init_structure that we've reached EOF */
        return scanner->n_token;         
}


/*
 *	Classify the character as an FSA input...
 */
enum CharClass
gXML_find_chartype(gint token)
{
        enum CharClass chartype;

        switch(token){
        case '\"':
                chartype = QUOTE;
                break;
        case '<':
                chartype = LT;
                break;
        case '>':
                chartype = GT;
                break;
        case '=':
                chartype = EQUAL;
                break;
        case '/':
                chartype = SLSH;
                break;
        case EOF:
                chartype = CL_EOF;
                break;
	case ';':
                chartype = SEMI;
                break;
	case '&':
                chartype = AMP;
                break;
	case ' ': case '\t': case '\n': case '\r': case '\f':
                chartype = WHSP;
                break;
        default:
                chartype = STRING;
                break;
        }
        return chartype;
}

gXML_scanner*
gXML_new_scanner(GString *input_stream)
{
        gXML_scanner *scanner;
        scanner = g_new(gXML_scanner, 1);
	/* Construct the scanner object... */
	memset(scanner, 0, sizeof(*scanner));
        scanner->input_stream = input_stream;
        scanner->current_char = EOS;
        scanner->next_char = EOS;
        scanner->cur_pos = 0;
	scanner->nex_pos = 1;
	scanner->tokstr = g_string_new("");
	scanner->escseq = g_string_new("");
	scanner->saved_char = FALSE;
        
	scanner->n_token.state = INIT;
	scanner->n_token.c_NextChar = EOS;
	scanner->n_token.TokString = g_string_new("");

	if (XMLDEBUGSCANNER) {
		fprintf(stderr, "New scanner created");
	}
	gXML_scan(scanner);
        return scanner;
}

void
gXML_scanner_unget_char(gXML_scanner *scanner)
{
	scanner->saved_char = TRUE;
}
gint
gXML_scanner_get_next_char(gXML_scanner *scanner)
{
	if (scanner->saved_char) {
		scanner->saved_char = FALSE;
		return scanner->current_char;
	}
        
        if (scanner->cur_pos < scanner->input_stream->len) {
		scanner->current_char
		=       scanner->input_stream->str[scanner->cur_pos++];
		scanner->next_char
		=       scanner->input_stream->str[scanner->nex_pos++];
		if (XMLDEBUGSCANNER) {
			fprintf(stderr, "CUR::NEXT = '%c'::'%c'\n"
			,	scanner->current_char, scanner->next_char);
		}
		return scanner->current_char;
        }
       
	return EOS;
}
static gchar
gXMLLookupEscape(const char * s)
{

	static GHashTable*	map = NULL;
	
	int			j;

	if (map == NULL) {
		map = g_hash_table_new(g_str_hash, g_str_equal);

		for (j=0; j < DIMOF(replacements); ++j) {
			g_hash_table_insert(map
			,	g_strdup(replacements[j].escseq)
			,	GUINT_TO_POINTER(
			(guint)	replacements[j].special));
		}
	}
	return (gchar) (GPOINTER_TO_UINT(g_hash_table_lookup(map, s)));
}
