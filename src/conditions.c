// conditions.c
// (c) G. Finch 2024 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

///////////////////////////////////////////////

#include "main.h"
#include "diagnostics.h"
#include "functions.h"

static lives_condition cond_create_va(boolean full, LiVESList **va_magic);

#define is_final(p0) (!(p0) || (p0)->stype != LIVES_SEED_FUNCINST)
#define is_true(p0) (is_final(p0) && (p0)->values.b[0])
#define is_false(p0) (!is_true(p0))

static LiVESList *cond_trans_list = NULL;

#define MATCH_ANY		0
#define MATCH_NUMERIC		1
#define MATCH_INT		2
#define MATCH_BOOL		3
#define MATCH_POINTER		4
#define MATCH_STRING		5

static boolean check_st_match(allvalues_t *p0, allvalues_t *p1, int match) {
  if (p0 && p1 && p1->stype != p0->stype) return FALSE;
  switch (match) {
  case MATCH_ANY: return TRUE;
  case MATCH_NUMERIC:
    if (p0->stype == WEED_SEED_DOUBLE
	|| p0->stype == WEED_SEED_FLOAT) return TRUE;
  case MATCH_INT:
    if (p0->stype == WEED_SEED_INT
	|| p0->stype == WEED_SEED_UINT
	|| p0->stype == WEED_SEED_INT64
	|| p0->stype == WEED_SEED_UINT64) return TRUE;
    break;
  case MATCH_BOOL:
    if (p0->stype == WEED_SEED_BOOLEAN) return TRUE;
    break;
  case MATCH_POINTER:
    if (p0->stype == WEED_SEED_VOIDPTR
	|| p0->stype == WEED_SEED_FUNCPTR
	|| p0->stype == WEED_SEED_PLANTPTR) return TRUE;
    break;
  case MATCH_STRING:
    if (p0->stype == WEED_SEED_STRING
	|| p0->stype == LIVES_SEED_CONST_CHARPTR) return TRUE;
    break;
  default: break;
  }
  return FALSE;
}


static void  whirlything(LiVESList **va_magicbox, lives_funcinst_t *finst, int offs, ...) {
  // here we take a funcinst and turn the params into a va_surprise
  // we work backwards so that when consuming the values appear in the correct order
  // we call this recursively, then call final func, to ensure none of the va_lists go out of scope
  // when consuming (in check_va_surprises) we call va_end
  int maxparms = get_funcinst_nparams(finst);
  va_surprise *surprise = NULL;
  weed_seed_t st;
  char *pkey;

  // this is a little convoluted
  // offs is 0 to start, we leave surprise as NULL and get a val
  // then call back here, offs == 1 and we have val, NULL in a va_list

  if (offs) {
    // for all but bottom layers we wrap up va_list
    surprise = LIVES_CALLOC_SIZEOF(va_surprise, 1);
    va_start(surprise->va, offs);

    if (offs == maxparms) {
      // we artifically vreated a  new va_liat, now we prepend  it in va_magic
      // va_args are pulled first from the spliced in va_list until exhausted, when we go back to the original va_liat
      *va_magicbox = lives_list_prepend(*va_magicbox, surprise);
      return;
    }
  }

  offs++;

  pkey = make_std_pname(maxparms - offs);

  st = weed_leaf_seed_type(finst->params, pkey);

  switch (st) {
  case WEED_SEED_INT: {
    int ival = weed_get_int_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, ival, surprise);
    break;
  }
  case WEED_SEED_BOOLEAN: {
    boolean bval = weed_get_boolean_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, bval, surprise);
    break;
  }
  case WEED_SEED_INT64: {
    int64_t i64val = weed_get_int64_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, i64val, surprise);
    break;
  }
  case WEED_SEED_DOUBLE: {
    double dval = weed_get_double_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, dval, surprise);
    break;
  }
  case LIVES_SEED_CONST_CHARPTR: {
    const char *sval = weed_get_const_string_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, sval, surprise);
    break;
  }
  case WEED_SEED_VOIDPTR: {
    void *vval = weed_get_voidptr_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, vval, surprise);
    break;
  }
  case WEED_SEED_FUNCPTR: {
    weed_funcptr_t fval = weed_get_funcptr_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, fval, surprise);
    break;
  }
  case WEED_SEED_PLANTPTR: {
    weed_plant_t *pval = weed_get_plantptr_value(finst->params, pkey, NULL);
    whirlything(va_magicbox, finst, offs, pval, surprise);
    break;
  }
  }
  lives_free(pkey);
}


static void check_for_surprises(LiVESList **va_magic) {
  if ((*va_magic)->next) {
    // see if we can pull a rabbit from a hat
    va_surprise *hat = (va_surprise *)((*va_magic)->data);
    va_surprise *rabbit = va_arg(hat->va, va_surprise *);
    va_end(hat->va);
    lives_free(hat);
    // check if the rabbit is alive
    if (rabbit)(*va_magic)->data = (void *)rabbit;
    else {
      *va_magic = lives_list_remove_node(*va_magic, *va_magic, FALSE);
      LIVES_ASSERT(!((*va_magic)->next));
    }
  }
}

// value funcs

static allvalues_t *cond_allv_const(LiVESList **va_magic, weed_seed_t st) {
  va_surprise *magic = (va_surprise *)((*va_magic)->data);
  allvalues_t *allvp =  MAKE_ALLVALUE_VA(st, magic->va);
  check_for_surprises(va_magic);
  weed_plant_t *tmpplant = lives_plant_new(LIVES_PLANT_TMP);
  LEAF_FROM_ALLV(tmpplant, WEED_LEAF_VALUE, allvp);
  char *calstr = weed_leaf_stringify(tmpplant, WEED_LEAF_VALUE);
  weed_plant_free(tmpplant);

  d_print_debug(", %s", calstr);
  lives_free(calstr);
  return allvp;
}

static allvalues_t *cond_local_const(const char *item) {return get_local_book_item(item);}

