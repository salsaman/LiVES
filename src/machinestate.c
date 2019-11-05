// machinestate.c
// LiVES
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#include <sys/statvfs.h>

#include "main.h"
#include "support.h"

void init_random() {
  ssize_t randres = -1;
  uint32_t rseed;
  int randfd;
  int i;

  // try to get randomness from /dev/urandom
  randfd = lives_open2("/dev/urandom", O_RDONLY);

  if (randfd > -1) {
    randres = read(randfd, &rseed, sizint);
    close(randfd);
  }

  gettimeofday(&tv, NULL);
  rseed += tv.tv_sec + tv.tv_usec;

  lives_srandom(rseed);

  for (i = 0; i < 10000; i++) {
    long int a, b;
    a = lives_random();
    b = a * lives_random();
    a = b * lives_random();
  }

  randres = -1;

  randfd = lives_open2("/dev/urandom", O_RDONLY);

  if (randfd > -1) {
    randres = read(randfd, &rseed, sizint);
    close(randfd);
  }

  if (randres != sizint) {
    gettimeofday(&tv, NULL);
    rseed = tv.tv_sec + tv.tv_usec;
  }

  fastsrand(rseed);
}


boolean load_measure_idle(livespointer data) {
#ifdef LOADCHECK

  // function is called as an idlefunc, and we count how many calls per second
  // to give an estimate of machine load

  // measured values: 161 - 215, avg 154 - 181

  static int64_t load_count = -1; // count the fn calls until we reach load_check_count
  static int64_t load_check_count = INIT_LOAD_CHECK_COUNT;
  static int64_t tchecks = 0;
  static ticks_t last_check_ticks = 0; // last time we checked
  static ticks_t total_check = 0; // sum of all deltas
  static int nchecks = 0; // how many times checked
  static int nchecks_counted = 0; // how many valid checks counted

  static double low_avg = 1000000000.;
  static double high_avg = 0.;

  static int varcount = 0; // count of variant values within varratio, reset by vardecay
  static int vardecay = 10, xvardecay = 0; // how many sequential non-variant counts to reset varcount
  double varratio = .9; // ratio of variant to last variant to increment varcount
  static double cstate = -1.; // new state set by similar variant values within a limit
  static double cstate_count = 0;
  int cstate_limit = 4; // number of consecutive variant values within varratio to set cstate
  int cstate_reset = 10; // number of variants at cstate to reset avg
  static double vardelta = 0.; // delta time between variant and last variant
  static ticks_t varperiod = 0; // periodic variance
  int nvarperiod = 2; // number of variant deltas to reset varperiod
  static int xnvarperiod = 0;
  double perratio = .9;
  static ticks_t varticks = 0;
  boolean spike = FALSE;
  static double lvariance = 0.;
  static double xlvariance = 0.;
  static double hload = 0.;
  static double lload = 100000000.;
  static double rescale = 1.;
  int64_t timelimit = 0;
  static int64_t ntimer = -1;
  static boolean add_idle = TRUE;
  double tpi;

  char *msg;
  ticks_t delta_ticks;
  double check_time = TARGET_CHECK_TIME; // adjust load_check_count so we check this number of secs.
  double variance;
  static int phase = 0;

  sched_yield();

  if (mainw->loadmeasure == 0) return FALSE; // was not added so we shouldn't be here
  if (prefs->loadchecktime <= 0.) return FALSE; // function disabled

  if (ntimer > -1 && mainw->loadmeasure_phase == 2) {
    if (++ntimer < timelimit) {
      // during playback, the timer doesnt run so we need to simulate it
      return TRUE;
    }
    ntimer = 0;
  }

  if (mainw->loadmeasure_phase == 1) {
    // continuous checking uses 100% cpu, so we need to pause it
    mainw->loadmeasure = lives_timer_add(ME_DELAY, load_measure_idle, NULL);
    mainw->loadmeasure_phase = 2; // timer wait phase
    return FALSE;
  }

  if (mainw->loadmeasure_phase == 2) {
    // timer fired mode
    last_check_ticks = lives_get_current_ticks();
    mainw->loadmeasure = lives_idle_add_full(G_PRIORITY_LOW, load_measure_idle, NULL, NULL);
    mainw->loadmeasure_phase = 0;
    return FALSE;
  }

  // idlecount mode

  // count idle calls until we reach load_check_count
  if (++load_count < load_check_count) return TRUE;

  g_print("idlephase finished\n");

  // check once per QUICK_CHECK_TIME until we reach TARGET_CHECK_TIME, then once per TARGET_CHECK_TIME seconds
  if (total_check < TARGET_CHECK_TIME || nchecks < N_QUICK_CHECKS - 1) check_time = QUICK_CHECK_TIME;

  tpi = capable->time_per_idle;

  delta_ticks = lives_get_current_ticks() - last_check_ticks;
  g_print("delta_ticks was %ld\n", delta_ticks);

  if (delta_ticks < 100) {
    // too quick, run more idleloops
    load_check_count *= 2;
    return TRUE;
  }

  // reset to timer
  mainw->loadmeasure = lives_timer_add(ME_DELAY, load_measure_idle, NULL);
  add_idle = TRUE;

  //load_count /= rescale;
  tchecks += (int64_t)load_count;
  total_check += delta_ticks;

  capable->time_per_idle = (double)delta_ticks / (double)load_count / TICKS_PER_SECOND_DBL;

  fprintf(stderr, "%.3f %ld %ld\n", capable->time_per_idle, delta_ticks, load_count);

  if (capable->time_per_idle > 0.) {
    int64_t nload_check_count;
    nload_check_count = 1. + check_time / capable->time_per_idle;
    load_check_count = nload_check_count;
    if (nload_check_count > 1.5 * load_check_count) load_check_count = 1.5 * load_check_count;
  }
  if (nchecks > N_QUICK_CHECKS - 1) {
    // variance tells us the ratio of delta time to the current target time
    // if this is very large or small we ignore this check. For example when playing the, idlefunc is not called so time
    // passes without any checking, producing false results.
    variance = (double)delta_ticks / (check_time * TICKS_PER_SECOND_DBL);
    if (variance < .8) {
      rescale = variance;
      check_time = 1;
      return TRUE;
    }
    if (variance > VAR_MAX || variance < VAR_MIN) {
      double load_value = LOAD_SCALING / (double)load_count * check_time;
      double tvar = (double)delta_ticks / (check_time * TICKS_PER_SECOND_DBL);
      double cvar = (double)delta_ticks / (double)load_count / (tpi * TICKS_PER_SECOND_DBL);
      if (xlvariance > 0. && variance < xlvariance && variance * xlvariance > .95) {
        LIVES_INFO("Spike value detected");
        spike = TRUE;
      }
      if (variance > lvariance) {
        xlvariance = variance;
      } else xlvariance = 0.;
      lvariance = variance;
      msg = lives_strdup_printf("Load value is %.3f, avg is %3.f, total loops = %ld\nVariance was %.3f, so ignoring this value."
                                "time variance was %f and count variance was %f",
                                load_value, capable->avg_load, tchecks, variance, tvar, cvar);
      LIVES_INFO(msg);
      g_free(msg);
      if (spike) {
        if (varcount > 0) varcount--;
      } else {
        varcount++;
        xvardecay = 0;
        if (varcount > cstate_limit) {
          if (cstate == -1.) {
            cstate = cvar;
            cstate_count = 1;
          } else {
            if ((cvar > cstate && cstate / cvar > varratio) || (cvar < cstate && cvar / cstate > varratio)) {
              cstate_count++;
              if (cstate_count >= cstate_reset) {
                // reset avg
                nchecks_counted = 0;
                capable->avg_load = load_value;
                cstate_count = 0;
                varcount = 0;
                cstate = -1;
                LIVES_INFO("Load average was reset.");
              } else {
                if (cstate_count > 0) {
                  cstate_count--;
                }
                if (cstate_count == 0) {
                  cstate = cvar;
                  cstate_count = 1;
                }
              }
            }
          }
        }
      }
      // check for periodic variance
      if (varticks > 0) {
        vardelta = last_check_ticks + delta_ticks - varticks;
        if (varperiod == 0) {
          varperiod = vardelta;
          xnvarperiod = nvarperiod;
        } else {
          if ((varperiod > vardelta && (double)vardelta / (double)varperiod > perratio) ||
              (vardelta <= varperiod && (double)vardelta / (double)varperiod > perratio)) {
            xnvarperiod++;
            if (spike) nvarperiod++;
            if (xnvarperiod == 6) {
              msg = lives_strdup_printf("Possible periodic variance with time %.3f\n", varperiod / TICKS_PER_SECOND_DBL);
              LIVES_INFO(msg);
              lives_free(msg);
            }
          } else {
            if (xnvarperiod > 0) {
              xnvarperiod--;
              if (xnvarperiod == 0) {
                varperiod = vardelta;
              }
            }
          }
        }
      }
      varticks = last_check_ticks + delta_ticks;
    } else {
      if (nchecks > 0 && nchecks != 0) {
        varcount--;
        xvardecay++;
        if (xvardecay >= vardecay) {
          cstate = -1.;
          cstate_count = 0;
          varcount = 0;
        }
        capable->load_value = LOAD_SCALING / (double)load_count * check_time;
        if (capable->load_value < lload) lload = capable->load_value;
        if (capable->load_value > hload) hload = capable->load_value;
        capable->avg_load = (capable->avg_load * (double)nchecks_counted + capable->load_value) / (double)(nchecks_counted + 1.);
        msg = lives_strdup_printf("Load value is %.3f (%.3f - %.3f), avg is %3.f (%.3f - %.3f), total loops = %ld, variance was %.3f",
                                  capable->load_value, lload, hload, capable->avg_load, low_avg, high_avg, tchecks, variance);
        LIVES_INFO(msg);
        g_free(msg);
        nchecks_counted++;
        //if (nchecks_counted > 6) {
        if (capable->avg_load > high_avg) high_avg = capable->avg_load;
        if (capable->avg_load < low_avg) low_avg = capable->avg_load;
        //}
      }
    }
  }
  load_count = 0;
  last_check_ticks += delta_ticks;
  nchecks++;

#endif
  return FALSE;
}


