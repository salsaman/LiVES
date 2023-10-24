// alarms.h
// LiVES
// (c) G. Finch 2019 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _ALARMS_H_
#define _ALARMS_H_

#include <signal.h>

typedef int lives_alarm_t;

/// lives_alarms
#define LIVES_NO_ALARM 0
#define LIVES_MAX_ALARMS 1024
#define LIVES_MAX_USER_ALARMS 512

#define LIVES_NEGLIGIBLE_TIMEOUT  (.1 * TICKS_PER_SECOND_DBL) // .1 sec timeout
#define LIVES_SHORTEST_TIMEOUT  (2. * TICKS_PER_SECOND_DBL) // 2 sec timeout
#define LIVES_SHORT_TIMEOUT  (5. * TICKS_PER_SECOND_DBL) // 5 sec timeout
#define LIVES_DEFAULT_TIMEOUT  (10. * TICKS_PER_SECOND_DBL) // 10 sec timeout
#define LIVES_LONGER_TIMEOUT  (20. * TICKS_PER_SECOND_DBL) // 20 sec timeout

#define LIVES_URGENCY_ALARM LIVES_MAX_ALARMS // this is fine since we will subtract 1
#define URGENCY_MSG_TIMEOUT 10. // seconds

typedef struct {
  ticks_t tleft;
  volatile ticks_t lastcheck;
} lives_timeout_t;

typedef struct {
  // sys timers
  timer_t tid;
  // initial value when set, only for info purposes
  uint64_t delay;
  lives_sigatomic triggered;
} lives_timer_t;

#define N_APP_TIMERS 1024
extern lives_timer_t app_timers[N_APP_TIMERS];

// alarms -  deprecated
#define lives_alarm_set(ticks) (lives_alarm_set_timeout((ticks) * 10) * 1000 + ALL_USED)
#define lives_alarm_check(dimmy) (!lives_alarm_triggered())
#define lives_alarm_reset(dummy, ticks) lives_alarm_set(ticks)
boolean lives_alarm_clear(int dummy);

// alarms -  new style
#define LIVES_ALARM_INVALID	-1
#define LIVES_ALARM_DISARMED	0
#define LIVES_ALARM_ARMED	1
#define LIVES_ALARM_TRIGGERED	2

void lives_alarms_init(void);

// thread specific alarms
// e.g

// lives_alarm_set_timeout(BILLIONS(5));
// lives_microsleep_while_false((condition) || lives_alarm-triggered());
// if (lives_alarm_disarm() && !(condition)) return LIVES_RESULT_TIMEDOUT;

lives_result_t lives_alarm_set_timeout(uint64_t nsec);
// returns TRUE if alarm was triggered
boolean lives_alarm_disarm(void);

void lives_alarm_wait(void);
int lives_alarm_get_state(void);

#define lives_alarm_triggered() (lives_alarm_get_state() != LIVES_ALARM_ARMED)

//shared (system) alarms

typedef enum {
  thread_alarm = -1,
  sys_alarms_min = 0,
  heartbeat_timeout,
  urgent_msg_timeout,
  overlay_msg_timeout,
  audio_msgq_timeout,
  test_timeout,
  sys_alarms_max = N_APP_TIMERS,
} alarm_name_t;

// lives_sys_alarm_set_timeout(audio_msgq_timeout, BILLIONS(5));
// lives_microsleep_while_false((condition) || lives_sys_alarm-triggered(audio_msgq_timeout));
// if (lives_sys_alarm_disarm(audio_msgq_timeout) && !(condition)) return LIVES_RESULT_TIMEDOUT;

lives_result_t lives_sys_alarm_set_timeout(alarm_name_t alaname, uint64_t nsec);
// returns TRUE if alarm was triggered
boolean lives_sys_alarm_disarm(alarm_name_t alaname);

void lives_sys_alarm_wait(alarm_name_t alaname);
int lives_sys_alarm_get_state(alarm_name_t alaname);

#define lives_sys_alarm_triggered(alaname)		\
 (lives_sys_alarm_get_state(alaname) != LIVES_ALARM_ARMED)

/////////// lowlevel functions /////

typedef void (*lives_sigfunc_t)(int signum, siginfo_t *si, void *uc);

#define LIVES_TIMER_SIG SIGRTMIN+8 // 42...

void timer_handler(int sig, siginfo_t *si, void *uc);

void thread_signal_establish(int sig, lives_sigfunc_t sigfunc);
void thrd_signal_unblock(int sig, boolean thrd_specific);
void thrd_signal_block(int sig, boolean thrd_specific);

////////////////// spinwait /////

// wait for 1 nsec
#define lives_spin() do {lives_nanosleep(1);} while (0);

// for compatibility
#define lives_usleep(a) lives_nanosleep(1000*(a))

// wait for specified nsec - could also use timer_wait
#define lives_nanosleep(nanosec)do{struct timespec ts;ts.tv_sec=(uint64_t)(nanosec)/ONE_BILLION; \
    ts.tv_nsec=(uint64_t)(nanosec)-ts.tv_sec*ONE_BILLION;while(clock_nanosleep(CLOCK_REALTIME,0,&ts,&ts)==-1 \
							       &&errno!=ETIMEDOUT);}while(0);
//#define lives_microsleep lives_nanosleep(1000)
//#define lives_millisleep lives_nanosleep(ONE_MILLION)

#define _lives_microsleep(usec) lives_nanosleep((usec) * 1000)
#define lives_microsleep  _lives_microsleep(1)

