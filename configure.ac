dnl Process this file with autoconf to produce a configure script.

dnl IMPORTANT !! $version must also be set to exactly the same string in smogrify
AC_INIT([LiVES],[4.0.0-pre],[https://github.com/salsaman/LiVES/issues],[],[http://lives-video.com])

AC_PREFIX_DEFAULT(/usr)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_AUX_DIR([.])

AC_PREREQ([2.69])

ifdef([m4_pattern_allow],
      [m4_pattern_allow([^AC_.*])])

AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CXX

AM_INTL_SUBDIR

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_MKDIR_P

_LT_SET_OPTION([LT_INIT],[dlopen])
AC_DIAGNOSE([obsolete],[AC_LIBTOOL_DLOPEN: Remove this warning and the call to _LT_SET_OPTION when you
put the 'dlopen' option into LT_INIT's first parameter.])

LT_INIT
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_SYS_LARGEFILE

AM_PROG_AS

PKG_PROG_PKG_CONFIG

PTHREAD_OK="false"
ACX_PTHREAD
if test "$THREAD_SUPPORT" = "yes"; then
    PTHREAD_OK="true"
fi

AM_CONDITIONAL(HAVE_PTHREAD, $PTHREAD_OK)

IS_DARWIN="false"
IS_IRIX="false"
IS_SOLARIS="false"
IS_LINUX="false"
IS_LINUX_GNU="false"
IS_LINUX_MUSL="false"
IS_MINGW="false"
IS_FREEBSD="false"

case "{$target_os}" in
 *darwin*)
   IS_DARWIN="true"
  ;;
 *irix*)
   IS_IRIX="true"
  ;;
 *linux-gnu*)
   IS_LINUX="true"
   IS_LINUX_GNU="true"
  ;;
 *linux-musl*)
   IS_LINUX="true"
   IS_LINUX_MUSL="true"
  ;;
 *freebsd*)
   IS_FREEBSD="true"
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
AM_CONDITIONAL(IS_FREEBSD,$IS_FREEBSD)

if test "$IS_MINGW" = "false"; then
AC_CHECK_FILE(lives-plugins/Makefile.am,,[
echo "LiVES plugins were not found. Please make sure you have checked them out."
echo
echo
exit
])
fi

IS_X86_64="false"

case "{$target_cpu}" in
 *x86_64*)
   IS_X86_64="true"
  ;;
 *)
  ;;
esac

AM_CONDITIONAL(IS_X86_64,$IS_X86_64)

GETTEXT_PACKAGE=lives
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [gtp])

AC_DEFINE_UNQUOTED(LOCALEDIR, "${datadir}/locale", [Name of gettext locale directory])

AC_DEFINE_UNQUOTED(PREFIX, "${prefix}", [prefix])
AC_DEFINE_UNQUOTED(LiVES_VERSION, "${VERSION}", [])

dnl Add the languages which your application supports here.
ALL_LINGUAS="ar fr ca cs da ja nl_NL de_DE pt pt_BR hu es it pl tr ru zh_CN sk he en_GB fi et gl uz te ro uk oc hr ug pms"
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.19)
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

dnl Check for getentropy
AC_CHECK_FUNCS(getentropy)

AC_PATH_PROGS(PERL, perl perl5)

dnl check for profiling

AC_ARG_ENABLE(profiling, [  --enable-profiling     Enable profiling with gmon.] , profiling=yes, profiling=no)

AM_CONDITIONAL(ENABLE_PROFILING, test $profiling != no)

dnl Check for pkg-config

AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

if test x$PKG_CONFIG = xno; then
   AC_MSG_ERROR([Cannot find pkg-config, make sure it is installed and in your PATH])
fi

dnl check for libpng

HAVE_LIBPNG=false
PKG_CHECK_MODULES(PNG,libpng > 1.2.0,HAVE_LIBPNG=true,HAVE_LIBPNG=false)
if test "$HAVE_LIBPNG" != "true" ; then
AC_MSG_ERROR("Unable to find libpng > 1.2.0, please install this first, then try again.")
fi

AC_SUBST(PNG_CFLAGS)
AC_SUBST(PNG_LIBS)

