// functions.c
// (c) G. Finch 2002 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

void toggle_var_cb(void *dummy, boolean *var) {if (var) *var = !(*var);}
void inc_counter_cb(void *dummy, int *var) {if (var)(*var)++;}
void dec_counter_cb(void *dummy, int *var) {if (var)(*var)--;}
void resetc_counter_cb(void *dummy, int *var) {if (var) *var = 0;}

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
  case WEED_SEED_VOIDPTR:  {
    if (prefs->show_dev_opts) {
      void *ptr;
      va_list vc;
      va_copy(vc, xargs);
      ptr = va_arg(vc, void *);
      va_end(vc);
      if (ptr && isstck(ptr)) g_print("Warning - wlfv, key = %s, isstack = 1\n", key);

    }
    return SET_LEAF_FROM_VARG(plant, key, voidptr, ne, xargs);
  }
  case WEED_SEED_PLANTPTR: return SET_LEAF_FROM_VARG(plant, key, plantptr, ne, xargs);
  default: return WEED_ERROR_WRONG_SEED_TYPE;
  }
}


LIVES_GLOBAL_INLINE lives_result_t weed_leaf_from_va(weed_plant_t *plant, const char *key, char fmtchar, ...) {
  va_list xargs;
  weed_error_t err;
  uint32_t st = get_seedtype(fmtchar);
  va_start(xargs, fmtchar);
  err = weed_leaf_from_varg(plant, key, st, 1, xargs);
  va_end(xargs);
  if (err != WEED_SUCCESS) return LIVES_RESULT_ERROR;
  return LIVES_RESULT_SUCCESS;
}



LIVES_LOCAL_INLINE uint32_t _char_to_st(char c) {
  // letter to seed_type
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].letter == c) return crossrefs[i].seed_btype;
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
      return crossrefs[i].seed_btype;
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
    if (crossrefs[i].seed_btype == st) return crossrefs[i].letter;
  }
  return '\0';
}

LIVES_GLOBAL_INLINE const char *get_fmtstr_for_st(uint32_t st) {
  // letter to sigbits
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].seed_btype == st) return crossrefs[i].fmtstr;
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


LIVES_GLOBAL_INLINE void dump_fn_stack(LiVESList *fnstack) {
  LiVESList *list = fnstack;
  if (list) {
    while (1) {
      g_printerr("%s", (char *)list->data);
      list = list->next;
      if (!list) break;
      g_printerr(" ->\n");
    }
    g_printerr("\n");
  }
}

static weed_plant_t *fn_looker = NULL;

static void add_quick_fn(lives_funcptr_t func, const char *funcname) {
  char *key;
  if (sizeof(lives_funcptr_t) != sizeof(void *)) return;
  key = lives_strdup_printf("%p", func);
  if (!fn_looker) fn_looker = lives_plant_new(LIVES_WEED_SUBTYPE_INDEX);
  weed_set_voidptr_value(fn_looker, key, (void *)funcname);
  lives_free(key);
}


const char *get_funcname(lives_funcptr_t func) {
  const char *fname;
  char *key;
  if (!fn_looker) return NULL;
  key = lives_strdup_printf("%p", func);
  fname = weed_get_voidptr_value(fn_looker, key, NULL);
  lives_free(key);
  return fname;
}


LIVES_GLOBAL_INLINE void _func_entry(lives_funcptr_t func, const char *funcname, int cat,
                                     const char *rettype, const char *args_fmt,
                                     char *file_ref, int line_ref) {
  THREADVAR(func_stack) = lives_list_prepend(THREADVAR(func_stack),
                          (void *)lives_strdup(funcname));
  g_printerr("Thread 0x%lx in func %s at %s, line %d\n",
             THREADVAR(uid), funcname, file_ref, line_ref);
  add_quick_fn(func, funcname);
  /* add_fn_lookup(func, funcname, cat, rettype, args_fmt, file_ref, line_ref); */
}


LIVES_GLOBAL_INLINE void _func_exit(char *file_ref, int line_ref) {
  LiVESList *list = THREADVAR(func_stack);
  g_print("Thread 0x%lx exiting func %s @ line %d, %s\n", THREADVAR(uid),
          (char *)list->data, line_ref, file_ref);
  THREADVAR(func_stack) = list->next;
  if (list->next) list->next = list->next->prev = NULL;
  lives_list_free_all(&list);
}


LIVES_GLOBAL_INLINE void _func_exit_val(weed_plant_t *pl, char *file_ref, int line_ref) {
  LiVESList *list = THREADVAR(func_stack);
  g_print("Thread 0x%lx exiting func %s @ line %d, %s\n", THREADVAR(uid),
          (char *)list->data, line_ref, file_ref);

  THREADVAR(func_stack) = list->next;
  if (list->next) list->next = list->next->prev = NULL;
  lives_list_free_all(&list);
}


boolean have_recursion_token(uint32_t tok) {
  for (LiVESList *list = THREADVAR(trest_list); list; list = list->next)
    if (LIVES_POINTER_TO_INT(list->data) == tok) return TRUE;
  return FALSE;
}


void push_recursion_token(uint32_t tok) {
  LiVESList *list = THREADVAR(trest_list);
  THREADVAR(trest_list) = lives_list_prepend(list, LIVES_INT_TO_POINTER(tok));
}


void remove_recursion_token(uint32_t tok) {
  LiVESList *list = THREADVAR(trest_list);
  if (list) THREADVAR(trest_list) = lives_list_remove_data(list, LIVES_INT_TO_POINTER(tok), FALSE);
}

///////////////////////////

LIVES_LOCAL_INLINE char *make_std_pname(int pn) {return lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, pn);}

static boolean is_child_of(LiVESWidget *w, LiVESContainer *C);
static boolean fn_match_child(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2);

