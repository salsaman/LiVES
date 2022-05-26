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
	- NO_GUI:    - this has no functional effect, but will set a flag in the worker thread's environment, and this may be
			checked for in the function code and used to bypass graphical updates
	- INHERIT_HOOKS - any NON_GLOBAL hooks set in the calling thread will be passed to the proc_thread
			which will have the same effect as if the hooks had been set for the bg thread
			after the thread is created, the hooks will be cleared (nullified) for caller
	- FG_THREAD: - create the thread but do not run it; main_thread_execute() uses this to create something that
	can be passed to the main thread.

   - the only requirements are to call lives_proc_thread_create() which will generate a lives_proc_thread_t and run it,
   and then (depending on the return_type parameter, call one of the lives_proc_thread_join_*() functions

   (see that function for more comments)
*/

typedef weed_plantptr_t lives_proc_thread_t;

static funcsig_t make_funcsig(lives_proc_thread_t func_info);

static void run_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr);

LIVES_GLOBAL_INLINE void lives_proc_thread_free(lives_proc_thread_t lpt) {
  pthread_mutex_t *state_mutex
    = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
  lives_nanosleep_while_false(lives_proc_thread_check_finished(lpt));
  if (state_mutex) lives_free(state_mutex);
  THREADVAR(tinfo) = NULL;
  weed_plant_free(lpt);
}


void lives_plant_params_from_vargs(weed_plant_t *info, lives_funcptr_t func, int return_type,
                                   const char *args_fmt, va_list xargs) {
  const char *c;
  char *param_prefix = weed_get_string_value(info, "param_prefix", NULL);
  int p = 0;
  weed_set_funcptr_value(info, LIVES_LEAF_THREADFUNC, func);
  if (!param_prefix) param_prefix = lives_strdup(LIVES_LEAF_THREAD_PARAM);
  for (c = args_fmt; *c; c++) {
    char *pkey = lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, p++);
    switch (*c) {
    case 'i': weed_set_int_value(info, pkey, va_arg(xargs, int)); break;
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

  // set the type of the return_value, but not the return_value itself yet
  if (return_type > 0)
    weed_leaf_set(info, _RV_, return_type, 0, NULL);

  weed_set_int64_value(info, LIVES_LEAF_FUNCSIG, make_funcsig(info));
  lives_free(param_prefix);
}


static lives_proc_thread_t lives_proc_thread_prepare(lives_thread_attr_t attr, lives_proc_thread_t thread_info,
    uint32_t return_type) {
  if (thread_info) {
    if (!weed_plant_has_leaf(thread_info, LIVES_LEAF_STATE_MUTEX)) {
      pthread_mutex_t *state_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(state_mutex, NULL);
      weed_set_voidptr_value(thread_info, LIVES_LEAF_STATE_MUTEX, state_mutex);
    }

    if (return_type)lives_proc_thread_set_state(thread_info, THRD_OPT_NOTIFY);

    if (!(attr & LIVES_THRDATTR_FG_THREAD)) {
      if (attr & LIVES_THRDATTR_NOTE_STTIME) weed_set_int64_value(thread_info, LIVES_LEAF_START_TICKS,
            lives_get_current_ticks());
      run_proc_thread(thread_info, attr);
      if (!return_type) return NULL;
    }
  }
  return thread_info;
}


lives_proc_thread_t lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs) {
  lives_proc_thread_t thread_info = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  lives_plant_params_from_vargs(thread_info, func, return_type, args_fmt, xargs);
  return lives_proc_thread_prepare(attr, thread_info, return_type);
}


lives_proc_thread_t lives_proc_thread_create_from_funcinst(lives_thread_attr_t attr, lives_funcinst_t *finst) {
  // for future use, eg. finst = create_funcinst(fdef, ret_loc, args...)
  // proc_thread = -this-(finst)
  if (finst) {
    lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL);
    return lives_proc_thread_prepare(attr, (lives_proc_thread_t)finst, fdef->return_type);
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
  va_list xargs;
  va_start(xargs, args_fmt);
  lpt = lives_proc_thread_create_vargs(attr, func, return_type, args_fmt, xargs);
  va_end(xargs);
  return lpt;
}


