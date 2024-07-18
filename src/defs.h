// defs.h
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef DEFS_H_
#define _DEFS_H_

//// The aim of this file is to have,, whrere practical,  all #defines in one location
/// Also there good arguments to be made for using #if as opposed to #ifdef
// so having a central point of reference will, help secure this

// debugging
//#define DEBUG_PLANTS 1 // weed_plant_new_host / weed_plant_free_host
//#define DEBUG_FILTER_MUTEXES
//#define DEBUG_REFCOUNT // weed_instance refcounting
//#define DEBUG_LPT_REFS // lives_proc_threa refs / unrefs
//#define DEBUG_WEED

// general purpose macros
#define VA_TO_STRING(func,...) (func(__VA_OPT__(#__VA_ARGS__, __VA_ARGS__,)NULL))
#define DO_CALL(macro,...) macro(__VA_ARGS__)
#define DO_IFARGS(macro, macro2,...) CHOOSE(macro,macro2,__VA_OPT__(1,)0__VA_OPT__(,__VA_ARGS__))
#define CHOOSE(macro,macro2,n,...) CHOOSE_##n(macro,macro2__VA_OPT__(,)__VA_ARGS__)
#define CHOOSE_1(macro,macro2,...) macro(__VA_ARGS__)
#define CHOOSE_0(macro,macro2) macro2
//
// then: LIVES_DO_nn_times(4,2,do_macro) // expand do_macro for all values from 0 to 42
// and: LIVES_DO_n_times(7,do_macro2) // expands to do_macro2 for all values from 0 to 7
// furthermore, CONST_0, CONST_1, etc can themselves be macros, provided there is no recursion, this can go several levels deep
#define LIVES_DO_n_times(n,m) LIVES_DO_FOR_##n(m)
#define LIVES_DO_FOR_0(m)m(0)
#define LIVES_DO_FOR_1(m)m(0) m(1)
#define LIVES_DO_FOR_2(m)m(0) m(1) m(2)
#define LIVES_DO_FOR_3(m)LIVES_DO_FOR_2(m) m(3)
#define LIVES_DO_FOR_4(m)LIVES_DO_FOR_3(m) m(4)
#define LIVES_DO_FOR_5(m)LIVES_DO_FOR_4(m) m(5)
#define LIVES_DO_FOR_6(m)LIVES_DO_FOR_5(m) m(6)
#define LIVES_DO_FOR_7(m)LIVES_DO_FOR_6(m) m(7)
#define LIVES_DO_FOR_8(m)LIVES_DO_FOR_7(m) m(8)
#define LIVES_DO_FOR_9(m)LIVES_DO_FOR_8(m) m(9)

#define LIVES_DO_FOR_x0(x,m)m(x##0)
#define LIVES_DO_FOR_x1(x,m)LIVES_DO_FOR_x0(x,m) m(x##1)
#define LIVES_DO_FOR_x2(x,m)LIVES_DO_FOR_x1(x,m) m(x##2)
#define LIVES_DO_FOR_x3(x,m)LIVES_DO_FOR_x2(x,m) m(x##3)
#define LIVES_DO_FOR_x4(x,m)LIVES_DO_FOR_x3(x,m) m(x##4)
#define LIVES_DO_FOR_x5(x,m)LIVES_DO_FOR_x4(x,m) m(x##5)
#define LIVES_DO_FOR_x6(x,m)LIVES_DO_FOR_x5(x,m) m(x##6)
#define LIVES_DO_FOR_x7(x,m)LIVES_DO_FOR_x6(x,m) m(x##7)
#define LIVES_DO_FOR_x8(x,m)LIVES_DO_FOR_x7(x,m) m(x##8)
#define LIVES_DO_FOR_x9(x,m)LIVES_DO_FOR_x8(x,m) m(x##9)

#define LIVES_DO_FOR_x(a,b,m)LIVES_DO_FOR_x##b(a,m)
#define LIVES_DO_FOR_xx1(b,m)LIVES_DO_n_times(9,m) LIVES_DO_FOR_x(1,9,m)
#define LIVES_DO_FOR_xx2(b,m)LIVES_DO_FOR_xx1(9,m) LIVES_DO_FOR_x(2,b,m)
#define LIVES_DO_FOR_xx3(b,m)LIVES_DO_FOR_xx2(9,m) LIVES_DO_FOR_x(3,b,m)
#define LIVES_DO_FOR_xx4(b,m)LIVES_DO_FOR_xx3(9,m) LIVES_DO_FOR_x(4,b,m)
#define LIVES_DO_FOR_xx5(b,m)LIVES_DO_FOR_xx4(9,m) LIVES_DO_FOR_x(5,b,m)
#define LIVES_DO_FOR_xx6(b,m)LIVES_DO_FOR_xx5(9,m) LIVES_DO_FOR_x(6,b,m)
#define LIVES_DO_FOR_xx7(b,m)LIVES_DO_FOR_xx6(9,m) LIVES_DO_FOR_x(7,b,m)
#define LIVES_DO_FOR_xx8(b,m)LIVES_DO_FOR_xx7(9,m) LIVES_DO_FOR_x(8,b,m)
#define LIVES_DO_FOR_xx9(b,m)LIVES_DO_FOR_xx8(9,m) LIVES_DO_FOR_x(9,b,m)
#define LIVES_DO_nn_times(a,b,m) LIVES_DO_FOR_xx##a(b,m)

