// threading.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _THREADING_H_
#define _THREADING_H_

#if defined (HOOKFUNCS_ONLY) || !defined (HAS_THREADING_H_HOOKFUNCS)
// call with HOOKFUNCS_ONLY defined to get early definitions
#define ADD_HOOKFUNCS
// also adds refcounter
#endif

#ifndef HOOKFUNCS_ONLY

typedef void *(*lives_thread_func_t)(void *);
typedef struct _lives_thread_data_t lives_thread_data_t;
typedef weed_plantptr_t lives_proc_thread_t;
typedef uint64_t funcsig_t;

#ifdef HAVE_PTHREAD
// etc...
typedef pthread_t native_thread_t;
typedef pthread_mutex_t native_mutex_t;
typedef pthread_attr_t native_attr_t;
#endif

void lives_mutex_lock_carefully(pthread_mutex_t *mutex);

// thread internals

typedef int(*funcptr_int_t)();
typedef double(*funcptr_dbl_t)();
typedef int(*funcptr_bool_t)();
typedef char *(*funcptr_string_t)();
typedef int64_t(*funcptr_int64_t)();
typedef weed_funcptr_t(*funcptr_funcptr_t)();
typedef void *(*funcptr_voidptr_t)();
typedef weed_plant_t *(*funcptr_plantptr_t)();

typedef uint64_t lives_thread_attr_t;
typedef LiVESList lives_thread_t;

typedef union {
  weed_funcptr_t func;
  funcptr_int_t funcint;
  funcptr_dbl_t funcdouble;
  funcptr_bool_t funcboolean;
  funcptr_int64_t funcint64;
  funcptr_string_t funcstring;
  funcptr_funcptr_t funcfuncptr;
  funcptr_voidptr_t funcvoidptr;
  funcptr_plantptr_t funcplantptr;
} allfunc_t;

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
} lives_sigdata_t;

#define LIVES_LEAF_TEMPLATE "template"

typedef weed_plant_t lives_funcinst_t;

#endif // HOOKFUNCS_ONLY

#ifdef ADD_HOOKFUNCS
// set hook func defs early

#ifndef HAS_THREADING_H_HOOKFUNCS
#define HAS_THREADING_H_HOOKFUNCS

#ifdef HOOKFUNCS_ONLY
#undef  _THREADING_H_
#endif

typedef struct {
  uint64_t uid;
  int category; // category type for function (0 for none)
  const char *funcname; // optional
  lives_funcptr_t function;
  uint32_t return_type;
  const char *args_fmt;
  const char *file;
  int line;
  void *data; // optional data, may be NULL
} lives_funcdef_t;

#ifdef __FILE__
#define _FILE_REF_ __FILE__
#else
#define _FILE_REF_ ""
#endif
#ifdef __LINE__
#define _LINE_REF_ __LINE__
#else
#define _LINE_REF_ 0
#endif

#define ADD_FUNC_DEF(func, rettype, args_fmt) \
  add_fn_lookup((lives_funcptr_t)(func), QUOTEME(func), rettype, args_fmt, \
		_FILE_REF_, _LINE_REF_, NULL)

/// hook funcs

#define LIVES_SEED_HOOK WEED_SEED_FUNCPTR

#define HOOK_CB_SINGLE_SHOT		(1ull << 1) //< hook function should be called only once then removed
#define HOOK_CB_ASYNC			(1ull << 2) //< hook function should not block
#define HOOK_CB_ASYNC_JOIN		(1ull << 3) //< hook function should not block, but the thread should be joined
///							at the end of processing, or before calling the hook
///							a subsequent time
#define HOOK_CB_CHILD_INHERITS		(1ull << 4) // TODO - child threads should inherit the hook callbacks
#define HOOK_CB_FG_THREAD		(1ull << 5) // force fg service run

#define HOOK_CB_PRIORITY		(1ull << 8) // prepend, not append

#define HOOK_CB_WAIT			(1ull << 9) // block till hook has run, do not free anything

#define HOOK_UNIQUE_FUNC		(1ull << 24) // do not add if func already in hooks

