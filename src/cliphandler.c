// cliphandler.c
// (c) G. Finch 2019 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "cvirtual.h"
#include "startup.h"
#include "callbacks.h"
#include "interface.h"
#include "effects-weed.h"
#include "effects.h"
#include "videodev.h"
#include "ce_thumbs.h"

static void _clip_srcs_free_all(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp);


LIVES_GLOBAL_INLINE int find_clip_by_uid(uint64_t uid) {
  int clipno = -1;
  for (LiVESList *list = mainw->cliplist; list; list = list->next) {
    clipno = LIVES_POINTER_TO_INT(list->data);
    if (IS_VALID_CLIP(clipno)) {
      lives_clip_t *sfile = mainw->files[clipno];
      if (sfile->unique_id == uid) break;
    }
    clipno = -1;
  }
  return clipno;
}


char *get_staging_dir_for(int index, const lives_intentcap_t *icaps) {
  // get staging_dir, this will eventually become an optional attibute for the import transform
  // for medi object instances
  // we will register this function as an Oracle providing the data for that attribute
  // and the function will be consulted during the negotiation stage for the import transform
  // however at this point we don't have anything to represent and unloaded clip,
  // so we have to fake things a bit

  lives_intention intent = icaps->intent;
  lives_clip_t *sfile;

  if (intent != OBJ_INTENTION_IMPORT) return NULL;

  sfile = RETURN_VALID_CLIP(index);

  if (lives_has_capacity(icaps->capacities, CAP_LOCAL)) {
    if (prefs->startup_phase != 0) return NULL;
    if (capable->writeable_shmdir == MISSING) return NULL;
    //if (obj->state == CLIP_STATE_READY) {
    if (sfile && sfile->staging_dir) return lives_strdup(sfile->staging_dir);
    get_shmdir();
    return lives_strdup(capable->shmdir_path);
  } else if (lives_has_capacity(icaps->capacities, CAP_REMOTE)) {
    char *uidstr, *tmpdir;
    uidstr = lives_strdup_printf("ytdl-%lu", gen_unique_id());
    tmpdir = get_systmp(uidstr, TRUE);
    lives_free(uidstr);
    return tmpdir;
  }
  return NULL;
}


LIVES_GLOBAL_INLINE boolean read_from_infofile(FILE *infofile) {
  size_t bread;
  if (!infofile) infofile = fopen(cfile->info_file, "r");
  if (!infofile) return FALSE;
  THREADVAR(read_failed) = FALSE;
  bread = lives_fread(mainw->msg, 1, MAINW_MSG_SIZE, infofile);
  fclose(infofile);
  if (ferror(infofile)) {
    THREADVAR(read_failed) = TRUE;
    return FALSE;
  }
  lives_memset(mainw->msg + bread, 0, 1);
  return TRUE;
}


LIVES_GLOBAL_INLINE
char *use_staging_dir_for(int clipno) {
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
  if (sfile) {
    char *clipdir = get_clip_dir(clipno);
    char *stfile = lives_build_filename(clipdir, LIVES_STATUS_FILE_NAME, NULL);
    lives_free(clipdir);
    lives_snprintf(sfile->info_file, PATH_MAX, "%s", stfile);
    lives_free(stfile);
    if (*sfile->staging_dir) {
      return lives_strdup_printf("%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL,
                                 capable->backend_path, sfile->staging_dir, prefs->configfile);
    }
  }
  return lives_strdup(prefs->backend_sync);
}


char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp) {
  char *key = NULL;

  switch (what) {
  case CLIP_DETAILS_HEADER_VERSION:
    key = lives_strdup("header_version"); break;
  case CLIP_DETAILS_BPP:
    key = lives_strdup("bpp"); break;
  // header v. 104
  case CLIP_DETAILS_IMG_TYPE:
    key = lives_strdup("img_type"); break;
  case CLIP_DETAILS_FPS:
    key = lives_strdup("fps"); break;
  case CLIP_DETAILS_PB_FPS:
    key = lives_strdup("pb_fps"); break;
  case CLIP_DETAILS_WIDTH:
    key = lives_strdup("width"); break;
  case CLIP_DETAILS_HEIGHT:
    key = lives_strdup("height"); break;
  case CLIP_DETAILS_UNIQUE_ID:
    key = lives_strdup("unique_id"); break;
  case CLIP_DETAILS_ARATE:
    key = lives_strdup("audio_rate"); break;
  case CLIP_DETAILS_PB_ARATE:
    key = lives_strdup("pb_audio_rate"); break;
  case CLIP_DETAILS_ACHANS:
    key = lives_strdup("audio_channels"); break;
  case CLIP_DETAILS_ASIGNED:
    key = lives_strdup("audio_signed"); break;
  case CLIP_DETAILS_AENDIAN:
    key = lives_strdup("audio_endian"); break;
  case CLIP_DETAILS_ASAMPS:
    key = lives_strdup("audio_sample_size"); break;
  case CLIP_DETAILS_FRAMES:
    key = lives_strdup("frames"); break;
  case CLIP_DETAILS_TITLE:
    key = lives_strdup("title"); break;
  case CLIP_DETAILS_AUTHOR:
    key = lives_strdup("author"); break;
  case CLIP_DETAILS_COMMENT:
    key = lives_strdup("comment"); break;
  case CLIP_DETAILS_KEYWORDS:
    key = lives_strdup("keywords"); break;
  case CLIP_DETAILS_PB_FRAMENO:
    key = lives_strdup("pb_frameno"); break;
  case CLIP_DETAILS_CLIPNAME:
    key = lives_strdup("clipname"); break;
  case CLIP_DETAILS_FILENAME:
    key = lives_strdup("filename"); break;
  case CLIP_DETAILS_INTERLACE:
    key = lives_strdup("interlace"); break;
  case CLIP_DETAILS_DECODER_NAME:
    key = lives_strdup("decoder"); break;
  case CLIP_DETAILS_DECODER_UID:
    key = lives_strdup("decoder_uid"); break;
  case CLIP_DETAILS_GAMMA_TYPE:
    key = lives_strdup("gamma_type"); break;
  default: break;
  }
  if (maxlenp && !*maxlenp) *maxlenp = 256;
  return key;
}

