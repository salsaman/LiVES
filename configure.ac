dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.57)

ifdef([m4_pattern_allow],
      [m4_pattern_allow([^AC_.*])])

dnl IMPORTANT !! $version must also be set to exactly the same string in smogrify
AC_INIT(LiVES, 3.0.1, https://github.com/salsaman/LiVES/issues, , http://lives-video.com)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE
AC_PREFIX_DEFAULT(/usr)

AC_GNU_SOURCE
AC_ISC_POSIX
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CXX

AC_CONFIG_MACRO_DIR([m4])

AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_SYS_LARGEFILE

AM_PROG_AS

dnl AC_DEFINE([LiVES_VERSION],[],[Description])
dnl AC_CONFIG_HEADERS(config.h)

PTHREAD_OK=false
ACX_PTHREAD
if test "$THREAD_SUPPORT" = "yes"; then
    PTHREAD_OK=true
fi

IS_DARWIN="false"
IS_IRIX="false"
IS_SOLARIS="false"
IS_LINUX_GNU="false"
IS_LINUX_MUSL="false"
IS_MINGW="false"

case "{$target_os}" in
 *darwin*)
   IS_DARWIN="true"
  ;;
 *irix*)
   IS_IRIX="true"
  ;;
 *linux-gnu*)
   IS_LINUX_GNU="true"
  ;;
 *linux-musl*)
   IS_LINUX_MUSL="true"
  ;;
 *solaris*)
   IS_SOLARIS="true"
  ;;
 *mingw*)
   IS_MINGW="true"
  ;;
 *msys*)
   IS_MINGW="true"
  ;;
 *)
  ;;
esac

AM_CONDITIONAL(IS_DARWIN,$IS_DARWIN)
AM_CONDITIONAL(IS_IRIX,$IS_IRIX)
AM_CONDITIONAL(IS_LINUX_GNU,$IS_LINUX_GNU)
AM_CONDITIONAL(IS_LINUX_MUSL,$IS_LINUX_MUSL)
AM_CONDITIONAL(IS_SOLARIS,$IS_SOLARIS)
AM_CONDITIONAL(IS_MINGW,$IS_MINGW)

if test "$IS_MINGW" = "false"; then
AC_CHECK_FILE(lives-plugins/Makefile.am,,[
echo "LiVES plugins were not found. Please make sure you have checked them out."
echo
echo
exit
])
fi

GETTEXT_PACKAGE=lives
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [gtp])

AC_DEFINE_UNQUOTED(LOCALEDIR, "${datadir}/locale", [Name of gettext locale directory])

AC_DEFINE_UNQUOTED(PREFIX, "${prefix}", [prefix])
AC_DEFINE_UNQUOTED(LiVES_VERSION, "${VERSION}", [])

dnl Add the languages which your application supports here.
ALL_LINGUAS="ar fr ca cs da ja nl_NL de_DE pt pt_BR hu es it pl tr ru zh_CN sk he en_GB fi et gl uz te ro uk oc hr ug pms"
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION(0.17)

AC_PROG_MKDIR_P

dnl Use -Wall if we have gcc.
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl

if test "$GXX" = ""; then
   AC_MSG_ERROR([A C++ compiler is required to compile some plugins.])
fi

echo "C++ compiler is $CXX"

dnl Check of -ldl
AC_CHECK_LIB(dl,open)
AM_CONDITIONAL(HAVE_LIBdl,$HAVE_LIBdl)

dnl Check for posix_memalign
AC_CHECK_FUNCS(posix_memalign)

dnl Check for posix_fadvise
AC_CHECK_FUNCS(posix_fadvise)

dnl Check for posix_fallocate
AC_CHECK_FUNCS(posix_fallocate)

AC_PATH_PROGS(PERL, perl perl5)

dnl check for profiling

AC_ARG_ENABLE(profiling, [  --enable-profiling     Enable profiling with gmon.] , profiling=yes, profiling=no)

AM_CONDITIONAL(ENABLE_PROFILING, test $profiling != no)

dnl Check for pkg-config

AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

if test x$PKG_CONFIG = xno; then
   AC_MSG_ERROR([Cannot find pkg-config, make sure it is installed and in your PATH])
fi

dnl Check for doxygen

AC_ARG_ENABLE(doxygen, [  --disable-doxygen     Disable doxygen documentation.] , disable_doxygen=yes)

if test "x$disable_doxygen" != "xyes" ; then

AC_PATH_PROG(DOXYGEN, doxygen, no)

if test $DOXYGEN = no; then
   echo "Cannot find doxygen, will not build sourcecode documentation"
fi

else
   echo "Doxygen documentation disabled, not checking"
   DOXYGEN=no
fi

AM_CONDITIONAL(HAVE_DOXYGEN, test $DOXYGEN != no)

dnl OSC
HAVE_OSC=true
AC_ARG_ENABLE(OSC,
[  --disable-OSC            Disable OSC support.],
[
AC_DEFINE_UNQUOTED(DISABLE_OSC,1, [OSC])
HAVE_OSC=false
]
)
AM_CONDITIONAL(ENABLE_OSC,$HAVE_OSC)

if test "x$IS_LINUX_MUSL" != "xfalse" ; then
if test "x$HAVE_OSC" != "xfalse" ; then

HAVE_TIRPC=false
dnl check for libtirpc
PKG_CHECK_MODULES(TIRPC, libtirpc, HAVE_TIRPC=true, HAVE_TIRPC=false)
AC_SUBST(TIRPC_CFLAGS)
AC_SUBST(TIRPC_LIBS)

if test "x$HAVE_TIRPC" != "xtrue" ; then
AC_MSG_ERROR([libtirpc not found, necessary for build on musl with OSC enabled])
fi

fi
fi

