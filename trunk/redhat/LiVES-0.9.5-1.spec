#
#
### RPM build definitions - partially taken over from kernel spec
#   I do not want to modify my RPM setup any more 
#   in order to stay compatible with user site installations
#   thus avoiding complains and bug hunting
#
#   I do not want compressed files at all, i.e. *.gz
%define __spec_install_post			/usr/lib/rpm/brp-compress || :
#   Break build process on missing doc files  - 1 = yes
%define _missing_doc_files_terminate_build	0
#   Break build process on missing prog files - 1 = yes
%define _unpackaged_files_terminate_build	0
#   We do not want debug packages
%define debug_package				%{nil}
#
## Fedora Core Special 
#  Certain developer freaks changed the RPM compiler options
#  this is not funny because i686 should not only run P4 optimized
#  Revert it here
%define rpm_opt_flags_rev %{__global_cflags} -march=%{_target_cpu} -mcpu=%{_target_cpu}
#  However, those who build local packages optimized for their
#  CPU set the following value to 1
#  NOTE: This only works with customized rpmc files
%define cpu_optimize				0


#####               PACKAGE DEFINITIONS START HERE
Summary:	Linux Video Editing System
Name:		LiVES
Version:	0.9.5
Release:	1
License:	GPL-v2
Group:		Applications/Multimedia
URL:		http://lives.sf.net/

Source0:	http://dl.sf.net/lives/%{name}-%{version}.tar.gz
# partial German language file - to be removed when entered in CVS
#Source1:	%{name}-%{version}-de_DE.mo
# scaled LiVES logos for menu entry - to be removed when entered in CVS
Source10:	%{name}_logo_16x.png
Source11:	%{name}_logo_22x.png
Source12:	%{name}_logo_32x.png
Source13:	%{name}_logo_48x.png
Source14:	%{name}_logo_64x.png
Source15:	%{name}_logo_128x.png
# Desktop menu entry - to be removed when entered in CVS
Source19:	%{name}.desktop
# German language files - to be removed when in CVS
#Patch0:		%{name}-%{version}-lang_de.patch
#Patch1:		%{name}-%{version}-config_de.patch
# Other patches
# Correct salsaman address in po/Makevars (missing l at the end)
Patch2:		%{name}-%{version}-po_makevars.patch

Buildroot:	%{_tmppath}/%{name}-%{version}-%{release}-root
Requires:	mplayer >= 0.96
Requires:	ImageMagick >= 5, perl >= 5, gtk+ >= 2
Requires:	libjpeg, gdk-pixbuf, sox
Requires:	libvisual >= 0.2.0, mjpegtools >= 1.6.2, python >= 2.3.4, SDL
#Requires:	libtheora, xmms, cdda2wav
Requires:	mplayer-mencoder >= 1.0, mjpegtools-libs, libtheora, xmms, cdda2wav
BuildRequires:	automake >= 1.7, autoconf >= 2.57, gettext >= 0.14.1
AutoReqProv:	no
Provides:	alien_overlay blurzoom fg_bg_removal fireTV libvis lifeTV mirrors negate noise onedTV posterise rippleTV rotozoom simple_blend textfun warpTV xeffect yuv4mpeg_stream

%description
LiVES is a Free, Open Source video editor *and* a VJ tool. 
It is fully extendible through open standard RFX plugin scripts.
LiVES is aimed at the digital video artist who wants to create their own content, 
the video editor who wants to create professional looking video, and the VJ who 
wants to captivate with spectacular images.

%description -l de
LiVES (Linux Video Editing System) ist ein Videobearbeitungssystem das es
ermöglicht Videos zu Schneiden, Nachzuvertonen, Konvertieren u.v.a.m..
LiVES besitzt verschiedene Erweiterungsschnittstellen wie z.B. RFX. Damit ist
es möglich das Basisprogramm zu erweitern - von Ihnen geschriebene RFX Module,
in ASCII Text, erlauben es auch Ihnen das Programm Ihren Bedürfnissen anzupassen.

%prep
%setup -n lives-%{version}
# Language patches
#%patch0 -p1
#%patch1 -p1
# Other patches
%patch2 -p1

%build
%configure
%{__make}

%install
%{__rm} -rf %{buildroot}
%makeinstall
rm -f ${RPM_BUILD_ROOT}/usr/bin/lives
ln -sf lives-exe ${RPM_BUILD_ROOT}/usr/bin/lives
install -m 0644 %{SOURCE10} %{SOURCE11} %{SOURCE12} %{SOURCE13} %{SOURCE14} %{SOURCE15} \
	%{buildroot}%{_datadir}/lives/icons/

mkdir -p $RPM_BUILD_ROOT%{_datadir}/applications
desktop-file-install --vendor LiVES \
  --dir $RPM_BUILD_ROOT%{_datadir}/applications        \
  --add-category X-Red-Hat-Base                        \
  --add-category System                                \
  --add-category Application                           \
  %{SOURCE19}

%clean
rm -rf %{buildroot}
if [ -d %{_builddir}/lives-%{version} ]; then
    rm -rf %{_builddir}/lives-%{version}
else
# Build is done here instead of regular location above.
    rm -rf %{_builddir}/lives
fi

%post
/sbin/ldconfig -n %{_libdir}/lives

%postun
/sbin/ldconfig -n %{_libdir}/lives

%files
%defattr(-, root, root, 0755)
# In order to let rpm -e delete empty directories use the following lines  
%{_bindir}
%{_docdir}
%{_datadir}/lives
%{_datadir}/locale
%{_datadir}/applications/LiVES.desktop

%changelog
* Tue Jan 03 2006 Herbert U. Hübner <www.friendglow.net> 0.9.5
- final 0.9.5 version
- uncomment DE language patch
  partial translation contained in the final version ;)

* Fri Nov 18 2005 Herbert U. Hübner <www.friendglow.net> 0.9.5-pre5.2
- add LiVES logo in various sizes for desktop menu

* Tue Nov 15 2005 Herbert U. Hübner <www.friendglow.net> 0.9.5-pre5.1
- add desktop menu entry

* Mon Nov 14 2005 Herbert U. Hübner <www.friendglow.net> 0.9.5-pre5
- updated rpms to version 0.9.5-pre5
- add partial German translation

* Thu Jul 28 2005 A. Pense <www.yousns.com> 0.9.5-pre4
- updated rpms to version 0.9.5-pre4

* Wed Jul 27 2005 A. Pense 0.9.5-pre3
- First creation of spec file, source rpm
