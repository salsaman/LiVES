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
} lookup_tab;

extern const lookup_tab crossrefs[];

#define PROC_THREAD_PARAM(n) LIVES_LEAF_THREAD_PARAM  #n

#define GETARG(thing, type, n) (p##n = WEED_LEAF_GET((thing), PROC_THREAD_PARAM(n), type))

// since the codification of a param type only requires 4 bits, in theory we could go up to 16 parameters
// however 8 is probably sufficient and looks neater
// it is also possible to pass functions as parameters, using _FUNCP, so things like
// FUNCSIG_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP
// are a possibility

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
// 8p
#define FUNCSIG_PLANTP_INT_INT_INT_INT_INT_INT_INT     			0XE1111111
#define FUNCSIG_INT_DOUBLE_PLANTP_INT_INT_INT_INT_BOOL			0X12E11113
typedef weed_plant_t lives_funcinst_t;

weed_error_t weed_leaf_from_varg(weed_plant_t *, const char *key, uint32_t type, weed_size_t ne, va_list xargs);
boolean call_funcsig(lives_proc_thread_t);

typedef uint64_t funcsig_t;

#define LIVES_LEAF_TEMPLATE "template"
#define LIVES_LEAF_FUNCSIG "funcsig"
#define LIVES_LEAF_FUNC_NAME "funcname"
#define LIVES_LEAF_CLOSURE "closure"

#define LIVES_LEAF_XLIST_TYPE "xlist_type"
#define LIVES_LEAF_XLIST_LIST "xlist_list"

#define LIVES_WEED_SUBTYPE_FUNCINST 150

#define FUNC_CATEGORY_HOOK_COMMON 	256
#define FUNC_CATEGORY_HOOK_SYNC 	257

typedef struct {
  uint64_t uid;
  int category; // category type for function (0 for none)
  const char *funcname; // optional
  lives_funcptr_t function;
  uint32_t return_type;
  const char *args_fmt;
  funcsig_t funcsig;
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

// this combination may be use for hggh priority GUI updates which need to be run before the thread cna continue
#define HOOK_CB_IMMEDIATE (HOOK_CB_FG_THREAD | HOOK_CB_PRIORITY | HOOK_CB_BLOCK)

//< caller will block when adding the hook and only return when the hook callback has returned
#define HOOK_CB_BLOCK       		(1ull << 0)

//< any hooks set with this flagbit will be transferred to child threads of the principle thread
#define HOOK_CB_CHILD_INHERITS		(1ull << 1)

// hook is GUI related and must be run ONLY by the fg / GUI thread
#define HOOK_CB_FG_THREAD		(1ull << 2) // force fg service run

// hook should be run as soon as possible when the hook trigger point is reached
#define HOOK_CB_PRIORITY		(1ull << 3) // prepend, not append

// for fg requests, if it cannot be run immediately then drop it rather than deferring
#define HOOK_OPT_NO_DEFER		(1ull << 4)

/// the following bits define how hooks should be added to the stack
// in case of duplicate functions / data
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

#define HOOK_STATUS_REMOVE			(1ull << 34) // hook was 'removed' whilst running, delay removal until return

typedef weed_plant_t lives_obj_t;
typedef boolean(*hook_funcptr_t)(lives_obj_t *, void *);
typedef char *(*make_key_f)(int pnum);

typedef struct {
  //hook_funcptr_t func;
  const lives_funcdef_t *fdef;
  lives_object_t *obj;
  void *attr;
  void *data;
  uint64_t flags;
  lives_proc_thread_t tinfo; // for fg threads
  void *retloc; // pointer to a var to store return val in
} lives_closure_t;

// funcinst has: funcdef, func, returm type, args_fmt and xargs
// closure designed more for storing, it has ptr to owner object, flags and a space for a procthread
// -> combine both as object instance

typedef struct {
  pthread_mutex_t *mutex;
  LiVESList *stack;
} lives_hook_stack_t;


lives_proc_thread_t _lives_hook_add(lives_hook_stack_t **hooks, int type, uint64_t flags, hook_funcptr_t func,
                                    const char *fname, livespointer data, boolean is_append);

void _lives_proc_thread_hook_append(lives_proc_thread_t lpt, int type, uint64_t flags,
                                    hook_funcptr_t func, const char *fname, livespointer data);

#define lives_proc_thread_hook_append(lpt, type, flags, func, data)	\
  (_lives_proc_thread_hook_append((lpt), (type), (flags), (hook_funcptr_t)(func), #func, (void *)(data)))

#define lives_hook_append(hooks, type, flags, func, data) _lives_hook_add((hooks), (type), (flags), \
									  (hook_funcptr_t)(func), #func, \
									  (void *)(data), TRUE)

#define lives_hook_prepend(hooks, type, flags, func, data) _lives_hook_add((hooks), (type), (flags), \
									   (hook_funcptr_t)(func), #func, \
									   (void *)(data), FALSE)

void _lives_hook_remove(lives_hook_stack_t **hooks, int type, hook_funcptr_t func, livespointer data);

#define lives_hook_remove(hooks, type, func, data)		\
  _lives_hook_remove((hooks), (type), (hook_funcptr_t)(func), (void *)(data))

void lives_hooks_clear(lives_hook_stack_t **hooks, int type);
void lives_hooks_clear_all(lives_hook_stack_t **hooks, int ntypes);

boolean lives_hooks_trigger(lives_obj_t *, lives_hook_stack_t **hooks, int type);
boolean lives_hooks_triggero(lives_object_t *obj, lives_hook_stack_t **xlist, int type);

void lives_hooks_transfer(lives_hook_stack_t **dest, lives_hook_stack_t **src, boolean include_glob);

void lives_hooks_trigger_async(lives_obj_t *, lives_hook_stack_t **hooks, int type);
void lives_hooks_trigger_asynco(lives_object_t *obj, lives_hook_stack_t **xlist, int type);

lives_proc_thread_t lives_hooks_trigger_async_sequential(lives_obj_t *lpt, lives_hook_stack_t **xlist, int type,
							 hook_funcptr_t finfunc, void *findata);
void lives_hooks_trigger_async_sequentialo(lives_object_t *, lives_hook_stack_t **hooks, int type, hook_funcptr_t finfunc,
    void *findata);

void lives_hooks_join(lives_hook_stack_t *);

lives_hook_stack_t **lives_proc_thread_get_hook_stacks(lives_proc_thread_t);

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
