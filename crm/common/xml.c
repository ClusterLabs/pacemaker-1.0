/* $Id: xml.c,v 1.38 2005/02/25 10:28:00 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <clplumbing/ipc.h>
#include <clplumbing/cl_log.h> 

#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/dmalloc_wrapper.h>


void dump_array(
	int log_level, const char *message, const char **array, int depth);

int print_spaces(char *buffer, int spaces);

int log_data_element(
	const char *function, int log_level, int depth, crm_data_t *data, gboolean formatted);

int dump_data_element(
	int depth, char **buffer, crm_data_t *data, gboolean formatted);

#ifndef USE_LIBXML

crm_data_t *parse_xml(const char *input, int *offset);
int get_tag_name(const char *input);
int get_attr_name(const char *input);
int get_attr_value(const char *input);

#endif

crm_data_t *
find_xml_node(crm_data_t *root, const char * search_path, gboolean must_find)
{
	if(must_find || root != NULL) {
		crm_validate_data(root);
	}
	
	if(search_path == NULL) {
		crm_warn("Will never find <NULL>");
		return NULL;
	}
	
	xml_child_iter(
		root, a_child, search_path,
/* 		crm_insane("returning node (%s).", xmlGetNodePath(a_child)); */
		crm_xml_insane(a_child, "contents\t%s");
		crm_xml_insane(root, "found in\t%s");
		crm_validate_data(a_child);
		return a_child;
		);

	if(must_find) {
		crm_warn("Could not find %s in %s.", search_path, xmlGetNodePath(root));
	} else if(root != NULL) {
		crm_devel("Could not find %s in %s.", search_path, xmlGetNodePath(root));
	} else {
		crm_devel("Could not find %s in <NULL>.", search_path);
	}
	
	
	return NULL;
}

crm_data_t*
find_xml_node_nested(crm_data_t *root, const char **search_path, int len)
{
	int	j;
	gboolean is_found = TRUE;
	crm_data_t *match =  NULL;
	crm_data_t *lastMatch = root;

	crm_validate_data(root);
	
	if(search_path == NULL || search_path[0] == NULL) {
		crm_warn("Will never find NULL");
		return NULL;
	}
	
	dump_array(LOG_INSANE, "Looking for.", search_path, len);

	for (j=0; j < len; ++j) {
		if (search_path[j] == NULL) {
/* a NULL also means stop searching */
			break;
		}

		match = find_xml_node(lastMatch, search_path[j], FALSE);
		if(match == NULL) {
			is_found = FALSE;
			break;
		} else {
			lastMatch = match;
		}
	}

	if (is_found) {
		crm_insane("returning node (%s).",
			   xmlGetNodePath(lastMatch));

		crm_xml_insane(lastMatch, "found\t%s");
		crm_xml_insane(root, "in \t%s");
		
		crm_validate_data(lastMatch);
		return lastMatch;
	}

	dump_array(LOG_WARNING,
		   "Could not find the full path to the node you specified.",
		   search_path, len);

	crm_warn("Closest point was node (%s) starting from %s.",
		 xmlGetNodePath(lastMatch), crm_element_name(root));

	return NULL;
    
}



const char *
get_xml_attr_nested(crm_data_t *parent,
		    const char **node_path, int length,
		    const char *attr_name, gboolean error)
{
	const char *attr_value = NULL;
	crm_data_t *attr_parent = NULL;

	if(error || parent != NULL) {
		crm_validate_data(parent);
	}
	
	if(parent == NULL) {
		crm_devel("Can not find attribute %s in NULL parent",attr_name);
		return NULL;
	} 

	if(attr_name == NULL || strlen(attr_name) == 0) {
		crm_err("Can not find attribute with no name in %s",
		       xmlGetNodePath(parent));
		return NULL;
	}
	
	if(length == 0) {
		attr_parent = parent;
		
	} else {
		attr_parent = find_xml_node_nested(parent, node_path, length);
		if(attr_parent == NULL && error) {
			crm_err("No node at the path you specified.");
			return NULL;
		}
	}
	
	attr_value = crm_element_value(attr_parent, attr_name);
	if((attr_value == NULL || strlen(attr_value) == 0) && error) {
		crm_err(
		       "No value present for %s at %s",
		       attr_name, xmlGetNodePath(attr_parent));
		return NULL;
	}
	
	return attr_value;
}


crm_data_t*
find_entity(crm_data_t *parent,
	    const char *node_name,
	    const char *id,
	    gboolean siblings)
{
	crm_validate_data(parent);
	xml_child_iter(
		parent, a_child, node_name,
		if(id == NULL 
		   || safe_str_eq(id,crm_element_value(a_child,XML_ATTR_ID))){
			crm_devel("returning node (%s).", 
				  xmlGetNodePath(a_child));
			return a_child;
		}
		);
	if(siblings) {
		abort();
	}

	crm_warn("node <%s id=%s> not found in %s.",
		 node_name, id, xmlGetNodePath(parent));
	return NULL;
}

void
copy_in_properties(crm_data_t* target, crm_data_t *src)
{
	crm_validate_data(src);
	crm_validate_data(target);
	if(src == NULL) {
		crm_warn("No node to copy properties from");
	} else if (target == NULL) {
		crm_err("No node to copy properties into");
	} else {
		xml_prop_iter(
			src, local_prop_name, local_prop_value,
			set_xml_property_copy(
				target, local_prop_name, local_prop_value);
			);
		crm_validate_data(target);
	}
	
	return;
}

