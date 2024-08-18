// lives_plants.c
// LiVES
// (c) G. Finch 2019 - 2024 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// specialised weed_plants for use in LiVES

#include "main.h"
#include "diagnostics.h"

lives_index_t *indices[idx_type_max];

lives_databook_t *global_databook;

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

LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new_empty(void) {
  return weed_plant_new(WEED_PLANT_LIVES);
}

// blueprints
//
static char *valdtypestr, *bltypestr, *idxtypestr;
//
static weed_plant_t *plant_from_template(bootstrap_template *templ, ...);
//
static weed_plant_t *allblu = NULL;

void dump_tmpl(bootstrap_template *tmpl) {
  g_print("\n\nShowing tmpl for type %lu\n", tmpl->pltype);
  for (LiVESList *v = tmpl->valdefs; v; v = v->next) {
    bootstrap_valdef *val = (bootstrap_valdef *)v->data;
    g_print("value: %s, type %u, flags %lu\n", val->name, val->type, val->flags);
  }
  g_print("\n\n");
}
//
void dump_template(uint64_t pltype) {
  char *pltypestr = LSPF("%"PRIu64, pltype);
  weed_plant_t *tmppl;
  lives_index_get_value(&tmppl, allblu, pltypestr);
  bootstrap_template *tmpl = (bootstrap_template *)
                             weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
  dump_tmpl(tmpl);
}
//
void dump_blueprint(uint64_t pltype) {
  char *pltypestr = LSPF("%"PRIu64, pltype);
  // get blueprint for type
  weed_plant_t *blup, *def = NULL;
  const char *key;
  lives_index_get_value(&blup, allblu, pltypestr);
  g_print("Showing details for BLUEPRINT type %lu (%p)\n\n"
          "\tName --- (type) --- flags\n", pltype, blup);
  //
  lives_index_t *validx = weed_get_plantptr_value(blup, LIVES_LEAF_VALUE_DEFS, NULL);
  //
  LIVES_INDEX_FOREACH(validx, key, def, g_print("\t%s (%d) %lu\n", *key == '_' ? key + 1 : key,
                      weed_get_int_value(def, LIVES_LEAF_VALUE_TYPE, NULL),
                      weed_get_uint64_value(def, WEED_LEAF_FLAGS, NULL)););
  g_print("\n");
}

void _register_blueprint(uint64_t pltype, const char *regstr, ...) {
  // what we do here is read the va_list, it will be name, type, flags
  //
  // OR a direcitve like "@EXTENDS", "@INCLUDES", "@OPT"
  //
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
  //
  // so we have - make templates
  // make plants from tempolates
  // now we have the first chance to adjust them
  // make plants from plants  - with any adjutsments
  // make plants from ajusted plants
  //
  bootstrap_template *tmpl = NULL, *valdef, *bltempl, *idxtmpl;
  weed_plant_t *blup = NULL, *tmppl, *valdef_plant;
  lives_index_t *val_index;
  va_list va;
  char *pltypestr, *name;
  weed_seed_t st;
  int64_t subtype = 0;
  uint64_t flags;
  static int generation = 0;
  //
  // generation 1a - make template for index
  //  - use index template to make index plant
  //
  // generation 2 make template for value_def
  // generation 3 make template for blueprint
  //
  // generation 4 make blueprint for blueprint, usign template for blueprint and template for valdef
  // generation 5 make blueprint for val def using blueprint for blueprint and template for valdef
  //
  // generation 6 make blueprint for valdef, using blueprint for blueprint and blueprint for valdef
  // generation 7 make blueprint for blueprint using blueprint for blueprint and blueprint for valdef
  //
  // generation 8 make blueprint for index using blueprint for blueprint and blueprint for valdef
  // make new index from blueprint, items from old index are moved
  //
  // AT THIS POINT WE CAN NOW ADJUST BP FOR VALDEFS, IDX AND BP
  //
  // RECTREATE THE BLUEPRINTS SELF REFERENTIALLY AS WE MAY HAVE ADJUSTED SOMETHING WHICH AFFECTS BLUEPRINT CONSTRUCTION
  //
  // generation 9 make blueprint for index using blueprint for blueprint and blueprint for valdef
  //
  // generation 10 make blueprint for valdef, using blueprint for blueprint and blueprint for valdef
  // generation 11 make blueprint for blueprint using blueprint for blueprint and blueprint for valdef
  //
  // one more time to bake in any changes
  // generation 12 make blueprint for index using blueprint for blueprint and blueprint for valdef
  //
  // generation 13 make blueprint for valdef, using blueprint for blueprint and blueprint for valdef
  // generation 14 make blueprint for blueprint using blueprint for blueprint and blueprint for valdef
  //
  pltypestr = LSPF("%"PRIu64, pltype);
  va_start(va, regstr);
  //
  weed_error_t err;
  if (generation > 13) {
    val_index = LIVES_MAKE_INDEX(idx_type_values, WEED_SEED_PLANTPTR);
    while (1) {
      uint64_t xflags = 0;
      name = va_arg(va, char *);
      if (!name) break;
      //
      // we can save some space now, we don't acually need to store the name,
      // because it is just the index name + pfxlen
      //
      if (*name == '@') {
        if (!lives_strcmp(name, "@EXTENDS")) {
          uint64_t pltype2 = va_arg(va, uint64_t);
          char *pltypestr2 = LSPF("%"PRIu64, pltype2);
          // get blueprint for type
          err = lives_index_get_value(&blup, allblu, pltypestr2);
          if (err != WEED_SUCCESS) {
            lives_free(pltypestr2);
            goto baderr;
          }
          lives_free(pltypestr2);
          if (blup) {
            weed_plant_t *def;
            char *key = NULL;
            lives_index_t *validx = weed_get_plantptr_value(blup, LIVES_LEAF_VALUE_DEFS, NULL);
            LIVES_INDEX_FOREACH(validx, key, def,
                                st = weed_get_int_value(def, LIVES_LEAF_VALUE_TYPE, NULL);
                                flags = weed_get_uint64_value(def, WEED_LEAF_FLAGS, NULL);
                                valdef_plant = plant_from_blueprint(LIVES_PLANT_VALUE_DEF,
                                               LIVES_LEAF_BLUEPRINT_PTR, NULL,
                                               WEED_LEAF_NAME, NULL,
                                               LIVES_LEAF_VALUE_TYPE, st,
                                               WEED_LEAF_FLAGS, flags, NULL);
                                if (key) name = lives_strdup(key);
                                lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
                                lives_index_set_autofree(val_index, name, TRUE); lives_free(name););

            continue;
          }
          goto baderr;
        }
        if (!lives_strcmp(name, "@USES")) {
          // just add a placeholder, when we create the blueprint, we call the setter func(blueprint)
          // an interface is like a bluprint, but it has const char *ifname, and a funcdef completion fun
          uint64_t pltype2 = va_arg(va, uint64_t);
          char *pltypestr2 = LSPF("%"PRIu64, pltype2);

          // get blueprint for type
          err = lives_index_get_value(&blup, allblu, pltypestr2);
          if (err != WEED_SUCCESS) {
            lives_free(pltypestr2);
            goto baderr;
          }
          char *iftypename = LSPF("%lu", pltypestr2);
          lives_free(pltypestr2);

          // checked now just add placeholder

          st = weed_get_int_value(blup, LIVES_LEAF_VALUE_TYPE, NULL);
          flags = weed_get_uint64_value(blup, WEED_LEAF_FLAGS, NULL);

          valdef_plant = plant_from_blueprint(LIVES_PLANT_VALUE_DEF,
                                              LIVES_LEAF_BLUEPRINT_PTR, NULL,
                                              WEED_LEAF_NAME, iftypename,
                                              LIVES_LEAF_VALUE_TYPE, st,
                                              WEED_LEAF_FLAGS, flags, NULL);
          lives_free(iftypename);
          name = lives_strdup(iftypename);
          lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
          lives_index_set_autofree(val_index, name, TRUE);
          lives_free(name);
          continue;
        }
      }
      //
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
        //valdef->pl_subtype =
        va_arg(va, int64_t);
        //subtype = va_arg(va, int64_t);
        st = WEED_SEED_PLANTPTR;
        xflags |= BLU_FLAG_AUTOUNREF;
      }
      flags = va_arg(va, uint64_t) | xflags;
      valdef_plant = plant_from_blueprint(LIVES_PLANT_VALUE_DEF, LIVES_LEAF_BLUEPRINT_PTR, NULL,
                                          WEED_LEAF_NAME, NULL, LIVES_LEAF_VALUE_TYPE, st,
                                          WEED_LEAF_FLAGS, flags, NULL);
      if (subtype) weed_set_int64_value(valdef_plant, LIVES_LEAF_SUBTYPE, subtype);
      if (flags & BLU_FLAG_HAS_DEFAULT)
        weed_leaf_from_varg(valdef_plant, WEED_LEAF_DEFAULT, st, -1, va);
      subtype = 0;
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX, pltype,
                                LIVES_LEAF_VALUE_DEFS, val_index);
    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, blup);
    lives_index_set_autofree(allblu, pltypestr, TRUE);
    //
