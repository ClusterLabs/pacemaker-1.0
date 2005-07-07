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
			  heartbeat-dev.dirs heartbeat-dev.files \
			  heartbeat.dirs heartbeat.files heartbeat.postinst \
			  heartbeat.postrm heartbeat.preinst \
			  ldirectord.files ldirectord.postinst \
			  ldirectord.postrm \
			  libpils-dev.dirs libpils-dev.files \
			  libpils0.dirs libpils0.files \
			  libstonith-dev.dirs libstonith-dev.files \
			  libstonith0.dirs libstonith0.files \
			  libstonith0.postinst \
			  stonith.files \
			  patches/00-ssh_scp_path.patch \
			  patches/01-conditional_build.patch \
			  patches/series/1.99.6-1


clean-local:
	rm -f *.substvars *.debhelper files
	rm -r -f heartbeat heartbeat-dev ldirectord \
		libpils0 libpils-dev libstonith0 libstonith-dev stonith tmp
