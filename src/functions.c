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
/* #include "object-constants.h" */

/* #include "bundles.h" */

// TODO - > weed_host_utils.c

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

#ifdef DEBUG_MUTEXES
#define PTMLH do {g_print("lock %p at %d\n", hmutex, _LINE_REF_); pthread_mutex_lock(hmutex);} while (0)
#define PTMUH do {g_print("unlock %p at %d\n", hmutex, _LINE_REF_); pthread_mutex_unlock(hmutex);} while (0)
#define PTMTLH (printf("lock %p at %d\n", hmutex, _LINE_REF_) ? pthread_mutex_trylock(hmutex) : 1)
#else
#define PTMLH do {pthread_mutex_lock(hmutex);} while (0)
#define PTMUH do {pthread_mutex_unlock(hmutex);} while (0)
#define PTMTLH pthread_mutex_trylock(hmutex)
#endif

const lookup_tab crossrefs[] = {{'i',  WEED_SEED_INT, 		0x01, 	"INT", "%d"},
  {'d',  WEED_SEED_DOUBLE, 	0x02, 	"DOUBLE", "%.4f"},
  {'b',  WEED_SEED_BOOLEAN, 	0x03, 	"BOOL", "%d"},
  {'s',  WEED_SEED_STRING, 	0x04, 	"STRING", "\"%s\""},
  {'I',  WEED_SEED_INT64,        	0x05, 	"INT64", "%"PRIi64},
#ifdef WEED_SEED_UINT
  {'u',  WEED_SEED_UINT, 		0x06, 	"UINT", "%u"},
#endif
#ifdef WEED_SEED_UINT64
  {'U',  WEED_SEED_UINT64,       	0x07, 	"UINT64", "%"PRIu64},
#endif
#ifdef WEED_SEED_FLOAT
  {'f',  WEED_SEED_FLOAT,        	0x08, 	"FLOAT", "%.4f"},
#endif
  {'F',  WEED_SEED_FUNCPTR, 	0x0C, 	"FUNCP", "%p"},
  {'v',  WEED_SEED_VOIDPTR, 	0x0D, 	"VOIDP", "%p"},
  {'V',  WEED_SEED_VOIDPTR, 	0x0D, 	"VOIDP", "%p"},
  {'p',  WEED_SEED_PLANTPTR, 	0x0E, 	"PLANTP", "%p"},
  {'P',  WEED_SEED_PLANTPTR, 	0x0E, 	"PLANTP", "%p"},
  {'\0', WEED_SEED_VOID, 		0, 	"", ""}
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
  case WEED_SEED_STRING:  return SET_LEAF_FROM_VARG(plant, key, string, ne, xargs);
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


LIVES_GLOBAL_INLINE const char get_char_for_st(uint32_t st) {
  // letter to sigbits
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].seed_type == st) return crossrefs[i].letter;
  }
  return '\0';
}

LIVES_GLOBAL_INLINE const char *get_fmtstr_for_st(uint32_t st) {
  // letter to sigbits
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].seed_type == st) return crossrefs[i].fmtstr;
  }
  return '\0';
}


LIVES_GLOBAL_INLINE const char *get_symbolname(uint8_t val) {
  // sigbits to symname
  val &= 0x0F;
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].sigbits == val) return crossrefs[i].symname;
  }
  return "";
}

static boolean is_child_of(LiVESWidget *w, LiVESContainer *C);
static boolean fn_match_child(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2);

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
  for (funcsig_t test = 0xF; test & sig; test <<= 4) nparms++;
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


//#define DEBUG_FN_CALLBACKS
static boolean _call_funcsig_inner(lives_proc_thread_t lpt, lives_funcptr_t func,
                                   uint32_t ret_type, funcsig_t sig) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint64_t attrs = (uint64_t)weed_get_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, NULL);
  allfunc_t *thefunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  char *msg;
  weed_error_t err = WEED_SUCCESS;
  int nparms = get_funcsig_nparms(sig);
#ifdef DEBUG_FN_CALLBACKS
  lives_closure_t *closure;
  const lives_funcdef_t *funcdef;
#endif

  if (lives_proc_thread_ref(lpt) <= 0 || !lpt) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid proc_thread");
    return FALSE;
  }

  thefunc->func = func;

  // STATE CHANGED - queued / preparing -> running
  lives_proc_thread_exclude_states(lpt, THRD_STATE_QUEUED | THRD_STATE_UNQUEUED | THRD_STATE_DEFERRED |
                                   THRD_STATE_PREPARING);
  lives_proc_thread_include_states(lpt, THRD_STATE_RUNNING);

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
  //   "i", ival); retval = lives_proc_thread_join_int(p); lives_proc_thread_unref(p);
  // (the lives_proc_thread_join_* functions wait for the function to return and then read the typed value of "return_value"
  // this would be equivalent to calling: retval = int_func(ival);
  // except that int_func() would run asyncronously by a (different) worker thread
  // the size of the worker pool changes dynamically, so there will always be a thread available

  // this is the important part here, as it expands to an actual function call which the compiler recognises
  // e.g DO_CALL(2, boolean, int)
  // expands to something like
  // (rtype == WEED_SEED_BOOLEAN ? weed_set_boolean_value(__RV__,
  //   (*thufunc->boolean)	((p0 = weed_get_boolean_value(lpt, P0_NAME, NULL)),
  //				(p1 = weed_get_int_value(lpt, P!_NAME, NULL))), NULL) :
  //  ...etc for all rtypes
  // p0, p1 etc are only there so we can free pn if it is a weed_get_string_value()
  // as well as being a useful cross-check that the formats are correct
  // Macro paremeters in DO_CALL have to be literals, therefore.
  // Unfortunately we cannot simply use DO_CALL(nparams, p0, p1, ...)

