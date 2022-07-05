// threading.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

/**
   lives_proc_threads API
   - both proc_threads and normal threads are executed by worker thread from the pool, however:

   proc_threads differ from normal lives_threads in several ways:

   - proc_threads can call any function as long as the funcsig / return value has a mapping
   - lives_threads are intended for lightweight function calls which may be split and run in parallel
		(e.g palette conversions)
   - proc_threads are for more heavyweight functions where an etire function is wrapped to be run in the bg
   - proc_threads can call literally any function, as as the function prototype
   - proc_threads have typed return values, (or no return)
   - proc_threads have a richer set of attributes to modify their behaviour
   - proc_threads can be cancelled, either at the code points, or the underlying pthread level
	a running thread can disable or enable code level cancellation;; pthread level cancellation cannot be blocked
	a cleanup function ensures even in case of pthread level cancellation, the pthread terminates cleanly
   - proc_threads with a timeout can be created. If the task does not complete before the timer expires, the thread will be
	instantly cancelled. The thread can request a temporary stay of execution by setting the BUSY state flag, then clearing
	it later.
   - specifying a return type of 0 causes the proc_thread to automatically be freed when it completes
   - a return type of -1 implies a (void) return
   - calling lives_proc_thread_dontcare() has the effect to of turning any return type to type 0
		(this is protected by a mutex to ensure it is always done atomically)
   - amongst the available attributes are:
	- PRIORITY   - this is also a lives_thread flag: - the result is to add the job at the head of the pool queue
			rather than at the tail end
	- AUTODELETE - tells the underlying thread to free it's resources automatically there is no need to set this for proc threads
			is this done in the create function
	- WAIT_START - the proc_thread will be created, but the caller will block until a worker begins processing

	- WAIT_SYNC  - the proc_thread will be created, but the worker will wait for the caller to set sync_ready before running

	only one of WAIT_START | WAIT_SYNC should be set else thread will block


	additionally, some functions may have sync_points, the caller should attach a function
	to the thread's WAIT_SYNC_HOOKs, and send sync_ready so the thread can continue.
	in addition, when in this state, lives_proc_thread_is_waiting(lpt) will return TRUE

	IGNORE_SYNCPT - setting this flagbit tells the thread to not wait at sync points, this
	can be set if the thread can continue regardless. Whilst in the waiting state,
	a cancellable thread may be cancelled, and all threads can be cancelled_immediate regardless.
	in addition, while waiting, the BUSY state will be set so that timeout threads are not cancelled
	automatically whilst waiting. in addition if the thread is waiting for > BLOCKED_TIME, the
	thread will mark itself as blocked.  The time limit can be altered via THREADVAR(blocked_limit)
	Also, a timeout for waiting for sync can be set, via THREADVARS(sync_timeout) (milliseconds).
	If the value is positive then the thread will continue after the timeout,
	if the value is negative the thread will finish instead, setting FINIISHED and INVALID.


	- NO_GUI:    - this has no functional effect, but will set a flag in the worker thread's environment, and this may be
			checked for in the function code and used to bypass graphical updates
	- INHERIT_HOOKS - any NON_GLOBAL hooks set in the calling thread will be passed to the proc_thread
			which will have the same effect as if the hooks had been set for the bg thread
			after the thread is created, the hooks will be cleared (nullified) for caller
	- FG_THREAD: - create the thread but do not run it; main_thread_execute() uses this to create something that
	can be passed to the main thread.
	- NOTE_STTIME - thread will record statr time in ticks, this can later be retrieved using
			lives_proc_thread_get_start_ticks

   - the only requirements are to call lives_proc_thread_create() which will generate a lives_proc_thread_t and run it,
   and then (depending on the return_type parameter, call one of the lives_proc_thread_join_*() functions

   (see that function for more comments)
*/


// lives_proc_threads can call this when waiting for some other thread to sync
// it has the following effects:
// first if the thread work packet has the IGNORE_SYNC_POINTS flag set
// thread will return immediately, otherwise,
//
// thread state flag THRD_STATE_WAITING will be added
// thread state flag THRD_STATE_BUSY, will be set. This will exclude the wait time from
// the time taken for timeout threads, preventing it from being culled if another thread delays it
//
// if trigger_sync_hooks is set, the thread will trigger any hook functions in its WAIT_SYNC_HOOKS stack
//   (TODO - make the stack type selectable)
// the thread will then loop until either: it is cancelled, another thread calls sync_ready(lpt), or it times out
// the timeout limit is 0 *unlimited) by default, but this can be set (value in msec) in THREADVARS(sync_timeout)
// if the timeout limit is reached or passed, the thread will effectivle call sync_ready() on itself,
// with imeout is > 0 the thread will simply exit with value FALSE
// if timeout is < 0, the timeout value  will be the positive value of this,
// and if timed out, the thread state flags will gain THRD_STATE_TIMED_OUT
// on timeout
// if blimit msec passes, the thread will gain THRD_STATE_BLOCKED.
// The default is 10 seconds, bu this can be altered via THREADVAR(blocked_limit)  (msec)
// on exit the states WAITING and  BLOCKED will be cleared. BUSY will be cleared unless it was already set when the thread
// entered.

boolean thread_wait_loop(lives_proc_thread_t lpt, thrd_work_t *work, boolean trigger_sync_hooks, boolean wake_gui) {
  ticks_t timeout;
  uint64_t ltimeout;
  int was_busy = 0;
  int64_t msec = 0, blimit;
  boolean mark_blocked = FALSE;

  if (work && (work->flags & LIVES_THRDFLAG_IGNORE_SYNCPT) == LIVES_THRDFLAG_IGNORE_SYNCPT)
    return TRUE;

  blimit = THREADVAR(blocked_limit);
  timeout = THREADVAR(sync_timeout);
  ltimeout = labs(timeout);

  if (lives_proc_thread_check_states(lpt, THRD_STATE_BUSY) == THRD_STATE_BUSY)
    was_busy = THRD_STATE_BUSY;

  if (work) work->sync_ready = FALSE;
  lives_proc_thread_include_states(lpt, THRD_STATE_WAITING | THRD_STATE_BUSY);

  if (trigger_sync_hooks)
    lives_hooks_trigger(NULL, work->hook_closures, WAIT_SYNC_HOOK);

  while ((lpt && !lives_proc_thread_is_done(lpt) && (!work || (work && !work->sync_ready)))
         || !mainw->clutch) {
    lives_nanosleep(ONE_MILLION);
    if (wake_gui) g_main_context_wakeup(NULL);
    msec++;
    if (timeout && msec >= ltimeout) {
      lives_proc_thread_exclude_states(lpt,
                                       THRD_STATE_WAITING | was_busy | THRD_STATE_BLOCKED);
      if (work) work->sync_ready = TRUE;
      if (timeout < 0) {
        lives_proc_thread_include_states(lpt, THRD_STATE_TIMED_OUT);
      }
      return FALSE;
    }
    if (msec >= blimit && !mark_blocked) {
      lives_proc_thread_include_states(lpt, THRD_STATE_BLOCKED);
    }
  }
  lives_proc_thread_exclude_states(lpt,
                                   THRD_STATE_WAITING | was_busy | THRD_STATE_BLOCKED);
  return TRUE;
}


//static funcsig_t make_funcsig(lives_proc_thread_t func_info);

// threads can call this and wait for sync_ready from thread caller
// - returns FALSE if timed out. or thread is idle
// - if timeout is < 0 then it will also set invalid state
// - if waiting too long, temporarily sets blocked state
// - thread should check for INVALID state on return, and exit after cleaning up
// - cancellable threads should also check for CANCELLED
// - otherwise, if return is FALSE, then appropriate recovery action should be taken
boolean sync_point(const char *motive) {
  lives_proc_thread_t lpt = THREADVAR(tinfo);
  thrd_work_t *work = lives_proc_thread_get_work(lpt);
  boolean bval = FALSE;
  if (work) {
    if (motive) THREADVAR(sync_motive) = lives_strdup(motive);
    bval = thread_wait_loop(lpt, work, TRUE, FALSE);
    if (motive) lives_free(THREADVAR(sync_motive));
    g_print("sync ready\n");
    return bval;
  }
  return FALSE;
}


static void run_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr);

LIVES_GLOBAL_INLINE void lives_proc_thread_free(lives_proc_thread_t lpt) {
  pthread_mutex_t *state_mutex;
  if (!lpt) return;

  lives_nanosleep_while_false(lives_proc_thread_check_finished(lpt));
  if (weed_refcount_dec(lpt) == -1) {
    weed_refcounter_unlock(lpt);
    state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (!state_mutex) lives_abort("proc_thread missing STATE_MUTEX in lives_proc_thread_free");
    pthread_mutex_lock(state_mutex);
    weed_plant_free(lpt);
    pthread_mutex_unlock(state_mutex);
    lives_free(state_mutex);
  }
}


LIVES_LOCAL_INLINE void lives_plant_params_from_nullvargs(weed_plant_t *info, lives_funcptr_t func, int return_type) {
  weed_set_funcptr_value(info, LIVES_LEAF_THREADFUNC, func);
  // set the type of the return_value, but not the return_value itself yet
  if (return_type > 0) weed_leaf_set(info, _RV_, return_type, 0, NULL);
  weed_set_int64_value(info, LIVES_LEAF_FUNCSIG, FUNCSIG_VOID);
}


static void lives_plant_params_from_vargs(weed_plant_t *info, lives_funcptr_t func, int return_type,
    const char *args_fmt, va_list xargs) {
  const char *c;
  int p = 0;
  if (!args_fmt) return lives_plant_params_from_nullvargs(info, func, return_type);
  for (c = args_fmt; *c; c++) {
    char *pkey = lpt_param_name(p++);
    switch (*c) {
    case 'i': weed_set_int_value(info, pkey, va_arg(xargs, int)); break;
#ifdef LIVES_SEED_FLOAT
    // take advantage of the auto-promotion feature of variadic functions...
    case 'f':
#endif
    case 'd': weed_set_double_value(info, pkey, va_arg(xargs, double)); break;
    case 'b': weed_set_boolean_value(info, pkey, va_arg(xargs, int)); break;
    case 's': case 'S': weed_set_string_value(info, pkey, va_arg(xargs, char *)); break;
    case 'I': weed_set_int64_value(info, pkey, va_arg(xargs, int64_t)); break;
    case 'F': weed_set_funcptr_value(info, pkey, va_arg(xargs, weed_funcptr_t)); break;
    case 'V': case 'v': weed_set_voidptr_value(info, pkey, va_arg(xargs, void *)); break;
    case 'P': weed_set_plantptr_value(info, pkey, va_arg(xargs, weed_plantptr_t)); break;
    default: lives_proc_thread_free(info); return;
    }
    lives_free(pkey);
  }
  lives_plant_params_from_nullvargs(info, func, return_type);
  weed_set_int64_value(info, LIVES_LEAF_FUNCSIG, funcsig_from_args_fmt(args_fmt));
}


