// threading.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// the terminology is a bit muddled here.
// "lives_thread_t" is actually a thread work_t packet as data in a list node
// this can then be appended / prepended to the task list for the actual threads (pool threads)
//
// then there are lives_proc_threads - this is a data structure which wraps the work packet, adding things like monitoring, hook points
//

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

#define THRDNATIVE_CAN_CORRECT (1ull << 0)

typedef enum {
  THRD_TYPE_UNKNOWN,
  /* internal types */
  THRD_TYPE_MAIN,
  THRD_TYPE_AUX,
  THRD_TYPE_WORKER,
  /* external types */
  THRD_TYPE_EXTERN = 256,
  THRD_TYPE_AUDIO_WRITER,
  THRD_TYPE_AUDIO_READER,
  THRD_TYPE_VIDEO_PLAYER,
} lives_thread_type;

typedef struct {
  // thread specific variables
  //
  uint64_t var_uid; // copy of tdata uid
  char var_origin[128]; // thread descriptive text eg "LiVES Worker Thread"
  lives_proc_thread_t var_proc_thread; // 'self' proc_thread
  lives_obj_attr_t **var_attributes; // attributes passed to proc_thread

  lives_intentcap_t var_intentcap;

  // sync related
  volatile boolean var_sync_ready;
  pthread_mutex_t *var_pause_mutex;
  pthread_cond_t *var_pcond;
  uint64_t var_sync_timeout;
  uint64_t var_blocked_limit;
  char *var_sync_motive; // optional text descriminator
  ticks_t var_event_ticks;

  // hooks
  volatile boolean var_fg_service;
  uint64_t var_hook_hints, var_perm_hook_hints;
  int var_hook_match_nparams;
  lives_hook_stack_t *var_hook_stacks[N_HOOK_POINTS];

  // error handling
  char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file;
  int var_write_failed, var_read_failed;
  boolean var_com_failed;
  boolean var_chdir_failed;
  int var_proc_file;
  int var_cancelled;

  // graphical / ui
  boolean var_no_gui;
  LiVESWidgetContext *var_guictx;
  LiVESWidgetLoop *var_guiloop;
  LiVESWidgetSource *var_guisource;

  int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data
  int var_rowstride_alignment_hint;
  int var_last_sws_block;

  // misc
  boolean var_fx_is_auto;
  boolean var_fx_is_audio;
  boolean var_force_button_image;

  // hardware values
  double var_loveliness; // a bit like 'niceness', only better
  uint64_t var_thrdnative_flags;
  uint64_t var_random_seed;
  void *var_stackaddr;
  size_t var_stacksize;
  int var_core_id;
  volatile float *var_core_load_ptr; // pointer to value that monitors core load

  // debugging
  LiVESList *var_func_stack;
  ticks_t var_timerinfo, var_round_trip_ticks, var_ticks_to_activate;
  const char *var_fn_alloc_trace, *var_fn_free_trace;
  boolean var_fn_alloc_triggered, var_fn_free_triggered;
} lives_threadvars_t;

struct _lives_thread_data_t {
  // thread specific data struct
  pthread_t thrd_self;
  uint64_t uid; // unique identifier
  lives_thread_type thrd_type;
  int32_t slot_id; // pool slot, -1 for non worker threads
  lives_threadvars_t vars; // thread_data
  boolean exited;
  int signum;
  //char padding[84];
};

typedef struct {
  // thread work packet
  //
  // copy of arg if packet is for a proc_thread, otherwise NULL
  lives_proc_thread_t lpt;
  // function to be called
  lives_thread_func_t func;
  // function argument
  void *arg;
  //  maybe unused ?
  void *ret;
  // flags representing current state
  uint64_t flags;
  // attrs set when creating / queuing
  lives_thread_attr_t attrs;
  uint64_t caller;  // uid of thread which created the packet
  boolean sync_ready; // flag for thread sync
  volatile uint64_t busy; // uid of thread worker
  volatile uint64_t done; // when finished, == busy
  // if set, the work was cancelled before being run
  volatile boolean skipped;
} thrd_work_t;

