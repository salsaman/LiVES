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
	$(MKDIR_P) "$(DESTDIR)$(libdir)/pkgconfig/"
	cp -f libweed*.pc "$(DESTDIR)$(libdir)/pkgconfig/"

if HAVE_DOXYGEN
docs-doxygen:
	$(DOXYGEN) libweed.doxygen
else
docs-doxygen:
	@echo "Doxygen not installed, skipping sourcecode documentation"
endif

WEED_SO_VERSION = 0:6:0
WEED_PSO_VERSION = 0:12:0

LIBS =

if IS_DARWIN
extra_ldflags = 

extra_cflags = 
else
extra_ldflags = -fPIC -DPIC

extra_cflags = -fPIC -DPIC
endif

lib_LTLIBRARIES = libweed.la libweed_slice.la libweed-utils.la libweed_static.la libweed_slice_static.la libweed-utils_static.la

EXTRA_DIST = libweed.doxygen libweed.pc libweed-plugin.pc libweed_slice.pc libweed-utils.pc libweed-compat.pc

libweed_la_SOURCES = weed.c

libweed_slice_la_SOURCES = weed-gslice.c

libweed_utils_la_SOURCES = weed-utils.c 

libweed_static_la_SOURCES = weed.c

libweed_slice_static_la_SOURCES = weed-gslice.c

libweed_utils_static_la_SOURCES = weed-utils.c 


libweedinclude_HEADERS = weed.h weed-host.h weed-plugin.h weed-utils.h weed-events.h weed-palettes.h weed-effects.h \
	weed-compat.h

libweedincludedir = "$(includedir)/weed"

libweed_ladir = "$(libdir)/weed"
libweed_static_ladir = "$(libdir)/weed"


libweed_la_CFLAGS = -fvisibility=hidden
libweed_la_static_CFLAGS = -FPIC -DPIC

shared_LDFLAGS = -shared --tag=disable-static

static_LDFLAGS = -static -FPIC -DPIC

libweed_la_LDFLAGS = -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)
libweed_static_la_LDFLAGS = -version-info $(WEED_SO_VERSION) $(static_LDFLAGS)

libweed_slice_la_CFLAGS = @GLIB_CFLAGS@ $(libweed_la_CFLAGS)
libweed_slice_static_la_CFLAGS = @GLIB_CFLAGS@ -FPIC -DPIC

libweed_slice_la_LDFLAGS = @GLIB_LIBS@ -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)
libweed_slice_static_la_LDFLAGS = @GLIB_LIBS@ -version-info $(WEED_SO_VERSION) $(static_LDFLAGS)

libweed_utils_la_CFLAGS = $(extra_cflags)
libweed_utils_static_la_CFLAGS = -FPIC -DPIC

libweed_utils_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)
libweed_utils_static_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION) $(static_LDFLAGS)


uninstall-local:
	rm -rf "$(libweedincludedir)"
	rm -rf "$(libweed_ladir)"
	rm -rf "$(libdir)/libweed"*
	rm -rf "$(libdir)/pkgconfig/libweed"*


install-exec-hook:
	rm -f /usr/lib/libweed*.la
## The following is removed as it breaks packaging.
#	/sbin/ldconfig


