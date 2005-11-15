# $Id: Makefile.am,v 1.14 2005/11/15 13:11:17 davidlee Exp $
#
# "pkg" Makefile for Solaris etc.
#
# Copyright (C) 2001, 2004 David Lee
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

# Overview of principle:
# 1. "make install" as a pseudo-install to a temporary location $(TMPINSTDIR)
# 2. "pkgmk" from whatever is in $(TMPINSTDIR) into a pkg area $(PKGDIR)

MAINTAINERCLEANFILES    = Makefile.in

PKGINFO =	$(INFOFILES)/prototype		\
		$(INFOFILES)/pkginfo		\
		$(INFOFILES)/preinstall		\
		$(INFOFILES)/postinstall

EXTRA_DIST =	$(INFOFILES)/pkginfo.in		\
		$(INFOFILES)/preinstall.in	\
		$(INFOFILES)/postinstall.in

CLEANFILES = stamp-tmpinst $(INFOFILES)/prototype

DISTCLEANFILES = $(PKGINFO)

PKGDIR		= packages

INFOFILES	= InfoFiles

# A pseudo-install directory, so that we know what files are generated.
# This allows libtool to maintain its flexibility.
TMPINSTDIR	= tmpinst

# Main target
pkg:	$(PKGDIR)/$(PKGNAME)/pkgmap

# Build the "pkg" from the pseudo-install
$(PKGDIR)/$(PKGNAME)/pkgmap:	$(PKGINFO)
	-rm -rf  $(PKGDIR)
	-mkdir  $(PKGDIR)
	cd $(INFOFILES) && pkgmk -d ../$(PKGDIR) -r ../$(TMPINSTDIR)

# pseudo-install into $(TMPINSTDIR)
stamp-tmpinst:
	cd $(top_builddir) \
	  && (umask 022 && $(MAKE) install DESTDIR=$(PWD)/$(TMPINSTDIR))
	echo timestamp > stamp-tmpinst

## The awk in here is hacky.  Its intention is to tweak owner:group information
## for the files, directories, named pipes, etc.  Most should end up as
## "root:sys", but a few should have either HA_CCMUSER or HA_APIGROUP or both.
##
## Note that there is the wider problem of maintaining this information
## (owner, group, etc.) across the various operating system package mechanisms.
$(INFOFILES)/prototype:	stamp-tmpinst
	-mkdir $(INFOFILES)
	(cd $(TMPINSTDIR) && pkgproto .) \
	  | $(AWK) ' \
	    $$1 !~ /^[ils]$$/ \
	      { $$(NF-1) = "root" ; $$(NF) = "sys" } \
	    "/" $$3 ~ /^(\/etc|\/etc\/init.d|\/opt|\/var|\/var\/run)$$/ \
	      { $$4 = $$5 = $$6 = "?" } \
	    "/" $$3 ~ /^(\/usr|\/usr\/lib|\/usr\/lib\/ocf)$$/ \
	      { $$4 = $$5 = $$6 = "?" } \
	    "/" $$3 == "@HA_VARRUNDIR@/@HB_PKG@/ccm" \
	      { $$5 = "@HA_CCMUSER@" ; $$6 = "@HA_APIGROUP@" } \
	    "/" $$3 == "@HA_VARRUNDIR@/@HB_PKG@/crm" \
	      { $$5 = "@HA_CCMUSER@" ; $$6 = "@HA_APIGROUP@" } \
	    "/" $$3 == "@HA_VARLIBDIR@/@HB_PKG@/crm" \
	      { $$5 = "@HA_CCMUSER@" ; $$6 = "@HA_APIGROUP@" } \
	    "/" $$3 == "@HA_COREDIR@/@HA_CCMUSER@" \
	      { $$5 = "@HA_CCMUSER@" } \
	    "/" $$3 == "@HA_COREDIR@/nobody" \
	      { $$5 = "nobody" } \
	    "/" $$3 == "@bindir@/cl_status" \
	      { $$5 = "@HA_CCMUSER@" ; $$6 = "@HA_APIGROUP@" } \
	    { print ; } \
	  ' > $@
	echo "d none etc/rc2.d ? ? ?" >> $@
	echo "d none etc/rc0.d ? ? ?" >> $@
	echo "d none etc/rc1.d ? ? ?" >> $@
	echo "d none etc/rcS.d ? ? ?" >> $@
	echo "s none etc/rc2.d/S@HB_INITSTARTPRI@heartbeat=@INITDIR@/heartbeat@INIT_EXT@" >> $@
	echo "s none etc/rc0.d/K@HB_INITSTOPPRI@heartbeat=@INITDIR@/heartbeat@INIT_EXT@" >> $@
	echo "s none etc/rc1.d/K@HB_INITSTOPPRI@heartbeat=@INITDIR@/heartbeat@INIT_EXT@" >> $@
	echo "s none etc/rcS.d/K@HB_INITSTOPPRI@heartbeat=@INITDIR@/heartbeat@INIT_EXT@" >> $@
	echo "i pkginfo" >> $@
	echo "i preinstall" >> $@
	echo "i postinstall" >> $@

clean-local:
	rm -rf $(TMPINSTDIR) $(PKGDIR)
