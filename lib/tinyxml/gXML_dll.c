/*
 * gXML_dll.c - GList gXML output file
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


/* gXML_dll.c */

#include "gXML.h"
#include "gXMLwrap.h"

static gboolean gXML_Glist_append(gXML_wrapper** parent, gXML_wrapper* child, GHashTable* attribs);

static void put_wrapped_item_GList(gpointer data, gpointer user_data);
/*	Precondition:  Function is passed an element in a GList
 * 	Postcondition:  Appends <elem> tags and the data element to the XML string
 */

static GString*        gXML_GList_out(gXML_wrapper* wrapper);
/*	Precondition:  Function is passed a wrapped GList
 * 	Postcondition:  returns an XML string representing the GList
 */
/* Initialize the wrapper function structure */
gXML_type GList_type = {
	"dll",
	gXML_Glist_append,
	gXML_GList_out,
};

/* Create "wrapped GList */

gXML_wrapper*
gXML_wrap_GList(GList* data)
{
	return gXML_wrap_generic(data, &GList_type);
}

GList*
gXML_unwrap_GList(gXML_wrapper*w)
{
	if (IS_GLISTTYPE(w)) {
		return w->data;
	}
	return NULL;
}


/* Step through function for doubly linked list */
static void
put_wrapped_item_GList(gpointer data, gpointer user_data)
{
	gXML_wrapper* wrapper = data;


	if (wrapper) {
		if (wrapper->identifier != WRAP_IDENT){
			g_string_append(user_data, "<OOPS (dllitem)!/>");
		}else{
			g_string_sprintfa(user_data, "<elem>%s</elem>"
			, (gXML_out(wrapper))->str);
		}
	}
}

/* doubly linked list output function */
static GString*
gXML_GList_out(gXML_wrapper* wrapper)
{
	GString *string;
	GList* list;

	if(wrapper->identifier != WRAP_IDENT){
		return g_string_new("<OOPS!/>\n");
	}
	
	list = wrapper->data;
	string = g_string_new("<dll>");
	g_list_foreach(list, put_wrapped_item_GList, string);
	string = g_string_append(string, "</dll>");
	return string;
}
static gboolean
gXML_Glist_append(gXML_wrapper** parent, gXML_wrapper* child, GHashTable* attribs)
{
	(*parent)->data = g_list_append(gXML_unwrap_GList(*parent), child);

	return TRUE;
}/* gXML_Glist_append */