#define HOOK_UNIQUE_DATA		(1ull << 25) // do not add if data already in hooks (UNIQUE_FUNC assumed)

// change data of first func of same type but leave func inplace,
// remove others of same func, but never add, only replace
#define HOOK_UNIQUE_REPLACE		(1ull << 26)

// change data of first func of same type but leave func inplace,
// remove others of same func, add if no other copies of the func
#define HOOK_UNIQUE_REPLACE_OR_ADD 	(HOOK_UNIQUE_DATA | HOOK_UNIQUE_REPLACE)

// replace (remove) other entries with same func and add
#define HOOK_UNIQUE_REPLACE_FUNC	(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_REPLACE)

// replace (remove) other entries having same func and data, and add
#define HOOK_UNIQUE_REPLACE_MATCH	(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_DATA | HOOK_UNIQUE_REPLACE)

#define HOOK_STATUS_BLOCKED			(1ull << 32) // hook function should not be called
#define HOOK_STATUS_RUNNING			(1ull << 33) // hook running, do not recurse

enum {
  ABORT_HOOK, ///< can be set to point to a function to be run before abort, for critical functions
  RESTART_HOOK,
  EXIT_HOOK,
  THREAD_EXIT_HOOK, // run on thread exit
  THREAD_INTERNAL_HOOK, /// reserved for internal use, should not be modified
  N_GLOBAL_HOOKS,
  ///
  TX_PRE_HOOK,
  TX_START_HOOK, /// status -> running
  TX_POST_HOOK,
  TX_DONE_HOOK,   /// status -> success
  WAIT_SYNC_HOOK, /// waiting to receive sync_ready from caller
  DATA_PREP_HOOK,   // data supplied, may be altered
  DATA_READY_HOOK, // data ready for processing
  PRE_VALUE_CHANGED_HOOK, /// attribute value amended
  POST_VALUE_CHANGED_HOOK, /// attribute value amended
  FINAL_HOOK, ///< about to be freed
  N_HOOK_FUNCS,
};

typedef void *(*hook_funcptr_t)(lives_object_t *, void *);
typedef void (*attr_listener_f)(lives_object_t *, lives_obj_attr_t *);

typedef struct {
  hook_funcptr_t func;
  lives_object_t *obj;
  void *attr;
  void *data;
  uint64_t flags;
  lives_proc_thread_t tinfo; // for async_join
  void *retloc; // pointer to a var to store return val in
} lives_closure_t;

lives_proc_thread_t lives_hook_append(LiVESList **hooks, int type, uint64_t flags, hook_funcptr_t func, livespointer data);
lives_proc_thread_t  lives_hook_prepend(LiVESList **hooks, int type, uint64_t flags, hook_funcptr_t func, livespointer data);
void lives_hook_remove(LiVESList **hooks, int type, hook_funcptr_t func, livespointer data);

void lives_hooks_clear(LiVESList **xlist, int type);

void lives_hooks_trigger(lives_object_t *obj, LiVESList **xlist, int type);
void lives_hooks_join(LiVESList **xlist, int type);

#endif // not def HAS_THREADING_H_HOOKFUNCS

#endif // ADD_HOOKFUNCS

#ifndef HOOKFUNCS_ONLY

#define THRDNATIVE_CAN_CORRECT (1ull << 0)

typedef struct {
  uint64_t var_uid;
  int var_id;
  lives_obj_attr_t **var_attributes;
  pthread_t var_self;
  //
  lives_proc_thread_t var_tinfo;
  lives_thread_data_t *var_mydata;
  char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file;
  uint64_t var_random_seed;
  ticks_t var_event_ticks;

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
  ticks_t var_timerinfo;
  uint64_t var_thrdnative_flags;
  void *var_stackaddr;
  size_t var_stacksize;
  uint64_t var_hook_hints;
  uint64_t var_sync_timeout;
  uint64_t var_blocked_limit;

  int var_hook_match_nparams;
  pthread_mutex_t var_hook_mutex[N_HOOK_FUNCS];
  LiVESList *var_hook_closures[N_HOOK_FUNCS];
  // hardware - values
  double var_loveliness; // a bit like 'niceness', only better
  volatile float *var_core_load_ptr; // pointer to value that monitors core load
} lives_threadvars_t;

