// threading.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

//#include <setjmp.h>

#include "main.h"

/**
   lives_proc_threads API
   - both proc_threads and normal threads are executed by worker thread from the pool, however:

   proc_threads differ from normal lives_threads in several ways:

   - proc_threads can call any function as long as the funcsig / return value has a mapping
   - lives_threads are intended for lightweight function calls which may be split and run in parallel
		(e.g palette conversions)
   - proc_threads are for more heavyweight functions where an etire function is wrapped to be run in the bg
   - proc_threads can call literally any function, provided the "funcsig" is linked to a function call
		(eg. in call_funcsig_iiner in function.c)
   - proc_threads have typed return values, (or no return)
   - proc_threads have a richer set of attributes to modify their behaviour
   - proc_threads can be cancelled, either at the code points, or the underlying pthread level
	a running thread can disable or enable code level cancellation;; pthread level cancellation cannot be blocked
	a cleanup function ensures even in case of pthread level cancellation, the pthread terminates cleanly
   - proc threads have a "state" which when changed can sometimes trigger hook callbacks
   - proc_threads with a timeout can be created. If the task does not finish before the timer expires, the thread will be
	instantly cancelled. The thread can request a temporary stay of execution by setting the BUSY state flag, then clearing
	it later.
   - proc_threads can be optionally be pausable, and then paused resumed
   - proc_threads can spawn child proc_threads; the only limit to this is the thread resource limit
   - "idle" proc_threads can be created - these can be either requeued or resumed after each cycle
   - proc_threads can be create "unqueued", acting like a "frozen" function call and later
	queued for execution
   - proc_threads can be used to assing tasks from bg threads to main thread
   - specifying a return type of 0 causes the proc_thread to automatically be freed when it completes
   - a return type of -1 implies a (void) return
   - calling lives_proc_thread_dontcare() has the effect to of turning any return type to type 0
		(this is protected by a mutex to ensure it is always done atomically)
   - amongst the available attributes are:
	- PRIORITY   - this is also a lives_thread flag: - the result is to add the job at the head of the pool queue
			rather than at the tail end
	- AUTODELETE - tells the underlying thread to free it's resources automatically there is no need to set this for proc threads
			is this done in the create function
	- WAIT_START - the proc_thread will be created, but the caller will block in lives_proc_thread_queue
				until a worker begins processing

	- WAIT_SYNC  - the proc_thread will be created, but the worker will wait for the caller
				 to set sync_ready before running

			NOTE: WAIT_START and WAIT_SYNC may be combined, the effect will be to block the caller until
			 the worker thread reaches the waiting state, and caller must subsequently call sync_ready
	 AUTO_PAUSE

	 AUTO_REQUEUE

	// may deprecate
	IGNORE_SYNCPT - setting this flagbit tells the thread to not wait at sync points, this


	- NO_GUI:    - this has no predefined functional effect,
			but will set a flag in the worker thread's environment (?); this may be
			checked for in the function code and used to bypass graphical updates
			This is intended for functions which are dual purpose and can be run in the foreground
			with interface, or in the background with no interface.

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

static boolean lpt_remove_from_pool(lives_proc_thread_t lpt);
static void lives_proc_thread_set_final_state(lives_proc_thread_t lpt);

static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;


static boolean sync_hooks_done(lives_obj_t *obj, void *data) {
  if (data) {
    boolean *b = (boolean *)data;
    *b = TRUE;
  }
  return FALSE;
}


#if 0
static boolean _thrd_cond_timedwait(lives_proc_thread_t self, int secs) {
  // if thread gets cancel_immediate here, this acts like cond_signal
  // the thread will continue up to the exit point then run the cleanup handler
  // thus it would seem better NOT to call pthread_cond_signal before theread cancellation
  // as this way we can ensure the thread is cancelled before returning to the caller function
  if (self) {
    thrd_work_t *work = lives_proc_thread_get_work(self);
    if (work) {
      static struct timespec ts;
      pthread_mutex_t *pause_mutex = THREADVAR(pause_mutex);
      pthread_cond_t *pcond = THREADVAR(pcond);

      int rc = 0;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += secs;
      pthread_mutex_lock(pause_mutex);
      while (!THREADVAR(sync_ready) && rc != ETIMEDOUT) {
        rc = pthread_cond_timedwait(pcond, pause_mutex, &ts);
      }
      pthread_mutex_unlock(pause_mutex);
      if (rc == ETIMEDOUT && !THREADVAR(sync_ready)) {
        THREADVAR(sync_ready) = TRUE;
        work->flags |= LIVES_THRDFLAG_TIMEOUT;
      }
      return TRUE;
    }
  }
  return FALSE;
}
#endif


static boolean _thrd_cond_wait(lives_proc_thread_t self) {
  // wait here for sync-ready. thread must be running a proc_thread
  // NB.
  // if thread gets cancel_immediate here, this acts like cond_signal
  // the thread will continue up to the exit point then run the cleanup handler
  // thus it would seem better NOT to call pthread_cond_signal before theread cancellation
  // as this way we can ensure the thread is cancelled before returning to the caller function
  if (self) {
    thrd_work_t *work = lives_proc_thread_get_work(self);
    if (work) {
      pthread_mutex_t *pause_mutex = THREADVAR(pause_mutex);
      pthread_cond_t *pcond = THREADVAR(pcond);
      while (!THREADVAR(sync_ready)) {
        work->flags |= LIVES_THRDFLAG_COND_WAITING;
        pthread_cond_wait(pcond, pause_mutex);
        work->flags &= ~LIVES_THRDFLAG_COND_WAITING;
      }
      return TRUE;
    }
  }
  return FALSE;
}

// unpause
static boolean _lives_proc_thread_cond_signal(lives_proc_thread_t lpt) {
  if (lives_proc_thread_ref(lpt) > 1) {
    thrd_work_t *work = lives_proc_thread_get_work(lpt);
    if (work) {
      uint64_t tuid = work->busy;
      if (tuid) {
        lives_thread_data_t *tdata = get_thread_data_by_uid(tuid);
        if (tdata) {
          pthread_mutex_t *pause_mutex = tdata->vars.var_pause_mutex;
          pthread_cond_t *pcond = tdata->vars.var_pcond;
          pthread_mutex_lock(pause_mutex);
          tdata->vars.var_sync_ready = TRUE;
          pthread_cond_signal(pcond);
          pthread_mutex_unlock(pause_mutex);
          // we need to ensure that an associated proc_thread has responded now
          lives_nanosleep_while_true(lives_proc_thread_is_paused(lpt));
          // excluding this permits the proc_thread to continue and either resume or cancel
          lives_proc_thread_exclude_states(lpt, THRD_STATE_RESUME_REQUESTED);
          lives_proc_thread_unref(lpt);
          return TRUE;
        }
      }
    }
    lives_proc_thread_unref(lpt);
  }
  return FALSE;
}


#if 0
static boolean lives_proc_thread_timedwait(lives_proc_thread_t self, int sec) {
  boolean retval = FALSE;
  if (self) {
    thrd_work_t *work = lives_proc_thread_get_work(self);
    if (work && !(work->flags & LIVES_THRDFLAG_IGNORE_SYNCPTS)) {
      pthread_mutex_t *pause_mutex = THREADVAR(pause_mutex);
      THREADVAR(sync_ready) = FALSE;
      pthread_mutex_lock(pause_mutex);
      lives_proc_thread_include_states(self, THRD_STATE_SYNC_WAITING);
      retval = _thrd_cond_timedwait(self, sec);
      lives_proc_thread_exclude_states(self, THRD_STATE_SYNC_WAITING);
      pthread_mutex_unlock(pause_mutex);
    }
  }
  return retval;
}
#endif


static boolean lives_proc_thread_wait(lives_proc_thread_t self) {
  boolean retval = FALSE;
  if (self) {
    pthread_mutex_t *pause_mutex = THREADVAR(pause_mutex);
    THREADVAR(sync_ready) = FALSE;
    pthread_mutex_lock(pause_mutex);
    lives_proc_thread_include_states(self, THRD_STATE_SYNC_WAITING);
    retval = _thrd_cond_wait(self);
    lives_proc_thread_exclude_states(self, THRD_STATE_SYNC_WAITING);
    pthread_mutex_unlock(pause_mutex);
  }
  return retval;
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

// Note: there are 4 entities invloved here: the base thread, which has the hook stack for SYNC_WAIT
// self, which is the proc thread waiting on the task
// lpt, the task (proc_thread) being monitored
// poller, a further proc thread which triggers the callbacks in the SYNC_WAIT stack
//  - if all callbacks return TRUE, then it will call finfunc(findata) before exiting
//     one would normally pass control as findata, and finfunc would then set *findata to TRUE

boolean thread_wait_loop(lives_proc_thread_t lpt, boolean full_sync, volatile boolean *control) {
  GET_PROC_THREAD_SELF(self);
  thrd_work_t *work = NULL;
  ticks_t timeout;
  uint64_t ltimeout;
  lives_proc_thread_t poller = NULL;
  lives_hook_stack_t **hook_stacks;
  uint64_t inc_states = THRD_STATE_WAITING, exc_states = inc_states;
  int64_t msec = 0, blimit;
  boolean retval = TRUE;
  boolean can_set_blocked = FALSE;

  // if control ariable not passed in, we will use a local variable instead
  volatile boolean ws_hooks_done = FALSE;
  if (!control) control = &ws_hooks_done;

  if (full_sync) {
    work = lives_proc_thread_get_work(lpt);
    if (work && (work->flags & LIVES_THRDFLAG_IGNORE_SYNCPTS) == LIVES_THRDFLAG_IGNORE_SYNCPTS)
      return TRUE;
  }

  if (lives_proc_thread_ref(lpt) < 2) lpt = NULL;

  weed_set_voidptr_value(lpt, "_control", (void *)control);

  blimit = THREADVAR(blocked_limit);
  timeout = THREADVAR(sync_timeout);
  ltimeout = labs(timeout);

  if (self) {
    // check which states are not set, we will exclude them again after
    exc_states |= ~lives_proc_thread_get_state(self) & (THRD_STATE_BLOCKED | THRD_STATE_BUSY);
    if (exc_states & THRD_STATE_BUSY) {
      // STATE CHANGE - > adds BUSY, so timeout threads do not count time spent waiting
      inc_states |= THRD_STATE_BUSY;
    }
    lives_proc_thread_include_states(lpt, inc_states);
    if (blimit && (exc_states & THRD_STATE_BLOCKED)) can_set_blocked = TRUE;
  }

  hook_stacks = THREADVAR(hook_stacks);

  // if we have callbacks in (thread statcks) SYNC_WAIT_HOOK,
  // launch a bg thread which will call all SYNC_WAIT_HOOK callbacks in turn repetedly
  // if all return TRUE, then the "poller" will run finfunc(findata)
  // then sync_hooks_done(&ws_hooks_done), in thase case we pass sync_hooks_done and "control"
  // 'control' need to become TRUE before we can return successfully
  // sync_hooks_done simple sets the value and exits
  // control can also be passed to th eother functions, allowing them to ovverride the full chack
  // if control is passed as an argument to this funcion, this allows the loop to be terminated on demoand
  // if the arg. is NULL, then a suitbale control var will be created

  if (hook_stacks[SYNC_WAIT_HOOK]->stack)
    poller = lives_hooks_trigger_async_sequential(hook_stacks, SYNC_WAIT_HOOK,
             (hook_funcptr_t)sync_hooks_done, (void *)control);

  //if (debug) g_print("\n\nCNTRL is %d\n\n\n", *control);

  if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt)) \
    check_pool_threads();						\

  while (!*control) {
    if (self && lives_proc_thread_should_cancel(self)) {
      if (lpt) lives_proc_thread_request_cancel(lpt, TRUE);
      lives_proc_thread_cancel(self);
      break;
    }
    lives_nanosleep(10000);
    msec++;
    if (timeout && msec >= ltimeout) {
      if (timeout < 0) {
        if (self) lives_proc_thread_include_states(self, THRD_STATE_TIMED_OUT);
      }
      retval = FALSE;
      goto finish;
    }
    if (can_set_blocked && msec >= blimit) {
      lives_proc_thread_include_states(self, THRD_STATE_BLOCKED);
      can_set_blocked = FALSE;
    }
  }

finish:
  if (poller) {
    lives_proc_thread_request_cancel(poller, FALSE);
    lives_proc_thread_join(poller);
  }
  if (lpt) {
    weed_leaf_delete(lpt, "_control");
    lives_proc_thread_unref(lpt);
  }
  lives_hooks_clear(hook_stacks, SYNC_WAIT_HOOK);
  if (self && exc_states) lives_proc_thread_exclude_states(self, exc_states);
  return retval;
}


boolean sync_point(const char *motive) {
  // threads can call sync_point("motive"), and will pause until sync_ready is set
  // this is similar to pause, with the difference being this is a spontaneous hook
  // whrereas pause is a response to a request hook
  //
  // this is also similar to thread_wait_loop, however that function is a constional wait
  // sync point is unconditional, the thread will continue when any other thread calls sync_ready(this)
  // the "motive" is purely informational
  GET_PROC_THREAD_SELF(self);
  if (motive) THREADVAR(sync_motive) = lives_strdup(motive);
  lives_proc_thread_wait(self);
  if (motive) {
    lives_free(THREADVAR(sync_motive));
    THREADVAR(sync_motive) = NULL;
  }
  return FALSE;
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


LIVES_GLOBAL_INLINE void lives_proc_thread_set_attrs(lives_proc_thread_t lpt, uint64_t attrs) {
  if (lpt) weed_set_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, (int64_t)attrs);
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_get_attrs(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, NULL) : 0;
}


static lives_proc_thread_t lives_proc_thread_run(lives_thread_attr_t attrs, lives_proc_thread_t lpt,
    uint32_t return_type) {
  if (lpt) {
    lives_proc_thread_set_attrs(lpt, attrs);
    if (!(attrs & LIVES_THRDATTR_START_UNQUEUED)) {
      // add to the pool
      lives_proc_thread_queue(lpt, attrs);
    }
  }
  return lpt;
}


LIVES_GLOBAL_INLINE weed_error_t lives_proc_thread_set_data(lives_proc_thread_t lpt, weed_plant_t *data) {
  if (lpt) return weed_set_plantptr_value(lpt, LIVES_LEAF_LPT_DATA, data);
  return WEED_ERROR_NOSUCH_PLANT;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_get_data(lives_proc_thread_t lpt) {
  if (lpt) {
    weed_plant_t *lpt_data = weed_get_plantptr_value(lpt, LIVES_LEAF_LPT_DATA, NULL);
    if (!lpt_data) {
      lpt_data = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
      lives_proc_thread_set_data(lpt, lpt_data);
    }
    return lpt_data;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_steal_data(lives_proc_thread_t lpt) {
  if (lpt) {
    weed_plant_t *data = lives_proc_thread_get_data(lpt);
    lives_proc_thread_set_data(lpt, NULL);
    return data;
  }
  return NULL;
}


static lives_proc_thread_t add_garnish(lives_proc_thread_t lpt, const char *fname, lives_thread_attr_t *attrs) {
  lives_hook_stack_t **hook_stacks;

  //if (attrs) *attrs |= LIVES_THRDATTR_NOTE_TIMINGS;

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

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL)) {
    pthread_rwlock_t *destruct_rwlock = (pthread_rwlock_t *)lives_malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(destruct_rwlock, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, destruct_rwlock);
  }

  hook_stacks = (lives_hook_stack_t **)lives_calloc(N_HOOK_POINTS, sizeof(lives_hook_stack_t *));

  for (int i = 0; i < N_HOOK_POINTS; i++) {
    hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
    hook_stacks[i]->mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(hook_stacks[i]->mutex, NULL);
  }

  if (*attrs & LIVES_THRDATTR_AUTO_REQUEUE) {
    if (!weed_plant_has_leaf(lpt, LIVES_LEAF_FOLLOWUP));
    weed_set_plantptr_value(lpt, LIVES_LEAF_FOLLOWUP, lpt);
  }

  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, (void *)hook_stacks);
  weed_set_string_value(lpt, LIVES_LEAF_FUNC_NAME, fname);

  lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);

  return lpt;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_nullify_on_destruction(lives_proc_thread_t lpt, void **ptr) {
  if (lpt && ptr) {
    lives_proc_thread_add_hook(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)ptr);
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_auto_nullify(lives_proc_thread_t *plpt) {
  if (plpt && *plpt) {
    return lives_proc_thread_add_hook(*plpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)plpt);
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_remove_nullify(lives_proc_thread_t lpt, void **ptr) {
  lives_proc_thread_remove_hook_by_data(lpt, DESTRUCTION_HOOK, lives_nullify_ptr_cb, (void *)ptr);
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_new(const char *fname, lives_thread_attr_t *attrs) {
  lives_proc_thread_t lpt;
#if USE_RPMALLOC
  // free up some thread memory before allocating more
  if (rpmalloc_is_thread_initialized())
    rpmalloc_thread_collect();
#endif
  lpt = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  if (lpt) add_garnish(lpt, fname, attrs);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_vargs(lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *fname, int return_type,
    const char *args_fmt, va_list xargs) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, &attrs);
  _proc_thread_params_from_vargs(lpt, func, return_type, args_fmt, xargs);
  lpt = lives_proc_thread_run(attrs, lpt, return_type);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_nullvargs(lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *fname, int return_type) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, &attrs);
  _proc_thread_params_from_nullvargs(lpt, func, return_type);
  return lives_proc_thread_run(attrs, lpt, return_type);
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


lives_proc_thread_t lives_proc_thread_create_from_funcinst(lives_thread_attr_t attrs, lives_funcinst_t *finst) {
  // for future use, eg. finst = create_funcinst(fdef, ret_loc, args...)
  /* // proc_thread = -this-(finst) */
  /* if (finst) { */
  /*   lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL); */
  /*   add_garnish(finst, fdef->funcname); */
  /*   if (attrs & LIVES_THRDATTR_NULLIFY_ON_DESTRUCTION) lives_proc_thread_auto_nullify(&lpt); */
  /*   return lives_proc_thread_run(attrs, (lives_proc_thread_t)finst, fdef->return_type); */
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
   then the returned value from this function may be re-utlised by passing it as the parameter
   to run_as_thread(). */
