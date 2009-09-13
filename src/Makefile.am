## Process this file with automake to produce Makefile.in

if ENABLE_OSC
osc_INCLUDES=-I $(top_srcdir)/libOSC
endif

if IS_DARWIN
darwin_INCLUDES=-I /sw/include
darwin_CFLAGS=-DIS_DARWIN=1
endif

if IS_IRIX
irix_CFLAGS=-DIS_IRIX=1
endif

if IS_LINUX_GNU
linux_CFLAGS=-DIS_LINUX_GNU=1
endif

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DLIVES_DIR=\""$(livesdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-I $(top_srcdir) \
	$(osc_INCLUDES) \
	@MJPEGTOOLS_CFLAGS@ \
	@GTK_CFLAGS@ \
	$(darwin_INCLUDES)


bin_PROGRAMS = lives-exe

if HAVE_MJPEGTOOLS
yuv4mpeg_SOURCES=lives-yuv4mpeg.c lives-yuv4mpeg.h
yuv4mpeg_CFLAGS = -DHAVE_MJPEGTOOLS=1
endif

if ENABLE_OSC
osc_SOURCES=osc.c osc.h omc-learn.c omc-learn.h osc_notify.h
osc_LDADD=-L$(top_builddir)/libOSC/ -lOSC -L$(top_builddir)/libOSC/client/ -lOSC_client
osc_CFLAGS = -DENABLE_OSC=1
endif

if ENABLE_LDVGRAB
ldvgrab_SOURCES = ldvgrab.c ldvgrab.h ldvcallbacks.c ldvcallbacks.h ldvinterface.c ldvinterface.h
ldvgrab_CFLAGS = -DHAVE_LDVGRAB=1 -I libavc1394/avc1394.h -I libraw1394/raw1394.h -I libraw1394/rom1394.h
ldvgrab_LDADD = -lavc1394 -lraw1394 -lrom1394
endif


if ENABLE_JACK
jack_SOURCES = jack.c jack.h
jack_LDADD=-ljack
endif

if ENABLE_LIBOIL

oil_LDADD = @OIL_LIBS@ -loil-0.3
oil_CFLAGS =  @OIL_CFLAGS@ -DENABLE_OIL=1

endif


if HAVE_ALSA
alsa_LDADD = @ALSA_LIBS@
alsa_CFLAGS = @ALSA_CFLAGS@ -DALSA_MIDI=1
endif



if DISABLE_GLSLICE
libweed_slice_LDADD = ../libweed/libweed.la
else
libweed_slice_LDADD = ../libweed/libweed_slice.la
endif

if HAVE_GTK_VERSION214
gtk_def=-DHAVE_GTK_NICE_VERSION
endif

lives_exe_SOURCES = \
	main.c\
	support.c support.h \
	effects.c\
	plugins.c plugins.h\
	effects-weed.c effects-weed.h\
	framedraw.c framedraw.h\
	interface.c interface.h \
	paramspecial.c paramspecial.h\
	paramwindow.c paramwindow.h\
	rfx-builder.c rfx-builder.h\
	$(yuv4mpeg_SOURCES)\
	preferences.c preferences.h\
	rte_window.c rte_window.h\
	gui.c \
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
	$(jack_SOURCES) \
	$(ldvgrab_SOURCES)

lives_exe_LDADD = @GTK_LIBS_NODYNAMIC@ @MJPEGTOOLS_LIBS@ $(osc_LDADD) $(jack_LDADD) $(ldvgrab_LDADD) $(oil_LDADD) $(alsa_LDADD) -lpthread $(libweed_slice_LDADD) ../libweed/libweed-utils.la

lives_exe_LDFLAGS = -O3
AM_CFLAGS = -O3 $(yuv4mpeg_CFLAGS) $(ldvgrab_CFLAGS) $(dvgrab_CFLAGS) $(oil_CFLAGS) $(darwin_CFLAGS) $(irix_CFLAGS) $(linux_CFLAGS) $(osc_CFLAGS) $(alsa_CFLAGS) -DLIVES_LIBDIR=\""$(libdir)"\" $(gtk_def)