struct _lives_thread_data_t {
  pthread_t pthread;
  LiVESWidgetContext *ctx;
  int64_t idx; // thread index
  lives_threadvars_t vars;
  boolean exited;
  int signum;
  char padding[84];
};

typedef struct {
  lives_thread_func_t func;
  void *arg;
  void *ret;
  uint64_t flags;
  lives_thread_attr_t attr;
  uint64_t caller;
  volatile uint64_t busy;
  volatile uint64_t done;
  volatile boolean sync_ready;
  LiVESList *hook_closures[N_HOOK_FUNCS];
} thrd_work_t;

#define LIVES_LEAF_THREADFUNC "tfunction"
#define LIVES_LEAF_PTHREAD_SELF "pthread_self"
#define LIVES_LEAF_THREAD_PROCESSING "t_processing"
#define LIVES_LEAF_RETURN_VALUE "return_value"
#define _RV_ LIVES_LEAF_RETURN_VALUE

#define LIVES_LEAF_FUNCSIG "funcsig"

#define LIVES_LEAF_THREAD_PARAM "thrd_param"

#define LIVES_LEAF_NULLIFY "nullify_ptr"

#define _LIVES_LEAF_THREAD_PARAM(n) LIVES_LEAF_THREAD_PARAM  n
#define LIVES_LEAF_THREAD_PARAM0 _LIVES_LEAF_THREAD_PARAM("0")
#define LIVES_LEAF_THREAD_PARAM1 _LIVES_LEAF_THREAD_PARAM("1")
#define LIVES_LEAF_THREAD_PARAM2 _LIVES_LEAF_THREAD_PARAM("2")

#define lpt_param_name(i) lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, (i))

// work flags
#define LIVES_THRDFLAG_AUTODELETE	(1 << 0)
#define LIVES_THRDFLAG_RUNNING		(1 << 1)
#define LIVES_THRDFLAG_FINISHED		(1 << 2)
#define LIVES_THRDFLAG_WAIT_SYNC	(1 << 3)
#define LIVES_THRDFLAG_NO_GUI		(1 << 4)
#define LIVES_THRDFLAG_TUNING		(1 << 5)
#define LIVES_THRDFLAG_IGNORE_SYNCPT	(1 << 6)

// internals

#define GETARG(thing, type, n) (p##n = WEED_LEAF_GET((thing), _LIVES_LEAF_THREAD_PARAM(QUOTEME(n)), type))

// since the codification of a param type only requires 4 bits, in theory we could go up to 16 parameters
// however 8 is probably sufficient and looks neater
// it is also possible to pass functions as parameters, using _FUNCP, so things like
// FUNCSIG_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP
// are a possibility

#define GEN_SET(thing, wret, funcname, FUNCARGS) err =			\
    (wret == WEED_SEED_INT ? weed_set_int_value((thing), _RV_, (*funcname->funcint)(FUNCARGS)) : \
     wret == WEED_SEED_DOUBLE ? weed_set_double_value((thing), _RV_, (*funcname->funcdouble)(FUNCARGS)) : \
     wret == WEED_SEED_BOOLEAN ? weed_set_boolean_value((thing), _RV_, (*funcname->funcboolean)(FUNCARGS)) : \
     wret == WEED_SEED_STRING ? weed_set_string_value((thing), _RV_, (*funcname->funcstring)(FUNCARGS)) : \
     wret == WEED_SEED_INT64 ? weed_set_int64_value((thing), _RV_, (*funcname->funcint64)(FUNCARGS)) : \
     wret == WEED_SEED_FUNCPTR ? weed_set_funcptr_value((thing), _RV_, (*funcname->funcfuncptr)(FUNCARGS)) : \
     wret == WEED_SEED_VOIDPTR ? weed_set_voidptr_value((thing), _RV_, (*funcname->funcvoidptr)(FUNCARGS)) : \
     wret == WEED_SEED_PLANTPTR ? weed_set_plantptr_value((thing), _RV_, (*funcname->funcplantptr)(FUNCARGS)) : \
     WEED_ERROR_WRONG_SEED_TYPE)

