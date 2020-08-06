// machinestate.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#include <sys/statvfs.h>
#include <malloc.h>
#include "main.h"
#include "callbacks.h"

static uint64_t fastrand_val = 0;

LIVES_GLOBAL_INLINE uint64_t fastrand(void) {
  fastrand_val ^= (fastrand_val << 13); fastrand_val ^= (fastrand_val >> 7);
  fastrand_val = ((fastrand_val & 0xFFFFFFFF00000000) >> 32) | ((fastrand_val & 0xFFFFFFFF) << 32);
  fastrand_val ^= (fastrand_val << 17);
  return fastrand_val;
}

LIVES_GLOBAL_INLINE void fastrand_add(uint64_t entropy) {fastrand_val += entropy;}

LIVES_GLOBAL_INLINE double fastrand_dbl(double range) {
  static const double divd = (double)(0xFFFFFFFFFFFFFFFF); return (double)fastrand() / divd * range;
}

/// pick a pseudo random uint between 0 and range (inclusive)
LIVES_GLOBAL_INLINE uint32_t fastrand_int(uint32_t range) {return (uint32_t)(fastrand_dbl((double)(++range)));}

LIVES_GLOBAL_INLINE void lives_srandom(unsigned int seed) {srandom(seed);}

LIVES_GLOBAL_INLINE uint64_t lives_random(void) {return random();}


uint64_t gen_unique_id(void) {
  static uint64_t last_rnum = 0;
  uint64_t rnum;
  int randres = getentropy(&rnum, 8);
  if (randres) {
    fastrand_val = lives_random();
    fastrand();
    fastrand_val ^= lives_get_current_ticks();
    rnum = fastrand();
  }
  /// if we have a genuine RNG for 64 bits, then the probability of generating
  // a numbr < 1 billion is approx. 2 ^ 30 / 2 ^ 64 or about 1 chance in 17 trillion
  // the chance of it happening the first time is thus minscule
  // and the chance of it happening twice by chance is so unlikely we should discount it
  if (rnum < BILLIONS(1) && last_rnum < BILLIONS(1)) abort();
  last_rnum = rnum;
  return rnum;
}


void init_random() {
  uint32_t rseed;
  if (getentropy(&rseed, 4)) rseed = (gen_unique_id() & 0xFFFFFFFF);
  lives_srandom(rseed);
  fastrand_val = gen_unique_id();
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


static const char *get_tunert(int idx) {
  switch (idx) {
  case 1: return "mallopt arenas"; /// disused
  case 2: return "orc_memcpy cutoff";
  case 3: return "read buffer size (small)";
  case 4: return "read buffer size (small / medium)";
  case 5: return "read buffer size (medium)";
  case 6: return "read buffer size (large)";
  default: break;
  }
  return "unknown variable";
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
            if ((avcost = weed_get_double_value(*tuner, res[i], NULL)
                          / (double)weed_get_int_value(*tuner, res[j], NULL)) < costmin
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
          g_printerr("value of %s tuned to %lu\n",
                     get_tunert(weed_get_int64_value(*tuner, WEED_LEAF_INDEX, NULL)), val);
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

  if (!mainw->multitrack) {
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
  if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) {
    oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);
    return dest;
  }
  return memcpy(dest, src, n);
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
  size_t totsize = nmemb * xsize;
  if (totsize == 0) return NULL;
  if (xsize < DEF_ALIGN) {
    xsize = DEF_ALIGN;
    nmemb = (totsize / xsize) + 1;
  }
  return __builtin_assume_aligned(lives_calloc(nmemb + (EXTRA_BYTES / xsize), xsize), DEF_ALIGN);
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


char *get_md5sum(const char *filename) {
  /// for future use
  char **array;
  char *md5;
  char *com = lives_strdup_printf("%s \"%s\"", EXEC_MD5SUM, filename);
  THREADVAR(com_failed) = FALSE;
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);
  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return NULL;
  }
  array = lives_strsplit(mainw->msg, " ", 2);
  md5 = lives_strdup(array[0]);
  lives_strfreev(array);
  return md5;
}


char *lives_format_storage_space_string(uint64_t space) {
  char *fmt;

  if (space >= lives_10pow(18)) {
    // TRANSLATORS: Exabytes
    fmt = lives_strdup_printf(_("%.2f EB"), (double)space / (double)lives_10pow(18));
  } else if (space >= lives_10pow(15)) {
    // TRANSLATORS: Petabytes
    fmt = lives_strdup_printf(_("%.2f PB"), (double)space / (double)lives_10pow(15));
  } else if (space >= lives_10pow(12)) {
    // TRANSLATORS: Terabytes
    fmt = lives_strdup_printf(_("%.2f TB"), (double)space / (double)lives_10pow(12));
  } else if (space >= lives_10pow(9)) {
    // TRANSLATORS: Gigabytes
    fmt = lives_strdup_printf(_("%.2f GB"), (double)space / (double)lives_10pow(9));
  } else if (space >= lives_10pow(6)) {
    // TRANSLATORS: Megabytes
    fmt = lives_strdup_printf(_("%.2f MB"), (double)space / (double)lives_10pow(6));
  } else if (space >= 1024) {
    // TRANSLATORS: Kilobytes (1024 bytes)
    fmt = lives_strdup_printf(_("%.2f KiB"), (double)space / 1024.);
  } else {
    fmt = lives_strdup_printf(_("%d bytes"), space);
  }

  return fmt;
}


lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, int64_t *dsval) {
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)
  // call with dsval set to ds_used to check for OVER_QUOTA
  // dsval is overwritten and set to ds_free
  uint64_t ds;
  lives_storage_status_t status = LIVES_STORAGE_STATUS_UNKNOWN;
  if (dsval && prefs->disk_quota > 0 && *dsval > prefs->disk_quota)
    status = LIVES_STORAGE_STATUS_OVER_QUOTA;
  if (!is_writeable_dir(dir)) return status;
  ds = get_ds_free(dir);
  if (dsval) *dsval = ds;
  if (ds < prefs->ds_crit_level) return LIVES_STORAGE_STATUS_CRITICAL;
  if (status != LIVES_STORAGE_STATUS_UNKNOWN) return status;
  if (ds < warn_level) return LIVES_STORAGE_STATUS_WARNING;
  return LIVES_STORAGE_STATUS_NORMAL;
}


boolean get_ds_used(int64_t *bytes) {
  /// returns bytes used for the workdir
  /// because this may take some time on some OS, a background thread is run and  FALSE is returned along with the last
  /// read value in bytes
  /// once a new value is obtained TRUE is returned and bytes will reflect the updated val
  boolean ret = TRUE;
  static uint64_t _bytes = 0;
  static lives_proc_thread_t running = NULL;
  if (!running) running = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)get_dir_size, WEED_SEED_INT64, "s",
                            prefs->workdir);
  if (!lives_proc_thread_check(running)) ret = FALSE;
  else {
    _bytes = lives_proc_thread_join_int64(running);
    running = FALSE;
  }
  if (bytes) *bytes = _bytes;
  capable->ds_used = _bytes;
  return ret;
}

