## Process this with automake to create Makefile.in

LIBS=

videopluginslibdir = $(libdir)/$(PACKAGE)/plugins/playback/video

if IS_DARWIN
shared_ldflags =  -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags =  -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags =  -fPIC -DPIC
endif

if HAVE_SYSTEM_WEED
weed_cflags = -DHAVE_SYSTEM_WEED=1
endif

if HAVE_SDL
SDL_libs = SDL.la
CFLAGS_SDL = @SDL_CFLAGS@
LDFLAGS_SDL = @SDL_LIBS@

else
if FORCE_SDL
SDL_libs = SDL.la
CFLAGS_SDL = -I $(SDL_PREFIX)
LDFLAGS_SDL = -lSDL
endif
endif

if HAVE_MJPEGTOOLS
YUV4MPEG_libs = yuv4mpeg_stream.la
oggstream_libs = oggstream.la
endif

if ENABLE_VJACK
vjack_output_libs = vjack_output.la
vjack_output_la_CFLAGS = $(shared_cflags) $(weed_cflags)
vjack_output_la_LDFLAGS = $(shared_ldflags) -ljack
endif

lives2lives_stream_la_CFLAGS = $(shared_cflags) $(weed_cflags)
lives2lives_stream_la_LDFLAGS = $(shared_ldflags)

yuv4mpeg_stream_la_CFLAGS = @MJPEGTOOLS_CFLAGS@ $(shared_cflags) $(weed_cflags)
yuv4mpeg_stream_la_LDFLAGS = $(shared_ldflags) @MJPEGTOOLS_LIBS@

oggstream_la_CFLAGS = @MJPEGTOOLS_CFLAGS@ $(shared_cflags) $(weed_cflags)
oggstream_la_LDFLAGS = $(shared_ldflags) @MJPEGTOOLS_LIBS@

SDL_la_CFLAGS = $(shared_cflags) $(CFLAGS_SDL) $(weed_cflags)
SDL_la_LDFLAGS = $(shared_ldflags) $(LDFLAGS_SDL)

if IS_LINUX_GNU
if HAVE_LIBv4l1
vloopback_libs = vloopback.la
vloopback_la_CFLAGS = $(shared_cflags) $(weed_cflags) -Dv4l1_INCFILE=@v4l1_INCFILE@
vloopback_la_LDFLAGS = $(shared_ldflags)
endif
endif

videopluginslib_LTLIBRARIES = $(SDL_libs) $(YUV4MPEG_libs) $(oggstream_libs) $(vjack_output_libs) lives2lives_stream.la $(vloopback_libs)

EXTRA_DIST = videoplugin.h
