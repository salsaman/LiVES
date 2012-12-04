## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.7

DOXYGEN = @DOXYGEN@

if ENABLE_OSC
OSC_SUBDIRS = libOSC
endif

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in config.log config.cache config.status

if !HAVE_SYSTEM_WEED
WEED_SUBDIRS=libweed
endif

if !HAVE_SYSTEM_WEED_UTILS
WEED_SUBDIRS=libweed
endif

if !HAVE_SYSTEM_WEED_COMPAT
WEED_SUBDIRS=libweed
endif

if !HAVE_SYSTEM_WEED_PLUGIN_UTILS
WEED_SUBDIRS=libweed
endif

SUBDIRS = $(OSC_SUBDIRS) intl $(WEED_SUBDIRS) src m4 lives-plugins po resources
PACKAGE_CAPITALISED = LiVES

docdir = "$(prefix)/share/doc/$(PACKAGE)-$(VERSION)"
sharedir = "$(prefix)/share"

if HAVE_DOXYGEN
docs-doxygen:
	$(DOXYGEN) LiVES.doxygen
else
docs-doxygen:
	@echo "Doxygen not installed, skipping sourcecode documentation"
endif

#NOTE - if packaging libweed separately, weed-docs should go in libweed/Makefile.am
doc_DATA = lives-plugins/marcos-encoders/README.multi_encoder AUTHORS BUGS ChangeLog FEATURES GETTING.STARTED README OMC/lives-OMC.txt RFX/LiVES-Perl.odt  RFX/rfxbuilder.odt RFX/RFX.spec weed-docs/weedspec.txt weed-docs/weedevents.txt docs/clip_format.txt

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
	INSTALL \
	menu-entries/LiVES.desktop \
	menu-entries/lives.png \
	menu-entries/lives.xpm \
	LiVES.doxygen \
	$(doc_DATA)


installdirs: 
	mkinstalldirs
	"$(srcdir)/mkinstalldirs" \
	mkdir -p "$(DESTDIR)$(bindir)" \
	mkdir -p "$(DESTDIR)$(datadir)" \
	mkdir -p "$(DESTDIR)$(docdir)" \
	mkdir -p "$(DESTDIR)$(sharedir)/applications/" \
	mkdir -p "$(DESTDIR)$(sharedir)/pixmaps/" \
	mkdir -p "$(DESTDIR)$(sharedir)/resources/"

install-data-local:
	make docs-doxygen
if HAVE_DOXYGEN
	mkdir -p "$(DESTDIR)$(docdir)/html/LiVES"
	cp -rf "$(srcdir)/docs/doxygen/html/"* "$(DESTDIR)$(docdir)/html/LiVES"
#NOTE - if packaging libweed separately, this should go in libweed/Makefile.am
	mkdir -p "$(DESTDIR)$(docdir)/html/libweed"


if HAVE_SYSTEM_WEED

else
	cp -rf "$(srcdir)/weed-docs/doxygen/html/"* "$(DESTDIR)$(docdir)/html/libweed"
endif

endif
	mkdir -p "$(DESTDIR)$(sharedir)/applications/"
	cp -f "$(srcdir)/menu-entries/LiVES.desktop" "$(DESTDIR)$(sharedir)/applications/"
	mkdir -p "$(DESTDIR)$(sharedir)/pixmaps/"
	cp -f "$(srcdir)/menu-entries/lives.xpm" "$(DESTDIR)$(sharedir)/pixmaps/"
	mkdir -p "$(DESTDIR)$(sharedir)/app-install/icons/"
	cp -f "$(srcdir)/menu-entries/lives.png" "$(DESTDIR)$(sharedir)/app-install/icons/"

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
	$(LN_S) "$(DESTDIR)$(bindir)/lives-exe" "$(DESTDIR)$(bindir)/lives"


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
	rm -rf "$(docdir)"
	rm -rf "$(sharedir)/applications/LiVES.desktop"
	rm -rf "$(sharedir)/pixmaps/lives.xmp"
	rm -rf "$(sharedir)/resources"

	@echo
	@echo "		*** WARNING - user specific configuration files have NOT been removed."
	@echo "		*** To complete uninstallation, you may wish to remove ~/.lives and ~/.lives-dir" 

dist-hook:
	"$(top_srcdir)/po/update_with_plugins.sh" dist "$(PACKAGE)" "$(prefix)"
	rm -rf "$(distdir)/po/"*.gmo
	rm -rf "$(distdir)/"*~

DISTCLEANFILES = intltool-extract intltool-merge intltool-update po/POTFILES_PLUGINS

ACLOCAL_AMFLAGS = -I m4