uint64_t get_ds_free(const char *dir) {
  // get free space in bytes for volume containing directory dir
  // return 0 if we cannot create/write to dir

  // caller should test with is_writeable_dir() first before calling this
  // since 0 is a valid return value

  // dir should be in locale encoding

  // WARNING: this may temporarily create the directory (since we dont know if its parents are needed)

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
  if (!strcmp(dir, prefs->workdir)) {
    capable->ds_free = bytes;
    capable->ds_tot = sbuf.f_bsize * sbuf.f_blocks;
  }

getfserr:
  if (must_delete) lives_rmdir(dir, FALSE);

  return bytes;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(ticks_t origsecs, ticks_t orignsecs) {
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((ts.tv_sec * ONE_BILLION + ts.tv_nsec) - (origsecs * ONE_BILLION + orignsecs)) / TICKS_TO_NANOSEC;
#else
#ifdef USE_MONOTONIC_TIME
  return (lives_get_monotonic_time() - orignsecs) / 10;
#else
  gettimeofday(&tv, NULL);
  return ((tv.tv_sec * ONE_MILLLION + tv.tv_usec) - (origsecs * ONE_MILLION + orignsecs / 1000)) * USEC_TO_TICKS;
#endif
#endif
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks(void) {
  //  return current (wallclock) time in ticks (units of 10 nanoseconds)
  return lives_get_relative_ticks(0, 0);
}


#define SECS_IN_DAY 86400
char *lives_datetime_rel(const char *datetime) {
  /// replace date w. yesterday, today
  char *dtxt;
  char *today = NULL, *yesterday = NULL;
  struct timeval otv;
  gettimeofday(&otv, NULL);
  today = lives_datetime(otv.tv_sec);
  yesterday = lives_datetime(otv.tv_sec - SECS_IN_DAY);
  if (!lives_strncmp(datetime, today, 10)) dtxt = lives_strdup_printf(_("Today %s"), datetime + 11);
  else if (!lives_strncmp(datetime, yesterday, 10))
    dtxt = lives_strdup_printf(_("Yesterday %s"), datetime + 11);
  else dtxt = (char *)datetime;
  if (today) lives_free(today);
  if (yesterday) lives_free(yesterday);
  return dtxt;
}


char *lives_datetime(uint64_t secs) {
  char buf[128];
  char *datetime = NULL;
  struct tm *gm = gmtime((time_t *)&secs);
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


boolean compress_files_in_dir(const char *dir, int method, void *data) {
  /// compress all files in dir with gzip (directories are not compressed)
  /// gzip default action is to compress all files, replacing foo.bar with foo.bar.gz
  /// if a file already has a .gz extension then it will be left uncchanged

  /// in future, method and data may be used to select compression method
  /// for now they are ignored
  char buff[65536];
  char *com, *cwd;
  boolean retval = FALSE;

  if (!check_for_executable(&capable->has_gzip, EXEC_GZIP)) return FALSE;
  if (lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
    boolean needs_norem = FALSE;
    char *norem = lives_build_filename(dir, LIVES_FILENAME_NOREMOVE, NULL);
    if (lives_file_test(norem, LIVES_FILE_TEST_EXISTS)) {
      needs_norem = TRUE;
      lives_rm(norem);
    }
    cwd = lives_get_current_dir();
    THREADVAR(chdir_failed) = FALSE;
    lives_chdir(dir, TRUE);
    if (THREADVAR(chdir_failed)) {
      THREADVAR(chdir_failed) = FALSE;
      lives_chdir(cwd, TRUE);
      lives_free(cwd);
      lives_free(norem);
      return FALSE;
    }
    com = lives_strdup_printf("%s * 2>&1", EXEC_GZIP);
    THREADVAR(com_failed) = FALSE;
    lives_popen(com, TRUE, buff, 65536);
    lives_free(com);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    else retval = TRUE;
    lives_chdir(cwd, TRUE);
    lives_free(cwd);
    if (needs_norem) {
      lives_touch(norem);
      lives_free(norem);
    }
  }
  return retval;
}


size_t get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  size_t fsize;
  lives_file_buffer_t *fbuff;
  fstat(fd, &filestat);
  fsize = (uint64_t)(filestat.st_size);
  //g_printerr("fssize for %d is %ld\n", fd, fsize);
  if ((fbuff = find_in_file_buffers(fd)) != NULL) {
    if (!fbuff->read) {
      /// because of padding bytes...
      size_t f2size;
      if ((f2size = (size_t)(fbuff->offset + fbuff->bytes)) > fsize) return f2size;
    }
  }
  return fsize;
}


size_t sget_file_size(const char *name) {
  // get the size of file fd
  int fd;
  size_t fsize;
  if ((fd = open(name, O_RDONLY)) == -1) return 0;
  fsize = get_file_size(fd);
  close(fd);
  return fsize;
}


void reget_afilesize(int fileno) {
  // re-get the audio file size
  lives_clip_t *sfile = mainw->files[fileno];
  boolean bad_header = FALSE;

  if (mainw->multitrack) return; // otherwise achans gets set to 0...

  sfile->afilesize = reget_afilesize_inner(fileno);

  if (sfile->afilesize == 0l) {
    if (!sfile->opening && fileno != mainw->ascrap_file && fileno != mainw->scrap_file) {
      if (sfile->arate != 0 || sfile->achans != 0 || sfile->asampsize != 0 || sfile->arps != 0) {
        sfile->arate = sfile->achans = sfile->asampsize = sfile->arps = 0;
        if (!save_clip_value(fileno, CLIP_DETAILS_ACHANS, &sfile->achans)) bad_header = TRUE;
        if (!save_clip_value(fileno, CLIP_DETAILS_ARATE, &sfile->arps)) bad_header = TRUE;
        if (!save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &sfile->arate)) bad_header = TRUE;
        if (!save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &sfile->asampsize)) bad_header = TRUE;
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


boolean is_empty_dir(const char *dirname) {
  DIR *tldir;
  struct dirent *tdirent;
  boolean empty = TRUE;
  if (!dirname) return TRUE;
  tldir = opendir(dirname);
  if (!tldir) return FALSE;
  while (empty && (tdirent = readdir(tldir))) {
    if (tdirent->d_name[0] == '.'
        && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
    empty = FALSE;
  }
  closedir(tldir);
  return empty;
}


char *get_mountpoint_for(const char *dir) {
  FILE *mountinfo;
  char **array;
  char *mp = NULL;
  size_t lmatch = 0, slen;
  char buff[65536];
  int j;

  if (!dir) return NULL;
  slen = lives_strlen(dir);

  if (!(mountinfo = fopen(MOUNTINFO, "r"))) return NULL;
  while (lives_fgets(buff, 65536, mountinfo)) {
    array = lives_strsplit(buff, " ", 3);
    for (j = 0; array[1][j] && j < slen; j++) if (array[1][j] != dir[j]) break;
    if (j > lmatch && !array[1][j]) {
      lmatch = j;
      if (mp) lives_free(mp);
      mp = lives_strdup(array[0]);
    }
    lives_strfreev(array);
  }
  fclose(mountinfo);
  return mp;
}


ssize_t get_dir_size(const char *dirname) {
  ssize_t dirsize = -1;
  if (check_for_executable(&capable->has_du, EXEC_DU)) {
    char buff[PATH_MAX * 2];
    char *com = lives_strdup_printf("%s -sb0 \"%s\"", EXEC_DU, dirname);
    THREADVAR(com_failed) = FALSE;
    lives_popen(com, TRUE, buff, PATH_MAX * 2);
    lives_free(com);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    else dirsize = atol(buff);
  }
  return dirsize;
}


void free_fdets_list(LiVESList **listp) {
  LiVESList *list = *listp;
  lives_file_dets_t *filedets;
  for (; list && list->data; list = list->next) {
    filedets = (lives_file_dets_t *)list->data;
    lives_struct_free(filedets->lsd);
    list->data = NULL;
  }
  if (*listp) {
    lives_list_free(*listp);
    *listp = NULL;
  }
}


int stat_to_file_dets(const char *fname, lives_file_dets_t *fdets) {
  struct stat filestat;
  int ret = stat(fname, &filestat);
  if (ret) {
    perror("stat failed:");
    fdets->size = -2;
    fdets->type = LIVES_FILE_TYPE_UNKNOWN;
    return ret;
  }
  fdets->type = (uint64_t)((filestat.st_mode & S_IFMT) >> 12);
  fdets->size = filestat.st_size;
  fdets->blk_size = (uint64_t)filestat.st_blksize;
  fdets->atime_sec = filestat.st_atim.tv_sec;
  fdets->atime_nsec = filestat.st_atim.tv_nsec;
  fdets->mtime_sec = filestat.st_mtim.tv_sec;
  fdets->mtime_nsec = filestat.st_mtim.tv_nsec;
  fdets->ctime_sec = filestat.st_ctim.tv_sec;
  fdets->ctime_nsec = filestat.st_ctim.tv_nsec;
  return ret;
}


static char *file_to_file_details(const char *filename, lives_file_dets_t *fdets, lives_proc_thread_t tinfo, uint64_t extra) {
  char *tmp;
  char *extra_details = lives_strdup("");

  if (!stat_to_file_dets(filename, fdets)) {
    // if stat fails, we have set set size to -2, type to LIVES_FILE_TYPE_UNKNOWN
    // and here we set extra_details to ""
    if (tinfo && lives_proc_thread_cancelled(tinfo)) {
      lives_free(extra_details);
      return NULL;
    }
    if (fdets->type == LIVES_FILE_TYPE_DIRECTORY) {
      boolean emptyd = FALSE;
      if (extra & EXTRA_DETAILS_EMPTY_DIR) {
        if ((emptyd = is_empty_dir(filename))) {
          extra_details = lives_strdup_printf("%s%s%s", extra_details, *extra_details ? ", " : "",
                                              (tmp = _("(empty)")));
          lives_free(tmp);
        }
        if (tinfo && lives_proc_thread_cancelled(tinfo)) {
          lives_free(extra_details);
          return NULL;
        }
      }
      if ((extra & EXTRA_DETAILS_DIRSIZE) &&
          check_for_executable(&capable->has_du, EXEC_DU)
          && !emptyd && fdets->type == LIVES_FILE_TYPE_DIRECTORY) {
        fdets->size = get_dir_size(filename);
      }

      if (!emptyd && (extra & EXTRA_DETAILS_CLIPHDR)) {
        int clipno;

        clipno = create_nullvideo_clip("tmp");

        if (clipno && IS_VALID_CLIP(clipno)) {
          if (read_headers(clipno, filename, NULL)) {
            lives_clip_t *sfile = mainw->files[clipno];
            char *name = lives_strdup(sfile->name);
            extra_details =
              lives_strdup_printf("%s%s%s", extra_details, *extra_details ? ", " : "",
                                  (tmp = lives_strdup_printf
                                         (_("Source: %s, frames: %d, size: %d X %d, fps: %.3f"),
                                          name, sfile->frames, sfile->hsize,
                                          sfile->vsize, sfile->fps)));
            lives_free(tmp);
            lives_free(name);
            lives_freep((void **)&mainw->files[clipno]);
            if (mainw->first_free_file == ALL_USED || mainw->first_free_file > clipno)
              mainw->first_free_file = clipno;
          }
          if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (extra & EXTRA_DETAILS_MD5) {
    fdets->md5sum = get_md5sum(filename);
  }
  return extra_details;
}


/**
   @brief create a list from a (sub)directory
   '.' and '..' are ignored
   subdir can be NULL
*/
void *_item_to_file_details(LiVESList **listp, const char *item,
                            const char *orig_loc, uint64_t extra, int type) {
  // type 0 = dir
  // type 1 = ordfile
  lives_file_dets_t *fdets;
  lives_proc_thread_t tinfo = NULL;
  LiVESList *list;
  char *extra_details;
  const char *dir = NULL;
  char *subdirname;
  boolean empty = TRUE;

  tinfo = THREADVAR(tinfo);
  if (tinfo) lives_proc_thread_set_cancellable(tinfo);

  switch (type) {
  case 0: {
    DIR *tldir;
    struct dirent *tdirent;
    // dir
    dir = item;
    if (!dir) return NULL;
    tldir = opendir(dir);
    if (!tldir) {
      *listp = lives_list_append(*listp, NULL);
      return NULL;
    }

    while (1) {
      tdirent = readdir(tldir);
      if (lives_proc_thread_cancelled(tinfo) || !tdirent) {
        closedir(tldir);
        if (lives_proc_thread_cancelled(tinfo)) return NULL;
        break;
      }
      if (tdirent->d_name[0] == '.'
          && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
      fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);
      fdets->name = lives_strdup(tdirent->d_name);
      fdets->size = -1;
      *listp = lives_list_append(*listp, fdets);
      if (lives_proc_thread_cancelled(tinfo)) {
        closedir(tldir);
        return NULL;
      }
    }
    break;
  }
  case 1: {
    FILE *orderfile;
    char buff[PATH_MAX];
    const char *ofname = item;

    if (!(orderfile = fopen(ofname, "r"))) return NULL;
    if (lives_proc_thread_cancelled(tinfo) || !orderfile) {
      if (lives_proc_thread_cancelled(tinfo)) return NULL;
      break;
    }
    if (!lives_fgets(buff, PATH_MAX, orderfile)) {
      fclose(orderfile);
      break;
    }
    lives_chomp(buff);
    fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);

    fdets->name = lives_strdup(buff);
    fdets->size = -1;
    *listp = lives_list_append(*listp, fdets);
    if (lives_proc_thread_cancelled(tinfo)) {
      fclose(orderfile);
      return NULL;
    }
    fclose(orderfile);
    break;
  }
  default: return NULL;
  }

  if (*listp) empty = FALSE;
  *listp = lives_list_append(*listp, NULL);

  if (empty || lives_proc_thread_cancelled(tinfo)) return NULL;

  // listing done, now get details for each entry
  list = *listp;
  while (list && list->data) {
    if (lives_proc_thread_cancelled(tinfo)) return NULL;

    extra_details = lives_strdup("");
    fdets = (lives_file_dets_t *)list->data;

    if (orig_loc && *orig_loc) subdirname = lives_build_filename(orig_loc, fdets->name, NULL);
    else subdirname = lives_build_path(dir, fdets->name, NULL);

    // need to call even with no extra, because it gets size / type tc.
    if (!(extra_details = file_to_file_details(subdirname, fdets, tinfo, extra))) {
      lives_free(subdirname);
      lives_free(extra_details);
      return NULL;
    }

    lives_free(subdirname);

    if (tinfo && lives_proc_thread_cancelled(tinfo)) {
      lives_free(extra_details);
      return NULL;
    }
    fdets->extra_details = lives_strdup(extra_details);
    lives_free(extra_details);
    list = list->next;
  }

  return NULL;
}

/**
   @brief create a list from a (sub)directory
   '.' and '..' are ignored
   subdir can be NULL
   runs in a proc_htread
*/
lives_proc_thread_t dir_to_file_details(LiVESList **listp, const char *dir,
                                        const char *orig_loc, uint64_t extra) {
  return lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)_item_to_file_details, -1, "vssIi",
                                  listp, dir, orig_loc, extra, 0);
}


lives_proc_thread_t ordfile_to_file_details(LiVESList **listp, const char *ofname,
    const char *orig_loc, uint64_t extra) {
  return lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)_item_to_file_details, -1, "vssIi",
                                  listp, ofname, orig_loc, extra, 1);
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
    widget_opts.non_modal = TRUE;
    do_error_dialog(
      _("Your version of mplayer/ffmpeg may be broken !\nSee http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    widget_opts.non_modal = FALSE;
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}


LIVES_GLOBAL_INLINE char *lives_concat_sep(char *st, const char *sep, char *x) {
  /// nb: lives strconcat
  // uses realloc / memcpy, frees x
  char *tmp;
  if (st) {
    size_t s1 = lives_strlen(st), s2 = lives_strlen(x), s3 = lives_strlen(sep);
    tmp = (char *)lives_realloc(st, ++s2 + s1 + s3);
    lives_memcpy(tmp + s1, sep, s3);
    lives_memcpy(tmp + s1 + s3, x, s2);
  } else tmp = lives_strdup(x);
  lives_free(x);
  return tmp;
}

LIVES_GLOBAL_INLINE char *lives_concat(char *st, char *x) {
  /// nb: lives strconcat
  // uses realloc / memcpy, frees x
  size_t s1 = lives_strlen(st), s2 = lives_strlen(x);
  char *tmp = (char *)lives_realloc(st, ++s2 + s1);
  lives_memcpy(tmp + s1, x, s2);
  lives_free(x);
  return tmp;
}

LIVES_GLOBAL_INLINE int lives_strappend(const char *string, int len, const char *xnew) {
  /// see also: lives_concat()
  size_t sz = lives_strlen(string);
  int newln = lives_snprintf((char *)(string + sz), len - sz, "%s", xnew);
  if (newln > len) newln = len;
  return --newln - sz; // returns strlen(xnew)
}

LIVES_GLOBAL_INLINE const char *lives_strappendf(const char *string, int len, const char *fmt, ...) {
  va_list xargs;
  char *text;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  lives_strappend(string, len, text);
  lives_free(text);
  return string;
}

/// each byte B can be thought of as a signed char, subtracting 1 sets bit 7 if B was <= 0, then AND with ~B clears bit 7 if it
/// was already set (i.e B was < 0), thus bit 7 only remains set if the byte started as 0.
#define hasNulByte(x) (((x) - 0x0101010101010101) & ~(x) & 0x8080808080808080)
#define getnulpos(nulmask) ((nulmask & 2155905152ul)	? ((nulmask & 32896ul) ? ((nulmask & 128ul) ? 0 : 1) : \
							   ((nulmask & 8388608ul) ? 2 : 3)) : (nulmask & 141287244169216ul) ? \
			    ((nulmask & 549755813888ul) ? 4 : 5) : ((nulmask & 36028797018963968ul) ? 6 : 7))

#define getnulpos_be(nulmask) ((nulmask & 9259542121117908992ul) ? ((nulmask & 9259400833873739776ul) ? \
								    ((nulmask & 9223372036854775808ul) ? 0 : 1) : ((nulmask & 140737488355328ul) ? 2 : 3)) \
			       : (nulmask & 2155872256ul) ? ((nulmask & 2147483648ul) ? 4 : 5) : ((nulmask & 32768ul) ? 6 : 7))

LIVES_GLOBAL_INLINE size_t lives_strlen(const char *s) {
  if (!s) return 0;
#ifndef STD_STRINGFUNCS
  else {
    uint64_t *pi = (uint64_t *)s, nulmask;
    if ((void *)pi == (void *)s) {
      while (!(nulmask = hasNulByte(*pi))) pi++;
      return (char *)pi - s + (capable->byte_order == LIVES_LITTLE_ENDIAN ? getnulpos(nulmask)
                               : getnulpos_be(nulmask));
    }
  }
#endif
  return strlen(s);
}


LIVES_GLOBAL_INLINE char *lives_strdup_quick(const char *s) {
  if (!s) return NULL;
#ifndef STD_STRINGFUNCS
  else {
    uint64_t *pi = (uint64_t *)s, nulmask, stlen;
    if (!s) return NULL;
    if ((void *)pi == (void *)s) {
      while (!(nulmask = hasNulByte(*pi))) pi++;
      stlen = (char *)pi - s + 1
              + (capable->byte_order == LIVES_LITTLE_ENDIAN)
              ? getnulpos(nulmask) : getnulpos_be(nulmask);
      return lives_memcpy(lives_malloc(stlen), s, stlen);
    }
  }
#endif
  return lives_strdup(s);
}




/// returns FALSE if strings match
LIVES_GLOBAL_INLINE boolean lives_strcmp(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
#ifdef STD_STRINGFUNCS
    return strcmp(st1, st2);
#endif
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
        while (1) {
          if ((d1 = *(ip1++)) == (d2 = *(ip2++))) {if (hasNulByte(d1)) return FALSE;}
          else {
            if (!hasNulByte(d1 | d2)) return TRUE;
            break;
          }
        }
        st1 = (const char *)(--ip1); st2 = (const char *)(--ip2);
      }
      if (*st1 != *(st2++)) return TRUE;
      if (!(*(st1++))) return FALSE;
    }
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE int lives_strcmp_ordered(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
#ifdef STD_STRINGFUNCS
    return strcmp(st1, st2);
#endif
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
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
#ifdef STD_STRINGFUNCS
    return strncmp(st1, st2, len);
#endif
    size_t xlen = len >> 3;
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if (xlen && (void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1) && --xlen);
        if (xlen) {
          if (!hasNulByte(d2)) return TRUE;
          ip1--;
          ip2--;
        }
        st1 = (void *)ip1; st2 = (void *)ip2;
        len -= ((len >> 3) - xlen) << 3;
      }
      if (!(len--)) return FALSE;
      if (*st1 != *(st2++)) return TRUE;
      if (!(*(st1++))) return FALSE;
    }
  }
  return (*st1 != *st2);
}