#define ARGS1(thing, t1) GETARG((thing), t1, 0)
#define ARGS2(thing, t1, t2) ARGS1((thing), t1), GETARG((thing), t2, 1)
#define ARGS3(thing, t1, t2, t3) ARGS2((thing), t1, t2), GETARG((thing), t3, 2)
#define ARGS4(thing, t1, t2, t3, t4) ARGS3((thing), t1, t2, t3), GETARG((thing), t4, 3)
#define ARGS5(thing, t1, t2, t3, t4, t5) ARGS4((thing), t1, t2, t3, t4), GETARG((thing), t5, 4)
#define ARGS6(thing, t1, t2, t3, t4, t5, t6) ARGS5((thing), t1, t2, t3, t4, t5), GETARG((thing), t6, 5)
#define ARGS7(thing, t1, t2, t3, t4, t5, t6, t7) ARGS6((thing), t1, t2, t3, t4, t5, t6), GETARG((thing), t7, 6)
#define ARGS8(thing, t1, t2, t3, t4, t5, t6, t7, t8) ARGS7((thing), t1, t2, t3, t4, t5, t6, t7), GETARG((thing), t8, 7)
#define CALL_VOID_8(thing, t1, t2, t3, t4, t5, t6, t7, t8) (*thefunc->func)(ARGS8((thing), t1, t2, t3, t4, t5, t6, t7, t8))
#define CALL_VOID_7(thing, t1, t2, t3, t4, t5, t6, t7) (*thefunc->func)(ARGS7((thing), t1, t2, t3, t4, t5, t6, t7))
#define CALL_VOID_6(thing, t1, t2, t3, t4, t5, t6) (*thefunc->func)(ARGS6((thing), t1, t2, t3, t4, t5, t6))
#define CALL_VOID_5(thing, t1, t2, t3, t4, t5) (*thefunc->func)(ARGS5((thing), t1, t2, t3, t4, t5))
#define CALL_VOID_4(thing, t1, t2, t3, t4) (*thefunc->func)(ARGS4((thing), t1, t2, t3, t4))
#define CALL_VOID_3(thing, t1, t2, t3) (*thefunc->func)(ARGS3((thing), t1, t2, t3))
#define CALL_VOID_2(thing, t1, t2) (*thefunc->func)(ARGS2((thing), t1, t2))
#define CALL_VOID_1(thing, t1) (*thefunc->func)(ARGS1((thing), t1))
#define XCALL_VOID_1(t1) CALL_VOID_1((info), t1)
#define XCALL_VOID_2(t1, t2) CALL_VOID_2((info), t1, t2)
#define XCALL_VOID_3(t1, t2, t3) CALL_VOID_3((info), t1, t2, t3)
#define XCALL_VOID_4(t1, t2, t3, t4) CALL_VOID_4((info), t1, t2, t3, t4)
#define XCALL_VOID_5(t1, t2, t3, t4, t5) CALL_VOID_5((info), t1, t2, t3, t4, t5)
#define XCALL_VOID_6(t1, t2, t3, t4, t5, t6) CALL_VOID_6((info), t1, t2, t3, t4, t5, t6)
#define XCALL_VOID_0() (*thefunc->func)()
#define XCALL_VOID_7(t1, t2, t3, t4, t5, t6, t7) CALL_VOID_7((info), t1, t2, t3, t4, t5, t6, t7)
#define XCALL_VOID_8(t1, t2, t3, t4, t5, t6, t7, t8) CALL_VOID_8((info), t1, t2, t3, t4, t5, t6, t7, t8)
#define XCALL_8(t1, t2, t3, t4, t5, t6, t7, t8) ACALL_8(info, ret_type, thefunc, t1, t2, t3, t4, t5, t6, t7, t8)
#define XCALL_7(t1, t2, t3, t4, t5, t6, t7) ACALL_7(info, ret_type, thefunc, t1, t2, t3, t4, t5, t6, t7)
#define XCALL_6(t1, t2, t3, t4, t5, t6) ACALL_6(info, ret_type, thefunc, t1, t2, t3, t4, t5, t6)
#define XCALL_5(t1, t2, t3, t4, t5) ACALL_5(info, ret_type, thefunc, t1, t2, t3, t4, t5)
#define XCALL_4(t1, t2, t3, t4) ACALL_4(info, ret_type, thefunc, t1, t2, t3, t4)
#define XCALL_3(t1, t2, t3) ACALL_3(info, ret_type, thefunc, t1, t2, t3)
#define XCALL_2(t1, t2) ACALL_2(info, ret_type, thefunc, t1, t2)
#define XCALL_1(t1) ACALL_1(info, ret_type, thefunc, t1)
#define XCALL_0() ACALL_0(info, ret_type, thefunc)
#define ACALL_8(thing, wret, funcname, t1, t2, t3, t4, t5, t6, t7, t8)	\
  GEN_SET(thing, wret, funcname, ARGS8((thing), t1, t2, t3, t4, t5, t6, t7, t8))
