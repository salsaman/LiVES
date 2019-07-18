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

WEED_SO_VERSION = 0:11:0
WEED_PSO_VERSION = 0:14:0

LIBS =

if IS_DARWIN
extra_ldflags = 

extra_cflags = 
else
extra_ldflags = -fPIC -DPIC

extra_cflags = -fPIC -DPIC -O3
endif

lib_LTLIBRARIES = libweed.la libweed_slice.la libweed-utils.la libweed_slice_scripting.la libweed-utils_scripting.la

EXTRA_DIST = libweed.doxygen libweed.pc libweed-plugin.pc libweed_slice.pc libweed-utils.pc libweed-compat.pc

libweed_la_SOURCES = weed.c

libweed_slice_la_SOURCES = weed-gslice.c

libweed_utils_la_SOURCES = weed-utils.c 


libweed_slice_scripting_la_SOURCES = weed-gslice_scripting.c

libweed_utils_scripting_la_SOURCES = weed-utils_scripting.c 


libweedinclude_HEADERS = weed.h weed-host.h weed-plugin.h weed-utils.h weed-events.h weed-palettes.h weed-effects.h \
	weed-compat.h

libweedincludedir = "$(includedir)/weed"

libweed_ladir = "$(libdir)/weed"
libweed_scripting_ladir = "$(libdir)/weed"


libweed_la_CFLAGS = -fvisibility=hidden

shared_LDFLAGS = -shared --tag=disable-static

libweed_la_LDFLAGS = -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)

libweed_slice_la_CFLAGS = @GLIB_CFLAGS@ $(libweed_la_CFLAGS)

libweed_slice_la_LDFLAGS = @GLIB_LIBS@ -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)

libweed_utils_la_CFLAGS = $(extra_cflags)

libweed_utils_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)


libweed_slice_scripting_la_CFLAGS = @GLIB_CFLAGS@ $(libweed_la_CFLAGS)

libweed_slice_scripting_la_LDFLAGS = @GLIB_LIBS@ -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS)

libweed_utils_scripting_la_CFLAGS = $(extra_cflags) @GLIB_CFLAGS@

libweed_utils_scripting_la_LDFLAGS = $(extra_ldflags) -version-info $(WEED_SO_VERSION) $(shared_LDFLAGS) @GLIB_LIBS@


uninstall-local:
	rm -rf "$(libweedincludedir)"
	rm -rf "$(libweed_ladir)"
	rm -rf "$(libdir)/libweed"*
	rm -rf "$(libdir)/pkgconfig/libweed"*


install-exec-hook:
	rm -f /usr/lib/libweed*.la
## The following is removed as it breaks packaging.
#	/sbin/ldconfig


