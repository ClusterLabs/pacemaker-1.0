#
# Copyright (C) 2008 Andrew Beekhof
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

-include Makefile

PACKAGE		?= pacemaker

# Force 'make dist' to be consistent with 'make export' 
#distdir		= $(PACKAGE)-$(VERSION)
distdir			= $(PACKAGE)
TARFILE			= $(distdir).tar.bz2
DIST_ARCHIVES		= $(TARFILE)

LAST_RELEASE		= Pacemaker-1.0.9.1
STABLE_SERIES		= stable-1.0

RPM_ROOT	= $(shell pwd)
RPM_OPTS	= --define "_sourcedir $(RPM_ROOT)" 	\
		  --define "_specdir   $(RPM_ROOT)" 	\
		  --define "_srcrpmdir $(RPM_ROOT)" 	\

# Default to fedora compliant spec files
# SLES:     /etc/SuSE-release
# openSUSE: /etc/SuSE-release
# RHEL:     /etc/redhat-release
# Fedora:   /etc/fedora-release, /etc/redhat-release, /etc/system-release
getdistro = $(shell test -e /etc/SuSE-release || echo fedora; test -e /etc/SuSE-release && echo suse)
DISTRO ?= $(call getdistro)
TAG    ?= tip

export:
	rm -f $(TARFILE)
	hg archive -t tbz2 -r $(TAG) $(TARFILE)
	echo `date`: Rebuilt $(TARFILE) from $(TAG)

pacemaker-fedora.spec: pacemaker.spec
	cp $(PACKAGE).spec $(PACKAGE)-$(DISTRO).spec
	@echo Rebuilt $@

pacemaker-epel.spec: pacemaker.spec
	cp $(PACKAGE).spec $(PACKAGE)-$(DISTRO).spec
	@echo Rebuilt $@

pacemaker-suse.spec: pacemaker.spec
	cp $(PACKAGE).spec $@
	sed -i.sed s:%{_docdir}/%{name}:%{_docdir}/%{name}-%{version}:g $@
	sed -i.sed s:corosynclib:libcorosync:g $@
	sed -i.sed s:pacemaker-libs:libpacemaker3:g $@
	sed -i.sed s:heartbeat-libs:heartbeat:g $@
	sed -i.sed s:cluster-glue-libs:libglue:g $@
	sed -i.sed s:libselinux-devel:automake:g $@
	sed -i.sed s:lm_sensors-devel:automake:g $@
	sed -i.sed s:bzip2-devel:libbz2-devel:g $@
	sed -i.sed s:Development/Libraries:Development/Libraries/C\ and\ C++:g $@
	sed -i.sed s:System\ Environment/Daemons:Productivity/Clustering/HA:g $@
	sed -i.sed s:\#global\ py_sitedir:\%global\ py_sitedir:g $@
	@echo Rebuilt $@

srpm:	export $(PACKAGE)-$(DISTRO).spec
	rm -f *.src.rpm
	rpmbuild -bs --define "dist .$(DISTRO)" $(RPM_OPTS) $(PACKAGE)-$(DISTRO).spec

rpm:	srpm
	@echo To create custom builds, edit the flags and options in $(PACKAGE)-$(DISTRO).spec first
	rpmbuild --rebuild $(RPM_ROOT)/*.src.rpm

mock:   srpm
	-rm -rf $(RPM_ROOT)/mock
	mock --root=fedora-12-x86_64 --resultdir=$(RPM_ROOT)/mock --rebuild $(RPM_ROOT)/*.src.rpm

scratch:
	hg commit -m "DO-NOT-PUSH"
	make mock
	hg rollback

deb:	
	echo To make create custom builds, edit the configure flags in debian/rules first
	dpkg-buildpackage -rfakeroot -us -uc 

global: clean-generic
	gtags -q

global-html: global
	htags -sanhIT

global-www: global-html
	rsync -avzxlSD --progress HTML/ root@clusterlabs.org:/var/lib/global/pacemaker

changes:
	@printf "\n* `date +"%a %b %d %Y"` `hg showconfig ui.username` $(VERSION)-1"
	@printf "\n- Update source tarball to revision: `hg id`"
	@printf "\n- Statistics:\n"
	@printf "  Changesets: `hg log -M --template "{desc|firstline|strip}\n" -r $(LAST_RELEASE):tip | wc -l`\n"
	@printf "  Diff:      "
	@hg diff -r $(LAST_RELEASE):tip | diffstat | tail -n 1
	@printf "\n- Changes since $(LAST_RELEASE)\n"
	@hg log -M --template "  + {desc|firstline|strip}\n" -r $(LAST_RELEASE):tip | grep -v -e Dev: -e Low: -e Hg: -e "Added tag.*for changeset" | sort -uf 
	@printf "\n"

rel-tags: tags
	find . -name TAGS -exec sed -i.sed 's:\(.*\)/\(.*\)/TAGS:\2/TAGS:g' \{\} \;
