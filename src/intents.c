// intents.c
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#define NEED_OBJECT_BUNDLES

#include "main.h"

// eventually will become lookup for all recognised uids
lives_objstore_t *main_objstore = NULL;
lives_objstore_t *fn_objstore = NULL;
lives_objstore_t *bdef_store = NULL;

static size_t dict_size = 0;

static boolean icaps_inited = FALSE;

static const lives_intentcap_t *std_icaps[N_STD_ICAPS];

#define STD_ICAP_ID "Standard ICAP"
#define CUSTOM_ICAP_ID "Custom ICAP"

#define DICT_SUBTYPE_OBJECT		IMkType("DICT.obj")
#define DICT_SUBTYPE_WEED_PLANT		IMkType("DICT.pla")
#define DICT_SUBTYPE_FUNCDEF		IMkType("DICT.fun")

// eventually this will part of a fully fledged object broker

#define DEBUG_BUNDLES 1
LIVES_GLOBAL_INLINE char *get_short_name(const char *q) {
  if (!q) return NULL;
  else {
    char *xiname = lives_strdup(".");
    char *p = (char *)q;
    if (*p != '.') {
      if (!lives_strncmp(q, "ELEM_", 5)) for (p += 5; *p && *p != '_'; p++);
      else if (!lives_strncmp(q, "ATTR_", 5)) p += 5;
      if (p != q && *p) p++;
      xiname = lives_concat(xiname, lives_string_tolower(p));
      return xiname;
    }
    return lives_strdup(p);
  }
}


uint64_t get_vflags(const char *q, off_t *offx) {
  uint64_t vflags = 0;
  if (offx)(*offx) = 0;
  if (*q == '#') {
    vflags = ELEM_FLAG_COMMENT;
    return vflags;
  }
  if (*q == '?') {
    vflags |= ELEM_FLAG_OPTIONAL;
    if (offx)(*offx)++;
  }
  return vflags;
}

uint32_t get_vtype(const char *q, off_t *offx) {
  off_t op = 0;
  if (offx) {
    op = *offx;
    (*offx)++;
  }
  return q[op];
}

LIVES_GLOBAL_INLINE const char *get_vname(const char *q) {return q;}

LIVES_GLOBAL_INLINE boolean get_is_array(const char *q) {return (q && *q == '1');}

uint32_t weed_seed_to_elem_type(uint32_t st) {
  switch (st) {
  case WEED_SEED_INT: return ELEM_TYPE_INT;
  case WEED_SEED_UINT: return ELEM_TYPE_UINT;
  case WEED_SEED_BOOLEAN: return ELEM_TYPE_BOOLEAN;
  case WEED_SEED_INT64: return ELEM_TYPE_INT64;
  case WEED_SEED_UINT64: return ELEM_TYPE_UINT64;
  case WEED_SEED_DOUBLE: return ELEM_TYPE_DOUBLE;
  case WEED_SEED_STRING: return ELEM_TYPE_STRING;
  case WEED_SEED_VOIDPTR: return ELEM_TYPE_VOIDPTR;
  case WEED_SEED_PLANTPTR: return ELEM_TYPE_BUNDLEPTR;
  default: break;
  }
  return ELEM_TYPE_NONE;
}


bundledef_t get_bundledef_from_bundle(lives_bundle_t *bundle) {
  char *sname2 = get_short_name(ELEM_INTROSPECTION_QUARKS_PTR);
  bundledef_t bundledef = (bundledef_t)weed_get_voidptr_value(bundle, sname2, NULL);
  lives_free(sname2);
  return bundledef;
}


static int bundledef_get_item_idx(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  bundle_element elem;
  off_t offx;
  uint64_t vflags;
  const char *vname;
  char *sname = exact ? (char *)item : get_short_name(item);
  for (int i = 0; (elem = bundledef[i]); i++) {
    offx = 0;
    vflags = get_vflags(elem, &offx);
    if (vflags & ELEM_FLAG_COMMENT) continue;
    get_vtype(elem, &offx);
    vname = get_vname(elem + offx);
    if (!exact) {
      char *sname2 = exact ? (char *)item : get_short_name(vname);
      if (!lives_strcmp(sname2, sname)) {
        if (sname != item) lives_free(sname);
        if (sname2 != vname) lives_free(sname2);
        return i;
      }
      if (sname2 != vname) lives_free(sname2);
    } else if (!lives_strcmp(vname, item)) {
      if (sname != item) lives_free(sname);
      return i;
    }
  }
  if (sname != item) lives_free(sname);
  return -1;
}


static uint32_t get_bundle_elem_type(lives_bundle_t *bundle, const char *name) {
  // get elem type for existing or optional element in bundle
  bundledef_t bundledef;
  char *sname = get_short_name(name);
  uint32_t st;
  st = weed_leaf_seed_type(bundle, sname);
  if (st) {
    lives_free(sname);
    return weed_seed_to_elem_type(st);
  }
  // may be an optional param
  bundledef = get_bundledef_from_bundle(bundle);
  if (bundledef) {
    int eidx = bundledef_get_item_idx(bundledef, sname, FALSE);
    if (eidx >= 0) {
      bundle_element elem = bundledef[eidx];
      off_t offx = 0;
      uint32_t vtype;
      get_vflags(elem, &offx);
      vtype = get_vtype(elem, &offx);
      lives_free(sname);
      return vtype;
    }
  }
  return ELEM_TYPE_NONE;
}


