// diagnostics.h
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _DIAGNOSTICS_H
#define _DIAGNOSTICS_H

#include "diagnostics.h"
#include "callbacks.h"
#include "startup.h"
#include "maths.h"

void print_diagnostics(uint64_t types) {
  char *tmp;
  if (types & DIAG_APP_STATUS) {
    int64_t nsc, onsc;
    if (what_sup_now() == sup_ready) g_print("Startup complete\n");
    else switch (what_sup_now()) {
      case nothing_sup: g_print("Early stage init\n"); break;
      case run_program_sup: g_print("Startup: First stage, run_the_program\n"); break;
      case pre_init_sup: g_print("Startup: pre_init\n"); break;
      case run_program2_sup: g_print("Startup: run_the_program part 2\n"); break;
      case gtk_launch_sup: g_print("Startup: launch gtk_main\n"); break;
      case startup_sup: g_print("Startup: lives_startup part A\n"); break;
      case init_sup: g_print("Startup: pre_init\n"); break;
      case startupB_sup: g_print("Startup: lives_startup part B\n"); break;
      case startup2_sup: g_print("Startup: lives_startup2\n"); break;
      default: break;
      }
    nsc = mainw->n_service_calls;
    g_print("Number of sevice calls: %lu\n", nsc);
    g_print("checking service call frequency:\n");
    onsc = nsc;
    lives_alarm_set_timeout(MILLIONS(100));
    while (!lives_alarm_triggered()) {
      nsc = mainw->n_service_calls;
      if (nsc != onsc) g_print(".");
    }
    g_print("%lu calls in 0.1 sec\n", nsc - onsc);
  }
  if (types & DIAG_MEMORY) {
    tmp = get_memstats();
    g_print("MEMORY\n");
    g_print("%s\n", tmp);
    lives_free(tmp);
    g_print("\n\nbigblock mapping\n");
    bbsummary();
  }
  if (types & DIAG_THREADS) {
    tmp = get_threadstats();
    g_print("THREADS\n");
    g_print("%s\n", tmp);
    lives_free(tmp);
  }
}


boolean debug_callback(LiVESAccelGroup *group, LiVESWidgetObject *obj, uint32_t keyval, LiVESXModifierType mod,
                       livespointer statep) {
  break_me("debug_callback");
  return TRUE;
}

#define STATS_MSEC 2000. // how often to update
static double inst_fps = 0.;

LIVES_GLOBAL_INLINE double get_inst_fps(boolean get_msg) {
  static ticks_t last_curr_time = 0;
  static ticks_t last_mini_ticks = 0;
  static frames_t last_mm = 0;
  ticks_t currtime = mainw->clock_ticks;
  boolean refresh = TRUE;

  if (mainw->fps_mini_ticks == last_mini_ticks) {
    double tdelta = (currtime - last_curr_time) / TICKS_PER_SECOND_DBL;
    if (tdelta > STATS_MSEC / 1000.)
      mainw->inst_fps = inst_fps = (double)(mainw->fps_mini_measure - last_mm) / tdelta;
    else refresh = FALSE;
  } else last_mini_ticks = mainw->fps_mini_ticks;
  if (refresh) {
    last_mm = mainw->fps_mini_measure;
    last_curr_time = currtime;
  }
  if (get_msg) get_stats_msg(TRUE);
  return inst_fps;
}


void show_all_leaves(weed_plant_t *plant) {
  weed_size_t nleaves;
  char **keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Displaying all %d leaf values in plant %p\n",  nleaves, plant);
  if (!keys) {
    fprintf(stderr, "keys are NULL\n");
    return;
  } else {
    if (!nleaves) {
      fprintf(stderr, "plant has no leaves\n");
      return;
    } else {
      for (int n = 0;  keys[n]; n++) {
        int st = weed_leaf_seed_type(plant, keys[n]);
        const char *fmt = get_fmtstr_for_st(st);
        char *txt = NULL;
        FOR_ALL_SEED_TYPES(st, txt = lives_strdup_printf, fmt, weed_get_, _value, plant, keys[n], NULL);
        if (txt) {
          g_print("\n%s %s has value: %s\n", weed_seed_to_ctype(st, FALSE), keys[n], txt);
          lives_free(txt);
        }
        lives_free(keys[n]);
      }
      lives_free(keys);
      g_print("\n\n");
    }
  }
}


void show_audit(weed_plant_t *plant) {
  // so in an audit plant we have a lot of other plants we are tracking, stored as plantptr_value, with keys
  // equivalent to stringified versions of their own ptrs
  // so we can list the leaves, convert back to pointers and then examine the plants
  weed_size_t nleaves;
  char **keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "AUDITOR %p has %d plants being tracked\n", plant, nleaves);
  if (!keys) {
    fprintf(stderr, "keys are NULL\n");
    return;
  } else {
    if (!nleaves) {
      fprintf(stderr, "plant has no leaves\n");
      return;
    } else {
      // skip "type" leaf
      for (int n = 1;  keys[n]; n++) {
        int st = weed_leaf_seed_type(plant, keys[n]);
        switch (st) {
        case WEED_SEED_PLANTPTR: {
          if (lives_strtol(keys[n] + 2)) {
            weed_plant_t *pl = weed_get_plantptr_value(plant, keys[n], NULL);
            fprintf(stderr, "plant %d is at %s\n", n, keys[n]);
            //sscanf(keys[n], "%p", &ptr);
            if (pl) show_all_leaves(pl);
          }
        }
        break;
        case WEED_SEED_VOIDPTR: {
          if (lives_strtol(keys[n] + 2)) {
            fprintf(stderr, "voidptr %d is at %s\n", n, keys[n]);
          }
        }
        break;
        default: break;
        }
        free(keys[n]);
      }
      free(keys);
      fprintf(stderr, "\nDONE\n");
    }
    fprintf(stderr, "\n");
  }
}


static void upd_rstats(int n1s, int *pmin, int *pmax, int esrc, uint64_t nx, uint64_t ny) {
  /* if (esrc && *pmax >= *pmin && (n1s < *pmin || n1s > *pmax)) { */
  /*   g_print("rnd limits %d %d 0X%016lX 0X%016lX\n", esrc, n1s, nx, ny); */
  /* } */
  if (n1s < *pmin) *pmin = n1s;
  if (n1s > *pmax) *pmax = n1s;
}


// p(a0 ^ b0) == 0.5
// p(a0 & b0) == 0.25
// however, a0 & b0 => a0 | b0, a0 ^ b0 => a0 | b0, a0 & b0 => !(a0 ^ b0), a0 ^ b0 => !(a0 & b0)
// conditions are not completely independent
// so, test a0 ^ b0 - this should be true 0.5 / 0.5
// if true,  then we already know a | b is true. and a & b is false
// if false then a & b will be true with p 0.5, as will a | b
// rather than summing, we can use Parity to check. Let us start with m bits all with parity 0
// then taking the first pair of 'random' numbers, perform a bitwise xor
// if both numbers are random, the we should have on average 0.5n bits with a 1
// we can store the 1s as parity - simply xor with the current parity, We multiply this by 20 to get around 10.
// now we can AND he numbers whichi should produce 0.25 1s, then we OR with parity, at first we will get 3/4,
// but this will tend to 5/8. So we multiply by 16 to get 10 and add.
// if we had a 0, then p == 0.5, the and result is 1, so again applying xor, approc half of the 0s should flip to 1
// now, counting the 1s and diviging by n. from pass 1 we should have 0.5. so multiplying by 6 gives us 3
// in pass 2, we should have 0.75, then we multiply this by 20 and add., the totalt should now comverge to NR * 20

#define NSTEPS 4

// proceed as follows: generate n 'random' (nomially 64 bit) numbers - r0. r1. r2. r3, ..., rn
// bitwise we define r0 as b0[63], b0[62], b0[61], ..., b0[0]
// and r1 as b1[64], ... b1[0], etc
// define the result of a logic op on rA, rB as l[63], l[62], ... l[0]
// initialise  parity bits to all 0s
// find x = n0 ^ n1 - write result to parity
// now if l[i] is 1, b0[i], b1[i] are {1,0} or {0,1}
// if l[i] is 0, b0[i], b1[i] are {0,0} or {1,1}
// next we find r0 & r2. We cannot determine anything about the bits in r0 from l0
// thus for the second result we cannot know anything a priori from r0 & r2
// however if l1[i] is 1, then r0[i] must be 1, and r2[i] must be 1
// thus if r1[i] was 0, we would have a 1 in parity, if 0, we would have a 1
// thus, if l1[i] is 1, this implies, if there is a 1 value in l1[i], then p[i] (parity) ....
// is the inverse of l1[i].
// Now the 1s in l1 have p(0.25) so we have B / 4 on avearage
// TBD.