crm_data_t*
add_node_copy(crm_data_t *new_parent, crm_data_t *xml_node)
{
	crm_data_t *node_copy = NULL;
	
	crm_validate_data(new_parent);
	crm_validate_data(xml_node);
		
	if(xml_node != NULL && new_parent != NULL) {
#ifdef USE_LIBXML
		node_copy = copy_xml_node_recursive(xml_node);
		xmlAddChild(new_parent, node_copy);
#else
		const char *name = crm_element_name(xml_node);
		CRM_DEV_ASSERT(HA_OK == ha_msg_addstruct(
				       new_parent, name, xml_node));
		
		node_copy = find_entity(new_parent, crm_element_name(xml_node), ID(xml_node), FALSE);
		crm_validate_data(node_copy);
		crm_update_parents(new_parent);
		crm_validate_data(new_parent);
#endif

	} else if(xml_node == NULL) {
		crm_err("Could not add copy of NULL node");

	} else {
		crm_err("Could not add copy of node to NULL parent");
	}
	
	crm_validate_data(node_copy);
	return node_copy;
}

const char *
set_xml_property_copy(crm_data_t* node, const char *name, const char *value)
{
	const char *parent_name = NULL;

	if(node != NULL) {
		parent_name = crm_element_name(node);
	}
	
	crm_insane("[%s] Setting %s to %s", crm_str(parent_name), name, value);

	if (name == NULL || strlen(name) <= 0) {
		
	} else if(node == NULL) {
		
	} else if(parent_name == NULL && strcmp(name, F_XML_TAGNAME) != 0) {
		
	} else if (value == NULL || strlen(value) <= 0) {
		xml_remove_prop(node, name);
		return NULL;
		
	} else {
#ifdef USE_LIBXML
		const char *local_name = NULL;
		const char *local_value = NULL;
		local_value = crm_strdup(value);
		local_name = crm_strdup(name);
		xmlUnsetProp(node, local_name);
		xmlSetProp(node, local_name, local_value);
		return xmlGetProp(node, local_name);
#else
		crm_validate_data(node);
		ha_msg_mod(node, name, value);
		return crm_element_value(node, name);
#endif
	}
	
	return NULL;
}

crm_data_t*
create_xml_node(crm_data_t *parent, const char *name)
{
	const char *local_name = NULL;
	const char *parent_name = NULL;
	crm_data_t *ret_value = NULL;
	

	if (name == NULL || strlen(name) < 1) {
		ret_value = NULL;
	} else {
#ifdef USE_LIBXML
		local_name = crm_strdup(name);

		if(parent == NULL) 
			ret_value = xmlNewNode(NULL, local_name);
		else {
			parent_name = parent->name;
			ret_value =
				xmlNewChild(parent, NULL, local_name, NULL);
		}
#else
		local_name = name;
		ret_value = ha_msg_new(1);
		CRM_DEV_ASSERT(ret_value != NULL);
		
		set_xml_property_copy(ret_value, XML_ATTR_TAGNAME, name);
		crm_validate_data(ret_value);
		if(parent) {
			crm_validate_data(parent);
			parent_name = crm_element_name(parent);
			crm_insane("Attaching %s to parent %s",
				   local_name, parent_name);
			CRM_DEV_ASSERT(HA_OK == ha_msg_addstruct(
					   parent, name, ret_value));
			crm_msg_del(ret_value);

			crm_update_parents(parent);
			crm_validate_data(parent);
			ret_value = parent->values[parent->nfields-1];
			crm_validate_data(ret_value);
		}
#endif
	}

	crm_insane("Created node [%s [%s]]",
		  crm_str(parent_name), crm_str(local_name));
/*	set_node_tstamp(ret_value); */
	return ret_value;
}

void
free_xml_from_parent(crm_data_t *parent, crm_data_t *a_node)
{
	CRM_DEV_ASSERT(parent != NULL);
	if(parent == NULL) {
		return;
	} else if(a_node == NULL) {
		return;
	}
	crm_validate_data(parent);
#ifdef USE_LIBXML	
	xmlUnlinkNode(a_node);
	node->doc = NULL;
	free_xml_fn(a_node);
#else
	cl_msg_remove_value(parent, a_node);	
#endif
	crm_validate_data(parent);
}


void
free_xml_fn(crm_data_t *a_node)
{
        if(a_node == NULL) {
		; /*  nothing to do */
#ifdef USE_LIBXML	
	} else if (a_node->doc != NULL) {
		xmlFreeDoc(a_node->doc);
	} else {
		/* make sure the node is unlinked first */
		xmlUnlinkNode(a_node);
		xmlFreeNode(a_node);
#else
	} else {
		int has_parent = 0;
		crm_validate_data(a_node);
		ha_msg_value_int(a_node, XML_ATTR_PARENT, &has_parent);

		/* there is no way in hell we should be deleting anything
		 * with a parent and without the parent knowning
		 */
		CRM_DEV_ASSERT(has_parent == 0);
		if(has_parent == 0) {
			crm_validate_data(a_node);
			crm_msg_del(a_node);
		}
#endif
	}
	
	return;
}

void
set_node_tstamp(crm_data_t *a_node)
{
	char *since_epoch = NULL;
	time_t a_time = time(NULL);
	
	crm_validate_data(a_node);

	if(a_time == (time_t)-1) {
		cl_perror("set_node_tstamp(): Invalid time returned");
		return;
	}
	
	crm_malloc(since_epoch, 128*(sizeof(char)));
	if(since_epoch != NULL) {
		sprintf(since_epoch, "%ld", (unsigned long)a_time);
#ifdef USE_LIBXML
		xmlUnsetProp(a_node, XML_ATTR_TSTAMP);
		xmlSetProp(a_node, XML_ATTR_TSTAMP, since_epoch);
#else
		ha_msg_mod(a_node, XML_ATTR_TSTAMP, since_epoch);
		crm_validate_data(a_node);
		crm_free(since_epoch);
#endif
	}
}