// book vals are allvalues so we dont know type till we read them
static allvalues_t *cond_local_var(LiVESList **va_magic, const char *item) {
  if (!item) {
    va_surprise *magic = (*va_magic)->data;
    item = va_arg(magic->va, char *);
    check_for_surprises(va_magic);
    d_print_debug(", %s", item);
  }
  lives_funcinst_t *finst = lives_funcinst_create(cond_local_const, NULL, WEED_SEED_VOIDPTR, "s", item);
  return MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}

static allvalues_t *cond_global_const(const char *item) {return get_global_book_item(item);}

static allvalues_t *cond_global_var(LiVESList **va_magic) {
  va_surprise *magic = (*va_magic)->data;
  char *item = va_arg(magic->va, char *);
  check_for_surprises(va_magic);
  d_print_debug(", %s", item);
  lives_funcinst_t *finst = lives_funcinst_create(cond_global_const, NULL, WEED_SEED_VOIDPTR, "s", item);
  return  MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}


static allvalues_t *cond_bookval_const(const char *item) {
  if (item[0] == '$') return cond_local_const(item + 1);
  // assume global
  return cond_global_const(item + 1);
}

static allvalues_t *cond_book_value(LiVESList **va_magic) {
  va_surprise *magic = (*va_magic)->data;
  char *item = va_arg(magic->va, char *);
  check_for_surprises(va_magic);
  d_print_debug(", %s", item);
  lives_funcinst_t *finst = lives_funcinst_create(cond_bookval_const, NULL, WEED_SEED_VOIDPTR, "s", item);
  return  MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}


static allvalues_t *cond_objattr_const(const char *obj, const char *attr) {
  allvalues_t *allvp = cond_bookval_const(obj), *retavp = NULL;
  if (allvp) {
    if (allvp->stype == WEED_SEED_PLANTPTR) {
      weed_plant_t *obj;
      if (allvp->flags & ALLV_FLAG_POINTER)
        obj = *(allvp->values.P);
      else obj = allvp->values.P[0];
      retavp = allvalues_from_leaf(NULL, obj, attr);
    }
    allvalues_free(allvp);
  }
  return retavp;
}

static allvalues_t *cond_objattr(LiVESList **va_magic) {
  va_surprise *magic = (*va_magic)->data;
  char *obj = va_arg(magic->va, char *);
  check_for_surprises(va_magic);
  magic = (va_surprise *)(*va_magic)->data;
  char *attr = va_arg(magic->va, char *);
  check_for_surprises(va_magic);
  lives_funcinst_t *finst =  lives_funcinst_create(cond_objattr_const, NULL, WEED_SEED_VOIDPTR, "ss", obj, attr);
  return  MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}

// opfuncs

enum {OP_ADD, OP_SUB, OP_MULT,};

#define _COND_BOOL(a) !!(a)
#define _COND_ARITH(a,b,op)(op==OP_ADD?(a)+(b):op==OP_SUB?(a)-(b):(a)*(b))
#define _COND_GENERIC(a,b,op) _COND_BOOL(_COND_ARITH(a, b, op))

#define _COND_NOT(a) _COND_GENERIC(1,(a),OP_SUB)
#define _COND_OR(a,b) _COND_GENERIC((a),(b),OP_ADD)
#define _COND_AND(a,b) _COND_GENERIC((a),(b),OP_MULT)
#define _COND_XOR(a,b) _COND_GENERIC((a),(b),OP_SUB)


static allvalues_t *cond_equals_const(allvalues_t *p0, allvalues_t *p1) {
  boolean res = FALSE;
  if (!check_st_match(p0, p1, MATCH_ANY)) return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  switch (p0->stype) {
  case WEED_SEED_STRING:
  case LIVES_SEED_CONST_CHARPTR:
    res = !(lives_strcmp(p0->values.s[0], p1->values.s[0]));
    break;

  case WEED_SEED_INT: res = (p0->values.i[0] == p1->values.i[0]); break;
  case WEED_SEED_UINT: res = (p0->values.u[0] == p1->values.u[0]); break;
  case WEED_SEED_INT64: res = (p0->values.I[0] == p1->values.I[0]); break;
  case WEED_SEED_UINT64: res = (p0->values.U[0] == p1->values.U[0]); break;
  case WEED_SEED_DOUBLE: res = (p0->values.d[0] == p1->values.d[0]); break;
  case WEED_SEED_FLOAT: res = (p0->values.f[0] == p1->values.f[0]);  break;
  case WEED_SEED_VOIDPTR: res = (p0->values.V[0] == p1->values.V[0]); break;
  case WEED_SEED_FUNCPTR: res = (p0->values.F[0] == p1->values.F[0]); break;
  case WEED_SEED_PLANTPTR: res = (p0->values.P[0] == p1->values.P[0]); break;
  default: break;
  }
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, res);
}


static allvalues_t *cond_equals(LiVESList **va_magic, allvalues_t *p0) {
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_equals_const, NULL, WEED_SEED_VOIDPTR, "AA", p0, p1);
  allvalues_t *allvp;
  if (is_final(p0) && is_final(p1)) {
    // if we have only const values, we can find the result now
    // but we want to leave orignal values so that we can properly describe the condition
    // so we will simply transfer the funcinst from old to new, without changing the seed type of new
    // so now we have the result (for cond_evak, and the funcinst + params) for cond_desc
    allvp = cond_equals_const(p0, p1);
    allvp->funcinst = finst;
  } else allvp = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
  return allvp;
}

static allvalues_t *cond_greater_const(allvalues_t *p0, allvalues_t *p1) {
  boolean res = FALSE;
  if (!check_st_match(p0, p1, MATCH_NUMERIC)) return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  switch (p0->stype) {
  case WEED_SEED_INT: res = (p0->values.i[0] > p1->values.i[0]); break;
  case WEED_SEED_UINT: res = (p0->values.u[0] > p1->values.u[0]); break;
  case WEED_SEED_INT64: res = (p0->values.I[0] > p1->values.I[0]);  break;
  case WEED_SEED_UINT64: res = (p0->values.U[0] > p1->values.U[0]); break;
  case WEED_SEED_DOUBLE: res = (p0->values.d[0] > p1->values.d[0]); break;
  case WEED_SEED_FLOAT: res = (p0->values.f[0] > p1->values.f[0]); break;
  default: break;
  }
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, res);
}


