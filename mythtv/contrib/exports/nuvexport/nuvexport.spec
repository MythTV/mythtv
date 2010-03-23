#
# RPM spec file for nuvexport
#

Name:       nuvexport
Version:    0.5
Release:    0.20090726.svn
License:    GPLv2
Summary:    Export MythTV .nuv and .mpeg files to other formats
URL:        http://forevermore.net/nuvexport/
Group:      Applications/Multimedia
Source:     %{name}-%{version}-%{release}.tar.bz2
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# This is a perl script -- no arch needed
BuildArch:  noarch

# Standard nuvexport modules:
Requires:  perl >= 5.6
Requires:  perl-DateManip
Requires:  perl-DBD-MySQL
Requires:  perl-DBI
Requires:  perl-MythTV >= 0.21
Requires:  id3lib
Requires:  transcode   >= 1.1
Requires:  ffmpeg      >= 0.5
Requires:  mjpegtools  >= 1.6.2
Requires:  mplayer

Requires:  /usr/bin/id3tag

# Provides some of its own perl modules -- rpm complains if this isn't included
Provides:  perl(nuvexport::shared_utils)

%description
nuvexport is a perl script wrapper to several encoders, which is capable of
letting users choose shows from their MythTV database and convert them to one
of several different formats, including SVCD/DVD mpeg and XviD avi.

%prep
%setup

%build

%install
[  %{buildroot} != "/" ] && rm -rf %{buildroot}
%{makeinstall}

# Remove incorrect attributes
chmod 644 %{buildroot}/%{_sysconfdir}/nuvexportrc
find %{buildroot} -name \*pm -exec chmod 644 {} \;

%clean
[  %{buildroot} != "/" ] && rm -rf %{buildroot}
rm -f $RPM_BUILD_DIR/file.list.%{name}

%files
%defattr(-, root, root, -)
%doc COPYING
%{_bindir}/nuv*
%{_datadir}/nuvexport/
%config(noreplace) %{_sysconfdir}/nuvexportrc
%doc COPYING

%changelog

* Mon Apr 13 2008 Chris Petersen <rpm@forevermore.net>
- Update spec to be closer to rpmfusion preferences

* Tue Dec 7  2004 Chris Petersen <rpm@forevermore.net>
- Built first spec
