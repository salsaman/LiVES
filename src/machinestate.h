// machinestate.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

/// TODO - split into: memory, files, sysops, threads

#ifndef _MACHINESTATE_H_
#define _MACHINESTATE_H_

#include <sys/time.h>
#include <time.h>

#define DEF_ALIGN 64
#define EXTRA_BYTES 64

typedef void *(*malloc_f)(size_t);
typedef void (*free_f)(void *);
typedef void *(*free_and_return_f)(void *); // like free() but returns NULL
typedef void *(*memcpy_f)(void *, const void *, size_t);
typedef int (*memcmp_f)(const void *, const void *, size_t);
typedef void *(*memset_f)(void *, int, size_t);
typedef void *(*memmove_f)(void *, const void *, size_t);
typedef void *(*realloc_f)(void *, size_t);
typedef void *(*calloc_f)(size_t, size_t);
typedef void *(*malloc_and_copy_f)(size_t, const void *);
typedef void (*unmalloc_and_copy_f)(size_t, void *);

#define USE_RPMALLOC

#ifdef USE_RPMALLOC
#include "rpmalloc.h"
#endif

boolean init_memfuncs(void);

void lives_free_check(void *p);

#ifdef USE_RPMALLOC
void *quick_calloc(size_t n, size_t s);
void quick_free(void *p);
#endif

#ifndef lives_malloc
#define lives_malloc malloc
#endif
#ifndef lives_realloc
#define lives_realloc realloc
#endif
#ifndef lives_free
#define lives_free free
#endif
#ifndef lives_memcpy
#define lives_memcpy memcpy
#endif
#ifndef lives_memcmp
#define lives_memcmp memcmp
#endif
#ifndef lives_memset
#define lives_memset memset
#endif
#ifndef lives_memmove
#define lives_memmove memmove
#endif
#ifndef lives_calloc
#define lives_calloc calloc
#endif

#ifdef _lives_malloc
#undef _lives_malloc
#endif
#ifdef _lives_malloc_and_copy
#undef _lives_malloc_and_copy
#endif
#ifdef _lives_realloc
#undef _lives_realloc
#endif
#ifdef _lives_free
#undef _lives_free
#endif
#ifdef _lives_memcpy
#undef _lives_memcpy
#endif
#ifdef _lives_memcmp
#undef _lives_memcmp
#endif
#ifdef _lives_memset
#undef _lives_memset
#endif
#ifdef _lives_memmove
#undef _lives_memmove
#endif
#ifdef _lives_calloc
#undef _lives_calloc
#endif

#ifndef USE_STD_MEMFUNCS
// here we can define optimised mem ory functions to used by setting the symbols _lives_malloc, _lives_free, etc.
// at the end of the header we check if the values have been set and update lives_malloc from _lives_malloc, etc.
// the same values are passed into realtime fx plugins via Weed function overloading
#ifdef HAVE_OPENCV
#ifndef NO_OPENCV_MEMFUNCS
#define _lives_malloc(sz)  alignPtr(sz, DEF_ALIGN);
#define _lives_free    fastFree
#define _lives_realloc proxy_realloc
#endif
#endif

#ifndef __cplusplus

#ifdef ENABLE_ORC
#ifndef NO_ORC_MEMFUNCS
#define _lives_memcpy lives_orc_memcpy
#endif
#endif

#else

#ifdef ENABLE_OIL
#ifndef NO_OIL_MEMFUNCS
#define _lives_memcpy(dest, src, n) {if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) { \
      oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);		\
      return dest;\							\
    }
#endif
#endif

#endif // __cplusplus
#endif // USE_STD_MEMFUNCS

#define __STDC_FORMAT_MACROS
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

/// TODO: this file should be split into at least: memory functions, thread functions, file utils

void *lives_free_and_return(void *p);
void *lives_calloc_safety(size_t nmemb, size_t xsize) GNU_ALIGNED(DEF_ALIGN);
void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize) GNU_ALIGNED(DEF_ALIGN);

size_t get_max_align(size_t req_size, size_t align_max);

/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN = 0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OVER_QUOTA,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;

