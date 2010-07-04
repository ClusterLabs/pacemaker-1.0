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

#include <crm_internal.h>
#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <libxml/xmlreader.h>

#include <clplumbing/md5.h>
#if HAVE_BZLIB_H
#  include <bzlib.h>
#endif

#define XML_BUFFER_SIZE	4096
#define XML_PARSER_DEBUG 0

xmlDoc *getDocPtr(xmlNode *node);

struct schema_s 
{
	int type;
	const char *name;
	const char *location;
	const char *transform;
	int after_transform;
};

struct schema_s known_schemas[] = {
/* 0 */    { 0, NULL, NULL, NULL, 1 },
/* 1 */    { 1, "pacemaker-0.6",    CRM_DTD_DIRECTORY"/crm.dtd",		CRM_DTD_DIRECTORY"/upgrade06.xsl", 4 },
/* 2 */    { 1, "transitional-0.6", CRM_DTD_DIRECTORY"/crm-transitional.dtd",	CRM_DTD_DIRECTORY"/upgrade06.xsl", 4 },
/* 3 */    { 2, "pacemaker-0.7",    CRM_DTD_DIRECTORY"/pacemaker-1.0.rng",	NULL, 0 },
/* 4 */    { 2, "pacemaker-1.0",    CRM_DTD_DIRECTORY"/pacemaker-1.0.rng",	NULL, 0 },
/* 5 */    { 0, "none", NULL, NULL, 0 },
};

static int all_schemas = DIMOF(known_schemas);
static int max_schemas = DIMOF(known_schemas) - 2; /* skip back past 'none' */

static const char *filter[] = {
    XML_ATTR_ORIGIN,
    XML_DIFF_MARKER,
    XML_CIB_ATTR_WRITTEN,		
};

static void add_ha_nocopy(HA_Message *parent, HA_Message *child, const char *field) 
{
    int next = parent->nfields;
    if (parent->nfields >= parent->nalloc && ha_msg_expand(parent) != HA_OK ) {
	crm_err("Parent expansion failed");
	return;
    }
    
    parent->names[next] = crm_strdup(field);
    parent->nlens[next] = strlen(field);
    parent->values[next] = child;
    parent->vlens[next] = sizeof(HA_Message);
    parent->types[next] = FT_UNCOMPRESS;
    parent->nfields++;	
}

int print_spaces(char *buffer, int spaces, int max);

int log_data_element(const char *function, const char *prefix, int log_level,
		     int depth, xmlNode *data, gboolean formatted);

int get_tag_name(const char *input, size_t offset, size_t max);
int get_attr_name(const char *input, size_t offset, size_t max);
int get_attr_value(const char *input, size_t offset, size_t max);
gboolean can_prune_leaf(xmlNode *xml_node);

void diff_filter_context(int context, int upper_bound, int lower_bound,
		    xmlNode *xml_node, xmlNode *parent);
int in_upper_context(int depth, int context, xmlNode *xml_node);
int write_file(const char *string, const char *filename);

xmlNode *
find_xml_node(xmlNode *root, const char * search_path, gboolean must_find)
{
	const char *name = "NULL";
	if(must_find || root != NULL) {
		crm_validate_data(root);
	}
	if(root != NULL) {
	    name = crm_element_name(root);
	}
	
	if(search_path == NULL) {
		crm_warn("Will never find <NULL>");
		return NULL;
	}
	
	xml_child_iter_filter(
		root, a_child, search_path,
/* 		crm_debug_5("returning node (%s).", crm_element_name(a_child)); */
		crm_validate_data(a_child);
		return a_child;
		);

	if(must_find) {
		crm_warn("Could not find %s in %s.", search_path, name);
	} else if(root != NULL) {
		crm_debug_3("Could not find %s in %s.", search_path, name);
	} else {
		crm_debug_3("Could not find %s in <NULL>.", search_path);
	}
	
	
	return NULL;
}

xmlNode*
find_entity(xmlNode *parent, const char *node_name, const char *id)
{
	crm_validate_data(parent);
	xml_child_iter_filter(
		parent, a_child, node_name,
		if(id == NULL || crm_str_eq(id, ID(a_child), TRUE)) {
			crm_debug_4("returning node (%s).", 
				    crm_element_name(a_child));
			return a_child;
		}
		);
	crm_debug_3("node <%s id=%s> not found in %s.",
		    node_name, id, crm_element_name(parent));
	return NULL;
}

void
copy_in_properties(xmlNode* target, xmlNode *src)
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
			expand_plus_plus(target, local_prop_name, local_prop_value)
			);
		crm_validate_data(target);
	}
	
	return;
}

void fix_plus_plus_recursive(xmlNode* target)
{
    xml_prop_iter(target, name, value, expand_plus_plus(target, name, value));
    xml_child_iter(target, child, fix_plus_plus_recursive(child));
}


void
expand_plus_plus(xmlNode* target, const char *name, const char *value)
{
    int offset = 1;
    int name_len = 0;
    int int_value = 0;
    int value_len = 0;

    const char *old_value = NULL;

    if(value == NULL || name == NULL) {
	return;
    }
    
    old_value = crm_element_value(target, name);

    if(old_value == NULL) {
	/* if no previous value, set unexpanded */
	goto set_unexpanded;

    } else if(strstr(value, name) != value) {
	goto set_unexpanded;
    }

    name_len = strlen(name);
    value_len = strlen(value);
    if(value_len < (name_len + 2)
	      || value[name_len] != '+'
	      || (value[name_len+1] != '+' && value[name_len+1] != '=')) {
	goto set_unexpanded;
    }

    /* if we are expanding ourselves,
     * then no previous value was set and leave int_value as 0
     */
    if(old_value != value) {
	int_value = char2score(old_value);
    }
    
    if(value[name_len+1] != '+') {
	const char *offset_s = value+(name_len+2);
	offset = char2score(offset_s);
    }
    int_value += offset;

    if(int_value > INFINITY) {
	int_value = INFINITY;
    }
    
    crm_xml_add_int(target, name, int_value);
    return;

  set_unexpanded:
    if(old_value == value) {
	/* the old value is already set, nothing to do */
	return;
    }
    crm_xml_add(target, name, value);
    return;
}

xmlDoc *getDocPtr(xmlNode *node)
{
    xmlDoc *doc = NULL;
    CRM_CHECK(node != NULL, return NULL);

    doc = node->doc;
    if(doc == NULL) {
	doc = xmlNewDoc((const xmlChar*)"1.0");
	xmlDocSetRootElement(doc, node);
	xmlSetTreeDoc(node, doc);
    }
    return doc;
}

xmlNode*
add_node_copy(xmlNode *parent, xmlNode *src_node) 
{
	xmlNode *child = NULL;
	xmlDoc *doc = getDocPtr(parent);
	CRM_CHECK(src_node != NULL, return NULL);

	child = xmlDocCopyNode(src_node, doc, 1);
	xmlAddChild(parent, child);
	return child;
}


int
add_node_nocopy(xmlNode *parent, const char *name, xmlNode *child)
{
	add_node_copy(parent, child);
	free_xml(child);
	return HA_OK;
}

const char *
crm_xml_add(xmlNode* node, const char *name, const char *value)
{
    xmlAttr *attr = NULL;
    CRM_CHECK_AND_STORE(node != NULL, return NULL);
    CRM_CHECK_AND_STORE(name != NULL, return NULL);

    if(value == NULL) {
	return NULL;
    }

#if XML_PARANOIA_CHECKS
    {
	const char *old_value = NULL;
	old_value = crm_element_value(node, name);
	
	/* Could be re-setting the same value */
	CRM_CHECK_AND_STORE(old_value != value,
			    crm_err("Cannot reset %s with crm_xml_add(%s)",
				    name, value);
			    return value);
    }
#endif
    
    attr = xmlSetProp(node, (const xmlChar*)name, (const xmlChar*)value);
    CRM_CHECK(attr && attr->children && attr->children->content, return NULL);
    return (char *)attr->children->content;
}

const char *
crm_xml_replace(xmlNode* node, const char *name, const char *value)
{
    xmlAttr *attr = NULL;
    const char *old_value = NULL;
    CRM_CHECK(node != NULL, return NULL);
    CRM_CHECK(name != NULL && name[0] != 0, return NULL);

    old_value = crm_element_value(node, name);

    /* Could be re-setting the same value */
    CRM_CHECK_AND_STORE(old_value != value, return value);

    if (old_value != NULL && value == NULL) {
	xml_remove_prop(node, name);
	return NULL;

    } else if(value == NULL) {
	return NULL;
    }
    
    attr = xmlSetProp(node, (const xmlChar*)name, (const xmlChar*)value);
    CRM_CHECK(attr && attr->children && attr->children->content, return NULL);
    return (char *)attr->children->content;
}

const char *
crm_xml_add_int(xmlNode* node, const char *name, int value)
{
    char *number = crm_itoa(value);
    const char *added = crm_xml_add(node, name, number);
    crm_free(number);
    return added;
}

xmlNode*
create_xml_node(xmlNode *parent, const char *name)
{
    xmlDoc *doc = NULL;
    xmlNode *node = NULL;	

    if (name == NULL || name[0] == 0) {
	return NULL;
    }
    
    if(parent == NULL) {
	doc = xmlNewDoc((const xmlChar*)"1.0");
	node = xmlNewDocRawNode(doc, NULL, (const xmlChar*)name, NULL);
	xmlDocSetRootElement(doc, node);
	
    } else {
	doc = getDocPtr(parent);
	node = xmlNewDocRawNode(doc, NULL, (const xmlChar*)name, NULL);
	xmlAddChild(parent, node);
    }
    return node;
}

void
free_xml_from_parent(xmlNode *parent, xmlNode *a_node)
{
	CRM_CHECK(a_node != NULL, return);

	xmlUnlinkNode(a_node);
	xmlFreeNode(a_node);
}

xmlNode*
copy_xml(xmlNode *src)
{
    xmlDoc *doc = xmlNewDoc((const xmlChar*)"1.0");
    xmlNode *copy = xmlDocCopyNode(src, doc, 1);
    xmlDocSetRootElement(doc, copy);
    xmlSetTreeDoc(copy, doc);
    return copy;
}


static void crm_xml_err(void * ctx, const char * msg, ...) G_GNUC_PRINTF(2,3);
extern size_t strlcat(char * dest, const char *source, size_t len);