//////////////////////

typedef void *livespointer;
typedef const void *livesconstpointer;

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
#define _DW0(...) do {__VA_ARGS__} while(0)

#define _NOTHING_ _DW0(if(0);)
#define _NOTHING_MORE_(...) _NOTHING_

#ifdef __GNUC__
#define INLINE_CODE 1
#define _INLINE_(code) ({code})
#else
#define INLINE_CODE 0
#define _INLINE_(code)  NOTHING
#endif

#define LIVES_RESTRICT __restrict__

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
#if defined(_MSC_VER) && !defined(__clang__)
#  define LIVES_ALWAYS_INLINE inline __forceinline
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
#endif

#if __sig_atomic_t_defined
typedef volatile sig_atomic_t lives_sigatomic;
#else
typedef volatile int lives_sigatomic;
#endif

#define LIVES_RESULT_SUCCESS	1
#define LIVES_RESULT_FAIL	0
#define LIVES_RESULT_FAILED	LIVES_RESULT_FAIL

#define LIVES_RESULT_INVALID	-1
#define LIVES_RESULT_ERROR	-2
#define LIVES_RESULT_CANCELLED	-3
#define LIVES_RESULT_TIMEDOUT	-4
#define LIVES_RESULT_NOPERM	-5
#define LIVES_RESULT_BUSY_RETRY	-6

// return value that is never returned (WARNING: returning this may lead to undefined behaviour)
#define LIVES_RESULT_MU	((lives_result_t) (nan("OM")) / 0.)

typedef int lives_result_t;

typedef lives_result_t(*lives_condfunc_f)(void *);

/// default defs

#define ENABLE_OSC2
#define GUI_GTK

#ifdef GUI_GTK
#define LIVES_PAINTER_IS_CAIRO
#define LIVES_LINGO_IS_PANGO 1
#if LIVES_IS_WINDOWS
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h
>
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
#define LIVES_COPYRIGHT_YEARS "2002 - 2024"

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

#define LIVES_LITTLE_ENDIAN 		0
#define LIVES_BIG_ENDIAN 		1

#define MY_OFFSET(xstruct, field) ((size_t)((char *)&(xstruct)->field-(char *)(xstruct)))

typedef int64_t ticks_t;
typedef int frames_t; // nb. will change to int64_t at some future point
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
  LIVES_DIRECTION_ANTI_CLOCKWISE = -4,
  LIVES_DIRECTION_DOWN = -3,
  LIVES_DIRECTION_LEFT = -2,
  LIVES_DIRECTION_BACKWARD = -1,
  LIVES_DIRECTION_NONE = 0,
  LIVES_DIRECTION_FORWARD,
  LIVES_DIRECTION_RIGHT,
  LIVES_DIRECTION_UP,
  LIVES_DIRECTION_CLOCKWISE,
  //
  LIVES_DIRECTION_RANDOM = 512,
  LIVES_DIRECTION_OTHER = 1024,
} lives_direction_t;

#define LIVES_DIRECTION_REVERSE		LIVES_DIRECTION_BACKWARD

#define LIVES_DIRECTION_OUTWARD		LIVES_DIRECTION_FORWARD
#define LIVES_DIRECTION_INWARD		LIVES_DIRECTION_BACKWARD

#define LIVES_DIRECTION_OUT		LIVES_DIRECTION_FORWARD
#define LIVES_DIRECTION_IN		LIVES_DIRECTION_BACKWARD

#define LIVES_DIRECTION_STOPPED	     	LIVES_DIRECTION_NONE

#define LIVES_NO_DIRECTION		LIVES_DIRECTION_NONE


#define LIVES_DIRECTION_DECREASING	LIVES_DIRECTION_BACKWARD
#define LIVES_DIRECTION_INCREASING	LIVES_DIRECTION_FORWARD

#define LIVES_DIRECTION_DESCENDING	LIVES_DIRECTION_BACKWARD
#define LIVES_DIRECTION_ASCENDING	LIVES_DIRECTION_FORWARD

#define LIVES_DIRECTION_LESS		LIVES_DIRECTION_BACKWARD
#define LIVES_DIRECTION_MORE		LIVES_DIRECTION_FORWARD

#define LIVES_DIRECTION_LOWER		LIVES_DIRECTION_BACKWARD
#define LIVES_DIRECTION_HIGHER		LIVES_DIRECTION_FORWARD

