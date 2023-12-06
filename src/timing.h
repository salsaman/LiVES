// timing.h
// (c) G. Finch 2019 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define TICKS_PER_SECOND ((ticks_t)MILLIONS(100)) ///< ticks per second - GLOBAL TIMEBASE
#define TICKS_PER_SECOND_DBL ((double)TICKS_PER_SECOND)   ///< actually microseconds / 100.
#define USEC_TO_TICKS (TICKS_PER_SECOND / ONE_MILLION) ///< multiplying factor uSec -> ticks_t  (def. 100)
#define MSEC_TO_TICKS (TICKS_PER_SECOND / 1000) ///< multiplying factor mSec -> ticks_t  (def. 100000)
#define TICKS_TO_NANOSEC (ONE_BILLION / TICKS_PER_SECOND) /// multiplying factor ticks_t -> nSec (def 10)

#define TICKS_TO_NSEC(ticks) ((uint64_t)(ticks) * TICKS_TO_NANOSEC)
#define NSEC_TO_TICKS(nsec) ((uint64_t)(nsec) / TICKS_TO_NANOSEC)

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

