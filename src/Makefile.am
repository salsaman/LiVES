## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

gtk_def =

cpu_CFLAGS =

AM_CFLAGS =

if HAVE_PTHREAD
pthread_CFLAGS = -DHAVE_PTHREAD=1
pthread_LIBFLAGS = -pthread
endif

if ENABLE_OSC
osc_CPPFLAGS = -I "$(top_srcdir)/libOSC"
endif

if IS_DARWIN
darwin_CPPFLAGS = -I /sw/include
darwin_CFLAGS = -DIS_DARWIN=1
endif

if IS_X86_64
cpu_CFLAGS += -DIS_X86_64
endif

if IS_IRIX
irix_CFLAGS = -DIS_IRIX=1
endif

if IS_LINUX_GNU
linux_CFLAGS = -DIS_LINUX_GNU=1
endif

if IS_LINUX_MUSL
linux_CFLAGS = -DIS_LINUX_MUSL=1
endif

if IS_SOLARIS
solaris_CFLAGS = -DIS_SOLARIS=1
endif

if IS_FREEBSD
freeBSD_CFLAGS = -DIS_FREEBSD=1
endif

if IS_MINGW
mingw_CFLAGS = -DIS_MINGW=1
endif

if HAVE_LADSPA
ladspa_CFLAGS = -DHAVE_LADSPA=1
endif

if HAVE_FREI0R
frei0r_CFLAGS = -DHAVE_FREI0R=1
endif

if HAVE_LIBVISUAL
libvisual_CFLAGS = -DHAVE_LIBVISUAL=1
endif

if HAVE_XRENDER
xrender_CFLAGS = -DHAVE_XRENDER=1
endif

if HAVE_X11
x11_CFLAGS = -DHAVE_X11=1
endif

if HAVE_OPENGL
opengl_CFLAGS = -DHAVE_OPENGL=1
endif

gtk_LDADD = @GTK_LIBS_NODYNAMIC@

bin_PROGRAMS = lives-exe

if HAVE_MJPEGTOOLS
yuv4mpeg_SOURCES = lives-yuv4mpeg.c lives-yuv4mpeg.h
yuv4mpeg_CFLAGS = -DHAVE_YUV4MPEG=1
yuv4mpeg_LDADD = @MJPEGTOOLS_LIBS@
endif

if HAVE_UNICAP
unicap_SOURCES = videodev.c videodev.h
unicap_CFLAGS = -DHAVE_UNICAP=1
unicap_LDADD = @UNICAP_LIBS@
endif

if ENABLE_OSC
osc_SOURCES = osc.c osc.h omc-learn.c omc-learn.h osc_notify.h
osc_LDADD = $(top_builddir)/libOSC/.libs/libOSC.a $(top_builddir)/libOSC/client/.libs/libOSC_client.a -lm
#osc_shared_LIBS = $(top_builddir)/libOSC/.libs/libOSC.so $(top_builddir)/libOSC/client/.libs/libOSC_client.so -lm
osc_CFLAGS = -DENABLE_OSC=1
endif

if ENABLE_LDVGRAB
ldvgrab_SOURCES = ldvgrab.c ldvgrab.h ldvcallbacks.c ldvcallbacks.h ldvinterface.c ldvinterface.h
ldvgrab_CFLAGS = -DHAVE_LDVGRAB=1 -I libavc1394/avc1394.h -I libraw1394/raw1394.h -I libraw1394/rom1394.h
ldvgrab_LDADD = -lavc1394 -lraw1394 -lrom1394
endif

if ENABLE_JACK
jack_CFLAGS = -DENABLE_JACK=1 @JACK_CFLAGS@
jack_SOURCES = jack.c jack.h
jack_LDADD = @JACK_LIBS@ @JACK_SERVER_LIBS@

if ENABLE_JACK_TRANSPORT
jack_CFLAGS += -DENABLE_JACK_TRANSPORT=1
endif

if JACK_V2
jack_CFLAGS += -DJACK_V2=1
endif
endif

if ENABLE_LIBOIL
oil_LDADD = @OIL_LIBS@
if IS_MINGW
else
oil_LIBFLAGS = -loil-0.3
endif
oil_CFLAGS = @OIL_CFLAGS@ -DENABLE_OIL=1

