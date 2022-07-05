// defs.h
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef DEFS_H
#define _DEFS_H 1

//// The aim of this file is to have,, whrere practical,  all #defines in one location
/// Also there good arguments to be made for using #if as opposed to #ifdef to having a central point of refernce will
// help to ensure this

#if !USE_STD_MEMFUNCS
#define USE_RPMALLOC 1
#endif

#define HW_ALIGNMENT ((capable && capable->hw.cacheline_size > 0) ? capable->hw.cacheline_size \
		      : DEF_ALIGN)

#if defined( _WIN32 ) || defined( __WIN32__ ) || defined( _WIN64 )	\
  || defined (__CYGWIN__) || defined (IS_MINGW)
#define LIVES_IS_WINDOWS TRUE
#define LIVES_BOOLEAN_TYPE uint8_t
#define LIVES_INT64_TYPE long long int
#define LIVES_UINT64_TYPE unsigned long long int
#define LIVES_PID_TYPE int

#else
#define LIVES_IS_WINDOWS FALSE
#define LIVES_BOOLEAN_TYPE int32_t
#define LIVES_INT64_TYPE  long int
#define LIVES_UINT64_TYPE unsigned long int
#define LIVES_PID_TYPE ipid_t
#endif

#ifndef HAVE_BOOLEAN_TYPE
typedef LIVES_BOOLEAN_TYPE	boolean;
#endif

#ifndef HAVE_PID_TYPE
typedef pid_t lives_pid_t;
#endif

#ifndef ulong
#define ulong unsigned long
#endif

typedef int64_t ticks_t;

typedef int frames_t; // nb. will chenge to int64_t at some future point
typedef int64_t frames64_t; // will become the new standard

typedef void (*lives_funcptr_t)();

#define QUOTEME(x) #x
#define QUOTEME_ALL(...) QUOTEME(__VA_ARGS__)
#define PREFIX_IT(A, B) QUOTEME_ALL(A B)

//#define WEED_STARTUP_TESTS
#define STD_STRINGFUNCS

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#  define LIVES_PURE  __attribute__((pure))
#  define LIVES_DEPRE1CATED(msg)  __attribute__((deprecated(msg)))
#  define LIVES_CONST  __attribute__((const))
#  define LIVES_MALLOC  __attribute__((malloc))
#  define LIVES_MALLOC_SIZE(argx) __attribute__((alloc_size(argx)))
#  define LIVES_MALLOC_SIZE2(argx, argy) __attribute__((alloc_size(argx, argy)))
#  define LIVES_ALIGN(argx) __attribute__((alloc_align(argx)))
#  define LIVES_ALIGNED(sizex) __attribute__((assume_aligned(sizex)))
#  define LIVES_NORETURN __attribute__((noreturn))
#  define LIVES_ALWAYS_INLINE inline __attribute__((always_inline))
#  define LIVES_NEVER_INLINE __attribute__((noipa)) __attribute__((optimize(0)))
#  define LIVES_FLATTEN  __attribute__((flatten)) // inline all function calls
#  define LIVES_HOT  __attribute__((hot))
#  define LIVES_SENTINEL  __attribute__((sentinel))
#  define LIVES_RETURNS_TWICE  __attribute__((returns_twice))
#  define LIVES_IGNORE_DEPRECATIONS G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#  define LIVES_IGNORE_DEPRECATIONS_END G_GNUC_END_IGNORE_DEPRECATIONS
#else
#  define WARN_UNUSED
#  define LIVES_PURE
#  define LIVES_CONST
#  define LIVES_MALLOC
#  define LIVES_MALLOC_SIZE(x)
#  define LIVES_MALLOC_SIZE2(x, y)
#  define LIVES_DEPRECATED(msg)
#  define LIVES_ALIGN(x)
#  define LIVES_ALIGNED(x)
#  define LIVES_NORETURN
#  define LIVES_ALWAYS_INLINE
#  define LIVES_NEVER_INLINE
#  define LIVES_FLATTEN
#  define LIVES_HOT
#  define LIVES_SENTINEL
#  define LIVES_RETURNS_TWICE
#  define LIVES_IGNORE_DEPRECATIONS
#  define LIVES_IGNORE_DEPRECATIONS_END
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for "environ"
#endif

#define LIVES_RESULT_SUCCESS	1
#define LIVES_RESULT_FAIL	0
#define LIVES_RESULT_ERROR	-1

/// default defs

#define ENABLE_OSC2
#define GUI_GTK
#define LIVES_PAINTER_IS_CAIRO
#define LIVES_LINGO_IS_PANGO

