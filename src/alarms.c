// alarms.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

LIVES_GLOBAL_INLINE lives_alarm_t lives_alarm_reset(lives_alarm_t alarm_handle, ticks_t ticks) {
  // set to now + offset
  // invalid alarm number
  lives_timeout_t *alarm;
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid alarm handle");
    break_me("inv alarm handle in lives_alarm_reset");
    return -1;
  }

  // offset of 1 was added for caller
  alarm = &mainw->alarms[--alarm_handle];

  alarm->lastcheck = lives_get_current_ticks();
  alarm->tleft = ticks;
  return ++alarm_handle;
}


/** set alarm for now + delta ticks (10 nanosec)
   param ticks (10 nanoseconds) is the offset when we want our alarm to trigger
   returns int handle or -1
   call lives_get_alarm(handle) to test if time arrived
*/

lives_alarm_t lives_alarm_set(ticks_t ticks) {
  int i;

  // we will assign [this] next
  lives_alarm_t ret;

  pthread_mutex_lock(&mainw->alarmlist_mutex);

  ret = mainw->next_free_alarm;

  if (ret > LIVES_MAX_USER_ALARMS) ret--;
  else {
    // no alarm slots left
    if (mainw->next_free_alarm == ALL_USED) {
      pthread_mutex_unlock(&mainw->alarmlist_mutex);
      LIVES_WARN("No alarms left");
      return ALL_USED;
    }
  }

  // system alarms
  if (ret >= LIVES_MAX_USER_ALARMS) {
    lives_alarm_reset(++ret, ticks);
    pthread_mutex_unlock(&mainw->alarmlist_mutex);
    return ret;
  }

  i = ++mainw->next_free_alarm;

  // find free slot for next time
  while (mainw->alarms[i].lastcheck != 0 && i < LIVES_MAX_USER_ALARMS) i++;

  if (i == LIVES_MAX_USER_ALARMS) mainw->next_free_alarm = ALL_USED; // no more alarm slots
  else mainw->next_free_alarm = i; // OK
  lives_alarm_reset(++ret, ticks);
  pthread_mutex_unlock(&mainw->alarmlist_mutex);

  return ret;
}


/*** check if alarm time passed yet, if so clear that alarm and return TRUE
   else return FALSE
*/
ticks_t lives_alarm_check(lives_alarm_t alarm_handle) {
  ticks_t curticks;
  lives_timeout_t *alarm;

  // invalid alarm number
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid alarm handle");
    break_me("inv alarm handle in lives_alarm_check");
    return -1;
  }

  // offset of 1 was added for caller
  alarm = &mainw->alarms[--alarm_handle];

  if (alarm->tleft == 0) {
    if (alarm->lastcheck == 0) {
      LIVES_WARN("Alarm time not set");
    }
    return 0;
  }

  curticks = lives_get_current_ticks();

  if (prefs->show_dev_opts) {
    /// guard against long interrupts (when debugging for example)
    // if the last check was > 5 seconds ago, we ignore the time jump, updating the check time but not reducing the time left
    if (curticks - alarm->lastcheck > 5 * TICKS_PER_SECOND) {
      alarm->lastcheck = curticks;
      return alarm->tleft;
    }
  }

  alarm->tleft -= curticks - alarm->lastcheck;

  if (alarm->tleft <= 0) {
    // reached alarm time, free up this timer and return TRUE
    //alarm->lastcheck = 0;
    alarm->tleft = 0;
    LIVES_DEBUG("Alarm reached");
    return 0;
  }
  alarm->lastcheck = curticks;
  // alarm time not reached yet
  return alarm->tleft;
}


boolean lives_alarm_clear(lives_alarm_t alarm_handle) {
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid clear alarm handle");
    return FALSE;
  }

  mainw->alarms[--alarm_handle].lastcheck = 0;

  if (alarm_handle < LIVES_MAX_USER_ALARMS
      && (mainw->next_free_alarm == ALL_USED || alarm_handle < mainw->next_free_alarm)) {
    mainw->next_free_alarm = alarm_handle;
  }
  return TRUE;
}


void timer_handler(int sig, siginfo_t *si, void *uc) {
  volatile lives_timer_t *xtimer;

  if (!THREADVAR(uid)) return;
  thread_signal_block(LIVES_TIMER_SIG);
  xtimer = THREADVAR(xtimer);
  // doesnt work !!!
  //(lives_timer_t *)si->si_value.sival_ptr;

  if (xtimer) {
    if (xtimer->tidp) {
      // valgrind - undefined here !?
      timer_delete(*xtimer->tidp);
      xtimer->tidp = NULL;
    }
    xtimer->triggered = TRUE;
  }
}


void lives_timer_free(lives_timer_t *xtimer) {
  thread_signal_block(LIVES_TIMER_SIG);
  if (xtimer && xtimer == THREADVAR(xtimer)) {
    THREADVAR(xtimer) = NULL;
    if (xtimer->tidp) {
      timer_delete(*xtimer->tidp);
      xtimer->tidp = NULL;
    }
    lives_free(xtimer);
  }
}


lives_timer_t *lives_timer_create(uint64_t delay) {
  timer_t timerid;
  struct sigevent sev;
  struct itimerspec its;
  uint64_t dly_nanosecs;
  lives_timer_t *xtimer;

  // create timer
  xtimer = lives_calloc(1, sizeof(lives_timer_t));
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = LIVES_TIMER_SIG;
  sev.sigev_value.sival_ptr = xtimer;
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) return FALSE;

  // set the delay (nanoseconds)
  dly_nanosecs = delay;
  its.it_value.tv_sec = dly_nanosecs / ONE_BILLION;
  its.it_value.tv_nsec = dly_nanosecs % ONE_BILLION;
  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;
  if (timer_settime(timerid, 0, &its, NULL) == -1) return NULL;

  xtimer->tidp = (volatile timer_t *)&timerid;
  THREADVAR(xtimer) = xtimer;

  // unblock timer signal for this thread
  thread_signal_unblock(LIVES_TIMER_SIG);

  return xtimer;
}