#define ACALL_7(thing, wret, funcname, t1, t2, t3, t4, t5, t6, t7) \
  GEN_SET(thing, wret, funcname, ARGS7((thing), t1, t2, t3, t4, t5, t6, t7))
#define ACALL_6(thing, wret, funcname, t1, t2, t3, t4, t5, t6) \
  GEN_SET(thing, wret, funcname, ARGS6((thing), t1, t2, t3, t4, t5, t6))
#define ACALL_5(thing, wret, funcname, t1, t2, t3, t4, t5) \
  GEN_SET(thing, wret, funcname, ARGS5((thing), t1, t2, t3, t4, t5))
#define ACALL_4(thing, wret, funcname, t1, t2, t3, t4) \
  GEN_SET(thing, wret, funcname, ARGS4((thing), t1, t2, t3, t4))
#define ACALL_3(thing, wret, funcname, t1, t2, t3) GEN_SET(thing, wret, funcname, ARGS3((thing), t1, t2, t3))
#define ACALL_2(thing, wret, funcname, t1, t2) GEN_SET(thing, wret, funcname, ARGS2((thing), t1, t2))
#define ACALL_1(thing, wret, funcname, t1) GEN_SET(thing, wret, funcname, ARGS1((thing), t1))
#define ACALL_0(thing, wret, funcname) GEN_SET(thing, wret, funcname, )