crm_data_t*
copy_xml_node_recursive(crm_data_t *src_node)
{
	crm_data_t *new_xml = NULL;
	
#ifdef USE_LIBXML
#   if 1
	return xmlCopyNode(src_node, 1);
#   else
	xmlNodePtr local_node = NULL, local_child = NULL;

	if(src_node == NULL || src_node->name == NULL) {
		return NULL;
	}
	
	local_node = create_xml_node(NULL, src_node->name);

	copy_in_properties(local_node, src_node);
	
	xml_child_iter(
		src_node, node_iter, NULL,
		local_child = copy_xml_node_recursive(node_iter);
		if(local_child != NULL) {
			xmlAddChild(local_node, local_child);
			crm_insane("Copied node [%s [%s]",
				   local_node->name, local_child->name);
		}
		);
	
	crm_insane("Returning [%s]", local_node->name);
	return local_node;
#   endif		
#else
	CRM_DEV_ASSERT(src_node != NULL);
	CRM_DEV_ASSERT(crm_element_name(src_node) != NULL);

	if(src_node == NULL) {
		crm_warn("Attempt to dup NULL XML");
		return NULL;
		
	} else if(crm_element_name(src_node) == NULL) {
		crm_xml_err(src_node, "Attempt to dup XML with no name");
		return NULL;
	}
	
	crm_validate_data(src_node);
	new_xml = ha_msg_copy(src_node);
	crm_set_element_parent(new_xml, NULL);
	crm_update_parents(new_xml);
	crm_validate_data(new_xml);
#endif
	return new_xml;
}


crm_data_t*
string2xml(const char *input)
{
#ifdef USE_LIBXML
	int lpc = 0;
	char ch = 0;
	int input_len = 0;
	gboolean more = TRUE;
	gboolean inTag = FALSE;
	crm_data_t *xml_object = NULL;
	const char *the_xml;
	xmlDocPtr doc;
	xmlBufferPtr xml_buffer = NULL;

	if(input == NULL || (input_len = strlen(input)) < 0) {
		return NULL;
	}
	
	xml_buffer = xmlBufferCreate();
	
	for(lpc = 0; (lpc < input_len) && more; lpc++) {
		ch = input[lpc];
		switch(ch) {
			case EOF: 
			case 0:
				ch = 0;
				more = FALSE; 
				xmlBufferAdd(xml_buffer, &ch, 1);
				break;
			case '>':
			case '<':
				inTag = TRUE;
				if(ch == '>') inTag = FALSE;
				xmlBufferAdd(xml_buffer, &ch, 1);
				break;
			case '\n':
			case '\t':
			case ' ':
				ch = ' ';
				if(inTag) {
					xmlBufferAdd(xml_buffer, &ch, 1);
				} 
				break;
			default:
				xmlBufferAdd(xml_buffer, &ch, 1);
				break;
		}
	}

	
	xmlInitParser();
	the_xml = xmlBufferContent(xml_buffer);
	doc = xmlParseMemory(the_xml, strlen(the_xml));
	xmlCleanupParser();

	if (doc == NULL) {
		crm_err("Malformed XML [xml=%s]", the_xml);
		xmlBufferFree(xml_buffer);
		return NULL;
	}

	xmlBufferFree(xml_buffer);
	xml_object = xmlDocGetRootElement(doc);

	return xml_object;
#else
	crm_data_t *output = parse_xml(input, NULL);
	if(output != NULL) {
		crm_update_parents(output);
		crm_validate_data(output);
	}
	return output;
#endif	
}

crm_data_t *
stdin2xml(void) 
{
#ifdef USE_LIBXML
	return file2xml(stdin);
#else
	int lpc = 0;
	int MAX_XML_BUFFER = 20000;
	
	char ch = 0;
	gboolean more = TRUE;
	gboolean inTag = FALSE;
	FILE *input = stdin;

	char *xml_buffer = NULL;
	crm_data_t *xml_obj = NULL;

	crm_malloc(xml_buffer, sizeof(char)*(MAX_XML_BUFFER+1));
	
	while (more && lpc < MAX_XML_BUFFER) {
		ch = fgetc(input);
/* 		crm_devel("Got [%c]", ch); */
		switch(ch) {
			case EOF: 
			case 0:
				ch = 0;
				more = FALSE; 
				xml_buffer[lpc++] = ch;
				break;
			case '>':
			case '<':
				inTag = TRUE;
				if(ch == '>') inTag = FALSE;
				xml_buffer[lpc++] = ch;
				break;
			case '\n':
			case '\t':
			case ' ':
				ch = ' ';
				if(inTag) {
					xml_buffer[lpc++] = ch;
				} 
				break;
			default:
				xml_buffer[lpc++] = ch;
				break;
		}
	}
	
	xml_buffer[MAX_XML_BUFFER] = 0;
	xml_obj = string2xml(xml_buffer);
	crm_free(xml_buffer);

	crm_xml_devel(xml_obj, "Created fragment");
	return xml_obj;
#endif
}


