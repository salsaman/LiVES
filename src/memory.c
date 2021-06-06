// memory.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

/// susbtitute memory functions. These must be real functions and not #defines since we need fn pointers

#include <sys/mman.h> // for mlock()

#include "main.h"

#ifdef USE_INTRINSICS
/// intrinsics
#if defined(_MSC_VER)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
// SSE SIMD intrinsics
#include <xmmintrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
// AVX SIMD intrinsics included
#include <x86intrin.h>
// SSE SIMD intrinsics
#include <xmmintrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
/* GCC-compatible compiler, targeting ARM with NEON */
#include <arm_neon.h>
#elif defined(__GNUC__) && defined(__IWMMXT__)
/* GCC-compatible compiler, targeting ARM with WMMX */
#include <mmintrin.h>
#endif
#endif

#ifdef THRD_SPECIFIC
//global-dynamic, local-dynamic, initial-exec, local-exec
#define TLS_MODEL __attribute__((tls_model("initial-exec")))

#if !defined(__clang__) && defined(__GNUC__)
#define _Thread_local __thread
#endif

static _Thread_local uint64_t _thread_id TLS_MODEL;


void set_thread_id(uint64_t id) {
  _thread_id = id;
}

uint64_t get_thread_id(void) {
  return _thread_id;
}
#endif


static uint64_t totalloc = 0, totfree = 0;

void dump_memstats(void) {
  g_print("Total allocated: %lu, total freed %lu\n", totalloc, totfree);
}

void *lives_slice_alloc(size_t sz) {
  totalloc += sz;
  return g_slice_alloc(sz);
}

void lives_slice_unalloc(size_t sz, void *p) {
  totfree += sz;
  return g_slice_free1(sz, p);
}

#if GLIB_CHECK_VERSION(2, 14, 0)
void *lives_slice_alloc_and_copy(size_t sz, void *p) {
  return g_slice_copy(sz, p);
}
#endif

LIVES_GLOBAL_INLINE boolean lives_freep(void **ptr) {
  // free a pointer and nullify it, only if it is non-null to start with
  // pass the address of the pointer in
  if (ptr && *ptr) {
    lives_free(*ptr);
    *ptr = NULL;
    return TRUE;
  }
  return FALSE;
}

#define OIL_MEMCPY_MAX_BYTES 12288 // this can be tuned to provide optimal performance

#ifdef ENABLE_ORC
livespointer lives_orc_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  static size_t maxbytes = OIL_MEMCPY_MAX_BYTES;
  static weed_plant_t *tuner = NULL;
  static boolean tuned = FALSE;
  static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
  boolean haslock = FALSE;
  if (n == 0) return dest;
  if (n < 32) return memcpy(dest, src, n);

  if (!mainw->multitrack && !LIVES_IS_PLAYING) {
    if (!tuned && !tuner) tuner = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 2);
    if (tuner) {
      if (!pthread_mutex_trylock(&tuner_mutex)) {
        haslock = TRUE;
      }
    }
  }

  if (maxbytes > 0 ? n <= maxbytes : n >= -maxbytes) {
    /// autotuning: first of all we provide the tuning parameters:
    /// (opaque) weed_plant_t *tuner, (int64_t)min range, (int64_t)max range, (int)ntrials,(double) cost
    /// the tuner will time from here until autotune_end and multiply the cost by the time
    /// we also reveal the value of the variable in autotune_end
    /// the tuner will run this ntrials times, then select a new value for the variable which is returned
    /// the costs for each value are totalled and averaged and finally the value with the lowest average cost / time is selected
    /// in this case what we are tuning is the bytesize threshold to select between one memory allocation function and another
    /// the cost in both cases is defined is 1.0 / n where n is the block size.
    /// The cost is the same for both functions - since time is also a factor
    /// the value should simply be the one with the lowest time per byte
    /// obviously this is very simplistic since there are many other costs than simply the malloc time
    /// however, it is a simple matter to adjust the cost calculation
    if (haslock) autotune_u64(tuner, -1024 * 1024, 1024 * 1024, 32, 1. / (double)n);
    orc_memcpy((uint8_t *)dest, (const uint8_t *)src, n);

    if (haslock) {
      maxbytes = autotune_u64_end(&tuner, maxbytes);
      if (!tuner) tuned = TRUE;
      pthread_mutex_unlock(&tuner_mutex);
    }
    return dest;
  }
  if (haslock) autotune_u64(tuner, -1024 * 1024, 1024 * 1024, 128, -1. / (double)n);
  memcpy(dest, src, n);
  if (haslock) {
    maxbytes = autotune_u64_end(&tuner, maxbytes);
    if (!tuner) tuned = TRUE;
    pthread_mutex_unlock(&tuner_mutex);
  }
  return dest;
}
#endif


