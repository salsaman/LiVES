// functions.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _FUNCSIGS_H
#define _FUNCSIGS_H

typedef int(*funcptr_int_t)();
typedef double(*funcptr_dbl_t)();
typedef int(*funcptr_bool_t)();
typedef char *(*funcptr_string_t)();
typedef int64_t(*funcptr_int64_t)();
typedef weed_funcptr_t(*funcptr_funcptr_t)();
typedef void *(*funcptr_voidptr_t)();
typedef weed_plant_t *(*funcptr_plantptr_t)();

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

#define GETARG(thing, type, n) (p##n = WEED_LEAF_GET((thing), PROC_THREAD_PARAM(n), type))

// since the codification of a param type only requires 4 bits, in theory we could go up to 16 parameters
// however 8 is probably sufficient and looks neater
// it is also possible to pass functions as parameters, using _FUNCP, so things like
// FUNCSIG_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP
// are a possibility

#ifdef WEED_SEED_UINT
#define _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)		\
  case(WEED_SEED_UINT):pre(pre2,pre3##uint##post(post2,post3,post4));break;
#else
#define _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)
#endif
#ifdef WEED_SEED_UINT64
#define _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4)	\
  case(WEED_SEED_UINT64):pre(pre2,pre3##uint64##post(post2,post3,post4));break;
#else
#define _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4)
#endif

#define CTYPE_INT int32_t
#define CTYPE_UINT uint32_t
#define CTYPE_INT64 int64_t
#define CTYPE_UINT64 uint64_t
#define CTYPE_DOUBLE double
#define CTYPE_FLOAT float
#define CTYPE_STRING char *
#define CTYPE_VOIDPTR void *
#define CTYPE_FUNCPTR lives_funcptr_t
#define CTYPE_PLANTPTR weed_plantptr_t

#define CTYPE_int int32_t
#define CPTRTYPE_int int32_t *
#define CTYPE_double double
#define CPTRTYPE_double double *
#define CTYPE_boolean boolean
#define CPTRTYPE_boolean boolean *
#define CTYPE_string char *
#define CPTRTYPE_string char **
#define CTYPE_int64 int64_t
#define CPTRTYPE_int64 int64_t *
#define CTYPE_uint uint32_t
#define CPTRTYPE_uint uint32_t *
#define CTYPE_uint64 uint64_t
#define CPTRTYPE_uint64 uint64_t *
#define CTYPE_float float
#define CPTRTYPE_float float *

#define CTYPE_funcptr lives_funcptr_t
#define CPTRTYPE_funcptr lives_funcptr_t *
#define CTYPE_voidptr void *
#define CPTRTYPE_voidptr void **
#define CTYPE_plantptr weed_plantptr_t
#define CPTRTYPE_plantptr weed_plantptr_t *

#define CTYPE(type) CTYPE_##type
#define CPTRTYPE(type) CPTRTYPE_##type
#define WEED_TYPE(type) WEED_SEED_##type

#define FOR_ALL_SEED_TYPES(var, pre, pre2, pre3, post, post2, post3, post4) \
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

#define GEN_SET(thing, wret, funcname, FUNCARGS) err =			\
    (wret == WEED_SEED_INT ? weed_set_int_value((thing), _RV_, (*(funcname)->funcint)(FUNCARGS)) : \
     wret == WEED_SEED_DOUBLE ? weed_set_double_value((thing), _RV_, (*(funcname)->funcdouble)(FUNCARGS)) : \
     wret == WEED_SEED_BOOLEAN ? weed_set_boolean_value((thing), _RV_, (*(funcname)->funcboolean)(FUNCARGS)) : \
     wret == WEED_SEED_STRING ? weed_set_string_value((thing), _RV_, (*(funcname)->funcstring)(FUNCARGS)) : \
     wret == WEED_SEED_INT64 ? weed_set_int64_value((thing), _RV_, (*(funcname)->funcint64)(FUNCARGS)) : \
     wret == WEED_SEED_FUNCPTR ? weed_set_funcptr_value((thing), _RV_, (*(funcname)->funcfuncptr)(FUNCARGS)) : \
     wret == WEED_SEED_VOIDPTR ? weed_set_voidptr_value((thing), _RV_, (*(funcname)->funcvoidptr)(FUNCARGS)) : \
     wret == WEED_SEED_PLANTPTR ? weed_set_plantptr_value((thing), _RV_, (*(funcname)->funcplantptr)(FUNCARGS)) : \
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
#define CALL_VOID_8(thing, funcname, ...) (*(funcname)->func)(ARGS8((thing), __VA_ARGS__))
#define CALL_VOID_7(thing, funcname, ...) (*(funcname)->func)(ARGS7((thing), __VA_ARGS__))
#define CALL_VOID_6(thing, funcname, ...) (*(funcname)->func)(ARGS6((thing), __VA_ARGS__))
#define CALL_VOID_5(thing, funcname, ...) (*(funcname)->func)(ARGS5((thing), __VA_ARGS__))
#define CALL_VOID_4(thing, funcname, ...) (*(funcname)->func)(ARGS4((thing), __VA_ARGS__))
#define CALL_VOID_3(thing, funcname, ...) (*(funcname)->func)(ARGS3((thing), __VA_ARGS__))
#define CALL_VOID_2(thing, funcname, ...) (*(funcname)->func)(ARGS2((thing), __VA_ARGS__))
#define CALL_VOID_1(thing, funcname, ...) (*(funcname)->func)(ARGS1((thing), __VA_ARGS__))
#define CALL_VOID_0(thing, funcname, dummy) (*(funcname)->func)()
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
#define XCALL_2(thing, wret, funcname, t1, t2)			\
  GEN_SET(thing, wret, funcname, _ARGS(1, (thing), t2, t1))
#define XCALL_1(thing, wret, funcname, t1)		\
  GEN_SET(thing, wret, funcname, ARGS1((thing), t1))
#define XCALL_0(thing, wret, funcname, dummy)	\
  GEN_SET(thing, wret, funcname, )
#if FIX_INDENT_IGNORE_THIS
}
#endif

#define FUNCSIG_VOID				       			0

#define FUNCSIG_INT 			       				1
#define FUNCSIG_DOUBLE 				       			2
#define FUNCSIG_BOOL 				       			3
#define FUNCSIG_STRING 				       			4
#define FUNCSIG_INT64 			       				5
#define FUNCSIG_FUNCP 				       			C
#define FUNCSIG_VOIDP 				       			D
#define FUNCSIG_PLANTP 				       			E

#define _JOIN2(a,b) a##b
#define JOIN2(a,b) _JOIN2(a,b)

#define FUNCSIG1(a) FUNCSIG_##a
#define FUNCSIG2(a,b) JOIN2(FUNCSIG_##a,FUNCSIG_##b)
#define FUNCSIG3(a,b,c) JOIN2(FUNCSIG_##a,FUNCSIG2(b,c))
#define FUNCSIG4(a,b,c,d) JOIN2(FUNCSIG_##a,FUNCSIG3(b,c,d))
#define FUNCSIG5(a,b,c,d,e) JOIN2(FUNCSIG_##a,FUNCSIG4(b,c,d,e))
#define FUNCSIG6(a,b,c,d,e,f) JOIN2(FUNCSIG_##a,FUNCSIG5(b,c,d,e,f))
#define FUNCSIG7(a,b,c,d,e,f,g) JOIN2(FUNCSIG_##a,FUNCSIG6(b,c,d,e,f,g))
#define FUNCSIG8(a,b,c,d,e,f,g,h) JOIN2(FUNCSIG_##a,FUNCSIG7(b,c,d,e,f,g,h))
#if FIX_INDENT_IGNORE_THIS
}
#endif

#define MAKE_HEX(a) JOIN2(0X,a)
#define FUNCSIG(n,...) MAKE_HEX(FUNCSIG##n(__VA_ARGS__))

#define DEF_VAR_INT(n) int p##n;
#define DEF_VAR_BOOL(n) boolean p##n;
#define DEF_VAR_DOUBLE(n) double p##n;
#define DEF_VAR_STRING(n) char *p##n = NULL;
#define DEF_VAR_INT64(n) int64_t p##n;
#define DEF_VAR_VOIDP(n) void *p##n;
#define DEF_VAR_PLANTP(n) weed_plantptr_t p##n;
#define DEF_VAR_FUNCP(n) lives_funcptr_t p##n;

#define DEF_VAR(a,b) DEF_VAR_##a(b)
#define DEF_VARS1(a) DEF_VAR(a, 0)
#define DEF_VARS2(a,b) DEF_VARS1(a) DEF_VAR(b, 1)
#define DEF_VARS3(a,b,c) DEF_VARS2(a,b) DEF_VAR(c, 2)
#define DEF_VARS4(a,b,c,d) DEF_VARS3(a,b,c) DEF_VAR(d, 3)
#define DEF_VARS5(a,b,c,d,e) DEF_VARS4(a,b,c,d) DEF_VAR(e, 4)
#define DEF_VARS6(a,b,c,d,e,f) DEF_VARS5(a,b,c,d,e) DEF_VAR(f, 5)
#define DEF_VARS7(a,b,c,d,e,f,g) DEF_VARS6(a,b,c,d,e,f) DEF_VAR(g, 6)
#define DEF_VARS8(a,b,c,d,e,f,g,h) DEF_VARS7(a,b,c,d,e,f,g) DEF_VAR(h, 7)
#if FIX_INDENT_IGNORE_THIS
}
#endif

