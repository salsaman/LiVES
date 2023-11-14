// threading.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "effects-weed.h" // for LIVES_LEAF_CONST_VALUE

#ifdef AUDIT_REFC
weed_plant_t *auditor_refc = NULL;
#endif

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

   - SET_CANCELLABLE - set the proc_thread cancellablke state to TRUE

   AUTO_REQUEUE - after completing (unless cancelled or error), the same proc_thread, or a designated "followup"
   will be queued. If combined with AUTO_PAUSE, the followup will be run immediately by
   the same thread. If the first proc__thread has no "followup" it will be itself reqeueud
   If subseqeunt proc_threads in the chain have no "followup", the original proc_thread will
   complete. All the proc_threads must be joined after completing / cancelling / error.
   each proc thread can pass static data to the the followups (see proc_thread DATA).

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
static uint64_t lives_proc_thread_set_final_state(lives_proc_thread_t lpt);

static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////// GENERIC HOOK CALLBACKS ////

/// useful hook callbacks for proc_thread magic
// (lsee also: nullify_ptr_cb)
// 'other' here is just to highlight the possible scope of the callbacks,
// target can also be 'self' if it makes sense to do so

boolean wake_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other) {
  // hook callback to resume a paused / waiting proc_thread
  // e.g pause and wake simultaneous with other ptoc_thread
  //lives_proc_thread_add_hook(lpt, RESUMING_HOOK, 0, wake_other_lpt, self);
  if (!other) return FALSE;
  lives_proc_thread_request_resume(other);
  return FALSE;
}


boolean pause_request_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other) {
  // hook callback ask other thread to pause
  // e.g have another proc_thread pause when this one resumes
  //lives_proc_thread_add_hook(self, RESUMING_HOOK, 0, pause_request_other_lpt, lpt);
  if (!other) return FALSE;
  lives_proc_thread_request_pause(other);
  return FALSE;
}


boolean cancel_request_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other) {
  // hook callback ask other thred to cancel
  // e.g have another proc_thread cancel when this is cancelled
  //lives_proc_thread_add_hook(self, CANCELLED_HOOK, 0, cancel_request_other_lpt, lpt);
  if (!other) return FALSE;
  lives_proc_thread_request_cancel(other, TRUE);
  return FALSE;
}


boolean hailmary_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other) {
  // this is a 'cheat code', adding this callback to
  lives_proc_thread_exclude_states(other, THRD_STATE_DESTROYING);
  return FALSE;
}


boolean queue_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other) {
  // hook callback to queue other lpt for actioning
  // e.g queue another proc_thread to run when this on completes
  //lives_proc_thread_add_hook(self, FINISHED_HOOK, 0, queue_other_lpt, other);
  // queue attrs will be taken from other attrs
  int64_t attrs;
  if (!other) return FALSE;
  attrs = lives_proc_thread_get_attrs(other);
  lives_proc_thread_queue(other, attrs);
  return FALSE;
}

//////////////////////////////////////////////


LIVES_GLOBAL_INLINE thrd_work_t *lives_proc_thread_get_work(lives_proc_thread_t lpt) {
  return lpt ? ((thrd_work_t *)weed_get_voidptr_value((lpt), LIVES_LEAF_THREAD_WORK, NULL)) : NULL;
}


LIVES_GLOBAL_INLINE lives_thread_data_t *lives_proc_thread_get_thread_data(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_THREAD_DATA, NULL) : NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_thread_data(lives_proc_thread_t lpt,
    lives_thread_data_t *tdata) {
  if (lpt) weed_set_voidptr_value(lpt, LIVES_LEAF_THREAD_DATA, (void *)tdata);
}


LIVES_GLOBAL_INLINE void lpt_desc_state(lives_proc_thread_t lpt) {
  if (lpt) {
    char *tmp = lives_proc_thread_state_desc(lives_proc_thread_get_state(lpt));
    g_printerr("Thread state is: %s\n", tmp);
    lives_free(tmp);
  }
}


// auto-pausse, returns TRUE if timedout, FALSE if another thread resumed it
boolean _lives_proc_thread_wait(lives_proc_thread_t self, uint64_t nanosec, boolean have_lock) {
  if (!nanosec) return TRUE;
  pthread_mutex_t *pause_mutex = NULL;
  pthread_cond_t *pcond = NULL;
  if (self) {
    if (!pause_mutex)  pause_mutex = &(THREADVAR(pause_mutex));
    if (!pcond) pcond = &(THREADVAR(pcond));
    uint64_t sec;
    int rc = 0;
    struct timespec ts;
    THREADVAR(sync_ready) = FALSE;
    clock_gettime(CLOCK_REALTIME, &ts);
    nanosec += ts.tv_nsec;
    sec = nanosec / ONE_BILLION;
    nanosec -= sec * ONE_BILLION;
    ts.tv_sec += sec;
    ts.tv_nsec = nanosec;

    if (!have_lock)pthread_mutex_lock(pause_mutex);

    //lives_proc_thread_include_states(self, THRD_STATE_AUTO_PAUSED);
    while (!rc //&& !lives_proc_thread_get_resume_requested(self)
           && !lives_proc_thread_should_cancel(self)) {
      rc = pthread_mutex_timedlock(pause_mutex, &ts);
    }

    if (!have_lock) pthread_mutex_unlock(pause_mutex);

    /* lives_proc_thread_resume(self); */
    /* if (lives_proc_thread_get_cancel_requested(self)) { */
    /*   if (have_lock) pthread_mutex_unlock(pause_mutex); */
    /*   lives_proc_thread_cancel(self); */
    /*   return FALSE; */
    /* } */
    /* if (rc == ETIMEDOUT) return TRUE; */
  }
  return FALSE;
}


