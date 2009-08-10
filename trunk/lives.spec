%define name lives
%define _name LiVES
%define version 0.9.9.4
%define libweedver 1.2.0
%define release 1
%define libweedrel 1
%define _prefix /usr
%define _sharedir %{_prefix}/share

# platform defines
%define fedora 0
%if %{_vendor} == redhat
	%define fedora 1
	%define _dist %(cat /etc/redhat-release)
	%define suse_version 0000
%endif
%define suse 0
%if %{_vendor} == suse
	%define suse 1
	%define _dist %(grep -i SuSE /etc/SuSE-release)
%endif
%define mandriva 0
%if %{_vendor} == Mandriva
	%define mandriva 1
	%define _dist %(grep Mand /etc/mandrake-release)
	%define suse_version 0000
%endif
# test for a platform definition
%if ! %{fedora} && ! %{suse} && ! %{mandriva}
%{error: Unknown platform. Please examine the spec file.}
exit 1
%endif

Summary: 	LiVES is a Video Editing System
Name: 		%{name}
Version: 	%{version}
Release: 	%{release}
Source0: 	http://www.xs4all.nl/~salsaman/lives/current/%{name}-%{version}.tar.gz
URL: 		http://www.xs4all.nl/~salsaman/lives
License: 	GPL v3
Distribution:	%{_dist}
Group: 		Video
Packager:	D. Scott Barninger <barninger@fairfieldcomputers.com>
BuildRoot:      %{_tmppath}/%{name}-root

%if %{suse}
BuildRequires: 	gtk2-devel >= 2.8 freetype2-devel SDL-devel pango-devel aalib-devel
BuildRequires:	bison libvisual-devel >= 0.2.0 slang-devel atk-devel dvgrab jack-devel >= 0.100
BuildRequires:  ImageMagick mjpegtools libffmpeg-devel alsa-devel libraw1394-devel libavc1394-devel perl
Requires:	gtk2 >= 2.8 xmms MPlayer sox ImageMagick freetype2 SDL pango aalib alsa slang atk dvgrab jack >= 0.100
Requires:	cdrecord cdda2wav libvisual >= 0.2.0 mjpegtools libraw1394 libavc1394 perl
%endif

%if %{suse} && %{suse_version} == 1000
Requires:	libffmpeg
%endif

%if %{suse} && %{suse_version} == 1010
Requires:	libffmpeg
%endif

%if %{suse} && %{suse_version} == 1020
Requires:	libffmpeg
%endif

%if %{suse} && %{suse_version} == 1030
Requires:	libffmpeg
%endif

%if %{suse} && %{suse_version} == 1100
Requires:	libffmpeg0
%endif

%if %{fedora}
BuildRequires: 	gtk2-devel >= 2.8 freetype-devel >= 2.0 SDL-devel pango-devel aalib-devel dvgrab
BuildRequires:	bison libvisual-devel >= 0.2.0 slang-devel atk-devel jack-audio-connection-kit-devel >= 0.100
BuildRequires:  ImageMagick mjpegtools-devel ffmpeg-devel alsa-lib-devel libraw1394-devel libavc1394-devel perl
Requires:	gtk2 >= 2.8 xmms mplayer sox ImageMagick freetype >= 2.0 SDL pango aalib alsa-lib slang atk libraw1394 libavc1394
Requires:	cdrecord cdda2wav ffmpeg libvisual >= 0.2.0 mjpegtools jack-audio-connection-kit >= 0.100 dvgrab mencoder perl
%endif

%if %{mandriva}
BuildRequires: 	gtk2-devel >= 2.8 freetype2-devel SDL-devel pango-devel aalib-devel libjack0-devel >= 0.100
BuildRequires:	bison libvisual-devel >= 0.2.0 slang-devel atk-devel libraw1394-devel libavc1394-devel
BuildRequires:  ImageMagick libmjpegtools-devel ffmpeg-devel alsa-lib-devel dvgrab perl
Requires:	gtk2 >= 2.8 xmms mplayer sox ImageMagick freetype2 SDL pango aalib alsa-lib slang atk libjack0 >= 0.100
Requires:	cdrecord cdrecord-cdda2wav ffmpeg libvisual >= 0.2.0 mjpegtools libraw1394 libavc1394 dvgrab mencoder perl
%endif

Requires: libweed


%description
The Linux Video Editing System (LiVES) is intended to be a simple yet powerful
video effects and editing system.  It uses common tools for most of its work
(mplayer, ImageMagick, GTK+, sox).

%package -n libweed
Summary: 	Weed Video/Audio Processing Library
Version: 	%{libweedver}
Release: 	%{libweedrel}
Group: 		Video
License:	LGPL v3

