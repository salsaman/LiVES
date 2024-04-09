// threading.c
// (c) G. Finch 2019 - 2024 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "effects-weed.h" // for LIVES_LEAF_CONST_VALUE
#include "diagnostics.h"

#ifdef AUDIT_REFC
weed_plant_t *auditor_refc = NULL;
#endif


void lives_proc_thread_autofree(lives_proc_thread_t lpt, ...) {
  // provided with a lives_proc_thread, and a NULL terminated sequence of &(void *)var0, var1,...
  // all vars will be freed and set to NULL when lpt COMPLETES. This also handles the case where lpt
  // is dontcared. If lpt already completed, the vars are freed/nullified immediately.
  //
  if (!lpt) return;

  lives_result_t res;
  void **vpptr;
  LiVESList *vptrptrs = NULL;
  va_list ap;

  va_start(ap, lpt);

  while ((vpptr = va_arg(ap, void **)))
    vptrptrs = lives_list_prepend(vptrptrs, (void *)vpptr);

  va_end(ap);

  res = lives_proc_thread_freeze_state(lpt, TRUE);
  if (res != LIVES_RESULT_SUCCESS || lives_proc_thread_check_completed(lpt)) {
    if (res == LIVES_RESULT_SUCCESS)
      lives_proc_thread_unfreeze_state(lpt);
    lives_freep_multi(&vptrptrs);
  } else {
    lives_proc_thread_add_hook_cb_full(lpt, COMPLETED_HOOK, 0, lives_freep_multi,
                                       WEED_SEED_VOID, "V", &vptrptrs);
    lives_proc_thread_unfreeze_state(lpt);
  }
}


static lives_result_t lives_proc_thread_guillotine(lives_proc_thread_t, timeout_data *);

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
   - proc_threads can be used to assign tasks from bg threads to main thread
   - calling lives_proc_thread_dontcare() has the effect to of turning any return type to type 0
   (this is protected by a mutex to ensure it is always done atomically)
   - amongst the available attributes are:
   - PRIORITY   - this is also a lives_thread flag: - the result is to add the job at the head of the pool queue
   rather than at the tail end
   - AUTODELETE - tells the underlying thread to free it's resources automatically there is no need to set this for proc threads
   is this done in the create function
   - WAIT_START - the proc_thread will be created, but the caller will block in lives_proc_thread_dispatch
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

   - START_UNQUEUED: - create the thread but do not queue it for executuion. lives_proc_thread_dispatchlpt) can be called
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

static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
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
  //or for example, add this to the COMPLETED hook of an lpt and request to be woken when it completes
  if (!other) return FALSE;
  lives_proc_thread_request_resume(other);
  return FALSE;
}


boolean signal_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other, weed_plant_t *data) {
  // hook callback to resume a paused / waiting proc_thread
  // e.g pause and wake simultaneous with other ptoc_thread
  //lives_proc_thread_add_hook(lpt, RESUMING_HOOK, 0, wake_other_lpt, self);
  //or for example, add this to the COMPLETED hook of an lpt and request to be woken when it completes
  if (!other) return FALSE;
  pthread_t pth = lives_proc_thread_get_pthread(other);
  if (!pth) return FALSE;
  if (!lives_proc_thread_try_interrupt(other, data)) return FALSE;
  pthread_kill(pth, LIVES_INTERRUPT_SIG);
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
  uint64_t attrs = lives_proc_thread_get_attrs(other);
  lives_proc_thread_set_attrs(other, attrs & ~LIVES_THRDATTR_DONTCARE);
  return FALSE;
}


boolean queue_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other) {
  // hook callback to queue other lpt for actioning
  // e.g queue another proc_thread to run when this on completes
  //lives_proc_thread_add_hook(self, FINISHED_HOOK, 0, queue_other_lpt, other);
  // queue attrs will be taken from other attrs
  if (!other) return FALSE;
  lives_proc_thread_dispatch(other);
  return FALSE;
}

//////////////////////////////////////////////

LIVES_LOCAL_INLINE void _lives_proc_thread_set_active_funcinst(lives_proc_thread_t lpt, lives_funcinst_t *finst) {
  weed_set_voidptr_value(lpt, LIVES_LEAF_ACTIVE_FUNCINST, finst);
}


LIVES_LOCAL_INLINE void lives_proc_thread_set_active_funcinst(lives_funcinst_t *finst) {
  GET_PROC_THREAD_SELF(self);
  LIVES_ASSERT(self);
  _lives_proc_thread_set_active_funcinst(self, finst);
}


LIVES_LOCAL_INLINE lives_funcinst_t *lives_proc_thread_pop_active_funcinst(void) {
  // pop funcinst, and possibly set active finstlist to NULL
  GET_PROC_THREAD_SELF(self);
  lives_funcinst_t *finst = NULL;
  lives_sync_list_t *sync_list = lives_proc_thread_get_active_finstlist(self);
  lives_sync_list_pop(&sync_list);
  if (!sync_list) {
    lives_proc_thread_set_active_finstlist(self, NULL);
    lives_proc_thread_set_initial_finstlist(self, NULL);
    lives_proc_thread_set_initial_funcinst(self, NULL);
  } else finst = lives_sync_list_peek(sync_list);
  lives_proc_thread_set_active_funcinst(finst);
  return finst;
}


LIVES_GLOBAL_INLINE lives_funcinst_t *lives_proc_thread_get_active_funcinst(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_ACTIVE_FUNCINST, NULL) : NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_active_finstlist(lives_proc_thread_t lpt, lives_sync_list_t *sync_list) {
  if (lpt) weed_set_voidptr_value(lpt, LIVES_LEAF_ACTIVE_FINSTLIST, sync_list);
}


LIVES_GLOBAL_INLINE lives_sync_list_t *lives_proc_thread_get_active_finstlist(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_ACTIVE_FINSTLIST, NULL) : NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_initial_finstlist(lives_proc_thread_t lpt, lives_sync_list_t *sync_list) {
  if (lpt) weed_set_voidptr_value(lpt, LIVES_LEAF_INIT_FINSTLIST, sync_list);
}


LIVES_GLOBAL_INLINE lives_sync_list_t *lives_proc_thread_get_initial_finstlist(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_INIT_FINSTLIST, NULL) : NULL;
}


lives_funcinst_t *lives_proc_thread_get_initial_funcinst(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_INIT_FUNCINST, NULL) : NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_initial_funcinst(lives_proc_thread_t lpt, lives_funcinst_t *finst) {
  if (lpt)  weed_set_voidptr_value(lpt, LIVES_LEAF_INIT_FUNCINST, finst);
}


void lives_proc_thread_push_active_funcinst(lives_proc_thread_t lpt, lives_funcinst_t *finst) {
  // will set finst disposition to READY (unless stacked)
  if (!finst) return;
  lives_sync_list_t *sync_list = lives_proc_thread_get_active_finstlist(lpt);
  sync_list = lives_sync_list_push(sync_list, (void *)finst);
  lives_proc_thread_set_active_finstlist(lpt, sync_list);
  if (!lives_proc_thread_get_initial_finstlist(lpt)) {
    lives_proc_thread_set_initial_finstlist(lpt, sync_list);
    weed_set_voidptr_value(lpt, LIVES_LEAF_INIT_FUNCINST, finst);
  }
  _lives_proc_thread_set_active_funcinst(lpt, finst);
  lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_READY);
}


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


// auto-pausse, returns TRUE if timedout, FALSE if another thread resumed it
boolean _lives_proc_thread_wait(lives_proc_thread_t self, uint64_t nanosec, boolean have_lock) {
  if (!nanosec) return TRUE;
  if (self) {
    uint64_t sec;
    int rc = 0;
    struct timespec ts;
    pthread_mutex_t *pause_mutex = pause_mutex = &(THREADVAR(pause_mutex));
    pthread_cond_t *pcond = &(THREADVAR(pcond));
    THREADVAR(sync_ready) = FALSE;
    clock_gettime(CLOCK_REALTIME, &ts);

    nanosec += ts.tv_nsec;
    sec = nanosec / ONE_BILLION;
    nanosec -= sec * ONE_BILLION;
    ts.tv_sec += sec;
    ts.tv_nsec = nanosec;

    if (!have_lock) pthread_mutex_lock(pause_mutex);

    while (!rc && !lives_proc_thread_get_resume_requested(self)
           && !lives_proc_thread_should_cancel(self)) {
      rc = pthread_cond_timedwait(pcond, pause_mutex, &ts);
    }

    if (!have_lock) pthread_mutex_unlock(pause_mutex);

    if (rc == ETIMEDOUT) return TRUE;
  }
  return FALSE;
}


// auto-pausse, returns TRUE if timedout, FALSE if another thread resumed it
boolean lives_proc_thread_wait(lives_proc_thread_t self, uint64_t nanosec) {
  return _lives_proc_thread_wait(self, nanosec, FALSE);
}


LIVES_GLOBAL_INLINE void lives_funcinst_set_attrs(lives_funcinst_t *finst, uint64_t attrs) {
  if (finst) LPT_DATA(finst, func_attrs) = attrs & LIVES_THRDATTR_FUNCINST_MASK;
}


LIVES_GLOBAL_INLINE uint64_t lives_funcinst_get_attrs(lives_funcinst_t *finst) {
  return finst ? LPT_DATA(finst, func_attrs) & LIVES_THRDATTR_FUNCINST_MASK : 0;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_attrs(lives_proc_thread_t lpt, uint64_t attrs) {
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    pthread_rwlock_rdlock(&finst->dispolock);
    if (MODULE_TYPE_IS(finst, LPT)) {
      // if we created the lpt UNQUEUED, it won't yet have a module
      lives_funcinst_set_attrs(finst, attrs);
      pthread_rwlock_unlock(&finst->dispolock);
    } else pthread_rwlock_unlock(&finst->dispolock);
    weed_set_int64_value(lpt, LIVES_LEAF_THRDATTRS, attrs);
  }
}


LIVES_GLOBAL_INLINE  uint64_t lives_proc_thread_get_attrs(lives_proc_thread_t lpt) {
  if (!lpt) return 0;
  lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
  uint64_t attrs = weed_get_int64_value(lpt, LIVES_LEAF_THRDATTRS, NULL);
  if (finst && MODULE_TYPE_IS(finst, LPT)) {
    attrs &= LIVES_THRDATTR_PROC_THREAD_MASK;
    attrs |= lives_funcinst_get_attrs(finst);
  }
  return attrs;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_make_indellible(lives_proc_thread_t lpt, const char *name) {
  weed_plant_t *book = lives_proc_thread_get_book(lpt);
  if (book && weed_plant_has_leaf(book, name))
    weed_leaf_set_undeletable(book, name, TRUE);
}


LIVES_GLOBAL_INLINE weed_error_t lives_proc_thread_set_book(lives_proc_thread_t lpt, weed_plant_t *book) {
  if (lpt) return weed_set_plantptr_value(lpt, LIVES_LEAF_DATA_BOOK, book);
  return WEED_ERROR_NOSUCH_PLANT;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_get_book(lives_proc_thread_t lpt) {
  return lpt ? weed_get_plantptr_value(lpt, LIVES_LEAF_DATA_BOOK, NULL) : NULL;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_has_book(lives_proc_thread_t lpt) {
  return lpt ? weed_plant_has_leaf(lpt, LIVES_LEAF_DATA_BOOK) : FALSE;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_proc_thread_ensure_book(lives_proc_thread_t lpt) {
  if (lpt) {
    weed_plant_t *book = weed_get_plantptr_value(lpt, LIVES_LEAF_DATA_BOOK, NULL);
    if (!book) {
      book = lives_plant_new(LIVES_PLANT_DATA_BOOK);
      lives_proc_thread_set_book(lpt, book);
    }
    return book;
  }
  return NULL;
}


LIVES_LOCAL_INLINE void lives_proc_thread_cleanup_book(lives_proc_thread_t lpt) {
  weed_plant_t *book = lives_proc_thread_get_book(lpt);
  if (book) {
    weed_refcount_inc(book);
    // this works because: with default _weed_plant_free(), any undeletable leaves are retained
    // and plant is not freed. incrementing refcount adds an undeleteable leaf
    // (refcount) if that does not already exist.
    // thus, the plant cannot be freed, but any leaves NOT flagged as undeletable WILL be deleted.
    // - undeleteable leaves include "type", "uid", the refcounter itself
    // and ANY data added as "static", thus the effect will be to delete any data not flagged as "static"
    _weed_plant_free(book);
    weed_refcount_dec(book);
  }
}


static pthread_rwlock_t *lives_proc_thread_get_state_rwlock(lives_proc_thread_t lpt) {
  pthread_rwlock_t *state_rwlock = NULL;
  //if (lives_proc_thread_ref(lpt) > 1)  {
  state_rwlock = (pthread_rwlock_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_RWLOCK, NULL);
  // lives_proc_thread_unref(lpt);
  return state_rwlock;
}


lives_result_t lives_proc_thread_freeze_state(lives_proc_thread_t lpt, boolean rdonly) {
  if (lpt) {
    pthread_rwlock_t *state_rwlock = lives_proc_thread_get_state_rwlock(lpt);
    if (state_rwlock) {
      if (rdonly) pthread_rwlock_rdlock(state_rwlock);
      else pthread_rwlock_wrlock(state_rwlock);
      return LIVES_RESULT_SUCCESS;
    }
    return LIVES_RESULT_INVALID;
  }
  return LIVES_RESULT_ERROR;
}


lives_result_t lives_proc_thread_unfreeze_state(lives_proc_thread_t lpt) {
  if (lpt) {
    pthread_rwlock_t *state_rwlock = lives_proc_thread_get_state_rwlock(lpt);
    if (state_rwlock) {
      pthread_rwlock_unlock(state_rwlock);
      return LIVES_RESULT_SUCCESS;
    }
    return LIVES_RESULT_INVALID;
  }
  return LIVES_RESULT_ERROR;
}


lives_proc_thread_t add_garnish(lives_proc_thread_t lpt) {
  lives_hook_stack_t **hook_stacks;

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_RWLOCK, NULL)) {
    pthread_rwlock_t *state_rwlock = (pthread_rwlock_t *)lives_malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(state_rwlock, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_STATE_RWLOCK, state_rwlock);
  }

  if (!weed_get_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, NULL)) {
    pthread_rwlock_t *destruct_rwlock =
      (pthread_rwlock_t *)lives_malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(destruct_rwlock, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_DESTRUCT_RWLOCK, destruct_rwlock);
  }

  hook_stacks = (lives_hook_stack_t **)lives_calloc(N_HOOK_POINTS, sizeof(lives_hook_stack_t *));

  for (int i = N_NATIVE_HOOKS; i < N_HOOK_POINTS; i++) {
    hook_stacks[i] = LIVES_CALLOC_SIZEOF(lives_hook_stack_t, 1);
    hook_stacks[i]->type = i;
    hook_stacks[i]->parent_stacks = hook_stacks;
    pthread_mutex_init(&hook_stacks[i]->mutex, NULL);
    hook_stacks[i]->owner_act_src_type = ACTION_SOURCE_LPT;
    hook_stacks[i]->owner.lpt = lpt;
    hook_stacks[i]->hsdesc = get_hs_desc(i);
  }

  weed_set_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, (void *)hook_stacks);
  if (weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, NULL) != (void *)hook_stacks) abort();

  lives_proc_thread_include_states(lpt, THRD_STATE_UNQUEUED);

  return lpt;
}


