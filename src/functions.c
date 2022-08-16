// functions.c
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

#ifdef IS_BUNDLE_MAKER
#undef IS_BUNDLE_MAKER
#endif
#include "object-constants.h"

#include "bundles.h"

#define _CTYPE_int int32_t
#define _CPTRTYPE_int int32_t *
#define _CTYPE_double double
#define _CPTRTYPE_double double *
#define _CTYPE_boolean boolean
#define _CPTRTYPE_boolean boolean *
#define _CTYPE_string char *
#define _CPTRTYPE_string char **
#define _CTYPE_int64 int64_t
#define _CPTRTYPE_int64 int64_t *
#define _CTYPE_uint uint32_t
#define _CPTRTYPE_uint uint32_t *
#define _CTYPE_uint64 uint64_t
#define _CPTRTYPE_uint64 uint64_t *
#define _CTYPE_float float
#define _CPTRTYPE_float float *

#define _CTYPE_funcptr lives_funcptr_t
#define _CPTRTYPE_funcptr lives_funcptr_t *
#define _CTYPE_voidptr void *
#define _CPTRTYPE_voidptr void **
#define _CTYPE_plantptr weed_plantptr_t
#define _CPTRTYPE_plantptr weed_plantptr_t *

#define _CTYPE(type) _CTYPE_##type
#define _CPTRTYPE(type) _CPTRTYPE_##type

const lookup_tab crossrefs[] = {{'i',  WEED_SEED_INT, 		0x01, 	"INT"},
  {'d',  WEED_SEED_DOUBLE, 	0x02, 	"DOUBLE"},
  {'b',  WEED_SEED_BOOLEAN, 	0x03, 	"BOOL"},
  {'s',  WEED_SEED_STRING, 	0x04, 	"STRING"},
  {'I',  WEED_SEED_INT64,        	0x05, 	"INT64"},
#ifdef WEED_SEED_UINT
  {'u',  WEED_SEED_UINT, 		0x06, 	"UINT"},
#endif
#ifdef WEED_SEED_UINT64
  {'U',  WEED_SEED_UINT64,       	0x07, 	"UINT64"},
#endif
#ifdef WEED_SEED_FLOAT
  {'f',  WEED_SEED_FLOAT,        	0x08, 	"FLOAT"},
#endif
  {'F',  WEED_SEED_FUNCPTR, 	0x0C, 	"FUNCP"},
  {'v',  WEED_SEED_VOIDPTR, 	0x0D, 	"VOIDP"},
  {'V',  WEED_SEED_VOIDPTR, 	0x0D, 	"VOIDP"},
  {'p',  WEED_SEED_PLANTPTR, 	0x0E, 	"PLANTP"},
  {'P',  WEED_SEED_PLANTPTR, 	0x0E, 	"PLANTP"},
  {'\0', WEED_SEED_VOID, 		0, 	""}
};

#define _SET_LEAF_FROM_VARG(plant, pkey, type, ctype, cptrtype, ne, args) \
  (ne == 1 ? weed_set_##type##_value((plant), (pkey), va_arg((args), ctype)) \
   : weed_set_##type##_array((plant), (pkey), (ne), va_arg((args), cptrtype)))

#define SET_LEAF_FROM_VARG(plant, pkey, type, ne, args) _SET_LEAF_FROM_VARG((plant), (pkey), type, \
									    _CTYPE(type), _CPTRTYPE(type), (ne), (args))

weed_error_t weed_leaf_from_varg(weed_plant_t *plant, const char *key, uint32_t type, weed_size_t ne, va_list xargs) {
  switch (type) {
  case WEED_SEED_INT: return SET_LEAF_FROM_VARG(plant, key, int, ne, xargs);
  case WEED_SEED_DOUBLE: return SET_LEAF_FROM_VARG(plant, key, double, ne, xargs);
  case WEED_SEED_BOOLEAN: return SET_LEAF_FROM_VARG(plant, key, boolean, ne, xargs);
  case WEED_SEED_STRING:  SET_LEAF_FROM_VARG(plant, key, string, ne, xargs);
    g_print("str is %s\n", weed_get_string_value(plant, key, 0)); return 0;
  case WEED_SEED_INT64: return SET_LEAF_FROM_VARG(plant, key, int64, ne, xargs);
#ifdef WEED_SEED_UINT
  case WEED_SEED_UINT: return SET_LEAF_FROM_VARG(plant, key, uint, ne, xargs);
#endif
#ifdef WEED_SEED_UINT64
  case WEED_SEED_UINT64: return SET_LEAF_FROM_VARG(plant, key, uint64, ne, xargs);
#endif
  /* #ifdef WEED_SEED_FLOAT */
  /*   case WEED_SEED_FLOAT: return SET_LEAF_FROM_VARG(plant, key, float, ne, xargs); */
  /* #endif */
  case WEED_SEED_FUNCPTR: return SET_LEAF_FROM_VARG(plant, key, funcptr, ne, xargs);
  case WEED_SEED_VOIDPTR:  return SET_LEAF_FROM_VARG(plant, key, voidptr, ne, xargs);
  case WEED_SEED_PLANTPTR: return SET_LEAF_FROM_VARG(plant, key, plantptr, ne, xargs);
  default: return WEED_ERROR_WRONG_SEED_TYPE;
  }
}


LIVES_LOCAL_INLINE uint32_t _char_to_st(char c) {
  // letter to seed_type
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].letter == c) return crossrefs[i].seed_type;
  }
  return WEED_SEED_INVALID;
}


LIVES_GLOBAL_INLINE const char get_typeletter(uint8_t val) {
  // sigbits to letter
  val &= 0x0F;
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].sigbits == val) return crossrefs[i].letter;
  }
  return '?';
}


LIVES_GLOBAL_INLINE uint32_t get_seedtype(char c) {
  // sigbits OR letter to seed_type
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].sigbits == c || crossrefs[i].letter == c)
      return crossrefs[i].seed_type;
  }
  return WEED_SEED_INVALID;
}


LIVES_GLOBAL_INLINE uint8_t get_typecode(char c) {
  // letter to sigbits
  if (c == 'v') c = 'V';
  if (c == 'p') c = 'P';
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].letter == c) return crossrefs[i].sigbits;
  }
  return 0x0F;
}


LIVES_GLOBAL_INLINE const char *get_symbolname(uint8_t val) {
  // sigbits to symname
  val &= 0x0F;
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].sigbits == val) return crossrefs[i].symname;
  }
  return "";
}


static char *make_std_pname(int pn) {return lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, pn);}

static lives_result_t weed_plant_params_from_valist(weed_plant_t *plant, const char *args_fmt, \
    make_key_f param_name, va_list xargs) {
  int p = 0;
  for (const char *c = args_fmt; *c; c++) {
    char *pkey = (*param_name)(p++);
    uint32_t st = _char_to_st(*c);
    weed_error_t err;
    err = weed_leaf_from_varg(plant, pkey, st, 1, xargs);
    lives_free(pkey);
    if (err != WEED_SUCCESS) {
      return LIVES_RESULT_ERROR;
    }
  }
  va_end(xargs);
  return LIVES_RESULT_SUCCESS;
}


lives_result_t weed_plant_params_from_args_fmt(weed_plant_t *plant, const char *args_fmt, ...) {
  va_list vargs;
  lives_result_t lres;
  va_start(vargs, args_fmt);
  lres = weed_plant_params_from_valist(plant, args_fmt, make_std_pname, vargs);
  va_end(vargs);
  return lres;
}


