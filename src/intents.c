// intents.c
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"

// eventually will become lookup for all recognised uids
lives_dict_t *main_dict = NULL;

static const lives_funcdef_t **funcdefs = NULL;

static size_t dict_size = 0;

static boolean icaps_inited = FALSE;

static const lives_intentcap_t *std_icaps[N_STD_ICAPS];

#define STD_ICAP_ID "Standard ICAP"
#define CUSTOM_ICAP_ID "Custom ICAP"

#define DICT_SUBTYPE_OBJECT		IMkType("DICT.obj")
#define DICT_SUBTYPE_WEED_PLANT		IMkType("DICT.pla")
#define DICT_SUBTYPE_FUNCDEF		IMkType("DICT.fun")

#define ATTR_INTROSPECTION_NATIVE_PTR "native_ptr"
#define ATTR_INTROSPECTION_NATIVE_TYPE "native_type"
#define ATTR_INTROSPECTION_NATIVE_SIZE "native_size"

// eventually this will part of a fully fledged object broker

typedef lives_object_t lives_dictentry_t;

lives_dictent_t  *_make_dictent(lives_dictent_t *dicte, lives_intention intent,
                                lives_object_t *obj, va_list xargs) {
  // dict entry, object with any type of attributes but no transforms
  // copy specified attributes from obj into the dictionary
  // if intent is LIVES_INTENTION_UPDATE then we only overwrite, otherwise we replace all

  char *aname;
  int count = 0;
  uint32_t st;

  // intents - update - set with new vals, no delete
  // replace - delete old add new

  if (!dicte) dicte = lives_object_instance_create(OBJECT_TYPE_DICTIONARY, DICT_SUBTYPE_OBJECT);
  else if (intent == LIVES_INTENTION_REPLACE && dicte->attributes) {
    lives_object_attributes_unref_all(dicte);
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

    st = obj_attr_get_value_type(attr);
    if (intent != LIVES_INTENTION_REPLACE) {
      xattr  = lives_object_get_attribute(dicte, aname);
      if (xattr) lives_object_attribute_unref(dicte, xattr);
    }
    xattr = lives_object_declare_attribute(dicte, aname, st);
    if (xattr) {
      // TODO = warn if attr type changing
      weed_plant_duplicate_clean(xattr, attr);
      obj_attr_set_readonly(xattr, obj_attr_is_readonly(attr));
      if (!obj_attr_is_mine(xattr))
        weed_leaf_clear_flagbits(xattr, WEED_LEAF_VALUE,
                                 WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE);
    }
    if (!xargs) lives_free(aname);
  }
  return dicte;
}


lives_dictentry_t  *replace_dictent(lives_dictent_t *dicte, lives_object_t *obj, ...) {
  // replace attributes if already there
  va_list xargs;
  va_start(xargs, obj);
  dicte = _make_dictent(dicte, LIVES_INTENTION_REPLACE, obj, xargs);
  va_end(xargs);
  return dicte;
}


lives_dictentry_t  *update_dictent(lives_dictent_t *dicte, lives_object_t *obj, ...) {
  // update a subset of attributes if already there
  va_list xargs;
  va_start(xargs, obj);
  dicte = _make_dictent(dicte, LIVES_INTENTION_UPDATE, obj, xargs);
  va_end(xargs);
  return dicte;
}


static lives_dict_t *_update_dictionary(lives_dict_t *dict, lives_intention intent, uint64_t uid,
                                        lives_dictentry_t *dicte) {
  // if intent == REPLACE replace existing entry, otherwise do not add if already there
  if (!dict) dict = lives_hash_store_new("main store");
  else if (intent != LIVES_INTENTION_REPLACE && get_from_hash_store_i(dict, uid)) return dict;
  return add_to_hash_store_i(dict, uid, (void *)dicte);
}


static lives_dict_t *add_to_dictionary(lives_dict_t *dict, uint64_t uid, lives_dictentry_t *dicte) {
  // add only if not there
  return _update_dictionary(dict, LIVES_INTENTION_NOTHING, uid, dicte);
}


static lives_dict_t *update_dictionary(lives_dict_t *dict, uint64_t uid, lives_dictentry_t *dicte) {
  // add if not there, else update
  return _update_dictionary(dict, LIVES_INTENTION_REPLACE, uid, dicte);
}


uint64_t add_object_to_dict(lives_object_t *obj) {
  lives_dictent_t *dicte = _make_dictent(NULL, 0, obj, NULL);
  main_dict = add_to_dictionary(main_dict, obj->uid, dicte);
  dict_size += weigh_object(obj);
  return dict_size;
}