int benchmark_rng(int ntests, lives_randfunc_t rfunc, double *q) {
  uint64_t tot_a = 0, tot_b_add = 0, tot_c = 0, rtot = 0, diff;
  int64_t tot_b_sub = 0;
  uint64_t n0, n1, n2, on0 = 0, par = 0;
  int n1s, pmin = 100000, pmax = 0, pmin_and = 10000, pmax_and = 0, pavg, pavg_and, rmin, rmax, rest;
  double dtot_a, dtot_c, dtot_b_add, dtot_b_sub, drtot, axc, ppmin, ppmax, ppmin_and, ppmax_and;
  double qual;
  char *tmp;

  MSGMODE_SAVE;

#ifdef DEBUG_RNG
  MSGMODE_SET(DEBUG_INIT);
#else
  MSGMODE_ON(BLOCK);
#endif

  n0 = (*rfunc)();
  rtot = get_onescount_64(n0);
  n1 = (*rfunc)();
  rtot += get_onescount_64(n1);

  for (int i = 1; i <= ntests; i++) {
    // xor diff
    diff = n0 ^ n1;

    n1s = get_onescount_64(diff);
    upd_rstats(n1s, &pmin, &pmax, 1, n0, n1);
    tot_a += n1s; // xor total
    par ^= diff; // parity apply

    if (on0) {
      // now we need to scramble the bias from n1, we can do this by XORing with
      // (n0 & n1) ^ (n1 & n2) from previous test
      uint64_t v0 = on0 & n0;
      uint64_t v1 = n0 & n1;
      v0 &= v1;
      par ^= v0;
    }

    n1s = get_onescount_64(par);
    upd_rstats(n1s, &pmin, &pmax, 2, par, 0);

    tot_b_add += (uint64_t)(n1s * 20); // avg should be 10 R
    tot_b_sub += (int64_t)(n1s * 20); // avg should be 10 R

    if (i == ntests) break;

    //(n0 ^ n1) | (n0 & n2) ^ (n1 ^ n2)
    n2 = (*rfunc)();
    rtot += get_onescount_64(n2);

    diff = n2 & n0;

    n1s = get_onescount_64(diff);
    upd_rstats(n1s, &pmin_and, &pmax_and, 3, n2, n0);

    tot_c += n1s;

    // if we OR par with diff, then any 1s in par will remain unchanged
    // any 0s will change to 1 iff we have a 1 in diff
    // since diff is n0 & n2, 0.25 * bits should be 1, thus we should alter 1 / 4 of the zeros
    // 1 / 2 + 1 / 2 * 1 / 4 = 5 / 8
    // if we multiply this by 16, we get 10, the same as 0.5 * 20
    // now we add this to tot_b_add and we should approach 20 * ntests
    // we subtract from tot_b_sub and we should be close to 0 at the limit

    par |= diff;
    n1s = get_onescount_64(par);
    tot_b_add += (uint64_t)(n1s * 16); // avg 10 * R
    tot_b_sub -= (int64_t)(n1s * 16); // avg 10 * R
    n1s = n1s * 4 / 5;
    upd_rstats(n1s, &pmin, &pmax, 4, par, 0);

    on0 = n0;
    n0 = n1;
    n1 = n2;
  }

  // XOR total: div by NR and double
  dtot_a = (double)tot_a / (double)(ntests / 2.);

  // tot_c is the and total, normalised to 1.
  dtot_c = (double)tot_c / (double)(ntests / 4.);

  rmax = (int)dtot_a;
  rmin = (int)dtot_a;

  if (dtot_c > dtot_a) rmax = (int)dtot_c;
  else rmin = (int)dtot_c;

  // get avg of pmin, pmax
  pavg = (pmax + pmin) / 2;
  if (pmin + pmax < rmin) rmin = pmin + pmax;

  pavg_and = (pmax_and + pmin_and) / 2;
  if ((pmin_and + pmax_and) * 2 < rmin) rmin = (pmin_and + pmax_and) * 2;

  rest = (rmin + rmax) / 2;

  // deviance of ln2(pmax) = ln2(min) should be approx. 2 * ln2(ntests)
  ppmin = binomial(pmin, rest, 0.5);
  ppmax = binomial(pmax, rest, 0.5);
  ppmin_and = binomial(pmin_and, rest, 0.25);
  ppmax_and = binomial(pmax_and, rest, 0.25);

  drtot = (double)rtot / (double)(ntests) / ((double)(rmax + rmin) / 2.);

  // div 20 NR
  dtot_b_add = (double)tot_b_add / (double)(ntests * 20.) / (double)rest;//   --> R
  dtot_b_sub = (double)tot_b_sub / (double)(ntests * 10.);//    ---> avg non correlation

  axc = dtot_c / dtot_a / 2.;

  // so simplest is to begin with tot_a and tot_c
  // tot_a --> R
  // tot_c --> R
  // - this should give a rough estimate for R
  // tot_b_add and tot_b_sub then give a measure of the parity between 1s and 0s
  // tot_b add should be close to 1, and tot_b_sub should be close to 0
  // tot_b_add represents the ratio of 1s to 0s (but it assumes R == 64)
  // tot_b_sub is a measure of the sequential difference, a negative value represents
  // a a bias towards keeping the same digits, a positve value denotes bias towards changing digits

  // (1 + R - pmax) / (1 + pmin) should be close to 1, as tot_b add, a value > 1. shows a bias towards 1s
  // (symmetry of 1s and 0s) - we can use this insially to estimate R

  // pmin and pmax should both be within the range predicted by the binomial distribution
  // for 1 or more occurances of an event with p(Pn) over T trials,
  // where T is ntests, and Pn is binomial probaility of n events with P(0,25) in R trials
  // from this we should be able to approximate Pn, and then from Pn we can estimate R
  qual = 1. / (fabs(1. - 2. * drtot) * 100  * fabs(1. - dtot_a / (double)rest) * 100 * fabs(1. - 2. * axc) * 100);

  tmp = lives_strdup_printf("Tested RNG, generated %d values.\nEstimate from sequential XOR is %d bits, "
                            "estimate from sequential AND is %d bits.\nRatio of 1s to 0s was %f\n"
                            "AND / XOR correlation is %f (should be 0.5), persistance of same digit is %f\n"
                            "Range over all trials was: XOR %d to %d 1s, average %d, p(min) is %f, p(max) is %f\n"
                            "AND %d to %d 1s, average %d, p(min) is %f, p(max) is %f\n"
                            "Parity checks were %f (should be 0.)  and %f (should be 1.0)\n"
                            "Final analysis: Rbits = %d to %d (avg %d).\nBias towards 1 is %f, "
                            "and sequential randomness is %f. "
                            "Logical correlation is %f\nQUALITY = %f\n\n",
                            ntests, (int)(dtot_a + .5), (int)(dtot_c + .5), drtot, axc, dtot_c / 2., pmin, pmax,
                            pavg, ppmin, ppmax,
                            pmin_and, pmax_and, pavg_and, ppmin_and, ppmax_and, dtot_b_sub, dtot_b_add, rmin, rmax, rest,
                            drtot, dtot_a / (double)rest, axc, qual);

  d_print(tmp);
  lives_free(tmp);

  if (q) *q = qual;

  MSGMODE_RESTORE;

  return rest;
}


#ifdef TEST_RTM_CODE

// Shared counter
#include <immintrin.h> // For RTM
#include <rtmintrin.h> // For RTM

volatile int zzcounter = 0;
static pthread_mutex_t fallback = PTHREAD_MUTEX_INITIALIZER;

void *increment_counter(void *arg) {
  int i;
  for (i = 0; i < 100000; ++i) {
    // Begin a hardware transaction
    uint32_t status = _xbegin();

    // Check if the transaction started successfully
    if (status == _XBEGIN_STARTED) {
      // Perform the memory operation
      zzcounter++;

      // Try to commit the transaction
      _xend();
    } else {
      // Fallback code, if transaction fails
      // In a real-world scenario, you'd use a traditional locking mechanism here
      g_printerr("Transaction failed, fallback to normal increment\n");
      pthread_mutex_lock(&fallback);
      zzcounter++;
      pthread_mutex_unlock(&fallback);
    }
  }
  return NULL;
}


boolean check_rtm(void) {
  // Check if the CPU supports RTM
  if (!((1 << 11) & _xgetbv(0))) {
    g_printerr("RTM not supported by this CPU.\n");
    return FALSE;
  }
  return TRUE;
}


int test_rtm(void) {
  // Create threads
  pthread_t t1, t2;
  if (pthread_create(&t1, NULL, increment_counter, NULL)) {
    g_printerr("Error creating thread\n");
    return 1;
  }
  if (pthread_create(&t2, NULL, increment_counter, NULL)) {
    g_printerr("Error creating thread\n");
    return 1;
  }

  // Wait for threads to finish
  if (pthread_join(t1, NULL)) {
    g_printerr("Error joining thread\n");
    return 2;
  }
  if (pthread_join(t2, NULL)) {
    g_printerr("Error joining thread\n");
    return 2;
  }

  // Print counter value
  g_printerr("Counter value: %d\n", zzcounter);
  return 0;
}


void do_rtm_test(void) {
  if (check_rtm()) test_rtm();
}

#endif


