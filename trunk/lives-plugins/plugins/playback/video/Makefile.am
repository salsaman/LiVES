## Process this with automake to create Makefile.in

LIBS=

videopluginslibdir = "$(libdir)/$(PACKAGE)/plugins/playback/video"

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
openGL_libs = openGL.la
openGL_la_SOURCES = openGL.cpp
openGL_la_CPPFLAGS = $(shared_cflags) $(weed_cflags) @X11_CFLAGS@ @XRENDER_CFLAGS@ @GL_CFLAGS@ @GLEE_CFLAGS@
openGL_la_LDFLAGS = $(shared_ldflags) @X11_LIBS@ @XRENDER_LIBS@ @GL_LIBS@ @GLEE_LIBS@

if HAVE_GLU
openGL_trickery_libs = openGL_trickery.la
openGL_trickery_la_SOURCES = openGL_trickery.cpp
openGL_trickery_la_CPPFLAGS = $(shared_cflags) $(weed_cflags) @X11_CFLAGS@ @XRENDER_CFLAGS@ @GL_CFLAGS@ @GLEE_CFLAGS@ \
	@GLU_CFLAGS@
openGL_trickery_la_LDFLAGS = $(shared_ldflags) @X11_LIBS@ @XRENDER_LIBS@ @GL_LIBS@ @GLEE_LIBS@ @GLU_LIBS@



if HAVE_SYSTEM_WEED
openGL_trickery_la_LDFLAGS += $(shared_ldflags) @WEED_UTILS_LIBS@ 
openGL_trickery_la_CPPFLAGS += -DHAVE_SYSTEM_WEED
else
openGL_trickery_la_LDFLAGS += $(shared_ldflags) ../../../../libweed/libweed-utils.la
endif

if HAVE_SYSTEM_WEED_PLUGIN_UTILS
openGL_trickery_la_LDFLAGS += @WEED_PLUGIN_UTILS_LIBS@
openGL_trickery_la_CPPFLAGS += -DHAVE_SYSTEM_WEED_PLUGIN_UTILS
else
openGL_trickery_la_LDFLAGS += ../../../../libweed/libweed-plugin-utils.la
endif




endif

endif
endif
endif
endif

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

videopluginslib_LTLIBRARIES = $(SDL_libs) $(YUV4MPEG_libs) $(oggstream_libs) $(icecast_output_libs) $(vjack_output_libs) lives2lives_stream.la $(vloopback_libs) $(vloopback2_libs) $(openGL_libs) $(openGL_trickery_libs)

EXTRA_DIST = videoplugin.h
