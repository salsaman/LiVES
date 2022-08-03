// threading.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _THREADING_H_
#define _THREADING_H_

typedef void *(*lives_thread_func_t)(void *);
typedef struct _lives_thread_data_t lives_thread_data_t;
typedef weed_plantptr_t lives_proc_thread_t;
typedef uint64_t lives_thread_attr_t;
typedef LiVESList lives_thread_t;

#ifdef HAVE_PTHREAD
// etc...
typedef pthread_t native_thread_t;
typedef pthread_mutex_t native_mutex_t;
typedef pthread_attr_t native_attr_t;
#endif

void lives_mutex_lock_carefully(pthread_mutex_t *mutex);

int proc_thread_kill_lock(void);
int proc_thread_kill_unlock(void);

#define THRDNATIVE_CAN_CORRECT (1ull << 0)

typedef struct {
  uint64_t var_uid;
  int var_idx;
  lives_obj_attr_t **var_attributes;
  pthread_t var_self;
  //
  lives_proc_thread_t var_tinfo;
  lives_thread_data_t *var_mydata;
  char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file;
  uint64_t var_random_seed;
  ticks_t var_event_ticks;
  LiVESList *var_func_stack;

  lives_intentcap_t var_intentcap;

  int var_write_failed, var_read_failed;
  boolean var_com_failed;
  boolean var_chdir_failed;
  int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data
  int var_rowstride_alignment_hint;
  int var_last_sws_block;
  int var_proc_file;
  int var_cancelled;
  int var_core_id;
  boolean var_fx_is_auto;
  boolean var_fx_is_audio;
  boolean var_no_gui;
  boolean var_force_button_image;
  volatile boolean var_fg_service;
  uint64_t var_hook_flag_hints;
  ticks_t var_timerinfo, var_round_trip_ticks, var_ticks_to_activate;
  uint64_t var_thrdnative_flags;
  void *var_stackaddr;
  size_t var_stacksize;
  uint64_t var_hook_hints;
  uint64_t var_sync_timeout;
  uint64_t var_blocked_limit;
  char *var_sync_motive; // contains reason why it might be waiting fro sync

  int var_hook_match_nparams;
  pthread_mutex_t var_hook_mutex[N_HOOK_POINTS];
  LiVESList *var_hook_closures[N_HOOK_POINTS];
  // hardware - values
  double var_loveliness; // a bit like 'niceness', only better
  volatile float *var_core_load_ptr; // pointer to value that monitors core load
  const char *var_fn_alloc_trace, *var_fn_free_trace;
  boolean var_fn_alloc_triggered, var_fn_free_triggered;
} lives_threadvars_t;

struct _lives_thread_data_t {
  pthread_t self;
  LiVESWidgetContext *ctx;
  int64_t idx; // thread index
  lives_threadvars_t vars;
  boolean exited;
  int signum;
  char padding[84];
};

typedef struct {
  lives_proc_thread_t lpt;  // may be NULL
  lives_thread_func_t func;
  void *arg;
  void *ret;
  uint64_t flags;
  lives_thread_attr_t attr;
  uint64_t caller;
  volatile uint64_t busy;
  volatile uint64_t done;
  volatile boolean sync_ready;
  LiVESList *hook_closures[N_HOOK_POINTS];
} thrd_work_t;

typedef struct {
  livespointer instance;
  lives_funcptr_t callback;
  livespointer user_data;
  uint8_t has_returnval;
  uint8_t is_timer;
  uint8_t added;
  volatile uint8_t swapped;
  unsigned long funcid;
  char *detsig;
  lives_proc_thread_t proc;
  lives_alarm_t alarm_handle;
  boolean finished;
  boolean destroyed;
} lives_sigdata_t;

#define LIVES_LEAF_THREADFUNC "tfunction"
#define LIVES_LEAF_PTHREAD_SELF "pthread_self"
#define LIVES_LEAF_RETURN_VALUE "return_value"

#define _RV_ LIVES_LEAF_RETURN_VALUE

#define LIVES_LEAF_THREAD_PARAM0 _LIVES_LEAF_THREAD_PARAM(0)
#define LIVES_LEAF_THREAD_PARAM1 _LIVES_LEAF_THREAD_PARAM(1)
#define LIVES_LEAF_THREAD_PARAM2 _LIVES_LEAF_THREAD_PARAM(2)