baderr:
    va_end(va);
    lives_free(pltypestr);
    //
    g_print("\nRegisterd new blueprint for %lu\nString was %s\n", pltype, regstr);
    dump_blueprint(pltype);
    return;
  }
  //
  generation++;
  //
  switch (generation) {
  case 1: case 2: case 3: {
    tmpl = LIVES_CALLOC_SIZEOF(bootstrap_template, 1);
    tmpl->pltype = pltype;
    while (1) {
      uint64_t xflags = 0;
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      LIVES_CALLOC_TYPE(bootstrap_valdef, valdef, 1);
      valdef->name = (const char *)lives_strdup(name);
      if (st == LIVES_SEED_LIVES_PLANT) {
        valdef->pl_subtype = va_arg(va, int64_t);
        st = WEED_SEED_PLANTPTR;
        xflags |= BLU_FLAG_AUTOUNREF;
      }
      valdef->type = st;
      valdef->flags = va_arg(va, uint64_t) | xflags;
      tmpl->valdefs = lives_list_prepend(tmpl->valdefs, valdef);
    }
    tmpl->valdefs = lives_list_reverse(tmpl->valdefs);
    //
    tmppl = lives_plant_new_empty();
    weed_set_voidptr_value(tmppl, WEED_LEAF_VALUE, tmpl);
    //
    if (generation == 1) allblu = plant_from_template(tmpl, ADD_DEF_LEAVES(tmpl), LIVES_LEAF_INDEX_TYPE,
                                    lookup_type_blueprints, LIVES_LEAF_PREFIX, IDX_PREFIX,
                                    LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    //
    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, tmppl);
    lives_index_set_autofree(allblu, pltypestr, TRUE);
    //
    break;
  }
  //
  case 4: {
    // generation 4 make blueprint for blueprint, using template for blueprint and template for ldef
    lives_index_get_value(&tmppl, allblu, idxtypestr);
    idxtmpl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    val_index = plant_from_template(idxtmpl, ADD_DEF_LEAVES(idxtmpl), LIVES_LEAF_INDEX_TYPE, idx_type_values,
                                    LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    lives_index_get_value(&tmppl, allblu, valdtypestr);
    valdef = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    //
    while (1) {
      uint64_t xflags = 0;
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
        //valdef->pl_subtype =
        va_arg(va, int64_t);
        st = WEED_SEED_PLANTPTR;
        xflags |= BLU_FLAG_AUTOUNREF;
      }
      flags = va_arg(va, uint64_t) | xflags;
      valdef_plant = plant_from_template(valdef, LIVES_LEAF_BLUEPRINT_PTR, valdef, WEED_LEAF_NAME, name,
                                         LIVES_LEAF_VALUE_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    lives_index_get_value(&tmppl, allblu, bltypestr);
    bltempl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    //
    blup = plant_from_template(bltempl, ADD_DEF_LEAVES(bltempl), LIVES_LEAF_BLUEPRINT_IDX, pltype,
                               LIVES_LEAF_VALUE_DEFS, val_index, NULL);
    //
    lives_index_set_value(allblu, bltypestr, WEED_SEED_PLANTPTR, blup);
    lives_index_set_autofree(allblu, bltypestr, TRUE);
    //
    break;
  }
  //
  case 5: {
    // generation 5 make blueprint for value def using blueprint for blueprint and template for ldef
    lives_index_get_value(&tmppl, allblu, idxtypestr);
    idxtmpl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    val_index = plant_from_template(idxtmpl, ADD_DEF_LEAVES(idxtmpl), LIVES_LEAF_INDEX_TYPE, idx_type_values,
                                    LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    lives_index_get_value(&tmppl, allblu, valdtypestr);
    valdef = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
    while (1) {
      uint64_t xflags = 0;
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
        // valdef->pl_subtype =
        va_arg(va, int64_t);
        st = WEED_SEED_PLANTPTR;
        xflags |= BLU_FLAG_AUTOUNREF;
      }
      flags = va_arg(va, uint64_t) | xflags;
      valdef_plant = plant_from_template(valdef, LIVES_LEAF_BLUEPRINT_PTR, valdef, WEED_LEAF_NAME, name,
                                         LIVES_LEAF_VALUE_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    //
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX,
                                pltype, LIVES_LEAF_VALUE_DEFS, val_index);
    //
    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, blup);
    lives_index_set_autofree(allblu, pltypestr, TRUE);
    break;
  }
  //
  case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: {
    // generation 6,7,8 make blueprint for leaf def using blueprint for blueprint and BLUE for ldef
    //
    lives_index_get_value(&blup, allblu, bltypestr);
    //
    if (generation > 8) {
      val_index = PLANT_FROM_BLUEPRINT(INDEX, LIVES_LEAF_INDEX_TYPE,
                                       idx_type_values, LIVES_LEAF_PREFIX, IDX_PREFIX,
                                       LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR);
    } else {
      lives_index_get_value(&tmppl, allblu, idxtypestr);
      idxtmpl = (bootstrap_template *)weed_get_voidptr_value(tmppl, WEED_LEAF_VALUE, NULL);
      val_index = plant_from_template(idxtmpl, ADD_DEF_LEAVES(idxtmpl), LIVES_LEAF_INDEX_TYPE, idx_type_values,
                                      LIVES_LEAF_PREFIX, IDX_PREFIX, LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR, NULL);
    }
    //
    while (1) {
      uint64_t xflags = 0;
      name = va_arg(va, char *);
      if (!name) break;
      st = va_arg(va, weed_seed_t);
      if (st == LIVES_SEED_LIVES_PLANT) {
        //valdef->pl_subtype =
        va_arg(va, int64_t);
        st = WEED_SEED_PLANTPTR;
        xflags |= BLU_FLAG_AUTOUNREF;
      }

      flags = va_arg(va, uint64_t) | xflags;
      valdef_plant = plant_from_blueprint(LIVES_PLANT_VALUE_DEF, LIVES_LEAF_BLUEPRINT_PTR, NULL, WEED_LEAF_NAME, name,
                                          LIVES_LEAF_VALUE_TYPE, st, WEED_LEAF_FLAGS, flags, NULL);
      //
      lives_index_set_value(val_index, name, WEED_SEED_PLANTPTR, valdef_plant);
      lives_index_set_autofree(val_index, name, TRUE);
    }
    //
    lives_index_get_value(&blup, allblu, bltypestr);
    blup = PLANT_FROM_BLUEPRINT(BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX,
                                pltype, LIVES_LEAF_VALUE_DEFS, val_index);
    //
    lives_index_set_value(allblu, pltypestr, WEED_SEED_PLANTPTR, blup);
    //
    if (generation == 8 || generation == 9 || generation == 12) {
      weed_plant_t *blup2, *allblu_nu;
      allblu_nu = PLANT_FROM_BLUEPRINT(INDEX, LIVES_LEAF_INDEX_TYPE,
                                       lookup_type_blueprints, LIVES_LEAF_PREFIX, IDX_PREFIX,
                                       LIVES_LEAF_ITEM_TYPE, WEED_SEED_PLANTPTR);
      lives_index_get_value(&blup2, allblu, valdtypestr);
      lives_index_set_value(allblu_nu, valdtypestr, WEED_SEED_PLANTPTR, blup2);
      //
      lives_index_get_value(&blup2, allblu, bltypestr);
      //
      lives_index_set_value(allblu_nu, bltypestr, WEED_SEED_PLANTPTR, blup2);
      //
      lives_index_get_value(&blup2, allblu, idxtypestr);
      lives_index_set_value(allblu_nu, idxtypestr, WEED_SEED_PLANTPTR, blup2);
      //
      if (generation == 12) {
        lives_index_set_autofree(allblu_nu, valdtypestr, TRUE);
        lives_index_set_autofree(allblu_nu, bltypestr, TRUE);
        lives_index_set_autofree(allblu_nu, idxtypestr, TRUE);
      }
      weed_plant_free(allblu);
      allblu = allblu_nu;
    }
    break;
  }
  default: break;
  }
  //
  va_end(va);
  lives_free(pltypestr);
}
//
void register_blueprints(void) {
  for (int i = 0; i < idx_type_max; indices[i++] = NULL);
  //
  valdtypestr = LSPF("%"PRIu64, LIVES_PLANT_VALUE_DEF);
  bltypestr = LSPF("%"PRIu64, LIVES_PLANT_BLUEPRINT);
  idxtypestr = LSPF("%"PRIu64, LIVES_PLANT_INDEX);
  //
  BOOTSTRAP_BLUEPRINTS;
  //
  register_blueprint(INDEX, LIVES_DEF_BLUEPRINT, LIVES_LEAF_PREFIX, LIVES_SEED_CONST_CHARPTR,
                     BLU_FLAGS_NONE, LIVES_LEAF_INDEX_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE,
                     LIVES_LEAF_ITEM_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE, NULL);
  //
  register_blueprint(VALUE_DEF, LIVES_LEAF_BLUEPRINT_PTR, WEED_SEED_VOIDPTR,
                     BLU_FLAGS_NONE, WEED_LEAF_NAME, LIVES_SEED_CONST_CHARPTR,
                     BLU_FLAGS_NONE, LIVES_LEAF_VALUE_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE,
                     WEED_LEAF_FLAGS, WEED_SEED_UINT64, BLU_FLAGS_NONE,
                     WEED_LEAF_DEFAULT, LIVES_SEED_ALLTYPES, BLU_FLAG_OPTIONAL, NULL);

  //
  register_blueprint(BLUEPRINT, LIVES_DEF_BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX,
                     WEED_SEED_UINT64, BLU_FLAGS_NONE, LIVES_LEAF_VALUE_DEFS, LIVES_SEED_LIVES_PLANT,
                     LIVES_PLANT_INDEX, BLU_FLAG_AUTOUNREF, NULL);
  //
  register_blueprint(INDEX, LIVES_DEF_BLUEPRINT, LIVES_LEAF_INDEX_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE,
                     LIVES_LEAF_PREFIX, LIVES_SEED_CONST_CHARPTR,
                     BLU_FLAGS_NONE, LIVES_LEAF_ITEM_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE, NULL);
  //
  register_blueprint(VALUE_DEF, LIVES_LEAF_BLUEPRINT_PTR, WEED_SEED_VOIDPTR,
                     BLU_FLAGS_NONE, WEED_LEAF_NAME, LIVES_SEED_CONST_CHARPTR,
                     BLU_FLAGS_NONE, LIVES_LEAF_VALUE_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE,
                     WEED_LEAF_FLAGS, WEED_SEED_UINT64, BLU_FLAGS_NONE,
                     WEED_LEAF_DEFAULT, LIVES_SEED_ALLTYPES, BLU_FLAG_OPTIONAL, NULL);
  //
  register_blueprint(BLUEPRINT, LIVES_DEF_BLUEPRINT, LIVES_LEAF_BLUEPRINT_IDX,
                     WEED_SEED_UINT64, BLU_FLAGS_NONE, LIVES_LEAF_VALUE_DEFS, LIVES_SEED_LIVES_PLANT,
                     LIVES_PLANT_INDEX, BLU_FLAG_AUTOUNREF, NULL);

  /// boostrap types now defined

  //
  register_blueprint(DATA_BOOK, "@EXTENDS", LIVES_PLANT_INDEX, LIVES_LEAF_SCOPE, WEED_SEED_INT, BLU_FLAG_READWRITE, NULL);
  register_blueprint(LOOKUP, "@EXTENDS", LIVES_PLANT_INDEX, NULL);
  //
  // al bootstrap done

  register_blueprint(VALUE, LIVES_DEF_BLUEPRINT, WEED_LEAF_NAME, LIVES_SEED_CONST_CHARPTR,
                     BLU_FLAGS_NONE, LIVES_LEAF_VALUE_TYPE, WEED_SEED_INT, BLU_FLAGS_NONE,
                     LIVES_LEAF_NUM_ELEMS, WEED_SEED_INT, BLU_FLAGS_NONE,
                     //                     LIVES_LEAF_SIZE, WEED_SEED_UINT64, BLU_FLAGS_NONE,
                     WEED_LEAF_FLAGS, WEED_SEED_UINT64, BLU_FLAG_READWRITE,
#ifdef NATIVE_RWLOCK_TYPE
                     LIVES_LEAF_RWLOCK, WEED_SEED_VOIDPTR, BLU_FLAG_OPTIONAL,
#endif
                     WEED_LEAF_VALUE, LIVES_SEED_ALLTYPES, BLU_FLAGS_NONE,
                     LIVES_LEAF_PLANT_TYPE, WEED_SEED_INT, BLU_FLAG_OPTIONAL,
                     LIVES_LEAF_EXT_TYPE, LIVES_SEED_CONST_CHARPTR, BLU_FLAG_OPTIONAL, NULL);

  /*   /\* LIVES_LEAF_FUNCINST, LIVES_SEED_FUNCINST, BLU_FLAG_OPTIONAL, *\/ */
  /*   /\* LIVES_LEAF_CONTINGENCIES, LIVES_SEED_FUNCINST, *\/ */
  /*   /\* BLU_FLAG_OPTIONAL | BLU_FLAG_ARRAY, LIVES_LEAF_PRIV_DATA, *\/ */
  /*   /\* WEED_SEED_VOIDPTR, BLU_FLAG_OPTIONAL); *\/ */

  /*   // update script will ensure default and value types align with value type */
  register_blueprint(ATTRIBUTE, "@EXTENDS", LIVES_PLANT_VALUE,
                     WEED_LEAF_DEFAULT, LIVES_SEED_ALLTYPES, BLU_FLAG_OPTIONAL, NULL);

  /*   register_blueprint(VARIABLE, "EXTENDS", VALUE, */
  /*                      LIVES_LEAF_OLDVAL, WEED_SEED_PLANTPTR, BLU_FLAG_OPTIONAL, */
  /* 		     WEED_LEAF_SCOPE, WEED_SEED_INT, BLU_FLAGS_READWRITE); */

  /*   register_blueprint(PREFERENCE, "EXTENDS", ATTRIBUTE, LIVES_LEAF_STATUS, WEED_SEED_INT, BLU_FLAGS_NONE, */
  /* 		     WEED_LEAF_DESCRIPTION, LIVES_SEED_CONST_CHARPTR, BLU_FLAG_OPTIONAL); */

  register_blueprint(OBJ_INSTANCE, LIVES_DEF_BLUEPRINT, LIVES_LEAF_OBJ_TYPE, WEED_SEED_UINT64, BLU_FLAGS_NONE,
                     LIVES_LEAF_OBJ_SUBTYPE, WEED_SEED_UINT64, BLU_FLAGS_NONE,
                     LIVES_LEAF_STATUS, WEED_SEED_UINT64, BLU_FLAG_READWRITE | BLU_FLAG_HAS_DEFAULT, 0,
                     LIVES_LEAF_HOOK_STACKS, LIVES_SEED_LIVES_PLANT, LIVES_PLANT_INDEX, BLU_FLAG_AUTOUNREF | BLU_FLAG_OPTIONAL,
                     LIVES_LEAF_ATTR_GRP, LIVES_SEED_LIVES_PLANT, LIVES_PLANT_INDEX, BLU_FLAG_AUTOUNREF | BLU_FLAG_OPTIONAL, NULL);
}


static weed_plant_t *plant_from_template_va(bootstrap_template *templ, va_list va) {
  weed_plant_t *opl = lives_plant_new_empty();
  while (1) {
    weed_seed_t st;
    weed_size_t ne = -1;
    uint64_t flags = 0;
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
          valdef->flags |= BLU_FLAG_AUTOUNREF;
        }
      }
      flags = valdef->flags;
      // if flags said array then we read num_elems
      if (flags & BLU_FLAG_ARRAY) {
        ne = va_arg(va, weed_size_t);
      }
    }
    weed_leaf_from_varg(opl, name, st, ne, va);
    if ((flags & BLU_FLAG_AUTOUNREF))
      weed_leaf_set_autounref(opl, name, TRUE);
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


