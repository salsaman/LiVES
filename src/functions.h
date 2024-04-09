// functions.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H

#include "funcsigs.h"

///// low level operations //////

#define LIST_TYPE LiVESList *

typedef struct _hstack_t lives_hook_stack_t;

typedef struct {
  uint64_t token;
  const void *dataptr;
} recursion_token;

typedef struct {
  uint64_t token;
  LIST_TYPE list;
  pthread_rwlock_t rwlock;
} recursion_tokens;

static inline boolean have_recursion_token(LIST_TYPE xlist, uint64_t token, void *dataptr) {
  LIVES_CONST_LIST_FOREACH(xlist, list)
  if (DATA_FIELD_IS(list, recursion_token, token, token)
      && DATA_FIELD_IS(list, recursion_token, dataptr, dataptr)) return TRUE;
  return FALSE;
}

static inline LIST_TYPE remove_recursion_token(LIST_TYPE xlist, uint64_t token, void *dataptr) {
  LIVES_CONST_LIST_FOREACH(xlist, list)
  if (DATA_FIELD_IS(list, recursion_token, token, token)
      && DATA_FIELD_IS(list, recursion_token, dataptr, dataptr))
    return lives_list_remove_node(xlist, list, TRUE);
  return xlist;
}

#define RTOKENS trectoks

#define _RECURSE_GUARD_START_ static recursion_tokens RTOKENS=(recursion_tokens){.token=0, .list=NULL}

// global

#define _RECURSE_GUARD_ARM_FOR_DATA_(rtokens, xdataptr)			\
  _DW0(LIVES_CALLOC_TYPE(recursion_token,rectok,1);			\
       if(!rtokens.token){RTOKENS.token=gen_unique_id();		\
	 pthread_rwlock_init(&RTOKENS.rwlock, NULL);}			\
       rectok->token=rtokens.token;rectok->dataptr=xdataptr;		\
       pthread_rwlock_wrlock(&rtokens.rwlock);				\
       rtokens.list=lives_list_prepend(rtokens.list,(void*)rectok);	\
       pthread_rwlock_unlock(&rtokens.rwlock);)

//

#define RECURSE_GUARD_START _RECURSE_GUARD_START_

#define RETURN_IF_RECURSED RETURN_IF_RECURSED_WITH_DATA(NULL)
#define RETURN_VAL_IF_RECURSED(val) RETURN_VAL_IF_RECURSED_WITH_DATA(val, NULL)

#define RECURSE_GUARD_ARM RECURSE_GUARD_ARM_FOR_DATA(NULL)
#define RECURSE_GUARD_END RECURSE_GUARD_END_FOR_DATA(NULL)

#define RETURN_IF_RECURSED_WITH_DATA(dataptr)				\
  _DW0(if(!RTOKENS.token){RTOKENS.token=gen_unique_id();		\
      pthread_rwlock_init(&RTOKENS.rwlock, NULL);}			\
    else {								\
      pthread_rwlock_rdlock(&RTOKENS.rwlock);				\
      if(have_recursion_token(RTOKENS.list,RTOKENS.token,dataptr))	\
	{pthread_rwlock_unlock(&RTOKENS.rwlock);return;}		\
      pthread_rwlock_unlock(&RTOKENS.rwlock);})

#define RETURN_VAL_IF_RECURSED_WITH_DATA(val, dataptr)			\
  _DW0(if(!RTOKENS.token){RTOKENS.token=gen_unique_id();		\
      pthread_rwlock_init(&RTOKENS.rwlock, NULL);}			\
    else {								\
      pthread_rwlock_rdlock(&RTOKENS.rwlock);				\
      if(have_recursion_token(RTOKENS.list,RTOKENS.token,dataptr))	\
	{pthread_rwlock_unlock(&RTOKENS.rwlock);return(val);}		\
      pthread_rwlock_unlock(&RTOKENS.rwlock);})

#define RECURSE_GUARD_ARM_FOR_DATA(dataptr) _RECURSE_GUARD_ARM_FOR_DATA_(RTOKENS,dataptr)
#define RECURSE_GUARD_END_FOR_DATA(dataptr) _DW0(pthread_rwlock_wrlock(&RTOKENS.rwlock); \
						 RTOKENS.list=remove_recursion_token(RTOKENS.list,RTOKENS.token,dataptr); \
						 pthread_rwlock_unlock(&RTOKENS.rwlock);)

// per thread

#define _T_RECURSE_GUARD_ARM_FOR_DATA_(rtokens, xdataptr)		\
  _DW0(LIVES_CALLOC_TYPE(recursion_token,rectok,1);			\
       if(!rtokens.token){RTOKENS.token=gen_unique_id();		\
	 pthread_rwlock_init(&RTOKENS.rwlock,NULL);}			\
       rectok->token=rtokens.token;rectok->dataptr=xdataptr;		\
 	 pthread_rwlock_wrlock(&rtokens.rwlock);			\
	 THREADVAR(trest_list)=lives_list_prepend(THREADVAR(trest_list),(void*)rectok); \
       pthread_rwlock_unlock(&rtokens.rwlock);)

//

#define T_RECURSE_GUARD_START _RECURSE_GUARD_START_

#define T_RETURN_IF_RECURSED T_RETURN_IF_RECURSED_WITH_DATA(NULL)
#define T_RETURN_VAL_IF_RECURSED(val) T_RETURN_VAL_IF_RECURSED_WITH_DATA(val,NULL)

#define T_RECURSE_GUARD_ARM _T_RECURSE_GUARD_ARM_FOR_DATA_(RTOKENS,NULL)
#define T_RECURSE_GUARD_END RECURSE_GUARD_END_FOR_DATA(NULL)

#define T_RETURN_IF_RECURSED_WITH_DATA(dataptr)				\
  _DW0(if(!RTOKENS.token){RTOKENS.token=gen_unique_id();		\
   pthread_rwlock_init(&RTOKENS.rwlock, NULL);}				\
   else {								\
     pthread_rwlock_rdlock(&RTOKENS.rwlock);				\
     if(have_recursion_token(THREADVAR(trest_list)),RTOKENS.token,dataptr) \
       {pthread_rwlock_unlock(&RTOKENS.rwlock);return;}			\
     pthread_rwlock_unlock(&RTOKENS.rwlock);})

#define T_RETURN_VAL_IF_RECURSED_WITH_DATA(val, dataptr) _DW0		\
  (if(!RTOKENS.token){RTOKENS.token=gen_unique_id();			\
    pthread_rwlock_init(&RTOKENS.rwlock, NULL);}			\
  else {								\
    pthread_rwlock_rdlock(&RTOKENS.rwlock);				\
    if(have_recursion_token(THREADVAR(trest_list),RTOKENS.token,dataptr)) \
      {pthread_rwlock_unlock(&RTOKENS.rwlock);return(val);}		\
    pthread_rwlock_unlock(&RTOKENS.rwlock);})

#define T_RECURSE_GUARD_ARM_FOR_DATA(dataptr) _T_RECURSE_GUARD_ARM_FOR_DATA_(RTOKENS,dataptr)
#define T_RECURSE_GUARD_END_FOR_DATA(dataptr) _DW0(pthread_rwlock_wrlock(&RTOKENS.rwlock); \
						   THREADVAR(trest_list) = remove_recursion_token(THREADVAR(trest_list), \
												  RTOKENS.token,dataptr); \
						   pthread_rwlock_unlock(&RTOKENS.rwlock);)

char *make_std_pname(int pn);

typedef uint64_t lives_thread_attr_t;
typedef LiVESList lives_thread_t;

void lpt_params_free(lives_proc_thread_t, boolean do_exec);

#define LIVES_LEAF_RETURN_VALUE "retval"
#define LIVES_LEAF_ARGS_FMT "args_fmt"

