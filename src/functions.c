// functions.c
// (c) G. Finch 2002 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

#include "diagnostics.h"

static hook_stack_descriptor_t hs_desc[N_HOOK_POINTS];

static boolean hs_inited = FALSE;
static boolean hsn_inited = FALSE;

VARNAME_FUNC

char **lives_cond_create(const char *condition, ...) {
  // place holder
  return NULL;
}

#if 0
///////////////////////////////////////////////

static LiVESList *cond_trans_list = NULL;

#define N_COND_FDEFS 64

static lives_funcdef_t *cond_fdefs[N_COND_FDEFS];

// value funcs
static allvalues_t *cond_allv_i(const char *vstr) {
  return MAKE_ALLVALUE(WEED_SEED_INT, atoi(vstr)));
}
// etc

// opfuncs
static allvalues_t *cond_equals(int nvals, allvalues_t **allvpp) {
  LIVES_ASSERT(nvals == 2);
  boolean res = FALSE;
  switch (allvpp[0]->stype) {
  case WEED_SEED_INT:
    res = (allvpp[0]->values.i[0] == allvpp[1]->values.i[0]);
    break;
    // etc
  default: break;
  }
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, res);
}

static allvalues_t *cond_bit_set(int nvals, allvalues_t **allvpp) {
  LIVES_ASSERT(nvals == 2);
  boolean res = FALSE;
  switch (allvpp[0]->stype) {
  case WEED_SEED_INT:
    res = (allvpp[0]->values.i[0] & allvpp[1]->values.i[0]);
    break;
    // etc
  default: break;
  }
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, res);
}


static cond_trans *find_ctrans(char *tok) {
  for (LiVESList *list = cond_trans_list; list; list = list->next) {
    cond_trans *ctrans = (cond_trans *)list->data;
    if (!lives_strcmp(tok, ctrans->sym)) return ctrans;
  }
  return NULL;
}


static lives_funcdef_t *get_funcdef_for(const char *name) {
  for (int i = 0; conf_fdefs[i]; i++) {
    if (!lives_strcmp(fname, cond_fdefs[i]->funcname)) return cond_fdefs[i];
  }
  return NULL;
}


boolean check_known_funcsigs(funcsig_t fsig) {
  if (!fsig) return TRUE;
  for (LiVESList *list = capable->known_funcsigs; list; list = list->next)
    if (fsig == *(funcsig_t *)list->data) return TRUE;

  if (prefs->show_dev_opts) {
    char *symstr;
    char *args_fmt = args_fmt_from_funcsig(fsig);
    char *filen = lives_strdup("funcsigs.h");
    const char *funcname = finst->funcdef->funcname;
    if (finst->paramnames) {
      symstr = lives_strdup("");
      for (int i = 0; i < nparms; i++)
	symstr = lives_strdup_concat(symstr, ", ", "%s", finst->paramnames[i] ? finst->paramnames[i] : "");
    }
    else symstr = funcsig_to_symstring(sig);
    msg = lives_strdup_printf("\nUnrecognised args_fmt \"%s\" in call to %s.\n"
			      "Please add a line in the appropriate #define in %s:\n\n"
			      "\tADD_FUNCSIG(%d,%s)\n", args_fmt, funcname, filen, nparms, symstr);
    lives_free(symstr); lives_free(filen);
    lives_free(args_fmt);
  }
  return FALSE;
}



static lives_funcdef_t *find_or_make_fundef(const char *funcname,
					    lives_funcptr_t func, weed_seed_t ret_type, const cgar *args_fmt) {
  if (!check_known_funcsigs(funcsig_from_args_fmt(args_fmt))) return NULL;
  lives_funcdef_t *fdef = get_funcdef_for(funcname);
  if (!fdef) fdef = MAKE_FUNCDEF(func, ret_type, args_fmt);
  return fdef;
}


lives_condition_t _lives_cond_create(const char *condition, ...) {
  // string condition is passed to vsprintf, so values are inserted
  // this is then broken into pieces delimited by ","
  // we read a token, which must begin with the prefix ("COND_")
  // we look this up in ctrans table. If fmt is "@", we simply insert symbol, fmt
  // cond[idx] = "SYMBOL|@"
  // If fmt is "X", we replace the token in input string with subst text, then parse it again
  // if fmt begins with "*" or "A", we write sym|fmt|funcdef
  // eg, "COND_EQUALS|AA|0x12344444"
  // if fmt is "S", we insert sym. fmt, funcdef, and if constval is not NULL, constval
  // if constval IS null, we insert next string piece
  // e.g "COND_SYMBOL|S|cond_local_val|old_value|"
  // or "COND_INT_CONST|S|cond_allv_i|2
  //
  // (when evaluating, for "S", we call function like allvp[n] = cond_allv_i("2")
  // if fmt is "A.." or "A*", we create an array of allvalues t and do partial evals until
  // we have enough. If the fmt ends with a *, we look for a matching COND_LIST_START and COND_LIST_END
  // pair,
  // _COND_PARENS_OPEN -> push current func, push current val -> call eval
  // pop func, pop val call func with popped and eval return
  // parens_close -> return current val

  if (!condstr) return NULL;

  lives_condition cond = NULL;
  LiVESList *toklist, *otoklist;
  cond_trans *ctrans;
  size_t ntoks;
  char output[3];
  char *condstr;
  size_t condstrsize = lives_strlen(condstr):
  int nstr = 0;

  if (!condstrsize) return NULL:

		      // first apply any preprocessing tokens (fmt == 'Z')
		      // search for "COND_", check for match and replace token
		      for (int i = 0; i < condstrlen - COND_PFXLEN; i++) {
			if (!lives_strncmp(condstr[i], COND_PFX, COND_PFXLEN)) {
			  // todo - replace tokens
			}
		      }

  va_start(ap, condition);
  condstr = lives_strdup_vprintf(condition, ap);
  va_end(ap);

  otoklist = toklist = get_token_count_split(condstr, ',', NULL);
  tok = lives_strchomp((char *)toklist->data);
  if (lives_strcmp(tok, _COND_BEGIN)) goto fmt_err;
  output[0] = output[1] = output[2] = NULL;

  for (; toklist; toklist = toklist->next) {
    char *otok = tok = lives_strchomp((char *)toklist->data);
    cond = lives_realloc(cond, nstr + 1, sizeof(char *));
    while (1) {
      ctrans = find_ctrans(tok);
      if (!ctrans) goto fmt_err;
      if (*ctrans->fmt == 'X') {
	LiVESList *xtoklist = get_token_count_split(ctrans->subst, ',', NULL);
	lives_free(toklist->data);
	lives_list_concat(toklist, xtocklist);
      }
      else break;
    }
    if (!lives_strcmp(tok, _COND_POPEN)
	|| !lives_strcmp(tok, _COND_PCLOSE)) { 
      cond[nstr++] = lives_strdup_printf("%s|@|%s", otok, tok);
    }
    else {
      char *nxttok = lives_strchomp((char *)toklist->next->data);
      if (ctrans->dispvalfunc) output[0] = (*ctrans->dispvalfunc)(ctrans->fmt, nxttok);
      else output[0] = lives_strdup(otok);

      output[1] = lives_strdup(ctrans->fmt);

      if (lives_strncmp(nxttok, COND_PFX, COND_PFXLEN)) {
	if (ctrans->valfunc) output[2] = (*ctrans->valfunc)(ctrans->fmt, nxttok);
	else if (ctrans->nxt_fmt) output[2] = lives_strdup_vprintf(ctrans->nxtfmt, nxttok);
	else output[2] = lives_strdup(nxttok);
      }
      
      if (output[2])
	cond[nstr++] = lives_strdup_printf("%s|%s|%s", output[0], output[1], output[2]);
      else cond[nstr++] = lives_strdup_printf("%s|%s", output[0], output[1]);

      for (int i = 0; i < 3; i++) if (output[i]) {
	  lives_free(output[i]);
	  output[i] = NULL;
	}
      lives_free(nxttok);
    }
    if (otok != tok) lives_free(otok);
    lives_free(tok);
    toklist = toklist->next;

    if (!lives_strcmp(otok, _COND_FINISH)) break;
  }

  goto done;

 fmt_err:
  tmp = LSPF("format error in lives_make_cond: %s\n%s\n", tok, condstr);
  LIVES_WARN(tmp);
  lives_free(tmp); lives_free(tok);
  if (otok != tok) lives_free(otok);
  for (; toklist; toklist = toklist->next) lives_free(toklist->data);
  if (cond) {
    for (int i = 0; i < nstr; i++) lives_free(cond[i]);
    lives_free(cond);
    cond = NULL;
  }

 done:
  if (otoklist) lives_list_free(otoklist);
  if (condstr) lives_free(condstr);
  return cond;
}


void register_cond_sym(const char *sym, const char *fmt, ...) {
  va_list ap;
  LIVES_CALLOC_TYPE(cond_trans, ctrans, 1);
  if (*fmt == 'X') ctrans->sym = lives_strdup(sym);
  else ctrans->sym = LSPF(sym, COND_PFX "%s", sym);

  ctrans->fmt = fmt;
  if (*fmt == '@') goto done;

  va_start(ap, fmt);
  if (*fmt == 'X') ctrans->subst = lives_strdup(va_arg(ap, char *));
  else {
    ctrans->desc = lives_strdup(va_arg(ap, char *));
    funcname = lives_strdup(va_arg(ap, char *));
    ctrans->funcdef = funcdef_for(funcname);
    if (*fmt == 'S') {
      ctrans->constval = lives_strdup(va_arg(ap, char *));
    }
  }
  va_end(ap);

 done:
  cond_trans_list = lives_list_prepend(cond_trans_list, (void *)ctrans);
}


lives_result_t cond_eval(lives_condition cond, int *idx, allvalues_t *avp, boolean full) {
  // function will evaluate cond starting at position idx
  // there are 2 modes - full and partial
  // partial is for function params, and will return thr first value obtained
  // full will parse until close_parens is read and will then set avp->b to curr_bool
  return LIVES_RESULT_SUCCESS;
}
  

