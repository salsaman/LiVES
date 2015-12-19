## Process this with automake to create Makefile.in

LIBS=

videopluginslibdir = "$(libdir)/$(PACKAGE)/plugins/playback/video"

if IS_DARWIN
shared_ldflags =  -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common -DIS_DARWIN=1
else
shared_ldflags =  -shared -fPIC -DPIC -module -avoid-version --tag=disable-static
shared_cflags =  -fPIC -DPIC
endif



if IS_IRIX
shared_cflags +=-DIS_IRIX=1
endif

if IS_MINGW
shared_cflags += -DIS_MINGW=1
shared_ldflags += -no-undefined -Wl,/usr/i686-pc-mingw32/sys-root/mingw/lib/libSDL.dll.a -Wl,/usr/i686-pc-mingw32/sys-root/mingw/lib/libSDLmain.a -mwindows -Wl,/usr/i686-pc-mingw32/sys-root/mingw/lib/libmingw32.a
SDL_libs = SDL.la
CFLAGS_SDL = @SDL_CFLAGS@

else

if HAVE_SDL
SDL_libs = SDL.la
CFLAGS_SDL = @SDL_CFLAGS@ -DHAVE_SDL=1
LDFLAGS_SDL = @SDL_LIBS@

else
if FORCE_SDL
SDL_libs = SDL.la
CFLAGS_SDL = -I $(SDL_PREFIX) -DHAVE_SDL=1
LDFLAGS_SDL = -lSDL
else

if HAVE_SDL2
SDL_libs = SDL.la
CFLAGS_SDL = @SDL2_CFLAGS@ -DHAVE_SDL2=1
LDFLAGS_SDL = @SDL2_LIBS@

endif
endif
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

if HAVE_X11
if HAVE_XRENDER
if HAVE_OPENGL
if HAVE_GLEE
if HAVE_GLU
openGL_libs = openGL.la
openGL_la_SOURCES = openGL.cpp
openGL_la_CPPFLAGS = $(shared_cflags) $(weed_cflags) @X11_CFLAGS@ @XRENDER_CFLAGS@ @GL_CFLAGS@ @GLEE_CFLAGS@ \
	@GLU_CFLAGS@
openGL_la_LDFLAGS = $(shared_ldflags) @X11_LIBS@ @XRENDER_LIBS@ @GL_LIBS@ @GLEE_LIBS@ @GLU_LIBS@



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
endif

if IS_MINGW
else
lives2lives_stream_libs = lives2lives_stream.la 
lives2lives_stream_la_CFLAGS = $(shared_cflags) $(weed_cflags)
lives2lives_stream_la_LDFLAGS = $(shared_ldflags)
endif

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

videopluginslib_LTLIBRARIES = $(SDL_libs) $(YUV4MPEG_libs) $(oggstream_libs) $(icecast_output_libs) \
	$(vjack_output_libs) $(lives2lives_stream_libs) $(vloopback_libs) $(vloopback2_libs) \
	$(openGL_libs)

EXTRA_DIST = videoplugin.h