static boolean set_bundle_val_varg(lives_bundle_t *bundle, const char *iname, uint32_t vtype,
                                   uint32_t ne, boolean array, va_list vargs) {
  // THIS IS FOR ELEM_* values, we handle ATTR_* elsewhere
  // set a value / array in bundle, element with name "name"
  // ne holds number of elements, array tells us whether value is array or scalar
  // value holds the data to be set
  // vtype is the 'extended' type, which can be one of:
  // INT, UINT, DOUBLE, FLOAT, BOOLEAN, STRING, CHAR, INT64, UINT64
  // VOIDPTR, BUNDLEPTR or SPECIAL
  // - SPECIAL types are handled elsewhere
  char *etext = NULL, *xiname = lives_strdup(".");
  boolean err = FALSE;
  xiname = get_short_name(iname);
  if (!vargs) {
    etext = lives_strdup_concat(etext, "\n", "missing value setting item %s [%s] in bundle.",
                                xiname, iname);
    err = TRUE;
  } else {
    if (!array) {
      switch (vtype) {
      case (ELEM_TYPE_INT): {
        int val = va_arg(vargs, int);
        weed_set_int_value(bundle, xiname, val);
      }
      break;
      case (ELEM_TYPE_UINT): {
        uint val = va_arg(vargs, uint);
        weed_set_uint_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_BOOLEAN: {
        int val = va_arg(vargs, int);
        weed_set_boolean_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_DOUBLE: {
        double val = va_arg(vargs, double);
        weed_set_double_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_STRING: {
        char *val = lives_strdup(va_arg(vargs, char *));
        weed_set_string_value(bundle, xiname, val);
        lives_free(val);
      }
      break;
      case ELEM_TYPE_INT64: {
        int64_t val = va_arg(vargs, int64_t);
        weed_set_int64_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_UINT64: {
        uint64_t val = va_arg(vargs, uint64_t);
        weed_set_uint64_value(bundle, xiname, (uint64_t)val);
      }
      break;
      case ELEM_TYPE_BUNDLEPTR: {
        weed_plant_t *val = va_arg(vargs, weed_plant_t *);
        weed_set_plantptr_value(bundle, xiname, val);
      }
      break;
      case ELEM_TYPE_VOIDPTR: {
        void *val = va_arg(vargs, void *);
        weed_set_voidptr_value(bundle, xiname, val);
      }
      break;
      default:
        etext = lives_strdup_concat(etext, "\n", "type invalid for %s in bundle.", xiname);
        err = TRUE;
        break;
      }
    } else {
      switch (vtype) {
      case ELEM_TYPE_INT: {
        int *val = va_arg(vargs, int *);
        weed_set_int_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_UINT: {
        uint32_t *val = va_arg(vargs, uint32_t *);
        weed_set_uint_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_BOOLEAN: {
        int *val = va_arg(vargs, int *);
        weed_set_boolean_array(bundle, xiname, ne, val);
      }
      break;
      //
      case ELEM_TYPE_DOUBLE: {
        double *val = va_arg(vargs, double *);
        weed_set_double_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_STRING: {
        char **val = va_arg(vargs, char **);
        char **xval = lives_calloc(ne, sizeof(char *));
        for (int j = 0; j < ne; j++) {
          if (val[j]) xval[j] = lives_strdup(val[j]);
          else xval[j] = NULL;
        }
        weed_set_string_array(bundle, xiname, ne, val);
        for (int j = 0; j < ne; j++) if (xval[j]) lives_free(xval[j]);
        lives_free(xval);
      }
      break;
      case ELEM_TYPE_INT64: {
        int64_t *val = va_arg(vargs, int64_t *);
        weed_set_int64_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_UINT64: {
        uint64_t *val = va_arg(vargs, uint64_t *);
        weed_set_uint64_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_BUNDLEPTR: {
        weed_plant_t **val = va_arg(vargs, weed_plant_t **);
        weed_set_plantptr_array(bundle, xiname, ne, val);
      }
      break;
      case ELEM_TYPE_VOIDPTR: {
        void **val = va_arg(vargs, void **);
        weed_set_voidptr_array(bundle, xiname, ne, val);
      }
      break;
      default:
        etext = lives_strdup_concat(etext, "\n", "type invalid for %s in bundle.", xiname);
        err = TRUE;
        break;
      }
    }
  }
  if (err) {
    if (etext) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: %s\n", etext);
#endif
      lives_free(etext);
    }
#if DEBUG_BUNDLES
    else g_printerr("\nUnspecified error\n");
#endif
  }
  lives_free(xiname);
  return err;
}


static boolean set_bundle_val(lives_bundle_t *bundle, const char *name, uint32_t vtype,
                              uint32_t ne, boolean array, ...) {
  va_list vargs;
  boolean bval;
  va_start(vargs, array);
  bval = set_bundle_val_varg(bundle, name, vtype, ne, array, vargs);
  va_end(vargs);
  return bval;
}


static boolean set_bundle_value(lives_bundle_t *bundle, const char *name, ...) {
  va_list varg;
  char *sname = get_short_name(name);
  uint32_t vtype = get_bundle_elem_type(bundle, name);
  boolean bval = FALSE;
  if (!vtype) {
#if DEBUG_BUNDLES
    g_printerr("Item %s [%s] not found in bundle\n", sname, name);
#endif
  } else {
    va_start(varg, name);
    bval = set_bundle_val_varg(bundle, name, vtype, 1, FALSE, varg);
    va_end(varg);
  }
  lives_free(sname);
  return bval;
}


static boolean set_bundle_values(lives_bundle_t *bundle, ...) {
  va_list vargs;
  char *name, *sname;
  boolean bval;
  va_start(vargs, bundle);
  while (1) {
    va_list xargs;
    uint32_t vtype;
    name = va_arg(vargs, char *);
    if (!name) break;
    sname = get_short_name(name);
    va_copy(xargs, vargs);
    vtype = get_bundle_elem_type(bundle, name);
    bval = set_bundle_val_varg(bundle, name, vtype, 1, FALSE, xargs);
    va_end(vargs);
    va_copy(vargs, xargs);
    va_end(xargs);
    lives_free(sname);
    if (bval) break;
  }
  va_end(vargs);
  return bval;
}


// create a contract for negociating a "transform"
// negotiation steps:
// - remove unecessary capacties
// call negotiate for target object, object will respond by either
//   readjusting the caps, or
// adding (more) mandatory / optional attributes / hooks,
// and marking exisiting ones optional / mandatory
// or updating contract state
// - host can readjust caps again,
// or set values for mandatory / opt attrs and hooks and try again
static lives_contract_t *create_contract_instance_vargs(lives_intention intent, va_list caps) {
  // TODO: call get_contracts on a contract template with "create instance" intent
  // the transform for that should call this function to create the output attribute
  // the created object should be in state "preview"
  // and should have its own contracts
  // - one to convert from state preview to prepare
  // -- either:
  // - one to convert from state prepare to state ready (a.k.a negotiate contract), or
  // - one to convert from state preview to ready, (IF the transform is flagged "no negociate")
  lives_bundle_t *contract = create_object_bundle(OBJECT_TYPE_CONTRACT, NO_SUBTYPE);
  if (contract) {
    //set_bundle_value(contract, "intention", intent);
    if (caps) {
      lives_capacities_t *cap = lives_capacities_new();
      while (1) {
        char *cname = va_arg(caps, char *);
        if (cname == CAP_END) break;
        lives_capacity_set(cap, cname);
      }
      set_bundle_value(contract, "capacities", cap);
    }
    // TODO - should be PREVIEW
    set_bundle_value(contract, "state", OBJECT_STATE_PREPARE);
  }
  return contract;
}


lives_obj_attr_t *lives_contract_declare_attribute(lives_contract_t *contract, const char *name,
    uint32_t atype) {
  // TODO


  return NULL;
}


void *lookup_entry_full(uint64_t uid) {
  // check dictionary objects (objects and plant snapshots)
  void *thing = (void *)get_from_hash_store_i(main_objstore, uid);
  // check structdefs
  if (!thing) thing = (void *)lsd_from_store(uid);
  // check plugins
  if (!thing) thing = (void *)plugin_from_store(uid);
  return thing;
}


static lives_dicto_t *lookup_entry(uint64_t uid) {
  return get_from_hash_store_i(main_objstore, uid);
}


void dump_plantdesc(lives_dicto_t *dicto) {
  //int
}


char *interpret_uid(uint64_t uid) {
  char *info = NULL;
  lives_dicto_t *dicto = lookup_entry(uid);
  if (dicto) {
    uint64_t sub = dicto->subtype;
    if (sub == DICT_SUBTYPE_OBJECT || sub == DICT_SUBTYPE_WEED_PLANT) {
      //if (sub == DICT_SUBTYPE_WEED_PLANT) dump_plantdesc(dicto);
      //else dump_obdesc(dicto);
      info = lives_object_dump_attributes(dicto);
    } else if (sub == DICT_SUBTYPE_FUNCDEF) {
      //lives_obj_attr_t *attr = lives_object_get_attribute(dicto, ELEM_INTROSPECTION_NATIVE_PTR);
      lives_obj_attr_t *attr = lives_object_get_attribute(dicto, ELEM_INTROSPECTION_NATIVE_PTR);
      lives_funcdef_t *funcdef = (lives_funcdef_t *)weed_get_voidptr_value(attr, WEED_LEAF_VALUE, NULL);
      info = lives_funcdef_explain(funcdef);
    }
  }
  return info;
}


lives_dicto_t  *_make_dicto(lives_dicto_t *dicto, lives_intention intent,
                            lives_object_t *obj, va_list xargs) {
  // dict entry, object with any type of attributes but no transforms
  // copy specified attributes from obj into the dictionary
  // if intent is OBJ_INTENTION_UPDATE then we only overwrite, otherwise we replace all

  char *aname;
  int count = 0;
  uint32_t st;

  // intents - update - set with new vals, no delete
  // replace - delete old add new

  if (!dicto) dicto = lives_object_instance_create(OBJECT_TYPE_DICTIONARY, DICT_SUBTYPE_OBJECT);
  else if (intent == OBJ_INTENTION_REPLACE && dicto->attributes) {
    lives_object_attributes_unref_all(dicto);
  }
  while (1) {
    lives_obj_attr_t *attr = NULL, *xattr = NULL;
    if (xargs) {
      aname = va_arg(xargs, char *);
      if (aname) attr = lives_object_get_attribute(obj, aname);
    } else {
      attr = obj->attributes[count++];
      if (attr) aname = weed_get_string_value(attr, WEED_LEAF_NAME, NULL);
    }
    if (!attr) break;

    st = lives_attr_get_value_type(attr);
    if (intent != OBJ_INTENTION_REPLACE) {
      xattr  = lives_object_get_attribute(dicto, aname);
      if (xattr) lives_object_attribute_unref(dicto, xattr);
    }
    xattr = lives_object_declare_attribute(dicto, aname, st);
    if (xattr) {
      // TODO = warn if attr type changing
      weed_plant_duplicate_clean(xattr, attr);
      //lives_attr_set_readonly(xattr, lives_attr_is_readonly(attr));
      /* if (!contract_attr_is_mine(xattr)) */
      /*   weed_leaf_clear_flagbits(xattr, WEED_LEAF_VALUE, */
      /*                            WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE); */
    }
    if (!xargs) lives_free(aname);
  }
  return dicto;
}


lives_dicto_t  *replace_dicto(lives_dicto_t *dicto, lives_object_t *obj, ...) {
  // replace attributes if already there
  va_list xargs;
  va_start(xargs, obj);
  dicto = _make_dicto(dicto, OBJ_INTENTION_REPLACE, obj, xargs);
  va_end(xargs);
  return dicto;
}


lives_dicto_t  *update_dicto(lives_dicto_t *dicto, lives_object_t *obj, ...) {
  // update a subset of attributes if already there
  va_list xargs;
  va_start(xargs, obj);
  dicto = _make_dicto(dicto, OBJ_INTENTION_UPDATE, obj, xargs);
  va_end(xargs);
  return dicto;
}


static lives_objstore_t *_update_dictionary(lives_objstore_t *objstore, lives_intention intent, uint64_t uid,
    lives_dicto_t *dicto) {
  // if intent == REPLACE replace existing entry, otherwise do not add if already there
  if (!objstore) objstore = lives_hash_store_new("main store");
  else if (intent != OBJ_INTENTION_REPLACE && get_from_hash_store_i(objstore, uid)) return objstore;
  return add_to_hash_store_i(objstore, uid, (void *)dicto);
}


static lives_objstore_t *add_to_objstore(lives_objstore_t *objstore, uint64_t uid, lives_dicto_t *dicto) {
  // add only if not there
  return _update_dictionary(objstore, OBJ_INTENTION_NOTHING, uid, dicto);
}


static lives_objstore_t *update_dictionary(lives_objstore_t *objstore, uint64_t uid, lives_dicto_t *dicto) {
  // add if not there, else update
  return _update_dictionary(objstore, OBJ_INTENTION_REPLACE, uid, dicto);
}


uint64_t add_object_to_objstore(lives_object_t *obj) {
  lives_dicto_t *dicto = _make_dicto(NULL, 0, obj, NULL);
  main_objstore = add_to_objstore(main_objstore, obj->uid, dicto);
  dict_size += weigh_object(obj);
  return dict_size;
}


uint64_t update_object_in_objstore(lives_object_t *obj) {
  // add or update
  lives_dicto_t *dicto = _make_dicto(NULL, 0, obj, NULL);
  main_objstore = update_dictionary(main_objstore, obj->uid, dicto);
  //dict_size += weigh_object(obj);
  return dict_size;
}


uint64_t add_weed_plant_to_objstore(weed_plant_t *plant) {
  // only add if not there
  lives_dicto_t *dicto;
  weed_error_t err;
  uint64_t uid = weed_get_uint64_value(plant, LIVES_LEAF_UID, &err);
  if (err == WEED_ERROR_NOSUCH_LEAF) {
    uid = gen_unique_id();
    weed_set_uint64_value(plant, LIVES_LEAF_UID, uid);
  } else if (err != WEED_SUCCESS) return 0;
  dicto  = weed_plant_to_dicto(plant);
  dict_size += weed_plant_weigh(plant);
  main_objstore = update_dictionary(main_objstore, uid, dicto);
  return dict_size;
}


lives_objstore_t *remove_from_objstore(uint64_t key) {
  main_objstore = remove_from_hash_store_i(main_objstore, key);
  return main_objstore;
}


LIVES_GLOBAL_INLINE lives_dicto_t *lives_dicto_new(uint64_t subtype) {
  return lives_object_instance_create(OBJECT_TYPE_DICTIONARY, subtype);
}

lives_dicto_t *weed_plant_to_dicto(weed_plant_t *plant) {
  lives_dicto_t *dicto = NULL;
  weed_size_t nleaves;
  char **leaves = weed_plant_list_leaves(plant, &nleaves);
  if (nleaves) {
    dicto = lives_object_instance_create(OBJECT_TYPE_DICTIONARY, DICT_SUBTYPE_WEED_PLANT);
    for (int i = 0; leaves[i]; i++) {
      char *key = leaves[i];
      uint32_t st = weed_leaf_seed_type(plant, key);
      lives_obj_attr_t *attr = lives_object_declare_attribute(dicto, key, st);
      weed_leaf_copy(attr, WEED_LEAF_VALUE, plant, key);
      lives_free(leaves[i]);
    }
    lives_free(leaves);
  }
  return dicto;
}


const lives_funcdef_t *add_fn_lookup(lives_funcptr_t func, const char *name, const char *rttype,
                                     const char *args_fmt, char *file, int line, void *txmap) {
  uint32_t rtype = get_seedtype(rttype[0]);
  const lives_funcdef_t *funcdef = get_from_hash_store(fn_objstore, name);
  if (!funcdef) {
    lives_dicto_t *dicto = lives_object_instance_create(OBJECT_TYPE_DICTIONARY, DICT_SUBTYPE_FUNCDEF);
    lives_obj_attr_t *xattr = lives_object_declare_attribute(dicto, ELEM_INTROSPECTION_NATIVE_PTR,
                              WEED_SEED_VOIDPTR);
    funcdef = create_funcdef(name, func, rtype, args_fmt, file, line ? ++line : 0, txmap);
    lives_object_set_attr_value(dicto, xattr, funcdef);
    fn_objstore = add_to_objstore(fn_objstore, funcdef->uid, dicto);
  }
  return funcdef;
}


const lives_funcdef_t *get_template_for_func(const char *funcname) {
  return get_from_hash_store(fn_objstore, funcname);
}


const lives_funcdef_t *get_template_for_func_by_uid(uint64_t uid) {
  lives_dicto_t *dicto = lookup_entry(uid);
  if (dicto && dicto->subtype == DICT_SUBTYPE_FUNCDEF) {
    lives_obj_attr_t *attr = lives_object_get_attribute(dicto, ELEM_INTROSPECTION_NATIVE_PTR);
    return weed_get_voidptr_value(attr, WEED_LEAF_VALUE, NULL);
  }
  return NULL;
}


LIVES_GLOBAL_INLINE char *get_argstring_for_func(const char *funcname) {
  const lives_funcdef_t *fdef = get_template_for_func(funcname);
  if (!fdef) return NULL;
  return funcsig_to_string(funcsig_from_args_fmt(fdef->args_fmt));
}


char *lives_funcdef_explain(const lives_funcdef_t *funcdef) {
  if (funcdef) {
    char *out = lives_strdup_printf("Function with uid 0X%016lX has prototype:\n"
                                    "\t%s %s(%s)\n"
                                    "function category is %d",
                                    funcdef->uid, weed_seed_to_ctype(funcdef->return_type, FALSE),
                                    funcdef->funcname ? funcdef->funcname : "??????",
                                    funcsig_to_string(funcsig_from_args_fmt(funcdef->args_fmt)),
                                    funcdef->category);
    return out;
  }
  return NULL;
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


LIVES_GLOBAL_INLINE int lives_attribute_get_value_int(lives_obj_attr_t *attr) {
  if (attr) {
    weed_param_t *param = weed_param_from_attr(attr);
    int val = weed_param_get_value_int(param);
    weed_plant_free(param);
    return val;
  }
  return 0;
}


/// refcounting

static void lives_object_instance_free(lives_object_instance_t *obj) {
  lives_hooks_trigger(obj, obj->hook_closures, FINAL_HOOK);

  // invalidate hooks
  for (int type = N_GLOBAL_HOOKS + 1; type < N_HOOK_FUNCS; type++) {
    lives_hooks_clear(obj->hook_closures, type);
  }

  // unref attributes
  lives_object_attributes_unref_all(obj);

  // TODO - call destructor
  if (obj->priv) lives_free(obj->priv);
  lives_free(obj);
}


LIVES_GLOBAL_INLINE int lives_object_instance_unref(lives_object_instance_t *obj) {
  int count;
  if ((count = refcount_dec(&obj->refcounter)) < 0) {
    refcount_unlock(&obj->refcounter);
    lives_object_instance_free(obj);
  }
  return count;
}


LIVES_GLOBAL_INLINE boolean lives_object_instance_destroy(lives_object_instance_t *obj) {
  // return FALSE if destroyed
  return (lives_object_instance_unref(obj) >= 0);
}


LIVES_GLOBAL_INLINE int lives_object_instance_ref(lives_object_instance_t *obj) {
  return refcount_inc(&obj->refcounter);
}


size_t weigh_object(lives_object_instance_t *obj) {
  size_t tot = 0;
  tot += sizeof(lives_object_instance_t);
  if (obj->attributes) {
    for (int i = 0; obj->attributes[i]; i++) tot += weed_plant_weigh(obj->attributes[i]);
  }
  return tot;
}

static bundledef_t get_obj_bundledef(uint64_t otype, uint64_t osubtype) {
  if (otype == OBJECT_TYPE_CONTRACT) {
    static bundle_element cb[] = {CONTRACT_BUNDLE};
    return (bundledef_t)cb;
  }
  return NULL;
}

static void lives_bundle_free(lives_bundle_t *bundle) {
  if (bundle) {
    char **leaves = weed_plant_list_leaves(bundle, NULL);
    int i = 0, nvals;
    for (char *leaf = leaves[0]; leaf; leaf = leaves[++i]) {
      if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_PLANTPTR) {
        lives_bundle_t **sub = weed_get_plantptr_array_counted(bundle, leaf, &nvals);
        for (int k = 0; k < nvals; k++) if (sub[k]) lives_bundle_free(sub[k]);
        weed_leaf_delete(bundle, leaf);
      } else if (weed_leaf_seed_type(bundle, leaf) == WEED_SEED_VOIDPTR) {
        if (weed_leaf_element_size(bundle, leaf, 0) > 0)
          lives_free(weed_get_voidptr_value(bundle, leaf, NULL));
      }
      lives_free(leaf);
    }
    if (nvals) lives_free(leaves);
    weed_plant_free(bundle);
  }
}


LIVES_GLOBAL_INLINE uint64_t lives_object_get_type(weed_plant_t *obj) {
  uint64_t otype = 0;
  if (obj) {
    lives_bundle_t *vb = weed_get_plantptr_value(obj, ELEM_OBJECT_TYPE, NULL);
    otype = (uint64_t)weed_get_uint64_value(vb, ELEM_VALUE_DATA, NULL);
  }
  return otype;
}


LIVES_GLOBAL_INLINE uint64_t lives_object_get_subtype(weed_plant_t *obj) {
  uint64_t osubtype = 0;
  if (obj) {
    lives_bundle_t *vb = weed_get_plantptr_value(obj, ELEM_OBJECT_SUBTYPE, NULL);
    osubtype = (uint64_t)weed_get_int64_value(vb, ELEM_VALUE_DATA, NULL);
  }
  return osubtype;
}


LIVES_GLOBAL_INLINE int lives_object_get_state(weed_plant_t *obj) {
  int ostate = 0;
  if (obj) {
    lives_bundle_t *vb = weed_get_plantptr_value(obj, ELEM_OBJECT_STATE, NULL);
    ostate = (uint64_t)weed_get_int64_value(vb, ELEM_VALUE_DATA, NULL);
  }
  return ostate;
}


/* static boolean handle_special_value(lives_bundle_t *bundle, bundle_type btype, */
/* 				    uint64_t otype, uint64_t osubtype, */
/* 				    const char *iname, va_list vargs) { */

static boolean init_special_value(lives_bundle_t *bundle, int btype, bundle_element elem,
                                  bundle_element elem2) {
  //if (btype == attr_bundle) {
  // setting value, default, or new_default
  //lives_bundle_t *vb = weed_get_plantptr_value(bundle, ATTR_OBJECT_TYPE, NULL);
  //uint32_t atype = (uint32_t)weed_get_int_value(vb, ATTR_VALUE_DATA, NULL);
  // TODO - set attr "value", "default" or "new_default"
  //}
  //if (elem->domain ICAP && item CAPACITIES) prefix iname and det boolean in bndle
  //if (elem->domain CONTRACT && item ATTRIBUTES) check list of lists, if iname there
  // - check if owner, check ir readonly, else add to owner list
  return FALSE;
}


static boolean set_def_value(lives_bundle_t *bundle, int btype,
                             bundle_element elem, bundle_element elem2) {
  const char *vname;
  char *sname;
  off_t offx = 0;
  uint32_t vtype;
  uint64_t vflags = get_vflags(elem, &offx);
  boolean err = FALSE;
  if (vflags & ELEM_FLAG_COMMENT) {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Trying to set default value for comment %s\n", elem);
#endif
    return TRUE;
  }
  vtype = get_vtype(elem, &offx);
  vname = get_vname(elem + offx);
  sname = get_short_name(vname);
  if (lives_strlen_atleast(elem2, 2)) {
    const char *defval = elem2 + 2;
    //boolean is_array = get_is_array(elem2);
    boolean is_array = FALSE; // just set to scalar and 1 value - defaults are mostly just 0 oe NULL
    switch (vtype) {
    case (ELEM_TYPE_SPECIAL):
      err = init_special_value(bundle, btype, elem, elem2);
      break;
    case (ELEM_TYPE_STRING): case (ELEM_TYPE_VOIDPTR):
    case (ELEM_TYPE_BUNDLEPTR):
      if (!lives_strcmp(defval, "NULL"))
        err = set_bundle_val(bundle, sname, vtype, 1, is_array, NULL);
      else if (vtype == ELEM_TYPE_STRING)
        err = set_bundle_val(bundle, sname, vtype, 1, is_array, defval);
      break;
    case (ELEM_TYPE_INT):
    case (ELEM_TYPE_UINT):
    case (ELEM_TYPE_BOOLEAN):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, atoi(defval));
      break;
    case (ELEM_TYPE_INT64):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtol(defval));
      break;
    case (ELEM_TYPE_UINT64):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtoul(defval));
      break;
    case (ELEM_TYPE_DOUBLE):
      err = set_bundle_val(bundle, sname, vtype, 1, is_array, lives_strtod(defval));
      break;
    default: break;
    }
#if DEBUG_BUNDLES
    if (!err) g_printerr("Setting default for %s [%s] to %s\n", sname, vname, defval);
#endif
    lives_free(sname);
  } else {
#if DEBUG_BUNDLES
    g_printerr("ERROR: Missing default for %s [%s]\n", sname, vname);
#endif
    err = TRUE;
  }
  return err;
}


static bundledef_t validate_bdef(int btype, bundledef_t bun, boolean base_only) {
#if DEBUG_BUNDLES
  g_printerr("\nParsing bundle definition for bundle type %d\n", btype);
#endif
  bundledef_t newq = NULL;
  bundle_element elema;
  boolean err = FALSE;
  int nq = 0;
  for (int i = 0; (elema = bun[i]); i++) {
    bundle_element elema2, elemb, elemb2;
    off_t offx = 0;
    uint64_t vflagsa, vflagsb;
    const char *vnamea, *vnameb;
    char *snamea, *snameb;
    uint32_t vtypea, vtypeb;
    int j;

    vflagsa = get_vflags(elema, &offx);
    if (vflagsa & ELEM_FLAG_COMMENT) {
#if DEBUG_BUNDLES
      g_print("%s\n", elema);
#endif
      continue;
    }
    if (!(elema2 = bun[++i])) {
#if DEBUG_BUNDLES
      g_print("%s\n", elema);
      g_print("Second quark not found !\n");
#endif
      err = TRUE;
      break;
    }
    vtypea = get_vtype(elema, &offx);
    vnamea = get_vname(elema + offx);
    snamea = get_short_name(vnamea);
    if (!newq) elemb = NULL;
    else {
      for (j = 0; (elemb = newq[j]); j += 2) {
        offx = 0;
        vflagsb = get_vflags(elemb, &offx);
        vtypeb = get_vtype(elemb, &offx);
        vnameb = get_vname(elemb + offx);
        snameb = get_short_name(vnameb);
        if (!lives_strcmp(snamea, snameb)) break;
      }
    }
    if (!elemb) {
      nq += 2;
      g_print("ADD2 %s\n", elema);
      newq = lives_realloc(newq, (nq + 1) * sizeof(char *));
      newq[nq - 2] = lives_strdup(elema);
      newq[nq - 1] = lives_strdup(elema2);
      newq[nq] = NULL;
#if DEBUG_BUNDLES
      if (!(vflagsa & ELEM_FLAG_OPTIONAL)) {
        g_print("mandatory");
      } else g_print("optional");
      if (!atoi(elema2)) g_print(" scalar");
      else g_print(" array");
      g_print(" %s [%s] found (data is %s)\n", snamea, vnamea, elema2);
#endif
      lives_free(snamea);
      continue;
    } else {
      elemb2 = newq[++j];
      //
      if (lives_strcmp(vnamea, vnameb)) {
        // dupl from2 domains
#if DEBUG_BUNDLES
        g_print("Duplicate items found: %s and %s cannot co-exist in same bundledef !\n",
                vnamea, vnameb);
#endif
        err = TRUE;
      } else {
#if DEBUG_BUNDLES
        g_print("Duplicate item found for %s [%s]\n", snamea, vnamea);
#endif
        if (vtypea != vtypeb) {
#if DEBUG_BUNDLES
          g_print("ERROR: types do not match, we had %d and now we have %d\n",
                  vtypeb, vtypea);
#endif
          err = TRUE;
        } else {
          if (lives_strcmp(elema2, elemb2)) {
            g_print("Mismatch in quark2: we had %s and now we have %s\n",
                    elemb2, elema2);
            err = TRUE;
          } else {
            g_print("This is normal due to the extendable nature of bundles\n");
            if ((vflagsb & ELEM_FLAG_OPTIONAL)
                && !(vflagsa & ELEM_FLAG_OPTIONAL)) {
              g_print("Replacing optional value with mandatory\n");
              lives_free(newq[j]); lives_free(newq[j + 1]);
              newq[j] = lives_strdup(elema);
              newq[j + 1] = lives_strdup(elema2);
            } else {
              g_print("Ignoring duplicate\n");
	      // *INDENT-OFF*
	    }}}}}
    lives_free(snameb);
  }
  // *INDENT-ON*
  if (err) {
    g_print("ERRORS found in bundledef - INVALID - please correct its definition\n");
    if (newq) {
      for (int i = 0; i < nq; i++) lives_free(newq[i]);
      lives_free(newq);
      newq = NULL;
    }
  }
  return newq;
}


static lives_objstore_t *add_to_bdef_store(uint64_t fixed, lives_obj_t *bstore) {
  // if intent == REPLACE replace existing entry, otherwise do not add if already there
  if (!bdef_store) bdef_store = lives_hash_store_new("flat_bundledef store");
  else if (get_from_hash_store_i(bdef_store, fixed)) return bdef_store;
  return add_to_hash_store_i(bdef_store, fixed, (void *)bstore);
}


LIVES_GLOBAL_INLINE boolean bundle_has_item(lives_bundle_t *bundle, const char *item) {
  char *sname = get_short_name(item);
  boolean bval;
  bval = weed_plant_has_leaf(bundle, sname);
  lives_free(sname);
  return bval == WEED_TRUE;
}


static boolean bundledef_has_item(bundledef_t bundledef,
                                  const char *item, boolean exact) {
  if (bundledef_get_item_idx(bundledef, item, exact) >= 0) return TRUE;
  return FALSE;
}


static boolean add_item_to_bundle(bundle_type btype, bundledef_t bundledef,
                                  boolean base_only, lives_bundle_t *bundle,
                                  const char *item, boolean add_value, bundledef_t *bdef, int *nq) {
  // check if ITEM is in bundledef, if so && add_value: add it to bundle, if not already there
  // if exact then match by full name, else by short name
  // if quarks and nq, then we will added to bdef and add 2 to nq
  bundle_element elem, elem2;
  off_t offx;
  uint64_t vflags;
  const char *vname;
  char *sname = NULL, *sname2 = NULL;
  char *etext = NULL;
  boolean err = FALSE;
  int i;

  sname = get_short_name(item);
  if (base_only && sname != item) {
    // if adding to a BASE bundle, we can only add simple elements (for now)
    if (lives_strncmp(item, "ELEM_", 5)) {
      etext = lives_strdup_concat(etext, "\n", "%s is not a base element.", item);
      err = TRUE;
      goto endit;
    }
  }
  for (i = 0; (elem = bundledef[i]); i++) {
    offx = 0;
    vflags = get_vflags(elem, &offx);
    if (vflags & ELEM_FLAG_COMMENT) continue;
    get_vtype(elem, &offx); // dont care about type for now
    vname = get_vname(elem + offx);
    if (!lives_strcmp(vname, item)) break;
    sname2 = get_short_name(vname);
    if (!lives_strcmp(sname2, sname)) break;
    lives_free(sname2);
    sname2 = NULL;
  }
  if (err) goto endit;
  if (!elem) {
    etext = lives_strdup_concat(etext, "\n", "item %s not found in bundledef.", item);
    err = TRUE;
    goto endit;
  }
  if (!(elem2 = bundledef[++i])) {
    etext = lives_strdup_concat(etext, "\n",
                                "quark2 for %s not found in bundledef !", bundledef[--i]);
    err = TRUE;
    goto endit;
  }

  if (!bundle_has_item(bundle, item)) {
    if (add_value) {
      err = set_def_value(bundle, btype, elem, elem2);
      if (err) goto endit;
    }
    if (bdef) {
      *nq += 2;
      *bdef = lives_realloc(*bdef, (*nq + 1) * sizeof(char *));
      (*bdef)[*nq - 2] = lives_strdup(elem);
      (*bdef)[*nq - 1] = lives_strdup(elem2);
      (*bdef)[*nq] = NULL;
      g_print("ADD %s\n", elem);
    }
  }
endit:
  if (err) {
    if (etext) {
#if DEBUG_BUNDLES
      g_printerr("\nERROR: %s\n", etext);
#endif
      lives_free(etext);
    }
  }
  if (sname) lives_free(sname);
  if (sname2) lives_free(sname2);
  return err;
}


static lives_bundle_t *create_bundle_from_def_vargs(bundle_type btype, bundledef_t xbundledef,
    va_list vargs) {
  // what we do in this function is parse vargs, a list of minimum item names to be included
  // in the bundle. If an item is not found in the bundldef, show an error and return NULL.
  // if a name is given multiple times, we ingore the duplicates after the first.
  // After doing this, we then add defaults for any non-optional items not specified in the
  // parameter list.
  // if an item appears more than once in the  bundldef, then we should eliminate the duplicates.
  // However if it appears as both "optional" and "non-optional" then the copy marked as optional
  // should be the one to be removed.
  // Fianlly we will end up with a bundle with all non-optional items set to defaults,
  // and any optional items in the parameter list included and set to defaults.
  // For convenience, if the bundle includes optional "INTROSPECTION_QUARKS", we place a copy
  // of the pared dwon quark array in it. This will be used later when adding or removing items
  // from the bundle. New optional items may be added, and optional items may also be removed
  // (deleted).
  lives_bundle_t *bundle = NULL, *xbundle = NULL;
  bundledef_t bundledef;
  char *etext = NULL;

  if (!(bundledef = validate_bdef(btype, xbundledef, btype != attr_bundle))) {
    g_printerr("ERROR: INVALID BUNDLEDEF !!\n");
    return NULL;
  }
  //

  bundle = weed_plant_new(LIVES_PLANT_BUNDLE);

  if (bundle) {
    // first we check the vargs. If an item is optional, we create it and set default,
    // and append the quarks to new_quakrs.
    //
    // Then we parse the original quarks. Any optional ones are skipped.
    // If we find a non-optional item, we insert in new_quarks
    // before the optional user added ones.
    // If it was already created as optional, we remove the optional version from new_quarks,
    // otherwise we create the element and set it to its default value.
    bundledef_t new_quarks = NULL;
    boolean err = FALSE;
    bundle_element elem;
    const char *vname;
    char *sname;
    off_t offx;
    uint64_t vflags;
    boolean add_qptr = FALSE;
    int nq = 0;

    // step 0, add default optional things
    if (bundledef_has_item(bundledef, ELEM_INTROSPECTION_QUARKS_PTR, TRUE)) {
      // add this first
      err = add_item_to_bundle(btype, bundledef, TRUE, bundle, ELEM_INTROSPECTION_QUARKS_PTR,
                               FALSE, &new_quarks, &nq);
      add_qptr = TRUE;
    }
    if (bundledef_has_item(bundledef, ELEM_GENERIC_UID, TRUE)) {
      // add this first
      add_item_to_bundle(btype, bundledef, TRUE, bundle,
                         ELEM_GENERIC_UID, FALSE, &new_quarks, &nq);
      sname = get_short_name(ELEM_GENERIC_UID);
      weed_set_uint64_value(bundle, sname, gen_unique_id());
      lives_free(sname);
      sname = NULL;
    }

    if (!err) {
      // step 1, go through params
      if (vargs) {
        while (1) {
          // caller should provide vargs, list of optional items to be initialized
          char *iname;
          iname = va_arg(vargs, char *);
          if (!iname) break; // done parsing params
          err = add_item_to_bundle(btype, bundledef, TRUE, bundle, iname, TRUE, NULL, NULL);
          if (err) break;
        }
      }
      if (!err) {
        // add any missing mandos
        for (int i = 0; (elem = bundledef[i]); i += 2) {
          offx = 0;
          vflags = get_vflags(elem, &offx);
          if (vflags & ELEM_FLAG_COMMENT) {
#if DEBUG_BUNDLES
            g_print("%s\n", elem);
#endif
            continue;
          }
          get_vtype(elem, &offx);
          vname = get_vname(elem + offx);
          sname = get_short_name(vname);
          if (bundle_has_item(bundle, sname)) {
            g_print("ADD222 %s\n", vname);
            err = add_item_to_bundle(btype, bundledef, TRUE, bundle, vname,
                                     FALSE, &new_quarks, &nq);
          } else {
            if (!(vflags & ELEM_FLAG_OPTIONAL)) {
              err = add_item_to_bundle(btype, bundledef, TRUE, bundle, vname,
                                       TRUE, &new_quarks, &nq);
            } else {
              g_print("ADD2227777 %s\n", vname);
              err = add_item_to_bundle(btype, bundledef, TRUE, bundle, vname,
                                       FALSE, &new_quarks, &nq);
            }
          }
          if (sname) {
            lives_free(sname);
            sname = NULL;
          }
          if (err) break;
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*
    if (err) {
      if (etext) {
#if DEBUG_BUNDLES
        g_printerr("\nERROR: %s\n", etext);
#endif
        lives_free(etext);
      }
#if DEBUG_BUNDLES
      else g_printerr("\nUnspecified error\n");
      g_print("Unable to create bundle.\n");
#endif
      lives_bundle_free(bundle);
      bundle = NULL;
    } else {
      bundledef_t bundledef = validate_bdef(btype, new_quarks, btype != attr_bundle);
      lives_bundle_t *bstore = create_bundle(storage_bundle, "comment", "native_ptr", NULL);
      if (bstore) {
        char *flat;
        uint64_t fixed_id = get_bundledef64sum(bundledef, &flat);
        set_bundle_value(bstore, "native_ptr", ELEM_TYPE_STRING, 1, FALSE, flat);
        set_bundle_value(bstore, "comment", ELEM_TYPE_STRING, 1, FALSE, "anon bundledef");
        bdef_store = add_to_bdef_store(fixed_id, bstore);
      }
      if (add_qptr) {
        set_bundle_val(bundle, ELEM_INTROSPECTION_QUARKS_PTR,
                       ELEM_TYPE_VOIDPTR, 1, FALSE, (void *)bundledef);
      }
    }
  }

  // reverse the leaves
  if (bundle) xbundle = weed_plant_copy(bundle);
  return xbundle;
}


char *flatten_bundledef(bundledef_t bdef) {
  char *buf;
  off_t offs = 0;
  size_t ts = 0;
  for (int i = 0; bdef[i]; i++) ts += strlen(bdef[i]) + 1;
  buf = (char *)lives_calloc(1, ts);
  for (int i = 0; bdef[i]; i++) {
    size_t s = strlen(bdef[i]);
    buf[offs++] = (uint8_t)(s & 255);
    lives_memcpy(buf + offs, bdef[i], s);
    offs += s;
  }
  return buf;
}


LIVES_GLOBAL_INLINE char *flatten_bundle(lives_bundle_t *bundle) {
  bundledef_t bdef = get_bundledef_from_bundle(bundle);
  return flatten_bundledef(bdef);
}


uint64_t get_bundledef64sum(bundledef_t bdef, char **flattened) {
  char *xfla;
  uint64_t uval;
  if (!flattened || !*flattened) {
    xfla = flatten_bundledef(bdef);
    if (flattened) *flattened = xfla;
  } else xfla = *flattened;
  uval = minimd5(xfla, lives_strlen(xfla));
  if (!flattened) lives_free(xfla);
  return uval;
}


LIVES_GLOBAL_INLINE uint64_t get_bundle64sum(lives_bundle_t *bundle, char **flattened) {
  bundledef_t bdef = get_bundledef_from_bundle(bundle);
  return get_bundledef64sum(bdef, flattened);
}


LIVES_SENTINEL lives_bundle_t *create_bundle(bundle_type btype, ...) {
  const_bundledef_t cbundledef;
  lives_bundle_t *bundle;
  va_list xargs;
  if (btype >= 0 || btype < n_bundles) {
    cbundledef = (const_bundledef_t)get_bundledef(btype);
  } else return NULL;
  va_start(xargs, btype);
  bundle = create_bundle_from_def_vargs(btype, (bundledef_t)cbundledef, xargs);
  va_end(xargs);
  return bundle;
}


lives_bundle_t *create_object_bundle(uint64_t otype, uint64_t subtype) {
  bundledef_t bundledef;
  lives_bundle_t *bundle;

  bundledef = get_obj_bundledef(otype, subtype);
  bundle = create_bundle_from_def_vargs(object_bundle, bundledef, NULL);

  // after this, we need to create contracts for the object
  // - (1) call get_contracts on a contract template with "create instance" intent
  ///
  //  (2) create a first instance with intent "negotiate", caps none, and flagged "no negotiate"
  // and mandatory attr: object type contract, state prepare.
  //
  // -- call more times to create contracts for other object transforms

  set_bundle_values(bundle, "type", otype, "subtype", subtype, NULL);
  return bundle;
}


LIVES_GLOBAL_INLINE const lives_contract_t *create_contract_template(void) {
  // a contract template will have a static contract "create instance"
  // no negociate, creates new instance in state preview, mandatory attrs intent / capacities

  // the create instance transform will create an instancem with a single
  // contract of its own for transform copy / no negoiate with new state == prepare

  return create_object_bundle(OBJECT_TYPE_CONTRACT, NO_SUBTYPE);
}



// - (1) call this on object to get contracts for various intents, these will all be in state preview
// - if the contract is flagged "no negociate" then the transform can be called directly, it just needs
//   mandatory attributes set
//
// - (2) otherwise, call the object with intent "negotiate" to find an object's negotiate function
//     (flagged no negotiate)
//  note that the negotiate transform only accepts contract instances in state prepare
///
//  (3) call this on a contract instance to get a second contract to convert preview to prepare
///        (flagged no negociate)
//    (call the transform in the contract instance to  create a copy instance
///   which can be passed to the negotiate transform)
//

lives_contract_t *get_contract_for(lives_obj_t *object, lives_intention intention) {
  //return create_object_bundle(OBJECT_TYPE_CONTRACT, NO_SUBTYPE, OBJECT_STATE_PREPARE);
  return NULL;
}


// this will get (or create) the static contract object template, get the contract in it
// for create instance, create the instance in state preview, and then return it
// for testing, this is short circuited, we return an instance already in state prepare

LIVES_GLOBAL_INLINE lives_contract_t *create_contract_instance(lives_intention intent, ...) {
  lives_contract_t *contract;
  va_list vargs;
  va_start(vargs, intent);
  contract = create_contract_instance_vargs(intent, vargs);
  va_end(vargs);
  return contract;
}


LIVES_GLOBAL_INLINE lives_object_instance_t *lives_object_instance_create(uint64_t type, uint64_t subtype) {
  lives_object_instance_t *obj_inst = lives_calloc(1, sizeof(lives_object_instance_t));
  obj_inst->uid = gen_unique_id();
  obj_inst->type = type;
  obj_inst->subtype = subtype;
  obj_inst->state = OBJECT_STATE_UNDEFINED;
  check_refcnt_init(&obj_inst->refcounter);
  return obj_inst;
}


////// object attributes //////////////

LIVES_GLOBAL_INLINE char *lives_attr_get_name(lives_obj_attr_t *attr) {
  if (attr) return weed_get_string_value(attr, WEED_LEAF_NAME, NULL);
  return NULL;
}


static void lives_object_attribute_free(lives_object_t *obj, lives_obj_attr_t *attr) {
  // TODO - free rfx_param
  lives_obj_attr_t **attrs;
  int i;
  if (obj) attrs = obj->attributes;
  else attrs = THREADVAR(attributes);
  for (i = 0; attrs[i]; i++) if (attrs[i] == attr) break;

  weed_refcounter_unlock(attr);
  weed_plant_free(attr);

  if (attrs[i] == attr) {
    for (; attrs[i]; i++) {
      attrs[i] = attrs[i + 1];
    }
    attrs = lives_realloc(attrs, (i + 1) * sizeof(lives_obj_attr_t *));
    if (obj) obj->attributes = attrs;
    else THREADVAR(attributes)  = attrs;
  }
}


LIVES_GLOBAL_INLINE boolean lives_object_attribute_unref(lives_object_t *obj, lives_obj_attr_t *attr) {
  int refs = weed_refcount_dec(attr);
  if (refs == -100) lives_abort("attr missing REFCOUNTER in weed_layer_unref");
  if (refs != -1) return TRUE;
  weed_refcounter_unlock(attr);
  lives_object_attribute_free(obj, attr);
  return FALSE;
}


LIVES_GLOBAL_INLINE void lives_object_attributes_unref_all(lives_object_t *obj) {
  lives_obj_attr_t **attrs = obj->attributes;
  if (attrs) {
    for (int count = 0; attrs[count]; count++) {
      lives_object_attribute_unref(obj, attrs[count]);
    }
    obj->attributes = NULL;
  }
}


static weed_error_t _set_obj_attribute_vargs(lives_obj_attr_t  *attr, const char *key,
    int ne, va_list args) {
  if (!args) return WEED_ERROR_NOSUCH_LEAF;
  if (attr) {
    weed_error_t err; uint32_t st = lives_attr_get_value_type(attr);
    if (ne == 1) {
      switch (st) {
      case WEED_SEED_INT: {
        int val = va_arg(args, int);
        err = weed_set_int_value(attr, key, val); break;
      }
      case WEED_SEED_BOOLEAN: {
        boolean val = va_arg(args, int);
        err = weed_set_boolean_value(attr, key, val); break;
      }
      case WEED_SEED_DOUBLE: {
        double val = va_arg(args, double);
        err = weed_set_double_value(attr, key, val); break;
      }
      case WEED_SEED_INT64: {
        int64_t val = va_arg(args, int64_t);
        err = weed_set_int64_value(attr, key, val); break;
      }
      case WEED_SEED_STRING: {
        char *val = va_arg(args, char *);
        err = weed_set_string_value(attr, key, val); break;
      }
      case WEED_SEED_VOIDPTR: {
        void *val = va_arg(args, void *);
        err = weed_set_voidptr_value(attr, key, val); break;
      }
      case WEED_SEED_FUNCPTR: {
        weed_funcptr_t val = va_arg(args, weed_funcptr_t);
        err = weed_set_funcptr_value(attr, key, val); break;
      }
      case WEED_SEED_PLANTPTR: {
        weed_plantptr_t val = va_arg(args, weed_plantptr_t);
        err = weed_set_plantptr_value(attr, key, val); break;
      }
      default: return OBJ_ERROR_ATTRIBUTE_INVALID;
      }
    } else {
      switch (st) {
      case WEED_SEED_INT: {
        int *vals = va_arg(args, int *);
        err = weed_set_int_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_BOOLEAN: {
        boolean *vals = va_arg(args, int *);
        err = weed_set_boolean_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_DOUBLE: {
        double *vals = va_arg(args, double *);
        err = weed_set_double_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_INT64: {
        int64_t *vals = va_arg(args, int64_t *);
        err = weed_set_int64_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_STRING: {
        char **vals = va_arg(args, char **);
        err = weed_set_string_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_VOIDPTR: {
        void **vals = va_arg(args, void **);
        err = weed_set_voidptr_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_FUNCPTR: {
        weed_funcptr_t *vals = va_arg(args, weed_funcptr_t *);
        err = weed_set_funcptr_array(attr, key, ne, vals); break;
      }
      case WEED_SEED_PLANTPTR: {
        weed_plantptr_t *vals = va_arg(args, weed_plantptr_t *);
        err = weed_set_plantptr_array(attr, key, ne, vals); break;
      }
      default: return OBJ_ERROR_ATTRIBUTE_INVALID;
      }
    } return err;
  }
  return OBJ_ERROR_NOSUCH_ATTRIBUTE;
}

weed_error_t lives_object_set_attribute_value(lives_object_t *obj, const char *name, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (name && *name) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (!attr) return OBJ_ERROR_NOSUCH_ATTRIBUTE;
    else {
      va_list xargs;
      va_start(xargs, name);
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
      err = _set_obj_attribute_vargs(attr, WEED_LEAF_VALUE,  1, xargs);
      va_end(xargs);
    }
    if (err == WEED_SUCCESS) {
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
    }
  }
  return err;
}


weed_error_t lives_object_set_attribute_default(lives_object_t *obj, const char *name, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (name && *name) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (!attr) return OBJ_ERROR_NOSUCH_ATTRIBUTE;
    else {
      va_list xargs;
      va_start(xargs, name);
      // TODO - hooks should be attached to the attribute itself
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures),
                          PRE_VALUE_CHANGED_HOOK);
      err = _set_obj_attribute_vargs(attr, WEED_LEAF_DEFAULT,  1, xargs);
      va_end(xargs);
    }
    if (err == WEED_SUCCESS) {
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures),
                          POST_VALUE_CHANGED_HOOK);
    }
  }
  return err;
}


weed_error_t lives_object_set_attr_value(lives_object_t *obj, lives_obj_attr_t *attr, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs;
    va_start(xargs, attr);
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
    err = _set_obj_attribute_vargs(attr, WEED_LEAF_VALUE, 1, xargs);
    va_end(xargs);
  }
  if (err == WEED_SUCCESS) {
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
  }
  return err;
}


weed_error_t lives_object_set_attr_default(lives_object_t *obj, lives_obj_attr_t *attr, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs;
    va_start(xargs, attr);
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
    err = _set_obj_attribute_vargs(attr, WEED_LEAF_DEFAULT, 1, xargs);
    va_end(xargs);
  }
  if (err == WEED_SUCCESS) {
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
  }
  return err;
}


weed_error_t lives_object_set_attribute_array(lives_object_t *obj, const char *name, weed_size_t ne, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (name && *name && ne > 0) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (!attr) return OBJ_ERROR_NOSUCH_ATTRIBUTE;
    else {
      va_list xargs;
      va_start(xargs, ne);
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
      err = _set_obj_attribute_vargs(attr, WEED_LEAF_VALUE, ne, xargs);
      va_end(xargs);
    }
    if (err == WEED_SUCCESS) {
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
    }
  }
  return err;
}


weed_error_t lives_object_set_attribute_def_array(lives_object_t *obj,
    const char *name, weed_size_t ne, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (name && *name && ne > 0) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (!attr) return OBJ_ERROR_NOSUCH_ATTRIBUTE;
    else {
      va_list xargs;
      va_start(xargs, ne);
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
      err = _set_obj_attribute_vargs(attr, WEED_LEAF_DEFAULT, ne, xargs);
      va_end(xargs);
    }
    if (err == WEED_SUCCESS) {
      lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
    }
  }
  return err;
}


weed_error_t lives_object_set_attr_array(lives_object_t *obj, lives_obj_attr_t *attr, weed_size_t ne,  ...) {
  weed_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs;
    va_start(xargs, ne);
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
    err = _set_obj_attribute_vargs(attr, WEED_LEAF_VALUE, ne, xargs);
    va_end(xargs);
  }
  if (err == WEED_SUCCESS) {
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
  }
  return err;
}


weed_error_t lives_object_set_attr_def_array(lives_object_t *obj, lives_obj_attr_t *attr, weed_size_t ne,  ...) {
  weed_error_t err = WEED_SUCCESS;
  if (attr) {
    va_list xargs;
    va_start(xargs, ne);
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), PRE_VALUE_CHANGED_HOOK);
    err = _set_obj_attribute_vargs(attr, WEED_LEAF_DEFAULT, ne, xargs);
    va_end(xargs);
  }
  if (err == WEED_SUCCESS) {
    lives_hooks_trigger(obj, obj ? obj->hook_closures : THREADVAR(hook_closures), POST_VALUE_CHANGED_HOOK);
  }
  return err;
}


lives_obj_attr_t *lives_object_get_attribute(lives_object_t *obj, const char *name) {
  if (name && *name) {
    lives_obj_attr_t **attrs;
    char *xname = get_short_name(name);
    if (obj) attrs = obj->attributes;
    else attrs = THREADVAR(attributes);
    if (attrs) {
      for (int count = 0; attrs[count]; count++) {
        char *pname = weed_get_string_value(attrs[count], WEED_LEAF_NAME, NULL);
        if (!lives_strcmp(name, pname)) {
          lives_free(pname); lives_free(xname);
          return attrs[count];
        }
        lives_free(pname);
      }
    }
    lives_free(xname);
    xname = NULL;
  }
  return NULL;
}


int lives_object_get_num_attributes(lives_object_t *obj) {
  int count = 0;
  lives_obj_attr_t **attrs;
  if (obj) attrs = obj->attributes;
  else attrs = THREADVAR(attributes);
  if (attrs) while (attrs[count++]);
  return count;
}

/* #define LIVES_WEED_SUBTYPE_MESSAGE 1 */

/* #define LIVES_WEED_SUBTYPE_WIDGET 2 */
/* #define LIVES_WEED_SUBTYPE_TUNABLE 3 */
/* #define LIVES_WEED_SUBTYPE_PROC_THREAD 4 */
/* #define LIVES_WEED_SUBTYPE_PREFERENCE 5 */

/* #define LIVES_WEED_SUBTYPE_TX_PARAM 128 */
/* #define LIVES_WEED_SUBTYPE_OBJ_ATTR 129 */

/* #define LIVES_WEED_SUBTYPE_BAG_OF_HOLDING 256 // generic - cant think of a better name right now */

/* #define LIVES_WEED_SUBTYPE_HASH_STORE 513 */


char *lives_object_dump_attributes(lives_object_t *obj) {
  lives_obj_attr_t **attrs;
  //lives_obj_attr_t *attr;
  char *out = lives_strdup(""), *tmp;
  char *thing, *what;
  uint64_t uid;
  int count = 0;
  if (obj) {
    //lives_strdup_concat(out, NULL, "\n", obtag);
    attrs = obj->attributes;
    if (obj->type == OBJECT_TYPE_DICTIONARY && obj->subtype == DICT_SUBTYPE_WEED_PLANT) {
      thing = "Weed plant";
      what = "leaves";
    } else {
      thing = "Object";
      what = "attributes";
    }
    uid = obj->uid;
  } else {
    attrs = THREADVAR(attributes);
    thing = "Thread";
    what = "attributes";
    uid = THREADVAR(uid);
  }
  out = lives_strdup_concat(out, NULL, "%s with UID 0X%016lX ", thing, uid);
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
      int type = 0, subtype = 0;
      if (ne) {
        if (weed_get_int_value(attrs[count], WEED_LEAF_FLAGS, NULL) & WEED_PARAM_FLAG_READ_ONLY)
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
        else if (!strcmp(pname, WEED_LEAF_LIVES_SUBTYPE)) subtype = ival;
      }
      out = lives_strdup_concat(out, NULL, "\n%s%s (%s)%s%s", pname, notes,
                                weed_seed_to_ctype(weed_leaf_seed_type(attrs[count],
                                    WEED_LEAF_VALUE), FALSE), obs, valstr);
      if (valstr) lives_free(valstr);
      lives_free(pname);
      if (type) {
        /* char *ptyp = ptype_to_string(type); */
        /* if (ptyp) { */
        /*   out = lives_strdup_concat(out, NULL, "Plant type identifed as %s\n", prtyp); */
        /*   lives_free(ptyp); */
        /* } */
      }
      if (subtype) {
        //char *ptyp = psubtype_to_string(type);
        /* if (ptyp) { */
        /*   out = lives_strdup_concat(out, NULL, "Plant subtype identifed as %s\n", prtyp); */
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


lives_obj_attr_t *lives_object_declare_attribute(lives_object_t *obj, const char *name, uint32_t st) {
  lives_obj_attr_t *attr;
  lives_obj_attr_t **attrs;
  uint64_t uid;
  int count = 0;
  if (obj) {
    attrs = obj->attributes;
    uid = capable->uid;
  } else {
    attrs = THREADVAR(attributes);
    uid = THREADVAR(uid);
  }
  if (attrs) {
    for (count = 0; attrs[count]; count++) {
      char *pname = weed_get_string_value(attrs[count], WEED_LEAF_NAME, NULL);
      if (!lives_strcmp(name, pname)) {
        lives_free(pname);
        return NULL;
      }
      lives_free(pname);
    }
  }

  attrs = lives_realloc(attrs, (count + 2) * sizeof(lives_obj_attr_t *));

  attr = lives_plant_new_with_refcount(LIVES_WEED_SUBTYPE_OBJ_ATTR);
  weed_set_string_value(attr, WEED_LEAF_NAME, name);
  weed_set_int64_value(attr, LIVES_LEAF_OWNER, uid);

  if (obj) weed_add_plant_flags(attr, WEED_FLAG_IMMUTABLE, LIVES_LEAF_REFCOUNTER);

  // set types for default and for value
  weed_leaf_set(attr, WEED_LEAF_VALUE, st, 0, NULL);
  weed_leaf_set(attr, WEED_LEAF_DEFAULT, st, 0, NULL);

  if (obj) weed_add_plant_flags(attr, WEED_FLAG_UNDELETABLE, NULL);

  attrs[count] = attr;
  attrs[count + 1] = NULL;

  if (obj) obj->attributes = attrs;
  else THREADVAR(attributes) = attrs;

  return attr;
}


LIVES_GLOBAL_INLINE uint32_t lives_attr_get_value_type(lives_obj_attr_t *attr) {
  if (!attr) return WEED_SEED_INVALID;
  if (weed_plant_has_leaf(attr, WEED_LEAF_DEFAULT))
    return weed_leaf_seed_type(attr, WEED_LEAF_DEFAULT);
  return weed_leaf_seed_type(attr, WEED_LEAF_VALUE);
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


/* LIVES_GLOBAL_INLINE boolean contract_attribute_is_mine(lives_object_t *obj, const char *name) { */
/*   if (obj) return contract_attr_is_mine(lives_object_get_attribute(obj, name)); */
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


/* LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_leaf_readonly(lives_object_t *obj, const char *name, */
/*     const char *key, boolean state) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_object_get_attribute(obj, name); */
/*     return lives_attr_set_leaf_readonly(attr, key, state); */
/*   } */
/*   return OBJ_ERROR_NULL_OBJECT; */
/* } */


/* LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_readonly(lives_object_t *obj, const char *name, */
/*     boolean state) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_object_get_attribute(obj, name); */
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

/* LIVES_GLOBAL_INLINE boolean lives_attribute_is_leaf_readonly(lives_object_t *obj, const char *name, const char *key) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_object_get_attribute(obj, name); */
/*     if (attr) return lives_attr_is_leaf_readonly(attr, key); */
/*   } */
/*   return FALSE; */
/* } */


/* LIVES_GLOBAL_INLINE boolean lives_attribute_is_readonly(lives_object_t *obj, const char *name) { */
/*   if (obj) { */
/*     lives_obj_attr_t *attr = lives_object_get_attribute(obj, name); */
/*     if (attr) return lives_attr_is_readonly(attr); */
/*   } */
/*   return FALSE; */
/* } */

///////////////////////////////

LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_param_type(lives_object_t *obj, const char *name,
    const char *label, int ptype) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (attr) {
      if (label) weed_set_string_value(attr, WEED_LEAF_LABEL, label);
      weed_set_int_value(attr, WEED_LEAF_PARAM_TYPE, ptype);
      return WEED_SUCCESS;
    }
    return OBJ_ERROR_NOSUCH_ATTRIBUTE;
  }
  return OBJ_ERROR_NULL_OBJECT;
}


LIVES_GLOBAL_INLINE int lives_attribute_get_param_type(lives_object_t *obj, const char *name) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (attr) return weed_get_int_value(attr, WEED_LEAF_PARAM_TYPE, NULL);
  }
  return LIVES_PARAM_TYPE_UNDEFINED;
}