static allvalues_t *cond_greater(LiVESList **va_magic, allvalues_t *p0) {
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_greater_const, NULL, WEED_SEED_VOIDPTR, "AA", p0, p1);
  allvalues_t *allvp;
  if (is_final(p0) && is_final(p1)) {
    allvp = cond_greater_const(p0, p1);
    allvp->funcinst = finst;
  } else allvp = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
  return allvp;
}

static allvalues_t *cond_bit_set_const(allvalues_t *p0, allvalues_t *p1) {
  boolean res = FALSE;
  if (!check_st_match(p0, p1, MATCH_INT)) return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  switch (p0->stype) {
  case WEED_SEED_INT: res = (p0->values.i[0] & p1->values.i[0]); break;
  case WEED_SEED_UINT: res = (p0->values.u[0] & p1->values.u[0]); break;
  case WEED_SEED_INT64: res = (p0->values.I[0] & p1->values.I[0]); break;
  case WEED_SEED_UINT64: res = (p0->values.U[0] & p1->values.U[0]); break;
  default: break;
  }
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, res);
}

static allvalues_t *cond_bit_set(LiVESList **va_magic, allvalues_t *p0) {
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_bit_set_const, NULL, WEED_SEED_VOIDPTR, "AA", p0, p1);
  allvalues_t *allvp;
  if (is_final(p0) && is_final(p1)) {
    allvp = cond_bit_set_const(p0, p1);
    allvp->funcinst = finst;
  } else allvp = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
  return allvp;
}


static boolean get_bool(allvalues_t *allvp) {
  if (allvp) {
    switch (allvp->stype) {
    case WEED_SEED_BOOLEAN: return allvp->values.b[0];
    case WEED_SEED_INT: return !!allvp->values.i[0];
    case WEED_SEED_UINT: return !!allvp->values.u[0];
    case WEED_SEED_INT64: return !!allvp->values.I[0];
    case WEED_SEED_UINT64: return !!allvp->values.U[0];
    case WEED_SEED_DOUBLE: return !!allvp->values.d[0];
    case WEED_SEED_FLOAT: return !!allvp->values.f[0];
    default: break;
    }
  }
  return FALSE;
}

static allvalues_t *cond_logic_not_const(allvalues_t *p0) {
  if (!check_st_match(p0, NULL, MATCH_NUMERIC)
      && !check_st_match(p0, NULL, MATCH_BOOL))
    return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, _COND_NOT(get_bool(p0)));
}


static allvalues_t *cond_logic_not(LiVESList **va_magic) {
  allvalues_t *p0 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_logic_not_const, NULL, WEED_SEED_VOIDPTR, "A", p0);
  allvalues_t *allvp;
  if (is_final(p0)) {
    // if we have only const values, we caan find the result now
    // but we want to leave orignal values so that we can properly describe the condition
    // so we will simply transfer the funcinst from old to new, without changing the seed type of new
    // so now we have the result (for cond_evak, and the funcinst + params) for cond_desc
    allvp = cond_logic_not_const(p0);
    allvp->funcinst = finst;
  } else allvp = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
  return allvp;
}


static allvalues_t *cond_seed_type_const(allvalues_t *p0) {
  return MAKE_ALLVALUE(WEED_SEED_INT, p0->stype);
}


static allvalues_t *cond_seed_type(LiVESList **va_magic) {
  allvalues_t *p0 = cond_create_va(FALSE, va_magic);
  if (is_final(p0)) return cond_seed_type_const(p0);
  lives_funcinst_t *finst = lives_funcinst_create(cond_seed_type_const, NULL, WEED_SEED_VOIDPTR, "A", p0);
  return MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}


static allvalues_t *cond_logic_and_const(allvalues_t *p0, allvalues_t *p1) {
  if (!check_st_match(p0, p1, MATCH_ANY)) return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, _COND_AND(get_bool(p0), get_bool(p1)));
}

static allvalues_t *cond_logic_and(LiVESList **va_magic, allvalues_t *p0) {
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_logic_and_const, NULL, WEED_SEED_VOIDPTR, "AA", p0, p1);
  allvalues_t *allvp;
  if (is_final(p0)) {
    if (is_final(p1)) {
      allvp = cond_logic_and_const(p0, p1);
      allvp->funcinst = finst;
      return allvp;
    }
    if (is_false(p0)) {
      allvp = cond_logic_and_const(p0, p0);
      allvp->funcinst = finst;
      return allvp;
    }
  }
  if (is_false(p1)) {
    allvp = cond_logic_and_const(p1, p1);
    allvp->funcinst = finst;
    return allvp;
  }
  return MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}

#if 0
static allvalues_t *cond_logic_test(LiVESList **va_magic, allvalues_t *p0) {
  // this is followed by a return_type, function, aargs_fmt, then the params.
  // 0 is a spevial value meaning p0 and 1 means p1
  // 0, 1 do not follow the args fmt, but any other params must, eg COND_INT_CONST
  // if the final function will be called with actual values unless the args fmt symbol is A
  // then the allvalues_is passed, p0 and p1 are always passed as boolean
  va_surprise *magic = (*va_magic)->data;
  weed_seed_t rval = va_arg(magic->va, weed_seed_t);
  check_for_surprises(va_magic);
  weed_function_t func = va_arg(magic->va, weed_function_t);
  check_for_surprises(va_magic);
  const char *args_fmt = va_arg(magic->va, const_char *);
  check_for_surprises(va_magic)
  //  remove 0 and 1, but note positions
  if (args__fmt && *args_fmt); {
    xargs_fmt = LSPF("_(%s)", args_fmt);
  }

  // now we want to parse a sequacne of 'C" / 'V'

  lives_funcinst_t *finst = lives_funcinst_create_va(func, NULL, rval, xargs_fmt, magic->va);
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  allvalues_t *allvp;
  // check all parms
  if (is_final(p0) && is_final(p1)) {
    allvp = cond_logic_and_const(p0, p1);
    allvp->funcinst = finst;
  } else allvp = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
  return allvp;
}
#endif

