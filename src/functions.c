// functions.c
// (c) G. Finch 2002 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

static const hook_stack_descriptor_t hs_desc[N_HOOK_POINTS];

static boolean hs_inited = FALSE;

VARNAME_FUNC

#if 0
///////////////////////////////////////////////

boolean cond_equalsi(int i0, int i1) {return i0 == i1;}

////////////////////////////////////////////////

static weed_plant_t *cond_helper = NULL;

typedef struct {
  const char *fname;
  lives_funcptr_t fn;
  char *args_fmt;
} types_map;

typedef struct {
  int sym_type;
  // for opval
  types_map *tmap;
  const char *ifmt; // eg "i%d" or "data|i|%s"
  const char *efmt; // eg "ii" ur ''
} cond_trans;

//fmt of a cond is
// sym_bol :: cond_start


#define FN_NAME(fn) fn, #fn

#define _COND_END 	return CURRENT_VAL;
#define _COND_EVAL  	NEW_VAL = eval_func(CONDITION, INDEX);


void init_cond_checks(void) {
  // PREDEFINED SYMBOLS, there are only two
  // call cond_response (*eeval_func)(CONDITION, &idx)
  // input CONDITION and idx, value returned can be COND_RESPONSE_TRUE, COND_RESPONSE_FALSE
  // which is the value of cond token idx, we continue from updated idx
  // or COND_RESPONSE_ERROR

  // n pairs
  // "" OR
  //  internal_type(s). ext format. . gumvtopm
  

  // 
  // during constuction, this is followed n
  //

  // ifmt, efmt == ""
  register_cond_sym("_COND_EVAl", 		"K", 		eval_func);
  register_cond_sym("_COND_RETURN",		"",  		"");

  // no ifmt, symbols, ""
  register_cond_sym("_COND_PARENS_OPEN", 	"", 		"_COND_EVAL", "" );
  register_cond_sym("_COND_PARENS_CLOSE", 	"", 		"_COND_RETURN", "" );

  register_cond_sym("(",		 	"", 		"_COND_PARENS_OPEN", "" );
  register_cond_sym(")",		 	"", 		"_COND_PARENS_CLOSE", "" );

  // symbol is a direct replacement (alias)   
  register_cond_sym("COND_START", 		"", 		"_COND_PARENS_OPEN", "");
  register_cond_sym("COND_END", 		"",  		"_COND_PARENS_CLOSE", "");

  register_cond_sym("COND_TRUE", 		"b", 		"%d", 	TRUE, 		"");
  register_cond_sym("COND_FALSE", 		"b", 		"%d", 	FALSE, 		"");

  register_cond_sym("COND_EQUALS", 		"F", 		"ii", 	FN_NAME(cond_equalsi), "bb", FN_NAME(cond_equalsb), "");     
  register_cond_sym("COND_AND", 		"F", 		"bb", 	FN_NAME(cond_logic_and), "");     

  register_cond_sym("COND_INT_CONST", 		"i", 		"%d",	value);
  register_cond_sym("COND_INT_VAR", 		"i", 		"%s"	"@symbolic_name");

  // sym_macro is replacedd recursively
  register_cond_sym("COND_ALWAYS", 		"",  		"COND_TRUE". 		"");
  register_cond_sym("COND_NEVER", 		"", 		"COND_FALSE", 		"");

  // oplogic is takes char ** cond, int idx and returns a cond_response
  // COND_TRUE, COND_FALSE or cond error
}

lives_cond_ret register_cond_sym(const char *key, int sym_type, ...) {
  LIVES_CALLOC_TYPE(cond_trans, ctran, 1);
  if (!cond_helper) cond_helper = lives_plant_new(123);
  if (weed_plant_has_leaf(cond_helper, key)) return LIVES_COND_DUPL_SYM;
  ctran->sym_type = sym_type;
  switch (sym_type) {
  case SYM_OPVAL: {
    LiVESList *types_list = NULL;
    va_list va;
    va_start(va, sym_type);
    while(1) {
      char *args_fmt = va_arg(va, char *);
      if (!args_fmt) break;
      tmap->args_fmt = lives_strdup(args_fmt);
      tmap->fn = va_arg(va, lives_funcptr_t);
      tmap->fname = lives_strdup(va_arg(va, char *));
      types_list = lives_list_prepend(types_list, tmap);
    }
    va_end(va);
    ctran->tmap = types_list;
    break;
  }
  case SYM_OPLOGIC:
  case SYM_BOL:
  case SYM_FMT:
    ctran->ifmt = lives_strdup(va_arg(va, char *));
    if (!ctran->ifmt) return LIVES_COND_BAD_IFMT;
    break;
  default: return LIVES_COND_BAD_SYM_TYPE;
  }

  return LIVES_COND_OK;
}

