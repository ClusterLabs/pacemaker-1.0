/* gXMLparse.h : header file for the gXML recursive descent parser for Linux-HA
 * Copyright (C) 2001 Tom Darrow <tdarrow@epsnorthwest.com>
 *  Bryan Weatherly <bryanweatherly@yahoo.com>
 *  Mike Martinez-Schiferl <mike@starfyter.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#ifndef G_XML_PARSE_H
#define G_XML_PARSE_H

#include <glib.h>
#include "gXML.h"
#include "gXMLwrap.h"
#include "gXMLscan.h"
 
gboolean gXML_struct(gXML_wrapper**, GString*);

gboolean gXML_tagset(gXML_wrapper**,gXML_scanner*,gXML_token*, gXML_wrapper** parent);

gboolean gXML_Entity_list(gXML_wrapper**, gXML_scanner*, gXML_token*, GHashTable*, gXML_wrapper** parent);
 
gboolean gXML_Attr_list(GHashTable*, gXML_scanner*, gXML_token*);
 
gXML_wrapper* gXML_create_item(GString*, GHashTable*);
 
void gXML_syntax_error(const gchar*);
 
#endif