boolean get_clip_value(int which, lives_clip_details_t what, void *retval, size_t maxlen) {
  // returns TRUE on success
  lives_clip_t *sfile = mainw->files[which];
  char *clipdir, *lives_header = NULL;
  char *val, *key, *tmp;
  uint32_t st = 1;

  int retval2 = LIVES_RESPONSE_NONE;

  if (!IS_VALID_CLIP(which)) return FALSE;
  clipdir = get_clip_dir(which);

  if (!mainw->hdrs_cache) {
    /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
    /// otherwise it may be wrongly classified as a recoverable clip
    /// (here this is largely academic, since the values are only read during crash recovery,
    /// and the header should have been cached)
    if (IS_ASCRAP_CLIP(which)) {
      lives_header = lives_build_filename(clipdir, LIVES_ACLIP_HEADER, NULL);
      if (!lives_file_test(lives_header, LIVES_FILE_TEST_EXISTS)) {
        lives_free(lives_header);
        lives_header = NULL;
      }
    }
    if (!lives_header)
      lives_header = lives_build_filename(clipdir, LIVES_CLIP_HEADER, NULL);
    if (!sfile->checked_for_old_header) {
      struct stat mystat;
      time_t old_time = 0, new_time = 0;
      char *old_header = lives_build_filename(clipdir, LIVES_CLIP_HEADER_OLD, NULL);
      sfile->checked_for_old_header = TRUE;
      if (!lives_file_test(old_header, LIVES_FILE_TEST_EXISTS)) {
        if (!stat(old_header, &mystat)) old_time = mystat.st_mtime;
        if (!stat(lives_header, &mystat)) new_time = mystat.st_mtime;
        if (old_time > new_time) {
          sfile->has_old_header = TRUE;
          lives_free(lives_header);
          lives_free(clipdir);
          return FALSE; // clip has been edited by an older version of LiVES
        }
      }
      lives_free(old_header);
    }
  }

  lives_free(clipdir);
  //////////////////////////////////////////////////
  key = clip_detail_to_string(what, &maxlen);

  if (!key) {
    tmp = lives_strdup_printf("Invalid detail %d requested from file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    return FALSE;
  }

  if (mainw->hdrs_cache) {
    val = get_val_from_cached_list(key, maxlen, mainw->hdrs_cache);
    lives_free(key);
    if (!val) return FALSE;
  } else {
    val = (char *)lives_malloc(maxlen);
    if (!val) return FALSE;
    retval2 = get_pref_from_file(lives_header, key, val, maxlen);
    lives_free(lives_header);
    lives_free(key);
  }

  if (retval2 == LIVES_RESPONSE_CANCEL) {
    lives_free(val);
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_WIDTH:
    *(int *)retval = atoi(val) / VSLICES; break;
  case CLIP_DETAILS_BPP:
  case CLIP_DETAILS_HEIGHT:
  case CLIP_DETAILS_ARATE:
  case CLIP_DETAILS_ACHANS:
  case CLIP_DETAILS_ASAMPS:
  case CLIP_DETAILS_FRAMES: // TODO - frames_t
  case CLIP_DETAILS_GAMMA_TYPE:
  case CLIP_DETAILS_HEADER_VERSION:
    *(int *)retval = atoi(val); break;
  // header v. 104
  case CLIP_DETAILS_IMG_TYPE:
    *(lives_img_type_t *)retval = lives_image_type_to_img_type(val); break;
  case CLIP_DETAILS_ASIGNED:
    *(int *)retval = 0;
    if (!sfile->header_version) *(int *)retval = atoi(val);
    if (!*(int *)retval && (!strcasecmp(val, "false"))) *(int *)retval = 1; // unsigned
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    *(int *)retval = atoi(val);
    if (!retval) *(int *)retval = 1;
    break;
  case CLIP_DETAILS_PB_ARATE:
    *(int *)retval = atoi(val);
    if (!retval) *(int *)retval = sfile->arps;
    break;
  case CLIP_DETAILS_INTERLACE:
    *(int *)retval = atoi(val);
    break;
  case CLIP_DETAILS_FPS:
    *(double *)retval = lives_strtod(val);
    if (*(double *)retval == 0.) *(double *)retval = prefs->default_fps;
    st = WEED_SEED_DOUBLE;
    break;
  case CLIP_DETAILS_PB_FPS:
    *(double *)retval = lives_strtod(val);
    if (*(double *)retval == 0.) *(double *)retval = sfile->fps;
    st = WEED_SEED_DOUBLE;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    *(uint64_t *)retval = (uint64_t)lives_strtoul(val);
    st = WEED_SEED_INT64;
    break;
  case CLIP_DETAILS_AENDIAN:
    *(int *)retval = atoi(val) * 2; break;
  case CLIP_DETAILS_TITLE:
  case CLIP_DETAILS_AUTHOR:
  case CLIP_DETAILS_COMMENT:
  case CLIP_DETAILS_CLIPNAME:
  case CLIP_DETAILS_KEYWORDS:
    lives_snprintf((char *)retval, maxlen, "%s", val);
    st = WEED_SEED_STRING;
    break;
  case CLIP_DETAILS_FILENAME:
  case CLIP_DETAILS_DECODER_NAME:
    lives_snprintf((char *)retval, maxlen, "%s", (tmp = F2U8(val)));
    lives_free(tmp);
    st = WEED_SEED_STRING;
    break;
  case CLIP_DETAILS_DECODER_UID:
    *(uint64_t *)retval = (uint64_t)lives_strtoul(val);
    st = WEED_SEED_INT64;
    break;
  default:
    lives_free(val);
    return FALSE;
  }
  (void)st;
  /* // TODO */
  /* make_object_for_clip(index, icaps); */
  /* if (st == WEED_SEED_INT) { */
  // add clip detail as attribute
  /* } */
  lives_free(val);
  return TRUE;
}


boolean save_clip_value(int which, lives_clip_details_t what, void *val) {
  lives_clip_t *sfile;
  char *clipdir, *lives_header;
  char *com, *tmp;
  char *myval;
  char *key;

  boolean needs_sigs = FALSE;

  THREADVAR(write_failed) = 0;
  THREADVAR(com_failed) = FALSE;

  if (!which || which == mainw->scrap_file) return FALSE;

  if (!IS_PHYSICAL_CLIP(which)) return FALSE;

  sfile = mainw->files[which];

  // make sure we don't try to write in the brief moment when the metadata is being copied
  pthread_mutex_lock(&sfile->transform_mutex);

  /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
  /// otherwise it may be wrongly classified as a recoverable clip
  clipdir = get_clip_dir(which);
  if (IS_ASCRAP_CLIP(which))
    lives_header = lives_build_filename(clipdir, LIVES_ACLIP_HEADER, NULL);
  else
    lives_header = lives_build_filename(clipdir, LIVES_CLIP_HEADER, NULL);

  lives_free(clipdir);
  key = clip_detail_to_string(what, NULL);

  if (!key) {
    tmp = lives_strdup_printf("Invalid detail %d added for file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    pthread_mutex_unlock(&sfile->transform_mutex);
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  // header v. 104
  case CLIP_DETAILS_IMG_TYPE:
    myval = lives_strdup(image_ext_to_lives_image_type(get_image_ext_for_type(*(lives_img_type_t *)val)));
    break;
  case CLIP_DETAILS_FPS:
    if (!sfile->ratio_fps) myval = lives_strdup_printf("%.3f", *(double *)val);
    else myval = lives_strdup_printf("%.8f", *(double *)val);
    break;
  case CLIP_DETAILS_PB_FPS:
    if (sfile->ratio_fps && (sfile->pb_fps == sfile->fps))
      myval = lives_strdup_printf("%.8f", *(double *)val);
    else myval = lives_strdup_printf("%.3f", *(double *)val);
    break;
  case CLIP_DETAILS_WIDTH:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_HEIGHT:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_UNIQUE_ID:
    myval = lives_strdup_printf("%"PRIu64, *(uint64_t *)val); break;
  case CLIP_DETAILS_ARATE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_PB_ARATE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_ACHANS:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_ASIGNED:
    if ((*(int *)val) == 1) myval = lives_strdup("true");
    else myval = lives_strdup("false");
    break;
  case CLIP_DETAILS_AENDIAN:
    myval = lives_strdup_printf("%d", (*(int *)val) / 2);
    break;
  case CLIP_DETAILS_ASAMPS:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_FRAMES: // TODO - frames_t
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_GAMMA_TYPE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_INTERLACE:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_TITLE:
    myval = lives_strdup((char *)val); break;
  case CLIP_DETAILS_AUTHOR:
    myval = lives_strdup((char *)val); break;
  case CLIP_DETAILS_COMMENT:
    myval = lives_strdup((const char *)val); break;
  case CLIP_DETAILS_KEYWORDS:
    myval = lives_strdup((const char *)val); break;
  case CLIP_DETAILS_PB_FRAMENO:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  case CLIP_DETAILS_CLIPNAME:
    myval = lives_strdup((char *)val); break;
  case CLIP_DETAILS_FILENAME:
    myval = U82F((const char *)val); break;
  case CLIP_DETAILS_DECODER_NAME:
    myval = U82F((const char *)val); break;
  // header v. 104
  case CLIP_DETAILS_DECODER_UID:
    myval = lives_strdup_printf("%"PRIu64, *(uint64_t *)val); break;
  case CLIP_DETAILS_HEADER_VERSION:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  default:
    pthread_mutex_unlock(&sfile->transform_mutex);
    return FALSE;
  }

  if (mainw->clip_header) {
    char *keystr_start = lives_strdup_printf("<%s>\n", key);
    char *keystr_end = lives_strdup_printf("\n</%s>\n\n", key);
    lives_fputs(keystr_start, mainw->clip_header);
    lives_fputs(myval, mainw->clip_header);
    lives_fputs(keystr_end, mainw->clip_header);
    lives_free(keystr_start);
    lives_free(keystr_end);
  } else {
    char *lives_header_bak = lives_strdup_printf("%s.%s", lives_header, LIVES_FILE_EXT_BAK);
    char *temp_backend = use_staging_dir_for(which);
    if (!mainw->signals_deferred) {
      set_signal_handlers((lives_sigfunc_t)defer_sigint);
      needs_sigs = TRUE;
    }
    com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"",
                              temp_backend, lives_header, key, myval);
    lives_system(com, FALSE);
    lives_cp(lives_header, lives_header_bak);
    pthread_mutex_unlock(&sfile->transform_mutex);
    if (mainw->signal_caught) catch_sigint(mainw->signal_caught, NULL, NULL);
    if (needs_sigs) set_signal_handlers((lives_sigfunc_t)catch_sigint);
    lives_free(temp_backend);
    lives_free(com);
    lives_free(lives_header_bak);
  }

  pthread_mutex_unlock(&sfile->transform_mutex);

  lives_free(lives_header);
  lives_free(myval);
  lives_free(key);

  if (mainw->clip_header && THREADVAR(write_failed) == fileno(mainw->clip_header) + 1) {
    THREADVAR(write_failed) = 0;
    return FALSE;
  }
  if (THREADVAR(com_failed)) return FALSE;
  return TRUE;
}


boolean save_clip_values(int which) {
  lives_clip_t *sfile = mainw->files[which];
  char *clipdir, *lives_header_new;
  boolean all_ok = FALSE;
  int asigned, endian;
  int retval;

  if (!which || which == mainw->scrap_file || which == mainw->ascrap_file) return TRUE;

  set_signal_handlers((lives_sigfunc_t)defer_sigint); // ignore ctrl-c

  asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
  endian = sfile->signed_endian & AFORM_BIG_ENDIAN;
  clipdir = get_clip_dir(which);
  if (IS_ASCRAP_CLIP(which))
    lives_header_new = lives_build_filename(clipdir, LIVES_ACLIP_HEADER "." LIVES_FILE_EXT_NEW, NULL);
  else
    lives_header_new = lives_build_filename(clipdir, LIVES_CLIP_HEADER "." LIVES_FILE_EXT_NEW, NULL);

  do {
    THREADVAR(com_failed) = THREADVAR(write_failed) = FALSE;
    mainw->clip_header = fopen(lives_header_new, "w");
    if (!mainw->clip_header) {
      retval = do_write_failed_error_s_with_retry(lives_header_new, lives_strerror(errno));
      if (retval == LIVES_RESPONSE_CANCEL) {
        set_signal_handlers((lives_sigfunc_t)catch_sigint);
        if (mainw->signal_caught) catch_sigint(mainw->signal_caught, NULL, NULL);
        lives_free(lives_header_new);
        lives_free(clipdir);
        return FALSE;
      }
    } else {
      sfile->header_version = LIVES_CLIP_HEADER_VERSION;
      do {
        retval = 0;
        if (!save_clip_value(which, CLIP_DETAILS_HEADER_VERSION, &sfile->header_version)) break;

        if (!save_clip_value(which, CLIP_DETAILS_UNIQUE_ID, &sfile->unique_id)) break;
        if (!save_clip_value(which, CLIP_DETAILS_FRAMES, &sfile->frames)) break;
        if (sfile->clip_type == CLIP_TYPE_FILE && get_primary_src(which)) {
          lives_clip_data_t *cdata = get_clip_cdata(which);
          double dfps = (double)cdata->fps;
          if (!save_clip_value(which, CLIP_DETAILS_FPS, &dfps)) break;
          if (!save_clip_value(which, CLIP_DETAILS_PB_FPS, &sfile->fps)) break;
        } else {
          if (!save_clip_value(which, CLIP_DETAILS_FPS, &sfile->fps)) break;
          if (!save_clip_value(which, CLIP_DETAILS_PB_FPS, &sfile->pb_fps)) break;
        }
        if (!save_clip_value(which, CLIP_DETAILS_WIDTH, &sfile->hsize)) break;
        if (!save_clip_value(which, CLIP_DETAILS_HEIGHT, &sfile->vsize)) break;
        if (!save_clip_value(which, CLIP_DETAILS_INTERLACE, &sfile->interlace)) break;
        if (!save_clip_value(which, CLIP_DETAILS_BPP, &sfile->bpp)) break;
        if (!save_clip_value(which, CLIP_DETAILS_IMG_TYPE, &sfile->img_type)) break;
        if (!save_clip_value(which, CLIP_DETAILS_ARATE, &sfile->arps)) break;
        if (!save_clip_value(which, CLIP_DETAILS_PB_ARATE, &sfile->arate)) break;
        if (!save_clip_value(which, CLIP_DETAILS_ACHANS, &sfile->achans)) break;
        if (sfile->achans > 0) {
          if (!save_clip_value(which, CLIP_DETAILS_ASIGNED, &asigned)) break;
          if (!save_clip_value(which, CLIP_DETAILS_AENDIAN, &endian)) break;
        }
        if (!save_clip_value(which, CLIP_DETAILS_ASAMPS, &sfile->asampsize)) break;
        if (!save_clip_value(which, CLIP_DETAILS_GAMMA_TYPE, &sfile->gamma_type)) break;
        if (!save_clip_value(which, CLIP_DETAILS_TITLE, sfile->title)) break;
        if (!save_clip_value(which, CLIP_DETAILS_AUTHOR, sfile->author)) break;
        if (!save_clip_value(which, CLIP_DETAILS_COMMENT, sfile->comment)) break;
        if (!save_clip_value(which, CLIP_DETAILS_PB_FRAMENO, &sfile->frameno)) break;
        if (!save_clip_value(which, CLIP_DETAILS_CLIPNAME, sfile->name)) break;
        if (!save_clip_value(which, CLIP_DETAILS_FILENAME, sfile->file_name)) break;
        if (!save_clip_value(which, CLIP_DETAILS_KEYWORDS, sfile->keywords)) break;
        if (sfile->clip_type == CLIP_TYPE_FILE && get_primary_src(which)) {
          lives_decoder_t *dplug = (lives_decoder_t *)((get_primary_src(which))->actor);
          if (!save_clip_value(which, CLIP_DETAILS_DECODER_NAME, (void *)dplug->dpsys->soname)) break;
          if (!save_clip_value(which, CLIP_DETAILS_DECODER_UID, (void *)&sfile->decoder_uid)) break;
        }
        all_ok = TRUE;
      } while (FALSE);

      fclose(mainw->clip_header);
      mainw->clip_header = NULL;

      if (!all_ok) {
        retval = do_write_failed_error_s_with_retry(lives_header_new, NULL);
      } else {
        char *lives_header;
        if (sfile->clip_type == CLIP_TYPE_DISK) {
          del_clip_value(which, CLIP_DETAILS_DECODER_NAME);
          del_clip_value(which, CLIP_DETAILS_DECODER_UID);
        }
        if (IS_ASCRAP_CLIP(which))
          lives_header = lives_build_filename(clipdir, LIVES_ACLIP_HEADER, NULL);
        else
          lives_header = lives_build_filename(clipdir, LIVES_CLIP_HEADER, NULL);
        // TODO - check the sizes before and after
        lives_cp(lives_header_new, lives_header);
        if (THREADVAR(com_failed) || THREADVAR(write_failed)) {
          retval = do_write_failed_error_s_with_retry(lives_header_new, NULL);
        } else {
          char *lives_header_bak = lives_strdup_printf("%s.%s", lives_header, LIVES_FILE_EXT_BAK);
          lives_mv(lives_header_new, lives_header_bak);
          lives_free(lives_header_bak);
        }
        lives_free(lives_header);
        dump_clip_binfmt(which);
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(clipdir);

  if (mainw->signal_caught) catch_sigint(mainw->signal_caught, NULL, NULL);
  set_signal_handlers((lives_sigfunc_t)catch_sigint);

  lives_free(lives_header_new);

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

  return TRUE;
}


boolean del_clip_value(int which, lives_clip_details_t what) {
  lives_clip_t *sfile;
  char *clipdir, *lives_header, *key, *com;
  char *lives_header_bak, *temp_backend;

  boolean needs_sigs = FALSE;

  if (!which || which == mainw->scrap_file) return FALSE;

  if (!IS_PHYSICAL_CLIP(which)) return FALSE;

  sfile = mainw->files[which];

  // make sure we don't try to write in the brief moment when the metadata is being copied
  pthread_mutex_lock(&sfile->transform_mutex);

  clipdir = get_clip_dir(which);
  if (IS_ASCRAP_CLIP(which))
    lives_header = lives_build_filename(clipdir, LIVES_ACLIP_HEADER, NULL);
  else
    lives_header = lives_build_filename(clipdir, LIVES_CLIP_HEADER, NULL);

  lives_free(clipdir);
  key = clip_detail_to_string(what, NULL);

  if (!key) {
    char *tmp = lives_strdup_printf("Invalid detail %d to be deleted for file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    pthread_mutex_unlock(&sfile->transform_mutex);
    return FALSE;
  }

  lives_header_bak = lives_strdup_printf("%s.%s", lives_header, LIVES_FILE_EXT_BAK);
  temp_backend = use_staging_dir_for(which);

  THREADVAR(write_failed) = 0;
  THREADVAR(com_failed) = FALSE;

  if (!mainw->signals_deferred) {
    set_signal_handlers((lives_sigfunc_t)defer_sigint);
    needs_sigs = TRUE;
  }

  com = lives_strdup_printf("%s del_clip_value \"%s\" \"%s\"",
                            temp_backend, lives_header, key);
  lives_system(com, FALSE);
  lives_free(com);

  if (mainw->signal_caught) catch_sigint(mainw->signal_caught, NULL, NULL);
  else lives_cp(lives_header, lives_header_bak);
  if (needs_sigs) set_signal_handlers((lives_sigfunc_t)catch_sigint);
  lives_free(temp_backend);
  lives_free(lives_header_bak);
  lives_free(lives_header);

  pthread_mutex_unlock(&sfile->transform_mutex);
  return TRUE;
}



size_t reget_afilesize(int fileno) {
  // re-get the audio file size
  lives_clip_t *sfile = mainw->files[fileno];
  boolean bad_header = FALSE;
  off_t res = reget_afilesize_inner(fileno);
  if (res > 0) sfile->afilesize = res;
  else sfile->afilesize = 0;
  if (mainw->multitrack) return sfile->afilesize;

  if (!sfile->afilesize) {
    if (!sfile->opening && fileno != mainw->ascrap_file && fileno != mainw->scrap_file) {
      if (sfile->arate != 0 || sfile->achans != 0 || sfile->asampsize != 0 || sfile->arps != 0) {
        sfile->arate = sfile->achans = sfile->asampsize = sfile->arps = 0;
        if (!save_clip_value(fileno, CLIP_DETAILS_ACHANS, &sfile->achans)) bad_header = TRUE;
        if (!save_clip_value(fileno, CLIP_DETAILS_ARATE, &sfile->arps)) bad_header = TRUE;
        if (!save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &sfile->arate)) bad_header = TRUE;
        if (!save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &sfile->asampsize)) bad_header = TRUE;
        if (bad_header) do_header_write_error(fileno);
      }
    }
  }

  if (!mainw->no_context_update && mainw->is_ready && fileno > 0
      && fileno == mainw->current_file) {
    // force a redraw
    update_play_times();
  }

  return sfile->afilesize;
}


off_t reget_afilesize_inner(int fileno) {
  // safe version that just returns the audio file size
  off_t filesize;
  char *afile = lives_get_audio_file_name(fileno);
  lives_sync(1);
  filesize = sget_file_size(afile);
  lives_free(afile);
  if (filesize < 0) filesize = 0;
  return filesize;
}


boolean read_file_details(const char *file_name, boolean is_audio, boolean is_img) {
  // get preliminary details

  // is_audio set to TRUE prevents us from checking for images, and deleting the (existing) first frame
  // therefore it is IMPORTANT to set it when loading new audio for an existing clip !

  // is_img will force unpacking of img into frames and return the count
  char *tmp, *com = lives_strdup_printf("%s get_details \"%s\" \"%s\" \"%s\" %d", prefs->backend_sync, cfile->handle,
                                        (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)),
                                        get_image_ext_for_type(IMG_TYPE_BEST), mainw->opening_loc ? 3 :
                                        is_audio ? 2 : is_img ? 4 : 0);
  lives_free(tmp);
  lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);
  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  }
  return TRUE;
}