// 0p
#define FUNCSIG_VOID				       			0X00000000
// 1p
#define FUNCSIG_INT 			       				0X00000001
#define FUNCSIG_DOUBLE 				       			0X00000002
#define FUNCSIG_BOOL 				       			0X00000003
#define FUNCSIG_STRING 				       			0X00000004
#define FUNCSIG_INT64 			       				0X00000005
#define FUNCSIG_VOIDP 				       			0X0000000D
#define FUNCSIG_PLANTP 				       			0X0000000E
// 2p
#define FUNCSIG_BOOL_INT 			       			0X00000031
#define FUNCSIG_INT_INT64 			       			0X00000015
#define FUNCSIG_INT_VOIDP 			       			0X0000001D
#define FUNCSIG_STRING_INT 			      			0X00000041
#define FUNCSIG_STRING_BOOL 			      			0X00000043
#define FUNCSIG_VOIDP_VOIDP 				       		0X000000DD
#define FUNCSIG_VOIDP_STRING 				       		0X000000D4
#define FUNCSIG_VOIDP_DOUBLE 				       		0X000000D2
#define FUNCSIG_VOIDP_INT64 				       		0X000000D5
#define FUNCSIG_DOUBLE_DOUBLE 				       		0X00000022
#define FUNCSIG_PLANTP_BOOL 				       		0X000000E3
// 3p
#define FUNCSIG_VOIDP_DOUBLE_INT 		        		0X00000D21
#define FUNCSIG_VOIDP_STRING_STRING 		        		0X00000D44
#define FUNCSIG_VOIDP_VOIDP_VOIDP 		        		0X00000DDD
#define FUNCSIG_VOIDP_VOIDP_BOOL 		        		0X00000DD3
#define FUNCSIG_STRING_VOIDP_VOIDP 		        		0X000004DD
#define FUNCSIG_BOOL_BOOL_STRING 		        		0X00000334
#define FUNCSIG_PLANTP_VOIDP_INT64 		        		0X00000ED5
#define FUNCSIG_INT_VOIDP_INT64 		        		0X000001D5
#define FUNCSIG_INT_INT_BOOL	 		        		0X00000113
// 4p
#define FUNCSIG_STRING_STRING_VOIDP_INT					0X000044D1
#define FUNCSIG_STRING_DOUBLE_INT_STRING       				0X00004214
#define FUNCSIG_INT_INT_BOOL_VOIDP					0X0000113D
// 5p
#define FUNCSIG_VOIDP_INT_INT_INT_INT					0X000D1111
#define FUNCSIG_INT_INT_INT_BOOL_VOIDP					0X0001113D
#define FUNCSIG_VOIDP_STRING_STRING_INT64_INT			       	0X000D4451
// 6p
#define FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP		       	0X0044D14D

////// lives_thread_t

#define LIVES_THRDATTR_NONE		0
#define LIVES_THRDATTR_PRIORITY		(1 << 0)
#define LIVES_THRDATTR_AUTODELETE	(1 << 1)

// worker pool threads
void lives_threadpool_init(void);
void lives_threadpool_finish(void);

// lives_threads
int lives_thread_create(lives_thread_t *, lives_thread_attr_t attr, lives_thread_func_t func, void *arg);
uint64_t lives_thread_done(lives_thread_t thread);
uint64_t lives_thread_join(lives_thread_t work, void **retval);
void lives_thread_free(lives_thread_t *thread);

// thread functions
lives_thread_data_t *get_thread_data_by_id(uint64_t idx);
int get_n_active_threads(void);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_thread_data_t *get_global_thread_data(void);
lives_threadvars_t *get_global_threadvars(void);
lives_thread_data_t *lives_thread_data_create(uint64_t thread_id);

#define THREADVAR(var) (get_threadvars()->var_##var)
#define FG_THREADVAR(var) (get_global_threadvars()->var_##var)

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BLOCKED_LIMIT 10000 // mSec

// lives_proc_thread state flags
#define THRD_STATE_FINISHED 	(1ull << 0)
#define THRD_STATE_CANCELLED 	(1ull << 1)
#define THRD_STATE_SIGNALLED 	(1ull << 2)
#define THRD_STATE_BUSY 	(1ull << 3)
#define THRD_STATE_ERROR 	(1ull << 4)
#define THRD_STATE_BLOCKED 	(1ull << 5)
#define THRD_STATE_IDLE 	(1ull << 8)

#define THRD_STATE_WAITING 	(1ull << 9)

#define THRD_STATE_RUNNING 	(1ull << 16)

#define THRD_STATE_INVALID 	(1ull << 31)

#define THRD_OPT_NOTIFY 	(1ull << 32)
#define THRD_OPT_CANCELLABLE 	(1ull << 34)
#define THRD_OPT_DONTCARE 	(1ull << 35)

boolean lives_proc_thread_set_state(lives_proc_thread_t lpt, uint64_t state);
uint64_t lives_proc_thread_get_state(lives_proc_thread_t lpt);
uint64_t lives_proc_thread_check_states(lives_proc_thread_t lpt, uint64_t state_bits);
uint64_t lives_proc_thread_include_states(lives_proc_thread_t lpt, uint64_t state_bits);
boolean lives_proc_thread_exclude_states(lives_proc_thread_t lpt, uint64_t state_bits);

uint64_t get_worker_id(lives_proc_thread_t);

uint64_t get_worker_payload(uint64_t tid);

