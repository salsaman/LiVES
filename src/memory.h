// memory.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef _MEMORY_H_
#define _MEMORY_H_

#define _VPP(p) ((void **)&(p))

#define DEF_ALIGN (sizeof(void *) * 8)

#define EXTRA_BYTES (HW_ALIGNMENT * 4)

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


#define FREE_AND_RETURN(a) lives_free_and_return((a))
#define FREE_AND_RETURN_VALUE(a, b) (!lives_free_and_return(((a)) ? (b) : (b))

boolean lives_freep(void **ptr);

void *lives_free_and_return(void *p);

void *lives_malloc_aligned(size_t nblocks, size_t align);
void *lives_calloc_aligned(size_t nblocks, size_t align);
void *lives_calloc_align(size_t xsize);
void *lives_calloc_safety(size_t nmemb, size_t xsize);
void *lives_calloc_aligned_safety(size_t xsize, size_t align);
void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize);

void lives_free_multi(int n, ...);

boolean init_memfuncs(int stage);

void lives_free_check(void *p);

#ifdef USE_RPMALLOC
void *quick_calloc(size_t n, size_t s);
void quick_free(void *p);
#endif

#define MAKE_WEAK(n, o) make_weak_ptr(_VPP(n), _VPP(o))

void make_weak_ptr(void **ptr_new, void **ptr_old);
void **get_other_ptr(void **ptr, int op);
void *lives_nullify_weak_check(void **pp);

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

void smallblock_init(void);

char *get_memstats(void);

void bigblock_init(void);
void *alloc_bigblock(size_t s);
void *calloc_bigblock(size_t s);
void *free_bigblock(void *bstart);

void swab2(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void swab4(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void swab8(const void *from, const void *to, size_t granularity) 	GNU_HOT;
void reverse_bytes(char *buff, size_t count, size_t granularity) 	GNU_HOT GNU_FLATTEN;
boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) 	GNU_HOT;

// can override defines here

#ifndef default_malloc
#define default_malloc malloc
#endif
#ifndef lives_realloc
#define lives_realloc realloc
#endif
#ifndef default_free
#define default_free free
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
#ifndef default_calloc
#define default_calloc calloc
#endif

#ifdef lives_malloc
#undef lives_malloc
#endif
#ifdef _lives_malloc_and_copy
#undef _lives_malloc_and_copy
#endif
#ifdef _lives_realloc
#undef _lives_realloc
#endif
#ifdef lives_free
#undef lives_free
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
#ifdef lives_calloc
#undef lives_calloc
#endif

#ifndef USE_STD_MEMFUNCS
// here we can define optimised memory functions to used by setting the symbols _lives_malloc, _lives_free, etc.
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
doso
#endif
#endif

#else

#ifdef ENABLE_OIL
#ifndef NO_OIL_MEMFUNCS
#define _lives_memcpy(dest, src, n) {if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) { \
      oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);		\
      return dest;\							\
    }
saisidsis
#endif
#endif
#endif

#endif // USE_STD_MEMFUNCS

// these function pointers will be passed to some plugins, effects for exmple
// do not allocate much memory, so can be optimised for small allocations

void *_ext_malloc(size_t n) GNU_MALLOC;
void *_ext_malloc_and_copy(size_t, const void *);// GNU_MALLOC_SIZE(1);
void _ext_unmalloc_and_copy(size_t, void *);
void _ext_free(void *);
void *_ext_free_and_return(void *);
void *_ext_memcpy(void *, const void *, size_t);
void *_ext_memset(void *, int, size_t);
void *_ext_memmove(void *, const void *, size_t);
void *_ext_realloc(void *, size_t);// GNU_MALLOC_SIZE(2);
void *_ext_calloc(size_t, size_t);// GNU_MALLOC_SIZE2(1, 2);

#if 0
#define OVERRIDE_MEMFUNCS

// pass optimised versions of memfuncs to headers and so on
static void *(*_lsd_memcpy)(void *dest, const void *src, size_t n) = _ext_memcpy;
static void *(*_lsd_memset)(void *s, int c, size_t n) = _ext_memset;

#ifdef USE_RPMALLOC
static void (*_lsd_free)(void *ptr) = rpfree;
#else
static void (*_lsd_free)(void *ptr) = _ext_free;
#endif

#ifdef USE_RPMALLOC
#define OVERRIDE_CALLOC_ALIGNED
extern capabilities *capable;
extern void *(*_lsd_calloc_aligned_)(void **memptr, size_t nmemb, size_t size);
#else
#define _lsd_calloc _ext_calloc
#endif

#if !HAVE_GETENTROPY
#define LSD_RANDFUNC(ptr, size) (lives_get_randbytes((ptr), (size)))
#endif

#undef OVERRIDE_MEMFUNCS

#ifdef USE_RPMALLOC
#define OVERRIDE_CALLOC_ALIGNED
#endif
#endif
///////////////////// here we can set ultimate overrides

void speedy_free(void *);
void *speedy_malloc(size_t s);
void *speedy_calloc(size_t, size_t);

////////////////////////////

#ifndef USE_STD_MEMFUNCS // not defined by default

// using _lives_malloc and so on, allows the pointers to be changed during runtime
// simply by redefining _lives_malloc, _lives_free, etc

#ifdef _lives_malloc
#undef  default_malloc
#define default_malloc _lives_malloc
#endif
#ifdef _lives_realloc
#undef  lives_realloc
#define lives_realloc _lives_realloc
#endif
#ifdef _lives_free
#undef  default_free
#define default_free _lives_free
#endif
#ifdef _lives_memcpy
#undef  lives_memcpy
#define lives_memcpy _lives_memcpy
#endif
#ifdef _lives_memcmp
#undef  lives_memcmp
#define lives_memcmp _lives_memcmp
#endif
#ifdef _lives_memset
#undef  lives_memset
#define lives_memset _lives_memset
#endif
#ifdef _lives_memmove
#undef  lives_memmove
#define lives_memmove _lives_memmove
#endif
#ifdef _lives_calloc
#undef  default_calloc
#define default_calloc _lives_calloc
#endif

#endif

#ifdef USE_INTRINSICS
double intrin_resample_vol(float *dst, size_t dst_skip, float *src, double offsd, double scale, float vol);
#endif

#ifdef __GNUC__
#define LIVES_GNU
#define _lives_malloc_auto(size) __builtin_alloc(size)
//void *lives_malloc_auto(size_t size);
#endif

extern void *(*lives_malloc)(size_t);
extern void (*lives_free)(void *);
extern void *(*lives_calloc)(size_t, size_t);

#endif