void add_blueprint_interface(weed_plant_t *pl, lives_blueprint_t *blup, uint64_t iftype) {
  switch (iftype) {
  case LIVES_DEF_INTERFACE:
    // add bluprint_ptr
    // add uid
    break;
  case LIVES_REFCOUNTER_INTERFACE:
    // add weed_refcouter
    weed_refcount_inc(pl);
    break;
  default: break;
  }
}


boolean lives_leaf_mandatory(lives_blueprint_t *blu, const char *name) {
  lives_valdef_t *def;
  lives_index_t *val_index = weed_get_plantptr_value(blu, LIVES_LEAF_VALUE_DEFS, NULL);
  if (lives_index_get_value(&def, val_index, name) == WEED_SUCCESS) {
    uint64_t flags = weed_get_uint64_value(def, WEED_LEAF_FLAGS, NULL);
    if (!(flags & BLU_FLAG_OPTIONAL)) return TRUE;
  }
  return FALSE;
}


static void err_missing(lives_blueprint_t *blu, const char *name) {
  g_print("Mandatory %s not defined for blueprint type %lu !\n", name,
          weed_get_uint64_value(blu, LIVES_LEAF_BLUEPRINT_IDX, NULL));
  abort();
}


weed_plant_t *plant_from_blueprint(int pltype, ...) {
  va_list va;
  boolean foundbptr = FALSE;
  lives_blueprint_t *blu;
  weed_plant_t *opl;
  lives_valdef_t *def;
  char *pltypestr = LSPF("%"PRIu64, pltype);
  // get blueprint for type
  weed_error_t err = lives_index_get_value(&blu, allblu, pltypestr);
  if (err != WEED_SUCCESS) {
    lives_free(pltypestr);
    return NULL;
  }
  lives_free(pltypestr);
  if (!blu) return NULL;
  //
  va_start(va, pltype);
  //
  // now for each value, we will make a leaf in pl_out, reading va_value
  // if st is 0, we read a seed_type, if flag ! scalar we read ne
  lives_index_t *val_index = weed_get_plantptr_value(blu, LIVES_LEAF_VALUE_DEFS, NULL);
  //
  opl = lives_plant_new_empty();
  //
  while (1) {
    weed_seed_t st;
    weed_size_t ne = -1;
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
      st = weed_get_int_value(def, LIVES_LEAF_VALUE_TYPE, NULL);
      if (!st) st = va_arg(va, int);
      flags = weed_get_uint64_value(def, WEED_LEAF_FLAGS, NULL);
      // if flags said array then we read num_elems
      if (flags & BLU_FLAG_ARRAY) ne = va_arg(va, weed_size_t);
    }

    weed_leaf_from_varg(opl, name, st, ne, va);
    if (!lives_strcmp(name, LIVES_LEAF_BLUEPRINT_PTR)) {
      if (!foundbptr) {
        foundbptr = TRUE;
        if (!weed_get_voidptr_value(opl, name, NULL))
          weed_set_voidptr_value(opl, name, blu);
      }
    }

    //g_print("Setting lLEAF %s\n", name);

    if (!(flags & BLU_FLAG_READWRITE))
      lives_leaf_set_rdonly(opl, name, TRUE, TRUE);
    if ((flags & BLU_FLAGS_REMOVABLE) != BLU_FLAGS_REMOVABLE)
      weed_leaf_set_undeletable(opl, name, TRUE);
    if ((flags & BLU_FLAG_AUTOUNREF))
      weed_leaf_set_autounref(opl, name, TRUE);
  }
  va_end(va);

  if (!foundbptr && lives_leaf_mandatory(val_index, LIVES_LEAF_BLUEPRINT_PTR)) {
    weed_set_voidptr_value(opl, LIVES_LEAF_BLUEPRINT_PTR, blu);
  }

  // TODO - make sure all mandatory leaves have a avlue
  //

  /* const char *key; */
  /* weed_plant_t *value; */
  /* LIVES_INDEX_FOREACH(val_index, key, value, if (lives_leaf_mandatory(blu, key) && !weed_plant_has_leaf(opl, key)) */
  /* 					       {if (weed_plant_has_leaf(value, WEED_LEAF_DEFAULT)) */
  /* 						   weed_leaf_copy(opl, key, value, WEED_LEAF_DEFAULT); */
  /* 						 else err_missing(blu, key);}); */
  return opl;
}