dnl prctl
AC_CHECK_HEADERS(sys/prctl.h, HAVE_PRCTL=true, HAVE_PRCTL=false)
AM_CONDITIONAL(HAVE_PRCTL, $HAVE_PRCTL)

dnl js_header
AC_CHECK_HEADERS(linux/joystick.h, HAVE_LINUX_JOYSTICK_H=true, HAVE_LINUX_JOYSTICK_H=false)
AM_CONDITIONAL(HAVE_LINUX_JOYSTICK_H, $HAVE_LINUX_JOYSTICK_H)

HAVE_JACK=false
HAVE_JACK_TRANSPORT=false

AC_ARG_ENABLE(jack, [  --disable-jack     Disable jack support.] , disable_jack=yes)
AC_ARG_ENABLE(jack-transport, [  --disable-jack-transport     Disable jack transport support.] , disable_jack_transport=yes)

if test "x$disable_jack" != "xyes" ; then

dnl check for jack

dnl PKG_CHECK_MODULES(JACK, [jack >= 0.100.0], HAVE_JACK=true, HAVE_JACK=false)

AC_CHECK_LIB(jack,jack_get_client_name,have_jacklib=true,have_jacklib=false)
if test "$have_jacklib" = "true"; then

AC_CHECK_HEADERS(jack/jack.h, HAVE_JACK=true, HAVE_JACK=false)

if test "$HAVE_JACK" != "false"; then
if test "x$disable_jack_transport" != "xyes" ; then

dnl check for jack-transport
AC_CHECK_HEADERS(jack/transport.h, HAVE_JACK_TRANSPORT=true, HAVE_JACK_TRANSPORT=false)

fi
fi
fi
else
echo not checking for jack
fi

dnl check for jack-video
HAVE_JACK_VIDEO=false
AC_ARG_ENABLE(vjack,
[  --enable-vjack            Enable video jack integration. Experimental. ],
[
AC_DEFINE_UNQUOTED(ENABLE_VJACK,1, [vjack])
HAVE_JACK_VIDEO=true
]
)
AM_CONDITIONAL(ENABLE_JACK,$HAVE_JACK)
AM_CONDITIONAL(ENABLE_JACK_TRANSPORT,$HAVE_JACK_TRANSPORT)
AM_CONDITIONAL(ENABLE_VJACK,$HAVE_JACK_VIDEO)

PKG_CHECK_MODULES(X11,x11,HAVE_X11=true,HAVE_X11=false)

if test "$HAVE_X11" = "false" ; then
AC_MSG_WARN("X11 libs not found, these are required if building LiVES for X11")
fi

AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

AM_CONDITIONAL(HAVE_X11,$HAVE_X11)

PKG_CHECK_MODULES(WAYLAND,gdk-wayland-3.0,HAVE_WAYLAND=true,HAVE_WAYLAND=false)
AC_SUBST(WAYLAND_CFLAGS)
AC_SUBST(WAYLAND_LIBS)

AM_CONDITIONAL(HAVE_WAYLAND,$HAVE_WAYLAND)

PKG_CHECK_MODULES(XRENDER,xrender,HAVE_XRENDER=true,HAVE_XRENDER=false)
AC_SUBST(XRENDER_CFLAGS)
AC_SUBST(XRENDER_LIBS)

AM_CONDITIONAL(HAVE_XRENDER,$HAVE_XRENDER)

HAVE_GTK_VERSION3=false
GTK310_WARN=false

AC_ARG_ENABLE(gtk3, [  --disable-gtk3     Disable gtk+ 3.x and compile with gtk+ 2.x] , disable_gtk3=yes)

if test "x$disable_gtk3" != "xyes" ; then

echo -n "checking for GTK+ >= 3.0.0... "

PKG_CHECK_EXISTS([gtk+-3.0 >= 3.0.0],[HAVE_GTK_VERSION3=true],[HAVE_GTK_VERSION3=false])

if test "x$HAVE_GTK_VERSION3" = "xfalse"; then
echo "no"
else
PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 3.0.0)
fi

else

echo "Not checking for gtk+ 3.x"

fi

if test "x$HAVE_GTK_VERSION3" = "xfalse"; then
echo -n "checking for GTK+ >= 2.4.0... "
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.4.0)
PKG_CHECK_EXISTS([gtk+-2.0 >= 2.16.0],[HAVE_GTK_VERSION216=true],[HAVE_GTK_VERSION216=false])
PKG_CHECK_EXISTS([gtk+-2.0 >= 2.6.0],[HAVE_GTK_VERSION206=true],[HAVE_GTK_VERSION206=false])

echo "- Compiling with gtk+ 2.x -"

else
GTK3_VERSION=`$PKG_CONFIG --modversion gtk+-3.0`

echo "- Compiling with gtk+ version " $GTK3_VERSION " -"

PKG_CHECK_EXISTS([gtk+-3.0 >= 3.10.0],[HAVE_GTK_VERSION310=true],[HAVE_GTK_VERSION310=false])
PKG_CHECK_EXISTS([gtk+-3.0 >= 3.11.0],[HAVE_GTK_VERSION311=true],[HAVE_GTK_VERSION311=false])

HAVE_GTK_VERSION216=true
HAVE_GTK_VERSION206=true

if test "$HAVE_GTK_VERSION310" = "true"; then
if test "$HAVE_GTK_VERSION311" = "false"; then
GTK310_WARN=true
fi
fi

fi

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

GTK_LIBS_NODYNAMIC=`echo "$GTK_LIBS" | sed 's/-Wl,--export-dynamic//g'`
AC_SUBST(GTK_LIBS_NODYNAMIC)

