## Process this with automake to create Makefile.in

LIBS =

AUTOMAKE_OPTIONS = subdir-objects

#NOTE - install-exec-local MUST go in the first subdir ONLY !!

SUBDIRS = scripts . gdk cairo # supercollider experimental

fxpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static -lm @WEED_EFFECT_PLUGIN_LIBS@
shared_cflags = -fno-common  @WEED_EFFECT_PLUGIN_CFLAGS@
else
shared_ldflags = -shared -fPIC -DPIC -module -avoid-version --tag=disable-static -lm  @WEED_EFFECT_PLUGIN_LIBS@
shared_cflags = -fPIC -DPIC -Wno-unused-function -Wno-unused-variable @WEED_EFFECT_PLUGIN_CFLAGS@
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

weed_CFLAGS = -Wno-unused-function

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) -ffast-math -Wall  @TURBO_CFLAGS@ -fno-math-errno
AM_CPPFLAGS = $(AM_CFLAGS)

if HAVE_LIBVISUAL
libvis_libs = libvis.la
libvis_la_LDFLAGS = @LIBVISUAL_LIBS@ -lpthread $(AM_LDFLAGS)
libvis_la_CFLAGS =  @LIBVISUAL_CFLAGS@ $(AM_CFLAGS)
libvispluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed/libvisual"
libvispluginslib_LTLIBRARIES = $(libvis_libs)
endif

if HAVE_FREI0R
frei0r_libs = frei0r.la
frei0r_la_LDFLAGS = -ldl $(AM_LDFLAGS)
frei0r_la_CFLAGS = $(AM_CFLAGS)
frei0rpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed/frei0r"
frei0rpluginslib_LTLIBRARIES = $(frei0r_libs)
endif

if HAVE_OPENCV4
farneback_analyser_libs = farneback_analyser.la
farneback_analyser_la_LDFLAGS = $(AM_LDFLAGS) $(OPENCV4_LIBS_ONLY_L) $(OPENCV4_LIBS_ONLY_OTHER) \
	-lopencv_core -lopencv_video -lopencv_imgproc
farneback_analyser_la_CXXFLAGS = $(OPENCV4_CFLAGS) -DHAVE_OPENCV4
farneback_analyser_la_CPPFLAGS = $(AM_CPPFLAGS)
farneback_analyser_la_SOURCES = farneback_analyser.cpp

if ENABLE_TOONZ
toonz_libs = toonz.la
toonz_la_LDFLAGS = $(AM_LDFLAGS) $(OPENCV4_LIBS_ONLY_L) -lopencv_core -lopencv_imgproc
toonz_la_CPPFLAGS = $(AM_CPPFLAGS) -std=c++11
toonz_la_SOURCES = toonz.cpp
else
toonz_libs =
endif

opencvlibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
opencvlib_LTLIBRARIES = $(farneback_analyser_libs) $(toonz_libs)

else

if HAVE_OPENCV
farneback_analyser_libs = farneback_analyser.la
farneback_analyser_la_LDFLAGS = $(AM_LDFLAGS) $(OPENCV_LIBS_ONLY_L) $(OPENCV_LIBS_ONLY_OTHER) \
	-lopencv_core -lopencv_video -lopencv_imgproc
farneback_analyser_la_CXXFLAGS = $(OPENCV_CFLAGS)
farneback_analyser_la_CPPFLAGS = $(AM_CPPFLAGS)
farneback_analyser_la_SOURCES = farneback_analyser.cpp

if ENABLE_TOONZ
toonz_libs = toonz.la
toonz_la_LDFLAGS = $(AM_LDFLAGS) $(OPENCV_LIBS_ONLY_L) -lopencv_core -lopencv_imgproc
toonz_la_CPPFLAGS = $(AM_CPPFLAGS) -std=c++11
toonz_la_SOURCES = toonz.cpp
else
toonz_libs =
endif

opencvlibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
opencvlib_LTLIBRARIES = $(farneback_analyser_libs) $(toonz_libs)
endif

endif

if HAVE_FFTW3F
audio_fft_libs = audio_fft.la
beat_detector_libs = beat_detector.la
audio_fft_la_LDFLAGS = $(FFTW3F_LIBS) $(AM_LDFLAGS)
audio_fft_la_CFLAGS = $(FFTW3F_CFLAGS) $(AM_CFLAGS)
beat_detector_la_LDFLAGS = $(FFTW3F_LIBS) $(AM_LDFLAGS)
beat_detector_la_CFLAGS = $(FFTW3F_CFLAGS) $(AM_CFLAGS)
audio_fftdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
audio_fft_LTLIBRARIES = $(audio_fft_libs)
beat_detectordir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
beat_detector_LTLIBRARIES = $(beat_detector_libs)
endif

if HAVE_LADSPA
ladspa_libs = ladspa.la
ladspa_la_LDFLAGS = $(AM_LDFLAGS)
ladspa_la_CFLAGS = $(AM_CFLAGS)
ladspadir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
ladspa_LTLIBRARIES = $(ladspa_libs)
endif

if HAVE_LIBFREENECT
freenect_libs = freenect.la
freenect_la_LDFLAGS = $(LIBFREENECT_LIBS) -lpthread $(AM_LDFLAGS)
freenect_la_CFLAGS = $(LIBFREENECT_CFLAGS) $(AM_CFLAGS)
freenectdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"
freenect_LTLIBRARIES = $(freenect_libs)
endif

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

fxpluginslib_LTLIBRARIES = RGBdelay.la \
	alpha_means.la \
	alpha_visualizer.la \
	alien_overlay.la \
	audio_transition.la \
	audio_volume.la \
	bump2d.la \
	blurzoom.la \
	blank_frame_detector.la \
	$(opencv_LIBS) \
	ccorrect.la \
	colorkey.la \
	comic.la \
	data_processor.la \
	data_unpacker.la \
	timer.la \
	deinterlace.la \
	edge.la \
	fg_bg_removal.la \
	fireTV.la \
	fourKlives.la \
	haip.la \
	kaleidoscope.la \
	layout_blends.la \
	lifeTV.la \
	livetext.la \
	log_sig.la \
	mirrors.la \
	multi_blends.la \
	multi_transitions.la \
	alpha_to_grey.la \
	negate.la \
	nn_programmer.la \
	data_counter.la \
	noise.la \
	onedTV.la \
	palette_test.la\
	plasma.la \
	posterise.la \
	randomiser.la \
	revTV.la \
	rippleTV.la \
	rotozoom.la \
	shift.la \
	simple_blend.la \
	slide_over.la \
	softlight.la \
	system.la \
	targeted_zoom.la \
	pan_and_zoom.la \
	textfun.la \
	tone_gen.la \
	tvpic.la \
	vertigo.la \
	warpTV.la \
	xeffect.la

EXTRA_DIST=data icons scripts syna.h

dist: distdir-am all

dist: distdir-am all

distdir-am:
	$(MAKE) -C scripts dist

install-exec-am:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)"

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(fxpluginslibdir)"
	cp -rf data "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/data"
	cp -rf icons "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/icons"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/data/".gitignore
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/icons/".gitignore