#define DEF_VARS(n,...) DEF_VARS##n(__VA_ARGS__)

#define GET_CTYPE_INT int
#define GET_CTYPE_BOOL boolean
#define GET_CTYPE_DOUBLE double
#define GET_CTYPE_STRING string
#define GET_CTYPE_INT64 int64
#define GET_CTYPE_VOIDP voidptr
#define GET_CTYPE_PLANTP plantptr
#define GET_CTYPE_FUNCP funcptr

#define GET_CTYPE(a) GET_CTYPE_##a
#define GET_CTYPES1(a) GET_CTYPE(a)
#define GET_CTYPES2(a,b) GET_CTYPE(a), GET_CTYPE(b)
#define GET_CTYPES3(a,b,c) GET_CTYPES2(a,b), GET_CTYPE(c)
#define GET_CTYPES4(a,b,c,d) GET_CTYPES3(a,b,c), GET_CTYPE(d)
#define GET_CTYPES5(a,b,c,d,e) GET_CTYPES4(a,b,c,d), GET_CTYPE(e)
#define GET_CTYPES6(a,b,c,d,e,f) GET_CTYPES5(a,b,c,d,e), GET_CTYPE(f)
#define GET_CTYPES7(a,b,c,d,e,f,g) GET_CTYPES6(a,b,c,d,e,f), GET_CTYPE(g)
#define GET_CTYPES8(a,b,c,d,e,f,g,h) GET_CTYPES7(a,b,c,d,e,f,g), GET_CTYPE(h)

