# $Id: Makefile.am,v 1.5 2005/06/25 18:43:30 msoffen Exp $
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

EXTRA_DIST =	\
		portMakefile			\
		heartbeat/pkg-deinstall		\
		heartbeat/pkg-descr		\
		heartbeat/pkg-install		\
		heartbeat/pkg-plist

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
	$(RM) $(TARFILE)
	$(CP) portMakefile port/Makefile
	$(TAR) -cvf heartbeat.tar heartbeat/Makefile heartbeat/distinfo heartbeat/pkg-deinstall heartbeat/pkg-descr heartbeat/pkg-install heartbeat/pkg-plist
	$(GZIP) heartbeat.tar


clean:
	$(RM) $(TARFILE) heartbeat.tar port/distinfo port/Makefile
	
clean-local:
	$(RM) -rf $(TMPINSTDIR) $(PKGDIR)
