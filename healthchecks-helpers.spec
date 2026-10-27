Name:           healthchecks-helpers
Version:        1.0
Release:        1%{?dist}
Summary:        Helper for systemd oneshot services to ping Healthchecks.io

License:        MIT
URL:            https://github.com/skyhisi/healthchecks-helpers
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(libsystemd)

%description
healthchecks-systemd-oneshot pings Healthchecks.io from systemd oneshot
services, optionally attaching recent journal entries of the unit.

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%{_bindir}/healthchecks-systemd-oneshot
%{_mandir}/man1/healthchecks-systemd-oneshot.1*
%license LICENSE

%changelog
* Tue Oct 27 2026 Silas Parker <silas@srp.me.uk> - 1.0-1
- Initial package
