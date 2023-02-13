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

// first some general purpose macros
# define EMPTY(...)
# define DEFER(...) __VA_ARGS__ EMPTY()
# define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()
# define EXPAND(...) __VA_ARGS__

#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define CHECK_N(x, n, ...) n
#define CHECK(...) CHECK_N(__VA_ARGS__, 0,)

#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 ~, 1,

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0

#define BOOL(x) COMPL(NOT(x))

#define IIF(c) PRIMITIVE_CAT(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t

#define IF(c) IIF(BOOL(c))

#define EAT(...)
#define EXPAND(...) __VA_ARGS__
#define WHEN(c) IF(c)(EXPAND, EAT)

//////////////////////

#define HW_ALIGNMENT ((capable && capable->hw.cacheline_size > 0) ? capable->hw.cacheline_size \
		      : DEF_ALIGN)

/* #include <bits/wordsize.h> */
/* #ifdef  __WORDSIZE32_SIZE_ULONG */

#if defined( _WIN32 ) || defined( __WIN32__ ) || defined( _WIN64 )	\
  || defined (__CYGWIN__) || defined (IS_MINGW)
#define LIVES_IS_WINDOWS TRUE
#define LIVES_BOOLEAN_TYPE uint8_t
#define LIVES_INT64_TYPE long long
#define LIVES_UINT64_TYPE unsigned long long
#define LIVES_PID_TYPE int

#ifndef ulong
#define ulong unsigned long long
#endif

#else
#define LIVES_IS_WINDOWS FALSE
#define LIVES_BOOLEAN_TYPE int32_t
#define LIVES_INT64_TYPE  long
#define LIVES_UINT64_TYPE unsigned long
#define LIVES_PID_TYPE pid_t

#ifndef ulong
#define ulong unsigned long
#endif

#endif

#ifndef HAVE_BOOLEAN_TYPE
typedef LIVES_BOOLEAN_TYPE	boolean;
#endif

#ifndef HAVE_PID_TYPE
typedef pid_t lives_pid_t;
#endif

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#endif

#include <stdint.h>
#include <stdarg.h>

#include <inttypes.h>

#undef PRId64
#undef PRIu64

#ifdef IS_MINGW
#define LONGSIZE 32
#else

#ifdef __WORDSIZE
#define LONGSIZE __WORDSIZE
#else
#if defined __x86_64__
# define LONGSIZE	64
#ifndef __WORDSIZE_COMPAT32
# define __WORDSIZE_COMPAT32	1
#endif
#else
# define LONGSIZE	32
#endif // x86
#endif // __WORDSIZE
#endif // mingw

#ifdef __PRI64_PREFIX
#undef __PRI64_PREFIX
#endif

# if LONGSIZE == 64
#  define __PRI64_PREFIX	"l"
# else
#  define __PRI64_PREFIX	"ll"
# endif

#undef PRId64
#undef PRIu64

# define PRId64		__PRI64_PREFIX "d"
# define PRIu64		__PRI64_PREFIX "u"

#define QUOTEME(x) #x
#define QUOTEME_ALL(...) QUOTEME(__VA_ARGS__)
#define PREFIX_IT(A, B) QUOTEME_ALL(A B)

#ifdef __GNUC__
#  define LIVES_WARN_UNUSED  __attribute__((warn_unused_result))
#  define LIVES_ALLOW_UNUSED  __attribute__((unused))
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
#  define LIVES_WARN_UNUSED
#  define LIVES_ALLOW_UNUSED
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

typedef int lives_result_t;

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
#define LIVES_COPYRIGHT_YEARS "2002 - 2023"

#if defined (IS_DARWIN) || defined (IS_FREEBSD)
#ifndef off64_t
#define off64_t off_t
#endif
#ifndef lseek64
#define lseek64 lseek
#endif
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

typedef int64_t ticks_t;
typedef int frames_t; // nb. will chenge to int64_t at some future point
typedef int64_t frames64_t; // will become the new standard
typedef void (*lives_funcptr_t)();

typedef struct {
  uint16_t red,  green, blue;
} lives_colRGB48_t;

typedef struct {
  uint16_t red, green, blue, alpha;
} lives_colRGBA64_t;

typedef enum {
  LIVES_MATCH_UNDEFINED = 0,
  LIVES_MATCH_NEAREST,
  LIVES_MATCH_AT_LEAST,
  LIVES_MATCH_AT_MOST,
  LIVES_MATCH_LOWEST,
  LIVES_MATCH_HIGHEST,
  LIVES_MATCH_CHOICE,
  LIVES_MATCH_SPECIFIED,
  N_MATCH_TYPES,
} lives_match_t;

#define MATCH_TYPE_ENABLED 1
#define MATCH_TYPE_DEFAULT 2

/// delivery types
typedef enum {
  LIVES_DELIVERY_UNDEFINED,
  LIVES_DELIVERY_PULL,
  LIVES_DELIVERY_PUSH,
  LIVES_DELIVERY_PUSH_PULL,
} lives_delivery_t;

