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

#include "alarms.h"
#include "colourspace.h"


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

#define SIG_ACT_IGNORE		0
#define SIG_ACT_CANCEL		1
#define SIG_ACT_ERROR		2
#define SIG_ACT_HOLD		3

#define SIG_ACT_CMD		8
#define SIG_ACT_REPORT		9
#define SIG_ACT_CONTEXT		10

#define LIVES_INTERRUPT_SIG SIGRTMIN+6 // 40

void set_interrupt_action(int action);
lives_result_t lives_proc_thread_try_interrupt(lives_proc_thread_t, weed_plant_t *data);
boolean lives_proc_thread_can_interrupt(lives_proc_thread_t);

#define THRDNATIVE_CAN_INTERRUPT (1ull << 0)

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
  // copy of thread_data_t
  pthread_t var_thrd_self;
#if IS_LINUX_GNU
  pid_t var_tid;
#else
  int var_tid;
#endif
  lives_thread_type var_thrd_type;
  int32_t var_slot_id; // pool slot, -1 for non worker threads
  uint64_t var_uid; // copy of tdata uid

  char var_origin[128]; // thread descriptive text eg "LiVES Worker Thread"

  lives_proc_thread_t var_proc_thread;

  lives_obj_attr_t **var_attributes; // attributes passed to proc_thread

  lives_intentcap_t var_intentcap;

  // recusrion guard tokens
  LiVESList *var_trest_list;

  // sync related
  volatile boolean var_sync_ready;

  pthread_mutex_t var_pause_mutex;
  pthread_cond_t var_pcond;

  uint64_t var_sync_timeout;
  uint64_t var_blocked_limit;
  ticks_t var_event_ticks;

  volatile uint64_t var_sync_idx;

  // built in timer (per thread)
  lives_timer_t var_xtimer;

  weed_plant_t *gcol; // per thread garbage collector

  // hooks
  volatile boolean var_fg_service;
  uint64_t var_hook_hints, var_perm_hook_hints;
  int var_hook_match_nparams;
  lives_hook_stack_t *var_hook_stacks[N_NATIVE_HOOKS];
  uint64_t var_hs_flag_mask;

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
  LiVESWidgetSource *var_guisource;

  // pixel conversions
  struct _conv_array var_conv_arrays;
  int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data
  int var_rowstride_alignment_hint;
  int var_last_sws_block;

  // accel callbacks
  LiVESAccelGroup *var_accel_group;
  uint32_t var_accel_key;
  LiVESXModifierType var_accel_mod;
  livespointer var_accel_data;

  // misc
  boolean var_fx_is_auto;
  boolean var_fx_is_audio;
  boolean var_force_button_image;

  // hardware values
  volatile double var_loveliness; // a bit like 'niceness', only better
  uint64_t var_thrdnative_flags;
  uint64_t var_random_seed;
  const void *var_stackaddr;
  size_t var_stacksize;
  int var_core_id;
  volatile float *var_core_load_ptr; // pointer to value that monitors core load

  int var_sig_act;

  lives_sync_list_t *var_simple_cmd_list;

  // msgs
  int *var_pmsgmode, var_msgmode;
  void *var_msgsocket;

  // debugging
  audit_tag *var_audit_tag;
  char *var_func_trace;
  lives_sync_list_t *var_func_stack;
  double var_timerinfo, var_round_trip_time, var_secs_to_activate;
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
#define SIGDATA_STATE_FIISHED	5
#define SIGDATA_STATE_DESTROYED 6

// work flags
// TODO: just use THRDFLAG_*
#define LIVES_THRDFLAG_QUEUED_WAITING  	(1ull << 0)
#define LIVES_THRDFLAG_HOLD		(1ull << 1)
#define LIVES_THRDFLAG_RUNNING		(1ull << 2)
#define LIVES_THRDFLAG_CONCLUDED	(1ull << 3)
#define LIVES_THRDFLAG_TIMEOUT		(1ull << 4)
#define LIVES_THRDFLAG_COND_WAITING	(1ull << 5)

#define LIVES_THRDFLAG_AUTODELETE	(1ull << 8)
#define LIVES_THRDFLAG_DETACH		(1ull << 9)
#define LIVES_THRDFLAG_WAIT_SYNC	(1ull << 10)
#define LIVES_THRDFLAG_WAIT_START	(1ull << 11)
#define LIVES_THRDFLAG_NO_GUI		(1ull << 12)
#define LIVES_THRDFLAG_TUNING		(1ull << 13)
#define LIVES_THRDFLAG_NOFREE_LIST	(1ull << 15)

#define LIVES_THRDFLAG_NOTE_TIMINGS	(1ull << 32)

// worker pool threads
void lives_threadpool_init(void);
void lives_threadpool_finish(void);

void check_pool_threads(boolean important);

// lives_threads
thrd_work_t *lives_thread_create(lives_thread_t **, lives_thread_attr_t attr, lives_thread_func_t func, void *arg);
uint64_t lives_thread_done(lives_thread_t *thread);
uint64_t lives_thread_join(lives_thread_t *thrd, void **retval);
void lives_thread_free(lives_thread_t *thread);

// thread functions
lives_thread_data_t *get_thread_data_by_slot_idx(int32_t idx);
lives_thread_data_t *get_thread_data_by_pthread(pthread_t pth);
lives_thread_data_t *get_thread_data_by_uid(uint64_t uid);
lives_thread_data_t *get_thread_data_for_lpt(lives_proc_thread_t);
int get_n_active_threads(void);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_threadvars_t *get_threadvars_bg_only(void);
lives_thread_data_t *get_global_thread_data(void);
lives_threadvars_t *get_global_threadvars(void);

