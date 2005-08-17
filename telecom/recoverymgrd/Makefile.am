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

CLEANFILES		= conf_lex.c  conf_yacc.c  conf_yacc.h

INCLUDES                = -I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@

COMMONLIBS		= $(top_builddir)/lib/clplumbing/libplumb.la	\
			$(GLIBLIB)

LIBRT 			= @LIBRT@

	
#
#	Since all this code compiles cleanly without warnings, we
#	disallow all warnings here just like everywhere else.	
#	If your version of lex or yacc is broken, then work around	
#	it.  This has been done for the current Linux lex/yacc
#	(bison version 1.875  and flex version 2.5.4 and others)
#
#	Setting CFLAGS here is specifically disallowed by automake
#	and causes a warning.
#
#	So, I reverted an incorrect  CFLAGS change and restored the comments
#	above which used to be here for these two reasons.  Before you remove
#	these comments or turn off warnings *again*, let's discuss this.
#
AM_CFLAGS		=  -DVAR_RUN_D='"$(havarrundir)"' \
-DHA_VARLIBDIR='"@HA_VARLIBDIR@"'

AM_YFLAGS		= -d


## binary progs
halib_PROGRAMS		= recoverymgrd

## SOURCES

recoverymgrd_SOURCES	= conf_yacc.y conf_lex.l recoverymgrd.c
noinst_HEADERS 		= recoverymgrd.h configfile.h conf_yacc.h

recoverymgrd_CFLAGS	=$(AM_CFLAGS)
recoverymgrd_LDADD	= $(COMMONLIBS) 				\
			$(top_builddir)/lib/pils/libpils.la		\
			$(top_builddir)/lib/apphb/libapphb.la		\
			$(LIBRT)

recoverymgrd_LDFLAGS	= @LIBADD_DL@ @LIBLTDL@ -export-dynamic		\
			-dlpreopen-force 				\
			-dlopen-self

conf_lex.c: conf_yacc.h

