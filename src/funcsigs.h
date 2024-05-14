// funcsigsns.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#if !defined(_FUNCSIGS_H) || defined(NEED_FSIG_CASES)
#ifndef NEED_FSIG_CASES
#define _FUNCSIGS_H
#endif

#ifndef I_KNOW_WHAT_FUNCSIG_IS
#define I_KNOW_WHAT_FUNCSIG_IS
typedef uint64_t funcsig_t;
#endif

#ifndef NEED_FSIG_CASES

#ifdef ADD_STRUCT_DTLS

#define DEF_STRUCT(stname, ...)				\
  const char *stname##_strctdef = #__VA_ARGS__;		\
  typedef struct _##stname {__VA_ARGS__} stname;	\
  const size_t stname##_size = sizeof(stname);

// same as struct, except all offsets are 0
#define DEF_UNION(uniname, ...)				\
  const char *uniname##_unidef = #__VA_ARGS__;		\
  typedef union _##uniname {__VA_ARGS__} uniname;	\
  const size_t uniname##_size = sizeof(uniname);

#define DEF_ENUM(enumtype, ...)				\
  const char *enumtype##_enumdef = #__VA_ARGS__;	\
  typedef enum {__VA_ARGS__} enumtype;

#else

#define DEF_STRUCT(stname, ...)				\
  typedef struct _##stname {__VA_ARGS__} stname;

// same as struct, except all offsets are 0
#define DEF_UNION(uniname, ...)				\
  typedef union _##uniname {__VA_ARGS__} uniname;

#define DEF_ENUM(enumtype, ...)			\
  typedef enum {__VA_ARGS__} enumtype;

#endif

typedef int(*funcptr_int_t)();
typedef double(*funcptr_dbl_t)();
typedef int(*funcptr_bool_t)();
typedef char *(*funcptr_string_t)();
typedef int64_t(*funcptr_int64_t)();
typedef weed_funcptr_t(*funcptr_funcptr_t)();
typedef void *(*funcptr_voidptr_t)();
typedef weed_plant_t *(*funcptr_plantptr_t)();

DEF_UNION(allfunc_t,
          weed_funcptr_t func;
          funcptr_int_t funcint;
          funcptr_dbl_t funcdouble;
          funcptr_bool_t funcboolean;
          funcptr_int64_t funcint64;
          funcptr_string_t funcstring;
          funcptr_funcptr_t funcfuncptr;
          funcptr_voidptr_t funcvoidptr;
          funcptr_plantptr_t funcplantptr;)

DEF_UNION(allval_t,
          // ptr to whichever value.type is active. eample with WEED_SEED_INT
          // if ne == 0, NULL; if ne == 1, &i[0]. if bound, &values.i
          // if array, also &i
          char *data;
          uint32_t *u; int32_t *i;
          uint64_t *U; int64_t *I; boolean *b;
          char **S; char **s; const char **C;
          float *f; double *d; void **V;
          weed_funcptr_t *F; weed_plant_t **P;)

// values->* is a pointer to type rather than array of type
#define ALLV_FLAG_POINTER		(1ull << 0)
#define ALLV_FLAG_RDWRITE		(1ull << 1)
#define ALLV_FLAG_TYPE_ONLY		(1ull << 2)
#define ALLV_FLAG_FREE_VALUE		(1ull << 3)
#define ALLV_FLAG_RWLOCK		(1ull << 4)
#define ALLV_FLAG_EXTERN		(1ull << 5)

// error flagbits
// unrecognised seed_type when setting val
#define ALLV_ERR_STYPE		(1ull << 32)
// attempt to bind to array (only scalars can be bound)
#define ALLV_ERR_NVALS		(1ull << 33)

DEF_STRUCT(allvalues_t,
           //@TYPEDEF u weed_seed_t
           //@TYPEDEF u weed_size_t
           //@TYPEDEF v LiVESList *
           //@UNION allval_t
           // text of value passed on creation, e.g. "2", "WEED_SEED_BOOLEAN"
           // or func name fpr fncinst
           char *aname;
           weed_seed_t stype;
           weed_size_t ne; // num elements - always 1 if POINTER set
           weed_size_t size;
           uint64_t flags;
#ifdef NATIVE_RWLOCK_TYPE
           NATIVE_RWLOCK_TYPE *rwlock;
#endif
           allval_t values;
           char *ext_typename;
           lives_funcinst_t *funcinst;
           LiVESList *contingencies;
           void *priv_data;)

#define ALLV_FROM_LEAF(avp, plant, key, st, ne) _DW0(st = weed_leaf_seed_type(plant, key); \
						     FOR_ALL_SEED_TYPES2(st, (avp)->values., =, weed_get_, \
									 _array_counted, (plant), (key), &(ne));)