else

if ENABLE_LIBORC
oil_LIBS = @ORC_LIBS@
if IS_MINGW
else
oil_LIBFLAGS = -lorc-0.4
endif
oil_CFLAGS = @ORC_CFLAGS@ -DENABLE_ORC=1
endif
endif

if HAVE_ALSA
alsa_LDADD = @ALSA_LIBS@
alsa_CFLAGS = @ALSA_CFLAGS@ -DALSA_MIDI=1
endif

if HAVE_WAYLAND
wayland_LDADD = @WAYLAND_LIBS@
wayland_CFLAGS = @WAYLAND_CFLAGS@ -DHAVE_WAYLAND=1
endif

if HAVE_LIBEXPLAIN
libexplain_LDADD = @LIBEXPLAIN_LIBS@
libexplain_CFLAGS = @LIBEXPLAIN_CFLAGS@ -DHAVE_LIBEXPLAIN=1
endif

if !NEED_LOCAL_WEED
libweed_LDADD = @WEED_LIBS@
libweed_CFLAGS = @WEED_CFLAGS@
else
libweed_LDADD = $(abs_top_srcdir)/libweed/libweed.la
libweed_CFLAGS = -DNEED_LOCAL_WEED=1
endif

if !NEED_LOCAL_WEED_UTILS
libweed_LDADD += @WEED_UTILS_LIBS@
else
libweed_LDADD += $(abs_top_srcdir)/libweed/libweed-utils.la
libweed_CFLAGS += -DNEED_LOCAL_WEED_UTILS=1
endif

if !NEED_LOCAL_WEED_HOST_UTILS
libweed_LDADD += @WEED_HOST_UTILS_LIBS@
else
libweed_LDADD += $(abs_top_srcdir)/libweed/libweed-host-utils.la
libweed_CFLAGS += -DNEED_LOCAL_WEED_HOST_UTILS=1
endif


if DISABLE_GSLICE
libweed_CFLAGS += -DDISABLE_GSLICE=1
endif

if NEED_LOCAL_WEED_UTILS
libweed_CFLAGS += -DNEED_LOCAL_WEED_UTILS=1
endif

if NEED_LOCAL_WEED_COMPAT
libweed_compat_CFLAGS = -DNEED_LOCAL_WEED_COMPAT=1
endif

if HAVE_PULSE_AUDIO
pulse_LDADD = @PULSE_LIBS@
pulse_CFLAGS = @PULSE_CFLAGS@ -DHAVE_PULSE_AUDIO=1
pulse_SOURCES = pulse.c pulse.h
if HAVE_PA_STREAM_BEGIN_WRITE
pulse_CFLAGS += -DHAVE_PA_STREAM_BEGIN_WRITE=1
endif
endif

if HAVE_LIBPNG
png_LDADD = @PNG_LIBS@
png_CFLAGS = @PNG_CFLAGS@ -DUSE_LIBPNG=1
endif

if HAVE_SWRESAMPLE
swresample_LDADD = @LIBSWRESAMPLE_LIBS@
swresample_CFLAGS = @LIBSWRESAMPLE_CFLAGS@ -Wno-deprecated-declarations -DHAVE_SWRESAMPLE=1
endif

if HAVE_SWSCALE
if HAVE_AVCODEC
if HAVE_AVUTIL
swscale_LDADD = @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@
swscale_CFLAGS = @LIBSWSCALE_CFLAGS@ @LIBAVUTIL_CFLAGS@ -DUSE_SWSCALE=1
if HAVE_SWRESAMPLE
transcode_SOURCES = transcode.c transcode.h
transcode_CFLAGS = -DLIBAV_TRANSCODE=1
endif
endif
endif
endif

if ENABLE_GIW
giw_SOURCES = giw/giwknob.c giw/giwknob.h giw/giwvslider.c giw/giwvslider.h giw/giwled.c giw/giwled.h giw/giwtimeline.c giw/giwtimeline.h
giw_LDADD = -lm
giw_CFLAGS = -DENABLE_GIW=1
endif

if !DISABLE_RPMALLOC
rpmalloc_SOURCES = rpmalloc.c rpmalloc.h
rpmalloc_CFLAGS = -DUSE_RPMALLOC=1
endif

