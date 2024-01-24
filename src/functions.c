// functions.c
// (c) G. Finch 2002 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

static const hookstack_descriptor_t hs_desc[N_HOOK_POINTS];

static boolean hs_inited = FALSE;

VARNAME_FUNC

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
      //trigger = LIVRS_LEAF_THRD_STATE;
      //test_pre = "^%%lu & %lu:
      // etc
      break;
    case FINISHED_HOOK:
      flags = HS_FLAGS_FINISHED;
      //triggr = LIVRS_LEAF_THRD_STATE;
      break;
    case ERROR_HOOK:
      flags = HS_FLAGS_ERROR;
      //triggr = LIVRS_LEAF_THRD_STATE;
      break;
    case CANCELLED_HOOK:
      flags = HS_FLAGS_CANCELLED;
      // triggr = LIVRS_LEAF_THRD_STATE;
      break;
    case DESTRUCTION_HOOK:
      flags = HS_FLAGS_DESTRUCTION;
      //triggr = LIVRS_LEAF_THRD_STATE;
      break;
    default: break;
    }

    xhs->pattern = pat;
    xhs->op_flags = flags;

    if (i > N_GLOBAL_HOOKS) {
      xhs->args_fmt_start = "v";
      xhs->ret_type = WEED_SEED_BOOLEAN;
    }
  }
}


const hookstack_descriptor_t *get_hs_desc(int hstype) {
  if (!hs_inited) init_hook_stacks();
  return &hs_desc[hstype];
}


void toggle_var_cb(void *dummy, boolean *var) {if (var) *var = !(*var);}
void inc_counter_cb(void *dummy, int *var) {if (var)(*var)++;}
void dec_counter_cb(void *dummy, int *var) {if (var)(*var)--;}
void resetc_counter_cb(void *dummy, int *var) {if (var) *var = 0;}

#define PTMLH do {pthread_mutex_lock(hmutex);} while (0)
#define PTMUH do {pthread_mutex_unlock(hmutex);} while (0)
#define PTMTLH pthread_mutex_trylock(hmutex)

const lookup_tab crossrefs[] = XREFS_TAB;


#define _SET_LEAF_FROM_VARG(plant, pkey, type, ctype, cptrtype, ne, args) \
  (ne == 1 ? weed_set_##type##_value((plant), (pkey), va_arg((args), ctype)) \
   : weed_set_##type##_array((plant), (pkey), (ne), va_arg((args), cptrtype)))

#define SET_LEAF_FROM_VARG(plant, pkey, type, ne, args) _SET_LEAF_FROM_VARG((plant), (pkey), type, \
									    CTYPE(type), CPTRTYPE(type), (ne), (args))
