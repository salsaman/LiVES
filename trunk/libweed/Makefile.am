# NOTE - warnings about underlinking can safely be ignored, since the host will (MUST)
# supply definitions of all missing (extern) symbols

# this is deliberate to allow host and plugins to use different versions of the same 
# functions, and to allow memory functions to be overloaded

# NOTE also, only the host may (MUST) dynamically link libweed
# plugins may statically link libweed-utils, etc.

# see the Weed spec for more details

DOXYGEN = @DOXYGEN@

install-data-local:
	make docs-doxygen
	mkdir -p $(DESTDIR)$(libdir)/pkgconfig/
	cp -f libweed*.pc $(DESTDIR)$(libdir)/pkgconfig/

if HAVE_DOXYGEN
docs-doxygen:
	$(DOXYGEN) libweed.doxygen
else
docs-doxygen:
	@echo "Doxygen not installed, skipping sourcecode documentation"
endif

WEED_SO_VERSION = 0:5:0

LIBS =

if IS_DARWIN
extra_ldflags = 

extra_cflags = 
else
extra_ldflags = -fPIC -DPIC

extra_cflags = -fPIC -DPIC
endif

EXTRA_DIST = libweed.doxygen libweed.pc libweed-plugin-utils.pc libweed_slice.pc libweed-utils.pc

lib_LTLIBRARIES = libweed.la libweed_slice.la libweed-utils.la libweed-plugin-utils.la

libweed_la_SOURCES = weed.c

libweed_slice_la_SOURCES = weed-gslice.c

libweed_utils_la_SOURCES = weed-utils.c 

libweed_plugin_utils_la_SOURCES = weed-plugin-utils.c

libweedinclude_HEADERS = weed.h weed-host.h weed-plugin.h weed-utils.h weed-events.h weed-palettes.h weed-effects.h \
	weed-plugin-utils.h

libweedincludedir = $(includedir)/weed

libweed_ladir = $(libdir)/weed

libweed_la_LDFLAGS = -version-info $(WEED_SO_VERSION)

libweed_slice_la_CFLAGS = @GLIB_CFLAGS@ 

libweed_slice_la_LDFLAGS = @GLIB_LIBS@ -version-info $(WEED_SO_VERSION)

libweed_utils_la_CFLAGS = $(extra_cflags)

libweed_utils_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION) -static

libweed_plugin_utils_la_CFLAGS = $(extra_cflags)

libweed_plugin_utils_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION) -static



install-exec-hook:
## The following is removed as it breaks packaging.
#	/sbin/ldconfig

