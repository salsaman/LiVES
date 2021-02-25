



#include "main.h"

typedef struct {
  int idx;
  lives_clip_t *sfile;
} clip_priv_data_t;

lives_intentparams_t *_get_params_for_clip_tx(lives_object_t *obj, int state,
    lives_intention_t intent) {
  lives_intentparams_t *iparams = NULL;
  lives_param_t *param;
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
    iparams->params = (lives_param_t *)lives_calloc(sizeof(lives_param_t), 1);
    param = &iparams->params[0];
    param->name = lives_strdup("staging directory");
    param->type = LIVES_PARAM_STRING;
    param->max = PATH_MAX;
    param->label = lives_strdup(param->name);
    param->source_type = LIVES_RFX_SOURCE_NEWCLIP;
    if (state == CLIP_STATE_READY) {
      set_string_param(&param->value, priv->sfile->staging_dir, PATH_MAX);
    } else {
      set_string_param(&param->value, capable->shmdir_path, PATH_MAX);
    }
  } else if (intent == LIVES_INTENTION_IMPORT_REMOTE) {
    char *uidstr, *tmpdir;
    iparams = (lives_intentparams_t *)lives_calloc(sizeof(lives_intentparams_t), 1);
    iparams->intent = intent;
    iparams->n_params = 1;
    iparams->params = (lives_param_t *)lives_calloc(sizeof(lives_param_t), 1);
    param = &iparams->params[0];
    param->name = lives_strdup("staging directory");
    param->type = LIVES_PARAM_STRING;
    param->max = PATH_MAX;
    param->label = lives_strdup(param->name);
    param->source_type = LIVES_RFX_SOURCE_NEWCLIP;
    uidstr = lives_strdup_printf("%lu", gen_unique_id());
    tmpdir = get_systmp(uidstr, TRUE);
    set_string_param(&param->value, tmpdir, PATH_MAX);
    lives_free(tmpdir);
  }
  return iparams;
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
      tx->prereqs->reqs = (lives_req_t *)lives_calloc(sizeof(lives_req_t), tx->prereqs->n_reqs);
      req = tx->prereqs->req[0];
      req->name = lives_strdup("filename");
      req->type = LIVES_PARAM_STRING;
      req->max = PATH_MAX;
      req = tx->prereqs->req[1];
      req->name = lives_strdup("start_time");
      req->type = LIVES_PARAM_DOUBLE;
      req->flags |= LIVES_REQ_FLAGS_OPTIONAL;
      req = tx->prereqs->req[2];
      req->name = lives_strdup("frames");
      req->type = LIVES_PARAM_DOUBLE;
      req->flags |= LIVES_REQ_FLAGS_OPTIONAL;
      req = tx->prereqs->req[3];
      req->name = lives_strdup("with_audio");
      req->type = LIVES_PARAM_BOOLEAN;
      req->flags |= LIVES_REQ_FLAGS_OPTIONAL;
      req->new_state = CLIP_STATE_LOADED;
      ///
      // TODO..appent to list, do same for IMPORT_REMOTE but with URI
    }
  }
  return NULL;
}
#endif


LIVES_LOCAL_INLINE
lives_intentparams_t *get_txparams_for_intent(lives_object_t *obj, lives_intention_t intent) {
  if (obj->type == OBJECT_TYPE_CLIP) {
    return _get_params_for_clip_tx(obj, obj->status->state, intent);
  }
  return NULL;
}


// kind of a fake function, until we habe proper objects for clips
lives_intentparams_t *get_txparams_for_clip(int clipno, lives_intention_t intent) {
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
