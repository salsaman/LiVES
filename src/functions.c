// functions.c
// (c) G. Finch 2002 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

static hookstack_descriptor_t hs_desc[N_HOOK_POINTS];

static boolean hs_inited = FALSE;

hookstack_descriptor_t *get_hs_desc(void) {
  return hs_desc;
}

static void init_hook_stacks(void) {
  if (hs_inited) return;
  hs_inited = TRUE;
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    uint64_t flags = 0;
    hookstack_pattern_t pat = HOOK_PATTERN_DATA;
    hookstack_descriptor_t *xhs = (hookstack_descriptor_t *)&hs_desc[i];
    xhs->htype = i;
    switch (i) {
    case FATAL_HOOK:
      flags = HS_FLAGS_FATAL;
      pat = HOOK_PATTERN_SPONTANEOUS;
      break;
    case THREAD_EXIT_HOOK:
      flags = HS_FLAGS_THREAD_EXIT;
      pat = HOOK_PATTERN_SPONTANEOUS;
      break;
    case DATA_READY_HOOK:
      flags = HS_FLAGS_DATA_READY;
      pat = HOOK_PATTERN_SPONTANEOUS;
      break;
    case LIVES_GUI_HOOK:
      flags = HS_FLAGS_LIVES_GUI;
      pat = HOOK_PATTERN_SPONTANEOUS;
      break;
    case SYNC_ANNOUNCE_HOOK:
      flags = HS_FLAGS_SYNC_ANNOUNCE;
      pat = HOOK_PATTERN_REQUEST;
      break;
    case COMPLETED_HOOK:
      flags = HS_FLAGS_COMPLETED;
      break;
    case FINISHED_HOOK:
      flags = HS_FLAGS_FINISHED;
      break;
    case ERROR_HOOK:
      flags = HS_FLAGS_ERROR;
      break;
    case CANCELLED_HOOK:
      flags = HS_FLAGS_CANCELLED;
      break;
    case DESTRUCTION_HOOK:
      flags = HS_FLAGS_DESTRUCTION;
      break;
    default: break;
    }
    xhs->pattern = pat;
    xhs->op_flags = flags;
  }
}


uint64_t lives_hookstack_op_flags(int htype) {
  if (!hs_inited) init_hook_stacks();
  return htype >= 0 && htype < N_HOOK_POINTS ? hs_desc[htype].op_flags : HOOKSTACK_INVALID;
}


hookstack_pattern_t lives_hookstack_pattern(int htype) {
  if (!hs_inited) init_hook_stacks();
  return htype >= 0 && htype < N_HOOK_POINTS ? hs_desc[htype].pattern : HOOK_PATTERN_INVALID;
}

void toggle_var_cb(void *dummy, boolean *var) {if (var) *var = !(*var);}
void inc_counter_cb(void *dummy, int *var) {if (var)(*var)++;}
void dec_counter_cb(void *dummy, int *var) {if (var)(*var)--;}
void resetc_counter_cb(void *dummy, int *var) {if (var) *var = 0;}

#ifdef DEBUG_MUTEXES
#define PTMLH do {g_print("lock %p at %d\n", hmutex, _LINE_REF_); pthread_mutex_lock(hmutex);} while (0)
#define PTMUH do {g_print("unlock %p at %d\n", hmutex, _LINE_REF_); pthread_mutex_unlock(hmutex);} while (0)
#define PTMTLH (printf("lock %p at %d\n", hmutex, _LINE_REF_) ? pthread_mutex_trylock(hmutex) : 1)
#else
#define PTMLH do {pthread_mutex_lock(hmutex);} while (0)
#define PTMUH do {pthread_mutex_unlock(hmutex);} while (0)
#define PTMTLH pthread_mutex_trylock(hmutex)
#endif

const lookup_tab crossrefs[] = XREFS_TAB;

#define _SET_LEAF_FROM_VARG(plant, pkey, type, ctype, cptrtype, ne, args) \
  (ne == 1 ? weed_set_##type##_value((plant), (pkey), va_arg((args), ctype)) \
   : weed_set_##type##_array((plant), (pkey), (ne), va_arg((args), cptrtype)))

#define SET_LEAF_FROM_VARG(plant, pkey, type, ne, args) _SET_LEAF_FROM_VARG((plant), (pkey), type, \
									    CTYPE(type), CPTRTYPE(type), (ne), (args))

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

extern char *nirvascope_bundle_to_header(weed_plant_t *, const char *tname, int idx);

void add_quick_fn(lives_funcptr_t func, const char *funcname) {
  return;
  char *key;
  if (sizeof(lives_funcptr_t) != sizeof(void *)) return;
  key = lives_strdup_printf("function@%p", func);
  if (!fn_looker) fn_looker = lives_plant_new(LIVES_PLANT_INDEX);
  weed_set_const_string_value(fn_looker, key, (void *)funcname);
  lives_free(key);
  /* #ifdef SHOW_KNOWN_FUNCS */
  /*   if (fn_looker) */
  /*     g_print("%s", nirvascope_bundle_to_header(fn_looker, 0, 0)); */
  /* #endif */
}


const char *get_funcname(lives_funcptr_t func) {
  return NULL;
  //  const char *fname;
  /* char *key; */
  /* if (!fn_looker) return NULL; */
  /* key = lives_strdup_printf("function@%p", func); */
  /* fname = weed_get_const_string_value(fn_looker, key, NULL); */
  /* lives_free(key); */
  /* return fname; */
}


LIVES_GLOBAL_INLINE void _func_entry(lives_funcptr_t func, const char *funcname, int category, const char *rettype,
                                     const char *args_fmt, char *file_ref, int line_ref, uint64_t flags, ...) {
  add_quick_fn(func, funcname);
}