lives_thread_data_t *lives_thread_data_create(void);

void pthread_cleanup_func(void *args);

lives_thread_data_t *get_thread_data_for_lpt(lives_proc_thread_t);

#define THREADVAR(var) (get_threadvars()->var_##var)

#define LPT_THREADVAR_GET(lpt, var) (get_thread_data_for_lpt(lpt) ?	\
				     get_thread_data_for_lpt(lpt)->vars.var_##var : 0)
#define LPT_THREADVAR_GETp(lpt, var) (get_thread_data_for_lpt(lpt) ?	\
				      &(get_thread_data_for_lpt(lpt)->vars.var_##var) : 0)
#define LPT_THREADVAR_SET(lpt, var, val) do {				\
    if (get_thread_data_for_lpt(lpt))					\
      get_thread_data_for_lpt(lpt)->vars.var_##var = (val);} while (0);

#define FG_THREADVAR(var) (get_global_threadvars()->var_##var)
#define BG_THREADVAR(var) (get_threadvars_bg_only()->var_##var)

#define COME_BACK_LATER_START static ucontext_t conA, conB; static boolean ret = TRUE; \
  if (ret) { ret = FALSE; swapcontext(THREADVAR(context_ptr2), THREADVAR(context_ptr));} \
  while (1) { if (!ret) {ret = TRUE; getcontext(&conB);			\
      if (!ret) {ret = TRUE; break;} ret = FALSE; swapcontext(&conA, &conB); ret = TRUE;} \
    if (ret) { ret = FALSE; swapcontext(&conA, THREADVAR(context_ptr));}}

#define COME_BACK_LATER_END swapcontext(&conB, &conA);

#define COME_BACJK_LATER_MAYBE					\
  static int times = 0;						\
  THREADVAR(context_ptr) = &THREADVAR(context);			\
  THREADVAR(context_ptr2) = &THREADVAR(context2);		\
  getcontext(&THREADVAR(context));
/* if (!times++0 do_func(params)*/

#define BACJK_FOR_MORE						\
  swapcontext(THREADVAR(context_ptr), THREADVAR(context_ptr2));

#define THREAD_CTX THREADVAR(guictx)

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////

#define SYNC_CHECK_TIME ONE_MILLION // usec between polling for sync_wait TODO - make into threadvar

#define BLOCKED_LIMIT 10000 // mSec before thread in sync_point gets state blocked

// these flags are for LIVES_PROC_THREADS
// TODO: change THRD to LPT

#define THRD_STATE_NONE		0

// 'transient' states (lower set)

#define THRD_STATE_UNQUEUED 	(1ull << 0) // intial state for all proc_threads
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

// destroyed is also a final state, but it should not be checked for, the correct way is to add a callback
// to the DESTRUCTION_HOOK. If the state is IDLING, the proc_thread mya be requeued
#define THRD_FINAL_STATES (THRD_STATE_IDLING | THRD_STATE_FINISHED)

// this state only exists while running completed hook cbs
#define THRD_STATE_WILL_DESTROY (THRD_STATE_COMPLETED | THRD_STATE_DESTROYING)

// there are 3 ways a proc_thread can be waiting:
// - sync_waiting -> thread will continue when another thread calls sync_ready()
// this can also occur when the thread is queued with the wait_sync attribute
//
// - (normal, non sync) waiting - the proc_thread is waiting for self defined condition(s)
// 	to become true, it may possibly become blocked and / or timeout
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
#define THRD_STATE_BUSY 		(1ull << 16)
// waiting for condition(s) to become TRUE
#define THRD_STATE_WAITING 		(1ull << 17)
// blimit passed, blocked waiting in sync_point
#define THRD_STATE_BLOCKED 		(1ull << 18)
// thread is paused for a short time and will resume automatically
// or can be requested to resume immediately
#define THRD_STATE_AUTO_PAUSED		(1ull << 19)

// proc_thread is running a secondary funcinst, the original state is backed up
// and replaced with this, then restored when initial funcinst is popped back
#define THRD_STATE_SWAPPED		(1ull << 20)

// requested states
// request to pause - ignored for non pauseable threads
#define THRD_STATE_PAUSE_REQUESTED 	(1ull << 24)
// paused by request, or due to idling
#define THRD_STATE_PAUSED 		(1ull << 25)
// request to pause - ignored for non pauseable threads (unless they are paused / idling)
#define THRD_STATE_RESUME_REQUESTED 	(1ull << 26)
// request to cancel - ignored for non cancellable threads
#define THRD_STATE_CANCEL_REQUESTED 	(1ull << 27)

// cancelled by request, this is not a final state until acompanied by completed
#define THRD_STATE_CANCELLED 		(1ull << 32)

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

// if set, indicates that the pthread has interrupt (signals) unblocked
#define THRD_OPT_CAN_INTERRUPT	(1ull << 48)

// can be set to prevent state change hooks from being triggered
#define THRD_BLOCK_HOOKS	(1ull << 60)

// flags thread as externally created / controlled
#define THRD_STATE_EXTERN	(1ull << 63)

#define SUB_INST_STATES (THRD_STATE_SWAPPED | THRD_STATE_PAUSED | THRD_STATE_RESUME_REQUESTED)

uint64_t lives_proc_thread_get_state(lives_proc_thread_t);
uint64_t lives_proc_thread_check_states(lives_proc_thread_t, uint64_t state_bits);
uint64_t _lives_proc_thread_check_states(lives_proc_thread_t, uint64_t state_bits); // pre-locked version
uint64_t lives_proc_thread_has_states(lives_proc_thread_t, uint64_t state_bits);

// values from active funcinst
lives_funcdef_t *lives_proc_thread_get_funcdef(lives_proc_thread_t);
lives_funcptr_t lives_proc_thread_get_function(lives_proc_thread_t);
const char *lives_proc_thread_get_funcname(lives_proc_thread_t);
uint32_t lives_proc_thread_get_rtype(lives_proc_thread_t);
char *lives_proc_thread_get_args_fmt(lives_proc_thread_t);
funcsig_t lives_proc_thread_get_funcsig(lives_proc_thread_t);

//void lives_proc_thread_push_active_funcinst(lives_funcinst_t *);
//lives_funcinst_t * lives_proc_thread_pop_active_funcinst(void);

lives_funcinst_t *lives_proc_thread_get_active_funcinst(lives_proc_thread_t);
//void lives_proc_thread_set_active_funcinst(lives_proc_thread_t, lives_funcinst_t *);

lives_funcinst_t *lives_proc_thread_get_initial_funcinst(lives_proc_thread_t);
void lives_proc_thread_set_initial_funcinst(lives_proc_thread_t, lives_funcinst_t *);

void lives_funcinst_append_chain(lives_funcinst_t *f1, lives_funcinst_t *f2);

void lives_proc_thread_set_active_finstlist(lives_proc_thread_t, lives_sync_list_t *);
lives_sync_list_t *lives_proc_thread_get_active_finstlist(lives_proc_thread_t);

void lives_proc_thread_set_initial_finstlist(lives_proc_thread_t, lives_sync_list_t *);
lives_sync_list_t *lives_proc_thread_get_initial_finstlist(lives_proc_thread_t);

int lives_proc_thread_get_chain_idx(lives_proc_thread_t);
int lives_proc_thread_get_stack_depth(lives_proc_thread_t);

boolean lives_proc_thread_is_original(lives_proc_thread_t lpt);

lives_proc_thread_t _lives_funcinst_queue(lives_funcinst_t *finst, uint64_t attrs);

#define lives_funcinst_queue(finst, attrs)	\
  (record_loc(_FUNC_REF_,_FILE_REF_,_LINE_REF_) ?	\
   _lives_funcinst_queue(finst, attrs) : NULL)

void lives_funcinst_set_disposition(lives_funcinst_t *, boolean incl_stacked, funcinst_disposition disposition, ...);
void finst_module_free(void *module, funcinst_module_type mod_type);

lives_proc_thread_t lives_thread_get_proc_thread(void);
void lives_thread_set_proc_thread(lives_proc_thread_t lpt);

#define GET_PROC_THREAD_SELF(self) lives_proc_thread_t self = lives_thread_get_proc_thread(); \
  THREADVAR(func_trace) = _FUNC_REF_;

void lives_proc_thread_set_pthread(lives_proc_thread_t, pthread_t pthread);
pthread_t lives_proc_thread_get_pthread(lives_proc_thread_t);

void toggle_var_cb(void *dummy, void *var);
void inc_counter_cb(void *dummy, void *var);
void dec_counter_cb(void *dummy, void *var);
void reset_counter_cb(void *dummy, void *var);

boolean wake_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other);
boolean pause_request_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other);
boolean cancel_request_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other);
boolean hailmary_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other);
boolean dispatch_other_lpt(lives_proc_thread_t self, lives_proc_thread_t other);