//void shoatend(void);

#define WEED_LEAF_MD5SUM "md5sum"

// weed plants with type >= 16384 are reserved for custom use, so let's take advantage of that
#define WEED_PLANT_LIVES 31337

#define WEED_LEAF_LIVES_SUBTYPE "subtype"
#define WEED_LEAF_LIVES_MESSAGE_STRING "message_string"

#define LIVES_WEED_SUBTYPE_MESSAGE 1
#define LIVES_WEED_SUBTYPE_WIDGET 2
#define LIVES_WEED_SUBTYPE_TUNABLE 3
#define LIVES_WEED_SUBTYPE_PROC_THREAD 4

weed_plant_t *lives_plant_new(int subtype);
weed_plant_t *lives_plant_new_with_index(int subtype, int64_t index);

void *_ext_malloc(size_t n) GNU_MALLOC;
void *_ext_malloc_and_copy(size_t, const void *) GNU_MALLOC_SIZE(1);
void _ext_unmalloc_and_copy(size_t, void *);
void _ext_free(void *);
void *_ext_free_and_return(void *);
void *_ext_memcpy(void *, const void *, size_t);
void *_ext_memset(void *, int, size_t);
void *_ext_memmove(void *, const void *, size_t);
void *_ext_realloc(void *, size_t) GNU_MALLOC_SIZE(2);
void *_ext_calloc(size_t, size_t) GNU_MALLOC_SIZE2(1, 2) GNU_ALIGN(2);

#define OVERRIDE_MEMFUNCS
static void *(*_lsd_memcpy)(void *dest, const void *src, size_t n) = _ext_memcpy;
static void *(*_lsd_memset)(void *s, int c, size_t n) = _ext_memset;
static void (*_lsd_free)(void *ptr) = rpfree;
#define OVERRIDE_CALLOC_ALIGNED
static int _lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) {
  return !memptr ? 0 : (!(*memptr = (rpaligned_calloc)(64, nmemb, size))) ? ENOMEM : 0;
}
#include "lsd.h"
#undef OVERRIDE_MEMFUNCS

#if defined _GNU_SOURCE
#define LIVES_GNU
#define lives_malloc_auto(size) __builtin_alloc(size)
#define lives_malloc_auto_aligned(size, align) __builtin_alloc_with_align(size, align)
#endif

size_t lives_strlen(const char *) GNU_HOT GNU_PURE;
boolean lives_strcmp(const char *, const char *) GNU_HOT GNU_PURE;
boolean lives_strncmp(const char *, const char *, size_t) GNU_HOT GNU_PURE;
char *lives_strdup_quick(const char *s);
int lives_strcmp_ordered(const char *, const char *) GNU_HOT GNU_PURE;
char *lives_concat(char *, char *) GNU_HOT;
char *lives_concat_sep(char *st, const char *sep, char *x);
char *lives_strstop(char *, const char term) GNU_HOT;
int lives_strappend(const char *string, int len, const char *xnew);
const char *lives_strappendf(const char *string, int len, const char *fmt, ...);

void swab2(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void swab4(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void swab8(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void reverse_bytes(char *buff, size_t count, size_t granularity) 	GNU_HOT GNU_FLATTEN;
boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) 	GNU_HOT;

uint64_t nxtval(uint64_t val, uint64_t lim, boolean less);
uint64_t autotune_u64_end(weed_plant_t **tuner, uint64_t val);
void autotune_u64(weed_plant_t *tuner,  uint64_t min, uint64_t max, int ntrials, double cost);

void init_random(void);
void lives_srandom(unsigned int seed);
uint64_t lives_random(void);

uint64_t fastrand(void) GNU_HOT;
void fastrand_add(uint64_t entropy);
double fastrand_dbl(double range);
uint32_t fastrand_int(uint32_t range);

uint64_t gen_unique_id(void);

#ifdef ENABLE_ORC
void *lives_orc_memcpy(void *dest, const void *src, size_t n);
#endif

#ifdef ENABLE_OIL
void *lives_oil_memcpy(void *dest, const void *src, size_t n);
#endif

void *proxy_realloc(void *ptr, size_t new_size);

char *get_md5sum(const char *filename);

char *lives_format_storage_space_string(uint64_t space);
lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, uint64_t *dsval);
uint64_t get_ds_free(const char *dir);
boolean get_ds_used(int64_t *bytes);

ticks_t lives_get_relative_ticks(ticks_t origsecs, ticks_t orignsecs);
ticks_t lives_get_current_ticks(void);
char *lives_datetime(uint64_t secs);

#define lives_nanosleep(nanosec) {struct timespec ts; ts.tv_sec = (uint64_t)nanosec / ONE_BILLION; \
    ts.tv_nsec = (uint64_t)nanosec - ts.tv_sec * ONE_BILLION; while (nanosleep(&ts, &ts) == -1 && \
								     errno != ETIMEDOUT);}