static lives_result_t weed_plant_params_from_valist(weed_plant_t *plant, const char *args_fmt, \
    make_key_f param_name, va_list xargs) {
  int p = 0;
  for (const char *c = args_fmt; *c; c++) {
    char *pkey = (*param_name)(p++);
    uint32_t st = _char_to_st(*c);
    weed_error_t err = weed_leaf_from_varg(plant, pkey, st, 1, xargs);
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
    int pn = 0, pnn ;
    for (int i = 60; i >= 0; i -= 4) {
      uint8_t ch = (sig >> i) & 0X0F;
      pnn = pn;
      if (!ch) continue;
      str = lives_strdup_concat(str, " ", "%sp%d",
                                weed_seed_to_ctype(get_seedtype(ch), TRUE), pn++);
      for (int k = i - 4; k >= 0; k -= 4) {
        uint8_t tch = (sig >> k) & 0X0F;
        if (tch == ch) {
          // group params of same type
          if (k == i - 4) {
            // skip over sequential params of same type
            i = k;
            pn++;
          }
          str = lives_strdup_concat(str, ", ", "p%d", pnn);
          // zero out so we dont end up repeating a type
          sig ^= tch << k;
        }
        pnn++;
      }
      str = lives_strdup_concat(str, NULL, ";");
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


LIVES_GLOBAL_INLINE lives_funcdef_t *lives_proc_thread_to_funcdef(lives_proc_thread_t lpt) {
  lives_funcdef_t *fdef;
  char *funcname, *args_fmt;
  if (!lpt) return NULL;
  funcname = lives_proc_thread_get_funcname(lpt);
  args_fmt = lives_proc_thread_get_args_fmt(lpt);
  fdef = create_funcdef(funcname, weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL),
                        weed_leaf_seed_type(lpt, _RV_), args_fmt, NULL, 0, NULL);
  lives_free(funcname);
  lives_free(args_fmt);
  return fdef;
}


#define _IF_(pn)if (pn) lives_free(pn)

#define _DC_(n, ...) do {if (ret_type) XCALL_##n(lpt, ret_type, &thefunc, __VA_ARGS__); \
    else CALL_VOID_##n(lpt, &thefunc, __VA_ARGS__);} while (0);

lives_result_t do_call(lives_proc_thread_t lpt) {
  // The compiler needs to know the types of the variables being passed as parameters
  // and the return type,
  // hence we cannot create a macro that just returns a value of the correct type and use this to fill in
  // the function parameters (unless the function is variadic)
  //. For this reason, the number and types of parameters have to be declared as literals
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

  weed_error_t err = WEED_SUCCESS;
  weed_funcptr_t func = weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  if (!func) return LIVES_RESULT_ERROR;

  uint32_t ret_type = weed_leaf_seed_type(lpt, _RV_);

  funcsig_t sig = lives_proc_thread_get_funcsig(lpt);

  int nparms = get_funcsig_nparms(sig);

  allfunc_t thefunc;

  thefunc.func = func;

  // compressed lines as this could become quite long...
  switch (nparms) {
  case 0:_DC_(0,);break;

  case 1:
    switch (sig) {
    case FUNCSIG_INT:{int p0;_DC_(1,int);}break;
    case FUNCSIG_BOOL:{boolean p0;_DC_(1,boolean);}break;
    case FUNCSIG_INT64:{int64_t p0;_DC_(1,int64);}break;
    case FUNCSIG_DOUBLE:{double p0;_DC_(1,double);}break;
    case FUNCSIG_STRING:{char*p0 = NULL;_DC_(1,string);_IF_(p0);}break;
    case FUNCSIG_VOIDP:{void*p0;_DC_(1,voidptr);}break;
    case FUNCSIG_PLANTP:{weed_plant_t*p0;_DC_(1,plantptr);}break;
    // undefined funcsig
    default: return LIVES_RESULT_ERROR;}
   break;

  case 2:
    switch (sig) {
    case FUNCSIG_INT_INT:{int p0,p1;_DC_(2,int,int);}break;
    case FUNCSIG_BOOL_INT:{int p0,p1;_DC_(2,int,int);}break;
    case FUNCSIG_INT_VOIDP:{int p0;void*p1;_DC_(2,int,voidptr);}break;
    case FUNCSIG_STRING_INT:{char*p0 = NULL;int p1;_DC_(2,string,int);_IF_(p0);}break;
    case FUNCSIG_STRING_BOOL:{char*p0 = NULL;int p1;_DC_(2,string,boolean);_IF_(p0);}break;
    case FUNCSIG_DOUBLE_DOUBLE:{double p0,p1;_DC_(2,double,double);}break;
    case FUNCSIG_VOIDP_DOUBLE:{void*p0;double p1;_DC_(2,voidptr,double);}break;
    case FUNCSIG_VOIDP_INT:{void*p0;int p1;_DC_(2,voidptr,int);}break;
    case FUNCSIG_VOIDP_INT64:{void*p0;int64_t p1;_DC_(2,voidptr,int64);}break;
    case FUNCSIG_VOIDP_VOIDP:{void*p0,*p1;_DC_(2,voidptr,voidptr);}break;
    case FUNCSIG_PLANTP_VOIDP:{weed_plant_t*p0;void*p1;_DC_(2,plantptr,voidptr);}break;
    case FUNCSIG_VOIDP_BOOL:{void*p0;int p1;_DC_(2,voidptr,boolean);}break;
    case FUNCSIG_VOIDP_STRING:{void*p0;char*p1 = NULL;_DC_(2,voidptr,string);_IF_(p1);}break;
    // undefined funcsig
    default: return LIVES_RESULT_ERROR;}
   break;

  case 3:
    switch (sig) {
    case FUNCSIG_VOIDP_VOIDP_VOIDP:{void*p0,*p1,*p2;_DC_(3,voidptr,voidptr,voidptr);}break;
    case FUNCSIG_VOIDP_VOIDP_BOOL:{void*p0,*p1;int p2;_DC_(3,voidptr,voidptr,boolean);}break;
    case FUNCSIG_STRING_VOIDP_VOIDP:{char*p0=NULL;void*p1,*p2;_DC_(3,string,voidptr,voidptr);_IF_(p0);}break;
    case FUNCSIG_VOIDP_DOUBLE_INT:{void*p0;double p1;int p2;_DC_(3,voidptr,double,int);}break;
    case FUNCSIG_VOIDP_DOUBLE_DOUBLE:{void*p0;double p1,p2;_DC_(3,voidptr,double,double);}break;
    case FUNCSIG_PLANTP_VOIDP_INT64:{weed_plant_t*p0;void*p1;int64_t p2;_DC_(3,plantptr,voidptr,int64);}break;
    case FUNCSIG_INT_INT_BOOL:{int p0,p1,p2;_DC_(3,int,int,boolean);}break;
    case FUNCSIG_STRING_INT_BOOL:{char*p0=NULL;int p1,p2;_DC_(3,string,int,boolean);_IF_(p0);}break;
    case FUNCSIG_INT_INT64_VOIDP:{int p0;int64_t p1;void*p2;_DC_(3,int,int64,voidptr);}break;
    // undefined funcsig
    default: return LIVES_RESULT_ERROR;}
   break;

  case 4:
    switch (sig) {
    case FUNCSIG_STRING_DOUBLE_INT_STRING:{char*p0=NULL,*p3=NULL;double p1;int p2;_DC_(4,string,double,int,string);
      _IF_(p0);_IF_(p3);}break;
    case FUNCSIG_INT_INT_BOOL_VOIDP:{int p0,p1,p2;void*p3;_DC_(4,int,int,boolean,voidptr);}break;
    case FUNCSIG_VOIDP_INT_FUNCP_VOIDP:{void*p0,*p3;int p1;weed_funcptr_t p2;_DC_(4,voidptr,int,funcptr,voidptr);}break;
    // undefined funcsig
    default: return LIVES_RESULT_ERROR;}
   break;

  case 5:
    switch (sig) {
    case FUNCSIG_VOIDP_STRING_STRING_INT64_INT:{void*p0;char*p1 = NULL,*p2 = NULL;int64_t p3;int p4;
      _DC_(5,voidptr,string,string,int64,int);_IF_(p1);_IF_(p2);}break;
    case FUNCSIG_INT_INT_INT_BOOL_VOIDP:{int p0,p1,p2,p3;void*p4;_DC_(5,int,int,int,boolean,voidptr);}break;
    case FUNCSIG_VOIDP_INT_INT_INT_INT:{void*p0;int p1,p2,p3,p4;_DC_(5,voidptr,int,int,int,int);}break;
    case FUNCSIG_VOIDP_VOIDP_BOOL_BOOL_INT:{void*p0,*p1;int p2,p3,p4;_DC_(5,voidptr,voidptr,boolean,boolean,int);}break;
    /*   // undefined funcsig*/
    default: return LIVES_RESULT_ERROR;}
   break;

  case 6:
    switch (sig) {
    case FUNCSIG_STRING_STRING_VOIDP_INT_STRING_VOIDP:{char*p0=NULL,*p1=NULL,*p4=NULL;void*p2,*p5;int p3;
	_DC_(6,string,string,voidptr,int,string,voidptr);_IF_(p0);_IF_(p1);_IF_(p4);}break;
    // undefined funcsig
    default: return LIVES_RESULT_ERROR;}
   break;
  // invalid nparms
  default: return LIVES_RESULT_ERROR;
  }
  if (err != WEED_SUCCESS) return LIVES_RESULT_FAIL;
  return LIVES_RESULT_SUCCESS;
}


#undef _IF_
#undef _DC_

//#define DEBUG_FN_CALLBACKS
static boolean _call_funcsig_inner(lives_proc_thread_t lpt) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint64_t attrs = lives_proc_thread_get_attrs(lpt);
  char *msg;
  weed_error_t err = WEED_SUCCESS;
  int nparms;
  lives_result_t ret;
#ifdef DEBUG_FN_CALLBACKS
  lives_closure_t *closure;
  const lives_funcdef_t *funcdef;
#endif
  funcsig_t sig;

  if (!lpt || lives_proc_thread_ref(lpt) < 2) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid proc_thread");
    return FALSE;
  }

  weed_funcptr_t func = weed_get_funcptr_value(lpt, LIVES_LEAF_THREADFUNC, NULL);
  if (!func) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid function");
    return FALSE;
  }

  // STATE CHANGED - queued / preparing -> running
  lives_proc_thread_include_states(lpt, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_QUEUED | THRD_STATE_UNQUEUED | THRD_STATE_DEFERRED |
                                   THRD_STATE_PREPARING);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_START_TICKS, lives_get_current_ticks());

  ret = do_call(lpt);

  if (weed_plant_has_leaf(lpt, LIVES_LEAF_LONGJMP))
    weed_leaf_delete(lpt, LIVES_LEAF_LONGJMP);

  lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS) {
    weed_set_int64_value(lpt, LIVES_LEAF_END_TICKS, lives_get_current_ticks());
  }

  if (ret == LIVES_RESULT_ERROR) goto funcerr;
      
  if (lives_proc_thread_get_cancel_requested(lpt)
      && !lives_proc_thread_was_cancelled(lpt)) {
    lives_proc_thread_cancel(lpt);
  }

  if (err == WEED_SUCCESS) {
    if (lpt == mainw->debug_ptr)
      g_print("pt a1\n");
    lives_proc_thread_unref(lpt);
    if (lpt == mainw->debug_ptr)
      g_print("pt a122\n");
    return TRUE;
  }
  msg = lives_strdup_printf("Got error %d running procthread ", err);
  goto funcerr2;

