// cliphandler.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp) {
  char *key = NULL;

  switch (what) {
  case CLIP_DETAILS_HEADER_VERSION:
    key = lives_strdup("header_version"); break;
  case CLIP_DETAILS_BPP:
    key = lives_strdup("bpp"); break;
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
  case CLIP_DETAILS_GAMMA_TYPE:
    key = lives_strdup("gamma_type"); break;
  default: break;
  }
  if (maxlenp && *maxlenp == 0) *maxlenp = 256;
  return key;
}


boolean get_clip_value(int which, lives_clip_details_t what, void *retval, size_t maxlen) {
  lives_clip_t *sfile = mainw->files[which];
  char *lives_header = NULL;
  char *val, *key, *tmp;

  int retval2 = LIVES_RESPONSE_NONE;

  if (!IS_VALID_CLIP(which)) return FALSE;

  if (!mainw->hdrs_cache) {
    /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
    /// otherwise it may be wrongly classified as a recoverable clip
    /// (here this is largely academic, since the values are only read during crash recovery,
    /// and the header should have been cached)
    if (which == mainw->ascrap_file) {
      lives_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle,
                                          LIVES_ACLIP_HEADER, NULL);
      if (!lives_file_test(lives_header, LIVES_FILE_TEST_EXISTS)) {
        lives_free(lives_header);
        lives_header = NULL;
      }
    }
    if (!lives_header)
      lives_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle,
                                          LIVES_CLIP_HEADER, NULL);
    if (!sfile->checked_for_old_header) {
      struct stat mystat;
      time_t old_time = 0, new_time = 0;
      char *old_header = lives_build_filename(prefs->workdir, sfile->handle, LIVES_CLIP_HEADER_OLD, NULL);
      sfile->checked_for_old_header = TRUE;
      if (!lives_file_test(old_header, LIVES_FILE_TEST_EXISTS)) {
        if (!stat(old_header, &mystat)) old_time = mystat.st_mtime;
        if (!stat(lives_header, &mystat)) new_time = mystat.st_mtime;
        if (old_time > new_time) {
          sfile->has_old_header = TRUE;
          lives_free(lives_header);
          return FALSE; // clip has been edited by an older version of LiVES
        }
      }
      lives_free(old_header);
    }
  }

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
  case CLIP_DETAILS_BPP:
  case CLIP_DETAILS_WIDTH:
  case CLIP_DETAILS_HEIGHT:
  case CLIP_DETAILS_ARATE:
  case CLIP_DETAILS_ACHANS:
  case CLIP_DETAILS_ASAMPS:
  case CLIP_DETAILS_FRAMES:
  case CLIP_DETAILS_GAMMA_TYPE:
  case CLIP_DETAILS_HEADER_VERSION:
    *(int *)retval = atoi(val); break;
  case CLIP_DETAILS_ASIGNED:
    *(int *)retval = 0;
    if (sfile->header_version == 0) *(int *)retval = atoi(val);
    if (*(int *)retval == 0 && (!strcasecmp(val, "false"))) *(int *)retval = 1; // unsigned
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    *(int *)retval = atoi(val);
    if (retval == 0) *(int *)retval = 1;
    break;
  case CLIP_DETAILS_PB_ARATE:
    *(int *)retval = atoi(val);
    if (retval == 0) *(int *)retval = sfile->arps;
    break;
  case CLIP_DETAILS_INTERLACE:
    *(int *)retval = atoi(val);
    break;
  case CLIP_DETAILS_FPS:
    *(double *)retval = strtod(val, NULL);
    if (*(double *)retval == 0.) *(double *)retval = prefs->default_fps;
    break;
  case CLIP_DETAILS_PB_FPS:
    *(double *)retval = strtod(val, NULL);
    if (*(double *)retval == 0.) *(double *)retval = sfile->fps;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    if (capable->cpu_bits == 32) {
      *(uint64_t *)retval = (uint64_t)atoll(val);
    } else {
      *(uint64_t *)retval = (uint64_t)atol(val);
    }
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
  default:
    lives_free(val);
    return FALSE;
  }
  lives_free(val);
  return TRUE;
}


boolean save_clip_value(int which, lives_clip_details_t what, void *val) {
  lives_clip_t *sfile;
  char *lives_header;
  char *com, *tmp;
  char *myval;
  char *key;

  boolean needs_sigs = FALSE;

  THREADVAR(write_failed) = 0;
  THREADVAR(com_failed) = FALSE;

  if (which == 0 || which == mainw->scrap_file) return FALSE;

  if (!IS_VALID_CLIP(which)) return FALSE;

  sfile = mainw->files[which];

  /// ascrap_file now uses a different header name; this is to facilitate diskspace cleanup
  /// otherwise it may be wrongly classified as a recoverable clip
  if (which == mainw->ascrap_file)
    lives_header = lives_build_filename(prefs->workdir, sfile->handle, LIVES_ACLIP_HEADER, NULL);
  else
    lives_header = lives_build_filename(prefs->workdir, sfile->handle, LIVES_CLIP_HEADER, NULL);

  key = clip_detail_to_string(what, NULL);

  if (!key) {
    tmp = lives_strdup_printf("Invalid detail %d added for file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
    myval = lives_strdup_printf("%d", *(int *)val);
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
  case CLIP_DETAILS_FRAMES:
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
  case CLIP_DETAILS_HEADER_VERSION:
    myval = lives_strdup_printf("%d", *(int *)val); break;
  default:
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
    if (!mainw->signals_deferred) {
      set_signal_handlers((SignalHandlerPointer)defer_sigint);
      needs_sigs = TRUE;
    }
    com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, lives_header, key, myval);
    lives_system(com, FALSE);
    if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
    if (needs_sigs) set_signal_handlers((SignalHandlerPointer)catch_sigint);
    lives_free(com);
  }

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