int
write_file(const char *string, const char *filename) 
{
	int rc = 0;
	FILE *file_output_strm = NULL;
	
	CRM_CHECK(filename != NULL, return -1);

	if (string == NULL) {
		crm_err("Cannot write NULL to %s", filename);
		return -1;
	}

	file_output_strm = fopen(filename, "w");
	if(file_output_strm == NULL) {
		crm_perror(LOG_ERR,"Cannot open %s for writing", filename);
		return -1;
	} 

	rc = fprintf(file_output_strm, "%s", string);
	if(rc < 0) {
	    crm_perror(LOG_ERR,"Cannot write output to %s", filename);
	}		
	
	if(fflush(file_output_strm) != 0) {
	    crm_perror(LOG_ERR,"fflush for %s failed:", filename);
	    rc = -1;
	}
	
	if(fsync(fileno(file_output_strm)) < 0) {
	    crm_perror(LOG_ERR,"fsync for %s failed:", filename);
	    rc = -1;
	}
	    
	fclose(file_output_strm);
	return rc;
}

static void crm_xml_err(void * ctx, const char * msg, ...)
{
    int len = 0;
    va_list args;
    char *buf = NULL;
    static int buffer_len = 0;
    static char *buffer = NULL;
    
    va_start(args, msg);
    len = vasprintf(&buf, msg, args);

    if(strchr(buf, '\n')) {
	buf[len - 1] = 0;
	if(buffer) {
	    crm_err("XML Error: %s%s", buffer, buf);
	    free(buffer);
	} else {
	    crm_err("XML Error: %s", buf);	    
	}
	buffer = NULL;
	buffer_len = 0;
	
    } else if(buffer == NULL) {
	buffer_len = len;
	buffer = buf;
	buf = NULL;

    } else {
	buffer_len += len;
	buffer = realloc(buffer, buffer_len);
	strlcat(buffer, buf, buffer_len);
    }
    
    va_end(args);
    if(buf) {
	free(buf);	
    }
}

xmlNode*
string2xml(const char *input)
{
	xmlNode *xml = NULL;
	xmlDocPtr output = NULL;
	xmlParserCtxtPtr ctxt = NULL;
	xmlErrorPtr last_error = NULL;
	
	if(input == NULL) {
	    crm_err("Can't parse NULL input");
	    return NULL;
	}
	
	/* create a parser context */
	ctxt = xmlNewParserCtxt();
	CRM_CHECK(ctxt != NULL, return NULL);

	/* xmlCtxtUseOptions(ctxt, XML_PARSE_NOBLANKS|XML_PARSE_RECOVER); */

	xmlCtxtResetLastError(ctxt);
	xmlSetGenericErrorFunc(ctxt, crm_xml_err);
	/* initGenericErrorDefaultFunc(crm_xml_err); */
	output = xmlCtxtReadDoc(ctxt, (const xmlChar*)input, NULL, NULL, XML_PARSE_NOBLANKS|XML_PARSE_RECOVER);
	if(output) {
	    xml = xmlDocGetRootElement(output);
	}
	last_error = xmlCtxtGetLastError(ctxt);
	if(last_error && last_error->code != XML_ERR_OK) {
	    /* crm_abort(__FILE__,__PRETTY_FUNCTION__,__LINE__, "last_error->code != XML_ERR_OK", TRUE, TRUE); */
	    /*
	     * http://xmlsoft.org/html/libxml-xmlerror.html#xmlErrorLevel
	     * http://xmlsoft.org/html/libxml-xmlerror.html#xmlParserErrors
	     */
	    crm_warn("Parsing failed (domain=%d, level=%d, code=%d): %s",
		    last_error->domain, last_error->level,
		    last_error->code, last_error->message);

	    if(last_error->code != XML_ERR_DOCUMENT_END) {
		crm_err("Couldn't%s parse %d chars: %s", xml?" fully":"", (int)strlen(input), input);
		if(xml != NULL) {
		    crm_log_xml_err(xml, "Partial");
		}

	    } else {
		int len = strlen(input);
		crm_warn("String start: %.50s", input);
		crm_warn("String start+%d: %s", len-50, input+len-50);
		crm_abort(__FILE__,__PRETTY_FUNCTION__,__LINE__, "String parsing error", TRUE, TRUE);
	    }
	}

	xmlFreeParserCtxt(ctxt);
	return xml;
}

xmlNode *
stdin2xml(void) 
{
 	size_t data_length = 0;
 	size_t read_chars = 0;
  
  	char *xml_buffer = NULL;
  	xmlNode *xml_obj = NULL;
  
 	do {
 		crm_realloc(xml_buffer, XML_BUFFER_SIZE + data_length + 1);
 		read_chars = fread(xml_buffer + data_length, 1, XML_BUFFER_SIZE, stdin);
 		data_length += read_chars;
 	} while (read_chars > 0);

	if(data_length == 0) {
	    crm_warn("No XML supplied on stdin");
	    return NULL;
	}

 	xml_buffer[data_length] = '\0';

	xml_obj = string2xml(xml_buffer);
	crm_free(xml_buffer);

	crm_log_xml_debug_3(xml_obj, "Created fragment");
	return xml_obj;
}

static char *
decompress_file(const char *filename)
{
    char *buffer = NULL;
#if HAVE_BZLIB_H
    int rc = 0;
    size_t length = 0, read_len = 0;
    
    BZFILE *bz_file = NULL;
    FILE *input = fopen(filename, "r");

    if(input == NULL) {
	crm_perror(LOG_ERR,"Could not open %s for reading", filename);
	return NULL;
    }
    
    bz_file = BZ2_bzReadOpen(&rc, input, 0, 0, NULL, 0);

    if ( rc != BZ_OK ) {
	BZ2_bzReadClose ( &rc, bz_file);
	return NULL;
    }
    
    rc = BZ_OK;
    while ( rc == BZ_OK ) {
	crm_realloc(buffer, XML_BUFFER_SIZE + length + 1);
	read_len = BZ2_bzRead (
	    &rc, bz_file, buffer + length, XML_BUFFER_SIZE);
	
	crm_debug_5("Read %ld bytes from file: %d",
		    (long)read_len, rc);
	
	if ( rc == BZ_OK || rc == BZ_STREAM_END) {
	    length += read_len;
	}
    }
    
    buffer[length] = '\0';
    read_len = length;
    
    if ( rc != BZ_STREAM_END ) {
	crm_err("Couldnt read compressed xml from file");
	crm_free(buffer);
	buffer = NULL;
    }
    
    BZ2_bzReadClose (&rc, bz_file);
    fclose(input);
    
#else
    crm_err("Cannot read compressed files:"
	    " bzlib was not available at compile time");
#endif
    return buffer;
}

xmlNode *
filename2xml(const char *filename)
{
    xmlNode *xml = NULL;
    xmlDocPtr output = NULL;
    xmlParserCtxtPtr ctxt = NULL;
    xmlErrorPtr last_error = NULL;
    static int xml_options = XML_PARSE_NOBLANKS|XML_PARSE_RECOVER;
    
    /* create a parser context */
    ctxt = xmlNewParserCtxt();
    CRM_CHECK(ctxt != NULL, return NULL);
    
    /* xmlCtxtUseOptions(ctxt, XML_PARSE_NOBLANKS|XML_PARSE_RECOVER); */
    
    xmlCtxtResetLastError(ctxt);
    xmlSetGenericErrorFunc(ctxt, crm_xml_err);
    /* initGenericErrorDefaultFunc(crm_xml_err); */

    if(filename == NULL) {
	/* STDIN_FILENO == fileno(stdin) */
	output = xmlCtxtReadFd(ctxt, STDIN_FILENO, "unknown.xml", NULL, xml_options);

    } else if(strstr(filename, ".bz2") == NULL) {
	output = xmlCtxtReadFile(ctxt, filename, NULL, xml_options);

    } else {
	char *input = decompress_file(filename);
	output = xmlCtxtReadDoc(ctxt, (const xmlChar*)input, NULL, NULL, xml_options);
	crm_free(input);
    }

    if(output) {
	xml = xmlDocGetRootElement(output);
    }
    
    last_error = xmlCtxtGetLastError(ctxt);
    if(last_error && last_error->code != XML_ERR_OK) {
	/* crm_abort(__FILE__,__PRETTY_FUNCTION__,__LINE__, "last_error->code != XML_ERR_OK", TRUE, TRUE); */
	/*
	 * http://xmlsoft.org/html/libxml-xmlerror.html#xmlErrorLevel
	 * http://xmlsoft.org/html/libxml-xmlerror.html#xmlParserErrors
	 */
	crm_err("Parsing failed (domain=%d, level=%d, code=%d): %s",
		last_error->domain, last_error->level,
		last_error->code, last_error->message);
	
	if(last_error && last_error->code != XML_ERR_OK) {
	    crm_err("Couldn't%s parse %s", xml?" fully":"", filename);
	    if(xml != NULL) {
		crm_log_xml_err(xml, "Partial");
	    }
	}
    }
    
    xmlFreeParserCtxt(ctxt);
    return xml;
}

