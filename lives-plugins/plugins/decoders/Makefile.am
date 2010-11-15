## Process this with automake to create Makefile.in



LIBS=

decoderpluginslibdir = $(libdir)/$(PACKAGE)/plugins/decoders

if IS_DARWIN
shared_ldflags =  -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags =  -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags =  -fPIC -DPIC
endif

if HAVE_SYSTEM_WEED
shared_cflags = $(shared_cflags) -DHAVE_SYSTEM_WEED=1
endif

if HAVE_LIBDV
dv_libs = dv_decoder.la
dv_decoder_la_SOURCES = dv_decoder.c dv_decoder.h
dv_decoder_la_CFLAGS = $(shared_cflags) @LIBDV_CFLAGS@
dv_decoder_la_LDFLAGS = $(shared_ldflags) @LIBDV_LIBS@
endif

if HAVE_OGG
ogg_libs = ogg_decoder.la
ogg_decoder_la_SOURCES = ogg_decoder.c ogg_decoder.h
ogg_decoder_la_CFLAGS = $(shared_cflags)
ogg_decoder_la_LDFLAGS = $(shared_ldflags)
if HAVE_THEORA
ogg_decoder_la_CFLAGS += @THEORA_CFLAGS@ -DHAVE_THEORA=1
ogg_decoder_la_LDFLAGS += @THEORA_LIBS@
endif
if HAVE_DIRAC
ogg_decoder_la_CFLAGS += @DIRAC_CFLAGS@ -DHAVE_DIRAC=1
ogg_decoder_la_LDFLAGS += @DIRAC_LIBS@
endif
endif


##experimental, does not work too well (fps and duration are often wrong; cannot seek to exact frame)
#if HAVE_AVFORMAT
#AVFORMAT_CFLAGS = -I /usr/include
#AVFORMAT_LIBS = -pthread -lavformat -lavcodec -lz -lbz2 -lfaac -lfaad -lmp3lame -lopencore-amrnb -lopencore-amrwb -ltheoraenc -ltheoradec -logg -lvpx -lx264 -lm -ljack -lX11 -lXext -lXfixes -lasound -ldl -lavutil
#avformat_libs = libzz_avformat_decoder.la
#libzz_avformat_decoder_la_SOURCES = avformat_decoder.c avformat_decoder.h
#libzz_avformat_decoder_la_CFLAGS = $(shared_cflags) $(AVFORMAT_CFLAGS)
#libzz_avformat_decoder_la_LDFLAGS = $(shared_ldflags) $(AVFORMAT_LIBS)
#endif


decoderpluginslib_LTLIBRARIES = $(ogg_libs) $(dv_libs)

EXTRA_DIST = decplugin.h