// TODO - add pre hooks + new_data

LIVES_GLOBAL_INLINE void lives_attribute_append_listener(lives_object_t *obj, const char *name, attr_listener_f func) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    lives_hook_append(obj->hook_closures, POST_VALUE_CHANGED_HOOK, 0, (hook_funcptr_t)func, attr);
  }
}


LIVES_GLOBAL_INLINE void lives_attribute_prepend_listener(lives_object_t *obj, const char *name, attr_listener_f func) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    lives_hook_prepend(obj->hook_closures, POST_VALUE_CHANGED_HOOK, 0, (hook_funcptr_t)func, attr);
  }
}


////// capacities ///////

LIVES_GLOBAL_INLINE lives_capacities_t *lives_capacities_new(void) {
  return lives_plant_new_with_refcount(LIVES_WEED_SUBTYPE_CAPACITIES);
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
    if (weed_refcount_dec(caps) == -1) {
      weed_refcounter_unlock(caps);
      lives_capacities_free(caps);
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
    add_special_field((lsd_struct_def_t *)lsd,
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
    ret = weed_plant_has_leaf(caps, key);
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
        lives_free(capnms[i]);
      }
    }
  }
  lives_free(capnms);
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
        lives_free(capnms[i]);
      }
    }
  }
  lives_free(capnms);
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
      lives_free(capnms[i]);
    }
  }
  if (capnms) lives_free(capnms);
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
        }
      }
    }
  }
  return list;
}


