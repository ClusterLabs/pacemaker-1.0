/*
 *  util_funcs.h:  utilitiy functions for extensible groups.
 */
#ifndef _UCD_MIBGROUP_UTIL_FUNCS_H
#define _UCD_MIBGROUP_UTIL_FUNCS_H

int header_simple_table (struct variable *, oid *,  size_t *, int,  size_t *, WriteMethod **write_method, int);
int header_generic (struct variable *,oid *,  size_t *, int,  size_t *, WriteMethod **);

#endif /* _UCD_MIBGROUP_UTIL_FUNCS_H */
