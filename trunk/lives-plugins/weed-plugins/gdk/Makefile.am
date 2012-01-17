# Process this with automake to create Makefile.in

LIBS =

fxpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -Wl,-Bsymbolic -module -avoid-version --tag=disable-static
shared_cflags = -fPIC -DPIC
endif

if HAVE_SYSTEM_WEED
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ @WEED_UTILS_LIBS@
weed_CFLAGS = -DHAVE_SYSTEM_WEED
else
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ ../../../libweed/libweed-utils.la
weed_CFLAGS = 
endif

if HAVE_SYSTEM_WEED_PLUGIN_UTILS
AM_LDFLAGS += @WEED_PLUGIN_UTILS_LIBS@
weed_CFLAGS += -DHAVE_SYSTEM_WEED_PLUGIN_UTILS
else
AM_LDFLAGS += ../../../libweed/libweed-plugin-utils.la
endif

if HAVE_SYSTEM_WEED_COMPAT
weed_CFLAGS += -DHAVE_SYSTEM_WEED_COMPAT
endif

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) @GTK_CFLAGS@ -ffast-math
AM_CXXFLAGS = $(shared_cflags) $(weed_CFLAGS) @GTK_CFLAGS@ -ffast-math

if ENABLE_VJACK
vjack_libs = vjack_rcv.la
vjack_rcv_la_LDFLAGS = -ljack $(AM_LDFLAGS)
endif

haar_analyser_la_SOURCES = haar_analyser.cpp

fxpluginslib_LTLIBRARIES = gdk_fast_resize.la \
	compositor.la \
	videowall.la \
	scribbler.la \
	puretext.la \
	mask_overlay.la \
	haar_analyser.la \
	$(vjack_libs) 


EXTRA_DIST=data icons

install-data-local:
	mkdir -p "$(DESTDIR)$(fxpluginslibdir)"
	cp -rf data/ "$(DESTDIR)$(fxpluginslibdir)"
	cp -rf icons/ "$(DESTDIR)$(fxpluginslibdir)"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/data/".*
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/icons/".*


install-exec-local:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/"*
