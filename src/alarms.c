// alarms.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

/* LIVES_GLOBAL_INLINE lives_alarm_t lives_alarm_reset(lives_alarm_t alarm_handle, ticks_t ticks) { */
/*   // set to now + offset */
/*   // invalid alarm number */
/*   lives_timeout_t *alarm; */
/*   if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) { */
/*     LIVES_WARN("Invalid alarm handle"); */
/*     break_me("inv alarm handle in lives_alarm_reset"); */
/*     return -1; */
/*   } */

/*   // offset of 1 was added for caller */
/*   alarm = &mainw->alarms[--alarm_handle]; */

/*   alarm->lastcheck = lives_get_current_ticks(); */
/*   alarm->tleft = ticks; */
/*   return ++alarm_handle; */
/* } */


/* /\\** set alarm for now + delta ticks (10 nanosec) */
/*    param ticks (10 nanoseconds) is the offset when we want our alarm to trigger */
/*    returns int handle or -1 */
/*    call lives_get_alarm(handle) to test if time arrived */
/* *\/ */

/* lives_alarm_t lives_alarm_set(ticks_t ticks) { */
/*   int i; */

/*   // we will assign [this] next */
/*   lives_alarm_t ret; */

/*   pthread_mutex_lock(&mainw->alarmlist_mutex); */

/*   ret = mainw->next_free_alarm; */

/*   if (ret > LIVES_MAX_USER_ALARMS) ret--; */
/*   else { */
/*     // no alarm slots left */
/*     if (mainw->next_free_alarm == ALL_USED) { */
/*       pthread_mutex_unlock(&mainw->alarmlist_mutex); */
/*       LIVES_WARN("No alarms left"); */
/*       return ALL_USED; */
/*     } */
/*   } */

/*   // system alarms */
/*   if (ret >= LIVES_MAX_USER_ALARMS) { */
/*     lives_alarm_reset(++ret, ticks); */
/*     pthread_mutex_unlock(&mainw->alarmlist_mutex); */
/*     return ret; */
/*   } */

/*   i = ++mainw->next_free_alarm; */

/*   // find free slot for next time */
/*   while (mainw->alarms[i].lastcheck != 0 && i < LIVES_MAX_USER_ALARMS) i++; */

/*   if (i == LIVES_MAX_USER_ALARMS) mainw->next_free_alarm = ALL_USED; // no more alarm slots */
/*   else mainw->next_free_alarm = i; // OK */
/*   lives_alarm_reset(++ret, ticks); */
/*   pthread_mutex_unlock(&mainw->alarmlist_mutex); */

/*   return ret; */
/* } */


/* /\*** check if alarm time passed yet, if so clear that alarm and return TRUE */
/*    else return FALSE */
/* *\/ */
/* ticks_t lives_alarm_check(lives_alarm_t alarm_handle) { */
/*   ticks_t curticks; */
/*   lives_timeout_t *alarm; */

/*   // invalid alarm number */
/*   if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) { */
/*     LIVES_WARN("Invalid alarm handle"); */
/*     break_me("inv alarm handle in lives_alarm_check"); */
/*     return -1; */
/*   } */

/*   // offset of 1 was added for caller */
/*   alarm = &mainw->alarms[--alarm_handle]; */

/*   if (alarm->tleft == 0) { */
/*     if (alarm->lastcheck == 0) { */
/*       LIVES_WARN("Alarm time not set"); */
/*     } */
/*     return 0; */
/*   } */

/*   curticks = lives_get_current_ticks(); */

/*   if (prefs->show_dev_opts) { */
/*     /// guard against long interrupts (when debugging for example) */
/*     // if the last check was > 5 seconds ago, we ignore the time jump, updating the check time but not reducing the time left */
/*     if (curticks - alarm->lastcheck > 5 * TICKS_PER_SECOND) { */
/*       alarm->lastcheck = curticks; */
/*       return alarm->tleft; */
/*     } */
/*   } */

/*   alarm->tleft -= curticks - alarm->lastcheck; */

/*   if (alarm->tleft <= 0) { */
/*     // reached alarm time, free up this timer and return TRUE */
/*     //alarm->lastcheck = 0; */
/*     alarm->tleft = 0; */
/*     LIVES_DEBUG("Alarm reached"); */
/*     return 0; */
/*   } */
/*   alarm->lastcheck = curticks; */
/*   // alarm time not reached yet */
/*   return alarm->tleft; */
/* } */


/* boolean lives_alarm_clear(lives_alarm_t alarm_handle) { */
/*   if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) { */
/*     LIVES_WARN("Invalid clear alarm handle"); */
/*     return FALSE; */
/*   } */

/*   mainw->alarms[--alarm_handle].lastcheck = 0; */

/*   if (alarm_handle < LIVES_MAX_USER_ALARMS */
/*       && (mainw->next_free_alarm == ALL_USED || alarm_handle < mainw->next_free_alarm)) { */
/*     mainw->next_free_alarm = alarm_handle; */
/*   } */
/*   return TRUE; */
/* } */


////////////////

void timer_handler(int sig, siginfo_t *si, void *uc) {
  lives_sigatomic *ptrigger = (lives_sigatomic *)si->si_value.sival_ptr;
  if (ptrigger) *ptrigger = 1;
}


LIVES_GLOBAL_INLINE void thread_signal_establish(int sig, lives_sigfunc_t sigfunc) {
  // establish a signal handler for signum
  // initially this is blocked for the process
  // it can then be unblocked either process wide, or just for the caller thread
  struct sigaction sa;

  thrd_signal_block(sig, FALSE);
  thrd_signal_block(sig, TRUE);
  sa.sa_sigaction = sigfunc;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaction(sig, &sa, NULL);
}


