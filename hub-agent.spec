Name:    libvirt-hub-agent
Version: 1
Release: 2%{?dist}
Summary: Hub Agent

Group:   stuff
License: BSD
URL:     http://invalid.com
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc libvirt-devel libudev-devel gcc-c++
Requires: libvirt

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
%config /etc/libvirt-hub-agent/agent.ini
%config /etc/libvirt-hub-agent/domain.example.xml
%config /etc/libvirt-hub-agent/init.example.sh
/usr/lib/systemd/system/libvirt-hub-agent.service


%changelog
