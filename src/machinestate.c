// machinestate.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#include <sys/statvfs.h>
#include <malloc.h>
#include "main.h"
#include "support.h"
#include "callbacks.h"

void init_random() {
  ssize_t randres = -1;
  uint64_t rseed;
  int randfd;

  // try to get randomness from /dev/urandom
  randfd = lives_open2("/dev/urandom", O_RDONLY);

  if (randfd > -1) {
    randres = read(randfd, &rseed, 8);
    close(randfd);
  }

  gettimeofday(&tv, NULL);
  rseed += tv.tv_sec + tv.tv_usec;

  lives_srandom((uint32_t)(rseed & 0xFFFFFFFF));

  randfd = lives_open2("/dev/urandom", O_RDONLY);

  if (randfd > -1) {
    randres = read(randfd, &rseed, 8);
    close(randfd);
  }

  if (randres != 8) {
    gettimeofday(&tv, NULL);
    rseed = tv.tv_sec + tv.tv_usec;
  }

  fastsrand(rseed);
}


//// AUTO-TUNING ///////

struct _decomp {
  uint64_t value;
  int i, j;
};

struct _decomp_tab {
  uint64_t value;
  int i, j;
  struct _decomp_tab *lower,  *higher;
};

static struct _decomp_tab nxttbl[64][25];
static boolean nxttab_inited = FALSE;

void make_nxttab(void) {
  LiVESList *preplist = NULL, *dccl, *dccl_last = NULL;
  uint64_t val6 = 1ul, val;
  struct _decomp *dcc;
  int max2pow, xi, xj;
  if (nxttab_inited) return;
  for (int j = 0; j < 25; j++) {
    val = val6;
    max2pow = 64 - ((j * 10 + 7) >> 2);
    dccl = preplist;
    for (int i = 0; i < max2pow; i++) {
      dcc = (struct _decomp *)lives_malloc(sizeof(struct _decomp));
      dcc->value = val;
      dcc->i = i;
      dcc->j = j;
      if (preplist == NULL) dccl = preplist = lives_list_append(preplist, dcc);
      else {
        LiVESList *dccl2 = lives_list_append(NULL, (livespointer)dcc);
        for (; dccl != NULL; dccl = dccl->next) {
          dcc = (struct _decomp *)dccl->data;
          if (dcc->value > val) break;
          dccl_last = dccl;
        }
        if (!dccl) {
          dccl_last->next = dccl2;
          dccl2->prev = dccl_last;
          dccl2->next = NULL;
          dccl = dccl2;
        } else {
          dccl2->next = dccl;
          dccl2->prev = dccl->prev;
          if (dccl->prev != NULL) dccl->prev->next = dccl2;
          else preplist = dccl2;
          dccl->prev = dccl2;
        }
      }
      val *= 2;
    }
    val6 *= 6;
  }
  for (dccl = preplist; dccl != NULL; dccl = dccl->next) {
    dcc = (struct _decomp *)dccl->data;
    xi = dcc->i;
    xj = dcc->j;
    nxttbl[xi][xj].value = dcc->value;
    nxttbl[xi][xj].i = xi;
    nxttbl[xi][xj].j = xj;
    if (dccl->prev != NULL) {
      dcc = (struct _decomp *)dccl->prev->data;
      nxttbl[xi][xj].lower = &(nxttbl[dcc->i][dcc->j]);
    } else nxttbl[xi][xj].lower = NULL;
    if (dccl->next != NULL) {
      dcc = (struct _decomp *)dccl->next->data;
      nxttbl[xi][xj].higher = &(nxttbl[dcc->i][dcc->j]);
    } else nxttbl[xi][xj].higher = NULL;
  }
  lives_list_free_all(&preplist);
  nxttab_inited = TRUE;
}



void autotune_u64(weed_plant_t *tuner,  uint64_t min, uint64_t max, int ntrials, double cost) {
  if (tuner) {
    double tc = cost;
    int trials = weed_get_int_value(tuner, "trials", NULL);
    if (trials == 0) {
      weed_set_int_value(tuner, "ntrials", ntrials);
      weed_set_int64_value(tuner, "min", min);
      weed_set_int64_value(tuner, "max", max);
    } else tc += weed_get_double_value(tuner, "tcost", NULL);
    weed_set_double_value(tuner, "tcost", tc);
    weed_set_int64_value(tuner, "tstart", lives_get_current_ticks());
  }
}

#define NCYCS 16


uint64_t nxtval(uint64_t val, uint64_t lim, boolean less) {
  // to avoid only checking powers of 2, we want some number which is (2 ** i) * (6 ** j)
  // which gives a nice range of results
  uint64_t oval = val;
  int i = 0, j = 0;
  if (!nxttab_inited) make_nxttab();
  /// decompose val into i, j
  /// divide by 6 until val mod 6 is non zero
  if (val & 1) {
    if (less) val--;
    else val++;
  }
  for (; !(val % 6) && val > 0; j++, val /= 6);
  /// divide by 2 until we reach 1; if the result of a division is odd we add or subtract 1
  for (; val > 1; i++, val /= 2) {
    if (val & 1) {
      if (less) val--;
      else val++;
    }
  }
  val = nxttbl[i][j].value;
  if (less) {
    if (val == oval) {
      if (nxttbl[i][j].lower) val = nxttbl[i][j].lower->value;
    } else {
      while (nxttbl[i][j].higher->value < oval) {
        int xi = nxttbl[i][j].higher->i;
        val = nxttbl[i][j].value;
        j = nxttbl[i][j].higher->j;
        i = xi;
      }
    }
    return val > lim ? val : lim;
  }
  if (val == oval) {
    if (nxttbl[i][j].higher) val = nxttbl[i][j].higher->value;
  } else {
    while (nxttbl[i][j].lower && nxttbl[i][j].lower->value > oval) {
      int xi = nxttbl[i][j].lower->i;
      j = nxttbl[i][j].lower->j;
      i = xi;
      val = nxttbl[i][j].value;
    }
  }
  return val < lim ? val : lim;
}


