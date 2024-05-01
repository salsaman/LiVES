// lives_plants.c
// LiVES
// (c) G. Finch 2019 - 2024 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// specialised weed_plants for use in LiVES

#include "main.h"

LIVES_GLOBAL_INLINE int64_t lives_plant_get_subtype(weed_plant_t *plant) {
  if (!IS_LIVES_PLANT(plant)) return 0;
  return weed_get_int64_value(plant, LIVES_LEAF_SUBTYPE, NULL);
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new(int64_t subtype) {
  weed_plant_t *plant = weed_plant_new(WEED_PLANT_LIVES);
  weed_set_int64_value(plant, LIVES_LEAF_SUBTYPE, subtype);
  weed_set_int64_value(plant, WEED_LEAF_UNIQUE_ID, gen_unique_id());
  lives_leaf_set_rdonly(plant, WEED_LEAF_UNIQUE_ID, TRUE, TRUE);
  weed_leaf_set_undeletable(plant, WEED_LEAF_UNIQUE_ID, TRUE);
  return plant;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new_with_serialno(int64_t subtype, int64_t serialno) {
  weed_plant_t *plant = lives_plant_new(subtype);
  weed_set_int64_value(plant, LIVES_LEAF_SERIAL_NUMBER, serialno);
  return plant;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new_with_refcount(int64_t subtype) {
  weed_plant_t *plant = lives_plant_new(subtype);
  weed_add_refcounter(plant);
  return plant;
}

// blueprints
static weed_plant_t *plant_from_template(bootstrap_template *templ);
static weed_plant_t *plant_from_blueprint(int pltype, ...);

static weed_plant_t *allblu = NULL;
static boolean have_leaf_def_plant, have_blueprint_plant;

void register_blueprint(uint64_t pltype, ...) {
  // what we do here is read the va_list, it will be name, type, flags
  // we append this to an array.
  // now we can make an internal blueprint
  // first we make blueprint for index plant, and we can use the array to make one of these, so now we have a place to store blueprints
  // next we create a blueprint for leaf def, this an array version but we store it in index
  // then we make a blueprint for blueprint plant - now we can use blueprint plant and leaf def to make blueprint plants
  // with leaf def plants, but we are still making blueprint and leaf defs from arrays
  // now we have index blueprint though, and we can make an index to store blueprint plants in
  // then we remake leaf_def and blueprint blueprints now as blueprint plants and store in index
  // then index and the rest of the blueprints are registered

  weed_plant_t **ld_array = NULL, *blu = NULL;
  char *pltypestr;
  va_list va;
  int nleaves = 0;

  if (!have_leaf_def_plant || !have_blueprint_plant) {
    LIVES_CALLOC_TYPE(lives_blueprint_t, blu, 1);
    blu->pltype = pltype;
  }
 
  pltypestr = LSPF("%"PRIu64, pltype);
  va_start(va, pltype);

  while (1) {
    char *name = va_arg(va, char *);
    if (!name) break;
    weed_seed_t st = va_arg(va, weed_seed_t);
    uint64_t flags = va_arg(va, uint64_t);
    if (blu) {
      LIVES_CALLOC_TYPE(leaf_desc_t, ldesc, 1);
      ldesc->name = (const char *)lives_strdup(name);
      ldesc->type = st;
      ldesc->flags = flags;
      blu->leaves = lives_list_prepend(blu->leaves, ldesc);
    }
    else {
      weed_plant_t *ld_plant = plant_from_blueprint(LEAF_DEF, WEED_LEAF_NAME, name, WEED_LEAF_SEED_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      ld_array = (weed_plant_t **)lives_realloc(ld_array, nleaves + 1, sizeof(weed_plant_t *));
      ld_array[nleaves++] = ld_plant;
    }
  }
  va_end(va);

  if (have_blueprint_plant && have_value_plant) {
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX, pltype, LIVES_LEAF_LEAF_DEFS, ld_array, nleaves); 
    lives_index_set_value(allblu, pltypestr, blup);
    lives_index_set_autofree(allblu, pltypestr, TRUE);
    lives_free(pltypestr);
    return;
  }

  blu->leaves = lives_list_reverse(blu->leaves);

  if (!allblu) {
    // first template we make is index, so we make one now, using the template version
    allblu = plant_from_template(blu);
    return;
  }

  // this is leaf_def or blueprint from templates
  // we will store these temporarily in index
  // add to a plant, so we have the right type for index
  blup = lives_plant_new(LIVES_PLANT_TMP);
  weed_set_voidptr_value(blup, WEED_LEAF_VALUE, blu);
  weed_leaf_set_autofree(blup, WEED_LEAF_VALUE, TRUE);
  lives_index_set_value(allblu, pltypestr, blup);
  lives_index_set_autofree(allblu, pltypestr, TRUE);

  if (pltype == LIVES_PLANT_LEAF_DEF) have_leaf_def = TRUE;
  else have_blueprint = TRUE;
}
  

void register_blueprints(void) {REGISTER_ALL_BLUEPRINTS;}


static weed_plant_t *plant_from_template(bootstrap_template *templ, ...) {
  weed_plant_t *opl = lives_plant_new(pltype);
  va_start(va, pltype);
  while (1) {
    weed_seed_t st;
    weed_size_t ne = 1;
    uint64_t flags;  
    char *name = va_arg(va, char *);
    if (!name) break;
    // search for "name" in leaf_defs
    LiVESList *list;
    for (list = tmpl->leaves; list; list = list->next) {
      bootstrap_leafdesc *ldef = (bootstrap_leafdesc *)list->data;
      if (!lives_strcmp(name, ldef->name)) break;
    }
    if (!list) {
      // no match found, but we can add extra leaves if we follow name with seed_type, num_elems
      st = va_arg(va, int);
      ne = va_arg(va, weed_size_t);
    }
    else {
      // got a match, we use defined seed_type and flags
      // if def st == 0, then it follows name
      st = ldef->type;
      if (!st) st = va_arg(va, int);
      flags = ldef->flags;
      // if flags said array then we read num_elems
      if (flags & BLU_FLAG_ARRAY) ne = va_arg(va, weed_size_t);
    }
    weed_leaf_from_varg(opl, name, st, ne, va);
  }
  va_end(va);
  return opl;
}


// make a plant from its blueprint
static weed_plant_t *plant_from_blueprint(int pltype, ...) {
  va_list va;
  // find the blueprint in allblu  
  weed_plant_t *blu;
  if (lives_index_get(&blu, allblu, pltypestr) != WEED_SUCCESS) return NULL;
  if (!blu) return NULL;

  if (lives_plant_get_subtype(blu) == LIVES_PLANT_TMP)
    return plant_from_template((bootstrap_template *)weed_get_voidptr_value(blu, WEED_SEED_VALUE, NULL));
  
  // now for each value, we will make a leaf in pl_out, reading va_value
  // if st is 0, we read a seed_type, if flag ! scalar we read ne

  int ndefs;
  weed_plant_t **defs = weed_get_plantptr_array_counted(blu, LIVES_LEAF_LEAF_DEFS, &ndefs);
  weed_plant_t *opl = lives_plant_new(pltype);
  va_start(va, pltype);
  while (1) {
    weed_seed_t st;
    weed_size_t ne = 1;
    uint64_t flags;
    int i;
    char *name = va_arg(va, char *);
    if (!name) break;
    // search for "name" in leaf_defs
    for (i = 0; i < ndefs; i++) { 
      const char *defname = weed_get_const_string_value(defs[i], WEED_LEAF_NAME, NULL);
      if (!lives_strcmp(name, defname)) break;
    }
    if (i == ndefs) {
      // no match found, but we can add extra leaves if we follow name with seed_type, num_elems
      st = va_arg(va, int);
      ne = va_arg(va, weed_size_t);
    }
    else {
      // got a match, we use defined seed_type and flags
      // if def st == 0, then it follows name
      st = weed_get_int_value(defs[i], WEED_LEAF_SEED_TYPE, NULL);
      if (!st) st = va_arg(va, int);
      flags = weed_get_uint64_value(defs[i], WEED_LEAF_FLAGS, NULL);
      // if flags said array then we read num_elems
      if (flags & BLU_FLAG_ARRAY) ne = va_arg(va, weed_size_t);
    }
    weed_leaf_from_varg(opl, name, st, ne, va);
  }
  va_end(va);
  if (defs) lives_free(defs);
  return opl;
}

////////////////////////////

// specialised plants

// LIVES_PLANT_INDEX
static char *name_for_index(lives_index_t *idx, const char *key) {
  const char *pfx = lives_index_get_prefix(idx);
  return LSPF("%s%s", pfx, key);
}

index_type lives_index_get_idxtype(lives_index_t *idx) {return idx ? weed_get_int_value(idx, LIVES_LEAF_INDEX_TYPE, NULL) : idx_type_anon;}

const char *lives_index_get_prefix(lives_index_t *idx) {return idx ? weed_get_const_string_value(idx, LIVES_LEAF_PREFIX, NULL);}

weed_seed_t lives_index_get_itemtype(lives_index_t *idx) {return idx ? weed_get_int_value(idx, LIVES_LEAF_ITEM_TYPE, NULL);}

weed_error_t lives_index_set_value(lives_index_t *idx, const char *key, ...) {
  weed_seed_t st = lives_index_get_itemtype(idx);
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_from_varg(idx, name, st, 1, va);
  lives_free(name);
  return err;
}

weed_error_t lives_index_set_autofree(lives_index_t *idx, const char *key, boolean set) { 
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_set_autofree(idx, name, set);
  lives_free(name);
  return err;
}

weed_error_t lives_index_get_value(void *retloc, lives_index_t *idx, const char *key) {
  weed_seed_t st = lives_index_get_itemtype(idx);
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_get(idx, name, st, 0, retloc);
  lives_free(name);
  return err;
}


// LIVES_PLANT_DATA_BOOK
// this is a specialised type of INDEX, prefix is DATA_BOOK_PREFIX, item_type is LIVES_SEED_ALLVALUES


allvalues_t *get_book_item(lives_databook_t *book, const char *itemnm) {
  allvalues_t *allvp;
  lives_index_get_data(&allvp, book, itemnm);
  return allvp;
}


weed_seed_t get_book_datatype(lives_databook_t *book, const char *itemnm) {
  allvalues_t *allvp = get_book_item(book, itemnm);
  return allvp ? allvp->st : WEED_SEED_INVALID;
}


weed_error_t lives_data_book_set_datatype(lives_databook_t *book, const char *name, weed_seed_t itype) {
  allvalues_t *allvp = get_book_item(book, name);
  if (!allvp) {
    allvp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    allvp->stype = itype;
  }
  if (flags & PARAM_FLAG_BOUND) {
    if (ne > 1) xflags = ALLV_ERR_NVALS;
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
      avp->ne = 1;
      xflags |= ALLV_FLAG_POINTER;
    }
  } else {
    weed_plant_t *tmp = lives_plant_new(LIVES_PLANT_TMP);
    weed_leaf_from_varg(tmp, WEED_LEAF_VALUE, stype, ne, va);
    avp = allvalues_from_leaf(avp, tmp, WEED_LEAF_VALUE);
    weed_plant_free(tmp);
    xflags &= ~ALLV_FLAG_POINTER;
  }
  avp->flags = xflags;
  return avp;
}

weed_error_t lives_databook_set_value(lives_databook_t *book, const char *name, weed_seed_t itype, ...) {
  allvalues_t *allvp = get_book_item(book, name);
  va_list va;
  va_start(va, itype);
  SET_ALLVALUE_VA(allvp, itype, va);
  va_end(va);
}

weed_error_t lives_databook_set_array(lives_databook_t *book, const char *name, weed_seed_t itype, int *nvals, void *vals) {
  allvalues_t *allvp = get_book_item(book, name);
  va_list va;
  va_start(va, itype);
  SET_ALLVALUE_ARRAY_VA(allvp, itype, nvals, va);
  va_end(va);
}

weed_error_t lives_databook_get_value(void *retloc, lives_databook_t *book, const char *name, weed_seed_t itype) {
  weed_error_t err;
  allvalues_t *allvp = get_book_item(book, name);
  weed_plant_t *tmpplant = lives_plant_new(LIVES_PLANT_TMP);
  LEAF_FROM_ALLV(tmpplant. WEED_LEAF_VALUE, allvp);
  err = weed_leaf_get(tmppplant, WEED_LEAF_VALUE, 0, retloc);
  weed_plant_free(tmpplant);
}

weed_error_t lives_databook_get_array(void *retloc, lives_databook_t *book, const char *name, weed_seed_t itype, int *nvals) {
  // TODO - get size to allocate
  allvalues_t *allvp = get_book_item(book, name);
  return WEED_SUCCESS;
}


allvalues_t *get_local_book_item(const char *itemnm) {
  allvalues_t *allvp;
  GET_PROC_THREAD_SELF(self);
  lives_databok_t *book = lives_proc_thread_get_book(self);
  lives_index_get_data(&allvp, book, itemnm);
  return allvp;
}

allvalues_t *get_global_book_item(const char *itemnm) {
  allvalues_t *allvp;
  GET_PROC_THREAD_SELF(self);
  lives_databok_t *book = mainw->global_databook;
  lives_index_get_data(&allvp, book, itemnm);
  return allvp;
}


// LIVES_PLANT_STRUCT_ADAPTOR

weed_plant_t *valplant_for_struct(const char *stname, void *struc) {
  weed_plant_t *vpl = PLANT_FROM_BLU(VALUE, stname, WEED_SEED_VOIDPTR, 0, struc);
  weed_set_int_value(vpl, "val_dtl", STRUCT_ADAPTOR);
  return vpl;
}
