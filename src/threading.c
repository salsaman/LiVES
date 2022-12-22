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
   - proc_threads with a timeout can be created. If the task does not finish before the timer expires, the thread will be
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

	- WAIT_START - the caller will block until the proc_thread is about to be run

			NOTE: WAIT_START and WAIT_SYNC may be combined, the effect will be to block the caller until
			 the worker thread reaches the waiting state, and caller must subsequently call sync_ready

	- IDLEFUNC - the proc_thread will run as normal, but after finishing succesfully,
			 the return value (which must be int or boolean), will be checked. If the value is TRUE,
			the thread will return with state IDLING | UNQUEUED
			however, if the return value is FALSE, the state will be COMPLETED / FINISHED

	additionally, some functions may have sync_points, the caller should attach a function
	to the thread's SYNC_WAIT_HOOKs, and send sync_ready so the thread can continue.
	in addition, when in this state, lives_proc_thread_is_waiting(lpt) will return TRUE

	IGNORE_SYNCPT - setting this flagbit tells the thread to not wait at sync points, this


	- NO_GUI:    - this has no functional effect, but will set a flag in the worker thread's environment, and this may be
			checked for in the function code and used to bypass graphical updates

	- START_UNQUEUED: - create the thread but do not queue it for executuion. lives_proc_thread_queue(lpt) can be called
			later to add the

	- NOTE_TIMINGS - thread will record timestanps in ticks in 3 or 4 values, the time when added to the worker queue
			the time when the workload is picked up and run by a thread, and the time when the task completes
			in addition, if the proc_thread was set SYN_CWAIT, the time spent waiting will be stored
			 in sync_wait_ticks
			the true time spent waiting in the queue is thus: queue_ticks - start_ticks - sync_wait_ticks
			and the time spent running the task is end_ticks - start_ticks
			this can be useful as statistical information

   - the only requirements are to call lives_proc_thread_create() which will generate a lives_proc_thread_t and run it,
   and then (depending on the return_type parameter, call one of the lives_proc_thread_join_*() functions

   (see that function for more comments)
*/


static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;

static boolean sync_hooks_done(lives_obj_t *obj, void *data) {
  boolean *b = (boolean *)data;
  *b = TRUE;
  return FALSE;
}


static boolean _work_cond_timedwait(thrd_work_t *work, int secs) {
  // if thread gets cancel_immediate here, this acts like cond_signal
  // the thread will continue up to the exit point then run the cleanup handler
  // thus it would seem better NOT to call pthread_cond_signal before theread cancellation
  // as this way we can ensure the thread is cancelled before returning to the caller function
  static struct timespec ts;
  int rc = 0;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += secs;
  pthread_mutex_lock(work->pause_mutex);
  while (!work->sync_ready && rc != ETIMEDOUT) {
    rc = pthread_cond_timedwait(work->pcond, work->pause_mutex, &ts);
  }
  pthread_mutex_unlock(work->pause_mutex);
  if (rc == ETIMEDOUT && !work->sync_ready) {
    work->flags |= LIVES_THRDFLAG_TIMEOUT;
  }
  return TRUE;
}

static boolean _work_cond_wait(thrd_work_t *work) {
  // if thread gets cancel_immediate here, this acts like cond_signal
  // the thread will continue up to the exit point then run the cleanup handler
  // thus it would seem better NOT to call pthread_cond_signal before theread cancellation
  // as this way we can ensure the thread is cancelled before returning to the caller function
  pthread_mutex_lock(work->pause_mutex);
  while (!work->sync_ready) pthread_cond_wait(work->pcond, work->pause_mutex);
  pthread_mutex_unlock(work->pause_mutex);
  return TRUE;
}

static boolean _work_cond_signal(thrd_work_t *work) {
  pthread_mutex_lock(work->pause_mutex);
  work->sync_ready = TRUE;
  pthread_cond_signal(work->pcond);
  pthread_mutex_unlock(work->pause_mutex);
  return TRUE;
}


boolean lives_proc_thread_timedwait(int sec) {
  lives_proc_thread_t self = THREADVAR(proc_thread);
  if (self) {
    thrd_work_t *work = lives_proc_thread_get_work(self);
    if (work) {
      if (work->flags & LIVES_THRDFLAG_IGNORE_SYNCPTS) return FALSE;
      pthread_mutex_lock(work->pause_mutex);
      work->sync_ready = FALSE;
      pthread_mutex_unlock(work->pause_mutex);
      return _work_cond_timedwait(work, sec);
    }
  }
  return FALSE;
}

boolean lives_proc_thread_wait(void) {
  lives_proc_thread_t self = THREADVAR(proc_thread);
  if (self) {
    thrd_work_t *work = lives_proc_thread_get_work(self);
    if (work) {
      pthread_mutex_lock(work->pause_mutex);
      work->sync_ready = FALSE;
      pthread_mutex_unlock(work->pause_mutex);
      return _work_cond_wait(work);
    }
  }
  return FALSE;
}

static boolean _lives_proc_thread_cond_signal(lives_proc_thread_t lpt) {
  if (lpt) {
    thrd_work_t *work = lives_proc_thread_get_work(lpt);
    if (work) return _work_cond_signal(work);
  }
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

boolean thread_wait_loop(lives_proc_thread_t lpt, boolean full_sync, boolean wake_gui,
                         volatile boolean *control) {
  thrd_work_t *work = NULL;
  ticks_t timeout;
  uint64_t ltimeout;
  lives_proc_thread_t poller;
  GET_PROC_THREAD_SELF(self);
  lives_hook_stack_t **hook_stacks;
  uint64_t exclude_busy = THRD_STATE_BUSY, exclude_blocked = THRD_STATE_BLOCKED, exclude_waiting = 0;
  int64_t msec = 0, blimit;
  int debug = 0;
  volatile boolean ws_hooks_done = FALSE;
  if (!control) control = &ws_hooks_done;

  if (full_sync == -1) {
    debug = 1;
    full_sync = FALSE;
  }

  if (full_sync) {
    work = lives_proc_thread_get_work(lpt);
    if (work && (work->flags & LIVES_THRDFLAG_IGNORE_SYNCPTS) == LIVES_THRDFLAG_IGNORE_SYNCPTS)
      return TRUE;
  }

  blimit = THREADVAR(blocked_limit);
  timeout = THREADVAR(sync_timeout);
  ltimeout = labs(timeout);

  if (lives_proc_thread_has_states(lpt, THRD_STATE_BUSY)) exclude_busy = 0;
  if (lives_proc_thread_has_states(lpt, THRD_STATE_BLOCKED)) exclude_blocked = 0;

  if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_BUSY);
  hook_stacks = THREADVAR(hook_stacks);

  // launch a bg thread which will call all SYNC_WAIT_HOOK callbacks in turn repetedly
  // if all return TRUE, then the "poller" will run finfunc(findata)
  // then sync_hooks_done(&ws_hooks_done), in thase case we pass sync_hooks_done and "control"
  // 'control' need to become TRUE before we can return successfully
  // sync_hooks_done simple sets the value and exits
  // control can also be passed to th eother functions, allowing them to ovverride the full chack
  // if control is passed as an argument to this funcion, this allows the loop to be terminated on demoand
  // if the arg. is NULL, then a suitbale control var will be created

  poller = lives_hooks_trigger_async_sequential(lpt, hook_stacks, SYNC_WAIT_HOOK, (hook_funcptr_t)sync_hooks_done,
           (void *)control);
  if (wake_gui) {
    if (get_gov_status() == GOV_RUNNING && mainw->clutch)
      mainw->clutch = FALSE;
    g_main_context_wakeup(NULL);
  }
  while (!*control) {
    if (self && lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_request_cancel(lpt, TRUE);
      break;
    }
    lives_nanosleep(10000);
    if (wake_gui) {
      g_main_context_wakeup(NULL);
    }
    msec++;
    if (timeout && msec >= ltimeout) {
      lives_proc_thread_request_cancel(poller, FALSE);
      if (lpt) lives_proc_thread_exclude_states(lpt, exclude_waiting | exclude_busy | exclude_blocked);
      if (timeout < 0) {
        if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_TIMED_OUT);
      }
      lives_proc_thread_join(poller);
      lives_hooks_clear(hook_stacks, SYNC_WAIT_HOOK);
      weed_leaf_delete(lpt, "_control");
      return FALSE;
    }
    if (lpt && msec >= blimit && exclude_blocked) {
      lives_proc_thread_include_states(lpt, THRD_STATE_BLOCKED);
    }
  }
  if (debug) g_print("cntrol is %d\n", *control);
  if (lpt) lives_proc_thread_exclude_states(lpt, exclude_waiting | exclude_busy | exclude_blocked);
  lives_proc_thread_request_cancel(poller, FALSE);
  lives_proc_thread_join(poller);
  lives_hooks_clear(hook_stacks, SYNC_WAIT_HOOK);
  weed_leaf_delete(lpt, "_control");
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


static boolean waitsyncready(void *p, void *data) {
  thrd_work_t *work = (thrd_work_t *)data;
  return work->sync_ready;
}


boolean sync_point(const char *motive) {
  // threads can call sync_point("motive"), and will pause until sync_ready is set
  // this is similar to pause, with the difference being this is a spontaneous hook
  // whrereas pause is a response to a request hook
  //
  // the way this operates, hook callbacks can be added to the SYNC_WAIT_HOOK for self
  // the thread will block until all callbacks return TRUE, or a control override is set to TRUE
  // if the wait is too long the thread can get BLOCKED and the TIMES_OUT status
  // normally thes callbacks would check on some external condition - the value of a variable for example
  // or the status of another thread
  //
  // by contrast, pause is requested by external parties, and the thread should block until resume_request is received
  // - but only those threads flagged as pausable are able to do this
  lives_proc_thread_t lpt = THREADVAR(proc_thread);
  boolean bval = FALSE;
  volatile boolean control = FALSE;
  if (lpt) {
    thrd_work_t *work = lives_proc_thread_get_work(lpt);
    if (motive) THREADVAR(sync_motive) = lives_strdup(motive);
    lives_proc_thread_append_hook(lpt, SYNC_WAIT_HOOK, 0, waitsyncready, (void *)work);
    bval = thread_wait_loop(lpt, TRUE, FALSE, &control);
    if (motive) {
      lives_free(THREADVAR(sync_motive));
      THREADVAR(sync_motive) = NULL;
    }
  }
  return bval;
}


void _proc_thread_params_from_nullvargs(lives_proc_thread_t lpt, lives_funcptr_t func, int return_type) {
  weed_set_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, func);
  // set the type of the return_value, but not the return_value itself yet
  if (return_type > 0) weed_leaf_set(lpt, _RV_, return_type, 0, NULL);
  else if (return_type == -1) lives_proc_thread_include_states(lpt, THRD_OPT_NOTIFY);
  weed_set_int64_value(lpt, LIVES_LEAF_FUNCSIG, FUNCSIG_VOID);
}


