// functions.c
// (c) G. Finch 2002 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#define FUNCTIONS_C
#include "main.h"
#undef FUNCTIONS_C

#include "diagnostics.h"

#define ADD_FUNCSIG_FUNCS
#include "funcsigs.h"
#undef ADD_FUNCSIG_FUNCS

MK_ALL_FUNCS

#undef MK_ALL_FUNCS

static hook_stack_descriptor_t hs_desc[N_HOOK_POINTS];

/* static int n_normal_stacks = N_HOOK_POINTS; */
/* static int n__stacks = N_HOOK_POINTS; */

static boolean hs_inited = FALSE;
static boolean hsn_inited = FALSE;

VARNAME_FUNC

///////////////////////

Type_Array vasu2allvp_array(va_surprise *boo) {
  const char *fmt = boo->fmt;
  int nvals = lives_strlen(fmt);
  LIVES_CALLOC_TYPE(allvalues_t *, allvp_arr, nvals + 1);
  for (int i = 0; fmt[i]; i++) {
    weed_seed_t st = get_seedtype(fmt[i]);
    allvp_arr[i] = MAKE_ALLVALUE_VA(st, boo->va);
  }
  return allvp_arr;
}


Type_List *params2allvp_list(weed_plant_t *params) {
  Type_List *allvp_list = NULL;
  allvalues_t *allvp;
  int i = 0;
  while (1) {
    char *pkey = make_std_pname(i++);
    if (!(weed_plant_has_leaf(params, pkey))) {
      lives_free(pkey);
      break;
    }
    allvp = allvalues_from_leaf(NULL, params, pkey);
    allvp_list = lives_list_prepend(allvp_list, (void *)allvp);
    lives_free(pkey);
  }
  return lives_list_reverse(allvp_list);
}


void t_array_free(T_array *t) {
  if (t && *t) {
    for (int i = 0; (*t)[i]; i++)
      allvalues_free((*t)[i]);
    lives_free(*t);
    *t = NULL;
  }
}


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
  avp->uid = gen_unique_id();
  return avp;
}


void get_val_from_allvals(void *valp, allvalues_t *allvp) {
  // get value whether or not bound
  switch (allvp->stype) {
  case WEED_SEED_INT: {int32_t *xvalp = (int32_t *)valp; *xvalp = *allvp->values.i; break;}
  case WEED_SEED_UINT: {uint32_t *xvalp = (uint32_t *)valp; *xvalp = *allvp->values.u; break;}
  case WEED_SEED_BOOLEAN: {boolean *xvalp = (boolean *)valp; *xvalp = *allvp->values.b; break;}
  case WEED_SEED_INT64: {int64_t *xvalp = (int64_t *)valp; *xvalp = *allvp->values.I; break;}
  case WEED_SEED_UINT64: {uint64_t *xvalp = (uint64_t *)valp; *xvalp = *allvp->values.U; break;}
  case WEED_SEED_DOUBLE: {double *xvalp = (double *)valp; *xvalp = *allvp->values.d; break;}
  case WEED_SEED_FLOAT: {float *xvalp = (float *)valp; *xvalp = *allvp->values.f; break;}
  case WEED_SEED_STRING: {char **xvalp = (char **)valp; *xvalp = *allvp->values.s; break;}
  case WEED_SEED_VOIDPTR: {void **xvalp = (void **)valp; *xvalp = *allvp->values.V; break;}
  case WEED_SEED_FUNCPTR: {weed_funcptr_t *xvalp = (weed_funcptr_t *)valp; *xvalp = *allvp->values.F; break;}
  case WEED_SEED_PLANTPTR: {weed_plantptr_t *xvalp = (weed_plantptr_t *)valp; *xvalp = *allvp->values.P; break;}
  case LIVES_SEED_CONST_CHARPTR: {const char **xvalp = (const char **)valp; *xvalp = *allvp->values.C; break;}
  case LIVES_SEED_FUNCINST: {lives_funcinst_t **xvalp = (lives_funcinst_t **)valp; *xvalp = allvp->funcinst; break;}
  default: break;
  }
}


void get_array_byref_from_allvals(void **array, allvalues_t *allvp, int *ne) {
  // gte array by ref
  // or bound value, like an array with 1 element
  if (ne) *ne = allvp->ne;
  switch (allvp->stype) {
  case WEED_SEED_INT: {*array = (void *)allvp->values.i; break;}
  case WEED_SEED_UINT: {*array = (void *)allvp->values.u; break;}
  case WEED_SEED_BOOLEAN: {*array = (void *)allvp->values.b; break;}
  case WEED_SEED_INT64: {*array = (void *)allvp->values.I; break;}
  case WEED_SEED_UINT64: {*array = (void *)allvp->values.U; break;}
  case WEED_SEED_DOUBLE: {*array = (void *)allvp->values.d; break;}
  case WEED_SEED_FLOAT: {*array = (void *)allvp->values.f; break;}
  case WEED_SEED_STRING: {*array = (void *)allvp->values.s; break;}
  case WEED_SEED_VOIDPTR: {*array = (void *)allvp->values.V; break;}
  case WEED_SEED_FUNCPTR: {*array = (void *)allvp->values.F; break;}
  case WEED_SEED_PLANTPTR: {*array = (void *)allvp->values.P; break;}
  case LIVES_SEED_CONST_CHARPTR: {*array = (void *)allvp->values.C; break;}
  default: break;
  }
}


allvalues_t *_make_allval_va(allvalues_t *avp, weed_seed_t stype,
                             int flags, va_list va) {
  uint64_t xflags = 0;
  if (!avp) {
    avp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    avp->uid = gen_unique_id();
  } else {
    if (stype != avp->stype) {
      avp->flags |= ALLV_ERR_WRONG_STYPE;
      return avp;
    }
    xflags = avp->flags;
    if (xflags & ALLV_FLAG_RDONLY) {
      avp->flags |= ALLV_ERR_RDONLY;
      //  BREAK_ME("ALLV RDONLY");
      return avp;
    }
    // clear old vals
    avp->values.V = NULL;
    if (avp->funcinst) {
      lives_funcinst_free(avp->funcinst);
      avp->funcinst = NULL;
    }
  }

  if (flags & ALLV_FLAG_RWLOCK) {
    avp->rwlock = (pthread_rwlock_t *)lives_malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(avp->rwlock, NULL);
  }
  avp->stype = stype;
  avp->ne = 1;

  if (flags & PARAM_FLAG_BOUND) {
    // args will be a pointer to var, cast to void *
    // e.g. void *voidpval = (void *)((int *)&intval)
    // now we can just cast this to a (void **) and set avp->values.V = (void **)voidpval
    // (because .V is normally an array of void *)
    // since values is a union, we now read avp->values.i - this is the same location but the type is (int *)
    // now we can read or write to *avp->values.i, and this is the same as reading or writing to / from intval
    // this trick works because in C, there is no distincion between pointer used as array of type and
    // pointer used as pointer to type. avp->flags tells us how to interpret the pointer value
    // the only caveat is that we can only point to a scalar and not to an array
    void *voidval = va_arg(va, void *);
    avp->values.V = (void **)voidval;
    xflags |= ALLV_FLAG_POINTER;
  } else {
    if (stype == LIVES_SEED_FUNCINST) {
      avp->values.V = NULL;
      avp->funcinst = va_arg(va, lives_funcinst_t *);
    } else {
      //      if (stype == WEED_SEED_FLOAT) BREAK_ME("shduh");
      weed_plant_t *tmp = lives_plant_new(LIVES_PLANT_TMP);
      weed_leaf_from_varg(tmp, WEED_LEAF_VALUE, stype, -1, va);
      avp = allvalues_from_leaf(avp, tmp, WEED_LEAF_VALUE);
      weed_plant_free(tmp);
      //
      avp->stype = stype;
    }
    xflags &= ~ALLV_FLAG_POINTER;
  }
  avp->ne = 1;
  avp->flags = xflags;
  return avp;
}


allvalues_t *_make_allval(allvalues_t *avp, weed_seed_t stype,
                          int flags, const char *valname, ...) {
  if (!avp) {
    avp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    avp->uid = gen_unique_id();
    if (stype == WEED_SEED_INVALID) {
      avp->flags = ALLV_ERR_INVALID_STYPE;
      return avp;
    }
    avp->stype = stype;
  } else {
    if (stype == WEED_SEED_INVALID) {
      avp->flags = ALLV_ERR_INVALID_STYPE;
      return avp;
    }
    if (stype != avp->stype) {
      avp->flags |= ALLV_ERR_WRONG_STYPE;
      return avp;
    }
    if (avp->flags & ALLV_FLAG_RDONLY) {
      avp->flags |= ALLV_ERR_RDONLY;
      //  BREAK_ME("ALLV3 RDONLY");
      return avp;
    }
  }

  avp->aname = lives_strdup(valname);
  va_list args;
  va_start(args, valname);
  _make_allval_va(avp, stype, flags, args);
  va_end(args);
  return avp;
}


allvalues_t *_make_allval_array_va(allvalues_t *avp, weed_seed_t stype,
                                   weed_size_t ne, int flags, va_list va) {
  uint64_t xflags = 0;
  if (!avp) {
    avp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    avp->uid = gen_unique_id();
  } else {
    if (stype != avp->stype) {
      avp->flags |= ALLV_ERR_WRONG_STYPE;
      return avp;
    }
    xflags = avp->flags;
    if (xflags & ALLV_FLAG_RDONLY) {
      avp->flags |= ALLV_ERR_RDONLY;
      //  BREAK_ME("ALLV RDONLY");
      return avp;
    }
    // clear old vals
    avp->values.V = NULL;
    if (avp->funcinst) {
      lives_funcinst_free(avp->funcinst);
      avp->funcinst = NULL;
    }
  }

  if (flags & ALLV_FLAG_RWLOCK) {
    avp->rwlock = (pthread_rwlock_t *)lives_malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(avp->rwlock, NULL);
  }
  avp->stype = stype;
  avp->ne = ne;

  if (flags & PARAM_FLAG_BOUND) {
    if (ne > 1) xflags = ALLV_ERR_BAD_BIND;
    else {
      // args will be a pointer to var, cast to void *
      // e.g. void *voidpval = (void *)((int *)&intval)
      // now we can just cast this to a (void **) and set avp->values.V = (void **)voidpval
      // (because .V is normally an array of void *)
      // since values is a union, we now read avp->values.i - this is the same location but the type is (int *)
      // now we can read or write to *avp->values.i, and this is the same as reading or writing to / from intval
      // this trick works because in C, there is no distincion between pointer used as array of type and
      // pointer used as pointer to type. avp->flags tells us how to interpret the pointer value
      // the only caveat is that we can only point to a scalar and not to an array
      void *voidval = va_arg(va, void *);
      avp->values.V = (void **)voidval;
      xflags |= ALLV_FLAG_POINTER;
    }
  } else {
    if (stype == LIVES_SEED_FUNCINST) {
      avp->values.V = NULL;
      avp->funcinst = va_arg(va, lives_funcinst_t *);
    } else {
      //      if (stype == WEED_SEED_FLOAT) BREAK_ME("shduh");
      weed_plant_t *tmp = lives_plant_new(LIVES_PLANT_TMP);
      weed_leaf_from_varg(tmp, WEED_LEAF_VALUE, stype, ne, va);
      avp = allvalues_from_leaf(avp, tmp, WEED_LEAF_VALUE);
      weed_plant_free(tmp);
      //
      avp->stype = stype;
    }
    xflags &= ~ALLV_FLAG_POINTER;
  }
  avp->ne = ne;
  avp->flags = xflags;
  return avp;
}