// Todo - if opvals. when get non sym fmt, backtrack to opval, check tmap for tok, pfmt
// append pfmt, fn_name, decr opvals
//
// for eval, get funcptr, convert string params to orig type and set 
char *lives_cond_repl(char *strg, lives_cond_ret *ret, va_list va) {
  static int opval = 0;
  char *tok = strg;
  static char *pfmt = NULL;
  cond_trans *ctran = (cond_trans *)weed_get_voidptr_value(cond_helper, strg, NULL);
  if (!ctran) {
    if (ret) *ret = LIVES_COND_BAD_TOKEN;
    return NULL;
  }
  switch (ctran->sym_type) {
  case SYM_OPLOGIC:
  case SYM_BOL:
    if (opvals) {
      rewind_append(conds, 
    break;
  case SYM_FMT: {
    va_list vc;
    va_copy(vc, va);
    for (int i = 0; ctran->efmt[i]; i++) {
      if (!va_check(ctran->efmt[i], va)) {
	va_end(vc);
	if (err) *err = LIVES_COND_EFMT_NOARG;
	return NULL;
      }
    }
    va_end(vc);
    if (opvals) {
      char *tmp = lives_strdup_print("%s%s", pfmt, ctran->efmt);
      lives_free(pfmt);
      pfmt = tmp;
    }
    if (err) *err = LIVES_COND_OK;
    return lives_strdup_vprintf(ctran->ifmt, va);
  }
    
  case SYM_OPVAL: {
    // opval would be something like "COND_EQUALS"
    // and this would give a ctran with type_maps
    // function, args_fmt pairs. now we need to parse any number of sym_fmt
    // for now we leace the original token (e.g "COND_EQUALS", and push idx
	
    // we need to collate the efmt values for these, then when we reach a non sym_fmt token.
    // backtrack to the previous opval
    // and set something like, "opval|COND_EQUALS|ii|cond_equalsi"
    // when evaluating, we will look up the translation again to get the actual funcptr

	cond = lives_strdup_printf("opval|%s", tok);
      opvals++;
      return cond;
  }
  }
  default:
    if (err) *err = LIVES_COND_BAD_SYM_TYPE;
    return NULL;
  }
  return lives_strdup(ctran->ifmt);
}


char **lives_cond_make(char *strg, ...) {
  LIVES_CALLOC_TYPE(char *, cond, 1);
  lives_cond_ret ret;
  int nv = 0;
  cond[nv++] = lives_cond_repl(strg, &ret, va);
  lives_free(strg);
  if (ret == LIVES_COND_END) return cond;
  if (ret == LIVES_COND_OK) {
    va_list va;
    va_start(va, strg);
    while (1) {
      cond = lives_realloc(cond, (nv + 1) * sizeof(char *));
      strg = lives_strdup(va_arg(va, char *));
      cond[nv++] = lives_cond_repl(strg, &ret, va);
      lives_free(strg);
      if (ret == LIVES_COND_END) return cond;
      if (ret != LIVES_COND_OK) break;
    }
  }
  while (--nv) lives_free(cond[nv]);
  lives_free(cond);
  return NULL;
}
#endif

static void init_hook_stacks(void) {
  if (hs_inited) return;
  hs_inited = TRUE;
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    uint64_t flags = 0;
    hook_stack_pattern_t pat = HOOK_PATTERN_DATA;
    hook_stack_descriptor_t *xhs = (hook_stack_descriptor_t *)&hs_desc[i];
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
      //trigger = LIVES_LEAF_THRD_STATE;
      //test_pre = "^%%lu & %lu:
      // etc
      break;
    case FINISHED_HOOK:
      flags = HS_FLAGS_FINISHED;
      //trigger = LIVES_LEAF_THRD_STATE;
      break;
    case ERROR_HOOK:
      flags = HS_FLAGS_ERROR;
      //trigger = LIVES_LEAF_THRD_STATE;
      break;
    case CANCELLED_HOOK:
      flags = HS_FLAGS_CANCELLED;
      // trigger = LIVES_LEAF_THRD_STATE;
      break;
    case DESTRUCTION_HOOK:
      flags = HS_FLAGS_DESTRUCTION;
      //trigger = LIVES_LEAF_THRD_STATE;
      break;
    default: break;
    }

    xhs->pattern = pat;
    xhs->op_flags = flags;

    if (i > N_NATIVE_HOOKS) {
      xhs->ret_type = WEED_SEED_BOOLEAN;
    }
  }
}


const hook_stack_descriptor_t *get_hs_desc(int hstype) {
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

#define _SET_CUSTOM_LEAF_FROM_VARG(plant, pkey, type, ne, args)		\
  (ne == 1 ? weed_set_custom_value((plant), (pkey), (type), va_arg((args), void *)) \
   : weed_set_custom_array((plant), (pkey), (type), (ne), va_arg((args), void **)))

#define SET_LEAF_FROM_VARG(plant, pkey, type, ne, args) _SET_LEAF_FROM_VARG((plant), (pkey), type, \
									    CTYPE(type), CPTRTYPE(type), (ne), (args))

#define SET_CUSTOM_LEAF_FROM_VARG(plant, pkey, type, ne, args) _SET_CUSTOM_LEAF_FROM_VARG((plant), (pkey), type, (ne), (args))
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
  case WEED_SEED_CONST_CHARPTR: return SET_CUSTOM_LEAF_FROM_VARG(plant, key, type, ne, xargs);
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


LIVES_GLOBAL_INLINE lives_proc_thread_t _lpt_from_funcdef(lives_funcdef_t *fdef, char **anames, lives_thread_attr_t attrs,
							  va_list ap) {
  lives_proc_thread_t lpt;
  lives_funcinst_t *finst = lives_funcinst_new(fdef);
  if (fdef->funcsig) {
    funcinst_params_from_vargs(finst, ap);
    finst->paramnames = anames;
  }
  lpt = lives_proc_thread_create_for_funcinst(finst, attrs);
  return lpt;
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


static boolean lives_funcinst_error_state(lives_funcinst_t *finst) {
  if (finst->disposition == DISPOSITION_STACKED) {
    LiVESList *recs = (LiVESList *)CL_DATA(finst, receipts);
    if (recs && recs->data) {
      int reply = lives_cb_receipt_get_req_reply(recs->data);
      return reply == LIVES_REPLY_ERROR;
    }
  }
  lives_proc_thread_t lpt = LPT_DATA(finst, runner);
  return lives_proc_thread_had_error(lpt);
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
  // in the first example, we would create p for example:
  //   p = lives_proc_thread_create(LIVES_THRDATTR_NONE, WEED_SEED_INT, int_func, "i", ival);
  //   retval = lives_proc_thread_join_int(p); lives_proc_thread_unref(p);
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
  weed_error_t err;

  if (ret_type == WEED_SEED_CONST_CHARPTR)
    ret_type = WEED_SEED_VOIDPTR;

  thefunc.func = func;

  // if we have "bound" params, update them from live variables now
  update_params_from_proxies(finst);

  if (MODULE_TYPE_IS(finst, HOOK_STACK)) is_stacked = TRUE;
  
  //ljlist = (LiVESList *) weed_get_voidptr_value(lpt, LIVES_LEAF_LONGJMP_LIST, NULL);						
  if (!is_stacked) sjval = sigsetjmp(env, 1);

  if (!sjval) {
    if (!is_stacked) LPT_DATA(finst, lj_stack) =
		       lives_sync_list_push(LPT_DATA(finst, lj_stack), (void *)&env);
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

  if (lives_funcinst_error_state(finst) || err != WEED_SUCCESS) res = LIVES_RESULT_ERROR;
  else res = LIVES_RESULT_CANCELLED;

  if (!is_stacked) lives_sync_list_pop(&LPT_DATA(finst, lj_stack));

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
    copy_leaf_value(finst->params, _RV_, 0, fdef->return_type, &finst->retloc);
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
  char *msg;

  if (lpt == mainw->debug_ptr) g_print("nrefss mmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  if (!lpt || lives_proc_thread_ref(lpt) < 2) {
    LIVES_CRITICAL("call_funcsig was supplied a NULL / invalid proc_thread");
    return FALSE;
  }

  finst = lives_proc_thread_get_active_funcinst(lpt);
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
  }

 funcerr2:

  if (!msg) msg = lives_strdup_printf("Got error %d running function with type "
				      "0x%016lX (%lu)", err, finst->funcdef->funcsig, finst->funcdef->funcsig);

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
	  //tuint64_t hs_op_flags = 0;
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


static int rcpt_mutex_lock(void *receipt) {
  if (!receipt) return 0;
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return pthread_mutex_lock(&rcpt->mutex);
}


static int rcpt_mutex_unlock(void *receipt) {
  if (!receipt) return 0;
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return pthread_mutex_unlock(&rcpt->mutex);
}


void flush_cb_added_list(lives_proc_thread_t lpt, boolean all) {
  // if all is FALSE, flusj only newly added and expired
  // else flush all
  if (lpt) {
    LiVESList *cb_add_list = weed_get_voidptr_value(lpt, LIVES_LEAF_CB_ADDED_LIST, NULL), *list;
    for (list = cb_add_list; list; list = list->next)
      if (list->data)
	if (all || lives_cb_receipt_check_expired(list->data)
	    || !lives_cb_receipt_get_nrefs(list->data)) {
	  cb_add_list = lives_list_remove_node(cb_add_list, list, FALSE);
	  weed_set_voidptr_value(lpt, LIVES_LEAF_CB_ADDED_LIST, cb_add_list);
	  rcpt_mutex_lock(list->data);
	  lives_cb_receipt_set_in_list(list->data, FALSE);
	  if (lives_cb_receipt_check_expired(list->data)) {
	    rcpt_mutex_unlock(list->data);
	    lives_cb_receipt_free(list->data);
	  }
	  else rcpt_mutex_unlock(list->data);
	}
  }
}


void ref_cb_added_list(void) {
  GET_PROC_THREAD_SELF(self);
  // do garbage collection
  if (self) {
    LiVESList *cb_add_list = weed_get_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, NULL);
    for (LiVESList *list = cb_add_list; list; list = list->next) 
      if (list->data) lives_cb_receipt_ref(list->data);
  }
}


void unref_cb_added_list(void) {
  // do garbage collection
  GET_PROC_THREAD_SELF(self);
  if (self) {
    LiVESList *cb_add_list = weed_get_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, NULL);
    for (LiVESList *list = cb_add_list; list; list = list->next) 
      if (list->data) lives_cb_receipt_unref(list->data);
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


LIVES_GLOBAL_INLINE void lives_hook_stack_clear(lives_hook_stack_t **hstacks, int type) {
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
          lives_hook_async_join(NULL, type);
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
	lives_funcinst_free(finst);
      }
      lives_list_free(hsstack);
      hstack->stack = NULL;
      //PTMUH;
    }
  }
}


