
// machinestate.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#ifdef _GNU_SOURCE
#include <sched.h>
#endif

#include <sys/statvfs.h>
#include "main.h"
#include "callbacks.h"

LIVES_LOCAL_INLINE char *mini_popen(char *cmd);

#if IS_X86_64

static void cpuid(unsigned int ax, unsigned int *p) {
  __asm __volatile
  ("movl %%ebx, %%esi\n\tcpuid\n\txchgl %%ebx, %%esi"
   : "=a"(p[0]), "=S"(p[1]), "=c"(p[2]), "=d"(p[3])
   : "0"(ax));
}

static int get_cacheline_size(void) {
  unsigned int cacheline = -1;
  unsigned int regs[4], regs2[4];
  cpuid(0x00000000, regs);
  if (regs[0] >= 0x00000001) {
    cpuid(0x00000001, regs2);
    cacheline = ((regs2[1] >> 8) & 0xFF) * 8;
    //has_sse2 = (regs2[3] & 0x4000000) ? TRUE : FALSE;
  }
  return cacheline;
}

#endif

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

void lives_get_randbytes(void *ptr, size_t size) {
  if (size <= 8) {
    uint64_t rbytes = gen_unique_id();
    lives_memcpy(ptr, &rbytes, size);
  }
}


uint64_t gen_unique_id(void) {
  static uint64_t last_rnum = 0;
  uint64_t rnum;
#if HAVE_GETENTROPY
  int randres = getentropy(&rnum, 8);
#else
  int randres = 1;
#endif
  if (randres) {
    fastrand_val = lives_random();
    fastrand();
    fastrand_val ^= lives_get_current_ticks();
    rnum = fastrand();
  }
  /// if we have a genuine RNG for 64 bits, then the probability of generating
  // a number < 1 billion is approx. 2 ^ 30 / 2 ^ 64 or about 1 chance in 17 trillion
  // the chance of it happening the first time is thus minscule
  // and the chance of it happening twice by chance is so unlikely we should discount it
  if (rnum < BILLIONS(1) && last_rnum < BILLIONS(1)) abort();
  last_rnum = rnum;
  return rnum;
}