boolean should_use_decoder(int clipno) {
  boolean use_decoder = FALSE;
  lives_clip_t *sfile = RETURN_NORMAL_CLIP(clipno);
  if (sfile) {
    if (sfile->decoder_uid || sfile->old_dec_uid) {
      if (!sfile->old_dec_uid) sfile->old_dec_uid = sfile->decoder_uid;
      use_decoder = TRUE;
    } else {
      if (sfile->header_version >= 104) {
        if (get_clip_value(mainw->current_file, CLIP_DETAILS_DECODER_UID, &sfile->old_dec_uid, 0))
          use_decoder = TRUE;
      } else {
        char decplugname[PATH_MAX];
        if (get_clip_value(mainw->current_file, CLIP_DETAILS_DECODER_NAME, decplugname, PATH_MAX)) {
          sfile->old_dec_uid = 1;
          use_decoder = TRUE;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  return use_decoder;
}


#define _RELOAD(field) sfile->field = loaded->field
#define _RELOAD_STRING(field, len) lives_snprintf(sfile->field, len, "%s", loaded->field)

#define LOADSIZE_MAX 1000000


static boolean recover_from_forensics(int fileno, lives_clip_t *loaded) {
  // try to regenerate file details from the binfmt dump,
  // used if header.lives is missing
  lives_clip_t *sfile = mainw->files[fileno];
  frames_t cframes;
  if (!lives_strcmp(loaded->handle, sfile->handle)) {
    _RELOAD(clip_type);
    _RELOAD(frames);
    _RELOAD(img_type);
    _RELOAD(fps);
    sfile->pb_fps = sfile->fps;
    _RELOAD_STRING(file_name, PATH_MAX);

    if (sfile->clip_type == CLIP_TYPE_DISK) {
      // see if all frames are present
      cframes = get_frame_count(fileno, 1);
    } else {
      if (!*sfile->file_name) return FALSE;
      if ((cframes = load_frame_index(fileno)) == 0) return FALSE;
      else {
        if (sfile->header_version >= 102) sfile->fps = sfile->pb_fps;
        _RELOAD(decoder_uid);;
        if (!reload_clip(fileno, cframes)) return FALSE;
        lives_clip_data_t *cdata = get_clip_cdata(fileno);
        if (sfile->img_type == IMG_TYPE_UNKNOWN) {
          int fvirt = count_virtual_frames(sfile->frame_index, 1, sfile->frames);
          if (fvirt < sfile->frames) {
            if (!sfile->checked && !check_clip_integrity(fileno, cdata, sfile->frames)) {
              sfile->checked = TRUE;
	      // *INDENT-OFF*
	    }}
	  if (!prefs->vj_mode && sfile->needs_update) do_clip_divergence_error(fileno);
	}}}
    // *INDENT-ON*

    if (sfile->frames == cframes) {
      int hsize, vsize;
      if (sfile->clip_type == CLIP_TYPE_FILE || get_frames_sizes(fileno, 1, &hsize, &vsize)) {
        if (sfile->clip_type == CLIP_TYPE_FILE || (hsize == loaded->hsize && vsize == loaded->vsize)) {
          sfile->start = 1;
          sfile->end = sfile->frames;
          _RELOAD(undo1_int); _RELOAD(undo1_dbl); _RELOAD(undo_action);
          _RELOAD(undo_arate); _RELOAD(undo_signed_endian); _RELOAD(undo_achans);
          _RELOAD(undo_asampsize); _RELOAD(undo_arps);
          _RELOAD(undoable); _RELOAD(redoable);
          _RELOAD(hsize); _RELOAD(vsize); _RELOAD(ratio_fps);
          _RELOAD(interlace); _RELOAD(bpp); _RELOAD(deinterlace);
          _RELOAD(gamma_type); _RELOAD(is_untitled);
          _RELOAD_STRING(name, PATH_MAX); _RELOAD_STRING(save_file_name, PATH_MAX);
          _RELOAD_STRING(mime_type, 256); _RELOAD_STRING(type, 64);
          _RELOAD(changed); _RELOAD(orig_file_name);
          _RELOAD(was_renamed); _RELOAD(arps); _RELOAD(arate);
          _RELOAD(achans); _RELOAD(asampsize); _RELOAD(signed_endian);
          _RELOAD(vol); _RELOAD(afilesize); _RELOAD(f_size);
          _RELOAD_STRING(title, 1024); _RELOAD_STRING(author, 1024);
          _RELOAD_STRING(comment, 1024); _RELOAD_STRING(keywords, 1024);
          _RELOAD(unique_id);
	  // *INDENT-OFF*
	}}
      return TRUE;
    }}
  // *INDENT-ON*
  return FALSE;
}


void dump_clip_binfmt(int which) {
  static boolean recurse = FALSE;
  char *fname = lives_build_filename(prefs->workdir, mainw->files[which]->handle,
                                     "." TOTALSAVE_NAME, NULL);
  int fd = lives_create_buffered(fname, DEF_FILE_PERMS);
  lives_write_buffered(fd, (const char *)mainw->files[which], sizeof(lives_clip_t), TRUE);
  lives_close_buffered(fd);
  if (recurse) return;

  if (check_for_executable(&capable->has_gzip, EXEC_GZIP)) {
    char *com = lives_strdup_printf("%s -f %s", EXEC_GZIP, fname), *gzname;
    lives_system(com, TRUE);
    lives_free(com);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      recurse = TRUE;
      dump_clip_binfmt(which);
      recurse = FALSE;
      gzname = lives_strdup_printf("%s.%s", fname, LIVES_FILE_EXT_GZIP);
      if (lives_file_test(gzname, LIVES_FILE_TEST_EXISTS)) lives_rm(gzname);
      lives_free(gzname);
    }
  }
  lives_free(fname);
}


static lives_clip_t *_restore_binfmt(int clipno, boolean forensic, char *binfmtname) {
  lives_clip_t *sfile = RETURN_NORMAL_CLIP(clipno);
  if (sfile) {
    char *clipdir = get_clip_dir(clipno);
    char *fname = lives_build_filename(clipdir, "." TOTALSAVE_NAME, NULL);
    char *gzname = lives_strdup_printf("%s.%s", fname, LIVES_FILE_EXT_GZIP);
    lives_free(clipdir);
    if ((binfmtname && !lives_strcmp(binfmtname, gzname)) || lives_file_test(gzname, LIVES_FILE_TEST_EXISTS)) {
      char *com;
      LiVESResponseType resp = LIVES_RESPONSE_NONE;
      do {
        if (!check_for_executable(&capable->has_gzip, EXEC_GZIP)) {
          resp = do_please_install(_("Unable to restore this clip"), EXEC_GZIP, NULL, INSTALL_IMPORTANT);
          if (resp == LIVES_RESPONSE_CANCEL) {
            lives_free(gzname);
            return NULL;
          }
        }
      } while (resp == LIVES_RESPONSE_RETRY);
      com = lives_strdup_printf("%s -df %s", EXEC_GZIP, gzname);
      lives_system(com, TRUE);
      lives_free(com);
    }
    lives_free(gzname);
    if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
      ssize_t bytes;
      size_t cursize = sizeof(lives_clip_t);
      size_t curdatasize = offsetof(lives_clip_t, binfmt_end);
      size_t loadsize;
      size_t missing = 0, extra;
      char *xloaded = (char *)lives_calloc(1, cursize);
      boolean badsize = FALSE;
      int fd = lives_open_buffered_rdonly(fname);
      size_t filesize = lives_buffered_orig_size(fd);
      lives_clip_t *loaded = (lives_clip_t *)xloaded;
      if (filesize < cursize) {
        missing = cursize - filesize;
      }
      bytes = lives_read_buffered(fd, xloaded, 8, TRUE);
      if (bytes < 8 || lives_memcmp(loaded->binfmt_check.chars, CLIP_BINFMT_CHECK, 8)) {
        badsize = TRUE;
      } else {
        bytes += lives_read_buffered(fd, xloaded + 8, 16, TRUE);
        if (bytes < 24) {
          badsize = TRUE;
        }
        loadsize = loaded->binfmt_bytes.num;
        if (loadsize > curdatasize) {
          // anything from 'future LiVES' should end up in binfmt_reserved
          // and be preserved, provided it fits
          extra = loadsize - curdatasize;
          if (extra > BINFMT_RSVD_BYTES) badsize = TRUE;
        }
        if (loadsize + missing < curdatasize) {
          badsize = TRUE;
        } else {
          if (loadsize > cursize && loadsize < LOADSIZE_MAX) {
            xloaded = lives_realloc(xloaded, loadsize);
            loaded = (lives_clip_t *)xloaded;
          } else loadsize = cursize;
          bytes += lives_read_buffered(fd, xloaded + 24, loadsize - 24, TRUE);
          if (bytes < loadsize) {
            badsize = TRUE;
          }
	  // *INDENT-OFF*
	}}
      // *INDENT-ON*
      lives_close_buffered(fd);
      //if (!forensic) lives_rm(fname);
      lives_free(fname);

      if (badsize) {
        lives_free(xloaded);
        return NULL;
      }

      THREADVAR(com_failed) = FALSE;
      if (THREADVAR(read_failed) == fd + 1) {
        THREADVAR(read_failed) = 0;
        lives_free(xloaded);
        return NULL;
      }

      sfile->has_binfmt = TRUE;

      if (forensic) return loaded;

      _RELOAD_STRING(save_file_name, PATH_MAX);  _RELOAD(start); _RELOAD(end); _RELOAD(is_untitled); _RELOAD(was_in_set);
      _RELOAD(ratio_fps); _RELOAD_STRING(mime_type, 256);
      _RELOAD(changed); _RELOAD(deinterlace); _RELOAD(vol);

      if (missing < 2 * sizeof(double)) {
        _RELOAD(pb_fps);
        if (missing < sizeof(double)) _RELOAD(target_framerate);
      }

      if (sfile->start < 1) sfile->start = 1;
      if (sfile->end < 1) sfile->end = sfile->frames;
      if (sfile->end > sfile->frames) sfile->end = sfile->frames;
      if (sfile->start > sfile->end) sfile->start = sfile->end;
      if (sfile->end < sfile->start) sfile->end = sfile->start;
      if (lives_strlen(sfile->save_file_name) > PATH_MAX) lives_memset(sfile->save_file_name, 0, PATH_MAX);
      if (sfile->pointer_time > sfile->video_time) sfile->pointer_time = 0.;
      if (sfile->real_pointer_time > CLIP_TOTAL_TIME(clipno)) sfile->real_pointer_time = sfile->pointer_time;
      return loaded;
    }
    lives_free(fname);
  }
  return NULL;
}

#undef _RELOAD
#undef _RELOAD_STRING

boolean restore_clip_binfmt(int clipno) {
  lives_clip_t *recov = _restore_binfmt(clipno, FALSE, NULL);
  if (!recov) return FALSE;
  lives_free(recov);
  return TRUE;
}


LIVES_GLOBAL_INLINE lives_clip_t *clip_forensic(int clipno, char *binfmtname) {
  return _restore_binfmt(clipno, TRUE, binfmtname);
}


LIVES_GLOBAL_INLINE void clear_event_frames(int clipno) {
  char *clipdir = get_clip_dir(clipno);
  char *hdrfile = lives_build_filename(clipdir, LIVES_LITERAL_EVENT "." LIVES_LITERAL_FRAMES, NULL);
  lives_rm(hdrfile);
  lives_free(hdrfile);
  lives_free(clipdir);
}


int save_event_frames(int clipno) {
  // when doing a resample, we save a list of frames for the back end to do
  // a reorder

  // here we also update the frame_index for clips of type CLIP_TYPE_FILE
  lives_clip_t *sfile;

  char *clipdir = get_clip_dir(clipno);
  char *hdrfile = lives_build_filename(clipdir, LIVES_LITERAL_EVENT "." LIVES_LITERAL_FRAMES, NULL);

  frames_t i = 0;
  int header_fd;
  int retval;
  int perf_start, perf_end;
  int nevents;

  lives_free(clipdir);

  sfile = mainw->files[clipno];

  if (!sfile->event_list) {
    lives_rm(hdrfile);
    return -1;
  }

  perf_start = (int)(sfile->fps * event_list_get_start_secs(sfile->event_list)) + 1;
  perf_end = perf_start + (nevents = count_events(sfile->event_list, FALSE, 0, 0)) - 1;

  if (!event_list_to_block(sfile->event_list, nevents)) return -1;

  if (sfile->frame_index) {
    LiVESResponseType response;
    frames_t xframes = sfile->frames;
    char *what = (_("creating the frame index for resampling "));

    if (sfile->frame_index_back) lives_free(sfile->frame_index_back);
    sfile->frame_index_back = sfile->frame_index;
    sfile->frame_index = NULL;

    do {
      response = LIVES_RESPONSE_OK;
      create_frame_index(clipno, FALSE, 0, nevents);
      if (!sfile->frame_index) {
        response = do_memory_error_dialog(what, nevents * 4);
      }
    } while (response == LIVES_RESPONSE_RETRY);
    lives_free(what);
    if (response == LIVES_RESPONSE_CANCEL) {
      sfile->frame_index = sfile->frame_index_back;
      sfile->frame_index_back = NULL;
      return -1;
    }

    for (i = 0; i < nevents; i++) {
      sfile->frame_index[i] = sfile->frame_index_back[(sfile->resample_events + i)->value - 1];
    }

    sfile->frames = nevents;
    if (!check_if_non_virtual(clipno, 1, sfile->frames)) save_frame_index(clipno);
    sfile->frames = xframes;
  }

  do {
    retval = 0;
    header_fd = creat(hdrfile, S_IRUSR | S_IWUSR);
    if (header_fd < 0) {
      retval = do_write_failed_error_s_with_retry(hdrfile, lives_strerror(errno));
    } else {
      // use machine endian.
      // When we call "smogrify reorder", we will pass the endianness as 3rd parameter

      THREADVAR(write_failed) = FALSE;
      lives_write(header_fd, &perf_start, 4, FALSE);

      if (sfile->resample_events) {
        for (i = 0; i <= perf_end - perf_start; i++) {
          if (THREADVAR(write_failed)) break;
          /// TODO: frames64_t
          lives_write(header_fd, &((sfile->resample_events + i)->value), 4, TRUE);
        }
        lives_freep((void **)&sfile->resample_events);
      }

      if (THREADVAR(write_failed)) {
        retval = do_write_failed_error_s_with_retry(hdrfile, NULL);
      }

      close(header_fd);
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  if (retval == LIVES_RESPONSE_CANCEL) i = -1;

  lives_free(hdrfile);
  return i;
}


void remove_old_headers(int clipno) {
  if (IS_VALID_CLIP(clipno)) {
    char *clipdir = get_clip_dir(clipno);
    char *hdrfile = lives_build_filename(clipdir, LIVES_CLIP_HEADER_OLD, NULL);
    lives_rm(hdrfile);
    lives_free(hdrfile);
    hdrfile = lives_build_filename(clipdir, LIVES_CLIP_HEADER_OLD2, NULL);
    lives_rm(hdrfile);
    lives_free(hdrfile);
    lives_free(clipdir);
  }
}


boolean update_clips_version(int fileno) {
  if (IS_VALID_CLIP(fileno)) {
    lives_clip_t *sfile = mainw->files[fileno];
    // in newer header versions, fps holds the value from the decoder
    // whereas pb_fps holds the actual value (e.g if the clip framerate was adjusted or resampled)
    // in version < 102, pb_fps was playback fps, which was not very useful
    // this is analagous to arps / arate where arps holds the file sample rate and arate holds the resampled rate
    if (sfile->clip_type == CLIP_TYPE_FILE && sfile->header_version >= 102)
      sfile->fps = sfile->pb_fps;
    if (sfile->header_version < 103) {
      char *clipdir = get_clip_dir(fileno);
      char *binfmt = lives_build_filename(clipdir, TOTALSAVE_NAME, NULL);
      lives_free(clipdir);
      if (lives_file_test(binfmt, LIVES_FILE_TEST_EXISTS)) {
        // this is mainly for aesthetic reasons
        char *binfmt_to = lives_build_filename(clipdir, "." TOTALSAVE_NAME, NULL);
        lives_mv(binfmt, binfmt_to);
        lives_free(binfmt_to);
      }
      lives_free(binfmt);
    }
  }
  return FALSE;
}


boolean write_headers(int clipno) {
  // this function is included only for backwards compatibility with ancient builds of LiVES
  //
  lives_clip_t *sfile;
  int retval;
  int header_fd;
  char *clipdir, *hdrfile;

  if (!IS_VALID_CLIP(clipno)) return FALSE;
  sfile = mainw->files[clipno];

  // save the file details
  clipdir = get_clip_dir(clipno);
  hdrfile = lives_build_filename(clipdir, LIVES_CLIP_HEADER_OLD, NULL);

  do {
    retval = 0;
    header_fd = creat(hdrfile, S_IRUSR | S_IWUSR);
    if (header_fd < 0) {
      retval = do_write_failed_error_s_with_retry(hdrfile, lives_strerror(errno));
    } else {
      THREADVAR(write_failed) = FALSE;

      lives_write_le(header_fd, &sfile->bpp, 4, TRUE);
      lives_write_le(header_fd, &sfile->fps, 8, TRUE);
      lives_write_le(header_fd, &sfile->hsize, 4, TRUE);
      lives_write_le(header_fd, &sfile->vsize, 4, TRUE);
      lives_write_le(header_fd, &sfile->arps, 4, TRUE);
      lives_write_le(header_fd, &sfile->signed_endian, 4, TRUE);
      lives_write_le(header_fd, &sfile->arate, 4, TRUE);
      lives_write_le(header_fd, &sfile->unique_id, 8, TRUE);
      lives_write_le(header_fd, &sfile->achans, 4, TRUE);
      lives_write_le(header_fd, &sfile->asampsize, 4, TRUE);

      lives_write(header_fd, LiVES_VERSION, strlen(LiVES_VERSION), TRUE);
      close(header_fd);

      if (THREADVAR(write_failed)) retval = do_write_failed_error_s_with_retry(hdrfile, NULL);
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(hdrfile);

  if (retval != LIVES_RESPONSE_CANCEL) {
    // more file details (since version 0.7.5)
    hdrfile = lives_build_filename(clipdir, LIVES_CLIP_HEADER_OLD2, NULL);

    do {
      retval = 0;
      header_fd = creat(hdrfile, S_IRUSR | S_IWUSR);

      if (header_fd < 0) {
        retval = do_write_failed_error_s_with_retry(hdrfile, lives_strerror(errno));
      } else {
        THREADVAR(write_failed) = FALSE;
        lives_write_le(header_fd, &sfile->frames, 4, TRUE);
        lives_write(header_fd, &sfile->title, 1024, TRUE);
        lives_write(header_fd, &sfile->author, 1024, TRUE);
        lives_write(header_fd, &sfile->comment, 1024, TRUE);
        close(header_fd);
      }
      if (THREADVAR(write_failed)) retval = do_write_failed_error_s_with_retry(hdrfile, NULL);
    } while (retval == LIVES_RESPONSE_RETRY);

    lives_free(hdrfile);
  }

  lives_free(clipdir);

  if (retval == LIVES_RESPONSE_CANCEL) {
    THREADVAR(write_failed) = FALSE;
    return FALSE;
  }
  return TRUE;
}


LIVES_GLOBAL_INLINE void make_cleanable(int clipno, boolean isit) {
  if (IS_VALID_CLIP(clipno)) {
    char *clipdir = get_clip_dir(clipno);
    char *clnup = lives_build_filename(clipdir, LIVES_FILENAME_NOCLEAN, NULL);
    if (!isit) lives_touch(clnup);
    else lives_rm(clnup);
    lives_free(clnup);
    lives_free(clipdir);
  }
}


LIVES_GLOBAL_INLINE boolean ignore_clip(int clipno) {
  boolean do_ignore = TRUE;
  if (IS_VALID_CLIP(clipno)) {
    char *clipdir = get_clip_dir(clipno);
    char *ignore = lives_build_filename(clipdir, LIVES_FILENAME_IGNORE, NULL);
    if (!lives_file_test(ignore, LIVES_FILE_TEST_EXISTS)) do_ignore = FALSE;
    lives_free(ignore);
    lives_free(clipdir);
  }
  return do_ignore;
}


// returns TRUE if we delete
boolean do_delete_or_mark(int clipno, int typ) {
  char *clipdir = NULL;
  boolean rember = FALSE, ret = FALSE;
  LiVESResponseType resp = LIVES_RESPONSE_NONE;
  clipdir = get_clip_dir(clipno);
  if (prefs->badfile_intent == OBJ_INTENTION_UNKNOWN) {
    LiVESWidget *vbox, *check;
    LiVESWidget *dialog;
    char *act, *msg;
    if (!typ) act = lives_strdup(_("become damaged beyond repair"));
    else act = lives_strdup(_("been opened with an unavailable decoder"));
    msg = lives_strdup_printf(_("LiVES was unable to reload a clip which seems to have %s.\n"
                                "This clip can be deleted (LiVES copy, not the original !), "
                                "or marked as unopenable and ignored\n"
                                "What would you like to do ?\n"), act);
    dialog = create_question_dialog(_("Cleanup options"), msg);
    lives_free(act); lives_free(msg);
    lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_ADD,
                                       _("Leave it and mark as 'unopenable'"), LIVES_RESPONSE_NO);

    lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL,
                                       _("Delete LiVES' copy of the clip"), LIVES_RESPONSE_YES);

    vbox = lives_dialog_get_action_area((LIVES_DIALOG(dialog)));

    check = lives_standard_check_button_new(_("Always do this for unopenable clips"),
                                            FALSE, LIVES_BOX(vbox), NULL);
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(check), &rember, FALSE);

    resp = lives_dialog_run_with_countdown(LIVES_DIALOG(dialog), LIVES_RESPONSE_YES, 2);
    lives_widget_destroy(dialog);
  }
  if (prefs->badfile_intent == OBJ_INTENTION_DELETE || resp == LIVES_RESPONSE_YES) {
    lives_rmdir(clipdir, TRUE);
    if (rember) pref_factory_int(PREF_BADFILE_INTENT, &prefs->badfile_intent, OBJ_INTENTION_DELETE, TRUE);
    ret = TRUE;
  } else {
    char *ignore = lives_build_filename(clipdir, LIVES_FILENAME_IGNORE, NULL);
    lives_touch(ignore);
    lives_free(ignore);
    if (rember) pref_factory_int(PREF_BADFILE_INTENT, &prefs->badfile_intent, OBJ_INTENTION_IGNORE, TRUE);
  }
  lives_free(clipdir);
  return ret;
}


static boolean recover_details(int fileno, char *hdr, char *hdrback, char *binfmt) {
  boolean ret = do_yesno_dialog(_("LiVES was unable to find the data header for a clip to be reloaded.\n"
                                  "Recovery from backup sources can be attempted. Click Yes to try this,\n"
                                  "or No to skip this and delete it or mark it as ignored\n"));
  if (ret) {
    if (hdrback) {
      lives_cp(hdrback, hdr);
      return TRUE;
    } else {
      lives_clip_t *recovered = clip_forensic(fileno, binfmt);
      if (recovered) {
        if (recover_from_forensics(fileno, recovered)) {
          char *clipdir = get_clip_dir(fileno);
          char *binfmt_to = lives_build_filename(clipdir, "." TOTALSAVE_NAME, NULL);
          if (lives_strncmp(binfmt_to, binfmt, lives_strlen(binfmt_to))) {
            lives_mv(binfmt, binfmt_to);
          }
          lives_free(recovered);
          lives_free(binfmt_to);
          return TRUE;
        }
        lives_free(recovered);
      }
    }
  }
  return FALSE;
}


boolean read_headers(int clipno, const char *dir, const char *file_name) {
  // file_name is only used to get the file size on the disk
  lives_clip_t *sfile;
  char **array;
  char buff[1024];
  char version[32];
  char *com, *tmp, *stdir;
  char *old_hdrfile, *lives_header = NULL;

  off_t header_size;
  int version_hash;
  int pieces;
  int header_fd;
  int retval2;
  int asigned = 0, aendian = LIVES_LITTLE_ENDIAN;

  lives_clip_details_t detail;

  boolean retval, retvala;
  boolean is_ascrap = FALSE;
  boolean bak_tried = FALSE, binfmt_tried = FALSE;
  boolean ahdr_tried = FALSE;

  off_t sizhead = 28; //8 * 4 + 8 + 8;

  time_t old_time = 0, new_time = 1;
  struct stat mystat;

  if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

  if (!IS_VALID_CLIP(clipno)) return FALSE;

  sfile = mainw->files[clipno];

  old_hdrfile = lives_build_filename(dir, LIVES_CLIP_HEADER_OLD, NULL);

  if (IS_ASCRAP_CLIP(clipno)) {
    is_ascrap = TRUE;
    /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
    /// otherwise it may be wrongly classified as a recoverable clip
    lives_header = lives_build_filename(dir, LIVES_ACLIP_HEADER, NULL);
    if (!lives_file_test(lives_header, LIVES_FILE_TEST_EXISTS)) {
      lives_free(lives_header);
      lives_header = NULL;
    }
  }
  if (!lives_header) lives_header = lives_build_filename(dir, LIVES_CLIP_HEADER, NULL);

  sfile->checked_for_old_header = TRUE;
  sfile->img_type = IMG_TYPE_UNKNOWN;

frombak:

  if (lives_file_test(lives_header, LIVES_FILE_TEST_EXISTS)) {
    do {
      retval2 = LIVES_RESPONSE_OK;
      if (!(mainw->hdrs_cache = cache_file_contents(lives_header))) {
        if (clipno != mainw->current_file) goto rhd_failed;
        retval2 = do_read_failed_error_s_with_retry(lives_header, NULL);
      }
    } while (retval2 == LIVES_RESPONSE_RETRY);

    if (retval2 == LIVES_RESPONSE_CANCEL) {
      goto rhd_failed;
    }

    if (clipno == mainw->current_file) {
      threaded_dialog_spin(0.);
    }

    if (!is_ascrap) restore_clip_binfmt(clipno);

    do {
      do {
        detail = CLIP_DETAILS_HEADER_VERSION;
        retval = get_clip_value(clipno, detail, &sfile->header_version, 16);
        if (retval) {
          if (sfile->header_version < 100) goto old_check;
        } else {
          if (lives_file_test(old_hdrfile, LIVES_FILE_TEST_EXISTS)) {
            goto old_check;
          }
          if (clipno != mainw->current_file) {
            goto rhd_failed;
          }
          if (mainw->hdrs_cache) {
            retval2 = do_header_missing_detail_error(clipno, CLIP_DETAILS_HEADER_VERSION);
          } else {
            retval2 = do_header_read_error_with_retry(clipno);
          }
        }
      } while (retval2 == LIVES_RESPONSE_RETRY);

      if (retval2 == LIVES_RESPONSE_CANCEL) goto rhd_failed;

      if (is_ascrap) goto get_avals;

      if (sfile->header_version >= 104) {
        uint64_t uid = sfile->unique_id;
        detail = CLIP_DETAILS_UNIQUE_ID;
        retval = get_clip_value(clipno, detail, &sfile->unique_id, 0);
        if (!sfile->unique_id) sfile->unique_id = uid;
      }

      if (sfile->header_version >= 104) {
        retval = get_clip_value(clipno, CLIP_DETAILS_DECODER_UID, &sfile->decoder_uid, 0);
        if (!retval) {
          retval = TRUE;
          sfile->decoder_uid = 0;
        }
      }

      detail = CLIP_DETAILS_FRAMES;
      retval = get_clip_value(clipno, detail, &sfile->frames, 0);
      if (!retval) {
        if (prefs->show_dev_opts) {
          g_printerr("no framecount detected\n");
        }
        sfile->needs_update = retval = TRUE;
      }

      if (retval) {
        int bpp = sfile->bpp;
        detail = CLIP_DETAILS_BPP;
        get_clip_value(clipno, detail, &sfile->bpp, 0);
        if (!retval) {
          sfile->bpp = bpp;
          retval = TRUE;
        }
      }
      if (retval) {
        detail = CLIP_DETAILS_IMG_TYPE;
        get_clip_value(clipno, detail, &sfile->img_type, 0);
      }
      if (retval) {
        detail = CLIP_DETAILS_FPS;
        retval = get_clip_value(clipno, detail, &sfile->fps, 0);
      }
      if (retval) {
        detail = CLIP_DETAILS_PB_FPS;
        retval = get_clip_value(clipno, detail, &sfile->pb_fps, 0);
        if (!retval) {
          retval = TRUE;
          sfile->pb_fps = sfile->fps;
        }
      }
      if (retval) {
        retval = get_clip_value(clipno, CLIP_DETAILS_PB_FRAMENO, &sfile->frameno, 0);
        if (!retval) {
          retval = TRUE;
          sfile->frameno = 1;
        }
        if (sfile->frameno <= 0) sfile->frameno = 1;
      }
      if (retval) {
        detail = CLIP_DETAILS_WIDTH;
        retval = get_clip_value(clipno, detail, &sfile->hsize, 0);
        if (!retval) {
          if (prefs->show_dev_opts) {
            g_printerr("no frame width detected\n");
          }
          sfile->needs_update = TRUE;
          retval = TRUE;
        }
      }
      if (retval) {
        detail = CLIP_DETAILS_HEIGHT;
        retval = get_clip_value(clipno, detail, &sfile->vsize, 0);
        if (!retval) {
          if (prefs->show_dev_opts) {
            g_printerr("no frame height detected\n");
          }
          sfile->needs_update = TRUE;
          retval = TRUE;
        }
      }
      if (retval) {
        if (sfile->header_version > 100) {
          detail = CLIP_DETAILS_GAMMA_TYPE;
          get_clip_value(clipno, detail, &sfile->gamma_type, 0);
          if (sfile->gamma_type == WEED_GAMMA_UNKNOWN) sfile->gamma_type = WEED_GAMMA_SRGB;
          if (sfile->gamma_type != WEED_GAMMA_SRGB) {
            if (!do_gamma_import_warn(sfile->has_binfmt ?
                                      sfile->binfmt_version.num : 0, sfile->gamma_type)) goto rhd_failed;
          }
        }
      }
      if (retval) {
        detail = CLIP_DETAILS_CLIPNAME;
        get_clip_value(clipno, detail, sfile->name, CLIP_NAME_MAXLEN);
      }
      if (retval) {
        detail = CLIP_DETAILS_FILENAME;
        get_clip_value(clipno, detail, sfile->file_name, PATH_MAX);
      }

get_avals:
      if (retval) {
        detail = CLIP_DETAILS_ACHANS;
        retvala = get_clip_value(clipno, detail, &sfile->achans, 0);
        if (!retvala) sfile->achans = 0;
      }

      if (!sfile->achans) retvala = FALSE;
      else retvala = TRUE;

      if (retval && retvala) {
        detail = CLIP_DETAILS_ARATE;
        retvala = get_clip_value(clipno, detail, &sfile->arps, 0);
      }

      if (!retvala) sfile->arps = sfile->achans = sfile->arate = sfile->asampsize = 0;
      if (!sfile->arps) retvala = FALSE;

      if (retvala && retval) {
        detail = CLIP_DETAILS_PB_ARATE;
        retvala = get_clip_value(clipno, detail, &sfile->arate, 0);
        if (!retvala) {
          retvala = TRUE;
          sfile->arate = sfile->arps;
        }
      }
      if (retvala && retval) {
        detail = CLIP_DETAILS_ASIGNED;
        retval = get_clip_value(clipno, detail, &asigned, 0);
      }
      if (retvala && retval) {
        detail = CLIP_DETAILS_AENDIAN;
        retval = get_clip_value(clipno, detail, &aendian, 0);
      }

      sfile->signed_endian = asigned + aendian;

      if (retvala && retval) {
        detail = CLIP_DETAILS_ASAMPS;
        retval = get_clip_value(clipno, detail, &sfile->asampsize, 0);
      }
      if (!retval) {
        if (clipno != mainw->current_file) goto rhd_failed;
        if (mainw->hdrs_cache) {
          retval2 = do_header_missing_detail_error(clipno, detail);
        } else {
          retval2 = do_header_read_error_with_retry(clipno);
        }
      } else {
        if (!is_ascrap) {
          get_clip_value(clipno, CLIP_DETAILS_TITLE, sfile->title, 1024);
          get_clip_value(clipno, CLIP_DETAILS_AUTHOR, sfile->author, 1024);
          get_clip_value(clipno, CLIP_DETAILS_COMMENT, sfile->comment, 1024);
          get_clip_value(clipno, CLIP_DETAILS_KEYWORDS, sfile->keywords, 1024);
          get_clip_value(clipno, CLIP_DETAILS_INTERLACE, &sfile->interlace, 0);
          // user must have selected this:
          if (sfile->interlace != LIVES_INTERLACE_NONE) sfile->deinterlace = TRUE;
        }
        lives_free(old_hdrfile);
        lives_free(lives_header);
        if (!prefs->vj_mode) {
          sfile->afilesize = reget_afilesize_inner(clipno);
        }
        /// need to maintain mainw->hdrs_cache in this case, as it may be
        // passed to further functions, but it needs to be freed and set to NULL
        // at some point
        goto cleanup;
      }
    } while (retval2 == LIVES_RESPONSE_RETRY);
    goto rhd_failed;
  }

old_check:

  if (lives_file_test(old_hdrfile, LIVES_FILE_TEST_EXISTS)) {
    sfile->has_old_header = TRUE;
    if (!stat(old_hdrfile, &mystat)) old_time = mystat.st_mtime;
    if (!stat(lives_header, &mystat)) new_time = mystat.st_mtime;
  }

  ///////////////

  if (sfile->has_old_header && old_time <= new_time) {
    retval2 = LIVES_RESPONSE_OK;
    detail = CLIP_DETAILS_FRAMES;

    if (get_clip_value(clipno, detail, &sfile->frames, 0)) {
      char *tmp;

      // use new style header (LiVES 0.9.6+)
      // clean up and get file sizes
      if (file_name) {
        com = lives_strdup_printf("%s restore_details \"%s\" \"%s\" 0",
                                  prefs->backend_sync, sfile->handle,
                                  (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));
        lives_free(tmp);
      } else
        com = lives_strdup_printf("%s restore_details \"%s\" . 1", prefs->backend_sync, sfile->handle);

      lives_popen(com, clipno != mainw->current_file, buff, 1024);
      lives_free(com);

      if (THREADVAR(com_failed)) {
        THREADVAR(com_failed) = FALSE;
        goto rhd_failed;
      }

      pieces = get_token_count(buff, '|');

      if (pieces > 3) {
        array = lives_strsplit(buff, "|", pieces);
        sfile->f_size = strtol(array[1], NULL, 10);
        sfile->afilesize = strtol(array[2], NULL, 10);
        if (sfile->clip_type == CLIP_TYPE_DISK) {
          if (!strcmp(array[3], LIVES_FILE_EXT_JPG)) sfile->img_type = IMG_TYPE_JPEG;
          else sfile->img_type = IMG_TYPE_PNG;
        }
        lives_strfreev(array);
      }
      if (clipno == mainw->current_file) threaded_dialog_spin(0.);
    } else goto rhd_failed;
    lives_free(old_hdrfile);
    /// mainw->hdrs_cache never set
    goto cleanup;
  }

  do {
    // old style headers (pre 0.9.6)
    retval = LIVES_RESPONSE_OK;
    THREADVAR(read_failed) = FALSE;
    lives_memset(version, 0, 32);
    lives_memset(buff, 0, 1024);

    header_fd = lives_open2(old_hdrfile, O_RDONLY);

    if (header_fd < 0) {
      // sfile->has_old_header should already be FALSE, but we checked anyway
      sfile->has_old_header = FALSE;
      goto rhd_failed;
    } else {
      THREADVAR(read_failed) = FALSE;
      header_size = get_file_size(header_fd, TRUE);

      if (header_size < sizhead) {
        close(header_fd);
        goto rhd_failed;
      } else {
        THREADVAR(read_failed) = FALSE;
        lives_read_le(header_fd, &sfile->fps, 8, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->bpp, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->hsize, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->vsize, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->arps, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->signed_endian, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->arate, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->unique_id, 8, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->achans, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->asampsize, 4, FALSE);

        if (header_size > sizhead) {
          if (header_size - sizhead > 31) {
            if (!THREADVAR(read_failed))
              lives_read(header_fd, &version, 31, FALSE);
            version[31] = '\0';
          } else {
            if (!THREADVAR(read_failed))
              lives_read(header_fd, &version, header_size - sizhead, FALSE);
            version[header_size - sizhead] = '\0';
          }
        }
      }
      close(header_fd);
    }

    if (THREADVAR(read_failed)) {
      if (clipno != mainw->current_file) goto rhd_failed;
      retval = do_read_failed_error_s_with_retry(old_hdrfile, NULL);
      if (retval == LIVES_RESPONSE_CANCEL) goto rhd_failed;
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_freep((void **)&old_hdrfile);

  if (retval == LIVES_RESPONSE_CANCEL) goto rhd_failed;

  // handle version changes
  version_hash = verhash(version);
  if (version_hash < 7001) {
    sfile->arps = sfile->arate;
    sfile->signed_endian = mainw->endian;
  }

  com = lives_strdup_printf("%s restore_details %s %s %d", prefs->backend_sync, sfile->handle,
                            (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)),
                            !strcmp(file_name, "."));

  lives_popen(com, FALSE, buff, 1024);
  lives_free(com); lives_free(tmp);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    goto rhd_failed;
  }

  pieces = get_token_count(buff, '|');
  array = lives_strsplit(buff, "|", pieces);
  sfile->f_size = strtol(array[1], NULL, 10);
  sfile->afilesize = strtol(array[2], NULL, 10);

  if (sfile->clip_type == CLIP_TYPE_DISK) {
    if (!strcmp(array[3], LIVES_FILE_EXT_JPG)) sfile->img_type = IMG_TYPE_JPEG;
    else sfile->img_type = IMG_TYPE_PNG;
  }

  sfile->frames = atoi(array[4]);

  sfile->bpp = (sfile->img_type == IMG_TYPE_JPEG) ? 24 : 32;

  if (pieces > 4 && array[5]) {
    lives_snprintf(sfile->title, 1024, "%s", lives_strstrip(array[4]));
  }
  if (pieces > 5 && array[6]) {
    lives_snprintf(sfile->author, 1024, "%s", lives_strstrip(array[5]));
  }
  if (pieces > 6 && array[7]) {
    lives_snprintf(sfile->comment, 1024, "%s", lives_strstrip(array[6]));
  }

  lives_strfreev(array);

cleanup:
  sfile->ratio_fps = calc_ratio_fps(sfile->fps, NULL, NULL);
  if (!sfile->ratio_fps) {
    double fps = sfile->fps;
    sfile->fps = lives_fix(sfile->fps, 3);
    if (sfile->fps != fps) sfile->needs_silent_update = TRUE;
    if (!calc_ratio_fps(sfile->pb_fps, NULL, NULL))
      sfile->pb_fps = lives_fix(sfile->pb_fps, 3);
  }

  return TRUE;

rhd_failed:

  stdir = get_staging_dir_for(clipno, ICAP(LOAD));
  if (stdir) {
    char *clip_stdir = lives_build_path(stdir, sfile->handle, NULL);
    if (lives_file_test(clip_stdir, LIVES_FILE_TEST_IS_DIR)) {
      char *clipdir = get_clip_dir(clipno);
      lives_free(stdir);
      //break_me("ready");
      lives_cp_noclobber(clip_stdir, clipdir);
      lives_rmdir(clip_stdir, TRUE);
      lives_free(stdir);
      lives_free(clip_stdir);
      lives_free(clipdir);
      goto frombak;
    }
    lives_free(stdir);
  }

  // header file (lives_header) missing or damaged -- see if we can recover ///////////////
  if (clipno == mainw->current_file) {
    char *clipdir = get_clip_dir(clipno);
    char *hdrback = lives_strdup_printf("%s.%s", lives_header, LIVES_FILE_EXT_BAK);
    char *binfmt = NULL, *gzbinfmt;

    if (!lives_file_test(hdrback, LIVES_FILE_TEST_EXISTS)) {
      lives_free(hdrback);
      hdrback = lives_strdup_printf("%s.%s", lives_header, LIVES_FILE_EXT_NEW);
      if (!lives_file_test(hdrback, LIVES_FILE_TEST_EXISTS)) {
        lives_free(hdrback);
        hdrback = NULL;
        binfmt = lives_build_filename(clipdir, "." TOTALSAVE_NAME, NULL);
        gzbinfmt = lives_build_filename(clipdir, "." TOTALSAVE_NAME "." LIVES_FILE_EXT_GZIP, NULL);
        if (lives_file_test(gzbinfmt, LIVES_FILE_TEST_EXISTS)) {
          lives_free(binfmt);
          binfmt = gzbinfmt;
        } else {
          lives_free(gzbinfmt);
          if (!lives_file_test(binfmt, LIVES_FILE_TEST_EXISTS)) {
            char *binfmt2 = lives_build_filename(clipdir, TOTALSAVE_NAME, NULL);
            if (lives_file_test(binfmt2, LIVES_FILE_TEST_EXISTS)) {
              lives_mv(binfmt2, binfmt);
              lives_free(binfmt);
              binfmt = binfmt2;
            } else {
              lives_free(binfmt2); lives_free(binfmt);
              binfmt = NULL;
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*

    if (hdrback && bak_tried) {
      lives_free(hdrback);
      hdrback = NULL;
    }
    if (binfmt && binfmt_tried) {
      lives_free(binfmt);
      binfmt = NULL;
    }

    if (!hdrback && !is_ascrap && !ahdr_tried) {
      char *alives_header = lives_build_filename(clipdir, LIVES_ACLIP_HEADER, NULL);
      ahdr_tried = TRUE;
      if (lives_file_test(alives_header, LIVES_FILE_TEST_EXISTS)) {
        lives_mv(alives_header, lives_header);
        lives_free(alives_header);
      }
      lives_free(clipdir);
      goto frombak;
    }

    lives_free(clipdir);

    if (hdrback || binfmt) {
      if (hdrback) bak_tried = TRUE;
      else {
        binfmt_tried = TRUE;
      }
      if (recover_details(clipno, lives_header, hdrback, binfmt)) {
        if (hdrback) {
          lives_free(hdrback);
          if (binfmt) lives_free(binfmt);
          goto frombak;
        }
        lives_free(binfmt);
        save_clip_values(clipno);
        return TRUE;
      }
    }

    do_delete_or_mark(clipno, 0);

    // set this to force updating of the set details (e.g. order file)
    // if it is the last clip in the set, we can also mark the set as .ignore
    if (sfile->was_in_set) mainw->invalid_clips = TRUE;
  }

  ////////////////////////////////////

  lives_freep((void **)&lives_header);
  lives_freep((void **)&old_hdrfile);

  return FALSE;
}


LIVES_GLOBAL_INLINE char *get_clip_dir(int clipno) {
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
  if (sfile) {
    if (IS_NORMAL_CLIP(clipno) || sfile->clip_type == CLIP_TYPE_TEMP
        || sfile->clip_type == CLIP_TYPE_VIDEODEV) {
      if (*sfile->staging_dir)
        return lives_build_path(sfile->staging_dir, sfile->handle, NULL);
    }
    return lives_build_path(prefs->workdir, sfile->handle, NULL);
  }
  return NULL;
}


void migrate_from_staging(int clipno) {
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
  if (IS_NORMAL_CLIP(clipno) || IS_TEMP_CLIP(clipno)) {
    if (*sfile->staging_dir) {
      char *old_clipdir, *new_clipdir, *stfile;
      old_clipdir = get_clip_dir(clipno);
      if (sfile->achans && get_primary_src(clipno)) wait_for_bg_audio_sync(clipno);
      pthread_mutex_lock(&sfile->transform_mutex);
      *sfile->staging_dir = 0;
      new_clipdir = get_clip_dir(clipno);
      lives_cp_noclobber(old_clipdir, new_clipdir);
      if (sfile->achans && get_primary_src(clipno)) wait_for_bg_audio_sync(clipno);
      lives_rmdir(old_clipdir, TRUE);
      lives_free(old_clipdir);
      stfile = lives_build_filename(new_clipdir, LIVES_STATUS_FILE_NAME, NULL);
      lives_free(new_clipdir);
      lives_snprintf(sfile->info_file, PATH_MAX, "%s", stfile);
      lives_free(stfile);
      pthread_mutex_unlock(&sfile->transform_mutex);
    } else {
      char *new_clipdir = get_clip_dir(clipno);
      if (lives_file_test(new_clipdir, LIVES_FILE_TEST_EXISTS)) {
        lives_make_writeable_dir(new_clipdir);
      }
      lives_free(new_clipdir);
    }
  }
}


void permit_close(int clipno) {
  // as a safety feature we create a special file which allows the back end to delete the directory
  if (IS_VALID_CLIP(clipno)) {
    lives_cancel_t cancelled = (lives_cancel_t)mainw->cancelled;
    char *clipdir = get_clip_dir(clipno);
    char *permitname = lives_build_filename(clipdir, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
    lives_free(clipdir);
#ifdef IS_MINGW
    // kill any active processes: for other OSes the backend does this
    lives_kill_subprocesses(mainw->files[clipno]->handle, TRUE);
#endif
    lives_touch(permitname);
    lives_free(permitname);
    mainw->cancelled = cancelled;
  }
}


void init_clipboard(void) {
  int current_file = mainw->current_file;
  char *com;

  if (!clipboard) {
    // here is where we create the clipboard
    // use get_new_handle(clipnumber,name);
    if (!get_new_handle(CLIPBOARD_FILE, "clipboard")) {
      mainw->error = TRUE;
      return;
    }
    migrate_from_staging(CLIPBOARD_FILE);
  } else {
    // experimental feature - we can have duplicate copies of the clipboard with different palettes / gamma
    for (int i = 0; i < mainw->ncbstores; i++) {
      if (mainw->cbstores[i] != clipboard) {
        char *clipd = lives_build_path(prefs->workdir, mainw->cbstores[i]->handle, NULL);
        if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
          char *com = lives_strdup_printf("%s close \"%s\"", prefs->backend, mainw->cbstores[i]->handle);
          char *permitname = lives_build_path(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
          lives_touch(permitname);
          lives_free(permitname);
          lives_system(com, TRUE);
          lives_free(com);
        }
        lives_free(clipd);
      }
    }
    mainw->ncbstores = 0;

    if (clipboard->frames > 0) {
      // clear old clipboard
      // need to set current file to 0 before monitoring progress
      mainw->current_file = CLIPBOARD_FILE;
      cfile->cb_src = current_file;

      if (cfile->clip_type == CLIP_TYPE_FILE) {
        lives_freep((void **)&cfile->frame_index);
        srcgrps_free_all(CLIPBOARD_FILE);
        cfile->clip_type = CLIP_TYPE_DISK;
      }

      com = lives_strdup_printf("%s clear_clipboard \"%s\"", prefs->backend, clipboard->handle);
      lives_rm(clipboard->info_file);
      lives_system(com, FALSE);
      lives_free(com);

      if (THREADVAR(com_failed)) {
        mainw->current_file = current_file;
        return;
      }

      cfile->progress_start = cfile->start;
      cfile->progress_end = cfile->end;
      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE, FALSE, _("Clearing the clipboard"));
    }
  }
  mainw->current_file = current_file;
  *clipboard->file_name = 0;
  clipboard->img_type = IMG_TYPE_BEST; // override the pref
  clipboard->cb_src = current_file;
}


///// undo / redo

void set_undoable(const char *what, boolean sensitive) {
  if (mainw->current_file > -1) {
    cfile->redoable = FALSE;
    cfile->undoable = sensitive;
    if (!what || *what) {
      if (what) {
        char *what_safe = lives_strdelimit(lives_strdup(what), "_", ' ');
        lives_snprintf(cfile->undo_text, 32, _("_Undo %s"), what_safe);
        lives_snprintf(cfile->redo_text, 32, _("_Redo %s"), what_safe);
        lives_free(what_safe);
      } else {
        cfile->undoable = FALSE;
        cfile->undo_action = UNDO_NONE;
        lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
        lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));
      }
      lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
      lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);
    }
  }

  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo, sensitive);

#ifdef PRODUCE_LOG
  lives_log(what);
#endif
}


void set_redoable(const char *what, boolean sensitive) {
  if (mainw->current_file > -1) {
    cfile->undoable = FALSE;
    cfile->redoable = sensitive;
    if (!what || *what) {
      if (what) {
        char *what_safe = lives_strdelimit(lives_strdup(what), "_", ' ');
        lives_snprintf(cfile->undo_text, 32, _("_Undo %s"), what_safe);
        lives_snprintf(cfile->redo_text, 32, _("_Redo %s"), what_safe);
        lives_free(what_safe);
      } else {
        cfile->redoable = FALSE;
        cfile->undo_action = UNDO_NONE;
        lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
        lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));
      }
      lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
      lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);
    }
  }
  lives_widget_hide(mainw->undo);
  lives_widget_show(mainw->redo);
  lives_widget_set_sensitive(mainw->redo, sensitive);
}


#define BINFMT_CHECK_CHARS "LiVESXXX"

/**
   @brief set default values for a clip (in memory)

   if new_file == -1 we create (malloc) a new clip and switch to it
   - setting its handle to "handle" (reload / crash recovery)

   if new_file != -1 the parameter "handle" is ignored, and we switch to new_file, without mallocing anything
   - "handle" in the clip must have been set already (called from get_new_handle() and get_temp_handle())
   -- get_new_handle() will set name and fliename and switch back to original clip.

   default values are then set for the clip
   - a "unique_id" is assigned via uuidgen or lives_random()
   - type is set to CLIP_TYPE_DISK
   - img_type is set depending on prefs->image_type
   - frames is set to 0
   etc.

   - loaded is set = to is_loaded

   WARNING: on success, returns the clip, and changes the value of
   mainw->current_file !!  returns NULL if: new_file is out of range
   or points to a NULL clip; new_file is -1 and all free clips are
   in use (unlikely), or malloc fails.
*/
lives_clip_t *create_cfile(int new_file, const char *handle, boolean is_loaded) {
  pthread_mutexattr_t mattr;
  lives_clip_t *sfile;
  char *stfile, *clipdir;

  if (new_file == -1) {
    // if new_file == -1, we are going to create a new clip
    new_file = mainw->first_free_file;
    if (new_file == -1) {
      too_many_files();
      return NULL;
    }

    mainw->current_file = new_file;
    get_next_free_file();

    if (new_file < 0 || new_file > MAX_FILES || IS_VALID_CLIP(new_file)) {
      char *msg = lives_strdup_printf("Attempt to create invalid new clip %d\n", new_file);
      LIVES_WARN(msg);
      lives_free(msg);
      return NULL;
    }

    if (!handle) {
      // if handle is NULL, we create a new clip on disk, switch to it
      // (unused)
      if (!get_handle_from_info_file(new_file)) return NULL;
      sfile = mainw->files[new_file];
    } else {
      // else just create the in-memory part and set the handle
      sfile = mainw->files[new_file] = (lives_clip_t *)(lives_calloc(1, sizeof(lives_clip_t)));
      if (!sfile) return NULL;
      lives_snprintf(sfile->handle, 256, "%s", handle);
    }
  }

  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

  mainw->current_file = new_file;

  cfile->is_loaded = is_loaded;

  // any (non-zero) cfile (clip) initialisation goes in here
  lives_memcpy((void *)&cfile->binfmt_check.chars, BINFMT_CHECK_CHARS, 8);
  cfile->binfmt_version.num = make_version_hash(LiVES_VERSION);
  cfile->binfmt_bytes.size = (size_t)(offsetof(lives_clip_t, binfmt_end));
  lives_snprintf(cfile->type, 40, "%s", _("Unknown"));
  cfile->adirection = LIVES_DIRECTION_FORWARD;
  cfile->fps_scale = 1.;
  cfile->aplay_fd = -1;
  cfile->fps = cfile->pb_fps = prefs->default_fps;
  cfile->is_untitled = TRUE;
  cfile->undo_action = UNDO_NONE;
  cfile->delivery = LIVES_DELIVERY_PULL;
  cfile->frameno = cfile->last_frameno = cfile->saved_frameno = 1;

  pthread_mutex_init(&cfile->srcgrp_mutex, NULL);

  cfile->clip_type = CLIP_TYPE_DISK;
  cfile->unique_id = gen_unique_id();
  pthread_mutex_init(&cfile->frame_index_mutex, &mattr);
  cfile->stored_layout_idx = -1;
  cfile->interlace = LIVES_INTERLACE_NONE;
  cfile->cb_src = -1;
  cfile->gamma_type = WEED_GAMMA_SRGB;
  cfile->has_binfmt = TRUE;

  pthread_mutex_init(&cfile->transform_mutex, NULL);

  cfile->img_type = lives_image_ext_to_img_type(prefs->image_ext);

  cfile->bpp = (cfile->img_type == IMG_TYPE_JPEG) ? 24 : 32;

  cfile->header_version = LIVES_CLIP_HEADER_VERSION;

  cfile->vol = 1.;

  cfile->signed_endian = AFORM_UNKNOWN;
  lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
  lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));

  clipdir = get_clip_dir(mainw->current_file);
  stfile = lives_build_filename(clipdir, LIVES_STATUS_FILE_NAME, NULL);
  lives_free(clipdir);

  lives_snprintf(cfile->info_file, PATH_MAX, "%s", stfile);
  lives_free(stfile);

  return cfile;
}