#define DO_CALL(n, ...) do {if (ret_type) XCALL_##n(lpt, ret_type, thefunc, __VA_ARGS__); \
    else CALL_VOID_##n(lpt, __VA_ARGS__);} while (0);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_START_TICKS, lives_get_current_ticks());

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
    case FUNCSIG_PLANTP_VOIDP: {
      weed_plant_t *p0; void *p1;
      DO_CALL(2, plantptr, voidptr);
    } break;
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
    case FUNCSIG_VOIDP_BOOL: {
      void *p0; int p1;
      DO_CALL(2, voidptr, boolean);
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
    case FUNCSIG_VOIDP_STRING_INT: {
      void *p0; char *p1 = NULL; int p2;
      DO_CALL(3, voidptr, string, int);
      if (p1) lives_free(p1);
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
    case FUNCSIG_STRING_INT_BOOL: {
      char *p0 = NULL; int p1, p2;
      DO_CALL(3, string, int, boolean);
      if (p0) lives_free(p0);
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
    case FUNCSIG_VOIDP_INT_FUNCP_VOIDP: {
      void *p0, *p3; int p1; weed_funcptr_t p2;
      DO_CALL(4, voidptr, int, funcptr, voidptr);
    } break;
    case FUNCSIG_VOIDP_VOIDP_VOIDP_VOIDP: {
      void *p0, *p1, *p2, *p3;
      DO_CALL(4, voidptr, voidptr, voidptr, voidptr);
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
    case FUNCSIG_VOIDP_VOIDP_BOOL_BOOL_INT: {
      void *p0, *p1; int p2, p3, p4;
      DO_CALL(5, voidptr, voidptr, boolean, boolean, int);
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
    case FUNCSIG_PLANTP_INT_INT_INT_INT_INT: {
      weed_plant_t *p0; int p1, p2, p3, p4, p5;
      DO_CALL(6, plantptr, int, int, int, int, int);
    } break;
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
  lives_free(thefunc);

  lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);

  if (lives_proc_thread_get_cancel_requested(lpt))
    lives_proc_thread_cancel(lpt);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS) {
    weed_set_int64_value(lpt, LIVES_LEAF_END_TICKS, lives_get_current_ticks());
  }

#ifdef DEBUG_FN_CALLBACKS
  if (1) {
    ticks_t qtime = lives_proc_thread_get_timing_info(lpt, TIME_TOT_QUEUE);
    ticks_t sytime = lives_proc_thread_get_timing_info(lpt, TIME_TOT_SYNC_START);
    ticks_t ptime = lives_proc_thread_get_timing_info(lpt, TIME_TOT_PROC);
    g_print("%s\n[thread tid %d, queue wait time %.4f usec, sync_wait time %.4f usec, proc time %.4f usec]\n\n",
            lives_proc_thread_show_func_call(lpt), THREADVAR(idx),
            (double)qtime / (double)USEC_TO_TICKS,
            (double)sytime / (double)USEC_TO_TICKS,
            (double)ptime / (double)USEC_TO_TICKS);
  }
#endif

  if (err == WEED_SUCCESS) {
    lives_proc_thread_unref(lpt);
    return TRUE;
  }
  msg = lives_strdup_printf("Got error %d running procthread ", err);
  goto funcerr2;

funcerr:
  // invalid args_fmt
  lives_free(thefunc);
  msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu), nparams = %d\n", sig, sig, nparms);
  if (prefs->show_dev_opts) {
    char *symstr = funcsig_to_symstring(sig);
    char *pdef = make_pdef(sig);
    char *plist = funcsig_to_short_param_string(sig);
    char *pfree = make_pfree(sig);
    char *pdstr = *pdef ? lives_strdup_printf("   %s\n", pdef) : lives_strdup("");
    char *pfstr = *pfree ? lives_strdup_printf("   %s\n", pfree) : lives_strdup("");
#ifdef __FILE__
    char *filen = lives_strdup(__FILE__);
    get_filename(filen, TRUE);
#else
    filen = lives_strdup("functions");
#endif

    msg = lives_strdup_concat(msg, NULL, "Please add a line in %s.h:\n\n"
                              "#define FUNCSIG_%s\t\t0X%08lX\n\n"
                              "and in %s.c, function _call_funcsig_inner,\n"
                              "locate the switch section for %d parameters "
                              "and add:\n\n case FUNCSIG_%s: {\n%s"
                              "   DO_CALL(%d, %s);\n%s } break;\n", filen,
                              symstr, sig, filen, nparms, symstr, pdstr, nparms, plist, pfstr);
    lives_free(symstr); lives_free(pdef); lives_free(plist); lives_free(filen);
    lives_free(pfree); lives_free(pdstr); lives_free(pfstr);
  }
  lives_proc_thread_include_states(lpt, THRD_STATE_INVALID);

funcerr2:
  if (lives_proc_thread_get_cancel_requested(lpt))
    lives_proc_thread_cancel(lpt);

  if (!msg) {
    lives_proc_thread_include_states(lpt, THRD_STATE_ERROR);
    msg = lives_strdup_printf("Got error %d running function with type 0x%016lX (%lu)", err, sig, sig);
  }

  //LIVES_FATAL(msg);
  LIVES_ERROR(msg);
  lives_free(msg);
  lives_proc_thread_unref(lpt);
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t lpt) {
  weed_funcptr_t func = weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  if (func) {
    uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);
    funcsig_t sig = weed_get_int64_value(lpt, LIVES_LEAF_FUNCSIG, NULL);
    _call_funcsig_inner(lpt, func, ret_type, sig);
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


static char *lpt_paramstr(lives_proc_thread_t lpt, funcsig_t sig) {
  int pn = 0;
  char *pstr = NULL, *pname, *fmtstr = lives_strdup("");
  const char *ctype;
  uint32_t ne, st;
  for (int i = 60; i >= 0; i -= 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) continue;
    st = get_seedtype(ch);
    pname = make_std_pname(pn++);
    ne = weed_leaf_num_elements(lpt, pname);
    ctype = weed_seed_to_ctype(st, FALSE);
    if (ne > 1) pstr = lives_strdup_printf("%s[%d]", ctype, ne);
    else {
      char *fmtpstr = lives_strdup_printf("(%s)%s", ctype, get_fmtstr_for_st(st));
      // TODO, free strings
      FOR_ALL_SEED_TYPES(st, pstr = lives_strdup_printf, fmtpstr, weed_get_, _value, lpt, pname, NULL);
      lives_free(fmtpstr);
    }
    fmtstr = lives_strdup_concat(fmtstr, ", ", "%s", pstr);
    lives_freep((void **)&pstr); lives_free(pname);
  }
  return fmtstr;
}


char *lives_proc_thread_show_func_call(lives_proc_thread_t lpt) {
  if (lpt) {
    char *fmtstring, *parvals;
    uint32_t ret_type = lives_proc_thread_get_rtype(lpt);
    funcsig_t sig = lives_proc_thread_get_funcsig(lpt);
    char *funcname = lives_proc_thread_get_funcname(lpt);

    parvals = lpt_paramstr(lpt, sig);

    if (ret_type) {
      fmtstring = lives_strdup_printf("%sreturn_value = %s(%s);",
                                      weed_seed_to_ctype(ret_type, TRUE), funcname, parvals);
    } else fmtstring = lives_strdup_printf("(void) %s(%s);", funcname, parvals);
    lives_free(funcname); lives_free(parvals);
    return fmtstring;
  }
  return NULL;
}


static int lives_fdef_get_category(int cat, int dtl) {
  int category = FUNC_CATEGORY_GENERAL;
  switch (cat) {
  case FUNC_CATEGORY_CALLBACK:
    switch (dtl) {
    case LIVES_GUI_HOOK:
      category = FUNC_CATEGORY_UTIL; break;
    case SYNC_WAIT_HOOK:
      category = FUNC_CATEGORY_HOOK_SYNC; break;
    default: break;
    }
  default: break;
  }
  return category;
}


lives_closure_t *lives_hook_closure_new_for_lpt(lives_proc_thread_t lpt,
    uint64_t flags, int hook_type) {
  if (lpt) {
    lives_closure_t *closure  = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
    closure->fdef = lives_proc_thread_make_funcdef(lpt);
    ((lives_funcdef_t *)closure->fdef)->category
      = lives_fdef_get_category(FUNC_CATEGORY_CALLBACK, hook_type);
    closure->proc_thread = lpt;
    closure->flags = flags;
    closure->retloc = weed_get_voidptr_value(lpt, "retloc", NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_CLOSURE, closure);
    return closure;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void lives_closure_free(lives_closure_t *closure) {
  if (closure) {
    lives_proc_thread_t lpt = closure->proc_thread;
    if (lives_proc_thread_ref(lpt) > 0) {
      weed_leaf_delete(lpt, LIVES_LEAF_CLOSURE);
      lives_proc_thread_unref(lpt);
      lives_proc_thread_unref(lpt);
    }
    free_funcdef((lives_funcdef_t *)closure->fdef);
    lives_free(closure);
  }
}


////// hook functions /////

LIVES_GLOBAL_INLINE void lives_hooks_clear(lives_hook_stack_t **hstacks, int type) {
  if (hstacks) {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = hstack->mutex;

    while (1) {
      lives_nanosleep_until_zero(hstack->flags & STACK_TRIGGERING);
      PTMLH;
      if (hstack->flags & STACK_TRIGGERING) PTMUH;
      else break;
    }

    if (hstack->stack) {
      for (LiVESList *cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist->next)
        lives_closure_free((lives_closure_t *)cblist->data);
      lives_list_free((LiVESList *)hstack->stack);
      hstack->stack = NULL;
    }
    PTMUH;
  }
}
//)


LIVES_GLOBAL_INLINE void lives_hooks_clear_all(lives_hook_stack_t **hstacks, int ntypes) {
  if (hstacks)
    for (int i = 0; i < ntypes; i++) {
      pthread_mutex_t *hmutex = hstacks[i]->mutex;
      lives_hooks_clear(hstacks, i);
      while (1) {
        lives_nanosleep_until_zero(hstacks[i]->flags & STACK_TRIGGERING);
        PTMLH;
        if (hstacks[i]->flags & STACK_TRIGGERING) PTMUH;
        else break;
      }
      PTMUH;
      pthread_mutex_destroy(hstacks[i]->mutex);
      lives_free(hstacks[i]->mutex);
      lives_free(hstacks[i]);
    }
}


// replace closure cl with repl (due to uniqueness)
// if cl is blocking, then we add a ref to repl, add a leaf to cl->proc_thread
// "replacement", pointing to repl
// the waiting thread should be checking for this, if the leaf is added,
// it should unref the original proc, add continue blocking on repl instead
// after repl completes, all threads waiting on it must unref it
static boolean replace_hook(lives_proc_thread_t lpt, lives_proc_thread_t repl) {
  if (!lpt || !repl) return FALSE;
  lives_proc_thread_ref(repl);
  weed_set_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, repl);
  return TRUE;
}


static boolean fn_data_replace(lives_proc_thread_t dst, lives_proc_thread_t src) {
  int nparms = fn_func_match(src, dst);
  if (nparms > 0) {
    for (int i = 0; i < nparms; i++) {
      char *pname = make_std_pname(i);
      weed_leaf_dup(dst, src, pname);
      lives_free(pname);
    }
    return TRUE;
  }
  return FALSE;
}


static void add_to_cb_list(lives_proc_thread_t self, lives_closure_t *closure) {
  // when adding a hook cb to another hook stack, keep a pointert to it
  LiVESList *list = (LiVESList *)weed_get_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, NULL);
  weed_set_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, lives_list_prepend(list, (void *)closure));
}


