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
/*     BREAK_ME("inv alarm handle in lives_alarm_reset"); */
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
/*     BREAK_ME("inv alarm handle in lives_alarm_check"); */
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

lives_timer_t app_timers[N_APP_TIMERS];

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


void lives_alarms_init(void) {
  static boolean inited = FALSE;
  if (inited) return;
  lives_memset(app_timers, 0, N_APP_TIMERS * sizeof(lives_timer_t));
  thread_signal_establish(LIVES_TIMER_SIG, timer_handler);
  inited = TRUE;
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
  xtimer->triggered = 0;

  return timer_settime(xtimer->tid, 0, &its, NULL);
}


void timer_handler(int sig, siginfo_t *si, void *uc) {
  lives_timer_t *timer = (lives_timer_t *)si->si_value.sival_ptr;
  if (timer) {
    timer->triggered = si->si_overrun + 1;
#if _POSIX_TIMERS
    if (timer->flags & TIMER_FLAG_GET_TIMING) {
      timer->ended = lives_get_session_ticks();
      if (timer->delay)
        timer->ratio =
          (double)(timer->ended
                   - timer->started) / (double)timer->delay;
    }
#endif
  }
}


static lives_timer_t *lives_timer_create(lives_timer_t *xtimer) {
  // To avoid too much confuion, only the main thread ha the timer signal unblocked.
  // all it doe in the handler is dereference the address of (lives_sigatomic *) trigger,
  // and et thi to 1
  //
  // we have an array of (static)lives_timer_t for shared (process wide) timers
  // each thread has its own specific timer (also static)
  // here we create a timer, set the delay and interval such that it only fires once
  // the timer trigger is first et to 0, and the address of the trigger is passed to handler in
  // sigevent.sigev_value.sival_ptr
  // we can then simply check if trigger == 1
  // when the timer is not needed any more, it should be deleted, or at least disarmed to
  // prevent unneeded timer events
  timer_t timerid;
  struct sigevent sev;

  if (!xtimer) return NULL;

  if (xtimer->tid) {
    if (xtimer->delay && !xtimer->triggered)
      lives_timer_set_delay(xtimer, 0);
  } else {
    // tell the timer to send a signal LIVES_TIMER_SIG when it expires
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = LIVES_TIMER_SIG;

    // pass the created struct as data to callback
    sev.sigev_value.sival_ptr = (void *)xtimer;

    // changes to CLOCK_REALTIME do not affect relative times
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) return NULL;
    xtimer->tid = timerid;
  }

  xtimer->triggered = 0;

  if (lives_timer_set_delay(xtimer, xtimer->delay) == -1) return NULL;
  thrd_signal_unblock(LIVES_TIMER_SIG, TRUE);

#if _POSIX_TIMERS
  if (xtimer->flags & TIMER_FLAG_GET_TIMING)
    xtimer->started = lives_get_session_ticks();
#endif

  return xtimer;
}


static boolean lives_timer_delete(lives_timer_t *xtimer) {
  boolean ret = FALSE;
  if (xtimer) {
    if (xtimer->tid) {
      // disarm first
      if (!xtimer->triggered) lives_timer_set_delay(xtimer, 0);
      else ret = TRUE;
      timer_delete(xtimer->tid);
      xtimer->tid = 0;
    }
    xtimer->triggered = 0;
  }
  return ret;
}

// alarms -  new style ///

// following functions return TRUE if alarm was triggered
boolean lives_alarm_clear(int dummy) {
  lives_timer_t *timer = &(THREADVAR(xtimer));
  if (timer->tid) return lives_timer_delete(timer);
  return FALSE;
}


boolean lives_alarm_disarm(void) {
  thrd_signal_block(LIVES_TIMER_SIG, TRUE);
  return lives_timer_delete(&(THREADVAR(xtimer)));
}


boolean lives_sys_alarm_disarm(alarm_name_t alaname, boolean delete) {
  boolean ret = FALSE;
  if (alaname > sys_alarms_min && alaname < sys_alarms_max) {
    lives_timer_t *timer = &app_timers[alaname];
    if (timer->tid) {
      thrd_signal_block(LIVES_TIMER_SIG, TRUE);
      if (delete) return lives_timer_delete(timer);
      if (!timer->triggered) lives_timer_set_delay(timer, 0);
      else ret = TRUE;
      timer->triggered = 0;
    }
  }
  return ret;
}

///////////////////////////////////////////
static void _lives_alarm_wait(lives_timer_t *timer) {
  if (timer && timer->tid) {
    thrd_signal_block(LIVES_TIMER_SIG, TRUE);
    if (!timer->triggered) {
      sigset_t sigset;
      siginfo_t si;
      sigemptyset(&sigset);
      sigaddset(&sigset, LIVES_TIMER_SIG);
      do {
        //thrd_signal_unblock(LIVES_TIMER_SIG, TRUE);
        sigwaitinfo(&sigset, &si);
        //thrd_signal_block(LIVES_TIMER_SIG, TRUE);
      } while (!timer->triggered && si.si_value.sival_ptr != (void *)timer);
#if _POSIX_TIMERS
      if (timer->flags & TIMER_FLAG_GET_TIMING) {
        timer->ended = lives_get_session_ticks();
        if (timer->delay)
          timer->ratio =
            (double)(timer->ended
                     - timer->started) / (double)timer->delay;
      }
#endif
      timer->triggered = si.si_overrun + 1;
    }
  }
}

void lives_alarm_wait(void)
{_lives_alarm_wait(&(THREADVAR(xtimer)));}

void lives_sys_alarm_wait(alarm_name_t alaname) {
  if (alaname <= sys_alarms_min || alaname >= sys_alarms_max) return;
  thrd_signal_block(LIVES_TIMER_SIG, FALSE);
  _lives_alarm_wait(&app_timers[alaname]);
  thrd_signal_unblock(LIVES_TIMER_SIG, FALSE);
}


static lives_result_t _lives_alarm_set_timeout(lives_timer_t *timer, uint64_t nsec) {
  if (!nsec) return LIVES_RESULT_INVALID;
  timer->delay = nsec;
  if (!lives_timer_create(timer)) return LIVES_RESULT_ERROR;
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_alarm_set_timeout(uint64_t nsec)
{return _lives_alarm_set_timeout(&(THREADVAR(xtimer)), nsec);}

lives_result_t lives_sys_alarm_set_timeout(alarm_name_t alaname, uint64_t nsec) {
  if (alaname <= sys_alarms_min || alaname >= sys_alarms_max) return LIVES_RESULT_INVALID;
  return _lives_alarm_set_timeout(&app_timers[alaname], nsec);
}


#define ALARM_STATE(timer) (!timer->tid ? LIVES_RESULT_ERROR		\
			    : !timer->triggered ? LIVES_RESULT_FAIL	\
			    : LIVES_RESULT_TIMEDOUT)			\

static int _get_alarm_state(lives_timer_t *timer) {
  lives_result_t ret = ALARM_STATE(timer);
  return ret ==  LIVES_RESULT_ERROR ? LIVES_ALARM_DISARMED
         : ret == LIVES_RESULT_FAIL ? LIVES_ALARM_ARMED
         : LIVES_ALARM_TRIGGERED;
}

int lives_alarm_get_state(void)
{return _get_alarm_state(&(THREADVAR(xtimer)));}

int lives_sys_alarm_get_state(alarm_name_t alaname) {
  if (alaname <= sys_alarms_min || alaname >= sys_alarms_max) return LIVES_ALARM_INVALID;
  return _get_alarm_state(&app_timers[alaname]);
}

int lives_sys_alarm_get_flags(alarm_name_t alaname) {
  if (alaname <= sys_alarms_min || alaname >= sys_alarms_max) return TIMER_FLAG_INVALID;
  return app_timers[alaname].flags;
}

lives_result_t lives_sys_alarm_set_flags(alarm_name_t alaname, int flags) {
  if (alaname <= sys_alarms_min || alaname >= sys_alarms_max) return LIVES_RESULT_INVALID;
  app_timers[alaname].flags = flags;
  return LIVES_RESULT_SUCCESS;
}

static double tot_sig_time = 1., tot_clk_time = 1.;

double alarm_measure_ratio(boolean reset) {
  if (tot_sig_time > 0. &&  tot_clk_time > 0.)
    return tot_clk_time / tot_sig_time;
  return 0.;
}


double measure_t_ratio(uint64_t msec, boolean disarm) {
  double ratio = 0.;
  if (msec) {
    uint64_t nsec = msec * ONE_MILLION;
    double xtime = -lives_get_session_time(), dmsec;
    for (int i = 2; --i;) {
      if (lives_alarm_set_timeout(nsec) != LIVES_RESULT_SUCCESS)
        lives_abort("Yardstick Timer failed");
      lives_alarm_wait();
    }
    xtime += lives_get_session_time();
    if (disarm) lives_alarm_disarm();
    dmsec = (double)msec / 1000.;
    ratio = xtime / dmsec;
    tot_sig_time += dmsec;
    tot_clk_time += xtime;
    g_print("rvafd %.6f anf %.7f\n", dmsec, xtime);
  }
  return ratio;
}

