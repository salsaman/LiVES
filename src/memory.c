// memory.c
// LiVES
// (c) G. Finch 2019 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include <sys/mman.h> // for mlock()

#include "main.h"

malloc_f lives_malloc;
calloc_f lives_calloc;
realloc_f lives_realloc;
free_f lives_free;
memcpy_f lives_memcpy;
memset_f lives_memset;
memcmp_f lives_memcmp;
memmove_f lives_memmove;

#define _MB_(m) ((m) * 1024 * 1024)

static size_t bmemsize;
static void bigblocks_end(void);
static void smallblocks_end(void);

#define BB_CACHE 512 // frame cache size (MB)
#define BBLOCK_SIZE 8 // bblocksize MB
#define BBLOCKSIZE (_MB_(BBLOCK_SIZE))
#define NBIGBLOCKS (BB_CACHE / BBLOCK_SIZE) // make each block 32MB (def 16 blocks)
static int NBBLOCKS = 0;
static char *bigblocks[NBIGBLOCKS];

#define assert_no_bblock(p) do {		\
    for (int i = 0; i < NBBLOCKS; i++) {	\
      if (p >= bigblocks[i] && p < bigblocks[i] + BBLOCKSIZE) abort();	\
    }} while (0);


/* #if USE_RPMALLOC */
/* void *(*_lsd_calloc_aligned_)(void **memptr, size_t nmemb, size_t size); */
/* #endif */

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