static lives_proc_thread_t lives_proc_thread_run(lives_thread_attr_t attr, lives_proc_thread_t thread_info,
    uint32_t return_type) {
  // make proc_threads joinable and if not FG. run it
  if (thread_info) {
    weed_set_int64_value(thread_info, LIVES_LEAF_THREAD_ATTRS, attr);
    if (!(attr & LIVES_THRDATTR_FG_THREAD)) {
      if (return_type) lives_proc_thread_set_state(thread_info, THRD_OPT_NOTIFY);
      if (attr & LIVES_THRDATTR_NOTE_STTIME) weed_set_int64_value(thread_info, LIVES_LEAF_START_TICKS,
            lives_get_current_ticks());

      // add to the pool
      run_proc_thread(thread_info, attr);

      if (!return_type) return NULL;
    } else lives_proc_thread_set_state(thread_info, THRD_STATE_UNQUEUED);
  }

  return thread_info;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_new(void) {
  lives_proc_thread_t thread_info = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  pthread_mutex_t *state_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(state_mutex, NULL);
  weed_set_voidptr_value(thread_info, LIVES_LEAF_STATE_MUTEX, state_mutex);
  return thread_info;
}


lives_proc_thread_t lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs) {

  lives_proc_thread_t thread_info = lives_proc_thread_new();
  lives_plant_params_from_vargs(thread_info, func, return_type, args_fmt, xargs);
  return lives_proc_thread_run(attr, thread_info, return_type);
}

lives_proc_thread_t lives_proc_thread_create_nullvargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type) {
  lives_proc_thread_t thread_info = lives_proc_thread_new();
  return lives_proc_thread_run(attr, thread_info, return_type);
}


lives_proc_thread_t lives_proc_thread_create_from_funcinst(lives_thread_attr_t attr, lives_funcinst_t *finst) {
  // for future use, eg. finst = create_funcinst(fdef, ret_loc, args...)
  // proc_thread = -this-(finst)
  if (finst) {
    lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL);
    return lives_proc_thread_run(attr, (lives_proc_thread_t)finst, fdef->return_type);
  }
  return NULL;
}


/**
   create the specific plant which defines a background task to be run
   - func is any function of a recognised type, with 0 - 16 parameters,
   and a value of type <return type> which may be retrieved by
   later calling the appropriate lives_proc_thread_join_*() function
   - args_fmt is a 0 terminated string describing the arguments of func, i ==int, d == double, b == boolean (int),
   s == string (0 terminated), I == uint64_t, int64_t, P = weed_plant_t *, V / v == (void *), F == weed_funcptr_t
   return_type is enumerated, e.g WEED_SEED_INT64. Return_type of 0 indicates no return value (void), then the thread
   will free its own resources and NULL is returned from this function (fire and forget)
   return_type of -1 has a special meaning, in this case no result is returned, but the thread can be monitored by calling:
   lives_proc_thread_check_finished() with the return : - this function is guaranteed to return FALSE whilst the thread is running
   and TRUE thereafter, the proc_thread should be freed once TRUE id returned and not before.
   for the other return_types, the appropriate join function should be called and it will block until the thread has completed its
   task and return a copy of the actual return value of the func
   alternatively, if return_type is non-zero,
   then the returned value from this function may be reutlised by passing it as the parameter
   to run_as_thread(). */