allvalues_t *_make_allval_array(allvalues_t *avp, weed_seed_t stype, weed_size_t ne,
                                int flags, const char *valname, ...) {
  if (!avp) {
    avp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    avp->uid = gen_unique_id();
    if (stype == WEED_SEED_INVALID) {
      avp->flags = ALLV_ERR_INVALID_STYPE;
      return avp;
    }
    avp->stype = stype;
  } else {
    if (stype == WEED_SEED_INVALID) {
      avp->flags = ALLV_ERR_INVALID_STYPE;
      return avp;
    }
    if (stype != avp->stype) {
      avp->flags |= ALLV_ERR_WRONG_STYPE;
      return avp;
    }
    if (avp->flags & ALLV_FLAG_RDONLY) {
      avp->flags |= ALLV_ERR_RDONLY;
      //  BREAK_ME("ALLV3 RDONLY");
      return avp;
    }
  }

  avp->aname = lives_strdup(valname);
  va_list args;
  va_start(args, valname);
  _make_allval_array_va(avp, stype, ne, flags, args);
  va_end(args);
  return avp;
}


// retloc must be large enough to hold a value or array os the relevant type
lives_result_t array_from_allvalues(void *retloc, allvalues_t *avp) {
  if (!avp) return LIVES_RESULT_INVALID;
  if (avp->funcinst) return LIVES_RESULT_FAIL;
  weed_plant_t *tmpplant = lives_plant_new(LIVES_PLANT_TMP);
  LEAF_FROM_ALLV(tmpplant, WEED_LEAF_VALUE, avp);
  weed_error_t err = weed_leaf_get(tmpplant, WEED_LEAF_VALUE, 0, retloc);
  weed_plant_free(tmpplant);
  if (err == WEED_SUCCESS) return LIVES_RESULT_SUCCESS;
  return LIVES_RESULT_ERROR;
}


LIVES_GLOBAL_INLINE allvalues_t *allvalues_copy(allvalues_t *avp) {
  LIVES_CALLOC_TYPE(allvalues_t, xavp, 1);
  lives_memcpy(xavp, avp, sizeof(allvalues_t));
  avp->uid = gen_unique_id();
  xavp->funcinst = NULL;
  xavp->aname = lives_strdup(avp->aname);
  xavp->ext_typename = lives_strdup(avp->ext_typename);
  // values.* will point to same values. whay we must do is get seed typem get size for it
  // so set src to weed_leaf, then get values back int dst
  weed_plant_t *tmpplant = lives_plant_new(LIVES_PLANT_TMP);
  LEAF_FROM_ALLV(tmpplant, WEED_LEAF_VALUE, avp);
  allvalues_from_leaf(xavp, tmpplant, WEED_LEAF_VALUE);
  weed_plant_free(tmpplant);
  return xavp;
}


void allvalues_free(allvalues_t *avp) {
  if (avp) {
    if (avp->aname) lives_free(avp->aname);

    if (avp->contingencies) {
      for (LiVESList *list = avp->contingencies; list; list = list->next)
        lives_funcinst_free((lives_funcinst_t *)list->data);
      lives_list_free(avp->contingencies);
    }

    if (avp->flags & ALLV_FLAG_FREE_VALUE) {
      if (avp->funcinst) lives_funcinst_free(avp->funcinst);
      else {
        for (int i = 0; i < avp->ne; i++)
          if (avp->values.V[i])	lives_free(avp->values.V[i]);
      }
    }

    if (!(avp->flags & ALLV_FLAG_POINTER) && avp->values.V) lives_free(avp->values.V);

    if (avp->rwlock) {
      pthread_rwlock_destroy(avp->rwlock);
      lives_free(avp->rwlock);
    }
    lives_free(avp);
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
    hook_stack_descriptor_t *xhs = (hook_stack_descriptor_t *)&hs_desc[i];
    boolean got = FALSE;
    xhs->htype = i;

    if (!hsn_inited) {
      switch (i) {
      case FATAL_HOOK:
        hs_desc[i] = HS_DETAILS(FATAL);
        hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, FATAL));
        break;
      case RESETTING_HOOK:
        hs_desc[i] = HS_DETAILS(RESETTING);
        hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, RESETTING));
        break;
      case THREAD_EXIT_HOOK:
        hs_desc[i] = HS_DETAILS(THREAD_EXIT);
        hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, THREAD_EXIT));
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
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, COMPLETED));
          break;
        case DATA_PREVIEW_HOOK:
          hs_desc[i] = HS_DETAILS(DATA_PREVIEW);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, DATA_PREVIEW));
          break;
        case DATA_READY_HOOK:
          hs_desc[i] = HS_DETAILS(DATA_READY);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, DATA_READY));
          break;
        case LIVES_GUI_HOOK:
          hs_desc[i] = HS_DETAILS(LIVES_GUI);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, GUI));
          break;
        case ATTRS_UPDATED_HOOK:
          hs_desc[i] = HS_DETAILS(ATTRS_UPDATED);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, ATTRS_UPDATED));
          break;
        case SYNC_ANNOUNCE_HOOK:
          hs_desc[i] = HS_DETAILS(SYNC_ANNOUNCE);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, SYNC_ANNOUNCE));
          break;
        case SEGMENT_END_HOOK:
          hs_desc[i] = HS_DETAILS(SEGMENT_END);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, SEGMENT_END));
          break;
        case FINISHED_HOOK:
          hs_desc[i] = HS_DETAILS(FINISHED);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, FINISHED));
          break;
        case ERROR_HOOK:
          hs_desc[i] = HS_DETAILS(ERROR);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, ERROR));
          break;
        case PAUSED_HOOK:
          hs_desc[i] = HS_DETAILS(PAUSED);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, PAUSED));
          break;
        case RESUMING_HOOK:
          hs_desc[i] = HS_DETAILS(RESUMING);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, RESUMING));
          break;
        case BUSY_HOOK:
          hs_desc[i] = HS_DETAILS(BUSY);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, BUSY));
          break;
        case UNBUSY_HOOK:
          hs_desc[i] = HS_DETAILS(UNBUSY);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, UNBUSY));
          break;
        case CANCELLED_HOOK:
          hs_desc[i] = HS_DETAILS(CANCELLED);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, CANCELLED));
          break;
        case DESTRUCTION_HOOK:
          hs_desc[i] = HS_DETAILS(DESTRUCTION);
          hs_desc[i].const_data_srcs = strings_to_list(HOOKSRCS(CONST, DESTRUCTION));
          break;
        default: break;
        }
      }
    }
    //g_print("Registered details for hs type %d, pattern = %d, flags = %lu\n", i, hs_desc[i].pattern, hs_desc[i].op_flags);
  }
  hsn_inited = TRUE;
}


//lives_result_t lives_hook_register(int hstype,


const hook_stack_descriptor_t *get_hs_desc(int hstype) {
  if (!hs_inited || !hsn_inited) init_hook_stacks();
  return &hs_desc[hstype];
}


#define PTMLH do {pthread_mutex_lock(hmutex);} while (0)
#define PTMUH do {pthread_mutex_unlock(hmutex);} while (0)
#define PTMTLH pthread_mutex_trylock(hmutex)

const lookup_tab crossrefs[] = XREFS_TAB;


weed_error_t weed_leaf_from_varg(weed_plant_t *plant, const char *key, weed_seed_t type, int32_t ne, va_list xargs) {
  if (!ne) return weed_leaf_set(plant, key, type, 0, NULL);

  if (ne == -1) {
    switch (type) {
    case WEED_SEED_INT:		return weed_set_int_value(plant, key, va_arg(xargs, int));
    case WEED_SEED_DOUBLE: 	return weed_set_double_value(plant, key, va_arg(xargs, double));
    case WEED_SEED_BOOLEAN: 	return weed_set_boolean_value(plant, key, va_arg(xargs, boolean));
    case WEED_SEED_STRING:  	return weed_set_string_value(plant, key, va_arg(xargs, char *));
    case WEED_SEED_INT64: 	return weed_set_int64_value(plant, key, va_arg(xargs, int64_t));
#ifdef WEED_SEED_FLOAT
    case WEED_SEED_FLOAT:    	return weed_set_float_value(plant, key, (float)va_arg(xargs, double));
#endif
#ifdef WEED_SEED_UINT
    case WEED_SEED_UINT:	return weed_set_uint_value(plant, key, va_arg(xargs, uint32_t));
#endif
#ifdef WEED_SEED_UINT64
    case WEED_SEED_UINT64: 	return weed_set_uint64_value(plant, key, va_arg(xargs, uint64_t));
#endif
    case WEED_SEED_FUNCPTR: 	return weed_set_funcptr_value(plant, key, va_arg(xargs, weed_funcptr_t));
    case WEED_SEED_VOIDPTR:	return weed_set_voidptr_value(plant, key, va_arg(xargs, void *));
    case WEED_SEED_PLANTPTR: 	return weed_set_plantptr_value(plant, key, va_arg(xargs, weed_plant_t *));
    case LIVES_SEED_CONST_CHARPTR: {
      weed_error_t err;
      const char *cval = va_arg(xargs, const char *);
      err = weed_set_custom_value(plant, key, type, (void *)cval);
      if (err == WEED_SUCCESS && cval) weed_set_custom_element_size(plant, key, 0, lives_strlen(cval));
      else weed_set_custom_element_size(plant, key, 0, 0);
      return err;
    }
    default:
      if (WEED_SEED_IS_CUSTOM(type))
        return weed_set_custom_value(plant, key, type, va_arg(xargs, void *));
      return WEED_ERROR_WRONG_SEED_TYPE;
    }
  }

  switch (type) {
  case WEED_SEED_INT:		return weed_set_int_array(plant, key, ne, va_arg(xargs, int *));
  case WEED_SEED_DOUBLE: 	return weed_set_double_array(plant, key, ne, va_arg(xargs, double *));
  case WEED_SEED_BOOLEAN: 	return weed_set_boolean_array(plant, key, ne, va_arg(xargs, boolean *));
  case WEED_SEED_STRING:  	return weed_set_string_array(plant, key, ne, va_arg(xargs, char **));
  case WEED_SEED_INT64: 	return weed_set_int64_array(plant, key, ne, va_arg(xargs, int64_t *));
#ifdef WEED_SEED_FLOAT
  case WEED_SEED_FLOAT:    	return weed_set_float_array(plant, key, ne, (float *)va_arg(xargs, double *));
#endif
#ifdef WEED_SEED_UINT
  case WEED_SEED_UINT:	return weed_set_uint_array(plant, key, ne, va_arg(xargs, uint32_t *));
#endif
#ifdef WEED_SEED_UINT64
  case WEED_SEED_UINT64: 	return weed_set_uint64_array(plant, key, ne, va_arg(xargs, uint64_t *));
#endif
  case WEED_SEED_FUNCPTR: 	return weed_set_funcptr_array(plant, key, ne, va_arg(xargs, weed_funcptr_t *));
  case WEED_SEED_VOIDPTR:	return weed_set_voidptr_array(plant, key, ne, va_arg(xargs, void **));
  case WEED_SEED_PLANTPTR: 	return weed_set_plantptr_array(plant, key, ne, va_arg(xargs, weed_plant_t **));
  case LIVES_SEED_CONST_CHARPTR: {
    weed_error_t err;
    const char **cvalp = va_arg(xargs, const char **);
    err = weed_set_custom_array(plant, key, type, ne, (void **)cvalp);
    if (err == WEED_SUCCESS && cvalp)  {
      for (int i = 0 ; i < ne; i ++)
        weed_set_custom_element_size(plant, key, i, lives_strlen(cvalp[i]));
    }
    return err;
  }
  default:
    if (WEED_SEED_IS_CUSTOM(type))
      return weed_set_custom_array(plant, key, type, ne, va_arg(xargs, void **));
    return WEED_ERROR_WRONG_SEED_TYPE;
  }
}