static allvalues_t *cond_logic_or_const(allvalues_t *p0, allvalues_t *p1) {
  if (!check_st_match(p0, p1, MATCH_ANY)) return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, _COND_OR(get_bool(p0), get_bool(p1)));
}

static allvalues_t *cond_logic_or(LiVESList **va_magic, allvalues_t *p0) {
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_logic_or_const, NULL, WEED_SEED_VOIDPTR, "AA", p0, p1);
  allvalues_t *allvp;
  if (is_final(p0)) {
    if (is_final(p1)) {
      allvp = cond_logic_or_const(p0, p1);
      allvp->funcinst = finst;
      return allvp;
    }
    if (is_true(p0)) {
      allvp = cond_logic_or_const(p0, p1);
      allvp->funcinst = finst;
      return allvp;
    }
  }
  if (is_true(p1)) {
    allvp = cond_logic_or_const(p1, p1);
    allvp->funcinst = finst;
    return allvp;
  }

  return MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
}

static allvalues_t *cond_logic_xor_const(allvalues_t *p0, allvalues_t *p1) {
  if (!check_st_match(p0, p1, MATCH_ANY)) return MAKE_ALLVALUE(WEED_SEED_INVALID, NULL);
  return MAKE_ALLVALUE(WEED_SEED_BOOLEAN, _COND_XOR(get_bool(p0), get_bool(p1)));
}

static allvalues_t *cond_logic_xor(LiVESList **va_magic, allvalues_t *p0) {
  allvalues_t *p1 = cond_create_va(FALSE, va_magic);
  lives_funcinst_t *finst = lives_funcinst_create(cond_logic_xor_const, NULL, WEED_SEED_VOIDPTR, "AA", p0, p1);
  allvalues_t *allvp;
  if (is_final(p0) && is_final(p1)) {
    allvp = cond_logic_xor_const(p0, p1);
    allvp->funcinst = finst;
  } else allvp = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst);
  return allvp;
}

/////////////////////////////////

static cond_trans *find_ctrans(char *tok) {
  for (LiVESList *list = cond_trans_list; list; list = list->next) {
    cond_trans *ctrans = (cond_trans *)list->data;
    if (!lives_strcmp(tok, ctrans->token)) return ctrans;
  }
  return NULL;
}