// as of present, there are 5 ways to run a proc_thread:
// - on creation, if the attribute START_UNQUEUED is not present, it will be queued for
// 	poolthread execution
// - if START_UNQUEUED is present, the proc_thread can be run by proc_thread_queue
//		to place it in poolthread queue
//   - or it can be executed directly using lives_proc_thread_execute()
//   - or it can be pushed immediately to main thread via fg_service_call()
//   - or it can be added to a hook_stack with lives_hook_append / prepend,
//         or lives_proc_thread_append_hook()
lives_proc_thread_t _lives_proc_thread_create(lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *fname, int return_type, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  if (args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    lpt = _lives_proc_thread_create_vargs(attrs, func, fname, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, fname, return_type);
  return lpt;
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
    lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *func_name, int return_type,
    const char *args_fmt, va_list xargs) {
  lives_alarm_t alarm_handle;
  lives_proc_thread_t lpt;
  ticks_t xtimeout = 1;
  lives_cancel_t cancel = CANCEL_NONE;
  int xreturn_type = return_type;

  if (xreturn_type == 0) xreturn_type--;

  if (args_fmt) {
    lpt = _lives_proc_thread_create_vargs(attrs, func, func_name, xreturn_type, args_fmt, xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, func_name, xreturn_type);

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


lives_proc_thread_t _lives_proc_thread_create_with_timeout(ticks_t timeout, lives_thread_attr_t attrss, lives_funcptr_t func,
    const char *funcname, int return_type,
    const char *args_fmt, ...) {
  lives_proc_thread_t ret;
  va_list xargs;
  va_start(xargs, args_fmt);
  ret = _lives_proc_thread_create_with_timeout_vargs(timeout, attrss, func, funcname, return_type, args_fmt, xargs);
  va_end(xargs);
  return ret;
}


boolean is_fg_thread(void) {
  if (!mainw || !capable || !capable->gui_thread) return TRUE;
  return pthread_equal(pthread_self(), capable->gui_thread);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_deferral_stack(lives_proc_thread_t lpt, uint64_t hook_hints) {
  GET_PROC_THREAD_SELF(self);
  lives_hook_stack_t **hstacks = lives_proc_thread_get_hook_stacks(self);
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  mainw->all_hstacks =
    lives_list_append_unique(mainw->all_hstacks, hstacks);
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  hook_hints |= HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT;
  return lives_hook_add(hstacks, LIVES_GUI_HOOK, hook_hints, (void *)lpt, 0);
}


LIVES_LOCAL_INLINE lives_proc_thread_t add_to_fg_deferral_stack(lives_proc_thread_t lpt, uint64_t hook_hints) {
  lives_proc_thread_include_states(lpt, THRD_STATE_DEFERRED);
  hook_hints |= HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT;
  return lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, hook_hints, (void *)lpt, 0);
}


LIVES_LOCAL_INLINE lives_proc_thread_t append_all_to_fg_deferral_stack(void) {
  GET_PROC_THREAD_SELF(self);
  lives_hook_stack_t **hstacks = lives_proc_thread_get_hook_stacks(self);
  lives_proc_thread_t lpt, lpt2 = NULL;
  LiVESList *cbnext;
  pthread_mutex_t *hmutex = hstacks[LIVES_GUI_HOOK]->mutex,
                   *fgmutex = mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex;

  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  mainw->all_hstacks =
    lives_list_remove_data(mainw->all_hstacks, hstacks, FALSE);
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);

  pthread_mutex_lock(hmutex);
  pthread_mutex_lock(fgmutex);

  for (LiVESList *cblist = (LiVESList *)hstacks[LIVES_GUI_HOOK]->stack; cblist; cblist = cbnext) {
    lives_closure_t *cl = (lives_closure_t *)cblist->data;
    cbnext = cblist->next;
    // need to set this first, because as soon as unlock fg thread mutex it can be triggered and cl free
    if (cl->flags &  HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstacks[LIVES_GUI_HOOK], cblist);
      continue;
    }
    lpt = cl->proc_thread;

    if ((lpt2 = lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK,
                               cl->flags | HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT,
                               (void *)cl, DTYPE_HAVE_LOCK | DTYPE_CLOSURE)) != lpt)
      lives_closure_free(cl);
  }
  lives_list_free((LiVESList *)hstacks[LIVES_GUI_HOOK]->stack);
  hstacks[LIVES_GUI_HOOK]->stack = NULL;

  pthread_mutex_unlock(fgmutex);
  pthread_mutex_unlock(hmutex);
  return lpt2;
}