#define LIVES_DIRECTION_SLOWER		LIVES_DIRECTION_BACKWARD
#define LIVES_DIRECTION_FASTER		LIVES_DIRECTION_FORWARD

#define LIVES_OFF			0
#define LIVES_ON			1

#define LIVES_INPUT			1
#define LIVES_OUTPUT			2
#define LIVES_INPUT_OUTPUT		3

#define LIVES_DIRECTION_FWD_OR_REV(dir) ((dir) == LIVES_DIRECTION_BACKWARD ? LIVES_DIRECTION_REVERSE : (dir))

/// LIVES_DIRECTION_REVERSE or LIVES_DIRECTION_FORWARD
#define LIVES_DIRECTION_SIG(dir) ((lives_direction_t)sigi((dir)))

/// LIVES_DIRECTION_BACKWARD or LIVES_DIRECTION_FORWARD
#define LIVES_DIRECTION_PAR(dir) ((lives_direction_t)((abs(dir)) & 1))

#define LIVES_DIRECTION_CONTRARY(dir1, dir2)				\
  (((dir1) == LIVES_DIR_BACKWARD || (dir1) == LIVES_DIR_REVERSED)	\
   ? (dir2) == LIVES_DIR_FORWARD :					\
   ((dir2) == LIVES_DIR_BACKWARD || (dir2) == LIVES_DIR_REVERSED)	\
   ? (dir1) == LIVES_DIR_FORWARD : (dir1) == LIVES_DIR_LEFT ? (dir2) == LIVES_DIR_RIGHT \
   : (dir1) == LIVES_DIR_RIGHT ? (dir2) == LIVES_DIR_LEFT : (dir1) == LIVES_DIR_UP \
   ? (dir2) == LIVES_DIR_DOWN : (dir1) == LIVES_DIR_DOWN ? (dir2) == LIVES_DIR_UP \
   : (dir1) == LIVES_DIR_IN ? (dir2) == LIVES_DIR_OUT : (dir1) == LIVES_DIR_OUT \
   ? (dir2) == LIVES_DIR_IN : sig(dir1) != sig(dir2))

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
#define MEM_USE_BIGBLOCKS 1
#define MEM_USE_SMALLBLOCKS 0

#define ALLOW_ORC_MEMCPY 1
#define ALLOW_OIL_MEMCPY 1
#endif

#define STD_STRINGFUNCS 0

#define AUTOTUNE_FILEBUFF_SIZES 1
#define AUTOTUNE_MALLOC_SIZES 1

#define ENABLE_DVD_GRAB 1

#define BG_LOAD_RFX 1

#define DEBUG_FUNC_ENTER_EXIT		(1 << 0)
#define DEBUG_PLAN_RUNNER		(1 << 1)
#define DEBUG_CONDITIONALS		(1 << 2)

#ifdef HAVE_MJPEGTOOLS
#define HAVE_YUV4MPEG		1
#endif

#ifdef __cplusplus
#ifdef HAVE_UNICAP
#undef HAVE_UNICAP
#endif
#endif

/// global (shared) definitions

#define LIVES_LEAF_THREAD_PARAM "thrd_param"
#define LIVES_LEAF_MD5SUM "md5sum"
#define LIVES_LEAF_MD5_CHKSIZE "md5_chksize"

// TODO WEED_LEAF -> LIVES_LEAF
#define LIVES_LEAF_TYPE "lives_type"
#define LIVES_LEAF_SUBTYPE "subtype"
//
#define WEED_LEAF_HOST_DEINTERLACE "host_deint" // frame needs deinterlacing
#define WEED_LEAF_HOST_TC "host_tc" // timecode for deinterlace

// testing / experimental
#define VSLICES 1

#define WEED_STARTUP_TESTS 0

//#define VALGRIND_ON  ///< define this to ease debugging with valgrind
#ifdef VALGRIND_ON
#define QUICK_EXIT 1
#else
#define USE_REC_RS 1
#endif

#define BILLIONS(n) ((int64_t)(n) * 1000000000)
#define ONE_BILLION BILLIONS(1)
#define MILLIONS(n) ((int64_t)(n) * 1000000)
#define ONE_MILLION MILLIONS(1)

#define BILLIONS_DBL(n) ((double)(n) * 1000000000.)
#define ONE_BILLION_DBL BILLIONS_DBL(1.)
#define MILLIONS_DBL(n) ((double)(n) * 1000000.)
#define ONE_MILLION_DBL MILLIONS_DBL(1.)

#define _LIVES_ASSERT(cond, fmt, ...) _DW0(if(!(cond)){BREAK_ME("assert failed");\
      lives_assert_failed(#cond, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__);})
#define LIVES_ASSERT(cond, ...) _LIVES_ASSERT(cond __VA_OPT__(,) __VA_ARGS__, NULL)

#define APPLY_BIT_TRANSFORMS(from, to, input, output) APPLY_BIT_X_##from##_##to(input, output)

#endif