LIVES_GLOBAL_INLINE char *get_untitled_name(int number) {
  // utility function to get clip name
  return lives_strdup_printf(_("Untitled%d"), number);
}


int create_nullvideo_clip(const char *handle) {
  // create a file with no video, just produces blank frames
  // may be used to playback with audio, for testign etc.
  int new_file;
  int current_file = mainw->current_file;
  create_cfile(-1, handle, TRUE);
  new_file = mainw->current_file;
  mainw->current_file = current_file;
  mainw->files[new_file]->clip_type = CLIP_TYPE_NULL_VIDEO;
  return new_file;
}


boolean check_for_ratio_fps(double fps) {
  boolean ratio_fps;
  char *test_fps_string1 = lives_strdup_printf("%.3f00", fps);
  char *test_fps_string2 = lives_strdup_printf("%.5f", fps);

  if (strcmp(test_fps_string1, test_fps_string2)) {
    // got a ratio
    ratio_fps = TRUE;
  } else {
    ratio_fps = FALSE;
  }
  lives_free(test_fps_string1);
  lives_free(test_fps_string2);

  return ratio_fps;
}


double get_ratio_fps(const char *string) {
  // return a ratio (8dp) fps from a string with format num:denom
  // inverse of calc_ratio_fps
  double fps;
  char *fps_string;
  char **array = lives_strsplit(string, ":", 2);
  int num = atoi(array[0]);
  int denom = atoi(array[1]);
  lives_strfreev(array);
  fps = (double)num / (double)denom;
  fps_string = lives_strdup_printf("%.8f", fps);
  fps = lives_strtod(fps_string);
  lives_free(fps_string);
  return fps;
}