char *get_stats_msg(boolean calc_only) {
  static double av_offs = 0.;
  static int last_pfile = -1;
  static int pseq = -1;
  volatile float *load;
  lives_clip_t *sfile = mainw->files[mainw->playing_file];
  char *msg, *audmsg = NULL, *bgmsg = NULL, *fgpal = NULL;
  char *tmp, *tmp2;
  char *msg2 = lives_strdup("");
  double avsync = 1.0;
  boolean have_avsync = FALSE;

  if (!LIVES_IS_PLAYING) return NULL;

  if (AUD_SRC_INTERNAL) {
    int clipno = get_aplay_clipno();
    if (CLIP_HAS_AUDIO(clipno)) {
      lives_clip_t *afile = mainw->files[clipno];
      avsync = (double)get_aplay_offset()
               / (double)afile->arate / (double)(afile->achans * (afile->asampsize >> 3)); //lives_pulse_get_pos(mainw->jackd
      avsync -= ((double)sfile->last_frameno - .5) / afile->fps
                + (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL;
      have_avsync = TRUE;
    }
    if (pseq != mainw->play_sequence ||
        mainw->playing_file != last_pfile ||
        !mainw->video_seek_ready || !mainw->audio_seek_ready) {
      pseq = mainw->play_sequence;
      last_pfile = mainw->playing_file;
      av_offs = avsync;
    }
    avsync -= av_offs;
  }

  //currticks = lives_get_current_ticks();

  if (calc_only) return NULL;
  load = get_core_loadvar(0);

  if (!prefs->vj_mode) {
    if (have_avsync) {
      audmsg = lives_strdup_printf(_("Audio is %s video by %.4f secs.\n"),
                                   tmp = lives_strdup(avsync >= 0. ? _("ahead of") : _("behind")), fabs(avsync));
      lives_free(tmp);
    } else {
      if (prefs->audio_src == AUDIO_SRC_INT) audmsg = (_("Audio is not playing.\n"));
      else audmsg = (_("Audio source external.\n"));
    }

    if (mainw->blend_file != mainw->current_file && IS_VALID_CLIP(mainw->blend_file)) {
      char *bgpal = get_palette_name_for_clip(mainw->blend_file);
      bgmsg = lives_strdup_printf(_("Bg clip: %d X %d, frame: %d / %d, palette: %s\n"),
                                  mainw->files[mainw->blend_file]->hsize,
                                  mainw->files[mainw->blend_file]->vsize,
                                  mainw->files[mainw->blend_file]->frameno,
                                  mainw->files[mainw->blend_file]->frames,
                                  bgpal);
      lives_free(bgpal);
    }

    fgpal = get_palette_name_for_clip(mainw->current_file);

    msg = lives_strdup_printf(_("%sFrame %d / %d / %d, fps %.3f (target: %.3f)\n"
                                "CPU load %.2f %% : Disk load: %f\n"
                                "Effort: %d / %d, quality: %d, %s (%s)\n%s\n"
                                "Fg clip: %d X %d, palette: %s\n%s\n%s"),
                              audmsg ? audmsg : "",
                              sfile->frameno, sfile->last_req_frame, sfile->frames,
                              inst_fps * sig(sfile->pb_fps), sfile->pb_fps,
                              *load, mainw->disk_pressure,
                              mainw->effort, EFFORT_RANGE_MAX,
                              prefs->pb_quality,
                              tmp = lives_strdup(prefs->pb_quality == 1 ? _("Low")
                                    : prefs->pb_quality == 2 ? _("Med") : _("High")),
                              tmp2 = lives_strdup(prefs->pbq_adaptive ? _("adaptive") : _("fixed")),
                              get_cache_stats(),
                              sfile->hsize, sfile->vsize,
                              fgpal, bgmsg ? bgmsg : "", msg2);
    lives_freep((void **)&msg2);
    lives_freep((void **)&bgmsg); lives_freep((void **)&audmsg);
    lives_freep((void **)&tmp); lives_freep((void **)&tmp2);
  } else {
    if (mainw->blend_file > 0)
      bgmsg = lives_strdup_printf(_("bg: %d/%d "),
                                  mainw->files[mainw->blend_file]->frameno,
                                  mainw->files[mainw->blend_file]->frames);

    msg = lives_strdup_printf("fg: %d/%d, fps %.3f / %.3f, CPU: %.2f, Eff. %d/%d, Q: %s %s",
                              mainw->actual_frame, sfile->frames,
                              inst_fps * sig(sfile->pb_fps), sfile->pb_fps,
                              *load, mainw->effort, EFFORT_RANGE_MAX,
                              tmp = lives_strdup(prefs->pb_quality == 1 ? _("Low")
                                    : prefs->pb_quality == 2 ? _("Med") : _("High")),
                              bgmsg ? bgmsg : "");
    lives_freep((void **)&bgmsg);
    lives_freep((void **)&tmp);
  }
  return msg;
}


static char *explain_missing(const char *exe) {
  char *pt2, *pt3, *pt1 = lives_strdup_printf(_("\t'%s' was not found on your system.\n"
                          "Installation is recommended as it provides the following features\n\t- "), exe);
  if (!lives_strcmp(exe, EXEC_FILE)) pt2 = (_("Enables easier identification of file types,\n\n"));
  else if (!lives_strcmp(exe, EXEC_DU)) pt2 = (_("Enables measuring of disk space used,\n\n"));
  else if (!lives_strcmp(exe, EXEC_GZIP)) pt2 = (_("Enables reduction in file size for some files,\n\n"));
  else if (!lives_strcmp(exe, EXEC_DU)) pt2 = (_("Enables measuring of disk space used,\n\n"));
  else if (!lives_strcmp(exe, EXEC_FFPROBE)) pt2 = (_("Assists in the identification of video clips\n\n"));
  else if (!lives_strcmp(exe, EXEC_IDENTIFY)) pt2 = (_("Assists in the identification of image files\n\n"));
  else if (!lives_strcmp(exe, EXEC_CONVERT)) pt2 = (_("Required for many rendered effects in the clip editor.\n\n"));
  else if (!lives_strcmp(exe, EXEC_COMPOSITE)) pt2 = (_("Enables clip merging in the clip editor.\n\n"));
  else if (!lives_strcmp(exe, EXEC_PYTHON)) pt2 = (_("Allows use of some additional encoder plugins\n\n"));
  else if (!lives_strcmp(exe, EXEC_YOUTUBE_DL)) pt2 = (_("Enables download and import of files from "
        "Youtube and other sites.\n\n"));
  else if (!lives_strcmp(exe, EXEC_XWININFO)) pt2 = (_("Enables identification of external windows "
        "so that they can be recorded.\n\n"));
  else {
    lives_free(pt1);
    pt1 = lives_strdup_printf(_("\t'%s' was not found on your system.\n"
                                "Installation is optional, but may enable additional features\n\n"), exe);
    if (!lives_strcmp(exe, EXEC_XDOTOOL)) pt2 = (_("Enables adjustment of windows within the desktop,\n\n"));
    else pt2 = lives_strdup("");
  }
  pt3 = get_install_cmd(NULL, exe);
  if (pt3) {
    pt3 = lives_strdup_printf(_("Try: %s\n\n"), pt3);
    pt2 = lives_concat(pt2, pt3);
  }
  return lives_concat(pt1, pt2);
}

enum {
  MISS_RT_AUDIO = 1,
  // etc....
};


static char *explain_missing_cpt(int idx) {
  char *libs[10];
  char *text, *text2;
  char *desc, *with = NULL;
  //chat *without = NULL;
  //uint64_t status = 0;

  for (int i = 10; i; libs[--i] = NULL);

  switch (idx) {
  case MISS_RT_AUDIO:
    desc = _("real time audio");
    //status = INSTALL_IMPORTANT;
    libs[0] = "pulse";
    libs[1] = "jack-jackd2";
    with = _("greatly improve audio performance");
    break;
  default:
    return lives_strdup("");
  }
  text = lives_strdup_printf(_("LiVES was compiled without support for %s."), desc);
  if (with) {
    text2 = lives_strdup_printf(_(" Recompiling with this will %s.\n"), with);
    text = lives_concat(text, text2);
  }
  if (libs[0]) {
    text2 = _("Before compiling, try ");
    text = lives_concat(text, text2);
    text2 = get_install_lib_cmd(NULL, libs[0]);
    if (!text2) text2 = lives_strdup_printf(_("installing lib%s-dev"), libs[0]);
    text = lives_concat(text, text2);
    if (libs[1]) {
      char *text3;
      for (int i = 1; i < 10 && libs[i]; i++) {
        text2 = get_install_lib_cmd(NULL, libs[i]);
        if (!text2) text2 = lives_strdup_printf(_("lib%s-dev"), libs[i]);
        text3 = lives_strdup_printf("%s OR %s", text, text2);
        lives_free(text); lives_free(text2);
        text = text3;
      }
    }
  }
  text = lives_concat(text, lives_strdup("\n\n"));
  return text;
}

#define ADD_TO_TEXT(what, exec)   if (!capable->has_##what)	\
    text = lives_concat(text, explain_missing(exec));

#define ADD_TO_CTEXT(idx) ctext = lives_concat(ctext, explain_missing_cpt(idx));

void explain_missing_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *title = (_("What is missing ?")), *text = lives_strdup("");
  char *ctext = lives_strdup("");

  ADD_TO_CTEXT(0);

#if !HAS_PULSE && !ENABLE_JACK
  ADD_TO_CTEXT(MISS_RT_AUDIO);
#endif

  check_for_executable(&capable->has_file, EXEC_FILE);

  ADD_TO_TEXT(sox_sox, EXEC_SOX);
  ADD_TO_TEXT(file, EXEC_FILE);
  ADD_TO_TEXT(du, EXEC_DU);
  ADD_TO_TEXT(identify, EXEC_IDENTIFY);
  ADD_TO_TEXT(ffprobe, EXEC_FFPROBE);
  ADD_TO_TEXT(convert, EXEC_CONVERT);
  ADD_TO_TEXT(composite, EXEC_COMPOSITE);
  if (check_for_executable(&capable->has_python, EXEC_PYTHON) != PRESENT
      && check_for_executable(&capable->has_python3, EXEC_PYTHON3) != PRESENT) {
    ADD_TO_TEXT(python, EXEC_PYTHON);
  }
  ADD_TO_TEXT(gzip, EXEC_GZIP);
  ADD_TO_TEXT(youtube_dl, EXEC_YOUTUBE_DL);
  ADD_TO_TEXT(xwininfo, EXEC_XWININFO);
  if (!*text && !*ctext) {
    lives_free(title); lives_free(text); lives_free(ctext);
    do_info_dialog(_("All optional components located\n"));
    return;
  }
  if (*ctext) {
    char *fintext = lives_strdup_printf(_("Compilation Options:\n%s\n%s"), ctext,
                                        *text ? _("Executables:\n") : "");
    lives_free(ctext);
    ctext = fintext;
  }

  if (*text) {
    if (*ctext) text = lives_concat(ctext, text);
    text = lives_concat(text, (_("\nIf you DO have any of these missing executables, please ensure they are "
                                 "located in your $PATH before restarting LiVES")));
  } else text = ctext;

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  create_text_window(title, text, NULL, TRUE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;;
  lives_free(title);
  lives_free(text);
}
#undef ADD_TO_TEXT

#if 0
//////////////// lives performance manager
static LiVESList *checkdirs = NULL;
boolean check_by_size = TRUE;

void *run_perfmgr(void *data) {
  // pref manager runs in a bg thread
  // monitors system preformance
  // - disk space
  // other threads can update perfmgr with an amount of data (bytes) written
  // or size 0 if unknown
  // if size written is known we check every X bytes
  // otherwise we go by time
  // - we monitor free space on mountpoint of livesprojects (if warn / crit are set)
  // - we monitor space used (if quota is set)
  // - we may check volume containing tmp if the diskspace is known to be low
  // (e.g downloading from remote site)

  while (!cancelled) {
    if (check_by_size) {
      mutex_lock;
      // is check_size > SIZE_THRESH ?
      // - zero size
      mutex_unlock;
      // - check
    } else {
      // check if time_elapsed > TIME_THRESH
      // if checkdirs exists, check each mountpoint
      // if its our mountpoint check quota also, set ds_warn state
      // otherwise, check time depends on fs fill level
    }
  }
}


uint64_t inform_perfmgr(const char *dir, uint64_t handle, size_t tsize) {
  LiVESList *cdirs;
  char *mp = get_mountpoint_for(dir);
  if (tsize) {
    mutex_lock;
    if (check_by_size) {
      // increase size count
      check_size += tsize;
    }
    mutex_unlock();
    return 0;
  }
  if (!handle) handle = gen_unique_id();
  mutex_lock();
  check_by_size = FALSE;
  for (cdirs = checkdirs; cdirs; cdirs = cdirs->next) {
    // thing is struct handle + thr_id
    thing *th = (thing *)cdirs->data;
    if (!lives_strcmp(mp, th->mp)) {
      // save struct with handle + thr_id
      th->handles = lives_list_append_unique(th->handles, handle_and_thrid);
      break;
    }
  }
  if (!cdirs) checkdirs = lives_list_append(checkdirs, new_thing);
  mutex_unlock();
  return handle;
}

void disinform_perfmgr(uint64_t handle, const char *dir) {
  // remove handle from corresponding mountpoint
  // if no more handles, remove node
  // if no more list check_by_time = FALSE;

}
#endif

/// TRASH area /////

#define NITERS 1024
#define DTHRESH 8
#define PMISS 0.99609375
void test_random(void) {
  int counter[64];
  int last[64];
  int buckets[64][4];
  int dist[8][256];
  int bval, dval;
  double prob;
  int d, x;
  uint64_t tt, r;
  /// check quality of random function
  /// - check 1: each binary digit should habe approx. equal 1s and 0s

  lives_memset(counter, 0, 256);
  lives_memset(last, 0, 256);
  lives_memset(buckets, 0, 1024);
  lives_memset(dist, 0, 8192);

  for (x = 0; x < NITERS; x++) {
    uint64_t uu = 1;
    tt = fastrand();
    bval = 0;
    dval = 0;
    for (d = 0; d < 64; d++) {
      bval <<= 1;
      r = tt & uu;
      if (!r) {
        if (x && !(x & 1)) buckets[d][last[d] << 1]++;
        counter[d]--;
        last[d] = 0;
      } else {
        if (x && !(x & 1)) buckets[d][(last[d] << 1) + 1]++;
        counter[d]++;
        last[d] = 1;
        bval++;
      }
      uu <<= 1;
      if ((d & 7) == 7) {
        dist[dval++][bval]++;
        bval = 0;
      }
    }
  }

  prob = PMISS;
  d_print("Checking statistical probabilities\n");
  for (x = 0; x < NITERS; x++) {
    d_print("%d %.4f  ", x, 1. - prob);
    prob *= prob;
  }
  d_print("\n");


  /// results:
  for (d = 0; d < 64; d++) {
    d_print("digit %d: score %d (%.2f%% 1s)\n", d, counter[d],
            ((double)counter[d] + (double)NITERS) / (double)NITERS * 50.);
    d_print("buckets:  ");
    for (x = 0; x < 4; x++) d_print("[%d]: %d    ", x, buckets[d][x]);
    d_print("\n");
  }
  for (d = 0; d < 8; d++) {
    d_print("segment %d:  ", d);
    for (x = 0; x < 256; x++) {
      dval = dist[d][x];
      if (dval >= DTHRESH) d_print("val %d / %d hit %d times  ", d, x, dist[d][x]);
    }
    d_print("\n");
  }
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

//static int test_palette_conversions(void);

void reset_timer_info(void) {
  THREADVAR(timerinfo) = lives_get_current_ticks();
}

double show_timer_info(void) {
  ticks_t xti = lives_get_current_ticks();
  double timesecs;
  g_print("\n\nTest completed in %.4f seconds\n\n",
          (timesecs = ((double)xti - (double)THREADVAR(timerinfo)) / TICKS_PER_SECOND_DBL));
  THREADVAR(timerinfo) = xti;
  return timesecs;
}

static char *randstrg(size_t len) {
  char *strg = lives_calloc(1, len), *ptr = strg;
  for (int i = 1; i < len; i++) *(ptr++) = ((lives_random() & 63) + 32);
  return strg;
}


#ifdef HASH_TEST
void hash_test(void) {
  char *str;
  int i;
  uint32_t val;
  int nr = 100000000;
  THREADVAR(timerinfo) = lives_get_current_ticks();
  for (i = 0; i < nr; i++) {
    str = randstrg(fastrand_int(20));
    val = fast_hash(str, 0) / 7;
    lives_free(str);
  }
  show_timer_info();
  for (i = 0; i < nr; i++) {
    str = randstrg(fastrand_int(20));
    lives_string_hash(str) / 7;
    lives_free(str);
  }
  show_timer_info();
}
#endif


#ifdef WEED_STARTUP_TESTS

void benchmark(void) {
  int nruns = 1000000;
  char *strg = randstrg(400);
  char *strg2 = randstrg(500);
  volatile size_t sz = 1;
  int i, j;
  memcpy(strg2, strg, 400);
  for (j = 0; j < 5; j++) {
    sz++;
    fprintf(stderr, "test %d runs with lives_strlen()", nruns);
    THREADVAR(timerinfo) = lives_get_current_ticks();
    for (i = 0; i < nruns; i++) {
      sz = lives_strcmp(strg, strg2);
      //sz += lives_strlen(strg2);
    }
    show_timer_info();
    sz++;
    fprintf(stderr, "test %d runs with strlen()", nruns);
    THREADVAR(timerinfo) = lives_get_current_ticks();
    for (i = 0; i < nruns; i++) {
      sz = strcmp(strg, strg2);
      //sz += strlen(strg2);
    }
    show_timer_info();
    sz++;
  }
  lives_free(strg2);
}

#endif

typedef struct {
  lsd_struct_def_t *lsd;
  char buff[100000];
  char *strg;
  uint64_t num0, num1;
  void *p;
  char **strgs;
} lives_test_t;



void lives_struct_test(void) {
  const lsd_struct_def_t *lsd;

  lives_test_t *tt = (lives_test_t *)lives_calloc(1, sizeof(lives_test_t)), *tt2;
  lives_clip_data_t *cdata = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));
  //LSD_CREATE_P(lsd, lives_clip_data_t);
  lsd = lsd_create_p("lives_test_t", tt, sizeof(lives_test_t), &tt->lsd);
  lsd_add_special_field((lsd_struct_def_t *)lsd, "strg", LSD_FIELD_CHARPTR,
                        &tt->strg, 0, tt, NULL);
  lives_free(tt);

  lsd_struct_create(lsd);

  THREADVAR(timerinfo) = lives_get_current_ticks();
  for (int i = 0; i < 1000; i++) {
    tt = lsd_struct_create(lsd);
    tt->strg = strdup("a string to be copied !");

    /* g_print("done\n"); */
    /* g_print("fields: struct ^%p lsd: %s %p, id %08lX uid: %08lX self %p  type %s " */
    /* 	    "top %p len %lu  spcl %p  user_data %p \n", tt, tt->strg, tt->lsd, */
    /* 	    tt->lsd->identifier, */
    /* 	    tt->lsd->unique_id, tt->lsd->self_fields, tt->lsd->struct_type, tt->lsd->top, */
    /* 	    tt->lsd->struct_size, tt->lsd->special_fields, tt->lsd->user_data); */

    //g_print("copy struct 1\n");
    tt2 = lsd_struct_copy(tt->lsd);

    /* g_print("done\n"); */
    /* g_print("fields: struct ^%p lsd: %s, %p id %08lX uid: %08lX self %p  type %s " */
    /* "top %p len %lu  spcl %p  user_data %p \n", tt2, tt2->strg, tt2->lsd, tt2->lsd->identifier, */
    /* tt2->lsd->unique_id, tt2->lsd->self_fields, tt2->lsd->struct_type, tt2->lsd->top, */
    /* tt2->lsd->struct_size, tt2->lsd->special_fields, tt2->lsd->user_data); */

    lsd_struct_free(tt->lsd);
    lsd_struct_free(tt2->lsd);
  }
  show_timer_info();
}

