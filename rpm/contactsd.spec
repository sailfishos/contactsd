Name: contactsd
Version: 1.4.2
Release: 1
Summary: Telepathy <> QtContacts bridge for contacts
URL: https://github.com/sailfishos/contactsd/
License: LGPLv2+ and (LGPLv2 or LGPLv2 with Nokia Qt LGPL Exception v1.1)
Source0: %{name}-%{version}.tar.bz2
Source1: %{name}.privileges
Requires: systemd
Requires: systemd-user-session-targets
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(Qt5Test)
BuildRequires: pkgconfig(TelepathyQt5)
BuildRequires: pkgconfig(Qt5Contacts)
BuildRequires: pkgconfig(Qt5Versit)
# mlite required only for tests
BuildRequires: pkgconfig(mlite5)
BuildRequires: pkgconfig(mlocale5)
BuildRequires: pkgconfig(libmkcal-qt5) >= 0.6.10
BuildRequires: pkgconfig(KF5CalendarCore)
BuildRequires: pkgconfig(telepathy-glib)
BuildRequires: pkgconfig(qofono-qt5)
BuildRequires: pkgconfig(qofonoext)
BuildRequires: pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.3.0
BuildRequires: pkgconfig(contactcache-qt5)
BuildRequires: pkgconfig(dbus-glib-1)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(accounts-qt5)
BuildRequires: pkgconfig(systemd)
BuildRequires: pkgconfig(buteosyncfw5) >= 0.6.33
BuildRequires: pkgconfig(qt5-boostable)
BuildRequires: qt5-qttools
BuildRequires: qt5-qttools-linguist
Requires: libqofono-qt5 >= 0.67
Requires: mapplauncherd-qt5

%description
%{name} is a service for collecting and observing changes in roster list
from all the users telepathy accounts (buddies, their status and presence
information), and store it to QtContacts.

%package devel
Summary: Development files for %{name}
Requires: %{name} = %{version}-%{release}

%description devel
%{summary}.

%package tests
Summary: Tests for %{name}
Requires: %{name} = %{version}-%{release}
Requires: blts-tools

%description tests
%{summary}.

%package ts-devel
Summary: Translation source for %{name}

%description ts-devel
Translation source for %{name}

%prep
%setup -q -n %{name}-%{version}

%build
export QT_SELECT=5
export VERSION=%{version}
./configure --bindir %{_bindir} --libdir %{_libdir} --includedir %{_includedir}
%qmake5
%make_build

%install
%qmake5_install

mkdir -p %{buildroot}%{_userunitdir}/post-user-session.target.wants
ln -s ../%{name}.service %{buildroot}%{_userunitdir}/post-user-session.target.wants/

mkdir -p %{buildroot}%{_datadir}/mapplauncherd/privileges.d
install -m 644 -p %{SOURCE1} %{buildroot}%{_datadir}/mapplauncherd/privileges.d

%post
if [ "$1" -ge 1 ]; then
systemctl-user daemon-reload || :
systemctl-user try-restart %{name}.service || :
fi

%postun
if [ "$1" -eq 0 ]; then
systemctl-user stop %{name}.service || :
systemctl-user daemon-reload || :
fi

%files
%license LICENSE.LGPL
%license LGPL_EXCEPTION.txt
%{_userunitdir}/%{name}.service
%{_userunitdir}/post-user-session.target.wants/%{name}.service
%{_bindir}/%{name}
%{_libdir}/%{name}-1.0
%{_datadir}/translations/*.qm
%{_datadir}/mapplauncherd/privileges.d/*
# we currently don't have a backup framework
%exclude /usr/share/backup-framework/applications/%{name}.conf
%{_libdir}/libcontactsd.so.*

%files devel
%{_includedir}/%{name}-1.0
%{_libdir}/pkgconfig/*.pc
%{_libdir}/libcontactsd.so

%files tests
/opt/tests/%{name}

%files ts-devel
%{_datadir}/translations/source/*.ts
