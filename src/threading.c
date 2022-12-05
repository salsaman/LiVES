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

boolean thread_wait_loop(lives_proc_thread_t lpt, thrd_work_t *work, boolean full_sync, boolean wake_gui, volatile boolean *control) {
  ticks_t timeout;
  uint64_t ltimeout;
  lives_proc_thread_t poller;
  lives_hook_stack_t **hook_stacks;
  uint64_t exclude_busy = THRD_STATE_BUSY, exclude_blocked = THRD_STATE_BLOCKED, exclude_waiting = 0;
  int64_t msec = 0, blimit;
  volatile boolean ws_hooks_done = FALSE;
  if (!control) control = &ws_hooks_done;

  if (full_sync) {
    if (work && (work->flags & LIVES_THRDFLAG_IGNORE_SYNCPT) == LIVES_THRDFLAG_IGNORE_SYNCPT)
      return TRUE;
  }

  blimit = THREADVAR(blocked_limit);
  timeout = THREADVAR(sync_timeout);
  ltimeout = labs(timeout);

  if (lives_proc_thread_has_states(lpt, THRD_STATE_BUSY)) exclude_busy = 0;
  if (lives_proc_thread_has_states(lpt, THRD_STATE_BLOCKED)) exclude_blocked = 0;

  if (work && full_sync) {
    exclude_waiting = THRD_STATE_WAITING;
    work->sync_ready = FALSE;
    hook_stacks = work->hook_stacks;
    lives_proc_thread_include_states(lpt, THRD_STATE_WAITING | THRD_STATE_BUSY);
  } else {
    if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_BUSY);
    hook_stacks = THREADVAR(hook_stacks);
  }

  // launch a bg thread which will call all SYNC_WAIT_HOOK callbacks in turn repetedly
  // once all return true then sync_hooks_done(&ws_hooks_done)

  poller = lives_hooks_trigger_async_sequential(lpt, hook_stacks, SYNC_WAIT_HOOK, sync_hooks_done, (void *)control);

  while (!control) {
    lives_nanosleep(10000);
    if (wake_gui) g_main_context_wakeup(NULL);
    msec++;
    if (timeout && msec >= ltimeout) {
      lives_proc_thread_cancel(poller, FALSE);
      if (lpt) lives_proc_thread_exclude_states(lpt, exclude_waiting | exclude_busy | exclude_blocked);
      if (work) work->sync_ready = TRUE;
      if (timeout < 0) {
        if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_TIMED_OUT);
      }
      lives_proc_thread_join(poller);
      lives_proc_thread_unref(poller);
      return FALSE;
    }
    if (lpt && msec >= blimit && exclude_blocked) {
      lives_proc_thread_include_states(lpt, THRD_STATE_BLOCKED);
    }
  }
  if (lpt) lives_proc_thread_exclude_states(lpt, exclude_waiting | exclude_busy | exclude_blocked);
  lives_proc_thread_cancel(poller, FALSE);
  lives_proc_thread_join(poller);
  lives_proc_thread_unref(poller);
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
      lives_hook_append(NULL, SYNC_WAIT_HOOK, 0, waitsyncready, work);
      bval = thread_wait_loop(NULL, work, TRUE, FALSE, NULL);
      lives_hook_remove(THREADVAR(hook_stacks), SYNC_WAIT_HOOK, waitsyncready, work);
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


static int proc_thread_unrefs_block(void) {return pthread_mutex_lock(&proc_thread_kill_mutex);}

static int proc_thread_unrefs_unblock(void) {return pthread_mutex_unlock(&proc_thread_kill_mutex);}


