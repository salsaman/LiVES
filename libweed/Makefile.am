# see the Weed spec for more details

## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.7

ACLOCAL_AMFLAGS = -I m4

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in config.log config.cache config.status

WEED_SO_VERSION = 4:0:0

WEED_HSO_VERSION = 4:0:0

LIBPACKAGE = lib$(PACKAGE)

LIBS =

DOXYGEN = @DOXYGEN@

distdir = $(LIBPACKAGE)-$(VERSION)

sharedir = $(prefix)/share/
docdir = $(sharedir)doc/$(PACKAGE)/
includedir = $(prefix)/include/

libdir = $(prefix)/lib/

pkgconfigdir = $(libdir)$(target_cpu)-$(target_os)/pkgconfig/

weed_includedir = $(includedir)$(PACKAGE)/
weed_pluginutilsdir = $(weed_includedir)weed-plugin-utils/


weed_headers = weed.h weed-host.h weed-plugin.h weed-utils.h weed-events.h weed-palettes.h \
	weed-effects.h weed-compat.h weed-plugin-utils.h weed-host-utils.h

weed_pluginutils_files = weed-plugin-utils.c

doc_DATA = docs/weedspec.txt docs/weedevents.txt docs/weedaudio.txt docs/weed-plugin-utils.txt \
	docs/weed-utils.txt

EXTRA_DIST = configure COPYING.LGPL README libweed.doxygen $(doc_DATA) $(weed_pluginutils_files) \
	libweed.pc weed-effect-plugin.pc libweed-host-utils.pc libweed-utils.pc weed-extras.pc \
	$(weed_headers)

if IS_DARWIN
extra_ldflags =  --tag=disable-static
extra_cflags = -Wno-unused-function  -D_BUILD_LOCAL_
else
extra_ldflags = -fPIC -DPIC --tag=disable-static
extra_cflags = -fPIC -DPIC -O3 -Wno-unused-function -D_BUILD_LOCAL_
endif

lib_LTLIBRARIES = libweed.la libweed-utils.la libweed-host-utils.la

libweed_la_SOURCES = weed.c

libweed_utils_la_SOURCES = weed-utils.c 

libweed_host_utils_la_SOURCES = weed-host-utils.c

libweed_la_CFLAGS = -fvisibility=hidden -pthread $(extra_cflags)
libweed_la_LDFLAGS = -version-info $(WEED_SO_VERSION) -pthread

libweed_utils_la_CFLAGS = $(extra_cflags)
libweed_utils_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION)

libweed_host_utils_la_CFLAGS = $(extra_cflags)
libweed_host_utils_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_HSO_VERSION)


#### targets

if HAVE_DOXYGEN
docs-doxygen:
	$(DOXYGEN) libweed.doxygen
else
docs-doxygen:
	@echo "Doxygen not installed, skipping sourcecode documentation"
endif


installdirs:
	$(MKDIR_P) "$(DESTDIR)$(docdir)"
	$(MKDIR_P) "$(DESTDIR)$(weed_includedir)"
	$(MKDIR_P) "$(DESTDIR)$(weed_pluginutilsdir)"
	$(MKDIR_P) "$(DESTDIR)$(pkgconfigdir)"

install-data-local:
	make installdirs

	make docs-doxygen
if HAVE_DOXYGEN
	cp -rf docs/doxygen/html "$(DESTDIR)$(docdir)"
endif

	cp -f $(weed_headers) "$(DESTDIR)$(weed_includedir)"

	cp -f $(LIBPACKAGE)*.pc $(PACKAGE)*.pc "$(DESTDIR)$(pkgconfigdir)"

	cp -f $(weed_pluginutils_files) "$(DESTDIR)$(weed_pluginutilsdir)"

if HAVE_DOXYGEN
docs-doxygen:
	$(DOXYGEN) libweed.doxygen
else
docs-doxygen:
	@echo "Doxygen not installed, skipping sourcecode documentation"
endif

uninstall-local:
	rm -rf "$(docdir)"
	rm -rf "$(weed_includedir)"
	rm -f "$(pkgconfigdir)$(LIBPACKAGE)"*
	rm -f "$(pkgconfigdir)$(PACKAGE)"*
	rm -f "$(libdir)pkgconfig/libweed"*

install-exec-hook:
	rm -f "$(libdir)$(LIBPACKAGE)"*.la
	rm -f "$(libdir)$(LIBPACKAGE)"*.a