void _proc_thread_params_from_vargs(lives_proc_thread_t lpt, lives_funcptr_t func, int return_type,
                                    const char *args_fmt, va_list xargs) {
  if (args_fmt && *args_fmt) {
    weed_set_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, func);
    if (return_type > 0) weed_leaf_set(lpt, _RV_, return_type, 0, NULL);
    else if (return_type == -1) lives_proc_thread_include_states(lpt, THRD_OPT_NOTIFY);
    weed_set_int64_value(lpt, LIVES_LEAF_FUNCSIG, funcsig_from_args_fmt(args_fmt));
    weed_plant_params_from_vargs(lpt, args_fmt, xargs);
  } else _proc_thread_params_from_nullvargs(lpt, func, return_type);
}


static lives_proc_thread_t lives_proc_thread_run(lives_thread_attr_t attr, lives_proc_thread_t lpt,
    uint32_t return_type) {
  // make proc_threads joinable and if not FG. run it
  if (lpt) {
    weed_set_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, attr);
    if (!(attr & LIVES_THRDATTR_START_UNQUEUED)) {
      // add to the pool
      lives_proc_thread_queue(lpt, attr);
    } else lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);
  }
  return lpt;
}


static lives_proc_thread_t add_garnish(lives_proc_thread_t lpt, const char *fname, lives_thread_attr_t *attr) {
  lives_hook_stack_t **hook_stacks = (lives_hook_stack_t **)lives_calloc(N_HOOK_POINTS, sizeof(lives_hook_stack_t *));

  *attr |= LIVES_THRDATTR_NOTE_TIMINGS;

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL)) {
    pthread_mutex_t *state_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(state_mutex, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, state_mutex);
  }

  lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL)) {
    pthread_mutex_t *destruct_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(destruct_mutex, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, destruct_mutex);
  }
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
    hook_stacks[i]->mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(hook_stacks[i]->mutex, NULL);
  }
  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, (void *)hook_stacks);
  weed_set_string_value(lpt, LIVES_LEAF_FUNC_NAME, fname);
  return lpt;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_nullify_on_destruction(lives_proc_thread_t lpt, void **ptr) {
  if (lpt && ptr) {
    lives_proc_thread_append_hook(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)ptr);
    return TRUE;
  }
  return FALSE;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_new(const char *fname, lives_thread_attr_t *attr) {
  lives_proc_thread_t lpt = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  add_garnish(lpt, fname, attr);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type,
    const char *args_fmt, va_list xargs) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, &attr);
  _proc_thread_params_from_vargs(lpt, func, return_type, args_fmt, xargs);
  lpt = lives_proc_thread_run(attr, lpt, return_type);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_nullvargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, &attr);
  _proc_thread_params_from_nullvargs(lpt, func, return_type);
  return lives_proc_thread_run(attr, lpt, return_type);
}


const lives_funcdef_t *lives_proc_thread_make_funcdef(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_funcptr_t func = weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, 0);
    //const lives_funcdef_t *cfdef = get_template_for_func(func);
    lives_funcdef_t *fdef;
    //if (cfdef) return cfdef;

    fdef = (lives_funcdef_t *)lives_calloc(1, sizeof(lives_funcdef_t));
    fdef->funcname = weed_get_string_value(lpt, WEED_LEAF_NAME, 0);
    fdef->uid = gen_unique_id();
    fdef->function = func;
    fdef->return_type = weed_leaf_seed_type(lpt, _RV_);
    fdef->funcsig = (uint64_t)weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, 0);
    fdef->args_fmt = args_fmt_from_funcsig(fdef->funcsig);
    //add_fdef_lookup(fdef);
    return (const lives_funcdef_t *)fdef;
  }
  return NULL;
}


