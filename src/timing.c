// timing.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


#ifdef _GNU_SOURCE
#include <sched.h>
#endif

#include <time.h>
#include <sys/statvfs.h>

#include "support.h"
#include "main.h"

glob_timedata_t *glob_timing = NULL;

#define N_CPU_MEAS 64
#define CPU_MEAS_THRESH 1000000

static cpuloadvals_t *cpu_stats = NULL;

double *get_proc_loads(boolean reset) {
  // get processor load values, and keep a rolling average
  static cpuloadvals_t cpuvals;
  static tab_data_t *cpuloadtab = NULL;
  static ticks_t ltime = 0;
  ticks_t ctime;

  if (!cpuloadtab) {
    cpuvals.loads =
      (double *)lives_calloc(capable->hw.ncpus, sizeof(double));
    cpuvals.avgs =
      (double *)lives_calloc(capable->hw.ncpus, sizeof(double));
    reset = TRUE;
  }

  if (reset && cpuloadtab) cpuloadtab = free_tabdata(cpuloadtab);
  if (!cpuloadtab) cpuloadtab = init_tab_data(capable->hw.ncpus, N_CPU_MEAS);

  ctime = lives_get_session_ticks();
  if (reset || ctime - ltime > CPU_MEAS_THRESH) {
    ltime = ctime;
    if (get_cpu_loads(&cpuvals, capable->hw.ncpus)) {
      if (!cpu_stats) cpu_stats = &cpuvals;
      tabdata_update(cpuloadtab, cpuvals.loads);
      for (int i = capable->hw.ncpus; i--;) {
	cpuvals.avgs[i] = cpuloadtab->avgs[i];
      }
    }
  }
  return cpuvals.avgs;
}


double get_cpu_load(void) {
  double *cpuvars = get_proc_loads(FALSE);
  glob_timing->curr_cpuload = cpuvars[0];
  return glob_timing->curr_cpuload;
}


volatile double const *get_core_loadvar(int corenum) {
  // return a pointer to the (static) array member containing the requested value
  // returning a pointer rather than the value allows for more in depth analysis
  double *vals = get_proc_loads(FALSE);
  return &vals[corenum];
}


void show_timing_subsys(void) {
  print_enabled(player);
  print_enabled(plan);
  print_enabled(aplayer);
}


void glob_timing_init(void) {
  if (glob_timing) return;
  glob_timing = LIVES_CALLOC_SIZEOF(glob_timedata_t, 1);
  glob_timing->cpuloadvar = get_core_loadvar(0);
  glob_timing->plan.enabled = TRUE;
  glob_timing->player.enabled = TRUE;
  glob_timing->aplayer.enabled = TRUE;
}


// do nothing and see how long it takes to do it
double do_nothing(int type_of_nothing) {
  double from_whence_you_came = 0.;
  if (type_of_nothing  == THE_TIMEY_WIMEY_KIND)
    from_whence_you_came = lives_get_session_time();
  return from_whence_you_came;
}

char *format_tstr(double xtime, int minlim) {
  // format xtime (secs) as h/min/secs
  // if minlim > 0 then for mins >= minlim we don't show secs.
  char *tstr;
  int hrs = (int64_t)xtime / 3600, min;
  xtime -= hrs * 3600;
  min = (int64_t)xtime / 60;
  xtime -= min * 60;
  if (xtime >= 60.) {
    min++;
    xtime -= 60.;
  }
  if (min >= 60) {
    hrs++;
    min -= 60;
  }
  if (hrs > 0) {
    // TRANSLATORS: h(ours) min(utes)
    if (minlim) tstr = lives_strdup_printf(_("%d h %d min"), hrs, min);
    // TRANSLATORS: h(ours) min(utes) sec(onds)
    else tstr = lives_strdup_printf("%d h %d min %.2f sec", hrs, min, xtime);
  } else {
    if (min > 0) {
      if (minlim) {
        // TRANSLATORS: min(utes)
        if (min >= minlim) tstr = lives_strdup_printf(_("%d min"), min);
        // TRANSLATORS: min(utes) sec(onds)
        else tstr = lives_strdup_printf("%d min %d sec", min, (int)(xtime + .5));
      }
      // TRANSLATORS: min(utes) sec(onds)
      else tstr = lives_strdup_printf("%d min %.2f sec", min, xtime);
    } else {
      if (minlim) tstr = lives_strdup_printf("%d sec", (int)(xtime + .5));
      else tstr = lives_strdup_printf("%.2f sec", xtime);
    }
  }
  return tstr;
}


