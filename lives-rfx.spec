%define version 0.9.6
# if a prerelease package set to 0.pre1 or 1 or higher for a release
# to preserve upgradeability
%define release 2
# if a prerelease package define so:
# %define _prerelease -pre2
# else define %nil
%define _prerelease %nil
%define _prefix /usr
%define _sharedir %{_prefix}/share
%define _scriptdir %{_sharedir}/lives/plugins/effects/RFXscripts

# platform defines - set one below or define the build_xxx on the command line
%define fedora 0
%{?build_fedora:%define fedora 1}
%define suse 0
%{?build_suse:%define suse 1}
%define mandriva 0
%{?build_mandriva:%define mandriva 1}
# test for a platform definition
%if ! %{fedora} && ! %{suse} && ! %{mandriva}
%{error: You must specify a platform. Please examine the spec file.}
exit 1
%endif

%if %{fedora}
%define _dist %(grep Fedora /etc/redhat-release)
%endif
%if %{suse}
%define _dist %(grep -i SuSE /etc/SuSE-release)
%endif
%if %{mandriva}
%define _dist %(grep Mand /etc/mandrake-release)
%endif

Summary:	LiVES Rendered/Realtime Effects
Name:		lives-rfx
Version:	%{version}
Release:	%{release}
License:	GPL
Group:		Applications/Multimedia
URL:		http://lives.sourceforge.net/index.php?do=addons
Source0:	%{name}-%{version}%{_prerelease}.tar.gz
Distribution:	%{_dist}
Packager:	D. Scott Barninger <barninger@fairfieldcomputers.com>
BuildRoot:      %{_tmppath}/%{name}-root
BuildArchitectures: noarch

Requires: lives >= %{version}, python >= 2.3.0, ImageMagick, metapixel

%if %{fedora}
Requires: kdegraphics
%endif
%if %{suse} || %{mandriva}
Requires: kdegraphics3
%endif

%description
RFX stands for rendered/realtime effects. It is the open standard being
developed by the author (Salsaman - G. Finch) for passing parameter window
requests between applications, in this case, LiVES GUI and its plugins.
The schema separates parameter type from layout. It can also contain 
sections for processing of those parameters in multiple languages.
Finally, an RFX script is compiled into an application specfic plugin,
depending on the application and target language.

%prep
%setup -n %{name}-%{version}%{_prerelease}

%build

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"
install -d $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 desub.py $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 desub.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 kruler.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 photomosaic.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 pixilate_level_2.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 ppmfilter.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 SpinWithDirection.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 textover_level_2.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 transition_slide.script $RPM_BUILD_ROOT%{_scriptdir}
install -m 0755 ZoomOnSpot.script $RPM_BUILD_ROOT%{_scriptdir}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root)
%{_scriptdir}/desub.py
%{_scriptdir}/desub.script
%{_scriptdir}/kruler.script
%{_scriptdir}/photomosaic.script
%{_scriptdir}/ppmfilter.script
%{_scriptdir}/pixilate_level_2.script
%{_scriptdir}/SpinWithDirection.script
%{_scriptdir}/textover_level_2.script
%{_scriptdir}/transition_slide.script
%{_scriptdir}/ZoomOnSpot.script


%changelog
* Sat Jul 29 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- add mandriva build
* Sun Apr 23 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.6-pre3 release
* Sat Jan 28 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.5 final release
* Sun Jul 31 2005 D. Scott Barninger <barninger@fairfieldcomputers.com>
- modify for combined fedora/suse build
* Thu Jul 28 2005 A. Pense <www.yousns.com> 0.9.5
- First creation of spec file, source rpm
- Includes all relevant RFX plugins on page except for the ppmfilter, due to technical difficulties with smilutils