PKG_CHECK_EXISTS([glib-2.0 >= 2.14.0],[HAVE_GLIB_VERSION214=true],[HAVE_GLIB_VERSION214=false])
PKG_CHECK_EXISTS([glib-2.0 >= 2.4.0],[HAVE_GLIB_VERSION24=true],[HAVE_GLIB_VERSION24=false])

DISABLE_GLSLICE=true
AC_ARG_ENABLE(glib-slice-allocator, [  --disable-glib-slice-allocator           Disable the glib slice allocator (not recommended).] , disable_glslice=yes)
if test "x$disable_glslice" != "xyes" ; then
DISABLE_GLSLICE=false
if test "x$HAVE_GLIB_VERSION24" = "xfalse"; then
echo "***********************************************"
echo "LiVES requires glib-2.0 version 2.4.0 or higher"
echo "version 2.10.0 or higher is strongly recommended"
echo "***********************************************"
else
if test "x$HAVE_GLIB_VERSION214" = "xfalse"; then
echo "***********************************************"
echo "LiVES runs best with glib-2.0 version 2.14.0 or higher"
echo "however, it can be compiled with the current version of glib-2.0"
echo "for versions < 2.10, use the configure option --disable-glib-slice-allocator"
echo "***********************************************"
fi
fi

if test "x$HAVE_GTK_VERSION216" = "xfalse"; then
echo "***********************************************"
echo "LiVES runs best with gtk+-2.0 version 2.16.0 or higher"
echo "***********************************************"
fi

PKG_CHECK_MODULES(GLIB,glib-2.0 >= 2.14.0)
else
PKG_CHECK_MODULES(GLIB,glib-2.0 >= 2.4.0)
fi

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

AM_CONDITIONAL(DISABLE_GLSLICE,$DISABLE_GLSLICE)

dnl giw
if test "x$HAVE_GTK_VERSION206" = "xfalse"; then
HAVE_GIW=false
else
HAVE_GIW=true
AC_ARG_ENABLE(GIW,
[  --disable-GIW            Disable GTK+ Instrument Widgets.],
[
AC_DEFINE_UNQUOTED(DISABLE_GIW,1, [GIW])
HAVE_GIW=false
]
)
fi

AM_CONDITIONAL(ENABLE_GIW,$HAVE_GIW)

HAVE_SDL1=false
HAVE_SDL2=false
FORCE_SDL=false

AC_ARG_ENABLE(sdl-force, [  --enable-sdl-force             Enable SDL1.x support without checking for it (use --with-sdl-prefix to set the include path).] , force_sdl=yes)

AC_ARG_ENABLE(sdl, [  --disable-sdl     Disable SDL support.] , disable_sdl=yes)
AC_ARG_ENABLE(sdl2, [  --disable-sdl2     Disable SDL2 support.] , disable_sdl2=yes)
AC_ARG_ENABLE(sdl1, [  --disable-sdl1     Disable SDL1 support.] , disable_sdl1=yes)

if test "x$force_sdl" != "xyes" ; then
if test "x$disable_sdl" != "xyes" ; then

dnl SDL2 only for projectM
if test "x$disable_sdl2" != "xyes" ; then
PKG_CHECK_MODULES(SDL2, sdl2, HAVE_SDL2=true, HAVE_SDL2=false)
fi

dnl check for SDL 1.x

if test "x$disable_sdl1" != "xyes" ; then

ifdef([m4_pattern_allow],
[m4_pattern_allow([^AM_PATH_SDL])])

ifdef(["$AM_PATH_SDL"],

[AM_PATH_SDL([1.2],[
HAVE_SDL1=true
SDL_LIBS=`$SDL_CONFIG --libs`
SDL_CFLAGS=`$SDL_CONFIG --cflags`]
,[AC_MSG_WARN([*** Unable to find SDL ***])])],

[
dnl if AM_PATH_SDL fails, try pkg-config
PKG_CHECK_MODULES(SDL, sdl >= 1.2 sdl < 2, HAVE_SDL1=true, HAVE_SDL1=false)

])
fi
else
FORCE_SDL=true
SDL_PREFIX=${with-sdl-prefix}
HAVE_SDL1=true
fi

AC_SUBST(SDL_LIBS)
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_PREFIX)

fi

AC_SUBST(SDL2_LIBS)
AC_SUBST(SDL2_CFLAGS)
AC_SUBST(SDL2_PREFIX)

AM_CONDITIONAL(HAVE_SDL1, $HAVE_SDL1)
AM_CONDITIONAL(HAVE_SDL2, $HAVE_SDL2)
AM_CONDITIONAL(FORCE_SDL, $FORCE_SDL)

AM_CONDITIONAL([HAVE_SDL_ANYVERSION], [test $HAVE_SDL1 = "true" -o  $HAVE_SDL2 = "true"])

HAVE_AVCODEC=false
HAVE_AVFORMAT=false
HAVE_AVUTIL=false
HAVE_SWSCALE=false
HAVE_SWRESAMPLE=false

AC_ARG_ENABLE(ffmpeg, [  --disable-ffmpeg            Disable ffmpeg (avcodec) support.] , disable_ffmpeg=yes)
if test "x$disable_ffmpeg" != "xyes" ; then

dnl check for avcodec, avformat, avutil, swscale

PKG_CHECK_MODULES(LIBAVCODEC,libavcodec >= 53.0.0,HAVE_AVCODEC=true,HAVE_AVCODEC=false)
PKG_CHECK_MODULES(LIBAVFORMAT,libavformat,HAVE_AVFORMAT=true,HAVE_AVFORMAT=false)
PKG_CHECK_MODULES(LIBAVUTIL,libavutil,HAVE_AVUTIL=true,HAVE_AVUTIL=false)
PKG_CHECK_MODULES(LIBSWSCALE,libswscale,HAVE_SWSCALE=true,HAVE_SWSCALE=false)
PKG_CHECK_MODULES(LIBSWRESAMPLE,libswresample,HAVE_SWRESAMPLE=true,HAVE_SWRESAMPLE=false)

