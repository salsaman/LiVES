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


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(ticks_t origticks) {
  return lives_get_current_ticks() - origticks;
}


LIVES_GLOBAL_INLINE int64_t lives_get_relative_time(int64_t origtime) {
  return lives_get_current_time() - origtime;
}


/// during playback, only player should call this
LIVES_GLOBAL_INLINE int64_t lives_get_current_time(void) {
  //  return current (wallclock) time in nsec, mapped to range 0 ... INT64_MAX
  uint64_t uret;
  ticks_t ret;
#if _POSIX_TIMERS
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uret = (ts.tv_sec * ONE_BILLION + ts.tv_nsec);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uret = (tv.tv_sec * ONE_MILLLION + tv.tv_usec)  * 1000;
#endif
  if ((int64_t)uret < 0) ret = (int64_t)((((uint64_t) -1) - uret) + 1);
  else ret = uret;
  if (mainw) mainw->wall_time = ret;
  return ret;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks(void) {
  return NSEC_TO_TICKS(lives_get_current_time());
}


LIVES_GLOBAL_INLINE int64_t lives_get_session_time_nsec(void) {
  // return time since application was (re)started
  return lives_get_relative_time(mainw->initial_time);
}


LIVES_GLOBAL_INLINE double lives_get_session_time(void) {
  // return time in sec. since application was (re)started
  return (double)lives_get_relative_time(mainw->initial_time) / ONE_BILLION_DBL;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_session_ticks(void) {
  // return time in seconds since application was (re)started
  return NSEC_TO_TICKS(lives_get_session_time_nsec());
}

////////// lax versions

LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks_lax(ticks_t origticks) {
  return lives_get_current_ticks_lax() - origticks;
}


LIVES_GLOBAL_INLINE int64_t lives_get_relative_time_lax(int64_t origtime) {
  return lives_get_current_time_lax() - origtime;
}

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


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks_lax(void) {
  return NSEC_TO_TICKS(lives_get_current_time_lax());
}


LIVES_GLOBAL_INLINE int64_t lives_get_session_time_nsec_lax(void) {
  // return time since application was (re)started
  return lives_get_relative_time_lax(mainw->initial_time);
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

static ticks_t baseItime, Itime, susp_ticks;
static double R = 1.;
static int last_tsource;
static ticks_t lclock_ticks, last_current, prev_current, clock_current, base_current, last_scticks;

void reset_playback_clock(ticks_t origticks) {
  // susp_ticks = total suspended time
  // clock_ticks == current_ticks - origticks - susp_ticks
  // lclock_ticks = last clock ticks
  // current = sc time
  // prev_current = previous current
  //
  // baseItime, base_current - a point in time (A), measurement start
  // clock_current, last_current - point in time (B) when sc time was last measued
  //
  // current - base_current - sc measured time since pt A
  // baseItime + sc measured time == actual corected time
  //
  // clock_delta == clock_ticks - lclock_ticks
  // Itime + R * clock_delta == actual uncorrected time

  R = 1.;
  lclock_ticks = -1;
  last_tsource = LIVES_TIME_SOURCE_NONE;
  prev_current = last_current = clock_current = base_current = last_scticks = 0;
  Itime = baseItime = 0;
  susp_ticks = 0;
}

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


// if we have delta time > this, we assume the process was suspended and ignore the interval
#define DELTA_THRESH TICKS_PER_SECOND

ticks_t lives_get_current_playback_ticks(ticks_t origticks, lives_time_source_t *time_source) {
  // get the time using a variety of methods
  // time_source may be NULL or LIVES_TIME_SOURCE_NONE to set auto
  // or another value to force it (EXTERNAL cannot be forced)
  lives_time_source_t tsource;
  ticks_t current = 0, clock_delta = 0;
  double X = 1.;
  //

  if (time_source) tsource = *time_source;
  else tsource = LIVES_TIME_SOURCE_NONE;

get_time:
  // clock time since playback started
  //mainw->clock_ticks + mainw->origticks == session_ticks
  mainw->clock_ticks = lives_get_current_ticks() - origticks - susp_ticks;
  if (lclock_ticks < 0 || lclock_ticks > mainw->clock_ticks)
    lclock_ticks = mainw->clock_ticks;

  clock_delta = mainw->clock_ticks - lclock_ticks;

  if (clock_delta > DELTA_THRESH) {
    g_print("TIME JUMP of %.4f sec DETECTED\n", clock_delta / TICKS_PER_SECOND_DBL);
    clock_delta -= TICKS_PER_SECOND_DBL / 10.;
    if (clock_delta > 0.) {
      susp_ticks += clock_delta;
      mainw->clock_ticks -= clock_delta;
      lclock_ticks = mainw->clock_ticks;
      goto get_time;
    }
  }

  lclock_ticks = mainw->clock_ticks;

  if (tsource == LIVES_TIME_SOURCE_EXTERNAL) tsource = LIVES_TIME_SOURCE_NONE;

  // force system clock
  if (mainw->foreign || prefs->force_system_clock || (prefs->vj_mode && AUD_SRC_EXTERNAL)) {
    tsource = LIVES_TIME_SOURCE_SYSTEM;
    current = mainw->clock_ticks;
  }

  //get timecode from jack transport
#ifdef ENABLE_JACK_TRANSPORT
  if (tsource == LIVES_TIME_SOURCE_NONE) {
    if (mainw->jack_can_stop && mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) {
      // calculate the time from jack transport
      tsource = LIVES_TIME_SOURCE_EXTERNAL;
      current = jack_transport_get_current_ticks(mainw->jackd_trans);
    }
  }
#endif

  // generally tsource is set to NONE, - here we check first for soundcard time
  if (is_realtime_aplayer(prefs->audio_player) && (tsource == LIVES_TIME_SOURCE_NONE ||
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
      (
        if ((prefs->audio_src == AUDIO_SRC_INT && mainw->jackd && mainw->jackd->in_use
             && IS_VALID_CLIP(mainw->jackd->playing_file) && mainw->files[mainw->jackd->playing_file]->achans > 0)
      || (prefs->audio_src == AUDIO_SRC_EXT && mainw->jackd_read && mainw->jackd_read->in_use)) {
      tsource = LIVES_TIME_SOURCE_SOUNDCARD;
      if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_jack_get_time(mainw->jackd_read);
        else
          current = lives_jack_get_time(mainw->jackd);
      })

      IF_APLAYER_PULSE
      (
        if ((prefs->audio_src == AUDIO_SRC_INT && mainw->pulsed && mainw->pulsed->in_use &&
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
      	clock_current = 0;
      }

      if (!baseItime) {
        baseItime = Itime;
        base_current = prev_current;
      }
      if (!clock_current) {
        clock_current = current;
        last_scticks = mainw->clock_ticks;
      }

      if (current > prev_current) {
        if (AUD_SRC_EXTERNAL) {
          IF_AREADER_PULSE
          (R = lives_pulse_get_timing_ratio(mainw->pulsed_read);)
          IF_AREADER_JACK
          (R = lives_jack_get_timing_ratio(mainw->jackd_read);)
        } else {
          IF_APLAYER_PULSE
          (R = lives_pulse_get_timing_ratio(mainw->pulsed);)
          IF_APLAYER_JACK
          (R = lives_jack_get_timing_ratio(mainw->jackd);)
        }
        ticks_t scdelta = current - base_current;
        ticks_t sctime = baseItime + scdelta;
        ticks_t systime = Itime + clock_delta * R;

        if (systime < sctime) X = 1.01;
        else if (systime > sctime) X = .99;

        prev_current = current;
      }
    }
  }

  Itime += clock_delta * R * X;

  if (tsource == LIVES_TIME_SOURCE_NONE) tsource = LIVES_TIME_SOURCE_SYSTEM;

  last_tsource = tsource;
  if (time_source) *time_source = tsource;
  return Itime;
}


