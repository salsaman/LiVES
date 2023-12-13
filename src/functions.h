// functions.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H

#include "funcsigs.h"

///// low level operations //////

#define LIST_TYPE LiVESList *

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

/* recursion_token *get_rec_token(vpod){return &trectoks;} */
/* recursion_token *rtp = get_rec_token(); */
/* recursion_token rtp = get_rec_token(); */

typedef uint64_t lives_thread_attr_t;
typedef LiVESList lives_thread_t;

typedef struct {
  char letter;
  uint32_t seed_btype;
  uint8_t sigbits;
  const char *symname;
  const char *fmtstr;
} lookup_tab;

void lpt_params_free(lives_proc_thread_t, boolean do_exec);

extern const lookup_tab crossrefs[];

#ifdef WEED_SEED_UINT
#define XREFS_TAB_UINT {'u',  WEED_SEED_UINT, 		0x06, 	"UINT", "%u"},
#else
#define XREFS_TAB_UINT
#endif
#ifdef WEED_SEED_UINT64
#define XREFS_TAB_UINT64  {'U',  WEED_SEED_UINT64,       	0x07, 	"UINT64", "%"PRIu64},
#else
#define XREFS_TAB_UINT64
#endif
#ifdef WEED_SEED_FLOAT
#define XREFS_TAB_FLOAT  {'f',  WEED_SEED_FLOAT,        	0x08, 	"FLOAT", "%.4f"},
#else
#define XREFS_TAB_FLOAT
#endif

#define XREFS_TAB						\
  {{'i',  WEED_SEED_INT,       	0x01, 	"INT", "%d"},	\
  {'d',  WEED_SEED_DOUBLE, 	0x02, 	"DOUBLE", "%.4f"},	\
  {'b',  WEED_SEED_BOOLEAN, 	0x03, 	"BOOL", "%d"},		\
  {'s',  WEED_SEED_STRING, 	0x04, 	"STRING", "\"%s\""},		\
  {'I',  WEED_SEED_INT64,      	0x05, 	"INT64", "%"PRIi64},	\
  {'F',  WEED_SEED_FUNCPTR, 	0x0C, 	"FUNCP", "%p"},	\
  {'v',  WEED_SEED_VOIDPTR, 	0x0D, 	"VOIDP", "%p"},	\
  {'V',  WEED_SEED_VOIDPTR, 	0x0D, 	"VOIDP", "%p"},\
  {'p',  WEED_SEED_PLANTPTR, 	0x0E, 	"PLANTP", "%p"},\
  {'P',  WEED_SEED_PLANTPTR, 	0x0E, 	"PLANTP", "%p"},\
    XREFS_TAB_UINT					\
    XREFS_TAB_UINT64					\
    XREFS_TAB_FLOAT					\
  {'\0', WEED_SEED_VOID,       	0, 	"", ""} \
}

#define LIVES_LEAF_RETURN_VALUE "return_value"
#define LIVES_LEAF_RETLOC "retloc"

#define _RV_ LIVES_LEAF_RETURN_VALUE

#define PROC_THREAD_PARAM(n) LIVES_LEAF_THREAD_PARAM  #n

#define LIVES_LEAF_LONGJMP "_longjmp_env_ptr"

weed_error_t weed_leaf_from_varg(weed_plant_t *, const char *key, uint32_t type, weed_size_t ne, va_list xargs);
lives_result_t weed_leaf_from_va(weed_plant_t *, const char *key, char fmtchar, ...);
boolean call_funcsig(lives_proc_thread_t);
lives_result_t do_call(lives_proc_thread_t);

#define LIVES_LEAF_TEMPLATE "_template"
#define LIVES_LEAF_FUNCSIG "_funcsig"
#define LIVES_LEAF_FUNC_NAME "_funcname"
#define LIVES_LEAF_FUNCDEF "_funcdef"
#define LIVES_LEAF_FUNCINST "_funcinst"
#define LIVES_LEAF_REPLACEMENT "_replacement"

#define LIVES_PLANT_FUNCINST 150

#define LIVES_LEAF_CLOSURE "_closure"

#define LIVES_LEAF_XLIST_STACKS "xlist_stacks"
#define LIVES_LEAF_XLIST_TYPE "xlist_type"
#define LIVES_LEAF_XLIST_LIST "xlist_list"

