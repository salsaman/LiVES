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

decoderpluginslib_LTLIBRARIES = $(ogg_theora_libs) $(dv_libs)

EXTRA_DIST = decplugin.h