void *lives_slice_alloc0(size_t sz) {
  totalloc += sz;
#if GLIB_CHECK_VERSION(2, 10, 0)
  return g_slice_alloc0(sz);
#endif
  return lives_memset(g_slice_alloc(sz), 0, sz);
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

// a version which can a calback target
boolean lives_nullify_ptr_cb(void *dummy, void *xvpp) {
  lives_nullify_ptr((void **)xvpp);
  return FALSE;
}


LIVES_GLOBAL_INLINE void *lives_steal_pointer(void **pptr) {
  void *ret = NULL;
  if (pptr && (ret = *pptr)) * pptr = NULL;
  return ret;
}

//#if 0
/* typedef struct { */
/*   // if set then *ptr_to points to real memory and should be freed */
/*   // free (*ptr); ptr = NULL; */
/*   // else just ptr = NULL; */
/*   // ptr_from is unique, but ptr_to can be replicated */
/*   uint8_t isr; */
/*   void **ptr_to; */
/*   void **ptr_from; */
/* } lives_weak; */

/* static LiVESList *weak_list = NULL; */

/* LIVES_GLOBAL_INLINE void make_weak_ptr(void **ptr_new, void **ptr_old) { */
/*   // if we create a weak ptr of type (void *)a = (void)*b, we can have problems */
/*   // if ptr a is freed, via freep((void**)&a), then b will point to freed memory */
/*   // thus we can use MAKE_WEAK(&a, &b); instead. then we must call LIVES_NULLIFY(&a) */
/*   // or LIVES_NULLIFY(&b), now both if both pointers are still pointing to the same memory, they */
/*   // will be nullified together, and the real target will only be freed once. */
/*   // also calling WEAK_UPPDATE(&a) or WEAK_UPDATE(&b) */

/*   // for now it is simple and just goes 1 level deep */
/*   if (ptr_new) { */
/*     lives_weak *weak = (lives_weak *)lives_calloc(1, sizeof(lives_weak)); */
/*     weak->weak = ptr_new; */
/*     weak->real = ptr_old; */
/*     *ptr_new = *ptr_old; */
/*     weak_list = lives_list_prepend(weak_list, &weak); */
/*   } */
/* } */


/* LIVES_GLOBAL_INLINE void **get_other_ptr(void **ptr, int op) { */
/*   // op == 0, return real ptr */
/*   // op == 1 return and remove node */

/*   // TODO */
/*   // op == 1, return real ptr recursively */

/*   // op = 2, as 1, but nullify it and its partner */

/*   // op 3, as 2 but then search for any other weak pointers */
/*   // - find lowest level realptr, nullify, then coming back up, if it was straight, */
/*   // find any other weaks pointed to it recursively, and nullify */
/*   // if it was crooked, skip over. until we return to top level, then free target) */

/*   // op = 2, remove a single node */

/*   LiVESList *list = weak_list; */
/*   lives_weak *xweak; */
/*   void **weak, void **real; */
/*   FIND_BY_DATA_2FIELD(list, lives_weak, weak, real, ptr); */
/*   if (!list) return NULL; */
/*   xweak = (lives_weak *)list->data; */
/*   weak = xweak->weak; */
/*   real = xweak->real; */
/*   if (op == 1) weak_list = lives_list_remove_node(weak_list, xweak, TRUE); */
/*   if (*weak != *real) return NULL; */
/*   if (ptr == real) return weak; */
/*   // is this what they call "tail recursion" ?? */
/*   //real = get_real_ptr(weak->real); */
/*   return real; */
/* } */


/* LIVES_GLOBAL_INLINE boolean is_weak_ptr(void **ptrptr) { */
/*   LiVESList *list = weak_list; */
/*   return !!(FIND_BY_DATA_FIELD(list, lives_weak, weak, ptrptr)); */
/* } */


/* void *lives_nullify_weak_check(void **pp) { */
/*   if (weak_list) { */
/*     other = get_other_ptr(ptr. 1); */
/*     if (other) *other = NULL; */
/*   } */
/*   return 0; */
/* } */

/* #endif */

LIVES_GLOBAL_INLINE boolean lives_freep(void **ptr) {
  // free a pointer and nullify it, only if it is non-null to start with
  // pass the address of the pointer in
  // WARNING !! if ptr == WEAK(other_ptr) then other_ptr will not be set to NULL !!
  static pthread_mutex_t freep_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&freep_mutex);
  if (ptr && (volatile void *)*ptr) {
    lives_free(*ptr);
    //lives_nullify_weak_check(ptr);
    *ptr = NULL;
    pthread_mutex_unlock(&freep_mutex);
    return TRUE;
  }
  pthread_mutex_unlock(&freep_mutex);
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_free_if_non_null(void *p) {
  if (p) {
    lives_free(p);
    return TRUE;
  }
  return FALSE;
}

// functions with fixed pointers that we can pass to plugins ///

void *_ext_malloc(size_t n) {
#if USE_RPMALLOC
  return rpmalloc(n);
#else
  return (n == 0 ? NULL : _lives_malloc(n));
#endif
}

void *_ext_malloc_and_copy(size_t bsize, const void *block) {
  if (!block || bsize == 0) return NULL;
#ifdef lives_malloc_and_copy
  return lives_malloc_and_copy(bsize, block);
#endif
  return (copy_if_nonnull(_lives_malloc(bsize), block, bsize));
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
#if USE_RPMALLOC
  rpfree(p);
#else
  if (p) _lives_free(p);
#endif
}

void *_ext_free_and_return(void *p) {if (p) _ext_free(p); return NULL;}

void *_ext_memcpy(void *dest, const void *src, size_t n) {return _lives_memcpy(dest, src, n);}

int _ext_memcmp(const void *s1, const void *s2, size_t n) {return _lives_memcmp(s1, s2, n);}

void *_ext_memset(void *p, int i, size_t n) {return _lives_memset(p, i, n);}

void *_ext_memmove(void *dest, const void *src, size_t n) {return _lives_memmove(dest, src, n);}

void *_ext_realloc(void *p, size_t n) {
#if USE_RPMALLOC
  return rprealloc(p, n);
#else
  return _lives_realloc(p, n);
#endif
}

void *_ext_calloc(size_t nmemb, size_t msize) {
#if USE_RPMALLOC
  size_t align = HW_ALIGNMENT;
  return rpaligned_calloc(align, nmemb, msize);
#else
  return _lives_calloc(nmemb, msize);
#endif
}

LIVES_GLOBAL_INLINE void *lives_free_and_return(void *p) {if (p) lives_free(p); return NULL;}

size_t PAGESIZE = 0;
static size_t hwlim = 0;

////////////// small allocators //////////////
// not suitable for multiple threads - therefor when allocating, we try to gtab the mutex, if not possible
// use a normal malloc, then when freeing we just check if the ptr is in range

#define CHUNK_SIZE (smblock_pool->chunk_size)
#define TOT_CHUNKS (smblock_pool->num_chunks)
#define FREE_CHUNKS (smblock_pool->free_chunks)
#define FREE_BYTES (FREE_CHUNKS * CHUNK_SIZE)
#define USED_CHUNKS (TOT_CHUNKS - FREE_CHUNKS)
#define USED_BYTES (USED_CHUNKS * CHUNK_SIZE)
#define USED_PERCENT (USED_CHUNKS / TOT_CHUNKS * 100.)
#define MAX_SIZE (smblock_pool->max_size)
#define TOO_BIG_SIZE (smblock_pool->toobig_size)

#define GET_BLOCK(node) ((alloc_block_t *)((node)->data))

#define GET_BLOCK_SIZE(node) (GET_BLOCK(node)->size)
#define GET_BLOCK_OFFS(node) (GET_BLOCK(node)->offs)

#define GET_BYTE_SIZE(node) (GET_BLOCK_SIZE(node) * CHUNK_SIZE)

#define IS_FREE(node) (GET_BLOCK_SIZE(node) > 0)

typedef struct {
  void *buffer;
  int chunk_size;
  int num_chunks;
  int free_chunks;
  int max_size;
  LiVESList *chunk_list;
  LiVESList *first_avail_chunk;
  pthread_mutex_t mutex;
  size_t toobig_size;
} mem_pool_t;


typedef struct {
  int size, offs;
  pthread_mutex_t mutex;
} alloc_block_t;


static malloc_f orig_malloc;
static calloc_f orig_calloc;
static realloc_f orig_realloc;
static free_f orig_free;

static mem_pool_t _smblock_pool;
static mem_pool_t *smblock_pool = &_smblock_pool;

// nmeb is 0 for malloc, > 0 for calloc
// ptrs are always laigned on cacheline size

static alloc_block_t *make_alloc_block(int size, int offs) {
  // have to use std malloc her, else we would end up with an infinite recursion
  alloc_block_t *block = _lives_malloc(sizeof(alloc_block_t));
  block->size = size;
  block->offs = offs;
  return block;
}

static void *_speedy_alloc(size_t nmemb, size_t xsize) {
  if (!nmemb) {
    if (xsize <= 0) return NULL;
    if (PAGESIZE && xsize >= PAGESIZE) return lives_malloc_medium(xsize);
    if (xsize > MAX_SIZE || (TOO_BIG_SIZE > 0 && xsize >= TOO_BIG_SIZE)) return orig_malloc(xsize);
    pthread_mutex_lock(&smblock_pool->mutex);
  } else {
    if (PAGESIZE && xsize * nmemb  >= PAGESIZE) return lives_calloc_medium(nmemb * xsize);
    if (xsize > MAX_SIZE || (TOO_BIG_SIZE > 0 && xsize >= TOO_BIG_SIZE)) return orig_calloc(nmemb, xsize);

    pthread_mutex_lock(&smblock_pool->mutex);
    xsize *= nmemb;
  }
  // Find a free block of memory with enough contiguous chunks
  // a more effient way memory wise would be to find the smallest block large enough
  // nxtchunks > 0 - free size following
  // nxthchunks < 0 - allocated size following

  void *ptr = NULL;
  off_t offs = GET_BLOCK_OFFS(smblock_pool->first_avail_chunk);
  // round up to next multiple of chunk size
  int nchunks_req = (xsize + CHUNK_SIZE - 1) / CHUNK_SIZE;
  LiVESList *list = smblock_pool->first_avail_chunk;
  int nxtchunks;

  for (; list; list = list->next) {
    nxtchunks = GET_BLOCK_SIZE(list);
    if (nxtchunks > 0) {
      if (nxtchunks > GET_BLOCK_SIZE(smblock_pool->first_avail_chunk))
        smblock_pool->first_avail_chunk = list;
      if (nxtchunks >= nchunks_req) break;
      offs += nxtchunks;
    } else offs -= nxtchunks;
  }

  if (list) {
    // mark n chunks allocted
    GET_BLOCK_SIZE(list) = -nchunks_req;
    // the remainder is carried over to the end of the allocated block
    nxtchunks -= nchunks_req;
    if (nxtchunks > 0) {
      // if anything is left over, we need to append a new node
      // this is because the following node after a free block node must be an allocated node
      // thus we cannot simply add the remainder to the next block
      alloc_block_t *block = make_alloc_block(nxtchunks, offs + nchunks_req);
      list = lives_list_append(list, block);
      if (smblock_pool->first_avail_chunk == list)
        smblock_pool->first_avail_chunk = list->next;
    } else {
      if (smblock_pool->first_avail_chunk == list)
        smblock_pool->first_avail_chunk = list->next->next;
    }
    smblock_pool->free_chunks -= nchunks_req;

    pthread_mutex_unlock(&smblock_pool->mutex);

    // zero out the newly allocated block
    ptr = (char *)smblock_pool->buffer + offs * CHUNK_SIZE;
    if (nmemb) lives_memset(ptr, 0, nchunks_req * CHUNK_SIZE);

    g_print("ALLOC of %d chunks @ %p, free space is %d of %d, %.2f %% used (%.2f MiB)\n", nchunks_req, ptr,
            smblock_pool->num_chunks - smblock_pool->free_chunks, smblock_pool->num_chunks,
            (double)(smblock_pool->num_chunks - smblock_pool->free_chunks)
            / (double)smblock_pool->num_chunks * 100.,
            (smblock_pool->num_chunks - smblock_pool->free_chunks) / 1000000.);

    return  ptr;
  }

  // insufficient space
  smblock_pool->toobig_size = xsize;
  pthread_mutex_unlock(&smblock_pool->mutex);
  if (!nmemb) return orig_malloc(xsize);
  return orig_calloc(nmemb, xsize / nmemb);
}


void *speedy_malloc(size_t xsize) {
  if (!xsize) break_me("bad malloc");
  return _speedy_alloc(0, xsize);
}



void *speedy_calloc(size_t nm, size_t xsize) {
  if (!nm || !xsize) break_me("bad calloc");
  void *ptr = _speedy_alloc(nm, xsize);
  return ptr;
}


// Free memory back to memory smblock_pool
// startin from beginning, count offesets until we reach the node for the ptr to be freed
// this will be set to -N (siz in chunks allocated which now become freed
// if the next node has a positive value, we add that value to N and remove the node
// otherwise we just set this node to +N
void speedy_free(void *ptr) {
  int nchunks = 0,  xxsize;
  off_t offs = 0, toffs = ((char *)ptr - (char *)smblock_pool->buffer);
  if (toffs < 0 || toffs >= TOT_CHUNKS * CHUNK_SIZE) {
    orig_free(ptr);
    g_print("pt aa555\n");
    return;
  }
  pthread_mutex_lock(&smblock_pool->mutex);
  for (LiVESList *list = smblock_pool->chunk_list; list; list = list->next) {
    nchunks = GET_BLOCK_SIZE(list);
    offs += abs(nchunks) * CHUNK_SIZE;
    if (offs == toffs) {
      if (nchunks >= 0) {
        // double free or corruption...
        char *msg = lives_strdup_printf("Double free ot corruption in speedy_free, ptr %p points to "
                                        "a free block of size %d chunks\n", ptr, nchunks);
        pthread_mutex_unlock(&smblock_pool->mutex);
        LIVES_FATAL(msg);
        return;
      }

      nchunks = -nchunks;

      if (nchunks > GET_BLOCK_SIZE(smblock_pool->first_avail_chunk))
        smblock_pool->first_avail_chunk = list;

      smblock_pool->free_chunks += nchunks;
      if (list->next) {
        int nxtchunks = GET_BLOCK_SIZE(list->next);
        // merge node and next node
        if (nxtchunks > 0) {
          nchunks += nxtchunks;
          if (list->next->next) {
            list->next->next->prev = list;
            list->next = list->next->next;
          }
          list->next->next = list->next->prev = NULL;
          lives_list_free_all(&list->next);
        }
      }

      if (list->prev) {
        int prevchunks = GET_BLOCK_SIZE(list->prev);
        if (prevchunks > 0) {
          // merge node and prev node
          nchunks += prevchunks;
          GET_BLOCK_SIZE(list->prev) = nchunks;
          list->prev->next = list->next;
          if (list->next) list->next->prev = list->prev;
          list->next = list->prev = NULL;
          lives_list_free_all(&list);
        }
      } else GET_BLOCK_SIZE(list) = nchunks;
      xxsize = nchunks * CHUNK_SIZE;
      if (xxsize > TOO_BIG_SIZE) TOO_BIG_SIZE = xxsize + 1;

      pthread_mutex_unlock(&smblock_pool->mutex);

      g_print("FREE of %d chunks @ %p, free space is %d of %d, %.2f %% used (%.2f MiB)\n", -nchunks, ptr,
              smblock_pool->num_chunks - smblock_pool->free_chunks, smblock_pool->num_chunks,
              (double)(smblock_pool->num_chunks - smblock_pool->free_chunks)
              / (double)smblock_pool->num_chunks * 100.,
              (smblock_pool->num_chunks - smblock_pool->free_chunks) / 1000000.);
      return;
    }
  }
  abort();
}

static  void *speedy_realloc(void *op, size_t xsize) {
  // we have various options in order of preference:
  // shrink size - just adjust nchunks, add extra blocks or append next node
  // fit in nchunks + nxtchunks - extend block, reduce count for next node, or delete if 0
  // fit in prevchunks - reduce and negate, assign extra blocks to this node (if any), copy chunks with memmove
  //    merge nxtchunks and delete nxt node
  // fit in prevhunks + nchunks = set prevchunks to - req., reduce out chunks, if 0 remove
  //  else merge with nxt
  // fit in prev + nchunks + nxt - set prev to - req, remove this node,
  //   reduxe chunks in nct node, if 0, delete it

  g_print("realloc\n");

  int nchunks_req, nchunks, onchunks, nxtchunks = 0, prevchunks = 0;
  off_t offs = 0, toffs;

  if (!op) return lives_malloc(xsize);

  // this acts like a free followed by a malloc, except for the following differences
  //- if new size is smaller, we reduce the -ve chunk size and add the remainder to the next block (if unassigned,
  // or append a new unassinged block
  // if growing, we check if there is an unallocated block following, if so we just increase the -ve value
  // and reduce the +ve from next
  // if the space is not enough we try to add prev block and do a memmove
  // if this is still not enough, we do a normal alloc followied by a copy
  g_print("REE2\n");

  toffs = ((char *)op - (char *)smblock_pool->buffer);
  if (toffs < 0 || toffs >= TOT_CHUNKS * CHUNK_SIZE)
    return orig_realloc(op, xsize);

  nchunks_req = (xsize + smblock_pool->chunk_size - 1) / smblock_pool->chunk_size;

  g_print("REE2332\n");
  pthread_mutex_lock(&smblock_pool->mutex);
  for (LiVESList *list = smblock_pool->chunk_list; list; list = list->next) {
    nchunks = -GET_BLOCK_SIZE(list);
    offs += abs(nchunks) * CHUNK_SIZE;
    if (offs == toffs) {
      if (nchunks <= 0) {
        // double free or corruption...
        pthread_mutex_unlock(&smblock_pool->mutex);
        return NULL;
      }

      if (nchunks == nchunks_req) {
        pthread_mutex_unlock(&smblock_pool->mutex);
        return op;
      }

      onchunks = nchunks;

      if (list->next) {
        nxtchunks = GET_BLOCK_SIZE(list->next);
        if (nxtchunks < 0) nxtchunks = 0;
      }
      g_print("REE232323\n");

      if (nchunks + nxtchunks >= nchunks_req) {
        // new alloc fits in node + next
        GET_BLOCK_SIZE(list) = -nchunks_req;
        nchunks -= nchunks_req;
        smblock_pool->free_chunks += nchunks;
        nchunks += nxtchunks;
        if (nxtchunks) {
          // add excess chunks to nxt
          GET_BLOCK_SIZE(list->next) = nchunks;
        } else {
          // append
          alloc_block_t *block = make_alloc_block(nchunks, offs / CHUNK_SIZE + nchunks_req);
          LiVESList *nlist = lives_list_append(NULL, block);
          if (list->next) {
            nlist->next = list->next;
            list->next->prev = nlist;
          }
          nlist->prev = list;
          list->next = nlist;
        }
        if (nchunks * smblock_pool->chunk_size > smblock_pool->toobig_size)
          smblock_pool->toobig_size = nchunks * smblock_pool->chunk_size;
        pthread_mutex_unlock(&smblock_pool->mutex);
        g_print("REE2ewew\n");
        return op;
      }

      if (list->prev) {
        void *nptr;
        prevchunks = GET_BLOCK_SIZE(list->prev);
        if (prevchunks < 0) prevchunks = 0;
        if (prevchunks) {
          // subsume next node into this
          nchunks += prevchunks;
          if (nchunks >= nchunks_req) {
            GET_BLOCK_SIZE(list->prev) = nchunks_req;
            nchunks -= nchunks_req;
            if (nchunks) GET_BLOCK_SIZE(list) = nchunks;
            else {
              if (list->next) list->next->prev = list->prev;
              list->prev->next = list->next;
              list->next = list->prev = NULL;
              lives_list_free_all(&list);
            }
            g_print("REewweweE2\n");
            nptr = op - prevchunks * smblock_pool->chunk_size;
            g_print("REE2cdsdcd\n");

            lives_memmove(nptr, op, onchunks * smblock_pool->chunk_size);
            smblock_pool->free_chunks -= nchunks_req - onchunks;
            pthread_mutex_unlock(&smblock_pool->mutex);
            g_print("REEeeee2\n");
            return nptr;
          } else {
            if (list->prev->prev)list->prev->prev->next = list;
            else smblock_pool->chunk_list = list;
            list->prev = list->prev->prev;
          }
          list->prev->next = list->prev->prev = NULL;
          lives_list_free_all(&list->prev);
        }

        pthread_mutex_unlock(&smblock_pool->mutex);
        g_print("REwwwwE2\n");
        nptr = speedy_malloc(xsize);
        g_print("REsssssE2\n");
        pthread_mutex_lock(&smblock_pool->mutex);
        g_print("REdsdasdsaE2\n");
        lives_memcpy(nptr, op, onchunks * smblock_pool->chunk_size);
        GET_BLOCK_SIZE(list) = nchunks;
        smblock_pool->free_chunks -= nchunks_req - onchunks;
        pthread_mutex_unlock(&smblock_pool->mutex);
        g_print("REeeeeeeeeeeeeeE2\n");
        return nptr;
      }
    }
  }
  // INVALID PTR
  g_print("RooooEeeeeeeeeeeeeeE2\n");
  pthread_mutex_unlock(&smblock_pool->mutex);
  return NULL;
}


#define N_SMCHUNKS 65536 * 64


static void smallblocks_end(void) {
  pthread_mutex_lock(&smblock_pool->mutex);
  lives_free = orig_free;
  lives_malloc = orig_malloc;
  lives_calloc = orig_calloc;
  lives_realloc = orig_realloc;
  pthread_mutex_unlock(&smblock_pool->mutex);
  munlock(smblock_pool->buffer, TOT_CHUNKS * CHUNK_SIZE);
  lives_free(smblock_pool->buffer);
}


void smallblock_init(void) {
  size_t memsize;
  hwlim = HW_ALIGNMENT;
  memsize = N_SMCHUNKS * hwlim;

  if (PAGESIZE) memsize = (size_t)(memsize / PAGESIZE) * PAGESIZE;

  smblock_pool->buffer = lives_calloc_medium(memsize);

  if (smblock_pool->buffer) {
    if (mlock(smblock_pool->buffer, memsize)) {
      lives_free(smblock_pool->buffer);
      return;
    }

    smblock_pool->chunk_size = hwlim;
    TOT_CHUNKS = memsize / hwlim;

    // start off with all memory unallocated
    alloc_block_t *block = make_alloc_block(smblock_pool->num_chunks, 0);
    smblock_pool->chunk_list = lives_list_append(NULL, block);
    smblock_pool->first_avail_chunk = smblock_pool->chunk_list;

    if (PAGESIZE) MAX_SIZE = PAGESIZE / CHUNK_SIZE;
    else MAX_SIZE = TOT_CHUNKS;

    smblock_pool->free_chunks = smblock_pool->num_chunks;

    pthread_mutex_init(&smblock_pool->mutex, NULL);
    pthread_mutex_lock(&smblock_pool->mutex);
    orig_free = lives_free;
    orig_malloc = lives_malloc;
    orig_calloc = lives_calloc;
    orig_realloc = lives_realloc;

    lives_free = speedy_free;
    lives_malloc = speedy_malloc;
    lives_calloc = speedy_calloc;
    lives_realloc = speedy_realloc;
    pthread_mutex_unlock(&smblock_pool->mutex);
  }
}


char *get_memstats(void) {
  char *msg;


  if (smblock_pool) msg = lives_strdup_printf("smallblock: total size %d, block size %d, page_size = %ld, "
                            "cachline_size = %d\n"
                            "Blocks in use: %d of %d (%.2f %%)\n",
                            TOT_CHUNKS * CHUNK_SIZE, CHUNK_SIZE,
                            capable->hw.pagesize, capable->hw.cacheline_size,
                            smblock_pool->num_chunks - smblock_pool->free_chunks, smblock_pool->num_chunks,
                            (double)(smblock_pool->num_chunks - smblock_pool->free_chunks)
                            / (double)smblock_pool->num_chunks * 100.);
  else msg = lives_strdup_printf("smallblock not in use\n");
  return msg;
}

/////////////////////// medium allocators ////////////

LIVES_LOCAL_INLINE void *lives_malloc_aligned(size_t nblocks, size_t align) {
  void *p = aligned_alloc(align, nblocks * align);
  return p;
}

LIVES_GLOBAL_INLINE void *lives_malloc_medium(size_t msize) {
  void *p;
  if (!PAGESIZE || msize < PAGESIZE) return default_malloc((msize + HW_ALIGNMENT - 1) / HW_ALIGNMENT * HW_ALIGNMENT);
  p = lives_malloc_aligned((msize + PAGESIZE - 1) / PAGESIZE, PAGESIZE);
  return p;
}

LIVES_GLOBAL_INLINE void *lives_calloc_medium(size_t msize) {
  void *p;
  if (!PAGESIZE || msize < PAGESIZE) return default_calloc((msize + HW_ALIGNMENT - 1) / HW_ALIGNMENT, HW_ALIGNMENT);
  p = lives_malloc_medium(msize);
  if (p) lives_memset(p, 0, msize);
  return p;
}

///////////////////////////////////////////////

/// for sizes < PAGESIZE, we use HW_ALIGNMENT, which is either cacheline size, or a default (64)
// if we dont have that

LIVES_GLOBAL_INLINE void *lives_calloc_align(size_t xsize) {
  if (!xsize) return NULL;
  if (xsize >= PAGESIZE) return lives_calloc_medium(xsize);
  else {
    size_t align = HW_ALIGNMENT;
    xsize = ((xsize + align - 1) / align);
    return default_calloc(xsize, align);
  }
}


LIVES_GLOBAL_INLINE void *lives_calloc_safety(size_t nmemb, size_t xsize) {
  return lives_calloc_align(nmemb * xsize + EXTRA_BYTES);
}


LIVES_GLOBAL_INLINE void *lives_recalloc(void *op, size_t nmemb, size_t omemb, size_t xsize) {
  /// realloc from omemb * size to nmemb * size
  if (nmemb <= omemb) return op;
#if 0
  if (IN_SMALLBLOCK(op)) {
    int i = GET_BLOCK(op);
    int nblocks = (GET_SIZE(i));
    if (i + nblocks < n_smblocks && !IN_USE(i + nblocks)) {
      int xblocks = ((nmemb - omemb) * xsize + hwlim - 1) / hwlim;
      int ysize = GET_SIZE(i + nblocks);
      if (!IN_USE(i + nblocks) && xblocks <= ysize) {
        pthread_rwlock_wrlock(&memlist_lock);
        ysize = GET_SIZE(i + nblocks);
        if (!IN_USE(i + nblocks) && xblocks <= ysize) {
          SET_PREV(i + nblocks, i + nblocks - 1);
          SET_NEXT(i + nblocks - 1, i + nblocks);
          SET_SIZE(i, nblocks + xblocks);
          SET_SIZE(i + nblocks, ysize - xblocks);
          smblock_count -= xblocks;
          for (int j = i + nblocks; j < i + nblocks + xblocks; j++) SET_IN_USE(j);
        }
        pthread_rwlock_unlock(&memlist_lock);
        return op;
      }
    }
  }
#endif
  do {
    void *np = lives_calloc_safety(nmemb, xsize);
    if (op && omemb > 0) {
      if (omemb > nmemb) omemb = nmemb;
      lives_memcpy(np, op, omemb * xsize);
      lives_free(op);
    }
    return np;
  } while (FALSE);
}


#define MEMMOVE_LIM 65536

void *lives_memcpy_extra(void *d, const void *s, size_t n) {
  if (n >= MEMMOVE_LIM)
    return lives_memmove(d, s, n);
  return _lives_memcpy(d, s, n);
}


void memory_cleanup(void) {
  bigblocks_end();
  smallblocks_end();
#if USE_RPMALLOC
  rpmalloc_finalize();
#endif
}


boolean init_memfuncs(int stage) {
  if (stage == 0) {
    lives_malloc = _lives_malloc;
    lives_calloc = _lives_calloc;
    lives_realloc = _lives_realloc;
    lives_free = _lives_free;
    lives_memcpy = lives_memcpy_extra;
    lives_memset = _lives_memset;
    lives_memcmp = _lives_memcmp;
    lives_memmove = _lives_memmove;

#if USE_RPMALLOC
    //_lsd_calloc_aligned_ = lsd_calloc_aligned;
    rpmalloc_initialize();
#endif
  }

  else if (stage == 1) {
    // should be called after we have pagesize
    if (capable && capable->hw.pagesize) PAGESIZE = capable->hw.pagesize;
    //smallblock_init();
    bigblock_init();
  }

  return TRUE;
}


static char loc_data32M[_MB_(32)];
static char loc_data8M[_MB_(8)];
static char loc_data1M[_MB_(1)];

void *localise_data(void *src, size_t dsize) {
  if (dsize <= capable->hw.cache_size) {
    g_print("1m is %p\n", loc_data1M);
    if (dsize <= _MB_(1)) {
      if (src) lives_memcpy(loc_data1M, src, dsize);
      else lives_memset(loc_data1M, 0, _MB_(1));
      return (void *)loc_data1M;
    }
    if (dsize <= _MB_(8)) {
      if (src) lives_memcpy(loc_data8M, src, dsize);
      else lives_memset(loc_data8M, 0, _MB_(8));
      return &loc_data8M;
    }
    if (dsize <= _MB_(32)) {
      if (src) lives_memcpy(loc_data32M, src, dsize);
      else lives_memset(loc_data32M, 0, _MB_(32));
      return (void *)loc_data32M;
    }
  }
  return NULL;
}

//#define TRACE_BBALLOC
#ifdef TRACE_BBALLOC
static LiVESList *ustacks[NBIGBLOCKS];
#endif

// blocks can be allocated in groups of 1,2 or 4
static volatile int used[NBIGBLOCKS];

#define NBAD_LIM 8
#define BBL_TEST
#ifdef BBL_TEST
static int bbused = 0;
#endif

static int nbads = 0;

static pthread_mutex_t bigblock_mutex = PTHREAD_MUTEX_INITIALIZER;

const char *bigblock_root = NULL;

static void bigblocks_end(void) {
  for (int i = 0; i < NBIGBLOCKS; i++) {
#ifdef TRACE_BBALLOC
    lives_list_free_all(&ustacks[i]);
#endif
  }

  if (bigblock_root) {
    munlock(bigblock_root, NBIGBLOCKS * bmemsize);
    lives_free((void *)bigblock_root);
  }
}


void bigblock_init(void) {
  char *ptr;

  bmemsize = BBLOCKSIZE;
  hwlim = HW_ALIGNMENT;
  bmemsize = (size_t)(bmemsize / hwlim) * hwlim;

  if (PAGESIZE) {
    bmemsize = (size_t)(bmemsize / PAGESIZE) * PAGESIZE;
  }
  bigblock_root = lives_calloc_medium(bmemsize * NBIGBLOCKS);
  if (!bigblock_root) {
    LIVES_WARN("Unable to allocate bigblock arena");
    return;
  }
  if (mlock(bigblock_root, bmemsize * NBIGBLOCKS)) {
    LIVES_WARN("Unable to mlock bigblock arena");
    lives_free((void *)bigblock_root);
    return;
  }

  ptr = (char *)bigblock_root;

  for (int i = 0; i < NBIGBLOCKS; i++) {
    bigblocks[i] = ptr;
    ptr += bmemsize;
#ifdef TRACE_BBALLOC
    ustacks[NBBLOCKS] = NULL;
    tuid[NBBLOCKS] = 0;
#endif
    used[NBBLOCKS++] = 0;
  }
}


static int get_bblock_idx(void *bstart, off_t *offs, int *bbafter) {
  off_t xoffs = (char *)bstart - (char *)bigblocks[0];

  if (offs) *offs = xoffs;
  if (xoffs < 0 || xoffs > NBBLOCKS * bmemsize) return -1;

  int bbidx = xoffs / bmemsize;
  if (bbafter) *bbafter = bbidx;

  xoffs -= bbidx * bmemsize;
  if (offs) *offs = xoffs;

  if (!xoffs)
    for (int i = 1; i <= bbidx; i++) {
      if (i > 3) break;
      if (used[bbidx - i] > i) {
	if (bbafter) *bbafter = bbidx - i;
	break;
      }
    }
  return bbidx;
}


static int _alloc_bigblock(size_t sizeb, int oblock) {
  int nblocks = 1;
  int i, j, max;
  prefs->show_dev_opts = TRUE;
  if (sizeb > bmemsize) {
    if (sizeb > (bmemsize >> 1)) {
      if (sizeb > (bmemsize >> 2)) {
	if (prefs->show_dev_opts) g_print("msize req %lu > %lu, cannot use bblockalloc\n",
					sizeb, bmemsize >> 2);
	return -2;
      }
      nblocks = 4;
    }
    else nblocks = 2;
  }

  if (oblock >= 0 && nblocks <= used[oblock]) {
#ifdef BBL_TEST
    bbused += nblocks - used[oblock];
#endif
    used[oblock] = nblocks;
    return oblock;
  }

  if (oblock >= 0) i = max = oblock;
  else {
    i = used[0];
    max = NBBLOCKS - nblocks;
  }
  
  while (i <= max) {
    for (j = nblocks - 1; j >= 0; j--) {
      int u = used[i + j];
      //g_print("block %d + %d (%d) used %d blocks\n", i, j, i + j, u);
      if (u) {
	if (oblock != -1) return -1;
	i += u + j;
	break;
      }
    }
    if (j < 0) break;
  }

  if (i <= max) {
#ifdef TRACE_BBALLOC
    if (oblock == -1) {
      ustacks[i] = lives_list_copy_reverse_strings(THREADVAR(func_stack));
      tuid[i] = THREADVAR(uid);
    }
#endif
#ifdef BBL_TEST
    bbused += nblocks - used[i];
#endif
    used[i] = nblocks;
  }

  //if (clear) lives_mesmset(bigblocks[i], 0, sizeb);
  //g_print("ALLOBIG %p size %d\n", bigblocks[i], nblocks);
  g_print("ALLOCBIG has vals %d and %d\n", i, max);
  if (i <= max) return i;

  ///////////////////////////////////////////////////
  //  dump_fn_notes();
  break_me("bblock");
  LIVES_WARN("OUT OF BIGBLOCKS");
#ifdef TRACE_BBALLOC
  for (int i = 0; i < NBBLOCKS; i++) {
    g_printerr("block %d assigned to thread %s\n", i, get_thread_id(tuid[i]));
    dump_fn_stack(ustacks[i]);
  }
#endif
  if (++nbads > NBAD_LIM) LIVES_FATAL("Aborting due to probable internal memory errors");
  ////////////////////////
  return -1;
}

#ifdef DEBUG_BBLOCKS
void *_calloc_bigblock(size_t xsize) {
#else
void *calloc_bigblock(size_t xsize) {
#endif
  int bbidx;
  pthread_mutex_lock(&bigblock_mutex);
  bbidx = _alloc_bigblock(xsize, -1);
  pthread_mutex_unlock(&bigblock_mutex);
  if (bbidx >= 0) {
    lives_memset(bigblocks[bbidx], 0, xsize);
    return bigblocks[bbidx];
  }
  return NULL;
}

  
#ifdef DEBUG_BBLOCKS
void *_malloc_bigblock(size_t xsize) {
#else
void *malloc_bigblock(size_t xsize) {
#endif
  int bbidx;
  pthread_mutex_lock(&bigblock_mutex);
  bbidx = _alloc_bigblock(xsize, -1);
  pthread_mutex_unlock(&bigblock_mutex);
  g_print("got bbidx %d, so returning biblocks[%d], which is %p\n", bbidx, bbidx, bigblocks[bbidx]);
  if (bbidx >= 0) return bigblocks[bbidx];
  return NULL;
}



 
void *realloc_bigblock(void *p, size_t new_size) {
  int bbidx;
  pthread_mutex_lock(&bigblock_mutex);
  int oblock = get_bblock_idx(p, NULL, NULL);
  bbidx = _alloc_bigblock(new_size, oblock);
  pthread_mutex_unlock(&bigblock_mutex);
  if (bbidx == oblock) return p;
  return NULL;
}


#ifdef DEBUG_BBLOCKS
void *_free_bigblock(void *bstart) {
#else
void *free_bigblock(void *bstart) {
#endif
  char *msg;
  off_t offs;
  int bbidx, bbafter;

  if (bstart == mainw->debug_ptr) {
    break_me("badfree");
  }

  pthread_mutex_lock(&bigblock_mutex);
  bbidx = get_bblock_idx(bstart, &offs, &bbafter);

  if (bbidx < 0) {
    pthread_mutex_unlock(&bigblock_mutex);
    msg = lives_strdup_printf("Invalid free of bigblock, %p not in range %p -> %p\n",
			      bstart, bigblocks[0], bigblocks[0] + NBBLOCKS * bmemsize);
    LIVES_WARN(msg);
    lives_free(msg);
    lives_free(bstart);
    return bstart;
  }

  if (offs) {
    pthread_mutex_unlock(&bigblock_mutex);
    msg = lives_strdup_printf("Invalid free of bigblock, %p is at offset %ld after block %d\n",
			      bstart, offs, bbafter);
    LIVES_FATAL(msg);
    return NULL;
  }

  if (bbafter != bbidx) {
    msg = lives_strdup_printf("Invalid free of bigblock, %p is %d blocks after block %d, with size %d blocks\n",
			      bstart, bbidx - bbafter, bbafter, used[bbafter]);
    pthread_mutex_unlock(&bigblock_mutex);
    LIVES_FATAL(msg);
    return NULL;
  }

#ifdef BBL_TEST
  bbused -= used[bbidx];
  g_print("bigblocks in use after free %d\n", bbused);
#endif
  used[bbidx] = 0;
#ifdef TRACE_BBALLOC
  lives_list_free_all(&ustacks[bbidx]);
  tuid[bbidx] = 0;
#endif
  pthread_mutex_unlock(&bigblock_mutex);
  g_print("FREEBIG %p %d\n", bigblocks[bbidx], bbidx);
  return bstart;
}


union split8 {
  uint64_t u64;
  uint32_t u32[2];
};

union split4 {
  uint32_t u32;
  uint16_t u16[2];
};


// src is divided into groups of bytesize gran(ularity)
// the groups from src are then copied to dest in reverse sequence

// gran(ularity) may be 1, or 2
LIVES_GLOBAL_INLINE void swab2(void *to, const void *from, size_t gran) {
  char tmp[2];
  if (gran == 1) {
    swab(from, tmp, 2);
    from = &tmp;
  }
  if (to != from) lives_memcpy(to, from, 2);
}

// gran(ularity) may be 1, 2 or 4
LIVES_GLOBAL_INLINE void swab4(const void *to, const void *from, size_t gran) {
  union split4 *d = (union split4 *)to, s;
  uint16_t tmp;

  if (gran > 2) {
    lives_memcpy((void *)to, from, 4);
    return;
  }
  s.u32 = *(uint32_t *)from;
  tmp = s.u16[0];
  if (gran == 2) {
    // abcd -> cdab
    d->u16[0] = s.u16[1];
    d->u16[1] = tmp;
  } else {
    s.u32 = *(uint32_t *)from;
    tmp = s.u16[0];
    // abcd -> dcba
    swab2(&d->u16[0], &s.u16[1], 1);
    swab2(&d->u16[1], &tmp, 1);
  }
}


// gran(ularity) may be 1, 2 or 4
LIVES_GLOBAL_INLINE void swab8(const void *to, const void *from, size_t gran) {
  union split8 *d = (union split8 *)to, s;
  uint32_t tmp;
  if (gran > 4) {
    lives_memcpy((void *)to, from, 8);
    return;
  }
  s.u64 = *(uint64_t *)from;
  tmp = s.u32[0];
  if (gran == 4) {
    // abcdefgh -> efghabcd
    d->u32[0] = s.u32[1];
    d->u32[1] = tmp;
  } else {
    // 2             1
    // ghefcdab or hgfedcba
    swab4(&d->u32[0], &s.u32[1], gran);
    swab4(&d->u32[1], &tmp, gran);
  }
}


boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) {
  // reverse chunk sized bytes in buff, count must be a multiple of chunk
  ssize_t start = -1, end;
  size_t ocount = count;

  if (chunk <= 8) {
    if ((chunk != 8 && chunk != 4 && chunk != 2 && chunk != 1)
        || (count % chunk) != 0) return FALSE;
  } else {
    if ((chunk & 0x01) || (count % chunk) != 0) return FALSE;
    else {
#if USE_RPMALLOC
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
#if USE_RPMALLOC
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
          swab8(&buff8[end], &buff8[++start], chunk);
          swab8(&buff8[start], &tmp8, chunk);
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
          swab4(&buff4[end], &buff4[++start], chunk);
          swab4(&buff4[start], &tmp4, chunk);
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
          swab2(&buff2[end], &buff2[++start], 1);
          swab2(&buff2[start], &tmp2, 1);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (!count) return TRUE;
  return FALSE;
}

#ifdef INTRINSICS_TEST
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

#define OIL_MEMCPY_MAX_BYTES 12288 // this can be tuned to provide optimal performance

#ifdef ENABLE_ORC
livespointer lives_orc_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  static size_t maxbytes = OIL_MEMCPY_MAX_BYTES;

#if AUTOTUNE_MALLOC_SIZES
  static weed_plant_t *tuner = NULL;
  static boolean tuned = FALSE;
  static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
  boolean haslock = FALSE;
#endif

  if (n == 0) return dest;
  if (n < 32) return memcpy(dest, src, n);

#if AUTOTUNE_MALLOC_SIZES
  /// autotuning: first of all we provide the tuning parameters:
  /// (opaque) weed_plant_t *tuner, (int64_t)min range, (int64_t)max range, (int)ntrials,(double) cost
  /// the tuner will time from here until autotune_end and multiply the cost by the time
  /// we also reveal the value of the variable in autotune_end
  /// the tuner will run this ntrials times, then select a new value for the variable which is returned
  /// the costs for each value are totalled and averaged
  /// and finally the value with the lowest average cost / time is selected
  /// in this case what we are tuning is the bytesize threshold to select between one memory allocation function and another
  /// the cost in both cases is defined is 1.0 / n where n is the block size.
  /// The cost is the same for both functions - since time is also a factor
  /// the value should simply be the one with the lowest time per byte
  /// obviously this is very simplistic since there are many other costs than simply the malloc time
  /// however, it is a simple matter to adjust the cost calculation

  if (!mainw->multitrack && !LIVES_IS_PLAYING) {
    if (!tuned && !tuner) tuner = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 2);
    if (tuner) {
      if (!pthread_mutex_trylock(&tuner_mutex)) {
        haslock = TRUE;
      }
    }
  }
#endif

  if (maxbytes > 0 ? n <= maxbytes : n >= -maxbytes) {
#if AUTOTUNE_MALLOC_SIZES
    if (haslock) autotune_u64_start(tuner, _MB_(-1), _MB_(1), 32);
#endif

    orc_memcpy((uint8_t *)dest, (const uint8_t *)src, n);

#if AUTOTUNE_MALLOC_SIZES
    if (haslock) {
      maxbytes = autotune_u64_end(&tuner, maxbytes, 1. / (double)n);
      if (!tuner) tuned = TRUE;
      pthread_mutex_unlock(&tuner_mutex);
    }
#endif

    return dest;
  }

#if AUTOTUNE_MALLOC_SIZES
  if (haslock) autotune_u64_start(tuner, _MB_(-1), _MB_(1), 128);
#endif

  def_memcpy(dest, src, n);

#if AUTOTUNE_MALLOC_SIZES
  if (haslock) {
    maxbytes = autotune_u64_end(&tuner, maxbytes, 1. / (double)n);
    if (!tuner) tuned = TRUE;
    pthread_mutex_unlock(&tuner_mutex);
  }
#endif

  return dest;
}
#endif


#ifdef ENABLE_OIL
livespointer lives_oil_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  static size_t maxbytes = OIL_MEMCPY_MAX_BYTES;

#if AUTOTUNE_MALLOC_SIZES
  static weed_plant_t *tuner = NULL;
  static boolean tuned = FALSE;
  static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
  boolean haslock = FALSE;
#endif

  if (n == 0) return dest;
  if (n < 32) return def_memcpy(dest, src, n);

#if AUTOTUNE_MALLOC_SIZES
  if (!mainw->multitrack && !LIVES_IS_PLAYING) {
    if (!tuned && !tuner) tuner = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 2);
    if (tuner) {
      if (!pthread_mutex_trylock(&tuner_mutex)) {
        haslock = TRUE;
      }
    }
  }
#endif

  if (maxbytes > 0 ? n <= maxbytes : n >= -maxbytes) {

#if AUTOTUNE_MALLOC_SIZES
    if (haslock) autotune_u64_start(tuner, _MB_(-1), _MB_(1), 32);
#endif

    oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);

#if AUTOTUNE_MALLOC_SIZES
    if (haslock) {
      maxbytes = autotune_u64_end(&tuner, 1. / (double)n);
      if (!tuner) tuned = TRUE;
      pthread_mutex_unlock(&tuner_mutex);
    }
#endif
    return dest;
  }
#if AUTOTUNE_MALLOC_SIZES
  if (haslock) autotune_u64_start(tuner, _MB_(-1), _MB_(1), 128);
#endif

  def_memcpy(dest, src, n);

#if AUTOTUNE_MALLOC_SIZES
  if (haslock) {
    maxbytes = autotune_u64_end(&tuner, maxbytes, 1. / (double)n);
    if (!tuner) tuned = TRUE;
    pthread_mutex_unlock(&tuner_mutex);
  }
#endif
  return dest;
}

#endif

