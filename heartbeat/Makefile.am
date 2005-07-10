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
			-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha \
			-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

hadir			= $(sysconfdir)/ha.d
habindir		= @bindir@
halibdir		= $(libdir)/@HB_PKG@
commmoddir		= $(halibdir)/modules/comm
havarlibdir		= $(localstatedir)/lib/@HB_PKG@
havarrundir		= $(localstatedir)/run
havarrunhbdir		= $(localstatedir)/run/@HB_PKG@
# fifos with path
hafifo			= $(havarlibdir)/fifo
apigid			= @HA_APIGID@
gliblib			= @GLIBLIB@

LIBRT 			= @LIBRT@
AM_CFLAGS			= @CFLAGS@ \
-DVAR_RUN_D='"$(havarrundir)"'	\
-DVAR_LIB_D='"$(havarlibdir)"'		\
-DHA_D='"$(hadir)"'			\
-DHALIB='"$(halibdir)"'

## script subdirs
SUBDIRS			= init.d lib logrotate.d rc.d libnet_util

noinst_HEADERS		=	hb_config.h		\
				hb_module.h		\
				hb_proc.h		\
				hb_resource.h		\
				hb_signal.h		\
				heartbeat_private.h	\
				test.h


## binary progs
halib_PROGRAMS		= heartbeat findif

## SOURCES
heartbeat_SOURCES	= heartbeat.c auth.c				\
			config.c \
			ha_msg_internal.c hb_api.c hb_resource.c	\
			hb_signal.c module.c hb_uuid.c

heartbeat_LDADD		= $(top_builddir)/lib/stonith/libstonith.la	\
			$(top_builddir)/lib/pils/libpils.la		\
			$(top_builddir)/lib/clplumbing/libplumb.la	\
			$(top_builddir)/lib/apphb/libapphb.la		\
			$(top_builddir)/replace/libreplace.la		\
			$(gliblib) $(LIBRT)

heartbeat_LDFLAGS	= @LIBADD_DL@ @LIBLTDL@ -export-dynamic	\
			-dlpreopen force \
			-dlopen self

heartbeat_CFLAGS       = $(AM_CFLAGS)

findif_SOURCES		= findif.c
findif_LDADD		=$(top_builddir)/replace/libreplace.la

## SCRIPTS/DATA
ha_DATA			= shellfuncs README.config 
ha_SCRIPTS		= harc


EXTRA_DIST              = $(ha_DATA) $(ha_SCRIPTS)


## additional Makefile targets
# additional installations not covered normally
install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(havarrundir) 
	$(mkinstalldirs) $(DESTDIR)$(havarlibdir)
	$(mkinstalldirs) $(DESTDIR)$(havarrunhbdir)
	-chmod 750 $(DESTDIR)$(havarrundir)
	-chmod 750 $(DESTDIR)$(havarrunhbdir)

uninstall-local:
	-test -p $(DESTDIR)$(hafifo) && rm $(DESTDIR)$(hafifo)
	rm -rf $(DESTDIR)$(havarrunhbdir)

