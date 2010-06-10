## Process this with automake to create Makefile.in



LIBS=

decoderpluginslibdir = $(libdir)/$(PACKAGE)/plugins/decoders

if IS_DARWIN
shared_ldflags = -O3 -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -O3 -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags = -O3 -fPIC -DPIC
endif


if HAVE_LIBDV
dv_libs = dv_decoder.la
dv_decoder_la_SOURCES = dv_decoder.c dv_decoder.h
dv_decoder_la_CFLAGS = $(shared_cflags) @LIBDV_CFLAGS@
dv_decoder_la_LDFLAGS = $(shared_ldflags) @LIBDV_LIBS@
endif

if HAVE_OGG
if HAVE_THEORA
ogg_theora_libs = ogg_theora_decoder.la
ogg_theora_decoder_la_SOURCES = ogg_theora_decoder.c ogg_theora_decoder.h
ogg_theora_decoder_la_CFLAGS = $(shared_cflags) @THEORA_CFLAGS@
ogg_theora_decoder_la_LDFLAGS = $(shared_ldflags) @THEORA_LIBS@
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

decoderpluginslib_LTLIBRARIES = $(ogg_theora_libs) $(dv_libs) $(avformat_libs) 

EXTRA_DIST = decplugin.h