lives_funcinst_t *lives_funcinst_create_va(lives_funcptr_t func,
    const char *fname, int return_type, const char **anames,
    const char *args_fmt, va_list xargs) {
  lives_funcdef_t *fdef = create_funcdef(fname, func, return_type, args_fmt, NULL, 0, 0);
  if (fname) add_quick_fn(func, fname);
  lives_funcinst_t *finst = lives_funcinst_new(fdef);
  if (args_fmt && *args_fmt) {
    funcinst_params_from_vargs(finst, args_fmt, xargs);
    set_args_fmt(finst->params, args_fmt);
    finst->paramnames = anames;
  }
  // still need this to hold retval
  else finst->params = lives_plant_new(LIVES_PLANT_FUNCPARAMS);

  return finst;
}


lives_funcinst_t *_lives_funcinst_create(lives_funcptr_t func, const char *fname, int return_type,
    const char **anames, const char *args_fmt, ...) {
  lives_funcinst_t *finst;
  if (args_fmt && *args_fmt) {
    va_list va;
    va_start(va, args_fmt);
    finst = lives_funcinst_create_va(func, fname, return_type, anames, args_fmt, va);
    va_end(va);
  } else finst = lives_funcinst_create_va(func, fname, return_type, NULL, NULL, NULL);
  return finst;
}


lives_proc_thread_t lives_proc_thread_create_for_funcinst(lives_funcinst_t *finst, uint64_t attrs) {
  lives_proc_thread_t lpt = lives_plant_new(LIVES_PLANT_PROC_THREAD);
  add_to_audit(lpt);
  lives_proc_thread_push_active_funcinst(lpt, finst);
  lives_proc_thread_set_attrs(lpt, attrs);
  if (lpt) add_garnish(lpt);
  return lpt;
}


void finst_module_free(void *module, funcinst_module_type mod_type)  {
  if (!module) return;
  switch (mod_type) {
  case module_lpt:
    break;
  case module_hook_cb:
    pthread_mutex_destroy(&(((hook_cb_data *)module)->mutex));
    break;
  default: break;
  }
  lives_free(module);
}


void *finst_module_new(lives_funcinst_t *finst, funcinst_module_type old, funcinst_module_type new) {
  if (old == new || new == module_any) return NULL;
  void *old_module = finst->module;
  void *module = CALLOC_MODULE(new);
  switch (new) {
  case module_lpt:
    break;
  case module_hook_cb:
    pthread_mutex_init(&(((hook_cb_data *)module)->mutex), NULL);
    break;
  }
  if (old_module) finst_module_free(old_module, old);
  return module;
}


void lives_funcinst_set_disposition(lives_funcinst_t *finst, boolean incl_stacked, funcinst_disposition dis, ...) {
  va_list xargs;
  // WAITING: dispatcher, attrs, runner / NULL
  // ACTIVE: runner

  if (mainw->debug)
    g_print("set disp 1\n");

  if (!finst) return;

  pthread_rwlock_rdlock(&finst->dispolock);
  if (finst->disposition == DISPOSITION_STACKED && !incl_stacked) {
    pthread_rwlock_unlock(&finst->dispolock);
    return;
  }

  if (dis == finst->disposition) {
    pthread_rwlock_unlock(&finst->dispolock);
    return;
  }

  pthread_rwlock_unlock(&finst->dispolock);

  pthread_rwlock_wrlock(&finst->dispolock);

  funcinst_module_type i = finst->mod_type, j = module_type_for_disposition(dis);

  if (i != j && j != module_any) {
    if (finst->module) {
      LIVES_CALLOC_TYPE(funcinst_module_t, modt, 1);
      modt->disposition = finst->disposition;
      modt->mod_type = finst->mod_type;
      modt->module_data = finst->module;
      finst->modules = lives_sync_list_push(finst->modules, modt);
      finst->module = NULL;
    }

    finst->module = finst_module_new(finst, i, j);
  }

  if (finst->module) {
    finst->mod_type = j;
    if (mainw->debug)
      g_print("set disp 3\n");
    switch (dis) {
    case DISPOSITION_CONSUMED:
    case DISPOSITION_CANCELLED:
    case DISPOSITION_ERROR:
      LPT_DATA(finst, runner) = NULL;
      break;

    case DISPOSITION_ACTIVE: {
      // set runner
      va_start(xargs, dis);
      LPT_DATA(finst, runner) = va_arg(xargs, lives_proc_thread_t);
      va_end(xargs);
    } break;

    case DISPOSITION_WAITING: {
      // set dispatcher, finst attrs, runner
      if (mainw->debug)
        g_print("set disp 4\n");
      va_start(xargs, dis);
      LPT_DATA(finst, dispatcher) = va_arg(xargs, lives_proc_thread_t);
      lives_funcinst_set_attrs(finst, va_arg(xargs, uint64_t));
      LPT_DATA(finst, runner) = va_arg(xargs, lives_proc_thread_t);
      va_end(xargs);
      if (mainw->debug) g_print("dispatcher for %p %p set to %p\n", finst, LPT_DATA(finst, runner)
                                  , LPT_DATA(finst, dispatcher));
    } break;

    default: break;
    }
  } else finst->mod_type = module_none;
  finst->disposition = dis;
  if (mainw->debug)
    g_print("set disp 5\n");
  pthread_rwlock_unlock(&finst->dispolock);
}


void lives_funcinst_append_chain(lives_funcinst_t *f1, lives_funcinst_t *f2) {
  if (!f1 || !f2) return;
  lives_sync_list_t *sync_list = lives_sync_list_push(NULL, (void *)f2);
  f1->next = (void *)sync_list;
}


/**
   // a LiVES function call (as opposed to a regular function call) consists of 3 elements:
  // - a funcdef which defines the function to be called, its parameter types and return value type
  // - a funcinst which is created from a funcdef template, and contains the actual parameter values
  // - a proc_thread which "Wraps" a native thread, and is the active element
  //
  // as of present, there are 5 ways to run a funcinst
  // executing ditectly - lives_funcinst_execute
  // create an inactive proc_thread, pushing the funcinst as active, then queueing the proc_thread
  // adding a funcinst as a hook callback
  // pass directly to gui thread via lives_service_call() / main_thread_execute
  // sending an interrupt to another thread and passing the funcinst as thread_data
*/

lives_proc_thread_t _lives_proc_thread_create(timeout_data *to_data, lives_thread_attr_t attrs,
    lives_funcptr_t func, const char *fname,
    int return_type, const char **anames, const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  lives_funcinst_t *finst;
  if (args_fmt && *args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    finst = lives_funcinst_create_va(func, fname, return_type, anames, args_fmt, xargs);
    va_end(xargs);
  } else finst = lives_funcinst_create_va(func, fname, return_type, NULL, NULL, NULL);
  lpt = lives_proc_thread_create_for_funcinst(finst, attrs);
  if (lpt) {
    if (attrs & LIVES_THRDATTR_CREATE_UNQUEUED)
      lives_funcinst_set_attrs(finst, attrs);
    else lives_proc_thread_dispatch(lpt);
  }
  return lpt;
}


lives_proc_thread_t _lives_proc_thread_create_with_timeout(uint64_t to_nsec, lives_cancel_type_t to_ctype,
    boolean to_ign_busy, uint64_t to_min_res,
    lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *funcname, int return_type, const char **anames,
    const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  lives_funcinst_t *finst;
  lives_result_t res = LIVES_RESULT_SUCCESS;
  va_list xargs;

  LIVES_CALLOC_TYPE(timeout_data, to_data, 1);
  to_data->nsec = to_nsec;
  to_data->cancel_type = to_ctype;
  to_data->ign_busy = to_ign_busy;
  to_data->min_resume = to_min_res;
  if (args_fmt && *args_fmt) {
    va_start(xargs, args_fmt);
    finst = lives_funcinst_create_va(func, funcname, return_type, anames, args_fmt, xargs);
    va_end(xargs);
  } else finst = lives_funcinst_create_va(func, funcname, return_type, NULL, NULL, NULL);

  lpt = lives_proc_thread_create_for_funcinst(finst, attrs);
  if (lpt) {
    lives_proc_thread_dispatch(lpt);
    res = lives_proc_thread_guillotine(lpt, to_data);
  }
  lives_free(to_data);

  if (res != LIVES_RESULT_SUCCESS && (to_data->cancel_type == CANCEL_TYPE_KILL
                                      || to_data->cancel_type == CANCEL_TYPE_DONTCARE)) return NULL;

  return lpt;
}


boolean is_fg_thread(void) {
  if (!capable) return TRUE;
  return pthread_equal(pthread_self(), capable->gui_thread);
}


LIVES_LOCAL_INLINE void *add_to_deferral_stack(lives_funcinst_t *finst, uint64_t hook_hints) {
  lives_hook_stack_t **hstacks = self_hook_stacks(LIVES_GUI_HOOK);
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  void *receipt = lives_hook_cb_add_funcinst(hstacks, LIVES_GUI_HOOK, finst, hook_hints);
  if (receipt)
    mainw->all_hstacks =
      lives_list_append_unique(mainw->all_hstacks, hstacks);

  pthread_mutex_unlock(&mainw->all_hstacks_mutex);
  return receipt;
}


LIVES_LOCAL_INLINE void *add_to_fg_deferral_stack(lives_funcinst_t *finst,
    uint64_t hook_hints, uint64_t addmode) {
  void *receipt = NULL;
  hook_hints |= HOOK_CB_FG_THREAD;
  if (addmode == ADDMODE_TEST)
    lives_hook_cb_add(mainw->global_hook_stacks,
                      LIVES_GUI_HOOK, finst, hook_hints, addmode);
  else
    receipt =  lives_hook_cb_add_funcinst(mainw->global_hook_stacks,
                                          LIVES_GUI_HOOK, finst, hook_hints);
  return receipt;
}


LIVES_LOCAL_INLINE void append_all_to_fg_deferral_stack(void) {
  lives_hook_stack_t **hstacks = self_hook_stacks(LIVES_GUI_HOOK);
  lives_hook_stack_t *hstack = hstacks[LIVES_GUI_HOOK];
  LiVESList *cbnext;
  pthread_mutex_t *hmutex = &(hstack->mutex),
                   *fgmutex = &(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);

  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  mainw->all_hstacks =
    lives_list_remove_data(mainw->all_hstacks, hstacks, FALSE);
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);

  pthread_mutex_lock(hmutex);
  pthread_mutex_lock(fgmutex);

  for (LiVESList *cblist = (LiVESList *)hstack->stack; cblist; cblist = cbnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)cblist->data;
    uint64_t cbflags = CL_DATA(finst, cb_flags);
    cbnext = cblist->next;
    lives_hook_cb_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, finst, cbflags, ADDMODE_TRANSFER);
    if (finst->flags & FINST_FLAG_REJECTED)
      remove_from_hstack(hstack, cblist);
    else hstack->stack =
        (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack,
            cblist, FALSE);
  }
  lives_list_free((LiVESList *)hstack->stack);
  hstack->stack = NULL;

  pthread_mutex_unlock(fgmutex);
  pthread_mutex_unlock(hmutex);
}


LIVES_LOCAL_INLINE void prepend_all_to_fg_deferral_stack(void) {
  lives_hook_stack_t **hstacks = self_hook_stacks(LIVES_GUI_HOOK);
  lives_hook_stack_t *hstack = hstacks[LIVES_GUI_HOOK];
  LiVESList *cbprev;

  pthread_mutex_t *hmutex = &(hstack->mutex),
                   *fgmutex = &(mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);

  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  mainw->all_hstacks =
    lives_list_remove_data(mainw->all_hstacks, hstacks, FALSE);
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);

  pthread_mutex_lock(hmutex);
  pthread_mutex_lock(fgmutex);

  for (LiVESList *cblist = lives_list_last((LiVESList *)hstacks[LIVES_GUI_HOOK]->stack);
       cblist; cblist = cbprev) {
    lives_funcinst_t *finst = (lives_funcinst_t *)cblist->data;
    uint64_t cbflags = CL_DATA(finst, cb_flags);
    cbprev = cblist->prev;
    CL_DATA(finst, cb_flags) |= HOOK_CB_PRIORITY;
    lives_hook_cb_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, finst, cbflags, ADDMODE_TRANSFER);
    if (finst->flags & FINST_FLAG_REJECTED)
      remove_from_hstack(hstack, cblist);
    else hstack->stack =
        (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack,
            cblist, FALSE);
  }

  lives_list_free((LiVESList *)hstack->stack);
  hstack->stack = NULL;

  pthread_mutex_unlock(fgmutex);
  pthread_mutex_unlock(hmutex);
}


LIVES_GLOBAL_INLINE lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_STACKS, NULL) : NULL;
}