uint64_t autotune_u64_end(weed_plant_t **tuner, uint64_t val) {
  if (!tuner || !*tuner) return val;
  else {
    ticks_t tottime = lives_get_current_ticks();
    int ntrials, trials;
    int64_t max;
    int64_t min = weed_get_int64_value(*tuner, "min", NULL);

    if (val < min) {
      val = min;
      weed_set_int_value(*tuner, "trials", 0);
      weed_set_int64_value(*tuner, "tottime", 0);
      weed_set_double_value(*tuner, "tcost", 0);
      return val;
    }
    max = weed_get_int64_value(*tuner, "max", NULL);
    if (val > max) {
      val = max;
      weed_set_int_value(*tuner, "trials", 0);
      weed_set_int64_value(*tuner, "tottime", 0);
      weed_set_double_value(*tuner, "tcost", 0);
      return val;
    }

    ntrials = weed_get_int_value(*tuner, "ntrials", NULL);
    trials = weed_get_int_value(*tuner, "trials", NULL);

    weed_set_int_value(*tuner, "trials", ++trials);
    tottime += (weed_get_int64_value(*tuner, "tottime", NULL)) - weed_get_int64_value(*tuner, "tstart", NULL);
    weed_set_int64_value(*tuner, "tottime", tottime);

    if (trials >= ntrials) {
      int cycs = weed_get_int_value(*tuner, "cycles", NULL) + 1;
      if (cycs < NCYCS) {
        double tcost = (double)weed_get_double_value(*tuner, "tcost", NULL);
        double totcost = (double)tottime * tcost;
        double avcost = totcost / (double)(cycs * ntrials);
        double ccosts, ccostl;
        boolean smfirst = FALSE;
        char *key1 = lives_strdup_printf("tottrials_%lu", val);
        char *key2 = lives_strdup_printf("totcost_%lu", val);

        weed_set_int_value(*tuner, key1, weed_get_int_value(*tuner, key1, NULL) + trials);
        weed_set_double_value(*tuner, key2, weed_get_double_value(*tuner, key2, NULL) + totcost);

        lives_free(key1);
        lives_free(key2);

        if (cycs & 1) smfirst = TRUE;
        weed_set_int_value(*tuner, "cycles", cycs);

        weed_set_int_value(*tuner, "trials", 0);
        weed_set_int64_value(*tuner, "tottime", 0);
        weed_set_double_value(*tuner, "tcost", 0);

        if (smfirst) {
          if (val > max || weed_plant_has_leaf(*tuner, "smaller")) {
            ccosts = weed_get_double_value(*tuner, "smaller", NULL);
            if (val > max || (ccosts < avcost)) {
              weed_set_double_value(*tuner, "larger", avcost);
              weed_leaf_delete(*tuner, "smaller");
              if (val > max) return max;
              return nxtval(val, min, TRUE); // TRUE to get smaller val
            }
          }
        }

        if (val < min || weed_plant_has_leaf(*tuner, "larger")) {
          ccostl = weed_get_double_value(*tuner, "larger", NULL);
          if (val < min || (ccostl < avcost)) {
            weed_set_double_value(*tuner, "smaller", avcost);
            weed_leaf_delete(*tuner, "larger");
            if (val < min) return min;
            return nxtval(val, max, FALSE);
          }
        }

        if (!smfirst) {
          if (val > max || weed_plant_has_leaf(*tuner, "smaller")) {
            ccosts = weed_get_double_value(*tuner, "smaller", NULL);
            if (val > max || (ccosts < avcost)) {
              weed_set_double_value(*tuner, "larger", avcost);
              weed_leaf_delete(*tuner, "smaller");
              if (val > max) return max;
              return nxtval(val, min, TRUE);
            }
          }

          if (!weed_plant_has_leaf(*tuner, "larger")) {
            weed_set_double_value(*tuner, "smaller", avcost);
            weed_leaf_delete(*tuner, "larger");
            return nxtval(val, max, FALSE);
          }
        }

        if (!weed_plant_has_leaf(*tuner, "smaller")) {
          weed_set_double_value(*tuner, "larger", avcost);
          weed_leaf_delete(*tuner, "smaller");
          return nxtval(val, min, TRUE);
        }

        if (smfirst) {
          if (!weed_plant_has_leaf(*tuner, "larger")) {
            weed_set_double_value(*tuner, "smaller", avcost);
            weed_leaf_delete(*tuner, "larger");
            return nxtval(val, max, FALSE);
          }
        }

        weed_leaf_delete(*tuner, "smaller");
        weed_leaf_delete(*tuner, "larger");
        if (!smfirst) {
          return nxtval(nxtval(val, max, FALSE), max, FALSE);
        } else {
          return nxtval(nxtval(val, min, TRUE), min, TRUE);
        }
      } else {
        weed_size_t nleaves;
        char **res = weed_plant_list_leaves(*tuner, &nleaves);
        uint64_t bestval = val, xval;
        const char *key1 = "totcost_";
        char *key2;
        double avcost, costmin = 0.;
        boolean gotcost = FALSE;
        int j;

        for (int i = 1; i < nleaves; i++) {
          if (!strncmp(res[i], key1, 8)) {
            xval = strtoul((const char *)(res[i] + 8), NULL, 10);
            key2 = lives_strdup_printf("totrials_%lu", xval);
            for (j = i + 1; j < nleaves; j++) {
              if (!strcmp(res[j], key2)) break;
            }
            if (j == nleaves) {
              for (j = 0; j < i; j++) {
                if (!strcmp(res[j], key2)) break;
              }
            }
            if ((avcost = weed_get_double_value(*tuner, res[i], NULL) / (double)weed_get_int_value(*tuner, res[j], NULL)) < costmin
                || !gotcost) {
              costmin = avcost;
              bestval = xval;
              gotcost = TRUE;
            }
            lives_free(key2);
          }
        }
        val = bestval;
        if (prefs->show_dev_opts)
          g_print("value of %d tuned to %lu\n", weed_plant_get_type(*tuner), val);
        // TODO: store value so we can recalibrate again later
        //tuned = (struct tuna *)lives_malloc(sizeof(tuna));
        //tuna->wptpp = tuner;
        //tuna->id = weed_get_in
        //lives_list_prepend(tunables, tuned);
        weed_plant_free(*tuner);
        *tuner = NULL;
        for (j = 0; j < nleaves; lives_free(res[j++]));
        lives_free(res);
      }
      return val;
    }
    weed_set_int64_value(*tuner, "tottime", tottime);
  }
  return val;
}