lives_proc_thread_t lives_proc_thread_create_from_funcinst(lives_thread_attr_t attr, lives_funcinst_t *finst) {
  // for future use, eg. finst = create_funcinst(fdef, ret_loc, args...)
  /* // proc_thread = -this-(finst) */
  /* if (finst) { */
  /*   lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL); */
  /*   add_garnish(finst, fdef->funcname); */
  /*   if (attr & LIVES_THRDATTR_NULLIFY_ON_DESTRUCTION) lives_proc_thread_auto_nullify(&lpt); */
  /*   return lives_proc_thread_run(attr, (lives_proc_thread_t)finst, fdef->return_type); */
  /* } */
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
   lives_proc_thread_check_done() with the return : - this function is guaranteed to return FALSE whilst the thread is running
   and TRUE thereafter, the proc_thread should be freed once TRUE is returned and not before.
   for the other return_types, the appropriate join function should be called
	 and it will block until the thread has completed its
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


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_deferral_stack(uint64_t xtraflags, lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT | xtraflags;
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  return _lives_hook_add(THREADVAR(hook_stacks), LIVES_GUI_HOOK, filtflags, (void *)lpt, 0);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_fg_deferral_stack(uint64_t xtraflags,
    lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT | xtraflags;
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  return _lives_hook_add(FG_THREADVAR(hook_stacks), LIVES_GUI_HOOK, filtflags, (void *)lpt, 0);
}


LIVES_LOCAL_INLINE void append_all_to_fg_deferral_stack(lives_hook_stack_t **hstacks) {
  // hstacks MUST be threadvar, not fg_threadvar
  pthread_mutex_t *hmutex = hstacks[LIVES_GUI_HOOK]->mutex;
  pthread_mutex_lock(hmutex);
  for (LiVESList *cblist = hstacks[LIVES_GUI_HOOK]->stack; cblist; cblist = cblist->next) {
    lives_closure_t *cl = (lives_closure_t *)cblist->data;
    // need to set this first, because as soon as unlock fg thread mutex it can be triggered and cl free
    lives_proc_thread_t lpt = cl->proc_thread;

    if ((cl->flags & HOOK_STATUS_REPLACED) ||
        _lives_hook_add(FG_THREADVAR(hook_stacks), LIVES_GUI_HOOK,
                        cl->flags, (void *)cl, DTYPE_CLOSURE) != lpt)
      lives_closure_free(cl);
  }
  lives_list_free(hstacks[LIVES_GUI_HOOK]->stack);
  hstacks[LIVES_GUI_HOOK]->stack = NULL;
  pthread_mutex_unlock(hmutex);
}


LIVES_LOCAL_INLINE void prepend_all_to_fg_deferral_stack(lives_hook_stack_t **hstacks) {
  lives_hook_stack_t **fghs = FG_THREADVAR(hook_stacks);
  pthread_mutex_t *hmutex = hstacks[LIVES_GUI_HOOK]->mutex;
  pthread_mutex_lock(hmutex);
  for (LiVESList *cblist = lives_list_last(hstacks[LIVES_GUI_HOOK]->stack); cblist; cblist = cblist->prev) {
    lives_closure_t *cl = (lives_closure_t *)cblist->data;
    lives_proc_thread_t lpt = cl->proc_thread;
    cl->flags &= ~HOOK_STATUS_REPLACED;
    if (_lives_hook_add(fghs, LIVES_GUI_HOOK, cl->flags, (void *)cl, DTYPE_CLOSURE | DTYPE_PREPEND)
        != lpt)
      lives_closure_free(cl);
  }
  lives_list_free(hstacks[LIVES_GUI_HOOK]->stack);
  hstacks[LIVES_GUI_HOOK]->stack = NULL;
  pthread_mutex_unlock(hmutex);
}


LIVES_GLOBAL_INLINE lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, NULL);
  return NULL;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_ref(lives_proc_thread_t lpt) {
  if (lpt == mainw->player_proc) g_print("pproc ref\n");
  return weed_refcount_inc(lpt);
}


LIVES_GLOBAL_INLINE int lives_proc_thread_count_refs(lives_proc_thread_t lpt) {
  return weed_refcount_query(lpt);
}

// if lpt is NULL, or freed, we return TRUE
boolean lives_proc_thread_unref(lives_proc_thread_t lpt) {
  if (lpt) {
    int refs;
    if (lpt == mainw->player_proc) g_print("pproc unref\n");
    if (!(refs = weed_refcount_dec(lpt))) {
      pthread_mutex_t *destruct_mutex =
        (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);
      if (destruct_mutex && !pthread_mutex_lock(destruct_mutex)) {
        lives_hook_stack_t **lpt_hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
        lives_hook_stack_t **thread_hook_stacks = THREADVAR(hook_stacks);
        pthread_mutex_t *state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
        thrd_work_t *work = lives_proc_thread_get_work(lpt);

        if (weed_plant_has_leaf(lpt, LIVES_LEAF_CLOSURE)) break_me("free lpt with closure !");

        // will trigger destruction hook
        lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);

        if (THREADVAR(proc_thread) == lpt) {
          append_all_to_fg_deferral_stack(thread_hook_stacks);
          THREADVAR(proc_thread) = NULL;
        }

        lives_hooks_clear_all(lpt_hook_stacks, N_HOOK_POINTS);

        for (int type = N_GLOBAL_HOOKS + 1; type < N_HOOK_POINTS; type++) {
          lives_hooks_clear(thread_hook_stacks, type);
        }

        if (weed_get_int_value(lpt, "dgb_level", NULL)) {
          g_print("unref %p\n", lpt);
        }

        lives_free(lpt_hook_stacks);
        if (work) work->lpt = NULL;

        weed_plant_free(lpt);

        lives_free(state_mutex);
        if (destruct_mutex) {
          pthread_mutex_unlock(destruct_mutex);
          lives_free(destruct_mutex);
        }
        return TRUE;
      }
    }
    if (refs < 0) {
      break_me("DBL UNREF of %p\n");
    }
    return FALSE;
  }
  return TRUE;
}


// what_to_wait for will do as follows:
// first test if we can add lpt to main thread's idle stack
// if so then we check if there is anything in this thread;s deferral stack
// if so then we append those callbacks to main thread's stack, and then append lpt
// if not we just appen lpt
// in both case lpt is returned
// if lpt would be rejected then lpt2 is the blocking callback
// and we prepend all our deferrals, and unref lpt - if blocking, we wait for lpt2 instead
static lives_proc_thread_t what_to_wait_for(lives_proc_thread_t lpt, uint64_t flags, lives_hook_stack_t **hstacks) {
  lives_proc_thread_t lpt2 = _lives_hook_add(hstacks, LIVES_GUI_HOOK, flags | HOOK_CB_TEST_ADD, (void *)lpt, 0);
  if (lpt2 != lpt) return lpt;
  add_to_deferral_stack(flags, lpt);
  append_all_to_fg_deferral_stack(THREADVAR(hook_stacks));
  return lpt;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_get_replacement(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, NULL);
  return NULL;
}