void lives_plant_find_value(void *retloc, weed_plant_t *pl, const char *name) {
  // run find script
  // what to return ??
}


// update scripts


static lives_result_t lives_plant_check_update(weed_plant_t *pl, const char *name, weed_seed_t stype,
    int ne, va_list va) {
  // lives_result_t res = lives_plant_get_value(&plcond, pl, LIVES_LEAF_UPDATE_COND);
  lives_condition plcond = NULL;
  //allvalues_t *oldval = allvalues_from_leaf(NULL, pl, name);
  weed_error_t err;
  plcond = weed_get_voidptr_value(pl, LIVES_LEAF_UPDATE_COND, &err);
  if (err == WEED_SUCCESS && plcond) {
    if (lives_cond_eval(plcond) == LIVES_COND_FAIL)
      return LIVES_RESULT_FAIL;
  }
  return LIVES_RESULT_SUCCESS;
}

typedef weed_plant_t lives_script_t;
static lives_result_t lives_plant_set_val_valist(weed_plant_t *pl, const char *name, weed_seed_t stype,
    int ne, va_list va) {
  weed_error_t err;
  err = weed_leaf_from_varg(pl, name, stype, ne, va);
  if (err != WEED_SUCCESS) return LIVES_RESULT_ERROR;
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_plant_set_val_va(weed_plant_t *pl, const char *name,
                                      weed_seed_t stype, int ne, ...) {
  va_list va;
  lives_result_t res;
  va_start(va, ne);
  res = lives_plant_set_val_valist(pl, name, stype, ne, va);
  va_end(va);
  return res;
}


lives_result_t lives_plant_update(weed_plant_t *pl, const char *name,
                                  weed_seed_t stype, int ne, ...) {
  va_list va;
  lives_result_t res = lives_plant_check_update(pl, name, stype, ne, va);
  if (res == LIVES_RESULT_SUCCESS) {
    va_start(va, ne);
    res = lives_plant_set_val_valist(pl, name, stype, ne, va);
    va_end(va);
  }
  return res;
}


lives_result_t lives_plant_include_sub(weed_plant_t *parent, const char *name, weed_plant_t *sub) {
  lives_result_t res;
  if (!parent || !sub || !name || !*name) return LIVES_RESULT_ERROR;

  // check update_cond
  res = lives_plant_check_update(sub, LIVES_LEAF_PARENT, WEED_SEED_VOIDPTR, -1, (void *)parent);
  if (res != LIVES_RESULT_SUCCESS) return res;

  res = lives_plant_update(parent, name, WEED_SEED_PLANTPTR, -1, sub);
  if (res != LIVES_RESULT_SUCCESS) return res;

  // run update_script
  res = lives_plant_set_val_va(sub, LIVES_LEAF_PARENT, WEED_SEED_VOIDPTR, -1, (void *)parent);
  return res;
}


//
////////////////////////////
//
// specialised plants
//
// LIVES_PLANT_INDEX
static char *name_for_index(lives_index_t *idx, const char *key) {
  const char *pfx = lives_index_get_prefix(idx);
  return LSPF("%s%s", pfx, key);
}

LIVES_GLOBAL_INLINE index_type lives_index_get_idxtype(lives_index_t *idx) {return idx ? weed_get_int_value(idx, LIVES_LEAF_INDEX_TYPE, NULL) : idx_type_anon;}
//
LIVES_GLOBAL_INLINE const char *lives_index_get_prefix(lives_index_t *idx) {return idx ? weed_get_const_string_value(idx, LIVES_LEAF_PREFIX, NULL) : NULL;}
//
LIVES_GLOBAL_INLINE weed_seed_t lives_index_get_itemtype(lives_index_t *idx) {return idx ? weed_get_int_value(idx, LIVES_LEAF_ITEM_TYPE, NULL) : WEED_SEED_INVALID;}
//
LIVES_GLOBAL_INLINE char *lives_index_get_itemname(lives_index_t *idx, const char *key) {return idx ? name_for_index(idx, key) : NULL;}
//
lives_result_t lives_index_set_value(lives_index_t *idx, const char *key, weed_seed_t stype, ...) {
  char *name = name_for_index(idx, key);

  if (weed_plant_has_leaf(idx, LIVES_LEAF_UPDATE_COND)) {
    allvalues_t *cond = (allvalues_t *)weed_get_custom_value
                        (idx, LIVES_LEAF_UPDATE_COND, LIVES_SEED_ALLVALUES, NULL);
    if (lives_cond_eval(cond, "Psi", idx, key, stype) == LIVES_COND_FAIL)
      return LIVES_RESULT_NOPERM;
  }
  va_list va;
  weed_seed_t st = lives_index_get_itemtype(idx);
  if (stype != st) return LIVES_RESULT_FAIL;
  //
  va_start(va, stype);
  weed_error_t err = weed_leaf_from_varg(idx, name, st, -1, va);
  va_end(va);
  lives_free(name);
  if (err != WEED_SUCCESS) return LIVES_RESULT_INVALID;
  return LIVES_RESULT_SUCCESS;
}
//
LIVES_GLOBAL_INLINE weed_error_t lives_index_set_autofree(lives_index_t *idx, const char *key, boolean set) {
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_set_autofree(idx, name, set);
  lives_free(name);
  return err;
}

LIVES_GLOBAL_INLINE weed_error_t lives_index_get_value(void *retloc, lives_index_t *idx, const char *key) {
  char *name = name_for_index(idx, key);
  weed_error_t err = weed_leaf_get(idx, name, 0, retloc);
  lives_free(name);
  return err;
}

LIVES_GLOBAL_INLINE boolean lives_index_contains_item(lives_index_t *idx, const char *key) {
  char *name = name_for_index(idx, key);
  boolean ret = weed_plant_has_leaf(idx, name);
  lives_free(name);
  return ret;
}

LIVES_GLOBAL_INLINE boolean lives_index_has_value(lives_index_t *idx, const char *key) {
  char *name = name_for_index(idx, key);
  boolean ret = weed_leaf_num_elements(idx, key) > 0;
  lives_free(name);
  return ret;
}

boolean lives_index_erase_value(lives_index_t *idx, const char *key) {
  boolean ret = FALSE;
  if (idx && key) {
    char *name = name_for_index(idx, key);
    if (weed_plant_has_leaf(idx, name)) {
      weed_leaf_delete(idx, name);
      lives_free(name);
      ret = TRUE;
    }
  }
  return ret;
}

/* lives_index_t *lives_index_clone(lives_index_t *index) { */
/*   lives_index_t *clone = LIVES_MAKE_INDEX(lives_index_get_idxtype(index), lives_index_get_itemtype(lives_index_t *); */
//
/* } */
//

boolean remove_from_lookup(lookup_type ltype, const char *name) {
  lives_index_t *idx = indices[ltype];
  return lives_index_erase_value(idx, name);
}


boolean remove_from_lookup_table(lives_lookup_t *lookup, const char *name) {
  return lives_index_erase_value(lookup, name);
}


lives_lookup_t *lives_make_lookup(lookup_type ltype) {
  //  lives_condition add_cond = lives_cond_create("!", "(",  $(target_object), "COND_HAS_LEAF", $(target_item), ")");
  //
  lives_lookup_t *lookup = PLANT_FROM_BLUEPRINT(LOOKUP, LIVES_LEAF_LOOKUP_TYPE,
                           ltype, LIVES_LEAF_PREFIX, LOOKUP_PREFIX,
                           LIVES_LEAF_ITEM_TYPE, LIVES_SEED_ALLVALUES);
  //						LIVES_LEAF_UPDATE_COND, upd_cond);
  return lookup;
}


static void add_to_lookup_inner(lives_index_t *idx, allvalues_t *allvp, weed_seed_t st, const char *name) {
  allvp->flags |= ALLV_FLAG_RDONLY;
  if (!lives_index_has_value(idx, name)) {
    // index will always check if there is an add condition, in the case we
    // make sure this is a unique value, if the cond fails, we get back LIVES_RESULT_NOPERM
    lives_result_t res = lives_index_set_value(idx, name, LIVES_SEED_ALLVALUES, allvp);
    if (res == LIVES_RESULT_SUCCESS) allvp->flags |= ALLV_FLAG_AUTOFREE;
  }
}


allvalues_t *add_to_lookup(lookup_type ltype, weed_seed_t st, const char *name, ...) {
  // create an allvalues from va_arg, making it readonly
  // and if name is not already in lookup, we will
  // store it and flag the allvp as nofree
  lives_index_t *idx = indices[ltype];
  if (!idx) idx = indices[ltype] = lives_make_lookup(ltype);
  va_list(va);
  va_start(va, name);
  allvalues_t *allvp = MAKE_ALLVALUE_VA(st, va);
  va_end(va);
  add_to_lookup_inner(idx, allvp, st, name);
  return allvp;
}


allvalues_t *add_to_lookup_table(lives_lookup_t *lookup, weed_seed_t st, const char *name, ...) {
  va_list(va);
  va_start(va, name);
  allvalues_t *allvp = MAKE_ALLVALUE_VA(st, va);
  va_end(va);
  add_to_lookup_inner(lookup, allvp, st, name);
  return allvp;
}


allvalues_t *find_in_lookup(lookup_type ltype, const char *name) {
  lives_index_t *idx = indices[ltype];
  if (!idx) return NULL;
  return get_databook_item(idx, name);
}

// LIVES_PLANT_DATA_BOOK
// this is a specialised type of INDEX, prefix is IDX_PREFIX, item_type is LIVES_SEED_ALLVALUES
//
allvalues_t *get_databook_item(lives_databook_t *book, const char *itemnm) {
  if (!book) return NULL;
  allvalues_t *allvp;
  if (lives_index_get_value(&allvp, book, itemnm) != WEED_SUCCESS)
    return NULL;
  if (allvp) {
    int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
    if (allvp->scope && allvp->scope != scope && allvp->scope
        != -scope && allvp->scope != 1 - scope) {
      return NULL;
    }
  }
  return allvp;
}

weed_seed_t lives_databook_get_datatype(lives_databook_t *book, const char *itemnm) {
  if (!book) return WEED_SEED_INVALID;
  allvalues_t *allvp = get_databook_item(book, itemnm);
  return allvp ? allvp->stype : WEED_SEED_INVALID;
}

lives_result_t lives_databook_set_datatype(lives_databook_t *book, const char *name, weed_seed_t itype) {
  if (!book) return LIVES_RESULT_ERROR;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  allvalues_t *allvp = get_databook_item(book, name), *retain = NULL;
  if (allvp) {
    if (!(allvp->scope && allvp->scope != scope && allvp->scope
          != -scope && allvp->scope != 1 - scope)) {
      retain = allvp;
      retain->flags &= ~ALLV_FLAG_AUTOFREE;
      allvp = NULL;
    }
  }
  if (!allvp) {
    allvp = LIVES_CALLOC_SIZEOF(allvalues_t, 1);
    allvp->stype = itype;
    allvp->scope = scope;
    if (retain) allvp->oldval = retain;
    lives_result_t res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
    return res;
  }
  if (allvp->stype == itype) return LIVES_RESULT_SUCCESS;
  return LIVES_RESULT_FAIL;
}

LIVES_GLOBAL_INLINE lives_result_t lives_databook_bind_value(lives_databook_t *book, const char *name, weed_seed_t itype,
    void *varptr) {
  if (!book) return LIVES_RESULT_ERROR;
  lives_result_t res;
  allvalues_t *allvp = get_databook_item(book, name);
  allvp = SET_ALLVALUE_BOUND(allvp, itype, varptr);
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  return res;
}

lives_result_t lives_databook_set_value_va(lives_databook_t *book, const char *name, weed_seed_t itype, va_list va) {
  if (!book) return LIVES_RESULT_ERROR;
  lives_result_t res = LIVES_RESULT_SUCCESS;
  allvalues_t *allvp = get_databook_item(book, name), *ignored = NULL;
  int scope;
  if (allvp) {
    if (allvp->stype != itype) return LIVES_RESULT_INVALID;
    scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
    if (allvp->scope < scope) {
      if (allvp->scope != 1 - scope || !allvp->ne) {
        if (allvp->scope == 1 - scope || !allvp->scope) {
          //BREAK_ME("ALLV RDONLY2");
          return LIVES_RESULT_NOPERM;
        }
        ignored = allvp;
        ignored->flags &= ~ALLV_FLAG_AUTOFREE;
        allvp = NULL;
      }
    }
  }
  //
  if (!allvp) {
    allvp = SET_ALLVALUE_VA(allvp, itype, va);
    scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
    allvp->scope = scope;
    if (ignored) allvp->oldval = ignored;
    res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
    allvp->flags |= ALLV_FLAG_AUTOFREE;
  } else {
    if (allvp->flags & ALLV_FLAG_RDONLY) return LIVES_RESULT_NOPERM;
    allvp = SET_ALLVALUE_VA(allvp, itype, va);
  }
  return res;
}

LIVES_GLOBAL_INLINE lives_result_t lives_databook_set_value(lives_databook_t *book, const char *name, weed_seed_t itype, ...) {
  lives_result_t res;
  va_list va;
  va_start(va, itype);
  res = lives_databook_set_value_va(book, name, itype, va);
  va_end(va);
  return res;
}

lives_result_t lives_databook_set_array(lives_databook_t *book, const char *name, weed_seed_t itype, int nvals, void *vals) {
  lives_result_t res;
  if (!book) return LIVES_RESULT_ERROR;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  allvalues_t *allvp = get_databook_item(book, name), *retain = NULL;
  if (allvp) {
    if (!(allvp->scope && allvp->scope != scope && allvp->scope
          != -scope && allvp->scope != 1 - scope)) {
      if (allvp && allvp->stype != itype) return LIVES_RESULT_INVALID;
      retain = allvp;
      retain->flags &= ~ALLV_FLAG_AUTOFREE;
      allvp = NULL;
    }
  }
  allvp = SET_ALLVALUE_ARRAY(allvp, itype, nvals, vals);
  allvp->scope = scope;
  allvp->oldval = retain;
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  return res;
}

lives_result_t lives_databook_pushdown_value(lives_databook_t *book, const char *name, weed_seed_t itype, ...) {
  lives_result_t res;
  if (!book) return LIVES_RESULT_ERROR;
  va_list va;
  va_start(va, itype);
  res = lives_databook_set_value_va(book, name, itype, va);
  va_end(va);
  if (res == LIVES_RESULT_SUCCESS) {
    allvalues_t *allvp = get_databook_item(book, name);
    if (allvp) allvp->scope = -allvp->scope;
  }
  return res;
}

lives_result_t lives_databook_pushdown_array(lives_databook_t *book, const char *name, weed_seed_t itype, int nvals,
    void *vals) {
  lives_result_t res;
  if (!book) return LIVES_RESULT_ERROR;
  allvalues_t *allvp = get_databook_item(book, name);
  allvp = SET_ALLVALUE_ARRAY(allvp, itype, nvals, vals);
  res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, allvp);
  if (res == LIVES_RESULT_SUCCESS) {
    allvalues_t *allvp = get_databook_item(book, name);
    allvp->scope = -allvp->scope;
  }
  return res;
}