lives_sigdata_t *lives_sigdata_new(lives_proc_thread_t lpt, boolean is_timer) {
  lives_sigdata_t *sigdata = (lives_sigdata_t *)lives_calloc(1, sizeof(lives_sigdata_t));
  sigdata->proc = lpt;
  sigdata->is_timer = is_timer;
  return sigdata;
}


// creates a proc_thread as above, then waits for timeout. If the proc_thread does not complete before timer expires,
// since we do block here while waiting, we also launch the govenor_loop to service fg requests from the thread
// we also need to update the GUI main loop because governor_loop may add itself as an idle func
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
  lives_sigdata_t *sigdata;
  ticks_t xtimeout = 1;
  boolean tres;
  boolean govrun = FALSE;
  lives_cancel_t cancel = CANCEL_NONE;
  int xreturn_type = return_type;

  if (xreturn_type == 0) xreturn_type--;

  attr |= LIVES_THRDATTR_WAIT_SYNC;

  va_start(xargs, args_fmt);
  lpt = lives_proc_thread_create_vargs(attr, func, xreturn_type, args_fmt, xargs);
  va_end(xargs);

  mainw->cancelled = CANCEL_NONE;
  lives_widget_context_update();

  sigdata = lives_sigdata_new(lpt, TRUE);
  alarm_handle = sigdata->alarm_handle = lives_alarm_set(timeout);

  if (is_fg_thread()) {
    govrun = TRUE;
    tres = governor_loop(sigdata);
  } else {
    run_proc_thread(lpt, 0);
    lives_proc_thread_sync_ready(lpt);
  }

  if (lives_proc_thread_check_states(lpt, THRD_STATE_BUSY) == THRD_STATE_BUSY) {
    // thread MUST unavoidably block; stop the timer (e.g showing a UI)
    // user or other condition may set cancelled
    if ((cancel = mainw->cancelled)) goto thrd_done;
    lives_alarm_reset(alarm_handle, timeout);
  }

  while (!(tres = lives_proc_thread_check_finished(lpt))
         && (timeout == 0 || (xtimeout = lives_alarm_check(alarm_handle)) > 0)) {
    lives_nanosleep(LIVES_QUICK_NAP);

    if (is_fg_thread()) {
      // allow governor_loop to run its idle func
      govrun = will_gov_run();
      if (!govrun) governor_loop(NULL);
      else lives_widget_context_update();
    }

    if (lives_proc_thread_check_states(lpt, THRD_STATE_BUSY) == THRD_STATE_BUSY) {
      // thread MUST unavoidably block; stop the timer (e.g showing a UI)
      // user or other condition may set cancelled
      if ((cancel = mainw->cancelled) != CANCEL_NONE) break;
      lives_alarm_reset(alarm_handle, timeout);
    }
  }

thrd_done:
  lives_free(sigdata);
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
  if (!mainw || !capable || !capable->gui_thread || mainw->go_away || !mainw->is_ready) return TRUE;
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
      if (has_lpttorun()) {
        if (!will_gov_run()) governor_loop(NULL);
        else lives_widget_context_update();
      }
    }
  } else pthread_mutex_lock(mutex);
}


LIVES_LOCAL_INLINE void add_to_deferral_stack(uint64_t xtraflags,  lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_SINGLE_SHOT | HOOK_CB_FG_THREAD | (xtraflags & HOOK_UNIQUE_DATA);
  lives_hook_append(THREADVAR(hook_closures), THREAD_INTERNAL_HOOK,
                    filtflags, NULL, (void *)lpt);
}


LIVES_LOCAL_INLINE void add_to_fg_deferral_stack(uint64_t xtraflags,  lives_proc_thread_t lpt) {
  uint64_t filtflags = HOOK_CB_SINGLE_SHOT | HOOK_CB_FG_THREAD | (xtraflags & HOOK_UNIQUE_DATA);
  lives_hook_append(FG_THREADVAR(hook_closures), THREAD_INTERNAL_HOOK,
                    filtflags, NULL, (void *)lpt);
}


