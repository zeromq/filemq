#
#    filemq - A FileMQ server and client
#
#    Copyright (c) the Contributors as noted in the AUTHORS file.       
#    This file is part of FileMQ, a C implemenation of the protocol:    
#    https://github.com/danriegsecker/filemq2.                          
#                                                                       
#    This Source Code Form is subject to the terms of the Mozilla Public
#    License, v. 2.0. If a copy of the MPL was not distributed with this
#    file, You can obtain one at http://mozilla.org/MPL/2.0/.           
#

Name:           filemq
Version:        2.0.0
Release:        1
Summary:        a filemq server and client
License:        MIT
URL:            http://example.com/
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkg-config
BuildRequires:  systemd-devel
BuildRequires:  libsodium-devel
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
filemq a filemq server and client.

%package -n libfilemq2
Group:          System/Libraries
Summary:        a filemq server and client

%description -n libfilemq2
filemq a filemq server and client.
This package contains shared library.

%post -n libfilemq2 -p /sbin/ldconfig
%postun -n libfilemq2 -p /sbin/ldconfig

%files -n libfilemq2
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libfilemq.so.*

%package devel
Summary:        a filemq server and client
Group:          System/Libraries
Requires:       libfilemq2 = %{version}
Requires:       libsodium-devel
Requires:       zeromq-devel
Requires:       czmq-devel

%description devel
filemq a filemq server and client.
This package contains development files.

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libfilemq.so
%{_libdir}/pkgconfig/libfilemq.pc

%prep
%setup -q

%build
sh autogen.sh
%{configure} --with-systemd
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%doc README.md
%doc COPYING
%{_bindir}/filemq_server
%{_bindir}/filemq_client


%changelog