#define _RV_ LIVES_LEAF_RETURN_VALUE

#define PROC_THREAD_PARAM(n) LIVES_LEAF_THREAD_PARAM  #n

#define LIVES_LEAF_LONGJMP "_longjmp_env_ptr"

weed_error_t weed_leaf_from_varg(weed_plant_t *, const char *key, uint32_t type, weed_size_t ne, va_list xargs);
lives_result_t weed_leaf_from_va(weed_plant_t *, const char *key, char fmtchar, ...);

weed_error_t weed_leaf_from_vargp(weed_plant_t *, const char *key, uint32_t type, weed_size_t ne, va_list xargs);
lives_result_t weed_leaf_from_vap(weed_plant_t *, const char *key, weed_seed_t st, ...);

lives_result_t update_params_from_proxies(lives_funcinst_t *finst);

boolean call_funcsig(lives_proc_thread_t);
lives_result_t do_call(lives_funcinst_t *);

#define LIVES_LEAF_CB_ADDED_LIST "cb_add_list"

#define FUNC_CATEGORY_HOOK_CALLBACK FUNC_CATEGORY_CALLBACK

//#define FUNC_CATEGORY_EXTERNAL

#define FUNC_CATEGORY_ASYNC_HOOK 	(N_STD_FUNC_CATEGORIES + 101)
#define FUNC_CATEGORY_HOOK_GUI	 	(N_STD_FUNC_CATEGORIES + 102)

void toggle_var_cb(void *dummy, boolean *var);
void inc_counter_cb(void *dummy, int *var);
void dec_counter_cb(void *dummy, int *var);
void reset_counter_cb(void *dummy, int *var);

/// tracing / debugging tools //

#define LIVES_LEAF_FILE_REF "file_ref"
#define LIVES_LEAF_LINE_REF "line_ref"

#ifdef __FILE__
#define _FILE_REF_ __FILE__
#define _ORIG_FILE_REF_ __FILE__
#else
#define _FILE_REF_ ""
#define _ORIG_FILE_REF_ ""
#endif
#ifdef __LINE__
#define _LINE_REF_ __LINE__
#define _ORIG_LINE_REF_ __LINE__
#else
#define _LINE_REF_ 0
#define _ORIG_LINE_REF_ 0
#endif
#ifdef __func__
#define _FUNC_REF_ __func__
#define _ORIG_FUNC_REF_ __func__
#else
#define _FUNC_REF_ "??????"
#define _ORIG_FUNC_REF_ "??????"
#endif

typedef enum {
  // enum   // va_args for add_fn_note
  _FN_FREE, // fundef, ptr
  _FN_ALLOC, // fundef, ptr
  _FN_REF, // fundef, ptr - returns (int)func(ptr)
  _FN_UNREF, // fundef, ptr
} fn_type_t;

#define _FUNCREF(fn,f,l)((void *)(create_funcdef(#fn,(lives_funcptr_t)fn,0,0,f,l,0)))

#define ADD_NOTE(ftype,fname,...)					\
  (add_fn_note(ftype,_FUNCREF(fname,_FILE_REF_,_LINE_REF_),fname(__VA_ARGS__)))

#define ADD_NOTEI(ftype,fname,ptr)					\
  (add_fn_note(ftype,_FUNCREF(fname,_FILE_REF_,_LINE_REF_),(ptr))?fname(ptr):fname(ptr))

#define DO_ADD_NOTE(ftype,fname,...) do{(void)ADD_NOTE(ftype,fname,__VA_ARGS__);}while(0)

#define FN_ALLOC_TARGET(fname,...) ADD_NOTE(_FN_ALLOC,fname,__VA_ARGS__)
#define FN_UNALLOC_TARGET(fname,...) ADD_NOTE(_FN_FREE,fname,__VA_ARGS__)
#define FN_FREE_TARGET(fname,...) DO_ADD_NOTE(_FN_FREE,fname,__VA_ARGS__)

#define FN_REF_TARGET(fname,...) (ADD_NOTEI(_FN_REF,fname,__VA_ARGS__))
#define FN_UNREF_TARGET(fname,...) (ADD_NOTEI(_FN_UNREF,fname,__VA_ARGS__))

void add_quick_fn(lives_funcptr_t func, const char *funcname);
const char *get_funcname(lives_funcptr_t);

boolean args_fmt_match(const char *def, const char *inst);

const char *get_args_fmt(weed_plant_t *);
void set_args_fmt(weed_plant_t *plant, const char *args_fmt);

// add a note to to ftrace_store, va_args depend on fn_type
void *add_fn_note(fn_type_t, ...);

// dump notes (traces) from ftrace_store
void dump_fn_notes(void);

#define FN_DEBUG_OUT(func) g_print("Thread 0x%lx in func %s at %s, line %d\n", \
				   THREADVAR(uid), #func, _FILE_REF_, _LINE_REF_)

#define FN_DEBUG_EXIT_OUT g_print("Thread %ld exiting func at %s, line %d\n", \
				  THREADVAR(uid), _FILE_REF_, _LINE_REF_)

void _func_entry(lives_funcptr_t, const char *funcname, int category, const char *rettype,
                 const char *args_fmt, char *file_ref, int line_ref, uint64_t flags, ...);
void _func_exit(char *file_ref, int line_ref);

void _func_exit_val(weed_plant_t *, char *file_ref, int line_ref);

#ifndef NO_FUNC_TAGS
// macro to be placed near start of "major" functions. It will prepend funcname to
// a thread's 'func_stack', print out a debug line (optional), and also add fn lookup
// to fn_store, e.g:   ____FUNC_ENTRY____(transcode_clip, "b", "iibs");

#define ___FUNC_ENTRY_FULL___(func, rettype, args_fmt, flags, ...)	\
  _DW0(_func_entry((lives_funcptr_t)(func),#func,0,rettype,args_fmt,_FILE_REF_,_LINE_REF_, \
		  (flags & ~FDEF_NO_FLAGS) | FDEF_FLAG_INSIDE););

#define ____FUNC_ENTRY____(func, rettype, ...) ___FUNC_ENTRY_FULL___(func, rettype, __VA_ARGS__, \
								     FDEF_NO_FLAGS, 0)

// macro to be placed near start of "major" functions, counterpart to ___FUNC_ENTRY___
// It will remove top entry from a thread's 'func_stack', and print out a debug line (optional)
#define ____FUNC_EXIT____ do {_func_exit(_FILE_REF_, _LINE_REF_);} while(0);

#define ____FUNC_EXIT_VAL____(rtype, val) do {weed_plant_t *pl = lives_plant_new(123); \
    weed_leaf_from_va(pl, "val", get_seedtype(rtype[0]), 1, (val));	\
    _func_exit_val(pl, _FILE_REF_, _LINE_REF_); weed_plant_free(pl);} while(0);

#else
#define ____FUNC_ENTRY____(func, rettype, args_fmt)
#define ____FUNC_EXIT____
#define ____FUNC_EXIT_VAL____(rtype, val)
#endif

// calls (void)func(args), and before or after calling it, adds a fn note with a ptr / file / line
// if ptr is already noted then will remove it
// ptr is the result of pre_expn or post_expn cast to void *
// dump_fn_notes will list all func / file / line entries
// thus e.g _FUNC_TRACE_(0, get_ptr(), myfunc, args)
// will call myfunc(args), then on return cast the return from get_ptr() to void * and add or remove a note
// - can be used to trace allocs / frees by tracing the same pointer, e.g malloc / post_expn == returned ptr
// free / pre_expn == ptr to be freed...dump_fn_notes will then list all non-freed calls to malloc by file / lineno

