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
	- WAIT_SYNC  - the proc_thread will be created, but the worker will wait for the caller to set sync_ready before running
	- NO_GUI:    - this has no functional effect, but will set a flag in the worker thread's environment, and this may be
			checked for in the function code and used to bypass graphical updates
	- FG_THREAD: - should not be used directly; main_thread_execute() uses this flag bit internally to ensure that the function is
			always run by the fg thread; (it is safe for the fg thread to call that function)

   - the only requirements are to call lives_proc_thread_create() which will generate a lives_proc_thread_t and run it,
   and then (depending on the return_type parameter, call one of the lives_proc_thread_join_*() functions

   (see that function for more comments)
*/

typedef weed_plantptr_t lives_proc_thread_t;

static funcsig_t make_funcsig(lives_proc_thread_t func_info);

static void resubmit_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr);

LIVES_GLOBAL_INLINE void lives_proc_thread_free(lives_proc_thread_t lpt) {
  pthread_mutex_t *state_mutex
    = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
  if (state_mutex) lives_free(state_mutex);
  weed_plant_free(lpt);
}

lives_proc_thread_t lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs) {
  lives_proc_thread_t thread_info = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  pthread_mutex_t *state_mutex;
  const char *c;
  int p = 0;

  if (!thread_info) return NULL;
  weed_set_funcptr_value(thread_info, LIVES_LEAF_THREADFUNC, func);
  state_mutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(state_mutex, NULL);
  weed_set_voidptr_value(thread_info, LIVES_LEAF_STATE_MUTEX, state_mutex);
  if (return_type) {
    lives_proc_thread_set_state(thread_info, THRD_OPT_NOTIFY);
    if (return_type > 0)  weed_leaf_set(thread_info, LIVES_LEAF_RETURN_VALUE, return_type, 0, NULL);
  }
  c = args_fmt;
  for (c = args_fmt; *c; c++) {
    char *pkey = lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, p++);
    switch (*c) {
    case 'i': weed_set_int_value(thread_info, pkey, va_arg(xargs, int)); break;
    case 'd': weed_set_double_value(thread_info, pkey, va_arg(xargs, double)); break;
    case 'b': weed_set_boolean_value(thread_info, pkey, va_arg(xargs, int)); break;
    case 's': case 'S': weed_set_string_value(thread_info, pkey, va_arg(xargs, char *)); break;
    case 'I': weed_set_int64_value(thread_info, pkey, va_arg(xargs, int64_t)); break;
    case 'F': weed_set_funcptr_value(thread_info, pkey, va_arg(xargs, weed_funcptr_t)); break;
    case 'V': case 'v': weed_set_voidptr_value(thread_info, pkey, va_arg(xargs, void *)); break;
    case 'P': weed_set_plantptr_value(thread_info, pkey, va_arg(xargs, weed_plantptr_t)); break;
    default: lives_proc_thread_free(thread_info); return NULL;
    }
    lives_free(pkey);
  }
  weed_set_int64_value(thread_info, LIVES_LEAF_FUNCSIG, make_funcsig(thread_info));
  if (!(attr & LIVES_THRDATTR_FG_THREAD)) {
    resubmit_proc_thread(thread_info, attr);
    if (!return_type) return NULL;
  }
  return thread_info;
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
  lives_sigdata_t *sigdata = lives_calloc(1, sizeof(lives_sigdata_t));
  ticks_t xtimeout = 1;
  uint64_t tstate;
  boolean tres;
  lives_cancel_t cancel = CANCEL_NONE;
  int xreturn_type = return_type;

  if (xreturn_type == 0) xreturn_type--;

  attr |= LIVES_THRDATTR_WAIT_SYNC;

  va_start(xargs, args_fmt);
  lpt = lives_proc_thread_create_vargs(attr, func, xreturn_type, args_fmt, xargs);
  va_end(xargs);
  sigdata->proc = lpt;
  sigdata->is_timer = TRUE;

  mainw->cancelled = CANCEL_NONE;
  lives_widget_context_update();
  alarm_handle = sigdata->alarm_handle = lives_alarm_set(timeout);
  tres = governor_loop(sigdata);

  tstate = lives_proc_thread_get_state(lpt);

  if (tstate & THRD_STATE_BUSY) {
    // thread MUST unavoidably block; stop the timer (e.g showing a UI)
    // user or other condition may set cancelled
    if ((cancel = mainw->cancelled)) goto thrd_done;
    lives_alarm_reset(alarm_handle, timeout);
  }

  while (!(tres = lives_proc_thread_check_finished(lpt))
         && (timeout == 0 || (xtimeout = lives_alarm_check(alarm_handle)) > 0)) {
    lives_nanosleep(LIVES_SHORT_SLEEP);
    tstate = lives_proc_thread_get_state(lpt);

    // allow governor_loop to run its idle func
    lives_widget_context_update();

    if (tstate & THRD_STATE_BUSY) {
      // thread MUST unavoidably block; stop the timer (e.g showing a UI)
      // user or other condition may set cancelled
      if ((cancel = mainw->cancelled) != CANCEL_NONE) break;
      lives_alarm_reset(alarm_handle, timeout);
    }
  }