%if %{suse}
Requires:	gtk2 >= 2.8 freetype2 SDL pango atk
Requires:	libvisual >= 0.2.0 perl
%endif

%if %{fedora}
Requires:	gtk2 >= 2.8 freetype >= 2.0 SDL pango atk
Requires:	libvisual >= 0.2.0 perl
%endif

%if %{mandriva}
Requires:	gtk2 >= 2.8 freetype2 SDL pango atk
Requires:	libvisual >= 0.2.0 perl
%endif

%description -n libweed
Weed is an object system developed for video/audio processing. Weed
currently has modules for video/audio effects (weed-effects), and for
timeline style events (weed-events).

%package -n libweed-devel
Summary: 	Development Environment for Weed Video/Audio Processing Library
Version: 	%{libweedver}
Release: 	%{libweedrel}
Group: 		Video
License:	LGPL v3

Requires:	libweed

%description -n libweed-devel
This package contains the development environment for the Weed
video/audio processing library.

# SuSE turns off stripping of binaries by default. In order to get
# stripped packages we must generate debug package. RedHat and Mandriva
# turn debug packages on by default but strip binaries regardless.
%if %{suse}
%debug_package
%endif

%prep
%setup -q

perl -p -i -e 's|"/usr/local/"|&get_home_dir||g' smogrify

%build

%if %{mandriva}
./configure --prefix=/usr
rm -f libtool
ln -s /usr/bin/libtool libtool
%else
%configure
%endif

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"
%makeinstall

rm -fr $RPM_BUILD_ROOT/%_docdir/LiVES-*
%find_lang lives
cp smogrify midistart midistop $RPM_BUILD_ROOT/%_bindir
cd $RPM_BUILD_ROOT/%_datadir/%name/themes
rm -fr `find -name '.xvpics'`
cd $RPM_BUILD_ROOT/%_bindir
rm -fr lives
ln -s lives-exe lives
rm -rf $RPM_BUILD_ROOT%{_sharedir}/doc/%{name}-*

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root)
%doc AUTHORS BUGS C* FEATURES GETTING* NEWS README*
%doc OMC/*.txt 
%{_bindir}/*
%{_sharedir}/%{name}
%{_sharedir}/locale
%{_sharedir}/doc
%{_sharedir}/applications/%{_name}.desktop
%{_sharedir}/pixmaps/%{name}.xpm

%files -n libweed
%{_libdir}

%files -n libweed-devel
%doc weed-docs/*.txt
%{_prefix}/include

%post -n libweed
ldconfig

%postun -n libweed
ldconfig 

%changelog
* Sat Dec 06 2008 D. Scott Barninger <barninger at fairfieldcomputers.com>
- 0.9.9.4 release
- break out libweed subpackages
* Sat Jul 12 2008 D. Scott Barninger <barninger at fairfieldcomputers.com>
- 0.9.9 release file relocations and libweed
- remove desktop file creation and icon as Makefile now works
* Sun Jun 22 2008 D. Scott Barninger <barninger at fairfieldcomputers.com>
- SuSE 11.0 release libffmpeg0 name change
* Sun Feb 03 2008 D. Scott Barninger <barninger at fairfieldcomputers.com>
- add debug package to strip suse
* Sun Dec 30 2007 D. Scott Barninger <barninger@fairfieldcomputers.com>
- add patch for ldvgrab problem, remove disable
- other patches for fedora 8 compile problems
* Tue Dec 18 2007 D. Scott Barninger <barninger@fairfieldcomputers.com>
- ver 0.9.8.6 fedora 7 compile problem with libraw1394
- adding --disable-ldvgrab
* Sun Mar 18 2007 D. Scott Barninger <barninger@fairfieldcomputers.com>
- add missing gtk2 requirement, set min version 2.8
- add perl and mencoder requirements
- add _vendor logic for platform selection
* Sat Jan 13 2007 D. Scott Barninger <barninger@fairfieldcomputers.com>
- add jack and iee1394 support 
* Sat Jul 29 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- add mandriva build
* Mon Jul 24 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.6 release 2 with append_clips.patch
* Sun Jul 23 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.6 final release
* Sun Apr 23 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.6-pre3 release
* Sat Jan 28 2006 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.5 final release
- remove _name as no longer packaged as LiVES
* Sat Mar 26 2005 D. Scott Barninger <barninger@fairfieldcomputers.com>
- 0.9.5
- add _prerelease tag
- remove unused icons
* Sun Jan 23 2005 D. Scott Barninger <barninger@fairfieldcomputers.com>
- fix desktop entry
- add fedora core build
* Sun Jan 16 2005 D. Scott Barninger <barninger@fairfieldcomputers.com>
- modify for SuSE
