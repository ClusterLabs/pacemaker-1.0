# $Id: Makefile.am,v 1.1 2005/06/13 20:08:00 msoffen Exp $
#
# Open-HA "port" Makefile for FreeBSD
#
# Copyright (C) 2005 Matthew Soffen
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

EXTRA_DIST =	port/Makefile.in		\
		port/distinfo			\
		port/pkg-deinstall		\
		port/pkg-descr			\
		port/pkg-install		\
		port/pkg-plist

CLEANFILES = 

DISTCLEANFILES = $(PKGINFO)

PKGNAME		= OPENHA

TARFILE		= @TARFILE@
MD5		= @MD5@
GZIP		= @GZIP@

# A pseudo-install directory, so that we know what files are generated.
# This allows libtool to maintain its flexibility.
TMPINSTDIR	= tmpinst

# Main target
portfile:
	$(LN) -s ../$(TARFILE) $(TARFILE)
	$(MD5) $(TARFILE) > port/distinfo
#	 Create the DistInfo file for the port file.
	$(LS) -l ../$(TARFILE) | $(AWK) '{print "SIZE \($(TARFILE)\) = "$$5}' >> port/distinfo
	$(RM) -f $(TARFILE)
	$(CP) portMakefile port/Makefile
	cd port
	$(TAR) -cvf heartbeat.tar Makefile distinfo pkg-deinstall pkg-descr pkg-install pkg-plist
	$(GZIP) heartbeat.tar


clean-local:
	rm -rf $(TMPINSTDIR) $(PKGDIR)
