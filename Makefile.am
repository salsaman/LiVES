## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.7

if ENABLE_OSC
OSC_SUBDIRS = libOSC
endif

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in config.log config.cache config.status

SUBDIRS = $(OSC_SUBDIRS) intl libweed src m4 lives-plugins po
PACKAGE_CAPITALISED = LiVES

docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
sharedir = $(prefix)/share

doc_DATA = lives-plugins/marcos-encoders/README.multi_encoder AUTHORS BUGS ChangeLog FEATURES GETTING.STARTED README OMC/lives-OMC.txt RFX/LiVES-Perl.odt  RFX/rfxbuilder.odt RFX/RFX.spec weed-docs/weedspec.txt weed-docs/weedevents.txt docs/clip_format.txt

EXTRA_DIST = \
	configure \
	autogen.sh \
	build-lives-rfx-plugin* \
	smogrify \
	midi* \
	tools/autolives.pl \
	tools/batchproc.pl \
	tools/monitorlives.pl \
	tools/set_shuffle.pl \
	po/pxgettext \
	po/make_rfx_builtin_list.pl \
	po/update_with_plugins.sh \
	po/update_with_smogrify.sh \
	po/update_with_smogrify \
	default.keymap \
	COPYING \
	menu-entries/LiVES.desktop \
	menu-entries/lives.xpm \
	$(doc_DATA)

installdirs: 
	rm -rf $(datadir)/$(PACKAGE)/*
	rm -rf $(libdir)/$(PACKAGE)/*
	mkinstalldirs
	$(srcdir)/mkinstalldirs \
	mkdir -p $(DESTDIR)$(bindir) \
	mkdir -p $(DESTDIR)$(datadir) \
	mkdir -p $(DESTDIR)$(docdir) \
	mkdir -p $(DESTDIR)$(sharedir)/applications/ \
	mkdir -p $(DESTDIR)$(sharedir)/pixmaps/

install-exec-local:
	rm -rf $(DESTDIR)$(bindir)/lives
	cp -f $(srcdir)/smogrify $(DESTDIR)$(bindir) && chmod a+rx $(DESTDIR)$(bindir)/smogrify
	cp -f $(srcdir)/midistart $(DESTDIR)$(bindir) && chmod a+rx $(DESTDIR)$(bindir)/midistart
	cp -f $(srcdir)/midistop $(DESTDIR)$(bindir) && chmod a+rx $(DESTDIR)$(bindir)/midistop
	cp -f $(srcdir)/tools/autolives.pl $(DESTDIR)$(bindir) && chmod a+rx $(DESTDIR)$(bindir)/autolives.pl
	cp -f $(srcdir)/build-lives-rfx-plugin $(DESTDIR)$(bindir) && chmod a+rx $(DESTDIR)$(bindir)/build-lives-rfx-plugin
	cp -f $(srcdir)/build-lives-rfx-plugin-multi $(DESTDIR)$(bindir) && chmod a+rx $(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi
	cp -f $(srcdir)/default.keymap $(DESTDIR)$(datadir)/$(PACKAGE)/
	$(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi builtin $(DESTDIR)$(datadir) $(DESTDIR)$(libdir) $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(sharedir)/applications/
	cp -f $(srcdir)/menu-entries/LiVES.desktop $(DESTDIR)$(sharedir)/applications/
	mkdir -p $(DESTDIR)$(sharedir)/pixmaps/
	cp -f $(srcdir)/menu-entries/lives.xpm $(DESTDIR)$(sharedir)/pixmaps/

	$(top_srcdir)/po/update_with_plugins.sh install $(PACKAGE) $(prefix) $(DESTDIR)



install-exec-hook:
	$(LN_S) $(DESTDIR)$(bindir)/lives-exe $(DESTDIR)$(bindir)/lives



uninstall-local:
	rm -rf $(DESTDIR)$(bindir)/lives
	rm -rf $(DESTDIR)$(bindir)/lives-exe
	rm -rf $(DESTDIR)$(bindir)/smogrify
	rm -rf $(DESTDIR)$(bindir)/midistart
	rm -rf $(DESTDIR)$(bindir)/midistop
	rm -rf $(DESTDIR)$(bindir)/autolives.pl
	rm -rf $(DESTDIR)$(bindir)/build-lives-rfx-plugin
	rm -rf $(DESTDIR)$(bindir)/build-lives-rfx-plugin-multi
	rm -rf $(DESTDIR)$(datadir)/$(PACKAGE)
	rm -rf $(DESTDIR)$(libdir)/$(PACKAGE)
	rm -rf $(DESTDIR)$(docdir)
	rm -rf $(DESTDIR)$(sharedir)/applications/LiVES.desktop
	rm -rf $(DESTDIR)$(sharedir)/pixmaps/lives.xmp

	@echo
	@echo "		*** WARNING - user specific configuration files have NOT been removed."
	@echo "		*** To complete uninstallation, you may wish to remove ~/.lives and ~/.lives-dir" 

dist-hook:
	$(top_srcdir)/po/update_with_plugins.sh dist $(PACKAGE) $(prefix)

DISTCLEANFILES = intltool-extract intltool-merge intltool-update po/POTFILES_PLUGINS

ACLOCAL_AMFLAGS = -I m4


