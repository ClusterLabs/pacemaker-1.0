%global gname haclient
%global uname hacluster
%global with_ais_support	1
%global with_heartbeat_support	1
%global pcmk_docdir %{_docdir}/%{name}

%global specversion 1
#global upstream_version ee19d8e83c2a
%global upstream_prefix pacemaker

# Keep around for when/if required
#global alphatag %{upstream_version}.hg

%global pcmk_release %{?alphatag:0.}%{specversion}%{?alphatag:.%{alphatag}}%{?dist}

# When downloading directly from Mercurial, it will automatically add a prefix
# Invoking 'hg archive' wont but you can add one with:
# hg archive -t tgz -p "$upstream_prefix-$upstream_version" -r $upstream_version $upstream_version.tar.gz

Name:		pacemaker
Summary:	Scalable High-Availability cluster resource manager
Version:	1.0.6
Release:	%{pcmk_release}
License:	GPLv2+ and LGPLv2+
Url:		http://www.clusterlabs.org
Group:		System Environment/Daemons
Source0:	pacemaker.tar.bz2
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
AutoReqProv:	on
Requires(pre):	cluster-glue
Requires:	resource-agents
Requires:       perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%if 0%{?fedora} || 0%{?centos} > 4 || 0%{?rhel} > 4
BuildRequires:  help2man libtool-ltdl-devel
%endif

# Required for core functionality
BuildRequires:  automake autoconf libtool pkgconfig
BuildRequires:	glib2-devel cluster-glue-libs-devel libxml2-devel libxslt-devel 
BuildRequires:	pkgconfig python-devel gcc-c++ bzip2-devel gnutls-devel pam-devel

# Enables optional functionality
BuildRequires:	ncurses-devel net-snmp-devel openssl-devel 
BuildRequires:	libesmtp-devel lm_sensors-devel libselinux-devel

%if %with_ais_support
BuildRequires:	corosynclib-devel
Requires:	corosync
%endif
%if %with_heartbeat_support
BuildRequires:	heartbeat-devel heartbeat-libs
Requires:	heartbeat >= 3.0.0
%endif

%description
Pacemaker is an advanced, scalable High-Availability cluster resource
manager for Linux-HA (Heartbeat) and/or OpenAIS.

It supports "n-node" clusters with significant capabilities for
managing resources and dependencies.

It will run scripts at initialization, when machines go up or down,
when related resources fail and can be configured to periodically check
resource health.

%package -n pacemaker-libs
License:	GPLv2+ and LGPLv2+
Summary:	Libraries used by the Pacemaker cluster resource manager and its clients
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description -n pacemaker-libs
Pacemaker is an advanced, scalable High-Availability cluster resource
manager for Linux-HA (Heartbeat) and/or OpenAIS.

It supports "n-node" clusters with significant capabilities for
managing resources and dependencies.

It will run scripts at initialization, when machines go up or down,
when related resources fail and can be configured to periodically check
resource health.

%package -n pacemaker-libs-devel 
License:	GPLv2+ and LGPLv2+
Summary:	Pacemaker development package
Group:		Development/Libraries
Requires:	%{name}-libs = %{version}-%{release}
Requires:	cluster-glue-libs-devel
Obsoletes:      libpacemaker3
%if %with_ais_support
Requires:	corosynclib-devel
%endif
%if %with_heartbeat_support
Requires:	heartbeat-devel
%endif

%description -n pacemaker-libs-devel
Headers and shared libraries for developing tools for Pacemaker.

Pacemaker is an advanced, scalable High-Availability cluster resource
manager for Linux-HA (Heartbeat) and/or OpenAIS.

It supports "n-node" clusters with significant capabilities for
managing resources and dependencies.

It will run scripts at initialization, when machines go up or down,
when related resources fail and can be configured to periodically check
resource health.

%prep
%setup -q -n %{upstream_prefix}%{?upstream_version}

%build
./autogen.sh