LIVES_LOCAL_INLINE lives_proc_thread_t prepend_all_to_fg_deferral_stack(void) {
  GET_PROC_THREAD_SELF(self);
  lives_hook_stack_t **hstacks = lives_proc_thread_get_hook_stacks(self);
  lives_proc_thread_t lpt, lpt2 = NULL;
  LiVESList *cbprev;
  pthread_mutex_t *hmutex, *fgmutex;

  hmutex = hstacks[LIVES_GUI_HOOK]->mutex;
  fgmutex = mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex;

  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  mainw->all_hstacks =
    lives_list_remove_data(mainw->all_hstacks, hstacks, FALSE);
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);

  pthread_mutex_lock(hmutex);
  pthread_mutex_lock(fgmutex);
  for (LiVESList *cblist = lives_list_last((LiVESList *)hstacks[LIVES_GUI_HOOK]->stack);
       cblist; cblist = cbprev) {
    lives_closure_t *cl = (lives_closure_t *)cblist->data;
    cbprev = cblist->prev;
    if (cl->flags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstacks[LIVES_GUI_HOOK], cblist);
      continue;
    }
    lpt = cl->proc_thread;
    if (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) continue;
    if ((lpt2 = lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK,
                               cl->flags | HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT,
                               (void *)cl, DTYPE_HAVE_LOCK | DTYPE_CLOSURE | DTYPE_PREPEND)) != lpt)
      lives_closure_free(cl);
  }
  lives_list_free((LiVESList *)hstacks[LIVES_GUI_HOOK]->stack);
  hstacks[LIVES_GUI_HOOK]->stack = NULL;
  pthread_mutex_unlock(fgmutex);
  pthread_mutex_unlock(hmutex);
  return lpt2;
}


LIVES_GLOBAL_INLINE lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, NULL);
  return NULL;
}


static pthread_mutex_t ref_sync_mutex = PTHREAD_MUTEX_INITIALIZER;

int lives_proc_thread_ref(lives_proc_thread_t lpt) {
  int refs = -1;
  if (lpt) {
    pthread_rwlock_t *destruct_rwlock;
    pthread_mutex_lock(&ref_sync_mutex);

    destruct_rwlock = (pthread_rwlock_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL);
    if (destruct_rwlock && !pthread_rwlock_rdlock(destruct_rwlock)) {
      pthread_mutex_unlock(&ref_sync_mutex);
      // having readlock, now we can unlock the mutex
      //pthread_mutex_unlock(destruct_mutex);
      // with the readlock, this ensures the proc_thread cannot be freed
      // since that requires a writelock
      refs = weed_refcount_inc(lpt);
      pthread_rwlock_unlock(destruct_rwlock);
    } else pthread_mutex_unlock(&ref_sync_mutex);
  }
  return refs;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_count_refs(lives_proc_thread_t lpt) {
  return weed_refcount_query(lpt);
}


// if lpt is NULL, or freed, we return TRUE
boolean lives_proc_thread_unref(lives_proc_thread_t lpt) {
  if (lpt) {
    pthread_mutex_t *destruct_mutex;
    pthread_rwlock_t *destruct_rwlock;

    pthread_mutex_lock(&ref_sync_mutex);
    destruct_rwlock
      = (pthread_rwlock_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL);
    if (destruct_rwlock && !pthread_rwlock_rdlock(destruct_rwlock)) {
      pthread_mutex_unlock(&ref_sync_mutex);
      if (weed_refcount_dec(lpt) != 0) {
        pthread_rwlock_unlock(destruct_rwlock);
        return FALSE;
      }

      pthread_rwlock_unlock(destruct_rwlock);

      pthread_mutex_lock(&ref_sync_mutex);
      destruct_mutex
        = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);
      if (!destruct_mutex || pthread_mutex_trylock(destruct_mutex)) {
        pthread_mutex_unlock(&ref_sync_mutex);
        return TRUE;
      }
      pthread_mutex_unlock(&ref_sync_mutex);

      // should stop any other threads trying to ref / unref
      weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL);
      weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_MUTEX, NULL);

      // now wait to get write lock, this ensures anything that read rwlock before it was set to NULL
      // will return
      if (!pthread_rwlock_wrlock(destruct_rwlock)) {
        pthread_mutex_t *state_mutex;
        lives_hook_stack_t **lpt_hooks;
        // will trigger destruction hook

        lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYING | THRD_STATE_DESTROYED);
        if (THREADVAR(proc_thread) == lpt) THREADVAR(proc_thread) = NULL;

        // make sure the proc_thread is not in the work queue
        if (lives_proc_thread_get_work(lpt)) {
          // try to remove from pool, but we may be too late
          // however we also lock twork_list, and worker threads should give up if DESTROYING is set
          lpt_remove_from_pool(lpt);
        }

        // action any deferred updates
        lives_proc_thread_trigger_hooks(lpt, LIVES_GUI_HOOK);

        if (weed_plant_has_leaf(lpt, LIVES_LEAF_CLOSURE)) break_me("free lpt with closure !");
        state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);

        pthread_mutex_lock(state_mutex);
        flush_cb_list(lpt);
        pthread_mutex_unlock(state_mutex);

        lpt_hooks = lives_proc_thread_get_hook_stacks(lpt);
        lives_hooks_clear_all(lpt_hooks, N_HOOK_POINTS);
        lives_free(lpt_hooks);

        weed_plant_free(lpt);

        pthread_mutex_destroy(state_mutex);
        lives_free(state_mutex);

        // pause briefly so that threads which just read rwlock or mutex
        // dont end up with invalid objects
        pthread_mutex_lock(&ref_sync_mutex);
        pthread_mutex_unlock(&ref_sync_mutex);

        pthread_mutex_unlock(destruct_mutex);
        pthread_mutex_destroy(destruct_mutex);
        lives_free(destruct_mutex);

        // we can now unlock the rwlock
        pthread_rwlock_unlock(destruct_rwlock);
        pthread_rwlock_destroy(destruct_rwlock);
        lives_free(destruct_rwlock);
        return TRUE;
	// *INDENT-OFF*
      }}
    else pthread_mutex_unlock(&ref_sync_mutex);
  }
  // *INDENT-ON*
  return FALSE;
}


// what_to_wait for will do as follows:
// first test if we can add lpt to main thread's idle stack
// if so then we check if there is anything in this thread;s deferral stack
// if so then we append those callbacks to main thread's stack, and then append lpt
// if not we just appen lpt
// in both case lpt is returned
// if lpt would be rejected then lpt2 is the blocking callback
// and we prepend all our deferrals, and unref lpt - if blocking, we wait for lpt2 instead
static lives_proc_thread_t what_to_wait_for(lives_proc_thread_t lpt, uint64_t hints) {
  lives_proc_thread_t lpt2 = add_to_deferral_stack(lpt, hints);
  if (hints & HOOK_CB_BLOCK) {
    if (lpt2 != lpt) lives_proc_thread_unref(lpt);
    lpt = append_all_to_fg_deferral_stack();
    if (lpt != lpt2) lives_proc_thread_unref(lpt2);
    lpt2 = lpt;
  }
  return lpt2;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_get_replacement(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, NULL);
  return NULL;
}


static lives_proc_thread_t lpt_wait_with_repl(lives_proc_thread_t lpt) {
  lives_proc_thread_t lpt2 = lpt;
  while (lpt) {
    if (lpt2 != lpt) {
      // check
      lives_proc_thread_unref(lpt);
      lpt = lpt2;
      if (!lpt) break;
    }
    lpt2 = lives_proc_thread_get_replacement(lpt);
    if (!lpt2) lpt2 = lpt;
    if (lpt2 != lpt) continue;

    // get retval
    if (lives_proc_thread_is_done(lpt)) break;
    if (lives_proc_thread_is_invalid(lpt)) break;

    lives_nanosleep(1000);
    // get retval
    if (lives_proc_thread_is_done(lpt)) break;
    if (lives_proc_thread_is_invalid(lpt)) break;
  }
  return lpt;
}


