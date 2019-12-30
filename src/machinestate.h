// machinestate.hf
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _MACHINESTATE_H_
#define _MACHINESTATE_H_

#include <sys/time.h>
#include <time.h>

#define EXTRA_BYTES 64

typedef void *(*malloc_f)(size_t);
typedef void (*free_f)(void *);
typedef void *(*free_and_return_f)(void *); // like free() but returns NULL
typedef void *(*memcpy_f)(void *, const void *, size_t);
typedef void *(*memset_f)(void *, int, size_t);
typedef void *(*memmove_f)(void *, const void *, size_t);
typedef void *(*realloc_f)(void *, size_t);
typedef void *(*calloc_f)(size_t, size_t);
typedef void *(*malloc_and_copy_f)(size_t, const void *);
typedef void (*unmalloc_and_copy_f)(size_t, void *);

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
#ifdef _lives_memset
#undef _lives_memset
#endif
#ifdef _lives_memmove
#undef _lives_memmove
#endif
#ifdef _lives_calloc
#undef _lives_calloc
#endif

//#define USE_STD_MEMFUNCS
#ifndef USE_STD_MEMFUNCS
// here we can define optimised mem ory functions to used by setting the symbols _lives_malloc, _lives_free, etc.
// at the end of the header we check if the values have been set and update lives_malloc from _lives_malloc, etc.
// the same values are passed into realtime fx plugins via Weed function overloading
#ifdef HAVE_OPENCV
#ifndef NO_OPENCV_MEMFUNCS
#define _lives_malloc  fastMalloc
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

void *lives_calloc_safety(size_t nmemb, size_t xsize);

/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN = 0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;

#define INIT_LOAD_CHECK_COUNT 100 // initial loops to get started
#define N_QUICK_CHECKS 3 // how many times we should run with quick checks after that
#define QUICK_CHECK_TIME .1 // the time in sec.s between quick checks
#define TARGET_CHECK_TIME 1. // how often we should check after that

// ignore variance outside these limits
#define VAR_MAX 1.2
#define VAR_MIN .8
#define ME_DELAY 2.

#define LOAD_SCALING (100000. / ME_DELAY) // scale factor to get reasonable values

#define WEED_SEED_MEMBLOCK 65536

// internal memory allocator
typedef struct memheader {
  unsigned int size;
  struct memheader *next;
  size_t align;
} memheader_t;

void *_ext_malloc(size_t n);
void *_ext_malloc_and_copy(size_t, const void *);
void _ext_unmalloc_and_copy(size_t, void *);
void _ext_free(void *);
void *_ext_free_and_return(void *);
void *_ext_memcpy(void *, const void *, size_t);
void *_ext_memset(void *, int, size_t);
void *_ext_memmove(void *, const void *, size_t);
void *_ext_realloc(void *, size_t);
void *_ext_calloc(size_t, size_t);

// TODO
void quick_free(memheader_t *bp);
void *quick_malloc(size_t alloc_size) GNU_MALLOC;

void init_random(void);
void lives_srandom(unsigned int seed);
uint64_t lives_random(void);

int load_measure_idle(void *data);

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
uint64_t get_fs_free(const char *dir);

ticks_t lives_get_relative_ticks(int64_t origsecs, int64_t origusecs);
ticks_t lives_get_current_ticks(void);
char *lives_datetime(struct timeval *tv);

int check_dev_busy(char *devstr);

uint64_t get_file_size(int fd);
uint64_t sget_file_size(const char *name);

void reget_afilesize(int fileno);

uint64_t reget_afilesize_inner(int fileno);

#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what);
#endif

uint32_t string_hash(const char *string) GNU_PURE;

int check_for_bad_ffmpeg(void);

#endif

typedef void *(*lives_funcptr_t)(void *);

typedef struct {
  lives_funcptr_t func;
  void *arg;
  volatile int busy;
  volatile int done;
  void *ret;
} thrd_work_t;


typedef LiVESList *lives_thread_t;

void lives_threadpool_init(void);
void lives_threadpool_finish(void);
int lives_thread_create(lives_thread_t *thread, void *attr, lives_funcptr_t func, void *arg);
int lives_thread_join(lives_thread_t work, void **retval);
