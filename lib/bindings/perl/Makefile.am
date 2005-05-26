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


EXTRA_DIST =	.cvsignore Makefile.PL.in MANIFEST test.pl 	\
		cl_raw/.cvsignore				\
		cl_raw/Makefile.PL.in cl_raw/Makefile.PL 	\
		cl_raw/cl_log_wrappers.c			\
		cl_raw/cl_log_wrappers.h  cl_raw/cl_raw.i 	\
		cl_raw/ipc_wrappers.c cl_raw/ipc_wrappers.h	\
		lib/heartbeat/clplumbing/ipc.pm			\
		lib/heartbeat/clplumbing/log.pm

if SWIG_BUILD

noinst_DATA = .made_mod

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
	-I$(top_builddir)/linux-ha -I$(top_srcdir)/linux-ha  \
	-I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl

SUB_INCLUDES = -I$(top_builddir)/../include -I$(top_srcdir)/../include \
	-I$(top_builddir)/../linux-ha -I$(top_srcdir)/../linux-ha  \
	-I$(top_builddir)/../libltdl -I$(top_srcdir)/../libltdl

Makefile.perl: Makefile.PL
	( old_pwd=`pwd` ; cd $(srcdir) && \
	$(PERL) $< && \
	test $@ -ef $$old_pwd/$@ || mv $@ $$old_pwd/; )

.made_mod: cl_raw/cl_raw.pm Makefile.perl
	( old_pwd=`pwd` ; cd $(srcdir) && \
	$(MAKE) -f$$old_pwd/Makefile.perl INCLUDES="$(SUB_INCLUDES)" )
	touch $@

cl_raw/cl_raw.pm: cl_raw/cl_raw.i
	( old_pwd=`pwd` ; cd $(srcdir)/cl_raw && \
	$(SWIG) $(SWIG_OPTIONS) $(SUB_INCLUDES) -perl cl_raw.i && \
	test cl_raw.pm -ef $$old_pwd/cl_raw/cl_raw.pm || \
	mv $@ $$old_pwd/cl_raw/; )

install-data-local:
	( old_pwd=`pwd` ; cd $(srcdir) && \
	$(MAKE) -f$$old_pwd/Makefile.perl PREFIX=$(prefix) \
	$(PERL_INSTALL_TARGET) )

CLEANFILES = .made_mod cl_raw/cl_raw.pm cl_raw/cl_raw_wrap.c

clean-local:
	if test -f Makefile.perl; then $(MAKE) -fMakefile.perl realclean; fi
	rm -f Makefile.perl Makefile.perl.old cl_raw/Makefile.perl.old
endif