/* #define _FN_ALLOC_TRACE(fname) do {THREADVAR(fn_alloc_trace) = #fname;} while (0); */
/* #define _FN_FREE_TRACE(fname) do {THREADVAR(fn_free_trace) = #fname;} while (0); */

/* #define _FN_ALLOC_TRACE_END do {THREADVAR(fn_alloc_trace) = NULL;} while (0); */
/* #define _FN_FREE_TRACE_END do {THREADVAR(fn_free_trace) = NULL;} while (0); */


// append func / file / line to THREADVAR(func_stack)
#define ADD_TO_FN_STACK(fn)						\
  (!!(THREADVAR(func_stack) =						\
      lives_list_append(THREADVAR(func_stack), _FUNCREF(fn, _FILE_REF_, _LINE_REF_))))

//////////

// conditions (Work in progress)

typedef char **lives_condition;

#define MAKE_COND_OPFUNC(func) cond_fdefs[i++] = MAKE_FUNCDEF(cond_##func, VOIDPTR, "iV")
#define MAKE_COND_VALFUNC(func) cond_fdefs[i++] = MAKE_FUNCDEF(cond_##func, VOIDPTR, "S")

#define COND_PFX "COND_"
#define COND_PFXLEN 5

#define _COND_BEGIN		"COND_START"
#define _COND_FINISH		"COND_END"
#define _COND_POPEN		"COND_PARENS_OPEN"
#define _COND_PCLOSE		"COND_PARENS_CLOSE"
#define _COND_LIST_OPEN		"COND_LIST_START"
#define _COND_LIST_CLOSE	"COND_LIST_END"

#define COND_TESTFUNC_CONSTVAL(func, args_fmt) find_or_make_funcdef(#func, func, WEED_SEED_BOOLEAN, args_fmt)

void lives_conditions_init(void);

typedef struct {
  const char *sym; // symbol text
  const char *fmt; // internal fmt
  const char *subst; // text subst
  const char *desc; // descriptive text
  // value function, either allvalues_t *valfunc(const char *valstr)
  // or allvalues_t *(int nvals, allvalues_t **vals);
  lives_funcdef_t *funcdef;
  const char *constval;
} cond_trans;

#define LIVES_COND(...) (_COND_BEGIN "," #__VA_ARGS__ ","  _COND_FINISH)

lives_funcdef_t *find_or_make_fundef(const char *funcname,
                                     lives_funcptr_t func, weed_seed_t ret_type, const char *args_fmt);

lives_condition lives_cond_create(const char *condstring, ...);

lives_result_t lives_cond_eval(lives_condition);
char *lives_cond_desc(lives_condition);

///

typedef union {
  lives_proc_thread_t lpt;
  pthread_t		thread;
} action_source;

#define ACTION_SOURCE_NONE 	0
#define ACTION_SOURCE_LPT 	1
#define ACTION_SOURCE_THREAD 	2

/// HOOK FUNCTIONS ///////

#define LIVES_LEAF_HOOK_STACKS "hook_stacks"

// some flag bits operate when adding the callback, others operate when the callback is triggered
// some operate in both actions

// the following are ADD options: PRIORITY, BLOCK, NODEFER, UNIQUE_FUNC, UNIQUE_DATA, INVALIDATE_DATA
// the following are TRIGGER options: FG_THREAD, ONESHOT

// for the LIVES_GUI_HOOK, the rules are altered a little:
// FG_THREAD can affect the ADD operation, and UNIQUE_FUNC, UNIQUE_DATA, INVALIDATE_DATA
// are extended to all threads when the callback is triggered.
// NODEFER is also specifically for the GUI stack

// when adding to the LIVES_GUI_STACK, generally the callback is actioned immediately
//
// HOOK CALLBACK RECEIPTS
//
// When an attempt is made to add a funcinst as cb to a stack, a callback_receipt is created and returned
// if the funcinst cannot be added, then the receipt is returned expired the caller should simply
// l set itself as receipt adder and then free the receipt
//
// Otherwise:
// in the majority of cases, the receipt becomes the first receipt in receipts list for the provided funcinst
// which is then appended or pepended to the hook stack
//
// in some cases, the add is preempted by another funcinst - in this case the receipt is appended to the receipts
// list of the pre-empting funcinst, and the supplied funcinst with no receipts can be freed
//
// if adding was successful, the adding proc_thread must include the returned receipt
// in its cb_add_list before setting receipt adder to self
//
// Every receipt has a req_reply field. The adder may optionally provide a callback funtion which will be called when the
// the req_reply value changes. This should be set ptior to setting the receipt adder to self.
// In some cases this is done automatically, for example, blocking callbacks when added to a stack
// cause the adder to pause, and the req_reply callback will wake the thread when the funcinst is run / cancelled / errored
//
// When a hook callback funcinst is to be removed by the owner, all receipts in the receipts list are sent the req_reply INVALID,
// are flagged as EXPIRED
//
// When a proc_thread is to be freed, it will flush all receipts from its cb_add_list
//
// A receipt will automatically be freed if and only if three conditions are true:
// the receipt has an adder, the receipt is not in a proc_thread cb_add_list, and the receipt is expired
//
// when a receipt is expired by the stack owner, whether in a cb_add_list or not, it is removed from the funcinst recepits
// when a hook is triggered, any receipts that are no longer in a cb_add_list are expired, with the exceprion of PERSISTENT
// callbacks. Thus to remove a callback from a stack, the adder simply needs to remove the receipt from its cb_add_list
//
// Persistent callbacks can be force removed by removing the receipt from cb_add_list, then setting the receipt to expired,
// but ensuring it is not freed. This casuses the triggerer to ignore the persistent flag,
// and to act as if it added the expired flag itself.
//
// If a callback funcinst has no receipts because all have been removed, then the funcinst will no longer be actioned and can
// be removed from the hook stack.
//
// if a funcinst is to be removed from a stack, before expiring all receipts, the req_reply INVALID is set for all receipts
// in its list and which are also in cb_add_lists. If the adder set a callback for req_reply changes, it can remove the
// receipt from its cb_add_list when the req_reply becomes invalid.
// Alternately, a proc thread can periodically call cleanup_self_list(), which will free any expired receipts
// In any case, when the proc_thread is freed, all receipts are flushed from its cb_add_list.
//
// Finally, if a stacked funcinst is replaced (preempted), any receipts with no req_reply callbacks are expired
// and any which do have req_reply callbacks are silently migrated and appended to the receipts of the
// replacement funcinst, and the original funcinst, since it no longer has receipts will be removed from the stack
// and possibly freed
//
// the format of the req_reply callback is:
//	boolean (reply_sent_cb)(void *receipt, void *user_data);
//		- returning TRUE forces the reply to be resent until FALSE is returned
//
// In the general case, all of the underlying details are handled automatically. In app code one would simply
// add a function call as a hook cb, and receive a
// (void *)receipt back, with a NULL receipt indicating that adding was nor possible.
//
// A non-NULL receipt can later be used to remove the callback. via:
//
// lives_hook_cb_remove(receipt);
//
// however, note that if the added funcinst preempted other funcinsts and gained additional receipts.
// the callback can only be removed this way if there are no other receipts with req_reply callbacks
// (This is a safety mechanism, e.g. there could be other proc_threads which are paused waiting foe the
// callback to complete.)

typedef boolean(*reply_sent_cb_f)(void *receipt, void *user_data);

#define RCPT_IN_LIST		(1ull << 0) // is in a proc_thread cb_add_list
#define RCPT_EXPIRED		(1ull << 1) // has been disassciated from a funcinst
#define RCPT_HAS_ADDER		(1ull << 2) // is in a proc_thread cb_add_list

#define RCPT_PERSISTENT		(1ull << 16) // do not expire just because IN_LIST is absent