crm_data_t*
file2xml(FILE *input)
{
	
#ifdef USE_LIBXML
	char ch = 0;
	gboolean more = TRUE;
	gboolean inTag = FALSE;
	crm_data_t *xml_object = NULL;
	xmlBufferPtr xml_buffer = xmlBufferCreate();
	const char *the_xml;
	xmlDocPtr doc;

	if(input == NULL) {
		crm_err("File pointer was NULL");
		return NULL;
	}
	
	while (more) {
		ch = fgetc(input);
/* 		crm_devel("Got [%c]", ch); */
		switch(ch) {
			case EOF: 
			case 0:
				ch = 0;
				more = FALSE; 
				xmlBufferAdd(xml_buffer, &ch, 1);
				break;
			case '>':
			case '<':
				inTag = TRUE;
				if(ch == '>') inTag = FALSE;
				xmlBufferAdd(xml_buffer, &ch, 1);
				break;
			case '\n':
			case '\t':
			case ' ':
				ch = ' ';
				if(inTag) {
					xmlBufferAdd(xml_buffer, &ch, 1);
				} 
				break;
			default:
				xmlBufferAdd(xml_buffer, &ch, 1);
				break;
		}
	}

	xmlInitParser();
	the_xml = xmlBufferContent(xml_buffer);
	doc = xmlParseMemory(the_xml, strlen(the_xml));
	xmlCleanupParser();
	
	if (doc == NULL) {
		crm_err("Malformed XML [xml=%s]", the_xml);
		xmlBufferFree(xml_buffer);
		return NULL;
	}
	xmlBufferFree(xml_buffer);
	xml_object = xmlDocGetRootElement(doc);

	crm_xml_devel(xml_object, "Created fragment");

	return xml_object;
#else
	char *buffer = NULL;
	crm_data_t *new_obj = NULL;
	int start = 0, length = 0, read_len = 0;
	
	/* see how big the file is */
	start  = ftell(input);
	fseek(input, 0L, SEEK_END);
	length = ftell(input);
	fseek(input, 0L, start);
	
	if(start != ftell(input)) {
		crm_err("fseek not behaving");
		return NULL;
	}

	crm_devel("Reading %d bytes from file", length);
	crm_malloc(buffer, sizeof(char) * (length+1));
	read_len = fread(buffer, sizeof(char), length, input);
	if(read_len != length) {
		crm_err("Calculated and read bytes differ: %d vs. %d",
			length, read_len);
	} else  if(length > 0) {
		new_obj = string2xml(buffer);
	} else {
		crm_warn("File contained no XML");
	}
	
	crm_free(buffer);
	return new_obj;
#endif	
}

void
dump_array(int log_level, const char *message, const char **array, int depth)
{
	int j;
	
	if(message != NULL) {
		do_crm_log(log_level, __FUNCTION__, NULL, "%s", message);
	}

	do_crm_log(log_level, __FUNCTION__, NULL,  "Contents of the array:");
	if(array == NULL || array[0] == NULL || depth == 0) {
		do_crm_log(log_level, __FUNCTION__, NULL, "\t<empty>");
		return;
	}
	
	for (j=0; j < depth && array[j] != NULL; j++) {
		if (array[j] == NULL) { break; }
		do_crm_log(log_level, __FUNCTION__, NULL, "\t--> (%s).", array[j]);
	}
}

int
write_xml_file(crm_data_t *xml_node, const char *filename) 
{
	int res = 0;
	char now_str[30];
	time_t now;

	crm_devel("Writing XML out to %s", filename);
	crm_validate_data(xml_node);
	if (xml_node == NULL) {
		return -1;
	}

	crm_validate_data(xml_node);
	crm_xml_devel(xml_node, "Writing out");
	crm_validate_data(xml_node);
	
	now = time(NULL);
	ctime_r(&now, now_str);
	now_str[24] = EOS; /* replace the newline */
	set_xml_property_copy(xml_node, "last_written", now_str);
	crm_validate_data(xml_node);
	
#ifdef USE_LIBXML
	if (xml_node->doc == NULL) {
		xmlDocPtr foo = NULL;
		crm_insane("Creating doc pointer for %s", xml_node->name);
		foo = xmlNewDoc("1.0");
		xmlDocSetRootElement(foo, xml_node);
		xmlSetTreeDoc(xml_node, foo);
	}

	/* save it.
	 * set arg 3 to 0 to disable line breaks,1 to enable
	 * res == num bytes saved
	 */
	res = xmlSaveFormatFile(filename, xml_node->doc, 1);
	/* for some reason, reading back after saving with
	 * line-breaks doesnt go real well 
	 */
#else
	{
		FILE *file_output_strm = fopen(filename, "w");
		char *buffer = dump_xml_formatted(xml_node);
		CRM_DEV_ASSERT(buffer != NULL && strlen(buffer) > 0);
		if(buffer != NULL && strlen(buffer) > 0) {
			res = fprintf(file_output_strm, "%s", buffer);
			fflush(file_output_strm);
		}
		fclose(file_output_strm);
		crm_free(buffer);
	}
#endif
	crm_devel("Saved %d bytes to the Cib as XML", res);

	return res;
}

void
print_xml_formatted(int log_level, const char *function,
		    crm_data_t *msg, const char *text)
{
	if(msg == NULL) {
		do_crm_log(log_level, function, NULL, "%s: %s",
			   crm_str(text), "<null>");
		return;
	}

	crm_validate_data(msg);
	do_crm_log(log_level, function, NULL, "%s:",
		   crm_str(text));
	log_data_element(function, log_level, 0, msg, TRUE);
	return;
}

crm_data_t *
get_message_xml(const HA_Message *msg, const char *field) 
{
	crm_data_t *xml_node = NULL;
#ifdef USE_LIBXML
	const char *xml_text = cl_get_string(msg, field);
	xml_node = string2xml(xml_text);
#else
	crm_data_t *tmp_node = NULL;
	crm_validate_data(msg);
	tmp_node = cl_get_struct(msg, field);
	if(tmp_node != NULL) {
		xml_node = copy_xml_node_recursive(tmp_node);
	}
#endif
	return xml_node;
}