int
write_xml_file(xmlNode *xml_node, const char *filename, gboolean compress) 
{
	int res = 0;
	time_t now;
	char *buffer = NULL;
	char *now_str = NULL;
	unsigned int out = 0;
	FILE *file_output_strm = NULL;
	static mode_t cib_mode = S_IRUSR|S_IWUSR;
	
	CRM_CHECK(filename != NULL, return -1);

	crm_debug_3("Writing XML out to %s", filename);
	crm_validate_data(xml_node);
	if (xml_node == NULL) {
		crm_err("Cannot write NULL to %s", filename);
		return -1;
	}

	file_output_strm = fopen(filename, "w");
	if(file_output_strm == NULL) {
		crm_perror(LOG_ERR,"Cannot open %s for writing", filename);
		return -1;
	} 

	/* establish the correct permissions */
	fchmod(fileno(file_output_strm), cib_mode);
	
	crm_log_xml_debug_4(xml_node, "Writing out");
	
	now = time(NULL);
	now_str = ctime(&now);
	now_str[24] = EOS; /* replace the newline */
	crm_xml_add(xml_node, XML_CIB_ATTR_WRITTEN, now_str);
	crm_validate_data(xml_node);
	
	buffer = dump_xml_formatted(xml_node);
	CRM_CHECK(buffer != NULL && strlen(buffer) > 0,
		  crm_log_xml_warn(xml_node, "dump:failed");
		  goto bail);	

	if(compress) {
#if HAVE_BZLIB_H
	    int rc = BZ_OK;
	    unsigned int in = 0;
	    BZFILE *bz_file = NULL;
	    bz_file = BZ2_bzWriteOpen(&rc, file_output_strm, 5, 0, 30);
	    if(rc != BZ_OK) {
		crm_err("bzWriteOpen failed: %d", rc);
	    } else {
		BZ2_bzWrite(&rc,bz_file,buffer,strlen(buffer));
		if(rc != BZ_OK) {
		    crm_err("bzWrite() failed: %d", rc);
		}
	    }
	    
	    if(rc == BZ_OK) {
		BZ2_bzWriteClose(&rc, bz_file, 0, &in, &out);
		if(rc != BZ_OK) {
		    crm_err("bzWriteClose() failed: %d",rc);
		    out = -1;
		} else {
		    crm_debug_2("%s: In: %d, out: %d", filename, in, out);
		}
	    }
#else
	    crm_err("Cannot write compressed files:"
		    " bzlib was not available at compile time");		
#endif
	}
	
	if(out <= 0) {
	    res = fprintf(file_output_strm, "%s", buffer);
	    if(res < 0) {
		crm_perror(LOG_ERR,"Cannot write output to %s", filename);
		goto bail;
	    }		
	}
	
  bail:
	
	if(fflush(file_output_strm) != 0) {
	    crm_perror(LOG_ERR,"fflush for %s failed:", filename);
	    res = -1;
	}
	
	if(fsync(fileno(file_output_strm)) < 0) {
	    crm_perror(LOG_ERR,"fsync for %s failed:", filename);
	    res = -1;
	}
	    
	fclose(file_output_strm);
	
	crm_debug_3("Saved %d bytes to the Cib as XML", res);
	crm_free(buffer);

	return res;
}

void
print_xml_formatted(int log_level, const char *function,
		    xmlNode *msg, const char *text)
{
	if(msg == NULL) {
		do_crm_log(log_level, "%s: %s: NULL", function, crm_str(text));
		return;
	}

	crm_validate_data(msg);
	log_data_element(function, text, log_level, 0, msg, TRUE);
	return;
}

static HA_Message*
convert_xml_message_struct(HA_Message *parent, xmlNode *src_node, const char *field) 
{
    xmlNode *child = NULL;
    xmlNode *__crm_xml_iter = src_node->children;
    xmlAttrPtr prop_iter = src_node->properties;
    const char *name = NULL;
    const char *value = NULL;

    HA_Message *result = ha_msg_new(3);
    ha_msg_add(result, F_XML_TAGNAME, (const char *)src_node->name);
    
    while(prop_iter != NULL) {
	name = (const char *)prop_iter->name;
	value = (const char *)xmlGetProp(src_node, prop_iter->name);
	prop_iter = prop_iter->next;
	ha_msg_add(result, name, value);
    }

    while(__crm_xml_iter != NULL) {
	child = __crm_xml_iter;
	__crm_xml_iter = __crm_xml_iter->next;
	convert_xml_message_struct(result, child, NULL);
    }

    if(parent == NULL) {
	return result;
    }
    
    if(field) {
	HA_Message *holder = holder = ha_msg_new(3);
	CRM_ASSERT(holder != NULL);
	
	ha_msg_add(holder, F_XML_TAGNAME, field);
	add_ha_nocopy(holder, result, (const char*)src_node->name);
	
	ha_msg_addstruct_compress(parent, field, holder);
	ha_msg_del(holder);

    } else {
	add_ha_nocopy(parent, result, (const char*)src_node->name);
    }
    return result;
}

static void
convert_xml_child(HA_Message *msg, xmlNode *xml) 
{
    int orig = 0;
    int rc = BZ_OK;
    unsigned int len = 0;
    
    char *buffer = NULL;
    char *compressed = NULL;
    const char *name = NULL;

    name = (const char *)xml->name;
    buffer = dump_xml_unformatted(xml);
    orig = strlen(buffer);
    if(orig < CRM_BZ2_THRESHOLD) {
	ha_msg_add(msg, name, buffer);
	goto done;
    }
    
    len = (orig * 1.1) + 600; /* recomended size */
    
    crm_malloc(compressed, len);
    rc = BZ2_bzBuffToBuffCompress(compressed, &len, buffer, orig, CRM_BZ2_BLOCKS, 0, CRM_BZ2_WORK);
    
    if(rc != BZ_OK) {
	crm_err("Compression failed: %d", rc);
	crm_free(compressed);
	convert_xml_message_struct(msg, xml, name);
	goto done;
    }
    
    crm_free(buffer);
    buffer = compressed;
    crm_debug_2("Compression details: %d -> %d", orig, len);
    ha_msg_addbin(msg, name, buffer, len);
  done:
    crm_free(buffer);


#  if 0
    {
	unsigned int used = orig;
	char *uncompressed = NULL;
	
	crm_debug("Trying to decompress %d bytes", len);
	crm_malloc0(uncompressed, orig);
	rc = BZ2_bzBuffToBuffDecompress(
	    uncompressed, &used, compressed, len, 1, 0);
	CRM_CHECK(rc == BZ_OK, ;);
	CRM_CHECK(used == orig, ;);
	crm_debug("rc=%d, used=%d", rc, used);
	if(rc != BZ_OK) {
	    exit(100);
	}
	crm_debug("Original %s, decompressed %s", buffer, uncompressed);
	crm_free(uncompressed);
    }
#  endif 
}

HA_Message*
convert_xml_message(xmlNode *xml) 
{
    HA_Message *result = NULL;

    result = ha_msg_new(3);
    ha_msg_add(result, F_XML_TAGNAME, (const char *)xml->name);

    xml_prop_iter(xml, name, value, ha_msg_add(result, name, value));
    xml_child_iter(xml, child, convert_xml_child(result, child));

    return result;
}

static void
convert_ha_field(xmlNode *parent, HA_Message *msg, int lpc) 
{
    int type = 0;
    const char *name = NULL;
    const char *value = NULL;
    xmlNode *xml = NULL;
    
    int rc = BZ_OK;
    size_t orig_len = 0;
    unsigned int used = 0;
    char *uncompressed = NULL;
    char *compressed = NULL;
    int size = orig_len * 10;
    
    CRM_CHECK(parent != NULL, return);
    CRM_CHECK(msg != NULL, return);
	
    name = msg->names[lpc];
    type = cl_get_type(msg, name);

    switch(type) {
	case FT_STRUCT:
	    convert_ha_message(parent, msg->values[lpc], name);
	    break;
	case FT_COMPRESS:
	case FT_UNCOMPRESS:
	    convert_ha_message(parent, cl_get_struct(msg, name), name);
	    break;
	case FT_STRING:
	    value = msg->values[lpc];
	    CRM_CHECK_AND_STORE(value != NULL, return);
	    crm_debug_5("Converting %s/%d/%s", name, type, value[0] == '<' ? "xml":"field");

	    if( value[0] != '<' ) {
		crm_xml_add(parent, name, value);
		break;
	    }
	    
	    /* unpack xml string */
	    xml = string2xml(value);
	    if(xml == NULL) {
		crm_err("Conversion of field '%s' failed", name);
		return;
	    }

	    add_node_nocopy(parent, NULL, xml);
	    break;

	case FT_BINARY:
	    value = cl_get_binary(msg, name, &orig_len);
	    size = orig_len * 10 + 1; /* +1 because an exact 10x compression factor happens occasionally */

	    if(orig_len < 3
	       || value[0] != 'B'
	       || value[1] != 'Z'
	       || value[2] != 'h') {
		if(strstr(name, "uuid") == NULL) {
		    crm_err("Skipping non-bzip binary field: %s", name);
		}
		return;
	    }

	    crm_malloc0(compressed, orig_len);
	    memcpy(compressed, value, orig_len);
	    
	    crm_debug_2("Trying to decompress %d bytes", (int)orig_len);
	  retry:
	    crm_realloc(uncompressed, size);
	    memset(uncompressed, 0, size);
	    used = size - 1; /* always leave room for a trailing '\0'
			      * BZ2_bzBuffToBuffDecompress wont say anything if
			      * the uncompressed data is exactly 'size' bytes 
			      */
	    
	    rc = BZ2_bzBuffToBuffDecompress(
		uncompressed, &used, compressed, orig_len, 1, 0);
	    
	    if(rc == BZ_OUTBUFF_FULL) {
		size = size * 2;
		/* dont try to allocate more memory than we have */
		if(size > 0) {
		    goto retry;
		}
	    }
	    
	    if(rc != BZ_OK) { 
		crm_err("Decompression of %s (%d bytes) into %d failed: %d",
			name, (int)orig_len, size, rc);
		
	    } else {
		CRM_ASSERT(used < size);
		CRM_CHECK(uncompressed[used] == 0, uncompressed[used] = 0);
		xml = string2xml(uncompressed);
	    }

	    if(xml != NULL) {
		add_node_copy(parent, xml);
		free_xml(xml);
	    }
	    
	    crm_free(uncompressed);
	    crm_free(compressed);		
	    break;
    }
}

xmlNode *
convert_ha_message(xmlNode *parent, HA_Message *msg, const char *field) 
{
    int lpc = 0;
    xmlNode *child = NULL;
    const char *tag = NULL;
    
    CRM_CHECK_AND_STORE(msg != NULL, crm_err("Empty message for %s", field); return parent);
    
    tag = cl_get_string(msg, F_XML_TAGNAME);
    if(tag == NULL) {
	tag = field;

    } else if(parent && safe_str_neq(field, tag)) {
	/* For compatability with 0.6.x */
	crm_debug("Creating intermediate parent %s between %s and %s", field, crm_element_name(parent), tag);
	parent = create_xml_node(parent, field);
    }
    
    if(parent == NULL) {
	parent = create_xml_node(NULL, tag);
	child = parent;
	
    } else {
	child = create_xml_node(parent, tag);
    }

    for (lpc = 0; lpc < msg->nfields; lpc++) {
	convert_ha_field(child, msg, lpc);
    }
    
    return parent;
}

xmlNode *convert_ipc_message(IPC_Message *msg, const char *field)
{
    HA_Message *hmsg = wirefmt2msg((char *)msg->msg_body, msg->msg_len, 0);
    xmlNode *xml = convert_ha_message(NULL, hmsg, __FUNCTION__);
    crm_msg_del(hmsg);
    return xml;
}