weed_error_t weed_leaf_from_varg(weed_plant_t *plant, const char *key, weed_seed_t type, weed_size_t ne, va_list xargs) {
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
      if (ptr && isstck(ptr)) {
	g_print("Warning - wlfv, key = %s, isstack = 1\n", key);
      }
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


#define _SET_LEAF_FROM_VARGP(plant, pkey, type, cptrtype, ne, args)	\
  (ne == 1 ? weed_set_##type##_value((plant), (pkey), (*(cptrtype)va_arg((args), cptrtype))) \
   : weed_set_##type##_array((plant), (pkey), (ne), (*(cptrtype *)va_arg((args), cptrtype))))

#define SET_LEAF_FROM_VARGP(plant, pkey, type, ne, args) _SET_LEAF_FROM_VARGP((plant), (pkey), type, \
									      CPTRTYPE(type), (ne), (args))

weed_error_t weed_leaf_from_vargp(weed_plant_t *plant, const char *key, weed_seed_t type, weed_size_t ne, va_list xargs) {
  switch (type) {
  case WEED_SEED_INT: return SET_LEAF_FROM_VARGP(plant, key, int, ne, xargs);
  case WEED_SEED_DOUBLE: return SET_LEAF_FROM_VARGP(plant, key, double, ne, xargs);
  case WEED_SEED_BOOLEAN: return SET_LEAF_FROM_VARGP(plant, key, boolean, ne, xargs);
  case WEED_SEED_STRING:  return SET_LEAF_FROM_VARGP(plant, key, string, ne, xargs);
  case WEED_SEED_INT64: return SET_LEAF_FROM_VARGP(plant, key, int64, ne, xargs);
#ifdef WEED_SEED_UINT
  case WEED_SEED_UINT: return SET_LEAF_FROM_VARGP(plant, key, uint, ne, xargs);
#endif
#ifdef WEED_SEED_UINT64
  case WEED_SEED_UINT64: return SET_LEAF_FROM_VARGP(plant, key, uint64, ne, xargs);
#endif
    /* #ifdef WEED_SEED_FLOAT */
    /*   case WEED_SEED_FLOAT: return SET_LEAF_FROM_VARGP(plant, key, float, ne, xargs); */
    /* #endif */
  case WEED_SEED_FUNCPTR: return SET_LEAF_FROM_VARGP(plant, key, funcptr, ne, xargs);
  case WEED_SEED_VOIDPTR:  {
    if (prefs->show_dev_opts) {
      void *ptr;
      va_list vc;
      va_copy(vc, xargs);
      ptr = va_arg(vc, void *);
      va_end(vc);
      if (ptr && isstck(ptr)) {
	g_print("Warning - wlfv, key = %s, isstack = 1\n", key);
      }
    }
    return SET_LEAF_FROM_VARGP(plant, key, voidptr, ne, xargs);
  }
  case WEED_SEED_PLANTPTR: return SET_LEAF_FROM_VARGP(plant, key, plantptr, ne, xargs);
  default: return WEED_ERROR_WRONG_SEED_TYPE;
  }
}


LIVES_GLOBAL_INLINE lives_result_t weed_leaf_from_vap(weed_plant_t *plant, const char *key, weed_seed_t st, ...) {
  va_list xargs;
  weed_error_t err;
  va_start(xargs, st);
  err = weed_leaf_from_vargp(plant, key, st, 1, xargs);
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

char *make_std_pname(int pn) {return lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, pn);}
char *make_proxy_pname(int pn) {return lives_strdup_printf("%s%d_proxy", LIVES_LEAF_THREAD_PARAM, pn);}

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

    // TODO
    // TODO - for request hooks, add a callback 0 request response
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


lives_result_t funcinst_params_from_vargs(lives_funcinst_t *finst,  va_list xargs) {
  lives_result_t res = LIVES_RESULT_INVALID;
  if (finst) {
    uint64_t attrs = lives_funcinst_get_attrs(finst);
    lives_funcdef_t *fdef = finst->funcdef;
    char *args_fmt = args_fmt_from_funcsig(fdef->funcsig);
    if (!args_fmt) return WEED_SUCCESS;
    if (!finst->params) finst->params = lives_plant_new(LIVES_PLANT_FUNCPARAMS);
    res = weed_plant_params_from_valist(finst->params, attrs, args_fmt, make_std_pname, xargs);
    lives_free(args_fmt);
  }
  return res;
}


/* LIVES_GLOBAL_INLINE lives_proc_thread_t lpt_from_funcdef_va(lives_funcdef_t *fdef, */
/*     lives_thread_attr_t attrs, va_list vargs) { */
/*   lives_proc_thread_t lpt = NULL; */
/*   if (fdef) { */
/*     char *args_fmt = args_fmt_from_funcsig(fdef->funcsig); */

/*     lpt =  _lives_proc_thread_create_vargs(attrs, fdef->function, fdef->funcname, */
/*                                            fdef->return_type, args_fmt, vargs); */

/*     if (args_fmt) lives_free(args_fmt); */
/*   } */
/*   return lpt; */
/* } */


LIVES_GLOBAL_INLINE lives_proc_thread_t _lpt_from_funcdefX(lives_funcdef_t *fdef, char **anames, lives_thread_attr_t attrs, ...) {
  lives_proc_thread_t lpt;
  lives_funcinst_t *finst;
  va_list ap;
  va_start(ap, attrs);
  lpt = lpt_from_funcdef_va(fdef, attrs, ap);
  va_end(ap);
  finst = lives_proc_thread_get_funcinst(lpt);
  finst->paramnames = anames;
  return lpt;
}


void *lives_proc_thread_execute_retvoidptr(weed_plant_t **plantp, char **anames, lives_funcptr_t func, ...) {
  uint64_t uid = gen_unique_id();
  char *key = lives_strdup_printf("key_%lu", uid);
  void *retval;
  lives_proc_thread_t lpt;
  lives_funcdef_t *fdef = NULL;//lookup_fdef(NULL, func);
  va_list ap;
  va_start(ap, func);
  lpt = _lpt_from_funcdefX(fdef, anames, LIVES_THRDATTR_START_UNQUEUED, ap);
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

lives_result_t do_call(lives_funcinst_t *finst) {
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

  if (!finst) return LIVES_RESULT_INVALID;
  
  lives_funcdef_t *fdef = finst->funcdef;

  if (!fdef) return LIVES_RESULT_INVALID;

  weed_funcptr_t func = fdef->function;

  if (!func) return LIVES_RESULT_INVALID;

  lives_result_t res;

  int sjval = 0;
  jmp_buf env;

  boolean is_stacked = FALSE;
  uint32_t ret_type = fdef->return_type;
  funcsig_t sig = fdef->funcsig;
  int nparms = get_funcsig_nparms(sig);

  allfunc_t thefunc;

  if (ret_rtype == WEED_SEED_CONST_CHARPTR)
    ret_type = WEED_SEED_VOIDPTR;

  thefunc.func = func;

  // if we have "bound" params, update them from live variables now
  update_params_from_proxies(finst);

  if (MODULE_TYPE_ID(finst, HOOK_STACK)) is_stacked = TRUE;
  
  //ljlist = (LiVESList *) weed_get_voidptr_value(lpt, LIVES_LEAF_LONGJMP_LIST, NULL);						
  if (!is_stacked) sjval = sigsetjmp(env, 1);

  if (!sjval) {
    if (!is_stacked) LPT_DATA(finst, lj_stack)) = lives_sync_list_push(finst->lj_stack, (void *)&env);
    // if we cancel, this call never returns, instead we hit the else

    switch (nparms) {
    case 0: _DC_(0,); break;

    case 1:
      switch (sig) {
	ONE_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      }
      break;
    case 2:
      switch (sig) {
	TWO_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

    case 3:
      switch (sig) {
	THREE_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

    case 4:
      switch (sig) {
	FOUR_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

    case 5:
      switch (sig) {
	FIVE_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

    case 6:
      switch (sig) {
	SIX_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

    case 7:
      switch (sig) {
	SEVEN_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

    case 8:
      switch (sig) {
	EIGHT_PARAM_FUNCSIGS
      default: return LIVES_RESULT_INVALID;
      } break;

      // invalid nparms
    default: return LIVES_RESULT_INVALID;
    }
  }

  /* point of return for threads if they cancel / error */

  if (lives_funcinst_error_state(finst)) ret = LIVES_RESULT_ERROR;
  else ret = LIVES_RESULT_CANCELLED;

  if (!is_stacked) lives_sync_list_pop(&LPT_DATA(finst, lj_stack));

 done:
#if USE_RPMALLOC
  rpmalloc_thread_collect();
#endif

  if (res != LIVES_RESULT_SUCCESS) return res;

  if (weed_plant_has_leaf(finst->params, _RV_)) {
    // copy finst rv to lpt when joined
    // in case we have a chain, we dont want to overwrite chain value
    // but we will set retloc for finst
    if (finst->retloc) finst->flags |= FINST_FLAG_NOFREE_RETLOC;
    else finst->flags &= ~FINST_FLAG_NOFREE_RETLOC;
    copy_leaf_value(dinst->params, _RV_, 0, fdef->return_type, &finst->retloc);
  }
  return LIVES_RESULT_SUCCESS;
}


#undef _IF_
#undef _DC_

//#define DEBUG_FN_CALLBACKS
boolean call_funcsig(lives_proc_thread_t lpt) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint64_t attrs = lives_proc_thread_get_attrs(lpt);
  weed_error_t err = WEED_SUCCESS;
  lives_funcinst_t *finst;
  lives_result_t ret;

  if (lpt == mainw->debug_ptr) g_print("nrefss mmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  if (!lpt || lives_proc_thread_ref(lpt) < 2) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid proc_thread");
    return FALSE;
  }

  lives_funcinst_t *finst = lives_proc_thread_get_active_funcinst(lpt);
  if (!finst || !finst->funcdef || !finst->funcdef->function) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid funcsig");
    return FALSE;
  }

  // STATE CHANGED - queued / preparing -> running
  lives_proc_thread_include_states(lpt, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_QUEUED | THRD_STATE_UNQUEUED | THRD_STATE_DEFERRED |
                                   THRD_STATE_PREPARING);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_START_TICKS, lives_get_current_ticks());

  if (lpt == mainw->debug_ptr) g_print("nrefs PPPmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  // set a longjump pointer. if the we then error or cancel while running the funcion,
  // we will jump back to this point


  if (lpt == mainw->debug_ptr) g_print("nrefss AAAAmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

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

    //todo
    // disarm the free_lpts
    lpt_params_free(lpt, FALSE);
    return TRUE;
  }
  msg = lives_strdup_printf("Got error %d running procthread ", err);
  goto funcerr2;

 funcerr:
  // invalid args_fmt
  if (1) {
    funcsig_t sig = lives_proc_thread_get_funcsig(lpt);
    int nparms = get_funcsig_nparms(sig);
    char *msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu), nparams = %d\n", sig, sig, nparms);
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
  }

 funcerr2:

  if (!msg) msg = lives_strdup_printf("Got error %d running function with type "
				      "0x%016lX (%lu)", err, sig, sig);

  lives_proc_thread_error_full(lpt, NULL, 0, 123, LPT_ERR_CRITICAL, msg);

  lives_free(msg);
  lives_proc_thread_unref(lpt);
  return LIVES_RESULT_INVALID;
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
  // turn funcsig into symstring (same format as listed in funcsigs.h)
  if (!sig) return lives_strdup("void");
  char *fmtstring = lives_strdup("");
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



char *func_category_to_text(int cat) {
  switch (cat) {
  case FUNC_CATEGORY_CALLBACK:
    return lives_strdup(_("hook callback"));
  case FUNC_CATEGORY_HOOK_GUI:
    return lives_strdup(_("GUI hook callback"));
  case FUNC_CATEGORY_UTIL:
    return lives_strdup(_("utility function"));
  default:
    return lives_strdup(_("generic function"));
  }
}


static int lives_fdef_get_category(int cat, int dtl) {
  int category = FUNC_CATEGORY_GENERAL;
  uint64_t hs_op_flags = 0;
  switch (cat) {
  case FUNC_CATEGORY_CALLBACK:
    /* hs_op_flags = lives_hookstack_op_flags(dtl); */
    /* HOOKSTACK_FLAGS_ADJUST(hs_op_flags); */
    /* if (hs_op_flags & HOOKSTACK_ASYNC_PARALLEL) { */
    /*   category = FUNC_CATEGORY_ASYNC_HOOK; */
    /*   break; */
    /* } */
    /* if (hs_op_flags & HOOKSTACK_GUI_THREAD) { */
    /*   category = FUNC_CATEGORY_HOOK_GUI; */
    /*   break; */
    /* } */
    /* // if stack owner is self, we need to async_join */
    /* if (hs_op_flags & HOOKSTACK_NATIVE) { */
    /*   category = FUNC_CATEGORY_STRUCTURAL; */
    /*   break; */
    /* } */
    break;
  default: break;
  }
  return category;
}


void flush_cb_list(lives_proc_thread_t lpt) {
  // -For callbacks added to another proc_thread's stack:
  //   unless the stack descriptor is flagged as PERSISTENT, or unless the callback
  //   was added with HOOK_CB_PERSISTENT, then the ADDING proc_thread maintains a pointer to
  //   the each callback added, appending the address to its EXT_CB_LIST. Just before a proc_thread is freed,
  //   the ext_cb_list will be cleared - each callback will be flagged for removal, and dispatcher set to NULL
  //
  //  conversely if the callback closure  is to be freed - for example if it is a oneshot callback,
  // or when the proc_thread owning the stack is being freed,
  // we check if there is an 'dispatcher' for the callback.
  // This information is used to remove the callback from the ext_cb_list of the dispatcher.
  //
  // A mutex lock is used to ensure single access to the cb_list
  // however this creates a race condition - B reads closure and gets dispatcher A
  // A is freed, B attempts to find cb_list for A
  //
  // to prevent this we proceed as follows:
  // B locks the closure, C
  // B gets dispatcher A from C
  // B gets lock on A cb_list
  // B removes C from A's list
  // B unlocks A list
  // B unlocks C
  // meanwhile...
  // A locks cb_list
  // A gets lock on C
  // A removes dispatcher from C
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
      lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
      if (finst) {
        if (pthread_mutex_trylock(&(CL_DATA(finst, mutex)))) {
          weed_set_voidptr_value(lpt, LIVES_LEAF_EXT_CB_LIST, xlist);
          pthread_mutex_unlock(extcb_mutex);
          restart = TRUE;
          break;
        }
        finst->dispatcher = NULL;
        CL_DATA(finst, cb_flags) |= HOOK_STATUS_REMOVE;
	pthread_mutex_unlock(&(CL_DATA(finst, mutex)));
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


static void examine_ext_cb_other(lives_funcinst_t *tinst) {
  if (tinst) {
    lives_proc_thread_t lpt = tinst->dispatcher;
    if (lpt) {
      pthread_mutex_t *extcb_mutex = (pthread_mutex_t *)weed_get_voidptr_value
	(lpt, LIVES_LEAF_EXT_CB_MUTEX, NULL);
      pthread_mutex_lock(extcb_mutex);
      if (tinst->dispatcher == lpt) remove_ext_cb_other(tinst);
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



void lives_hook_cb_free(lives_funcinst_t *finst) {
  // with state mutex locked
  if (finst) {
    pthread_mutex_lock(&finst->mutex);
    if (finst->dispatcger) examine_ext_cb_other(finst);
    pthread_mutex_unlock(&finst->mutex);
    lives_funcinst_free(finst);
  }
}


////// hook functions /////
LIVES_GLOBAL_INLINE uint64_t get_hs_op_flags(lives_hook_stack_t *hstack) {
  uint64_t opflags = 0;
  if (hstack) {
    opflags = hstack->hsdesc->op_flags;
    HOOKSTACK_FLAGS_ADJUST(opflags);
  }
  return opflags;
}


LIVES_GLOBAL_INLINE void lives_hook_clear(lives_hook_stack_t **hstacks, int type) {
  if (hstacks) {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = &(hstack->mutex);
    LiVESList *hsstack;

    uint64_t hs_op_flags = get_hs_op_flags(hstack);

    if (hs_op_flags & HOOKSTACK_ASYNC_PARALLEL) {
      // if stack owner is self, we need to async_join
      if (!(hs_op_flags & HOOKSTACK_NATIVE)) {
        GET_PROC_THREAD_SELF(self);
        if (hstack->owner.lpt == self) {
          if (hstack->flags & HS_FLAG_TRIGGERING) PTMUH;
          lives_hooks_async_join(NULL, type);
        }
      }
    }

    while (1) {
      // caution - astbc stacks can stay triggered for a long time
      lives_microsleep_until_zero(hstack->flags & HS_FLAG_TRIGGERING);
      PTMLH;
      if (hstack->flags & HS_FLAG_TRIGGERING) PTMUH;
      else break;
    }

    hsstack = (LiVESList *)hstack->stack;
    if (hsstack) {
      for (LiVESList *cblist = hsstack; cblist; cblist = cblist->next) {
        lives_funcinst_t *finst = (lives_funcinst_t *)cblist->data;
	if (finst) {
	  if (finst->waiters) 
	    lives_hook_cb_free(finst);
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
      if (hstacks[i]) {
	lives_hooks_clear(hstacks, i);
	pthread_mutex_destroy(&hstacks[i]->mutex);
	lives_free(hstacks[i]);
      }
    }
}


static lives_result_t duplicate_params(lives_funcinst_t *dst, lives_funcinst_t *src, int nparams) {
  if (nparams <= 0 || !dst || !src) return LIVES_RESULT_INVALID;

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
  // todo
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


boolean fn_data_replace(lives_funcinst_t *dst, lives_funcinst_t *src) {
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
  lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
  hstack->stack = (volatile LiVESList *)lives_list_remove_node
    ((LiVESList *)hstack->stack, finst, FALSE);
  lives_funcinst_free(finst);
}


void lives_hook_cb_wake_waiters(LiVESList *list) {
  while (list) {
    LiVESList *xln;
    for (LiVESList *xl = list; xl; xl = xln) {
      lives_wait_obj *waiter = (lives_wait_obj *)xl->data;
      xln = xl->next;
      if (!pthread_mutex_trylock(&waiter->mutex)) {
	if (waiter->expired) {
	  pthread_mutex_unlock(&waiter->mutex);
	  list = lives_list_remove_node(list, xl, TRUE);
	  continue;
	}
	if (!waiter->woken) {
	  if (!lives_proc_thread_is_paused(waiter->lpt)) continue;
	  waiter->woken = TRUE;
	  lives_proc_thread_request_resume(waiter->lpt);
	  list = lives_list_remove_node(list, xl, FALSE);
	}
	pthread_mutex_unlock(&waiter->mutex);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


LIVES_GLOBAL_INLINE void lives_proc_thread_expire_wait_object(lives_proc_thread_t self) {
  lives_wait_obj *waiter = (lives_wait_obj *)weed_get_voidptr_value(self, LIVES_LEAF_WAITER, NULL);
  if (waiter) {
    pthread_mutex_lock(&waiter->mutex);
    waiter->expired = TRUE;
    pthread_mutex_unlock(&waiter->mutex);
    if (woken) waiter_obj_free(waiter);
    //
  }
}


LIVES_GLOBAL_INLINE lives_wait_obj *lives_proc_thread_create_wait_object(lives_proc_thread_t self) {
  LIVES_CALLOC_TYPE(lives_wait_obj, waiter, 1);
  pthread_mutex_init(&waiter->mutex);
  waiter->lpt = self;
  return waiter;
}



lives_funcinst_t *lives_hook_cb_add(lives_hook_stack_t **hstacks, int type, lives_funcinst_t finst, uint64_t dflags) {
  if (!finst) return NULL;

  lives_funcinst_t *xfinst = NULL, *ret_funst = NULL;
  uint64_t flags = CL_DATA(finst, cb_flags)l
  uint64_t xflags = flags & (HOOK_UNIQUE_REPLACE | HOOK_INVALIDATE_DATA | HOOK_TOGGLE_FUNC);
  pthread_mutex_t *hmutex;

  boolean is_close = FALSE, is_append = TRUE, is_remove = FALSE;

  boolean have_lock = FALSE;
  boolean is_self_stack = FALSE;
  uint64_t hs_op_flags;

  GET_PROC_THREAD_SELF(self);

  lives_funcinst_set_disposition(finst, DISPOSITION_STACKED);

  if (!hstacks) return NULL;

  hstack = hstacks[type];
  if (!hstack) return NULL;

  hs_op_flags = get_hs_op_flags(hstack);

  if (self && hstacks == lives_proc_thead_get_hook_stacks(self)) {
    if (hstack->owner.lpt == self)
      is_self_stack = TRUE;
  }
  finst->adder = self;

  if (flags & HOOK_CB_PRIORITY) is_append = FALSE;
  if (dtype & DTYPE_PREPEND) is_append = FALSE;
  if (dtype & DTYPE_NOADD) is_remove = TRUE;
  if (dtype & DTYPE_HAVE_LOCK) have_lock = TRUE;

  // append, then everything else will check
  if (is_append) xflags &= ~(HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);

  if (!is_append) ret_finst = finst;

  hmutex = &hstack->mutex;
 
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

  //lives_proc_thread_t lpt2 = NULL;
  LiVESList *cblist, *cblistnext;
  int maxp = CL_DATA(finst, nmatch_params);
  boolean fmatch;

  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblistnext) {
    uint64_t cfinv = 0;
    cblistnext = cblist->next;
    xfinst = (lives_funcinst_t *)cblist->data;
    if (!xfinst) continue;

    if ((CL_DATA(xfinst, hook_cb_flag)
	 & (HOOK_STATUS_BLOCKED | HOOK_STATUS_ACTIONED | HOOK_STATUS_RUNNING)) continue;

    while (xfinst = CL_DATA(xfinst, replacement));

    // check uniqueness restrictions when adding a new callback
    if (is_append) {
      cfinv = flags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
      if (!(cfinv & HOOK_INVALIDATE_DATA)) cfinv = 0;
    }

    // check if the function matches (unless we are just invalidating data)
    if (!(fmatch = (fn_func_match(finst, xfinst) >= 0))
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
	if (!fn_data_match(xfinst, finst, maxp)) continue;
	if (!ret_finst) {
	  ret_finst = xfinst;
	  // skip over if the target stack is triggering
	  if (hstacks->flags & HS_FLAG_TRIGGERING) break;
	  if (!(xflags & HOOK_TOGGLE_FUNC)) {
	    if (!fn_data_match(xfinst, finst, 0)) {
	      fn_data_replace(xfinst, finst);
	      lives_funcinst_send_reply(xfinst, LIVES_REPLY_NO);
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

      if (!ret_finst) {
	// if this is the first match, then we wont remove it, unless fn toggles
	ret_finst = xfinst;
	if (!(xflags & HOOK_TOGGLE_FUNC)) {
	  if (is_append) break;
	  continue;
	}
	if (hstack->flags & HS_FLAG_TRIGGERING) {
	  ret_finst = NULL;
	  break;
	}
      }

      // here we have established that the closure must be removed
      // since we already found a match, or we will prepend, or it is a "toggle" function
      CL_DATA(finst, hook_cb_flags) |= HOOK_STATUS_REMOVE;
      lives_funcinst_send_reply(finst, LIVES_REPLY_NO);

      if (xflags & HOOK_TOGGLE_FUNC) {
	// for toggle func, this closure will be removed, and new lpt will be rejected
	// so neither will be replaced, but marking the proc_threads as "invalid" we force the waiters to give  up
	// but we will return lpt2 (we need to return somehting different)
	// but we will flag it as invalid,
	lives_funcinst_send_reply(finst, LIVES_REPLY_NO);
	break;
      }

      if (finst->waiters) {
	    // if replacing a closure with BLOCKing flagged, we need to get the blocked threads to wait on
	    // the closure which replaced theirs. We will return a pointer the the replacement
	    // and we also need to add a ref to lpt2, since the waiter will unref it,
	    // and it will also be unreffed when the closure is freeud
	    // we will also add a ref to ret_closure->proc_thread, for similar reasons
	if (ret_finst != finst) {
	  ret_finst->waitres = lives_list_concat(ret_finst->waiters, finst->waiters);
	  finst->waiters = NULL;
	}
	continue;
      }

      if (!(cfinv || (xflags & (HOOK_INVALIDATE_DATA | HOOK_UNIQUE_DATA)))) continue;

      if (!fn_data_match(xfinst, lpt, maxp)) {
	if (!((cfinv | flags) & HOOK_OPT_MATCH_CHILD)) continue;
	if (!(((flags & HOOK_OPT_MATCH_CHILD) && fn_match_child(finst, xfinst))
	      || ((cfinv & HOOK_OPT_MATCH_CHILD) && fn_match_child(xfinst, finst))))
	  continue;
      }

      // if we reach here it means that xflags == HOOK_UNIQUE_DATA, or closure->flags
      // had INVALIDATE_DATA, and we found a match
      // we wont remove anything, but we will reject an append
      if (!ret_finst) {
	ret_finst = xfinst;
	break;
      }

      CL_DATA(xfinst, cb_flags) |= HOOK_STATUS_REMOVE;
      continue;
    }
  }

  if (is_remove) {
    if (!have_lock) PTMUH;
    return finst;
  }

  if (ret_finst && ret_finst != xfinst) {
    if (!have_lock) PTMUH;
    if (CL_DATA(finst, cb_flags) & HOOK_CB_BLOCK) {
      finst->replacement = finst2;
      finst2->waiters =
	lives_list_prepend(finst2->waiters,
			   (void *)lives_proc_thread_create_wait_object(self);
    }
    return finst2;
  }

  if (!is_self_stack && !(hs_op_flags & HOOKSTACK_PERSISTENT) && !(flags & HOOK_CB_PERSISTENT)) {
    // add a pointer to the callback if we added it to the hook stack for another thread
    // this is done so that we can remove any external callbacks when the proc_thread is freed
    // however, we don't do this for self hooks (we can simply clear those)
    // also we dont do this if transferring ownership of the callback (as when adding to the
    // player's sync_announce stack)
    finst->adder = self;
    add_to_cb_list(self, finst);

    lives_send_reply(finst, LIVES_REPLY_YES);
    
    if (is_append) hstack->stack = lives_list_append((LiVESList *)hstack->stack, finst);
    else hstacks[type]->stack = lives_list_prepend((LiVESList *)hstack->stack, finst);

    CL_DATA(finst, hook_stacks) = hstacks;
    CL_DATA(finst, hook_type) = type;

    if (!have_lock) PTMUH;

    if (lpt_attrs & LIVES_THRDATTR_NOTE_TIMINGS)
      weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());
  }
  return finst->uid;  
}


static lives_proc_thread_t update_linked_stacks(lives_closure_t *cl, uint64_t flags) {
  uint64_t dflags = DTYPE_NOADD | DTYPE_PREPEND | DTYPE_HAVE_LOCK | DTYPE_CLOSURE;
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  if (!is_fg_thread()) {
    GET_PROC_THREAD_SELF(self); lives_hook_stack_t **mystacks = lives_proc_thread_get_hook_stacks(self);
    lives_microsleep_until_zero(pthread_mutex_lock(&mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex));
    if (!(mainw->global_hook_stacks[LIVES_GUI_HOOK]->flags & HS_FLAG_TRIGGERING))
      lives_hook_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, flags, (void *)cl, dflags);
    pthread_mutex_unlock(&mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);
    for (LiVESList *links = mainw->all_hstacks; links; links = links->next) {
      lives_hook_stack_t **xhs = (lives_hook_stack_t **)links->data;
      if (xhs == mystacks) continue;
      lives_microsleep_until_zero(pthread_mutex_lock(&xhs[LIVES_GUI_HOOK]->mutex));
      if (xhs[LIVES_GUI_HOOK]->flags & HS_FLAG_TRIGGERING) {
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
      if (xhs[LIVES_GUI_HOOK]->flags &  HS_FLAG_TRIGGERING) {
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


boolean lives_hook_trigger(lives_hook_stack_t **hstacks, int type) {
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
  uint64_t hsflags;
  hookstack_pattern_t hs_pattern = lives_hookstack_pattern(type);
  boolean rerun = FALSE;


  //if (type == SYNC_ANNOUNCE_HOOK) dump_hook_stack(hstacks, type);

  hstack = hstacks[type];

  hs_op_flags = get_hs_op_flags(hstack);

  if (hs_op_flags & HOOKSTACK_ASYNC_PARALLEL) {
    lives_hooks_trigger_async(hstacks, type);
    return TRUE;
  }

  if (hstack->flags & HS_FLAG__INVALID) return FALSE;

  hmutex = &(hstack->mutex);

  if (type != FATAL_HOOK)
    PTMLH;
  hmulocked = TRUE;

  if (!hstack->stack || (hstack->flags & HS_FLAG_TRIGGERING)) {
    if (type != FATAL_HOOK) PTMUH;
    hmulocked = FALSE;
    goto trigdone;
  }

  hstack->flags |= HS_FLAG_TRIGGERING;

  if (hstack->req_target_stacks) {
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
      finst = (lives_funcinst_t *)list->data;

      if (!finst) continue;

      flags = CL_DATA(finst, hs_cb_fkags);

      if (flags & (HOOK_STATUS_BLOCKED | HOOK_CB_IGNORE | HOOK_STATUS_RUNNING)) continue;

      if (flags & HOOK_STATUS_REMOVE) {
        remove_from_hstack(hstack, list);
        continue;
      }

      if (!(flags & HOOK_STATUS_ACTIONED)) continue;

      if (CL_DATA(finst, replacement)) continue;

      if (req_stack) {
        if ((myhints & flags) == myhints) {
          int dflags = DTYPE_HAVE_LOCK | DTYPE_CLOSURE;
          flagss &= ~HOOK_STATUS_ACTIONED;
          flags |= hstack->req_target_set_flags;
          if (flags & HOOK_CB_PRIORITY) dflags |= DTYPE_PREPEND;

	  CL_DATA(finst, hook_cb_flags) = flags;
          //lives_proc_thread_show_func_call(closure->proc_thread);

          if (lives_hook_add(hstack->req_target_stacks, hstack->req_target_type,
                             finst->hook_cb_flags, finst, dflags) != finst)
            remove_from_hstack(hstack, list);
          else hstack->stack =
		 (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack, list, FALSE);
        }
        lives_proc_thread_unref(lpt);
        continue;
      }

      if (type == LIVES_GUI_HOOK) {
        uint64_t xflags = finst->hook_cb_flags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
        if (xflags) {
          if (!have_recheck_mutex) {
            if (pthread_mutex_trylock(&recheck_mutex)) {
              // this means some other thread also has invalidate_data, we must let it run then recheck
              // UNREF
              lives_proc_thread_unref(finst->runner);
              lives_sleep_until_zero(pthread_mutex_trylock(&recheck_mutex));
              have_recheck_mutex = TRUE;
              rerun = TRUE;
              break;
            }
          }

          wait_parent = update_linked_stacks(finst, xflags);
          pthread_mutex_unlock(&recheck_mutex);

          if (wait_parent) {
            lives_proc_thread_unref(finst->runner);
            lives_proc_thread_wait_done(wait_parent, 0., FALSE);
            rerun = TRUE;
            break;
          }
        }
      }

      finst->hook_cb_flags |= HOOK_STATUS_RUNNING;
      finst->hook+cb+flags &= ~HOOK_STATUS_ACTIONED;

      lives_proc_thread_exclude_states(finst->runne, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
                                       | THRD_STATE_FINISHED);
      fdef = finst->funcdef;
      if ((finst->hook_cb_flags & HOOK_OPT_REMOVE_ON_FALSE)
          && fdef->return_type == WEED_SEED_BOOLEAN) {
        // test should be boolean_combined
        bret = TRUE; // set in case func is cancelled
        if (!finst->retloc) finst->retloc = &bret;
        //g_print("sync run: %s\n", lives_proc_thread_show_func_call(lpt));
      }

      // test here is hook_dtl_request - we forward some action request to another thread
      //
      if (!(flags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
        GET_PROC_THREAD_SELF(self);
        if (type != FATAL_HOOK) PTMUH;
        hmulocked = FALSE;

	if (flags & HOOK_OPT_REMOVE_ON_FALSE && finst->funcdef->return_type == WEED_SEED_BOOLEAN) {
	  rem_on_false = TRUE;
	  if (!finst->retloc) finsr->retloc = &bret;
	}   
	
	finst->runner = self;
	lives_push_active_funcinst(finst);
        lives_proc_thread_execute();

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
        lives_proc_thread_queue(finst, LIVES_THRDATTR_FG_THREAD | LIVES_THRDATTR_FG_LIGHT);
      }

      if (type != FATAL_HOOK) PTMLH;
      hmulocked = TRUE;

      if (flags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT)
          || (hs_op_flags & HOOKSTACK_ALWAYS_ONESHOT)) {
        // remove our added ref UNLESS this is a blocking call, then blocked thread will do that
        // if it is blocking, then caller will be waiting and will unref it now
        // so we leave our added ref to compensate for one which will be removed as closure is freed
        // UNREF
        if (!(flags & HOOK_CB_BLOCK)) remove_from_hstack(hstack, list);
        rerun = TRUE;
        break;
      }

      flags &= ~HOOK_STATUS_RUNNING;
      
      CL_DATA(finst, hook_cb_flahs) = flags;

      if (((flags & HOOK_OPT_REMOVE_ON_FALSE) && *(boolean *)finst->retloc == FALSE)) {
	remove_from_hstack(hstack, list);
	rerun = TRUE;
	break;
      }

      // will be be HOOK_DTL_SINGLE
      if (hs_op_flags & HOOKSTACK_RUN_SINGLE) {
        //g_print("done single\n");
        retval = TRUE;
        break;
      }
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

  hstacks[type]->flags &= ~HS_FLAG_TRIGGERING;

  if (have_recheck_mutex) {
    pthread_mutex_unlock(&recheck_mutex);
  }

  //if (type == SYNC_WAIT_HOOK) g_print("sync all res: %d\n", retval);
  return retval;
}


int lives_hook_trigger_async(lives_hook_stack_t **hstacks, int type) {
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

  hs_op_flags = get_hs_op_flags(hstack);
 
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

    hstack->flags |= HS_FLAG_TRIGGERING;

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
  lives_proc_thread_t poller = lives_proc_thread_create(LIVES_THRDATTR_START_CANCELLABLE,
							(lives_funcptr_t)_lives_hooks_tr_seq, -1, "viFv",
							hstacks, type, (weed_funcptr_t)finfunc, findata);;
  return poller;
}


lives_result_t lives_hook_cb_invalidate(lives_funcinst_t *finst) {
  if (finst && finst->dispostition == DISPOSITION_STACKED) {
    GET_PROC_THREAD_SELF(self);
    if (self != CL_DATA(finst, adder)) return LIVES_RESULT_NOPERM;
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_REMOVE;
    return LIVES_RESULT_SUCCESS;
  }
  LIVES_RESULT_INVALID;
}


 lives_result_t lives_hook_cb_block(lives_funcinst_t *finst) {
  if (finst && finst->dispostition == DISPOSITION_STACKED) {
    GET_PROC_THREAD_SELF(self);
    if (self != CL_DATA(finst, adder)) return LIVES_RESULT_NOPERM;
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_IGNORE;
    return LIVES_RESULT_SUCCESS;
  }
  LIVES_RESULT_INVALID;
}


 lives_result_t lives_hook_cb_unblock(lives_funcinst_t *finst) {
  if (finst && finst->dispostition == DISPOSITION_STACKED) {
    GET_PROC_THREAD_SELF(self);
    if (self != CL_DATA(finst, adder)) return LIVES_RESULT_NOPERM;
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_IGNORE;
    return LIVES_RESULT_SUCCESS;
  }
  LIVES_RESULT_INVALID;
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

  if (!(hstack->flags & HS_FLAG_TRIGGERING)) return;

  hmutex = &(hstack->mutex);
  PTMLH;

  hs_op_flags = get_hs_op_flags(hstack);

  if (!(hs_op_flags & HOOKSTACK_ASYNC_PARALLEL)) {
    PTMUH;
    return;
  }

  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist_next) {
    cblist_next = cblist->next;
    finst = (lives_funcinst_t *)cblist->data;
    if (!finst) continue;

    cbflags = CL_DATA(finst, cb_flags);
    if (cbflags & HOOK_STATUS_BLOCKED) continue;

    PTMUH;

    lives_hook_cb__wait_fulfilled(finst);

    PTMLH;

    if (CL_DATAflags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT)
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
  hstacks[htype]->flags &= ~HS_FLAG_TRIGGERING;
  PTMUH;
}


 boolean fn_data_match(lives_funcinst_t *finst1, lives_funcinst_t *finst2, int maxp) {
   if (fn_func_match(finst1, finst2) < maxp) return FALSE;
   for (int i = 0; i < maxp; i++) {
    char *pname = make_std_pname(i);
    if (!weed_leaf_elements_equate(finst1->params, pname, finst2->params, pname, -1)) {
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
int fn_func_match(lives_funcinst_t *finst1, lives_funcinst_t *finst2) {
  if (!finst1 || !finst2 || !finst1->funcdef || !finst2->funcdef ||
      finst1->funcdef->function != finst2->funcdef->function) return -1;
  if (finst1->funcdef->return_type != finst2->funcdef->return_type) return -2;
  if (finst1->funcdef->funcsig != finst2->funcdef->funcsig) return -3;
  return get_funcsig_nparms(finst1->funcdef->funcsig);
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


LIVES_GLOBAL_INLINE lives_funcinst_t *lives__funcinst_new(lives_funcdef_t *tmpl) {
  LIVES_CALLOC_TYPE(lives_funcinst_t, finst, 1);
  if (finst) {
    finst->uid = gen_unique_id();
    finst->funcdef = tmpl;
    

    flags = HS_FLAGS_FATAL;
    pat = HOOK_PATTERN_SPONTANEOUS;

  }
  return finst;
}


LIVES_GLOBAL_INLINE void lives_funcdef_free(lives_funcdef_t *fdef) {
  if (fdef && !(fdef->flags & FDEF_FLAG_STATIC)) {
    lives_freep((void **)&fdef->funcname);
    lives_freep((void **)&fdef->file);
    lives_free(fdef);
  }
}


LIVES_GLOBAL_INLINE void lives_funcinst_free(lives_funcinst_t *finst) {
  // ensure that sync_list is popped first
  if (finst) {
    lives_sync_list_t *sync_list = finst->next;
    if (sync_list) {
      lives_funclist_t *xfinst = lives_sync_list_pop(sync_list);
      if (xfinst) lives_funcinst_free(xfinst);
    }
    if (finst->flags & FINST_FLAG_STATIC) return;
    if (retloc && !(finst->flags & FINST_FLAG_NOFREE_RETLOC))
      lives_free(retloc);
    if (finst->module) finst_module_free(finst->mod_type, finsr->module);
    if (finst->params) weed_plant_free(finst->params);
    if (finst->funcdef) lives_funcdef_free(finst->funcdef);
    lives_free(finst);
  }
}


//////////// proxy data / param shadowing
// st would normally be WEED_SEED_PROXY
weed_error_t weed_leaf_bind_value(weed_plant_t *pl, const char *key, weed_seed_t st, void *locn, weed_size_t size) {
  // set a custom ptr to the address / size of a variable or blob data
  if (!pl) return WEED_ERROR_NOSUCH_PLANT;
  if (weed_plant_has_leaf(pl, key)) {
    if (weed_leaf_is_immutable(pl, key)) return WEED_ERROR_IMMUTABLE;
    err = weed_leaf_delete(pl, key);
    if (err != WEED_SUCCESS) return err;
  }
  err  = weed_set_custom_value(pl, key. st, locn);
  if (err != WEED_SUCCESS) return err;
  weed_ext_set_element_size(pl, key, size);
  return err;
}


lives_result_t lives_funcinst_bind_param(lives_funcinst_t *finst, int idx, void *locn) {  
  // what we do here is to make a second param, eg, for p0 we would make p0_proxy
  // in the second leaf we store a pointer to a variable, and the size
  // when the funcinst is actioned, we read the value dereferncing, e.g for WEED_SEED_INT
  // we would have
  // weed_set_int_value(finst->params, "p0", *(int *)weed_get_custom_value(finst->params, "p0_proxy", WEED_SEED_PROXY. &err);
  // 
  if (!finst || idx < -1) return LIVES_RESULT_INVALID;

  lives_funcdef_t *fdef = finst->funcdef;
  if (!fdef || !fdef->function) return LIVES_RESULT_INVALID;
  int nparms = get_funcsig_nparms(fdef->funcsig);
  if (idx >= nparms) return LIVES_RESULT_ERROR;
 
  char *pkey = make_proxy_pname(idx);
  if (weed_leaf_bind_value(finst->params, pkey, WEED_SEED_PROXY, locn, 0) != WEED_SUCCESS) {
    lives_free(pkey);
    return LIVES_RESULT_FAILED;
  }
 
  lives_free(pkey);
  return LIVES_RESULT_SUCCESS;
}


lives_result_t update_params_from_proxies(lives_funcinst_t *finst) {
  if (!finst || idx < -1) return LIVES_RESULT_INVALID;
  lives_funcdef_t *fdef = finst->funcdef;
  if (!fdef || !fdef->function) return LIVES_RESULT_INVALID;
  int nparms = get_funcsig_nparms(fdef->funcsig);
  for (int i = 0; i < nparms; i++) {
    char *prkey = make_proxy_pname(idx);
    if (weed_plant_has_leaf(finst->params, pkey)) {
      weed_seed_t st = nth_seed_type(fdef->funcsig, i);
      char *pkey = make_std_pname(idx);
      weed_leaf_from_vap(finst->params, pkey, st,
			 weed_get_custom_value(finst->params, prkey, WEED_SEED_PROXY, &err));
      lives_free(pkey);
    }
    if (err != WEED_SUCCESS) return LIVES_RESULT_FAILED;
  }
  lives_free(prkey);
  return LIVES_RESULT_SUCCESS;
}


static void *get_proxy_value(lives_proxy_data_t pdata, int idx, void **retlocp, void *valptr, weed_size_t size.
			     weed_error_t **errp) {
  // copy proxied data to a memory location
  // passing in a pointer to a pointer to a variable
  // if pointer to variable is set, value is copied into variable
  // if the addess of a NULL pointer is passed, the variable is alloced and then filled
  // if NULL is passed, then a value is allocated and return
  // in other cases, a pointer to var is still returned
  // on error, NULL is returned and err set in errp, if non NULL
  void *retloc = NULL;
  boolean nofree = FALSE;
  weed_error_t err = WEED_SUCCESS;

  if (!valptr || !size) goto failed;

  if (retlocp) {
    if (!*retlocp) *retlocp = lives_malloc(size);
    else noftree = TRUE;
    retloc = *retlocp;
  }
  else retloc = lives_malloc(size);

  lives_memcpy(retloc, valptr, size);

  goto success;

 failed:
  if (!retloc && *retlocp) lives_freep(&retloc);

 success:
  if (copy && retlocp && !*retlocp) *retlocp = retloc;
  if (errp) *errp = err;

  return retloc;
}


void *lives_proxy_data_get_value(weed_plant_t *pl, const char *key, int idx, void **retlocp,
				 boolean copy, weed_error_t *errp) {
  // get proxy value and copy it
  // see description above

  weed_error_t err = WEED_SUCCESS;
  void *pdata;
  if (!pl) {
    err = WEED_ERROR_NOSUCH_PLANT;
    gptp failed;
  }
  pdata = weed_get_custom_value(pl, key, WEED_SEED_PROXY, 0, &err);
  if (err != WEED_SUCCESS) goto failed;

  if (!copy) return pdata;
  if (!pdata || !size) goto failed;
  return get_proxy_data(pdata, idx, retlocp, pdata, size, errp);

 failed:
  if (errp) *errp = err;
  return NULL;
}


LIVES_GLOBAL_INLINE weed_size_t lives_proxy_data_get_size(weed_plant_t *pl, const char *key, int idx, weed_error_t *errp) {
  weed_error_t err = WEED_SUCCESS;
  if (!pl) return WEED_ERROR_NOSUCH_PLANT;
  return weed_leaf_element_size(plamt, key);
}


weed_error_t copy_leaf_value(weed_plant_t *pl, const char *key, int idx, weed_seed_t xst, void **retlocp) {
  // memcpy leaf value to *variable
  // for now we ignore arrays

  // xst is the "real" seed type for the data
  // this only matters if the real seed type is WEED_SEED_BLOB_DATA
  // then the variable whose address is passed should be a void **.
  // (void *)(*var) will be allocated to the blob size, and the data copied into it
  
  if (!pl) return WEED_ERROR_NOSUCH_PLANT;
  if (!retlocp) return WEED_ERROR_NOSUCH_PLANT;

  weed_seed_t st = weed_leaf_seed_type(pl, key);
  if (st == WEED_SEED_INVALID) return WEED_ERROR_NOSUCH_LEAF;
  sz = weed_leaf_element_size(pl, key);
  if (st == WEED_SEED_PROXY) {
    // retloc is already a void **
    if (xst == WEED_SEED_BLOB_DATA)
      lives_proxy_data_get_value(pl, key, idx, *retlocp, &err);
    else lives_proxy_data_get_value(pl, key, idx, retlocp, &err);
    return err;
  }
  if (sz) {
    if (!*retocp) *retlocp = lives_malloc(sz);
    weed_leaf_get(pl, key, idx. retloc);
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