//#endif

LIVES_LOCAL_INLINE void show_quadstate(weed_plant_t *p) {
  // do nothing
}

static inline void  werr_expl(weed_error_t werr) {
  char *msg = weed_error_to_literal(werr);
  fprintf(stderr, "(%s)\n", msg);
  lives_free(msg);
}


void list_leaves(weed_plant_t *plant) {
  weed_size_t nleaves;
  char **keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant %p has %d leaves\n", plant, nleaves);
  if (!keys) {
    fprintf(stderr, "keys are NULL\n");
    return;
  } else {
    if (!nleaves) {
      fprintf(stderr, "plant has no leaves\n");
      return;
    } else {
      for (int n = 0;  keys[n]; n++) {
        fprintf(stderr, "key %d is %s\n", n, keys[n]);
        free(keys[n]);
      }
      free(keys);
      fprintf(stderr, "\n");
      show_quadstate(plant);
    }
    fprintf(stderr, "\n");
  }
}

#define CONCYC 100000
#define MAXLVS 1000
#define NCTHRD 8

static void weed_concurrency_test(weed_plant_t *plant) {
  weed_error_t werr;
  int count = 0;

  for (int i = 0; i < CONCYC; i++) {
    int z;
    int x = fastrand_int(MAXLVS);
    char *key = lives_strdup_printf("%d%d", x * 917, x % 13);
    int act = fastrand_int(10);
    //g_print("ACTis %d\n", act);
    if (act < 2) werr = _weed_leaf_delete(plant, key);
    else if (act < 6) {
      weed_set_int_value(plant, key, x);
      //fprintf(stderr, "set %s to %d\n", key, x);
    } else {
      z = weed_get_int_value(plant, key, &werr);
      if (werr == WEED_SUCCESS) {
        if (z != x) {
          g_print("CF %d and %d\n", z, x);
          abort();
        }
      }
    }
    lives_free(key);
    if (++count == 100) {
      g_print(".");
#if USE_RPMALLOC
      rpmalloc_thread_collect();
#endif
      count = 0;
    }
    lives_microsleep;
  }
}


