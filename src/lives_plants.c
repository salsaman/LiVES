// lives_plants.c
// LiVES
// (c) G. Finch 2019 - 2024 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// specialised weed_plants for use in LiVES

#include "main.h"
#include "diagnostics.h"

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

static char *valdtypestr, *bltypestr, *idxtypestr;

static weed_plant_t *plant_from_template(bootstrap_template *templ, ...);

static weed_plant_t *allblu = NULL;


void dump_tmpl(  bootstrap_template *tmpl) {
  g_print("\n\nshowing tmpl for type %lu\n", tmpl->pltype);
  for (LiVESList *v = tmpl->valdefs; v; v = v->next) {
    bootstrap_valdef *val = (bootstrap_valdef *)v->data;
    g_print("value: %s, type %u, flags %lu\n", val->name, val->type, val->flags);
  }
  g_print("\n\n");
}


void register_blueprint(uint64_t pltype, ...) {
  // what we do here is read the va_list, it will be name, type, flags
  // we append this to an array.
  // gen 1 - we create a boostrap template and this is filled with bootstrap leafdefs
  // we can now use the bootstrap template to build the plant type
  // 
  // first we make template for index plant, and use it to make one,
  // so now we have a place to store templates
  // next we create a template for leaf def, and store in index
  // then we make a template for blueprint plant and store in index
  // now we can make blueprint plant from template,
  // and fill it with leaf defs from template to make blueprint plants
  // then we remake value_def and blueprint templates, now as blueprint plants and rpelace in index
  // biw we can use the soft versions  to remake blueptints / leaf def for blueprint and leaf defs

  // so we have - make templates
  // make plants from tempolates
  // now we have the first chance to adjust them
  // make plants from plants  - with any adjutsments
  // make plants from ajusted plants

  bootstrap_template *tmpl = NULL, *valdef, *bltempl, *idxtmpl; 
  weed_plant_t *blup = NULL, *tmppl, *valdef_plant;
  lives_index_t *val_index;
  va_list va;
  char *pltypestr, *name;
  weed_seed_t st;
  uint64_t flags;
  static int generation = 0;

  // generation 1a - make template for index
  //  - use index template to make index plant
  
  // generation 2 make template for value_def
  // generation 3 make template for blueprint
  //
  // generation 4 make blueprint for blueprint, usign template for blueprint and template for valdef
  // generation 5 make blueprint for val def using blueprint for blueprint and template for valdef
  //
  // generation 6 make blueprint for valdef, using blueprint for blueprint and blueprint for valdef
  // generation 7 make blueprint for blueprint using blueprint for blueprint and blueprint for valdef

  // generation 8 make blueprint for index using blueprint for blueprint and blueprint for valdef
  // make new index from blueprint, items from old index are moved

  // AT THIS POINT WE CAN NOW ADJUST BP FOR VALDEFS, IDX AND BP
  //
  // RECTREATE THE BLUEPRINTS SELF REFERENTIALLY AS WE MAY HAVE ADJUSTED SOMETHING WHICH AFFECTS BLUEPRINT CONSTRUCTION

  // generation 9 make blueprint for index using blueprint for blueprint and blueprint for valdef

  // generation 10 make blueprint for valdef, using blueprint for blueprint and blueprint for valdef
  // generation 11 make blueprint for blueprint using blueprint for blueprint and blueprint for valdef

  // one more time to bake in any changes
  // generation 12 make blueprint for index using blueprint for blueprint and blueprint for valdef

  // generation 13 make blueprint for valdef, using blueprint for blueprint and blueprint for valdef
  // generation 14 make blueprint for blueprint using blueprint for blueprint and blueprint for valdef

  // one more time

  pltypestr = LSPF("%"PRIu64, pltype);
  va_start(va, pltype);

  if (generation > 14) {
    val_index = LIVES_MAKE_INDEX(idx_type_values, WEED_SEED_PLANTPTR);
    while (1) {
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      flags = va_arg(va, uint64_t);
      valdef_plant = plant_from_blueprint(LIVES_PLANT_VALUE_DEF, LIVES_LEAF_BLUEPRINT_PTR, NULL, WEED_LEAF_NAME, name, LIVES_LEAF_SEED_TYPE, st,
				      WEED_LEAF_FLAGS, flags, NULL);
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, ADD_STD_LEAVES(NULL), LIVES_LEAF_BLUEPRINT_IDX, pltype, LIVES_LEAF_VALUE_DEFS, val_index);

    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, blup);
    lives_index_set_autofree(allblu, pltypestr, TRUE);

    va_end(va);
    lives_free(pltypestr);
    return;
  }

  generation++;

  switch (generation) {
  case 1: case 2: case 3: {
    tmpl = LIVES_CALLOC_SIZEOF(bootstrap_template, 1);
    tmpl->pltype = pltype;
    while (1) {
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      flags = va_arg(va, uint64_t);
      LIVES_CALLOC_TYPE(bootstrap_valdef, valdef, 1);
      valdef->name = (const char *)lives_strdup(name);
      if (st == LIVES_SEED_LIVES_PLANT) {
	valdef->pl_subtype = va_arg(va, int64_t);
	st = WEED_SEED_PLANTPTR;
      }
      valdef->type = st;
      valdef->flags = flags;
      tmpl->valdefs = lives_list_prepend(tmpl->valdefs, valdef);
    }
    tmpl->valdefs = lives_list_reverse(tmpl->valdefs);
    
    tmppl = lives_plant_new(LIVES_PLANT_TMP);
    weed_set_voidptr_value(tmppl, WEED_LEAF_VALUE, tmpl);
    //lives_index_set_autofree(tmppl, WEED_LEAF_VALUE, TRUE);

    if (generation == 1) allblu = plant_from_template(tmpl, ADD_STD_LEAVES(tmpl), LIVES_LEAF_INDEX_TYPE,
						      idx_type_blueprints, LIVES_LEAF_PREFIX, "_data",
						      LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);

    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, tmppl);
    lives_index_set_autofree(allblu, pltypestr, TRUE);

    break;
  }

  case 4: {
    // generation 4 make blueprint for blueprint, using template for blueprint and template for ldef
    lives_index_get_value(&tmppl, allblu, idxtypestr);
    idxtmpl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    val_index = plant_from_template(idxtmpl, ADD_STD_LEAVES(idxtmpl), LIVES_LEAF_INDEX_TYPE, idx_type_values,
				    LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    lives_index_get_value(&tmppl, allblu, valdtypestr);
    valdef = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);

    while (1) {
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
	va_arg(va, int64_t);
	st = WEED_SEED_PLANTPTR;
      }
      flags = va_arg(va, uint64_t);
      valdef_plant = plant_from_template(valdef, LIVES_LEAF_BLUEPRINT_PTR, valdef, WEED_LEAF_NAME, name,
					 LIVES_LEAF_SEED_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    lives_index_get_value(&tmppl, allblu, bltypestr);
    bltempl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    
    blup = plant_from_template(bltempl, ADD_STD_LEAVES(bltempl), LIVES_LEAF_BLUEPRINT_IDX, pltype,
			       LIVES_LEAF_VALUE_DEFS, val_index, NULL);
    
    lives_index_set_value(allblu, bltypestr, WEED_SEED_PLANTPTR, blup);
    lives_index_set_autofree(allblu, bltypestr, TRUE);
    break;
  }
    
  case 5: {
    // generation 5 make blueprint for value def using blueprint for blueprint and template for ldef
    lives_index_get_value(&tmppl, allblu, idxtypestr);
    idxtmpl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    val_index = plant_from_template(idxtmpl, ADD_STD_LEAVES(idxtmpl), LIVES_LEAF_INDEX_TYPE, idx_type_values,
				    LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    lives_index_get_value(&tmppl, allblu, valdtypestr);
    valdef = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    while (1) {
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
	va_arg(va, int64_t);
	st = WEED_SEED_PLANTPTR;
      }
      flags = va_arg(va, uint64_t);
      valdef_plant = plant_from_template(valdef, LIVES_LEAF_BLUEPRINT_PTR, valdef, WEED_LEAF_NAME, name,
					 LIVES_LEAF_SEED_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    //
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX, pltype, LIVES_LEAF_VALUE_DEFS, val_index);
    //
    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, blup);
    lives_index_set_autofree(allblu, pltypestr, TRUE);
    break;
  }
    
  case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: {
    // generation 6,7,8 make blueprint for leaf def using blueprint for blueprint and BLUE for ldef

    lives_index_get_value(&blup, allblu, bltypestr);

    if (generation > 8) {
      list_leaves(allblu);
      val_index = PLANT_FROM_BLUEPRINT(INDEX, LIVES_LEAF_INDEX_TYPE,
					idx_type_values, LIVES_LEAF_PREFIX, IDX_PREFIX,
					LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    }
    else {
      lives_index_get_value(&tmppl, allblu, idxtypestr);
      idxtmpl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
      val_index = plant_from_template(idxtmpl, ADD_STD_LEAVES(idxtmpl), LIVES_LEAF_INDEX_TYPE, idx_type_values,
				      LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    }

    while (1) {
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
	va_arg(va, int64_t);
	st = WEED_SEED_PLANTPTR;
      }
      flags = va_arg(va, uint64_t);
      valdef_plant = plant_from_blueprint(LIVES_PLANT_VALUE_DEF, LIVES_LEAF_BLUEPRINT_PTR, NULL, WEED_LEAF_NAME, name,
				       LIVES_LEAF_SEED_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }

    lives_index_get_value(&blup, allblu, bltypestr);
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX, pltype, LIVES_LEAF_VALUE_DEFS, val_index);

    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, blup);

    if (generation == 8 || generation == 9 || generation == 12) {
      weed_plant_t *blup2, *allblu_nu;
      allblu_nu = PLANT_FROM_BLUEPRINT(INDEX, LIVES_LEAF_INDEX_TYPE, idx_type_blueprints, LIVES_LEAF_PREFIX, "_data",
				       LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
      list_leaves(allblu);

      lives_index_get_value(&blup2, allblu, valdtypestr);
      lives_index_set_value(allblu_nu, valdtypestr, WEED_SEED_PLANTPTR, blup2);

      lives_index_get_value(&blup2, allblu, bltypestr);

      lives_index_set_value(allblu_nu, bltypestr, WEED_SEED_PLANTPTR, blup2);

      lives_index_get_value(&blup2, allblu, idxtypestr);
      lives_index_set_value(allblu_nu, idxtypestr, WEED_SEED_PLANTPTR, blup2);

      if (generation == 12) {
	lives_index_set_autofree(allblu_nu, valdtypestr, TRUE);
	lives_index_set_autofree(allblu_nu, bltypestr, TRUE);
	lives_index_set_autofree(allblu_nu, idxtypestr, TRUE);
      }
      weed_plant_free(allblu);
      allblu = allblu_nu;
      list_leaves(allblu);
    }
    break;
  }
  default: break;
  }

  va_end(va);
  lives_free(pltypestr);
}
  

