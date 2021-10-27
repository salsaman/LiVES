// intents.c
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"

LIVES_GLOBAL_INLINE lives_object_instance_t *lives_object_instance_create(uint64_t type, uint64_t subtype) {
  lives_object_instance_t *obj_inst = lives_calloc(1, sizeof(lives_object_instance_t));
  obj_inst->uid = gen_unique_id();
  obj_inst->type = type;
  obj_inst->subtype = subtype;
  obj_inst->state = OBJECT_STATE_UNDEFINED;
  return obj_inst;
}


weed_error_t lives_object_set_attribute_value(lives_object_t *obj, const char *name, ...) {
  weed_error_t err = WEED_SUCCESS;
  if (obj && name && *name) {
    lives_obj_attr_t *attr = lives_attr_from_object(obj, name);
    if (!attr) return WEED_ERROR_NOSUCH_LEAF;
    else {
      va_list args;
      int st;
      st = weed_leaf_seed_type(attr, WEED_LEAF_VALUE);
      va_start(args, name);
      switch (st) {
      case WEED_SEED_INT: {
        int val = va_arg(args, int);
        err = weed_set_int_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_BOOLEAN: {
        boolean val = va_arg(args, int);
        err = weed_set_boolean_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_DOUBLE: {
        boolean val = va_arg(args, double);
        err = weed_set_double_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_INT64: {
        int64_t val = va_arg(args, int64_t);
        err = weed_set_int64_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_STRING: {
        char *val = va_arg(args, char *);
        err = weed_set_string_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_VOIDPTR: {
        void *val = va_arg(args, void *);
        err = weed_set_voidptr_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_FUNCPTR: {
        weed_funcptr_t val = va_arg(args, weed_funcptr_t);
        err = weed_set_funcptr_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      case WEED_SEED_PLANTPTR: {
        weed_plantptr_t val = va_arg(args, weed_plantptr_t);
        err = weed_set_plantptr_value(attr, WEED_LEAF_VALUE, val);
        break;
      }
      // TODO - allow custom types - object and hook (?)
      default:
        va_end(args);
        return WEED_ERROR_WRONG_SEED_TYPE;
      }
      va_end(args);
    }
  }
  return err;
}


boolean lives_object_declare_attribute(lives_object_t *obj, const char *name, int32_t st) {
  lives_obj_attr_t **attrs = obj->attributes;
  lives_obj_attr_t *attr;
  int count = 0;
  if (attrs) {
    for (count = 0; attrs[count]; count++) {
      char *pname = weed_get_string_value(attrs[count], WEED_LEAF_NAME, NULL);
      if (!lives_strcmp(name, pname)) {
        lives_free(pname);
        return FALSE;
      }
    }
  }
  obj->attributes = lives_realloc(obj->attributes, (count + 2) * sizeof(lives_obj_attr_t *));
  attr = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_ATTR);
  weed_set_string_value(attr, WEED_LEAF_NAME, name);
  weed_leaf_set(attr, WEED_LEAF_VALUE, st, 0, NULL);
  obj->attributes[count] = attr;
  obj->attributes[count + 1] = NULL;
  return TRUE;
}


LIVES_GLOBAL_INLINE lives_capacity_t *lives_capacities_new(void) {
  return lives_plant_new(LIVES_WEED_SUBTYPE_CAPACITIES);
}

LIVES_GLOBAL_INLINE void lives_capacities_free(lives_capacity_t *cap) {
  if (cap) weed_plant_free(cap);
}


LIVES_LOCAL_INLINE lives_intentcap_t *lives_icaps_new(lives_intention intent) {
  lives_intentcap_t *icaps = (lives_intentcap_t *)lives_malloc(sizeof(lives_intentcap_t));
  icaps->intent = intent;
  icaps->capacities = lives_capacities_new();
  return icaps;
}


void lives_intentcaps_free(lives_intentcap_t *icaps) {
  if (icaps) {
    if (icaps->capacities) lives_capacities_free(icaps->capacities);
    lives_free(icaps);
  }
}


lives_intentcap_t *lives_intentcaps_new(int icapstype) {
  lives_intentcap_t *icaps = NULL;
  switch (icapstype) {
  case LIVES_ICAPS_LOAD:
  case LIVES_ICAPS_DOWNLOAD:
    icaps = lives_icaps_new(LIVES_INTENTION_IMPORT);
    break;
  default: break;
  }

  icaps->capacities = lives_capacities_new();

  switch (icapstype) {
  case LIVES_ICAPS_LOAD:
    lives_capacity_unset(icaps->capacities, LIVES_CAPACITY_REMOTE);
    break;
  case LIVES_ICAPS_DOWNLOAD:
    lives_capacity_set(icaps->capacities, LIVES_CAPACITY_REMOTE);
    break;
  default: break;
  }
  return icaps;
}


extern int32_t weed_plant_mutate(weed_plantptr_t plant, int32_t newtype);

lives_tx_param_t *weed_param_from_iparams(lives_intentparams_t *iparams, const char *name) {
  // find param by NAME, if it lacks a VALUE, set it from default
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  for (int i = 0; iparams->params[i]; i++) {
    char *pname = weed_get_string_value(iparams->params[i], WEED_LEAF_NAME, NULL);
    if (!lives_strcmp(name, pname)) {
      lives_free(pname);
      weed_plant_mutate(iparams->params[i], WEED_PLANT_PARAMETER);
      if (!weed_plant_has_leaf(iparams->params[i], WEED_LEAF_VALUE)) {
        if (weed_plant_has_leaf(iparams->params[i], WEED_LEAF_DEFAULT)) {
          weed_leaf_copy(iparams->params[i], WEED_LEAF_VALUE, iparams->params[i], WEED_LEAF_DEFAULT);
        }
      }
      return iparams->params[i];
    }
  }
  return NULL;
}


LIVES_GLOBAL_INLINE lives_obj_attr_t *lives_attr_from_object(lives_object_t *obj, const char *name) {
  lives_intentparams_t iparams;
  iparams.params = obj->attributes;
  return weed_param_from_iparams(&iparams, name);
}


static lives_tx_param_t *iparam_from_name(lives_tx_param_t **params, const char *name) {
  //  find the param by name
  for (int i = 0; params[i]; i++) {
    char *pname = weed_get_string_value(params[i], WEED_LEAF_NAME, NULL);
    if (!lives_strcmp(name, pname)) {
      lives_free(pname);
      return params[i];
    }
  }
  return NULL;
}


static lives_tx_param_t *iparam_match_name(lives_tx_param_t **params, lives_tx_param_t *param) {
  // weed need to find the param by (voidptr) name
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  char *name = weed_get_string_value(param, WEED_LEAF_NAME, NULL);
  for (int i = 0; params[i]; i++) {
    char *pname = weed_get_string_value(params[i], WEED_LEAF_NAME, NULL);
    if (!lives_strcmp(name, pname)) {
      lives_free(name); lives_free(pname);
      return params[i];
    }
    lives_free(name); lives_free(pname);
  }
  return NULL;
}


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


boolean rules_lack_param(lives_rules_t *prereq, const char *pname) {
  lives_tx_param_t *iparam = iparam_from_name(prereq->reqs->params, pname);
  if (iparam) {
    if (!weed_plant_has_leaf(iparam, WEED_LEAF_VALUE)
        && !weed_plant_has_leaf(iparam, WEED_LEAF_DEFAULT)) {
      int flags = weed_get_int_value(iparam, WEED_LEAF_FLAGS, NULL);
      if (flags & PARAM_FLAGS_OPTIONAL) return TRUE;
      return FALSE;
    }
  }
  iparam = iparam_from_name(prereq->oinst->attributes, pname);
  if (iparam) return TRUE;
  return TRUE;
}


static void lives_transform_status_unref(lives_transform_status_t *st) {
  if (--st->refcount < 0) lives_free(st);
}


void lives_transform_status_free(lives_transform_status_t *st) {
  lives_transform_status_unref(st);
}


boolean requirements_met(lives_object_transform_t *tx) {
  lives_tx_param_t *req;
  for (int i = 0; ((req = tx->prereqs->reqs->params[i])); i++) {
    if (!weed_plant_has_leaf(req, WEED_LEAF_VALUE) &&
        !weed_plant_has_leaf(req, WEED_LEAF_DEFAULT)) {
      int flags = weed_get_int_value(req, WEED_LEAF_FLAGS, NULL);
      if (!(flags & PARAM_FLAGS_OPTIONAL)) return FALSE;
    }
    continue;
  }
  req = iparam_match_name(tx->prereqs->oinst->attributes, req);
  if (!req) return FALSE;
  return TRUE;
}


static void lives_rules_unref(lives_rules_t *rules) {
  if (--rules->refcount < 0) {
    if (rules->reqs) {
      for (int i = 0; rules->reqs->params[i]; i++) {
        weed_plant_free(rules->reqs->params[i]);
      }
      lives_free(rules->reqs);
    }
    lives_free(rules);
  }
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
  /*   if (!(flags & PARAM_FLAGS_VALUE_SET) && !(flags & PARAM_FLAGS_OPTIONAL)) { */
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


