/* gXMLparse.c : implementation file for gXML parser for Linux-HA
 * 
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

#include <portability.h>
#include <glib.h>
#include <stdio.h>
#include "gXMLparse.h"
#include "gXMLscan.h"

/*
 * A recursive descent parser for a small XML subset.
 *
 * As part of the parsing of the XML, a data structure is created
 * representing the contents of the XML as a data structure...
 *
 * The goal of this work is to be able to create data structures, and then
 * serialize them into XML.  Another party would be able to then use this
 * parser to read the XML stream created earlier, and reproduce the data
 * structures which were present on the other side.
 *
 * It is the function of this code to be able to parse said XML stream and
 * then return these data structures.
 *
 * The code currently returns "wrapped" data structures which identify
 * the types of the objects created in this process.
 *
 * This is necessary because everything is done in 'C', and 'C'
 * data structures are not self-identifying (do not naturally
 * contain type information).  We need that type information!
 *
 * In the future, this may be replaced with a type structure which
 * marks the type of each item in a hash table.  This would mean that
 * fewer changes would have to be made to a program to have it be usable
 * with this system.
 *
 * *****************************************************************
 *
 * The grammar is presented non-terminal by non-terminal as comments
 * above each non-terminal parsing function.
 *
 * The list of terminals is as follows:
 *
 * EOF		the end of input condition
 * WORD		a string suitable for being an XML tag name or attribute name
 * XMLSTR	A string of characters outside of a tag
 * QUOTED_STR	A string of characters surrounded by quotes - as in an
 * 			attribute value
 * LT		<		Appears as itself in the grammar
 * LTSL		</		Appears as itself in the grammar
 * GT		>		Appears as itself in the grammar
 * SLGT		/>		Appears as itself in the grammar
 */

gboolean XMLDEBUG = FALSE;
static void dump_struct(const char * label, gXML_wrapper* w);

/*
 * XML_struct is the entirety of an XML string (including EOF)
 *
 *	XML_struct	::= XML_tagset EOF
 */

gboolean
gXML_struct(gXML_wrapper** returned, GString* string)
{
	gXML_scanner* scanner;
	scanner = gXML_new_scanner(string);
	
	/* finds the XML tagset, which should be followed by EOF */
	if (gXML_tagset(returned, scanner, &scanner->n_token, NULL)) {
		gXML_scan(scanner);

		if((scanner->n_token.peek == T_EOF)) {
			if (XMLDEBUG) {
				dump_struct("gXML_struct return: "
				,	*returned);
			}
			return TRUE;
		}; /* if */
		gXML_syntax_error("no end-of-file found");
		return FALSE;
	}

	gXML_syntax_error("no gXML_tagset");
	return FALSE;
}/* gXML_struct */



/*
 * TagSet is an series of well-formed XML surrounded by '<tag> ...</tag>'
 * bracketing (or the <tag /> form)
 *
 *	TagSet	::= < WORD Attr_list > Entity_List </ WORD >
 *		::= < WORD Attr_list />
 *
 */