#ifdef ENABLE_OIL
livespointer lives_oil_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  static size_t maxbytes = OIL_MEMCPY_MAX_BYTES;
  static weed_plant_t *tuner = NULL;
  static boolean tuned = FALSE;
  static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
  boolean haslock = FALSE;
  if (n == 0) return dest;
  if (n < 32) return memcpy(dest, src, n);

  if (!mainw->multitrack && !LIVES_IS_PLAYING) {
    if (!tuned && !tuner) tuner = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 2);
    if (tuner) {
      if (!pthread_mutex_trylock(&tuner_mutex)) {
        haslock = TRUE;
      }
    }
  }

  if (maxbytes > 0 ? n <= maxbytes : n >= -maxbytes) {
    if (haslock) autotune_u64(tuner, -1024 * 1024, 1024 * 1024, 32, 1. / (double)n);
    oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);

    if (haslock) {
      maxbytes = autotune_u64_end(&tuner, maxbytes);
      if (!tuner) tuned = TRUE;
      pthread_mutex_unlock(&tuner_mutex);
    }
    return dest;
  }
  if (haslock) autotune_u64(tuner, -1024 * 1024, 1024 * 1024, 128, -1. / (double)n);
  memcpy(dest, src, n);
  if (haslock) {
    maxbytes = autotune_u64_end(&tuner, maxbytes);
    if (!tuner) tuned = TRUE;
    pthread_mutex_unlock(&tuner_mutex);
  }
  return dest;
}
#endif

#define _cpy_if_nonnull(d, s, size) (d ? lives_memcpy(d, s, size) : d)

// functions with fixed pointers that we can pass to plugins ///
void *_ext_malloc(size_t n) {
#ifdef USE_RPMALLOC
  return rpmalloc(n);
#else
  return (n == 0 ? NULL : lives_malloc(n));
#endif
}


void *_ext_malloc_and_copy(size_t bsize, const void *block) {
  if (!block || bsize == 0) return NULL;
#ifdef lives_malloc_and_copy
  return lives_malloc_and_copy(bsize, block);
#endif
  return (_cpy_if_nonnull(malloc(bsize), block, bsize));
}

void _ext_unmalloc_and_copy(size_t bsize, void *p) {
  if (!p || bsize == 0) return;
#ifdef lives_unmalloc_and_copy
  lives_unmalloc_and_copy(bsize, p);
#else
  _ext_free(p);
#endif
}

void _ext_free(void *p) {
#ifdef USE_RPMALLOC
  rpfree(p);
#else
  if (p) lives_free(p);
#endif
}


void *_ext_free_and_return(void *p) {if (p) _ext_free(p); return NULL;}

void *_ext_memcpy(void *dest, const void *src, size_t n) {return lives_memcpy(dest, src, n);}

int _ext_memcmp(const void *s1, const void *s2, size_t n) {return lives_memcmp(s1, s2, n);}

void *_ext_memset(void *p, int i, size_t n) {return lives_memset(p, i, n);}

void *_ext_memmove(void *dest, const void *src, size_t n) {return lives_memmove(dest, src, n);}

void *_ext_realloc(void *p, size_t n) {
#ifdef USE_RPMALLOC
  return rprealloc(p, n);
#else
  return lives_realloc(p, n);
#endif
}

void *_ext_calloc(size_t nmemb, size_t msize) {
#ifdef USE_RPMALLOC
  return quick_calloc(nmemb, msize);
#else
  return lives_calloc(nmemb, msize);
#endif
}

LIVES_GLOBAL_INLINE void *lives_free_and_return(void *p) {if (p) lives_free(p); return NULL;}


LIVES_GLOBAL_INLINE size_t get_max_align(size_t req_size, size_t align_max) {
  size_t align = 1;
  while (align < align_max && !(req_size & align)) align *= 2;
  return align;
}


LIVES_GLOBAL_INLINE void *lives_calloc_safety(size_t nmemb, size_t xsize) {
  void *p;
  size_t totsize = nmemb * xsize;
  if (!totsize) return NULL;
  if (xsize < DEF_ALIGN) {
    xsize = DEF_ALIGN;
    nmemb = (totsize / xsize) + 1;
  }
  p = __builtin_assume_aligned(lives_calloc(nmemb + (EXTRA_BYTES / xsize), xsize), DEF_ALIGN);
  return p;
}

LIVES_GLOBAL_INLINE void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize) {
  /// realloc from omemb * size to nmemb * size
  /// memory allocated via calloc, with DEF_ALIGN alignment and EXTRA_BYTES extra padding
  void *np = __builtin_assume_aligned(lives_calloc_safety(nmemb, xsize), DEF_ALIGN);
  void *op = __builtin_assume_aligned(p, DEF_ALIGN);
  if (omemb > nmemb) omemb = nmemb;
  lives_memcpy(np, op, omemb * xsize);
  lives_free(p);
  return np;
}

