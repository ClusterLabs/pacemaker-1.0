/*
 * gXML.h - Header file for gXML output files
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

#ifndef gXML_H
#  define gXML_H
#  include <glib.h>

typedef struct _gXML_type  gXML_type;

/* Define the unique wrapper identifier */
#define WRAP_IDENT      0xFEEDFACE

/* Define wrapper structure */
typedef struct _gXML_wrapper {
	guint		identifier;
	gXML_type*	functions;
	gpointer	data;
}gXML_wrapper;

/* Define function pointers */
struct _gXML_type {
	const gchar*	type;
	/* FIXME! Need to add free object function */
	gboolean	(*append)(gXML_wrapper** parent
	,	gXML_wrapper* child, GHashTable*attrs);
	GString*	(*to_XML)(gXML_wrapper* wrapper);
};


/* generic functions (gXML.c) */
GString*        gXML_out(gXML_wrapper* wrapper);
/*	Precondition:  Function is passed any predefined wrapped Glib structure
 * 	Postcondition:  Call appropriate output function for structure
 */

gXML_wrapper*   gXML_wrap_generic(gpointer data, gXML_type* funcs);
/*	Precondition:  Function is passed an unwrapped Glib data structure
 * 	Postcondition:  returns a wrapped Glib data structure
 */


/* sll functions (gXML_sll.c) */
gXML_wrapper*   gXML_wrap_GSList(GSList* data);
/*	Precondition:  Function is passed unwrapped GSList
 * 	Postcondition:  returns a wrapped GSList
 */

/* dll functions (gXML_dll.c) */
gXML_wrapper*   gXML_wrap_GList(GList* data);
/*	Precondition:  Function is passed unwrapped GList
 * 	Postcondition:  returns a wrapped GSList
 */


/* associative array functions (gXML_aa.c) */
gXML_wrapper* gXML_wrap_GHashTable(GHashTable* data);
/*	Precondition:  Function is passed unwrapped GHashTable
 * 	Postcondition:  returns a wrapped GHashTable
 */

GString*         gXML_GHashTable_out(gXML_wrapper* wrapper);
/*	Precondition:  Function is passed a wrapped GHashTable
 * 	Postcondition:  returns an XML string representing the GHashTable
 */


void hash_elem_out(gpointer key, gpointer value, GString* string);
/*	Precondition:  Function is passed a key and a value of a GHashTable
 * 	Postcondition:  Appends <name> and <value> tags and the elements to
 *	the XML string
 */


/* string functions (gXML_string.c) */
gXML_wrapper* gXML_wrap_GString(GString* sting);
/*	Precondition:  Function is passed unwrapped GString
 * 	Postcondition:  returns a wrapped GString
 */

GString* gXML_GString_out(gXML_wrapper* wrapper);
/*	Precondition:  Function is passed a wrapped GString
 * 	Postcondition:  returns an XML string representing a GString
 */

gboolean gXML_insert_item(gXML_wrapper** parent, gXML_wrapper* child, GHashTable* attrs);


extern gXML_type GHashTable_type;
extern gXML_type GList_type;
extern gXML_type GSList_type;
extern gXML_type GString_type;

#define IS_WRAPPED(v)		\
	((v) != NULL && (((gXML_wrapper*)v)->identifier) == WRAP_IDENT)

#define IS_GHASHTABLETYPE(v)	\
	(IS_WRAPPED(v) && (((gXML_wrapper*)v)->functions) == &GHashTable_type)
#define IS_GLISTTYPE(v)		\
	(IS_WRAPPED(v) && (((gXML_wrapper*)v)->functions) == &GList_type)
#define IS_GSLISTTYPE(v)	\
	(IS_WRAPPED(v) && (((gXML_wrapper*)v)->functions) == &GSList_type)
#define IS_STRINGTYPE(v)	\
	(IS_WRAPPED(v) && (((gXML_wrapper*)v)->functions) == &GString_type)

#endif /* gXML_H */