////// memory funcs ////

/// susbtitute memory functions. These must be real functions and not #defines since we need fn pointers
#define OIL_MEMCPY_MAX_BYTES 12288 // this can be tuned to provide optimal performance

#ifdef ENABLE_ORC
livespointer lives_orc_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  static size_t maxbytes = OIL_MEMCPY_MAX_BYTES;
  static weed_plant_t *tuner = NULL;
  static boolean tuned = FALSE;
  static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
  boolean haslock = FALSE;
  if (n == 0) return dest;

  if (!tuned && !tuner) tuner = weed_plant_new(31337);
  if (tuner) {
    if (!pthread_mutex_trylock(&tuner_mutex)) {
      haslock = TRUE;
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
    if (haslock) autotune_u64(tuner, -1024 * 1024, 1024 * 1024, 128, 1. / (double)n);
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
  if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) {
    oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);
    return dest;
  }
  return memcpy(dest, src, n);
}
#endif


livespointer proxy_realloc(livespointer ptr, size_t new_size) {
  livespointer nptr = lives_malloc(new_size);
  if (nptr && ptr) {
    lives_memmove(nptr, ptr, new_size);
    lives_free(ptr);
  }
  return nptr;
}


#define _cpy_if_nonnull(d, s, size) (d ? lives_memcpy(d, s, size) : d)

// functions with fixed pointers that we can pass to plugins ///
void *_ext_malloc(size_t n) {
  return (n == 0 ? NULL : lives_malloc(n));
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
  if (p) lives_free(p);
}

void *_ext_free_and_return(void *p) {
  _ext_free(p);
  return NULL;
}

void *_ext_memcpy(void *dest, const void *src, size_t n) {
  return lives_memcpy(dest, src, n);
}

void *_ext_memset(void *p, int i, size_t n) {
  return lives_memset(p, i, n);
}

void *_ext_memmove(void *dest, const void *src, size_t n) {
  return lives_memmove(dest, src, n);
}

void *_ext_realloc(void *p, size_t n) {
  return lives_realloc(p, n);
}

void *_ext_calloc(size_t nmemb, size_t msize) {
  return lives_calloc(nmemb, msize);
}

LIVES_GLOBAL_INLINE void *lives_calloc_safety(size_t nmemb, size_t xsize) {
  size_t totsize = nmemb * xsize;
  if (totsize == 0) return NULL;
  if (xsize < DEF_ALIGN) {
    xsize = DEF_ALIGN;
    nmemb = (totsize / xsize) + 1;
  }
  return lives_calloc(nmemb + (EXTRA_BYTES / xsize), xsize);
}

LIVES_GLOBAL_INLINE void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize) {
  /// realloc from omemb * size to nmemb * size
  /// memory allocated via calloc, with DEF_ALIGN alignment and EXTRA_BYTES extra padding
  void *np = lives_calloc_safety(nmemb, xsize);
  if (omemb > nmemb) omemb = nmemb;
  lives_memcpy(np, p, omemb * xsize);
  lives_free(p);
  return np;
}


// slice allocator //// TODO

static memheader_t base;           /* Zero sized block to get us started. */
static memheader_t *freep = &base; /* Points to first free block of memory. */
static memheader_t *usedp;         /* Points to first used block of memory. */

/*
   Scan the free list and look for a place to put the block. Basically, we're
   looking for any block the to be freed block might have been partitioned from.
*/
void quick_free(memheader_t *bp) {
  memheader_t *p;

  for (p = freep; !(bp > p && bp < p->next); p = p->next)
    if (p >= p->next && (bp > p || bp < p->next))
      break;

  if (bp + bp->size == p->next) {
    bp->size += p->next->size;
    bp->next = p->next->next;
  } else
    bp->next = p->next;

  if (p + p->size == bp) {
    p->size += bp->size;
    p->next = bp->next;
  } else
    p->next = bp;

  freep = p;
}