void register_blueprints(void) {
  valdtypestr = LSPF("%"PRIu64, LIVES_PLANT_VALUE_DEF);
  bltypestr = LSPF("%"PRIu64, LIVES_PLANT_BLUEPRINT);
  idxtypestr = LSPF("%"PRIu64, LIVES_PLANT_INDEX);
  REGISTER_ALL_BLUEPRINTS;
}


static weed_plant_t *plant_from_template_va(bootstrap_template *templ, va_list va) {
  weed_plant_t *opl = lives_plant_new(templ->pltype);
  while (1) {
    weed_seed_t st;
    weed_size_t ne = 1;
    uint64_t flags;
    char *name = va_arg(va, char *);
    if (!name) break;
    bootstrap_valdef *valdef;
    // search for "name" in value_defs
    LiVESList *list;
    for (list = templ->valdefs; list; list = list->next) {
      valdef = (bootstrap_valdef *)list->data;
      if (!lives_strcmp(name, valdef->name)) break;
    }
    if (!list) {
      // no match found, but we can add extra leaves if we follow name with seed_type, num_elems
      st = va_arg(va, int);
      ne = va_arg(va, weed_size_t);
    } else {
      // got a match, we use defined seed_type and flags
      // if def st == 0, then it follows name
      st = valdef->type;
      if (!st) {
	st = va_arg(va, int);
	if (st == LIVES_SEED_LIVES_PLANT) {
	  valdef->pl_subtype = va_arg(va, int64_t);
	  st = WEED_SEED_PLANTPTR;
	}
      }
      flags = valdef->flags;
      // if flags said array then we read num_elems
      if (flags & BLU_FLAG_ARRAY) {	
	ne = va_arg(va, weed_size_t);
      }
    }
    weed_leaf_from_varg(opl, name, st, ne, va);
  }
  return opl;
}


