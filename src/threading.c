// threading.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

/**
   lives  proc_threads API
   - the only requirements are to call lives_proc_thread_create() which will generate a lives_proc_thread_t and run it,
   and then (depending on the return_type parameter, call one of the lives_proc_thread_join_*() functions

   (see that function for more comments)
*/

typedef weed_plantptr_t lives_proc_thread_t;

LIVES_GLOBAL_INLINE void lives_proc_thread_free(lives_proc_thread_t lpt) {weed_plant_free(lpt);}

static lives_proc_thread_t _lives_proc_thread_create(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs) {
  int p = 0;
  const char *c;
  weed_plant_t *thread_info = lives_plant_new(LIVES_WEED_SUBTYPE_PROC_THREAD);
  if (!thread_info) return NULL;
  weed_set_funcptr_value(thread_info, WEED_LEAF_THREADFUNC, func);
  if (return_type) {
    pthread_mutex_t *dcmutex = (pthread_mutex_t *)lives_malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(dcmutex, NULL);
    weed_set_voidptr_value(thread_info, WEED_LEAF_DONTCARE_MUTEX, dcmutex);
    weed_set_boolean_value(thread_info, WEED_LEAF_NOTIFY, WEED_TRUE);
    if (return_type > 0)  weed_leaf_set(thread_info, WEED_LEAF_RETURN_VALUE, return_type, 0, NULL);
  }
  c = args_fmt;
  for (c = args_fmt; *c; c++) {
    char *pkey = lives_strdup_printf("%s%d", WEED_LEAF_THREAD_PARAM, p++);
    switch (*c) {
    case 'i': weed_set_int_value(thread_info, pkey, va_arg(xargs, int)); break;
    case 'd': weed_set_double_value(thread_info, pkey, va_arg(xargs, double)); break;
    case 'b': weed_set_boolean_value(thread_info, pkey, va_arg(xargs, int)); break;
    case 's': case 'S': weed_set_string_value(thread_info, pkey, va_arg(xargs, char *)); break;
    case 'I': weed_set_int64_value(thread_info, pkey, va_arg(xargs, int64_t)); break;
    case 'F': weed_set_funcptr_value(thread_info, pkey, va_arg(xargs, weed_funcptr_t)); break;
    case 'V': case 'v': weed_set_voidptr_value(thread_info, pkey, va_arg(xargs, void *)); break;
    case 'P': weed_set_plantptr_value(thread_info, pkey, va_arg(xargs, weed_plantptr_t)); break;
    default: weed_plant_free(thread_info); return NULL;
    }
    lives_free(pkey);
  }

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
   lives_proc_thread_check() with the return : - this function is guaranteed to return FALSE whilst the thread is running
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
  lpt = _lives_proc_thread_create(attr, func, return_type, args_fmt, xargs);
  va_end(xargs);
  return lpt;
}


void *main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...) {
  void *dcmutex;
  LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
  lives_proc_thread_t lpt;
  va_list xargs;
  void *ret;
  va_start(xargs, args_fmt);
  lpt = _lives_proc_thread_create(LIVES_THRDATTR_FG_THREAD, func, return_type, args_fmt, xargs);
  dcmutex = weed_get_voidptr_value(lpt, WEED_LEAF_DONTCARE_MUTEX, NULL);
  if (dcmutex) lives_free(dcmutex);
  if (!ctx || ctx == lives_widget_context_default()) {
    // run direct
    ret = fg_run_func(lpt, retval);
  } else {
    ret = lives_fg_run(lpt, retval);
  }
  va_end(xargs);
  return ret;
}


static void call_funcsig(funcsig_t sig, lives_proc_thread_t info) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  allfunc_t *thefunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  char *msg;

  thefunc->func = weed_get_funcptr_value(info, WEED_LEAF_THREADFUNC, NULL);

#define FUNCSIG_VOID				       			0X00000000
#define FUNCSIG_INT 			       				0X00000001
#define FUNCSIG_DOUBLE 				       			0X00000002
#define FUNCSIG_STRING 				       			0X00000004
#define FUNCSIG_VOIDP 				       			0X0000000D
#define FUNCSIG_PLANTP 				       			0X0000000E
#define FUNCSIG_INT_INT64 			       			0X00000015
#define FUNCSIG_STRING_INT 			      			0X00000041
#define FUNCSIG_STRING_BOOL 			      			0X00000043
#define FUNCSIG_VOIDP_VOIDP 				       		0X000000DD
#define FUNCSIG_VOIDP_STRING 				       		0X000000D4
#define FUNCSIG_VOIDP_DOUBLE 				       		0X000000D2
#define FUNCSIG_PLANTP_BOOL 				       		0X000000E3
  // 3p