LIVES_GLOBAL_INLINE lives_hook_stack_t **self_hook_stacks(int hstype) {
  if (hstype < N_NATIVE_HOOKS) return THREADVAR(hook_stacks);
  GET_PROC_THREAD_SELF(self);
  return self ? lives_proc_thread_get_hook_stacks(self) : NULL;
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
  T_RETURN_VAL_IF_RECURSED_WITH_DATA(FALSE, lpt);
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
      lives_funcinst_t *finst;
      lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
      weed_plant_t *databook;
      int64_t state;
      pthread_rwlock_t *state_rwlock;
      lives_hook_stack_t **lpt_hooks = lives_proc_thread_get_hook_stacks(lpt);
      //if (lpt == mainw->debug_ptr) BREAK_ME("lpt free");

      pthread_mutex_lock(&twork_mutex);
      if (lives_proc_thread_get_work(lpt)) {
        // try to remove from pool, but we may be too late
        // however we also lock twork_list, and worker threads should give up if DESTROYING is set
        lpt_remove_from_pool(lpt);
      }
      pthread_mutex_unlock(&twork_mutex);

      pthread_mutex_lock(&mainw->all_hstacks_mutex);
      mainw->all_hstacks =
        lives_list_remove_data(mainw->all_hstacks, lpt_hooks, FALSE);
      pthread_mutex_unlock(&mainw->all_hstacks_mutex);

      T_RECURSE_GUARD_ARM_FOR_DATA(lpt);
      // flush gui hook
      lives_hook_trigger(lpt_hooks, LIVES_GUI_HOOK);
      T_RECURSE_GUARD_END_FOR_DATA(lpt);

      // cannot use include_states here, as that will try to ref / unref lpt !
      state_rwlock = (pthread_rwlock_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_RWLOCK, NULL);
      pthread_rwlock_wrlock(state_rwlock);
      state = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      state |= THRD_STATE_DESTROYED;
      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, state);
      pthread_rwlock_unlock(state_rwlock);

      finst = lives_proc_thread_get_active_funcinst(lpt);

      //lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYED);
      lives_hook_trigger(lpt_hooks, DESTRUCTION_HOOK);

      // clear list of added callback receipts
      flush_cb_added_list(lpt, TRUE);

      if (finst) lives_funcinst_free(finst);

      // this will expire callback receipts for other threads, unless flagged as persistent
      lives_hook_stacks_clear_all(lpt_hooks, N_HOOK_POINTS);

      lives_free(lpt_hooks);

      databook = lives_proc_thread_get_book(lpt);
      if (databook) weed_plant_free(databook);

      if (tdata) {
        // GET_PROC_THREAD_SELF will now return NULL, until we call lives_thread_switch_self()
        tdata->vars.var_proc_thread = NULL;
      }

      remove_from_audit(lpt);

      ////////
      weed_plant_free(lpt);
      ////////

      pthread_rwlock_destroy(state_rwlock);
      lives_free(state_rwlock);

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
// first test if we can add finst to main thread's idle stack

// if so then we check if there is anything in this thread's deferral stack
// if so then we append those callbacks to main thread's stack, and then append finst

// if not we just append finst
// in both cases finst is returned

static void *what_to_wait_for(lives_funcinst_t *finst, uint64_t hints) {
  void *rcpt = NULL;
  add_to_fg_deferral_stack(finst, hints, ADDMODE_TEST);
  if (!(finst->flags & FINST_FLAG_REJECTED)) {
    rcpt = add_to_deferral_stack(finst, hints);
    append_all_to_fg_deferral_stack();
  }
  return rcpt;
}

lives_proc_thread_t lives_funcinst_queue(lives_funcinst_t *finst, uint64_t attrs) {
  // if attrs contain FG_THREAD, funcinst is handed directly to fg_thread
  // else we will create a proc_thread wrapper and dispatch it
  lives_proc_thread_t lpt = NULL;
  GET_PROC_THREAD_SELF(self);
  boolean is_static = !!(finst->flags & FINST_FLAG_STATIC);
  if (!is_static) finst->flags |= FINST_FLAG_STATIC;
  if (attrs & LIVES_THRDATTR_FG_THREAD) {
    // ignored if finst is in a hook_stack
    lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_WAITING, self, attrs, NULL);
    fg_service_call(finst);
  } else {
    // for async_trigger
    lpt = lives_proc_thread_create_for_funcinst(finst, attrs);
    lives_proc_thread_dispatch(lpt);
  }
  if (!is_static) finst->flags &= ~FINST_FLAG_STATIC;
  return lpt;
}


static boolean _main_thread_execute_vargs(lives_funcptr_t func, const char *fname, int return_type,
    void *retloc, const char **anames, const char *args_fmt, va_list xargs) {
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

  lives_funcinst_t *finst;
  uint64_t hook_hints = 0;
  boolean is_fg_service = FALSE;
  boolean is_fg = is_fg_thread();
  boolean retval = TRUE;
  void *rcpt;
  GET_PROC_THREAD_SELF(self);

  // create a lives_proc_thread, which will either be run directly (fg thread)
  // passed to the fg thread
  // or queued for sequential execution (since we need to avoid nesting calls)

  if (args_fmt && *args_fmt) {
    finst = lives_funcinst_create_va(func, fname, return_type, anames, args_fmt, xargs);
  } else finst = lives_funcinst_create_va(func, fname, return_type, NULL, NULL, NULL);

  if (retloc) {
    finst->retloc = retloc;
    finst->flags |= FINST_FLAG_NOFREE_RETLOC;
  }
  if (THREADVAR(fg_service)) {
    is_fg_service = TRUE;
  } else THREADVAR(fg_service) = TRUE;

  if (is_fg) {
    // run direct
    retval = lives_funcinst_execute(finst);
    lives_funcinst_free(finst);
    goto mte_done;
  } else {
    if (THREADVAR(perm_hook_hints))
      hook_hints |= THREADVAR(perm_hook_hints);
    else hook_hints = THREADVAR(hook_hints);

    THREADVAR(hook_hints) = 0;

    //attrs = lives_proc_thread_get_attrs(lpt);

    /* if (attrs & LIVES_THRDATTR_NOTE_TIMINGS) */
    /*   weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks()); */

    if (hook_hints & HOOK_OPT_FG_LIGHT) {
      lives_funcinst_queue(finst, LIVES_THRDATTR_FG_THREAD);
      lives_funcinst_free(finst);
      goto mte_done;
    }

    if (!is_fg_service || (hook_hints & HOOK_CB_BLOCK) || (hook_hints & HOOK_CB_PRIORITY)) {
      // first level service call
      if (!(hook_hints & HOOK_CB_PRIORITY) && FG_THREADVAR(fg_service)) {
        // call is not priority,  and main thread is already performing
        // a service call - in this case we will add the request to main thread's deferral stack

        void *rcpt = what_to_wait_for(finst, hook_hints);
        if (rcpt && (hook_hints & HOOK_CB_BLOCK)) {
          // when we add a funcinst with block, a callback is set to unpause us
          // if rcpt gets any reply other than YES / NO
          // rcpt will be moved from cb_added_list and freed
          // finst will be freed the next time the hook is triggered
          lives_proc_thread_pause();
          lives_funcinst_free(finst);
        }
        goto mte_done;
      }

      // high priority, or gov loop is running or main thread is running a service call
      // we need to wait and action this rather than add it to the queue
      //
      // if priority and not blocking, we will append lpt to the thread stack
      // then prepend our stack to maim thread
      // must not unref lpt, as it is in a hook_stack, it well be unreffed when the closure is freed

      // adds a reply_sent callback so we remove from cb_add_list
      rcpt = add_to_deferral_stack(finst, hook_hints);

      if (rcpt) {
        if (mainw->debug) {
          mainw->debug = FALSE;
          BREAK_ME("here");
        }

        if (!(hook_hints & HOOK_CB_BLOCK)) {
          if (hook_hints & HOOK_CB_PRIORITY) {
            // rcpt will transfer
            prepend_all_to_fg_deferral_stack();
            fg_service_wake();
          } else {
            append_all_to_fg_deferral_stack();
          }
        } else {
          if (hook_hints & HOOK_CB_PRIORITY) {
            lives_proc_thread_trigger_hook(LIVES_GUI_HOOK);
            // gui hook is always oneshot and fg_thread
          } else {
            append_all_to_fg_deferral_stack();
            // when finst is done, resume will be sent
            lives_proc_thread_pause();
          }
        }
        goto mte_done;
      }
    } else {
      // we are already running in a service call, we will add any calls to our own deferral stack to
      // be actioned when we return, since we cannot nest service calls
      // finst here is a freshly created funcinst, it will be stored and then
      // these will be triggered after we return from waiting for the current service call
      // - must not free finst unless rejected
      add_to_deferral_stack(finst, hook_hints);
      flush_cb_added_list(self, FALSE);
    }
    goto mte_done;
  }

mte_done:
  THREADVAR(hook_hints) = hook_hints;

  if (!is_fg_service) THREADVAR(fg_service) = FALSE;
  return retval;
}


boolean _main_thread_execute(lives_funcptr_t func, const char *fname, int return_type,
                             void *retval, const char **anames, const char *args_fmt, ...) {
  boolean bret;
  va_list xargs;
  if (!args_fmt || !*args_fmt) return _main_thread_execute_vargs(func, fname, return_type, retval, NULL, "", NULL);
  va_start(xargs, args_fmt);
  bret = _main_thread_execute_vargs(func, fname, return_type, retval, anames, args_fmt, xargs);
  va_end(xargs);
  return bret;
}


boolean _main_thread_execute_rvoid(lives_funcptr_t func, const char *fname, const char **anames, const char *args_fmt, ...) {
  boolean bret;
  va_list xargs;
  if (!args_fmt || !*args_fmt) return _main_thread_execute_vargs(func, fname, WEED_SEED_VOID, NULL, NULL, "", NULL);
  va_start(xargs, args_fmt);
  bret = _main_thread_execute_vargs(func, fname, WEED_SEED_VOID, NULL, anames, args_fmt, xargs);
  va_end(xargs);
  return bret;
}


boolean _main_thread_execute_pvoid(lives_funcptr_t func, const char *fname, int return_type, void *retloc) {
  return _main_thread_execute(func, fname, return_type, retloc, NULL, "", NULL);
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt) {
  uint64_t tstate = THRD_STATE_INVALID;
  if (lives_proc_thread_freeze_state(lpt, TRUE) == LIVES_RESULT_SUCCESS) {
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    lives_proc_thread_unfreeze_state(lpt);
  }
  return tstate;
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
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    if (finst) return finst->funcdef->funcname;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE lives_funcptr_t lives_proc_thread_get_function(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    if (finst) return finst->funcdef->function;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE funcsig_t lives_proc_thread_get_funcsig(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    if (finst) return finst->funcdef->funcsig;
  }
  return 0;
}


LIVES_GLOBAL_INLINE char *lives_proc_thread_get_args_fmt(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    if (finst) return args_fmt_from_funcsig(finst->funcdef->funcsig);
  }
  return NULL;
}


// check any of states (c.f has_states)
LIVES_GLOBAL_INLINE uint64_t _lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  return lpt ? weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL) & state_bits : THRD_STATE_INVALID;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  pthread_rwlock_t *state_rwlock = lives_proc_thread_get_state_rwlock(lpt);
  if (state_rwlock) {
    uint64_t tstate;
    if (state_bits != THRD_STATE_INVALID) pthread_rwlock_rdlock(state_rwlock);
    tstate = _lives_proc_thread_check_states(lpt, state_bits);
    if (state_bits != THRD_STATE_INVALID) pthread_rwlock_unlock(state_rwlock);
    return tstate;
  }
  return state_bits & THRD_STATE_INVALID;
}


LIVES_GLOBAL_INLINE uint64_t lives_proc_thread_has_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  // test if ALL states in state_bits are set (cf. check_states)
  return lpt && lives_proc_thread_check_states(lpt, state_bits) == state_bits;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_is_original(lives_proc_thread_t lpt) {
  return lpt && lives_proc_thread_get_active_funcinst(lpt)
         == lives_proc_thread_get_initial_funcinst(lpt);
}


LIVES_GLOBAL_INLINE void lives_proc_thread_restore_state(lives_proc_thread_t lpt, uint64_t tstate) {
  // sets the state but without triggering hooks
  if (lives_proc_thread_freeze_state(lpt, FALSE) == LIVES_RESULT_SUCCESS) {
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate);
    lives_proc_thread_unfreeze_state(lpt);
  }
}

uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  uint64_t tstate = THRD_STATE_INVALID;
  if (!lpt) return tstate;
  uint64_t allowed = (uint64_t) -1;
  uint64_t deferred = 0;
  if (!lives_proc_thread_is_original(lpt)) allowed = SUB_INST_STATES;
  //if (lpt == mainw->debug_ptr) BREAK_ME("inc st");
  lives_proc_thread_ref(lpt);
  if (lpt) {
    if (lives_proc_thread_freeze_state(lpt, FALSE) == LIVES_RESULT_SUCCESS) {
      uint64_t masked = 0;
      boolean no_hooks = FALSE;
      tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
      // remove any bits from state_bits already in tstate
      // so we can tell which ones changed
      state_bits &= ~tstate;
      if (tstate & THRD_STATE_IDLING) masked |= THRD_STATE_FINISHED;

      // if the new state includes ERROR or CANCELLED, we need to defer these
      // else we won't be able to run hook callbacks

      state_bits &= ~masked & allowed;

      deferred = state_bits & (THRD_STATE_CANCELLED | THRD_STATE_ERROR);
      state_bits &= ~deferred;

      tstate |= state_bits;

      weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate);
      lives_proc_thread_unfreeze_state(lpt);

      state_bits |= deferred;

      if ((tstate & THRD_BLOCK_HOOKS)
          || (lives_proc_thread_get_attrs(lpt) & LIVES_THRDATTR_NO_HOOKS)) no_hooks = TRUE;

      if (!no_hooks) {
        lives_hook_stack_t **hook_stacks = lives_proc_thread_get_hook_stacks(lpt);
        // only new bits

        if (state_bits & THRD_STATE_PREPARING) {
          lives_hook_trigger(hook_stacks, PREPARING_HOOK);
        }

        if (state_bits & THRD_STATE_RUNNING) {
          lives_hook_trigger(hook_stacks, TX_START_HOOK);
        }

        if (state_bits & THRD_STATE_IDLING) {
          lives_hook_trigger(hook_stacks, IDLE_HOOK);
        }

        if (state_bits & THRD_STATE_BLOCKED) {
          lives_hook_trigger(hook_stacks, TX_BLOCKED_HOOK);
        }

        if (state_bits & THRD_STATE_TIMED_OUT) {
          lives_hook_trigger(hook_stacks, TIMED_OUT_HOOK);
        }

        if (state_bits & THRD_STATE_BUSY) {
          lives_hook_trigger(hook_stacks, BUSY_HOOK);
        }

        if (state_bits & THRD_STATE_ERROR) {
          lives_hook_trigger(hook_stacks, ERROR_HOOK);
        }

        if (state_bits & THRD_STATE_CANCELLED) {
          lives_hook_trigger(hook_stacks, CANCELLED_HOOK);
        }

        if (state_bits & THRD_STATE_COMPLETED) {
          // this hook is triggered when processing is complete
          // if combined with DESTROYING, the next hook point will be DESTROYED and then
          // lpt will be unreffed (*unless it gets a 'hailmary')
          // if combined with IDLING, the next hook depends on external triggers
          // in all other cases the next and final hook will be FINISHED
          /* if (!lives_proc_thread_was_cancelled(lpt)) { */
          /*   lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt); */
          /*   g_print("RUN completed hook for %s!\n", finst ? finst->funcdef->funcname : "?"); */
          /* } */
          lives_hook_trigger(hook_stacks, COMPLETED_HOOK);
        }

        if (state_bits & THRD_STATE_FINISHED)
          lives_hook_trigger(hook_stacks, FINISHED_HOOK);
	// *INDENT-OFF*
      }}
    // *INDENT-ON*

    if (deferred) {
      if (lives_proc_thread_freeze_state(lpt, FALSE) == LIVES_RESULT_SUCCESS) {
        tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
        weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | deferred);
        lives_proc_thread_unfreeze_state(lpt);
      }
    }
    lives_proc_thread_unref(lpt);
  }
  return tstate & allowed;
}