LIVES_GLOBAL_INLINE void lives_hook_stacks_clear_all(lives_hook_stack_t **hstacks, int ntypes) {
  if (hstacks)   for (int i = 0; i < ntypes; i++) {
      if (hstacks[i]) {
	lives_hook_stack_clear(hstacks, i);
	pthread_mutex_destroy(&hstacks[i]->mutex);
	lives_free(hstacks[i]);
      }
    }
}


static void call_free_func(lives_funcinst_t *finst, int i, boolean do_exec) {
  // todo
  char *pkey = lives_strdup_printf("free_lpt%d", i);
  lives_funcinst_t *finst = lives_proc_thread_get_funcinst(lpt);
  lives_funcinst_t *free_finst = (lives_funcinst_t *)weed_get_voidptr_value(finst->params, pkey, NULL);
  if (free_finst) {
    weed_leaf_delete(finst->params, pkey);
    if (do_exec) lives_funcinst_execute(free_finst);
    lives_funcinst_free(free_finst);
  }
  lives_free(pkey);
}


static void remove_from_hstack(lives_hook_stack_t *hstack, LiVESList *list) {
  // should be called with hstack mutex LOCKED !
  // remove list from htack, free the closure
  lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
  lives_funcinst_free(finst);
  hstack->stack = (volatile LiVESList *)lives_list_remove_node
    ((LiVESList *)hstack->stack, list, FALSE);
}


boolean free_finst_paramdata(void *receipt, lives_funcinst_t *finst) {
  lives_proc_thread_t adder = lives_cb_receipt_get_adder(receipt);
  GET_PROC_THREAD_SELF(self);

  // adder can call this directly to check receipt reply later
  if (adder != self) return FALSE;
  
  int reply = lives_cb_receipt_get_req_reply(receipt);
  if (reply != LIVES_REPLY_YES && reply != LIVES_REPLY_NO) {
    int nparams = get_funcsig_nparms(finst->funcdef->funcsig);
    for (int i = 0; i < nparams; i++)
      call_free_func(lpt, i, reply != LIVES_REPLY_FULFILLED);
    lives_hook_cb_remove(receipt);
  }
  return FALSE;
}


static boolean rem_from_list(void *receipt, void *data) {
  int reply = lives_cb_receipt_get_req_reply(receipt);
  if (reply != LIVES_REPLY_YES && reply != LIVES_REPLY_NO)
    lives_cb_receipt_remove_from_list(receipt);
  return FALSE;
}


static boolean unblock_waiter(void *receipt, lives_funcinst_t *finst) {
  // if !paused, stop it from doing do
  int reply = lives_cb_receipt_get_req_reply(receipt);
  lives_proc_thread_t adder = lives_cb_receipt_get_adder(receipt);
  if (!adder) return FALSE;
  if (reply == LIVES_REPLY_YES || reply == LIVES_REPLY_NO) return FALSE;

  // setting state directly will work in every case - if adder is not paused it will
  // prevent it from pausing and it will clear the request
  
  lives_proc_thread_force_resume(adder);

  if (CL_DATA(finst, cb_flags) & HOOK_CB_HAS_FREEFUNCS)
    free_finst_paramdata(receipt, finst); 
  else lives_hook_cb_remove(receipt);
  return FALSE;
}


// hook cb's and receipts

lives_proc_thread_t lives_cb_receipt_get_adder(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->adder : NULL;
}


int lives_cb_receipt_get_req_reply(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->req_reply : LIVES_REPLY_NONE;
}

int lives_cb_receipt_get_nrefs(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->nrefs : -1;
}


void lives_cb_receipt_ref(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->nrefs++;
}


void lives_cb_receipt_unref(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->nrefs--;
}


boolean lives_cb_receipt_check_expired(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->expired : FALSE;
}


boolean lives_cb_receipt_is_in_list(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->in_list : FALSE;
}


void lives_cb_receipt_set_adder(void *receipt, lives_proc_thread_t adder) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->adder = adder;
}


void lives_cb_receipt_set_reply(void *receipt, int reply) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->req_reply = reply;
}


void *lives_cb_receipt_set_in_list(void *receipt, boolean in_list) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->in_list = in_list;
}


void *lives_cb_receipt_set_reply_callback(void *receipt, reply_sent_cb_f *reply_sent_cb, void *user_data) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    rcpt->reply_cb = reply_sent_cb;
    rcpt->reply_cb_data = user_data;
  }
}


void *lives_cb_receipt_has_reply_callback(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? !!rcpt->reply_callback; : FALSE;  
}