AM_CONDITIONAL(HAVE_LIBPNG,$HAVE_LIBPNG)

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
AC_ARG_ENABLE(prctl, [  --disable-prctl     Disable prctl support.] , disable_prctl=yes)
if test "$disable_prctl" != "xyes" ; then
AC_CHECK_HEADERS(sys/prctl.h, HAVE_PRCTL=true, HAVE_PRCTL=false)
fi
AM_CONDITIONAL(HAVE_PRCTL, $HAVE_PRCTL)

dnl js_header
AC_CHECK_HEADERS(linux/joystick.h, HAVE_LINUX_JOYSTICK_H=true, HAVE_LINUX_JOYSTICK_H=false)
AM_CONDITIONAL(HAVE_LINUX_JOYSTICK_H, $HAVE_LINUX_JOYSTICK_H)

HAVE_JACK=false
HAVE_JACK_SERVER=false
HAVE_JACK_TRANSPORT=false
JACK_V2=false

AC_ARG_ENABLE(jack, [  --disable-jack     Disable jack support.] , disable_jack=yes)
AC_ARG_ENABLE(jack-transport, [  --disable-jack-transport     Disable jack transport support.] , disable_jack_transport=yes)

if test "x$disable_jack" != "xyes" ; then

dnl check for jack

PKG_CHECK_MODULES(JACK, [jack >= 0.100.0], HAVE_JACK=true, HAVE_JACK=false)

if test "$HAVE_JACK" = "true" ; then

PKG_CHECK_MODULES(JACK_V2, [jack >= 1.0.0], JACK_V2=true, JACK_V2=false)

if test "$JACK_V2" = "true"; then
JACK_SERVER_LIBS=`$PKG_CONFIG --variable=server_libs jack`

if test "$JACK_SERVER_LIBS" != ""; then
HAVE_JACK_SERVER=true
fi

else
AC_CHECK_HEADERS(jack/control.h, HAVE_JACK_SERVER=true, HAVE_JACK_SERVER=false)
if test "$HAVE_JACK_SERVER" = "true"; then
JACK_SERVER_LIBS=-ljackserver
fi
fi

if test "$HAVE_JACK_SERVER" = "true"; then
echo jack server libs found
else
AC_MSG_ERROR([jack server libraries were not found. These are now required to compile lives with jack.
Please make sure libjackserver-dev is installed and that jack/control.h is present.\n
Alternately you can build lives without jack using the configure option --disable-jack.])
fi

else dnl HAVE_JACK

AC_CHECK_LIB(jack,jack_get_client_name,have_jacklib=true,have_jacklib=false)
if test "$have_jacklib" = "true"; then

AC_CHECK_HEADERS(jack/jack.h, HAVE_JACK=true, HAVE_JACK=false)
AC_CHECK_HEADERS(jack/control.h, HAVE_JACK_SERVER=true, HAVE_JACK_SERVER=false)

JACK_CFLAGS=
JACK_LIBS=-ljack
JACK_SERVER_LIBS=-ljackserver

fi dnl HAVE_JACKLIB
fi dnl HAVE_JACK

if test "$HAVE_JACK" != "false"; then
if test "$HAVE_JACK_SERVER" != "false"; then

AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)
AC_SUBST(JACK_SERVER_LIBS)

if test "x$disable_jack_transport" != "xyes" ; then

dnl check for jack-transport
AC_CHECK_HEADERS(jack/transport.h, HAVE_JACK_TRANSPORT=true, HAVE_JACK_TRANSPORT=false)

fi dnl JACK_TRANS.
fi dnl HAVER_JACK_SERVER
fi dnl HAVE_JACK

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
AM_CONDITIONAL(JACK_V2,$JACK_V2)

if test "$HAVE_JACK" = "true"; then
if test "$JACK_V2" = "false"; then

echo
echo **********************************************************************************************************
echo WARNING: jack1 was found on this system - this version will function perfectly well with LiVES
echo but it lacks some user friendly features which are only available in jack2.
echo For this reason it is recommended to compile LiVES with jack2 in order to provide a better user experience.
echo **********************************************************************************************************
echo

fi
fi

PKG_CHECK_MODULES(X11,x11,HAVE_X11=true,HAVE_X11=false)

if test "$HAVE_X11" = "false" ; then
AC_MSG_WARN("X11 libs not found, these are required if building LiVES for X11")
fi

AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

AM_CONDITIONAL(HAVE_X11,$HAVE_X11)