LIVES_GLOBAL_INLINE void _func_exit(char *file_ref, int line_ref) {
  char *fname = (char *)lives_sync_list_pop(&THREADVAR(func_stack));
  lives_free(fname);
}


LIVES_GLOBAL_INLINE void _func_exit_val(weed_plant_t *pl, char *file_ref, int line_ref) {
  char *fname = (char *)lives_sync_list_pop(&THREADVAR(func_stack));
  lives_free(fname);
  /* LiVESList *list = THREADVAR(func_stack); */
  /* g_print("Thread 0x%lx exiting func %s @ line %d, %s\n", THREADVAR(uid), */
  /*         (char *)list->data, line_ref, file_ref); */

  /* THREADVAR(func_stack) = list->next; */
  /* if (list->next) list->next = list->next->prev = NULL; */
  /* lives_list_free_all(&list); */
}

///////////////////////////

LIVES_LOCAL_INLINE char *make_std_pname(int pn) {return lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, pn);}

static boolean is_child_of(LiVESWidget *w, LiVESContainer *C);
static boolean fn_match_child(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2);

static lives_result_t weed_plant_params_from_valist(weed_plant_t *plant, uint64_t attrs, const char *args_fmt, \
    make_key_f param_name_func, va_list xargs) {
  int p = 0;
  for (const char *c = args_fmt; *c; c++) {
    char *pkey = (*param_name_func)(p);
    uint32_t st = _char_to_st(*c);
    weed_error_t err = weed_leaf_from_varg(plant, pkey, st, 1, xargs);
    lives_free(pkey);
    if (attrs & (attrs & LIVES_THRDATTR_HAS_FREEFUNCS)) {
      void *free_lpt = va_arg(xargs, void *);
      if (free_lpt) {
        pkey = lives_strdup_printf("free_lpt%d", p);
        weed_set_voidptr_value(plant, pkey, free_lpt);
        lives_free(pkey);
      }
    }
    if (err != WEED_SUCCESS) {
      return LIVES_RESULT_ERROR;
    }
    p++;
  }
  return LIVES_RESULT_SUCCESS;
}


