// memory.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

/// susbtitute memory functions. These must be real functions and not #defines since we need fn pointers

#include "main.h"

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


void *_ext_free_and_return(void *p) {_ext_free(p); return NULL;}

void *_ext_memcpy(void *dest, const void *src, size_t n) {return lives_memcpy(dest, src, n);}

int _ext_memcmp(const void *s1, const void *s2, size_t n) {return lives_memcmp(s1, s2, n);}

void *_ext_memset(void *p, int i, size_t n) {return lives_memset(p, i, n);}

void *_ext_memmove(void *dest, const void *src, size_t n) {return lives_memmove(dest, src, n);}

void *_ext_realloc(void *p, size_t n) {
#ifdef USE_RPMALLOC
  return rprealloc(p, n);
#else
  return lives_realloc(p, n);
}
#endif
}

void *_ext_calloc(size_t nmemb, size_t msize) {
#ifdef USE_RPMALLOC
  return quick_calloc(nmemb, msize);
#else
  return lives_calloc(nmemb, msize);
}
#endif
}

LIVES_GLOBAL_INLINE void *lives_free_and_return(void *p) {lives_free(p); return NULL;}


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

void quick_free(void *p) {rpfree(p);}

void *quick_calloc(size_t n, size_t s) {return rpaligned_calloc(DEF_ALIGN, n, s);}

boolean init_memfuncs(void) {
#ifdef USE_RPMALLOC
  rpmalloc_initialize();
#endif
  return TRUE;
}


boolean init_thread_memfuncs(void) {
#ifdef USE_RPMALLOC
  rpmalloc_thread_initialize();
#endif
  return TRUE;
}