gboolean
gXML_tagset(gXML_wrapper** returned, gXML_scanner* scanner
,	gXML_token* input_token, gXML_wrapper** parentobject)
{
	GString* tagname = NULL;
	GHashTable* attributes = g_hash_table_new(g_str_hash, g_str_equal);
	gXML_wrapper* object = NULL;
	int	nullcreate = FALSE;

	if (input_token->peek != T_LT) {
		gXML_syntax_error("no less than in gXML_tagset");
		return FALSE;
	}
	if (XMLDEBUG) {
		fprintf(stderr, "TagSet ::= < WORD Attr_list...\n");
		dump_struct("gXML_tagset (init returned): ", *returned);
	}
	*input_token = gXML_scan(scanner);
 
	if (XMLDEBUG) fprintf(stderr, "%s\n", input_token->TokString->str);
    
	if (input_token->peek != T_WORD) {
		gXML_syntax_error("no opening tag in gXML_tagset");
		return FALSE;
	}
	

	if (XMLDEBUG) { fprintf(stderr, "TAGNAME: %s\n", input_token->TokString->str); }
    
	tagname = g_string_new(input_token->TokString->str);

	*input_token = gXML_scan(scanner);


	if (XMLDEBUG) {
		fprintf(stderr, "Adding attributes for %s\n", tagname->str);
	}
	if (!gXML_Attr_list(attributes, scanner, input_token)) {
		gXML_syntax_error("malformed attribute list");
		return FALSE;
	}
	if (XMLDEBUG) {
		fprintf(stderr, "%s now has %d attributes.\n", tagname->str
		,	g_hash_table_size(attributes));
	}

	switch (input_token->peek) {

	case T_GT:	/* Next token is a '>' */
	
		if (XMLDEBUG) {
			fprintf(stderr
			, "TagSet ::= < WORD Attr_list>"
		       " Entity_list </WORD>\n");
		}
		object = gXML_create_item(tagname, attributes);
		if (XMLDEBUG) {
			dump_struct("gXML_tagset value from create_item: "
       			,	object);
		}
		*input_token = gXML_scan(scanner);
		if (object == NULL) {
			nullcreate = TRUE;
			if (XMLDEBUG) {
				fprintf(stderr
				,	"Object created for %s is NULL\n"
				,	tagname->str);
			}
		}

		if (XMLDEBUG) {
			fprintf(stderr, "token (after '>'): %s\n"
			, input_token->TokString->str);
	       }
	        
		if (!gXML_Entity_list(&object, scanner, input_token, attributes
		,	object == NULL ? parentobject : &object)) {
			gXML_syntax_error("call to Entity_list failed.");
			return FALSE;
		}
		if (nullcreate) {
			/* Insert it into the parent object ... */
			if (XMLDEBUG) {
				fprintf(stderr
				, "Inserting child in parent object of %s\n"
				,	tagname->str);
			}
			if (XMLDEBUG) {
				dump_struct(
				"gXML_tagset ret from entity list: "
				,	object);
				if (parentobject) {
					dump_struct("gXML_tagset"
				       " before insert: "
					,	*parentobject);
				}
			}
			gXML_insert_item(parentobject, object, attributes); 
			if (XMLDEBUG) {
				dump_struct(
				"gXML_tagset object to be appended: "
				,	object);
				if (parentobject) {
					dump_struct("gXML_tagset append: "
					,	*parentobject);
					dump_struct("parent object before"
					" assignment to object"
					,	*parentobject);
				}
			}
			*returned=object=NULL;
		}

		if (input_token->peek != T_LTSL) {
			gXML_syntax_error("could not find closing </ tag.");
			return FALSE;
		}
		
		if (XMLDEBUG && parentobject != NULL) {
			dump_struct("parent object before call"
			" to gXML_scan(</)", *parentobject);
		}
		*input_token = gXML_scan(scanner);
        
		if (XMLDEBUG) { fprintf(stderr, "token: %s\n", input_token->TokString->str); }
        
		if (input_token->peek != T_WORD) {
			gXML_syntax_error("closing tag has no tagname.");
			return FALSE;        
		}
        
		if (XMLDEBUG) {
			fprintf(stderr, "TOKEN: %s\n"
			,	input_token->TokString->str);
		}      

		if (strcmp(input_token->TokString->str, tagname->str) != 0) {
			gXML_syntax_error("mismatched closing tag.");
			if (XMLDEBUG) { fprintf(stderr, "TOKEN: %s / orig: %s\n"
			,	input_token->TokString->str, tagname->str);
			}      
			return FALSE;
		}
		if (XMLDEBUG && parentobject) {
			dump_struct("parent object before call to gXML_scan(>)", *parentobject);
		}
		*input_token = gXML_scan(scanner);
        
		if (input_token->peek != T_GT) {
			gXML_syntax_error(
			"closing tag not followed by closing '>'.");
			return FALSE;        
		}
        
		if (XMLDEBUG) {
			fprintf(stderr, "tagset: T_GT: %s\n"
			,	input_token->TokString->str);
		}
		*input_token = gXML_scan(scanner);

		
		*returned = object;
		if (XMLDEBUG) {
			fprintf(stderr
			,	"RETURNING: TagSet ::= < %s"
			" Attr_list> Entity_list </%s>\n"
			,	tagname->str, tagname->str);
		}
		break;
		
    
	case T_SLGT:		/* Next token is a '/>' */

		if (XMLDEBUG) {
			fprintf(stderr, "TagSet ::= < WORD Attr_list/>\n");
		}
		object = gXML_create_item(tagname, attributes);   
		*input_token = gXML_scan(scanner);
        
		if (XMLDEBUG) { fprintf(stderr, "SLGT: %s\n", input_token->TokString->str); }
  
		*returned = object;
		if (XMLDEBUG) {
			fprintf(stderr, "RETURNING: TagSet ::= < WORD Attr_list/>\n");
		}
		break;

	default:		/* OOPS! */

		gXML_syntax_error("error inside opening tag");
		return FALSE;
	}/*endswitch*/
	if (XMLDEBUG) {
		fprintf(stderr
		,	"tagset: returned: 0x%lx\n"
		,	(unsigned long)*returned);
 
		if (XMLDEBUG && *returned != NULL) {
			dump_struct("==============gXML_tagset return: "
			,	*returned);
		}else{
			fprintf(stderr
			,	"==============gXML_tagset returning NULL\n");
		}
	}

	/*  return (returned != NULL); */
	return TRUE;
}/* gXML_tagset */
 
 
 