funcerr:
  // invalid args_fmt
  sig = lives_proc_thread_get_funcsig(lpt);
  nparms = get_funcsig_nparms(sig);
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

funcerr2:

  if (!msg)
    msg = lives_strdup_printf("Got error %d running function with type 0x%016lX (%lu)", err, sig, sig);

  lives_proc_thread_error(lpt, 0, LPT_ERR_CRITICAL, msg);

  /* LIVES_ERROR(msg); */
  /* lives_free(msg); */
  /* lives_proc_thread_unref(lpt); */
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t lpt) {
  _call_funcsig_inner(lpt);
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
      fmtstring = lives_strdup_printf("(%s) %s(%s);",
                                      weed_seed_to_ctype(ret_type, TRUE), funcname, parvals);
    } else fmtstring = lives_strdup_printf("(void) %s(%s);", funcname, parvals);
    lives_free(funcname); lives_free(parvals);
    return fmtstring;
  }
  return NULL;
}


char *func_category_to_text(int cat) {
  switch (cat) {
  case FUNC_CATEGORY_HOOK_COMMON:
    return lives_strdup(_("hook callback"));
  case FUNC_CATEGORY_HOOK_GUI:
    return lives_strdup(_("GUI hook callback"));
  case FUNC_CATEGORY_HOOK_SYNC:
    return lives_strdup(_("sync hook callback"));
  case FUNC_CATEGORY_UTIL:
    return lives_strdup(_("utility function"));
  default:
    return lives_strdup(_("generic function"));
  }
}