weed_error_t my_weed_leaf_set(weed_plant_t *pl, const char *key, weed_seed_t stype, int ne, ...) {
  va_list va;
  va_start(va, ne);
  weed_error_t err = weed_leaf_from_varg(pl, key, stype, ne, va);
  va_end(va);
  return err;
}


LIVES_GLOBAL_INLINE const char get_typeletter(uint8_t val) {
  // sigbits to letter
  val &= 0x0F;
  for (int i = 0; crossrefs[i].typeletter; i++) {
    if (crossrefs[i].sigbits == val) return crossrefs[i].typeletter;
  }
  return ARGS_FMT_UNKNOWN;
}


LIVES_GLOBAL_INLINE weed_seed_t get_seedtype(char c) {
  // typeletter to seed_type
  for (int i = 0; crossrefs[i].typeletter; i++) {
    if (crossrefs[i].typeletter == c)
      return crossrefs[i].seed_btype;
  }
  return WEED_SEED_INVALID;
}


LIVES_GLOBAL_INLINE uint8_t get_typecode(char c) {
  // letter to sigbits
  for (int i = 0; crossrefs[i].typeletter; i++) {
    if (crossrefs[i].typeletter == c) return crossrefs[i].sigbits;
  }
  return 0x0;
}


LIVES_GLOBAL_INLINE const char get_char_for_st(weed_seed_t st) {
  // seed_type to char
  for (int i = 0; crossrefs[i].typeletter; i++) {
    if (crossrefs[i].seed_btype == st) return crossrefs[i].typeletter;
  }
  return '\0';
}


LIVES_GLOBAL_INLINE uint8_t get_sigbits_for_st(weed_seed_t st) {
  // seed_type to sigbits
  for (int i = 0; crossrefs[i].typeletter; i++) {
    if (crossrefs[i].seed_btype == st) return crossrefs[i].sigbits;
  }
  return 0x0;
}

LIVES_GLOBAL_INLINE const char *get_fmtstr_for_st(weed_seed_t st) {
  // letter to sigbits
  for (int i = 0; crossrefs[i].typeletter; i++) {
    if (crossrefs[i].seed_btype == st) return crossrefs[i].fmtstr;
  }
  return '\0';
}


LIVES_GLOBAL_INLINE const char *get_symbolname(uint8_t val) {
  // sigbits to symname
  val &= 0x0F;
  for (int i = 0; crossrefs[i].typeletter; i++) {
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

void add_quick_fn(lives_funcptr_t func, lives_funcdef_t *fdef) {
  char *namei = LSPF("%p", func);
  add_to_lookup_table(fn_looker, WEED_SEED_VOIDPTR, namei, fdef);
  lives_free(namei);
}


const char *get_funcname(lives_funcptr_t func) {
  return NULL;
  if (!fn_looker) return NULL;
  lives_funcdef_t *fdef = (lives_funcdef_t *)get_from_hash_store_i(fn_looker, (uint64_t)func);
  return fdef ? fdef->funcname : NULL;
}



void dump_known_functions(void) {
  if (fn_looker) {
    const char *key;
    lives_funcdef_t *fdef;
    g_print("Summary of known functions:\n");
    LIVES_INDEX_FOREACH(fn_looker, key, fdef, g_print("%s\n", lives_funcdef_explain(fdef)););
    g_print("\ndone\n\n");
  }
}


void _func_entry(lives_funcptr_t func, const char *funcname, int category, const char *rettype,
                 const char *args_fmt, char *file_ref, int line_ref, uint64_t flags) {
  lives_funcdef_t *fdef = NULL;
  weed_seed_t rtype = WEED_SEED_NONE;
  if (!fn_looker) fn_looker = lives_make_lookup(lookup_type_funcs);
  char *namei = LSPF("%p", func);

  if (rettype && *rettype) rtype = get_seedtype(*rettype);
  allvalues_t *avp = find_in_lookup_table(fn_looker, namei);
  if (avp) fdef = (lives_funcdef_t *)avp->values.V[0];
  if (!fdef) {
    fdef = create_funcdef(funcname, func, rtype, args_fmt, file_ref, line_ref, flags);
    add_to_lookup_table(fn_looker, WEED_SEED_VOIDPTR, namei, fdef);
  }
  lives_free(namei);
  THREADVAR(func_stack) = lives_sync_list_push(THREADVAR(func_stack), fdef);
}


void _func_exit(char *file_ref, int line_ref, const char *valname, ...) {
  //
  lives_sync_list_pop(&THREADVAR(func_stack));
}

///////////////////////////

char *make_std_pname(int pn) {return lives_strdup_printf("%s%d", LIVES_LEAF_THREAD_PARAM, pn);}


static boolean is_child_of(LiVESWidget *w, LiVESContainer *C);

static boolean fn_match_child(lives_funcinst_t *finst1, lives_funcinst_t *finst2);


static lives_result_t weed_plant_params_from_valist(weed_plant_t *plant, const char *args_fmt, \
    make_key_f param_name_func, int *start, int np, va_list xargs) {

  // here we have to filter non-strict
  // this will - leav custom values as custom
  // alter _(vars) to vars, using inline va

  if (!plant) return LIVES_RESULT_INVALID;
  int maxparms = weed_get_int_value(plant, LIVES_LEAF_MAXPARAM, NULL);
  int pstart = 0;

  if (start) pstart = *start;
  for (const char *c = args_fmt; *c; c++) {
    if (*c == ARGS_FMT_GAP) {
      pstart++;
      if (np && !--np) break;
      continue;
    }

    uint32_t st = get_seedtype(*c);

    if (st == WEED_SEED_INVALID) {
      continue;
    }

    char *pkey = (*param_name_func)(pstart++);
    weed_error_t err = weed_leaf_from_varg(plant, pkey, st, -1, xargs);
    lives_free(pkey);
    if (err != WEED_SUCCESS) return LIVES_RESULT_ERROR;
    if (np && !--np) break;
  }

  if (pstart > maxparms) weed_set_int_value(plant, LIVES_LEAF_MAXPARAM, pstart);
  if (start) *start = pstart;
  return LIVES_RESULT_SUCCESS;
}


char *get_args_fmt(weed_plant_t *plant) {
  if (!plant) return NULL;
  char *args_fmt = NULL, *unknown = LSPF("%c", ARGS_FMT_UNKNOWN);
  int maxparms = weed_get_int_value(plant, LIVES_LEAF_MAXPARAM, NULL);
  for (int i = 0; i < maxparms; i++) {
    char *pkey = make_std_pname(i);
    weed_seed_t st = weed_leaf_seed_type(plant, pkey);
    if (st == WEED_SEED_INVALID) args_fmt = lives_strcollate(&args_fmt, NULL, unknown);
    else {
      // if we have a custom type, fake as voidptr - so we can pass the known funcsig check
      // and the switch in do_call,
      if (WEED_SEED_IS_CUSTOM(st)) st = WEED_SEED_VOIDPTR;
      char *fmt = LSPF("%c", get_char_for_st(st));
      args_fmt = lives_strcollate(&args_fmt, NULL, (const char *)fmt);
      lives_free(fmt);
    }
    lives_free(pkey);
  }
  lives_free(unknown);
  return args_fmt;
}


boolean args_fmt_match(const char *def, const char *inst) {
  char *xinst = NULL;

  if ((!inst || !*inst) && (!def || !*def || !strcmp(def, "*"))) goto yes;
  if (!inst || !def) goto no;

  xinst = args_fmt_filter(inst);

  for (int i = 0; def[i]; i++) {
    if (def[i] == '*') goto yes;
    if (!xinst[i]) goto no;
    if (xinst[i] == ARGS_FMT_GAP) continue;
    if (get_typecode(xinst[i]) != get_typecode(def[i])) goto no;
  }

yes:
  if (xinst) lives_free(xinst);
  return TRUE;

no:
  if (xinst) lives_free(xinst);
  return FALSE;
}


lives_result_t funcinst_params_from_vargs(lives_funcinst_t *finst,  const char *args_fmt, va_list xargs) {
  // because fdef params can now end in variadic
  lives_result_t res = LIVES_RESULT_INVALID;
  if (!args_fmt) return LIVES_RESULT_SUCCESS;
  if (finst) {
    lives_funcdef_t *fdef = finst->funcdef;
    if (fdef) {
      // we can have  finst with no fdef eg type 'X' cond tokens
      char *xargs_fmt = args_fmt_from_funcsig(fdef->funcsig);
      if (!args_fmt_match(xargs_fmt, args_fmt)) {
        lives_free(xargs_fmt);
        return LIVES_RESULT_ERROR;
      }
      lives_free(xargs_fmt);
    }
    if (!finst->params) finst->params = lives_plant_new(LIVES_PLANT_FUNCPARAMS);
    res = weed_plant_params_from_valist(finst->params, args_fmt, make_std_pname, NULL, 0, xargs);
  }
  return res;
}


char *args_fmt_from_allvals(int nvals, allvalues_t **pvals) {
  funcsig_t fsig = 0;
  if (nvals > 16) return NULL;
  for (int i = 0; i < nvals; i++) {
    fsig = (fsig << 4) + get_char_for_st(pvals[i]->stype);
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
      return NULL;
    }
    lives_free(xargs_fmt);
  }
  lives_free(args_fmt);
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
      str = lives_strdup_concat_sep(str, " ", "%sp%d",
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
          str = lives_strdup_concat_sep(str, ", ", "p%d", pnn);
          // zero out so we dont end up repeating a type
          sig ^= tch << k;
        }
        pnn++;
      }
      str = lives_strdup_concat_sep(str, NULL, ";");
    }
  }
  return str;
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
      do_call(free_finst);
      if (CONTINGENCY_DATA(free_finst, flags) & CONTINGENCY_BEHAVIOUR_READY_ON_IDLE) {
        CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_READY;
        return;
      }
      CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_IDLE;
      /* if (CONTINGENCY_DATA(free_finst, flags) & CONTINGENCY_BEHAVIOUR_NO_FREE_ON_IDLE) */
      /* 	return; */
    } else {
      CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_EXPIRED;
      if (CONTINGENCY_DATA(free_finst, flags) & CONTINGENCY_BEHAVIOUR_NO_FREE_ON_EXPIRED)
        return;
    }
    lives_funcinst_free(free_finst);
  } else lives_free(pkey);
}