void lives_conditions_init(void) {
  int i = 0;
  // values are actually COND_whatever, unless fmt is X

  // NOW: format is symbol, description, function, (const char *) constval
  // the function is looked up from the name, which returns a funcdef
  // for value funcs, the fmt is "S"
  // if constval is NULL, piece 1 of the cond array line is set from the following string piece
  // eg, "COND_INT_CONST|2".
  // During eval this would become cond_allv_i("2") which would return an allvalues_t *
  // with type WEED_SEED_INT and value.i == 2
  //
  // if the symbol is registered with non-NULL constval this is used instead of next stting piece e.g
  //
  // "COND_SYM_LOCAL|new_value"
  //
  // COND_INT_VAR is also translated like this:
  // COND_SYM_LOCAL|src_item/funcdef/return_type
  //
  // during eval the first would call cond_local_value("new_value") which would look up "new_value"
  // in the context data_book, and return an allvalues_t *
  //
  // the second would evaluate as cond_local_value("src_item/funcdef/return_type") which would look up "src_item"
  // in the context data_book, find the value of the funcdef field, then within that, the value of return_type
  //
  // then we have operators which have fmt of type "AA" for example.
  // Here A is a special type repreasenting an allvalues_t *
  // In reality the function will have two real params (int nvals, allvalues_t **vals)
  // the function will check type and number of allvalues, apply the operation and return an allvalues_t *
  // the returned allvalues_t * may be then used to fill a pushed allvalues_t *array
  // the args_fmt may also be "A*", "AA*", "*", etc. where the * repesents any number of allvalues *

  // we have 2 special symbols: COND_PARENS_OPEN and COND_PARENS_CLOSE
  // COND_PARENS_OPEN (and its alias, COND_START) has the following effect
  // the current funcdef and allvalues_t * array are pushed to the eval stack,
  // eval is called recursively with full set to TRUE
  //
  // the difference between full and partial eval:
  // for partial we evaluate symbols until an allvalues_t * is returned,
  // and this becomes the next value in allvalues_t * array.
  // if we parse a COND_PARENS_OPEN, we push func, and allvalues_t *array, call a full eval,
  // which only returns when COND_PARENS_CLOSE is read. The function and array are popped, and then return val
  // is added to allvals_t * array. At the top level we have no function, and the first symbol read will be
  // COND_START, which is an alias for COND_PARENS_OPEN, COND_END is an alias for COND_PARENS_CLOSE and must be
  // final symbol read.
  // The first eval should return an allvalues_t * with type WEED_SEED_BOOLEAN,
  // which is the return value for the condition
  
  // symbols with fmt "@" are not followed by any data
  // eg. "COND_PARENS_OPEN" -> "COND_PARENS_OPEN|"
  register_cond_sym(_COND_POPEN, "@");
  register_cond_sym(_COND_PCLOSE, "@");

  register_cond_sym(_COND_LIST_OPEN, "@");
  register_cond_sym(_COND_LIST_CLOSE, "@");

  // "X" - alias  (no "COND_" prepended)
  // these are replaced durins lives_cond_create
  // e.g "(" -> "COND_PARENS_OPEN|"
  register_cond_sym("(",	"X",	 	_COND_POPEN);
  register_cond_sym(")", 	"X",		_COND_PCLOSE);

  // symbols are checked for before replavement
  register_cond_sym(_COND_BEGIN,	"X",   	_COND_POPEN);
  register_cond_sym(_COND_FINISH, 	"X",   	_COND_PCLOSE);

  register_cond_sym("COND_INT_VAR", "X", COND_SYMBOL);
  
  // these are the evaluation functions, each corresponds to a funcdef
  // the params for each are "S", so we will pass piece 1 as data
  // e.g "COND_INT_CONST", "2", ...  -> "COND_INT_CONST|2"
  // then at evaluation time

  register_cond_sym("INT_CONST", "S", "", "cond_allv_i", NULL);
  register_cond_sym("UINT_CONST", "S", "",  "cond_allv_u", NULL);
  register_cond_sym("INT64_CONST", "S", "", "cond_allv_I", NULL);
  register_cond_sym("UINT64_CONST", "S", "",  "cond_allv_U", NULL);
  register_cond_sym("BOOL_CONST", "S", "", "cond_allv_b", NULL);
  register_cond_sym("DOUBLE_CONST", "S", "",  "cond_allv_d", NULL);
  register_cond_sym("FLOAT_CONST", "S", "", "cond_allv_f", NULL);
  register_cond_sym("STRING_CONST", "S", "",  "cond_allv_S", NULL);
  register_cond_sym("VOIDPTR_CONST", "S", "", "cond_allv_V", NULL);
  register_cond_sym("FLOATPTR_CONST", "S", "",  "cond_allv_F", NULL);
  register_cond_sym("PLANTPTR_CONST", "S", "", "cond_allv_P", NULL);
  
  // aliases for symbols - (values corresponding to context dependant DATA_BOOK)
  // these are replaced 
  register_cond_sym("SYM_SRC_ITEM", "S", "{$p0}", "cond_local_val",	"src_item");
  register_cond_sym("SYM_OLD_VALUE", "S", "{$p0}", "cond_local_val",	"old_value");
  register_cond_sym("SYM_NEW_VALUE", "S", "{$p0}", "cond_local_val",	"new_value");

  register_cond_sym("SYMBOL", "S", "${$p0}", "cond_local_val", NULL);
  register_cond_sym("GLOBAL", "S", "@{$p0}", "cond_global_val", NULL);
  
  // value ops
  register_cond_sym("EQUALS", "AA", "$p0 is equal to $p1",  "cond_equals");
  
  // TODO:  "cond_equalsu", "cond_equalsU", "cond_equalsf"

  register_cond_sym("BIT_SET", "AA",  "$p0 has bit $p1 set", "cond_bit_set");

  // logic ops
  register_cond_sym("NOT", "A",  "NOT $p0",	        "cond_logic_not");
  register_cond_sym("OR",  "AA", "$p0 AND $p1",		"cond_logic_or");
  register_cond_sym("AND", "AA", "$p0 AND/OR $p1",	"cond_logic_and");
  register_cond_sym("XOR", "AA", "either $p0 OR $p1",	"cond_logic_xor");

  // this is going to be followed by a lives_funcptr_t and then an args_fmt, then ,atching params, const or var
  // we need to handle this specially - first we get the func
  /* register_cond_sym("TESTFUNC_CONST", "",	 	"Z",		"COND_XTESTFUNC_CONST, %p"); */
  /* register_cond_sym("XTESTFUNC_CONST", "",	 	"!",		NULL); */

  // END OF COND TOKENS //

  // built-in funcdefs

  lives_memset(cond_fdefs, 0, N_COND_DEFS * sizeof(lives_funcdef_t *));

  MAKE_COND_OPFUNC(equals);
  MAKE_COND_OPFUNC(bit_set);
  MAKE_COND_OPFUNC(logic_not);
  MAKE_COND_OPFUNC(logic_and);
  MAKE_COND_OPFUNC(logic_or);
  MAKE_COND_OPFUNC(logic_xor);

  MAKE_COND_VALFUNC(local_val);
  MAKE_COND_VALFUNC(global_val);
  MAKE_COND_VALFUNC(allv_i);
  MAKE_COND_VALFUNC(allv_u);
  MAKE_COND_VALFUNC(allv_I);
  MAKE_COND_VALFUNC(allv_U);
  MAKE_COND_VALFUNC(allv_b);
  MAKE_COND_VALFUNC(allv_d);
  MAKE_COND_VALFUNC(allv_f);
  MAKE_COND_VALFUNC(allv_S);
  MAKE_COND_VALFUNC(allv_V);
  MAKE_COND_VALFUNC(allv_F);
  MAKE_COND_VALFUNC(allv_P);
}

#endif

///////////////////////

lives_structdef *stdef_for(char *stname) {
  // return stdef for struct type stname
  lives_structdef *stdef = NULL;
  return stdef;
}


static void bind_to_fields(lives_struct_t *strct) {
  // create leaves in strct->plant according to values
  //lives_structdef *stdef = strct->stdef;
  // go through stdef->fields, and bind plant leaf values to
  // struct offsets
}


lives_struct_t *lives_struct_new(char *stname) { 
  lives_structdef *stdef = stdef_for(stname);
  if (!stdef) return NULL;
  LIVES_CALLOC_TYPE(lives_struct_t, strct, 1);
  strct->stdef = stdef;
  strct->strct = lives_calloc(1, stdef->st_size);
  strct->plant = lives_plant_new(LIVES_PLANT_STRUCT_MIRROR);
  bind_to_fields(strct);
  return strct;
}


allvalues_t *allvalues_from_leaf(allvalues_t *avp, weed_plant_t *plant, const char *key) {
  int ne;
  weed_seed_t st;
  if (!avp) avp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
  ALLV_FROM_LEAF(avp, plant, key, st, ne);
  avp->stype = st;
  avp->ne = ne;
  return avp;
}


allvalues_t *_make_allval(allvalues_t *avp,
			  weed_seed_t stype, weed_size_t ne, int flags, const char *valname, ...) {
  va_list args;
  uint64_t xflags = 0;
  if (stype == WEED_SEED_INVALID) {
    if (!avp) avp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    xflags = ALLV_ERR_STYPE;
  }
  else {
    weed_plant_t *tmp = lives_plant_new(LIVES_PLANT_TMP);
    va_start(args, valname);
    if (flags & PARAM_FLAG_BOUND) {
      if (ne > 1) {
	va_end(args);
	weed_plant_free(tmp);
	return NULL;
      }
      xflags = ALLV_FLAG_POINTER;
      weed_leaf_from_vargp(tmp, "val", stype, ne, args);
    }
    else weed_leaf_from_varg(tmp, "val", stype, ne, args);
    va_end(args);
    avp = allvalues_from_leaf(avp, tmp, "val");
    weed_plant_free(tmp);
  }
  avp->aname = lives_strdup(valname);
  avp->flags = xflags;
  return avp;
}


void allvalue_free(allvalues_t *avp) {
  if (avp) {
    if (avp->aname) lives_free(avp->aname);
    if (avp->contingencies) {
      for (LiVESList *list = avp->contingencies; list; list = list->next)
	lives_funcinst_free((lives_funcinst_t *)list->data);
      lives_list_free(avp->contingencies);
    }
    // TODO - free typed value *, unless flags & PARAM_FLAGS_BOUND
  }
}


lives_structdef *parse_structdef(const char *stname, const char *stdefdata, size_t stsize) {
  LIVES_CALLOC_TYPE(lives_structdef, stdef, 1);
  stdef->uid = gen_unique_id();
  stdef->struct_name = lives_strdup(stname);
  //
  // TODO -
  // parse stdefdata - skip any comments. for the rest we expect ctype (*) fieldname;
  // create a strctdef field with name, seed_type, offset, nvals (eg. [4])
  // for offset we guess - add sizeof(type) to current offs rounded up to sizeof(type)
  // after adding all fields we check that offs = strct size
  //
  // for unknown types, look for a comment like //@TYPEDEF seed_type alt_type 
  //
  stdef->st_size = stsize;
  // add stdef to hash store, keyed by stname
  return stdef;
}


////////////////////////////

static void init_hook_stacks(void) {
  if (hs_inited && hsn_inited) return;
  for (int i = 0; i < N_HOOK_POINTS; i++) {
    uint64_t flags = 0;
    hook_stack_pattern_t pat = HOOK_PATTERN_DATA;
    hook_stack_descriptor_t *xhs = (hook_stack_descriptor_t *)&hs_desc[i];
    boolean got = FALSE;
    xhs->htype = i;

    if (!hsn_inited) {
      switch (i) {
      case FATAL_HOOK:
	flags = HS_FLAGS_FATAL;
	pat = HOOK_PATTERN_SPONTANEOUS;
	got = TRUE;
	break;

      case THREAD_EXIT_HOOK:
	flags = HS_FLAGS_THREAD_EXIT;
	pat = HOOK_PATTERN_SPONTANEOUS;
	got = TRUE;
	break;
     default: break;
      }
    }

    if (!got) {
      if (FEATURE_READY(CONDITIONALS)) {
	hs_inited = TRUE;
	switch (i) {
	case COMPLETED_HOOK:
	  hs_desc[i] = HS_DETAILS(COMPLETED);
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
	case CB_ADDED_HOOK:
	  flags = HS_FLAGS_CB_ADDED;
	  pat = HOOK_PATTERN_SPONTANEOUS;
	  break;
	default: break;
	}
      }
    }
    xhs->pattern = pat;
    xhs->op_flags = flags;
    g_print("Registered details for hs type %d, pattern = %d, flags = %lu\n", i, pat, flags);
  }
  hsn_inited = TRUE;
}