AC_ARG_ENABLE(wayland, [  --disable-wayland     Disable wayland support.] , disable_wayland=yes)
if test "$disable_wayland" != "xyes" ; then
PKG_CHECK_MODULES(WAYLAND,gdk-wayland-3.0,HAVE_WAYLAND=true,HAVE_WAYLAND=false)
AC_SUBST(WAYLAND_CFLAGS)
AC_SUBST(WAYLAND_LIBS)
fi
AM_CONDITIONAL(HAVE_WAYLAND,$HAVE_WAYLAND)


AC_ARG_ENABLE(xrender, [  --disable-xrender     Disable xrender support.] , disable_xrender=yes)
if test "$disable_xrender" != "xyes" ; then
PKG_CHECK_MODULES(XRENDER,xrender,HAVE_XRENDER=true,HAVE_XRENDER=false)
AC_SUBST(XRENDER_CFLAGS)
AC_SUBST(XRENDER_LIBS)
fi
AM_CONDITIONAL(HAVE_XRENDER,$HAVE_XRENDER)

DISABLE_RPMALLOC=true
AC_ARG_ENABLE(rpmalloc, [  --disable-rpmalloc           Disable rpmalloc memory allocator.] , disable_rpmalloc=yes)
if test "x$disable_rpmalloc" != "xyes" ; then
DISABLE_RPMALLOC=false
echo "built-in rpmalloc memory allocator is enabled"
else
echo "rpmalloc memory allocator features disabled"
fi

AM_CONDITIONAL(DISABLE_RPMALLOC,$DISABLE_RPMALLOC)

HAVE_GTK_VERSION3=false
GTK310_WARN=false

echo -n "checking for GTK+ >= 3.0.0... "

PKG_CHECK_EXISTS([gtk+-3.0 >= 3.0.0],[HAVE_GTK_VERSION3=true],[HAVE_GTK_VERSION3=false])

if test "x$HAVE_GTK_VERSION3" = "xfalse"; then
echo "no"
echo LiVES runs best with gtk+ 3.24, but the development libraries for gtk+ 3 were not found.
echo Depending on your system it might be called something like libgtk-3-dev
echo Please consider installing this package and its dependencies before running configure again.
echo
echo gtk+ 4 support is in preparation, but not yet implemented. This is planned for a future release.
echo
echo LiVES also has limited support for gtk+ 2, which can be enabled by passing the --disable-gtk3 option to configure.
echo However this is provided only as a legacy feature and is not actively maintained.
AC_MSG_ERROR([Bye !])
else
PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 3.0.0)
fi

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

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

GTK_LIBS_NODYNAMIC=`echo "$GTK_LIBS" | sed 's/-Wl,--export-dynamic//g'`
AC_SUBST(GTK_LIBS_NODYNAMIC)

PKG_CHECK_EXISTS([glib-2.0 >= 2.14.0],[HAVE_GLIB_VERSION214=true],[HAVE_GLIB_VERSION214=false])
PKG_CHECK_EXISTS([glib-2.0 >= 2.4.0],[HAVE_GLIB_VERSION24=true],[HAVE_GLIB_VERSION24=false])

if test "x$HAVE_GLIB_VERSION24" = "xfalse"; then
echo "***********************************************"
echo "LiVES requires glib-2.0 version 2.4.0 or higher"
echo "version 2.10.0 or higher is strongly recommended"
else
if test "x$HAVE_GLIB_VERSION214" = "xfalse"; then
echo "***********************************************"
echo "LiVES runs best with glib-2.0 version 2.14.0 or higher"
echo "however, it can be compiled with the current version of glib-2.0"
echo "for versions < 2.10, use the configure option --disable-glib-slice-allocator"
echo "***********************************************"
fi
fi

if test "$HAVE_GTK_VERSION216" = "false"; then
echo "***********************************************"
echo "LiVES runs best with gtk+-2.0 version 2.16.0 or higher"
echo "***********************************************"
fi

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

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
HAVE_AVCODEC_SEND_PACKET=false