static void flush_cb_list(lives_proc_thread_t self) {
  // when freeing a proc_threead, remove all added hook cbs from other proc_threads / thread stacks
  LiVESList *xlist = (LiVESList *)weed_get_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, NULL), *list;
  if (xlist) {
    for (list = xlist; list; list = list->next) {
      lives_closure_t *cl = (lives_closure_t *)list->data;
      if (cl) {
        lives_hook_remove(cl->hook_stacks, cl->hook_type, cl->proc_thread);
      }
    }
    lives_list_free(xlist);
    weed_leaf_delete(self, LIVES_LEAF_EXT_CB_LIST);
  }
}


static void remove_ext_cb(lives_closure_t *cl) {
  // when freeing a closure, remove the pointer to it from the adder
  if (cl) {
    lives_proc_thread_t lpt = cl->adder;
    if (lives_proc_thread_ref(lpt) > 0) {
      //_remove_ext_cb(lpt, cl);
    }
  }
}


// for the GUI stacks:
// things to note - for dtype != DTYPE_CLOSURE (ie. lpt) we only ever add when appending
// when prepending the type is always DTYPE_CLOSURE, with or without DTYPE_NOADD
// DTYPE_NOADD only gets set with prepend
// generally we append to thread stack, then at some point append or prepend to fg thread stack
// uniquenes tests are applied when adding. Invalidate_data is tested when adding (same stack)
// and when triggering (all thread stacks with deferal queues)
lives_proc_thread_t lives_hook_add(lives_hook_stack_t **hstacks, int type, uint64_t flags,
                                   livespointer data, uint64_t dtype) {
  lives_proc_thread_t lpt = NULL;

  lives_closure_t *closure, *xclosure = NULL, *ret_closure = NULL;
  GET_PROC_THREAD_SELF(self);
  uint64_t xflags = flags & (HOOK_UNIQUE_REPLACE | HOOK_INVALIDATE_DATA);
  uint64_t attrs;
  pthread_mutex_t *hmutex;
  boolean is_close = FALSE, is_append = TRUE, is_remove = FALSE;
  boolean have_lock = FALSE;

  if (!hstacks) {
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      if (!self) return NULL;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return NULL;
    }
  }

  if (dtype & DTYPE_CLOSURE) is_close = TRUE;

  if (dtype & DTYPE_PREPEND) is_append = FALSE;
  if (dtype & DTYPE_NOADD) is_remove = TRUE;

  if (dtype & DTYPE_HAVE_LOCK) have_lock = TRUE;

  // append, then everything else will check
  if (is_append) xflags &= ~HOOK_INVALIDATE_DATA;

  // is_close,is for here
  // the important flags for cehcking are ia_append, is_remove

  if (is_close) {
    xclosure = (lives_closure_t *)data;
    lpt = xclosure->proc_thread;
    if (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
      while (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
        lpt =  weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, 0);
      }
      return lpt;
    }
  } else {
    lpt = (lives_proc_thread_t)data;
  }

  if (!is_append) ret_closure = xclosure;

  hmutex = hstacks[type]->mutex;

  if (!have_lock) {
    if (PTMTLH) {
      if (is_fg_thread()) {
        // it is possible for the main thread to be adding callbacks
        // at the same time as the target is triggering hooks
        // we need to keep servicing requests while waiting, since
        // the triggering thread might be waiting for the main thread to service
        // a callback
        while (PTMTLH) {
          fg_service_fulfill();
          lives_nanosleep(10000000);
        }
      } else PTMLH;
    }
  }

  if (1) {
    lives_proc_thread_t lpt2 = NULL;
    LiVESList *cblist, *cblistnext;
    int maxp = THREADVAR(hook_match_nparams);
    boolean fmatch;

    for (cblist = (LiVESList *)hstacks[type]->stack; cblist; cblist = cblistnext) {
      uint64_t cfinv = 0;
      cblistnext = cblist->next;
      closure = (lives_closure_t *)cblist->data;
      if (!closure) continue;
      /// ??
      //if (!closure->fdef) continue;
      if (closure->flags & (HOOK_STATUS_BLOCKED | HOOK_STATUS_ACTIONED | HOOK_STATUS_RUNNING)) continue;
      lpt2 = closure->proc_thread;
      if (weed_plant_has_leaf(lpt2, LIVES_LEAF_REPLACEMENT)) continue;
      if (!lpt2) break_me("null procthread in closure\n");
      if (!lpt2 || lpt2 == lpt) continue;

      if (is_append) {
        cfinv = closure->flags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
        if (!(cfinv & HOOK_INVALIDATE_DATA)) cfinv = 0;
      }

      if (!(fmatch = fn_func_match(lpt, lpt2) < 0)
          && !((xflags & HOOK_INVALIDATE_DATA) || cfinv != 0)) continue;

      if (fmatch && (xflags & HOOK_UNIQUE_FUNC)) {
        // skip over if the target stack is triggering
        if (!(hstacks[type]->flags & STACK_TRIGGERING)) {
          // unique_func -> maintain only 1st fn match
          // with unique replace -> also replace 1st with our data
          if (!ret_closure) {
            if (xflags & HOOK_UNIQUE_DATA) {
              if (!fn_data_match(lpt2, lpt, maxp)) continue;
              if (maxp != 0 && !fn_data_match(lpt2, lpt, 0)) {
                fn_data_replace(lpt2, lpt);
              }
            }
            ret_closure = closure;
            continue;
          }
          if (closure->flags & HOOK_STATUS_RUNNING) closure->flags |= HOOK_STATUS_REMOVE;
          else {
            boolean isblock = FALSE;
            hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, cblist, FALSE);
            if (closure->flags & HOOK_CB_BLOCK) {
              isblock = TRUE;
              lives_proc_thread_ref(lpt2);
            }

            lives_closure_free(closure);

            if (isblock) {
              ret_closure->flags |= HOOK_CB_BLOCK;
              replace_hook(lpt2, ret_closure->proc_thread);
            }
          }
        }
        continue;
      }

      if (!(cfinv || (xflags & (HOOK_INVALIDATE_DATA | HOOK_UNIQUE_DATA)))) continue;

      if (!fn_data_match(lpt2, lpt, maxp)) {
        if (!((cfinv | flags) & HOOK_OPT_MATCH_CHILD)) continue;
        if (!(((flags & HOOK_OPT_MATCH_CHILD) && fn_match_child(lpt, lpt2))
              || ((cfinv & HOOK_OPT_MATCH_CHILD) && fn_match_child(lpt2, lpt))))
          continue;
      }

      // if we reach here it means that xflags == HOOK_UNIQUE_DATA, or closure->flags
      // had INVALIDATE_DATA, and we found a match
      // so we will not add
      ret_closure = closure;
      break;
    }
  }

  if (is_remove) {
    if (!have_lock) PTMUH;
    return lpt;
  }

  if (ret_closure && ret_closure != xclosure) {
    if (!have_lock) PTMUH;
    if (flags & HOOK_CB_BLOCK) {
      ret_closure->flags |= HOOK_CB_BLOCK;
      replace_hook(lpt, ret_closure->proc_thread);
    }
    return ret_closure->proc_thread;
  }

  closure = NULL;
  if (is_append) {
    if (is_close) closure = (lives_closure_t *)data;
    else closure = xclosure;
  }

  if (!closure) {
    closure = lives_hook_closure_new_for_lpt(lpt, flags, type);
    closure->adder = self;
    add_to_cb_list(self, closure);
  }

  lives_proc_thread_include_states(closure->proc_thread, THRD_STATE_STACKED);

  if (is_append) hstacks[type]->stack = lives_list_append((LiVESList *)hstacks[type]->stack, closure);
  else hstacks[type]->stack = lives_list_prepend((LiVESList *)hstacks[type]->stack, closure);

  closure->hook_stacks = hstacks;
  closure->hook_type = type;

  if (!have_lock) PTMUH;

  attrs = (uint64_t)weed_get_int64_value(lpt, LIVES_LEAF_THREAD_ATTRS, NULL);
  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());

  return lpt;
}