/*
 * An Entity_list is a string of tags and XML text strung together...
 *
 * Probably they won't be strung together in the useful cases...
 *
 *
 *	Entity_list ::= XMLSTR |  TagSet Entity_List	| {empty}
 *
 */
gboolean
gXML_Entity_list(gXML_wrapper** object
,	gXML_scanner* scanner
,	gXML_token* token
,	GHashTable* attribs, gXML_wrapper** parentobject)
{
	GString* string = NULL;
	gXML_wrapper*	ts_obj = NULL;
	gXML_wrapper*	swrap = NULL;

	if (XMLDEBUG) {
		fprintf(stderr, "Entity List: Next token: %d\n", token->peek);
	}

	switch (token->peek) {

	case T_XMLSTR:	/* Found an XML string */
		if (XMLDEBUG) {
			fprintf(stderr, "Entity_list ::= XMLSTR\n");
		}
     
		if (XMLDEBUG) fprintf(stderr, "XMLSTR: [%s]\n", token->TokString->str);
		string = g_string_new(token->TokString->str);
		if (XMLDEBUG) fprintf(stderr, "XMLSTR (new): [%s]\n", string->str);
		swrap = gXML_wrap_GString(string);
		if (XMLDEBUG) {
			dump_struct("Entity_list ::= XMLSTR ==", swrap);
		}

		*token = gXML_scan(scanner);
		if (*object == NULL) {
			*object = swrap;
		}else{
			/* Insert it into the containing object ... 
			 *
			 * The passing of "attribs" from this element
			 * is a reasonable kludge for inserting elements into
			 * hash tables.  This works because we're using the
			 * form <elem name="name">value</elem>.  Other
			 * ways of doing this (like XML-RPC) require
			 * something more like
			 * <struct><name>Name</name><value>Value</value>
			 * This kind of construction requires keeping
			 * a scratch GHashTable associated with the
			 * <struct> object.  In this case the struct
			 * construction represents a hash table...
			 *
			 * So, the fix to this would be to create a
			 * scratch GHashTable for each tag for the
			 * use of tags below it which might want to
			 * use it.  Perhaps this behavior could be triggered
			 * by the containing object if it knows it needs
			 * one.  It would be NULL otherwise, and then
			 * would have to be destroyed when we left the
			 * context.
			 *
			 * Anyway, this requires some more thought...
			 */
			gXML_insert_item(object, swrap, attribs);
		}
		if (XMLDEBUG) {
			if (object) {
				dump_struct("gXML_entity_list return2: "
				,	*object);
			}
			fprintf(stderr
			,	"RETURNING: Entity_list ::= XMLSTR(%s)\n"
			,	string->str);
		}
     		return TRUE;


	case T_LT:	/* Peek: we saw a '<' (an optimization) */
			/* We let tagset process the '<' */
		if (XMLDEBUG) {
			fprintf(stderr, "Entity List: got T_LT\n");
		}
		if (XMLDEBUG) {
			fprintf(stderr, "Entity_list ::=  TagSet Entity_List\n"
			);
		}
		if (XMLDEBUG && parentobject) {
			dump_struct("parent object before call"
		       " to gXML_tagset", *parentobject);
		}
		if (gXML_tagset(&ts_obj, scanner, token, parentobject)) {
			if (XMLDEBUG && parentobject) {
				dump_struct("parent after call"
			       " to gXML_tagset", *parentobject);
			}
			if (XMLDEBUG) {
				fprintf(stderr
				,	"Entity_list ::=  TagSet found...\n");
				fprintf(stderr
				,	"Calling Entity_list recursively...\n"
				);
			}
			if (gXML_Entity_list(&ts_obj, scanner, token
			,		attribs, parentobject)){
				if (ts_obj != NULL) {
					if (XMLDEBUG) {
						dump_struct("gXML_entity_list"
						"	tagset return: "
						,	ts_obj);
					}
					gXML_insert_item(object, ts_obj
					,	attribs);
				}
				if (XMLDEBUG) {
					fprintf(stderr
					,	"RETURNING: Entity_list"
					" ::= TagSet Entity_List\n");
				}
				return TRUE;
			}else{
				gXML_syntax_error(
				"entity list incomplete after tagset");
			}
		}else{
			gXML_syntax_error("expected XML tagset after '<'");
		}
		return FALSE;

	default:	/* empty entity list */
		if (XMLDEBUG) {
			fprintf(stderr, "Entity_list ::= {empty}\n");
			if (object) {
				dump_struct("gXML_entity_list empty return: "
				,	*object);
			}
		}
		return TRUE;
	}/*endswitch*/
}/* gXML_Entity_list */
 
 
/*
 * An attribute list (Attr_list) is a list of name="value" pairs found
 * inside an open tag...
 *
 *	Attr_list	::=	WORD = QUOTED_STRING Attr_list
 *			::=	{empty}
 */
 