#define GET_CTYPES(n,...) GET_CTYPES##n(__VA_ARGS__)

#define FREE_CHARPTR_INT(n)
#define FREE_CHARPTR_BOOL(n)
#define FREE_CHARPTR_DOUBLE(n)
#define FREE_CHARPTR_STRING(n) _IF_(p##n);
#define FREE_CHARPTR_INT64(n)
#define FREE_CHARPTR_VOIDP(n)
#define FREE_CHARPTR_PLANTP(n)
#define FREE_CHARPTR_FUNCP(n)

#define FREE_CHARPTR(a,n) FREE_CHARPTR_##a(n)
#define FREE_CHARPTRS1(a) FREE_CHARPTR(a,0)
#define FREE_CHARPTRS2(a,b) FREE_CHARPTRS1(a) FREE_CHARPTR(b,1)
#define FREE_CHARPTRS3(a,b,c) FREE_CHARPTRS2(a,b) FREE_CHARPTR(c,2)
#define FREE_CHARPTRS4(a,b,c,d) FREE_CHARPTRS3(a,b,c) FREE_CHARPTR(d,3)
#define FREE_CHARPTRS5(a,b,c,d,e) FREE_CHARPTRS4(a,b,c,d) FREE_CHARPTR(e,4)
#define FREE_CHARPTRS6(a,b,c,d,e,f) FREE_CHARPTRS5(a,b,c,d,e) FREE_CHARPTR(f,5)
#define FREE_CHARPTRS7(a,b,c,d,e,f,g) FREE_CHARPTRS6(a,b,c,d,e,f) FREE_CHARPTR(g,6)
#define FREE_CHARPTRS8(a,b,c,d,e,f,g,h) FREE_CHARPTRS7(a,b,c,d,e,f,g) FREE_CHARPTR(h,7)
#if FIX_INDENT_IGNORE_THIS
}
#endif