##################### all sources ################3

lives_exe_SOURCES = \
	$(rpmalloc_SOURCES)\
	bundles.c bundles.h \
	lsd.h lsd-tab.c lsd-tab.h \
	effects.c effects.h\
	colourspace.c colourspace.h\
	cvirtual.c cvirtual.h \
	audio.c audio.h \
	threading.c threading.h \
	functions.c functions.h \
	intents.c intents.h object-constants.h\
	preferences.c preferences.h\
	effects-weed.c effects-weed.h\
	memory.c memory.h \
	diagnostics.c diagnostics.h \
	machinestate.c machinestate.h \
	events.c events.h \
	support.c support.h \
	messaging.c messaging.h \
	callbacks.c callbacks.h \
	clip_load_save.c clip_load_save.h \
	player-control.c player-control.h \
	widget-helper.c widget-helper.h \
	widget-helper-gtk.h widget-helper-qt.h \
	main.c main.h \
        mainwindow.h \
	plugins.c plugins.h\
	startup.c startup.h \
	filesystem.c filesystem.h \
	lists.c lists.h \
	layers.c layers.h \
	nodemodel.c nodemodel.h \
	cliphandler.c cliphandler.h \
	player.c player.h \
	dialogs.c dialogs.h\
	interface.c interface.h \
	gui.c gui.h \
	sethandler.c sethandler.h \
	resample.c resample.h \
	$(jack_SOURCES) \
	$(pulse_SOURCES) \
	paramwindow.c paramwindow.h\
	utils.c utils.h\
	maths.c maths.h \
	effects-data.c effects-data.h \
	multitrack.h multitrack.c \
	multitrack-gui.h multitrack-gui.c \
	paramspecial.c paramspecial.h\
	rfx-builder.c rfx-builder.h\
	$(yuv4mpeg_SOURCES)\
	rte_window.c rte_window.h\
	user-interface.c user-interface.h \
	frameloader.c frameloader.h \
	setup.c setup.h \
	stringfuncs.c stringfuncs.h \
	ce_thumbs.c ce_thumbs.h \
	pangotext.c pangotext.h \
	alarms.c alarms.h \
	merge.c merge.h \
	framedraw.c framedraw.h\
	$(osc_SOURCES) \
	keyboard.c keyboard.h\
	stream.h stream.c \
	htmsocket.c htmsocket.h \
	$(transcode_SOURCES) \
	$(unicap_SOURCES) \
	$(ldvgrab_SOURCES) \
	$(giw_SOURCES)


