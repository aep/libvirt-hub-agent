Name:    hub-agent
Version: 1
Release: 1%{?dist}
Summary: Hub Agent

Group:   stuff
License: BSD
URL:     http://invalid.com
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc libvirt-devel libudev-devel
Requires: gcc libvirt

%description

%prep

%build
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
/usr/bin/libvirt-hub-agent
/etc/libvirt-hub-agent/example.ini
/etc/libvirt-hub-agent/domain.example.xml


%changelog
