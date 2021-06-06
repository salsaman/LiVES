// diagnostics.h
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _DIAGNOSTICS_H
#define _DIAGNOSTICS_H

#include "diagnostics.h"
#include "callbacks.h"

#define STATS_TC (TICKS_PER_SECOND_DBL)
static double inst_fps = 0.;

LIVES_GLOBAL_INLINE double get_inst_fps(void) {
  if (!mainw->lockstats) get_stats_msg(TRUE);
  return inst_fps;
}


char *get_stats_msg(boolean calc_only) {
  double avsync = 1.0;
  static int last_play_sequence = -1;
  static ticks_t last_curr_tc = 0, currticks;
  static ticks_t last_mini_ticks = 0;
  static frames_t last_mm = 0;
  volatile float *load;

  boolean have_avsync = FALSE;
  char *msg, *audmsg = NULL, *bgmsg = NULL, *fgpal = NULL;
  char *tmp, *tmp2;
  char *msg2 = lives_strdup("");

  if (!LIVES_IS_PLAYING) return NULL;
  //currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
  currticks = mainw->clock_ticks;

  if (CURRENT_CLIP_HAS_AUDIO) {
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd && mainw->jackd->in_use &&
        IS_VALID_CLIP(mainw->jackd->playing_file) && mainw->files[mainw->jackd->playing_file]->arate != 0) {
      avsync = (double)mainw->jackd->seek_pos
               / (double)mainw->files[mainw->jackd->playing_file]->arate / 4.; //lives_pulse_get_pos(mainw->jackd);
      avsync -= ((double)mainw->files[mainw->jackd->playing_file]->frameno - 1.) / mainw->files[mainw->jackd->playing_file]->fps
                + (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL;
      have_avsync = TRUE;
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed && mainw->pulsed->in_use &&
        IS_VALID_CLIP(mainw->pulsed->playing_file) && mainw->files[mainw->pulsed->playing_file]->arate != 0) {
      avsync = (double)mainw->pulsed->seek_pos
               / (double)mainw->files[mainw->pulsed->playing_file]->arate / 4.; //lives_pulse_get_pos(mainw->pulsed);
      avsync -= ((double)mainw->files[mainw->pulsed->playing_file]->frameno - 1.) / mainw->files[mainw->pulsed->playing_file]->fps
                + (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL;
      have_avsync = TRUE;
    }
#endif
  }
  //currticks = lives_get_current_ticks();

  if (mainw->play_sequence > last_play_sequence) {
    last_curr_tc = currticks;
    last_play_sequence = mainw->play_sequence;
    inst_fps = cfile->pb_fps;
    return NULL;
  }

  if (currticks > last_curr_tc + STATS_TC) {
    if (mainw->fps_mini_ticks == last_mini_ticks) {
      inst_fps = (double)(mainw->fps_mini_measure - last_mm
                          + (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL / cfile->pb_fps)
                 / ((double)(currticks - last_curr_tc) / TICKS_PER_SECOND_DBL);
      mainw->inst_fps = inst_fps;
    }
    last_curr_tc = currticks;
    last_mini_ticks = mainw->fps_mini_ticks;
    last_mm = mainw->fps_mini_measure;
  }

  if (calc_only) return NULL;
  load = get_core_loadvar(0);

  if (!prefs->vj_mode) {
    if (have_avsync) {
      audmsg = lives_strdup_printf(_("Audio is %s video by %.4f secs.\n"),
                                   tmp = lives_strdup(avsync >= 0. ? _("ahead of") : _("behind")), fabs(avsync));
      lives_free(tmp);
    } else {
      if (prefs->audio_src == AUDIO_SRC_INT) audmsg = (_("Clip has no audio.\n"));
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

    msg = lives_strdup_printf(_("%sFrame %d / %d, fps %.3f (target: %.3f)\n"
                                "CPU load %.2f %% : Disk load: %f\n"
                                "Effort: %d / %d, quality: %d, %s (%s)\n%s\n"
                                "Fg clip: %d X %d, palette: %s\n%s\n%s"),
                              audmsg ? audmsg : "",
                              mainw->actual_frame, cfile->frames,
                              inst_fps * sig(cfile->pb_fps), cfile->pb_fps,
                              *load, mainw->disk_pressure,
                              mainw->effort, EFFORT_RANGE_MAX,
                              prefs->pb_quality,
                              tmp = lives_strdup(prefs->pb_quality == 1 ? _("Low")
                                    : prefs->pb_quality == 2 ? _("Med") : _("High")),
                              tmp2 = lives_strdup(prefs->pbq_adaptive ? _("adaptive") : _("fixed")),
                              get_cache_stats(),
                              cfile->hsize, cfile->vsize,
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
                              mainw->actual_frame, cfile->frames,
                              inst_fps * sig(cfile->pb_fps), cfile->pb_fps,
                              *load, mainw->effort, EFFORT_RANGE_MAX,
                              tmp = lives_strdup(prefs->pb_quality == 1 ? _("Low")
                                    : prefs->pb_quality == 2 ? _("Med") : _("High")),
                              bgmsg ? bgmsg : "");
    lives_freep((void **)&bgmsg);
    lives_freep((void **)&tmp);
  }
  return msg;
}


#ifdef WEED_STARTUP_TESTS

#define NITERS 1024
#define DTHRESH 8
#define PMISS 0.99609375
void check_random(void) {
  int counter[64];
  int last[64];
  int buckets[64][4];
  int dist[8][256];
  int bval, dval;
  double prob;
  register int d, x;
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
  fprintf(stderr, "Checking statistical probabilities\n");
  for (x = 0; x < NITERS; x++) {
    fprintf(stderr, "%d %.4f  ", x, 1. - prob);
    prob *= prob;
  }
  fprintf(stderr, "\n");


  /// results:
  for (d = 0; d < 64; d++) {
    fprintf(stderr, "digit %d: score %d (%.2f%% 1s)\n", d, counter[d],
            ((double)counter[d] + (double)NITERS) / (double)NITERS * 50.);
    fprintf(stderr, "buckets:  ");
    for (x = 0; x < 4; x++) fprintf(stderr, "[%d]: %d    ", x, buckets[d][x]);
    fprintf(stderr, "\n");
  }
  for (d = 0; d < 8; d++) {
    fprintf(stderr, "segment %d:  ", d);
    for (x = 0; x < 256; x++) {
      dval = dist[d][x];
      if (dval >= DTHRESH) fprintf(stderr, "val %d / %d hit %d times  ", d, x, dist[d][x]);
    }
    fprintf(stderr, "\n");
  }
}


ticks_t timerinfo;

static double show_timer_info(void) {
  ticks_t xtimerinfo = lives_get_current_ticks();
  double timesecs;
  g_print("\n\nTest completed in %.4f seconds\n\n",
          (timesecs = ((double)xtimerinfo - (double)timerinfo) / TICKS_PER_SECOND_DBL));
  timerinfo = xtimerinfo;
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
  timerinfo = lives_get_current_ticks();
  for (i = 0; i < nr; i++) {
    str = randstrg(fastrand_int(20));
    val = fast_hash(str) / 7;
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

#include "lsd.h"

typedef struct {
  lives_struct_def_t lsd;
  char buff[100000];
  char *strg;
  uint64_t num0, num1;
  void *p;
  char **strgs;
} lives_test_t;


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
    timerinfo = lives_get_current_ticks();
    for (i = 0; i < nruns; i++) {
      sz = lives_strcmp(strg, strg2);
      //sz += lives_strlen(strg2);
    }
    show_timer_info();
    sz++;
    fprintf(stderr, "test %d runs with strlen()", nruns);
    timerinfo = lives_get_current_ticks();
    for (i = 0; i < nruns; i++) {
      sz = strcmp(strg, strg2);
      //sz += strlen(strg2);
    }
    show_timer_info();
    sz++;
  }
  lives_free(strg2);
}


void lives_struct_test(void) {
  const lives_struct_def_t *lsd;

  lives_test_t *tt = (lives_test_t *)lives_calloc(1, sizeof(lives_test_t)), *tt2;

  lsd = lsd_create("lives_test_t", sizeof(lives_test_t), "strgs", 1);

  lsd->special_fields[0] = make_special_field(LIVES_FIELD_CHARPTR, tt,
                           &tt->strg, "strg", 0,
                           NULL, NULL, NULL);
  lives_struct_init(lsd, tt, &tt->lsd);
  lives_free(tt);

  timerinfo = lives_get_current_ticks();
  for (int i = 0; i < 1000000; i++) {
    tt = lives_struct_create(lsd);
    tt->strg = strdup("a string to be copied !");

    /* //g_print("done\n"); */
    /* //g_print("fields: struct ^%p lsd: %s %p, id %08lX uid: %08lX self %p  type %s " */
    /* 	    "top %p len %lu  last field %s  spcl %p  user_data %p \n", tt, tt->strg, &tt->lsd, */
    /* 	    tt->lsd.identifier, */
    /* 	    tt->lsd.unique_id, tt->lsd.self_fields, tt->lsd.structtype, tt->lsd.top, */
    /* 	    tt->lsd.structsize, tt->lsd.last_field, tt->lsd.special_fields, tt->lsd.user_data); */

    //g_print("copy struct 1\n");
    tt2 = lives_struct_copy(&tt->lsd);
    ///
    //g_print("done\n");
    //g_print("fields: struct ^%p lsd: %s, %p id %08lX uid: %08lX self %p  type %s "
    //"top %p len %lu  last field %s  spcl %p  user_data %p \n", tt2, tt2->strg, &tt2->lsd, tt2->lsd.identifier,
    //tt2->lsd.unique_id, tt2->lsd.self_fields, tt2->lsd.structtype, tt2->lsd.top,
    //tt2->lsd.structsize, tt2->lsd.last_field, tt2->lsd.special_fields, tt2->lsd.user_data);

    lives_struct_free(&tt->lsd);
    lives_struct_free(&tt2->lsd);
  }
  show_timer_info();
}

LIVES_LOCAL_INLINE void show_quadstate(weed_plant_t *p) {
  // do nothing
}

int run_weed_startup_tests(void) {
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
  int n;
  weed_size_t nleaves;

  g_print("Testing libweed functionality:\n\n");

  timerinfo = lives_get_current_ticks();

  // run some tests..
  plant = weed_plant_new(WEED_PLANT_HOST_INFO);
  fprintf(stderr, "plant is %p\n", plant);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  type = weed_get_int_value(plant, WEED_LEAF_TYPE, &werr);
  fprintf(stderr, "type is %d, err was %d\n", type, werr);

  ne = weed_leaf_num_elements(plant, WEED_LEAF_TYPE);
  fprintf(stderr, "ne was %d\n", ne);

  st = weed_leaf_seed_type(plant, "type");
  fprintf(stderr, "seedtype is %d\n", st);

  flags = weed_leaf_get_flags(plant, WEED_LEAF_TYPE);
  fprintf(stderr, "flags is %d\n", flags);

  /// flags ok here

  keys = weed_plant_list_leaves(plant, &nleaves);
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


  fprintf(stderr, "check NULL plant\n");
  type = weed_get_int_value(NULL, WEED_LEAF_TYPE, &werr);

  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  ne = weed_leaf_num_elements(NULL, WEED_LEAF_TYPE);

  fprintf(stderr, "ne was %d\n", ne);
  st = weed_leaf_seed_type(NULL, "type");

  fprintf(stderr, "seedtype is %d\n", st);
  flags = weed_leaf_get_flags(NULL, WEED_LEAF_TYPE);

  fprintf(stderr, "flags is %d\n", flags);

  keys = weed_plant_list_leaves(NULL, NULL);
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

  // flags ok here


  fprintf(stderr, "zzztype setfff\n");
  werr = weed_leaf_set_flags(plant, "type", 0);
  fprintf(stderr, "zzztype setflags %d\n", werr);


  fprintf(stderr, "Check NULL key \n");

  type = weed_get_int_value(plant, NULL, &werr);
  fprintf(stderr, "type is %d, err was %d\n", type, werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  /// flags nok

  ne = weed_leaf_num_elements(plant, NULL);
  fprintf(stderr, "ne was %d\n", ne);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");


  /// flags nok

  st = weed_leaf_seed_type(plant, NULL);
  fprintf(stderr, "seedtype is %d\n", st);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");


  flags = weed_leaf_get_flags(plant, NULL);
  fprintf(stderr, "flags is %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "Check zero key \n");
  type = weed_get_int_value(plant, "", &werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  /// flags nok here

  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  ne = weed_leaf_num_elements(plant, "");

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "ne was %d\n", ne);
  st = weed_leaf_seed_type(plant, "");

  fprintf(stderr, "seedtype is %d\n", st);
  flags = weed_leaf_get_flags(plant, "");
  fprintf(stderr, "flags is %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "checking get / set values\n");

  weed_plant_t *plant2 = weed_plant_new(0);
  weed_set_string_value(plant2, "astr", "hello");

  weed_set_voidptr_value(plant2, "vptr", &flags);

  fprintf(stderr, "read x1 %s %p\n", weed_get_string_value(plant2, "astr", NULL), weed_get_voidptr_value(plant2, "vptr", NULL));

  weed_set_int_value(plant, "Test", 99);
  fprintf(stderr, "Set 'Test' = 99\n");

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "Test", &werr);

  fprintf(stderr, "value 1 read was %d, err was %d\n", a, werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  keys = weed_plant_list_leaves(plant, &nleaves);
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

  weed_set_int_value(plant, "Test", 143);
  a = weed_get_int_value(plant, "Test", &werr);

  fprintf(stderr, "value 2 read was %d, err was %d\n", a, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
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

  weed_set_string_value(plant, "Test2", "abc");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 3 read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", NULL);
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 4 read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
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

  fprintf(stderr, "value 3a read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);


  werr = weed_set_string_value(plant, "Test2", "");
  fprintf(stderr, "value 5 set err was %d\n", werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 5 read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);


  weed_set_string_value(plant, "Test2", "xyzabc");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 3b read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);


  werr = weed_set_string_value(plant, "Test2", "");
  fprintf(stderr, "value 5b set err was %d\n", werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 5b read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", NULL);

  werr = weed_leaf_get(plant, "Test2", WEED_SEED_STRING, &str);
  if (!str) {
    fprintf(stderr, "red 6 was NULL\n");
  } else {
    str = weed_get_string_value(plant, "Test2", &werr);
    fprintf(stderr, "value 6 read was %s, err was %d\n", str, werr);
  }
  keys = weed_plant_list_leaves(plant, &nleaves);
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

  fprintf(stderr, "value 7 read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
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

  fprintf(stderr, "value 8 read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
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

  intpr = weed_get_int_array(plant, "intarray", &werr);
  fprintf(stderr, "int array got %d %d %d %d , err was %d\n", intpr[0], intpr[1], intpr[2], intpr[3], werr);

  intpr = weed_get_int_array(plant, "xintarray", &werr);
  fprintf(stderr, "int array got %p, err was %d\n", intpr, werr);

  intpr = weed_get_int_array(NULL, "xintarray",  &werr);
  fprintf(stderr, "int array got %p , err was %d\n", intpr, werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  fprintf(stderr, "\n\nflag tests\n");

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (0) are %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d %d\n", a, werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  werr = weed_set_int_value(plant, "type", 99);
  fprintf(stderr, "set type returned %d\n", werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d %d\n", a, werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (1) are %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  werr = weed_leaf_set_flags(plant, "type", 0);
  fprintf(stderr, "type setflags %d\n", werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (1a) are %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");




  werr = weed_set_int_value(plant, "type", 123);
  fprintf(stderr, "set type returned %d\n", werr);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");




  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d %d\n", a, werr);


  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");



  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (2) are %d\n", flags);

  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");
  /// hang....


  werr = weed_leaf_set_flags(plant, "type", WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "type setflags %d\n", werr);

  werr = weed_set_int_value(plant, "type", 200);
  fprintf(stderr, "set type returned %d\n", werr);

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags (3) are %d\n", flags);

  flags = weed_leaf_get_flags(plant, "Test2");
  fprintf(stderr, "test getflags %d\n", flags);

  weed_set_string_value(plant, "Test2", "abcde");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 9 read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "888888");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 10 read was %s, err was %d\n", str, werr);

  weed_leaf_set_flags(plant, "Test2", WEED_FLAG_IMMUTABLE);

  werr = weed_set_string_value(plant, "Test2", "hello");
  fprintf(stderr, "set immutable returned %d\n", werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 11 read was %s, err was %d\n", str, werr);

  weed_leaf_set_flags(plant, "Test2", 0);

  weed_set_string_value(plant, "Test2", "OK");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value 12 read was %s, err was %d\n", str, werr);

  weed_set_string_value(plant, "string1", "abccc");
  weed_set_string_value(plant, "string2", "xyyyyzz");
  weed_set_string_value(plant, "string3", "11111  11111");

  werr = weed_set_string_value(plant, "string2", "xxxxx");
  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "value 13 read was %s, err was %d\n", str, werr);


  fprintf(stderr, "\n");
  show_quadstate(plant);
  fprintf(stderr, "\n");


  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  ////
  werr = weed_leaf_delete(plant, "string1");
  fprintf(stderr, "del leaf returned %d\n", werr);

  if (werr) abort();

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  str = weed_get_string_value(plant, "string1", &werr);
  fprintf(stderr, "del leaf returned %s %d\n", str, werr);

  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);
  weed_leaf_set_flags(plant, "string2", WEED_FLAG_UNDELETABLE);
  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_delete(plant, "string2");
  fprintf(stderr, "del aaa leaf returned %d\n", werr);
  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "del zzz leaf returned %s %d\n", str, werr);


  weed_leaf_set_flags(plant, "string2", 0);
  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "set flags returned %d\n", flags);
  werr = weed_leaf_delete(plant, "string2");
  fprintf(stderr, "del yyy leaf returned %d\n", werr);

  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "del xxx leaf val returned %s %d\n", str, werr);



  werr = weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);

  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  /// will crash

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);



  werr = weed_set_voidptr_value(plant, "nullptr",  NULL);
  fprintf(stderr, "set null void * returned %d\n", werr);

  ptr = weed_get_voidptr_value(plant, "nullptr", &werr);
  fprintf(stderr, "get null vooid * returned (%p) %d\n", ptr, werr);

  ptr = weed_get_voidptr_value(plant, "nullptrxx", &werr);
  fprintf(stderr, "get nonexist void * returned (%p) %d\n", ptr, werr);

  /// will crash !

  /* werr = weed_leaf_set(plant, "nullbasic", WEED_SEED_VOIDPTR, 0, NULL); */
  /* fprintf(stderr, "set null basic voidptr zero returned %d\n", werr); */

  ptr = weed_get_voidptr_value(plant, "nullbasic", &werr);
  fprintf(stderr, "get null basic voidptr 0 returned (%p) %d\n", ptr, werr);

  /// will crash !!

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  /// WILL segfault, programmer error
  /* werr = weed_leaf_set(plant, "nullbasic", WEED_SEED_VOIDPTR, 1, NULL); */
  /* fprintf(stderr, "set null string returned %d\n", werr); */

  ptr = weed_get_voidptr_value(plant, "nullbasic", &werr);
  fprintf(stderr, "get null string returned (%p) %d\n", ptr, werr);

  /* ptr2 = NULL; */
  /* werr = weed_leaf_set(plant, "indirect", WEED_SEED_VOIDPTR, 1, &ptr2); */
  /* fprintf(stderr, "set null ptr returned %d\n", werr); */

  ptr = weed_get_voidptr_value(plant, "indirect", &werr);
  fprintf(stderr, "get null string returned (%p) %d\n", ptr, werr);

  ptra[0] = &werr;
  ptra[1] = &keys;
  ptra[2] = NULL;
  ptra[3] = &ptra[3];

  weed_leaf_set(plant, "ptrs", WEED_SEED_VOIDPTR, 4, &ptra);
  fprintf(stderr, "set null array elem ptra returned %d\n", werr);

  void **ptrb = weed_get_voidptr_array(plant, "ptrs", &werr);
  fprintf(stderr, "get void ** returned (%p %p %p %p) %d\n", ptrb[0], ptrb[1], ptrb[2], ptrb[3], werr);

  s[0] = "okok";
  s[1] = "1ok2ok";
  s[2] = NULL;
  s[3] = "1ok2ok";

  weed_leaf_set(plant, "ptrs", WEED_SEED_VOIDPTR, 4, &ptra);
  fprintf(stderr, "set null array elem ptra returned %d\n", werr);

  ptrb = weed_get_voidptr_array(plant, "ptrs", &werr);
  fprintf(stderr, "get void ** returned (%p %p %p %p) %d\n", ptrb[0], ptrb[1], ptrb[2], ptrb[3], werr);

  weed_leaf_set(plant, "strings", WEED_SEED_STRING, 4, &s);
  fprintf(stderr, "set char ** %d\n", werr);

  char **stng2;
  stng2 = weed_get_string_array(plant, "strings", &werr);
  fprintf(stderr, "get char ** returned (%s %s %s %s) %d\n", stng2[0], stng2[1], stng2[2], stng2[3], werr);

  werr = weed_leaf_set(plant, "arrawn", WEED_SEED_VOIDPTR, 4, ptra);
  fprintf(stderr, "set null array returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  flags = weed_leaf_get_flags(plant, "Test2");
  fprintf(stderr, "get flags for Test2returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);
  flags = weed_leaf_get_flags(plant, "string2");

  fprintf(stderr, "get flags for Test2returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "Test2", 0);
  flags = weed_leaf_get_flags(plant, "string2");

  fprintf(stderr, "get flags for Test2returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "type", WEED_FLAG_UNDELETABLE | WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "wlsf for type returned %d\n", werr);



  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "get type flags returned %d\n", flags);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_leaf_set_flags(plant, "type", WEED_FLAG_IMMUTABLE);
  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "arrawn", WEED_FLAG_UNDELETABLE);
  fprintf(stderr, "set flags returned %d\n", werr);

  flags = weed_leaf_get_flags(plant, "arrawn");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "indirect", WEED_FLAG_UNDELETABLE);
  werr = weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_leaf_set_flags(plant, "arrawn", 0);
  fprintf(stderr, "set flags returned %d\n", werr);
  flags = weed_leaf_get_flags(plant, "arrawn");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "indirect", WEED_FLAG_IMMUTABLE);
  werr = weed_leaf_set_flags(plant, "Test2", 0);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  /// will crash on freed plant

  /* keys = weed_plant_list_leaves(plant, &nleaves); */
  /* n = 0; */
  /* while (keys[n] != NULL) { */
  /*   fprintf(stderr, "key %d is %s\n", n, keys[n]); */
  /*   free(keys[n]); */
  /*   n++; */
  /* } */
  /* free(keys); */

  show_timer_info();

#define BPLANT_LEAVES 10000
  g_print("Big plant test: \n");
  g_print("adding %d leaves\n", BPLANT_LEAVES);
  plant = weed_plant_new(WEED_PLANT_EVENT);
  for (int i = 1; i <= BPLANT_LEAVES; i++) {
    int num = fastrand() >> 32;
    int r = i * 1873;
    char *key = lives_strdup_printf("%dln%dum%d", r, r % 3, r % 7);
    weed_set_int_value(plant, key, i);
    free(key);
  }
  int z;
  g_print("done\n");
  /* g_print("test %d random reads\n", BPLANT_LEAVES * 10); */
  n = 0;
  double fhit, phit, time1, time2;
  for (int i = 0; i < BPLANT_LEAVES * 10; i++) {
    int x = fastrand_int(20000);
    int r = x * 1873;
    char *key = lives_strdup_printf("%dln%dum%d", r, r % 3, r % 7);
    z = weed_get_int_value(plant, key, &werr);
    if (werr == WEED_SUCCESS) {
      n++;
      if (z != x) abort();
    }
    free(key);
  }
  g_print("done, hit percentage was %.2f\n", (phit = (double)n / (double)BPLANT_LEAVES * 10.));
  time1 = show_timer_info();

  fhit = phit / 100.;

  int dist = 1;
  g_print("test %d last-leaf reads\n", BPLANT_LEAVES * 10);
  n = 0;
  int mm = BPLANT_LEAVES * 2;
  do {
    for (int i = 0; i < 0 * BPLANT_LEAVES * 5; i++) {
      int i = mm;
      int r = i * 1873;
      char *key = lives_strdup_printf("%dln%dum%d", r, r % 3, r % 7);
      z = weed_get_int_value(plant, key, &werr);
      if (werr == WEED_SUCCESS) n++;
      free(key);
    }
    time2 = show_timer_info();
    mm = 1;
    for (int i = 0; i < BPLANT_LEAVES * 10; i++) {
      int i = mm;
      int r = i * 1873;
      char *key = lives_strdup_printf("%dln%dum%d", r, r % 3, r % 7);
      z = weed_get_int_value(plant, key, &werr);
      if (werr == WEED_SUCCESS) n++;
      free(key);
    }
    //g_print("done, hit percentage was %.2f\n", (double)n / (double)BPLANT_LEAVES * 10.);
    g_print("search for leaf %d\n", BPLANT_LEAVES - mm);
    time2 = show_timer_info();
    mm += dist;
    dist++;
  } while (mm < BPLANT_LEAVES);
  g_print("freeing big plant\n");
  weed_plant_free(plant);
  g_print("done\n");

  // now, fhit * BPLANT_LEAVES * 10 hits in test 1
  // (1. - fhit) * BPLANT_LEAVES * 10 misses in test 1
  // let n = BPLANT_LEAVES * 10

  // from test2 we know that n seeks + n reads = time2
  // from test 1 we know n * (1 - fhit) seeks  + n * fhit reads + n * fhit / 2 seeks took time1
  // since test1 hits were random, the avg should be half the length
  // dividing time1 by fhit and subtracting from time2. we get:
  // n seeks - (n * (1. - fhit) / fhit seeks + n / 2 seeks) = time2 - time1 / fhit
  // div by n: seek (1 - (1 - fhit) / fhit - 1 / 2) = (time2 - time1 / fhit) / n
  // seek = (time2 - time1 / fhit) / n / (1 - (1. - fhit) / fhit - 1 / 2)

  /* double sktm = ((double)time2 - (double)time1 / fhit) / (double)BPLANT_LEAVES / 10. / (1. - (1. - fhit) / fhit - .5); */
  /* fprintf(stderr, "avg seek to end is %f\n", sktm); */

  /* double rdtm = (double)time2 / ((double)BPLANT_LEAVES * 10.) - sktm; */
  /* fprintf(stderr, "avg read time is %f\n", rdtm); */

  /* double res = (1. - fhit) * BPLANT_LEAVES * 10. * sktm + (fhit * BPLANT_LEAVES * 10.) * .5  * sktm + */
  /*   (fhit * BPLANT_LEAVES * 10.) * rdtm; */

  /* fprintf(stderr, "%f = %d seeks + %d / 2 seeks + %d reads = %f\n", time1, (int)((1. - fhit) * BPLANT_LEAVES * 10.), */
  /* 	  (int)(fhit * BPLANT_LEAVES * 10.), (int)(fhit * BPLANT_LEAVES * 10.), res); */

  /* fprintf(stderr, "%f = %d seeks + %d reads = %f\n", time2, BPLANT_LEAVES * 10, */
  /* 	  BPLANT_LEAVES * 10, BPLANT_LEAVES * 10. *(rdtm + sktm)); */

  weed_threadsafe = FALSE;
  plant = weed_plant_new(0);
  if (weed_leaf_set_private_data(plant, WEED_LEAF_TYPE, NULL) == WEED_ERROR_CONCURRENCY) {
    weed_threadsafe = TRUE;
    g_print("libweed built with FULL threadsafety\n");
  } else {
    weed_threadsafe = FALSE;
    g_print("libweed built with PARTIAL threadsafety\n");
  }
  weed_plant_free(plant);

  //g_print

  return 0;
}

#endif

#define SCALE_FACT 65793. /// (2 ^ 24 - 1) / (2 ^ 8 - 1)

int test_palette_conversions(void) {
  double val;
  int inval, outval;
  for (val = 0.; val < 256.; val += .1) {
    inval = val * SCALE_FACT;
    outval = round_special(inval);
    if (fabs((float)outval - val) > .51)
      g_print("in val was %.6f, stored as %d, returned as %d\n", val, inval, outval);
  }
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
        lives_func_info_t **funcinfo;
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

#define show_size(s) fprintf(stderr, "sizeof s is %lu\n", sizeof(s))

void show_struct_sizes(void) {
  show_size(lives_plugin_id_t);
  show_size(thrd_work_t);
  show_size(lives_func_info_t);
  show_size(struct _lives_thread_data_t);
  show_size(thrd_work_t);
  show_size(lives_cc_params);
  show_size(lives_sw_params);
}

#endif