// TODO - fg_service_call returns of its own accord, and needs to set retval
// - make lpt, add leaf for retloc,  ??, make closure, closure->retloc must be retval from here
static boolean _main_thread_execute_vargs(lives_funcptr_t func, const char *fname, int return_type,
    void *retval, const char *args_fmt, va_list xargs) {
  /* this function exists because GTK+ can only run certain functions in the thread which called gtk_main */
  /* amy other function can be called via this, if the main thread calls it then it will simply run the target itself */
  /* for other threads, the main thread will run the function as a fg_service. */
  /* however care must be taken since fg_service cannot run another fg_service */

  /* - this has now become quite complex. There can be several bg threads all wanting to do GUI updates. */
  /* if the main thread is idle, then it will simply pick up the request and run it. Otherewise, */
  /* the requests are queued to be run in series. If a bg thread should re-enter here itself,
     then the second request will be added to its deferral queue. There are some rules to prevent multiple requests. */
  /* otherwise, the main thread may be busy running a request from a different thread. In this case, */
  /* the request is added to the main thread's deferral stack, which it will process whenever it is idle */
  /* as a further complication, the bg thread may need to wait for its request to complete,
     e.g running a dialog where it needs a response, in this case, it can set THREADVAR(hook_hint) to contain HOOK_CB_BLOCK. */
  /* for the main thread, it also needs to service GUI callbacks like key press responses. In this case it will monitor the task
     until it finsishes, since it must run this in another background thread, and return to the gtk main loop,
     in this case it may re add itslef via an idle func so it can return and continue monitoring.
     While doing so it must still be ready to service requests from other threads, as well as more requests from the
     monitored thread. As well as this if the fg thread is running a service for an idle func or timer,
     it cannot return to the gtk main loop, as it needs to wait for the final response (TRUE or FALSE) from the subordinate timer task
     . Thus it will keep looping without returning, but still it needs to be servicing other threads.
     In particular one thread may be waitng for antother to complete and if not serviced the second thread can hang
     waiting and block the first thread, wwhich can in turn block the main thread. */

  lives_proc_thread_t lpt, lpt2;
  boolean is_fg_service = FALSE;
  // create a lives_proc_thread, which will either be run directly (fg thread)
  // passed to the fg thread
  // or queued for sequential execution (since we need to avoid nesting calls)
  if (args_fmt && *args_fmt) {
    lpt = _lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD  | LIVES_THRDATTR_START_UNQUEUED,
                                          func, fname, return_type, args_fmt, xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(LIVES_THRDATTR_FG_THREAD | LIVES_THRDATTR_START_UNQUEUED,
                 func, fname, return_type);

  // this is necessary, because if caller thread is cancelled it MUST cancel the hook callback
  // or it will block waiting
  lives_proc_thread_set_cancellable(lpt);

  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  if (is_fg_thread()) {
    // run direct
    fg_run_func(lpt, retval);
  } else {
    uint64_t hook_hints = THREADVAR(hook_hints);
    uint64_t attrs = (uint64_t)weed_get_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, NULL);
    THREADVAR(hook_hints) = 0;

    if (THREADVAR(perm_hook_hints))
      hook_hints = THREADVAR(perm_hook_hints);

    if (retval) weed_set_voidptr_value(lpt, "retloc", retval);

    if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
      weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());

    if (!is_fg_service || (hook_hints & HOOK_CB_BLOCK) || (hook_hints & HOOK_CB_PRIORITY)) {
      // first level service call
      if (!(hook_hints & HOOK_CB_PRIORITY)
          && get_gov_status() != GOV_RUNNING && FG_THREADVAR(fg_service)) {
        // call is not priority, governor loop not running and main thread is already performing
        // a service call - in this case we will add the request to main thread's deferral stack
        if (hook_hints & HOOK_OPT_NO_DEFER) {
          lives_proc_thread_unref(lpt);
          THREADVAR(hook_hints) = hook_hints;
          return FALSE;
        }

        if (hook_hints & HOOK_CB_BLOCK) lives_proc_thread_ref(lpt);

        lpt2 = what_to_wait_for(lpt, hook_hints, FG_THREADVAR(hook_stacks));

        if (lpt2 != lpt) {
          lives_proc_thread_unref(lpt);
        }
        if (hook_hints & HOOK_CB_BLOCK) {
          while (1) {
            if (lpt2 != lpt) {
              lives_proc_thread_unref(lpt);
              lpt = lpt2;
              if (!lpt) break;
            }
            lpt2 = lives_proc_thread_get_replacement(lpt);
            if (lpt2 != lpt) continue;

            // get retval
            if (lives_proc_thread_is_done(lpt)) break;

            if (get_gov_status() == GOV_RUNNING) {
              mainw->clutch = FALSE;
              lives_nanosleep_until_nonzero(mainw->clutch);
            } else lives_nanosleep(1000);
            // get retval
            if (lives_proc_thread_is_done(lpt)) break;
          }
          // freed here, but somehow gets triggered
          lives_proc_thread_unref(lpt);
        }
      } else {
        // high priority, or gov loop is running or main thread is running a service call
        // we need to wait and action this rather than add it to the queue
        //
        // if priority and not blocking, we will append lpt to the thread stack
        // then prepend our stack to maim thread
        if ((hook_hints & HOOK_CB_PRIORITY) && !(hook_hints & HOOK_CB_BLOCK)) {
          add_to_deferral_stack(hook_hints, lpt);
          prepend_all_to_fg_deferral_stack(THREADVAR(hook_stacks));
        } else {
          // special case - flag HOOK_INVALIDATE_DATA
          // - we test / prepend to fg hook stack - this will remove any func with matching data
          // from fg stack and from all thread stacks
          // following this we  action the new callback immediately
          lives_hook_stack_t **thrd_hook_stacks = THREADVAR(hook_stacks);
          if ((hook_hints & HOOK_INVALIDATE_DATA) == HOOK_INVALIDATE_DATA) {
            // check lpt valid
            lives_hook_prepend(thrd_hook_stacks, LIVES_GUI_HOOK, HOOK_INVALIDATE_DATA, lpt);
            //type, flags, func, rtype, args_fmt, ...)
            lives_hook_prepend(FG_THREADVAR(hook_stacks), LIVES_GUI_HOOK, HOOK_CB_TEST_ADD
                               | HOOK_INVALIDATE_DATA, lpt);
          } else {
            //
            // nothing or block or priority / block
            // for nothing we just append to our stack,
            // for block or prio / block, we action our stack + lpt
            add_to_deferral_stack(hook_hints, lpt);
          }
          //g_print("LLEN is %d\n", lives_list_length(THREADVAR(hook_stacks)[LIVES_GUI_HOOK]));
          // get retval
          if (hook_hints & HOOK_CB_BLOCK) {
            lives_hooks_trigger(THREADVAR(hook_stacks), LIVES_GUI_HOOK);
          }
	    // *INDENT-OFF*
	  }}}
    // *INDENT-ON*
    else {
      // we are already running in a service call, we will add any calls to our own deferral stack to
      // be actioned when we return, since we cannot nest service calls
      // lpt here is a freshly created proc_thread, it will be stored and then
      // these will be triggered after we return from waiting for the current service call
      add_to_deferral_stack(hook_hints, lpt);
      THREADVAR(hook_hints) = hook_hints;
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
  return _main_thread_execute(func, fname, return_type, retloc, "", NULL);
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


ticks_t lives_proc_thread_get_timing_info(lives_proc_thread_t lpt, int info_type) {
  if (lpt) {
    switch (info_type) {
    case (TIME_TOT_QUEUE):
      return weed_plant_has_leaf(lpt, LIVES_LEAF_QUEUED_TICKS)
             ? weed_get_int64_value(lpt, LIVES_LEAF_START_TICKS, NULL)
             - weed_get_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, NULL)
             - weed_get_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS, NULL) : 0;
    case (TIME_TOT_SYNC_START):
      return weed_get_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS, NULL);
    case (TIME_TOT_PROC):
      return weed_get_int64_value(lpt, LIVES_LEAF_END_TICKS, NULL)
             - weed_get_int64_value(lpt, LIVES_LEAF_START_TICKS, NULL);
    default: break;
    }
  }
  return 0;
}


LIVES_GLOBAL_INLINE char *lives_proc_thread_get_funcname(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_string_value(lpt, LIVES_LEAF_FUNC_NAME, NULL);
  return NULL;
}


LIVES_GLOBAL_INLINE funcsig_t lives_proc_thread_get_funcsig(lives_proc_thread_t lpt) {
  return weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL);
}