uint64_t update_object_in_dict(lives_object_t *obj) {
  // add or update
  lives_dictent_t *dicte = _make_dictent(NULL, 0, obj, NULL);
  main_dict = update_dictionary(main_dict, obj->uid, dicte);
  //dict_size += weigh_object(obj);
  return dict_size;
}


uint64_t add_weed_plant_to_dict(weed_plant_t *plant) {
  // only add if not there
  lives_dictent_t *dicte;
  weed_error_t err;
  uint64_t uid = weed_get_int64_value(plant, LIVES_LEAF_UID, &err);
  if (err == WEED_ERROR_NOSUCH_LEAF) {
    uid = gen_unique_id();
    weed_set_int64_value(plant, LIVES_LEAF_UID, uid);
  } else if (err != WEED_SUCCESS) return 0;
  dicte  = weed_plant_to_dictent(plant);
  dict_size += weed_plant_weigh(plant);
  main_dict = update_dictionary(main_dict, uid, dicte);
  return dict_size;
}


lives_dict_t *remove_from_dict(uint64_t key) {
  main_dict = remove_from_hash_store_i(main_dict, key);
  return main_dict;
}


static lives_dictent_t *lookup_entry(uint64_t uid) {
  return get_from_hash_store_i(main_dict, uid);
}


lives_dictent_t *weed_plant_to_dictent(weed_plant_t *plant) {
  lives_dictent_t *dicte = NULL;
  weed_size_t nleaves;
  char **leaves = weed_plant_list_leaves(plant, &nleaves);
  if (nleaves) {
    dicte = lives_object_instance_create(OBJECT_TYPE_DICTIONARY, DICT_SUBTYPE_WEED_PLANT);
    for (int i = 0; leaves[i]; i++) {
      char *key = leaves[i];
      uint32_t st = weed_leaf_seed_type(plant, key);
      lives_obj_attr_t *attr = lives_object_declare_attribute(dicte, key, st);
      weed_leaf_copy(attr, WEED_LEAF_VALUE, plant, key);
      lives_free(leaves[i]);
    }
    lives_free(leaves);
  }
  return dicte;
}


void add_fn_lookup(lives_funcptr_t func, const char *name, const char *rrtype, const char *args_fmt, int flags) {
  uint32_t rtype = get_seedtype(rrtype[0]);
  int funcidx = get_fn_idx(name);
  if (!funcdefs) funcdefs = lives_calloc(N_MAX_FUNCS, sizeof(lives_funcdef_t *));
  else if (funcdefs[funcidx]) return;
  if (1) {
    lives_dictentry_t *dicte = lives_object_instance_create(OBJECT_TYPE_DICTIONARY, DICT_SUBTYPE_FUNCDEF);
    lives_obj_attr_t *xattr = lives_object_declare_attribute(dicte, ATTR_INTROSPECTION_NATIVE_PTR, WEED_SEED_VOIDPTR);
    const lives_funcdef_t *funcdef = funcdefs[funcidx] = create_funcdef(name, func, rtype, args_fmt, flags);
    lives_object_set_attr_value(dicte, xattr, funcdef);
    add_to_dictionary(main_dict, funcdef->uid, dicte);
  }
}


const lives_funcdef_t *get_template_for_func(funcidx_t funcidx) {
  return funcdefs[funcidx];
}


const lives_funcdef_t *get_template_for_func_by_uid(uint64_t uid) {
  lives_dictent_t *dicte = lookup_entry(uid);
  if (dicte && dicte->subtype == DICT_SUBTYPE_FUNCDEF) {
    lives_obj_attr_t *attr = lives_object_get_attribute(dicte, ATTR_INTROSPECTION_NATIVE_PTR);
    return weed_get_voidptr_value(attr, WEED_LEAF_VALUE, NULL);
  }
  return NULL;
}


LIVES_GLOBAL_INLINE char *get_argstring_for_func(funcidx_t funcidx) {
  const lives_funcdef_t *fdef = get_template_for_func(funcidx);
  if (!fdef) return NULL;
  return funcsig_to_string(funcsig_from_args_fmt(fdef->args_fmt));
}