lives_proc_thread_t lives_hook_add_full(lives_hook_stack_t **hooks, int type, uint64_t flags,
                                        lives_funcptr_t func, const char *fname, int return_type,
                                        const char *args_fmt, ...) {
  lives_proc_thread_t lpt;
  uint64_t attrs = LIVES_THRDATTR_START_UNQUEUED;
  uint64_t dtype = 0;
  // test should be async_callbacks
  if (type == DATA_READY_HOOK) {
    // for async callbacks, we treat each callback as an idlefunc and queue it instead of running
    // direct. If the callback returns TRUE, then it will finish in state IDLING
    // otherwise (FALSE) it will end in state FINISHED. So async_join will check the state and
    // wait for is_done, then remove any callbacks having proc_thread state FINISHED
    // also when triggered, we only run (queue) those is state UNQUEUED and ! FINISHED
    attrs |= LIVES_THRDATTR_IDLEFUNC;
  }

  if (flags & HOOK_CB_PRIORITY) dtype |= DTYPE_PREPEND;

  if (args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    lpt = _lives_proc_thread_create_vargs(attrs, func, fname, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, fname, return_type);

  return lives_hook_add(hooks, type, flags, (void *)lpt, dtype);
}


static lives_proc_thread_t update_linked_stacks(lives_closure_t *cl, uint64_t flags) {
  uint64_t dflags = DTYPE_NOADD | DTYPE_PREPEND | DTYPE_CLOSURE;
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  if (!is_fg_thread()) {
    GET_PROC_THREAD_SELF(self);
    lives_hook_stack_t **mystacks = lives_proc_thread_get_hook_stacks(self);
    if (!(mainw->global_hook_stacks[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING))
      lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
    for (LiVESList *links = mainw->all_hstacks; links; links = links->next) {
      lives_hook_stack_t **xhs = (lives_hook_stack_t **)links->data;
      if (xhs == mystacks) continue;
      if (xhs[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING) continue;
      lives_hook_add(xhs, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
    }
  } else {
    for (LiVESList *links = mainw->all_hstacks; links; links = links->next) {
      lives_hook_stack_t **xhs = (lives_hook_stack_t **)links->data;
      if (xhs[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING) continue;
      lives_hook_add(xhs, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
    }
  }
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);
  return NULL;
}


boolean lives_hooks_trigger(lives_hook_stack_t **hstacks, int type) {
  static pthread_mutex_t recheck_mutex = PTHREAD_MUTEX_INITIALIZER;
  lives_proc_thread_t lpt, wait_parent;
  LiVESList *list, *listnext;
  lives_closure_t *closure;
  pthread_mutex_t *hmutex;
  boolean bret;
  boolean retval = TRUE;
  boolean have_recheck_mutex = FALSE;
  boolean hmulocked = TRUE;

  if (!hstacks) {
    // test should be HOOK_TYPE_SELF
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      GET_PROC_THREAD_SELF(self);
      if (!self) return FALSE;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return FALSE;
    }
  }

  if (type == DATA_READY_HOOK) {
    lives_hooks_trigger_async(hstacks, type);
    return FALSE;
  }

  hmutex = hstacks[type]->mutex;
  //if (type == SYNC_WAIT_HOOK) dump_hook_stack(hstacks, type);

  if (type == LIVES_GUI_HOOK && is_fg_thread()) {
    if (PTMTLH) return FALSE;
  } else PTMLH;

  if (!hstacks[type]->stack || (hstacks[type]->flags & STACK_TRIGGERING)) {
    PTMUH;
    hmulocked = FALSE;
    goto trigdone;
  }

  hstacks[type]->flags |= STACK_TRIGGERING;

  list = (LiVESList *)hstacks[type]->stack;

  // mark all entries in list at entry as "ACTIONED"
  // since we will parse the list several times, we only check those which are present now
  // this avoids a situation where we would be endlessly traversing the list as new items are added
  for (; list; list = list->next) {
    closure = (lives_closure_t *)list->data;
    if (!closure || !closure->proc_thread) continue;
    closure->flags |= HOOK_STATUS_ACTIONED;
  }

  do {
    retval = FALSE;
    if (!hmulocked) {
      if (type == LIVES_GUI_HOOK && is_fg_thread()) {
        if (PTMTLH) goto trigdone;
      } else PTMLH;
      hmulocked = TRUE;
    }

    retval = TRUE;

    list = (LiVESList *)hstacks[type]->stack;

    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;

      if (!closure || !closure->proc_thread) continue;
      if (!(closure->flags & HOOK_STATUS_ACTIONED)) continue;
      if (closure->flags & (HOOK_STATUS_BLOCKED | HOOK_STATUS_RUNNING)) continue;

      lives_proc_thread_ref(closure->proc_thread);
      lpt = closure->proc_thread;
      if (!lpt) continue;

      if (closure->flags & HOOK_STATUS_REMOVE) {
        hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, list, FALSE);
        // for oneshot triggers, we remove a ref, so caller should ref anything important before
        // triggering hooks
        //g_print("free one-shot hook %s\n", lives_proc_thread_show_func_call(lpt));
        PTMUH;
        // remove our added ref UNLESS this is a blocking call, then blocked thrread will do that
        if (!(closure->flags & HOOK_CB_BLOCK)) lives_proc_thread_unref(lpt);
        lives_closure_free(closure);
        break;
      }

      if (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
        lives_proc_thread_unref(lpt);
        continue;
      }

      if (type == LIVES_GUI_HOOK) {
        uint64_t xflags = closure->flags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
        if (xflags) {
          if (!have_recheck_mutex) {
            if (pthread_mutex_trylock(&recheck_mutex)) {
              // this means some other thread also has invalidate_data, we must let it run then recheck
              lives_proc_thread_unref(lpt);
              lives_nanosleep_until_zero(pthread_mutex_trylock(&recheck_mutex));
              have_recheck_mutex = TRUE;
              break;
            }
          }

          wait_parent = update_linked_stacks(closure, xflags);
          pthread_mutex_unlock(&recheck_mutex);

          if (wait_parent) {
            lives_proc_thread_unref(lpt);
            lives_proc_thread_wait_done(wait_parent, 0.);
            break;
          }
        }
      }

      closure->flags |= HOOK_STATUS_RUNNING;
      closure->flags &= ~HOOK_STATUS_ACTIONED;

      PTMUH;
      hmulocked = FALSE;

      /* g_print("\nrun hook cb %s %p %p\n", lives_proc_thread_show_func_call(lpt), */
      /* 	      lpt, lpt); */

      lives_proc_thread_include_states(lpt, THRD_STATE_STACKED);
      lives_proc_thread_exclude_states(lpt, THRD_STATE_IDLING | THRD_STATE_UNQUEUED
                                       | THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                       | THRD_STATE_FINISHED | THRD_STATE_DEFERRED);

      if (type == SYNC_WAIT_HOOK) {
        // test should be boolean_combined
        bret = TRUE; // set in case func is cancelled
        closure->retloc = &bret;
        //g_print("sync run: %s\n", lives_proc_thread_show_func_call(lpt));
      }

      if (!(closure->flags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
        lives_proc_thread_execute(lpt, closure->retloc);
      } else {
        lives_proc_thread_include_states(lpt, THRD_STATE_QUEUED);
        fg_service_call(lpt, closure->retloc);
      }

      if (closure->flags & (HOOK_OPT_ONESHOT | HOOK_STATUS_REMOVE)) {
        PTMLH;
        hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, list, FALSE);
        // for oneshot triggers, we remove a ref, so caller should ref anything important before
        // triggering hooks
        //g_print("free one-shot hook %s\n", lives_proc_thread_show_func_call(lpt));
        PTMUH;

        closure->flags &= ~HOOK_STATUS_REMOVE;

        // remove our added ref UNLESS this is a blocking call, then blocked thrread will do that
        if (!(closure->flags & HOOK_CB_BLOCK)) lives_proc_thread_unref(lpt);

        // this also removes a ref
        lives_closure_free(closure);
        break;
      }

      closure->flags &= ~HOOK_STATUS_RUNNING;

      if (type == SYNC_WAIT_HOOK && !bret) {
        // combined bool / exit on false
        lives_proc_thread_unref(lpt);
        retval = FALSE;
        list = NULL;
        break;
      }

      // actually should be HOOK_DTL_SINGLE
      if (type == LIVES_GUI_HOOK) {
        if (is_fg_thread()) {
          //g_print("done single\n");
          lives_proc_thread_unref(lpt);
          list = NULL;
          retval = TRUE;
          break;
        }
      }
      lives_proc_thread_unref(lpt);
      break;
    }

    if (!list) {
      list = (LiVESList *)hstacks[type]->stack;
      if (!list) {
        if (!is_fg_thread() && type == LIVES_GUI_HOOK) {
          pthread_mutex_lock(&mainw->all_hstacks_mutex);
          mainw->all_hstacks =
            lives_list_remove_data(mainw->all_hstacks, hstacks, FALSE);
          pthread_mutex_unlock(&mainw->all_hstacks_mutex);
        }
      } else {
        for (; list; list = list->next) {
          closure = (lives_closure_t *)list->data;
          if (closure) closure->flags &= ~HOOK_STATUS_ACTIONED;
        }
      }
    }
    if (hmulocked) {
      PTMUH;
      hmulocked = FALSE;
    }
  } while (list);

trigdone:
  if (hmulocked) {
    // should not happen, but just in case...
    PTMUH;
  }

  hstacks[type]->flags &= ~STACK_TRIGGERING;

  if (have_recheck_mutex) {
    pthread_mutex_unlock(&recheck_mutex);
  }

  //if (type == SYNC_WAIT_HOOK) g_print("sync all res: %d\n", retval);
  return retval;
}


void lives_hooks_trigger_async(lives_hook_stack_t **hstacks, int type) {
  lives_proc_thread_t lpt;
  LiVESList *list, *listnext;
  lives_closure_t *closure;
  pthread_mutex_t *hmutex;

  if (!hstacks) {
    // test should be HOOK_TYPE_SELF
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      GET_PROC_THREAD_SELF(self);
      if (!self) return;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return;
    }
  }

  hmutex = hstacks[type]->mutex;

  if (!hstacks[type]->stack) {
    PTMUH;
    return;
  }

  list = (LiVESList *)hstacks[type]->stack;

  for (; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;

    if (!closure || !closure->proc_thread) continue;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;

    lives_proc_thread_ref(closure->proc_thread);
    lpt = closure->proc_thread;
    if (!lpt) continue;

    if (closure->flags & HOOK_STATUS_REMOVE) {
      hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, list, FALSE);
      lives_proc_thread_unref(lpt);
      lives_closure_free(closure);
      continue;
    }

    lives_proc_thread_include_states(lpt, THRD_STATE_STACKED);

    lives_proc_thread_exclude_states(lpt, THRD_STATE_IDLING | THRD_STATE_UNQUEUED
                                     | THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                     | THRD_STATE_FINISHED | THRD_STATE_DEFERRED);

    lives_proc_thread_queue(lpt, 0);
  }
  PTMUH;
}