thrd_done:
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
  LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
  if (!ctx || ctx == lives_widget_context_default()) return TRUE;
  return FALSE;
}


void *main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  va_list xargs;
  void *ret;
  va_start(xargs, args_fmt);
  lpt = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, func, return_type, args_fmt, xargs);
  if (is_fg_thread()) {
    // run direct
    ret = fg_run_func(lpt, retval);
  } else {
    ret = lives_fg_run(lpt, retval);
  }
  va_end(xargs);
  return ret;
}


void call_funcsig(lives_proc_thread_t info) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  allfunc_t *thefunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  funcsig_t sig = weed_get_int64_value(info, LIVES_LEAF_FUNCSIG, NULL);
  char *msg;

  thefunc->func = weed_get_funcptr_value(info, LIVES_LEAF_THREADFUNC, NULL);

  // Note: C compilers don't care about the type / number of function args., (else it would be impossible to alias any function pointer)
  // just the type / number must be correct at runtime;
  // However it DOES care about the return type. The funcsigs are a guide so that the correct cast / number of args. can be
  // determined in the code., the first argument to the GETARG macro is set by this.
  // return_type determines which function flavour to call, e.g func, funcb, funci
  /// the second argument to GETARG relates to the internal structure of the lives_proc_thread;

  /// LIVES_PROC_THREADS ////////////////////////////////////////

  /// to make any function usable by lives_proc_thread, the _ONLY REQUIREMENT_ is to ensure that there is a function call
  /// corresponding the function arguments (i.e the funcsig) and return value here below
  /// (use of the FUNCSIG_* symbols is optional, they exist only to make it clearer what the function parameters should be)

  /// all of this boils down to the following example:
  // if (sig == 1 && ret_type == WEED_SEED_INT) {
  //   weed_set_int_value(p, "return_value", int_func(weed_get_int_value(p, "p0", NULL)));
  // }
  ///or in the case of a function returning void:
  // if (sig == 0x22) void_func(weed_get_double_value(p, "p0", NULL), weed_get_double_value(p, "p0", NULL))
  switch (sig) {
  case FUNCSIG_VOID:
    switch (ret_type) {
    case WEED_SEED_INT64: CALL_0(int64); break;
    default: CALL_VOID_0(); break;
    } break;
  case FUNCSIG_INT: {
    int p0;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_1(boolean, int); break;
    default: CALL_VOID_1(int); break;
    } break;
  }
  case FUNCSIG_DOUBLE: {
    double p0;
    switch (ret_type) {
    default: CALL_VOID_1(double); break;
    } break;
  }
  case FUNCSIG_STRING: {
    char *p0;
    switch (ret_type) {
    case WEED_SEED_STRING: CALL_1(string, string); break;
    case WEED_SEED_INT64: CALL_1(int64, string); break;
    default: CALL_VOID_1(string); break;
    }
    lives_free(p0);
    break;
  }
  case FUNCSIG_VOIDP: {
    void *p0;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_1(boolean, voidptr); break;
    case WEED_SEED_INT: CALL_1(int, voidptr); break;
    case WEED_SEED_DOUBLE: CALL_1(double, voidptr); break;
    default: CALL_VOID_1(voidptr); break;
    } break;
  }
  case FUNCSIG_PLANTP: {
    weed_plant_t *p0;
    switch (ret_type) {
    case WEED_SEED_INT64: CALL_1(int64, plantptr); break;
    default: CALL_VOID_1(plantptr); break;
    } break;
  }
  case FUNCSIG_INT_INT64: {
    int p0; int64_t p1;
    switch (ret_type) {
    default: CALL_VOID_2(int, int64); break;
    } break;
  }
  case FUNCSIG_INT_VOIDP: {
    int p0; void *p1;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_2(boolean, int, voidptr); break;
    default: CALL_VOID_2(int, voidptr); break;
    } break;
  }
  case FUNCSIG_STRING_INT: {
    char *p0; int p1;
    switch (ret_type) {
    default: CALL_VOID_2(string, int); break;
    }
    lives_free(p0);
    break;
  }
  case FUNCSIG_STRING_BOOL: {
    char *p0; int p1;
    switch (ret_type) {
    default: CALL_VOID_2(string, boolean); break;
    }
    lives_free(p0);
    break;
  }
  case FUNCSIG_VOIDP_DOUBLE: {
    void *p0; double p1;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_2(boolean, voidptr, double); break;
    default: CALL_VOID_2(voidptr, double); break;
    } break;
  }
  case FUNCSIG_VOIDP_VOIDP: {
    void *p0, *p1;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_2(boolean, voidptr, voidptr); break;
    default: CALL_VOID_2(voidptr, voidptr); break;
    } break;
  }
  case FUNCSIG_VOIDP_STRING: {
    void *p0; char *p1;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_2(boolean, voidptr, string); break;
    default: CALL_VOID_2(voidptr, string); break;
    }
    lives_free(p1);
    break;
  }
  case FUNCSIG_PLANTP_BOOL: {
    weed_plant_t *p0; int p1;
    switch (ret_type) {
    default: CALL_VOID_2(plantptr, boolean); break;
    } break;
  }
  case FUNCSIG_VOIDP_VOIDP_VOIDP: {
    void *p0, *p1, *p2;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_3(boolean, voidptr, voidptr, voidptr); break;
    default: CALL_VOID_3(voidptr, voidptr, voidptr); break;
    } break;
  }
  case FUNCSIG_VOIDP_VOIDP_BOOL: {
    void *p0, *p1; int p2;
    switch (ret_type) {
    case WEED_SEED_VOIDPTR: CALL_3(voidptr, voidptr, voidptr, boolean); break;
    default: CALL_VOID_3(voidptr, voidptr, boolean); break;
    } break;
  }
  case FUNCSIG_STRING_VOIDP_VOIDP: {
    char *p0; void *p1, *p2;
    switch (ret_type) {
    case WEED_SEED_VOIDPTR: CALL_3(voidptr, string, voidptr, voidptr); break;
    default: CALL_VOID_3(string, voidptr, voidptr); break;
    }
    lives_free(p0); lives_free(p1);
    break;
  }
  case FUNCSIG_VOIDP_DOUBLE_INT: {
    void *p0; double p1; int p2;
    switch (ret_type) {
    case WEED_SEED_VOIDPTR: CALL_3(voidptr, voidptr, double, int); break;
    default: CALL_VOID_3(voidptr, double, int); break;
    } break;
  }
  case FUNCSIG_BOOL_BOOL_STRING: {
    int p0, p1; char *p2;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_3(boolean, boolean, boolean, string); break;
    default: CALL_VOID_3(boolean, boolean, string); break;
    }
    lives_free(p2);
    break;
  }
  case FUNCSIG_PLANTP_VOIDP_INT64: {
    weed_plant_t *p0; void *p1; int64_t p2;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_3(boolean, plantptr, voidptr, int64); break;
    default: CALL_VOID_3(plantptr, voidptr, int64); break;
    } break;
  }
  case FUNCSIG_STRING_STRING_VOIDP_INT: {
    char *p0, *p1; void *p2; int p3;
    switch (ret_type) {
    case WEED_SEED_STRING: CALL_4(string, string, string, voidptr, int); break;
    default: CALL_VOID_4(string, string, voidptr, int); break;
    }
    lives_free(p0); lives_free(p1);
    break;
  }
  case FUNCSIG_INT_INT_BOOL_VOIDP: {
    int p0, p1, p2; void *p3;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_4(boolean, int, int, boolean, voidptr); break;
    default: CALL_VOID_4(int, int, boolean, voidptr); break;
    } break;
  }
  case FUNCSIG_VOIDP_STRING_STRING_INT64_INT: {
    void *p0; char *p1, *p2; int64_t p3; int p4;
    switch (ret_type) {
    default: CALL_VOID_5(voidptr, string, string, int64, int); break;
    }
    lives_free(p1); lives_free(p2);
    break;
  }
  case FUNCSIG_INT_INT_INT_BOOL_VOIDP: {
    int p0, p1, p2, p3; void *p4;
    switch (ret_type) {
    default: CALL_VOID_5(int, int, int, boolean, voidptr); break;
    } break;
  }
  case FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP: {
    char *p0, *p1, *p4; void *p2, *p5; int p3;
    switch (ret_type) {
    case WEED_SEED_STRING: CALL_6(string, string, string, voidptr, int, string, voidptr); break;
    default: CALL_VOID_6(string, string, voidptr, int, string, voidptr); break;
    }
    lives_free(p0); lives_free(p1); lives_free(p4);
    break;
  }
  default:
    msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX called", sig);
    LIVES_FATAL(msg);
    lives_free(msg);
    break;
  }

  lives_free(thefunc);
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL) : 0;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  return lpt ? (weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL) & state_bits) : 0;
}


uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    uint64_t tstate;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    pthread_mutex_lock(state_mutex);
    tstate = lives_proc_thread_get_state(lpt);
    if (!(tstate & THRD_STATE_FINISHED)) {
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | state_bits);
    }
    pthread_mutex_unlock(state_mutex);
    return tstate & state_bits;
  }
  return 0;
}

boolean lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    pthread_mutex_lock(state_mutex);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, lives_proc_thread_get_state(lpt) & ~state_bits);
    pthread_mutex_unlock(state_mutex);
    return TRUE;
  }
  return FALSE;
}

boolean lives_proc_thread_set_state(lives_proc_thread_t lpt, uint64_t state) {
  if (lpt) {
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    pthread_mutex_lock(state_mutex);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, state);
    pthread_mutex_unlock(state_mutex);
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_get_state(tinfo) & THRD_STATE_FINISHED)) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_get_state(tinfo) & THRD_STATE_SIGNALLED)) ? TRUE : FALSE;
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
  return (tinfo && (lives_proc_thread_get_state(tinfo) & THRD_OPT_CANCELLABLE)) ? TRUE : FALSE;
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


boolean lives_proc_thread_dontcare(lives_proc_thread_t tinfo) {
  if (!tinfo) return FALSE;
  if (!lives_proc_thread_include_states(tinfo, THRD_OPT_DONTCARE)) {
    // task FINISHED before we could set this, so we need to unblock and free it
    // (otherwise it would do this itself when transitioning to FINISHED)
    lives_proc_thread_join(tinfo);
    return FALSE;
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancelled(lives_proc_thread_t tinfo) {
  return (tinfo && (lives_proc_thread_get_state(tinfo) & THRD_STATE_CANCELLED)) ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_set_signalled(lives_proc_thread_t lpt, int signum, void *data) {
  if (!lpt) return FALSE;
  else {
    lives_thread_data_t *mydata = (lives_thread_data_t *)data;
    pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
    if (mydata) mydata->signum = signum;
    pthread_mutex_lock(state_mutex);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, lives_proc_thread_get_state(lpt) | THRD_STATE_SIGNALLED);
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


#define _join(tinfo, stype) if (is_fg_thread()) {while (!(lives_proc_thread_get_state(tinfo) & THRD_STATE_FINISHED)) { \
      if (has_lpttorun()) lives_widget_context_update(); lives_nanosleep(LIVES_QUICK_NAP * 10);}} \
  else lives_nanosleep_until_nonzero(weed_leaf_num_elements(tinfo, _RV_)); \
  return weed_get_##stype##_value(tinfo, _RV_, NULL);


LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t tinfo) {
  // WARNING !! this version without a return value will free tinfo !
  if (is_fg_thread()) {
    while (!lives_proc_thread_check_finished(tinfo)) {
      if (has_lpttorun()) lives_widget_context_update();
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
  boolean dontcare = lives_proc_thread_check_states(info, THRD_OPT_DONTCARE) ? TRUE : FALSE;
  if (dontcare || (!ret_type && !(lives_proc_thread_check_states(info, THRD_OPT_NOTIFY)))) {
    lives_proc_thread_free(info);
  } else lives_proc_thread_include_states(info, THRD_STATE_FINISHED);
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


void *fg_run_func(lives_proc_thread_t lpt, void *retval) {
  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);

  call_funcsig(lpt);

  switch (ret_type) {
  case WEED_SEED_INT: {
    int *ival = (int *)retval;
    *ival = weed_get_int_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return (void *)ival;
  }
  case WEED_SEED_BOOLEAN: {
    int *bval = (int *)retval;
    *bval = weed_get_boolean_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return (void *)bval;
  }
  case WEED_SEED_DOUBLE: {
    double *dval = (double *)retval;
    *dval = weed_get_double_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return (void *)dval;
  }
  case WEED_SEED_STRING: {
    char *chval = weed_get_string_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return (void *)chval;
  }
  case WEED_SEED_INT64: {
    int64_t *i64val = (int64_t *)retval;
    *i64val = weed_get_int64_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return (void *)i64val;
  }
  case WEED_SEED_VOIDPTR: {
    void *val;
    val = weed_get_voidptr_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return val;
  }
  case WEED_SEED_PLANTPTR: {
    weed_plant_t *pval;
    pval = weed_get_plantptr_value(lpt, _RV_, NULL);
    lives_proc_thread_free(lpt);
    return (void *)pval;
  }
  /// no funcptrs or custom...yet
  default:
    lives_proc_thread_free(lpt);
    break;
  }
  return NULL;
}

#undef _RV_

/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
static void resubmit_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  lives_thread_t *thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));

  /// tell the thread to clean up after itself [but it won't delete thread_info]
  attr |= LIVES_THRDATTR_AUTODELETE;
  lives_thread_create(thread, attr, _plant_thread_func, (void *)thread_info);
  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    weed_set_voidptr_value(thread_info, LIVES_LEAF_THREAD_WORK, thread->data);
  }
}