char *lives_funcdef_explain(const lives_funcdef_t *funcdef) {
  if (funcdef) {
    char *out = lives_strdup_printf("Function with uid 0X%016lX has prototype:\n"
                                    "\t%s %s(%s)\n"
                                    "function category is %d, flags = 0X%016lX",
                                    funcdef->uid, weed_seed_to_ctype(funcdef->return_type, FALSE),
                                    funcdef->funcname ? funcdef->funcname : "??????",
                                    funcsig_to_string(funcsig_from_args_fmt(funcdef->args_fmt)),
                                    funcdef->category, funcdef->flags);
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


void dump_plantdesc(lives_dictent_t *dicte) {
  //int
}


char *interpret_uid(uint64_t uid) {
  char *info = NULL;
  lives_dictent_t *dicte = lookup_entry(uid);
  if (dicte) {
    uint64_t sub = dicte->subtype;
    if (sub == DICT_SUBTYPE_OBJECT || sub == DICT_SUBTYPE_WEED_PLANT) {
      //if (sub == DICT_SUBTYPE_WEED_PLANT) dump_plantdesc(dicte);
      //else dump_obdesc(dicte);
      info = lives_object_dump_attributes(dicte);
    } else if (sub == DICT_SUBTYPE_FUNCDEF) {
      lives_obj_attr_t *attr = lives_object_get_attribute(dicte, ATTR_INTROSPECTION_NATIVE_PTR);
      lives_funcdef_t *funcdef = (lives_funcdef_t *)weed_get_voidptr_value(attr, WEED_LEAF_VALUE, NULL);
      info = lives_funcdef_explain(funcdef);
    }
  }
  return info;
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
  if (attr) {
    weed_error_t err; uint32_t st = obj_attr_get_value_type(attr);
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
      default: return LIVES_ERROR_ATTRIBUTE_INVALID;
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
      default: return LIVES_ERROR_ATTRIBUTE_INVALID;
      }
    } return err;
  }
  return LIVES_ERROR_NOSUCH_ATTRIBUTE;
}

weed_error_t lives_object_set_attribute_value(lives_object_t *obj, const char *name, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (name && *name) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (!attr) return LIVES_ERROR_NOSUCH_ATTRIBUTE;
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
    if (!attr) return LIVES_ERROR_NOSUCH_ATTRIBUTE;
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
    if (!attr) return LIVES_ERROR_NOSUCH_ATTRIBUTE;
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
    if (!attr) return LIVES_ERROR_NOSUCH_ATTRIBUTE;
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
    if (obj) attrs = obj->attributes;
    else attrs = THREADVAR(attributes);
    if (attrs) {
      for (int count = 0; attrs[count]; count++) {
        char *pname = weed_get_string_value(attrs[count], WEED_LEAF_NAME, NULL);
        if (!lives_strcmp(name, pname)) {
          lives_free(pname);
          return attrs[count];
        }
        lives_free(pname);
      }
    }
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
                                    WEED_LEAF_VALUE), FALSE), obs,
                                valstr);
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
    //lives_dictent_t *dicte = weed_plant_to_dictent(plant);
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


LIVES_GLOBAL_INLINE uint32_t obj_attr_get_value_type(lives_obj_attr_t *attr) {
  if (!attr) return WEED_SEED_INVALID;
  if (weed_plant_has_leaf(attr, WEED_LEAF_DEFAULT))
    return weed_leaf_seed_type(attr, WEED_LEAF_DEFAULT);
  return weed_leaf_seed_type(attr, WEED_LEAF_VALUE);
}


LIVES_GLOBAL_INLINE uint64_t obj_attr_get_owner(lives_obj_attr_t *attr) {
  if (attr) return weed_get_int64_value(attr, LIVES_LEAF_OWNER, NULL);
  return 0;
}


LIVES_GLOBAL_INLINE uint64_t lives_attribute_get_owner(lives_object_t *obj, const char *name) {
  if (obj) return obj_attr_get_owner(lives_object_get_attribute(obj, name));
  return 0;
}