boolean lives_cb_receipt_call_reply_callback(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    ret = (*rcpt->reply_cb)(receipt, rcpt->reply_cb_data);
  }
}


LIVES_LOCAL_INLINE void *lives_cb_receipt_new(void) {
  LIVES_CALLOC_TYPE(hook_cb_receipt, receipt, 1);
  pthread_mutex_init(&receipt->mutex);
  return receipt;
}


LIVES_LOCAL_INLINE lives_result_t lives_cb_receipt_free(void) {
  // in order to free a cb receipt, expired must be true,
  // in_list must be false. and adder must have been set
  if (!rcpt) return LIVES_RESULT_INVALID;
  rcpt_mutex_lock(rcpt);
  if (!lives_cb_receipt_check_expired(rcpt)
      || lives_cb_receipt_is_in_list(rcpt)
      || !lives_cb_receipt_get_adder(rcpt)) {
    rcpt_mutex_unlock(rcpt);
    return LIVES_RESULT_FAIL;
  }
  rcpt_mutex_unlock(rcpt);
  pthread_mutex_destroy(&hstacks[i]->mutex);
  lives_free(rcpt);
  return LIVES_RESULT_SUCCESS;
}


static boolean check_if_can_remove(lives_funcinst_t *finst, LiVESList *list) {
  // check funcinst receipt, if it has an adder and is not in a cb_add_list
  // expire it, detach it, free it
  // if no mote receipts in funcinst, flag it for removal
  void *rcpt = list->data;
  rcpt_mutex_lock(rcpt);
  if (lives_cb_receipt_get_adder(rcpt)
      && !lives_cb_receipt_is_in_list(rcpt)
      && !(CL_DATA(finst, cb_flags) & HOOK_CB_PERSISTENT)) {
    lives_cb_receipt_set_expired(rcpt);
    rcpt_mutex_unlock(rcpt);

    CL_DATA(finst, receipts) = lives_list_remove_node(CL_DATA(finst, receipts), list, FALSE);
    lives_cb_receipt_free(rcpt);

    if (!CL_DATA(finst, receipts)) Cl_DATA(finst, cb_flags) |= HOOK_STATUS_REMOVE;
    return TRUE;
  } 
  rcpt_mutex_unlock(rcpt);
  return FALSE;
}


boolean cleanup_funcinst_receipts(lives_funcinst_t *finst) {
  // expire any receipts which are bo longer in a cb_list, freeing them
  // then if there are no recopts for funcinst, and it is not persistent,
  // it will be flagged for removal and TRUE is returned
  for (LiVESList *rcpts = CL_DATA(finst, receipts); rcpts; rcpts = rcpts->next) {
    void *rcpt = rcpts->data;
    if (!rcpt) continue;
    check_if_can_remove(finst, list);
  }
  return !CL_DATA(finst, receipts);
}


static void lives_funcinst_move_receipts(lives_funcinst_t *dst, lives_funcinst_t *src) {
  // fikter out any receipts with no reply_snt_cb (since the adder is not tracking)
  // and expire them (else they will never be removed)
  // these will be freed when the funcinst is removed, and adder removes from cb_added_list
  // the rest will be appended to dst receipts
  if (!src || !dst || src == dst) return;
  cleanup_receipts(src);
  if (CL_DATA(src, receipts)) {
    lives_list_concat(CL_DATA(dst, receipts), rcpts);
    CL_DATA(src, receipts) = NULL;
  }
}


void  lives_funcinst_send_replies(lives_funcinst_t *finst, int reply) {
  LiVESList *retry = NULL, *listnxt;  
  do {
    for (LiVESList *list = finst->receipts; list; list = listnxt) {
      boolean tryagain = FALSE;
      void *rcpt = list->data;
      int oreply = lives_lives_cb_receipt_get_req_reply(rcpt);
      listnxt = list->next;

      if (reply == oreply) continue;	
      if (retry && !lives_list_find_by_data(retry, rcpt)) continue;

      if (!lives_cb_get_adder(rcpt) && oreply == LIVES_REPLY_YES) {
	// wait for adder to confirm before changing reply
	lives_list_append_unique(retry, recpt);
	continue;
      }

      if (!retry && reply == LIVES_REPLY_INVALID) lives_cb_receipt_set_expired(rcpt);

      if (!check_if_can_remove(finst, list)) {
	if (!retry) lives_cb_receipt_set_reply(rcpt, reply);
	if (lives_cb_receipt_has_reply_callback(rcpt))
	  tryagain = lives_cb_receipt_call_reply_callback(rcpt);
      }
      if (tryagain) retry = lives_list_append_unique(retry, recpt);
      else if (retry) retry = lives_list_remove_data(retry, recpt, FALSE);
    }
  } while (retry);
}


static lives_funcinst_t *expel_from_stack(lives_funcinst_t *expelled, lives_funcinst_t *ret_finst) {
  if (!ret_finst) return expelled;

  // blocking callbacks that already received YES should only continue on fulfilled
  lives_funcinst_send_replies(expelled, LIVES_REPLY_NO);
  cleanup_receipts(expelled);
  if (!(CL_DATA(expelled, cb_flags) & HOOK_STATUS_REMOVE))
    lives_funcinst_move_receipts(expeller, expelled);
  CL_DATA(expelled, cb_flags) |= HOOK_STATUS_REMOVE;
  // expelled also expels expeller 
  if (xflags & HOOK_TOGGLE_FUNC) ret_finst = expelled;  
  return ret_finst;
}


static void *lives_cb_receipt_set_expired(void *receipt) {
  if (!receipt) return NULL;
  hook_cb_receipt *rcpt = hook_cb_receipt *(receipt);
  rcpt_mutex_lock(receipt);
  rcpt->expired = TRUE;
  if (rcpt->adder && !rcpt->in_list) can_free = TRUE;
  rcpt_mutex_unlock(receipt);
  if (can_free) {
    lives_cb_receipt_free(rcpt);
    receipt = NULL;
  }
  return receipt;
}


lives_result_t lives_cb_receipt_add_to_list(void *hook_cb_receipt) {
  GET_PROC_THREAD_SELF(self);
  LiVESList *cb_add_list = weed_get_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, NULL);
  cb_add_list = lives_list_prepend(cb_add_list, receipt);
  weed_set_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, cb_add_list);
  lives_cb_receipt_set_in_list(receipt, TRUE);
}


