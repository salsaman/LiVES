// memory.c
// LiVES
// (c) G. Finch 2019 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

/// susbtitute memory functions. These must be real functions and not #defines since we need fn pointers

#include <sys/mman.h> // for mlock()

#include "main.h"

#ifdef USE_RPMALLOC
//void *(*_lsd_calloc_aligned_)(void **memptr, size_t nmemb, size_t size);
#endif

///////////////////////////// testing - not used /////////
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

////////////////////////////////////////////

// TODO
/* LIVES_GLOBAL_INLINE boolean make_critical(boolean is_crit) { */
/*   if (capable) { */
/*     if (!capable->hw.oom_adj_file) { */
/*       capable->hw_oom_adj_file =  */

/*     } */
/*     if (!*capable->hw.oom_adj_file) return FALSE; */
/*   } */
/* } */


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

#if 0
typedef struct {
  // if set then *ptr_to points to real memory and should be freed
  // free (*ptr); ptr = NULL;
  // else just ptr = NULL;
  // ptr_from is unique, but ptr_to can be replicated
  uint8_t isr;
  void **ptr_to;
  void **ptr_from;
} lives_weak;

static LiVESList *weak_list = NULL;

LIVES_GLOBAL_INLINE void make_weak_ptr(void **ptr_new, void **ptr_old) {
  // if we create a weak ptr of type (void *)a = (void)*b, we can have problems
  // if ptr a is freed, via freep((void**)&a), then b will point to freed memory
  // thus we can use MAKE_WEAK(&a, &b); instead. then we must call LIVES_NULLIFY(&a)
  // or LIVES_NULLIFY(&b), now both if both pointers are still pointing to the same memory, they
  // will be nullified together, and the real target will only be freed once.
  // also calling WEAK_UPPDATE(&a) or WEAK_UPDATE(&b)

  // for now it is simple and just goes 1 level deep
  if (ptr_new) {
    lives_weak *weak = (lives_weak *)lives_calloc(1, sizeof(lives_weak));
    weak->weak = ptr_new;
    weak->real = ptr_old;
    *ptr_new = *ptr_old;
    weak_list = lives_list_prepend(weak_list, &weak);
  }
}


LIVES_GLOBAL_INLINE void **get_other_ptr(void **ptr, int op) {
  // op == 0, return real ptr
  // op == 1 return and remove node

  // TODO
  // op == 1, return real ptr recursively

  // op = 2, as 1, but nullify it and its partner

  // op 3, as 2 but then search for any other weak pointers
  // - find lowest level realptr, nullify, then coming back up, if it was straight,
  // find any other weaks pointed to it recursively, and nullify
  // if it was crooked, skip over. until we return to top level, then free target)

  // op = 2, remove a single node

  LiVESList *list = weak_list;
  lives_weak *xweak;
  void **weak, void **real;
  FIND_BY_DATA_2FIELD(list, lives_weak, weak, real, ptr);
  if (!list) return NULL;
  xweak = (lives_weak *)list->data;
  weak = xweak->weak;
  real = xweak->real;
  if (op == 1) weak_list = lives_list_remove_node(weak_list, xweak, TRUE);
  if (*weak != *real) return NULL;
  if (ptr == real) return weak;
  // is this what they call "tail recursion" ??
  //real = get_real_ptr(weak->real);
  return real;
}


LIVES_GLOBAL_INLINE boolean is_weak_ptr(void **ptrptr) {
  LiVESList *list = weak_list;
  return !!(FIND_BY_DATA_FIELD(list, lives_weak, weak, ptrptr));
}


void *lives_nullify_weak_check(void **pp) {
  if (weak_list) {
    other = get_other_ptr(ptr. 1);
    if (other) *other = NULL;
  }
  return 0;
}

#endif

