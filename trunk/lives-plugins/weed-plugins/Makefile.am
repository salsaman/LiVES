## Process this with automake to create Makefile.in

LIBS =

SUBDIRS = gdk

fxpluginslibdir = $(libdir)/$(PACKAGE)/plugins/effects/realtime/weed

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static

shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -Wl,-Bsymbolic -module -avoid-version --tag=disable-static

shared_cflags = -fPIC -DPIC
endif

AM_LDFLAGS = $(shared_ldflags) -O3 ../../libweed/libweed-utils.la ../../libweed/libweed-plugin-utils.la
AM_CFLAGS = $(shared_cflags) -O3 -ffast-math
AM_CPPFLAGS = $(shared_cflags) -O3 -ffast-math

if HAVE_VISUAL
libvis_libs = libvis.la
libvis_la_LDFLAGS = @VISUAL_LIBS@ $(AM_LDFLAGS)
libvis_la_CFLAGS =  @VISUAL_CFLAGS@ $(AM_CFLAGS)
endif

if HAVE_FREI0R
frei0r_libs = frei0r.la
frei0r_la_LDFLAGS = -ldl $(AM_LDFLAGS)
endif


fxpluginslib_LTLIBRARIES = alien_overlay.la \
	audio_transition.la \
	audio_volume.la \
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
	xeffect.la \
	$(frei0r_libs) \
	$(libvis_libs)