#define FUNC_CATEGORY_HOOK_COMMON 	(N_STD_FUNC_CATEGORIES + 100)
#define FUNC_CATEGORY_HOOK_SYNC 	(N_STD_FUNC_CATEGORIES + 101)
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
#else
#define _FILE_REF_ ""
#endif
#ifdef __LINE__
#define _LINE_REF_ __LINE__
#else
#define _LINE_REF_ 0
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

// add a note to to ftrace_store, va_args depend on fn_type
void *add_fn_note(fn_type_t, ...);

// dump notes (traces) from ftrace_store
void dump_fn_notes(void);

#define FN_DEBUG_OUT(func) g_print("Thread 0x%lx in func %s at %s, line %d\n", \
				   THREADVAR(uid), #func, _FILE_REF_, _LINE_REF_)

#define FN_DEBUG_EXIT_OUT g_print("Thread %ld exiting func at %s, line %d\n", \
				  THREADVAR(uid), _FILE_REF_, _LINE_REF_)

void _func_entry(lives_funcptr_t, const char *funcname, int category, const char *rettype,
                 const char *args_fmt, char *file_ref, int line_ref);
void _func_exit(char *file_ref, int line_ref);

void _func_exit_val(weed_plant_t *, char *file_ref, int line_ref);

#ifndef NO_FUNC_TAGS
// macro to be placeyd near start of "major" functions. It will prepend funcname to
// a thread's 'func_stack', print out a debug line (optional), and also add fn lookup
// to fn_store, e.g:   ____FUNC_ENTRY____(transcode_clip, "b", "iibs");
#define ____FUNC_ENTRY____(func, rettype, args_fmt)			\
  do {_func_entry((lives_funcptr_t)(func),#func,0,rettype,args_fmt,_FILE_REF_,_LINE_REF_);}while (0);

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

//< caller will block when adding the hook and only return when the hook callback has returned
// if the cb function is barred by another (due to uniqueness constraints),
// the the thread will block until the imposing function returns
#define HOOK_CB_BLOCK       		(1ull << 0)

// hook should be run as soon as possible when the hook trigger point is reached
#define HOOK_CB_PRIORITY		(1ull << 1) // prepend, not append

// callback will only be run at most one time, and then removed from the stack
#define HOOK_OPT_ONESHOT		(1ull << 2)

// ignored if retrun value is not boolean. If FALSE is returned, remove rom stack
#define HOOK_OPT_REMOVE_ON_FALSE	(1ull << 3)

// this is intended for callbacks which have parameter values which need to be freed / unreffed even if the target func id not run
// this includes - cases where the proc_thread is cancelled while still in the queue,
//  - cases where the proc_thread is added with UNIQUE_DATA / UNIQUE_FUNC and the function data is replaced
// When when adding the callback with this flagbit set, each parameter value is followed by an unqueued proc_thread
// (which may be NULL), if non-null and the parameter data is replaced in the stack, or if the proc thread is cancelled before being run,
// the proc thread free func will be executed directly, freeing or unreffing the parameter value
// (assume this would normally be done in the target function or in a callback)
// When the proc_thread is unreffed, any free_lpts are also unreffed, whether executed or
#define HOOK_CB_HAS_FREEFUNCS		(1ull << 4)

#define HOOK_CB_PERSISTENT		(1ull << 5)

#define HOOK_CB_IGNORE			(1ull << 6)

// hook is GUI related and must be run ONLY by the fg / GUI thread
#define HOOK_CB_FG_THREAD		(1ull << 8) // force fg service run

// the following corresponds directly to LIVES_THRDATTR_LIGHT
#define HOOK_OPT_FG_LIGHT		(1ull << 9)

/// the following bits define how hooks should be added to the stack
// in case of duplicate functions / data
// after adding, ensure only a single copy of FUNC in the stack, with whatever data
// (when prepending, this always succeeds to add, and expels other copies of same func,
// when appending, the callback will be blocked or added)
#define HOOK_UNIQUE_FUNC		(1ull << 16) // do not add if func already in hooks

// after adding, ensure only a single copy of FUNC  / DATA in stack
// may be other copies of func with other data

// NOTE: for data matching, it is sometimes desirable to match only the first n paramaeters, with the remainder
// being the data to be replaced
// in this case,   THREADVAR(hook_match_nparams) can be set to the number to match. If set to 0, the default,
// all params must be matched. Thus HOOK_UNIQUE_REPLACE only makes sense with a non zero value
// else all parameters would be matched, and there would be no "data" to be replaced
// (when prepending, this always succeeds to add, and expels other copies with identical func / data,
// when appending, the callback will be blocked or added)
#define HOOK_UNIQUE_DATA		(1ull << 17)

// change data of first func of same type, with n_match_params equal,  but leave func inplace,
// (after adding, there will be only one copy of FUNC, with our data)
// * copies of the func in linked_stacks must also be removed
// (when prepending, this always succeeds to add, and acts identically to unique_func,
// when appending, the callback will replace data and be rejected, or if no match is found, appended)
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
// when triggered, it will eliminate matching data from all thread hook_stacks
// NB. it is important to specify the number of matching data params, otherwise ALL params will be matched
// - this is designed for functions which free DATA

#define HOOK_INVALIDATE_DATA			(1ull << 18)

// this is a special modifier for INVALIDATE_DATA
// if set then data will also match "child" data (for some deifnition of "child")
#define HOOK_OPT_MATCH_CHILD			(1ull << 19)

// similar to unique_function, however, if the function is already in the stack we also remove it
// and do not add.
#define HOOK_TOGGLE_FUNC			(1ull << 24)

///////////////////

#define HOOK_STATUS_BLOCKED			(1ull << 32) // hook function should not be called
#define HOOK_STATUS_RUNNING			(1ull << 33) // hook cb running, do not recurse

// when triggering, mark the callbacks already actioned, and on a recheck we skip over them
#define HOOK_STATUS_ACTIONED			(1ull << 34)

// hook was 'removed' whilst running, delay removal until return
#define HOOK_STATUS_REMOVE			(1ull << 35)

// values higher than this are reserved for closure flags
#define HOOK_CB_FLAG_MAX			(1ull << 35)

typedef weed_plant_t lives_obj_t;
typedef boolean(*hook_funcptr_t)(lives_obj_t *, void *);
typedef char *(*make_key_f)(int pnum);

typedef struct {
  uint64_t uid;
  uint64_t flags; // flags can include static (do not free)
  int category; // category type for function (0 for general)
  const char *funcname; // optional
  //
  lives_funcptr_t function;
  int return_type;
  // these are equivalent, but we add both for convenience
  const char *args_fmt;
  funcsig_t funcsig; //
  // locator
  const char *file;
  int line;
  //
  void *data; // optional data, may be NULL
} lives_funcdef_t;

// denotes a static funcdef - do not free during runtime
#define FDEF_FLAG_STATIC		(1ull << 0)

// denotes a line inside a function (i.e maybe far from entry point)
#define FDEF_FLAG_INSIDE		(1ull << 1)

typedef weed_plant_t lives_funcinst_t;

typedef struct {
  uint64_t idx;
  lives_funcdef_t *funcdef;

  // description of each function param, funcsig order
  // for variadic functions, we may have optional |param_goup, min_repeats, max_rrepeats|
  lives_param_t **func_params;

  // mapped from return val from func
  lives_param_t *out_param;

  // optionally, we can describe the data to be passed in returned in the data book
  // (if function is run via a proc_thread)
  lives_param_t **book_data;
} lives_full_funcdef_t;

// when adding a hook callback, there are several methods
// use a registered funcname, in this case the funcdef_t is looed up from funcname, and
// we create a lpt using funcdef as a template, and including the function va_args
//
// alternately, we can create a lpt from function args
// finally, we can create an unqueued lpt directly, then include this
//
//

typedef struct _hstack_t lives_hook_stack_t;

typedef struct {
  // which hook stacks it is added to, and which hook_type
  lives_hook_stack_t **hook_stacks;
  int hook_type;
  pthread_mutex_t mutex;
  // proc_thread intially in unqueued state - either supplied directly
  // or created from param args / hook_type
  // adder is used so we can trace back to lpt which added the callback, and if freed,
  // remove it from other proc_thread's hook_cb_list
  // holder is the proc_thread owning the stacks
  lives_proc_thread_t proc_thread, adder;
  // func def describing hook cb, this can be created from the lpt
  // or it can be a pointer to a static funcdef
  // once registered, then we can simply add hooks using funcname, params
  // it also had a field for category
  const lives_funcdef_t *fdef;
  volatile uint64_t flags; // HOOK_CB flags
  // how many params must be identical to count as a data match ?
  // default is 0, all params
  int nmatch_params;
  void *retloc; // pointer to a var to store return val in
} lives_closure_t;

/* lives_closure_t *lives_hook_closure_new(lives_funcptr_t func, const char *fname, uint64_t flags, */
/* 					int hook_type, void *data); */

lives_closure_t *lives_hook_closure_new_for_lpt(lives_proc_thread_t lpt, uint64_t flags, int hook_type);

void lives_closure_free(lives_closure_t *closure);

#define LIVES_GUI_HOOK		INTERNAL_HOOK_0
#define LIVES_PRE_HOOK		INTERNAL_HOOK_1
#define LIVES_POST_HOOK		INTERNAL_HOOK_2

typedef struct _hstack_t {
  int type;
  lives_hook_stack_t **parent_stacks;
  volatile LiVESList *stack;
  pthread_mutex_t mutex;
  volatile uint64_t flags;
  union {
    lives_proc_thread_t lpt;
    pthread_t		thread;
  } owner;
  // for hook stacks with pattern request,
  // when triggered, the callbacks are not actioned, but instead
  // transferred to req_targer as if the caller had added them there originally
  // - except that the set_flags and unset_flags are applied before transfer
  lives_hook_stack_t **req_target_stacks;
  int req_target_type;
  uint64_t req_target_set_flags;
  uint64_t req_target_unset_flags;
} lives_hook_stack_t;

// hook_stack_flags

// stack is for native thread
#define STACK_NATIVE			(1ull << 0)
// callbacks are running now - used to prevnet multiple trigger instances
#define STACK_TRIGGERING		(1ull << 1)

// HOOK STACK_DESCRIPTORS - each hook stack type has an assosciated hook_descriptor
// which defines wken the stack may be triggered and how callbacks are handled on trigged

// data hooks are triggered before or after some item of data is added, altered or removed
// spontaneous hooks are triggered on demand, by the owner of the stack
// request hooks are a special type of spontaneous hook. When triggered, the callbacks are not
// actioned directly, instead they are transferred to another hook stack (the req_target)
// as if the original caller had added them there originally.
// this allows for accumulation and filtering of callbacks prior to them being added to the target
// the owner of the request queue responds to orinal request, accepting or denying it

typedef enum {HOOK_PATTERN_INVALID = -1,
              HOOK_PATTERN_DATA,
              HOOK_PATTERN_REQUEST,
              HOOK_PATTERN_SPONTANEOUS
             } hookstack_pattern_t;

typedef struct {
  int htype; // the hook type (e.g. COMPLETED, PREPARING)
  hookstack_pattern_t pattern; // base pattern data, spontaneous, request
  uint64_t op_flags; // flags defining trigger operation
  lives_funcdef_t *cb_proto; // defines the callback function prototype
} hookstack_descriptor_t;

uint64_t lives_hookstack_op_flags(int htype);
hookstack_pattern_t lives_hookstack_pattern(int htype);

hookstack_descriptor_t *get_hs_desc(void);

// flagbits for trigger_details
// the values are ORed with the callback flags

#define HOOKSTACK_INVALID		((uint64_t)-1)

// denotes that callbacks in the stack are run once only and removed
#define HOOKSTACK_ALWAYS_ONESHOT       	(1ull << 0)

// hook callbacks should be run asyncronously in parallel
#define HOOKSTACK_ASYNC_PARALLEL       	(1ull << 1)

// denotes that if the stack triggerer is NOT the GUI thread,
// flags should be ANDed with HOOKSTACK_MASK_NON_GUI
#define HOOKSTACK_GUI_THREAD	       	(1ull << 7)

#define HOOKSTACK_MASK_NON_GUI		((uint64_t)(~((uint64_t)0xFF00)))

#define HOOKSTACK_FLAGS_ADJUST(flagvar, is_gui_thread)			\
  _DW0(if(((flagvar)&HOOKSTACK_GUI_THREAD)&&!(is_gui_thread)) (flagvar)&=HOOKSTACK_MASK_NON_GUI;)

// this flagbit denotes that triggering the hooks will run only the first callback on the stack and return
// (when combined with ASYNC_POLL, the effect is to run a single iteration, all callbacks)
// note:
// the default stack order is  FIFO, however callbacks may be prepended in case LIFO is needed
#define HOOKSTACK_RUN_SINGLE      	(1ull << 8)

// callbacks are normally removed when the entity which added them is invalidated / freed
// setting this prevents that from happening, the callback remains in the stack beyond the lifetime of the adder
#define HOOKSTACK_PERSISTENT	       	(1ull << 9)

// if callbacks cannot be run immediately (because some other thread holds the mutex lock)
// return immediately and do not run the callbacks
#define HOOKSTACK_NOWAIT	       	(1ull << 10)

//

#define HOOKSTACK_NATIVE	       	(1ull << 16)

#define HS_FLAGS_FATAL			(HOOKSTACK_ALWAYS_ONESHOT | HOOKSTACK_NATIVE | HOOKSTACK_PERSISTENT)
#define HS_FLAGS_THREAD_EXIT		(HOOKSTACK_ALWAYS_ONESHOT | HOOKSTACK_NATIVE)
#define HS_FLAGS_DATA_READY		(HOOKSTACK_ASYNC_PARALLEL)
#define HS_FLAGS_LIVES_GUI		(HOOKSTACK_RUN_SINGLE | HOOKSTACK_GUI_THREAD | HOOKSTACK_ALWAYS_ONESHOT \
 							| HOOKSTACK_NOWAIT)
#define HS_FLAGS_SYNC_ANNOUNCE		(HOOKSTACK_PERSISTENT | HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_COMPLETED		(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_FINISHED		(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_CANCELLED		(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_ERROR			(HOOKSTACK_ALWAYS_ONESHOT)
#define HS_FLAGS_DESTRUCTION		(HOOKSTACK_ALWAYS_ONESHOT)

// low level flags used internally when adding callbacks*/

#define DTYPE_PREPEND 		(1ull << 0) // unset == append
#define DTYPE_CLOSURE 		(1ull << 1) // unset == lpt
#define DTYPE_NOADD 		(1ull << 2) // remove others only, no add
#define DTYPE_HAVE_LOCK 	(1ull << 8) // mutex already locked

void remove_from_hstack(lives_hook_stack_t *, LiVESList *);

// all dtypes
lives_proc_thread_t lives_hook_add(lives_hook_stack_t **, int type, uint64_t flags, livespointer data, uint64_t dtype);

// lpt like
lives_proc_thread_t lives_hook_add_full(lives_hook_stack_t **, int type, uint64_t flags, lives_funcptr_t func,
                                        const char *fname, int return_type, const char *args_fmt, ...);

// fixed cb type
#define lives_hook_append(hooks, type, flags, func, data) \
  lives_hook_add_full((hooks), (type), (flags),				\
		       (lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
		       "vv", NULL, (void *)(data))

#define lives_proc_thread_add_hook(lpt, type, flags, func, data)	\
  lives_hook_add_full(lives_proc_thread_get_hook_stacks(lpt), (type), (flags), \
		      (lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
		      "vv", (void *)lpt, (void *)(data))

// any func

#define lives_proc_thread_add_hook_full(lpt, type, flags, func, rtype, args_fmt, ...) \
  lives_hook_add_full(lives_proc_thread_get_hook_stacks(lpt), (type), (flags), \
		       (lives_funcptr_t)(func), #func, rtype, args_fmt, __VA_ARGS__)

#define lives_hook_append_full(hooks, type, flags, func, rtype, args_fmt, ...) \
  lives_hook_add_full((hooks), (type), (flags), (lives_funcptr_t)(func), #func, rtype, args_fmt, __VA_ARGS__)

#define lives_hook_prepend_full(hooks, type, flags, func, rtype, args_fmt, ...) \
 lives_hook_add_full((hooks), (type), (flags) | HOOK_CB_PRIORITY, (lives_funcptr_t)(func), #func, rtype, \
		       args_fmt, __VA_ARGS__)

// lpt arg

#define lives_hook_prepend(hooks, type, flags, lpt) lives_hook_add((hooks), (type), (flags),lpt, DTYPE_PREPEND)

////////////////////////////
lives_result_t proc_thread_params_from_vargs(lives_proc_thread_t, va_list xargs);


void lives_hook_remove(lives_proc_thread_t lpt);

void lives_hook_remove_by_data(lives_hook_stack_t **, int type, lives_funcptr_t func, void *data);

#define lives_proc_thread_remove_hook_by_data(lpt, type, func, data)	\
  lives_hook_remove_by_data(lives_proc_thread_get_hook_stacks(lpt), type, (lives_funcptr_t)func, data)

lives_closure_t *lives_proc_thread_get_closure(lives_proc_thread_t);

void flush_cb_list(lives_proc_thread_t self);

void lives_hooks_clear(lives_hook_stack_t **, int type);
void lives_hooks_clear_all(lives_hook_stack_t **, int ntypes);

boolean lives_hooks_trigger(lives_hook_stack_t **, int type);

boolean lives_proc_thread_trigger_hooks(lives_proc_thread_t, int type);

int lives_hooks_trigger_async(lives_hook_stack_t **, int type);

lives_proc_thread_t lives_hooks_trigger_async_sequential(lives_hook_stack_t **hstacks, int type, hook_funcptr_t finfunc,
    void *findata);

void lives_hooks_async_join(lives_hook_stack_t **, int htype);

lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t);

char *lives_proc_thread_show_func_call(lives_proc_thread_t lpt);

char *cl_flags_desc(uint64_t clflags);

void dump_hook_stack(lives_hook_stack_t **, int type);
void dump_hook_stack_for(lives_proc_thread_t, int type);

///////////// funcdefs, funcinsts and funcsigs /////

lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
                                int return_type,  const char *args_fmt, const char *file, int line,
                                void *data);

// can be used to link a file / line as being "inside" a function
#define create_funcdef_here(func) create_funcdef(#func, (lives_funcptr_t)func, 0, NULL, _FILE_REF_, -(_LINE_REF_), NULL)

// for future use - we can "steal" the funcdef from a proc_thread, stor it in a hash store
// then later do things like create a funcinst from a funcdef and params, then create a proc_thread from the funcinst
// so it then becomes like a template for factory producing proc_threads
// or, we could make a funcinst (snapshot) from a proc_thread, then use that to produce clones with the same param values
// as well as file / line ref of function (if known)
lives_funcdef_t *lives_proc_thread_to_funcdef(lives_proc_thread_t);

void free_funcdef(lives_funcdef_t *);

/* // a funcinst bears some similarity to a proc_thread, except it has only leaves for the paramters */
/* // plus a pointer to funcdef, in funcdef we can have uid, flags, cat, function, funcneme, ret_type, args_fmt */
/* lives_funcinst_t *create_funcinst(lives_funcdef_t *template, void *retstore, ...); */
/* void free_funcinst(lives_funcinst_t *); */

lives_funcinst_t *lives_funcinst_create(lives_funcdef_t *template, lives_proc_thread_t lpt,
                                        lives_thread_attr_t attrs, va_list xargs);

//lives_result_t weed_plant_params_from_vargs(weed_plant_t *plant, const char *args_fmt, va_list vargs);
lives_result_t weed_plant_params_from_args_fmt(weed_plant_t *plant, const char *args_fmt, ...);

funcsig_t funcsig_from_args_fmt(const char *args_fmt);
char *args_fmt_from_funcsig(funcsig_t funcsig);
const char get_typeletter(uint8_t val);
uint8_t get_typecode(char c);
const char get_char_for_st(uint32_t st);
const char *get_fmtstr_for_st(uint32_t st);

char *funcsig_to_string(funcsig_t sig);
char *funcsig_to_symstring(funcsig_t sig);
char *funcsig_to_param_string(funcsig_t sig);
char *funcsig_to_short_param_string(funcsig_t sig);

uint32_t get_seedtype(char c);

int fn_func_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2);
boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp);
//boolean fn_data_replace(lives_proc_thread_t src, lives_proc_thread_t dst);

int get_funcsig_nparms(funcsig_t sig);

const lives_funcdef_t *get_template_for_func(lives_funcptr_t func);
char *get_argstring_for_func(lives_funcptr_t func);
char *lives_funcdef_explain(const lives_funcdef_t *funcdef);

#endif