boolean lives_proc_thread_trigger_hooks(lives_proc_thread_t lpt, int type) {
  if (lpt) {
    return lives_hooks_trigger(lives_proc_thread_get_hook_stacks(lpt), type);
  }
  return FALSE;
}


static void _lives_hooks_tr_seq(lives_hook_stack_t **hstacks, int type,  hook_funcptr_t finfunc,
                                void *findata) {
  GET_PROC_THREAD_SELF(self);
  while (1) {
    if (lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_cancel(self);
      return;
    }
    if (lives_hooks_trigger(hstacks, type)) {
      // if all functions return TRUE, execute finfunc, and exit
      if (finfunc)((*finfunc)(NULL, findata));
      if (lives_proc_thread_get_cancel_requested(self)) {
        lives_proc_thread_cancel(self);
      }
      return;
    }
    if (lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_cancel(self);
      return;
    }
    lives_nanosleep(SYNC_CHECK_TIME);
  }
}


lives_proc_thread_t lives_hooks_trigger_async_sequential(lives_hook_stack_t **hstacks, int type,
    hook_funcptr_t finfunc, void *findata) {
  lives_proc_thread_t poller = lives_proc_thread_create(LIVES_THRDATTR_WAIT_SYNC,
                               (lives_funcptr_t)_lives_hooks_tr_seq, -1, "viFv",
                               hstacks, type, (weed_funcptr_t)finfunc, findata);
  lives_proc_thread_set_cancellable(poller);
  lives_proc_thread_sync_ready(poller);
  return poller;
}