#define RCPT_CAN_FREE(flags) (((flags) & (RCPT_EXPIRED | RCPT_HAS_ADDER | RCPT_IN_LIST)) \
			      == (RCPT_EXPIRED | RCPT_HAS_ADDER))

#define RCPT_SHOULD_EXPIRE(flags, canxtra) (((flags) & (RCPT_EXPIRED | RCPT_PERSISTENT \
							| RCPT_HAS_ADDER | (canxtra ? 0 : RCPT_IN_LIST))) == RCPT_HAS_ADDER)

#define RCPT_SHOULD_REMOVE(flags) (((flags) & (RCPT_EXPIRED | RCPT_HAS_ADDER | RCPT_IN_LIST)) \
				   == (RCPT_EXPIRED | RCPT_HAS_ADDER | RCPT_IN_LIST))

typedef struct {
  pthread_mutex_t *status_mutex;
  volatile uint64_t status;

  lives_proc_thread_t adder;


  volatile boolean expired;
  volatile boolean in_list;
  ///
  volatile int req_reply;
  reply_sent_cb_f reply_cb;
  void *reply_cb_data;
  int nrefs;
  ///
} hook_cb_receipt;

lives_proc_thread_t lives_cb_receipt_get_adder(void *hook_cb_receipt);
int lives_cb_receipt_get_req_reply(void *hook_cb_receipt);
int lives_cb_receipt_get_nrefs(void *hook_cb_receipt);
void lives_cb_receipt_ref(void *hook_cb_receipt);
void lives_cb_receipt_unref(void *hook_cb_receipt);
boolean lives_cb_receipt_check_expired(void *hook_cb_receipt);
boolean lives_cb_receipt_is_in_list(void *hook_cb_receipt);

// add remove from self cb_added list
void lives_cb_receipt_add_to_list(void *hook_cb_receipt);
lives_result_t lives_cb_receipt_remove_from_list(void *hook_cb_receipt);
#define lives_hook_cb_remove(receipt) lives_cb_receipt_remove_from_list((receipt))

void lives_cb_receipt_set_adder(void *hook_cb_receipt, lives_proc_thread_t);
void lives_cb_receipt_set_reply(void *hook_cb_receipt, int reply);
void *lives_cb_receipt_set_expired(void *hook_cb_receipt);
void lives_cb_receipt_set_in_list(void *hook_cb_receipt, boolean in_list);
void lives_cb_receipt_set_reply_callback(void *hook_cb_receipt, reply_sent_cb_f, void *user_data);
boolean lives_cb_receipt_has_reply_callback(void *hook_cb_receipt);
boolean lives_cb_receipt_call_reply_callback(void *hook_cb_receipt);

void *lives_cb_receipt_new(void);

void remove_from_hstack(lives_hook_stack_t *, LiVESList *);

//< caller will block when adding the hook and only return when the hook callback has returned
// if the cb function is barred by another (due to uniqueness constraints),
// the the thread will block until the imposing function returns
#define HOOK_CB_BLOCK       		(1ull << 0)

// hook should be run as soon as possible when the hook trigger point is reached
#define HOOK_CB_PRIORITY		(1ull << 1) // prepend, not append

// callback will only be run at most one time, and then removed from the stack
#define HOOK_OPT_ONESHOT		(1ull << 2)

// this option can be set for a stack or for a callback
// when the callback is triggered, if the adder is paused, it will get a resume request and a notification to run the cb
// if not paused, the adder will get an interrupt signal with data notifying it to run the callback itself
// the adder will notify receipt holders by updating the req_reply rather than the triggerer doing this
//
// It is invalid to combine this with uniqueness constraints - the reason being that such a constraint can
// cause receipts to be collected, and then it may not be clear who "adder" refers to.
#define HOOK_OPT_ADDER_RUNS		(1ull << 3)

// this is intended for callbacks which have parameter values which need to be freed / unreffed even if the target func id not run
// this includes - cases where the proc_thread is cancelled while still in the queue,
//  - cases where the proc_thread is added with UNIQUE_DATA / UNIQUE_FUNC and the function data is replaced
// When when adding the callback with this flagbit set, each parameter value is followed by an unqueued proc_thread
// (which may be NULL), if non-null and the parameter data is replaced in the stack, or if the proc thread is cancelled before being run,
// the proc thread free func will be executed directly, freeing or unreffing the parameter value
// (assume this would normally be done in the target function or in a callback)
// When the proc_thread is unreffed, any free_lpts are also unreffed, whether executed or
// TODO - this will be deprecated and replaced with CONTINGENCIES
#define HOOK_CB_HAS_FREEFUNCS		(1ull << 4)

#define HOOK_CB_PERSISTENT		(1ull << 5)

// can be set to temporarily "block" a callback, in this case LIVES_REPLY_NO will be sent to receipts
#define HOOK_CB_IGNORE			(1ull << 7)

// hook is GUI related and must be run ONLY by the fg / GUI thread
#define HOOK_CB_FG_THREAD		(1ull << 8) // force fg service run

// the following corresponds directly to LIVES_THRDATTR_LIGHT
#define HOOK_OPT_FG_LIGHT		(1ull << 9)

// TODO - will be replaced with remove on condition failed, and condition set to test return value
#define HOOK_OPT_REMOVE_ON_FALSE	(1ull << 10)

// UNIQUENESS constraints:
/// the following bits define how hooks should be added to the stack
// in case of duplicate functions / data
///
// if a funcinst is expelled due to constraints, any receipts with reply_sent callbacks are silently migrated
// to the expelling funcinst. Receipts without callback functions are expired.
//
// after adding, ensure only a single copy of FUNC in the stack, with whatever data
//
// (when prepending, this always succeeds to add, and expels other copies of same func,
// when appending, the callback will leave first matching func in place, and expel all others,
// and deny the new callback. except if no match is found, will be appended)
#define HOOK_UNIQUE_FUNC		(1ull << 16) // do not add if func already in hooks
///
// after adding, ensure only a single copy of FUNC  / DATA in stack
// there may be other copies of func with non matching data
//
// NOTE: for data matching, it is sometimes desirable to match only the first n paramaeters, with the remainder
// being the data to be replaced or ignored
// in this case, THREADVAR(hook_match_nparams) cant be set to the number to match. If set to 0, the default,
// all params must be matched.
//
// (when prepending, this always succeeds to add, and expels other copies with identical func / data,
// when appending, the callback will replace the first matching func / data , and expel it and any other matches
// if no match is found, append
#define HOOK_UNIQUE_DATA		(1ull << 17)

//
// after adding, ensure only a single copy of func,  and with our data
//
///
// * copies of the func in linked_stacks must also be removed
// (when prepending, this always succeeds to add, and expels anything with same func
// when appending, the callback will replace the first matching func, and expel it and all other func matches
// or if no match is found, append
#define HOOK_UNIQUE_REPLACE		(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_DATA)

// Summary: hook_unique_func ensures there is only a single copy of func in the stack, with any data
//          hook_unique_data ensures there is at most one copy func with matching data
//          setting both flags ensures there is only one copy of func, and it will have our data
//

// NOTEs:
//
// 1) when prepending, for uniqueness purposes, the prepended callback can replace (and remove) others already in the stack
//
// 2) when appending unique_func and/or unique_data to the main GUI stack,
// the new callback can be blocked or added
// If blocked, all subsequent matching callbacks must be replaced / removed in the stack
// as if the blocking callback had just been prepended to the stack.
//
// 3) if a TRIGGERED callback has invalidate data, we should replace (or remove)
// any other callbacks in ALL stacks (fg and bg) with matching data, as if the callback were being prepended to those
// stacks.
//
// 4) before removing a callback, check if it was added with HOOK_CB_BLOCK
// if so, the proc_thread must not be unreffed as a thread will be waiting on it,
// instead set a leaf LIVES_LEAF_REPLACEMENT in the proc thread being replaced, pointing the newly added callback
// the blocked thread should detect this, unref the original blocker, and continue waiting instead for the new callback
// the replacement callback can also be replaced, so the sequence must be followed until reaching a non replaced cb,
// or one which has. The proc_thread which is the repalcement msut get an added reference, as the blocked thread will
// unref it once it completes.
//

