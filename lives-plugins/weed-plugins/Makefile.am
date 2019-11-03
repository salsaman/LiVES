## Process this with automake to create Makefile.inaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

LIBS =

AUTOMAKE_OPTIONS = subdir-objects

#NOTE - install-exec-local MUST go in the first subdir ONLY !!
SUBDIRS = #gdk cairo # supercollider experimental

fxpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static

shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -module -avoid-version --tag=disable-static

shared_cflags = -fPIC -DPIC -Wno-unused-function
endif

if HAVE_SDL2
CFLAGS_SDL = @SDL2_CFLAGS@ -DHAVE_SDL2
LDFLAGS_SDL = @SDL2_LIBS@

else

if HAVE_X11
if HAVE_SDL1
CFLAGS_SDL = @SDL_CFLAGS@ @X11_CFLAGS@
LDFLAGS_SDL = @SDL_LIBS@ @X11_LIBS@

else

if FORCE_SDL
CFLAGS_SDL = -I $(SDL_PREFIX) @X11_CFLAGS@
LDFLAGS_SDL = -lSDL @X11_LIBS@
HAVE_SDL_ANYVERSION = true
endif
endif
endif
else
HAVE_SDL_ANYVERSION = false
endif

AM_LDFLAGS = $(shared_ldflags)

weed_CFLAGS =

if NEED_LOCAL_WEED
weed_CFLAGS += -DNEED_LOCAL_WEED
endif

if NEED_LOCAL_WEED_PLUGIN
weed_CFLAGS += -DNEED_LOCAL_WEED_PLUGIN
endif

if NEED_LOCAL_WEED_COMPAT
weed_CFLAGS += -DNEED_LOCAL_WEED_COMPAT
endif

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) -ffast-math -Wall

if HAVE_OPENGL
if HAVE_SDL_ANYVERSION
if HAVE_PROJECTM
projectM_libs = projectM.la
projectM_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_SDL) $(LIBPROJECTM_LIBS) -lpthread @X11_LIBS@ @GL_LIBS@
projectM_la_CPPFLAGS = $(LIBPROJECTM_CFLAGS) $(CFLAGS_SDL) \
	-DPROJECTM_PREFIX=\"`pkg-config --variable=libdir libprojectM`/libprojectM\" $(AM_CPPFLAGS) \
	 @X11_CFLAGS@ @GL_CFLAGS@
projectMdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
projectM_la_SOURCES = projectM.cpp projectM-getConfigFilename.cpp projectM-getConfigFilename.h projectM-ConfigFile.h
projectM_LTLIBRARIES = $(projectM_libs)
endif
endif
endif

fxpluginslib_LTLIBRARIES = negate.la simple_blend.la haip.la RGBdelay.la kaleidoscope.la

EXTRA_DIST=data icons syna.h weed-plugin-utils.c

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(fxpluginslibdir)"
	cp -rf data/ "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/data/"
	cp -rf icons/ "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/icons/"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/data/".gitignore
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/icons/".gitignore