lives_result_t lives_cb_receipt_remove_from_list(void *hook_cb_receipt) {
  // removes receipt from self cb_added_list
  // - if rexeipt is expired we also free receipt
  // - func_inst is NOT freed - that will happen when funcinst is retriggered
  //
  if (receipt) return LIVES_RESULT_INVALID;
  if (lives_cb_receipt_get_adder(receipt) != self) return LIVES_RESULT_NOPERM;
  if (!lives_cb_receipt_is_in_list(receipt)) return LIVES_RESULT_ERROR;
  if (!(list = lives_list_remove_data(cb_add_list, receipt, FALSE))) return LIVES_RESULT_ERROR;
  weed_set_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, cb_add_list);

  rcpt_mutex_lock(rcpt);
  lives_cb_receipt_set_in_list(receipt, FALSE);
  if (lives_cb_receipt_check_expired(receipt)) {
    rcpt_mutex_unlock(rcpt);
    lives_cb_receipt_free(receipt);
    return LIVES_RESULT_SUCCESS;
  }
  // set expired so we can force remove persistent callbacks
  lives_cb_receipt_set_expired(receipt);
  rcpt_mutex_unlock(rcpt);
  return LIVES_RESULT_SUCCESS;
}


void *lives_hook_cb_add(lives_hook_stack_t **hstacks, int type, lives_funcinst_t finst, uint64_t addmode) {
  if (finst) return NULL;

  lives_funcinst_t *xfinst = NULL, *ret_finst = NULL;
  pthread_mutex_t *hmutex;
  void *receipt = NULL;
  LiVESList *cblist, *cblistnext;
  uint64_t cbflags, hs_op_flags, xflags;

  boolean have_lock = FALSE;
  boolean is_self_stack = FALSE;
  boolean fmatch;
  boolean is_append = TRUE, is_remove = FALSE, is_test = FALSE;

  int maxp;
			
  GET_PROC_THREAD_SELF(self);

  if (!(addmode & _ADDMODE_NORCPT)) receipt = lives_cb_receipt_new();

  if (!finst || !hstacks || type < 0 || type >= N_HOOK_POINTS) {
    if (receipt) lives_cb_receipt_send_replies(receipt, LIVES_REPLY_INVALID);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    return receipt;
  }

  if (!hstacks) {
    if (receipt) lives_funcinst_send_replies(xfinst, LIVES_REPLY_INVALID);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    return receipt;
  }

  hstack = hstacks[type];

  if (!hstack) {
    if (receipt) lives_funcinst_send_replies(xfinst, LIVES_REPLY_INVALID);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    return receipt;
  }

  // check if ret_type matches
  if (hstack->hsdesc->ret_rtype
      && finst->funcdef->return_type != hstack->hsdesc->ret_rtype) {
    lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    lives_cb_receipt_set_expired(receipt);
    return receipt;
  }

  if (addmode == ADDMODE_UPD_LINKED) is_remove = TRUE;

  if (addmode == ADDMODE_TEST) is_test = TRUE;

  finst->flags &= ~FINST_FLAG_REJECTED;

  cbflags = CL_DATA(finst, cb_flags);
  xflags = cbflags & (HOOK_UNIQUE_REPLACE | HOOK_INVALIDATE_DATA | HOOK_TOGGLE_FUNC);

  if ((cbflags & HOOK_CB_PRIORITY) || (addmode & _ADDMODE_FORCE_PREPEND))
    is_append = FALSE;
  if (addmode & ADDMODE_HAVE_LOCK) have_lock = TRUE;

  if (receipt) lives_funcinst_add_receipt(finst, receipt);

  lives_funcinst_set_disposition(finst, DISPOSITION_STACKED);

  hs_op_flags = get_hs_op_flags(hstack);

  if (hs_op_flags & HOOKSTACK_PERSISTENT) cbflags |= HOOK_CB_PERSISTENT;
  CL_DATA(finst, cb_flags) = cbflags;

  if (!(hs_op_flags & HS_NATIVE) && hstack->owner.lpt == self)
    is_self_stack = TRUE;

  // if append, then everything else will check
  if (is_append) xflags &= ~(HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);

  if (cbflags & HOOK_OPT_REMOVE_ON_FALSE)

  if ((hstack->hsdesc->accept_cond && !lives_cond_eval(hstack->hsdesc->accept_cond))
      || (hstack->reject_cond && lives_cond_eval(hstack->reject_cond))) {
    lives_send_replies(finst, LIVES_REPLY_NO);
    lives_cb_receipt_set_expired(receipt);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    if (!have_lock) PTMUH;
    return receipt;  
  }

  // first we will do a bit of function bending
  // - if we have args_fmt_start,
  //  args_fmt_start will be prepended when calling the function
  // shift existing params + free funcs using weed_ext_rename_leaf
  // create new fixed params with type WEED_SEED_PROXY with 0 elements
  // bind the params to the values to be passed, e.g &(hstack->owner.lpt)
  // done
  
  // if prepending, nothing can block us
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

  maxp = CL_DATA(finst, nmatch_params) = THREADVAR(hook_match_nparams);

  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblistnext) {
    uint64_t cfinv = 0;
    cblistnext = cblist->next;
    xfinst = (lives_funcinst_t *)cblist->data;
    if (!xfinst) continue;
    uint64_t xcbflags;

    xcbflags = CL_DATA(xfinst, cb_flags);
    if (xcbflags & (HOOK_STATUS_BLOCKED | HOOK_STATUS_ACTIONED | HOOK_STATUS_RUNNING)) continue;

    // check uniqueness restrictions when adding a new callback

    if (is_append) {
      cfinv = xcbflags & HOOK_INVALIDATE_DATA;
      if (cfinv) cfinv |= (xcbflags & HOOK_OPT_MATCH_CHILD);
    }
    if (cfinv || (xflags & HOOK_INVALIDATE_DATA)) {
      if (!fn_data_match(xfinst, finst, maxp)) {
	if (!((cfinv | xflags) & HOOK_OPT_MATCH_CHILD)) continue;
	if ((!(xflags & HOOK_OPT_MATCH_CHILD) || fn_match_child(finst, xfinst))
	    && (!(cfinv & HOOK_OPT_MATCH_CHILD) || fn_match_child(xfinst, finst))) {
	  if (is_append) {
	    // denied !
	    ret_finst = xfinst;
	    break;
	  }
	  ret_finst = expel_from_stack(xfinst, ret_finst);
	  continue;
	}
      }
    }
    
    if (!(xflags & (HOOK_UNIQUE_FUNC | HOOK_UNIQUE_FUNC | HOOK_TOGGLE_FUNC))) continue;
    
    // check if the function matches (unless we are just invalidating data)
    if ((fmatch = (fn_func_match(finst, xfinst))) < 0) continue;
    // unique_func -> maintain only 1st fn match, expel matching funcs
    // unique_data -> replace 1st matching func / data, expel other matching funcs / data
    // unique func /  unique data, -> replace 1st func match, expel other func matches
    if (xflags & HOOK_UNIQUE_DATA) {
      if (!HOOK_UNIQUE_FUNC) {
	// for unique data, it makes no sense to match all params
	if (!maxp) maxp = fmatch;
	if (!fn_data_match(xfinst, finst, maxp)) continue;
      }
      if (!ret_finst && !(xflags & HOOK_TOGGLE_FUNC))
	lives_list_append(cblist, (void *)finst);
      ret_finst = expel_from_stack(xfinst, finst);
      continue;
    }
    // unique func
    ret_finst = expel_from_stack(xfinst, ret_finst);
  }

  if (is_remove) {
    // is_remove is set if removing conflictig callabacks from other thread stacks
    // in this case we don't move or append
    if (!have_lock) PTMUH;
    return NULL;
  }

  if (ret_finst && ret_finst != finst) {
    // callback was supplanted by another
    //
    // we need to free funcinst at some point - normally this would happen when the funcinst is
    // flagged for removal and then triggered. But for adder, this may be  indistinguishable from the
    // case where funcinst is rejected in favour of another. So here we flag the funcinst
    // FINST_FLAG_REJECTED, and adder can take appropriate actions
    finst->flags |= FINST_FLAG_REJECTED;
    if (!have_lock) PTMUH;
    if (xflags & HOOK_TOGGLE_FUNC) {
      lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
      lives_cb_receipt_set_expired(receipt);      
    }
    else lives_funcinst_send_replies(finst, LIVES_REPLY_YES);
    return receipt;
  }

  lives_send_replies(finst, LIVES_REPLY_YES);

  if (is_append){
    if (!ret_finst) hstack->stack = lives_list_append((LiVESList *)hstack->stack, finst);
  }
  else hstacks[type]->stack = lives_list_prepend((LiVESList *)hstack->stack, finst);

  CL_DATA(finst, hook_stack) = hstack;

  if (!have_lock) PTMUH;

  if (lpt_attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());
  //}
  return receipt;  
}


 void *lives_hook_cb_add_funcinst(lives_hook_stack_t **hooks, int type, uint64_t cbflags,
				  lives_funcinst_t *finst) {
  // funcinst may be blocked from adding for a variety of reasons
  // if superceded by another funcinst, we return the receipt, if not replaced, we return NULL
  // and in both cases, FINST_FLAG_REJECTED is set

  void *receipt;
  int reply;

  lives_funcinst_set_dispostition(finst, DISPOSITION_STACKED);
  CL_DATA(finst, cb_flags) = cbflags;

  receipt = lives_hook_cb_add(hooks, type, finst, ADDMODE_NORMAL);

  cbflags = CL_DATA(finst, cb_flags);
  reply = lives_cb_receipt_get_req_reply(receipt);
  if (reply == LIVES_REPLY_YES) {
    lives_cb_receipt_add_to_list(receipt);

    if (cbflags & HOOK_CB_BLOCK)
      lives_cb_receipt_set_reply_callback(receipt, unblock_waiter, (void *)finst);
    else {
      if (cbflags & HOOK_CB_HAS_FREEFUNCS)
	lives_cb_receipt_set_reply_callback(receipt, free_finst_paramdata, (void *)finst);
      else lives_cb_receipt_set_reply_callback(receipt, rem_from_list, NULL);
    }
  }
  lives_cb_receipt_set_adder(receipt, self);
  return receipt;
}