LIVES_GLOBAL_INLINE double reset_timer_info(void) {
  double timenow = lives_get_session_time();
  g_print("\n\nAction start @ %.8f\n\n", timenow);
  THREADVAR(timerinfo) = timenow;
  return timenow;
}


LIVES_GLOBAL_INLINE double show_timer_info(void) {
  double timesecs = lives_get_session_time(), tottime = timesecs - THREADVAR(timerinfo);
  char *tstr = lives_format_timing_string(tottime);
  g_print("\n\nAction completed in %s\n\n", tstr);
  lives_free(tstr);
  THREADVAR(timerinfo) = timesecs;
  return timesecs;
}


/// during playback, only player should call this
LIVES_GLOBAL_INLINE int64_t lives_get_current_time(void) {
  //  return current (wallclock) time in nsec, mapped to range 0 ... INT64_MAX
  uint64_t uret;
  int64_t ret;
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uret = (ts.tv_sec * ONE_BILLION + ts.tv_nsec);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uret = (tv.tv_sec * ONE_MILLION + tv.tv_usec)  * 1000;
#endif
  //uret = mainw ? mainw->n_service_calls * 100000 : 0;
  if ((int64_t)uret < 0) ret = (int64_t)((((uint64_t) -1) - uret) + 1);
  else ret = uret;
  if (mainw) mainw->wall_time = ret;
  return ret;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks(void) {
  return NSEC_TO_TICKS(lives_get_current_time());
}


LIVES_GLOBAL_INLINE int64_t lives_get_relative_time(int64_t origtime) {
  return lives_get_current_time() - origtime;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(ticks_t origticks) {
  return NSEC_TO_TICKS(lives_get_relative_time(TICKS_TO_NSEC(origticks)));
}

// if we have delta time (nsec) > this, we assume the process was suspended and ignore the interval
#define DELTA_THRESH ONE_BILLION

LIVES_GLOBAL_INLINE int64_t lives_get_session_time_nsec(void) {
  // return time since application was (re)started
  //
  // when running through GDB
  // we also check for any large jumps in the time
  // - these are considred as susp (application suspend)
  int64_t nsec = lives_get_relative_time(mainw->initial_time) - mainw->susp_time;

  if (RUNNER_IS(gdb)) {
    int64_t clock_delta;
    static int64_t last_nsec = 0;
    static boolean inited = FALSE;
    if (!inited) {
      inited = TRUE;
      last_nsec = nsec;
    }

    mainw->time_jump = 0;

    clock_delta = nsec - last_nsec;

    if (clock_delta > DELTA_THRESH || clock_delta < -0.0001) {
      g_print("TIME JUMP of %.4f sec DETECTED\n", clock_delta / ONE_BILLION_DBL);
      if (clock_delta > 0) mainw->susp_time += clock_delta;
      nsec -= clock_delta;
      mainw->time_jump = clock_delta;
    }
    last_nsec = nsec;
  }

  return nsec;

}


LIVES_GLOBAL_INLINE double lives_get_session_time(void) {
  // return time in sec. since application was (re)started
  return (double)lives_get_session_time_nsec() / ONE_BILLION_DBL;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_session_ticks(void) {
  // return time in seconds since application was (re)started
  return NSEC_TO_TICKS(lives_get_session_time_nsec());
}

////////// lax versions

#define RECHK_COUNT 100
/// during playback, only player should call this
LIVES_GLOBAL_INLINE int64_t lives_get_current_time_lax(void) {
  static int count = 0;
  if (++count == RECHK_COUNT) {
    count = 0;
    return lives_get_current_time();
  }
  return mainw->wall_time;
}

LIVES_GLOBAL_INLINE int64_t lives_get_relative_time_lax(int64_t origtime) {
  return lives_get_current_time_lax() - origtime;
}

LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks_lax(ticks_t origticks) {
  return NSEC_TO_TICKS(lives_get_relative_time_lax(TICKS_TO_NSEC(origticks)));
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks_lax(void) {
  return NSEC_TO_TICKS(lives_get_current_time_lax());
}


LIVES_GLOBAL_INLINE int64_t lives_get_session_time_nsec_lax(void) {
  // return time since application was (re)started
  return lives_get_relative_time_lax(mainw->initial_time) - mainw->susp_time;
}


LIVES_GLOBAL_INLINE double lives_get_session_time_lax(void) {
  // return time in sec. since application was (re)started
  return (double)lives_get_relative_time_lax(mainw->initial_time) / ONE_BILLION_DBL;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_session_ticks_lax(void) {
  // return time in seconds since application was (re)started
  return NSEC_TO_TICKS(lives_get_session_time_nsec_lax());
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


///////////////// playback clock /////////////////////////////

static int64_t baseItime, Itime, last_itime;
static double R, X, catchup;
static int last_tsource;
static int64_t lclock_nsec, last_current, prev_current, base_current;
static int64_t drift, owed_nsec, last_scnsec;

static tab_data_t *tabdata = NULL;

void reset_playback_clock(void) {
  // susp_time = total suspended time
  // clock_nsec == current_nsec - orignsec
  // lclock_nsec = last clock nsec
  // current = sc time
  // prev_current = previous current
  //
  // baseItime, base_current - a point in time (A), measurement start
  // clock_current, last_current - point in time (B) when sc time was last measued
  //
  // current - base_current - sc measured time since pt A
  // baseItime + sc measured time == actual corected time
  //
  // clock_delta == clock_nsec - lclock_nsec
  // Itime + R * clock_delta == actual uncorrected time

  R = X = 1.;
  lclock_nsec = -1;
  last_tsource = LIVES_TIME_SOURCE_NONE;
  prev_current = last_current = base_current = last_scnsec = 0;
  Itime = baseItime = last_itime = 0;
  //
  drift = 0;
  owed_nsec = 0;
  catchup = 0.1;
  if (tabdata) free_tabdata(tabdata);
  tabdata = NULL;
}


static void get_pbtimer_stats(double clock_delta) {
  char *tmp, *tmp2;
  /* g_printerr("Stats for pbtimer:\n"); */
  /* g_printerr("After %lu calls, average cycle time is %s, current load is %.2f, " */
  /*            "current clock ratio is %.4f, drift is %s\n", */
  if (!tabdata) tabdata = init_tab_data(2, 50);

  glob_timing->player.timer_ncalls++;

  glob_timing->player.timer_clock_ratio = X * R;
  glob_timing->player.timer_drift = drift / TICKS_PER_SECOND_DBL;

  if (glob_timing->player.timer_ncalls > 10000) {
    double newvals[2], av_delta, timer_load;
    newvals[0] = clock_delta;
    newvals[1] = 0.;
    tabdata_update(tabdata, newvals);
    av_delta = tabdata->avgs[0];
    timer_load = (double)clock_delta / (double)av_delta;
    newvals[1] = timer_load * 2.;
    tabdata_update(tabdata, newvals);
    glob_timing->player.timer_avcycle = tabdata->avgs[0];
    glob_timing->player.timer_load = tabdata->avgs[1];
  }
}

  /* (tmp = lives_format_timing_string(get_pbtimer_avcycle())), */
  /*            get_pbtimer_load(), */
  /*            get_pbtimer_clock_ratio(), */
  /*            (tmp2 = lives_format_timing_string(get_pbtimer_drift()))); */
  /* lives_free(tmp); lives_free(tmp2); */

/// synchronised timing
// assume we have several time sources, each running at a slightly varying rate and with their own offsets
// the goal here is to invent a "virtual" timing source which doesnt suffer from jumps (ie. monotonic)
// and in addition avoids sudden changes in the timing rate if we switch from one source to another

// since we generally want to synchronise audio and video, this virtual time should advance
// at the measured rate of the soundcard
// (unless we force system time, or unless using some other time source like transport)
//
// firstly we want to set this virtual time (Itime) to 0. at playback start
// then if we have a time source other than sys time, we measure the ratio of sys time rate verssus alt time rate
// the ratio (R) is avg(sc time - last sc time) / clock_delta
// if we get time from soundcard, we look at sc time - last sc time.
// If this has not advanced we can intepolate by adding clock_delta * R to Itime
// if it has advanced, we check sc time vs Itime. If Itime is ahead now, we want to slow down a little
// if behind we want to speed up a little.
// This is done with ratio X. If sc time is ahead of Itime, X is set to eg. 1.01
// if behind, eg. 0.99. Then we next increae Itime by adding X times sc delta.
//
// The factor them becomes X * R

//  IN summary - when playback starts, we reset Itime to 0, Then if we get time from sc, we calculate R
// If c did not update we interpolate using R * sys time. When we get an update
// we check if last Itime + sc delta > or < then adjust X.


ticks_t lives_get_current_playback_ticks(lives_time_source_t *time_source) {
  // get the time using a variety of methods
  // time_source may be NULL or LIVES_TIME_SOURCE_NONE to set auto
  // or another value to force it (EXTERNAL cannot be forced)
  lives_time_source_t tsource;
  int64_t current = 0, clock_delta = 0, clock_nsec, tdiff;

  if (time_source) tsource = *time_source;
  else tsource = LIVES_TIME_SOURCE_NONE;

  // clock time since playback started
  //mainw->clock_nsec + mainw->orignsec == session_nsec
  clock_nsec = lives_get_session_time_nsec();

  if (mainw->time_jump) {
    mainw->time_jump = 0;
    mainw->force_show = TRUE;
  }

  if (lclock_nsec < 0) lclock_nsec = clock_nsec;
  clock_delta = clock_nsec - lclock_nsec;

  lclock_nsec = clock_nsec;

  if (tsource == LIVES_TIME_SOURCE_EXTERNAL) tsource = LIVES_TIME_SOURCE_NONE;

  // force system clock
  if (mainw->foreign || prefs->force_system_clock || (prefs->vj_mode && AUD_SRC_EXTERNAL)
      || tsource == LIVES_TIME_SOURCE_SYSTEM) {
    tsource = LIVES_TIME_SOURCE_SYSTEM;
    current = clock_nsec;
  }

  //get timecode from jack transport
#ifdef ENABLE_JACK_TRANSPORT
  if (tsource == LIVES_TIME_SOURCE_NONE) {
    if (mainw->jack_can_stop && mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) {
      // calculate the time from jack transport
      tsource = LIVES_TIME_SOURCE_EXTERNAL;
      current = TICKS_TO_NSEC(jack_transport_get_current_ticks(mainw->jackd_trans));
    }
  }
#endif

  // generally tsource is set to NONE, - here we check first for soundcard time
  if (is_real_aplayer(prefs->audio_player) && (tsource == LIVES_TIME_SOURCE_NONE ||
      tsource == LIVES_TIME_SOURCE_SOUNDCARD) && !mainw->xrun_active) {
    if ((!mainw->is_rendering || (mainw->multitrack && !cfile->opening && !mainw->multitrack->is_rendering)) &&
        (!(mainw->fixed_fpsd > 0. || (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)))) {
      // get time from soundcard
      // this is done so as to synch video stream with the audio
      // we do this in two cases:
      // - for internal audio, playing back a clip with audio (writing)
      // - or when audio source is set to external (reading), no internal audio generator is running

      // we ignore this if we are running with a playback plugin which requires a fixed framerate (e.g a streaming plugin)
      // in that case we will adjust the audio rate to fit the system clock
      // or if we are rendering

      // if the timecard cannot return current time we get a value of -1 back, and then fall back to system clock

      IF_APLAYER_JACK
      (if ((prefs->audio_src == AUDIO_SRC_INT && mainw->jackd && mainw->jackd->in_use
            && IS_VALID_CLIP(mainw->jackd->playing_file) && mainw->files[mainw->jackd->playing_file]->achans > 0)
      || (prefs->audio_src == AUDIO_SRC_EXT && mainw->jackd_read && mainw->jackd_read->in_use)) {
      tsource = LIVES_TIME_SOURCE_SOUNDCARD;
      if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_jack_get_time(mainw->jackd_read);
        else
          current = lives_jack_get_time(mainw->jackd);
      })

      IF_APLAYER_PULSE
      (if ((prefs->audio_src == AUDIO_SRC_INT && mainw->pulsed && mainw->pulsed->in_use &&
            ((mainw->multitrack && cfile->achans > 0)
             || (!mainw->multitrack && IS_VALID_CLIP(mainw->pulsed->playing_file)
                 && CLIP_HAS_AUDIO(mainw->pulsed->playing_file))))
      || (prefs->audio_src == AUDIO_SRC_EXT && mainw->pulsed_read && mainw->pulsed_read->in_use)) {
      tsource = LIVES_TIME_SOURCE_SOUNDCARD;
      if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_pulse_get_time(mainw->pulsed_read);
        else
          current = lives_pulse_get_time(mainw->pulsed);
      })

    }
  }

  if (tsource == LIVES_TIME_SOURCE_SOUNDCARD) {
    // what we do here - do not actually adjust Itime from souncard, instead we have 2 ratios:
    // R: avg (scdelta / clockdelta) then clockdelta * R emulates sctime
    // X: if Itime delta measured from some point > sctime delta from same point, we want to slow down
    // so X == .99, otherwise speed up, so X = 1.01.

    if (last_tsource == LIVES_TIME_SOURCE_SYSTEM) {
      prev_current = current - clock_delta * R;
    }

    if (current < prev_current) {
      prev_current = current;
      baseItime = 0;
    }

    if (!baseItime) {
      baseItime = Itime;
      base_current = prev_current;
    }

    if (current > prev_current) {
      int64_t scdelta = current - prev_current;
      // audio drivers may do their own interolation
      // so we get the ratio from them
      if (AUD_SRC_EXTERNAL) {
        IF_AREADER_PULSE
        (R = lives_pulse_get_timing_ratio(mainw->pulsed_read, clock_nsec);)
        IF_AREADER_JACK
        (R = lives_jack_get_timing_ratio(mainw->jackd_read);)
      } else {
        IF_APLAYER_PULSE
        (R = lives_pulse_get_timing_ratio(mainw->pulsed, clock_nsec);)
        IF_APLAYER_JACK
        (R = lives_jack_get_timing_ratio(mainw->jackd);)
      }

      // check the calculated time against the measured time
      // either slow down or speed up to align
      scdelta = current - base_current;
      int64_t sctime = baseItime + scdelta; // measured time
      int64_t systime = Itime + clock_delta * R * X; // calculated time

      // negative drift means measured < calculated
      drift = systime - sctime;

      if (drift < 0) {
        if (X < 1.) X = 1.;
        else X *= 1. + prefs->pbtimer_resync_factor;
      } else if (drift > 0) {
        if (X > 1.) X = 1.;
        else X /= 1. + prefs->pbtimer_resync_factor;
      }
    }
    prev_current = current;
  }

  if (X > 1.5) X = 1.5;
  if (X < 0.66666666) X = 0.66666666;

  if (R > 1.5) R = 1.5;
  if (R < 0.66666666) R = 0.66666666;

  tdiff = clock_delta * R * X;

  if (clock_delta) {
    int64_t toomuch = 0;//tdiff - (int64_t)(prefs->pbtimer_maxdiff);
    if (toomuch > 0) {
      //g_print("tdiff was %ld, toomuch by  %ld\n",tdiff, toomuch);
      owed_nsec += toomuch;
      tdiff -= toomuch;
    } else {
      int64_t allowed;
      if (!owed_nsec) catchup = .1;
      allowed = tdiff * catchup;
      if (allowed > owed_nsec) {
	allowed = owed_nsec;
	catchup = (double)allowed / (double)tdiff;
      } else catchup *= 1.1;
      tdiff += allowed;
      owed_nsec -= allowed;
    }

    Itime += tdiff;

    if (tsource == LIVES_TIME_SOURCE_NONE) tsource = LIVES_TIME_SOURCE_SYSTEM;

    last_tsource = tsource;
    if (time_source) *time_source = tsource;
  }
  if (mainw->mark_time) Itime = last_itime;
  last_itime = Itime;

  if (clock_delta && glob_timing->player.enabled)
    get_pbtimer_stats(clock_delta);
  
  return NSEC_TO_TICKS(Itime);
}


void fdef_add_data(lives_funcdef_t *fdef, ...) {
  double xtime;
  va_list va;
  boolean end = FALSE;
  va_start(va, fdef);
  do {
    int what = va_arg(va, int);
    switch (what) {
    case TIMER_STAT_LAST: end = TRUE; break;
    case TIMER_STAT_ST_TIME: xtime = va_arg(va, double); break;
    case TIMER_STAT_EN_TIME: xtime = va_arg(va, double); break;
    case TIMER_STAT_COUNT: ((fdef_stats *)fdef->stats)->counter++; break;
    default: break;
    }
  } while (!end);
  IGN_RET(xtime);
  va_end(va);
}

////////////////////////// effort, pressure, etc
// - functions to estimate performance issues amd try to remediate
// some major areas - adaptive quality for playback
// - soon - adaptive quality for audio
// memory, disk io. cpu, network
//
// memory will try to free up some mapped data areas
// disk - will try to rebalance disk io
// cpu - tied to adaptive quality
// network N/A yet

/// estimate the machine overall load
static boolean inited = FALSE;
static int struggling = 0;
static tab_data_t *force = NULL;

void reset_effort(void) {
  if (force) {
    free_tabdata(force);
    force = NULL;
  }

  prefs->pb_quality = future_prefs->pb_quality;
  inited = TRUE;
  struggling = 0;
  if ((mainw->is_rendering || (mainw->multitrack
                               && mainw->multitrack->is_rendering)) && !mainw->preview_rendering)
    mainw->effort = -EFFORT_RANGE_MAX;
  else {
    if (mainw->effort > EFFORT_LIMIT_MED) mainw->effort = EFFORT_LIMIT_MED;
    if (mainw->effort < -EFFORT_LIMIT_MED) mainw->effort = -EFFORT_LIMIT_MED;
  }
}


void update_effort(double impulse) {
  short pb_quality = prefs->pb_quality;

  double newvals[1];
  
  if (LIVES_IS_RENDERING) {
    mainw->effort = -EFFORT_RANGE_MAX;
    prefs->pb_quality = PB_QUALITY_HIGH;
    return;
  }

  if (!force) force = init_tab_data(1, EFFORT_RANGE_MAX >> 2);

  newvals[0] = impulse;
  tabdata_update(force, newvals);
  mainw->effort = (int)(force->avgs[0]);

  g_print("eff is %f %d\n", force->avgs[0],  mainw->effort);

  if (mainw->effort > EFFORT_RANGE_MAX) mainw->effort = EFFORT_RANGE_MAX;
  if (mainw->effort < -EFFORT_RANGE_MAX) mainw->effort = -EFFORT_RANGE_MAX;

  if (mainw->effort <= 0) struggling--;
  else struggling++;

  g_print("strf is %d\n", struggling);

  if (struggling > EFFORT_LIMIT_MED) struggling = EFFORT_LIMIT_MED;
  if (struggling < -EFFORT_LIMIT_MED) struggling = -EFFORT_LIMIT_MED;

  if (mainw->effort > 0) {
    if (struggling >= EFFORT_LIMIT_MED && mainw->effort >= EFFORT_LIMIT_MED)
      pb_quality = PB_QUALITY_LOW;
    else if (struggling > 0 && pb_quality == PB_QUALITY_HIGH)
      pb_quality = PB_QUALITY_MED;
  }

  if (mainw->effort < 0) {
    if (struggling <= -EFFORT_LIMIT_MED && mainw->effort <= EFFORT_LIMIT_MED)
      pb_quality = PB_QUALITY_HIGH;
    else if (struggling > 0 && pb_quality == PB_QUALITY_LOW)
      pb_quality = PB_QUALITY_MED;
  }

  if (pb_quality != future_prefs->pb_quality)
    future_prefs->pb_quality = pb_quality;
  //g_print("STRG %d and %d %d\n", struggling, mainw->effort, prefs->pb_quality);
}




