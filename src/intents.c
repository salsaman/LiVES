// intents.c
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#define NEED_OBJECT_BUNDLES

#include "main.h"
//#include "bundles.h"

static boolean icaps_inited = FALSE;

static const lives_intentcap_t *std_icaps[N_STD_ICAPS];

#define STD_ICAP_ID "Standard ICAP"
#define CUSTOM_ICAP_ID "Custom ICAP"

#define DICT_SUBTYPE_OBJECT		IMkType("DICT.obj")
#define DICT_SUBTYPE_WEED_PLANT		IMkType("DICT.pla")
#define DICT_SUBTYPE_FUNCDEF		IMkType("DICT.fun")
// eventually will become lookup for all recognised uids
lives_objstore_t *main_objstore = NULL;
lives_objstore_t *fn_objstore = NULL;
lives_objstore_t *bdef_store = NULL;

////////////////////////////////
// object attr groups. For now we use the smae mechanism as the data "book" for proc_threads

// proc_thread -> obj_inst
// book -> attr_group

/// work in progress, being rewritten
// eventually this will be merged in with Nirva bundles

// "bundles" in the LiVES implementation willbe represented by weed_plants (LiVES plants)
// there will be fixed structs, only self defining data collections

// proc_threads will become on type of object instnace, clips, plugins etc, will be other objects
// abject instances. There will be no function per se (except for small inline functions)
// instead there will be "functionals", wrrapped as objects, which will combine to form "segments"
// a grouping of segment will create a "Transform", some transforms will be ervices provided by objects
// some will be provided by the infrastructure (structure transforms).

// the app will be highly modular, each modul being self contained with data inputs / outputs
// defined. The application will operate on an intents / capacities model, some transforms will be
// transactional and require a contract. hooks / hook callbacks will be
// the mechanism via which various parts of the app counicate, synchronise and exchange data.


LIVES_GLOBAL_INLINE weed_plant_t *lives_obj_instance_get_attr_group(lives_obj_instance_t *loi) {
  return loi ? weed_get_plantptr_value(loi, LIVES_LEAF_ATTR_GRP, NULL) : NULL;
}


lives_result_t lives_add_subobj(weed_plant_t *parent, const char *name, weed_plant_t *sub) {
  if (!parent || !sub || !name || !*name) return LIVES_RESULT_ERROR;
  if (weed_plant_has_leaf(parent, name)) return LIVES_RESULT_FAIL;
  weed_set_plantptr_value(parent, name, sub);

  weed_set_voidptr_value(sub, LIVES_LEAF_PARENT, (void *)parent);
  weed_leaf_set_autounref(parent, name, TRUE);
  return LIVES_RESULT_SUCCESS;
}


LIVES_GLOBAL_INLINE lives_result_t lives_obj_instance_set_attr_group(lives_obj_instance_t *loi,
    weed_plant_t *attrgrp) {
  return lives_add_subobj(loi, LIVES_LEAF_ATTR_GRP, attrgrp);
}

/* LIVES_GLOBAL_INLINE  */
/* weed_plant_t *attrgrp) { */
/*   if (loi) weed_set_plantptr_value(loi, LIVES_LEAF_ATTR_GRP, attrgrp); */
/* } */

/* LIVES_GLOBAL_INLINE boolean lives_proc_thread_has_attrgroup( */
/*   return lpt ? weed_plant_has_leaf(lpt, LIVES_LEAF_LPT_BOOK) : FALSE; */
/* } */
/* y */