// when prepended to a stack, it will eliminate any entries with matching data
// when appending, if we find an existing cb with this set, the callback will be rejected
// when triggered, it will eliminate matching data from ALL thread hook_stacks of indentical type
///
// NB. it is important to specify the number of matching data params, otherwise ALL params will be considered matching
// - this is designed for functions which free DATA

#define HOOK_INVALIDATE_DATA			(1ull << 18)

// this is a special modifier for INVALIDATE_DATA
// if set then data will also match "child" data (for some definition of "child")
#define HOOK_OPT_MATCH_CHILD			(1ull << 19)

// similar to unique_function (which is implicitly set),
// however, if the function is already in the stack we also remove it
// and do not add.
// combined with UNIQUE_DATA, we only toggle if data matches as well as func
#define HOOK_TOGGLE_FUNC			(1ull << 24)

///////////////////

// status bits

#define HOOK_STATUS_BLOCKED			(1ull << 32) // hook function should not be called
#define HOOK_STATUS_RUNNING			(1ull << 33) // hook cb running, do not recurse

// when triggering, mark the callbacks already actioned, and on a recheck we skip over them
#define HOOK_STATUS_ACTIONED			(1ull << 34)

// hook was 'removed' whilst running, delay removal until return
#define HOOK_STATUS_REMOVE			(1ull << 35)

// original adder can block / unblock the callback
#define HOOK_STATUS_IGNORE 			(1ull << 36)

typedef weed_plant_t lives_obj_t;
typedef boolean(*hook_funcptr_t)(lives_obj_t *, void *);
typedef char *(*make_key_f)(int pnum);

typedef struct {
  pthread_mutex_t *timer_mutex;
  uint64_t ncalls;
  uint64_t tot_time, last_time;
  int nvaries;
  uint64_t *vartimes;
} fdef_timeinfo_t;

// denotes a static funcdef - do not free during runtime
#define FDEF_FLAG_STATIC		(1ull << 0)

// denotes a line inside a function (i.e maybe far from entry point)
#define FDEF_FLAG_INSIDE		(1ull << 1)

// function can only be run by one thread at a time
#define FDEF_FLAG_SINGLE		(1ull << 2)

// function is non re-entrant (by same thread)
#define FDEF_FLAG_NO_RECURSE		(1ull << 3)

// function has timeinfo
#define FDEF_FLAG_HAS_TIMEINFO		(1ull << 16)

#define FDEF_NO_FLAGS			(1ull << 63)

// module extensions

#define module_any	       -1
#define module_none		0

#define module_lpt		1
#define module_hook_cb		2
#define module_contingency	3

#define MODULE_TYPE_LPT module_lpt
#define MODULE_DATA_TYPE_LPT proc_thrd_data

#define MODULE_TYPE_HOOK_STACK 	module_hook_cb
#define MODULE_DATA_TYPE_HOOK_STACK hook_cb_data

#define MODULE_TYPE_CONTINGENCY	module_contingency
#define MODULE_DATA_TYPE_CONTINGENCY contingency_data

#define MODULE_DATA_TYPE(n) MODULE_DATA_TYPE_##n
#define MODULE_TYPE(n) MODULE_TYPE_##n

#define MODULE_TYPE_IS(finst, type) ((finst)->mod_type == MODULE_TYPE(type))

typedef struct {
  // for DISPOSITION_WAITING / ACTIVE
  uint64_t func_attrs;
  // stack of longjump env buffers
  lives_sync_list_t *lj_stack;
  // pointer to the proc_thread which queued or stacked this
  lives_proc_thread_t dispatcher;
  // pointer to the proc_thread which is running this
  lives_proc_thread_t runner;
} MODULE_DATA_TYPE_LPT;

typedef struct {
  // for DISPOSITION_STACKED
  uint64_t cb_flags;
  lives_condition trigger_cond; // condition for triggering
  pthread_mutex_t mutex;
  lives_hook_stack_t **hstacks;
  int hstype;
  int trigger_act_src_type;
  action_source triggerer;
  lives_proc_thread_t orig_adder;
  int nmatch_params;
  volatile LiVESList *receipts;
} MODULE_DATA_TYPE_HOOK_STACK;

// definition of contingencies is still a little vague
// edtablished so far

#define CONTINGENCY_BEHAVIOUR_EXPIRE_ON_EXEC		(1ull << 0)
#define CONTINGENCY_BEHAVIOUR_NO_FREE_ON_EXPIRED	(1ull << 1)

// repeating contingencies
#define CONTINGENCY_BEHAVIOUR_READY_ON_IDLE		(1ull << 8)

typedef struct {
  // for DISPOSITION_CONTINGENCY
  // same value as for clip_srcs
  // may be
  // NOT_SET - initial value
  // READY ready for contingency activation
  // RUNNING - currently active
  // IDLE - was executed and finished
  // EXPIRED - no longer required
  // DELETED - parent object has been freed
  // ERROR

  int src_status;

  // the target is a lives_struct_t *
  lives_struct_t *target;

  lives_condition run_cond;

  // behaviour flags - funcinst is normally freed after status becomes EXPIRED or IDLE
  // but this can be overridden
  uint64_t flags;
} MODULE_DATA_TYPE_CONTINGENCY;

/* add more modules if desired */

#define LPT_DATA(finst, field)  (((MODULE_DATA_TYPE_LPT *)(finst->module))->field)
#define CL_DATA(finst, field)  (((MODULE_DATA_TYPE_HOOK_STACK *)(finst->module))->field)
#define CONTINGENCY_DATA(finst, field)  (((MODULE_DATA_TYPE_CONTINGENCY *)(finst->module))->field)

// dispositions that have a module type other than any are BASE dispositions
// if a funcinst gains a base disposition with a new module type
// the old module is pushed to the modules sync_list and popped instead of freeing the funcinst
#define module_type_for_disposition(dis)				\
  ((dis) == DISPOSITION_STACKED ? module_hook_cb			\
   : ((dis) == DISPOSITION_CONTINGENCY ? module_contingency		\
      : ((dis) == DISPOSITION_WAITING || (dis) == DISPOSITION_ACTIVE	\
	 || (dis) == DISPOSITION_READY) ? module_lpt : module_any))

#define CALLOC_MODULE(j) \
  (j == module_lpt ? (void *)LIVES_CALLOC_SIZEOF(MODULE_DATA_TYPE_LPT, 1) \
   : j == module_hook_cb ? (void *)LIVES_CALLOC_SIZEOF(MODULE_DATA_TYPE_HOOK_STACK, 1) \
   : j == module_contingency ? (void *)LIVES_CALLOC_SIZEOF(MODULE_DATA_TYPE_CONTINGENCY, 1) : NULL)

// funcinst is static, do not free it
#define FINST_FLAG_STATIC	  	(1ull << 0)

// retloc points to user defined variable so do not free it when freeing funcinst
#define FINST_FLAG_NOFREE_RETLOC  	(1ull << 1)

//  error flagbits - set on adding

// funcinst was blocked from being added because it failed to pass the 'accept_conditions'
#define FINST_FLAG_NOT_ACCEPTED	  	(1ull << 32)

// funcinst was blocked from being added because it passed the 'reject_conditions'
#define FINST_FLAG_REJECTED	  	(1ull << 33)