xmlNode *
get_message_xml(xmlNode *msg, const char *field) 
{
    xmlNode *tmp = first_named_child(msg, field);
    return first_named_child(tmp, NULL);
}

gboolean
add_message_xml(xmlNode *msg, const char *field, xmlNode *xml) 
{
    xmlNode *holder = create_xml_node(msg, field);
    add_node_copy(holder, xml);
    return TRUE;
}

static char *
dump_xml(xmlNode *an_xml_node, gboolean formatted, gboolean for_digest)
{
    int len = 0;
    char *buffer = NULL;
    xmlBuffer *xml_buffer = NULL;
    xmlDoc *doc = getDocPtr(an_xml_node);

    /* doc will only be NULL if an_xml_node is */
    CRM_CHECK(doc != NULL, return NULL);

    xml_buffer = xmlBufferCreate();
    CRM_ASSERT(xml_buffer != NULL);
    
    len = xmlNodeDump(xml_buffer, doc, an_xml_node, 0, formatted);

    if(len > 0) {
	if(for_digest) {
	    /* for compatability with the old result which is used for digests */
	    len += 3;
	    crm_malloc0(buffer, len);
	    snprintf(buffer, len, " %s\n", (char *)xml_buffer->content);
	} else {
	    buffer = crm_strdup((char *)xml_buffer->content);	    
	}

    } else {
	crm_err("Conversion failed");
    }

    xmlBufferFree(xml_buffer);
    return buffer;    
}

char *
dump_xml_formatted(xmlNode *an_xml_node)
{
    return dump_xml(an_xml_node, TRUE, FALSE);
}

char *
dump_xml_unformatted(xmlNode *an_xml_node)
{
    return dump_xml(an_xml_node, FALSE, FALSE);
}
    
#define update_buffer() do {				\
	if(printed < 0) {				\
	    crm_perror(LOG_ERR,"snprintf failed");		\
	    goto print;					\
	} else if(printed >= (buffer_len - offset)) {	\
	    crm_err("Output truncated: available=%d, needed=%d", buffer_len - offset, printed);	\
	    offset += printed;				\
	    goto print;					\
	} else if(offset >= buffer_len) {		\
	    crm_err("Buffer exceeded");			\
	    offset += printed;				\
	    goto print;					\
	} else {					\
	    offset += printed;				\
	}						\
} while(0)

int
print_spaces(char *buffer, int depth, int max) 
{
	int lpc = 0;
	int spaces = 2*depth;
	max--;
	
	/* <= so that we always print 1 space - prevents problems with syslog */
	for(lpc = 0; lpc <= spaces && lpc < max; lpc++) {
		if(sprintf(buffer+lpc, "%c", ' ') < 1) {
			return -1;
		}
	}
	return lpc;
}

int
log_data_element(
	const char *function, const char *prefix, int log_level, int depth,
	xmlNode *data, gboolean formatted) 
{
	int child_result = 0;

	int offset = 0;
	int printed = 0;
	char *buffer = NULL;
	int buffer_len = 1000;

	const char *name = NULL;
	const char *hidden = NULL;

	if(data == NULL) {
	    crm_warn("No data to dump as XML");
	    return 0;
	}
	
	name = crm_element_name(data);
	CRM_ASSERT(name != NULL);
	
	crm_debug_5("Dumping %s", name);
	crm_malloc0(buffer, buffer_len);
	
	if(formatted) {
	    offset = print_spaces(buffer, depth, buffer_len - offset);
	}

	printed = snprintf(buffer + offset, buffer_len - offset, "<%s", name);
	update_buffer();
	
	hidden = crm_element_value(data, "hidden");
	xml_prop_iter(
		data, prop_name, prop_value,

		if(prop_name == NULL
		   || safe_str_eq(F_XML_TAGNAME, prop_name)) {
			continue;

		} else if(hidden != NULL
			  && prop_name[0] != 0
			  && strstr(hidden, prop_name) != NULL) {
			prop_value = "*****";
		}
		
		crm_debug_5("Dumping <%s %s=\"%s\"...",
			    name, prop_name, prop_value);
		printed = snprintf(buffer + offset, buffer_len - offset,
				   " %s=\"%s\"", prop_name, prop_value);
		update_buffer();
		);

	printed = snprintf(buffer + offset, buffer_len - offset,
			   " %s>", xml_has_children(data)?"":"/");
	update_buffer();
	
  print:
	do_crm_log(log_level, "%s: %s%s", function, prefix?prefix:"", buffer);
	
	if(xml_has_children(data) == FALSE) {
		crm_free(buffer);
		return 0;
	}
	
	xml_child_iter(
		data, a_child, 
		child_result = log_data_element(
			function, prefix, log_level, depth+1, a_child, formatted);
		);

	if(formatted) {
		offset = print_spaces(buffer, depth, buffer_len);
	}
	do_crm_log(log_level, "%s: %s%s</%s>", function, prefix?prefix:"", buffer, name);
	crm_free(buffer);
	return 1;
}

gboolean
xml_has_children(const xmlNode *xml_root)
{
	if(xml_root != NULL && xml_root->children != NULL) {
	    return TRUE;
	}
	return FALSE;
}

void
xml_validate(const xmlNode *xml_root)
{
	CRM_ASSERT(xml_root != NULL);
}

int
crm_element_value_int(xmlNode *data, const char *name, int *dest)
{
    const char *value = crm_element_value(data, name);
    CRM_CHECK(dest != NULL, return -1);
    if(value) {
	*dest = crm_int_helper(value, NULL);
	return 0;
    }
    return -1;
}

const char *
crm_element_value_const(const xmlNode *data, const char *name)
{
    return crm_element_value((xmlNode*)data, name);
}

char *
crm_element_value_copy(xmlNode *data, const char *name)
{
	char *value_copy = NULL;
	const char *value = crm_element_value(data, name);
	if(value != NULL) {
		value_copy = crm_strdup(value);
	}
	return value_copy;
}

void
xml_remove_prop(xmlNode *obj, const char *name)
{
    xmlUnsetProp(obj, (const xmlChar*)name);
}

void
log_xml_diff(unsigned int log_level, xmlNode *diff, const char *function)
{
	xmlNode *added = find_xml_node(diff, "diff-added", FALSE);
	xmlNode *removed = find_xml_node(diff, "diff-removed", FALSE);
	gboolean is_first = TRUE;

	if(crm_log_level < log_level) {
		/* nothing will ever be printed */
		return;
	}
	
	xml_child_iter(
		removed, child, 
		log_data_element(function, "-", log_level, 0, child, TRUE);
		if(is_first) {
			is_first = FALSE;
		} else {
			do_crm_log(log_level, " --- ");
		}
		
		);
	is_first = TRUE;
	xml_child_iter(
		added, child, 
		log_data_element(function, "+", log_level, 0, child, TRUE);
		if(is_first) {
			is_first = FALSE;
		} else {
			do_crm_log(log_level, " +++ ");
		}
		);
}

void
purge_diff_markers(xmlNode *a_node)
{
	CRM_CHECK(a_node != NULL, return);

	xml_remove_prop(a_node, XML_DIFF_MARKER);
	xml_child_iter(a_node, child,
		       purge_diff_markers(child);
		);
}

gboolean
apply_xml_diff(xmlNode *old, xmlNode *diff, xmlNode **new)
{
	gboolean result = TRUE;
	const char *digest = crm_element_value(diff, XML_ATTR_DIGEST);
	xmlNode *added = find_xml_node(diff, "diff-added", FALSE);
	xmlNode *removed = find_xml_node(diff, "diff-removed", FALSE);

	int root_nodes_seen = 0;

	CRM_CHECK(new != NULL, return FALSE);

	crm_debug_2("Substraction Phase");
	xml_child_iter(removed, child_diff, 
		       CRM_CHECK(root_nodes_seen == 0, result = FALSE);
		       if(root_nodes_seen == 0) {
			       *new = subtract_xml_object(old, child_diff, NULL);
		       }
		       root_nodes_seen++;
		);
	if(root_nodes_seen == 0) {
		*new = copy_xml(old);
		
	} else if(root_nodes_seen > 1) {
		crm_err("(-) Diffs cannot contain more than one change set..."
			" saw %d", root_nodes_seen);
		result = FALSE;
	}

	root_nodes_seen = 0;
	crm_debug_2("Addition Phase");
	if(result) {
		xml_child_iter(added, child_diff, 
			       CRM_CHECK(root_nodes_seen == 0, result = FALSE);
			       if(root_nodes_seen == 0) {
				       add_xml_object(NULL, *new, child_diff);
			       }
			       root_nodes_seen++;
			);
	}

	if(root_nodes_seen > 1) {
		crm_err("(+) Diffs cannot contain more than one change set..."
			" saw %d", root_nodes_seen);
		result = FALSE;

	} else if(result && digest) {
	    char *new_digest = calculate_xml_digest(*new, FALSE, TRUE);
	    if(safe_str_neq(new_digest, digest)) {
		crm_info("Digest mis-match: expected %s, calculated %s",
			 digest, new_digest);
 		result = FALSE;
	    } else {
		crm_debug_2("Digest matched: expected %s, calculated %s",
			    digest, new_digest);
	    }
	    crm_free(new_digest);
	    
	} else if(result) {
		int lpc = 0;
		xmlNode *intermediate = NULL;
		xmlNode *diff_of_diff = NULL;
		xmlNode *calc_added = NULL;
		xmlNode *calc_removed = NULL;

		const char *value = NULL;
		const char *name = NULL;
		const char *version_attrs[] = {
			XML_ATTR_NUMUPDATES,
			XML_ATTR_GENERATION,
			XML_ATTR_GENERATION_ADMIN
		};

		crm_debug_2("Verification Phase");
		intermediate = diff_xml_object(old, *new, FALSE);
		calc_added = find_xml_node(intermediate, "diff-added", FALSE);
		calc_removed = find_xml_node(intermediate, "diff-removed", FALSE);

		/* add any version details to the diff so they match */
		for(lpc = 0; lpc < DIMOF(version_attrs); lpc++) {
			name = version_attrs[lpc];

			value = crm_element_value(added, name);
			crm_xml_add(calc_added, name, value);
			
			value = crm_element_value(removed, name);
			crm_xml_add(calc_removed, name, value);	
		}

		diff_of_diff = diff_xml_object(intermediate, diff, TRUE);
		if(diff_of_diff != NULL) {
			crm_info("Diff application failed!");
			crm_log_xml_debug(old, "diff:original");
			crm_log_xml_debug(diff, "diff:input");
			result = FALSE;
		}
		
		free_xml(diff_of_diff);
		free_xml(intermediate);
		diff_of_diff = NULL;
		intermediate = NULL;
	}
	
	if(result) {
		purge_diff_markers(*new);
	}

	return result;
}


