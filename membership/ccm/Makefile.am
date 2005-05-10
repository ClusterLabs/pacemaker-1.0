#
# heartbeat: Linux-HA heartbeat code
#
# Copyright (C) 2001 Michael Moerz
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
			-I$(top_builddir)/stonith -I$(top_srcdir)/stonith \
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl \
                        -I$(top_builddir)/include/clplumbing \
			-I$(top_srcdir)/include/clplumbing

hadir			= $(sysconfdir)/ha.d
halibdir		= $(libdir)/@HB_PKG@
ccmlibdir		= $(halibdir)
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
havarrundir		= $(localstatedir)/run/

ccmdir			= $(havarlibdir)/ccm
apigid			= @HA_APIGID@
ccmuid			= @HA_CCMUID@
gliblib			= @GLIBLIB@


AM_CFLAGS			= @CFLAGS@	\
-DVAR_RUN_D='"$(havarrundir)"'	\
-DVAR_LIB_D='"$(havarlibdir)"'		\
-DHA_D='"$(hadir)"'			\
-DHALIB='"$(halibdir)"'

noinst_HEADERS		= ccm.h ccmlib.h

## libraries

lib_LTLIBRARIES		= libccmclient.la libclm.la


## binary progs
ccmlib_PROGRAMS		= ccm ccm_testclient

## SOURCES
ccm_SOURCES	= ccm.c \
		  ccmmisc.c \
		  ccmgraph.c \
		  ccmversion.c \
		  ccmupdate.c \
		  ccmllm.c \
		  ccmbitmap.c \
		  ccm.h \
		  ccmmain.c \
		  ccmclient.c \
		  ccmmsg.c

ccm_LDADD	= $(top_builddir)/lib/clplumbing/libplumb.la	\
		$(top_builddir)/lib/hbclient/libhbclient.la  	\
		$(gliblib) 

ccm_LDFLAGS		= 

ccm_testclient_SOURCES	= ccm_testclient.c
ccm_testclient_LDADD	= libccmclient.la \
			  $(top_builddir)/lib/clplumbing/libplumb.la \
			  $(gliblib) 

libccmclient_la_SOURCES	= ccmlib_memapi.c ccmlib_eventapi.c ccmlib.h
libccmclient_la_LDFLAGS	= -version-info 1:0:0


## library libclm.la
libclmdir		= $(libdir)/@HB_PKG@

## binary program
libclm_PROGRAMS		= clmtest

libclm_la_SOURCES	= ccmlib_clm.c

## Add -DPOSIX_THREADS to CFLAGS to compile a thread-safe version library
libclm_la_CFLAGS	= -DHA_VARLIBDIR='"@HA_VARLIBDIR@"' -g #-DPOSIX_THREADS
libclm_la_LDFLAGS	= -version-info 1:0:0

clmtest_SOURCES		= clmtest.c
clmtest_LDADD		= libclm.la libccmclient.la \
			  $(top_builddir)/lib/clplumbing/libplumb.la \
			  $(gliblib)

## additional Makefile targets
# additional installations not covered normally
install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(ccmdir)
	-chgrp $(apigid) $(DESTDIR)/$(ccmdir)
	-chown $(ccmuid) $(DESTDIR)/$(ccmdir)
	chmod 750 $(DESTDIR)/$(ccmdir)


uninstall-local:
	rm -fr $(DESTDIR)$(ccmdir)