void init_random() {
  uint32_t rseed;
#ifdef HAVE_GETENTROPY
  if (getentropy(&rseed, 4)) rseed = (gen_unique_id() & 0xFFFFFFFF);
#else
  rseed = (gen_unique_id() & 0xFFFFFFFF);
#endif
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
      if (!preplist) dccl = preplist = lives_list_append(preplist, dcc);
      else {
        LiVESList *dccl2 = lives_list_append(NULL, (livespointer)dcc);
        for (; dccl; dccl = dccl->next) {
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
          if (dccl->prev) dccl->prev->next = dccl2;
          else preplist = dccl2;
          dccl->prev = dccl2;
        }
      }
      val *= 2;
    }
    val6 *= 6;
  }
  for (dccl = preplist; dccl; dccl = dccl->next) {
    dcc = (struct _decomp *)dccl->data;
    xi = dcc->i;
    xj = dcc->j;
    nxttbl[xi][xj].value = dcc->value;
    nxttbl[xi][xj].i = xi;
    nxttbl[xi][xj].j = xj;
    if (dccl->prev) {
      dcc = (struct _decomp *)dccl->prev->data;
      nxttbl[xi][xj].lower = &(nxttbl[dcc->i][dcc->j]);
    } else nxttbl[xi][xj].lower = NULL;
    if (dccl->next) {
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


char *get_md5sum(const char *filename) {
  /// for future use
  char **array;
  char *md5;
  char *com = lives_strdup_printf("%s \"%s\"", EXEC_MD5SUM, filename);
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


LIVES_GLOBAL_INLINE
char *use_staging_dir_for(int clipno) {
  if (clipno > 0 && IS_VALID_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
    char *clipdir = get_clip_dir(clipno);
    char *stfile = lives_build_filename(clipdir, LIVES_STATUS_FILE_NAME, NULL);
    lives_free(clipdir);
    lives_snprintf(sfile->info_file, PATH_MAX, "%s", stfile);
    lives_free(stfile);
    if (*sfile->staging_dir) {
      return lives_strdup_printf("%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL,
                                 capable->backend_path, sfile->staging_dir, prefs->configfile);
    }
  }
  return lives_strdup(prefs->backend_sync);
}


const char *get_shmdir(void) {
  if (!*capable->shmdir_path) {
    char *xshmdir = NULL, *shmdir = lives_build_path(LIVES_RUN_DIR, NULL);
    if (lives_file_test(shmdir, LIVES_FILE_TEST_IS_DIR)) {
      xshmdir = lives_build_path(LIVES_RUN_DIR, LIVES_SHM_DIR, NULL);
      if (!lives_file_test(xshmdir, LIVES_FILE_TEST_IS_DIR) || !is_writeable_dir(xshmdir)) {
        lives_free(xshmdir);
        if (!is_writeable_dir(shmdir)) {
          lives_free(shmdir);
          shmdir = lives_build_path(LIVES_DEVICE_DIR, LIVES_SHM_DIR, NULL);
          if (!lives_file_test(shmdir, LIVES_FILE_TEST_IS_DIR)  || !is_writeable_dir(shmdir)) {
            lives_free(shmdir);
            shmdir = lives_build_path(LIVES_TMP_DIR, NULL);
            if (!lives_file_test(shmdir, LIVES_FILE_TEST_IS_DIR) || !is_writeable_dir(shmdir)) {
              lives_free(shmdir);
              capable->writeable_shmdir = MISSING;
              return NULL;
	      // *INDENT-OFF*
	    }}}}
      // *INDENT-ON*
      else {
        lives_free(shmdir);
        shmdir = xshmdir;
      }
    }
    capable->writeable_shmdir = PRESENT;
    xshmdir = lives_build_path(shmdir, LIVES_DEF_WORK_SUBDIR, NULL);
    lives_free(shmdir);
    lives_snprintf(capable->shmdir_path, PATH_MAX, "%s", xshmdir);
    lives_free(xshmdir);
  }
  if (capable->writeable_shmdir) return capable->shmdir_path;
  return NULL;
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


lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, int64_t *dsval, int64_t ds_resvd) {
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)
  // call with dsval set to ds_used to check for OVER_QUOTA
  // dsval is overwritten and set to ds_free
  int64_t ds;
  lives_storage_status_t status = LIVES_STORAGE_STATUS_UNKNOWN;
  if (dsval && prefs->disk_quota > 0 && *dsval > (int64_t)((double)prefs->disk_quota * prefs->quota_limit / 100.))
    status = LIVES_STORAGE_STATUS_OVER_QUOTA;
  if (!is_writeable_dir(dir)) return status;
  ds = (int64_t)get_ds_free(dir);
  ds -= ds_resvd;
  if (dsval) *dsval = ds;
  if (ds <= 0) return LIVES_STORAGE_STATUS_OVERFLOW;
  if (ds < prefs->ds_crit_level) return LIVES_STORAGE_STATUS_CRITICAL;
  if (status != LIVES_STORAGE_STATUS_UNKNOWN) return status;
  if (ds < warn_level) return LIVES_STORAGE_STATUS_WARNING;
  return LIVES_STORAGE_STATUS_NORMAL;
}

static lives_proc_thread_t running = NULL;
static char *running_for = NULL;

boolean disk_monitor_running(const char *dir)
{return (running && (!dir || !lives_strcmp(dir, running_for)));}

lives_proc_thread_t disk_monitor_start(const char *dir) {
  if (disk_monitor_running(dir)) disk_monitor_forget();
  running = lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                     (lives_funcptr_t)get_dir_size, WEED_SEED_INT64, "s", dir);
  mainw->dsu_valid = TRUE;
  running_for = lives_strdup(dir);
  return running;
}

int64_t disk_monitor_check_result(const char *dir) {
  // caller MUST check if mainw->ds_valid is TRUE, or recheck the results
  int64_t bytes;
  if (!disk_monitor_running(dir)) disk_monitor_start(dir);
  if (!lives_strcmp(dir, running_for)) {
    if (!lives_proc_thread_check_finished(running)) {
      return -1;
    }
    bytes = lives_proc_thread_join_int64(running);
    lives_proc_thread_free(running);
    running = NULL;
  } else bytes = (int64_t)get_dir_size(dir);
  return bytes;
}


LIVES_GLOBAL_INLINE int64_t disk_monitor_wait_result(const char *dir, ticks_t timeout) {
  // caller MUST check if mainw->ds_valid is TRUE, or recheck the results
  lives_alarm_t alarm_handle = LIVES_NO_ALARM;
  int64_t dsval;

  if (*running_for && !lives_strcmp(dir, running_for)) {
    if (timeout) return -1;
    return get_dir_size(dir);
  }

  if (timeout < 0) timeout = LIVES_LONGEST_TIMEOUT;
  if (timeout > 0) alarm_handle = lives_alarm_set(timeout);

  while ((dsval = disk_monitor_check_result(dir)) < 0
         && (alarm_handle == LIVES_NO_ALARM || ((timeout = lives_alarm_check(alarm_handle)) > 0))) {
    lives_nanosleep(1000);
  }
  if (alarm_handle != LIVES_NO_ALARM) {
    lives_alarm_clear(alarm_handle);
    if (!timeout) {
      disk_monitor_forget();
      return -1;
    }
  }
  return dsval;
}

void disk_monitor_forget(void) {
  if (!disk_monitor_running(NULL)) return;
  lives_proc_thread_dontcare(running);
  running = NULL;
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
  if (!lives_strcmp(dir, prefs->workdir)) {
    capable->ds_free = bytes;
    capable->ds_tot = sbuf.f_bsize * sbuf.f_blocks;
  }

getfserr:
  if (must_delete) lives_rmdir(dir, FALSE);

  return bytes;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(ticks_t origsecs, ticks_t orignsecs) {
  ticks_t ret = -1;
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ret = ((ts.tv_sec * ONE_BILLION + ts.tv_nsec)
         - (origsecs * ONE_BILLION + orignsecs)) / TICKS_TO_NANOSEC;
#else
#ifdef USE_MONOTONIC_TIME
  ret = (lives_get_monotonic_time() - orignsecs) / 10;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  ret = ((tv.tv_sec * ONE_MILLLION + tv.tv_usec)
         - (origsecs * ONE_MILLION + orignsecs / 1000)) * USEC_TO_TICKS;
#endif
#endif
  mainw->wall_ticks = ret;
  if (ret >= 0)
    mainw->wall_ticks += (origsecs * ONE_BILLION + orignsecs) / TICKS_TO_NANOSEC;
  return ret;
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
  today = lives_datetime(otv.tv_sec, TRUE);
  yesterday = lives_datetime(otv.tv_sec - SECS_IN_DAY, TRUE);
  if (!lives_strncmp(datetime, today, 10)) dtxt = lives_strdup_printf(_("Today %s"), datetime + 11);
  else if (!lives_strncmp(datetime, yesterday, 10))
    dtxt = lives_strdup_printf(_("Yesterday %s"), datetime + 11);
  else dtxt = (char *)datetime;
  if (today) lives_free(today);
  if (yesterday) lives_free(yesterday);
  return dtxt;
}


char *lives_datetime(uint64_t secs, boolean use_local) {
  char buf[128];
  char *datetime = NULL;
  struct tm *gm = use_local ? localtime((time_t *)&secs) : gmtime((time_t *)&secs);
  ssize_t written;

  if (gm) {
    written = (ssize_t)strftime(buf, 128, "%Y-%m-%d    %H:%M:%S", gm);
    if ((written > 0) && ((size_t)written < 128)) {
      datetime = lives_strdup(buf);
    }
  }
  return datetime;
}


LIVES_GLOBAL_INLINE char *get_current_timestamp(void) {
  struct timeval otv;
  gettimeofday(&otv, NULL);
  return lives_datetime(otv.tv_sec, TRUE);
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


off_t get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  off_t fsize;
  lives_file_buffer_t *fbuff;
  fstat(fd, &filestat);
  fsize = filestat.st_size;
  //g_printerr("fssize for %d is %ld\n", fd, fsize);
  if ((fbuff = find_in_file_buffers(fd)) != NULL) {
    if (!fbuff->read) {
      /// because of padding bytes... !!!!
      off_t f2size;
      if ((f2size = (off_t)(fbuff->offset + fbuff->bytes)) > fsize) return f2size;
    }
  }
  return fsize;
}


off_t sget_file_size(const char *name) {
  off_t res;
  struct stat xstat;
  if (!name) return 0;
  res = stat(name, &xstat);
  if (res < 0) return res;
  return xstat.st_size;
}


// check with list like subdir1, subdir2|subsubdir2,.. for subdirs in dirname. Returns list wuth matched subdirs removed.
LiVESList *check_for_subdirs(const char *dirname, LiVESList * subdirs) {
  DIR *tldir, *xsubdir;
  struct dirent *tdirent, *xtdirent;
  LiVESList *xlist, *nlist;
  char *xdirname, *xxdirname;
  int part;
  if (!dirname) return subdirs;
  tldir = opendir(dirname);
  if (!tldir) return subdirs;
  while (subdirs && (tdirent = readdir(tldir))) {
    if (tdirent->d_name[0] == '.'
        && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
    for (xlist = subdirs; xlist; xlist = nlist) {
      int nparts = get_token_count((const char *)xlist->data, '|');
      char **array = lives_strsplit((const char *)xlist->data, "|", nparts);
      xtdirent = tdirent;
      xdirname = (char *)dirname;
      nlist = xlist->next;
      for (part = 0; part < nparts; part++) {
        if (part == 0) {
          if (lives_strcmp(array[0], xtdirent->d_name)) {
            lives_strfreev(array);
            break;
          }
          continue;
        }

        // get next level of subdirs
        xxdirname = lives_build_path(xdirname, xtdirent->d_name, NULL);
        if (xdirname != dirname) lives_free(xdirname);
        xdirname = xxdirname;
        xsubdir = opendir(xdirname);
        if (!xsubdir) break;
        while ((xtdirent = readdir(xsubdir))) {
          if (!lives_strcmp(array[part], xtdirent->d_name)) break;
        }

        closedir(xsubdir);
        if (xtdirent) continue;
        lives_strfreev(array);
        if (xdirname != dirname) lives_free(xdirname);
        break; // no match for array[part]
      }
      if (part == nparts) {
        // matched, we can remove from the list
        subdirs = lives_list_remove_node(subdirs, xlist, TRUE);
      }
    }
  }
  closedir(tldir);
  return subdirs;
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


LIVES_GLOBAL_INLINE char *get_symlink_for(const char *link) {
  char buff[PATH_MAX];
  ssize_t nbytes = readlink(link, buff, PATH_MAX);
  if (nbytes < 0) return lives_strdup(link);
  return lives_strndup(buff, nbytes);
}


char *get_mountpoint_for(const char *dirx) {
  char *mp = NULL, *tmp, *com, *res, *dir;
  size_t lmatch = 0, slen;
  int j;

  if (!dirx) return NULL;
  dir = get_symlink_for(dirx);
  slen = lives_strlen(dir);

  com = lives_strdup("df -P");
  if ((res = mini_popen(com))) {
    int lcount = get_token_count(res, '\n');
    char **array0 = lives_strsplit(res, "\n", lcount);
    for (int l = 0; l < lcount; l++) {
      int pccount = get_token_count(array0[l], ' ');
      char **array1 = lives_strsplit(array0[l], " ", pccount);
      lives_chomp(array1[pccount - 1], FALSE);
      for (j = 0; array1[pccount - 1][j] && j < slen; j++) if (array1[pccount - 1][j] != dir[j]) break;
      if (j > lmatch && !array1[pccount - 1][j]) {
        lmatch = j;
        if (mp) lives_free(mp);
        tmp = lives_strdup(array1[0]);
        mp = lives_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
        lives_free(tmp);
      }
      lives_strfreev(array1);
    }
    lives_strfreev(array0);
    lives_free(res);
  }
  lives_free(dir);
  return mp;
}


char *get_fstype_for(const char *volx) {
  char *fstype = NULL;
  if (volx) {
    char *vol = get_symlink_for(volx);
    char *res, *com = lives_strdup("df --output=fstype,target");
    if ((res = mini_popen(com))) {
      int lcount = get_token_count(res, '\n');
      char **array0 = lives_strsplit(res, "\n", lcount);
      for (int l = 0; l < lcount; l++) {
        int pccount = get_token_count(array0[l], ' ');
        char **array1 = lives_strsplit(array0[l], " ", pccount);
        if (!lives_strcmp(array1[pccount - 1], vol)) fstype = lives_strdup(array1[0]);
        lives_strfreev(array1);
        if (fstype) break;
      }
      lives_strfreev(array0);
      lives_free(res);
    }
    lives_free(vol);
  }
  return fstype;
}


#ifdef IS_FREEBSD
#define DU_BLOCKSIZE 1024
#else
#define DU_BLOCKSIZE 1
#endif

off_t get_dir_size(const char *dirname) {
  off_t dirsize = -1;
  if (!dirname || !*dirname || !lives_file_test(dirname, LIVES_FILE_TEST_IS_DIR)) return -1;
  if (check_for_executable(&capable->has_du, EXEC_DU)) {
    char buff[PATH_MAX * 2];
    char *com = lives_strdup_printf("%s -sB %d \"%s\"", EXEC_DU, DU_BLOCKSIZE, dirname);
    lives_popen(com, TRUE, buff, PATH_MAX * 2);
    lives_free(com);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    else dirsize = atol(buff) / DU_BLOCKSIZE;
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
    if (prefs->show_dev_opts) {
      char *msg = lives_strdup_printf("\nstat failed for file %s\n", fname);
      perror(msg);
      lives_free(msg);
    }
    fdets->size = -2;
    fdets->type = LIVES_FILE_TYPE_UNKNOWN;
    return ret;
  }
  fdets->type = (uint64_t)((filestat.st_mode & S_IFMT) >> 12);
  fdets->size = filestat.st_size;
  fdets->mode = (uint64_t)(filestat.st_mode & 0x0FFF);
  fdets->uid = filestat.st_uid;
  fdets->gid = filestat.st_gid;
  fdets->blk_size = (uint64_t)filestat.st_blksize;
  fdets->atime_sec = filestat.st_atim.tv_sec;
  fdets->atime_nsec = filestat.st_atim.tv_nsec;
  fdets->mtime_sec = filestat.st_mtim.tv_sec;
  fdets->mtime_nsec = filestat.st_mtim.tv_nsec;
  fdets->ctime_sec = filestat.st_ctim.tv_sec;
  fdets->ctime_nsec = filestat.st_ctim.tv_nsec;
  return ret;
}


static char *file_to_file_details(const char *filename, lives_file_dets_t *fdets,
                                  lives_proc_thread_t tinfo, uint64_t extra) {
  char *tmp, *tmp2;
  char *extra_details = lives_strdup("");

  if (!stat_to_file_dets(filename, fdets)) {
    // if stat fails, we have set set size to -2, type to LIVES_FILE_TYPE_UNKNOWN
    // and here we set extra_details to ""
    if (tinfo && lives_proc_thread_get_cancelled(tinfo)) {
      lives_free(extra_details);
      return NULL;
    }
    if (LIVES_FILE_IS_DIRECTORY(fdets->type)) {
      boolean emptyd = FALSE;
      if (extra & EXTRA_DETAILS_EMPTY_DIRS) {
        if ((emptyd = is_empty_dir(filename))) {
          fdets->type |= LIVES_FILE_TYPE_FLAG_EMPTY;
          tmp2 = lives_strdup_printf("%s%s%s", extra_details, *extra_details ? ", " : "",
                                     (tmp = _("(empty)")));
          lives_free(tmp);
          lives_free(extra_details);
          extra_details = tmp2;
        }
        if (tinfo && lives_proc_thread_get_cancelled(tinfo)) {
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
	}}}
    if (extra & EXTRA_DETAILS_MD5SUM) {
      fdets->md5sum = get_md5sum(filename);
    }
    if (extra & EXTRA_DETAILS_SYMLINK) {
      if (lives_file_test(filename, LIVES_FILE_TEST_IS_SYMLINK))
	fdets->type |= LIVES_FILE_TYPE_FLAG_SYMLINK;
    }
    if (extra & EXTRA_DETAILS_EXECUTABLE) {
      if (lives_file_test(filename, LIVES_FILE_TEST_IS_EXECUTABLE))
	fdets->type |= LIVES_FILE_TYPE_FLAG_EXECUTABLE;
    }
    /// TODO
    /* if (extra & EXTRA_DETAILS_WRITEABLE) { */
    /*   if (LIVES_FILE_TEST_IS_EXECUTABLE(filename)) fdets->type |= LIVES_FILE_TYPE_FLAG_EXECUTABLE; */
    /* } */
    /* if (extra & EXTRA_DETAILS_ACCESSIBLE) { */
    /*   if (LIVES_FILE_TEST_IS_EXECUTABLE(filename)) fdets->type |= LIVES_FILE_TYPE_FLAG_EXECUTABLE; */
    /* } */
  }
  // *INDENT-ON*
  else {
    /// stat failed
    if (extra & EXTRA_DETAILS_CHECK_MISSING) {
      if (!lives_file_test(filename, LIVES_FILE_TEST_EXISTS)) {
        fdets->type |= LIVES_FILE_TYPE_FLAG_MISSING;
        tmp2 = lives_strdup_printf("%s%s%s", extra_details, *extra_details ? ", " : "",
                                   (tmp = _("(ABSENT)")));
        lives_free(tmp);
        lives_free(extra_details);
        extra_details = tmp2;
      }
    }
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
      if (lives_proc_thread_get_cancelled(tinfo) || !tdirent) {
        closedir(tldir);
        if (lives_proc_thread_get_cancelled(tinfo)) return NULL;
        break;
      }
      if (tdirent->d_name[0] == '.'
          && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
      fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);
      fdets->name = lives_strdup(tdirent->d_name);
      //g_print("GOT %s\n", fdets->name);
      fdets->size = -1;
      *listp = lives_list_append(*listp, fdets);
      if (lives_proc_thread_get_cancelled(tinfo)) {
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
    while (1) {
      if (lives_proc_thread_get_cancelled(tinfo) || !orderfile) {
        if (orderfile) {
          fclose(orderfile);
        }
        return NULL;
      }
      if (!lives_fgets(buff, PATH_MAX, orderfile)) {
        fclose(orderfile);
        break;
      }
      lives_chomp(buff, FALSE);

      fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);

      fdets->name = lives_strdup(buff);
      fdets->size = -1;
      *listp = lives_list_append(*listp, fdets);
      if (lives_proc_thread_get_cancelled(tinfo)) {
        fclose(orderfile);
        return NULL;
      }
    }
    break;
  }
  default: return NULL;
  }

  if (*listp) empty = FALSE;
  *listp = lives_list_append(*listp, NULL);

  if (empty || lives_proc_thread_get_cancelled(tinfo)) return NULL;

  // listing done, now get details for each entry
  list = *listp;
  while (list && list->data) {
    if (lives_proc_thread_get_cancelled(tinfo)) return NULL;

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

    if (tinfo && lives_proc_thread_get_cancelled(tinfo)) {
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
  return lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                  (lives_funcptr_t)_item_to_file_details, -1, "vssIi",
                                  listp, dir, orig_loc, extra, 0);
}


lives_proc_thread_t ordfile_to_file_details(LiVESList **listp, const char *ofname,
    const char *orig_loc, uint64_t extra) {
  return lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                  (lives_funcptr_t)_item_to_file_details, -1, "vssIi",
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
      _("Your version of mplayer/ffmpeg may be broken !\n"
        "See http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    widget_opts.non_modal = FALSE;
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}


LIVES_GLOBAL_INLINE char *lives_concat_sep(char *st, const char *sep, char *x) {
  /// nb: lives strconcat
  // uses realloc / memcpy, frees x; st becomes invalid
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
  // uses realloc / memcpy, frees x; st becomes invalid
  size_t s1 = lives_strlen(st), s2 = lives_strlen(x);
  char *tmp = (char *)lives_realloc(st, ++s2 + s1);
  lives_memcpy(tmp + s1, x, s2);
  lives_free(x);
  return tmp;
}


LIVES_GLOBAL_INLINE char *lives_strcollate(char **strng, const char *sep, const char *xnew) {
  // appends xnew to *string, if strlen(*string) > 0 appends sep first
  //
  char *tmp = (strng && *strng && **strng) ? *strng : lives_strdup("");
  char *strng2 = lives_strdup_printf("%s%s%s", tmp, (sep && *tmp) ? sep : "", xnew);
  lives_freep((void **)strng);
  return strng2;
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
  if (st && term) for (int i = 0; st[i]; i++) if (st[i] == term) {st[i] = 0; break;}
  return st;
}


LIVES_GLOBAL_INLINE char *lives_chomp(char *buff, boolean multi) {
  /// chop off final newline
  /// see also lives_strchomp() which removes all whitespace
  if (buff) {
    size_t xs = lives_strlen(buff);
    do {
      if (xs && buff[xs - 1] == '\n') buff[--xs] = '\0'; // remove trailing newline
      else break;
    } while (multi);
  }
  return buff;
}


LIVES_GLOBAL_INLINE char *lives_strtrim(const char *buff) {
  /// return string with start and end newlines stripped
  /// see also lives_strstrip() which removes all whitespace
  int i, j;
  if (!buff) return NULL;
  for (i = 0; buff[i] == '\n'; i++);
  for (j = i; buff[j]; j++) if (buff[j] == '\n') break;
  return lives_strndup(buff + i, j - i);
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


LIVES_GLOBAL_INLINE pid_t lives_getpid(void) {
#ifdef IS_MINGW
  return GetCurrentProcessId(),
#else
  return getpid();
#endif
}

LIVES_GLOBAL_INLINE int lives_getuid(void) {return geteuid();}

LIVES_GLOBAL_INLINE int lives_getgid(void) {return getegid();}

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
  if ((mainw->is_rendering || (mainw->multitrack
                               && mainw->multitrack->is_rendering)) && !mainw->preview_rendering)
    mainw->effort = -EFFORT_RANGE_MAX;
  else mainw->effort = 0;
}


void update_effort(int nthings, boolean badthings) {
  int spcycles;
  short pb_quality = prefs->pb_quality;
  if (!inited) reset_effort();
  if (!nthings) return;

  if (nthings > EFFORT_RANGE_MAX) nthings = EFFORT_RANGE_MAX;

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


static char *grep_in_cmd(const char *cmd, int mstart, int npieces, const char *mphrase, int ridx, int rlen, boolean partial) {
  char **lines, **words, **mwords;
  char *match = NULL, *wline;
  char buff[65536];
  size_t nlines, mwlen, mlen = 0;
  int m, minpieces;

  //break_me("GIC");

  if (!mphrase || npieces < -1 || !npieces || rlen < 1 || (ridx <= mstart && ridx + rlen > mstart)
      || (npieces > 0 && (ridx + rlen > npieces || mstart >= npieces))) return NULL;

  mwlen = get_token_count(mphrase, ' ');
  if (mstart + mwlen > npieces
      || (ridx + rlen > mstart && ridx < mstart + mwlen)) return NULL;

  mwords = lives_strsplit(mphrase, " ", mwlen);

  if (!cmd || !mphrase || !*cmd || !*mphrase) goto grpcln;
  lives_popen(cmd, FALSE, buff, 65536);
  if (THREADVAR(com_failed)
      || (!*buff || !(nlines = get_token_count(buff, '\n')))) {
    THREADVAR(com_failed) = FALSE;
    goto grpcln;
  }

  if (partial) mlen = lives_strlen(mwords[mwlen - 1]);

  minpieces = MAX(mstart + mwlen, ridx + rlen);

  lines = lives_strsplit(buff, "\n", nlines);
  for (int l = 0; l < nlines; l++) {
    if (*lines[l]) {
      // reduce multiple spaces to one
      char *tmp = lives_strdup(lines[l]);
      size_t llen = lives_strlen(tmp), tlen;
      while (1) {
	wline = subst(tmp, "  ", " ");
	if ((tlen = lives_strlen(wline)) == llen) break;
	lives_free(tmp);
	tmp = wline;
	llen = tlen;
      }

      if (*wline && get_token_count(wline, ' ') >= minpieces) {
	words = lives_strsplit(wline, " ", npieces);
	for (m = 0; m < mwlen; m++) {
	  if (partial && m == mwlen - 1) {
	    if (lives_strncmp(words[m + mstart], mwords[m], mlen)) break;
	  }
	  else if (lives_strcmp(words[m + mstart], mwords[m])) break;
	}
	if (m == mwlen) {
	  match = lives_strdup(words[ridx]);
	  for (int w = 1; w < rlen; w++) {
	    char *tmp = lives_strdup_printf(" %s", words[ridx + w]);
	    match = lives_concat(match, tmp);
	  }
	}
	else {
	  if (partial) {
	    // check end as well
	    if (!lives_strncmp(mphrase, wline + llen - mlen, mlen)) {
	      match = lives_strdup(words[ridx]);
	      break;
	    }
	  }
	}
	lives_strfreev(words);
      }
      lives_free(wline);
      if (match) break;
    }
  }
  lives_strfreev(lines);
 grpcln:
  lives_strfreev(mwords);
  return match;
}

LIVES_LOCAL_INLINE boolean mini_run(char *cmd) {
  if (!cmd) return FALSE;
  lives_system(cmd, TRUE);
  lives_free(cmd);
  if (THREADVAR(com_failed)) return FALSE;
  return TRUE;
}

LIVES_LOCAL_INLINE char *mini_popen(char *cmd) {
  if (!cmd) return NULL;
  else {
    char buff[PATH_MAX];
    //char *com = lives_strdup_printf("%s $(%s)", capable->echo_cmd, EXEC_MKTEMP);
    lives_popen(cmd, TRUE, buff, PATH_MAX);
    lives_free(cmd);
    lives_chomp(buff, FALSE);
    return lives_strdup(buff);
  }
}


LiVESResponseType send_to_trash(const char *item) {
  LiVESResponseType resp = LIVES_RESPONSE_NONE;
  boolean retval = TRUE;
  char *reason = NULL;
#ifndef IMPL_TRASH
  do {
    resp = LIVES_RESPONSE_NONE;
    if (check_for_executable(&capable->has_gio, EXEC_GIO) != PRESENT) {
      reason = lives_strdup_printf(_("%s was not found\n"), EXEC_GIO);
      retval = FALSE;
    }
    else {
      char *com = lives_strdup_printf("%s trash \"%s\"", EXEC_GIO, item);
      retval = mini_run(com);
    }
#else
    /// TODO *** - files should be moved to
    /// 1) if not $HOME partition, capable->mountpoint/.Trash; also check all toplevels
    /// check for sticky bit and also non symlink. Then create uid subdir
    /// else try to create mountpoint / .Trash-$uid
    /// else (or if in home dir):
    /// capable->xdg_data_home/Trash/

    /// create an entry like info/foo1.trashinfo (O_EXCL)

    /// [Trash Info]
    /// Path=/home/user/livesprojects/foo1
    /// DeletionDate=2020-07-11T14:57:00

    /// then move / copy file or dir to files/foo1
    /// - if already exists, append .2, .3 etc.
    // see: https://specifications.freedesktop.org/trash-spec/trashspec-latest.html
    int vnum = 0;
    char *trashdir;
    char *mp1 = get_mountpount_for(item);
    char *mp2 = get_mountpount_for(capable->home_dir);
    if (!lives_strcmp(mp1, mp2)) {
      char *localshare = get_localsharedir(NULL);
      trashdir = lives_build_path(localshare, "Trash", NULL);
      lives_free(localshare);
      trashinfodir = lives_build_path(trashdir, "info", NULL);
      trashfilesdir = lives_build_path(trashdir, "files", NULL);
      umask = capable->umask;
      capable->umask = 0700;
      if (!check_dir_access(trashinfodir, TRUE)) {
	retval = FALSE;
	reason = lives_strdup_printf(_("Could not write to %s\n"), trashinfodir);
      }
      if (retval) {
	if (!check_dir_access(trashfilesdir, TRUE)) {
	  retval = FALSE;
	  reason = lives_strdup_printf(_("Could not write to %s\n"), trashfilesdir);
	}
      }
      capable->umask = umask;
      if (retval) {
	char *trashinfo;
	int fd;
	while (1) {
	  if (!vnum) trashinfo = lives_strdup_printf("%s.trashinfo", basenm);
	  else trashinfo = lives_strdup_printf("%s.%d.trashinfo", basenm, vnum);
	  fname = lives_build_filename(trashinfodir, trashinfo, NULL);
	  fd = lives_open2(fname, O_CREAT | O_EXCL);
	  if (fd) break;
	  vnum++;
	}
	// TODO - write stuff, close, move item


      }
    }
    /// TODO...
#endif
    if (!retval) {
      char *msg = lives_strdup_printf(_("LiVES was unable to send the item to trash.\n%s"),
				      reason ? reason : "");
      lives_freep((void **)&reason);
      resp = do_abort_retry_cancel_dialog(msg);
      lives_free(msg);
      if (resp == LIVES_RESPONSE_CANCEL) return resp;
    }
  } while (resp == LIVES_RESPONSE_RETRY);
  return LIVES_RESPONSE_OK;
}


/// x11 stuff

#ifdef GDK_WINDOWING_X11
/* Window *find_wm_toplevel(Window xid) { */
/*   Window root, parent, *children; */
/*   unsigned int nchildren; */
/*   if (XQueryTree (gdk_x11_get_default_xdisplay (), xid, &root, */
/*   		  &parent, &children, &nchildren) == 0) { */
/*     // None found */
/*   } */
/* } */


static boolean rec_desk_done(livespointer data) {
  rec_args *recargs = (rec_args *)data;
  // need to do this, as lpt has a notify value; otherwise it would not be cancellable
  if (recargs->lpt) {
    lives_proc_thread_join(recargs->lpt);
    recargs->lpt = NULL;
  }

  if (lives_get_status() != LIVES_STATUS_IDLE) return TRUE;
  else {
    lives_clip_t *sfile;
    /* ticks_t tc; */
    /* int from_files[1]; */
    /* double aseeks[1], avels[1], chvols[1]; */
    char *com;
    int current_file = mainw->current_file;

    if (!IS_VALID_CLIP(recargs->clipno)) goto ohnoes2;
    sfile = mainw->files[recargs->clipno];
    sfile->is_loaded = TRUE;

    migrate_from_staging(recargs->clipno);

    if (sfile->frames <= 0) goto ohnoes;

    do_info_dialogf(_("Grabbed %d frames"), sfile->frames);

    add_to_clipmenu_any(recargs->clipno);
    switch_clip(1, recargs->clipno, FALSE);

    // render the audio track
    /* from_files[0] = mainw->ascrap_file; */
    /* avels[0] = 1.; */
    /* aseeks[0] =01.; */
    /* tc = 10 * TICKS_PER_SECOND_DBL; */
    /* chvols[0] = 1.; */
    /* cfile->achans = 2; */
    /* cfile->asampsize = 16; */
    /* cfile->arate = 48000; */

    /* render_audio_segment(1, from_files, mainw->current_file, avels, aseeks, 0, tc, chvols, 1., 1., NULL); */
    /* reget_afilesize(mainw->current_file); */

    cfile->undo1_dbl = recargs->fps;
    cfile->fps = 0.;

    THREAD_INTENTION = LIVES_INTENTION_RECORD;

    on_resample_vid_ok(NULL, NULL);

    com = lives_strdup_printf("%s clear_tmp_files \"%s\"", prefs->backend, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);
    cfile->end = cfile->frames;

    switch_clip(1, mainw->current_file, TRUE);

    lives_widget_set_sensitive(mainw->desk_rec, TRUE);
    lives_free(recargs);

    return FALSE;

  ohnoes:
    mainw->current_file = recargs->clipno;
    close_current_file(current_file);
    lives_free(recargs);
  ohnoes2:
    do_error_dialog(_("Screen grab failed"));
    lives_widget_set_sensitive(mainw->desk_rec, TRUE);
  }
  return FALSE;
}


void rec_desk(void *args) {
  // experimental
  // TODO - start disk space monitor
  savethread_priv_t *saveargs = NULL;
  lives_thread_t *saver_thread = NULL;
  lives_painter_surface_t *csurf = NULL;
  lives_proc_thread_t lpt = THREADVAR(tinfo);
  rec_args *recargs = (rec_args *)args;
  LiVESWidget *win;
  lives_clip_t *sfile;
  weed_timecode_t tc;
  weed_layer_t *layer;
  LiVESPixbuf *pixbuf;
  LiVESXCursor *cursor;
  int clips[1];
  int64_t frames[1];
  double cx, cy;
  char *imname;
  lives_alarm_t alarm_handle, fps_alarm = LIVES_NO_ALARM;

  int x = 0, y = 0, frameno = 0;
  int w = GUI_SCREEN_WIDTH, h = GUI_SCREEN_HEIGHT;

  if (lpt) lives_proc_thread_set_cancellable(lpt);

  if (recargs->screen_area == SCREEN_AREA_FOREGROUND) {
    win = LIVES_MAIN_WINDOW_WIDGET;
    get_border_size(win, &x, &y);
    x = abs(x);
    y = abs(y);
    w = lives_widget_get_allocation_width(win);
    h = lives_widget_get_allocation_height(win);
  }

  sfile = mainw->files[recargs->clipno];
  clips[0] = recargs->clipno;
  migrate_from_staging(recargs->clipno);

  lives_widget_set_sensitive(mainw->desk_rec, TRUE);
  alarm_handle = lives_alarm_set(TICKS_PER_SECOND_DBL * recargs->delay_time);
  lives_nanosleep_until_nonzero(!lives_alarm_check(alarm_handle) || (lpt && lives_proc_thread_get_cancelled(lpt)));
  lives_alarm_clear(alarm_handle);

  if (lpt && lives_proc_thread_get_cancelled(lpt)) goto done;
  //lives_widget_set_sensitive(mainw->desk_rec, FALSE);

  saveargs = (savethread_priv_t *)lives_calloc(1, sizeof(savethread_priv_t));
  saveargs->compression = 100 - prefs->ocp;

  alarm_handle = lives_alarm_set(TICKS_PER_SECOND_DBL * recargs->rec_time);

  //start_audio_rec();

  while (1) {
    if ((recargs->rec_time && !lives_alarm_check(alarm_handle))
	|| (lpt && lives_proc_thread_get_cancelled(lpt)))
      break;

    fps_alarm = lives_alarm_set(TICKS_PER_SECOND_DBL / recargs->fps);

    if (saver_thread) {
      lives_thread_join(*saver_thread, NULL);
      if (saveargs->error
	  || ((recargs->rec_time && !lives_alarm_check(alarm_handle))
	      || (lpt && lives_proc_thread_get_cancelled(lpt)))) {
	lives_alarm_clear(fps_alarm);
	break;
      }
    }
    else saver_thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));

    tc = lives_get_current_ticks();
    layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);

#ifdef FEEDBACK
    lives_widget_set_opacity(LIVES_MAIN_WINDOW_WIDGET, 0.);
    if (mainw->play_window) lives_widget_set_opacity(mainw->play_window, 0.);
    lives_widget_context_update();
#endif

    pixbuf = gdk_pixbuf_get_from_window (capable->wm_caps.root_window, x, y, w, h);
    if (!pixbuf) {
      lives_alarm_clear(fps_alarm);
      break;
    }
    if (!pixbuf_to_layer(layer, pixbuf)) lives_widget_object_unref(pixbuf);

#ifdef FEEDBACK
    if (LIVES_IS_PLAYING) {
      weed_layer_ref(layer);
      mainw->ext_layer = layer;
    }
    if (mainw->play_window) lives_widget_set_opacity(mainw->play_window, 1.);
    lives_widget_context_update();
#endif

    cursor = gdk_window_get_cursor(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET));
    if (cursor) csurf = gdk_cursor_get_surface(cursor, &cx, &cy);
    else csurf = NULL;
    /* GdkCursor *curs = gdk_window_get_device_cursor(NULL, device); */
    /* gdk_cursor_get_image(curs); */

    if (!csurf) g_print("no cursor\n");
    else g_print("Curs at %f, %f\n", cx, cy);

    if (recargs->scale < 1.) {
      if (!resize_layer(layer, (double)w * recargs->scale, (double)h * recargs->scale,
			LIVES_INTERP_FAST, WEED_PALETTE_END, 0)) {
	lives_alarm_clear(fps_alarm);
	weed_layer_free(layer);
	break;
      }
    }

    imname = make_image_file_name(sfile, ++frameno, NULL);

    frames[0] = frameno;
    sfile->event_list = append_frame_event(sfile->event_list, tc, 1, clips, frames);

    if (saveargs->layer) weed_layer_unref(saveargs->layer);
    saveargs->layer = layer;
    if (saveargs->fname) lives_free(saveargs->fname);
    saveargs->fname = imname;

    lives_thread_create(saver_thread, LIVES_THRDATTR_NONE, save_to_png_threaded, saveargs);

    // TODO - check for timeout / cancel here too
    lives_nanosleep_until_zero(lives_alarm_check(fps_alarm) && (!recargs->rec_time || lives_alarm_check(alarm_handle))
			       && (!lpt || !lives_proc_thread_get_cancelled(lpt)));
    lives_alarm_clear(fps_alarm);
  }
  lives_alarm_clear(alarm_handle);

#ifdef FEEDBACK
  lives_widget_set_opacity(LIVES_MAIN_WINDOW_WIDGET, 1.);
  if (mainw->play_window) lives_widget_set_opacity(mainw->play_window, 1.);
  lives_widget_context_update();

  mainw->ext_layer = NULL;
#endif

  if (saver_thread) lives_free(saver_thread);

  if (saveargs) {
    if (saveargs->layer) weed_layer_unref(saveargs->layer);
    if (saveargs->fname) lives_free(saveargs->fname);
    lives_free(saveargs);
  }

 done: // timed out or cancelled
  lives_signal_handler_block(mainw->desk_rec, mainw->desk_rec_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->desk_rec), FALSE);
  lives_signal_handler_unblock(mainw->desk_rec, mainw->desk_rec_func);
  sfile->fps = recargs->fps;
  sfile->hsize = w;
  sfile->vsize = h;
  sfile->start = 1;
  sfile->end = sfile->frames = frameno;
  if (!save_clip_values(recargs->clipno)) sfile->frames = -1;
  if (sfile->frames > 0 && prefs->crash_recovery) add_to_recovery_file(sfile->handle);
  recargs->lpt = lpt;
  lives_widget_set_sensitive(mainw->desk_rec, FALSE);
  lives_idle_add_simple(rec_desk_done, recargs);
}

#endif

char *get_wid_for_name(const char *wname) {
#ifndef GDK_WINDOWING_X11
  return NULL;
#else
  char *wid = NULL, *cmd;
  if (!wname || !*wname) return NULL;

  if (check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL)) {
    cmd = lives_strdup_printf("%s -l", EXEC_WMCTRL);
    wid = grep_in_cmd(cmd, 3, 4, wname, 0, 1, TRUE);
    lives_free(cmd);
    if (wid) {
      //g_print("GOT wm wid %s\n", wid);
      return wid;
    }
  }

  // need to use -root -tree

  if (check_for_executable(&capable->has_xwininfo, EXEC_XWININFO)) {
    cmd = lives_strdup_printf("%s -name \"%s\" 2>/dev/null", EXEC_XWININFO, wname);
    wid = grep_in_cmd(cmd, 1, -1, "Window id:", 3, 1, FALSE);
    lives_free(cmd);
    if (wid) {
      //g_print("GOT xw wid %s\n", wid);
      return wid;
    }
  }

  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    char buff[65536];
    size_t nlines;
    // returns a list, and we need to check each one
    cmd = lives_strdup_printf("%s search \"%s\"", EXEC_XDOTOOL, wname);
    lives_popen(cmd, FALSE, buff, 65536);
    lives_free(cmd);
    if (THREADVAR(com_failed)
	|| (!*buff || !(nlines = get_token_count(buff, '\n')))) {
      THREADVAR(com_failed) = FALSE;
      return wid;
    }
    else {
      char buff2[1024];
      char **lines = lives_strsplit(buff, "\n", nlines);
      for (int l = 0; l < nlines; l++) {
	if (!*lines[l]) continue;
	cmd = lives_strdup_printf("%s getwindowname %s", EXEC_XDOTOOL, lines[l]);
	lives_popen(cmd, FALSE, buff2, 1024);
	lives_free(cmd);
	if (THREADVAR(com_failed)) {
	  THREADVAR(com_failed) = FALSE;
	  break;
	}
	lives_chomp(buff2, FALSE);
	if (!lives_strcmp(wname, buff2)) {
	  wid = lives_strdup_printf("0x%lX", atol(lines[l]));
	  break;
	}
      }
      lives_strfreev(lines);
    }
  }
  if (wid) {
    g_print("GOT xxxxxxwm wid %s\n", wid);
    return wid;
  }
  return wid;
