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

MAINTAINERCLEANFILES    = Makefile.in

EXTRA_DIST      = \
	.cvsignore \
	Makefile.PL.in Makefile.PL \
	cl_log_wrappers.c cl_log_wrappers.h \
	cl_raw.i \
	ipc_wrappers.c ipc_wrappers.h

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
	-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl    \
	-I$(top_builddir)/lib/bindings/perl/cl_raw -I$(top_srcdir)/lib/bindings/perl/cl_raw

.made_mod: cl_raw.c Makefile.perl
	$(MAKE) -f Makefile.perl INCLUDES="$(INCLUDES)"

cl_raw.c: cl_raw.i 
	$(SWIG) -perl5 $(SWIGFLAGS) $(INCLUDES) -o $@ $^

Makefile.perl: Makefile.PL
	$(PERL) $< MAKEFILE=$@

#lib_LTLIBRARIES         = libcl_log.la

#libcl_log_la_SOURCES    = cl_log_wrappers.c cl_raw_wrap.c ipc_wrappers.c
#	$(MAKE) -f Makefile.perl INCLUDES="$(SUB_INCLUDES)"

cl_raw.pm: 
	$(SWIG) $(SWIG_OPTIONS) $(INCLUDES) -perl cl_raw.i && \
	test cl_raw.pm -ef cl_raw.pm

install-data-local:
	$(MAKE) -fMakefile.perl PREFIX=$(prefix) $(PERL_INSTALL_TARGET)

CLEANFILES = .made_mod cl_raw.pm cl_raw_wrap.c

clean-local:
	if test -f Makefile.perl; then $(MAKE) -fMakefile.perl realclean; fi
	rm -f Makefile.perl Makefile.perl.old