const hook_stack_descriptor_t *get_hs_desc(int hstype) {
  if (!hs_inited || !hsn_inited) init_hook_stacks();
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
    case LIVES_SEED_CONST_CHARPTR: return SET_CUSTOM_LEAF_FROM_VARG(plant, key, type, ne, xargs);
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


LIVES_GLOBAL_INLINE weed_seed_t get_seedtype(char c) {
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
  return 0x0;
}


LIVES_GLOBAL_INLINE const char get_char_for_st(weed_seed_t st) {
  // seed_type to char
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].seed_btype == st) return crossrefs[i].letter;
  }
  return '\0';
}


LIVES_GLOBAL_INLINE uint8_t get_typecode_for_st(weed_seed_t st) {
  // seed_type to sigbits
  for (int i = 0; crossrefs[i].letter; i++) {
    if (crossrefs[i].seed_btype == st) return crossrefs[i].sigbits;
  }
  return 0x0;
}

LIVES_GLOBAL_INLINE const char *get_fmtstr_for_st(weed_seed_t st) {
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
static boolean fn_match_child(lives_funcinst_t *finst1, lives_funcinst_t *finst2);

static lives_result_t weed_plant_params_from_valist(weed_plant_t *plant, const char *args_fmt, \
						    make_key_f param_name_func, va_list xargs) {
  int p = 0;
  for (const char *c = args_fmt; *c; c++) {
    char *pkey = (*param_name_func)(p);
    uint32_t st = _char_to_st(*c);
    weed_error_t err = weed_leaf_from_varg(plant, pkey, st, 1, xargs);
    lives_free(pkey);
    if (err != WEED_SUCCESS) return LIVES_RESULT_ERROR;
    p++;
  }
  return LIVES_RESULT_SUCCESS;
}


const char *get_args_fmt(weed_plant_t *plant) {
  return plant ? weed_get_const_string_value(plant, LIVES_LEAF_ARGS_FMT, NULL) : NULL;
}


void set_args_fmt(weed_plant_t *plant, const char *args_fmt) {
  if (plant) weed_set_const_string_value(plant, LIVES_LEAF_ARGS_FMT, args_fmt);
}


boolean args_fmt_match(const char *def, const char *inst) {
  if ((!inst || !*inst) &&  (!def || !*def)) return TRUE;
  size_t deflen = lives_strlen(def), instlen;
  boolean variad = FALSE;
  if (def[deflen - 1] == '*') {
    if (!--deflen) return TRUE;
    variad = TRUE;
  }
  instlen = lives_strlen(inst);
  if (instlen < deflen || (!variad && instlen > deflen)) return FALSE;
  if (deflen > 0 && lives_strncmp(def, inst, deflen)) return FALSE;
  return TRUE;
}


lives_result_t funcinst_params_from_vargs(lives_funcinst_t *finst,  const char *args_fmt, va_list xargs) {
  lives_result_t res = LIVES_RESULT_INVALID;
  if (finst) {
    lives_funcdef_t *fdef = finst->funcdef;
    if (!args_fmt_match(args_fmt, args_fmt_from_funcsig(fdef->funcsig)))
      return LIVES_RESULT_ERROR;
    if (!args_fmt) return LIVES_RESULT_SUCCESS;
    if (!finst->params) finst->params = lives_plant_new(LIVES_PLANT_FUNCPARAMS);
    res = weed_plant_params_from_valist(finst->params, args_fmt, make_std_pname, xargs);
  }
  return res;
}


char *args_fmt_from_allvals(int nvals, allvalues_t **pvals) {
  funcsig_t fsig = 0;
  if (nvals > 16) return NULL;
  for (int i = 0; i < nvals; i++) {
    fsig = (fsig << 4) + get_typecode_for_st(pvals[i]->stype);
  }
  return args_fmt_from_funcsig(fsig);
}


lives_funcinst_t *_funcinst_from_allvals(lives_funcdef_t *fdef, lives_funcptr_t func,
					 const char *funcname, weed_seed_t ret_type,
					 int nvals, allvalues_t **pvals) {
  lives_funcinst_t *finst = NULL;
  char *args_fmt = args_fmt_from_allvals(nvals, pvals);
  if (!fdef) fdef = create_funcdef(funcname, func, ret_type, args_fmt, NULL, 0, 0);
  else {
    char *xargs_fmt = args_fmt_from_funcsig(fdef->funcsig);
    if (!args_fmt_match(xargs_fmt, args_fmt)) {
      lives_free(args_fmt); lives_free(xargs_fmt);
    }
  }
  finst = lives_funcinst_new(fdef);
  return finst;
  // TODO - set params from allvalues array
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


#if 0
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
#endif

int get_funcsig_nparms(funcsig_t sig) {
  int nparms = 0;
  for (funcsig_t test = 0xF; test & sig; test <<= 4) nparms++;
  return nparms;
}


static boolean lives_funcinst_error_state(lives_funcinst_t *finst) {
  lives_proc_thread_t lpt;
  pthread_rwlock_rdlock(&finst->dispolock);
  if (MODULE_TYPE_IS(finst,HOOK_STACK)) {
      pthread_rwlock_unlock(&finst->dispolock);
      LiVESList *recs = (LiVESList *)CL_DATA(finst, receipts);
      if (recs && recs->data) {
	int reply = lives_cb_receipt_get_req_reply(recs->data);
	return reply == LIVES_REPLY_ERROR;
      }
      return FALSE;
  }
  if (MODULE_TYPE_IS(finst, LPT)) {
    lpt = LPT_DATA(finst, runner);
    pthread_rwlock_unlock(&finst->dispolock); 
    if (lpt) return lives_proc_thread_had_error(lpt);
  }
  pthread_rwlock_unlock(&finst->dispolock); 
  return FALSE;
}


#define NEED_FSIG_CASES 1
#include "funcsigs.h"
#undef NEED_FSIG_CASES

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
  //   weed_set_int_value(p, "retval", int_func(weed_get_int_value(p, "p0", NULL)));
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

  lives_result_t res = LIVES_RESULT_SUCCESS;
  
  int sjval = 0;
  jmp_buf env;

  boolean is_stacked = FALSE;
  uint32_t ret_type = fdef->return_type;
  funcsig_t sig = funcsig_from_args_fmt(get_args_fmt(finst->params));
  int nparms = get_funcsig_nparms(sig);
  allfunc_t thefunc;

  // need to set this in case return type is void
  weed_error_t err = WEED_SUCCESS;

  if (ret_type == LIVES_SEED_CONST_CHARPTR)
    ret_type = WEED_SEED_VOIDPTR;

  thefunc.func = func;

  // if we have "bound" params, update them from live variables now
  update_params_from_proxies(finst);

  if (MODULE_TYPE_IS(finst, HOOK_STACK)) is_stacked = TRUE;
  
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

  if (lives_funcinst_error_state(finst) || err != WEED_SUCCESS) {
    g_print("got %d and %d\n" ,lives_funcinst_error_state(finst), err);
    res = LIVES_RESULT_ERROR;
    if (!is_stacked) {
      lives_proc_thread_t lpt = LPT_DATA(finst, runner);
      if (lives_proc_thread_had_error(lpt)) {
	int errnum = lives_proc_thread_get_errnum(lpt);
	int errsev = lives_proc_thread_get_errsev(lpt);
	const char *errmsg = lives_proc_thread_get_errmsg(lpt);
	int errline = lives_proc_thread_get_line_ref(lpt);
	const char *errfile = lives_proc_thread_get_file_ref(lpt);
	lives_make_errmsg_full(lpt, errfile, errline, errsev, errnum, errmsg);
	g_print("Error during funcinst execution:\n%s\n", errmsg);
      }
      else res = LIVES_RESULT_CANCELLED;
    }
  }

  if (!is_stacked) lives_sync_list_pop(&(LPT_DATA(finst, lj_stack)));

#if USE_RPMALLOC
  rpmalloc_thread_collect();
#endif

  if (res != LIVES_RESULT_SUCCESS) return res;
      g_print("RETLOC11\n");

  if (finst->retloc) {
      g_print("RETLOC222\n");
    // funcinsts which are not r
    if (weed_plant_has_leaf(finst->params, _RV_)) {
      //finst->flags |= FINST_FLAG_NOFREE_RETLOC;
      //copy_leaf_value(finst->params, _RV_, 0, fdef->return_type, &finst->retloc);
      weed_leaf_get(finst->params, _RV_, 0, finst->retloc);
      g_print("RETLOC444y\n"); 
   }
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
  funcsig_t sig;
  lives_funcinst_t *finst;
  lives_result_t res;
  char *msg;
  int nparms;

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

  g_print("func call is %s\n", lives_funcinst_show_func_call(finst));

  // STATE CHANGED - queued / preparing -> running
  lives_proc_thread_include_states(lpt, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_QUEUED | THRD_STATE_UNQUEUED | THRD_STATE_DEFERRED |
				   THRD_STATE_PREPARING);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_START_TICKS, lives_get_current_ticks());

  if (lpt == mainw->debug_ptr) g_print("nrefs PPPmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  res = do_call(finst);

  g_print("func call was %s\nres was %d\n", lives_funcinst_show_func_call(finst), res);
  
  if (lpt == mainw->debug_ptr) g_print("nrefss AAAAmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  lives_proc_thread_exclude_states(lpt, THRD_STATE_RUNNING);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS) {
    weed_set_int64_value(lpt, LIVES_LEAF_END_TICKS, lives_get_current_ticks());
  }

  if (res == LIVES_RESULT_INVALID) goto funcerr;

  if (res == LIVES_RESULT_SUCCESS || res == LIVES_RESULT_CANCELLED) {
    if (lpt == mainw->debug_ptr)
      g_print("pt a1\n");

    lives_proc_thread_unref(lpt);
    if (lpt == mainw->debug_ptr) g_print("nrefss nnnn = %d\n", lives_proc_thread_count_refs(lpt));

    if (lpt == mainw->debug_ptr)
      g_print("pt a122\n");
    return res == LIVES_RESULT_SUCCESS;
  }
  msg = lives_strdup_printf("Got error %d and res %d running procthread ", err, res);
  g_printerr("%s", msg);
  lives_freep((void **)&msg);
  goto funcerr2;

 funcerr:
  // invalid args_fmt
  sig = finst->funcdef->funcsig;
  nparms = get_funcsig_nparms(sig);
  msg = lives_strdup_printf("Unknown funcsig with type 0x%016lX (%lu), nparams = %d\n", sig, sig, nparms);
  if (prefs->show_dev_opts) {
#ifdef __FILE__
    char *filen = lives_strdup(__FILE__);
    get_filename(filen, TRUE);
#else
    filen = lives_strdup("functions");
#endif
    }

 funcerr2:
  lives_proc_thread_error(123, LPT_ERR_CRITICAL, "Got error %d running function %s with type "
			  "0x%016lX (%lu)", err, finst->funcdef->funcname,
			  finst->funcdef->funcsig, finst->funcdef->funcsig);
  lives_proc_thread_unref(lpt);
  return LIVES_RESULT_INVALID;
}


LIVES_GLOBAL_INLINE funcsig_t funcsig_from_args_fmt(const char *args_fmt) {
  funcsig_t fsig = 0;
  if (args_fmt) {
    char c;
    for (int i = 0; (c = args_fmt[i]); i++) {
      if (c == '*') continue;
      fsig <<= 4;
      fsig |= get_typecode(c);
    }
  }
  return fsig;
}



char *args_fmt_from_funcsig(funcsig_t sig) {
  char it[2];
  char *args_fmt;
  if (!sig) return lives_strdup("");
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


char *args_fmt_to_param_string(const char *args_fmt) {
  if (args_fmt && *args_fmt) {
    char *fmtstring = lives_strdup("");
    size_t arglen = lives_strlen(args_fmt);
    for (int i = 0; i < arglen; i++) {
      uint8_t ch = (uint8_t)args_fmt[i];
      if (!ch) continue;
      if (i == arglen - 1 && ch == '*')
	fmtstring = lives_strdup_concat(fmtstring, ", ", "%s", "...");
      else 
	fmtstring = lives_strdup_concat(fmtstring, ", ", "%s",
					weed_seed_to_ctype(get_seedtype(ch), FALSE));
   }
    return fmtstring;
  }
  return lives_strdup("void");
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


uint8_t symname_to_sigbits(const char *symname) {
  for (int j = 0; crossrefs[j].letter; j++) {
    if (!lives_strcmp(crossrefs[j].symname, symname)) {
      return crossrefs[j].sigbits;
    }
  }
  return 0;
}



funcsig_t short_params_to_funcsig(int nvals, const char **symnames) {
  funcsig_t fsig = 0;
  for (int i = 0; i < nvals; i++) {
    fsig <<= 4;
    fsig |= symname_to_sigbits(symnames[i]);
  }
  return fsig;
}


char *funcsig_to_symstring(funcsig_t sig) {
  // turn funcsig into symstring (same format as listed in funcsigs.h)
  char *fmtstring = lives_strdup("");
  if (sig) {
    for (int i = 60; i >= 0; i -= 4) {
      uint8_t ch = (sig >> i) & 0X0F;
      if (!ch) continue;
      fmtstring = lives_strdup_concat(fmtstring, "_", "%s", get_symbolname(ch));
    }
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


#if 0
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
#endif


LIVES_LOCAL_INLINE lives_result_t lives_cb_receipt_free(void *rcpt) {
  // in order to free a cb receipt, expired must be true,
  // in_list must be false. and adder must have been set
  if (!rcpt) return LIVES_RESULT_INVALID;

  boolean can_dstmut = FALSE;
  hook_cb_receipt *xrcpt = (hook_cb_receipt *)rcpt;
  pthread_mutex_t *xmutex = xrcpt->status_mutex;

  if (!pthread_mutex_trylock(xmutex))can_dstmut = TRUE;

  if (!RCPT_CAN_FREE(xrcpt->status)) {
    if (can_dstmut) pthread_mutex_unlock(xmutex);
    return LIVES_RESULT_FAIL;
  }
  if (can_dstmut) {
    pthread_mutex_unlock(xmutex);
    pthread_mutex_destroy(xmutex);
    lives_free(xmutex);
  }
  lives_free(rcpt);
  return LIVES_RESULT_SUCCESS;
}


void flush_cb_added_list(lives_proc_thread_t lpt, boolean all) {
  // if all is FALSE, flusj only newly added and expired
  // else flush all
  if (lpt) {
    LiVESList *cb_add_list = weed_get_voidptr_value(lpt, LIVES_LEAF_CB_ADDED_LIST, NULL), *list, *listnxt;

    for (list = cb_add_list; list; list = listnxt) {
      void *rcpt = list->data;
      listnxt = list->next;
      if (rcpt) {
	hook_cb_receipt *xrcpt = (hook_cb_receipt *)rcpt;
	// use of nrefs here allows us to flush receipts per
	// funcinst level. 'all' is set when a proc_thread is freed
	if (all || RCPT_SHOULD_REMOVE(xrcpt->status)
	    || lives_cb_receipt_get_nrefs(rcpt) == -1) {
	  lives_cb_receipt_remove_from_list(rcpt);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
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
    hstack->hsdesc = get_hs_desc(hstack->type);
    opflags = hstack->hsdesc->op_flags;
    HOOKSTACK_FLAGS_ADJUST(opflags);
  }
  return opflags;
}


LIVES_LOCAL_INLINE void expire_all_rcpts(lives_funcinst_t *finst) {
  LiVESList *list, *listnext;
  for (list = (LiVESList *)CL_DATA(finst, receipts); list; list = listnext) {
    hook_cb_receipt *xrcpt = (hook_cb_receipt *)list->data;
    pthread_mutex_t *xmutex = xrcpt->status_mutex;
    listnext = list->next;
    pthread_mutex_lock(xmutex);
    lives_cb_receipt_set_expired(list->data); 
    pthread_mutex_unlock(xmutex);
    pthread_mutex_destroy(xmutex);
 }
  lives_list_free((LiVESList *)CL_DATA(finst, receipts));
  CL_DATA(finst, receipts) = NULL;
}


static void remove_from_hstack(lives_hook_stack_t *hstack, LiVESList *list) {
  // should be called with hstack mutex LOCKED !
  // remove list from htack, free the closure
  lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
  hstack->stack = (volatile LiVESList *)lives_list_remove_node
    ((LiVESList *)hstack->stack, list, FALSE);
  expire_all_rcpts(finst);
  lives_funcinst_free(finst);
}


LIVES_GLOBAL_INLINE void lives_hook_stack_clear(lives_hook_stack_t **hstacks, int type) {
  if (hstacks) {
    lives_hook_stack_t *hstack = hstacks[type];
    pthread_mutex_t *hmutex = &(hstack->mutex);
    LiVESList *cblist, *cb_next;

    while (1) {
      // caution - as the stacks can stay triggered for a long time
      lives_microsleep_until_zero(hstack->flags & HS_FLAG_TRIGGERING);
      PTMLH;
      if (hstack->flags & HS_FLAG_TRIGGERING) PTMUH;
      else break;
    }

    if (hstack->stack) {
      if (type >= N_NATIVE_HOOKS) {
	uint64_t hs_op_flags = get_hs_op_flags(hstack);
	if ((hs_op_flags & HOOKSTACK_ASYNC)
	    && (hs_op_flags & HOOKSTACK_PARALLEL)) {
	  // if stack owner is self, we need to async_join
	  if (hstack->owner_act_src_type == ACTION_SOURCE_LPT) {
	    GET_PROC_THREAD_SELF(self);
	    if (hstack->owner.lpt == self) {
	      lives_hook_async_join(type);
	    }
	  }
	}
      }

      for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cb_next) {
	cb_next = cblist->next;
	remove_from_hstack(hstack, cblist);
      }
      lives_list_free((LiVESList *)hstack->stack);
      hstack->stack = NULL;
    }
    PTMUH;
  }
}


LIVES_GLOBAL_INLINE void lives_hook_stacks_clear_all(lives_hook_stack_t **hstacks, int ntypes) {
  if (hstacks)
    for (int i = 0; i < ntypes; i++) {
      if (hstacks[i]) {
	lives_hook_stack_clear(hstacks, i);
	pthread_mutex_destroy(&hstacks[i]->mutex);
	lives_free(hstacks[i]);
	hstacks[i] = NULL;
      }
    }
}


static void call_free_func(lives_funcinst_t *finst, int i, boolean do_exec) {
  // todo
  char *pkey = lives_strdup_printf("p%d_free", i);
  lives_funcinst_t *free_finst =
    (lives_funcinst_t *)weed_get_voidptr_value(finst->params, pkey, NULL);
  if (free_finst) {
    weed_leaf_delete(finst->params, pkey);
    lives_free(pkey);
    if (do_exec) {
      CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_RUNNING;
      lives_funcinst_execute(free_finst);
      g_print("EXEC cibtubdgfg $%s\n", lives_funcinst_show_func_call(free_finst));
      if (CONTINGENCY_DATA(free_finst, flags) & CONTINGENCY_BEHAVIOUR_READY_ON_IDLE) {
	CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_READY;
	return;
      }
      CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_IDLE;
      if (CONTINGENCY_DATA(free_finst, flags) & CONTINGENCY_BEHAVIOUR_NO_FREE_ON_IDLE)
	return;
    }
    else {
      CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_EXPIRED;
      if (CONTINGENCY_DATA(free_finst, flags) & CONTINGENCY_BEHAVIOUR_NO_FREE_ON_EXPIRED)
	return;
    }
    lives_funcinst_free(free_finst);
  }
  else lives_free(pkey);
}


boolean free_finst_paramdata(lives_funcinst_t *finst, boolean do_exec) {
  funcsig_t funcsig = funcsig_from_args_fmt(get_args_fmt(finst->params));
  int nparams = get_funcsig_nparms(funcsig);
  int nstdparams = 0;
  pthread_rwlock_rdlock(&finst->dispolock);
  if (finst->disposition == DISPOSITION_STACKED) {
    const hook_stack_descriptor_t *hsdesc;
    pthread_rwlock_unlock(&finst->dispolock);
    hsdesc = get_hs_desc((CL_DATA(finst, hook_stack))->type);
    nstdparams = get_funcsig_nparms(funcsig_from_args_fmt(hsdesc->def_args_fmt));
    for (int i = nstdparams; i < nparams; i++) call_free_func(finst, i, do_exec);
    return FALSE;
  }
  pthread_rwlock_unlock(&finst->dispolock);
  return TRUE;
}


static boolean unblock_waiter(void *receipt, void *data) {
  // if !paused, stop it from doing do
  int reply = lives_cb_receipt_get_req_reply(receipt);
  lives_proc_thread_t adder = lives_cb_receipt_get_adder(receipt);
  if (!adder) return FALSE;
  if (reply == LIVES_REPLY_YES || reply == LIVES_REPLY_NO) return FALSE;

  // setting state directly will work in every case - if adder is not paused it will
  // prevent it from pausing and it will clear the request
  
  lives_proc_thread_force_resume(adder);

  while (lives_proc_thread_is_paused(adder)) lives_millisleep;

  lives_hook_cb_remove(receipt);
  return FALSE;
}


// hook cb's and receipts

lives_proc_thread_t lives_cb_receipt_get_adder(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->adder : NULL;
}


int lives_cb_receipt_get_req_reply(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->req_reply : LIVES_REPLY_INVALID;
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
  return rcpt ? !!(rcpt->status & RCPT_EXPIRED) : FALSE;
}


boolean lives_cb_receipt_is_in_list(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? !!(rcpt->status & RCPT_IN_LIST) : FALSE;
}


void lives_cb_receipt_set_adder(void *receipt, lives_proc_thread_t adder) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    rcpt->adder = adder;
    rcpt->status |= RCPT_HAS_ADDER;
  }
}


void lives_cb_receipt_set_reply(void *receipt, int reply) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->req_reply = reply;
}


void lives_cb_receipt_set_in_list(void *receipt, boolean in_list) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    if (in_list) rcpt->status |= RCPT_IN_LIST;
    else rcpt->status &= ~RCPT_IN_LIST;
  }
}

void lives_cb_receipt_set_reply_callback(void *receipt, reply_sent_cb_f reply_sent_cb, void *user_data) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    rcpt->reply_cb = reply_sent_cb;
    rcpt->reply_cb_data = user_data;
  }
}


boolean lives_cb_receipt_has_reply_callback(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? !!rcpt->reply_cb : FALSE;  
}


boolean lives_cb_receipt_call_reply_callback(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt && rcpt->reply_cb)
    return (*rcpt->reply_cb)(receipt, rcpt->reply_cb_data);
  return FALSE;
}