static int lives_fdef_get_category(int cat, int dtl) {
  int category = FUNC_CATEGORY_GENERAL;
  switch (cat) {
  case FUNC_CATEGORY_HOOK_COMMON:
    switch (dtl) {
    case LIVES_GUI_HOOK:
      category = FUNC_CATEGORY_HOOK_GUI; break;
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
  if (lpt && lives_proc_thread_ref(lpt) > 1) {
    lives_closure_t *closure  = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
    pthread_mutex_init(&closure->mutex, NULL);
    closure->fdef = lives_proc_thread_make_funcdef(lpt);
    ((lives_funcdef_t *)closure->fdef)->category
      = lives_fdef_get_category(FUNC_CATEGORY_HOOK_COMMON, hook_type);
    closure->proc_thread = lpt;
    closure->flags = flags;
    closure->retloc = weed_get_voidptr_value(lpt, LIVES_LEAF_RETLOC, NULL);
    weed_set_voidptr_value(lpt, LIVES_LEAF_CLOSURE, closure);
    lives_proc_thread_unref(lpt);
    return closure;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE lives_closure_t *lives_proc_thread_get_closure(lives_proc_thread_t lpt) {
  return lpt ? weed_get_voidptr_value(lpt, LIVES_LEAF_CLOSURE, NULL) : NULL;
}



static void _remove_ext_cb(lives_proc_thread_t lpt,
                           lives_closure_t *cl) {
  LiVESList *xlist, *list, *listnext;
  cl->adder = NULL;
  cl->flags |= HOOK_STATUS_REMOVE;
  xlist = (LiVESList *)weed_get_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, NULL);
  if (xlist) {
    for (list = xlist; list; list = listnext) {
      listnext = list->next;
      if (cl == (lives_closure_t *)list->data) {
        weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST,
                               (void *)lives_list_remove_node(xlist, list, FALSE));
        break;
      }
    }
  }
}


static void remove_ext_cb(lives_closure_t *cl) {
  // when freeing a closure, remove the pointer to it from the adder
  // state mutex for cl->proc_thread MUST be loecked, to avois a race where
  // cl->adder is flushing its list
  if (cl) {
    lives_proc_thread_t lpt = cl->adder;
    if (lpt) {
      pthread_mutex_t *state_mutex = weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
      if (state_mutex) {
        pthread_mutex_lock(state_mutex);
        if (cl->adder == lpt) _remove_ext_cb(lpt, cl);
        pthread_mutex_unlock(state_mutex);
      }
    }
  }
}


LIVES_GLOBAL_INLINE void lives_closure_free(lives_closure_t *closure) {
  // with state mutex locked
  if (closure) {
    lives_proc_thread_t lpt;
    pthread_mutex_lock(&closure->mutex);
    if (closure->adder) remove_ext_cb(closure);
    lpt = closure->proc_thread;
    closure->proc_thread = NULL;
    int nrefs = lives_proc_thread_ref(lpt);
    if (lpt && nrefs > 1) {
      // do this to avoid error check in unref
      if (weed_plant_has_leaf(lpt, LIVES_LEAF_CLOSURE))
        weed_leaf_delete(lpt, LIVES_LEAF_CLOSURE);

      // this removes our added ref and MAY free lpt if not reffed elsewhere
      nrefs = lives_proc_thread_unref(lpt);
      nrefs = lives_proc_thread_unref(lpt);
    }
  }
  if (closure->fdef)
    free_funcdef((lives_funcdef_t *)closure->fdef);
  pthread_mutex_unlock(&closure->mutex);
  lives_free(closure);
}



////// hook functions /////

LIVES_GLOBAL_INLINE void lives_hooks_clear(lives_hook_stack_t **hstacks, int type) {
  if (hstacks) {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = &(hstack->mutex);
    LiVESList *hsstack;

    while (1) {
      lives_microsleep_until_zero(hstack->flags & STACK_TRIGGERING);
      PTMLH;
      if (hstack->flags & STACK_TRIGGERING) PTMUH;
      else break;
    }

    hsstack = (LiVESList *)hstack->stack;
    if (hsstack) {
      for (LiVESList *cblist = hsstack; cblist; cblist = cblist->next) {
        lives_closure_t *cl = (lives_closure_t *)cblist->data;
        if (cl) lives_closure_free(cl);
      }
      lives_list_free(hsstack);
      hstack->stack = NULL;
    }
    PTMUH;
  }
}


LIVES_GLOBAL_INLINE void lives_hooks_clear_all(lives_hook_stack_t **hstacks, int ntypes) {
  if (hstacks)
    for (int i = 0; i < ntypes; i++) {
      lives_hooks_clear(hstacks, i);
      lives_free(hstacks[i]);
    }
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
  pthread_mutex_t *state_mutex = weed_get_voidptr_value(self, LIVES_LEAF_STATE_MUTEX, NULL);
  if (state_mutex && !pthread_mutex_lock(state_mutex)) {
    LiVESList *ext_cbs = (LiVESList *)weed_get_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, NULL);
    weed_set_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, lives_list_prepend(ext_cbs, (void *)closure));
    pthread_mutex_unlock(state_mutex);
  }
}


void flush_cb_list(lives_proc_thread_t lpt) {
  // need to call this when clearing a hook stack
  // it will check each closure for adder
  // and call remove_ext_cb
  //
  // so: flush_list -
  // vice versa, when a lpt exits, it will lock its state, go throught the list
  // trylock for the first closure, on failing it will start over again
  // if it gets the lock it will set adder to null and flag for removel

  pthread_mutex_t *state_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_STATE_MUTEX, NULL);
  pthread_mutex_lock(state_mutex);

  while (1) {
    LiVESList *xlist = (LiVESList *)weed_get_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, NULL);
    lives_closure_t *cl;
    if (!xlist) break;
    cl = (lives_closure_t *)xlist->data;
    if (!cl) {
      xlist = lives_list_remove_node(xlist, xlist, FALSE);
      weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, xlist);
      continue;
    }
    // now we need to trylock the closure, if we cannot get it, it means the owner is about to
    // delete the entry itself and is wating on state mutex
    // so we will drop state_mutex, regain it, then start over parsing the list
    if (pthread_mutex_trylock(&cl->mutex)) {
      pthread_mutex_unlock(state_mutex);
      pthread_yield();
      // allow the stack owner to delete this
      pthread_mutex_lock(state_mutex);
      continue;
    }
    cl->adder = NULL;


    cl->flags |= HOOK_STATUS_REMOVE;
    pthread_mutex_unlock(&cl->mutex);
    xlist = lives_list_remove_node(xlist, xlist, FALSE);
    weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, xlist);
  }
  pthread_mutex_unlock(state_mutex);
}