static weed_plant_t *plant_from_template(bootstrap_template *templ, ...) {
  va_list va;
  weed_plant_t *opl;
  va_start(va, templ);
  opl = plant_from_template_va(templ, va);
  va_end(va);
  return opl;
}


weed_plant_t *plant_from_blueprint(int pltype, ...) {
  va_list va;
  boolean foundbptr = FALSE;
  weed_plant_t *blu, *opl, *def;
  char *pltypestr = LSPF("%"PRIu64, pltype);
  // get blueprint for type
  weed_error_t err = lives_index_get_value(&blu, allblu, pltypestr);
  if (err != WEED_SUCCESS) {
    lives_free(pltypestr);
    return NULL;
  }
  lives_free(pltypestr);
  if (!blu) return NULL;

  va_start(va, pltype);

  // now for each value, we will make a leaf in pl_out, reading va_value
  // if st is 0, we read a seed_type, if flag ! scalar we read ne
  lives_index_t *val_index = weed_get_plantptr_value(blu, LIVES_LEAF_VALUE_DEFS, NULL);

  opl = lives_plant_new(pltype);

  while (1) {
    weed_seed_t st;
    weed_size_t ne = 1;
    uint64_t flags;
    char *name = va_arg(va, char *);
    if (!name) break;
    // search for "name" in value_defs
    if (lives_index_get_value(&def, val_index, name) != WEED_SUCCESS) {
      // no match found, but we can add extra leaves if we follow name with seed_type, flags, num_elems
      st = va_arg(va, int);
      flags = va_arg(va, uint64_t);
      ne = va_arg(va, weed_size_t);
    } else {
      // got a match, we use defined seed_type and flags
      // if def st == 0, then it follows name
      st = weed_get_int_value(def, LIVES_LEAF_SEED_TYPE, NULL);
      if (!st) st = va_arg(va, int);
      flags = weed_get_uint64_value(def, WEED_LEAF_FLAGS, NULL);
      // if flags said array then we read num_elems
      if (flags & BLU_FLAG_ARRAY) ne = va_arg(va, weed_size_t);
    }
    weed_leaf_from_varg(opl, name, st, ne, va);
    if (!foundbptr && !lives_strcmp(name, LIVES_LEAF_BLUEPRINT_PTR)) {
      foundbptr = TRUE;
      if (!weed_get_voidptr_value(opl, name, NULL))
	weed_set_voidptr_value(opl, name, blu);
    }
    if ((flags & BLU_FLAGS_REMOVABLE) != BLU_FLAGS_REMOVABLE)
    weed_leaf_set_undeletable(opl, name, TRUE);
  }
  va_end(va);
  return opl;
}