#endif
}


boolean hide_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return NULL;
#endif
  if (!wid) return FALSE;
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    cmd = lives_strdup_printf("%s windowminimize \"%s\"", EXEC_XDOTOOL, wid);
    return mini_run(cmd);
  }
  return FALSE;
}


boolean unhide_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (!wid) return FALSE;
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
    cmd = lives_strdup_printf("%s windowmap \"%s\"", EXEC_XDOTOOL, wid);
  return mini_run(cmd);
}


boolean activate_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (!wid) return FALSE;

  if (capable->has_xdotool != MISSING) {
    if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
      cmd = lives_strdup_printf("%s windowactivate \"%s\"", EXEC_XDOTOOL, wid);
  }
  else if (capable->has_wmctrl != MISSING) {
    if (check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL))
      cmd = lives_strdup_printf("%s -Fa \"%s\"", EXEC_WMCTRL, wid);
  }
  else return FALSE;
  return mini_run(cmd);
}


// TODO: xprop -name "title"

char *wm_property_get(const char *key, int *type_guess) {
  char *com, *val = NULL, *res = NULL;
  if (check_for_executable(&capable->has_gsettings, EXEC_GSETTINGS) == PRESENT) {
    com = lives_strdup_printf("%s list-recursively | %s %s", EXEC_GSETTINGS, EXEC_GREP, key);
    res = mini_popen(com);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      if (res) lives_free(res);
      return NULL;
    }
    if (!res) return NULL;
    if (!*res) {
      lives_free(res);
      return NULL;
    }
    else {
      char *separ = lives_strdup_printf(" %s ", key);
      char **array = lives_strsplit(res, separ, 2);
      lives_free(separ);
      lives_free(res);
      if (*array[1] == '\'') {
	val = lives_strndup(array[1] + 1, lives_strlen(array[1]) - 2);
	if (type_guess) *type_guess = WEED_SEED_STRING;
      }
      else {
	val = lives_strdup(array[1]);
	if (type_guess) {
	  if (!lives_strcmp(val, "true") || !lives_strcmp(val, "false")) {
	    *type_guess = WEED_SEED_BOOLEAN;
	  }
	  if (atoi(val)) {
	    *type_guess = WEED_SEED_INT;
	  // *INDENT-OFF*
	  }}}
      lives_strfreev(array);
    }}
  // *INDENT-OFF*
  return val;
}


