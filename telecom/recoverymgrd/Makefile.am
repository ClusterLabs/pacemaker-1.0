#
# heartbeat: Linux-HA telecom code: sample recovery manager daemon
#
# Copyright (C) 2002 Intel Corporation
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
MAINTAINERCLEANFILES    = Makefile.in

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@

COMMONLIBS		= $(top_builddir)/lib/clplumbing/libplumb.la	\
			$(GLIBLIB)

LIBRT 			= @LIBRT@

AM_CFLAGS		=  -DPIDFILE='"$(PIDFILE)"'


## binary progs
halib_PROGRAMS		= recoverymgrd 

## SOURCES

recoverymgrd_SOURCES	= conf_yacc.yacc conf_lex.lex recoverymgrd.c lex.yy.c y.tab.c y.tab.h
noinst_HEADERS = recoverymgrd.h configfile.h

recoverymgrd_CFLAGS		= -DHA_VARLIBDIR='"@HA_VARLIBDIR@"'
recoverymgrd_LDADD		= $(COMMONLIBS) 			\
			$(top_builddir)/lib/pils/libpils.la		\
			$(top_builddir)/lib/apphb/libapphb.la		\
			$(top_builddir)/replace/libreplace.la		\
			$(LIBRT)

recoverymgrd_LDFLAGS	= @LIBADD_DL@ @LIBLTDL@ -export-dynamic	\
			-dlpreopen force \
			-dlopen self

lex.yy.c: conf_lex.lex
	lex conf_lex.lex

y.tab.c: conf_yacc.yacc lex.yy.c recoverymgrd.c
	yacc -d conf_yacc.yacc

y.tab.h: y.tab.c # Automatically built at the same time

recoverymgrd-recoverymgrd.$(OBJEXT): y.tab.h y.tab.c