// returns receipt. or NULL if add condition failed, or it was toggled out
void *_lives_hook_cb_add_full(lives_hook_stack_t **hooks, int type, uint64_t cbflags, lives_funcptr_t func,
			      const char *fname, int return_type, char **anames, const char *args_fmt, ...) {
  void *receipt;
  lives_funcinst_t *finst;
  boolean free_funcinst = FALSE;
  if (args_fmt && *args_fmt) {
    va_list va;;
    va_start(va, args_fmt);
    finst = lives_funcinst_create_va(func ,fname, return_type, anames, args_fmt, va);
    va_end(va);
  }
  else finst = lives_funcinst_create_va(func ,fname, return_type, NULL, NULL, NULL);
  lives_funcinst_set_disposition(finst, DISPOSITION_STACKED);
  CL_DATA(finst, cb_flags) = cbflags;
  receipt = lives_hook_cb_add_funcinst(hooks, type, finst, 0);
  if (finst->flags & FINST_FLAG_REJECTED) lives_funcinst_free(finst);
  return receipt;
}


static lives_proc_thread_t update_linked_stacks(lives_duncinst_t *tinst) {
  uint64_t dflags = ADDMODE_UPD_LINKED; //sets prepend, has lock, norcp;
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  if (!is_fg_thread()) {
    GET_PROC_THREAD_SELF(self);
    lives_hook_stack_t **mystacks = my_hook_stacks();
    lives_microsleep_until_zero(pthread_mutex_lock(&mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex));
    if (!(mainw->global_hook_stacks[LIVES_GUI_HOOK]->flags & HS_FLAG_TRIGGERING))
      lives_hook_cb_add(mainw->global_hook_stacks, LIVES_GUI_HOOK, finst, dflags);
    pthread_mutex_unlock(&mainw->global_hook_stacks[LIVES_GUI_HOOK]->mutex);
    for (LiVESList *links = mainw->all_hstacks; links; links = links->next) {
      lives_hook_stack_t **xhs = (lives_hook_stack_t **)links->data;
      if (xhs == mystacks) continue;
      lives_microsleep_until_zero(pthread_mutex_lock(&xhs[LIVES_GUI_HOOK]->mutex));
      if (xhs[LIVES_GUI_HOOK]->flags & HS_FLAG_TRIGGERING) {
	pthread_mutex_unlock(&xhs[LIVES_GUI_HOOK]->mutex);
	continue;
      }
      lives_hook_cb_add(xhs, LIVES_GUI_HOOK, finst, dflags);
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
      lives_hook_cb_add(xhs, LIVES_GUI_HOOK, finst, dflags);
      pthread_mutex_unlock(&xhs[LIVES_GUI_HOOK]->mutex);
    }
  }
  pthread_mutex_unlock(&mainw->all_hstacks_mutex);
  return NULL;
}