#define lpt_param_name(i) lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, (i))

// work flags

#define LIVES_THRDFLAG_QUEUED_WAITING  	(1ull << 0)
#define LIVES_THRDFLAG_RUNNING		(1ull << 1)
#define LIVES_THRDFLAG_FINISHED		(1ull << 2)

#define LIVES_THRDFLAG_AUTODELETE	(1ull << 8)
#define LIVES_THRDFLAG_WAIT_SYNC	(1ull << 9)
#define LIVES_THRDFLAG_WAIT_START	(1ull << 10)
#define LIVES_THRDFLAG_NO_GUI		(1ull << 11)
#define LIVES_THRDFLAG_TUNING		(1ull << 12)
#define LIVES_THRDFLAG_IGNORE_SYNCPT	(1ull << 13)
#define LIVES_THRDFLAG_NOFREE_LIST	(1ull << 14)

#define LIVES_THRDATTR_NONE		0
#define LIVES_THRDATTR_PRIORITY		(1ull << 0)
#define LIVES_THRDATTR_AUTODELETE	(1ull << 1)
#define LIVES_THRDATTR_WAIT_SYNC       	(1ull << 2)

// worker pool threads
void lives_threadpool_init(void);
void lives_threadpool_finish(void);

// lives_threads
thrd_work_t *lives_thread_create(lives_thread_t *, lives_thread_attr_t attr, lives_thread_func_t func, void *arg);
uint64_t lives_thread_done(lives_thread_t thread);
uint64_t lives_thread_join(lives_thread_t work, void **retval);
void lives_thread_free(lives_thread_t *thread);

// thread functions
lives_thread_data_t *get_thread_data_by_idx(uint64_t idx);
int get_n_active_threads(void);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_thread_data_t *get_global_thread_data(void);
lives_threadvars_t *get_global_threadvars(void);
lives_thread_data_t *lives_thread_data_create(uint64_t thread_id);

#define THREADVAR(var) (get_threadvars()->var_##var)
#define FG_THREADVAR(var) (get_global_threadvars()->var_##var)

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LIVES_LEAF_PROC_THREAD "proc_thread"

#define BLOCKED_LIMIT 10000 // mSec before thread in sync_point gets state blocked

// lives_proc_thread state flags
#define THRD_STATE_NONE		0

// LIFECYLCE - pproc_threads created in state NONE, the for a FG_THREAD, returned in UNQUEUED
// for bg threads, returned in state QUEUED
// when a pool thread picks up the work, states goes to PREPARING, and if wait sync is set, then
// will get state WAITING
// state goes to running, then finished : note running continues even after finished
#define THRD_STATE_UNQUEUED 	(1ull << 0)
#define THRD_STATE_QUEUED 	(1ull << 1)
#define THRD_STATE_PREPARING 	(1ull << 2)
#define THRD_STATE_RUNNING 	(1ull << 3) // check for FINISHED or CANCELLED
#define THRD_STATE_FINISHED 	(1ull << 4) // finsihed, but may be destroyed
#define THRD_STATE_COMPLETED 	(1ull << 5) // done - will not be destroyed via lifecycle

#define THRD_STATE_DEFERRED 	(1ull << 8) // will be run later due to resource limitations
#define THRD_STATE_DESTROYED 	(1ull << 9) // unreffed but not yet freed, next unref will free it

// temporary states
#define THRD_STATE_BUSY 	(1ull << 16)
// waiting for sync_ready
#define THRD_STATE_WAITING 	(1ull << 17) // waiting for sync_ready() or mainw->clutch
// blimit passed, blocked waiting in sync_point
#define THRD_STATE_BLOCKED 	(1ull << 18)
// request to pause - ignored for non pauseable threads
#define THRD_STATE_PAUSE_REQUESTED 	(1ull << 19)
// paused by request
#define THRD_STATE_PAUSED 	(1ull << 20)
// request to pause - ignored for non pauseable threads
#define THRD_STATE_RESUME_REQUESTED 	(1ull << 21)

// abnormal states
#define THRD_STATE_CANCELLED 	(1ull << 32)
// received system signal
#define THRD_STATE_SIGNALLED 	(1ull << 33)
#define THRD_STATE_ERROR 	(1ull << 34)
// called with invalid args_fmt
#define THRD_STATE_INVALID 	(1ull << 35)
// timed out waiting for sync_ready
#define THRD_STATE_TIMED_OUT	(1ull << 36)