if test "$HAVE_AVCODEC" = "true" ; then
AC_CHECK_LIB(avcodec,avcodec_open2,HAVE_AVCODEC_OPEN2=true,HAVE_AVCODEC_OPEN2=false)
AC_CHECK_LIB(avcodec,avcodec_alloc_context3,HAVE_AVCODEC_ALLOC_CONTEXT3=true,HAVE_AVCODEC_ALLOC_COBTEXT3=false)
AC_CHECK_LIB(avcodec,avcodec_get_name,HAVE_AVCODEC_GET_NAME=true,HAVE_AVCODEC_GET_NAME=false)
AC_CHECK_LIB(avcodec,av_codec_set_pkt_timebase,HAVE_AV_CODEC_SET_PKT_TIMEBASE=true,HAVE_AV_CODEC_SET_PKT_TIMEBASE=false)
AC_CHECK_LIB(avcodec,av_packet_unref,HAVE_AVPACKET_UNREF=true,HAVE_AVPACKET_UNREF=false)
AC_CHECK_LIB(avcodec,av_get_bits_per_sample,HAVE_AV_SET_BITS_PER_SAMPLE=true,HAVE_AV_SET_BITS_PER_SAMPLE=false)
AC_CHECK_LIB(avcodec,avcodec_send_packet,HAVE_AVCODEC_SEND_PACKET=true,HAVE_AVCODEC_SEND_PACKET=false)
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
AM_CONDITIONAL(HAVE_AVCODEC_SEND_PACKET,$HAVE_AVCODEC_SEND_PACKET)
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
HAVE_OPENCV4=false

dnl check for opencv

AC_ARG_ENABLE(opencv, [  --disable-opencv     Disable opencv support.] , disable_opencv=yes)
if test "$disable_opencv" != "xyes" ; then

PKG_CHECK_MODULES(OPENCV, opencv >= 2.3.1, HAVE_OPENCV=true, HAVE_OPENCV=false)

if test "$HAVE_OPENCV" = "false" ; then

PKG_CHECK_MODULES(OPENCV4, opencv4, HAVE_OPENCV4=true, HAVE_OPENCV4=false)

if test "$HAVE_OPENCV4" = "true" ; then
OPENCV4_LIBS_ONLY_L=`$PKG_CONFIG opencv4 --libs-only-L`
OPENCV4_LIBS_ONLY_OTHER=`$PKG_CONFIG opencv4 --libs-only-other`
fi

else

if test "$HAVE_OPENCV" = "true" ; then
OPENCV_LIBS_ONLY_L=`$PKG_CONFIG opencv --libs-only-L`
OPENCV_LIBS_ONLY_OTHER=`$PKG_CONFIG opencv --libs-only-other`
fi

fi

AC_SUBST(OPENCV4_CFLAGS)
AC_SUBST(OPENCV4_LIBS_ONLY_L)
AC_SUBST(OPENCV4_LIBS_ONLY_OTHER)

AC_SUBST(OPENCV_CFLAGS)
AC_SUBST(OPENCV_LIBS_ONLY_L)
AC_SUBST(OPENCV_LIBS_ONLY_OTHER)
fi

AM_CONDITIONAL(HAVE_OPENCV4,$HAVE_OPENCV4)
AM_CONDITIONAL(HAVE_OPENCV,$HAVE_OPENCV)

HAVE_FFTW3F=false

dnl check for fftw3f

AC_ARG_ENABLE(fftw3f, [  --disable-fftw3     Disable fftw3 support.] , disable_fftw3=yes)
if test "$disable_fftw3" != "xyes" ; then

PKG_CHECK_MODULES(FFTW3F, fftw3f, HAVE_FFTW3F=true, HAVE_FFTW3F=false)

AC_SUBST(FFTW3F_CFLAGS)
AC_SUBST(FFTW3F_LIBS)
fi

AM_CONDITIONAL(HAVE_FFTW3F,$HAVE_FFTW3F)

dnl check for libfreenect

HAVE_LIBFREENECT=false
AC_ARG_ENABLE(libfreenect, [  --disable-libfreenect     Disable libfreenect support.] , disable_libfreenect=yes)
if test "x$disable_libfreenect" != "xyes" ; then

PKG_CHECK_MODULES(LIBFREENECT, libfreenect, HAVE_LIBFREENECT=true, HAVE_LIBFREENECT=false)
fi

AC_SUBST(LIBFREENECT_CFLAGS)
AC_SUBST(LIBFREENECT_LIBS)

AM_CONDITIONAL(HAVE_LIBFREENECT, $HAVE_LIBFREENECT)

TURBO_CFLAGS=""

AC_ARG_ENABLE(turbo, [  --enable-turbo     Most optimised compile. Do NOT use if making binaries for general distribution.] , TURBO=true, TURBO=false)

