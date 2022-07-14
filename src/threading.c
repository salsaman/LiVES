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


static pthread_mutex_t proc_thread_kill_mutex = PTHREAD_MUTEX_INITIALIZER;

static boolean sync_hooks_done(lives_obj_t *obj, void *data) {
  boolean *b = (boolean *)data;
  *b = TRUE;
  return FALSE;
}

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

boolean thread_wait_loop(lives_proc_thread_t lpt, thrd_work_t *work, int hook_type, boolean wake_gui) {
  ticks_t timeout;
  uint64_t ltimeout;
  LiVESList **hook_closures;
  uint64_t exclude_busy = THRD_STATE_BUSY, exclude_blocked = THRD_STATE_BLOCKED, exclude_waiting = 0;
  int64_t msec = 0, blimit;
  boolean ws_hooks_done = FALSE;

  if (hook_type == WAIT_SYNC_HOOK) {
    if (work && (work->flags & LIVES_THRDFLAG_IGNORE_SYNCPT) == LIVES_THRDFLAG_IGNORE_SYNCPT)
      return TRUE;
  }

  blimit = THREADVAR(blocked_limit);
  timeout = THREADVAR(sync_timeout);
  ltimeout = labs(timeout);

  if (lives_proc_thread_has_states(lpt, THRD_STATE_BUSY)) exclude_busy = 0;

  if (lives_proc_thread_has_states(lpt, THRD_STATE_BLOCKED)) exclude_blocked = 0;

  if (work && hook_type == WAIT_SYNC_HOOK) {
    exclude_waiting = THRD_STATE_WAITING;
    work->sync_ready = FALSE;
    hook_closures = work->hook_closures;
  } else hook_closures = THREADVAR(hook_closures);

  if (work && hook_type == WAIT_SYNC_HOOK) {
    lives_proc_thread_include_states(lpt, THRD_STATE_WAITING | THRD_STATE_BUSY);
  } else if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_BUSY);

  lives_hooks_trigger_async_sequential(lpt, hook_closures, WAIT_SYNC_HOOK, sync_hooks_done, &ws_hooks_done);

  while (!ws_hooks_done) {
    lives_nanosleep(10000);
    if (wake_gui) g_main_context_wakeup(NULL);
    msec++;
    if (timeout && msec >= ltimeout) {
      lives_proc_thread_exclude_states(lpt, exclude_waiting | exclude_busy | exclude_blocked);
      if (work) work->sync_ready = TRUE;
      if (timeout < 0) {
        lives_proc_thread_include_states(lpt, THRD_STATE_TIMED_OUT);
      }
      return FALSE;
    }
    if (msec >= blimit && exclude_blocked) {
      lives_proc_thread_include_states(lpt, THRD_STATE_BLOCKED);
    }
  }
  lives_proc_thread_exclude_states(lpt, exclude_waiting | exclude_busy | exclude_blocked);
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


static boolean waitsyncready(lives_obj_t *obj, void *werk) {
  thrd_work_t *work = (thrd_work_t *)werk;
  return !work || work->sync_ready;
}


boolean sync_point(const char *motive) {
  lives_proc_thread_t lpt = THREADVAR(tinfo);
  boolean bval = FALSE;

  if (lpt) {
    thrd_work_t *work = lives_proc_thread_get_work(lpt);
    if (work) {
      if (motive) THREADVAR(sync_motive) = lives_strdup(motive);
      lives_hook_append(NULL, WAIT_SYNC_HOOK, 0, waitsyncready, work);
      bval = thread_wait_loop(NULL, work, WAIT_SYNC_HOOK, FALSE);
      lives_hook_remove(THREADVAR(hook_closures), WAIT_SYNC_HOOK, waitsyncready, work, THREADVAR(hook_mutex));
      if (motive) {
        lives_free(THREADVAR(sync_motive));
        THREADVAR(sync_motive) = NULL;
      }
    }
  }
  g_print("sync ready\n");
  return bval;
}


static void run_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr);


int proc_thread_kill_lock(void) {return pthread_mutex_lock(&proc_thread_kill_mutex);}

int proc_thread_kill_unlock(void) {return pthread_mutex_unlock(&proc_thread_kill_mutex);}


// return TRUE if freed
LIVES_GLOBAL_INLINE boolean lives_proc_thread_free(lives_proc_thread_t lpt) {
  if (!lpt) return TRUE;

  // wait for the thread to finish - either completing or being cancelled
  while (!lives_proc_thread_is_done(lpt)) {
    if (is_fg_thread()) while (fg_service_fulfill());
    lives_nanosleep(100000);
  }

  if (weed_refcount_dec(lpt) == -1) {
    pthread_mutex_t *state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    pthread_mutex_t *destruct_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);
    thrd_work_t *work = lives_proc_thread_get_work(lpt);
    LiVESList **hook_closures = (LiVESList **)weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_CLOSURES, NULL);

    weed_refcounter_unlock(lpt);

    lives_hooks_trigger(lpt, hook_closures, FINAL_HOOK);
    lives_hooks_clear_all(hook_closures, N_HOOK_POINTS);
    if (lpt == mainw->lazy_starter) abort();
    // after locking proc_thread_kill_mutex, you can be sure that no proc_threads_will be killed
    // while holding it
    proc_thread_kill_lock();
    pthread_mutex_lock(state_mutex);
    weed_plant_free(lpt);
    pthread_mutex_unlock(state_mutex);
    lives_free(state_mutex);
    if (work) work->lpt = NULL;
    THREADVAR(tinfo) = NULL;
    proc_thread_kill_unlock();

    if (!pthread_mutex_trylock(destruct_mutex)) {
      pthread_mutex_unlock(destruct_mutex);
      lives_free(destruct_mutex);
    }

    return TRUE;
  }
  return FALSE;
}


LIVES_LOCAL_INLINE void lives_plant_params_from_nullvargs(weed_plant_t *info, lives_funcptr_t func, int return_type) {
  weed_set_funcptr_value(info, LIVES_LEAF_THREADFUNC, func);
  // set the type of the return_value, but not the return_value itself yet
  if (return_type > 0) weed_leaf_set(info, _RV_, return_type, 0, NULL);
  else if (return_type == -1) lives_proc_thread_include_states(info, THRD_OPT_NOTIFY);
  weed_set_int64_value(info, LIVES_LEAF_FUNCSIG, FUNCSIG_VOID);
}


static void lives_plant_params_from_vargs(weed_plant_t *info, lives_funcptr_t func, int return_type,
    const char *args_fmt, va_list xargs) {
  if (args_fmt && *args_fmt) {
    const char *c;
    int p = 0;
    weed_set_funcptr_value(info, LIVES_LEAF_THREADFUNC, func);
    if (return_type > 0) weed_leaf_set(info, _RV_, return_type, 0, NULL);
    else if (return_type == -1) lives_proc_thread_include_states(info, THRD_OPT_NOTIFY);
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
      case 'P': case 'p' : weed_set_plantptr_value(info, pkey, va_arg(xargs, weed_plantptr_t)); break;
      default: lives_proc_thread_free(info); return;
      }
      lives_free(pkey);
    }
    weed_set_int64_value(info, LIVES_LEAF_FUNCSIG, funcsig_from_args_fmt(args_fmt));
  } else lives_plant_params_from_nullvargs(info, func, return_type);
}


static lives_proc_thread_t lives_proc_thread_run(lives_thread_attr_t attr, lives_proc_thread_t thread_info,
    uint32_t return_type) {
  // make proc_threads joinable and if not FG. run it
  if (thread_info) {
    weed_set_int64_value(thread_info, LIVES_LEAF_THREAD_ATTRS, attr);
    if (!(attr & LIVES_THRDATTR_FG_THREAD)) {
      if (attr & LIVES_THRDATTR_NOTE_STTIME) weed_set_int64_value(thread_info, LIVES_LEAF_START_TICKS,
            lives_get_current_ticks());
      // add to the pool
      run_proc_thread(thread_info, attr);
      if (!return_type) return NULL;
    } else lives_proc_thread_include_states(thread_info, THRD_STATE_UNQUEUED);
  }
  return thread_info;
}