static lives_capacities_t *_add_cap(lives_capacities_t *caps, const char *cname) {
  if (cname) {
    if (!caps) caps = lives_capacities_new();
    if (caps) lives_capacity_set(caps, cname);
  }
  return caps;
}


lives_obj_attr_t *mk_attr(const char *ctype, const char *name, size_t size, void *vptr, int ne) {
  lives_obj_attr_t *attr = lives_object_declare_attribute(NULL, name, WEED_SEED_VOIDPTR);
  weed_set_string_value(attr, ELEM_INTROSPECTION_NATIVE_TYPE, ctype);
  weed_set_int64_value(attr, ELEM_INTROSPECTION_NATIVE_SIZE, size);
  lives_object_set_attr_value(NULL, attr, vptr);
  return attr;
}

#define MK_ATTR(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(tdata->vars.var_##name), \
				     (void *)&(tdata->vars.var_##name), 1)

#define MK_ATTR_P(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(ctype), \
				       (void *)tdata->vars.var_##name), 1)


void make_thrdattrs(lives_thread_data_t *tdata) {
  MK_ATTR(uint64_t, uid);
  MK_ATTR(int, idx);
  MK_ATTR(lives_proc_thread_t, tinfo);
  /* attr = mk_attr("lives_proc_thread_t",  "tinfo", sizeof(THREADVAR(tinfo)), (void *)&(THREADVAR(tinfo)), 1); */
  /* lives_thread_data_t *var_mydata; */
  /* char *var_read_failed_file, *var_write_failed_file, *var_bad_aud_file; */
  /* uint64_t var_random_seed; */
  /* ticks_t var_event_ticks; */

  MK_ATTR(lives_intentcap_t, intentcap);

  /* int var_write_failed, var_read_failed; */
  /* boolean var_com_failed; */
  /* boolean var_chdir_failed; */
  /* int var_rowstride_alignment;   // used to align the rowstride bytesize in create_empty_pixel_data */
  /* int var_rowstride_alignment_hint; */
  /* int var_last_sws_block; */
  /* int var_proc_file; */
  /* int var_cancelled; */
  /* int var_core_id; */
  /* boolean var_fx_is_auto; */
  /* boolean var_fx_is_audio; */
  /* boolean var_no_gui; */
  /* boolean var_force_button_image; */
  /* volatile boolean var_fg_service; */
  /* uint64_t var_hook_flag_hints; */
  /* ticks_t var_timerinfo; */
  /* uint64_t var_thrdnative_flags; */
  /* void *stack; */
  /* size_t stacksize; */
  /* uint64_t var_hook_hints; */
  /* uint64_t var_sync_timeout; */
  /* uint64_t var_blocked_limit; */

  /* int var_hook_match_nparams; */
  /* pthread_mutex_t var_hook_mutex[N_HOOK_FUNCS]; */
  /* LiVESList *var_hook_closures[N_HOOK_FUNCS]; */
  /* // hardware - values */
  /* double var_loveliness; // a bit like 'niceness', only better */
  /* volatile float *var_core_load_ptr; // pointer to value that monitors core load */
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
        lives_free(acaps[i]);
      }
      lives_free(acaps);
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
            }
          }
        }
        lives_free(acaps[i]);
      }
      lives_free(acaps);
    }
  }
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
        weed_leaf_copy(param, WEED_LEAF_VALUE, param, WEED_LEAF_DEFAULT);
      }
    }
    weed_remove_refcounter(param);
    return param;
  }
  return NULL;
}