uint64_t get_worker_status(uint64_t tid);

// proc_thread specific attributes
#define LIVES_LEAF_THREAD_WORK "thread_work" // refers to underyling lives_thread

#define LIVES_LEAF_STATE_MUTEX "state_mutex" ///< ensures state is accessed atomically
#define LIVES_LEAF_THRD_STATE "thread_state" // proc_thread state

#define LIVES_LEAF_SIGNAL_DATA "signal_data"

#define LIVES_LEAF_THREAD_ATTRS "thread_attibutes" // attributes used to create pro_thread

// also LIVES_THRDATR_PRIORITY
// also LIVES_THRDATR_AUTODELETE
#define LIVES_THRDATTR_WAIT_START	(1 << 2)
#define LIVES_THRDATTR_WAIT_SYNC	(1 << 3)
#define LIVES_THRDATTR_FG_THREAD	(1 << 4)
#define LIVES_THRDATTR_NO_GUI		(1 << 5)
#define LIVES_THRDATTR_INHERIT_HOOKS   	(1 << 6)
#define LIVES_THRDATTR_IGNORE_SYNCPT   	(1 << 7)
#define LIVES_THRDATTR_NOFREE   	(1 << 8)

// extra info requests
#define LIVES_LEAF_START_TICKS "_start_ticks"
#define LIVES_THRDATTR_NOTE_STTIME	(1 << 16)

#define lives_proc_thread_get_work(tinfo)				\
  ((thrd_work_t *)weed_get_voidptr_value((tinfo), LIVES_LEAF_THREAD_WORK, NULL))

#define lives_proc_thread_set_work(tinfo, work)				\
  weed_set_voidptr_value((tinfo), LIVES_LEAF_THREAD_WORK, (work))

ticks_t lives_proc_thread_get_start_ticks(lives_proc_thread_t);

lives_proc_thread_t lives_proc_thread_create(lives_thread_attr_t, lives_funcptr_t,
    int return_type, const char *args_fmt, ...);

lives_proc_thread_t lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs);

lives_proc_thread_t lives_proc_thread_create_nullvargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type);

#define LPT_WITH_TIMEOUT(to, attr, func, rtype, args_fmt, ...) \
      (lives_proc_thread_create_with_timeout_named((to), (attr),	\
						   (lives_funcptr_t)(func), #func, \
						   (rtype), (args_fmt), __VA_ARGS__))

lives_proc_thread_t lives_proc_thread_create_with_timeout_named(ticks_t timeout, lives_thread_attr_t attr, lives_funcptr_t func,
    const char *func_name, int return_type, const char *args_fmt, ...);

lives_proc_thread_t lives_proc_thread_create_with_timeout(ticks_t timeout, lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, ...);

void lives_proc_thread_free(lives_proc_thread_t lpt);

// forces fg execution (safe to run in fg or bg)
boolean main_thread_execute(lives_funcptr_t, int return_type, void *retval, const char *args_fmt, ...);

// safer version which only calls it for bg threads (note that must be retloc instead of &retloc)
#define MAIN_THREAD_EXECUTE(func, st, retloc, args_fmt, ...) do { \
  if (is_fg_thread()) (retloc) = (func)(__VA_ARGS__);			\
  else main_thread_execute((lives_funcptr_t)(func), st, &/**/retloc, args_fmt, __VA_ARGS__); \
  } while(0);


#define MAIN_THREAD_EXECUTE_VOID(func, args_fmt, ...) do { \
  if (is_fg_thread()) (func)(__VA_ARGS__);		  \
  else main_thread_execute((lives_funcptr_t)(func), 0, NULL, args_fmt, __VA_ARGS__); \
  } while(0);

#define MAIN_THREAD_EXECUTE_VOID_VOID(func) do { \
  if (is_fg_thread()) (func)();		  \
  else main_thread_execute((lives_funcptr_t)(func), 0, NULL, ""); \
  } while(0);


// returns TRUE once the proc_thread will call the target function
// the thread can also be cancelled or finished
boolean lives_proc_thread_is_running(lives_proc_thread_t);