// this for rerouted GUI callbacks, some of this maybe irrelevant now (TODO)
typedef struct {
  livespointer instance;
  lives_funcptr_t callback;
  livespointer user_data;
  uint8_t has_returnval;
  uint8_t is_timer;
  volatile uint8_t swapped;
  unsigned long funcid;
  char *detsig;
  lives_proc_thread_t proc;
  lives_alarm_t alarm_handle;
  int depth; // governor_loop depth when added
  volatile int state;
} lives_sigdata_t;

#define SIGDATA_STATE_NEW	1
#define SIGDATA_STATE_ADDED	2
#define SIGDATA_STATE_RUNNING	3
#define SIGDATA_STATE_POSTPONED	4
#define SIGDATA_STATE_FINISHED	5
#define SIGDATA_STATE_DESTROYED 6

// work flags
// TODO: just use THRDFLAG_*
#define LIVES_THRDFLAG_QUEUED_WAITING  	(1ull << 0)
#define LIVES_THRDFLAG_HOLD		(1ull << 1)
#define LIVES_THRDFLAG_RUNNING		(1ull << 2)
#define LIVES_THRDFLAG_FINISHED		(1ull << 3)
#define LIVES_THRDFLAG_TIMEOUT		(1ull << 4)
#define LIVES_THRDFLAG_COND_WAITING	(1ull << 5)

#define LIVES_THRDFLAG_AUTODELETE	(1ull << 8)
#define LIVES_THRDFLAG_DETACH		(1ull << 9)
#define LIVES_THRDFLAG_WAIT_SYNC	(1ull << 10)
#define LIVES_THRDFLAG_WAIT_START	(1ull << 11)
#define LIVES_THRDFLAG_NO_GUI		(1ull << 12)
#define LIVES_THRDFLAG_TUNING		(1ull << 13)
#define LIVES_THRDFLAG_IGNORE_SYNCPTS	(1ull << 14)
#define LIVES_THRDFLAG_NOFREE_LIST	(1ull << 15)
#define LIVES_THRDFLAG_SKIP_EXEC	(1ull << 16)

#define LIVES_THRDFLAG_NOTE_TIMINGS	(1ull << 32)

// worker pool threads
void lives_threadpool_init(void);
void lives_threadpool_finish(void);

void check_pool_threads(void);

// lives_threads
thrd_work_t *lives_thread_create(lives_thread_t **, lives_thread_attr_t attr, lives_thread_func_t func, void *arg);
uint64_t lives_thread_done(lives_thread_t *thread);
uint64_t lives_thread_join(lives_thread_t *thrd, void **retval);
void lives_thread_free(lives_thread_t *thread);

// thread functions
lives_thread_data_t *get_thread_data_by_slot_idx(int32_t idx);
lives_thread_data_t *get_thread_data_by_uid(uint64_t uid);
int get_n_active_threads(void);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_threadvars_t *get_threadvars_bg_only(void);
lives_thread_data_t *get_global_thread_data(void);
lives_threadvars_t *get_global_threadvars(void);
///lives_thread_data_t *lives_thread_data_create(uint64_t thread_id);
//lives_thread_data_t *lives_thread_data_create(void *pidx);
void *lives_thread_data_create(void *pidx);

#define THREADVAR(var) (get_threadvars()->var_##var)
#define FG_THREADVAR(var) (get_global_threadvars()->var_##var)
#define BG_THREADVAR(var) (get_threadvars_bg_only()->var_##var)

#define THREAD_CTX THREADVAR(guictx)

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////

#define SYNC_CHECK_TIME ONE_MILLION // usec between polling for sync_wait TODO - make into threadvar

#define BLOCKED_LIMIT 10000 // mSec before thread in sync_point gets state blocked

// these flags are for LIVES_PROC_THREADS
// TODO: change THRD to LPT

#define THRD_STATE_NONE		0

// 'transient' states (lower set)

#define THRD_STATE_UNQUEUED 	(1ull << 0) // intial state for all proc_threads
#define THRD_STATE_DEFERRED 	(1ull << 1) // will be run later due to resource limitations
#define THRD_STATE_QUEUED 	(1ull << 2) // queued for execution (eithr in worker ppol, or as fg reequst)
#define THRD_STATE_PREPARING 	(1ull << 3) // has been assigned from queue, but not yet running
#define THRD_STATE_RUNNING 	(1ull << 4) // thread is processing