gboolean
add_message_xml(HA_Message *msg, const char *field, crm_data_t *xml) 
{
	crm_validate_data(xml);
#ifdef USE_LIBXML
	char *buffer = dump_xml_unformatted(xml);
	CRM_DEV_ASSERT(buffer != NULL);
	if(buffer != NULL) {
		CRM_DEV_ASSERT(cl_is_allocated(buffer));
		ha_msg_add(msg, field, buffer);
		crm_devel("Added XML to message");
		CRM_DEV_ASSERT(cl_is_allocated(buffer));
		crm_free(buffer);
	}
#else
	crm_validate_data(msg);
	ha_msg_addstruct(msg, field, xml);
	crm_update_parents(msg);
#endif
	return TRUE;
}


char *
dump_xml_formatted(crm_data_t *an_xml_node)
{
	char *buffer     = NULL;
#if 0
	int       len        = 0;
	xmlChar  *xml_buffer = NULL;
	xmlDocPtr foo        = NULL;

	crm_data_t* xml_node  = NULL;

	xml_node  = copy_xml_node_recursive(an_xml_node);
	
	if (xml_node == NULL) {
		return NULL;
		
	} else {
		/* reset the doc pointer */
		crm_insane("Creating doc pointer for %s", xml_node->name);
		foo = xmlNewDoc("1.0");
		xmlDocSetRootElement(foo, xml_node);
		xmlSetTreeDoc(xml_node, foo);
		crm_insane("Doc pointer set for %s", xml_node->name);
	}

	crm_insane("Initializing Parser");
	xmlInitParser();
	crm_insane("Dumping data");
	xmlDocDumpFormatMemory(xml_node->doc, &xml_buffer, &len, 1);
	crm_insane("Cleaning up parser");
	xmlCleanupParser();

	crm_insane("Copying memory into crm_ space");
	if(xml_buffer != NULL && len > 0) {
		/* copy the text into crm_ memory */ 
		buffer = crm_strdup(xml_buffer);
		xmlFree(xml_buffer);
	}
	crm_insane("Buffer coppied");
	
	free_xml(xml_node);
#else
	char *mutable_ptr = NULL;
/* 	crm_malloc(buffer, 2*(an_xml_node->stringlen)); */
	crm_malloc(buffer, sizeof(char)*30000);
	mutable_ptr = buffer;
	
	crm_validate_data(an_xml_node);
	CRM_DEV_ASSERT(dump_data_element(
			       0, &mutable_ptr, an_xml_node, TRUE) >= 0);
	if(crm_assert_failed) {
		crm_crit("Could not dump the whole message");
	}
	crm_trace("Dumped: %s", buffer);
#endif
	return buffer;
}
	
char *
dump_xml_unformatted(crm_data_t *an_xml_node)
{
	char *buffer     = NULL;
#if 0
	int       lpc	     = 0;
	int       len        = 0;
	xmlChar  *xml_buffer = NULL;
	xmlDocPtr foo        = NULL;

	crm_data_t* xml_node  = NULL;

	xml_node  = copy_xml_node_recursive(an_xml_node);
	
	if (xml_node == NULL) {
		return NULL;
		
	} else {
	  /* reset the doc pointer */
		crm_insane("Creating doc pointer for %s", xml_node->name);
		foo = xmlNewDoc("1.0");
		xmlDocSetRootElement(foo, xml_node);
		xmlSetTreeDoc(xml_node, foo);
		crm_insane("Doc pointer set for %s", xml_node->name);
	}

	crm_insane("Initializing Parser");
	xmlInitParser();
	crm_insane("Dumping data");
	xmlDocDumpFormatMemory(xml_node->doc, &xml_buffer, &len, 0);
	crm_insane("Cleaning up parser");
	xmlCleanupParser();

	crm_insane("Copying memory into crm_ space");
	if(xml_buffer != NULL && len > 0) {
		/* copy the text into crm_ memory */ 
		buffer = crm_strdup(xml_buffer);
		xmlFree(xml_buffer);
	}
	crm_insane("Buffer coppied");
	
	free_xml(xml_node);

	/* remove <?xml version="1.0"?> and the newline	 */
/* 	for(lpc = 0; lpc < len; lpc++) { */
/* 		if(buffer[lpc] == '\n') { */
/* 			buffer[lpc] = ' '; */
/* 			break; */
/* 		} else { */
/* 			buffer[lpc] = ' '; */
/* 		} */
/* 	} */
/* 	for(lpc = len - 2; lpc > 0 && lpc < len; lpc++) { */
	for(lpc = 0; lpc < len; lpc++) {
		if(buffer[lpc] == '\n') {
			crm_devel("Reset newline at %d", lpc);
			buffer[lpc] = ' ';
		}
	}
	crm_devel("Processed %d chars for newlines", lpc);
#else
	char *mutable_ptr = NULL;
/* 	crm_malloc(buffer, 2*(an_xml_node->stringlen)); */
	crm_malloc(buffer, sizeof(char)*20000);
	mutable_ptr = buffer;
	
	crm_validate_data(an_xml_node);
	CRM_DEV_ASSERT(dump_data_element(
			       0, &mutable_ptr, an_xml_node, TRUE) >= 0);
	if(crm_assert_failed) {
		crm_crit("Could not dump the whole message");
	}

	crm_trace("Dumped: %s", buffer);
#endif
	return buffer;
}

