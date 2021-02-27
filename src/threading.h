// threading.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _THREADING_H_
#define _THREADING_H_

typedef void (*lives_funcptr_t)();

typedef void *(*lives_thread_func_t)(void *);

typedef struct _lives_thread_data_t lives_thread_data_t;

typedef weed_plantptr_t lives_proc_thread_t;

typedef uint64_t funcsig_t;

typedef struct {
  uint64_t var_uid;
  lives_proc_thread_t var_tinfo;
  lives_thread_data_t *var_mydata;
  int var_id;
  boolean var_com_failed;
  int var_write_failed, var_read_failed;
  boolean var_chdir_failed;
  char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file;
  int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data
  int var_rowstride_alignment_hint;
  int var_last_sws_block;
  boolean var_no_gui;
  uint64_t var_random_seed;
  int var_core_id;
  volatile float *var_core_load_ptr; // pointer to value that monitors core load
} lives_threadvars_t;

struct _lives_thread_data_t {
  LiVESWidgetContext *ctx;
  int64_t idx;
  lives_threadvars_t vars;
};

typedef struct {
  lives_thread_func_t func;
  void *arg;
  uint64_t flags;
  volatile uint64_t busy;
  volatile uint64_t done;
  void *ret;
  volatile boolean sync_ready;
} thrd_work_t;

typedef struct {
  int category; // category type for function (0 for none)
  lives_funcptr_t function; // pointer to a function
  char *args_fmt; // definition of the params, e.g. "idV" (int, double, void *)
  uint32_t rettype; // weed_seed type e.g. WEED_SEED_INT, a value of 0 implies a void *(fuunc)
  void *data; // category specific data, may be NULL
} lives_func_info_t;

#define WEED_LEAF_NOTIFY "notify"
#define WEED_LEAF_DONE "done"
#define WEED_LEAF_THREADFUNC "tfunction"
#define WEED_LEAF_THREAD_PROCESSING "t_processing"
#define WEED_LEAF_THREAD_CANCELLABLE "t_can_cancel"
#define WEED_LEAF_THREAD_CANCELLED "t_cancelled"
#define WEED_LEAF_RETURN_VALUE "return_value"
#define WEED_LEAF_DONTCARE "dontcare"  ///< tell proc_thread with return value that we n o longer need return val.
#define WEED_LEAF_DONTCARE_MUTEX "dontcare_mutex" ///< ensures we can set dontcare without it finishing while doing so

#define WEED_LEAF_FUNCSIG "funcsig"

#define WEED_LEAF_SIGNALLED "signalled"
#define WEED_LEAF_SIGNAL_DATA "signal_data"

#define WEED_LEAF_THREAD_PARAM "thrd_param"
#define _WEED_LEAF_THREAD_PARAM(n) WEED_LEAF_THREAD_PARAM  n
#define WEED_LEAF_THREAD_PARAM0 _WEED_LEAF_THREAD_PARAM("0")
#define WEED_LEAF_THREAD_PARAM1 _WEED_LEAF_THREAD_PARAM("1")
#define WEED_LEAF_THREAD_PARAM2 _WEED_LEAF_THREAD_PARAM("2")

#define LIVES_THRDFLAG_AUTODELETE	(1 << 0)
#define LIVES_THRDFLAG_TUNING		(1 << 1)
#define LIVES_THRDFLAG_WAIT_SYNC	(1 << 2)

typedef LiVESList lives_thread_t;
typedef uint64_t lives_thread_attr_t;

#define LIVES_THRDATTR_NONE		0
#define LIVES_THRDATTR_AUTODELETE	(1 << 0)
#define LIVES_THRDATTR_PRIORITY		(1 << 1)
#define LIVES_THRDATTR_WAIT_SYNC	(1 << 2)
#define LIVES_THRDATTR_FG_THREAD	(1 << 3)
#define LIVES_THRDATTR_NO_GUI		(1 << 4)

void lives_threadpool_init(void);
void lives_threadpool_finish(void);
int lives_thread_create(lives_thread_t *thread, lives_thread_attr_t attr, lives_thread_func_t func, void *arg);
uint64_t lives_thread_join(lives_thread_t work, void **retval);

// lives_proc_thread_t //////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _RV_ WEED_LEAF_RETURN_VALUE