#define FREE_CHARPTRS(n,...) FREE_CHARPTRS##n(__VA_ARGS__)

#define ADD_FUNCSIG(n,...) case FUNCSIG(n,__VA_ARGS__): {DEF_VARS(n,__VA_ARGS__) \
      _DC_(n,GET_CTYPES(n,__VA_ARGS__));FREE_CHARPTRS(n,__VA_ARGS__)} break;

typedef uint64_t funcsig_t;

// to make a function callable as a proc_thread or a hook callback, it is only necessary to add the funcsig here

#define ONE_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(1,INT);				\
  ADD_FUNCSIG(1,BOOL)				\
  ADD_FUNCSIG(1,INT64)				\
  ADD_FUNCSIG(1,DOUBLE)				\
  ADD_FUNCSIG(1,STRING)				\
  ADD_FUNCSIG(1,VOIDP)				\
  ADD_FUNCSIG(1,PLANTP)

#define TWO_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(2,INT,INT)			\
  ADD_FUNCSIG(2,BOOL,INT)			\
  ADD_FUNCSIG(2,INT,VOIDP)			\
  ADD_FUNCSIG(2,STRING,INT)			\
  ADD_FUNCSIG(2,STRING,BOOL)			\
  ADD_FUNCSIG(2,DOUBLE,DOUBLE)			\
  ADD_FUNCSIG(2,VOIDP,DOUBLE)			\
  ADD_FUNCSIG(2,VOIDP,INT)			\
  ADD_FUNCSIG(2,VOIDP,INT64)			\
  ADD_FUNCSIG(2,VOIDP,VOIDP)			\
  ADD_FUNCSIG(2,PLANTP,VOIDP)			\
  ADD_FUNCSIG(2,VOIDP,BOOL)			\
  ADD_FUNCSIG(2,VOIDP,STRING)

#define THREE_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(3,VOIDP,VOIDP,VOIDP)		\
  ADD_FUNCSIG(3,VOIDP,VOIDP,BOOL)		\
  ADD_FUNCSIG(3,STRING,VOIDP,VOIDP)		\
  ADD_FUNCSIG(3,VOIDP,DOUBLE,INT)		\
  ADD_FUNCSIG(3,VOIDP,INT,INT)		\
  ADD_FUNCSIG(3,VOIDP,DOUBLE,DOUBLE)		\
  ADD_FUNCSIG(3,PLANTP,VOIDP,INT64)		\
  ADD_FUNCSIG(3,PLANTP,INT64,BOOL)		\
  ADD_FUNCSIG(3,INT,INT,BOOL)			\
  ADD_FUNCSIG(3,BOOL,INT,BOOL)			\
  ADD_FUNCSIG(3,STRING,INT,BOOL)		\
  ADD_FUNCSIG(3,INT,INT64,VOIDP)

#define FOUR_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(4,STRING,DOUBLE,INT,STRING)	\
  ADD_FUNCSIG(4,INT,INT,BOOL,VOIDP)		\
  ADD_FUNCSIG(4,VOIDP,INT,FUNCP,VOIDP)

#define FIVE_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(5,VOIDP,STRING,STRING,INT64,INT)	\
  ADD_FUNCSIG(5,INT,INT,INT,BOOL,VOIDP)		\
  ADD_FUNCSIG(5,VOIDP,INT,INT,INT,INT)		\
  ADD_FUNCSIG(5,VOIDP,VOIDP,BOOL,BOOL,INT)

#define SIX_PARAM_FUNCSIGS				\
  ADD_FUNCSIG(6,STRING,STRING,VOIDP,INT,STRING,VOIDP)

#define SEVEN_PARAM_FUNCSIGS

#define EIGHT_PARAM_FUNCSIGS

#endif