void remove_from_hstack(lives_hook_stack_t *hstack, LiVESList *list) {
  // should be called with hstack mutex LOCKED !
  // remove list from htack, free the closure
  // (which will unref the proc_thread in it)
  // lpt will be FREED unless reffed elsewhere !
  lives_closure_t *closure = (lives_closure_t *)list->data;
  if (list->prev) list->prev->next = list->next;
  else hstack->stack = (volatile LiVESList *)list->next;
  if (list->next) list->next->prev = list->prev;
  list->next = list->prev = NULL;
  lives_closure_free(closure);
  lives_list_free(list);
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
  uint64_t lpt_attrs;
  lives_closure_t *closure, *xclosure = NULL, *ret_closure = NULL;
  GET_PROC_THREAD_SELF(self);
  uint64_t xflags = flags & (HOOK_UNIQUE_REPLACE | HOOK_INVALIDATE_DATA | HOOK_TOGGLE_FUNC);
  pthread_mutex_t *hmutex;
  boolean is_close = FALSE, is_append = TRUE, is_remove = FALSE;
  boolean have_lock = FALSE;
  boolean is_self = FALSE;

  if (!data) return NULL;

  if (!hstacks) {
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      if (!self) return NULL;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return NULL;
      is_self = TRUE;
    }
  } else if (hstacks == lives_proc_thread_get_hook_stacks(self))
    is_self = TRUE;

  if (dtype & DTYPE_CLOSURE) is_close = TRUE;

  if (dtype & DTYPE_PREPEND) is_append = FALSE;
  if (dtype & DTYPE_NOADD) is_remove = TRUE;

  if (dtype & DTYPE_HAVE_LOCK) have_lock = TRUE;

  // append, then everything else will check
  if (is_append) xflags &= ~(HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);

  // is_close,is for here
  // the important flags for checking are is_append, is_remove

  if (is_close) {
    xclosure = (lives_closure_t *)data;
    lpt = xclosure->proc_thread;
    if (!lpt) return NULL;
    if (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
      while (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
        lpt =  weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, 0);
      }
      return lpt;
    }
  } else {
    lpt = (lives_proc_thread_t)data;
    if (!is_append) xclosure = lives_hook_closure_new_for_lpt(lpt, flags, type);
  }

  if (!is_append) ret_closure = xclosure;

  hmutex = &(hstacks[type]->mutex);

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
          lives_millisleep;
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
      if (weed_plant_has_leaf(lpt2, LIVES_LEAF_REPLACEMENT)) {
        while (weed_plant_has_leaf(lpt2, LIVES_LEAF_REPLACEMENT)) {
          lpt2 = weed_get_plantptr_value(lpt2, LIVES_LEAF_REPLACEMENT, 0);
        }
        closure = lives_proc_thread_get_closure(lpt2);
        if (!closure) continue;
      }

      if (lives_proc_thread_is_queued(lpt2)) continue;
      if (!lpt2) break_me("null procthread in closure\n");
      if (!lpt2 || lpt2 == lpt) continue;

      // check uniqueness restrictions when adding a new callback
      if (is_append) {
        cfinv = closure->flags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
        if (!(cfinv & HOOK_INVALIDATE_DATA)) cfinv = 0;
      }

      // check if the function matches (unless we are just invalidating data)
      if (!(fmatch = (fn_func_match(lpt, lpt2) >= 0))
          && !((xflags & HOOK_INVALIDATE_DATA) || cfinv != 0)) continue;

      if (fmatch && (xflags & (HOOK_UNIQUE_FUNC | HOOK_TOGGLE_FUNC))) {
        // if function matches, and UNIQUE_FUNC was set, then we may need to remove this
        // or not append.
        //
        // unique_func -> maintain only 1st fn match
        // with unique data, -> also replace 1st with our data
        if (xflags & HOOK_UNIQUE_DATA) {
          // for unique func / unique data, if we are appending, we check if the match part
          // of the params is equal, and if so, we replace the remaining params
          // after this we remove any others with matching func / data
          //
          if (!fn_data_match(lpt2, lpt, maxp)) continue;
          if (!ret_closure) {
            ret_closure = closure;
            // skip over if the target stack is triggering
            if (hstacks[type]->flags & STACK_TRIGGERING) break;
            if (!(xflags & HOOK_TOGGLE_FUNC)) {
              if (!fn_data_match(lpt2, lpt, 0)) {
                fn_data_replace(lpt2, lpt);
                // data replaced
              }
              continue;
            }
            // toggle - fall thru and remove
          }
        }
        // if we reach here, it means that we got a matching function,
        // and either we got a data match and must remove the node
        // or else unique_data was not flagged

        if (!ret_closure) {
          // if this is the first match, then we wont remove it, unless fn toggles
          ret_closure = closure;
          if (!(xflags & HOOK_TOGGLE_FUNC)) {
            if (is_append) break;
            continue;
          }
          if (hstacks[type]->flags & STACK_TRIGGERING) {
            ret_closure = NULL;
            break;
          }
        }

        // here we have established that the closure must be removed
        // since we already found a match, or we will prepend, or it is a "toggle" function
        closure->flags |= HOOK_STATUS_REMOVE;

        if (xflags & HOOK_TOGGLE_FUNC) {
          // for toggle func, this closure will be removed, and new lpt will be rejected
          // so neither will be replaced, but marking the proc_threads as "invalid" we force the waiters to give  up
          // but we will return lpt2 (we need to return somehting different)
          // but we will flag it as invalid,
          lives_proc_thread_set_state(closure->proc_thread, (THRD_STATE_INVALID | THRD_STATE_DESTROYING |
                                      THRD_STATE_STACKED));
          break;
        }

        if (closure->flags & HOOK_CB_BLOCK) {
          // if replacing a closure with BLOCKing flagged, we need to get the blocked threads to wait on
          // the closure which replaced theirs. We will return a pointer the the replacement
          // and we also need to add a ref to lpt2, since the waiter will unref it,
          // and it will also be unreffed when the closure is freed
          // we will also add a ref to ret_closure->proc_thread, for similar reasons
          if (ret_closure != closure) {
            lives_proc_thread_ref(lpt2);
            lives_proc_thread_ref(ret_closure->proc_thread);
            ret_closure->flags |= HOOK_CB_BLOCK;
            weed_set_plantptr_value(lpt2, LIVES_LEAF_REPLACEMENT, ret_closure->proc_thread);
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
      // we wont remove anything, but we will reject an append
      if (!ret_closure) {
        ret_closure = closure;
        break;
      }

      closure->flags |= HOOK_STATUS_REMOVE;
      continue;
    }
  }

  if (is_remove) {
    if (!have_lock) PTMUH;
    return lpt;
  }

  if (ret_closure && ret_closure != xclosure) {
    if (!have_lock) PTMUH;
    if (flags & HOOK_CB_BLOCK) {
      if (!weed_plant_has_leaf(ret_closure->proc_thread, LIVES_LEAF_REPLACEMENT)) {
        lives_proc_thread_ref(ret_closure->proc_thread);
        ret_closure->flags |= HOOK_CB_BLOCK;
      }
      lives_proc_thread_ref(lpt);
      weed_set_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, ret_closure->proc_thread);
      lives_proc_thread_set_state(lpt, THRD_STATE_INVALID);
    }
    return ret_closure->proc_thread;
  }

  // setting this state ensures the proc_thread is not freed, even if set to dontcare or has no return type/value
  lives_proc_thread_include_states(lpt, THRD_STATE_STACKED);

  // just to be doubly sure...
  lpt_attrs = lives_proc_thread_get_attrs(lpt);
  lpt_attrs |= LIVES_THRDATTR_NO_UNREF;
  lives_proc_thread_set_attrs(lpt, lpt_attrs);

  closure = NULL;
  if (is_close || !is_append) closure = xclosure;

  if (!closure) {
    closure = lives_hook_closure_new_for_lpt(lpt, flags, type);
  }

  if (!is_self && !(flags & HOOK_CB_TRANSFER_OWNER)) {
    // add a pointer to the callback if we added it to the hook stack for another thread
    // this is done so that we can remove any external callbacks when the proc_thread is freed
    // however, we don't do this for self hooks (we can simply clear those)
    // also we dont do this if transferring ownership of the callback (as when adding to the
    // player's sync_announce stack)
    closure->adder = self;
    add_to_cb_list(self, closure);
  }

  if (is_append) hstacks[type]->stack = lives_list_append((LiVESList *)hstacks[type]->stack, closure);
  else hstacks[type]->stack = lives_list_prepend((LiVESList *)hstacks[type]->stack, closure);

  closure->hook_stacks = hstacks;
  closure->hook_type = type;

  if (!have_lock) PTMUH;

  if (lpt_attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());

  return lpt;
}