lives_result_t proc_thread_params_from_vargs(lives_proc_thread_t lpt, va_list xargs) {
  lives_result_t res = LIVES_RESULT_INVALID;
  if (lpt) {
    lives_funcdef_t *fdef = lives_proc_thread_get_funcdef(lpt);
    if (fdef) {
      lives_funcinst_t *finst;
      uint64_t attrs = lives_proc_thread_get_attrs(lpt);
      char *args_fmt = args_fmt_from_funcsig(fdef->funcsig);
      if (!args_fmt) return WEED_SUCCESS;
      finst = lives_proc_thread_get_funcinst(lpt);
      if (!finst->params) finst->params = lives_plant_new(LIVES_PLANT_FUNCPARAMS);
      res = weed_plant_params_from_valist(finst->params, attrs, args_fmt, make_std_pname, xargs);
      lives_free(args_fmt);
    }
  }
  return res;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lpt_from_funcdef_va(lives_funcdef_t *fdef,
    lives_thread_attr_t attrs, va_list vargs) {
  lives_proc_thread_t lpt = NULL;
  if (fdef) {
    char *args_fmt = args_fmt_from_funcsig(fdef->funcsig);
    lpt =  _lives_proc_thread_create_vargs(attrs, fdef->function, fdef->funcname,
                                           fdef->return_type, args_fmt, vargs);
    if (args_fmt) lives_free(args_fmt);
  }
  return lpt;
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lpt_from_funcdef(lives_funcdef_t *fdef, lives_thread_attr_t attrs, ...) {
  lives_proc_thread_t lpt;
  va_list ap;
  va_start(ap, attrs);
  lpt = lpt_from_funcdef_va(fdef, attrs, ap);
  va_end(ap);
  return lpt;
}


void *lives_proc_thread_execute_retvoidptr(weed_plant_t **plantp, lives_funcptr_t func, ...) {
  uint64_t uid = gen_unique_id();
  char *key = lives_strdup_printf("key_%lu", uid);
  void *retval;
  lives_proc_thread_t lpt;
  lives_funcdef_t *fdef = NULL;//lookup_fdef(NULL, func);
  va_list ap;
  va_start(ap, func);
  lpt = lpt_from_funcdef(fdef, LIVES_THRDATTR_START_UNQUEUED, ap);
  va_end(ap);
  lives_proc_thread_execute(lpt);
  retval = lives_proc_thread_join_voidptr(lpt);
  lives_proc_thread_unref(lpt);
  if (!*plantp) *plantp = lives_plant_new(LIVES_PLANT_CLEANER);
  weed_set_voidptr_value(*plantp, key, retval);
  weed_leaf_set_autofree(*plantp, key, TRUE);
  lives_free(key);
  return retval;
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


#define _IF_(pn)if (pn) lives_free(pn)

#define _DC_(n, ...) do {if (ret_type) XCALL_##n(finst->params, ret_type, &thefunc, __VA_ARGS__); \
    else CALL_VOID_##n(finst->params, &thefunc, __VA_ARGS__);} while (0);

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

  lives_funcinst_t *finst = lives_proc_thread_get_funcinst(lpt);
  lives_funcdef_t *fdef = lives_proc_thread_get_funcdef(lpt);
  if (!fdef || !finst) abort();

  weed_funcptr_t func = fdef->function;//lives_proc_thread_get_function(lpt);
  if (!func) return LIVES_RESULT_ERROR;

  uint32_t ret_type = fdef->return_type;//lives_proc_thread_get_rtype(lpt);
  funcsig_t sig = fdef->funcsig;//lives_proc_thread_get_funcsig(lpt);
  int nparms = get_funcsig_nparms(sig);

  allfunc_t thefunc;

  thefunc.func = func;

  if (lpt == mainw->debug_ptr) g_print("nrefssxxxx = %d\n", lives_proc_thread_count_refs(lpt));

  // compressed lines as this could become quite long...
  switch (nparms) {
  case 0: _DC_(0,); break;

  case 1:
    switch (sig) {
      ONE_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;
  case 2:
    switch (sig) {
      TWO_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  case 3:
    switch (sig) {
      THREE_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  case 4:
    switch (sig) {
      FOUR_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  case 5:
    switch (sig) {
      FIVE_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  case 6:
    switch (sig) {
      SIX_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  case 7:
    switch (sig) {
      SEVEN_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  case 8:
    switch (sig) {
      EIGHT_PARAM_FUNCSIGS
    default: return LIVES_RESULT_ERROR;
    }
    break;

  // invalid nparms
  default: return LIVES_RESULT_ERROR;
  }

#if USE_RPMALLOC
  rpmalloc_thread_collect();
#endif

  if (lpt == mainw->debug_ptr) g_print("nrefssxxxxzzZZZ = %d\n", lives_proc_thread_count_refs(lpt));

  if (err != WEED_SUCCESS) return LIVES_RESULT_FAIL;

  lives_leaf_dup(lpt, finst->params, _RV_);
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
  if (lpt == mainw->debug_ptr) g_print("nrefss mmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  if (!lpt || lives_proc_thread_ref(lpt) < 2) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid proc_thread");
    return FALSE;
  }

  weed_funcptr_t func = lives_proc_thread_get_function(lpt);
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

  if (lpt == mainw->debug_ptr) g_print("nrefs PPPmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  ret = do_call(lpt);

  if (lpt == mainw->debug_ptr) g_print("nrefss AAAAmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  /* if (weed_plant_has_leaf(lpt, LIVES_LEAF_LONGJMP)) */
  /*   weed_leaf_delete(lpt, LIVES_LEAF_LONGJMP); */

  lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS) {
    weed_set_int64_value(lpt, LIVES_LEAF_END_TICKS, lives_get_current_ticks());
  }

  if (ret == LIVES_RESULT_ERROR) goto funcerr;

  if (err == WEED_SUCCESS) {
    if (lpt == mainw->debug_ptr)
      g_print("pt a1\n");

    lives_proc_thread_unref(lpt);
    if (lpt == mainw->debug_ptr) g_print("nrefss nnnn = %d\n", lives_proc_thread_count_refs(lpt));

    if (lpt == mainw->debug_ptr)
      g_print("pt a122\n");

    // disarm the free_lpts
    lpt_params_free(lpt, FALSE);
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

  lives_proc_thread_error_full(lpt, NULL, 0, 123, LPT_ERR_CRITICAL, msg);

  /* LIVES_ERROR(msg); */
  /* lives_free(msg); */
  lives_proc_thread_unref(lpt);
  return FALSE;
}


boolean call_funcsig(lives_proc_thread_t lpt) {
  _call_funcsig_inner(lpt);
  return FALSE;
}


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


static weed_seed_t nth_seed_type(funcsig_t sig, int n) {
  if (n >= 0) {
    for (int i = 60; i >= 0; i -= 4) {
      uint8_t ch = (sig >> i) & 0X0F;
      if (!ch) continue;
      if (!n--) return get_seedtype(ch);
    }
  }
  return WEED_SEED_INVALID;
}


static char *lpt_paramstr(lives_proc_thread_t lpt, funcsig_t sig) {
  int pn = 0;
  char *pstr = NULL, *pname, *fmtstr = lives_strdup("");
  const char *ctype;
  uint32_t ne, st;
  lives_funcinst_t *finst = lives_proc_thread_get_funcinst(lpt);
  for (int i = 60; i >= 0; i -= 4) {
    uint8_t ch = (sig >> i) & 0X0F;
    if (!ch) continue;
    st = get_seedtype(ch);
    pname = make_std_pname(pn++);
    ne = weed_leaf_num_elements(finst->params, pname);
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
    const char *funcname = lives_proc_thread_get_funcname(lpt);

    parvals = lpt_paramstr(lpt, sig);

    if (ret_type) {
      fmtstring = lives_strdup_printf("(%s) %s(%s);",
                                      weed_seed_to_ctype(ret_type, FALSE), funcname, parvals);
    } else fmtstring = lives_strdup_printf("(void) %s(%s);", funcname, parvals);
    lives_free(parvals);
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
    lives_funcdef_t *fdef;
    lives_closure_t *closure  = (lives_closure_t *)lives_calloc(1, sizeof(lives_closure_t));
    pthread_mutex_init(&closure->mutex, NULL);
    fdef = lives_proc_thread_get_funcdef(lpt);
    fdef->category = lives_fdef_get_category(FUNC_CATEGORY_HOOK_COMMON, hook_type);
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



void flush_cb_list(lives_proc_thread_t lpt) {
  // -For callbacks added to another proc_thread's stack:
  //   unless the stack descriptor is flagged as PERSISTENT, or unless the callback
  //   was added with HOOK_CB_PERSISTENT, then the ADDING proc_thread maintains a pointer to
  //   the each callback added, appending the address to its EXT_CB_LIST. Just before a proc_thread is freed,
  //   the ext_cb_list will be cleared - each callback will be flagged for removal, and adder set to NULL
  //
  //  conversely if the callback closure  is to be freed - for example if it is a oneshot callback,
  // or when the proc_thread owning the stack is being freed,
  // we check if there is an 'adder' for the callback.
  // This information is used to remove the callback from the ext_cb_list of the adder.
  //
  // A mutex lock is used to ensure single access to the cb_list
  // however this creates a race condition - B reads closure and gets adder A
  // A is freed, B attempts to find cb_list for A
  //
  // to prevent this we proceed as follows:
  // B locks the closure, C
  // B gets adder A from C
  // B gets lock on A cb_list
  // B removes C from A's list
  // B unlocks A list
  // B unlocks C
  // meanwhile...
  // A locks cb_list
  // A gets lock on C
  // A removes adder from C
  // A unlocks C
  // .. or
  // A fails to get lock on C
  // A must unlock cb_list, allowing B to remove C
  // A reaquires cb_list lock
  // A reparses list, starting from beginning

  pthread_mutex_t *extcb_mutex = (pthread_mutex_t *)weed_get_voidptr_value(lpt, LIVES_LEAF_EXT_CB_MUTEX, NULL);
  boolean restart = FALSE;
  LiVESList *list, *xlist;

  do {
    restart = FALSE;
    pthread_mutex_lock(extcb_mutex);
    xlist = list = (LiVESList *)weed_get_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, NULL);
    while (list) {
      LiVESList *listnext = list->next;
      lives_closure_t *cl = (lives_closure_t *)list->data;
      if (cl) {
        if (pthread_mutex_trylock(&cl->mutex)) {
          weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, xlist);
          pthread_mutex_unlock(extcb_mutex);
          restart = TRUE;
          break;
        }
        cl->adder = NULL;
        cl->flags |= HOOK_STATUS_REMOVE;
        pthread_mutex_unlock(&cl->mutex);
      }
      xlist = lives_list_remove_node(xlist, list, FALSE);
      list = listnext;
    }
  } while (restart);
  weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, NULL);
  pthread_mutex_unlock(extcb_mutex);
}


static void remove_ext_cb_other(lives_proc_thread_t lpt, lives_closure_t *cl) {
  LiVESList *list = (LiVESList *)weed_get_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, NULL);
  if (list) weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST,
                                     lives_list_remove_data(list, cl, FALSE));
}


static void examine_ext_cb_other(lives_closure_t *cl) {
  if (cl) {
    lives_proc_thread_t lpt = cl->adder;
    if (lpt) {
      pthread_mutex_t *extcb_mutex = (pthread_mutex_t *)weed_get_voidptr_value
                                     (lpt, LIVES_LEAF_EXT_CB_MUTEX, NULL);
      pthread_mutex_lock(extcb_mutex);
      if (cl->adder == lpt) remove_ext_cb_other(lpt, cl);
      pthread_mutex_unlock(extcb_mutex);
    }
  }
}


static void add_to_cb_list(lives_proc_thread_t self, lives_closure_t *closure) {
  // when adding a hook cb to another hook stack, keep a pointer to it
  LiVESList *ext_cbs;
  pthread_mutex_t *extcb_mutex = (pthread_mutex_t *)weed_get_voidptr_value(self, LIVES_LEAF_EXT_CB_MUTEX, NULL);
  pthread_mutex_lock(extcb_mutex);
  ext_cbs = (LiVESList *)weed_get_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, NULL);
  weed_set_voidptr_value(self, LIVES_LEAF_EXT_CB_LIST, lives_list_prepend(ext_cbs, (void *)closure));
  pthread_mutex_unlock(extcb_mutex);
}



LIVES_GLOBAL_INLINE void lives_closure_free(lives_closure_t *closure) {
  // with state mutex locked
  if (closure) {
    lives_proc_thread_t lpt;
    pthread_mutex_lock(&closure->mutex);
    if (closure->adder) examine_ext_cb_other(closure);
    pthread_mutex_unlock(&closure->mutex);

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

  /* if (closure->fdef) */
  /*   free_funcdef((lives_funcdef_t *)closure->fdef); */

  lives_free(closure);
}


////// hook functions /////

LIVES_GLOBAL_INLINE void lives_hooks_clear(lives_hook_stack_t **hstacks, int type) {
  if (hstacks) {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = &(hstack->mutex);
    LiVESList *hsstack;
    uint64_t hs_op_flags;

    hs_op_flags = lives_hookstack_op_flags(type);
    if (hs_op_flags == HOOKSTACK_INVALID) return;
    HOOKSTACK_FLAGS_ADJUST(hs_op_flags, is_fg_thread());

    if (hs_op_flags & HOOKSTACK_ASYNC_PARALLEL) {
      // if stack owner is self, we need to async_join
      if (!(hstack->flags & HOOKSTACK_NATIVE)) {
        GET_PROC_THREAD_SELF(self);
        if (hstack->owner.lpt == self) {
          if (hstack->flags & STACK_TRIGGERING) PTMUH;
          lives_hooks_async_join(NULL, type);
        }
      }
    }

    while (1) {
      // caution - astbc stacks can stay triggered for a long time
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
      pthread_mutex_destroy(&hstacks[i]->mutex);
      lives_free(hstacks[i]);
    }
}


static lives_result_t duplicate_params(weed_plant_t *dst, weed_plant_t *src, int nparams) {
  if (nparams <= 0 || !dst || !src) return LIVES_RESULT_INVALID;
  lives_funcinst_t *fdst = lives_proc_thread_get_funcinst(dst);
  lives_funcinst_t *fsrc = lives_proc_thread_get_funcinst(src);
  for (int i = 0; i < nparams; i++) {
    char *pkey = lives_strdup_printf("free_lpt%d", i);
    char *pname = make_std_pname(i);
    lives_leaf_dup(fdst->params, fsrc->params, pname);
    if (weed_plant_has_leaf(fsrc->params, pkey)) {
      lives_leaf_dup(fdst->params, fsrc->params, pkey);
      weed_leaf_delete(fsrc->params, pkey);
    }
  }
  return LIVES_RESULT_SUCCESS;
}


static void call_free_func(lives_proc_thread_t lpt, int i, boolean do_exec) {
  char *pkey = lives_strdup_printf("free_lpt%d", i);
  lives_funcinst_t *finst = lives_proc_thread_get_funcinst(lpt);
  lives_proc_thread_t free_lpt = (lives_proc_thread_t)weed_get_voidptr_value(finst->params, pkey, NULL);
  if (free_lpt) {
    weed_leaf_delete(finst->params, pkey);
    if (do_exec) lives_proc_thread_execute(free_lpt);
    lives_proc_thread_unref(free_lpt);
  }
  lives_free(pkey);
}


void lpt_params_free(lives_proc_thread_t lpt, boolean do_exec) {
  funcsig_t funcsig = lives_proc_thread_get_funcsig(lpt);
  int nparams = get_funcsig_nparms(funcsig);
  for (int i = 0; i < nparams; i++) call_free_func(lpt, i, do_exec);
}


static boolean fn_data_replace(lives_proc_thread_t dst, lives_proc_thread_t src) {
  int nparms = fn_func_match(src, dst);
  if (nparms > 0) {
    // check all the params we are replacing
    for (int i = 0; i < nparms; i++) call_free_func(dst, i, TRUE);
    duplicate_params(dst, src, nparms);
    return TRUE;
  }
  return FALSE;
}


void remove_from_hstack(lives_hook_stack_t *hstack, LiVESList *list) {
  // should be called with hstack mutex LOCKED !
  // remove list from htack, free the closure
  // (which will unref the proc_thread in it)
  // lpt will be FREED unless reffed elsewhere !
  lives_closure_t *closure = (lives_closure_t *)list->data;

  hstack->stack = (volatile LiVESList *)lives_list_remove_node
                  ((LiVESList *)hstack->stack, list, FALSE);
  lives_closure_free(closure);
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
  boolean is_self_stack = FALSE;
  uint64_t hs_op_flags;

  if (!data) return NULL;

  if (!hstacks) {
    if (type == SYNC_WAIT_HOOK) hstacks = THREADVAR(hook_stacks);
    else {
      if (!self) return NULL;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return NULL;
      is_self_stack = TRUE;
    }
  } else if (hstacks[type]->owner.lpt == self)
    is_self_stack = TRUE;

  hs_op_flags = lives_hookstack_op_flags(type);

  if (hs_op_flags == HOOKSTACK_INVALID) return NULL;
  HOOKSTACK_FLAGS_ADJUST(hs_op_flags, is_fg_thread());

  if (flags & HOOK_CB_PRIORITY) is_append = FALSE;

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
          // and it will also be unreffed when the closure is freeud
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

  if (!is_self_stack && !(hs_op_flags & HOOKSTACK_PERSISTENT) && !(flags & HOOK_CB_PERSISTENT)) {
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
  if (flags & HOOK_CB_HAS_FREEFUNCS) attrs |= LIVES_THRDATTR_HAS_FREEFUNCS;

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
  lives_funcdef_t *fdef;
  LiVESList *list, *listnext;
  lives_closure_t *closure;
  pthread_mutex_t *hmutex;
  boolean bret;
  boolean retval = TRUE;
  boolean have_recheck_mutex = FALSE;
  boolean hmulocked = FALSE;
  lives_hook_stack_t *req_stack = NULL;
  uint64_t myhints;
  uint64_t hs_op_flags = lives_hookstack_op_flags(type);
  hookstack_pattern_t hs_pattern = lives_hookstack_pattern(type);
  boolean rerun = FALSE;

  if (hs_op_flags == HOOKSTACK_INVALID) return FALSE;

  HOOKSTACK_FLAGS_ADJUST(hs_op_flags, is_fg_thread());

  //if (type == SYNC_ANNOUNCE_HOOK) dump_hook_stack(hstacks, type);

  if (!hstacks) {
    if (hs_op_flags & HOOKSTACK_NATIVE) hstacks = THREADVAR(hook_stacks);
    else {
      GET_PROC_THREAD_SELF(self);
      if (!self) return TRUE;
      hstacks = lives_proc_thread_get_hook_stacks(self);
      if (!hstacks) return TRUE;
    }
  }

  if (hs_op_flags & HOOKSTACK_ASYNC_PARALLEL) {
    lives_hooks_trigger_async(hstacks, type);
    return TRUE;
  }

  hstack = hstacks[type];
  hmutex = &(hstack->mutex);

  if (type != FATAL_HOOK)
    PTMLH;
  hmulocked = TRUE;

  if (!hstack->stack || (hstack->flags & STACK_TRIGGERING)) {
    if (type != FATAL_HOOK) PTMUH;
    hmulocked = FALSE;
    goto trigdone;
  }

  hstack->flags |= STACK_TRIGGERING;

  if (hs_pattern == HOOK_PATTERN_REQUEST) {
    if (hstack->req_target_stacks)
      req_stack = hstack->req_target_stacks[hstack->req_target_type];
    if (!req_stack) {
      if (type != FATAL_HOOK) PTMUH;
      hmulocked = FALSE;
      goto trigdone;
    }
    pthread_mutex_lock(&req_stack->mutex);
  }

  myhints = THREADVAR(hook_hints);
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
    rerun = FALSE;
    retval = FALSE;
    if (type != FATAL_HOOK)
      if (!hmulocked) {
        if (hs_op_flags & HOOKSTACK_NOWAIT) {
          if (PTMTLH) break;
        } else PTMLH;
        hmulocked = TRUE;
      }

    retval = TRUE;

    list = (LiVESList *)hstack->stack;

    for (; list; list = listnext) {
      listnext = list->next;
      closure = (lives_closure_t *)list->data;

      if (!closure) continue;

      if (closure->flags & (HOOK_STATUS_BLOCKED | HOOK_CB_IGNORE | HOOK_STATUS_RUNNING)) continue;

      if (closure->flags & HOOK_STATUS_REMOVE) {
        remove_from_hstack(hstack, list);
        continue;
      }

      if (!(closure->flags & HOOK_STATUS_ACTIONED)) continue;

      lpt = closure->proc_thread;

      // REF
      if (!lpt || lives_proc_thread_ref(closure->proc_thread) < 2) continue;

      if (lives_proc_thread_is_invalid(lpt)) {
        remove_from_hstack(hstack, list);
        continue;
      }

      if (weed_plant_has_leaf(lpt, LIVES_LEAF_REPLACEMENT)) {
        // UNREF
        lives_proc_thread_unref(lpt);
        continue;
      }

      if (req_stack) {
        if ((myhints & closure->flags) == myhints) {
          int dflags = DTYPE_HAVE_LOCK | DTYPE_CLOSURE;
          closure->flags &= ~HOOK_STATUS_ACTIONED;
          closure->flags |= hstack->req_target_set_flags;
          if (closure->flags & HOOK_CB_PRIORITY) dflags |= DTYPE_PREPEND;
          lives_proc_thread_show_func_call(closure->proc_thread);
          if (lives_hook_add(hstack->req_target_stacks, hstack->req_target_type,
                             closure->flags, closure, dflags) != lpt)
            remove_from_hstack(hstack, list);
          else hstack->stack =
              (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack, list, FALSE);
        }
        lives_proc_thread_unref(lpt);
        continue;
      }

      if (type == LIVES_GUI_HOOK) {
        uint64_t xflags = closure->flags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
        if (xflags) {
          if (!have_recheck_mutex) {
            if (pthread_mutex_trylock(&recheck_mutex)) {
              // this means some other thread also has invalidate_data, we must let it run then recheck
              // UNREF
              lives_proc_thread_unref(lpt);
              lives_sleep_until_zero(pthread_mutex_trylock(&recheck_mutex));
              have_recheck_mutex = TRUE;
              rerun = TRUE;
              break;
            }
          }

          wait_parent = update_linked_stacks(closure, xflags);
          pthread_mutex_unlock(&recheck_mutex);

          if (wait_parent) {
            lives_proc_thread_unref(lpt);
            lives_proc_thread_wait_done(wait_parent, 0., FALSE);
            rerun = TRUE;
            break;
          }
        }
      }

      closure->flags |= HOOK_STATUS_RUNNING;
      closure->flags &= ~HOOK_STATUS_ACTIONED;

      lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                       | THRD_STATE_FINISHED);
      fdef = lives_proc_thread_get_funcdef(lpt);
      if ((closure->flags & HOOK_OPT_REMOVE_ON_FALSE)
          && fdef->return_type == WEED_SEED_BOOLEAN) {
        // test should be boolean_combined
        bret = TRUE; // set in case func is cancelled
        if (!closure->retloc) closure->retloc = &bret;
        //g_print("sync run: %s\n", lives_proc_thread_show_func_call(lpt));
      }

      // test here is hook_dtl_request - we forward some action request to another thread
      //
      if (!(closure->flags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
        GET_PROC_THREAD_SELF(self);
        if (type != FATAL_HOOK) PTMUH;
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
        if (type != FATAL_HOOK) PTMUH;
        hmulocked = FALSE;
        // this function will call fg_service_call directly,
        // block until the lpt completes or is cancelled
        // We should have set ONESHOT and BLOCK as appropriate
        lives_proc_thread_queue(lpt, LIVES_THRDATTR_FG_THREAD | LIVES_THRDATTR_FG_LIGHT);
      }

      if (type != FATAL_HOOK) PTMLH;
      hmulocked = TRUE;

      if (closure->flags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT)
          || (hs_op_flags & HOOKSTACK_ALWAYS_ONESHOT)) {
        // remove our added ref UNLESS this is a blocking call, then blocked thread will do that
        // if it is blocking, then caller will be waiting and will unref it now
        // so we leave our added ref to compensate for one which will be removed as closure is freed
        // UNREF
        if (!(closure->flags & HOOK_CB_BLOCK)) lives_proc_thread_unref(lpt);
        remove_from_hstack(hstack, list);
        rerun = TRUE;
        break;
      }

      closure->flags &= ~HOOK_STATUS_RUNNING;

      if (type == SYNC_WAIT_HOOK || ((closure->flags & HOOK_OPT_REMOVE_ON_FALSE)
                                     && fdef->return_type == WEED_SEED_BOOLEAN)) {
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
            break;
          }
          // remove on FALSE
          if (!(closure->flags & HOOK_CB_BLOCK)) lives_proc_thread_unref(lpt);
          if (type != FATAL_HOOK) remove_from_hstack(hstack, list);
          else closure->flags |= HOOK_CB_IGNORE;
          rerun = TRUE;
          break;
        }
      }

      // will be be HOOK_DTL_SINGLE
      if (hs_op_flags & HOOKSTACK_RUN_SINGLE) {
        //g_print("done single\n");
        lives_proc_thread_unref(lpt);
        retval = TRUE;
        break;
      }
      lives_proc_thread_unref(lpt);
      rerun = TRUE;
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
      if (type != FATAL_HOOK)
        PTMUH;
      hmulocked = FALSE;
    }
  } while (rerun);

trigdone:

  if (req_stack) pthread_mutex_unlock(&req_stack->mutex);

  if (type != FATAL_HOOK)
    if (hmulocked) PTMUH;

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
  uint64_t hs_op_flags;
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

  hs_op_flags = lives_hookstack_op_flags(type);

  if (!(hs_op_flags & HOOKSTACK_ASYNC_PARALLEL)) {
    PTMUH;
    return ncount;
  }

  list = (LiVESList *)hstack->stack;

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

  list = (LiVESList *)hstack->stack;
  hs_op_flags = lives_hookstack_op_flags(type);

  for (; list; list = listnext) {
    listnext = list->next;
    closure = (lives_closure_t *)list->data;

    if (!closure) continue;
    if (!(closure->flags & HOOK_STATUS_ACTIONED)) continue;

    closure->flags &= ~HOOK_STATUS_ACTIONED;

    if ((closure->flags & HOOK_STATUS_BLOCKED) || (closure->flags & HOOK_STATUS_RUNNING)) continue;

    if (closure->flags & (HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, list);
      continue;
    }

    lpt = closure->proc_thread;
    // REF
    if (!lpt || lives_proc_thread_ref(lpt) < 2) continue;

    if (lives_proc_thread_paused_idling(lpt)) {
      lives_proc_thread_unref(lpt);
      continue;
    }

    if (lives_proc_thread_was_cancelled(lpt)) {
      remove_from_hstack(hstack, list);
      lives_proc_thread_unref(lpt);
      continue;
    }

    hstack->flags |= STACK_TRIGGERING;

    lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                     | THRD_STATE_FINISHED);

    closure->flags |= HOOK_STATUS_RUNNING;

    lives_proc_thread_queue(lpt, LIVES_THRDATTR_FAST_QUEUE);

    ncount++;

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
  lives_proc_thread_t poller = lives_proc_thread_create(LIVES_THRDATTR_SET_CANCELLABLE,
                               (lives_funcptr_t)_lives_hooks_tr_seq, -1, "viFv",
                               hstacks, type, (weed_funcptr_t)finfunc, findata);
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
      lives_funcinst_t *finst;
      lives_closure_t *closure = (lives_closure_t *)cblist->data;
      cbnext = cblist->next;
      if (closure) {
        lives_funcdef_t *fdef;
        lives_proc_thread_t lpt = closure->proc_thread;
        if (!lpt) continue;
        finst = lives_proc_thread_get_funcinst(lpt);
        fdef = finst->funcdef;
        if (fdef->function != func) continue;
        if (data != weed_get_voidptr_value(finst->params, make_std_pname(1), NULL)) continue;
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
  uint64_t hs_op_flags;

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

  if (!(hstack->flags & STACK_TRIGGERING)) return;

  hmutex = &(hstack->mutex);
  PTMLH;

  hs_op_flags = lives_hookstack_op_flags(htype);
  if (!(hs_op_flags & HOOKSTACK_ASYNC_PARALLEL)) {
    PTMUH;
    return;
  }

  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist_next) {
    cblist_next = cblist->next;
    closure = (lives_closure_t *)cblist->data;
    if (!closure) continue;
    if (closure->flags & HOOK_STATUS_BLOCKED) continue;
    lpt = closure->proc_thread;
    if (!lpt || lives_proc_thread_ref(lpt) < 2) continue;

    PTMUH;
    lives_proc_thread_wait_done(lpt, 0., FALSE);
    PTMLH;

    if (closure->flags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT)
        || (hs_op_flags & HOOKSTACK_ALWAYS_ONESHOT)) {
      remove_from_hstack(hstack, cblist);
      lives_proc_thread_unref(lpt);
      continue;
    }

    closure->flags &= ~HOOK_STATUS_RUNNING;

    if (lives_proc_thread_was_cancelled(lpt)) {
      remove_from_hstack(hstack, cblist);
      lives_proc_thread_unref(lpt);
      continue;
    }
    lives_proc_thread_unref(lpt);
  }
  hstacks[htype]->flags &= ~STACK_TRIGGERING;
  PTMUH;
}


boolean fn_data_match(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2, int maxp) {
  funcsig_t fsig1 = lives_proc_thread_get_funcsig(lpt1);
  int nparms = get_funcsig_nparms(fsig1);
  lives_funcinst_t *finst1 = lives_proc_thread_get_funcinst(lpt1);
  lives_funcinst_t *finst2 = lives_proc_thread_get_funcinst(lpt2);
  if (!maxp || maxp > nparms) maxp = nparms;
  for (int i = 0; i < maxp; i++) {
    char *pname = make_std_pname(i);
    if (weed_leaf_elements_equate(finst1->params, pname, finst2->params, pname, -1) == WEED_FALSE) {
      lives_free(pname);
      return FALSE;
    }
    lives_free(pname);
  }
  return TRUE;
}


static boolean is_child_of(LiVESWidget *w, LiVESContainer *C) {
  if ((LiVESWidget *)w == (LiVESWidget *)C) return TRUE;
  LiVESList *children = lives_container_get_children(C);
  for (LiVESList *list = children; list; list = list->next) {
    LiVESWidget *x = (LiVESWidget *)list->data;
    if (LIVES_IS_CONTAINER(x) && is_child_of(w, LIVES_CONTAINER(x))) {
      lives_list_free(children);
      return TRUE;
    }
  }
  if (children) lives_list_free(children);
  return FALSE;
}


static boolean fn_match_child(lives_proc_thread_t lpt1, lives_proc_thread_t lpt2) {
  lives_funcinst_t *finst;
  LiVESWidget *w, *C;
  char *pname;
  char *args_fmt = lives_proc_thread_get_args_fmt(lpt1);
  if (!args_fmt || get_seedtype(args_fmt[0]) != WEED_SEED_VOIDPTR) {
    if (args_fmt) lives_free(args_fmt);
    return FALSE;
  }
  lives_free(args_fmt);
  pname = make_std_pname(0);
  finst = lives_proc_thread_get_funcinst(lpt2);
  C = (LiVESWidget *)(weed_get_voidptr_value(finst->params, pname, NULL));
  if (!LIVES_IS_WIDGET(C) || !LIVES_IS_CONTAINER(C)) {
    lives_free(pname);
    return FALSE;
  }
  finst = lives_proc_thread_get_funcinst(lpt1);
  w = (LiVESWidget *)(weed_get_voidptr_value(finst->params, pname, NULL));
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
  if (!lpt1 || !lpt2 || lives_proc_thread_get_function(lpt1)
      != lives_proc_thread_get_function(lpt2)) return -1;
  if (lives_proc_thread_get_rtype(lpt1) != lives_proc_thread_get_rtype(lpt2)) return 2;
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
    fdef = lives_proc_thread_get_funcdef(lpt);
    if (!fdef) {
      g_print("No funcdef\n");
      continue;
    }
    char *args_fmt = args_fmt_from_funcsig(fdef->funcsig);
    g_print("Funcdef: fname = %s, args_fmt '%s', return_type %u\n\n",
            fdef->funcname, args_fmt, fdef->return_type);
    lives_free(args_fmt);
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
    int32_t return_type,  const char *args_fmt,
    const char *file, int line, uint64_t flags) {
  lives_funcdef_t *fdef = (lives_funcdef_t *)lives_calloc(1, sizeof(lives_funcdef_t));
  if (fdef) {
    if (return_type < 0) return_type = 0;
    if (funcname) fdef->funcname = lives_strdup(funcname);
    else fdef->funcname = NULL;
    fdef->uid = gen_unique_id();
    fdef->function = function;
    fdef->return_type = return_type;
    fdef->funcsig = funcsig_from_args_fmt(args_fmt);
    if (file) fdef->file = lives_strdup(file);
    fdef->flags = flags;
    fdef->line = line;
  }
  return fdef;
}


LIVES_GLOBAL_INLINE lives_funcinst_t *create_funcinst(lives_funcdef_t *tmpl, void *privdata) {
  LIVES_CALLOC_TYPE(lives_funcinst_t, finst, 1);
  if (finst) {
    finst->funcdef = tmpl;
    finst->priv = privdata;
  }
  return finst;
}


LIVES_GLOBAL_INLINE void lives_funcinst_free(lives_funcinst_t *finst) {
  if (finst) {
    if (finst->params) weed_plant_free(finst->params);
    lives_free(finst);
  }
}


LIVES_GLOBAL_INLINE void lives_funcdef_free(lives_funcdef_t *fdef) {
  if (fdef && !(fdef->flags & FDEF_FLAG_STATIC)) {
    lives_freep((void **)&fdef->funcname);
    lives_freep((void **)&fdef->file);
    lives_free(fdef);
  }
}


lives_result_t lives_funcinst_bind_param(lives_funcinst_t *finst, int idx, uint64_t flags, ...) {
  // idx == -1 return loc (optional)
  va_list ap;
  void *valptr;
  if (!finst || idx < -1) return LIVES_RESULT_INVALID;
  lives_funcdef_t *fdef = finst->funcdef;
  if (!fdef || !fdef->function) return LIVES_RESULT_INVALID;
  int nparms = get_funcsig_nparms(fdef->funcsig);
  if (idx >= nparms) return LIVES_RESULT_ERROR;
  va_start(ap, flags);
  if (idx == -1) {
    if (!fdef->return_type) return LIVES_RESULT_ERROR;
    // this will be a pointer to var to receive return value in
    valptr = va_arg(ap, void *);
    va_end(ap);
    weed_set_voidptr_value(finst->params, "retloc", valptr);
    return LIVES_RESULT_SUCCESS;
  }

  uint32_t st = nth_seed_type(fdef->funcsig, idx);
  weed_error_t err = WEED_SUCCESS;

  if (flags & FINST_FLAG_SHADOWED) {
    valptr = va_arg(ap, void *);
    BIND_SHADOW_PARAM(finst, idx, st, valptr);
  } else {
    char *pkey = make_std_pname(idx);
    err = weed_leaf_from_varg(finst->params, pkey, st, idx, ap);
    lives_free(pkey);
  }
  va_end(ap);

  return err;
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
    note = lives_plant_new(LIVES_PLANT_BAG_OF_HOLDING);
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
    if (fdef) lives_funcdef_free(fdef);
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
      _ext_free(items[i]);
    }
    _ext_free(items);
  }
}


LIVES_GLOBAL_INLINE char *get_argstring_for_func(lives_funcptr_t func) {
  const lives_funcdef_t *fdef = get_template_for_func(func);
  if (!fdef) return NULL;
  return funcsig_to_param_string(fdef->funcsig);
}


char *lives_funcdef_explain(const lives_funcdef_t *funcdef) {
  if (funcdef) {
    char *tmp, *out =
      lives_strdup_printf("Function with uid 0X%016lX has prototype:\n"
                          "\t%s %s(%s)\n function category is %d", funcdef->uid,
                          weed_seed_to_ctype(funcdef->return_type, FALSE),
                          funcdef->funcname ? funcdef->funcname : "??????",
                          (tmp = funcsig_to_param_string(funcdef->funcsig)),
                          funcdef->category);
    lives_free(tmp);
    return out;
  }
  return NULL;
}
