## Process this with automake to create Makefile.in

LIBS=

decoderpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/decoders"

if IS_DARWIN
shared_ldflags =  -lpthread -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common -DIS_DARWIN=1 -I /sw/include
else
shared_ldflags =  -lpthread -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags =  -fPIC -DPIC
endif


if IS_MINGW
shared_cflags =  -DIS_MINGW -I /usr/include/ffmpeg -I /usr/i686-pc-mingw32/sys-root/mingw/include
shared_ldflags += -no-undefined -Wl,/home/gabriel/lives-wine-test/ffmpeg/avcodec-54.dll -Wl,/home/gabriel/lives-wine-test/ffmpeg/avformat-54.dll -Wl,/home/gabriel/lives-wine-test/ffmpeg/avutil-51.dll
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

if IS_MINGW

HAVE_AVFORMAT = true
HAVE_AVCODEC = true
HAVE_AVUTIL = true

ogg_decoder_la_LDFLAGS = $(shared_ldflags) -Wl,/usr/i686-pc-mingw32/sys-root/mingw/lib/libogg.dll.a

endif



if HAVE_AVFORMAT
if HAVE_AVCODEC
if HAVE_AVUTIL

libav_cflags = -DHAVE_LIBAV_LIBS=1

if HAVE_AVCODEC_OPEN2
libav_cflags += -DHAVE_AVCODEC_OPEN2=1
endif

if HAVE_AVCODEC_ALLOC_CONTEXT3
libav_cflags += -DHAVE_AVCODEC_ALLOC_CONTEXT3=1
endif

if HAVE_AVCODEC_GET_NAME
libav_cflags += -DHAVE_AVCODEC_GET_NAME=1
endif

if HAVE_AV_CODEC_SET_PKT_TIMEBASE
libav_cflags += -DHAVE_AV_CODEC_SET_PKT_TIMEBASE=1
endif

if HAVE_AVFORMAT_NEW_STREAM
libav_cflags += -DHAVE_AVFORMAT_NEW_STREAM=1
endif

if HAVE_AVPACKET_UNREF
libav_cflags += -DHAVE_AVPACKET_UNREF=1
endif

if HAVE_AVFRAME_UNREF
libav_cflags += -DHAVE_AVFRAME_UNREF=1
endif

if HAVE_AVFORMAT_FIND_STREAM_INFO
libav_cflags += -DHAVE_AVFORMAT_FIND_STREAM_INFO=1
endif

if HAVE_AVFORMAT_CLOSE_INPUT
libav_cflags += -DHAVE_AVFORMAT_CLOSE_INPUT=1
endif

if HAVE_AVFORMAT_INTERNAL_H
libav_cflags += -DHAVE_AVFORMAT_INTERNAL_H=1
endif

if HAVE_AV_SET_PTS_INFO
libav_cflags += -DHAVE_AV_SET_PTS_INFO=1
endif

if HAVE_AVPRIV_SET_PTS_INFO
libav_cflags += -DHAVE_AVPRIV_SET_PTS_INFO=1
endif



flv_libs = flv_decoder.la
FLVDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@
FLVDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ -Wno-deprecated-declarations
flv_decoder_la_SOURCES = flv_decoder.c flv_decoder.h dec_helper.c
flv_decoder_la_CFLAGS = $(libav_cflags) $(shared_cflags) $(weed_cflags) $(FLVDEC_CFLAGS)
flv_decoder_la_LDFLAGS = $(shared_ldflags) $(FLVDEC_LIBS)


asf_libs = asf_decoder.la
ASFDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@
ASFDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ -Wno-deprecated-declarations
asf_decoder_la_SOURCES = asf_decoder.c asf_decoder.h dec_helper.c
asf_decoder_la_CFLAGS = $(libav_cflags) $(shared_cflags) $(weed_cflags) $(ASFDEC_CFLAGS)
asf_decoder_la_LDFLAGS = $(shared_ldflags) $(ASFDEC_LIBS)



mkv_libs = mkv_decoder.la
MKVDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVUTIL_LIBS@ @LIBZ_LIBS@ @LIBBZ_LIBS@
MKVDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBAVUTIL_CFLAGS@ @LIBZ_CFLAGS@ @LIBBZ_CFLAGS@ \
	-Wno-deprecated-declarations
mkv_decoder_la_SOURCES = mkv_decoder.c mkv_decoder.h dec_helper.c
mkv_decoder_la_CFLAGS = $(libav_cflags) $(shared_cflags) $(weed_cflags) $(MKVDEC_CFLAGS)
mkv_decoder_la_LDFLAGS = $(shared_ldflags) $(MKVDEC_LIBS)



mpegts_libs = mpegts_decoder.la
MPEGTSDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVUTIL_LIBS@
MPEGTSDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBAVUTIL_CFLAGS@ \
	-Wno-deprecated-declarations
mpegts_decoder_la_SOURCES = mpegts_decoder.c mpegts_decoder.h dec_helper.c
mpegts_decoder_la_CFLAGS = $(libav_cflags) $(shared_cflags) $(weed_cflags) $(MPEGTSDEC_CFLAGS)
mpegts_decoder_la_LDFLAGS = $(shared_ldflags) $(MPEGTSDEC_LIBS)


zzavformat_libs = zzavformat_decoder.la
AVIDEC_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@
AVIDEC_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ -Wno-deprecated-declarations
zzavformat_decoder_la_SOURCES = avformat_decoder.c avformat_decoder.h
zzavformat_decoder_la_CFLAGS = $(libav_cflags) $(shared_cflags) $(weed_cflags) $(AVIDEC_CFLAGS)
zzavformat_decoder_la_LDFLAGS = $(shared_ldflags) $(AVIDEC_LIBS)


endif
endif
endif


decoderpluginslib_LTLIBRARIES = $(ogg_libs) $(dv_libs) $(flv_libs) $(asf_libs) $(mkv_libs) $(mpegts_libs) $(zzavformat_libs)

EXTRA_DIST = decplugin.h libav_helper.h