lives_proc_thread_t lives_proc_thread_create(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  if (args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    lpt = lives_proc_thread_create_vargs(attr, func, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = lives_proc_thread_create_nullvargs(attr, func, return_type);
  return lpt;
}


lives_sigdata_t *lives_sigdata_new(lives_proc_thread_t lpt, boolean is_timer) {
  lives_sigdata_t *sigdata = (lives_sigdata_t *)lives_calloc(1, sizeof(lives_sigdata_t));
  sigdata->proc = lpt;
  sigdata->is_timer = is_timer;
  return sigdata;
}


// creates a proc_thread as above, then waits for timeout. If the proc_thread does not complete before timer expires,
// since we do block here while waiting, we also service GUI requests
//
// the underlying pthread is cancelled, the proc_thread is freed and and NULL is returned,
// if the proc_thread completes before timeout, it is returned as normal and the return value
// can be read via lives_proc_thread_join_*, and the proc_thread freed as normal
// providing a return_type of 0 should not in itself cause problems for this function,
// however since NULL is always returned in this case,
// if there is a need to discover whether the function was cancelled or completed, some other mechanism must be used
lives_proc_thread_t lives_proc_thread_create_with_timeout_named(ticks_t timeout, lives_thread_attr_t attr, lives_funcptr_t func,
    const char *func_name, int return_type, const char *args_fmt, ...) {
  va_list xargs;
  lives_alarm_t alarm_handle;
  lives_proc_thread_t lpt;
  ticks_t xtimeout = 1;
  lives_cancel_t cancel = CANCEL_NONE;
  int xreturn_type = return_type;

  if (xreturn_type == 0) xreturn_type--;

  if (args_fmt) {
    va_start(xargs, args_fmt);
    lpt = lives_proc_thread_create_vargs(attr, func, xreturn_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = lives_proc_thread_create_nullvargs(attr, func, xreturn_type);

  mainw->cancelled = CANCEL_NONE;

  alarm_handle = lives_alarm_set(timeout);

  while (!lives_proc_thread_check_finished(lpt)
         && (timeout == 0 || (xtimeout = lives_alarm_check(alarm_handle)) > 0)) {
    lives_nanosleep(LIVES_QUICK_NAP);

    if (is_fg_thread()) if (!fg_service_fulfill()) lives_widget_context_iteration(NULL, FALSE);

    if (lives_proc_thread_check_states(lpt, THRD_STATE_BUSY) == THRD_STATE_BUSY) {
      // thread MUST unavoidably block; stop the timer (e.g showing a UI)
      // user or other condition may set cancelled
      if ((cancel = mainw->cancelled) != CANCEL_NONE) break;
      lives_alarm_reset(alarm_handle, timeout);
    }
  }

  lives_alarm_clear(alarm_handle);
  if (xtimeout == 0) {
    if (!lives_proc_thread_check_finished(lpt)) {
      if (prefs->jokes) {
        // TODO - come up with some other "jokes". They should be debug messages that come up _rarely_,
        // and should either insult the developer for doing something patently stupid,
        // or output a debug message in a completely over the top fashion, e.g.:
        g_print("function call to %s was ruthlessly shot down in cold blood !\nOh the humanity !", func_name);
      }
      lives_proc_thread_cancel_immediate(lpt);
      return NULL;
    }
  }

  if (return_type == 0 || cancel) {
    lives_proc_thread_join(lpt);
    return NULL;
  }
  return lpt;
}


lives_proc_thread_t lives_proc_thread_create_with_timeout(ticks_t timeout, lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, ...) {
  lives_proc_thread_t ret;
  va_list xargs;
  va_start(xargs, args_fmt);
  ret = lives_proc_thread_create_with_timeout_named(timeout, attr, func, "thefunc", return_type, args_fmt, xargs);
  va_end(xargs);
  return ret;
}


boolean is_fg_thread(void) {
  if (!mainw || !capable || !capable->gui_thread) return TRUE;
  else {
    LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
    if (!ctx) return pthread_equal(pthread_self(), capable->gui_thread);
    if (ctx == lives_widget_context_default()) {
      if (!pthread_equal(pthread_self(), capable->gui_thread)) LIVES_FATAL("pthread / widget context mismatch");
      return TRUE;
    }
    if (pthread_equal(pthread_self(), capable->gui_thread)) LIVES_FATAL("pthread / widget context mismatch");
  }
  return FALSE;
}


void lives_mutex_lock_carefully(pthread_mutex_t *mutex) {
  // this should be called whenever there is a possibility that the fg thread could deadlock
  // with a bg thread which holds the lock and needs fg GUI services
  // - we will service the bg thread requests until it releases the lock and we can obtain it for ourselves
  if (is_fg_thread()) {
    while (pthread_mutex_trylock(mutex)) {
      while (fg_service_fulfill()) lives_widget_context_update();
      lives_nanosleep(1000);
    }
  } else pthread_mutex_lock(mutex);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_deferral_stack(uint64_t xtraflags,
    lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_SINGLE_SHOT | HOOK_CB_FG_THREAD | xtraflags;
  if (THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
    return lives_hook_prepend(THREADVAR(hook_closures), THREAD_INTERNAL_HOOK,
                              filtflags, NULL, (void *)lpt);
  else
    return lives_hook_append(THREADVAR(hook_closures), THREAD_INTERNAL_HOOK,
                             filtflags, NULL, (void *)lpt);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_fg_deferral_stack(uint64_t xtraflags,
    lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_SINGLE_SHOT | HOOK_CB_FG_THREAD | xtraflags;
  if (THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
    return lives_hook_append(FG_THREADVAR(hook_closures), THREAD_INTERNAL_HOOK,
                             filtflags, NULL, (void *)lpt);
  else
    return lives_hook_append(FG_THREADVAR(hook_closures), THREAD_INTERNAL_HOOK,
                             filtflags, NULL, (void *)lpt);
}


boolean main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...) {
  /* this function exists because GTK+ can only run certain functions in the thread which called gtk_main */
  /* amy other function can be called via this, if the main thread calls it then it will simply run the target itself */
  /* for other threads, the main thread will run the function as a fg_service. */
  /* however care must be taken since fg_service cannot run another fg_service */

  /* - this has now become quite complex. There can be severla bg threads all wanting to do GUI updates. */
  /* if the main thread is idle, then it will simply pick up the request and run it. Otherewise, */
  /* the requests are queued to be run in series. If a bg thread should re-enter here itself, then the second request will be added to its deferral queue. There are some rules to prevent multiple requests. */
  /* otherwise, the main thread may be busy running a request from a different thread. In this case, */
  /* the request is added to the main thread's deferral stack, which it will process after finishing the current request. */
  /* as a further complication, the bg thread may need to wait for its request to complete, e.g running a dialog where it needs a response, in this case it will refecount the task so it is not freed, and then monitor it to see when it completes. */
  /* for the main thread, it also needs to service GUI callbacks like key press responses. In this case it will monitor the task until it finsishes, since it must run this in another background thread, and return to the gtk main loop, in this case it may re add itslef via an idel func so it can return and continue monitoring. While doin so it must still be ready to service requests from other threads, as well as more requests from the onitored thread. As well as this if the fg thread is running a service for aidle func or timer, it cannot return to the gtk main loop, as it needs to wait for the final response (TRUE or FALSE) from the subordinate timer task. Thus it will keep looping without returning, but still it needs to be servicing other threads. In particular one thread may be waitng for antother to complete and if not serviced the second thread can hagn waiting and block the first thread, wwhich can in turn block the main thread. */


  lives_proc_thread_t lpt;
  va_list xargs;
  boolean is_fg_service = FALSE;
  if (args_fmt) {
    va_start(xargs, args_fmt);
    lpt = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, func, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = lives_proc_thread_create_nullvargs(LIVES_THRDATTR_FG_THREAD, func, return_type);
  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;
  if (is_fg_thread()) {
    // run direct
    fg_run_func(lpt, retval);
    lives_proc_thread_free(lpt);
    lives_hooks_trigger(NULL, THREADVAR(hook_closures), THREAD_INTERNAL_HOOK);
  } else {
    if (!is_fg_service) {
      if (get_gov_status() != GOV_RUNNING && FG_THREADVAR(fg_service)) {
        if (THREADVAR(hook_hints) & HOOK_CB_WAIT) weed_refcount_inc(lpt);
        if (add_to_fg_deferral_stack(FG_THREADVAR(hook_flag_hints), lpt) != lpt) {
          lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
          weed_refcount_dec(lpt);
          lives_proc_thread_free(lpt);
        } else {
          if (THREADVAR(hook_hints) & HOOK_CB_WAIT) {
            while (!lives_proc_thread_check_finished(lpt)
                   && !lives_proc_thread_get_cancelled(lpt)) {
              if (get_gov_status() == GOV_RUNNING) {
                mainw->clutch = FALSE;
                lives_nanosleep_until_nonzero(mainw->clutch);
              } else lives_nanosleep(1000);
            }
            weed_refcount_dec(lpt);
            lives_proc_thread_free(lpt);
          }
        }
      } else {
        // will call fg_run_func() indirectly, so no need to call lives_proc_thread_free
        if (!lives_proc_thread_get_cancelled(lpt))
          fg_service_call(lpt, retval);
        lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
        lives_proc_thread_free(lpt);
        // some functions may have been deferred, since we cannot stack multiple fg service calls
        lives_hooks_trigger(NULL, THREADVAR(hook_closures), THREAD_INTERNAL_HOOK);
      }
    } else {
      // lpt here is a freshly created proc_thread, it will be stored and then
      if (add_to_deferral_stack(THREADVAR(hook_flag_hints), lpt) != lpt) {
        lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
        lives_proc_thread_free(lpt);
      }
      return FALSE;
    }
  }

  if (!is_fg_service) THREADVAR(fg_service) = FALSE;
  return TRUE;
}


int get_funcsig_nparms(funcsig_t sig) {
  int nparms = 0;
  for (funcsig_t test = 0x1; test <= sig; test <<= 4) nparms++;
  return nparms;
}


static boolean _call_funcsig_inner(lives_proc_thread_t thread_info, lives_funcptr_t func,
                                   uint32_t ret_type, funcsig_t sig) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  lives_proc_thread_t info;
  allfunc_t *thefunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  char *msg;
  weed_error_t err = WEED_SUCCESS;
  int nparms = get_funcsig_nparms(sig);

  if (thread_info) {
    info = thread_info;
  } else info = lives_proc_thread_new();

  lives_proc_thread_set_state(info, THRD_STATE_RUNNING);

  thefunc->func = func;

  // Note: C compilers don't care about the type / number of function args., (else it would be impossible to alias any function pointer)
  // just the type / number must be correct at runtime;
  // However it DOES care about the return type. The funcsigs are a guide so that the correct cast / number of args. can be
  // determined in the code., the first argument to the GETARG macro is set by this.
  // return_type determines which function flavour to call, e.g func, funcb, funci
  /// the second argument to GETARG relates to the internal structure of the lives_proc_thread;

  /// LIVES_PROC_THREADS ////////////////////////////////////////

  /// to make any function usable by lives_proc_thread, the _ONLY REQUIREMENT_ is to ensure that there is a switch option
  /// corresponding to the function parameters (i.e the funcsig) and return type, included here below
  /// (use of the FUNCSIG_* symbols is optional, they exist only for the sake of readability)

  ///after expanding the macros, we end up with, for example:
  // if (sig == 0x1 && ret_type == WEED_SEED_INT) {
  //   weed_set_int_value(p, "return_value", int_func(weed_get_int_value(p, "p0", NULL)));
  // }
  ///or in the case of a function returning void:
  // if (sig == 0x22) void_func(weed_get_double_value(p, "p0", NULL), weed_get_double_value(p, "p0", NULL))
  //
  // where 'p' is a lives_proc_thread_t, and int_func / void_func represent any function with a matching return type
  // in the first example, we would create p for example: p = lives_proc_thread_create(LIVES_THRDATTR_NONE, WEED_SEED_INT, int_func,
  //   "i", ival); retval = lives_proc_thread_join_int(p); lives_proc_thread_free(p);
  // (the lives_proc_thread_join_* functions wait for the function to return and then read the typed value of "return_value"
  // this would be equivalent to calling: retval = int_func(ival);
  // except that int_func() would run asyncronously by a (different) worker thread
  // the size of the worker pool changes dynamically, so there will always be a thread available

#define DO_CALL(n, ...) do {if (ret_type) XCALL_##n(__VA_ARGS__); \
  else XCALL_VOID_##n(__VA_ARGS__);} while (0);

  switch (nparms) {
  case 0: //FUNCSIG_VOID
    DO_CALL(0,)
    break;

  case 1:
    switch (sig) {
    case FUNCSIG_INT: {
      int p0;
      DO_CALL(1, int);
    } break;
    case FUNCSIG_BOOL: {
      boolean p0;
      DO_CALL(1, boolean);
    } break;
    case FUNCSIG_INT64: {
      int64_t p0;
      DO_CALL(1, int64);
    } break;
    case FUNCSIG_DOUBLE: {
      double p0;
      DO_CALL(1, double);
    } break;
    case FUNCSIG_STRING: {
      char *p0 = NULL;
      DO_CALL(1, string);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_VOIDP: {
      void *p0;
      DO_CALL(1, voidptr);
    } break;
    case FUNCSIG_PLANTP: {
      weed_plant_t *p0;
      DO_CALL(1, plantptr);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 2:
    switch (sig) {
    case FUNCSIG_INT_INT64: {
      int p0; int64_t p1;
      DO_CALL(2, int, int64);
    } break;
    case FUNCSIG_BOOL_INT: {
      int p0; int64_t p1;
      DO_CALL(2, boolean, int);
    } break;
    case FUNCSIG_INT_VOIDP: {
      int p0; void *p1;
      DO_CALL(2, int, voidptr);
    } break;
    case FUNCSIG_STRING_INT: {
      char *p0 = NULL; int p1;
      DO_CALL(2, string, int);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_STRING_BOOL: {
      char *p0 = NULL; int p1;
      DO_CALL(2, string, boolean);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_DOUBLE_DOUBLE: {
      double p0, p1;
      DO_CALL(2, double, double);
    } break;
    case FUNCSIG_VOIDP_DOUBLE: {
      void *p0; double p1;
      DO_CALL(2, voidptr, double);
    } break;
    case FUNCSIG_VOIDP_INT64: {
      void *p0; int64_t p1;
      DO_CALL(2, voidptr, int64);
    } break;
    case FUNCSIG_VOIDP_VOIDP: {
      void *p0, *p1;
      DO_CALL(2, voidptr, voidptr);
    } break;
    case FUNCSIG_VOIDP_STRING: {
      void *p0; char *p1 = NULL;
      DO_CALL(2, voidptr, string);
      if (p1) lives_free(p1);
    } break;
    case FUNCSIG_PLANTP_BOOL: {
      weed_plant_t *p0; int p1;
      DO_CALL(2, plantptr, boolean);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 3:
    switch (sig) {
    case FUNCSIG_VOIDP_VOIDP_VOIDP: {
      void *p0, *p1, *p2;
      DO_CALL(3, voidptr, voidptr, voidptr);
    } break;
    case FUNCSIG_VOIDP_VOIDP_BOOL: {
      void *p0, *p1; int p2;
      DO_CALL(3, voidptr, voidptr, boolean);
    } break;
    case FUNCSIG_STRING_VOIDP_VOIDP: {
      char *p0 = NULL; void *p1, *p2;
      DO_CALL(3, string, voidptr, voidptr);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_VOIDP_DOUBLE_INT: {
      void *p0; double p1; int p2;
      DO_CALL(3, voidptr, double, int);
    } break;
    case FUNCSIG_VOIDP_STRING_STRING: {
      void *p0; char *p1 = NULL, *p2 = NULL;
      DO_CALL(3, voidptr, string, string);
      if (p1) lives_free(p1);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_BOOL_BOOL_STRING: {
      int p0, p1; char *p2 = NULL;
      DO_CALL(3, boolean, boolean, string);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_PLANTP_VOIDP_INT64: {
      weed_plant_t *p0; void *p1; int64_t p2;
      DO_CALL(3, plantptr, voidptr, int64);
    } break;
    case FUNCSIG_INT_VOIDP_INT64: {
      int p0; void *p1; int64_t p2;
      DO_CALL(3, int, voidptr, int64);
    } break;
    case FUNCSIG_INT_INT_BOOL: {
      int p0, p1, p2;
      DO_CALL(3, int, int, boolean);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 4:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT: {
      char *p0 = NULL, *p1 = NULL; void *p2; int p3;
      DO_CALL(4, string, string, voidptr, int);
      if (p0) lives_free(p0);
      if (p1) lives_free(p1);
    } break;
    case FUNCSIG_STRING_DOUBLE_INT_STRING: {
      char *p0 = NULL, *p3 = NULL; double p1; int p2;
      DO_CALL(4, string, double, int, string);
      if (p0) lives_free(p0);
      if (p3) lives_free(p3);
    } break;
    case FUNCSIG_INT_INT_BOOL_VOIDP: {
      int p0, p1, p2; void *p3;
      DO_CALL(4, int, int, boolean, voidptr);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 5:
    switch (sig) {
    case FUNCSIG_VOIDP_STRING_STRING_INT64_INT: {
      void *p0; char *p1 = NULL, *p2 = NULL; int64_t p3; int p4;
      DO_CALL(5, voidptr, string, string, int64, int);
      if (p1) lives_free(p1);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_INT_INT_INT_BOOL_VOIDP: {
      int p0, p1, p2, p3; void *p4;
      DO_CALL(5, int, int, int, boolean, voidptr);
    } break;
    case FUNCSIG_VOIDP_INT_INT_INT_INT: {
      void *p0; int p1, p2, p3, p4;
      DO_CALL(5, voidptr, int, int, int, int);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 6:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP: {
      char *p0 = NULL, *p1 = NULL, *p4 = NULL; void *p2, *p5; int p3;
      DO_CALL(6, string, string, voidptr, int, string, voidptr);
      if (p0) lives_free(p0);
      if (p1) lives_free(p1);
      if (p4) lives_free(p4);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  // invalid nparms
  default: goto funcerr;
  }
  //lives_free(thefunc);
  if (err == WEED_SUCCESS) {
    lives_proc_thread_include_states(info, THRD_STATE_FINISHED);
    return TRUE;
  }
  msg = lives_strdup_printf("Got error %d running prothread ", err);
  goto funcerr2;

funcerr:
  // invalid args_fmt
  msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu) called", sig, sig);
  lives_proc_thread_set_state(info, THRD_STATE_INVALID);
funcerr2:
  lives_proc_thread_include_states(info, THRD_STATE_ERROR);
  msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu) called", sig, sig);
  LIVES_FATAL(msg);
  lives_free(msg);
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t info) {
  weed_funcptr_t func = weed_get_funcptr_value(info, LIVES_LEAF_THREADFUNC, NULL);
  if (func) {
    uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
    funcsig_t sig = weed_get_int64_value(info, LIVES_LEAF_FUNCSIG, NULL);
    return _call_funcsig_inner(info, func, ret_type, sig);
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      pthread_mutex_unlock(state_mutex);
      return tstate;
    }
  }
  return THRD_STATE_INVALID;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      pthread_mutex_unlock(state_mutex);
      return tstate & state_bits;
    }
  }
  return THRD_STATE_INVALID;
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | state_bits);
      pthread_mutex_unlock(state_mutex);
      return tstate;
    }
  }
  return THRD_STATE_INVALID;
}

boolean lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate & ~state_bits);
      pthread_mutex_unlock(state_mutex);
      return TRUE;
    }
  }
  return FALSE;
}

boolean lives_proc_thread_set_state(lives_proc_thread_t lpt, uint64_t state) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      pthread_mutex_lock(state_mutex);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, state);
      pthread_mutex_unlock(state_mutex);
      return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t tinfo) {
  if (tinfo && (lives_proc_thread_check_states(tinfo, THRD_STATE_FINISHED))
      == THRD_STATE_FINISHED) {
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_done(lives_proc_thread_t tinfo) {
  if (tinfo && (lives_proc_thread_check_states(tinfo, THRD_STATE_FINISHED | THRD_STATE_CANCELLED))) {
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_check_states(tinfo, THRD_STATE_SIGNALLED))
          == THRD_STATE_SIGNALLED) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_signal_data(lives_proc_thread_t tinfo, int64_t *tidx, void **data) {
  lives_thread_data_t *tdata
    = (lives_thread_data_t *)weed_get_voidptr_value(tinfo, LIVES_LEAF_SIGNAL_DATA, NULL);
  if (data) *data = tdata;
  if (tdata) {
    if (tidx) *tidx = tdata->idx;
    return tdata->signum;
  }
  return 0;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_cancellable(lives_proc_thread_t tinfo) {
  if (tinfo) lives_proc_thread_include_states(tinfo, THRD_OPT_CANCELLABLE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_check_states(tinfo, THRD_OPT_CANCELLABLE))
          == THRD_OPT_CANCELLABLE) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t tinfo, boolean dontcare) {
  if (!tinfo || !lives_proc_thread_get_cancellable(tinfo)) return FALSE;
  lives_proc_thread_include_states(tinfo, THRD_STATE_CANCELLED);
  // must set this after cancelling
  if (dontcare) lives_proc_thread_dontcare(tinfo);
  return TRUE;
}


// calls pthread_cancel on underlying thread
// - cleanup function casues the thread to the normal post cleanup, so lives_proc_thread_join_*
// will work as normal (though the values returned will not be valid)
// however this should still be called, and the proc_thread freed as normal
// (except for auto / dontcare proc_threads)
// there is a small chance the cancellation could occur either before or after the function is called
LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel_immediate(lives_proc_thread_t lpt) {
  if (lpt) {
    pthread_t base_thread = (pthread_t)weed_get_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, NULL);
    if (base_thread) {
      pthread_cancel(base_thread);
      return TRUE;
    }
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t tinfo) {
  return tinfo ? weed_get_int64_value(tinfo, LIVES_LEAF_START_TICKS, NULL) : 0;
}

boolean lives_proc_thread_dontcare(lives_proc_thread_t tinfo) {
  if (!tinfo) return FALSE;
  if (lives_proc_thread_include_states(tinfo, THRD_OPT_DONTCARE) & THRD_STATE_FINISHED) {
    // task FINISHED before we could set this, so we need to unblock and free it
    // (otherwise it would do this itself when transitioning to FINISHED)
    lives_proc_thread_join(tinfo);
    return FALSE;
  }
  return TRUE;
}


void lives_proc_thread_dontcare_nullify(lives_proc_thread_t tinfo, void **thing) {
  if (!tinfo) {
    if (thing) *thing = NULL;
    return;
  }
  if (lives_proc_thread_include_states(tinfo, THRD_OPT_DONTCARE) & THRD_STATE_FINISHED) {
    // task FINISHED before we could set this, so we need to unblock and free it
    // (otherwise it would do this itself when transitioning to FINISHED)
    lives_proc_thread_join(tinfo);
    if (thing) *thing = NULL;
  }
  weed_set_voidptr_value(tinfo, LIVES_LEAF_NULLIFY, thing);
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancelled(lives_proc_thread_t tinfo) {
  return (!tinfo || (lives_proc_thread_check_states(tinfo, THRD_STATE_CANCELLED))
          == THRD_STATE_CANCELLED) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_running(lives_proc_thread_t tinfo) {
  // state is maintaained even if cancelled or finished
  return (!tinfo || (lives_proc_thread_check_states(tinfo, THRD_STATE_RUNNING))
          == THRD_STATE_RUNNING) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_set_signalled(lives_proc_thread_t lpt, int signum, void *data) {
  if (!lpt) return FALSE;
  else {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      lives_thread_data_t *mydata = (lives_thread_data_t *)data;
      uint64_t tstate;
      if (mydata) mydata->signum = signum;
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | THRD_STATE_SIGNALLED);
      weed_set_voidptr_value(lpt, LIVES_LEAF_SIGNAL_DATA, data);
      pthread_mutex_unlock(state_mutex);
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_sync_ready(lives_proc_thread_t tinfo) {
  // this cannot be handled via states as it affects the underlying lives_thread
  if (!tinfo) return;
  else {
    thrd_work_t *work = lives_proc_thread_get_work(tinfo);
    if (work) work->sync_ready = TRUE;
  }
}

#define _join(tinfo, stype) if (is_fg_thread()) {while (!(lives_proc_thread_check_finished(tinfo))) { \
      fg_service_fulfill(); lives_nanosleep(LIVES_QUICK_NAP * 10);}}	\
  else lives_nanosleep_until_nonzero(weed_leaf_num_elements(tinfo, _RV_)); \
  return weed_get_##stype##_value(tinfo, _RV_, NULL);

/* #define _join(tinfo, stype) while (!lives_proc_thread_check_finished(tinfo)) { \ */
/*   if (!is_fg_thread() || !fg_service_fulfill()) 			\ */
/*     lives_nanosleep(LIVES_QUICK_NAP * 10);				\ */
/* 									\ */
/*  else lives_nanosleep_until_nonzero(weed_leaf_num_elements(tinfo, _RV_));} \ */
/*   return weed_get_##stype##_value(tinfo, _RV_, NULL); */


LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t tinfo) {
  // WARNING !! this version without a return value will free tinfo !
  if (is_fg_thread()) {
    while (!lives_proc_thread_check_finished(tinfo)) {
      while (fg_service_fulfill());
      lives_nanosleep(LIVES_QUICK_NAP * 10);
    }
  } else lives_nanosleep_while_false(lives_proc_thread_check_finished(tinfo));
}
//lives_proc_thread_free(tinfo);
//}

LIVES_GLOBAL_INLINE int lives_proc_thread_join_int(lives_proc_thread_t tinfo) { _join(tinfo, int);}
LIVES_GLOBAL_INLINE double lives_proc_thread_join_double(lives_proc_thread_t tinfo) {_join(tinfo, double);}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_boolean(lives_proc_thread_t tinfo) { _join(tinfo, boolean);}
LIVES_GLOBAL_INLINE int64_t lives_proc_thread_join_int64(lives_proc_thread_t tinfo) {_join(tinfo, int64);}
LIVES_GLOBAL_INLINE char *lives_proc_thread_join_string(lives_proc_thread_t tinfo) {_join(tinfo, string);}
LIVES_GLOBAL_INLINE weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t tinfo) {_join(tinfo, funcptr);}
LIVES_GLOBAL_INLINE void *lives_proc_thread_join_voidptr(lives_proc_thread_t tinfo) {_join(tinfo, voidptr);}
LIVES_GLOBAL_INLINE weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t tinfo) {_join(tinfo, plantptr);}

/**
   create a funcsig from a lives_proc_thread_t object
   the returned value can be passed to call_funcsig, along with the original lives_proc_thread_t
*/

#if 0
static funcsig_t make_funcsig(lives_proc_thread_t func_info) {
  funcsig_t funcsig = 0;
  for (int nargs = 0; nargs < 16; nargs++) {
    char *lname = lpt_param_name(nargs);
    int st = weed_leaf_seed_type(func_info, lname);
    lives_free(lname);
    if (!st) break;
    funcsig <<= 4;  /// 4 bits per argtype, hence up to 16 args in a uint64_t
    if (st < 12) funcsig |= st; // 1 == int, 2 == double, 3 == boolean (int), 4 == char *, 5 == int64_t
    else {
      switch (st) {
      case WEED_SEED_FUNCPTR: funcsig |= 0XC; break;
      case WEED_SEED_VOIDPTR: funcsig |= 0XD; break;
      case WEED_SEED_PLANTPTR: funcsig |= 0XE; break;
      default: funcsig |= 0XF; break;
      }
    }
  }
  return funcsig;
}
#endif

static void pthread_cleanup_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  boolean dontcare = (lives_proc_thread_check_states(info, THRD_OPT_DONTCARE)
                      == THRD_OPT_DONTCARE) ? TRUE : FALSE;

  // add FINISHED flag in cas not set already
  lives_proc_thread_include_states(info, THRD_STATE_FINISHED);

  lives_hooks_trigger(NULL, THREADVAR(hook_closures), THREAD_EXIT_HOOK);

  if (dontcare || (!ret_type && lives_proc_thread_check_states(info, THRD_OPT_NOTIFY)
                   != THRD_OPT_NOTIFY)) {
    void **nulif = (void **)weed_get_voidptr_value(info, LIVES_LEAF_NULLIFY, NULL);
    if (nulif) *nulif = NULL;
    THREADVAR(tinfo) = NULL;
    lives_proc_thread_free(info);
  }
}


static void *_plant_thread_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  THREADVAR(tinfo) = info;

  // ensures that tinfo is handled cleanly even if pthread_cancel is called on the underlying pthread
  pthread_cleanup_push(pthread_cleanup_func, args);

  weed_set_voidptr_value(info, LIVES_LEAF_PTHREAD_SELF, (void *)pthread_self());
  call_funcsig(info);
  weed_set_voidptr_value(info, LIVES_LEAF_PTHREAD_SELF, NULL);

  // arg of 1 ensures it is executed when popped
  pthread_cleanup_pop(1);

  return NULL;
}


boolean fg_run_func(lives_proc_thread_t lpt, void *rloc) {
  // rloc should be a pointer to a variable of the correct type. After calling this,
  boolean bret = FALSE;
  if (!lives_proc_thread_get_cancelled(lpt)) {
    call_funcsig(lpt);
    if (rloc) {
      if (weed_leaf_seed_type(lpt, _RV_) == WEED_SEED_STRING) *(char **)rloc = weed_get_string_value(lpt, _RV_, NULL);
      else _weed_leaf_get(lpt, _RV_, 0, rloc);
    }
  }
  lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
  return bret;
}


/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
static void run_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  lives_thread_t *thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
  thrd_work_t *mywork;
  //pthread_mutex_t *state_mutex = weed_get_voidptr_value(thread_info, LIVES_LEAF_STATE_MUTEX, NULL);

  /// tell the thread to clean up after itself [but it won't delete thread_info]
  attr |= LIVES_THRDATTR_AUTODELETE;
  attr |= LIVES_THRDATTR_IS_PROC_THREAD;

  // add the work to the pool
  lives_thread_create(thread, attr, _plant_thread_func, (void *)thread_info);
  lives_proc_thread_set_state(thread_info, THRD_STATE_QUEUED);

  mywork = (thrd_work_t *)thread->data;

  if (attr & LIVES_THRDATTR_IGNORE_SYNCPT) {
    mywork->flags |= LIVES_THRDFLAG_IGNORE_SYNCPT;
  }

  lives_proc_thread_set_work(thread_info, mywork);
  if (attr & LIVES_THRDATTR_WAIT_START) {
    // WAIT_START: caller waits for thread to run or finish
    lives_nanosleep_until_nonzero(mywork->flags & (LIVES_THRDFLAG_RUNNING |
                                  LIVES_THRDFLAG_FINISHED));
    mywork->flags &= ~LIVES_THRDATTR_WAIT_START;
  }
}


//////// worker thread pool //////////////////////////////////////////

///////// thread pool ////////////////////////
#ifndef VALGRIND_ON
#define MINPOOLTHREADS 8
#else
#define MINPOOLTHREADS 2
#endif
static volatile int npoolthreads, rnpoolthreads;
static pthread_t **poolthrds;
static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t allctx_rwlock;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile LiVESList *twork_first, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

// TODO - use THRD_SPECIFIC
static LiVESList *allctxs = NULL;

lives_thread_data_t *get_thread_data(void) {
  LiVESList *list;
  pthread_rwlock_rdlock(&allctx_rwlock);
  list = allctxs;
  for (; list; list = list->next) {
    if (pthread_equal(((lives_thread_data_t *)list->data)->pthread, pthread_self())) {
      pthread_rwlock_unlock(&allctx_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return NULL;
}


lives_thread_data_t *get_global_thread_data(void) {
  pthread_rwlock_rdlock(&allctx_rwlock);
  for (LiVESList *list = allctxs; list; list = list->next) {
    if (pthread_equal(((lives_thread_data_t *)list->data)->pthread, capable->gui_thread)) {
      pthread_rwlock_unlock(&allctx_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return NULL;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars(void) {
  static lives_threadvars_t *dummyvars = NULL;
  lives_thread_data_t *thrdat = get_thread_data();
  if (!thrdat) {
    if (!dummyvars) dummyvars = lives_calloc(1, sizeof(lives_threadvars_t));
    return dummyvars;
  }
  return &thrdat->vars;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_global_threadvars(void) {
  static lives_threadvars_t *dummyvars = NULL;
  lives_thread_data_t *thrdat = get_global_thread_data();
  if (!thrdat) {
    if (!dummyvars) dummyvars = lives_calloc(1, sizeof(lives_threadvars_t));
    return dummyvars;
  }
  return &thrdat->vars;
}


lives_thread_data_t *get_thread_data_by_id(uint64_t idx) {
  LiVESList *list = allctxs;
  pthread_rwlock_rdlock(&allctx_rwlock);
  for (; list; list = list->next) {
    if (((lives_thread_data_t *)list->data)->idx == idx) {
      pthread_rwlock_unlock(&allctx_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return NULL;
}


LIVES_GLOBAL_INLINE int isstck(void *ptr) {
  size_t stacksize = THREADVAR(stacksize);
  if (stacksize) {
    void *stack = THREADVAR(stackaddr);
    if ((uintptr_t)ptr >= (uintptr_t)stack
        && (uintptr_t)ptr < (uintptr_t)stack + stacksize)
      return LIVES_RESULT_SUCCESS;
    return LIVES_RESULT_FAIL;
  }
  return LIVES_RESULT_ERROR;
}


lives_thread_data_t *lives_thread_data_create(uint64_t idx) {
  lives_thread_data_t *tdata;
  pthread_t self;

#ifdef USE_RPMALLOC
  // must be done before anything else
  if (!rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_initialize();
  }
#endif

  self = pthread_self();

  tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));

  if (idx != 0) tdata->ctx = lives_widget_context_new();
  else tdata->ctx = lives_widget_context_default();

  tdata->vars.var_id = tdata->idx = idx;
  tdata->vars.var_rowstride_alignment = RS_ALIGN_DEF;
  tdata->vars.var_last_sws_block = -1;
  tdata->vars.var_uid = gen_unique_id();
  tdata->vars.var_blocked_limit = BLOCKED_LIMIT;
  tdata->vars.var_mydata = tdata;
  tdata->vars.var_self = self;

  lives_icap_init(&tdata->vars.var_intentcap);

#ifndef NO_NP
  if (1) {
    pthread_attr_t attr;
    void *stack;
    size_t stacksize;
    pthread_getattr_np(self, &attr);
    pthread_attr_getstack(&attr, &stack, &stacksize);
    THREADVAR(stackaddr) = stack;
    THREADVAR(stacksize) = stacksize;
  }
#endif
  for (int i = 0; i < N_HOOK_FUNCS; i++) {
    pthread_mutex_init(&tdata->vars.var_hook_mutex[i], NULL);
  }
  pthread_rwlock_wrlock(&allctx_rwlock);
  allctxs = lives_list_prepend(allctxs, (livespointer)tdata);
  pthread_rwlock_unlock(&allctx_rwlock);

  lives_nanosleep(LIVES_FORTY_WINKS);

  make_thrdattrs();

  return tdata;
}


static boolean widget_context_wrapper(livespointer data) {
  thrd_work_t *mywork = (thrd_work_t *)data;
  // for lpt, we set this in call_funcsig
  mywork->flags |= LIVES_THRDFLAG_RUNNING;
  (*mywork->func)(mywork->arg);
  mywork->flags = (mywork->flags & ~LIVES_THRDFLAG_RUNNING) | LIVES_THRDFLAG_FINISHED;
  return FALSE;
}


//static pthread_mutex_t cpusel_mutex = PTHREAD_MUTEX_INITIALIZER;

LIVES_GLOBAL_INLINE void lives_hooks_clear(LiVESList **xlist, int type) {
  if (xlist) {
    lives_list_free_all(&(xlist[type]));
    xlist[type] = NULL;
  }
}

LIVES_LOCAL_INLINE LiVESList *lives_hooks_copy(LiVESList *in) {
  LiVESList *out = NULL;
  for (; in; in = in->next) {
    lives_closure_t *inclosure = (lives_closure_t *)in->data;
    lives_closure_t *outclosure = (lives_closure_t *)lives_malloc(sizeof(lives_closure_t));
    outclosure->func = inclosure->func;
    outclosure->data = inclosure->data;
    outclosure->retloc = inclosure->retloc;
    out = lives_list_append(out, outclosure);
  }
  return out;
}


LIVES_LOCAL_INLINE void lives_hooks_transfer(LiVESList **dest, LiVESList **src, boolean include_glob) {
  int type = 0;
  if (!include_glob) type = N_GLOBAL_HOOKS + 1;
  for (; type < N_HOOK_FUNCS && src[type]; type++) {
    dest[type] = src[type];
    src[type] = NULL;
  }
}


boolean do_something_useful(lives_thread_data_t *tdata) {
  /// yes, why don't you lend a hand instead of just lying around nanosleeping...
  lives_proc_thread_t lpt;
  LiVESList *list;
  thrd_work_t *mywork;
  uint64_t myflags = 0;
  //boolean didlock = FALSE;

  if (!tdata->idx) lives_abort("Invalid worker thread ID - internal error");

  pthread_mutex_lock(&twork_mutex);

  list = (LiVESList *)twork_last;

  if (LIVES_UNLIKELY(!list)) {
    pthread_mutex_unlock(&twork_mutex);
    return FALSE;
  }

  if ((LiVESList *)twork_first == list) twork_last = twork_first = NULL;
  else {
    twork_last = (volatile LiVESList *)list->prev;
    twork_last->next = NULL;
  }
  pthread_mutex_unlock(&twork_mutex);

  mywork = (thrd_work_t *)list->data;
  if (!mywork) return FALSE;

  lpt = mywork->lpt;
  if (lpt) lives_proc_thread_set_state(lpt, THRD_STATE_PREPARING);

  mywork->busy = tdata->idx;
  myflags = mywork->flags;

  if (mywork->attr & LIVES_THRDATTR_INHERIT_HOOKS) {
    // attribute is set to indicate that hook closures should be passed to thread which runs the job
    // closures are first copied from caller to the task definition, then finally to the thread
    lives_hooks_transfer(THREADVAR(hook_closures), mywork->hook_closures, FALSE);
  }

  /* if (prefs->jokes) { */
  /*   fprintf(stderr, "thread id %ld reporting for duty, Sir !\n", get_thread_id()); */
  /* } */

  // wait for SYNC READY

  if (!mywork->sync_ready) {
    if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_WAITING);
    lives_nanosleep_while_false(mywork->sync_ready);
    if (lpt) lives_proc_thread_exclude_states(lpt, THRD_STATE_WAITING);
  }
  widget_context_wrapper(mywork);

  /* lives_widget_context_invoke_full(tdata->ctx, mywork->attr & LIVES_THRDATTR_PRIORITY */
  /*                                  ? LIVES_WIDGET_PRIORITY_HIGH - 100 : LIVES_WIDGET_PRIORITY_HIGH, */
  /*                                  widget_context_wrapper, mywork, NULL); */

  for (int type = N_GLOBAL_HOOKS + 1; type < N_HOOK_FUNCS; type++) {
    lives_hooks_clear(THREADVAR(hook_closures), type);
  }

  // make sure caller has noted that thread finished
  lives_nanosleep_until_zero(myflags & LIVES_THRDATTR_WAIT_START);

  if (myflags & LIVES_THRDFLAG_AUTODELETE) {
    lives_free(mywork);
    lives_thread_free((lives_thread_t *)list);
  } else mywork->done = tdata->idx;

  THREADVAR(tinfo) = NULL;

  pthread_mutex_lock(&twork_count_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_count_mutex);

#ifdef USE_RPMALLOC
  rpmalloc_thread_collect();
#endif
  return TRUE;
}


#define POOL_TIMEOUT_SEC 120

static void *thrdpool(void *arg) {
  boolean skip_wait = FALSE;
  static struct timespec ts;
  int rc = 0;
  int tid = LIVES_POINTER_TO_INT(arg);

  lives_thread_data_t *tdata;

  // must do before anything else
  tdata = lives_thread_data_create(tid);

  lives_widget_context_push_thread_default(tdata->ctx);

#ifdef THRD_SPECIFIC
  set_thread_id(gen_unique_id());
#endif

  while (!threads_die) {
    if (!skip_wait) {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += POOL_TIMEOUT_SEC;
      lives_mutex_lock_carefully(&tcond_mutex);
      rc = pthread_cond_timedwait(&tcond, &tcond_mutex, &ts);
      pthread_mutex_unlock(&tcond_mutex);
      if (rc == ETIMEDOUT) {
        if (!pthread_mutex_trylock(&pool_mutex)) {
          npoolthreads--;
          tdata->exited = TRUE;
          lives_widget_context_pop_thread_default(tdata->ctx);
          lives_widget_context_unref(tdata->ctx);
          pthread_mutex_unlock(&pool_mutex);
          break;
        }
      }
    }
    if (LIVES_UNLIKELY(threads_die)) {
      lives_widget_context_pop_thread_default(tdata->ctx);
      break;
    }
    skip_wait = do_something_useful(tdata);
#ifdef USE_RPMALLOC
    if (skip_wait) {
      if (rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_collect();
      }
    }
#endif
  }
#ifdef USE_RPMALLOC
  if (rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_finalize();
  }
#endif

  return NULL;
}


void lives_threadpool_init(void) {
  pthread_rwlock_init(&allctx_rwlock, NULL);
  rnpoolthreads = npoolthreads = MINPOOLTHREADS;
  if (mainw->debug) rnpoolthreads = npoolthreads = 0;
  if (prefs->nfx_threads > npoolthreads) rnpoolthreads = npoolthreads = prefs->nfx_threads;
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_first = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
  }
}


void lives_threadpool_finish(void) {
  threads_die = TRUE;
  lives_mutex_lock_carefully(&tcond_mutex);
  pthread_cond_broadcast(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  for (int i = 0; i < rnpoolthreads; i++) {
    lives_thread_data_t *tdata = get_thread_data_by_id(i + 1);
    if (!tdata->exited) {
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
      pthread_join(*(poolthrds[i]), NULL);
      if (tdata->ctx && tdata->ctx != lives_widget_context_default())
        lives_widget_context_unref(tdata->ctx);
      lives_free(tdata);
      lives_free(poolthrds[i]);
    }
  }
  lives_list_free(allctxs);
  allctxs = NULL;
  lives_free(poolthrds);
  poolthrds = NULL;
  rnpoolthreads = npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_first);
  twork_first = twork_last = NULL;
  ntasks = 0;
}


LIVES_GLOBAL_INLINE void lives_thread_free(lives_thread_t *thread) {
  if (thread) lives_free(thread);
}


int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr,
                        lives_thread_func_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  if (!thread) list = (LiVESList *)lives_calloc(1, sizeof(LiVESList));
  else list->next = list->prev = NULL;
  list->data = work;
  work->func = func;
  work->attr = attr;
  work->arg = arg;
  work->caller = THREADVAR(id);
  work->sync_ready = TRUE;

  if (attr & LIVES_THRDATTR_IS_PROC_THREAD) work->lpt = (lives_proc_thread_t)arg;

  if (!thread || (attr & LIVES_THRDATTR_AUTODELETE))
    work->flags |= LIVES_THRDFLAG_AUTODELETE;

  if (attr & LIVES_THRDATTR_WAIT_SYNC) work->sync_ready = FALSE;

  if (attr & LIVES_THRDATTR_INHERIT_HOOKS) {
    lives_hooks_transfer(work->hook_closures, THREADVAR(hook_closures), FALSE);
  }

  lives_mutex_lock_carefully(&twork_mutex);
  if (!twork_first) {
    twork_first = twork_last = list;
  } else {
    if (!(attr & LIVES_THRDATTR_PRIORITY)) {
      twork_first->prev = list;
      list->next = (LiVESList *)twork_first;
      twork_first = list;
    } else {
      twork_last->next = list;
      list->prev = (LiVESList *)twork_last;
      twork_last = list;
    }
  }
  pthread_mutex_unlock(&twork_mutex);
  lives_mutex_lock_carefully(&twork_count_mutex);
  ntasks++;
  pthread_mutex_unlock(&twork_count_mutex);
  lives_mutex_lock_carefully(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  lives_mutex_lock_carefully(&pool_mutex);
  if (rnpoolthreads && ntasks >= npoolthreads) {
    if (npoolthreads < rnpoolthreads) {
      for (int i = 0; i < rnpoolthreads; i++) {
        lives_thread_data_t *tdata = get_thread_data_by_id(i + 1);
        if (tdata->exited) {
          pthread_rwlock_wrlock(&allctx_rwlock);
          allctxs = lives_list_remove_data(allctxs, tdata, TRUE);
          pthread_rwlock_unlock(&allctx_rwlock);
          pthread_join(*poolthrds[i], NULL);
          lives_free(poolthrds[i]);
          tdata = lives_thread_data_create(i + 1);
          poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
          pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
          lives_mutex_lock_carefully(&tcond_mutex);
          pthread_cond_signal(&tcond);
          pthread_mutex_unlock(&tcond_mutex);
        }
      }
    }
    if (ntasks <= rnpoolthreads) {
      lives_mutex_lock_carefully(&tcond_mutex);
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    } else {
      poolthrds =
        (pthread_t **)lives_realloc(poolthrds, (rnpoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
      for (int i = rnpoolthreads; i < rnpoolthreads + MINPOOLTHREADS; i++) {
        poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
        pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
        lives_mutex_lock_carefully(&tcond_mutex);
        pthread_cond_signal(&tcond);
        pthread_mutex_unlock(&tcond_mutex);
      }
      rnpoolthreads += MINPOOLTHREADS;
      lives_mutex_lock_carefully(&tcond_mutex);
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    npoolthreads = rnpoolthreads;
  }
  pthread_mutex_unlock(&pool_mutex);
  return 0;
}


uint64_t lives_thread_join(lives_thread_t work, void **retval) {
  thrd_work_t *task = (thrd_work_t *)work.data;
  uint64_t nthrd = 0;
  if (task->flags & LIVES_THRDFLAG_AUTODELETE) {
    LIVES_FATAL("lives_thread_join() called on an autodelete thread");
    return 0;
  }

  while (!task->busy) {
    lives_mutex_lock_carefully(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    if (!task->busy) lives_nanosleep(1000);
  }

  while (!task->done) {
    while (fg_service_fulfill()) lives_widget_context_update();
    if (!task->done) lives_nanosleep(1000);
  }
  nthrd = task->done;

  if (retval) *retval = task->ret;
  lives_free(task);
  return nthrd;
}


LIVES_GLOBAL_INLINE uint64_t lives_thread_done(lives_thread_t work) {
  thrd_work_t *task = (thrd_work_t *)work.data;
  return task->done;
}

//// hook functions

static lives_proc_thread_t _lives_hook_add(LiVESList **hooks, int type, uint64_t flags,
    hook_funcptr_t func, livespointer data, boolean is_append) {
  CACHE_THREADVARS;
  lives_proc_thread_t xlpt = NULL, lpt = NULL;
  lives_closure_t *closure;
  uint64_t xflags = flags & HOOK_UNIQUE_REPLACE_MATCH;
  boolean cull = FALSE;

  if (flags & HOOK_CB_FG_THREAD) {
    xlpt = lpt = (lives_proc_thread_t)data;
  }
  if (xflags) {
    lives_proc_thread_t lpt2 = NULL;
    LiVESList *list = hooks[type], *listnext;
    int maxp = 0;

    if (flags & HOOK_CB_FG_THREAD) {
      maxp = CTHREADVAR(hook_match_nparams);
    }
    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_STATUS_BLOCKED) continue;
      if (lpt) {
        if (!(closure->flags & HOOK_CB_FG_THREAD)) continue;
        lpt2 = (lives_proc_thread_t)data;
        if (fn_func_match(lpt, lpt2) < 0) continue;
      } else if (closure->func != func) continue;
      if (xflags == HOOK_UNIQUE_FUNC) return NULL;
      if (cull || xflags == HOOK_UNIQUE_REPLACE_FUNC) {
        if (lpt && lpt == lpt2) continue;
        if (!pthread_mutex_lock(&CTHREADVAR(hook_mutex[type]))) {
          LiVESList *xlist = hooks[type], *xlistnext;
          for (; xlist; xlist = xlistnext) {
            xlistnext = xlist->next;
            if (xlist == list) {
              hooks[type] = lives_list_remove_node(hooks[type], list, FALSE);
              break;
            }
          }
          pthread_mutex_unlock(&CTHREADVAR(hook_mutex[type]));
        }
        if (lpt2) {
          lives_proc_thread_include_states(lpt2, THRD_STATE_FINISHED);
          lives_proc_thread_free(lpt2);
        }
        continue;
      }
      if ((!lpt && closure->data == data)
          || (lpt && fn_data_match(lpt2, lpt, maxp))) {
        if (xflags == HOOK_UNIQUE_REPLACE_MATCH) {
          if (lpt && lpt == lpt2) continue;
          if (!pthread_mutex_lock(&CTHREADVAR(hook_mutex[type]))) {
            LiVESList *xlist = hooks[type], *xlistnext;
            for (; xlist; xlist = xlistnext) {
              xlistnext = xlist->next;
              if (xlist == list) {
                hooks[type] = lives_list_remove_node(hooks[type], list, FALSE);
                break;
              }
            }
            pthread_mutex_unlock(&CTHREADVAR(hook_mutex[type]));
          }
          if (lpt2) {
            lives_proc_thread_include_states(lpt2, THRD_STATE_FINISHED);
            lives_proc_thread_free(lpt2);
          } else lives_free(closure);
          continue;
        }
        if (xflags & HOOK_UNIQUE_DATA) return NULL;
      } else {
        if (!cull && (xflags == HOOK_UNIQUE_REPLACE
                      || xflags == HOOK_UNIQUE_REPLACE_OR_ADD)) {
          if (lpt) {
            if (lpt2 != lpt) {
              fn_data_replace(lpt2, lpt);
              xlpt = lpt2;
            }
          } else closure->data = data;
          cull = TRUE;
        }
      }
    }
  }
  if (cull || xflags == HOOK_UNIQUE_REPLACE) return xlpt;

  closure = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
  closure->func = func;
  closure->data = data;
  closure->flags = flags;
  if (is_append) hooks[type] = lives_list_append(hooks[type], closure);
  else hooks[type] = lives_list_append(hooks[type], closure);
  if (lpt) return xlpt;
  return NULL;
}

lives_proc_thread_t lives_hook_append(LiVESList **hooks, int type, uint64_t flags, hook_funcptr_t func,
                                      livespointer data) {
  return _lives_hook_add(hooks, type, flags, func, data, TRUE);
}

lives_proc_thread_t lives_hook_prepend(LiVESList **hooks, int type, uint64_t flags, hook_funcptr_t func,
                                       livespointer data) {
  return _lives_hook_add(hooks, type, flags, func, data, FALSE);
}


LIVES_GLOBAL_INLINE void lives_hooks_trigger(lives_object_t *obj, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_closure_t *closure;
  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return;
    if (closure->flags |= HOOK_STATUS_RUNNING);
    if (closure->flags & HOOK_CB_FG_THREAD) {
      if (!pthread_mutex_lock(&THREADVAR(hook_mutex[type]))) {
        LiVESList *zlist = xlist[type], *zlistnext;
        for (; zlist; zlist = zlistnext) {
          zlistnext = zlist->next;
          if (zlist == list) {
            xlist[type] = lives_list_remove_node(xlist[type], list, FALSE);
            break;
          }
        }
        pthread_mutex_unlock(&THREADVAR(hook_mutex[type]));
      }
      closure->tinfo = (lives_proc_thread_t)closure->data;
      if (is_fg_thread()) {
        if (closure->tinfo) {
          if (!lives_proc_thread_get_cancelled(closure->tinfo))
            fg_run_func(closure->tinfo, closure->retloc);
          lives_proc_thread_free(closure->tinfo);
        }
        lives_free(closure);
        continue;
      }

      // some functions may have been deferred, since we cannot stack multiple fg service calls
      if (!lives_proc_thread_get_cancelled(closure->tinfo))
        fg_service_call(closure->tinfo, closure->retloc);
      lives_proc_thread_include_states(closure->tinfo, THRD_STATE_FINISHED);
      lives_proc_thread_free(closure->tinfo);
      lives_free(closure);
      continue;
    }
    if (closure->flags & HOOK_CB_ASYNC_JOIN) {
      closure->tinfo = lives_proc_thread_create(LIVES_THRDATTR_NO_GUI | LIVES_THRDATTR_PRIORITY,
                       (lives_funcptr_t)closure->func, WEED_SEED_VOIDPTR,
                       "vv", obj, closure->data);
    } else if (closure->flags & HOOK_CB_ASYNC) {
      closure->tinfo = lives_proc_thread_create(0, (lives_funcptr_t)closure->func, 0, "vv", obj, closure->data);
    } else (*closure->func)(obj, closure->data);
    if (closure->flags & HOOK_CB_SINGLE_SHOT && !(closure->flags & HOOK_CB_ASYNC_JOIN))
      xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
  }
}



LIVES_GLOBAL_INLINE void lives_hook_remove(LiVESList **xlist, int type, hook_funcptr_t func, livespointer data) {
  // do not call for HOOK_CB_SINGLE_SHOT (TODO)
  LiVESList *list = xlist[type];
  lives_closure_t *closure;
  for (; list; list = list->next) {
    closure = (lives_closure_t *)list->data;
    if (closure->func == func && closure->data == data) {
      closure->flags |= HOOK_STATUS_BLOCKED;
      lives_nanosleep_until_zero(closure->tinfo);
      pthread_mutex_lock(&THREADVAR(hook_mutex[type]));
      xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
      pthread_mutex_unlock(&THREADVAR(hook_mutex[type]));
      return;
    }
  }
}

LIVES_GLOBAL_INLINE void lives_hooks_join(LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_closure_t *closure;
  for (; list; list = listnext) {
    closure = (lives_closure_t *)list->data;
    listnext = list->next;
    if (closure->tinfo) {
      lives_proc_thread_join(closure->tinfo);
      closure->tinfo = NULL;
      if (closure->flags & HOOK_CB_SINGLE_SHOT) {
        if (!pthread_mutex_lock(&THREADVAR(hook_mutex[type]))) {
          LiVESList *zlist = xlist[type], *zlistnext;
          for (; zlist; zlist = zlistnext) {
            zlistnext = zlist->next;
            if (zlist == list) {
              xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
              break;
            }
          }
          pthread_mutex_unlock(&THREADVAR(hook_mutex[type]));
        }
      }
    }
  }
}


LIVES_GLOBAL_INLINE uint8_t get_typecode(char c) {
  switch (c) {
  case 'i': return 0x01;
  case 'd': return 0x02;
  case 'b': return 0x03;
  case 's': return 0x04;
  case 'I': return 0x05;
#ifdef LIVES_SEED_FLOAT
  case 'f': return LIVES_SEED_FLOAT; // 0x06
#endif
  case 'F': return 0x0C;
  case 'v':
  case 'V': return 0x0D;
  case 'p':
  case 'P': return 0x0E;
  default: return 0x0F;
  }
}


LIVES_GLOBAL_INLINE uint32_t get_seedtype(char c) {
  switch (c) {
  case 0x01:
  case 'i': return WEED_SEED_INT;
  case 0x02:
  case 'd': return WEED_SEED_DOUBLE;
  case 0x03:
  case 'b': return WEED_SEED_BOOLEAN;
  case 0x04:
  case 's': return WEED_SEED_STRING;
  case 0x05:
  case 'I': return WEED_SEED_INT64;

#ifdef LIVES_SEED_FLOAT
  case 'f': return LIVES_SEED_FLOAT;
#endif

  case 0x0C:
  case 'F': return WEED_SEED_FUNCPTR;
  case 0x0D:
  case 'v':
  case 'V': return WEED_SEED_VOIDPTR;
  case 0x0E:
  case 'p':
  case 'P': return WEED_SEED_PLANTPTR;
  default: return WEED_SEED_INVALID;
  }
}


LIVES_GLOBAL_INLINE funcsig_t funcsig_from_args_fmt(const char *args_fmt) {
  funcsig_t fsig = 0;
  if (args_fmt) {
    char c;
    for (int i = 0; (c = args_fmt[i]); i++) {
      fsig <<= 4;
      fsig |= get_typecode(c);
    }
  }
  return fsig;
}


char *funcsig_to_string(funcsig_t sig) {
  char *fmtstring = "", *tmp;
  if (!sig) return "void";
  for (int i = 60; i >= 0; i -= 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) break;
    tmp = lives_strdup_printf("%s%s%s",
                              weed_seed_to_ctype(get_seedtype(ch), FALSE),
                              i == 60 ? "" : ",", fmtstring);
    lives_free(fmtstring);
    fmtstring = tmp;
  }
  return fmtstring;
}


LIVES_GLOBAL_INLINE lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
    uint32_t return_type,  const char *args_fmt,
    const char *file, int line, void *data) {
  lives_funcdef_t *fdef = (lives_funcdef_t *)lives_malloc(sizeof(lives_funcdef_t));
  if (fdef) {
    if (funcname) fdef->funcname = lives_strdup(funcname);
    else fdef->funcname = NULL;
    fdef->uid = gen_unique_id();
    fdef->function = function;
    fdef->return_type = return_type;
    if (args_fmt) fdef->args_fmt = lives_strdup(args_fmt);
    else fdef->args_fmt = NULL;
    fdef->file = lives_strdup(file);
    fdef->line = line;
    fdef->data = data;
  }
  return fdef;
}


LIVES_GLOBAL_INLINE void free_funcdef(lives_funcdef_t *fdef) {
  if (fdef) {
    lives_freep((void **)&fdef->funcname);
    lives_freep((void **)&fdef->args_fmt);
    lives_free(fdef);
  }
}


LIVES_LOCAL_INLINE lives_funcinst_t *create_funcinst_valist(lives_funcdef_t *template, va_list xargs) {
  lives_funcinst_t *finst = lives_plant_new(LIVES_WEED_SUBTYPE_FUNCINST);
  if (finst) {
    lives_plant_params_from_vargs(finst, template->function,
                                  template->return_type ? template->return_type : -1,
                                  template->args_fmt, xargs);
    weed_set_voidptr_value(finst, LIVES_LEAF_TEMPLATE, template);
  }
  return finst;
}

LIVES_LOCAL_INLINE lives_funcinst_t *create_funcinst_nullvalist(lives_funcdef_t *template) {
  lives_funcinst_t *finst = lives_plant_new(LIVES_WEED_SUBTYPE_FUNCINST);
  if (finst) {
    lives_plant_params_from_nullvargs(finst, template->function,
                                      template->return_type ? template->return_type : -1);
    weed_set_voidptr_value(finst, LIVES_LEAF_TEMPLATE, template);
  }
  return finst;
}


LIVES_GLOBAL_INLINE lives_funcinst_t *create_funcinst(lives_funcdef_t *template, void *retstore, ...) {
  lives_funcinst_t *finst = NULL;
  if (template) {
    if (template->args_fmt) {
      va_list xargs;
      va_start(xargs, retstore);
      finst = create_funcinst_valist(template, xargs);
      va_end(xargs);
    } else finst = create_funcinst_nullvalist(template);
  }
  return finst;
}


LIVES_GLOBAL_INLINE void free_funcinst(lives_funcinst_t *finst) {
  if (finst) {
    lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL);
    if (fdef) free_funcdef(fdef);
    lives_free(finst);
  }
}


boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp) {
  if (!lpt1 || !lpt2 || weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)
      != weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)) return FALSE;
  else {
    funcsig_t fsig1 = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
    funcsig_t fsig2 = weed_get_int64_value(lpt2, LIVES_LEAF_FUNCSIG, NULL);
    if (fsig1 != fsig2) return FALSE;
    int nparms = get_funcsig_nparms(fsig1);
    if (maxp && nparms > maxp) nparms = maxp;
    for (int i = 0; i < nparms; i++) {
      char *pname = lpt_param_name(i);
      if (weed_leaf_elements_equate(lpt1, pname, lpt2, pname, -1) == WEED_FALSE) {
        lives_free(pname);
        return FALSE;
      }
      lives_free(pname);
    }
  }
  return TRUE;
}


// return -ve value on mismatch, or nparams on match
// -1 == NULL or fn mismatch
// -2 == return_type mismatch
// -3 == arg_fmt mismatch
int fn_func_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2) {
  if (!lpt1 || !lpt2 || weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)
      != weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)) return -1;
  if (weed_leaf_seed_type(lpt1, _RV_) != weed_leaf_seed_type(lpt2, _RV_)) return -2;
  else {
    funcsig_t fsig = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
    if (fsig != weed_get_int64_value(lpt2, LIVES_LEAF_FUNCSIG, NULL)) return -3;
    return get_funcsig_nparms(fsig);
  }
}


boolean fn_data_replace(lives_proc_thread_t src, lives_proc_thread_t dst) {
  int nparms = fn_func_match(src, dst);
  if (nparms >= 0) {
    for (int i = 0; i < nparms; i++) {
      char *pname = lpt_param_name(i);
      weed_leaf_dup(dst, src, pname);
      lives_free(pname);
    }
    return TRUE;
  }
  return FALSE;
}

///////////// refcounting ///////////////

LIVES_GLOBAL_INLINE boolean check_refcnt_init(lives_refcounter_t *refcount) {
  if (refcount) {
    if (!refcount->mutex_inited) {
      pthread_mutexattr_t mattr;
      pthread_mutexattr_init(&mattr);
      pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
      refcount->mutex_inited = TRUE;
      pthread_mutex_init(&refcount->mutex, &mattr);
    }
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE int refcount_inc(lives_refcounter_t *refcount) {
  if (check_refcnt_init(refcount)) {
    int count;
    pthread_mutex_lock(&refcount->mutex);
    count = ++refcount->count;
    pthread_mutex_unlock(&refcount->mutex);
    return count;
  }
  return -50;
}


LIVES_GLOBAL_INLINE int refcount_dec(lives_refcounter_t *refcount) {
  if (check_refcnt_init(refcount)) {
    int count;
    pthread_mutex_lock(&refcount->mutex);
    count = --refcount->count;
    if (count != -1) pthread_mutex_unlock(&refcount->mutex);
    if (count < -1) break_me("double unref");
    return count;
  }
  return -50;
}


LIVES_GLOBAL_INLINE int refcount_query(obj_refcounter *refcount) {
  if (check_refcnt_init(refcount)) {
    int count;
    pthread_mutex_lock(&refcount->mutex);
    count = refcount->count;
    pthread_mutex_unlock(&refcount->mutex);
    return count;
  }
  return -50;
}


LIVES_GLOBAL_INLINE int weed_refcount_inc(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) weed_add_refcounter(plant);
    return refcount_inc(refcnt);
  }
  return -200;
}


LIVES_GLOBAL_INLINE int weed_refcount_dec(weed_plant_t *plant) {
  // value of -1 indicates the plant should be free with weed_plant_free()
  // the mutex lock is held in this case, so caller must unlock it before freeing, by
  // calling weed_refcounter_unlock()
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) weed_add_refcounter(plant);
    return refcount_dec(refcnt);
  }
  return -200;
}


LIVES_GLOBAL_INLINE int weed_refcount_query(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return -100;
    return refcount_query(refcnt);
  }
  return -200;
}


LIVES_GLOBAL_INLINE void refcount_unlock(lives_refcounter_t *refcnt) {
  if (refcnt) {
    pthread_mutex_trylock(&refcnt->mutex);
    pthread_mutex_unlock(&refcnt->mutex);
  }
}


LIVES_GLOBAL_INLINE void weed_refcounter_unlock(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (refcnt) {
      refcount_unlock(refcnt);
    }
  }
}


LIVES_GLOBAL_INLINE boolean weed_add_refcounter(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcount;
    if (weed_plant_has_leaf(plant, LIVES_LEAF_REFCOUNTER)) return TRUE;
    refcount = (lives_refcounter_t *)lives_calloc(1, sizeof(lives_refcounter_t));
    if (refcount) {
      refcount->mutex_inited = FALSE;
      weed_set_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, refcount);
      weed_leaf_set_autofree(plant, LIVES_LEAF_REFCOUNTER, TRUE);
      check_refcnt_init(refcount);
      // autofree will clear this
      weed_leaf_set_flagbits(plant, LIVES_LEAF_REFCOUNTER, WEED_FLAG_UNDELETABLE);
      return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean weed_remove_refcounter(weed_plant_t *plant) {
  if (plant && weed_plant_has_leaf(plant, LIVES_LEAF_REFCOUNTER)) {
    weed_leaf_autofree(plant, LIVES_LEAF_REFCOUNTER);
    if (weed_leaf_delete(plant, LIVES_LEAF_REFCOUNTER) == WEED_SUCCESS) return TRUE;
  }
  return FALSE;
}


char *get_threadstats(void) {
  char *msg = lives_strdup_printf("Total threads in use: %d, active threads\n\n", rnpoolthreads);
  pthread_rwlock_rdlock(&allctx_rwlock);
  for (LiVESList *list = allctxs; list; list = list->next) {
    char *notes = NULL;
    lives_threadvars_t *thrdinfo = (lives_threadvars_t *)list->data;
    lives_proc_thread_t tinfo = thrdinfo->var_tinfo;
    if (tinfo) {
      thrd_work_t *work = (thrd_work_t *)weed_get_voidptr_value(tinfo, LIVES_LEAF_THREAD_WORK, NULL);
      if (pthread_equal(thrdinfo->var_self, capable->gui_thread)) notes = lives_strdup("GUI thread");
      if (pthread_equal(thrdinfo->var_self, capable->gui_thread)) notes = lives_strdup("me !");
      msg = lives_strdup_concat(msg, "\n", "Thread %d (%s) is %d", thrdinfo->var_id, notes,
                                work ? "busy" : "idle");
      if (notes) lives_free(notes);
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return msg;
}