//////// worker thread pool //////////////////////////////////////////

///////// thread pool ////////////////////////
#ifndef VALGRIND_ON
#define MINPOOLTHREADS 8
#else
#define MINPOOLTHREADS 2
#endif
static int npoolthreads;
static pthread_t **poolthrds;
static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static LiVESList *twork_first, *twork_last; /// FIFO list of tasks
static volatile int ntasks;
static boolean threads_die;

// TODO - use THRD_SPECIFIC
static LiVESList *allctxs = NULL;

lives_thread_data_t *get_thread_data(void) {
  LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
  LiVESList *list = allctxs;
  if (!ctx) ctx = lives_widget_context_default();
  for (; list; list = list->next) {
    if (((lives_thread_data_t *)list->data)->ctx == ctx) return list->data;
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

static lives_thread_data_t *get_thread_data_by_id(uint64_t idx) {
  LiVESList *list = allctxs;
  for (; list; list = list->next) {
    if (((lives_thread_data_t *)list->data)->idx == idx) return list->data;
  }
  return NULL;
}


lives_thread_data_t *lives_thread_data_create(uint64_t idx) {
  lives_thread_data_t *tdata = (lives_thread_data_t *)lives_calloc(1, sizeof(lives_thread_data_t));
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


static boolean gsrc_wrapper(livespointer data) {
  thrd_work_t *mywork = (thrd_work_t *)data;
  (*mywork->func)(mywork->arg);
  return FALSE;
}


//static pthread_mutex_t cpusel_mutex = PTHREAD_MUTEX_INITIALIZER;

boolean do_something_useful(lives_thread_data_t *tdata) {
  /// yes, why don't you lend a hand instead of just lying around nanosleeping...
  LiVESList *list;
  thrd_work_t *mywork;
  uint64_t myflags = 0;
  //boolean didlock = FALSE;

  if (!tdata->idx) abort();

  pthread_mutex_lock(&twork_mutex);
  list = twork_last;
  if (LIVES_UNLIKELY(!list)) {
    pthread_mutex_unlock(&twork_mutex);
    return FALSE;
  }

  if (twork_first == list) twork_last = twork_first = NULL;
  else {
    twork_last = list->prev;
    twork_last->next = NULL;
  }
  pthread_mutex_unlock(&twork_mutex);

  mywork = (thrd_work_t *)list->data;
  mywork->self =  pthread_self();
  mywork->busy = tdata->idx;
  myflags = mywork->flags;

  /* if (prefs->jokes) { */
  /*   fprintf(stderr, "thread id %ld reporting for duty, Sir !\n", get_thread_id()); */
  /* } */

  lives_nanosleep_until_nonzero(mywork->sync_ready);

  lives_widget_context_invoke(tdata->ctx, gsrc_wrapper, mywork);

  if (myflags & LIVES_THRDFLAG_AUTODELETE) {
    lives_free(mywork); lives_free(list);
  } else mywork->done = tdata->idx;

  pthread_mutex_lock(&twork_count_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_count_mutex);
#ifdef USE_RPMALLOC
  rpmalloc_thread_collect();
#endif
  return TRUE;
}


static void *thrdpool(void *arg) {
  boolean skip_wait = FALSE;
  lives_thread_data_t *tdata = (lives_thread_data_t *)arg;
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
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_wait(&tcond, &tcond_mutex);
      pthread_mutex_unlock(&tcond_mutex);
    }
    if (LIVES_UNLIKELY(threads_die)) break;
    skip_wait = do_something_useful(tdata);
#ifdef USE_RPMALLOC
    if (rpmalloc_is_thread_initialized()) {
      rpmalloc_thread_collect();
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
  npoolthreads = MINPOOLTHREADS;
  if (prefs->nfx_threads > npoolthreads) npoolthreads = prefs->nfx_threads;
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
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_broadcast(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  for (int i = 0; i < npoolthreads; i++) {
    lives_thread_data_t *tdata = get_thread_data_by_id(i + 1);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    pthread_join(*(poolthrds[i]), NULL);
    lives_widget_context_unref(tdata->ctx);
    lives_free(tdata);
    lives_free(poolthrds[i]);
  }
  lives_free(poolthrds);
  poolthrds = NULL;
  npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_first);
  twork_first = twork_last = NULL;
  ntasks = 0;
}


int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr,
                        lives_thread_func_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  if (!thread) list = (LiVESList *)lives_calloc(1, sizeof(LiVESList));
  else list->next = list->prev = NULL;
  list->data = work;
  work->func = func;
  work->arg = arg;
  work->sync_ready = TRUE;

  if (!thread || (attr & LIVES_THRDATTR_AUTODELETE))
    work->flags |= LIVES_THRDFLAG_AUTODELETE;

  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    work->sync_ready = FALSE;
  }

  pthread_mutex_lock(&twork_mutex);
  if (!twork_first) {
    twork_first = twork_last = list;
  } else {
    if (!(attr & LIVES_THRDATTR_PRIORITY)) {
      twork_first->prev = list;
      list->next = twork_first;
      twork_first = list;
    } else {
      twork_last->next = list;
      list->prev = twork_last;
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
  if (ntasks >= npoolthreads) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    poolthrds =
      (pthread_t **)lives_realloc(poolthrds, (npoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
    for (int i = npoolthreads; i < npoolthreads + MINPOOLTHREADS; i++) {
      lives_thread_data_t *tdata = lives_thread_data_create(i + 1);
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, thrdpool, tdata);
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    npoolthreads += MINPOOLTHREADS;
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
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    if (task->busy) break;
    sched_yield();
    lives_nanosleep(1000);
  }

  if (!task->done) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_signal(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
  }

  lives_nanosleep_until_nonzero(task->done);
  nthrd = task->done;

  if (retval) *retval = task->ret;
  lives_free(task);
  return nthrd;
}