weed_param_t *weed_param_from_attribute(lives_object_instance_t *obj, const char *name) {
  // find param by NAME, if it lacks a VALUE, set it from default
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
  return weed_param_from_attr(attr);
}

/* static lives_tx_param_t *iparam_from_name(lives_tx_param_t **params, const char *name) { */
/*   //  find the param by name */
/*   for (int i = 0; params[i]; i++) { */
/*     char *pname = weed_get_string_value(params[i], WEED_LEAF_NAME, NULL); */
/*     if (!lives_strcmp(name, pname)) { */
/*       lives_free(pname); */
/*       return params[i]; */
/*     } */
/*   } */
/*   return NULL; */
/* } */


/* static lives_tx_param_t *iparam_match_name(lives_tx_param_t **params, lives_tx_param_t *param) { */
/*   // weed need to find the param by (voidptr) name */
/*   // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow */
/*   // other functions to use the weed_parameter_get_*_value() functions etc. */
/*   char *name = weed_get_string_value(param, WEED_LEAF_NAME, NULL); */
/*   for (int i = 0; params[i]; i++) { */
/*     char *pname = weed_get_string_value(params[i], WEED_LEAF_NAME, NULL); */
/*     if (!lives_strcmp(name, pname)) { */
/*       lives_free(name); lives_free(pname); */
/*       return params[i]; */
/*     } */
/*     lives_free(name); lives_free(pname); */
/*   } */
/*   return NULL; */
/* } */