if test "$TURBO" = "true" ; then
TURBO_CFLAGS="-Ofast -O3 -march=native -mtune=native -ffast-math -fno-math-errno"
echo "Turbo enabled"
echo TURBO_CFLAGS set to $TURBO_CFLAGS
fi

AC_SUBST(TURBO_CFLAGS)
AM_CONDITIONAL(TURBO, $TURBO)

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

PKG_CHECK_MODULES(GLX, glx, HAVE_GLX=true, HAVE_GLX=false)

AC_SUBST(GLX_CFLAGS)
AC_SUBST(GLX_LIBS)


AM_CONDITIONAL(HAVE_OPENGL, $HAVE_OPENGL)

echo GL libs is $GL_LIBS

dnl check for mjpegtools
HAVE_MJPEGTOOLS=false
AC_ARG_ENABLE(mjpegtools, [  --disable-mjpegtools    Disable mjpegtools (yuv4mpeg) support.] , disable_mjpegtools=yes)
if test "x$disable_mjpegtools" != "xyes" ; then
PKG_CHECK_MODULES(MJPEGTOOLS,mjpegtools >= 1.6.2,HAVE_MJPEGTOOLS=true,HAVE_MJPEGTOOLS=false)
fi

AC_SUBST(MJPEGTOOLS_CFLAGS)
AC_SUBST(MJPEGTOOLS_LIBS)

AM_CONDITIONAL(HAVE_MJPEGTOOLS,$HAVE_MJPEGTOOLS)

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

HAVE_LIBEXPLAIN=false

AC_ARG_ENABLE(explain, [  --disable-libexplain     Disable libexplain support.] , disable_libexplain=yes)
if test "$disable_libexplain" != "xyes" ; then

PKG_CHECK_MODULES(LIBEXPLAIN, libexplain, HAVE_LIBEXPLAIN=true, HAVE_LIBEXPLAIN=false)
AC_SUBST(LIBEXPLAIN_CFLAGS)
AC_SUBST(LIBEXPLAIN_LIBS)
fi

AM_CONDITIONAL(HAVE_LIBEXPLAIN, $HAVE_LIBEXPLAIN)

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

HAVE_LIBDV=false
dnl check for v4l2

AC_ARG_ENABLE(v4l2, [  --disable-v4l2     Disable Video for Linux 2 support.] , disable_v4l2=yes)
if test "$disable_v4l2" != "xyes" ; then

AC_CHECK_HEADERS(linux/videodev2.h, HAVE_LIBv4l2=true, HAVE_LIBv4l2=false)
AM_CONDITIONAL(HAVE_LIBv4l2, $HAVE_LIBv4l2)

fi

HAVE_LIBDV=false
dnl check for libdv

AC_ARG_ENABLE(dv, [  --disable-libdv     Disable libdv support.] , disable_libdv=yes)
if test "$disable_libdv" != "xyes" ; then

PKG_CHECK_MODULES(LIBDV,libdv,HAVE_LIBDV=true,HAVE_LIBDV=false)
AC_SUBST(LIBDV_CFLAGS)
AC_SUBST(LIBDV_LIBS)
fi
AM_CONDITIONAL(HAVE_LIBDV,$HAVE_LIBDV)

HAVE_OGG=false
dnl check for ogg

AC_ARG_ENABLE(ogg, [  --disable-ogg     Disable ogg support.] , disable_ogg=yes)
if test "$disable_ogg" != "xyes" ; then

PKG_CHECK_MODULES(OGG,ogg,HAVE_OGG=true,HAVE_OGG=false)
AC_SUBST(OGG_CFLAGS)
AC_SUBST(OGG_LIBS)
fi

AM_CONDITIONAL(HAVE_OGG,$HAVE_OGG)

HAVE_THEORA=false
dnl check for libtheora

AC_ARG_ENABLE(theora, [  --disable-theora     Disable theora support.] , disable_theora=yes)
if test "$disable_theora" != "xyes" ; then

PKG_CHECK_MODULES(THEORA,theora,HAVE_THEORA=true,HAVE_THEORA=false)
AC_SUBST(THEORA_CFLAGS)
AC_SUBST(THEORA_LIBS)
fi

AM_CONDITIONAL(HAVE_THEORA,$HAVE_THEORA)

HAVE_DIRAC=false

dnl check for dirac