#define lives_nanosleep_until_nonzero(var) {while (!(var)) lives_nanosleep(1000);}

int check_dev_busy(char *devstr);

uint64_t get_file_size(int fd);
uint64_t sget_file_size(const char *name);

void reget_afilesize(int fileno);
uint64_t reget_afilesize_inner(int fileno);

boolean compress_all_in_dir(const char *dir, int method, void *data);

#define EXTRA_DETAILS_EMPTY_DIR			(1 << 0)
#define EXTRA_DETAILS_CLIPHDR			(1 << 1)
#define EXTRA_DETAILS_MD5			(1 << 2)

typedef struct {
  ///< if we can retrieve some kind of uinque id, we set it here
  ///< may be useful in future for dictionary lookups
  uint64_t uniq;
  lives_struct_def_t *lsd;
  char *name;
  uint64_t type; /// e.g. LIVES_FILE_TYPE_FILE
  off_t size; // -1 not checked, -2 unreadable
  uint64_t blk_size;
  uint64_t atime_sec;
  uint64_t atime_nsec;
  uint64_t mtime_sec;
  uint64_t mtime_nsec;
  uint64_t ctime_sec;
  uint64_t ctime_nsec;
  char *md5sum; /// only filled if EXTRA_DETAILS_MD5 is set, otherwis NULL
  char *extra_details;  /// intialized to NULL, set to at least ""
  LiVESWidget *widgets[16]; ///< caller set widgets for presentation, e.g. labels, spinners. final entry must be followed by NULL
} lives_file_dets_t;

#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what);
#endif

uint32_t lives_string_hash(const char *string) GNU_PURE GNU_HOT;
uint32_t fast_hash(const char *key) GNU_PURE GNU_HOT;
size_t lives_chomp(char *string);

int check_for_bad_ffmpeg(void);

void update_effort(int nthings, boolean badthings);
void reset_effort(void);

//// threadpool API

typedef void *(*lives_funcptr_t)(void *);
typedef weed_plantptr_t lives_proc_thread_t;

typedef struct {
  lives_proc_thread_t var_tinfo;
  boolean var_com_failed;
  int var_write_failed, var_read_failed;
  boolean var_chdir_failed;
  char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file;
  int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data
  int var_rowstride_alignment_hint;
} lives_threadvars_t;

typedef struct {
  LiVESWidgetContext *ctx;
  int64_t idx;
  lives_threadvars_t vars;
} lives_thread_data_t;

typedef struct {
  lives_funcptr_t func;
  void *arg;
  uint64_t flags;
  volatile uint64_t busy;
  volatile uint64_t done;
  void *ret;
  volatile boolean sync_ready;
} thrd_work_t;

#define WEED_LEAF_NOTIFY "notify"
#define WEED_LEAF_DONE "done"
#define WEED_LEAF_THREADFUNC "tfunction"
#define WEED_LEAF_THREAD_PROCESSING "t_processing"
#define WEED_LEAF_THREAD_CANCELLABLE "t_can_cancel"
#define WEED_LEAF_THREAD_CANCELLED "t_cancelled"
#define WEED_LEAF_RETURN_VALUE "return_value"