#define MIN_ALLOC_SIZE 4096     /* We allocate blocks in page sized chunks. */

/*
   Request more memory from the kernel.
*/
static memheader_t *morecore(size_t num_units) {
  void *vp;
  memheader_t *up;

  if (num_units > MIN_ALLOC_SIZE)
    num_units = MIN_ALLOC_SIZE / sizeof(memheader_t);

  if ((vp = sbrk(num_units * sizeof(memheader_t))) == (void *) - 1)
    return NULL;

  up = (memheader_t *) vp;
  up->size = num_units;
  quick_free(up); // add to freelist
  return freep;
}


/*
   Find a chunk from the free list and put it in the used list.
*/
LIVES_INLINE void *_quick_malloc(size_t alloc_size, size_t align) {
  size_t num_units;
  memheader_t *p, *prevp;

  num_units = (alloc_size + sizeof(memheader_t) - 1) / sizeof(memheader_t) + 1;
  prevp = freep;

  for (p = prevp->next;; prevp = p, p = p->next) {
    if (p->size >= num_units) { /* Big enough. */
      if (p->size == num_units) /* Exact size. */
        prevp->next = p->next;
      else {
        p->size -= num_units;
        p += p->size;
        p->size = num_units;
      }

      freep = prevp;

      /* Add to p to the used list. */
      if (usedp == NULL)
        usedp = p->next = p;
      else {
        p->next = usedp->next;
        usedp->next = p;
      }
      p->align = align;
      return (void *)(p + 1);
    }
    if (p == freep) { /* Not enough memory. */
      p = morecore(num_units);
      if (p == NULL) /* Request for more memory failed. */
        return NULL;
    }
  }
}


/* void *quick_malloc(size_t alloc_size) { */
/*   return _quick_malloc(alloc_size, 1); */
/* } */

/* void *quick_calloc(size_t nmemb, size_t size) { */
/*   return _quick_malloc(nmemb * size, size); */
/* } */

/*   quick_calloc(); */

/* quick_memcpy(); */

/* quick_memmove(); */

/* quick_memset(); */


char *get_md5sum(const char *filename) {
  /// for future use
  char **array;
  char *md5;
  char *com = lives_strdup_printf("%s \"%s\"", EXEC_MD5SUM, filename);
  mainw->com_failed = FALSE;
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);
  if (mainw->com_failed) {
    mainw->com_failed = FALSE;
    return NULL;
  }
  array = lives_strsplit(mainw->msg, " ", 2);
  md5 = lives_strdup(array[0]);
  lives_strfreev(array);
  return md5;
}


char *lives_format_storage_space_string(uint64_t space) {
  char *fmt;

  if (space > lives_10pow(18)) {
    // TRANSLATORS: Exabytes
    fmt = lives_strdup_printf(_("%.2f EB"), (double)space / (double)lives_10pow(18));
  } else if (space > lives_10pow(15)) {
    // TRANSLATORS: Petabytes
    fmt = lives_strdup_printf(_("%.2f PB"), (double)space / (double)lives_10pow(15));
  } else if (space > lives_10pow(12)) {
    // TRANSLATORS: Terabytes
    fmt = lives_strdup_printf(_("%.2f TB"), (double)space / (double)lives_10pow(12));
  } else if (space > lives_10pow(9)) {
    // TRANSLATORS: Gigabytes
    fmt = lives_strdup_printf(_("%.2f GB"), (double)space / (double)lives_10pow(9));
  } else if (space > lives_10pow(6)) {
    // TRANSLATORS: Megabytes
    fmt = lives_strdup_printf(_("%.2f MB"), (double)space / (double)lives_10pow(6));
  } else if (space > 1024) {
    // TRANSLATORS: Kilobytes (1024 bytes)
    fmt = lives_strdup_printf(_("%.2f KiB"), (double)space / 1024.);
  } else {
    fmt = lives_strdup_printf(_("%d bytes"), space);
  }

  return fmt;
}


lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, uint64_t *dsval) {
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)
  uint64_t ds;
  if (!is_writeable_dir(dir)) return LIVES_STORAGE_STATUS_UNKNOWN;
  ds = get_fs_free(dir);
  if (dsval != NULL) *dsval = ds;
  if (ds < prefs->ds_crit_level) return LIVES_STORAGE_STATUS_CRITICAL;
  if (ds < warn_level) return LIVES_STORAGE_STATUS_WARNING;
  return LIVES_STORAGE_STATUS_NORMAL;
}


uint64_t get_fs_free(const char *dir) {
  // get free space in bytes for volume containing directory dir
  // return 0 if we cannot create/write to dir

  // caller should test with is_writeable_dir() first before calling this
  // since 0 is a valid return value

  // dir should be in locale encoding

  // WARNING: this will actually create the directory (since we dont know if its parents are needed)

  struct statvfs sbuf;

  uint64_t bytes = 0;
  boolean must_delete = FALSE;

  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) must_delete = TRUE;
  if (!is_writeable_dir(dir)) goto getfserr;

  // use statvfs to get fs details
  if (statvfs(dir, &sbuf) == -1) goto getfserr;
  if (sbuf.f_flag & ST_RDONLY) goto getfserr;

  // result is block size * blocks available
  bytes = sbuf.f_bsize * sbuf.f_bavail;

