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

if IS_MINGW
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ -D IS_MINGW=1
else
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@
endif

AM_CFLAGS = $(shared_cflags) @GTK_CFLAGS@  @TURBO_CFLAGS@ -ffast-math -fno-math-errno
AM_CXXFLAGS = AM_CFLAGS

fxpluginslib_LTLIBRARIES = vector_visualiser.la


EXTRA_DIST=data icons

install-data-local:
	mkdir -p "$(DESTDIR)$(fxpluginslibdir)"
	cp -rf data "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/data"
	cp -rf icons "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/icons"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/data/".gitignore
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/icons/".gitignore


install-exec-local:



