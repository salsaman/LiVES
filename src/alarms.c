// alarms.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

lives_timer_t app_timers[N_APP_TIMERS];

static int lives_timer_set_delay(lives_timer_t *, uint64_t delay, boolean rpt);
static lives_timer_t *lives_timer_create(lives_timer_t *);

LIVES_GLOBAL_INLINE void thread_signal_establish(int sig, lives_sigfunc_t sigfunc) {
  // establish a signal handler for signum
  // initially this is blocked for the process
  // it can then be unblocked either process wide, or just for the caller thread
  struct sigaction sa;
  thrd_signal_block(sig);
  sa.sa_sigaction = sigfunc;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaction(sig, &sa, NULL);
}


static void ticker_handler(int sig, siginfo_t *si, void *uc) {
  // this should be getting called when the thread signal is unblocked
  // but is is not.
  lives_timer_t *timer = (lives_timer_t *)si->si_value.sival_ptr;
  printf("tick\n");
  if (timer) {
    timer->overs = si->si_overrun + 1;
    thrd_signal_block(sig);
  }
}


int64_t get_ticker_count(void) {
  int64_t overs;
  thrd_signal_unblock(LIVES_TICKER_SIG);
  lives_millisleep_until_nonzero((overs = app_timers[heartbeat_timer].overs));
  app_timers[heartbeat_timer].overs = 0;  
  thrd_signal_block(LIVES_TICKER_SIG);
  return overs;
}

void lives_alarms_init(void) {
  static boolean inited = FALSE;
  if (inited) return;
  lives_memset(app_timers, 0, N_APP_TIMERS * sizeof(lives_timer_t));

  thread_signal_establish(LIVES_TIMER_SIG, timer_handler);
  //thread_signal_establish(LIVES_TICKER_SIG, ticker_handler);

  /* app_timers[heartbeat_timer].signo = LIVES_TICKER_SIG; */
  /* lives_timer_create(&app_timers[heartbeat_timer]); */
  /* lives_timer_set_delay(&app_timers[heartbeat_timer], 100000, TRUE); */

  inited = TRUE;
}


LIVES_GLOBAL_INLINE void thrd_signal_unblock(int sig) {
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, sig);

  if (pthread_sigmask(SIG_UNBLOCK, &sigset, NULL)) {
    char *msg = lives_strdup_printf("Error in pthread_sigmask UNBLOCK signal %d", sig);
    LIVES_FATAL(msg);
  }
}


LIVES_GLOBAL_INLINE void thrd_signal_block(int sig) {
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, sig);

  // thread specific
  if (pthread_sigmask(SIG_BLOCK, &sigset, NULL)) {
    char *msg = lives_strdup_printf("Error in pthread_sigmask BLOCK signal %d", sig);
    LIVES_FATAL(msg);
  }
}


static int lives_timer_set_delay(lives_timer_t *xtimer, uint64_t delay, boolean rpt) {
  // set the delay (nanoseconds)
  xtimer->its.it_value.tv_sec = delay / ONE_BILLION;
  xtimer->its.it_value.tv_nsec = delay - xtimer->its.it_value.tv_sec * ONE_BILLION;

  if (rpt) {
    xtimer->its.it_interval.tv_sec = delay / ONE_BILLION;
    xtimer->its.it_interval.tv_nsec = delay - xtimer->its.it_interval.tv_sec * ONE_BILLION;
  }
  else xtimer->its.it_interval.tv_sec = xtimer->its.it_interval.tv_nsec = 0;

  xtimer->delay = delay;
  xtimer->triggered = 0;

#if _POSIX_TIMERS
  if (xtimer->flags & TIMER_FLAG_GET_TIMING)
    xtimer->started = lives_get_session_ticks();
#endif

  return timer_settime(xtimer->tid, 0, &xtimer->its, NULL);
}