int run_weed_startup_tests(void) {
  lives_proc_thread_t lpts[NCTHRD];
  weed_plant_t *plant;
  int a, type, ne, st, flags;
  int *intpr;
  char *str;
  int pint[4];//, zint[4];
  weed_error_t werr;
  char **keys;
  void *ptr;;//, *ptr2;
  void *ptra[4];
  char *s[4];
  char *text;
  int n;
  weed_size_t nleaves;

  g_print("Testing libweed functionality:\n\n");

  THREADVAR(timerinfo) = lives_get_current_ticks();

  // run some tests..
  plant = _weed_plant_new(WEED_PLANT_HOST_INFO);
  fprintf(stderr, "plant is %p\n", plant);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  type = weed_get_int_value(plant, WEED_LEAF_TYPE, &werr);
  fprintf(stderr, "type is %d, should be %d err was %d\n", type, WEED_PLANT_HOST_INFO, werr);
  werr_expl(werr);

  if (type != WEED_PLANT_HOST_INFO) {
    abort();
  }

  ne = _weed_leaf_num_elements(plant, WEED_LEAF_TYPE);
  fprintf(stderr, "ne was %d\n", ne);

  if (ne != 1) {
    abort();
  }

  st = _weed_leaf_seed_type(plant, "type");
  fprintf(stderr, "seedtype is %d\n", st);
  if (ne != WEED_SEED_INT) {
    abort();
  }

  flags = _weed_leaf_get_flags(plant, WEED_LEAF_TYPE);
  fprintf(stderr, "flags is %d\n", flags);

  if (ne != WEED_SEED_INT) {
    abort();
  }

  list_leaves(plant);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "check NULL plant\n");
  type = weed_get_int_value(NULL, WEED_LEAF_TYPE, &werr);

  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  ne = _weed_leaf_num_elements(NULL, WEED_LEAF_TYPE);
  werr_expl(werr);

  fprintf(stderr, "ne was %d\n", ne);
  st = _weed_leaf_seed_type(NULL, "type");

  fprintf(stderr, "seedtype is %d\n", st);
  flags = _weed_leaf_get_flags(NULL, WEED_LEAF_TYPE);

  fprintf(stderr, "flags is %d\n", flags);

  list_leaves(NULL);

  show_quadstate(plant);
  fprintf(stderr, "\n");

  /* fprintf(stderr, "clearing flags for type leaf\n"); */
  /* werr = weed_leaf_set_flags(plant, "type", 0); */
  /* fprintf(stderr, "zzztype setflags %d\n", werr); */
  /* werr_expl(werr); */

  fprintf(stderr, "Check NULL key \n");

  type = weed_get_int_value(plant, NULL, &werr);
  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  ne = _weed_leaf_num_elements(plant, NULL);
  fprintf(stderr, "ne was %d\n", ne);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  st = _weed_leaf_seed_type(plant, NULL);
  fprintf(stderr, "seedtype is %d\n", st);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  flags = _weed_leaf_get_flags(plant, NULL);
  fprintf(stderr, "flags is %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "Check zero length key \n");
  type = weed_get_int_value(plant, "", &werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  /// flags nok here

  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  werr_expl(werr);
  ne = _weed_leaf_num_elements(plant, "");

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "ne was %d\n", ne);
  st = _weed_leaf_seed_type(plant, "");

  fprintf(stderr, "seedtype is %d\n", st);
  flags = _weed_leaf_get_flags(plant, "");
  fprintf(stderr, "flags is %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "checking get / set values\n");
  fprintf(stderr, "creating new plant\n");

  weed_plant_t *plant2 = _weed_plant_new(0);
  weed_set_string_value(plant2, "astr", "hello");

  weed_set_voidptr_value(plant2, "vptr", &flags);

  fprintf(stderr, "read x1 %s %p, should be been 'hello' and %p\n",
          weed_get_string_value(plant2, "astr", NULL),
          weed_get_voidptr_value(plant2, "vptr", NULL),
          &flags);

  weed_set_int_value(plant, "Test", 99);
  fprintf(stderr, "Set 'Test' = 99\n");

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "Test", &werr);

  fprintf(stderr, "value 1 read was %d, err was %d\n", a, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  list_leaves(plant);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  weed_set_int_value(plant, "Test", 143);
  a = weed_get_int_value(plant, "Test", &werr);

  fprintf(stderr, "value 2 read was %d,  should have been 143,  err was %d\n", a, werr);
  werr_expl(werr);
  if (a != 143) abort();

  list_leaves(plant);


  weed_set_string_value(plant, "Test2", "abc");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 3 read was %s, should be ábc', err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", NULL);
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 4 read was %s, should be (NULL), err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);
  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  weed_set_string_value(plant, "Test2", "xyzabc");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 3a read was %s, should be 'xyzabc', err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_set_string_value(plant, "Test2", "");
  fprintf(stderr, "value 5 set err was %d\n", werr);
  werr_expl(werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 5 read was %s, should be empty string, err was %d\n", str, werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "xyzabc");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 3b read was %s, should be 'xyzabc', err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_set_string_value(plant, "Test2", "");
  fprintf(stderr, "value 5b set err was %d\n", werr);
  werr_expl(werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 5b read was %s, should be empty string, err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", NULL);

  werr = _weed_leaf_get(plant, "Test2", WEED_SEED_STRING, &str);
  if (!str) {
    fprintf(stderr, "read 6 was NULL, good, err wad %d\n", werr);
  } else {
    str = weed_get_string_value(plant, "Test2", &werr);
    fprintf(stderr, "value 6 read was %s, err was %d\n", str, werr);
  }
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;

  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);
  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  weed_set_string_value(plant, "Test3", NULL);
  str = weed_get_string_value(plant, "Test3", &werr);

  fprintf(stderr, "value 7 read was %s, should be (NULL), err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);
  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  weed_set_string_value(plant, NULL, NULL);
  str = weed_get_string_value(NULL, NULL, &werr);

  fprintf(stderr, "value 8 read was %s, err was %d, should be nosuch leaf\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);
  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  pint[0] = 10000000;
  pint[1] = 1;
  pint[2] = 5;
  pint[3] = -199;

  werr = weed_set_int_array(plant, "intarray", 4, pint);
  fprintf(stderr, "int array set, err was %d\n", werr);
  werr_expl(werr);

  intpr = weed_get_int_array(plant, "intarray", &werr);
  if (!intpr) {
    fprintf(stderr, "NULL returned when getting intarray !");
  } else fprintf(stderr, "int array got %d %d %d %d, should be 1000000, 1, 5, -199"
                   "err was %d\n", intpr[0], intpr[1], intpr[2], intpr[3], werr);
  werr_expl(werr);

  intpr = weed_get_int_array(plant, "xintarray", &werr);
  if (!intpr) {
    fprintf(stderr, "NULL returned when getting xintarray !");
  }
  fprintf(stderr, "int array got %p, err was %d, should be nosuch leaf\n", intpr, werr);
  werr_expl(werr);

  intpr = weed_get_int_array(NULL, "xintarray",  &werr);
  if (!intpr) {
    fprintf(stderr, "NULL returned when getting xintarray with NULL plant !");
  } else fprintf(stderr, "int array got %p , err was %d, NULL plant\n", intpr, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "\n\nflag tests\n");

  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (0) are %d, should be IMMUTABLE - %d\n", flags,
          WEED_FLAG_IMMUTABLE);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d %d\n", a, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  type = 99;
  werr = _weed_leaf_set(plant, "type", WEED_SEED_INT, 1, &type);
  fprintf(stderr, "set type returned %d, should de WEED_ERROR_IMMUTABLE\n", werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d, should not have changed err was  %d\n", a, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (1) are %d, should still be immutable %d\n", flags,
          WEED_FLAG_IMMUTABLE);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  werr = _weed_leaf_set_flags(plant, "type", 0);
  fprintf(stderr, "clearing flags for type leaf - err was  %d\n", werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (1a) are %d, should be 0\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  type = 123;
  werr = _weed_leaf_set(plant, "type", WEED_SEED_INT, 1, &type);
  fprintf(stderr, "set type returned %d, should now be WEED_SUCCESS\n", werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d, should be 123, err was  %d\n", a, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (2) are %d, should still be 0\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  werr = _weed_leaf_set_flags(plant, "type", WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "type set flags to IMMUTABLE again - error was  %d\n", werr);
  werr_expl(werr);

  type = 200;
  werr = _weed_leaf_set(plant, "type", WEED_SEED_INT, 1, &type);
  fprintf(stderr, "set type returned %d, should be WEED_ERROR_IMMUTABLE\n", werr);
  werr_expl(werr);

  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (3) are %d, should be immutable\n", flags);

  flags = _weed_leaf_get_flags(plant, "Test2");
  fprintf(stderr, "test getflags %d\n", flags);

  weed_set_string_value(plant, "Test2", "abcde");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 9 read was %s, should be 'abcde'err was %d\n", str, werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "888888");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 10 read was %s, should be '888888', err was %d\n", str, werr);
  werr_expl(werr);

  _weed_leaf_set_flags(plant, "Test2", WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "set leaf Test2 immutable, returned %d\n", werr);
  werr_expl(werr);

  text = strdup("hello");

  werr = _weed_leaf_set(plant, "Test2", WEED_SEED_STRING, 1, &text);
  fprintf(stderr, "setting value returned %d\n", werr);
  werr_expl(werr);
  free(text);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 11 read was %s, should still be '888888'and not 'hello',"
          "err was %d\n", str, werr);
  werr_expl(werr);

  werr = _weed_leaf_set_flags(plant, "Test2", 0);
  fprintf(stderr, "set leaf Test2 mutable, returned %d\n", werr);
  werr_expl(werr);

  werr = weed_set_string_value(plant, "Test2", "OK");
  fprintf(stderr, "set leaf Test2 value to 'OK', returned %d\n", werr);
  werr_expl(werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 12 read was %s, should be ÓK', err was %d\n", str, werr);
  werr_expl(werr);

  werr = weed_set_string_value(plant, "string1", "abccc");
  fprintf(stderr, "set string1 to 'abccc', err was %d\n", werr);
  werr_expl(werr);
  werr = weed_set_string_value(plant, "string2", "xyyyyzz");
  fprintf(stderr, "set string2 to 'xyyyyzz', err was %d\n", werr);
  werr_expl(werr);
  weed_set_string_value(plant, "string3", "11111  11111");
  fprintf(stderr, "set string3 to '11111  11111', err was %d\n", werr);
  werr_expl(werr);

  werr = weed_set_string_value(plant, "string2", "xxxxx");
  fprintf(stderr, "set string2 to 'xxxxx', err was %d\n", werr);
  werr_expl(werr);
  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "value 13 read was %s, should be 'xxxxx', err was %d\n", str, werr);
  werr_expl(werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  ////
  fprintf(stderr, "deleting string1\n");
  werr = _weed_leaf_delete(plant, "string1");
  fprintf(stderr, "returned %d\n", werr);
  werr_expl(werr);

  if (werr) {
    fprintf(stderr, "should not have returned an error, aborting !\n");
    werr_expl(werr);
    abort();
  }

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  str = weed_get_string_value(plant, "string1", &werr);
  fprintf(stderr, "value for deleted leaf returned %s, err was %d\n", str, werr);
  werr_expl(werr);

  flags = _weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags for string 2 returned %d\n", flags);
  _weed_leaf_set_flags(plant, "string2", WEED_FLAG_UNDELETABLE);
  fprintf(stderr, "set flags to undeleteable\n");
  flags = _weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d, should be %d\n", flags,
          WEED_FLAG_UNDELETABLE);

  werr = _weed_leaf_delete(plant, "string2");

  fprintf(stderr, "del aaa leaf returned %d, should be %d\n", werr,
          WEED_ERROR_UNDELETABLE);
  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "value of leaf returned %s, should be 'xxxxx', err was  %d\n", str, werr);
  werr_expl(werr);

  fprintf(stderr, "clearing flags\n");
  weed_leaf_set_undeletable(plant, "string2", WEED_FALSE);
  //weed_leaf_set_flags(plant, "string2", 0);
  flags = _weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "set flags returned %d\n", flags);
  werr = _weed_leaf_delete(plant, "string2");
  fprintf(stderr, "deleting string2 returned %d, should be WEED_SUCCESS\n", werr);
  werr_expl(werr);

  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "del xxx leaf val returned %s %d, should be %d\n", str, werr,
          WEED_ERROR_NOSUCH_LEAF);
  werr_expl(werr);

  werr = _weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);

  flags = _weed_leaf_get_flags(plant, "Test2");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = _weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  str = weed_get_string_value(plant, "Test2", &werr);
  fprintf(stderr, "Get undel. val %s from undel plant, returned %d\n", str, werr);
  werr_expl(werr);

  werr = weed_set_voidptr_value(plant, "nulldel",  NULL);
  fprintf(stderr, "Xset null void * returned %d\n", werr);
  werr_expl(werr);

  werr = weed_leaf_delete(plant, "nulldel");
  fprintf(stderr, "delete null void * returned %d\n", werr);
  werr_expl(werr);

  werr = weed_leaf_delete(plant, "nulldel");
  fprintf(stderr, "delete already void * returned %d\n", werr);
  werr_expl(werr);

  werr = weed_leaf_delete(plant, "foo");
  fprintf(stderr, "delete non-existent leaf returned %d\n", werr);
  werr_expl(werr);

  werr = weed_set_voidptr_value(plant, "nullptr",  NULL);
  fprintf(stderr, "set null void * returned %d\n", werr);
  werr_expl(werr);

  ptr = weed_get_voidptr_value(plant, "nullptr", &werr);
  fprintf(stderr, "get null vooid * returned (%p) %d\n", ptr, werr);
  werr_expl(werr);

  ptr = weed_get_voidptr_value(plant, "nullptrxx", &werr);
  fprintf(stderr, "get nonexist void * returned (%p) %d\n", ptr, werr);
  werr_expl(werr);

  /// will crash !

  /* werr = weed_leaf_set(plant, "nullbasic", WEED_SEED_VOIDPTR, 0, NULL); */
  /* fprintf(stderr, "set null basic voidptr zero returned %d\n", werr); */

  ptr = weed_get_voidptr_value(plant, "nullbasic", &werr);
  fprintf(stderr, "get null basic voidptr 0 returned (%p) %d\n", ptr, werr);
  werr_expl(werr);

  /// will crash !!

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  /// WILL segfault, programmer error
  /* werr = _weed_leaf_set(plant, "nullbasic", WEED_SEED_VOIDPTR, 1, NULL); */
  /* fprintf(stderr, "set null string returned %d\n", werr); */

  ptr = weed_get_voidptr_value(plant, "nullbasic", &werr);
  fprintf(stderr, "get null string returned (%p) %d\n", ptr, werr);
  werr_expl(werr);

  /* ptr2 = NULL; */
  /* werr = _weed_leaf_set(plant, "indirect", WEED_SEED_VOIDPTR, 1, &ptr2); */
  /* fprintf(stderr, "set null ptr returned %d\n", werr); */

  ptr = weed_get_voidptr_value(plant, "indirect", &werr);
  fprintf(stderr, "get null string returned (%p) %d\n", ptr, werr);
  werr_expl(werr);

  ptra[0] = &werr;
  ptra[1] = &keys;
  ptra[2] = NULL;
  ptra[3] = &ptra[3];

  _weed_leaf_set(plant, "ptrs", WEED_SEED_VOIDPTR, 4, &ptra);
  fprintf(stderr, "set null array elem ptra returned %d\n", werr);
  werr_expl(werr);

  void **ptrb = weed_get_voidptr_array(plant, "ptrs", &werr);
  fprintf(stderr, "get void ** returned (%p %p %p %p) should be %p %p NULL %p  %d\n", ptrb[0], ptrb[1], ptrb[2], ptrb[3], &werr,
          &keys, &ptra[3], werr);
  werr_expl(werr);

  s[0] = "okok";
  s[1] = "1ok2ok";
  s[2] = NULL;
  s[3] = "1ok2ok";

  _weed_leaf_set(plant, "ptrs", WEED_SEED_VOIDPTR, 4, ptrb);
  fprintf(stderr, "set copy back array elem ptra returned %d\n", werr);
  werr_expl(werr);

  ptrb = weed_get_voidptr_array(plant, "ptrs", &werr);
  fprintf(stderr, "get void ** returned (%p %p %p %p) should be %p %p NULL %p  %d\n", ptrb[0], ptrb[1], ptrb[2], ptrb[3], &werr,
          &keys, &ptra[3], werr);

  werr_expl(werr);

  _weed_leaf_set(plant, "strings", WEED_SEED_STRING, 4, &s);
  fprintf(stderr, "set char ** %d\n", werr);
  werr_expl(werr);

  char **stng2;
  stng2 = weed_get_string_array(plant, "strings", &werr);
  fprintf(stderr, "get char ** returned (%s %s %s %s) should okok 1ok2ok NULL 1ok2ok %d\n", stng2[0], stng2[1], stng2[2],
          stng2[3], werr);
  werr_expl(werr);

  werr = weed_leaf_set(plant, "arrawn", WEED_SEED_VOIDPTR, 4, ptra);
  fprintf(stderr, "set null array returned %d\n", werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  flags = _weed_leaf_get_flags(plant, "Test2");
  fprintf(stderr, "get flags for Test2returned %d\n", flags);

  werr = _weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);
  flags = _weed_leaf_get_flags(plant, "string2");

  fprintf(stderr, "get flags for Test2returned %d\n", flags);

  werr = _weed_leaf_set_flags(plant, "Test2", 0);
  flags = _weed_leaf_get_flags(plant, "string2");

  fprintf(stderr, "get flags for Test2returned %d\n", flags);

  werr = _weed_leaf_set_flags(plant, "type", WEED_FLAG_UNDELETABLE | WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "wlsf for type returned %d\n", werr);

  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "get type flags returned %d\n", flags);

  werr = _weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);
  werr_expl(werr);

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = _weed_leaf_set_flags(plant, "type", WEED_FLAG_IMMUTABLE);
  flags = _weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = _weed_leaf_set_flags(plant, "arrawn", WEED_FLAG_UNDELETABLE);
  fprintf(stderr, "set flags returned %d\n", werr);
  werr_expl(werr);

  flags = _weed_leaf_get_flags(plant, "arrawn");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = _weed_leaf_set_flags(plant, "indirect", WEED_FLAG_UNDELETABLE);
  fprintf(stderr, "set flags returned %d\n", werr);
  werr_expl(werr);

  werr = _weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);
  fprintf(stderr, "set flags returned %d\n", werr);
  werr_expl(werr);

  werr = _weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);
  fprintf(stderr, "set flags returned %d\n", werr);
  werr_expl(werr);

  list_leaves(plant);

  werr = _weed_leaf_set_flags(plant, "arrawn", 0);
  fprintf(stderr, "set flags a returned %d\n", werr);
  werr_expl(werr);

  flags = _weed_leaf_get_flags(plant, "arrawn");
  fprintf(stderr, "get flags a returned %d\n", flags);
  werr_expl(werr);

  werr = _weed_leaf_set_flags(plant, "indirect", WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "set flags b returned %d\n", werr);
  werr_expl(werr);

  werr = _weed_leaf_set_flags(plant, "Test2", 0);
  fprintf(stderr, "set flags c returned %d\n", werr);
  werr_expl(werr);

  werr = _weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);
  werr_expl(werr);

  show_timer_info();