#define _lives_millisleep(msec) lives_nanosleep(MILLIONS(msec))
#define lives_millisleep _lives_millisleep(1)

#define lives_nanosleep_until_nonzero(condition){while(!(condition))lives_spin();}
#define lives_nanosleep_until_zero(condition)lives_nanosleep_until_nonzero(!(condition))
#define lives_nanosleep_while_false(c)lives_nanosleep_until_nonzero(c)
#define lives_nanosleep_while_true(c)lives_nanosleep_until_zero(c)

/* we always check condition first, as being TRUE may have side effects
   in case it was TRUE we want to prevent alarm from triggering before it can be checked
   Thus we disarm the timer which will stop it triggering, then set trigger to 0 to make
   everything consistent.
*/

#define lives_nanosleep_until_nonzero_timeout(t_nsec,...) do {		\
  boolean condres;							\
  lives_alarm_set_timeout((t_nsec));					\
  while (!(condres=(__VA_ARGS__))&&!lives_alarm_triggered())lives_nanosleep; \
  if (condres)lives_alarm_disarm();} while (0);
#define lives_nanosleep_until_zero_timeout(t_nsec,...)lives_nanoleep_until_nonzero_timeout(t_nsec,!(__VA_ARGS__))
#define lives_nanosleep_while_false_timeout(t,...)lives_nanoleep_until_nonzero_timeout(t,__VA_ARGS__)
#define lives_nanosleep_while_true_timeout(t,...)lives_nanoleep_until_zero_timeout(t,__VA_ARGS__)

#define lives_microsleep_until_nonzero(condition)	\
  {while (!(condition))lives_microsleep;}
#define lives_microsleep_until_zero(condition)lives_microsleep_until_nonzero(!(condition))
#define lives_microsleep_while_false(c)lives_microsleep_until_nonzero(c)
#define lives_microsleep_while_true(c)lives_microsleep_until_zero(c)

#define lives_microsleep_until_nonzero_timeout(t_nsec,...) do {	\
  boolean condres;							\
  lives_alarm_set_timeout((t_nsec));					\
  while (!(condres=(__VA_ARGS__))&&!lives_alarm_triggered())lives_microsleep; \
  if (condres)lives_alarm_disarm();} while (0);
#define lives_microsleep_until_zero_timeout(t_nsec,...)lives_microsleep_until_nonzero_timeout(t_nsec,!(__VA_ARGS__))
#define lives_microsleep_while_false_timeout(t,...)lives_microleep_until_nonzero_timeout(t,__VA_ARGS__)
#define lives_microsleep_while_true_timeout(t,...)lives_microleep_until_zero_timeout(t,__VA_ARGS__)

#define lives_millisleep_until_nonzero(condition)	\
  {while (!(condition))lives_millisleep;}
#define lives_millisleep_until_zero(condition)lives_millisleep_until_nonzero(!(condition))
#define lives_millisleep_while_false(c)lives_millisleep_until_nonzero(c)
#define lives_millisleep_while_true(c)lives_millisleep_until_zero(c)

#define lives_millisleep_until_nonzero_timeout(t_nsec,...) do {	\
  boolean condres;							\
  lives_alarm_set_timeout((t_nsec));					\
  while (!(condres=(__VA_ARGS__))&&!lives_alarm_triggered())lives_millisleep; \
  if (condres)lives_alarm_disarm();} while (0);
#define lives_millisleep_until_zero_timeout(t_nsec,...)lives_millisleep_until_nonzero_timeout(t_nsec,!(__VA_ARGS__))
#define lives_millisleep_while_false_timeout(t,...)lives_millisleep_until_nonzero_timeout(t,__VA_ARGS__)
#define lives_millisleep_while_true_timeout(t,...)lives_millisleep_until_zero_timeout(t,__VA_ARGS__)

#define _DEF_GRANULE_TIME_	ONE_MILLION // nsec, ie 1 millisec
#define _lives_def_granule_sleep lives_nanosleep(_DEF_GRANULE_TIME_)

#define lives_defsleep_until_nonzero(condition)		\
  {while (!(condition))_lives_def_granule_sleep;}
#define lives_sleep_until_nonzero(condition)lives_defsleep_until_nonzero(condition)
#define lives_sleep_until_zero(condition)lives_sleep_until_nonzero(!(condition))
#define lives_sleep_while_false(c)lives_sleep_until_nonzero(c)
#define lives_sleep_while_true(c)lives_sleep_until_zero(c)

#define lives_sleep_until_nonzero_timeout(t_nsec,...) do {	\
  boolean condres;							\
  lives_alarm_set_timeout((t_nsec));					\
  while (!(condres=(__VA_ARGS__))&&!lives_alarm_triggered())_lives_def_granule_sleep; \
  if (condres)lives_alarm_disarm();} while (0);
#define lives_sleep_until_zero_timeout(t_nsec,...)lives_sleep_until_nonzero_timeout(t_nsec,!(__VA_ARGS__))
#define lives_sleep_while_false_timeout(t,...)lives_sleep_until_nonzero_timeout(t,__VA_ARGS__)
#define lives_sleep_while_true_timeout(t,...)lives_sleep_until_zero_timeout(t,__VA_ARGS__)

#define return_val_if_triggered(v)do{if(lives_alarm_triggered()){lives_alarm_disarm();return(v);}}while (0);

#define LIVES_FORTY_WINKS MILLIONS(40) // 40 mSec
#define LIVES_WAIT_A_SEC ONE_BILLION // 1 second

#endif
