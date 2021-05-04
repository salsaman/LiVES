// memory.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _MEMORY_H_
#define _MEMORY_H_

#define EXTRA_BYTES (DEF_ALIGN * 4)

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

#ifdef USE_RPMALLOC
#include "rpmalloc.h"
#endif

boolean lives_freep(void **ptr);

void *lives_free_and_return(void *p);
void *lives_calloc_safety(size_t nmemb, size_t xsize) GNU_ALIGNED(DEF_ALIGN);
void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize) GNU_ALIGNED(DEF_ALIGN);

size_t get_max_align(size_t req_size, size_t align_max);

boolean init_memfuncs(void);

void lives_free_check(void *p);

#ifdef USE_RPMALLOC
void *quick_calloc(size_t n, size_t s);
void quick_free(void *p);
#endif

#ifdef THRD_SPECIFIC
void set_thread_id(uint64_t id);
uint64_t get_thread_id(void);
#endif

void dump_memstats(void);

void *lives_slice_alloc(size_t sz);
void lives_slice_unalloc(size_t sz, void *p);

#if GLIB_CHECK_VERSION(2, 14, 0)
void *lives_slice_alloc_and_copy(size_t sz, void *p);
#endif

void bigblock_init(void);
void *alloc_bigblock(size_t s);
void *calloc_bigblock(size_t nmemb, size_t align);
void *free_bigblock(void *bstart);

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
#if defined (HAVE_OPENCV) || defined (HAVE_OPENCV4)
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
#endif

#endif // USE_STD_MEMFUNCS

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
#ifdef USE_RPMALLOC
static void (*_lsd_free)(void *ptr) = rpfree;
#else
static void (*_lsd_free)(void *ptr) = _ext_free;
#endif
#ifdef USE_RPMALLOC
#define OVERRIDE_CALLOC_ALIGNED
static inline int _lsd_calloc_aligned_(void **memptr, size_t nmemb, size_t size) {
  return !memptr ? 0 : (!(*memptr = (rpaligned_calloc)(64, nmemb, size))) ? ENOMEM : 0;
}
#else
#define _lsd_calloc _ext_calloc
#endif
#if !HAVE_GETENTROPY
#define LSD_RANDFUNC(ptr, size) (lives_get_randbytes((ptr), (size)))
#endif

#include "lsd.h"
#undef OVERRIDE_MEMFUNCS

#ifdef __GNUC__
#define LIVES_GNU
#define lives_malloc_auto(size) __builtin_alloc(size)
#define lives_malloc_auto_aligned(size, align) __builtin_alloc_with_align(size, align)
#endif

#endif
