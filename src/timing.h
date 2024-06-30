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

void reset_playback_clock(void);
ticks_t lives_get_current_playback_ticks(lives_time_source_t *time_source);

double do_nothing(int type_of_nothing);

////////////////////////

void glob_timing_init(void);

/* lives_ann_t *ann; */
/* int ann_gens; */

/* pthread_mutex_t ann_mutex; */
/* pthread_mutex_t upd_mutex; */

typedef struct {
  // exec plan timings
  boolean enabled;
  LiVESList *proc_times;
  int cpu_nsamples;
  pthread_mutex_t upd_mutex;
  double last_cyc_duration;
  double tot_duration;
  double avg_duration;
  double tgt_duration;
  boolean active;
  double bytes_per_sec;
  double gbytes_per_sec;
} plan_timings;

typedef struct {
  boolean enabled;
  double inst_arate;
  double aplayer_pressure;
  int av_samples;
  double av_freq;
  double av_resp_time;
} aplayer_timings;

typedef struct {
  volatile double const *cpuloadvar;
  double curr_cpuload;
  plan_timings plan;
  aplayer_timings aplayer;
} glob_timedata_t;

#define print_enabled(subsys) _DW0(if (glob_timing && glob_timing->##subsys.active) \
				     d_print("%s\n", #subsys);)

void show_timing_subsys(void);

double get_cpu_load(void);

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

#define TIMER_STAT_LAST			0
#define TIMER_STAT_COUNT       		1 // followed by name
#define TIMER_STAT_ST_TIME		2
#define TIMER_STAT_EN_TIME		3
#define TIMER_STAT_EXLINE		4
#define TIMER_STAT_EXVAL		5 // followed by typeletter

/* typedef struct { */
/*   int detail; */
/*   allvalues_t *allvp; */
/* } timing_entry; */

/* typedef struct { */
/*   void *target; */
/*   uint64_t tuid; */
/*   lookup *entries; */
/*   lookup *avers; */
/* } timing_object; */

/* lives_lookup_t *alltimings; */


/* void send_timing_data(void *what, uid, ...); */
/* void *create_timing_pad(void *what, ..); */
/* void timer_query(void *pad, int qry, void *res); */
/* void free_timing_data(void *what); */

#endif
