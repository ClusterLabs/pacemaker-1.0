/*
 * gXML_aarray.c - Associative array gXML output file
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




/* gXML_aarray.c */

#include "gXML.h"
#include "gXMLwrap.h"
#include "stdio.h"
void hash_elem_out(gpointer key, gpointer value, GString* string);
extern gboolean XMLDEBUG;
gboolean gXML_append_GHashTable(gXML_wrapper**parent, gXML_wrapper* child, GHashTable* parentattrs);

/* Initialize the wrapper function structure */
gXML_type GHashTable_type = {
         "aarray",
	 gXML_append_GHashTable,
         gXML_GHashTable_out,
};

/* Create "wrapped GHashTable */

gXML_wrapper*
gXML_wrap_GHashTable(GHashTable* data)
{
         return gXML_wrap_generic(data, &GHashTable_type);
}

GHashTable*
gXML_unwrap_GHashTable(gXML_wrapper*w)
{
	if (IS_GHASHTABLETYPE(w)) {
		return w->data;
	}
	return NULL;
}

gboolean
gXML_append_GHashTable(gXML_wrapper**parent, gXML_wrapper* child, GHashTable* attrs)
{
	GString*	name;
	/* Hmmm... */
	/* It seems that this requires it to have the name in
	 * the attribute list...
	 * I didn't think that's how this code worked...
	 * Not that it doesn't make sense, but that it isn't
	 * general enough to handle XML-RPC...
	 *
	 */
	if ((name = g_hash_table_lookup(attrs, "name")) != NULL) {
		g_assert(g_hash_table_lookup(gXML_unwrap_GHashTable(*parent), name->str) == NULL);
		g_hash_table_insert(gXML_unwrap_GHashTable(*parent)
		,	name->str, child);
		if (XMLDEBUG) {
			fprintf(stderr, "Added key '%s' to Hash Table\n"
			,	name->str);
		}
		return TRUE;
	}else{
		fprintf(stderr
		,	"ERROR: Cannot find value of 'name' in attributes");
		g_assert_not_reached();
	}
	return FALSE;
}

/* Associative Array output function */
GString*
gXML_GHashTable_out(gXML_wrapper* wrapper)
{
  GString *string = g_string_new("<aarray>");

	g_assert(wrapper->functions == &GHashTable_type);
	g_hash_table_foreach(wrapper->data, (GHFunc)hash_elem_out, string);

	string = g_string_append(string, "</aarray>");

	return string;
} /* gXML_GHashTable_out */

void
hash_elem_out(gpointer key, gpointer value, GString* string)
{
	GString * gsvalue = gXML_out(value);

	g_string_append(string, "<elem name=\"");
	g_string_append(string, ((gchar*)key));
	g_string_append(string, "\">");
	g_string_append(string, gsvalue->str);
	g_string_append(string, "</elem>");

	g_string_free(gsvalue, TRUE);
}/* hash_elem_out */