#ifdef GUI_GTK
#if LIVES_IS_WINDOWS
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#ifndef GDK_IS_WIN32_DISPLAY
#define GDK_IS_WIN32_DISPLAY(display) (TRUE)
#endif
#endif //window_win32
#else //win

#ifndef GDK_WINDOWING_X11
#define GDK_WINDOWING_X11
#endif
#endif // win


#define USE_GLIB
#define LIVES_OS_UNIX G_OS_UNIX

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if GTK_CHECK_VERSION(3, 0, 0)
#ifdef ENABLE_GIW
#define ENABLE_GIW_3
#endif
#else
#undef ENABLE_GIW_3
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
// borked in < 3.0
#undef HAVE_WAYLAND
#endif

#ifdef HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#ifndef GDK_IS_WAYLAND_DISPLAY
#define GDK_IS_WAYLAND_DISPLAY(a) FALSE
#endif
#endif
#endif // gtk

#include <sys/stat.h>

#ifndef PREFIX_DEFAULT
#define PREFIX_DEFAULT "/usr"
#endif

/// if --prefix= was not set, this is set to "NONE"
#ifndef PREFIX
#define PREFIX PREFIX_DEFAULT
#endif

#define LIVES_DIR_SEP "/"
#define LIVES_COPYRIGHT_YEARS "2002 - 2022"

#if defined (IS_DARWIN) || defined (IS_FREEBSD)
#ifndef off64_t
#define off64_t off_t
#endif
#ifndef lseek64
#define lseek64 lseek
#endif
#endif

#if ENABLE_ORC
#include <orc/orc.h>
#endif

#if ENABLE_OIL
#include <liboil/liboil.h>
#endif

#ifndef IS_SOLARIS
#define LIVES_INLINE static inline
#define LIVES_GLOBAL_INLINE inline
#else
#define LIVES_INLINE static
#define LIVES_GLOBAL_INLINE
#define LIVES_LOCAL_INLINE
#endif

#define LIVES_LOCAL_INLINE LIVES_INLINE

#include <limits.h>
#include <float.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

#define URL_MAX 2048

#ifndef NEED_ENDIAN_TEST
#include <endian.h>
#endif

#ifdef _ENDIAN_H
#define IS_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)
#ifdef NEED_ENDIAN_TEST
#undef NEED_ENDIAN_TEST
#endif
#define NEED_ENDIAN_TEST 0
#else
#ifndef NEED_ENDIAN_TEST
#define NEED_ENDIAN_TEST 2
#endif
#endif

#if NEED_ENDIAN_TEST
#define  NEED_ENDIAN_TEST 0
static const int32_t testint = 0x12345678;
#define IS_BIG_ENDIAN (((char *)&testint)[0] == 0x12)  // runtime test only !
#endif

// GLOBAL VALUES
#define MAX_FILES 65535


// OPTIONS

#define AUTOTUNE_FILEBUFF_SIZES 1
#define AUTOTUNE_MALLOC_SIZES 1

#define WEED_ADVANCED_PALETTES 1
#define ENABLE_DVD_GRAB 1

#ifdef HAVE_MJPEGTOOLS
#define HAVE_YUV4MPEG		1
#endif

#ifdef __cplusplus
#ifdef HAVE_UNICAP
#undef HAVE_UNICAP
#endif
#endif

// directions
/// use REVERSE / FORWARD when a sign is used, BACKWARD / FORWARD when a parity is used
typedef enum {
  LIVES_DIRECTION_REVERSE = -1,
  LIVES_DIRECTION_BACKWARD,
  LIVES_DIRECTION_FORWARD,
  LIVES_DIRECTION_LEFT,
  LIVES_DIRECTION_RIGHT,
  LIVES_DIRECTION_UP,
  LIVES_DIRECTION_DOWN,
  LIVES_DIRECTION_IN,
  LIVES_DIRECTION_OUT,
  LIVES_DIRECTION_UNKNOWN,
  LIVES_DIRECTION_STOPPED,
  LIVES_DIRECTION_CYCLIC,
  LIVES_DIRECTION_RANDOM,
  LIVES_DIRECTION_OTHER,
} lives_direction_t;

#define LIVES_DIRECTION_NONE 0

typedef enum {
  IMG_TYPE_UNKNOWN = 0,
  IMG_TYPE_JPEG,
  IMG_TYPE_PNG,
  N_IMG_TYPES
} lives_img_type_t;

typedef struct {
  uint16_t red,  green, blue;
} lives_colRGB48_t;

typedef struct {
  uint16_t red, green, blue, alpha;
} lives_colRGBA64_t;

#endif