////

int find_next_clip(int index, int old_file) {
  // when a clip becomes invalid, find another clip to switch to
  if (IS_VALID_CLIP(index)) {
    if (mainw->multitrack) {
      if (old_file != mainw->multitrack->render_file) {
        mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
        mt_clip_select(mainw->multitrack, TRUE);
      }
      return mainw->current_file;
    }
    if (!mainw->noswitch) {
      switch_clip(1, index, TRUE);
      if (mainw->blend_file == old_file)
        switch_clip(SCREEN_AREA_BACKGROUND, mainw->current_file, FALSE);
      d_print("");
    }
    return index;
  }

  if (mainw->clips_available > 0) {
    for (int i = mainw->current_file; --i;) {
      if (mainw->files[i]) {
        if (!mainw->noswitch) {
          switch_clip(1, i, TRUE);
          if (mainw->blend_file == old_file)
            switch_clip(SCREEN_AREA_BACKGROUND, mainw->current_file, FALSE);
          d_print("");
        }
        return i;
      }
    }

    for (int i = 1; i < MAX_FILES; i++) {
      if (mainw->files[i]) {
        if (!mainw->noswitch) {
          switch_clip(1, i, TRUE);
          if (mainw->blend_file == old_file)
            switch_clip(SCREEN_AREA_BACKGROUND, mainw->current_file, FALSE);
          d_print("");
        }
        return i;
      }
    }
  }
  return -1;
}