#define LEAF_FROM_ALLV(plant, key, allv)				\
  _DW0(									\
       weed_leaf_set(plant, key, allv->stype, allv->ne,			\
		     (allv->stype == WEED_SEED_INT ? (void *)allv->values.i \
		      : allv->stype == WEED_SEED_UINT ? (void *)allv->values.u \
		      : allv->stype == WEED_SEED_BOOLEAN ? (void *)allv->values.b \
		      : allv->stype == WEED_SEED_INT64 ? (void *)allv->values.I \
		      : allv->stype == WEED_SEED_UINT64 ? (void *)allv->values.U \
		      : allv->stype == WEED_SEED_DOUBLE ? (void *)allv->values.d \
		      : allv->stype == WEED_SEED_FLOAT ? (void *)allv->values.f \
		      : allv->stype == WEED_SEED_STRING ? (void *)allv->values.s \
		      : (allv->stype == WEED_SEED_VOIDPTR || WEED_SEED_IS_CUSTOM(allv->stype)) \
		      ? (void *)allv->values.V				\
		      : allv->stype == WEED_SEED_FUNCPTR ? (void *)allv->values.F \
		      : allv->stype == WEED_SEED_PLANTPTR ? (void *)allv->values.P : NULL));)

// since the codification of a param type only requires 4 bits, in theory we could go up to 16 parameters
// however 8 is probably sufficient and looks neater
// it is also possible to pass functions as parameters, using _FUNCP, so things like
// FUNCSIG_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP_FUNCP
// are a possibility