#define HASHROOT 5381
LIVES_GLOBAL_INLINE uint32_t lives_string_hash(const char *st) {
  if (st) for (uint32_t hash = HASHROOT;; hash += (hash << 5)
                 + * (st++)) if (!(*st)) return hash;
  return 0;
}


// fast hash from: http://www.azillionmonkeys.com/qed/hash.html
// (c) Paul Hsieh
#define get16bits(d) (*((const uint16_t *) (d)))

LIVES_GLOBAL_INLINE uint32_t fast_hash(const char *key) {
  /// approx 5 - 10 % faster than lives_string_hash
  if (key && *key) {
    int len = lives_strlen(key), rem = len & 3;
    uint32_t hash = len + HASHROOT, tmp;
    len >>= 2;
    for (; len > 0; len--) {
      hash  += get16bits(key);
      tmp    = (get16bits(key + 2) << 11) ^ hash;
      hash   = (hash << 16) ^ tmp;
      key  += 4;
      hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
    case 3: hash += get16bits(key);
      hash ^= hash << 16;
      hash ^= ((int8_t)key[2]) << 18;
      hash += hash >> 11;
      break;
    case 2: hash += get16bits(key);
      hash ^= hash << 11; hash += hash >> 17;
      break;
    case 1: hash += (int8_t) * key;
      hash ^= hash << 10; hash += hash >> 1;
      break;
    default: break;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3; hash += hash >> 5; hash ^= hash << 4;
    hash += hash >> 17; hash ^= hash << 25; hash += hash >> 6;
    return hash;
  }
  return 0;
}

LIVES_GLOBAL_INLINE char *lives_strstop(char *st, const char term) {
  /// truncate st, replacing term with \0
  if (st && term) for (char *p = (char *)st; *p; p++) if (*p == term) {*p = 0; return st;}
  return st;
}

LIVES_GLOBAL_INLINE size_t lives_chomp(char *buff) {
  size_t xs = lives_strlen(buff);
  if (xs && buff[xs - 1] == '\n') buff[--xs] = '\0'; // remove trailing newline
  return xs;
}


/**
   lives  proc_threads API
   - the only requirements are to call lives_proc_thread_create() which will generate a lives_proc_thread_t and run it,
   and then (depending on the return_type parameter, call one of the lives_proc_thread_join_*() functions

   (see that function for more comments)
*/

typedef weed_plantptr_t lives_proc_thread_t;

static lives_proc_thread_t _lives_proc_thread_create(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs) {
  int p = 0;
  const char *c;
  weed_plant_t *thread_info = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  if (!thread_info) return NULL;
  weed_set_funcptr_value(thread_info, WEED_LEAF_THREADFUNC, func);
  if (return_type) {
    pthread_mutex_t *dcmutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(dcmutex, NULL);
    weed_set_voidptr_value(thread_info, WEED_LEAF_DONTCARE_MUTEX, dcmutex);
    weed_set_boolean_value(thread_info, WEED_LEAF_NOTIFY, WEED_TRUE);
    if (return_type > 0)  weed_leaf_set(thread_info, WEED_LEAF_RETURN_VALUE, return_type, 0, NULL);
  }
  c = args_fmt;
  for (c = args_fmt; *c; c++) {
    char *pkey = lives_strdup_printf("%s%d", WEED_LEAF_THREAD_PARAM, p++);
    switch (*c) {
    case 'i': weed_set_int_value(thread_info, pkey, va_arg(xargs, int)); break;
    case 'd': weed_set_double_value(thread_info, pkey, va_arg(xargs, double)); break;
    case 'b': weed_set_boolean_value(thread_info, pkey, va_arg(xargs, int)); break;
    case 's': case 'S': weed_set_string_value(thread_info, pkey, va_arg(xargs, char *)); break;
    case 'I': weed_set_int64_value(thread_info, pkey, va_arg(xargs, int64_t)); break;
    case 'F': weed_set_funcptr_value(thread_info, pkey, va_arg(xargs, weed_funcptr_t)); break;
    case 'V': case 'v': weed_set_voidptr_value(thread_info, pkey, va_arg(xargs, void *)); break;
    case 'P': weed_set_plantptr_value(thread_info, pkey, va_arg(xargs, weed_plantptr_t)); break;
    default: weed_plant_free(thread_info); return NULL;
    }
    lives_free(pkey);
  }

  if (!(attr & LIVES_THRDATTR_FG_THREAD)) {
    resubmit_proc_thread(thread_info, attr);
    if (!return_type) return NULL;
  }
  return thread_info;
}


/**
   create the specific plant which defines a background task to be run
   - func is any function of a recognised type, with 0 - 16 parameters,
   and a value of type <return type> which may be retrieved by
   later calling the appropriate lives_proc_thread_join_*() function
   - args_fmt is a 0 terminated string describing the arguments of func, i ==int, d == double, b == boolean (int),
   s == string (0 terminated), I == uint64_t, int64_t, P = weed_plant_t *, V / v == (void *), F == weed_funcptr_t
   return_type is enumerated, e.g WEED_SEED_INT64. Return_type of 0 indicates no return value (void), then the thread
   will free its own resources and NULL is returned from this function (fire and forget)
   return_type of -1 has a special meaning, in this case no result is returned, but the thread can be monitored by calling:
   lives_proc_thread_check() with the return : - this function is guaranteed to return FALSE whilst the thread is running
   and TRUE thereafter, the proc_thread should be freed once TRUE id returned and not before.
   for the other return_types, the appropriate join function should be called and it will block until the thread has completed its
   task and return a copy of the actual return value of the func
   alternatively, if return_type is non-zero,
   then the returned value from this function may be reutlised by passing it as the parameter
   to run_as_thread(). */
lives_proc_thread_t lives_proc_thread_create(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  va_list xargs;
  va_start(xargs, args_fmt);
  lpt = _lives_proc_thread_create(attr, func, return_type, args_fmt, xargs);
  va_end(xargs);
  return lpt;
}


void *main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  va_list xargs;
  void *ret;
  va_start(xargs, args_fmt);
  lpt = _lives_proc_thread_create(LIVES_THRDATTR_FG_THREAD, func, return_type, args_fmt, xargs);
  ret = lives_fg_run(lpt, retval);
  va_end(xargs);
  return ret;
}