// options
// can be cancelled by calling proc_thread_cancel
#define THRD_OPT_CANCELLABLE 	(1ull << 48)
// can be cancelled by calling proc_thread_cancel
#define THRD_OPT_PAUSEABLE 	(1ull << 49)

// wait for join, even if return type is void
#define THRD_OPT_NOTIFY 	(1ull << 50)
// return value irrelevant, thread should simply finish and lcean up its resources
// calling proc_thread_join after this is bad.
#define THRD_OPT_DONTCARE 	(1ull << 51)
// simulated 'idlefunc'
#define THRD_OPT_IDLEFUNC	(1ull << 52)

boolean lives_proc_thread_set_state(lives_proc_thread_t lpt, uint64_t state);
uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt);
uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits);
uint64_t lives_proc_thread_has_states(lives_proc_thread_t lpt, uint64_t state_bits);
uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits);
boolean lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits);

uint64_t get_worker_id(lives_proc_thread_t);
uint64_t get_worker_payload(uint64_t tid);
uint64_t get_worker_status(uint64_t tid);

// proc_thread specific attributes
#define LIVES_LEAF_THREAD_WORK "thread_work" // refers to underyling lives_thread

#define LIVES_LEAF_STATE_MUTEX "state_mutex" ///< ensures state is accessed atomically
#define LIVES_LEAF_DESTRUCT_MUTEX "destruct_mutex" ///< prevent thread from being freed as soon as dontcare is set
#define LIVES_LEAF_THRD_STATE "thread_state" // proc_thread state
#define LIVES_LEAF_SIGNAL_DATA "signal_data"
#define LIVES_LEAF_THREAD_ATTRS "thread_attributes" // attributes used to create pro_thread

// also LIVES_THRDATR_PRIORITY
// also LIVES_THRDATR_AUTODELETE
//#define LIVES_THRDATTR_WAIT_SYNC		(1ull << 2)
#define LIVES_THRDATTR_WAIT_START		(1ull << 3)
#define LIVES_THRDATTR_FG_THREAD		(1ull << 4)
#define LIVES_THRDATTR_NO_GUI			(1ull << 5)
#define LIVES_THRDATTR_INHERIT_HOOKS   		(1ull << 6)
#define LIVES_THRDATTR_IGNORE_SYNCPT   		(1ull << 7)
#define LIVES_THRDATTR_IDLEFUNC   		(1ull << 8)
#define LIVES_THRDATTR_NULLIFY_ON_DESTRUCTION  	(1ull << 9)

// extra info requests
#define LIVES_LEAF_START_TICKS "_start_ticks"

#define LIVES_THRDATTR_NOTE_STTIME		(1ull << 16)

#define LIVES_THRDATTR_IS_PROC_THREAD   	(1ull << 24)

#define lives_proc_thread_get_work(tinfo)				\
  ((thrd_work_t *)weed_get_voidptr_value((tinfo), LIVES_LEAF_THREAD_WORK, NULL))

#define lives_proc_thread_set_work(tinfo, work)				\
  weed_set_voidptr_value((tinfo), LIVES_LEAF_THREAD_WORK, (work))

ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t);

void _proc_thread_params_from_vargs(lives_proc_thread_t , lives_funcptr_t func, int return_type,
				    const char *args_fmt, va_list xargs);

void _proc_thread_params_from_nullvargs(lives_proc_thread_t , lives_funcptr_t func, int return_type);

lives_proc_thread_t _lives_proc_thread_create(lives_thread_attr_t, lives_funcptr_t, const char *fname,
    int return_type, const char *args_fmt, ...);

#define lives_proc_thread_create(a, f, r, af, ...) _lives_proc_thread_create(a, (lives_funcptr_t)f, #f, \
									     r, af, __VA_ARGS__)

#define lives_proc_thread_create_pvoid(a, f, r) _lives_proc_thread_create(a, (lives_funcptr_t)f, #f, r, "", NULL)
#define lives_proc_thread_create_rvoid(a, f, af, ...) _lives_proc_thread_create(a, (lives_funcptr_t)f, #f, \
										0, af, __VA_ARGS__)
#define lives_proc_thread_create_pvoid_rvoid(a, f) _lives_proc_thread_create(a, (lives_funcptr_t)f, #f, 0, "", NULL)
#define lives_proc_thread_create_rvoid_pvoid(a, f) _lives_proc_thread_create(a, (lives_funcptr_t)f, #f, 0, "", NULL)