xmlNode *
diff_xml_object(xmlNode *old, xmlNode *new, gboolean suppress)
{
	xmlNode *diff = NULL;
	xmlNode *tmp1 = NULL;
	xmlNode *added = NULL;
	xmlNode *removed = NULL;

	tmp1 = subtract_xml_object(old, new, "removed:top");
	if(tmp1 != NULL) {
		if(suppress && can_prune_leaf(tmp1)) {
			free_xml(tmp1);

		} else {
			diff = create_xml_node(NULL, "diff");
			removed = create_xml_node(diff, "diff-removed");
			added = create_xml_node(diff, "diff-added");
			add_node_nocopy(removed, NULL, tmp1);
		}
	}
	
	tmp1 = subtract_xml_object(new, old, "added:top");
	if(tmp1 != NULL) {
		if(suppress && can_prune_leaf(tmp1)) {
			free_xml(tmp1);
			return diff;
			
		}

		if(diff == NULL) {
			diff = create_xml_node(NULL, "diff");
		}
		if(removed == NULL) {
			removed = create_xml_node(diff, "diff-removed");
		}
		if(added == NULL) {
			added = create_xml_node(diff, "diff-added");
		}
		add_node_nocopy(added, NULL, tmp1);
	}

	return diff;
}

gboolean
can_prune_leaf(xmlNode *xml_node)
{
	gboolean can_prune = TRUE;
/* 	return FALSE; */
	
	xml_prop_iter(xml_node, prop_name, prop_value,
		      if(safe_str_eq(prop_name, XML_ATTR_ID)) {
			      continue;
		      }		      
		      can_prune = FALSE;
		);
	xml_child_iter(xml_node, child, 
		       if(can_prune_leaf(child)) {
				free_xml(child);
		       } else {
			       can_prune = FALSE;
		       }
		);
	return can_prune;
}


void
diff_filter_context(int context, int upper_bound, int lower_bound,
		    xmlNode *xml_node, xmlNode *parent) 
{
	xmlNode *us = NULL;
	xmlNode *new_parent = parent;
	const char *name = crm_element_name(xml_node);

	CRM_CHECK(xml_node != NULL && name != NULL, return);
	
	us = create_xml_node(parent, name);
	xml_prop_iter(xml_node, prop_name, prop_value,
		      lower_bound = context;
		      crm_xml_add(us, prop_name, prop_value);
		);
	if(lower_bound >= 0 || upper_bound >= 0) {
		crm_xml_add(us, XML_ATTR_ID, ID(xml_node));
		new_parent = us;

	} else {
		upper_bound = in_upper_context(0, context, xml_node);
		if(upper_bound >= 0) {
			crm_xml_add(us, XML_ATTR_ID, ID(xml_node));
			new_parent = us;
		} else {
			free_xml(us);
			us = NULL;
		}
	}

	xml_child_iter(us, child, 
		       diff_filter_context(
			       context, upper_bound-1, lower_bound-1,
			       child, new_parent);
		);
}

int
in_upper_context(int depth, int context, xmlNode *xml_node)
{
	gboolean has_attributes = FALSE;
	if(context == 0) {
		return 0;
	}
	
	xml_prop_iter(xml_node, prop_name, prop_value,
		      has_attributes = TRUE;
		      break;
		);
	
	if(has_attributes) {
		return depth;

	} else if(depth < context) {
		xml_child_iter(xml_node, child, 
			       if(in_upper_context(depth+1, context, child)) {
				       return depth;
			       }
			);
	}
	return 0;       
}


xmlNode *
subtract_xml_object(xmlNode *left, xmlNode *right, const char *marker)
{
	gboolean skip = FALSE;
	gboolean differences = FALSE;
	xmlNode *diff = NULL;
	xmlNode *child_diff = NULL;
	xmlNode *right_child = NULL;

	const char *id = NULL;
	const char *name = NULL;
	const char *value = NULL;
	const char *right_val = NULL;

	int lpc = 0;
	static int filter_len = DIMOF(filter);
	
	if(left == NULL) {
		return NULL;
	}

	id = ID(left);
	if(right == NULL) {
		xmlNode *deleted = NULL;

		crm_debug_5("Processing <%s id=%s> (complete copy)",
			    crm_element_name(left), id);
		deleted = copy_xml(left);
		crm_xml_add(deleted, XML_DIFF_MARKER, marker);

		return deleted;
	}

	name = crm_element_name(left);
	CRM_CHECK(name != NULL, return NULL);

	diff = create_xml_node(NULL, name);

	/* changes to name/value pairs */
	xml_prop_iter(left, prop_name, left_value,
		      if(crm_str_eq(prop_name, XML_ATTR_ID, TRUE)) {
			      continue;
		      }

		      skip = FALSE;
		      for(lpc = 0; skip == FALSE && lpc < filter_len; lpc++){
			      if(crm_str_eq(prop_name, filter[lpc], TRUE)) {
				      skip = TRUE;
			      }
		      }
		      
		      if(skip) { continue; }
		      
		      right_val = crm_element_value(right, prop_name);
		      if(right_val == NULL) {
			  /* new */
			  differences = TRUE;
			  crm_xml_add(diff, prop_name, left_value);
				      
		      } else if(strcmp(left_value, right_val) == 0) {
			  /* unchanged */

		      } else {
			  /* changed */
			  differences = TRUE;
			  crm_xml_add(diff, prop_name, left_value);
		      }
		);

	/* changes to child objects */
	xml_child_iter(
		left, left_child,  
		right_child = find_entity(
			right, crm_element_name(left_child), ID(left_child));
		child_diff = subtract_xml_object(
			left_child, right_child, marker);
		if(child_diff != NULL) {
			differences = TRUE;
			add_node_nocopy(diff, NULL, child_diff);
		}
		
		);

	if(differences == FALSE) {
		/* check for XML_DIFF_MARKER in a child */ 
		xml_child_iter(
			right, right_child,  
			value = crm_element_value(right_child, XML_DIFF_MARKER);
			if(value != NULL && safe_str_eq(value, "removed:top")) {
				crm_debug_3("Found the root of the deletion: %s", name);
				differences = TRUE;
				break;
			}
			);
	}
	
	if(differences == FALSE) {
		free_xml(diff);
		crm_debug_5("\tNo changes to <%s id=%s>", crm_str(name), id);
		return NULL;
	}
	crm_xml_add(diff, XML_ATTR_ID, id);
	return diff;
}

int
add_xml_object(xmlNode *parent, xmlNode *target, xmlNode *update)
{
	const char *object_id = NULL;
	const char *object_name = NULL;

#if XML_PARSE_DEBUG
	crm_log_xml(LOG_DEBUG_5, "update:", update);
	crm_log_xml(LOG_DEBUG_5, "target:", target);
#endif

	CRM_CHECK(update != NULL, return 0);

	object_name = crm_element_name(update);
	object_id = ID(update);

	CRM_CHECK(object_name != NULL, return 0);
	
	if(target == NULL && object_id == NULL) {
		/*  placeholder object */
		target = find_xml_node(parent, object_name, FALSE);

	} else if(target == NULL) {
		target = find_entity(parent, object_name, object_id);
	}

	if(target == NULL) {
		target = create_xml_node(parent, object_name);
		CRM_CHECK(target != NULL, return 0);
#if XML_PARSER_DEBUG
		crm_debug_2("Added  <%s%s%s/>", crm_str(object_name),
			    object_id?" id=":"", object_id?object_id:"");

	} else {
		crm_debug_3("Found node <%s%s%s/> to update",
			    crm_str(object_name),
			    object_id?" id=":"", object_id?object_id:"");
#endif
	}

	copy_in_properties(target, update);

	xml_child_iter(
		update, a_child,  
#if XML_PARSER_DEBUG
		crm_debug_4("Updating child <%s id=%s>",
			    crm_element_name(a_child), ID(a_child));
#endif
		add_xml_object(target, NULL, a_child);
		);

#if XML_PARSER_DEBUG
	crm_debug_3("Finished with <%s id=%s>",
		    crm_str(object_name), crm_str(object_id));
#endif
	return 0;
}

gboolean
update_xml_child(xmlNode *child, xmlNode *to_update)
{
	gboolean can_update = TRUE;
	
	CRM_CHECK(child != NULL, return FALSE);
	CRM_CHECK(to_update != NULL, return FALSE);
	
	if(safe_str_neq(crm_element_name(to_update), crm_element_name(child))) {
		can_update = FALSE;

	} else if(safe_str_neq(ID(to_update), ID(child))) {
		can_update = FALSE;

	} else if(can_update) {
#if XML_PARSER_DEBUG
		crm_log_xml_debug_2(child, "Update match found...");
#endif
		add_xml_object(NULL, child, to_update);
	}
	
	xml_child_iter(
		child, child_of_child, 
		/* only update the first one */
		if(can_update) {
			break;
		}
		can_update = update_xml_child(child_of_child, to_update);
		);
	
	return can_update;
}


int
find_xml_children(xmlNode **children, xmlNode *root,
		  const char *tag, const char *field, const char *value,
		  gboolean search_matches)
{
	int match_found = 0;
	
	CRM_CHECK(root != NULL, return FALSE);
	CRM_CHECK(children != NULL, return FALSE);
	
	if(tag != NULL && safe_str_neq(tag, crm_element_name(root))) {

	} else if(value != NULL
		  && safe_str_neq(value, crm_element_value(root, field))) {

	} else {
		if(*children == NULL) {
			*children = create_xml_node(NULL, __FUNCTION__);
		}
		add_node_copy(*children, root);
		match_found = 1;
	}

	if(search_matches || match_found == 0) {
		xml_child_iter(
			root, child, 
			match_found += find_xml_children(
				children, child, tag, field, value,
				search_matches);
			);
	}
	
	return match_found;
}