////////////////////////////

// specialised plants

// LIVES_PLANT_INDEX
static char *name_for_index(lives_index_t *idx, const char *key) {
  const char *pfx = lives_index_get_prefix(idx);
  return LSPF("%s%s", pfx, key);
}


LIVES_GLOBAL_INLINE index_type lives_index_get_idxtype(lives_index_t *idx) {return idx ? weed_get_int_value(idx, LIVES_LEAF_INDEX_TYPE, NULL) : idx_type_anon;}

LIVES_GLOBAL_INLINE const char *lives_index_get_prefix(lives_index_t *idx) {return idx ? weed_get_const_string_value(idx, LIVES_LEAF_PREFIX, NULL) : NULL;}

LIVES_GLOBAL_INLINE weed_seed_t lives_index_get_itemtype(lives_index_t *idx) {return idx ? weed_get_int_value(idx, LIVES_LEAF_ITEM_TYPE, NULL) : WEED_SEED_INVALID;}

LIVES_GLOBAL_INLINE char *lives_index_get_itemname(lives_index_t *idx, const char *key) {return idx ? name_for_index(idx, key) : NULL;}

lives_result_t lives_index_set_value(lives_index_t *idx, const char *key, weed_seed_t stype, ...) {
  va_list va;
  weed_seed_t st = lives_index_get_itemtype(idx);
  if (stype != st) return LIVES_RESULT_FAIL;
  char *name = name_for_index(idx, key);
  va_start(va, stype);
  weed_error_t err = weed_leaf_from_varg(idx, name, st, 1, va);
  va_end(va);
  lives_free(name);
  if (err != WEED_SUCCESS) return LIVES_RESULT_INVALID;
  return LIVES_RESULT_SUCCESS;
}

