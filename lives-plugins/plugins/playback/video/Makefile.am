## Process this with automake to create Makefile.in

LIBS=

videopluginslibdir = "$(libdir)/$(PACKAGE)/plugins/playback/video"

if IS_DARWIN
shared_ldflags =  -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common -DIS_DARWIN=1
else
shared_ldflags =  -shared -fPIC -DPIC -module -Wno-unused-function -avoid-version --tag=disable-static
shared_cflags =  -fPIC -DPIC
endif

if IS_IRIX
shared_cflags +=-DIS_IRIX=1
endif

if HAVE_SDL1
SDL_libs = SDL.la
CFLAGS_SDL = @SDL_CFLAGS@ -DHAVE_SDL=1
LDFLAGS_SDL = @SDL_LIBS@

else
if FORCE_SDL
SDL_libs = SDL.la
CFLAGS_SDL = -I $(SDL_PREFIX) -DHAVE_SDL=1
LDFLAGS_SDL = -lSDL
endif
endif

if HAVE_SYSTEM_WEED
weed_cflags = -DHAVE_SYSTEM_WEED=1
endif

if HAVE_MJPEGTOOLS
YUV4MPEG_libs = yuv4mpeg_stream.la
oggstream_libs = oggstream.la
icecast_output_libs = icecast_output.la
endif

if ENABLE_VJACK
vjack_output_libs = vjack_output.la
vjack_output_la_CFLAGS = $(shared_cflags) $(weed_cflags)
vjack_output_la_LDFLAGS = $(shared_ldflags) -ljack
endif

if HAVE_XRENDER
if HAVE_X11
if HAVE_OPENGL
if HAVE_GLU
openGL_libs = openGL.la
openGL_la_SOURCES = openGL.cpp glad.cpp glad_glx.cpp
openGL_la_CPPFLAGS = $(shared_cflags) $(weed_cflags) @X11_CFLAGS@ @XRENDER_CFLAGS@ @GL_CFLAGS@ @GLU_CFLAGS@
openGL_la_LDFLAGS = $(shared_ldflags) @X11_LIBS@ @XRENDER_LIBS@ @GL_LIBS@ @GLU_LIBS@

if HAVE_SYSTEM_WEED
openGL_la_CPPFLAGS += -DHAVE_SYSTEM_WEED
else
openGL_la_LDFLAGS += $(shared_ldflags)
endif

if HAVE_SYSTEM_WEED_PLUGIN
openGL_la_CPPFLAGS += -DHAVE_SYSTEM_WEED_PLUGIN_H
endif

endif
endif
endif
endif

lives2lives_stream_libs = lives2lives_stream.la 
lives2lives_stream_la_CFLAGS = $(shared_cflags) $(weed_cflags)
lives2lives_stream_la_LDFLAGS = $(shared_ldflags)

yuv4mpeg_stream_la_CFLAGS = @MJPEGTOOLS_CFLAGS@ $(shared_cflags) $(weed_cflags)
yuv4mpeg_stream_la_LDFLAGS = $(shared_ldflags) @MJPEGTOOLS_LIBS@

oggstream_la_CFLAGS = @MJPEGTOOLS_CFLAGS@ $(shared_cflags) $(weed_cflags)
oggstream_la_LDFLAGS = $(shared_ldflags) @MJPEGTOOLS_LIBS@

icecast_output_la_CFLAGS = @MJPEGTOOLS_CFLAGS@ $(shared_cflags) $(weed_cflags)
icecast_output_la_LDFLAGS = $(shared_ldflags) @MJPEGTOOLS_LIBS@

SDL_la_CFLAGS = $(shared_cflags) $(CFLAGS_SDL) $(weed_cflags)
SDL_la_LDFLAGS = $(shared_ldflags) $(LDFLAGS_SDL)

if HAVE_LIBv4l1
vloopback_libs = vloopback.la
vloopback_la_CFLAGS = $(shared_cflags) $(weed_cflags) -Dv4l1_INCFILE=@v4l1_INCFILE@
vloopback_la_LDFLAGS = $(shared_ldflags)
endif

if HAVE_LIBv4l2
vloopback2_libs = vloopback2.la
vloopback2_la_CFLAGS = $(shared_cflags) $(weed_cflags)
vloopback2_la_LDFLAGS = $(shared_ldflags)
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

if HAVE_SWRESAMPLE
if HAVE_SWSCALE
libav_stream_libs = libav_stream.la
AVSTREAM_LIBS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVUTIL_LIBS@ @LIBSWSCALE_LIBS@ @LIBSWRESAMPLE_LIBS@
AVSTREAM_CFLAGS = @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBAVUTIL_CFLAGS@ @LIBSWSCALE_CFLAGS@ @LIBSWRESAMPLE_CFLAGS@ -Wno-deprecated-declarations

libav_stream_la_CFLAGS = $(libav_cflags) $(shared_cflags) $(weed_cflags) $(AVSTREAM_CFLAGS)
libav_stream_la_LDFLAGS = $(shared_ldflags) $(AVSTREAM_LIBS) $(libav_ldflags)
endif
endif

endif
endif
endif

videopluginslib_LTLIBRARIES = $(SDL_libs) $(YUV4MPEG_libs) $(oggstream_libs) $(icecast_output_libs) \
	$(vjack_output_libs) $(lives2lives_stream_libs) $(vloopback_libs) $(vloopback2_libs) \
	$(openGL_libs) $(libav_stream_libs)

EXTRA_DIST = videoplugin.h glad.h glad_glx.h