// (semi) final states
// for IDLEFUNCS, combined with unqueued implies the proc_thread can be requued
// combined instead with paused means the thread can be resumed rather than requeud
// may be combined with PAUSED, and always combined with UNQUEUED
#define THRD_STATE_IDLING 	(1ull << 5)

// fixed states
// completed is combined with FINISHED after 'completed' hook cbs have all returned
#define THRD_STATE_COMPLETED 	(1ull << 8) // processing complete, not idling
#define THRD_STATE_DESTROYING 	(1ull << 9) // proc_thread will be destroyed as soon as all refs are removed

// permanent states
#define THRD_STATE_FINISHED 	(1ull << 10) // processing finished and all hooks have been triggered
#define THRD_STATE_DESTROYED 	(1ull << 11) // proc_thread is about to be freed; must not be reffed or unreffed
#define THRD_STATE_STACKED 	(1ull << 12) // is held in a hook stack

// destroyed is also a final state, but it should not be checked for, the correct way is to add a callback
// to the DESTRUCTION_HOOK. If the state is IDLING, the proc_thread mya be requeued
#define THRD_FINAL_STATES (THRD_STATE_IDLING | THRD_STATE_FINISHED)

// this state only exists while running completed hook cbs
#define THRD_STATE_WILL_DESTROY (THRD_STATE_COMPLETED | THRD_STATE_DESTROYING)

// there are 3 ways a proc_thread can be waiting:
// - sync_waiting -> thread will continue when another thread calls sync_ready()
// this can also occur when the thread is queued with the wait_sync attribute
//
// - (normal, non sync) waiting - the proc_thread is waiting for self defined condidion(s)
// 	to become true, it may pssobly become blocked and / or timeout
//
// - paused - similar to sync_waiting, however, the wait is conditional and depends on
//    another thread calling pause request. The proc_trhead_must specifially be set (or set itself)
//    pausable for this to function.
//    idle / pausing threads also enter this state after completeing (if not canceelled)
//    even if not pausable during execution
//    The proc_thread will continue after a resume_request is called for it.
//
//   If a waiting proc_thread receives a cancel_request, it will resume and then (quickly) be cancelled
//   and finish.
//
// waiting for another thread to call sync_ready(this_proc_thread)
#define THRD_STATE_SYNC_WAITING 	(1ull << 15)

// temporary states (with PREPARING or RUNNING)
#define THRD_STATE_BUSY 	(1ull << 16)
// waiting for condition(s) to become TRUE
#define THRD_STATE_WAITING 	(1ull << 17)
// blimit passed, blocked waiting in sync_point
#define THRD_STATE_BLOCKED 	(1ull << 18)

// requested states
// request to pause - ignored for non pauseable threads
#define THRD_STATE_PAUSE_REQUESTED 	(1ull << 19)
// paused by request, or due to idling
#define THRD_STATE_PAUSED 		(1ull << 20)
// request to pause - ignored for non pauseable threads (unless they are paused / idling)
#define THRD_STATE_RESUME_REQUESTED 	(1ull << 21)
// request to cancel - ignored for non cancellable threads
#define THRD_STATE_CANCEL_REQUESTED 	(1ull << 22)

// cancelled by request, this is not a final state until acompanied by completed
#define THRD_STATE_CANCELLED 	(1ull << 32)

// bits 0,1,2,3,4,5 and 16,17,18,19,20,21,22
#define THRD_TRANSIENT_STATES  0X007F003F

// for proc_threads created with attr IDLEFUNC: after processing and returning TRUE
// the proc_thread will be returned in state UNQUEUED | IDLING
// at a later time, the idlefunc can be restarted via lives_proc_thread_queue()
//
// this process continues until either the idlefunc returns FALSE, or if cancellable, the idlefunc proc_thread
// gets a cancel request, and acts on it, in this case the COMPLETED hook is called and final state includes FINISHED
// when requeud, the UNQUEUED / IDLING flag bits are removed, and the status will change to QUEUED

// abnormal states

// timed out waiting for sync_ready
#define THRD_STATE_TIMED_OUT	(1ull << 33)
// other unspeficified error
#define THRD_STATE_ERROR 	(1ull << 34)