static lives_proc_thread_t add_garnish(lives_proc_thread_t lpt, const char *fname) {
  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL)) {
    pthread_mutex_t *state_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(state_mutex, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, state_mutex);
  }
  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL)) {
    pthread_mutex_t *destruct_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(destruct_mutex, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, destruct_mutex);
  }
  LiVESList **hook_callbacks = (LiVESList **)lives_calloc(N_HOOK_POINTS, sizeof(LiVESList *));
  pthread_mutex_t *hook_mutex = (pthread_mutex_t *)lives_calloc(N_HOOK_POINTS, sizeof(pthread_mutex_t));
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    pthread_mutex_init(&hook_mutex[i], NULL);
    hook_callbacks[i] = NULL;
  }
  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_CLOSURES, (void *)hook_callbacks);
  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_MUTEXES, (void *)hook_mutex);
  weed_set_string_value(lpt, LIVES_LEAF_FUNC_NAME, (void *)fname);
  return lpt;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_new(const char *fname) {
  lives_proc_thread_t lpt = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  return add_garnish(lpt, fname);
}


lives_proc_thread_t _lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type,
    const char *args_fmt, va_list xargs) {
  lives_proc_thread_t thread_info = lives_proc_thread_new(fname);
  lives_plant_params_from_vargs(thread_info, func, return_type, args_fmt, xargs);
  break_me("lptc");
  return lives_proc_thread_run(attr, thread_info, return_type);
}

lives_proc_thread_t _lives_proc_thread_create_nullvargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type) {
  lives_proc_thread_t thread_info = lives_proc_thread_new(fname);
  lives_plant_params_from_nullvargs(thread_info, func, return_type);
  return lives_proc_thread_run(attr, thread_info, return_type);
}