// funcinst was blocked from being added because it passed the 'reject_conditions'
#define FINST_FLAG_NOPERM	  	(1ull << 34)

// when adding a hook callback, there are several methods
// use a registered funcname, in this case the funcdef_t is looked up from funcname, and
// we create a funcinst using funcdef as a template, and including the function va_args
//
// add a callback defining the funcdef and funcinst in one go
//
// alternately, we can create a lpt from function args
// finally, we can create a funcinst directly, then include this
//

#define BUSY_HOOK		TX_BUSY_HOOK
#define UNBUSY_HOOK		TX_UNBUSY_HOOK

#define ACCEL_START_HOOK	SEGMENT_START_HOOK
#define ACCEL_END_HOOK		SEGMENT_END_HOOK

#define LIVES_GUI_HOOK		INTERNAL_HOOK_0
#define LIVES_PRE_HOOK		INTERNAL_HOOK_1
#define LIVES_POST_HOOK		INTERNAL_HOOK_2

// TODO:
// for data_hooks, there are really only 2 types - pre and post
// then depending on a conditions check, this will cascade to another hook value
// the item name must match, eg "attrs"
// the pre_cond defines some conditione the pre change value must have
// the post_cond defines some condition the post value must have
// the default params for the data pattern are owner, item, pre_val, post_val
// for add: owner, item, post_val
// for del: owner, item, ore_val

#define _TRIGGER_ON_PRE		1
#define _TRIGGER_ON_ADD		2
#define _TRIGGER_ON_DEL 	4

#define POST_TRIGGER		0
#define PRE_TRIGGER		_TRIGGER_ON_PRE
#define ADDED_TRIGGER		_TRIGGER_ON_ADD
#define DELETING_TRIGGER	(_TRIGGER_ON_DEL | _TRIGGER_ON_PRE)

typedef struct {
  // name of leaf (attribute) in stack owner
  const char *target_item;
  int when;
  // ignored for add
  lives_condition pre_cond;
  // ignored for del
  lives_condition post_cond;
} hook_cascade;

// HOOK STACK_DESCRIPTORS - each hook stack type has an assosciated hook_descriptor
// which defines wken the stack may be triggered and how callbacks are handled on trigged

// data hooks are triggered before or after some item of data is added, altered or removed
//
// spontaneous hooks are triggered on demand, usually at fixed function points
// or in response to some event - normally by the owner of the stack, unless anon_trigger is flagged
//
// request hooks allow the threads to make requests to other threads
// the "callback" differs in that it has no target function
// there are 2 types of request stacks, realtime and non realtime
// if a proc_thread supports realtime requests, it will unblock interrupt singnals
// add a callback to its own "callback_added" hook
// when this is triggered, if the target stack is a realtime request, the adder will send an interrupt
// to the owner, and the owner will reply "quickly"

typedef enum {
  HOOK_PATTERN_INVALID = -1,
  HOOK_PATTERN_DATA,
  HOOK_PATTERN_REQUEST,
  HOOK_PATTERN_SPONTANEOUS
} hook_stack_pattern_t;

typedef struct {
  int htype; // the hook type (e.g. COMPLETED, PREPARING)

  hook_stack_pattern_t pattern; // base pattern data, spontaneous, request

  hook_cascade cascade;

  uint64_t op_flags; // flags defining trigger operation

  // conditions for accepting a func_inst in the stack
  lives_condition accept_cond;

  // cb_prototype parameters. Empty string means no parameters
  // if the final character is '*', callbacks may append bound parameters in
  // funcinst->params
  // NULL means any params (equivalent to "*")
  const char *def_args_fmt;

  // mapping of def_args_fmt_params to rcpt values
  const char **bindings;
} hook_stack_descriptor_t;

const hook_stack_descriptor_t *get_hs_desc(int hstype);
const char *get_def_args_fmt(lives_hook_stack_t **, int hstype);
uint64_t get_hs_op_flags(int hstype);

#define HS_FLAG_TRIGGERING	(1ull << 0)
#define HS_FLAG_INVALID		(1ull << 1)

typedef struct _hstack_t {
  int type;
  lives_hook_stack_t **parent_stacks;

  const hook_stack_descriptor_t *hsdesc;

  // optional condition for rejecting from  stack (NULL == COND_FALSE)
  lives_condition reject_cond;

  volatile LiVESList *stack;
  pthread_mutex_t mutex;

  // for native stacks, the action_source owner will be thread
  // for other standard stacks, the owner will be will be lives_proc_thread
  int owner_act_src_type;
  action_source owner;

  uint64_t flags;

  // for hook stacks with pattern request,
  // it is possible to create "triage" stacks
  // when such stacks are triggered, the callbacks are not actioned
  // immediately, instead they are added to another request stack
  // (prepended if dlagged high prio, otherwise appended)
  // in addition, the callback flags may be mutated when transferring
  // some callbacks may be rejected bt the target, in which case a YES reply will change to NO
  lives_hook_stack_t **req_target_stacks;
  int req_target_type;
  uint64_t req_target_set_flags;
  uint64_t req_target_unset_flags;
} lives_hook_stack_t;

// hook_stack_flags

#define HS_MASK_GUI		0xFFFFFFFFFFFF00FF
#define HS_MASK_WORKER		0xFFFFFFFF0000FFFF

#define HOOKSTACK_FLAGS_ADJUST(flags)  _DW0(flags &= THREADVAR(hs_flag_mask);)

#define HOOKSTACK_NATIVE	       	(1ull << 0)

// denotes that callbacks in the stack are run once only and removed
#define HOOKSTACK_ALWAYS_ONESHOT       	(1ull << 1)

// it is invalid to set both async and uniqueness constraints
// the logic here is that the adder

// hook callbacks should be run asynchronously. May be combined with ADDER_RUNS
#define HOOKSTACK_ASYNC		       	(1ull << 2)

// modifies async behaviour
#define HOOKSTACK_PARALLEL	       	(1ull << 3)

// only the hook stack owner may add callbacks for this hook
#define HOOKSTACK_SELF_ONLY       	(1ull << 4)

// if this flagbit is set, then the return type of callbacks must be boolean
// any callback which returns FALSE will be blocked / ignored until either unblocked or invalidated
#define HOOKSTACK_REMOVE_ON_FALSE	(1ull << 5)

// if the trigger condition (if present) fails, then the callback will be removed rather than skipped over
#define HOOKSTACK_REMOVE_ON_COND_FAIL	(1ull << 6)

// MASKED OUT FOR GUI THREAD

#define HOOKSTACK_GUI_THREAD		(1ull << 8)

// MASKED OUT FOR WORKER THREADS

// this flagbit denotes that triggering the hooks will run only the first callback on the stack and return
// this only makes sense when combined with always_oneshot
#define HOOKSTACK_RUN_SINGLE      	(1ull << 16)

// callback receipts are usually expired when they have been removed from the addet;s cb_add_list
// setting this prevents that, allowing callbacks to persist beyond the lifetime of the addery
#define HOOKSTACK_PERSISTENT	       	(1ull << 17)

// if callbacks cannot be run immediately (because some other thread holds the mutex lock)
// return immediately and do not run the callbacks
#define HOOKSTACK_NOWAIT	       	(1ull << 18)

// see HOOK_OPT_ADDER_RUNS
#define HOOKSTACK_ADDER_RUNS	       	(1ull << 32)

// usually hook stack triggering can only be effected by the stack owner
// if this flag is set, then any thread may trigger the stack (and probably run the callbacks itself,
// unless flags dictate otherwise)
#define HOOKSTACK_ANON_TRIGGER	       	(1ull << 33)