/// returns FALSE while the thread is running, TRUE once it has finished
boolean lives_proc_thread_check_finished(lives_proc_thread_t);
boolean lives_proc_thread_get_signalled(lives_proc_thread_t);
boolean lives_proc_thread_set_signalled(lives_proc_thread_t, int signum, void *data);
int lives_proc_thread_get_signal_data(lives_proc_thread_t, int64_t *tidx_return, void **data_return);

void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);

// set dontcare if the return result is no longer relevant / needed, otherwise the thread should be joined as normal
// if thread is already set dontcare, value here is ignored. For non-cancellable threads use lives_proc_thread_dontcare instead.
boolean lives_proc_thread_cancel(lives_proc_thread_t, boolean dontcare);
boolean lives_proc_thread_get_cancelled(lives_proc_thread_t);

// low level cancel, which will cause the thread to abort
boolean lives_proc_thread_cancel_immediate(lives_proc_thread_t);

/// tell a thread with return value that we no longer need the value so it can free itself
/// after setting this, no further operations may be done on the proc_thread
boolean lives_proc_thread_dontcare(lives_proc_thread_t);

// as above but will set *thing = NULL when finished or cancelled
void lives_proc_thread_dontcare_nullify(lives_proc_thread_t tinfo, void **thing);

void lives_proc_thread_sync_ready(lives_proc_thread_t);

boolean sync_point(const char *motive);

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

////
char *get_threadstats(void);

lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
                                uint32_t return_type,  const char *args_fmt, const char *file, int line,
                                void *data);

void free_funcdef(lives_funcdef_t *);
lives_funcinst_t *create_funcinst(lives_funcdef_t *template, void *retstore, ...);
void free_funcinst(lives_funcinst_t *);

funcsig_t funcsig_from_args_fmt(const char *args_fmt);
char *funcsig_to_string(funcsig_t sig);

uint32_t get_seedtype(char c);

int fn_func_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2);
boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp);
boolean fn_data_replace(lives_proc_thread_t src, lives_proc_thread_t dst);

// utility funcs (called from widget-helper.c)
boolean is_fg_thread(void);
int get_funcsig_nparms(funcsig_t sig);
boolean call_funcsig(lives_proc_thread_t info);
boolean fg_run_func(lives_proc_thread_t lpt, void *retval);

int isstck(void *ptr);

#endif // ndef HOOKS_ONLY

///////////////// refcounting ////////////////

#ifdef ADD_HOOKFUNCS

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

boolean weed_add_refcounter(weed_plant_t *);
boolean weed_remove_refcounter(weed_plant_t *);

#undef ADD_HOOKFUNCS
#endif

#ifdef HOOKFUNCS_ONLY
#undef HOOKFUNCS_ONLY
#else

///////////////////////////
void make_thrdattrs(void);

#define LIVES_WEED_SUBTYPE_FUNCINST 150

#define THREAD_INTENTION THREADVAR(intentcap).intent
#define THREAD_CAPACITIES THREADVAR(intentcap).capacities

// intents - for future use
// type = thread, subtype livesproc
#define PROC_THREAD_INTENTION_CREATE LIVES_INTENTION_CREATE // timeout is an optional ivar, default 0, how to handle
//						argc, argv in reqmts. ?
#define PROC_THREAD_INTENTION_DESTROY LIVES_INTENTION_DESTROY // free func
#define PROC_THREAD_INTENTION_CANCEL LIVES_INTENTION_CANCEL // -> cancel_immediate

// main_thread_execute etc will be a transform to the running state

// check will be the RUNNING status, cancelled will be CANCELLED status
// sync_wait - set in flags, and waits in prep state
//

#define PROC_THREAD_INTENTION_GET_VALUE LIVES_INTENTION_GET_VALUE // e.g cancellable
#define PROC_THREAD_INTENTION_SET_VALUE LIVES_INTENTION_SET_VALUE // e.g cancellable

#endif // HOOKFUNCS_ONLY

#endif