fi

AC_SUBST(LIBAVCODEC_CFLAGS)
AC_SUBST(LIBAVCODEC_LIBS)
AC_SUBST(LIBAVFORMAT_CFLAGS)
AC_SUBST(LIBAVFORMAT_LIBS)
AC_SUBST(LIBAVUTIL_CFLAGS)
AC_SUBST(LIBAVUTIL_LIBS)
AC_SUBST(LIBSWSCALE_CFLAGS)
AC_SUBST(LIBSWSCALE_LIBS)
AC_SUBST(LIBSWRESAMPLE_CFLAGS)
AC_SUBST(LIBSWRESAMPLE_LIBS)

AM_CONDITIONAL(HAVE_AVCODEC,$HAVE_AVCODEC)
AM_CONDITIONAL(HAVE_AVFORMAT,$HAVE_AVFORMAT)
AM_CONDITIONAL(HAVE_AVUTIL,$HAVE_AVUTIL)
AM_CONDITIONAL(HAVE_SWSCALE,$HAVE_SWSCALE)
AM_CONDITIONAL(HAVE_SWRESAMPLE,$HAVE_SWRESAMPLE)

HAVE_AVCODEC_OPEN2=false
HAVE_AVCODEC_ALLOC_CONTEXT3=false
HAVE_AVCODEC_GET_NAME=false
HAVE_AV_CODEC_SET_PKT_TIMEBASE=false
HAVE_AVFORMAT_NEW_STREAM=false
HAVE_AVFORMAT_FIND_STREAM_INFO=false
HAVE_AVFORMAT_CLOSE_INPUT=false
HAVE_AVPACKET_UNREF=false
HAVE_AVFRAME_UNREF=false
HAVE_AVFORMAT_INTERNAL_H=false
HAVE_AV_SET_PTS_INFO=false
HAVE_AVPRIV_SET_PTS_INFO=false
HAVE_AV_GET_BITS_PER_SAMPLE=false

if test "$HAVE_AVCODEC" = "true" ; then
AC_CHECK_LIB(avcodec,avcodec_open2,HAVE_AVCODEC_OPEN2=true,HAVE_AVCODEC_OPEN2=false)
AC_CHECK_LIB(avcodec,avcodec_alloc_context3,HAVE_AVCODEC_ALLOC_CONTEXT3=true,HAVE_AVCODEC_ALLOC_COBTEXT3=false)
AC_CHECK_LIB(avcodec,avcodec_get_name,HAVE_AVCODEC_GET_NAME=true,HAVE_AVCODEC_GET_NAME=false)
AC_CHECK_LIB(avcodec,av_codec_set_pkt_timebase,HAVE_AV_CODEC_SET_PKT_TIMEBASE=true,HAVE_AV_CODEC_SET_PKT_TIMEBASE=false)
AC_CHECK_LIB(avcodec,av_packet_unref,HAVE_AVPACKET_UNREF=true,HAVE_AVPACKET_UNREF=false)
AC_CHECK_LIB(avcodec,av_get_bits_per_sample,HAVE_AV_SET_BITS_PER_SAMPLE=true,HAVE_AV_SET_BITS_PER_SAMPLE=false)
fi

if test "$HAVE_AVFORMAT" = "true" ; then
AC_CHECK_LIB(avformat,avformat_new_stream,HAVE_AVFORMAT_NEW_STREAM=true,HAVE_AVFORMAT_NEW_STREAM=false)
AC_CHECK_LIB(avformat,avformat_find_stream_info,HAVE_AVFORMAT_FIND_STREAM_INFO=true,HAVE_AVFORMAT_FIND_STREAM_INFO=false)
AC_CHECK_LIB(avformat,avformat_close_input,HAVE_AVFORMAT_CLOSE_INPUT=true,HAVE_AVFORMAT_CLOSE_INPUT=false)
AC_CHECK_LIB(avformat,av_set_pts_info,HAVE_AV_SET_PTS_INFO=true,HAVE_AV_SET_PTS_INFO=false)

if test "$HAVE_AV_SET_PTS_INFO" = "false"; then
AC_CHECK_LIB(avformat,avpriv_set_pts_info,HAVE_AVPRIV_SET_PTS_INFO=true,HAVE_AVPRIV_SET_PTS_INFO=false)
if test "$HAVE_AV_SET_PTS_INFO" = "true"; then
AC_CHECK_HEADERS(libavformat/internal.h,HAVE_AVFORMAT_INTERNAL_H=true,HAVE_AVFORMAT_INTERNAL_H=false)
fi
fi

fi

if test "$HAVE_AVUTIL" = "true" ; then
AC_CHECK_LIB(avutil,av_frame_unref,HAVE_AVFRAME_UNREF=true,HAVE_AVFRAME_UNREF=false)
fi