// TODO - fg_service_call returns of its own accord, and needs to set retval
// - make lpt, add leaf for retloc,  ??, make closure, closure->retloc must be retval from here
static boolean _main_thread_execute_vargs(lives_funcptr_t func, const char *fname, int return_type,
    void *retloc, const char *args_fmt, va_list xargs) {
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
  uint64_t hook_hints = 0, attrs = LIVES_THRDATTR_FG_THREAD  | LIVES_THRDATTR_START_UNQUEUED;
  boolean is_fg_service = FALSE;
  boolean retval = TRUE;
  boolean is_fg = is_fg_thread();

  // create a lives_proc_thread, which will either be run directly (fg thread)
  // passed to the fg thread
  // or queued for sequential execution (since we need to avoid nesting calls)
  if (args_fmt && *args_fmt) {
    lpt = _lives_proc_thread_create_vargs(attrs, func, fname, return_type, args_fmt, xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, fname, return_type);

  // this is necessary, because if caller thread is cancelled it MUST cancel the hook callback
  // or it will block waiting
  lives_proc_thread_set_cancellable(lpt);
  if (retval) weed_set_voidptr_value(lpt, LIVES_LEAF_RETLOC, retloc);

  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  if (is_fg) {
    // run direct
    retval = lives_proc_thread_execute(lpt, retloc);
    lives_proc_thread_unref(lpt);
    goto mte_done2;
  } else {
    uint64_t attrs;
    if (THREADVAR(perm_hook_hints))
      hook_hints = THREADVAR(perm_hook_hints);
    else {
      hook_hints = THREADVAR(hook_hints);
    }
    THREADVAR(hook_hints) = 0;

    attrs = lives_proc_thread_get_attrs(lpt);

    if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
      weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());

    if (hook_hints & HOOK_OPT_FG_LIGHT) {
      // call this just so it can set the correct STATE
      lives_proc_thread_queue(lpt, attrs | LIVES_THRDATTR_FG_LIGHT | LIVES_THRDATTR_NO_UNREF);

      // must unref, as it was never really in a hook_stack
      lives_proc_thread_unref(lpt);
      goto mte_done;
    }

    if (retloc && !(hook_hints & HOOK_CB_BLOCK) && isstck(retloc)) {
      break_me("mte retloc is on thread stack");
    }

    if (!is_fg_service || (hook_hints & HOOK_CB_BLOCK) || (hook_hints & HOOK_CB_PRIORITY)) {
      // first level service call
      if (!(hook_hints & HOOK_CB_PRIORITY) && FG_THREADVAR(fg_service)) {
        // call is not priority,  and main thread is already performing
        // a service call - in this case we will add the request to main thread's deferral stack
        if (hook_hints & HOOK_CB_BLOCK) lives_proc_thread_ref(lpt);

        lpt2 = what_to_wait_for(lpt, hook_hints);

        if (lpt2 != lpt) {
          lives_proc_thread_unref(lpt);
        }
        if (!(hook_hints & HOOK_CB_BLOCK)) {
          retval = FALSE;
          goto mte_done;
        }
        if (lpt2 != lpt) {
          lives_proc_thread_unref(lpt);
          lpt = lpt2;
        }
        lpt = lpt_wait_with_repl(lpt);
        if (lpt && !lives_proc_thread_is_invalid(lpt))
          lives_proc_thread_unref(lpt);
        goto mte_done;
      }
      // high priority, or gov loop is running or main thread is running a service call
      // we need to wait and action this rather than add it to the queue
      //
      // if priority and not blocking, we will append lpt to the thread stack
      // then prepend our stack to maim thread
      // must not unref lpt, as it is in a hook_stack, it well be unreffed when the closure is freed
      add_to_deferral_stack(lpt, hook_hints);

      if (!(hook_hints & HOOK_CB_BLOCK)) {
        if (hook_hints & HOOK_CB_PRIORITY) {
          prepend_all_to_fg_deferral_stack();
          fg_service_wake();
        }
      } else {
        if (hook_hints & HOOK_CB_PRIORITY) {
          GET_PROC_THREAD_SELF(self);
          lives_proc_thread_trigger_hooks(self, LIVES_GUI_HOOK);
        } else {
          append_all_to_fg_deferral_stack();
          lpt = lpt_wait_with_repl(lpt);
        }
      }
      goto mte_done;
    } else {
      // we are already running in a service call, we will add any calls to our own deferral stack to
      // be actioned when we return, since we cannot nest service calls
      // lpt here is a freshly created proc_thread, it will be stored and then
      // these will be triggered after we return from waiting for the current service call
      // - must not unref lpt here
      add_to_deferral_stack(lpt, hook_hints);
    }
    goto mte_done;
  }

mte_done:
  THREADVAR(hook_hints) = hook_hints;
mte_done2:
  if (!is_fg_service) THREADVAR(fg_service) = FALSE;
  return retval;
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
  // test if ALL states in state_bits are set
  return (lpt && lives_proc_thread_check_states(lpt, state_bits) == state_bits);
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate, new_state;

      // if new state includes DESTROYED, refccount will be zero
      // so we do not want to ref it and unref it, else this would cause recursion when refcount
      // goes back to zero
      boolean do_refs = lives_proc_thread_count_refs(lpt) > 1;

      if (do_refs) lives_proc_thread_ref(lpt);

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
          if (!(new_state & THRD_STATE_IDLING)) {
            lives_hooks_trigger(hook_stacks, PAUSED_HOOK);
          }
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
  return THRD_STATE_INVALID;
}


uint64_t lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lives_proc_thread_ref(lpt) > 1) {
    uint64_t new_state = THRD_STATE_INVALID;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
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
  return THRD_STATE_INVALID;
}

// the following functions should NEVER be called if there is a possibility that the proc_thread
// may be destroyed (state includes DONTACARE, return type is 0, or it is a hook callback with options
// ONESHOT or stack_type REMOVE_IF_FALSE)
//
// however if the caller holds a reference on the proc_thread, then these are safe to call

// check if thread is idling (only set if idlefunc was queued at least once already)
LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_queued(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_QUEUED))) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_unqueued(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_UNQUEUED))) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_paused(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_PAUSED))) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_preparing(lives_proc_thread_t lpt) {
  if (lpt) {
    if (lives_proc_thread_has_states(lpt, THRD_STATE_PREPARING)) return TRUE;
    if (lives_proc_thread_is_queued(lpt)) check_pool_threads();
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_stacked(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_STACKED))) return TRUE;
  return FALSE;
}


// check if thread finished normally
LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads();
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_FINISHED)));
}



// check if thread is idling (only set if idlefunc was queued at least once already)
LIVES_GLOBAL_INLINE boolean lives_proc_thread_paused_idling(lives_proc_thread_t lpt) {
  return lives_proc_thread_has_states(lpt, THRD_STATE_IDLING | THRD_STATE_PAUSED);
}