AC_ARG_ENABLE(dirac, [  --disable-dirac     Disable dirac (schroedinger) support.] , disable_dirac=yes)
if test "$disable_dirac" != "xyes" ; then

echo -n "checking for Dirac (schroedinger): "
PKG_CHECK_MODULES(DIRAC,schroedinger-1.0,HAVE_DIRAC=true,HAVE_DIRAC=false)
AC_SUBST(DIRAC_CFLAGS)
AC_SUBST(DIRAC_LIBS)
fi

AM_CONDITIONAL(HAVE_DIRAC,$HAVE_DIRAC)

HAVE_ALSA=false
dnl check for libasound
AC_ARG_ENABLE(alsa, [  --disable-alsa     Disable alsa support.] , disable_alsa=yes)
if test "x$disable_alsa" != "xyes" ; then

PKG_CHECK_MODULES(ALSA,alsa,HAVE_ALSA=true,HAVE_ALSA=false)
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)

else
echo not checking for alsa
fi

AM_CONDITIONAL(HAVE_ALSA,$HAVE_ALSA)

HAVE_PULSE_AUDIO=false
HAVE_PA_STREAM_BEGIN_WRITE=false
dnl check for pulse-audio

AC_ARG_ENABLE(pulse, [  --disable-pulse    Disable pulseaudio support.] , disable_pulse=yes)
if test "x$disable_pulse" != "xyes" ; then

PKG_CHECK_MODULES(PULSE,libpulse >= 0.9.14,HAVE_PULSE_AUDIO=true,HAVE_PULSE_AUDIO=false)
if test "$HAVE_PULSE_AUDIO" = "true" ; then
AC_CHECK_LIB(pulse,pa_stream_begin_write,HAVE_PA_STREAM_BEGIN_WRITE=true,HAVE_PA_STREAM_BEGIN_WRITE=false)
fi

AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)

else
echo not checking for pulse
fi

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
if test "x$disable_frei0r" != "xyes" ; then

dnl check for frei0r
AC_CHECK_HEADERS(frei0r.h, HAVE_FREI0R=true, HAVE_FREI0R=false)
fi

AM_CONDITIONAL(HAVE_FREI0R,$HAVE_FREI0R)

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

ALL_WEED_CPTS="true"

PKG_CHECK_MODULES(WEED, libweed >= 4.0.0, HAVE_LIBWEED=true, HAVE_LIBWEED=false)
if test "$HAVE_LIBWEED" = "false"; then
echo "Required library libweed >= 4.0.0 has not been installed."
ALL_WEED_CPTS="false"
fi


PKG_CHECK_MODULES(WEED_UTILS, libweed-utils >= 4.0.0, HAVE_LIBWEED_UTILS=true, HAVE_LIBWEED_UTILS=false)
if test "$HAVE_LIBWEED_UTILS" = "false"; then
echo "Required library libweed-utils >= 4.0.0 has not been installed."
ALL_WEED_CPTS="false"
fi


PKG_CHECK_MODULES(WEED_HOST_UTILS, libweed-host-utils >= 4.0.0, HAVE_LIBWEED_HOST_UTILS=true, HAVE_LIBWEED_HOST_UTILS=false)
if test "$HAVE_LIBWEED_HOST_UTILS" = "false"; then
echo "Required library libweed-host-utils >= 4.0.0 has not been installed."
ALL_WEED_CPTS="false"
fi


PKG_CHECK_MODULES(WEED_EXTRAS, weed-extras >= 4.0.0, HAVE_WEED_EXTRAS=true, HAVE_WEED_EXTRAS=false)
if test "$HAVE_WEED_EXTRAS" = "false"; then
echo "weed/weed-compat.h or weed/weed-palettes.h appear to be missing or outdated.."
ALL_WEED_CPTS="false"
fi


PKG_CHECK_MODULES(WEED_EFFECT_PLUGIN, weed-effect-plugin >= 4.0.0, HAVE_WEED_EFFECT_PLUGIN=true, HAVE_WEED_EFFECT_PLUGIN=false)
if test "$HAVE_WEED_EFFECT_PLUGIN" = "false"; then
echo "Components for building weed effect plugins appear to be missing or outdated."
ALL_WEED_CPTS="false"
fi