boolean main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...) {
  // this function exists because GTK+ can only run certain functions in the thread which called gtk_main
  // amy other function can be called via this, if the main thread calls it then it will simply run the target itself
  // for other threads, the main thread will run the function as a fg_service.
  // however care must be taken since fg_service cannot run another fg_service
  lives_proc_thread_t lpt;
  va_list xargs;
  boolean is_fg_service = FALSE;
  va_start(xargs, args_fmt);
  lpt = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, func, return_type, args_fmt, xargs);
  va_end(xargs);
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
      if (FG_THREADVAR(fg_service)) {
        add_to_fg_deferral_stack(FG_THREADVAR(hook_flag_hints), lpt);
      } else {
        // will call fg_run_func() indirectly, so no need to call lives_proc_thread_free
        fg_service_call(lpt, retval);
        // some functions may have been deferred, since we cannot stack multiple fg service calls
        lives_hooks_trigger(NULL, THREADVAR(hook_closures), THREAD_INTERNAL_HOOK);
      }
    } else {
      // lpt here is a freshly created proc_thread, it will be stored and then
      add_to_deferral_stack(THREADVAR(hook_flag_hints), lpt);
      return FALSE;
    }
  }

  THREADVAR(fg_service) = FALSE;
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
  int nparms = get_funcsig_nparms(sig);

  if (thread_info) info = thread_info;
  else info = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);

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

  switch (nparms) {
  case 0: //FUNCSIG_VOID:
    switch (ret_type) {
    case WEED_SEED_INT64: CALL_0(info, int64); break;
    default: CALL_VOID_0(); break;
    }
    break;

  case 1:
    switch (sig) {
    case FUNCSIG_INT: {
      int p0;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_1(info, boolean, int); break;
      default: CALL_VOID_1(info, int); break;
      }
    } break;
    case FUNCSIG_INT64: {
      int64_t p0;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_1(info, boolean, int64); break;
      default: CALL_VOID_1(info, int64); break;
      }
    } break;
    case FUNCSIG_DOUBLE: {
      double p0;
      switch (ret_type) {
      case WEED_SEED_DOUBLE: CALL_1(info, double, double); break;
      default: CALL_VOID_1(info, double); break;
      }
    } break;
    case FUNCSIG_STRING: {
      char *p0;
      switch (ret_type) {
      case WEED_SEED_STRING: CALL_1(info, string, string); break;
      case WEED_SEED_INT64: CALL_1(info, int64, string); break;
      default: CALL_VOID_1(info, string); break;
      }
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_VOIDP: {
      void *p0;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_1(info, boolean, voidptr); break;
      case WEED_SEED_INT: CALL_1(info, int, voidptr); break;
      case WEED_SEED_DOUBLE: CALL_1(info, double, voidptr); break;
      default: CALL_VOID_1(info, voidptr); break;
      }
    } break;
    case FUNCSIG_PLANTP: {
      weed_plant_t *p0;
      switch (ret_type) {
      case WEED_SEED_INT64: CALL_1(info, int64, plantptr); break;
      default: CALL_VOID_1(info, plantptr); break;
      }
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 2:
    switch (sig) {
    case FUNCSIG_INT_INT64: {
      int p0; int64_t p1;
      switch (ret_type) {
      default: CALL_VOID_2(info, int, int64); break;
      }
    } break;
    case FUNCSIG_BOOL_INT: {
      int p0; int64_t p1;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_2(info, boolean, boolean, int); break;
      default: CALL_VOID_2(info, boolean, int); break;
      }
    } break;
    case FUNCSIG_INT_VOIDP: {
      int p0; void *p1;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_2(info, boolean, int, voidptr); break;
      default: CALL_VOID_2(info, int, voidptr); break;
      }
    } break;
    case FUNCSIG_STRING_INT: {
      char *p0; int p1;
      switch (ret_type) {
      default: CALL_VOID_2(info, string, int); break;
      }
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_STRING_BOOL: {
      char *p0; int p1;
      switch (ret_type) {
      default: CALL_VOID_2(info, string, boolean); break;
      }
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_DOUBLE_DOUBLE: {
      double p0, p1;
      switch (ret_type) {
      case WEED_SEED_DOUBLE: CALL_2(info, double, double, double); break;
      default: CALL_VOID_2(info, double, double); break;
      }
    } break;
    case FUNCSIG_VOIDP_DOUBLE: {
      void *p0; double p1;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_2(info, boolean, voidptr, double); break;
      default: CALL_VOID_2(info, voidptr, double); break;
      }
    } break;
    case FUNCSIG_VOIDP_INT64: {
      void *p0; int64_t p1;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_2(info, boolean, voidptr, int64); break;
      default: CALL_VOID_2(info, voidptr, int64); break;
      }
    } break;
    case FUNCSIG_VOIDP_VOIDP: {
      void *p0, *p1;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_2(info, boolean, voidptr, voidptr); break;
      default: CALL_VOID_2(info, voidptr, voidptr); break;
      }
    } break;
    case FUNCSIG_VOIDP_STRING: {
      void *p0; char *p1;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_2(info, boolean, voidptr, string); break;
      default: CALL_VOID_2(info, voidptr, string); break;
      }
      if (p1) lives_free(p1);
    } break;
    case FUNCSIG_PLANTP_BOOL: {
      weed_plant_t *p0; int p1;
      switch (ret_type) {
      default: CALL_VOID_2(info, plantptr, boolean); break;
      }
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 3:
    switch (sig) {
    case FUNCSIG_VOIDP_VOIDP_VOIDP: {
      void *p0, *p1, *p2;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_3(info, boolean, voidptr, voidptr, voidptr); break;
      default: CALL_VOID_3(info, voidptr, voidptr, voidptr); break;
      }
    } break;
    case FUNCSIG_VOIDP_VOIDP_BOOL: {
      void *p0, *p1; int p2;
      switch (ret_type) {
      case WEED_SEED_VOIDPTR: CALL_3(info, voidptr, voidptr, voidptr, boolean); break;
      default: CALL_VOID_3(info, voidptr, voidptr, boolean); break;
      }
    } break;
    case FUNCSIG_STRING_VOIDP_VOIDP: {
      char *p0; void *p1, *p2;
      switch (ret_type) {
      case WEED_SEED_VOIDPTR: CALL_3(info, voidptr, string, voidptr, voidptr); break;
      default: CALL_VOID_3(info, string, voidptr, voidptr); break;
      }
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_VOIDP_DOUBLE_INT: {
      void *p0; double p1; int p2;
      switch (ret_type) {
      case WEED_SEED_VOIDPTR: CALL_3(info, voidptr, voidptr, double, int); break;
      default: CALL_VOID_3(info, voidptr, double, int); break;
      }
    } break;
    case FUNCSIG_VOIDP_STRING_STRING: {
      void *p0; char *p1, *p2;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_3(info, boolean, voidptr, string, string); break;
      default: CALL_VOID_3(info, voidptr, string, string); break;
      }
      if (p1) lives_free(p1);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_BOOL_BOOL_STRING: {
      int p0, p1; char *p2;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_3(info, boolean, boolean, boolean, string); break;
      default: CALL_VOID_3(info, boolean, boolean, string); break;
      }
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_PLANTP_VOIDP_INT64: {
      weed_plant_t *p0; void *p1; int64_t p2;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_3(info, boolean, plantptr, voidptr, int64); break;
      default: CALL_VOID_3(info, plantptr, voidptr, int64); break;
      }
    } break;
    case FUNCSIG_INT_VOIDP_INT64: {
      int p0; void *p1; int64_t p2;
      switch (ret_type) {
      case WEED_SEED_INT64: CALL_3(info, int64, int, voidptr, int64); break;
      default: CALL_VOID_3(info, int, voidptr, int64); break;
      }
    } break;
    case FUNCSIG_INT_INT_BOOL: {
      int p0, p1, p2;
      switch (ret_type) {
      default: CALL_VOID_3(info, int, int, boolean); break;
      }
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 4:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT: {
      char *p0, *p1; void *p2; int p3;
      switch (ret_type) {
      case WEED_SEED_STRING: CALL_4(info, string, string, string, voidptr, int); break;
      default: CALL_VOID_4(info, string, string, voidptr, int); break;
      }
      if (p0) lives_free(p0);
      if (p1) lives_free(p1);
    } break;
    case FUNCSIG_INT_INT_BOOL_VOIDP: {
      int p0, p1, p2; void *p3;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_4(info, boolean, int, int, boolean, voidptr); break;
      default: CALL_VOID_4(info, int, int, boolean, voidptr); break;
      }
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 5:
    switch (sig) {
    case FUNCSIG_VOIDP_STRING_STRING_INT64_INT: {
      void *p0; char *p1, *p2; int64_t p3; int p4;
      switch (ret_type) {
      default: CALL_VOID_5(info, voidptr, string, string, int64, int); break;
      }
      if (p1) lives_free(p1);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_INT_INT_INT_BOOL_VOIDP: {
      int p0, p1, p2, p3; void *p4;
      switch (ret_type) {
      default: CALL_VOID_5(info, int, int, int, boolean, voidptr); break;
      }
    } break;
    case FUNCSIG_VOIDP_INT_INT_INT_INT: {
      void *p0; int p1, p2, p3, p4;
      switch (ret_type) {
      case WEED_SEED_BOOLEAN: CALL_5(info, boolean, voidptr, int, int, int, int); break;
      default: CALL_VOID_5(info, voidptr, int, int, int, int); break;
      }
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 6:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP: {
      char *p0, *p1, *p4; void *p2, *p5; int p3;
      switch (ret_type) {
      case WEED_SEED_STRING: CALL_6(info, string, string, string, voidptr, int, string, voidptr); break;
      default: CALL_VOID_6(info, string, string, voidptr, int, string, voidptr); break;
      }
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
  lives_free(thefunc);
  return TRUE;

funcerr:
  msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX called", sig);
  LIVES_FATAL(msg);
  lives_free(msg);
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t info) {
  weed_funcptr_t func = weed_get_funcptr_value(info, LIVES_LEAF_THREADFUNC, NULL);
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  funcsig_t sig = weed_get_int64_value(info, LIVES_LEAF_FUNCSIG, NULL);
  return _call_funcsig_inner(info, func, ret_type, sig);
}



/* lives_proc_thread_t call_funcsig_direct(lives_funcptr_t func, */
/*     int return_type, const char *args_fmt, ...) { */


/* } */


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    uint64_t tstate;
    lives_mutex_lock_carefully(state_mutex);
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    pthread_mutex_unlock(state_mutex);
    return tstate;
  }
  return THRD_STATE_INVALID;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    uint64_t tstate;
    lives_mutex_lock_carefully(state_mutex);
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    pthread_mutex_unlock(state_mutex);
    return tstate & state_bits;
  }
  return THRD_STATE_INVALID;
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    uint64_t tstate;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    lives_mutex_lock_carefully(state_mutex);
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | state_bits);
    pthread_mutex_unlock(state_mutex);
    return tstate;
  }
  return THRD_STATE_INVALID;
}