gboolean
gXML_Attr_list(GHashTable* attribs, gXML_scanner* scanner, gXML_token* token)
{
	GString* name = NULL;
	GString* value = NULL;
    
	if (token->peek == T_WORD) {
		if (XMLDEBUG) {
 			fprintf(stderr, "Attr_list ::= WORD = \"string\"\n");
		}
		name = g_string_new(token->TokString->str);
		*token = gXML_scan(scanner);

		if (token->peek != T_EQ) {
			gXML_syntax_error("No equals sign in attribute");
			return FALSE;
		}else{
			*token = gXML_scan(scanner);

			if (token->peek != T_QSTR) {
				gXML_syntax_error(
				"No quoted string in attribute");
				return FALSE;
			}else{
				value = g_string_new(token->TokString->str);
				*token = gXML_scan(scanner);
				if (XMLDEBUG) {
					fprintf(stderr, "Inserting (%s,%s) into attr list\n"
					,	name->str, value->str);
				}
				g_hash_table_insert(attribs, name->str, value);
				/* FIXME - should name be wrapped or not? */
                
				if (!gXML_Attr_list(attribs, scanner, token)){
					/* SYNTAX ERROR PRINTED IN FAILED CALL */
					return FALSE;
				} /* !gXML_Attr_list */
				if (XMLDEBUG) {
					fprintf(stderr, "RETURNING: Attr_list ::= WORD = \"string\"\n");
				}
			} /* else */
		} /* if !T_EQ */
	}else{
		if (XMLDEBUG) {
 			fprintf(stderr, "Attr_list ::= {empty}\n");
		}
	}
    	return TRUE;
    
 }/* gXML_Attr_list */
 
/*
 * gXML_create_item: create an object of the type associated with the
 *	given tag name (and attributes).
 *
 *	It should go through the function tables for this tag
 *	type and call the create function associated with the tag.
 *
 *	This would allow this code to be extended without modifying this
 *	function.
 */
gXML_wrapper*
gXML_create_item(GString* tagname, GHashTable* attributes)
{
	gXML_wrapper* created = NULL;
	if (XMLDEBUG) {
		fprintf(stderr, "===== Create Item: %s\n", tagname->str);
	}
	if (strcmp(tagname->str, "sll") == 0) {
		created = gXML_wrap_GSList(NULL);
	}else if (strcmp(tagname->str, "dll") == 0) {
		created = gXML_wrap_GList(NULL);
	}else if (strcmp(tagname->str, "aarray") == 0) {
		created = gXML_wrap_GHashTable(g_hash_table_new(g_str_hash
		,	g_str_equal));
	}else{
		if (XMLDEBUG) {
			fprintf(stderr
			,	"attempt to create '%s' object\n"
			,	tagname->str);
		}
		return created;
	}
	if (XMLDEBUG) {
		dump_struct("gXML_create_item return: ", created);
	}
	return created;
}/* gXML_create_item */
  

 
void
gXML_syntax_error(const gchar* errormessage)
{
	fprintf(stderr, "Syntax error!  %s\n", errormessage);
}/* gXML_syntax_error */

static void
dump_struct(const char * label, gXML_wrapper* w)
{
	GString*	result;
#if 0
	fprintf(stderr, ">>>>in dump_struct(%s, 0x%lx)\n", label, (unsigned long)w);
#endif
	fprintf(stderr, "STRUCT::::::::::::: %s ", label);
	result = gXML_out(w);

	fprintf(stderr, "%s\n", result->str);
	g_string_free(result, TRUE);
#if 0
	fprintf(stderr, "<<<<leaving dump_struct(%s, 0x%lx)<<<<\n", label, (unsigned long)w);
#endif
}