LIVES_GLOBAL_INLINE void thrd_signal_unblock(int sig, boolean thrd_specific) {
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, sig);

  if (!thrd_specific) {
    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1) {
      char *msg = lives_strdup_printf("Error in sigproc mask UNBLOCK signal %d", sig);
      LIVES_FATAL(msg);
    }
  } else {
    // thread specific
    if (pthread_sigmask(SIG_UNBLOCK, &sigset, NULL)) {
      char *msg = lives_strdup_printf("Error in pthread_sigmask UNBLOCK signal %d", sig);
      LIVES_FATAL(msg);
    }
  }
}


LIVES_GLOBAL_INLINE void thrd_signal_block(int sig, boolean thrd_specific) {
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, sig);

  if (!thrd_specific) {
    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) {
      char *msg = lives_strdup_printf("Error in sigproc mask BLOCK signal %d", sig);
      LIVES_FATAL(msg);
    }
  } else {
    // thread specific
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL)) {
      char *msg = lives_strdup_printf("Error in pthread_sigmask BLOCK signal %d", sig);
      LIVES_FATAL(msg);
    }
  }
}


static int lives_timer_set_delay(lives_timer_t *xtimer, uint64_t delay) {
  // set the delay (nanoseconds)
  struct itimerspec its;

  its.it_value.tv_sec = delay / ONE_BILLION;
  its.it_value.tv_nsec = delay - its.it_value.tv_sec * ONE_BILLION;
  its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;

  xtimer->delay = delay;

  g_print("TIDD is %d\n", xtimer->tid);
  return timer_settime(xtimer->tid, 0, &its, NULL);
}


static lives_timer_t *lives_timer_create(lives_timer_t *xtimer) {
  if (!xtimer || xtimer->tid || !xtimer->delay) return NULL;
  timer_t timerid;
  struct sigevent sev;

  // create timer, this is per process, as the same-thread version is non portable
  // thread can pass in an xtimer struct, otherwise we allocate one
  // delay is in nsec, relative to current time. Timer will only trigger once.
  //
  // set xtimer as the sigevent.ssgev_value.sival_ptr
  // when this alarm is triggered, si_info->si_value.sival_ptr should point to the xtimer we set here
  // then whichever thread runs the singnal handler, it will set
  // the 'triggered' field of xtimer to TRUE, then remove the timer so it is not called again
  //
  // the orignal thread can store xtimer as a local variable or in THREADVAR(xtimer)
  // by checking the value of 'triggered' it can determine whether the timer has expired or not
  // in addition, if the xtimer holds a valid proc_thread, and the proc_thread is wait, it will
  // be sent a resume_request

  /* if (!xtimer) { */
  /*   xtimer = lives_calloc(1, sizeof(lives_timer_t)); */
  /*   xtimer.triggered = ATOMIC_VAR_INIT(0); */
  /* } */
  /* else */

  xtimer->triggered = 0;

  // tell the timer to send a signal LIVES_TIMER_SIG when it expires
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = LIVES_TIMER_SIG;

  // pass the created struct as data to callback
  sev.sigev_value.sival_ptr = (void *)&xtimer->triggered;;

  // changes to CLOCK_REALTIME do not affect relative times
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) return FALSE;
  xtimer->tid = timerid;

  if (lives_timer_set_delay(xtimer, xtimer->delay) == -1) return NULL;

  return xtimer;
}


static void lives_timer_delete(lives_timer_t *xtimer) {
  if (xtimer) {
    timer_t timerid = xtimer->tid;
    if (timerid) {
      if (!lives_timer_set_delay(xtimer, 0))
        timer_delete(timerid);
      xtimer->tid = 0;
    }
  }
}


// alarms -  new style ///

static void _lives_alarm_clear(lives_timer_t *timer) {
  if (!timer) timer = &(THREADVAR(xtimer));
  //
  if (timer->tid) lives_timer_delete(timer);
  //
  timer->delay = 0;
  timer->triggered = 0;
  timer->tid = 0;
}


void lives_alarm_disarm(void) {_lives_alarm_clear(NULL);}

boolean lives_alarm_clear(int dummy) {lives_alarm_disarm(); return TRUE;}


void lives_alarm_wait(void) {
  lives_timer_t *timer = &(THREADVAR(xtimer));
  if (timer && timer->tid && !timer->triggered)
    lives_microsleep_until_nonzero(timer->triggered);
  _lives_alarm_clear(timer);
}


lives_result_t lives_alarm_set_timeout(uint64_t nsec) {
  lives_timer_t *timer = &(THREADVAR(xtimer));
  _lives_alarm_clear(timer);
  if (!nsec) return LIVES_RESULT_ERROR;
  timer->delay = nsec;
  if (!lives_timer_create(timer)) return LIVES_RESULT_FAIL;
  return LIVES_RESULT_SUCCESS;
}


static lives_result_t check_alarm_state(void) {
  lives_timer_t *timer = &(THREADVAR(xtimer));
  return !timer->tid ? LIVES_RESULT_ERROR
         : !timer->triggered ? LIVES_RESULT_FAIL
         : LIVES_RESULT_TIMEDOUT;
}


int lives_alarm_get_state(void) {
  lives_result_t ret = check_alarm_state();
  return ret ==  LIVES_RESULT_ERROR ? LIVES_ALARM_DISARMED
         : ret == LIVES_RESULT_FAIL ? LIVES_ALARM_ARMED
         : LIVES_ALARM_TRIGGERED;
}


