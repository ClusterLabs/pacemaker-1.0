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

noinst_DATA = .made_mod

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
	-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

Makefile.perl: Makefile.PL
	$(PERL) $<

.made_mod: cl_raw/cl_raw.pm Makefile.perl
	$(MAKE) -fMakefile.perl
	touch $@

cl_raw/cl_raw.pm: cl_raw/cl_raw.i
	$(SWIG) $(SWIG_OPTIONS) $(INCLUDES) -perl -outdir cl_raw $^

install-data-local:
	$(MAKE) -fMakefile.perl PREFIX=$(prefix) $(PERL_INSTALL_TARGET)

CLEANFILES = .made_mod cl_raw/cl_raw.pm cl_raw/cl_raw_wrap.c

clean-local:
	if test -f Makefile.perl; then $(MAKE) -fMakefile.perl realclean; fi
	rm -f Makefile.perl Makefile.perl.old cl_raw/Makefile.perl.old