#define CONCURRENCY_TST
#ifdef CONCURRENCY_TST
  print_diagnostics(DIAG_MEMORY);

  THREADVAR(timerinfo) = lives_get_current_ticks();
  fprintf(stderr, "test random reads, writes and deletes\n");
  plant = _weed_plant_new(123);

  for (int tt = 0; tt < NCTHRD; tt++) {
    lpts[tt] = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                        weed_concurrency_test, -1, "v", plant);
  }
  for (int tt = 0; tt < NCTHRD; tt++) {
    lives_proc_thread_queue(lpts[tt], LIVES_THRDATTR_NONE);
  }

  for (int tt = 0; tt < NCTHRD; tt++) {
    fprintf(stderr, "joining %d of %d\n", tt, NCTHRD);
    lives_proc_thread_join(lpts[tt]);
    fprintf(stderr, "done\n");
  }

  fprintf(stderr, "tested random reads, writes and deletes\n");
  fprintf(stderr, "%d threads running %d random read/writes/deletes with set of %d leaves\n",
          NCTHRD, CONCYC, MAXLVS);
  show_timer_info();

  fprintf(stderr, "final state is:\n");

  keys = _weed_plant_list_leaves(plant, &nleaves);
  fprintf(stderr, "Plant has %d leaves\n", nleaves);

  /* n = 0; */
  /* for (n = 0; keys[n]; n++) { */
  /*   //fprintf(stderr, "key %d is %s with val %d\n", n, keys[n], weed_get_int_value(plant, keys[n], NULL)); */
  /*   free(keys[n]); */
  /* } */
  /* free(keys); */
  print_diagnostics(DIAG_MEMORY);
  _weed_plant_free(plant);
  print_diagnostics(DIAG_MEMORY);
