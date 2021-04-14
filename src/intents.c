// intents.c
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"

weed_param_t *weed_param_from_iparams(lives_intentparams_t *iparams, const char *name) {
  // find param by NAME, if it lacks a VALUE, set it from default
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  for (int i = 0; i < iparams->n_params; i++) {
    char *pname = weed_get_string_value(iparams->params[i], WEED_LEAF_NAME, NULL);
    if (!lives_strcmp(name, pname)) {
      free(pname);
      weed_set_int_value(iparams->params[i], WEED_LEAF_TYPE, WEED_PLANT_PARAMETER);
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


static weed_param_t *iparam_from_name(weed_param_t **params, int nparams, const char *name) {
  // weed need to find the param by (voidptr) name
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  for (int i = 0; i < nparams; i++) {
    char *pname = weed_get_string_value(params[i], WEED_LEAF_NAME, NULL);
    if (!lives_strcmp(name, pname)) {
      free(pname);
      return params[i];
    }
  }
  return NULL;
}


static weed_param_t *iparam_match_name(weed_param_t **params, int nparams, weed_param_t *param) {
  // weed need to find the param by (voidptr) name
  // and also set the plant type to WEED_PLANT_PARAMETER - this is to allow
  // other functions to use the weed_parameter_get_*_value() functions etc.
  char *name = weed_get_string_value(param, WEED_LEAF_NAME, NULL);
  for (int i = 0; i < nparams; i++) {
    char *pname = weed_get_string_value(params[i], WEED_LEAF_NAME, NULL);
    if (!lives_strcmp(name, pname)) {
      free(name); free(pname);
      return params[i];
    }
    free(name); free(pname);
  }
  return NULL;
}


weed_plant_t *int_req_init(const char *name, int def, int min, int max) {
  weed_plant_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
  int ptype = WEED_PARAM_INTEGER;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  return paramt;
}

weed_plant_t *boolean_req_init(const char *name, int def) {
  weed_plant_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
  int ptype = WEED_PARAM_SWITCH;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_BOOLEAN, 1, &def);
  return paramt;
}

weed_plant_t *double_req_init(const char *name, double def, double min, double max) {
  weed_plant_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
  int ptype = WEED_PARAM_FLOAT;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  return paramt;
}

weed_plant_t *string_req_init(const char *name, const char *def) {
  weed_plant_t *paramt = lives_plant_new(LIVES_WEED_SUBTYPE_OBJ_PARAM);
  int ptype = WEED_PARAM_TEXT;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_STRING, 1, &def);
  return paramt;
}


LIVES_GLOBAL_INLINE
const lives_object_template_t *lives_object_template_for_type(uint64_t type) {
  // TODO - get object template for type
  // find a transform which creates child with subtype subtype

  //if (type == IMkType("obj.MATH")) return maths_object_with_subtype(subtype);
  return NULL;
}


boolean rules_lack_param(lives_rules_t *prereq, const char *pname) {
  weed_param_t *iparam = iparam_from_name(prereq->reqs->params, prereq->reqs->n_params, pname);
  if (iparam) {
    if (!weed_plant_has_leaf(iparam, WEED_LEAF_VALUE)
        && !weed_plant_has_leaf(iparam, WEED_LEAF_DEFAULT)) {
      int flags = weed_get_int_value(iparam, WEED_LEAF_FLAGS, NULL);
      if (flags & PARAM_FLAGS_OPTIONAL) return TRUE;
      return FALSE;
    }
  }
  iparam = iparam_from_name(prereq->oinst->params, prereq->oinst->n_params, pname);
  if (iparam) return TRUE;
  return TRUE;
}


static void lives_object_status_unref(lives_object_status_t *st) {
  if (--st->refcount < 0) lives_free(st);
}


void lives_object_status_free(lives_object_status_t *st) {
  lives_object_status_unref(st);
}


boolean requirements_met(lives_object_transform_t *tx) {
  lives_obj_param_t *req;
  for (int i = 0; i < tx->prereqs->reqs->n_params; i++) {
    req = tx->prereqs->reqs->params[i];
    if (req) {
      if (!weed_plant_has_leaf(req, WEED_LEAF_VALUE) &&
          !weed_plant_has_leaf(req, WEED_LEAF_DEFAULT)) {
        int flags = weed_get_int_value(req, WEED_LEAF_FLAGS, NULL);
        if (!(flags & PARAM_FLAGS_OPTIONAL)) return FALSE;
      }
      continue;
    }
    req = iparam_match_name(tx->prereqs->oinst->params, tx->prereqs->oinst->n_params, req);
    if (!req) return FALSE;
  }
  for (int i = 0; i < tx->prereqs->n_conditions; i++) {
    if (!*tx->prereqs->conditions[i]) return FALSE;
  }
  return TRUE;
}


static void lives_rules_unref(lives_rules_t *rules) {
  if (--rules->refcount < 0) {
    if (rules->reqs) {
      for (int i = 0; i < rules->reqs->n_params; i++) {
        weed_plant_free(rules->reqs->params[i]);
      }
      lives_free(rules->reqs);
    }
    if (rules->conditions) lives_free(rules->conditions);
    lives_free(rules);
  }
}


void lives_object_transform_free(lives_object_transform_t *tx) {
  lives_rules_unref(tx->prereqs);
  //if (tx->mappings) tx_mappings_free(tx->mappings);
  //if (tx->oparams) tx_oparams_free(tx->oparams);
  lives_free(tx);
}


lives_object_transform_t *find_transform_for_intent(lives_object_t *obj, lives_intention intent) {
  uint64_t type = obj->type;
  if (type == IMkType("MATH.obj")) {
    return math_transform_for_intent(obj, intent);
  }
  return NULL;
}


lives_object_status_t *transform(lives_object_t *obj, lives_object_transform_t *tx,
                                 lives_object_t **other) {
  /* for (int i = 0; i < tx->prereqs->n_conditions; i++) { */
  /*   if (!*tx->prereqs->conditions[i]) return FALSE; */
  /* } */
  /* for (int i = 0; i < tx->prereqs->n_reqs; i++) { */
  /*   param = &tx->prereqs->reqs[i]; */
  /*   flags = weed_get_int_value(param, WEED_LEAF_FLAGS, NULL); */
  /*   if (!(flags & PARAM_FLAGS_VALUE_SET) && !(flags & PARAM_FLAGS_OPTIONAL)) { */
  /*     xparam = weed_param_from_name(prereq->oinst->params, prereq->oinst->n_params, param->name); */
  /*     weed_leaf_dup(param, xparam, WEED_LEAF_VALUE); */
  /*   } */

  /*   switch ( */




  /* pth = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, funcinf->function, */
  /* 				       WEED_SEED_DOUBLE, args_fmt, xargs); */


  return NULL;
}


void lives_intentparams_free(lives_intentparams_t *iparams) {
  for (int i = 0; i < iparams->n_params; i++) {
    weed_plant_free(iparams->params[i]);
  }
  lives_free(iparams->params);
  lives_free(iparams);
}


#if 0
LiVESTransformList *list_transformations(lives_object_t *obj, int state) {
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

  if (obj->type == Imktype("MATH    ")) {
    if (state == STATE_NONE) {
    }
  }

  return NULL;
}
#endif