void lives_hook_remove(lives_hook_stack_t **hstacks, int type, lives_proc_thread_t lpt) {
  if (lives_proc_thread_ref(lpt) > 0) {
    if (!hstacks) {
      // test should be HOOK_TYPE_SELF
      if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
      else {
        GET_PROC_THREAD_SELF(self);
        if (!self) return;
        hstacks = lives_proc_thread_get_hook_stacks(self);
        if (!hstacks) return;
      }
    } else {
      lives_closure_t *closure;
      lives_hook_stack_t *hstack = hstacks[type];
      pthread_mutex_t *mutex = hstack->mutex;
      LiVESList *cblist;
      if (!pthread_mutex_trylock(mutex)) {
        for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist->next) {
          closure = (lives_closure_t *)cblist->data;
          if (closure->proc_thread == lpt || (closure->flags & HOOK_STATUS_REMOVE)) {
            if (closure->flags & HOOK_STATUS_RUNNING) closure->flags |= HOOK_STATUS_REMOVE;
            else {
              hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, cblist, FALSE);
              lives_closure_free(closure);
            }
            break;
          }
        }
        pthread_mutex_unlock(mutex);
      } else {
        closure = weed_get_voidptr_value(lpt, LIVES_LEAF_CLOSURE, NULL);
        if (closure) closure->flags |= HOOK_STATUS_REMOVE;
      }
    }
    lives_proc_thread_unref(lpt);
  }
}