boolean lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    uint64_t tstate;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    lives_mutex_lock_carefully(state_mutex);
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate & ~state_bits);
    pthread_mutex_unlock(state_mutex);
    return TRUE;
  }
  return FALSE;
}

boolean lives_proc_thread_set_state(lives_proc_thread_t lpt, uint64_t state) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    lives_mutex_lock_carefully(state_mutex);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, state);
    pthread_mutex_unlock(state_mutex);
    return TRUE;
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


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancelled(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_check_states(tinfo, THRD_STATE_CANCELLED))
          == THRD_STATE_CANCELLED) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_set_signalled(lives_proc_thread_t lpt, int signum, void *data) {
  if (!lpt) return FALSE;
  else {
    lives_thread_data_t *mydata = (lives_thread_data_t *)data;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    uint64_t tstate;
    if (mydata) mydata->signum = signum;
    lives_mutex_lock_carefully(state_mutex);
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | THRD_STATE_SIGNALLED);
    weed_set_voidptr_value(lpt, LIVES_LEAF_SIGNAL_DATA, data);
    pthread_mutex_unlock(state_mutex);
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_sync_ready(lives_proc_thread_t tinfo) {
  // this cannot be handles via states as it affects the underlying lives_thread
  if (!tinfo) return;
  else {
    thrd_work_t *work = (thrd_work_t *)weed_get_voidptr_value(tinfo, LIVES_LEAF_THREAD_WORK, NULL);;
    if (work) work->sync_ready = TRUE;
  }
}


#define _join(tinfo, stype) if (is_fg_thread()) {while (!(lives_proc_thread_check_finished(tinfo))) { \
      if (has_lpttorun()) lives_widget_context_update(); lives_nanosleep(LIVES_QUICK_NAP * 10);}} \
  else lives_nanosleep_until_nonzero(weed_leaf_num_elements(tinfo, _RV_)); \
  return weed_get_##stype##_value(tinfo, _RV_, NULL);


LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t tinfo) {
  // WARNING !! this version without a return value will free tinfo !
  if (is_fg_thread()) {
    while (!lives_proc_thread_check_finished(tinfo)) {
      if (has_lpttorun()) {
        if (!will_gov_run()) governor_loop(NULL);
        else lives_widget_context_update();
      }
      //governor_loop(NULL);//lives_widget_context_update();
      lives_nanosleep(LIVES_QUICK_NAP * 10);
    }
  } else lives_nanosleep_while_false(lives_proc_thread_check_finished(tinfo));

  lives_proc_thread_free(tinfo);
}

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
static funcsig_t make_funcsig(lives_proc_thread_t func_info) {
  funcsig_t funcsig = 0;
  for (int nargs = 0; nargs < 16; nargs++) {
    char *lname = lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, nargs);
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


static void pthread_cleanup_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  boolean dontcare = (lives_proc_thread_check_states(info, THRD_OPT_DONTCARE)
                      == THRD_OPT_DONTCARE) ? TRUE : FALSE;

  lives_proc_thread_include_states(info, THRD_STATE_FINISHED);

  lives_hooks_trigger(NULL, THREADVAR(hook_closures), THREAD_EXIT_HOOK);

  if (dontcare || (!ret_type && lives_proc_thread_check_states(info, THRD_OPT_NOTIFY)
                   != THRD_OPT_NOTIFY)) {
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
  boolean bret = call_funcsig(lpt);
  lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
  if (rloc) _weed_leaf_get(lpt, _RV_, 0, rloc);
  return bret;
}


/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
static void run_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  lives_thread_t *thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
  //pthread_mutex_t *state_mutex = weed_get_voidptr_value(thread_info, LIVES_LEAF_STATE_MUTEX, NULL);

  /// tell the thread to clean up after itself [but it won't delete thread_info]
  attr |= LIVES_THRDATTR_AUTODELETE;
  lives_thread_create(thread, attr, _plant_thread_func, (void *)thread_info);
  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    weed_set_voidptr_value(thread_info, LIVES_LEAF_THREAD_WORK, thread->data);
  }
  if (attr & LIVES_THRDATTR_WAIT_START) {
    thrd_work_t *mywork = (thrd_work_t *)thread->data;
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
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile LiVESList *twork_first, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

// TODO - use THRD_SPECIFIC
static LiVESList *allctxs = NULL;

lives_thread_data_t *get_thread_data(void) {
  for (LiVESList *list = allctxs; list; list = list->next) {
    if (pthread_equal(((lives_thread_data_t *)list->data)->pthread, pthread_self())) return list->data;
  }
  return NULL;
}


lives_thread_data_t *get_global_thread_data(void) {
  for (LiVESList *list = allctxs; list; list = list->next) {
    if (pthread_equal(((lives_thread_data_t *)list->data)->pthread, capable->gui_thread)) return list->data;
  }
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


static lives_thread_data_t *get_thread_data_by_id(uint64_t idx) {
  LiVESList *list = allctxs;
  for (; list; list = list->next) {
    if (((lives_thread_data_t *)list->data)->idx == idx) return list->data;
  }
  return NULL;
}


lives_thread_data_t *lives_thread_data_create(uint64_t idx) {
  lives_thread_data_t *tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));
  tdata->pthread = pthread_self();
  if (idx != 0) tdata->ctx = lives_widget_context_new();
  else tdata->ctx = lives_widget_context_default();
  tdata->vars.var_id = tdata->idx = idx;
  tdata->vars.var_rowstride_alignment = ALIGN_DEF;
  tdata->vars.var_last_sws_block = -1;
  tdata->vars.var_uid = gen_unique_id();
  tdata->vars.var_mydata = tdata;
  allctxs = lives_list_prepend(allctxs, (livespointer)tdata);
  return tdata;
}