typedef struct {
  lives_colRGB48_t fg;
  lives_colRGB48_t bg;
} lives_subtitle_style_t;

typedef enum {
  SUBTITLE_TYPE_NONE = 0,
  SUBTITLE_TYPE_SRT,
  SUBTITLE_TYPE_SUB
} lives_subtitle_type_t;

typedef struct _lives_subtitle_t xlives_subtitle_t;

typedef struct _lives_subtitle_t {
  double start_time;
  double end_time;
  lives_subtitle_style_t *style; ///< for future use
  long textpos;
  xlives_subtitle_t *prev; ///< for future use
  xlives_subtitle_t *next;
} lives_subtitle_t;

typedef struct {
  lives_subtitle_type_t type;
  int tfile;
  char *text;
  lives_subtitle_t *current; ///< pointer to current entry in index
  lives_subtitle_t *first;
  lives_subtitle_t *last;
  int offset; ///< offset in frames (default 0)
  double last_time;
} lives_subtitles_t;

typedef enum {
  LIVES_INTERLACE_NONE = 0,
  LIVES_INTERLACE_BOTTOM_FIRST = 1,
  LIVES_INTERLACE_TOP_FIRST = 2
} lives_interlace_t;

/// this struct is used only when physically resampling frames on the disk
/// we create an array of these and write them to the disk
typedef struct {
  int value;
  int64_t reltime;
} resample_event;

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
  UNDO_NONE = 0,
  UNDO_EFFECT,
  UNDO_RESIZABLE,
  UNDO_MERGE,
  UNDO_RESAMPLE,
  UNDO_TRIM_AUDIO,
  UNDO_TRIM_VIDEO,
  UNDO_CHANGE_SPEED,
  UNDO_AUDIO_RESAMPLE,
  UNDO_APPEND_AUDIO,
  UNDO_INSERT,
  UNDO_CUT,
  UNDO_DELETE,
  UNDO_DELETE_AUDIO,
  UNDO_INSERT_SILENCE,
  UNDO_NEW_AUDIO,

  /// resample/resize and resample audio for encoding
  UNDO_ATOMIC_RESAMPLE_RESIZE,

  /// resample/reorder/resize/apply effects
  UNDO_RENDER,

  UNDO_FADE_AUDIO,
  UNDO_AUDIO_VOL,

  /// record audio to selection
  UNDO_REC_AUDIO,

  UNDO_INSERT_WITH_AUDIO
} lives_undo_t;

typedef enum {
  IMG_TYPE_UNKNOWN = 0,
  IMG_TYPE_JPEG,
  IMG_TYPE_PNG,
  N_IMG_TYPES
} lives_img_type_t;

// global constants
#define MAX_FILES 65535

// OPTIONS settings

#define USE_STD_MEMFUNCS 0

#if !USE_STD_MEMFUNCS
#if ENABLE_ORC
#include <orc/orc.h>
#endif

#if ENABLE_OIL
#include <liboil/liboil.h>
#endif

#define USE_RPMALLOC 1
#define ALLOW_ORC_MEMCPY 1
#define ALLOW_OIL_MEMCPY 1
#endif

#define STD_STRINGFUNCS 1

#define AUTOTUNE_FILEBUFF_SIZES 1
#define AUTOTUNE_MALLOC_SIZES 1

#define WEED_ADVANCED_PALETTES 1
#define ENABLE_DVD_GRAB 1

#define BG_LOAD_RFX 1

#define USE_RESTHREAD 0

#ifdef HAVE_MJPEGTOOLS
#define HAVE_YUV4MPEG		1
#endif

#ifdef __cplusplus
#ifdef HAVE_UNICAP
#undef HAVE_UNICAP
#endif
#endif

#define RECURSE_GUARD_START static pthread_mutex_t recursion_mutex=PTHREAD_MUTEX_INITIALIZER;
#define RETURN_IF_RECURSED do{if(pthread_mutex_trylock(&recursion_mutex))return;}while (0);
#define RETURN_VAL_IF_RECURSED(val)do{if(pthread_mutex_trylock(&recursion_mutex))return val;\
    pthread_mutex_unlock(&recursion_mutex);}while (0);
#define RECURSE_GUARD_LOCK do{pthread_mutex_lock(&recursion_mutex);}while(0);
#define RECURSE_GUARD_END do{pthread_mutex_trylock(&recursion_mutex);pthread_mutex_unlock(&recursion_mutex);}while(0);

/// global (shared) definiitions

#define LIVES_LEAF_THREAD_PARAM "thrd_param"

// testing / experimental
#define VSLICES 1

//#define WEED_STARTUP_TESTS

//#define VALGRIND_ON  ///< define this to ease debugging with valgrind
#ifdef VALGRIND_ON
#define QUICK_EXIT 1
#else
#define USE_REC_RS 1
#endif

#endif
