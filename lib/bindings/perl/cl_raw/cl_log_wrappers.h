/* $Id: cl_log_wrappers.h,v 1.2 2004/03/04 13:22:06 lars Exp $ */
/*
 * Some helpers for wrapping the cl_log functionality for Perl.
 *
 * Copyright (c) 2004 Lars Marowsky-Brée <lmb@suse.de>
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
 *
 */

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