void switch_to_file(int old_file, int new_file) {
  // this function is used for full clip switching (during non-playback or non fs)

  // calling this function directly is now deprecated in favour of switch_clip()

  int orig_file = mainw->current_file;

  // should use close_current_file
  if (!IS_VALID_CLIP(new_file)) {
    char *msg = lives_strdup_printf("attempt to switch to invalid clip %d", new_file);
    LIVES_WARN(msg);
    lives_free(msg);
    return;
  }

  if (!LIVES_IS_PLAYING) {
    if (old_file != new_file) {
      if (CURRENT_CLIP_IS_VALID) {
        mainw->laudio_drawable = cfile->laudio_drawable;
        mainw->raudio_drawable = cfile->raudio_drawable;
        mainw->drawsrc = mainw->current_file;
      }
    }
  }

  if (mainw->multitrack) return;

  if (mainw->noswitch) {
    mainw->new_clip = new_file;
    return;
  }

  mainw->current_file = new_file;

  if (old_file != new_file) {
    if (CURRENT_CLIP_IS_VALID) {
      mainw->laudio_drawable = cfile->laudio_drawable;
      mainw->raudio_drawable = cfile->raudio_drawable;
      mainw->drawsrc = mainw->current_file;
    }
    if (old_file != 0 && new_file != 0) mainw->preview_frame = 0;
    lives_widget_set_sensitive(mainw->select_new, (cfile->insert_start > 0));
    lives_widget_set_sensitive(mainw->select_last, (cfile->undo_start > 0));
    if ((cfile->start == 1 || cfile->end == cfile->frames) && !(cfile->start == 1 && cfile->end == cfile->frames)) {
      lives_widget_set_sensitive(mainw->select_invert, TRUE);
    } else {
      lives_widget_set_sensitive(mainw->select_invert, FALSE);
    }
    if (IS_VALID_CLIP(old_file) && mainw->files[old_file]->opening) {
      // switch while opening - come out of processing dialog
      if (mainw->proc_ptr) {
        lives_widget_destroy(mainw->proc_ptr->processing);
        lives_freep((void **)&mainw->proc_ptr->text);
        lives_freep((void **)&mainw->proc_ptr);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

  if (mainw->play_window && cfile->is_loaded && orig_file != new_file) {
    // if the clip is loaded
    if (!mainw->preview_box) {
      // create the preview box that shows frames...
      make_preview_box();
    }
    // add it the play window...
    if (!lives_widget_get_parent(mainw->preview_box)) {
      lives_widget_queue_draw(mainw->play_window);
      lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
    }

    if (mainw->play_window) { // must keep checking this, as it can be switched of by user
      // and resize it
      lives_widget_grab_focus(mainw->preview_spinbutton);
      resize_play_window();
    }
    //
    if (mainw->play_window) {
      load_preview_image(FALSE);
    }
  }

  if (mainw->play_window) { // must keep checking this, as it can be switched of by user
    lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
    lives_widget_show_all(mainw->preview_box);
    lives_widget_show_now(mainw->preview_box);
    lives_widget_set_no_show_all(mainw->preview_controls, TRUE);
  }

  if (!mainw->go_away && CURRENT_CLIP_IS_NORMAL) {
    mainw->no_context_update = TRUE;
    reget_afilesize(mainw->current_file);
    mainw->no_context_update = FALSE;
  }

  if (!CURRENT_CLIP_IS_VALID) return;
  //chill_decoder_plugin(mainw->current_file);

  if (!CURRENT_CLIP_IS_NORMAL || cfile->opening) {
    lives_widget_set_sensitive(mainw->rename, FALSE);
  }

  if (cfile->menuentry) {
    reset_clipmenu();
  }

  lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
  lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);

  set_sel_label(mainw->sel_label);

  if (mainw->eventbox5) lives_widget_show(mainw->eventbox5);
  lives_widget_show(mainw->hruler);
  lives_widget_show(mainw->vidbar);
  lives_widget_show(mainw->laudbar);

  if (cfile->achans < 2) {
    lives_widget_hide(mainw->raudbar);
  } else {
    lives_widget_show(mainw->raudbar);
  }

  if (cfile->redoable) {
    lives_widget_show(mainw->redo);
    lives_widget_hide(mainw->undo);
  } else {
    lives_widget_hide(mainw->redo);
    lives_widget_show(mainw->undo);
  }

  if (new_file > 0) {
    if (cfile->menuentry) {
      set_main_title(cfile->name, 0);
    } else set_main_title(cfile->file_name, 0);
  }

  set_start_end_spins(mainw->current_file);

  resize(1);

  if (!mainw->go_away) get_play_times();

  // if the file was opening, continue...
  if (cfile->opening) {
    open_file(cfile->file_name);
  } else {
    showclipimgs();
    lives_ce_update_timeline(0, cfile->pointer_time);
    mainw->ptrtime = cfile->pointer_time;
    lives_widget_queue_draw(mainw->eventbox2);
  }

  if (mainw->is_ready) {
    if (!mainw->multitrack && !mainw->reconfig) {
      if (prefs->show_msg_area && !mainw->only_close) {
        reset_message_area(); // necessary
        if (mainw->idlemax == 0) {
          lives_idle_add(resize_message_area, NULL);
        }
        mainw->idlemax = DEF_IDLE_MAX;
      }
      redraw_timeline(mainw->current_file);
    }
  }
}


boolean switch_audio_clip(int new_file, boolean activate) {
  //ticks_t cticks;
  lives_clip_t *sfile;
  int aplay_file;

  if (AUD_SRC_EXTERNAL) return FALSE;

  aplay_file = get_aplay_clipno();

  if ((aplay_file == new_file && (!(prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
                                  || !CLIP_HAS_AUDIO(aplay_file)))
      || (aplay_file != new_file && !(prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS))) {
    return FALSE;
  }

  sfile = RETURN_VALID_CLIP(new_file);
  if (!sfile) return FALSE;

  if (new_file == aplay_file) {
    if (sfile->pb_fps > 0. || (sfile->play_paused && sfile->freeze_fps > 0.))
      sfile->adirection = LIVES_DIRECTION_FORWARD;
    else sfile->adirection = LIVES_DIRECTION_REVERSE;
  } else if (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP) {
    mainw->scratch = SCRATCH_JUMP;
  }

  IF_APLAYER_JACK
  (
    if (!activate) mainw->jackd->in_use = FALSE;

  if (new_file != aplay_file) {
    if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
        mainw->cancelled = handle_audio_timeout();
        return FALSE;
      }
      if (mainw->jackd->playing_file > 0) {
        if (!CLIP_HAS_AUDIO(new_file)) {
          jack_get_rec_avals(mainw->jackd);
          mainw->rec_avel = 0.;
        }
        jack_message.command = ASERVER_CMD_FILE_CLOSE;
        jack_message.data = NULL;
        jack_message.next = NULL;
        mainw->jackd->msgq = &jack_message;

        if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
          mainw->cancelled = handle_audio_timeout();
          return FALSE;
        }
      }
    }

  if (!IS_VALID_CLIP(new_file)) {
  mainw->jackd->in_use = FALSE;
  return FALSE;
}

if (new_file == aplay_file) return TRUE;

  if (CLIP_HAS_AUDIO(new_file) && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
    // tell jack server to open audio file and start playing it

    jack_message.command = ASERVER_CMD_FILE_OPEN;

    jack_message.data = lives_strdup_printf("%d", new_file);

      jack_message2.command = ASERVER_CMD_FILE_SEEK;

      jack_message.next = &jack_message2;
      jack_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
      jack_message2.next = NULL;

      mainw->jackd->msgq = &jack_message;

      mainw->jackd->in_use = TRUE;

      if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
        mainw->cancelled = handle_audio_timeout();
        return FALSE;
      }

      //jack_time_reset(mainw->jackd, 0);

      mainw->jackd->is_paused = sfile->play_paused;
      mainw->jackd->is_silent = FALSE;
    } else {
      mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    })

  IF_APLAYER_PULSE
  (
    if (!activate) mainw->pulsed->in_use = FALSE;

  if (new_file != aplay_file) {
    if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
        mainw->cancelled = handle_audio_timeout();
        return FALSE;
      }

      if (IS_PHYSICAL_CLIP(aplay_file)) {
        lives_clip_t *afile = mainw->files[aplay_file];
        if (!CLIP_HAS_AUDIO(new_file)) {
          pulse_get_rec_avals(mainw->pulsed);
          mainw->rec_avel = 0.;
        }
        pulse_message.command = ASERVER_CMD_FILE_CLOSE;
        pulse_message.data = NULL;
        pulse_message.next = NULL;
        mainw->pulsed->msgq = &pulse_message;

        if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
          mainw->cancelled = handle_audio_timeout();
          return FALSE;
        }

        afile->sync_delta = lives_pulse_get_time(mainw->pulsed) - mainw->startticks;
        afile->aseek_pos = mainw->pulsed->seek_pos;
      }

      if (!IS_VALID_CLIP(new_file)) {
        mainw->pulsed->in_use = FALSE;
        return FALSE;
      }

      if (new_file == aplay_file) return TRUE;

      if (CLIP_HAS_AUDIO(new_file) && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
        // tell pulse server to open audio file and start playing it

        mainw->video_seek_ready = FALSE;

        // have to set this, else the msgs are not acted on
        pulse_driver_uncork(mainw->pulsed);

        pulse_message.command = ASERVER_CMD_FILE_OPEN;
        pulse_message.data = lives_strdup_printf("%d", new_file);

        pulse_message2.command = ASERVER_CMD_FILE_SEEK;
        /* if (LIVES_IS_PLAYING && !mainw->preview) { */
        /*   pulse_message2.tc = cticks; */
        /*   pulse_message2.command = ASERVER_CMD_FILE_SEEK_ADJUST; */
        /* } */
        pulse_message.next = &pulse_message2;
        pulse_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
        pulse_message2.next = NULL;

        mainw->pulsed->msgq = &pulse_message;

        mainw->pulsed->is_paused = sfile->play_paused;

        //pa_time_reset(mainw->pulsed, 0);
      } else {
        mainw->video_seek_ready = TRUE;
        video_sync_ready();
      }
    })

#if 0
  if (prefs->audio_player == AUD_PLAYER_NONE) {
    if (!IS_VALID_CLIP(new_file)) {
      mainw->nullaudio_playing_file = -1;
      return FALSE;
    }
    if (mainw->nullaudio->playing_file == new_file) return FALSE;
    nullaudio_clip_set(new_file);
    if (activate && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
      if (!sfile->play_paused)
        nullaudio_arate_set(sfile->arate * sfile->pb_fps / sfile->fps);
      else nullaudio_arate_set(sfile->arate * sfile->freeze_fps / sfile->fps);
    } else nullaudio_arate_set(sfile->arate);
    nullaudio_seek_set(sfile->aseek_pos);
    if (CLIP_HAS_AUDIO(new_file)) {
      nullaudio_get_rec_avals();
    } else {
      nullaudio_get_rec_avals();
      mainw->rec_avel = 0.;
    }
  }
#endif

  return TRUE;
}