static void call_funcsig(funcsig_t sig, lives_proc_thread_t info) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion posibilities (nargs < 16 * all return types)
  /// it is not feasable to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  allfunc_t *thefunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  char *msg;

  thefunc->func = weed_get_funcptr_value(info, WEED_LEAF_THREADFUNC, NULL);

#define FUNCSIG_VOID				       			0X00000000
#define FUNCSIG_INT 			       				0X00000001
#define FUNCSIG_STRING 				       			0X00000004
#define FUNCSIG_VOIDP 				       			0X0000000D
#define FUNCSIG_INT_INT64 			       			0X00000015
#define FUNCSIG_STRING_INT 			      			0X00000041
#define FUNCSIG_VOIDP_VOIDP 				       		0X000000DD
#define FUNCSIG_PLANTP_BOOL 				       		0X000000E3
#define FUNCSIG_VOIDP_VOIDP_VOIDP 		        		0X00000DDD
#define FUNCSIG_PLANTP_VOIDP_INT64 		        		0X00000ED5
  // 4p
#define FUNCSIG_STRING_STRING_VOIDP_INT					0X000044D1
  // 5p
#define FUNCSIG_INT_INT_INT_BOOL_VOIDP					0X0001113D
#define FUNCSIG_VOIDP_STRING_STRING_INT64_INT			       	0X000D4451
  // 6p