// check if thread finished normally, is idling or will be destroyed
LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_done(lives_proc_thread_t lpt) {
  if (lpt) {
    return (lives_proc_thread_check_finished(lpt) || lives_proc_thread_paused_idling(lpt)
            || lives_proc_thread_will_destroy(lpt));
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_will_destroy(lives_proc_thread_t lpt) {
  return lives_proc_thread_has_states(lpt, THRD_STATE_DESTROYING);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_should_cancel(lives_proc_thread_t lpt) {
  return lpt && ((lives_proc_thread_get_state(lpt) &
                  (THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_invalid(lives_proc_thread_t lpt) {
  return lives_proc_thread_has_states(lpt, THRD_STATE_INVALID);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_sync_waiting(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads();
  if (lpt && lives_proc_thread_has_states(lpt, THRD_STATE_SYNC_WAITING)) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_waiting(lives_proc_thread_t lpt) {
  // self / conditional wait
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads();
  if (lpt && lives_proc_thread_has_states(lpt, THRD_STATE_WAITING)) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_had_error(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_ERROR)));
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_errnum(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_ERRNUM, NULL) : 0;
}


LIVES_GLOBAL_INLINE char *lives_proc_thread_get_errmsg(lives_proc_thread_t lpt) {
  return lpt ? weed_get_string_value(lpt, LIVES_LEAF_ERRMSG, NULL) : NULL;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_SIGNALLED)));
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_signal_data(lives_proc_thread_t lpt, uint64_t *tuid, void **data) {
  lives_thread_data_t *tdata
    = (lives_thread_data_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_SIGNAL_DATA, NULL);
  if (data) *data = tdata;
  if (tdata) {
    if (tuid) *tuid = tdata->uid;
    return tdata->signum;
  }
  return 0;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_cancellable(lives_proc_thread_t lpt) {
  if (lpt) lives_proc_thread_include_states(lpt, THRD_OPT_CANCELABLE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_OPT_CANCELABLE)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_was_cancelled(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads();
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


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_cancel(lives_proc_thread_t lpt, boolean dontcare) {
  if (!lpt || (!lives_proc_thread_get_cancellable(lpt))) return FALSE;

  if (lives_proc_thread_is_unqueued(lpt)) {
    lives_proc_thread_cancel(lpt);
    lives_proc_thread_set_final_state(lpt);
    return TRUE;
  }

  if (lives_proc_thread_is_done(lpt)) return FALSE;

  if (dontcare) if (!lives_proc_thread_dontcare(lpt)) return FALSE;

  lives_proc_thread_include_states(lpt, THRD_STATE_CANCEL_REQUESTED);

  if (lives_proc_thread_sync_waiting(lpt)) {
    if (lives_proc_thread_is_preparing(lpt)) lives_proc_thread_sync_ready(lpt);
    else lives_proc_thread_sync_continue(lpt);
  }

  if (lives_proc_thread_is_paused(lpt)) lives_proc_thread_request_resume(lpt);

  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancel_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_CANCEL_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t self) {
  if (self) {
    lives_proc_thread_include_states(self, THRD_STATE_CANCELLED);
    lives_proc_thread_exclude_states(self, THRD_STATE_CANCEL_REQUESTED);
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_error(lives_proc_thread_t self, int errnum, const char *fmt, ...) {
  if (self) {
    weed_set_int_value(self, LIVES_LEAF_ERRNUM, errnum);
    if (fmt && *fmt) {
      char *errmsg;
      va_list vargs;
      va_start(vargs, fmt);
      errmsg = lives_strdup_vprintf(fmt, vargs);
      va_end(vargs);
      weed_set_string_value(self, LIVES_LEAF_ERRMSG, errmsg);
      lives_free(errmsg);
    }
    lives_proc_thread_include_states(self, THRD_STATE_ERROR);
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_resume(lives_proc_thread_t lpt) {
  // to ensure proper synchronisation, - set resume_requested, and wake thread
  // and wait for paused state to go away.
  // - target thread wakes, removes paused state, but waits for resume_req. to be cleared
  // once paused is cleared, remove resume_req, allowing target to unblock
  if (!lpt || !lives_proc_thread_is_paused(lpt)) return FALSE;
  lives_proc_thread_include_states(lpt, THRD_STATE_RESUME_REQUESTED);
  _lives_proc_thread_cond_signal(lpt);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_resume_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_RESUME_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_resume(lives_proc_thread_t self) {
  if (self) {
    // need to remove idling and unqueued in case this is an idle proc thread
    // which is paused / idling
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSED | THRD_STATE_UNQUEUED |
                                     THRD_STATE_IDLING);
    lives_nanosleep_while_true(lives_proc_thread_get_state(self) & THRD_STATE_RESUME_REQUESTED);
    if (lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_cancel(self);
      return FALSE;
    }
  }
  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_pause(lives_proc_thread_t lpt) {
  if (!lpt || !lives_proc_thread_get_pauseable(lpt)) return FALSE;
  if (!lives_proc_thread_is_paused(lpt))
    lives_proc_thread_include_states(lpt, THRD_STATE_PAUSE_REQUESTED);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_pause_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_PAUSE_REQUESTED)));
}


LIVES_GLOBAL_INLINE void lives_proc_thread_pause(lives_proc_thread_t self) {
  // self function to called for pausable proc_threads when pause is requested
  // for idle / pause proc_threads, this will  be called on completion, unless cancelled
  //
  if (self) {
    if (!lives_proc_thread_get_pauseable(self)
        && !(lives_proc_thread_paused_idling(self))) return;
    if (lives_proc_thread_should_cancel(self)) {
      if (!lives_proc_thread_was_cancelled(self))
        lives_proc_thread_cancel(self);
      return;
    } else {
      pthread_mutex_t *pause_mutex = THREADVAR(pause_mutex);
      THREADVAR(sync_ready) = FALSE;
      pthread_mutex_lock(pause_mutex);
      lives_proc_thread_include_states(self, THRD_STATE_PAUSED);
      lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);
      _thrd_cond_wait(self);
      pthread_mutex_unlock(pause_mutex);
      lives_proc_thread_resume(self);
    }
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
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads();
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_START_TICKS, NULL) : 0;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_running(lives_proc_thread_t lpt) {
  if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt))
    check_pool_threads();
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


LIVES_GLOBAL_INLINE boolean lives_proc_thread_sync_ready(lives_proc_thread_t lpt) {
  // by preference, use this function to unlbock proc_threads with wait_sync
  // returns FALSE if lpt was cancelled / cancel_requested
  if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt))
    check_pool_threads();
  lives_nanosleep_while_false(lives_proc_thread_is_preparing(lpt)
                              || lives_proc_thread_should_cancel(lpt));
  //if (lives_proc_thread_should_cancel(lpt)) return FALSE;
  _lives_proc_thread_cond_signal(lpt);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_sync_continue(lives_proc_thread_t lpt) {
  // by preference, use this function to unblock proc_threads at sync_points
  // but to be sure, it will also first unblock those with wait_sync, if not sync_readied
  lives_nanosleep_while_false(lives_proc_thread_sync_waiting(lpt)
                              || lives_proc_thread_should_cancel(lpt));

  if (lives_proc_thread_has_states(lpt, THRD_STATE_PREPARING | THRD_STATE_SYNC_WAITING))
    lives_proc_thread_sync_ready(lpt);

  lives_nanosleep_while_false(lives_proc_thread_has_states(lpt, THRD_STATE_RUNNING | THRD_STATE_SYNC_WAITING)
                              || lives_proc_thread_should_cancel(lpt));

  _lives_proc_thread_cond_signal(lpt);
  return TRUE;
}


LIVES_GLOBAL_INLINE const char *lives_proc_thread_get_wait_motive(lives_proc_thread_t lpt) {
  thrd_work_t *work = lives_proc_thread_get_work(lpt);
  if (work) {
    uint64_t tuid = work->busy;
    if (tuid) {
      lives_thread_data_t *tdata = get_thread_data_by_uid(tuid);
      if (tdata) {
        return tdata->vars.var_sync_motive;
      }
    }
  }
  return NULL;
}


boolean lives_proc_threads_sync_at(lives_proc_thread_t lpt, const char *motive) {
  // this cannot be handled via states as it affects the underlying lives_thread
  boolean it_is;
  if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt))
    check_pool_threads();
  do {
    lives_nanosleep_while_false(lives_proc_thread_sync_waiting(lpt)
                                || lives_proc_thread_is_done(lpt));
    if (lives_proc_thread_is_done(lpt)) return FALSE;
    it_is = !lives_strcmp(lives_proc_thread_get_wait_motive(lpt), motive);
    _lives_proc_thread_cond_signal(lpt);
  } while (!it_is);
  return TRUE;
}


// this function is safe to call even in case
// timeout is in seconds
int lives_proc_thread_wait_done(lives_proc_thread_t lpt, double timeout) {
  if (lives_proc_thread_ref(lpt) > 1) {
    ticks_t slptime = LIVES_QUICK_NAP * 10;
    if (timeout) timeout *= ONE_BILLION;
    if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt))
      check_pool_threads();
    while (timeout >= 0. && !lives_proc_thread_is_done(lpt)
           && !lives_proc_thread_is_invalid(lpt)) {
      // fg thread needs to service requests, otherwise the thread we are waiting on
      // may block waiting for a fg_service_call
      if (is_fg_thread()) fg_service_fulfill();
      // wait 10 usec
      lives_nanosleep(slptime);
      if (timeout) {
        do {
          timeout -= slptime;
        } while (!timeout);
      }
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
  if (lives_proc_thread_ref(lpt) > 1) {
    lives_proc_thread_wait_done(lpt, timeout);

    // caller should ref the proc_thread if it wants to avoid this
    if (!lives_proc_thread_is_stacked(lpt))
      lives_proc_thread_unref(lpt);
    lives_proc_thread_unref(lpt);
  }
}


LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t lpt) {
  // version without a return value will free lpt
  _lives_proc_thread_join(lpt, 0.);
}

#define _join(lpt, stype) lives_proc_thread_wait_done(lpt, 0.);		\
  if (!lives_proc_thread_is_invalid(lpt) && !lives_proc_thread_was_cancelled(lpt) \
      && !(lives_proc_thread_is_unqueued(lpt)				\
	   && lives_proc_thread_should_cancel(lpt))) {			\
  if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt)) \
    check_pool_threads();						\
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
    /* if (!lives_proc_thread_will_destroy(lpt)) */
    /*   lives_proc_thread_unref(lpt); */
    lives_proc_thread_unref(lpt);
    return FALSE;
  }
  lives_proc_thread_include_states(lpt, THRD_OPT_DONTCARE);
  lives_proc_thread_unref(lpt);
  return TRUE;
}


boolean lives_proc_thread_dontcare_nullify(lives_proc_thread_t lpt, void **thing) {
  if (lpt) {
    lives_proc_thread_add_hook(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)thing);
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
    lives_hooks_trigger(NULL, THREAD_EXIT_HOOK);
}


// FINAL for a proc_thread will either be:
//
// - DESTROYED - the proc_thread was flagged DONTCARE, or has no monitored return
//	the state will first go to COMPLETED, then DESTROYED
//	(if the thread was reffed, then the state will stay at COMPLETED unreffed)
// - IDLING - proc_thread was flagged as idlefunc, and the function returned TRUE
// - IDLING / PAUSED - proc_thread was flagged as idlefunc/pauseable, and the function returned TRUE
// - FINISHED - in all other cases (will pass through COMPLETED first)
//
// thus adding a hook callback for COMPLETED will work in all cases, except for idlefuncs
// where we can add a hook for IDLING
// after requeueing the idlefunc, the idling state will be removed
//
// state may be combined with: - unqueued (for idling), cancelled, error, timed_out, etc.
// paused is not a final state unless acompanied by idling,
// the proc_thread should be cancel_requested first, then resume_requested
// for cancel_immediate, there will be no final state, but thread_exit will be triggered
static void lives_proc_thread_set_final_state(lives_proc_thread_t lpt) {
  uint64_t attrs = lives_proc_thread_get_attrs(lpt);
  uint64_t state;
  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);

  // make sure we dont set this twice
  if (lives_proc_thread_is_done(lpt)) return;

  weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, NULL);

  state = lives_proc_thread_get_state(lpt);

  // for idlefunc proc_threads - these will always finish in state unqueued / idling
  // unless idle_pause was also set (then they will end up as paused / idling, unless cancelled)
  //  if cancelled then idle pause threads will not pause but will act like normal idlefuncs
  //  (and become unqueud . idling - cancelled
  // requeueing will remove any unqueued - idling -cancelled states and set queued
  // if dontcare was set then idlefunc proc_threads will act like normal proc_threads and will be destroyed
  if ((state & THRD_OPT_AUTO_PAUSE) && !lives_proc_thread_should_cancel(lpt)) {
    lives_proc_thread_include_states(lpt, THRD_STATE_IDLING);
    return;
  }

  if (!lives_proc_thread_is_stacked(lpt)
      && ((state & THRD_OPT_DONTCARE) || (!ret_type && !(state & THRD_OPT_NOTIFY)))) {
    // if dontcare, or there is no return type then we should unref the proc_thread
    // unless it is a hook callback, (then it stays in the stack - the equivalent would be
    // to add it as ONESHOT, to return FALSE from a REMOVE_IF_FALSE stack, or to remove manually)

    // call the COMPLETED hook, but with DESTROYING set
    lives_proc_thread_include_states(lpt, THRD_STATE_WILL_DESTROY);

    // TODO - check this, now we have is_hook_cb
    if (!(attrs & LIVES_THRDATTR_NO_UNREF)) {
      // if called via a hook trigger or fg_service_call, we set attr to avoid freeing
      lives_proc_thread_unref(lpt);
    }
  } else {
    // once a proc_thread reaches this state
    // provided DESTROYING is not set, it is guaranteed not to be unreffed automatically
    // this is the final  hook trigger, the following state is FINISHED

    // STATE CHANGE -> completed
    lives_proc_thread_include_states(lpt, THRD_STATE_COMPLETED);
  }
}


// could separate - auto_requue -, maybe nor needed if we have a nxtlpt
// auto_pause = pause on completion
// - reequeue - do nxtlpt
// want - q next -> ok
// ret to start or finish
//
// if nxtlpt - queue it (def)
// or run it (immed)

// else - reloop, (auto_requeue)
// pause (auto_pause), return (def)
//
// auto_retart - if nxtlpt is null, go back to olpt
// nxt_immediate - run nxtlpt immediately, bo queue
//
// pause

