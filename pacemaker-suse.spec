#
# spec file for package pacemaker (Version 1.0.2)
#
# Copyright (c) 2009 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# norootforbuild
#global _without_ais		1
#global _without_heartbeat	1
%global pcmk_docdir %{_docdir}/%{name}

Name:           pacemaker
Summary:        The Pacemaker scalable High-Availability cluster resource manager
Version:        1.0.6
Release:	1%{?dist}
License:        GPL v2 or later; LGPL v2.1 or later
Url:            http://www.clusterlabs.org
Group:		Productivity/Clustering/HA
Source:         pacemaker.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
AutoReqProv:    on
Conflicts:      heartbeat < 2.99
Requires(pre):	cluster-glue
Requires:       libpacemaker3 = %{version}-%{release}
Requires:	resource-agents python

BuildRequires:  e2fsprogs-devel glib2-devel libglue-devel libxml2-devel libxslt-devel pkgconfig python-devel
BuildRequires:  gnutls-devel ncurses-devel net-snmp-devel pam-devel openssl-devel

# SLES10 doesn't pull in tcpd-devel with net-snmp-devel
BuildRequires:  libbz2-devel help2man tcpd-devel

%if !0%{?_without_ais}
BuildRequires:  libcorosync-devel
%endif

%if !0%{?_without_heartbeat}
BuildRequires:  heartbeat-devel
%endif

%define _libexecdir %{_libdir}

%if 0%{?suse_version} > 1999
BuildRequires:  libesmtp-devel
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

%package -n libpacemaker3
License:        GPL v2 or later; LGPL v2.1 or later
Summary:        The Pacemaker scalable High-Availability cluster resource manager
Group:		Productivity/Clustering/HA

%description -n libpacemaker3
Pacemaker is an advanced, scalable High-Availability cluster resource
manager for Linux-HA (Heartbeat) and/or OpenAIS.

It supports "n-node" clusters with significant capabilities for
managing resources and dependencies.

It will run scripts at initialization, when machines go up or down,
when related resources fail and can be configured to periodically check
resource health.

%package -n libpacemaker-devel 
License:        GPL v2 only; GPL v2 or later; LGPL v2.1 or later
Summary:        The Pacemaker scalable High-Availability cluster resource manager
Group:		Development/Libraries/C and C++
Requires:       %{name} = %{version}-%{release}
Requires:       libpacemaker3 = %{version}-%{release}
Requires:       libheartbeat-devel

%description -n libpacemaker-devel
Headers and shared libraries for developing tools for Pacemaker.

Pacemaker is an advanced, scalable High-Availability cluster resource
manager for Linux-HA (Heartbeat) and/or OpenAIS.

It supports "n-node" clusters with significant capabilities for
managing resources and dependencies.

It will run scripts at initialization, when machines go up or down,
when related resources fail and can be configured to periodically check
resource health.

%prep
###########################################################
%setup -n pacemaker -q
###########################################################

%build
CFLAGS="${CFLAGS} ${RPM_OPT_FLAGS}"
export CFLAGS

# Distribution specific settings:
%if 0%{?suse_version} < 1001
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/gnome/%{_lib}/pkgconfig:/opt/gnome/share/pkgconfig"
%endif
./autogen.sh
%{configure} --localstatedir=%{_var}			\
	--with-ais-prefix=%{_prefix}   			\
	%{?_without_heartbeat}      			\
	%{?_without_ais}		                \
	--docdir=%{pcmk_docdir}				\
	--enable-fatal-warnings=no 
export MAKE="make %{?jobs:-j%jobs}"
make %{?jobs:-j%jobs}
###########################################################

%install
###########################################################
make DESTDIR=$RPM_BUILD_ROOT install
chmod a+x $RPM_BUILD_ROOT/%{_libdir}/heartbeat/crm_primitive.py
chmod a+x $RPM_BUILD_ROOT/%{_libdir}/heartbeat/hb2openais-helper.py
rm $RPM_BUILD_ROOT/%{_libdir}/service_crm.so
# Dont package static libs or compiled python
find $RPM_BUILD_ROOT -name '*.a' -type f -print0 | xargs -0 rm -f
find $RPM_BUILD_ROOT -name '*.la' -type f -print0 | xargs -0 rm -f
find $RPM_BUILD_ROOT -name '*.pyc' -type f -print0 | xargs -0 rm -f
find $RPM_BUILD_ROOT -name '*.pyo' -type f -print0 | xargs -0 rm -f
###########################################################

%clean
###########################################################
if
  [ -n "${RPM_BUILD_ROOT}" -a "${RPM_BUILD_ROOT}" != "/" ]
then
  rm -rf $RPM_BUILD_ROOT
fi
rm -rf $RPM_BUILD_DIR/pacemaker
###########################################################

%post -n libpacemaker3 -p /sbin/ldconfig

%postun -n libpacemaker3 -p /sbin/ldconfig

%files
###########################################################
%defattr(-,root,root)
%dir %{_libdir}/heartbeat
%dir %{_var}/lib/heartbeat
%dir %{pcmk_docdir}
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

%if !0%{?_without_heartbeat}
%{_sbindir}/crm_uuid
%else
%exclude %{_sbindir}/crm_uuid
%endif

# Packaged elsewhere
%exclude %{pcmk_docdir}/AUTHORS
%exclude %{pcmk_docdir}/COPYING
%exclude %{pcmk_docdir}/COPYING.LIB

%doc %{pcmk_docdir}/README.hb2openais
%doc %{pcmk_docdir}/crm_cli.txt
%doc %{pcmk_docdir}/crm_fencing.txt
%doc %{pcmk_docdir}/*.html
%doc %{_mandir}/man8/*.8*

%dir %attr (750, hacluster, haclient) %{_var}/lib/heartbeat/crm
%dir %attr (750, hacluster, haclient) %{_var}/lib/pengine
%dir %attr (750, hacluster, haclient) %{_var}/run/crm
%dir /usr/lib/ocf
%dir /usr/lib/ocf/resource.d
/usr/lib/ocf/resource.d/pacemaker
%if !0%{?_without_ais}
%{_libexecdir}/lcrso/pacemaker.lcrso
%endif

%files -n libpacemaker3
%defattr(-,root,root)
%{_libdir}/libcib.so.*
%{_libdir}/libcrmcommon.so.*
%{_libdir}/libcrmcluster.so.*
%{_libdir}/libpe_status.so.*
%{_libdir}/libpe_rules.so.*
%{_libdir}/libpengine.so.*
%{_libdir}/libtransitioner.so.*
%{_libdir}/libstonithd.so.*

%files -n libpacemaker-devel
%defattr(-,root,root)
%{_includedir}/pacemaker
%{_includedir}/heartbeat/fencing
%{_libdir}/*.so

%changelog