LIVES_GLOBAL_INLINE uint32_t lives_proc_thread_get_rtype(lives_proc_thread_t lpt) {
  return weed_leaf_seed_type(lpt, _RV_);
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


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    lives_proc_thread_ref(lpt);
    if (lpt) {
      pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
      if (state_mutex) {
        uint64_t tstate, new_state;

	// if new state includes DESTROYED, refccount will be zero
	// so we do not want to ref it and unref it, else this would cause recursion when refcount
	// goes back to zero
	boolean do_refs = lives_proc_thread_count_refs(lpt) > 0;

        if (do_refs) lives_proc_thread_ref(lpt);
	if (lpt == mainw->player_proc) g_print("pproc states inc\n");

        pthread_mutex_lock(state_mutex);
        tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
        weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | state_bits);
        pthread_mutex_unlock(state_mutex);

        new_state = tstate | state_bits;

        if (!(new_state & THRD_BLOCK_HOOKS)) {
          lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
          // only new bits
          state_bits &= ~tstate;

          if (state_bits & THRD_STATE_PREPARING) {
            lives_hooks_trigger(hook_stacks, PREPARING_HOOK);
          }

          if (state_bits & THRD_STATE_RUNNING) {
            lives_hooks_trigger(hook_stacks, TX_START_HOOK);
          }

	  if (state_bits & THRD_STATE_IDLING) {
            lives_hooks_trigger(hook_stacks, IDLE_HOOK);
          }

          if (state_bits & THRD_STATE_BLOCKED) {
            lives_hooks_trigger(hook_stacks, TX_BLOCKED_HOOK);
          }

          if (state_bits & THRD_STATE_TIMED_OUT) {
            lives_hooks_trigger(hook_stacks, TIMED_OUT_HOOK);
          }

          if (state_bits & THRD_STATE_ERROR) {
            lives_hooks_trigger(hook_stacks, ERROR_HOOK);
          }

          if (state_bits & THRD_STATE_CANCELLED) {
            lives_hooks_trigger(hook_stacks, CANCELLED_HOOK);
          }

          if (state_bits & THRD_STATE_PAUSED) {
            lives_hooks_trigger(hook_stacks, PAUSED_HOOK);
          }

          if (state_bits & THRD_STATE_COMPLETED) {
            lives_hooks_trigger(hook_stacks, COMPLETED_HOOK);

	    if (!(new_state & THRD_STATE_DESTROYING)) {
	      // set state to include FINISHED. This is not a hook trigger, so by checking for this state
	      // we can be certain that all hook callbacks have been run and it is safe to unref the proc_thread
	      pthread_mutex_lock(state_mutex);
	      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
	      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | THRD_STATE_FINISHED);
	      pthread_mutex_unlock(state_mutex);
	      new_state = tstate | THRD_STATE_FINISHED;
	    }
	  }

          if (state_bits & THRD_STATE_DESTROYED) {
	    // this hook type is triggered when refcount is zero
	    // the proc_thread must not be reffed or unreffed
            lives_hooks_trigger(hook_stacks, DESTRUCTION_HOOK);
          }
        }
        if (do_refs) lives_proc_thread_unref(lpt);
        return new_state;
      }
    }
  }
  return THRD_STATE_INVALID;
}


uint64_t lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    lives_proc_thread_ref(lpt);
    if (lpt) {
      uint64_t new_state = THRD_STATE_INVALID;
      pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
      if (state_mutex) {
        uint64_t tstate;
        lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
	if (lpt == mainw->player_proc) g_print("pproc states exc\n");
        pthread_mutex_lock(state_mutex);
        tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
        new_state = tstate & ~state_bits;
        weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, new_state);
        pthread_mutex_unlock(state_mutex);

        if (!(new_state & THRD_BLOCK_HOOKS)) {
          tstate &= state_bits;
          if (tstate & THRD_STATE_PAUSED) lives_hooks_trigger(hook_stacks, RESUMING_HOOK);
        }
      }
      lives_proc_thread_unref(lpt);

      // because nanosleep is a cancellation point
      if (state_bits & THRD_STATE_BUSY) lives_cancel_point;
      return new_state;
    }
  }
  return THRD_STATE_INVALID;
}

// the following functions should NEVER be called if there is a possibility that the proc_thread
// may be destroyed (state includes DONTACARE, return type is 0, or it is a hook callback with options
// ONESHOT or stack_type REMOVE_IF_FALSE)
//
// however if the caller holds a reference on the proc_thread, then these are safe to call

// check if thread finished normally
LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_FINISHED)));
}


// check if thread is idling (only set if idlefunc was queued at least once already)
LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_idling(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_IDLING))) return TRUE;
  return FALSE;
}