gboolean
replace_xml_child(xmlNode *parent, xmlNode *child, xmlNode *update, gboolean delete_only)
{
	gboolean can_delete = FALSE;

	const char *up_id = NULL;
	const char *child_id = NULL;
	const char *right_val = NULL;
	
	CRM_CHECK(child != NULL, return FALSE);
	CRM_CHECK(update != NULL, return FALSE);

	up_id = ID(update);
	child_id = ID(child);
	
	if(up_id == NULL || safe_str_eq(child_id, up_id)) {
		can_delete = TRUE;
	} 
	if(safe_str_neq(crm_element_name(update), crm_element_name(child))) {
		can_delete = FALSE;
	}
	if(can_delete && delete_only) {
		xml_prop_iter(update, prop_name, left_value,
			      right_val = crm_element_value(child, prop_name);
			      if(safe_str_neq(left_value, right_val)) {
				      can_delete = FALSE;
			      }
			);
	}
	
	if(can_delete && parent != NULL) {
		crm_log_xml_debug_4(child, "Delete match found...");
		if(delete_only || update == NULL) {
		    free_xml_from_parent(NULL, child);
		    
		} else {	
		    xmlNode *tmp = copy_xml(update);
		    xmlDoc *doc = tmp->doc;
		    xmlNode *old = xmlReplaceNode(child, tmp);
		    free_xml_from_parent(NULL, old);
		    xmlDocSetRootElement(doc, NULL);
		    xmlFreeDoc(doc);
		}
		child = NULL;
		return TRUE;
		
	} else if(can_delete) {
		crm_log_xml_debug(child, "Cannot delete the search root");
		can_delete = FALSE;
	}
	
	
	xml_child_iter(
		child, child_of_child, 
		/* only delete the first one */
		if(can_delete) {
			break;
		}
		can_delete = replace_xml_child(child, child_of_child, update, delete_only);
		);
	
	return can_delete;
}

void
hash2nvpair(gpointer key, gpointer value, gpointer user_data) 
{
	const char *name    = key;
	const char *s_value = value;

	xmlNode *xml_node  = user_data;
	xmlNode *xml_child = create_xml_node(xml_node, XML_CIB_TAG_NVPAIR);

	crm_xml_add(xml_child, XML_ATTR_ID, name);
	crm_xml_add(xml_child, XML_NVPAIR_ATTR_NAME, name);
	crm_xml_add(xml_child, XML_NVPAIR_ATTR_VALUE, s_value);

	crm_debug_3("dumped: name=%s value=%s", name, s_value);
}

void
hash2smartfield(gpointer key, gpointer value, gpointer user_data) 
{
	const char *name    = key;
	const char *s_value = value;

	xmlNode *xml_node  = user_data;

	if(isdigit(name[0])) {
	    xmlNode *tmp = create_xml_node(xml_node, XML_TAG_PARAM);
	    crm_xml_add(tmp, XML_NVPAIR_ATTR_NAME, name);
	    crm_xml_add(tmp, XML_NVPAIR_ATTR_VALUE, s_value);
	    
	} else if(crm_element_value(xml_node, name) == NULL) {
		crm_xml_add(xml_node, name, s_value);
		crm_debug_3("dumped: %s=%s", name, s_value);

	} else {
		crm_debug_2("duplicate: %s=%s", name, s_value);
	}
}

void
hash2field(gpointer key, gpointer value, gpointer user_data) 
{
	const char *name    = key;
	const char *s_value = value;

	xmlNode *xml_node  = user_data;

	if(crm_element_value(xml_node, name) == NULL) {
		crm_xml_add(xml_node, name, s_value);
		crm_debug_3("dumped: %s=%s", name, s_value);

	} else {
		crm_debug_2("duplicate: %s=%s", name, s_value);
	}
}

void
hash2metafield(gpointer key, gpointer value, gpointer user_data) 
{
    char *crm_name = NULL;
    
    if(key == NULL || value == NULL) {
	return;
    }
    
    crm_name = crm_meta_name(key);
    hash2field(crm_name, value, user_data);
    crm_free(crm_name);
}


GHashTable *
xml2list(xmlNode *parent)
{
	xmlNode *nvpair_list = NULL;
	GHashTable *nvpair_hash = g_hash_table_new_full(
		g_str_hash, g_str_equal,
		g_hash_destroy_str, g_hash_destroy_str);
	
	CRM_CHECK(parent != NULL, return nvpair_hash);

	nvpair_list = find_xml_node(parent, XML_TAG_ATTRS, FALSE);
	if(nvpair_list == NULL) {
		crm_debug_2("No attributes in %s",
			    crm_element_name(parent));
		crm_log_xml_debug_2(
			parent,"No attributes for resource op");
	}
	
	crm_log_xml_debug_3(nvpair_list, "Unpacking");

	xml_prop_iter(
		nvpair_list, key, value, 
		
		crm_debug_4("Added %s=%s", key, value);
		
		g_hash_table_insert(
			nvpair_hash, crm_strdup(key), crm_strdup(value));
		);

	xml_child_iter_filter(
	    nvpair_list, child, XML_TAG_PARAM,

	    const char *key = crm_element_value(child, XML_NVPAIR_ATTR_NAME);
	    const char *value = crm_element_value(child, XML_NVPAIR_ATTR_VALUE);
	    crm_debug_4("Added %s=%s", key, value);
	    if(key != NULL && value != NULL) {
		g_hash_table_insert(nvpair_hash, crm_strdup(key), crm_strdup(value));		
	    }
	    );
	
	return nvpair_hash;
}


typedef struct name_value_s 
{
	const char *name;
	const void *value;
} name_value_t;

static gint
sort_pairs(gconstpointer a, gconstpointer b)
{
    int rc = 0;
	const name_value_t *pair_a = a;
	const name_value_t *pair_b = b;

	CRM_ASSERT(a != NULL);
	CRM_ASSERT(pair_a->name != NULL);

	CRM_ASSERT(b != NULL);
	CRM_ASSERT(pair_b->name != NULL);

	rc = strcmp(pair_a->name, pair_b->name);
	if(rc < 0) {
	    return -1;
	} else if(rc > 0) {
	    return 1;
	}
	return 0;
}

static void
dump_pair(gpointer data, gpointer user_data)
{
	name_value_t *pair = data;
	xmlNode *parent = user_data;
	crm_xml_add(parent, pair->name, pair->value);
}

xmlNode *
sorted_xml(xmlNode *input, xmlNode *parent, gboolean recursive)
{
	GListPtr sorted = NULL;
	GListPtr unsorted = NULL;
	name_value_t *pair = NULL;
	xmlNode *result = NULL;
	const char *name = crm_element_name(input);

	CRM_CHECK(input != NULL, return NULL);
	
	name = crm_element_name(input);
	CRM_CHECK(name != NULL, return NULL);

	result = create_xml_node(parent, name);
	
	xml_prop_iter(input, p_name, p_value,
		      crm_malloc0(pair, sizeof(name_value_t));
		      pair->name  = p_name;
		      pair->value = p_value;
		      unsorted = g_list_prepend(unsorted, pair);
		      pair = NULL;
		);

	sorted = g_list_sort(unsorted, sort_pairs);
	g_list_foreach(sorted, dump_pair, result);
	slist_destroy(name_value_t, child, sorted, crm_free(child));

	if(recursive) {
	    xml_child_iter(input, child, sorted_xml(child, result, recursive));
	} else {
	    xml_child_iter(input, child, add_node_copy(result, child));
	}
	
	return result;
}

static void
filter_xml(xmlNode *data, const char **filter, int filter_len, gboolean recursive) 
{
    int lpc = 0;
    
    for(lpc = 0; lpc < filter_len; lpc++) {
	xml_remove_prop(data, filter[lpc]);
    }

    if(recursive == FALSE) {
	return;
    }
    
    xml_child_iter(data, child, filter_xml(child, filter, filter_len, recursive));
}

/* "c048eae664dba840e1d2060f00299e9d" */
char *
calculate_xml_digest(xmlNode *input, gboolean sort, gboolean do_filter)
{
	int i = 0;
	int digest_len = 16;
	char *digest = NULL;
	unsigned char *raw_digest = NULL;
	xmlNode *sorted = NULL;
	char *buffer = NULL;
	size_t buffer_len = 0;

	if(sort || do_filter) {
	    sorted = sorted_xml(input, NULL, TRUE);
	} else {
	    sorted = copy_xml(input);
	}

	if(do_filter) {
	    filter_xml(sorted, filter, DIMOF(filter), TRUE);
	}

	buffer = dump_xml(sorted, FALSE, TRUE);
	buffer_len = strlen(buffer);
	
	CRM_CHECK(buffer != NULL && buffer_len > 0, free_xml(sorted); crm_free(buffer); return NULL);

	crm_malloc(digest, (2 * digest_len + 1));
	crm_malloc(raw_digest, (digest_len + 1));
	MD5((unsigned char *)buffer, buffer_len, raw_digest);
	for(i = 0; i < digest_len; i++) {
 		sprintf(digest+(2*i), "%02x", raw_digest[i]);
 	}
	digest[(2*digest_len)] = 0;
	crm_debug_2("Digest %s: %s\n", digest, buffer);
	crm_log_xml(LOG_DEBUG_3,  "digest:source", sorted);
	crm_free(buffer);
	crm_free(raw_digest);
	free_xml(sorted);
	return digest;
}


#if HAVE_LIBXML2
#  include <libxml/parser.h>
#  include <libxml/tree.h>
#  include <libxml/relaxng.h>
#  include <libxslt/xslt.h>
#  include <libxslt/transform.h>
#endif

static gboolean
validate_with_dtd(
	xmlDocPtr doc, gboolean to_logs, const char *dtd_file) 
{
	gboolean valid = TRUE;

	xmlDtdPtr dtd = NULL;
	xmlValidCtxtPtr cvp = NULL;
	
	CRM_CHECK(doc != NULL, return FALSE);
	CRM_CHECK(dtd_file != NULL, return FALSE);

	dtd = xmlParseDTD(NULL, (const xmlChar *)dtd_file);
	CRM_CHECK(dtd != NULL, crm_err("Could not find/parse %s", dtd_file); goto cleanup);

	cvp = xmlNewValidCtxt();
	CRM_CHECK(cvp != NULL, goto cleanup);

	if(to_logs) {
		cvp->userData = (void *) LOG_ERR;
		cvp->error    = (xmlValidityErrorFunc) cl_log;
		cvp->warning  = (xmlValidityWarningFunc) cl_log;
	} else {
		cvp->userData = (void *) stderr;
		cvp->error    = (xmlValidityErrorFunc) fprintf;
		cvp->warning  = (xmlValidityWarningFunc) fprintf;
	}
	
	if (!xmlValidateDtd(cvp, doc, dtd)) {
		valid = FALSE;
	}
	
  cleanup:
	if(cvp) {
		xmlFreeValidCtxt(cvp);
	}
	if(dtd) {
		xmlFreeDtd(dtd);
	}
	
	return valid;
}