static lives_condition cond_create_va(boolean full, LiVESList **va_magic) {
  // here we compile a lives_condition from a va_list of tokens and values
  // we acually evaluate quite a lot in this function, const values are resolved
  // and we also resolve any opduncs which act only on const values.
  // The general method is - look up each token in the translation table
  // for "COND_POPEN",we do a recursive call until we reach "COND_PCLOSE" - this is a full create pass
  // for alias 'X' tokens, there is no function to run, but the funcinst params are effectively inserted
  // in the valist.
  // for all other tokens we have a function to call and some parameters
  // the first param is a voidptr set to null, this is reserved so we can set it to
  // point to a list containing a pointer to a struct cuntaining the initial va_list
  // we can prepend a second struct here which can contain reccursive levels for an aliased token
  // for tokens of type 'O' we also pass a second void * value, the condition to this point
  // which becomes p0 for the opfunc, the function will pull its remaining params from the va_list
  // using partial create - this is becaise we dont know how many additional params it might take
  // 'C; type tokens call a function which returns a const value
  // 'V' type tokens return a funcinst which we can execute to obtain the value
  // all inputs and outputs are passed as allvalues_t *, so the same struct can be used for final values
  // and for funcinsts, and we can ignor actual vatiable types until the moment we apply an opfunc
  // the operation is carried out on the typed values, and the result returned as another allvalues_t *
  //
  // the final return - lives_condition is actually a typedef of allvalues_t *
  // in non-trivial cases, this would hold a funcinst, and when evaluating this,
  // we would find that some parameters are also funcinsts which we then evaluate depth first recursively
  // and replacing with final values once we reach leaf nodes (const values). returning back along the tree,
  // we now apply the opfuncs to the constant values, until we reach the root.
  char *pkey, *tok;
  int state = 0, tcount = 0;
  lives_condition condition = NULL;
  LiVESList *cond_syntax = NULL, *synsym;
  va_surprise *magic = (va_surprise *)((*va_magic)->data);

  if (!full) state = 2;

  while (1) {
    // this works like a state machine, for each state we have an array of
    // allowed tokens + token types. Reading next token may change the state
    int maxp;
    magic = (va_surprise *)((*va_magic)->data);
    LIVES_ASSERT(magic);
    tok = va_arg(magic->va, char *);

    if (!(*va_magic)->next && lives_strcmp(tok, _COND_PCLOSE)) {
      if (!full || tcount) d_print_debug(", ");
      d_print_debug("\"%s\"", tok);
    }
    check_for_surprises(va_magic);

    cond_trans *ctrans = find_ctrans(tok);
    lives_funcinst_t *xfinst;

    if (!ctrans) goto fmt_err;

    if (ctrans->fmt == 'X') {
      // for type 'X' we want to insert substitute values, but we cannot insert in va_list
      // so we make a va_magicbox and prepend this to the list
      // lets call whirlything - it will take the params from funcinst,
      // processesing them in reverse order, building a nice va_surprise
      // gift us with a va_surprise, then call here recursively
      // when we reach the bottom of the va_surprise, because we had return on magic end set
      // we return here, after parsing all substituted tokens, and we continue with next magicbox in the va_list gun
      // so let us just break adn continue as if nothing happened
      // from now on we, must not exit this function until the overlaid va_lits is exhausted
      whirlything(va_magic, ctrans->funcinst, 0);
      continue;
    }

    if (state == 0) cond_syntax = strings_to_list(COND_SYNTAX_0);
    if (state == 1) cond_syntax = strings_to_list(COND_SYNTAX_1);
    if (state == 2) cond_syntax = strings_to_list(COND_SYNTAX_2);

    for (synsym = cond_syntax; synsym; synsym = synsym->next) {
      char *syntok = (char *)synsym->data;
      if (!syntok[1]) {
        if (ctrans->fmt == (char)((syntok[0]))) break;
      } else {
        if (!lives_strcmp(tok, syntok)) break;
      }
    }

    if (!synsym) goto fmt_err;
    lives_list_free(cond_syntax);
    cond_syntax = NULL;

    if (ctrans->fmt == 'C' || ctrans->fmt == 'V') state = 1;

    if (!lives_strcmp(tok, _COND_POPEN)) {
      // if first symbol is popen, and this is partial, convert to full
      if (!tcount && !full) {
        full = TRUE;
        state = 0;
      } else {
        if (tcount) {
          condition->priv_data = (void *)ctrans;
          condition = cond_create_va(TRUE, va_magic);
          state = 1;
        }
      }
      tcount++;
      continue;
    }

    if (!lives_strcmp(tok, _COND_PCLOSE)) {
      // want to mark this a parens for describe
      // so we make a new allvalues_t * with type finst
      // new finst with no func, one para, and priv_dara
      /* lives_funcinst_t *finst = lives_funcinst_create(NULL, "parens", WEED_SEED_VOID, "A", condition); */
      /* condition = MAKE_ALLVALUE(LIVES_SEED_FUNCINST, finst); */
      /* condition->priv_data = find_ctrans(_COND_POPEN); */
      if (!(*va_magic)->next) d_print_debug("\"%s\"\n", _COND_PCLOSE);
      return condition;
    }

    tcount++;

    switch (ctrans->fmt) {
    case 'C':
    case 'V':
      xfinst = lives_funcinst_copy(ctrans->funcinst);
      SET_RETVAR(xfinst, condition);
      // set last param to magic
      pkey = make_std_pname(0);
      weed_set_voidptr_value(xfinst->params, pkey, va_magic);
      maxp = get_funcinst_nparams(xfinst);
      if (!maxp) weed_set_int_value(xfinst->params, LIVES_LEAF_MAXPARAM, 1);
      lives_free(pkey);
      do_call(xfinst);
      lives_funcinst_free(xfinst);
      if (!condition) goto fmt_err;
      condition->priv_data = (void *)ctrans;
      if (!full) return condition;
      break;

    case 'O':
      xfinst = lives_funcinst_copy(ctrans->funcinst);
      SET_RETVAR(xfinst, condition);
      // va_list with alias overlaid
      pkey = make_std_pname(0);
      weed_set_voidptr_value(xfinst->params, pkey, va_magic);
      lives_free(pkey);
      // p0 for opfuncs
      pkey = make_std_pname(1);
      weed_set_custom_value(xfinst->params, pkey, LIVES_SEED_ALLVALUES, condition);
      lives_free(pkey);
      maxp = get_funcinst_nparams(xfinst);
      if (maxp < 2) weed_set_int_value(xfinst->params, LIVES_LEAF_MAXPARAM, 2);
      do_call(xfinst);
      lives_funcinst_free(xfinst);
      if (!condition) goto fmt_err;
      condition->priv_data = (void *)ctrans;
      state = 1;

      // should never happen, because if !full. syntax check should bar this
      // but anyway
      if (!full) return condition;
      break;

    case 'U':
      xfinst = lives_funcinst_copy(ctrans->funcinst);
      SET_RETVAR(xfinst, condition);
      pkey = make_std_pname(0);
      weed_set_voidptr_value(xfinst->params, pkey, va_magic);
      maxp = get_funcinst_nparams(xfinst);
      if (!maxp) weed_set_int_value(xfinst->params, LIVES_LEAF_MAXPARAM, 1);
      do_call(xfinst);
      lives_funcinst_free(xfinst);
      if (!condition) goto fmt_err;
      condition->priv_data = (void *)ctrans;
      state = 1;
      break;
    default: break;
    }
  }

  return condition;

fmt_err: {
    char *tmp = LSPF("format error in lives_make_cond: %s\n", tok);
    LIVES_WARN(tmp);
    lives_free(tmp);
  }
  return NULL;
}


lives_condition  _lives_cond_create(const char *cond_start, ...) {
  lives_condition condition;
  LIVES_CALLOC_TYPE(LiVESList *, va_magicbox, 1);
  LIVES_CALLOC_TYPE(va_surprise, magic, 1);
  d_print_debug("\n\nCompilinhg condition using: lives_cond_create(");
  va_start(magic->va, cond_start);
  *va_magicbox = lives_list_append(*va_magicbox, magic);
  condition = cond_create_va(TRUE, va_magicbox);
  va_end(magic->va);
  lives_free(magic);
  lives_list_free(*va_magicbox);
  d_print_debug(");\nDone\n");
  return condition;
}


void lives_cond_free(lives_condition cond) {
  // this is really just an alias, but for quality:
  allvalues_t *allvp = (allvalues_t *)cond, *xallvp;
  if (!allvp->funcinst) {
    allvalues_free(allvp);
    return;
  }
  // free funcinst params first, then funcinst itself, then finally allvalues_t
  char *pkey;
  lives_funcinst_t *finst = allvp->funcinst;
  int maxparms = get_funcinst_nparams(finst);
  for (int i = 0; i  < maxparms; i++) {
    pkey = make_std_pname(i);
    if (weed_leaf_seed_type(finst->params, pkey) == LIVES_SEED_ALLVALUES) {
      xallvp = (allvalues_t *)weed_get_custom_value(finst->params, pkey, LIVES_SEED_ALLVALUES, NULL);
      lives_cond_free(xallvp);
    }
    lives_free(pkey);
  }
  lives_funcinst_free(finst);
  allvalues_free(allvp);
}