#ifdef USE_RPMALLOC
void quick_free(void *p) {rpfree(p);}
#endif

#ifdef USE_RPMALLOC
void *quick_calloc(size_t n, size_t s) {return rpaligned_calloc(DEF_ALIGN, n, s);}
#endif

boolean init_memfuncs(void) {
#ifdef USE_RPMALLOC
  rpmalloc_initialize();
#endif
  bigblock_init();
  return TRUE;
}


boolean init_thread_memfuncs(void) {
#ifdef USE_RPMALLOC
  rpmalloc_thread_initialize();
#endif
  return TRUE;
}


#define NBIGBLOCKS 8
#define BBLOCKSIZE (33554432ul)

static void *bigblocks[NBIGBLOCKS];
static volatile int used[NBIGBLOCKS];

static int NBBLOCKS = 0;

#ifdef BBL_TEST
static int bbused = 0;
#endif

static pthread_mutex_t bigblock_mutex = PTHREAD_MUTEX_INITIALIZER;

void bigblock_init(void) {
  for (int i = 0; i < NBIGBLOCKS; i++) {
    bigblocks[i] = lives_calloc(1, BBLOCKSIZE);
    if (mlock(bigblocks[i], BBLOCKSIZE)) return;
    used[NBBLOCKS++] = 0;
  }
}

void *alloc_bigblock(size_t sizeb) {
  if (sizeb >= BBLOCKSIZE) {
    if (prefs->show_dev_opts) g_print("msize req %lu > %lu, cannot use bblockalloc\n", sizeb, BBLOCKSIZE);
    return NULL;
  }
  pthread_mutex_lock(&bigblock_mutex);
  for (int i = 0; i < NBBLOCKS; i++) {
    if (!used[i]) {
      used[i] = 1;
#ifdef BBL_TEST
      bbused++;
#endif
      pthread_mutex_unlock(&bigblock_mutex);
      return bigblocks[i];
    }
  }
  pthread_mutex_unlock(&bigblock_mutex);
  return NULL;
}

void *calloc_bigblock(size_t nmemb, size_t align) {
  void *start;
  if (nmemb * align + align >= BBLOCKSIZE) {
    if (prefs->show_dev_opts) g_print("size req %lu > %lu, cannot use bblockalloc\n", nmemb * align + align, BBLOCKSIZE);
    return NULL;
  }
  pthread_mutex_lock(&bigblock_mutex);
  for (int i = 0; i < NBBLOCKS; i++) {
    if (!used[i]) {
      used[i] = 1;
#ifdef BBL_TEST
      bbused++;
#endif
      pthread_mutex_unlock(&bigblock_mutex);
      start = (void *)((size_t)((size_t)((char *)bigblocks[i] + align - 1) / align) * align);
      lives_memset(start, 0, nmemb * align);
      return start;
    }
  }
  pthread_mutex_unlock(&bigblock_mutex);
  g_print("OUT OF BIGBLOCKS !!\n");
  return NULL;
}

void *free_bigblock(void *bstart) {
  for (int i = 0; i < NBBLOCKS; i++) {
    if ((char *)bstart >= (char *)bigblocks[i]
        && (char *)bstart - (char *)bigblocks[i] < BBLOCKSIZE) {
      used[i] = 0;
#ifdef BBL_TEST
      pthread_mutex_lock(&bigblock_mutex);
      if (prefs->show_dev_opts) g_print("bblocks in use: %d\n", bbused);
      bbused--;
      pthread_mutex_unlock(&bigblock_mutex);
#endif
      return NULL;
    }
  }
  abort();
  return NULL;
}


#ifdef USE_INTRINSICS
double intrin_resample_vol(float *dst, size_t dst_skip, float *src, double offsd, double scale, float vol) {
  // *dst = src[offs] * vol; dst += dst_skip; offs = rnd(offs + scale);
  // returns: offs after last float
  // load 4 vols
  // load 4 src vals
  // mult 4
  // map dst back
  float srcf[4], dstf[4];
  __m128 srcv, dstv;
  __m128 volsv = _mm_load1_ps(&vol);
  off64_t offs;
  int i;
  for (i = 0; i < 3; i++) {
    if (scale < 0.) {
      offs = (off64_t)(offsd - .4999);
    } else {
      offs = (off64_t)(offsd + .4999);
    }
    srcf[i] = src[offs];
    offsd += scale;
  }
  srcv = _mm_load_ps(srcf);
  dstv = _mm_mul_ps(srcv, volsv);
  _mm_store_ps(dstf, dstv);
  for (i = 0; i < 3; i++) {
    dst[dst_skip++] = dstf[i];
  }
  return offsd;
}
#endif