#define FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP		       	0X0044D14D

  // Note: C compilers don't care about the type / number of function args., (else it would be impossible to alias any function pointer)
  // just the type / number must be correct at runtime;
  // However it DOES care about the return type. The funcsigs are a guide so that the correct cast / number of args. can be
  // determined in the code., the first argument to the GETARG macro is set by this.
  // return_type determines which function flavour to call, e.g func, funcb, funci
  /// the second argument to GETARG relates to the internal structure of the lives_proc_thread;

  /// LIVES_PROC_THREADS ////////////////////////////////////////

  /// to make any function usable by lives_proc_thread, the _ONLY REQUIREMENT_ is to ensure that there is a function call
  /// corresponding the function arguments (i.e the funcsig) and return value here below
  /// (use of the FUNCSIG_* symbols is optional, they exist only to make it clearer what the function parameters should be)

  switch (sig) {
  case FUNCSIG_VOID:
    switch (ret_type) {
    case WEED_SEED_INT64: CALL_0(int64); break;
    default: CALL_VOID_0(); break;
    }
    break;
  case FUNCSIG_INT:
    switch (ret_type) {
    default: CALL_VOID_1(int); break;
    }
    break;
  case FUNCSIG_STRING:
    switch (ret_type) {
    case WEED_SEED_STRING: CALL_1(string, string); break;
    case WEED_SEED_INT64: CALL_1(int64, string); break;
    default: CALL_VOID_1(string); break;
    }
    break;
  case FUNCSIG_VOIDP:
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_1(boolean, voidptr); break;
    default: CALL_VOID_1(voidptr); break;
    }
    break;
  case FUNCSIG_INT_INT64:
    switch (ret_type) {
    default: CALL_VOID_2(int, int64); break;
    }
    break;
  case FUNCSIG_STRING_INT:
    switch (ret_type) {
    default: CALL_VOID_2(string, int); break;
    }
    break;
  case FUNCSIG_VOIDP_VOIDP:
    switch (ret_type) {
    default: CALL_VOID_2(voidptr, voidptr); break;
    }
    break;
  case FUNCSIG_PLANTP_BOOL:
    switch (ret_type) {
    default: CALL_VOID_2(plantptr, boolean); break;
    }
    break;
  case FUNCSIG_VOIDP_VOIDP_VOIDP:
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_3(boolean, voidptr, voidptr, voidptr); break;
    default: CALL_VOID_3(voidptr, voidptr, voidptr); break;
    }
    break;
  case FUNCSIG_PLANTP_VOIDP_INT64:
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_3(boolean, plantptr, voidptr, int64); break;
    default: CALL_VOID_3(plantptr, voidptr, int64); break;
    }
    break;
  case FUNCSIG_STRING_STRING_VOIDP_INT:
    switch (ret_type) {
    case WEED_SEED_STRING: CALL_4(string, string, string, voidptr, int); break;
    default: CALL_VOID_4(string, string, voidptr, int); break;
    }
    break;
  case FUNCSIG_VOIDP_STRING_STRING_INT64_INT:
    switch (ret_type) {
    default: CALL_VOID_5(voidptr, string, string, int64, int); break;
    }
    break;
  case FUNCSIG_INT_INT_INT_BOOL_VOIDP:
    switch (ret_type) {
    default: CALL_VOID_5(int, int, int, boolean, voidptr); break;
    }
    break;
  case FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP:
    switch (ret_type) {
    case WEED_SEED_STRING: CALL_6(string, string, string, voidptr, int, string, voidptr); break;
    default: CALL_VOID_6(string, string, voidptr, int, string, voidptr); break;
    }
    break;
  default:
    msg = lives_strdup_printf("Unknown funcsig with tyte 0x%016lX called", sig);
    LIVES_FATAL(msg);
    lives_free(msg);
    break;
  }

  lives_free(thefunc);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_sync_ready(lives_proc_thread_t tinfo) {
  volatile boolean *sync_ready = (volatile boolean *)weed_get_voidptr_value(tinfo, "sync_ready", NULL);
  if (sync_ready) *sync_ready = TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_check(lives_proc_thread_t tinfo) {
  /// returns FALSE while the thread is running, TRUE once it has finished
  return (weed_leaf_num_elements(tinfo, _RV_) > 0
          || weed_get_boolean_value(tinfo, WEED_LEAF_DONE, NULL) == WEED_TRUE);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_cancellable(lives_proc_thread_t tinfo) {
  weed_set_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLABLE, WEED_TRUE);
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t tinfo) {
  return weed_get_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLABLE, NULL) == WEED_TRUE ? TRUE : FALSE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t tinfo) {
  if (!lives_proc_thread_get_cancellable(tinfo)) return FALSE;
  weed_set_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLED, WEED_TRUE);
  lives_proc_thread_join(tinfo);
  return TRUE;
}

boolean lives_proc_thread_dontcare(lives_proc_thread_t tinfo) {
  /// if thread is running, tell it we no longer care about return value, so it can free itself
  /// if finished we just call lives_proc_thread_join() to free it
  /// a mutex is used to ensure the proc_thread does not finish between setting the flag and checking if it has ifnished
  pthread_mutex_t *dcmutex = weed_get_voidptr_value(tinfo, WEED_LEAF_DONTCARE_MUTEX, NULL);
  if (dcmutex) {
    pthread_mutex_lock(dcmutex);
    if (!lives_proc_thread_check(tinfo)) {
      weed_set_boolean_value(tinfo, WEED_LEAF_DONTCARE, WEED_TRUE);
      pthread_mutex_unlock(dcmutex);
    } else {
      pthread_mutex_unlock(dcmutex);
      lives_proc_thread_join(tinfo);
    }
  }
  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancelled(lives_proc_thread_t tinfo) {
  return (tinfo && weed_get_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLED, NULL) == WEED_TRUE)
         ? TRUE : FALSE;
}