uint64_t lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits) {
  uint64_t allowed = (uint64_t) -1;
  if (!lives_proc_thread_is_original(lpt)) allowed = SUB_INST_STATES;
  if (lives_proc_thread_freeze_state(lpt, FALSE) == LIVES_RESULT_SUCCESS) {
    boolean no_hooks = FALSE;
    uint64_t tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);

    if ((tstate & THRD_BLOCK_HOOKS)
        || (lives_proc_thread_get_attrs(lpt) & LIVES_THRDATTR_NO_HOOKS)) no_hooks = TRUE;

    state_bits &= tstate & allowed;
    tstate &= ~state_bits;
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate);
    lives_proc_thread_ref(lpt);
    lives_proc_thread_unfreeze_state(lpt);

    if (state_bits & THRD_STATE_BUSY) {
      lives_cancel_point;
      if (!no_hooks)
        lives_hook_trigger(lives_proc_thread_get_hook_stacks(lpt), UNBUSY_HOOK);
    }

    return tstate & allowed;
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
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_initial_funcinst(lpt);
    if (finst) {
      pthread_rwlock_rdlock(&finst->dispolock);
      if (finst->disposition == DISPOSITION_STACKED) {
        pthread_rwlock_unlock(&finst->dispolock);
        return TRUE;
      }
      pthread_rwlock_unlock(&finst->dispolock);
    }
  }
  return FALSE;
}


// check if thread finished normally
LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_finished(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_FINISHED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_check_completed(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_COMPLETED)));
}