LIVES_GLOBAL_INLINE void *lives_cb_receipt_new(void) {
  LIVES_CALLOC_TYPE(hook_cb_receipt, receipt, 1);
  receipt->status_mutex = LIVES_CALLOC_SIZEOF(pthread_mutex_t, 1);
  pthread_mutex_init(receipt->status_mutex, NULL);
  return receipt;
}


LIVES_GLOBAL_INLINE void *lives_cb_receipt_set_expired(void *receipt) {
  // this is called in two scenarios
  // - hook was triggered and the receipt has adder set, is no longer in a cb_add_list
  // and is not flagged as persistent
  // - hook stack is being cleared
  if (!receipt) return NULL;
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  rcpt->status |= RCPT_EXPIRED;
  rcpt->status &= ~RCPT_PERSISTENT;
  if (lives_cb_receipt_free(rcpt) == LIVES_RESULT_SUCCESS)
    receipt = NULL;
  return receipt;
}


static boolean check_if_can_remove(lives_funcinst_t *finst, LiVESList *list, boolean expire_extra) {
  // check funcinst receipt: if it has an adder and is not in a proc_thread cb_add_list
  // expire and free it
  // if no more receipts in funcinst, flag it for removal
  // (cleanup_funcinst_receipts does this for ALL receipts)

  void *rcpt = list->data;
  hook_cb_receipt *xrcpt = (hook_cb_receipt *)rcpt;
  pthread_mutex_t *xmutex = xrcpt->status_mutex;
  boolean can_extra = FALSE;

  pthread_mutex_lock(xmutex);

  // expire any receipts which are not in a cb_add_list, have an adder
  // and are not persistent
  // if expire_more is set, we can also expire non persistent receipts that are in a cb_add_list
  // provided they have no req_reply callback set
  if (expire_extra && !xrcpt->reply_cb) can_extra = TRUE;
  if (RCPT_SHOULD_EXPIRE(xrcpt->status, can_extra)) {
    if (!lives_cb_receipt_set_expired(rcpt)) {
      pthread_mutex_unlock(xmutex);
      pthread_mutex_destroy(xmutex);
      lives_free(xmutex);
    }
    else pthread_mutex_unlock(xmutex);

    CL_DATA(finst, receipts) =
      lives_list_remove_node((LiVESList *)CL_DATA(finst, receipts),
			     list, FALSE);

    if (!CL_DATA(finst, receipts))
      CL_DATA(finst, cb_flags) |= HOOK_STATUS_REMOVE;

    return TRUE;
  }
  pthread_mutex_unlock(xmutex);
  return FALSE;
}