#define _join(stype) lives_nanosleep_until_nonzero(weed_leaf_num_elements(tinfo, _RV_)); \
  return weed_get_##stype##_value(tinfo, _RV_, NULL);

LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t tinfo) {
  // WARNING !! version without a return value will free tinfo !
  void *dcmutex;
  lives_nanosleep_until_nonzero((weed_get_boolean_value(tinfo, WEED_LEAF_DONE, NULL) == WEED_TRUE));
  dcmutex = weed_get_voidptr_value(tinfo, WEED_LEAF_DONTCARE_MUTEX, NULL);
  if (dcmutex) lives_free(dcmutex);
  weed_plant_free(tinfo);
}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_int(lives_proc_thread_t tinfo) { _join(int);}
LIVES_GLOBAL_INLINE double lives_proc_thread_join_double(lives_proc_thread_t tinfo) {_join(double);}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_boolean(lives_proc_thread_t tinfo) { _join(boolean);}
LIVES_GLOBAL_INLINE int64_t lives_proc_thread_join_int64(lives_proc_thread_t tinfo) {_join(int64);}
LIVES_GLOBAL_INLINE char *lives_proc_thread_join_string(lives_proc_thread_t tinfo) {_join(string);}
LIVES_GLOBAL_INLINE weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t tinfo) {_join(funcptr);}
LIVES_GLOBAL_INLINE void *lives_proc_thread_join_voidptr(lives_proc_thread_t tinfo) {_join(voidptr);}
LIVES_GLOBAL_INLINE weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t tinfo) {_join(plantptr);}

/**
   create a funcsig from a lives_proc_thread_t object
   the returned value can be passed to call_funcsig, along with the original lives_proc_thread_t
*/
static funcsig_t make_funcsig(lives_proc_thread_t func_info) {
  funcsig_t funcsig = 0;
  for (register int nargs = 0; nargs < 16; nargs++) {
    char *lname = lives_strdup_printf("%s%d", WEED_LEAF_THREAD_PARAM, nargs);
    int st = weed_leaf_seed_type(func_info, lname);
    lives_free(lname);
    if (!st) break;
    funcsig <<= 4;  /// 4 bits per argtype, hence up to 16 args in a uint64_t
    if (st < 12) funcsig |= st; // 1 == int, 2 == double, 3 == boolean (int), 4 == char *, 5 == int64_t
    else {
      switch (st) {
      case WEED_SEED_FUNCPTR: funcsig |= 0XC; break;
      case WEED_SEED_VOIDPTR: funcsig |= 0XD; break;
      case WEED_SEED_PLANTPTR: funcsig |= 0XE; break;
      default: funcsig |= 0XF; break;
      }
    }
  }
  return funcsig;
}

static void *_plant_thread_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  funcsig_t sig = make_funcsig(info);
  THREADVAR(tinfo) = info;
  call_funcsig(sig, info);

  if (weed_get_boolean_value(info, WEED_LEAF_NOTIFY, NULL) == WEED_TRUE) {
    boolean dontcare;
    pthread_mutex_t *dcmutex = (pthread_mutex_t *)weed_get_voidptr_value(info, WEED_LEAF_DONTCARE_MUTEX, NULL);
    pthread_mutex_lock(dcmutex);
    dontcare = weed_get_boolean_value(info, WEED_LEAF_DONTCARE, NULL);
    weed_set_boolean_value(info, WEED_LEAF_DONE, WEED_TRUE);
    pthread_mutex_unlock(dcmutex);
    if (dontcare == WEED_TRUE) {
      lives_free(dcmutex);
      weed_plant_free(info);
    }
  } else if (!ret_type) weed_plant_free(info);
  return NULL;
}


void *fg_run_func(lives_proc_thread_t lpt, void *retval) {
  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);
  funcsig_t sig = make_funcsig(lpt);

  call_funcsig(sig, lpt);

  switch (ret_type) {
  case WEED_SEED_INT: {
    int *ival = (int *)retval;
    *ival = weed_get_int_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)ival;
  }
  case WEED_SEED_BOOLEAN: {
    int *bval = (int *)retval;
    *bval = weed_get_boolean_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)bval;
  }
  case WEED_SEED_DOUBLE: {
    double *dval = (double *)retval;
    *dval = weed_get_double_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)dval;
  }
  case WEED_SEED_STRING: {
    char *chval = weed_get_string_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)chval;
  }
  case WEED_SEED_INT64: {
    int64_t *i64val = (int64_t *)retval;
    *i64val = weed_get_int64_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)i64val;
  }
  case WEED_SEED_VOIDPTR: {
    void *val;
    val = weed_get_voidptr_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return val;
  }
  case WEED_SEED_PLANTPTR: {
    weed_plant_t *pval;
    pval = weed_get_plantptr_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)pval;
  }
  /// no funcptrs or custom...yet
  default: break;
  }
  return NULL;
}

#undef _RV_

/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
void resubmit_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  lives_thread_t *thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
  thrd_work_t *work;

  /// tell the thread to clean up after itself [but it won't delete thread_info]
  attr |= LIVES_THRDATTR_AUTODELETE;
  lives_thread_create(thread, attr, _plant_thread_func, (void *)thread_info);
  work = (thrd_work_t *)thread->data;
  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    weed_set_voidptr_value(thread_info, "sync_ready", (void *) & (work->sync_ready));
  }
}


//////// worker thread pool //////////////////////////////////////////

///////// thread pool ////////////////////////
#ifndef VALGRIND_ON
#define MINPOOLTHREADS 8
#else
#define MINPOOLTHREADS 2
#endif
static int npoolthreads;
static pthread_t **poolthrds;
static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static LiVESList *twork_first, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

static LiVESList *allctxs = NULL;

lives_thread_data_t *get_thread_data(void) {
  LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
  LiVESList *list = allctxs;
  if (!ctx) ctx = lives_widget_context_default();
  for (; list; list = list->next) {
    if (((lives_thread_data_t *)list->data)->ctx == ctx) return list->data;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars(void) {
  static lives_threadvars_t *dummyvars = NULL;
  lives_thread_data_t *thrdat = get_thread_data();
  if (!thrdat) {
    if (!dummyvars) dummyvars = lives_calloc(1, sizeof(lives_threadvars_t));
    return dummyvars;
  }
  return &thrdat->vars;
}

static lives_thread_data_t *get_thread_data_by_id(uint64_t idx) {
  LiVESList *list = allctxs;
  for (; list; list = list->next) {
    if (((lives_thread_data_t *)list->data)->idx == idx) return list->data;
  }
  return NULL;
}

lives_thread_data_t *lives_thread_data_create(uint64_t idx) {
  lives_thread_data_t *tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));
  if (idx != 0) tdata->ctx = lives_widget_context_new();
  else tdata->ctx = lives_widget_context_default();
  tdata->idx = idx;
  tdata->vars.var_rowstride_alignment = ALIGN_DEF;
  allctxs = lives_list_prepend(allctxs, (livespointer)tdata);
  return tdata;
}


static boolean gsrc_wrapper(livespointer data) {
  thrd_work_t *mywork = (thrd_work_t *)data;
  (*mywork->func)(mywork->arg);
  return FALSE;
}


boolean do_something_useful(lives_thread_data_t *tdata) {
  /// yes, why don't you lend a hand instead of just lying around nanosleeping...
  LiVESList *list;
  thrd_work_t *mywork;
  uint64_t myflags = 0;

  if (!tdata->idx) abort();

  pthread_mutex_lock(&twork_mutex);
  list = twork_last;
  if (LIVES_UNLIKELY(!list)) {
    pthread_mutex_unlock(&twork_mutex);
    return FALSE;
  }

  if (twork_first == list) twork_last = twork_first = NULL;
  else {
    twork_last = list->prev;
    twork_last->next = NULL;
  }
  pthread_mutex_unlock(&twork_mutex);

  mywork = (thrd_work_t *)list->data;
  mywork->busy = tdata->idx;
  myflags = mywork->flags;

  if (myflags & LIVES_THRDFLAG_WAIT_SYNC) {
    lives_nanosleep_until_nonzero(mywork->sync_ready);
  }

  lives_widget_context_invoke(tdata->ctx, gsrc_wrapper, mywork);

  if (myflags & LIVES_THRDFLAG_AUTODELETE) {
    lives_free(mywork); lives_free(list);
  } else mywork->done = tdata->idx;

  pthread_mutex_lock(&twork_count_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_count_mutex);
  return TRUE;
}