// check if thread finished normally, is idling or will be destroyed
LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_done(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_check_states(lpt, THRD_STATE_FINISHED | THRD_STATE_IDLING))) return TRUE;
  return lives_proc_thread_will_destroy(lpt);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_will_destroy(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_check_states(lpt, THRD_STATE_WILL_DESTROY))
      == THRD_STATE_WILL_DESTROY) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_invalid(lives_proc_thread_t lpt) {
  if (!lpt || (lpt && (lives_proc_thread_get_state(lpt) == THRD_STATE_INVALID))) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_SIGNALLED)));
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_signal_data(lives_proc_thread_t lpt, int64_t *tidx, void **data) {
  lives_thread_data_t *tdata
    = (lives_thread_data_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_SIGNAL_DATA, NULL);
  if (data) *data = tdata;
  if (tdata) {
    if (tidx) *tidx = tdata->idx;
    return tdata->signum;
  }
  return 0;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_cancellable(lives_proc_thread_t lpt) {
  if (lpt) lives_proc_thread_include_states(lpt, THRD_OPT_CANCELLABLE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_OPT_CANCELLABLE)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancelled(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_CANCELLED)));
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_pauseable(lives_proc_thread_t lpt, boolean state) {
  if (lpt) {
    if (state) lives_proc_thread_include_states(lpt, THRD_OPT_PAUSEABLE);
    else lives_proc_thread_exclude_states(lpt, THRD_OPT_PAUSEABLE);
  }
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_pauseable(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_OPT_PAUSEABLE)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_paused(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_PAUSED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_cancel(lives_proc_thread_t lpt, boolean dontcare) {
  if (!lpt || !lives_proc_thread_get_cancellable(lpt)) return FALSE;
  if (lives_proc_thread_is_done(lpt)) return FALSE;
  if (dontcare) if (!lives_proc_thread_dontcare(lpt)) return FALSE;
  lives_proc_thread_include_states(lpt, THRD_STATE_CANCEL_REQUESTED);
  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancel_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_CANCEL_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t self) {
  if (self) {
    lives_proc_thread_exclude_states(self, THRD_STATE_CANCEL_REQUESTED);
    lives_proc_thread_include_states(self, THRD_STATE_CANCELLED);
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_resume(lives_proc_thread_t lpt) {
  if (!lpt || !lives_proc_thread_get_paused(lpt)) return FALSE;
  lives_proc_thread_include_states(lpt, THRD_STATE_RESUME_REQUESTED);
  _lives_proc_thread_cond_signal(lpt);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_resume_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_RESUME_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_resume(lives_proc_thread_t self) {
  if (self) {
    // idling ??
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSED | THRD_STATE_RESUME_REQUESTED | THRD_STATE_IDLING);
  }
  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_pause(lives_proc_thread_t lpt) {
  if (!lpt || !lives_proc_thread_get_pauseable(lpt)) return FALSE;
  if (!lives_proc_thread_get_paused(lpt))
    lives_proc_thread_include_states(lpt, THRD_STATE_PAUSE_REQUESTED);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_pause_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_PAUSE_REQUESTED)));
}


static boolean get_unpaused(void *obj, void *xlpt) {
  lives_proc_thread_t lpt = (lives_proc_thread_t)xlpt;
  return lives_proc_thread_get_resume_requested(lpt);
}


LIVES_GLOBAL_INLINE void lives_proc_thread_pause(lives_proc_thread_t self) {
  // self function to called for pausable proc_threads when pause is requested
  // for idle / loop funcs, do not call this, the thread will exit unqueed instead
  // and be resubmitted on resume
  if (self) {
    if (!lives_proc_thread_get_pauseable(self)) return;
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);
    lives_proc_thread_include_states(self, THRD_STATE_PAUSED);
    if (!lives_proc_thread_wait()) {
      lives_proc_thread_append_hook(self, SYNC_WAIT_HOOK, 0, get_unpaused, self);
      sync_point("paused");
    }
    lives_proc_thread_resume(self);
  }
}


// calls pthread_cancel on underlying thread
// - cleanup function casues the thread to the normal post cleanup, so lives_proc_thread_join_*
// will work as normal (though the values returned will not be valid)
// however this should still be called, and the proc_thread freed as normal
// (except for auto / dontcare proc_threads)
// there is a small chance the cancellation could occur either before or after the function is called
LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel_immediate(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES);
    pthread_t base_thread = (pthread_t)weed_get_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, NULL);
    if (base_thread) {
      pthread_cancel(base_thread);
      return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_START_TICKS, NULL) : 0;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_running(lives_proc_thread_t lpt) {
  // state is maintaained even if cancelled or finished
  return (!lpt || (lives_proc_thread_has_states(lpt, THRD_STATE_RUNNING)));
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


LIVES_GLOBAL_INLINE void lives_proc_thread_sync_continue(lives_proc_thread_t lpt) {
  boolean *control = (boolean *)weed_get_voidptr_value(lpt, "_control", NULL);
  *control = TRUE;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_sync_ready(lives_proc_thread_t lpt) {
  // this cannot be handled via states as it affects the underlying lives_thread
  _lives_proc_thread_cond_signal(lpt);
}


// this function is safe to call even in case
// timeout is in seconds
int lives_proc_thread_wait_done(lives_proc_thread_t lpt, double timeout) {
  if (lpt) {
    ticks_t slptime = LIVES_QUICK_NAP * 10;
    lives_proc_thread_ref(lpt);
    if (timeout) timeout *= ONE_BILLION;
    while (timeout >= 0. && !lives_proc_thread_is_done(lpt)
           && !lives_proc_thread_is_invalid(lpt)) {
      // fg thread needs to service requests, otherwise the thread we are waiting on
      // may block waiting for a fg_service_call
      if (is_fg_thread()) fg_service_fulfill();
      // wait 10 usec
      lives_nanosleep(slptime);
      if (timeout) timeout -= slptime;
    }
    if (!lives_proc_thread_is_done(lpt)) {
      lives_proc_thread_unref(lpt);
      return LIVES_RESULT_FAIL;
    }
    lives_proc_thread_unref(lpt);
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_ERROR;
}


LIVES_LOCAL_INLINE void _lives_proc_thread_join(lives_proc_thread_t lpt, double timeout) {
  // version without a return value will free lpt
  lives_proc_thread_wait_done(lpt, timeout);

  // caller should ref the proc_thread if it wants to avoid this
  lives_proc_thread_unref(lpt);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t lpt) {
  // version without a return value will free lpt
  _lives_proc_thread_join(lpt, 0.);
}

#define _join(lpt, stype) lives_proc_thread_wait_done(lpt, 0.);	\
  if (!lives_proc_thread_is_invalid(lpt)) {				\
    lives_nanosleep_while_false(weed_plant_has_leaf(lpt, _RV_) == WEED_TRUE);} \
  return weed_get_##stype##_value(lpt, _RV_, NULL);

LIVES_GLOBAL_INLINE int lives_proc_thread_join_int(lives_proc_thread_t lpt) { _join(lpt, int);}
LIVES_GLOBAL_INLINE double lives_proc_thread_join_double(lives_proc_thread_t lpt) {_join(lpt, double);}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_boolean(lives_proc_thread_t lpt) { _join(lpt, boolean);}
LIVES_GLOBAL_INLINE int64_t lives_proc_thread_join_int64(lives_proc_thread_t lpt) {_join(lpt, int64);}
LIVES_GLOBAL_INLINE char *lives_proc_thread_join_string(lives_proc_thread_t lpt) {_join(lpt, string);}
LIVES_GLOBAL_INLINE weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t lpt) {_join(lpt, funcptr);}
LIVES_GLOBAL_INLINE void *lives_proc_thread_join_voidptr(lives_proc_thread_t lpt) {_join(lpt, voidptr);}
LIVES_GLOBAL_INLINE weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t lpt) {_join(lpt, plantptr);}


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

boolean lives_proc_thread_dontcare(lives_proc_thread_t lpt) {
  if (!lpt) return FALSE;
  // we presume entering here that the lpt hasnt been freed
  // what we want to avoid is that the we set the flag right after the runner thread has finished
  // - either it can free lpt and the we will segfault reading flags
  // or else we can read flags and think it is still running
  // so the order has to be - lock a mutex -> around check flags to set state
  // check if state is finsihed - if so, unref the lpt
  // otherwise leave it
  // return TRUE if we set the state

  lives_proc_thread_ref(lpt);

  // stop proc_thread from being freed as soon as we set the flagbit
  if (lives_proc_thread_check_finished(lpt)) {
    // if the proc_thread already completed, we just unref it
    lives_proc_thread_unref(lpt);
    lives_proc_thread_unref(lpt);
    return FALSE;
  }
  lives_proc_thread_include_states(lpt, THRD_OPT_DONTCARE);
  lives_proc_thread_unref(lpt);
  return TRUE;
}


boolean lives_proc_thread_dontcare_nullify(lives_proc_thread_t lpt, void **thing) {
  if (lpt) {
    lives_proc_thread_append_hook(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)thing);
    return lives_proc_thread_dontcare(lpt);
  }
  return FALSE;
}


static void pthread_cleanup_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  // if pthread_cancel was called, the thread immediately jumps here,
  // because will called pthread_cleanup_push
  // after this, the pthread will exit
  if (lives_proc_thread_get_signalled(info))
    lives_hooks_trigger(THREADVAR(hook_stacks), THREAD_EXIT_HOOK);
}