void lives_hook_remove_by_data(lives_hook_stack_t **hstacks, int type,
                               lives_funcptr_t func, void *data) {
  if (!hstacks) {
    GET_PROC_THREAD_SELF(self);
    if (!self) return;
    hstacks = lives_proc_thread_get_hook_stacks(self);
    if (!hstacks) return;
  } else {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = hstack->mutex;
    LiVESList *cblist, *cbnext;
    while (1) {
      lives_nanosleep_until_zero(hstack->flags & STACK_TRIGGERING);
      PTMLH;
      if (hstack->flags & STACK_TRIGGERING) PTMUH;
      else break;
    }
    for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cbnext) {
      lives_closure_t *closure = (lives_closure_t *)cblist->data;
      cbnext = cblist->next;
      if (closure) {
        lives_proc_thread_t lpt = closure->proc_thread;
        if (!lpt || !closure->fdef) continue;
        if (closure->fdef->function != func) continue;
        if (data != weed_get_voidptr_value(lpt, PROC_THREAD_PARAM(1), NULL)) continue;
        if (closure->flags & HOOK_STATUS_RUNNING) closure->flags |= HOOK_STATUS_REMOVE;
        else {
          hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, cblist, FALSE);
          lives_closure_free(closure);
        }
      }
    }
    PTMUH;
  }
}

// invalidates cblpt
void lives_hook_remove_by_lpt(lives_hook_stack_t **hstacks, int type,
                              lives_proc_thread_t cblpt) {
  if (!hstacks) {
    GET_PROC_THREAD_SELF(self);
    if (!self) return;
    hstacks = lives_proc_thread_get_hook_stacks(self);
    if (!hstacks) return;
  } else {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = hstack->mutex;
    LiVESList *cblist, *cbnext;
    while (1) {
      lives_nanosleep_until_zero(hstack->flags & STACK_TRIGGERING);
      PTMLH;
      if (hstack->flags & STACK_TRIGGERING) PTMUH;
      else break;
    }
    for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cbnext) {
      lives_closure_t *closure = (lives_closure_t *)cblist->data;
      lives_proc_thread_t lpt2 = closure->proc_thread;
      cbnext = cblist->next;
      if (cblpt == lpt2) {
        hstacks[type]->stack = lives_list_remove_node((LiVESList *)hstacks[type]->stack, cblist, FALSE);
        lives_closure_free(closure);
        break;
      }
    }
    PTMUH;
  }
}


LIVES_GLOBAL_INLINE void lives_hooks_async_join(lives_hook_stack_t *hstack) {
  lives_closure_t *closure;
  pthread_mutex_t *hmutex = hstack->mutex;
  lives_proc_thread_t lpt;
  LiVESList *cblist;
  PTMLH;
  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist->next) {
    closure = (lives_closure_t *)cblist->data;
    if (!closure) continue;
    if (closure->flags & (HOOK_STATUS_BLOCKED | HOOK_STATUS_REMOVE)) {
      // TODO - remove on REMOVE
      continue;
    }
    lpt = closure->proc_thread;
    if (!lpt) continue;

    // TODO - we should do a sync_wait here (for blocked / timeout)
    lives_proc_thread_wait_done(lpt, 0.);
  }
  PTMUH;
}


boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp) {
  funcsig_t fsig1 = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
  int nparms = get_funcsig_nparms(fsig1);
  if (!maxp || maxp > nparms) maxp = nparms;
  for (int i = 0; i < maxp; i++) {
    char *pname = make_std_pname(i);
    if (weed_leaf_elements_equate(lpt1, pname, lpt2, pname, -1) == WEED_FALSE) {
      lives_free(pname);
      return FALSE;
    }
    lives_free(pname);
  }
  return TRUE;
}


static boolean is_child_of(LiVESWidget *w, LiVESContainer *C) {
  if ((LiVESWidget *)w == (LiVESWidget *)C) return TRUE;
  for (LiVESList *list = lives_container_get_children(C); list; list = list->next) {
    LiVESWidget *x = (LiVESWidget *)list->data;
    if (LIVES_IS_CONTAINER(x) && is_child_of(w, LIVES_CONTAINER(x))) return TRUE;
  }
  return FALSE;
}


static boolean fn_match_child(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2) {
  LiVESWidget *w, *C;
  funcsig_t fsig = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
  char *pname, *args_fmt = args_fmt_from_funcsig(fsig);
  if (!args_fmt || get_seedtype(args_fmt[0]) != WEED_SEED_VOIDPTR) {
    if (args_fmt) lives_free(args_fmt);
    return FALSE;
  }
  lives_free(args_fmt);
  pname = make_std_pname(0);
  C = (LiVESWidget *)(weed_get_voidptr_value(lpt2, pname, NULL));
  if (!LIVES_IS_WIDGET(C) || !LIVES_IS_CONTAINER(C)) {
    lives_free(pname);
    return FALSE;
  }
  w = (LiVESWidget *)(weed_get_voidptr_value(lpt1, pname, NULL));
  if (!LIVES_IS_WIDGET(w)) {
    lives_free(pname);
    return FALSE;
  }
  lives_free(pname);
  return is_child_of(w, LIVES_CONTAINER(C));
}


// return -ve value on mismatch-, or nparams on match
// -1 == NULL or fn mismatch
// -2 == return_type mismatch
// -3 == arg_fmt mismatch
int fn_func_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2) {
  if (!lpt1 || !lpt2 || weed_get_funcptr_value(lpt1, LIVES_LEAF_THREADFUNC, NULL)
      != weed_get_funcptr_value(lpt2, LIVES_LEAF_THREADFUNC, NULL)) return -1;
  if (weed_leaf_seed_type(lpt1, _RV_) != weed_leaf_seed_type(lpt2, _RV_)) return -2;
  else {
    funcsig_t fsig = weed_get_int64_value(lpt1, LIVES_LEAF_FUNCSIG, NULL);
    if (fsig != weed_get_int64_value(lpt2, LIVES_LEAF_FUNCSIG, NULL)) return -3;
    return get_funcsig_nparms(fsig);
  }
}


