/*
 * gXML.c - general gXML output file
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


/* gXML.c */
#include <stdio.h>
#include <glib.h>
#include "gXML.h"
#include <stdlib.h>

/* generic output function */
GString*
gXML_out(gXML_wrapper* wrapper)
{
	if (wrapper == NULL) {
		return g_string_new("<null object>");
	}
	if (wrapper->identifier == WRAP_IDENT)  {
		if (wrapper->functions->to_XML != NULL) {
			return wrapper->functions->to_XML(wrapper);
		}else{
			return g_string_new("<NoToXMLFunction/>");
		}
	}
	return g_string_new("<NotWrappedType/>");
}

/*
 * Generic wrapper function
 */
gXML_wrapper*
gXML_wrap_generic(gpointer data, gXML_type* funcs)
{
            gXML_wrapper* wrapper;

            wrapper = (gXML_wrapper*)malloc(sizeof(gXML_wrapper));
	    memset(wrapper, 0, sizeof(*wrapper));
            wrapper->identifier = WRAP_IDENT;
            wrapper->functions = funcs;
            wrapper->data = data;
     
            return wrapper;
}
/*
 * gXML_insert_item: insert an object into the enclosing object
 *
 *	In fact, it should go through the function tables for the surrounding
 *	type and call an insert/append function associated with the tag.
 *
 *	This would allow this code to be extended without modifying this
 *	function.
 */
gboolean
gXML_insert_item(gXML_wrapper** parent
,	gXML_wrapper*	child
,	GHashTable*	attribs)
{

	if (parent == NULL) {
		fprintf(stderr, "ERROR:   gXML_insert_item: null parent\n");
		return FALSE;
	}
	if (*parent) {
		if ((*parent)->functions->append != NULL) {
			return (*parent)->functions->append(parent, child, attribs);
		}
	}else{
		*parent = child;
	}
	return FALSE;
}/* gXML_insert_item */

