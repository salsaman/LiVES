## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

gtk_def =

if ENABLE_OSC
osc_CPPFLAGS = -I "$(top_srcdir)/libOSC"
endif

if IS_DARWIN
darwin_CPPFLAGS = -I /sw/include
darwin_CFLAGS = -DIS_DARWIN=1
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

gtk_LIBFLAGS = @GTK_LIBS_NODYNAMIC@ -pthread

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DLIVES_DIR=\""$(livesdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-I $(top_srcdir) \
	$(osc_CPPFLAGS) \
	@MJPEGTOOLS_CFLAGS@ \
	@UNICAP_CFLAGS@ \
	@GTK_CFLAGS@ \
	@X11_CFLAGS@ \
	$(darwin_CPPFLAGS)

bin_PROGRAMS = lives-exe

if HAVE_MJPEGTOOLS
yuv4mpeg_SOURCES = lives-yuv4mpeg.c lives-yuv4mpeg.h
yuv4mpeg_CFLAGS = -DHAVE_YUV4MPEG=1
endif

if HAVE_UNICAP
unicap_SOURCES = videodev.c videodev.h
unicap_CFLAGS = -DHAVE_UNICAP=1
endif

if ENABLE_OSC
osc_SOURCES = osc.c osc.h omc-learn.c omc-learn.h osc_notify.h
osc_LDADD = $(top_builddir)/libOSC/.libs/libOSC.a $(top_builddir)/libOSC/client/.libs/libOSC_client.a -lm
osc_CFLAGS = -DENABLE_OSC=1
endif

if ENABLE_LDVGRAB
ldvgrab_SOURCES = ldvgrab.c ldvgrab.h ldvcallbacks.c ldvcallbacks.h ldvinterface.c ldvinterface.h
ldvgrab_CFLAGS = -DHAVE_LDVGRAB=1 -I libavc1394/avc1394.h -I libraw1394/raw1394.h -I libraw1394/rom1394.h
ldvgrab_LDADD = -lavc1394 -lraw1394 -lrom1394
endif

if ENABLE_JACK
jack_CFLAGS = -DENABLE_JACK
jack_SOURCES = jack.c jack.h
jack_LDADD = -ljack

if ENABLE_JACK_TRANSPORT
jack_CFLAGS += -DENABLE_JACK_TRANSPORT
endif
endif

if ENABLE_LIBOIL
if IS_MINGW
oil_LIBFLAGS = @OIL_LIBS@
else
oil_LIBFLAGS = @OIL_LIBS@ -loil-0.3
endif
oil_CFLAGS = @OIL_CFLAGS@ -DENABLE_OIL=1

else

if ENABLE_LIBORC
if IS_MINGW
oil_LIBFLAGS = @ORC_LIBS@
else
oil_LIBFLAGS = @ORC_LIBS@ -lorc-0.4
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

if DISABLE_GLSLICE
if HAVE_SYSTEM_WEED
libweed_LDADD = @WEED_LIBS@ @WEED_UTILS_LIBS@
libweed_CFLAGS = @WEED_CFLAGS@ -DHAVE_SYSTEM_WEED=1
else
libweed_LDADD = $(abs_top_srcdir)/libweed/libweed.la $(abs_top_srcdir)/libweed/libweed-utils.la
endif
else
if HAVE_SYSTEM_WEED
libweed_LDADD = @WEED_SLICE_LIBS@ @WEED_UTILS_LIBS@
libweed_CFLAGS = @WEED_CFLAGS@ -DHAVE_SYSTEM_WEED=1
else
libweed_LDADD = $(abs_top_srcdir)/libweed/libweed_slice.la $(abs_top_srcdir)/libweed/libweed-utils.la
endif
endif

if HAVE_SYSTEM_WEED_COMPAT
libweed_compat_CFLAGS = -DHAVE_SYSTEM_WEED_COMPAT=1
endif

if HAVE_PULSE_AUDIO
pulse_LDADD = @PULSE_LIBS@
pulse_CFLAGS = @PULSE_LIBS@ -DHAVE_PULSE_AUDIO=1
pulse_SOURCES = pulse.c pulse.h
if HAVE_PA_STREAM_BEGIN_WRITE
pulse_CFLAGS += -DHAVE_PA_STREAM_BEGIN_WRITE=1
endif
endif

if HAVE_LIBPNG
png_LDADD = @PNG_LIBS@
png_CFLAGS = @PNG_CFLAGS@ -DUSE_LIBPNG=1
endif

if HAVE_SWSCALE
if HAVE_AVCODEC
if HAVE_AVUTIL
if HAVE_SWSCALE
swscale_LDADD = @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@
swscale_CFLAGS = @LIBSWSCALE_CFLAGS@ @LIBAVUTIL_CFLAGS@ -DUSE_SWSCALE=1
if HAVE_SWRESAMPLE
transcode_SOURCES = transcode.c transcode.h
transcode_CFLAGS = -DLIBAV_TRANSCODE=1
endif
endif
endif
endif
endif

if ENABLE_GIW
giw_SOURCES = giw/giwknob.c giw/giwknob.h giw/giwvslider.c giw/giwvslider.h giw/giwled.c giw/giwled.h giw/giwtimeline.c giw/giwtimeline.h
giw_LDADD = -lm
giw_CFLAGS = -DENABLE_GIW=1
endif

lives_exe_SOURCES = \
	widget-helper.c widget-helper.h \
	widget-helper-gtk.h widget-helper-qt.h \
	main.c\
	support.c support.h \
	effects.c\
	plugins.c plugins.h\
	effects-weed.c effects-weed.h\
	effects-data.c effects-data.h \
	framedraw.c framedraw.h\
	interface.c interface.h \
	paramspecial.c paramspecial.h\
	paramwindow.c paramwindow.h\
	rfx-builder.c rfx-builder.h\
	$(yuv4mpeg_SOURCES)\
	preferences.c preferences.h\
	rte_window.c rte_window.h\
	gui.c \
	ce_thumbs.c ce_thumbs.h \
	htmsocket.c htmsocket.h \
	merge.c merge.h \
	dialogs.c saveplay.c \
	audio.c audio.h \
	events.c events.h \
	resample.c resample.h \
	$(osc_SOURCES) \
	callbacks.c callbacks.h \
	colourspace.c colourspace.h\
	keyboard.c keyboard.h\
	utils.c \
        main.h \
        mainwindow.h \
        effects.h \
	multitrack.h multitrack.c \
	stream.h stream.c \
	cvirtual.c cvirtual.h \
	startup.c startup.h \
	pangotext.c pangotext.h \
	$(transcode_SOURCES) \
	$(unicap_SOURCES) \
	$(jack_SOURCES) \
	$(pulse_SOURCES) \
	$(ldvgrab_SOURCES) \
	$(giw_SOURCES)

lives_exe_LDFLAGS = $(gtk_LIBFLAGS) -shared $(oil_LIBFLAGS)

lives_exe_LDADD = @X11_LIBS@ $(wayland_LDADD) @MJPEGTOOLS_LIBS@ $(osc_LDADD) $(jack_LDADD) $(ldvgrab_LDADD) \
	$(alsa_LDADD) $(pulse_LDADD) $(png_LDADD) $(swscale_LDADD) $(pthread_LDADD) $(libweed_LDADD) \
	 $(giw_LDADD) $(v4l1_LDADD) @UNICAP_LIBS@

AM_CFLAGS = -Wstrict-aliasing=0 -Wall $(yuv4mpeg_CFLAGS) $(ldvgrab_CFLAGS) $(dvgrab_CFLAGS) \
	$(oil_CFLAGS) $(wayland_CFLAGS) $(transcode_CFLAGS) \
	$(darwin_CFLAGS) $(irix_CFLAGS) $(linux_CFLAGS) $(solaris_CFLAGS) $(osc_CFLAGS) $(alsa_CFLAGS) $(png_CFLAGS) $(swscale_CFLAGS) \
	$(jack_CFLAGS) $(pulse_CFLAGS) $(giw_CFLAGS) $(unicap_CFLAGS) $(libweed_CFLAGS) $(libweed_compat_CFLAGS) \
	-DLIVES_LIBDIR=\""$(libdir)"\" $(gtk_def)

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

AM_CXXFLAGS = $(AM_CFLAGS)

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

extra_LDFLAGS = @X11_LIBS@ $(wayland_LDADD) @MJPEGTOOLS_LIBS@ \
        $(jack_LDADD) $(ldvgrab_LDADD) \
	$(alsa_LDADD) $(pulse_LDADD) $(png_LDADD) $(swscale_LDADD) $(pthread_LDADD) \
	$(giw_LDADD) $(v4l1_LDADD) @UNICAP_LIBS@ $(gtk_LIBFLAGS) $(oil_LIBFLAGS)

if HAVE_SYSTEM_WEED
libweed_scripting_LDADD = -lweed_slice_scripting -lweed-utils_scripting
libweed_scripting_CFLAGS = @WEED_CFLAGS@ -DHAVE_SYSTEM_WEED=1 -D_WEED_VISIBLE_FUNCTIONS_
else
libweed_scripting_LDADD = $(abs_top_srcdir)/libweed/libweed_slice_scripting.la $(abs_top_srcdir)/libweed/libweed-utils_scripting.la
libweed_scripting_CFLAGS = -D_WEED_VISIBLE_FUNCTIONS_
endif

liblives_la_LDFLAGS = $(extra_LDFLAGS) $(libweed_scripting_LDADD) $(osc_LDADD)

liblives_la_CFLAGS += $(AM_CFLAGS) $(libweed_scripting_CFLAGS) -Wno-unused-function -DIS_LIBLIVES

install-exec-local:
	$(MKDIR_P) "$(DESTDIR)$(includedir)/liblives/"
	cp -f "$(srcdir)/liblives.hpp" "$(DESTDIR)$(includedir)/liblives/"

uninstall-local:
	rm -rf "$(includedir)/liblives"

endif