#define HS_FLAGS_CB_ADDED		(HOOKSTACK_SELF_ONLY)

#define HS_FLAGS_FATAL			(HOOKSTACK_ALWAYS_ONESHOT | HOOKSTACK_NATIVE | HOOKSTACK_PERSISTENT)
#define HS_FLAGS_THREAD_EXIT		(HOOKSTACK_ALWAYS_ONESHOT | HOOKSTACK_NATIVE)

#define HS_FLAGS_DATA_READY		(HOOKSTACK_ASYNC | HOOKSTACK_PARALLEL | HOOKSTACK_REMOVE_ON_FALSE)
#define HS_RET_TYPE_DATA_READY		WEED_SEED_BOOLEAN

#define HS_FLAGS_LIVES_GUI		(HOOKSTACK_RUN_SINGLE | HOOKSTACK_GUI_THREAD | HOOKSTACK_ALWAYS_ONESHOT \
 							| HOOKSTACK_NOWAIT)
#define HS_FLAGS_SYNC_ANNOUNCE		(HOOKSTACK_PERSISTENT | HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_COMPLETED		(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_FINISHED		(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_CANCELLED		(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_ERROR			(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_DESTRUCTION		(HOOKSTACK_ALWAYS_ONESHOT)

#define HS_DETAILS(type) HS_DETAILS_##type##_HOOK

#define COND_RET_BOOLEAN "COND_EQUALS, COND_INT_VAR, COND_SYM_SRC_ITEM/funcdef/return_type,"\
  "COND_INT_CONST, %ld", WEED_SEED_BOOLEAN

#define HS_DETAILS_COMPLETED_HOOK ((hook_stack_descriptor_t)		\
  {.htype = COMPLETED_HOOK, .pattern = HOOK_PATTERN_DATA,		\
     .cascade = {							\
		 .target_item = LIVES_LEAF_THRD_STATE,			\
		 .when = LIVES_POST_HOOK,				\
		 .pre_cond = lives_cond_create("COND_NOT, COND_BIT_SET, COND_SYM_OLD_VALUE, "\
					       "COND_UINT64_CONST, %lu", THRD_STATE_COMPLETED),	\
		 .post_cond = lives_cond_create("COND_BIT_SET, COND_SYM_NEW_VALUE, " \
						"COND_UINT64_CONST, %lu", THRD_STATE_COMPLETED) \
		 },							\
     .op_flags = HOOKSTACK_ALWAYS_ONESHOT,				\
     .accept_cond = lives_cond_create(COND_RET_BOOLEAN),		\
     .def_args_fmt = "v*",						\
     .bindings = (const char *[]){"target_object"}})

// low level flags used internally when adding callbacks*/

#define _ADDMODE_NORCPT 	(1ull << 0) // do not create a request receipt; if not stacked, create a temporary module
#define _ADDMODE_HAVE_LOCK 	(1ull << 1) // mutex locked; already has a hookstack module
#define _ADDMODE_NOADD	 	(1ull << 2) // do not add the funcinst
#define _ADDMODE_NOREMOVE 	(1ull << 3) // do not remove anything
#define _ADDMODE_FORCE_PREPEND	(1ull << 4) // act as if prepending

#define ADDMODE_NORMAL		0

// test if we can add, but do not add or remove anything (dry run)
// test for finst->flags & FINST_FLAG_REJECTED afterwards
#define ADDMODE_TEST 		(_ADDMODE_NOADD | _ADDMODE_NOREMOVE | _ADDMODE_NORCPT)

// insert the callback, checking for remove / block but do not create a new receipt
// caller should check funcinst FINST_FLAG_REJECTED
#define ADDMODE_TRANSFER	(_ADDMODE_NORCPT | _ADDMODE_HAVE_LOCK)

// remove conflicting callbacks, do not add
#define ADDMODE_UPD_LINKED	(ADDMODE_TRANSFER | _ADDMODE_NOADD | _ADDMODE_FORCE_PREPEND)

// initial value, no reply received yet
#define LIVES_REPLY_INVALID	NIRVA_REPLY_INVALID

// reuest denied (possibly due to uniqueness conditions,
// or invalid request oaraneters)
#define LIVES_REPLY_NO 		NIRVA_REPLY_NO

// request accepted and will be actioned (provisional)
// note: can become NO later if another request replaces it
#define LIVES_REPLY_YES		NIRVA_REPLY_YES

// request completed succesfully
#define LIVES_REPLY_FULFILLED	NIRVA_REPLY_FULFILLED

// request ran but encountered an error
#define LIVES_REPLY_ERROR	NIRVA_REPLY_ERROR

// request ran but was cancelled
#define LIVES_REPLY_CANCELLED 	NIRVA_REPLY_CANCELLED

#define LIVES_REPLY_MU 	NIRVA_REPLY_MU

// normal funcs, but not called directly
void *_lives_hook_cb_add_full(lives_hook_stack_t **, int type, uint64_t cbflags, lives_funcptr_t func,
                              const char *fname, int return_type, const char **anames, const char *args_fmt, ...);
#define lives_hook_cb_add_full(hs, type, cbflags, func, fname, rtype, afmt, ...) \
  _lives_hook_cb_add_full((hs), (type), (cbflags), func, fname, (rtype), VARNAMES(__VA_ARGS__), (afmt), __VA_ARGS__)

// func call with variant addmode
void *lives_hook_cb_add(lives_hook_stack_t **hooks, int type, lives_funcinst_t *finst, uint64_t cbflags, uint64_t addmode);

//fixed cb type: boolean cb(void *owner, void *data)
// hstacks == NULL -> self_hook_stacks(hstype)
#define lives_hook_cb_append(hstacks, hstype, cbflags, func, data,...)	\
  _lives_hook_cb_add_full((hstacks), (hstype), (cbflags), (lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
			  VARNAMES(__VA_ARGS__), "vv", NULL, (void *)(data))

// same, but get hstacks for lpt
#define lives_proc_thread_add_hook_cb(lpt, hstype, cbflags, func, data,...)	\
  _lives_hook_cb_add_full(lives_proc_thread_get_hook_stacks(lpt), (hstype), (cbflags), \
			  (lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
			  VARNAMES(__VA_ARGS__), "vv", NULL, (void *)(data))

// same but caller defines rtype and args
#define lives_hook_cb_append_full(hstacks, hstype, cbflags, func, rtype, args_fmt, ...) \
  _lives_hook_cb_add_full((hstacks), (hstype), (cbflags), (lives_funcptr_t)(func), #func, (rtype), \
			  VARNAMES(__VA_ARGS__), (args_fmt), __VA_ARGS__)

#define lives_proc_thread_add_hook_cb_full(lpt, hstype, cbflags, func, rtype, args_fmt, ...) \
  _lives_hook_cb_add_full(lives_proc_thread_get_hook_stacks(lpt), (hstype), (cbflags), \
			  (lives_funcptr_t)(func), #func, (rtype),	\
			  VARNAMES(__VA_ARGS__), (args_fmt), __VA_ARGS__)

// for those times when you already have a funcinst (no paramdata freefuncs allowed though)
void *lives_hook_cb_add_funcinst(lives_hook_stack_t **hstacks, int type,
                                 lives_funcinst_t *finst, uint64_t cbflags);

////////////////////////////

// invalidate a callback, so that the next time it is triggered it will be removed and not run
// this removes from add_cb_list AND sets expired so we can force remove even persistent cbs
lives_result_t lives_hook_cb_invalidate(void *rcpt);

// blocks a callback, so that it will be ignored instead of running
// voluntary blocking  is only allowed for the callback adder
lives_result_t lives_hook_cb_block(lives_funcinst_t *);
lives_result_t lives_hook_cb_unblock(lives_funcinst_t *);

void flush_cb_added_list(lives_proc_thread_t lpt, boolean all);
void ref_cb_added_list(void);

void unref_cb_added_list(void);

void lives_hook_stack_clear(lives_hook_stack_t **, int hstype);
void lives_hook_stacks_clear_all(lives_hook_stack_t **, int ntypes);

lives_result_t lives_hook_trigger(lives_hook_stack_t **, int hstype);

lives_result_t lives_proc_thread_trigger_hook(int hstype);

int lives_hook_trigger_async(int hstype, lives_proc_thread_t **);

// start a bg thread which triggers the hook, running each callback in sequence
// if there are no callbacks in the stack, the proc thread completes
// a common use would be to configure the stack to reject any funcinst without a boolean return type
// optionally set the stack to run_single
// then configure each thread with a condition 'COND_EQUALS, COND_SYM_RETVAL, COND_FALSE", and set remove on condfail
//
lives_proc_thread_t lives_hook_trigger_async_sequential(int hstype, uint64_t delay);


void lives_hook_async_join(int hstype);
void lives_hook_async_cancel(int hstype);

lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t);
lives_hook_stack_t **self_hook_stacks(int hstype);

///////////// funcdefs, funcinsts and funcsigs /////
lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
                                int return_type, const char *args_fmt, const char *file, int line, uint64_t flags);