LIVES_GLOBAL_INLINE boolean obj_attr_is_mine(lives_obj_attr_t *attr) {
  if (attr && obj_attr_get_owner(attr) == capable->uid) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_attribute_is_mine(lives_object_t *obj, const char *name) {
  if (obj) return obj_attr_is_mine(lives_object_get_attribute(obj, name));
  return FALSE;
}


LIVES_GLOBAL_INLINE weed_error_t obj_attr_set_leaf_readonly(lives_obj_attr_t *attr, const char *key, boolean state) {
  if (!attr) return LIVES_ERROR_NOSUCH_ATTRIBUTE;
  if (!obj_attr_is_mine(attr)) return LIVES_ERROR_NOT_OWNER;
  if (!strcmp(key, WEED_LEAF_VALUE)) return obj_attr_set_readonly(attr, state);
  if (state)
    return weed_leaf_set_flagbits(attr, key, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE);
  else
    return weed_leaf_clear_flagbits(attr, key, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE);
}


LIVES_GLOBAL_INLINE weed_error_t obj_attr_set_readonly(lives_obj_attr_t *attr, boolean state) {
  if (!attr) return LIVES_ERROR_NOSUCH_ATTRIBUTE;
  if (!obj_attr_is_mine(attr)) return LIVES_ERROR_NOT_OWNER;
  if (!weed_leaf_num_elements(attr, WEED_LEAF_VALUE)) return LIVES_ERROR_ATTRIBUTE_INVALID;
  if (state) {
    weed_leaf_set_flagbits(attr, WEED_LEAF_VALUE, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE);
    return weed_set_int_value(attr, WEED_LEAF_FLAGS, weed_get_int_value(attr, WEED_LEAF_FLAGS, NULL)
                              | PARAM_FLAG_READONLY);
  } else {
    weed_leaf_clear_flagbits(attr, WEED_LEAF_VALUE, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE);
    return weed_set_int_value(attr, WEED_LEAF_FLAGS,
                              (weed_get_int_value(attr, WEED_LEAF_FLAGS, NULL)
                               & ~PARAM_FLAG_READONLY));
  }
}


LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_leaf_readonly(lives_object_t *obj, const char *name,
    const char *key, boolean state) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    return obj_attr_set_leaf_readonly(attr, key, state);
  }
  return LIVES_ERROR_NULL_OBJECT;
}


LIVES_GLOBAL_INLINE weed_error_t lives_attribute_set_readonly(lives_object_t *obj, const char *name,
    boolean state) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    return obj_attr_set_readonly(attr, state);
  }
  return LIVES_ERROR_NULL_OBJECT;
}


