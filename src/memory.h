// memory.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifndef FINALISE_MEMFUNCS

#ifndef _MEMORY_H_
#define _MEMORY_H_

#if USE_RPMALLOC
#include "rpmalloc.h"
#endif

#define _VPP(p) ((void **)&(p))

#define DEF_ALIGN (sizeof(void *) * 8)

extern size_t PAGESIZE;

#define EXTRA_BYTES (HW_ALIGNMENT * 4)

typedef void *(*malloc_f)(size_t);
typedef void *(*calloc_f)(size_t, size_t);
typedef void *(*realloc_f)(void *, size_t);

typedef void (*free_f)(void *);
typedef void *(*free_and_return_f)(void *); // like free() but returns NULL

typedef void *(*memcpy_f)(void *, const void *, size_t);
typedef void *(*memset_f)(void *, int, size_t);
typedef int (*memcmp_f)(const void *, const void *, size_t);
typedef void *(*memmove_f)(void *, const void *, size_t);

typedef void *(*malloc_and_copy_f)(size_t, const void *);
typedef void (*unmalloc_and_copy_f)(size_t, void *);

typedef void *(*lives_slice_alloc_f)(size_t);
typedef void (*lives_slice_unalloc_f)(size_t, void *);

/* extern void *(*lives_malloc)(size_t); */
/* extern void (*lives_free)(void *); */
/* extern void *(*lives_calloc)(size_t, size_t); */

// _default_* contain the standard (non-overriden) versions
// these can be overriden by #defining _default_* (not recommended)
// initially, _lives_malloc, etc will be defined as the default versions

// a header can #undef then #define _lives_malloc, _lives_free, etc.

// during runtime, lives_malloc will start by pointing at _lives_malloc, however it can be pointed at other
// compatible functions, then later pointed back to _lives_malloc

// default_malloc and so may be called to use the standard non-overridden versions

// if USE_STD_MEMFUNCS is #defined as 1, this will force LiVES to use only malloc, free, etc. This can be useful for debugging
// testing, or if there is an isse with overriding memory functions etc.

// if USE_DEFAULT_MEMFUNCS is defined, LiVES will only use the default_ values set below

// finally, alt_malloc, alt_free. etc can be defined to force lives to use these values, ignoring _lives_malloc
// etc, but without having to fall back to USE_STD_MEMFUNCS or chenging the default values

// std funcitons defined are malloc, realloc, calloc, free, memcpy, memset, memmove and memcmp

// specific use functions -
// free_and_return - the same as free, but returns NULL. Useful for macros.
// recalloc - like realloc, but clears extra memory allocated. May need passing the old number of elements.
// malloc_and_copy   - may be a single function or done as malloc then memcpy
// unamlloc_and_copy - reverse of above if it needs a special function (takes a size parameter)
// slice_alloc - may be used to allocate from a slab allocator
// slice_unalloc - reverse of above if it needs a special function (takes a size parameter)
// slice_alloc_and_copy   - may be a single function, otherwise calls slice_alloc / memcpy

#if USE_STD_MEMFUNCS

#ifdef default_malloc
#undef default_malloc
#endif
#ifdef default_calloc
#undef default_calloc
#endif
#ifdef default_realloc
#undef default_realloc
#endif
#ifdef default_free
#undef default_free
#endif
#ifdef default_memcpy
#undef default_memcpy
#endif
#ifdef default_memset
#undef default_memset
#endif
#ifdef default_memcmp
#undef default_memcmp
#endif
#ifdef default_memmove
#undef default_memmove
#endif

#ifndef USE_DEFAULT_MEMFUNCS
#define USE_DEFAULT_MEMFUNCS
#endif

#endif

#ifndef default_malloc
#define default_malloc malloc
#endif
#ifndef default_calloc
#define default_calloc calloc
#endif
#ifndef default_realloc
#define default_realloc realloc
#endif
#ifndef default_free
#define default_free free
#endif
#ifndef default_memcpy
#define default_memcpy memcpy
#endif
#ifndef default_memset
#define default_memset memset
#endif
#ifndef default_memcmp
#define default_memcmp memcmp
#endif
#ifndef default_memmove
#define default_memmove memmove
#endif

#ifdef lives_malloc
#undef lives_malloc
#endif
#ifdef _lives_malloc
#undef _lives_malloc
#endif

#ifdef lives_calloc
#undef lives_calloc
#endif
#ifdef _lives_calloc
#undef _lives_calloc
#endif

#ifdef _lives_realloc
#undef _lives_realloc
#endif
#ifdef lives_realloc
#undef lives_realloc
#endif