lives_result_t lives_hook_trigger(lives_hook_stack_t **hstacks, int type) {
  static pthread_mutex_t recheck_mutex = PTHREAD_MUTEX_INITIALIZER;
  lives_hook_stack_t *hstack;
  lives_proc_thread_t lpt;
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
  boolean rerun = TRUE;

  //if (type == SYNC_ANNOUNCE_HOOK) dump_hook_stack(hstacks, type);

  hstack = hstacks[type];
  if (hstack->flags & HS_FLAG_INVALID) return LIVES_RESULT_INVALID;

  hs_op_flags = get_hs_op_flags(hstack);

  if (hs_op_flags & HOOKSTACK_ASYNC_PARALLEL)
    return LIVES_RESULT_ERROR;

  if (!(hs_op_flags & HOOKSTACK_ANON_TRIGGER)
      && ((type >= N_NATIVE_HOOKS && hstack->owner.lpt != self)
	  || type < N_NATIVE_HOOKS && hstack.owner.thread != pthread_self()))
    return LIVES_RESULT_NOPERM;
  
  hmutex = &(hstack->mutex);

  if (type != FATAL_HOOK) {
    PTMLH;
    hmulocked = TRUE;
  }

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
    finst = (lives_funcinst_t *)list->data;
    if (!finst) continue;
    zzfinst = cleanup_receipts(finst);
    cbflags = CL_FLAGS(finst, cb_flags);
    if ((cbflags & HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, list);
      continue;
    }
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_ACTIONED;
  }

  do {
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

      cbflags = CL_DATA(finst, cb_fkags);

      if (cbflags & HOOK_STATUS_RUNNING) continue;

      if (cbflags & (HOOK_STATUS_BLOCKED | HOOK_CB_IGNORE)) {
	lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
	continue;
      }

      if (cbflags & HOOK_STATUS_REMOVE) {
	remove_from_hstack(hstack, list);
	continue;
      }

      if (!(cbflags & HOOK_STATUS_ACTIONED)) continue;
      
      /* lives_cond_create(COND_BEGIN. COND_EQUALS, COND_INT32_VAR(finst->funcdef->return_type), */
      /* 			COND_INT32_CONST(WEED_SEED_BOOLEAN), COND_END); */

      /* if (CL_DATA(finst, trigger_cond)) { */
      /* 	if (!lives_cond_eval(CL_DATA(finst, trigger_cond))) { */
      /* 	  if (hs_op_flags & HSTACK_REMOVE_ON_COND_FAIL) */
      /* 	    remove_from_hstack(hstack, list); */
      /* 	  continue; */
      /* 	} */
      /* } */

      if (req_stack) {
	if ((myhints & flags) == myhints) {
	  int dflags = ADDMODE_TRANSFER;
	  cbflags &= ~HOOK_STATUS_ACTIONED;
	  cbflags |= hstack->req_target_set_flags;
	  CL_DATA(finst, cb_flags) = cbflags;
  //lives_proc_thread_show_func_call(closure->proc_thread);
	  lives_hook_cb_add(hstack->req_target_stacks, hstack->req_target_type,
			    finst, dflags);
	  if (finst->flags & FINST_FLAG_REJECTED)
	    lives_funcinst_free(finst);
	  hstack->stack =
	    (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack, list, FALSE);
	}
	continue;
      }

      if (cbflags & HOOK_OPT_ADDER_RUNS) {
	lives_proc_thread_t adder = CL_DATA(finst, adder);
	weed_plant_t *strucval = valplant_for_struct("lives_funcinst_t", finst);
	lives_proc_thread_try_interrupt(adder, strucval);
	continue;
      }

      if (type == LIVES_GUI_HOOK) {
	uint64_t xflags = cbflags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
	if (xflags) {
	  if (!have_recheck_mutex) {
	    if (pthread_mutex_trylock(&recheck_mutex)) {
	      // this means some other thread also has invalidate_data, we must let it run then recheck
	      // UNREF
	      lives_proc_thread_unref(finst->runner);
	      lives_sleep_until_zero(pthread_mutex_trylock(&recheck_mutex));
	      have_recheck_mutex = TRUE;
	      break;
	    }
	  }
	  CL_DATA(finst, cb_flags) = xflags;
	  update_linked_stacks(finst);
	}
      }

      cbflags |= HOOK_STATUS_RUNNING;
      cbflags &= ~HOOK_STATUS_ACTIONED;
      CL_DATA(finst, cb_flas) = cbflags;

      /* lives_proc_thread_exclude_states(finst->runner, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED */
      /* 				       | THRD_STATE_FINISHED); */

      if (!(cbflags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
	// SELF RUN CALLBACK
	//
	GET_PROC_THREAD_SELF(self);
	if (type != FATAL_HOOK) PTMUH;
	hmulocked = FALSE;

	if (cbflags & HOOK_OPT_REMOVE_ON_FALSE
	    && finst->funcdef->return_type == WEED_SEED_BOOLEAN) {
	  rem_on_false = TRUE;
	  bret = TRUE; // set in case func is cancelled
	  if (!finst->retloc) finsr->retloc = &bret;
	}

	lives_funcinst_send_replies(finst, LIVES_REPLY_YES); 
	///// run the callback (self running)
	lives_funcinst_execute(finst);

	rcpts = CL_DATA(finst, receipts);
	reply = lives_cb_receipt_get_req_reply(rcpts->data);

	lives_proc_thread_try_interrupt(lives_proc_thread_t lpt, weed_plant_t *data) {
	if (cbflags & HOOK_STATUS_REMOVE) {
	  if (reply == LIVES_REPLY_YES)
	    lives_funcinst_send_replies(finst, LIVES_REPLY_FULFILLED); 
	  remove_from_hstack(hstack, list);
	  break;
	}
      } else {
	// PUSH CALLBACK TO FG THREAD
	//
	if (type != FATAL_HOOK) PTMUH;
	hmulocked = FALSE;
	// this function will call fg_service_call directly,
	// block until the lpt completes or is cancelled
	// We should have set ONESHOT and BLOCK as appropriate


	cbflags |= (HOOK_OPT_ONESHOT | HOOK_OPT_BLOCK);
	CL_DATA(finst, cb_flags) = cbflags;
	lives_funcinst_queue(finst, LIVES_THRDATTR_FG_THREAD | LIVES_THRDATTR_FG_LIGHT);
      }

      if (type != FATAL_HOOK) PTMLH;
      hmulocked = TRUE;

      cbflags &= ~HOOK_STATUS_RUNNING;      

      CL_DATA(finst, cb_flags) = cbflags;

      if (CL_DATA(finst, adder) == self) cleanup_receipts(finst);

      if ((flags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT) ||
	   (rem_on_false && !*(boolean *)finst->retloc))) {
	remove_from_hstack(hstack, list);
	break;
      }

      // will be be HOOK_DTL_SINGLE
      if (hs_op_flags & HOOKSTACK_RUN_SINGLE) {
	//g_print("done single\n");
	retval = TRUE;
	rerun = FALSE;
	break;
      }
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
	  finst = (lives_funcinst_t *)list->data;
	  if (finst) CL_DATA(finst, cb_flags) &= ~HOOK_STATUS_ACTIONED;
	}
      }
    }
    if (hmulocked) {
      if (type != FATAL_HOOK) PTMUH;
      hmulocked = FALSE;
    }
  } while (rerun);

 trigdone:

  if (req_stack) pthread_mutex_unlock(&req_stack->mutex);

  if (type != FATAL_HOOK && hmulocked) PTMUH;

  hstacks[type]->flags &= ~HS_FLAG_TRIGGERING;

  if (have_recheck_mutex) pthread_mutex_unlock(&recheck_mutex);

  //if (type == SYNC_WAIT_HOOK) g_print("sync all res: %d\n", retval);
  return retvaL ? LIVES_RESULT_SUCCESS | LIVES_RESULT_FAIL;
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
      hstacks = my_hook_stacks();
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
    finst = (lives_funcinst_t *)list->data;
    if (!finst) continue;
    cleanup_receipts(finst);
    cbflags = CL_DATA(finst, cb_flags);
    if (cbflags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstack, list);
      continue;
    }
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_ACTIONED;
  }

  list = (LiVESList *)hstack->stack;

  for (; list; list = listnext) {
    listnext = list->next;
    finst = (lives_funcinst_t *)list->data;

    if (!finst) continue;
    cbflags = CL_DATA(finst, cb_flags);
    if (!(cbflags & HOOK_STATUS_ACTIONED)) continue;

    cbflags &= ~HOOK_STATUS_ACTIONED;

    if ((cbflags & HOOK_STATUS_BLOCKED) || (cbflags & HOOK_STATUS_RUNNING)) continue;

    if (cbflags & (HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, list);
      continue;
    }

    hsflags |= HS_FLAG_TRIGGERING;

    lives_proc_thread_exclude_states(lpt, THRD_TRANSIENT_STATES | THRD_STATE_COMPLETED
				     | THRD_STATE_FINISHED);

    cbflags |= HOOK_STATUS_RUNNING;
    CL_DATA(finst, cb_flags) = cbflags;


    lives_finst_queue(lpt, LIVES_THRDATTR_FAST_QUEUE);

    ncount++;
  }
  PTMUH;
  return ncount;
}