static void *thrdpool(void *arg) {
  boolean skip_wait = FALSE;
  lives_thread_data_t *tdata = (lives_thread_data_t *)arg;
  if (!rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_initialize();
  }

  lives_widget_context_push_thread_default(tdata->ctx);

  while (!threads_die) {
    if (!skip_wait) {
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_wait(&tcond, &tcond_mutex);
      pthread_mutex_unlock(&tcond_mutex);
    }
    if (LIVES_UNLIKELY(threads_die)) break;
    skip_wait = do_something_useful(tdata);
    if (rpmalloc_is_thread_initialized()) {
      rpmalloc_thread_collect();
    }
  }
  if (rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_finalize();
  }
  return NULL;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new(int subtype) {
  weed_plant_t *plant = weed_plant_new(WEED_PLANT_LIVES);
  weed_set_int_value(plant, WEED_LEAF_LIVES_SUBTYPE, subtype);
  return plant;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new_with_index(int subtype, int64_t index) {
  weed_plant_t *plant = lives_plant_new(subtype);
  weed_set_int64_value(plant, WEED_LEAF_INDEX, index);
  return plant;
}


void lives_threadpool_init(void) {
  npoolthreads = MINPOOLTHREADS;
  if (prefs->nfx_threads > npoolthreads) npoolthreads = prefs->nfx_threads;
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_first = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    lives_thread_data_t *tdata = lives_thread_data_create(i + 1);
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, thrdpool, tdata);
  }
}


void lives_threadpool_finish(void) {
  threads_die = TRUE;
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_broadcast(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  for (int i = 0; i < npoolthreads; i++) {
    lives_thread_data_t *tdata = get_thread_data_by_id(i + 1);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    pthread_join(*(poolthrds[i]), NULL);
    lives_widget_context_unref(tdata->ctx);
    lives_free(tdata);
    lives_free(poolthrds[i]);
  }
  lives_free(poolthrds);
  poolthrds = NULL;
  npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_first);
  twork_first = twork_last = NULL;
  ntasks = 0;
}


int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr, lives_funcptr_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  if (!thread) list = (LiVESList *)lives_calloc(1, sizeof(LiVESList));
  else list->next = list->prev = NULL;
  list->data = work;
  work->func = func;
  work->arg = arg;

  if (!thread || (attr & LIVES_THRDATTR_AUTODELETE))
    work->flags |= LIVES_THRDFLAG_AUTODELETE;
  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    work->flags |= LIVES_THRDFLAG_WAIT_SYNC;
    work->sync_ready = FALSE;
  }

  pthread_mutex_lock(&twork_mutex);
  if (twork_first == NULL) {
    twork_first = twork_last = list;
  } else {
    if (!(attr & LIVES_THRDATTR_PRIORITY)) {
      twork_first->prev = list;
      list->next = twork_first;
      twork_first = list;
    } else {
      twork_last->next = list;
      list->prev = twork_last;
      twork_last = list;
    }
  }
  pthread_mutex_unlock(&twork_mutex);
  pthread_mutex_lock(&twork_count_mutex);
  ntasks++;
  pthread_mutex_unlock(&twork_count_mutex);
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  if (ntasks >= npoolthreads) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    poolthrds = (pthread_t **)lives_realloc(poolthrds, (npoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
    for (int i = npoolthreads; i < npoolthreads + MINPOOLTHREADS; i++) {
      lives_thread_data_t *tdata = lives_thread_data_create(i + 1);
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, thrdpool, tdata);
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    npoolthreads += MINPOOLTHREADS;
  }
  return 0;
}


uint64_t lives_thread_join(lives_thread_t work, void **retval) {
  thrd_work_t *task = (thrd_work_t *)work.data;
  uint64_t nthrd = 0;
  if (task->flags & LIVES_THRDFLAG_AUTODELETE) {
    LIVES_FATAL("lives_thread_join() called on an autodelete thread");
    return 0;
  }

  while (!task->busy) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    if (task->busy) break;
    sched_yield();
    lives_nanosleep(1000);
  }

  if (!task->done) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
  }

  lives_nanosleep_until_nonzero(task->done);
  nthrd = task->done;

  if (retval) *retval = task->ret;
  lives_free(task);
  return nthrd;
}


LIVES_GLOBAL_INLINE pid_t lives_getpid(void) {
#ifdef IS_MINGW
  return GetCurrentProcessId(),
#else
  return getpid();
#endif
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
  short pb_quality = prefs->pb_quality;
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

  //g_print("vals2x %d %d %d %d\n", mainw->effort, badthingcount, goodthingcount, struggling);

  if (!badthingcount) {
    /// no badthings, good
    if (goodthingcount > EFFORT_RANGE_MAX) goodthingcount = EFFORT_RANGE_MAX;
    if (--mainw->effort < -EFFORT_RANGE_MAX) mainw->effort = -EFFORT_RANGE_MAX;
  } else {
    if (badthingcount > EFFORT_RANGE_MAX) badthingcount = EFFORT_RANGE_MAX;
    mainw->effort = badthingcount;
  }
  //g_print("vals2 %d %d %d %d\n", mainw->effort, badthingcount, goodthingcount, struggling);

  if (mainw->effort < 0) {
    if (struggling > -EFFORT_RANGE_MAX) {
      struggling--;
    }
    if (mainw->effort < -EFFORT_LIMIT_MED) {
      if (struggling == -EFFORT_RANGE_MAX && pb_quality < PB_QUALITY_HIGH) {
        pb_quality++;
      } else if (struggling < -EFFORT_LIMIT_MED && pb_quality < PB_QUALITY_MED) {
        pb_quality++;
      }
    }
  }

  if (mainw->effort > 0) {
    if (pb_quality > future_prefs->pb_quality) {
      pb_quality = future_prefs->pb_quality;
      goto tryset;
    }
    if (!struggling) {
      struggling = 1;
      return;
    }
    if (mainw->effort > EFFORT_LIMIT_MED || (struggling > 0 && (mainw->effort > EFFORT_LIMIT_LOW))) {
      if (struggling < EFFORT_RANGE_MAX) struggling++;
      if (struggling == EFFORT_RANGE_MAX) {
        if (pb_quality > PB_QUALITY_LOW) {
          pb_quality = PB_QUALITY_LOW;
        } else if (mainw->effort > EFFORT_LIMIT_MED) {
          if (pb_quality > PB_QUALITY_MED) {
            pb_quality--;
          }
        }
      } else {
        if (pb_quality > future_prefs->pb_quality) {
          pb_quality = future_prefs->pb_quality;
        } else if (future_prefs->pb_quality > PB_QUALITY_LOW) {
          pb_quality = future_prefs->pb_quality - 1;
        }
	// *INDENT-OFF*
      }}}
  // *INDENT-ON
 tryset:
  if (pb_quality != prefs->pb_quality && (!mainw->frame_layer_preload || mainw->pred_frame == -1
					  || is_layer_ready(mainw->frame_layer_preload))) {
    prefs->pb_quality = pb_quality;
    mainw->blend_palette = WEED_PALETTE_END;
  }

  //g_print("STRG %d and %d %d\n", struggling, mainw->effort, prefs->pb_quality);
}