#define update_buffer_head(buffer, len) if(len < 0) {	\
		(*buffer) = EOS; return -1;		\
	} else {					\
		buffer += len;				\
	}


int
print_spaces(char *buffer, int depth) 
{
	int lpc = 0;
	int spaces = 2*depth;
	/* <= so that we always print 1 space - prevents problems with syslog */
	for(lpc = 0; lpc <= spaces; lpc++) {
		if(sprintf(buffer, "%c", ' ') < 1) {
			return -1;
		}
		buffer += 1;
	}
	return lpc;
}

int
log_data_element(
	const char *function, int log_level, int depth,
	crm_data_t *data, gboolean formatted) 
{
	int printed = 0;
	int child_result = 0;
	int has_children = 0;
	char print_buffer[1000];
	char *buffer = print_buffer;
	const char *name = crm_element_name(data);

	crm_insane("Dumping %s...", name);
	crm_validate_data(data);
	if(data == NULL) {
		crm_warn("No data to dump as XML");
		return 0;

	} else if(name == NULL && depth == 0) {
		xml_child_iter(
			data, a_child, NULL,
			child_result = log_data_element(
				function, log_level, depth, a_child, formatted);
			if(child_result < 0) {
				return child_result;
			}
			);
		return 0;

	} else if(name == NULL) {
		crm_err("Cannot dump NULL element at depth %d", depth);
		return -1;
	}
	
	if(formatted) {
		printed = print_spaces(buffer, depth);
		update_buffer_head(buffer, printed);
	}
	
	printed = sprintf(buffer, "<%s", name);
	update_buffer_head(buffer, printed);

	xml_prop_iter(
		data, prop_name, prop_value,

		if(safe_str_eq(XML_ATTR_TAGNAME, prop_name)) {
			continue;
		} else if(safe_str_eq(XML_ATTR_PARENT, prop_name)) {
			continue;
		}
		
		crm_insane("Dumping <%s %s=\"%s\"...",
			  name, prop_name, prop_value);
		printed = sprintf(buffer, " %s=\"%s\"", prop_name, prop_value);
		update_buffer_head(buffer, printed);
		);

	xml_child_iter(
		data, child, NULL,
		if(child != NULL) {
			has_children++;
			break;
		}
		);

	printed = sprintf(buffer, "%s>", has_children==0?"/":"");
	update_buffer_head(buffer, printed);
	do_crm_log(log_level,  function, NULL, "%s", print_buffer);
	buffer = print_buffer;
	
	if(has_children == 0) {
		return 0;
	}
	
	xml_child_iter(
		data, a_child, NULL,
		child_result = log_data_element(
			function, log_level, depth+1, a_child, formatted);

		if(child_result < 0) { return -1; }
		);

	if(formatted) {
		printed = print_spaces(buffer, depth);
		update_buffer_head(buffer, printed);
	}
	do_crm_log(log_level, function, NULL, "%s</%s>",
		   print_buffer, name);
	crm_insane("Dumped %s...", name);

	return has_children;
}


int
dump_data_element(
	int depth, char **buffer,  crm_data_t *data, gboolean formatted) 
{
	int printed = 0;
	int child_result = 0;
	int has_children = 0;
	const char *name = crm_element_name(data);

	crm_insane("Dumping %s...", name);
	crm_validate_data(data);
	if(buffer == NULL || *buffer == NULL) {
		crm_err("No buffer supplied to dump XML into");
		return -1;

	} else if(data == NULL) {
		crm_warn("No data to dump as XML");
		(*buffer)[0] = EOS;
		return 0;

	} else if(name == NULL && depth == 0) {
		xml_child_iter(
			data, a_child, NULL,
			child_result = dump_data_element(
				depth, buffer, a_child, formatted);
			if(child_result < 0) {
				return child_result;
			}
			);
		return 0;

	} else if(name == NULL) {
		crm_err("Cannot dump NULL element at depth %d", depth);
		return -1;
	}
	
	if(formatted) {
		printed = print_spaces(*buffer, depth);
		update_buffer_head(*buffer, printed);
	}
	
	printed = sprintf(*buffer, "<%s", name);
	update_buffer_head(*buffer, printed);

	xml_prop_iter(data, prop_name, prop_value,
			if(safe_str_eq(XML_ATTR_TAGNAME, prop_name)) {
				continue;
			} else if(safe_str_eq(XML_ATTR_PARENT, prop_name)) {
				continue;
			}
			crm_insane("Dumping <%s %s=\"%s\"...",
			  name, prop_name, prop_value);
			printed = sprintf(*buffer, " %s=\"%s\"", prop_name, prop_value);
			update_buffer_head(*buffer, printed);
		);
	
	xml_child_iter(
		data, child, NULL,
		if(child != NULL) {
			has_children++;
			break;
		}
		);

	printed = sprintf(*buffer, "%s>%s",
			  has_children==0?"/":"", formatted?"\n":"");
	update_buffer_head(*buffer, printed);

	if(has_children == 0) {
		return 0;
	}
	
	xml_child_iter(
		data, child, NULL,
		child_result = dump_data_element(
			depth+1, buffer, child, formatted);

		if(child_result < 0) { return -1; }
		);

	if(formatted) {
		printed = print_spaces(*buffer, depth);
		update_buffer_head(*buffer, printed);
	}
	printed = sprintf(*buffer, "</%s>%s", name, formatted?"\n":"");
	update_buffer_head(*buffer, printed);
	crm_insane("Dumped %s...", name);

	return has_children;
}

