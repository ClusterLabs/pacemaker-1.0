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
	$(MAKE) -fMakefile.perl PREFIX=$(prefix) install

CLEANFILES = .made_mod cl_raw/cl_raw.pm cl_raw/cl_raw_wrap.c

clean-local:
	if test -f Makefile.perl; then $(MAKE) -fMakefile.perl realclean; fi
	rm -f Makefile.perl Makefile.perl.old cl_raw/Makefile.perl.old