// FINAL for a proc_thread will either be:
// - DESTROYED - the proc_thread was flagged DONTCARE, or has no monitored return
//	the state will first go to COMPLETED, then DESTROYED
//	(if the thread was reffed, then the state will stay at COMPLETED unreffed)
// - IDLING - proc_thread was flagged as idlefunc, and the function returned TRUE
// - FINISHED - in all other cases
//
// thus adding a hook callback for COMPLETED will work in all cases, except for idlefuncs
// where we can add a hook for IDLING
// after requeueing the idlefunc, the idling state will be removed
//
// state may be combined with: - unqueued (for idling), cancelled, error, timed_out, etc.
// paused is not a final state, the proc_thread should be cancel_requested first, then resume_requested
// for cancel_immediate, there will be no final state, but thread_exit will be triggered
static void lives_proc_thread_set_final_state(lives_proc_thread_t lpt, boolean is_hook_cb) {
  uint64_t attrs = (uint64_t)weed_get_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, NULL);
  uint64_t state;
  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);

  weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, NULL);

  //if (destruct_mutex) pthread_mutex_lock(destruct_mutex);

  state = lives_proc_thread_get_state(lpt);

  if (state & THRD_OPT_IDLEFUNC) {
    if (weed_get_boolean_value(lpt, _RV_, 0)) {
      lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES);
      // will trigger IDLE hook
      lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED | THRD_STATE_IDLING);
      return;
    }
  }

  if (!is_hook_cb && ((state & THRD_OPT_DONTCARE) || (!ret_type && !(state & THRD_OPT_NOTIFY)))) {
    // if dontcare, or there is no return type then we should unref the proc_thread
    // unless it is a hook callback, then we justput it back in the stack
    // TODO - check this

    // call the COMPLETED hook, but with DESTROYING set
    lives_proc_thread_include_states(lpt, THRD_STATE_WILL_DESTROY);

    // TODO - check this, now we have is_hook_cb
    if (!(attrs & LIVES_THRDATTR_NO_UNREF)) {
      // if called via a hook trigger or fg_service_call, we set attr to avoid freeing
      lives_proc_thread_unref(lpt);
    }
  } else {
    // once a proc_thread reaches this state it is guaranteed not to be freed
    // provided DESTROYING is not set
    // this is a hook trigger, the final state to check for is FINISHED
    lives_proc_thread_include_states(lpt, THRD_STATE_COMPLETED);
  }
}


// for lives_proc_threads, this is a wrapper function which gets called from the worker thread
// the real payload is run via call_funcsig, which allow any type of function with any return type
// to be wrapped
static void *proc_thread_worker_func(void *args) {
  lives_proc_thread_t lpt = (lives_proc_thread_t)args;

  lives_proc_thread_ref(lpt);
  THREADVAR(proc_thread) = lpt;
  weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, (void *)pthread_self());

  // if pthread_cancel is called, the cleanup_func will be called and pthread will exit
  // this makes sure we trigger the THREAD_EXIT hook callbacks
  pthread_cleanup_push(pthread_cleanup_func, args);

  call_funcsig(lpt);

  pthread_cleanup_pop(0);

  lives_proc_thread_set_final_state(lpt, FALSE);
  lives_proc_thread_unref(lpt);

  return NULL;
}


boolean fg_run_func(lives_proc_thread_t lpt, void *rloc) {
  // rloc should be a pointer to a variable of the correct type. After calling this,
  // name should change, this can run by bg
  //
  boolean bret = FALSE;

  lives_proc_thread_ref(lpt);

  if (!lives_proc_thread_get_cancel_requested(lpt)) {

    call_funcsig(lpt);

    if (rloc) {
      if (weed_leaf_seed_type(lpt, _RV_) == WEED_SEED_STRING) *(char **)rloc = weed_get_string_value(lpt, _RV_, NULL);
      else _weed_leaf_get(lpt, _RV_, 0, rloc);
    }

    lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);
  } else lives_proc_thread_cancel(lpt);

  // can unref lpt !!
  lives_proc_thread_set_final_state(lpt, TRUE);
  lives_proc_thread_unref(lpt);

  return bret;
}


/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
// THRD_ATTR_WAIT_START can be provided at this point if not already
void lives_proc_thread_queue(lives_proc_thread_t lpt, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  thrd_work_t *mywork;

  /// tell the thread to clean up after itself [but it won't delete lpt]
  attr |= LIVES_THRDATTR_AUTODELETE;

  attr |= LIVES_THRDATTR_IS_PROC_THREAD;
  lives_proc_thread_include_states(lpt, THRD_STATE_QUEUED);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_UNQUEUED |  THRD_STATE_IDLING);

  if (attr & LIVES_THRDATTR_DETACHED) {
    lives_proc_thread_exclude_states(lpt, THRD_OPT_NOTIFY);
    lives_proc_thread_include_states(lpt, THRD_OPT_DONTCARE);
  }

  // add the work to the pool
  mywork = lives_thread_create(NULL, attr, proc_thread_worker_func, (void *)lpt);

  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);

  if (attr & LIVES_THRDATTR_WAIT_START) {
    // WAIT_START: caller waits for thread to run or finish
    lives_nanosleep_until_nonzero(mywork->flags & (LIVES_THRDFLAG_RUNNING | LIVES_THRDFLAG_HOLD |
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
// npoolthreads is the number of available (free) poolthreads, we try to maintain this > ntasks
static volatile int npoolthreads, rnpoolthreads;
static pthread_t **poolthrds;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t allctx_rwlock;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
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
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  mainw->all_hstacks = lives_list_remove_data(mainw->all_hstacks, tdata->vars.var_hook_stacks, FALSE);
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    lives_hooks_clear(tdata->vars.var_hook_stacks, i);
    lives_free(tdata->vars.var_hook_stacks[i]->mutex);
    lives_free(tdata->vars.var_hook_stacks[i]);
  }
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
    tdata->vars.var_hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
    tdata->vars.var_hook_stacks[i]->mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(tdata->vars.var_hook_stacks[i]->mutex, NULL);
  }

  if (idx) {
    pthread_mutex_lock(&mainw->all_hstacks_mutex);
    mainw->all_hstacks = lives_list_append(mainw->all_hstacks, THREADVAR(hook_stacks));
    pthread_mutex_unlock(&mainw->all_hstacks_mutex);
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


boolean do_something_useful(lives_thread_data_t *tdata) {
  /// yes, why don't you lend a hand instead of just lying around nanosleeping...
  lives_proc_thread_t lpt;
  LiVESList *list;
  thrd_work_t *mywork;

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

  if (!list->data) {
    //pthread_mutex_lock(&twork_count_mutex);
    ntasks--;
    //pthread_mutex_unlock(&twork_count_mutex);
    pthread_mutex_unlock(&twork_mutex);
    list->next = list->prev = NULL;
    return FALSE;
  }

  pthread_mutex_unlock(&twork_mutex);

  mywork = (thrd_work_t *)list->data;
  list->next = list->prev = NULL;

  lpt = mywork->lpt;
  if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_PREPARING);

  mywork->busy = tdata->idx;

  mywork->flags &= ~LIVES_THRDFLAG_QUEUED_WAITING;

  // wait for SYNC READY
  if (!mywork->sync_ready) {
    if (lpt) {
      lives_proc_thread_include_states(lpt, THRD_STATE_WAITING);
      if (mywork->flags & LIVES_THRDFLAG_NOTE_TIMINGS)
        weed_set_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS,
                             lives_get_current_ticks());
    }
    mywork->flags |= LIVES_THRDFLAG_HOLD;
    _work_cond_wait(mywork);
    if (lpt) {
      lives_proc_thread_exclude_states(lpt, THRD_STATE_WAITING);
      if (mywork->flags & LIVES_THRDFLAG_NOTE_TIMINGS)
        weed_set_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS,
                             lives_get_current_ticks() -
                             weed_get_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS, NULL));

    }
    mywork->flags &= ~LIVES_THRDFLAG_HOLD;
  }

  // RUN TASK
  widget_context_wrapper(mywork);

  for (int i = 0; i < N_HOOK_POINTS; i++) {
    lives_hooks_clear(tdata->vars.var_hook_stacks, i);
  }

  // make sure caller has noted that thread finished
  lives_nanosleep_until_zero(mywork->flags & LIVES_THRDFLAG_WAIT_START);

  if (mywork->flags & LIVES_THRDFLAG_AUTODELETE) {
    lives_thread_free((lives_thread_t *)list);
  } else mywork->done = tdata->idx;

  pthread_mutex_lock(&twork_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_mutex);

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
  //pthread_t pself = pthread_self();

  lives_thread_data_t *tdata;

  // must do before anything else
  tdata = lives_thread_data_create(tid);

  lives_widget_context_push_thread_default(tdata->ctx);

  while (!threads_die) {
    if (!skip_wait) {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += POOL_TIMEOUT_SEC;
      pthread_mutex_lock(&tcond_mutex);
      //g_print("thrd %d (0x%lx) WAITING : %d\n", tid, pself, tid);
      rc = pthread_cond_timedwait(&tcond, &tcond_mutex, &ts);
      pthread_mutex_unlock(&tcond_mutex);
      //g_print("thrd %d (0x%lx) woke\n", tid, pself);
      if (rc == ETIMEDOUT) {
        // if the thread is waiting around doing nothing, exit, maybe free up some resources
        //g_print("thrd %d (0x%lx) aged\n", tid, pself);
        if (!pthread_mutex_trylock(&pool_mutex)) {
          //g_print("thrd %d (0x%lx) leaving\n", tid, pself);
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
    //g_print("thrd %d (0x%lx) check for owrk\n", tid, pself);
    skip_wait = do_something_useful(tdata);
#ifdef USE_RPMALLOC
    if (skip_wait) {
      // g_print("thrd %d (0x%lx) did someting\n", tid, pself);
      if (rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_collect();
      }
    }
#endif
  }
  //  g_print("thrd %d (0x%lx) killed\n", tid, pself);

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
  twork_list = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  }
}


void lives_threadpool_finish(void) {
  threads_die = TRUE;
  pthread_mutex_lock(&tcond_mutex);
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
  rnpoolthreads = npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_list);
  twork_list = twork_last = NULL;
  ntasks = 0;
}


