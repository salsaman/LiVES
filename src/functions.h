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
h  GEN_SET(thing, wret, funcname, ARGS7((thing), t1, t2, t3, t4, t5, t6, t7))
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
#define FUNCSIG_INT_PLANTP 			       			0X0000001E
#define FUNCSIG_STRING_INT 			      			0X00000041
#define FUNCSIG_STRING_BOOL 			      			0X00000043
#define FUNCSIG_VOIDP_VOIDP 				       		0X000000DD
#define FUNCSIG_VOIDP_STRING 				       		0X000000D4
#define FUNCSIG_VOIDP_DOUBLE 				       		0X000000D2
#define FUNCSIG_VOIDP_INT64 				       		0X000000D5
#define FUNCSIG_DOUBLE_DOUBLE 				       		0X00000022
#define FUNCSIG_PLANTP_BOOL 				       		0X000000E3
#define FUNCSIG_PLANTP_VOIDP						0X000000ED
// 3p
#define FUNCSIG_VOIDP_DOUBLE_INT 		        		0X00000D21
#define FUNCSIG_VOIDP_STRING_STRING 		        		0X00000D44
#define FUNCSIG_VOIDP_VOIDP_VOIDP 		        		0X00000DDD
#define FUNCSIG_VOIDP_VOIDP_BOOL 		        		0X00000DD3
#define FUNCSIG_STRING_VOIDP_VOIDP 		        		0X000004DD
#define FUNCSIG_BOOL_BOOL_STRING 		        		0X00000334
#define FUNCSIG_PLANTP_VOIDP_INT 		        		0X00000ED1
#define FUNCSIG_PLANTP_VOIDP_INT64 		        		0X00000ED5
#define FUNCSIG_INT_VOIDP_INT64 		        		0X000001D5
#define FUNCSIG_INT_INT_BOOL	 		        		0X00000113
#define FUNCSIG_INT_INT64_VOIDP			        		0X0000015D
// 4p
#define FUNCSIG_STRING_STRING_VOIDP_INT					0X000044D1
#define FUNCSIG_STRING_DOUBLE_INT_STRING       				0X00004214
#define FUNCSIG_INT_INT_BOOL_VOIDP					0X0000113D
// 5p
#define FUNCSIG_VOIDP_INT_INT_INT_INT					0X000D1111
#define FUNCSIG_INT_INT_INT_BOOL_VOIDP					0X0001113D
#define FUNCSIG_VOIDP_STRING_STRING_INT64_INT			       	0X000D4451
#define FUNCSIG_PLANTP_VOIDP_INT_FUNCP_VOIDP			       	0X000ED1CD
#define FUNCSIG_PLANTP_INT_INT_INT_INT					0X000E1111
// 6p
#define FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP		       	0X0044D14D
#define FUNCSIG_PLANTP_INT_INT_INT_INT_INT	       			0X00E11111 
// 8p
#define FUNCSIG_PLANTP_INT_INT_INT_INT_INT_INT_INT     			0XE1111111
#define FUNCSIG_INT_DOUBLE_PLANTP_INT_INT_INT_INT_BOOL			0X12E11113
typedef weed_plant_t lives_funcinst_t;

weed_error_t weed_leaf_from_varg(weed_plant_t *, const char *key, uint32_t type, weed_size_t ne, va_list xargs);
boolean call_funcsig(lives_proc_thread_t);

typedef uint64_t funcsig_t;

#define LIVES_LEAF_TEMPLATE "_template"
#define LIVES_LEAF_FUNCSIG "_funcsig"
#define LIVES_LEAF_FUNC_NAME "_funcname"
#define LIVES_LEAF_CLOSURE "_closure"
#define LIVES_LEAF_FUNCDEF "_funcdef"
#define LIVES_LEAF_REPLACEMENT "_replacement"

#define LIVES_LEAF_XLIST_STACKS "xlist_stacks"
#define LIVES_LEAF_XLIST_TYPE "xlist_type"
#define LIVES_LEAF_XLIST_LIST "xlist_list"

#define LIVES_WEED_SUBTYPE_FUNCINST 150