#define FUNCSIG_VOIDP_VOIDP_VOIDP 		        		0X00000DDD
#define FUNCSIG_BOOL_BOOL_STRING 		        		0X00000334
#define FUNCSIG_PLANTP_VOIDP_INT64 		        		0X00000ED5
  // 4p
#define FUNCSIG_STRING_STRING_VOIDP_INT					0X000044D1
#define FUNCSIG_INT_INT_BOOL_VOIDP					0X0000113D
  // 5p
#define FUNCSIG_INT_INT_INT_BOOL_VOIDP					0X0001113D
#define FUNCSIG_VOIDP_STRING_STRING_INT64_INT			       	0X000D4451
  // 6p
#define FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP		       	0X0044D14D

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

  switch (sig) {
  case FUNCSIG_VOID:
    switch (ret_type) {
    case WEED_SEED_INT64: CALL_0(int64); break;
    default: CALL_VOID_0(); break;
    }
    break;
  case FUNCSIG_INT: {
    int p0;
    switch (ret_type) {
    default: CALL_VOID_1(int); break;
    }
    break;
  }
  case FUNCSIG_DOUBLE: {
    double p0;
    switch (ret_type) {
    default: CALL_VOID_1(double); break;
    }
    break;
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
    default: CALL_VOID_1(voidptr); break;
    }
    break;
  }
  case FUNCSIG_PLANTP: {
    weed_plant_t *p0;
    switch (ret_type) {
    case WEED_SEED_INT64: CALL_1(int64, plantptr); break;
    default: CALL_VOID_1(plantptr); break;
    }
    break;
  }
  case FUNCSIG_INT_INT64: {
    int p0; int64_t p1;
    switch (ret_type) {
    default: CALL_VOID_2(int, int64); break;
    }
    break;
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
    default: CALL_VOID_2(voidptr, double); break;
    }
    break;
  }
  case FUNCSIG_VOIDP_VOIDP: {
    void *p0, *p1;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_2(boolean, voidptr, voidptr); break;
    default: CALL_VOID_2(voidptr, voidptr); break;
    }
    break;
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
    }
    break;
  }
  case FUNCSIG_VOIDP_VOIDP_VOIDP: {
    void *p0, *p1, *p2;
    switch (ret_type) {
    case WEED_SEED_BOOLEAN: CALL_3(boolean, voidptr, voidptr, voidptr); break;
    default: CALL_VOID_3(voidptr, voidptr, voidptr); break;
    }
    break;
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
    }
    break;
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
    }
    break;
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
    }
    break;
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
    msg = lives_strdup_printf("Unknown funcsig with tyte 0x%016lX called", sig);
    LIVES_FATAL(msg);
    lives_free(msg);
    break;
  }

  lives_free(thefunc);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_sync_ready(lives_proc_thread_t tinfo) {
  volatile boolean *sync_ready = (volatile boolean *)weed_get_voidptr_value(tinfo, "sync_ready", NULL);
  if (sync_ready) *sync_ready = TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_check(lives_proc_thread_t tinfo) {
  /// returns FALSE while the thread is running, TRUE once it has finished
  if (!tinfo) return TRUE;
  if (weed_plant_has_leaf(tinfo, WEED_LEAF_NOTIFY) && weed_get_boolean_value(tinfo, WEED_LEAF_DONE, NULL)
      == WEED_FALSE)
    return FALSE;
  return (weed_leaf_num_elements(tinfo, _RV_) > 0
          || weed_get_boolean_value(tinfo, WEED_LEAF_DONE, NULL) == WEED_TRUE);
}

LIVES_GLOBAL_INLINE int lives_proc_thread_signalled(lives_proc_thread_t tinfo) {
  /// returns FALSE while the thread is running, TRUE once it has finished
  return (weed_get_int_value(tinfo, WEED_LEAF_SIGNALLED, NULL) == WEED_TRUE);
}

LIVES_GLOBAL_INLINE int64_t lives_proc_thread_signalled_idx(lives_proc_thread_t tinfo) {
  /// returns FALSE while the thread is running, TRUE once it has finished
  lives_thread_data_t *tdata = (lives_thread_data_t *)weed_get_voidptr_value(tinfo, WEED_LEAF_SIGNAL_DATA, NULL);
  if (tdata) return tdata->idx;
  return 0;
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_cancellable(lives_proc_thread_t tinfo) {
  weed_set_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLABLE, WEED_TRUE);
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t tinfo) {
  return weed_get_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLABLE, NULL) == WEED_TRUE ? TRUE : FALSE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancel(lives_proc_thread_t tinfo) {
  if (!lives_proc_thread_get_cancellable(tinfo)) return FALSE;
  weed_set_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLED, WEED_TRUE);
  lives_proc_thread_join(tinfo);
  return TRUE;
}