// auto-pausse, returns TRUE if timedout, FALSE if another thread resumed it
boolean lives_proc_thread_wait(lives_proc_thread_t self, uint64_t nanosec) {
  return _lives_proc_thread_wait(self, nanosec, FALSE);
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


LIVES_GLOBAL_INLINE void lives_proc_thread_make_indellible(lives_proc_thread_t lpt, const char *name) {
  weed_plant_t *book = lives_proc_thread_get_book(lpt);
  if (book && weed_plant_has_leaf(book, name))
    weed_leaf_set_undeletable(book, name, TRUE);
}


LIVES_GLOBAL_INLINE weed_error_t lives_proc_thread_set_book(lives_proc_thread_t lpt, weed_plant_t *book) {
  if (lpt) return weed_set_plantptr_value(lpt, LIVES_LEAF_LPT_BOOK, book);
  return WEED_ERROR_NOSUCH_PLANT;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_get_book(lives_proc_thread_t lpt) {
  return lpt ? weed_get_plantptr_value(lpt, LIVES_LEAF_LPT_BOOK, NULL) : NULL;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_has_book(lives_proc_thread_t lpt) {
  return lpt ? weed_plant_has_leaf(lpt, LIVES_LEAF_LPT_BOOK) : FALSE;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_ensure_book(lives_proc_thread_t lpt) {
  if (lpt) {
    weed_plant_t *book = weed_get_plantptr_value(lpt, LIVES_LEAF_LPT_BOOK, NULL);
    if (!book) {
      book = lives_plant_new(LIVES_PLANT_BAG_OF_HOLDING);
      lives_proc_thread_set_book(lpt, book);
    }
    return book;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_share_book(lives_proc_thread_t dst,
    lives_proc_thread_t src) {
  // if we have subordinate proc_threads, then data can be passed from one chain member to the next
  // and then finally back to the chain prime
  //
  // if src is non NULL, we take data from src, else we take data from dst
  // the function will strip any data not flagged as "static" (meaning flagged ass undletable)
  // then if dst is non-NULL and doesnt already have this data et,
  // it will get data as its data, and we add a ref to data
  //
  weed_plant_t *book = NULL;
  if (dst || src) {
    if (src) lives_proc_thread_ref(src);
    if (dst) lives_proc_thread_ref(dst);
    if (src) book = lives_proc_thread_get_book(src);
    else book = lives_proc_thread_get_book(dst);
    if (book) {
      weed_refcount_inc(book);
      // this works because: with default _weed_plant_free(), any undeletable leaves are retained
      // and plant is not freed. incrementing refcount adds an undeleteable leaf
      // (refcount) if that does not already exist.
      // thus, the plant cannot be freed, but any leaves NOT flagged as undeletable WILL be deleted.
      // - undeleteable leaves include "type", "uid", the refcounter itself
      // and ANY data added as "static", thus the effect will be to delete any data not flagged as "static"

      _weed_plant_free(book);

      // when extending from src to dst, we leave the added ref,
      // since when dst is freed, it will unref book

      if (!dst || lives_proc_thread_get_book(dst) == book)
        weed_refcount_dec(book);
      else lives_proc_thread_set_book(dst, book);
    }
    if (dst) lives_proc_thread_unref(dst);
    if (src) lives_proc_thread_unref(src);
  }
  return book;
}


lives_proc_thread_t add_garnish(lives_proc_thread_t lpt, const char *fname, lives_thread_attr_t *attrs) {
  lives_hook_stack_t **hook_stacks;

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL)) {
    pthread_mutex_t *state_mutex = (pthread_mutex_t *)lives_calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(state_mutex, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, state_mutex);
  }

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL)) {
    pthread_rwlock_t *destruct_rwlock = (pthread_rwlock_t *)lives_calloc(1, sizeof(pthread_rwlock_t));
    pthread_rwlock_init(destruct_rwlock, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, destruct_rwlock);
  }

  hook_stacks = (lives_hook_stack_t **)lives_calloc(N_HOOK_POINTS, sizeof(lives_hook_stack_t *));

  for (int i = 0; i < N_HOOK_POINTS; i++) {
    hook_stacks[i] = (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
    pthread_mutex_init(&hook_stacks[i]->mutex, NULL);
    hook_stacks[i]->owner = lpt;
  }

  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, (void *)hook_stacks);
  if (weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, NULL) != (void *)hook_stacks) abort();
  weed_set_const_string_value(lpt, LIVES_LEAF_FUNC_NAME, fname);

  if (attrs && (*attrs & LIVES_THRDATTR_CREATE_REFFED)) lives_proc_thread_ref(lpt);

  lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);

  return lpt;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_nullify_on_destruction(lives_proc_thread_t lpt,
    void **ptr) {
  if (lpt && ptr) {
    lives_proc_thread_add_hook(lpt, DESTRUCTION_HOOK, 0, lives_nullify_ptr_cb, (void *)ptr);
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_nullify_on_completion(lives_proc_thread_t lpt,
    void **ptr) {
  if (lpt && ptr) {
    lives_proc_thread_add_hook(lpt, COMPLETED_HOOK, 0, lives_nullify_ptr_cb, (void *)ptr);
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t _lpt_auto_nullify(lives_proc_thread_t *plpt) {
  if (plpt && *plpt) {
    lives_proc_thread_nullify_on_destruction(*plpt, (void **)plpt);
    return *plpt;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t _lpt_remove_nullify(lives_proc_thread_t lpt, void **ptr) {
  if (lpt && ptr)
    lives_proc_thread_remove_hook_by_data(lpt, DESTRUCTION_HOOK, lives_nullify_ptr_cb, (void *)ptr);
  return lpt;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_remove_nullify(lives_proc_thread_t lpt, void **ptr) {
  return lpt &&  _lpt_remove_nullify(lpt, ptr) == lpt;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_new(const char *fname, lives_thread_attr_t *attrs) {
  lives_proc_thread_t lpt;
  /* #if USE_RPMALLOC */
  /*   // free up some thread memory before allocating more */
  /*   if (rpmalloc_is_thread_initialized()) */
  /*     rpmalloc_thread_collect(); */
  /* #endif */
  lpt = lives_plant_new(LIVES_PLANT_PROC_THREAD);
  if (lpt) add_garnish(lpt, fname, attrs);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_vargs(lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *fname, int return_type, const char *args_fmt, va_list xargs) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, &attrs);
  if (fname) add_quick_fn(func, fname);
  _proc_thread_params_from_vargs(lpt, func, return_type, args_fmt, xargs);
  lpt = lives_proc_thread_run(attrs, lpt, return_type);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_nullvargs(lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *fname, int return_type) {
  lives_proc_thread_t lpt = lives_proc_thread_new(fname, &attrs);
  if (fname) add_quick_fn(func, fname);
  _proc_thread_params_from_nullvargs(lpt, func, return_type);
  return lives_proc_thread_run(attrs, lpt, return_type);
}


lives_proc_thread_t lives_proc_thread_chain(lives_proc_thread_t lpt, ...) {
  va_list va;
  va_start(va, lpt);
  while (1) {
    lives_funcptr_t func = va_arg(va, lives_funcptr_t);
    if (!func) break;
    lives_proc_thread_t xlpt;
    uint64_t attrs = LIVES_THRDATTR_START_UNQUEUED | LIVES_THRDATTR_NXT_IMMEDIATE;
    int32_t rtype = va_arg(va, int32_t);
    const char *fname = get_funcname(func), *args_fmt = va_arg(va, const char *);
    if (args_fmt && *args_fmt)
      xlpt = _lives_proc_thread_create_vargs(attrs, func, fname, rtype, args_fmt, va);
    else xlpt = _lives_proc_thread_create_nullvargs(attrs, func, fname, rtype);

    if (!lpt) lpt = xlpt;
    else lives_proc_thread_add_chain_next(lpt, xlpt);
  }
  va_end(va);
  return lpt;
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
    fdef->return_type = lives_proc_thread_get_rtype(lpt);
    fdef->funcsig = lives_proc_thread_get_funcsig(lpt);
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
   lives_proc_thread_is_done() with the return : - this function is guaranteed to return FALSE whilst the thread is running
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
  if (args_fmt && *args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    lpt = _lives_proc_thread_create_vargs(attrs, func, fname, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, fname, return_type);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_with_timeout_vargs
(ticks_t timeout, lives_thread_attr_t attrs, lives_funcptr_t func,
 const char *func_name, int return_type, const char *args_fmt, va_list xargs) {
  lives_proc_thread_t lpt;
  lives_cancel_t cancel;
  boolean is_fg = is_fg_thread();
  uint64_t tnsec = TICKS_TO_NSEC(timeout);

  attrs |= LIVES_THRDATTR_CREATE_REFFED;

  mainw->cancelled = CANCEL_NONE;

  if (args_fmt && *args_fmt) {
    lpt = _lives_proc_thread_create_vargs(attrs, func, func_name, return_type, args_fmt, xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, func_name, return_type);

  g_print("lpt w tim1\n");

  while (!lives_proc_thread_is_done(lpt, FALSE)
         && !(lives_proc_thread_is_running(lpt) && !lives_proc_thread_is_busy(lpt))
         && (cancel = mainw->cancelled) == CANCEL_NONE) {
    if (is_fg && !fg_service_fulfill())
      lives_widget_context_iteration(NULL, FALSE);
    else lives_microsleep;
  }

  g_print("lpt w tim2\n");

  while (cancel == CANCEL_NONE && !lives_proc_thread_is_done(lpt, FALSE)) {
    g_print("lpt w tim3 alm %lu\n", tnsec);
    lives_alarm_set_timeout(tnsec);
    while (!lives_proc_thread_is_done(lpt, FALSE) && !lives_alarm_triggered()) {
      if (is_fg && !fg_service_fulfill())
        lives_widget_context_iteration(NULL, FALSE);
      else lives_microsleep;
    }
    if (lives_proc_thread_is_busy(lpt)) {
      // thread MUST unavoidably block; stop the timer (e.g showing a UI)
      // user or other condition may set cancelled
      while (lives_proc_thread_is_busy(lpt)
             && (cancel = mainw->cancelled) == CANCEL_NONE) {
        if (is_fg && !fg_service_fulfill())
          lives_widget_context_iteration(NULL, FALSE);
        else lives_millisleep;
      }
    } else if (lives_alarm_triggered()) break;
  }

  if (lives_alarm_triggered()) {
    if (!lives_proc_thread_is_done(lpt, FALSE)) {
      g_print("TIMED IT OUT\n");
      pthread_t pth = lives_proc_thread_get_pthread(lpt);
      pthread_kill(pth, LIVES_INTERRUPT_SIG);
      lives_proc_thread_join(lpt);
      lives_proc_thread_unref(lpt);
      return NULL;
    }
    if (cancel != CANCEL_NONE) {
      lives_proc_thread_dontcare(lpt);
      lives_proc_thread_unref(lpt);
      return NULL;
    }
  }
  lives_proc_thread_unref(lpt);
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_with_timeout(ticks_t timeout,
    lives_thread_attr_t attrss, lives_funcptr_t func,
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
  if (!main_thread) return TRUE;
  return pthread_equal(pthread_self(), main_thread);
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
  lives_proc_thread_t lpt, lpt2 = NULL, xlpt = NULL;
  LiVESList *cbnext;
  pthread_mutex_t *hmutex = &(hstacks[LIVES_GUI_HOOK]->mutex),
                   *fgmutex = &(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);

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
    if (cl->flags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstacks[LIVES_GUI_HOOK], cblist);
      continue;
    }
    lpt = cl->proc_thread;

    if ((lpt2 = lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK,
                               cl->flags | HOOK_CB_FG_THREAD | HOOK_OPT_ONESHOT,
                               (void *)cl, DTYPE_HAVE_LOCK | DTYPE_CLOSURE)) != lpt) {
      if (cl->flags & HOOK_CB_BLOCK) {
        // lpt was added to bg thread deferral stack, but now due to unqieuness constraints,
        // it has been blocked from gui thread deferral stack
        // if it is flagged as blocking then, the caller must be waiting for it to run,
        // thus, just as if it were added to the gui thread stack initially, we need to return lpt2
        // so the waiting thread will wait on that instead
        // as in the usual case we need to add refs to lpt and lpt2
        xlpt = lpt2;
        lives_proc_thread_ref(lpt);
        lives_proc_thread_ref(lpt2);
      }
      lives_closure_free(cl);
    }
  }
  lives_list_free((LiVESList *)hstacks[LIVES_GUI_HOOK]->stack);
  hstacks[LIVES_GUI_HOOK]->stack = NULL;

  pthread_mutex_unlock(fgmutex);
  pthread_mutex_unlock(hmutex);
  return xlpt;
}


LIVES_LOCAL_INLINE lives_proc_thread_t prepend_all_to_fg_deferral_stack(void) {
  GET_PROC_THREAD_SELF(self);
  lives_hook_stack_t **hstacks = lives_proc_thread_get_hook_stacks(self);
  lives_proc_thread_t lpt, lpt2 = NULL;
  LiVESList *cbprev;

  pthread_mutex_t *hmutex = &(hstacks[LIVES_GUI_HOOK]->mutex),
                   *fgmutex = &(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);

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

#ifdef DEBUG_LPT_REFS
int _lives_proc_thread_ref(lives_proc_thread_t lpt) {
#else
int lives_proc_thread_ref(lives_proc_thread_t lpt) {
#endif
#if 0
}
#endif
int refs = -1;

if (lpt) {
  pthread_rwlock_t *destruct_rwlock;
  pthread_mutex_lock(&ref_sync_mutex);
  destruct_rwlock = (pthread_rwlock_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL);
  if (destruct_rwlock && !pthread_rwlock_rdlock(destruct_rwlock)) {
    pthread_mutex_unlock(&ref_sync_mutex);
    // having readlock, now we can unlock the mutex
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
#ifdef DEBUG_LPT_REFS
int _lives_proc_thread_unref(lives_proc_thread_t lpt) {
#else
int lives_proc_thread_unref(lives_proc_thread_t lpt) {
#endif
#if 0
}
#endif
T_RECURSE_GUARD_START;
if (lpt) {
  GET_PROC_THREAD_SELF(self);
  T_RETURN_VAL_IF_RECURSED_CHECK(lpt == self, FALSE);
  pthread_rwlock_t *destruct_rwlock;
  pthread_mutex_lock(&ref_sync_mutex);
  destruct_rwlock
    = (pthread_rwlock_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL);
  if (destruct_rwlock && !pthread_rwlock_rdlock(destruct_rwlock)) {
    int refs = weed_refcount_dec(lpt);
    if (refs != 0) {
      pthread_rwlock_unlock(destruct_rwlock);
      pthread_mutex_unlock(&ref_sync_mutex);
      return FALSE;
    }

    pthread_rwlock_unlock(destruct_rwlock);
    pthread_mutex_unlock(&ref_sync_mutex);

    // should stop any other threads trying to ref / unref
    weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL);

    // now wait to get write lock, this ensures anything that read rwlock before it was set to NULL
    // will return
    if (!pthread_rwlock_wrlock(destruct_rwlock)) {
      // cna't use get_thread_data_for_lpt() here, since we have passed the "compleeted" / "finished"
      // status, and that function will return NULL
      lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
      lives_proc_thread_t nxtlpt;
      weed_plant_t *data;
      int64_t state;
      pthread_mutex_t *state_mutex;
      lives_hook_stack_t **lpt_hooks = lives_proc_thread_get_hook_stacks(lpt);
      //if (lpt == mainw->debug_ptr) BREAK_ME("lpt free");
      if (lives_proc_thread_get_closure(lpt)) BREAK_ME("free lpt with closure !");

      state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);

      if (lives_proc_thread_get_work(lpt)) {
        // try to remove from pool, but we may be too late
        // however we also lock twork_list, and worker threads should give up if DESTROYING is set
        lpt_remove_from_pool(lpt);
      }

      pthread_mutex_lock(&mainw->all_hstacks_mutex);
      mainw->all_hstacks =
        lives_list_remove_data(mainw->all_hstacks, lpt_hooks, FALSE);
      pthread_mutex_unlock(&mainw->all_hstacks_mutex);

      nxtlpt = lives_proc_thread_get_chain_next(lpt);
      if (nxtlpt && nxtlpt != lives_proc_thread_get_chain_prime(lpt)) {
        lives_proc_thread_unref(nxtlpt);
      }

      lives_hooks_trigger(lpt_hooks, LIVES_GUI_HOOK);

      pthread_mutex_lock(state_mutex);
      state = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      state |= THRD_STATE_DESTROYED;
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, state);
      pthread_mutex_unlock(state_mutex);

      //lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
      lives_hooks_trigger(lpt_hooks, DESTRUCTION_HOOK);

      if (tdata) {
        // GET_PROC_THREAD_SELF will now return NULL, until we call lives_thread_switch_self()
        tdata->vars.var_active_lpt = NULL;
      }

      // this will force other lpts to remove their pointers to callbacks in our stacks
      T_RECURSE_GUARD_ARM;
      lives_hooks_clear_all(lpt_hooks, N_HOOK_POINTS);
      T_RECURSE_GUARD_END;

      lives_free(lpt_hooks);

      // remove any callbacks from stacks in other lpts / threads
      flush_cb_list(lpt);

      data = lives_proc_thread_get_book(lpt);
      if (data) {
        int nr = weed_refcount_dec(data);
        if (!nr) weed_plant_free(data);
      }

      weed_plant_free(lpt);

      pthread_mutex_destroy(state_mutex);
      lives_free(state_mutex);

      // pause briefly so that threads which just read rwlock or mutex
      // dont end up with invalid objects
      pthread_mutex_lock(&ref_sync_mutex);
      pthread_mutex_unlock(&ref_sync_mutex);

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
    if (lpt && lpt != lpt2) {
      lpt2 = lpt;
      lives_proc_thread_unref(lpt2);
    }
  }
  return lpt2;
}


LIVES_LOCAL_INLINE lives_proc_thread_t lives_proc_thread_get_replacement(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, NULL);
  return NULL;
}


lives_proc_thread_t lpt_wait_with_repl(lives_proc_thread_t lpt) {
  // wait on a blocking hook callback to complete.
  // While doing show we keep track in case the callback is displaced by another
  // (due to uniqueness constraints)
  lives_proc_thread_t lpt2 = lpt;
  while (lpt) {
    if (lpt2 != lpt) {
      // check
      lives_proc_thread_unref(lpt);
      lives_proc_thread_unref(lpt);
      lpt = lpt2;
      if (!lpt) break;
    }
    lpt2 = lives_proc_thread_get_replacement(lpt);
    if (!lpt2) lpt2 = lpt;
    if (lpt2 != lpt) {
      // lpt2 will have had 1 ref added, but here we will use double refcounting
      lives_proc_thread_ref(lpt2);
      continue;
    }

    lives_microsleep;
    // get retval
    if (lives_proc_thread_is_done(lpt, FALSE)) break;
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

  if (retloc) weed_set_voidptr_value(lpt, LIVES_LEAF_RETLOC, retloc);

  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  if (is_fg) {
    // run direct
    GET_PROC_THREAD_SELF(self);
    weed_set_plantptr_value(lpt, LIVES_LEAF_DISPATCHER, self);
    retval = lives_proc_thread_execute(lpt);
    lives_proc_thread_unref(lpt);
    goto mte_done2;
  } else {
    uint64_t attrs;
    if (THREADVAR(perm_hook_hints))
      hook_hints = THREADVAR(perm_hook_hints);
    else
      hook_hints = THREADVAR(hook_hints);

    THREADVAR(hook_hints) = 0;

    attrs = lives_proc_thread_get_attrs(lpt);

    if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
      weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());

    if (hook_hints & HOOK_OPT_FG_LIGHT) {
      lives_proc_thread_queue(lpt, attrs | LIVES_THRDATTR_FG_LIGHT
                              | LIVES_THRDATTR_NO_UNREF);
      lives_proc_thread_unref(lpt);
      goto mte_done;
    }

    if (!is_fg_service || (hook_hints & HOOK_CB_BLOCK) || (hook_hints & HOOK_CB_PRIORITY)) {
      // first level service call
      if (!(hook_hints & HOOK_CB_PRIORITY) && FG_THREADVAR(fg_service)) {

        // call is not priority,  and main thread is already performing
        // a service call - in this case we will add the request to main thread's deferral stack

        if (hook_hints & HOOK_CB_BLOCK) lives_proc_thread_ref(lpt); // !!! pt ref 1

        // we do a pre check here - because of uniqeness constraints it may not be posible to queue the original lpt
        // in this case we would still blok but wait on a differnet proc thread (ie, the one that replaced ours)
        lpt2 = what_to_wait_for(lpt, hook_hints);

        /* if (lpt2 != lpt) { */
        /*   // if we get back a different proc_thread, that means that this one was blocked from */
        /*   // the stack (due to uniqueness restrictions for example) */
        /*   // if we are not blocking, we can just unref it */
        /*   // if blocking, then we reffed it above, so this only undoes that */
        /*   if (!(hook_hints & HOOK_CB_BLOCK)) { */
        /*     lives_proc_thread_unref(lpt); */
        /*   } */
        /* } */

        if (!(hook_hints & HOOK_CB_BLOCK)) {
          retval = FALSE;
          goto mte_done;
        }
        if (lpt2 != lpt) {
          // remove added ref
          lives_proc_thread_unref(lpt);
          // free lpt, since it was replaced
          lives_proc_thread_unref(lpt);
          // if replaced, lpt2 will have been reffed once,
          // i.e. extra adde replaces ours
          // so this is OK for next time
          lpt = lpt2;
          lives_proc_thread_ref(lpt);
        }

        lpt = lpt_wait_with_repl(lpt);

        // we need to check for invalid, because in the case of a togglefunc, lpt is repalced by lpt2
        // but lpt2 is marked invalid
        if (lpt && !lives_proc_thread_is_invalid(lpt)) {
          lives_proc_thread_unref(lpt);
          lives_proc_thread_unref(lpt);
        }
        goto mte_done;
      }

      // high priority, or gov loop is running or main thread is running a service call
      // we need to wait and action this rather than add it to the queue
      //
      // if priority and not blocking, we will append lpt to the thread stack
      // then prepend our stack to maim thread
      // must not unref lpt, as it is in a hook_stack, it well be unreffed when the closure is freed

      // add ref
      lives_proc_thread_ref(lpt);

      lpt2 = add_to_deferral_stack(lpt, hook_hints);

      if (lpt2 != lpt) {
        lives_proc_thread_unref(lpt);
        //lives_proc_thread_unref(lpt);
        lpt = lpt2;
        lives_proc_thread_ref(lpt);
      }

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
          lpt2 = append_all_to_fg_deferral_stack();
          if (lpt2 && lpt2 != lpt) {
            lives_proc_thread_unref(lpt);
            // lives_proc_thread_unref(lpt);
            lpt = lpt2;
            lives_proc_thread_ref(lpt);
          }
          lpt = lpt_wait_with_repl(lpt);
        }
        // we need to check for invalid, because in the case of a togglefun, lpt is repalced by lpt2
        // but lpt2 is marked invalid
        if (lpt && !lives_proc_thread_is_invalid(lpt))
          lives_proc_thread_unref(lpt);
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
  if (!args_fmt || !*args_fmt) return _main_thread_execute_vargs(func, fname, WEED_SEED_VOID, NULL, "", NULL);
  va_start(xargs, args_fmt);
  bret = _main_thread_execute_vargs(func, fname, WEED_SEED_VOID, NULL, args_fmt, xargs);
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


LIVES_GLOBAL_INLINE const char *lives_proc_thread_get_funcname(lives_proc_thread_t lpt) {
  if (lpt) return weed_get_const_string_value(lpt, LIVES_LEAF_FUNC_NAME, NULL);
  return NULL;
}


LIVES_GLOBAL_INLINE funcsig_t lives_proc_thread_get_funcsig(lives_proc_thread_t lpt) {
  return weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL);
}


LIVES_GLOBAL_INLINE char *lives_proc_thread_get_args_fmt(lives_proc_thread_t lpt) {
  return args_fmt_from_funcsig(lives_proc_thread_get_funcsig(lpt));
}


LIVES_GLOBAL_INLINE uint32_t lives_proc_thread_get_rtype(lives_proc_thread_t lpt)
{return weed_leaf_seed_type(lpt, _RV_);}


// check any of states (c.f has_states)
LIVES_GLOBAL_INLINE uint64_t _lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    uint64_t tstate;
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    return tstate & state_bits;
  }
  return THRD_STATE_INVALID;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      if (state_bits != THRD_STATE_INVALID)
        pthread_mutex_lock(state_mutex);
      tstate = _lives_proc_thread_check_states(lpt, state_bits);
      if (state_bits != THRD_STATE_INVALID)
        pthread_mutex_unlock(state_mutex);
      return tstate;
    }
  }
  return THRD_STATE_INVALID;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_has_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  // test if ALL states in state_bits are set (cf. check_states)
  return (lpt && lives_proc_thread_check_states(lpt, state_bits) == state_bits);
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    // if new state includes DESTROYED, refccount will be zero and -1 will be returned
    int nrefs = lives_proc_thread_ref(lpt);
    boolean do_refs = FALSE;
    uint64_t tstate = THRD_STATE_INVALID;
    uint64_t masked = 0;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (nrefs > 1) do_refs = TRUE;
    if (state_mutex) {
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      // remove any bits from state_bits already in tstate
      // so we can tell which ones changed
      state_bits &= ~tstate;
      if (tstate & THRD_STATE_IDLING) masked |= THRD_STATE_FINISHED;
      tstate |= (state_bits & ~masked);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate);
      pthread_mutex_unlock(state_mutex);

      if (!(tstate & THRD_BLOCK_HOOKS)) {
        lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
        // only new bits

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
          // this hook is triggered when processing is complete
          // if combined with DESTROYING, the next hook point will be DESTROYED and then
          // lpt will be unreffed (*unless it gets a 'hailmary')
          // if combineding with IDLING, the next hook depends on external triggers
          // in all other cases the next and last hook will be FINISHED
          lives_hooks_trigger(hook_stacks, COMPLETED_HOOK);
        }

        if (state_bits & THRD_STATE_FINISHED) {
          lives_hooks_trigger(hook_stacks, FINISHED_HOOK);
        }
      }

      // some states are reflected in the prime_lpt
      if (lives_proc_thread_is_subordinate(lpt)) {
        uint64_t cstate = state_bits & INC_TX_BITS;
        lives_proc_thread_t parent = weed_get_plantptr_value(lpt, "parent", NULL);
        if (parent) lives_proc_thread_include_states(parent, cstate);
      }
    }
    if (do_refs) lives_proc_thread_unref(lpt);
    return tstate;
  }
  return THRD_STATE_INVALID;
}


uint64_t lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lives_proc_thread_ref(lpt) > 1) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      uint64_t tstate;
      pthread_mutex_lock(state_mutex);
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      state_bits &= tstate;
      tstate &= ~state_bits;
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate);
      pthread_mutex_unlock(state_mutex);

      // some states are reflected in the prime_lpt
      if (lives_proc_thread_is_subordinate(lpt)) {
        lives_proc_thread_t parent = weed_get_plantptr_value(lpt, "parent", NULL);
        uint64_t cstate = state_bits & EXC_TX_BITS;
        if (parent) lives_proc_thread_exclude_states(parent, cstate);
      }

      lives_proc_thread_unref(lpt);

      // because nanosleep is a cancellation point
      if (state_bits & THRD_STATE_BUSY) lives_cancel_point;
      return tstate;
    }

    lives_proc_thread_unref(lpt);
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
  if (lpt && (lives_proc_thread_check_states(lpt, THRD_STATE_PAUSED
              | THRD_STATE_AUTO_PAUSED))) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_preparing(lives_proc_thread_t lpt) {
  if (lpt) {
    if (lives_proc_thread_has_states(lpt, THRD_STATE_PREPARING)) return TRUE;
    if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_stacked(lives_proc_thread_t lpt) {
  if (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_STACKED))) return TRUE;
  return FALSE;
}


// check if thread finished normally
LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_FINISHED)));
}