lives_proc_thread_t lives_proc_thread_create_from_funcinst(lives_thread_attr_t attr, lives_funcinst_t *finst) {
  // for future use, eg. finst = create_funcinst(fdef, ret_loc, args...)
  // proc_thread = -this-(finst)
  if (finst) {
    lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL);
    add_garnish(finst, fdef->funcname);
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
lives_proc_thread_t _lives_proc_thread_create(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  if (args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    lpt = _lives_proc_thread_create_vargs(attr, func, fname, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attr, func, fname, return_type);
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
lives_proc_thread_t _lives_proc_thread_create_with_timeout_vargs(ticks_t timeout,
    lives_thread_attr_t attr, lives_funcptr_t func,
    const char *func_name, int return_type,
    const char *args_fmt, va_list xargs) {
  lives_alarm_t alarm_handle;
  lives_proc_thread_t lpt;
  ticks_t xtimeout = 1;
  lives_cancel_t cancel = CANCEL_NONE;
  int xreturn_type = return_type;

  if (xreturn_type == 0) xreturn_type--;

  if (args_fmt) {
    lpt = _lives_proc_thread_create_vargs(attr, func, func_name, xreturn_type, args_fmt, xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attr, func, func_name, xreturn_type);

  mainw->cancelled = CANCEL_NONE;

  alarm_handle = lives_alarm_set(timeout);

  while (!lives_proc_thread_check_finished(lpt)
         && (timeout == 0 || (xtimeout = lives_alarm_check(alarm_handle)) > 0)) {
    lives_nanosleep(LIVES_QUICK_NAP);

    if (is_fg_thread()) if (!fg_service_fulfill()) lives_widget_context_iteration(NULL, FALSE);

    if (lives_proc_thread_has_states(lpt, THRD_STATE_BUSY)) {
      // thread MUST unavoidably block; stop the timer (e.g showing a UI)
      // user or other condition may set cancelled
      if ((cancel = mainw->cancelled) != CANCEL_NONE) break;
      lives_alarm_reset(alarm_handle, timeout);
    }
  }

  lives_alarm_clear(alarm_handle);
  if (xtimeout == 0) {
    if (!lives_proc_thread_is_done(lpt)) {
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
    lives_proc_thread_dontcare(lpt);
    return NULL;
  }
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_with_timeout(ticks_t timeout, lives_thread_attr_t attr, lives_funcptr_t func,
    const char *funcname, int return_type,
    const char *args_fmt, ...) {
  lives_proc_thread_t ret;
  va_list xargs;
  va_start(xargs, args_fmt);
  ret = _lives_proc_thread_create_with_timeout_vargs(timeout, attr, func, funcname, return_type, args_fmt, xargs);
  va_end(xargs);
  return ret;
}


boolean is_fg_thread(void) {
  if (!mainw || !capable || !capable->gui_thread) return TRUE;
  return !THREADVAR(idx);
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
  uint64_t filtflags = HOOK_CB_FG_THREAD | xtraflags;
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  if (THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
    return lives_hook_prepend(THREADVAR(hook_closures), INTERNAL_HOOK_0,
                              filtflags, NULL, (void *)lpt);
  else
    return lives_hook_append(THREADVAR(hook_closures), INTERNAL_HOOK_0,
                             filtflags, NULL, (void *)lpt);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_fg_deferral_stack(uint64_t xtraflags,
    lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_FG_THREAD | xtraflags;
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  if (THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
    return lives_hook_append(FG_THREADVAR(hook_closures), INTERNAL_HOOK_0,
                             filtflags, NULL, (void *)lpt);
  else
    return lives_hook_append(FG_THREADVAR(hook_closures), INTERNAL_HOOK_0,
                             filtflags, NULL, (void *)lpt);
}


static boolean _main_thread_execute_vargs(lives_funcptr_t func, const char *fname, int return_type,
    void *retval, const char *args_fmt, va_list xargs) {
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
  boolean is_fg_service = FALSE;
  // create a lives_proc_thread, which will either be run directly (fg thread)
  // passed to the fg thread
  // or queued for sequential execution (since we need to avoid nesting calls)
  if (args_fmt && *args_fmt) {
    lpt = _lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, func, fname, return_type, args_fmt, xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(LIVES_THRDATTR_FG_THREAD, func, fname, return_type);

  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  if (is_fg_thread()) {
    // run direct
    fg_run_func(lpt, retval);
    lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
    lives_proc_thread_free(lpt);
    lives_hooks_trigger(NULL, THREADVAR(hook_closures), INTERNAL_HOOK_0);
  } else {
    if (!is_fg_service) {
      if (get_gov_status() != GOV_RUNNING && FG_THREADVAR(fg_service)) {
        if (THREADVAR(hook_hints) & HOOK_CB_BLOCKING) weed_refcount_inc(lpt);
        if (add_to_fg_deferral_stack(FG_THREADVAR(hook_flag_hints), lpt) != lpt) {
          lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
          if (THREADVAR(hook_hints) & HOOK_CB_BLOCKING) weed_refcount_dec(lpt);
          lives_proc_thread_free(lpt);
        } else {
          if (THREADVAR(hook_hints) & HOOK_CB_BLOCKING) {
            while (!lives_proc_thread_check_finished(lpt)
                   && !lives_proc_thread_get_cancelled(lpt)) {
              if (get_gov_status() == GOV_RUNNING) {
                mainw->clutch = FALSE;
                lives_nanosleep_until_nonzero(mainw->clutch);
              } else lives_nanosleep(1000);
            }
            weed_refcount_dec(lpt);
            lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
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
        lives_hooks_trigger(NULL, THREADVAR(hook_closures), INTERNAL_HOOK_0);
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


boolean _main_thread_execute(lives_funcptr_t func, const char *fname, int return_type,
                             void *retval, const char *args_fmt, ...) {
  boolean bret;
  va_list xargs;
  if (!args_fmt || !*args_fmt) return _main_thread_execute_vargs(func, fname, return_type, retval, "", NULL);
  va_start(xargs, args_fmt);
  bret = _main_thread_execute_vargs(func, fname, return_type, retval, args_fmt, xargs);
  va_end(xargs);
  return bret;
}


boolean _main_thread_execute_rvoid(lives_funcptr_t func, const char *fname, const char *args_fmt, ...) {
  boolean bret;
  va_list xargs;
  if (!args_fmt || !*args_fmt) return _main_thread_execute_vargs(func, fname, 0, NULL, "", NULL);
  va_start(xargs, args_fmt);
  bret = _main_thread_execute_vargs(func, fname, 0, NULL, args_fmt, xargs);
  va_end(xargs);
  return bret;
}


boolean _main_thread_execute_pvoid(lives_funcptr_t func, const char *fname, int return_type, void *retloc) {

  _main_thread_execute(func, fname, return_type, retloc, "", NULL);
}


static char *make_pdef(funcsig_t sig) {
  return lives_strdup("type p0, p1; type2 p2; etc..");
}

static char *make_pfree(funcsig_t sig) {
  return lives_strdup("lives_free(p0); etc for each char *pn..");
}


int get_funcsig_nparms(funcsig_t sig) {
  int nparms = 0;
  for (funcsig_t test = 0x1; test <= sig; test <<= 4) nparms++;
  return nparms;
}


LIVES_LOCAL_INLINE lives_funcdef_t *lives_proc_thread_define_func(lives_proc_thread_t lpt) {
  lives_funcdef_t *fdef;
  char *funcname, *args_fmt;
  if (!lpt) return NULL;
  funcname = weed_get_string_value(lpt, LIVES_LEAF_FUNC_NAME, 0);
  args_fmt = args_fmt_from_funcsig(weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL));
  fdef = create_funcdef(funcname, weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL),
                        weed_leaf_seed_type(lpt, _RV_), args_fmt, NULL, 0, NULL);
  lives_free(funcname);
  lives_free(args_fmt);
  return fdef;
}


#define DEBUG_FN_CALLBACKS
static boolean _call_funcsig_inner(lives_proc_thread_t lpt, lives_funcptr_t func,
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
#ifdef DEBUG_FN_CALLBACKS
  lives_closure_t *closure;
  const lives_funcdef_t *funcdef;
#endif

  if (!lpt) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL proc_thread");
    return FALSE;
  }
  info = lpt;

  thefunc->func = func;

#ifdef DEBUG_FN_CALLBACKS
  closure = weed_get_voidptr_value(lpt, "closure", NULL);
  if (closure) {
    funcdef = closure->fdef;
    if (funcdef) {
      msg = lives_funcdef_explain(funcdef);
      g_print("\ncall_funcsig nparms = %d, funcsig (0X%016lx) called from thread %d, "
              "at current time t + %.4f, with target:\n%s\n", nparms, sig, THREADVAR(idx),
              lives_get_session_time(), msg);
      lives_free(msg);
    } else g_print("func with no fdef !!\n");
  }
#endif

  lives_proc_thread_include_states(info, THRD_STATE_RUNNING);

  // Note: C compilers don't necessarily care about the type / number of function args.,
  // or what the function returns, (else it would be impossible to alias any function pointer)
  // e.g ((generic_func_t) f)(parameters........)
  // the catch is that thhe compiler does need to know the types of the variables being passed as parameters
  // hence we cannot create a macro that just returns a value of the correct type and use this to fill in
  // the function parameters. For this reason, the number and types of parameters have to be defined as literas
  // which is the rason for the switch
  //

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
  /* #define DO_CALL(n, ...) do {if (ret_type) XCALL_##n(__VA_ARGS__); \ */
  /*   else XCALL_VOID_##n(__VA_ARGS__);} while (0); */

  // unfortunately, the parameter TYPES need to be known at compile time, so we cant just call a single function

  // and put in the parameters at runtime


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
    case FUNCSIG_PLANTP_VOIDP_INT_FUNCP_VOIDP: {
      weed_plant_t *p0; void *p1, *p4; int p2; weed_funcptr_t p3;
      DO_CALL(5, plantptr, voidptr, int, funcptr, voidptr);
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

  if (err == WEED_SUCCESS) {
    return TRUE;
  }
  msg = lives_strdup_printf("Got error %d running prothread ", err);
  goto funcerr2;

funcerr:
  // invalid args_fmt
  if (1) {
    char *symstr = funcsig_to_symstring(sig);
    char *pdef = make_pdef(sig);
    char *plist = funcsig_to_param_string(sig);
    char *pfree = make_pfree(sig);
    msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu)\n"
                              "Please add a line in threading.h:\n\n"
                              "#define FUNCSIG_%s\t\t0X%016lX\n\n"
                              "and in threading.c, function _call_funcsig_inner,\n"
                              "locate the switch section for %d parameters "
                              "and add:\n\n case FUNCSIG_%s: {\n\t%s\n"
                              "\tDO_CALL(%d, %s)\n\t%s\n\t} break;\n"
                              , sig, sig, symstr, sig, nparms, symstr, pdef, nparms, plist, pfree);
    lives_free(symstr); lives_free(pdef); lives_free(plist); lives_free(pfree);
  }
  lives_proc_thread_include_states(info, THRD_STATE_INVALID);
funcerr2:
  if (!msg) {
    lives_proc_thread_include_states(info, THRD_STATE_ERROR);
    msg = lives_strdup_printf("Got error %d running function with type 0x%016lX (%lu)", err, sig, sig);
  }
  LIVES_FATAL(msg);
  lives_free(msg);
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t lpt) {
  weed_funcptr_t func = weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  if (func) {
    uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);
    funcsig_t sig = weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL);
    return _call_funcsig_inner(lpt, func, ret_type, sig);
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


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_has_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  return (lives_proc_thread_check_states(lpt, state_bits) == state_bits);
}

static boolean get_unpaused(void *obj, void *xlpt) {
  lives_proc_thread_t lpt = (lives_proc_thread_t)xlpt;
  if (!lives_proc_thread_has_states(lpt, THRD_STATE_RESUME_REQUESTED)) return FALSE;
  return TRUE;
}


void lives_proc_thread_resume(lives_proc_thread_t self) {
  if (self) {
    LiVESList **hook_closures = (LiVESList **)weed_get_voidptr_value(self, LIVES_LEAF_HOOK_CLOSURES, NULL);
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSED | THRD_STATE_RESUME_REQUESTED);
    lives_hooks_trigger(self, hook_closures, RESUMING_HOOK);
    if (lives_proc_thread_has_states(self, THRD_STATE_UNQUEUED)) run_proc_thread(self, 0);
  }
}


LIVES_GLOBAL_INLINE void lives_proc_thread_pause(lives_proc_thread_t self) {
  // self function to called for pausable proc_threads when pause is requested
  // for idle / loop funcs, do not call this, the thread will exit unqueed instead
  // and be resubmitted on resume
  if (self) {
    LiVESList **hook_closures = (LiVESList **)weed_get_voidptr_value(self, LIVES_LEAF_HOOK_CLOSURES, NULL);
    lives_proc_thread_include_states(self, THRD_STATE_PAUSED);
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);
    if (lives_proc_thread_has_states(self, THRD_OPT_IDLEFUNC)) {
      lives_proc_thread_hook_append(self, WAIT_SYNC_HOOK, 0, get_unpaused, self);
      lives_hooks_trigger(self, hook_closures, PAUSED_HOOK);
      sync_point("paused");
      lives_proc_thread_resume(self);
    }
  }
}


LIVES_GLOBAL_INLINE void lives_proc_thread_wait_sync(lives_proc_thread_t self) {
  // internal function for lives_proc_threads. Will block until all hooks have returned
  if (self) {
    LiVESList **hook_closures = (LiVESList **)weed_get_voidptr_value(self, LIVES_LEAF_HOOK_CLOSURES, NULL);
    lives_proc_thread_include_states(self, THRD_STATE_WAITING);
    lives_hooks_trigger(self, hook_closures, WAIT_SYNC_HOOK);
  }
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      LiVESList **hook_closures = (LiVESList **)weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_CLOSURES, NULL);
      uint64_t tstate;

      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | state_bits);
      pthread_mutex_unlock(state_mutex);

      if ((state_bits & THRD_STATE_PREPARING) && !(tstate & THRD_STATE_PREPARING)) {
        lives_hooks_trigger(lpt, hook_closures, TX_PREPARING_HOOK);
      }

      if ((state_bits & THRD_STATE_RUNNING) && !(tstate & THRD_STATE_RUNNING)) {
        if (tstate & THRD_STATE_PREPARING) lives_hooks_trigger(lpt, hook_closures, TX_START_HOOK);
        lives_hooks_trigger(lpt, hook_closures, TX_START_HOOK);
      }

      if ((state_bits & THRD_STATE_FINISHED) && !(tstate & THRD_STATE_FINISHED)) {
        lives_hooks_trigger(lpt, hook_closures, TX_FINISHED_HOOK);
      }

      if ((state_bits & THRD_STATE_WAITING) && !(tstate & THRD_STATE_WAITING)) {
        lives_hooks_trigger(lpt, hook_closures, TX_SYNC_WAIT_HOOK);
      }

      if ((state_bits & THRD_STATE_BLOCKED) && !(tstate & THRD_STATE_BLOCKED)) {
        lives_hooks_trigger(lpt, hook_closures, TX_BLOCKED_HOOK);
      }

      if ((state_bits & THRD_STATE_TIMED_OUT) && !(tstate & THRD_STATE_TIMED_OUT)) {
        lives_hooks_trigger(lpt, hook_closures, TX_TIMED_OUT_HOOK);
      }

      if ((state_bits & THRD_STATE_ERROR) && !(tstate & THRD_STATE_ERROR)) {
        lives_hooks_trigger(lpt, hook_closures, TX_ERROR_HOOK);
      }
      return tstate | state_bits;
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
  return (tinfo && (lives_proc_thread_has_states(tinfo, THRD_STATE_FINISHED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_done(lives_proc_thread_t tinfo) {
  proc_thread_kill_lock();
  if (tinfo && (lives_proc_thread_check_states(tinfo, THRD_STATE_FINISHED | THRD_STATE_CANCELLED
                | THRD_OPT_IDLEFUNC))) {
    proc_thread_kill_unlock();
    return TRUE;
  }
  proc_thread_kill_unlock();
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_has_states(tinfo, THRD_STATE_SIGNALLED)));
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
  return (tinfo && (lives_proc_thread_has_states(tinfo, THRD_OPT_CANCELLABLE)));
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_pauseable(lives_proc_thread_t tinfo, boolean state) {
  if (tinfo) {
    if (state) lives_proc_thread_include_states(tinfo, THRD_OPT_PAUSEABLE);
    else lives_proc_thread_exclude_states(tinfo, THRD_OPT_PAUSEABLE);
  }
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_pauseable(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_has_states(tinfo, THRD_OPT_PAUSEABLE)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_paused(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_has_states(tinfo, THRD_STATE_PAUSED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t tinfo, boolean dontcare) {
  if (!tinfo || !lives_proc_thread_get_cancellable(tinfo)) return FALSE;
  if (dontcare) lives_proc_thread_dontcare(tinfo);
  else lives_proc_thread_include_states(tinfo, THRD_STATE_CANCELLED);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_pause(lives_proc_thread_t tinfo) {
  if (!tinfo || !lives_proc_thread_get_pauseable(tinfo)) return FALSE;
  if (!lives_proc_thread_get_paused(tinfo))
    lives_proc_thread_include_states(tinfo, THRD_STATE_PAUSE_REQUESTED);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_resume(lives_proc_thread_t lpt) {
  if (!lpt || !lives_proc_thread_get_paused(lpt)) return FALSE;
  lives_proc_thread_include_states(lpt, THRD_STATE_RESUME_REQUESTED);
  if (lives_proc_thread_has_states(lpt, THRD_STATE_UNQUEUED)) lives_proc_thread_resume(lpt);
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


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancelled(lives_proc_thread_t tinfo) {
  return (!tinfo || (lives_proc_thread_has_states(tinfo, THRD_STATE_CANCELLED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_running(lives_proc_thread_t tinfo) {
  // state is maintaained even if cancelled or finished
  return (!tinfo || (lives_proc_thread_has_states(tinfo, THRD_STATE_RUNNING)));
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


static int lives_proc_thread_wait_done(lives_proc_thread_t tinfo, double timeout) {
  ticks_t slptime = LIVES_QUICK_NAP * 10;
  if (timeout) timeout *= ONE_BILLION;
  while (timeout >= 0. && !lives_proc_thread_is_done(tinfo)) {
    if (is_fg_thread()) {
      while (fg_service_fulfill());
    }
    // wait 10 usec
    lives_nanosleep(slptime);
    if (timeout) timeout -= slptime;
  }
  if (timeout < 0.) return LIVES_RESULT_FAIL;
  return LIVES_RESULT_SUCCESS;
}


LIVES_LOCAL_INLINE void _lives_proc_thread_join(lives_proc_thread_t tinfo, double timeout) {
  // version without a return value will free tinfo
  lives_proc_thread_wait_done(tinfo, timeout);

  // caller should ref the proc_thread if it wants to avoid this
  lives_proc_thread_free(tinfo);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t tinfo) {
  // version without a return value will free tinfo
  _lives_proc_thread_join(tinfo, 0.);
}

#define _join(tinfo, stype) lives_proc_thread_wait_done(tinfo, 0.);	\
  lives_nanosleep_while_false(weed_plant_has_leaf(tinfo, _RV_) == WEED_TRUE); \
  return weed_get_##stype##_value(tinfo, _RV_, NULL);

LIVES_GLOBAL_INLINE int lives_proc_thread_join_int(lives_proc_thread_t tinfo) { _join(tinfo, int);}
LIVES_GLOBAL_INLINE double lives_proc_thread_join_double(lives_proc_thread_t tinfo) {_join(tinfo, double);}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_boolean(lives_proc_thread_t tinfo) { _join(tinfo, boolean);}
LIVES_GLOBAL_INLINE int64_t lives_proc_thread_join_int64(lives_proc_thread_t tinfo) {_join(tinfo, int64);}
LIVES_GLOBAL_INLINE char *lives_proc_thread_join_string(lives_proc_thread_t tinfo) {_join(tinfo, string);}
LIVES_GLOBAL_INLINE weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t tinfo) {_join(tinfo, funcptr);}
LIVES_GLOBAL_INLINE void *lives_proc_thread_join_voidptr(lives_proc_thread_t tinfo) {_join(tinfo, voidptr);}
LIVES_GLOBAL_INLINE weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t tinfo) {_join(tinfo, plantptr);}


// inform a thread that we no longer care about the return value
// if it is still running, we cancel it if cancellable.
// otherwise, if it already finished, we simply free it
// all this is done inside a mutex lock, so that we can be sure a running thread takes note of the the flagbits
// and does actually free. After calling this it should be assumed that the thread may be freed, and thus
// should not be referenced further.
// - lives_proc_thread_nullify, is identical, except that it takes an extra parameter, a pointer to void *
// when the proc_thread_is free, the void * target will be set to NULL
// (note, only one pointer can be nullified like this, for multiple pointers, use the proc_thread's
//

boolean lives_proc_thread_dontcare(lives_proc_thread_t tinfo) {
  if (!tinfo) return FALSE;
  pthread_mutex_t *destruct_mutex = (pthread_mutex_t *)weed_get_voidptr_value(tinfo, LIVES_LEAF_DESTRUCT_MUTEX, NULL);

  // stop proc_thread from being freed as soon as we set the flagbit
  pthread_mutex_lock(destruct_mutex);
  if (lives_proc_thread_check_finished(tinfo)) {
    // if the proc_thread runner already checked and exited we need to do the free
    pthread_mutex_unlock(destruct_mutex);
    lives_proc_thread_free(tinfo);
    return TRUE;
  }
  if (lives_proc_thread_get_cancellable(tinfo))
    lives_proc_thread_include_states(tinfo, THRD_OPT_DONTCARE | THRD_STATE_CANCELLED);
  else
    lives_proc_thread_include_states(tinfo, THRD_OPT_DONTCARE);
  pthread_mutex_unlock(destruct_mutex);
  return FALSE;
}


boolean lives_proc_thread_dontcare_nullify(lives_proc_thread_t lpt, void **thing) {
  if (lpt) {
    lives_proc_thread_hook_append(lpt, DESTROYED_HOOK, 0, lives_nullify_ptr_cb, (void *)thing);
    return lives_proc_thread_dontcare(lpt);
  }
  return TRUE;
}


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
  // if pthread_cancel was called, the thread immediately jumps here,
  // because will called pthread_cleanup_push
  // after this, the pthread will exit
  if (lives_proc_thread_get_signalled(info))
    lives_hooks_trigger(NULL, THREADVAR(hook_closures), TX_EXIT_HOOK);
}


// for lives_proc_threads, this is a wrapper function which gets called from the worker thread
// the real payload is run via call_funcsig, which allow any type of function with any return type
// to be wrapped
static void *proc_thread_worker_func(void *args) {
  lives_proc_thread_t lpt = (lives_proc_thread_t)args;
  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);
  uint64_t state;
  pthread_mutex_t *destruct_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);

  THREADVAR(tinfo) = lpt;
  weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, (void *)pthread_self());

  do {
    // if pthread_cancel is called, the cleanup_func will be called and pthread will exit
    pthread_cleanup_push(pthread_cleanup_func, args);

    call_funcsig(lpt);

    pthread_cleanup_pop(0);

    weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, NULL);

    pthread_mutex_lock(destruct_mutex);

    state = lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);

    if ((state & THRD_OPT_DONTCARE) || (!ret_type && !(state & THRD_OPT_NOTIFY))) {
      // if there is no return type then we should free the proc_thread
      // QUERY REFS - if > 0 leave
      if (lives_proc_thread_free(lpt)) {
        pthread_mutex_unlock(destruct_mutex);
        return NULL;
      }
    }

    if (lives_proc_thread_has_states(lpt, THRD_OPT_IDLEFUNC) && lives_proc_thread_join_boolean(lpt)) {
      pthread_mutex_unlock(destruct_mutex);
      if (!lives_proc_thread_has_states(lpt, THRD_STATE_PAUSED | THRD_STATE_PAUSE_REQUESTED)) {
        // TODO - make configurable
        lives_nanosleep(MILLIONS(10));
        continue;
      }
      lives_proc_thread_pause(lpt);
      lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING | THRD_STATE_QUEUED | THRD_STATE_PREPARING);
      lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);
      return NULL;
    }
  } while (0);

  pthread_mutex_unlock(destruct_mutex);
  return NULL;
}


boolean fg_run_func(lives_proc_thread_t lpt, void *rloc) {
  // rloc should be a pointer to a variable of the correct type. After calling this,
  int refs;  boolean bret = FALSE;
  if (!lives_proc_thread_get_cancelled(lpt)) {
    refs = weed_refcount_query(lpt);
    weed_refcount_inc(lpt);
    call_funcsig(lpt);
    if (weed_refcount_query(lpt) > refs) weed_refcount_dec(lpt);
    if (rloc) {
      if (weed_leaf_seed_type(lpt, _RV_) == WEED_SEED_STRING) *(char **)rloc = weed_get_string_value(lpt, _RV_, NULL);
      else _weed_leaf_get(lpt, _RV_, 0, rloc);
    }
    lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
  }
  return bret;
}


/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
static void run_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  thrd_work_t *mywork;

  /// tell the thread to clean up after itself [but it won't delete thread_info]
  attr |= LIVES_THRDATTR_AUTODELETE;

  attr |= LIVES_THRDATTR_IS_PROC_THREAD;
  lives_proc_thread_include_states(thread_info, THRD_STATE_QUEUED);

  // add the work to the pool
  mywork = lives_thread_create(NULL, attr, proc_thread_worker_func, (void *)thread_info);

  if (attr & LIVES_THRDATTR_WAIT_START) {
    // WAIT_START: caller waits for thread to run or finish
    lives_nanosleep_until_nonzero(mywork->flags & (LIVES_THRDFLAG_RUNNING |
                                  LIVES_THRDFLAG_FINISHED));
    mywork->flags &= ~LIVES_THRDFLAG_WAIT_START;
  }
}


//////// worker thread pool //////////////////////////////////////////

///////// thread pool ////////////////////////
#ifndef VALGRIND_ON
#define MINPOOLTHREADS 8
#else
#define MINPOOLTHREADS 2
#endif
// rnpoolthreads is the reserved npoolthreads, npoolthreads is the ctual number, which may be lower because idle
// threads will time out and exit after a while
// frnpoolthreads is the number of available (free) poolthreads, we try to maintain this > ntasks
static volatile int npoolthreads, rnpoolthreads, frnpoolthreads;
static pthread_t **poolthrds;
static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t allctx_rwlock;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile LiVESList *twork_list, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

static pthread_key_t tdata_key;
static lives_thread_data_t *global_tdata = NULL;
static LiVESList *allctxs = NULL;

static int extern_thread_idx = -1;

lives_thread_data_t *get_thread_data(void) {
  lives_thread_data_t *tdata = pthread_getspecific(tdata_key);
  if (!tdata) tdata = lives_thread_data_create(extern_thread_idx--);
  return tdata;
}

lives_thread_data_t *get_global_thread_data(void) {return global_tdata;}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars(void) {
  lives_thread_data_t *thrdat = get_thread_data();
  return thrdat ? &thrdat->vars : NULL;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_global_threadvars(void) {
  lives_thread_data_t *thrdat = get_global_thread_data();
  return thrdat ? &thrdat->vars : NULL;
}


lives_thread_data_t *get_thread_data_by_idx(uint64_t idx) {
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


static void lives_thread_data_destroy(void *data) {
  lives_thread_data_t *tdata = (lives_thread_data_t *)data;
  pthread_rwlock_wrlock(&allctx_rwlock);
  allctxs = lives_list_remove_data(allctxs, tdata, TRUE);
  pthread_rwlock_unlock(&allctx_rwlock);
}


lives_thread_data_t *lives_thread_data_create(uint64_t idx) {
  lives_thread_data_t *tdata;
  pthread_t self;

  if (!idx)(void) pthread_key_create(&tdata_key, lives_thread_data_destroy);

#ifdef USE_RPMALLOC
  // must be done before anything else
  if (!rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_initialize();
  }
#endif

  self = pthread_self();

  tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));
  if (!idx) global_tdata = tdata;

  (void) pthread_setspecific(tdata_key, tdata);

  if (idx) tdata->ctx = lives_widget_context_new();
  else tdata->ctx = lives_widget_context_default();

  tdata->vars.var_idx = tdata->idx = idx;
  tdata->vars.var_rowstride_alignment = RS_ALIGN_DEF;
  tdata->vars.var_last_sws_block = -1;
  tdata->vars.var_uid = gen_unique_id();
  tdata->vars.var_blocked_limit = BLOCKED_LIMIT;
  tdata->vars.var_mydata = tdata;
  tdata->vars.var_self = tdata->self = self;

  lives_icap_init(&tdata->vars.var_intentcap);

#ifndef NO_NP
  if (1) {
    pthread_attr_t attr;
    void *stack;
    size_t stacksize;
    pthread_getattr_np(self, &attr);
    pthread_attr_getstack(&attr, &stack, &stacksize);
    tdata->vars.var_stackaddr = stack;
    tdata->vars.var_stacksize = stacksize;
  }
#endif
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    pthread_mutex_init(&tdata->vars.var_hook_mutex[i], NULL);
  }
  pthread_rwlock_wrlock(&allctx_rwlock);
  allctxs = lives_list_prepend(allctxs, (livespointer)tdata);
  pthread_rwlock_unlock(&allctx_rwlock);

  make_thrdattrs(tdata);

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
  if (xlist) lives_list_free_all(&(xlist[type]));
}


LIVES_GLOBAL_INLINE void lives_hooks_clear_all(LiVESList **xlist, int ntypes) {
  if (xlist) {
    for (int i = 0; i < ntypes; i++) if (xlist[i]) lives_list_free_all(&(xlist[i]));
  }
}


LIVES_LOCAL_INLINE LiVESList *lives_hooks_copy(LiVESList *in) {
  LiVESList *out = NULL;
  for (; in; in = in->next) {
    lives_closure_t *inclosure = (lives_closure_t *)in->data;
    lives_closure_t *outclosure = (lives_closure_t *)lives_malloc(sizeof(lives_closure_t));
    outclosure->fdef = inclosure->fdef;
    outclosure->data = inclosure->data;
    outclosure->retloc = inclosure->retloc;
    out = lives_list_append(out, outclosure);
  }
  return out;
}


LIVES_LOCAL_INLINE void lives_hooks_transfer(LiVESList **dest, LiVESList **src, boolean include_glob) {
  int type = 0;
  if (!include_glob) type = N_GLOBAL_HOOKS + 1;
  for (; type < N_HOOK_POINTS && src[type]; type++) {
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

  if (!tdata->idx) lives_abort("Invalid worker thread ID - internal error");

  pthread_mutex_lock(&twork_mutex);

  list = (LiVESList *)twork_list;

  if (!list) {
    pthread_mutex_unlock(&twork_mutex);
    return FALSE;
  }

  if ((LiVESList *)twork_last == list) twork_list = twork_last = NULL;
  else {
    twork_list = (volatile LiVESList *)list->next;
    twork_list->prev = NULL;
  }

  pthread_mutex_unlock(&twork_mutex);

  mywork = (thrd_work_t *)list->data;
  list->next = list->prev = NULL;

  if (!mywork) {
    pthread_mutex_lock(&twork_count_mutex);
    ntasks--;
    pthread_mutex_unlock(&twork_count_mutex);
    return FALSE;
  }

  lpt = mywork->lpt;
  if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_PREPARING);

  mywork->busy = tdata->idx;

  myflags = mywork->flags = myflags & ~LIVES_THRDFLAG_QUEUED_WAITING;

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

  // lpt may be freed now, so recheck
  lpt = mywork->lpt;

  /* lives_widget_context_invoke_full(tdata->ctx, mywork->attr & LIVES_THRDATTR_PRIORITY */
  /*                                  ? LIVES_WIDGET_PRIORITY_HIGH - 100 : LIVES_WIDGET_PRIORITY_HIGH, */
  /*                                  widget_context_wrapper, mywork, NULL); */

  for (int type = N_GLOBAL_HOOKS + 1; type < N_HOOK_POINTS; type++) {
    lives_hooks_clear(THREADVAR(hook_closures), type);
  }

  // make sure caller has noted that thread finished

  lives_nanosleep_until_zero(mywork->flags & LIVES_THRDFLAG_WAIT_START);

  if (myflags & LIVES_THRDFLAG_AUTODELETE) {
    lives_free(mywork);
    if (!(myflags & LIVES_THRDFLAG_NOFREE_LIST)) lives_thread_free((lives_thread_t *)list);
  } else mywork->done = tdata->idx;

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

  while (!threads_die) {
    if (!skip_wait) {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += POOL_TIMEOUT_SEC;
      lives_mutex_lock_carefully(&tcond_mutex);
      rc = pthread_cond_timedwait(&tcond, &tcond_mutex, &ts);
      pthread_mutex_unlock(&tcond_mutex);
      if (rc == ETIMEDOUT) {
        // if the thread is waiting around doing nothing, exit, maybe free up some resources
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
  frnpoolthreads = rnpoolthreads = npoolthreads = MINPOOLTHREADS;
  if (mainw->debug) frnpoolthreads = rnpoolthreads = npoolthreads = 0;
  if (prefs->nfx_threads > npoolthreads) frnpoolthreads = rnpoolthreads = npoolthreads = prefs->nfx_threads;
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_list = twork_last = NULL;
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
    lives_thread_data_t *tdata = get_thread_data_by_idx(i + 1);
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
  lives_free(poolthrds);
  poolthrds = NULL;
  frnpoolthreads = rnpoolthreads = npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_list);
  twork_list = twork_last = NULL;
  ntasks = 0;
}


LIVES_GLOBAL_INLINE void lives_thread_free(lives_thread_t *thread) {if (thread) lives_list_free(thread);}


thrd_work_t *lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr,
                                 lives_thread_func_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  if (!thread) list = lives_list_append(NULL, NULL);
  else list->next = list->prev = NULL;
  list->data = work;
  work->func = func;
  work->attr = attr;
  work->arg = arg;
  work->flags = LIVES_THRDFLAG_QUEUED_WAITING;
  work->caller = THREADVAR(idx);
  work->sync_ready = TRUE;

  if (thread) work->flags |= LIVES_THRDFLAG_NOFREE_LIST;

  if (attr & LIVES_THRDATTR_IS_PROC_THREAD) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)arg;
    work->lpt = lpt;
    if (attr & LIVES_THRDATTR_IGNORE_SYNCPT) {
      work->flags |= LIVES_THRDFLAG_IGNORE_SYNCPT;
    }
    if (attr & LIVES_THRDATTR_IDLEFUNC) {
      lives_proc_thread_include_states(lpt, THRD_OPT_IDLEFUNC);
    }
    lives_proc_thread_set_work(lpt, work);
  }

  if (!thread || (attr & LIVES_THRDATTR_AUTODELETE)) {
    work->flags |= LIVES_THRDFLAG_AUTODELETE;
  }

  if (attr & LIVES_THRDATTR_WAIT_SYNC) work->sync_ready = FALSE;

  if (attr & LIVES_THRDATTR_INHERIT_HOOKS) {
    lives_hooks_transfer(work->hook_closures, THREADVAR(hook_closures), FALSE);
  }

  lives_mutex_lock_carefully(&twork_mutex);
  if (!twork_list) {
    twork_list = twork_last = list;
  } else {
    if (attr & LIVES_THRDATTR_PRIORITY) {
      twork_list->prev = list;
      list->next = (LiVESList *)twork_list;
      twork_list = list;
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
  if (rnpoolthreads && ntasks <= frnpoolthreads) {
    if (npoolthreads < rnpoolthreads) {
      frnpoolthreads += rnpoolthreads - npoolthreads;
      for (int i = 0; i < rnpoolthreads; i++) {
        lives_thread_data_t *tdata = get_thread_data_by_idx(i + 1);
        if (!tdata || tdata->exited) {
          if (tdata && tdata->exited) pthread_join(*poolthrds[i], NULL);
          lives_free(poolthrds[i]);
          poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
          pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
          lives_mutex_lock_carefully(&tcond_mutex);
          pthread_cond_signal(&tcond);
          pthread_mutex_unlock(&tcond_mutex);
        }
      }
    }
    if (ntasks <= frnpoolthreads) {
      lives_mutex_lock_carefully(&tcond_mutex);
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    } else {
      int extrs = MAX(MINPOOLTHREADS, ntasks - frnpoolthreads);
      poolthrds =
        (pthread_t **)lives_realloc(poolthrds, (rnpoolthreads + extrs) * sizeof(pthread_t *));
      for (int i = rnpoolthreads; i < rnpoolthreads + extrs; i++) {
        poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
        pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
        lives_mutex_lock_carefully(&tcond_mutex);
        pthread_cond_signal(&tcond);
        pthread_mutex_unlock(&tcond_mutex);
      }
      rnpoolthreads += extrs;
      frnpoolthreads += extrs;
      lives_mutex_lock_carefully(&tcond_mutex);
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    npoolthreads = rnpoolthreads;
  }
  pthread_mutex_unlock(&pool_mutex);
  return work;
}


uint64_t lives_thread_join(lives_thread_t work, void **retval) {
  thrd_work_t *task = (thrd_work_t *)work.data;
  uint64_t nthrd = 0;
  boolean fg = is_fg_thread();

  if (task->flags & LIVES_THRDFLAG_AUTODELETE) {
    LIVES_FATAL("lives_thread_join() called on an autodelete thread");
    return 0;
  }

  while (!task->busy) {
    if (fg) lives_mutex_lock_carefully(&tcond_mutex);
    else pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    if (!task->busy) lives_nanosleep(1000);
  }

  while (!task->done) {
    if (fg) while (fg_service_fulfill()) lives_widget_context_update();
    if (!task->done) lives_nanosleep(1000);
  }

  nthrd = task->done;
  if (retval) *retval = task->ret;

  return nthrd;
}


LIVES_GLOBAL_INLINE uint64_t lives_thread_done(lives_thread_t work) {
  thrd_work_t *task = (thrd_work_t *)work.data;
  return task->done;
}

//// hook functions

lives_proc_thread_t _lives_hook_add(LiVESList **hooks, int type, uint64_t flags,
                                    hook_funcptr_t func, const char *fname,
                                    livespointer data, boolean is_append) {
  lives_proc_thread_t xlpt = NULL, lpt = NULL;
  lives_closure_t *closure;
  uint64_t xflags = flags & HOOK_UNIQUE_REPLACE_MATCH;
  boolean cull = FALSE;
  pthread_mutex_t hmutex;

  if (!hooks) hooks = THREADVAR(hook_closures);

  if (lpt) hmutex = ((pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_MUTEXES, NULL))[type];
  else hmutex = THREADVAR(hook_mutex[type]);

  if (flags & HOOK_CB_FG_THREAD) {
    xlpt = lpt = (lives_proc_thread_t)data;
  }
  if (xflags) {
    lives_proc_thread_t lpt2 = NULL;
    LiVESList *list = hooks[type], *listnext;
    int maxp = 0;

    if (flags & HOOK_CB_FG_THREAD) {
      maxp = THREADVAR(hook_match_nparams);
    }
    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_STATUS_BLOCKED) continue;
      if (lpt) {
        if (!(closure->flags & HOOK_CB_FG_THREAD)) continue;
        lpt2 = (lives_proc_thread_t)data;
        if (fn_func_match(lpt, lpt2) < 0) continue;
      } else if (closure->fdef->function != (lives_funcptr_t)func) continue;
      if (xflags == HOOK_UNIQUE_FUNC) return NULL;
      if (cull || xflags == HOOK_UNIQUE_REPLACE_FUNC) {
        if (lpt && lpt == lpt2) continue;
        pthread_mutex_lock(&hmutex);
        hooks[type] = lives_list_remove_node(hooks[type], list, FALSE);
        pthread_mutex_unlock(&hmutex);
        if (lpt2) {
          // if we preempted another proc_thread, free the older one
          lives_proc_thread_include_states(lpt2, THRD_STATE_FINISHED);
          lives_proc_thread_free(lpt2);
        }  else lives_free(closure);
        continue;
      }
      if ((!lpt && closure->data == data)
          || (lpt && fn_data_match(lpt2, lpt, maxp))) {
        if (xflags == HOOK_UNIQUE_REPLACE_MATCH) {
          if (lpt && lpt == lpt2) continue;
          pthread_mutex_lock(&hmutex);
          hooks[type] = lives_list_remove_node(hooks[type], list, FALSE);
          pthread_mutex_unlock(&hmutex);
          if (lpt2) {
            // if we preempted another proc_thread, free the older one
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
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (cull || xflags == HOOK_UNIQUE_REPLACE) return xlpt;

  closure = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
  closure->fdef = get_template_for_func((lives_funcptr_t)func);
  if (!closure->fdef) {
    closure->fdef = add_fn_lookup((lives_funcptr_t)func, fname, "b", "vv", NULL, 0, NULL);
  }
  /* closure->func = func; */
  /* closure->funcname = fname; */
  closure->data = data;
  closure->flags = flags;

  pthread_mutex_lock(&hmutex);
  if (is_append) hooks[type] = lives_list_append(hooks[type], closure);
  else hooks[type] = lives_list_append(hooks[type], closure);
  pthread_mutex_unlock(&hmutex);

  if (lpt) return xlpt;
  return NULL;
}


static lives_proc_thread_t _lives_hook_append(LiVESList **hooks, int type, uint64_t flags,
    hook_funcptr_t func, const char *fname, livespointer data) {
  return _lives_hook_add(hooks, type, flags, func, fname, data, TRUE);
}

static ALLOW_UNUSED lives_proc_thread_t _lives_hook_prepend(LiVESList **hooks, int type, uint64_t flags,
    hook_funcptr_t func, const char *fname, livespointer data) {
  return _lives_hook_add(hooks, type, flags, func, fname, data, FALSE);
}


LIVES_GLOBAL_INLINE boolean lives_hooks_trigger(lives_obj_t *obj, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_closure_t *closure;
  boolean bret;
  pthread_mutex_t hmutex;

  if (type == DATA_READY_HOOK) {
    lives_hooks_trigger_async(obj, xlist, type);
    return TRUE;
  }

  if (obj) hmutex = ((pthread_mutex_t *)weed_get_voidptr_value(obj, LIVES_LEAF_HOOK_MUTEXES, NULL))[type];
  else hmutex = THREADVAR(hook_mutex[type]);

  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (!closure) continue;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return FALSE;
    if (closure->flags |= HOOK_STATUS_RUNNING) {
      if (closure->flags & HOOK_CB_FG_THREAD) {
        closure->tinfo = (lives_proc_thread_t)closure->data;
        weed_set_voidptr_value(closure->tinfo, "closure", closure);
        if (is_fg_thread()) {
          if (!lives_proc_thread_is_done(closure->tinfo))
            fg_run_func(closure->tinfo, closure->retloc);
        } else {
          // some functions may have been deferred, since we cannot stack multiple fg service calls
          if (!lives_proc_thread_is_done(closure->tinfo))
            fg_service_call(closure->tinfo, closure->retloc);
        }
        lives_proc_thread_include_states(closure->tinfo, THRD_STATE_FINISHED);
        lives_proc_thread_free(closure->tinfo);
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
        continue;
      }

      closure->tinfo =
        _lives_proc_thread_create(LIVES_THRDATTR_FG_THREAD, (lives_funcptr_t)closure->fdef->function,
                                  closure->fdef->funcname, closure->fdef->return_type,
                                  closure->fdef->args_fmt, obj, closure->data);
      weed_set_voidptr_value(closure->tinfo, "closure", closure);
      fg_run_func(closure->tinfo, &bret);


      if (type != WAIT_SYNC_HOOK) {
        lives_proc_thread_include_states(closure->tinfo, THRD_STATE_FINISHED);
        lives_proc_thread_free(closure->tinfo);
        closure->tinfo = NULL;
      }

      if (!bret) {
        if (type == WAIT_SYNC_HOOK) {
          closure->flags &= ~HOOK_STATUS_RUNNING;
          return FALSE;
        }
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
      } else closure->flags &= ~HOOK_STATUS_RUNNING;
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_hooks_triggero(lives_object_t *obj, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  boolean bret;
  pthread_mutex_t hmutex;

  if (type == DATA_READY_HOOK) {
    lives_hooks_trigger_asynco(obj, xlist, type);
    return TRUE;
  }

  if (obj) hmutex = obj->hook_mutex[type];
  else hmutex = THREADVAR(hook_mutex[type]);

  for (list = xlist[type]; list; list = listnext) {
    lives_closure_t *closure = (lives_closure_t *)list->data;
    listnext = list->next;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return FALSE;
    if (closure->flags |= HOOK_STATUS_RUNNING) {
      if (closure->flags & HOOK_CB_FG_THREAD) {
        closure->tinfo = (lives_proc_thread_t)closure->data;
        weed_set_voidptr_value(closure->tinfo, "closure", closure);
        if (is_fg_thread()) {
          if (!lives_proc_thread_is_done(closure->tinfo))
            fg_run_func(closure->tinfo, closure->retloc);
        } else {
          // some functions may have been deferred, since we cannot stack multiple fg service calls
          if (!lives_proc_thread_is_done(closure->tinfo))
            fg_service_call(closure->tinfo, closure->retloc);
        }
        lives_proc_thread_include_states(closure->tinfo, THRD_STATE_FINISHED);
        lives_proc_thread_free(closure->tinfo);
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
        continue;
      }

      closure->tinfo =
        _lives_proc_thread_create(LIVES_THRDATTR_FG_THREAD, (lives_funcptr_t)closure->fdef->function,
                                  closure->fdef->funcname, closure->fdef->return_type,
                                  closure->fdef->args_fmt, obj, closure->data);
      weed_set_voidptr_value(closure->tinfo, "closure", closure);
      fg_run_func(closure->tinfo, &bret);
      lives_proc_thread_include_states(closure->tinfo, THRD_STATE_FINISHED);
      lives_proc_thread_free(closure->tinfo);
      closure->tinfo = NULL;

      if (!bret) {
        if (type == WAIT_SYNC_HOOK) {
          closure->flags &= ~HOOK_STATUS_RUNNING;
          return FALSE;
        }
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
      } else closure->flags &= ~HOOK_STATUS_RUNNING;
    }
  }
  return TRUE;
}


static void _lives_hooks_tr_seq(lives_proc_thread_t lpt, LiVESList **xlist, int type,  hook_funcptr_t finfunc,
                                void *findata) {
  while (1) {
    if (lives_hooks_trigger(lpt, xlist, type)) {
      // if all functions return TRUE, execute finfunc, and exit
      if (finfunc)((*finfunc)(lpt, findata));
      return;
    }
    lives_nanosleep(10000);
  }
}

static void _lives_hooks_tr_seqo(lives_object_t *obj, LiVESList **xlist, int type, hook_funcptr_t finfunc,
                                 void *findata) {
  /* lives_hooks_triggero(obj, xlist, type); */
  /* (*finfunc)(obj,findata); */
}

static boolean remifalseo(lives_object_t *xobj, void *xxlist) {
  /* if (weed_get_boolean_value(xlpt, _RV_, NULL) == FALSE) { */
  /*   pthread_mutex_t *hook_mutex = (pthread_mutex_t *)weed_get_voidptr_array(lpt, LIVES_LEAF_HOOK_MUTEXES, NULL); */
  /*   LiVESList **xlist = (LiVESList **)xxlist; */
  /*   LiVESList *list = weed_get_voidptr_value(xlpt, "xlist_list", NULL); */
  /*   int type = weed_get_int_value(xlpt, "xlist_type", NULL); */
  /*   pthread_mutex_lock(hook_mutex[type]); */
  /*   xlist[type] = lives_list_remove_node(xlist[type], list, FALSE); */
  /*   pthread_mutex_unlock(hook_mutex[type]); */
  /*   lives_free(hook_mutex); */
  /* } */
  /* lives_proc_thread_free(xlpt); */
  return TRUE;
}

static boolean remifalse(lives_obj_t *xlpt, void *xxlist) {
  if (weed_get_boolean_value(xlpt, _RV_, NULL) == FALSE) {
    int type = weed_get_int_value(xlpt, "xlist_type", NULL);
    pthread_mutex_t *hook_mutex =
      (pthread_mutex_t *)weed_get_voidptr_value(xlpt, LIVES_LEAF_HOOK_MUTEXES, NULL);
    LiVESList **xlist = (LiVESList **)xxlist;
    LiVESList *list = weed_get_voidptr_value(xlpt, "xlist_list", NULL);
    pthread_mutex_lock(&hook_mutex[type]);
    xlist[type] = lives_list_remove_node(xlist[type], list, FALSE);
    pthread_mutex_unlock(&hook_mutex[type]);
    lives_free(hook_mutex);
  }
  lives_proc_thread_free(xlpt);
  return FALSE;
}


void lives_hooks_trigger_async_sequentialo(lives_object_t *lpt, LiVESList **xlist, int type,
    hook_funcptr_t finfunc, void *findata) {
}


void lives_hooks_trigger_async_sequential(lives_obj_t *lpt, LiVESList **xlist, int type,
    hook_funcptr_t finfunc, void *findata) {
  lives_proc_thread_create(0, (lives_funcptr_t)_lives_hooks_tr_seq, 0, "pviFv",
                           lpt, xlist, type, (weed_funcptr_t)finfunc, findata);
}


LIVES_GLOBAL_INLINE void _lives_proc_thread_hook_append(lives_proc_thread_t lpt, int type, uint64_t flags,
    hook_funcptr_t func, const char *fname, livespointer data) {
  LiVESList **hook_closures =
    (LiVESList **)weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_CLOSURES, NULL);
  _lives_hook_append(hook_closures, type, flags, func, fname, data);
}


void lives_hooks_trigger_async(lives_obj_t *lpt, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_proc_thread_t xlpt;
  lives_closure_t *closure;
  LiVESList **hook_closures;
  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return;
    if (closure->flags |= HOOK_STATUS_RUNNING);
    closure->tinfo = xlpt =
                       _lives_proc_thread_create(LIVES_THRDATTR_WAIT_SYNC, (lives_funcptr_t)closure->fdef->function,
                           closure->fdef->funcname, closure->fdef->return_type,
                           closure->fdef->args_fmt, lpt, closure->data);
    weed_set_voidptr_value(closure->tinfo, "closure", closure);
    hook_closures = (LiVESList **)weed_get_voidptr_value(xlpt, LIVES_LEAF_HOOK_CLOSURES, NULL);
    lives_hook_append(hook_closures, TX_FINISHED_HOOK, 0, remifalse, xlist);
    weed_set_int_value(xlpt, "xlist_type", type);
    weed_set_voidptr_value(xlpt, "xlist_list", list);
    lives_proc_thread_sync_ready(xlpt);
  }
}


void lives_hooks_trigger_asynco(lives_object_t *obj, LiVESList **xlist, int type) {
  /* LiVESList *list = xlist[type], *listnext; */
  /* lives_closure_t *closure; */
  /* for (list = xlist[type]; list; list = listnext) { */
  /*   listnext = list->next; */
  /*   closure = (lives_closure_t *)list->data; */
  /*   if (closure->flags & HOOK_STATUS_BLOCKED) continue; */
  /*   if (closure->flags & HOOK_STATUS_RUNNING) return; */
  /*   if (closure->flags |= HOOK_STATUS_RUNNING); */
  /*   xlpt = lives_proc_thread_create(LIVES_THRDATTR_WAIT_SYNC,(lives_funcptr_t)closure->func, */
  /* 				    WEED_SEED_BOOLEAN, "vv", obj, closure->data); */

  /*   lives_hook_append(hook_closures, TX_FINISHED_HOOK, 0, remifalse, xlist); */
  /*   weed_set_int_value(xlpt, "xlist_type", type); */
  /*   weed_set_voidptr_value(xlpt, "xlist_list", list); */
  /*   lives_proc_thread_sync_ready(xlpt); */
  /* } */
}


LIVES_GLOBAL_INLINE void lives_hook_remove(LiVESList **xlist, int type, hook_funcptr_t func, livespointer data,
    pthread_mutex_t *hmutexes) {
  // do not call for HOOK_CB_SINGLE_SHOT (TODO)
  pthread_mutex_lock(&hmutexes[type]);
  for (LiVESList *list = xlist[type]; list; list = list->next) {
    lives_closure_t *closure = (lives_closure_t *)list->data;
    if (closure->fdef->function == (lives_funcptr_t)func && closure->data == data) {
      closure->flags |= HOOK_STATUS_BLOCKED;
      xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
      break;
    }
  }
  pthread_mutex_unlock(&hmutexes[type]);
}


LIVES_GLOBAL_INLINE void lives_hooks_join(LiVESList * xlist, pthread_mutex_t *mutex) {
  lives_closure_t *closure;
  while (1) {
    LiVESList *list;
    pthread_mutex_lock(mutex);
    for (list = xlist; list; list = list->next) {
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_STATUS_RUNNING) break;
    }
    pthread_mutex_unlock(mutex);
    if (!list) break;
    lives_nanosleep(10000);
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


LIVES_GLOBAL_INLINE const char get_typeletter(uint8_t val) {
  val &= 0x0F;
  switch (val) {
  case 0x01: return 'i';
  case 0x02: return 'd';
  case 0x03: return 'b';
  case 0x04: return 's';
  case 0x05: return 'I';
#ifdef LIVES_SEED_FLOAT
  case LIVES_SEED_FLOAT; return 'f';
#endif
  case 0x0C: return 'F';
  case 0x0D: return 'V';
  case 0x0E: return 'P';
  default: return '?';
  }
}


LIVES_GLOBAL_INLINE const char *get_symbolname(uint8_t val) {
  val &= 0x0F;
  switch (val) {
  case 0x01: return "INT";
  case 0x02: return "DOUBLE";
  case 0x03: return "BOOL";
  case 0x04: return "STRING";
  case 0x05: return "INT64";
  case 0x0C: return "FUNCP";
  case 0x0D: return "VOIDP";
  case 0x0E: return "PLANTP";
  default: return "";
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


char *args_fmt_from_funcsig(funcsig_t sig) {
  char it[2];
  char *args_fmt;
  if (!sig) return lives_strdup("void");
  it[1] = 0;
  args_fmt = lives_strdup("");
  for (int i = 0; i <= 60; i += 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) break;
    it[0] = get_typeletter(ch);
    args_fmt = lives_strdup_concat(args_fmt, NULL, "%s", it);
  }
  return args_fmt;
}


char *funcsig_to_param_string(funcsig_t sig) {
  char *fmtstring;
  if (!sig) return lives_strdup("void");
  fmtstring = lives_strdup("");
  for (int i = 0; i <= 60; i += 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) break;
    fmtstring = lives_strdup_concat(fmtstring, ", ", "%s", weed_seed_to_ctype(get_seedtype(ch), FALSE));
  }
  return fmtstring;
}


char *funcsig_to_symstring(funcsig_t sig) {
  char *fmtstring;
  if (!sig) return lives_strdup("void");
  fmtstring = lives_strdup("");
  for (int i = 60; i >= 0; i -= 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) continue;
    fmtstring = lives_strdup_concat(fmtstring, "_", "%s", get_symbolname(ch));
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
    fdef->funcsig = funcsig_from_args_fmt(args_fmt);
    if (file) fdef->file = lives_strdup(file);
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
  return 0;
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
  return -1;
}


LIVES_GLOBAL_INLINE int refcount_query(obj_refcounter * refcount) {
  if (check_refcnt_init(refcount)) {
    int count;
    pthread_mutex_lock(&refcount->mutex);
    count = refcount->count;
    pthread_mutex_unlock(&refcount->mutex);
    return count;
  }
  return 0;
}


LIVES_GLOBAL_INLINE int weed_refcount_inc(weed_plant_t *plant) {
  // increment refcount for a plant. If it does not have a refcounter, one will be added first
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) {
      weed_add_refcounter(plant);
      refcnt = (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    }
    if (refcnt) return refcount_inc(refcnt);
  }
  return 0;
}


LIVES_GLOBAL_INLINE int weed_refcount_dec(weed_plant_t *plant) {
  // value of -1 indicates the plant should be free with weed_plant_free()
  // if the plant has a refcounter, mutex lock will be held in this cas
  // so caller must unlock it before freeing, by calling weed_refcounter_unlock()
  // if plant does not have a refcounter, -1 will be returned.
  // It is always safe to call weed_refcounter_unlock(), as if the plant has no refcounter, it will do nothing
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return -1;
    return refcount_dec(refcnt);
  }
  return -0;
}


LIVES_GLOBAL_INLINE int weed_refcount_query(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return 0;
    return refcount_query(refcnt);
  }
  return 0;
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


LIVES_GLOBAL_INLINE lives_refcounter_t *weed_add_refcounter(weed_plant_t *plant) {
  lives_refcounter_t *refcount = NULL;
  if (plant) {
    if (weed_plant_has_leaf(plant, LIVES_LEAF_REFCOUNTER))
      refcount = (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    else {
      refcount = (lives_refcounter_t *)lives_calloc(1, sizeof(lives_refcounter_t));
      if (refcount) {
        refcount->mutex_inited = FALSE;
        weed_set_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, refcount);
        weed_leaf_set_autofree(plant, LIVES_LEAF_REFCOUNTER, TRUE);
        weed_leaf_set_undeletable(plant, LIVES_LEAF_REFCOUNTER, WEED_TRUE);
        check_refcnt_init(refcount);
      }
    }
  }
  return refcount;
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
      msg = lives_strdup_concat(msg, "\n", "Thread %d (%s) is %d", thrdinfo->var_idx, notes,
                                work ? "busy" : "idle");
      if (notes) lives_free(notes);
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return msg;
}