#############
## not sure what the differece is between CPPFLAGS and CFLAGS

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DLIVES_DIR=\""$(livesdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-I $(top_srcdir) \
	$(osc_CPPFLAGS) \
	-mrtm -mxsave	\
	@MJPEGTOOLS_CFLAGS@ \
	@UNICAP_CFLAGS@ \
	@GTK_CFLAGS@ \
	@X11_CFLAGS@ \
	$(darwin_CPPFLAGS) @TURBO_CFLAGS@

###

AM_CFLAGS += -fcommon -fPIE -Wstrict-aliasing=0 -Wall \
	$(yuv4mpeg_CFLAGS) $(ldvgrab_CFLAGS) $(dvgrab_CFLAGS) \
	$(oil_CFLAGS) $(wayland_CFLAGS) $(transcode_CFLAGS) $(cpu_CFLAGS) 	\
	$(darwin_CFLAGS) $(irix_CFLAGS) $(linux_CFLAGS) $(solaris_CFLAGS) $(freeBSD_CFLAGS) $(mingw_CFLAGS) \
	$(osc_CFLAGS) $(alsa_CFLAGS) $(png_CFLAGS) $(swscale_CFLAGS) \
	$(jack_CFLAGS) $(pulse_CFLAGS) $(libexplain_CFLAGS) $(giw_CFLAGS) $(unicap_CFLAGS) \
	$(libweed_CFLAGS) $(libweed_compat_CFLAGS) \
	-DLIVES_LIBDIR=\""$(libdir)"\" $(gtk_def) @TURBO_CFLAGS@ \
	$(libvisual_CFLAGS) $(frei0r_CFLAGS) $(ladspa_CFLAGS) $(ffthumb_CFLAGS) \
	$(xrender_CFLAGS) $(x11_CFLAGS) $(opengl_CFLAGS) $(pthread_CFLAGS)

## NOTE: we also add to AM_CFLAGS below then set AM_CXX_FLAGS ???

## this below is correct; items like -pthread need to go in _LDFLAGS
## libraries go in _LDADD, because, confusingly, if you add them in _LIBS they get placed
## BEFORE the source file, and apparently that makes a difference...

## also it has to be oil_LIBFLAGS instead of oil_LDFLAGS because  otherwise
## automake complains (it should be liboil_LDFLAGS) etc. same with pthread amd yuv4mpeg

lives_exe_LDFLAGS = $(oil_LIBFLAGS)

lives_exe_LDADD = @X11_LIBS@ $(wayland_LDADD) $(gtk_LDADD) $(oil_LDADD)\
	$(osc_LDADD) $(jack_LDADD) $(ldvgrab_LDADD) \
	$(alsa_LDADD) $(pulse_LDADD) $(png_LDADD) $(swscale_LDADD) \
	$(pthread_LIBADD) $(libweed_LDADD) $(swresample_LDADD) \
	$(giw_LDADD) $(libexplain_LDADD) $(unicap_LDADD) $(yuv4mpeg_LDADD)

################################

if HAVE_AVFORMAT
if HAVE_AVCODEC
if HAVE_AVUTIL
AM_CFLAGS += -DHAVE_WEBM=1
endif
endif
endif

if HAVE_LINUX_JOYSTICK_H
AM_CFLAGS += -DHAVE_LINUX_JOYSTICK_H=1
endif

if HAVE_PRCTL
AM_CFLAGS += -DHAVE_PRCTL=1
endif

if ENABLE_PROFILING
AM_CFLAGS += -pg
endif

################################3

AM_CXXFLAGS = $(AM_CFLAGS)

####################################3

if BUILD_LIBLIVES

liblives_libs = liblives.la
lib_LTLIBRARIES = $(liblives_libs)

liblives_la_SOURCES = $(lives_exe_SOURCES) liblives.cpp liblives.hpp lbindings.c lbindings.h 

liblives_la_CFLAGS = -fvisibility=hidden
if IS_DARWIN
liblives_la_LIBFLAGS = -dynamiclib -module
liblives_la_CFLAGS += -fno-common
else
liblives_la_LIBFLAGS = -fPIC -DPIC -module
liblives_la_CFLAGS += -shared -fPIC -DPIC
endif

liblivesdir = $(libdir)

extra_LDFLAGS = @X11_LIBS@ $(wayland_LIBS) @MJPEGTOOLS_LIBS@ \
        $(jack_LIBS) $(ldvgrab_LIBS) \
	$(alsa_LIBS) $(pulse_LIBS) $(png_LIBS) $(swscale_LIBS) $(pthread_LIBS) \
	$(giw_LIBS) @UNICAP_LIBS@ $(gtk_LIBFLAGS) $(oil_LIBFLAGS)

lives_exe_LDFLAGS += -shared $(extra_LDFLAGS)

#if NEED_SCRIPTING_LIBS
#if !NEED_LOCAL_WEED
#libweed_scripting_LIBS = -lweed_gslice -lweed-utils
#libweed_scripting_CFLAGS = @WEED_CFLAGS@
#else
#libweed_scripting_LIBS = $(abs_top_srcdir)/libweed/libweed_slice.la $(abs_top_srcdir)/libweed/libweed-utils.la
#libweed_scripting_CFLAGS = @WEED_CFLAGS@ -D__LIBWED__ -DNEED_LOCAL_WEED=1
#endif
#endif

liblives_la_LDFLAGS = $(extra_LDFLAGS) -lOSC
liblives_la_CFLAGS += $(AM_CFLAGS) -Wno-unused-function

install-exec-local:
	$(MKDIR_P) "$(DESTDIR)$(includedir)/liblives/"
	cp -f "$(srcdir)/liblives.hpp" "$(DESTDIR)$(includedir)/liblives/"

uninstall-local:
	rm -rf "$(includedir)/liblives"

endif

clean-local:
	-rm $(srcdir)/.deps/*