LIVES_GLOBAL_INLINE boolean lives_freep(void **ptr) {
  // free a pointer and nullify it, only if it is non-null to start with
  // pass the address of the pointer in
  // WARNING !! if ptr == WEAK(other_ptr) then other_ptr will not be set to NULL !!

  if (ptr && *ptr) {
    lives_free(*ptr);
    //lives_nullify_weak_check(ptr);
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
  //return (n == 0 ? NULL : default_malloc(n));
  return (n == 0 ? NULL : malloc(n));
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

#ifdef USE_RPMALLOC
void *lsd_calloc_aligned(void **memptr, size_t nmemb, size_t size) {
  return !memptr ? NULL : (!(*memptr = (rpaligned_calloc)(HW_ALIGNMENT, nmemb, size))) ? NULL : *memptr;
}
#endif

void _ext_free(void *p) {
#ifdef USE_RPMALLOC
  rpfree(p);
#else
  //if (p) default_free(p);
  if (p) free(p);
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
  //return lives_realloc(p, n);
  return realloc(p, n);
#endif
}

void *_ext_calloc(size_t nmemb, size_t msize) {
#ifdef USE_RPMALLOC
  return quick_calloc(nmemb, msize);
#else
  //return default_calloc(nmemb, msize);
  return calloc(nmemb, msize);
#endif
}

LIVES_GLOBAL_INLINE void *lives_free_and_return(void *p) {if (p) lives_free(p); return NULL;}


LIVES_GLOBAL_INLINE void *lives_malloc_aligned(size_t nblocks, size_t align) {
  return aligned_alloc(align, nblocks * align);
}


static size_t hwlim = 0;
static LiVESList *smblock_list = NULL;
static LiVESList *smu_list = NULL;
static int smblock_count = 0;
static char *smblock = 0;

static pthread_mutex_t smblock_mutex = PTHREAD_MUTEX_INITIALIZER;

#define SMBLOCKS 1024 * 1024

static int n_smblocks = SMBLOCKS;

static size_t memsize;
char *get_memstats(void) {
  char *msg;

  if (smblock) msg = lives_strdup_printf("smallblock: total size %ld, block size %ld, page_size = %ld, "
					 "cachline_size = %d\n"
					 "Blocks in use: %d of %d (%.2f %%)\n",
					 memsize, hwlim, capable->hw.pagesize, capable->hw.cacheline_size,
					 n_smblocks - smblock_count, n_smblocks,
					 (double)(n_smblocks - smblock_count)
					 / (double)smblock_count * 100.);
  else msg = lives_strdup_printf("smallblock not in use\n");
  return msg;
}
				 

void smallblock_init(void) {
  hwlim = HW_ALIGNMENT;
  memsize = n_smblocks * hwlim;
  if (capable->hw.pagesize) {
    memsize = (size_t)(memsize / capable->hw.pagesize) * capable->hw.pagesize;
#ifdef HAVE_POSIX_MEMALIGN
    IGN_RET(posix_memalign((void **)&smblock, capable->hw.pagesize, memsize));
#endif
  }

  if (!smblock) {
    if (capable->hw.pagesize) {
      smblock = lives_calloc_aligned(memsize / capable->hw.pagesize, capable->hw.pagesize);
    } else {
      smblock = lives_calloc_align(memsize);
    }
  }

  if (smblock) {
    char *smbptr;
    if (mlock(smblock, memsize)) {
      lives_free(smblock);
      return;
    }
    memset(smblock, 0, memsize);
    smbptr = smblock;
    n_smblocks = memsize / hwlim;

    for (int i = 0; i < n_smblocks; i++) {
      smblock_list = lives_list_prepend(smblock_list, (void *)smbptr);
      smbptr += hwlim;
    }
    smblock_list = lives_list_reverse(smblock_list);
    smblock_count = n_smblocks;
    pthread_mutex_lock(&smblock_mutex);
    lives_free = speedy_free;
    lives_malloc = speedy_malloc;
    lives_calloc = speedy_calloc;
    pthread_mutex_unlock(&smblock_mutex);
  }
}


void *speedy_malloc(size_t xsize) {
  if (xsize <= hwlim) {
    void *p;
    pthread_mutex_lock(&smblock_mutex);
    if (smblock_list) {
      p = smblock_list->data;
      smblock_list = smblock_list->next;
      smu_list = lives_list_prepend(smu_list, p);
      smblock_count--;
    } else {
      lives_malloc = default_malloc;
      lives_calloc = default_calloc;
      return lives_malloc(xsize);
    }
    pthread_mutex_unlock(&smblock_mutex);
    return p;
  }
  return (*default_malloc)(xsize);
}


void *speedy_calloc(size_t nelems, size_t esize) {
  if (smblock_count >= nelems && esize <= hwlim) {
    if (nelems == 1) return speedy_malloc(esize);
    pthread_mutex_lock(&smblock_mutex);
    if (smblock_count >= nelems) {
      void *p = smblock_list->data;
      for (int i = 0; i < nelems; i++) {
        smu_list = lives_list_prepend(smu_list, smblock_list->data);
        smblock_list = smblock_list->next;
      }
      smblock_count -= nelems;
      pthread_mutex_unlock(&smblock_mutex);
      return p;
    }
    pthread_mutex_unlock(&smblock_mutex);
  }
  return (*default_calloc)(nelems, esize);
}

void speedy_free(void *p) {
  if ((char *)p >= smblock && (char *)p < smblock + memsize) return;
  default_free(p);
  if (!smblock_count) lives_free = default_free;
}



// WARNING - size allocated is nblocks * align
LIVES_GLOBAL_INLINE void *lives_calloc_aligned(size_t nblocks, size_t align) {
  void *p = lives_malloc_aligned(nblocks, align);
  if (p) lives_memset(p, 0, nblocks * align);
  return p;
}


LIVES_GLOBAL_INLINE void *lives_calloc_align(size_t xsize) {
  if (!xsize) return NULL;
  else {
    size_t align = HW_ALIGNMENT;
    xsize = ((xsize + align - 1) / align);
    return lives_calloc_aligned(xsize, align);
  }
}


LIVES_GLOBAL_INLINE void *lives_calloc_safety(size_t nmemb, size_t xsize) {
  return lives_calloc_align(nmemb * xsize + EXTRA_BYTES);
}


LIVES_GLOBAL_INLINE void *lives_calloc_aligned_safety(size_t xsize, size_t align) {
  xsize = ((size_t)(xsize + EXTRA_BYTES + align - 1) / align);
  return lives_calloc_aligned(xsize, align);
}


LIVES_GLOBAL_INLINE void *lives_recalloc(void *op, size_t nmemb, size_t omemb, size_t xsize) {
  /// realloc from omemb * size to nmemb * size
  do {
    void *np = lives_calloc_safety(nmemb, xsize);
    if (op && omemb > 0) {
      if (omemb > nmemb) omemb = nmemb;
      lives_memcpy(np, op, omemb * xsize);
    }
    if (op) lives_free(op);
    return np;
  } while (FALSE);
}

#ifdef USE_RPMALLOC
void quick_free(void *p) {rpfree(p);}
#endif

#ifdef USE_RPMALLOC
void *quick_calloc(size_t n, size_t s) {
  size_t align = HW_ALIGNMENT;
  return rpaligned_calloc(align, n, s);
}
#endif

boolean init_memfuncs(int stage) {
  if (stage == 0) {
    lives_malloc = default_malloc;
    lives_calloc = default_calloc;
    lives_free = default_free;

#ifdef USE_RPMALLOC
    //_lsd_calloc_aligned_ = lsd_calloc_aligned;
    rpmalloc_initialize();
#endif
  }

  else if (stage == 1) {
    bigblock_init();
    smallblock_init();
  }

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

#define NBAD_LIM 8
#define BBL_TEST
#ifdef BBL_TEST
static int bbused = 0;
#endif

static int nbads = 0;

static pthread_mutex_t bigblock_mutex = PTHREAD_MUTEX_INITIALIZER;

static size_t bmemsize;

void bigblock_init(void) {
  bmemsize = BBLOCKSIZE;
  hwlim = HW_ALIGNMENT;
  bmemsize = (size_t)(bmemsize / hwlim) * hwlim;

  if (capable->hw.pagesize) {
    bmemsize = (size_t)(bmemsize / capable->hw.pagesize) * capable->hw.pagesize;
  }
  for (int i = 0; i < NBIGBLOCKS; i++) {
#ifdef HAVE_POSIX_MEMALIGN
    IGN_RET(posix_memalign(&bigblocks[i], capable->hw.pagesize, bmemsize));
#endif
    if (!bigblocks[i]) {
      if (capable->hw.pagesize) {
        bigblocks[i] = lives_calloc_aligned(bmemsize / capable->hw.pagesize, capable->hw.pagesize);
      } else {
        bigblocks[i] = lives_calloc_align(bmemsize);
      }
    }
    if (mlock(bigblocks[i], bmemsize)) {
      lives_free(bigblocks[i]);
      return;
    }
    used[NBBLOCKS++] = -1;
  }
  bmemsize -= EXTRA_BYTES;
}

void *alloc_bigblock(size_t sizeb) {
  if (sizeb >= bmemsize) {
    if (prefs->show_dev_opts) g_print("msize req %lu > %lu, cannot use bblockalloc\n",
                                        sizeb, bmemsize);
    return NULL;
  }
  pthread_mutex_lock(&bigblock_mutex);
  for (int i = 0; i < NBBLOCKS; i++) {
    if (used[i] == -1) {
      used[i] = 0;
#ifdef BBL_TEST
      bbused++;
#endif
      pthread_mutex_unlock(&bigblock_mutex);
      //g_print("ALLOBIG %p\n", bigblocks[i]);
      return bigblocks[i];
    }
  }
  pthread_mutex_unlock(&bigblock_mutex);
  return NULL;
}

void *calloc_bigblock(size_t xsize) {
  void *start;
  if (xsize > bmemsize) {
    if (prefs->show_dev_opts) g_print("size req %lu > %lu, "
                                        "cannot use bblockalloc\n", xsize,
                                        bmemsize);
    return NULL;
  }
  pthread_mutex_lock(&bigblock_mutex);
  for (int i = 0; i < NBBLOCKS; i++) {
    if (used[i] == -1) {
      used[i] = 0;
#ifdef BBL_TEST
      bbused++;
#endif
      pthread_mutex_unlock(&bigblock_mutex);
      g_print("CALLOBIG %p %d\n", bigblocks[i], i);
      break_me("callobig");
      nbads = 0;
      start = bigblocks[i];
      /* start = (void *)((size_t)((size_t)((char *)bigblocks[i] + align - 1) / align) * align); */
      /* used[i] = (char *)start - (char *)bigblocks[i]; */
      lives_memset(start, 0, xsize + EXTRA_BYTES);
      return start;
    }
  }
  pthread_mutex_unlock(&bigblock_mutex);
  break_me("bblock");
  g_print("OUT OF BIGBLOCKS !!\n");
  if (++nbads > NBAD_LIM) lives_abort("Aborting due to probable internal memory errors");
  return NULL;
}

void *free_bigblock(void *bstart) {
  for (int i = 0; i < NBBLOCKS; i++) {
    if ((char *)bstart >= (char *)bigblocks[i]
        && (char *)bstart - (char *)bigblocks[i] < bmemsize + EXTRA_BYTES) {
      if (used[i] == -1) lives_abort("Bigblock freed twice, Aborting due to probable internal memory errors");
      used[i] = -1;
#ifdef BBL_TEST
      pthread_mutex_lock(&bigblock_mutex);
      if (prefs->show_dev_opts) g_print("bblocks in use: %d\n", bbused);
      bbused--;
      pthread_mutex_unlock(&bigblock_mutex);
#endif
      g_print("FREEBIG %p %d\n", bigblocks[i], i);
      return NULL;
    }
  }
  lives_abort("Attempt to free() invalid bigblock - aborting due to internal memory errors");
  return NULL;
}


static uint16_t swabtab[65536];
static boolean swabtab_inited = FALSE;

static void init_swabtab(void) {
  for (int i = 0; i < 256; i++) {
    int z = i << 8;
    for (int j = 0; j < 256; j++) {
      swabtab[z++] = (j << 8) + i;
    }
  }
  swabtab_inited = TRUE;
}

union split8 {
  uint64_t u64;
  uint32_t u32[2];
};

union split4 {
  uint32_t u32;
  uint16_t u16[2];
};

// gran(ularity) may be 1, or 2
LIVES_GLOBAL_INLINE void swab2(const void *from, const void *to, size_t gran) {
  uint16_t *s = (uint16_t *)from;
  uint16_t *d = (uint16_t *)to;
  if (gran == 2) {
    uint16_t tmp = *s;
    *s = *d;
    *d = tmp;
    return;
  }
  if (!swabtab_inited) init_swabtab();
  *d = swabtab[*s];
}

// gran(ularity) may be 1, 2 or 4
LIVES_GLOBAL_INLINE void swab4(const void *from, const void *to, size_t gran) {
  union split4 *d = (union split4 *)to, s;
  uint16_t tmp;

  if (gran > 2) {
    lives_memcpy((void *)to, from, gran);
    return;
  }
  s.u32 = *(uint32_t *)from;
  tmp = s.u16[0];
  if (gran == 2) {
    d->u16[0] = s.u16[1];
    d->u16[1] = tmp;
  } else {
    swab2(&s.u16[1], &d->u16[0], 1);
    swab2(&tmp, &d->u16[1], 1);
  }
}


// gran(ularity) may be 1, 2 or 4
LIVES_GLOBAL_INLINE void swab8(const void *from, const void *to, size_t gran) {
  union split8 *d = (union split8 *)to, s;
  uint32_t tmp;
  if (gran > 4) {
    lives_memcpy((void *)to, from, gran);
    return;
  }
  s.u64 = *(uint64_t *)from;
  tmp = s.u32[0];
  if (gran == 4) {
    d->u32[0] = s.u32[1];
    d->u32[1] = tmp;
  } else {
    swab4(&s.u32[1], &d->u32[0], gran);
    swab4(&tmp, &d->u32[1], gran);
  }
}


LIVES_GLOBAL_INLINE void reverse_bytes(char *buff, size_t count, size_t gran) {
  if (count == 2) swab2(buff, buff, 1);
  else if (count == 4) swab4(buff, buff, gran);
  else if (count == 8) swab8(buff, buff, gran);
}


boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) {
  // reverse chunk sized bytes in buff, count must be a multiple of chunk
  ssize_t start = -1, end;
  size_t ocount = count;

  if (chunk < 8) {
    if ((chunk != 4 && chunk != 2 && chunk != 1) || (count % chunk) != 0) return FALSE;
  } else {
    if ((chunk & 0x01) || (count % chunk) != 0) return FALSE;
    else {
#ifdef USE_RPMALLOC
      void *tbuff = rpmalloc(chunk);
#else
      void *tbuff = lives_malloc(chunk);
#endif
      start++;
      end = ocount - 1 - chunk;
      while (start + chunk < end) {
        lives_memcpy(tbuff, &buff[end], chunk);
        lives_memcpy(&buff[end], &buff[start], chunk);
        lives_memcpy(&buff[start], tbuff, chunk);
        start += chunk;
        end -= chunk;
      }
#ifdef USE_RPMALLOC
      rpfree(tbuff);
#else
      lives_free(tbuff);
#endif
      return TRUE;
    }
  }

  /// halve the number of bytes, since we will work forwards and back to meet in the middle
  count >>= 1;

  if (count >= 8 && (ocount & 0x07) == 0) {
    // start by swapping 8 bytes from each end
    uint64_t *buff8 = (uint64_t *)buff;
    if ((void *)buff8 == (void *)buff) {
      end = ocount  >> 3;
      for (; count >= 8; count -= 8) {
        /// swap 8 bytes at a time from start and end
        uint64_t tmp8 = buff8[--end];
        if (chunk == 8) {
          buff8[end] = buff8[++start];
          buff8[start] = tmp8;
        } else {
          swab8(&buff8[++start], &buff8[end], chunk);
          swab8(&tmp8, &buff8[start], chunk);
        }
      }
      if (count <= chunk / 2) return TRUE;
      start = (start + 1) << 3;
      start--;
    }
  }

  /// remainder should be only 6, 4, or 2 bytes in the middle
  if (chunk >= 8) return FALSE;

  if (count >= 4 && (ocount & 0x03) == 0) {
    uint32_t *buff4 = (uint32_t *)buff;
    if ((void *)buff4 == (void *)buff) {
      if (start > 0) {
        end = (ocount - start) >> 2;
        start >>= 2;
      } else end = ocount >> 2;
      for (; count >= 4; count -= 4) {
        /// swap 4 bytes at a time from start and end
        uint32_t tmp4 = buff4[--end];
        if (chunk == 4) {
          buff4[end] = buff4[++start];
          buff4[start] = tmp4;
        } else {
          swab4(&buff4[++start], &buff4[end], chunk);
          swab4(&tmp4, &buff4[start], chunk);
        }
      }
      if (count <= chunk / 2) return TRUE;
      start = (start + 1) << 2;
      start--;
    }
  }

  /// remainder should be only 6 or 2 bytes in the middle, with a chunk size of 4 or 2 or 1
  if (chunk >= 4) return FALSE;

  if (count > 0) {
    uint16_t *buff2 = (uint16_t *)buff;
    if ((void *)buff2 == (void *)buff) {
      if (start > 0) {
        end = (ocount - start) >> 1;
        start >>= 1;
      } else end = ocount >> 1;
      for (; count >= chunk / 2; count -= 2) {
        /// swap 2 bytes at a time from start and end
        uint16_t tmp2 = buff2[--end];
        if (chunk >= 2) {
          buff2[end] = buff2[++start];
          buff2[start] = tmp2;
        }
        /// swap single bytes
        else {
          swab2(&buff2[++start], &buff2[end], 1);
          swab2(&tmp2, &buff2[start], 1);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (!count) return TRUE;
  return FALSE;
}

#ifdef USE_INTRINSICS
double intrin_resample_vol(float * dst, size_t dst_skip, float * src, double offsd, double scale, float vol) {
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
