/* $Id: cl_log_wrappers.h,v 1.1 2004/03/04 11:33:12 lars Exp $ */

/* These two wrapper functions need to be in place, because SWIG does
 * not grok the G_GNUC_PRINTF attributes, and the scripting languages
 * like to pass us simple strings */
void            cl_log_helper(int priority, const char *s);
void            cl_perror_helper(const char * s);

/* The remaining functions are copied verbatim from the cl_log.h */
void            cl_log_enable_stderr(int truefalse);
void            cl_log_send_to_logging_daemon(int truefalse);
void            cl_log_set_facility(int facility);
void            cl_log_set_entity(const char *  entity);
void            cl_log_set_logfile(const char * path);
void            cl_log_set_debugfile(const char * path);

