// cliphandler.c
// (c) G. Finch 2019 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "cvirtual.h"


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


char *get_staging_dir_for(int index, lives_intention intent) {
  // optionally we can stage the clip open in an alt directory
  char *shm_path = NULL;
  lives_intentparams_t *iparams;
  lives_intentcap_t *icaps = lives_intentcaps_new(intent);
  iparams = get_txparams_for_clip(index, icaps);
  lives_intentcaps_free(icaps);

  if (iparams) {
    weed_param_t *adir_param =
      weed_param_from_iparams(iparams, CLIP_PARAM_STAGING_DIR);
    if (adir_param) shm_path = weed_param_get_value_string(adir_param);
    lives_intentparams_free(iparams);
  }
  return shm_path;
}


LIVES_GLOBAL_INLINE
char *use_staging_dir_for(int clipno) {
  if (clipno > 0 && IS_VALID_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
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
    break;
  case CLIP_DETAILS_PB_FPS:
    *(double *)retval = lives_strtod(val);
    if (*(double *)retval == 0.) *(double *)retval = sfile->fps;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    *(uint64_t *)retval = (uint64_t)lives_strtoul(val);
    break;
  case CLIP_DETAILS_AENDIAN:
    *(int *)retval = atoi(val) * 2; break;
  case CLIP_DETAILS_TITLE:
  case CLIP_DETAILS_AUTHOR:
  case CLIP_DETAILS_COMMENT:
  case CLIP_DETAILS_CLIPNAME:
  case CLIP_DETAILS_KEYWORDS:
    lives_snprintf((char *)retval, maxlen, "%s", val);
    break;
  case CLIP_DETAILS_FILENAME:
  case CLIP_DETAILS_DECODER_NAME:
    lives_snprintf((char *)retval, maxlen, "%s", (tmp = F2U8(val)));
    lives_free(tmp);
    break;
  case CLIP_DETAILS_DECODER_UID:
    *(uint64_t *)retval = (uint64_t)lives_strtol(val);
    break;
  default:
    lives_free(val);
    return FALSE;
  }
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

  if (!IS_VALID_CLIP(which)) return FALSE;

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
      set_signal_handlers((SignalHandlerPointer)defer_sigint);
      needs_sigs = TRUE;
    }
    com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"",
                              temp_backend, lives_header, key, myval);
    lives_system(com, FALSE);
    lives_cp(lives_header, lives_header_bak);
    pthread_mutex_unlock(&sfile->transform_mutex);
    if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
    if (needs_sigs) set_signal_handlers((SignalHandlerPointer)catch_sigint);
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

  set_signal_handlers((SignalHandlerPointer)defer_sigint); // ignore ctrl-c

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
        set_signal_handlers((SignalHandlerPointer)catch_sigint);
        if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
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
        if (sfile->clip_type == CLIP_TYPE_FILE && sfile->ext_src) {
          lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
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
        if (sfile->clip_type == CLIP_TYPE_FILE && sfile->ext_src) {
          lives_decoder_t *dplug = (lives_decoder_t *)sfile->ext_src;
          if (!save_clip_value(which, CLIP_DETAILS_DECODER_NAME, (void *)dplug->dpsys->soname)) break;
          if (!save_clip_value(which, CLIP_DETAILS_DECODER_UID, (void *)&sfile->decoder_uid)) break;
        }
        all_ok = TRUE;
      } while (FALSE);

      fclose(mainw->clip_header);

      if (!all_ok) {
        retval = do_write_failed_error_s_with_retry(lives_header_new, NULL);
      } else {
        char *lives_header;
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

  if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
  set_signal_handlers((SignalHandlerPointer)catch_sigint);

  lives_free(lives_header_new);
  mainw->clip_header = NULL;

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

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
  if (filesize < 0) {
    filesize = 0;
  }
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
        lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
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
  if (IS_NORMAL_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
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
      if (lives_file_test(binfmt, LIVES_FILE_TEST_EXISTS)) {
        // this is mainly for aesthetic reasons
        char *binfmt_to = lives_build_filename(clipdir, "." TOTALSAVE_NAME, NULL);
        lives_mv(binfmt, binfmt_to);
      }
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
static boolean do_delete_or_mark(int clipno) {
  char *clipdir = NULL;
  boolean rember = FALSE, ret = FALSE;
  LiVESResponseType resp = LIVES_RESPONSE_NONE;
  clipdir = get_clip_dir(clipno);
  if (prefs->badfile_intent == LIVES_INTENTION_UNKNOWN) {
    LiVESWidget *vbox, *check;
    LiVESWidget *dialog;
    dialog = create_question_dialog(_("Cleanup options"), _("LiVES was unable to load a clip which seems to be "
                                    "damaged beyond repair\n"
                                    "This clip can be deleted or marked as unopenable "
                                    "and ignored\n"
                                    "What would you like to do ?\n"));

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
  if (prefs->badfile_intent == LIVES_INTENTION_DELETE || resp == LIVES_RESPONSE_YES) {
    lives_rmdir(clipdir, TRUE);
    if (rember) pref_factory_int(PREF_BADFILE_INTENT, &prefs->badfile_intent, LIVES_INTENTION_DELETE, TRUE);
    ret = TRUE;
  } else {
    char *ignore = lives_build_filename(clipdir, LIVES_FILENAME_IGNORE, NULL);
    lives_touch(ignore);
    lives_free(ignore);
    if (rember) pref_factory_int(PREF_BADFILE_INTENT, &prefs->badfile_intent, LIVES_INTENTION_IGNORE, TRUE);
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


boolean read_headers(int fileno, const char *dir, const char *file_name) {
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

  if (!IS_VALID_CLIP(fileno)) return FALSE;

  sfile = mainw->files[fileno];

  old_hdrfile = lives_build_filename(dir, LIVES_CLIP_HEADER_OLD, NULL);

  if (IS_ASCRAP_CLIP(fileno)) {
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
        if (fileno != mainw->current_file) goto rhd_failed;
        retval2 = do_read_failed_error_s_with_retry(lives_header, NULL);
      }
    } while (retval2 == LIVES_RESPONSE_RETRY);

    if (retval2 == LIVES_RESPONSE_CANCEL) {
      goto rhd_failed;
    }

    if (fileno == mainw->current_file) {
      threaded_dialog_spin(0.);
    }

    if (!is_ascrap) restore_clip_binfmt(fileno);

    do {
      do {
        detail = CLIP_DETAILS_HEADER_VERSION;
        retval = get_clip_value(fileno, detail, &sfile->header_version, 16);
        if (retval) {
          if (sfile->header_version < 100) goto old_check;
        } else {
          if (lives_file_test(old_hdrfile, LIVES_FILE_TEST_EXISTS)) {
            goto old_check;
          }
          if (fileno != mainw->current_file) {
            goto rhd_failed;
          }
          if (mainw->hdrs_cache) {
            retval2 = do_header_missing_detail_error(fileno, CLIP_DETAILS_HEADER_VERSION);
          } else {
            retval2 = do_header_read_error_with_retry(fileno);
          }
        }
      } while (retval2 == LIVES_RESPONSE_RETRY);

      if (retval2 == LIVES_RESPONSE_CANCEL) goto rhd_failed;

      if (is_ascrap) goto get_avals;

      if (sfile->header_version >= 104) {
        uint64_t uid = sfile->unique_id;
        detail = CLIP_DETAILS_UNIQUE_ID;
        retval = get_clip_value(fileno, detail, &sfile->unique_id, 0);
        if (!sfile->unique_id) sfile->unique_id = uid;
      }

      if (sfile->header_version >= 104) {
        retval = get_clip_value(fileno, CLIP_DETAILS_DECODER_UID, &sfile->decoder_uid, 0);
        if (!retval) {
          retval = TRUE;
          sfile->decoder_uid = 0;
        }
      }

      detail = CLIP_DETAILS_FRAMES;
      retval = get_clip_value(fileno, detail, &sfile->frames, 0);
      if (!retval) {
        if (prefs->show_dev_opts) {
          g_printerr("no framecount detected\n");
        }
        sfile->needs_update = retval = TRUE;
      }

      if (retval) {
        int bpp = sfile->bpp;
        detail = CLIP_DETAILS_BPP;
        get_clip_value(fileno, detail, &sfile->bpp, 0);
        if (!retval) {
          sfile->bpp = bpp;
          retval = TRUE;
        }
      }
      if (retval) {
        detail = CLIP_DETAILS_IMG_TYPE;
        get_clip_value(fileno, detail, &sfile->img_type, 0);
      }
      if (retval) {
        detail = CLIP_DETAILS_FPS;
        retval = get_clip_value(fileno, detail, &sfile->fps, 0);
      }
      if (retval) {
        detail = CLIP_DETAILS_PB_FPS;
        retval = get_clip_value(fileno, detail, &sfile->pb_fps, 0);
        if (!retval) {
          retval = TRUE;
          sfile->pb_fps = sfile->fps;
        }
      }
      if (retval) {
        retval = get_clip_value(fileno, CLIP_DETAILS_PB_FRAMENO, &sfile->frameno, 0);
        if (!retval) {
          retval = TRUE;
          sfile->frameno = 1;
        }
        if (sfile->frameno <= 0) sfile->frameno = 1;
      }
      if (retval) {
        detail = CLIP_DETAILS_WIDTH;
        retval = get_clip_value(fileno, detail, &sfile->hsize, 0);
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
        retval = get_clip_value(fileno, detail, &sfile->vsize, 0);
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
          get_clip_value(fileno, detail, &sfile->gamma_type, 0);
          if (sfile->gamma_type == WEED_GAMMA_UNKNOWN) sfile->gamma_type = WEED_GAMMA_SRGB;
          if (sfile->gamma_type != WEED_GAMMA_SRGB) {
            if (!do_gamma_import_warn(sfile->has_binfmt ?
                                      sfile->binfmt_version.num : 0, sfile->gamma_type)) goto rhd_failed;
          }
        }
      }
      if (retval) {
        detail = CLIP_DETAILS_CLIPNAME;
        get_clip_value(fileno, detail, sfile->name, CLIP_NAME_MAXLEN);
      }
      if (retval) {
        detail = CLIP_DETAILS_FILENAME;
        get_clip_value(fileno, detail, sfile->file_name, PATH_MAX);
      }

get_avals:
      if (retval) {
        detail = CLIP_DETAILS_ACHANS;
        retvala = get_clip_value(fileno, detail, &sfile->achans, 0);
        if (!retvala) sfile->achans = 0;
      }

      if (!sfile->achans) retvala = FALSE;
      else retvala = TRUE;

      if (retval && retvala) {
        detail = CLIP_DETAILS_ARATE;
        retvala = get_clip_value(fileno, detail, &sfile->arps, 0);
      }

      if (!retvala) sfile->arps = sfile->achans = sfile->arate = sfile->asampsize = 0;
      if (!sfile->arps) retvala = FALSE;

      if (retvala && retval) {
        detail = CLIP_DETAILS_PB_ARATE;
        retvala = get_clip_value(fileno, detail, &sfile->arate, 0);
        if (!retvala) {
          retvala = TRUE;
          sfile->arate = sfile->arps;
        }
      }
      if (retvala && retval) {
        detail = CLIP_DETAILS_ASIGNED;
        retval = get_clip_value(fileno, detail, &asigned, 0);
      }
      if (retvala && retval) {
        detail = CLIP_DETAILS_AENDIAN;
        retval = get_clip_value(fileno, detail, &aendian, 0);
      }

      sfile->signed_endian = asigned + aendian;

      if (retvala && retval) {
        detail = CLIP_DETAILS_ASAMPS;
        retval = get_clip_value(fileno, detail, &sfile->asampsize, 0);
      }
      if (!retval) {
        if (fileno != mainw->current_file) goto rhd_failed;
        if (mainw->hdrs_cache) {
          retval2 = do_header_missing_detail_error(fileno, detail);
        } else {
          retval2 = do_header_read_error_with_retry(fileno);
        }
      } else {
        if (!is_ascrap) {
          get_clip_value(fileno, CLIP_DETAILS_TITLE, sfile->title, 1024);
          get_clip_value(fileno, CLIP_DETAILS_AUTHOR, sfile->author, 1024);
          get_clip_value(fileno, CLIP_DETAILS_COMMENT, sfile->comment, 1024);
          get_clip_value(fileno, CLIP_DETAILS_KEYWORDS, sfile->keywords, 1024);
          get_clip_value(fileno, CLIP_DETAILS_INTERLACE, &sfile->interlace, 0);
          // user must have selected this:
          if (sfile->interlace != LIVES_INTERLACE_NONE) sfile->deinterlace = TRUE;
        }
        lives_free(old_hdrfile);
        lives_free(lives_header);
        if (!prefs->vj_mode) {
          sfile->afilesize = reget_afilesize_inner(fileno);
        }
        /// need to maintain mainw->hdrs_cache in this case, as it may be
        // passed to further functions, but it needs to be freed and set to NULL
        // at some point
        return TRUE;
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

    if (get_clip_value(fileno, detail, &sfile->frames, 0)) {
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

      lives_popen(com, fileno != mainw->current_file, buff, 1024);
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
      if (fileno == mainw->current_file) threaded_dialog_spin(0.);
    } else goto rhd_failed;
    lives_free(old_hdrfile);
    /// mainw->hdrs_cache never set
    return TRUE;
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
      if (fileno != mainw->current_file) goto rhd_failed;
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
  return TRUE;

rhd_failed:

  stdir = get_staging_dir_for(fileno, LIVES_ICAPS_LOAD);
  if (stdir) {
    char *clip_stdir = lives_build_path(stdir, sfile->handle, NULL);
    if (lives_file_test(clip_stdir, LIVES_FILE_TEST_IS_DIR)) {
      char *clipdir = get_clip_dir(fileno);
      //break_me("ready");
      lives_cp_noclobber(clip_stdir, clipdir);
      lives_rmdir(clip_stdir, TRUE);
      lives_free(stdir);
      lives_free(clip_stdir);
      lives_free(clipdir);
      goto frombak;
    }
  }
  lives_free(stdir);

  // header file (lives_header) missing or damaged -- see if we can recover ///////////////
  if (fileno == mainw->current_file) {
    char *clipdir = get_clip_dir(fileno);
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
      if (recover_details(fileno, lives_header, hdrback, binfmt)) {
        if (hdrback) {
          lives_free(hdrback);
          if (binfmt) lives_free(binfmt);
          goto frombak;
        }
        lives_free(binfmt);
        save_clip_values(fileno);
        return TRUE;
      }
    }

    do_delete_or_mark(fileno);

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
  if (IS_VALID_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
    if (IS_NORMAL_CLIP(clipno) || sfile->clip_type == CLIP_TYPE_TEMP || sfile->clip_type == CLIP_TYPE_VIDEODEV) {
      if (*sfile->staging_dir) {
        return lives_build_path(sfile->staging_dir, sfile->handle, NULL);
      }
      return lives_build_path(prefs->workdir, sfile->handle, NULL);
    }
    return lives_build_path(prefs->workdir, sfile->handle, NULL);
  }
  return NULL;
}


void migrate_from_staging(int clipno) {
  if (IS_NORMAL_CLIP(clipno) || IS_TEMP_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
    if (*sfile->staging_dir) {
      char *old_clipdir, *new_clipdir, *stfile;
      old_clipdir = get_clip_dir(clipno);
      if (sfile->achans && sfile->ext_src) wait_for_bg_audio_sync(clipno);
      pthread_mutex_lock(&sfile->transform_mutex);
      *sfile->staging_dir = 0;
      new_clipdir = get_clip_dir(clipno);
      lives_cp_noclobber(old_clipdir, new_clipdir);
      if (sfile->achans && sfile->ext_src) wait_for_bg_audio_sync(clipno);
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

///////////////////////////// intents ////////////////////////
typedef struct {
  int idx;
  lives_clip_t *sfile;
} clip_priv_data_t;


lives_intentparams_t *_get_params_for_clip_tx(lives_object_t *obj, int state,
    lives_intentcap_t *icaps) {
  lives_intention intent = icaps->intent;
  lives_intentparams_t *iparams = NULL;
  clip_priv_data_t *priv = (clip_priv_data_t *)obj->priv;

  if (intent != LIVES_INTENTION_IMPORT) return NULL;

  if (LIVES_CAPACITY_FALSE(icaps->capacities, LIVES_CAPACITY_REMOTE)) {
    if (prefs->startup_phase != 0) return NULL;
    if (capable->writeable_shmdir == MISSING) return NULL;
    if (state == CLIP_STATE_READY) {
      if (!*priv->sfile->staging_dir) return NULL;
    } else get_shmdir();

    iparams = (lives_intentparams_t *)lives_calloc(sizeof(lives_intentparams_t), 1);
    iparams->params = (weed_param_t **)lives_calloc(sizeof(weed_param_t *), 2);

    // TODO - need to mark
    if (state == CLIP_STATE_READY) {
      iparams->params[0] = string_req_init(CLIP_PARAM_STAGING_DIR, priv->sfile->staging_dir);
    } else {
      iparams->params[0] = string_req_init(CLIP_PARAM_STAGING_DIR, capable->shmdir_path);
    }
    iparams->params[1] = NULL;
  } else if (lives_capacity_get(icaps->capacities, LIVES_CAPACITY_REMOTE)) {
    char *uidstr, *tmpdir;
    iparams = (lives_intentparams_t *)lives_calloc(sizeof(lives_intentparams_t), 1);
    //iparams->intent = intent;
    iparams->params = (weed_param_t **)lives_calloc(sizeof(weed_param_t *), 2);

    // eventually 'ytdl' should come from an optional req CLIP_STAGING_PARAM
    // to perform the IMPORT transformation, which would resolve to open_file_sel for LiVES
    // and get_clip_data for the clip
    uidstr = lives_strdup_printf("ytdl-%lu", gen_unique_id());
    tmpdir = get_systmp(uidstr, TRUE);
    iparams->params[0] = string_req_init(CLIP_PARAM_STAGING_DIR, tmpdir);
    lives_free(tmpdir);
    iparams->params[1] = NULL;
  }
  return iparams;
}


// TODO - merge with transform for intent, or whatever it ends up being...
LIVES_LOCAL_INLINE
lives_intentparams_t *get_txparams_for_intent(lives_object_t *obj, lives_intentcap_t *icaps) {
  if (obj->type == OBJECT_TYPE_CLIP) {
    return _get_params_for_clip_tx(obj, obj->state, icaps);
  }
  return NULL;
}


// placeholder function, until we have proper objects for clips
lives_intentparams_t *get_txparams_for_clip(int clipno, lives_intentcap_t *icaps) {
  lives_object_t obj;
  lives_intentparams_t *iparams;
  clip_priv_data_t *priv;
  obj.type = OBJECT_TYPE_CLIP;
  priv = obj.priv = (clip_priv_data_t *)lives_calloc(1, sizeof(clip_priv_data_t));
  priv->idx = clipno;
  if (!IS_VALID_CLIP(clipno) || mainw->recovering_files) {
    priv->sfile = NULL;
    obj.state = CLIP_STATE_NOT_LOADED;
  } else {
    priv->sfile = mainw->files[clipno];
    obj.state = CLIP_STATE_READY;
  }
  iparams = get_txparams_for_intent(&obj, icaps);
  lives_free(priv);
  return iparams;
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

  mainw->current_file = new_file;

  cfile->is_loaded = is_loaded;

  // any cfile (clip) initialisation goes in here
  lives_memcpy((void *)&cfile->binfmt_check.chars, BINFMT_CHECK_CHARS, 8);
  cfile->binfmt_version.num = make_version_hash(LiVES_VERSION);
  cfile->binfmt_bytes.size = (size_t)(offsetof(lives_clip_t, binfmt_end));
  cfile->menuentry = NULL;
  cfile->start = cfile->end = 0;
  cfile->old_frames = cfile->opening_frames = cfile->frames = 0;
  lives_snprintf(cfile->type, 40, "%s", _("Unknown"));
  cfile->f_size = 0l;
  cfile->achans = 0;
  cfile->arate = 0;
  cfile->arps = 0;
  cfile->afilesize = 0l;
  cfile->asampsize = 0;
  cfile->adirection = LIVES_DIRECTION_FORWARD;
  cfile->aplay_fd = -1;
  cfile->undoable = FALSE;
  cfile->redoable = FALSE;
  cfile->changed = FALSE;
  cfile->was_in_set = FALSE;
  cfile->hsize = cfile->vsize = cfile->ohsize = cfile->ovsize = 0;
  cfile->fps = cfile->pb_fps = prefs->default_fps;
  cfile->resample_events = NULL;
  cfile->insert_start = cfile->insert_end = 0;
  cfile->is_untitled = TRUE;
  cfile->was_renamed = FALSE;
  cfile->undo_action = UNDO_NONE;
  cfile->delivery = LIVES_DELIVERY_PULL;
  cfile->opening_audio = cfile->opening = cfile->opening_only_audio = FALSE;
  cfile->pointer_time = 0.;
  cfile->real_pointer_time = 0.;
  cfile->restoring = cfile->opening_loc = cfile->nopreview = FALSE;
  cfile->video_time = cfile->laudio_time = cfile->raudio_time = 0.;
  cfile->freeze_fps = 0.;
  cfile->last_vframe_played = 0;
  cfile->frameno = cfile->last_frameno = cfile->saved_frameno = 1;
  cfile->progress_start = cfile->progress_end = 0;
  cfile->play_paused = cfile->nokeep = FALSE;
  cfile->undo_start = cfile->undo_end = 0;
  cfile->ext_src = NULL;
  cfile->ext_src_type = LIVES_EXT_SRC_NONE;
  cfile->clip_type = CLIP_TYPE_DISK;
  cfile->ratio_fps = FALSE;
  cfile->aseek_pos = 0;
  cfile->unique_id = gen_unique_id();
  cfile->layout_map = NULL;
  cfile->frame_index = cfile->frame_index_back = NULL;
  cfile->pumper = NULL;
  cfile->stored_layout_frame = 0;
  cfile->stored_layout_audio = 0.;
  cfile->stored_layout_fps = 0.;
  cfile->stored_layout_idx = -1;
  cfile->interlace = LIVES_INTERLACE_NONE;
  cfile->subt = NULL;
  cfile->no_proc_sys_errors = cfile->no_proc_read_errors = cfile->no_proc_write_errors = FALSE;
  cfile->keep_without_preview = FALSE;
  cfile->cb_src = -1;
  cfile->needs_update = cfile->needs_silent_update = FALSE;
  cfile->audio_waveform = NULL;
  cfile->md5sum[0] = 0;
  cfile->gamma_type = WEED_GAMMA_SRGB;
  cfile->last_play_sequence = 0;
  cfile->tcache_dubious_from = 0;
  cfile->tcache_height = 0;
  cfile->tcache = NULL;
  cfile->checked = FALSE;
  cfile->has_binfmt = TRUE;

  pthread_mutex_init(&cfile->transform_mutex, NULL);

  cfile->img_type = lives_image_ext_to_img_type(prefs->image_ext);

  cfile->bpp = (cfile->img_type == IMG_TYPE_JPEG) ? 24 : 32;
  cfile->deinterlace = FALSE;

  cfile->play_paused = FALSE;
  cfile->header_version = LIVES_CLIP_HEADER_VERSION;

  cfile->event_list = cfile->event_list_back = NULL;
  cfile->next_event = NULL;
  cfile->vol = 1.;

  lives_memset(cfile->name, 0, 1);
  lives_memset(cfile->mime_type, 0, 1);
  lives_memset(cfile->file_name, 0, 1);
  lives_memset(cfile->save_file_name, 0, 1);

  lives_memset(cfile->comment, 0, 1);
  lives_memset(cfile->author, 0, 1);
  lives_memset(cfile->title, 0, 1);
  lives_memset(cfile->keywords, 0, 1);

  cfile->signed_endian = AFORM_UNKNOWN;
  lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
  lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));

  clipdir = get_clip_dir(mainw->current_file);
  stfile = lives_build_filename(clipdir, LIVES_STATUS_FILE_NAME, NULL);
  lives_free(clipdir);

  lives_snprintf(cfile->info_file, PATH_MAX, "%s", stfile);
  lives_free(stfile);

  // backwards compat.
  cfile->checked_for_old_header = FALSE;
  cfile->has_old_header = FALSE;

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
