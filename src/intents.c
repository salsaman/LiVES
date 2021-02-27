// intents.c
// LiVES
// (c) G. Finch 2003-2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"

static weed_plant_t *weed_integer_init(const char *name, int def, int min, int max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_INTEGER;
  weed_plant_t *gui;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_INT, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_INT, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_INT, 1, &max);
  return paramt;
}

static weed_plant_t *weed_switch_init(const char *name, int def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_SWITCH;
  weed_plant_t *gui;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_BOOLEAN, 1, &def);
  return paramt;
}

static weed_plant_t *weed_float_init(const char *name, double def, double min, double max) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_FLOAT;
  weed_plant_t *gui;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_DOUBLE, 1, &def);
  weed_leaf_set(paramt, WEED_LEAF_MIN, WEED_SEED_DOUBLE, 1, &min);
  weed_leaf_set(paramt, WEED_LEAF_MAX, WEED_SEED_DOUBLE, 1, &max);
  return paramt;
}

static weed_plant_t *weed_text_init(const char *name, const char *def) {
  weed_plant_t *paramt = weed_plant_new(WEED_PLANT_PARAMETER_TEMPLATE);
  int ptype = WEED_PARAM_TEXT;
  weed_plant_t *gui;
  weed_set_string_value(paramt, WEED_LEAF_NAME, name);
  weed_leaf_set(paramt, WEED_LEAF_PARAM_TYPE, WEED_SEED_INT, 1, &ptype);
  weed_leaf_set(paramt, WEED_LEAF_DEFAULT, WEED_SEED_STRING, 1, &def);
  return paramt;
}


LIVES_GLOBAL_INLINE
const lives_object_template_t *lives_object_template_for_type(uint64_t type, uint64_t subtype) {
  if (type == IMkType("obj.MATH")) return maths_object_with_subtype(subtype);
  return NULL;
}


boolean rules_lack_param(lives_rules_t *prereq, const char *pname) {
  weed_param_t *param = weed_param_from_name(prereq->reqs, prereq->n_reqs, pname);
  int flags = weed_get_int_value(param, WEED_LEAF_FLAGS, NULL);
  if ((flags & PARAM_FLAGS_VALUE_SET) || (flags & PARAM_FLAGS_OPTIONAL)) return FALSE;
  param = weed_param_from_name(prereq->oinst->params, prereq->oinst->n_params, pname);
  if (param) return FALSE;
  return TRUE;
}


static void lives_object_status_unref(lives_object_status_t *st) {
  if (--st->refcount < 0) lives_free(st);
}


void lives_object_status_free(lives_object_status_t *st) {
  lives_object_status_unref(st);
}


boolean requirements_met(lives_object_transform_t *tx) {
  lives_req_t *req;
  int flags;
  char *name;
  for (int i = 0; i < tx->prereqs->n_reqs; i++) {
    req = tx->prereqs->reqs[i];
    flags = weed_get_int_value(req, WEED_LEAF_FLAGS, NULL);
    if ((flags & PARAM_FLAGS_VALUE_SET) || (flags & PARAM_FLAGS_OPTIONAL)) continue;
    name = weed_get_string_value(req, WEED_LEAF_NAME, NULL);
    req = weed_param_from_name(tx->prereqs->oinst->params, tx->prereqs->oinst->n_params, name);
    weed_free(name);
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
      for (int i = 0; i < rules->n_reqs; i++) {
        weed_plant_free(rules->reqs[i]);
      }
      lives_free(rules->reqs);
    }
    if (rules->conditions) lives_free(rules->conditions);
    lives_free(rules);
  }
}


void lives_object_transform_free(lives_object_transform_t *tx) {
  lives_rules_unref(tx->prereqs);
  if (tx->funcinfo) lives_free(tx->funcinfo);
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
  /*   flags = weed_get_int_value(param, WEED_LEAF_FLAGS, NULL);  */
  /*   if (!(flags & PARAM_FLAGS_VALUE_SET) && !(flags & PARAM_FLAGS_OPTIONAL)) { */
  /*     xparam = weed_param_from_name(prereq->oinst->params, prereq->oinst->n_params, param->name); */
  /*     weed_leaf_dup(param, xparam, WEED_LEAF_VALUE); */
  /*   } */
  /* } */

  /* pth = lives_proc_thread_create_vargs(LIVES_THRDATTR_FG_THREAD, funcinf->function, */
  /* 				       WEED_SEED_DOUBLE, funcinf->args_fmt, xargs); */


  return NULL;
}



