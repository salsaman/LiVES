## Process this with automake to create Makefile.in

LIBS =

#NOTE - install-exec-local MUST go in the first subdir !!
SUBDIRS = gdk

fxpluginslibdir = $(libdir)/$(PACKAGE)/plugins/effects/realtime/weed

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static

shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -Wl,-Bsymbolic -module -avoid-version --tag=disable-static

shared_cflags = -fPIC -DPIC
endif

if HAVE_SYSTEM_WEED
AM_LDFLAGS = $(shared_ldflags) @WEED_UTILS_LIBS@ @WEED_PLUGIN_UTILS_LIBS@
weed_CFLAGS = -DHAVE_SYSTEM_WEED
else
AM_LDFLAGS = $(shared_ldflags) ../../libweed/libweed-utils.la ../../libweed/libweed-plugin-utils.la
endif

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) -ffast-math
AM_CPPFLAGS = $(shared_cflags) $(weed_CFLAGS) -ffast-math

if HAVE_VISUAL
libvis_libs = libvis.la
libvis_la_LDFLAGS = @VISUAL_LIBS@ $(AM_LDFLAGS)
libvis_la_CFLAGS =  @VISUAL_CFLAGS@ $(AM_CFLAGS)
libvispluginslibdir = $(libdir)/$(PACKAGE)/plugins/effects/realtime/weed/libvisual
libvispluginslib_LTLIBRARIES = $(libvis_libs)
endif

if HAVE_FREI0R
frei0r_libs = frei0r.la
frei0r_la_LDFLAGS = -ldl $(AM_LDFLAGS)
frei0r_la_CFLAGS = $(AM_CFLAGS)
frei0rpluginslibdir = $(libdir)/$(PACKAGE)/plugins/effects/realtime/weed/frei0r
frei0rpluginslib_LTLIBRARIES = $(frei0r_libs)
endif


fxpluginslib_LTLIBRARIES = RGBdelay.la \
	alien_overlay.la \
	audio_transition.la \
	audio_volume.la \
	blank_frame_detector.la \
	bump2d.la \
	blurzoom.la \
	ccorrect.la \
	colorkey.la \
	comic.la \
	deinterlace.la \
	edge.la \
	fg_bg_removal.la \
	fireTV.la \
	haip.la \
	layout_blends.la \
	lifeTV.la \
	livetext.la \
	mirrors.la \
	multi_blends.la \
	multi_transitions.la \
	negate.la \
	noise.la \
	onedTV.la \
	palette_test.la\
	plasma.la \
	posterise.la \
	rippleTV.la \
	rotozoom.la \
	simple_blend.la \
	slide_over.la \
	softlight.la \
	targeted_zoom.la \
	textfun.la \
	tvpic.la \
	vertigo.la \
	warpTV.la \
	xeffect.la

EXTRA_DIST=data icons

install-data-local:
	mkdir -p $(DESTDIR)$(fxpluginslibdir)
	cp -rf data/ $(DESTDIR)$(fxpluginslibdir)
	cp -rf icons/ $(DESTDIR)$(fxpluginslibdir)
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf $(DESTDIR)$(fxpluginslibdir)/data/.*
	rm -rf $(DESTDIR)$(fxpluginslibdir)/icons/.*