static boolean widget_context_wrapper(livespointer data) {
  thrd_work_t *mywork = (thrd_work_t *)data;
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

  mywork->self =  pthread_self();
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

  lives_nanosleep_while_false(mywork->sync_ready);

  lives_widget_context_invoke_full(tdata->ctx, mywork->attr & LIVES_THRDATTR_PRIORITY
                                   ? LIVES_WIDGET_PRIORITY_HIGH - 100 : LIVES_WIDGET_PRIORITY_HIGH,
                                   widget_context_wrapper, mywork, NULL);

  for (int type = N_GLOBAL_HOOKS + 1; type < N_HOOK_FUNCS; type++) {
    lives_hooks_clear(THREADVAR(hook_closures), type);
  }

  lives_nanosleep_until_zero(myflags & LIVES_THRDATTR_WAIT_START);

  if (myflags & LIVES_THRDFLAG_AUTODELETE) {
    lives_free(mywork);
    lives_thread_free((lives_thread_t *)list);
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
  lives_thread_data_t *tdata = (lives_thread_data_t *)arg;
  static struct timespec ts;
  int rc = 0;
#ifdef USE_RPMALLOC
  if (!rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_initialize();
  }
#endif

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
  rnpoolthreads = npoolthreads = MINPOOLTHREADS;
  if (mainw->debug) rnpoolthreads = npoolthreads = 0;
  if (prefs->nfx_threads > npoolthreads) rnpoolthreads = npoolthreads = prefs->nfx_threads;
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_first = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    lives_thread_data_t *tdata = lives_thread_data_create(i + 1);
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, thrdpool, tdata);
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
  work->sync_ready = TRUE;

  if (!thread || (attr & LIVES_THRDATTR_AUTODELETE))
    work->flags |= LIVES_THRDFLAG_AUTODELETE;

  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    work->sync_ready = FALSE;
  }

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
          allctxs = lives_list_remove_data(allctxs, tdata, TRUE);
          pthread_join(*poolthrds[i], NULL);
          lives_free(poolthrds[i]);
          tdata = lives_thread_data_create(i + 1);
          poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
          pthread_create(poolthrds[i], NULL, thrdpool, tdata);
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
      // triiggering too often
      poolthrds =
        (pthread_t **)lives_realloc(poolthrds, (rnpoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
      for (int i = rnpoolthreads; i < rnpoolthreads + MINPOOLTHREADS; i++) {
        lives_thread_data_t *tdata = lives_thread_data_create(i + 1);
        poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
        pthread_create(poolthrds[i], NULL, thrdpool, tdata);
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
    if (has_lpttorun()) {
      if (!will_gov_run()) governor_loop(NULL);
      else lives_widget_context_update();
    }
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

LIVES_GLOBAL_INLINE void lives_hook_append(LiVESList **hooks, int type, uint64_t flags, hook_funcptr_t func,
    livespointer data) {
  lives_closure_t *closure;
  boolean skip = FALSE;
  if (flags & HOOK_CB_FG_THREAD) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)data;
    func = (hook_funcptr_t)weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  }
  if (flags & HOOK_UNIQUE_REPLACE_MATCH) {
    LiVESList *list = hooks[type], *listnext;
    skip = TRUE;
    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_BLOCKED) break;
      if (closure->func != func) continue;
      if ((flags & HOOK_UNIQUE_DATA) && closure->data != data) {
        if (flags & HOOK_UNIQUE_CHANGE_DATA) {
          closure->data = data;
          break;
        }
        continue;
      }
      if (!(flags & HOOK_UNIQUE_REPLACE)) break;
      if (!(flags & HOOK_UNIQUE_FUNC)) {
        closure->data = data;
        break;
      }
      hooks[type] = lives_list_remove_node(hooks[type], list, TRUE);
    }
    if (!list) skip = FALSE;
  }
  if (skip) return;
  closure = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
  closure->func = func;
  closure->data = data;
  closure->flags = flags;
  hooks[type] = lives_list_append(hooks[type], closure);
}