void cleanup_self_receipts(void) {
  // remove / free any expired receipts from self cb_add_list
  GET_PROC_THREAD_SELF(self);
  flush_cb_added_list(self, FALSE);
}


LIVES_LOCAL_INLINE boolean cleanup_funcinst_receipts(lives_funcinst_t *finst, boolean force) {
  // this is for funcinsts which are used as hook callbacks.
  // expire / free any receipts which are no longer in a proc_thread cb_list, and not persistent
  // if there are no receipts for funcinst it will be flagged for removal and TRUE is returned
  LiVESList *rcpts, *rcpt_next;
  for (rcpts = (LiVESList *)CL_DATA(finst, receipts); rcpts; rcpts = rcpt_next) {
    rcpt_next = rcpts->next;
    check_if_can_remove(finst, rcpts, force);
  }
  return !CL_DATA(finst, receipts);
}


static void lives_funcinst_move_receipts(lives_funcinst_t *dst, lives_funcinst_t *src) {
  // filter out any receipts with no reply_cb (since the adder is not tracking)
  // and expire them (else they will never be removed)
  // these will be freed when the adder removes from cb_added_list
  // the rest will be appended to dst receipts
  // src should be freed after return
  LiVESList *rcpts;
  if (!src || !dst || src == dst) return;
  // set 
  cleanup_funcinst_receipts(src, TRUE);
  rcpts = (LiVESList *)CL_DATA(src, receipts);    
  CL_DATA(dst, receipts) = lives_list_concat((LiVESList *)CL_DATA(dst, receipts), (LiVESList *)rcpts);
  CL_DATA(src, receipts) = NULL;
}


void lives_funcinst_send_replies(lives_funcinst_t *finst, int reply) {
  LiVESList *retries = NULL;
  boolean retry = FALSE;
  do {
    for (LiVESList *list = (LiVESList *)CL_DATA(finst, receipts); list; list = list->next) {
      void *rcpt = list->data;

      if (retry) {
	if (!lives_list_find_by_data(retries, rcpt)) continue;
      }
      else {
	if (reply == lives_cb_receipt_get_req_reply(rcpt)) continue;	
	lives_cb_receipt_set_reply(rcpt, reply);
      }

      if (lives_cb_receipt_has_reply_callback(rcpt)) {
	boolean tryagain = lives_cb_receipt_call_reply_callback(rcpt);
	if (tryagain) {
	  if (!retry) retries = lives_list_append(retries, rcpt);
	}
	else {
	  if (retry) retries = lives_list_remove_data(retries, rcpt, FALSE);
	}
      }
    }
    retry = !!retries;
    if (retry) lives_millisleep;
  } while (retry);
}


static void expel_from_stack(lives_funcinst_t *expelled, lives_funcinst_t *expeller) {
  // blocking callbacks that already received YES should only continue on fulfilled
  cleanup_funcinst_receipts(expelled, TRUE);
  if (expeller && !(CL_DATA(expelled, cb_flags) & HOOK_STATUS_REMOVE))
    lives_funcinst_move_receipts(expeller, expelled);
}


void  lives_cb_receipt_add_to_list(void *receipt) {
  GET_PROC_THREAD_SELF(self);
  LiVESList *cb_add_list = weed_get_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, NULL);
  cb_add_list = lives_list_prepend(cb_add_list, receipt);
  weed_set_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, cb_add_list);
  lives_cb_receipt_set_in_list(receipt, TRUE);
}


lives_result_t lives_cb_receipt_remove_from_list(void *receipt) {
  // removes receipt from self cb_added_list
  // - if rexeipt is expired we also free receipt
  //
  // the next time the hook is triggered (or when the owner is freed)
  // receipts not in a list will be expired and freed
  // 
  LiVESList *cb_add_list;

  if (!receipt) return LIVES_RESULT_INVALID;
  GET_PROC_THREAD_SELF(self);

  hook_cb_receipt *xrcpt = (hook_cb_receipt *)receipt;
  pthread_mutex_t *xmutex = xrcpt->status_mutex;

  if (lives_cb_receipt_get_adder(receipt) != self) return LIVES_RESULT_NOPERM;
  if (!lives_cb_receipt_is_in_list(receipt)) return LIVES_RESULT_ERROR;

  cb_add_list = (LiVESList *)weed_get_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, NULL);
  if (!cb_add_list) return LIVES_RESULT_ERROR;

  if (!lives_list_check_remove_data(&cb_add_list, receipt, FALSE)) return LIVES_RESULT_ERROR;
  weed_set_voidptr_value(self, LIVES_LEAF_CB_ADDED_LIST, cb_add_list);

  pthread_mutex_lock(xmutex);

  // if the function is called directly, we force remove persistent cllbacks
  xrcpt->status &= ~HOOK_CB_PERSISTENT;

  lives_cb_receipt_set_in_list(receipt, FALSE);

  // if rcpt is expired, this will free it
  if (lives_cb_receipt_free(receipt) == LIVES_RESULT_SUCCESS) {
    pthread_mutex_unlock(xmutex);
    pthread_mutex_destroy(xmutex);
    lives_free(xmutex);
  }
  else pthread_mutex_unlock(xmutex);

  return LIVES_RESULT_SUCCESS;
}


LIVES_LOCAL_INLINE void lives_funcinst_add_receipt(lives_funcinst_t *finst, void *rcpt) {
  CL_DATA(finst, receipts) = lives_list_prepend((LiVESList *)CL_DATA(finst, receipts), rcpt);
}



uint64_t cbflags_for_hs_op_flags(uint64_t hs_op_flags) {
  uint64_t cbflags = 0;

  if (hs_op_flags & HOOKSTACK_PERSISTENT)
    cbflags |= HOOK_CB_PERSISTENT;
  if (hs_op_flags & HOOKSTACK_REMOVE_ON_FALSE)
    cbflags |= HOOK_OPT_REMOVE_ON_FALSE;
  if (hs_op_flags & HOOKSTACK_ALWAYS_ONESHOT)
    cbflags |= HOOK_OPT_ONESHOT;
  if (hs_op_flags & HOOKSTACK_GUI_THREAD)
    cbflags |= HOOK_CB_FG_THREAD;

  return cbflags;
}