// return TRUE if freed
LIVES_GLOBAL_INLINE boolean lives_proc_thread_free(lives_proc_thread_t lpt) {
  if (!lpt) return TRUE;

  /* // wait for the thread to finish - either completing or being cancelled */
  /* while (!lives_proc_thread_is_done(lpt)) { */
  /*   if (is_fg_thread()) while (fg_service_fulfill()); */
  /*   lives_nanosleep(100000); */
  /* } */

  // this is a "last chance" hook. It is possible to ref the proc_thread here and defer its immediate destruction
  return lives_proc_thread_unref(lpt);
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
    if (!(attr & LIVES_THRDATTR_FG_THREAD)) {
      if (attr & LIVES_THRDATTR_NOTE_STTIME) weed_set_int64_value(lpt, LIVES_LEAF_START_TICKS,
            lives_get_current_ticks());
      // add to the pool
      run_proc_thread(lpt, attr);
      if (!return_type) return NULL;
    } else lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);
  }
  return lpt;
}


static lives_proc_thread_t add_garnish(lives_proc_thread_t lpt, const char *fname) {
  lives_hook_stack_t **hook_stacks = (lives_hook_stack_t **)lives_calloc(N_HOOK_POINTS, sizeof(lives_hook_stack_t *));

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
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
    hook_stacks[i]->mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(hook_stacks[i]->mutex, NULL);
  }
  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, (void *)hook_stacks);
  weed_set_string_value(lpt, LIVES_LEAF_FUNC_NAME, (void *)fname);
  return lpt;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_nullify_on_destruction(lives_proc_thread_t lpt, void **ptr) {
  if (lpt && ptr) {
    lives_proc_thread_hook_append(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)ptr);
    return TRUE;
  }
  return FALSE;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_new(const char *fname, lives_thread_attr_t attr) {
  lives_proc_thread_t lpt = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  add_garnish(lpt, fname);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type,
    const char *args_fmt, va_list xargs) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, attr);
  _proc_thread_params_from_vargs(lpt, func, return_type, args_fmt, xargs);
  return lives_proc_thread_run(attr, lpt, return_type);
}