lives_proc_thread_t lives_hook_add_full(lives_hook_stack_t **hooks, int type, uint64_t flags,
                                        lives_funcptr_t func, const char *fname, int return_type,
                                        const char *args_fmt, ...) {
  lives_proc_thread_t lpt, lpt2;
  uint64_t attrs = LIVES_THRDATTR_START_UNQUEUED;
  uint64_t dtype = 0;

  if (flags & HOOK_CB_PRIORITY) dtype |= DTYPE_PREPEND;

  if (args_fmt) {
    va_list xargs;
    va_start(xargs, args_fmt);
    lpt = _lives_proc_thread_create_vargs(attrs, func, fname, return_type, args_fmt, xargs);
    va_end(xargs);
  } else lpt = _lives_proc_thread_create_nullvargs(attrs, func, fname, return_type);

  lpt2 = lives_hook_add(hooks, type, flags, (void *)lpt, dtype);
  if (lpt2 != lpt) {
    lives_proc_thread_unref(lpt);
    lpt = lpt2;
  }
  return lpt;
}


static lives_proc_thread_t update_linked_stacks(lives_closure_t *cl, uint64_t flags) {
  uint64_t dflags = DTYPE_NOADD | DTYPE_PREPEND | DTYPE_HAVE_LOCK | DTYPE_CLOSURE;
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  if (!is_fg_thread()) {
    GET_PROC_THREAD_SELF(self); lives_hook_stack_t **mystacks = lives_proc_thread_get_hook_stacks(self);
    lives_microsleep_until_zero(pthread_mutex_lock(&mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex));
    if (!(mainw->global_hook_stacks[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING))
      lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
    pthread_mutex_unlock(&mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);
    for (LiVESList *links = mainw->all_hstacks; links; links = links->next) {
      lives_hook_stack_t **xhs = (lives_hook_stack_t **)links->data;
      if (xhs == mystacks) continue;
      lives_microsleep_until_zero(pthread_mutex_lock(&xhs[LIVES_GUI_HOOK]->mutex));
      if (xhs[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING) {
        pthread_mutex_unlock(&xhs[LIVES_GUI_HOOK]->mutex);
        continue;
      }
      lives_hook_add(xhs, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
      pthread_mutex_unlock(&xhs[LIVES_GUI_HOOK]->mutex);
    }
  } else {
    for (LiVESList *links = mainw->all_hstacks; links; links = links->next) {
      lives_hook_stack_t **xhs = (lives_hook_stack_t **)links->data;
      lives_microsleep_until_zero(pthread_mutex_lock(&xhs[LIVES_GUI_HOOK]->mutex));
      if (xhs[LIVES_GUI_HOOK]->flags & STACK_TRIGGERING) {
        pthread_mutex_unlock(&xhs[LIVES_GUI_HOOK]->mutex);
        continue;
      }
      lives_hook_add(xhs, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
      pthread_mutex_unlock(&xhs[LIVES_GUI_HOOK]->mutex);
    }
  }
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);
  return NULL;
}


boolean lives_hooks_trigger(lives_hook_stack_t **hstacks, int type) {
  static pthread_mutex_t recheck_mutex = PTHREAD_MUTEX_INITIALIZER;
  lives_hook_stack_t *hstack;
  lives_proc_thread_t lpt, wait_parent;
  LiVESList *list, *listnext;
  lives_closure_t *closure;
  pthread_mutex_t *hmutex;
  boolean bret;
  boolean retval = TRUE;
  boolean have_recheck_mutex = FALSE;
  boolean hmulocked = FALSE;

  //if (type == SYNC_ANNOUNCE_HOOK) dump_hook_stack(hstacks, type);

  if (!hstacks) {
    // test should be HOOK_TYPE_SELF
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      GET_PROC_THREAD_SELF(self);
      if (!self) return TRUE;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return TRUE;
    }
  }

  if (type == DATA_READY_HOOK) {
    lives_hooks_trigger_async(hstacks, type);
    return TRUE;
  }

  hstack = hstacks[type];
  hmutex = &(hstack->mutex);
  //if (type == SYNC_WAIT_HOOK) dump_hook_stack(hstacks, type);

  /* if (type == LIVES_GUI_HOOK && is_fg_thread()) { */
  /*   if (PTMTLH) return TRUE; */
  /* } else */

  PTMLH;
  hmulocked = TRUE;

  if (!hstack->stack || (hstack->flags & STACK_TRIGGERING)) {
    PTMUH;
    hmulocked = FALSE;
    goto trigdone;
  }

  hstack->flags |= STACK_TRIGGERING;

  list = (LiVESList *)hstack->stack;

  // mark all entries in list at entry as "ACTIONED"
  // since we will parse the list several times, we only check those which are present now
  // this avoids a situation where we would be endlessly traversing the list as new items are added
  for (; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;
    if (!closure || !closure->proc_thread) continue;
    if (closure->flags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstack, list);
      continue;
    }
    if (!closure->proc_thread) continue;
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

    list = (LiVESList *)hstack->stack;

    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;

      if (!closure) continue;

      if (closure->flags & (HOOK_STATUS_BLOCKED | HOOK_STATUS_RUNNING)) continue;

      if (closure->flags & HOOK_STATUS_REMOVE) {
	remove_from_hstack(hstack, list);
        continue;
      }

      if (!(closure->flags & HOOK_STATUS_ACTIONED)) continue;

      lpt = closure->proc_thread;

      if (!lpt || lives_proc_thread_ref(closure->proc_thread) < 2) continue;

      if (lives_proc_thread_is_invalid(lpt)) {
	remove_from_hstack(hstack, list);
        continue;
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
            lives_proc_thread_wait_done(wait_parent, 0., FALSE);
            break;
          }
        }
      }

      closure->flags |= HOOK_STATUS_RUNNING;
      closure->flags &= ~HOOK_STATUS_ACTIONED;

      lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                       | THRD_STATE_FINISHED);

      if (type == SYNC_WAIT_HOOK || ((closure->flags & HOOK_OPT_REMOVE_ON_FALSE)
                                     && closure->fdef
                                     && closure->fdef->return_type == WEED_SEED_BOOLEAN)) {
        // test should be boolean_combined
        bret = TRUE; // set in case func is cancelled
        if (!closure->retloc) closure->retloc = &bret;
        //g_print("sync run: %s\n", lives_proc_thread_show_func_call(lpt));
      }

      // test here is hook_dtl_request - we forward some action request to another thread
      //
      if (!(closure->flags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
	GET_PROC_THREAD_SELF(self);
        PTMUH;
        hmulocked = FALSE;

	weed_set_plantptr_value(lpt, LIVES_LEAF_DISPATCHER, self);

        lives_proc_thread_execute(lpt);
        //if (lpt == mainw->debug_ptr) {
        //lives_proc_thread_t xxlpt = hstack->owner;
        //mainw->debug_ptr = xxlpt;
        /* g_print("runing hstack type %d, fun was %s, holder is %s. with %d refs\nflags is %s\n", type, */
        /*         lives_proc_thread_get_funcname(lpt), lives_proc_thread_get_funcname(xxlpt), */
        /*         lives_proc_thread_count_refs(lpt), cl_flags_desc(closure->flags)); */
        //}
      } else {
        PTMUH;
        hmulocked = FALSE;
        // this function will call fg_service_call directly,
        // block until the lpt completes or is cancelled
        // We should have set ONESHOT and BLOCK as appropriate
        lives_proc_thread_queue(lpt, LIVES_THRDATTR_FG_THREAD | LIVES_THRDATTR_FG_LIGHT);
      }

      PTMLH;
      hmulocked = TRUE;

      if (closure->flags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT)) {
        // remove our added ref UNLESS this is a blocking call, then blocked thread will do that
        // if it is blocking, then caller will be waiting and will unref it now
        // so we leave our added ref to compensate for one which will be removed as closure is freed
        if (!(closure->flags & HOOK_CB_BLOCK)) lives_proc_thread_unref(lpt);
        remove_from_hstack(hstack, list);
        break;
      }

      closure->flags &= ~HOOK_STATUS_RUNNING;

      if (type == SYNC_WAIT_HOOK || ((closure->flags & HOOK_OPT_REMOVE_ON_FALSE)
                                     && closure->fdef
                                     && closure->fdef->return_type == WEED_SEED_BOOLEAN)) {
        // combined bool
        bret = lives_proc_thread_join_boolean(lpt);
        if (closure->retloc) {
          *(boolean *)closure->retloc = bret;
        }
        if (!bret) {
          if (type == SYNC_WAIT_HOOK) {
            // combined bool
            retval = FALSE;

            // exit on FALSE
            lives_proc_thread_unref(lpt);
            list = NULL;
            break;
          }
          // remove on FALSE
          if (!(closure->flags & HOOK_CB_BLOCK)) lives_proc_thread_unref(lpt);
          remove_from_hstack(hstack, list);
          break;
        }
      }

      // will be be HOOK_DTL_SINGLE
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
          lives_microsleep_until_zero(pthread_mutex_lock(&mainw->all_hstacks_mutex));
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
    PTMUH;
  }

  hstacks[type]->flags &= ~STACK_TRIGGERING;

  if (have_recheck_mutex) {
    pthread_mutex_unlock(&recheck_mutex);
  }

  //if (type == SYNC_WAIT_HOOK) g_print("sync all res: %d\n", retval);
  return retval;
}