static void *proc_thread_worker_func(void *args) {
  // this is the function which worker threads run when executing a llives_proc_thread which
  // has been queued
  lives_proc_thread_t olpt = (lives_proc_thread_t)args, lpt = olpt, nxtlpt = NULL;
  if (lpt) {
    weed_plant_t *data;
    uint64_t state;

    lives_proc_thread_ref(lpt);

    // this will stay fixed to the prime lpt
    THREADVAR(proc_thread) = lpt;

    while (1) {
      weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, (void *)pthread_self());

      // if pthread_cancel is called, the cleanup_func will be called and pthread will exit
      // this makes sure we trigger the THREAD_EXIT hook callbacks
      pthread_cleanup_push(pthread_cleanup_func, args);
      call_funcsig(lpt);
      pthread_cleanup_pop(0);

      weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_SELF, 0);

      data = lives_proc_thread_steal_data(lpt);
      if (data && _weed_plant_free(data) != WEED_FLAG_UNDELETABLE) data = NULL;

      state = lives_proc_thread_get_state(lpt);

      if ((state & (THRD_STATE_ERROR | THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0) {
        if (lpt != olpt) {
          if (state & THRD_STATE_ERROR) {
            char *errmsg = weed_get_string_value(lpt, LIVES_LEAF_ERRMSG, 0);
            lives_proc_thread_error(olpt, lives_proc_thread_get_errnum(lpt),
                                    "%s", errmsg);
            lives_free(errmsg);
          }
          if ((state & (THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0) {
            lives_proc_thread_cancel(olpt);
          }
        }
        break;
      }

      nxtlpt = NULL;

      if ((state & (THRD_OPT_AUTO_REQUEUE | THRD_OPT_AUTO_PAUSE)) != 0) {
        nxtlpt = weed_get_plantptr_value(lpt, LIVES_LEAF_FOLLOWUP, NULL);
        if (lpt == olpt && !nxtlpt) nxtlpt = lpt;
        if (lives_proc_thread_ref(nxtlpt) < 2) nxtlpt = NULL;
      }

      if (nxtlpt == lpt) {
        lives_proc_thread_unref(nxtlpt);
        lives_proc_thread_include_states(lpt, THRD_STATE_IDLING);
        if (!lives_proc_thread_has_states(lpt, THRD_OPT_AUTO_PAUSE)) break;
      }

      state = lives_proc_thread_get_state(lpt);

      if (!(state & THRD_OPT_AUTO_PAUSE)) break;

      if (!(state & THRD_OPT_AUTO_REQUEUE)) {
        // function will wait for resume_request, and then run lives_proc_thread_resume()
        lives_proc_thread_pause(lpt);
      }
      // another thread must call lives_proc_thread_request_resume()
      // but we can also be cancelled while paused
      if ((state & (THRD_STATE_ERROR | THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0) break;
      if (nxtlpt) {
        if (lpt != olpt) lives_proc_thread_unref(lpt);
        lpt = nxtlpt;
      }
      g_print("run nxt %p %p\n", olpt, lpt);
    }

    state = lives_proc_thread_get_state(lpt);

    if ((state & (THRD_STATE_ERROR | THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0) {
      nxtlpt = NULL;
      lives_proc_thread_include_states(lpt, THRD_STATE_CANCELLED);
      if (lpt != olpt) {
        lives_proc_thread_include_states(olpt, THRD_STATE_CANCELLED);
      }
    }

    lives_proc_thread_set_final_state(olpt);

    if (nxtlpt) {
      lives_proc_thread_set_data(nxtlpt, data);
      lives_proc_thread_queue(nxtlpt, 0);
      if (nxtlpt != olpt) lives_proc_thread_unref(nxtlpt);
    } else lives_proc_thread_set_data(olpt, data);

    lives_proc_thread_unref(olpt);
  }
  return olpt;
}


boolean lives_proc_thread_execute(lives_proc_thread_t lpt, void *rloc) {
  boolean bret = TRUE;
  if (lives_proc_thread_ref(lpt) < 2) return FALSE;

  if (lives_proc_thread_is_done(lpt)) {
    lives_proc_thread_unref(lpt);
    return TRUE;
  }
  if (lives_proc_thread_is_running(lpt)) {
    lives_proc_thread_unref(lpt);
    return FALSE;
  }

  if (!lives_proc_thread_should_cancel(lpt)) {
    if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_PREPARING);
    ////
    call_funcsig(lpt);
    ////
    if (rloc) {
      if (weed_leaf_seed_type(lpt, _RV_) == WEED_SEED_STRING)
        *(char **)rloc = weed_get_string_value(lpt, _RV_, NULL);
      else _weed_leaf_get(lpt, _RV_, 0, rloc);
    }
  } else {
    if (!lives_proc_thread_was_cancelled(lpt))
      lives_proc_thread_cancel(lpt);
    bret = FALSE;
  }

  lives_proc_thread_set_final_state(lpt);

  lives_proc_thread_unref(lpt);

  return bret;
}


/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once,
// if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
// THRD_ATTR_WAIT_START can be provided at this point if not already specified
// returns TRUE if queueing was succesful, FALSE if the lpt was cancelled before being queued
// NB: some attrs alter the proc_thread attrs, some change lpt state
// others amend the work flags (for the worker thread)
// additionally, some attrs only take effect when the proc_thread is created
boolean lives_proc_thread_queue(lives_proc_thread_t lpt, lives_thread_attr_t attrs) {
  uint64_t lpt_attrs = lives_proc_thread_get_attrs(lpt);
  thrd_work_t *mywork;
  uint64_t state = lives_proc_thread_get_state(lpt);

  lpt_attrs |= attrs;

  /// tell the thread to clean up after itself [but it won't delete lpt]

  attrs |= LIVES_THRDATTR_AUTODELETE;
  attrs |= LIVES_THRDATTR_IS_PROC_THREAD;

  // STATE CHANGE -> unqueued / idling -> queued
  state &= ~(THRD_STATE_IDLING | THRD_STATE_COMPLETED | THRD_STATE_FINISHED);

  state &= ~THRD_STATE_UNQUEUED;
  state |= THRD_STATE_QUEUED;

  if (attrs & LIVES_THRDATTR_AUTO_PAUSE)
    state |= THRD_OPT_AUTO_PAUSE;

  lives_proc_thread_set_state(lpt, state);

  // this ought to be set fo FG_LISGHT to prevent it from being unreffed when complete
  if (attrs & (LIVES_THRDATTR_FG_THREAD))
    lpt_attrs |= LIVES_THRDATTR_NO_UNREF;

  lives_proc_thread_set_attrs(lpt, lpt_attrs);

  if (attrs & LIVES_THRDATTR_FG_THREAD) {
    // special option for the GUI_HOOK:
    if (attrs & LIVES_THRDATTR_FG_LIGHT) {
      fg_service_call(lpt, weed_get_voidptr_value(lpt, LIVES_LEAF_RETLOC, NULL));
      return TRUE;
    }
  }

  // add the work to the pool
  mywork = lives_thread_create(NULL, attrs, proc_thread_worker_func, (void *)lpt);

  if (!mywork) {
    // proc_thread was cancelled before being added to the pool
    return FALSE;
  }

  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);

  if (attrs & LIVES_THRDATTR_WAIT_START) {
    // WAIT_START: caller waits for thread to run or finish
    lives_nanosleep_until_zero(mywork->flags & LIVES_THRDFLAG_WAIT_START);
  }
  return TRUE;
}


char *lives_proc_thread_state_desc(uint64_t state) {
  char *fstr = lives_strdup("");
  if (state & THRD_STATE_UNQUEUED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is not queued");
  if (state & THRD_STATE_IDLING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is idling");
  if (state & THRD_STATE_QUEUED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is queued");
  if (state & THRD_STATE_DEFERRED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "was deferred");
  if (state & THRD_STATE_PREPARING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is preparing");
  if (state & THRD_STATE_RUNNING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is running");
  if (state & THRD_STATE_COMPLETED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "has completed");
  if (state & THRD_STATE_DESTROYING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will be destroyed");
  if (state & THRD_STATE_FINISHED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "has finished");
  if (state & THRD_STATE_DESTROYED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "was destroyed");
  if (state & THRD_STATE_STACKED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "in a hook stack");
  if (state & THRD_STATE_BUSY)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is busy");
  if (state & THRD_STATE_WAITING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is waiting on conditions");
  if (state & THRD_STATE_SYNC_WAITING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is waiting for synx");
  if (state & THRD_STATE_BLOCKED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is blocked");
  if (state & THRD_STATE_PAUSE_REQUESTED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "pause was requested");
  if (state & THRD_STATE_PAUSED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is paused");
  if (state & THRD_STATE_RESUME_REQUESTED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "resume was requested");
  if (state & THRD_STATE_CANCEL_REQUESTED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "cancel was requested");
  if (state & THRD_STATE_CANCELLED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "was cancelled");
  if (state & THRD_STATE_TIMED_OUT)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "has timed out");
  if (state & THRD_STATE_ERROR)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "error encountered");
  if (state & THRD_STATE_SIGNALLED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "was signalled");
  if (state & THRD_STATE_INVALID)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is INVALID");
  if (state & THRD_OPT_CANCELABLE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is cancelable");
  if (state & THRD_OPT_PAUSEABLE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is pauseable");
  if (state & THRD_OPT_NOTIFY)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will wait when finish");
  if (state & THRD_OPT_DONTCARE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will exit when finished");
  if (state & THRD_OPT_AUTO_PAUSE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will pause on completion");
  if (state & THRD_OPT_AUTO_REQUEUE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will be requeued automatically");
  if (state & THRD_BLOCK_HOOKS)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "hooks blocked");

  return fstr;
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

static int next_extern_tidx = 0; // decrements

static lives_threadvars_t dummy_vars;

lives_thread_data_t *get_thread_data(void) {
  lives_thread_data_t *tdata = pthread_getspecific(tdata_key);
  if (!tdata) tdata = lives_thread_data_create(LIVES_INT_TO_POINTER(--next_extern_tidx));
  return tdata;
}

lives_thread_data_t *get_global_thread_data(void) {return global_tdata;}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars(void) {
  lives_thread_data_t *thrdat = get_thread_data();
  return thrdat ? &thrdat->vars : NULL;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars_bg_only(void) {
  if (is_fg_thread()) return &dummy_vars;
  else {
    lives_thread_data_t *thrdat = get_thread_data();
    return thrdat ? &thrdat->vars : NULL;
  }
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_global_threadvars(void) {
  lives_thread_data_t *thrdat = get_global_thread_data();
  return thrdat ? &thrdat->vars : NULL;
}


lives_thread_data_t *get_thread_data_by_slot_id(int idx) {
  LiVESList *list = allctxs;
  pthread_rwlock_rdlock(&allctx_rwlock);
  for (; list; list = list->next) {
    if (!list->data) continue;
    if (((lives_thread_data_t *)list->data)->slot_id == idx) {
      pthread_rwlock_unlock(&allctx_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return NULL;
}


lives_thread_data_t *get_thread_data_by_uid(uint64_t uid) {
  LiVESList *list = allctxs;
  pthread_rwlock_rdlock(&allctx_rwlock);
  for (; list; list = list->next) {
    if (!list->data) continue;
    if (((lives_thread_data_t *)list->data)->uid == uid) {
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
  lives_hook_stack_t **hook_stacks = tdata->vars.var_hook_stacks;

  pthread_rwlock_wrlock(&allctx_rwlock);
  allctxs = lives_list_remove_data(allctxs, tdata, FALSE);
  pthread_rwlock_unlock(&allctx_rwlock);

  if (hook_stacks) {
    for (int i = 0; i < N_HOOK_POINTS; i++) {
      lives_hooks_clear(tdata->vars.var_hook_stacks, i);
      pthread_mutex_destroy(hook_stacks[i]->mutex);
      lives_free(hook_stacks[i]->mutex);
      lives_free(hook_stacks[i]);
    }
  }

  pthread_cond_destroy(tdata->vars.var_pcond);
  lives_free(tdata->vars.var_pcond);
  pthread_mutex_destroy(tdata->vars.var_pause_mutex);
  lives_free(tdata->vars.var_pause_mutex);

  if (tdata->vars.var_guiloop) g_main_loop_unref(tdata->vars.var_guiloop);
  if (tdata->vars.var_guictx && tdata->vars.var_guictx != g_main_context_default())
    g_main_context_unref(tdata->vars.var_guictx);
  if (tdata->vars.var_guisource) g_source_unref(tdata->vars.var_guisource);

#if USE_RPMALLOC
  if (rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_finalize();
  }
#endif
}


static boolean thrdpool(void *arg);

static  pthread_once_t do_once = PTHREAD_ONCE_INIT;

static void make_pth_key(void) {
  (void)pthread_key_create(&tdata_key, lives_thread_data_destroy);
}

void *lives_thread_data_create(void *pslot_id) {
  lives_thread_data_t *tdata;
  int slot_id = LIVES_POINTER_TO_INT(pslot_id);

  pthread_once(&do_once, make_pth_key);
  tdata = pthread_getspecific(tdata_key);

  if (!tdata) {
#if USE_RPMALLOC
    // must be done before anything else
    if (!rpmalloc_is_thread_initialized()) {
      rpmalloc_thread_initialize();
    }
#endif

    tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));

    (void)pthread_setspecific(tdata_key, tdata);

    tdata->uid = tdata->vars.var_uid = gen_unique_id();

    if (slot_id < 0) {
      if (pthread_equal(pthread_self(), capable->main_thread)) {
        tdata->thrd_type = THRD_TYPE_MAIN;
        global_tdata = tdata;
      } else tdata->thrd_type = THRD_TYPE_EXTERN;
    } else tdata->thrd_type = THRD_TYPE_WORKER;

    tdata->slot_id = slot_id;

    pthread_rwlock_wrlock(&allctx_rwlock);
    allctxs = lives_list_prepend(allctxs, (livespointer)tdata);
    pthread_rwlock_unlock(&allctx_rwlock);

    tdata->thrd_self = pthread_self();

    tdata->vars.var_pause_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(tdata->vars.var_pause_mutex, NULL);
    tdata->vars.var_pcond = (pthread_cond_t *)lives_malloc(sizeof(pthread_cond_t));
    pthread_cond_init(tdata->vars.var_pcond, NULL);
    tdata->vars.var_sync_ready = TRUE;

    if (tdata->thrd_type < THRD_TYPE_EXTERN) {
      tdata->vars.var_guictx = lives_widget_context_new();
      lives_widget_context_push_thread_default(tdata->vars.var_guictx);

      tdata->vars.var_guiloop = g_main_loop_new(tdata->vars.var_guictx, FALSE);
      tdata->vars.var_guisource = lives_thrd_idle_priority(thrdpool, tdata);

      if (tdata->thrd_type == THRD_TYPE_MAIN) {
        lives_snprintf(tdata->vars.var_origin, 128, "%s", "LiVES Main Thread");
        tdata->vars.var_guictx = g_main_context_default();
        lives_widget_context_push_thread_default(tdata->vars.var_guictx);
        tdata->vars.var_guiloop = NULL;
        tdata->vars.var_guisource = NULL;
      } else {
        lives_snprintf(tdata->vars.var_origin, 128, "%s", "LiVES Worker Thread");
      }

      tdata->vars.var_rowstride_alignment = RS_ALIGN_DEF;
      tdata->vars.var_last_sws_block = -1;
    }

    tdata->vars.var_blocked_limit = BLOCKED_LIMIT;

    lives_icap_init(&tdata->vars.var_intentcap);

#ifndef NO_NP
    if (1) {
      pthread_attr_t attr;
      void *stack;
      size_t stacksize;
      pthread_getattr_np(tdata->thrd_self, &attr);
      pthread_attr_getstack(&attr, &stack, &stacksize);
      tdata->vars.var_stackaddr = stack;
      tdata->vars.var_stacksize = stacksize;
      pthread_attr_destroy(&attr);
    }
#endif
    for (int i = 0; i < N_HOOK_POINTS; i++) {
      tdata->vars.var_hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
      tdata->vars.var_hook_stacks[i]->mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(tdata->vars.var_hook_stacks[i]->mutex, NULL);
    }

    make_thrdattrs(tdata);
  }

  if (tdata->thrd_type >= THRD_TYPE_EXTERN) return tdata;

  while (1) {
    if (tdata->thrd_type != THRD_TYPE_WORKER && tdata->vars.var_guictx == g_main_context_default()) return tdata;
    if (tdata->vars.var_guictx != g_main_context_default())
      g_main_context_iteration(tdata->vars.var_guictx, TRUE);
    if (tdata->vars.var_guictx != g_main_context_default()) break;

    // we can do this - thread with main ctx, hands over main ctx, by setting our threadvar gictx to default
    // when we return from task, either completing or being ccancelled, we quit from here, ending the iteration
    //
    // force other thread to quit main loop, it will pop the old default ctx, find the loop and the source
    // (or create new source)
    // meanwhile:
    // push def context to thread def.
    // update loop and source
    // run the main loop

    lives_widget_context_push_thread_default(g_main_context_default());
    tdata->vars.var_guiloop = NULL;
    tdata->vars.var_guisource = lives_idle_priority(fg_service_fulfill_cb, NULL);
    g_main_context_iteration(g_main_context_default(), TRUE);
  }
  return NULL;
}


static boolean widget_context_wrapper(livespointer data) {
  thrd_work_t *mywork = (thrd_work_t *)data;
  // for lpt, we set this in call_funcsig
  mywork->flags |= LIVES_THRDFLAG_RUNNING;
  (*mywork->func)(mywork->arg);
  mywork->flags = (mywork->flags & ~LIVES_THRDFLAG_RUNNING) | LIVES_THRDFLAG_FINISHED;
  return FALSE;
}


static boolean lpt_remove_from_pool(lives_proc_thread_t lpt) {
  pthread_mutex_lock(&twork_mutex);
  for (LiVESList *list = (LiVESList *)twork_list; list; list = list->next) {
    thrd_work_t *mywork = (thrd_work_t *)list->data;
    if (mywork && mywork->lpt == lpt) {
      if ((LiVESList *)twork_list == list)
        twork_list = (volatile LiVESList *)list->next;
      if ((LiVESList *)twork_last == list)
        twork_last = (volatile LiVESList *)list->prev;
      if (list->prev) list->prev->next = list->next;
      if (list->next) list->next->prev = list->prev;
      ntasks--;
      pthread_mutex_unlock(&twork_mutex);
      list->next = list->prev = NULL;
      lives_thread_free((lives_thread_t *)list);
      return TRUE;
    }
  }
  pthread_mutex_unlock(&twork_mutex);
  return FALSE;
}


#define should_skip(lpt, work)						\
  (lpt ? ((lives_proc_thread_will_destroy(lpt) || lives_proc_thread_should_cancel(lpt)) ? TRUE \
	  : (work->flags & LIVES_THRDFLAG_SKIP_EXEC)) : FALSE)


static boolean do_something_useful(lives_thread_data_t *tdata) {
  /// yes, why don't you lend a hand instead of just lying around nanosleeping...
  lives_proc_thread_t lpt = NULL;
  LiVESList *list;
  thrd_work_t *mywork;
  boolean was_skipped = TRUE;

  if (tdata->thrd_type != THRD_TYPE_WORKER)
    lives_abort("Invalid worker thread type - internal error");

  pthread_mutex_lock(&twork_mutex);

  if (!(list = (LiVESList *)twork_list)) {
    pthread_mutex_unlock(&twork_mutex);
    return FALSE;
  }

  if ((LiVESList *)twork_last == list) twork_list = twork_last = NULL;
  else {
    twork_list = (volatile LiVESList *)list->next;
    twork_list->prev = NULL;
  }

  // removed from list
  if (!(mywork = (thrd_work_t *)list->data)) {
    ntasks--;
    pthread_mutex_unlock(&twork_mutex);
    list->next = list->prev = NULL;
    lives_thread_free((lives_thread_t *)list);
    return FALSE;
  }

  if (lives_proc_thread_ref((lpt = mywork->lpt)) < 2) lpt = NULL;

  pthread_mutex_unlock(&twork_mutex);

  list->next = list->prev = NULL;

  // check if lpt will be destroyed or cancelled
  if (should_skip(lpt, mywork)) goto skip_over;

  mywork->busy = tdata->uid;

  // STATE chenge - queued - queued / preparing
  if (lpt) lives_proc_thread_include_states(lpt, THRD_STATE_PREPARING);

  mywork->flags &= ~(LIVES_THRDFLAG_WAIT_START | LIVES_THRDFLAG_QUEUED_WAITING);

  // check if lpt will be destroyed or cancelled
  if (should_skip(lpt, mywork)) goto skip_over;

  // wait for SYNC READY
  if (!mywork->sync_ready) {
    pthread_mutex_t *pause_mutex = THREADVAR(pause_mutex);
    pthread_cond_t *pcond = THREADVAR(pcond);

    THREADVAR(sync_ready) = FALSE;

    pthread_mutex_lock(pause_mutex);
    if (lpt) {
      // STATE CHANGE - > gains WAITING
      lives_proc_thread_include_states(lpt, THRD_STATE_SYNC_WAITING);
      if (mywork->flags & LIVES_THRDFLAG_NOTE_TIMINGS)
        weed_set_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS,
                             lives_get_current_ticks());
    }

    while (!THREADVAR(sync_ready)) {
      mywork->flags |= LIVES_THRDFLAG_COND_WAITING;
      pthread_cond_wait(pcond, pause_mutex);
      mywork->flags &= ~LIVES_THRDFLAG_COND_WAITING;
    }
    pthread_mutex_unlock(pause_mutex);
    mywork->sync_ready = TRUE;

    if (lpt) {
      lives_proc_thread_exclude_states(lpt, THRD_STATE_SYNC_WAITING);
    }

    // check if lpt will be destroyed or cancelled
    if (should_skip(lpt, mywork)) goto skip_over;

    if (lpt) {
      if (mywork->flags & LIVES_THRDFLAG_NOTE_TIMINGS)
        weed_set_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS,
                             lives_get_current_ticks() -
                             weed_get_int64_value(lpt, LIVES_LEAF_SYNC_WAIT_TICKS, NULL));
    }
  }

  // check if lpt will be destroyed or cancelled
  if (should_skip(lpt, mywork)) goto skip_over;

  // RUN TASK
  lives_widget_context_invoke_full(tdata->vars.var_guictx, mywork->attrs & LIVES_THRDATTR_PRIORITY
                                   ? LIVES_WIDGET_PRIORITY_HIGH - 100 : LIVES_WIDGET_PRIORITY_HIGH,
                                   widget_context_wrapper, mywork, NULL);

  was_skipped = FALSE;

skip_over:

  if (lpt) {
    if (lives_proc_thread_has_states(lpt, THRD_STATE_CANCEL_REQUESTED)
        && !lives_proc_thread_has_states(lpt, THRD_STATE_CANCELLED))
      lives_proc_thread_cancel(lpt);
    if (was_skipped)
      lives_proc_thread_set_final_state(lpt);
  }

  // clear all hook stacks for the thread (self hooks)
  for (int i = N_GLOBAL_HOOKS + 1; i < N_HOOK_POINTS; i++) {
    lives_hooks_clear(tdata->vars.var_hook_stacks, i);
  }

  pthread_mutex_lock(&twork_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_mutex);

#if USE_RPMALLOC
  rpmalloc_thread_collect();
#endif

  if (mywork->flags & LIVES_THRDFLAG_AUTODELETE) {
    lives_thread_free((lives_thread_t *)list);
  } else {
    if (was_skipped) mywork->skipped = TRUE;
    else mywork->done = tdata->uid;
  }
  return TRUE;
}


#define POOL_TIMEOUT_SEC 120

static boolean thrdpool(void *arg) {
  boolean skip_wait = TRUE;
  static struct timespec ts;
  int rc = 0;
  lives_thread_data_t *tdata = (lives_thread_data_t *)arg;

  while (!threads_die) {
    if (!skip_wait) {
      int lifetime = POOL_TIMEOUT_SEC + fastrand_int(60);
      clock_gettime(CLOCK_REALTIME, &ts);
      // add random factor so we dont get multiple threads all timing out at once
      ts.tv_sec += lifetime;
      pthread_mutex_lock(&tcond_mutex);
      //g_print("thrd %d (0x%lx) WAITING : %d\n", tid, pself, tid);
      // there is no predicate here, since spurious wakeups are not a problem
      // they will just mean that the thread wont age this time
      rc = pthread_cond_timedwait(&tcond, &tcond_mutex, &ts);
      pthread_mutex_unlock(&tcond_mutex);
      //g_print("thrd %d (0x%lx) woke\n", tid, pself);
      if (rc == ETIMEDOUT) {
        // if the thread is waiting around doing nothing, and there are no tasks waitng,
        // exit, maybe free up some resources
        g_print("thrd %d (0x%lx) expired, idled for %d sec.\n", tdata->slot_id, pthread_self(), lifetime);
        if (!pthread_mutex_trylock(&pool_mutex)) {
          if (!pthread_mutex_trylock(&twork_mutex)) {
            if (ntasks < npoolthreads) {
              //g_print("thrd %d (0x%lx) leaving\n", tid, pself);
              pthread_t *myslot = poolthrds[tdata->slot_id];
              poolthrds[tdata->slot_id] = NULL;
              // slot can now be reused
              npoolthreads--;
              lives_free(myslot);
              tdata->exited = TRUE;
              pthread_mutex_unlock(&twork_mutex);
              pthread_mutex_unlock(&pool_mutex);
              break;
            }
            pthread_mutex_unlock(&twork_mutex);
          }
          pthread_mutex_unlock(&pool_mutex);
        }
      }
    }

    //lives_widget_context_pop_thread_default(tdata->vars.var_guictx);

    if (LIVES_UNLIKELY(threads_die)) break;

    //g_print("thrd %d (0x%lx) check for owrk\n", tid, pself);
    skip_wait = do_something_useful(tdata);
    if (skip_wait) {
#if USE_RPMALLOC
      // g_print("thrd %d (0x%lx) did someting\n", tid, pself);
      if (rpmalloc_is_thread_initialized()) {
        rpmalloc_thread_collect();
      }
#endif
    }
    if (tdata->vars.var_guictx == g_main_context_default()) {
      return FALSE;
    }
  }
  //  g_print("thrd %d (0x%lx) killed\n", tid, pself);

  return FALSE;
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
    pthread_create(poolthrds[i], NULL, lives_thread_data_create, LIVES_INT_TO_POINTER(i));
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  }
}


void lives_threadpool_finish(void) {
  threads_die = TRUE;
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_broadcast(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  for (int i = 0; i < rnpoolthreads; i++) {
    lives_thread_data_t *tdata = get_thread_data_by_slot_id(i);
    if (tdata) {
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_broadcast(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
      pthread_join(*(poolthrds[i]), NULL);
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
    thread->data = NULL;
    lives_free(work);
    if (!(flags & LIVES_THRDFLAG_NOFREE_LIST))
      lives_list_free(thread);
  }
}


void check_pool_threads(void) {
  pthread_mutex_lock(&pool_mutex);
  pthread_mutex_lock(&twork_mutex);

  while (ntasks >= npoolthreads && npoolthreads < rnpoolthreads) {
    for (int i = 0; i < rnpoolthreads; i++) {
      if (poolthrds[i]) continue;
      // relaunch thread, npoolthreads ---> rnpoolthreads
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      npoolthreads++;
      pthread_mutex_unlock(&twork_mutex);
      pthread_mutex_unlock(&pool_mutex);
      pthread_create(poolthrds[i], NULL, lives_thread_data_create, LIVES_INT_TO_POINTER(i));
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
      pthread_mutex_lock(&pool_mutex);
      pthread_mutex_lock(&twork_mutex);
      break;
    }
  }

  if (ntasks <= rnpoolthreads) {
    for (int i = 0; i < ntasks && i < rnpoolthreads; i++) {
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
  } else {
    // we need more threads to service all tasks
    int extrs = MAX(MINPOOLTHREADS, ntasks - rnpoolthreads);
    poolthrds =
      (pthread_t **)lives_realloc(poolthrds, (rnpoolthreads + extrs) * sizeof(pthread_t *));
    for (int i = rnpoolthreads; i < rnpoolthreads + extrs; i++) {
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, lives_thread_data_create, LIVES_INT_TO_POINTER(i));
    }
    rnpoolthreads += extrs;
    npoolthreads = rnpoolthreads;
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
  }
  pthread_mutex_unlock(&twork_mutex);
  pthread_mutex_unlock(&pool_mutex);
}


thrd_work_t *lives_thread_create(lives_thread_t **threadptr, lives_thread_attr_t attrs,
                                 lives_thread_func_t func, void *arg) {
  LiVESList *list = NULL;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  lives_proc_thread_t lpt = NULL;

  list = lives_list_append(list, work);
  work->func = func;
  work->attrs = attrs;
  work->arg = arg;
  work->flags = LIVES_THRDFLAG_QUEUED_WAITING;
  work->caller = THREADVAR(uid);

  if (threadptr) work->flags |= LIVES_THRDFLAG_NOFREE_LIST;

  if (attrs & LIVES_THRDATTR_IS_PROC_THREAD) {
    lpt = (lives_proc_thread_t)arg;
    work->lpt = lpt;
    if (attrs & LIVES_THRDATTR_IGNORE_SYNCPTS) {
      work->flags |= LIVES_THRDFLAG_IGNORE_SYNCPTS;
    }

    if (attrs & LIVES_THRDATTR_AUTO_PAUSE) {
      lives_proc_thread_include_states(lpt, THRD_OPT_AUTO_PAUSE);
    }

    if (attrs & LIVES_THRDATTR_AUTO_REQUEUE) {
      lives_proc_thread_include_states(lpt, THRD_OPT_AUTO_REQUEUE);
    }

    lives_proc_thread_set_work(lpt, work);
    if (attrs & LIVES_THRDATTR_NOTE_TIMINGS) {
      weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS,
                           lives_get_current_ticks());
      work->flags |= LIVES_THRDFLAG_NOTE_TIMINGS;
    }
  }

  if (!threadptr || (attrs & LIVES_THRDATTR_AUTODELETE)) {
    work->flags |= LIVES_THRDFLAG_AUTODELETE;
  }

  if (attrs & LIVES_THRDATTR_WAIT_START) work->flags |= LIVES_THRDFLAG_WAIT_START;
  if (!(attrs & LIVES_THRDATTR_WAIT_SYNC)) work->sync_ready = TRUE;

  pthread_mutex_lock(&twork_mutex);
  if (lpt && ((lives_proc_thread_get_state(lpt)
               & (THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0)) {
    //ooops !!
    lives_thread_free(list);
    /////
    pthread_mutex_unlock(&twork_mutex);
    return NULL;
  }
  if (mainw->debug) {
    g_print("adding to pool %p\n", list);
  }
  if (!twork_list) {
    twork_list = twork_last = list;
  } else {
    if (attrs & LIVES_THRDATTR_PRIORITY) {
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

  check_pool_threads();

  if (threadptr) *threadptr = list;

  return work;
}


uint64_t lives_thread_join(lives_thread_t *thread, void **retval) {
  thrd_work_t *task = (thrd_work_t *)thread->data;
  uint64_t nthrd = 0;

  if (task->flags & LIVES_THRDFLAG_AUTODELETE) {
    LIVES_FATAL("lives_thread_join() called on an autodelete thread");
    return 0;
  }

  if (!task->skipped) {
    if (!task->busy) check_pool_threads();
    lives_nanosleep_until_nonzero(task->done);
  }

  nthrd = task->done;
  lives_thread_free(thread);
  return nthrd;
}


LIVES_GLOBAL_INLINE uint64_t lives_thread_done(lives_thread_t *thrd) {
  thrd_work_t *task = (thrd_work_t *)thrd->data;
  if (!task) return TRUE;
  return task->done;
}


///////////// refcounting ///////////////

LIVES_GLOBAL_INLINE boolean check_refcnt_init(lives_refcounter_t *refcount) {
  if (refcount) {
    if (!refcount->mutex_inited) {
      refcount->mutex_inited = TRUE;
      pthread_mutex_init(&refcount->mutex, NULL);
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
  return 0;
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
  return 0;
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
  return -1;
}


LIVES_GLOBAL_INLINE int weed_refcount_dec(weed_plant_t *plant) {
  // value of 0 indicates the plant should be freed with weed_plant_free()
  // if plant does not have a refcounter, 0 will be returned.
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    return refcount_dec(refcnt);
  }
  return -1;
}


LIVES_GLOBAL_INLINE int weed_refcount_query(weed_plant_t *plant) {
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return 1;
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
    lives_thread_data_t *tdata  = (lives_thread_data_t *)list->data;
    lives_proc_thread_t lpt = tdata->vars.var_proc_thread;
    if (tdata) {
      thrd_work_t *work = lives_proc_thread_get_work(lpt);
      if (pthread_equal(tdata->thrd_self, capable->gui_thread)) notes = lives_strdup("GUI thread");
      if (pthread_equal(tdata->thrd_self, pthread_self())) notes = lives_strdup("me !");
      msg = lives_strdup_concat(msg, "\n", "Thread %d (%s) is %d", tdata->slot_id, notes,
                                work ? "busy" : "idle");
      if (notes) lives_free(notes);
    }
  }
  pthread_rwlock_unlock(&allctx_rwlock);
  return msg;
}
