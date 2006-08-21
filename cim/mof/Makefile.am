# Linux-HA: Makefile for register
#
# Author: Jia Ming Pan <jmltc@cn.ibm.com>
# Copyright (c) 2005 International Business Machines
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

mof_DATA    = LinuxHA.mof LinuxHA.reg 
mof_SCRIPTS = register_providers.sh unregister_providers.sh do_register.sh
mofdir      = $(libdir)/heartbeat/cim

DIST_LIST   = $(mof_DATA) $(mof_SCRIPTS)
EXTRA_DIST  = $(DIST_LIST)