void *lives_hook_cb_add(lives_hook_stack_t **hstacks, int type, lives_funcinst_t *finst, uint64_t addmode) {
  if (!finst) return NULL;

  lives_funcinst_t *xfinst = NULL, *ret_finst = NULL;
  lives_hook_stack_t *hstack;
  pthread_mutex_t *hmutex;
  void *receipt = NULL;
  LiVESList *cblist, *cblistnext;
  uint64_t cbflags, hs_op_flags, xflags, extra_cb_flags;

  boolean have_lock = FALSE;
  boolean is_self_stack = FALSE;
  boolean fmatch;
  boolean is_append = TRUE, is_remove = FALSE;

  int maxp;

  GET_PROC_THREAD_SELF(self);

  if (!(addmode & _ADDMODE_NORCPT)) receipt = lives_cb_receipt_new();

  if (!finst || !hstacks || type < 0 || type >= N_HOOK_POINTS) {
    if (receipt) lives_cb_receipt_set_reply(receipt, LIVES_REPLY_INVALID);
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

  // check that  in funcinst->params matches args_fmt for hook_stack
  // - if args_fmt ends with "*", we can append more params
  // - if args_fmt in inst contains '^', we replace this with 'V' and set param to funcinst

  const hook_stack_descriptor_t *hsdesc = get_hs_desc(hstack->type);
  if (hstack->hsdesc->def_args_fmt && !args_fmt_match(hsdesc->def_args_fmt,
						      get_args_fmt(finst->params))) {
    if (receipt) lives_funcinst_send_replies(xfinst, LIVES_REPLY_INVALID);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    return receipt;    
  }

  // check if ret_type matches
  /* if (hstack->hsdesc->ret_rtype */
  /*     && finst->funcdef->return_type != hstack->hsdesc->ret_rtype) { */
  /*   lives_funcinst_send_replies(finst, LIVES_REPLY_NO); */
  /*   if (finst) finst->flags |= FINST_FLAG_REJECTED; */
  /*   lives_cb_receipt_set_expired(receipt); */
  /*   return receipt; */
  /* } */

  if (addmode == ADDMODE_UPD_LINKED) is_remove = TRUE;

  finst->flags &= ~FINST_FLAG_REJECTED;

  if (receipt) lives_funcinst_add_receipt(finst, receipt);

  cbflags = CL_DATA(finst, cb_flags);
  xflags = cbflags & (HOOK_UNIQUE_REPLACE | HOOK_INVALIDATE_DATA | HOOK_TOGGLE_FUNC);

  if ((cbflags & HOOK_CB_PRIORITY) || (addmode & _ADDMODE_FORCE_PREPEND))
    is_append = FALSE;
  if (addmode & _ADDMODE_HAVE_LOCK) have_lock = TRUE;

  lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_STACKED);

  hs_op_flags = get_hs_op_flags(hstack);

  if (hstack->owner_act_src_type == ACTION_SOURCE_LPT && hstack->owner.lpt == self)
    is_self_stack = TRUE;

  if (!is_self_stack && !is_remove && (hs_op_flags & HOOKSTACK_SELF_ONLY)) {
    lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
    lives_cb_receipt_set_expired(receipt);
    if (finst) finst->flags |= FINST_FLAG_NOPERM;
    return receipt;  
  }

  extra_cb_flags = cbflags_for_hs_op_flags(hs_op_flags);
  cbflags |= extra_cb_flags;

  CL_DATA(finst, cb_flags) = cbflags;

  // if append, then everything else will check
  if (is_append) xflags &= ~(HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);

  //if (cbflags & HOOK_OPT_REMOVE_ON_FALSE)

#ifdef HAVE_COND_EVAL
  const hook_stack_descriptor_t *hsdesc = get_hs_desc(hstack->type);
  if ((hstack->hsdesc->accept_cond && !lives_cond_eval(hsdesc->accept_cond))
      || (hstack->reject_cond && lives_cond_eval(hstack->reject_cond))) {
    lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
    lives_cb_receipt_set_expired(receipt);
    if (finst) finst->flags |= FINST_FLAG_NOT_ACCEPTED;
    return receipt;  
  }
#endif

  // if prepending, nothing can block us
  if (!is_append) ret_finst = finst;
  hmutex = &hstack->mutex;
 
  if (!have_lock) {
    if (PTMTLH) {
      if (is_fg_thread())
	// it is possible for the main thread to be adding callbacks
	// at the same time as the target is triggering hooks
	// we need to keep servicing requests while waiting, since
	// the triggering thread might be waiting for the main thread to service
	// a callback
	LIVES_ACTIVE_WAIT_FOR(!PTMTLH);
      else PTMLH;
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
    if (xcbflags & (HOOK_STATUS_BLOCKED | HOOK_STATUS_IGNORE
		    | HOOK_STATUS_ACTIONED | HOOK_STATUS_RUNNING)) continue;

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
	  expel_from_stack(xfinst, ret_finst);
	  lives_funcinst_free(xfinst);
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
	hstack->stack = lives_list_append(cblist, (void *)finst);

      ret_finst = finst;
    }
    // unique func
    expel_from_stack(xfinst, ret_finst);
    lives_funcinst_free(xfinst);
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

  if (CL_DATA(finst, cb_flags) & HOOK_CB_PERSISTENT) {
    hook_cb_receipt *xrcpt = (hook_cb_receipt *)receipt;
    xrcpt->status |= RCPT_PERSISTENT;
  }

  lives_funcinst_send_replies(finst, LIVES_REPLY_YES);

  if (is_append){
    if (!ret_finst) hstack->stack = lives_list_append((LiVESList *)hstack->stack, finst);
  }
  else hstacks[type]->stack = lives_list_prepend((LiVESList *)hstack->stack, finst);

  CL_DATA(finst, hook_stack) = hstack;

  if (self) CL_DATA(finst, orig_adder) = self;

  if (!have_lock) PTMUH;

  if (lives_proc_thread_get_attrs(self) & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(self, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks());
  //}
  return receipt;  
}


void *lives_hook_cb_add_funcinst(lives_hook_stack_t **hooks, int type, uint64_t cbflags,
				 lives_funcinst_t *finst) {
  // funcinst may be blocked from adding for a variety of reasons
  // if superceded by another funcinst, we return the receipt, if not replaced, we return NULL
  // and in both cases, FINST_FLAG_REJECTED is set

  GET_PROC_THREAD_SELF(self);
  void *receipt;
  int reply;

  lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_STACKED);
  CL_DATA(finst, cb_flags) = cbflags;

  receipt = lives_hook_cb_add(hooks, type, finst, ADDMODE_NORMAL);

  cbflags = CL_DATA(finst, cb_flags);
  reply = lives_cb_receipt_get_req_reply(receipt);
  if (reply == LIVES_REPLY_YES) {
    lives_cb_receipt_add_to_list(receipt);
    if (cbflags & HOOK_CB_BLOCK)
      lives_cb_receipt_set_reply_callback(receipt, unblock_waiter, NULL);
  }

  lives_cb_receipt_set_adder(receipt, self);
  return receipt;
}


// returns receipt. or NULL if add condition failed, or it was toggled out
void *_lives_hook_cb_add_full(lives_hook_stack_t **hooks, int type, uint64_t cbflags, lives_funcptr_t func,
			      const char *fname, int return_type, const char **anames, const char *args_fmt, ...) {
  void *receipt;
  lives_funcinst_t *finst;
  if (args_fmt && *args_fmt) {
    va_list va;;
    va_start(va, args_fmt);
    finst = lives_funcinst_create_va(func ,fname, return_type, anames, args_fmt, va);

    if (cbflags & HOOK_CB_HAS_FREEFUNCS) {
      lives_hook_stack_t *hstack = hooks[type];
      int nparams = get_funcsig_nparms(funcsig_from_args_fmt(args_fmt));
      const hook_stack_descriptor_t *hsdesc = get_hs_desc(hstack->type);
      int nstdparams = get_funcsig_nparms(funcsig_from_args_fmt(hsdesc->def_args_fmt));
      for (int i = nstdparams; i < nparams;  i++) {
	// each param may have a funcinst to free its data. These funcinst are CONTINGENCIES,
	// which are triggered (in this case) by an alteration in the req_reply for the receipt
	// 
	lives_funcinst_t *free_finst = va_arg(va, lives_funcinst_t *);
	if (free_finst) {	
	  char *pkey = lives_strdup_printf("p%d_free", i);
	  lives_funcinst_set_disposition(free_finst, FALSE, DISPOSITION_CONTINGENCY);	
	  CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_READY;
	  weed_set_voidptr_value(finst->params, pkey, free_finst);
	  lives_free(pkey);
	}
      }
      va_end(va);
    }
  }
  else finst = lives_funcinst_create_va(func ,fname, return_type, NULL, NULL, NULL);

  if (finst) lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_STACKED);
  if (!hooks) hooks = THREADVAR(hook_stacks);

  receipt = lives_hook_cb_add(hooks, type, finst, ADDMODE_NORMAL);

  if (finst->flags & FINST_FLAG_REJECTED) lives_funcinst_free(finst);
  return receipt;
}