// check if thread is idling (only set if idlefunc was queued at least once already)
LIVES_GLOBAL_INLINE boolean lives_proc_thread_paused_idling(lives_proc_thread_t lpt) {
  return lives_proc_thread_has_states(lpt, THRD_STATE_IDLING | THRD_STATE_PAUSED);
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
  return lpt && (LPT_THREADVAR_GET(lpt, thrdnative_flags) & THRDNATIVE_CAN_INTERRUPT)
         ? TRUE : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_set_interruptable(boolean state) {
  if (state) {
    thrd_signal_unblock(LIVES_INTERRUPT_SIG);
    THREADVAR(thrdnative_flags) |= THRDNATIVE_CAN_INTERRUPT;
  } else {
    thrd_signal_block(LIVES_INTERRUPT_SIG);
    THREADVAR(thrdnative_flags) &= ~THRDNATIVE_CAN_INTERRUPT;
  }
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


LIVES_GLOBAL_INLINE void lives_proc_thread_set_cancellable(lives_proc_thread_t lpt) {
  // actually sets funcinst
  if (lpt) lives_proc_thread_set_attrs(lpt, lives_proc_thread_get_attrs(lpt)
                                         | LIVES_THRDATTR_CANCELLABLE);
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_ignore_syncpts(lives_proc_thread_t lpt, boolean ignore) {
  if (lpt) lives_proc_thread_set_attrs(lpt, ignore ? lives_proc_thread_get_attrs(lpt) | LIVES_THRDATTR_IGNORE_SYNCPTS
                                         : lives_proc_thread_get_attrs(lpt) & ~LIVES_THRDATTR_IGNORE_SYNCPTS);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_ignore_syncpts(lives_proc_thread_t lpt) {
  return lpt ? !!(lives_proc_thread_get_attrs(lpt) & LIVES_THRDATTR_IGNORE_SYNCPTS) : FALSE;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_loveliness(lives_proc_thread_t lpt, double how_lovely_it_is) {
  if (lpt) {
    if (how_lovely_it_is < 1.) how_lovely_it_is = 1.;
    if (how_lovely_it_is > MAX_LOVELINESS) how_lovely_it_is = MAX_LOVELINESS;
    LPT_THREADVAR_SET(lpt, loveliness, how_lovely_it_is);
  }
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancellable(lives_proc_thread_t lpt) {
  return lpt && (lives_proc_thread_get_attrs(lpt) & LIVES_THRDATTR_CANCELLABLE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_was_cancelled(lives_proc_thread_t lpt) {
  if (lives_proc_thread_is_queued(lpt)) check_pool_threads(FALSE);
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_CANCELLED)));
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_pauseable(lives_proc_thread_t lpt, boolean state) {
  // actually sets funcinst
  if (lpt) lives_proc_thread_set_attrs(lpt, lives_proc_thread_get_attrs(lpt)
                                         | LIVES_THRDATTR_PAUSEABLE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_pauseable(lives_proc_thread_t lpt) {
  return lpt && (lives_proc_thread_get_attrs(lpt) & LIVES_THRDATTR_PAUSEABLE);
}



LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_cancel(lives_proc_thread_t lpt, boolean dontcare) {
  if (lives_proc_thread_ref(lpt) > 1) {
    lives_proc_thread_include_states(lpt, THRD_STATE_CANCEL_REQUESTED);
    lives_proc_thread_exclude_states(lpt, THRD_STATE_PAUSE_REQUESTED);
    if (dontcare) lives_proc_thread_dontcare(lpt);
    lives_proc_thread_unref(lpt);
    return !!(dontcare);
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_cancel_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_CANCEL_REQUESTED)));
}


boolean _lives_proc_thread_request_resume(lives_proc_thread_t lpt, boolean have_lock, boolean ensure) {
  // to ensure proper synchronisation, - set resume_requested, and wake thread
  // and wait for paused state to go away.
  // - target thread wakes, removes paused state, but waits for resume_req. to be cleared
  // once paused is cleared, remove resume_req, allowing target to unblock
  // if ensure is set, this implies that the target is expected to pause and we want to prevent it
  // from doing so. In this case we will leave resume request in its state
  if (lives_proc_thread_ref(lpt) > 1) {
    lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
    if (tdata) {
      pthread_cond_t *pcond;
      pthread_mutex_t *pause_mutex = &tdata->vars.var_pause_mutex;
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
  return _lives_proc_thread_request_resume(lpt, FALSE, FALSE);
}


boolean lives_proc_thread_force_resume(lives_proc_thread_t lpt) {
  return _lives_proc_thread_request_resume(lpt, FALSE, TRUE);
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_resume_requested(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_RESUME_REQUESTED)));
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_resume(lives_proc_thread_t self) {
  if (self) {
    lives_hook_stack_t **hstacks = self_hook_stacks(RESUMING_HOOK);
    // need to remove idling and unqueued in case this is an idle proc thread
    // which is paused / idling
    lives_proc_thread_exclude_states(self, THRD_STATE_PAUSED | THRD_STATE_UNQUEUED |
                                     THRD_STATE_IDLING | THRD_STATE_AUTO_PAUSED |
                                     THRD_STATE_RESUME_REQUESTED);
    lives_hook_trigger(hstacks, RESUMING_HOOK);
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_request_pause(lives_proc_thread_t lpt) {
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
      lives_proc_thread_cancel();
      return FALSE;
    } else {
      pthread_cond_t *pcond = &(THREADVAR(pcond));
      lives_hook_stack_t **hook_stacks = self_hook_stacks(PAUSED_HOOK);
      lives_proc_thread_exclude_states(self, THRD_STATE_PAUSE_REQUESTED);

      lives_hook_trigger(hook_stacks, PAUSED_HOOK);

      lives_proc_thread_include_states(self, THRD_STATE_PAUSED);
      if (!lives_proc_thread_get_resume_requested(self)) {
        if (!lives_proc_thread_get_cancel_requested(self)) {
          if (!have_lock) pthread_mutex_lock(pause_mutex);
          THREADVAR(sync_ready) = FALSE;
          while (!THREADVAR(sync_ready)) {
            pthread_cond_wait(pcond, pause_mutex);
          }
          lives_proc_thread_exclude_states(self, THRD_STATE_SYNC_WAITING);
          if (!have_lock) pthread_mutex_unlock(pause_mutex);
        }
      }
      lives_proc_thread_resume(self);
      if (lives_proc_thread_get_cancel_requested(self)) {
        if (have_lock) pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_cancel();
        return FALSE;
      }
      return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_pause(void) {
  GET_PROC_THREAD_SELF(self);
  return _lives_proc_thread_pause(self, FALSE);
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


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_proc_thread_get_dispatcher(lives_proc_thread_t lpt) {
  // returns the proc_thread_ which queued active_funcinst
  // for stacked funcinst, we cannot return the adder, since there may be multiple adders
  // (if the funcinst expelled others)
  if (lpt) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    if (finst && MODULE_TYPE_IS(finst, LPT)) return LPT_DATA(finst, dispatcher);
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_sync_idx(uint64_t idx) {THREADVAR(sync_idx) = idx;}


LIVES_GLOBAL_INLINE volatile uint64_t lives_proc_thread_get_sync_idx(lives_proc_thread_t lpt) {
  return LPT_THREADVAR_GET(lpt, sync_idx);
}


LIVES_GLOBAL_INLINE lives_result_t lives_proc_thread_sync_with_timeout(lives_proc_thread_t lpt,
    uint64_t sync_idx, int mm_op, int64_t timeout_nsec) {
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

  MSGMODE_ON(DEBUG);
  GET_PROC_THREAD_SELF(self);
  uint64_t attrs = lives_proc_thread_get_attrs(self);
  if (attrs & LIVES_THRDATTR_IGNORE_SYNCPTS) return LIVES_RESULT_SUCCESS;
  if (sync_idx == 0) sync_idx = -1;
  d_print_debug("syncwith: %p says: start sync with %p, sync identifier is %d\n", self, lpt, sync_idx);
  if (lives_proc_thread_ref(lpt) > 1)  {
    d_print_debug("syncwith: %p says: got ref on other\n", self);
    if (lpt != self) {
      volatile uint64_t osync_idx;
      boolean gotmatch = FALSE;
      if (lives_proc_thread_check_finished(lpt)) {
        d_print_debug("syncwith: %p says: other thread finished without us !\n", self);
        if (lives_proc_thread_was_cancelled(lpt))
          d_print_debug("syncwith: %p says: other thread was cancelled\n", self);
        if (lives_proc_thread_had_error(lpt))
          d_print_debug("syncwith: %p says: other thread got an error\n", self);
        return LIVES_RESULT_FAIL;
      }
      pthread_mutex_t *opause_mutex = LPT_THREADVAR_GETp(lpt, pause_mutex),
                       *pause_mutex = &(THREADVAR(pause_mutex));

      if (!opause_mutex) {
        d_print_debug("syncwith: no pause mutex !! %p %p %p\n", lpt, mainw->def_lpt, get_thread_data_for_lpt(lpt));
        BREAK_ME("nopause");
        lives_proc_thread_unref(lpt);
        return LIVES_RESULT_ERROR;
      }

      d_print_debug("syncwith: %p says: got pause mutex of other (%p)\n", self, opause_mutex);
      d_print_debug("syncwith: %p says: getting lock on other..", self);

      // (A)
      pthread_mutex_lock(opause_mutex);
      d_print_debug("succeeded\n");

      d_print_debug("syncwith: %p says: advertising sync idx %d\n", self, sync_idx);
      lives_proc_thread_set_sync_idx(sync_idx);

      d_print_debug("syncwith: %p says: checking sync idx of other...\n", self);
      osync_idx = lives_proc_thread_get_sync_idx(lpt);
      d_print_debug("got value %d\n", osync_idx);

      if (osync_idx == sync_idx) {
        gotmatch = TRUE;
        d_print_debug("syncwith: %p says: idx matches !\n", self);
      } else d_print_debug("syncwith: %p says:  no idx match, will wait\n", self);

      d_print_debug("syncwith: %p says: unlocking pause mutex of other\n", self);
      pthread_mutex_unlock(opause_mutex);

      d_print_debug("syncwith: %p says: get lock on self pause mutex...\n", self);
      // (B)
      pthread_mutex_lock(pause_mutex);

      d_print_debug("OK\n");

      if (!gotmatch) {
        // check again but now with lock - either other has not yet reached (A), will block at (A)
        // or has passed (B)
        // if it passed (B), we will now get a match,
        osync_idx = lives_proc_thread_get_sync_idx(lpt);
        if (osync_idx == sync_idx) {
          gotmatch = TRUE;
          d_print_debug("syncwith: %p says: recheched sync idx, "
                        "now we DID get match\n", self);
        }
      }

      if (gotmatch) {
        boolean resrq = FALSE;
        // we got a match, then we wait for other to either pause / wait,
        // or to reset its sync-idx. While waitng we reset our sync_idx, so
        // if th eother notices this it can also reset its sync_idx
        lives_proc_thread_set_sync_idx(0);
        //pthread_mutex_unlock(pause_mutex);
        //
        while (1) {
          osync_idx = lives_proc_thread_get_sync_idx(lpt);
          if (!osync_idx) {
            d_print_debug("syncwith: %p says:  other reset sync_idx, assume synced\n", self);
            goto synced;
          }
          pthread_mutex_lock(opause_mutex);
          if (!resrq && lives_proc_thread_is_paused(lpt)) {
            d_print_debug("syncwith: %p says: other is paused, requesting resume\n", self);
            lives_proc_thread_set_sync_idx(sync_idx);
            _lives_proc_thread_request_resume(lpt, TRUE, FALSE);
            d_print_debug("syncwith: %p says: waiting for other to reset sync_idx\n", self);
            resrq = TRUE;
          }
          pthread_mutex_unlock(opause_mutex);
          lives_microsleep;
        }
      }
      // non-match, pause / wait
      if (!timeout_nsec) {
        d_print_debug("syncwith: %p says: pausing...\n", self);
        _lives_proc_thread_pause(self, TRUE);
        pthread_mutex_unlock(pause_mutex);
        d_print_debug("syncwith: %p says: resumed, checking for idx match\n", self);
        if (lives_proc_thread_get_sync_idx(lpt) == sync_idx) {
          //pthread_mutex_unlock(pause_mutex);
          goto synced;
        }
        d_print_debug("syncwith: %p says: no match after resuming "
                      "- wrong thread woke us ?\n", self);
        lives_proc_thread_set_sync_idx(0);
        pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_unref(lpt);
        return LIVES_RESULT_FAIL;
      } else {
        d_print_debug("syncwith: waiting\n");
        if (_lives_proc_thread_wait(self, timeout_nsec, TRUE)) {
          // timed out waiting
          d_print_debug("syncwith: timed out waiting\n");
          if (lives_proc_thread_get_sync_idx(lpt) == sync_idx) {
            // pthread_mutex_unlock(pause_mutex);
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
          //pthread_mutex_unlock(pause_mutex);
          goto synced;
        }
        d_print_debug("no match after resuming - wrong thread woke us ?\n");
        lives_proc_thread_set_sync_idx(0);
        pthread_mutex_unlock(pause_mutex);
        lives_proc_thread_unref(lpt);
        MSGMODE_OFF(DEBUG);
        return LIVES_RESULT_FAIL;
      }

synced:
      // if here, either - requested resume, or nrither paused, or thread resumed us
      // or timed out but sync_idx matched
      d_print_debug("syncwith: %p says:  SYNCED with %p!!\n", self, lpt);
      pthread_mutex_unlock(pause_mutex);
      lives_proc_thread_set_sync_idx(0);
      pthread_mutex_lock(opause_mutex);
      // lock to ensure other read our sync_idx before unlocking
      pthread_mutex_unlock(opause_mutex);
      lives_proc_thread_unref(lpt);
      d_print_debug("syncwith: DONE !!\n");
      MSGMODE_OFF(DEBUG);
      return LIVES_RESULT_SUCCESS;
      /* mismatch: */
      /*   lives_proc_thread_error(self, 0, "sync_idx mismatch, wating for %d and found %d\n", sync_idx, osync_idx); */
      /*   lives_proc_thread_unref(lpt); */
      /*   return LIVES_RESULT_ERROR; */
    } else d_print_debug("sync with self !\n");

    lives_proc_thread_unref(lpt);
    MSGMODE_OFF(DEBUG);
    return LIVES_RESULT_SUCCESS;
  }
  MSGMODE_OFF(DEBUG);
  return LIVES_RESULT_FAIL;
}


LIVES_GLOBAL_INLINE lives_result_t lives_proc_thread_sync_with(lives_proc_thread_t lpt, uint64_t sync_idx, int mm_op) {
  if (mainw->debug) g_print("async with\n");
  return lives_proc_thread_sync_with_timeout(lpt, sync_idx, mm_op, 0);
}


/////////////////////////////////////////////////////////////////////

static boolean timeout_busy(void *self, void *data) {
  timeout_data *to_data = (timeout_data *)data;
  if (!to_data->ign_busy) {
    to_data->nsec = lives_alarm_disarm();
    to_data->nsec -= lives_get_session_time();
    to_data->is_busy = TRUE;
  }
  return TRUE;
}


static boolean timeout_unbusy(void *self, void *data) {
  timeout_data *to_data = (timeout_data *)data;
  if (!to_data->dontcare) {
    if (to_data->is_busy) {
      to_data->nsec += lives_get_session_time();
      to_data->is_busy = FALSE;
      if (to_data->nsec < to_data->min_resume)
        to_data->nsec = to_data->min_resume;
      lives_alarm_set_timeout(to_data->nsec);
    }
  }
  return TRUE;
}


static boolean timeout_dontcare(void *lpt, void *data) {
  timeout_data *to_data = (timeout_data *)data;
  uint64_t attrs = lives_proc_thread_get_attrs(lpt);
  if (attrs & LIVES_THRDATTR_DONTCARE) {
    lives_proc_thread_set_attrs(lpt, attrs | LIVES_THRDATTR_DONTCARE);
    if (to_data->is_busy) lives_alarm_disarm();
    to_data->dontcare = TRUE;
    return FALSE;
  }
  return TRUE;
}


static lives_result_t _lives_proc_thread_wait_finished(lives_proc_thread_t lpt, timeout_data * to_data) {
  if (!lpt) return LIVES_RESULT_INVALID;
  void *rcpt1 = NULL, *rcpt2 = NULL, *rcpt3 = NULL;
  boolean is_fg = is_fg_thread();
  lives_result_t res = LIVES_RESULT_SUCCESS;
  int count = 0;
  if (to_data) {
    //to_data is set when called from the guillotinee
    //add callbacks for busy / unbusy
    //and a callback which should trigger in case dontcare is set
    rcpt1 = lives_proc_thread_add_hook_cb(lpt, BUSY_HOOK, 0, timeout_busy, (void *)to_data);
    rcpt2 = lives_proc_thread_add_hook_cb(lpt, UNBUSY_HOOK, 0, timeout_unbusy, (void *)to_data);

    //THREADVAR(hook_hints) |= HOOK_OPT_REMOVE_ON_FALSE;

    //this is a special case (cascaded fram data  hook) for the target item "attrs" in a proc_thrad
    //params are tgt_item  old_val, new_val
    //however we dont want it trigger on every change so we can add a condition to trigger
    // TODO
    /* lives_funcinst_t *fi3 = */

    // for now we fake the condiiton and the hook is only called for dontcare
    rcpt3 = lives_proc_thread_add_hook_cb(lpt, ATTRS_UPDATED_HOOK, 0, timeout_dontcare, (void *)to_data);

    /* char **cond = lives_cond_create(COND_BEGIN. COND_BITS_SET, COND_UINT64_VAR(@new_value), */
    /* 				    COND_UINT64_CONST(LIVES_THRDATTR_DONTCARE), COND_END); */

    /* lives_callback_add_trigger_condition(fi3 ,3, cond); */

    THREADVAR(hook_hints) = 0;;
    if (!lives_proc_thread_is_busy(lpt))
      lives_alarm_set_timeout(to_data->nsec);
    else to_data->is_busy = TRUE;
  }

  while (1) {
    if (to_data && to_data->dontcare) {
      res = LIVES_RESULT_FAIL;
      break;
    }
    if (to_data && (!to_data->is_busy || to_data->ign_busy)
        && lives_alarm_triggered()) {
      res = LIVES_RESULT_TIMEDOUT;
      break;
    }

    if (lives_proc_thread_check_finished(lpt)) {
      if (lives_proc_thread_was_cancelled(lpt)
          || lives_proc_thread_had_error(lpt))
        res = LIVES_RESULT_ERROR;
      break;
    }
    if (is_fg && ++count == 1000) {
      count = 0;
      fg_service_fulfill();
    }
    lives_microsleep;
  }
  if (to_data) {
    lives_alarm_disarm();
    if (!to_data->dontcare && rcpt1) {
      lives_hook_cb_remove(rcpt1);
      lives_hook_cb_remove(rcpt2);
    }
    if (rcpt3) lives_hook_cb_remove(rcpt3);
  }
  return res;
}


lives_result_t lives_proc_thread_wait_finished(lives_proc_thread_t lpt) {
  return _lives_proc_thread_wait_finished(lpt, NULL);
}


#define _join(lpt, stype, nullv) {					\
    return (lpt && lives_proc_thread_wait_finished(lpt) == LIVES_RESULT_SUCCESS \
	    && weed_plant_has_leaf(lpt, _RV_))				\
      ? weed_get_##stype##_value(lpt, _RV_, NULL) : nullv;}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_join_void(lives_proc_thread_t lpt) {
  return (lpt && lives_proc_thread_wait_finished(lpt) == LIVES_RESULT_SUCCESS) ? TRUE : FALSE;
}

LIVES_GLOBAL_INLINE allvalues_t *lives_proc_thread_join_any(lives_proc_thread_t lpt) {
  if (lpt && lives_proc_thread_wait_finished(lpt) == LIVES_RESULT_SUCCESS) {
    allvalues_t *allvp = allvalues_from_leaf(NULL, lpt, _RV_);
    return allvp;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_join_int(lives_proc_thread_t lpt)  _join(lpt, int, 0)
LIVES_GLOBAL_INLINE double lives_proc_thread_join_double(lives_proc_thread_t lpt) _join(lpt, double, 0.)
LIVES_GLOBAL_INLINE int lives_proc_thread_join_boolean(lives_proc_thread_t lpt)  _join(lpt, boolean, FALSE)
LIVES_GLOBAL_INLINE int64_t lives_proc_thread_join_int64(lives_proc_thread_t lpt) _join(lpt, int64, 0)
LIVES_GLOBAL_INLINE char *lives_proc_thread_join_string(lives_proc_thread_t lpt) _join(lpt, string, NULL)
LIVES_GLOBAL_INLINE weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t lpt) _join(lpt, funcptr, NULL)
LIVES_GLOBAL_INLINE void *lives_proc_thread_join_voidptr(lives_proc_thread_t lpt) _join(lpt, voidptr, NULL)
LIVES_GLOBAL_INLINE weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t lpt) _join(lpt, plantptr, NULL)

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
  // we presume entering here that the lpt hasnt been freed
  // what we want to avoid is that the we set the flag right after the runner thread has finished
  // - either it can free lpt and the we will segfault reading flags
  // or else we can read flags and think it is still running
  // so the order has to be - lock a mutex -> around check flags to set state
  // check if state is finsihed - if so, unref the lpt
  // otherwise leave it
  // return TRUE if we set the state

  if (lives_proc_thread_ref(lpt) > 1) {
    if (!lives_proc_thread_is_stacked(lpt) && lpt != mainw->def_lpt) {
      if (lives_proc_thread_check_finished(lpt)) lives_proc_thread_unref(lpt);
      else {
        uint64_t attrs = lives_proc_thread_get_attrs(lpt);
        lives_proc_thread_set_attrs(lpt, attrs | LIVES_THRDATTR_DONTCARE);
        lives_hook_trigger(lives_proc_thread_get_hook_stacks(lpt), ATTRS_UPDATED_HOOK);
      }
      lives_proc_thread_unref(lpt);
      return TRUE;
    }
    lives_proc_thread_unref(lpt);
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
// for cancel_immdeiate, there will be no final state, but thread_exit will be triggered
//
// if the state is CANELLED + COMPLETED + FINISHED + UNUQEUEUD (without paused or idling)
// - this means lpt was cancelled before being queued
//
//
static uint64_t lives_proc_thread_set_final_state(lives_proc_thread_t lpt) {
  uint64_t attrs;
  lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);

  attrs = lives_proc_thread_get_attrs(lpt);

  lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);
  lives_proc_thread_include_states(lpt, THRD_STATE_COMPLETED);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_BUSY);

  if (finst) {
    if (lives_proc_thread_had_error(lpt))
      lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_ERROR);
    else if (lives_proc_thread_was_cancelled(lpt))
      lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_CANCELLED);
  }

  lives_proc_thread_pop_active_funcinst();

  // something can happen to finst
  /* if (finst->disposition != DISPOSITION_STACKED) */
  /*   lives_funcinst_free(finst); */

  if (attrs & LIVES_THRDATTR_DONTCARE)
    lives_proc_thread_include_states(lpt, THRD_STATE_DESTROYING);

  // caller should set FINISHED state before unreffing
  return lives_proc_thread_get_state(lpt);
}


// signals,  error handling and cancelling /////////////

LIVES_GLOBAL_INLINE boolean lives_proc_thread_had_error(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_ERROR)));
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_errnum(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_ERRNUM, NULL) : 0;
}


LIVES_GLOBAL_INLINE const char *lives_proc_thread_get_errmsg(lives_proc_thread_t lpt) {
  return lpt ? weed_get_const_string_value(lpt, LIVES_LEAF_ERRMSG, NULL) : NULL;
}


LIVES_GLOBAL_INLINE const char *lives_proc_thread_get_file_ref(lives_proc_thread_t lpt) {
  return lpt ? weed_get_const_string_value(lpt, LIVES_LEAF_FILE_REF, NULL) : NULL;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_line_ref(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_LINE_REF, NULL) : 0;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_errsev(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_ERRSEV, NULL) : 0;
}


LIVES_GLOBAL_INLINE boolean lives_proc_thread_get_signalled(lives_proc_thread_t lpt) {
  return (lpt && (lives_proc_thread_has_states(lpt, THRD_STATE_SIGNALLED)));
}


static void lives_proc_thread_signalled(int sig, siginfo_t *si, void *uc) {
  //a pthread, or proc_thread via the native thread, cna set itself interruptable
  // via LIVES_INTERRUPT_SIG
  // when this happens it will trigger the native INTERRUPT_HOOK.
  // for
  GET_PROC_THREAD_SELF(self);
  weed_plant_t *plant = weed_get_plantptr_value(self, LIVES_LEAF_SIGNAL_DATA, NULL);
  if (!plant) return;
  if (IS_LIVES_PLANT(plant)) {
    int64_t subtype = lives_plant_get_subtype(plant);
    switch (subtype) {
    case LIVES_PLANT_VALUE: {
      allvalues_t *avp = allvalues_from_leaf(NULL, plant, LIVES_LEAF_VALUE);
      int dtl = weed_get_int_value(plant, "val_dtl", NULL);
      switch (dtl) {
      case STRUCT_ADAPTOR: {
        const char *structype = weed_get_const_string_value(plant, WEED_LEAF_NAME, NULL);
        if (!lives_strcmp(structype, "lives_funcinst_t")) {
          lives_funcinst_t *finst = *(lives_funcinst_t **)avp->values.V;
          allvalue_free(avp);
          lives_funcinst_execute(finst);
          weed_plant_free(plant);
          return;
        }
        break;
      }
      default: break;
      }
      break;
    }
    default: break;
    }
  }

  /* case SIG_ACT_REPORT: { */
  /*   // describe self */
  /*   if (tdata) { */
  /*     lives_proc_thread_t active = tdata->vars.var_active_lpt; */
  /*     g_print("Hello, I am thread;\n%s\n", get_thread_id(tdata->uid)); */
  /*     if (active) { */
  /* 	g_print("I am currently busy with proc_thread %p:\n", active); */
  /* 	g_print("\t%s\n", lives_proc_thread_show_func_call(active)); */
  /* 	lpt_desc_state(active); */
  /* 	if (tdata->vars.var_func_stack && tdata->vars.var_func_stack->list) { */
  /* 	  char *fname = lives_sync_list_pop(&tdata->vars.var_func_stack); */
  /* 	  if (fname) */
  /* 	    g_print("Last func entry recorded was %s\n", fname); */
  /* 	} */
  /*     } else g_print("I am curently hanging out in the thread pool\n"); */
  /*   } */
  /* } */
  /*   break; */
  /* case SIG_ACT_CMD: { */
  /*   while (tdata->vars.var_simple_cmd_list) { */
  /*     LiVESList *list = lives_sync_list_pop(&tdata->vars.var_simple_cmd_list); */
  /*     if (list) { */
  /* 	lives_proc_thread_t lpt = (lives_proc_thread_t)list->data; */
  /* 	// we can execute some simple tasks, but from a signal handler, only asyn safe functions may be used */
  /* 	// this is the most basic method of running an lpt */
  /* 	if (lpt) do_call(lpt); */
  /* 	lives_list_free(list); */
  /*     } */
  /*   } */
  /* } */
  /*   break; */
  /* default: break; */
  /* } */
}


lives_result_t lives_proc_thread_try_interrupt(lives_proc_thread_t lpt, weed_plant_t *data) {
  if (!lpt) return LIVES_RESULT_ERROR;
  GET_PROC_THREAD_SELF(self);
  if (lpt == self) return LIVES_RESULT_INVALID;
  pthread_t pth = lives_proc_thread_get_pthread(lpt);
  if (!pth) return LIVES_RESULT_INVALID;
  if (!lives_proc_thread_can_interrupt(lpt)) return LIVES_RESULT_FAIL;
  lives_proc_thread_set_signalled(lpt, LIVES_INTERRUPT_SIG, data);
  pthread_kill(pth, LIVES_INTERRUPT_SIG);
  return LIVES_RESULT_SUCCESS;
}



LIVES_GLOBAL_INLINE boolean lives_proc_thread_set_signalled(lives_proc_thread_t lpt, int signum, weed_plant_t *data) {
  if (!lpt) return FALSE;
  if (lives_proc_thread_freeze_state(lpt, FALSE) == LIVES_RESULT_SUCCESS) {
    lives_thread_data_t *mydata = (lives_thread_data_t *)data;
    uint64_t tstate;
    if (mydata) mydata->signum = signum;
    tstate = weed_get_int64_value(lpt, LIVES_LEAF_THRD_STATE, NULL);
    weed_set_int64_value(lpt, LIVES_LEAF_THRD_STATE, tstate | THRD_STATE_SIGNALLED);
    weed_set_plantptr_value(lpt, LIVES_LEAF_SIGNAL_DATA, data);
    lives_proc_thread_unfreeze_state(lpt);
  }
  return TRUE;
}


// calls pthread_cancel on underlying thread
// - cleanup function casues the thread to the normal post cleanup, so lives_proc_thread_join_*
// will work as normal (though the values returned will not be valid)
// however this should still be called, and the proc_thread freed as normal
// (except for auto / dontcare proc_threads)
// there is a small chance the cancellation could occur either before or after the function is called
LIVES_GLOBAL_INLINE lives_result_t lives_proc_thread_cancel_immediate(lives_proc_thread_t lpt,
    lives_cancel_type_t cancel_type) {
  void *res;
  if (!lpt) return LIVES_RESULT_ERROR;

  lives_proc_thread_request_cancel(lpt, (cancel_type == CANCEL_TYPE_DONTCARE));
  if (cancel_type == CANCEL_TYPE_SOFT || cancel_type == CANCEL_TYPE_DONTCARE)
    return LIVES_RESULT_SUCCESS;

  _lives_millisleep(100);

  if (lives_proc_thread_was_cancelled(lpt)) return LIVES_RESULT_SUCCESS;

  // if that fails, try an inrerrupt
  pthread_t pth = lives_proc_thread_get_pthread(lpt);
  if (!pth) return LIVES_RESULT_FAIL;

  if (lives_proc_thread_can_interrupt(lpt)) {
    pthread_kill(pth, LIVES_INTERRUPT_SIG);
    if (cancel_type == CANCEL_TYPE_INTERRUPT) return LIVES_RESULT_SUCCESS;
    _lives_millisleep(100);
    if (lives_proc_thread_was_cancelled(lpt)) return LIVES_RESULT_SUCCESS;
  }

  if (cancel_type == CANCEL_TYPE_INTERRUPT) return LIVES_RESULT_FAIL;

  // if that fails, cancel the pthread
  pthread_cancel(pth);
  pthread_join(pth, &res);
  if (res != PTHREAD_CANCELED) return LIVES_RESULT_FAIL;
  return LIVES_RESULT_SUCCESS;
}


static lives_result_t lives_proc_thread_guillotine(lives_proc_thread_t lpt, timeout_data * to_data) {
  if (to_data->dontcare) return LIVES_RESULT_CANCELLED;
  lives_result_t res = _lives_proc_thread_wait_finished(lpt, to_data);
  if (res != LIVES_RESULT_TIMEDOUT) return res;

  if (to_data->dontcare) return res;

  if (to_data->cancel_type == CANCEL_TYPE_DONTCARE) {
    lives_proc_thread_request_cancel(lpt, TRUE);
    return res;
  }

  lives_proc_thread_cancel_immediate(lpt, to_data->cancel_type);
  return res;
}


void lives_make_errmsg_full(lives_proc_thread_t lpt, const char *errfile, int errline, int sev, int errnum,
                            const char *xerrmsg) {
  if (!errfile)
    lives_snprintf(errmsg, 1024, "lives proc thread %p, (%s)\ngot a %s error\n"
                   "Error code %d: %s\n",
                   lpt, get_lpt_id(lpt), sev == 2 ? "major" : sev == 3 ? "critical"
                   : sev == 4 ? "fatal" : sev == 5 ? "deadly" : "unknown", errnum, xerrmsg);
  else
    lives_snprintf(errmsg, 1024, "lives proc thread %p, (%s)\ngot a %s error "
                   "at line %d in file %s\nerror code %d: %s\n",
                   lpt, get_lpt_id(lpt), sev == 2 ? "major" : sev == 3 ? "critical"
                   : sev == 4 ? "fatal" : sev == 5 ? "deadly" : "unknown",
                   errline, errfile, errnum, xerrmsg);
}


void lpt_error_handle(lives_proc_thread_t lpt, int sev) {
  // this is called from the signal handler
  int errnum = 0;
  if (lpt) {
    // pop module
    if (sev == LPT_ERR_MINOR) {
      // dont report minor errors
      lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
      pthread_rwlock_rdlock(&finst->dispolock);
      if (finst->disposition == DISPOSITION_STACKED) {
        pthread_rwlock_unlock(&finst->dispolock);
        lives_funcinst_send_replies(finst, LIVES_REPLY_ERROR);
      } else {
        pthread_rwlock_unlock(&finst->dispolock);
        lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_ERROR);
        return;
      }
    }

    if (lives_proc_thread_had_error(lpt)) {
      if (!*errmsg) {
        errnum = lives_proc_thread_get_errnum(lpt);
        const char *xerrmsg = lives_proc_thread_get_errmsg(lpt);
        int errline = lives_proc_thread_get_line_ref(lpt);
        const char *errfile = lives_proc_thread_get_file_ref(lpt);
        lives_make_errmsg_full(lpt, errfile, errline, sev, errnum, xerrmsg);
      }
      fprintf(stderr, "%s\n", errmsg);
      switch (sev) {
      case (LPT_ERR_MAJOR) :
        fprintf(stderr, "task was cancelled\n");
        break;
      case (LPT_ERR_CRITICAL) :
        fprintf(stderr, "sending signal 11\n");
        break;
      case (LPT_ERR_FATAL) :
        fprintf(stderr, "aborting\n");
        break;
      case (LPT_ERR_DEADLY) :
        fprintf(stderr, "bye\n");
        _exit(errnum);
        break;
      default: return;
      }
    }

    fprintf(stderr, "The current thread was running ");
    fprintf(stderr, "%s\n", lives_proc_thread_show_func_call(lpt));
  }

  if (mainw) {
    fprintf(stderr, "\nApplication status was %d\n", lives_get_status());

    if (!mainw->multitrack && LIVES_IS_PLAYING) {
      char *bgstr;
      if (mainw->blend_file) bgstr = lives_strdup_printf(", background clip was %d", mainw->blend_file);
      else bgstr = lives_strdup(", no background clip");
      fprintf(stderr, "LiVES was playing file %d%s\n", mainw->playing_file, bgstr);
      if (mainw->plan_cycle) {
        fprintf(stderr, "Plan cycle was active, with state %lu\n", mainw->plan_cycle->state);
        display_plan(mainw->plan_cycle);
      }
    }
    print_diagnostics(DIAG_ALL);
    fprintf(stderr, "Total run time: %s\n", lives_format_timing_string(lives_get_session_time() / ONE_BILLION_DBL));
  } else {
    fprintf(stderr, "mainw is NULL\n");
  }
}


lives_result_t _lives_proc_thread_cancel(char *file_ref, int line_ref) {
  lives_funcinst_t *finst;

  GET_PROC_THREAD_SELF(self);
  if (self == mainw->debug_ptr) BREAK_ME("cancelled");

  finst = lives_proc_thread_get_active_funcinst(self);

  if (finst) {
    // pop module - if running async, the runneer can cancel itself
    pthread_rwlock_rdlock(&finst->dispolock);
    if (finst->disposition == DISPOSITION_STACKED) {
      pthread_rwlock_unlock(&finst->dispolock);
      abort();
      char *msg = LSPF("in file %s, line %d:\n"
                       "lives_proc_thread cancel() cannot be used in hook callbacks !",
                       file_ref, line_ref);
      LIVES_WARN(msg);
      lives_free(msg);
      return LIVES_RESULT_FAIL;
    }
    pthread_rwlock_unlock(&finst->dispolock);
  }

  if (lives_proc_thread_was_cancelled(self)) {
    LIVES_WARN("proc_thread cancelled > 1 times !");
    return LIVES_RESULT_SUCCESS;
  }

  lives_proc_thread_set_line_ref(self, line_ref, FALSE);
  lives_proc_thread_set_file_ref(self, file_ref, FALSE);

  lives_proc_thread_include_states(self, THRD_STATE_CANCELLED);
  lives_proc_thread_exclude_states(self, THRD_STATE_CANCEL_REQUESTED);

  if (finst && LPT_DATA(finst, lj_stack)) {
    jmp_buf *env = (jmp_buf *)lives_sync_list_peek(LPT_DATA(finst, lj_stack));
    if (env) siglongjmp(*env, THRD_STATE_CANCELLED >> 32);
    return LIVES_RESULT_MU;
  }
  LIVES_WARN("canclled proc_thread had no longjmp destination");
  return LIVES_RESULT_ERROR;
}


LIVES_GLOBAL_INLINE void lives_proc_thread_set_line_ref(lives_proc_thread_t self, int line, boolean over) {
  if (over || !weed_plant_has_leaf(self, LIVES_LEAF_LINE_REF))
    weed_set_int_value(self, LIVES_LEAF_LINE_REF, line);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_file_ref(lives_proc_thread_t self, char *file, boolean over) {
  if (over || !weed_plant_has_leaf(self, LIVES_LEAF_FILE_REF))
    weed_set_const_string_value(self, LIVES_LEAF_FILE_REF, file);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_errnum(lives_proc_thread_t self, int num) {
  weed_set_int_value(self, LIVES_LEAF_ERRNUM, num);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_errmsg(lives_proc_thread_t self, const char *msg) {
  weed_set_const_string_value(self, LIVES_LEAF_ERRMSG, msg);
}

LIVES_GLOBAL_INLINE void lives_proc_thread_set_errsev(lives_proc_thread_t self, int sev) {
  weed_set_int_value(self, LIVES_LEAF_ERRSEV, sev);
}


LIVES_GLOBAL_INLINE boolean _lives_proc_thread_error_full(lives_proc_thread_t self, char *file_ref, int line_ref,
    int errnum, int severity, const char *xerrmsg) {
  const char *xfile_ref = file_ref;

  if (severity == LPT_ERR_DEADLY) _exit(errnum);
  if (severity == LPT_ERR_FATAL) LIVES_FATAL(errmsg);

  // pop module

  if (severity == LPT_ERR_MINOR) {
    // for minor errors we just cancel the active func_indt
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(self);

    lives_make_errmsg_full(self, file_ref, line_ref, severity, errnum, xerrmsg);
    d_print_debug("%s\n", errmsg);
    lives_memset(errmsg, 0, 1);
    pthread_rwlock_rdlock(&finst->dispolock);
    if (finst->disposition == DISPOSITION_STACKED) {
      pthread_rwlock_unlock(&finst->dispolock);
      lives_funcinst_send_replies(finst, LIVES_REPLY_ERROR);
      return TRUE;
    }
    pthread_rwlock_unlock(&finst->dispolock);
    lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_ERROR);
    if (LPT_DATA(finst, lj_stack)) {
      jmp_buf *env = (jmp_buf *)lives_sync_list_peek(LPT_DATA(finst, lj_stack));
      if (env) siglongjmp(*env, THRD_STATE_CANCELLED >> 32);
    }
    LIVES_WARN("cancelled proc_thread had no longjmp destination");
    return FALSE;
  }

  if (!lives_proc_thread_check_states(self, THRD_STATE_ERROR)) {
    lives_proc_thread_include_states(self, THRD_STATE_ERROR);

    lives_proc_thread_set_line_ref(self, line_ref, TRUE);
    lives_proc_thread_set_file_ref(self, file_ref, TRUE);
    lives_proc_thread_set_errnum(self, errnum);
    lives_proc_thread_set_errsev(self, severity);
    lives_proc_thread_set_errmsg(self, xerrmsg);
  } else {
    if (xerrmsg) g_printerr("%s", xerrmsg);
    //errmsg = lives_proc_thread_get_errmsg(self);
    line_ref = lives_proc_thread_get_line_ref(self);
    xfile_ref = lives_proc_thread_get_file_ref(self);
    errnum = lives_proc_thread_get_errnum(self);
    //severity = lives_proc_thread_get_errsev(self);
  }

  if (severity == LPT_ERR_CRITICAL) {
#ifdef _FILE_REF_
#undef _FILE_REF_
#endif
#ifdef _LINE_REF_
#undef _LINE_REF_
#endif

#define _FILE_REF_ xfile_ref
#define _LINE_REF_ line_ref

    LIVES_CRITICAL(lives_proc_thread_get_errmsg(self));

#undef _FILE_REF_
#undef _LINE_REF_

#ifdef _ORIG_FILE_REF_
#define _FILE_REF_ _ORIG_FILE_REF_
#endif
#ifdef _ORIG_LINE_REF_
#define _LINE_REF_ _ORIG_LINE_REF_
#endif
  }

  if (severity == LPT_ERR_MAJOR) {
    // for amjor errors, in case we are running a chain we will j
    lives_sync_list_t *finstlist;
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(self);
    // unnwind to chain head
    while ((finstlist = finst->prev)) {
      pthread_rwlock_rdlock(&finst->dispolock);
      if (finst->disposition == DISPOSITION_STACKED) {
        pthread_rwlock_unlock(&finst->dispolock);
        lives_funcinst_send_replies(finst, LIVES_REPLY_ERROR);
      } else {
        pthread_rwlock_unlock(&finst->dispolock);
        lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_ERROR);
      }
      finst = lives_sync_list_peek(finstlist);
    }

    pthread_rwlock_rdlock(&finst->dispolock);
    if (finst->disposition != DISPOSITION_STACKED)  {
      pthread_rwlock_unlock(&finst->dispolock);
      if (LPT_DATA(finst, lj_stack)) {
        jmp_buf *env = (jmp_buf *)lives_sync_list_pop_to_last(&LPT_DATA(finst, lj_stack));
        if (env) siglongjmp(*env, THRD_STATE_CANCELLED >> 32);
      }
      LIVES_WARN("cancelled proc_thread had no longjmp destination");
    } else pthread_rwlock_unlock(&finst->dispolock);
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean _lives_proc_thread_error(char *file_ref, int line_ref,
    int errnum, int severity, const char *fmt, ...) {
  GET_PROC_THREAD_SELF(self);
  char *errmsg = NULL;
  if (fmt && *fmt) {
    va_list vargs;
    va_start(vargs, fmt);
    errmsg = lives_strdup_vprintf(fmt, vargs);
    va_end(vargs);
  }
  return _lives_proc_thread_error_full(self, file_ref, line_ref, errnum, severity, errmsg);
}


// funcinst /////////////


LIVES_GLOBAL_INLINE int lives_proc_thread_get_chain_idx(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_CHAIN_IDX, NULL) : 0;
}


LIVES_GLOBAL_INLINE int lives_proc_thread_get_stack_depth(lives_proc_thread_t lpt) {
  return lpt ? weed_get_int_value(lpt, LIVES_LEAF_STACK_DEPTH, NULL) : 0;
}


static lives_result_t _lives_funcinst_execute(void) {
  // execute a funcinst, which must be pushed as active

  // we can arrive here by several routes:
  // - a pool thread picked up a work packet which wrapped a proc_thread
  // - a thread has triggered hook callbacks
  // - the gui thread is servicing a bg thread request
  // - a thread decides to spontaneously run a funcinst

  // for queued proc threads we usually have a single funcinst which holds param data and
  // attributes (thread attrs, not object attributes !). the attibutes are per function
  // and split with the proc_thread. We can also have a sequence (chain) of funcinst
  //
  // for hook callbacks (stacked func instances) we do not need to join them, as this is handled in the trigger function
  // (exception - async hook stacks DO need to be joined)
  // for queued funcinst, if attr DONTCARE has been set, both proc thread and funcinst are freed when completed
  // to get the return value in this case, set finst->retloc to point to a variable to receive data
  //
  // for async callbacks, these are a mixture of the above, the funcinst is held in the hook stack until triggered
  // at which point a proc_thread is created for it and dispatched to worker pool. at that time, the disposition is set to
  // waiting, this would normally be the first disposition, but in this case since we already had stacked, the old
  // disposition is pushed to finst->dispositions, along with the module / module_type (CL_DATA) and we get an LPT_DATA
  // module. The lpt will be joined later (LPT_DATA(finst, runner)) and there the funcinst will not be freed but the old
  // disposition / module / module_type will be popped back.

  // we can arrive recursively if the proc thread is running a hook callback or simply executin another funcinst
  // for such calls, the new funcinst is pushed to the chain level sync_list, and becomes the new active funcinst
  // on return, the old value is popped,
  //

  int depth, cidx;
  lives_funcinst_t *finst;
  lives_sync_list_t *sync_list;

  GET_PROC_THREAD_SELF(self);
  LIVES_ASSERT(self);

  depth = lives_proc_thread_get_stack_depth(self);

  finst = lives_proc_thread_get_active_funcinst(self);

  while (1) {
    if (!lives_proc_thread_was_cancelled(self)
        && !lives_proc_thread_had_error(self)) {
      // go through cb_added_list and increment refcount for existing items
      // after returning, we will remove any newly added callbacks
      cleanup_self_receipts();
      ref_cb_added_list();
      weed_set_int_value(self, LIVES_LEAF_STACK_DEPTH, ++depth);

      lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_ACTIVE, self);

      ///////////////////////
      call_funcsig(self);

      weed_set_int_value(self, LIVES_LEAF_STACK_DEPTH, --depth);
      unref_cb_added_list();

      flush_cb_added_list(self, FALSE);
      ///////////////////
    }

    if (lives_proc_thread_had_error(self)
        || lives_proc_thread_was_cancelled(self))
      break;

    // remove any non static values from the data book
    lives_proc_thread_cleanup_book(self);

    cidx = lives_proc_thread_get_chain_idx(self);

    if (!finst->next) break;

    lives_sync_list_t *xsync_list = lives_proc_thread_get_active_finstlist(self);

    lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_IDLING);

    weed_set_int_value(self, LIVES_LEAF_CHAIN_IDX, cidx + 1);

    if (depth) {
      finst->depth = depth;
      weed_set_int_value(self, LIVES_LEAF_STACK_DEPTH, 0);
    }

    sync_list = finst->next;
    lives_proc_thread_set_active_finstlist(self, sync_list);
    finst = lives_sync_list_peek(sync_list);
    finst->prev = xsync_list;
  }

  while (finst->prev) {
    if (!lives_proc_thread_was_cancelled(self)
        && !lives_proc_thread_had_error(self)) {
      lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_CONSUMED);
    }
    sync_list = finst->prev;
    lives_sync_list_pop_to_last(&sync_list);
    finst = lives_sync_list_peek(sync_list);
    lives_proc_thread_set_active_finstlist(self, sync_list);
    lives_proc_thread_set_active_funcinst(finst);
  }

  depth = finst->depth;

  if (depth) weed_set_int_value(self, LIVES_LEAF_STACK_DEPTH, --depth);

  weed_set_int_value(self, LIVES_LEAF_STACK_DEPTH, depth);
  weed_set_int_value(self, LIVES_LEAF_CHAIN_IDX, 0);

  // we end as we started with same active funcinst
  // caller needs to pop active finstlist, then possibly free the funcinst
  // for stacked funcinst, we do not free automatically
  // for lpt funcinst, the pop / free happens when joined, or automatically if dontcare
  // and if no more funcinst, auto will unref lpt

  pthread_rwlock_rdlock(&finst->dispolock);
  if (finst->disposition == DISPOSITION_ACTIVE) {
    pthread_rwlock_unlock(&finst->dispolock);
    lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_CONSUMED);
  } else pthread_rwlock_unlock(&finst->dispolock);

#if USE_RPMALLOC
  rpmalloc_thread_collect();
#endif

  return LIVES_RESULT_SUCCESS;
}


lives_result_t lives_funcinst_execute(lives_funcinst_t *finst) {
  // this is for funcimsts which are executed directly
  // via an existing proc_thread
  lives_result_t res;
  GET_PROC_THREAD_SELF(self);
  if (!self) {
    g_print("naked func call %s\n", finst->funcdef->funcname);
    abort();
  }
  LIVES_ASSERT(self);

  uint64_t state = lives_proc_thread_get_state(self);

  // if we are executing a hook callback then finst will have the stacked disposition
  // if this is the gui thread running a service call, instead this depends -
  // for fg_light calls, the funcinst is never stacked and has the waiting disposition
  // fot other service calls, these are stacked
  lives_proc_thread_push_active_funcinst(self, finst);

  lives_proc_thread_include_states(self, THRD_STATE_SWAPPED);

  res = _lives_funcinst_execute();

  lives_proc_thread_exclude_states(self, THRD_STATE_SWAPPED);

  lives_proc_thread_pop_active_funcinst();

  if (lives_proc_thread_was_cancelled(self))
    if (!lives_proc_thread_is_original(self)) lives_proc_thread_cancel();

  lives_proc_thread_restore_state(self, state);

  return res;
}


// this is for toplevel proc_threads which always start by being dispatched
// the active funcinst ( == initial funcinst) was set
lives_result_t lives_proc_thread_execute(lives_proc_thread_t lpt) {
  lives_result_t res =  _lives_funcinst_execute();

  // this is done so that funcinst can be detatched, and the value read from
  // proc_thread when joined
  // cf retloc for non dispatched finst
  // TODO: finst->params should be merged with local data_book
  // the book can be shared with lpt
  /// - for bare funcinsts, rv in data book can be bound to retloc
  if (res == LIVES_RESULT_SUCCESS) {
    lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
    if (finst && finst->params)
      lives_leaf_dup(lpt, finst->params, _RV_);
  }
  // the pool thread handles setting final state, and freeing the funcinst
  // (unless funcinst was flagged as static)

  return res;
}


// dispatch a lives_proc_thread to the worker queue
// the proc_thread should have an active_funcinst which will be executed as the payload
// Unless dispatched as DONTCARE, or DONTCARE is set during execution, the lpt must be joined
// according to return type
// and then should be unreffed if no longer needed
boolean lives_proc_thread_dispatch(lives_proc_thread_t lpt) {
  GET_PROC_THREAD_SELF(self);
  thrd_work_t *mywork;

  uint64_t state = lives_proc_thread_get_state(lpt);
  lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
  uint64_t attrs = lives_proc_thread_get_attrs(lpt);

  // TRUE here allows overriding of DISPOSITION_STACKED for async hook callbacks
  // the prior disposition is pushed and will be popped back when the callback is async_join()ed

  // some of the lpt attrs are passed on to funcinst, e.g. cancellable, pausable
  lives_funcinst_set_disposition(finst, TRUE, DISPOSITION_WAITING, self, attrs, lpt);

  // if we have cancel_requested, this means the proc_thread was
  // cancelled BEFORE being queued,
  // (this can be done even if the proc_thread is not explicitly 'cancellable',
  if (lives_proc_thread_should_cancel(lpt)) {
    if (!lives_proc_thread_was_cancelled(lpt))
      lives_proc_thread_include_states(lpt, THRD_STATE_CANCELLED);

    state = lives_proc_thread_set_final_state(lpt);
    if (!(attrs & LIVES_THRDATTR_DONTCARE)) {
      // notify successful completion
      if (state & THRD_STATE_COMPLETED)
        lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
    } else lives_proc_thread_unref(lpt);
    return FALSE;
  }

  /// tell the thread to clean up after itself [but it won't delete lpt]

  attrs |= (LIVES_THRDATTR_AUTODELETE | LIVES_THRDATTR_IS_PROC_THREAD);

  //if (!mainw->debug_ptr) mainw->debug_ptr = lpt;

  if (attrs & LIVES_THRDATTR_START_CANCELLABLE)
    lives_proc_thread_set_cancellable(lpt);

  if (attrs & LIVES_THRDATTR_START_PAUSEABLE)
    lives_proc_thread_set_pauseable(lpt, TRUE);

  // STATE CHANGE -> unqueued / idling -> queued
  state &= ~(THRD_STATE_IDLING | THRD_STATE_COMPLETED | THRD_STATE_FINISHED
             | THRD_STATE_CANCELLED | THRD_STATE_UNQUEUED);

  state |= THRD_STATE_QUEUED;

  lives_proc_thread_set_state(lpt, state);

  lives_proc_thread_set_attrs(lpt, attrs);

  // add the work to the pool
  mywork = lives_thread_create(NULL, attrs, (lives_thread_func_t)lives_proc_thread_execute, (void *)lpt);

  if (!mywork) {
    // proc_thread was cancelled before being added to the pool
    return FALSE;
  }

  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);

  if (attrs & LIVES_THRDATTR_WAIT_START) {
    // WAIT_START: caller waits for thread to run or finish
    lives_millisleep_until_zero(mywork->flags & LIVES_THRDFLAG_WAIT_START);
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
  if (state & THRD_OPT_CAN_INTERRUPT)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "will exit when finished");
  if (state & THRD_BLOCK_HOOKS)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "hooks blocked");
  if (state & THRD_STATE_EXTERN)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "is external");

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
static volatile int npoolthreads, rnpoolthreads, nthrds_needed;
static pthread_t **poolthrds;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t all_tdata_rwlock;
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
  if (!lpt || (lives_proc_thread_is_unqueued(lpt) && lpt != mainw->def_lpt
               && !lives_proc_thread_check_states(lpt, THRD_STATE_EXTERN))) return NULL;
  tdata = lives_proc_thread_get_thread_data(lpt);
  if (!tdata)
    // ignore subrd check, since that requires tdata !
    lives_microsleep_until_nonzero(lives_proc_thread_check_finished(lpt)
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


lives_proc_thread_t lives_thread_get_proc_thread(void) {
  return THREADVAR(proc_thread);
}


void lives_thread_set_proc_thread(lives_proc_thread_t lpt) {THREADVAR(proc_thread) = lpt;}


static pthread_mutex_t blmutex = PTHREAD_MUTEX_INITIALIZER;

static LiVESList *blocklist = NULL;

static void add_to_blocklist(pthread_t self) {
  pthread_mutex_lock(&blmutex);
  blocklist = lives_list_prepend(blocklist, (void *)self);
  pthread_mutex_unlock(&blmutex);
}

static void rem_from_blocklist(pthread_t self) {
  pthread_mutex_lock(&blmutex);
  blocklist = lives_list_remove_data(blocklist, (void *)self, FALSE);
  pthread_mutex_unlock(&blmutex);
}

static boolean is_in_blocklist(void *self) {
  return !!lives_list_find_by_data(blocklist, self);
}


static void lives_thread_data_destroy(void *data) {
  lives_thread_data_t *tdata = (lives_thread_data_t *)data;
  add_to_blocklist(pthread_self());

  lives_hook_stack_t **hook_stacks = tdata->vars.var_hook_stacks;

  if (is_fg_thread()) {
    lives_hook_trigger(mainw->global_hook_stacks, FATAL_HOOK);
    if (mainw->record) backup_recording(NULL, NULL);
    if (mainw->multitrack) mainw->multitrack->idlefunc = 0;
  }

  if (hook_stacks) {
    // this will force other lpts to remove their pointers to callbacks in our stacks
    lives_hook_stacks_clear_all(hook_stacks, N_NATIVE_HOOKS);
  }

  pthread_rwlock_wrlock(&all_tdata_rwlock);
  all_tdatas = lives_list_remove_data(all_tdatas, tdata, FALSE);
  pthread_rwlock_unlock(&all_tdata_rwlock);

  if (tdata->vars.var_func_stack) lives_sync_list_free(tdata->vars.var_func_stack, TRUE);

  lives_list_free((LiVESList *)tdata->vars.var_trest_list);

  pthread_mutex_destroy(&tdata->vars.var_pause_mutex);
  pthread_cond_destroy(&tdata->vars.var_pcond);

  lives_free(tdata);

#if USE_RPMALLOC
  if (rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_collect();
    rpmalloc_thread_finalize(1);
  }
#endif
  rem_from_blocklist(pthread_self());
}

void pthread_cleanup_func(void *args) {
  // if the main_thread is ever cancelled by pthread_cancel, this will be triggered, and any hook callbacks
  // added, to THREAD_EXIT_HOOK will be triggered
  // this is also called after gtk_main() exits, thus on normal exit, any threads still running lpts
  // with hook callbacks in the main thread THREAD_EXIT_HOOK should flush their ext_cb lists, or manually remove the
  // callbacks, unless they need informing when this happens

  // this is called BEFORE thread data destroy

  lives_hook_trigger(THREADVAR(hook_stacks), THREAD_EXIT_HOOK);
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
    thrd_signal_block(LIVES_TICKER_SIG);
    thrd_signal_block(LIVES_TIMER_SIG);

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

    for (int i = 0; i < N_NATIVE_HOOKS; i++) {
      tdata->vars.var_hook_stacks[i] =
        (lives_hook_stack_t *)lives_calloc(1, sizeof(lives_hook_stack_t));
      pthread_mutex_init(&tdata->vars.var_hook_stacks[i]->mutex, NULL);
      tdata->vars.var_hook_stacks[i]->type = i;
      tdata->vars.var_hook_stacks[i]->parent_stacks = tdata->vars.var_hook_stacks;
      tdata->vars.var_hook_stacks[i]->owner_act_src_type = ACTION_SOURCE_THREAD;
      tdata->vars.var_hook_stacks[i]->owner.thread = pthread_self();
      tdata->vars.var_hook_stacks[i]->hsdesc = get_hs_desc(i);
    }

    if (slot_id < 0) {
      if (pthread_equal(pthread_self(), capable->main_thread)) {
        tdata->thrd_type = THRD_TYPE_MAIN;
      } else tdata->thrd_type = THRD_TYPE_EXTERN;
      tdata->vars.var_hs_flag_mask = HS_MASK_GUI;
    } else {
      tdata->thrd_type = THRD_TYPE_WORKER;
      tdata->vars.var_hs_flag_mask = HS_MASK_WORKER;
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
      if (tdata->thrd_type == THRD_TYPE_MAIN) {
        lives_snprintf(tdata->vars.var_origin, 128, "%s", "LiVES Main Thread");
        tdata->vars.var_guictx = g_main_context_default();
      } else {
        lives_snprintf(tdata->vars.var_origin, 128, "%s", "LiVES Worker Thread");
      }

      tdata->vars.var_rowstride_alignment = RS_ALIGN_DEF;
      tdata->vars.var_last_sws_block = -1;
    } else
      tdata->vars.var_blocked_limit = BLOCKED_LIMIT;

    lives_icap_init(&tdata->vars.var_intentcap);
    thread_signal_establish(LIVES_INTERRUPT_SIG, lives_proc_thread_signalled);
    thrd_signal_block(LIVES_INTERRUPT_SIG);

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

  if (tdata->thrd_type != THRD_TYPE_WORKER) return tdata;

  pthread_cleanup_push(pthread_cleanup_func, tdata);

  thrdpool(tdata);

  /* if (tdata->vars.var_guictx != g_main_context_default()) */
  /*   g_main_context_iteration(tdata->vars.var_guictx, TRUE); */

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

  if (is_in_blocklist((void *)pthread_self())) return NULL;

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
  for (LiVESList *list = (LiVESList *)twork_list; list; list = list->next) {
    thrd_work_t *mywork = (thrd_work_t *)list->data;
    if (mywork && mywork->lpt == lpt) {
      lives_thread_free((lives_thread_t *)list);
      return TRUE;
    }
  }
  return FALSE;
}


#define should_skip(lpt, work)						\
  (lpt ? ((lives_proc_thread_will_destroy(lpt) || lives_proc_thread_should_cancel(lpt) \
	   || lives_proc_thread_had_error(lpt)) ? TRUE : FALSE) : FALSE)


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

  twork_list = (volatile LiVESList *)list->next;

  if ((LiVESList *)twork_last == list) twork_last = twork_last->prev;
  if (twork_list) twork_list->prev = NULL;

  // removed from list
  if (!(mywork = (thrd_work_t *)list->data)) {
    ntasks--;
    list->next = list->prev = NULL;
    lives_thread_free((lives_thread_t *)list);
    pthread_mutex_unlock(&twork_mutex);
    return FALSE;
  }

  /* if (lpt == mainw->debug_ptr) */
  /*   g_print("GOT PCUSTCOL\n"); */

  if ((lpt = mywork->lpt)) {
    if (lives_proc_thread_ref(lpt) < 2) {
      ntasks--;
      list->next = list->prev = NULL;
      lives_thread_free((lives_thread_t *)list);
      pthread_mutex_unlock(&twork_mutex);
      return FALSE;
    }
  }

  pthread_mutex_unlock(&twork_mutex);
  list->next = list->prev = NULL;

  if (lpt) {
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
    lives_proc_thread_set_thread_data(lpt, get_thread_data());
    lives_thread_set_proc_thread(lpt);
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

  was_skipped = FALSE;

skip_over:

  if (lpt) {
    uint64_t state;
    // detatch funcinst from lpt
    //lives_proc_thread_pop_active_funcinst();
    state = lives_proc_thread_set_final_state(lpt);

    if (!(state & THRD_STATE_DESTROYING)) {
      // notify successful completion
      // LIVES_ASSERT(state & THRD_STATE_COMPLETED, "WARNING - %p failed to get completed state !!\n", lpt);
      // ptoc_thread can be joined and unreffed as soon as we set finished
      // however, we still hold a ref on lpt, so it is safe to access it until we unref it
      lives_proc_thread_include_states(lpt, THRD_STATE_FINISHED);
    } else {
      lives_proc_thread_unref(lpt);
      //g_print("Will destroy %p\n", lpt);
    }

    lives_proc_thread_set_thread_data(lpt, NULL);
    lives_thread_set_proc_thread(NULL);

    lives_proc_thread_unref(lpt);
  }

  //mainw->debug_ptr = NULL;

  pthread_mutex_lock(&twork_mutex);
  ntasks--;

  if (mywork->flags & LIVES_THRDFLAG_AUTODELETE) {
    lives_thread_free((lives_thread_t *)list);
  } else {
    if (was_skipped) mywork->skipped = TRUE;
    else mywork->done = tdata->uid;
  }

  pthread_mutex_unlock(&twork_mutex);

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
            if (nthrds_needed < npoolthreads) {
              pthread_t *myslot = (pthread_t *)STEAL_POINTER(poolthrds[tdata->slot_id]);
              // slot can now be reused
              npoolthreads--;
              lives_free(myslot);
              tdata->exited = TRUE;
              pthread_mutex_unlock(&twork_mutex);
              pthread_mutex_unlock(&pool_mutex);
              break;
            } else nthrds_needed--;
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
  }
  //  g_print("thrd %d (0x%lx) killed\n", tid, pself);

  return FALSE;
}