weed_error_t lives_index_set_autofree(lives_index_t *idx, const char *key, boolean set) {
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_set_autofree(idx, name, set);
  lives_free(name);
  return err;
}


weed_error_t lives_index_get_value(void *retloc, lives_index_t *idx, const char *key) {
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_get(idx, name, 0, retloc);
  if (err != WEED_SUCCESS) 
  lives_free(name);
  return err;
}


boolean lives_index_contains_item(lives_index_t *idx, const char *key) {
  char *name = name_for_index(idx, key);
  boolean ret = weed_plant_has_leaf(idx, name);
  lives_free(name);
  return ret;
}
 

boolean lives_index_has_value(lives_index_t *idx, const char *key) {
  char *name = name_for_index(idx, key);
  boolean ret = weed_leaf_num_elements(idx, key) > 0;
  lives_free(name);
  return ret;
}


boolean lives_index_erase_value(lives_index_t *idx, const char *key) {
  boolean ret = FALSE;
  if (idx && key) {
    char *name = name_for_index(idx, key);
    if (lives_index_contains_item(idx, name)) {
      weed_leaf_delete(idx, name);
      ret = TRUE;
    }
    lives_free(name);
  }
  return ret;
}


/* lives_index_t *lives_index_clone(lives_index_t *index) { */
/*   lives_index_t *clone = LIVES_MAKE_INDEX(lives_index_get_idxtype(index), lives_index_get_itemtype(lives_index_t *); */
  
/* } */



// LIVES_PLANT_DATA_BOOK
// this is a specialised type of INDEX, prefix is IDX_PREFIX, item_type is LIVES_SEED_ALLVALUES

allvalues_t *get_databook_item(lives_databook_t *book, const char *itemnm) {
  allvalues_t *allvp;
  if (lives_index_get_value(&allvp, book, itemnm) != WEED_SUCCESS)
    return NULL;
  return allvp;
}


weed_seed_t lives_databook_get_datatype(lives_databook_t *book, const char *itemnm) {
  allvalues_t *allvp = get_databook_item(book, itemnm);
  return allvp ? allvp->stype : WEED_SEED_INVALID;
}