static void free_finst_paramdata(lives_funcinst_t *finst, boolean do_exec) {
  if (finst && finst->params) {
    int nparms = get_funcinst_nparams(finst);
    for (int i = 0; i < nparms; i++) call_free_func(finst, i, do_exec);
  }
}


int get_funcsig_nparms(funcsig_t sig) {
  int nparms = 0;
  for (funcsig_t test = 0xF; test & sig; test <<= 4) nparms++;
  return nparms;
}


static boolean lives_funcinst_error_state(lives_funcinst_t *finst) {
  lives_proc_thread_t lpt = NULL;
  pthread_rwlock_rdlock(&finst->dispolock);
  if (MODULE_TYPE_IS(finst, HOOK_STACK)) {
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
  }
  pthread_rwlock_unlock(&finst->dispolock);

  if (lpt) return lives_proc_thread_had_error(lpt);
  return FALSE;
}


#define NEED_FSIG_CASES 1
#include "funcsigs.h"
#undef NEED_FSIG_CASES

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
  // e.g DO_CALL(2)
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
  lives_proc_thread_t lpt = NULL;

  if (!fdef) return LIVES_RESULT_INVALID;

  weed_funcptr_t func = fdef->function;

  if (!func) return LIVES_RESULT_INVALID;

  lives_result_t res = LIVES_RESULT_SUCCESS;

  int sjval = 0;
  jmp_buf env;

  weed_seed_t ret_type = fdef->return_type;

  // get args fmt here. Values are taken from parameter seed_types in funcinst, which may not always match
  // fdef->funcsig, as that can be variadic '*' at end.
  // Custom types are substituted with 'V', so that we dont need  an infinte amount of funcsig types.
  // custom tyoes listed in _ARGS_FMT_REPLACE are allowed as param types and matched with 'V' / VOIDP
  // these tyoes will soo be allowed as return types, for now use VOIDPTR
  // For variadic functions, args_fmt still needs to match a known type
  // a second method (function wrapping) is in testing, it can avoid the need to register the params format
  // passing a va_list is possible provided it doesnt go out of scope. It should first be embedded in a va_surprise, then
  // it can be passed as a void *

  char *args_fmt = get_args_fmt(finst->params);
  funcsig_t sig = funcsig_from_args_fmt(args_fmt);
  int nparms = get_funcsig_nparms(sig);
  allfunc_t thefunc;

  // need to set this in case return type is void
  weed_error_t err = WEED_SUCCESS;

  boolean is_lpt = FALSE;

  if (args_fmt) lives_free(args_fmt);

  if (ret_type == LIVES_SEED_CONST_CHARPTR)
    ret_type = WEED_SEED_VOIDPTR;

  thefunc.func = func;

  if (MODULE_TYPE_IS(finst, LPT)) is_lpt = TRUE;
  if (is_lpt) sjval = sigsetjmp(env, 1);

  if (!sjval) {
    if (is_lpt) LPT_DATA(finst, lj_stack) =
        lives_sync_list_push(LPT_DATA(finst, lj_stack), (void *)&env);
    // if we cancel or error, this call never returns, instead we hit the else

    switch (nparms) {
    case 0:
      switch (sig) {
        ZERO_PARAM_FUNCSIG
      default: return LIVES_RESULT_INVALID;
      }
      break;
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
  } else {
    /* point of return for threads if they cancel / error */
  }

  if (MODULE_TYPE_IS(finst, LPT)) is_lpt = TRUE;

  if (is_lpt && LPT_DATA(finst, lj_stack))
    lives_sync_list_pop(&(LPT_DATA(finst, lj_stack)));

  if (is_lpt)
    lpt = LPT_DATA(finst, runner);
  if ((lpt && lives_proc_thread_was_cancelled(lpt))
      || lives_funcinst_error_state(finst)
      || err != WEED_SUCCESS) {
    res = LIVES_RESULT_ERROR;
    if (lpt && lives_proc_thread_had_error(lpt)) {
      int errnum = lives_proc_thread_get_errnum(lpt);
      int errsev = lives_proc_thread_get_errsev(lpt);
      const char *errmsg = lives_proc_thread_get_errmsg(lpt);
      int errline = lives_proc_thread_get_line_ref(lpt);
      const char *errfile = lives_proc_thread_get_file_ref(lpt);
      lives_make_errmsg_full(lpt, errfile, errline, errsev, errnum, errmsg);
    } else if (lpt && lives_proc_thread_was_cancelled(lpt))
      res = LIVES_RESULT_CANCELLED;
  }

  if (res != LIVES_RESULT_SUCCESS) return res;

  if (finst->retloc) {
    if (weed_plant_has_leaf(finst->params, _RV_)) {
      weed_leaf_get(finst->params, _RV_, 0, finst->retloc);
    }
  }

  free_finst_paramdata(finst, FALSE);
  return LIVES_RESULT_SUCCESS;
}

