/*
 * gXML_string.c - GString gXML output file
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

#include <portability.h>
#include "gXML.h"
#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "XMLchars.h"

static GString* gXML_strencode(GString* string);
/*	Precondition:  Function is passed a GString
 * 	Postcondition:  returns a GString with any special XML characters
 *	encoded
 */


/* gXML_string.c */

static GString*
gXML_strencode(GString* string)
{
	static char specialchars [DIMOF(replacements)+1] = "\0";
	
	int	lengthleft= string->len;
	int	wherearewe = 0;
	int	nextchar;
	int	prevcharisspace = FALSE;

	if (specialchars[0] == EOS) {
		int	j;
		/* Inititalize special character map */
		for (j=0; j < DIMOF(replacements); ++j)	{
			specialchars[j] = replacements[j].special;
		}
		specialchars[j] = EOS;
	}

	if (string == NULL || string->str == NULL) {
		fprintf(stderr, "strencode: NULL string\n");
		return string;
	}

	while(lengthleft >= 0) {
		gchar		funnychar;
		char*		found;
		int		whichchar;
		int		replacelen;
		
		/* Search for the next funny character... */
		nextchar = strcspn(string->str + wherearewe, specialchars);

		/* Skip over "normal" characters */
		wherearewe += nextchar;
		lengthleft -= nextchar;

		g_assert(lengthleft >= 0);
		/* Are we there yet? */
		if (lengthleft <= 0) {
			break;
		}
		if (nextchar > 0) {
			/* We skipped over some normal chars */
			prevcharisspace = FALSE;
		}

g_assert(wherearewe < strlen(string->str));

		/* OK... We've located a funny character... Which one? */
		funnychar = string->str[wherearewe];
		if (isspace(funnychar)) {
			/*
			 * The first char can get eaten by the scanner...
			 * So can the last one
			 * So can any one preceded by an unescaped space...
			 *
			 * But the others (which is most of them) are safe
			 * This slims down our output, and speeds things up.
		 	 * Heaven knows, XML doesn't need to be slowed down or
			 * artifically fattened ;-)
			 */
			if (	wherearewe != 0		/* Not at beginning */
			&&	lengthleft > 1		/* Not at the end */
			&&	!prevcharisspace) {	/* Previous char */
							/* not a space */

				lengthleft --;
				wherearewe ++;
				prevcharisspace = TRUE;
				continue;
			}
		}

		/* Not an unescaped white space char */
		prevcharisspace = FALSE;

		found = strchr(specialchars, funnychar);
		g_assert(found != NULL);
		whichchar = found - specialchars;

		/* OK... Replace the char with '&' */
		string->str[wherearewe] = '&';			/* Whack! */

		/* Insert the replacement string here... */
		g_string_insert(string, wherearewe+1
		,	replacements[whichchar].escseq);

		replacelen = strlen(replacements[whichchar].escseq) + 1;

		/* We only ate one char from original string... */
		lengthleft -= 1;
		/* But we need to bump scanning ahead by replacelen chars ... */
		wherearewe += replacelen;
	
	}
	/*fprintf(stderr, "strencode: Returning \"%s\"\n", string->str); */
	return string;
}
gboolean
gXML_GString_append(gXML_wrapper** parent, gXML_wrapper* child, GHashTable* attribs);

/* Initialize wrapper function structure */
gXML_type GString_type = {
	"string",
	gXML_GString_append,
	gXML_GString_out,
};

/* Create wrapped GString */

gXML_wrapper*  
gXML_wrap_GString(GString* data)
{
	gXML_wrapper*	w;
	w = gXML_wrap_generic(data, &GString_type);
	return w;
}

/* GString output function */
GString*
gXML_GString_out(gXML_wrapper* wrapper)
{
	if (IS_STRINGTYPE(wrapper)) {
		return gXML_strencode
		(	g_string_new(((GString*)wrapper->data)->str));
	}
	return NULL;
}

gboolean
gXML_GString_append(gXML_wrapper** parent
,	gXML_wrapper* child, GHashTable* attribs)
{
	/* Not a collection structure... */
	return FALSE;
}/* gXML_Glist_append */