typedef int(*funcptr_int_t)();
typedef double(*funcptr_dbl_t)();
typedef int(*funcptr_bool_t)();
typedef char *(*funcptr_string_t)();
typedef int64_t(*funcptr_int64_t)();
typedef weed_funcptr_t(*funcptr_funcptr_t)();
typedef void *(*funcptr_voidptr_t)();
typedef weed_plant_t(*funcptr_plantptr_t)();

#define GETARG(type, n, m) (p##m = WEED_LEAF_GET(info, _WEED_LEAF_THREAD_PARAM(n), type))

#define ARGS1(t1) GETARG(t1, "0", 0)
#define ARGS2(t1, t2) ARGS1(t1), GETARG(t2, "1", 1)
#define ARGS3(t1, t2, t3) ARGS2(t1, t2), GETARG(t3, "2", 2)
#define ARGS4(t1, t2, t3, t4) ARGS3(t1, t2, t3), GETARG(t4, "3", 3)
#define ARGS5(t1, t2, t3, t4, t5) ARGS4(t1, t2, t3, t4), GETARG(t5, "4", 4)
#define ARGS6(t1, t2, t3, t4, t5, t6) ARGS5(t1, t2, t3, t4, t5), GETARG(t6, "5", 5)
#define ARGS7(t1, t2, t3, t4, t5, t6, t7) ARGS6(t1, t2, t3, t4, t5, t6), GETARG(t7, "6", 6)
#define ARGS8(t1, t2, t3, t4, t5, t6, t7, t8) ARGS7(t1, t2, t3, t4, t5, t6, t7), GETARG(t8, "7", 7)
#define ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9) ARGS8(t1, t2, t3, t4, t5, t6, t7. t8), GETARG(t9, "8", 8)
#define ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9), GETARG(t10, "9", 9)

#define CALL_VOID_10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) (*thefunc->func)(ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10))
#define CALL_VOID_9(t1, t2, t3, t4, t5, t6, t7, t8, t9) (*thefunc->func)(ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9))
#define CALL_VOID_8(t1, t2, t3, t4, t5, t6, t7, t8) (*thefunc->func)(ARGS8(t1, t2, t3, t4, t5, t6, t7, t8))
#define CALL_VOID_7(t1, t2, t3, t4, t5, t6, t7) (*thefunc->func)(ARGS7(t1, t2, t3, t4, t5, t6, t7))
#define CALL_VOID_6(t1, t2, t3, t4, t5, t6) (*thefunc->func)(ARGS6(t1, t2, t3, t4, t5, t6))
#define CALL_VOID_5(t1, t2, t3, t4, t5) (*thefunc->func)(ARGS5(t1, t2, t3, t4, t5))
#define CALL_VOID_4(t1, t2, t3, t4) (*thefunc->func)(ARGS4(t1, t2, t3, t4))
#define CALL_VOID_3(t1, t2, t3) (*thefunc->func)(ARGS3(t1, t2, t3))
#define CALL_VOID_2(t1, t2) (*thefunc->func)(ARGS2(t1, t2))
#define CALL_VOID_1(t1) (*thefunc->func)(ARGS1(t1))
#define CALL_VOID_0() (*thefunc->func)()