gboolean
xml_has_children(crm_data_t *xml_root)
{
	crm_validate_data(xml_root);

#ifdef USE_LIBXML
	if(xml_root != NULL && xml_root->children != NULL) {
		return TRUE;
	}
#else
	xml_child_iter(
		xml_root, a_child, NULL,
		return TRUE;
		);
#endif
	return FALSE;
}


void
crm_validate_data(const crm_data_t *xml_root)
{
#ifdef USE_LIBXML
	CRM_DEV_ASSERT(xml_root != NULL);
#else
#  ifndef XML_PARANOIA_CHECKS
	CRM_DEV_ASSERT(xml_root != NULL);
#  else
	int lpc = 0;
	CRM_ASSERT(xml_root != NULL);
	CRM_ASSERT(cl_is_allocated(xml_root) == 1);
	CRM_ASSERT(xml_root->nfields < 500);
	
	for (lpc = 0; lpc < xml_root->nfields; lpc++) {
		void *child = xml_root->values[lpc];
		CRM_ASSERT(cl_is_allocated(xml_root->names[lpc]) == 1);

		if(child == NULL) {
			
		} else if(xml_root->types[lpc] == FT_STRUCT) {
			crm_validate_data(child);
			
		} else if(xml_root->types[lpc] == FT_STRING) {
			CRM_ASSERT(cl_is_allocated(child) == 1);
/* 		} else { */
/* 			CRM_DEV_ASSERT(FALSE); */
		}
	}
#  endif
#endif
}


/* FIXME!! This whole function is evil!! */
void
crm_set_element_parent(crm_data_t *data, crm_data_t *parent)
{
#ifdef USE_LIBXML
	CRM_DEV_ASSERT(FALSE/* not implemented*/);  
#else
	crm_validate_data(data);
	if(parent != NULL) {
		ha_msg_mod_int(data, XML_ATTR_PARENT, 1);
		
	} else if(cl_get_string(data, XML_ATTR_PARENT) != NULL) {
		ha_msg_mod_int(data, XML_ATTR_PARENT, 0);
	}
#endif
}

const char *
crm_element_value(crm_data_t *data, const char *name)
{
#ifdef USE_LIBXML
	return xmlGetProp(data, name);
#else
	const char *value = NULL;
	crm_validate_data(data);
	value = cl_get_string(data, name);
	if(value != NULL) {
		cl_is_allocated(value);
	}
	return value;
#endif
}

char *
crm_element_value_copy(crm_data_t *data, const char *name)
{
	const char *value = NULL;
	char *value_copy = NULL;
#ifdef USE_LIBXML
	value = xmlGetProp(data, name);
#else
	crm_validate_data(data);
	value = cl_get_string(data, name);
	if(value != NULL) {
		cl_is_allocated(value);
	}
#endif
 	CRM_DEV_ASSERT(value != NULL);
	if(value != NULL) {
		value_copy = crm_strdup(value);
	}
	return value_copy;
}

const char *
crm_element_name(crm_data_t *data)
{
#ifdef USE_LIBXML
	return (data ? data->name : NULL);
#else
	crm_validate_data(data);
	return cl_get_string(data, XML_ATTR_TAGNAME);
#endif
}

void
xml_remove_prop(crm_data_t *obj, const char *name)
{
#ifdef USE_LIBXML
	xmlUnsetProp(obj, name);
#else
	if(crm_element_value(obj, name) != NULL) {
		cl_msg_remove(obj, name);
	}
#endif
}

void
crm_update_parents(crm_data_t *xml_root)
{
#ifndef USE_LIBXML
	crm_validate_data(xml_root);
	xml_child_iter(
		xml_root, a_child, NULL,
		crm_set_element_parent(a_child, xml_root);
		crm_update_parents(a_child);
		);
#endif
}


#ifndef USE_LIBXML

int
get_tag_name(const char *input) 
{
	int lpc = 0;
	char ch = 0;
	const char *error = NULL;
	gboolean do_special = FALSE;
	
	for(lpc = 0; error == NULL && lpc < strlen(input); lpc++) {
		ch = input[lpc];
		crm_insane("Processing char %c [%d]", ch, lpc);

		switch(ch) {
			case EOF: 
			case 0:
				error = "unexpected EOS";
				break;
			case '?':
				if(lpc == 0) {
					/* weird xml tag that we dont care about */
					do_special = TRUE;
				} else {
					return lpc;
				}
				break;
			case '/':
			case '>':
			case '\t':
			case '\n':
			case ' ':
				if(!do_special) {
					return lpc;
				}
				break;
			default:
				if(do_special) {

				} else if('a' <= ch && ch <= 'z') {
				} else if('A' <= ch && ch <= 'Z') {
				} else if(ch == '_') {
				} else {
					error = "bad character, not in [a-zA-Z_]";
				}
 				break;
		}
	}
	crm_err("Error parsing token near %.15s: %s", input, crm_str(error));
	return -1;
}

int
get_attr_name(const char *input) 
{
	int lpc = 0;
	char ch = 0;
	const char *error = NULL;
	
	for(lpc = 0; error == NULL && lpc < strlen(input); lpc++) {
		ch = input[lpc];
		crm_insane("Processing char %c[%d]", ch, lpc);

		switch(ch) {
			case EOF: 
			case 0:
				error = "unexpected EOS";
 				break;
			case '\t':
			case '\n':
			case ' ':
				error = "unexpected whitespace";
 				break;
			case '=':
				return lpc;
			default:
				if('a' <= ch && ch <= 'z') {
				} else if('A' <= ch && ch <= 'Z') {
				} else if(ch == '_') {
				} else {
					error = "bad character, not in [a-zA-Z_]";
				}
 				break;
		}
	}
	crm_err("Error parsing token near %.15s: %s", input, crm_str(error));
	return -1;
}