lives_result_t weed_plant_params_from_vargs(weed_plant_t *plant, const char *args_fmt, va_list vargs) {
  lives_result_t lres;
  lres = weed_plant_params_from_valist(plant, args_fmt, make_std_pname, vargs);
  return lres;
}


char *make_pdef(funcsig_t sig) {
  char *str = lives_strdup("");
  if (sig) {
    int pn = 0;
    for (int i = 60; i >= 0; i -= 4) {
      int j = i;
      uint8_t ch = (sig >> i) & 0X0F;
      if (!ch) continue;
      str = lives_strdup_concat(str, " ", "%sp%d",
                                weed_seed_to_ctype(get_seedtype(ch), TRUE), pn++);
      for (int k = j - 4; k >= 0; k -= 4) {
        uint8_t tch = (sig >> k) & 0X0F;
        if (!tch) break;
        if (tch == ch) {
          if (k == j - 4) j = k;
          str = lives_strdup_concat(str, ", ", "p%d", pn++);
        }
      }
      str = lives_strdup_concat(str, NULL, ";");
      i = j;
    }
  }
  return str;
}


static char *make_pfree(funcsig_t sig) {
  char *str = lives_strdup("");
  if (sig) {
    int pn = 0;
    for (int i = 60; i >= 0; i -= 4) {
      uint8_t ch = (sig >> i) & 0X0F;
      if (get_seedtype(ch) != WEED_SEED_STRING) continue;
      str = lives_strdup_concat(str, " ", "lives_free(p%d);",  pn++);
    }
  }
  return str;
}


int get_funcsig_nparms(funcsig_t sig) {
  int nparms = 0;
  for (funcsig_t test = 0x1; test <= sig; test <<= 4) nparms++;
  return nparms;
}


LIVES_LOCAL_INLINE lives_funcdef_t *lives_proc_thread_define_func(lives_proc_thread_t lpt) {
  lives_funcdef_t *fdef;
  char *funcname, *args_fmt;
  if (!lpt) return NULL;
  funcname = weed_get_string_value(lpt, LIVES_LEAF_FUNC_NAME, 0);
  args_fmt = args_fmt_from_funcsig(weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL));
  fdef = create_funcdef(funcname, weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL),
                        weed_leaf_seed_type(lpt, _RV_), args_fmt, NULL, 0, NULL);
  lives_free(funcname);
  lives_free(args_fmt);
  return fdef;
}


#define DEBUG_FN_CALLBACKS
static boolean _call_funcsig_inner(lives_proc_thread_t lpt, lives_funcptr_t func,
                                   uint32_t ret_type, funcsig_t sig) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  lives_proc_thread_t info;
  allfunc_t *thefunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  char *msg;
  weed_error_t err = WEED_SUCCESS;
  int nparms = get_funcsig_nparms(sig);
#ifdef DEBUG_FN_CALLBACKS
  lives_closure_t *closure;
  const lives_funcdef_t *funcdef;
#endif

  if (!lpt) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL proc_thread");
    return FALSE;
  }
  info = lpt;

  thefunc->func = func;

#ifdef DEBUG_FN_CALLBACKS
  closure = weed_get_voidptr_value(lpt, LIVES_LEAF_CLOSURE, NULL);
  if (closure) {
    funcdef = closure->fdef;
    if (funcdef) {
      if (funcdef->category < FUNC_CATEGORY_HOOK_COMMON) {
        msg = lives_funcdef_explain(funcdef);
        g_print("\ncall_funcsig nparms = %d, funcsig (0X%016lx) called from thread %d, "
                "at current time t + %.4f, with target:\n%s\n", nparms, sig, THREADVAR(idx),
                lives_get_session_time(), msg);
        lives_free(msg);
      }
    } else g_print("func with no fdef !!\n");
  }
#endif

  lives_proc_thread_include_states(info, THRD_STATE_RUNNING);

  // Note: C compilers don't necessarily care about the type / number of function args.,
  // or what the function returns, (else it would be impossible to alias any function pointer)
  // e.g ((generic_func_t) f)(parameters........)
  // the catch is that thhe compiler does need to know the types of the variables being passed as parameters
  // hence we cannot create a macro that just returns a value of the correct type and use this to fill in
  // the function parameters. For this reason, the number and types of parameters have to be defined as literas
  // which is the rason for the switch
  //

  /// LIVES_PROC_THREADS ////////////////////////////////////////

  /// to make any function usable by lives_proc_thread, the _ONLY REQUIREMENT_ is to ensure that there is a switch option
  /// corresponding to the function parameters (i.e the funcsig) and return type, included here below
  /// (use of the FUNCSIG_* symbols is optional, they exist only for the sake of readability)

  ///after expanding the macros, we end up with, for example:
  // if (sig == 0x1 && ret_type == WEED_SEED_INT) {
  //   weed_set_int_value(p, "return_value", int_func(weed_get_int_value(p, "p0", NULL)));
  // }
  ///or in the case of a function returning void:
  // if (sig == 0x22) void_func(weed_get_double_value(p, "p0", NULL), weed_get_double_value(p, "p0", NULL))
  //
  // where 'p' is a lives_proc_thread_t, and int_func / void_func represent any function with a matching return type
  // in the first example, we would create p for example: p = lives_proc_thread_create(LIVES_THRDATTR_NONE, WEED_SEED_INT, int_func,
  //   "i", ival); retval = lives_proc_thread_join_int(p); lives_proc_thread_free(p);
  // (the lives_proc_thread_join_* functions wait for the function to return and then read the typed value of "return_value"
  // this would be equivalent to calling: retval = int_func(ival);
  // except that int_func() would run asyncronously by a (different) worker thread
  // the size of the worker pool changes dynamically, so there will always be a thread available

#define DO_CALL(n, ...) do {if (ret_type) XCALL_##n(__VA_ARGS__);	\
    else XCALL_VOID_##n(__VA_ARGS__);} while (0);
  /* #define DO_CALL(n, ...) do {if (ret_type) XCALL_##n(__VA_ARGS__); \ */
  /*   else XCALL_VOID_##n(__VA_ARGS__);} while (0); */

  // unfortunately, the parameter TYPES need to be known at compile time, so we cant just call a single function

  // and put in the parameters at runtime