lives_result_t lives_databook_set_datatype(lives_databook_t *book, const char *name, weed_seed_t itype) {
  allvalues_t *allvp = get_databook_item(book, name);
  if (!allvp) {
    allvp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    allvp->stype = itype;
    allvp->flags = ALLV_FLAG_TYPE_ONLY;
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_FAIL;
}


lives_result_t lives_databook_bind_value(lives_databook_t *book, const char *name, weed_seed_t itype, void *varptr) {
  lives_result_t res;
  allvalues_t *allvp = get_databook_item(book, name);
  allvp = SET_ALLVALUE_BOUND(allvp, itype, varptr);
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  return res;
}


lives_result_t lives_databook_set_value(lives_databook_t *book, const char *name, weed_seed_t itype, ...) {
  lives_result_t res;
  allvalues_t *allvp = get_databook_item(book, name);
  va_list va;
  if (allvp && allvp->stype != itype) return LIVES_RESULT_INVALID;
  va_start(va, itype);
  allvp = SET_ALLVALUE_VA(allvp, itype, va);
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  va_end(va);

  

  return res;
}


lives_result_t lives_databook_set_value_va(lives_databook_t *book, const char *name, weed_seed_t itype, va_list va) {
  lives_result_t res;
  allvalues_t *allvp = get_databook_item(book, name);
  if (allvp && allvp->stype != itype) return LIVES_RESULT_INVALID;
  allvp = SET_ALLVALUE_VA(allvp, itype, va);
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  char *name2 = name_for_index(book, name);
  g_print("XXX %p\n", weed_get_custom_value(book, name2, LIVES_SEED_ALLVALUES, NULL)); 
  return res;
}


lives_result_t lives_databook_set_array(lives_databook_t *book, const char *name, weed_seed_t itype, int nvals, void *vals) {
  lives_result_t res;
  allvalues_t *allvp = get_databook_item(book, name);
  if (allvp && allvp->stype != itype) return LIVES_RESULT_INVALID;
  allvp = SET_ALLVALUE_ARRAY(allvp, itype, nvals, vals);
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  return res;
}


lives_result_t lives_databook_get_value(void *retloc, lives_databook_t *book, const char *name) {
  allvalues_t *allvp = get_databook_item(book, name);
  return value_from_allvalues(retloc, allvp);
}


LIVES_GLOBAL_INLINE lives_databook_t *lives_local_databook(void) {
  GET_PROC_THREAD_SELF(self);
  return lives_proc_thread_get_book(self);
}


allvalues_t *get_local_book_item(const char *itemnm) {
  allvalues_t *allvp;
  lives_index_get_value(&allvp, lives_local_databook(), itemnm);
  return allvp;
}


allvalues_t *get_global_book_item(const char *itemnm) {
  allvalues_t *allvp;
  lives_databook_t *book = mainw->global_databook;
  lives_index_get_value(&allvp, book, itemnm);
  return allvp;
}


LIVES_GLOBAL_INLINE void lives_localbook_make_indellible(const char *item) {
  lives_databook_t *book = lives_local_databook();
  char *name = name_for_index(book, item);
  weed_leaf_set_undeletable(book, name, TRUE);
  lives_free(name);
}


void lives_book_item_make_indellible(lives_databook_t *book, const char *item) {
  char *name = name_for_index(book, item);
  weed_leaf_set_undeletable(book, item, TRUE);
  lives_free(name);
}


static void databook_clean(lives_databook_t *book) {
  weed_refcount_inc(book);
  // this works because: with default _weed_plant_free(), any undeletable leaves are retained
  // and plant is not freed. incrementing refcount adds an undeleteable leaf
  // (refcount) if that does not already exist.
  // thus, the plant cannot be freed, but any leaves NOT flagged as undeletable WILL be deleted.
  // - undeleteable leaves include "type", "uid", the refcounter itself
  // and ANY data added as "static", thus the effect will be to delete any data not flagged as "static"
  _weed_plant_free(book);
  weed_refcount_dec(book);
}


LIVES_GLOBAL_INLINE void lives_localbook_clean(void) {
  // cllaing oringal func - will delete all deletable leaves then return error
  // undeltable, since src_object is undeletable
  databook_clean(lives_local_databook());
}


boolean lives_databook_erase_value(lives_databook_t *book, const char *name) {
  // erases even indellib
  allvalues_t *allvp = get_databook_item(book, name);
  if (allvp) {
    lives_index_set_autofree(book, name, FALSE);
    allvalues_free(allvp);
    lives_index_erase_value(book, name);
    return TRUE;
  }
  return FALSE;
}

/* void lives_local_databook_push(lives_data_book_t *clone) { */
/*   lives_synclist_push(ldb_sybclist(clone)); */
/* } */


/* void lives_localbook_clone(void) { */
/*   lives_data_book_t *clone = lives_index_clone(lives_local_databook()); */
/*   lives_local_databook_push(clone); */
/*   lives_localbook_clean(); */
/* } */


/* void lives_localbook_pop(void) { */
/*   lives_synclist_pop(ldb_sybclist); */
/* } */


// LIVES_PLANT_STRUCT_ADAPTOR

weed_plant_t *valplant_for_struct(const char *stname, void *struc) {
  /* weed_plant_t *vpl = blueprint(VALUE, stname, WEED_SEED_VOIDPTR, 0, struc); */
  /* weed_set_int_value(vpl, "val_dtl", STRUCT_ADAPTOR); */
  /* return vpl; */
  return NULL;
}