/// susbtitute memory functions. These must be real functions and not #defines since we need fn pointers
#define OIL_MEMCPY_MAX_BYTES 1024 // this can be tuned to provide optimal performance

#ifdef ENABLE_ORC
livespointer lives_orc_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) {
    orc_memcpy((uint8_t *)dest, (const uint8_t *)src, n);
    return dest;
  }
  return memcpy(dest, src, n);
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


static memheader_t base;           /* Zero sized block to get us started. */
static memheader_t *freep = &base; /* Points to first free block of memory. */
static memheader_t *usedp;         /* Points to first used block of memory. */

/*
 * Scan the free list and look for a place to put the block. Basically, we're
 * looking for any block the to be freed block might have been partitioned from.
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
 * Request more memory from the kernel.
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
 * Find a chunk from the free list and put it in the used list.
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


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(int64_t origsecs, int64_t origusecs) {
#ifdef USE_MONOTONIC_TIME
  return (lives_get_monotonic_time() - origusecs) * USEC_TO_TICKS;
#else
  gettimeofday(&tv, NULL);
  return TICKS_PER_SECOND * (tv.tv_sec - origsecs) + tv.tv_usec * USEC_TO_TICKS - origusecs * USEC_TO_TICKS;
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


lives_cancel_t check_for_bad_ffmpeg(void) {
  int i, fcount, ofcount;
  char *fname_next;
  boolean maybeok = FALSE;

  ofcount = cfile->frames;
  get_frame_count(mainw->current_file);
  fcount = cfile->frames;

  for (i = 1; i <= fcount; i++) {
    fname_next = make_image_file_name(cfile, i, get_image_ext_for_type(cfile->img_type));
    if (sget_file_size(fname_next) > 0) {
      lives_free(fname_next);
      maybeok = TRUE;
      break;
    }
    lives_free(fname_next);
  }

  cfile->frames = ofcount;

  if (!maybeok) {
    do_error_dialog(
      _("Your version of mplayer/ffmpeg may be broken !\nSee http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}