/* weed_plant_t *int_req_init(const char *name, int def, int min, int max) { */
/*   lives_tx_param_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM); */
/*   int ptype = WEED_PARAM_INTEGER; */
/*   weed_set_string_value(paramt, WEED_LEAF_NAME, name); */
/*   weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype); */
/*   weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def); */
/*   weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min); */
/*   weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max); */
/*   return paramt; */
/* } */

/* weed_plant_t *boolean_req_init(const char *name, int def) { */
/*   lives_tx_param_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM); */
/*   int ptype = WEED_PARAM_SWITCH; */
/*   weed_set_string_value(paramt, WEED_LEAF_NAME, name); */
/*   weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype); */
/*   weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_BOOLEAN, 1, &def); */
/*   return paramt; */
/* } */

/* weed_plant_t *double_req_init(const char *name, double def, double min, double max) { */
/*   weed_plant_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM); */
/*   int ptype = WEED_PARAM_FLOAT; */
/*   weed_set_string_value(paramt, WEED_LEAF_NAME, name); */
/*   weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype); */
/*   weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def); */
/*   weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min); */
/*   weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max); */
/*   return paramt; */
/* } */

/* weed_plant_t *string_req_init(const char *name, const char *def) { */
/*   weed_param_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM); */
/*   int ptype = WEED_PARAM_TEXT; */
/*   weed_set_string_value(paramt, WEED_LEAF_NAME, name); */
/*   weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype); */
/*   weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_STRING, 1, &def); */
/*   return paramt; */
/* } */


