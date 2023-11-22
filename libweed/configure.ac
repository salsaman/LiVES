AC_INIT([weed], [4.0.0])
AC_PREREQ(2.57)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_AUX_DIR([.])

ifdef([m4_pattern_allow],
      [m4_pattern_allow([^AC_.*])])

AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE([dist-bzip2])
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CXX

LT_INIT

AC_PREFIX_DEFAULT(/usr)

AC_CONFIG_FILES([Makefile])

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_MKDIR_P

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

AC_OUTPUT