getfserr:
  if (must_delete) lives_rmdir(dir, FALSE);

  return bytes;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(ticks_t origsecs, ticks_t orignsecs) {
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (ts.tv_sec - origsecs) * TICKS_PER_SECOND + ts.tv_nsec / 10  - orignsecs / 10;
#else
#ifdef USE_MONOTONIC_TIME
  return (lives_get_monotonic_time() - orignsecs) / 10;
#else
  gettimeofday(&tv, NULL);
  return TICKS_PER_SECOND * (tv.tv_sec - origsecs) + tv.tv_usec * USEC_TO_TICKS - orignsecs / 10;
#endif
#endif
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks(void) {
  //  return current (wallclock) time in ticks (units of 10 nanoseconds)
  return lives_get_relative_ticks(0, 0);
}


char *lives_datetime(struct timeval *tv) {
  char buf[128];
  char *datetime = NULL;
  struct tm *gm = gmtime(&tv->tv_sec);
  ssize_t written;

  if (gm) {
    written = (ssize_t)strftime(buf, 128, "%Y-%m-%d    %H:%M:%S", gm);
    if ((written > 0) && ((size_t)written < 128)) {
      datetime = lives_strdup(buf);
    }
  }
  return datetime;
}


boolean check_dev_busy(char *devstr) {
  int ret;
#ifdef IS_SOLARIS
  struct flock lock;
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
#endif
  int fd = open(devstr, O_RDONLY | O_NONBLOCK);
  if (fd == -1) return FALSE;
#ifdef IS_SOLARIS
  ret = fcntl(fd, F_SETLK, &lock);
#else
  ret = flock(fd, LOCK_EX | LOCK_NB);
#endif
  close(fd);
  if (ret == -1) return FALSE;
  return TRUE;
}


uint64_t get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  fstat(fd, &filestat);
  return (uint64_t)(filestat.st_size);
}


uint64_t sget_file_size(const char *name) {
  // get the size of file fd
  struct stat filestat;
  int fd;

  if ((fd = open(name, O_RDONLY)) == -1) {
    return (uint32_t)0;
  }

  fstat(fd, &filestat);
  close(fd);

  return (uint64_t)(filestat.st_size);
}


void reget_afilesize(int fileno) {
  // re-get the audio file size
  lives_clip_t *sfile = mainw->files[fileno];
  boolean bad_header = FALSE;

  if (mainw->multitrack != NULL) return; // otherwise achans gets set to 0...

  sfile->afilesize = reget_afilesize_inner(fileno);

  if (sfile->afilesize == 0l) {
    if (!sfile->opening && fileno != mainw->ascrap_file && fileno != mainw->scrap_file) {
      if (sfile->arate != 0 || sfile->achans != 0 || sfile->asampsize != 0 || sfile->arps != 0) {
        sfile->arate = sfile->achans = sfile->asampsize = sfile->arps = 0;
        save_clip_value(fileno, CLIP_DETAILS_ACHANS, &sfile->achans);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_ARATE, &sfile->arps);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &sfile->arate);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &sfile->asampsize);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        if (bad_header) do_header_write_error(fileno);
      }
    }
  }

  if (mainw->is_ready && fileno > 0 && fileno == mainw->current_file) {
    // force a redraw
    update_play_times();
  }
}


uint64_t reget_afilesize_inner(int fileno) {
  // safe version that just returns the audio file size
  uint64_t filesize;
  char *afile = lives_get_audio_file_name(fileno);
  lives_sync(1);
  filesize = sget_file_size(afile);
  lives_free(afile);
  return filesize;
}


#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what) {
  char *lives_log_file = lives_build_filename(prefs->workdir, LIVES_LOG_FILE, NULL);
  if (mainw->log_fd < 0) mainw->log_fd = open(lives_log_file, O_WRONLY | O_CREAT, DEF_FILE_PERMS);
  if (mainw->log_fd != -1) {
    char *msg = lives_strdup("%s|%d|", what, mainw->current_file);
    write(mainw->log_fd, msg, strlen(msg));
    lives_free(msg);
  }
  lives_free(lives_log_file);
}
#endif