#define DO_CALL(n, ...) do {if (ret_type) XCALL_##n(__VA_ARGS__);	\
    else XCALL_VOID_##n(__VA_ARGS__);} while (0);

  switch (nparms) {
  case 0: //FUNCSIG_VOID
    DO_CALL(0,)
    break;

  case 1:
    switch (sig) {
    case FUNCSIG_INT: {
      int p0;
      DO_CALL(1, int);
    } break;
    case FUNCSIG_BOOL: {
      boolean p0;
      DO_CALL(1, boolean);
    } break;
    case FUNCSIG_INT64: {
      int64_t p0;
      DO_CALL(1, int64);
    } break;
    case FUNCSIG_DOUBLE: {
      double p0;
      DO_CALL(1, double);
    } break;
    case FUNCSIG_STRING: {
      char *p0 = NULL;
      DO_CALL(1, string);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_VOIDP: {
      void *p0;
      DO_CALL(1, voidptr);
    } break;
    case FUNCSIG_PLANTP: {
      weed_plant_t *p0;
      DO_CALL(1, plantptr);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 2:
    switch (sig) {
    case FUNCSIG_INT_INT64: {
      int p0; int64_t p1;
      DO_CALL(2, int, int64);
    } break;
    case FUNCSIG_BOOL_INT: {
      int p0; int64_t p1;
      DO_CALL(2, boolean, int);
    } break;
    case FUNCSIG_INT_VOIDP: {
      int p0; void *p1;
      DO_CALL(2, int, voidptr);
    } break;
    case FUNCSIG_INT_PLANTP: {
      int p0; weed_plant_t *p1;
      DO_CALL(2, int, plantptr);
    } break;
    case FUNCSIG_STRING_INT: {
      char *p0 = NULL; int p1;
      DO_CALL(2, string, int);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_STRING_BOOL: {
      char *p0 = NULL; int p1;
      DO_CALL(2, string, boolean);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_DOUBLE_DOUBLE: {
      double p0, p1;
      DO_CALL(2, double, double);
    } break;
    case FUNCSIG_VOIDP_DOUBLE: {
      void *p0; double p1;
      DO_CALL(2, voidptr, double);
    } break;
    case FUNCSIG_VOIDP_INT64: {
      void *p0; int64_t p1;
      DO_CALL(2, voidptr, int64);
    } break;
    case FUNCSIG_VOIDP_VOIDP: {
      void *p0, *p1;
      DO_CALL(2, voidptr, voidptr);
    } break;
    case FUNCSIG_VOIDP_STRING: {
      void *p0; char *p1 = NULL;
      DO_CALL(2, voidptr, string);
      if (p1) lives_free(p1);
    } break;
    case FUNCSIG_PLANTP_BOOL: {
      weed_plant_t *p0; int p1;
      DO_CALL(2, plantptr, boolean);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 3:
    switch (sig) {
    case FUNCSIG_VOIDP_VOIDP_VOIDP: {
      void *p0, *p1, *p2;
      DO_CALL(3, voidptr, voidptr, voidptr);
    } break;
    case FUNCSIG_VOIDP_VOIDP_BOOL: {
      void *p0, *p1; int p2;
      DO_CALL(3, voidptr, voidptr, boolean);
    } break;
    case FUNCSIG_STRING_VOIDP_VOIDP: {
      char *p0 = NULL; void *p1, *p2;
      DO_CALL(3, string, voidptr, voidptr);
      if (p0) lives_free(p0);
    } break;
    case FUNCSIG_VOIDP_DOUBLE_INT: {
      void *p0; double p1; int p2;
      DO_CALL(3, voidptr, double, int);
    } break;
    case FUNCSIG_VOIDP_STRING_STRING: {
      void *p0; char *p1 = NULL, *p2 = NULL;
      DO_CALL(3, voidptr, string, string);
      if (p1) lives_free(p1);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_BOOL_BOOL_STRING: {
      int p0, p1; char *p2 = NULL;
      DO_CALL(3, boolean, boolean, string);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_PLANTP_VOIDP_INT64: {
      weed_plant_t *p0; void *p1; int64_t p2;
      DO_CALL(3, plantptr, voidptr, int64);
    } break;
    case FUNCSIG_INT_VOIDP_INT64: {
      int p0; void *p1; int64_t p2;
      DO_CALL(3, int, voidptr, int64);
    } break;
    case FUNCSIG_INT_INT_BOOL: {
      int p0, p1, p2;
      DO_CALL(3, int, int, boolean);
    } break;
    case FUNCSIG_INT_INT64_VOIDP: {
      int p0; int64_t p1; void *p2;
      DO_CALL(3, int, int64, voidptr);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 4:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT: {
      char *p0 = NULL, *p1 = NULL; void *p2; int p3;
      DO_CALL(4, string, string, voidptr, int);
      if (p0) lives_free(p0);
      if (p1) lives_free(p1);
    } break;
    case FUNCSIG_STRING_DOUBLE_INT_STRING: {
      char *p0 = NULL, *p3 = NULL; double p1; int p2;
      DO_CALL(4, string, double, int, string);
      if (p0) lives_free(p0);
      if (p3) lives_free(p3);
    } break;
    case FUNCSIG_INT_INT_BOOL_VOIDP: {
      int p0, p1, p2; void *p3;
      DO_CALL(4, int, int, boolean, voidptr);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 5:
    switch (sig) {
    case FUNCSIG_VOIDP_STRING_STRING_INT64_INT: {
      void *p0; char *p1 = NULL, *p2 = NULL; int64_t p3; int p4;
      DO_CALL(5, voidptr, string, string, int64, int);
      if (p1) lives_free(p1);
      if (p2) lives_free(p2);
    } break;
    case FUNCSIG_INT_INT_INT_BOOL_VOIDP: {
      int p0, p1, p2, p3; void *p4;
      DO_CALL(5, int, int, int, boolean, voidptr);
    } break;
    case FUNCSIG_VOIDP_INT_INT_INT_INT: {
      void *p0; int p1, p2, p3, p4;
      DO_CALL(5, voidptr, int, int, int, int);
    } break;
    case FUNCSIG_PLANTP_VOIDP_INT_FUNCP_VOIDP: {
      weed_plant_t *p0; void *p1, *p4; int p2; weed_funcptr_t p3;
      DO_CALL(5, plantptr, voidptr, int, funcptr, voidptr);
    } break;
    case FUNCSIG_PLANTP_INT_INT_INT_INT: {
      weed_plant_t *p0; int p1, p2, p3, p4;
      DO_CALL(5, plantptr, int, int, int, int);
    } break;
    /*   // undefined funcsig */
    default: goto funcerr;
    }
    break;

  case 6:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP: {
      char *p0 = NULL, *p1 = NULL, *p4 = NULL; void *p2, *p5; int p3;
      DO_CALL(6, string, string, voidptr, int, string, voidptr);
      if (p0) lives_free(p0);
      if (p1) lives_free(p1);
      if (p4) lives_free(p4);
    } break;
    // undefined funcsig
    default: goto funcerr;
    }
    break;

  case 8:
    switch (sig) {
    case FUNCSIG_PLANTP_INT_INT_INT_INT_INT_INT_INT: {
      weed_plant_t *p0; int p1, p2, p3, p4, p5, p6, p7;
      DO_CALL(8, plantptr, int, int, int, int, int, int, int);
    } break;
    case FUNCSIG_INT_DOUBLE_PLANTP_INT_INT_INT_INT_BOOL: {
      int p0, p3, p4, p5, p6; double p1; weed_plant_t *p2; boolean p7;
      DO_CALL(8, int, double, plantptr, int, int, int, int, boolean);
    } break;
    /*   // undefined funcsig */
    default: goto funcerr;
    }
    break;

  // invalid nparms
  default: goto funcerr;
  }

  if (err == WEED_SUCCESS) {
    return TRUE;
  }
  msg = lives_strdup_printf("Got error %d running prothread ", err);
  goto funcerr2;

funcerr:
  // invalid args_fmt
  msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu)\n", sig, sig);
  if (prefs->show_dev_opts) {
    char *symstr = funcsig_to_symstring(sig);
    char *pdef = make_pdef(sig);
    char *plist = funcsig_to_short_param_string(sig);
    char *pfree = make_pfree(sig);
    char *pdstr = *pdef ? lives_strdup_printf("   %s\n", pdef) : lives_strdup("");
    char *pfstr = *pfree ? lives_strdup_printf("   %s\n", pfree) : lives_strdup("");
    msg = lives_strdup_concat(msg, NULL, "Please add a line in threading.h:\n\n"
                              "#define FUNCSIG_%s\t\t0X%08lX\n\n"
                              "and in threading.c, function _call_funcsig_inner,\n"
                              "locate the switch section for %d parameters "
                              "and add:\n\n case FUNCSIG_%s: {\n%s"
                              "   DO_CALL(%d, %s);\n%s } break;\n",
                              symstr, sig, nparms, symstr, pdstr, nparms, plist, pfstr);
    lives_free(symstr); lives_free(pdef); lives_free(plist);
    lives_free(pfree); lives_free(pdstr); lives_free(pfstr);
  }
  lives_proc_thread_include_states(info, THRD_STATE_INVALID);

funcerr2:
  if (!msg) {
    lives_proc_thread_include_states(info, THRD_STATE_ERROR);
    msg = lives_strdup_printf("Got error %d running function with type 0x%016lX (%lu)", err, sig, sig);
  }
  LIVES_FATAL(msg);
  lives_free(msg);
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t lpt) {
  weed_funcptr_t func = weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  if (func) {
    uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);
    funcsig_t sig = weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL);
    return _call_funcsig_inner(lpt, func, ret_type, sig);
  }
  return FALSE;
}


/**
   create a funcsig from a lives_proc_thread_t object
   the returned value can be passed to call_funcsig, along with the original lives_proc_thread_t
*/
#if 0
static funcsig_t make_funcsig(lives_proc_thread_t func_info) {
  funcsig_t funcsig = 0;
  for (int nargs = 0; nargs < 16; nargs++) {
    char *lname = make_std_pname(nargs);
    int st = weed_leaf_seed_type(func_info, lname);
    lives_free(lname);
    if (!st) break;
    funcsig <<= 4;  /// 4 bits per argtype, hence up to 16 args in a uint64_t
    if (st < 12) funcsig |= st; // 1 == int, 2 == double, 3 == boolean (int), 4 == char *, 5 == int64_t
    else {
      switch (st) {
      case WEED_SEED_FUNCPTR: funcsig |= 0XC; break;
      case WEED_SEED_VOIDPTR: funcsig |= 0XD; break;
      case WEED_SEED_PLANTPTR: funcsig |= 0XE; break;
      default: funcsig |= 0XF; break;
      }
    }
  }
  return funcsig;
}
#endif


LIVES_GLOBAL_INLINE funcsig_t funcsig_from_args_fmt(const char *args_fmt) {
  funcsig_t fsig = 0;
  if (args_fmt) {
    char c;
    for (int i = 0; (c = args_fmt[i]); i++) {
      fsig <<= 4;
      fsig |= get_typecode(c);
    }
  }
  return fsig;
}


char *args_fmt_from_funcsig(funcsig_t sig) {
  char it[2];
  char *args_fmt;
  if (!sig) return lives_strdup("void");
  it[1] = 0;
  args_fmt = lives_strdup("");
  for (int i = 60; i >= 0; i -= 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) continue;
    it[0] = get_typeletter(ch);
    args_fmt = lives_strdup_concat(args_fmt, NULL, "%s", it);
  }
  return args_fmt;
}


char *funcsig_to_param_string(funcsig_t sig) {
  if (sig) {
    char *fmtstring = lives_strdup("");
    for (int i = 60; i >= 0; i -= 4) {
      uint8_t ch = (sig >> i) & 0X0F;
      if (!ch) continue;
      fmtstring = lives_strdup_concat(fmtstring, ", ", "%s",
                                      weed_seed_to_ctype(get_seedtype(ch), FALSE));
    }
    return fmtstring;
  }
  return lives_strdup("void");
}


char *funcsig_to_short_param_string(funcsig_t sig) {
  if (sig) {
    char *fmtstring = lives_strdup("");
    for (int i = 60; i >= 0; i -= 4) {
      uint8_t ch = (sig >> i) & 0X0F;
      if (!ch) continue;
      fmtstring = lives_strdup_concat(fmtstring, ", ", "%s",
                                      weed_seed_to_short_text(get_seedtype(ch)));
    }
    return fmtstring;
  }
  return lives_strdup("void");
}


char *funcsig_to_symstring(funcsig_t sig) {
  char *fmtstring;
  if (!sig) return lives_strdup("void");
  fmtstring = lives_strdup("");
  for (int i = 60; i >= 0; i -= 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) continue;
    fmtstring = lives_strdup_concat(fmtstring, "_", "%s", get_symbolname(ch));
  }
  return fmtstring;
}


static lives_closure_t *lives_hook_closure_new(lives_funcptr_t func, const char *fname, uint64_t flags,
    int type, void *data) {
  lives_closure_t *closure = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
  closure->fdef = get_template_for_func((lives_funcptr_t)func);
  if (!closure->fdef) {
    int category = 0;
    if (type == SYNC_WAIT_HOOK) category = 257;
    else if (type == COMPLETED_HOOK || type == DESTRUCTION_HOOK || type == FINISHED_HOOK ||
             type == CANCELLED_HOOK) category = 256;
    closure->fdef = add_fn_lookup((lives_funcptr_t)func, fname, category, "b", "vv", NULL, 0);
  }
  closure->data = data;
  closure->flags = flags;
  return closure;
}


////// hook functions /////

LIVES_GLOBAL_INLINE void lives_hooks_clear(LiVESList **xlist, int type) {
  if (xlist) lives_list_free_all(&(xlist[type]));
}

LIVES_GLOBAL_INLINE void lives_hooks_clear_all(LiVESList **xlist, int ntypes) {
  if (xlist) {
    for (int i = 0; i < ntypes; i++) if (xlist[i]) lives_list_free_all(&(xlist[i]));
  }
}

LIVES_LOCAL_INLINE LiVESList *lives_hooks_copy(LiVESList *in) {
  LiVESList *out = NULL;
  for (; in; in = in->next) {
    lives_closure_t *inclosure = (lives_closure_t *)in->data;
    lives_closure_t *outclosure = (lives_closure_t *)lives_malloc(sizeof(lives_closure_t));
    outclosure->fdef = inclosure->fdef;
    outclosure->data = inclosure->data;
    outclosure->retloc = inclosure->retloc;
    out = lives_list_append(out, outclosure);
  }
  return out;
}


LIVES_GLOBAL_INLINE void lives_hooks_transfer(LiVESList **dest, LiVESList **src, boolean include_glob) {
  int type = include_glob ? 0 : N_GLOBAL_HOOKS + 1;
  for (; type < N_HOOK_POINTS && src[type]; type++) {
    dest[type] = src[type];
    src[type] = NULL;
  }
}


lives_proc_thread_t _lives_hook_add(LiVESList **hooks, int type, uint64_t flags,
                                    hook_funcptr_t func, const char *fname,
                                    livespointer data, boolean is_append) {

  lives_proc_thread_t xlpt = NULL, lpt = NULL;
  lives_closure_t *closure;
  uint64_t xflags = flags & HOOK_UNIQUE_REPLACE_MATCH;
  boolean cull = FALSE;
  pthread_mutex_t hmutex;
  //  int category = FUNC_CATEGORY_HOOK_COMMON;

  if (!hooks) hooks = THREADVAR(hook_closures);

  if (lpt) hmutex = ((pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_HOOK_MUTEXES, NULL))[type];
  else hmutex = THREADVAR(hook_mutex[type]);

  if (flags & HOOK_CB_FG_THREAD) {
    xlpt = lpt = (lives_proc_thread_t)data;
  }
  if (xflags) {
    lives_proc_thread_t lpt2 = NULL;
    LiVESList *list = hooks[type], *listnext;
    int maxp = 0;

    if (flags & HOOK_CB_FG_THREAD) {
      maxp = THREADVAR(hook_match_nparams);
    }
    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_STATUS_BLOCKED) continue;
      if (lpt) {
        if (!(closure->flags & HOOK_CB_FG_THREAD)) continue;
        lpt2 = (lives_proc_thread_t)data;
        if (fn_func_match(lpt, lpt2) < 0) continue;
      } else if (closure->fdef->function != (lives_funcptr_t)func) continue;
      if (xflags == HOOK_UNIQUE_FUNC) return NULL;
      if (cull || xflags == HOOK_UNIQUE_REPLACE_FUNC) {
        lpt2 = (lives_proc_thread_t)data;
        if (lpt && lpt == lpt2) continue;
        pthread_mutex_lock(&hmutex);
        hooks[type] = lives_list_remove_node(hooks[type], list, FALSE);
        pthread_mutex_unlock(&hmutex);
        if (lpt2) {
          lives_closure_t *cl2 = weed_get_voidptr_value(lpt2, LIVES_LEAF_CLOSURE, NULL);
          if (cl2 && (cl2->flags & HOOK_CB_BLOCK)) {
            // if the caller is waiting for the hoko to complete, we will not kill the proc thread
            // but we will set the state to CANCELLED instead. In this case, it will be cancelled
            // the function will return but the thread will lack the COMPLETED (or DESTROYED) state.
            lives_proc_thread_include_states(lpt2, THRD_STATE_CANCELLED);
          } else {
            // if we preempted another proc_thread, free the older one
            lives_proc_thread_include_states(lpt2, THRD_STATE_DESTROYED);
            lives_proc_thread_free(lpt2);
          }
          if (cl2) lives_free(cl2);
        }  else lives_free(closure);
        continue;
      }
      if ((!lpt && closure->data == data)
          || (lpt && fn_data_match(lpt2, lpt, maxp))) {
        if (xflags == HOOK_UNIQUE_REPLACE_MATCH) {
          if (lpt2) {
            lives_closure_t *cl2;
            if (lpt && lpt == lpt2) continue;
            pthread_mutex_lock(&hmutex);
            hooks[type] = lives_list_remove_node(hooks[type], list, FALSE);
            pthread_mutex_unlock(&hmutex);
            cl2 = weed_get_voidptr_value(lpt2, LIVES_LEAF_CLOSURE, NULL);
            if (cl2 && (cl2->flags & HOOK_CB_BLOCK)) {
              // if the caller is waiting for the hoko to complete, we will not kill the proc thread
              // but we will set the state to CANCELLED instead. In this case, it will be cancelled
              // the function will return but the thread will lack the COMPLETED (or DESTROYED) state.
              lives_proc_thread_include_states(lpt2, THRD_STATE_CANCELLED);
            } else {
              // if we preempted another proc_thread, free the older one
              lives_proc_thread_include_states(lpt2, THRD_STATE_DESTROYED);
              lives_proc_thread_free(lpt2);
            }
            if (cl2) lives_free(cl2);
          } else lives_free(closure);
          continue;
        }
        if (xflags & HOOK_UNIQUE_DATA) return NULL;
      } else {
        if (!cull && (xflags == HOOK_UNIQUE_REPLACE
                      || xflags == HOOK_UNIQUE_REPLACE_OR_ADD)) {
          if (lpt) {
            if (lpt2 != lpt) {
              fn_data_replace(lpt2, lpt);
              xlpt = lpt2;
            }
          } else closure->data = data;
          cull = TRUE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (cull || xflags == HOOK_UNIQUE_REPLACE) return xlpt;

  closure = lives_hook_closure_new((lives_funcptr_t)func, fname, flags, type, data);

  pthread_mutex_lock(&hmutex);
  if (is_append) hooks[type] = lives_list_append(hooks[type], closure);
  else hooks[type] = lives_list_prepend(hooks[type], closure);
  pthread_mutex_unlock(&hmutex);

  if (lpt) return xlpt;
  return NULL;
}


static lives_proc_thread_t _lives_hook_append(LiVESList **hooks, int type, uint64_t flags,
    hook_funcptr_t func, const char *fname, livespointer data) {
  return _lives_hook_add(hooks, type, flags, func, fname, data, TRUE);
}

static ALLOW_UNUSED lives_proc_thread_t _lives_hook_prepend(LiVESList **hooks, int type, uint64_t flags,
    hook_funcptr_t func, const char *fname, livespointer data) {
  return _lives_hook_add(hooks, type, flags, func, fname, data, FALSE);
}


LIVES_GLOBAL_INLINE boolean lives_hooks_trigger(lives_obj_t *obj, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_closure_t *closure;
  boolean bret;
  pthread_mutex_t hmutex;

  if (type == DATA_READY_HOOK) {
    lives_hooks_trigger_async(obj, xlist, type);
    return TRUE;
  }

  if (obj) hmutex = ((pthread_mutex_t *)weed_get_voidptr_value(obj, LIVES_LEAF_HOOK_MUTEXES, NULL))[type];
  else hmutex = THREADVAR(hook_mutex[type]);

  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (!closure) continue;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return FALSE;
    if (closure->flags |= HOOK_STATUS_RUNNING) {
      if (closure->flags & HOOK_CB_FG_THREAD) {
        closure->tinfo = (lives_proc_thread_t)closure->data;
        weed_set_voidptr_value(closure->tinfo, LIVES_LEAF_CLOSURE, closure);
        if (is_fg_thread()) {
          if (!lives_proc_thread_is_done(closure->tinfo)) {
            fg_run_func(closure->tinfo, closure->retloc);
            lives_proc_thread_include_states(closure->tinfo, THRD_STATE_DESTROYED);
            lives_proc_thread_free(closure->tinfo);
          } else {
            // some functions may have been deferred, since we cannot stack multiple fg service calls
            closure->tinfo = lives_proc_thread_auto_secure(&closure->tinfo);
            if (closure->tinfo) {
              if (!lives_proc_thread_is_done(closure->tinfo))
                fg_service_call(closure->tinfo, closure->retloc);
              if (!lives_proc_thread_unref(closure->tinfo)) {
                lives_proc_thread_include_states(closure->tinfo, THRD_STATE_DESTROYED);
                lives_proc_thread_free(closure->tinfo);
		// *INDENT-OFF*
	      }}}}
	// *INDENT-ON*

        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
        continue;
      }

      closure->tinfo =
        _lives_proc_thread_create(LIVES_THRDATTR_FG_THREAD, (lives_funcptr_t)closure->fdef->function,
                                  closure->fdef->funcname, closure->fdef->return_type,
                                  closure->fdef->args_fmt, obj, closure->data);
      weed_set_voidptr_value(closure->tinfo, LIVES_LEAF_CLOSURE, closure);
      fg_run_func(closure->tinfo, &bret);

      if (type != SYNC_WAIT_HOOK) {
        lives_proc_thread_include_states(closure->tinfo, THRD_STATE_DESTROYED);
        lives_proc_thread_free(closure->tinfo);
        closure->tinfo = NULL;
      }

      if (!bret) {
        if (type == SYNC_WAIT_HOOK) {
          closure->flags &= ~HOOK_STATUS_RUNNING;
          return FALSE;
        }
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
      } else closure->flags &= ~HOOK_STATUS_RUNNING;
    }
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean lives_hooks_triggero(lives_object_t *obj, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  boolean bret;
  pthread_mutex_t hmutex;

  if (type == DATA_READY_HOOK) {
    lives_hooks_trigger_asynco(obj, xlist, type);
    return TRUE;
  }

  if (obj) hmutex = obj->hook_mutex[type];
  else hmutex = THREADVAR(hook_mutex[type]);

  for (list = xlist[type]; list; list = listnext) {
    lives_closure_t *closure = (lives_closure_t *)list->data;
    listnext = list->next;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return FALSE;
    if (closure->flags |= HOOK_STATUS_RUNNING) {
      if (closure->flags & HOOK_CB_FG_THREAD) {
        closure->tinfo = (lives_proc_thread_t)closure->data;
        weed_set_voidptr_value(closure->tinfo, LIVES_LEAF_CLOSURE, closure);
        if (is_fg_thread()) {
          if (!lives_proc_thread_is_done(closure->tinfo))
            fg_run_func(closure->tinfo, closure->retloc);
        } else {
          // some functions may have been deferred, since we cannot stack multiple fg service calls
          closure->tinfo = lives_proc_thread_auto_secure(&closure->tinfo);
          if (closure->tinfo) {
            if (!lives_proc_thread_is_done(closure->tinfo))
              fg_service_call(closure->tinfo, closure->retloc);
            if (lives_proc_thread_unref(closure->tinfo)) closure->tinfo = NULL;
          }
        }
        if (closure->tinfo) {
          lives_proc_thread_include_states(closure->tinfo, THRD_STATE_DESTROYED);
          lives_proc_thread_free(closure->tinfo);
        }
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
        continue;
      }

      closure->tinfo =
        _lives_proc_thread_create(LIVES_THRDATTR_FG_THREAD, (lives_funcptr_t)closure->fdef->function,
                                  closure->fdef->funcname, closure->fdef->return_type,
                                  closure->fdef->args_fmt, obj, closure->data);
      weed_set_voidptr_value(closure->tinfo, LIVES_LEAF_CLOSURE, closure);
      fg_run_func(closure->tinfo, &bret);
      lives_proc_thread_include_states(closure->tinfo, THRD_STATE_DESTROYED);
      lives_proc_thread_free(closure->tinfo);
      closure->tinfo = NULL;

      if (!bret) {
        if (type == SYNC_WAIT_HOOK) {
          closure->flags &= ~HOOK_STATUS_RUNNING;
          return FALSE;
        }
        pthread_mutex_lock(&hmutex);
        xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
        pthread_mutex_unlock(&hmutex);
      } else closure->flags &= ~HOOK_STATUS_RUNNING;
    }
  }
  return TRUE;
}


#define SYNC_CHECK_TIME ONE_MILLION

static void _lives_hooks_tr_seq(lives_proc_thread_t lpt, LiVESList **xlist, int type,  hook_funcptr_t finfunc,
                                void *findata) {
  while (1) {
    if (lives_hooks_trigger(lpt, xlist, type)) {
      // if all functions return TRUE, execute finfunc, and exit
      if (finfunc)((*finfunc)(lpt, findata));
      return;
    }
    lives_nanosleep(SYNC_CHECK_TIME);
  }
}

static void _lives_hooks_tr_seqo(lives_object_t *obj, LiVESList **xlist, int type, hook_funcptr_t finfunc,
                                 void *findata) {
  /* lives_hooks_triggero(obj, xlist, type); */
  /* (*finfunc)(obj,findata); */
}

static boolean remifalseo(lives_object_t *xobj, void *xxlist) {
  return TRUE;
}

static boolean remifalse(lives_obj_t *xlpt, void *xxlist) {
  if (weed_get_boolean_value(xlpt, _RV_, NULL) == FALSE) {
    int type = weed_get_int_value(xlpt, LIVES_LEAF_XLIST_TYPE, NULL);
    pthread_mutex_t *hook_mutex =
      (pthread_mutex_t *)weed_get_voidptr_value(xlpt, LIVES_LEAF_HOOK_MUTEXES, NULL);
    LiVESList **xlist = (LiVESList **)xxlist;
    LiVESList *list = weed_get_voidptr_value(xlpt, LIVES_LEAF_XLIST_LIST, NULL);
    pthread_mutex_lock(&hook_mutex[type]);
    xlist[type] = lives_list_remove_node(xlist[type], list, FALSE);
    pthread_mutex_unlock(&hook_mutex[type]);
    lives_free(hook_mutex);
  }
  lives_proc_thread_free(xlpt);
  return FALSE;
}


void lives_hooks_trigger_async_sequentialo(lives_object_t *lpt, LiVESList **xlist, int type,
    hook_funcptr_t finfunc, void *findata) {
}


void lives_hooks_trigger_async_sequential(lives_obj_t *lpt, LiVESList **xlist, int type,
    hook_funcptr_t finfunc, void *findata) {
  lives_proc_thread_create(0, (lives_funcptr_t)_lives_hooks_tr_seq, 0, "pviFv",
                           lpt, xlist, type, (weed_funcptr_t)finfunc, findata);
}


LIVES_GLOBAL_INLINE void _lives_proc_thread_hook_append(lives_proc_thread_t lpt, int type, uint64_t flags,
    hook_funcptr_t func, const char *fname, livespointer data) {
  LiVESList **hook_closures = lives_proc_thread_get_hook_closures(lpt);
  _lives_hook_append(hook_closures, type, flags, func, fname, data);
}


void lives_hooks_trigger_async(lives_obj_t *lpt, LiVESList **xlist, int type) {
  LiVESList *list = xlist[type], *listnext;
  lives_proc_thread_t xlpt;
  lives_closure_t *closure;
  LiVESList **hook_closures;
  for (list = xlist[type]; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    if (closure->flags & HOOK_STATUS_RUNNING) return;
    if (closure->flags |= HOOK_STATUS_RUNNING);
    closure->tinfo = xlpt =
                       _lives_proc_thread_create(LIVES_THRDATTR_WAIT_SYNC, (lives_funcptr_t)closure->fdef->function,
                           closure->fdef->funcname, closure->fdef->return_type,
                           closure->fdef->args_fmt, lpt, closure->data);
    weed_set_voidptr_value(closure->tinfo, LIVES_LEAF_CLOSURE, closure);
    hook_closures = lives_proc_thread_get_hook_closures(xlpt);
    lives_hook_append(hook_closures, FINISHED_HOOK, 0, remifalse, xlist);
    weed_set_int_value(xlpt, LIVES_LEAF_XLIST_TYPE, type);
    weed_set_voidptr_value(xlpt, LIVES_LEAF_XLIST_LIST, list);
    lives_proc_thread_sync_ready(xlpt);
  }
}


void lives_hooks_trigger_asynco(lives_object_t *obj, LiVESList **xlist, int type) {
  ///
}


void _lives_hook_remove(LiVESList **xlist, int type, hook_funcptr_t func, livespointer data,
                        pthread_mutex_t *hmutexes) {
  pthread_mutex_lock(&hmutexes[type]);
  for (LiVESList *list = xlist[type]; list; list = list->next) {
    lives_closure_t *closure = (lives_closure_t *)list->data;
    if (closure->fdef->function == (lives_funcptr_t)func && closure->data == data) {
      closure->flags |= HOOK_STATUS_BLOCKED;
      xlist[type] = lives_list_remove_node(xlist[type], list, TRUE);
      break;
    }
  }
  pthread_mutex_unlock(&hmutexes[type]);
}


LIVES_GLOBAL_INLINE void lives_hooks_join(LiVESList * xlist, pthread_mutex_t *mutex) {
  lives_closure_t *closure;
  while (1) {
    LiVESList *list;
    pthread_mutex_lock(mutex);
    for (list = xlist; list; list = list->next) {
      closure = (lives_closure_t *)list->data;
      if (closure->flags & HOOK_STATUS_RUNNING) break;
    }
    pthread_mutex_unlock(mutex);
    if (!list) break;
    lives_nanosleep(10000);
  }
}


boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp) {
  if (!lpt1 || !lpt2 || weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)
      != weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)) return FALSE;
  else {
    funcsig_t fsig1 = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
    funcsig_t fsig2 = weed_get_int64_value(lpt2, LIVES_LEAF_FUNCSIG, NULL);
    if (fsig1 != fsig2) return FALSE;
    int nparms = get_funcsig_nparms(fsig1);
    if (maxp && nparms > maxp) nparms = maxp;
    for (int i = 0; i < nparms; i++) {
      char *pname = make_std_pname(i);
      if (weed_leaf_elements_equate(lpt1, pname, lpt2, pname, -1) == WEED_FALSE) {
        lives_free(pname);
        return FALSE;
      }
      lives_free(pname);
    }
  }
  return TRUE;
}


// return -ve value on mismatch-, or nparams on match
// -1 == NULL or fn mismatch
// -2 == return_type mismatch
// -3 == arg_fmt mismatch
int fn_func_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2) {
  if (!lpt1 || !lpt2 || weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)
      != weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)) return -1;
  if (weed_leaf_seed_type(lpt1, _RV_) != weed_leaf_seed_type(lpt2, _RV_)) return -2;
  else {
    funcsig_t fsig = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
    if (fsig != weed_get_int64_value(lpt2, LIVES_LEAF_FUNCSIG, NULL)) return -3;
    return get_funcsig_nparms(fsig);
  }
}


boolean fn_data_replace(lives_proc_thread_t src, lives_proc_thread_t dst) {
  int nparms = fn_func_match(src, dst);
  if (nparms >= 0) {
    for (int i = 0; i < nparms; i++) {
      char *pname = PROC_THREAD_PARAM(i);
      weed_leaf_dup(dst, src, pname);
      lives_free(pname);
    }
    return TRUE;
  }
  return FALSE;
}


//////////////////////////// funcdefs & funcinsts /////////////////////////////////

LIVES_GLOBAL_INLINE lives_funcdef_t *create_funcdef(const char *funcname, lives_funcptr_t function,
    uint32_t return_type,  const char *args_fmt,
    const char *file, int line, void *data) {
  lives_funcdef_t *fdef = (lives_funcdef_t *)lives_calloc(1, sizeof(lives_funcdef_t));
  if (fdef) {
    if (funcname) fdef->funcname = lives_strdup(funcname);
    else fdef->funcname = NULL;
    fdef->uid = gen_unique_id();
    fdef->function = function;
    fdef->return_type = return_type;
    if (args_fmt) fdef->args_fmt = lives_strdup(args_fmt);
    else fdef->args_fmt = NULL;
    fdef->funcsig = funcsig_from_args_fmt(args_fmt);
    if (file) fdef->file = lives_strdup(file);
    fdef->line = line;
    fdef->data = data;
  }
  return fdef;
}


LIVES_GLOBAL_INLINE void free_funcdef(lives_funcdef_t *fdef) {
  if (fdef) {
    lives_freep((void **)&fdef->funcname);
    lives_freep((void **)&fdef->args_fmt);
    lives_free(fdef);
  }
}


LIVES_LOCAL_INLINE lives_funcinst_t *create_funcinst_valist(lives_funcdef_t *template, va_list xargs) {
  lives_funcinst_t *finst = lives_plant_new(LIVES_WEED_SUBTYPE_FUNCINST);
  if (finst) {
    _proc_thread_params_from_vargs(finst, template->function, template->return_type
                                   ? template->return_type : -1, template->args_fmt, xargs);
    weed_set_voidptr_value(finst, LIVES_LEAF_TEMPLATE, template);
  }
  return finst;
}


LIVES_LOCAL_INLINE lives_funcinst_t *create_funcinst_nullvalist(lives_funcdef_t *template) {
  lives_funcinst_t *finst = lives_plant_new(LIVES_WEED_SUBTYPE_FUNCINST);
  if (finst) {
    _proc_thread_params_from_nullvargs(finst, template->function,
                                       template->return_type ? template->return_type : -1);
    weed_set_voidptr_value(finst, LIVES_LEAF_TEMPLATE, template);
  }
  return finst;
}


LIVES_GLOBAL_INLINE lives_funcinst_t *create_funcinst(lives_funcdef_t *template, void *retstore, ...) {
  lives_funcinst_t *finst = NULL;
  if (template) {
    if (template->args_fmt) {
      va_list xargs;
      va_start(xargs, retstore);
      finst = create_funcinst_valist(template, xargs);
      va_end(xargs);
    } else finst = create_funcinst_nullvalist(template);
  }
  return finst;
}


LIVES_GLOBAL_INLINE void free_funcinst(lives_funcinst_t *finst) {
  if (finst) {
    lives_funcdef_t *fdef = (lives_funcdef_t *)weed_get_voidptr_value(finst, LIVES_LEAF_TEMPLATE, NULL);
    if (fdef) free_funcdef(fdef);
    lives_free(finst);
  }
}


///////////// lookup functions //////

static lives_hash_store_t *ftrace_store = NULL;

void add_fn_note(int io, void *ptr, const char *fname, const char *fref, int lineno) {
  weed_plant_t *note;
  if (!ftrace_store)  ftrace_store = lives_hash_store_new("ftrace");
  note = (weed_plant_t *)get_from_hash_store_i(ftrace_store, (uintptr_t)ptr);
  if (note) {
    int count = weed_get_int_value(note, "count", NULL);
    if (io == _FN_ALLOC) {
      if (count > 0) weed_set_int_value(note, "count", ++count);
    } else {
      if (count == 1)
        ftrace_store = remove_from_hash_store_i(ftrace_store, (uintptr_t)ptr);
      else
        weed_set_int_value(note, "count", --count);
    }
    return;
  }
  note = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
  weed_set_string_value(note, "func", fname);
  weed_set_string_value(note, "file", fref);
  weed_set_int_value(note, "line", lineno);
  weed_set_int_value(note, "count", io == _FN_ALLOC ? 1 : -1);
  ftrace_store = add_to_hash_store_i(ftrace_store, (uintptr_t)ptr, (void *)note);
}


void dump_fn_notes(void) {
  if (ftrace_store) {
    const char *key;
    char **items = weed_plant_list_leaves(ftrace_store, NULL);
    for (int i = 0; items[i]; i++) {
      if ((key = hash_key_from_leaf_name(items[i]))) {
        weed_plant_t *note = (weed_plant_t *)get_from_hash_store_i(ftrace_store, lives_strtoul(key));
        g_print("func %s, called from %s, line %d (%d times)\n", weed_get_string_value(note, "func", NULL),
                weed_get_string_value(note, "file", NULL), weed_get_int_value(note, "line", NULL),
                weed_get_int_value(note, "count", NULL));
      }
      lives_free(items[i]);
    }
    lives_free(items);
  }
}


LIVES_GLOBAL_INLINE char *get_argstring_for_func(lives_funcptr_t func) {
  const lives_funcdef_t *fdef = get_template_for_func(func);
  if (!fdef) return NULL;
  return funcsig_to_param_string(funcsig_from_args_fmt(fdef->args_fmt));
}


char *lives_funcdef_explain(const lives_funcdef_t *funcdef) {
  if (funcdef) {
    char *tmp, *out =
      lives_strdup_printf("Function with uid 0X%016lX has prototype:\n"
                          "\t%s %s(%s)\n function category is %d", funcdef->uid,
                          weed_seed_to_ctype(funcdef->return_type, FALSE),
                          funcdef->funcname ? funcdef->funcname : "??????",
                          (tmp = funcsig_to_param_string(funcsig_from_args_fmt(funcdef->args_fmt))),
                          funcdef->category);
    lives_free(tmp);
    return out;
  }
  return NULL;
}


//////////////////// objects / transforms API /////

/* typedef lives_bundle_t trajectory_t; */


/* typedef lives_bundle_t lives_tseg_t; */
/* typedef lives_bundle_t lives_trajectory_t; */

/* // -> this will become create instance for the trajectory object */
/* // returns a new bunle of type trajectory with default values set */
/* trajectory_t *create_trajectory(lives_obj_t *transform) { */
/*   static boolean defined = FALSE; */
/*   lives_bundle_t *traj; */
/*   // first create the trajectory_bundle */
/*   traj = def_bundle_from_bundledef(GET_BDEF(TRAJECTORY_BUNDLE_TYPE)); */

/*   // now we should point attributes at tx attrs */
/*   // then create segment bundles */
/*   return traj; */
/* } */


/* lives_tseg_t *trajectory_add_segment(lives_trajectory_t *traj, uint64_t flags, lives_obj_attr_t *ret, */
/* 				     lives_funcptr_t func, ...) { */
/*   lives_bundle_t *tseg = var_bundle_from_bundledef(GET_BDEF(TSEGMENT_BUNDLEDEF), "native_function", func, NULL); */
/*   // now we just need to create the pmapping */
/*   va_list vargs; */
/*   lives_bundle_t *pmap = */
/*     var_bundle_from_bundledef(PMAP_BUNDLE_TYPE, "param_num", -1, "attribute", ret, NULL); */
/*   bundle_array_append(tseg, "mapping", 1, NULL, pmap); */
/*   va_start(vargs, func); */
/*   for (int i = 0; ; i++) { */
/*     lives_obj_attr_t *attr = va_arg(vargs, lives_obj_attr_t *); */
/*     if (!attr) break; */
/*     pmap = var_bundle_from_bundledef(GET_BDEF(PMAP_BUNDLE_TYPE), "param_num", i, "attribute", attr, NULL); */
/*     bundle_array_append(tseg, "mapping", 1, NULL, pmap); */
/*   } */
/*   va_end(vargs); */
/*   bundle_array_append(traj, "tseg", 1, NULL, tseg); */
/*   return tseg; */
/* } */


/* void link_segments(lives_trajectory_t *traj, lives_tseg_t *from, lives_tseg_t *to, lives_bundle_t *conditions) { */

/*   lives_bundle_t *trajc = var_bundle_from_bundledef(TRAJECTORY_CONDITION_BUNDLEDEF, "conditionals", conditions, */
/* 						    "segments", to, NULL); */
/*   bundle_array_append(traj, "segments", 1, NULL, trajc); */
/* } */


/* int testa(int v) { */
/*   g_print("got val %d\n", v); */
/*   return 10; */
/* } */


/* int testb(int v2) { */
/*   g_print("got val %d\n", v2); */
/*   return 100; */
/* } */


/* boolean testc(lives_obj_t *obj, void *data) { */
/*   g_print("segment done\n"); */
/*   return TRUE; */
/* } */


/* #define WRAP_FN(func, ret, nparams, ...) WRAP_FN##nparams(func, ret, __VA_ARGS__) \ */
/*     uint32_t func##_get_ret_type(void) {return ret;} */

/* #define WRAP_FN0(func, ret) _CTYPE(ret) func##_wrapper(lives_proc_thread_t lpt) {return func();} \ */
/*   const char * func##_get_args_fmt(void) {return NULL;} \ */


/* #define WRAP_FN1(func, ret, t0)  _CTYPE(ret) func##_wrapper(lives_proc_thread_t lpt) { \ */
/*     _CTYPE(t0) p0 = GET_ARG_##t0(lpt, 0); return func(p0);}		\ */
/*   const char * func##_get_args_fmt(void) {return TL(t0);} */

/* #define WRAP_FN2(func, ret, t0, t1)  _CTYPE(ret) func##_wrapper(lives_proc_thread_t lpt) { \ */
/*   _CTYPE(t0) p0 = GET_ARG_##t0(lpt, 0); _CTYPE(t1) p1 = GET_ARG_##t1(lpt, 1);  return func(p0);} \ */
/*   const char * func##_get_args_fmt(void) {return TL(t0) TL(t1);} */

/* #define WRAP_FN3(func, ret, t0, t1, t3)  _CTYPE(ret) func##_wrapper(lives_proc_thread_t lpt) { \ */
/*     _CTYPE(t0) p0 = GET_ARG_##t0(lpt, 0); _CTYPE(t1) p1 = GET_ARG_##t1(lpt, 1); CTYPE(t2) p2 = GET_ARG_##t2(lpt, 2); \ */
/*     return func(p0, p1, p2);}						\ */
/*   const char * func##_get_args_fmt(void) {return TL(t0) TL(t1) TL(t2);} */



/* void test_traj(void) { */
/*   lives_trajectory_t *traj = create_trajectory(NULL); */
/*   lives_tseg_t *sega, *segb; */
/*   lives_obj_attr_t *attr_int, *ret_attr; */
/*   int status; */
/*   // create funcdefs for a and b */
/*   ret_attr = lives_object_declare_attribute(NULL, "return", ATTR_TYPE_INT); */
/*   attr_int = lives_object_declare_attribute(NULL, "int var", ATTR_TYPE_INT); */
/*   lives_object_set_attr_value(NULL, attr_int, 22); */
/*   sega = trajectory_add_segment(traj, 0, ret_attr, (lives_funcptr_t)testa, attr_int, NULL); */
/*   segb = trajectory_add_segment(traj, 0, ret_attr, (lives_funcptr_t)testb, ret_attr, NULL); */
/*   link_segments(traj, sega, segb, NULL); */
/*   lives_hook_append(THREADVAR(hook_closures), SEGMENT_END_HOOK, 0, testc, NULL); */
/*   // status = run_trajectory(traj, NULL); */
/*   lives_hook_remove(THREADVAR(hook_closures), SEGMENT_END_HOOK, testc, NULL, THREADVAR(hook_mutex)); */
/* } */