// received system signal
#define THRD_STATE_SIGNALLED 	(1ull << 38)

// called with invalid args_fmt
#define THRD_STATE_INVALID 	(1ull << 40)

// options - TODO - these should be retained as ATTRs instead
// can be cancelled by calling proc_thread_cancel
#define THRD_OPT_CANCELABLE 	(1ull << 48)
// can be cancelled by calling proc_thread_cancel
#define THRD_OPT_PAUSEABLE 	(1ull << 49)

// wait for join, even if return type is void
#define THRD_OPT_NOTIFY 	(1ull << 50)

// return value irrelevant, thread should simply finish and lcean up its resources
// doing anything with lpt after this may cause undefined behaviour
// - adding a hook callback to either the completed and/or destruction hooks is valid however
// however this must be done before setting dontcare. Dontcare can be set when creating the lpt, during execution, or
// as an option when reequesting cancel
#define THRD_OPT_DONTCARE 	(1ull << 51)

// will queue self or a follow on proc_thread on completion
#define THRD_OPT_AUTO_REQUEUE	(1ull << 52)

// thread will pause on completion, can then be resumed and it will run again.
#define THRD_OPT_AUTO_PAUSE	(1ull << 53)

// can be set to prevent state change hooks from being triggered
#define THRD_BLOCK_HOOKS	(1ull << 60)

// flags thread as externally created / controlled
#define THRD_STATE_EXTERN	(1ull << 63)

char *lives_proc_thread_get_funcname(lives_proc_thread_t lpt);
uint32_t lives_proc_thread_get_rtype(lives_proc_thread_t lpt);
funcsig_t lives_proc_thread_get_funcsig(lives_proc_thread_t lpt);

uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt);
uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits);
uint64_t lives_proc_thread_has_states(lives_proc_thread_t lpt, uint64_t state_bits);

#define GET_PROC_THREAD_SELF(self) lives_proc_thread_t self = THREADVAR(proc_thread);

// because of hook triggers, there is no set_state, instead use lives_proc_thread_include_states(lives_proc_t
// i.e.
// exclude anything in state which is not in new_state, then include anything in new_state which is not in state
#define lives_proc_thread_set_state(lpt, new_state)	\
  lives_proc_thread_include_states(lpt, new_state & ~(lives_proc_thread_exclude_states \
						      (lpt, lives_proc_thread_get_state(lpt) & ~new_state)))

uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits);
uint64_t lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits);

uint64_t get_worker_id(lives_proc_thread_t);
uint64_t get_worker_payload(uint64_t tid);
uint64_t get_worker_status(uint64_t tid);

// proc_thread leaves

#define LIVES_LEAF_THREADFUNC "tfunction"
#define LIVES_LEAF_PTHREAD_SELF "pthread_self"
#define LIVES_LEAF_RETURN_VALUE "return_value"

#define _RV_ LIVES_LEAF_RETURN_VALUE

#define LIVES_LEAF_THREAD_PARAM0 LIVES_LEAF_THREAD_PARAM(0)
#define LIVES_LEAF_THREAD_PARAM1 LIVES_LEAF_THREAD_PARAM(1)
#define LIVES_LEAF_THREAD_PARAM2 LIVES_LEAF_THREAD_PARAM(2)

#define LIVES_LEAF_PROC_THREAD "proc_thread"

#define lpt_param_name(i) lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, (i))

#define LIVES_LEAF_THREAD_WORK "thread_work" // refers to underyling lives_thread

#define LIVES_LEAF_STATE_MUTEX "state_mutex" ///< ensures state is accessed atomically
#define LIVES_LEAF_DESTRUCT_MUTEX "destruct_mutex" ///< ensures destruct is accessed atomically
#define LIVES_LEAF_DESTRUCT_RWLOCK "destruct_rwlock" ///< ensures destruct is accessed atomically
#define LIVES_LEAF_THRD_STATE "thread_state" // proc_thread state
#define LIVES_LEAF_SIGNAL_DATA "signal_data"
#define LIVES_LEAF_EXT_CB_LIST "ext_cb_list" // list of closures added to other thread hook_stacks
#define LIVES_LEAF_THREAD_ATTRS "thread_attributes" // attributes used to create pro_thread
#define LIVES_LEAF_RETLOC "retloc" // pointer to variable to store retval in directly
#define LIVES_LEAF_FOLLOWUP "followup"  // proc_thread to be run after this one (for AUTO_REQUEUE)
#define LIVES_LEAF_LPT_DATA "lpt_data" // scratch data area for proc_threads