typedef struct {
  int idx;
  lives_clip_t *sfile;
} clip_priv_data_t;

lives_intentparams_t *_get_params_for_clip_tx(lives_object_t *obj, int state,
    lives_intention intent) {
  lives_intentparams_t *iparams = NULL;
  weed_param_t *param;
  clip_priv_data_t *priv = (clip_priv_data_t *)obj->priv;
  if (intent == LIVES_INTENTION_IMPORT_LOCAL) {
    if (prefs->startup_phase != 0) return NULL;
    if (capable->writeable_shmdir == MISSING) return NULL;
    if (state == CLIP_STATE_READY) {
      if (!*priv->sfile->staging_dir) return NULL;
    } else {
      if (!*capable->shmdir_path) {
        char *xshmdir, *shmdir = lives_build_path(LIVES_DEVICE_DIR, LIVES_SHM_DIR, NULL);
        if (!lives_file_test(shmdir, LIVES_FILE_TEST_IS_DIR)) {
          lives_free(shmdir);
          shmdir = lives_build_path(LIVES_RUN_DIR, LIVES_SHM_DIR, NULL);
          if (!lives_file_test(shmdir, LIVES_FILE_TEST_IS_DIR)) {
            lives_free(shmdir);
            capable->writeable_shmdir = MISSING;
            return NULL;
          }
        }
        if (!is_writeable_dir(shmdir)) {
          lives_free(shmdir);
          capable->writeable_shmdir = MISSING;
          return NULL;
        }
        xshmdir = lives_build_path(shmdir, LIVES_DEF_WORK_SUBDIR, NULL);
        lives_free(shmdir);
        lives_snprintf(capable->shmdir_path, PATH_MAX, "%s", xshmdir);
        lives_free(xshmdir);
      }
    }
    iparams = (lives_intentparams_t *)lives_calloc(sizeof(lives_intentparams_t), 1);
    iparams->intent = intent;
    iparams->n_params = 1;
    iparams->params = (weed_param_t **)lives_calloc(sizeof(weed_param_t *), 1);

    if (state == CLIP_STATE_READY) {
      iparams->params[0] = weed_text_init(CLIP_PARAM_STAGING_DIR, priv->sfile->staging_dir);
    } else {
      iparams->params[0] = weed_text_init(CLIP_PARAM_STAGING_DIR, capable->shmdir_path);
    }
  } else if (intent == LIVES_INTENTION_IMPORT_REMOTE) {
    char *uidstr, *tmpdir;
    iparams = (lives_intentparams_t *)lives_calloc(sizeof(lives_intentparams_t), 1);
    iparams->intent = intent;
    iparams->n_params = 1;
    iparams->params = (weed_param_t **)lives_calloc(sizeof(weed_param_t *), 1);
    uidstr = lives_strdup_printf("%lu", gen_unique_id());
    tmpdir = get_systmp(uidstr, TRUE);
    iparams->params[0] = weed_text_init(CLIP_PARAM_STAGING_DIR, tmpdir);
    lives_free(tmpdir);
  }
  return iparams;
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

      tx->prereqs->req[0] = weed_text_init("filename", NULL);

      tx->prereqs->req[1] = weed_float_init("start_time", -1., 0., 0.);

      tx->prereqs->req[2] = weed_integer_init("frames", NULL, -1, 0, -0);
      weed_set_int_value(req, WEED_LEAF_FLAGS, LIVES_REQ_FLAGS_OPTIONAL);

      tx->prereqs->req[3] = weed_switch_init("with_audio", TRUE);
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


// TODO - merge with transform for intent

LIVES_LOCAL_INLINE
lives_intentparams_t *get_txparams_for_intent(lives_object_t *obj, lives_intention intent) {
  if (obj->type == OBJECT_TYPE_CLIP) {
    return _get_params_for_clip_tx(obj, obj->status->state, intent);
  }
  return NULL;
}


// kind of a fake function, until we habe proper objects for clips
lives_intentparams_t *get_txparams_for_clip(int clipno, lives_intention intent) {
  lives_object_t obj;
  lives_object_status_t ostat;
  clip_priv_data_t priv;
  obj.type = OBJECT_TYPE_CLIP;
  obj.status = &ostat;
  obj.priv = &priv;
  priv.idx = clipno;
  if (!IS_VALID_CLIP(clipno)) {
    priv.sfile = NULL;
    ostat.state = CLIP_STATE_NOT_LOADED;
  } else {
    priv.sfile = mainw->files[clipno];
    ostat.state = CLIP_STATE_READY;
  }
  return get_txparams_for_intent(&obj, intent);
}