LIVES_GLOBAL_INLINE void lives_hook_prepend(LiVESList **hooks, int type, uint64_t flags, hook_funcptr_t func,
    livespointer data) {
  lives_closure_t *closure;
  boolean skip = FALSE;
  if (flags & HOOK_CB_FG_THREAD) {
    lives_proc_thread_t lpt = (lives_proc_thread_t)data;
    func = (hook_funcptr_t)weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  }
  if (flags & HOOK_UNIQUE_REPLACE_MATCH) {
    LiVESList *list = hooks[type], *listnext;
    skip = TRUE;
    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_BLOCKED) break;
      if (closure->func != func) continue;
      if ((flags & HOOK_UNIQUE_DATA) && closure->data != data) {
        if (flags & HOOK_UNIQUE_CHANGE_DATA) {
          closure->data = data;
          break;
        }
        continue;
      }
      if (!(flags & HOOK_UNIQUE_REPLACE)) break;
      if (!(flags & HOOK_UNIQUE_FUNC)) {
        closure->data = data;
        break;
      }
      hooks[type] = lives_list_remove_node(hooks[type], list, TRUE);
    }
    if (!list) skip = FALSE;
  }
  if (skip) return;
  closure = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
  closure->func = func;
  closure->data = data;
  closure->flags = flags;
  hooks[type] = lives_list_prepend(hooks[type], closure);
}


