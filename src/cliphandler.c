// cliphandler.c
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "cvirtual.h"

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
    if (which == mainw->ascrap_file) {
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
  char *clipdir, *lives_header;
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
  clipdir = get_clip_dir(which);
  if (which == mainw->ascrap_file)
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
    char *lives_header_bak = lives_strdup_printf("%s.%s", lives_header, LIVES_FILE_EXT_BAK);
    if (!mainw->signals_deferred) {
      set_signal_handlers((SignalHandlerPointer)defer_sigint);
      needs_sigs = TRUE;
    }
    com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, lives_header, key, myval);
    lives_system(com, FALSE);
    lives_cp(lives_header, lives_header_bak);
    if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
    if (needs_sigs) set_signal_handlers((SignalHandlerPointer)catch_sigint);
    lives_free(com);
    lives_free(lives_header_bak);
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


boolean save_clip_values(int which) {
  lives_clip_t *sfile = mainw->files[which];
  char *clipdir, *lives_header_new;
  boolean all_ok = FALSE;
  int asigned, endian;
  int retval;

  if (which == 0 || which == mainw->scrap_file || which == mainw->ascrap_file) return TRUE;

  set_signal_handlers((SignalHandlerPointer)defer_sigint); // ignore ctrl-c

  asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
  endian = sfile->signed_endian & AFORM_BIG_ENDIAN;
  clipdir = get_clip_dir(which);
  if (which == mainw->ascrap_file)
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
        if (!save_clip_value(which, CLIP_DETAILS_BPP, &sfile->bpp)) break;
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
        if (!save_clip_value(which, CLIP_DETAILS_UNIQUE_ID, &sfile->unique_id)) break;
        if (!save_clip_value(which, CLIP_DETAILS_ARATE, &sfile->arps)) break;
        if (!save_clip_value(which, CLIP_DETAILS_PB_ARATE, &sfile->arate)) break;
        if (!save_clip_value(which, CLIP_DETAILS_ACHANS, &sfile->achans)) break;
        if (sfile->achans > 0) {
          if (!save_clip_value(which, CLIP_DETAILS_ASIGNED, &asigned)) break;
          if (!save_clip_value(which, CLIP_DETAILS_AENDIAN, &endian)) break;
        }
        if (!save_clip_value(which, CLIP_DETAILS_ASAMPS, &sfile->asampsize)) break;
        if (!save_clip_value(which, CLIP_DETAILS_FRAMES, &sfile->frames)) break;
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
          if (!save_clip_value(which, CLIP_DETAILS_DECODER_NAME, (void *)dplug->decoder->name)) break;
        }
        all_ok = TRUE;
      } while (FALSE);

      fclose(mainw->clip_header);

      if (!all_ok) {
        retval = do_write_failed_error_s_with_retry(lives_header_new, NULL);
      } else {
        char *lives_header;
        if (which == mainw->ascrap_file)
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

#define DSIZE_MAX 100000

static lives_clip_t *_restore_binfmt(int clipno, boolean forensic) {
  if (IS_NORMAL_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
    char *clipdir = get_clip_dir(clipno);
    char *fname = lives_build_filename(clipdir, TOTALSAVE_NAME, NULL);
    lives_free(clipdir);
    if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
      ssize_t bytes;
      size_t cursize = (size_t)((char *)&sfile->binfmt_end - (char *)sfile), dsize;
      char *xloaded = (char *)lives_calloc(1, cursize);
      boolean badsize = FALSE;
      int fd = lives_open_buffered_rdonly(fname);
      size_t fsize = lives_buffered_orig_size(fd);
      lives_clip_t *loaded = (lives_clip_t *)xloaded;
      if (fsize < cursize) badsize = TRUE;
      else {
        bytes = lives_read_buffered(fd, xloaded, 8, TRUE);
        if (bytes < 8 || lives_memcmp(loaded->binfmt_check.chars, CLIP_BINFMT_CHECK, 8)) badsize = TRUE;
        else {
          bytes += lives_read_buffered(fd, xloaded + 8, 16, TRUE);
          if (bytes < 24) badsize = TRUE;
          else {
            dsize = loaded->binfmt_bytes.num;
            if (dsize < cursize || dsize > fsize) badsize = TRUE;
            else {
              if (dsize > cursize && dsize < DSIZE_MAX) {
                xloaded = lives_realloc(xloaded, dsize);
                loaded = (lives_clip_t *)xloaded;
              } else dsize = cursize;
              bytes += lives_read_buffered(fd, xloaded + 24, dsize - 24, TRUE);
              if (bytes < dsize) badsize = TRUE;
            }
          }
        }
      }
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
      if (sfile->start < 1) sfile->start = 1;
      if (sfile->end > sfile->frames) sfile->end = sfile->frames;
      if (sfile->start > sfile->end) sfile->start = sfile->end;
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
  lives_clip_t *recov = _restore_binfmt(clipno, FALSE);
  if (!recov) return FALSE;
  lives_free(recov);
  return TRUE;
}

lives_clip_t *clip_forensic(int clipno) {
  return  _restore_binfmt(clipno, TRUE);
}


int save_event_frames(void) {
  // when doing a resample, we save a list of frames for the back end to do
  // a reorder

  // here we also update the frame_index for clips of type CLIP_TYPE_FILE

  char *clipdir = get_clip_dir(mainw->current_file);
  char *hdrfile = lives_build_filename(clipdir, "event.frames", NULL);

  int header_fd, i = 0;
  int retval;
  int perf_start, perf_end;
  int nevents;

  lives_free(clipdir);

  if (!cfile->event_list) {
    lives_rm(hdrfile);
    return -1;
  }

  perf_start = (int)(cfile->fps * event_list_get_start_secs(cfile->event_list)) + 1;
  perf_end = perf_start + (nevents = count_events(cfile->event_list, FALSE, 0, 0)) - 1;

  if (!event_list_to_block(cfile->event_list, nevents)) return -1;

  if (cfile->frame_index) {
    LiVESResponseType response;
    int xframes = cfile->frames;
    char *what = (_("creating the frame index for resampling "));

    if (cfile->frame_index_back) lives_free(cfile->frame_index_back);
    cfile->frame_index_back = cfile->frame_index;
    cfile->frame_index = NULL;

    do {
      response = LIVES_RESPONSE_OK;
      create_frame_index(mainw->current_file, FALSE, 0, nevents);
      if (!cfile->frame_index) {
        response = do_memory_error_dialog(what, nevents * 4);
      }
    } while (response == LIVES_RESPONSE_RETRY);
    lives_free(what);
    if (response == LIVES_RESPONSE_CANCEL) {
      cfile->frame_index = cfile->frame_index_back;
      cfile->frame_index_back = NULL;
      return -1;
    }

    for (i = 0; i < nevents; i++) {
      cfile->frame_index[i] = cfile->frame_index_back[(cfile->resample_events + i)->value - 1];
    }

    cfile->frames = nevents;
    if (!check_if_non_virtual(mainw->current_file, 1, cfile->frames)) save_frame_index(mainw->current_file);
    cfile->frames = xframes;
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

      if (cfile->resample_events) {
        for (i = 0; i <= perf_end - perf_start; i++) {
          if (THREADVAR(write_failed)) break;
          lives_write(header_fd, &((cfile->resample_events + i)->value), 4, TRUE);
        }
        lives_freep((void **)&cfile->resample_events);
      }

      if (THREADVAR(write_failed)) {
        retval = do_write_failed_error_s_with_retry(hdrfile, NULL);
      }

      close(header_fd);
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  if (retval == LIVES_RESPONSE_CANCEL) {
    i = -1;
  }

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


boolean read_headers(int fileno, const char *dir, const char *file_name) {
  // file_name is only used to get the file size on the disk
  lives_clip_t *sfile;
  char **array;
  char buff[1024];
  char version[32];
  char *com, *tmp;
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

  off_t sizhead = 28; //8 * 4 + 8 + 8;

  time_t old_time = 0, new_time = 1;
  struct stat mystat;

  if (!IS_VALID_CLIP(fileno)) return FALSE;

  if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

  sfile = mainw->files[fileno];

  old_hdrfile = lives_build_filename(dir, LIVES_CLIP_HEADER_OLD, NULL);

  if (fileno == mainw->ascrap_file) {
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

      detail = CLIP_DETAILS_FRAMES;
      retval = get_clip_value(fileno, detail, &sfile->frames, 0);

      if (retval) {
        detail = CLIP_DETAILS_BPP;
        retval = get_clip_value(fileno, detail, &sfile->bpp, 0);
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
      }
      if (retval) {
        detail = CLIP_DETAILS_HEIGHT;
        retval = get_clip_value(fileno, detail, &sfile->vsize, 0);
      }
      if (retval) {
        if (sfile->header_version > 100) {
          detail = CLIP_DETAILS_GAMMA_TYPE;
          get_clip_value(fileno, detail, &sfile->gamma_type, 0);
          if (sfile->gamma_type == 0) sfile->gamma_type = WEED_GAMMA_SRGB;
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

      if (sfile->achans == 0) retvala = FALSE;
      else retvala = TRUE;

      if (retval && retvala) {
        detail = CLIP_DETAILS_ARATE;
        retvala = get_clip_value(fileno, detail, &sfile->arps, 0);
      }

      if (!retvala) sfile->arps = sfile->achans = sfile->arate = sfile->asampsize = 0;
      if (sfile->arps == 0) retvala = FALSE;

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

  lives_free(lives_header);
  lives_header = NULL;
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
      if (fileno != mainw->current_file) {
        goto rhd_failed;
      }
      retval = do_read_failed_error_s_with_retry(old_hdrfile, lives_strerror(errno));
    } else {
      THREADVAR(read_failed) = FALSE;
      header_size = get_file_size(header_fd);

      if (header_size < sizhead) {
        close(header_fd);
        goto rhd_failed;
      } else {
        THREADVAR(read_failed) = FALSE;
        lives_read_le(header_fd, &sfile->fps, 4, FALSE);
        if (!THREADVAR(read_failed))
          lives_read_le(header_fd, &sfile->bpp, 8, FALSE);
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
  lives_free(com);
  lives_free(tmp);

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
  lives_freep((void **)&lives_header);
  lives_freep((void **)&old_hdrfile);
  return FALSE;
}


LIVES_GLOBAL_INLINE char *get_clip_dir(int clipno) {
  if (IS_VALID_CLIP(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
    if (IS_NORMAL_CLIP(clipno) || sfile->clip_type == CLIP_TYPE_TEMP) {
      return lives_build_path(prefs->workdir, sfile->handle, NULL);
    }
  }
  return NULL;
}


void permit_close(int clipno) {
  // as a safety feature we create a special file which allows the back end to delete the directory
  if (IS_VALID_CLIP(clipno)) {
    char *clipdir = get_clip_dir(clipno);
    char *permitname = lives_build_filename(clipdir, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
    lives_free(clipdir);
#ifdef IS_MINGW
    // kill any active processes: for other OSes the backend does this
    lives_kill_subprocesses(mainw->files[clipno]->handle, TRUE);
#endif
    lives_touch(permitname);
    lives_free(permitname);
  }
}