void _register_cond_token(const char *token, char fmt, ...) {
  // regisetr a token for a syntax, in his cas cond stax
  // when registering functions we do not register the final function to be called
  // in create / eval, insyead we register a function that createhs the real function call
  // during compilation of the condsting
  // this can thought of as a pre-function
  // params for the pre function can be supplied hwen registering, or can be pulled from the tape
  // when compiling
  //
  // Now what we also want to do is send the tape to the pre function as first param, in effect we want to prepend to the va_list
  // so what we do is take the args_ftm and va)list and create a va_surprise. Since we want first param at lowest level we create it in forwards order
  // then unroll itto get the last first and build a second va_surprise

  //

  va_list ap;
  weed_funcptr_t function;
  lives_funcdef_t *fdef;
  char *args_fmt, *xargs_fmt;

  LIVES_CALLOC_TYPE(cond_trans, ctrans, 1);
  va_start(ap, fmt);

  ctrans->fmt = fmt;
  switch (fmt) {
  case '!':
    ctrans->token = LSPF(COND_PFX "%s", token);
    ctrans->desc = lives_strdup(va_arg(ap, char *));
    break;

  case 'X':
    ctrans->token = lives_strdup(token);
    // make an anon funcinst to store va_args
    // when parsing condstring, we read then convert params to a va_surprise
    args_fmt = va_arg(ap, char *);
    ctrans->funcinst = lives_funcinst_create_va(NULL, WEED_SEED_VOID, args_fmt, ap);
    break;

  case 'C': case 'V':
    ctrans->token = LSPF(COND_PFX "%s", token);
    ctrans->desc = lives_strdup(va_arg(ap, char *));
    function = va_arg(ap, lives_funcptr_t);
    ctrans->funcname = lives_strdup(va_arg(ap, char *));
    // add param for magic surprise box
    args_fmt = va_arg(ap, char *);
    // we wnt to leave a gap for the 1st param, because this is where will pass the tape
    // so we can do this in 2 steps - make a funcdef with the real args_fmt - so we can check the
    // fmt. Then we will create the funcinst using the funcdef, but using a gap marker
    // we will create the type from fdef, but not set the value yet

    xargs_fmt = LSPF("V%s", args_fmt ? args_fmt : "");
    fdef = lives_funcdef_create(ctrans->funcname, function, WEED_SEED_VOIDPTR, xargs_fmt);
    lives_free(xargs_fmt);
    xargs_fmt = LSPF("_%s", args_fmt ? args_fmt : "");
    ctrans->funcinst = lives_funcinst_create_for_funcdef(fdef, xargs_fmt, ap);
    lives_free(xargs_fmt);
    break;

  case 'O': case 'U':
    ctrans->token = LSPF(COND_PFX "%s", token);
    ctrans->desc = lives_strdup(va_arg(ap, char *));
    function = va_arg(ap, lives_funcptr_t);
    ctrans->funcname = lives_strdup(va_arg(ap, char *));
    // we will set params in create
    if (fmt == 'O')
      ctrans->funcinst = lives_funcinst_create(function, ctrans->funcname, WEED_SEED_VOIDPTR, NULL);
    else
      ctrans->funcinst = lives_funcinst_create(function, ctrans->funcname, WEED_SEED_VOIDPTR, NULL);
    break;
  default: break;
  }
  va_end(ap);

  d_print_debug("registering cond token %s, type %c", ctrans->token, fmt);
  if (ctrans->funcname) d_print_debug("   -->  %s", ctrans->funcname);
  d_print_debug("\n");
  cond_trans_list = lives_list_prepend(cond_trans_list, (void *)ctrans);
}