//#define DEBUG_FN_CALLBACKS
boolean call_funcsig(lives_proc_thread_t lpt) {
  /// funcsigs define the signature of any function we may wish to call via lives_proc_thread
  /// however since there are almost 3 quadrillion possibilities (nargs < 16 * all return types)
  /// it is not feasible to add every one; new funcsigs can be added as needed; then the only remaining thing is to
  /// ensure the matching case is handled in the switch statement
  uint64_t attrs = lives_proc_thread_get_attrs(lpt);
  weed_error_t err = WEED_SUCCESS;
  lives_funcinst_t *finst;
  lives_result_t res;
  char *msg, *tmp;

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

  //g_print("func call is %s\n", lives_funcinst_show_func_call(finst));

  // STATE CHANGED - queued / preparing -> running
  lives_proc_thread_include_states(lpt, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(lpt, THRD_STATE_QUEUED | THRD_STATE_UNQUEUED);

  if (attrs & LIVES_THRDATTR_NOTE_TIMINGS)
    weed_set_int64_value(lpt, LIVES_LEAF_START_TICKS, lives_get_current_ticks());

  if (lpt == mainw->debug_ptr) g_print("nrefs PPPmmmmm = %d\n", lives_proc_thread_count_refs(lpt));

  res = do_call(finst);

  //g_print("func call was %s\n", lives_funcinst_show_func_call(finst));

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
  validate_args_fmt((tmp = get_args_fmt(finst->params)), finst->funcdef->funcname, finst->paramnames);
  lives_free(tmp);
  lives_proc_thread_unref(lpt);
  return LIVES_RESULT_INVALID;

funcerr2:
  lives_proc_thread_error(123, LPT_ERR_CRITICAL, "Got error %d running function %s with type "
                          "0x%016lX", err, finst->funcdef->funcname,
                          finst->funcdef->funcsig);

  lives_proc_thread_unref(lpt);
  return LIVES_RESULT_INVALID;
}


LIVES_GLOBAL_INLINE funcsig_t funcsig_from_args_fmt(const char *args_fmt) {
  funcsig_t fsig = 0;
  if (args_fmt && *args_fmt) {
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
    args_fmt = lives_strdup_concat_sep(args_fmt, NULL, "%s", it);
  }
  return args_fmt;
}


uint8_t symname_to_sigbits(const char *symname) {
  for (int j = 0; crossrefs[j].typeletter; j++) {
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

LIVES_GLOBAL_INLINE uint64_t get_hs_op_flags(int hstype) {
  uint64_t opflags = 0;
  const hook_stack_descriptor_t *hsdesc = get_hs_desc(hstype);
  opflags = hsdesc->op_flags;
  HOOKSTACK_FLAGS_ADJUST(opflags);
  return opflags;
}


LIVES_LOCAL_INLINE void expire_all_rcpts(lives_funcinst_t *finst) {
  LiVESList *list, *listnext;
  for (list = (LiVESList *)CL_DATA(finst, receipts); list; list = listnext) {
    hook_cb_receipt *xrcpt = (hook_cb_receipt *)list->data;
    pthread_mutex_t *xmutex = xrcpt->status_mutex;
    listnext = list->next;
    pthread_mutex_lock(xmutex);
    if (!lives_cb_receipt_set_expired(list->data)) {
      pthread_mutex_unlock(xmutex);
      pthread_mutex_destroy(xmutex);
      lives_free(xmutex);
    } else pthread_mutex_unlock(xmutex);
  }
  lives_list_free((LiVESList *)CL_DATA(finst, receipts));
  CL_DATA(finst, receipts) = NULL;
}


void remove_from_hstack(lives_hook_stack_t *hstack, LiVESList * list) {
  // should be called with hstack mutex LOCKED !
  // remove list from htack, free the closure
  lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
  //g_print("removing %p from %p %p and free %p\n", list, hstack, hstack->stack, finst);
  hstack->stack = (volatile LiVESList *)lives_list_remove_node
                  ((LiVESList *)hstack->stack, list, FALSE);
  expire_all_rcpts(finst);
  CL_DATA(finst, hstack) = NULL;
  lives_funcinst_free(finst);
  //g_print("REM %p / %p from %p\n", list, finst, hstack);
}


LIVES_GLOBAL_INLINE boolean has_hook_cbs(lives_hook_stack_t **hstacks, int hstype) {
  return (hstacks && hstype >= 0 && hstype < N_HOOK_POINTS &&
          hstacks[hstype] && hstacks[hstype]->stack);
}


LIVES_GLOBAL_INLINE boolean lives_obj_instance_has_hook_cbs(lives_obj_instance_t *obj, int hstype) {
  if (obj) {
    lives_hook_stack_t *hstack = lives_obj_instance_find_hook_stack(obj, hstype);
    return hstack && hstack->stack;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE void lives_hook_stack_clear(lives_hook_stack_t *hstack) {
  if (hstack) {
    pthread_mutex_t *hmutex = &(hstack->mutex);
    LiVESList *cblist, *cb_next;

    if (hstack->stack) {
      if (hstack->flags & HS_FLAG_TRIGGERING) {
        if (hstack->owner_act_src_type == ACTION_SOURCE_LPT
            || hstack->owner_act_src_type == ACTION_SOURCE_OBJ) {
          int hstype = hstack->type;
          uint64_t hs_op_flags = get_hs_op_flags(hstype);
          if ((hs_op_flags & HOOKSTACK_ASYNC)
              && (hs_op_flags & HOOKSTACK_PARALLEL)) {
            lives_hook_async_cancel(hstype);
          }
        }

        while (1) {
          lives_microsleep_until_zero(hstack->flags & HS_FLAG_TRIGGERING);
          PTMLH;
          if (hstack->flags & HS_FLAG_TRIGGERING) PTMUH;
          else break;
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
        lives_hook_stack_clear(hstacks[i]);
        pthread_mutex_destroy(&hstacks[i]->mutex);
        lives_free(hstacks[i]);
        hstacks[i] = NULL;
      }
    }
}


LIVES_GLOBAL_INLINE void lives_obj_instance_clear_all_hook_stacks(lives_obj_instance_t *obj) {
  if (obj) {
    lives_index_t *idx = lives_obj_instance_get_hook_stacks(obj);
    if (idx) {
      char *key;
      lives_hook_stack_t *hstack;
      LIVES_INDEX_FOREACH(idx, key, hstack,
                          lives_hook_stack_clear(hstack));
    }
  }
}


static boolean unblock_waiter(void *receipt, void *data) {
  // if !paused, stop it from doing do
  int reply = lives_cb_receipt_get_req_reply(receipt);
  lives_proc_thread_t adder = lives_cb_receipt_get_adder(receipt);
  if (!adder) return FALSE;
  if (reply == LIVES_REPLY_YES || reply == LIVES_REPLY_NO) return FALSE;

  // setting state directly will work in every case - if adder is not paused it will
  // prevent it from pausing and it will clear the request

  lives_proc_thread_ensure_resume(adder);
  lives_hook_cb_remove(receipt);
  return FALSE;
}


// hook cb's and receipts

LIVES_GLOBAL_INLINE lives_proc_thread_t lives_cb_receipt_get_adder(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->adder : NULL;
}


LIVES_GLOBAL_INLINE int lives_cb_receipt_get_req_reply(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->req_reply : LIVES_REPLY_INVALID;
}

LIVES_GLOBAL_INLINE int lives_cb_receipt_get_nrefs(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? rcpt->nrefs : -1;
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_ref(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->nrefs++;
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_unref(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->nrefs--;
}


LIVES_GLOBAL_INLINE boolean lives_cb_receipt_check_expired(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? !!(rcpt->status & RCPT_EXPIRED) : FALSE;
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_block_cb(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt && rcpt->finst) lives_hook_cb_block(rcpt->finst);
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_unblock_cb(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt && rcpt->finst) lives_hook_cb_block(rcpt->finst);
}


LIVES_GLOBAL_INLINE boolean lives_cb_receipt_is_in_list(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? !!(rcpt->status & RCPT_IN_LIST) : FALSE;
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_set_adder(void *receipt, lives_proc_thread_t adder) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    rcpt->adder = adder;
    rcpt->status |= RCPT_HAS_ADDER;
  }
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_set_reply(void *receipt, int reply) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) rcpt->req_reply = reply;
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_set_in_list(void *receipt, boolean in_list) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    if (in_list) rcpt->status |= RCPT_IN_LIST;
    else rcpt->status &= ~RCPT_IN_LIST;
  }
}


LIVES_GLOBAL_INLINE void lives_cb_receipt_set_reply_callback(void *receipt, reply_sent_cb_f reply_sent_cb, void *user_data) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  if (rcpt) {
    rcpt->reply_cb = reply_sent_cb;
    rcpt->reply_cb_data = user_data;
  }
}


LIVES_GLOBAL_INLINE boolean lives_cb_receipt_has_reply_callback(void *receipt) {
  hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
  return rcpt ? !!rcpt->reply_cb : FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_cb_receipt_call_reply_callback(void *receipt) {
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
  rcpt->finst = NULL;
  if (lives_cb_receipt_free(rcpt) == LIVES_RESULT_SUCCESS)
    receipt = NULL;
  return receipt;
}


static boolean check_if_can_remove(lives_funcinst_t *finst, LiVESList * list, boolean expire_extra) {
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
    } else pthread_mutex_unlock(xmutex);

    CL_DATA(finst, receipts) =
      lives_list_remove_node((LiVESList *)CL_DATA(finst, receipts),
                             list, FALSE);
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
  LiVESList *rcpt, *rcpt_next;
  for (rcpt = (LiVESList *)CL_DATA(finst, receipts); rcpt; rcpt = rcpt_next) {
    rcpt_next = rcpt->next;
    check_if_can_remove(finst, rcpt, force);
  }

  if (!CL_DATA(finst, receipts))
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_REMOVE;

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
  for (LiVESList *list = rcpts; list; list = list->next) {
    hook_cb_receipt *rcpt = (hook_cb_receipt *)list->data;
    rcpt->finst = dst;
  }
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
      } else {
        if (reply == lives_cb_receipt_get_req_reply(rcpt)) continue;
        lives_cb_receipt_set_reply(rcpt, reply);
      }

      if (lives_cb_receipt_has_reply_callback(rcpt)) {
        boolean tryagain = lives_cb_receipt_call_reply_callback(rcpt);
        if (tryagain) {
          if (!retry) retries = lives_list_append(retries, rcpt);
        } else {
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
  } else pthread_mutex_unlock(xmutex);

  return LIVES_RESULT_SUCCESS;
}


LIVES_LOCAL_INLINE void lives_funcinst_add_receipt(lives_funcinst_t *finst, void *receipt) {
  if (finst && receipt) {
    hook_cb_receipt *rcpt = (hook_cb_receipt *)receipt;
    CL_DATA(finst, receipts) = lives_list_prepend((LiVESList *)CL_DATA(finst, receipts), receipt);
    rcpt->finst = finst;
  }
}



uint64_t cbflags_for_hs_op_flags(uint64_t hs_op_flags) {
  uint64_t cbflags = 0;
  APPLY_BIT_TRANSFORMS(HS_OP_FLAGS, CBFLAGS, hs_op_flags, cbflags)
  return cbflags;
}


static void finst_disposition_pop(lives_funcinst_t *finst) {
  funcinst_module_t *modt;
  void *old_module;
  int oldt;

  if (!finst) return;

  pthread_rwlock_wrlock(&finst->dispolock);

  old_module = finst->module;
  oldt = finst->mod_type;

  modt = (funcinst_module_t *)lives_sync_list_pop(&finst->modules);
  if (modt) {
    finst->mod_type = modt->mod_type;
    finst->module = modt->module_data;
    finst->disposition = modt->disposition;
    lives_free(modt);
  } else {
    finst->mod_type = module_none;
    finst->module = NULL;
    finst->disposition = DISPOSITION_READY;
  }
  pthread_rwlock_unlock(&finst->dispolock);

  if (old_module) finst_module_free(old_module, oldt);
}


boolean set_param_symbolic(const char *datasrc, weed_seed_t st, int pcount, lives_funcinst_t *finst) {
  char *pkey = NULL;
  boolean ret = TRUE;

  if (!*datasrc || !datasrc[1]) goto fail;

  const char *item = (const char *)(datasrc + 1);
  pkey = make_std_pname(pcount);

  switch (*datasrc) {
  case '-': {
    break;
  }
  case '$': {
    // data comes from local data book
    if (GET_BOOK_DATATYPE(lives_local_databook(), item) != st) goto fail;
    leaf_from_allvalues(finst->params, pkey, get_local_book_item(item));
    weed_leaf_set_undeletable(finst->params, pkey, TRUE);
    break;
  }
  case '@': {
    // data comes from global data book
    if (GET_BOOK_DATATYPE(global_databook, item) != st) goto fail;
    leaf_from_allvalues(finst->params, pkey, get_global_book_item(item));
    weed_leaf_set_undeletable(finst->params, pkey, TRUE);
    break;
  }
  default: break;
  }

  goto success;

fail: ret = FALSE;
success:
  if (pkey) lives_free(pkey);
  return ret;
}


void *lives_hook_cb_add_funcinst_full(lives_hook_stack_t *hstack, lives_funcinst_t *finst,
                                      uint64_t cbflags, uint64_t addmode, ...) {
  if (!finst) return NULL;

  lives_funcinst_t *ret_finst = NULL;
  pthread_mutex_t *hmutex;
  void *receipt = NULL;
  LiVESList *cblist, *cblistnext;
  uint64_t hs_op_flags, xflags, extra_cb_flags;

  boolean have_lock = FALSE;
  boolean is_self_stack = FALSE;
  boolean fmatch;
  boolean is_append = TRUE, is_remove = FALSE, no_add = FALSE, no_remove = FALSE;
  boolean temp_module = FALSE;

  int maxp;

  GET_PROC_THREAD_SELF(self);

  if (!(addmode & _ADDMODE_NORCPT)) receipt = lives_cb_receipt_new();
  if (addmode & _ADDMODE_HAVE_LOCK) have_lock = TRUE;
  if (addmode & _ADDMODE_NOADD) no_add = TRUE;
  if (addmode & _ADDMODE_NOREMOVE) no_remove = TRUE;

  if (!have_lock) {
    pthread_rwlock_rdlock(&finst->dispolock);
    LIVES_ASSERT(!receipt || finst->disposition != DISPOSITION_STACKED);
    if (finst->disposition != DISPOSITION_STACKED) {
      pthread_rwlock_unlock(&finst->dispolock);
      if (!receipt) temp_module = TRUE;
      lives_funcinst_set_disposition(finst, FALSE, DISPOSITION_STACKED);
      CL_DATA(finst, cb_flags) = cbflags;
    } else pthread_rwlock_unlock(&finst->dispolock);
  }

  if (!finst || !hstack) {
    if (receipt) lives_cb_receipt_set_reply(receipt, LIVES_REPLY_INVALID);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    if (temp_module) finst_disposition_pop(finst);
    return receipt;
  }

  if (!no_add) CL_DATA(finst, hstack) = hstack;

  if (!hstack) {
    if (receipt) lives_funcinst_send_replies(finst, LIVES_REPLY_INVALID);
    if (finst) finst->flags |= FINST_FLAG_REJECTED;
    if (temp_module) finst_disposition_pop(finst);
    return receipt;
  }

  if (addmode == ADDMODE_UPD_LINKED) is_remove = TRUE;

  finst->flags &= ~FINST_FLAG_REJECTED;

  cbflags = CL_DATA(finst, cb_flags);
  xflags = cbflags & (HOOK_UNIQUE_REPLACE | HOOK_INVALIDATE_DATA | HOOK_TOGGLE_FUNC);

  if ((cbflags & HOOK_OPT_PRIORITY) || (addmode & _ADDMODE_FORCE_PREPEND))
    is_append = FALSE;

  if (receipt) lives_funcinst_add_receipt(finst, receipt);

  hs_op_flags = get_hs_op_flags(hstack->type);

  if (hstack->owner_act_src_type == ACTION_SOURCE_LPT && hstack->owner.lpt == self)
    is_self_stack = TRUE;

  if (!is_self_stack && !is_remove && (hs_op_flags & HOOKSTACK_SELF_ONLY)) {
    if (receipt) {
      lives_cb_receipt_set_expired(receipt);
      lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
    }
    if (finst) finst->flags |= FINST_FLAG_NOPERM | FINST_FLAG_REJECTED;
    if (temp_module) finst_disposition_pop(finst);
    return receipt;
  }

  extra_cb_flags = cbflags_for_hs_op_flags(hs_op_flags);
  cbflags |= extra_cb_flags;

  CL_DATA(finst, cb_flags) = cbflags;

  // if append, then everything else will check
  if (is_append) xflags &= ~(HOOK_INVALIDATE_DATA | HOOK_OPT_MATCH_CHILD);

#ifdef HAVE_COND_EVAL
  const hook_stack_descriptor_t *hsdesc = get_hs_desc(hstype);
  if ((hstack->hsdesc->accept_cond && !lives_cond_eval(hsdesc->accept_cond))
      || (hstack->reject_cond && lives_cond_eval(hstack->reject_cond))) {
    if (receipt) {
      lives_funcinst_send_replies(finst, LIVES_REPLY_NO);
      lives_cb_receipt_set_expired(receipt);
    }
    if (finst) finst->flags |= FINST_FLAG_NOT_ACCEPTED | FINST_FLAG_REJECTED;
    if (temp_module) finst_disposition_pop(finst);
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
    uint64_t xcbflags;
    uint64_t cfinv = 0;
    lives_funcinst_t *ret_finst = NULL;
    lives_funcinst_t *xfinst = (lives_funcinst_t *)cblist->data;
    cblistnext = cblist->next;
    if (!xfinst) continue;

    xcbflags = CL_DATA(xfinst, cb_flags);
    if (xcbflags & (HOOK_STATUS_IGNORE
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
          if (!ret_finst) {
            // denied !
            ret_finst = xfinst;
            break;
          }
          if (!no_remove) {
            expel_from_stack(xfinst, ret_finst);
            remove_from_hstack(hstack, cblist);
          }
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
      if (!no_add) {
        // for unique data, append our func / data after first match
        // then we will expel the match
        // if this was a prepend, ret_finst is set already, so we will skip this
        if (!ret_finst && !(xflags & HOOK_TOGGLE_FUNC)) {
          hstack->stack = lives_list_append(cblist, (void *)finst);
        }
      }
      if (!ret_finst) ret_finst = finst;
    }
    // unique func
    if (!no_remove && ret_finst != xfinst) {
      expel_from_stack(xfinst, ret_finst);
      remove_from_hstack(hstack, cblist);
    }
  }

  if (no_add) {
    if (!have_lock) PTMUH;
    if (temp_module) finst_disposition_pop(finst);
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
    } else lives_funcinst_send_replies(finst, LIVES_REPLY_YES);
    if (temp_module) finst_disposition_pop(finst);
    return receipt;
  }

  if (receipt && (CL_DATA(finst, cb_flags) & HOOK_CB_PERSISTENT)) {
    hook_cb_receipt *xrcpt = (hook_cb_receipt *)receipt;
    xrcpt->status |= RCPT_PERSISTENT;
  }


  if (addmode == ADDMODE_NORMAL) {
    if (hstack->owner_act_src_type == ACTION_SOURCE_LPT
        || hstack->owner_act_src_type == ACTION_SOURCE_OBJ)
      SET_SELF_VALUE(WEED_SEED_PLANTPTR, LDB_TARGET_OBJECT, hstack->owner.lpt);

    va_list va;
    va_start(va, addmode);
    va_surprise *magic = va_arg(va, va_surprise *);
    va_end(va);
    if (magic) {
      const hook_stack_descriptor_t *hsdesc = get_hs_desc(hstack->type);
      lives_condition C = NULL;
      int pcount = 0;
      char *tmp;
      boolean has_ffuncs = FALSE;

      if (cbflags & HOOK_CB_HAS_FREEFUNCS) has_ffuncs = TRUE;

      // add predefined const data here
      for (LiVESList *l = hsdesc->const_data_srcs; l; l = l->next) {
        weed_seed_t st;
        const char *datasrc = (const char *)l->data;
        // format is "X|Ysrcname", where X is seed_type, Y is origin
        if (!datasrc[0] || !datasrc[1] || !datasrc[2] || !datasrc[3]) continue;
        if (datasrc[1] != '|' || (datasrc[2] != '-' && datasrc[2] != '$' && datasrc[2] != '@')) continue;
        st = get_seedtype(datasrc[0]);
        if (st == WEED_SEED_INVALID) continue;
        if (!set_param_symbolic((const char *)(datasrc + 2), st, pcount, finst)) {
          if (receipt) lives_funcinst_send_replies(finst, LIVES_REPLY_INVALID);
          if (finst) finst->flags |= FINST_FLAG_MISSING_CONST | FINST_FLAG_REJECTED;
          g_print("Error adding %s to hook stack %d in lpt %p\n"
                  "%s not defined !\n", get_func_call(finst), hstack->type,
                  hstack->owner.lpt, desc_bookval(datasrc[0], datasrc + 2));
          if (temp_module) finst_disposition_pop(finst);
          if (!have_lock) PTMUH;
          return receipt;
        } else {
          weed_set_int_value(finst->params, LIVES_LEAF_MAXPARAM,
                             weed_get_int_value(finst->params, LIVES_LEAF_MAXPARAM, NULL) + 1);
        }
        pcount++;
      }

      if (cbflags & HOOK_CB_CONDITIONAL) C = va_arg(magic->va, lives_condition);

      const char *args_fmt = va_arg(magic->va, const char *);

      if (args_fmt) {
        for (int i = 0; args_fmt[i]; i++) {
          weed_plant_params_from_valist(finst->params, args_fmt + i, make_std_pname, &pcount, 1, magic->va);
          if (has_ffuncs) {
            lives_funcinst_t *free_finst = va_arg(magic->va, lives_funcinst_t *);
            if (free_finst) {
              char *fpkey = lives_strdup_printf("p%d_free", pcount);
              lives_funcinst_set_disposition(free_finst, FALSE, DISPOSITION_CONTINGENCY);
              CONTINGENCY_DATA(free_finst, src_status) = SRC_STATUS_READY;
              weed_set_voidptr_value(finst->params, fpkey, free_finst);
              lives_free(fpkey);
	      // *INDENT-OFF*
            }}}}
      // *INDENT-ON*
      validate_args_fmt((tmp = get_args_fmt(finst->params)), finst->funcdef->funcname, finst->paramnames);
      lives_free(tmp);
      if (C) CL_DATA(finst, trigger_cond) = C;
    }
  }
  lives_funcinst_send_replies(finst, LIVES_REPLY_YES);

  if (is_append) {
    if (!ret_finst) hstack->stack = lives_list_append((LiVESList *)hstack->stack, finst);
  } else hstack->stack = lives_list_prepend((LiVESList *)hstack->stack, finst);

  if (self) CL_DATA(finst, orig_adder) = self;

  if (addmode == ADDMODE_NORMAL) {
    if (hstack->owner_act_src_type == ACTION_SOURCE_LPT
        || hstack->owner_act_src_type == ACTION_SOURCE_OBJ)
      DEL_BOOK_VALUE(lives_local_databook(), LDB_TARGET_OBJECT);
  }

  if (!have_lock) PTMUH;

  /* if (lives_proc_thread_get_attrs(self) & LIVES_THRDATTR_NOTE_TIMINGS) */
  /*   weed_set_int64_value(self, LIVES_LEAF_QUEUED_TICKS, lives_get_current_ticks()); */
  //}
  if (temp_module) finst_disposition_pop(finst);
  return receipt;
}


static void *lives_hook_cb_add_funcinst_va(lives_hook_stack_t *hstack, lives_funcinst_t *finst,
    uint64_t cbflags, va_surprise * wand) {
  // funcinst may be blocked from adding for a variety of reasons
  // if superceded by another funcinst, we return the receipt, if not replaced, we return NULL
  // and in both cases, FINST_FLAG_REJECTED is set

  GET_PROC_THREAD_SELF(self);

  // block funcinst from being called until we set in cb_added_list

  void *receipt = lives_hook_cb_add_funcinst_full(hstack, finst, cbflags | HOOK_STATUS_NOTREADY,
                  ADDMODE_NORMAL, wand);
  int reply = lives_cb_receipt_get_req_reply(receipt);
  if (reply == LIVES_REPLY_YES) {
    // reply can be YES even if func inst was rejected
    // in this case finst was blocked, but receipt has been attached to blocker
    lives_cb_receipt_add_to_list(receipt);
    if (cbflags & HOOK_CB_BLOCKING)
      lives_cb_receipt_set_reply_callback(receipt, unblock_waiter, NULL);
  }

  lives_cb_receipt_set_adder(receipt, self);

  if (finst->flags & FINST_FLAG_REJECTED) {
    lives_funcinst_free(finst);
    receipt = NULL;
  } else {
    cbflags = CL_DATA(finst, cb_flags);
    CL_DATA(finst, cb_flags) = cbflags & ~HOOK_STATUS_NOTREADY;
  }
  return receipt;
}


void *lives_hook_cb_add_funcinst(lives_hook_stack_t *hstack, lives_funcinst_t *finst, uint64_t cbflags) {
  return lives_hook_cb_add_funcinst_va(hstack, finst, cbflags, NULL);
}


// returns receipt. or NULL if add condition failed, or it was toggled out
// calls hook_cb_add_funci
void *_lives_hook_cb_add_full(lives_hook_stack_t *hstack, uint64_t cbflags, lives_funcptr_t func,
                              const char *fname, int return_type, const char **anames, ...) {
  // create funcinst, no params
  void *rcpt = NULL;
  lives_funcinst_t *finst = lives_funcinst_create_named(func, fname, return_type, NULL, NULL, NULL);
  va_surprise va_magic;
  va_surprise *va_wand = NULL;
  boolean has_cond = FALSE, has_af = FALSE;
  va_start(va_magic.va, anames);
  if (cbflags & HOOK_CB_CONDITIONAL) has_cond = TRUE;
  else {
    va_list vc;
    va_copy(vc, va_magic.va);
    if (va_arg(vc, const char *)) has_af = TRUE;
    va_end(vc);
  }

  if (has_af || has_cond) va_wand = &va_magic;
  rcpt = lives_hook_cb_add_funcinst_va(hstack, finst, cbflags, va_wand);

  if (va_wand) va_end(va_magic.va);
  return rcpt;
}


void fg_deferral_remove_persistent(void) {
  lives_hook_stack_t *hstack = mainw->global_hook_stacks[LIVES_GUI_HOOK];
  pthread_mutex_t *hmutex = &(hstack->mutex);
  LiVESList *list, *listnext, *list2, *list2next;
  PTMLH;
  for (list = (LiVESList *)hstack->stack; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;
    if (!finst) continue;
    LiVESList *rcpts = (LiVESList *)CL_DATA(finst, receipts);
    for (list2 = rcpts; list2; list2 = list2next) {
      hook_cb_receipt *xrcpt = (hook_cb_receipt *)list2->data;
      pthread_mutex_t *xmutex = xrcpt->status_mutex;
      list2next = list2->next;
      pthread_mutex_lock(xmutex);
      if (xrcpt->status & HOOK_CB_PERSISTENT) {
        xrcpt->status &= ~HOOK_CB_PERSISTENT;
        if (!lives_cb_receipt_is_in_list(xrcpt)) {
          if (!lives_cb_receipt_set_expired(xrcpt)) {
            pthread_mutex_unlock(xmutex);
            pthread_mutex_destroy(xmutex);
            lives_free(xmutex);
          }
          CL_DATA(finst, receipts) =
            lives_list_remove_node((LiVESList *)CL_DATA(finst, receipts),
                                   list2, FALSE);
        } else pthread_mutex_unlock(xmutex);
      } else pthread_mutex_unlock(xmutex);
    }
    if (!CL_DATA(finst, receipts)) remove_from_hstack(hstack, list);
  }
  PTMUH;
}


static lives_result_t _lives_hook_trigger_va(lives_hook_stack_t *hstack, const char *args_fmt, va_list va) {
  lives_databook_t *lbook;
  LiVESList *list, *listnext;
  pthread_mutex_t *hmutex;
  LiVESList *rcpts;
  int reply = LIVES_REPLY_MU;
  boolean retval = TRUE;
  boolean hmulocked = FALSE;
  lives_hook_stack_t *req_stack = NULL;
  uint64_t hs_op_flags, cbflags;
  boolean emitted = FALSE;
  int np;
  //if (hstype == SYNC_ANNOUNCE_HOOK) dump_hook_stack(hstacks, hstype);

  if (!hstack) return LIVES_RESULT_ERROR;

  if (hstack->flags & HS_FLAG_INVALID) return LIVES_RESULT_INVALID;

  int hstype = hstack->type;

  // hs_op_flags come from the hook stack descriptor, and define the
  // operational details of the stack
  hs_op_flags = get_hs_op_flags(hstype);

  // should use async / async_parallel
  if (hs_op_flags & HOOKSTACK_ASYNC)
    return LIVES_RESULT_ERROR;

  GET_PROC_THREAD_SELF(self);
  if ((hstack->owner_act_src_type == ACTION_SOURCE_LPT
       && hstack->owner.lpt != self)
      || (hstack->owner_act_src_type == ACTION_SOURCE_THREAD
          && hstack->owner.thread != pthread_self()))
    return LIVES_RESULT_NOPERM;

  hmutex = &(hstack->mutex);

  if (hstype != FATAL_HOOK) {
    // skip mutex locking for the global FATAL hook
    PTMLH;
    hmulocked = TRUE;
  }

  if (!hstack->stack || (hstack->flags & HS_FLAG_TRIGGERING)) {
    if (hstype != FATAL_HOOK) PTMUH;
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
      if (hstype != FATAL_HOOK) PTMUH;
      hmulocked = FALSE;
      goto trigdone;
    }
    pthread_mutex_lock(&req_stack->mutex);
  }

  list = (LiVESList *)hstack->stack;

  // while running the callbacks, set src_object to hstack owner
  lbook = lives_local_databook();
  lives_databook_emit(lbook, self);
  emitted = TRUE;
  lives_databook_descend(lbook);

  // mark all entries in list at entry as "ACTIONED"
  // since we may parse the list several times, we only check those which are present now
  // this avoids a situation where we would be endlessly traversing the list as new items are added
  for (; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    //g_print("clearing %p from %p %p and free %p\n", list, hstack, hstack->stack, finst);
    listnext = list->next;
    if (!finst) continue;
    cleanup_funcinst_receipts(finst, FALSE);

    cbflags = CL_DATA(finst, cb_flags);
    if (cbflags & HOOK_STATUS_NOTREADY) {
      continue;
    }
    if (cbflags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstack, list);
      continue;
    }
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_ACTIONED;
  }

  retval = FALSE;
  if (hstype != FATAL_HOOK)
    if (!hmulocked) {
      if (hs_op_flags & HOOKSTACK_NOWAIT) {
        if (PTMTLH) goto trigdone;
      } else PTMLH;
      hmulocked = TRUE;
    }

  retval = TRUE;

  for (list = (LiVESList *)hstack->stack; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;
    if (!finst) continue;

    cbflags = CL_DATA(finst, cb_flags);

    /* if (cbflags & HOOK_OPT_ADDER_RUNS) { */
    /* 	if (cbflags & HOOK_STATUS_RUNNING) { */
    /* 	  if (lives_proc_thread_get_active_funcinst(lpt) == finst) continue; */
    /* 	} */
    /* 	cbflags &= ~HOOK_STATUS_RUNNING; */
    /* 	CL_DATA(finst, triggerer.lpt) = ACTION_SOURCE_NONE; */
    /* } */

    if (cbflags & (HOOK_CB_IGNORE)) {
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
      cbflags &= ~HOOK_STATUS_ACTIONED;
      cbflags &= ~hstack->req_target_unset_flags;
      cbflags |= hstack->req_target_set_flags;
      CL_DATA(finst, cb_flags) = cbflags;
      //lives_proc_thread_show_func_call(closure->proc_thread);
      lives_hook_cb_add_funcinst_full(hstack->req_target_stacks[hstack->req_target_type],
                                      finst, cbflags, ADDMODE_TRANSFER);
      // 3 results - cb accepted, cb rejected, rcpt transferred, cb rejected, rcpt not transferred
      // action: remove but no free; free finst, leave rcpt; rem cb frm cb_add list free finst
      if (finst->flags & FINST_FLAG_REJECTED) {
        remove_from_hstack(hstack, list);
        cleanup_self_receipts();
      } else hstack->stack =
          (volatile LiVESList *)lives_list_remove_node((LiVESList *)hstack->stack,
              list, FALSE);
      continue;
    }

    if (hstype >= N_NATIVE_HOOKS) {
      GET_PROC_THREAD_SELF(self);
      CL_DATA(finst, triggerer.lpt) = self;
      CL_DATA(finst, trigger_act_src_type) = ACTION_SOURCE_LPT;
    } else {
      CL_DATA(finst, trigger_act_src_type) = ACTION_SOURCE_THREAD;
      CL_DATA(finst, triggerer.thread) = pthread_self();
    }

    if (CL_DATA(finst, trigger_cond)) {
      if (lives_cond_eval(CL_DATA(finst, trigger_cond))
          != LIVES_COND_PASS) {
        cbflags &= ~HOOK_STATUS_ACTIONED;
        CL_DATA(finst, cb_flags) = cbflags;
        continue;
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

    if (cbflags & HOOK_CB_ADDER_RUNS) {
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

    if (args_fmt && *args_fmt) {
      va_list vc;
      np = get_funcinst_nparams(finst);
      va_copy(vc, va);
      for (int i = 0; args_fmt[i]; i++) {
        char *pkey = make_std_pname(np + i);
        weed_leaf_set_distinguished(finst->params, pkey, TRUE);
        weed_set_int_value(finst->params, LIVES_LEAF_MAXPARAM, np + i + 1);
        lives_free(pkey);
      }
      va_end(vc);
    }

    if (hstype != FATAL_HOOK) PTMUH;
    hmulocked = FALSE;

    if (hstype < N_NATIVE_HOOKS) {
      do_call(finst);
      CL_DATA(finst, triggerer.thread) = ACTION_SOURCE_NONE;
    } else {
      if (!(cbflags & HOOK_CB_FG_THREAD) || is_fg_thread()) {
        // SELF RUN CALLBACK
        lives_funcinst_send_replies(finst, LIVES_REPLY_YES);
        lives_funcinst_execute(finst);
      } else {
        // this function will call fg_service_call directly,
        // block until the lpt completes or is cancelled
        lives_funcinst_fg_queue(finst, LIVES_THRDATTR_FG_THREAD);
      }
      CL_DATA(finst, triggerer.lpt) = ACTION_SOURCE_NONE;
    }

    rcpts = (LiVESList *)CL_DATA(finst, receipts);
    if (rcpts) reply = lives_cb_receipt_get_req_reply(rcpts->data);

    if (cbflags & HOOK_OPT_ONESHOT) cbflags |= HOOK_STATUS_REMOVE;

    if (hstype != FATAL_HOOK) PTMLH;
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
    } else {
      np = get_funcinst_nparams(finst);
      while (np--) {
        char *pkey = make_std_pname(np);
        if (weed_leaf_is_distinguished(finst->params, pkey)) {
          weed_leaf_delete(finst->params, pkey);
          weed_set_int_value(finst->params, LIVES_LEAF_MAXPARAM, np);
          lives_free(pkey);
        } else {
          lives_free(pkey);
          break;
        }
      }
    }
    if (hs_op_flags & HOOKSTACK_RUN_SINGLE) {
      for (list = listnext; list; list = list->next) {
        finst = (lives_funcinst_t *)list->data;
        //g_print("valXXX %p...%p %p %d\n", list, list->data, finst->module, finst->mod_type);
        LIVES_ASSERT(finst->module);
        cbflags = CL_DATA(finst, cb_flags);
        cbflags &= ~HOOK_STATUS_ACTIONED;
        CL_DATA(finst, cb_flags) = cbflags;
      }
      break;
    }
  }

  if (hmulocked) {
    if (hstype != FATAL_HOOK) PTMUH;
    hmulocked = FALSE;
  }

trigdone:

  if (req_stack) pthread_mutex_unlock(&req_stack->mutex);

  if (hstype != FATAL_HOOK && hmulocked) PTMUH;

  hstack->flags &= ~HS_FLAG_TRIGGERING;

  cleanup_self_receipts();

  if (emitted) {
    if (hstack->owner_act_src_type == ACTION_SOURCE_LPT
        || hstack->owner_act_src_type == ACTION_SOURCE_OBJ) {
      lbook = lives_local_databook();
      LIVES_ASSERT(lbook);
      lives_databook_end_emmission(lbook, self);
    }
  }

  //if (hstype == SYNC_WAIT_HOOK) g_print("sync all res: %d\n", retval);
  return retval ? LIVES_RESULT_SUCCESS : LIVES_RESULT_FAIL;
}


// when triggering a hook, there may be a def_args_fmt ftrom the stack descript
lives_result_t _lives_hook_trigger(lives_hook_stack_t *hstack, const char *args_fmt, ...) {
  lives_result_t res;
  va_list va;
  va_start(va, args_fmt);
  res = _lives_hook_trigger_va(hstack, args_fmt, va);
  va_end(va);
  return res;
}


int _lives_hook_trigger_async_va(lives_hook_stack_t *hstack, lives_proc_thread_t **xlpts, const char *args_fmt, va_list va) {
  LiVESList *list, *listnext;
  pthread_mutex_t *hmutex;
  uint64_t hs_op_flags, cbflags;
  int ncount = 0;
  lives_proc_thread_t *lpts = NULL, lpt;
  va_list vc;

  if (xlpts) *xlpts = NULL;
  hmutex = &(hstack->mutex);
  PTMLH;

  if (!hstack->stack) {
    PTMUH;
    return ncount;
  }

  int hstype = hstack->type;

  hs_op_flags = get_hs_op_flags(hstype);

  if (!((hs_op_flags & HOOKSTACK_ASYNC)
        && (hs_op_flags & HOOKSTACK_PARALLEL))) {
    PTMUH;
    return ncount;
  }

  list = (LiVESList *)hstack->stack;

  hstack->flags |= HS_FLAG_TRIGGERING;

  for (; list; list = listnext) {
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;
    if (!finst) continue;
    cleanup_funcinst_receipts(finst, FALSE);
    cbflags = CL_DATA(finst, cb_flags);
    if (cbflags & HOOK_STATUS_NOTREADY) continue;
    if (cbflags & HOOK_STATUS_REMOVE) {
      remove_from_hstack(hstack, list);
      continue;
    }
    CL_DATA(finst, cb_flags) |= HOOK_STATUS_ACTIONED;
  }

  list = (LiVESList *)hstack->stack;

  for (; list; list = listnext) {
    int np;
    lives_funcinst_t *finst = (lives_funcinst_t *)list->data;
    listnext = list->next;

    if (!finst) continue;
    cbflags = CL_DATA(finst, cb_flags);
    if (!(cbflags & HOOK_STATUS_ACTIONED)) continue;

    cbflags &= ~HOOK_STATUS_ACTIONED;

    if (cbflags & HOOK_STATUS_RUNNING) continue;

    if (cbflags & (HOOK_STATUS_REMOVE)) {
      remove_from_hstack(hstack, list);
      continue;
    }

    if (args_fmt && *args_fmt) {
      np = get_funcinst_nparams(finst);
      va_copy(vc, va);
      for (int i = 0; args_fmt[i]; i++) {
        weed_seed_t st = get_seedtype(args_fmt[i]);
        char *pkey = make_std_pname(np + i);
        weed_leaf_from_varg(finst->params, pkey, st, -1, vc);
        weed_leaf_set_distinguished(finst->params, pkey, TRUE);
        //g_print("set p %d D, np is %d\n", np + i, np + i + 1);
        weed_set_int_value(finst->params, LIVES_LEAF_MAXPARAM, np + i + 1);
        lives_free(pkey);
      }
      va_end(vc);
    }

    cbflags |= HOOK_STATUS_RUNNING;

    CL_DATA(finst, cb_flags) = cbflags;

    // assuming FG_THREAD is not set, then this will wrap finst in a lpt
    // and dispatch it to pool thread

    // NOTE: abscence of LIVES_THRDATT_FG_THREAD ensures this is sent to pool threads
    // and not to fg thread
    // while running the callbacks, set src_object to hstack owner
    // todo hold in queu till set

    lpt = lives_funcinst_bg_queue(finst, LIVES_THRDATTR_NO_HOOKS | LIVES_THRDATTR_PRIORITY | LIVES_THRDATTR_FAST_QUEUE);

    //g_print("queued lpt %p\n", lpt);

    if (xlpts) lives_dynarray_append(lpts, ncount, lpt);
    else ncount++;
  }
  PTMUH;

  if (!ncount) hstack->flags &= ~HS_FLAG_TRIGGERING;

  if (xlpts) *xlpts = lpts;
  return ncount;
}


// when triggering a hook, there may be a def_args_fmt ftrom the stack descript
lives_result_t _lives_hook_trigger_async(lives_hook_stack_t *hstack, lives_proc_thread_t **xlpts, const char *args_fmt, ...) {
  lives_result_t res;
  va_list va;
  va_start(va, args_fmt);
  res = _lives_hook_trigger_async_va(hstack, xlpts, args_fmt, va);
  va_end(va);
  return res;
}


lives_result_t _lives_proc_thread_trigger_hook(int hstype, const char *args_fmt, ...) {
  lives_result_t res;
  va_list va;
  va_start(va, args_fmt);
  res = _lives_hook_trigger_va(self_hook_stack(hstype), args_fmt, va);
  va_end(va);
  return res;
}


lives_result_t _lives_obj_instance_trigger_hook(lives_obj_instance_t *obj, int hstype, const char *args_fmt, ...) {
  lives_result_t res;
  va_list va;
  va_start(va, args_fmt);
  res = _lives_hook_trigger_va(lives_obj_instance_find_hook_stack(obj, hstype),
                               args_fmt, va);
  va_end(va);
  return res;
}


lives_result_t _lives_obj_instance_trigger_hook_async(lives_obj_instance_t *obj, int hstype,
    lives_proc_thread_t **xlpts, const char *args_fmt, ...) {
  lives_result_t res;
  va_list va;
  va_start(va, args_fmt);
  res = _lives_hook_trigger_async_va(lives_obj_instance_find_hook_stack(obj, hstype),
                                     xlpts, args_fmt, va);
  va_end(va);
  return res;
}


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


static boolean _lives_hook_async_join(lives_hook_stack_t *hstack, boolean cancel) {
  pthread_mutex_t *hmutex;
  LiVESList *cblist, *cblist_next;
  uint64_t hs_op_flags, cbflags;

  if (!(hstack->flags & HS_FLAG_TRIGGERING)) return TRUE;

  hmutex = &(hstack->mutex);
  PTMLH;

  int hstype = hstack->type;
  hs_op_flags = get_hs_op_flags(hstype);

  if (!(hs_op_flags & HOOKSTACK_ASYNC)) {
    PTMUH;
    return TRUE;
  }

  for (cblist = (LiVESList *)hstack->stack; cblist; cblist = cblist_next) {
    int np;
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

    //g_print("check finst %p, disp us %d\n", finst, finst->disposition);

    if (finst->disposition != DISPOSITION_STACKED) {
      lives_proc_thread_t lpt = LPT_DATA(finst, runner);
      //g_print("async check %p\n", lpt);

      if (finst->disposition == DISPOSITION_WAITING
          || finst->disposition == DISPOSITION_ACTIVE) {
        if (cancel) lives_proc_thread_request_cancel(lpt, FALSE);
        else {
          pthread_rwlock_unlock(&finst->dispolock);
          PTMUH;
          return FALSE;
        }
      }

      pthread_rwlock_unlock(&finst->dispolock);

      lives_proc_thread_join_void(lpt);
      lives_proc_thread_unref(lpt);

      pthread_rwlock_rdlock(&finst->dispolock);
      if (finst->disposition == DISPOSITION_CANCELLED) {
        pthread_rwlock_unlock(&finst->dispolock);
        finst_disposition_pop(finst);
        lives_funcinst_send_replies(finst, LIVES_REPLY_CANCELLED);
        remove = TRUE;
      } else {
        if (finst->disposition == DISPOSITION_ERROR) {
          pthread_rwlock_unlock(&finst->dispolock);
          finst_disposition_pop(finst);
          lives_funcinst_send_replies(finst, LIVES_REPLY_ERROR);
          remove = TRUE;
        } else {
          // pop prior disposition - ie. STACKED
          pthread_rwlock_unlock(&finst->dispolock);
          finst_disposition_pop(finst);
        }
      }
    } else pthread_rwlock_unlock(&finst->dispolock);

    cbflags = CL_DATA(finst, cb_flags);

    cbflags &= ~HOOK_STATUS_RUNNING;
    CL_DATA(finst, cb_flags) = cbflags;

    if (cbflags & HOOK_STATUS_IGNORE) continue;

    cleanup_funcinst_receipts(finst, FALSE);

    if (remove || (cbflags & (HOOK_STATUS_REMOVE | HOOK_OPT_ONESHOT))) {
      remove_from_hstack(hstack, cblist);
      continue;
    }

    np = get_funcinst_nparams(finst);

    while (np--) {
      char *pkey = make_std_pname(np);
      if (weed_leaf_is_distinguished(finst->params, pkey)) {
        weed_leaf_delete(finst->params, pkey);
        weed_set_int_value(finst->params, LIVES_LEAF_MAXPARAM, np);
        lives_free(pkey);
      } else {
        lives_free(pkey);
        break;
      }
    }

  }
  hstack->flags &= ~HS_FLAG_TRIGGERING;
  PTMUH;
  return TRUE;
}


boolean lives_hook_async_join(int hstype) {
  lives_hook_stack_t *hstack = self_hook_stack(hstype);
  return hstack ?  _lives_hook_async_join(hstack, FALSE) : FALSE;
}


void lives_hook_async_cancel(int hstype) {
  lives_hook_stack_t *hstack = self_hook_stack(hstype);
  if (hstack) _lives_hook_async_join(hstack, TRUE);
}


boolean lives_obj_instance_async_join(lives_obj_instance_t *obj, int hstype) {
  lives_hook_stack_t *hstack = lives_obj_instance_find_hook_stack(obj, hstype);
  return hstack ?  _lives_hook_async_join(hstack, FALSE) : FALSE;
}


void lives_obj_instance_async_cancel(lives_obj_instance_t *obj, int hstype) {
  lives_hook_stack_t *hstack = lives_obj_instance_find_hook_stack(obj, hstype);
  if (hstack) _lives_hook_async_join(hstack, TRUE);
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


static boolean is_child_of(LiVESWidget * w, LiVESContainer * C) {
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


LIVES_GLOBAL_INLINE lives_funcdef_t *lives_funcdef_copy(lives_funcdef_t *src) {
  char *tmp;
  if (!src) return NULL;
  if (src->flags & FDEF_FLAG_STATIC) return src;
  lives_funcdef_t *dst = create_funcdef(src->funcname, src->function, src->return_type,
                                        (tmp = args_fmt_from_funcsig(src->funcsig)),
                                        NULL, 0, src->flags);
  lives_free(tmp);
  return dst;
}


lives_funcinst_t *lives_funcinst_copy(lives_funcinst_t *src) {
  LIVES_CALLOC_TYPE(lives_funcinst_t, dst, 1);
  if (dst) {
    dst->uid = gen_unique_id();
    if (src->funcdef) dst->funcdef = lives_funcdef_copy(src->funcdef);
    pthread_rwlock_init(&dst->dispolock, NULL);
    int nparams = get_funcinst_nparams(src);
    // copy params
    dst->params = weed_plant_copy(src->params);
    if (src->paramnames) {
      dst->paramnames = (const char **)lives_calloc(nparams, sizeof(char *));
      for (int i = 0; i < nparams; i++)
        dst->paramnames[i] = lives_strdup(src->paramnames[i]);
    }
  }
  return dst;
}


LIVES_GLOBAL_INLINE lives_funcinst_t *lives_funcinst_new(lives_funcdef_t *tmpl) {
  LIVES_CALLOC_TYPE(lives_funcinst_t, finst, 1);
  if (finst) {
    finst->uid = gen_unique_id();
    if (tmpl) finst->funcdef = tmpl;
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


LIVES_GLOBAL_INLINE int get_funcinst_nparams(lives_funcinst_t *finst) {
  return finst && finst->params ? weed_get_int_value(finst->params, LIVES_LEAF_MAXPARAM, NULL) : 0;
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

    //if (finst == mainw->debug_ptr) abort();

    pthread_rwlock_rdlock(&finst->dispolock);
    if (finst->disposition == DISPOSITION_STACKED) {
      // before we can free a funcinst with disposition stacked
      // we must ensure that all the receipts held by it are exired
      // - send an INVALID reply so any param data can be freed / blockers unblocked
      // if there is a sendt_reply callback, adder can
      pthread_rwlock_unlock(&finst->dispolock);
      if (CL_DATA(finst, hstack)) return;
      if (CL_DATA(finst, receipts)) {
        lives_funcinst_send_replies(finst, LIVES_REPLY_INVALID);
        if (CL_DATA(finst, receipts)) {
          return;
        }
      }
    } else pthread_rwlock_unlock(&finst->dispolock);

    pthread_rwlock_wrlock(&finst->dispolock);
    if (finst->retloc && !(finst->flags & FINST_FLAG_NOFREE_RETLOC)) lives_free(finst->retloc);

    if (finst->params) {
      free_finst_paramdata(finst, TRUE);
      weed_plant_free(finst->params);
    }

    if (finst->funcdef) lives_funcdef_free(finst->funcdef);
    if (finst->module) finst_module_free(finst->module, finst->mod_type);

    if (finst->paramnames) {
      int nparams = get_funcinst_nparams(finst);
      for (int i = 0; i < nparams; i++)
        if (finst->paramnames[i]) lives_free((void *)finst->paramnames[i]);
      lives_free(finst->paramnames);
    }

    pthread_rwlock_unlock(&finst->dispolock);
    pthread_rwlock_destroy(&finst->dispolock);

    lives_free(finst);
  }
}