boolean wm_property_set(const char *key, const char *val) {
  char *com, *res = NULL;
  if (check_for_executable(&capable->has_gsettings, EXEC_GSETTINGS) == PRESENT) {
    com = lives_strdup_printf("%s list-recursively | %s %s", EXEC_GSETTINGS, EXEC_GREP, key);
    res = mini_popen(com);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      if (res) lives_free(res);
      return FALSE;
    }
    if (!res) return FALSE;
    if (!*res) {
      lives_free(res);
      return FALSE;
    }
    else {
      char *separ = lives_strdup_printf(" %s ", key);
      char **array = lives_strsplit(res, separ, 2);
      char *schema = array[0];
      lives_free(separ);
      lives_free(res);
      com = lives_strdup_printf("%s set %s %s %s", EXEC_GSETTINGS, schema, key, val);
      g_print("COM is %s\n", com);
      lives_system(com, TRUE);
      lives_free(com);
      lives_strfreev(array);
    }}
  // *INDENT-OFF*
  return TRUE;
}


boolean get_wm_caps(void) {
  char *wmname;
  if (capable->has_wm_caps) return TRUE;
  capable->has_wm_caps = TRUE;

#if IS_MINGW
  capable->wm_caps.is_composited = TRUE;
  capable->wm_caps.root_window = gdk_screen_get_root_window(mainw->mgeom[widget_opts.monitor].screen);
#else
#ifdef GUI_GTK
  capable->wm_caps.is_composited = gdk_screen_is_composited(mainw->mgeom[widget_opts.monitor].screen);
  capable->wm_caps.root_window = gdk_screen_get_root_window(mainw->mgeom[widget_opts.monitor].screen);
#else
  capable->wm_caps.is_composited = FALSE;
  capable->wm_caps.root_window = NULL;
#endif
#endif

  capable->wm_caps.wm_focus = wm_property_get("focus-new-windows", NULL);

  capable->wm_type = getenv(XDG_SESSION_TYPE);
  wmname = getenv(XDG_CURRENT_DESKTOP);

  if (!wmname) {
    if (capable->wm_name) wmname = capable->wm_name;
  }
  if (!wmname) return FALSE;

  capable->has_wm_caps = TRUE;
  lives_snprintf(capable->wm_caps.wm_name, 64, "%s", wmname);

  if (!strcmp(capable->wm_caps.wm_name, WM_XFWM4) || !strcmp(capable->wm_name, WM_XFWM4)) {
    lives_snprintf(capable->wm_caps.panel, 64, "%s", WM_XFCE4_PANEL);
    capable->wm_caps.pan_annoy = ANNOY_DISPLAY | ANNOY_FS;
    capable->wm_caps.pan_res = RES_HIDE | RESTYPE_ACTION;
    lives_snprintf(capable->wm_caps.ssave, 64, "%s", WM_XFCE4_SSAVE);
    lives_snprintf(capable->wm_caps.color_settings, 64, "%s", WM_XFCE4_COLOR);
    lives_snprintf(capable->wm_caps.display_settings, 64, "%s", WM_XFCE4_DISP);
    lives_snprintf(capable->wm_caps.ssv_settings, 64, "%s", WM_XFCE4_SSAVE);
    lives_snprintf(capable->wm_caps.pow_settings, 64, "%s", WM_XFCE4_POW);
    lives_snprintf(capable->wm_caps.settings, 64, "%s", WM_XFCE4_SETTINGS);
    lives_snprintf(capable->wm_caps.term, 64, "%s", WM_XFCE4_TERMINAL);
    lives_snprintf(capable->wm_caps.taskmgr, 64, "%s", WM_XFCE4_TASKMGR);
    lives_snprintf(capable->wm_caps.sshot, 64, "%s", WM_XFCE4_SSHOT);
    return TRUE;
  }
  if (!strcmp(capable->wm_caps.wm_name, WM_KWIN) || !strcmp(capable->wm_name, WM_KWIN)) {
    lives_snprintf(capable->wm_caps.panel, 64, "%s", WM_KWIN_PANEL);
    lives_snprintf(capable->wm_caps.ssave, 64, "%s", WM_KWIN_SSAVE);
    lives_snprintf(capable->wm_caps.color_settings, 64, "%s", WM_KWIN_COLOR);
    lives_snprintf(capable->wm_caps.display_settings, 64, "%s", WM_KWIN_DISP);
    lives_snprintf(capable->wm_caps.ssv_settings, 64, "%s", WM_KWIN_SSAVE);
    lives_snprintf(capable->wm_caps.pow_settings, 64, "%s", WM_KWIN_POW);
    lives_snprintf(capable->wm_caps.settings, 64, "%s", WM_KWIN_SETTINGS);
    lives_snprintf(capable->wm_caps.term, 64, "%s", WM_KWIN_TERMINAL);
    lives_snprintf(capable->wm_caps.taskmgr, 64, "%s", WM_KWIN_TASKMGR);
    lives_snprintf(capable->wm_caps.sshot, 64, "%s", WM_KWIN_SSHOT);
    return TRUE;
  }
  return FALSE;
}