#define LIVES_THRDATTR_NONE			0

// worker flagbits (can be set when queueing)
#define LIVES_THRDATTR_PRIORITY			(1ull << 0)
#define LIVES_THRDATTR_AUTODELETE		(1ull << 1)
#define LIVES_THRDATTR_WAIT_SYNC   	    	(1ull << 2)
#define LIVES_THRDATTR_WAIT_START		(1ull << 3)
#define LIVES_THRDATTR_IGNORE_SYNCPTS  		(1ull << 4)

// lpt flagbits (only valid when creating a proc_thread)
// create proc thread but do not queue it
#define LIVES_THRDATTR_START_UNQUEUED		(1ull << 16)

// after completion, a follow on proc_thread will be run
// by default this will be the same proc thread, but an alternate proc_thread can be prepared unqued and
#define LIVES_THRDATTR_AUTO_REQUEUE		(1ull << 17)

// after completion, thread will pause, can be resumed
// when combined with auto requeue, the thread will pause  until joined
#define LIVES_THRDATTR_AUTO_PAUSE   		(1ull << 18)

// non function attrs
#define LIVES_THRDATTR_NOTE_TIMINGS		(1ull << 32)
#define LIVES_THRDATTR_NO_GUI			(1ull << 33)

// create a proc thread for the gui thread to run
// instead of being pushed to the poolthread queue, it will be pushed to
// the main thread's stack
// - this will go away, and be replaced by stack dtls
#define LIVES_THRDATTR_FG_THREAD   		(1ull << 40)

// light - indicates a trivial request, such as updating a spinbutton which can be carried out quickly
// this will block and get executed as soon as possible
#define LIVES_THRDATTR_FG_LIGHT	   		(1ull << 41)

// internal flagbits
#define LIVES_THRDATTR_NO_UNREF   		(1ull << 50) // deprecated ?

// timing related values
#define LIVES_LEAF_QUEUED_TICKS "_queue_ticks"
#define LIVES_LEAF_SYNC_WAIT_TICKS "_s_wait_ticks"
#define LIVES_LEAF_START_TICKS "_start_ticks"
#define LIVES_LEAF_END_TICKS "_end_ticks"

enum {
  TIME_STAMP_QUEUED,
  TIME_STAMP_START,
  TIME_STAMP_END,
  TIME_TOT_QUEUE,
  TIME_TOT_SYNC_START,
  TIME_TOT_PROC,
  N_TIME_DTLS,
};

ticks_t lives_proc_thread_get_timing_info(lives_proc_thread_t lpt, int info_type);

// internal value
#define LIVES_THRDATTR_IS_PROC_THREAD   	(1ull << 24)

#define lives_proc_thread_get_work(lpt)				\
  ((thrd_work_t *)weed_get_voidptr_value((lpt), LIVES_LEAF_THREAD_WORK, NULL))

#define lives_proc_thread_set_work(lpt, work)				\
  weed_set_voidptr_value((lpt), LIVES_LEAF_THREAD_WORK, (work))

// attrs
void lives_proc_thread_set_attrs(lives_proc_thread_t, uint64_t attrs);
uint64_t lives_proc_thread_get_attrs(lives_proc_thread_t);

// ---> get timing info
ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t);

void _proc_thread_params_from_vargs(lives_proc_thread_t, lives_funcptr_t func, int return_type,
                                    const char *args_fmt, va_list xargs);

void _proc_thread_params_from_nullvargs(lives_proc_thread_t, lives_funcptr_t func, int return_type);

lives_proc_thread_t _lives_proc_thread_create(lives_thread_attr_t, lives_funcptr_t, const char *fname,
    int return_type, const char *args_fmt, ...);

#define lives_proc_thread_create(a, f, r, af, ...) _lives_proc_thread_create(a, (lives_funcptr_t)f, #f, \
									     r, af, __VA_ARGS__)