LIVES_LOCAL_INLINE boolean lives_proc_thread_check_completed(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_COMPLETED)));
}


// check if thread is idling (only set if idlefunc was queued at least once already)
LIVES_GLOBAL_INLINE boolean lives_proc_thread_paused_idling(lives_proc_thread_t lpt) {
  return lives_proc_thread_has_states(lpt, THRD_STATE_IDLING | THRD_STATE_PAUSED);
}


// check if thread finished normally, is idling or will be destroyed
LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_done(lives_proc_thread_t lpt, boolean ignore_cancel) {
  if (lpt) {
    if (lives_proc_thread_is_invalid(lpt)) return TRUE;
    if (lives_proc_thread_is_suspended(lpt)) return FALSE;

    return (lives_proc_thread_check_finished(lpt) || lives_proc_thread_paused_idling(lpt)
            || (lives_proc_thread_check_completed(lpt) && lives_proc_thread_will_destroy(lpt))
            || (!ignore_cancel && lives_proc_thread_was_cancelled(lpt)));
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_will_destroy(lives_proc_thread_t lpt) {
  return lpt && lives_proc_thread_has_states(lpt, THRD_STATE_DESTROYING);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_should_cancel(lives_proc_thread_t lpt) {
  return lpt && ((lives_proc_thread_get_state(lpt) &
                  (THRD_STATE_CANCELLED | THRD_STATE_CANCEL_REQUESTED)) != 0);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_invalid(lives_proc_thread_t lpt) {
  return !lpt || lives_proc_thread_has_states(lpt, THRD_STATE_INVALID);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_busy(lives_proc_thread_t lpt) {
  return lpt && lives_proc_thread_has_states(lpt, THRD_STATE_BUSY);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_can_interrupt(lives_proc_thread_t lpt) {
  return lpt && lives_proc_thread_has_states(lpt, THRD_OPT_CAN_INTERRUPT);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_sync_waiting(lives_proc_thread_t lpt) {
  if (lpt) {
    if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
    if (lives_proc_thread_has_states(lpt, THRD_STATE_SYNC_WAITING)) return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_waiting(lives_proc_thread_t lpt) {
  // self / conditional wait
  if (lpt) {
    if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
    if (lives_proc_thread_has_states(lpt, THRD_STATE_WAITING)) return TRUE;
  }
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


LIVES_GLOBAL_INLINE int lives_proc_thread_get_errsev(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_ERRSEV, NULL) : 0;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_SIGNALLED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_suspended(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
    if (tdata && lpt == tdata->vars.var_prime_lpt) {
      pthread_mutex_t *alpt_mutex = &tdata->vars.var_active_lpt_mutex;
      lives_proc_thread_t active_lpt;
      pthread_mutex_lock(alpt_mutex);
      active_lpt = tdata->vars.var_active_lpt;
      pthread_mutex_unlock(alpt_mutex);
      if (active_lpt != lpt) return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_subordinate(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
    if (tdata) {
      lives_proc_thread_t plpt = tdata->vars.var_prime_lpt;
      if (plpt && plpt != lpt) return TRUE;
    }
  }
  return FALSE;
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
  if (lpt) lives_proc_thread_include_states(lpt, THRD_OPT_CANCELLABLE);
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_loveliness(lives_proc_thread_t lpt, double how_lovely_it_is) {
  if (lpt) {
    if (how_lovely_it_is < 1.) how_lovely_it_is = 1.;
    if (how_lovely_it_is > MAX_LOVELINESS) how_lovely_it_is = MAX_LOVELINESS;
    LPT_THREADVAR_SET(lpt, loveliness, how_lovely_it_is);
  }
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_OPT_CANCELLABLE)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_was_cancelled(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
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
  lives_proc_thread_t xlpt;
  lives_thread_data_t *tdata;
  pthread_mutex_t *alpt_mutex;
  boolean ret;

  if (!lpt) return FALSE;

  if (lives_proc_thread_ref(lpt) > 1) {
    if (lives_proc_thread_is_unqueued(lpt)) {
      lives_proc_thread_include_states(lpt, (THRD_STATE_CANCEL_REQUESTED | THRD_STATE_CANCELLED));
      lives_proc_thread_unref(lpt);
      return TRUE;
    }

    ret = lives_proc_thread_is_done(lpt, TRUE);
    if (ret) {
      lives_proc_thread_unref(lpt);
      return FALSE;
    }

    if (dontcare && !lives_proc_thread_dontcare(lpt)) {
      // lpt already finished before we could set this
      lives_proc_thread_unref(lpt);
      return FALSE;
    }

    if (!lives_proc_thread_get_cancellable(lpt)) {
      lives_proc_thread_unref(lpt);
      return FALSE;
    }

    // some requests may be forwarded to active_lpt
    // (includes cancel, resume, pause)
    tdata = lives_proc_thread_get_thread_data(lpt);
    if (tdata) {
      alpt_mutex = &tdata->vars.var_active_lpt_mutex;
      pthread_mutex_lock(alpt_mutex);
      xlpt = tdata->vars.var_active_lpt;
      if (xlpt == lpt) xlpt = NULL;
      if (xlpt) lives_proc_thread_ref(xlpt);
      pthread_mutex_unlock(alpt_mutex);

      if (xlpt) {
        boolean ret = lives_proc_thread_request_cancel(xlpt, FALSE);
        lives_proc_thread_unref(xlpt);
        lives_proc_thread_unref(lpt);
        return ret;
      }
    }

    lives_proc_thread_exclude_states(lpt, THRD_STATE_PAUSE_REQUESTED);
    lives_proc_thread_include_states(lpt, THRD_STATE_CANCEL_REQUESTED);

    if (lives_proc_thread_is_paused(lpt)) lives_proc_thread_request_resume(lpt);

    lives_proc_thread_unref(lpt);
    return TRUE;
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancel_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_CANCEL_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t xself) {
  if (xself == mainw->debug_ptr) BREAK_ME("cancelled");
  if (xself) {
    GET_PROC_THREAD_SELF(self);
    if (!xself || (self && xself != self) ||
        lives_proc_thread_is_invalid(xself)) {
      LIVES_WARN("Invalid thread cancelled !");
      return FALSE;
    }
    if (lives_proc_thread_was_cancelled(xself)) {
      LIVES_WARN("proc_thread cancelled > 1 times !");
      return FALSE;
    }

    lives_proc_thread_include_states(xself, THRD_STATE_CANCELLED);
    lives_proc_thread_exclude_states(xself, THRD_STATE_CANCEL_REQUESTED);

    if (self) {
      if (weed_plant_has_leaf(self, LIVES_LEAF_LONGJMP)) {
        // prepare to jump into hyperspace...
        jmp_buf *env = (jmp_buf *)weed_get_voidptr_value(self, LIVES_LEAF_LONGJMP, NULL);
        if (env) siglongjmp(*env, THRD_STATE_CANCELLED >> 32);
        LIVES_WARN("canclled proc_thread had no longjmp destination");
      }
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_error(lives_proc_thread_t xself, int errnum, int severity,
    const char *fmt, ...) {
  char *errmsg = NULL;
  if (severity == LPT_ERR_DEADLY) _exit(errnum);

  if (severity == LPT_ERR_FATAL) LIVES_FATAL("-error-");

  if (fmt && *fmt) {
    va_list vargs;
    va_start(vargs, fmt);
    errmsg = lives_strdup_vprintf(fmt, vargs);
    va_end(vargs);
  }

  if (severity == LPT_ERR_CRITICAL) LIVES_CRITICAL(errmsg);

  // major, minor
  if (xself) {
    GET_PROC_THREAD_SELF(self);

    if (errmsg) {
      weed_set_string_value(xself, LIVES_LEAF_ERRMSG, errmsg);
      lives_free(errmsg);
    }

    lives_proc_thread_include_states(xself, THRD_STATE_ERROR);

    if (severity == LPT_ERR_MAJOR) {
      if (xself == self) {
        lives_proc_thread_cancel(self);
        // does not return !!
      } else lives_proc_thread_request_cancel(xself, FALSE);
    }
  }
  return TRUE;
}


boolean _lives_proc_thread_request_resume(lives_proc_thread_t lpt, boolean have_lock) {
  // to ensure proper synchronisation, - set resume_requested, and wake thread
  // and wait for paused state to go away.
  // - target thread wakes, removes paused state, but waits for resume_req. to be cleared
  // once paused is cleared, remove resume_req, allowing target to unblock
  if (lives_proc_thread_ref(lpt) > 1) {
    lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
    if (tdata) {
      pthread_mutex_t *alpt_mutex;
      lives_proc_thread_t xlpt;
      pthread_mutex_t *pause_mutex;
      pthread_cond_t *pcond;

      // some requests may be forwarded to active_lpt
      // (includes cancel, resume, pause)
      alpt_mutex = &tdata->vars.var_active_lpt_mutex;
      pthread_mutex_lock(alpt_mutex);
      xlpt = tdata->vars.var_active_lpt;
      if (xlpt == lpt) xlpt = NULL;
      if (xlpt) lives_proc_thread_ref(xlpt);
      pthread_mutex_unlock(alpt_mutex);

      if (xlpt) {
        boolean ret = lives_proc_thread_request_resume(xlpt);
        lives_proc_thread_unref(xlpt);
        lives_proc_thread_unref(lpt);
        return ret;
      }

      pause_mutex = &tdata->vars.var_pause_mutex;
      if (!have_lock) pthread_mutex_lock(pause_mutex);

      if (!lives_proc_thread_is_paused(lpt)
          && !lives_proc_thread_sync_waiting(lpt)) {
        if (!have_lock) pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_unref(lpt);
        return FALSE;
      }

      lives_proc_thread_include_states(lpt, THRD_STATE_RESUME_REQUESTED);
      pcond = &tdata->vars.var_pcond;

      tdata->vars.var_sync_ready = TRUE;
      pthread_cond_signal(pcond);

      if (!have_lock) pthread_mutex_unlock(pause_mutex);

      lives_proc_thread_unref(lpt);
      return TRUE;
    }
    lives_proc_thread_unref(lpt);
  }
  return FALSE;
}


boolean lives_proc_thread_request_resume(lives_proc_thread_t lpt) {
  return _lives_proc_thread_request_resume(lpt, FALSE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_resume_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_RESUME_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_resume(lives_proc_thread_t self) {
  if (self) {
    lives_hook_stack_t **hstacks = lives_proc_thread_get_hook_stacks(self);
    // need to remove idling and unqueued in case this is an idle proc thread
    // which is paused / idling
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSED | THRD_STATE_UNQUEUED |
                                     THRD_STATE_IDLING | THRD_STATE_AUTO_PAUSED |
                                     THRD_STATE_RESUME_REQUESTED);
    lives_hooks_trigger(hstacks, RESUMING_HOOK);
  }
  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_pause(lives_proc_thread_t lpt) {
  lives_thread_data_t *tdata;
  pthread_mutex_t *alpt_mutex;
  lives_proc_thread_t xlpt;
  if (!lpt) return FALSE;

  lives_proc_thread_t plpt = lives_proc_thread_get_chain_prime(lpt);
  lives_closure_t *hook_closure = weed_get_voidptr_value(plpt, LIVES_LEAF_CLOSURE, NULL);
  if (hook_closure) {
    if (hook_closure->hook_type == DATA_READY_HOOK) {
      // test will become --> async_callbacks
      // for these, we cannot pause per se, or we would block the hook holder
      // instead we will set state paused | idling, which will cause the callback to skipped over
      lives_proc_thread_include_states(plpt, THRD_STATE_PAUSED | THRD_STATE_IDLING);
      lives_proc_thread_exclude_states(plpt, THRD_STATE_PAUSE_REQUESTED);
      return TRUE;
    }
  }
  tdata = lives_proc_thread_get_thread_data(lpt);
  if (tdata) {
    alpt_mutex = &tdata->vars.var_active_lpt_mutex;
    pthread_mutex_lock(alpt_mutex);
    xlpt = tdata->vars.var_active_lpt;
    if (xlpt == lpt) xlpt = NULL;
    if (xlpt) lives_proc_thread_ref(xlpt);
    pthread_mutex_unlock(alpt_mutex);

    if (xlpt) {
      boolean ret = lives_proc_thread_request_pause(xlpt);
      lives_proc_thread_unref(xlpt);
      lives_proc_thread_unref(lpt);
      return ret;
    }
  }

  if (!lives_proc_thread_get_pauseable(lpt)) return FALSE;

  if (!lives_proc_thread_is_paused(lpt))
    lives_proc_thread_include_states(lpt, THRD_STATE_PAUSE_REQUESTED);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_pause_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_PAUSE_REQUESTED)));
}


boolean _lives_proc_thread_pause(lives_proc_thread_t self, boolean have_lock) {
  // self function to be called during processing,
  // (pauseable proc_threads may pause when pause is requested)
  // for auto_pause proc_threads, this will be called on completion, unless cancelled
  // self pausing does not requeire proc_thread to be "pausable"
  //
  if (self) {
    pthread_mutex_t *pause_mutex = &(THREADVAR(pause_mutex));
    if (lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);
      if (have_lock) pthread_mutex_unlock(pause_mutex);
      lives_proc_thread_cancel(self);
      return FALSE;
    } else {
      pthread_cond_t *pcond = &(THREADVAR(pcond));
      if (!have_lock) pthread_mutex_lock(pause_mutex);

      // we will wait for this to be TRUE
      THREADVAR(sync_ready) = FALSE;
      lives_proc_thread_include_states(self, THRD_STATE_PAUSED);
      lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);

      while (!THREADVAR(sync_ready)) {
        pthread_cond_wait(pcond, pause_mutex);
      }
      lives_proc_thread_exclude_states(self, THRD_STATE_SYNC_WAITING);

      if (!have_lock) pthread_mutex_unlock(pause_mutex);

      lives_proc_thread_resume(self);
      if (lives_proc_thread_get_cancel_requested(self)) {
        if (have_lock) pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_cancel(self);
        return FALSE;
      }
      return TRUE;
    }
  }
  return FALSE;
}


boolean lives_proc_thread_pause(lives_proc_thread_t self)
{return _lives_proc_thread_pause(self, FALSE);}


static void lives_proc_thread_signalled(int sig, siginfo_t *si, void *uc) {
  // any proc__thread can be interrupted and end up here asynchronously
  // - thread establishes this as the signal handler for a specific signal
  // thread unblocks or blocks that signal
  // another thread can call pthread_kill providing the signal number, if other thread has the signal unblocked it
  // will end up here
  // TODO - check for deffered signals
  //
  // WE GOT SIGNAL !
  GET_PROC_THREAD_SELF(self);
  lives_thread_data_t *tdata = get_thread_data();
  int sig_act = tdata->vars.var_sig_act;

  switch (sig_act) {
  case SIG_ACT_IGNORE: return;
  case SIG_ACT_CANCEL:
    // cancel immediate, sent form other thread
    g_print("cancelling self !\n");
    lives_proc_thread_cancel(self);
    break;
  case SIG_ACT_ERROR: {
    int xerrno = weed_get_int_value(self, LIVES_LEAF_ERRNUM, NULL);
    int errsev = weed_get_int_value(self, LIVES_LEAF_ERRSEV, NULL);
    char *errmsg = weed_get_string_value(self, LIVES_LEAF_ERRMSG, NULL);
    lives_proc_thread_error(self, xerrno, errsev, errmsg ? "%s" : NULL, errmsg);
  }
  break;
  case SIG_ACT_HOLD:
    // cancel immediate, sent form other thread
    lives_proc_thread_pause(self);
    break;
  case SIG_ACT_REPORT: {
    // describe self
    if (tdata) {
      lives_proc_thread_t active = tdata->vars.var_active_lpt;
      g_print("Hello, I am thread;\n%s\n", get_thread_id(tdata->uid));
      if (active) {
        g_print("I am currently busy with proc_thread %p:\n", active);
        g_print("\t%s\n", lives_proc_thread_show_func_call(active));
        lpt_desc_state(active);
        if (tdata->vars.var_func_stack && tdata->vars.var_func_stack->list) {
          char *fname = lives_sync_list_pop(&tdata->vars.var_func_stack);
          if (fname)
            g_print("Last func entry recorded was %s\n", fname);
        }
      } else g_print("I am curently hanging out in the thread pool\n");
    }
  }
  break;
  case SIG_ACT_CMD: {
    while (tdata->vars.var_simple_cmd_list) {
      LiVESList *list = lives_sync_list_pop(&tdata->vars.var_simple_cmd_list);
      if (list) {
        lives_proc_thread_t lpt = (lives_proc_thread_t)list->data;
        // we can execute some simple tasks, but from a signal handler, only asyn safe functions may be used
        // this is the most basic method of running an lpt
        if (lpt) do_call(lpt);
        lives_list_free(list);
      }
    }
  }
  break;
  case SIG_ACT_CONTEXT: {
    g_print("will setctx\n");
    swapcontext(tdata->vars.var_context_ptr, tdata->vars.var_context_ptr2);
    g_print("after ctx\n");
  }
  break;
  default: break;
  }
}


LIVES_GLOBAL_INLINE void set_interrupt_action(int action) {
  // unblock just for calling thread (main thread)
  // TODO - we should ignore signal, as ther may be queued stale interrupts
  GET_PROC_THREAD_SELF(self);
  if (action != SIG_ACT_IGNORE) {
    THREADVAR(sig_act) = SIG_ACT_IGNORE;
    thrd_signal_unblock(LIVES_INTERRUPT_SIG, TRUE);
    THREADVAR(sig_act) = action;
    lives_proc_thread_include_states(self, THRD_OPT_CAN_INTERRUPT);
  } else {
    lives_proc_thread_exclude_states(self, THRD_OPT_CAN_INTERRUPT);
    THREADVAR(sig_act) = SIG_ACT_IGNORE;
    thrd_signal_block(LIVES_INTERRUPT_SIG, TRUE);
  }
}


lives_result_t lives_proc_thread_try_interrupt(lives_proc_thread_t lpt) {
  if (!lpt) return LIVES_RESULT_ERROR;
  pthread_t pth = lives_proc_thread_get_pthread(lpt);
  if (!pth) return LIVES_RESULT_INVALID;
  if (!lives_proc_thread_can_interrupt(lpt)) return LIVES_RESULT_FAIL;
  pthread_kill(pth, LIVES_INTERRUPT_SIG);
  return LIVES_RESULT_SUCCESS;
}


// calls pthread_cancel on underlying thread
// - cleanup function casues the thread to the normal post cleanup, so lives_proc_thread_join_*
// will work as normal (though the values returned will not be valid)
// however this should still be called, and the proc_thread freed as normal
// (except for auto / dontcare proc_threads)
// there is a small chance the cancellation could occur either before or after the function is called
LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel_immediate(lives_proc_thread_t lpt) {
  lives_thread_data_t *tdata = tdata = lives_proc_thread_get_thread_data(lpt);
  if (tdata) {
    pthread_t base_thread = tdata->thrd_self;
    if (lpt) lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES);
    if (base_thread) {
      pthread_cancel(base_thread);
      return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_START_TICKS, NULL) : 0;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_running(lives_proc_thread_t lpt) {
  if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt))
    check_pool_threads(FALSE);
  return (!lpt || (lives_proc_thread_has_states(lpt, THRD_STATE_RUNNING)));
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_get_chain_prime(lives_proc_thread_t lpt) {
  // get prime lpt for a chain. This is set when chain is created and is static
  // a opposed to proc_thread_prime which is prime lpt for a ssubordinate proc_thread
  lives_proc_thread_t plpt = NULL;
  if (lpt) {
    plpt = weed_get_plantptr_value(lpt, LIVES_LEAF_PRIME_LPT, NULL);
    if (!plpt) plpt = lives_thread_get_prime();
  }
  return plpt;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_get_chain_next(lives_proc_thread_t lpt) {
  return lpt ? weed_get_plantptr_value(lpt, LIVES_LEAF_FOLLOWER, NULL) : NULL;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_add_chain_next(lives_proc_thread_t lpt,
    lives_proc_thread_t follower) {
  if (lpt) {
    lives_proc_thread_t prime, xprime;
    // set attrs and prime
    uint64_t attrs = lives_proc_thread_get_attrs(lpt);
    lives_proc_thread_set_attrs(lpt, attrs | LIVES_THRDATTR_NXT_IMMEDIATE);
    weed_set_plantptr_value(lpt, LIVES_LEAF_FOLLOWER, follower);
    xprime = prime = lpt;
    while ((xprime = weed_get_plantptr_value(xprime, LIVES_LEAF_PRIME_LPT, NULL))) prime = xprime;
    weed_set_plantptr_value(follower, LIVES_LEAF_PRIME_LPT, prime);
    return follower;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_set_signalled(lives_proc_thread_t lpt, int signum, void *data) {
  if (!lpt) return FALSE;
  else {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (state_mutex) {
      lives_thread_data_t *mydata = (lives_thread_data_t *)data;
      uint64_t tstate;
      pthread_mutex_lock(state_mutex);
      if (mydata) mydata->signum = signum;
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | THRD_STATE_SIGNALLED);
      weed_set_voidptr_value(lpt, LIVES_LEAF_SIGNAL_DATA, data);
      pthread_mutex_unlock(state_mutex);
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_get_dispatcher(lives_proc_thread_t lpt) {
  // returns the proc_thread_ which queued (lpt)
  if (lpt) return weed_get_plantptr_value(lpt, LIVES_LEAF_DISPATCHER, NULL);
  return NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_sync_idx(int idx) {
  THREADVAR(sync_idx) = idx;
}


LIVES_GLOBAL_INLINE volatile int lives_proc_thread_get_sync_idx(lives_proc_thread_t lpt) {
  return LPT_THREADVAR_GET(lpt, sync_idx);
}


LIVES_GLOBAL_INLINE lives_result_t lives_proc_thread_sync_with_timeout(lives_proc_thread_t lpt,
    int sync_idx, int mm_op, int64_t timeout) {
  // wait for sync with other lpt
  // we want to avoid two situations - both threads are waiting for each other
  // one thread continues and leaves the other waiting
  // - we cannot lock both pause mutex AND opause mutex, or this could cause a deadlock
  // so we follow this sequence:
  // - read sync_idx of other
  // - set our sync_idx
  // - compare two sync_idx
  /// - if these match, we are synced, but other thread will not know this
  //  - if mismatch, we need to wait for other thread to see the match and wake us
  //  --
  // if matching:
  // - lock opause mutex, if other about to wait then it will block
  // else if it had the lock it will now be waiting

  // - check if other is waiting, if so resume it
  // -- on resuming it will check for sync_idx match, then reset its sync_idx
  //
  // --- if other thread is in a timedwait, it could timeout
  // --   if this happens it will check for match, then reset its sync_idx, the same as if it were resumed
  // - so after resuming we wait for other thread to reset its sync_idx

  //MSGMODE_ON(DEBUG);
  d_print_debug("start sync\n");
  if (sync_idx == 0) sync_idx = -1;
  if (lives_proc_thread_ref(lpt) > 1)  {
    d_print_debug("got ref\n");
    GET_PROC_THREAD_SELF(self);
    if (lpt != self) {
      volatile int osync_idx;
      boolean gotmatch = FALSE;
      if (lives_proc_thread_is_done(lpt, FALSE)) return LIVES_RESULT_FAIL;
      pthread_mutex_t *opause_mutex = LPT_THREADVAR_GETp(lpt, pause_mutex),
                       *pause_mutex = &(THREADVAR(pause_mutex));

      if (!opause_mutex) {
        d_print_debug("no pause mutex !! %p %p %p\n", lpt, mainw->def_lpt, get_thread_data_for_lpt(lpt));
        lives_proc_thread_unref(lpt);
        return LIVES_RESULT_ERROR;
      }

      d_print_debug("syncwith: check state of other (%p)\n", lpt);
      d_print_debug("get opause lock\n");

      // (A)
      pthread_mutex_lock(opause_mutex);
      d_print_debug("got opause lock\n");

      lives_proc_thread_set_sync_idx(sync_idx);
      osync_idx = lives_proc_thread_get_sync_idx(lpt);

      if (osync_idx == sync_idx) {
        gotmatch = TRUE;
        d_print_debug("syncwith: idx matches\n");
      } else d_print_debug("syncwith: no idx match, will wait\n");
      pthread_mutex_unlock(opause_mutex);

      // (B)
      pthread_mutex_lock(pause_mutex);

      d_print_debug("got PAuse mutex\n");

      if (!gotmatch) {
        // check again but now with lock - either other has not yet reached (A), will block at (A)
        // or has passed (B)
        // if it passed (B), we will now get a match,
        osync_idx = lives_proc_thread_get_sync_idx(lpt);
        if (osync_idx == sync_idx) {
          gotmatch = TRUE;
          d_print_debug("syncwith: now we DID get match\n");
        }
      }

      if (gotmatch) {
        // we got a match, then we wait for other to either pause / wait,
        // or to reset its sync-idx. While waitng we reset our sync_idx, so
        // if th eother notices this it can also reset its sync_idx
        lives_proc_thread_set_sync_idx(0);
        pthread_mutex_unlock(pause_mutex);
        //
        while (1) {
          if (!lives_proc_thread_get_sync_idx(lpt)) goto synced;
          pthread_mutex_lock(opause_mutex);
          if (lives_proc_thread_is_paused(lpt)) {
            d_print_debug("syncwith: other is paused, requesting resume\n");
            lives_proc_thread_set_sync_idx(sync_idx);
            _lives_proc_thread_request_resume(lpt, TRUE);
            d_print_debug("syncwith: waiting foR other to reset sync_idx\n");
          }
          pthread_mutex_unlock(opause_mutex);
          lives_microsleep;
          osync_idx = lives_proc_thread_get_sync_idx(lpt);
          if (!osync_idx) {
            d_print_debug("syncwith: other reset sync_idx, assume synced\n");
            //pthread_mutex_unlock(pause_mutex);
            goto synced;
          }
        }
      }
      // non-match, pause / wait
      if (!timeout) {
        d_print_debug("syncwith: pausing\n");
        _lives_proc_thread_pause(self, TRUE);
        pthread_mutex_unlock(pause_mutex);
        d_print_debug("syncwith: resumed, checking for match\n");
        if (lives_proc_thread_get_sync_idx(lpt) == sync_idx) {
          pthread_mutex_unlock(pause_mutex);
          goto synced;
        }
        d_print_debug("no match after resuming - wrong thread woek us ?\n");
        lives_proc_thread_set_sync_idx(0);
        pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_unref(lpt);
        return LIVES_RESULT_FAIL;
      } else {
        d_print_debug("syncwith: waiting\n");
        if (_lives_proc_thread_wait(self, timeout, TRUE)) {
          // timed out waiting
          d_print_debug("syncwith: timed out waiting\n");
          if (lives_proc_thread_get_sync_idx(lpt) == sync_idx) {
            pthread_mutex_unlock(pause_mutex);
            d_print_debug("syncwith: synced anyway\n");
            goto synced;
          }
          d_print_debug("syncwith: timed out, should retry");
          lives_proc_thread_set_sync_idx(0);
          pthread_mutex_unlock(pause_mutex);
          lives_proc_thread_unref(lpt);
          return LIVES_RESULT_FAIL;
        }
        d_print_debug("syncwith: resumed\n");
        if (lives_proc_thread_get_sync_idx(lpt) == sync_idx) {
          pthread_mutex_unlock(pause_mutex);
          goto synced;
        }
        d_print_debug("no match after resuming - wrong thread woek us ?\n");
        lives_proc_thread_set_sync_idx(0);
        pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_unref(lpt);
        //MSGMODE_OFF(DEBUG);
        return LIVES_RESULT_FAIL;
      }

synced:
      // if here, either - requested resume, or nrither paused, or thread resumed us
      // or timed out but sync_idx matched
      d_print_debug("syncwith: %p SYNCED with %p!!\n", self, lpt);
      pthread_mutex_lock(opause_mutex);
      // lock to ensure other read our sync_idx before unlocking
      lives_proc_thread_set_sync_idx(0);
      pthread_mutex_unlock(opause_mutex);
      lives_proc_thread_unref(lpt);
      d_print_debug("syncwith: DONE !!\n");
      //MSGMODE_OFF(DEBUG);
      return LIVES_RESULT_SUCCESS;
      /* mismatch: */
      /*   lives_proc_thread_error(self, 0, "sync_idx mismatch, wating for %d and found %d\n", sync_idx, osync_idx); */
      /*   lives_proc_thread_unref(lpt); */
      /*   return LIVES_RESULT_ERROR; */
    } else d_print_debug("sync with self !\n");

    lives_proc_thread_unref(lpt);
    //MSGMODE_OFF(DEBUG);
    return LIVES_RESULT_SUCCESS;
  }
  //MSGMODE_OFF(DEBUG);
  return LIVES_RESULT_FAIL;
}


LIVES_GLOBAL_INLINE lives_result_t lives_proc_thread_sync_with(lives_proc_thread_t lpt, int sync_idx, int mm_op) {
  return lives_proc_thread_sync_with_timeout(lpt, sync_idx, mm_op, 0);
}


// this function is safe to call even in case
// timeout is in seconds
// if lpt cannot be reffed or will be destroyed, we return LIVES_RESULT_INVALID
// if it had an error LIVES_RESULT_ERROR
// cancelled - LIVES_RESULT_FAIL
// if timed out, we return LiVES_RESULT_TIMEDOUT
lives_result_t lives_proc_thread_wait_done(lives_proc_thread_t lpt, double timeout, boolean ign_cancel) {
  if (lives_proc_thread_ref(lpt) > 1) {
    int count = 0;
    lives_result_t ret = LIVES_RESULT_SUCCESS;
    uint64_t attrs = lives_proc_thread_get_attrs(lpt);

    lives_proc_thread_set_attrs(lpt, attrs | LIVES_THRDATTR_NO_UNREF);
    if (timeout) timeout *= ONE_MILLION;
    if (lpt && lives_proc_thread_is_queued(lpt) && !lives_proc_thread_is_preparing(lpt))
      check_pool_threads(FALSE);
    while (timeout >= 0. && !lives_proc_thread_is_done(lpt, ign_cancel)) {
      // fg thread needs to service requests, otherwise the thread we are waiting on
      // may block waiting for a service call to be fulfilled, and causing a dealock ituation
      if (++count == 1000 && is_fg_thread()) {
        count = 0;
        fg_service_fulfill();
      }
      lives_microsleep;
      if (timeout) timeout--;
    }
    if (!lives_proc_thread_will_destroy(lpt)) {
      if (lives_proc_thread_had_error(lpt)) ret = LIVES_RESULT_ERROR;
      else if (lives_proc_thread_was_cancelled(lpt)) ret = LIVES_RESULT_FAIL;
      else if (!lives_proc_thread_is_done(lpt, ign_cancel)) ret = LIVES_RESULT_TIMEDOUT;
      lives_proc_thread_unref(lpt);
      return ret;
    }
  }
  return LIVES_RESULT_INVALID;
}


LIVES_LOCAL_INLINE boolean _lives_proc_thread_join(lives_proc_thread_t lpt, double timeout) {
  // version without a return value will free lpt
  boolean ret = FALSE;
  if (lives_proc_thread_ref(lpt) > 1) {
    lives_result_t res;
    res = lives_proc_thread_wait_done(lpt, timeout, TRUE);
    if (res == LIVES_RESULT_INVALID) return FALSE;
    if (res == LIVES_RESULT_SUCCESS) ret = TRUE;
    lives_proc_thread_unref(lpt);
  }
  return ret;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t lpt) {
  // version without a return value will free lpt, unless it is a hook stack
  // caller should ref the proc_thread if it wants to avoid this
  if (_lives_proc_thread_join(lpt, 0.) == LIVES_RESULT_SUCCESS
      && !lives_proc_thread_is_stacked(lpt)) lives_proc_thread_unref(lpt);
}


#define _join(lpt, stype) return (lpt && _lives_proc_thread_join(lpt, 0.) \
				  && weed_plant_has_leaf(lpt, _RV_)) 	\
  ? weed_get_##stype##_value(lpt, _RV_, NULL) : 0;

//      {lives_sleep_while_false(weed_plant_has_leaf(lpt, _RV_));

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

  if (lives_proc_thread_check_states(lpt, THRD_OPT_DONTCARE)) {
    lives_proc_thread_unref(lpt);
    return TRUE;
  }

  if (lives_proc_thread_is_stacked(lpt)) {
    lives_closure_t *cl = lives_proc_thread_get_closure(lpt);
    if (cl) {
      cl->flags |= HOOK_STATUS_REMOVE;
      lives_proc_thread_unref(lpt);
      return TRUE;
    }
  }

  if (lives_proc_thread_check_finished(lpt)) {
    // if the proc_thread already finished, we just unref it
    lives_proc_thread_unref(lpt);
    lives_proc_thread_unref(lpt);
    return FALSE;
  } else {
    if (lives_proc_thread_check_completed(lpt)) {
      if (!lives_proc_thread_will_destroy(lpt)) {
        uint64_t attrs = lives_proc_thread_get_attrs(lpt);
        lives_proc_thread_set_state(lpt, THRD_STATE_WILL_DESTROY);
        lives_sleep_while_false(lives_proc_thread_check_finished(lpt));
        lives_proc_thread_unref(lpt);
        if (!(attrs & LIVES_THRDATTR_NO_UNREF)) lives_proc_thread_unref(lpt);
      }
      return FALSE;
    }
  }
  if (weed_plant_has_leaf(lpt, _RV_)) weed_leaf_delete(lpt, _RV_);
  lives_proc_thread_exclude_states(lpt, THRD_OPT_NOTIFY);
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
// FINAL for a proc_thread will either be:
//
// the proc_thread was flagged DONTCARE, or has no monitored return
//	the state will first go to COMPLETED | WILL_DESTROY,
//  then if there a re no extern refs - DESTROYED and be freed
// the function will return FALSE, and lpt should not be used further in code (excpet for original caller to perhaps
// unref it it added a ref
//
//
// - IDLING - proc_thread was flagged as idlefunc, and the function returned TRUE
// - IDLING / PAUSED - proc_thread was flagged as idlefunc/pauseable, and the function returned TRUE
// - COMPLETED - in all other cases
// in theses cases the function returns TRUE
// - if the state is COMPLETED, then once all final processing on lpt has been done
//     set the FINISHED state before unreffing lpt to free it
// - other threads waiting for a thread to complete will get a ref on it, then wait for the FINISHED strate

// the reason for having both COMPLETED and FNISHED is this:
// if a proc_thread sets ERROR or CANCELLED state, this can cause it to finish processing early
// the state will then be COMPLETED | ERROR | CANCELLED accordingly
// this a caller thread can request cancel / dontcare, wait for the completed state
// and be sure the thread is exiting
//
// other callers can wait for the COMPLETED state, check if CANCEL or ERROR are present
//  - if so then the result is invalid, otherwise wait for FINSHED state and read the result
// (the thread can be cancelled after completing, in which case the result is still valid)
//
//  - other callers can add a ref, wait for the COMPLETED state, check if WILL_DESTROY is preseent, if so
//    avoid wating for FINSHED

//
// thus adding a hook callback for COMPLETED will work in all cases, except for idlefuncs
// where we can add a hook for IDLING
// after requeueing the idlefunc, the idling state will be removed
//
// state may be combined with: - unqueued (for idling), cancelled, error, timed_out, etc.
// paused is not a final state unless acompanied by idling,
//  - then proc_thread should be cancel_requested first, then resume_requested
// for cancel_immediate, there will be no final state, but thread_exit will be triggered
//
// if the state is CANELLED + COMPLETED + FINISHED + UNUQEUEUD (without paused or idling)
// - this means lpt was cancelled before being queued
//
//
// WARNING - if FALSE is returned then lpt will be unreffed
// to avoid this, if waiitng on the thread to return for examaple,
// set LIVES_THRDATTR_NO_UNREF beefore queueing it
static uint64_t lives_proc_thread_set_final_state(lives_proc_thread_t lpt) {
  uint32_t rtype;
  uint64_t state = lives_proc_thread_get_state(lpt);

  if (state & THRD_STATE_RUNNING)
    lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);
  state &= ~THRD_STATE_RUNNING;

  state |= THRD_STATE_COMPLETED;

  rtype = lives_proc_thread_get_rtype(lpt);

  if (!lives_proc_thread_is_stacked(lpt)
      && ((state & THRD_OPT_DONTCARE)
          || (rtype == WEED_SEED_VOID && !(state & THRD_OPT_NOTIFY)))) {
    // if dontcare, or there is no return type then we should unref the proc_thread
    // unless it is a hook callback, (then it stays in the stack - the equivalent would be
    // to add it as ONESHOT, to return FALSE from a REMOVE_IF_FALSE stack, or to remove manually)
    // call the COMPLETED hook, but with WILL DESTROY set
    state |= THRD_STATE_WILL_DESTROY;
  }
  // once a proc_thread reaches this state
  // provided DESTROYING is not set, it is guaranteed not to be unreffed automatically
  // this is the final  hook trigger, the following state is FINISHED

  // STATE CHANGE -> completed
  lives_proc_thread_include_states(lpt, state);
  // caller should set FINISHED state before unreffing
  return lives_proc_thread_get_state(lpt);
}


// get next lpt in chain (if apause or aqueue are set)
// if both are set, a "followup" must be supplied and it will be executed immediately
// in this case folloup cannot be self, but may be prime lpt
//
// if only one or other (apuse, aqueue) are set, the followup is always SELF.
// olpt is the prime proc_thread, lpt is the one just finished (can be == olpt)
// if lpt != olpt, lpt is unreffed here, removing the ref we added.
// if lpt is cancelled, caller must unref it
//
// if there is a nxtlpt be run, this will be reffed and returned. caller will check if this is cancelled
//
static lives_proc_thread_t next_in_chain(lives_proc_thread_t olpt, lives_proc_thread_t lpt) {
  lives_proc_thread_t nxtlpt = NULL;
  boolean debug = FALSE;
  boolean apause = FALSE, arq = FALSE;
  uint64_t attrs;

  attrs = lives_proc_thread_get_attrs(lpt);
  if (attrs & LIVES_THRDATTR_AUTO_REQUEUE) arq = TRUE;
  if (attrs & LIVES_THRDATTR_AUTO_PAUSE) apause = TRUE;

  if (!apause && !arq) {
    // if neither auto_pause nor auto_queue, just exit
    return NULL;
  }

  if (debug) g_print("running chain lpt 0 %d %d\n", apause, arq);

  if (apause && arq) {
    // run followup immed.
    nxtlpt = weed_get_plantptr_value(lpt, LIVES_LEAF_FOLLOWER, NULL);
    if (nxtlpt == lpt) nxtlpt = NULL;
    else if (nxtlpt && lives_proc_thread_ref(nxtlpt) < 2) nxtlpt = NULL;
    return nxtlpt;
  }

  // queue or pause - same lpt
  lives_proc_thread_include_states(olpt, THRD_STATE_IDLING);

  if (!apause) {
    // auto requeue, but not autopause - requeue self via normal queue
    // or we had autopause / autorequeue and nxtlpt was invalid
    // - state is IDLING, it will go to IDLING | COMPLETED when done
    // state won't go to finished unless cancelled, but we still call FINAL_HOOK
    // even though it wont get final state,
    // one  other thing we need to do, since we are requeuing the same proc_thread, it cannot
    // be deleted even if set to DONTCARE. So we add a 'hailmary' oneshot to completed hook
    // the effect will be to clear the DESTROYING flag bit, thus saving it from certain doom
    // we could also set attribute LIVES_THRDATTR_NO_UNREF, but that would be a permanent
    // flag change. we want to make it easy so when the lpt is not to be requeud any more it can just
    // clear any ATURREQUEU attribute.
    lives_proc_thread_add_hook(olpt, COMPLETED_HOOK, HOOK_OPT_ONESHOT, hailmary_other_lpt, olpt);
    lives_proc_thread_add_hook(olpt, FINISHED_HOOK, HOOK_OPT_ONESHOT, queue_other_lpt, olpt);
    return NULL;
  }

  // auto pause, but no auto requeue
  // function will wait for resume_request, and then run lives_proc_thread_resume()
  // and then re-run self

  lives_proc_thread_pause(lpt);
  if (debug) g_print("pause no chain lpt %p\n", olpt);
  return olpt;
}


static void *proc_thread_worker_func(void *args) {
  // this is the function which worker threads run when executing a llives_proc_thread which
  // has been queued

  // we have to keep track of various levels of proc_thread
  // there are 4 circumanstances to consider:
  // proc thread chains
  // proc threads queueing subordinates
  // proc_threads executing other proc_threads directly
  // (handled in lives_proc_thread_execute)
  //
  // prime - this is the proc thread in the work packet pick up by a pool thread
  // this is the proc thread created by create_proc_thread an then queued
  //
  // dispatcher - this is the proc_thread which queued the work. this will be another active
  // thread or it could have completed
  //
  // active - this begins as prime, but may change - for example when there are chained proc
  /// threads or when running a hook callback function
  //
  // we have situations where requests and states need to passed between proc_threads
  // for example, when we have a chain, and the prime lpt of the chain receives a cancel_request
  // this must be forwarded to the chain active
  //
  // if a proc thread queues another proc thread it is up to the dispatcher how to handle cancel
  // and pause reuqests, sometimes these will be forwarded othe times not

  // when a proc_thread queues another proc thread, the subordiante will run and
  // the dispatcher, on receiving a request for cancel, pause, or resume can decide whether to
  // forward that on
  //
  // likewise, when the state of a sub proc changes to error
  // it is up to dispatcher how to handle this. The sub proc will longjump back to here
  //
  //

  // we keep track of various levels of proc-thread:
  // - self is the active_lpt on entry (initially also prime lpt)
  // however, if a proc thread queues another proc thread,
  // the work packet is checked and we find the dispatcher of the packet
  // (ie. the thread which queud the work)
  // - since the caller may only have a reference to prime, reqeuests (pause, resum, cancel)
  // sent to prime are forwarded to all active lpts
  // the active lpts then may respond with paused / cancelled
  //
  // then prime will be the toplevel lpt active (i.e self) will switch to new lpt
  //
  if (args) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)args;
    lives_proc_thread_execute(lpt);
  }
  return args;
}


uint64_t lives_proc_thread_execute(lives_proc_thread_t lpt) {
  // run a proc thread directly
  // this can happen in several situations:
  // - a pool thread picked up a work packet which wrapped a proc_thread
  // - a thread has triggered hook callbacks
  // - the gui thread is servicing a bg thread request
  // - a thread decides to spontaneously run a (another) proc_thread

  // normally we just run the sole lpt, but we can also have chained lpts

  // we have to handle several situations carefully
  // - for chained lpts, requests (cancel, pause, resume) are sent to the chain prime
  //    abd forwarded to chain active, and the active state is relfected in prime state
  //
  // in the case of queued proc threads, the caller (dispatcher) runs the subordinate async
  // thus if it receives a request it shall forward it as necesssary
  //
  // if the proc_thread is being run directly (all other cases), it is run synchrounously
  // - any requests sent to a higher level proc_thread have to be passed down,
  //   the situation is similar for chained ptoc_threads, execpt that the prime proc_thread
  // is at the same level as its successors
  //
  // we have to keep track of proc_threads at 2 levels - the proc_thread level and pthread level
  //
  // pthread level:
  // when a pool thread gets work from the queue it  sets active_lpt for the pthread
  //
  //
  // proc_thread level
  // when a thread runs a proc_thread in the queue and sets it to active,
  // we set leaves to indicate the pthread and the pthread data
  // we want to do this ASAP, so proc_thread has a thread_data / pthread link.
  //
  // querying for proc_thread "self" always gets the proc_thread thread_data and returns
  // active_lpt, whilst query for prime gets prime_lpt
  //
  // when querying for other proc_threads, toplevel wil return prime from pthread thread data
  // and subord returns active_lpt ftom thread data
  //
  // when another proc_thread is run - either next in chain or subordinate
  // it gets a leaf "subordinate" pointing to new proc_thread
  // new proc_thread gets a leaf "parent" pointing back
  // now when a request is sent to a proc_thread. the request is also forwarded to subordinate,
  // where it may be forwareded again and so on
  // when a thread changes state, certain state bits are reflected in the parent
  // if the sub is paused, parent also pauses (so other threads can know to resume it)
  // if sub thread is blocked, waiting, etc this is also reflected
  // if the sub gets a major error or cancels itself, it will longjump back and the parent can
  // decide whether to cancel or error itself (for async, the dispatcher
  // needs to actively monitor for this), checking the final state of the sub, which is returned
  // from this function (for asynch, the final state is set by the pthread worker)
  //
  // when we run a new proc_threead here also we copy the thread data to the new proc_thread
  // we set pthread for the old lpt to zero, and set pthread for the new proc_thrread
  // - having state running and pthread == 0 sets virtual state suspended
  //  getting pthread for a suspended lpt will return -0, but getting thread_data will work
  // from there we can read active lpt, which is the originsl queued proc_thread
  // then following down the chain of subordinates we get to the real active proc_thread
  // getting proc_thread_active will always return the lowest level subordinate
  // whilst  getting proc_thread prime will return the prime lpt - this is waht external threads
  // will reference by.
  //
  // now get_thread_self will always return the active proc thrread

  // if running in a chain we do not exit when the proc_thread
  // returns, instead we may run the next. Chain proc threads get an additional leaf,
  // chain prime, which is set equal to parent
  // active_lpt is always the lowest level subordinate

  // quick ref:
  // pthread - sets prime_lpt and active_lpt  when picking up work packet
  // proc_thread gets tdata and pthread set when it becomes active
  // when becoming inactive, pthread is set to zero, but tdata remains
  // here we update active for the pthread
  // we set subordinate and parent and possibly chain_prime
  // proc_threads which have a subordinate leaf are :suspended"
  // proc_threads which are active but not prime get "subordinate" state
  // sending a pause request to a proc_thread will set it in the active thread instead
  // (if pausable), then paused state is reflected in all parents.
  // ident with resume
  // cancel reuqests - if receiver is cancelable each cancellable sub will get a cancel request
  // on return if proc_thread is a chain leader, if sub was cancelled it will also cancel
  // if sub had major error, this is copied to head
  // for non chained, return state should be for error / cancelled
  //
  // when the subordinate returns we get its state
  // if the sub is pausable, that state is added temporarily to this lpt
  //
  // if cancellable that state is not reflected. however if proc_thread that receives
  // the cancel request IS cancellable, then
  // cancel requests will be passed down to each subordiante, if subordinate is cancelable
  // it will get a cancel request, on return the caller should check the sub state
  // as returned from the function to see if the sub cancelled or had a major error
  // - it is recommended that if sub cancelled, this proc_thread cancel
  // itself even if not officially cancellable
  // recieve the cancel request
  // for chained proc threads, if subordnate cancelled or had error this is autoamtically acted
  // on by chain prime
  //
  // cancel / dontcare or dontcare only affect the target proc thread, the dontcare is not passed down
  // although cancel is
  //

  // the proc_thread passed to be executed will either be self (if running from the thread pool)
  // or we are running a subordinat proc_thread synchronously
  // we always have a proc_thread chain, even if it has only one member
  // the proc_thread passed as a parameter is the chain leader
  // if it is not "self", then its parent is self, and it is a subordinate of self
  // if we have more members in a chain, then their parent is chain_leader, and they are subordinates
  // of chain_leader.
  // if both self and chain_leader are cancellable, cancel requests are forwarded
  // to chain_leader' subordinate. IF subord cancels, then chain leader also cancels (unless it is self)
  // pause / resume requests area likewise passed down, state changes are reflected in self
  // If subordinate has a major error, this is replicated in chain_leader. If chain_leader != self,
  // we will return to caller, caller shall decide how to handle cancellation . error by its subordinate
  // if chain_leader == self, we return to the thread pool function
  // there is also a data "book" - this is preset in chain_leader by self (or by dispatcher in case of async)
  // this is passed from chain_leader to
  // subordinate, from subordiante to subordinate, then finally back to chain leader
  // book entries can be read by self after joining the chian leader, but before unreffing it.
  // (see SET_LPT_VALUE, GET_LPT_VALUE, SET_SELF_VALUE, etc)

  // can be called recursivly. First time is aleays async from the thread pool
  // (except for main thread, which is never queued)
  // subsequent calls will be sync, direct.

  // get current active proc_thread
  // this is set to queued lpt on first iteration, then will be lpt, active wa switched to it
  GET_PROC_THREAD_SELF(self);

  lives_proc_thread_t chain_leader = NULL;
  uint64_t state = 0;
  int sjval;
  jmp_buf env;

  if (lpt != self && lives_proc_thread_ref(lpt) < 2) return THRD_STATE_INVALID;

  sjval = sigsetjmp(env, 1);
  if (sjval) {
    // point of return for proc_threads if they cancel / error
    // since it is not clear what value lpt has now, get active_lpt again
    lives_proc_thread_t subord;
    lpt = lives_thread_get_active();
    weed_leaf_delete(lpt, LIVES_LEAF_LONGJMP);

    subord = weed_get_plantptr_value(self, "subord", NULL);
    if (subord != lpt) {
      chain_leader = weed_get_plantptr_value(lpt, "parent", NULL);
      if (subord != chain_leader) lives_abort("Error in proc_thread hierarchy");
    } else chain_leader = self;
    goto done;
  }

  if (lpt != self) {
    weed_set_plantptr_value(self, "subord", lpt);
    weed_set_plantptr_value(lpt, "parent", self);
    lives_thread_set_active(lpt);
    lives_proc_thread_share_book(self, lpt);
  }

  chain_leader = lpt;

  while (1) {
    lives_proc_thread_t nxtlpt;

    if (lives_proc_thread_should_cancel(lpt)) {
      if (!lives_proc_thread_was_cancelled(lpt))
        lives_proc_thread_cancel(lpt);
      break;
    }

    weed_set_voidptr_value(lpt, LIVES_LEAF_LONGJMP, &env);

    ///////////////////////
    call_funcsig(lpt);
    //if (lpt == mainw->debug_ptr) g_print("nrefss 555 = %d\n", lives_proc_thread_count_refs(self));

    ///////////////////

    weed_leaf_delete(lpt, LIVES_LEAF_LONGJMP);

    // for chained proc_threads, this returns next in chain
    nxtlpt = next_in_chain(chain_leader, lpt);
    if (!nxtlpt) break;

    weed_leaf_delete(lpt, "parent");

    weed_set_plantptr_value(chain_leader, "subord", nxtlpt);
    weed_set_plantptr_value(nxtlpt, "parent", chain_leader);

    lives_proc_thread_share_book(lpt, nxtlpt);

    lives_thread_set_active(nxtlpt);

    if (lpt != chain_leader) {
      lives_proc_thread_set_pthread(lpt, 0);
      lives_proc_thread_unref(lpt);
    }
    lpt = nxtlpt;
    //g_print("next in chain %s\n", lives_proc_thread_show_func_call(lpt));
  }

  if (!chain_leader) chain_leader = self;
  void *rloc = weed_get_voidptr_value(chain_leader, LIVES_LEAF_RETLOC, NULL);
  if (rloc) {
    uint32_t rtype = lives_proc_thread_get_rtype(chain_leader);
    if (rtype == WEED_SEED_STRING)
      *(char **)rloc = weed_get_string_value(chain_leader, _RV_, NULL);
    else _weed_leaf_get(chain_leader, _RV_, 0, rloc);
  }

  //if (lpt == mainw->debug_ptr) g_print("nrefss 1111 = %d\n", lives_proc_thread_count_refs(self));
  // pass data book back to self
  lives_proc_thread_share_book(lpt, self);

done:
  if (chain_leader && lpt != chain_leader) {
    weed_leaf_delete(chain_leader, "subord");
    weed_leaf_delete(lpt, "parent");

    if (lives_proc_thread_was_cancelled(lpt)) {
      lives_thread_set_active(chain_leader);
      lives_proc_thread_cancel(chain_leader);
    }

    if (lives_proc_thread_had_error(lpt))
      lives_proc_thread_error(chain_leader, lives_proc_thread_get_errnum(lpt),
                              lives_proc_thread_get_errsev(lpt), "%s",
                              lives_proc_thread_get_errmsg(lpt));

    lives_thread_set_active(chain_leader);

    // unref lpt
    // lives_proc_thread_unref(lpt);

    lpt = chain_leader;
  }

  if (lpt != self) {
    weed_leaf_delete(self, "subord");
    weed_leaf_delete(lpt, "parent");
    state = lives_proc_thread_set_final_state(lpt);
    lives_proc_thread_set_pthread(lpt, 0);
    if (!(state & THRD_STATE_DESTROYING)) {
      // notify successful completion
      if (state & THRD_STATE_COMPLETED)
        lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
    } else {
      // check attrs
      uint64_t attrs = lives_proc_thread_get_attrs(lpt);
      if (!(attrs & LIVES_THRDATTR_NO_UNREF))
        lives_proc_thread_unref(lpt);
    }
    lives_thread_set_active(self);
    lives_proc_thread_unref(lpt);
  } else state = lives_proc_thread_get_state(self);

  return state;
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
  GET_PROC_THREAD_SELF(self);
  thrd_work_t *mywork;
  uint64_t lpt_attrs = lives_proc_thread_get_attrs(lpt);
  uint64_t state = lives_proc_thread_get_state(lpt);

  //g_print("QUEUEING LPT  %p\n", lpt);

  // if we have BOTH cancelled and cancel_requested, this means the proc_thread was
  // cancelled BEFORE being queued, (if it were from a previous run, we would have only one or the other)
  // (this can be done even if the proc_thread is not explicitly 'cancellable',
  // otherwise, in some circumstances,  there would be no way to prevent a proc_thread from being queued)
  if (lives_proc_thread_should_cancel(lpt)) {
    if (!lives_proc_thread_was_cancelled(lpt))
      lives_proc_thread_include_states(lpt, THRD_STATE_CANCELLED);

    state = lives_proc_thread_set_final_state(lpt);
    if ((state & THRD_STATE_WILL_DESTROY) != THRD_STATE_WILL_DESTROY) {
      // notify successful completion
      if (state & THRD_STATE_COMPLETED)
        lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
    } else if (!(lpt_attrs & LIVES_THRDATTR_NO_UNREF)) lives_proc_thread_unref(lpt);
    return FALSE;
  }

  lpt_attrs |= attrs;

  weed_set_plantptr_value(lpt, LIVES_LEAF_DISPATCHER, self);

  /// tell the thread to clean up after itself [but it won't delete lpt]

  attrs |= (LIVES_THRDATTR_AUTODELETE | LIVES_THRDATTR_IS_PROC_THREAD);

  //if (!mainw->debug_ptr) mainw->debug_ptr = lpt;

  if (attrs & LIVES_THRDATTR_SET_CANCELLABLE)
    lives_proc_thread_set_cancellable(lpt);

  // STATE CHANGE -> unqueued / idling -> queued
  state &= ~(THRD_STATE_IDLING | THRD_STATE_COMPLETED | THRD_STATE_FINISHED
             | THRD_STATE_CANCELLED | THRD_STATE_UNQUEUED);

  state |= THRD_STATE_QUEUED;

  lives_proc_thread_set_state(lpt, state);

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
    lives_sleep_until_zero(mywork->flags & LIVES_THRDFLAG_WAIT_START);
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
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is waiting for sync");
  if (state & THRD_STATE_BLOCKED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is blocked");
  if (state & THRD_STATE_PAUSE_REQUESTED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "pause was requested");
  if (state & THRD_STATE_AUTO_PAUSED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is auto paused for set time");
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
  if (state & THRD_OPT_CANCELLABLE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is cancellable");
  if (state & THRD_OPT_CAN_INTERRUPT)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "accepts interrupt signal");
  if (state & THRD_OPT_PAUSEABLE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is pauseable");
  if (state & THRD_OPT_NOTIFY)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will wait when finish");
  if (state & THRD_OPT_DONTCARE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will exit when finished");
  if (state & THRD_BLOCK_HOOKS)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "hooks blocked");

  return fstr;
}


//////// worker thread pool //////////////////////////////////////////

///////// thread pool ////////////////////////
#ifndef VALGRIND_ON
#define MINPOOLTHREADS 8
#else
#define MINPOOLTHREADS 8
#endif
// rnpoolthreads is the reserved npoolthreads, npoolthreads is the ctual number, which may be lower because idle
// threads will time out and exit after a while
// npoolthreads is the number of available (free) poolthreads, we try to maintain this > ntasks
static volatile int npoolthreads, rnpoolthreads;
static pthread_t **poolthrds;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t all_tdata_rwlock;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile LiVESList *twork_list, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

static pthread_key_t tdata_key;
static LiVESList *all_tdatas = NULL;

static lives_threadvars_t dummy_vars;

lives_thread_data_t *get_thread_data_for_lpt(lives_proc_thread_t lpt) {
  // if lpt is unqueued, finished, completed, or was cancelled, will return NULL
  // otherwise will wait and only return when lpt has thread data (ie. is in running state)
  // should be called if the current state of lpt is not known
  lives_thread_data_t *tdata;
  if (!lpt || (lives_proc_thread_is_unqueued(lpt) && lpt != mainw->def_lpt)) return NULL;
  tdata = lives_proc_thread_get_thread_data(lpt);
  if (!tdata)
    // ignore subrd check, since that requires tdata !
    lives_microsleep_until_nonzero(lives_proc_thread_is_done(lpt, TRUE)
                                   || (tdata = lives_proc_thread_get_thread_data(lpt)));
  return tdata;
}


LIVES_GLOBAL_INLINE pthread_t lives_proc_thread_get_pthread(lives_proc_thread_t lpt) {
  pthread_t *pth = (pthread_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_PTHREAD_PTR, NULL);
  return pth ? *pth : 0;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_pthread(lives_proc_thread_t lpt, pthread_t pthread) {
  if (lpt) {
    pthread_t *pth = (pthread_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_PTHREAD_PTR, NULL);
    if (!pth) {
      pth = LIVES_CALLOC_SIZEOF(pthread_t, 1);
      weed_set_voidptr_value(lpt, LIVES_LEAF_PTHREAD_PTR, pth);
      weed_leaf_set_autofree(lpt, LIVES_LEAF_PTHREAD_PTR, TRUE);
    }
    // leave value unchanged if lpt was pushed to thread's se;f_stack
    else if (weed_leaf_get_flags(lpt, LIVES_LEAF_PTHREAD_PTR)
             & LIVES_FLAG_CONST_VALUE) return;
    *pth = pthread;
  }
}


// get active proc_thread for pthread_self()
lives_proc_thread_t lives_thread_get_active(void) {
  lives_thread_data_t *tdata = get_thread_data();
  if (tdata) {
    pthread_mutex_t *alpt_mutex = &tdata->vars.var_active_lpt_mutex;
    lives_proc_thread_t active_lpt;
    pthread_mutex_lock(alpt_mutex);
    active_lpt = tdata->vars.var_active_lpt;
    pthread_mutex_unlock(alpt_mutex);
    return active_lpt;
  }
  return NULL;
}

// get active (subordinate) proc_thread
lives_proc_thread_t lives_proc_thread_get_subord(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
    if (tdata) {
      pthread_mutex_t *alpt_mutex = &tdata->vars.var_active_lpt_mutex;
      lives_proc_thread_t active_lpt;
      pthread_mutex_lock(alpt_mutex);
      active_lpt = tdata->vars.var_active_lpt;
      pthread_mutex_unlock(alpt_mutex);
      return active_lpt != lpt ? active_lpt : NULL;
    }
  }
  return NULL;
}


// get prime (toplevel) proc_thread for pthread_self()
lives_proc_thread_t lives_thread_get_prime(void) {
  lives_thread_data_t *tdata = get_thread_data();
  return tdata ?  tdata->vars.var_prime_lpt : NULL;
}


// get prime (toplevel) proc_thread for lpt
LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_get_toplevel(lives_proc_thread_t lpt) {
  lives_thread_data_t *tdata = NULL;
  if (lpt) tdata = lives_proc_thread_get_thread_data(lpt);
  return tdata ? tdata->vars.var_prime_lpt : lpt;
}


void lives_thread_set_active(lives_proc_thread_t lpt) {
  // set current active proc_thread for pthread
  // set pthread for old lpt to 0
  // sets pthread for lpt to pthread_self()
  // sets thread_data for lpt
  lives_thread_data_t *tdata = get_thread_data();
  pthread_mutex_t *alpt_mutex = &tdata->vars.var_active_lpt_mutex;
  lives_proc_thread_t self;
  if (lpt && !tdata->vars.var_prime_lpt) tdata->vars.var_prime_lpt = lpt;
  pthread_mutex_lock(alpt_mutex);
  self = tdata->vars.var_active_lpt;
  if (lpt == self) {
    pthread_mutex_unlock(alpt_mutex);
    return;
  }

  // these values are mutable and show the current linked state
  tdata->vars.var_active_lpt = lpt;

  if (lpt) {
    lives_proc_thread_set_pthread(lpt, pthread_self());
    weed_set_voidptr_value(lpt, LIVES_LEAF_THREAD_DATA, tdata);
  }
  pthread_mutex_unlock(alpt_mutex);

  //g_print("THR switched %p to %p\n", self, lpt);
}


void lives_thread_set_prime(lives_proc_thread_t lpt) {
  lives_thread_data_t *tdata = get_thread_data();
  if (tdata) {
    lives_thread_set_active(lpt);
    tdata->vars.var_prime_lpt = lpt;
  }
}

static void lives_thread_data_destroy(void *data) {
  lives_thread_data_t *tdata = (lives_thread_data_t *)data;
  lives_hook_stack_t **hook_stacks = tdata->vars.var_hook_stacks;

  lives_list_free((LiVESList *)tdata->vars.var_trest_list);

  pthread_rwlock_wrlock(&all_tdata_rwlock);
  all_tdatas = lives_list_remove_data(all_tdatas, tdata, FALSE);
  pthread_rwlock_unlock(&all_tdata_rwlock);

  if (hook_stacks) {
    // this will force other lpts to remove their pointers to callbacks in our stacks
    lives_hooks_clear_all(hook_stacks, -N_HOOK_POINTS);
  }

  if (tdata->vars.var_guiloop) g_main_loop_unref(tdata->vars.var_guiloop);
  if (tdata->vars.var_guictx && tdata->vars.var_guictx != g_main_context_default())
    g_main_context_unref(tdata->vars.var_guictx);
  if (tdata->vars.var_guisource) g_source_unref(tdata->vars.var_guisource);

  lives_free(tdata);

#if USE_RPMALLOC
  if (rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_finalize(1);
  }
#endif
}

void pthread_cleanup_func(void *args) {
  // if the main_thread is ever cancelled by pthread_cancel, this will be triggered, and any hook callbacks
  // added, to THREAD_EXIT_HOOK will be triggered
  // this is also called after gtk_main() exits, thus on normal exit, any threads still running lpts
  // with hook callbacks in the main thread THREAD_EXIT_HOOK should flush their ext_cb lists, or manually remove the
  // callbacks, unless they need informing when this happens

  // this is called BEFORE thread data destroy

  lives_hooks_trigger(THREADVAR(hook_stacks), THREAD_EXIT_HOOK);
}


static int next_extern_tidx = 0;

static boolean thrdpool(void *arg);

static  pthread_once_t do_once = PTHREAD_ONCE_INIT;

static void make_pth_key(void) {
  (void)pthread_key_create(&tdata_key, lives_thread_data_destroy);
}


static void *_lives_thread_data_create(void *pslot_id) {
  lives_thread_data_t *tdata;
  int slot_id = LIVES_POINTER_TO_INT(pslot_id);
  pthread_once(&do_once, make_pth_key);
  tdata = pthread_getspecific(tdata_key);

  if (!tdata) {
#if USE_RPMALLOC
    // must be done before anything else
    if (!rpmalloc_is_thread_initialized())
      rpmalloc_thread_initialize();
#endif

    tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));

    (void)pthread_setspecific(tdata_key, tdata);

    tdata->uid = tdata->vars.var_uid = gen_unique_id();

#if IS_LINUX_GNU
    tdata->vars.var_tid = gettid();
#endif
#if IS_FREEBSD
    // also #include <pthread_np.h>
    tdata->vars.var_tid = pthread_getthreadid_np();
#endif

    pthread_mutex_init(&tdata->vars.var_active_lpt_mutex, NULL);

    for (int i = 0; i < N_HOOK_POINTS; i++) {
      tdata->vars.var_hook_stacks[i] =
        (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
      pthread_mutex_init(&tdata->vars.var_hook_stacks[i]->mutex, NULL);
      tdata->vars.var_hook_stacks[i]->flags |= STACK_NATIVE;
    }

    if (slot_id < 0) {
      if (pthread_equal(pthread_self(), capable->main_thread)) {
        tdata->thrd_type = THRD_TYPE_MAIN;
      } else tdata->thrd_type = THRD_TYPE_EXTERN;
    } else {
      tdata->thrd_type = THRD_TYPE_WORKER;
    }

    tdata->vars.var_thrd_type = tdata->thrd_type;
    tdata->vars.var_slot_id = tdata->slot_id = slot_id;

    pthread_rwlock_wrlock(&all_tdata_rwlock);
    all_tdatas = lives_list_prepend(all_tdatas, (livespointer)tdata);
    pthread_rwlock_unlock(&all_tdata_rwlock);

    tdata->vars.var_func_stack = NULL;

    tdata->vars.var_thrd_self = tdata->thrd_self = pthread_self();

    pthread_mutex_init(&tdata->vars.var_pause_mutex, NULL);

    pthread_cond_init(&tdata->vars.var_pcond, NULL);
    tdata->vars.var_sync_ready = TRUE;

    tdata->vars.var_loveliness = DEF_LOVELINESS;

    tdata->vars.var_pmsgmode = &prefs->msg_routing;

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

    thread_signal_establish(LIVES_INTERRUPT_SIG, lives_proc_thread_signalled);
    thrd_signal_block(LIVES_INTERRUPT_SIG, TRUE);

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

    //make_thrdattrs(tdata);
  }

  if (tdata->thrd_type >= THRD_TYPE_EXTERN) return tdata;

  if (tdata->thrd_type != THRD_TYPE_WORKER && tdata->vars.var_guictx
      == g_main_context_default()) return tdata;

  pthread_cleanup_push(pthread_cleanup_func, tdata);

  if (tdata->vars.var_guictx != g_main_context_default())
    g_main_context_iteration(tdata->vars.var_guictx, TRUE);

  pthread_cleanup_pop(1);

  /* while (1) { */
  /*   if (tdata->vars.var_guictx != g_main_context_default()) */
  /*     g_main_context_iteration(tdata->vars.var_guictx, TRUE); */
  /*   if (tdata->vars.var_guictx != g_main_context_default()) break; */

  /*   // we can do this - thread with main ctx, hands over main ctx, by setting our threadvar gictx to default */
  /*   // when we return from task, either completing or being ccancelled, we quit from here, ending the iteration */
  /*   // */
  /*   // force other thread to quit main loop, it will pop the old default ctx, find the loop and the source */
  /*   // (or create new source) */
  /*   // meanwhile: */
  /*   // push def context to thread def. */
  /*   // update loop and source */
  /*   // run the main loop */

  /*   lives_widget_context_push_thread_default(g_main_context_default()); */
  /*   tdata->vars.var_guiloop = NULL; */
  /*   tdata->vars.var_guisource = lives_idle_priority(fg_service_fulfill_cb, NULL); */
  /*   g_main_context_iteration(g_main_context_default(), TRUE); */
  /* } */

  return NULL;
}


lives_thread_data_t *get_thread_data(void) {
  // return pthread_specific data for pthread_self
  // in case no thread_data exists, we assume this is being called from an external thread, and we assign it
  // the next available (negative) slot_id
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  lives_thread_data_t *tdata;
  pthread_once(&do_once, make_pth_key);
  tdata = pthread_getspecific(tdata_key);
  if (!tdata) {
    pthread_mutex_lock(&mutex);
    next_extern_tidx--;
    pthread_mutex_unlock(&mutex);
    tdata = _lives_thread_data_create(LIVES_INT_TO_POINTER(next_extern_tidx));
  }
  return tdata;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars(void) {
  lives_thread_data_t *tdata = get_thread_data();
  return tdata ? &tdata->vars : NULL;
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_threadvars_bg_only(void) {
  // if caller is a bg thread, return its thread variables
  // otherwise we return the dummy variables. This ensures that we do not
  // alter a thread variable for the fg thread (by mistake or by choice)
  if (is_fg_thread()) return &dummy_vars;
  else {
    lives_thread_data_t *thrdat = get_thread_data();
    return thrdat ? &thrdat->vars : NULL;
  }
}


LIVES_GLOBAL_INLINE lives_threadvars_t *get_global_threadvars(void) {
  if (mainw && mainw->fg_tdata) return &mainw->fg_tdata->vars;
  return NULL;
}


lives_thread_data_t *get_thread_data_by_slot_id(int idx) {
  LiVESList *list = all_tdatas;
  pthread_rwlock_rdlock(&all_tdata_rwlock);
  for (; list; list = list->next) {
    if (!list->data) continue;
    if (((lives_thread_data_t *)list->data)->slot_id == idx) {
      pthread_rwlock_unlock(&all_tdata_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&all_tdata_rwlock);
  return NULL;
}


lives_thread_data_t *get_thread_data_by_uid(uint64_t uid) {
  LiVESList *list = all_tdatas;
  pthread_rwlock_rdlock(&all_tdata_rwlock);
  for (; list; list = list->next) {
    if (!list->data) continue;
    if (((lives_thread_data_t *)list->data)->uid == uid) {
      pthread_rwlock_unlock(&all_tdata_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&all_tdata_rwlock);
  return NULL;
}


lives_thread_data_t *get_thread_data_by_pthread(pthread_t pth) {
  LiVESList *list = all_tdatas;
  pthread_rwlock_rdlock(&all_tdata_rwlock);
  for (; list; list = list->next) {
    if (!list->data) continue;
    if (pthread_equal(((lives_thread_data_t *)list->data)->thrd_self, pth)) {
      pthread_rwlock_unlock(&all_tdata_rwlock);
      return list->data;
    }
  }
  pthread_rwlock_unlock(&all_tdata_rwlock);
  return NULL;
}


LIVES_GLOBAL_INLINE int isstck(void *ptr) {
  size_t stacksize = THREADVAR(stacksize);
  if (stacksize) {
    const void *stack = THREADVAR(stackaddr);
    if ((uintptr_t)ptr >= (uintptr_t)stack
        && (uintptr_t)ptr < (uintptr_t)stack + stacksize)
      return LIVES_RESULT_SUCCESS;
    return LIVES_RESULT_FAIL;
  }
  return LIVES_RESULT_ERROR;
}


LIVES_GLOBAL_INLINE lives_thread_data_t *lives_thread_data_create(void) {return get_thread_data();}


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

  // REFS++
  if (lives_proc_thread_ref((lpt = mywork->lpt)) < 2) lpt = NULL;

  /* if (lpt == mainw->debug_ptr) */
  /*   g_print("GOT PCUSTCOL\n"); */

  pthread_mutex_unlock(&twork_mutex);

  list->next = list->prev = NULL;

  if (lpt) {
    // if prime is NULL, also sets that
    lives_thread_set_active(lpt);
    // check if lpt will be destroyed or cancelled
    if (should_skip(lpt, mywork)) {
      if (lives_proc_thread_should_cancel(lpt)
          && !lives_proc_thread_was_cancelled(lpt))
        lives_proc_thread_include_states(lpt, THRD_STATE_CANCELLED);
      goto skip_over;
    }
  }

  mywork->busy = tdata->uid;

  // STATE change - queued - queued / preparing
  if (lpt) {
    lives_proc_thread_include_states(lpt, THRD_STATE_PREPARING);
    lives_proc_thread_exclude_states(lpt, THRD_STATE_QUEUED);
  }

  mywork->flags &= ~(LIVES_THRDFLAG_WAIT_START | LIVES_THRDFLAG_QUEUED_WAITING);

  // recheck afer updating flag states
  if (lpt && should_skip(lpt, mywork)) {
    if (lives_proc_thread_should_cancel(lpt)
        && !lives_proc_thread_was_cancelled(lpt))
      lives_proc_thread_include_states(lpt, THRD_STATE_CANCELLED);
    goto skip_over;
  }

  // RUN TASK
  mywork->flags |= LIVES_THRDFLAG_RUNNING;
  (*mywork->func)(mywork->arg);
  mywork->flags = (mywork->flags & ~LIVES_THRDFLAG_RUNNING) | LIVES_THRDFLAG_CONCLUDED;

  /* lives_widget_context_invoke_full(tdata->vars.var_guictx, mywork->attrs & LIVES_THRDATTR_PRIORITY */
  /*                                  ? LIVES_WIDGET_PRIORITY_HIGH - 100 : LIVES_WIDGET_PRIORITY_HIGH, */
  /*                                  widget_context_wrapper, mywork, NULL); */

  was_skipped = FALSE;

skip_over:

  if (lpt) {
    uint64_t state = lives_proc_thread_set_final_state(lpt);
    if ((state & THRD_STATE_WILL_DESTROY) != THRD_STATE_WILL_DESTROY) {
      // notify successful completion
      if (state & THRD_STATE_COMPLETED) {
        lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
      } else {
        g_print("WARNING - %p falied to get completed state !!\n", lpt);
      }
    } else {
      // check attrs
      uint64_t attrs = lives_proc_thread_get_attrs(lpt);
      if (!(attrs & LIVES_THRDATTR_NO_UNREF)) lives_proc_thread_unref(lpt);
      //g_print("Will destroy %p\n", lpt);
    }
    // also sets 'active'
    lives_thread_set_prime(NULL);

    //g_print("nrefss ++++ = %d %p\n", lives_proc_thread_count_refs(lpt), lpt);

    // should have a ref on this
    lives_proc_thread_unref(lpt);
  }

  mainw->debug_ptr = NULL;

  // clear all hook stacks for the thread (self hooks)
  for (int i = N_GLOBAL_HOOKS + 1; i < N_HOOK_POINTS; i++) {
    lives_hooks_clear(tdata->vars.var_hook_stacks, i);
  }

  pthread_mutex_lock(&twork_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_mutex);

  if (mywork->flags & LIVES_THRDFLAG_AUTODELETE) {
    lives_thread_free((lives_thread_t *)list);
  } else {
    if (was_skipped) mywork->skipped = TRUE;
    else mywork->done = tdata->uid;
  }
#if USE_RPMALLOC
  rpmalloc_thread_collect();
#endif
  return TRUE;
}


#define POOL_TIMEOUT_SEC 200

static boolean thrdpool(void *arg) {
  static struct timespec ts;
  boolean skip_wait = TRUE;
  int rc;
  lives_thread_data_t *tdata = (lives_thread_data_t *)arg;

  while (!threads_die) {
    if (!skip_wait) {
      int lifetime = POOL_TIMEOUT_SEC + fastrand_int(30);
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
#ifndef VALGRIND_ON
      if (rc == ETIMEDOUT) {
        // if the thread is waiting around doing nothing, and there are no tasks waitng,
        // exit, maybe free up some resources
        if (!pthread_mutex_trylock(&pool_mutex)) {
          if (!pthread_mutex_trylock(&twork_mutex)) {
            if (ntasks < npoolthreads) {
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
#endif
    }
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
  pthread_rwlock_init(&all_tdata_rwlock, NULL);
  rnpoolthreads = npoolthreads = MINPOOLTHREADS;
  if (mainw->debug) rnpoolthreads = npoolthreads = 0;
  if (prefs->nfx_threads > npoolthreads) rnpoolthreads = npoolthreads = prefs->nfx_threads;
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_list = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, _lives_thread_data_create, LIVES_INT_TO_POINTER(i));
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

#define POOL_CHK_THRESH (5. * TICKS_PER_SECOND_DBL)

void check_pool_threads(boolean important) {
  static ticks_t last_check_ticks = 0;
  if (!important && mainw->wall_ticks - last_check_ticks < POOL_CHK_THRESH) return;
  last_check_ticks = mainw->wall_ticks;
  pthread_mutex_lock(&pool_mutex);

  while (ntasks >= npoolthreads && npoolthreads < rnpoolthreads) {
    for (int i = 0; i < rnpoolthreads; i++) {
      if (poolthrds[i]) continue;
      // relaunch thread, npoolthreads ---> rnpoolthreads
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      npoolthreads++;
      //pthread_mutex_unlock(&twork_mutex);
      pthread_mutex_unlock(&pool_mutex);
      pthread_create(poolthrds[i], NULL, _lives_thread_data_create, LIVES_INT_TO_POINTER(i));
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
      pthread_mutex_lock(&pool_mutex);
      //pthread_mutex_lock(&twork_mutex);
      break;
    }
  }

  if (ntasks <= rnpoolthreads) {
    for (int i = 0; i < ntasks && i < rnpoolthreads; i++) {
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    //pthread_mutex_unlock(&twork_mutex);
  } else {
    // we need more threads to service all tasks
    int extrs = MAX(MINPOOLTHREADS, ntasks - rnpoolthreads);
    //pthread_mutex_unlock(&twork_mutex);
    poolthrds =
      (pthread_t **)lives_realloc(poolthrds, (rnpoolthreads + extrs) * sizeof(pthread_t *));
    for (int i = rnpoolthreads; i < rnpoolthreads + extrs; i++) {
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, _lives_thread_data_create, LIVES_INT_TO_POINTER(i));
    }
    rnpoolthreads += extrs;
    npoolthreads = rnpoolthreads;
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
  }
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
    if (lpt == mainw->debug_ptr)
      if (attrs & LIVES_THRDATTR_IGNORE_SYNCPTS) {
        work->flags |= LIVES_THRDFLAG_IGNORE_SYNCPTS;
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

  pthread_mutex_lock(&twork_mutex);
  if (lpt && lives_proc_thread_should_cancel(lpt)) {
    lives_proc_thread_cancel(lpt);
    //ooops !!
    lives_thread_free(list);
    /////
    pthread_mutex_unlock(&twork_mutex);
    return NULL;
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

  check_pool_threads(TRUE);

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
    if (!task->busy) check_pool_threads(FALSE);
    lives_microsleep_until_nonzero(task->done);
  }

  nthrd = task->done;

  // thread has been joined, so now it can be freed
  task->flags &= ~LIVES_THRDFLAG_NOFREE_LIST;
  lives_thread_free(thread);

#if USE_RPMALLOC
  // free up some thread memory
  if (rpmalloc_is_thread_initialized())
    rpmalloc_thread_collect();
#endif
  return nthrd;
}


LIVES_GLOBAL_INLINE uint64_t lives_thread_done(lives_thread_t *thrd) {
  thrd_work_t *task = (thrd_work_t *)thrd->data;
  if (!task) return TRUE;
  return task->done;
}


///////////// refcounting ///////////////

static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

LIVES_GLOBAL_INLINE boolean check_refcnt_init(lives_refcounter_t *refcount) {
  if (refcount) {
    if (!refcount->mutex_inited) {
      // there is a reace condition here
      // - we do the init, but before get the lock, another thread reaches this point
      // it will init the mutex again
      pthread_mutex_lock(&init_mutex);
      if (!refcount->mutex_inited) {
        pthread_mutex_init(&refcount->mutex, NULL);
        refcount->mutex_inited = TRUE;
        pthread_mutex_lock(&refcount->mutex);
        refcount->count = 1;
        pthread_mutex_unlock(&refcount->mutex);
      }
      pthread_mutex_unlock(&init_mutex);
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
    else BREAK_ME("double unref");
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
  // if plant does not have a refcounter, we add one then decrement it
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) {
      weed_add_refcounter(plant);
      refcnt = (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    }
    return refcount_dec(refcnt);
  }
  return -1;
}


LIVES_GLOBAL_INLINE int weed_refcount_query(weed_plant_t *plant) {
  // query number of refs for plant
  if (plant) {
    lives_refcounter_t *refcnt =
      (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    if (!refcnt) return 1;
    return refcount_query(refcnt);
  }
  return -1;
}


LIVES_GLOBAL_INLINE lives_refcounter_t *weed_add_refcounter(weed_plant_t *plant) {
  // if plant does not have a refcounter, add one with refcount initialised to 1
  lives_refcounter_t *refcount = NULL;
  if (plant) {
    if (weed_plant_has_leaf(plant, LIVES_LEAF_REFCOUNTER))
      refcount = (lives_refcounter_t *)weed_get_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, NULL);
    else {
      refcount = (lives_refcounter_t *)lives_calloc(1, sizeof(lives_refcounter_t));
      if (refcount) {
        weed_set_voidptr_value(plant, LIVES_LEAF_REFCOUNTER, refcount);
        weed_leaf_set_autofree(plant, LIVES_LEAF_REFCOUNTER, TRUE);
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


void thread_stackdump(void) {
  const void *stackaddr = THREADVAR(stackaddr);
  size_t stacksize = THREADVAR(stacksize);
  g_print("thread %d stackaddr %p wiith size %ld\n", THREADVAR(slot_id), stackaddr, stacksize);
  for (size_t i = 0; i < stacksize; i++) g_print("%c", ((char *)stackaddr)[i]);
  g_print("\n\n");
}


char *get_thread_id(uint64_t uid) {
  char *tnum = NULL;
  lives_thread_data_t *tdata = get_thread_data_by_uid(uid);
  if (tdata) {
#if IS_LINUX_GNU
    tnum = lives_strdup_printf("uid: 0x%lx (Thread 0x%lx (LWP %d)) ",
                               tdata->uid, tdata->vars.var_thrd_self, tdata->vars.var_tid);
#else
    tnum = lives_strdup_printf("uid: 0x%lx (Thread 0x%lx) ",
                               tdata->uid, tdata->vars.var_thrd_self);
#endif
  } else tnum = lives_strdup(_("Unknown thread "));
  return tnum;
}


LiVESList *filter_unknown_threads(LiVESList * allthrds) {
#if IS_LINUX_GNU
  int known = 0, unknown = 0;
  char *tmp = get_threadstats();
  g_print("Filtering known threads from lit\n");
  g_print("Known threads:\n%s\n", tmp);
  lives_free(tmp);
  pthread_rwlock_rdlock(&all_tdata_rwlock);
  for (LiVESList *list = all_tdatas; list; list = list->next) {
    lives_thread_data_t *tdata  = (lives_thread_data_t *)list->data;
    if (tdata) {
      for (LiVESList *xlist = allthrds; xlist; xlist = xlist->next) {
        if (tdata->vars.var_tid == LIVES_POINTER_TO_INT(xlist->data)) {
          g_printerr("Known: %d\n", LIVES_POINTER_TO_INT(xlist->data));
          allthrds = lives_list_remove_node(allthrds, xlist, FALSE);
          known++;
          break;
        }
      }
    }
  }
  pthread_rwlock_unlock(&all_tdata_rwlock);

  for (LiVESList *xlist = allthrds; xlist; xlist = xlist->next) {
    unknown++;
    g_printerr("\nFound unknow thread %d (LWP %d))",
               unknown, LIVES_POINTER_TO_INT(xlist->data));
  }
  g_print("\ntotal threads %d. Known %d, unknown %d\n", known + unknown, known, unknown);
#endif
  return allthrds;
}

static lives_result_t print_function(void *data) {
  const char *funcdets = (const char *)data;
  lives_printerr("%s\n", funcdets);
  return LIVES_RESULT_FAIL;
}


char *get_threadstats(void) {
  int totthreads = 0, actthreads = 0;
  char *msg = NULL;
  pthread_rwlock_rdlock(&all_tdata_rwlock);
  g_printerr("\nThreads current state\n");
  for (LiVESList *list = all_tdatas; list; list = list->next) {
    char *notes = NULL, *tnum;
    lives_thread_data_t *tdata  = (lives_thread_data_t *)list->data;
    if (tdata) {
      lives_proc_thread_t prime_lpt, active_lpt, nxtlpt;
      char *tmp;
      ticks_t qtime, sytime, ptime;
      totthreads++;

      if (pthread_equal(tdata->thrd_self, capable->gui_thread)) notes = lives_strdup("GUI thread");
      else if (tdata->thrd_type >= THRD_TYPE_EXTERN) notes = lives_strdup("External");
      tnum = get_thread_id(tdata->vars.var_uid);
      g_printerr("\nThread %d %s(%s):\nType: %s\n", tdata->slot_id, tnum,
                 notes ? notes : "-", tdata->vars.var_origin);
      lives_free(tnum);
      pthread_mutex_t *alpt_mutex = &tdata->vars.var_active_lpt_mutex;
      pthread_mutex_lock(alpt_mutex);
      active_lpt = tdata->vars.var_active_lpt;
      if (!active_lpt) g_printerr("Idling in threadpool\n");
      else {
        actthreads++;
        g_printerr("Running proc_thread, ");
        prime_lpt = tdata->vars.var_prime_lpt;
        nxtlpt = weed_get_plantptr_value(prime_lpt, LIVES_LEAF_FOLLOWER, NULL);
        if (nxtlpt) {
          if (active_lpt == prime_lpt) {
            g_print("proc_thread is chain leader\n");
          } else {
            g_print("proc_thread is part of a chain\n");
            while (prime_lpt != active_lpt) {
              tmp = lives_proc_thread_show_func_call(prime_lpt);
              g_printerr("\t%s\n", tmp);
              lives_free(tmp);
              prime_lpt = nxtlpt;
              nxtlpt = weed_get_plantptr_value(prime_lpt, LIVES_LEAF_FOLLOWER, NULL);
            }
          }
        }
        while (1) {
          tmp = lives_proc_thread_show_func_call(prime_lpt);
          g_printerr("\t%s%s\n", prime_lpt == active_lpt ? "Active: " : "", tmp);
          lives_free(tmp);
          if (prime_lpt == active_lpt) {
            if (tdata->vars.var_func_stack) {
              lives_sync_list_find(tdata->vars.var_func_stack, print_function);
            } else lives_printerr("Running unknown function\n");
          }
          if (!nxtlpt) break;
          prime_lpt = nxtlpt;
          nxtlpt = weed_get_plantptr_value(prime_lpt, LIVES_LEAF_FOLLOWER, NULL);
        }

        lpt_desc_state(active_lpt);
        pthread_mutex_unlock(alpt_mutex);

        g_printerr("Loveliness %.2f\n", tdata->vars.var_loveliness);
        qtime = lives_proc_thread_get_timing_info(active_lpt, TIME_TOT_QUEUE);
        sytime = lives_proc_thread_get_timing_info(active_lpt, TIME_TOT_SYNC_START);
        ptime = lives_proc_thread_get_timing_info(active_lpt, TIME_TOT_PROC);
        g_printerr("\n[queue wait time %.4f usec, sync_wait time %.4f usec, "
                   "proc time %.4f usec]\n\n",
                   (double)qtime / (double)USEC_TO_TICKS,
                   (double)sytime / (double)USEC_TO_TICKS,
                   (double)ptime / (double)USEC_TO_TICKS);
      }
      pthread_mutex_unlock(alpt_mutex);
    }
  }
  pthread_rwlock_unlock(&all_tdata_rwlock);
  msg = lives_strdup_printf("Total threads in use: %d, (%d poolhtreads, %d other), "
                            "active threads %d\n\n", totthreads, rnpoolthreads,
                            totthreads - rnpoolthreads, actthreads);
  return msg;
}