#define CALL_10(ret, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) weed_set_##ret##_value(info, _RV_, \
										     (*thefunc->func##ret)(ARGS10(t1, t2, t3, t4, t5, t6, t7, t8, t9, t19)))
#define CALL_9(ret, t1, t2, t3, t4, t5, t6, t7, t8, t9) weed_set_##ret##_value(info, _RV_, \
									       (*thefunc->func##ret)(ARGS9(t1, t2, t3, t4, t5, t6, t7, t8, t9)))
#define CALL_8(ret, t1, t2, t3, t4, t5, t6, t7, t8) weed_set_##ret##_value(info, _RV_, \
									   (*thefunc->func##ret)(ARGS8(t1, t2, t3, t4, t5, t6, t7, t7)))
#define CALL_7(ret, t1, t2, t3, t4, t5, t6, t7) weed_set_##ret##_value(info, _RV_, \
								       (*thefunc->func##ret)(ARGS7(t1, t2, t3, t4, t5, t6, t7)))
#define CALL_6(ret, t1, t2, t3, t4, t5, t6) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS6(t1, t2, t3, t4, t5, t6)))
#define CALL_5(ret, t1, t2, t3, t4, t5) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS5(t1, t2, t3, t4, t5)))
#define CALL_4(ret, t1, t2, t3, t4) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS4(t1, t2, t3, t4)))
#define CALL_3(ret, t1, t2, t3) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS3(t1, t2, t3)))
#define CALL_2(ret, t1, t2) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS2(t1, t2)))
#define CALL_1(ret, t1) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)(ARGS1(t1)))
#define CALL_0(ret) weed_set_##ret##_value(info, _RV_, (*thefunc->func##ret)())

#define FUNCSIG_VOID				       			0X00000000
#define FUNCSIG_INT 			       				0X00000001
#define FUNCSIG_DOUBLE 				       			0X00000002
#define FUNCSIG_STRING 				       			0X00000004
#define FUNCSIG_VOIDP 				       			0X0000000D
#define FUNCSIG_PLANTP 				       			0X0000000E
#define FUNCSIG_INT_INT64 			       			0X00000015
#define FUNCSIG_STRING_INT 			      			0X00000041
#define FUNCSIG_STRING_BOOL 			      			0X00000043
#define FUNCSIG_VOIDP_VOIDP 				       		0X000000DD
#define FUNCSIG_VOIDP_STRING 				       		0X000000D4
#define FUNCSIG_VOIDP_DOUBLE 				       		0X000000D2
#define FUNCSIG_PLANTP_BOOL 				       		0X000000E3
// 3p
#define FUNCSIG_VOIDP_DOUBLE_INT 		        		0X00000D21
#define FUNCSIG_VOIDP_VOIDP_VOIDP 		        		0X00000DDD
#define FUNCSIG_BOOL_BOOL_STRING 		        		0X00000334
#define FUNCSIG_PLANTP_VOIDP_INT64 		        		0X00000ED5
// 4p
#define FUNCSIG_STRING_STRING_VOIDP_INT					0X000044D1
#define FUNCSIG_INT_INT_BOOL_VOIDP					0X0000113D
// 5p
#define FUNCSIG_INT_INT_INT_BOOL_VOIDP					0X0001113D
#define FUNCSIG_VOIDP_STRING_STRING_INT64_INT			       	0X000D4451
// 6p
#define FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP		       	0X0044D14D

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

lives_proc_thread_t lives_proc_thread_create(lives_thread_attr_t, lives_funcptr_t,
    int return_type, const char *args_fmt, ...);

lives_proc_thread_t lives_proc_thread_create_vargs(lives_thread_attr_t attr, lives_funcptr_t func,
    int return_type, const char *args_fmt, va_list xargs);

void call_funcsig(lives_proc_thread_t info);

void lives_proc_thread_free(lives_proc_thread_t lpt);

/// returns FALSE while the thread is running, TRUE once it has finished
boolean lives_proc_thread_check(lives_proc_thread_t);
int lives_proc_thread_signalled(lives_proc_thread_t tinfo);
int64_t lives_proc_thread_signalled_idx(lives_proc_thread_t tinfo);

lives_thread_data_t *get_thread_data(void);
lives_threadvars_t *get_threadvars(void);
lives_thread_data_t *lives_thread_data_create(uint64_t idx);

#define THREADVAR(var) (get_threadvars()->var_##var)

/// only threads with no return value can possibly be cancellable. For threads with a value, use
/// lives_proc_thread_dontcare()
void lives_proc_thread_set_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_get_cancellable(lives_proc_thread_t);
boolean lives_proc_thread_cancel(lives_proc_thread_t);
boolean lives_proc_thread_cancelled(lives_proc_thread_t);

/// tell a threead with return value that we no longer need the value so it can free itself
boolean lives_proc_thread_dontcare(lives_proc_thread_t);

void lives_proc_thread_sync_ready(lives_proc_thread_t);

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

void resubmit_proc_thread(lives_proc_thread_t, lives_thread_attr_t);

boolean is_fg_thread(void);

void *fg_run_func(lives_proc_thread_t lpt, void *retval);
void *main_thread_execute(lives_funcptr_t func, int return_type, void *retval, const char *args_fmt, ...);

#endif