char *cl_flags_desc(uint64_t clflags) {
  char *fstr = lives_strdup("");
  if (clflags & HOOK_CB_BLOCK)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "BLOCKING");
  if (clflags & HOOK_CB_PRIORITY)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "PRIORITY");
  if (clflags & HOOK_OPT_ONESHOT)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "ONESHOT");
  if (clflags & HOOK_CB_FG_THREAD)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "FG_THREAD");
  if (clflags & HOOK_OPT_FG_LIGHT)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "FG_LIGHT");
  if (clflags & HOOK_UNIQUE_FUNC)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "UNIQUE_FUNC");
  if (clflags & HOOK_UNIQUE_DATA)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "UNIQUE_DATA");
  if (clflags & HOOK_INVALIDATE_DATA)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "INVALIDATE_DATA");
  if (clflags & HOOK_OPT_MATCH_CHILD)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "MATCH_CHILD");
  if (clflags & HOOK_STATUS_BLOCKED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "BLOCKED");
  if (clflags & HOOK_STATUS_RUNNING)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "RUNNING");
  if (clflags & HOOK_STATUS_ACTIONED)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "ACTIONED");
  if (clflags & HOOK_STATUS_REMOVE)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "REMOVE");
  return fstr;
}


void dump_hook_stack(lives_hook_stack_t **hstacks, int type) {
  lives_hook_stack_t *hstack;
  pthread_mutex_t *hmutex;
  lives_closure_t *closure;
#ifdef SHOW_FDEFS
  lives_funcdef_t *fdef;
#endif
  lives_proc_thread_t lpt;
  int64_t sta;
  int x = 0;
  g_print("\n\nDUMPING hook stack type %d\n", type);
  if (!hstacks) {
    g_print("NO stacks !\n");
    return;
  }
  hstack = hstacks[type];
  hmutex = hstack->mutex;
  //PTMLH;
  if (!hstack->stack) {
    g_print("Stack empty\n");
    goto done;
  }
  for (LiVESList *cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist->next) {
    g_print("Item %d:\n", x++);
    closure = (lives_closure_t *)cblist->data;
    if (!closure) {
      g_print("NO CLOSURE !!\n");
      continue;
    }
    g_print("retloc = %p, closure flags: 0X%016lX\n", closure->retloc, closure->flags);
    g_print("%s\n", cl_flags_desc(closure->flags));
    lpt = closure->proc_thread;
    if (!lpt) {
      g_print("NO PROC_THREAD !!\n");
      continue;
    }
    if (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
      g_print("\t\t------- REPLACED BY LPT %p\n",
              weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, 0));
    }
    sta = lives_proc_thread_get_state(lpt);
    g_print("lpt: %p, state: 0X%016lX\n(%s)\nfunc call: ",
            lpt, sta, sta ? lives_proc_thread_state_desc(sta) : "NONE");
    g_print("%s\n\n", lives_proc_thread_show_func_call(lpt));
#ifdef SHOW_FDEFS
    fdef = closure->fdef;
    if (!fdef) {
      g_print("No funcdef\n");
      continue;
    }
    g_print("Funcdef: fname = %s, args_fmt '%s', return_type %u\n\n",
            fdef->funcname, fdef->args_fmt, fdef->return_type);
#endif
  }
done:
  //  PTMUH;
  return;
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
    if (!(fdef->flags & LIVES_FDEF_STATIC)) {
      lives_freep((void **)&fdef->funcname);
      lives_freep((void **)&fdef->args_fmt);
      lives_free(fdef);
    }
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

// if io is FN_ALLOC, add a note to ftrace_store or increment the count
// if io is FN_FREE, decrement the count and if zero, remove the note
void *add_fn_note(fn_type_t ftype, ...) {
  va_list va;
  weed_plant_t *note;
  void *ptr = NULL;
  static pthread_mutex_t ftrace_mutex = PTHREAD_MUTEX_INITIALIZER;
  if (ftype == _FN_ALLOC || ftype == _FN_FREE) {
    lives_funcdef_t *fdef = NULL;
    va_start(va, ftype);
    fdef = va_arg(va, lives_funcdef_t *);
    ptr = va_arg(va, void *);
    va_end(va);
    g_print("add fn note type %d for %p\n", ftype, ptr);
    g_print("%s\n", lives_funcdef_explain(fdef));
    g_print("located at line %d in file %s\n", fdef->line, fdef->file);
    if (ptr) {
      pthread_mutex_lock(&ftrace_mutex);
      if (!ftrace_store) ftrace_store = lives_hash_store_new("ftrace");
      note = (weed_plant_t *)get_from_hash_store_i(ftrace_store, (uintptr_t)ptr);
      if (note) {
        // update remove existing entry
        int count = weed_get_int_value(note, "count", NULL);
        if (ftype == _FN_ALLOC) {
          if (count >= 0) weed_set_int_value(note, "count", ++count);
        } else {
          /* if (!(--count)) */
          /*   ftrace_store = remove_from_hash_store_i(ftrace_store, (uintptr_t)ptr); */
          /* else */
          weed_set_int_value(note, "count", --count);
        }
        pthread_mutex_unlock(&ftrace_mutex);
        goto done;
      }
      // add a new entry
      note = lives_plant_new(LIVES_WEED_SUBTYPE_BAG_OF_HOLDING);
      weed_set_string_value(note, "func", fdef->funcname);
      weed_set_string_value(note, "file", fdef->file);
      weed_set_int_value(note, "line", fdef->line);
      weed_set_int_value(note, "count", ftype == _FN_ALLOC ? 1 : -1);
      ftrace_store = add_to_hash_store_i(ftrace_store, (uintptr_t)ptr, (void *)note);
      pthread_mutex_unlock(&ftrace_mutex);
    }
done:
    if (fdef) free_funcdef(fdef);
  }
  return ptr;
}


void dump_fn_notes(void) {
  if (ftrace_store) {
    const char *key;
    char **items = weed_plant_list_leaves(ftrace_store, NULL);
    for (int i = 0; items[i]; i++) {
      if ((key = hash_key_from_leaf_name(items[i]))) {
        weed_plant_t *note = (weed_plant_t *)get_from_hash_store_i(ftrace_store, lives_strtoul(key));
        if (!weed_get_int_value(note, "count", NULL)) continue;
        g_print("0X%016lX from func %s, called from %s, line %d (%d times)\n", lives_strtoul(key),
                weed_get_string_value(note, "func", NULL),
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