int check_for_bad_ffmpeg(void) {
  int i, fcount;
  char *fname_next;
  boolean maybeok = FALSE;

  fcount = get_frame_count(mainw->current_file, 1);

  for (i = 1; i <= fcount; i++) {
    fname_next = make_image_file_name(cfile, i, get_image_ext_for_type(cfile->img_type));
    if (sget_file_size(fname_next) > 0) {
      lives_free(fname_next);
      maybeok = TRUE;
      break;
    }
    lives_free(fname_next);
  }

  if (!maybeok) {
    do_error_dialog(
      _("Your version of mplayer/ffmpeg may be broken !\nSee http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}


#define hasNulByte(x) ((x - 0x0101010101010101) & ~x & 0x8080808080808080)
#define getnulpos(nulmask) ((nulmask & 2155905152ul) ? ((nulmask & 32896ul) ? ((nulmask & 128ul) ? 0 : 1) :\
						      (((nulmask & 8388608ul) ? 2 : 3))) : ((nulmask & 141287244169216ul) ? \
											  ((nulmask & 549755813888ul) ? 4 : 5) : \
											  ((nulmask & 36028797018963968ul) ? 6 : 7)))

LIVES_GLOBAL_INLINE size_t lives_strlen(const char *s) {
  if (!s) return 0;
  else {
    const char *p = s;
    uint64_t *pi = (uint64_t *)p, nulmask;
    while (*p) {
      if ((void *)pi == (void *)p) {
        while (!(nulmask = hasNulByte(*pi))) ++pi;
        if ((void *)pi - (void *)s + getnulpos(nulmask) != strlen(s)) {
          g_print("len of %s (%ld) is of course %ld + %d, i.e. %ld %lx   \n", s, strlen(s),
                  (void *)pi - (void *)s, getnulpos(nulmask), (void *)pi - (void *)s + getnulpos(nulmask), nulmask);
          return (void *)pi - (void *)s + getnulpos(nulmask) ;
        }
      }
      p++;
    }
    return p - s;
  }
}

/// returns FALSE if strings match
LIVES_GLOBAL_INLINE boolean lives_strcmp(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
        while (1) {
          if ((d1 = *(ip1++)) == (d2 = *(ip2++))) {
            if (hasNulByte(d1)) {
              if (!hasNulByte(d2)) return TRUE;
              break;
            }
          } else {
            if (!hasNulByte(d1) || !(hasNulByte(d2))) return TRUE;
            break;
          }
        }
        st1 = (const char *)(--ip1); st2 = (const char *)(--ip2);
      }
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st2 != 0);
}

LIVES_GLOBAL_INLINE int lives_strcmp_ordered(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((const char *)ip1 == st1 && (const char *)ip2 == st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1));
        st1 = (const char *)(--ip1); st2 = (const char *)(--ip2);
      }
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st1 > *st2) - (*st1 < *st2);
}

/// returns FALSE if strings match
LIVES_GLOBAL_INLINE boolean lives_strncmp(const char *st1, const char *st2, size_t len) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    size_t xlen = len >> 3;
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if (xlen && (const char *)ip1 == st1 && (const char *)ip2 == st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1) && --xlen);
        if (xlen) {
          if (!hasNulByte(d2)) return TRUE;
          ip1--;
          ip2--;
        }
        st1 = (const char *)ip1; st2 = (const char *)ip2;
        len -= ((len >> 3) - xlen) << 3;
      }
      if (!(len--)) return FALSE;
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st1 != *st2);
}


LIVES_GLOBAL_INLINE uint32_t string_hash(const char *string) {
  uint32_t hash = 5381;
  if (string == NULL) return 0;
  for (char c; (c = *(string++)) != 0; hash += (hash << 5) + c);
  return hash;
}

///////// thread pool ////////////////////////
//#define TUNE_MALLOPT 1
#define MINPOOLTHREADS 4
static int npoolthreads;
static pthread_t **poolthrds;
static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static LiVESList *twork_first, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

#ifdef TUNE_MALLOPT
static size_t narenas;
static weed_plant_t *mtuner = NULL;
static boolean mtuned = FALSE;
static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
#define MALLOPT_WAIT_MAX 30
#endif

static void *thrdpool(void *arg) {
  LiVESList *list;
  thrd_work_t *mywork;
  int myidx = LIVES_POINTER_TO_INT(arg);
  while (!threads_die) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_wait(&tcond, &tcond_mutex);
    pthread_mutex_unlock(&tcond_mutex);
    if (threads_die) break;
    while (!threads_die) {
      pthread_mutex_lock(&twork_mutex);
      if ((list = twork_last) == NULL) {
        pthread_mutex_unlock(&twork_mutex);
        break;
      }
      if (twork_first == list) twork_first = NULL;
      twork_last = list->prev;
      if (twork_last != NULL) twork_last->next = NULL;
      pthread_mutex_unlock(&twork_mutex);

      mywork = (thrd_work_t *)list->data;

#ifdef TUNE_MALLOPT
      if (!pthread_mutex_trylock(&tuner_mutex)) {
        if (mtuner) {
          mywork->flags |= LIVES_THRDFLAG_TUNING;
          autotune_u64(mtuner, 1, npoolthreads * 4, 128, (16. + (double)narenas * 2.
                       + (double)(mainw->effort > 0 ? mainw->effort : 0) / 16));
        }
      }
#endif

      (*mywork->func)(mywork->arg);

#ifdef TUNE_MALLOPT
      if (mywork->flags & LIVES_THRDFLAG_TUNING) {
        if (mtuner) {
          size_t onarenas = narenas;
          narenas = autotune_u64_end(&mtuner, narenas);
          if (!mtuner) mtuned = TRUE;
          if (narenas != onarenas) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += MALLOPT_WAIT_MAX;
            if (!pthread_rwlock_timedwrlock(&mainw->mallopt_lock, &ts)) {
              if (prefs->show_dev_opts) g_printerr("mallopt %ld\n", narenas);
              //lives_invalidate_all_file_buffers();
              mallopt(M_ARENA_MAX, narenas);
              pthread_rwlock_unlock(&mainw->mallopt_lock);
            } else narenas = onarenas;
          }
        }
        pthread_mutex_unlock(&tuner_mutex);
        mywork->flags = 0;
      }
#endif

      pthread_mutex_lock(&twork_mutex);
      ntasks--;
      pthread_mutex_unlock(&twork_mutex);
      if (mywork->flags & LIVES_THRDFLAG_AUTODELETE) {
        lives_free(mywork);
        lives_free(list);
      } else {
        mywork->done = myidx + 1;
      }
    }
  }
  return NULL;
}