lives_condition lives_cond_copy(lives_condition condition) {
  // recursively copy current level, if we have a funcinst, recursively copy each paran
  lives_condition xcond = NULL;
  if (condition) {
    // copy toplevel
    xcond = allvalues_copy(condition);

    if (!is_final(xcond)) {
      // we only copy the actual evaluation part of the tree
      // this means lives_cond_desc() will only work on an original lives_condition returned from lives_cond_create()
      // but not on copies made with lives_cond_copy()
      // copy funcinst - this makes an exact copy with cloned params
      lives_funcinst *finst = lives_funcinst_copy(condition->funcinst);
      xcond->funcinst = finst;
      //
      int maxparms = get_funcinst_nparams(finst);
      for (int i = 0; i  < maxparms; i++) {
        char *pkey = make_std_pname(0);
        if (weed_leaf_seed_type(finst->params, pkey) == LIVES_SEED_ALLVALUES) {
          allvalues_t *allvp = (allvalues_t *)weed_get_custom_value(finst->params, pkey, LIVES_SEED_ALLVALUES, NULL);
          if (allvp) {
            if (allvp->funcinst)
              // call recursively
              weed_set_custom_value(finst->params, pkey, LIVES_SEED_ALLVALUES, lives_cond_copy(allvp));
            else weed_set_custom_value(finst->params, pkey, LIVES_SEED_ALLVALUES, allvalues_copy(allvp));
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*
  return xcond;
}


static lives_condition _lives_cond_eval(lives_condition condition) {
  if (!condition) return NULL;
  if (is_final(condition)) return condition;
  lives_funcinst *finst = condition->funcinst;
  int maxparms = get_funcinst_nparams(finst);
  for (int i = 0; i < maxparms; i++) {
    char *pkey = make_std_pname(i);
    allvalues_t *allvp = (allvalues_t *)weed_get_custom_value(finst->params,
                         pkey, LIVES_SEED_ALLVALUES, NULL);
    if (allvp->funcinst)
      weed_set_custom_value(finst->params, pkey, LIVES_SEED_ALLVALUES, _lives_cond_eval(allvp));
    lives_free(pkey);
  }
  // after evaluating all funcinst params recursively
  // we execute the funcinst and return the result
  if (finst->funcdef) {
    LIVES_CALLOC_TYPE(allvalues_t, retvpp, 1);
    SET_RETVAR(finst, retvpp);
    do_call(finst);
    return retvpp;
  }
  return condition;
}


lives_cond_result lives_cond_eval(lives_condition condition) {
  lives_condition xcond = lives_cond_copy(condition);
  allvalues_t *res = _lives_cond_eval(xcond);
  boolean bres = res->values.b[0];
  if (res != xcond) lives_cond_free(res);
  lives_cond_free(xcond);
  //g_print("\nCondition evaluates to: %s\n", CONDRES_NAME(bres));
  return bres ? LIVES_COND_PASS : LIVES_COND_FAIL;
}


void lives_cond_desc(lives_condition cond) {
  allvalues_t *allvp = (allvalues_t *)cond;
  if (allvp) {
    cond_trans *ctrans = (cond_trans *)allvp->priv_data;
    lives_funcinst *finst = allvp->funcinst;
    if (finst) {
      // check all params. and recursively execute any which have a funcinst
      // replacing the allvp with final val returned
      //allvalues_t *allvp;
      char *pkey;
      int maxparms = get_funcinst_nparams(finst);
      // for  'C' 'V', 'U'  print name out right away
      // (we should not have any 'C' with funcinst, since the funcis is
      // run then replaced with the value

      if ((ctrans) && (ctrans->fmt == 'C' || ctrans->fmt == 'V' || ctrans->fmt == 'U' || ctrans->fmt == '!'))
        g_print("%s, ", ctrans->token);

      for (int i = 0; i  < maxparms; i++) {
        pkey = make_std_pname(i);
        if (weed_leaf_seed_type(finst->params, pkey) == LIVES_SEED_ALLVALUES) {
          allvalues_t *xallvp = (allvalues_t *)weed_get_custom_value(finst->params, pkey, LIVES_SEED_ALLVALUES, NULL);
          lives_cond_desc(xallvp);
        } else {
          char *calstr = weed_leaf_stringify(finst->params, pkey);
          g_print("%s, ", calstr);
          lives_free(calstr);
        }
        lives_free(pkey);
        if (!i) {
          // for type 'O' print func after 1st param
          if (ctrans  && ctrans->fmt == 'O') g_print("%s, ", ctrans->token);
        }
      } //for
    } // finst
    else {
      weed_plant_t *tmpplant = lives_plant_new(LIVES_PLANT_TMP);
      LEAF_FROM_ALLV(tmpplant, WEED_LEAF_VALUE, allvp);
      char *calstr = weed_leaf_stringify(tmpplant, WEED_LEAF_VALUE);
      weed_plant_free(tmpplant);
      g_print("%s, %s,  ", ctrans->token, calstr);
      lives_free(calstr);
    }
    d_print("\n");
  }
}

void lives_conditions_init(void) {
  // format is: "TOKEN", type
  // then depending on type
  // '!' directive - nothing
  // 'X' alias - "ALIAS_FOR"
  // 'C' const - description, function, args_fmt [, const_params]
  // - const values are evaluated when the condition is created
  // function is called using the params that follow. and va_list
  // the function takes what it needs from the va_list
  // The function allocs and
  // returns an allvalues_t *, we then store %p in the cong
  // 'V' variable - description, function, args_fmt [, const_params]
  // value is read when cond is evaluated
  // when compiling the cond we make a funcinst from function, args_fmt, [, params]
  // but it is not executed yet
  // on eval. symbolic values are re read. funcinst is executed
  // return value is an allvalues_t *
  // 'O' - operator - description, function, nparams
  //  when compiling the condition, we store only the token name
  //  when evaluating, lookup the token again. When suddicent allvalues_t * have been collated
  // function is called with the following prototype
  // int nvals, allvalues_t *array, return val is also an allvalues_t *
  // nparams is 0. this indicates 1 or more, if nparams is < 0, this indicates 1 to -n params
  // in this case the next token must be COND(LIST_BEGIN), then the list of values, then COND(LIST_END)
  //
  // othwe than thr list designators,  we have just 3 more special symbols: COND(POPEN) and COND(PCLOSE) and COND(EVAL)
  // COND(POPEN) has the following effect
  // the current funcdef and allvalues_t * array are pushed to the eval stack,
  // eval is called recursively with full set to TRUE
  //
  // the difference between full and partial eval:
  // for partial we evaluate symbols until an allvalues_t * is returned,
  // and this becomes the next value in allvalues_t * array.
  // if we parse a COND(POPEN), we push func, and allvalues_t *array, call a full eval,
  // which only returns when COND(PCLOSE) is read. The function and array are popped, and then return val
  // isy added to allvals_t * array.
  // An implied COND(POPEN) / COND(PCLOSE) are implitly added at the start and end of each condition
  // The first eval should return an allvalues_t * with type WEED_SEED_BOOLEAN,
  // which is the return value for the condition

  // symbols with fmt "!" are directives, and are inserted unchanged
  // eg. "COND(POPEN) -> "COND_POPEN"
  register_cond_token("POPEN", '!', "(");
  register_cond_token("PCLOSE", '!', ")");

  register_cond_token("LIST_BEGIN", '!', "[");
  register_cond_token("LIST_END", '!', "[");

  // "X" - alias  (no "COND_" prepended)
  // these are replaced during lives_cond_create
  // after replacement, the new symbol is parsed
  // $ here is "const char *"
  register_cond_token("(",	'X',	"$", _COND_POPEN);
  register_cond_token(")", 	'X',	"$", _COND_PCLOSE);

  register_cond_token("[",	'X',	"$", _COND_LIST_BEGIN);
  register_cond_token("]", 	'X',	"$", _COND_LIST_END);

  register_cond_token("==", 	'X',	"$", COND_PFX "EQUALS");
  register_cond_token(">", 	'X',	"$", COND_PFX "GREATER_THAN");

  register_cond_token("COND_FALSE", 	'X', 	"$b", COND_PFX "BOOLEAN_VAL", FALSE);
  register_cond_token("COND_TRUE", 	'X', 	"$b", COND_PFX "BOOLEAN_VAL", TRUE);

  register_cond_token("COND_NEVER", 	'X', 	"$", COND_PFX "FALSE");
  register_cond_token("COND_ALWAYS", 	'X', 	"$", COND_PFX "TRUE");

  // TODO - wit would be nice to allow aliases which can reference pn, eg #p0, #p1
  // xor ->  #p0 OR #p1 AND NOT POPEN (#p0 AND #p1)
  // gte #p0 EQUALS #p1 OR #p0 GREATER_THAN #p1
  // if we have COND_EVAL evalname, COND_POPEN....COND_PCLOSE
  // then COND_EVAL can be an opfunc
  // supose we read COND_GET we tranlate this to COND_EVAL. #p0, EQUALS. #p1 OR #p0 GTHAN. #p1
  // in the varuable form, we just call a dummy func, which makes funcinst to call cond_eval_const(tok. p0, p1)
  // all this means is, we have an unexpanded part fo the tree which we are going to expand during evaluation
  // then when p0, p` are finalized, in cond_eval_const, we go back to the translation table and get funcinst
  // now we want to splice this in as usueal converting funcinst to tokens, but we also pass in a second funcinst whith p0, p1
  // etc, when we go to splice in #p0, we pull the value (allvalues_t *) from the second funcinst, and splice those values in
  // then we parse the spliced tokens to get const vals, then we insert the sub tree into the value tree, and parse this new part
  /// ie. cond_evel is like an alias that does substutution during evaluation (unless its params are both const)
  //
  // value funcs

  //CONST values are evaluated when the condition is compiled
  // when registering we orivude th func params following th va tape
  // EXAMPLE: "COND_INT_VAL", 2
  register_cond_token("INT_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_INT);
  register_cond_token("UINT_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_UINT);
  register_cond_token("INT64_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_INT64);
  register_cond_token("UINT64_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_UINT64);
  register_cond_token("BOOLEAN_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_BOOLEAN);
  register_cond_token("DOUBLE_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_DOUBLE);
  register_cond_token("FLOAT_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_FLOAT);
  register_cond_token("STRING_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_STRING);
  register_cond_token("VOIDPTR_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_VOIDPTR);
  register_cond_token("FUNCPTR_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_FUNCPTR);
  register_cond_token("PLANTPTR_VAL", 'C', "", cond_allv_const, "i", WEED_SEED_PLANTPTR);

  // VAR values are set only whem the condition is evaluated.
  // when creating the condition, we create a funcinst which calls the const version
  // the funcinst is executed when we evaluate the condition, so the const value is read
  // each time.
  //
  // these functioms all take a string param (item name). If item name is NULL, we take it from va_list
  register_cond_token("LOCAL", 'V', "$#p0", cond_local_var, "$", 		NULL);
  register_cond_token("GLOBAL", 'V', "@#p0", cond_global_var, "$", 		NULL);
  register_cond_token("SYMBOLIC", 'V', "#p0", cond_book_value, "$", 		NULL);

  /* // value of leaf p1 in plant p0 */
  /* register_cond_token("ATTRIBUTE", 		'V', "attribute #p1 of #p0", cond_objattr, NULL); */

  // VAR values, const params
  register_cond_token("SYM_SRC_ITEM", 		'V', "#$p0", cond_local_var, "$",			"src_item");
  register_cond_token("SYM_TARGET_ITEM", 	'V', "#$p0", cond_local_var, "$",			"target_item");
  register_cond_token("SYM_SRC_OBJECT", 	'V', "#$p0", cond_local_var, "$", 	       		"src_object");
  register_cond_token("SYM_TARGET_OBJECT", 	'V', "#$p0", cond_local_var, "$", 			"target_object");
  register_cond_token("SYM_OLD_VALUE", 		'V', "#$p0", cond_local_var, "$",			"old_value");
  register_cond_token("SYM_NEW_VALUE", 		'V', "#$p0", cond_local_var, "$",			"new_value");

  // OP functions have any number of params, which they retrieve themselves
  // fpr varable numbers of params, they must be followed by "COND_LIST_BEGIN" ... "COND_LIST_END"

  // 'U' is the same as 'O', but represent unitary functions.
  // the distinction is only for the parser syntax table
  register_cond_token("NOT", 'U', "NOT #p0",	        cond_logic_not);

  // returns alvalues, stype INT
  register_cond_token("TYPEOF", 'U', "seed_type of #p0",   cond_seed_type);
  //
  register_cond_token("OR",  'O', "#p0 AND/OR #p1",	cond_logic_or);
  register_cond_token("AND", 'O', "#p0 AND #p1",	cond_logic_and);
  // todo - use AND + OR
  register_cond_token("XOR", 'O', "either #p0 OR #p1",	cond_logic_xor);

  register_cond_token("EQUALS", 'O',  "#p0 is equal to #p1",  cond_equals);
  register_cond_token("GREATER", 'O',  "#p0 is greater than #p1",  cond_greater);
  register_cond_token("BIT_SET", 'O', "#p0 has bit #p1 set",  cond_bit_set);

  // COND_EVAL allows the inclusion of another cond within a cond, so we can extend
  // and combine existing conds

  // here we look ar args_fmt, and this tells us how many allvalues_t * should be in the array we pass
  // the evaluator collates allvalues from value funcs and passes them to the opfunc

  // there is no real distinction between type 'V' and type 'O'
  // except that for the former. when we make

  //  register_cond_token("EVAL", 'V', "", cond_eval_var, NULL);

  // this is going to be followed by a lives_funcptr_t and then an args_fmt, then ,atching params, const or var
  // we need to handle this specially - first we get the func
  /* register_cond_token("TESTFUNC_CONST", "",	 	"Z",		"COND_XTESTFUNC_CONST, %p"); */
  /* register_cond_token("XTESTFUNC_CONST", "",	 	"!",		NULL); */

  // END OF COND TOKENS //


  // we also have command symbols
  // these are preceded by CMD_PREFIX
  // 'A' is like an opfunc, but only after initial 'V'
  // 'L' same as 'V' but onlu certain locations
  // 'I' is similar to 'U', followed by cond_popen
  // '/' like "!"
#if 0
  register_cmd_token("ASSIGN", 'A',  "set #p0 to #p1",  cmd_assign);
  register_cmd_token("LABEL", 	'L',  "",  "$", cmd_label);
  register_cmd_token("GOTO", 	'L',  "",  "$", cmd_goto);
  register_cmd_token("IF",	'I',  "IF #P0 THEN #P1 ELSE #P2",  cmd_if);
  //
  register_cmd_token("ELSE", 	'/',  "CMD_ELSE");// only after bl end
  register_cmd_token("BLOCK_START", 	'/',  "{"); // same as popen
  register_cmd_token("BLOCK_END", 	'/',  "}"); // pclose
#endif

  cond_trans_list = lives_list_reverse(cond_trans_list);
}