int lives_hooks_trigger_async(lives_hook_stack_t **hstacks, int type) {
  lives_proc_thread_t lpt;
  LiVESList *list, *listnext;
  lives_closure_t *closure;
  pthread_mutex_t *hmutex;
  lives_hook_stack_t *hstack;
  int ncount = 0;

  if (!hstacks) {
    // test should be HOOK_TYPE_SELF
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      GET_PROC_THREAD_SELF(self);
      if (!self) return ncount;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return ncount;
    }
  }

  hstack = hstacks[type];
  hmutex = &(hstack->mutex);
  PTMLH;

  if (!hstack->stack) {
    PTMUH;
    return ncount;
  }

  list = (LiVESList *)hstack->stack;

  for (; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;

    if (!closure) continue;
    if ((closure->flags & HOOK_STATUS_BLOCKED) || (closure->flags & HOOK_STATUS_RUNNING)) continue;

    if (closure->flags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstack, list);
      continue;
    }

    lpt = closure->proc_thread;
    if (!lpt || lives_proc_thread_ref(lpt) < 2) continue;

    if (lives_proc_thread_was_cancelled(lpt)) {
      remove_from_hstack(hstack, list);
      continue;
    } else {
      lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                       | THRD_STATE_FINISHED);
      closure->flags |= HOOK_STATUS_RUNNING;
      lives_proc_thread_queue(lpt, 0);
      ncount++;
    }
    lives_proc_thread_unref(lpt);
  }
  PTMUH;
  return ncount;
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