AM_CONDITIONAL(HAVE_AVCODEC_OPEN2,$HAVE_AVCODEC_OPEN2)
AM_CONDITIONAL(HAVE_AVCODEC_ALLOC_CONTEXT3,$HAVE_AVCODEC_ALLOC_CONTEXT3)
AM_CONDITIONAL(HAVE_AVCODEC_GET_NAME,$HAVE_AVCODEC_GET_NAME)
AM_CONDITIONAL(HAVE_AV_CODEC_SET_PKT_TIMEBASE,$HAVE_AV_CODEC_SET_PKT_TIMEBASE)
AM_CONDITIONAL(HAVE_AVFORMAT_NEW_STREAM,$HAVE_AVFORMAT_NEW_STREAM)
AM_CONDITIONAL(HAVE_AVPACKET_UNREF,$HAVE_AVPACKET_UNREF)
AM_CONDITIONAL(HAVE_AVFRAME_UNREF,$HAVE_AVFRAME_UNREF)
AM_CONDITIONAL(HAVE_AVFORMAT_FIND_STREAM_INFO,$HAVE_AVFORMAT_FIND_STREAM_INFO)
AM_CONDITIONAL(HAVE_AVFORMAT_CLOSE_INPUT,$HAVE_AVFORMAT_CLOSE_INPUT)
AM_CONDITIONAL(HAVE_AVFORMAT_INTERNAL_H,$HAVE_AVFORMAT_INTERNAL_H)
AM_CONDITIONAL(HAVE_AV_SET_PTS_INFO,$HAVE_AV_SET_PTS_INFO)
AM_CONDITIONAL(HAVE_AV_SET_BITS_PER_SAMPLE,$HAVE_AV_SET_BITS_PER_SAMPLE)
AM_CONDITIONAL(HAVE_AVPRIV_SET_PTS_INFO,$HAVE_AVPRIV_SET_PTS_INFO)

HAVE_OPENCV=false

dnl check for opencv

PKG_CHECK_MODULES(OPENCV, opencv >= 2.3.1, HAVE_OPENCV=true, HAVE_OPENCV=false)

if test "HAVE_OPENCV" = "true" ; then
OPENCV_LIBS_ONLY_L = `pkg-config opencv --libs-only-L`
fi

AC_SUBST(OPENCV_CFLAGS)
AC_SUBST(OPENCV_LIBS_ONLY_L)

AM_CONDITIONAL(HAVE_OPENCV,$HAVE_OPENCV)

HAVE_FFTW3F=false

dnl check for fftw3f

PKG_CHECK_MODULES(FFTW3F, fftw3f, HAVE_FFTW3F=true, HAVE_FFTW3F=false)

AC_SUBST(FFTW3F_CFLAGS)
AC_SUBST(FFTW3F_LIBS)

AM_CONDITIONAL(HAVE_FFTW3F,$HAVE_FFTW3F)

dnl check for libfreenect

HAVE_LIBFREENECT=false
AC_ARG_ENABLE(libfreenect, [  --disable-libfreenect     Disable libfreenect support.] , disable_libfreenect=yes)
if test "x$disable_libfreenect" != "xyes" ; then

PKG_CHECK_MODULES(LIBFREENECT, libfreenect, HAVE_LIBFREENECT=true, HAVE_LIBFREENECT=false)
fi

AC_SUBST(LIBFREENECT_CFLAGS)
AC_SUBST(LIBFREENECT_LIBS)

AM_CONDITIONAL(HAVE_LIBFREENECT,$HAVE_LIBFREENECT)

TURBO=false
AC_ARG_ENABLE(turbo, [  --enable-turbo     Most optimised compile.] , TURBO=true, TURBO=false)
AM_CONDITIONAL(TURBO,$TURBO)

ENABLE_TOONZ=false
AC_ARG_ENABLE(toonz, [  --enable-toonz     Enable plugins ported from toonz. Requires c++11 compiler support and opencv.] , ENABLE_TOONZ=true, ENABLE_TOONZ=false)
AM_CONDITIONAL(ENABLE_TOONZ,$ENABLE_TOONZ)


dnl check for openGL
HAVE_OPENGL=false
AC_ARG_ENABLE(opengl, [  --disable-opengl     Disable openGL support.] , disable_opengl=yes)
if test "x$disable_opengl" != "xyes" ; then

PKG_CHECK_MODULES(GL, gl, HAVE_OPENGL=true, HAVE_OPENGL=false)
fi

AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

AM_CONDITIONAL(HAVE_OPENGL, $HAVE_OPENGL)

dnl check for GLU
PKG_CHECK_MODULES(GLU,glu,HAVE_GLU=true,HAVE_GLU=false)
AC_SUBST(GLU_CFLAGS)
AC_SUBST(GLU_LIBS)

AM_CONDITIONAL(HAVE_GLU,$HAVE_GLU)

dnl check for mjpegtools
HAVE_MJPEGTOOLS=false
AC_ARG_ENABLE(mjpegtools, [  --disable-mjpegtools    Disable mjpegtools (yuv4mpeg) support.] , disable_mjpegtools=yes)
if test "x$disable_mjpegtools" != "xyes" ; then
PKG_CHECK_MODULES(MJPEGTOOLS,mjpegtools >= 1.6.2,HAVE_MJPEGTOOLS=true,HAVE_MJPEGTOOLS=false)
fi

AC_SUBST(MJPEGTOOLS_CFLAGS)
AC_SUBST(MJPEGTOOLS_LIBS)

AM_CONDITIONAL(HAVE_MJPEGTOOLS,$HAVE_MJPEGTOOLS)

dnl check for libpng
HAVE_LIBPNG=false
AC_ARG_ENABLE(libpng, [  --disable-libpng    Disable direct libpng support.] , disable_libpng=yes)
if test "x$disable_libpng" != "xyes" ; then
PKG_CHECK_MODULES(PNG,libpng > 1.2.0,HAVE_LIBPNG=true,HAVE_LIBPNG=false)
fi

AC_SUBST(PNG_CFLAGS)
AC_SUBST(PNG_LIBS)

AM_CONDITIONAL(HAVE_LIBPNG,$HAVE_LIBPNG)

HAVE_ZLIB=false
AC_ARG_ENABLE(libz, [  --disable-libz    Disable libz support.] , disable_libz=yes)
if test "x$disable_libz" != "xyes" ; then

dnl check for libz
PKG_CHECK_MODULES(LIBZ,zlib,HAVE_LIBZ=true,HAVE_LIBZ=false)
fi

AC_SUBST(LIBZ_CFLAGS)
AC_SUBST(LIBZ_LIBS)