#define create_funcdef_here(func) create_funcdef(#func, (lives_funcptr_t)func, 0, NULL,	\
						 _FILE_REF_, _LINE_REF_, FDEF_FLAG_INSIDE)

#define MAKE_FUNCDEF(func, rt, args) create_funcdef(#func, func, WEED_SEED_##rt, args, NULL, 0, 0);

void lives_funcdef_free(lives_funcdef_t *);

// funcinst is the variable part of a function call

lives_funcinst_t *lives_funcinst_new(lives_funcdef_t *);

lives_result_t funcinst_params_from_vargs(lives_funcinst_t *,  const char *args_fmt, va_list xargs);

void lives_funcinst_free(lives_funcinst_t *);

void cleanup_self_receipts(void);

// proxy values

#define WEED_BIND_VALUE(pl, key, st, var) weed_leaf_bind_value(pl, key, st, &var, sizeof(var))

void *lives_proxy_data_get_value(weed_plant_t *pl, const char *key, int idx, void **retlocp,
                                 boolean copy, weed_error_t *errp);

lives_result_t lives_funcinst_bind_param(lives_funcinst_t *finst, int idx, void *locn, weed_size_t size);

weed_error_t update_param_from_proxy(weed_plant_t *, const char *key, int idx, const char *real_key);

weed_error_t copy_leaf_value(weed_plant_t *, const char *key, int idx, weed_seed_t xst, void **retlocp);

weed_size_t lives_proxy_data_get_size(weed_plant_t *, const char *key, int idx, weed_error_t *errp);

lives_result_t weed_plant_params_from_args_fmt(weed_plant_t *plant, const char *args_fmt, ...);

funcsig_t funcsig_from_args_fmt(const char *args_fmt);
char *args_fmt_from_funcsig(funcsig_t funcsig);

void lives_funcinst_send_replies(lives_funcinst_t *finst, int reply);

//
#if 0
// todo:
// bound params - takes a param number (pnum) and target (eg. "funcinst/retloc")
// when the funcinst params are created a placeholder value is added
// when the funcinst is executed, unbound placeholder values will be located via structdef magic
// the seedtype will be set and the values will then be updated prior to each funcinst call
// - these are valid for funcdefs, and if the funcdef allows additional params, the funcinst
// may append further bound values
// -- the args_fmt in funcinst->params will not include bound values,
// however, the funcsig will be "edited" befor the func call, and the bound value types shall be inserted


// can be used in funcsigs, obj attributes, fields in a lives_struct_t
// the allvalue can be used to bootstrap plant blueprints
//
// value plant can be updated from allvalue and vice-versa (bound params)

typedef struct {
  int pnum;

  uint64_t flags;

  lives_hook_stack_t *data_hook;

  weed_plant_t *value;
  // value "value" is a CUSTOM (WEED_SEED_PROXY)
  // value that points to &(allv.value)
  // from there we read stype, flags, ne

  // equivalent of value plant, but as an array
  // allv.value.X can point to ext value(s)
  allvalues_t *allv;

  // VALUE plant can be passed to anything that needs a weed_plant_t * as input
  // allv can be passed to any function which needs an array as input
  // allv.value.V for example can be a void * param for a function / funcinst
} funcinst_param_t;

void lives_funcdef_include_bound_value(lives_funcdef_t *, int pnum, const char *target);
void lives_funcinst_include_bound_value(lives_funcisnt_t *, int pnum, const char *target);
#endif

allvalues_t *_make_allval(allvalues_t *, weed_seed_t stype, weed_size_t ne, int flags, const char *valname, ...);

#define MAKE_ALLVALUE(stype, val) (_make_allval(NULL, stype, 1, 0, #val, (val)))
#define MAKE_ALLVALUE_BOUND(stype, variable) (_make_allval(NULL, stype, 1, PARAM_FLAG_BOUND, #variable, (void *)&(variable)))
#define MAKE_ALLVALUE_ARRAY(stype, ne, vals) (_make_allval(NULL, stype, ne, 0, #vals, (vals)))

#define SET_ALLVALUE(avp, stype, val) (_make_allval(avp, stype, 1, 0, #val, (val)))
#define SET_ALLVALUE_BOUND(avp, stype, variable) (_make_allval(avp, stype, 1, PARAM_FLAG_BOUND, #variable, (void *)&(variable)))
#define SET_ALLVALUE_ARRAY(avp, stype, ne, vals) (_make_allval(avp, stype, ne, 0, #vals, (vals)))
#define set_allvp_value(allvp, value) _DW0(SET_ALLVALUE(allvp, get_seedtype(type_name((value))), value);)

void allvalue_free(allvalues_t *);

lives_funcinst_t *_funcinst_from_allvals(lives_funcdef_t *fdef, lives_funcptr_t func,
    const char *funcname, weed_seed_t ret_type,
    int nvals, allvalues_t **pvals);

#define finst_from_allvals(func, rtype, nvals, pvals) _funcinst_from_allvals(NULL, func, #func, rtype, nvals, pvals)
#define finst_from_fdef_allvals(fdef, nvals, pvals) _funcinst_from_allvals(fdef, NULL, NULL, 0, nvals, pvals)

allvalues_t *allvalues_from_leaf(allvalues_t *avp, weed_plant_t *plant, const char *key);

const char get_typeletter(uint8_t val);
uint8_t get_typecode(char c);
const char get_char_for_st(weed_seed_t);
uint8_t get_typecode_for_st(weed_seed_t);
const char *get_fmtstr_for_st(weed_seed_t);

char *funcsig_to_string(funcsig_t);
char *funcsig_to_symstring(funcsig_t);
char *funcsig_to_param_string(funcsig_t);
char *funcsig_to_short_param_string(funcsig_t);

char *args_fmt_to_param_string(const char *args_fmt);

weed_seed_t get_seedtype(char c);

int fn_func_match(lives_funcinst_t *finst1, lives_funcinst_t *finst2);
boolean fn_data_match(lives_funcinst_t *finst1, lives_funcinst_t *finst2, int maxp);

int get_funcsig_nparms(funcsig_t sig);

funcsig_t short_params_to_funcsig(int nvals, const char **symnames);

uint8_t symname_to_sigbits(const char *symname);

const lives_funcdef_t *get_template_for_func(lives_funcptr_t func);
char *get_argstring_for_func(lives_funcptr_t func);

char *finst_paramstr(lives_proc_thread_t, funcsig_t sig);

#endif