int
get_attr_value(const char *input) 
{
	int lpc = 0;
	char ch = 0;
	const char *error = NULL;
	
	for(lpc = 0; error == NULL && lpc < strlen(input); lpc++) {
		ch = input[lpc];
		crm_insane("Processing char %c [%d]", ch, lpc);
		
		switch(ch) {
			case EOF: 
			case 0:
				error = "unexpected EOS";
 				break;
			case '\\':
				if(input[lpc+1] == '"') {
					/* skip over the next char */ 
					lpc++;
					break;
				}
				/*fall through*/
			case '"':
				return lpc;
			default:
 				break;
		}
	}
	crm_err("Error parsing token near %.15s: %s", input, crm_str(error));
	return -1;
}


crm_data_t*
parse_xml(const char *input, int *offset)
{
	int len = 0, lpc = 0;
	char ch = 0;
	char *tag_name = NULL;
	char *attr_name = NULL;
	char *attr_value = NULL;
	gboolean more = TRUE;
	const char *error = NULL;
	const char *our_input = input;
	crm_data_t *new_obj = NULL;

	if(offset != NULL) {
		our_input = input + (*offset);
	}

	len = strlen(our_input);
	while(lpc < len) {
		if(our_input[lpc] != '<') {

		} else if(our_input[lpc+1] == '!') {
			crm_err("XML Comments are not supported");
			crm_insane("Skipping char %c", our_input[lpc]);
			lpc++;
			
		} else if(our_input[lpc+1] == '?') {
			crm_insane("Skipping char %c", our_input[lpc]);
			lpc++;
		} else {
			lpc++;
			our_input += lpc;
			break;
		}
		crm_insane("Skipping char %c", our_input[lpc]);
		lpc++;
	}
	
	len = get_tag_name(our_input);
	if(len < 0) {
		return NULL;
	}
	crm_malloc(tag_name, len+1);
	strncpy(tag_name, our_input, len+1);
	tag_name[len] = EOS;
	crm_devel("Processing tag %s", tag_name);
	
	new_obj = ha_msg_new(1);
	CRM_DEV_ASSERT(cl_is_allocated(new_obj) == 1);
	
	ha_msg_add(new_obj, XML_ATTR_TAGNAME, tag_name);
	lpc = len;

	for(; more && error == NULL && lpc < strlen(input); lpc++) {
			ch = our_input[lpc];
			crm_insane("Processing char %c[%d]", ch, lpc);
			switch(ch) {
				case EOF: 
				case 0:
					error = "unexpected EOS";
					break;
				case '/':
					if(our_input[lpc+1] == '>') {
						more = FALSE;
					}
					break;
				case '<':
					if(our_input[lpc+1] != '/') {
						crm_data_t *child = NULL;
						crm_devel("Start parsing child...");
						child = parse_xml(our_input, &lpc);
						if(child == NULL) {
							error = "error parsing child";
						} else {
							CRM_DEV_ASSERT(cl_is_allocated(child) == 1);
							ha_msg_addstruct(
								new_obj, crm_element_name(child), child);
							crm_devel("Finished parsing child: %s",
								  crm_element_name(child));
/* 							lpc++; /\* > *\/ */
						}

					} else {
						lpc += 2; /* </ */
						len = get_tag_name(our_input+lpc);
						if(len < 0) {
							error = "couldnt find tag";
						} else if(strncmp(our_input+lpc, tag_name, len) == 0) {
							more = FALSE;
							lpc += len;
/* 							lpc++; /\* > *\/ */
							if(our_input[lpc] != '>') {
								error = "clase tag cannot contain attrs";
							}
							crm_devel("Finished parsing ourselves: %s",
								  crm_element_name(new_obj));
							
						} else {
							error = "Mismatching close tag";
							crm_err("Expected: %s", tag_name);
						}
					}
					break;
				case '=':
					lpc++; /* = */
					/*fall through*/
				case '"':
					lpc++; /* " */
					len = get_attr_value(our_input+lpc);
					if(len < 0) {
						error = "couldnt find attr_value";
					} else {
						crm_malloc(attr_value, len+1);
						strncpy(attr_value, our_input+lpc, len+1);
						attr_value[len] = EOS;
						lpc += len;
/* 						lpc++; /\* " *\/ */

						crm_devel("creating nvpair: <%s %s=\"%s\"...",
							  tag_name,
							  crm_str(attr_name),
							  crm_str(attr_value));
						
						ha_msg_add(new_obj, attr_name, attr_value);
						crm_free(attr_name);
						crm_free(attr_value);
					}
					break;
				case '>':
				case ' ':
				case '\t':
				case '\n':
					break;
				default:
					len = get_attr_name(our_input+lpc);
					if(len < 0) {
						error = "couldnt find attr_name";
					} else {
						crm_malloc(attr_name, len+1);
						strncpy(attr_name, our_input+lpc, len+1);
						attr_name[len] = EOS;
						lpc += len;
						crm_trace("found attr name: %s", attr_name);
						lpc--; /* make sure the '=' is seen next time around */
					}
					break;
			}
	}
	
	if(error) {
		crm_err("Error parsing token: %s", error);
		crm_err("Error at or before: %s", our_input+lpc-3);
		return NULL;
	}
	
	crm_devel("Finished processing %s tag", tag_name);
	crm_free(tag_name);
	if(offset != NULL) {
		(*offset) += lpc;
	}
	
	CRM_DEV_ASSERT(cl_is_allocated(new_obj) == 1);
	return new_obj;
}


#endif