#endif


  /* #define BPLANT_LEAVES 100000 */
  /* #define CYCLES 1000 */
  /*   g_print("Big plant test: \n"); */
  /*   g_print("adding %d leaves\n", BPLANT_LEAVES); */
  /*   plant = _weed_plant_new(WEED_PLANT_EVENT); */

  /*   if (1) { */
  /*     double time1, time2 = 0., dly = -1., tot1 = 0., totx = 0.; */
  /*     int rnds = 3; */
  /*     char *key; */
  /*     int count = 0, cval = 1000; */
  /*     int mm = CYCLES; */

  /*     THREADVAR(timerinfo) = lives_get_current_ticks(); */
  /*     for (int i = 1; i <= BPLANT_LEAVES; i++) { */
  /*       key = lives_strdup_printf("%d%d", i * 917, i % 13); */
  /*       weed_set_int_value(plant, key, i); */
  /*       lives_free(key); */
  /*       if (++count == cval) { */
  /*         g_print("%d", i); */
  /*         totx += show_timer_info(); */
  /*         count = 0; */
  /*       } */
  /*     } */

  /*     g_print("done in %.2f sec\n", totx); */

  /*     g_print("Find delay  to generate n random keys\n"); */
  /*     show_timer_info(); */
  /*     for (int i = 0; i < mm; i++) { */
  /*       int x = fastrand_int(BPLANT_LEAVES); */
  /*       char *key = lives_strdup_printf("%d%d", x * 917, x % 13); */
  /*       free(key); */
  /*     } */
  /*     dly = show_timer_info(); */
  /*     g_print("Delay is %.2f sec\n", dly); */

  /*     for (int vv = 0; vv < rnds; vv++) { */
  /*       g_print("test %d random reads\n", mm); */
  /*       for (int i = 0; i < mm; i++) { */
  /*         int z; */
  /*         int x = fastrand_int(BPLANT_LEAVES / 100); */
  /*         char *key = lives_strdup_printf("%d%d", x * 917, x % 13); */
  /*         z = weed_get_int_value(plant, key, &werr); */
  /*         if (werr == WEED_SUCCESS) { */
  /*           n++; */
  /*           if (z != x) abort(); */
  /*         } */
  /*         free(key); */
  /*       } */

  /*       time1 = show_timer_info(); */

  /*       g_print("done, subtracting time to make random leaves\n"); */

  /*       time1 -= dly; */
  /*       tot1 += time1; */

  /*       fprintf(stderr, "result for rnd %d is %.2f\n", vv, time1); */

  /*       g_print("test %d last-leaf reads\n", mm); */
  /*       key = lives_strdup_printf("%d%d", 917, 1); */
  /*       for (int zz = 0; zz < mm; zz++) { */
  /*         int z = weed_get_int_value(plant, key, &werr); */
  /*       } */
  /*       free(key); */
  /*       time2 += show_timer_info(); */
  /*     } */
  /*     fprintf(stderr, */
  /*             "avgs over %d rounds of %d cycles, : (nleaves = %d), random reads %.2f (%.2f usec each)  and last leaf reads %.2f, (%.2f usec each) ratio %.3f should be around 50 %%\n", */
  /*             rnds, mm, BPLANT_LEAVES, tot1 / rnds, tot1 / rnds / mm * ONE_MILLION, time2 / rnds, */
  /*             time2 / rnds / mm * ONE_MILLION, tot1 / time2 * 100.); */
  /*   } */

  /*   g_print("freeing big plant\n"); */
  /*   _weed_plant_free(plant); */
  g_print("done\n");
  show_timer_info();


  //g_print

  return 0;
}

#ifdef WEED_STARTUP_TESTS

#define SCALE_FACT 65793. /// (2 ^ 24 - 1) / (2 ^ 8 - 1)

int test_palette_conversions(void) {
  double val, dif, tot = 0., totp = 0., totn = 0., tota = 0., totap = 0., totan = 0., fdif;
  int inval, outval, divg = 0, divbad = 0;
  int pbq = prefs->pb_quality;
  //prefs->pb_quality = PB_QUALITY_LOW;
  //prefs->pb_quality = PB_QUALITY_MED;
  //prefs->pb_quality = PB_QUALITY_HIGH;
  for (int pb = 1; pb < 4; pb++) {
    prefs->pb_quality = pb;
    for (val = 0.; val < 256.; val += .1) {
      inval = val * SCALE_FACT;
      outval = round_special(inval);
      dif = (float)outval - val;
      if (dif > 0.) totap += dif;
      else totan += dif;
      fdif = fabs(dif);
      tota += fdif;
      if (fdif > .5) {
        //g_print("in val was %.6f, stored as %d, returned as %d\n", val, inval, outval);
        divg++;
        tot += fdif;
        if (dif > 0.) totp += dif;
        else totn += fdif;
        if (fdif > 1.) {
          //g_print("in val was %.6f, stored as %d, returned as %d\n", val, inval, outval);
          divbad++;
        }
      }
    }
    g_print("quality %d found %d divergences; total = %f, avg = %f\n", prefs->pb_quality, divg, tot, tot / divg);
    g_print("totp = %f, totn = %f, tota = %f, %f, %f, bad = %d\n", totp, totn, tota, totap, totan, divbad);
    tot = totp = totn = tota = totan = totap = 0;
    divg = divbad = 0;
  }
  prefs->pb_quality = pbq;
  return 0;
}


#ifdef WEED_WIDGETS
void show_widgets_info(void) {
  // list toolkits
  const LiVESList *tklist = lives_toolkits_available();
  LiVESList *list = (LiVESList *)tklist;
  for (; list; list = list->next) {
    int tk = LIVES_POINTER_TO_INT(list->data);
    const char *tkname = widget_toolkit_get_name(tk);
    const LiVESList *klist = widget_toolkit_klasses_list(tk);
    LiVESList *list2 = (LiVESList *)klist;
    g_print("\n - Showing details for toolkit '%s' - \n\n", tkname);
    for (; list2; list2 = list2->next) {
      LiVESList *fnlist, *list3;
      const lives_widget_klass_t *k = (const lives_widget_klass_t *)list2->data;
      int kidx = widget_klass_get_idx(k);
      const char *kname = klass_idx_get_name(kidx);
      const char *krole = klass_role_get_name(tk, widget_klass_get_role(k));
      g_print("Klass type '%s' (%s) provides functions:\n", kname, krole);
      list3 = fnlist = widget_klass_list_funcs(k, TRUE);
      for (; list3; list3 = list3->next) {
        lives_funcdef_t **funcinfo;
        int nfuncs, fnidx = LIVES_POINTER_TO_INT(list3->data);
        const char *fname = widget_functype_get_name(fnidx);
        g_print("%s:", fname);
        funcinfo = get_widget_funcs_for_klass(k, fnidx, &nfuncs);
        if (nfuncs > 1) g_print(" %d alternatives found\n", nfuncs);
        else g_print("\n");
        for (int j = 0; j < nfuncs; j++) {
          g_print("\t%swidget_func_%s(winst, %s",
                  weed_seed_to_ctype(funcinfo[j]->rettype, WEED_TRUE),
                  weed_seed_type_to_short_text(funcinfo[j]->rettype), fname);
          if (!*funcinfo[j]->args_fmt) g_print("void");
          else {
            int pc = 0;
            char c = funcinfo[j]->args_fmt[pc];
            while (c) {
              int n = 0;
              switch (c) {
              case 'i' : n = 1; break;
              case 'd' : n = 2; break;
              case 'b' : n = 3; break;
              case 'I' : n = 4; break;
              case 's' : n = 5; break;
              case 'f' : case 'F' : n = 64; break;
              case 'v' : case 'V': n = 65; break;
              case 'p' : case 'P': n = 66; break;
              default: break;
              }
              g_print(", %s", weed_seed_to_ctype(n, WEED_FALSE));
              c = funcinfo[j]->args_fmt[++pc];
            }
          }
          g_print(");\n");
        }
        lives_free(funcinfo);
        g_print("\n");
      }
      lives_list_free(fnlist);
    }
  }
}
#endif