lives_result_t lives_proc_thread_trigger_hook(lives_proc_thread_t lpt, int type) {
  return lpt ? lives_hook_trigger(lives_proc_thread_get_hook_stacks(lpt), type) : LIVES_RESULT_INVALID;
}


/* static void _lives_hook_tr_seq(lives_hook_stack_t **hstacks, int type,  hook_funcptr_t finfunc, */
/* 			       void *findata) { */
/*   GET_PROC_THREAD_SELF(self); */
/*   while (1) { */
/*     if (lives_proc_thread_get_cancel_requested(self)) { */
/*       lives_proc_thread_cancel(self); */
/*       return; */
/*     } */
/*     if (lives_hook_trigger(hstacks, type)) { */
/*       // if all functions return TRUE, execute finfunc, and exit */
/*       if (finfunc)((*finfunc)(NULL, findata)); */
/*       if (lives_proc_thread_get_cancel_requested(self)) { */
/* 	lives_proc_thread_cancel(self); */
/*       } */
/*       return; */
/*     } */
/*     if (lives_proc_thread_get_cancel_requested(self)) { */
/*       lives_proc_thread_cancel(self); */
/*       return; */
/*     } */
/*     lives_nanosleep(SYNC_CHECK_TIME); */
/*   } */
/* } */


/* lives_proc_thread_t lives_hook_trigger_async_sequential(lives_hook_stack_t **hstacks, int type, */
/* 							hook_funcptr_t finfunc, void *findata) { */
/*   lives_proc_thread_t poller = lives_proc_thread_create(LIVES_THRDATTR_START_CANCELLABLE, */
/* 							(lives_funcptr_t)_lives_hook_tr_seq, -1, "viFv", */
/* 							hstacks, type, (weed_funcptr_t)finfunc, findata);; */
/*   return poller; */
/* } */


lives_result_t lives_hook_cb_invalidate(lives_funcinst_t *finst) {
  if (finst && finst->dispostition == DISPOSITION_STACKED) {
    GET_PROC_THREAD_SELF(self);
    if (self != CL_DATA(finst, adder)) return LIVES_RESULT_NOPERM;
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



static void lives_hook_cb__wait_fulfilled(lives_funcinst_t *finst) {
  lives_proc_thread_t lpt = CL_DATA(finst, runner);
  boolean ret = lives_proc_thread_join_boolean(lpt);
  if (!ret || lives_proc_thread_was_cancelled(lpt)
      || lives_proc_thread_had_error(lpt)) {
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_REMOVE;
  }
  cleanup_receipts(finst);
  lives_proc_thread_unref(lpt);
}

 

void lives_hook_async_join(lives_hook_stack_t **hstacks, int htype) {
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
      hstacks = my_hook_stacks();
      if (!hstacks) return;
    }
  }

  hstack = hstacks[htype];

  if (!(hstack->flags & HS_FLAG_TRIGGERING)) return;

  hmutex = &(hstack->mutex);
  PTMLH;

  hs_op_flags = get_hs_op_flags(hstack);

  if (!(hs_op_flags & HOOKSTACK_ASYNC)) {
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

    lives_hook_cb_wait_fulfilled(finst);

    PTMLH;

    cleanup_receipts(finst);

    if (CL_DATAflags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT))
      remove_from_hstack(hstack, cblist);
    lives_funcinst_free(finst);
     continue;
    }

    cbflags &= ~HOOK_STATUS_RUNNING;

    if (lives_proc_thread_was_cancelled(lpt)) {
      remove_from_hstack(hstack, cblist);
      lives_funcinst_free(finst);
      continue;
    }
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


LIVES_GLOBAL_INLINE lives_funcinst_t *lives_funcinst_new(lives_funcdef_t *tmpl) {
  LIVES_CALLOC_TYPE(lives_funcinst_t, finst, 1);
  if (finst) {
    finst->uid = gen_unique_id();
    finst->funcdef = tmpl;
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
  if (finst) {
    if (finst->next) lives_funcinst_free(lives_sync_list_pop(&finst_next));

    if (finst->disposition == DISPOSITION_STACKED) {
      // before we can free a funcinst with disposition stacked
      // we must ensure that all the receipts held by it are exired
      // - send an INVALID reply so any param data can be freed / blockers unblocked
      // if there is a sendt_reply callback, adder can
      if (CL_DATA(finst, receipts)) {
	lives_finxt_send_replies(finst, LIVES_REPLY_INVALID);
	if (CL_DATA(finst, receipts) && !(finst->flags & FINST_FLAG_REJECTED)) return;
      }
    }

    if (finst->flags & FINST_FLAG_STATIC) {
      finst->flags &= ~FINST_FLAG_REJECTED;
      return;
    }
    if (retloc && !(finst->flags & FINST_FLAG_NOFREE_RETLOC)) lives_free(retloc);
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