AM_CONDITIONAL(CONFIG_ZLIB,$HAVE_LIBZ)

HAVE_BZLIB=false
AC_ARG_ENABLE(libbz, [  --disable-libbz    Disable libbz support.] , disable_libbz=yes)
if test "x$disable_libbz" != "xyes" ; then

dnl check for libz
AC_CHECK_HEADERS(bzlib.h,HAVE_LIBBZ=true,HAVE_LIBBZ=false)

if test "HAVE_LIBBZ" = "true" ; then
LIBBZ_LIBS=-lbz
fi
fi

AC_SUBST(LIBBZ_CFLAGS)
AC_SUBST(LIBBZ_LIBS)

AM_CONDITIONAL(CONFIG_BZLIB,$HAVE_LIBBZ)

HAVE_LADSPA=false
AC_ARG_ENABLE(ladspa, [  --disable-ladspa    Disable LADSPA support.] , disable_ladspa=yes)
if test "x$disable_ladspa" != "xyes" ; then

dnl check for libz
AC_CHECK_HEADERS(ladspa.h,HAVE_LADSPA=true,HAVE_LADSPA=false)
fi

AM_CONDITIONAL(HAVE_LADSPA,$HAVE_LADSPA)

HAVE_UNICAP=false
AC_ARG_ENABLE(unicap, [  --disable-unicap    Disable unicap support.] , disable_unicap=yes)
if test "x$disable_unicap" != "xyes" ; then
dnl check for unicap
PKG_CHECK_MODULES(UNICAP,libunicap,HAVE_UNICAP=true,HAVE_UNICAP=false)
fi

AC_SUBST(UNICAP_CFLAGS)
AC_SUBST(UNICAP_LIBS)

AM_CONDITIONAL(HAVE_UNICAP,$HAVE_UNICAP)

dnl Check of -lv4l1
AC_CHECK_HEADERS(linux/videodev.h, HAVE_LIBv4l1=true, HAVE_LIBv4l1=false)
if test "$HAVE_LIBv4l1" = "true"; then
v4l1_INCFILE=1
else
AC_CHECK_HEADERS(libv4l1-videodev.h, HAVE_LIBv4l1=true, HAVE_LIBv4l1=false)
if test "$HAVE_LIBv4l1" = "true"; then
v4l1_INCFILE=2
fi
fi

AM_CONDITIONAL(HAVE_LIBv4l1,$HAVE_LIBv4l1)
AC_SUBST(v4l1_INCFILE)

AC_CHECK_HEADERS(linux/videodev2.h, HAVE_LIBv4l2=true, HAVE_LIBv4l2=false)
AM_CONDITIONAL(HAVE_LIBv4l2,$HAVE_LIBv4l2)

HAVE_LIBDV=false
dnl check for libdv
PKG_CHECK_MODULES(LIBDV,libdv,HAVE_LIBDV=true,HAVE_LIBDV=false)
AC_SUBST(LIBDV_CFLAGS)
AC_SUBST(LIBDV_LIBS)

AM_CONDITIONAL(HAVE_LIBDV,$HAVE_LIBDV)

HAVE_OGG=false
dnl check for ogg
PKG_CHECK_MODULES(OGG,ogg,HAVE_OGG=true,HAVE_OGG=false)
AC_SUBST(OGG_CFLAGS)
AC_SUBST(OGG_LIBS)

AM_CONDITIONAL(HAVE_OGG,$HAVE_OGG)

HAVE_THEORA=false
dnl check for libtheora
PKG_CHECK_MODULES(THEORA,theora,HAVE_THEORA=true,HAVE_THEORA=false)
AC_SUBST(THEORA_CFLAGS)
AC_SUBST(THEORA_LIBS)

AM_CONDITIONAL(HAVE_THEORA,$HAVE_THEORA)

HAVE_VORBIS=false
dnl check for libvorbis
PKG_CHECK_MODULES(VORBIS,vorbis,HAVE_VORBIS=true,HAVE_VORBIS=false)
AC_SUBST(VORBIS_CFLAGS)
AC_SUBST(VORBIS_LIBS)

AM_CONDITIONAL(HAVE_VORBIS,$HAVE_VORBIS)

HAVE_DIRAC=false
dnl check for dirac
echo "Checking for Dirac (schroedinger)..."
PKG_CHECK_MODULES(DIRAC,schroedinger-1.0,HAVE_DIRAC=true,HAVE_DIRAC=false)
AC_SUBST(DIRAC_CFLAGS)
AC_SUBST(DIRAC_LIBS)

AM_CONDITIONAL(HAVE_DIRAC,$HAVE_DIRAC)

HAVE_ALSA=false
dnl check for libasound
PKG_CHECK_MODULES(ALSA,alsa,HAVE_ALSA=true,HAVE_ALSA=false)
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)

AM_CONDITIONAL(HAVE_ALSA,$HAVE_ALSA)

HAVE_PULSE_AUDIO=false
HAVE_PA_STREAM_BEGIN_WRITE=false
dnl check for pulse-audio
PKG_CHECK_MODULES(PULSE,libpulse >= 0.9.14,HAVE_PULSE_AUDIO=true,HAVE_PULSE_AUDIO=false)
if test "$HAVE_PULSE_AUDIO" = "true" ; then
AC_CHECK_LIB(pulse,pa_stream_begin_write,HAVE_PA_STREAM_BEGIN_WRITE=true,HAVE_PA_STREAM_BEGIN_WRITE=false)
fi

AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)

AM_CONDITIONAL(HAVE_PULSE_AUDIO,$HAVE_PULSE_AUDIO)
AM_CONDITIONAL(HAVE_PA_STREAM_BEGIN_WRITE,$HAVE_PA_STREAM_BEGIN_WRITE)