#define WEED_LEAF_THREAD_PARAM "thrd_param"
#define _WEED_LEAF_THREAD_PARAM(n) WEED_LEAF_THREAD_PARAM  n
#define WEED_LEAF_THREAD_PARAM0 _WEED_LEAF_THREAD_PARAM("0")
#define WEED_LEAF_THREAD_PARAM1 _WEED_LEAF_THREAD_PARAM("1")
#define WEED_LEAF_THREAD_PARAM2 _WEED_LEAF_THREAD_PARAM("2")

#define LIVES_THRDFLAG_AUTODELETE (1 << 0)
#define LIVES_THRDFLAG_TUNING (1 << 1)
#define LIVES_THRDFLAG_WAIT_SYNC (1 << 2)

typedef LiVESList lives_thread_t;
typedef uint64_t lives_thread_attr_t;

#define LIVES_THRDATTR_AUTODELETE (1 << 0)
#define LIVES_THRDATTR_PRIORITY (1 << 1)
#define LIVES_THRDATTR_WAIT_SYNC (1 << 2)

void lives_threadpool_init(void);
void lives_threadpool_finish(void);
int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t *attr, lives_funcptr_t func, void *arg);
uint64_t lives_thread_join(lives_thread_t work, void **retval);

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _RV_ WEED_LEAF_RETURN_VALUE

typedef uint64_t funcsig_t;

typedef int(*funcptr_int_t)();
typedef double(*funcptr_dbl_t)();
typedef int(*funcptr_bool_t)();
typedef char *(*funcptr_string_t)();
typedef int64_t(*funcptr_int64_t)();
typedef weed_funcptr_t(*funcptr_funcptr_t)();
typedef void *(*funcptr_voidptr_t)();
typedef weed_plant_t(*funcptr_plantptr_t)();

#define GETARG(type, n) WEED_LEAF_GET(info, _WEED_LEAF_THREAD_PARAM(n), type)

#define ARGS1(t1) GETARG(t1, "0")
#define ARGS2(t1, t2) ARGS1(t1), GETARG(t2, "1")
#define ARGS3(t1, t2, t3) ARGS2(t1, t2), GETARG(t3, "2")
#define ARGS4(t1, t2, t3, t4) ARGS3(t1, t2, t3), GETARG(t4, "3")
#define ARGS5(t1, t2, t3, t4, t5) ARGS4(t1, t2, t3, t4), GETARG(t5, "4")
#define ARGS6(t1, t2, t3, t4, t5, t6) ARGS5(t1, t2, t3, t4, t5), GETARG(t6, "5")
#define ARGS7(t1, t2, t3, t4, t5, t6, t7) ARGS6(t1, t2, t3, t4, t5, t6), GETARG(t7, "6")
#define ARGS8(t1, t2, t3, t4, t5, t6, t7, t8) ARGS7(t1, t2, t3, t4, t5, t6, t7), GETARG(t8, "7")
#define ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9) ARGS8(t1, t2, t3, t4, t5, t6, t7. t8), GETARG(t9, "8")
#define ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9), GETARG(t10, "9")

#define CALL_VOID_10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) (*thefunc->func)(ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10))
#define CALL_VOID_9(t1, t2, t3, t4, t5, t6, t7, t8, t9) (*thefunc->func)(ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9))
#define CALL_VOID_8(t1, t2, t3, t4, t5, t6, t7, t8) (*thefunc->func)(ARGS8(t1, t2, t3, t4, t5, t6, t7, t8))
#define CALL_VOID_7(t1, t2, t3, t4, t5, t6, t7) (*thefunc->func)(ARGS7(t1, t2, t3, t4, t5, t6, t7))
#define CALL_VOID_6(t1, t2, t3, t4, t5, t6) (*thefunc->func)(ARGS6(t1, t2, t3, t4, t5, t6))
#define CALL_VOID_5(t1, t2, t3, t4, t5) (*thefunc->func)(ARGS5(t1, t2, t3, t4, t5))
#define CALL_VOID_4(t1, t2, t3, t4) (*thefunc->func)(ARGS4(t1, t2, t3, t4))
#define CALL_VOID_3(t1, t2, t3) (*thefunc->func)(ARGS3(t1, t2, t3))
#define CALL_VOID_2(t1, t2) (*thefunc->func)(ARGS2(t1, t2))
#define CALL_VOID_1(t1) (*thefunc->func)(ARGS1(t1))
#define CALL_VOID_0() (*thefunc->func)()