int get_window_stack_level(LiVESXWindow *xwin, int *nwins) {
#ifndef GUI_GTK
  if (nwins) *nwins = -1;
  return -1;
#else
  int mywin = -1, i = 0;
  LiVESList *winlist = gdk_screen_get_window_stack(mainw->mgeom[widget_opts.monitor].screen),
    *list = winlist;
  for (; list; list = list->next, i++) {
    if ((LiVESXWindow *)list->data == xwin) mywin = i;
    lives_widget_object_unref(list->data);
  }
  lives_list_free(winlist);
  if (nwins) *nwins = ++i;
  return mywin;
#endif
}


boolean show_desktop_panel(void) {
  boolean ret = FALSE;
#ifdef GDK_WINDOWING_X11
  char *wid = get_wid_for_name(capable->wm_caps.panel);
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
  char *wid = get_wid_for_name(capable->wm_caps.panel);
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
  if (!wname || !*wname) return FALSE;
  if (check_for_executable(&capable->has_xwininfo, EXEC_XWININFO)) {
    char *state;
    cmd = lives_strdup_printf("%s -name \"%s\"", EXEC_XWININFO, wname);
    state = grep_in_cmd(cmd, 2, -1, "Map State:", 4, 1, FALSE);
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

#define XTEMP "XXXXXXXXXX"

static char *get_systmp_inner(const char *suff, boolean is_dir, const char *prefix) {
  /// create a file or dir in /tmp or prefs->workdir
  /// check the name returned has the length we expect
  /// check it was created
  /// ensure it is not a symlink
  /// if a directory, ensure we have rw access
  char *res = NULL;

  if (!check_for_executable(&capable->has_mktemp, EXEC_MKTEMP)) return NULL;
  else {
    size_t slen;
    char *tmp, *com;
    const char *dirflg, *tmpopt;
    if (!prefix) {
      // no prefix, create in $TMPDIR
      if (suff) tmp = lives_strdup_printf("lives-%s-%s", XTEMP, suff);
      else tmp = lives_strdup_printf("lives-%s", XTEMP);
      tmpopt = "t";
      slen = lives_strlen(tmp) + 2;
    }
    else {
      /// suff here is the directory name
      char *tmpfile = lives_strdup_printf("%s%s", prefix, XTEMP);
      tmp = lives_build_filename(suff, tmpfile, NULL);
      lives_free(tmpfile);
      tmpopt = "";
      slen = lives_strlen(tmp);
    }

    if (is_dir) dirflg = "d";
    else dirflg = "";

    com = lives_strdup_printf("%s -n $(%s -q%s%s \"%s\")", capable->echo_cmd, EXEC_MKTEMP, tmpopt,
			      dirflg, tmp);
    lives_free(tmp);
    res = mini_popen(com);
    if (THREADVAR(com_failed)) {
      if (res) lives_free(res);
      THREADVAR(com_failed) = FALSE;
      return NULL;
    }
    if (!res) return NULL;
    if (lives_strlen(res) < slen) {
      lives_free(res);
      return NULL;
    }
  }
  if (!lives_file_test(res, LIVES_FILE_TEST_EXISTS)
      || lives_file_test(res, LIVES_FILE_TEST_IS_SYMLINK)) {
      lives_free(res);
      return NULL;
  }
  if (is_dir) {
    if (!check_dir_access(res, FALSE)) {
      lives_free(res);
      return NULL;
    }
  }
  return res;
}

LIVES_GLOBAL_INLINE char *get_systmp(const char *suff, boolean is_dir) {
  return get_systmp_inner(suff, is_dir, NULL);
}

static char *_get_worktmp(const char *prefix, boolean is_dir) {
  char *dirname = NULL;
  char *tmpdir = get_systmp_inner(prefs->workdir, is_dir, prefix);
  if (tmpdir) {
    dirname = lives_path_get_basename(tmpdir);
    lives_free(tmpdir);
  }
  return dirname;
}

LIVES_GLOBAL_INLINE char *get_worktmp(const char *prefix) {
  if (!prefix) return NULL;
  return _get_worktmp(prefix, TRUE);
}

LIVES_GLOBAL_INLINE char *get_worktmpfile(const char *prefix) {
  if (!prefix) return NULL;
  return _get_worktmp(prefix, FALSE);
}


LIVES_GLOBAL_INLINE char *get_localsharedir(const char *subdir) {
  char *localshare;
  if (!capable->xdg_data_home) capable->xdg_data_home = getenv(XDG_DATA_HOME);
  if (!capable->xdg_data_home) capable->xdg_data_home = lives_strdup("");
  if (!*capable->xdg_data_home)
    localshare = lives_build_path(capable->home_dir, LOCAL_HOME_DIR, LIVES_SHARE_DIR, subdir, NULL);
  else localshare = lives_build_path(capable->xdg_data_home, subdir, NULL);
  return localshare;
}


boolean notify_user(const char *detail) {
  if (check_for_executable(&capable->has_notify_send, EXEC_NOTIFY_SEND)) {
    char *msg = lives_strdup_printf("\n\n%s-%s says '%s completed\n\n'",
				    lives_get_application_name(), LiVES_VERSION, detail);
    char *cmd = lives_strdup_printf("%s \"%s\"", EXEC_NOTIFY_SEND, msg);
    boolean ret = mini_run(cmd);
    lives_free(msg);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    return ret;
  }
  return FALSE;
}


boolean check_snap(const char *prog) {
  // not working yet...
  if (!check_for_executable(&capable->has_snap, EXEC_SNAP)) return FALSE;
  char *com = lives_strdup_printf("%s find %s", EXEC_SNAP, prog);
  char *res = grep_in_cmd(com, 0, 1, prog, 0, 1, FALSE);
  if (!res) return FALSE;
  lives_free(res);
  return TRUE;
}


#define SUDO_APT_INSTALL "sudo apt install %s"
#define SU_PKG_INSTALL "su pkg install %s"

char *get_install_cmd(const char *distro, const char *exe) {
  char *cmd = NULL, *pkgname = NULL;

  if (!lives_strcmp(exe, EXEC_PIP)) {
    if (!lives_strcmp(distro, DISTRO_UBUNTU)) {
      if (capable->python_version >= 3000000) pkgname = "python3-pip";
      else if (capable->python_version >= 2000000) pkgname = "python-pip";
      else pkgname = "python3 python3-pip";
    }
    if (!lives_strcmp(distro, DISTRO_FREEBSD)) {
      if (capable->python_version >= 3000000) pkgname = "py3-pip";
      else if (capable->python_version >= 2000000) pkgname = "py2-pip";
      else pkgname = "python py3-pip";
    }
  }
  if (!strcmp(exe, EXEC_GZIP)) pkgname = EXEC_GZIP;

  if (!pkgname) return NULL;

  // TODO - add more, eg. pacman, dpkg
  if (!lives_strcmp(distro, DISTRO_UBUNTU)) {
    cmd = lives_strdup_printf(SUDO_APT_INSTALL, pkgname);
  }
  if (!lives_strcmp(distro, DISTRO_FREEBSD)) {
    cmd = lives_strdup_printf(SU_PKG_INSTALL, pkgname);
  }
  return cmd;
}


boolean get_distro_dets(void) {
#ifndef IS_LINUX_GNU
  capable->distro_name = lives_strdup(capable->os_name);
  capable->distro_ver = lives_strdup(capable->os_release);
#else
#define LSB_OS_FILE "/etc/lsb-release"
  char *com = lives_strdup_printf("%s %s", capable->cat_cmd, LSB_OS_FILE), *ret;
  if ((ret = mini_popen(com))) {
    int xlen = get_token_count(ret, '=');
    char **array = lives_strsplit(ret, "=", xlen);
    lives_free(ret);
    if (xlen > 1) {
      lives_strstop(array[1], '\n');
      capable->distro_name = lives_strdup(array[1]);
      if (xlen > 2) {
	lives_strstop(array[2], '\n');
	capable->distro_ver = lives_strdup(array[2]);
	if (xlen > 3) {
	  lives_strstop(array[3], '\n');
	  capable->distro_codename = lives_strdup(array[3]);
	}}}
    lives_strfreev(array);
    return TRUE;
  }
#endif
  return FALSE;
}


int get_num_cpus(void) {
#ifdef IS_DARWIN
  kerr = host_processor_info(mach_host_self(), PROCESSOR_BASIC_INFO, &numProcessors, &processorInfo, &numProcessorInfo);
  if (kerr == KERN_SUCCESS) {
    vm_deallocate(mach_task_self(), (vm_address_t) processorInfo, numProcessorInfo * sizint);
  }
  return numProcessors;
#else
  char buffer[1024];
  char command[PATH_MAX];
#ifdef _SC_NPROCESSORS_ONLN
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cores > 0) return num_cores;
#endif
#ifdef IS_FREEBSD
  lives_snprintf(command, PATH_MAX, "sysctl -n kern.smp.cpus");
#else
  lives_snprintf(command, PATH_MAX, "%s processor /proc/cpuinfo 2>/dev/null | %s -l 2>/dev/null",
                 capable->grep_cmd, capable->wc_cmd);
#endif
  lives_popen(command, TRUE, buffer, 1024);
  return atoi(buffer);
#endif
}


boolean get_machine_dets(void) {
#ifdef IS_FREEBSD
  char *com = lives_strdup("sysctl -n hw.model");
#else
  char *com = lives_strdup_printf("%s -m1 \"^model name\" /proc/cpuinfo | "
				  "%s -e \"s/.*: //\" -e \"s:\\s\\+:/:g\"",
				  capable->grep_cmd, capable->sed_cmd);
#endif
  capable->cpu_name = mini_popen(com);

  com = lives_strdup("uname -o");
  capable->os_name = mini_popen(com);

  com = lives_strdup("uname -r");
  capable->os_release = mini_popen(com);

  com = lives_strdup("uname -m");
  capable->os_hardware = mini_popen(com);

  capable->cacheline_size = capable->cpu_bits * 8;

#if IS_X86_64
  if (!strcmp(capable->os_hardware, "x86_64")) capable->cacheline_size = get_cacheline_size();
#endif

  com = lives_strdup("uname -n");
  capable->mach_name = mini_popen(com);

  com = lives_strdup("whoami");
  capable->username = mini_popen(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  }
  return TRUE;
}


double get_disk_load(const char *mp) {
  // not really working yet...
  if (!mp) return -1.;
  else {
#ifndef IS_LINUX_GNU
  return 0.;
#else
#define DISK_STATS_FILE "/proc/diskstats"
    static ticks_t lticks = 0;
    static uint64_t lval = 0;
    double ret = -1.;
    const char *xmp;
    char *com, *res;
    if (!lives_strncmp(mp, "/dev/", 5))
      xmp = (char *)mp + 5;
    else
      xmp = mp;
    com = lives_strdup_printf("%s -n $(%s %s %s)", capable->echo_cmd,
			      capable->grep_cmd, xmp, DISK_STATS_FILE);
    if ((res = mini_popen(com))) {
      int p;
      int xbits = get_token_count(res, ' ');
      char **array = lives_strsplit(res, " ", xbits);
      lives_free(res);
      for (p = 0; p < xbits; p++) if (!strcmp(array[p], xmp)) break;
      p += 10;
      if (xbits > p) {
	uint64_t val = atoll(array[p]);
	ticks_t clock_ticks;
	if (LIVES_IS_PLAYING) clock_ticks = mainw->clock_ticks;
	else clock_ticks = lives_get_current_ticks();
	if (lticks > 0 && clock_ticks > lticks && val > lval) {
	  ret = (double)(val - lval) / ((double)(clock_ticks - lticks) / TICKS_PER_SECOND_DBL);
	  g_print("AND %f %f %f\n", ret, (double)(val - lval),
		  ((double)(clock_ticks - lticks) / TICKS_PER_SECOND_DBL));
	  lticks = clock_ticks;
	  lval = val;
	}
	if (lval == 0 && val > 0) {
	  lticks = clock_ticks;
	  lval = val;
	}
      }
      lives_strfreev(array);
    }
  return ret;
  }
  return -1.;
#endif
}


LIVES_GLOBAL_INLINE double check_disk_pressure(double current) {
  /* double dload = get_disk_load(capable->mountpoint); */
  /* if (dload == 0.) { */
  /*   dload = (current + 1.) * ((double)get_cpu_load(0) / (double)80000000.); */
  /*   if (dload < 1.) dload = 0.; */
  /* } */
  /* if (dload > current) current = dload; */
  get_proc_loads(FALSE);
  return current;
}


int set_thread_cpuid(pthread_t pth) {
#ifdef CPU_ZERO
  cpu_set_t cpuset;
  int mincore = 0;
  float minload = 1000.;
  int ret = 0;
  for (int i = 1; i <= capable->ncpus; i++) {
    float load = *(get_core_loadvar(i));
    if (load > 0. && load < minload) {
      minload = load;
      mincore = i;
    }
  }
  if (mincore) {
    int err;
    CPU_ZERO(&cpuset);
    CPU_SET(--mincore, &cpuset);
    err = pthread_setaffinity_np(pth, sizeof(cpu_set_t), &cpuset);
    if (err) {LIVES_ERROR("pthread aff error");}
    else {
      ret = mincore;
    }
  }
#endif
  return ret;
}


typedef struct {
  uint64_t tot, idlet;
  int64_t ret;
} oldvalues;

static LiVESList *cpuloadlist = NULL;

#define CPU_STATS_FILE "/proc/stat"

int64_t get_cpu_load(int cpun) {
  /// return reported load for CPU cpun (% * 1 million)
  /// as a bonus, if cpun == -1, returns boot time
  // returns -1 if the value cannot be read
  oldvalues *ovals = NULL;
  FILE *file;
  char buffer[1024];
  unsigned long long boottime = 0;
  unsigned long long user = 0, nice = 0, system = 0, idle = 0;
  unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
  uint64_t idlet, sum, tot;
  int64_t ret = -1;
  int xcpun = 0;

  if (!lives_file_test(CPU_STATS_FILE, LIVES_FILE_TEST_EXISTS)) return -1;

  file = fopen(CPU_STATS_FILE, "r");
  if (!file) return -1;

  if (cpun < 0) {
    while (1) {
      if (!fgets(buffer, 1024, file)) {
	fclose(file);
	return -1;
      }
      if (sscanf(buffer, "btime %16llu", &boottime) > 0) {
	fclose(file);
	return boottime;
      }
    }
  }

  if (!fgets(buffer, 1024, file)) {
    fclose(file);
    return -1;
  }
  if (!cpun) {
    if (fscanf(file,
	     "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %*s %*s\n",
	       &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) <= 0) {
      fclose(file);
      return -1;
    }
  }
  else {
    while (1) {
      if (!fgets(buffer, 1024, file)) {
	fclose(file);
	return -1;
      }
      if (sscanf(buffer,
		 "cpu%d %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %*s %*s\n",
		 &xcpun, &user, &nice, &system, &idle, &iowait, &irq,
		 &softirq, &steal) < 0) {
	fclose(file);
	return -1;
      }
      if (++xcpun == cpun) break;
    }
  }
  fclose(file);

  idlet = idle + iowait;
  sum = user + nice + system + irq + softirq + steal;
  tot = sum + idlet;

  if (idx_list_get_data(cpuloadlist, cpun, (void **)&ovals)) {
    if (tot != ovals->tot) {
      float totd = (float)(tot - ovals->tot);
      float idled = (float)(idlet - ovals->idlet);
      float load = (totd - idled) / totd;
      ret = load * (float)MILLIONS(1);
    }
    else ret = ovals->ret;
  }
  else ovals = (oldvalues *)lives_malloc(sizeof(oldvalues));
  ovals->tot = tot;
  ovals->idlet = idlet;
  ovals->ret = ret;
  cpuloadlist = idx_list_update(cpuloadlist, cpun, ovals);
  return ret;
}

#define N_CPU_MEAS 64

static volatile float **vals = NULL;
static void *proc_load_stats = NULL;
static size_t nfill = 0; // will go away

volatile float **const get_proc_loads(boolean reset) {
  boolean doinit = FALSE;

  if (!vals || reset) {
    running_average(NULL, capable->ncpus + 1, &proc_load_stats);
    running_average(NULL, N_CPU_MEAS, &proc_load_stats);
  }
  if (!vals) {
    doinit = TRUE;
    vals = (volatile float **)lives_calloc(sizeof(float *), capable->ncpus + 1);
  }
  for (int i = 0; i <= capable->ncpus; i++) {
    float load = (float)get_cpu_load(i) / (float)10000.;
    nfill = running_average(&load, i, &proc_load_stats);
    if (doinit) vals[i] = (volatile float *)lives_malloc(4);
    *vals[i] = load;
  }
  return vals;
}


volatile float *get_core_loadvar(int corenum) {
  return vals[corenum];
}


double analyse_cpu_stats(void) {
#if 0
  static lives_object_instance_t *statsinst = NULL;
  volatile float *cpuvals = vals[0];
  weed_param_t *param;
  lives_object_transform_t *tx;
  lives_object_status_t *st;

  // we should either create or update statsinst
  if (!statsinst) {
    const lives_object_t *math_obj = lives_object_template_for_type(OBJECT_TYPE_MATH,
								    MATH_OBJECT_SUBTYPE_STATS);
    // get the math.stats template
    // we want to know how to get an instantc of it, we can look for the transform for
    // the LIVES_INTENTION_CREATE_INSTANCE intent
    tx = find_transform_for_intent(LIVES_OBJECT(math_obj), LIVES_INTENTION_CREATE_INSTANCE);
    if (requirements_met(tx)) {
      transform(LIVES_OBJECT(math_obj), tx, &statsinst);
    }
    else {
      // display unmet reqts.
    }
  }
  // now we have a few things we can do with the maths stats instance
  // in this case, look for deviance from the mean
  tx = find_transform_for_intent(statsinst, MATH_INTENTION_DEV_FROM_MEAN);

  // we could check but requts. are a value and an array of data
  // check if data can be satisifed internally from running_averags
  if (rules_lack_param(tx->prereqs, MATH_PARAM_DATA)) {
    param = weed_param_from_prereqs(tx->prereqs, MATH_PARAM_DATA);
    //set_float_array_param(&param->value, proc_load_stats, get_tab_size());
    weed_set_voidptr_value(param, WEED_LEAF_VALUE, proc_load_stats);

    param = weed_param_from_prereqs(tx->prereqs, MATH_PARAM_DATA_SIZE);
    weed_set_int_value(param, WEED_LEAF_VALUE, nfill);
  }
  // need to set the value too
  //param = rfx_param_from_name(tx->prereqs->reqs,
  param = weed_param_from_prereqs(tx->prereqs, MATH_PARAM_VALUE);
  weed_set_double_value(param, WEED_LEAF_VALUE, cpuvals[nfill - 1]);

  // call transform and check status
  st = transform(statsinst, tx, NULL);

  if (*st->status != LIVES_OBJECT_STATUS_NORMAL) {
    lives_object_status_free(st);
    lives_object_transform_free(tx);
    return 0.;
  }
  lives_object_transform_free(tx);
  lives_object_status_free(st);

  // now just get result
  param = weed_param_from_object(statsinst, MATH_PARAM_RESULT);
  return (float)weed_param_get_value_double(param);
#endif
  return 0.;
}