HAVE_LIBVISUAL=false
AC_ARG_ENABLE(libvisual, [  --disable-libvisual     Disable libvisual support.] , disable_libvisual=yes)
if test "x$disable_libvisual" != "xyes" ; then

dnl check for libvisual
PKG_CHECK_MODULES(LIBVISUAL,libvisual-0.4 >= 0.4.0,HAVE_LIBVISUAL=true,HAVE_LIBVISUAL=false)
if test "x$HAVE_LIBVISUAL" = "xfalse"; then
PKG_CHECK_MODULES(LIBVISUAL,libvisual = 0.2.0,HAVE_LIBVISUAL=true,HAVE_LIBVISUAL=false)
fi
AC_SUBST(LIBVISUAL_CFLAGS)
AC_SUBST(LIBVISUAL_LIBS)
fi

AM_CONDITIONAL(HAVE_LIBVISUAL,$HAVE_LIBVISUAL)

HAVE_PROJECTM=false
AC_ARG_ENABLE(projectM, [  --disable-projectM     Disable projectM support.] , disable_projectM=yes)
if test "x$disable_projectM" != "xyes" ; then

dnl check for projectM
PKG_CHECK_MODULES(LIBPROJECTM, libprojectM, HAVE_PROJECTM=true, HAVE_PROJECTM=false)
AC_SUBST(LIBPROJECTM_CFLAGS)
AC_SUBST(LIBPROJECTM_LIBS)
fi

AM_CONDITIONAL(HAVE_PROJECTM, $HAVE_PROJECTM)

if test "$HAVE_PROJECTM" = "true" ; then
if test "$HAVE_OPENGL" = "false" ; then
AC_MSG_WARN("openGL is required to build projectM")
fi
if test "$HAVE_SDL_ANYVERSION" = "false" ; then
AC_MSG_WARN("SDL1 or SDL2 is required to build projectM")
else
if test "$HAVE_SDL2" = "false" ; then
if test "$HAVE_X11" = "false" ; then
AC_MSG_WARN("X11 is required to build projectM with SDL1")
fi
fi
fi
fi

HAVE_LIBOIL=false
HAVE_LIBORC=false

AC_ARG_ENABLE(liboil, [  --disable-liboil     Disable liboil support.] , disable_liboil=yes)
if test "x$disable_liboil" != "xyes" ; then

dnl check for liboil
PKG_CHECK_MODULES(OIL,liboil-0.3 >= 0.3.9,HAVE_LIBOIL=true,HAVE_LIBOIL=false)
AC_SUBST(OIL_CFLAGS)
AC_SUBST(OIL_LIBS)

if test "$HAVE_LIBOIL" = "false"; then
PKG_CHECK_MODULES(ORC,orc-0.4,HAVE_LIBORC=true,HAVE_LIBORC=false)
AC_SUBST(ORC_CFLAGS)
AC_SUBST(ORC_LIBS)
fi

fi

AM_CONDITIONAL(HAVE_LIBOIL,$HAVE_LIBOIL)
AM_CONDITIONAL(ENABLE_LIBOIL,$HAVE_LIBOIL)

AM_CONDITIONAL(HAVE_LIBORC,$HAVE_LIBORC)
AM_CONDITIONAL(ENABLE_LIBORC,$HAVE_LIBORC)

HAVE_FREI0R=false
AC_ARG_ENABLE(frei0r, [  --disable-frei0r     Disable frei0r support.] , disable_frei0r=yes)
if test "x$disable_freior" != "xyes" ; then

dnl check for frei0r
AC_CHECK_HEADERS(frei0r.h, HAVE_FREI0R=true, HAVE_FREI0R=false)
AM_CONDITIONAL(HAVE_FREI0R,$HAVE_FREI0R)
fi

HAVE_LDVGRAB=false

AC_ARG_ENABLE(ldvgrab, [  --disable-ldvgrab     Disable LiVES-dvgrab.] , disable_ldvgrab=yes)
if test "x$disable_ldvgrab" != "xyes" ; then

dnl check for ldvgrab requirements
AC_CHECK_HEADERS(libraw1394/raw1394.h, HAVE_RAW1394=true, HAVE_RAW1394=false)
if $HAVE_RAW1394  ; then
AC_CHECK_HEADERS(libavc1394/avc1394.h, HAVE_AVC1394=true, HAVE_AVC1394=false)
if $HAVE_AVC1394 ; then
AC_CHECK_HEADERS(libavc1394/rom1394.h, HAVE_ROM1394=true, HAVE_ROM1394=false)
if $HAVE_ROM1394 ; then
HAVE_LDVGRAB=true

fi
fi
fi
fi
AM_CONDITIONAL(ENABLE_LDVGRAB,$HAVE_LDVGRAB)

HAVE_SYSTEM_WEED=false
HAVE_SYSTEM_WEED_SLICE=false
HAVE_SYSTEM_WEED_UTILS=false
HAVE_SYSTEM_WEED_COMPAT=false
HAVE_SYSTEM_WEED_PLUGIN=false

if test "$IS_MINGW" = "false"; then

PKG_CHECK_MODULES(WEED, libweed >= 0.11.0, HAVE_SYSTEM_WEED=true, HAVE_SYSTEM_WEED=false)

if test "$HAVE_SYSTEM_WEED" = "false"; then
echo "...this is not a problem if we have WEED_SLICE"
fi

AC_SUBST(WEED_CFLAGS)
AC_SUBST(WEED_LIBS)

PKG_CHECK_MODULES(WEED_SLICE, libweed_slice >= 0.11.0, HAVE_SYSTEM_WEED_SLICE=true, HAVE_SYSTEM_WEED_SLICE=false)

if test "$HAVE_SYSTEM_WEED_SLICE" = "true"; then
HAVE_SYSTEM_WEED=true
else
echo "...will try to build from included sources"
fi