xmlNode *first_named_child(xmlNode *parent, const char *name) 
{
    xml_child_iter_filter(parent, match, name, return match);
    return NULL;
}

#if 0
static void relaxng_invalid_stderr(void * userData, xmlErrorPtr error)
{
    /*
Structure xmlError
struct _xmlError {
    int	domain	: What part of the library raised this er
    int	code	: The error code, e.g. an xmlParserError
    char *	message	: human-readable informative error messag
    xmlErrorLevel	level	: how consequent is the error
    char *	file	: the filename
    int	line	: the line number if available
    char *	str1	: extra string information
    char *	str2	: extra string information
    char *	str3	: extra string information
    int	int1	: extra number information
    int	int2	: column number of the error or 0 if N/A
    void *	ctxt	: the parser context if available
    void *	node	: the node in the tree
}
     */
    crm_err("Structured error: line=%d, level=%d %s",
	    error->line, error->level, error->message);
}
#endif

static gboolean
validate_with_relaxng(
    xmlDocPtr doc, gboolean to_logs, const char *relaxng_file) 
{
    gboolean valid = TRUE;
    int rc = 0;

    xmlRelaxNGPtr rng = NULL;
    xmlRelaxNGValidCtxtPtr valid_ctx = NULL;
    xmlRelaxNGParserCtxtPtr parser_ctx = NULL;
    
    CRM_CHECK(doc != NULL, return FALSE);
    CRM_CHECK(relaxng_file != NULL, return FALSE);

    xmlLoadExtDtdDefaultValue = 1;
    parser_ctx = xmlRelaxNGNewParserCtxt(relaxng_file);
    CRM_CHECK(parser_ctx != NULL, goto cleanup);

    if(to_logs) {
	xmlRelaxNGSetParserErrors(parser_ctx,
				  (xmlRelaxNGValidityErrorFunc) cl_log,
				  (xmlRelaxNGValidityWarningFunc) cl_log,
				  GUINT_TO_POINTER(LOG_ERR));
    } else {
	xmlRelaxNGSetParserErrors(parser_ctx,
				  (xmlRelaxNGValidityErrorFunc) fprintf,
				  (xmlRelaxNGValidityWarningFunc) fprintf,
				  stderr);
    }

    rng = xmlRelaxNGParse(parser_ctx);
    CRM_CHECK(rng != NULL, crm_err("Could not find/parse %s", relaxng_file); goto cleanup);

    valid_ctx = xmlRelaxNGNewValidCtxt(rng);
    CRM_CHECK(valid_ctx != NULL, goto cleanup);

    if(to_logs) {
	xmlRelaxNGSetValidErrors(valid_ctx,
				 (xmlRelaxNGValidityErrorFunc) cl_log,
				 (xmlRelaxNGValidityWarningFunc) cl_log,
				 GUINT_TO_POINTER(LOG_ERR));
    } else {
	xmlRelaxNGSetValidErrors(valid_ctx,
				 (xmlRelaxNGValidityErrorFunc) fprintf,
				 (xmlRelaxNGValidityWarningFunc) fprintf,
				 stderr);
    }

    /* xmlRelaxNGSetValidStructuredErrors( */
    /* 	valid_ctx, relaxng_invalid_stderr, valid_ctx); */
    
    xmlLineNumbersDefault(1);
    rc = xmlRelaxNGValidateDoc(valid_ctx, doc);
    if (rc > 0) {
	valid = FALSE;

    } else if (rc < 0) {
	crm_err("Internal libxml error during validation\n");
    }

  cleanup:
    if(parser_ctx != NULL) {
	xmlRelaxNGFreeParserCtxt(parser_ctx);
	xmlCleanupParser();
    }

    if(valid_ctx != NULL) {
	xmlRelaxNGFreeValidCtxt(valid_ctx);
    }
    
    if (rng != NULL) {
	xmlRelaxNGFree(rng);    
    }
    return valid;
}

static gboolean validate_with(xmlNode *xml, int method, gboolean to_logs) 
{
    xmlDocPtr doc = NULL;
    gboolean valid = FALSE;
    int type = known_schemas[method].type;
    const char *file = known_schemas[method].location;    

    CRM_CHECK(xml != NULL, return FALSE);
    doc = getDocPtr(xml);
    
    crm_debug_2("Validating with: %s (type=%d)", crm_str(file), type);
    switch(type) {
	case 0:
	    valid = TRUE;
	    break;
	case 1:
	    valid = validate_with_dtd(doc, to_logs, file);
	    break;
	case 2:
	    valid = validate_with_relaxng(doc, to_logs, file);
	    break;
	default:
	    crm_err("Unknown validator type: %d", type);
	    break;
    }

    return valid;
}

#include <stdio.h>
static void dump_file(const char *filename) 
{

    FILE *fp = NULL;
    int ch, line = 0;

    CRM_CHECK(filename != NULL, return);

    fp = fopen(filename, "r");
    CRM_CHECK(fp != NULL, return);

    fprintf(stderr, "%4d ", ++line);
    do {
	ch = getc(fp);
	if(ch == EOF) {
	    putc('\n', stderr);
	    break;
	} else if(ch == '\n') {
	    fprintf(stderr, "\n%4d ", ++line);
	} else {
	    putc(ch, stderr);
	}
    } while(1);
    
    fclose(fp);
}

gboolean validate_xml_verbose(xmlNode *xml_blob) 
{
    xmlDoc *doc = NULL;
    xmlNode *xml = NULL;
    gboolean rc = FALSE;

    char *filename = NULL;
    static char *template = NULL;
    if(template == NULL) {
	template = crm_strdup(CRM_STATE_DIR"/cib-invalid.XXXXXX");
    }
    
    filename = mktemp(template);
    write_xml_file(xml_blob, filename, FALSE);
    
    dump_file(filename);
    
    doc = xmlParseFile(filename);
    xml = xmlDocGetRootElement(doc);
    rc = validate_xml(xml, NULL, FALSE);
    free_xml(xml);
    
    return rc;
}

gboolean validate_xml(xmlNode *xml_blob, const char *validation, gboolean to_logs)
{
    int lpc = 0;
    
    if(validation == NULL) {
	validation = crm_element_value(xml_blob, XML_ATTR_VALIDATION);
    }

    if(validation == NULL) {
	validation = crm_element_value(xml_blob, "ignore-dtd");
	if(crm_is_true(validation)) {
	    validation = "none";
	} else {
	    validation = "transitional-0.6";
	}
    }
    
    if(safe_str_eq(validation, "none")) {
	return TRUE;
    }
    
    for(; lpc < all_schemas; lpc++) {
	if(safe_str_eq(validation, known_schemas[lpc].name)) {
	    return validate_with(xml_blob, lpc, to_logs);
	}
    }

    crm_err("Unknown validator: %s", validation);
    return FALSE;
}

static xmlNode *apply_transformation(xmlNode *xml, const char *transform) 
{
    xmlNode *out = NULL;
    xmlDocPtr res = NULL;
    xmlDocPtr doc = NULL;
    xsltStylesheet *xslt = NULL;

    CRM_CHECK(xml != NULL, return FALSE);
    doc = getDocPtr(xml);

    xmlLoadExtDtdDefaultValue = 1;
    xmlSubstituteEntitiesDefault(1);
    
    xslt = xsltParseStylesheetFile((const xmlChar *)transform);
    CRM_CHECK(xslt != NULL, goto cleanup);
    
    res = xsltApplyStylesheet(xslt, doc, NULL);
    CRM_CHECK(res != NULL, goto cleanup);

    out = xmlDocGetRootElement(res);
    
  cleanup:
    if(xslt) {
	xsltFreeStylesheet(xslt);
    }

    xsltCleanupGlobals();
    xmlCleanupParser();
    
    return out;
}

const char *get_schema_name(int version)
{
    if(version < 0 || version >= all_schemas) {
	return "unknown";
    }
    return known_schemas[version].name;
}


int get_schema_version(const char *name) 
{
    int lpc = 0;
    for(; lpc < all_schemas; lpc++) {
	if(safe_str_eq(name, known_schemas[lpc].name)) {
	    return lpc;
	}
    }
    return -1;
}

/* set which validation to use */
#include <crm/cib.h>
int update_validation(
    xmlNode **xml_blob, int *best, gboolean transform, gboolean to_logs) 
{
    xmlNode *xml = NULL;
    char *value = NULL;
    int lpc = 0, match = -1, rc = cib_ok;

    CRM_CHECK(best != NULL, return cib_invalid_argument);
    CRM_CHECK(xml_blob != NULL, return cib_invalid_argument);
    CRM_CHECK(*xml_blob != NULL, return cib_invalid_argument);
    
    *best = 0;
    xml = *xml_blob;
    value = crm_element_value_copy(xml, XML_ATTR_VALIDATION);

    if(value != NULL) {
	match = get_schema_version(value);
	
	lpc = match;
	if(lpc >= 0 && transform == FALSE) {
	    lpc++;

	} else if(lpc < 0) {
	    crm_debug("Unknown validation type");
	    lpc = 0;
	}
    }

    if(match >= max_schemas) {
	/* nothing to do */
	crm_free(value);
	*best = match;
	return cib_ok;
    }
    
    for(; lpc < max_schemas; lpc++) {
	gboolean valid = TRUE;
	crm_debug("Testing '%s' validation", known_schemas[lpc].name?known_schemas[lpc].name:"<unset>");
	valid = validate_with(xml, lpc, to_logs);
	
	if(valid) {
	    *best = lpc;
	}
	
	if(valid && transform && known_schemas[lpc].transform != NULL) {
	    xmlNode *upgrade = NULL;
	    int next = known_schemas[lpc].after_transform;
	    if(next <= 0) {
		next = lpc+1;
	    }
	    
	    crm_notice("Upgrading %s-style configuration to %s with %s",
		       known_schemas[lpc].name, known_schemas[next].name, known_schemas[lpc].transform);
	    upgrade = apply_transformation(xml, known_schemas[lpc].transform);
	    if(upgrade == NULL) {
		crm_err("Transformation %s failed", known_schemas[lpc].transform);
		rc = cib_transform_failed;
		
	    } else if(validate_with(upgrade, next, to_logs)) {
		crm_info("Transformation %s successful", known_schemas[lpc].transform);
		lpc = next; *best = next;
		free_xml(xml);
		xml = upgrade;
		rc = cib_ok;
		
	    } else {
		crm_err("Transformation %s did not produce a valid configuration", known_schemas[lpc].transform);
		crm_log_xml_info(upgrade, "transform:bad");
		free_xml(upgrade);
		rc = cib_dtd_validation;
	    }
	}
    }
    
    if(*best > match) {
	crm_notice("Upgraded from %s to %s validation", value?value:"<none>", known_schemas[*best].name);
	crm_xml_add(xml, XML_ATTR_VALIDATION, known_schemas[*best].name);
    }

    *xml_blob = xml;
    crm_free(value);
    return rc;
}