lives_proc_thread_t _lives_proc_thread_create_with_timeout(ticks_t timeout, lives_thread_attr_t attr, lives_funcptr_t func,
    const char *funcname, int return_type,
    const char *args_fmt, ...);

#define lives_proc_thread_create_with_timeout(timeout, attr, func, return_type, args_fmt, ...) \
  _lives_proc_thread_create_with_timeout((timeout), (attr), (lives_funcptr_t)(func), #func, \
					 (return_type), (args_fmt), __VA_ARGS__)

boolean lives_proc_thread_free(lives_proc_thread_t lpt);

boolean _main_thread_execute(lives_funcptr_t, const char *fname, int return_type, void *retval, const char *args_fmt, ...);
boolean _main_thread_execute_rvoid(lives_funcptr_t func, const char *fname, const char *args_fmt, ...) ;
boolean _main_thread_execute_pvoid(lives_funcptr_t func, const char *fname, int return_type, void *retloc);

#define main_thread_execute(func, return_type, retloc, args_fmt, ...) \
  ((!args_fmt || !*args_fmt) ?  _main_thread_execute_pvoid((lives_funcptr_t)(func), #func, (return_type), retloc) \
   : _main_thread_execute((lives_funcptr_t)(func), #func, (return_type), retloc, args_fmt, __VA_ARGS__))

#define main_thread_execute_pvoid(func, return_type, retloc)		\
  (_main_thread_execute_pvoid((lives_funcptr_t)(func), #func, (return_type), retloc))

#define main_thread_execute_rvoid(func, args_fmt, ...)			\
(_main_thread_execute_rvoid((lives_funcptr_t)(func), #func, args_fmt, __VA_ARGS__))

#define main_thread_execute_rvoid_pvoid(func) (_main_thread_execute_pvoid((lives_funcptr_t)(func), #func, 0, NULL))
#define main_thread_execute_pvoid_rvoid(func) (_main_thread_execute_pvoid((lives_funcptr_t)(func), #func, 0, NULL))

#define MAIN_THREAD_EXECUTE(func, return_type, retloc, args_fmt, ...)	\
  main_thread_execute(func, return_type, retloc, args_fmt, __VA_ARGS__)

#define MAIN_THREAD_EXECUTE_VOID(func, args_fmt, ...) (main_thread_execute_rvoid(func, args_fmt, __VA_ARGS__))

boolean fg_run_func(lives_proc_thread_t lpt, void *rloc);

int lives_proc_thread_ref(lives_proc_thread_t);
boolean lives_proc_thread_unref(lives_proc_thread_t);
boolean lives_proc_thread_unref_check(lives_proc_thread_t);

void lives_proc_thread_nullify_on_destruction(lives_proc_thread_t, void **ptr);
void lives_proc_thread_auto_nullify(lives_proc_thread_t *lpt);

lives_proc_thread_t lives_proc_thread_secure_ptr(lives_proc_thread_t, void **);
lives_proc_thread_t lives_proc_thread_auto_secure(lives_proc_thread_t *);

// returns TRUE once the proc_thread will call the target function
// the thread can also be cancelled or finished
boolean lives_proc_thread_is_running(lives_proc_thread_t);

// returns TRUE if state id cancelled or finished
boolean lives_proc_thread_is_done(lives_proc_thread_t);

boolean lives_proc_thread_exited(lives_proc_thread_t);

/// returns FALSE while the thread is running, TRUE once it has finished
boolean lives_proc_thread_check_finished(lives_proc_thread_t);
boolean lives_proc_thread_check_completed(lives_proc_thread_t);

boolean lives_proc_thread_get_signalled(lives_proc_thread_t);
boolean lives_proc_thread_set_signalled(lives_proc_thread_t, int signum, void *data);
int lives_proc_thread_get_signal_data(lives_proc_thread_t, int64_t *tidx_return, void **data_return);

void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);

// self function for running proc_threads, sets pausable if not set already
// then calls sync_point, and waits for paused flagbit to be unset
void lives_proc_thread_pause(lives_proc_thread_t self);

void lives_proc_thread_set_pauseable(lives_proc_thread_t, boolean state);
boolean lives_proc_thread_get_pauseable(lives_proc_thread_t);

// set dontcare if the return result is no longer relevant / needed, otherwise the thread should be joined as normal
// if thread is already set dontcare, value here is ignored. For non-cancellable threads use lives_proc_thread_dontcare instead.
boolean lives_proc_thread_cancel(lives_proc_thread_t, boolean dontcare);
boolean lives_proc_thread_get_cancelled(lives_proc_thread_t);

// ask proc_thread to pause, ignored if non-pausable
// once paused, the paused hooks will be called, only once these have returned and unpause has been called
// will processing continue (after calling and returning from any unpaused hook callbacks)
boolean lives_proc_thread_request_pause(lives_proc_thread_t);
boolean lives_proc_thread_get_paused(lives_proc_thread_t);

// ask a paused proc_thread to resume. Processing only continues after this has been called, and any
// paused and unpaused hook callbacks have returnes
boolean lives_proc_thread_request_resume(lives_proc_thread_t);

// low level cancel, which will cause the thread to abort
boolean lives_proc_thread_cancel_immediate(lives_proc_thread_t);

/// tell a thread with return value that we no longer need the value so it can free itself
/// after setting this, no further operations may be done on the proc_thread
boolean lives_proc_thread_dontcare(lives_proc_thread_t);

// as above but takes address of ptr to be nullified when the thread is just about to be freed
boolean lives_proc_thread_dontcare_nullify(lives_proc_thread_t, void **nullif);

void lives_proc_thread_sync_ready(lives_proc_thread_t);

boolean sync_point(const char *motive);
//boolean thread_wait_loop(lives_proc_thread_t, thrd_work_t *, boolean trigger_sync_hooks, boolean wake_gui);
boolean thread_wait_loop(lives_proc_thread_t lpt, thrd_work_t *work, int hook_type, boolean wake_gui);

// WARNING !! version without a return value will free tinfo !
void lives_proc_thread_join(lives_proc_thread_t);

// with return value should free proc_thread
int lives_proc_thread_join_int(lives_proc_thread_t);
double lives_proc_thread_join_double(lives_proc_thread_t);
int lives_proc_thread_join_boolean(lives_proc_thread_t);
char *lives_proc_thread_join_string(lives_proc_thread_t);
weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t);
void *lives_proc_thread_join_voidptr(lives_proc_thread_t);
weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t) ;
int64_t lives_proc_thread_join_int64(lives_proc_thread_t);

char *get_threadstats(void);

// utility funcs (called from widget-helper.c)
boolean is_fg_thread(void);

int isstck(void *ptr);

///////////////// refcounting ////////////////

#define LIVES_LEAF_REFCOUNTER "refcounter" ///< generic

typedef struct {
  int count; // if count < 0, object should be destroyed
  pthread_mutex_t mutex;
  boolean mutex_inited;
} lives_refcounter_t;

boolean check_refcnt_init(lives_refcounter_t *);

int refcount_inc(lives_refcounter_t *);
int refcount_dec(lives_refcounter_t *);
int refcount_query(lives_refcounter_t *);
void refcount_unlock(lives_refcounter_t *);

int weed_refcount_inc(weed_plant_t *);
int weed_refcount_dec(weed_plant_t *);
int weed_refcount_query(weed_plant_t *);
void weed_refcounter_unlock(weed_plant_t *);

lives_refcounter_t *weed_add_refcounter(weed_plant_t *);
boolean weed_remove_refcounter(weed_plant_t *);

///////////////////////////
void make_thrdattrs(lives_thread_data_t *);

#define THREAD_INTENTION THREADVAR(intentcap).intent
#define THREAD_CAPACITIES THREADVAR(intentcap).capacities

// intents - for future use
// type = thread, subtype livesproc
#define PROC_THREAD_INTENTION_CREATE OBJ_INTENTION_CREATE // timeout is an optional ivar, default 0, how to handle
//						argc, argv in reqmts. ?
#define PROC_THREAD_INTENTION_DESTROY OBJ_INTENTION_DESTROY // free func
#define PROC_THREAD_INTENTION_CANCEL OBJ_INTENTION_CANCEL // -> cancel_immediate

#define PROC_THREAD_INTENTION_GET_VALUE OBJ_INTENTION_GET_VALUE // e.g cancellable
#define PROC_THREAD_INTENTION_SET_VALUE OBJ_INTENTION_SET_VALUE // e.g cancellable

#endif