# RHEL <= 5 doesn't support --docdir
export docdir=%{pcmk_docdir}
%{configure} --localstatedir=%{_var} --enable-fatal-warnings=no
make %{_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# Scripts that need should be executable
chmod a+x %{buildroot}/%{_libdir}/heartbeat/hb2openais-helper.py
chmod a+x %{buildroot}/%{_datadir}/pacemaker/cts/CTSlab.py
chmod a+x %{buildroot}/%{_datadir}/pacemaker/cts/OCFIPraTest.py
chmod a+x %{buildroot}/%{_datadir}/pacemaker/cts/extracttests.py

# These are not actually scripts
find %{buildroot} -name '*.xml' -type f -print0 | xargs -0 chmod a-x
find %{buildroot} -name '*.xsl' -type f -print0 | xargs -0 chmod a-x
find %{buildroot} -name '*.rng' -type f -print0 | xargs -0 chmod a-x
find %{buildroot} -name '*.dtd' -type f -print0 | xargs -0 chmod a-x
 
# Dont package static libs or compiled python
find %{buildroot} -name '*.a' -type f -print0 | xargs -0 rm -f
find %{buildroot} -name '*.la' -type f -print0 | xargs -0 rm -f
find %{buildroot} -name '*.pyc' -type f -print0 | xargs -0 rm -f
find %{buildroot} -name '*.pyo' -type f -print0 | xargs -0 rm -f

# Don't package these either
rm %{buildroot}/%{_libdir}/heartbeat/crm_primitive.py
rm %{buildroot}/%{_libdir}/service_crm.so

%clean
rm -rf %{buildroot}

%post -n pacemaker-libs -p /sbin/ldconfig

%postun -n pacemaker-libs -p /sbin/ldconfig

%files
###########################################################
%defattr(-,root,root)

%{_datadir}/pacemaker
%{_datadir}/snmp/mibs/PCMK-MIB.txt
%{_libdir}/heartbeat/*
%{_sbindir}/cibadmin
%{_sbindir}/crm_attribute
%{_sbindir}/crm_diff
%{_sbindir}/crm_failcount
%{_sbindir}/crm_master
%{_sbindir}/crm_mon
%{_sbindir}/crm
%{_sbindir}/crm_resource
%{_sbindir}/crm_standby
%{_sbindir}/crm_verify
%{_sbindir}/crmadmin
%{_sbindir}/iso8601
%{_sbindir}/attrd_updater
%{_sbindir}/ptest
%{_sbindir}/crm_shadow
%{_sbindir}/cibpipe
%{_sbindir}/crm_node

%if %with_heartbeat_support
%{_sbindir}/crm_uuid
%else
%exclude %{_sbindir}/crm_uuid
%endif

# Packaged elsewhere
%exclude %{pcmk_docdir}/AUTHORS
%exclude %{pcmk_docdir}/COPYING
%exclude %{pcmk_docdir}/COPYING.LIB

%exclude %{pcmk_docdir}/index.html
%doc %{pcmk_docdir}/crm_cli.txt
%doc %{pcmk_docdir}/crm_fencing.txt
%doc %{pcmk_docdir}/README.hb2openais
%doc %{_mandir}/man8/*.8*
%doc COPYING
%doc AUTHORS

%dir %attr (750, %{uname}, %{gname}) %{_var}/lib/heartbeat/crm
%dir %attr (750, %{uname}, %{gname}) %{_var}/lib/pengine
%dir %attr (750, %{uname}, %{gname}) %{_var}/run/crm
%dir /usr/lib/ocf
%dir /usr/lib/ocf/resource.d
/usr/lib/ocf/resource.d/pacemaker
%if %with_ais_support
%{_libexecdir}/lcrso/pacemaker.lcrso
%endif

%files -n pacemaker-libs
%defattr(-,root,root)
%{_libdir}/libcib.so.*
%{_libdir}/libcrmcommon.so.*
%{_libdir}/libcrmcluster.so.*
%{_libdir}/libpe_status.so.*
%{_libdir}/libpe_rules.so.*
%{_libdir}/libpengine.so.*
%{_libdir}/libtransitioner.so.*
%{_libdir}/libstonithd.so.*
%doc COPYING.LIB
%doc AUTHORS

%files -n pacemaker-libs-devel
%defattr(-,root,root)
%{_includedir}/pacemaker
%{_includedir}/heartbeat/fencing
%{_libdir}/*.so
%doc COPYING.LIB
%doc AUTHORS

%changelog
* Thu Oct 29 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-4
- Include the fixes from CoroSync integration testing
- Move the resource templates - they're not documentation
- Ensure documentation is placed in a standard location
- Exclude documentation that is included elsewhere in the package

- Update the tarball from upstream to version ee19d8e83c2a
  + High: cib: Correctly clean up when both plaintext and tls remote ports are requested
  + High: PE: Bug bnc#515172 - Provide better defaults for lt(e) and gt(e) comparisions
  + High: PE: Bug lf#2197 - Allow master instances placemaker to be influenced by colocation constraints
  + High: PE: Make sure promote/demote pseudo actions are created correctly
  + High: PE: Prevent target-role from promoting more than master-max instances
  + High: ais: Bug lf#2199 - Prevent expected-quorum-votes from being populated with garbage
  + High: ais: Prevent deadlock - dont try to release IPC message if the connection failed
  + High: cib: For validation errors, send back the full CIB so the client can display the errors
  + High: cib: Prevent use-after-free for remote plaintext connections
  + High: crmd: Bug lf#2201 - Prevent use-of-NULL when running heartbeat

* Wed Oct 13 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-3
- Update the tarball from upstream to version 38cd629e5c3c
  + High: Core: Bug lf#2169 - Allow dtd/schema validation to be disabled
  + High: PE: Bug lf#2106 - Not all anonymous clone children are restarted after configuration change
  + High: PE: Bug lf#2170 - stop-all-resources option had no effect
  + High: PE: Bug lf#2171 - Prevent groups from starting if they depend on a complex resource which can't
  + High: PE: Disable resource management if stonith-enabled=true and no stonith resources are defined
  + High: PE: Don't include master score if it would prevent allocation
  + High: ais: Avoid excessive load by checking for dead children every 1s (instead of 100ms)
  + High: ais: Bug rh#525589 - Prevent shutdown deadlocks when running on CoroSync
  + High: ais: Gracefully handle changes to the AIS nodeid
  + High: crmd: Bug bnc#527530 - Wait for the transition to complete before leaving S_TRANSITION_ENGINE
  + High: crmd: Prevent use-after-free with LOG_DEBUG_3
  + Medium: xml: Mask the "symmetrical" attribute on rsc_colocation constraints (bnc#540672)
  + Medium (bnc#520707): Tools: crm: new templates ocfs2 and clvm
  + Medium: Build: Invert the disable ais/heartbeat logic so that --without (ais|heartbeat) is available to rpmbuild
  + Medium: PE: Bug lf#2178 - Indicate unmanaged clones
  + Medium: PE: Bug lf#2180 - Include node information for all failed ops
  + Medium: PE: Bug lf#2189 - Incorrect error message when unpacking simple ordering constraint
  + Medium: PE: Correctly log resources that would like to start but can't
  + Medium: PE: Stop ptest from logging to syslog
  + Medium: ais: Include version details in plugin name
  + Medium: crmd: Requery the resource metadata after every start operation

* Fri Aug 21 2009 Tomas Mraz <tmraz@redhat.com> - 1.0.5-2.1
- rebuilt with new openssl

* Wed Aug 19 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-2
- Add versioned perl dependancy as specified by
    https://fedoraproject.org/wiki/Packaging/Perl#Packages_that_link_to_libperl
- No longer remove RPATH data, it prevents us finding libperl.so and no other
  libraries were being hardcoded
- Compile in support for heartbeat
- Conditionally add heartbeat-devel and corosynclib-devel to the -devel requirements 
  depending on which stacks are supported

* Mon Aug 17 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-1
- Add dependancy on resource-agents
- Use the version of the configure macro that supplies --prefix, --libdir, etc
- Update the tarball from upstream to version 462f1569a437 (Pacemaker 1.0.5 final)
  + High: Tools: crm_resource - Advertise --move instead of --migrate
  + Medium: Extra: New node connectivity RA that uses system ping and attrd_updater
  + Medium: crmd: Note that dc-deadtime can be used to mask the brokeness of some switches

* Tue Aug 11 2009 Ville Skyttä <ville.skytta@iki.fi> - 1.0.5-0.7.c9120a53a6ae.hg
- Use bzipped upstream tarball.

* Wed Jul  29 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-0.6.c9120a53a6ae.hg
- Add back missing build auto* dependancies
- Minor cleanups to the install directive

* Tue Jul  28 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-0.5.c9120a53a6ae.hg
- Add a leading zero to the revision when alphatag is used

* Tue Jul  28 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.5-0.4.c9120a53a6ae.hg
- Incorporate the feedback from the cluster-glue review
- Realistically, the version is a 1.0.5 pre-release
- Use the global directive instead of define for variables
- Use the haclient/hacluster group/user instead of daemon
- Use the _configure macro
- Fix install dependancies

* Fri Jul  24 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.4-3
- Include an AUTHORS and license file in each package
- Change the library package name to pacemaker-libs to be more 
  Fedora compliant
- Remove execute permissions from xml related files
- Reference the new cluster-glue devel package name
- Update the tarball from upstream to version c9120a53a6ae
  + High: PE: Only prevent migration if the clone dependancy is stopping/starting on the target node
  + High: PE: Bug 2160 - Dont shuffle clones due to colocation
  + High: PE: New implementation of the resource migration (not stop/start) logic
  + Medium: Tools: crm_resource - Prevent use-of-NULL by requiring a resource name for the -A and -a options
  + Medium: PE: Prevent use-of-NULL in find_first_action()
  + Low: Build: Include licensing files

* Tue Jul 14 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.4-2
- Reference authors from the project AUTHORS file instead of listing in description
- Change Source0 to reference the project's Mercurial repo
- Cleaned up the summaries and descriptions
- Incorporate the results of Fedora package self-review

* Tue Jul 14 2009 Andrew Beekhof <andrew@beekhof.net> - 1.0.4-1
- Initial checkin