lives_result_t lives_databook_get_value(void *retloc, lives_databook_t *book, const char *name) {
  if (!book) return LIVES_RESULT_ERROR;
  allvalues_t *allvp = get_databook_item(book, name);
  if (!allvp) return LIVES_RESULT_FAIL;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  if (allvp->scope && allvp->scope != scope && allvp->scope
      != -scope && allvp->scope != 1 - scope)
    return LIVES_RESULT_FAIL;
  get_val_from_allvals(retloc, allvp);
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_get_array_by_ref(void **array, lives_databook_t *book, const char *name, int *ne) {
  // caution - array contents can be changed even for readonly !
  *array = NULL;
  if (!book) return LIVES_RESULT_ERROR;
  allvalues_t *allvp = get_databook_item(book, name);
  if (!allvp) return LIVES_RESULT_FAIL;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  if (allvp->scope && allvp->scope != scope && allvp->scope
      != -scope && allvp->scope != 1 - scope)
    return LIVES_RESULT_FAIL;

  get_array_byref_from_allvals(array, allvp, ne);
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_get_bound_by_ref(void **var, lives_databook_t *book, const char *name) {
  *var = NULL;
  if (!book) return LIVES_RESULT_ERROR;
  allvalues_t *allvp = get_databook_item(book, name);
  if (!allvp) return LIVES_RESULT_FAIL;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  if (allvp->scope && allvp->scope != scope && allvp->scope
      != -scope && allvp->scope != 1 - scope)
    return LIVES_RESULT_FAIL;
  if (!(allvp->flags & ALLV_FLAG_POINTER)) return LIVES_RESULT_INVALID;
  get_array_byref_from_allvals(var, allvp,  NULL);
  return LIVES_RESULT_SUCCESS;
}

LIVES_GLOBAL_INLINE lives_databook_t *lives_local_databook(void) {
  GET_PROC_THREAD_SELF(self);
  if (!self) {
    LIVES_WARN("Trying to remember things, but I am not yet self aware");
  }
  return lives_proc_thread_get_book(self);
}

allvalues_t *get_local_book_item(const char *itemnm) {
  allvalues_t *allvp;
  lives_databook_t *book = lives_local_databook();
  if (!book) {
    LIVES_WARN("My mind is a blank");
  }
  lives_index_get_value(&allvp, book, itemnm);
  //
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  if (!allvp || (allvp->scope && allvp->scope != scope && allvp->scope
                 != -scope && allvp->scope != 1 - scope)) {
    char *msg = LSPF("What is this \"%s\" of which you speak ?", itemnm);
    LIVES_WARN(msg);
    lives_free(msg);
    return NULL;
  }
  return allvp;
}

allvalues_t *get_global_book_item(const char *itemnm) {
  allvalues_t *allvp;
  lives_databook_t *book = global_databook;
  lives_index_get_value(&allvp, book, itemnm);
  return allvp;
}

lives_result_t lives_databook_set_scope(lives_databook_t *dbook, const char *name, int scope) {
  if (!dbook) return LIVES_RESULT_ERROR;
  if (!name) {
    weed_set_int_value(dbook, LIVES_LEAF_SCOPE, scope);
  } else {
    allvalues_t *allvp = get_databook_item(dbook, name);
    if (!allvp) return LIVES_RESULT_INVALID;
    allvp->scope = scope;
  }
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_promote(lives_databook_t *dbook, const char *name) {
  if (!dbook) return LIVES_RESULT_ERROR;
  if (!name) return LIVES_RESULT_INVALID;
  allvalues_t *allvp = get_databook_item(dbook, name);
  if (!allvp) return LIVES_RESULT_INVALID;
  int scope = weed_get_int_value(dbook, LIVES_LEAF_SCOPE, NULL);
  if (allvp->scope && allvp->scope != scope && allvp->scope != -scope && allvp->scope != 1 - scope)
    return LIVES_RESULT_INVALID;
  if (allvp->scope != scope) return LIVES_RESULT_NOPERM;
  if (!scope) return LIVES_RESULT_FAIL;
  allvp->scope = 1 - allvp->scope;
  allvp->flags |= ALLV_FLAG_PROMOTED;
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_retain(lives_databook_t *dbook, const char *name) {
  if (!dbook) return LIVES_RESULT_ERROR;
  if (!name) return LIVES_RESULT_INVALID;
  allvalues_t *allvp = get_databook_item(dbook, name);
  if (!allvp) return LIVES_RESULT_FAIL;
  int scope = weed_get_int_value(dbook, LIVES_LEAF_SCOPE, NULL);
  if (allvp->scope && allvp->scope != scope && allvp->scope != -scope && allvp->scope != 1 - scope)
    return LIVES_RESULT_FAIL;
  if (allvp->scope != scope && allvp->scope != -scope) return LIVES_RESULT_NOPERM;
  allvp->flags &= ~ALLV_FLAG_PROMOTED;
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_descend(lives_databook_t *dbook) {
  if (!dbook) return LIVES_RESULT_ERROR;
  weed_set_int_value(dbook, LIVES_LEAF_SCOPE, weed_get_int_value(dbook, LIVES_LEAF_SCOPE, NULL) + 1);
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_ascend(lives_databook_t *dbook) {
  if (!dbook) return LIVES_RESULT_ERROR;
  weed_set_int_value(dbook, LIVES_LEAF_SCOPE, weed_get_int_value(dbook, LIVES_LEAF_SCOPE, NULL) - 1);
  return LIVES_RESULT_SUCCESS;
}

lives_result_t lives_databook_get_scope(lives_databook_t *dbook, const char *name, int *pscope) {
  if (!dbook) return LIVES_RESULT_ERROR;
  if (!name) *pscope = weed_get_int_value(dbook, LIVES_LEAF_SCOPE, NULL);
  else {
    allvalues_t *allvp = get_databook_item(dbook, name);
    if (!allvp) return LIVES_RESULT_INVALID;
    *pscope = allvp->scope;
  }
  return LIVES_RESULT_SUCCESS;
}

LIVES_LOCAL_INLINE lives_result_t  databook_clean(lives_databook_t *book) {
  // go through book. anything with scope > bbok->scope is erased
  // anything with -ve scope becomes +v
  if (!book) return LIVES_RESULT_ERROR;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  char *key;
  allvalues_t *allvp;
  LIVES_INDEX_FOREACH(book, key, allvp,
                      if (allvp->scope == -scope) allvp->scope = scope;
  else {
    if (allvp->scope > scope ||
          (allvp->scope == scope && (allvp->flags & ALLV_FLAG_PROMOTED)))
        lives_databook_erase_value(book, key);
    });
  return LIVES_RESULT_SUCCESS;
}

LIVES_GLOBAL_INLINE lives_result_t lives_localbook_clean(void) {
  return databook_clean(lives_local_databook());
}

LIVES_GLOBAL_INLINE void show_databook_contents(lives_databook_t *book) {
  const char *key;
  allvalues_t *val;
  g_print("contents of databook, scope at %d\n", weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL));
  LIVES_INDEX_FOREACH(book, key, val, g_print("%s (scope %d)\n", key, val->scope););
}

LIVES_GLOBAL_INLINE lives_result_t lives_databook_erase_value(lives_databook_t *book, const char *name) {
  // we can dlete any values at +- scope, and any values at scope - 1, provided they are flagged undeletable
  lives_result_t res = LIVES_RESULT_SUCCESS;
  if (!book) return LIVES_RESULT_ERROR;
  allvalues_t *allvp = get_databook_item(book, name), *retain = NULL;
  if (!allvp) return LIVES_RESULT_FAIL;
  int scope = weed_get_int_value(book, LIVES_LEAF_SCOPE, NULL);
  if (allvp->scope != scope && allvp->scope != -scope && allvp->scope != 1 - scope)
    return LIVES_RESULT_FAIL;
  if (allvp->scope == 1 - scope && !(allvp->flags & ALLV_FLAG_PROMOTED))
    return LIVES_RESULT_NOPERM;
  //
  if (allvp->oldval) {
    retain = allvp->oldval;
    allvp->oldval = NULL;
  }
  //
  if (retain) res = lives_index_set_value(book, name, LIVES_SEED_ALLVALUES, retain);
  else lives_index_erase_value(book, name);
  //
  if (allvp->flags & ALLV_FLAG_AUTOFREE) allvalues_free(allvp);
  if (retain) retain->flags |= ALLV_FLAG_AUTOFREE;
  //
  return res;
}

LIVES_GLOBAL_INLINE lives_result_t lives_databook_emit(lives_databook_t *dbook, weed_plant_t *src_obj) {
  // emission -  used when a hook callback is triggered
  // the target object - hook stack owner, now becomes source object for the emission
  // the consant value target object would already have been
  // set in the callback params
  if (!dbook) return LIVES_RESULT_ERROR;

  if (src_obj) {
    lives_result_t res = lives_databook_pushdown_value(dbook, LDB_SRC_OBJECT,
                         WEED_SEED_PLANTPTR, src_obj);
    if (res == LIVES_RESULT_SUCCESS)
      res = lives_databook_erase_value(dbook, LDB_TARGET_OBJECT);
    return res;
  }
  return LIVES_RESULT_INVALID;
}

LIVES_GLOBAL_INLINE lives_result_t lives_databook_end_emmission(lives_databook_t *dbook, weed_plant_t *src_obj) {
  if (!dbook) return LIVES_RESULT_ERROR;
  lives_databook_ascend(dbook);
  lives_databook_erase_value(dbook, LDB_TARGET_OBJECT);
  databook_clean(dbook);
  databook_clean(dbook);
  return LIVES_RESULT_SUCCESS;
}

LIVES_GLOBAL_INLINE lives_result_t lives_databook_inject(lives_databook_t *dbook, lives_databook_t *ctxbook) {
  // "inject" values from ctx_book into dbook, whilst preserving existing values in dbook
  // values are set a -scop, so they can be made visible, reaodnly after a book descent
  char *key;
  allvalues_t *allvp, *allvp2;
  if (!dbook) return LIVES_RESULT_ERROR;
  if (!ctxbook) return LIVES_RESULT_INVALID;
  int scope = weed_get_int_value(dbook, LIVES_LEAF_SCOPE, NULL);
  LIVES_INDEX_FOREACH(ctxbook, key, allvp,
                      allvp2 = allvalues_copy(allvp);
                      lives_index_set_value(dbook, key, LIVES_SEED_ALLVALUES, allvp2);
                      allvp->scope = -scope;);
  return  LIVES_RESULT_SUCCESS;
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
//
weed_plant_t *valplant_for_struct(const char *stname, void *struc) {
  /* weed_plant_t *vpl = blueprint(VALUE, stname, WEED_SEED_VOIDPTR, 0, struc); */
  /* weed_set_int_value(vpl, "val_dtl", STRUCT_ADAPTOR); */
  /* return vpl; */
  return NULL;
}
