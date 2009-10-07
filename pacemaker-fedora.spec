%define gname haclient
%define uname hacluster

#global _without_heartbeat 1

# When downloading directly from Mercurial, it will automatically add this prefix
# Invoking 'hg archive' wont but you can add one with:
#   hg archive -t tgz -p "Pacemaker-1-0-$upstreamversion" -r $upstreamversion $upstreamversion.tar.gz
%define upstreamprefix Pacemaker-1-0-
%define upstreamversion c9120a53a6ae
%global specversion 9

# Keep around for when/if required
#global alphatag %{upstreamversion}.hg

Name:		pacemaker
Summary:	Scalable High-Availability cluster resource manager
Version:	1.0.5
Release:	%{?alphatag:0.}%{specversion}%{?alphatag:.%{alphatag}}%{?dist}
License:	GPLv2+ and LGPLv2+
Url:		http://www.clusterlabs.org
Group:		System Environment/Daemons
Source0:	pacemaker.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
AutoReqProv:	on
Requires(pre):	cluster-glue
Requires:	pacemaker-libs = %{version}-%{release}

BuildRequires:	glib2-devel cluster-glue-libs-devel libxml2-devel libxslt-devel pkgconfig python-devel which
BuildRequires:	gcc-c++ gnutls-devel ncurses-devel pam-devel openssl-devel bzip2-devel
BuildRequires:  net-snmp-devel lm_sensors-devel libselinux-devel
%if 0%{?fedora}
BuildRequires:	libesmtp-devel help2man
%endif

%if 0%{?rhel} == 4 || 0%{?centos} == 4
# Nothing
%else
BuildRequires:  libtool-ltdl-devel
%endif
%if 0%{!?_without_ais}
BuildRequires:	libopenais-devel
Requires:	openais
%endif
%if 0%{!?_without_heartbeat}
BuildRequires:	heartbeat-devel
Requires:	heartbeat
%endif

%description
Pacemaker is an advanced, scalable High-Availability cluster resource
manager for Linux-HA (Heartbeat) and/or OpenAIS.

It supports "n-node" clusters with significant capabilities for
managing resources and dependencies.

It will run scripts at initialization, when machines go up or down,
when related resources fail and can be configured to periodically check
resource health.

Available rpmbuild rebuild options:
 --without : heartbeat ais

%package -n pacemaker-libs
License:	GPLv2+ and LGPLv2+
Summary:	Libraries used by the Pacemaker cluster resource manager and its clients
Group:		System Environment/Daemons

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
Requires:	%{name} = %{version}-%{release}
Requires:	pacemaker-libs = %{version}-%{release}
Requires:	libheartbeat-devel

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
%setup -q -n pacemaker

%build
./autogen.sh
./configure --localstatedir=%{_var}   \
        --mandir=%{_mandir}           \
	--with-ais-prefix=%{_prefix}  \
	%{?_without_heartbeat:--without-heartbeat} \
	%{?_without_ais:--without-ais} \
	--enable-fatal-warnings=no 
make %{_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
chmod a+x %{buildroot}/%{_libdir}/heartbeat/crm_primitive.py
chmod a+x %{buildroot}/%{_libdir}/heartbeat/hb2openais-helper.py
chmod a+x %{buildroot}/%{_datadir}/pacemaker/cts/CTSlab.py
chmod a+x %{buildroot}/%{_datadir}/pacemaker/cts/OCFIPraTest.py
chmod a+x %{buildroot}/%{_datadir}/pacemaker/cts/extracttests.py
rm %{buildroot}/%{_libdir}/service_crm.so

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

%clean
rm -rf %{buildroot}

%post -n pacemaker-libs -p /sbin/ldconfig

%postun -n pacemaker-libs -p /sbin/ldconfig

%files
###########################################################
%defattr(-,root,root)
%dir %{_datadir}/doc/packages/pacemaker
%dir %{_datadir}/doc/packages/pacemaker/templates

# Owned by cluster-glue
# %dir %{_libdir}/heartbeat
# %dir %{_var}/lib/heartbeat

%{_datadir}/pacemaker
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

%doc %{_datadir}/doc/packages/pacemaker/AUTHORS
%doc %{_datadir}/doc/packages/pacemaker/README
%doc %{_datadir}/doc/packages/pacemaker/README.hb2openais
%doc %{_datadir}/doc/packages/pacemaker/COPYING
%doc %{_datadir}/doc/packages/pacemaker/COPYING.LGPL
%doc %{_datadir}/doc/packages/pacemaker/crm_cli.txt
%doc %{_datadir}/doc/packages/pacemaker/templates/*
%doc %{_mandir}/man8/*.8*
%doc COPYING
%doc AUTHORS

%dir %attr (750, %{uname}, %{gname}) %{_var}/lib/heartbeat/crm
%dir %attr (750, %{uname}, %{gname}) %{_var}/lib/pengine
%dir %attr (750, %{uname}, %{gname}) %{_var}/run/crm
%dir /usr/lib/ocf
%dir /usr/lib/ocf/resource.d
/usr/lib/ocf/resource.d/pacemaker
%if 0%{!?_without_ais}
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