#define FUNC_CATEGORY_HOOK_COMMON 	(N_STD_FUNC_CATEGORIES + 100)
#define FUNC_CATEGORY_HOOK_SYNC 	(N_STD_FUNC_CATEGORIES + 101)
#define FUNC_CATEGORY_HOOK_GUI	 	(N_STD_FUNC_CATEGORIES + 102)

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

#define ____FUNC_ENTRY____(func, rettype, args_fmt) \
  ((THREADVAR(func_stack) = lives_list_prepend(THREADVAR(func_stack),	\
					       QUOTEME(func)))		\
   ? add_fn_lookup((lives_funcptr_t)(func), QUOTEME(func), 0, rettype, args_fmt, \
		   _FILE_REF_, _LINE_REF_) : NULL)

#define ____FUNC_EXIT____ do {LiVESList *list = THREADVAR(func_stack);		\
    THREADVAR(func_stack) = list->next; list->next = NULL; lives_list_free(list);} while (0);

// calls (void)func(args), and before or after calling it, adds a fn note with a ptr / file / line
// if ptr is already noted then will remove it
// ptr is the result of pre_expn or post_expn cast to void *
// dump_fn_notes will list all func / file / line entries
// thus e.g _FUNC_TRACE_(0, get_ptr(), myfunc, args)
// will call myfunc(args), then on return cast the return from get_ptr() to void * and add or remove a note
// - can be used to trace allocs / frees by tracing the same pointer, e.g malloc / post_expn == returned ptr
// free / pre_expn == ptr to be freed...dump_fn_notes will then list all non-freed calls to malloc by file / lineno
#define _FN_ALLOC 1
#define _FN_FREE 0

#define _FUNC_ADD_TRACE_(io, expn, func, ...) do {\
    if ((expn)) add_fn_note(io, (void *)((expn)), #func,  _FILE_REF_, _LINE_REF_); \
  } while (0);


#define _FN_ALLOC_TRACE(fname) do {\
  THREADVAR(fn_alloc_trace) = #fname; THREADVAR(fn_alloc_triggered) = FALSE;\
  } while (0);

#define _FN_FREE_TRACE(fname) do {					\
  THREADVAR(fn_free_trace) = #fname; THREADVAR(fn_free_triggered) = FALSE;\
  } while (0);

#define _FN_ALLOC_TRACE_END do {\
  THREADVAR(fn_alloc_trace) = NULL; THREADVAR(fn_alloc_triggered) = FALSE;\
  } while (0);

#define _FN_FREE_TRACE_END do {\
  THREADVAR(fn_free_trace) = NULL; THREADVAR(fn_free_triggered) = FALSE;\
  } while (0);

#define _CHK_FN_ALLOC(expn) do {					\
    if (THREADVAR(fn_alloc_triggered)) _FUNC_ADD_TRACE_(_FN_ALLOC, expn, THREADVAR(fn_alloc_trace)); \
    THREADVAR(fn_alloc_triggered) = FALSE;				\
  } while (0);

#define _CHK_FN_FREE(var, expn, func, ...) do {				\
    void *p = (void *)(expn); var = func(__VA_ARGS__);			\
    if (THREADVAR(fn_free_triggered))					\
      _FUNC_ADD_TRACE_(_FN_FREE, p, THREADVAR(fn_free_trace));		\
    THREADVAR(fn_free_triggered) = FALSE;				\
  } while (0);

#define _CHK_FN_FREE_VOID(expn, func, ...) do {				\
    void *p = (void *)(expn);						\
    func(__VA_ARGS__);							\
    if (THREADVAR(fn_free_triggered))					\
      _FUNC_ADD_TRACE_(_FN_FREE, p, THREADVAR(fn_free_trace));		\
    THREADVAR(fn_free_triggered) = FALSE;				\
  } while (0);