// sets the state, ttiggering hooks
// exclude anything in state which is not in new_state, then include anything in new_state which is not in state
#define lives_proc_thread_set_state(lpt, new_state)			\
  lives_proc_thread_include_states(lpt,	new_state & ~(lives_proc_thread_exclude_states \
						      (lpt, lives_proc_thread_get_state(lpt) \
						       & ~new_state)))
// sets the state, wuthout ttiggering hooks
void lives_proc_thread_restore_state(lives_proc_thread_t, uint64_t tstate);

uint64_t lives_proc_thread_include_states(lives_proc_thread_t, uint64_t state_bits);
uint64_t lives_proc_thread_exclude_states(lives_proc_thread_t, uint64_t state_bits);

lives_result_t lives_proc_thread_freeze_state(lives_proc_thread_t, boolean rdonly);
lives_result_t lives_proc_thread_unfreeze_state(lives_proc_thread_t);

uint64_t get_worker_id(lives_proc_thread_t);
uint64_t get_worker_payload(uint64_t tid);
uint64_t get_worker_status(uint64_t tid);

// proc_thread leaves
#define LIVES_LEAF_THREADFUNC "tfunction"

#define LIVES_LEAF_THREAD_PARAM0 LIVES_LEAF_THREAD_PARAM(0)
#define LIVES_LEAF_THREAD_PARAM1 LIVES_LEAF_THREAD_PARAM(1)
#define LIVES_LEAF_THREAD_PARAM2 LIVES_LEAF_THREAD_PARAM(2)

#define LIVES_LEAF_PTHREAD_PTR "pthread_ptr"

#define LIVES_LEAF_PROC_THREAD "proc_thread"

#define LIVES_LEAF_MAXPARAM "_maxparam"

#define lpt_param_name(i) lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, (i))

#define LIVES_LEAF_THREAD_WORK "thread_work" // refers to underyling lives_thread
#define LIVES_LEAF_THREAD_DATA "thread_data" // pointer to thread data area (proc_thread data, not pthread data !)

#define LIVES_LEAF_THRDATTRS "thread_attrs" // thread attributes (combine with funcinst
#define LIVES_LEAF_INIT_FUNCINST "init_finst"
#define LIVES_LEAF_ACTIVE_FUNCINST "active_finst"
#define LIVES_LEAF_INIT_FINSTLIST "init_finstlist"
#define LIVES_LEAF_ACTIVE_FINSTLIST "act_finstlist"