const lives_funcdef_t *lives_proc_thread_make_funcdef(lives_proc_thread_t lpt);
lives_proc_thread_t lives_proc_thread_create_from_funcinst(lives_thread_attr_t attr, lives_funcinst_t *finst);

lives_proc_thread_t _lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type,
    const char *args_fmt, va_list xargs);

lives_proc_thread_t _lives_proc_thread_create_nullvargs(lives_thread_attr_t attr, lives_funcptr_t func,
    const char *fname, int return_type);

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

boolean lives_proc_thread_unref(lives_proc_thread_t lpt);

boolean _main_thread_execute(lives_funcptr_t, const char *fname, int return_type, void *retval, const char *args_fmt, ...);
boolean _main_thread_execute_rvoid(lives_funcptr_t func, const char *fname, const char *args_fmt, ...) ;
boolean _main_thread_execute_pvoid(lives_funcptr_t func, const char *fname, int return_type, void *retloc);

// real params, real ret_tpye
#define MAIN_THREAD_EXECUTE(func, return_type, retloc, args_fmt, ...) do { \
    if (!is_fg_thread())						\
      _main_thread_execute((lives_funcptr_t)func, #func, return_type, retloc, args_fmt, __VA_ARGS__); \
    else *retloc = func(__VA_ARGS__);					\
  } while (0)

// real params, ret_type 0 or -1
#define MAIN_THREAD_EXECUTE_RVOID(func, return_type, args_fmt, ...) do { \
    if (!is_fg_thread())						\
      _main_thread_execute((lives_funcptr_t)func, #func, return_type, NULL, args_fmt, __VA_ARGS__); \
    else func(__VA_ARGS__);						\
  } while (0)

// void params, real return_type
#define MAIN_THREAD_EXECUTE_PVOID(func, return_type, retloc) do {	\
    if (!is_fg_thread())					\
      _main_thread_execute((lives_funcptr_t)func, #func, return_type, retloc, "", NULL); \
    else *retloc = func();						\
    } while (0)

// void params, ret_type 0 or 1
#define MAIN_THREAD_EXECUTE_VOID(func, return_type) do {				\
    if (!is_fg_thread())						\
      _main_thread_execute((lives_funcptr_t)func, #func, return_type, NULL, "", NULL); \
    else func();						\
  } while (0)

#define main_thread_execute(func, return_type, retloc, args_fmt, ...)	\
  do {MAIN_THREAD_EXECUTE(func, return_type, retloc, args_fmt, __VA_ARGS__);} while (0)

#define main_thread_execute_rvoid(func, return_type, args_fmt, ...) \
  do {MAIN_THREAD_EXECUTE_RVOID(func, return_type, args_fmt, __VA_ARGS__);} while (0)

#define main_thread_execute_pvoid(func, return_type, retloc)	\
  do {MAIN_THREAD_EXECUTE_PVOID(func, return_type, retloc);} while (0)

#define main_thread_execute_void(func, return_type)			\
  do {MAIN_THREAD_EXECUTE_VOID(func, return_type);} while (0)

boolean lives_proc_thread_execute(lives_proc_thread_t lpt, void *rloc);

boolean lives_proc_thread_queue(lives_proc_thread_t lpt, lives_thread_attr_t);

int lives_proc_thread_ref(lives_proc_thread_t);
boolean lives_proc_thread_unref(lives_proc_thread_t);
int lives_proc_thread_count_refs(lives_proc_thread_t);

boolean lives_proc_thread_nullify_on_destruction(lives_proc_thread_t, void **ptr);
lives_proc_thread_t lives_proc_thread_auto_nullify(lives_proc_thread_t *);
void lives_proc_thread_remove_nullify(lives_proc_thread_t, void **ptr);

// each lpt has a "data" area. Any type of data can be written here
// and later recalled
// live_proc-thread_steal_data will return the data area (plant) and set the dat ain lpt to NULL
weed_plant_t *lives_proc_thread_get_data(lives_proc_thread_t);
weed_plant_t *lives_proc_thread_steal_data(lives_proc_thread_t);

#define lives_proc_thread_make_static(lpt, name)	\
  weed_leaf_set_flags(lpt, name, WEED_FLAG_UNDELETABLE)

