#
# linux-ha: Linux-HA heartbeat code
#
# Copyright (C) 2001 Michael Moerz
#                    This instance created by Horms
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

EXTRA_DIST		= apply changelog compat control copyright \
			  dpkg-checkbuild rules \
			  heartbeat-2.dirs heartbeat-2.files \
			  heartbeat-2.postinst heartbeat-2.postrm \
			  heartbeat-2.preinst \
			  heartbeat-2-dev.dirs heartbeat-2-dev.files \
			  ldirectord-2.files ldirectord-2.postinst \
			  ldirectord-2.postrm \
			  patches/chown-check-2.patch \
			  patches/series/2.0.8-1


clean-local:
	rm -f *.substvars *.debhelper files
	rm -r -f heartbeat heartbeat-dev ldirectord \
		libpils0 libpils-dev libstonith0 libstonith-dev stonith tmp