#ifdef _lives_free
#undef _lives_free
#endif
#ifdef lives_free
#undef lives_free
#endif

#ifdef _lives_memcpy
#undef _lives_memcpy
#endif
#ifdef lives_memcpy
#undef lives_memcpy
#endif

#ifdef _lives_memset
#undef _lives_memset
#endif
#ifdef lives_memset
#undef lives_memset
#endif

#ifdef _lives_memcmp
#undef _lives_memcmp
#endif
#ifdef lives_memcmp
#undef lives_memcmp
#endif

#ifdef _lives_memmove
#undef _lives_memmove
#endif
#ifdef lives_memmove
#undef lives_memmove
#endif

#ifdef _lives_malloc_and_copy
#undef _lives_malloc_and_copy
#endif
#ifdef lives_malloc_and_copy
#undef lives_malloc_and_copy
#endif

extern malloc_f lives_malloc;
extern calloc_f lives_calloc;
extern realloc_f lives_realloc;
extern free_f lives_free;
extern memcpy_f lives_memcpy;
extern memset_f lives_memset;
extern memcmp_f lives_memcmp;
extern memmove_f lives_memmove;

/////////////////////
// these functions are guaranteed to be pointers, and may be passed to external components
// such as libraries or plugins

void *_ext_malloc(size_t n) LIVES_MALLOC;
void *_ext_calloc(size_t, size_t)LIVES_MALLOC_SIZE2(1, 2);
void *_ext_realloc(void *, size_t) LIVES_MALLOC_SIZE(2);
void _ext_free(void *);
void *_ext_free_and_return(void *);
void *_ext_memcpy(void *, const void *, size_t);
void *_ext_memset(void *, int, size_t);
int _ext_memcmp(const void *, const void *, size_t);
void *_ext_memmove(void *, const void *, size_t);

void *_ext_malloc_and_copy(size_t, const void *);// LIVES_MALLOC_SIZE(1);
void _ext_unmalloc_and_copy(size_t, void *);

// ext_slice_alloc
// ext_slice_unalloc
// ext_slice_alloc_and_copy

///////////////////////////////////

boolean init_memfuncs(int stage);

#define copy_if_nonnull(d, s, size) (d ? lives_memcpy(d, s, size) : d)

#define FREE_AND_RETURN(a) lives_free_and_return((a))
#define FREE_AND_RETURN_VALUE(a, b) (!lives_free_and_return(((a)) ? (b) : (b))

#define LIVES_MALLOC_AND_COPY(src, size) \
  lives_memcpy(lives_malloc(size), src, size)

#define lives_nullify_ptr(void_ptr_ptr) do {if (void_ptr_ptr) *void_ptr_ptr = NULL;} while (0);

boolean lives_nullify_ptr_cb(void *dummy, void **vpp);

void *lives_free_and_return(void *p);

boolean lives_freep(void **ptr);

void *lives_slice_alloc(size_t sz);
void lives_slice_unalloc(size_t sz, void *);

#if GLIB_CHECK_VERSION(2, 14, 0)
void *lives_slice_alloc_and_copy(size_t sz, void *);
#endif

//////////////// aligned pointers

void *lives_calloc_align(size_t xsize);
void *lives_calloc_safety(size_t nmemb, size_t xsize);

///////////////////// byte manipulation /////////

void swab2(const void *from, const void *to, size_t granularity) 	LIVES_HOT;
void swab4(const void *from, const void *to, size_t granularity) 	LIVES_HOT;
void swab8(const void *from, const void *to, size_t granularity) 	LIVES_HOT;

#define reverse_bytes(buff, count, granularity) do {\
  count == 2 ? swab2(buff, buff, 1) : count == 4 ? swab4(buff, buff, granularity) \
    : count == 8 ? swab8(buff, buff, granularity) : (void)0;} while (0);

boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) 	LIVES_HOT;

///////////////////////// special allocators //////////

void smallblock_init(void);

void bigblock_init(void);
void *alloc_bigblock(size_t s);
void *calloc_bigblock(size_t s);
void *free_bigblock(void *bstart);

void *lives_malloc_medium(size_t msize);
void *lives_calloc_medium(size_t msize);

//////////////////////////// utility functions ///

void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize);

void lives_free_multi(int n, ...);

/////////// info functions //////////

void lives_free_check(void *p);
void dump_memstats(void);
char *get_memstats(void);

////////////////////// memfunc overrides /////////

// here we can define optimised memory functions to used by setting the symbols _lives_malloc, _lives_free, etc.