void do_quick_switch(int new_file) {
  // handle clip switching during playback
  // calling this function directly is now deprecated in favour of switch_clip()
  // the only place it should be called directly is from the player
  //
  // when swapping fg and bg clips, we get first a new fg_clip (the old bg clip)
  // then a new bg clip (the old fg) clip
  // thus when changeing to / from a generator for exemple, we need to check whether
  // it swaps to the other track or not
  lives_clip_t *sfile = NULL;
  boolean osc_block;
  int old_file = mainw->playing_file;

  if (!LIVES_IS_PLAYING) {
    switch_to_file(mainw->current_file, new_file);
    return;
  }

  if (old_file == new_file || old_file < 1 || !IS_VALID_CLIP(new_file)) return;

  if (mainw->multitrack
      || (mainw->record && !mainw->record_paused && !(prefs->rec_opts & REC_CLIPS)) ||
      mainw->foreign || (mainw->preview && !mainw->is_rendering)) return;

  if (mainw->noswitch && !mainw->can_switch_clips) {
    mainw->new_clip = new_file;
    return;
  }

  if (new_file == mainw->blend_file && mainw->new_blend_file == mainw->blend_file
      && IS_VALID_CLIP(old_file))
    mainw->new_blend_file = old_file;

  sfile = RETURN_VALID_CLIP(old_file);

  mainw->whentostop = NEVER_STOP;

  if (old_file != new_file && old_file == mainw->playing_file) {
    weed_layer_t *old_frame_layer = get_old_frame_layer();
    if (old_frame_layer) weed_layer_set_invalid(old_frame_layer, TRUE);
    if (mainw->cached_frame) {
      weed_layer_set_invalid(mainw->cached_frame, TRUE);
      if (mainw->cached_frame != mainw->frame_layer
          && mainw->cached_frame != get_old_frame_layer()) {
        if (weed_layer_get_pixel_data(mainw->cached_frame)
            == weed_layer_get_pixel_data(mainw->frame_layer))
          weed_layer_nullify_pixel_data(mainw->cached_frame);
        weed_layer_free(mainw->cached_frame);
        mainw->cached_frame = NULL;
      }
    }

    if (mainw->frame_layer) weed_layer_set_invalid(mainw->frame_layer, TRUE);

    if (mainw->frame_layer_preload) {
      // TODO - if we are preloading, find the thread loading the frame and cancel it
      // then once cancelled, remove + free the srcgroup, we can do this async however
      if (mainw->frame_layer_preload != mainw->frame_layer) {
        weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);

        // TODO - avoid waiting, cancel thread, set layer invalid
        // free srcgrp
        weed_layer_unref(mainw->frame_layer_preload);
        mainw->frame_layer_preload = NULL;
        mainw->pred_clip = -1;
        mainw->pred_frame = 0;
      }
    }
  }

  // TODO - do not invalidate the layers yet. Instead, update the clip_index and build a new nodemodel
  // but do not update the plan. Then if the layer metadata does not change, simply relabel the layer
  // otherwise check layer status. If it has none have yet reached LOADED,
  // pause the loader threads, update metadata
  // in the layers, relabel them, then resume the threads.
  // Otherwise, one or more layers are already  being converted,
  // - we should then run the this cycle with the current plan, allowing the layers to be converted
  // according to the old plane, and only update the plan from the new nodemodel
  // after this, for the following cycle.

  if (mainw->blend_layer) {
    // TODO - if swapping to fg,
    // and status has not yet reached LOADED, we can now pause the thread,
    // set new metadata (size, palette) for the layer, make it frame_layer,
    // invalidate blend_layer, then resume the thread
    weed_layer_set_invalid(mainw->blend_layer, TRUE);
  }

  if (old_file != mainw->blend_file && !mainw->is_rendering) {
    if (sfile && sfile->clip_type == CLIP_TYPE_GENERATOR && get_primary_src(old_file)) {
      if (new_file != mainw->blend_file) {
        // switched from generator to another clip, end the generator
        // if we dont don this here, then this would happen on the next call to map_sources_to_tracks
        // ie when nodemodel is rebuilt
        remove_primary_src(mainw->blend_file, LIVES_SRC_TYPE_GENERATOR);
      } else {
        // swap fg / bg gen keys/modes
        rte_swap_fg_bg();
      }
    }

    if (new_file == mainw->blend_file
        && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR
        && get_primary_src(mainw->blend_file)) {
      // swap fg / bg gen keys/modes
      rte_swap_fg_bg();
    }
  }

  osc_block = mainw->osc_block;
  mainw->osc_block = TRUE;

  if (mainw->loop_locked) {
    unlock_loop_lock();
  }

  // this sets some minor features such as prevenitng render to same clip
  mainw->clip_switched = TRUE;

  // if we switch clips then we break out of selection play
  mainw->playing_sel = FALSE;
  find_when_to_stop();

  // HERE we do the actual switchover
  mainw->drawsrc = mainw->playing_file = mainw->current_file = new_file;
  ///

  mainw->laudio_drawable = cfile->laudio_drawable;
  mainw->raudio_drawable = cfile->raudio_drawable;

  lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), mainw->files[new_file]->pb_fps);
  lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps));
  lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);

  // switch audio clip
  if (AUD_SRC_INTERNAL && (!mainw->event_list || mainw->record)) {
    if (APLAYER_REALTIME && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
        && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS)
        && !mainw->is_rendering && (mainw->preview || !(mainw->agen_key != 0 || mainw->agen_needs_reinit))) {
      switch_audio_clip(new_file, TRUE);
    }
  }

  set_main_title(cfile->name, 0);

  if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_FOREGROUND) ce_thumbs_highlight_current_clip();

  if (CURRENT_CLIP_IS_NORMAL) {
    char *tmp;
    tmp = lives_build_filename(prefs->workdir, cfile->handle, LIVES_STATUS_FILE_NAME, NULL);
    lives_snprintf(cfile->info_file, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  if (!CURRENT_CLIP_IS_NORMAL || (mainw->event_list && !mainw->record))
    mainw->play_end = INT_MAX;

  // act like we are not playing a selection (but we will try to keep to
  // selection bounds)
  mainw->playing_sel = FALSE;
  find_when_to_stop();

  if (CURRENT_CLIP_IS_NORMAL) {
    if (cfile->last_play_sequence != mainw->play_sequence) {
      cfile->frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
    }
  }

  changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), LIVES_INT_TO_POINTER(1));

  cfile->next_event = NULL;

#if GTK_CHECK_VERSION(3, 0, 0)
  if (LIVES_IS_PLAYING && !mainw->play_window
      && (!CURRENT_CLIP_IS_VALID
          || (sfile && (cfile->hsize != sfile->hsize
                        || cfile->vsize != sfile->vsize)))) {
    clear_widget_bg(mainw->play_image, mainw->play_surface);
  }
#endif

  if (CURRENT_CLIP_HAS_VIDEO) {
    if (!mainw->fs && !mainw->faded) {
      set_start_end_spins(mainw->current_file);
      if (!mainw->play_window && mainw->double_size) {
        //frame_size_update();
        resize(2.);
      } else resize(1);
    }
  } else resize(1);

  if (!mainw->fs && !mainw->faded) showclipimgs();

  mainw->osc_block = osc_block;
  lives_ruler_set_upper(LIVES_RULER(mainw->hruler), CURRENT_CLIP_TOTAL_TIME);

  mainw->ignore_screen_size = TRUE;
  reset_mainwin_size();
  mainw->ignore_screen_size = FALSE;

  if (!mainw->fs && !mainw->fade) redraw_timeline(mainw->current_file);
}


void switch_clip(int type, int newclip, boolean force) {
  // generic switch clip callback

  // This is the new single entry function for switching clips.
  // It should eventually replace switch_to_file() and do_quick_switch()

  // type = auto : if we are playing and a transition is active, this will change the background clip
  // type = foreground fg only
  // type = background bg only

  if (!IS_VALID_CLIP(newclip)) return;
  if (!LIVES_IS_PLAYING && !force && newclip == mainw->current_file) return;

  if (mainw->current_file < 1 || mainw->multitrack || mainw->preview || mainw->internal_messaging ||
      (mainw->is_processing && cfile && cfile->is_loaded) || !mainw->cliplist) return;

  if (type == SCREEN_AREA_BACKGROUND || (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND && mainw->playing_file > 0
                                         && type != SCREEN_AREA_FOREGROUND
                                         && !(mainw->blend_file != -1 && !IS_NORMAL_CLIP(mainw->blend_file)
                                             && mainw->blend_file != mainw->playing_file))) {
    if (mainw->num_tr_applied < 1 || (newclip == mainw->blend_file && !prefs->tr_self)) return;

    // switch bg clip
    if (LIVES_IS_PLAYING) {
      mainw->new_blend_file = newclip;
      weed_layer_set_invalid(mainw->blend_layer, TRUE);
    } else {
      if (IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != mainw->playing_file
          && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
        weed_instance_t *inst = (weed_instance_t *)((get_primary_src(mainw->blend_file))->actor);
        if (mainw->blend_layer) {
          weed_layer_unref(mainw->blend_layer);
          mainw->blend_layer = NULL;
        }
        if (inst) {
          if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) {
            int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
            rte_key_on_off(key + 1, FALSE);
          }
        }
        //chill_decoder_plugin(mainw->blend_file);
      }

      track_source_free(1, mainw->blend_file);

      mainw->blend_file = newclip;
      mainw->whentostop = NEVER_STOP;
      if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) {
        ce_thumbs_highlight_current_clip();
      }
      mainw->blend_palette = WEED_PALETTE_END;
    }
    return;
  }

  // switch fg clip

  if (LIVES_IS_PLAYING) {
    mainw->new_clip = newclip;
  } else {
    int current_file = mainw->current_file;
    if (cfile && !cfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
    if (!CURRENT_CLIP_IS_VALID || (force && newclip == mainw->current_file)) current_file = -1;
    switch_to_file(current_file, newclip);
  }
}


///////// clip sources /////////////

// - add a source (actor) to a clipsrc_group
// - get a clip_src from clip
// - remove clip_src from a group
// - swap src between groups
// - free clip_src
// - free srcgrp

// clone srcgrp ??
//

static lives_clipsrc_group_t *_get_srcgrp(lives_clip_t *sfile, int track, int purpose) {
  if (sfile) {
    int nsrcs = sfile->n_src_groups;
    for (int i = 0; i < nsrcs; i++) {
      if (sfile->src_groups[i]) {
        int xpurpose = sfile->src_groups[i]->purpose;
        if ((purpose == SRC_PURPOSE_ANY || xpurpose == purpose
             || (purpose == SRC_PURPOSE_TRACK_OR_PRIMARY && (xpurpose == SRC_PURPOSE_PRIMARY
                 || xpurpose == SRC_PURPOSE_PRIMARY)))
            && (track == -1 || sfile->src_groups[i]->track == track)) {
          return sfile->src_groups[i];
        }
      }
    }
  }
  return NULL;
}


lives_clipsrc_group_t *get_srcgrp(int nclip, int track, int purpose) {
  // find srcgrp in clip nclip with matching track and purpose
  // track can be -1 for non playback groups
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    lives_clipsrc_group_t *srcgrp;
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    srcgrp = _get_srcgrp(sfile, track, purpose);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
    return srcgrp;
  }
  return NULL;
}


lives_clipsrc_group_t *get_primary_srcgrp(int nclip) {
  return get_srcgrp(nclip, -1, SRC_PURPOSE_PRIMARY);
}


static lives_clipsrc_group_t *_add_srcgrp(lives_clip_t *sfile, int track, int purpose) {
  int ngrps = sfile->n_src_groups;
  lives_clipsrc_group_t *srcgrp =
    (lives_clipsrc_group_t *)lives_calloc(1, sizeof(lives_clipsrc_group_t));
  srcgrp->status = SRC_STATUS_IDLE;
  srcgrp->track = track;
  srcgrp->purpose = purpose;

  pthread_mutex_init(&srcgrp->src_mutex, NULL);
  pthread_mutex_init(&srcgrp->refcnt_mutex, NULL);
  srcgrp->refcnt = 1;
  sfile->src_groups = lives_recalloc(sfile->src_groups, ngrps + 1,
                                     ngrps, sizeof(lives_clipsrc_group_t *));
  sfile->src_groups[ngrps] = srcgrp;
  sfile->n_src_groups = ++ngrps;
  return srcgrp;
}


// begin by adding a clip_src to a clip
// this will create a clipsrc_group and add the src inside it
// the srcgrp can then be looked up by purpose and track
// fither clipsrcs can be added to it or removed, or found
// srcgrps can also be removed or cloned
// if a srcgrp is removed, all the clipsrcs inside it will be freed
//


static lives_clip_src_t *_add_src_to_group(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp, void *actor,
    void *actor_inst, int src_type, uint64_t actor_uid,
    fingerprint_t *chksum, const char *ext_URI) {
  int nsrcs;
  lives_clip_src_t *mysrc = (lives_clip_src_t *)lives_calloc(1, sizeof(lives_clip_src_t));

  mysrc->uid = gen_unique_id();
  mysrc->class_uid = gen_unique_id();
  mysrc->actor = actor;
  mysrc->actor_inst = actor_inst;
  mysrc->actor_uid = actor_uid;
  mysrc->src_type = src_type;
  lives_memcpy(&mysrc->ext_checksum, &chksum, sizeof(fingerprint_t));
  if (ext_URI) mysrc->ext_URI = lives_strdup(ext_URI);

  pthread_mutex_lock(&srcgrp->src_mutex);
  nsrcs = srcgrp->n_srcs;
  srcgrp->srcs = lives_recalloc(srcgrp->srcs, nsrcs + 1, nsrcs, sizeof(lives_clip_src_t *));
  for (int i = 0; i < nsrcs; i++) {
    srcgrp->srcs[i + 1] = srcgrp->srcs[i];
  }

  srcgrp->srcs[0] = mysrc;
  srcgrp->n_srcs = ++nsrcs;
  pthread_mutex_unlock(&srcgrp->src_mutex);

  if (src_type == LIVES_SRC_TYPE_BLANK)
    mysrc->action_func = lives_blankframe_srcfunc;

  return mysrc;
}


static lives_clip_src_t *_add_clip_src(lives_clip_t *sfile, int track, int purpose, void *actor,
                                       void *actor_inst, int src_type, uint64_t actor_uid,
                                       fingerprint_t *chksum, const char *ext_URI) {
  lives_clip_src_t *mysrc = NULL;
  if (sfile) {
    if (sfile->clip_type == CLIP_TYPE_DISK && src_type != LIVES_SRC_TYPE_IMAGE) break_me("badsrctyp");
    // find a clipsrc_group with purpose
    // if such does not exist we will clone primary and make one
    // then make a clip_src for source, and insert in src_group
    lives_clipsrc_group_t *srcgrp = _get_srcgrp(sfile, track, purpose);
    if (!srcgrp) srcgrp = _add_srcgrp(sfile, track, purpose);
    if (srcgrp)
      mysrc = _add_src_to_group(sfile, srcgrp, actor, actor_inst, src_type,
                                actor_uid, chksum, ext_URI);
  }
  return mysrc;
}


lives_clip_src_t *add_clip_src(int nclip, int track, int purpose, void *actor, int src_type,
                               uint64_t actor_uid, fingerprint_t *chksum, const char *ext_URI) {
  lives_clip_src_t *mysrc = NULL;
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    mysrc = _add_clip_src(sfile, track, purpose, actor, NULL, src_type, actor_uid, chksum, ext_URI);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
  return mysrc;
}


static lives_clip_src_t *_get_clip_src(lives_clipsrc_group_t *srcgrp, uint64_t actor_uid, void *actor_inst,
                                       int src_type, uint64_t class_uid, const char *ext_URI, fingerprint_t *chksum) {
  // find clpsrc in srcgrp
  // actor_uid can be 0 / non 0
  // src_type can be undefined or defined
  // ext_URI can be NULL or a string
  // chksum may be NULL or a pointer to an indentifier
  if (srcgrp) {
    int nsrcs = srcgrp->n_srcs;
    for (int i = 0; i < nsrcs; i++) {
      lives_clip_src_t *mysrc = srcgrp->srcs[i];
      if (actor_uid && mysrc->actor_uid != actor_uid) continue;
      if (actor_inst && mysrc->actor_inst != actor_inst) continue;
      if (src_type != LIVES_SRC_TYPE_UNDEFINED && mysrc->src_type != src_type) continue;
      if (class_uid && mysrc->class_uid != class_uid) continue;
      if (ext_URI && *ext_URI && (!mysrc->ext_URI || !*mysrc->ext_URI
                                  || lives_strcmp(ext_URI, mysrc->ext_URI)))
        continue;
      return mysrc;
    }
  }
  return NULL;
}


