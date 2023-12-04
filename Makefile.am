
## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.16.1

DOXYGEN = @DOXYGEN@

if ENABLE_OSC
OSC_SUBDIRS = libOSC
endif

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in config.log config.cache config.status

SUBDIRS = $(OSC_SUBDIRS) src m4 lives-plugins po resources
PACKAGE_CAPITALISED = LiVES

docdir = "$(prefix)/share/doc"
sharedir = "$(prefix)/share"

if HAVE_DOXYGEN
docs-doxygen:
	$(DOXYGEN) LiVES.doxygen
if BUILD_LIBLIVES
	$(DOXYGEN) liblives.doxygen
endif
else
docs-doxygen:
	@echo "Doxygen not installed, skipping sourcecode documentation"
endif

doc_DATA = lives-plugins/marcos-encoders/README.multi_encoder AUTHORS ChangeLog FEATURES GETTING.STARTED README OMC/lives-OMC.txt RFX/LiVES-Perl.odt  RFX/rfxbuilder.odt RFX/RFX.spec docs/clip_format.txt docs/c++-bindings.odt

EXTRA_DIST = \
	configure \
	build-lives-rfx-plugin* \
	smogrify \
	midi* \
	tools/autolives.pl \
	tools/batchproc.pl \
	tools/monitorlives.pl \
	tools/set_shuffle.pl \
	tools/lives* \
	po/pxgettext \
	po/make_rfx_builtin_list.pl \
	po/update_with_plugins.sh \
	po/update_with_smogrify.sh \
	po/update_with_smogrify \
	default.keymap \
	COPYING \
	COPYING.LGPL \
	INSTALL \
	menu-entries/LiVES.desktop \
	menu-entries/*.png \
	menu-entries/*.xpm \
	LiVES.doxygen \
	liblives.doxygen \
	src/liblives.cpp \
	src/liblives.hpp \
	src/lbindings.c \
	src/lbindings.h \
	$(doc_DATA)

installdirs: 
	$(MKDIR_P) "$(DESTDIR)$(bindir)"
	$(MKDIR_P) "$(DESTDIR)$(datadir)"
	$(MKDIR_P) "$(DESTDIR)$(docdir)/$(PACKAGE)"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/applications/" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/pixmaps/" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/$(PACKAGE)/resources/" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/$(PACKAGE)/tools/"

install-data-local:
	make docs-doxygen
if HAVE_DOXYGEN
	$(MKDIR_P) "$(DESTDIR)$(docdir)/$(PACKAGE)/html"
	cp -rf "$(srcdir)/docs/doxygen/html/" "$(DESTDIR)$(docdir)/$(PACKAGE)/html"
if BUILD_LIBLIVES
	cp -rf "$(srcdir)/docs/doxygen/liblives/html/"* "$(DESTDIR)$(docdir)/$(PACKAGE)/html/liblives"
endif

endif
	$(MKDIR_P) "$(DESTDIR)$(docdir)"
	@echo $(VERSION) > "$(DESTDIR)$(docdir)/$(PACKAGE)/version"

	$(MKDIR_P) "$(DESTDIR)$(sharedir)/applications/"
	cp -f "$(srcdir)/menu-entries/LiVES.desktop" "$(DESTDIR)$(sharedir)/applications/"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/pixmaps/"
	cp -f "$(srcdir)/menu-entries/lives32.xpm" "$(DESTDIR)$(sharedir)/pixmaps/lives.xpm"
	cp -f "$(srcdir)/menu-entries/lives48.png" "$(DESTDIR)$(sharedir)/pixmaps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/app-install/icons/"
	cp -f "$(srcdir)/menu-entries/lives64.png" "$(DESTDIR)$(sharedir)/app-install/icons/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/48x48/apps/"
	cp -f "$(srcdir)/menu-entries/lives48.png" "$(DESTDIR)$(sharedir)/icons/hicolor/48x48/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/64x64/apps/"
	cp -f "$(srcdir)/menu-entries/lives64.png" "$(DESTDIR)$(sharedir)/icons/hicolor/64x64/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/128x128/apps/"
	cp -f "$(srcdir)/menu-entries/lives128.png" "$(DESTDIR)$(sharedir)/icons/hicolor/128x128/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/256x256/apps/"
	cp -f "$(srcdir)/menu-entries/lives256.png" "$(DESTDIR)$(sharedir)/icons/hicolor/256x256/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/32x32/apps/"
	cp -f "$(srcdir)/menu-entries/lives32.png" "$(DESTDIR)$(sharedir)/icons/hicolor/32x32/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/16x16/apps/"
	cp -f "$(srcdir)/menu-entries/lives16.png" "$(DESTDIR)$(sharedir)/icons/hicolor/16x16/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/icons/hicolor/22x22/apps/"
	cp -f "$(srcdir)/menu-entries/lives22.png" "$(DESTDIR)$(sharedir)/icons/hicolor/22x22/apps/lives.png"
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/$(PACKAGE)/devicemaps/"
	cp -rf "$(srcdir)/resources/devicemaps" "$(DESTDIR)$(sharedir)/$(PACKAGE)"
	cp -rf "$(srcdir)/resources/stock-icons" "$(DESTDIR)$(sharedir)/$(PACKAGE)"
	cp -f "$(srcdir)/resources/fxdefs.perkey" "$(DESTDIR)$(sharedir)/$(PACKAGE)"
	cp -f "$(srcdir)/resources/datacons.map" "$(DESTDIR)$(sharedir)/$(PACKAGE)"

install-exec-local:
	rm -f "$(DESTDIR)$(bindir)/lives"
	cp -f "$(srcdir)/smogrify" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/smogrify"
	cp -f "$(srcdir)/midistart" "$(DESTDIR)$(bindir)/lives-midistart" && chmod a+rx "$(DESTDIR)$(bindir)/lives-midistart"
	cp -f "$(srcdir)/midistop" "$(DESTDIR)$(bindir)/lives-midistop" && chmod a+rx "$(DESTDIR)$(bindir)/lives-midistop"
	cp -f "$(srcdir)/tools/autolives.pl" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/autolives.pl"
	cp -f "$(srcdir)/build-lives-rfx-plugin" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/build-lives-rfx-plugin"
	cp -f "$(srcdir)/build-lives-rfx-plugin-multi" "$(DESTDIR)$(bindir)" \
		&& chmod a+rx "$(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi"
	cp -f "$(srcdir)/default.keymap" "$(DESTDIR)$(datadir)/$(PACKAGE)/"
	"$(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi" builtin "$(DESTDIR)$(datadir)" "$(DESTDIR)$(libdir)" \
		"$(DESTDIR)$(bindir)"
	"$(top_srcdir)/po/update_with_plugins.sh" install "$(PACKAGE)" "$(prefix)" "$(DESTDIR)"

install-exec-hook:
	$(LN_S) "lives-exe" "$(DESTDIR)$(bindir)/lives"

uninstall-local:
	rm -f "$(bindir)/lives"
	rm -f "$(bindir)/lives-exe"
	rm -f "$(bindir)/smogrify"
	rm -f "$(bindir)/lives-midistart"
	rm -f "$(bindir)/lives-midistop"
	rm -f "$(bindir)/autolives.pl"
	rm -f "$(bindir)/build-lives-rfx-plugin"
	rm -f "$(bindir)/build-lives-rfx-plugin-multi"
	rm -rf "$(datadir)/$(PACKAGE)"
	rm -rf "$(libdir)/$(PACKAGE)"
	rm -rf "$(sharedir)/$(PACKAGE)"
	rm -rf "$(docdir)/$(PACKAGE)"
	rm -rf "$(sharedir)/applications/LiVES.desktop"
	rm -rf "$(sharedir)/pixmaps/lives.xmp"

	@echo
	@echo "		*** WARNING - user specific configuration files have NOT been removed."
	@echo "		*** To complete uninstallation, you may wish to remove ~/.lives and ~/.lives-dir" 


dist-hook:
	"$(top_srcdir)/po/update_with_plugins.sh" dist "$(PACKAGE)" "$(prefix)"
	rm -rf "$(distdir)/po/"*.gmo
	rm -rf "$(distdir)/"*~

DISTCLEANFILES = intltool-extract intltool-merge intltool-update po/POTFILES_PLUGINS

ACLOCAL_AMFLAGS = -I m4