// at the end of the header we check if the values have been set and update lives_malloc from _lives_malloc, etc.
// the same values are passed into realtime fx plugins via Weed function overloading

/* #if defined (HAVE_OPENCV) || defined (HAVE_OPENCV4) */
/* #ifndef NO_OPENCV_MEMFUNCS */
/* #define _lives_malloc(sz) alignPtr(sz, HW_ALIGNMENT); */
/* #define _lives_free    fastFree */
/* #define _lives_realloc proxy_realloc */
/* #endif */
/* #endif */

#ifndef __cplusplus

#ifdef ENABLE_ORC

void *lives_orc_memcpy(void *dest, const void *src, size_t n);
#if ALLOW_ORC_MEMCPY
#define _lives_memcpy lives_orc_memcpy
#endif

#else

#ifdef ENABLE_OIL
void *lives_oil_memcpy(void *dest, const void *src, size_t n);
#if ALLOW_OIL_MEMFUNCS
#define _lives_memcpy(dest, src, n) {if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) { \
    lives_oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n); return dest;}
#endif
#endif

#endif

#endif // c++


#endif // HAVE_MEMFUNC

#else // FINALISE_MEMFUNCS

#ifndef HAVE_FINAL_MEMFUNCS
#define HAVE_FINAL_MEMFUNCS

#ifdef USE_DEFAULT_MEMFUNCS
#undef _lives_malloc
#undef _lives_calloc
#undef _lives_realloc
#undef _lives_free
#undef _lives_memcpy
#undef _lives_memset
#undef _lives_memcmp
#undef _lives_memmove
#endif

#ifndef _lives_malloc
#define _lives_malloc default_malloc
#endif
#ifndef _lives_mclloc
#define _lives_calloc default_calloc
#endif
#ifndef _lives_realloc
#define _lives_realloc default_realloc
#endif
#ifndef _lives_free
#define _lives_free default_free
#endif
#ifndef _lives_memcpy
#define _lives_memcpy default_memcpy
#endif
#ifndef _lives_memset
#define _lives_memset default_memset
#endif
#ifndef _lives_memcmp
#define _lives_memcmp default_memcmp
#endif
#ifndef _lives_memmove
#define _lives_memmove default_memmove
#endif

void speedy_free(void *);
void *speedy_malloc(size_t s);
void *speedy_calloc(size_t, size_t);

#endif // !HAVE_FINAL_MEMFUNCS
#endif // FINALISE_MEMFUNCS

///////////////////// TRASH SECTION ///////////////

#if 0
// these defines can be for lsd.h which uses self contained definitions
#define OVERRIDE_MEMFUNCS

// pass optimised versions of memfuncs to headers and so on
static void *(*_lsd_memcpy)(void *dest, const void *src, size_t n) = _ext_memcpy;
static void *(*_lsd_memset)(void *s, int c, size_t n) = _ext_memset;

#if USE_RPMALLOC
static void (*_lsd_free)(void *ptr) = rpfree;
#else
static void (*_lsd_free)(void *ptr) = _ext_free;
#endif

/* #if USE_RPMALLOC */
/* #define OVERRIDE_CALLOC_ALIGNED */
/* extern capabilities *capable; */
/* extern void *(*_lsd_calloc_aligned_)(void **memptr, size_t nmemb, size_t size); */
/* #else */
/* #define _lsd_calloc _ext_calloc */
/* #endif */

#if !HAVE_GETENTROPY
#define LSD_RANDFUNC(ptr, size) (lives_get_randbytes((ptr), (size)))
#endif

#undef OVERRIDE_MEMFUNCS

/* #if USE_RPMALLOC */
/* #define OVERRIDE_CALLOC_ALIGNED */
/* #endif */

///////////////////// here we can set ultimate overrides


////////////////// test functions /////

#ifdef USE_INTRINSICS
double intrin_resample_vol(float *dst, size_t dst_skip, float *src, double offsd, double scale, float vol);
#endif

#ifdef __GNUC__
#define LIVES_GNU
#define _lives_malloc_auto(size) __builtin_alloc(size)
//void *lives_malloc_auto(size_t size);
#endif

/* #define MAKE_WEAK(n, o) make_weak_ptr(_VPP(n), _VPP(o)) */

/* void make_weak_ptr(void **ptr_new, void **ptr_old); */
/* void **get_other_ptr(void **ptr, int op); */
/* void *lives_nullify_weak_check(void **pp); */

/* #ifdef THRD_SPECIFIC */
/* void set_thread_id(uint64_t id); */
/* uint64_t get_thread_id(void); */
/* #endif */

#endif // 0
