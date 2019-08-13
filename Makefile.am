
## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.7

DOXYGEN = @DOXYGEN@

if ENABLE_OSC
OSC_SUBDIRS = libOSC
endif

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in config.log config.cache config.status

if !IS_MINGW

if !HAVE_SYSTEM_WEED
WEED_SUBDIRS=libweed
endif

if !HAVE_SYSTEM_WEED_UTILS
WEED_SUBDIRS=libweed
endif

if !HAVE_SYSTEM_WEED_COMPAT
WEED_SUBDIRS=libweed
endif

if !HAVE_SYSTEM_WEED_PLUGIN
WEED_SUBDIRS=libweed
endif

endif

SUBDIRS = $(OSC_SUBDIRS) intl $(WEED_SUBDIRS) src m4 lives-plugins po resources
PACKAGE_CAPITALISED = LiVES

docdir = "$(prefix)/share/doc/$(PACKAGE)-$(VERSION)"
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

#NOTE - if packaging libweed separately, weed-docs should go in libweed/Makefile.am
doc_DATA = lives-plugins/marcos-encoders/README.multi_encoder AUTHORS ChangeLog FEATURES GETTING.STARTED README OMC/lives-OMC.txt RFX/LiVES-Perl.odt  RFX/rfxbuilder.odt RFX/RFX.spec weed-docs/weedspec.txt weed-docs/weedevents.txt docs/clip_format.txt docs/c++-bindings.odt

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
	mkinstalldirs
	"$(srcdir)/mkinstalldirs" \
	$(MKDIR_P) "$(DESTDIR)$(bindir)" \
	$(MKDIR_P) "$(DESTDIR)$(datadir)" \
	$(MKDIR_P) "$(DESTDIR)$(docdir)" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/applications/" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/pixmaps/" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/$(PACKAGE)/resources/" \
	$(MKDIR_P) "$(DESTDIR)$(sharedir)/$(PACKAGE)/tools/"

install-data-local:
	make docs-doxygen
if HAVE_DOXYGEN
	$(MKDIR_P) "$(DESTDIR)$(docdir)/html/LiVES"
	cp -rf "$(srcdir)/docs/doxygen/html/"* "$(DESTDIR)$(docdir)/html/LiVES"
if BUILD_LIBLIVES
	$(MKDIR_P) "$(DESTDIR)$(docdir)/html/liblives"
	cp -rf "$(srcdir)/docs/doxygen/liblives/html/"* "$(DESTDIR)$(docdir)/html/liblives"
endif
#NOTE - if packaging libweed separately, this should go in libweed/Makefile.am
	$(MKDIR_P) "$(DESTDIR)$(docdir)/html/libweed"


if HAVE_SYSTEM_WEED

else
	cp -rf "$(srcdir)/weed-docs/doxygen/html/"* "$(DESTDIR)$(docdir)/html/libweed"
endif

endif
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
	cp -rf "$(srcdir)/resources/devicemaps/" "$(DESTDIR)$(sharedir)/$(PACKAGE)/"


install-exec-local:
	rm -rf "$(DESTDIR)$(bindir)/lives"
	cp -f "$(srcdir)/smogrify" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/smogrify"
	cp -f "$(srcdir)/midistart" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/midistart"
	cp -f "$(srcdir)/midistop" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/midistop"
	cp -f "$(srcdir)/tools/autolives.pl" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/autolives.pl"
	cp -f "$(srcdir)/build-lives-rfx-plugin" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/build-lives-rfx-plugin"
	cp -f "$(srcdir)/build-lives-rfx-plugin-multi" "$(DESTDIR)$(bindir)" && chmod a+rx "$(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi"
	cp -f "$(srcdir)/default.keymap" "$(DESTDIR)$(datadir)/$(PACKAGE)/"
	"$(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi" builtin "$(DESTDIR)$(datadir)" "$(DESTDIR)$(libdir)" "$(DESTDIR)$(bindir)"
	"$(top_srcdir)/po/update_with_plugins.sh" install "$(PACKAGE)" "$(prefix)" "$(DESTDIR)"

install-exec-hook:
	$(LN_S) "lives-exe" "$(DESTDIR)$(bindir)/lives"


uninstall-local:
	rm -rf "$(bindir)/lives"
	rm -rf "$(bindir)/lives-exe"
	rm -rf "$(bindir)/smogrify"
	rm -rf "$(bindir)/midistart"
	rm -rf "$(bindir)/midistop"
	rm -rf "$(bindir)/autolives.pl"
	rm -rf "$(bindir)/build-lives-rfx-plugin"
	rm -rf "$(bindir)/build-lives-rfx-plugin-multi"
	rm -rf "$(datadir)/$(PACKAGE)"
	rm -rf "$(libdir)/$(PACKAGE)"
	rm -rf "$(sharedir)/$(PACKAGE)"
	rm -rf "$(docdir)"
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

if TURBO
CFLAGS += -Ofast
endif