#define CALL_10(ret, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) weed_set_##ret##_value(info, _RV_, \
										     (*thefunc->func##ret)(ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t19)))
#define CALL_9(ret, t1, t2, t3, t4, t5, t6, t7, t8, t9) weed_set_##ret##_value(info, _RV_, \
									       (*thefunc->func##ret)(ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9)))
#define CALL_8(ret, t1, t2, t3, t4, t5, t6, t7, t8) weed_set_##ret##_value(info, _RV_, \
									   (*thefunc->func##ret)(ARGS8(t1, t2, t3, t4, t5, t6, t7, t7)))
#define CALL_7(ret, t1, t2, t3, t4, t5, t6, t7) weed_set_##ret##_value(info, _RV_, \
								       (*thefunc->func##ret)(ARGS7(t1, t2, t3, t4, t5, t6, t7)))
#define CALL_6(ret, t1, t2, t3, t4, t5, t6) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS6(t1, t2, t3, t4, t5, t6)))
#define CALL_5(ret, t1, t2, t3, t4, t5) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS5(t1, t2, t3, t4, t5)))
#define CALL_4(ret, t1, t2, t3, t4) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS4(t1, t2, t3, t4)))
#define CALL_3(ret, t1, t2, t3) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS3(t1, t2, t3)))
#define CALL_2(ret, t1, t2) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS2(t1, t2)))
#define CALL_1(ret, t1) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS1(t1)))
#define CALL_0(ret) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)())

typedef union {
  weed_funcptr_t func;
  funcptr_int_t funcint;
  funcptr_dbl_t funcdouble;
  funcptr_bool_t funcboolean;
  funcptr_int64_t funcint64;
  funcptr_string_t funcstring;
  funcptr_funcptr_t funcfuncptr;
  funcptr_voidptr_t funcvoidptr;
  funcptr_plantptr_t funcplantptr;
} allfunc_t;

lives_proc_thread_t lives_proc_thread_create(lives_thread_attr_t *, lives_funcptr_t, int return_type, const char *args_fmt,
    ...);
boolean lives_proc_thread_check(lives_proc_thread_t);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_thread_data_t *lives_thread_data_create(uint64_t idx);

#define THREADVAR(var) (get_threadvars()->var_##var)

void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_cancel(lives_proc_thread_t);
boolean lives_proc_thread_cancelled(lives_proc_thread_t);

void lives_proc_thread_sync_ready(lives_proc_thread_t);

// WARNING !! version without a return value will free tinfo !
void lives_proc_thread_join(lives_proc_thread_t);

// with return value should free proc_thread
int lives_proc_thread_join_int(lives_proc_thread_t);
double lives_proc_thread_join_double(lives_proc_thread_t);
int lives_proc_thread_join_boolean(lives_proc_thread_t);
char *lives_proc_thread_join_string(lives_proc_thread_t);
weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t);
void *lives_proc_thread_join_voidptr(lives_proc_thread_t);
weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t) ;
int64_t lives_proc_thread_join_int64(lives_proc_thread_t);

void resubmit_proc_thread(lives_proc_thread_t, lives_thread_attr_t *);

lives_proc_thread_t dir_to_file_details(LiVESList **listp, const char *dir, const char *tsubdir,
                                        const char *orig_loc, uint64_t extra);
///// cmdline
char *grep_in_cmd(const char *cmd, int mstart, int npieces, const char *mphrase, int ridx, int rlen);

/// x11
char *get_wid_for_name(const char *wname);
boolean hide_x11_window(const char *wid);
boolean unhide_x11_window(const char *wid);
boolean activate_x11_window(const char *wid);
boolean show_desktop_panel(void);
boolean hide_desktop_panel(void);
boolean get_x11_visible(const char *wname);

void get_wm_caps(const char *wm_name);

#endif