lives_proc_thread_t _lives_proc_thread_create_nullvargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, attr);
  _proc_thread_params_from_nullvargs(lpt, func, return_type);
  return lives_proc_thread_run(attr, lpt, return_type);
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
      lives_nanosleep(ONE_MILLION);
    }
  } else pthread_mutex_lock(mutex);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_deferral_stack(uint64_t xtraflags, lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_FG_THREAD | xtraflags;
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  if (THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
    return lives_hook_prepend(THREADVAR(hook_stacks), INTERNAL_HOOK_0,
                              filtflags, NULL, (void *)lpt);
  else
    return lives_hook_append(THREADVAR(hook_stacks), INTERNAL_HOOK_0,
                             filtflags, NULL, (void *)lpt);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_fg_deferral_stack(uint64_t xtraflags,
    lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_FG_THREAD | xtraflags;
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  if (THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
    return lives_hook_append(FG_THREADVAR(hook_stacks), INTERNAL_HOOK_0,
                             filtflags, NULL, (void *)lpt);
  else
    return lives_hook_append(FG_THREADVAR(hook_stacks), INTERNAL_HOOK_0,
                             filtflags, NULL, (void *)lpt);
}


lives_proc_thread_t lives_proc_thread_secure_ptr(lives_proc_thread_t lpt, void **ptr) {
  // ptr should be any void * whose address was passed to lives_proc_thread_nullify_on_destruction()
  // if lpt was to be freed, it would have been nullified
  // it is possible for it to be nullified even if not freed
  // if other threads, including this one, add references in a destruction callback
  pthread_mutex_t *destruct_mutex;

  if (!lpt || !ptr || !*ptr) return NULL;

  proc_thread_unrefs_block();

  //lpt = (lives_proc_thread_t)*ptr;
  if (!*ptr) {
    // too late
    proc_thread_unrefs_unblock();
    return NULL;
  }
  destruct_mutex =
    (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);
  if (!pthread_mutex_trylock(destruct_mutex)) {
    // since we locked the kill lock before reading the value of ptr
    // and also got the destuct mutex
    // we can be sure that the destuct hook has not been called
    // thus if we ref lpt, it will prevent it from being freed
    lives_proc_thread_ref(lpt);
    pthread_mutex_unlock(destruct_mutex);
  } else lpt = NULL;
  proc_thread_unrefs_unblock();
  return lpt;
}


LIVES_GLOBAL_INLINE lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, NULL);
  return NULL;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_ref(lives_proc_thread_t lpt) {return weed_refcount_inc(lpt);}


boolean lives_proc_thread_unref(lives_proc_thread_t lpt) {
  if (lpt) {
    int refs = weed_refcount_query(lpt);
    if (!refs) {
      // this hook could be called several times if threads add references then remove them
      // we will clear the queue to prevent this
      pthread_mutex_t *destruct_mutex =
        (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);

      // we will do this - lock global mutex - recheck refs (in case any were added while we waited)
      // - lock lpt destruct mutex (this prevents other threads reading 0 refs in a race)
      // - free

      // other threads will fo - lock global mutex - trylock destruct mutex - read ptr value
      // if NULL, go away, if non null, add a ref

      proc_thread_unrefs_block(); // xxx

      // another thread could have freed lpt while we waited for the lock.
      // but call should have locked global lock, then checked if destructor was called
      // in added a ref. So we can assume that didnt happen

      // check if refs were added wile we waited for lock
      refs = weed_refcount_query(lpt);

      if (!refs) {
        // ok, still good, all other threads are blocked at xxx

        // try to lock the destruct mutex. Then we can unlock the global lock
        // any other threads waiting on it wont be able to get this lock and will unlock global and go away
        if (destruct_mutex && !pthread_mutex_trylock(destruct_mutex)) {
          lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
          // ideally we would hold this through the hook callbacks, but they could block and we
          // dont want to delay things
          proc_thread_unrefs_unblock();

          // thread 2 can get unref lock, but cant get destruct lock, so they will unlock global lock and go away
          // but
          lives_hooks_trigger(lpt, hook_stacks, DESTRUCTION_HOOK);
          lives_hooks_clear(hook_stacks, DESTRUCTION_HOOK);

          // another thread might have got global lock and do a trylock of destruct mutex at some point
          // so we will block here until the trylock falis and they unlock the global lock
          proc_thread_unrefs_block();
          proc_thread_unrefs_unblock();

          // should be in the clear now - any thread wanting to check destruct lock should have held us at global
          // lock either they got the trylock - so we shouldn tbe here

          // else they failed to get the trylock and went away, or added a ref, so we will block until they are done
          // othe threads grabbing the lock at xxx will also fail to get destruct mutex so they should go away

          // check again, sombody may have added a ref
          refs = weed_refcount_query(lpt);
          if (!refs) {
            // still good
            // give threads a chance to read a null value before continuing
            // check refs again, other threads could have added more refs with kill_lock held
            if (weed_refcount_dec(lpt) == -1) {
              // at this point we have kill_lock, the lpt destruct_mutex was not locked and refcount is -1
              // plus we triggered the destuction_hook for the proc_thread
              // that should be more than enough safeguards
              pthread_mutex_t *state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
              thrd_work_t *work = lives_proc_thread_get_work(lpt);
	      lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);

              weed_refcounter_unlock(lpt);
              lives_hooks_clear_all(hook_stacks, N_HOOK_POINTS);
	      for (int i = 0; i < N_HOOK_POINTS; i++) {
		lives_free(hook_stacks[i]->mutex);
		lives_free(hook_stacks[i]);
	      }

              if (work) work->lpt = NULL;
              THREADVAR(tinfo) = NULL;

              weed_plant_free(lpt);
              lives_free(state_mutex);
	      if (destruct_mutex) {
		pthread_mutex_unlock(destruct_mutex);
		lives_free(destruct_mutex);
	      }
              return TRUE;
            }
          }
        } else {
          // there is a small chance we failed to get the trylock because a thread was checking
          // if the destuct hooks had been called
          // so we will wait to get the global lock, which means they should have added a ref
          // - if refs is > 0, we should decrement it
          refs = weed_refcount_query(lpt);
          if (refs > 0) weed_refcount_dec(lpt);
          proc_thread_unrefs_unblock();
        }
      } else proc_thread_unrefs_unblock();
    }
    return FALSE;
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_unref_check(lives_proc_thread_t lpt) {
  if (weed_refcount_query(lpt)) return lives_proc_thread_unref(lpt);
  return FALSE;
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
  /* for the main thread, it also needs to service GUI callbacks like key press responses. In this case it will monitor the task until it finsishes, since it must run this in another background thread, and return to the gtk main loop, in this case it may re add itslef via an idel func so it can return and continue monitoring. While doin so it must still be ready to service requests from other threads, as well as more requests from the monitored thread. As well as this if the fg thread is running a service for aidle func or timer, it cannot return to the gtk main loop, as it needs to wait for the final response (TRUE or FALSE) from the subordinate timer task. Thus it will keep looping without returning, but still it needs to be servicing other threads. In particular one thread may be waitng for antother to complete and if not serviced the second thread can hagn waiting and block the first thread, wwhich can in turn block the main thread. */

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
    lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
    lives_proc_thread_free(lpt);
    lives_hooks_trigger(NULL, THREADVAR(hook_stacks), INTERNAL_HOOK_0);
  } else {
    if (!is_fg_service) {
      if (!(THREADVAR(hook_hints) & HOOK_CB_PRIORITY)
          && get_gov_status() != GOV_RUNNING && FG_THREADVAR(fg_service)) {
        if (THREADVAR(hook_hints) & HOOK_OPT_NO_DEFER) {
          lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
          lives_proc_thread_free(lpt);
          return FALSE;
        }

        if (THREADVAR(hook_hints) & HOOK_CB_BLOCK) lives_proc_thread_ref(lpt);

        if (add_to_fg_deferral_stack(FG_THREADVAR(hook_flag_hints), lpt) != lpt) {
          // could not be added due to matching restrictions
          lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
          if (THREADVAR(hook_hints) & HOOK_CB_BLOCK) lives_proc_thread_unref(lpt);
          lives_proc_thread_free(lpt);
        } else {
          if (THREADVAR(hook_hints) & HOOK_CB_BLOCK) {
            //
            while (!lives_proc_thread_check_finished(lpt)
                   && !lives_proc_thread_get_cancelled(lpt)) {
              if (get_gov_status() == GOV_RUNNING) {
                mainw->clutch = FALSE;
                lives_nanosleep_until_nonzero(mainw->clutch);
              } else lives_nanosleep(1000);
            }
            lives_proc_thread_unref_check(lpt);
          }
        }
      } else {
        // will call fg_run_func() indirectly, so no need to call lives_proc_thread_free
        //lpt = lives_proc_thread_auto_secure(&lpt);
        if (lpt) {
          if (!lives_proc_thread_is_done(lpt))
            fg_service_call(lpt, retval);
          if (!lives_proc_thread_unref(lpt)) {
            if (THREADVAR(hook_hints) & HOOK_CB_BLOCK) {
              lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
              lives_proc_thread_free(lpt);
            }
          }
        }
        // some functions may have been deferred, since we cannot stack multiple fg service calls
        lives_hooks_trigger(NULL, THREADVAR(hook_stacks), INTERNAL_HOOK_0);
      }
    } else {
      // lpt here is a freshly created proc_thread, it will be stored and then
      if (add_to_deferral_stack(THREADVAR(hook_flag_hints), lpt) != lpt) {
        lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
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
    lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(self);
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSED | THRD_STATE_RESUME_REQUESTED);
    lives_hooks_trigger(self, hook_stacks, RESUMING_HOOK);
    if (lives_proc_thread_has_states(self, THRD_STATE_UNQUEUED)) run_proc_thread(self, 0);
  }
}


LIVES_GLOBAL_INLINE void lives_proc_thread_pause(lives_proc_thread_t self) {
  // self function to called for pausable proc_threads when pause is requested
  // for idle / loop funcs, do not call this, the thread will exit unqueed instead
  // and be resubmitted on resume
  if (self) {
    lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(self);
    lives_proc_thread_include_states(self, THRD_STATE_PAUSED);
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);
    if (lives_proc_thread_has_states(self, THRD_OPT_IDLEFUNC)) {
      lives_proc_thread_hook_append(self, SYNC_WAIT_HOOK, 0, get_unpaused, self);
      lives_hooks_trigger(self, hook_stacks, PAUSED_HOOK);
      sync_point("paused");
      lives_proc_thread_resume(self);
    }
  }
}


LIVES_GLOBAL_INLINE void lives_proc_thread_wait_sync(lives_proc_thread_t self) {
  // internal function for lives_proc_threads. Will block until all hooks have returned
  if (self) {
    lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(self);
    lives_proc_thread_include_states(self, THRD_STATE_WAITING);
    lives_hooks_trigger(self, hook_stacks, SYNC_WAIT_HOOK);
  }
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
      uint64_t tstate;

      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | state_bits);
      pthread_mutex_unlock(state_mutex);

      if ((state_bits & THRD_STATE_PREPARING) && !(tstate & THRD_STATE_PREPARING)) {
        lives_hooks_trigger(lpt, hook_stacks, PREPARING_HOOK);
      }

      if ((state_bits & THRD_STATE_RUNNING) && !(tstate & THRD_STATE_RUNNING)) {
        if (tstate & THRD_STATE_PREPARING) lives_hooks_trigger(lpt, hook_stacks, TX_START_HOOK);
        lives_hooks_trigger(lpt, hook_stacks, TX_START_HOOK);
      }

      if ((state_bits & THRD_STATE_FINISHED) && !(tstate & THRD_STATE_FINISHED)) {
        lives_hooks_trigger(lpt, hook_stacks, FINISHED_HOOK);
      }

      if ((state_bits & THRD_STATE_WAITING) && !(tstate & THRD_STATE_WAITING)) {
        lives_hooks_trigger(lpt, hook_stacks, SYNC_WAIT_HOOK);
      }

      if ((state_bits & THRD_STATE_BLOCKED) && !(tstate & THRD_STATE_BLOCKED)) {
        lives_hooks_trigger(lpt, hook_stacks, TX_BLOCKED_HOOK);
      }

      if ((state_bits & THRD_STATE_TIMED_OUT) && !(tstate & THRD_STATE_TIMED_OUT)) {
        lives_hooks_trigger(lpt, hook_stacks, TIMED_OUT_HOOK);
      }

      if ((state_bits & THRD_STATE_ERROR) && !(tstate & THRD_STATE_ERROR)) {
        lives_hooks_trigger(lpt, hook_stacks, ERROR_HOOK);
      }

      if ((state_bits & THRD_STATE_COMPLETED) && !(tstate & THRD_STATE_COMPLETED)) {
        lives_hooks_trigger(lpt, hook_stacks, COMPLETED_HOOK);
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


LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_FINISHED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_completed(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_COMPLETED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_done(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_check_states(lpt, THRD_STATE_CANCELLED | THRD_STATE_DESTROYED
              | THRD_STATE_COMPLETED | THRD_OPT_IDLEFUNC))) return TRUE;
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


LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t lpt, boolean dontcare) {
  if (!lpt || !lives_proc_thread_get_cancellable(lpt)) return FALSE;
  if (dontcare) lives_proc_thread_dontcare(lpt);
  else {
    pthread_mutex_t *destruct_mutex =
      (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);
    if (!pthread_mutex_trylock(destruct_mutex)) {
      if (lives_proc_thread_check_completed(lpt)) {
	// if the proc_thread runner already checked and exited we need to do the free
	pthread_mutex_unlock(destruct_mutex);
	lives_proc_thread_unref(lpt);
	return TRUE;
      }
      lives_proc_thread_include_states(lpt, THRD_STATE_CANCELLED);
      pthread_mutex_unlock(destruct_mutex);
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_pause(lives_proc_thread_t lpt) {
  if (!lpt || !lives_proc_thread_get_pauseable(lpt)) return FALSE;
  if (!lives_proc_thread_get_paused(lpt))
    lives_proc_thread_include_states(lpt, THRD_STATE_PAUSE_REQUESTED);
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

LIVES_GLOBAL_INLINE ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_START_TICKS, NULL) : 0;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancelled(lives_proc_thread_t lpt) {
  return (!lpt || (lives_proc_thread_has_states(lpt, THRD_STATE_CANCELLED)));
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


LIVES_GLOBAL_INLINE void lives_proc_thread_sync_ready(lives_proc_thread_t lpt) {
  // this cannot be handled via states as it affects the underlying lives_thread
  if (!lpt) return;
  else {
    thrd_work_t *work = lives_proc_thread_get_work(lpt);
    if (work) work->sync_ready = TRUE;
  }
}


static int lives_proc_thread_wait_done(lives_proc_thread_t lpt, double timeout) {
  ticks_t slptime = LIVES_QUICK_NAP * 10;
  if (timeout) timeout *= ONE_BILLION;
  while (timeout >= 0. && !lives_proc_thread_is_done(lpt)) {
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


LIVES_LOCAL_INLINE void _lives_proc_thread_join(lives_proc_thread_t lpt, double timeout) {
  // version without a return value will free lpt
  lives_proc_thread_wait_done(lpt, timeout);

  // caller should ref the proc_thread if it wants to avoid this
  lives_proc_thread_free(lpt);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t lpt) {
  // version without a return value will free lpt
  _lives_proc_thread_join(lpt, 0.);
}

#define _join(lpt, stype) lives_proc_thread_wait_done(lpt, 0.);	\
  lives_nanosleep_while_false(weed_plant_has_leaf(lpt, _RV_) == WEED_TRUE); \
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

  pthread_mutex_t *destruct_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);

  // stop proc_thread from being freed as soon as we set the flagbit
  pthread_mutex_lock(destruct_mutex);
  if (lives_proc_thread_check_completed(lpt)) {
    // if the proc_thread runner already checked and exited we need to do the free
    pthread_mutex_unlock(destruct_mutex);
    lives_proc_thread_unref(lpt);
    return TRUE;
  }
  if (lives_proc_thread_get_cancellable(lpt))
    lives_proc_thread_include_states(lpt, THRD_OPT_DONTCARE | THRD_STATE_CANCELLED);
  else
    lives_proc_thread_include_states(lpt, THRD_OPT_DONTCARE);
  pthread_mutex_unlock(destruct_mutex);
  return FALSE;
}


boolean lives_proc_thread_dontcare_nullify(lives_proc_thread_t lpt, void **thing) {
  if (lpt) {
    lives_proc_thread_hook_append(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)thing);
    return lives_proc_thread_dontcare(lpt);
  }
  return TRUE;
}


static void pthread_cleanup_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  // if pthread_cancel was called, the thread immediately jumps here,
  // because will called pthread_cleanup_push
  // after this, the pthread will exit
  if (lives_proc_thread_get_signalled(info))
    lives_hooks_trigger(NULL, THREADVAR(hook_stacks), THREAD_EXIT_HOOK);
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

    if (destruct_mutex) pthread_mutex_lock(destruct_mutex);
    // dontcare thread blocked here

    // thread finished, but it is possible it will be freed
    // the next state / hook will be either DESTRUCTION or COMPLETED
    state = lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);

    if ((state & THRD_OPT_DONTCARE) || (!ret_type && !(state & THRD_OPT_NOTIFY))) {
      // if there is no return type then we should free the proc_thread
      // QUERY REFS - if > 0 leave

      // if doncare thread is blocked it cant have set dontcar, so we should not be here
      if (destruct_mutex) pthread_mutex_unlock(destruct_mutex);
      if (lives_proc_thread_unref(lpt)) {
        return NULL;
      }
      // thread survived being freed, it must have been reffed
    } else {
      if (lives_proc_thread_has_states(lpt, THRD_OPT_IDLEFUNC) && lives_proc_thread_join_boolean(lpt)) {
        if (destruct_mutex) pthread_mutex_unlock(destruct_mutex);
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
      // once a proc_thread reaches this state it is guaranteed not to be freed
      // so by including this state the doncare thread wil know to unref it itself
      state = lives_proc_thread_include_states(lpt, THRD_STATE_COMPLETED);
    }
  } while (0);

  pthread_mutex_unlock(destruct_mutex);
  return NULL;
}


boolean fg_run_func(lives_proc_thread_t lpt, void *rloc) {
  // rloc should be a pointer to a variable of the correct type. After calling this,
  boolean bret = FALSE;
  if (!lives_proc_thread_get_cancelled(lpt)) {
    lives_proc_thread_ref(lpt);

    call_funcsig(lpt);

    if (rloc) {
      if (weed_leaf_seed_type(lpt, _RV_) == WEED_SEED_STRING) *(char **)rloc = weed_get_string_value(lpt, _RV_, NULL);
      else _weed_leaf_get(lpt, _RV_, 0, rloc);
    }
    lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);
    lives_proc_thread_include_states(lpt, THRD_STATE_COMPLETED);
    lives_proc_thread_unref(lpt);
  }
  return bret;
}


/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
static void run_proc_thread(lives_proc_thread_t lpt, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  thrd_work_t *mywork;

  /// tell the thread to clean up after itself [but it won't delete lpt]
  attr |= LIVES_THRDATTR_AUTODELETE;

  attr |= LIVES_THRDATTR_IS_PROC_THREAD;
  lives_proc_thread_include_states(lpt, THRD_STATE_QUEUED);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_UNQUEUED);

  // add the work to the pool
  mywork = lives_thread_create(NULL, attr, proc_thread_worker_func, (void *)lpt);

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
// npoolthreads is the number of available (free) poolthreads, we try to maintain this > ntasks
static volatile int npoolthreads, rnpoolthreads;
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
    tdata->vars.var_hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
    tdata->vars.var_hook_stacks[i]->mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(tdata->vars.var_hook_stacks[i]->mutex, NULL);
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
    lives_hooks_transfer(THREADVAR(hook_stacks), mywork->hook_stacks, FALSE);
  }

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
    lives_hooks_clear(THREADVAR(hook_stacks), type);
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
      pthread_mutex_lock(&tcond_mutex);
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
    lives_hooks_transfer(work->hook_stacks, THREADVAR(hook_stacks), FALSE);
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

  pthread_mutex_lock(&twork_count_mutex);
  ntasks++;
  pthread_mutex_unlock(&twork_count_mutex);
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);

  pthread_mutex_lock(&pool_mutex);
  if (rnpoolthreads) {
    if (npoolthreads < rnpoolthreads) {
      for (int i = 0; i < rnpoolthreads; i++) {
        lives_thread_data_t *tdata = get_thread_data_by_idx(i + 1);
        if (!tdata || tdata->exited) {
          if (tdata && tdata->exited) pthread_join(*poolthrds[i], NULL);
          lives_free(poolthrds[i]);
          poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
          pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i + 1));
          pthread_mutex_lock(&tcond_mutex);
          pthread_cond_signal(&tcond);
          pthread_mutex_unlock(&tcond_mutex);
        }
      }
    }
    if (ntasks <= rnpoolthreads) {
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    } else {
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
      pthread_mutex_lock(&tcond_mutex);
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
    pthread_mutex_lock(&tcond_mutex);
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


LIVES_GLOBAL_INLINE int refcount_query(obj_refcounter *refcount) {
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
