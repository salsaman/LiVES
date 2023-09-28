// alarms.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
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

lives_result_t lives_alarm_set_timeout(uint64_t nsec);
void lives_alarm_wait(void);
int lives_alarm_get_state(void);
void lives_alarm_disarm(void);

#define lives_alarm_triggered() (lives_alarm_get_state() != LIVES_ALARM_ARMED)

//shared (system) alarms

typedef enum {
      thread_alarm = -1,
      sys_alarms_min = 0,
      urgent_msg_timeout,
      overlay_msg_timeout,
      sys_alarms_max = N_APP_TIMERS,
} alarm_name_t;

lives_result_t lives_sys_alarm_set_timeout(alarm_name_t alaname, uint64_t nsec);
void lives_sys_alarm_wait(alarm_name_t alaname);
int lives_sys_alarm_get_state(alarm_name_t alaname);
void lives_sys_alarm_disarm(alarm_name_t alaname);

#define lives_sys_alarm_triggered(alaname)		\
 (lives_sys_alarm_get_state(alaname) != LIVES_ALARM_ARMED)

/////////// lowlevel functions /////

typedef void (*lives_sigfunc_t)(int signum, siginfo_t *si, void *uc);

#define LIVES_TIMER_SIG SIGRTMIN+8 // 42...

void timer_handler(int sig, siginfo_t *si, void *uc);

void thread_signal_establish(int sig, lives_sigfunc_t sigfunc);
void thrd_signal_unblock(int sig, boolean thrd_specific);
void thrd_signal_block(int sig, boolean thrd_specific);

#define lives_spin() do {lives_nanosleep(1);} while (0);

#endif