#ifdef WEED_SEED_UINT
#define _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)		\
  case(WEED_SEED_UINT):pre(pre2,(uint32_t)pre3##uint##post(post2,post3,post4));break;
#define _CASE2_UINT(pre, op, pre2, post, post2, post3, post4)		\
  case(WEED_SEED_UINT):pre u op (uint32_t *)pre2##uint##post(post2,post3,post4);break;
#else
#define _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4)
#define _CASE2_UINT(pre, op, pre2, post, post2, post3, post4)
#endif
#ifdef WEED_SEED_UINT64
#define _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4)	\
  case(WEED_SEED_UINT64):pre(pre2,(uint64_t)pre3##uint64##post(post2,post3,post4));break;
#define _CASE2_UINT64(pre, op, pre2, post, post2, post3, post4)		\
  case(WEED_SEED_UINT64):pre U op (uint64_t *)pre2##uint64##post(post2,post3,post4);break;
#else
#define _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4)
#define _CASE2_UINT64(pre, op, pre2, post, post2, post3, post4)
#endif
#ifdef WEED_SEED_FLOAT
#define _CASE_FLOAT(pre, pre2, pre3, post, post2, post3, post4) \
  case(WEED_SEED_FLOAT):pre(pre2,(float)pre3##double##post(post2,post3,post4));break;
#define _CASE2_FLOAT(pre, op, pre2, post, post2, post3, post4)	\
  case(WEED_SEED_FLOAT):pre f op (float *)pre2##double##post(post2,post3,post4);break;
#else
#define _CASE_FLOAT(pre, pre2, pre3, post, post2, post3, post4)
#define _CASE2_FLOAT(pre, op, pre2, post, post2, post3, post4)
#endif

#define CTYPE_INT int32_t
#define CTYPE_UINT uint32_t
#define CTYPE_INT64 int64_t
#define CTYPE_UINT64 uint64_t
#define CTYPE_DOUBLE double
#define CTYPE_FLOAT float
#define CTYPE_STRING char *
#define CTYPE_VOIDPTR void *
#define CTYPE_FUNCPTR weed_funcptr_t
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

#define CTYPE_funcptr weed_funcptr_t
#define CPTRTYPE_funcptr weed_funcptr_t *
#define CTYPE_voidptr void *
#define CPTRTYPE_voidptr void **
#define CTYPE_plantptr weed_plantptr_t
#define CPTRTYPE_plantptr weed_plantptr_t *

#define CTYPE(type) CTYPE_##type
#define CPTRTYPE(type) CPTRTYPE_##type
#define WEED_TYPE(type) WEED_SEED_##type

// custom seed types, match VOIDP, passed as custom
// can be used in internal args_fmt
#define LIVES_SEED_ALLVALUES 2048
#define LIVES_SEED_CONST_CHARPTR 2049

// seed types which can be wrapped in allvalues_t->values.V
#define LIVES_SEED_FUNCINST 2050
#define LIVES_SEED_BLOB_DATA 2051

// syntactic marker for variadic functions, must be final in funcsig / args_fmt
// not passed in func calls
#define LIVES_SEED_VARIADIC 5192

// va_list with declared args_fmt in params, values are read and spliced in
// aas replacements
#define LIVES_SEED_VALIST 5193

#define FOR_ALL_SEED_TYPES(st, pre, pre2, pre3, post, post2, post3, post4) \
  _DW0(switch(st){case(WEED_SEED_INT):pre(pre2,pre3##int##post(post2,post3,post4));break; \
    case(WEED_SEED_INT64):pre(pre2,pre3##int64##post(post2,post3,post4));break;	\
    case(WEED_SEED_BOOLEAN):pre(pre2,pre3##boolean##post(post2,post3,post4));break; \
    case(WEED_SEED_DOUBLE):pre(pre2,pre3##double##post(post2,post3,post4));break; \
    case(WEED_SEED_STRING):pre(pre2,pre3##string##post(post2,post3,post4));break; \
    case(WEED_SEED_VOIDPTR):pre(pre2,pre3##voidptr##post(post2,post3,post4));break; \
    case(WEED_SEED_FUNCPTR):pre(pre2,pre3##funcptr##post(post2,post3,post4));break; \
    case(WEED_SEED_PLANTPTR):pre(pre2,pre3##plantptr##post(post2,post3,post4));break; \
      _CASE_UINT(pre, pre2, pre3, post, post2, post3, post4);		\
      _CASE_UINT64(pre, pre2, pre3, post, post2, post3, post4);		\
      _CASE_FLOAT(pre, pre2, pre3, post, post2, post3, post4);		\
    default:if(WEED_SEED_IS_CUSTOM(st))pre(pre2,pre3##custom##post(post2,post3,st,post4));break;})

#define FOR_ALL_SEED_TYPES2(st, pre, op, pre2, post, post2, post3, post4) \
  _DW0(switch(st){case(WEED_SEED_INT):pre i op pre2##int##post(post2,post3,post4);break; \
  case(WEED_SEED_INT64):pre I op pre2##int64##post(post2,post3,post4);break; \
    case(WEED_SEED_BOOLEAN):pre b op pre2##boolean##post(post2,post3,post4);break; \
    case(WEED_SEED_DOUBLE):pre d op pre2##double##post(post2,post3,post4);break; \
    case(WEED_SEED_STRING):pre s op pre2##string##post(post2,post3,post4);break; \
    case(WEED_SEED_VOIDPTR):pre V op pre2##voidptr##post(post2,post3,post4);break; \
    case(WEED_SEED_FUNCPTR):pre F op pre2##funcptr##post(post2,post3,post4);break; \
    case(WEED_SEED_PLANTPTR):pre P op pre2##plantptr##post(post2,post3,post4);break; \
      _CASE2_UINT(pre, op, pre2, post, post2, post3, post4);		\
      _CASE2_UINT64(pre, op, pre2, post, post2, post3, post4);		\
      _CASE2_FLOAT(pre, op, pre2, post, post2, post3, post4);		\
    default:if(WEED_SEED_IS_CUSTOM(st))pre V op pre2##custom##post(post2,post3,st,post4);break;})

#define FOR_ALL_SEED_TYPES3(hdr, st, pre, post)	\
  hdr						\
  (st == WEED_SEED_INT ? pre i post		\
    (st == WEED_SEED_BOOLEAN ? pre b post	\
     :st == WEED_SEED_INT64 ? pre I post	\
     :st == WEED_SEED_DOUBLE ? pre d post	\
     :st == WEED_SEED_STRING ? pre s post	\
     :st == WEED_SEED_FUNCPTR ? pre F post	\
     :st == WEED_SEED_PLANTPTR ? pre P post	\
     :pre V post)

#define get_allv_for_st(var, avp, st, idx) FOR_ALL_SEED_TYPES3(var =, st,(avp)->values., [(idx)])

#define GEN_SET(thing, wret, funcname, ...) err =			\
    (wret == WEED_SEED_INT ? weed_set_int_value((thing), _RV_, (*(funcname)->funcint)(__VA_ARGS__)) : \
     wret == WEED_SEED_DOUBLE ? weed_set_double_value((thing), _RV_, (*(funcname)->funcdouble)(__VA_ARGS__)) : \
     wret == WEED_SEED_BOOLEAN ? weed_set_boolean_value((thing), _RV_, (*(funcname)->funcboolean)(__VA_ARGS__)) : \
     wret == WEED_SEED_STRING ? weed_set_string_value((thing), _RV_, (*(funcname)->funcstring)(__VA_ARGS__)) : \
     wret == WEED_SEED_INT64 ? weed_set_int64_value((thing), _RV_, (*(funcname)->funcint64)(__VA_ARGS__)) : \
     wret == WEED_SEED_FUNCPTR ? weed_set_funcptr_value((thing), _RV_, (*(funcname)->funcfuncptr)(__VA_ARGS__)) : \
     wret == WEED_SEED_VOIDPTR ? weed_set_voidptr_value((thing), _RV_, (*(funcname)->funcvoidptr)(__VA_ARGS__)) : \
     wret == WEED_SEED_PLANTPTR ? weed_set_plantptr_value((thing), _RV_, (*(funcname)->funcplantptr)(__VA_ARGS__)) : \
     WEED_SEED_IS_CUSTOM(wret) ? weed_set_custom_value((thing), _RV_, wret, (*(funcname)->funcvoidptr)(__VA_ARGS__)) : \
     WEED_ERROR_WRONG_SEED_TYPE)

#define CALL_VOID_8(thing, funcname) (*(funcname)->func)(p0,p1,p2,p3,p4,p5,p6,p7)
#define CALL_VOID_7(thing, funcname) (*(funcname)->func)(p0,p1,p2,p3,p4,p5,p6)
#define CALL_VOID_6(thing, funcname) (*(funcname)->func)(p0,p1,p2,p3,p4,p5)
#define CALL_VOID_5(thing, funcname) (*(funcname)->func)(p0,p1,p2,p3,p4)
#define CALL_VOID_4(thing, funcname) (*(funcname)->func)(p0,p1,p2,p3)
#define CALL_VOID_3(thing, funcname) (*(funcname)->func)(p0,p1,p2)
#define CALL_VOID_2(thing, funcname) (*(funcname)->func)(p0,p1)
#define CALL_VOID_1(thing, funcname) (*(funcname)->func)(p0)
#define CALL_VOID_0(thing, funcname) (*(funcname)->func)()

#define XCALL_8(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1,p2,p3,p4,p5,p6,p7)
#define XCALL_7(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1,p2,p3,p4,p5,p6)
#define XCALL_6(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1,p2,p3,p4,p5)
#define XCALL_5(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1,p2,p3,p4)
#define XCALL_4(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1,p2,p3)
#define XCALL_3(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1,p2)
#define XCALL_2(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0,p1)
#define XCALL_1(thing, wret, funcname) GEN_SET(thing, wret, funcname, p0)
#define XCALL_0(thing, wret, funcname) GEN_SET(thing, wret, funcname)
#if FIX_INDENT_IGNORE_THIS
}
#endif

#define FUNCSIG_VOID				       			0

#define FUNCSIG_INT 			       				1
#define FUNCSIG_DOUBLE 				       			2
#define FUNCSIG_BOOL 				       			3
#define FUNCSIG_STRING 				       			4
#define FUNCSIG_INT64 			       				5
// 6,7,8 reserved for uint, uint64, float

#define FUNCSIG_FUNCP 				       			C
#define FUNCSIG_VOIDP 				       			D
#define FUNCSIG_PLANTP 				       			E

#define FUNCSIG_VARIADIC 				       		F

#define _JOIN2(a,b) a##b
#define JOIN2(a,b) _JOIN2(a,b)

#define _FUNCSIG0(...) FUNCSIG_VOID
#define _FUNCSIG1(a) FUNCSIG_##a
#define _FUNCSIG2(a,b) JOIN2(FUNCSIG_##a,FUNCSIG_##b)
#define _FUNCSIG3(a,b,c) JOIN2(FUNCSIG_##a,_FUNCSIG2(b,c))
#define _FUNCSIG4(a,b,c,d) JOIN2(FUNCSIG_##a,_FUNCSIG3(b,c,d))
#define _FUNCSIG5(a,b,c,d,e) JOIN2(FUNCSIG_##a,_FUNCSIG4(b,c,d,e))
#define _FUNCSIG6(a,b,c,d,e,f) JOIN2(FUNCSIG_##a,_FUNCSIG5(b,c,d,e,f))
#define _FUNCSIG7(a,b,c,d,e,f,g) JOIN2(FUNCSIG_##a,_FUNCSIG6(b,c,d,e,f,g))
#define _FUNCSIG8(a,b,c,d,e,f,g,h) JOIN2(FUNCSIG_##a,_FUNCSIG7(b,c,d,e,f,g,h))
#if FIX_INDENT_IGNORE_THIS
}
#endif

#define MAKE_HEX(a) JOIN2(0X,a)
#define _FUNCSIG(n,...) MAKE_HEX(_FUNCSIG##n(__VA_ARGS__))
#define FUNCSIG(n) MAKE_HEX(FUNCSIG_##n)

#define DEF_VAR_INT(thing,n) int p##n;
#define DEF_VAR_BOOL(thing,n) boolean p##n;
#define DEF_VAR_DOUBLE(thing,n) double p##n;
#define DEF_VAR_STRING(thing,n) char *p##n = \
    lives_calloc(weed_leaf_element_size((thing),PROC_THREAD_PARAM(n),0)+1,1);
#define DEF_VAR_INT64(thing,n) int64_t p##n;
#define DEF_VAR_VOIDP(thing,n) void *p##n;
#define DEF_VAR_PLANTP(thing,n) weed_plantptr_t p##n;
#define DEF_VAR_FUNCP(thing,n) weed_funcptr_t p##n;

#define DEF_VAR(thing,a,b) DEF_VAR_##a(thing,b)
#define DEF_VARS0(thing,X)
#define DEF_VARS1(thing,a) DEF_VAR(thing,a, 0)
#define DEF_VARS2(thing,a,b) DEF_VARS1(thing,a) DEF_VAR(thing,b, 1)
#define DEF_VARS3(thing,a,b,c) DEF_VARS2(thing,a,b) DEF_VAR(thing,c, 2)
#define DEF_VARS4(thing,a,b,c,d) DEF_VARS3(thing,a,b,c) DEF_VAR(thing,d, 3)
#define DEF_VARS5(thing,a,b,c,d,e) DEF_VARS4(thing,a,b,c,d) DEF_VAR(thing,e, 4)
#define DEF_VARS6(thing,a,b,c,d,e,f) DEF_VARS5(thing,a,b,c,d,e) DEF_VAR(thing,f, 5)
#define DEF_VARS7(thing,a,b,c,d,e,f,g) DEF_VARS6(thing,a,b,c,d,e,f) DEF_VAR(thing,g, 6)
#define DEF_VARS8(thing,a,b,c,d,e,f,g,h) DEF_VARS7(thing,a,b,c,d,e,f,g) DEF_VAR(thing,h, 7)
#if FIX_INDENT_IGNORE_THIS
}
#endif

// real values
#define ARGS_FMT_INT		'i'
#define ARGS_FMT_DOUBLE		'd'
#define ARGS_FMT_BOOLEAN	'b'
#define ARGS_FMT_INT64		'I'
#define ARGS_FMT_STRING		's'
#define ARGS_FMT_STRING_ALT	'S'
#define ARGS_FMT_VOIDPTR	'V'
#define ARGS_FMT_VOIDPTR_ALT	'v'
#define ARGS_FMT_FUNCPTR	'F'
#define ARGS_FMT_PLANTPTR	'P'
#define ARGS_FMT_PLANTPTR_ALT	'p'

// replaced values - become 'V' when filtered
#define ARGS_FMT_CONST_CHARPTR	'$'
#define ARGS_FMT_ALLVALUES	'A'
#define ARGS_FMT_BLOB_DATA	'B'

// extended values
#define ARGS_FMT_GAP		'_'
#define ARGS_FMT_VARIADIC	'*'
#define ARGS_FMT_UNKNOWN	'?'

typedef struct {
  char typeletter;
  weed_seed_t seed_btype;
  uint8_t sigbits;
  const char *symname;
  const char *fmtstr;
} lookup_tab;

extern const lookup_tab crossrefs[];

#if HAVE_WEED_SEED_UINT
#define ARGS_FMT_UINT		'u'
#define XREFS_TAB_UINT 		,{ARGS_FMT_UINT,  WEED_SEED_UINT, 0x06, "UINT", "%u"}
#else
#define XREFS_FMT_UINT		'?'
#define XREFS_TAB_UINT
#endif
#if HAVE_WEED_SEED_UINT64
#define ARGS_FMT_UINT64		'U'
#define XREFS_TAB_UINT64  	,{ARGS_FMT_UINT64,  WEED_SEED_UINT64, 0x07, "UINT64", "%"PRIu64}
#else
#define ARGS_FMT_UINT64		'?'
#define XREFS_TAB_UINT64
#endif
#if HAVE_WEED_SEED_FLOAT
#define ARGS_FMT_FLOAT		'f'
#define XREFS_TAB_FLOAT  	,{ARGS_FMT_FLOAT,  WEED_SEED_FLOAT,  0x08, "FLOAT", "%.4f"}
#else
#define ARGS_FMT_FLOAT		'?'
#define XREFS_TAB_FLOAT
#endif

#define _ARGS_FMT_SYNTH (int)ARGS_FMT_GAP

#define _ARGS_FMT_REAL (int)ARGS_FMT_INT, (int)ARGS_FMT_DOUBLE, (int)ARGS_FMT_BOOLEAN, \
    (int)ARGS_FMT_INT64, (int)ARGS_FMT_STRING, (int)ARGS_FMT_STRING_ALT, \
    (int)ARGS_FMT_VOIDPTR, (int)ARGS_FMT_VOIDPTR_ALT, (int)ARGS_FMT_FUNCPTR, \
    (int)ARGS_FMT_PLANTPTR, (int)ARGS_FMT_PLANTPTR_ALT, (int)ARGS_FMT_FLOAT, \
    (int)ARGS_FMT_UINT, (int)ARGS_FMT_UINT64

#define ARGS_FMT_REAL _ARGS_FMT_REAL, 0

#define ARGS_FMT_ALLOWED _ARGS_FMT_REAL, _ARGS_FMT_SYNTH, 0

#define _ARGS_FMT_REPLACE (int)ARGS_FMT_ALLVALUES, (int)ARGS_FMT_CONST_CHARPTR
#define ARGS_FMT_REPLACE _ARGS_FMT_REPLACE, 0

// args_fmt letter (char), seed_type (uint32), funcsig value (4 bits), short name, prinf fmt
// LIVES_SEED_* types aew only for convenience and cannot be used in actual function calls (yet)
#define XREFS_TAB							\
  {{ARGS_FMT_INT,  		WEED_SEED_INT,       		FUNCSIG(INT), 		"INT", "%d"} \
    ,{ARGS_FMT_DOUBLE,  	WEED_SEED_DOUBLE, 		FUNCSIG(DOUBLE), 	"DOUBLE", "%.4f"} \
    ,{ARGS_FMT_BOOLEAN,  	WEED_SEED_BOOLEAN, 		FUNCSIG(BOOL),	 	"BOOL", "%d"} \
    ,{ARGS_FMT_STRING,  	WEED_SEED_STRING, 		FUNCSIG(STRING), 	"STRING", "\"%s\""} \
    ,{ARGS_FMT_STRING_ALT,  	WEED_SEED_STRING, 		FUNCSIG(STRING), 	"STRING", "\"%s\""} \
    ,{ARGS_FMT_INT64,  		WEED_SEED_INT64, 	   	FUNCSIG(INT64), 	"INT64", "%"PRIi64} \
    ,{ARGS_FMT_FUNCPTR,  	WEED_SEED_FUNCPTR, 		FUNCSIG(FUNCP), 	"FUNCP", "%p"} \
    ,{ARGS_FMT_VOIDPTR,  	WEED_SEED_VOIDPTR, 		FUNCSIG(VOIDP), 	"VOIDP", "%p"} \
    ,{ARGS_FMT_VOIDPTR_ALT,  	WEED_SEED_VOIDPTR, 		FUNCSIG(VOIDP), 	"VOIDP", "%p"} \
    ,{ARGS_FMT_PLANTPTR,  	WEED_SEED_PLANTPTR, 		FUNCSIG(PLANTP), 	"PLANTP", "%p"}	\
    ,{ARGS_FMT_PLANTPTR_ALT,  	WEED_SEED_PLANTPTR, 		FUNCSIG(PLANTP), 	"PLANTP", "%p"}	\
    ,{ARGS_FMT_CONST_CHARPTR,  	LIVES_SEED_CONST_CHARPTR,	FUNCSIG(VOIDP),		"VOIDP", "%s"} \
    ,{ARGS_FMT_ALLVALUES,  	LIVES_SEED_ALLVALUES,		FUNCSIG(VOIDP),		"VOIDP", "%p"} \
    ,{ARGS_FMT_VARIADIC,  	LIVES_SEED_VARIADIC,       	FUNCSIG(VARIADIC), 	"VARIADIC", "..."} \
    XREFS_TAB_UINT							\
      XREFS_TAB_UINT64							\
      XREFS_TAB_FLOAT							\
      ,{'\0', WEED_SEED_VOID,       	0, 	"", ""}}

/* if we have an _ va_lsy in an args_fmt string, this can be followed by a descriptio of
   the types held in the va_list, eg. _(iV) indicates a va_list containing an int and a void *
   this can be useful when passing variable types with an args_fmt,, the referenced values can be pulled
   from the valist and set in params */

#define DEF_VARS(n,thing,...) DEF_VARS##n(thing,__VA_ARGS__)

#define GET_WTYPE_INT int
#define GET_WTYPE_BOOL boolean
#define GET_WTYPE_DOUBLE double
#define GET_WTYPE_STRING string
#define GET_WTYPE_INT64 int64
#define GET_WTYPE_VOIDP voidptr
#define GET_WTYPE_PLANTP plantptr
#define GET_WTYPE_FUNCP funcptr

#define GET_WTYPE(a) GET_WTYPE_##a
#define GET_WTYPES1(a) GET_WTYPE(a)
#define GET_WTYPES2(a,b) GET_WTYPE(a), GET_WTYPE(b)
#define GET_WTYPES3(a,b,c) GET_WTYPES2(a,b), GET_WTYPE(c)
#define GET_WTYPES4(a,b,c,d) GET_WTYPES3(a,b,c), GET_WTYPE(d)
#define GET_WTYPES5(a,b,c,d,e) GET_WTYPES4(a,b,c,d), GET_WTYPE(e)
#define GET_WTYPES6(a,b,c,d,e,f) GET_WTYPES5(a,b,c,d,e), GET_WTYPE(f)
#define GET_WTYPES7(a,b,c,d,e,f,g) GET_WTYPES6(a,b,c,d,e,f), GET_WTYPE(g)
#define GET_WTYPES8(a,b,c,d,e,f,g,h) GET_WTYPES7(a,b,c,d,e,f,g), GET_WTYPE(h)

#define GET_WTYPES(n,...) GET_WTYPES##n(__VA_ARGS__)

#define _GETPARAM(thing, n) weed_leaf_get((thing), PROC_THREAD_PARAM(n), 0, &p##n);

#define GET_PARAMS0(thing)
#define GET_PARAMS1(thing) _GETPARAM(thing, 0)
#define GET_PARAMS2(thing) GET_PARAMS1(thing) _GETPARAM(thing, 1)
#define GET_PARAMS3(thing) GET_PARAMS2(thing) _GETPARAM(thing, 2)
#define GET_PARAMS4(thing) GET_PARAMS3(thing) _GETPARAM(thing, 3)
#define GET_PARAMS5(thing) GET_PARAMS4(thing) _GETPARAM(thing, 4)
#define GET_PARAMS6(thing) GET_PARAMS5(thing) _GETPARAM(thing, 5)
#define GET_PARAMS7(thing) GET_PARAMS6(thing) _GETPARAM(thing, 6)
#define GET_PARAMS8(thing) GET_PARAMS7(thing) _GETPARAM(thing, 7)

#define GET_PARAMS(n, thing) GET_PARAMS##n(thing)

#define FREE_CHARPTR_INT(n)
#define FREE_CHARPTR_BOOL(n)
#define FREE_CHARPTR_DOUBLE(n)
#define FREE_CHARPTR_STRING(n) if (p##n) lives_free(p##n);
#define FREE_CHARPTR_INT64(n)
#define FREE_CHARPTR_VOIDP(n)
#define FREE_CHARPTR_PLANTP(n)
#define FREE_CHARPTR_FUNCP(n)

#define FREE_CHARPTR(a,n) FREE_CHARPTR_##a(n)
#define FREE_CHARPTRS0(a)
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

#define VARNAMES(a,...)_VARNAMES(a __VA_OPT__(,)__VA_ARGS__,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL, \
				 NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,"X")
#define _VARNAMES(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,...)			\
  __VARNAMES(#a,#b,#c,#d,#e,#f,#g,#h,#i,#j,#k,#l,#m,#n,#o,#p,__VA_ARGS__)

const char **__VARNAMES(char *a, ...);
#define VARNAME_FUNC const char** __VARNAMES(char*a,...){char*x;va_list b,c;int n=0; \
    va_start(b,a);va_copy(c,b);do{if(!(x=va_arg(b,char*)))n++;}while(!(x&&*x=='X')); \
    va_end(b);LIVES_CALLOC_TYPE(const char*,r,n+1);r[0]=strdup(a);		\
    for(int i=1;i<n;i++)r[i]=(const char *)strdup(va_arg(c,char*));va_end(c);return r;}

#define ADD_FUNCSIG(n,...) reg_funcsig(n, VARNAMES(__VA_ARGS__));

void reg_funcsig(int nparms, const char **anames);

#define REG_FUNCSIGS void reg_known_funcsigs(void) {ZERO_PARAM_FUNCSIG ONE_PARAM_FUNCSIGS TWO_PARAM_FUNCSIGS \
      THREE_PARAM_FUNCSIGS FOUR_PARAM_FUNCSIGS FIVE_PARAM_FUNCSIGS SIX_PARAM_FUNCSIGS \
      SEVEN_PARAM_FUNCSIGS EIGHT_PARAM_FUNCSIGS}

void reg_known_funcsigs(void);

#else // #ifndef NEED_FSIG_CASES

#ifdef ADD_FUNCSIG
#undef ADD_FUNCSIG
#endif

#define ADD_FUNCSIG(n,...) case _FUNCSIG(n,__VA_ARGS__):\
  {DEF_VARS(n,finst->params,__VA_ARGS__); GET_PARAMS(n, finst->params);	\
    if(ret_type)XCALL_##n(finst->params,ret_type,&thefunc);		\
    else CALL_VOID_##n(finst->params,&thefunc);FREE_CHARPTRS(n,__VA_ARGS__);} break;

#endif // NEED_FSIG_CASES

#ifndef HAVE_FSIG_CASES

/////////////////////////////////////////////////////////////////////
// to make a function callable as a proc_thread or a hook callback,
// it is only necessary to add the funcsig here
// for example, if we wsnt to enable al; functions with the prototype
// (void *, weed_funcptr_t, weed_plant_t *), (args_fmt "VFP")
// we would need to add a line: ADD_FUNCSIG(3,VOIDP,FUNCP,PLANTP))

#define ZERO_PARAM_FUNCSIG ADD_FUNCSIG(0, 0)

#define ONE_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(1,INT)				\
  ADD_FUNCSIG(1,BOOL)				\
  ADD_FUNCSIG(1,INT64)				\
  ADD_FUNCSIG(1,DOUBLE)				\
  ADD_FUNCSIG(1,STRING)				\
  ADD_FUNCSIG(1,VOIDP)				\
  ADD_FUNCSIG(1,PLANTP)				\
  ADD_FUNCSIG(1,FUNCP)

#define TWO_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(2,INT,INT)			\
  ADD_FUNCSIG(2,BOOL,BOOL)			\
  ADD_FUNCSIG(2,INT64,INT64)			\
  ADD_FUNCSIG(2,STRING,STRING)			\
  ADD_FUNCSIG(2,DOUBLE,DOUBLE)			\
  ADD_FUNCSIG(2,VOIDP,VOIDP)			\
  ADD_FUNCSIG(2,PLANTP,PLANTP)			\
  ADD_FUNCSIG(2,FUNCP,FUNCP)			\
  ADD_FUNCSIG(2,INT,VOIDP)			\
  ADD_FUNCSIG(2,STRING,INT)			\
  ADD_FUNCSIG(2,STRING,BOOL)			\
  ADD_FUNCSIG(2,BOOL,INT)			\
  ADD_FUNCSIG(2,VOIDP,DOUBLE)			\
  ADD_FUNCSIG(2,VOIDP,INT)			\
  ADD_FUNCSIG(2,VOIDP,INT64)			\
  ADD_FUNCSIG(2,VOIDP,BOOL)			\
  ADD_FUNCSIG(2,VOIDP,STRING)			\
  ADD_FUNCSIG(2,PLANTP,VOIDP)			\
  ADD_FUNCSIG(2,PLANTP,INT64)

#define THREE_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(3,VOIDP,VOIDP,VOIDP)		\
  ADD_FUNCSIG(3,VOIDP,VOIDP,BOOL)		\
  ADD_FUNCSIG(3,STRING,VOIDP,VOIDP)		\
  ADD_FUNCSIG(3,VOIDP,DOUBLE,INT)		\
  ADD_FUNCSIG(3,VOIDP,DOUBLE,VOIDP)		\
  ADD_FUNCSIG(3,VOIDP,INT,INT)			\
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
  ADD_FUNCSIG(4,VOIDP,INT,FUNCP,VOIDP)		\
  ADD_FUNCSIG(4,STRING,INT,FUNCP,VOIDP)

#define FIVE_PARAM_FUNCSIGS			\
  ADD_FUNCSIG(5,VOIDP,STRING,STRING,INT64,INT)	\
  ADD_FUNCSIG(5,INT,INT,INT,BOOL,VOIDP)		\
  ADD_FUNCSIG(5,VOIDP,INT,INT,INT,INT)		\
  ADD_FUNCSIG(5,VOIDP,VOIDP,BOOL,BOOL,INT)

#define SIX_PARAM_FUNCSIGS				\
  ADD_FUNCSIG(6,STRING,STRING,VOIDP,INT,STRING,VOIDP)

#define SEVEN_PARAM_FUNCSIGS
#define EIGHT_PARAM_FUNCSIGS

//////////////////////////////////////////////////////////////

#ifdef NEED_FSIG_CASES
#define HAVE_FSIG_CASES
#endif

#else
// anything to be appended should go here

#endif
#endif