char *grep_in_cmd(const char *cmd, int mstart, int npieces, const char *mphrase, int ridx, int rlen) {
  char **lines, **words, **mwords;
  char *match = NULL;
  char buff[65536];
  size_t nlines, mwlen;
  int m, minpieces;

  if (!mphrase || npieces < -1 || !npieces || rlen < 1 || (ridx <= mstart && ridx + rlen > mstart)
      || (npieces > 0 && (ridx + rlen > npieces || mstart >= npieces))) return NULL;

  mwlen = get_token_count(mphrase, ' ');
  if (mstart + mwlen > npieces
      || (ridx + rlen > mstart && ridx < mstart + mwlen)) return NULL;

  mwords = lives_strsplit(mphrase, " ", mwlen);

  if (!cmd || !mphrase || !*cmd || !*mphrase) goto grpcln;
  THREADVAR(com_failed) = FALSE;
  lives_popen(cmd, FALSE, buff, 65536);
  if (THREADVAR(com_failed)
      || (!*buff || !(nlines = get_token_count(buff, '\n')))) {
    THREADVAR(com_failed) = FALSE;
    goto grpcln;
  }

  minpieces = MAX(mstart + mwlen, ridx + rlen);

  lines = lives_strsplit(buff, "\n", nlines);
  for (int l = 0; l < nlines; l++) {
    if (*lines[l] && get_token_count(lines[l], ' ') >= minpieces) {
      words = lives_strsplit(lines[l], " ", npieces);
      for (m = 0; m < mwlen; m++) {
	if (lives_strcmp(words[m + mstart], mwords[m])) break;
      }
      if (m == mwlen) {
	match = lives_strdup(words[ridx]);
	for (int w = 1; w < rlen; w++) {
	  char *tmp = lives_strdup_printf(" %s", words[ridx + w]);
	  match = lives_concat(match, tmp);
	}
      }
      lives_strfreev(words);
    }
    if (match) break;
  }
  lives_strfreev(lines);
 grpcln:
  lives_strfreev(mwords);
  return match;
}

static boolean mini_run(char *cmd) {
  THREADVAR(com_failed) = FALSE;
  if (!cmd) return FALSE;
  lives_system(cmd, TRUE);
  lives_free(cmd);
  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  }
  return TRUE;
}


LiVESResponseType send_to_trash(const char *item) {
  LiVESResponseType resp = LIVES_RESPONSE_NONE;
  boolean retval = TRUE;
  char *reason = NULL;
  do {
    resp = LIVES_RESPONSE_NONE;
    if (!check_for_executable(&capable->has_gio, EXEC_GIO)) {
      reason = lives_strdup_printf(_("%s was not found\n"), EXEC_GIO);
      retval = FALSE;
    }
    else {
      char *com = lives_strdup_printf("%s trash \"%s\"", EXEC_GIO, item);
      THREADVAR(com_failed) = FALSE;
      retval = mini_run(com);
      lives_free(com);
    }
    if (!retval) {
      char *msg = lives_strdup_printf(_("LiVES was unable to send the item to trash.\n%s"), reason ? reason : "");
      lives_freep((void **)&reason);
      resp = do_abort_cancel_retry_dialog(msg);
      lives_free(msg);
      if (resp == LIVES_RESPONSE_CANCEL) return resp;
    }
  } while (resp == LIVES_RESPONSE_RETRY);
  return LIVES_RESPONSE_OK;
}


/// x11 stuff

char *get_wid_for_name(const char *wname) {
#ifndef GDK_WINDOWING_X11
  return NULL;
#else
  char *wid = NULL, *cmd;
  THREADVAR(com_failed) = FALSE;
  if (check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL)) {
    cmd = lives_strdup_printf("%s -l", EXEC_WMCTRL);
    wid = grep_in_cmd(cmd, 3, 4, wname, 0, 1);
    lives_free(cmd);
    if (wid) return wid;
  }
  if (check_for_executable(&capable->has_xwininfo, EXEC_XWININFO)) {
    cmd = lives_strdup_printf("%s -name \"%s\" 2>/dev/null", EXEC_XWININFO, wname);
    wid = grep_in_cmd(cmd, 1, -1, "Window id:", 3, 1);
    lives_free(cmd);
    if (wid) return wid;
  }
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    char buff[65536];
    size_t nlines;
    // returns a list, and we need to check each one
    cmd = lives_strdup_printf("%s search \"%s\"", EXEC_XDOTOOL, wname);
    THREADVAR(com_failed) = FALSE;
    lives_popen(cmd, FALSE, buff, 65536);
    lives_free(cmd);
    if (THREADVAR(com_failed)
	|| (!*buff || !(nlines = get_token_count(buff, '\n')))) {
      if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    }
    else {
      char buff2[1024];
      char **lines = lives_strsplit(buff, "\n", nlines);
      for (int l = 0; l < nlines; l++) {
	if (!*lines[l]) continue;
	cmd = lives_strdup_printf("%s getwindowname %s", EXEC_XDOTOOL, lines[l]);
	THREADVAR(com_failed) = FALSE;
	lives_popen(cmd, FALSE, buff2, 1024);
	lives_free(cmd);
	if (THREADVAR(com_failed)) {
	  THREADVAR(com_failed) = FALSE;
	  break;
	}
	lives_chomp(buff2);
	if (!lives_strcmp(wname, buff2)) {
	  wid = lives_strdup_printf("0x%lX", atol(lines[l]));
	  break;
	}
      }
      lives_strfreev(lines);
    }
  }
  return wid;
#endif
}


boolean hide_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return NULL;
#endif
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
    cmd = lives_strdup_printf("%s windowminimize \"%s\"", EXEC_XDOTOOL, wid);
  return mini_run(cmd);
}


boolean unhide_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
    cmd = lives_strdup_printf("%s windowmap \"%s\"", EXEC_XDOTOOL, wid);
  return mini_run(cmd);
}

boolean activate_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
    cmd = lives_strdup_printf("%s windowactivate \"%s\"", EXEC_XDOTOOL, wid);
  else if (check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL))
    cmd = lives_strdup_printf("%s -Fa \"%s\"", EXEC_WMCTRL, wid);
  return mini_run(cmd);
}

boolean show_desktop_panel(void) {
  boolean ret = FALSE;
#ifdef GDK_WINDOWING_X11
  char *wid = get_wid_for_name("xfce4-panel");
  if (wid) {
    ret = unhide_x11_window(wid);
    lives_free(wid);
  }
#endif
  return ret;
}

boolean hide_desktop_panel(void) {
  boolean ret = FALSE;
#ifdef GDK_WINDOWING_X11
  char *wid = get_wid_for_name("xfce4-panel");
  if (wid) {
    ret = hide_x11_window(wid);
    lives_free(wid);
  }
#endif
  return ret;
}


boolean get_x11_visible(const char *wname) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (0 && check_for_executable(&capable->has_xwininfo, EXEC_XWININFO)) {
    char *state;
    cmd = lives_strdup_printf("%s -name \"%s\"", EXEC_XWININFO, wname);
    state = grep_in_cmd(cmd, 2, -1, "Map State:", 4, 1);
    lives_free(cmd);
    if (state && !strcmp(state, "IsViewable")) {
      lives_free(state);
      return TRUE;
    }
  }
  if (wname && check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    char buff[65536];
    size_t nlines;

    // returns a list, and we need to check each one
    cmd = lives_strdup_printf("%s search --all --onlyvisible \"%s\" 2>/dev/null", EXEC_XDOTOOL, wname);
    THREADVAR(com_failed) = FALSE;
    lives_popen(cmd, FALSE, buff, 65536);
    lives_free(cmd);
    if (THREADVAR(com_failed)
	|| (!*buff || !(nlines = get_token_count(buff, '\n')))) {
      if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    }
    else {
      char *wid = get_wid_for_name(wname);
      if (wid) {
	int l;
	char **lines = lives_strsplit(buff, "\n", nlines), *xwid;
	for (l = 0; l < nlines; l++) {
	  if (!*lines[l]) continue;
	  xwid = lives_strdup_printf("0x%08lX", atol(lines[l]));
	  if (!strcmp(xwid, wid)) break;
	}
	lives_strfreev(lines);
	lives_free(wid);
	if (l < nlines)  return TRUE;
      }
    }
  }
  return FALSE;
}

#define WM_XFWM4 "Xfwm4"
#define WM_XFCE4_PANEL "xfce4-panel"


void get_wm_caps(const char *wm_name) {
  if (wm_name) {
    if (!strcmp(wm_name, WM_XFWM4)) {
      capable->has_wm_caps = TRUE;
      capable->wm_caps = (wm_caps_t){WM_XFCE4_PANEL};
    }
    else capable->has_wm_caps = FALSE;
  }
}