void lives_threadpool_init(void) {
  npoolthreads = MINPOOLTHREADS;
  if (prefs->nfx_threads > npoolthreads) npoolthreads = prefs->nfx_threads;
#ifdef TUNE_MALLOPT
  narenas = npoolthreads * 2;
  mallopt(M_ARENA_MAX, narenas);
  if (!mtuned && !mtuner) mtuner = weed_plant_new(12345);
#endif
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_first = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i));
  }
}


void *_plant_thread_func(void *args) {
  weed_plant_t *info = (weed_plant_t *)args;
  weed_funcptr_t func = weed_get_funcptr_value(info, WEED_LEAF_THREADFUNC, NULL);
  if (!weed_plant_has_leaf(info, WEED_LEAF_THREAD_PARAM0)) {
    (*func)();
    weed_plant_free(info);
    return NULL;
  }
  switch (weed_leaf_seed_type(info, WEED_LEAF_THREAD_PARAM0)) {
  case WEED_SEED_VOIDPTR:
    if (!weed_plant_has_leaf(info, WEED_LEAF_THREAD_PARAM1)) {
      (*func)(weed_get_voidptr_value(info, WEED_LEAF_THREAD_PARAM0, NULL));
      break;
    }
    switch (weed_leaf_seed_type(info, WEED_LEAF_THREAD_PARAM1)) {
    case WEED_SEED_VOIDPTR:
      if (!weed_plant_has_leaf(info, WEED_LEAF_THREAD_PARAM2)) {
        (*func)(weed_get_voidptr_value(info, WEED_LEAF_THREAD_PARAM0, NULL),
                weed_get_voidptr_value(info, WEED_LEAF_THREAD_PARAM1, NULL));
        break;
      }
    // etc.
    default:
      break;
    }
  default:
    break;
  }
  if (weed_get_boolean_value(info, WEED_LEAF_NOTIFY, NULL)) weed_set_boolean_value(info, WEED_LEAF_DONE, WEED_TRUE);
  else weed_plant_free(info);
  return NULL;
}


boolean run_as_thread(weed_plant_t *info) {
  /// run any function as a thread
  lives_thread_t *thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
  lives_thread_attr_t attr = LIVES_THRDATTR_AUTODELETE;
  lives_thread_create(thread, &attr, _plant_thread_func, (void *)info);
  return TRUE;
}


void lives_threadpool_finish(void) {
  threads_die = TRUE;
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_broadcast(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  for (int i = 0; i < npoolthreads; i++) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    pthread_join(*(poolthrds[i]), NULL);
    lives_free(poolthrds[i]);
  }
  lives_free(poolthrds);
  poolthrds = NULL;
  npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_first);
  twork_first = twork_last = NULL;
  ntasks = 0;
}


int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t *attr, lives_funcptr_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  if (!thread) list = (LiVESList *)lives_calloc(1, sizeof(LiVESList));
  list->data = work;
  list->prev = NULL;
  work->func = func;
  work->arg = arg;
  if (!thread || (attr && (*attr & LIVES_THRDATTR_AUTODELETE))) work->flags |= LIVES_THRDFLAG_AUTODELETE;

  pthread_mutex_lock(&twork_mutex);
  if (twork_first == NULL) {
    twork_first = twork_last = list;
  } else {
    if (!attr || !(*attr & LIVES_THRDATTR_PRIORITY)) {
      twork_first->prev = list;
      list->next = twork_first;
      twork_first = list;
    } else {
      twork_last->next = list;
      list->prev = twork_last;
      twork_last = list;
    }
  }
  if (++ntasks > npoolthreads) {
    pthread_mutex_unlock(&twork_mutex);

    poolthrds = (pthread_t **)lives_realloc(poolthrds, (npoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
    for (int i = npoolthreads; i < npoolthreads + MINPOOLTHREADS; i++) {
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i));
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    npoolthreads += MINPOOLTHREADS;
#ifdef TUNE_MALLOPT
    if (!mtuner) {
      mtuner = weed_plant_new(12345);
      mtuned = FALSE;
    }
#endif
  } else pthread_mutex_unlock(&twork_mutex);
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  return 0;
}


int lives_thread_join(lives_thread_t work, void **retval) {
  thrd_work_t *task = (thrd_work_t *)work.data;
  int nthrd = 0;
  if (task->flags & LIVES_THRDFLAG_AUTODELETE) {
    LIVES_FATAL("lives_thread_join() called on an autodelete thread");
    return 0;
  }

  lives_nanosleep_until_nonzero(task->done);
  nthrd = task->done;

  if (retval) *retval = task->ret;
  lives_free(task);
  return nthrd;
}

LIVES_GLOBAL_INLINE void lives_srandom(unsigned int seed) {
  srandom(seed);
}

LIVES_GLOBAL_INLINE uint64_t lives_random(void) {
  return random();
}

LIVES_GLOBAL_INLINE pid_t lives_getpid(void) {
  return getpid();
}

LIVES_GLOBAL_INLINE int lives_getuid(void) {
  return geteuid();
}