/* boolean rules_lack_param(lives_rules_t *prereq, const char *pname) { */
/*   lives_tx_param_t *iparam = iparam_from_name(prereq->reqs->params, pname); */
/*   if (iparam) { */
/*     if (!weed_plant_has_leaf(iparam, WEED_LEAF_VALUE) */
/* 	&& !weed_plant_has_leaf(iparam, WEED_LEAF_DEFAULT)) { */
/*       int flags = weed_get_int_value(iparam, WEED_LEAF_FLAGS, NULL); */
/*       if (flags & PARAM_FLAG_OPTIONAL) return TRUE; */
/*       return FALSE; */
/*     } */
/*   } */
/*   iparam = iparam_from_name(prereq->oinst->attributes, pname); */
/*   if (iparam) return TRUE; */
/*   return TRUE; */
//}


static boolean lives_transform_status_unref(lives_transform_status_t *st) {
  // return FALSE if destroyed
  if (refcount_dec(&st->refcounter) < 0) {
    refcount_unlock(&st->refcounter);
    lives_free(st);
    return FALSE;
  }
  return TRUE;
}


boolean lives_transform_status_free(lives_transform_status_t *st) {
  return lives_transform_status_unref(st);
}


/* boolean requirements_met(lives_object_transform_t *tx) { */
/*   lives_tx_param_t *req; */
/*   for (int i = 0; ((req = tx->prereqs->reqs->params[i])); i++) { */
/*     if (!weed_plant_has_leaf(req, WEED_LEAF_VALUE) && */
/* 	!weed_plant_has_leaf(req, WEED_LEAF_DEFAULT)) { */
/*       int flags = weed_get_int_value(req, WEED_LEAF_FLAGS, NULL); */
/*       if (!(flags & PARAM_FLAG_OPTIONAL)) return FALSE; */
/*     } */
/*     continue; */
/*   } */
/*   req = iparam_match_name(tx->prereqs->oinst->attributes, req); */
/*   if (!req) return FALSE; */
/*   return TRUE; */
/* } */