xmlNode *
getXpathResult(xmlXPathObjectPtr xpathObj, int index) 
{
    xmlNode *match = NULL;
    CRM_CHECK(index >= 0, return NULL);
    CRM_CHECK(xpathObj != NULL, return NULL);

    if(index >= xpathObj->nodesetval->nodeNr) {
	crm_err("Requested index %d of only %d items", index, xpathObj->nodesetval->nodeNr);
	return NULL;
    }
    
    match = xpathObj->nodesetval->nodeTab[index];
    CRM_CHECK(match != NULL, return NULL);

    /*
     * From xpath2.c
     *
     * All the elements returned by an XPath query are pointers to
     * elements from the tree *except* namespace nodes where the XPath
     * semantic is different from the implementation in libxml2 tree.
     * As a result when a returned node set is freed when
     * xmlXPathFreeObject() is called, that routine must check the
     * element type. But node from the returned set may have been removed
     * by xmlNodeSetContent() resulting in access to freed data.
     * This can be exercised by running
     *       valgrind xpath2 test3.xml '//discarded' discarded
     * There is 2 ways around it:
     *   - make a copy of the pointers to the nodes from the result set 
     *     then call xmlXPathFreeObject() and then modify the nodes
     * or
     *   - remove the reference to the modified nodes from the node set
     *     as they are processed, if they are not namespace nodes.
     */
    if (xpathObj->nodesetval->nodeTab[index]->type != XML_NAMESPACE_DECL) {
	xpathObj->nodesetval->nodeTab[index] = NULL;
    }

    if(match->type == XML_DOCUMENT_NODE) {
	/* Will happen if section = '/' */
	match = match->children;

    } else if(match->type != XML_ELEMENT_NODE
	      && match->parent
	      && match->parent->type == XML_ELEMENT_NODE) {
	/* reurning the parent instead */
	match = match->parent;
	
    } else if(match->type != XML_ELEMENT_NODE) {
	/* We only support searching nodes */
	crm_err("We only support %d not %d", XML_ELEMENT_NODE, match->type);
	match = NULL;
    }
    return match;
}

/* the caller needs to check if the result contains a xmlDocPtr or xmlNodePtr */
xmlXPathObjectPtr 
xpath_search(xmlNode *xml_top, const char *path)
{
    xmlDocPtr doc = NULL;
    xmlXPathObjectPtr xpathObj = NULL; 
    xmlXPathContextPtr xpathCtx = NULL; 
    const xmlChar *xpathExpr = (const xmlChar *)path;

    CRM_CHECK(path != NULL, return NULL);
    CRM_CHECK(xml_top != NULL, return NULL);
    CRM_CHECK(strlen(path) > 0, return NULL);
    
    doc = getDocPtr(xml_top);

    crm_debug_2("Evaluating: %s", path);
    xpathCtx = xmlXPathNewContext(doc);
    CRM_ASSERT(xpathCtx != NULL);
    
    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    xmlXPathFreeContext(xpathCtx);
    return xpathObj;
}

gboolean
cli_config_update(xmlNode **xml, int *best_version, gboolean to_logs) 
{
    gboolean rc = TRUE;
    const char *value = crm_element_value(*xml, XML_ATTR_VALIDATION);
    int min_version = get_schema_version(MINIMUM_SCHEMA_VERSION);
    int max_version = get_schema_version(LATEST_SCHEMA_VERSION);
    int version = get_schema_version(value);

    if(version < max_version) {
	xmlNode *converted = NULL;
	
	converted = copy_xml(*xml);
	update_validation(&converted, &version, TRUE, to_logs);
	
	value = crm_element_value(converted, XML_ATTR_VALIDATION);
	if(version < min_version) {
	    if(to_logs) {
		crm_config_err("Your current configuration could only be upgraded to %s... "
			"the minimum requirement is %s.\n", crm_str(value), MINIMUM_SCHEMA_VERSION);
	    } else {
		fprintf(stderr, "Your current configuration could only be upgraded to %s... "
			"the minimum requirement is %s.\n", crm_str(value), MINIMUM_SCHEMA_VERSION);
	    }
	    
	    free_xml(converted);
	    converted = NULL;
	    rc = FALSE;
	    
	} else {
	    free_xml(*xml);
	    *xml = converted;

	    if(version < max_version) {
		crm_config_warn("Your configuration was internally updated to %s... "
				"which is acceptable but not the most recent",
				get_schema_name(version));
		
	    } else if(to_logs){
		crm_config_warn("Your configuration was internally updated to the latest version (%s)",
				get_schema_name(version));
	    } else {
		fprintf(stderr, "Your configuration was internally updated to the latest version (%s)\n",
			get_schema_name(version));
	    }	    
	}
    } else if(version > max_version) {
	if(to_logs){
	    crm_config_warn("Configuration validation is currently disabled."
			    " It is highly encouraged and prevents many common cluster issues.");

	} else {
	    fprintf(stderr, "Configuration validation is currently disabled."
			    " It is highly encouraged and prevents many common cluster issues.\n");
	}
    }

    if(best_version) {
	*best_version = version;	    
    }
    
    return rc;
}

xmlNode *expand_idref(xmlNode *input, xmlNode *top) 
{
    const char *tag = NULL;
    const char *ref = NULL;
    xmlNode *result = input;
    char *xpath_string = NULL;

    if(result == NULL) {
	return NULL;

    } else if(top == NULL) {
	top = input;
    }

    tag = crm_element_name(result);
    ref = crm_element_value(result, XML_ATTR_IDREF);
    
    if(ref != NULL) {
	int xpath_max = 512, offset = 0;
	crm_malloc0(xpath_string, xpath_max);

	offset += snprintf(xpath_string + offset, xpath_max - offset, "//%s[@id='%s']", tag, ref);
	result = get_xpath_object(xpath_string, top, LOG_ERR);
	if(result == NULL) {
	    char *nodePath = (char *)xmlGetNodePath(top);
	    crm_err("No match for %s found in %s: Invalid configuration", xpath_string, crm_str(nodePath));
	    crm_free(nodePath);
	}
    }
    
    crm_free(xpath_string);
    return result;
}

xmlNode*
get_xpath_object_relative(const char *xpath, xmlNode *xml_obj, int error_level)
{
    int len = 0;
    xmlNode *result = NULL;
    char *xpath_full = NULL;
    char *xpath_prefix = NULL;
    
    if(xml_obj == NULL || xpath == NULL) {
	return NULL;
    }

    xpath_prefix = (char *)xmlGetNodePath(xml_obj);
    len += strlen(xpath_prefix);
    len += strlen(xpath);

    xpath_full = crm_strdup(xpath_prefix);
    crm_realloc(xpath_full, len+1);
    strncat(xpath_full, xpath, len);

    result = get_xpath_object(xpath_full, xml_obj, error_level);

    crm_free(xpath_prefix);
    crm_free(xpath_full);
    return result;
}

xmlNode*
get_xpath_object(const char *xpath, xmlNode *xml_obj, int error_level)
{
    xmlNode *result = NULL;
    xmlXPathObjectPtr xpathObj = NULL;
    char *nodePath = NULL;
    char *matchNodePath = NULL;

    if(xpath == NULL) {
	return xml_obj; /* or return NULL? */
    }
    
    xpathObj = xpath_search(xml_obj, xpath);
    nodePath = (char *)xmlGetNodePath(xml_obj);
    if(xpathObj == NULL || xpathObj->nodesetval == NULL || xpathObj->nodesetval->nodeNr < 1) {
	do_crm_log(error_level, "No match for %s in %s", xpath, crm_str(nodePath));
	crm_log_xml(LOG_DEBUG_2, "Bad Input", xml_obj);
	
    } else if(xpathObj->nodesetval->nodeNr > 1) {
	int lpc = 0, max = xpathObj->nodesetval->nodeNr;
	do_crm_log(error_level, "Too many matches for %s in %s", xpath, crm_str(nodePath));

	for(lpc = 0; lpc < max; lpc++) {
	    xmlNode *match = getXpathResult(xpathObj, lpc);
	    CRM_CHECK(match != NULL, continue);

	    matchNodePath = (char *)xmlGetNodePath(match);
	    do_crm_log(error_level, "%s[%d] = %s", xpath, lpc, crm_str(matchNodePath));
	    crm_free(matchNodePath);
	}
	crm_log_xml(LOG_DEBUG_2, "Bad Input", xml_obj);

    } else {
	result = getXpathResult(xpathObj, 0);
    }
    
    if(xpathObj) {
	xmlXPathFreeObject(xpathObj);
    }
    crm_free(nodePath);

    return result;
}

const char *
crm_element_value(xmlNode *data, const char *name)
{
    xmlAttr *attr = NULL;
    
    if(data == NULL) {
	crm_err("Couldn't find %s in NULL", name?name:"<null>");
	return NULL;

    } else if(name == NULL) {
	crm_err("Couldn't find NULL in %s", crm_element_name(data));
	return NULL;
    }
    
    attr = xmlHasProp(data, (const xmlChar*)name);
    if(attr == NULL || attr->children == NULL) {
	return NULL;
    }
    return (const char*)attr->children->content;
}
