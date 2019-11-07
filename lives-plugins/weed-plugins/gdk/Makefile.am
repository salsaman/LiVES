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

if !NEED_LOCAL_WEED
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@
weed_CFLAGS = 
else
if IS_MINGW
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ -D IS_MINGW=1
else
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@
endif
weed_CFLAGS = -DNEED_LOCAL_WEED
endif

if NEED_LOCAL_WEED_PLUGIN
weed_CFLAGS = -DNEED_LOCAL_WEED_PLUGIN
endif

if NEED_LOCAL_WEED_COMPAT
weed_CFLAGS += -DNEED_LOCAL_WEED_COMPAT
endif

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) @GTK_CFLAGS@ -ffast-math
AM_CXXFLAGS = $(shared_cflags) $(weed_CFLAGS) @GTK_CFLAGS@ -ffast-math



fxpluginslib_LTLIBRARIES = gdk_fast_resize.la \
	compositor.la \
	videowall.la \
	scribbler.la



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
