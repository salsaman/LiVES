# Process this with automake to create Makefile.in

LIBS =

fxpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -no-undefined -module -avoid-version --tag=disable-static
shared_cflags = -fPIC -DPIC -Wno-unused-function
endif



if HAVE_SYSTEM_WEED
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ 
weed_CFLAGS = -DHAVE_SYSTEM_WEED
else
if IS_MINGW
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ -D IS_MINGW=1
else
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@
endif
weed_CFLAGS =
endif

if HAVE_SYSTEM_WEED_PLUGIN
weed_CFLAGS = -DHAVE_SYSTEM_WEED_PLUGIN_H
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

haar_analyser_la_SOURCES = haar_analyser.cpp haar_analyser.h


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
	cp -rf data/ "$(DESTDIR)$(fxpluginslibdir)" && chmod -R a+r "$(DESTDIR)$(fxpluginslibdir)/data/"
	cp -rf icons/ "$(DESTDIR)$(fxpluginslibdir)" && chmod -R a+r "$(DESTDIR)$(fxpluginslibdir)/icons/"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/data/".gitignore
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/icons/".gitignore


install-exec-local:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/"*