#define LIVES_LEAF_STACK_DEPTH "stck_depth"
#define LIVES_LEAF_CHAIN_IDX "chn_idx"

#define LIVES_LEAF_STATE_RWLOCK "state_rwlock" ///< ensures state is accessed atomically
#define LIVES_LEAF_DESTRUCT_RWLOCK "destruct_rwlock" ///< ensures destruct is accessed atomically
#define LIVES_LEAF_THRD_STATE "thread_state" // proc_thread state
#define LIVES_LEAF_SIGNAL_DATA "signal_data"
#define LIVES_LEAF_THREAD_ATTRS "thread_attributes" // attributes used to create pro_thread
#define LIVES_LEAF_DATA_BOOK "data_book" // scratch data area for proc_threads

#define LIVES_LEAF_ERRNUM "errnum"
#define LIVES_LEAF_ERRMSG "errmsg"
#define LIVES_LEAF_ERRSEV "errsev"

#define LIVES_THRDATTR_NONE			0

#define LIVES_THRDATTR_FUNCINST_MASK		0xFFFF
#define LIVES_THRDATTR_PROC_THREAD_MASK 	0xFFFFFFFFFFFF0000

//////// these bits are stored in the funcinst ///////////

// can be cancelled by calling proc_thread_cancel
#define LIVES_THRDATTR_CANCELLABLE 		(1ull << 0)

// can be paused by calling proc_thread_pause
#define LIVES_THRDATTR_PAUSEABLE 		(1ull << 1)

// this is only set for hook callbacks added with HOOK_CB_FREEFUNCS - see description there
#define LIVES_THRDATTR_HAS_FREEFUNCS		(1ull << 2)

// proc thread does not call hooks on status changes
// for example async_hook callbacks - sets state THRD_BLOCK_HOOKS
#define LIVES_THRDATTR_NO_HOOKS		     	(1ull << 3)

// skip check_pool_threads if avoidable
#define LIVES_THRDATTR_FAST_QUEUE	     	(1ull << 4)

///////////////////////////////////////////////////////////

// do not wait at sync points
#define LIVES_THRDATTR_IGNORE_SYNCPTS  		(1ull << 16)

// can be set when creating or when queueing
// -----------------
// the proc_thread will be destryoed when the original funcinst returns
// no need to join
#define LIVES_THRDATTR_DONTCARE   		(1ull << 20)

// miminise time spent waiting in queue
#define LIVES_THRDATTR_PRIORITY			(1ull << 22)

// block until proc_thread is cancelled or running
#define LIVES_THRDATTR_WAIT_START		(1ull << 23)

// set cancelable before queueing
#define LIVES_THRDATTR_START_CANCELLABLE     	(1ull << 24)

// set cancelable before queueing
#define LIVES_THRDATTR_START_PAUSEABLE     	(1ull << 25)

// create only, do not dispatch
#define LIVES_THRDATTR_CREATE_UNQUEUED		(1ull << 29)

// create a proc thread for the gui thread to run
// instead of being pushed to the poolthread queue, it will be pushed to
// the main thread's stack
#define LIVES_THRDATTR_FG_THREAD   		(1ull << 32)

// light - indicates a trivial request, such as updating a spinbutton which can be carried out quickly
// this will block and get executed as soon as possible
#define LIVES_THRDATTR_FG_LIGHT	   		(1ull << 33)

// internal flagbits
#define LIVES_THRDATTR_IS_PROC_THREAD   	(1ull << 42)

// non function attrs
#define LIVES_THRDATTR_NOTE_TIMINGS		(1ull << 48)
#define LIVES_THRDATTR_NO_GUI			(1ull << 49)

// internal value
#define LIVES_THRDATTR_AUTODELETE		(1ull << 50)

///////////

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

ticks_t lives_proc_thread_get_timing_info(lives_proc_thread_t, int info_type);

thrd_work_t *lives_proc_thread_get_work(lives_proc_thread_t);
lives_thread_data_t *lives_proc_thread_get_thread_data(lives_proc_thread_t);
void lives_proc_thread_set_thread_data(lives_proc_thread_t, lives_thread_data_t *);
#define lives_proc_thread_set_work(lpt, work) do {			\
    if (lpt) weed_set_voidptr_value((lpt), LIVES_LEAF_THREAD_WORK, (work));} while(0);

// attrs
void lives_proc_thread_set_attrs(lives_proc_thread_t, uint64_t attrs);
uint64_t lives_proc_thread_get_attrs(lives_proc_thread_t);
void lives_funcinst_set_attrs(lives_funcinst_t *, uint64_t attrs);
uint64_t lives_funcinst_get_attrs(lives_funcinst_t *finst);

// ---> get timing info
ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t);

typedef struct {
  uint64_t nsec;
  boolean ign_busy;
  volatile boolean dontcare;
  boolean is_busy;
  lives_cancel_type_t cancel_type; // hard, normal, dontcare
  uint64_t min_resume;
} timeout_data;

lives_funcinst_t *lives_funcinst_create_va(lives_funcdef_t *fdef, lives_funcptr_t func,
    const char *fname, int return_type, const char **anames, const char *args_fmt, va_list xargs);

lives_funcinst_t *_lives_funcinst_create(lives_funcdef_t *fdef, lives_funcptr_t func,
    const char *fname, int return_type, const char **anames, const char *args_fmt, ...);