if test "$ALL_WEED_CPTS" = "false"; then
AC_CHECK_FILE(libweed/Makefile.am, [echo "Source subdirectory present; libraries will be built along with main appLication."],
[
AC_MSG_ERROR([Libweed not installed and sourcecode not found. Please install missing libraries and run configure again.])
])

echo "Re-checking with local build enabled"

AC_CONFIG_SUBDIRS([libweed])

old_PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"

export PKG_CONFIG_PATH="./libweed:${PKG_CONFIG_PATH}"

PKG_CHECK_MODULES(WEED, libweed >= 4.0.0, HAVE_LIBWEED=true, HAVE_LIBWEED=false)
if test "$HAVE_LIBWEED" = "false"; then
   export PKG_CONFIG_PATH=$old_PKG_CONFIG_PATH
   AC_MSG_ERROR([Required library libweed >= 4.0.0 still not found. Aborting build])
fi

PKG_CHECK_MODULES(WEED_UTILS, libweed-utils >= 4.0.0, HAVE_LIBWEED_UTILS=true, HAVE_LIBWEED_UTILS=false)
if test "$HAVE_LIBWEED_UTILS" = "false"; then
   export PKG_CONFIG_PATH=$old_PKG_CONFIG_PATH
   AC_MSG_ERROR([Required library libweed-utils >= 4.0.0 still not found. Aborting build])
fi

PKG_CHECK_MODULES(WEED_HOST_UTILS, libweed-host-utils >= 4.0.0, HAVE_LIBWEED_HOST_UTILS=true, HAVE_LIBWEED_HOST_UTILS=false)
if test "$HAVE_LIBWEED_HOST_UTILS" = "false"; then
   export PKG_CONFIG_PATH=$old_PKG_CONFIG_PATH
   AC_MSG_ERROR([Required library libweed-host-utils >= 4.0.0 still not found. Aborting build])
fi

PKG_CHECK_MODULES(WEED_EXTRAS, weed-extras >= 4.0.0, HAVE_WEED_EXTRAS=true, HAVE_WEED_EXTRAS=false)
if test "$HAVE_WEED_EXTRAS" = "false"; then
   export PKG_CONFIG_PATH=$old_PKG_CONFIG_PATH
   AC_MSG_ERROR([Required weed-extras >= 4.0.0 still not found. Aborting build])
fi

PKG_CHECK_MODULES(WEED_EFFECT_PLUGIN, weed-effect-plugin >= 4.0.0, HAVE_WEED_EFFECT_PLUGIN=true, HAVE_WEED_EFFECT_PLUGIN=false)
if test "$HAVE_WEED_EFFECT_PLUGIN" = "false"; then
   export PKG_CONFIG_PATH="$old_PKG_CONFIG_PATH"
   AC_MSG_ERROR([weed-effect-plugin >= 4.0.0 still not found. Aborting build])
fi

export PKG_CONFIG_PATH=$old_PKG_CONFIG_PATH
fi


AC_SUBST(WEED_LIBS)
AC_SUBST(WEED_CFLAGS)

AC_SUBST(WEED_UTILS_CFLAGS)
AC_SUBST(WEED_UTILS_LIBS)

AC_SUBST(WEED_HOST_UTILS_CFLAGS)
AC_SUBST(WEED_HOST_UTILS_LIBS)

AC_SUBST(WEED_EXTRAS_CFLAGS)

AC_SUBST(WEED_EFFECT_PLUGIN_CFLAGS)

AM_CONDITIONAL(BUILD_EXPERIMENTAL, false)

AC_ARG_ENABLE(liblives, [  --enable-liblives     Build additional liblives static library.] , liblives=yes, liblives=no)

AM_CONDITIONAL(BUILD_LIBLIVES, test $liblives != no)

dnl find -name Makefile.am | sed "s/\.\///g" | sed "s/\.am//g" >> configure.in
AC_CONFIG_FILES([
libOSC/Makefile
libOSC/client/Makefile
libOSC/sendOSC/Makefile
po/Makefile.in
Makefile
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
lives-plugins/weed-plugins/scripts/Makefile
lives-plugins/weed-plugins/gdk/Makefile
lives-plugins/weed-plugins/cairo/Makefile
lives-plugins/marcos-encoders/Makefile
])

dnl lives-plugins/weed-plugins/supercollider/Makefile
dnl lives-plugins/weed-plugins/experimental/Makefile

AC_OUTPUT


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
echo
echo
echo
echo
echo  "*******************************************************************"