static lives_proc_thread_t update_linked_stacks(lives_funcinst_t *finst) {
  uint64_t dflags = ADDMODE_UPD_LINKED; //sets prepend, has lock, norcp;
  pthread_mutex_lock(&mainw->all_hstacks_mutex);
  if (!is_fg_thread()) {
    lives_hook_stack_t **mystacks = self_hook_stacks();
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
  LiVESList *list, *listnext;
  pthread_mutex_t *hmutex;
  LiVESList *rcpts;
  int reply = LIVES_REPLY_MU;
  boolean bret;
  boolean retval = TRUE;
  boolean have_recheck_mutex = FALSE;
  boolean hmulocked = FALSE;
  lives_hook_stack_t *req_stack = NULL;
  uint64_t hs_op_flags, cbflags;
  boolean rerun = FALSE;

  //if (type == SYNC_ANNOUNCE_HOOK) dump_hook_stack(hstacks, type);

  hstack = hstacks[type];
  if (hstack->flags & HS_FLAG_INVALID) return LIVES_RESULT_INVALID;

  // hs_op_flags come from the hook stack descriptor, and define the
  // operational details of the stack
  hs_op_flags = get_hs_op_flags(hstack);

  // should use async / async_parallel
  if (hs_op_flags & HOOKSTACK_ASYNC)
    return LIVES_RESULT_ERROR;

  if (!(hs_op_flags & HOOKSTACK_ANON_TRIGGER)) {
    // unless the op flags specify ANON_TRIGGERER, only the stack owner
    // may trigger the hook. In all other cases we return LIVES_RESULT_NOPERM
    GET_PROC_THREAD_SELF(self);
    if ((hstack->owner_act_src_type == ACTION_SOURCE_LPT
	 && hstack->owner.lpt != self)
	|| (hstack->owner_act_src_type == ACTION_SOURCE_THREAD
	    && hstack->owner.thread != pthread_self()))
      return LIVES_RESULT_NOPERM;
  }
  
  hmutex = &(hstack->mutex);

  if (type != FATAL_HOOK) {
    // skip mutex locking for the global FATAL hook
    PTMLH;
    hmulocked = TRUE;
  }

  if (!hstack->stack || (hstack->flags & HS_FLAG_TRIGGERING)) {
    if (type != FATAL_HOOK) PTMUH;
    hmulocked = FALSE;
    goto trigdone;
  }

  // flag the stack as triggering
  hstack->flags |= HS_FLAG_TRIGGERING;

  if (hstack->req_target_stacks) {
    // if the stack is a triage stack, instead of triggering, we forward to the target,
    // after making any predefined adjustments
    // make sure we get the mutex on target stack
    req_stack = hstack->req_target_stacks[hstack->req_target_type];
    if (!req_stack) {
      if (type != FATAL_HOOK) PTMUH;
      hmulocked = FALSE;
      goto trigdone;
    }
    pthread_mutex_lock(&req_stack->mutex);
  }

  list = (LiVESList *)hstack->stack;

  // mark all entries in list at entry as "ACTIONED"
  // since we may parse the list several times, we only check those which are present now
  // this avoids a situation where we would be endlessly traversing the list as new items are added
   for (; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;
    if (!finst) continue;
    cleanup_funcinst_receipts(finst, FALSE);
    cbflags = CL_DATA(finst, cb_flags);
    if ((cbflags & HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, list);
      continue;
    }
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_ACTIONED;
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

    for (list = (LiVESList *)hstack->stack; list; list = listnext) {
      lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
      listnext = list->next;
      if (!finst) continue;

      pthread_rwlock_rdlock(&finst->dispolock);
      if (finst->disposition != DISPOSITION_STACKED) {
	// if being run by another thread, it will have pushed CL_DATA module and added LPT_DATA
	pthread_rwlock_unlock(&finst->dispolock);
	continue;
      }

      pthread_rwlock_unlock(&finst->dispolock);
      
      cbflags = CL_DATA(finst, cb_flags);

      /* if (cbflags & HOOK_OPT_ADDER_RUNS) { */
      /* 	if (cbflags & HOOK_STATUS_RUNNING) { */
      /* 	  if (lives_proc_thread_get_active_funcinst(lpt) == finst) continue; */
      /* 	} */
      /* 	cbflags &= ~HOOK_STATUS_RUNNING; */
      /* 	CL_DATA(finst, triggerer.lpt) = ACTION_SOURCE_NONE; */
      /* } */
      
      if (cbflags & (HOOK_STATUS_BLOCKED | HOOK_CB_IGNORE)) {
	lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
	cbflags &= ~HOOK_STATUS_ACTIONED;
	CL_DATA(finst, cb_flags) = cbflags;
	continue;
      }

      if (cbflags & HOOK_STATUS_RUNNING) continue;

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
	int dflags = ADDMODE_TRANSFER;
	cbflags &= ~HOOK_STATUS_ACTIONED;
	cbflags &= ~hstack->req_target_unset_flags;
	cbflags |= hstack->req_target_set_flags;
	CL_DATA(finst, cb_flags) = cbflags;
	//lives_proc_thread_show_func_call(closure->proc_thread);
	lives_hook_cb_add(hstack->req_target_stacks, hstack->req_target_type,
			  finst, dflags);
	if (finst->flags & FINST_FLAG_REJECTED)	remove_from_hstack(hstack, list);
	else hstack->stack =
	       (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack,
							    list, FALSE);
	continue;
      }

      if (type >= N_NATIVE_HOOKS) {	
	GET_PROC_THREAD_SELF(self);
	CL_DATA(finst, triggerer.lpt) = self;
	CL_DATA(finst, trigger_act_src_type) = ACTION_SOURCE_LPT;
      }
      else {
	CL_DATA(finst, trigger_act_src_type) = ACTION_SOURCE_THREAD;
	CL_DATA(finst, triggerer.thread) = pthread_self();
      }
      if (type == LIVES_GUI_HOOK) {
	uint64_t xflags = cbflags & (HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);
	if (xflags) {
	  // we have a callback which is going to invalidate other hook cbs
	  // we want to get recheck mutex to ensure only one thread at a time does this
	  //
	  if (!have_recheck_mutex) {
	    if (pthread_mutex_trylock(&recheck_mutex)) {
	      // this means some other thread also has invalidate_data, we must let it run then recheck
	      lives_sleep_until_zero(pthread_mutex_trylock(&recheck_mutex));
	    }
	    have_recheck_mutex = TRUE;
	    rerun = TRUE;
	    break;
	  }
	  // narrow the flags down to just these 2
	  // and then apply this to all gui hook stacks actoss all threads
	  CL_DATA(finst, cb_flags) = xflags;
	  update_linked_stacks(finst);
	}
      }

      cbflags |= HOOK_STATUS_RUNNING;
      cbflags &= ~HOOK_STATUS_ACTIONED;
      CL_DATA(finst, cb_flags) = cbflags;

      // now we have 3 types of action:
      // most commonly self run - the owner lpt takes the funcinst from the hook stack
      // and pushes it as active_funcinst, the funcinst is exeuted. on return the prior funcinst
      // is popped. Status changes are reflected in receipt req_reply
      // next most common are funcinsts ditected at the gui thread
      // these are queued in a virtual queue and passed direct
      // finally - adder runs and async - with these, the funcinst is passed to another thread

      if (cbflags & HOOK_OPT_ADDER_RUNS) {
	lives_proc_thread_t adder = NULL;
	weed_plant_t *strucval = valplant_for_struct("lives_funcinst_t", finst);
	LiVESList *rcpts = (LiVESList *)CL_DATA(finst, receipts);
	if (rcpts) {
	  hook_cb_receipt *rcpt = (hook_cb_receipt *)rcpts->data;
	  if (rcpt) adder = rcpt->adder;
	}
	// TODO - if adder is already busy with another callback, just continue

	lives_proc_thread_try_interrupt(adder, strucval);
	CL_DATA(finst, cb_flags) = cbflags;
	continue;
      }

      if (type != FATAL_HOOK) PTMUH;
      hmulocked = FALSE;

      if (!(cbflags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
	// SELF RUN CALLBACK
	lives_funcinst_send_replies(finst, LIVES_REPLY_YES); 
	lives_funcinst_execute(finst);
      }
      else {
	// this function will call fg_service_call directly,
	// block until the lpt completes or is cancelled
	lives_funcinst_queue(finst, LIVES_THRDATTR_FG_THREAD);
      }

      CL_DATA(finst, triggerer.lpt) = ACTION_SOURCE_NONE;

      rcpts = (LiVESList *)CL_DATA(finst, receipts);
      if (rcpts) reply = lives_cb_receipt_get_req_reply(rcpts->data);

      if (cbflags & HOOK_OPT_ONESHOT) cbflags |= HOOK_STATUS_REMOVE;

      if (type != FATAL_HOOK) PTMLH;
      hmulocked = TRUE;

      cbflags &= ~HOOK_STATUS_RUNNING;
      cbflags &= ~HOOK_STATUS_ACTIONED;

      if (finst) {
	CL_DATA(finst, cb_flags) = cbflags;
	cleanup_funcinst_receipts(finst, FALSE);
      }
	
      if (cbflags & HOOK_STATUS_REMOVE) {
	if (reply == LIVES_REPLY_YES)
	  lives_funcinst_send_replies(finst, LIVES_REPLY_FULFILLED); 
	remove_from_hstack(hstack, list);
      }

      if (hs_op_flags & HOOKSTACK_RUN_SINGLE) {
	//g_print("done single\n");	
	break;
      }
    }

    if (!list) list = (LiVESList *)hstacks[type]->stack;

    if (!list) {
      if (!is_fg_thread() && type == LIVES_GUI_HOOK) {
	lives_microsleep_until_zero(pthread_mutex_trylock(&mainw->all_hstacks_mutex));
	mainw->all_hstacks =
	  lives_list_remove_data(mainw->all_hstacks, hstacks, FALSE);
	pthread_mutex_unlock(&mainw->all_hstacks_mutex);
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

  cleanup_self_receipts();

  //if (type == SYNC_WAIT_HOOK) g_print("sync all res: %d\n", retval);
  return retval ? LIVES_RESULT_SUCCESS : LIVES_RESULT_FAIL;
}


int lives_hook_trigger_async(int type, lives_proc_thread_t **xlpts) {
  LiVESList *list, *listnext;
  pthread_mutex_t *hmutex;
  lives_hook_stack_t *hstack;
  uint64_t hs_op_flags, cbflags;
  int ncount = 0;
  lives_proc_thread_t *lpts = NULL, lpt;

  if (xlpts) *xlpts = NULL;

  hstack = self_hook_stacks()[type];
  hmutex = &(hstack->mutex);
  PTMLH;

  if (!hstack->stack) {
    PTMUH;
    return ncount;
  }

  hs_op_flags = get_hs_op_flags(hstack);
 
  if (!((hs_op_flags & HOOKSTACK_ASYNC)
	&& (hs_op_flags & HOOKSTACK_PARALLEL))) {
    PTMUH;
    return ncount;
  }

  list = (LiVESList *)hstack->stack;

  for (; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;
    if (!finst) continue;
    cleanup_funcinst_receipts(finst, FALSE);
    cbflags = CL_DATA(finst, cb_flags);
    if (cbflags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstack, list);
      continue;
    }
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_ACTIONED;
  }

  list = (LiVESList *)hstack->stack;

  for (; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;

    if (!finst) continue;
    cbflags = CL_DATA(finst, cb_flags);
    if (!(cbflags & HOOK_STATUS_ACTIONED)) continue;

    cbflags &= ~HOOK_STATUS_ACTIONED;

    if ((cbflags & HOOK_STATUS_BLOCKED) || (cbflags & HOOK_STATUS_RUNNING)) continue;

    if (cbflags & (HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, list);
      continue;
    }

    hstack->flags |= HS_FLAG_TRIGGERING;
    cbflags |= HOOK_STATUS_RUNNING;
    CL_DATA(finst, cb_flags) = cbflags;

    // assuming FG_THREAD is not set, then this will wrap finst in a lpt
    // and dispatch it to pool thread

    lpt = lives_funcinst_queue(finst, LIVES_THRDATTR_FAST_QUEUE);
    if (xlpts) lives_dynarray_append(lpts, ncount, lpt);
    else ncount++;
  }
  PTMUH;
  if (xlpts) *xlpts = lpts;
  return ncount;
}


lives_result_t lives_proc_thread_trigger_hook(int type) {
  return lives_hook_trigger(self_hook_stacks(), type);
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


LIVES_GLOBAL_INLINE lives_result_t lives_hook_cb_invalidate(void *rcpt) {
  return lives_cb_receipt_remove_from_list(rcpt);
}


lives_result_t lives_hook_cb_block(lives_funcinst_t *finst) {
  if (finst) {
    pthread_rwlock_rdlock(&finst->dispolock);
    if (finst->disposition == DISPOSITION_STACKED) {
      pthread_rwlock_unlock(&finst->dispolock);
      GET_PROC_THREAD_SELF(self);
      if (self != CL_DATA(finst, orig_adder)) return LIVES_RESULT_NOPERM;
      CL_DATA(finst, cb_flags) |= HOOK_STATUS_IGNORE;
      return LIVES_RESULT_SUCCESS;
    }
    pthread_rwlock_unlock(&finst->dispolock);
  }
  return LIVES_RESULT_INVALID;
}


lives_result_t lives_hook_cb_unblock(lives_funcinst_t *finst) {
  if (finst) { 
    pthread_rwlock_rdlock(&finst->dispolock); 
    if (finst->disposition == DISPOSITION_STACKED) {
      pthread_rwlock_unlock(&finst->dispolock); 
      GET_PROC_THREAD_SELF(self);
      if (self != CL_DATA(finst, orig_adder)) return LIVES_RESULT_NOPERM;
      CL_DATA(finst, cb_flags) &= ~HOOK_STATUS_IGNORE;
      return LIVES_RESULT_SUCCESS;
    } 
    pthread_rwlock_unlock(&finst->dispolock); 
  }
  return LIVES_RESULT_INVALID;
}


static void finst_disposition_pop(lives_funcinst_t *finst) {
  funcinst_module_t *modt;
  void *old_module;
  int oldt;

  if (!finst || !finst->modules) return;
  pthread_rwlock_wrlock(&finst->dispolock);

  old_module = finst->module;
  oldt = finst->mod_type;

  modt = (funcinst_module_t *)lives_sync_list_pop(&finst->modules);
  if (modt) {
    finst->mod_type = modt->mod_type;
    finst->module = modt->module_data;
    finst->disposition = modt->disposition;
  }

  pthread_rwlock_unlock(&finst->dispolock);

  if (old_module) finst_module_free(old_module, oldt);
}


static void _lives_hook_async_join(int htype, boolean cancel) {
  pthread_mutex_t *hmutex;
  LiVESList *cblist, *cblist_next;
  lives_hook_stack_t *hstack;
  uint64_t hs_op_flags, cbflags;

  hstack = self_hook_stacks()[htype];

  if (!(hstack->flags & HS_FLAG_TRIGGERING)) return;

  hmutex = &(hstack->mutex);
  PTMLH;

  hs_op_flags = get_hs_op_flags(hstack);

  if (!(hs_op_flags & HOOKSTACK_ASYNC)) {
    PTMUH;
    return;
  }

  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist_next) {
    boolean remove = FALSE;
    lives_funcinst_t *finst = (lives_funcinst_t *)cblist->data;
    cblist_next = cblist->next;
    if (!finst) continue;

    // check disposition - this will be either:
    // stacked - not executed
    // waiting - still in queue
    // active - running
    // consumed - finished
    // cancelled or error

    pthread_rwlock_rdlock(&finst->dispolock); 

    if (finst->disposition != DISPOSITION_STACKED) { 
      lives_proc_thread_t lpt = LPT_DATA(finst, runner);
      if (finst->disposition == DISPOSITION_WAITING
	  || finst->disposition == DISPOSITION_ACTIVE)
	if (cancel) lives_proc_thread_request_cancel(lpt, FALSE);
	
      pthread_rwlock_unlock(&finst->dispolock); 
      lives_proc_thread_join_void(lpt);

      pthread_rwlock_rdlock(&finst->dispolock); 
      if (finst->disposition == DISPOSITION_CANCELLED) {
	pthread_rwlock_unlock(&finst->dispolock); 
	finst_disposition_pop(finst);
	lives_funcinst_send_replies(finst, LIVES_REPLY_CANCELLED);
	remove = TRUE;
      }
      else if (finst->disposition == DISPOSITION_ERROR) {
	pthread_rwlock_unlock(&finst->dispolock); 
	finst_disposition_pop(finst);
	lives_funcinst_send_replies(finst, LIVES_REPLY_ERROR);
	remove = TRUE;
      }
      else {
	// pop prior module (CL_DATA)
	pthread_rwlock_unlock(&finst->dispolock); 
	finst_disposition_pop(finst);
      }
    }
    else pthread_rwlock_unlock(&finst->dispolock); 

    cbflags = CL_DATA(finst, cb_flags);
    if (cbflags & HOOK_STATUS_BLOCKED) continue;
    if (cbflags & HOOK_STATUS_IGNORE) continue;

    cleanup_funcinst_receipts(finst, FALSE);

    if (remove || (cbflags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT))) {
      remove_from_hstack(hstack, cblist);
      lives_funcinst_free(finst);
      continue;
    }

    cbflags &= ~HOOK_STATUS_RUNNING;
    CL_DATA(finst, cb_flags) = cbflags;
  }
  hstack->flags &= ~HS_FLAG_TRIGGERING;
  PTMUH;
}


void lives_hook_async_join(int htype) {
  _lives_hook_async_join(htype, FALSE);
}


void lives_hook_async_cancel(int htype) {
  _lives_hook_async_join(htype, TRUE);
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


static boolean fn_match_child(lives_funcinst_t *finst1, lives_funcinst_t *finst2) {
  LiVESWidget *w, *C;
  char *pname;
  char *args_fmt = args_fmt_from_funcsig(finst1->funcdef->funcsig);
  if (!args_fmt || get_seedtype(args_fmt[0]) != WEED_SEED_VOIDPTR) {
    if (args_fmt) lives_free(args_fmt);
    return FALSE;
  }
  lives_free(args_fmt);
  pname = make_std_pname(0);
  C = (LiVESWidget *)(weed_get_voidptr_value(finst2->params, pname, NULL));
  if (!LIVES_IS_WIDGET(C) || !LIVES_IS_CONTAINER(C)) {
    lives_free(pname);
    return FALSE;
  }
  w = (LiVESWidget *)(weed_get_voidptr_value(finst1->params, pname, NULL));
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
    pthread_rwlock_init(&finst->dispolock, NULL);
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
    // should only free funcdef in its initial dispositon
    // or a variant for the module type

    if (finst->modules) return;
    
    if (finst->next) lives_funcinst_free(lives_sync_list_pop((lives_sync_list_t **)&finst->next));

    if (finst->flags & FINST_FLAG_STATIC) {
      finst->flags &= ~FINST_FLAG_REJECTED;
      return;
    }

    pthread_rwlock_rdlock(&finst->dispolock); 
    if (finst->disposition == DISPOSITION_STACKED) {
      // before we can free a funcinst with disposition stacked
      // we must ensure that all the receipts held by it are exired
      // - send an INVALID reply so any param data can be freed / blockers unblocked
      // if there is a sendt_reply callback, adder can 
       if (CL_DATA(finst, receipts)) {
	 lives_funcinst_send_replies(finst, LIVES_REPLY_INVALID);
	 if (CL_DATA(finst, receipts)) {	 
	   pthread_rwlock_unlock(&finst->dispolock); 
	   return;
	 }
       }
    }
    pthread_rwlock_unlock(&finst->dispolock); 

    // oh look, we cannot wrlock this now, i 
    //pthread_rwlock_wrlock(&finst->dispolock); 

    if (finst->retloc && !(finst->flags & FINST_FLAG_NOFREE_RETLOC)) lives_free(finst->retloc);

    if (finst->params) {
      free_finst_paramdata(finst, TRUE); 
      weed_plant_free(finst->params);
    }

    if (finst->funcdef) lives_funcdef_free(finst->funcdef);
    if (finst->module) finst_module_free(finst->module, finst->mod_type);

    /* pthread_rwlock_unlock(&finst->dispolock);  */
    pthread_rwlock_destroy(&finst->dispolock);
    
    lives_free(finst);
  }
}


//////////// proxy data / param shadowing
// st would normally be LIVES_SEED_PROXY
weed_error_t weed_leaf_bind_value(weed_plant_t *pl, const char *key, weed_seed_t st, void *locn, weed_size_t size) {
  // set a custom ptr to the address / size of a variable or blob data
  weed_error_t err = WEED_SUCCESS;
  if (!pl) return WEED_ERROR_NOSUCH_PLANT;
  if (weed_plant_has_leaf(pl, key)) {
    if (weed_leaf_is_immutable(pl, key)) return WEED_ERROR_IMMUTABLE;
    err = weed_leaf_delete(pl, key);
    if (err != WEED_SUCCESS) return err;
  }
  err  = weed_set_custom_value(pl, key, st, locn);
  if (err != WEED_SUCCESS) return err;
  weed_ext_set_element_size(pl, key, 0, size);
  return err;
}


lives_result_t lives_funcinst_bind_param(lives_funcinst_t *finst, int idx, void *locn, weed_size_t size) {  
  // what we do here is to make a second param, eg, for p0 we would make p0_proxy
  // in the second leaf we store a pointer to a variable, and the size
  // when the funcinst is actioned, we read the value dereferncing, e.g for WEED_SEED_INT
  // we would have
  // weed_set_int_value(finst->params, "p0", *(int *)weed_get_custom_value(finst->params, "p0_proxy", LIVES_SEED_PROXY. &err);
  // 
  if (!finst || idx < -1) return LIVES_RESULT_INVALID;

  lives_funcdef_t *fdef = finst->funcdef;
  if (!fdef || !fdef->function) return LIVES_RESULT_INVALID;
  int nparms = get_funcsig_nparms(fdef->funcsig);
  if (idx >= nparms) return LIVES_RESULT_ERROR;
 
  char *pkey = make_proxy_pname(idx);
  if (weed_leaf_bind_value(finst->params, pkey, LIVES_SEED_PROXY, locn, size) != WEED_SUCCESS) {
    lives_free(pkey);
    return LIVES_RESULT_FAILED;
  }
 
  lives_free(pkey);
  return LIVES_RESULT_SUCCESS;
}


lives_result_t update_params_from_proxies(lives_funcinst_t *finst) {
  if (!finst) return LIVES_RESULT_INVALID;
  lives_funcdef_t *fdef = finst->funcdef;
  if (!fdef || !fdef->function) return LIVES_RESULT_INVALID;
  int nparms = get_funcsig_nparms(fdef->funcsig);
  for (int i = 0; i < nparms; i++) {
    char *prkey = make_proxy_pname(i);
    if (weed_plant_has_leaf(finst->params, prkey)) {
      weed_error_t err;
      weed_seed_t st = nth_seed_type(fdef->funcsig, i);
      char *pkey = make_std_pname(i);
      weed_leaf_from_vap(finst->params, pkey, st,
			 weed_get_custom_value(finst->params, prkey, LIVES_SEED_PROXY, &err));
      lives_free(pkey);
      if (err != WEED_SUCCESS) return LIVES_RESULT_FAILED;
    }
    lives_free(prkey);
  }
  return LIVES_RESULT_SUCCESS;
}

#if 0
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
  if (!retloc && *retlocp) lives_freep((void **)retlocp);

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
  pdata = weed_get_custom_value(pl, key, LIVES_SEED_PROXY, 0, &err);
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
#endif

weed_error_t copy_leaf_value(weed_plant_t *pl, const char *key, int idx, weed_seed_t xst, void **retlocp) {
  // memcpy leaf value to *variable
  // for now we ignore arrays

  // xst is the "real" seed type for the data
  // this only matters if the real seed type is LIVES_SEED_BLOB_DATA
  // then the variable whose address is passed should be a void **.
  // (void *)(*var) will be allocated to the blob size, and the data copied into it
  
  if (!pl) return WEED_ERROR_NOSUCH_PLANT;
  if (!retlocp) return WEED_ERROR_NOSUCH_PLANT;

  weed_seed_t st = weed_leaf_seed_type(pl, key);
  if (st == WEED_SEED_INVALID) return WEED_ERROR_NOSUCH_LEAF;
  weed_size_t sz = weed_leaf_element_size(pl, key, 0);
  /* if (st == LIVES_SEED_PROXY) { */
  /*   // retloc is already a void ** */
  /*   if (xst == LIVES_SEED_BLOB_DATA) */
  /*     lives_proxy_data_get_value(pl, key, idx, *retlocp, &err); */
  /*   else lives_proxy_data_get_value(pl, key, idx, retlocp, &err); */
  /*   return err; */
  /* } */
  if (sz) {
    if (!*retlocp) *retlocp = lives_malloc(sz);
    return weed_leaf_get(pl, key, idx, *retlocp);
  }
  return WEED_SUCCESS;
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