#endif

void weed_utils_test(void) {
  weed_plant_t *plant1, *plant2;
  void *vp1, *vp2;
  weed_error_t err;
  int iv[4], *iv2;
  plant1 = weed_plant_new(123);
  plant2 = weed_plant_new(123);

  iv[0] = 10;
  iv[1] = 20;
  iv[2] = 80;
  iv[3] = 50;
  weed_set_int_value(plant1, "test", 123);
  weed_set_int_array(plant1, "test", 4, iv);
  iv2 = weed_get_int_array(plant1, "test", 0);
  g_print("valzz %d and %d %d %d\n", iv2[0], iv2[1], iv2[2], iv2[3]);

  vp1 = (void *)0x123;
  fprintf(stderr, "initial val will be %p\n", vp1);
  err = weed_set_voidptr_value(plant1, WEED_LEAF_PIXEL_DATA, vp1);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);
  vp1 = weed_get_voidptr_value(plant1, WEED_LEAF_PIXEL_DATA, &err);
  fprintf(stderr, "initial val read; %p\n", vp1);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  fprintf(stderr, "copy from plant1; %p to plant2: %p\n", plant1, plant2);
  err = lives_leaf_copy(plant2, "voidptr2", plant1, WEED_LEAF_PIXEL_DATA);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  vp2 = weed_get_voidptr_value(plant2, "voidptr2", &err);
  fprintf(stderr, "copy val read; %p\n", vp2);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  vp1 = weed_get_voidptr_value(plant1, WEED_LEAF_PIXEL_DATA, &err);
  fprintf(stderr, "check orig val, val read; %p\n", vp1);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  fprintf(stderr, "dup from plant1; %p to plant2: %p\n", plant1, plant2);
  err = lives_leaf_dup(plant2, plant1, WEED_LEAF_PIXEL_DATA);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  vp2 = weed_get_voidptr_value(plant2, WEED_LEAF_PIXEL_DATA, &err);
  fprintf(stderr, "dup val read; %p\n", vp2);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  err = weed_set_voidptr_value(plant2, WEED_LEAF_PIXEL_DATA, NULL);
  fprintf(stderr, "reset value for %p\n", plant2);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  vp2 = weed_get_voidptr_value(plant2, WEED_LEAF_PIXEL_DATA, &err);
  fprintf(stderr, "reet val read; %p\n", vp2);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  fprintf(stderr, "dup again from plant1; %p to plant2: %p\n", plant1, plant2);
  err = lives_leaf_dup(plant2, plant1, WEED_LEAF_PIXEL_DATA);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  vp2 = weed_get_voidptr_value(plant2, WEED_LEAF_PIXEL_DATA, &err);
  fprintf(stderr, "dup val read; %p\n", vp2);
  fprintf(stderr, "err was %d\n", err);
  werr_expl(err);

  weed_plant_free(plant1);  weed_plant_free(plant2);
}


void do_lsd_tests(void) {
  const lsd_struct_def_t *lsd;
  lives_clip_data_t *fcd1, *fcd2;

  g_print("create test template\n");
  lsd = get_lsd(LIVES_STRUCT_CLIP_DATA_T);

  g_print("lsd1 is %p\n", lsd);

  g_print("test struct from template\n");

  break_me("lsd0");

  fcd1 = lsd_struct_create(lsd);

  g_print("fcd1 is %p\n", fcd1);

  break_me("lsd1");

  fcd2 = (lives_clip_data_t *)struct_from_template(LIVES_STRUCT_CLIP_DATA_T);

  g_print("fcd is %p\n", fcd2);

  break_me("lsd2");

  g_print("check integrity\n");

  g_print("\n\nResult is %lu\n", lsd_check_match(fcd1->lsd, fcd2->lsd));

  g_print("check unref\n");

  lsd_struct_unref(fcd2->lsd);

  break_me("lsd3");

  g_print("check copy\n");

  fcd2 = lsd_struct_copy(fcd1->lsd);

  break_me("lsd4");

  g_print("check integrity\n");

  g_print("\n\nResult is %lu\n", lsd_check_match(fcd1->lsd, fcd2->lsd));

  lsd_struct_unref(fcd1->lsd);

  return;

  g_print("test static\n");


  fcd1 = (lives_clip_data_t *)lives_calloc(1, sizeof(lives_clip_data_t));

  lsd_struct_initialise(fcd2->lsd, fcd1);

  g_print("check integrity\n");

  g_print("\n\nResult is %lu\n", lsd_check_match(fcd1->lsd, fcd2->lsd));

  g_print("check static unref\n");

  lsd_struct_unref(fcd2->lsd);

  lsd_struct_unref(fcd1->lsd);

  lives_free(fcd1);
}


#define show_size(s) fprintf(stderr, "sizeof s is %lu\n", sizeof(s))

void show_struct_sizes(void) {
  show_size(lives_plugin_id_t);
  show_size(thrd_work_t);
  show_size(lives_funcdef_t);
  show_size(lsd_struct_def_t);
  show_size(lsd_special_field_t);
  show_size(lives_clip_data_t);
  show_size(lives_clip_t);
  show_size(struct _lives_thread_data_t);
  show_size(lives_cc_params);
  //show_size(lives_sw_params);
  show_size(lives_sigdata_t);
  g_print("weed data size is %ld\n", weed_get_data_t_size());
  g_print("weed leaf size is %ld\n", weed_get_leaf_t_size());
}

void run_diagnostic(LiVESWidget *mi, const char *testname) {
  if (!lives_strcmp(testname, "libweed")) run_weed_startup_tests();
  if (!lives_strcmp(testname, "structsizes")) show_struct_sizes();
}

/// bonus functions

char *weed_plant_to_header(weed_plant_t *plant, const char *tname) {
  char **leaves = weed_plant_list_leaves(plant, NULL);
  char *hdr, *ar = NULL, *line;

  if (tname)
    hdr  = lives_strdup("typedef struct {");
  else
    hdr = lives_strdup("struct {");

  for (int i = 0; leaves[i]; i++) {
    uint32_t st = weed_leaf_seed_type(plant, leaves[i]);
    weed_size_t ne = weed_leaf_num_elements(plant, leaves[i]);
    const char *tp = weed_seed_to_ctype(st, TRUE);
    if (ne > 1) ar = lives_strdup_printf("[%d]", ne);
    line = lives_strdup_printf("\n  %s%s%s;", tp, leaves[i], ar ? ar : "");
    hdr = lives_concat(hdr, line);
    if (ar) {
      lives_free(ar);
      ar = NULL;
    }
    lives_free(leaves[i]);
  }
  lives_free(leaves);

  if (!tname)
    line = lives_strdup("\n}");
  else
    line = lives_strdup_printf("\n} %s;", tname);
  lives_concat(hdr, line);
  return hdr;
}


/* weed_plant_t *header_to_weed_plant(const char *fname, const char *sruct_type) { */
/*   // we are looking for something like "} structtype;" */
/*   // the work back from there to something like "typedef struct {" */

/*   int bfd = lives_open_buffered_rdonly(fname); */
/*   if (bfd >= 0) { */
/*     char line[512]; */
/*     while (lives_buffered_readline(bfd, line, '\n', 512) > 0) { */
/*       g_print("line is %s\n", line); */
/*     } */
/*   } */
/*   lives_close_buffered(bfd); */
/* } */


/* void bundle_test(void) { */
/*   //lives_contract_t *c = create_contract_instance(OBJ_INTENTION_NONE, NULL); */
/*   bundledef_t bdef; */
/*   lives_obj_t *c = create_bundle(vtrack_bundle, NULL); */
/*   char *buf; */
/*   size_t ts = 0; */
/*   g_print("%s\n\n", bundle_to_header(c, "matroska_vtrack_t")); */
/*   reset_timer_info(); */
/*   for (int i = 0; i < 10000; i++) { */
/*     tinymd5((void *)buf, ts); */
/*   } */
/*   g_print("mini is 0X%016lX\n", x); */
/*   show_timer_info(); */
/*   lives_free(buf); */
/*   g_print("size is %ld\n", ts); */
/* } */


static void pth_testfunc(void *var) {
  g_print("in test pth, got %s\n", (char *)var);
}


void test_procthreads(void) {
  void *testv = lives_malloc(100);
  lives_proc_thread_t pth;
  lives_snprintf((char *)testv, 100, "hi there");
  pth = lives_proc_thread_create(0, pth_testfunc, -1, "V", testv);
  lives_proc_thread_join(pth);
}


/// any diagnostic tests can be placed in this section - the functional will be called early in
// startup. If abort_after is TRUE, then the function will abort() after completing all designatedd testing
//////////////
lives_result_t do_startup_diagnostics(uint64_t tests_to_run) {
  static int testpoint = 0;
  boolean ran_test = FALSE;
  lives_bundle_t *bundle;

  testpoint++;

  if (testpoint == 1) {
    /* if (tests_to_run & TEST_WEED) */
    /*   run_weed_startup_tests(); */

#ifdef TEST_RTM_CODE
    if (tests_to_run & TEST_RTM)
      do_rtm_test();
#endif

    if (tests_to_run & TEST_RNG)
      test_random();

    if (tests_to_run & TEST_LSD)
      lives_struct_test();

    if (tests_to_run & TEST_PAL_CONV)
      test_palette_conversions();

    if (tests_to_run & TEST_BUNDLES)
      init_bundles();

    if (tests_to_run & TEST_WEED_UTILS)
      weed_utils_test();
  }
  if (testpoint == 2) {
    if (tests_to_run & TEST_WEED)
      run_weed_startup_tests();
    if (tests_to_run & TEST_PROCTHRDS)
      test_procthreads();
  }

  if (tests_to_run & ABORT_AFTER
      && (testpoint > 1 || !(tests_to_run & TEST_POINT_2)))
    abort();

  return LIVES_RESULT_SUCCESS;
}

#pragma GCC diagnostic pop

#endif