void lives_hook_remove(lives_proc_thread_t lpt) {
  // finds closure for lpt and flags it for removal
  // when the stack is triggered, the closure will be freed, and lpt unreffed
  // caller must ensure that lpt is valid during this call
  // NB: if a thread is blocking on lpt then it may remain in the blocked state
  // Should be called if and only if
  // - adder lpt is finishing
  // - lpt is finishing
  // - hook callback is no longer needed
  //
  lives_proc_thread_t xlpt;
  lives_closure_t *closure = lives_proc_thread_get_closure(lpt);
  if (closure) {
    closure->flags |= HOOK_STATUS_REMOVE;
    // flag lpt as INVALID, if a thread id waiting it will continue
    xlpt = closure->proc_thread;
    if (xlpt) lives_proc_thread_set_state(xlpt, (THRD_STATE_INVALID | THRD_STATE_DESTROYING));
  }
}


void lives_hook_remove_by_data(lives_hook_stack_t **hstacks, int type,
                               lives_funcptr_t func, void *data) {
  if (!hstacks) {
    GET_PROC_THREAD_SELF(self);
    if (!self) return;
    hstacks = lives_proc_thread_get_hook_stacks(self);
  }
  if (!hstacks) return;
  else {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = &(hstack->mutex);
    LiVESList *cblist, *cbnext;
    PTMLH;
    for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cbnext) {
      lives_closure_t *closure = (lives_closure_t *)cblist->data;
      cbnext = cblist->next;
      if (closure) {
        lives_proc_thread_t lpt = closure->proc_thread;
        if (!lpt || !closure->fdef) continue;
        if (closure->fdef->function != func) continue;
        if (data != weed_get_voidptr_value(lpt, PROC_THREAD_PARAM(1), NULL)) continue;
        closure->flags |= HOOK_STATUS_REMOVE;
        // flag lpt as INVALID, if a thread id waiting it will ontinue
        lives_proc_thread_set_state(lpt, (THRD_STATE_INVALID | THRD_STATE_DESTROYING));
      }
    }
    PTMUH;
  }
}


void lives_hooks_async_join(lives_hook_stack_t **hstacks, int htype) {
  lives_closure_t *closure;
  pthread_mutex_t *hmutex;
  lives_proc_thread_t lpt;
  LiVESList *cblist, *cblist_next;
  lives_hook_stack_t *hstack;
  if (!hstacks) {
    // test should be HOOK_TYPE_SELF
    if (htype == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      GET_PROC_THREAD_SELF(self);
      if (!self) return;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return;
    }
  }
  hstack = hstacks[htype];
  hmutex = &(hstack->mutex);
  PTMLH;
  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist_next) {
    cblist_next = cblist->next;
    closure = (lives_closure_t *)cblist->data;
    if (!closure) continue;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;

    lpt = closure->proc_thread;
    if (!lpt) continue;

    lives_proc_thread_wait_done(lpt, 0., FALSE);

    if (closure->flags & (HOOK_OPT_ONESHOT | HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, cblist);
      continue;
    }

    closure->flags &= ~HOOK_STATUS_RUNNING;

    if (lives_proc_thread_was_cancelled(lpt)) {
      remove_from_hstack(hstack, cblist);
      continue;
    }
  }
  PTMUH;
}


boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp) {
  funcsig_t fsig1 = lives_proc_thread_get_funcsig(lpt1);
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
  char *pname, *args_fmt = lives_proc_thread_get_args_fmt(lpt1);
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
    funcsig_t fsig = lives_proc_thread_get_funcsig(lpt1);
    if (fsig != lives_proc_thread_get_funcsig(lpt2)) return -3;
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
  if (clflags & HOOK_TOGGLE_FUNC)
    fstr = lives_strdup_concat(fstr, ", ", "%s", "TOGGLE_FUNC");
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
  if (!hstack) {
    g_print("hstacks[%d] is NULL !!\n", type);
    return;
  }
  hmutex = &(hstack->mutex);
  PTMLH;
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
      while (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
        lpt =  weed_get_plantptr_value(lpt, LIVES_LEAF_REPLACEMENT, 0);
      }
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
  PTMUH;
  return;
}

void dump_hook_stack_for(lives_proc_thread_t lpt, int type) {
  if (lpt) dump_hook_stack(lives_proc_thread_get_hook_stacks(lpt), type);
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
    fdef->funcsig = funcsig_from_args_fmt(args_fmt);
    if (file) fdef->file = lives_strdup(file);
    if (line < 0) {
      line = -line;
      fdef->flags |= FDEF_FLAG_INSIDE;
    }
    fdef->line = line;
    fdef->data = data;
  }
  return fdef;
}


LIVES_GLOBAL_INLINE void free_funcdef(lives_funcdef_t *fdef) {
  if (fdef) {
    if (!(fdef->flags & FDEF_FLAG_STATIC)) {
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
  // for future use
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


static void _add_fn_note(fn_type_t ftype, lives_funcdef_t *fdef, void *ptr) {
  static pthread_mutex_t ftrace_mutex = PTHREAD_MUTEX_INITIALIZER;
  weed_plant_t *note;
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
      if (ftype == _FN_ALLOC || ftype == _FN_REF) {
        if (count >= 0) weed_set_int_value(note, "count", ++count);
      } else {
        if (!(--count))
          ftrace_store = remove_from_hash_store_i(ftrace_store, (uintptr_t)ptr);
        else
          weed_set_int_value(note, "count", count);
      }
      pthread_mutex_unlock(&ftrace_mutex);
      return;
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
}


// if io is FN_ALLOC/FN_REF, add a note to ftrace_store or increment the count
// if io is FN_FREE/FN_UNREF, decrement the count and if zero, remove the note
void *add_fn_note(fn_type_t ftype, ...) {
  va_list va;
  void *ptr = NULL;
  if (ftype == _FN_ALLOC || ftype == _FN_FREE
      || ftype == _FN_REF || ftype == _FN_UNREF) {
    lives_funcdef_t *fdef = NULL;
    va_start(va, ftype);
    fdef = va_arg(va, lives_funcdef_t *);
    ptr = va_arg(va, void *);
    _add_fn_note(ftype, fdef, ptr);
    va_end(va);
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