LIVES_GLOBAL_INLINE void lives_hooks_trigger(lives_object_t *obj, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_closure_t *closure;
  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (closure->flags & HOOK_BLOCKED) continue;
    if (closure->flags & HOOK_CB_FG_THREAD) {
      closure->tinfo = (lives_proc_thread_t)closure->data;
      if (is_fg_thread()) {
        fg_run_func(closure->tinfo, closure->retloc);
        lives_proc_thread_free(closure->tinfo);
        continue;
      }
      // some functions may have been deferred, since we cannot stack multiple fg service calls
      fg_service_call(closure->tinfo, closure->retloc);
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

  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (closure->flags & HOOK_BLOCKED) continue;
    if (closure->flags & HOOK_CB_FG_THREAD) {
      // remove here so UNIQUE* works
      xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
    }
  }
}


LIVES_GLOBAL_INLINE void lives_hook_remove(LiVESList **xlist, int type, hook_funcptr_t func, livespointer data) {
  // do not call for HOOK_CB_SINGLE_SHOT (TODO)
  LiVESList *list = xlist[type];
  lives_closure_t *closure;
  for (; list; list = list->next) {
    closure = (lives_closure_t *)list->data;
    if (closure->func == func && closure->data == data) {
      closure->flags |= HOOK_BLOCKED;
      lives_nanosleep_until_zero(closure->tinfo);
      xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
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
      if (closure->flags & HOOK_CB_SINGLE_SHOT)
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
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
  case 'F': return 0x0C;
  case 'V': return 0x0D;
  case 'P': return 0x0C;
  default: return 0x0F;
  }
}


LIVES_GLOBAL_INLINE funcsig_t funcsig_from_args_fmt(const char *args_fmt) {
  funcsig_t fsig = 0;
  if (args_fmt) {
    int i = 0;
    for (char c = args_fmt[i]; c; i++) {
      fsig |= get_typecode(c);
      fsig <<= 4;
    }
  }
  return fsig;
}


LIVES_GLOBAL_INLINE lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
    uint32_t return_type,  const char *args_fmt, uint64_t flags) {
  lives_funcdef_t *fdef = (lives_funcdef_t *)lives_malloc(sizeof(lives_funcdef_t));
  if (fdef) {
    if (funcname) fdef->funcname = lives_strdup(funcname);
    else fdef->funcname = NULL;
    fdef->function = function;
    fdef->return_type = return_type;
    if (args_fmt) fdef->args_fmt = lives_strdup(args_fmt);
    else fdef->args_fmt = NULL;
    fdef->flags = flags;
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


LIVES_GLOBAL_INLINE lives_funcinst_t *create_funcinst_valist(lives_funcdef_t *template, va_list xargs) {
  lives_funcinst_t *finst = lives_plant_new(LIVES_WEED_SUBTYPE_FUNCINST);
  if (finst) {
    lives_plant_params_from_vargs(finst, template->function,
                                  template->return_type ? template->return_type : -1,
                                  template->args_fmt, xargs);
    weed_set_voidptr_value(finst, LIVES_LEAF_TEMPLATE, template);
  }
  return finst;
}


LIVES_GLOBAL_INLINE lives_funcinst_t *create_funcinst(lives_funcdef_t *template, void *retstore, ...) {
  lives_funcinst_t *finst = NULL;
  if (template) {
    va_list xargs;
    va_start(xargs, retstore);
    finst = create_funcinst_valist(template, xargs);
    va_end(xargs);
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
