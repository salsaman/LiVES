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
  volatile boolean triggered;
} lives_timer_t;

lives_alarm_t lives_alarm_set(ticks_t ticks);
ticks_t lives_alarm_check(lives_alarm_t alarm_handle);
boolean lives_alarm_clear(lives_alarm_t alarm_handle);
lives_alarm_t lives_alarm_reset(lives_alarm_t alarm_handle, ticks_t ticks);

typedef void (*lives_sigfunc_t)(int signum, siginfo_t *si, void *uc);

#define LIVES_TIMER_SIG SIGRTMIN+8 // 42

void timer_handler(int sig, siginfo_t *si, void *uc);

void thread_signal_establish(int sig, lives_sigfunc_t sigfunc);
void thrd_signal_unblock(int sig, boolean thrd_specific);
void thrd_signal_block(int sig, boolean thrd_specific);

lives_timer_t *lives_timer_create(lives_timer_t *, uint64_t delay);
void lives_timer_delete(lives_timer_t *);

#define lives_spin() do {lives_nanosleep(1);} while (0);

#endif