lives_object_transform_t *find_transform_for_intentcaps(lives_object_t *obj, lives_intentcap_t *icaps) {
  uint64_t type = obj->type;
  if (type == OBJECT_TYPE_MATH) {
    return math_transform_for_intent(obj, icaps->intent);
  }
  return NULL;
}


lives_transform_status_t *transform(lives_object_transform_t *tx) {
  /* lives_tx_param_t *iparam; */
  /* lives_rules_t *prereq = tx->prereqs; */
  /* for (int i = 0; (iparam = prereq->reqs->params[i]) != NULL; i++) { */
  /*   int flags = weed_get_int_value(iparam, WEED_LEAF_FLAGS, NULL); */
  /*   if (!(flags & PARAM_FLAG_VALUE_SET) && !(flags & PARAM_FLAG_OPTIONAL)) { */
  /*     lives_tx_param_t *xparam = iparam_from_name(prereq->reqs->params, iparam->pname); */
  /*     //lives_tx_param_t *xparam = iparam_from_name(tx->prereqs->oinst->params, iparam->name); */
  /*     weed_leaf_dup(iparam, xparam, WEED_LEAF_VALUE); */
  /*   } */

  /*   switch ( */

  /* pth = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, funcinf->function, */
  /* 				       WEED_SEED_DOUBLE, args_fmt, xargs); */
  return NULL;
}


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


