// timing.h
// (c) G. Finch 2019 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _TIMING_H
#define _TIMING_H

char *format_tstr(double xtime, int minlim);

#define THE_TIMEY_WIMEY_KIND 1

#define TICKS_PER_SECOND ((ticks_t)MILLIONS(100)) ///< ticks per second - GLOBAL TIMEBASE
#define TICKS_PER_SECOND_DBL ((double)TICKS_PER_SECOND)   ///< actually microseconds / 100.
#define USEC_TO_TICKS (TICKS_PER_SECOND / ONE_MILLION) ///< multiplying factor uSec -> ticks_t  (def. 100)
#define MSEC_TO_TICKS (TICKS_PER_SECOND / 1000) ///< multiplying factor mSec -> ticks_t  (def. 100000)
#define TICKS_TO_NANOSEC (ONE_BILLION / TICKS_PER_SECOND) /// multiplying factor ticks_t -> nSec (def 10)

#define TICKS_TO_NSEC(ticks) ((uint64_t)(ticks) * TICKS_TO_NANOSEC)
#define NSEC_TO_TICKS(nsec) ((uint64_t)(nsec) / TICKS_TO_NANOSEC)

double  reset_timer_info(void);
double show_timer_info(void);

typedef enum {
  LIVES_TIME_SOURCE_NONE = 0,
  LIVES_TIME_SOURCE_SYSTEM,
  LIVES_TIME_SOURCE_SOUNDCARD,
  LIVES_TIME_SOURCE_EXTERNAL
} lives_time_source_t;

int64_t lives_get_current_time(void);
ticks_t lives_get_current_ticks(void);

int64_t lives_get_relative_time(int64_t origtime);
ticks_t lives_get_relative_ticks(ticks_t origticks);

int64_t lives_get_session_time_nsec(void);
double lives_get_session_time(void);

ticks_t lives_get_session_ticks(void);

int64_t lives_get_current_time_lax(void);
ticks_t lives_get_current_ticks_lax(void);

int64_t lives_get_relative_time_lax(int64_t origtime);
ticks_t lives_get_relative_ticks_lax(ticks_t origticks);

int64_t lives_get_session_time_nsec_lax(void);
double lives_get_session_time_lax(void);

ticks_t lives_get_session_ticks_lax(void);

char *lives_datetime(uint64_t secs, boolean use_local);
char *lives_datetime_rel(const char *datetime);
char *get_current_timestamp(void);

double get_pbtimer_load(void);
double get_pbtimer_avcycle(void);
uint64_t get_pbtimer_ncalls(void);
double get_pbtimer_clock_ratio(void);
double get_pbtimer_drift(void);

void show_pbtimer_stats(void);

void reset_playback_clock(ticks_t origticks);
ticks_t lives_get_current_playback_ticks(ticks_t origticks, lives_time_source_t *time_source);

double do_nothing(int type_of_nothing);

////////////////////////

// exec plan timings

typedef struct {
  lives_ann_t *ann;
  int ann_gens;
  pthread_mutex_t ann_mutex;
  pthread_mutex_t upd_mutex;
  LiVESList *proc_times;
  int cpu_nsamples;
  volatile float const *cpuloadvar;
  float curr_cpuload;
  double last_cyc_duration;
  double tot_duration;
  double avg_duration;
  double tgt_duration;
  double bytes_per_sec;
  double gbytes_per_sec;
  boolean active;
} glob_timedata_t;

typedef struct {
  // offsets from plan trigger time
  // since we do not know exact frame load times
  // we only set est dur for now
  // real_start / real_end are in session_time
  ticks_t
  // steps / template
  est_start,
  est_end,
  deadline;
  //
  // some of these are absolute tines (session times)
  // some are durations (totals)
  double
  // step + plan timings
  // thime when plan was actioned via func call
  real_start,

  // cycle finished time
  real_end,

  // predicted duration
  est_duration,

  // paused time
  paused_time,

  // real_end - real_start
  real_duration,

  // real_end - actual_start
  effective_duration,

  // time when a frame was played
  actual_start, // ?
  // durations
  preload_time, // actual_start - real_start
  active_pl_time, // step busy time berween time until actual_start
  tgt_time, // 1. / pb_fps
  concurrent_time, // total time when > 1 steps were active
  sequential_time, // sum of all steps if run sequentially
  exec_time, // dispatch time (a)
  trun_time, // thread run time (a)
  queued_time, // trun_time - exec_time
  trigger_time, // time when plan is triggered - allowed to run (a)
  start_wait, // time between thread running and trigger (trigger - trun) (d)
  waiting_time; // after triggering, time when no steps were running (idle time - d)
} timedata_t;

extern glob_timedata_t *glob_timing;

// function timings
typedef struct {
  int *exit_pts;
  double *avg_time;
  uint64_t counter;
} fdef_stats;

#define add_fdef_stats(fdef) _DW0(if (fdef) {		\
    if (!(fdef->flags & FDEF_FLAG_HAS_TIMEINFO)) {	\
      LIVES_CALLOC_TYPE(fdef_stats, fdstats, 1);	\
      fdef->stats = (void *)fdstats;			\
      fdef->flags |= FDEF_FLAG_HAS_TIMEINFO;}})

void fdef_add_data(lives_funcdef_t *, ...);

#define FDEF_STAT_LAST			0
#define FDEF_STAT_COUNT			1
#define FDEF_STAT_ST_TIME		2
#define FDEF_STAT_EN_TIME		3
#define FDEF_STAT_EXLINE		4
#define FDEF_STAT_EXVAL			5

#endif