#define lives_funcinst_create(func, rtype, af, ...)		\
  (_lives_funcinst_create(NULL, (lives_funcptr_t)func, #func, (rtype), VARNAMES(__VA_ARGS__), (af), __VA_ARGS__))

#define lives_funcinst_create_for_funcdef(fdef, af, ...)		\
  (_lives_funcinst_create(fdef, (lives_funcptr_t)fdef->function, fdef->funcname, fdef->return_type, VARNAMES(__VA_ARGS__), (af), __VA_ARGS__))

#define lives_funcinst_from_allvals(func, nvals, allvals) _lives_funcinst_from_allvals(func, #func, nvals, allvals)

lives_proc_thread_t lives_proc_thread_create_for_funcinst(lives_funcinst_t *finst, uint64_t attrs);

lives_proc_thread_t _lives_proc_thread_create(timeout_data *to_data, lives_thread_attr_t attrs, lives_funcptr_t func,
    const char *fname,
    int return_type, const char **anames, const char *args_fmt, ...);

#define lives_proc_thread_create(attrs, func,rtype, af, ...)		\
  (record_loc(_FUNC_REF_,_FILE_REF_,_LINE_REF_) ?			\
   _lives_proc_thread_create(NULL, (attrs), (lives_funcptr_t)func, #func, (rtype), VARNAMES(__VA_ARGS__), \
			     (af), __VA_ARGS__, "", NULL) : NULL)

#define lives_proc_thread_create_pvoid(attra, func, rrype)				\
  (_lives_proc_thread_create((attrs), (lives_funcptr_t)func, #func, (rtype), NULL, "", NULL))

#define lives_proc_thread_create_rvoid(a, f, af, ...) _lives_proc_thread_create(NULL, (a) | LIVES_THRDATTR_DONTCARE, \
										(lives_funcptr_t)f, #f, \
										WEED_SEED_VOID, VARNAMES(__VA_ARGS__), (af), __VA_ARGS__)
#define lives_proc_thread_create_pvoid_rvoid(attrs, f) _lives_proc_thread_create(NULL, (attrs) | LIVES_THRDATTR_DONTCARE, \
										 (lives_funcptr_t)f, #f, WEED_SEED_VOID, NULL, "", NULL)

#define lives_proc_thread_create_rvoid_pvoid(attrs, f) lives_proc_thread_create_pvoid_rvoid(attrs, f)
#define lives_proc_thread_create_void(attrs, f) lives_proc_thread_create_pvoid_rvoid(attrs, f)

lives_proc_thread_t _lives_proc_thread_create_with_timeout(uint64_t to_nsec, lives_cancel_type_t to_ctype,
    boolean ign_busy, uint64_t min_res,
    lives_thread_attr_t attr, lives_funcptr_t func,
    const char *funcname, int return_type, const char **anames,
    const char *args_fmt, ...);

#define lives_proc_thread_create_with_timeout(to_nsec, to_ctype, to_ign_busy, to_min_res, attrs, func, return_type, args_fmt, ...) \
  _lives_proc_thread_create_with_timeout((to_nsec), (to_ctype), (to_ign_busy), (to_min_res), (attrs), \
					 (lives_funcptr_t)func, #func, (return_type), VARNAMES(__VA_ARGS__), (args_fmt), __VA_ARGS__)

lives_proc_thread_t add_garnish(lives_proc_thread_t);

boolean lives_proc_thread_unref(lives_proc_thread_t);

boolean _main_thread_execute(lives_funcptr_t, const char *fname, int return_type, void *retloc, const char **anames,
                             const char *args_fmt, ...);
boolean _main_thread_execute_rvoid(lives_funcptr_t func, const char *fname, const char **anames, const char *args_fmt, ...) ;
boolean _main_thread_execute_pvoid(lives_funcptr_t func, const char *fname, int return_type, void *retloc);

// real params, real ret_tpye
#define MAIN_THREAD_EXECUTE(func, return_type, retloc, args_fmt, ...) \
  _DW0(if (!is_fg_thread())						\
	 _main_thread_execute((lives_funcptr_t)func, #func, return_type, retloc, VARNAMES(__VA_ARGS__), args_fmt, __VA_ARGS__); \
       else *retloc = func(__VA_ARGS__);)

// real params, ret_type 0
#define MAIN_THREAD_EXECUTE_RVOID(func, args_fmt, ...) \
  _DW0(if (!is_fg_thread())						\
	 _main_thread_execute((lives_funcptr_t)func, #func, WEED_SEED_VOID, NULL, VARNAMES(__VA_ARGS__), args_fmt, __VA_ARGS__); \
       else func(__VA_ARGS__);)

// void params, real return_type
#define MAIN_THREAD_EXECUTE_PVOID(func, return_type, retloc)		\
  _DW0(if (!is_fg_thread())						\
	 _main_thread_execute((lives_funcptr_t)func, #func, return_type, retloc, NULL, "", NULL); \
       else *retloc = func();)

// void params, ret_type 0 or 1
#define MAIN_THREAD_EXECUTE_VOID(func)		\
  _DW0(if (!is_fg_thread())						\
	 _main_thread_execute((lives_funcptr_t)func, #func, 0, NULL, NULL, "", NULL); \
       else func();)

#define main_thread_execute(func, return_type, retloc, args_fmt, ...)	\
  MAIN_THREAD_EXECUTE(func, return_type, retloc, args_fmt, __VA_ARGS__)

#define main_thread_execute_rvoid(func, args_fmt, ...)	\
  MAIN_THREAD_EXECUTE_RVOID(func, args_fmt, __VA_ARGS__)

#define main_thread_execute_pvoid(func, return_type, retloc) MAIN_THREAD_EXECUTE_PVOID(func, return_type, retloc)

#define main_thread_execute_void(func) MAIN_THREAD_EXECUTE_VOID(func)

// e.g int var = pool_thread_execute(func, WEED_SEED_INT, "v", ptr)
#define pool_thread_execute(func, rtype, args_fmt, ...)			\
  (is_fg_thread()?lives_proc_thread_create(0,func,rtype,args_fmt,__VA_ARGS__)?var;var \
   ;func(__VA_ARGS__))

// e.g pool_thread_execute_rvoid(func, -1, "v", ptr)
#define pool_thread_execute_rvoid(func, args_fmt, ...)		\
  _DW0(if(is_fg_thread())lives_proc_thread_create(0,func,WEED_SEED_VOID,args_fmt,__VA_ARGS__); \
       else func(__VA_ARGS__);)

// e.g int var = pool_thread_execute_pvoid(func, WEED_SEED_INT)
#define pool_thread_execute_pvoid(func, rtype)				\
  (is_fg_thread()?lives_proc_thread_create(0,func,rtype,"")?var;var:func();)

// e.g pool_thread_execute_rvoid_pvoid(func)
#define pool_thread_execute_rvoid_pvoid(func)			\
  _DW0(if(is_fg_thread())lives_proc_thread_create(0,func,WEED_SEED_VOID,NULL);else func();)

#define pool_thread_execute_pvoid_rvoid(func) pool_thread_execute_rvoid_pvoid(func)

lives_result_t lives_proc_thread_execute(lives_proc_thread_t);
lives_result_t lives_funcinst_execute(lives_funcinst_t *finst);

boolean lives_proc_thread_dispatch(lives_proc_thread_t);

//#define DEBUG_LPT_REFS
#ifdef DEBUG_LPT_REFS
int _lives_proc_thread_ref(lives_proc_thread_t);
boolean _lives_proc_thread_unref(lives_proc_thread_t);
#define lives_proc_thread_refb(lpt) (FN_REF_TARGET(_lives_proc_thread_ref,(lpt)))
#define lives_proc_thread_unref(lpt) FN_UNREF_TARGET(_lives_proc_thread_unref,(lpt))
#else
int lives_proc_thread_ref(lives_proc_thread_t);
boolean lives_proc_thread_unref(lives_proc_thread_t);
#endif

int lives_proc_thread_count_refs(lives_proc_thread_t);

boolean lives_proc_thread_nullify_on_destruction(lives_proc_thread_t, void **ptr);

#define DEL_SELF_VALUE(name)weed_leaf_delete(lives_proc_thread_get_data(self),name)

#define SELF_HAS_VALUE(name) 

#define SET_SELF_VALUE(type, name, val)					\
  weed_set_##type##_value(lives_proc_thread_ensure_book(self), name, val)
#define SET_SELF_ARRAY(type, name, nvals, valsptr)			\
  weed_set_##type##_array(lives_proc_thread_ensure_book(self), name, nvals, valsptr)
#define GET_SELF_VALUE(type, name)			\
  lives_proc_thread_get_##type##_value(self, name)
#define GET_SELF_ARRAY(type, name, nvals)			\
  lives_proc_thread_get_##type##_array(self, name, nvals)

#define SET_LPT_VALUE(lpt, type, name, val) do {			\
    weed_set_##type##_value(lives_proc_thread_ensure_book(lpt), name, val); \
    lives_proc_thread_make_indellible(lives_proc_thread_get_book(lpt), name);} while(0);
#define SET_LPT_ARRAY(lpt, type, name, nvals, valsptr) do {		\
    weed_set_##type##_array(lives_proc_thread_ensure_book(lpt), name, nvals, valsptr); \
    lives_proc_thread_make_indellible(lives_proc_thread_get_book(lpt), name);} while(0);
#define GET_LPT_VALUE(lpt, type, name)			\
  lives_proc_thread_get_##type##_value(lpt, name)
#define GET_LPT_ARRAY(lpt, type, name, nvals)			\
  lives_proc_thread_get_##type##_array(lpt, name, nvals)

#define lives_proc_thread_get_int_value(lpt, name)			\
  (weed_get_int_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_boolean_value(lpt, name)			\
  (weed_get_boolean_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_double_value(lpt, name)			\
  (weed_get_double_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_string_value(lpt, name)			\
  (weed_get_string_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_int64_value(lpt, name)			\
  (weed_get_int64_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_uint64_value(lpt, name)			\
  (weed_get_uint64_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_funcptr_value(lpt, name)			\
  (weed_get_funcptr_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_voidptr_value(lpt, name)			\
  (weed_get_voidptr_value(lives_proc_thread_get_book(lpt), name, NULL))
#define lives_proc_thread_get_plantptr_value(lpt, name)			\
  (weed_get_plantptr_value(lives_proc_thread_get_book(lpt), name, NULL))

#define lives_proc_thread_get_int_array(lpt, name, nvals)		\
  (weed_get_int_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_boolean_array(lpt, name, nvals)		\
  (weed_get_boolean_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_double_array(lpt, name, nvals)		\
  (weed_get_double_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_string_array(lpt, name, nvals)		\
  (weed_get_string_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_int64_array(lpt, name, nvals)		\
  (weed_get_int64_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_uint64_array(lpt, name, nvals)		\
  (weed_get_int64_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_funcptr_array(lpt, name, nvals)		\
  (weed_get_funcptr_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_voidptr_array(lpt, name, nvals)		\
  (weed_get_voidptr_array_counted(lives_proc_thread_get_book(lpt), name, nvals))
#define lives_proc_thread_get_plantptr_array(lpt, name, nvals)		\
  (weed_get_plantptr_array_counted(lives_proc_thread_get_book(lpt), name, nvals))

/// data book

// each proc_thread can have a "data_book", known as the Local Data Book.
// Any type of data can be written here and later recalled
// some of the data is context dependent, eg. when a funcinst is addded as hook callback
// it gets "target_object", "src_object", "target_item" "src_item".
// Amongst other features - there is also a Global Data Book. This is not owned by any proc_thread
// - values can also be "bound" to a variable
weed_error_t lives_proc_thread_set_book(lives_proc_thread_t, weed_plant_t *book);
weed_plant_t *lives_proc_thread_get_book(lives_proc_thread_t);
weed_plant_t *lives_proc_thread_ensure_book(lives_proc_thread_t);
weed_plant_t *lives_proc_thread_share_book(lives_proc_thread_t dst,
    lives_proc_thread_t src);

void lives_proc_thread_make_indellible(lives_proc_thread_t lpt, const char *name);

// lives_proc_thread

lives_proc_thread_t lives_proc_thread_get_dispatcher(lives_proc_thread_t);

#define WAS_DISPATCHED_HERE(lpt) (!lives_strcmp(lives_proc_thread_get_active_funcinst(lpt)->funcdef->funcname, _FUNC_REF_))

// test if lpt is queued for execution
boolean lives_proc_thread_is_queued(lives_proc_thread_t);
// this is not the same as !queueu
boolean lives_proc_thread_is_unqueued(lives_proc_thread_t);
boolean lives_proc_thread_is_preparing(lives_proc_thread_t);
boolean lives_proc_thread_is_running(lives_proc_thread_t);

boolean lives_proc_thread_sync_waiting(lives_proc_thread_t);

//test if lpt is wating for self condition(s)
boolean lives_proc_thread_is_waiting(lives_proc_thread_t);
boolean lives_proc_thread_is_busy(lives_proc_thread_t);
boolean lives_proc_thread_is_paused(lives_proc_thread_t);
boolean lives_proc_thread_is_idling(lives_proc_thread_t);
boolean lives_proc_thread_idle_paused(lives_proc_thread_t);
boolean lives_proc_thread_was_cancelled(lives_proc_thread_t);
boolean lives_proc_thread_paused_idling(lives_proc_thread_t);

#define LPT_ERR_NONE		0 // no error
#define LPT_ERR_MINOR		1 // minor errors do not stop processing
#define LPT_ERR_MAJOR		2 // major errors stop processing in the thread
#define LPT_ERR_CRITICAL	3 // critical error, causes main thread to abort ASAP
#define LPT_ERR_FATAL		4 // signal SIGSEGV to main thread
#define LPT_ERR_DEADLY		5 // calls _exit() immediately

// error handling
boolean lives_proc_thread_error_full(lives_proc_thread_t self, char *file_ref, int line_ref,
                                     int errnum, int severity, const char *errmsg);

boolean _lives_proc_thread_error(char *file_ref, int line_ref,
                                 int errnum, int severity, const char *fmt, ...);

#define lives_proc_thread_error(errnum, severity, fmtstr, ...)		\
  _lives_proc_thread_error(_FILE_REF_, _LINE_REF_, errnum, severity, fmtstr __VA_OPT__(,) __VA_ARGS__, NULL)

boolean lives_proc_thread_had_error(lives_proc_thread_t);

void lpt_error_handle(lives_proc_thread_t, int sev);

void lives_make_errmsg_full(lives_proc_thread_t lpt, const char *errfile, int errline,
                            int sev, int errnum, const char *errmsg);

typedef struct {
  uint64_t uid;
  int errnum;
  int errsev;
  int linenref;
  char fileref[128];
  char errmsg[128];
} lpt_err_data;

extern lpt_err_data obits[128];

int lives_proc_thread_get_errnum(lives_proc_thread_t);
const char *lives_proc_thread_get_errmsg(lives_proc_thread_t);
int lives_proc_thread_get_errsev(lives_proc_thread_t);
const char *lives_proc_thread_get_file_ref(lives_proc_thread_t);
int lives_proc_thread_get_line_ref(lives_proc_thread_t);

void lives_proc_thread_set_line_ref(lives_proc_thread_t self, int line, boolean over);
void lives_proc_thread_set_file_ref(lives_proc_thread_t self, char *file, boolean over);
void lives_proc_thread_set_errnum(lives_proc_thread_t self, int num);
void lives_proc_thread_set_errmsg(lives_proc_thread_t self, const char *msg);
void lives_proc_thread_set_errsev(lives_proc_thread_t self, int sev);

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
boolean lives_proc_thread_check_completed(lives_proc_thread_t);

// va_list of &(void *)vars, all vars are freed / nullified when lpt completes
// (NOT finishes, so we can catch dontcare proc_threads
void lives_proc_thread_autofree(lives_proc_thread_t, ...);

boolean lives_proc_thread_get_signalled(lives_proc_thread_t);
boolean lives_proc_thread_set_signalled(lives_proc_thread_t, int signum, weed_plant_t *data);
int lives_proc_thread_get_signal_data(lives_proc_thread_t, uint64_t *tuid_return, void **data_return);

void lives_proc_thread_set_loveliness(lives_proc_thread_t, double how_lovely_it_is);

void lives_proc_thread_set_ignore_syncpts(lives_proc_thread_t, boolean ignore);
boolean lives_proc_thread_get_ignore_syncpts(lives_proc_thread_t);

void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);

// set dontcare if the return result is no longer relevant / needed,
// otherwise the thread should be joined as normal
// if thread is already set dontcare, value here is ignored.
// For non-cancellable threads, this is ignored;  use lives_proc_thread_dontcare instead.
boolean lives_proc_thread_request_cancel(lives_proc_thread_t, boolean dontcare);

boolean _lives_proc_thread_cancel(char *file_ref, int line_ref);

#define lives_proc_thread_cancel() \
  _lives_proc_thread_cancel(_FILE_REF_, _LINE_REF_ )

boolean lives_proc_thread_get_cancel_requested(lives_proc_thread_t);

// self function for running proc_threads, sets pausable if not set already
// then calls sync_point, and waits for paused flagbit to be unset
void lives_proc_thread_set_pauseable(lives_proc_thread_t, boolean state);
boolean lives_proc_thread_get_pauseable(lives_proc_thread_t);

// ask proc_thread to pause, ignored if non-pausable
// once paused, the paused hooks will be called, only once these have returned and unpause has been called
// will processing continue (after calling and returning from any unpaused hook callbacks)
boolean lives_proc_thread_request_pause(lives_proc_thread_t);
boolean lives_proc_thread_pause(void);
boolean _lives_proc_thread_pause(lives_proc_thread_t self, boolean have_lock);
// cf. is_paused
boolean lives_proc_thread_get_pause_requested(lives_proc_thread_t);

// ask a paused proc_thread to resume. Processing only continues after this has been called, and any
// paused and unpaused hook callbacks have returned
boolean lives_proc_thread_request_resume(lives_proc_thread_t);

// this is used in the case where we know target may pause and we want to wake it or prevent it
// from pausing, - if not paused, the resume request state is left active
// if target decides not to pause, it should check and clear this state
boolean lives_proc_thread_force_resume(lives_proc_thread_t);

boolean lives_proc_thread_get_resume_requested(lives_proc_thread_t);
boolean lives_proc_thread_resume(lives_proc_thread_t self);

// low level cancel, which will cause the thread to abort
// WARING - currently will result in the entire app exiting
// to use this, the uid of the underlying thread must be used
// TODO - set signal handler (similar to lives_timer, but with a signal that we send)
lives_result_t lives_proc_thread_cancel_immediate(lives_proc_thread_t lpt, lives_cancel_type_t cancel_type);

// flags the proc_thread so it will be unreffed automatically when it finshes
// such threads do not need to be joined. Setting this will tigger dontcare_hook fo the proc_thread whose state is
// being altered. Threads with this attribute set will never reach finished state, but will go from completed to
// destroying. The action can be blocked by adding hailmary_other_thread as a callback to the dontcare_hook.

boolean lives_proc_thread_dontcare(lives_proc_thread_t);

boolean _lives_proc_thread_wait(lives_proc_thread_t self, uint64_t nanosec, boolean have_lock);

boolean lives_proc_thread_wait(lives_proc_thread_t self, uint64_t nanosec);

// ignore idx mismatch
#define MM_IGNORE		0
// wait for matching sync_idx
#define MM_WAIT_MATCH		1
// return on mismatch
#define MM_RETURN		2
// error on mismatch
#define MM_ERROR		3

lives_result_t lives_proc_thread_sync_with_timeout(lives_proc_thread_t,
    uint64_t sync_idx, int mm_op, int64_t timeout_nsec);
lives_result_t lives_proc_thread_sync_with(lives_proc_thread_t lpt, uint64_t sync_idx, int mm_op);

void lives_proc_thread_set_sync_idx(uint64_t idx);
uint64_t lives_proc_thread_get_sync_idx(lives_proc_thread_t lpt);

// wait until a proc thread signals FINISHED
// or timeout (seconds) has elapsed (timeout == 0. means unlimited)
// if caller is the fg thread, it will service fg requests while waiting
// otherwise it can deadlock when the bg thread is waiting on service calls
lives_result_t lives_proc_thread_wait_finished(lives_proc_thread_t);

// proc_thread should ne unreffed after calling these
// should
boolean lives_proc_thread_join_void(lives_proc_thread_t);
int lives_proc_thread_join_int(lives_proc_thread_t);
double lives_proc_thread_join_double(lives_proc_thread_t);
int lives_proc_thread_join_boolean(lives_proc_thread_t);
char *lives_proc_thread_join_string(lives_proc_thread_t);
int64_t lives_proc_thread_join_int64(lives_proc_thread_t);
weed_funcptr_t lives_proc_thread_join_funcptr(lives_proc_thread_t);
void *lives_proc_thread_join_voidptr(lives_proc_thread_t);
weed_plantptr_t lives_proc_thread_join_plantptr(lives_proc_thread_t) ;

char *lives_proc_thread_state_desc(uint64_t state);

char *get_thread_id(uint64_t uid);
char *get_lpt_id(lives_proc_thread_t);

void dump_fn_stack(LiVESList *fnstack);

char *get_threadstats(void);
void thread_stackdump(void);

// utility funcs (called from widget-helper.c)
boolean is_fg_thread(void);

int isstck(void *ptr);

LiVESList *filter_unknown_threads(LiVESList *);

/// loveliness
#define MAX_LOVELINESS 200.
#define DEF_LOVELINESS 100.
#define MIN_LOVELINESS 1.

///////////////// refcounting ////////////////

#define LIVES_LEAF_REFCOUNTER "refcounter" ///< generic

typedef struct {
  int count; // if count < 0, object should be destroyed
  boolean mutex_inited;
  pthread_mutex_t mutex;
} lives_refcounter_t;

boolean check_refcnt_init(lives_refcounter_t *);

int refcount_inc(lives_refcounter_t *);
int refcount_dec(lives_refcounter_t *);

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