LIVES_GLOBAL_INLINE boolean obj_attr_is_leaf_readonly(lives_obj_attr_t *attr, const char *key) {
  if (attr) {
    if (!lives_strcmp(key, WEED_LEAF_VALUE)) return obj_attr_is_readonly(attr);
    if (weed_leaf_get_flags(attr, key) & WEED_FLAG_IMMUTABLE) return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean obj_attr_is_readonly(lives_obj_attr_t *attr) {
  if (attr) {
    if (weed_leaf_get_flags(attr, WEED_LEAF_VALUE) & WEED_FLAG_IMMUTABLE
        || (weed_get_int_value(attr, WEED_LEAF_FLAGS, NULL)
            & PARAM_FLAG_READONLY) == PARAM_FLAG_READONLY) {
      if (obj_attr_is_mine(attr)) obj_attr_set_readonly(attr, TRUE);
      return TRUE;
    }
    if (obj_attr_is_mine(attr)) obj_attr_set_readonly(attr, FALSE);
  }
  return FALSE;
}

LIVES_GLOBAL_INLINE boolean lives_attribute_is_leaf_readonly(lives_object_t *obj, const char *name, const char *key) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (attr) return obj_attr_is_leaf_readonly(attr, key);
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean lives_attribute_is_readonly(lives_object_t *obj, const char *name) {
  if (obj) {
    lives_obj_attr_t *attr = lives_object_get_attribute(obj, name);
    if (attr) return obj_attr_is_readonly(attr);
  }
  return FALSE;
}

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
    return LIVES_ERROR_NOSUCH_ATTRIBUTE;
  }
  return LIVES_ERROR_NULL_OBJECT;
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


void icap_caps_copy(lives_intentcap_t *dst, lives_intentcap_t *src) {
  if (dst && src) {
    if (dst->capacities) lives_capacities_unref(dst->capacities);
    dst->capacities = lives_capacities_copy(NULL, src->capacities);
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
  static const lives_struct_def_t *lsd = NULL;
  lives_intentcap_t *icap = NULL;
  if (!lsd) {
    lsd = get_lsd(LIVES_STRUCT_INTENTCAP_T);
    if (1) {
      lives_intentcap_t *icap = (lives_intentcap_t *)lives_calloc(1, sizeof(lives_intentcap_t));
      lsd_special_field_t **specf = lsd->special_fields;
      specf[0] = make_special_field(LSD_FIELD_FLAG_ZERO_ON_COPY, icap, &icap->capacities,
                                    "capacities", 0, NULL, (lsd_field_copy_cb)icap_caps_copy_cb,
                                    (lsd_field_delete_cb)icap_caps_delete_cb);
    }
  }
  if (lsd) {
    if (stat) {
      lives_struct_create_static(lsd, stat);
      icap = stat;
    } else icap = lives_struct_create(lsd);
    if (icap) {
      icap->intent = intent;
      if (intent != LIVES_INTENTION_NOTHING) {
        icap->capacities = lives_capacities_new();
      }
    }
  }
  return icap;
}


void lives_icap_init(lives_intentcap_t *stat) {
  lives_icap_new(LIVES_INTENTION_NOTHING, stat);
}


void lives_icap_free(lives_intentcap_t *icap) {
  if (icap) {
    // make sure we are not freeing a std index entry
    if (lives_struct_get_generation(icap->lsd) == 1 &&
        !lives_strcmp(lives_struct_get_class_id(icap->lsd), STD_ICAP_ID)) return;
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
  if (icap) lives_struct_set_class_id(icap->lsd, CUSTOM_ICAP_ID);
  return icap;
}


LIVES_LOCAL_INLINE const lives_intentcap_t *make_std_icap(const char *desc,
    lives_intention intent, ...) {
  lives_intentcap_t *icap;
  va_list xargs;
  va_start(xargs, intent);
  icap = _make_icap(intent, xargs);
  if (icap) {
    if (desc) lives_snprintf(icap->desc, ICAP_DESC_LEN, "%s", desc);
    lives_struct_set_class_id(icap->lsd, STD_ICAP_ID);
  }
  va_end(xargs);
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
  weed_set_string_value(attr, ATTR_INTROSPECTION_NATIVE_TYPE, ctype);
  weed_set_int64_value(attr, ATTR_INTROSPECTION_NATIVE_SIZE, size);
  lives_object_set_attr_value(NULL, attr, vptr);
  return attr;
}

#define MK_ATTR(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(THREADVAR(name)), \
				     (void *)&(THREADVAR(name)), 1)

#define MK_ATTR_P(ctype, name) mk_attr(QUOTEME(ctype), QUOTEME(name), sizeof(ctype), \
				     (void *)THREADVAR(name), 1)


void make_thrdattrs(void) {
  MK_ATTR(uint64_t, uid);
  MK_ATTR(int, id);
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
  std_icaps[_ICAP_IDLE] = make_std_icap("idle", LIVES_INTENTION_NOTHING, NULL);
  std_icaps[_ICAP_DOWNLOAD] = make_std_icap("download", LIVES_INTENTION_IMPORT,
                              LIVES_CAPACITY_REMOTE, NULL);
  std_icaps[_ICAP_LOAD] = make_std_icap("load", LIVES_INTENTION_IMPORT, LIVES_CAPACITY_LOCAL, NULL);
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


weed_plant_t *int_req_init(const char *name, int def, int min, int max) {
  lives_tx_param_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
  int ptype = WEED_PARAM_INTEGER;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  return paramt;
}

weed_plant_t *boolean_req_init(const char *name, int def) {
  lives_tx_param_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
  int ptype = WEED_PARAM_SWITCH;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_BOOLEAN, 1, &def);
  return paramt;
}

weed_plant_t *double_req_init(const char *name, double def, double min, double max) {
  weed_plant_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
  int ptype = WEED_PARAM_FLOAT;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  return paramt;
}

weed_plant_t *string_req_init(const char *name, const char *def) {
  weed_param_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_TX_PARAM);
  int ptype = WEED_PARAM_TEXT;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_STRING, 1, &def);
  return paramt;
}


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


static boolean lives_rules_unref(lives_rules_t *rules) {
  // return FALSE if destroyed
  if (refcount_dec(&rules->refcounter) < 0) {
    if (rules->reqs) {
      for (int i = 0; rules->reqs->params[i]; i++) {
        weed_plant_free(rules->reqs->params[i]);
      }
      lives_free(rules->reqs);
    }
    lives_free(rules);
    return FALSE;
  }
  return TRUE;
}


void lives_object_transform_free(lives_object_transform_t *tx) {
  lives_rules_unref(tx->prereqs);
  //if (tx->mappings) tx_mappings_free(tx->mappings);
  //if (tx->oparams) tx_oparams_free(tx->oparams);
  lives_free(tx);
}


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
      tx->icaps.intent = LIVES_INTENTION_IMPORT;
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