#define _FN_ALLOC_TARGET(fname) do {\
  if (!lives_strcmp(THREADVAR(fn_alloc_trace), #fname)) THREADVAR(fn_alloc_triggered) = TRUE; \
  } while (0);

#define _FN_FREE_TARGET(fname) do {\
  if (!lives_strcmp(THREADVAR(fn_free_trace), #fname)) THREADVAR(fn_free_triggered) = TRUE; \
  } while (0);

//////////

/// HOOK FUNCTIONS ///////

#define LIVES_LEAF_HOOK_STACKS "hook_stacks"

#define LIVES_SEED_HOOK WEED_SEED_FUNCPTR

//< caller will block when adding the hook and only return when the hook callback has returned
// if the cb function is barred by another (due to unqiqueness constraints), the the thread will block until
// the imposing function returns
#define HOOK_CB_BLOCK       		(1ull << 0)

// hook is GUI related and must be run ONLY by the fg / GUI thread
#define HOOK_CB_FG_THREAD		(1ull << 2) // force fg service run

// hook should be run as soon as possible when the hook trigger point is reached
#define HOOK_CB_PRIORITY		(1ull << 3) // prepend, not append

// for fg requests, if it cannot be run immediately then destroy it rather than deferring
#define HOOK_OPT_NO_DEFER		(1ull << 4)

// callback will only be run at most one time, and then removed from the stack
#define HOOK_OPT_ONESHOT		(1ull << 5)

// this combination may be use for hggh priority GUI updates which need to be run before the thread can continue
#define HOOK_CB_IMMEDIATE (HOOK_CB_FG_THREAD | HOOK_CB_PRIORITY | HOOK_CB_BLOCK)

/// the following bits define how hooks should be added to the stack
// in case of duplicate functions / data
// after adding, ensure only a single copy of FUNC in the stack, with whatever data
#define HOOK_UNIQUE_FUNC		(1ull << 16) // do not add if func already in hooks

// after adding, ensure only a single copy of FUNC  / DATA in stack
// may be other copies of func with other data

// NOTE: for data matching, it is sometimes desirable to match only the first n paramaeters, with the remainder
// being the data to be replaced
// in this case,   THREADVAR(hook_match_nparams) can be set to the number to match. If set to 0, the default,
// all params must be matched. Thus HOOK_UNIQUE_REPLACE only makes sense with a non zero value
// else all parameters would be matched, and there would be no "data" to be replaced

#define HOOK_UNIQUE_DATA		(1ull << 17) // do not add if func / data already in hooks (UNIQUE_FUNC assumed)

// change data of first func of same type but leave func inplace,
// (after adding, there will be only one copy of FUNC, with our data)
// * copies of the func in linked_stacks must also be removed
#define HOOK_UNIQUE_REPLACE		(HOOK_UNIQUE_FUNC | HOOK_UNIQUE_DATA)

// NOTEs:
//
// 1) when prepending, for uniqueness purposes, the prepended callback can replace (and remove) others already in the stack
// including in linked stacks
//
// 2) when appending unique_func and/or unique_data to the main GUI stack,
// the new callback can be blocked or added
// Regardless, all matching callbacks must be replaced (but not removed) in linked stacks,
// as if the callback were being prepended to them.
//
// If the replaced callback is subsequently to be appended to fg stack it should be removed instead
// if to be prepended, the replacmeent marker is removed, and the callback is prepended as normal
//
// 1) if a TRIGGERED callback has uniqueness flags, we can replace (and remove)
// callbacks in the fg_thread stack, + linked stacks, in an equivalent manner to if the callback was prepended
//
// 4) when replacing a callback, check if it was added with HOOK_CB_BLOCK
// if so, it cannot be removed directly, instead we must add a reference to the new callback, on behalf of the blocked thread
// and set a leaf LIVES_LEAF_REPLACEMENT in the proc thread being replaced, pointing the newly added callback
// we also add
// the blocked thread should detect this, unref the original blocker, and continue waiting instead for the new callback
// the replacement callback can also be replaced, so the sequence must be followed until reaching a non replaced cb, or one which has
// completed / been destroyed
// - replace (remove) means replace the proc_thread and remove the closure form the stack
// - replace (no remove) means mark the proc_thread as replaced, but leave in the stack
//

// first, remove any callbacks (globally) with matching data, any function
// then run this callback - this is designed for functions which
// functions which free the data, - preventing any older callbacks from trying to access older data
// - callbacks marked as BLOCK will be replaced by this one
#define HOOK_INVALIDATE_DATA		((1ull << 18) | HOOK_CB_PRIORITY | HOOK_CB_BLOCK) 

///////////////////

#define HOOK_STATUS_BLOCKED			(1ull << 20) // hook function should not be called
#define HOOK_STATUS_RUNNING			(1ull << 21) // hook cb running, do not recurse

// hook cb has been replaced by another closure (due to uniqueness)
// callback should be ignored
#define HOOK_STATUS_REPLACED			(1ull << 22)

#define HOOK_STATUS_REMOVE			(1ull << 28) // hook was 'removed' whilst running, delay removal until return

 // test uniquenes conditions to see if we can add, but do not add, remove or replace anything
#define HOOK_CB_TEST_ADD			(1ull << 30)

// values higher than this are reserved for closure flags
#define HOOK_CB_FLAG_MAX			(1ull << 31)

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

// when adding a hook callback, there are several methods
// use a registered funcname, in this case the funcdef_t is looed up from funcname, and
// we create a lpt using funcdef as a template, and including the function va_args
//
// alternately, we can create a lpt from function args
// finally, we can create an unqueued lpt directly, then include this
//
// 
typedef struct {
  // proc_thread intially in unqueued state - either supplied directly
  // or created from param args / hook_type
  lives_proc_thread_t proc_thread;
  // func def describing hook cb, this can be created from the lpt
  // or it can be a pointer to a static funcdef
  // once registered, then we can simply add hooks using funcname, params
  // it also had a field for category
  const lives_funcdef_t *fdef;
  uint64_t flags; // HOOK_CB flags
  // how many params must be identical to count as a data match ?
  // default is 0, all params
  int nmatch_params;
  void *retloc; // pointer to a var to store return val in
} lives_closure_t;

#define LIVES_FDEF_STATIC		(1ull << 0)


/* lives_closure_t *lives_hook_closure_new(lives_funcptr_t func, const char *fname, uint64_t flags, */
/* 					int hook_type, void *data); */

lives_closure_t *lives_hook_closure_new_for_lpt(lives_proc_thread_t lpt, uint64_t flags, int hook_type);

void lives_closure_free(lives_closure_t *closure);

#define LIVES_GUI_HOOK INTERNAL_HOOK_0

typedef struct {
  LiVESList *stack;
  pthread_mutex_t *mutex;
  uint64_t flags;
} lives_hook_stack_t;

// only thread owning the stack may add callbacks
#define HOOK_DTL_SELF			(1ull < 0)

// hook callbacks should be run asyncronously in parallel
// if combined with REMOVE_FALSE:
// they will not be removed from the stack, that must be done when joined
#define HOOK_DTL_ASYNC_CALLBACKS     	(1ull < 1)

// hook callbacks should be run with async caller, but the trigger must then run them syncronously in sequence
#define HOOK_DTL_ASYNC_TRIGGER	       	(1ull < 2)

// the return type of the callbacks must be be boolean, any which return FALSE should be removed from the stack
// combined with ASYNC, the callbacks must be joined and removed if they return FALSE
#define HOOK_DTL_REMOVE_FALSE      	(1ull < 3)

// the return type of the callbacks must be boolean, the callbacks are to be triggered in sequence
// the hook trigger function should return TRUE if and only if all callbacks return TRUE
// otherwise it should return FALSE
#define HOOK_DTL_COMBINED_BOOL      	(1ull < 4)

// the return type of the callbacks must be boolean, the callbacks are to be triggered in sequence
// hook callbacks are to be triggered, then after a short pause, triggered again
// this continues until all callbacks return TRUE
// - the trigger thread MUST be cancellable and must return between triggering if cancelled
// - the trigger thread MUST be pausable and can pause between hook triggereing
#define HOOK_DTL_BOOL_LOOP	       	(1ull < 5)

// callback payload should be passed to the main thread to be run
#define HOOK_DTL_MAIN_THREAD	       	(1ull < 6)

// this flagbit denotes that triggering the hooks will run only the first callback on the stack and return
// otherwise all callbacks are triggered in sequence or in parallel
#define HOOK_DTL_SINGLE	 	      	(1ull < 7)

/* #define HSTACK_FLAGS(DATA_READY_HOOK) HOOK_DTL_ASYNC_CALLBACKS */
/* #define HSTACK_FLAGS(SYNC_WAIT_HOOK) HOOK_DTL_SELF | HOOK_DTL_ASYNC_TRIGGER | HOOK_DETAIL_COMBINED_BOOL | HOOK_DTL_BOOL_LOOP */
/* #define HSTACK_FLAGS(INTERNAL_HOOK_0) HOOK_DETAIL_MAIN_THREAD | HOOK_DTL_SINGLE */

// TODO - add retloc as param to hook_add

#define DTYPE_PREPEND (1ull << 0) // unset == append
#define DTYPE_CLOSURE (1ull << 1) // unset == lpt

// all dtypes
lives_proc_thread_t _lives_hook_add(lives_hook_stack_t **hooks, int type, uint64_t flags, livespointer data, uint64_t dtype);

// lpt like
lives_proc_thread_t _lives_hook_add_full(lives_hook_stack_t **hooks, int type, uint64_t flags, boolean is_append,
    lives_funcptr_t func, const char *fname, int return_type, const char *args_fmt, ...);


#define lives_proc_thread_append_hook(lpt, type, flags, func, data)	\
  _lives_hook_add_full(lives_proc_thread_get_hook_stacks(lpt), (type), (flags), TRUE, \
		       (lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
		       "vv", NULL, (void *)(data))

#define lives_proc_thread_append_hook_full(lpt, type, flags, func, rtype, args_fmt, ...) \
  _lives_hook_add_full(lives_proc_thread_get_hook_stacks(lpt), (type), (flags), TRUE, \
		       (lives_funcptr_t)(func), #func, rtype, args_fmt, __VA_ARGS__)

// all dtype
#define lives_hook_test_add(hooks, type, flags, data, dtype) _lives_hook_add((hooks), (type), (flags) | HOOK_CB_TEST_ADD, \
									     data, dtype)
//(void *)(data), lpt like
#define lives_hook_append_full(hooks, type, flags, func, rtype, args_fmt, ...) \
  _lives_hook_add_full((hooks), (type), (flags), TRUE, (lives_funcptr_t)(func), #func, rtype, args_fmt, __VA_ARGS__)

// scalar
#define lives_hook_append(hooks, type, flags, func, data) _lives_hook_add_full((hooks), (type), (flags), TRUE, \
									       (lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
									       "vv", NULL, (void *)(data))
// lpt like
#define lives_hook_prepend_full(hooks, type, flags, func, rtype, args_fmt, ...) \
  _lives_hook_add_full((hooks), (type), (flags), FALSE, (lives_funcptr_t)(func), #func, rtype, args_fmt, __VA_ARGS__)

// scalar
#define lives_hook_prepend(hooks, type, flags, func, data) _lives_hook_add_full((hooks), (type), (flags), FALSE, \
										(lives_funcptr_t)(func), #func, WEED_SEED_BOOLEAN, \
										"vv", NULL, (void *)(data))

void lives_hook_remove(lives_hook_stack_t **hstacks, int type, lives_proc_thread_t lpt);

#define lives_proc_thread_remove_hook(lpt, type, cb_lpt) \
  lives_hook_remove(lives_proc_thread_get_hook_stacks(lpt), type, cb_lpt)

void lives_hooks_clear(lives_hook_stack_t **hooks, int type);
void lives_hooks_clear_all(lives_hook_stack_t **hooks, int ntypes);

boolean lives_hooks_trigger(lives_hook_stack_t **hooks, int type);

void lives_hooks_trigger_async(lives_hook_stack_t **hooks, int type);

lives_proc_thread_t lives_hooks_trigger_async_sequential(lives_proc_thread_t lpt, lives_hook_stack_t **hstacks, int type,
							 hook_funcptr_t finfunc, void *findata);

void lives_hooks_join(lives_hook_stack_t *);

lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t);

char *lives_proc_thread_show_func_call(lives_proc_thread_t lpt);

///////////// funcdefs, funcinsts and funcsigs /////

lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
                                uint32_t return_type,  const char *args_fmt, const char *file, int line,
                                void *data);

void free_funcdef(lives_funcdef_t *);

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
boolean fn_data_replace(lives_proc_thread_t src, lives_proc_thread_t dst);

int get_funcsig_nparms(funcsig_t sig);

const lives_funcdef_t *get_template_for_func(lives_funcptr_t func);
char *get_argstring_for_func(lives_funcptr_t func);
char *lives_funcdef_explain(const lives_funcdef_t *funcdef);

void add_fn_note(int in_out, void *ptr, const char *fname, const char *fref, int lineno);

void dump_fn_notes(void);

#endif