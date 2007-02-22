/* gXMLwrap.h */
/* for the gXML library */
/* library of functions to wrap and unwrap data using the gXML_wrapper */
/*  structure defined in gXML.h */

#include "gXML.h"

#ifndef gXML_WRAP_H
#define gXML_WRAP_H

/* ****************** WRAPPING FUNCTIONS *********************** */
/* Preconditions: The data pointer passed in is of the proper type
 * Postconditions: A wrapper is created, and the data is wrapped.
 *  The gXML_wrapper's data field points to the data.
 *  The gXML_wrapper's identifier field is set properly
 *  The pointer to the gXML_wrapper is returned as a gpointer
 * The magic numbers for the wrapper's "identifier" field are as follows: 
 *  0 - GString
 *  1 - GSList
 *  2 - GList
 *  3 - GHashTable
 */

gXML_wrapper* gXML_wrap_GString(GString*);

gXML_wrapper* gXML_wrap_GSList(GSList*);

gXML_wrapper* gXML_wrap_GList(GList*);

gXML_wrapper* gXML_wrap_GHashTable(GHashTable*);

/******************* UNWRAPPING FUNCTIONS *********************
 * Preconditions: The gXML_wrapper passed in is a valid gXML_wrapper, and
 *  the identifier field matches the function
 * Postconditions: The data pointer is returned
 * The magic numbers for the wrapper's "identifier" field are as follows: 
 *  0 - GString
 *  1 - GSList
 *  2 - GList
 *  3 - GHashTable
 */

GString* gXML_unwrap_GString(gXML_wrapper*);

GSList* gXML_unwrap_GSList(gXML_wrapper*);

GList* gXML_unwrap_GList(gXML_wrapper*);

GHashTable* gXML_unwrap_GHashTable(gXML_wrapper*);

#endif