boolean lives_proc_thread_dontcare(lives_proc_thread_t tinfo) {
  /// if thread is running, tell it we no longer care about return value, so it can free itself
  /// if finished we just call lives_proc_thread_join() to free it
  /// a mutex is used to ensure the proc_thread does not finish between setting the flag and checking if it has ifnished
  if (tinfo) {
    pthread_mutex_t *dcmutex = weed_get_voidptr_value(tinfo, WEED_LEAF_DONTCARE_MUTEX, NULL);
    if (dcmutex) {
      pthread_mutex_lock(dcmutex);
      if (!lives_proc_thread_check(tinfo)) {
	weed_set_boolean_value(tinfo, WEED_LEAF_DONTCARE, WEED_TRUE);
	pthread_mutex_unlock(dcmutex);
      } else {
	pthread_mutex_unlock(dcmutex);
	lives_proc_thread_join(tinfo);
      }
    }
  }
  return TRUE;
}

LIVES_GLOBAL_INLINE boolean lives_proc_thread_cancelled(lives_proc_thread_t tinfo) {
  return (tinfo && weed_get_boolean_value(tinfo, WEED_LEAF_THREAD_CANCELLED, NULL) == WEED_TRUE)
         ? TRUE : FALSE;
}

#define _join(stype) lives_nanosleep_until_nonzero(weed_leaf_num_elements(tinfo, _RV_)); \
  return weed_get_##stype##_value(tinfo, _RV_, NULL);

LIVES_GLOBAL_INLINE void lives_proc_thread_join(lives_proc_thread_t tinfo) {
  // WARNING !! version without a return value will free tinfo !
  void *dcmutex;
  lives_nanosleep_until_nonzero((weed_get_boolean_value(tinfo, WEED_LEAF_DONE, NULL) == WEED_TRUE));
  dcmutex = weed_get_voidptr_value(tinfo, WEED_LEAF_DONTCARE_MUTEX, NULL);
  if (dcmutex) lives_free(dcmutex);
  weed_plant_free(tinfo);
}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_int(lives_proc_thread_t tinfo) { _join(int);}
LIVES_GLOBAL_INLINE double lives_proc_thread_join_double(lives_proc_thread_t tinfo) {_join(double);}
LIVES_GLOBAL_INLINE int lives_proc_thread_join_boolean(lives_proc_thread_t tinfo) { _join(boolean);}
LIVES_GLOBAL_INLINE int64_t lives_proc_thread_join_int64(lives_proc_thread_t tinfo) {_join(int64);}
LIVES_GLOBAL_INLINE char *lives_proc_thread_join_string(lives_proc_thread_t tinfo) {_join(string);}
LIVES_GLOBAL_INLINE weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t tinfo) {_join(funcptr);}
LIVES_GLOBAL_INLINE void *lives_proc_thread_join_voidptr(lives_proc_thread_t tinfo) {_join(voidptr);}
LIVES_GLOBAL_INLINE weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t tinfo) {_join(plantptr);}

/**
   create a funcsig from a lives_proc_thread_t object
   the returned value can be passed to call_funcsig, along with the original lives_proc_thread_t
*/
static funcsig_t make_funcsig(lives_proc_thread_t func_info) {
  funcsig_t funcsig = 0;
  for (register int nargs = 0; nargs < 16; nargs++) {
    char *lname = lives_strdup_printf("%s%d", WEED_LEAF_THREAD_PARAM, nargs);
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

static void *_plant_thread_func(void *args) {
  lives_proc_thread_t info = (lives_proc_thread_t)args;
  uint32_t ret_type = weed_leaf_seed_type(info, _RV_);
  funcsig_t sig = make_funcsig(info);
  THREADVAR(tinfo) = info;
  if (weed_get_boolean_value(info, "no_gui", NULL) == WEED_TRUE) THREADVAR(no_gui) = TRUE;
  call_funcsig(sig, info);

  if (weed_get_boolean_value(info, WEED_LEAF_NOTIFY, NULL) == WEED_TRUE) {
    boolean dontcare;
    pthread_mutex_t *dcmutex = (pthread_mutex_t *)weed_get_voidptr_value(info, WEED_LEAF_DONTCARE_MUTEX, NULL);
    pthread_mutex_lock(dcmutex);
    dontcare = weed_get_boolean_value(info, WEED_LEAF_DONTCARE, NULL);
    weed_set_boolean_value(info, WEED_LEAF_DONE, WEED_TRUE);
    pthread_mutex_unlock(dcmutex);
    if (dontcare == WEED_TRUE) {
      lives_free(dcmutex);
      weed_plant_free(info);
    }
  } else if (!ret_type) weed_plant_free(info);
  return NULL;
}


void *fg_run_func(lives_proc_thread_t lpt, void *retval) {
  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);
  funcsig_t sig = make_funcsig(lpt);

  call_funcsig(sig, lpt);

  switch (ret_type) {
  case WEED_SEED_INT: {
    int *ival = (int *)retval;
    *ival = weed_get_int_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)ival;
  }
  case WEED_SEED_BOOLEAN: {
    int *bval = (int *)retval;
    *bval = weed_get_boolean_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)bval;
  }
  case WEED_SEED_DOUBLE: {
    double *dval = (double *)retval;
    *dval = weed_get_double_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)dval;
  }
  case WEED_SEED_STRING: {
    char *chval = weed_get_string_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)chval;
  }
  case WEED_SEED_INT64: {
    int64_t *i64val = (int64_t *)retval;
    *i64val = weed_get_int64_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)i64val;
  }
  case WEED_SEED_VOIDPTR: {
    void *val;
    val = weed_get_voidptr_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return val;
  }
  case WEED_SEED_PLANTPTR: {
    weed_plant_t *pval;
    pval = weed_get_plantptr_value(lpt, _RV_, NULL);
    weed_plant_free(lpt);
    return (void *)pval;
  }
  /// no funcptrs or custom...yet
  default:
    weed_plant_free(lpt);
    break;
  }
  return NULL;
}