#define lives_proc_thread_set_int_value(lpt, name, val)			\
  weed_set_int_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_int_value(lpt, name)			\
  (weed_get_int_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_boolean_value(lpt, name, val)		\
  weed_set_boolean_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_boolean_value(lpt, name)			\
  (weed_get_boolean_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_double_value(lpt, name, val)		\
  weed_set_double_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_double_value(lpt, name)			\
  (weed_get_double_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_string_value(lpt, name, val)		\
  weed_set_string_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_string_value(lpt, name)			\
  (weed_get_string_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_int64_value(lpt, name, val)		\
  weed_set_int64_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_int64_value(lpt, name)			\
  (weed_get_int64_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_funcptr_value(lpt, name, val)		\
  weed_set_funcptr_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_funcptr_value(lpt, name)			\
  (weed_get_funcptr_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_voidptr_value(lpt, name, val)		\
  weed_set_voidptr_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_voidptr_value(lpt, name)			\
  (weed_get_voidptr_value(lives_proc_thread_get_data(lpt), name, NULL))
#define lives_proc_thread_set_plantptr_value(lpt, name, val)		\
  weed_set_plantptr_value(lives_proc_thread_get_data(lpt), name, val)
#define lives_proc_thread_get_plantptr_value(lpt, name)			\
  (weed_get_plantptr_value(lives_proc_thread_get_data(lpt), name, NULL))

// test if lpt is queued for execution
boolean lives_proc_thread_is_queued(lives_proc_thread_t);
// this is not the same as !queueu
boolean lives_proc_thread_is_unqueued(lives_proc_thread_t);
boolean lives_proc_thread_is_preparing(lives_proc_thread_t);
boolean lives_proc_thread_is_running(lives_proc_thread_t);
//test if lpt is wating for sync_ready()
boolean lives_proc_thread_sync_waiting(lives_proc_thread_t);
//test if lpt is wating for self condition(s)
boolean lives_proc_thread_is_waiting(lives_proc_thread_t);
boolean lives_proc_thread_is_paused(lives_proc_thread_t);
boolean lives_proc_thread_is_idling(lives_proc_thread_t);
boolean lives_proc_thread_idle_paused(lives_proc_thread_t);
boolean lives_proc_thread_was_cancelled(lives_proc_thread_t);
// test if lpt is in a hook stack
boolean lives_proc_thread_is_stacked(lives_proc_thread_t);
boolean lives_proc_thread_is_invalid(lives_proc_thread_t);

// returns TRUE if state is FINISHED or IDLING
boolean lives_proc_thread_is_done(lives_proc_thread_t);
boolean lives_proc_thread_exited(lives_proc_thread_t);

// this test is ONLY valid inside the COMPLETED hook callbacks
boolean lives_proc_thread_will_destroy(lives_proc_thread_t);

// test for cancelled OR cancel_requested
boolean lives_proc_thread_should_cancel(lives_proc_thread_t);

boolean lives_proc_thread_check_finished(lives_proc_thread_t);

boolean lives_proc_thread_get_signalled(lives_proc_thread_t);
boolean lives_proc_thread_set_signalled(lives_proc_thread_t, int signum, void *data);
int lives_proc_thread_get_signal_data(lives_proc_thread_t, uint64_t *tuid_return, void **data_return);

void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);

// set dontcare if the return result is no longer relevant / needed, o
// therwise the thread should be joined as normal
// if thread is already set dontcare, value here is ignored.
// For non-cancellable threads, this is ignored;  use lives_proc_thread_dontcare instead.
boolean lives_proc_thread_request_cancel(lives_proc_thread_t lpt, boolean dontcare);
boolean lives_proc_thread_cancel(lives_proc_thread_t);
boolean lives_proc_thread_get_cancel_requested(lives_proc_thread_t);

// self function for running proc_threads, sets pausable if not set already
// then calls sync_point, and waits for paused flagbit to be unset
void lives_proc_thread_set_pauseable(lives_proc_thread_t, boolean state);
boolean lives_proc_thread_get_pauseable(lives_proc_thread_t);

// ask proc_thread to pause, ignored if non-pausable
// once paused, the paused hooks will be called, only once these have returned and unpause has been called
// will processing continue (after calling and returning from any unpaused hook callbacks)
boolean lives_proc_thread_request_pause(lives_proc_thread_t);
void lives_proc_thread_pause(lives_proc_thread_t self);
// cf. is_paused
boolean lives_proc_thread_get_pause_requested(lives_proc_thread_t);

// ask a paused proc_thread to resume. Processing only continues after this has been called, and any
// paused and unpaused hook callbacks have returnes
boolean lives_proc_thread_request_resume(lives_proc_thread_t);
boolean lives_proc_thread_get_resume_requested(lives_proc_thread_t);
boolean lives_proc_thread_resume(lives_proc_thread_t self);

// low level cancel, which will cause the thread to abort
// WARING - quite likely will result in the entire app exiting
boolean lives_proc_thread_cancel_immediate(lives_proc_thread_t);

/// tell a thread with return value that we no longer need the value so it can free itself
/// after setting this, no further operations may be done on the proc_thread
boolean lives_proc_thread_dontcare(lives_proc_thread_t);

// as above but takes address of ptr to be nullified when the thread is about to be freed
boolean lives_proc_thread_dontcare_nullify(lives_proc_thread_t, void **nullif);

/* boolean lives_proc_thread_timedwait(int sec); */
/* boolean lives_proc_thread_wait(void); */

// soft wait (waiting thread remains active while waiting, can update state to blocked,
// check if it has been cancelled, poll multiple functions, abort waiting by setting controlvar)

// this is a self function for sync_wait hook
// set control to TRUE to return - if all SYNC_WAIT_HOOK funcs return TRUE (polled for), control is also set to TRUE
// if control is NULL, an internal variable will be used
// proc_thread can either add hook_callbacks to its SYNC_WAIT hook stack, or set othr hook callabcks which set
// control to TRUE
boolean thread_wait_loop(lives_proc_thread_t lpt, boolean full_sync, volatile boolean *control);

// hard wait...resume when another thread calls lives_proc_thread_sync_ready()
boolean sync_point(const char *motive);

// pthread level waiting
// hard wait, thread can only continue by either timedwait timing out, or another thread calling
// lives_proc_thread_sync_ready on its behalf
// essentially the proc_thread is "frozen"
boolean lives_proc_thread_sync_ready(lives_proc_thread_t);

// block until lpt is sync_waiting, then both threads continue
boolean lives_proc_thread_sync_continue(lives_proc_thread_t);
// ditto, but will zoom past sytn_points until motives are euqal
boolean lives_proc_threads_sync_at(lives_proc_thread_t, const char *motive);

const char *lives_proc_thread_get_wait_motive(lives_proc_thread_t);

// WARNING !! version without a return value will free lpt !
void lives_proc_thread_join(lives_proc_thread_t);

// wait until a proc thread signals FINISHED | IDLING
// or timeout (seconds) has elapsed (timeout == 0. means unlimited)
// if caller is the fg thread, it will service fg requests while waiting
int lives_proc_thread_wait_done(lives_proc_thread_t lpt, double timeout);

// with return value should free proc_thread
int lives_proc_thread_join_int(lives_proc_thread_t);
double lives_proc_thread_join_double(lives_proc_thread_t);
int lives_proc_thread_join_boolean(lives_proc_thread_t);
char *lives_proc_thread_join_string(lives_proc_thread_t);
int64_t lives_proc_thread_join_int64(lives_proc_thread_t);
weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t);
void *lives_proc_thread_join_voidptr(lives_proc_thread_t);
weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t) ;

char *lives_proc_thread_state_desc(uint64_t state);

#define lpt_desc_state(lpt) lives_proc_thread_state_desc(lives_proc_thread_get_state(lpt))

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

int weed_refcount_inc(weed_plant_t *);
int weed_refcount_dec(weed_plant_t *);
int weed_refcount_query(weed_plant_t *);

lives_refcounter_t *weed_add_refcounter(weed_plant_t *);
boolean weed_remove_refcounter(weed_plant_t *);

///////////////////////////
void make_thrdattrs(lives_thread_data_t *);

#define THREAD_INTENTION THREADVAR(intentcap).intent
#define THREAD_CAPACITIES THREADVAR(intentcap).capacities

#endif

