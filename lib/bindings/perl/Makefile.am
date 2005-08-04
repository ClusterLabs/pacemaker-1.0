#
# Makefile.am for Perl language bindings for heartbeat
#
# Copyright (C) 2004 Lars Marowsky-Brée
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


EXTRA_DIST =	.cvsignore Makefile.PL.in MANIFEST test.pl \
		lib/heartbeat/clplumbing/ipc.pm \
		lib/heartbeat/clplumbing/log.pm

MAINTAINERCLEANFILES    = Makefile.in

if SWIG_BUILD
SWIG_DIR = cl_raw
endif

## Subdirectories...
SUBDIRS			=  	\
	$(SWIG_DIR)