LIVES_GLOBAL_INLINE int lives_getgid(void) {
  return getegid();
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
  if (count == 4) swab4(buff, buff, gran);
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
      void *tbuff = lives_malloc(chunk);
      start++;
      end = ocount - 1 - chunk;
      while (start + chunk < end) {
        lives_memcpy(tbuff, &buff[end], chunk);
        lives_memcpy(&buff[end], &buff[start], chunk);
        lives_memcpy(&buff[start], tbuff, chunk);
        start += chunk;
        end -= chunk;
      }
      lives_free(tbuff);
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

  if (count == 0) return TRUE;
  return FALSE;
}


/// estimate the machine load
static int16_t theflow[EFFORT_RANGE_MAX];
static int flowlen = 0;
static boolean inited = FALSE;
static int struggling = 0;
static int badthingcount = 0;
static int goodthingcount = 0;

static int pop_flowstate(void) {
  int ret = theflow[0];
  flowlen--;
  for (int i = 0; i < flowlen; i++) {
    theflow[i] = theflow[i + 1];
  }
  return ret;
}


void reset_effort(void) {
  prefs->pb_quality = future_prefs->pb_quality;
  mainw->blend_palette = WEED_PALETTE_END;
  lives_memset(theflow, 0, sizeof(theflow));
  inited = TRUE;
  badthingcount = goodthingcount = 0;
  struggling = 0;
  if ((mainw->is_rendering || (mainw->multitrack != NULL
                               && mainw->multitrack->is_rendering)) && !mainw->preview_rendering)
    mainw->effort = -EFFORT_RANGE_MAX;
  else mainw->effort = 0;
}


void update_effort(int nthings, boolean badthings) {
  int spcycles;

  if (!inited) reset_effort();
  if (!nthings) return;
  //g_print("VALS %d %d %d %d %d\n", nthings, badthings, mainw->effort, badthingcount, goodthingcount);
  if (badthings)  {
    badthingcount += nthings;
    goodthingcount = 0;
    spcycles = -1;
  } else {
    spcycles = nthings;
    if (spcycles + goodthingcount > EFFORT_RANGE_MAX) spcycles = EFFORT_RANGE_MAX - goodthingcount;
    goodthingcount += spcycles;
    if (goodthingcount > EFFORT_RANGE_MAX) goodthingcount = EFFORT_RANGE_MAX;
    nthings = 1;
  }

  while (nthings-- > 0) {
    if (flowlen >= EFFORT_RANGE_MAX) {
      /// +1 for each badthing, so when it pops out we subtract it
      int res = pop_flowstate();
      if (res > 0) badthingcount -= res;
      else goodthingcount += res;
      //g_print("vals %d %d %d  ", res, badthingcount, goodthingcount);
    }
    /// - all the good things, so when it pops out we add it (i.e subtract the value)
    theflow[flowlen] = -spcycles;
    flowlen++;
  }

  if (!badthingcount) {
    /// no badthings, good
    if (goodthingcount > EFFORT_RANGE_MAX) goodthingcount = EFFORT_RANGE_MAX;
    mainw->effort = -goodthingcount;
  } else {
    if (badthingcount > EFFORT_RANGE_MAX) badthingcount = EFFORT_RANGE_MAX;
    mainw->effort = badthingcount;
  }
  //g_print("vals2 %d %d %d\n", mainw->effort, badthingcount, goodthingcount);

  if (mainw->effort < 0) {
    if (struggling) {
      struggling += ((mainw->effort | 16) >> 4);
      if (struggling < 0) struggling = 0;
    } else {
      if (mainw->effort < -EFFORT_LIMIT_MED) {
        if (prefs->pb_quality < PB_QUALITY_HIGH) {
          prefs->pb_quality++;
          mainw->blend_palette = WEED_PALETTE_END;
        } else if (prefs->pb_quality < PB_QUALITY_MED) {
          prefs->pb_quality++;
          mainw->blend_palette = WEED_PALETTE_END;
        }
      }
    }
  }

  if (mainw->effort > EFFORT_LIMIT_LOW) {
    if (prefs->pb_quality > future_prefs->pb_quality) {
      prefs->pb_quality = future_prefs->pb_quality;
      mainw->blend_palette = WEED_PALETTE_END;
      return;
    }
    if (!struggling) {
      struggling = 1;
      return;
    }
    if (mainw->effort > EFFORT_LIMIT_MED || (struggling && (mainw->effort > EFFORT_LIMIT_LOW))) {
      if (struggling < EFFORT_RANGE_MAX) struggling++;
      if (struggling == EFFORT_RANGE_MAX) {
        if (prefs->pb_quality > PB_QUALITY_LOW) {
          prefs->pb_quality--;
          mainw->blend_palette = WEED_PALETTE_END;
        }
        if (mainw->effort > EFFORT_LIMIT_MED) {
          if (prefs->pb_quality > PB_QUALITY_LOW) {
            prefs->pb_quality--;
            mainw->blend_palette = WEED_PALETTE_END;
          }
        }
      } else {
        if (prefs->pb_quality > future_prefs->pb_quality) {
          prefs->pb_quality = future_prefs->pb_quality;
          mainw->blend_palette = WEED_PALETTE_END;
        } else if (future_prefs->pb_quality > PB_QUALITY_LOW) {
          prefs->pb_quality = future_prefs->pb_quality - 1;
          mainw->blend_palette = WEED_PALETTE_END;
        }
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
  //g_print("STRG %d and %d %d %d\n", struggling, mainw->effort, dfcount, prefs->pb_quality);
}