LIVES_GLOBAL_INLINE void lives_thread_free(lives_thread_t *thread) {
  if (thread) {
    thrd_work_t *work = (thrd_work_t *)thread->data;
    uint64_t flags = work->flags;
    lives_free(work->pcond);
    lives_free(work->pause_mutex);
    lives_free(work);
    if (!(flags & LIVES_THRDFLAG_NOFREE_LIST))
      lives_list_free(thread);
  }
}


thrd_work_t *lives_thread_create(lives_thread_t **threadptr, lives_thread_attr_t attr,
                                 lives_thread_func_t func, void *arg) {
  LiVESList *list = NULL;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  list = lives_list_append(list, work);
  work->func = func;
  work->attr = attr;
  work->arg = arg;
  work->flags = LIVES_THRDFLAG_QUEUED_WAITING;
  work->caller = THREADVAR(idx);

  work->pause_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(work->pause_mutex, NULL);
  work->pcond = (pthread_cond_t *)lives_malloc(sizeof(pthread_cond_t));
  pthread_cond_init(work->pcond, NULL);

  if (threadptr) work->flags |= LIVES_THRDFLAG_NOFREE_LIST;

  if (attr & LIVES_THRDATTR_IS_PROC_THREAD) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)arg;
    work->lpt = lpt;
    if (attr & LIVES_THRDATTR_IGNORE_SYNCPTS) {
      work->flags |= LIVES_THRDFLAG_IGNORE_SYNCPTS;
    }
    if (attr & LIVES_THRDATTR_IDLEFUNC) {
      lives_proc_thread_include_states(lpt, THRD_OPT_IDLEFUNC);
    }
    lives_proc_thread_set_work(lpt, work);
    if (attr & LIVES_THRDATTR_NOTE_TIMINGS) {
      weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS,
                           lives_get_current_ticks());
      work->flags |= LIVES_THRDFLAG_NOTE_TIMINGS;
    }
  }

  if (!threadptr || (attr & LIVES_THRDATTR_AUTODELETE)) {
    work->flags |= LIVES_THRDFLAG_AUTODELETE;
  }

  if (!(attr & LIVES_THRDATTR_WAIT_SYNC)) work->sync_ready = TRUE;

  pthread_mutex_lock(&twork_mutex);
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

  ntasks++;

  pthread_mutex_unlock(&twork_mutex);

  pthread_mutex_lock(&pool_mutex);

  // check if any threads aged (exited), and if necessary re-create enough
  if (rnpoolthreads && npoolthreads < rnpoolthreads)
    for (int i = 0; i < rnpoolthreads; i++) {
      if (poolthrds[i]) {
        lives_thread_data_t *tdata = get_thread_data_by_idx(i + 1);
        if (!tdata || tdata->exited) {
          if (tdata && tdata->exited) pthread_join(*poolthrds[i], NULL);
          lives_free(poolthrds[i]);
          poolthrds[i] = NULL;
        }
      }
      // relaunch thread, npoolthreads ---> rnpoolthreads
      if (npoolthreads < ntasks && !poolthrds[i]) {
        poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
        pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&tcond_mutex);
        pthread_cond_signal(&tcond);
        pthread_mutex_unlock(&tcond_mutex);
        npoolthreads++;
      }
    }
  if (ntasks <= rnpoolthreads) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
  } else {
    // we need more threads to service all tasks
    int extrs = MAX(MINPOOLTHREADS, ntasks - rnpoolthreads);
    poolthrds =
      (pthread_t **)lives_realloc(poolthrds, (rnpoolthreads + extrs) * sizeof(pthread_t *));
    for (int i = rnpoolthreads; i < rnpoolthreads + extrs; i++) {
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    rnpoolthreads += extrs;
    npoolthreads = rnpoolthreads;
  }

  pthread_mutex_unlock(&pool_mutex);

  if (threadptr) *threadptr = list;

  return work;
}


uint64_t lives_thread_join(lives_thread_t *thread, void **retval) {
  thrd_work_t *task = (thrd_work_t *)thread->data;
  uint64_t nthrd = 0;
  boolean fg = is_fg_thread();

  if (task->flags & LIVES_THRDFLAG_AUTODELETE) {
    LIVES_FATAL("lives_thread_join() called on an autodelete thread");
    return 0;
  }

  while (!task->busy) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    if (!task->busy) lives_nanosleep(1000);
  }

  while (!task->done) {
    if (fg) fg_service_fulfill();
    if (!task->done) lives_nanosleep(1000);
  }

  nthrd = task->done;
  lives_thread_free(thread);
  return nthrd;
}


LIVES_GLOBAL_INLINE uint64_t lives_thread_done(lives_thread_t *thrd) {
  thrd_work_t *task = (thrd_work_t *)thrd->data;
  return task->done;
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
      refcount->count = 1;
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
  return -1;
}


LIVES_GLOBAL_INLINE int refcount_dec(lives_refcounter_t *refcount) {
  if (check_refcnt_init(refcount)) {
    int count;
    pthread_mutex_lock(&refcount->mutex);
    count = refcount->count;
    if (count > 0) count = --refcount->count;
    else break_me("double unref");
    pthread_mutex_unlock(&refcount->mutex);
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
  return -1;
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
  return -1;
}


LIVES_GLOBAL_INLINE int weed_refcount_dec(weed_plant_t *plant) {
  // value of 0 indicates the plant should be freed with weed_plant_free()
  // if plant does not have a refcounter, 0 will be returned.
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return 0;
    return refcount_dec(refcnt);
  }
  return -1;
}


LIVES_GLOBAL_INLINE int weed_refcount_query(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return 0;
    return refcount_query(refcnt);
  }
  return -1;
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
    weed_leaf_set_undeletable(plant, LIVES_LEAF_REFCOUNTER, WEED_FALSE);
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
    lives_proc_thread_t tinfo = thrdinfo->var_proc_thread;
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