lives_clip_src_t *get_clip_src(lives_clipsrc_group_t *srcgrp, uint64_t actor_uid, int src_type, const char *ext_URI,
                               fingerprint_t *chksum) {
  lives_clip_src_t *mysrc;
  pthread_mutex_lock(&srcgrp->src_mutex);
  mysrc = _get_clip_src(srcgrp, actor_uid, NULL, src_type, 0, ext_URI, chksum);
  pthread_mutex_unlock(&srcgrp->src_mutex);
  return mysrc;
}


lives_clip_src_t *find_src_by_class_uid(lives_clipsrc_group_t *srcgrp, uint64_t class_uid) {
  lives_clip_src_t *mysrc;
  pthread_mutex_lock(&srcgrp->src_mutex);
  mysrc = _get_clip_src(srcgrp, 0, NULL, LIVES_SRC_TYPE_UNDEFINED, class_uid, NULL, NULL);
  pthread_mutex_unlock(&srcgrp->src_mutex);
  return mysrc;
}


// swap srcgrp a (purpose / track) with srcgrp_b
// uses: swap primary_srcgrp with precache; swap primary_srcgrp with track_srcgrp
// srcgrps are swapped in array, but track and purpose do not move
boolean swap_srcgrps(int nclip, int otrack, int opurpose, int ntrack, int npurpose) {
  lives_clip_t *sfile;
  if (opurpose != npurpose) return FALSE;
  if ((sfile = RETURN_VALID_CLIP(nclip))) {
    if (!pthread_mutex_lock(&sfile->srcgrp_mutex)) {
      lives_clipsrc_group_t *srcgrp_a = NULL, *srcgrp_b = NULL;
      int nsrcgrps = sfile->n_src_groups;
      int ai = -1, bi = -1;
      for (int i = 0; i < nsrcgrps; i++) {
        lives_clipsrc_group_t *srcgrp = sfile->src_groups[i];
        if (srcgrp->purpose == opurpose && (otrack == -1 || srcgrp->track == otrack)) {
          ai = i;
          srcgrp_a = srcgrp;
        } else if (srcgrp->purpose == npurpose && (ntrack == -1 || srcgrp->track == ntrack)) {
          srcgrp_b = srcgrp;
          bi = i;
        }
        if (srcgrp_a && srcgrp_b) {
          srcgrp_a->track = ntrack;
          srcgrp_b->track = otrack;
          srcgrp_a->purpose = npurpose;
          srcgrp_b->purpose = opurpose;
          sfile->src_groups[ai] = srcgrp_b;
          sfile->src_groups[bi] = srcgrp_a;
          for (int i = 0; i < mainw->num_tracks; i++) {
            if (mainw->track_sources[i] == srcgrp_a) {
              mainw->track_sources[i] = srcgrp_b;
              srcgrp_b->track = i;
            } else if (mainw->track_sources[i] == srcgrp_b) {
              mainw->track_sources[i] = srcgrp_a;
              srcgrp_a->track = i;
            }
          }
          pthread_mutex_unlock(&sfile->srcgrp_mutex);
          return TRUE;
	  // *INDENT-OFF*
	}}}
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
  // *INDENT-ON*
  return FALSE;
}


static void _clip_src_free(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp, lives_clip_src_t *mysrc) {
  if (sfile) {
    lives_layer_t *layer;
    int nsrcs = srcgrp->n_srcs, i;
    for (i = 0; i < sfile->n_src_groups; i++)
      if (sfile->src_groups[i] == srcgrp) break;

    if (i == sfile->n_src_groups) return;
    if (!nsrcs) return;

    for (i = 0; i < nsrcs; i++) if (srcgrp->srcs[i] == mysrc) break;

    if (i == nsrcs) return;

    layer = srcgrp->layer;
    if (layer) {
      weed_layer_set_invalid(layer, TRUE);
      weed_layer_pixel_data_free(layer);
    }

    switch (mysrc->src_type) {
    case LIVES_SRC_TYPE_DECODER: {
      lives_decoder_t *dec = (lives_decoder_t *)mysrc->actor;
      clip_decoder_free(sfile, dec);
    }
    break;
    case LIVES_SRC_TYPE_FILE_BUFF:
      lives_close_buffered(LIVES_POINTER_TO_INT(mysrc->actor_inst));
      mysrc->actor_inst = NULL;
      break;
    case LIVES_SRC_TYPE_GENERATOR: {
      weed_instance_t *inst = (weed_instance_t *)(mysrc->actor_inst);
      weed_generator_end(inst);
    }
    break;
    default:
      switch (sfile->clip_type) {
      case CLIP_TYPE_YUV4MPEG:
#ifdef HAVE_YUV4MPEG
        lives_yuv_stream_stop_read((lives_yuv4m_t *)mysrc->actor);
#endif
        break;
      case CLIP_TYPE_VIDEODEV:
#ifdef HAVE_UNICAP
        lives_vdev_free((lives_vdev_t *)mysrc->actor);
#endif
        break;
      default: break;
      }
      break;
    }

    if (mysrc->ext_URI) lives_free(mysrc->ext_URI);
    lives_free(mysrc);

    if (i < nsrcs) {
      srcgrp->n_srcs = --nsrcs;
      for (; i < nsrcs; i++) {
        srcgrp->srcs[i] = srcgrp->srcs[i + 1];
      }
      if (nsrcs) srcgrp->srcs = lives_recalloc(srcgrp->srcs, nsrcs,
                                  nsrcs + 1, sizeof(lives_clip_src_t *));
      else {
        lives_freep((void **)&srcgrp->srcs);
      }
    }
  }
}


void clip_src_free(int nclip,  lives_clipsrc_group_t *srcgrp, lives_clip_src_t *mysrc) {
  // free a clipsrc from clipgrp
  // check flags for nonfree
  // does full locking
  if (srcgrp && mysrc) {
    lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
    if (sfile) {
      if (!(mysrc->flags & SRC_FLAG_NOFREE)) {
        pthread_mutex_lock(&srcgrp->src_mutex);
        _clip_src_free(sfile, srcgrp, mysrc);
        pthread_mutex_unlock(&srcgrp->src_mutex);
      }
    }
  }
}


static void _clip_srcs_free_all(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp) {
  for (int i = 0; i < srcgrp->n_srcs; i++) {
    _clip_src_free(sfile, srcgrp, srcgrp->srcs[i]);
    lives_freep((void **)&srcgrp->srcs);
  }
}


void clip_srcs_free_all(int nclip, lives_clipsrc_group_t *srcgrp) {
  // free all srcs in grp
  // check first to see if nclip is valid, srcgrp is in its src_groups
  // does not free or remove srcgrp
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    int i;
    for (i = 0; i < sfile->n_src_groups; i++)
      if (sfile->src_groups[i] == srcgrp) break;
    if (i == sfile->n_src_groups) return;
    pthread_mutex_lock(&srcgrp->src_mutex);
    _clip_srcs_free_all(sfile, srcgrp);
    pthread_mutex_unlock(&srcgrp->src_mutex);
  }
}


static void _srcgrp_free(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp) {
  // free all srcs in srcgrp, then
  if (sfile && srcgrp) {
    if (srcgrp->n_srcs) _clip_srcs_free_all(sfile, srcgrp);
    lives_free(srcgrp);
  }
}


void srcgrp_free(int nclip, lives_clipsrc_group_t *srcgrp) {
  // with locking and flag checking
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    //if (!(srcgrp->flags & SRC_FLAG_NOFREE))
    _srcgrp_free(sfile, srcgrp);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
}


static void  _srcgrp_remove(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp) {
  // remove but do not free srcgrp. no locking or error checking
  int ngrps = sfile->n_src_groups;
  if (ngrps) {
    int i;
    for (i = 0; i < ngrps; i++)
      if (sfile->src_groups[i] == srcgrp) break;
    if (i < ngrps) {
      sfile->n_src_groups = --ngrps;
      if (ngrps) {
        for (; i < ngrps; i++) {
          sfile->src_groups[i] = sfile->src_groups[i + 1];
        }
        sfile->src_groups = lives_recalloc(sfile->src_groups,
                                           ngrps, ngrps + 1, sizeof(lives_clipsrc_group_t *));
      } else {
        lives_free(sfile->src_groups);
        sfile->src_groups = NULL;
      }
    }
  }
}


void srcgrp_remove(int nclip, int track, int purpose) {
  // TODO - do not remove if any srcs flagged as NOFREE
  // remove + free a srcgrp with purpose,
  // also does locking and checking but ignores nofree if purpose is any
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    lives_clipsrc_group_t *srcgrp;
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    srcgrp = _get_srcgrp(sfile, track, purpose);
    if (srcgrp) {
      if (purpose == SRC_PURPOSE_ANY || srcgrp->purpose != SRC_PURPOSE_PRIMARY) {
        _srcgrp_remove(sfile, srcgrp);
        _srcgrp_free(sfile, srcgrp);
      }
    }
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
}


LIVES_GLOBAL_INLINE void srcgrps_free_all(int nclip) {
  // free all srcgrps, does locking but  ignores nofree flag
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    while (sfile->n_src_groups) {
      lives_clipsrc_group_t *srcgrp = sfile->src_groups[0];
      _srcgrp_remove(sfile, srcgrp);
      _srcgrp_free(sfile, srcgrp);
    }
    lives_freep((void **)&sfile->src_groups);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
}


void srcgrp_set_apparent(lives_clip_t *sfile, lives_clipsrc_group_t *srcgrp, full_pal_t pally, int gamma_type) {
  if (srcgrp && sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    srcgrp->apparent_pal = pally.pal;
    srcgrp->apparent_gamma = gamma_type;
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
}


static lives_clip_src_t *_get_primary_src(lives_clip_t *sfile) {
  if (sfile) {
    lives_clipsrc_group_t *srcgrp = _get_srcgrp(sfile, -1, SRC_PURPOSE_PRIMARY);
    if (srcgrp && srcgrp->n_srcs) {
      for (int i = 0; i < srcgrp->n_srcs; i++) {
        if (srcgrp->srcs[i] && srcgrp->srcs[i]->actor)
          return srcgrp->srcs[i];
      }
    }
  }
  return NULL;
}


// primary source is the first non-static clipsrc in the PRIMARY srcgrp.
// to the srcgrp
lives_clip_src_t *get_primary_src(int nclip) {
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    lives_clip_src_t *src;
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    src = _get_primary_src(sfile);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
    return src;
  }
  return NULL;
}


lives_clip_src_t *_clone_clipsrc(lives_clipsrc_group_t *srcgrp, int nclip, lives_clip_src_t *insrc) {
  lives_clip_src_t *outsrc;
  if (!insrc) return NULL;
  if (insrc->actor_inst) return NULL;
  outsrc = (lives_clip_src_t *)lives_calloc(1, sizeof(lives_clip_src_t));
  if (outsrc) {
    outsrc->uid = gen_unique_id();
    outsrc->class_uid = insrc->class_uid;
    outsrc->actor_uid = insrc->actor_uid;

    outsrc->src_type = insrc->src_type;
    outsrc->flags = insrc->flags;

    switch (insrc->src_type) {
    case LIVES_SRC_TYPE_DECODER:
      outsrc->actor = clone_decoder(nclip);
      break;
    case LIVES_SRC_TYPE_FILE_BUFF:
      outsrc->actor_inst = insrc->actor_inst;
      break;
    default:
      outsrc->actor = insrc->actor;
      break;
    }

    lives_memcpy(&outsrc->ext_checksum, &insrc->ext_checksum, sizeof(fingerprint_t));
    if (insrc->ext_URI) outsrc->ext_URI = lives_strdup(insrc->ext_URI);
  }
  return outsrc;
}


lives_clip_src_t *clone_clipsrc(lives_clipsrc_group_t *srcgrp, int nclip, lives_clip_src_t *insrc) {
  lives_clip_src_t *mysrc = NULL;
  if (srcgrp && insrc && IS_VALID_CLIP(nclip)) {
    pthread_mutex_lock(&srcgrp->src_mutex);
    mysrc = _clone_clipsrc(srcgrp, nclip, insrc);
    pthread_mutex_unlock(&srcgrp->src_mutex);
  }
  return mysrc;
}


// a shortcut for adding a new clip_src to PRIMARY srcgrp
lives_clip_src_t *add_primary_src(int nclip, void *actor, int src_type) {
  lives_clip_src_t *mysrc = NULL;
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    mysrc = _add_clip_src(sfile, -1, SRC_PURPOSE_PRIMARY,
                          actor, NULL, src_type, 0, NULL, NULL);

    for (int i = 0; i < sfile->n_src_groups; i++) {
      lives_clipsrc_group_t *srcgrp = sfile->src_groups[i];
      if (srcgrp->purpose == SRC_PURPOSE_PRIMARY) continue;
      _clone_clipsrc(srcgrp, nclip, mysrc);
    }
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
  return mysrc;
}


lives_clip_src_t *add_primary_inst(int nclip, void *actor, void *inst, int src_type) {
  // add actor + actor_inst to primary srcgroup, which will be created if it does not exist
  // clip_srcs with actor_inst cannot be cloned, since instances are specific to  single clip_src
  // so this is not copied to other srcgrps
  lives_clip_src_t *mysrc = NULL;
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    mysrc = _add_clip_src(sfile, -1, SRC_PURPOSE_PRIMARY,
                          actor, inst, src_type, 0, NULL, NULL);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
  return mysrc;
}


// a shortcut for removing a clip_src from primary srcgrp, only clip number and src_type need be specified
void remove_primary_src(int nclip, int src_type) {
  // TODO - remove from all srcgrps
  lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
  if (sfile) {
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    for (int i = 0; i < sfile->n_src_groups; i++) {
      lives_clipsrc_group_t *srcgrp = sfile->src_groups[i];
      lives_clip_src_t *mysrc = get_clip_src(srcgrp, 0, src_type, NULL, NULL);
      if (mysrc) clip_src_free(nclip, srcgrp, mysrc);
    }
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
  }
}


void *get_primary_actor(lives_clip_t *sfile) {
  if (sfile) {
    lives_clip_src_t *src;
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    src = _get_primary_src(sfile);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
    if (src) return src->actor;
  }
  return NULL;
}


void *get_primary_inst(lives_clip_t *sfile) {
  if (sfile) {
    lives_clip_src_t *src;
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    src = _get_primary_src(sfile);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
    if (src) return src->actor_inst;
  }
  return NULL;
}


int get_primary_src_type(lives_clip_t *sfile) {
  if (sfile) {
    lives_clip_src_t *src;
    pthread_mutex_lock(&sfile->srcgrp_mutex);
    src = _get_primary_src(sfile);
    pthread_mutex_unlock(&sfile->srcgrp_mutex);
    if (src) return src->src_type;
  }
  return LIVES_SRC_TYPE_UNDEFINED;
}


lives_clipsrc_group_t *clone_srcgrp(int dclip, int sclip, int track, int purpose) {
  // create a clone of PRIMARY with track and purpose
  // from sclip to dclip (dclip can == sclip)
  // clip_srcs which have actor_inst defined will not be copied
  lives_clip_t *sfile = RETURN_VALID_CLIP(sclip);
  if (sfile) {
    lives_clipsrc_group_t *srcgrp = get_srcgrp(sclip, -1, SRC_PURPOSE_PRIMARY);
    if (srcgrp) {
      // create new grp, memcpy, adjust track and purpose and uid
      // step trhough srcs, clone each one
      lives_clipsrc_group_t *xsrcgrp = _add_srcgrp(sfile, track, purpose);
      lives_memcpy(xsrcgrp, srcgrp, sizeof(lives_clipsrc_group_t));
      pthread_mutex_init(&xsrcgrp->src_mutex, NULL);
      pthread_mutex_init(&xsrcgrp->refcnt_mutex, NULL);
      xsrcgrp->refcnt = 1;
      // set again as memcpy will overwrite
      xsrcgrp->track = track;
      xsrcgrp->purpose = purpose;
      pthread_mutex_lock(&xsrcgrp->src_mutex);
      for (int i = 0; i < srcgrp->n_srcs; i++)
        _clone_clipsrc(xsrcgrp, sclip, srcgrp->srcs[i]);
      pthread_mutex_unlock(&xsrcgrp->src_mutex);
      return xsrcgrp;
    }
  }
  return NULL;
}