void lives_threadpool_init(void) {
  pthread_rwlock_init(&all_tdata_rwlock, NULL);
  rnpoolthreads = npoolthreads = nthrds_needed = MINPOOLTHREADS;
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
    thrd_work_t *work = (thrd_work_t *)STEAL_POINTER(thread->data);
    uint64_t flags = 0;

    if (thread->prev || thread->next || (lives_thread_t *)twork_last == thread
        || (lives_thread_t *)twork_list == thread) {
      if (thread->prev) {
        thread->prev->next = thread->next;
        if ((lives_thread_t *)twork_last == thread)
          twork_last = (volatile LiVESList *)thread->prev;
        thread->prev = NULL;
      } else if ((lives_thread_t *)twork_list == thread)
        twork_list = NULL;

      if (thread->next) {
        thread->next->prev = thread->prev;
        if ((lives_thread_t *)twork_list == thread)
          twork_list = (volatile LiVESList *)thread->next;
        thread->next = NULL;
      } else if ((lives_thread_t *)twork_last == thread)
        twork_last = NULL;
    }

    if (work) {
      flags = work->flags;
      lives_free(work);
    }

    if (!(flags & LIVES_THRDFLAG_NOFREE_LIST)) lives_list_free_1(thread);
  }
}


#define POOL_CHK_THRESH (5. * TICKS_PER_SECOND_DBL)