void timer_handler(int sig, siginfo_t *si, void *uc) {
  lives_timer_t *timer = (lives_timer_t *)si->si_value.sival_ptr;
  if (timer) {
    timer->triggered = 1;
#if _POSIX_TIMERS
    if (timer->flags & TIMER_FLAG_GET_TIMING) {
      timer->ended = lives_get_session_ticks();
      if (timer->delay) timer->ratio =
			  (double)(timer->ended - timer->started)
			  / (double)timer->delay;
    }
#endif
  }
}


static lives_timer_t *lives_timer_create(lives_timer_t *xtimer) {
  // To avoid too much confuion, only the main thread ha the timer signal unblocked.
  // all it doe in the handler is dereference the address of (lives_sigatomic *) trigger,
  // and set this to 1
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
      lives_timer_set_delay(xtimer, 0, FALSE);
  } else {
    // tell the timer to send a signal LIVES_TIMER_SIG when it expires
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = xtimer->signo;

    // pass the created struct as data to callback
    sev.sigev_value.sival_ptr = (void *)xtimer;

    // changes to CLOCK_REALTIME do not affect relative times
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) return NULL;
    xtimer->tid = timerid;
  }

  xtimer->triggered = 0;

  if (xtimer->delay && lives_timer_set_delay(xtimer, xtimer->delay, FALSE) == -1) return NULL;

  return xtimer;
}


static boolean lives_timer_delete(lives_timer_t *xtimer) {
  boolean ret = FALSE;
  if (xtimer) {
    if (xtimer->tid) {
      // disarm first
      if (!xtimer->triggered) lives_timer_set_delay(xtimer, 0, FALSE);
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
  thrd_signal_block(LIVES_TIMER_SIG);
  return lives_timer_delete(&(THREADVAR(xtimer)));
}


boolean lives_sys_alarm_disarm(alarm_name_t alaname, boolean delete) {
  boolean ret = FALSE;
  if (alaname > sys_alarms_min && alaname < sys_alarms_max) {
    lives_timer_t *timer = &app_timers[alaname];
    if (timer->tid) {
      thrd_signal_block(timer->signo);
      if (delete) return lives_timer_delete(timer);
      if (!timer->triggered) lives_timer_set_delay(timer, 0, FALSE);
      else ret = TRUE;
      timer->triggered = 0;
    }
  }
  return ret;
}

///////////////////////////////////////////
static void _lives_alarm_wait(lives_timer_t *timer) {
  if (timer && timer->tid) {
    thrd_signal_block(LIVES_TIMER_SIG);
    if (!timer->triggered) {
      sigset_t sigset;
      siginfo_t si;
      lives_timer_t *xtimer;
      sigemptyset(&sigset);
      sigaddset(&sigset, LIVES_TIMER_SIG);
      timer->timeout.tv_sec = timer->its.it_value.tv_sec;
      timer->timeout.tv_nsec = timer->its.it_value.tv_nsec;
      do {
        sigtimedwait(&sigset, &si, &timer->timeout);
	xtimer = si.si_value.sival_ptr;
	if (xtimer) xtimer->triggered = 1;
      } while (!timer->triggered);

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
}

void lives_alarm_wait(void)
{_lives_alarm_wait(&(THREADVAR(xtimer)));}

void lives_sys_alarm_wait(alarm_name_t alaname) {
  if (alaname <= sys_alarms_min || alaname >= sys_alarms_max) return;
  thrd_signal_block(LIVES_TIMER_SIG);
  _lives_alarm_wait(&app_timers[alaname]);
}


static lives_result_t _lives_alarm_set_timeout(lives_timer_t *timer, uint64_t nsec) {
  if (!nsec) return LIVES_RESULT_INVALID;
  timer->delay = nsec;
  timer->signo = LIVES_TIMER_SIG;
  if (!lives_timer_create(timer)) return LIVES_RESULT_ERROR;
  thrd_signal_unblock(timer->signo);
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