#undef _RV_

/// (re)submission point, the function call is added to the threadpool tasklist
/// if we have sufficient threads the task will be run at once, if all threads are busy then MINPOOLTHREADS new threads will be created
/// and added to the pool
void resubmit_proc_thread(lives_proc_thread_t thread_info, lives_thread_attr_t attr) {
  /// run any function as a lives_thread
  lives_thread_t *thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
  thrd_work_t *work;

  /// tell the thread to clean up after itself [but it won't delete thread_info]
  attr |= LIVES_THRDATTR_AUTODELETE;
  lives_thread_create(thread, attr, _plant_thread_func, (void *)thread_info);
  work = (thrd_work_t *)thread->data;
  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    weed_set_voidptr_value(thread_info, "sync_ready", (void *) & (work->sync_ready));
  }
  if (attr & LIVES_THRDATTR_NO_GUI) {
    weed_set_boolean_value(thread_info, "no_gui", WEED_TRUE);
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
  tdata->idx = idx;
  tdata->vars.var_rowstride_alignment = ALIGN_DEF;
  tdata->vars.var_last_sws_block = -1;
  tdata->vars.var_mydata = tdata;
  allctxs = lives_list_prepend(allctxs, (livespointer)tdata);
  return tdata;
}


static boolean gsrc_wrapper(livespointer data) {
  thrd_work_t *mywork = (thrd_work_t *)data;
  (*mywork->func)(mywork->arg);
  return FALSE;
}


boolean do_something_useful(lives_thread_data_t *tdata) {
  /// yes, why don't you lend a hand instead of just lying around nanosleeping...
  LiVESList *list;
  thrd_work_t *mywork;
  uint64_t myflags = 0;

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
  mywork->busy = tdata->idx;
  myflags = mywork->flags;

  if (myflags & LIVES_THRDFLAG_WAIT_SYNC) {
    lives_nanosleep_until_nonzero(mywork->sync_ready);
  }

  lives_widget_context_invoke(tdata->ctx, gsrc_wrapper, mywork);
  //(*mywork->func)(mywork->arg);

  if (myflags & LIVES_THRDFLAG_AUTODELETE) {
    lives_free(mywork); lives_free(list);
  } else mywork->done = tdata->idx;

  pthread_mutex_lock(&twork_count_mutex);
  ntasks--;
  pthread_mutex_unlock(&twork_count_mutex);

  rpmalloc_thread_collect();
  return TRUE;
}


static void *thrdpool(void *arg) {
  boolean skip_wait = FALSE;
  lives_thread_data_t *tdata = (lives_thread_data_t *)arg;
  if (!rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_initialize();
  }

  lives_widget_context_push_thread_default(tdata->ctx);

  while (!threads_die) {
    if (!skip_wait) {
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_wait(&tcond, &tcond_mutex);
      pthread_mutex_unlock(&tcond_mutex);
    }
    if (LIVES_UNLIKELY(threads_die)) break;
    skip_wait = do_something_useful(tdata);
    if (rpmalloc_is_thread_initialized()) {
      rpmalloc_thread_collect();
    }
  }
  if (rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_finalize();
  }
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


int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr, lives_funcptr_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  if (!thread) list = (LiVESList *)lives_calloc(1, sizeof(LiVESList));
  else list->next = list->prev = NULL;
  list->data = work;
  work->func = func;
  work->arg = arg;

  if (!thread || (attr & LIVES_THRDATTR_AUTODELETE))
    work->flags |= LIVES_THRDFLAG_AUTODELETE;
  if (attr & LIVES_THRDATTR_WAIT_SYNC) {
    work->flags |= LIVES_THRDFLAG_WAIT_SYNC;
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
    poolthrds = (pthread_t **)lives_realloc(poolthrds, (npoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
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