dnl CFLAGS will be the same
AC_SUBST(WEED_SLICE_LIBS)

PKG_CHECK_MODULES(WEED_UTILS, libweed-utils >= 0.11.0, HAVE_SYSTEM_WEED_UTILS=true, HAVE_SYSTEM_WEED_UTILS=false)

if test "$HAVE_SYSTEM_WEED_UTILS" = "false"; then
echo "...will try to build from included sources"
fi

AC_SUBST(WEED_UTILS_CFLAGS)
AC_SUBST(WEED_UTILS_LIBS)

PKG_CHECK_MODULES(WEED_COMPAT, libweed-compat >= 0.10.0, HAVE_SYSTEM_WEED_COMPAT=true, HAVE_SYSTEM_WEED_COMPAT=false)

AC_SUBST(WEED_COMPAT_CFLAGS)
AC_SUBST(WEED_COMPAT_LIBS)


if test "$HAVE_SYSTEM_WEED_COMPAT" = "false"; then
echo "...will try to build from included sources"
fi

PKG_CHECK_MODULES(WEED_PLUGIN, libweed-plugin >= 0.13.0, HAVE_SYSTEM_WEED_PLUGIN=true, HAVE_SYSTEM_WEED_PLUGIN=false)

AC_SUBST(WEED_PLUGIN_CFLAGS)
AC_SUBST(WEED_PLUGIN_LIBS)

fi

AM_CONDITIONAL(HAVE_SYSTEM_WEED,$HAVE_SYSTEM_WEED)
AM_CONDITIONAL(HAVE_SYSTEM_WEED_SLICE,$HAVE_SYSTEM_WEED_SLICE)
AM_CONDITIONAL(HAVE_SYSTEM_WEED_UTILS,$HAVE_SYSTEM_WEED_UTILS)
AM_CONDITIONAL(HAVE_SYSTEM_WEED_COMPAT,$HAVE_SYSTEM_WEED_COMPAT)
AM_CONDITIONAL(HAVE_SYSTEM_WEED_PLUGIN,$HAVE_SYSTEM_WEED_PLUGIN)

AM_CONDITIONAL(BUILD_EXPERIMENTAL,false)

AC_ARG_ENABLE(liblives, [  --enable-liblives     Build additional liblives static library.] , liblives=yes, liblives=no)

if test "$liblives" = "yes"; then
if test "$disable_glslice" = "yes" ; then
AC_MSG_ERROR("cannot disable glib-slice when building liblives")
fi
fi

AM_CONDITIONAL(BUILD_LIBLIVES, test $liblives != no)

dnl find -name Makefile.am | sed "s/\.\///g" | sed "s/\.am//g" >> configure.in
AC_CONFIG_FILES([
libweed/Makefile
libOSC/Makefile
libOSC/client/Makefile
libOSC/sendOSC/Makefile
po/Makefile.in
Makefile
intl/Makefile
m4/Makefile
src/Makefile
resources/Makefile
lives-plugins/Makefile
lives-plugins/plugins/effects/RFXscripts/Makefile
lives-plugins/plugins/effects/compound/Makefile
lives-plugins/plugins/effects/Makefile
lives-plugins/plugins/encoders/Makefile
lives-plugins/plugins/decoders/Makefile
lives-plugins/plugins/Makefile
lives-plugins/plugins/playback/video/Makefile
lives-plugins/plugins/playback/audiostream/Makefile
lives-plugins/plugins/playback/Makefile
lives-plugins/icons/Makefile
lives-plugins/themes/cutting_room/Makefile
lives-plugins/themes/pinks/Makefile
lives-plugins/themes/camera/Makefile
lives-plugins/themes/editor/Makefile
lives-plugins/themes/crayons/Makefile
lives-plugins/themes/crayons-bright/Makefile
lives-plugins/themes/Makefile
lives-plugins/weed-plugins/Makefile
lives-plugins/weed-plugins/gdk/Makefile
lives-plugins/weed-plugins/cairo/Makefile
lives-plugins/marcos-encoders/Makefile
])
AC_OUTPUT

dnl lives-plugins/weed-plugins/supercollider/Makefile
dnl lives-plugins/weed-plugins/experimental/Makefile

if test "$HAVE_JACK" != "true"; then
echo
echo "*******************************************************************"
echo You are compiling LiVES without jack \(\>= 0.100.0\) support.
echo It is recommended that you install/upgrade libjack-devel 
echo before compiling LiVES.
echo
echo For more details, see: http://jackaudio.org
echo
echo "*******************************************************************"
echo
fi

if test "$HAVE_PULSE_AUDIO" != "true"; then
echo
echo "*******************************************************************"
echo You are compiling LiVES without pulse audio \(\>= 0.9.14\) support.
echo It is recommended that you install/upgrade libpulse-devel 
echo before compiling LiVES.
echo
echo For more details, see: http://www.pulseaudio.org
echo
echo "*******************************************************************"
echo
fi

if test "x$HAVE_GTK_VERSION210" = "xfalse"; then
echo "*******************************************************************"
echo "LiVES will compile with your current version of gtk+-2.0"
echo "however your strongly advised to upgrade to at least version"
echo "2.10.0 of gtk+-2.0 to ensure problem free running of LiVES"
echo "*******************************************************************"
echo
fi

echo "*******************************************************************"
echo "LiVES is now configured and ready for building."
echo "If you are building for MULTILIBS on a 64 bit system, "
echo "you may wish to re-run this, e.g.: ./configure --libdir=/usr/lib64"
echo "In this case you MUST also manually update the library paths in libweed/*.pc files."
echo ""
echo "Remember to do \"sudo ldconfig\" or equivalent after installing."
echo  "*******************************************************************"