void check_pool_threads(boolean important) {
  static ticks_t last_check_ticks = 0;
  if (!important && lives_get_relative_ticks_lax(last_check_ticks) < POOL_CHK_THRESH) return;
  last_check_ticks = lives_get_current_ticks_lax();

  pthread_mutex_lock(&pool_mutex);

  if (ntasks > nthrds_needed) nthrds_needed = ntasks;

  while (ntasks > npoolthreads && npoolthreads < rnpoolthreads) {
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
    int extrs = MAX(MINPOOLTHREADS >> 1, ntasks - rnpoolthreads);
    g_print("Adding %d poolthreads\n", extrs);
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
  thrd_work_t *work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  lives_proc_thread_t lpt = NULL;

  work->func = func;
  work->attrs = attrs;
  work->arg = arg;
  work->flags = LIVES_THRDFLAG_QUEUED_WAITING;
  work->caller = THREADVAR(uid);

  if (threadptr) work->flags |= LIVES_THRDFLAG_NOFREE_LIST;

  if (attrs & LIVES_THRDATTR_IS_PROC_THREAD) {
    lpt = (lives_proc_thread_t)arg;
    work->lpt = lpt;

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

  if (attrs & LIVES_THRDATTR_PRIORITY) {
    twork_list = lives_list_prepend((LiVESList *)twork_list, (void *)work);
    if (!twork_last) twork_last = twork_list;
    if (threadptr) *threadptr = (lives_thread_t *)twork_list;
  } else {
    twork_list = lives_list_append((LiVESList *)twork_list, (void *)work);
    if (!twork_last) twork_last = twork_list;
    else twork_last = twork_last->next;
    if (threadptr) *threadptr = (lives_thread_t *)twork_last;
  }

  ntasks++;

  pthread_mutex_unlock(&twork_mutex);

  if (!(attrs & LIVES_THRDATTR_FAST_QUEUE)) check_pool_threads(TRUE);

  return work;
}


uint64_t lives_thread_join(lives_thread_t *thread, void **retval) {
  if (!thread) return 0;

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

  pthread_mutex_lock(&twork_mutex);
  lives_thread_free(thread);
  pthread_mutex_unlock(&twork_mutex);

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


LIVES_LOCAL_INLINE int refcount_query(obj_refcounter * refcount) {
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


static char *id_from_tdata(lives_thread_data_t *tdata) {
  char *tnum;
  if (tdata) {
#if IS_LINUX_GNU
    tnum = lives_strdup_printf("uid: 0x%lx, Thread 0x%lx, LWP %d",
                               tdata->uid, tdata->vars.var_thrd_self, tdata->vars.var_tid);
#else
    tnum = lives_strdup_printf("uid: 0x%lx, Thread 0x%lx",
                               tdata->uid, tdata->vars.var_thrd_self);
#endif
  } else tnum = lives_strdup(_("Unknown threadx"));
  return tnum;
}


char *get_thread_id(uint64_t uid) {
  lives_thread_data_t *tdata = get_thread_data_by_uid(uid);
  return id_from_tdata(tdata);
}


char *get_lpt_id(lives_proc_thread_t lpt) {
  lives_thread_data_t *tdata = lives_proc_thread_get_thread_data(lpt);
  return id_from_tdata(tdata);
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
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
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

/* static lives_result_t print_function(void *data) { */
/*   const char *funcdets = (const char *)data; */
/*   lives_printerr("%s\n", funcdets); */
/*   return LIVES_RESULT_FAIL; */
/* } */


char *get_threadstats(void) {
  int totthreads = 0, actthreads = 0;
  char *msg = NULL;
  pthread_rwlock_rdlock(&all_tdata_rwlock);
  g_printerr("\nThreads current state\n");
  for (LiVESList *list = all_tdatas; list; list = list->next) {
    char *notes = NULL, *tnum;
    lives_thread_data_t *tdata  = (lives_thread_data_t *)list->data;
    if (tdata) {
      char *tmp;
      totthreads++;

      if (pthread_equal(tdata->thrd_self, capable->gui_thread)) notes = lives_strdup("GUI thread");
      else if (tdata->thrd_type >= THRD_TYPE_EXTERN) notes = lives_strdup("External");
      tnum = get_thread_id(tdata->vars.var_uid);
      g_printerr("\nThread %d %s(%s):\nType: %s\n", tdata->slot_id, tnum,
                 notes ? notes : "-", tdata->vars.var_origin);
      lives_free(tnum);
      if (!tdata->vars.var_proc_thread)
        g_printerr("Idling in threadpool\n");
      else {
        lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(tdata->vars.var_proc_thread);
        if (finst) {
          actthreads++;
          g_printerr("Running ");
          tmp = lives_funcinst_show_func_call(finst);
          lives_free(tmp);
        } else lives_printerr("Running unknown function\n");
      }
      lpt_desc_state(tdata->vars.var_proc_thread);
    }

    g_printerr("Loveliness %.2f\n", tdata->vars.var_loveliness);

    if (tdata->vars.var_proc_thread) {
      ticks_t qtime, sytime, ptime;
      lives_proc_thread_t active_lpt = tdata->vars.var_proc_thread;
      qtime = lives_proc_thread_get_timing_info(active_lpt, TIME_TOT_QUEUE);
      sytime = lives_proc_thread_get_timing_info(active_lpt, TIME_TOT_SYNC_START);
      ptime = lives_proc_thread_get_timing_info(active_lpt, TIME_TOT_PROC);
      g_printerr("\n[queue wait time %.4f usec, sync_wait time %.4f usec, "
                 "proc time %.4f usec]\n\n",
                 (double)qtime / (double)USEC_TO_TICKS,
                 (double)sytime / (double)USEC_TO_TICKS,
                 (double)ptime / (double)USEC_TO_TICKS);
    }
  }

  pthread_rwlock_unlock(&all_tdata_rwlock);
  msg = lives_strdup_printf("Total threads in use: %d, (%d poolhtreads, %d other), "
                            "active threads %d\n\n", totthreads, npoolthreads,
                            totthreads - npoolthreads, actthreads);
  return msg;
}
