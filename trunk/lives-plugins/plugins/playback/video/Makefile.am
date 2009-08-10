## Process this with automake to create Makefile.in

LIBS=

videopluginslibdir = $(libdir)/$(PACKAGE)/plugins/playback/video

if IS_DARWIN
shared_ldflags = -O3 -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -O3 -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags = -O3 -fPIC -DPIC
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
endif

if ENABLE_VJACK
vjack_output_libs = vjack_output.la
vjack_output_la_CFLAGS = $(shared_cflags)
vjack_output_la_LDFLAGS = $(shared_ldflags) -ljack
endif

lives2lives_stream_la_CFLAGS = $(shared_cflags)
lives2lives_stream_la_LDFLAGS = $(shared_ldflags)

yuv4mpeg_stream_la_CFLAGS = @MJPEGTOOLS_CFLAGS@ $(shared_cflags)
yuv4mpeg_stream_la_LDFLAGS = $(shared_ldflags) @MJPEGTOOLS_LIBS@

SDL_la_CFLAGS = $(shared_cflags) $(CFLAGS_SDL)
SDL_la_LDFLAGS = $(shared_ldflags) $(LDFLAGS_SDL)

videopluginslib_LTLIBRARIES = $(SDL_libs) $(YUV4MPEG_libs) $(vjack_output_libs) lives2lives_stream.la

EXTRA_DIST = videoplugin.h
