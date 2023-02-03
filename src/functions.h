// functions.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H

///// low level operations //////

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
  char letter;
  uint32_t seed_type;
  uint8_t sigbits;
  const char *symname;
  const char *fmtstr;
} lookup_tab;

extern const lookup_tab crossrefs[];

#define PROC_THREAD_PARAM(n) LIVES_LEAF_THREAD_PARAM  #n

#define GETARG(thing, type, n) (p##n = WEED_LEAF_GET((thing), PROC_THREAD_PARAM(n), type))

// since the codification of a param type only requires 4 bits, in theory we could go up to 16 parameters
// however 8 is probably sufficient and looks neater
// it is also possible to pass functions as parameters, using _FUNCP, so things like
// FUNCSIG_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP
// are a possibility

#ifdef WEED_SEED_UINT
#define _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)\
  case(WEED_SEED_UINT):pre(pre2,pre3##uint##post(post2,post3,post4));break;
#else
#define _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)
#endif
#ifdef WEED_SEED_UINT64
#define _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4) \
  case(WEED_SEED_UINT64):pre(pre2,pre3##uint64##post(post2,post3,post4));break;
#else
#define _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4)
#endif

#define FOR_ALL_SEED_TYPES(var, pre, pre2, pre3, post, post2, post3, post4)	\
  do{switch(var){case(WEED_SEED_INT):pre(pre2,pre3##int##post(post2,post3,post4));break; \
  case(WEED_SEED_INT64):pre(pre2,pre3##int64##post(post2,post3,post4));break;	\
 case(WEED_SEED_BOOLEAN):pre(pre2,pre3##boolean##post(post2,post3,post4));break; \
    case(WEED_SEED_DOUBLE):pre(pre2,pre3##double##post(post2,post3,post4));break; \
 case(WEED_SEED_STRING):pre(pre2,pre3##string##post(post2,post3,post4));break; \
    case(WEED_SEED_VOIDPTR):pre(pre2,pre3##voidptr##post(post2,post3,post4));break; \
 case(WEED_SEED_FUNCPTR):pre(pre2,pre3##funcptr##post(post2,post3,post4));break; \
    case(WEED_SEED_PLANTPTR):pre(pre2,pre3##plantptr##post(post2,post3,post4));break; \
      _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)		\
	_CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4)	\
    default:break;}}while(0);

#define GEN_SET(thing, wret, funcname, FUNCARGS) err = \
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

// e.g ARGS(7, lpt, t8, t1, ,,,)
#define _ARGS(n, thing, tn, ...) ARGS##n(thing, __VA_ARGS__), GETARG(thing, tn, n)
#define CALL_VOID_8(thing, ...) (*thefunc->func)(ARGS8((thing), __VA_ARGS__))
#define CALL_VOID_7(thing, ...) (*thefunc->func)(ARGS7((thing), __VA_ARGS__))
#define CALL_VOID_6(thing, ...) (*thefunc->func)(ARGS6((thing), __VA_ARGS__))
#define CALL_VOID_5(thing, ...) (*thefunc->func)(ARGS5((thing), __VA_ARGS__))
#define CALL_VOID_4(thing, ...) (*thefunc->func)(ARGS4((thing), __VA_ARGS__))
#define CALL_VOID_3(thing, ...) (*thefunc->func)(ARGS3((thing), __VA_ARGS__))
#define CALL_VOID_2(thing, ...) (*thefunc->func)(ARGS2((thing), __VA_ARGS__))
#define CALL_VOID_1(thing, ...) (*thefunc->func)(ARGS1((thing), __VA_ARGS__))
#define CALL_VOID_0(thing, dummy) (*thefunc->func)()
#define XCALL_8(thing, wret, funcname, t1, t2, t3, t4, t5, t6, t7, t8)	\
  GEN_SET(thing, wret, funcname, _ARGS(7, (thing), t8, t1, t2, t3, t4, t5, t6, t7))
#define XCALL_7(thing, wret, funcname, t1, t2, t3, t4, t5, t6, t7)	\
  GEN_SET(thing, wret, funcname, _ARGS(6, (thing), t7, t1, t2, t3, t4, t5, t6))
#define XCALL_6(thing, wret, funcname, t1, t2, t3, t4, t5, t6)		\
  GEN_SET(thing, wret, funcname, _ARGS(5, (thing), t6, t1, t2, t3, t4, t5))
#define XCALL_5(thing, wret, funcname, t1, t2, t3, t4, t5)		\
  GEN_SET(thing, wret, funcname, _ARGS(4, (thing), t5, t1, t2, t3, t4))
#define XCALL_4(thing, wret, funcname, t1, t2, t3, t4)			\
  GEN_SET(thing, wret, funcname, _ARGS(3, (thing), t4, t1, t2, t3))
#define XCALL_3(thing, wret, funcname, t1, t2, t3)		\
  GEN_SET(thing, wret, funcname, _ARGS(2, (thing), t3, t1, t2))
#define XCALL_2(thing, wret, funcname, t1, t2)		\
  GEN_SET(thing, wret, funcname, _ARGS(1, (thing), t2, t1))
#define XCALL_1(thing, wret, funcname, t1)		\
  GEN_SET(thing, wret, funcname, ARGS1((thing), t1))
#define XCALL_0(thing, wret, funcname, dummy)	\
  GEN_SET(thing, wret, funcname, )

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
#define FUNCSIG_INT_INT 			       			0X00000011
#define FUNCSIG_BOOL_INT 			       			0X00000031
#define FUNCSIG_INT_VOIDP 			       			0X0000001D
#define FUNCSIG_STRING_INT 			      			0X00000041
#define FUNCSIG_STRING_BOOL 			      			0X00000043
#define FUNCSIG_VOIDP_BOOL 				       		0X000000D3
#define FUNCSIG_VOIDP_VOIDP 				       		0X000000DD
#define FUNCSIG_VOIDP_STRING 				       		0X000000D4
#define FUNCSIG_VOIDP_DOUBLE 				       		0X000000D2
#define FUNCSIG_VOIDP_INT64 				       		0X000000D5
#define FUNCSIG_DOUBLE_DOUBLE 				       		0X00000022
// 3p
#define FUNCSIG_VOIDP_DOUBLE_INT 		        		0X00000D21
#define FUNCSIG_VOIDP_DOUBLE_DOUBLE 		        		0X00000D22
#define FUNCSIG_VOIDP_VOIDP_VOIDP 		        		0X00000DDD
#define FUNCSIG_VOIDP_VOIDP_BOOL 		        		0X00000DD3
#define FUNCSIG_STRING_VOIDP_VOIDP 		        		0X000004DD
#define FUNCSIG_PLANTP_VOIDP_INT64 		        		0X00000ED5
#define FUNCSIG_INT_INT_BOOL	 		        		0X00000113
#define FUNCSIG_STRING_INT_BOOL	 		        		0X00000413
#define FUNCSIG_INT_INT64_VOIDP			        		0X0000015D

// 4p
#define FUNCSIG_STRING_DOUBLE_INT_STRING       				0X00004214
#define FUNCSIG_INT_INT_BOOL_VOIDP					0X0000113D
#define FUNCSIG_VOIDP_INT_FUNCP_VOIDP				       	0X0000D1CD
// 5p
#define FUNCSIG_VOIDP_INT_INT_INT_INT					0X000D1111
#define FUNCSIG_INT_INT_INT_BOOL_VOIDP					0X0001113D
#define FUNCSIG_VOIDP_STRING_STRING_INT64_INT			       	0X000D4451
#define FUNCSIG_VOIDP_VOIDP_BOOL_BOOL_INT				0X000DD331
// 6p
#define FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP		       	0X0044D14D

typedef uint64_t funcsig_t;

weed_error_t weed_leaf_from_varg(weed_plant_t *, const char *key, uint32_t type, weed_size_t ne, va_list xargs);
boolean call_funcsig(lives_proc_thread_t);

// funcinst (future use)

typedef weed_plant_t lives_funcinst_t;
#define LIVES_LEAF_TEMPLATE "_template"
#define LIVES_LEAF_FUNCSIG "_funcsig"
#define LIVES_LEAF_FUNC_NAME "_funcname"
#define LIVES_LEAF_FUNCDEF "_funcdef"
#define LIVES_LEAF_REPLACEMENT "_replacement"

#define LIVES_WEED_SUBTYPE_FUNCINST 150

#define LIVES_LEAF_CLOSURE "_closure"

#define LIVES_LEAF_XLIST_STACKS "xlist_stacks"
#define LIVES_LEAF_XLIST_TYPE "xlist_type"
#define LIVES_LEAF_XLIST_LIST "xlist_list"

#define FUNC_CATEGORY_HOOK_COMMON 	(N_STD_FUNC_CATEGORIES + 100)
#define FUNC_CATEGORY_HOOK_SYNC 	(N_STD_FUNC_CATEGORIES + 101)
#define FUNC_CATEGORY_HOOK_GUI	 	(N_STD_FUNC_CATEGORIES + 102)

/// tracing / debugging tools //

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

typedef
enum {
  // enum   // va_args for add_fn_note
  _FN_FREE, // fundef, ptr
  _FN_ALLOC, // fundef, ptr
} fn_type_t;

#define _FUNCREF(fn,f,l)((void *)(create_funcdef(#fn,(lives_funcptr_t)fn,0,0,f,l,0)))
#define ADD_NOTE(ftype,fname,...)					\
  (add_fn_note(ftype,_FUNCREF(fname,_FILE_REF_,_LINE_REF_),fname(__VA_ARGS__)))
#define DO_ADD_NOTE(ftype,fname,...) do{(void)ADD_NOTE(ftype,fname,__VA_ARGS__);}while(0)

#define FN_ALLOC_TARGET(fname,...) ADD_NOTE(_FN_ALLOC,fname,__VA_ARGS__)
#define FN_FREE_TARGET(fname,...) DO_ADD_NOTE(_FN_FREE,fname,__VA_ARGS__)

// add a note to to ftrace_store, va_args depend on fn_type
void *add_fn_note(fn_type_t, ...);
// dump notes (traces) from ftrace_store
void dump_fn_notes(void);

#define FN_DEBUG_OUT(func) g_print("Thread %ld in func %s at %s, line %d\n", \
				   THREADVAR(uid), #func, _FILE_REF_, _LINE_REF_)

#define FN_DEBUG_EXIT_OUT g_print("Thread %ld exiting func at %s, line %d\n", \
				  THREADVAR(uid), _FILE_REF_, _LINE_REF_)

// macro to be placed near start of "major" functions. It will prepend funcname to
// a thread's 'func_stack', print out a debug line (optional), and also add fn lookup
// to fn_store, e.g:   ____FUNC_ENTRY____(transcode_clip, "b", "iibs");
#define ____FUNC_ENTRY____(func, rettype, args_fmt)			\
  do {THREADVAR(func_stack)=lives_list_prepend(THREADVAR(func_stack),#func);FN_DEBUG_OUT(func);	\
    add_fn_lookup((lives_funcptr_t)(func),#func,0,rettype,args_fmt,_FILE_REF_, _LINE_REF_);}while (0);

// macro to be placed near start of "major" functions, counterpart to ___FUNC_ENTRY___
// It will remove top entry from a thread's 'func_stack', and print out a debug line (optional)
#define ____FUNC_EXIT____ do {LiVESList *list=THREADVAR(func_stack);FN_DEBUG_EXIT_OUT; \
    THREADVAR(func_stack)=list->next;list->next=NULL;lives_list_free(list);} while (0);

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

#define LIVES_SEED_HOOK WEED_SEED_FUNCPTR

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
#define HOOK_UNIQUE_DATA		(1ull << 17) // do not add if func / data already in hooks (UNIQUE_FUNC assumed)

// change data of first func of same type but leave func inplace,
// (after adding, there will be only one copy of FUNC, with our data)
// * copies of the func in linked_stacks must also be removed
// (when prepending, this always succeeds to add, and acts identically to unique_func,
// when appending, the callback will replace data and be blocked or else appended)
#define HOOK_UNIQUE_REPLACE		(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_DATA)

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
  uint32_t return_type;
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

  // proc_thread intially in unqueued state - either supplied directly
  // or created from param args / hook_type
  // adder is used so we can trace back to lpt which added the callback, and if freed,
  // remove it from other proc_thread's hook_cb_list
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
  volatile LiVESList *stack;
  pthread_mutex_t *mutex;
  volatile uint64_t flags;
} lives_hook_stack_t;

// hook_stack_flags

// callbacks are running now - used to prevnet multiple trigger instances
#define STACK_TRIGGERING		(1ull < 0)


// HOOK DETAILS - each hook stack TYPE (by enumeration) can have a hook details flag,
// describing the operation of that hook_type

typedef struct {
  int htype; // the hook type (e.g. COMPLETED, PREPARING)
  // (data hooks are trigger before and after some item of data is added, altered or removed)
  // spontaneous hooks are triggered on demand, by the same proc_thread holding the stack
  // request hooks are in two parts - a callback is added to the request stack of another proc_thread
  // the target proc_thread responds to the request, accepting or denying it
  int model; // data, spontaneous, request
  // conditional - for data hooks, defines the conditions which cause the hook to be triggered
  // - the specific item triggering it, before or after, old value, new value
  uint64_t trigger_details; // flags defining trigger operation
  lives_funcdef_t *cb_proto; // defines the callback function prototype
} hook_descriptor_t;

// flagbits for trigger_details

// denotes that the hook stack is in the thread, rather than in the proc_thread
// only the thread itself may add / remove callbacks and trigger this
#define HOOK_DTL_SELF			(1ull < 0)

// hook callbacks should be run asyncronously in parallel
#define HOOK_DTL_ASYNC_PARALLEL	     	(1ull < 1)

// the return type of the callbacks must be be boolean,
// any which return FALSE will be blocked (ignored)
// blocked callbacks can then either be removed, or unblocked
#define HOOK_DTL_BLOCK_FALSE      	(1ull < 2)

// the return type of the callbacks must be boolean,
// the callbacks are run as normal (depending on other flags)
// the hook trigger function will return TRUE if and only if ALL callbacks return TRUE
// FALSE if any return FALSE
#define HOOK_DTL_COMBINED_BOOL      	(1ull < 3)

// this flagbit denotes that triggering the hooks will run only the first callback on the stack and return
// (when combined with ASYNC_POLL, the effect is to run a single iteration, all callbacks)
// note:
// the default stack order is  FIFO, however callbacks may be prepended in case LIFO is needed
#define HOOK_DTL_SINGLE	 	      	(1ull < 4)

// an async poller is created, it will trigger all callbacks in sequence, then after a short pause
// repeat this process
// the poller itself has hooks, so adding a callback to COMPLETED_HOOK can determine when it returns
// - if COMBINED_BOOL is also in flags, polling will stop when TRUE is returned
// - if SINGLE is in flags, the effect is to run a single iteration (all callbacks) once and return
// - the poller MUST be cancellable and must return between triggering if cancelled
// - the poller MUST be pausable and can pause between hook triggereing
#define HOOK_DTL_ASYNC_POLL		       	(1ull < 5)

// callback payload should be passed to the main thread to be run
#define HOOK_DTL_MAIN_THREAD	       	(1ull < 6)

//

/* #define HSTACK_FLAGS(DATA_READY_HOOK) HOOK_DTL_ASYNC_CALLBACKS */
/* #define HSTACK_FLAGS(SYNC_WAIT_HOOK) HOOK_DTL_SELF | HOOK_DTL_ASYNC_TRIGGER | HOOK_DETAIL_COMBINED_BOOL | HOOK_DTL_BOOL_LOOP */
/* #define HSTACK_FLAGS(INTERNAL_HOOK_0) HOOK_DETAIL_MAIN_THREAD | HOOK_DTL_SINGLE */

// low level flags used internally when adding callbacks

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
		      "vv", NULL, (void *)(data))

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

void lives_hook_remove(lives_proc_thread_t lpt);

void lives_hook_remove_by_data(lives_hook_stack_t **, int type, lives_funcptr_t func, void *data);

#define lives_proc_thread_remove_hook_by_data(lpt, type, func, data)	\
  lives_hook_remove_by_data(lives_proc_thread_get_hook_stacks(lpt), type, (lives_funcptr_t)func, data)

void lives_proc_thread_remove_nullify(lives_proc_thread_t, void **ptr);

lives_closure_t *lives_proc_thread_get_closure(lives_proc_thread_t);

void flush_cb_list(lives_proc_thread_t self);

void lives_hooks_clear(lives_hook_stack_t **, int type);
void lives_hooks_clear_all(lives_hook_stack_t **, int ntypes);

boolean lives_hooks_trigger(lives_hook_stack_t **, int type);

boolean lives_proc_thread_trigger_hooks(lives_proc_thread_t, int type);

void lives_hooks_trigger_async(lives_hook_stack_t **, int type);

lives_proc_thread_t lives_hooks_trigger_async_sequential(lives_hook_stack_t **hstacks, int type, hook_funcptr_t finfunc,
    void *findata);

void lives_hooks_async_join(lives_hook_stack_t *);

lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t);

char *lives_proc_thread_show_func_call(lives_proc_thread_t lpt);

char *cl_flags_desc(uint64_t clflags);

void dump_hook_stack(lives_hook_stack_t **, int type);

///////////// funcdefs, funcinsts and funcsigs /////

lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
                                uint32_t return_type,  const char *args_fmt, const char *file, int line,
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


// a funcinst bears some similarity to a proc_thread, except it has only leaves for the paramters
// plus a pointer to funcdef, in funcdef we can have uid, flags, cat, function, funcneme, ret_type, args_fmt
lives_funcinst_t *create_funcinst(lives_funcdef_t *template, void *retstore, ...);
void free_funcinst(lives_funcinst_t *);

lives_result_t weed_plant_params_from_vargs(weed_plant_t *plant, const char *args_fmt, va_list vargs);
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