LIVES_GLOBAL_INLINE weed_plant_t *lives_obj_instance_create(uint64_t type, uint64_t subtype) {
  lives_obj_instance_t *loi = lives_plant_new(LIVES_PLANT_OBJECT);
  weed_set_int64_value(loi, WEED_LEAF_UNIQUE_ID, gen_unique_id());
  weed_set_int64_value(loi, LIVES_LEAF_OBJ_TYPE, type);
  weed_set_int64_value(loi, LIVES_LEAF_OBJ_SUBTYPE, subtype);
  add_garnish(loi);
  return loi;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_obj_instance_ensure_attr_group(lives_obj_instance_t *loi) {
  if (loi) {
    lives_databook_t *attr_group = weed_get_plantptr_value(loi, LIVES_LEAF_ATTR_GRP, NULL);
    if (!attr_group) {
      attr_group = LIVES_MAKE_INDEX(idx_type_attr_grp, WEED_SEED_PLANTPTR);
      lives_obj_instance_set_attr_group(loi, attr_group);
    }
    return attr_group;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_obj_instance_share_attr_group(lives_obj_instance_t *dst,
    lives_obj_instance_t *src) {
  // if we have subordinate obj_instances, then data can be passed from one chain member to the next
  // and then finally back to the chain prime
  //
  // if src is non NULL, we take data from src, else we take data from dst
  // the function will strip any data not flagged as "static" (meaning flagged ass undletable)
  // then if dst is non-NULL and doesnt already have this data et,
  // it will get data as its data, and we add a ref to data
  //
  weed_plant_t *attr_group = NULL;
  if (dst || src) {
    if (src) attr_group = lives_obj_instance_get_attr_group(src);
    else attr_group = lives_obj_instance_get_attr_group(dst);
    if (attr_group) {
      weed_refcount_inc(attr_group);
      // this works because: with default _weed_plant_free(), any undeletable leaves are retained
      // and plant is not freed. incrementing refcount adds an undeleteable leaf
      // (refcount) if that does not already exist.
      // thus, the plant cannot be freed, but any leaves NOT flagged as undeletable WILL be deleted.
      // - undeleteable leaves include "type", "uid", the refcounter itself
      // and ANY data added as "static", thus the effect will be to delete any data not flagged as "static"

      _weed_plant_free(attr_group);

      // when extending from src to dst, we leave the added ref,
      // since when dst is freed, it will unref attr_group

      if (!dst || lives_obj_instance_get_attr_group(dst) == attr_group)
        weed_refcount_dec(attr_group);
      else lives_obj_instance_set_attr_group(dst, attr_group);
    }
  }
  return attr_group;
}


/// refcounting

LIVES_GLOBAL_INLINE int lives_obj_instance_unref(lives_obj_instance_t *obj) {
  return weed_refcount_dec(obj);
}


LIVES_GLOBAL_INLINE boolean lives_obj_instance_destroy(lives_obj_instance_t *obj) {
  // return FALSE if destroyed
  return (lives_obj_instance_unref(obj) > 0);
}


LIVES_GLOBAL_INLINE int lives_obj_instance_ref(lives_obj_instance_t *obj) {
  return weed_refcount_inc(obj);
}


void lives_thread_set_intention(lives_intention intent, lives_capacities_t *caps) {
  if (THREAD_CAPACITIES) {
    lives_capacities_free(THREAD_CAPACITIES);
    THREAD_CAPACITIES = NULL;
  }
  THREAD_INTENTION = intent;
  if (caps) THREAD_CAPACITIES = caps;
}


LIVES_GLOBAL_INLINE void lives_thread_set_intentcap(const lives_intentcap_t *icap) {
  THREAD_INTENTION = icap->intent;
  if (THREAD_CAPACITIES) lives_capacities_unref(THREAD_CAPACITIES);
  THREAD_CAPACITIES = lives_capacities_copy(NULL, icap->capacities);
}


////// object attributes //////////////

#define LIVES_ATTR_PREFIX "."

weed_plant_t *lives_obj_attr_new(const char *name, weed_seed_t st) {
  weed_plant_t *attr = lives_obj_instance_create(0, 0);
  weed_set_string_value(attr, LIVES_LEAF_NAME, name);
  weed_set_int_value(attr, LIVES_LEAF_VALUE_TYPE, st);
  return attr;
}


weed_plant_t *attr_grp_find_attr(lives_lookup_t *attr_grp, const char *name) {
  weed_plant_t *attr = NULL;
  lives_index_get_value(&attr, attr_grp, name);
  return attr;
}


weed_plant_t *lives_obj_instance_get_attribute(lives_obj_instance_t *loi, const char *name) {
  weed_plant_t *attr = NULL;
  if (!loi || !name || !*name) return NULL;
  weed_plant_t *attr_grp = lives_obj_instance_get_attr_group(loi);
  if (attr_grp) {
    return attr_grp_find_attr(attr_grp, name);
  }
  return attr;
}


LIVES_GLOBAL_INLINE weed_error_t lives_attr_unref(lives_obj_attr_t *attr) {
  if (!weed_refcount_dec(attr)) {
    //lives_index_erase_value(attr_grp, name);
    weed_plant_free(attr);
  }
  return WEED_SUCCESS;
}


LIVES_GLOBAL_INLINE weed_error_t lives_attr_ref(lives_obj_attr_t *attr) {
  return weed_refcount_inc(attr);
}



LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_param_type(lives_obj_instance_t *loi, const char *name,
    const char *label, int ptype) {
  if (loi) {
    lives_obj_attr_t *attr = lives_obj_instance_get_attribute(loi, name);
    if (attr) {
      if (label) weed_set_string_value(attr, WEED_LEAF_LABEL, label);
      weed_set_int_value(attr, WEED_LEAF_PARAM_TYPE, ptype);
      return WEED_SUCCESS;
    }
    return OBJ_ERROR_NOSUCH_ATTRIBUTE;
  }
  return OBJ_ERROR_NULL_OBJECT;
}


LIVES_GLOBAL_INLINE int lives_attribute_get_param_type(lives_obj_instance_t *loi, const char *name) {
  if (loi) {
    lives_obj_attr_t *attr = lives_obj_instance_get_attribute(loi, name);
    if (attr) return weed_get_int_value(attr, WEED_LEAF_PARAM_TYPE, NULL);
  }
  return LIVES_PARAM_TYPE_UNDEFINED;
}


// in the final version of this, we will decalre attributes for objects, and these
// will be used as template to create object intances
// for now we will creeate attributes directly in obj_instance
// first we need to ensure obj_int ha an attr_group
// - all atttributes will live in here
// then we create an attibute and add in the the attr_group
//
// for now we ue simple attributes, they have a name, a type and a value
// plus an optional "param" - a pointer to a lives_param_t
// we can store extra info in the param, e.g. max min values, widgets, list values
//
// here we declare the most basic form of an attribute
// just the name ane value type
// in order not confuse attr name with internal leaves of the plant,
//  all attr names internally are prefixed with a "."

lives_obj_attr_t *lives_obj_instance_declare_attribute(lives_obj_instance_t *loi,
    const char *name, weed_seed_t st) {
  if (!loi || !name || !*name) return NULL;
  weed_plant_t *attr_grp = lives_obj_instance_ensure_attr_group(loi);
  weed_plant_t *attr = attr_grp_find_attr(attr_grp, name);
  if (attr) {
    if (weed_get_int_value(attr, LIVES_LEAF_VALUE_TYPE, NULL) != st)
      return NULL;
    return attr;
  }
  attr = lives_obj_attr_new(name, st);

  lives_index_set_value(attr_grp, name, WEED_SEED_PLANTPTR, attr);

  return attr;
}


weed_error_t lives_obj_instance_set_attr_val(lives_obj_instance_t *loi, const char *name, ...) {
  weed_plant_t *attr = lives_obj_instance_get_attribute(loi, name);
  if (!attr) return -1; // invalid attribute
  weed_seed_t attr_type = lives_attr_get_value_type(attr);
  va_list ap;
  va_start(ap, name);
  weed_leaf_from_varg(attr, WEED_LEAF_VALUE, attr_type, (weed_size_t)1, ap);
  va_end(ap);
  return WEED_SUCCESS;
}


weed_error_t lives_obj_instance_set_attr_array(lives_obj_instance_t *loi, const char *name, int ne, ...) {
  weed_plant_t *attr = lives_obj_instance_get_attribute(loi, name);
  if (!attr) return -1; // invalid attribute
  weed_seed_t attr_type = lives_attr_get_value_type(attr);
  va_list ap;
  va_start(ap, ne);
  weed_leaf_from_varg(attr, WEED_LEAF_VALUE, attr_type, (weed_size_t)ne, ap);
  va_end(ap);
  return WEED_SUCCESS;
}


LIVES_GLOBAL_INLINE weed_seed_t lives_attr_get_value_type(lives_obj_attr_t *attr) {
  return weed_get_int_value(attr, LIVES_LEAF_VALUE_TYPE, NULL);
}

///

LIVES_GLOBAL_INLINE int lives_attribute_get_value_int(lives_obj_attr_t *attr) {
  return weed_get_int_value(attr, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE int lives_attribute_get_value_boolean(lives_obj_attr_t *attr) {
  return weed_get_boolean_value(attr, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE double lives_attribute_get_value_double(lives_obj_attr_t *attr) {
  return weed_get_double_value(attr, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE float lives_attribute_get_value_float(lives_obj_attr_t *attr) {
  return weed_get_double_value(attr, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE int64_t lives_attribute_get_value_int64(lives_obj_attr_t *attr) {
  return weed_get_int64_value(attr, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE uint64_t lives_attribute_get_value_uint64(lives_obj_attr_t *attr) {
  return weed_get_uint64_value(attr, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE char *lives_attribute_get_value_string(lives_obj_attr_t *attr) {
  if (weed_leaf_num_elements(attr, WEED_LEAF_VALUE) == 0) return NULL;
  return weed_get_string_value(attr, WEED_LEAF_VALUE, NULL);
}


////////////////////////


void *lookup_entry_full(uint64_t uid) {
  // check dictionary objects (objects and plant snapshots)
  void *thing = (void *)get_from_hash_store_i(main_objstore, uid);
  // check structdefs
  if (!thing) thing = (void *)lsd_from_store(uid);
  // check plugins
  if (!thing) thing = (void *)plugin_from_store(uid);
  return thing;
}


LIVES_GLOBAL_INLINE uint64_t lives_obj_instance_get_type(lives_obj_t *obj) {
  return obj ? weed_get_int64_value(obj, "obj_type", NULL) : 0;
}


/* LIVES_GLOBAL_INLINE int lives_obj_instance_get_state(lives_obj_t *obj) { */
/*   return obj ? weed_get_int_value(obj, "state", NULL) : 0; */
/* } */


LIVES_GLOBAL_INLINE uint64_t lives_obj_instance_get_subtype(lives_obj_t *obj) {
  return obj ? weed_get_int64_value(obj, "subtype", NULL) : 0;
}


LIVES_GLOBAL_INLINE uint64_t lives_obj_instance_get_uid(lives_obj_t *obj) {
  return obj ? weed_get_int64_value(obj, WEED_LEAF_UNIQUE_ID, NULL) : 0;
}


LIVES_GLOBAL_INLINE weed_plant_t **lives_obj_instance_get_attrs(lives_obj_t *obj) {
  return obj ? weed_get_plantptr_array(obj, "attrs", NULL) : 0;
}



size_t weigh_object(lives_obj_instance_t *obj) {
  lives_obj_attr_t **attrs = lives_obj_instance_get_attrs(obj);
  size_t tot = weed_plant_weigh(obj);
  for (int i = 0; attrs[i]; i++) tot += weed_plant_weigh(attrs[i]);
  lives_free(attrs);
  return tot;
}


// special function for funcs with no params..hah
weed_error_t set_plant_leaf_any_type_funcret(weed_plant_t *pl, const char *key, uint32_t st, weed_funcptr_t func) {
  weed_error_t err;
  allfunc_t *uberfunc = (allfunc_t *)lives_malloc(sizeof(allfunc_t));
  uberfunc->func = func;
  switch (st) {
  case WEED_SEED_INT: {
    err = weed_set_int_value(pl, key, uberfunc->funcint()); break;
  }
  case WEED_SEED_BOOLEAN: {
    err = weed_set_boolean_value(pl, key, uberfunc->funcboolean()); break;
  }
  case WEED_SEED_DOUBLE: {
    err = weed_set_double_value(pl, key, uberfunc->funcdouble()); break;
  }
  case WEED_SEED_INT64: {
    err = weed_set_int64_value(pl, key, uberfunc->funcint64()); break;
  }
  case WEED_SEED_STRING: {
    err = weed_set_string_value(pl, key, uberfunc->funcstring()); break;
  }
  case WEED_SEED_VOIDPTR: {
    err = weed_set_voidptr_value(pl, key, uberfunc->funcvoidptr()); break;
  }
  case WEED_SEED_FUNCPTR: {
    err = weed_set_funcptr_value(pl, key, uberfunc->funcfuncptr()); break;
  }
  case WEED_SEED_PLANTPTR: {
    err = weed_set_plantptr_value(pl, key, uberfunc->funcplantptr()); break;
  }
  default: return WEED_ERROR_WRONG_SEED_TYPE;
  }
  return err;
}


char *lives_obj_instance_dump_attributes(lives_obj_t *obj) {
  lives_obj_attr_t **attrs;
  //lives_obj_attr_t *attr;
  char *out = lives_strdup(""), *tmp;
  char *thing, *what;
  uint64_t uid;
  int count = 0;
  if (obj) {
    //lives_strdup_concat_sep(out, NULL, "\n", obtag);
    attrs = lives_obj_instance_get_attrs(obj);
    if (lives_obj_instance_get_type(obj) == OBJECT_TYPE_DICTIONARY
        && lives_obj_instance_get_subtype(obj) == DICT_SUBTYPE_WEED_PLANT) {
      thing = "Weed plant";
      what = "leaves";
    } else {
      thing = "Object";
      what = "attributes";
    }
    uid = lives_obj_instance_get_uid(obj);
  } else {
    attrs = THREADVAR(attributes);
    thing = "Thread";
    what = "attributes";
    uid = THREADVAR(uid);
  }
  out = lives_strdup_concat_sep(out, NULL, "%s with UID 0X%016lX ", thing, uid);
  if (attrs) {
    tmp = lives_strdup_printf("%s the following %s:", out, what);
    lives_free(out);
    out = tmp;
    for (; attrs[count]; count++) {
      const char *notes, *obs;
      char *valstr = NULL;
      weed_size_t ne = weed_leaf_num_elements(attrs[count], WEED_LEAF_VALUE);
      char *pname = weed_get_string_value(attrs[count], WEED_LEAF_NAME, NULL);
      uint32_t st = weed_leaf_seed_type(attrs[count], WEED_LEAF_VALUE);
      int type = 0;
      int64_t subtype = 0;
      if (ne) {
        if (weed_get_int_value(attrs[count], WEED_LEAF_FLAGS, NULL) & PARAM_FLAG_READONLY)
          notes = " (readonly)";
        else notes = "";
        obs = "";
      } else {
        obs = "value undefined";
        notes = "";
      }
      if (st == WEED_SEED_INT && ne == 1) {
        int ival = lives_attribute_get_value_int(attrs[count]);
        valstr = lives_strdup_printf("%d", ival);
        if (!strcmp(pname, WEED_LEAF_TYPE)) type = ival;
        if (!strcmp(pname, LIVES_LEAF_SUBTYPE)) subtype = ival;
      }
      if (st == WEED_SEED_INT64 && ne == 1) {
        int64_t i64val = lives_attribute_get_value_int64(attrs[count]);
        valstr = lives_strdup_printf("%ld", i64val);
        if (!strcmp(pname, LIVES_LEAF_SUBTYPE)) subtype = i64val;
      }

      out = lives_strdup_concat_sep(out, NULL, "\n%s%s (%s)%s%s", pname, notes,
                                    weed_seed_to_ctype(weed_leaf_seed_type(attrs[count],
                                        WEED_LEAF_VALUE), FALSE), obs, valstr);
      if (valstr) lives_free(valstr);
      lives_free(pname);
      if (type) {
        /* char *ptyp = ptype_to_string(type); */
        /* if (ptyp) { */
        /*   out = lives_strdup_concat_sep(out, NULL, "Plant type identifed as %s\n", prtyp); */
        /*   lives_free(ptyp); */
        /* } */
      }
      if (subtype) {
        //char *ptyp = psubtype_to_string(type);
        /* if (ptyp) { */
        /*   out = lives_strdup_concat_sep(out, NULL, "Plant subtype identifed as %s\n", prtyp); */
        /*   lives_free(ptyp); */
        /* } */
      }
    }
  } else {
    tmp = lives_strdup_printf("%s no attributes", out);
    lives_free(out);
    out = tmp;
  }
  return out;
}


void weed_plant_take_snapshot(weed_plant_t *plant) {
  if (plant) {
    //lives_dicto_t *dicto = weed_plant_to_objstoreo(plant);
  }
}



LIVES_GLOBAL_INLINE uint64_t contract_attr_get_owner(lives_obj_attr_t *attr) {
  if (attr) return weed_get_int64_value(attr, LIVES_LEAF_OWNER, NULL);
  return 0;
}


/* LIVES_GLOBAL_INLINE uint64_t contract_attribute_get_owner(lives_contract_t *contract, */
/* 							  const char *name) { */
/*   if (contract) return contract_attr_get_owner(lives_contract_get_attribute(contract, name)); */
/*   return 0; */
/* } */


/* LIVES_GLOBAL_INLINE boolean contract_attr_is_mine(lives_obj_attr_t *attr) { */
/*   if (attr && contract_attr_get_owner(attr) == capable->uid) return TRUE; */
/*   return FALSE; */
/* } */


/* LIVES_GLOBAL_INLINE boolean contract_attribute_is_mine(lives_obj_t *obj, const char *name) { */
/*   if (obj) return contract_attr_is_mine(lives_obj_instance_get_attribute(obj, name)); */
/*   return FALSE; */
/* } */

char *contract_attr_get_value_string(lives_contract_t *contract, lives_obj_attr_t *attr) {

  return NULL;
}


/* LIVES_GLOBAL_INLINE weed_error_t lives_attr_set_leaf_readonly(lives_obj_attr_t *attr, const char *key, boolean state) { */
/*   if (!attr) return OBJ_ERROR_NOSUCH_ATTRIBUTE; */
/*   if (!contract_attr_is_mine(attr)) return OBJ_ERROR_NOT_OWNER; */
/*   if (!strcmp(key, WEED_LEAF_VALUE)) return lives_attr_set_readonly(attr, state); */
/*   if (state) */
/*     return weed_leaf_set_flagbits(attr, key, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE); */
/*   else */
/*     return weed_leaf_clear_flagbits(attr, key, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE); */
/* } */


/* LIVES_GLOBAL_INLINE weed_error_t lives_attr_set_readonly(lives_obj_attr_t *attr, boolean state) { */
/*   if (!attr) return OBJ_ERROR_NOSUCH_ATTRIBUTE; */
/*   if (!contract_attr_is_mine(attr)) return OBJ_ERROR_NOT_OWNER; */
/*   if (!weed_leaf_num_elements(attr, WEED_LEAF_VALUE)) return OBJ_ERROR_ATTRIBUTE_INVALID; */
/*   if (state) { */
/*     weed_leaf_set_flagbits(attr, WEED_LEAF_VALUE, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE); */
/*     return weed_set_int_value(attr, WEED_LEAF_FLAGS, weed_get_int_value(attr, WEED_LEAF_FLAGS, NULL) */
/*                               | PARAM_FLAG_READONLY); */
/*   } else { */
/*     weed_leaf_clear_flagbits(attr, WEED_LEAF_VALUE, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE); */
/*     return weed_set_int_value(attr, WEED_LEAF_FLAGS, */
/*                               (weed_get_int_value(attr, WEED_LEAF_FLAGS, NULL) */
/*                                & ~PARAM_FLAG_READONLY)); */
/*   } */
/* } */


/* LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_leaf_readonly(lives_obj_t *obj, const char *name, */
/*     const char *key, boolean state) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_obj_instance_get_attribute(obj, name); */
/*     return lives_attr_set_leaf_readonly(attr, key, state); */
/*   } */
/*   return OBJ_ERROR_NULL_OBJECT; */
/* } */


/* LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_readonly(lives_obj_t *obj, const char *name, */
/*     boolean state) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_obj_instance_get_attribute(obj, name); */
/*     return lives_attr_set_readonly(attr, state); */
/*   } */
/*   return OBJ_ERROR_NULL_OBJECT; */
/* } */


/* LIVES_GLOBAL_INLINE boolean lives_attr_is_leaf_readonly(lives_obj_attr_t *attr, const char *key) { */
/*   if (attr) { */
/*     if (!lives_strcmp(key, WEED_LEAF_VALUE)) return lives_attr_is_readonly(attr); */
/*     if (weed_leaf_get_flags(attr, key) & WEED_FLAG_IMMUTABLE) return TRUE; */
/*   } */
/*   return FALSE; */
/* } */


/* LIVES_GLOBAL_INLINE boolean lives_attr_is_readonly(lives_obj_attr_t *attr) { */
/*   if (attr) { */
/*     if (weed_leaf_get_flags(attr, WEED_LEAF_VALUE) & WEED_FLAG_IMMUTABLE */
/*         || (weed_get_int_value(attr, WEED_LEAF_FLAGS, NULL) */
/*             & PARAM_FLAG_READONLY) == PARAM_FLAG_READONLY) { */
/*       if (contract_attr_is_mine(attr)) lives_attr_set_readonly(attr, TRUE); */
/*       return TRUE; */
/*     } */
/*     if (contract_attr_is_mine(attr)) lives_attr_set_readonly(attr, FALSE); */
/*   } */
/*   return FALSE; */
/* } */

/* LIVES_GLOBAL_INLINE boolean lives_attribute_is_leaf_readonly(lives_obj_t *obj, const char *name, const char *key) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_obj_instance_get_attribute(obj, name); */
/*     if (attr) return lives_attr_is_leaf_readonly(attr, key); */
/*   } */
/*   return FALSE; */
/* } */


/* LIVES_GLOBAL_INLINE boolean lives_attribute_is_readonly(lives_obj_t *obj, const char *name) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_obj_instance_get_attribute(obj, name); */
/*     if (attr) return lives_attr_is_readonly(attr); */
/*   } */
/*   return FALSE; */
/* } */

///////////////////////////////



////// capacities ///////

LIVES_GLOBAL_INLINE lives_capacities_t *lives_capacities_new(void) {
  return lives_plant_new_with_refcount(LIVES_PLANT_CAPACITIES);
}


LIVES_LOCAL_INLINE void _lives_capacities_free(lives_capacities_t *caps) {
  if (caps) weed_plant_free(caps);
}


LIVES_GLOBAL_INLINE void lives_capacities_free(lives_capacities_t *caps) {
  lives_capacities_unref(caps);
}


// ret TRUE if freed
LIVES_GLOBAL_INLINE boolean lives_capacities_unref(lives_capacities_t *caps) {
  if (caps) {
    if (!weed_refcount_dec(caps)) {
      _lives_capacities_free(caps);
      return TRUE;
    }
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE lives_capacities_t *lives_capacities_copy(lives_capacities_t *dst, lives_capacities_t *src) {
  if (!dst) return lives_plant_copy(src);
  weed_plant_duplicate_clean(dst, src);
  return dst;
}


void icap_copy(lives_intentcap_t *dst, lives_intentcap_t *src) {
  if (dst && src) {
    if (dst->capacities) lives_capacities_unref(dst->capacities);
    dst->capacities = lives_capacities_copy(NULL, src->capacities);
    dst->intent = src->intent;
  }
}


static void icap_caps_copy_cb(void *dstruc, void *sstruct, const char *sttype,
                              const char *fname, lives_capacities_t **dcaps,
                              lives_capacities_t **scaps) {
  // capacities is a ptr, so will be copied by ref, the field flag means the copied
  // value will then be mullified, so we need to create a new copy-by-value
  // (the new caps will be created with a refcount of zero)
  *dcaps = lives_capacities_copy(NULL, *scaps);
}


static void icap_caps_delete_cb(void *strct, const char *sttype,
                                const char *fname, lives_capacities_t **caps) {
  // unref caps, possibly freeing them
  if (*caps) lives_capacities_unref(*caps);
}


LIVES_LOCAL_INLINE lives_intentcap_t *lives_icap_new(lives_intention intent, lives_intentcap_t *stat) {
  static const lsd_struct_def_t *lsd = NULL;
  lives_intentcap_t *icap = NULL;
  lsd = get_lsd(LIVES_STRUCT_INTENTCAP_T);
  if (!lsd) {
    icap = (lives_intentcap_t *)lives_calloc(1, sizeof(lives_intentcap_t));
    lsd_add_special_field((lsd_struct_def_t *)lsd,
                          "capacities", LSD_FIELD_FLAG_ZERO_ON_COPY, &icap->capacities,
                          0, icap, lsd_null_cb, (lsd_field_copy_cb)icap_caps_copy_cb, NULL,
                          (lsd_field_delete_cb)icap_caps_delete_cb, NULL);
    lives_free(icap);
    icap = NULL;
  }
  if (lsd) {
    if (stat) {
      icap = lsd_struct_initialise(lsd, stat);
    } else icap = lsd_struct_create(lsd);
    if (icap) {
      icap->intent = intent;
      if (intent != OBJ_INTENTION_NOTHING) {
        icap->capacities = lives_capacities_new();
      }
    }
  }
  return icap;
}


void lives_icap_init(lives_intentcap_t *stat) {
  //lives_icap_new(OBJ_INTENTION_NOTHING, stat);
}


void lives_icap_free(lives_intentcap_t *icap) {
  if (icap) {
    // make sure we are not freeing a std index entry
    if (lsd_struct_get_generation(icap->lsd) == 1 &&
        !lives_strcmp(lsd_struct_get_class_id(icap->lsd), STD_ICAP_ID)) return;
    g_print("here\n");
    unref_struct(icap->lsd);
    g_print("here2\n");
  }
}

////////////////////////////////////

LIVES_GLOBAL_INLINE void lives_capacity_set(lives_capacities_t *caps, const char *key) {
  if (caps) {
    char *capname = MAKE_CAPNAME(key);
    weed_set_boolean_value(caps, capname, TRUE);
    lives_free(capname);
  }
}


LIVES_LOCAL_INLINE void lives_capacity_delete(lives_capacities_t *caps, const char *key) {
  if (caps) {
    char *capname = MAKE_CAPNAME(key);
    weed_leaf_delete(caps, capname);
    lives_free(capname);
  }
}


LIVES_GLOBAL_INLINE void lives_capacity_unset(lives_capacities_t *caps, const char *key) {
  lives_capacity_delete(caps, key);
}


LIVES_GLOBAL_INLINE boolean lives_has_capacity(lives_capacities_t *caps, const char *key) {
  boolean ret = FALSE;
  if (caps) {
    char *capname = MAKE_CAPNAME(key);
    ret = weed_plant_has_leaf(caps, capname);
    lives_free(capname);
  }
  return ret;
}

LIVES_LOCAL_INLINE boolean _lives_has_capnm(lives_capacities_t *caps, const char *key) {
  if (caps) return weed_plant_has_leaf(caps, key);
  return FALSE;
}


LIVES_LOCAL_INLINE lives_intentcap_t *_make_icap(lives_intention intent, va_list xargs) {
  lives_intentcap_t *icap = lives_icap_new(intent, NULL);
  char *val;
  if (icap) while ((val = va_arg(xargs, char *))) lives_capacity_set(icap->capacities, val);
  return icap;
}


lives_intentcap_t *make_icap(lives_intention intent, ...) {
  lives_intentcap_t *icap;
  va_list xargs;
  va_start(xargs, intent);
  icap = _make_icap(intent, xargs);
  va_end(xargs);
  if (icap) lsd_struct_set_class_id(icap->lsd, CUSTOM_ICAP_ID);
  return icap;
}


LIVES_LOCAL_INLINE const lives_intentcap_t *make_std_icap(const char *desc,
    lives_intention intent, ...) {
  lives_intentcap_t *icap;
  va_list xargs;
  va_start(xargs, intent);
  icap = _make_icap(intent, xargs);
  va_end(xargs);
  if (icap) {
    if (desc) lives_snprintf(icap->desc, ICAP_DESC_LEN, "%s", desc);
    lsd_struct_set_class_id(icap->lsd, STD_ICAP_ID);
  }
  return icap;
}


int _count_caps(lives_capacities_t *caps, int limit) {
  weed_size_t nleaves;
  char **capnms = weed_plant_list_leaves(caps, &nleaves);
  int count = 0;
  for (int i = 0; i < nleaves; i++) {
    if (limit == -1 || count < limit) {
      if (capnms[i]) {
        if (IS_CAPNAME(capnms[i])) count++;
        _ext_free(capnms[i]);
      }
    }
  }
  _ext_free(capnms);
  return count;
}

int count_caps(lives_capacities_t *caps) {
  return _count_caps(caps, -1);
}

int count_caps_limited(lives_capacities_t *caps, int limit) {
  return _count_caps(caps, limit);
}


char *list_caps(lives_capacities_t *caps) {
  weed_size_t nleaves;
  char *out = "";
  char **capnms = weed_plant_list_leaves(caps, &nleaves);
  for (int i = 0; i < nleaves; i++) {
    if (capnms[i]) {
      if (IS_CAPNAME(capnms[i])) {
        char *tmp = lives_strdup_printf("%s%s\n", out, capnms[i]);
        lives_free(out);
        out = tmp;
        _ext_free(capnms[i]);
      }
    }
  }
  _ext_free(capnms);
  return out;
}


static int _check_caps_match_vargs(va_list xargs, lives_capacities_t *caps, int max_misses) {
  // count misses (or hits) and stop if misses > max_misses
  // if no misses, we return -(ncaps)
  int misses = 0;
  char *name;
  if (!xargs) return 0;
  while ((name = va_arg(xargs, char *))) {
    if (lives_has_capacity(caps, name)) {
      if (misses <= 0) misses--; // no misses - count hits (negative)
    } else {
      if (misses < 0) misses = 0; // now we count misses
      if (++misses > max_misses) return misses;
    }
  }
  return misses;
}


int check_caps_match_caps(lives_capacities_t *a, lives_capacities_t *b, int max_misses) {
  // count misses (or hits) and stop if misses > max_misses
  // if no misses, we return -(hits)
  int misses = 0;
  weed_size_t nleaves;
  char **capnms = weed_plant_list_leaves(a, &nleaves);
  for (int i = 0; i < nleaves; i++) {
    if (capnms[i]) {
      if (IS_CAPNAME(capnms[i])) {
        if (_lives_has_capnm(b, capnms[i])) {
          if (misses <= 0) misses--; // no misses - count hits (negative)
        } else {
          if (misses < 0) misses = 0; // now we count misses
          if (++misses > max_misses) return misses;
        }
      }
      _ext_free(capnms[i]);
    }
  }
  if (capnms) _ext_free(capnms);
  return misses;
}


LiVESList *find_std_icaps_v(lives_intention intention, boolean allow_fuzzy,  ...) {
  LiVESList *list = NULL;
  int res;
  int nearest = -1;
  if (!allow_fuzzy) nearest = 0;
  for (int i = 0; std_icaps[i]; i++) {
    if (std_icaps[i]->intent == intention) {
      va_list xargs;
      va_start(xargs, allow_fuzzy);
      res = _check_caps_match_vargs(xargs, std_icaps[i]->capacities, nearest);
      va_end(xargs);
      if (res <= 0) {
        if (!allow_fuzzy && count_caps_limited(std_icaps[i]->capacities, res) != -res) continue;
        if (nearest < 0) {
          nearest = 0;
          if (list) lives_list_free(list);
          list = NULL;
        }
        list = lives_list_append(list, (void *)std_icaps[i]);
      } else if (allow_fuzzy) {
        if (nearest == -1 || res >= nearest) {
          if (res > nearest) {
            nearest = res;
            if (list) lives_list_free(list);
            list = NULL;
          }
          list = lives_list_append(list, (void *)std_icaps[i]);
        }
      }
    }
  }
  return list;
}


LiVESList *find_std_icaps(lives_intentcap_t *icap, boolean allow_fuzzy) {
  LiVESList *list = NULL;
  int nearest = -1;
  if (!allow_fuzzy) nearest = 0;
  for (int i = 0; std_icaps[i]; i++) {
    if (std_icaps[i]->intent == icap->intent) {
      int res = check_caps_match_caps(icap->capacities, std_icaps[i]->capacities, nearest);
      if (res <= 0) {
        if (!allow_fuzzy && count_caps_limited(std_icaps[i]->capacities, res) != -res) continue;
        if (nearest < 0) {
          nearest = 0;
          if (list) lives_list_free(list);
          list = NULL;
        }
        list = lives_list_append(list, (void *)std_icaps[i]);
      } else if (allow_fuzzy) {
        if (nearest == -1 || res >= nearest) {
          if (res > nearest) {
            nearest = res;
            if (list) lives_list_free(list);
            list = NULL;
          }
          list = lives_list_append(list, (void *)std_icaps[i]);
	  // *INDENT-OFF*
        }}}}

  return list;
}


static lives_capacities_t *_add_cap(lives_capacities_t *caps, const char *cname) {
  if (cname) {
    if (!caps) caps = lives_capacities_new();
    if (caps) lives_capacity_set(caps, cname);
  }
  return caps;
}


void native_type_view(lives_obj_attr_t *attr) {
  /* weed_get_string_value(attr, STRAND_INTROSPECTION_PTRTYPE, ctype); */
  /* weed_get_string_value(attr, STRAND_INTROSPECTION_NATIVE_TYPE, ctype); */
  /* weed_set_int64_value(attr, STRAND_INTROSPECTION_NATIVE_SIZE, size); */;
}


lives_obj_attr_t *mk_attr(const char *ctype, const char *name, size_t size, void *vptr, int ne) {
  lives_obj_attr_t *attr = lives_obj_instance_declare_attribute(NULL, name, WEED_SEED_VOIDPTR);
  weed_set_string_value(attr, ".native_type", ctype);
  //weed_set_int_value(attr, STRAND_INTROSPECTION_PTRTYPE, ctypes_to_weed_seed(ctype));
  weed_set_int64_value(attr, ".native_size", size);
  lives_obj_instance_set_attr_val(NULL, name, attr, vptr);
  return attr;
}

#define MK_ATTR(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(tdata->vars.var_##name), \
				     (void *)&(tdata->vars.var_##name), 1)

#define MK_ATTR_P(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(ctype), \
				       (void *)tdata->vars.var_##name), 1)

#define MK_ATTR_META(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(tdata->name), \
					  (void *)&(tdata->name), 1)

void make_thrdattrs(lives_thread_data_t *tdata) {
  MK_ATTR_META(uint64_t, uid);
  MK_ATTR_META(int, slot_id);
  MK_ATTR(lives_intentcap_t, intentcap);
  MK_ATTR(int, core_id);
  MK_ATTR(weed_voidptr_t, stackaddr);
  MK_ATTR(size_t, stacksize);
}

static void _caps_partition(int op, lives_capacities_t *caps_a, lives_capacities_t *caps_b, lives_capacities_t **a_only,
                            lives_capacities_t **common, lives_capacities_t **b_only, int *na, int *nc, int *nb) {
  // divide caps in a and b into 3 sets - only in a, in a & b, only in b
  // common is actually the intersection, a + b + c is the union
  // if na == 0, then a is a subset of b, if nb also == 0, then they are an exact match
  //
  // start by going through all caps of a - if it is also in b, then it goes in the common bucket, otherwise in the a bucket
  // then go through the b caps, anything which is not in the common bucket goes into b
  // op tells us what to look for so we dont waste time
  // op is a bitmap 4 == stop if na > 0, 2 == stop if nc > 0, 1 == stop if nb > 0
  // if both nb and b_only are NULL then we will stop after checking one value -
  // so this is reinterpreted to mean 'create a union in common (and or nc)'

  if (!a_only && !b_only && !common && !na && !nb && !nc) return;
  if (!caps_a || !caps_b) return;
  if (op == 6 && !nc && !common) return;
  else {
    // start by checking a
    weed_size_t numa;
    char **acaps = weed_plant_list_leaves(caps_a, &numa);
    int i;
    if (numa) {
      for (i = 0; acaps[i]; i++) {
        if (IS_CAPNAME(acaps[i])) {
          if (op == 6 || lives_has_capacity(caps_b, acaps[i])) {
            if (nc)(*nc)++;
            if (common) *common = _add_cap(*common, acaps[i]);
            if (op & 2 && !(op & 4)) return;
          } else {
            if (na)(*na)++;
            if (a_only) *a_only = _add_cap(*a_only, acaps[i]);
            if (op & 4) return;
          }
        }
        _ext_free(acaps[i]);
      }
      _ext_free(acaps);
    }
    // check b
    if (op != 6 && !nb && !b_only) return;
    if (op == 4 || op == 2) return;
    acaps = weed_plant_list_leaves(caps_b, &numa);
    if (numa) {
      for (i = 0; acaps[i]; i++) {
        if (IS_CAPNAME(acaps[i])) {
          if ((common && !lives_has_capacity(*common, acaps[i])) ||
              (!common && !lives_has_capacity(caps_a, acaps[i]))) {
            if (op != 6) {
              if (nb)(*nb)++;
              if (b_only) *b_only = _add_cap(*b_only, acaps[i]);
              if (op & 1) return;
            } else {
              if (nc)(*nc)++;
              if (common) *common = _add_cap(*common, acaps[i]);
	      // *INDENT-OFF*
            }}}
        _ext_free(acaps[i]);
      }
      _ext_free(acaps);
    }}
  // *INDENT-OFF*
}


int caps_intersection(lives_capacities_t *caps_a, lives_capacities_t *caps_b, lives_capacities_t **ret) {
  // find the intersection of caps_a with caps_b
  // if ret is not NULL, the intersection will be set in it
  // returns count of intersection
  int nc = 0;
  _caps_partition(0, caps_a, caps_b, NULL, ret, NULL, NULL, &nc, NULL);
  return nc;
}


int caps_union(lives_capacities_t *caps_a, lives_capacities_t *caps_b, lives_capacities_t **ret) {
  // find the union of caps_a with caps_b
  // if ret is not NULL, the union will be set in it
  // returns count of union
  int nc = 0;
  _caps_partition(6, caps_a, caps_b, NULL, ret, NULL, NULL, &nc, NULL);
  return nc;
}


int caps_lacking(lives_capacities_t *caps_a, lives_capacities_t *caps_b, lives_capacities_t **ret) {
  // make a list of all caps in a that are not in b
  // algorithms can try to lower the discrepancy
  int na = 0;
  _caps_partition(0, caps_a, caps_b, ret, NULL, NULL, &na, NULL, NULL);
  return na;
}


void make_std_icaps(void) {
  if (!icaps_inited) {
    lives_memset(std_icaps, 0, N_STD_ICAPS * sizeof(lives_intentcap_t *));
    icaps_inited = TRUE;
  }
  std_icaps[_ICAP_IDLE] = make_std_icap("idle", OBJ_INTENTION_NOTHING, NULL);
  std_icaps[_ICAP_DOWNLOAD] = make_std_icap("download", OBJ_INTENTION_IMPORT,
                              CAP_REMOTE, NULL);
  std_icaps[_ICAP_LOAD] = make_std_icap("load", OBJ_INTENTION_IMPORT, CAP_LOCAL, NULL);
}


const lives_intentcap_t *get_std_icap(int ref_id) {
  if (ref_id < 0 || ref_id >= N_STD_ICAPS) return NULL;
  return std_icaps[ref_id];
}


/////////////////////////////////

weed_param_t *weed_param_from_attr(lives_obj_attr_t *attr) {
  // find param by NAME, if it lacks a VALUE, set it from default
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  if (attr) {
    weed_param_t *param = weed_plant_copy(attr);
    weed_plant_mutate(param, WEED_PLANT_PARAMETER);
    if (!weed_plant_has_leaf(param, WEED_LEAF_VALUE)) {
      if (weed_plant_has_leaf(param, WEED_LEAF_DEFAULT)) {
        lives_leaf_copy(param, WEED_LEAF_VALUE, param, WEED_LEAF_DEFAULT);
      }
    }
    weed_remove_refcounter(param);
    return param;
  }
  return NULL;
}

weed_param_t *weed_param_from_attribute(lives_obj_instance_t *obj, const char *name) {
  // find param by NAME, if it lacks a VALUE, set it from default
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  lives_obj_attr_t *attr = lives_obj_instance_get_attribute(obj, name);
  return weed_param_from_attr(attr);
}


/* static boolean lives_transform_status_unref(lives_transform_status_t *st) { */
/*   // return FALSE if destroyedy */
/*   check_refcnt_init(&st->refcounter); */
/*   if (!refcount_dec(&st->refcounter)) { */
/*     lives_free(st); */
/*     return FALSE; */
/*   } */
/*   return TRUE; */
/* } */

void lives_intentparams_free(lives_intentparams_t *iparams) {
  for (int i = 0; iparams->params[i]; i++) {
    weed_plant_free(iparams->params[i]);
  }
  lives_free(iparams->params);
  lives_free(iparams);
}


#if 0
lives_intentcaps_t **list_intentcaps(void) {
  LiVESList *txlist = NULL;
  if (obj->type == OBJECT_TYPE_CLIP) {
    if (state == CLIP_STATE_NOT_LOADED) {
      // TODO - needs to be turned into functions

      lives_transform_t *tx = (lives_transform_t *)lives_calloc(sizeof(lives_transform_t), 1);
      tx->start_state = state;
      tx->icaps.intent = OBJ_INTENTION_IMPORT;
      tx->n_caps = 1;
      tx->caps = lives_calloc(sizint, 1);
      tx->caps[0] = IMPORT_LOCAL;

      tx->prereqs = (lives_rules_t *)lives_calloc(sizeof(lives_rules_t), 1);
      tx->prereqs->n_reqs = 4;
      tx->prereqs->reqs = (lives_req_t **)lives_calloc(sizeof(lives_req_t *), tx->prereqs->n_reqs);

      tx->prereqs->req[0] = string_req_init("filename", NULL);

      tx->prereqs->req[1] = double_req_init("start_time", -1., 0., 0.);

      tx->prereqs->req[2] = int_req_init("frames", NULL, -1, 0, -0);
      weed_set_int_value(req, WEED_LEAF_FLAGS, LIVES_REQ_FLAGS_OPTIONAL);

      tx->prereqs->req[3] = boolean_req_init("with_audio", TRUE);
      weed_set_int_value(req, WEED_LEAF_FLAGS, LIVES_REQ_FLAGS_OPTIONAL);

      req->new_state = CLIP_STATE_LOADED;
      ///
      // TODO..appent to list, do same for IMPORT_REMOTE but with URI
    }
  }

  if (obj->type == OBJ_TYPE_MATH) {
    if (state == STATE_NONE) {
    }
  }

  return NULL;
}
#endif


