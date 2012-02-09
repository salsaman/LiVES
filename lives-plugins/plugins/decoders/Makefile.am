## Process this with automake to create Makefile.in



LIBS=

decoderpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/decoders"

if IS_DARWIN
shared_ldflags =  -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags =  -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags =  -fPIC -DPIC
endif

if HAVE_SYSTEM_WEED
weed_cflags = -DHAVE_SYSTEM_WEED=1
else
weed_cflags =
endif

if HAVE_SYSTEM_WEED_COMPAT
weed_cflags += -DHAVE_SYSTEM_WEED_COMPAT
endif


if HAVE_LIBDV
dv_libs = dv_decoder.la
dv_decoder_la_SOURCES = dv_decoder.c dv_decoder.h
dv_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags) @LIBDV_CFLAGS@
dv_decoder_la_LDFLAGS = $(shared_ldflags) $(weed_cflags) @LIBDV_LIBS@
endif

if HAVE_OGG
ogg_libs = ogg_decoder.la
ogg_decoder_la_SOURCES = ogg_decoder.c ogg_decoder.h
ogg_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags)
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


# TODO - at least mpegts, mpegps, mp4, mov, mng
# provided they support frame accurate seeking

# avi is broken and needs a patch for decoding

# h264 does not work and needs a patch


if HAVE_AVFORMAT
if HAVE_AVCODEC
if HAVE_AVUTIL
flv_libs = flv_decoder.la
FLVDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@
FLVDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ -Wno-deprecated-declarations
flv_decoder_la_SOURCES = flv_decoder.c flv_decoder.h
flv_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags) $(FLVDEC_CFLAGS)
flv_decoder_la_LDFLAGS = $(shared_ldflags) $(FLVDEC_LIBS)

asf_libs = asf_decoder.la
ASFDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@
ASFDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ -Wno-deprecated-declarations
asf_decoder_la_SOURCES = asf_decoder.c asf_decoder.h
asf_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags) $(ASFDEC_CFLAGS)
asf_decoder_la_LDFLAGS = $(shared_ldflags) $(ASFDEC_LIBS)


mkv_libs = mkv_decoder.la
MKVDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVUTIL_LIBS@ @LIBZ_LIBS@ @LIBBZ_LIBS@
MKVDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBAVUTIL_CFLAGS@ @LIBZ_CFLAGS@ @LIBBZ_CFLAGS@ \
	-Wno-deprecated-declarations
mkv_decoder_la_SOURCES = mkv_decoder.c mkv_decoder.h
mkv_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags) $(MKVDEC_CFLAGS)
mkv_decoder_la_LDFLAGS = $(shared_ldflags) $(MKVDEC_LIBS)



#in progress

mpegts_libs = mpegts_decoder.la
MPEGTSDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVUTIL_LIBS@
MPEGTSDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBAVUTIL_CFLAGS@ \
	-Wno-deprecated-declarations
mpegts_decoder_la_SOURCES = mpegts_decoder.c mpegts_decoder.h
mpegts_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags) $(MPEGTSDEC_CFLAGS)
mpegts_decoder_la_LDFLAGS = $(shared_ldflags) $(MPEGTSDEC_LIBS)



#does not work - unable to parse video packets (patch would be very nice...)

#avi_libs = avi_decoder.la
#AVIDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@
#AVIDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ -Wno-deprecated-declarations
#avi_decoder_la_SOURCES = avi_decoder.c avi_decoder.h
#avi_decoder_la_CFLAGS = $(shared_cflags) $(weed_cflags) $(AVIDEC_CFLAGS)
#avi_decoder_la_LDFLAGS = $(shared_ldflags) $(AVIDEC_LIBS)


endif
endif
endif



decoderpluginslib_LTLIBRARIES = $(ogg_libs) $(dv_libs) $(flv_libs) $(asf_libs) $(mkv_libs) $(mpegts_libs) #$(avi_libs)

EXTRA_DIST = decplugin.h
