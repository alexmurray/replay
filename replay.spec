Name:           replay
Version:        0.8.1
Release:        1%{?dist}
Summary:        Visualiser for interconnected systems

Group:          Development/Tools
License:        GPLv2+
URL:            https://pxhermes.dsto.defence.gov.au/trac/siatm
Source:         %{name}-%{version}.tar.gz

BuildRequires:  intltool >= 0.40.0
BuildRequires:  gettext
BuildRequires:  automake >= 1.10
BuildRequires:  autoconf
BuildRequires:  libtool >= 2.2.6
BuildRequires:  yelp-tools >= 3.4.1
BuildRequires:  pkgconfig(gtk-doc) >= 1.0
BuildRequires:  pkgconfig(glib-2.0) >= 2.26.0
BuildRequires:  pkgconfig(gtk+-3.0) >= 3.4.0
BuildRequires:  pkgconfig(gtkglext-3.0) >= 2.99.0
BuildRequires:  pkgconfig(libpeas-1.0) >= 0.7.2
BuildRequires:  pkgconfig(libpeas-gtk-1.0) >= 0.7.2
BuildRequires:  pkgconfig(gobject-introspection-1.0) >= 1.32
BuildRequires:  pkgconfig(libgraph)
BuildRequires:  pkgconfig(python2) >= 2.5.2
#Requires:

%description
Replay provides a generic visualiser for interconnected systems, displaying
the connectivity and behaviour (interactions) of nodes within the system.

%package        devel
Summary:        Visualiser for interconnected systems (development files)
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description    devel
This package is needed for the development of plugins for Replay

%prep
%setup -q

%build
%configure --enable-gtk-doc
make %{?_smp_mflags}

%install
make install DESTDIR=$RPM_BUILD_ROOT

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%files
%doc
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/glib-2.0/schemas/au.gov.defence.dsto.parallax.%{name}.gschema.xml
%{_datadir}/help/C/%{name}
%{_datadir}/icons/
%{_datadir}/locale/
%{_libdir}/%{name}/plugins/
%{_libdir}/girepository-1.0/Replay-1.0.typelib

%files devel
%doc
%{_datadir}/gtk-doc/
%{_datadir}/gir-1.0/Replay-1.0.gir
%{_includedir}/%{name}
%{_libdir}/pkgconfig


%changelog
* Wed Jan 23 2013 Alex Murray <murray.alex@gmail.com> 0.8.1-1
- RPM of 0.8.1 release
* Thu Jan 10 2013 Alex Murray <murray.alex@gmail.com> 0.8.0-1
- RPM of 0.8.0 release
* Mon Dec 17 2012 Alex Murray <murray.alex@gmail.com> 0.7.0-1
- RPM of 0.7.0 release
* Mon Oct 15 2012 Alex Murray <murray.alex@gmail.com> 0.6.0-1
- RPM of 0.6.0 release
* Wed Sep 29 2010 Alex Murray <murray.alex@gmail.com> 0.3.0-1
- RPM of 0.3.0 release
* Mon May 10 2010 Alex Murray <murray.alex@gmail.com> 0.1.3-1
- Initial RPM release
