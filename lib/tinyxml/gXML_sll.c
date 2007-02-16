/*
 * gXML_sll.c - GSList gXML output file
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
#include "gXML.h"
#include "gXMLwrap.h"

static void put_wrapped_item_GSList(gpointer data, gpointer user_data);
/*	Precondition:  Function is passed an element in a GSList
 * 	Postcondition:  Appends <elem> tags and the data element to the XML string
 */

static GString*        gXML_GSList_out(gXML_wrapper* wrapper);
/*	Precondition:  Function is passed a wrapped GSList
 * 	Postcondition:  returns an XML string representing the GSList
 */

static gboolean gXML_GSlist_append(gXML_wrapper** parent, gXML_wrapper* child, GHashTable* attribs);

/* Initialize our wrapper function structure */
gXML_type GSList_type = {
	"sll",
	gXML_GSlist_append,
	gXML_GSList_out,
};

/* Create "wrapped" Glist singly-linked-list */
gXML_wrapper*
gXML_wrap_GSList(GSList* data)
{
	return gXML_wrap_generic(data, &GSList_type);
}

GSList*
gXML_unwrap_GSList(gXML_wrapper*w)
{
	if (IS_GSLISTTYPE(w)) {
		return w->data;
	}
	return NULL;
}

/* Singly Linked List output function */

/* Walk through function for singly linked list */
static void 
put_wrapped_item_GSList(gpointer data, gpointer user_data)
{
	gXML_wrapper*   wrapper = data;
	
	if (wrapper) {
		if (wrapper->identifier != WRAP_IDENT) {
			g_string_append(user_data, "<OOPS (item)!/>");
		}else{
			g_string_sprintfa(user_data, "<elem>%s</elem>"
			,       (gXML_out(wrapper))->str);
		}
	}
}

/* Singly linked list output function */
static GString*
gXML_GSList_out(gXML_wrapper* wrapper)
{
	GString *string;
	GSList* list;
	if (wrapper->identifier != WRAP_IDENT) {
		return  g_string_new("<OOPS!/>");
	}
	list = wrapper->data;
	string = g_string_new("<sll>");
	g_slist_foreach(list, put_wrapped_item_GSList, string);
	string = g_string_append(string, "</sll>");
	return string;
}
static gboolean
gXML_GSlist_append(gXML_wrapper** parent, gXML_wrapper* child
,	GHashTable* attribs)
{
	(*parent)->data=g_slist_append(gXML_unwrap_GSList(*parent), child);

	return TRUE;
}/* gXML_Glist_append */
