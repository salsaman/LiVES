// clip_load_save.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2022 (salsaman+lives@gmail.com)
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <fcntl.h>
#include <glib.h>

#include "main.h"
#include "callbacks.h"
#include "resample.h"
#include "effects.h"
#include "audio.h"
#include "cliphandler.h"
#include "htmsocket.h"
#include "videodev.h"
#include "cvirtual.h"
#include "interface.h"
#include "multitrack-gui.h"
#include "startup.h"

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

const char *get_deinterlace_string(void) {
  if (mainw->open_deint) {
    if (USE_MPV) return "--deinterlace=yes";
    return "-vf pp=ci";
  } else return "";
}


static void save_subs_to_file(lives_clip_t *sfile, char *fname) {
  char *ext;
  lives_subtitle_type_t otype, itype;

  if (!sfile->subt) return;

  itype = sfile->subt->type;

  ext = get_extension(fname);

  if (!strcmp(ext, LIVES_FILE_EXT_SUB)) otype = SUBTITLE_TYPE_SUB;
  else if (!strcmp(ext, LIVES_FILE_EXT_SRT)) otype = SUBTITLE_TYPE_SRT;
  else otype = itype;

  lives_free(ext);

  // TODO - use sfile->subt->save_fn
  switch (otype) {
  case SUBTITLE_TYPE_SUB:
    save_sub_subtitles(sfile, (double)(sfile->start - 1) / sfile->fps, (double)sfile->end / sfile->fps,
                       (double)(sfile->start - 1) / sfile->fps, fname);
    break;

  case SUBTITLE_TYPE_SRT:
    save_srt_subtitles(sfile, (double)(sfile->start - 1) / sfile->fps, (double)sfile->end / sfile->fps,
                       (double)(sfile->start - 1) / sfile->fps, fname);
    break;

  default: return;
  }

  d_print(_("Subtitles were saved as %s\n"), mainw->subt_save_file);
}


boolean get_handle_from_info_file(int index) {
  // called from get_new_handle to get the 'real' file handle
  // because until we know the handle we can't use the normal info file yet
  char *com;
  char *shm_path = get_staging_dir_for(index, ICAP(LOAD));

  if (shm_path)
    com = lives_strdup_printf("%s new \"%s\"", prefs->backend_sync, shm_path);
  else
    com = lives_strdup_printf("%s new", prefs->backend_sync);

  lives_popen(com, FALSE, mainw->msg);
  lives_free(com);

  if (!lives_strncmp(mainw->msg, "error|", 6)) {
    if (shm_path) lives_free(shm_path);
    handle_backend_errors(FALSE);
    return FALSE;
  }

  if (!mainw->files[index]) {
    mainw->files[index] = (lives_clip_t *)(lives_calloc(1, sizeof(lives_clip_t)));
    mainw->files[index]->clip_type = CLIP_TYPE_DISK; // the default
  }

  lives_snprintf(mainw->files[index]->handle, 256, "%s", mainw->msg);

  if (shm_path) {
    lives_snprintf(mainw->files[index]->staging_dir, PATH_MAX, "%s", shm_path);
    lives_free(shm_path);
  }

  return TRUE;
}


void save_frame(LiVESMenuItem *menuitem, livespointer user_data) {
  int frame;
  // save a single frame from a clip
  char *filt[2];
  char *ttl;
  char *filename, *defname;

  filt[0] = lives_strdup_printf("*.%s", get_image_ext_for_type(cfile->img_type));
  filt[1] = NULL;

  frame = LIVES_POINTER_TO_INT(user_data);

  if (frame > 0)
    ttl = lives_strdup_printf(_("Save Frame %d"), frame);

  else
    ttl = (_("Save Frame"));

  defname = lives_strdup_printf("frame%08d.%s", frame, get_image_ext_for_type(cfile->img_type));

  filename = choose_file(*mainw->image_dir ? mainw->image_dir : NULL, defname,
                         filt, LIVES_FILE_CHOOSER_ACTION_SAVE, ttl, NULL);

  lives_free(defname); lives_free(filt[0]); lives_free(ttl);

  if (!filename) return;
  if (!*filename) {
    lives_free(filename);
    return;
  }

  if (!save_frame_inner(mainw->current_file, frame, filename, -1, -1, FALSE)) {
    lives_free(filename);
    return;
  }

  lives_snprintf(mainw->image_dir, PATH_MAX, "%s", filename);
  lives_free(filename);
  get_dirname(mainw->image_dir);
  if (prefs->save_directories) {
    set_utf8_pref(PREF_IMAGE_DIR, mainw->image_dir);
  }
}


static void save_log_file(const char *prefix) {
  int logfd;

  // save the logfile in workdir
#ifndef IS_MINGW
  char *logfile = lives_strdup_printf("%s/%s_%d_%d.txt", prefs->workdir, prefix, lives_getuid(), lives_getgid());
  if ((logfd = creat(logfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) != -1) {
#else
  char *logfile = lives_strdup_printf("%s\\%s_%d_%d.txt", prefs->workdir, prefix, lives_getuid(), lives_getgid());
  if ((logfd = creat(logfile, S_IRUSR | S_IWUSR)) != -1) {
#endif
#if 0
  }
#endif
  char *btext = lives_text_view_get_text(mainw->optextview);
  lives_write(logfd, btext, strlen(btext), TRUE); // not really important if it fails
  lives_free(btext);
  close(logfd);
}
lives_free(logfile);
}


LIVES_GLOBAL_INLINE void set_default_comment(lives_clip_t *sfile, const char *extract) {
  if (!*sfile->comment)
    lives_snprintf(sfile->comment, 1024, "Created with LiVES version %s.\nSee: %s\n%s",
                   LiVES_VERSION, LIVES_WEBSITE, extract);
  if (!*sfile->author && *prefs->def_author)
    lives_snprintf(sfile->author, 1024, "%s", prefs->def_author);
}


void save_clip_audio_values(int clipno) {
  lives_clip_t *sfile = RETURN_PHYSICAL_CLIP(clipno);
  if (sfile) {
    int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
    int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

    sfile->header_version = LIVES_CLIP_HEADER_VERSION;
    save_clip_value(clipno, CLIP_DETAILS_HEADER_VERSION, &sfile->header_version);
    save_clip_value(clipno, CLIP_DETAILS_ACHANS, &sfile->achans);
    save_clip_value(clipno, CLIP_DETAILS_ARATE, &sfile->arps);
    save_clip_value(clipno, CLIP_DETAILS_PB_ARATE, &sfile->arate);
    save_clip_value(clipno, CLIP_DETAILS_ASAMPS, &sfile->asampsize);
    save_clip_value(clipno, CLIP_DETAILS_AENDIAN, &aendian);
    save_clip_value(clipno, CLIP_DETAILS_ASIGNED, &asigned);
  }
}


void save_file(int clip, frames_t start, frames_t end, const char *filename) {
  // save clip from frame start to frame end
  lives_clip_t *sfile = mainw->files[clip], *nfile = NULL;
  double aud_start = 0., aud_end = 0.;

  char *n_file_name = NULL;
  char *fps_string;
  char *extra_params = NULL;
  char *redir = lives_strdup("1>&2 2>" LIVES_DEVNULL);
  char *new_stderr_name = NULL;
  char *mesg, *bit, *tmp;
  char *com, *msg;
  char *full_file_name = NULL;
  char *enc_exec_name = NULL;
  char *clipdir;
  char *cwd;

  frames_t startframe = 1;

  boolean recheck_name = FALSE;

  int new_stderr = -1;
  int retval;
  int current_file = mainw->current_file;
  int asigned = !(sfile->signed_endian & AFORM_UNSIGNED); // 1 is signed (in backend)
  int aendian = (sfile->signed_endian & AFORM_BIG_ENDIAN); // 2 is bigend
  int arate;
  int new_file = -1;
  int oclip = clip;

#ifdef GUI_GTK
  GError *gerr = NULL;
#endif

  struct stat filestat;

  off_t fsize;

  LiVESWidget *hbox;
  frames_t res;

  boolean safe_symlinks = prefs->safe_symlinks;
  boolean not_cancelled = FALSE;
  boolean output_exists = FALSE;
  boolean save_all = FALSE;
  boolean debug_mode = FALSE;

  if (!check_storage_space(clip, FALSE)) return;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_context_update();

  if (start == 1 && end == sfile->frames) save_all = TRUE;

  // new handling for save selection:
  // symlink images 1 - n to the encoded frames
  // symlinks are now created in /tmp (for dynebolic)
  // then encode the symlinked frames

  if (!filename) {
    // prompt for encoder type/output format
    if (prefs->show_rdet) {
      int response;
      rdet = create_render_details(1); // WARNING !! - rdet is global in events.h

      while (1) {
        response = lives_dialog_run(LIVES_DIALOG(rdet->dialog));
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        lives_widget_hide(rdet->dialog);

        if (response == LIVES_RESPONSE_CANCEL) {
          lives_widget_destroy(rdet->dialog);
          lives_free(rdet->encoder_name);
          lives_freep((void **)&rdet);
          lives_freep((void **)&resaudw);
          return;
        }

        clear_mainw_msg();
        // initialise new plugin

        if (enc_exec_name) lives_free(enc_exec_name);
        enc_exec_name = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR,
                                             PLUGIN_ENCODERS, prefs->encoder.name, NULL);
        com = lives_strdup_printf("\"%s\" init", enc_exec_name);
        lives_popen(com, TRUE, mainw->msg);
        lives_free(com);

        if (strcmp(mainw->msg, "initialised\n")) {
          if (*mainw->msg) {
            msg = lives_strdup_printf(_("\n\nThe '%s' plugin reports:\n%s\n"), prefs->encoder.name, mainw->msg);
          } else {
            msg = lives_strdup_printf
                  (_("\n\nUnable to find the 'init' method in the %s plugin.\n"
                     "The plugin may be broken or not installed correctly."), prefs->encoder.name);
          }
          do_error_dialog(msg);
          lives_free(msg);
        } else break;
      }
      if (rdet->debug && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->debug)))
        debug_mode = TRUE;
    }
  }

  if (!enc_exec_name)
    enc_exec_name = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_ENCODERS, prefs->encoder.name, NULL);

  // get file extension
  check_encoder_restrictions(TRUE, FALSE, save_all);

  hbox = lives_hbox_new(FALSE, 0);
  mainw->fx1_bool = TRUE;
  add_suffix_check(LIVES_BOX(hbox), prefs->encoder.of_def_ext);
  lives_widget_show_all(hbox);

  lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);

  if (!filename) {
    char *ttl = (_("Save Clip"));
    do {
      lives_freep((void **)&n_file_name);
      n_file_name = choose_file(mainw->vid_save_dir, NULL, NULL, LIVES_FILE_CHOOSER_ACTION_SAVE, ttl, hbox);
      if (!n_file_name) return;
    } while (!*n_file_name);
    lives_snprintf(mainw->vid_save_dir, PATH_MAX, "%s", n_file_name);
    get_dirname(mainw->vid_save_dir);
    if (prefs->save_directories) {
      set_utf8_pref(PREF_VID_SAVE_DIR, mainw->vid_save_dir);
    }
    lives_free(ttl);
  } else n_file_name = lives_strdup(filename);

  //append default extension (if necessary)
  if (!*prefs->encoder.of_def_ext) {
    // encoder adds its own extension
    get_filename(n_file_name, FALSE);
  } else {
    if (mainw->fx1_bool && (strlen(n_file_name) <= strlen(prefs->encoder.of_def_ext) ||
                            strncmp(n_file_name + strlen(n_file_name) - strlen(prefs->encoder.of_def_ext) - 1, ".", 1) ||
                            strcmp(n_file_name + strlen(n_file_name) - strlen(prefs->encoder.of_def_ext),
                                   prefs->encoder.of_def_ext))) {
      full_file_name = lives_strconcat(n_file_name, ".", prefs->encoder.of_def_ext, NULL);
      recheck_name = TRUE;
    }
  }

  if (!full_file_name) {
    full_file_name = lives_strdup(n_file_name);
  }

  if (!filename && recheck_name) {
    if (!check_file(full_file_name, strcmp(full_file_name, n_file_name))) {
      lives_free(full_file_name);
      lives_free(n_file_name);
      if (rdet) {
        lives_widget_destroy(rdet->dialog);
        lives_free(rdet->encoder_name);
        lives_freep((void **)&rdet);
        lives_freep((void **)&resaudw);
      }
      return;
    }
    sfile->orig_file_name = FALSE;
  }

  if (!*sfile->comment) set_default_comment(sfile, NULL);

  if (!do_comments_dialog(clip, full_file_name)) {
    lives_free(full_file_name);
    if (rdet) {
      lives_widget_destroy(rdet->dialog);
      lives_free(rdet->encoder_name);
      lives_freep((void **)&rdet);
      lives_freep((void **)&resaudw);
    }
    lives_freep((void **)&mainw->subt_save_file);
    return;
  }

  if (rdet) {
    lives_widget_destroy(rdet->dialog);
    lives_freep((void **)&rdet->encoder_name);
    lives_freep((void **)&rdet);
    lives_freep((void **)&resaudw);
  }

  if (sfile->arate * sfile->achans != 0) {
    aud_start = calc_time_from_frame(clip, start) * sfile->arps / sfile->arate;
    aud_end = calc_time_from_frame(clip, end + 1) * sfile->arps / sfile->arate;
  }

  // get extra params for encoder

  if (!sfile->ratio_fps) {
    fps_string = lives_strdup_printf("%.3f", sfile->fps);
  } else {
    fps_string = lives_strdup_printf("%.8f", sfile->fps);
  }

  arate = sfile->arate;

  if (!mainw->save_with_sound || prefs->encoder.of_allowed_acodecs == 0) {
    arate = 0;
  }

  /// get extra parameters for saving
  if (prefs->encoder.capabilities & HAS_RFX) {
    char buff[65536];

    com = lives_strdup_printf("\"%s\" get_rfx %s %lu %d %d", enc_exec_name, prefs->encoder.of_name,
                              prefs->encoder.audio_codec, sfile->hsize, sfile->vsize);
    if (debug_mode) {
      fprintf(stderr, "Running command: %s\n", com);
    }
    lives_popen(com, TRUE, buff);
    lives_free(com);

    if (!THREADVAR(com_failed)) {
      extra_params = plugin_run_param_window(buff, NULL, NULL);
    }
    if (!extra_params) {
      lives_free(fps_string);
      if (!mainw->multitrack) {
        switch_clip(1, current_file, TRUE);
      }
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }
  }

  if (!save_all && !safe_symlinks) {
    // we are saving a selection - make symlinks from a temporary clip

    if ((new_file = mainw->first_free_file) == ALL_USED) {
      too_many_files();
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }

    // create new clip
    if (!get_new_handle(new_file, (_("selection")))) {
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }

    migrate_from_staging(new_file);
    sfile = mainw->files[new_file];

    if (sfile->clip_type == CLIP_TYPE_FILE) {
      mainw->cancelled = CANCEL_NONE;
      sfile->progress_start = 1;
      sfile->progress_end = count_virtual_frames(sfile->frame_index, start, end);
      do_threaded_dialog(_("Pulling frames from clip..."), TRUE);
      res = virtual_to_images(clip, start, end, TRUE, NULL);
      end_threaded_dialog();

      if (mainw->cancelled != CANCEL_NONE || res < 0) {
        mainw->cancelled = CANCEL_USER;
        lives_freep((void **)&mainw->subt_save_file);
        if (res <= 0) d_print_file_error_failed();
        return;
      }
    }

    mainw->effects_paused = FALSE;

    nfile = mainw->files[new_file];
    nfile->hsize = sfile->hsize;
    nfile->vsize = sfile->vsize;
    sfile->progress_start = nfile->start = 1;
    sfile->progress_end = nfile->frames = nfile->end = end - start + 1;
    nfile->fps = sfile->fps;
    nfile->arps = sfile->arps;
    nfile->arate = sfile->arate;
    nfile->achans = sfile->achans;
    nfile->asampsize = sfile->asampsize;
    nfile->signed_endian = sfile->signed_endian;
    nfile->img_type = sfile->img_type;

    com = lives_strdup_printf("%s link_frames \"%s\" %d %d %.8f %.8f %d %d %d %d %d \"%s\"",
                              prefs->backend, nfile->handle, start, end, aud_start,
                              aud_end, nfile->arate, nfile->achans, nfile->asampsize,
                              !(nfile->signed_endian & AFORM_UNSIGNED),
                              !(nfile->signed_endian & AFORM_BIG_ENDIAN), sfile->handle);

    lives_rm(nfile->info_file);
    lives_system(com, FALSE);
    lives_free(com);

    clip = new_file;
    sfile = mainw->files[new_file];

    if (THREADVAR(com_failed)) {
      permit_close(clip);
      lives_system(lives_strdup_printf("%s close \"%s\"", prefs->backend, sfile->handle), TRUE);
      lives_freep((void **)&sfile);
      if (mainw->first_free_file == ALL_USED || mainw->first_free_file > new_file)
        mainw->first_free_file = new_file;
      if (!mainw->multitrack) switch_clip(1, current_file, TRUE);
      d_print_cancelled();
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }

    sfile->nopreview = TRUE;
    if (!(do_progress_dialog(TRUE, TRUE, _("Linking selection")))) {
      permit_close(clip);
      lives_system((tmp = lives_strdup_printf("%s close \"%s\"", prefs->backend, sfile->handle)), TRUE);
      lives_free(tmp);
      lives_freep((void **)&sfile);
      if (mainw->first_free_file == ALL_USED || mainw->first_free_file > new_file)
        mainw->first_free_file = new_file;

      if (!mainw->multitrack) {
        switch_clip(1, current_file, TRUE);
      }
      if (mainw->error) d_print_failed();
      else d_print_cancelled();
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }

    // sfile->arate, etc., would have been reset by calls to do_progress_dialog() which calls get_total_time() [since sfile->afilesize==0]
    // so we need to set these again now that link_frames has provided an actual audio clip

    nfile->arps = sfile->arps;
    nfile->arate = sfile->arate;
    nfile->achans = sfile->achans;
    nfile->asampsize = sfile->asampsize;
    nfile->signed_endian = sfile->signed_endian;

    reget_afilesize(new_file);

    aud_start = calc_time_from_frame(new_file, 1) * nfile->arps / nfile->arate;
    aud_end = calc_time_from_frame(new_file, nfile->frames + 1) * nfile->arps / nfile->arate;
    sfile->nopreview = FALSE;
  } else {
    clip = oclip; // for encoder restns
    sfile = mainw->files[clip];
  }

  if (rdet) rdet->is_encoding = TRUE;

  if (!check_encoder_restrictions(FALSE, FALSE, save_all)) {
    if (!save_all && !safe_symlinks) {
      permit_close(new_file);
      lives_system((com = lives_strdup_printf("%s close \"%s\"", prefs->backend, nfile->handle)), TRUE);
      lives_free(com);
      lives_free(nfile);
      mainw->files[new_file] = NULL;
      if (mainw->first_free_file == ALL_USED || new_file) mainw->first_free_file = new_file;
    }
    if (!mainw->multitrack) {
      switch_clip(1, current_file, TRUE);
    }
    d_print_cancelled();
    lives_freep((void **)&mainw->subt_save_file);
    return;
  }

  if (!save_all && safe_symlinks) {
    int xarps, xarate, xachans, xasamps, xasigned_endian;
    // we are saving a selection - make symlinks in /tmp

    startframe = -1;

    if (sfile->clip_type == CLIP_TYPE_FILE) {
      mainw->cancelled = CANCEL_NONE;
      sfile->progress_start = 1;
      sfile->progress_end = count_virtual_frames(sfile->frame_index, start, end);
      do_threaded_dialog(_("Pulling frames from clip..."), TRUE);
      res = virtual_to_images(clip, start, end, TRUE, NULL);
      end_threaded_dialog();

      if (mainw->cancelled != CANCEL_NONE || res <= 0) {
        if (mainw->cancelled != CANCEL_NONE) mainw->cancelled = CANCEL_USER;
        lives_freep((void **)&mainw->subt_save_file);
        if (res <= 0) d_print_file_error_failed();
        return;
      }
    }

    com = lives_strdup_printf("%s link_frames \"%s\" %d %d %.8f %.8f %d %d %d %d %d",
                              prefs->backend, sfile->handle, start, end, aud_start,
                              aud_end, sfile->arate, sfile->achans, sfile->asampsize,
                              !(sfile->signed_endian & AFORM_UNSIGNED),
                              !(sfile->signed_endian & AFORM_BIG_ENDIAN));

    lives_system(com, FALSE);
    lives_free(com);

    clip = oclip;
    sfile = mainw->files[clip];

    xarps = sfile->arps;
    xarate = sfile->arate;
    xachans = sfile->achans;
    xasamps = sfile->asampsize;
    xasigned_endian = sfile->signed_endian;

    if (THREADVAR(com_failed)) {
      com = lives_strdup_printf("%s clear_symlinks \"%s\"", prefs->backend_sync, sfile->handle);
      lives_system(com, TRUE);
      lives_free(com);
      sfile->nopreview = FALSE;
      if (!mainw->multitrack) {
        switch_clip(1, current_file, TRUE);
      }
      d_print_cancelled();
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }

    sfile->nopreview = TRUE;
    if (!(do_progress_dialog(TRUE, TRUE, _("Linking selection")))) {
      com = lives_strdup_printf("%s clear_symlinks \"%s\"", prefs->backend_sync, sfile->handle);
      lives_system(com, TRUE);
      lives_free(com);
      sfile->nopreview = FALSE;
      if (!mainw->multitrack) {
        switch_clip(1, current_file, TRUE);
      }
      if (mainw->error) d_print_failed();
      else d_print_cancelled();
      lives_freep((void **)&mainw->subt_save_file);
      return;
    }

    // sfile->arate, etc., would have been reset by calls to do_progress_dialog()
    // which calls get_total_time() [since sfile->afilesize==0]
    // so we need to set these again now that link_frames has provided an actual audio clip

    sfile->arps = xarps;
    sfile->arate = xarate;
    sfile->achans = xachans;
    sfile->asampsize = xasamps;
    sfile->signed_endian = xasigned_endian;

    reget_afilesize(clip);

    aud_start = calc_time_from_frame(clip, 1) * sfile->arps / sfile->arate;
    aud_end = calc_time_from_frame(clip, end - start + 1) * sfile->arps / sfile->arate;
    sfile->nopreview = FALSE;
  }

  if (save_all) {
    if (sfile->clip_type == CLIP_TYPE_FILE) {
      frames_t ret;
      char *msg = (_("Pulling frames from clip..."));
      if ((ret = realize_all_frames(clip, msg, FALSE, 1, 0)) < sfile->frames) {
        lives_free(msg);
        lives_freep((void **)&mainw->subt_save_file);
        if (ret > 0) d_print_cancelled();
        if (!mainw->multitrack) {
          switch_clip(1, current_file, TRUE);
        }
        return;
      }
      lives_free(msg);
    }
  }

  if (!mainw->save_with_sound || prefs->encoder.of_allowed_acodecs == 0) {
    bit = (_(" (with no sound)\n"));
  } else {
    bit = lives_strdup("\n");
  }

  if (!save_all) {
    mesg = lives_strdup_printf(_("Saving frames %d to %d%s as \"%s\" : encoder = %s : format = %s..."),
                               start, end, bit, full_file_name, prefs->encoder.name,
                               prefs->encoder.of_desc);
  } // end selection
  else {
    mesg = lives_strdup_printf(_("Saving frames 1 to %d%s as \"%s\" : encoder %s : format = %s..."),
                               sfile->frames, bit, full_file_name, prefs->encoder.name,
                               prefs->encoder.of_desc);
  }
  lives_free(bit);

  mainw->no_switch_dprint = TRUE;
  d_print(mesg);
  mainw->no_switch_dprint = FALSE;
  lives_free(mesg);

  clipdir = get_clip_dir(clip);

  if (prefs->show_gui && !debug_mode) {
    // open a file for stderr
    new_stderr_name = lives_build_filename(clipdir, LIVES_ENC_DEBUG_FILE_NAME, NULL);
    lives_free(redir);

    do {
      retval = 0;
      new_stderr = lives_open3(new_stderr_name, O_CREAT | O_RDONLY | O_TRUNC
                               | O_SYNC, S_IRUSR | S_IWUSR);
      if (new_stderr < 0) {
        retval = do_write_failed_error_s_with_retry(new_stderr_name, lives_strerror(errno));
        if (retval == LIVES_RESPONSE_CANCEL) redir = lives_strdup("1>&2");
      } else {

#ifdef IS_MINGW

#ifdef GUI_GTK
        mainw->iochan = g_io_channel_win32_new_fd(new_stderr);
#endif
        redir = lives_strdup_printf("2>&1 >\"%s\"", new_stderr_name);
#else
#ifdef GUI_GTK
        mainw->iochan = g_io_channel_unix_new(new_stderr);
#endif
        redir = lives_strdup_printf("2>\"%s\"", new_stderr_name);
#endif

#ifdef GUI_QT
        mainw->iochan = new QFile;
        mainw->iochan->open(new_stderr, QIODevice::ReadOnly);
#endif

#ifdef GUI_GTK
        g_io_channel_set_encoding(mainw->iochan, NULL, NULL);
        g_io_channel_set_buffer_size(mainw->iochan, 0);
        g_io_channel_set_flags(mainw->iochan, G_IO_FLAG_NONBLOCK, &gerr);
        if (gerr) lives_error_free(gerr);
        gerr = NULL;
#endif
        mainw->optextview = create_output_textview();
      }
    } while (retval == LIVES_RESPONSE_RETRY);
  } else {
    lives_free(redir);
    redir = lives_strdup("1>&2");
  }

  if (lives_file_test((tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)),
                      LIVES_FILE_TEST_EXISTS)) {
    lives_rm(tmp);
  }
  lives_free(tmp);

  /// re-read values in case they were resampled

  if (arate != 0) arate = sfile->arate;

  if (!sfile->ratio_fps) {
    fps_string = lives_strdup_printf("%.3f", sfile->fps);
  } else {
    fps_string = lives_strdup_printf("%.8f", sfile->fps);
  }

  // if startframe is -ve, we will use the links created for safe_symlinks - in /tmp
  // for non-safe symlinks, sfile will be our new links file
  // for save_all, sfile will be sfile

  if (prefs->encoder.capabilities & ENCODER_NON_NATIVE) {
    com = lives_strdup_printf("%s save \"%s\" \"%s\" \"%s\" \"%s\" %d %d %d %d %d %d "
                              "%.4f %.4f %s %s", prefs->backend,
                              sfile->handle, enc_exec_name, fps_string,
                              (tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)),
                              startframe, sfile->frames, arate, sfile->achans, sfile->asampsize,
                              asigned | aendian, aud_start, aud_end, (extra_params == NULL) ? ""
                              : extra_params, redir);
  } else {
    com = lives_strdup_printf("%s save \"%s\" \"native:%s\" \"%s\" \"%s\" %d %d %d %d %d %d %.4f "
                              "%.4f %s %s", prefs->backend, sfile->handle, enc_exec_name, fps_string,
                              (tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)),
                              startframe, sfile->frames, arate, sfile->achans, sfile->asampsize,
                              asigned | aendian, aud_start, aud_end, (extra_params == NULL) ? ""
                              : extra_params, redir);
  }
  lives_free(tmp); lives_free(fps_string);
  lives_freep((void **)&extra_params);

  mainw->effects_paused = FALSE;
  sfile->nokeep = TRUE;

  lives_rm(sfile->info_file);
  THREADVAR(write_failed) = FALSE;
  save_file_comments(current_file);

  if (debug_mode) {
    fprintf(stderr, "Running command: %s\n", com);
  }

  lives_system(com, FALSE);
  lives_free(com);
  mainw->error = FALSE;

  if (THREADVAR(com_failed) || THREADVAR(write_failed)) {
    mainw->error = TRUE;
  }

  if (!mainw->error) {
    //char *pluginstr;

    sfile->progress_start = 1;
    sfile->progress_end = sfile->frames;

    not_cancelled = do_progress_dialog(TRUE, TRUE, _("Saving [can take a long time]"));

    if (mainw->iochan) {
      /// flush last of stdout/stderr from plugin

      lives_fsync(new_stderr);
      pump_io_chan(mainw->iochan);

#ifdef GUI_GTK
      g_io_channel_shutdown(mainw->iochan, FALSE, &gerr);
      g_io_channel_unref(mainw->iochan);
      if (gerr) lives_error_free(gerr);
#endif
#ifdef GUI_QT
      delete mainw->iochan;
#endif
      mainw->iochan = NULL;

      close(new_stderr);
      lives_rm(new_stderr_name);
      lives_free(new_stderr_name);
      lives_free(redir);
    }

    mainw->effects_paused = FALSE;
    sfile->nokeep = FALSE;
  } else {
    if (mainw->iochan) {
      /// flush last of stdout/stderr from plugin

      lives_fsync(new_stderr);
      pump_io_chan(mainw->iochan);

#ifdef GUI_GTK
      g_io_channel_shutdown(mainw->iochan, FALSE, &gerr);
      g_io_channel_unref(mainw->iochan);
      if (gerr) lives_error_free(gerr);
#endif
#ifdef GUI_QT
      delete mainw->iochan;
#endif
      mainw->iochan = NULL;
      close(new_stderr);
      lives_rm(new_stderr_name);
      lives_free(new_stderr_name);
      lives_free(redir);
    }
  }

  cwd = lives_get_current_dir();

  lives_chdir(clipdir, FALSE);
  lives_free(clipdir);

  com = lives_strdup_printf("\"%s\" clear", enc_exec_name);

  if (debug_mode) {
    fprintf(stderr, "Running command: %s\n", com);
  }
  lives_system(com, FALSE);
  lives_free(com);

  lives_chdir(cwd, FALSE);
  lives_free(cwd);

  lives_free(enc_exec_name);

  if (not_cancelled || mainw->error) {
    if (mainw->error) {
      mainw->no_switch_dprint = TRUE;
      d_print_failed();
      mainw->no_switch_dprint = FALSE;
      lives_free(full_file_name);
      if (!save_all && !safe_symlinks) {
        permit_close(clip);
        lives_system((com = lives_strdup_printf("%s close \"%s\"", prefs->backend, sfile->handle)),
                     TRUE);
        lives_free(com);
        lives_freep((void **)&sfile);
        if (mainw->first_free_file == ALL_USED || mainw->first_free_file > clip)
          mainw->first_free_file = clip;
      } else if (!save_all && safe_symlinks) {
        com = lives_strdup_printf("%s clear_symlinks \"%s\"", prefs->backend_sync, sfile->handle);
        lives_system(com, TRUE);
        lives_free(com);
      }

      switch_clip(1, current_file, TRUE);

      lives_freep((void **)&mainw->subt_save_file);
      sensitize();
      return;
    }

    if (lives_file_test((tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)),
                        LIVES_FILE_TEST_EXISTS)) {
      lives_free(tmp);
      stat((tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)), &filestat);
      if (filestat.st_size > 0) output_exists = TRUE;
    }
    if (!output_exists) {
      lives_free(tmp);

      mainw->no_switch_dprint = TRUE;
      d_print_failed();
      mainw->no_switch_dprint = FALSE;
      lives_free(full_file_name);
      if (!save_all && !safe_symlinks) {
        permit_close(clip);
        lives_system((com = lives_strdup_printf("%s close \"%s\"", prefs->backend, sfile->handle)), TRUE);
        lives_free(com);
        lives_freep((void **)&sfile);
        if (mainw->first_free_file == ALL_USED || mainw->first_free_file > clip)
          mainw->first_free_file = clip;
      } else if (!save_all && safe_symlinks) {
        com = lives_strdup_printf("%s clear_symlinks \"%s\"", prefs->backend_sync, sfile->handle);
        lives_system(com, TRUE);
        lives_free(com);
      }

      if (!mainw->multitrack) {
        switch_clip(1, current_file, TRUE);
      }
      retval = do_error_dialog(_("\n\nEncoder error - output file was not created !\n"));

      if (retval == LIVES_RESPONSE_SHOW_DETAILS) {
        /// show iochan (encoder) details
        on_details_button_clicked();
      }

      if (mainw->iochan) {
        save_log_file("failed_encoder_log");
        mainw->iochan = NULL;
        lives_widget_object_unref(mainw->optextview);
      }

      lives_freep((void **)&mainw->subt_save_file);
      sensitize();
      if (mainw->error) d_print_failed();

      return;
    }
    lives_free(tmp);

    if (save_all) {
      if (prefs->enc_letterbox) {
        /// replace letterboxed frames with maxspect frames
        int iwidth = sfile->ohsize;
        int iheight = sfile->ovsize;
        boolean bad_header = FALSE;

        com = lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\" 1", prefs->backend,
                                  sfile->handle, 1, sfile->frames,
                                  get_image_ext_for_type(sfile->img_type));

        lives_rm(sfile->info_file);
        lives_system(com, FALSE);

        do_progress_dialog(TRUE, FALSE, _("Clearing letterbox"));

        if (mainw->error) {
          //	  sfile->may_be_damaged=TRUE;
          d_print_failed();
          return;
        }

        calc_maxspect(sfile->hsize, sfile->vsize, &iwidth, &iheight);

        sfile->hsize = iwidth;
        sfile->vsize = iheight;

        save_clip_value(clip, CLIP_DETAILS_WIDTH, &sfile->hsize);
        if (THREADVAR(com_failed) || THREADVAR(write_failed)) bad_header = TRUE;
        save_clip_value(clip, CLIP_DETAILS_HEIGHT, &sfile->vsize);
        if (THREADVAR(com_failed) || THREADVAR(write_failed)) bad_header = TRUE;
        if (bad_header) do_header_write_error(clip);
      }

      lives_snprintf(sfile->save_file_name, PATH_MAX, "%s", full_file_name);
      sfile->changed = FALSE;

      /// save was successful
      /// TODO - check for size < 0 !!!
      fsize = sget_file_size(full_file_name);
      if (fsize < 0) fsize = 0;
      sfile->f_size = (size_t)fsize;

      if (sfile->is_untitled) {
        sfile->is_untitled = FALSE;
      }
      if (!sfile->was_renamed) {
        lives_menu_item_set_text(sfile->menuentry, full_file_name, FALSE);
        lives_snprintf(sfile->name, CLIP_NAME_MAXLEN, "%s", full_file_name);
      }
      set_main_title(sfile->name, 0);
      if (prefs->show_recent) {
        add_to_recent(full_file_name, 0., 0, NULL);
        global_recent_manager_add(full_file_name);
      }
    } else {
      if (!safe_symlinks) {
        permit_close(new_file);
        lives_system((com = lives_strdup_printf("%s close \"%s\"",
                                                prefs->backend, nfile->handle)), TRUE);
        lives_free(com);
        lives_free(nfile);
        mainw->files[new_file] = NULL;
        if (mainw->first_free_file == ALL_USED || mainw->first_free_file > clip)
          mainw->first_free_file = new_file;
      } else {
        com = lives_strdup_printf("%s clear_symlinks \"%s\"", prefs->backend_sync, sfile->handle);
        lives_system(com, TRUE);
        lives_free(com);
      }
    }
  }

  if (!mainw->multitrack) {
    switch_clip(1, current_file, TRUE);
  }
  if (mainw->iochan) {
    save_log_file("encoder_log");
    lives_widget_object_unref(mainw->optextview);
    mainw->iochan = NULL;
  }

  if (not_cancelled) {
    char *fsize_ds;
    mainw->no_switch_dprint = TRUE;
    d_print_done();

    /// get size of file and show it

    fsize = sget_file_size(full_file_name);
    if (fsize >= 0) {
      /// TODO - handle file errors !!!!!

      fsize_ds = lives_format_storage_space_string(fsize);
      d_print(_("File size was %s\n"), fsize_ds);
      lives_free(fsize_ds);

      if (mainw->subt_save_file) {
        save_subs_to_file(sfile, mainw->subt_save_file);
        lives_freep((void **)&mainw->subt_save_file);
      }
    }
    mainw->no_switch_dprint = FALSE;

    lives_notify(LIVES_OSC_NOTIFY_SUCCESS,
                 (mesg = lives_strdup_printf("encode %d \"%s\"", clip,
                         (tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)))));
    lives_free(tmp);
    lives_free(mesg);
  } else {
    lives_rm((tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)));
    lives_free(tmp);
  }

  lives_free(full_file_name);
}

/**
   @brief close cfile and switch to new clip (may be -1)

   note this only closes the disk and basic resources, it does not affect the interface
   (c.f. close_current_file())
   returns new_clip */

int close_temp_handle(int new_clip) {
  char *com;
  char *temp_backend, *clipd;
  int clipno = mainw->current_file;

  if (!IS_VALID_CLIP(new_clip)) new_clip = -1;
  if (!IS_VALID_CLIP(clipno)) {
    mainw->current_file = new_clip;
    return new_clip;
  }
  if (cfile->clip_type != CLIP_TYPE_TEMP
      && mainw->current_file != mainw->scrap_file && mainw->current_file != mainw->ascrap_file) {
    close_current_file(new_clip);
    return new_clip;
  }

  clipd = get_clip_dir(mainw->current_file);
  if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
    lives_cancel_t cancelled = mainw->cancelled;
    permit_close(mainw->current_file);

    temp_backend = use_staging_dir_for(mainw->current_file);
    com = lives_strdup_printf("%s close \"%s\"", temp_backend, cfile->handle);
    lives_system(com, TRUE);
    lives_free(com);
    lives_free(temp_backend);
    mainw->cancelled = cancelled;
  }

  srcgrps_free_all(clipno);

  lives_freep((void **)&mainw->files[clipno]);

  mainw->current_file = new_clip;

  if (mainw->first_free_file == ALL_USED || mainw->first_free_file > clipno)
    mainw->first_free_file = clipno;
  return new_clip;
}


/**
   @brief get a temp "handle" from disk.

   Call this to get a temp handle for returning info from the backend
   (this is deprecated for simple data, use lives_popen() instead whenever possible)

   This function is also called from get_new_handle() to create a permanent handle
   for an opened file.

   there are two special instances when this is called with an index != -1:
   - when saving a set and a clip is moved from outside the set to inside it.
   we need a new handle which is guaranteed unique for the set, but we retain all the other details
   - when called from get_new_handle() to create the disk part of a clip

   otherwise, index should be passed in as -1 (the normal case)
   -- handle will be fetched and a directory created in workdir.
   -- clip_type is set to CLIP_TYPE_TEMP.
   call close_temp_handle() on it after use, then restore mainw->current_file

   function returns FALSE if write to workdir fails.

   WARNING:
   this function changes mainw->current_file, unless it returns FALSE (could not create cfile)

   get_new_handle() calls this with the index value passed to it, which should not be -1,
   sets defaults for the clip,
   and also sets the clip name and filename. That function should be used instead to create permanent clips. */
boolean get_temp_handle(int index) {
  boolean is_unique, create = FALSE;

  if (CURRENT_CLIP_IS_TEMP) {
    BREAK_ME("temp clip in temp clip !!");
    return TRUE;
  }

  if (index < -1 || index > MAX_FILES) {
    char *msg = lives_strdup_printf("Attempt to create invalid new temp clip %d\n", index);
    LIVES_WARN(msg);
    lives_free(msg);
    return FALSE;
  }

  if (index == -1) {
    if (mainw->first_free_file == ALL_USED) {
      too_many_files();
      return FALSE;
    }
    create = TRUE;
    index = mainw->first_free_file;
    get_next_free_file();
  }

  do {
    is_unique = TRUE;

    // get handle from info file, the first time we will also malloc a
    // new "file" struct here and create a directory in prefs->workdir
    if (!get_handle_from_info_file(index)) {
      lives_freep((void **)&mainw->files[index]);
      if (mainw->first_free_file == ALL_USED || index < mainw->first_free_file)
        mainw->first_free_file = index;
      return FALSE;
    }

    if (*mainw->set_name) {
      char *setclipdir = CURRENT_SET_CLIP_DIR(mainw->files[index]->handle);
      if (lives_file_test(setclipdir, LIVES_FILE_TEST_IS_DIR)) is_unique = FALSE;
      lives_free(setclipdir);
    }
  } while (!is_unique);

  mainw->current_file = index;

  if (create) {
    // for tmep handles, create a marker file in directory, else we will be barred from
    // removing it
    permit_close(mainw->current_file);
    // fill with default values
    create_cfile(index, cfile->handle, FALSE);
    cfile->clip_type = CLIP_TYPE_TEMP;
  }
  return TRUE;
}


boolean get_new_handle(int index, const char *name) {
  // here is where we first initialize for the clipboard
  // and for paste_as_new, and restore, etc.
  // pass in name as NULL or "" and it will be set with an untitled number

  // this function *does not* change mainw->current_file (except briefly), or add to the menu
  // or update mainw->clips_available

  // differences from get_temp_handle:
  // - here we don't switch clips;
  // - index is normally passed in rather than generated (pulled from next_free_file) - this allows
  //     the caller to know the index number and do preconfig before calling
  // - we set name and file_name from the name parameter, or if name is NULL, we set an untitled name
  //        and increment mainw->untitled_number
  // - the clip should be closed using close_current_file() instead of close_temp_handle()

  // - if the clip changes from temp. to normal clip, or before doing any large writes to it (e.g decoding frames into it)
  //     then migrate_from_staging(fileno) must be called, This is because all files are now created by default in a "staging directory"
  //     which would be something like /dev/shm - fast access but only for small amounts of data. migrate_from_staging, causes
  // the clip directory to be migrated to normal workdir. get_clip_dir() and use_staging_for() must be used before the migration is done.

  char *xname;

  int current_file = mainw->current_file;

  // if TRUE, changes mainw->current_file (and hence cfile)
  if (!get_temp_handle(index)) return FALSE;

  // setup would have been done already in get_temp_handle()
  if (index == -1) index = mainw->current_file;

  else create_cfile(index, cfile->handle, FALSE);

  // note : don't need to update first_free_file for the clipboard
  // because we used index 0 instead of a free index number
  if (index != 0) {
    get_next_free_file();
  }

  if (!name || !*name) {
    cfile->is_untitled = TRUE;
    xname = get_untitled_name(mainw->untitled_number++);
  } else xname = lives_strdup(name);

  lives_snprintf(cfile->file_name, PATH_MAX, "%s", xname);
  lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "%s", xname);

  mainw->current_file = current_file;

  lives_free(xname);
  return TRUE;
}


boolean add_file_info(const char *check_handle, boolean aud_only) {
  // file information has been retrieved, set struct cfile with details
  // contained in mainw->msg. We do this twice, once before opening the file, once again after.
  // The first time, frames and afilesize may not be correct.
  char *mesg, *mesg1;
  char **array;
  char *temp_backend;

  if (aud_only && !mainw->save_with_sound) {
    cfile->arps = cfile->arate = cfile->achans = cfile->asampsize = 0;
    cfile->afilesize = 0l;
    return TRUE;
  }

  temp_backend = use_staging_dir_for(mainw->current_file);
  if (!strcmp(mainw->msg, "killed")) {
    char *com;
    // user pressed "enough"
    // just in case last frame is damaged, we delete it (physically,
    // otherwise it will get dragged in when the file is opened)
    if (!get_primary_src(mainw->current_file)) {
      cfile->frames = get_frame_count(mainw->current_file, cfile->opening_frames);
      if (cfile->frames > 1) {
        com = lives_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
                                  temp_backend, cfile->handle, cfile->frames, cfile->frames,
                                  FALSE, cfile->frames, get_image_ext_for_type(cfile->img_type),
                                  0., 0, 0, 0);
        lives_system(com, FALSE);
        lives_free(com);
        cfile->frames--;
      }
    }

    // commit audio
    com = lives_strdup_printf("%s commit_audio \"%s\" 1", temp_backend, cfile->handle);
    lives_free(temp_backend);
    lives_system(com, TRUE);
    lives_free(com);

    wait_for_bg_audio_sync(mainw->current_file);

    reget_afilesize(mainw->current_file);
    d_print_enough(cfile->frames);
  } else {
    if (check_handle) {
      int npieces = get_token_count(mainw->msg, '|');
      if (npieces < 2) return FALSE;

      array = lives_strsplit(mainw->msg, "|", npieces);

      if (!strcmp(array[0], "error")) {
        if (npieces >= 3) {
          mesg = lives_strdup_printf(_("\nAn error occurred doing\n%s\n"), array[2]);
          LIVES_ERROR(array[2]);
        } else mesg = (_("\nAn error occurred opening the file\n"));
        widget_opts.non_modal = TRUE;
        do_error_dialog(mesg);
        widget_opts.non_modal = FALSE;
        lives_free(mesg);
        lives_strfreev(array);
        return FALSE;
      }

      // sanity check handle against status file
      // (this should never happen...)
      if (strcmp(check_handle, array[1])) {
        LIVES_ERROR("Handle!=statusfile !");
        mesg = lives_strdup_printf(_("\nError getting file info for clip %s.\n"
                                     "Bad things may happen with this clip.\n"),
                                   check_handle);
        widget_opts.non_modal = TRUE;
        do_error_dialog(mesg);
        widget_opts.non_modal = FALSE;
        lives_free(mesg);
        lives_strfreev(array);
        return FALSE;
      }

      cfile->arps = cfile->arate = atoi(array[9]);
      cfile->achans = atoi(array[10]);

      cfile->asampsize = atoi(array[11]);
      cfile->signed_endian = get_signed_endian(atoi(array[12]), atoi(array[13]));
      cfile->afilesize = strtol(array[14], NULL, 10);

      if (aud_only) {
        lives_strfreev(array);
        return TRUE;
      }

      cfile->frames = atoi(array[2]);
      if (aud_only) {
        lives_strfreev(array);
        return TRUE;
      }
      lives_snprintf(cfile->type, 40, "%s", array[3]);
      cfile->hsize = atoi(array[4]);
      cfile->vsize = atoi(array[5]);
      cfile->bpp = atoi(array[6]);
      cfile->pb_fps = cfile->fps = lives_strtod(array[7]);
      cfile->f_size = strtol(array[8], NULL, 10);

      if (npieces > 15 && array[15]) {
        if (prefs->btgamma) {
          if (!strcmp(array[15], "bt709")) cfile->gamma_type = WEED_GAMMA_BT709;
        }
      }

      if (!*cfile->title && npieces > 16 && array[16]) {
        lives_snprintf(cfile->title, 1024, "%s", lives_strstrip(array[16]));
      }
      if (!*cfile->author && npieces > 17 && array[17]) {
        lives_snprintf(cfile->author, 1024, "%s", lives_strstrip(array[17]));
      }
      if (!*cfile->comment && npieces > 18 && array[18]) {
        lives_snprintf(cfile->comment, 1024, "%s", lives_strstrip(array[18]));
      }

      lives_strfreev(array);
    }
  }

  cfile->video_time = 0;

  if (!mainw->save_with_sound) {
    cfile->arps = cfile->arate = cfile->achans = cfile->asampsize = 0;
    cfile->afilesize = 0l;
  }

  if (cfile->frames <= 0) {
    if (cfile->afilesize == 0l && cfile->is_loaded) {
      // we got no video or audio...
      return FALSE;
    }
    cfile->start = cfile->end = cfile->undo_start = cfile->undo_end = 0;
  } else {
    // start with all selected
    cfile->start = 1;
    cfile->end = cfile->frames;
    cfile->undo_start = cfile->start;
    cfile->undo_end = cfile->end;
  }

  cfile->orig_file_name = TRUE;
  cfile->is_untitled = FALSE;

  // some files give us silly frame rates, even single frames...
  // fps of 1000. is used for some streams (i.e. play each frame as it is received)
  if (cfile->fps == 0. || cfile->fps == 1000. || (cfile->frames < 2 && cfile->is_loaded)) {

    if ((cfile->afilesize * cfile->asampsize * cfile->arate * cfile->achans == 0) || cfile->frames < 2) {
      if (cfile->frames != 1) {
        d_print(_("\nPlayback speed not found or invalid ! Using default fps of %.3f fps. \n"
                  "Default can be set in Tools | Preferences | Misc.\n"),
                prefs->default_fps);
      }
      cfile->pb_fps = cfile->fps = prefs->default_fps;
    } else {
      cfile->laudio_time = cfile->raudio_time = cfile->afilesize / cfile->asampsize * 8. / cfile->arate / cfile->achans;
      cfile->pb_fps = cfile->fps = 1. * (int)(cfile->frames / cfile->laudio_time);
      if (cfile->fps > FPS_MAX || cfile->fps < 1.) {
        cfile->pb_fps = cfile->fps = prefs->default_fps;
      }
      d_print(_("Playback speed was adjusted to %.3f frames per second to fit audio.\n"), cfile->fps);
    }
  }

  cfile->video_time = (double)cfile->frames / cfile->fps;

  if (cfile->opening) return TRUE;

  if ((!strcmp(cfile->type, LIVES_IMAGE_TYPE_JPEG) || !strcmp(cfile->type, LIVES_IMAGE_TYPE_PNG))) {
    mesg = (_("Image format detected"));
    d_print(mesg);
    lives_free(mesg);
    return TRUE;
  }

  if (cfile->bpp == 256) {
    mesg1 = lives_strdup_printf(_("Frames=%d type=%s size=%dx%d *bpp=Greyscale* fps=%.3f\nAudio:"), cfile->frames,
                                cfile->type, cfile->hsize, cfile->vsize, cfile->fps);
  } else {
    if (cfile->bpp != 32) cfile->bpp = 24; // assume RGB24  *** TODO - check
    mesg1 = lives_strdup_printf(_("Frames=%d type=%s size=%dx%d bpp=%d fps=%.3f\nAudio:"), cfile->frames,
                                cfile->type, cfile->hsize, cfile->vsize, cfile->bpp, cfile->fps);
  }

  if (cfile->achans == 0) {
    mesg = lives_strdup_printf(_("%s none\n"), mesg1);
  } else {
    mesg = lives_strdup_printf(P_("%s %d Hz %d channel %d bps\n", "%s %d Hz %d channels %d bps\n",
                                  cfile->achans),
                               mesg1, cfile->arate, cfile->achans, cfile->asampsize);
  }
  d_print(mesg);
  lives_free(mesg1);
  lives_free(mesg);

  // get the author,title,comments
  if (*cfile->author) {
    d_print(_(" - Author: %s\n"), cfile->author);
  }
  if (*cfile->title) {
    d_print(_(" - Title: %s\n"), cfile->title);
  }
  if (*cfile->comment) {
    d_print(_(" - Comment: %s\n"), cfile->comment);
  }

  return TRUE;
}


boolean save_file_comments(int fileno) {
  // save the comments etc for smogrify
  int retval;
  int comment_fd;

  char *clipdir = get_clip_dir(mainw->current_file);
  char *comment_file = lives_build_path(clipdir, COMMENT_FILENAME, NULL);
  lives_clip_t *sfile = mainw->files[fileno];

  lives_free(clipdir);
  lives_rm(comment_file);

  do {
    retval = 0;
    comment_fd = creat(comment_file, S_IRUSR | S_IWUSR);
    if (comment_fd < 0) {
      THREADVAR(write_failed) = TRUE;
      retval = do_write_failed_error_s_with_retry(comment_file, lives_strerror(errno));
    } else {
      THREADVAR(write_failed) = FALSE;
      lives_write(comment_fd, sfile->title, strlen(sfile->title), TRUE);
      lives_write(comment_fd, "||%", 3, TRUE);
      lives_write(comment_fd, sfile->author, strlen(sfile->author), TRUE);
      lives_write(comment_fd, "||%", 3, TRUE);
      lives_write(comment_fd, sfile->comment, strlen(sfile->comment), TRUE);

      close(comment_fd);

      if (THREADVAR(write_failed)) {
        retval = do_write_failed_error_s_with_retry(comment_file, NULL);
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(comment_file);

  if (THREADVAR(write_failed)) return FALSE;

  return TRUE;
}


boolean save_frame_inner(int clip, frames_t frame, const char *file_name, int width, int height, boolean from_osc) {
  // save 1 frame as an image
  // width==-1, height==-1 to use "natural" values
  LiVESResponseType resp;
  lives_clip_t *sfile = mainw->files[clip];
  char full_file_name[PATH_MAX];
  char *com, *tmp;

  boolean allow_over = FALSE;

  if (!from_osc && strrchr(file_name, '.') == NULL) {
    lives_snprintf(full_file_name, PATH_MAX, "%s.%s", file_name,
                   get_image_ext_for_type(sfile->img_type));
  } else {
    lives_snprintf(full_file_name, PATH_MAX, "%s", file_name);
    if (!allow_over) allow_over = TRUE;
  }

  // TODO - allow overwriting in sandbox
  if (from_osc && lives_file_test(full_file_name, LIVES_FILE_TEST_EXISTS)) return FALSE;

  tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL);

  if (!mainw->multitrack) {
    d_print(_("Saving frame %d as %s..."), frame, full_file_name);

    if (sfile->clip_type == CLIP_TYPE_FILE) {
      frames_t res = virtual_to_images(clip, frame, frame, FALSE, NULL);
      if (res <= 0) {
        d_print_file_error_failed();
        return FALSE;
      }
    }

    do {
      resp = LIVES_RESPONSE_NONE;

      com = lives_strdup_printf("%s save_frame %s %d \"%s\" %d %d", prefs->backend_sync, sfile->handle,
                                frame, tmp, width, height);
      lives_system(com, FALSE);
      lives_free(com);

      if (THREADVAR(write_failed)) {
        THREADVAR(write_failed) = 0;
        d_print_file_error_failed();
        resp = do_file_perm_error(tmp, TRUE);
        if (resp == LIVES_RESPONSE_CANCEL) {
          return FALSE;
        }
      }
      if (!THREADVAR(com_failed)) {
        lives_free(tmp);
        d_print_done();
      }
    } while (resp == LIVES_RESPONSE_RETRY);
  } else {
    // multitrack mode
    weed_layer_t *layer;
    LiVESError *gerr = NULL;
    LiVESPixbuf *pixbuf = NULL;
    LiVESResponseType retval;
    short pbq = prefs->pb_quality;
    boolean internal = FALSE;

    if (sfile->img_type == IMG_TYPE_PNG) internal = TRUE;

    prefs->pb_quality = PB_QUALITY_BEST;

    layer = mt_show_current_frame(mainw->multitrack, TRUE);

    resize_layer(layer, sfile->hsize, sfile->vsize, LIVES_INTERP_BEST, WEED_PALETTE_RGB24, 0);
    convert_layer_palette(layer, WEED_PALETTE_RGB24, 0);

    if (!internal) pixbuf = layer_to_pixbuf(layer, TRUE, FALSE);

    do {
      retval = LIVES_RESPONSE_NONE;
      if (internal) {
        layer_to_png(layer, tmp, 100 - prefs->ocp);
        if (THREADVAR(write_failed)) {
          THREADVAR(write_failed) = 0;
          retval = do_write_failed_error_s_with_retry(full_file_name, NULL);
        }
      } else {
        pixbuf_to_png(pixbuf, tmp, IMG_TYPE_JPEG, 100, sfile->hsize, sfile->vsize, &gerr);
        if (gerr) {
          retval = do_write_failed_error_s_with_retry(full_file_name, gerr->message);
          lives_error_free(gerr);
          gerr = NULL;
        }
      }
    } while (retval == LIVES_RESPONSE_RETRY);
    free(tmp);

    prefs->pb_quality = pbq;

    if (internal) weed_layer_unref(layer);
    else weed_plant_free(layer);

    if (pixbuf) lives_widget_object_unref(pixbuf);
  }
  return TRUE;
}


void backup_file(int clip, int start, int end, const char *file_name) {
  lives_clip_t *sfile = mainw->files[clip];
  char **array;

  char *title;
  char full_file_name[PATH_MAX];

  char *com, *tmp;

  boolean with_perf = FALSE;
  boolean retval = -1, allow_over;
  boolean has_old_headers = FALSE;

  int withsound = 1;
  int current_file = mainw->current_file;

  if (strrchr(file_name, '.') == NULL) {
    lives_snprintf(full_file_name, PATH_MAX, "%s.%s", file_name, LIVES_FILE_EXT_BACKUP);
    allow_over = FALSE;
  } else {
    lives_snprintf(full_file_name, PATH_MAX, "%s", file_name);
    allow_over = TRUE;
  }

  // check if file exists
  if (!check_file(full_file_name, allow_over)) return;

  // create header files
  if (prefs->back_compat) {
    retval = write_headers(clip); // for pre LiVES 0.9.6
    has_old_headers = TRUE;
  }

  // should be present anyway, but just in case...
  retval = save_clip_values(clip); // new style (0.9.6+)

  if (!retval) {
    if (has_old_headers) {
      remove_old_headers(clip);
    }
    return;
  }

  //...and backup
  title = get_menu_name(sfile, FALSE);
  d_print(_("Backing up %s to %s"), title, full_file_name);
  lives_free(title);

  if (!mainw->save_with_sound) {
    d_print(_(" without sound"));
    withsound = 0;
  }

  d_print("...");

  if (sfile->clip_type == CLIP_TYPE_FILE) {
    frames_t ret;
    char *msg = (_("Pulling frames from clip..."));
    if ((ret = realize_all_frames(clip, msg, FALSE, 1, sfile->frames)) < sfile->frames) {
      lives_free(msg);
      if (ret > 0) d_print_cancelled();
      return;
    }
    lives_free(msg);
  }

  com = lives_strdup_printf("%s backup %s %d %d %d %s", prefs->backend, sfile->handle, withsound,
                            start, end, (tmp = lives_filename_from_utf8(full_file_name, -1, NULL, NULL, NULL)));
  lives_free(tmp);

  // TODO
  mainw->current_file = clip;

  cfile->nopreview = TRUE;
  cfile->progress_start = 1;
  cfile->progress_end = sfile->frames;

  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    if (has_old_headers) {
      remove_old_headers(clip);
    }
    mainw->current_file = current_file;
    return;
  }

  if (!(do_progress_dialog(TRUE, TRUE, _("Backing up"))) || mainw->error) {
    if (mainw->error) {
      d_print_failed();
    }

    // cancelled - clear up files
    cfile->nopreview = FALSE;
    if (has_old_headers) {
      remove_old_headers(clip);
    }

    mainw->current_file = current_file;
    return;
  }

  if (has_old_headers) {
    remove_old_headers(clip);
  }

  cfile->nopreview = FALSE;

  mainw->current_file = current_file;

  if (mainw->error) {
    widget_opts.non_modal = TRUE;
    do_error_dialog(mainw->msg);
    widget_opts.non_modal = FALSE;
    d_print_failed();
    return;
  }

  if (with_perf) {
    d_print(_("performance data was backed up..."));
  }

  array = lives_strsplit(mainw->msg, "|", 3);
  sfile->f_size = strtol(array[1], NULL, 10);
  lives_strfreev(array);

  lives_snprintf(sfile->file_name, PATH_MAX, "%s", full_file_name);
  if (!sfile->was_renamed) {
    lives_snprintf(sfile->name, CLIP_NAME_MAXLEN, "%s", full_file_name);
    set_main_title(cfile->name, 0);
    lives_menu_item_set_text(sfile->menuentry, full_file_name, FALSE);
  }
  if (prefs->show_recent)
    add_to_recent(full_file_name, 0., 0, NULL);

  sfile->changed = FALSE;
  // set is_untitled to stop users from saving with a .lv1 extension
  sfile->is_untitled = TRUE;
  d_print_done();
}


void reload_subs(int fileno) {
  lives_clip_t *sfile;
  char *subfname, *clipdir;
  if (!IS_VALID_CLIP(fileno)) return;

  sfile = mainw->files[fileno];
  clipdir = get_clip_dir(fileno);
  subfname = lives_build_filename(clipdir, SUBS_FILENAME "." LIVES_FILE_EXT_SRT, NULL);
  if (lives_file_test(subfname, LIVES_FILE_TEST_EXISTS)) {
    subtitles_init(sfile, subfname, SUBTITLE_TYPE_SRT);
  } else {
    lives_free(subfname);
    subfname = lives_build_filename(clipdir, SUBS_FILENAME "." LIVES_FILE_EXT_SUB, NULL);
    if (lives_file_test(subfname, LIVES_FILE_TEST_EXISTS)) {
      subtitles_init(sfile, subfname, SUBTITLE_TYPE_SUB);
    }
  }
  lives_free(subfname);
  lives_free(clipdir);
}


uint64_t restore_file(const char *file_name) {
  char *com = lives_strdup("dummy");
  char *mesg, *mesg1, *tmp;
  boolean is_OK = TRUE;
  char *fname = lives_strdup(file_name);
  char *clipdir;

  int old_file = mainw->current_file, current_file;
  int new_file = mainw->first_free_file;
  boolean not_cancelled;

  // create a new file
  if (!get_new_handle(new_file, fname)) {
    return 0;
  }

  d_print(_("Restoring %s..."), file_name);

  mainw->current_file = new_file;
  migrate_from_staging(mainw->current_file);
  cfile->hsize = mainw->def_width;
  cfile->vsize = mainw->def_height;

  if (!mainw->multitrack) {
    switch_clip(1, mainw->current_file, TRUE);
  }

#if 1
  BREAK_ME("restoring");

  com = lives_strdup_printf("%s restore_headers %s %s", prefs->backend, cfile->handle,
                            (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));

  lives_free(tmp);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    close_current_file(old_file);
    return 0;
  }

  do_progress_dialog(TRUE, FALSE, _("Restoring Metadata"));

  if (mainw->error) {
    close_current_file(old_file);
    return 0;
  }

  // call function to return rest of file details
  // fsize, afilesize and frames
  clipdir = get_clip_dir(mainw->current_file);
  is_OK = read_headers(mainw->current_file, clipdir, file_name);
  lives_free(clipdir);

  if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

  if (!is_OK) {
    mesg = lives_strdup_printf(_("\n\nThe file %s is corrupt.\nLiVES was unable to restore it.\n"),
                               file_name);
    do_error_dialog(mesg);
    lives_free(mesg);

    d_print_failed();
    close_current_file(old_file);
    return 0;
  }

  BREAK_ME("restored");

#endif

  com = lives_strdup_printf("%s restore %s %s", prefs->backend, cfile->handle,
                            (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));

  lives_free(tmp);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    close_current_file(old_file);
    return 0;
  }

  cfile->restoring = TRUE;
  not_cancelled = do_progress_dialog(TRUE, TRUE, _("Restoring"));
  cfile->restoring = FALSE;

  if (mainw->error || !not_cancelled) {
    if (mainw->error && mainw->cancelled != CANCEL_ERROR) {
      do_error_dialog(mainw->msg);
    }
    close_current_file(old_file);
    return 0;
  }

  // call function to return rest of file details
  // fsize, afilesize and frames
  clipdir = get_clip_dir(mainw->current_file);
  is_OK = read_headers(mainw->current_file, clipdir, file_name);
  lives_free(clipdir);

  if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

  if (!is_OK) {
    mesg = lives_strdup_printf(_("\n\nThe file %s is corrupt.\nLiVES was unable to restore it.\n"),
                               file_name);
    do_error_dialog(mesg);
    lives_free(mesg);

    d_print_failed();
    close_current_file(old_file);
    return 0;
  }

  // get img_type, check frame count and size
  if (!cfile->checked && !check_clip_integrity(mainw->current_file, NULL, cfile->frames)) {
    if (cfile->afilesize == 0) {
      reget_afilesize_inner(mainw->current_file);
    }
    if (!check_frame_count(mainw->current_file, FALSE)) {
      cfile->frames = get_frame_count(mainw->current_file, 1);
    }
  }
  cfile->checked = TRUE;

  add_to_clipmenu();

  if (prefs->show_recent) {
    add_to_recent(file_name, 0., 0, NULL);
  }

  if (cfile->frames > 0) {
    cfile->start = 1;
  } else {
    cfile->start = 0;
  }
  cfile->end = cfile->frames;
  cfile->arps = cfile->arate;
  cfile->pb_fps = cfile->fps;
  cfile->opening = FALSE;
  cfile->changed = FALSE;

  if (prefs->autoload_subs) {
    reload_subs(mainw->current_file);
  }

  lives_snprintf(cfile->type, 40, "Frames");
  mesg1 = lives_strdup_printf(_("Frames=%d type=%s size=%dx%d bpp=%d fps=%.3f\nAudio:"), cfile->frames, cfile->type,
                              cfile->hsize, cfile->vsize, cfile->bpp, cfile->fps);

  if (cfile->afilesize == 0l) {
    cfile->achans = 0;
    mesg = lives_strdup_printf(_("%s none\n"), mesg1);
  } else {
    mesg = lives_strdup_printf(P_("%s %d Hz %d channel %d bps\n", "%s %d Hz %d channels %d bps\n", cfile->achans),
                               mesg1, cfile->arate, cfile->achans, cfile->asampsize);
  }
  d_print(mesg);
  lives_free(mesg);
  lives_free(mesg1);

  cfile->is_loaded = TRUE;
  current_file = mainw->current_file;

  // set new bpp
  cfile->bpp = (cfile->img_type == IMG_TYPE_JPEG) ? 24 : 32;

  cfile->saved_frameno = cfile->frameno;
  if (cfile->frameno > cfile->frames && cfile->frameno > 1) cfile->frameno = cfile->frames;
  cfile->last_frameno = cfile->frameno;
  cfile->pointer_time = cfile->real_pointer_time = calc_time_from_frame(mainw->current_file, cfile->frameno);
  if (cfile->real_pointer_time > CLIP_TOTAL_TIME(mainw->current_file))
    cfile->real_pointer_time = CLIP_TOTAL_TIME(mainw->current_file);
  if (cfile->pointer_time > cfile->video_time) cfile->pointer_time = 0.;

  if (cfile->achans) {
    cfile->aseek_pos = (off64_t)((double)(cfile->real_pointer_time * cfile->arate) * cfile->achans *
                                 (cfile->asampsize / 8));
    //g_print("HHHHHH %d %f and %ld\n", cfile->frameno, cfile->real_pointer_time, cfile->aseek_pos);
    if (cfile->aseek_pos > cfile->afilesize) cfile->aseek_pos = 0.;
    cfile->async_delta = 0;
  }

  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    return 0;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

  if (!mainw->multitrack) {
    switch_clip(1, current_file, TRUE);
  }
  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");

  return cfile->unique_id;
}


/////////////////////////////////////////////////
/// scrap file
///  the scrap file is used during recording to dump any streamed (non-disk) clips to
/// during render/preview we load frames from the scrap file, but only as necessary

/// ascrap file
/// this is used to record external audio during playback with record on (if the user requests this)
/// afterwards the audio from it can be rendered/played back

uint64_t free_mb; // MB free to write
extern ticks_t lscrap_check;
extern double ascrap_mb;  // MB written to audio file

boolean open_scrap_file(void) {
  // create a scrap file for recording generated video frames
  int current_file = mainw->current_file;
  char *dir;
  char *handle, *scrap_handle;

  if (!check_for_executable(&capable->has_mktemp, EXEC_MKTEMP)) {
    do_program_not_found_error(EXEC_MKTEMP);
    return FALSE;
  }

  handle = get_worktmp("_scrap");
  if (!handle) {
    workdir_warning();
    return FALSE;
  }

  if (!create_cfile(-1, handle, FALSE)) {
    lives_free(handle);
    return FALSE;
  }
  lives_free(handle);

  mainw->scrap_file = mainw->current_file;
  dir = get_clip_dir(mainw->current_file);

  lives_snprintf(cfile->type, 40, SCRAP_LITERAL);

  scrap_handle = lives_strdup_printf(SCRAP_LITERAL "|%s", cfile->handle);
  if (prefs->crash_recovery) add_to_recovery_file(scrap_handle);
  lives_free(scrap_handle);

  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist = lives_list_append(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);

  free_mb = (double)get_ds_free(dir) / (double)ONE_MILLION;
  lives_free(dir);

  mainw->current_file = current_file;
  mainw->scrap_file_size = -1;
  lscrap_check = -1;

  // reset static vars
  save_to_scrap_file(NULL);

  if (mainw->ascrap_file == -1) ascrap_mb = 0.;

  return TRUE;
}


boolean open_ascrap_file(int clipno) {
  // create a scrap file for recording audio
  // we don't actually open the audio file here, that is done later
  lives_clip_t *sfile;
  char *dir, *handle, *ascrap_handle;
  int current_file = mainw->current_file;
  boolean new_clip = FALSE;

  if (!IS_VALID_CLIP(clipno)) {
    if (!check_for_executable(&capable->has_mktemp, EXEC_MKTEMP)) {
      do_program_not_found_error(EXEC_MKTEMP);
      return FALSE;
    }

    handle = get_worktmp("_" ASCRAP_LITERAL);
    if (!handle) {
      workdir_warning();
      return FALSE;
    }
    if (!create_cfile(-1, handle, FALSE)) {
      dir = get_clip_dir(mainw->current_file);
      lives_rmdir(dir, FALSE);
      lives_free(dir); lives_free(handle);
      return FALSE;
    }
    lives_free(handle);
    mainw->ascrap_file = mainw->current_file;
    lives_snprintf(cfile->type, 40, ASCRAP_LITERAL);
    cfile->opening = FALSE;
    new_clip = TRUE;
  } else mainw->ascrap_file = clipno;
  sfile = mainw->files[mainw->ascrap_file];

  // audio player will reset these, values are just defaults
  sfile->achans = 2;
  sfile->arate = sfile->arps = DEFAULT_AUDIO_RATE;
  sfile->asampsize = 16;
  sfile->signed_endian = 0; // the audio player will set this

  IF_APLAYER_PULSE
  (
  if (prefs->audio_src == AUDIO_SRC_EXT) {
  if (mainw->pulsed_read) {
      sfile->arate = sfile->arps = mainw->pulsed_read->in_arate;
    }
  } else {
    if (mainw->pulsed) {
      sfile->arate = sfile->arps = mainw->pulsed->out_arate;
    }
  })

  IF_APLAYER_JACK
  (
  if (prefs->audio_src == AUDIO_SRC_EXT) {
  if (mainw->jackd_read) {
      sfile->arate = sfile->arps = mainw->jackd_read->sample_in_rate;
      sfile->asampsize = 32;
    }
  } else {
    if (mainw->jackd) {
      sfile->arate = sfile->arps = mainw->jackd->sample_out_rate;
    }
  })

  if (new_clip) {
    ascrap_handle = lives_strdup_printf(ASCRAP_LITERAL "|%s", sfile->handle);
    if (prefs->crash_recovery) add_to_recovery_file(ascrap_handle);
    lives_free(ascrap_handle);

    pthread_mutex_lock(&mainw->clip_list_mutex);
    mainw->cliplist = lives_list_append(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
    pthread_mutex_unlock(&mainw->clip_list_mutex);

    dir = get_clip_dir(mainw->current_file);
    free_mb = (double)get_ds_free(dir) / (double)ONE_MILLION;
    lives_free(dir);

    mainw->current_file = current_file;
  }
  ascrap_mb = 0.;

  return TRUE;
}


void close_scrap_file(boolean remove) {
  int current_file = mainw->current_file;

  if (flush_scrap_file()) {
    mainw->current_file = mainw->scrap_file;
    srcgrps_free_all(mainw->current_file);
    if (remove) close_temp_handle(current_file);
    else mainw->current_file = current_file;
  }

  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist = lives_list_remove(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->scrap_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);

  if (prefs->crash_recovery) rewrite_recovery_file();

  mainw->scrap_file = -1;
  mainw->scrap_file_size = -1;
}


void close_ascrap_file(boolean remove) {
  int current_file = mainw->current_file;

  if (mainw->ascrap_file == -1) return;

  if (remove) {
    mainw->current_file = mainw->ascrap_file;
    close_temp_handle(current_file);
  }

  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist = lives_list_remove(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->ascrap_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);

  if (prefs->crash_recovery) rewrite_recovery_file();

  mainw->ascrap_file = -1;
}


static LiVESResponseType manual_locate(const char *orig_filename, lives_clip_t *sfile) {
  LiVESResponseType response = do_file_notfound_dialog(_("The original file"), orig_filename);
  if (response == LIVES_RESPONSE_RETRY) {
    return response;
  }

  if (response == LIVES_RESPONSE_BROWSE) {
    LiVESWidget *chooser;
    char fname[PATH_MAX], dirname[PATH_MAX], *newname;

    lives_snprintf(dirname, PATH_MAX, "%s", orig_filename);
    lives_snprintf(fname, PATH_MAX, "%s", orig_filename);

    get_dirname(dirname);
    get_basename(fname);

    chooser = choose_file_with_preview(dirname, fname, NULL, LIVES_FILE_SELECTION_VIDEO_AUDIO);

    response = lives_dialog_run(LIVES_DIALOG(chooser));

    end_fs_preview(NULL, NULL);

    if (response == LIVES_RESPONSE_ACCEPT) {
      newname = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));
      //lives_widget_destroy(LIVES_WIDGET(chooser));

      if (newname && *newname) {
        char *tmp;
        lives_snprintf(sfile->file_name, PATH_MAX, "%s",
                       (tmp = lives_filename_to_utf8(newname, -1, NULL, NULL, NULL)));
        lives_free(tmp);
        lives_free(newname);
      }
    }
    // cancelled from filechooser
    lives_widget_destroy(LIVES_WIDGET(chooser));
  }
  return response;
}


boolean reload_clip(int clipno, frames_t maxframe) {
  // reload clip -- for CLIP_TYPE_FILE
  // cd to clip directory - so decoder plugins can write temp files
  LiVESList *odeclist;
  lives_clip_t *sfile = mainw->files[clipno];
  const lives_clip_data_t *cdata = NULL;
  lives_clip_data_t *fake_cdata;

  double orig_fps = sfile->fps;

  char *orig_filename = lives_strdup(sfile->file_name);
  char *cwd = lives_get_current_dir();
  char *clipdir = get_clip_dir(clipno);

  LiVESResponseType response;
  boolean was_renamed = FALSE, retb = FALSE, ignore;
  int current_file;

  while (!lives_file_test(sfile->file_name, LIVES_FILE_TEST_EXISTS)) {
    response = manual_locate(orig_filename, sfile);
    if (response == LIVES_RESPONSE_RETRY) continue;
    else if (response == LIVES_RESPONSE_ACCEPT) {
      //re-scan for this
      sfile->fps = 0.;
      maxframe = 0;
      was_renamed = TRUE;
      break;
    } else return FALSE;
  }

  fake_cdata = (lives_clip_data_t *)struct_from_template(LIVES_STRUCT_CLIP_DATA_T);

  load_decoders();

  ///< retain original order to restore for freshly opened clips
  // get_decoder_cdata() may alter this
  odeclist = lives_list_copy(capable->plugins_list[PLUGIN_TYPE_DECODER]);

  lives_chdir(clipdir, FALSE);

  while (1) {
    fake_cdata->URI = lives_strdup(sfile->file_name);
    fake_cdata->fps = sfile->fps;

    fake_cdata->nframes = maxframe;

#ifdef VALGRIND_ON
    fake_cdata->debug = TRUE;
#endif

    response = LIVES_RESPONSE_NONE;

    if ((cdata = get_decoder_cdata(clipno, fake_cdata->fps != 0. ? fake_cdata : NULL)) == NULL) {
manual_locate:
      if (mainw->error || was_renamed) {
        mainw->error = FALSE;
        response = manual_locate(orig_filename, sfile);
        if (response == LIVES_RESPONSE_RETRY) {
          lives_freep((void **)fake_cdata->URI);
          continue;
        } else if (response == LIVES_RESPONSE_ACCEPT) {
          lives_freep((void **)&fake_cdata->URI);
          //re-scan for this
          sfile->fps = 0.;
          maxframe = 0;
          was_renamed = TRUE;
          continue;
        }
      } else {
        // unopenable
        if (!sfile->old_dec_uid) do_no_decoder_error(clipno);
        ignore = TRUE;
      }

      lives_chdir(cwd, FALSE);
      lives_free(cwd);

      // NOT openable, or not found and user cancelled, switch back to original clip
      if (!sfile->checked && cdata) {
        check_clip_integrity(clipno, cdata, maxframe);
        if (sfile->frames > 0 || sfile->afilesize > 0) {
          // recover whatever we can
          sfile->clip_type = CLIP_TYPE_FILE;
          retb = check_if_non_virtual(clipno, 1, sfile->frames);
        }
        sfile->checked = TRUE;
      }
      if (!retb) {
        if (ignore) {
          ignore_clip(clipdir);
          lives_freep((void **)&mainw->files[clipno]);
        } else {
          current_file = mainw->current_file;
          mainw->current_file = clipno;
          close_current_file(current_file);
        }
      }
      unref_struct(fake_cdata->lsd);

      lives_free(orig_filename);
      lives_list_free(capable->plugins_list[PLUGIN_TYPE_DECODER]);
      capable->plugins_list[PLUGIN_TYPE_DECODER] = odeclist;
      lives_free(clipdir);
      return retb;
    }

    // got cdata
    if (was_renamed) {
      // manual relocation
      sfile->fps = orig_fps;
      if (!sfile->checked && !check_clip_integrity(clipno, cdata, maxframe)) {
        // get correct img_type, fps, etc.
        if (THREADVAR(com_failed) || THREADVAR(write_failed)) do_header_write_error(clipno);
        goto manual_locate;
      }
      sfile->checked = TRUE;
      sfile->needs_silent_update = TRUE; // force filename update in header
      if (prefs->show_recent) {
        // replace in recent menu
        char file[PATH_MAX];
        int i;
        for (i = 0; i < 4; i++) {
          char *tmp;
          char *pref = lives_strdup_printf("%s%d", PREF_RECENT, i + 1);
          get_utf8_pref(pref, file, PATH_MAX);
          tmp = subst(file, orig_filename, sfile->file_name);
          if (lives_utf8_strcmp(tmp, file)) {
            lives_snprintf(file, PATH_MAX, "%s", tmp);
            set_utf8_pref(pref, file);
            lives_menu_item_set_text(mainw->recent[i], file, FALSE);
            if (mainw->multitrack) lives_menu_item_set_text(mainw->multitrack->recent[i], file, FALSE);
          }
          lives_free(tmp);
          lives_free(pref);
        }
        if (mainw->prefs_cache) {
          // update recent files -> force reload of prefs
          cached_list_free(&mainw->prefs_cache);
          mainw->prefs_cache = cache_file_contents(prefs->configfile);
        }
      }
    }

    if (cdata != fake_cdata) unref_struct(fake_cdata->lsd);
    break;
  }

  lives_free(orig_filename);
  lives_chdir(cwd, FALSE);
  lives_free(cwd);

  sfile->clip_type = CLIP_TYPE_FILE;
  get_mime_type(sfile->type, 40, cdata);

  // read_headers() may have set this to "jpeg" (default)
  if (sfile->header_version < 104) sfile->img_type = IMG_TYPE_UNKNOWN;

  // we will set correct value in check_clip_integrity() if there are any real images

  if (get_primary_src(clipno)) {
    boolean bad_header = FALSE;
    boolean correct = TRUE;
    if (!was_renamed) {
      if (!sfile->checked)
        correct = check_clip_integrity(clipno, cdata, maxframe); // get correct img_type, fps, etc.
      sfile->checked = TRUE;
    }
    if (!correct) {
      if (THREADVAR(com_failed) || THREADVAR(write_failed)) bad_header = TRUE;
    } else {
      lives_decoder_t *dplug = (lives_decoder_t *)(get_primary_src(clipno))->actor;
      if (dplug) {
        lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)dplug->dpsys;
        sfile->decoder_uid = dpsys->id->uid;
        save_clip_value(clipno, CLIP_DETAILS_DECODER_UID, (void *)&sfile->decoder_uid);
        if (THREADVAR(com_failed) || THREADVAR(write_failed)) bad_header = TRUE;
        else {
          save_clip_value(clipno, CLIP_DETAILS_DECODER_NAME, (void *)dpsys->soname);
          if (THREADVAR(com_failed) || THREADVAR(write_failed)) bad_header = TRUE;
        }
      }
    }

    if (bad_header) do_header_write_error(clipno);
  }
  lives_list_free(capable->plugins_list[PLUGIN_TYPE_DECODER]);
  capable->plugins_list[PLUGIN_TYPE_DECODER] = odeclist;
  if (prefs->autoload_subs) {
    reload_subs(clipno);
  }
  return TRUE;
}


// return the new current_file - if the player is active then we do not switch, but let the player do that
int close_current_file(int file_to_switch_to) {
  // close the current file, and free the file struct and all sub storage
  LiVESList *list_index;
  char *com;
  boolean need_new_blend_file = FALSE, post_dprint = FALSE;
  int index = -1, old_file = mainw->current_file;

  if (mainw->close_this_clip == mainw->current_file) mainw->close_this_clip = -1;

  if (!CURRENT_CLIP_IS_VALID) return -1;

  if (cfile->clip_type == CLIP_TYPE_TEMP) {
    close_temp_handle(file_to_switch_to);
    return file_to_switch_to;
  }

  if (mainw->noswitch) {
    mainw->new_clip = file_to_switch_to;
    mainw->close_this_clip = mainw->current_file;
    return mainw->new_clip;
  }

  if (cfile->clip_type != CLIP_TYPE_GENERATOR && mainw->current_file != mainw->scrap_file &&
      mainw->current_file != mainw->ascrap_file && mainw->current_file != 0 &&
      (!mainw->multitrack || mainw->current_file != mainw->multitrack->render_file)) {
    post_dprint = TRUE;
    d_print(_("Closed clip %s\n"), cfile->file_name);
    lives_notify(LIVES_OSC_NOTIFY_CLIP_CLOSED, "");
  }

  cfile->hsize = mainw->def_width;
  cfile->vsize = mainw->def_height;

  if (cfile->laudio_drawable) {
    if (mainw->laudio_drawable == cfile->laudio_drawable
        || mainw->drawsrc == mainw->current_file) mainw->laudio_drawable = NULL;
    if (cairo_surface_get_reference_count(cfile->laudio_drawable))
      lives_painter_surface_destroy(cfile->laudio_drawable);
    cfile->laudio_drawable = NULL;
  }
  if (cfile->raudio_drawable) {
    if (mainw->raudio_drawable == cfile->raudio_drawable
        || mainw->drawsrc == mainw->current_file) mainw->raudio_drawable = NULL;
    if (cairo_surface_get_reference_count(cfile->raudio_drawable))
      lives_painter_surface_destroy(cfile->raudio_drawable);
    cfile->raudio_drawable = NULL;
  }
  if (mainw->drawsrc == mainw->current_file) mainw->drawsrc = -1;

  if (mainw->st_fcache) {
    if (mainw->en_fcache == mainw->st_fcache) mainw->en_fcache = NULL;
    if (mainw->pr_fcache == mainw->st_fcache) mainw->pr_fcache = NULL;
    weed_layer_unref(mainw->st_fcache);
    mainw->st_fcache = NULL;
  }
  if (mainw->en_fcache) {
    if (mainw->pr_fcache == mainw->en_fcache) mainw->pr_fcache = NULL;
    weed_layer_unref(mainw->en_fcache);
    mainw->en_fcache = NULL;
  }
  if (mainw->pr_fcache) {
    weed_layer_unref(mainw->pr_fcache);
    mainw->pr_fcache = NULL;
  }

  for (int i = 0; i < FN_KEYS - 1; i++) {
    if (mainw->clipstore[i][0] == mainw->current_file) mainw->clipstore[i][0] = -1;
  }

  // this must all be done last...
  if (cfile->menuentry) {
    // c.f. on_prevclip_activate
    list_index = lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
    do {
      if (!(list_index = lives_list_previous(list_index))) list_index = lives_list_last(mainw->cliplist);
      index = LIVES_POINTER_TO_INT(lives_list_nth_data(list_index, 0));
    } while ((mainw->files[index] || mainw->files[index]->opening || mainw->files[index]->restoring ||
              (index == mainw->scrap_file && index > -1) || (index == mainw->ascrap_file && index > -1)
              || (mainw->files[index]->frames == 0 &&
                  LIVES_IS_PLAYING)) &&
             index != mainw->current_file);
    if (index == mainw->current_file) index = -1;
    if (mainw->current_file != mainw->scrap_file && mainw->current_file != mainw->ascrap_file)
      remove_from_clipmenu();
  }

  if (mainw->blend_file == mainw->current_file) {
    need_new_blend_file = TRUE;
    // if closing a generator clip, it is better to reomve the track_source here,
    // otherwise map_sources_to_tracks will try to find a clip_srcgrp and make the track_source idle
    track_source_free(1, mainw->blend_file);
    if (!LIVES_IS_PLAYING) mainw->blend_file = -1;
  } else if (mainw->current_file == mainw->playing_file) {
    track_source_free(0, mainw->current_file);
  }

  free_thumb_cache(mainw->current_file, 0);
  lives_freep((void **)&cfile->frame_index);
  lives_freep((void **)&cfile->frame_index_back);

  if (cfile->clip_type != CLIP_TYPE_GENERATOR && !mainw->close_keep_frames) {
    char *clipd = get_clip_dir(mainw->current_file);
    if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
      // as a safety feature we create a special file which allows the back end to delete the directory
      char *temp_backend;
      char *permitname = lives_build_filename(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
      lives_touch(permitname);
      lives_free(permitname);
      temp_backend = use_staging_dir_for(mainw->current_file);
      com = lives_strdup_printf("%s close \"%s\"", temp_backend, cfile->handle);
      lives_free(temp_backend);
      lives_system(com, TRUE);
      lives_free(com);
      temp_backend = lives_build_path(LIVES_DEVICE_DIR, LIVES_SHM_DIR, cfile->handle, NULL);
      if (!lives_file_test(temp_backend, LIVES_FILE_TEST_IS_DIR)) {
        lives_rmdir_with_parents(temp_backend);
      }
      lives_free(temp_backend);
    }
    lives_free(clipd);
    if (cfile->event_list_back) event_list_free(cfile->event_list_back);
    if (cfile->event_list) event_list_free(cfile->event_list);

    lives_list_free_all(&cfile->layout_map);
  }

  if (cfile->subt) subtitles_free(cfile);

  srcgrps_free_all(mainw->current_file);

  if (cfile->audio_waveform) {
    drawtl_cancel();
    for (int i = 0; i < cfile->achans; i++) lives_freep((void **)&cfile->audio_waveform[i]);
    lives_freep((void **)&cfile->audio_waveform);
    lives_freep((void **)&cfile->aw_sizes);
    cfile->aw_sizes = NULL;
    unlock_timeline();
  }

  if (mainw->files[mainw->current_file]) {
    lives_free(mainw->files[mainw->current_file]);
    mainw->files[mainw->current_file] = NULL;
  }

  if (mainw->multitrack && mainw->current_file != mainw->multitrack->render_file) {
    mt_delete_clips(mainw->multitrack, mainw->current_file);
  }

  if ((mainw->first_free_file == ALL_USED || mainw->first_free_file > mainw->current_file) && mainw->current_file > 0)
    mainw->first_free_file = mainw->current_file;

  if (!mainw->only_close) {
    if (IS_VALID_CLIP(file_to_switch_to) && file_to_switch_to > 0) {
      if (!mainw->multitrack) {
        if (!LIVES_IS_PLAYING) {
          mainw->current_file = file_to_switch_to;
          switch_clip(1, file_to_switch_to, TRUE);
          if (post_dprint) d_print("");
        } else {
          if (file_to_switch_to != mainw->playing_file) {
            mainw->new_clip = file_to_switch_to;
            if (need_new_blend_file) {
              mainw->new_blend_file = file_to_switch_to;
            }
          } else mainw->current_file = file_to_switch_to;
        }
      } else if (old_file != mainw->multitrack->render_file) {
        mt_clip_select(mainw->multitrack, TRUE);
      }
      return file_to_switch_to;
    }
  }
  // file we were asked to switch to is invalid, thus we must find one

  //if (mainw->only_close) mainw->noswitch = TRUE;

  file_to_switch_to = find_next_clip(index, old_file);

  if (LIVES_IS_PLAYING) return file_to_switch_to;

  if (CURRENT_CLIP_IS_VALID) return mainw->current_file;

  // no other clips
  mainw->preview_frame = 0;
  mainw->current_file = mainw->blend_file = -1;
  set_main_title(NULL, 0);

  if (mainw->play_window) {
    lives_widget_hide(mainw->preview_controls);
    load_preview_image(FALSE);
    resize_play_window();
  }

  lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
  lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
  lives_widget_set_sensitive(mainw->export_proj, FALSE);
  lives_widget_set_sensitive(mainw->import_proj, FALSE);

  if (mainw->multitrack && old_file != mainw->multitrack->render_file)
    lives_widget_set_sensitive(mainw->multitrack->load_set, TRUE);

  // can't use set_undoable, as we don't have a cfile
  lives_menu_item_set_text(mainw->undo, _("_Undo"), TRUE);
  lives_menu_item_set_text(mainw->redo, _("_Redo"), TRUE);
  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo, FALSE);

  if (!mainw->is_ready || mainw->recovering_files) return -1;

  if (!mainw->multitrack) {
    //resize(1);
    lives_widget_set_opacity(mainw->playframe, 0.);
    //lives_widget_hide(mainw->playframe);
    load_start_image(0);
    load_end_image(0);
    /* if (prefs->show_msg_area && !mainw->only_close) { */
    /*   if (mainw->idlemax == 0) { */
    /*     lives_idle_add(resize_message_area, NULL); */
    /*   } */
    /*   mainw->idlemax = DEF_IDLE_MAX; */
    /* } */
  }

  set_sel_label(mainw->sel_label);

  zero_spinbuttons();
  show_playbar_labels(-1);

  if (!mainw->only_close) {
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    d_print("");

    if (mainw->multitrack) {
      if (old_file != mainw->multitrack->render_file) {
        mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
        mt_clip_select(mainw->multitrack, TRUE);
      }
    }
  }
  if (!mainw->is_processing && !mainw->preview) {
    if (mainw->multitrack) {
      if (old_file != mainw->multitrack->render_file) {
        mt_sensitise(mainw->multitrack);
      }
    } else sensitize();
  }
  return mainw->current_file;
}


boolean recover_files(char *recovery_file, boolean auto_recover) {
  FILE *rfile = NULL;

  char buff[256], *buffptr;
  char *clipdir;

  LiVESResponseType resp;

  int clipnum = 0;
  int maxframe;
  int last_good_file = -1, ngoodclips = 0;

  boolean is_scrap, is_ascrap;
  boolean did_set_check = FALSE;
  boolean is_ready = mainw->is_ready, mt_is_ready = FALSE;
  boolean mt_needs_idlefunc = FALSE;
  boolean retb = TRUE, retval;
  boolean load_from_set = TRUE;
  boolean rec_cleanup = FALSE;
  boolean use_decoder;

  // setting is_ready allows us to get the correct transient window for dialogs
  // otherwise the dialogs will appear behind the main interface
  // we do this for mainwindow and multitrack

  // we will reset these before returning
  mainw->is_ready = TRUE;

  //resize_message_area(NULL);
  msg_area_config(mainw->msg_area);

  if (mainw->multitrack) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    mt_is_ready = mainw->multitrack->is_ready;
    mainw->multitrack->is_ready = TRUE;
  }

  if (!auto_recover) {
    char *tmp;

    if (!do_yesno_dialogf_with_countdown
        (2, FALSE, (tmp = _("\nFiles from a previous run of LiVES were found.\n"
                            "Do you want to attempt to recover them ?\n")))) {
      lives_free(tmp);
      retb = FALSE;
      goto recovery_done;
    }
    lives_free(tmp);
  }
  if (recovery_file) {
    do {
      resp = LIVES_RESPONSE_NONE;
      rfile = fopen(recovery_file, "r");
      if (!rfile) {
        resp = do_read_failed_error_s_with_retry(recovery_file, lives_strerror(errno));
        if (resp == LIVES_RESPONSE_CANCEL) {
          retb = FALSE;
          goto recovery_done;
        }
      }
    } while (resp == LIVES_RESPONSE_RETRY);
  }

  do_threaded_dialog(_("Recovering files"), FALSE);
  d_print(_("\nRecovering files..."));

  threaded_dialog_auto_spin();

  mainw->suppress_dprint = TRUE;
  mainw->recovering_files = TRUE;

  while (1) {
    // must set this until after setting strt and end.
    // otherwise  all_config() may get triggered for start and end image.
    // If start or end is zero,
    // this will cause the widget to be redrawn with the image blank pixbuf
    defer_config(mainw->start_image);
    defer_config(mainw->end_image);

    if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

    is_scrap = FALSE;
    is_ascrap = FALSE;

    THREADVAR(read_failed) = FALSE;

    if (recovery_file) {
      if (!lives_fgets(buff, 256, rfile)) {
        reset_clipmenu();
        mainw->suppress_dprint = FALSE;
        if (THREADVAR(read_failed)) {
          d_print_failed();
          do_read_failed_error_s(recovery_file, NULL);
        } else d_print_done();
        break;
      }
    } else {
      if (!mainw->recovery_list) {
        reset_clipmenu();
        mainw->suppress_dprint = FALSE;
        d_print_done();
        break;
      }
      lives_snprintf(buff, 256, "%s", (char *)mainw->recovery_list->data);
      mainw->recovery_list = mainw->recovery_list->next;
    }

    if (!*buff) continue;
    lives_chomp(buff, FALSE);
    if (!*buff) continue;

    if (buff[lives_strlen(buff) - 1] == '*') {
      boolean crash_recovery = prefs->crash_recovery;
      LiVESResponseType resp;
      // set to be opened

      buff[lives_strlen(buff) - 1 - strlen(LIVES_DIR_SEP)] = 0;
      do {
        resp = LIVES_RESPONSE_OK;
        if (!is_legal_set_name(buff, TRUE, TRUE)) {
          resp = do_abort_retry_cancel_dialog(_("Click Abort to exit LiVES immediately, Retry to try again,"
                                                " or Cancel to continue without reloading the set.\n"));
        }
      } while (resp == LIVES_RESPONSE_RETRY);
      if (resp == LIVES_RESPONSE_CANCEL) continue;

      /** dont write an entry yet, in case of the unklikely chance we were assigned the same pid as the recovery file,
        otherwise we will end up in am endless loop of reloading the same set and appending it to the recovery file
        in any case, the old file is still there and we will create a fresh recovery file after a successful reload */
      prefs->crash_recovery = FALSE;

      if (!reload_set(buff)) {
        prefs->crash_recovery = crash_recovery; /// reset to original value
        mainw->suppress_dprint = FALSE;
        d_print_failed();
        mainw->suppress_dprint = FALSE;
        continue;
      }
      last_good_file = mainw->current_file;
      mainw->was_set = TRUE;
      prefs->crash_recovery = crash_recovery; /// reset to original value
      continue;
    } else {
      /// load single file
      if (!strncmp(buff, SCRAP_LITERAL "|", SCRAP_LITERAL_LEN + 1)) {
        is_scrap = TRUE;
        buffptr = buff + 6;
      } else if (!strncmp(buff, ASCRAP_LITERAL "|", ASCRAP_LITERAL_LEN + 1)) {
        is_ascrap = TRUE;
        buffptr = buff + 7;
      } else {
        if (!strncmp(buff, ASCRAP_LITERAL, ASCRAP_LITERAL_LEN)
            || !strncmp(buff, SCRAP_LITERAL, SCRAP_LITERAL_LEN)) {
          rec_cleanup = TRUE;
          continue;
        }
        buffptr = buff;
      }

      if (should_ignore_ext_clip(buff)) continue;

      if (strstr(buffptr, "/" CLIPS_DIRNAME "/")) {
        char **array;
        if (!load_from_set) continue;
        array = lives_strsplit(buffptr, "/" CLIPS_DIRNAME "/", -1);
        mainw->was_set = TRUE;
        lives_snprintf(mainw->set_name, 128, "%s", array[0]);
        lives_strfreev(array);

        if (!did_set_check && !check_for_lock_file(mainw->set_name, 0)) {
          if (!do_set_locked_warning(mainw->set_name)) {
            load_from_set = FALSE;
            mainw->was_set = FALSE;
            mainw->set_name[0] = 0;
          }
          did_set_check = TRUE;
        }
      }

      /// create a new cfile and fill in the details
      if (!create_cfile(-1, buffptr, FALSE)) {
        end_threaded_dialog();
        mainw->suppress_dprint = FALSE;
        d_print_failed();
        break;
      }

      if (is_scrap || is_ascrap) {
        // need this for rewrite recovery
        pthread_mutex_lock(&mainw->clip_list_mutex);
        mainw->cliplist = lives_list_append(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
        pthread_mutex_unlock(&mainw->clip_list_mutex);
      }

      if (is_scrap) {
        mainw->scrap_file = mainw->current_file;
        cfile->opening = FALSE;
        lives_snprintf(cfile->type, 40, SCRAP_LITERAL);
        cfile->frames = 1;
        cfile->hsize = 640;
        cfile->vsize = 480;
        continue;
      }

      if (is_ascrap) {
        mainw->ascrap_file = mainw->current_file;
        cfile->opening = FALSE;
        lives_snprintf(cfile->type, 40, ASCRAP_LITERAL);
        // TODO - if we dont end up with an event_list, silently dorp this - disk celanup can remove it later
      }

      /// get file details; this will cache the header in mainw->hdrs_cache
      // we need to keep this around for open_set_file(), below.
      clipdir = get_clip_dir(mainw->current_file);
      retval = read_headers(mainw->current_file, clipdir, NULL);
      lives_free(clipdir);

      if (!retval) {
        /// clip failed to reload
        lives_free(mainw->files[mainw->current_file]);
        mainw->files[mainw->current_file] = NULL;
        mainw->first_free_file = mainw->current_file;
        if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);
        if (is_ascrap) mainw->ascrap_file = -1;
        continue;
      }

      if (mainw->current_file < 1) continue;
      if (is_ascrap) continue;

      use_decoder = should_use_decoder(mainw->current_file);
      maxframe = load_frame_index(mainw->current_file);

      /// see function reload_set() for detailed comments
      if (use_decoder || maxframe) {
        /// CLIP_TYPE_FILE
        if (!*cfile->file_name) continue;
        if (!reload_clip(mainw->current_file, maxframe)) continue;
        add_primary_src(mainw->current_file, NULL, LIVES_SRC_TYPE_IMAGE);
        if (cfile->needs_update || cfile->img_type == IMG_TYPE_UNKNOWN) {
          lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
          if (cfile->needs_update || count_virtual_frames(cfile->frame_index, 1, cfile->frames)
              < cfile->frames) {
            if (!cfile->checked && !check_clip_integrity(mainw->current_file, cdata, cfile->frames)) {
              cfile->needs_update = TRUE;
            }
            cfile->checked = TRUE;
          }
        }
        check_if_non_virtual(mainw->current_file, 1, cfile->frames);
      } else {
        /// CLIP_TYPE_DISK
        // add the img_loader clip_src, which will create the primary srcgroup with one clip_src
        add_primary_src(mainw->current_file, NULL, LIVES_SRC_TYPE_IMAGE);
        if (!cfile->checked) {
          if (!check_clip_integrity(mainw->current_file, NULL, cfile->frames)) {
            cfile->needs_update = TRUE;
          }
          cfile->checked = TRUE;
        }
        if (!prefs->vj_mode && cfile->needs_update) {
          if (cfile->afilesize == 0) {
            reget_afilesize_inner(mainw->current_file);
          }
          if (!check_frame_count(mainw->current_file, !cfile->needs_update)) {
            cfile->frames = get_frame_count(mainw->current_file, 1);
            if (prefs->show_dev_opts) {
              g_printerr("frame count recalculated\n");
            }
            cfile->needs_update = TRUE;
          }
        }
      }
      if (!recovery_file && !cfile->checked) {
        lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
        if (!check_clip_integrity(mainw->current_file, cdata, cfile->frames)) {
          cfile->needs_update = TRUE;
        }
      }
    }

    cfile->start = cfile->frames > 0 ? 1 : 0;
    cfile->end = cfile->frames;

    /** not really from a set, but let's pretend to get the details
        read the playback fps, play frame, and name */

    /// NEED TO maintain mainw->hdrs_cache when entering the function,
    /// else it will be considered a legacy file load
    open_set_file(++clipnum);

    if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

    if (!CURRENT_CLIP_IS_VALID) continue;

    get_total_time(cfile);

    if (CLIP_TOTAL_TIME(mainw->current_file) == 0.) {
      close_current_file(last_good_file);
      continue;
    }

    if (cfile->video_time < cfile->laudio_time) {
      if (!cfile->checked) {
        lives_clip_data_t *cdata = get_clip_cdata(mainw->current_file);
        if (!check_clip_integrity(mainw->current_file, cdata, cfile->frames)) {
          cfile->needs_update = TRUE;
        }
      }
    }

    last_good_file = mainw->current_file;

    if (update_clips_version(mainw->current_file)) cfile->needs_silent_update = TRUE;

    if (!prefs->vj_mode || (cfile->needs_update || cfile->needs_silent_update)) {
      if (!prefs->vj_mode && cfile->needs_update) do_clip_divergence_error(mainw->current_file);
      save_clip_values(mainw->current_file);
      cfile->needs_silent_update = cfile->needs_update = FALSE;
    }

    // add to clip menu

    add_to_clipmenu();

    cfile->is_loaded = TRUE;
    cfile->changed = TRUE;
    lives_rm(cfile->info_file);
    if (!mainw->multitrack) set_main_title(cfile->name, 0);

    if (cfile->frameno > cfile->frames) cfile->frameno = cfile->last_frameno = 1;

    if (!mainw->multitrack) resize(1);

    if (!mainw->multitrack) {
      lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start);
      lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
      lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end);
      lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);
      //g_print("MWC is %d\n", mainw->current_file);
      showclipimgs();
    } else {
      int current_file = mainw->current_file;
      lives_mt *multi = mainw->multitrack;
      mainw->multitrack = NULL;
      mainw->current_file = -1;
      reget_afilesize(current_file);
      mainw->current_file = current_file;
      mainw->multitrack = multi;
      get_total_time(cfile);
      mainw->current_file = mainw->multitrack->render_file;
      mt_init_clips(mainw->multitrack, current_file, TRUE);
      set_poly_tab(mainw->multitrack, POLY_CLIPS);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      mt_clip_select(mainw->multitrack, TRUE);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      mainw->current_file = current_file;
    }

    if (cfile->frameno > cfile->frames && cfile->frameno > 1) cfile->frameno = cfile->frames;
    cfile->last_frameno = cfile->frameno;
    cfile->pointer_time = cfile->real_pointer_time = calc_time_from_frame(mainw->current_file, cfile->frameno);
    if (cfile->real_pointer_time > CLIP_TOTAL_TIME(mainw->current_file))
      cfile->real_pointer_time = CLIP_TOTAL_TIME(mainw->current_file);
    if (cfile->pointer_time > cfile->video_time) cfile->pointer_time = 0.;
    if (cfile->achans) {
      cfile->aseek_pos = (off64_t)((double)(cfile->real_pointer_time * cfile->arate) * cfile->achans *
                                   (cfile->asampsize >> 3));
      if (cfile->aseek_pos > cfile->afilesize) cfile->aseek_pos = 0.;
      //g_print("HHHHHH 222 %d %f and %ld\n", cfile->frameno, cfile->real_pointer_time, cfile->aseek_pos);
      cfile->async_delta = 0;
    }

    lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  }

  /* run_deferred_config(mainw->start_image, &mainw->si_surface); */
  /* run_deferred_config(mainw->end_image, &mainw->ei_surface); */

  //if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

  ngoodclips = lives_list_length(mainw->cliplist);
  if (mainw->scrap_file != -1) ngoodclips--;
  if (mainw->ascrap_file != -1) ngoodclips--;
  if (!ngoodclips) {
    d_print(_("No clips were recovered.\n"));
  } else
    d_print(P_("%d clip was recovered ", "%d clips were recovered ", ngoodclips), ngoodclips);

  if (recovery_file)
    d_print(_("from the previous session.\n"));
  else
    d_print(_("from previous sessions.\n"));

  if (!mainw->multitrack) { // TODO check if we can do this in mt too
    int start_file = mainw->current_file;
    if (start_file > 1 && start_file == mainw->ascrap_file && mainw->files[start_file - 1]) {
      start_file--;
    }
    if (start_file > 1 && start_file == mainw->scrap_file && mainw->files[start_file - 1]) {
      start_file--;
    }
    if (start_file > 1 && start_file == mainw->ascrap_file && mainw->files[start_file - 1]) {
      start_file--;
    }
    if ((!IS_VALID_CLIP(start_file) || (mainw->files[start_file]->frames == 0 && mainw->files[start_file]->afilesize == 0))
        && mainw->files[1] && start_file != 1) {
      for (start_file = MAX_FILES; start_file > 0; start_file--) {
        if (mainw->files[start_file]
            && (mainw->files[start_file]->frames > 0 || mainw->files[start_file]->afilesize > 0))
          if (start_file != mainw->scrap_file && start_file != mainw->ascrap_file) break;
      }
    }

    if (start_file != mainw->current_file) {
      rec_cleanup = TRUE;
      switch_clip(1, start_file, TRUE);
      showclipimgs();
    }
  } else {
    mt_clip_select(mainw->multitrack, TRUE); // scroll clip on screen
  }

  if (recovery_file) fclose(rfile);

recovery_done:
  end_threaded_dialog();

  mainw->invalid_clips = FALSE;
  mainw->suppress_dprint = FALSE;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  mainw->is_ready = is_ready;
  if (mainw->multitrack) {
    mainw->multitrack->is_ready = mt_is_ready;
    mainw->current_file = mainw->multitrack->render_file;
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    if (mt_needs_idlefunc) mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
  } else update_play_times();
  mainw->last_dprint_file = -1;
  mainw->no_switch_dprint = FALSE;
  d_print("");
  mainw->invalid_clips = rec_cleanup;

  if (ngoodclips && *mainw->set_name) recover_layout_map();
  mainw->recovering_files = FALSE;

  switch_clip(1, mainw->current_file, TRUE);

  return retb;
}


void add_to_recovery_file(const char *handle) {
  lives_echo(handle, mainw->recovery_file, TRUE);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return;
  }

  if ((mainw->multitrack && mainw->multitrack->event_list) || mainw->stored_event_list)
    write_backup_layout_numbering(mainw->multitrack);
}


boolean rewrite_recovery_file(void) {
  // part of the crash recovery system
  // returns TRUE if successful
  LiVESList *clist = mainw->cliplist;
  char *recovery_entry;
  char *temp_recovery_file;

  boolean opened = FALSE;
  boolean wrote_set_entry = FALSE;

  int recovery_fd = -1;
  LiVESResponseType retval;

  if (!clist || !prefs->crash_recovery) {
    lives_rm(mainw->recovery_file);
    return FALSE;
  }

  temp_recovery_file = lives_strdup_printf("%s.%s", mainw->recovery_file, LIVES_FILE_EXT_TMP);

  do {
    retval = LIVES_RESPONSE_NONE;
    THREADVAR(write_failed) = FALSE;
    opened = FALSE;
    recovery_fd = -1;

    for (; clist; clist = clist->next) {
      int i = LIVES_POINTER_TO_INT(clist->data);
      if (IS_NORMAL_CLIP(i)) {
        lives_clip_t *sfile = mainw->files[i];
        if (i == mainw->scrap_file) {
          recovery_entry = lives_strdup_printf(SCRAP_LITERAL "|%s\n", sfile->handle);
        } else if (i == mainw->ascrap_file) {
          recovery_entry = lives_strdup_printf(ASCRAP_LITERAL "|%s\n", sfile->handle);
        } else {
          if (sfile->was_in_set && *mainw->set_name) {
            if (!wrote_set_entry) {
              recovery_entry = lives_build_filename_relative(mainw->set_name, "*\n", NULL);
              wrote_set_entry = TRUE;
            } else continue;
          } else recovery_entry = lives_strdup_printf("%s\n", sfile->handle);
        }

        if (!opened) recovery_fd = creat(temp_recovery_file, S_IRUSR | S_IWUSR);
        if (recovery_fd < 0) retval = do_write_failed_error_s_with_retry(temp_recovery_file, lives_strerror(errno));
        else {
          opened = TRUE;
          lives_write(recovery_fd, recovery_entry, strlen(recovery_entry), TRUE);
          if (THREADVAR(write_failed)) retval = do_write_failed_error_s_with_retry(temp_recovery_file, NULL);
        }
        lives_free(recovery_entry);
      }
      if (THREADVAR(write_failed)) break;
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  if (!opened) lives_rm(mainw->recovery_file);
  else if (recovery_fd >= 0) {
    close(recovery_fd);
    retval = LIVES_RESPONSE_INVALID;
    do {
      lives_mv(temp_recovery_file, mainw->recovery_file);
      if (THREADVAR(com_failed)) {
        retval = do_write_failed_error_s_with_retry(temp_recovery_file, NULL);
      }
    } while (retval == LIVES_RESPONSE_RETRY);
  }

  lives_free(temp_recovery_file);

  if ((mainw->multitrack && mainw->multitrack->event_list) || mainw->stored_event_list)
    write_backup_layout_numbering(mainw->multitrack);

  return TRUE;
}


static boolean rewrite_recovery_file_cb(lives_obj_t *obj, void *data) {
  rewrite_recovery_file();
  return FALSE;
}

static lives_proc_thread_t rewrite_recovery_lpt = NULL;

boolean check_for_recovery_files(boolean auto_recover, boolean no_recover) {
  uint32_t recpid = 0;

  char *recovery_file, *recovery_numbering_file, *recording_file, *recording_numbering_file, *xfile;
  char *com, *uldir = NULL, *ulf, *ulfile;
  char *recfname, *recfname2, *recfname3, *recfname4;

  boolean retval = FALSE;
  boolean found_clips = FALSE, found_recording = FALSE, found_layout = FALSE;

  int lgid = lives_getgid();
  int luid = lives_getuid();
  uint64_t uid = gen_unique_id();

  lives_pid_t lpid = capable->mainpid;

  // recheck:

  // ask backend to find the latest recovery file which is not owned by a running version of LiVES
  com = lives_strdup_printf("%s get_recovery_file %d %d %s %s %d",
                            prefs->backend_sync, luid, lgid,
                            capable->myname, RECOVERY_LITERAL, capable->mainpid);

  lives_popen(com, FALSE, mainw->msg);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  }

  recpid = atoi(mainw->msg);
  if (recpid == 0) {
    recfname4 = lives_strdup_printf("%s-%s", RECORDED_LITERAL, LAYOUT_NUMBERING_FILENAME);

    com = lives_strdup_printf("%s get_recovery_file %d %d %s %s %d",
                              prefs->backend_sync, luid, lgid,
                              capable->myname, recfname4, capable->mainpid);

    lives_popen(com, FALSE, mainw->msg);
    lives_free(com); lives_free(recfname4);

    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      return FALSE;
    }
    recpid = atoi(mainw->msg);
  } else {
    recfname = lives_strdup_printf("%s.%d.%d.%d", RECOVERY_LITERAL, luid, lgid, recpid);
    recovery_file = lives_build_filename(prefs->workdir, recfname, NULL);
    lives_free(recfname);

    // PROMPT TO RELOAD CLIPS
    if (!no_recover) retval = recover_files(recovery_file, auto_recover);
    /////////////

    else lives_rm(recovery_file);
    lives_free(recovery_file);

    found_clips = TRUE;
  }

  if (!retval || prefs->vj_mode) {
    com = lives_strdup_printf("%s clean_recovery_files %d %d \"%s\" %d %d",
                              prefs->backend_sync, luid, lgid, capable->myname,
                              capable->mainpid, prefs->vj_mode);
    lives_system(com, FALSE);
    lives_free(com);
    if (!no_recover || prefs->vj_mode) {
      if (!no_recover && prefs->vj_mode) {
        rewrite_recovery_file();
        return TRUE;
      }
      return FALSE;
    }
  }

  if (!no_recover) {
#if !GTK_CHECK_VERSION(3, 0, 0)
    if (CURRENT_CLIP_IS_VALID) {
      showclipimgs();
      lives_widget_queue_resize(mainw->video_draw);
      lives_widget_queue_resize(mainw->laudio_draw);
      lives_widget_queue_resize(mainw->raudio_draw);
    }
#endif
  }

  THREADVAR(com_failed) = FALSE;

  /// CRITICAL: make sure this gets called even on system failure and abort
  if (prefs->crash_recovery && !no_recover)
    rewrite_recovery_lpt = lives_hook_append(mainw->global_hook_stacks, FATAL_HOOK, 0, rewrite_recovery_file_cb, NULL);

  // check for layout recovery file
  recfname = lives_strdup_printf("%s.%d.%d.%d.%s", LAYOUT_FILENAME, luid, lgid, recpid,
                                 LIVES_FILE_EXT_LAYOUT);
  recovery_file = lives_build_filename(prefs->workdir, recfname, NULL);
  lives_free(recfname);

  recfname2 = lives_strdup_printf("%s.%d.%d.%d", LAYOUT_NUMBERING_FILENAME, luid, lgid, recpid);
  recovery_numbering_file = lives_build_filename(prefs->workdir, recfname2, NULL);
  if (!no_recover) lives_free(recfname2);

  recfname3 = lives_strdup_printf("%s-%s.%d.%d.%d.%s", RECORDED_LITERAL, LAYOUT_FILENAME, luid, lgid, recpid,
                                  LIVES_FILE_EXT_LAYOUT);
  recording_file = lives_build_filename(prefs->workdir, recfname3, NULL);
  if (!no_recover) lives_free(recfname3);

  recfname4 = lives_strdup_printf("%s-%s.%d.%d.%d", RECORDED_LITERAL, LAYOUT_NUMBERING_FILENAME,
                                  luid, lgid, recpid);
  recording_numbering_file = lives_build_filename(prefs->workdir, recfname4, NULL);
  if (!no_recover) lives_free(recfname4);

  uldir = lives_build_path(prefs->workdir, UNREC_LAYOUTS_DIR, NULL);

  if (!lives_file_test(recovery_file, LIVES_FILE_TEST_EXISTS)) {
    lives_free(recovery_file);
    recfname = lives_strdup_printf("%s.%d.%d.%d", LAYOUT_FILENAME, luid, lgid, recpid);
    recovery_file = lives_build_filename(prefs->workdir, recfname, NULL);
    if (lives_file_test(recovery_file, LIVES_FILE_TEST_EXISTS)) {
      if (no_recover) {
        if (!lives_file_test(uldir, LIVES_FILE_TEST_IS_DIR)) {
          lives_mkdir_with_parents(uldir, capable->umask);
        }
        ulf = lives_strdup_printf("%s.%lu", recfname, uid);
        ulfile = lives_build_filename(uldir, ulf, NULL);
        lives_free(ulf); lives_free(recfname);
        lives_mv(recovery_file, ulfile);
        lives_free(ulfile);
      } else found_layout = TRUE;
    }
    lives_free(recfname);
  } else {
    if (no_recover) lives_rm(recovery_file);
    found_layout = TRUE;
  }

  if (!found_layout && !no_recover) {
    if (lives_file_test(recovery_numbering_file, LIVES_FILE_TEST_EXISTS)) {
      if (lives_file_test(recording_file, LIVES_FILE_TEST_EXISTS)) {
        goto cleanse;
      }
    }
  }

  if (found_layout) {
    if (!lives_file_test(recovery_numbering_file, LIVES_FILE_TEST_EXISTS)) {
      found_layout = FALSE;
    } else if (no_recover) {
      if (!lives_file_test(uldir, LIVES_FILE_TEST_IS_DIR)) {
        lives_mkdir_with_parents(uldir, capable->umask);
      }
      ulf = lives_strdup_printf("%s.%lu", recfname2, uid);
      ulfile = lives_build_filename(uldir, ulf, NULL);
      lives_free(ulf);
      lives_mv(recovery_numbering_file, ulfile);
      lives_free(ulfile);
    }
  }

  if (no_recover) {
    if (lives_file_test(recording_file, LIVES_FILE_TEST_EXISTS)) {
      if (!lives_file_test(uldir, LIVES_FILE_TEST_IS_DIR)) {
        lives_mkdir_with_parents(uldir, capable->umask);
      }
      ulf = lives_strdup_printf("%s.%lu", recfname3, uid);
      ulfile = lives_build_filename(uldir, ulf, NULL);
      lives_free(recfname3); lives_free(ulf);
      lives_mv(recording_file, ulfile);
      lives_free(ulfile);
    }
    if (lives_file_test(recording_numbering_file, LIVES_FILE_TEST_EXISTS)) {
      if (!lives_file_test(uldir, LIVES_FILE_TEST_IS_DIR)) {
        lives_mkdir_with_parents(uldir, capable->umask);
      }
      ulf = lives_strdup_printf("%s.%lu", recfname4, uid);
      ulfile = lives_build_filename(uldir, ulf, NULL);
      lives_free(recfname4); lives_free(ulf);
      lives_mv(recording_numbering_file, ulfile);
      lives_free(ulfile);
    }
    found_layout = found_recording = FALSE;
  } else {
    if (prefs->rr_crash && lives_file_test(recording_file, LIVES_FILE_TEST_EXISTS)) {
      if (lives_file_test(recording_numbering_file, LIVES_FILE_TEST_EXISTS)) {
        found_recording = TRUE;
        xfile = lives_strdup_printf("%s/keep_%s-%s.%d.%d.%d", prefs->workdir, RECORDED_LITERAL,
                                    LAYOUT_FILENAME, luid, lgid, lpid);
        lives_mv(recording_file, xfile);
        lives_free(xfile);
        xfile = lives_strdup_printf("%s/keep_%s-%s.%d.%d.%d", prefs->workdir, RECORDED_LITERAL,
                                    LAYOUT_NUMBERING_FILENAME, luid, lgid, lpid);
        lives_mv(recording_numbering_file, xfile);
        lives_free(xfile);

        mainw->recording_recovered = TRUE;
      }
    }
  }

  if (found_layout) {
    // move files temporarily to stop them being cleansed
    xfile = lives_strdup_printf("%s/keep_%s.%d.%d.%d", prefs->workdir, LAYOUT_FILENAME, luid, lgid, lpid);
    lives_mv(recovery_file, xfile);
    lives_free(xfile);
    xfile = lives_strdup_printf("%s/keep_%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME,
                                luid, lgid, lpid);
    lives_mv(recovery_numbering_file, xfile);
    lives_free(xfile);
    mainw->recoverable_layout = TRUE;
  }

cleanse:

  if (!found_layout  && !found_recording) {
    if (mainw->scrap_file != -1) close_scrap_file(TRUE);
    if (mainw->ascrap_file != -1) close_ascrap_file(TRUE);
  }

  if (uldir) lives_free(uldir);

  lives_free(recovery_file);
  lives_free(recovery_numbering_file);
  lives_free(recording_file);
  lives_free(recording_numbering_file);

  if (THREADVAR(com_failed) && prefs->crash_recovery && !no_recover) {
    rewrite_recovery_file();
    lives_hook_remove(rewrite_recovery_lpt);
    return FALSE;
  }

  com = lives_strdup_printf("%s clean_recovery_files %d %d \"%s\" %d 0",
                            prefs->backend_sync, luid, lgid, capable->myname,
                            capable->mainpid);
  lives_system(com, FALSE);
  lives_free(com);

  if (no_recover) goto show_err;

  recovery_file = lives_strdup_printf("%s/%s.%d.%d.%d.%s", prefs->workdir,
                                      LAYOUT_FILENAME, luid, lgid, lpid, LIVES_FILE_EXT_LAYOUT);
  recovery_numbering_file = lives_strdup_printf("%s/%s.%d.%d.%d", prefs->workdir,
                            LAYOUT_NUMBERING_FILENAME, luid, lgid, lpid);

  if (mainw->recoverable_layout) {
    // move files back
    xfile = lives_strdup_printf("%s/keep_%s.%d.%d.%d", prefs->workdir, LAYOUT_FILENAME, luid, lgid, lpid);
    lives_mv(xfile, recovery_file);
    lives_free(xfile);
    xfile = lives_strdup_printf("%s/keep_%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME,
                                luid, lgid, lpid);
    lives_mv(xfile, recovery_numbering_file);
    lives_free(xfile);
  }

  recording_file = lives_strdup_printf("%s/%s-%s.%d.%d.%d.%s", prefs->workdir, RECORDED_LITERAL,
                                       LAYOUT_FILENAME, luid, lgid, lpid, LIVES_FILE_EXT_LAYOUT);
  recording_numbering_file = lives_strdup_printf("%s/%s-%s.%d.%d.%d", prefs->workdir, RECORDED_LITERAL,
                             LAYOUT_NUMBERING_FILENAME, luid, lgid, lpid);

  if (mainw->recording_recovered) {
    xfile = lives_strdup_printf("%s/keep_%s-%s.%d.%d.%d", prefs->workdir, RECORDED_LITERAL, LAYOUT_FILENAME,
                                luid, lgid, lpid);
    /// may fail -> abort
    lives_mv(xfile, recording_file);
    lives_free(xfile);
    xfile = lives_strdup_printf("%s/keep_%s-%s.%d.%d.%d", prefs->workdir, RECORDED_LITERAL,
                                LAYOUT_NUMBERING_FILENAME, luid, lgid, lpid);
    lives_mv(xfile, recording_numbering_file);
    lives_free(xfile);
  }

  lives_free(recovery_file);
  lives_free(recovery_numbering_file);
  lives_free(recording_file);
  lives_free(recording_numbering_file);

  if (prefs->crash_recovery) {
    rewrite_recovery_file();
    lives_hook_remove(rewrite_recovery_lpt);
  }

show_err:

  if (!mainw->recoverable_layout && !mainw->recording_recovered) {
    if (found_clips) {
      if (mainw->invalid_clips
          && (prefs->warning_mask ^ (WARN_MASK_CLEAN_AFTER_CRASH | WARN_MASK_CLEAN_INVALID))
          == WARN_MASK_CLEAN_INVALID) do_after_invalid_warning();
      else do_after_crash_warning();
      mainw->invalid_clips = FALSE;
    }
  }
  return retval;
}



uint64_t deduce_file(const char *file_name, double start, int end) {
  // this is a utility function to deduce whether we are dealing with a file,
  // a selection, a backup, or a location
  char short_file_name[PATH_MAX];
  uint64_t uid;
  mainw->img_concat_clip = -1;

  if (lives_strrstr(file_name, "://") && strncmp(file_name, "dvd://", 6)) {
    mainw->opening_loc = TRUE;
    uid = open_file(file_name);
    mainw->opening_loc = FALSE;
  } else {
    lives_snprintf(short_file_name, PATH_MAX, "%s", file_name);
    if (!(strcmp(file_name + strlen(file_name) - 4, "."LIVES_FILE_EXT_BACKUP))) {
      uid = restore_file(file_name);
    } else {
      uid = open_file_sel(file_name, start, end);
    }
  }
  return uid;
}


uint64_t open_file(const char *file_name) {
  // this function should be called to open a whole file
  return open_file_sel(file_name, 0., 0);
}


void pad_with_silence(int clipno, boolean at_start, boolean is_auto) {
  lives_clip_t *sfile;

  if (!IS_NORMAL_CLIP(clipno)) return;
  sfile = mainw->files[clipno];

  if (at_start) {
    sfile->undo1_dbl = 0.;
    sfile->undo2_dbl = CLIP_TOTAL_TIME(clipno) - sfile->laudio_time;
  } else {
    sfile->undo1_dbl = sfile->laudio_time;
    sfile->undo2_dbl = CLIP_TOTAL_TIME(clipno);
  }

  sfile->undo_arate = sfile->arate;
  sfile->undo_signed_endian = sfile->signed_endian;
  sfile->undo_achans = sfile->achans;
  sfile->undo_asampsize = sfile->asampsize;
  sfile->undo_arps = sfile->arps;

  d_print(_("Padding %s with %.4f seconds of silence..."), at_start ? _("start") : _("end"),
          sfile->undo2_dbl - sfile->undo1_dbl);

  if (on_ins_silence_activate(NULL, LIVES_INT_TO_POINTER(clipno))) d_print_done();
  else d_print("\n");
}


#define GET_MD5
uint64_t open_file_sel(const char *file_name, double start, frames_t frames) {
  const lives_clip_data_t *cdata;
  weed_plant_t *mt_pb_start_event = NULL;

  char msg[256], loc[PATH_MAX];
#ifdef GET_MD5
  void *md5sum;
#endif
  char *tmp = NULL;
  char *isubfname = NULL;
  char *msgstr;
  char *com, *what;
  char *temp_backend;

  uint64_t clip_uid = 0;

  boolean mt_has_audio_file = TRUE;
  LiVESResponseType response;

  int withsound = 1;
  int old_file = mainw->current_file;
  int new_file = old_file;

  int achans, arate, arps, asampsize;
  int current_file;
  int extra_frames = 0;
  int probed_achans = 0;

  if (!mainw->opening_loc && !lives_file_test(file_name, LIVES_FILE_TEST_EXISTS)) {
    do_no_loadfile_error(file_name);
    return 0;
  }

  if (!CURRENT_CLIP_IS_VALID || !cfile->opening) {
    new_file = mainw->first_free_file;

    if (!get_new_handle(new_file, file_name)) {
      return 0;
    }

    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    //lives_widget_context_update();

    msgstr = lives_strdup_printf(_("Opening %s"), file_name);
    d_print(msgstr);
    if (start > 0.) d_print(_(" from time %.2f"), start);
    if (frames > 0) d_print(_(" max. frames %d"), frames);

    if (!mainw->save_with_sound) {
      d_print(_(" without sound"));
      withsound = 0;
    }

    d_print(""); // exhaust "switch" message

    do_threaded_dialog(msgstr, TRUE);
    threaded_dialog_spin(0.);
    threaded_dialog_auto_spin();

    mainw->current_file = new_file;

    /// probe the file to see what it might be...
    read_file_details(file_name, FALSE, FALSE);
    lives_rm(cfile->info_file);
    if (THREADVAR(com_failed)) {
      end_threaded_dialog();
      lives_free(msgstr);
      return 0;
    }

    if (*mainw->msg) add_file_info(cfile->handle, FALSE);

    if (mainw->multitrack) {
      // set up for opening preview
      mt_pb_start_event = mainw->multitrack->pb_start_event;
      mt_has_audio_file = mainw->multitrack->has_audio_file;
      mainw->multitrack->pb_start_event = NULL;
      mainw->multitrack->has_audio_file = TRUE;
    }

    if (cfile->header_version < 104)
      cfile->img_type = lives_image_ext_to_img_type(prefs->image_ext);

    if ((!strcmp(cfile->type, LIVES_IMAGE_TYPE_JPEG) || !strcmp(cfile->type, LIVES_IMAGE_TYPE_PNG))) {
      lives_free(msgstr);
      read_file_details(file_name, FALSE, TRUE);
      add_file_info(cfile->handle, FALSE);
      if (cfile->frames == 0) {
        d_print_failed();
        close_current_file(old_file);
        if (mainw->multitrack) {
          mainw->multitrack->pb_start_event = mt_pb_start_event;
          mainw->multitrack->has_audio_file = mt_has_audio_file;
        }
        lives_freep((void **)&mainw->file_open_params);
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        end_threaded_dialog();
        return 0;
      }
      goto img_load;
    }

    add_primary_src(mainw->current_file, NULL, LIVES_SRC_TYPE_IMAGE);

    if (prefs->instant_open && !mainw->opening_loc) {
      // cd to clip directory - so decoder plugins can write temp files
      char *clipdir = get_clip_dir(mainw->current_file);
      char *cwd = lives_get_current_dir();

      load_decoders();

      lives_chdir(clipdir, FALSE);
      lives_free(clipdir);

      cdata = get_decoder_cdata(mainw->current_file, NULL);

      lives_chdir(cwd, FALSE);
      lives_free(cwd);

      if (cdata) {
        frames_t st_frame = cdata->fps * start;
        if (cfile->frames > cdata->nframes && cfile->frames != 123456789) {
          extra_frames = cfile->frames - cdata->nframes;
        }
        cfile->frames = cdata->nframes;

        if (st_frame >= cfile->frames) {
          lives_free(msgstr);
          return 0;
        }

        if (!frames) {
          frames = cfile->frames - st_frame;
        } else {
          if (st_frame + frames >= cfile->frames) frames = cfile->frames - st_frame;
        }

        cfile->frames = frames;

        cfile->start = 1;
        cfile->end = cfile->frames;

        cfile->opening = TRUE;

        cfile->img_type = IMG_TYPE_BEST; // override the pref

        cfile->hsize = cdata->width;
        cfile->vsize = cdata->height;

        what = (_("creating the frame index for the clip"));

        do {
          response = LIVES_RESPONSE_NONE;
          create_frame_index(mainw->current_file, TRUE, st_frame, frames);
          if (!cfile->frame_index) {
            end_threaded_dialog();
            response = do_memory_error_dialog(what, frames * 4);
          }
        } while (response == LIVES_RESPONSE_RETRY);
        lives_free(what);
        if (response == LIVES_RESPONSE_CANCEL) {
          lives_free(msgstr);
          return 0;
        }

        if (response == LIVES_RESPONSE_OK) {
          do_threaded_dialog(msgstr, TRUE);
          threaded_dialog_spin(0.);
          threaded_dialog_auto_spin();
        }

        lives_free(msgstr);

        if (!*cfile->author)
          lives_snprintf(cfile->author, 1024, "%s", cdata->author);
        if (!*cfile->title)
          lives_snprintf(cfile->title, 1024, "%s", cdata->title);
        if (!*cfile->comment)
          lives_snprintf(cfile->comment, 1024, "%s", cdata->comment);

        probed_achans = cdata->achans;
        cfile->arate = cfile->arps = cdata->arate;
        cfile->asampsize = cdata->asamps;

        cfile->signed_endian =
          get_signed_endian(cdata->asigned, capable->hw.byte_order == LIVES_LITTLE_ENDIAN);

        cfile->fps = cfile->pb_fps = cdata->fps;

        d_print("\n");

        if ((cfile->achans > 0 || probed_achans > 0) && withsound == 1) {
          // plugin returned no audio, try with mplayer / mpv
          if (probed_achans > MAX_ACHANS) {
            probed_achans = MAX_ACHANS;
            d_print(_("Forcing audio channels to %d\n"), MAX_ACHANS);
          }

          if (!mainw->file_open_params) {
#if 1
            mainw->file_open_params = lives_strdup("alang eng");
#else
            mainw->file_open_params = lives_strdup("");
#endif
          }
          temp_backend = use_staging_dir_for(mainw->current_file);
          com = lives_strdup_printf("%s open \"%s\" \"%s\" %d %s:%s %.2f %d %d \"%s\"",
                                    temp_backend, cfile->handle,
                                    (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)),
                                    -1, prefs->image_ext, get_image_ext_for_type(IMG_TYPE_BEST),
                                    start, frames, probed_achans, mainw->file_open_params);
          lives_free(temp_backend);
          lives_free(tmp);
          lives_system(com, FALSE);
          lives_free(com);

          // if we have a quick-opening file, display the first and last frames now
          // for some codecs this can be helpful since we can locate the last frame
          // while audio is loading
          if (cfile->clip_type == CLIP_TYPE_FILE && !LIVES_IS_PLAYING) resize(1);

          mainw->effects_paused = FALSE; // set to TRUE if user clicks "Enough"

          end_threaded_dialog();

          msgstr = lives_strdup_printf(_("Opening audio"), file_name);
          if (!do_progress_dialog(TRUE, TRUE, msgstr)) {
            use_staging_dir_for(0);
            // error or user cancelled or switched to another clip
            lives_free(msgstr);

            cfile->opening_frames = -1;

            if (mainw->multitrack) {
              mainw->multitrack->pb_start_event = mt_pb_start_event;
              mainw->multitrack->has_audio_file = mt_has_audio_file;
            }

            if (mainw->cancelled == CANCEL_NO_PROPOGATE) {
              lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
              lives_widget_context_update();
              mainw->cancelled = CANCEL_NONE;
              return 0;
            }

            // cancelled
            if (mainw->cancelled != CANCEL_ERROR && mainw->cancel_type != CANCEL_INTERRUPT) {
              lives_kill_subprocesses(cfile->handle, TRUE);
            }

            lives_freep((void **)&mainw->file_open_params);
            close_current_file(old_file);
            lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
            lives_widget_context_update();
            if (mainw->error) {
              do_error_dialog(mainw->msg);
              mainw->error = 0;
              clear_mainw_msg();
            }
            sensitize();
            return 0;
          }
          use_staging_dir_for(0);

          lives_free(msgstr);

          cfile->opening = FALSE;

          if (mainw->error == 0) add_file_info(cfile->handle, TRUE);
          mainw->error = 0;
          get_total_time(cfile);

          if (prefs->auto_trim_audio) {
            if ((cdata->sync_hint & SYNC_HINT_VIDEO_PAD_START) && cdata->video_start_time <= 1.) {
              // pad with blank frames at start
              int st_extra_frames = cdata->video_start_time * cfile->fps;
              end_threaded_dialog();
              insert_blank_frames(mainw->current_file, st_extra_frames, 0, WEED_PALETTE_RGB24);
              cfile->video_time += st_extra_frames / cfile->fps;
              extra_frames -= st_extra_frames;
              showclipimgs();
              if (!mainw->multitrack)
                redraw_timeline(mainw->current_file);
            }

            if ((cfile->frames + extra_frames) / cfile->fps > cfile->laudio_time) {
              extra_frames = (cfile->laudio_time - (double)cfile->frames / cfile->fps) * cfile->fps;
            }

            if ((cdata->sync_hint & SYNC_HINT_VIDEO_PAD_END)
                && (double)cfile->frames / cfile->fps < cfile->laudio_time) {
              // pad with blank frames at end
              if (cdata->sync_hint & SYNC_HINT_VIDEO_PAD_END) {
                int xextra_frames =
                  (cfile->laudio_time - (double)cfile->frames / cfile->fps) * cfile->fps;
                if (xextra_frames > extra_frames) extra_frames = xextra_frames;
              }
              insert_blank_frames(mainw->current_file, extra_frames, cfile->frames, WEED_PALETTE_RGB24);
              cfile->video_time += extra_frames / cfile->fps;
              end_threaded_dialog();
              load_end_image(cfile->end);
            }
            if (cfile->laudio_time > cfile->video_time + AV_TRACK_MIN_DIFF && cfile->frames > 0) {
              if (cdata->sync_hint & SYNC_HINT_AUDIO_TRIM_START) {
                cfile->undo1_dbl = 0.;
                cfile->undo2_dbl = cfile->laudio_time - cfile->video_time;
                d_print(_("Auto trimming %.4f seconds of audio at start..."), cfile->undo2_dbl);
                cfile->opening_audio = TRUE;
                end_threaded_dialog();
                if (on_del_audio_activate(NULL, NULL)) d_print_done();
                else d_print("\n");
                cfile->changed = FALSE;
              }
            }
            if (cfile->laudio_time > cfile->video_time + AV_TRACK_MIN_DIFF && cfile->frames > 0) {
              if (cdata->sync_hint & SYNC_HINT_AUDIO_TRIM_END) {
                cfile->end = cfile->frames;
                d_print(_("Auto trimming %.4f seconds of audio at end..."),
                        cfile->laudio_time - cfile->video_time);
                cfile->undo1_dbl = cfile->video_time;
                cfile->undo2_dbl = cfile->laudio_time;
                end_threaded_dialog();
                cfile->opening_audio = TRUE;
                if (on_del_audio_activate(NULL, NULL)) d_print_done();
                else d_print("\n");
                cfile->changed = FALSE;
              }
            }
            if (!mainw->effects_paused && cfile->afilesize > 0 && cfile->achans > 0
                && CLIP_TOTAL_TIME(mainw->current_file) > cfile->laudio_time + AV_TRACK_MIN_DIFF) {
              if (cdata->sync_hint & SYNC_HINT_AUDIO_PAD_START) {
                end_threaded_dialog();
                pad_with_silence(mainw->current_file, TRUE, TRUE);
                cfile->changed = FALSE;
              }
              if (cdata->sync_hint & SYNC_HINT_AUDIO_PAD_END) {
                end_threaded_dialog();
                pad_with_silence(mainw->current_file, FALSE, TRUE);
                cfile->changed = FALSE;
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*
        cfile->opening_audio = FALSE;

        get_mime_type(cfile->type, 40, cdata);
        save_frame_index(mainw->current_file);
      } else lives_free(msgstr);
    } else lives_free(msgstr);

    if (get_primary_src(mainw->current_file)) {
      if (mainw->open_deint) {
        // override what the plugin says
        cfile->deinterlace = TRUE;
        cfile->interlace = LIVES_INTERLACE_TOP_FIRST; // guessing
        save_clip_value(mainw->current_file, CLIP_DETAILS_INTERLACE, &cfile->interlace);
        if (THREADVAR(com_failed) || THREADVAR(write_failed)) {
          end_threaded_dialog();
          do_header_write_error(mainw->current_file);
        }
      }
    } else {
      end_threaded_dialog();
      // be careful, here we switch from mainw->opening_loc to cfile->opening_loc
      if (mainw->opening_loc) {
        cfile->opening_loc = mainw->opening_loc = FALSE;
      } else {
        if (cfile->f_size > prefs->warn_file_size * 1000000. && mainw->is_ready && frames == 0) {
          char *fsize_ds = lives_format_storage_space_string((uint64_t)cfile->f_size);
          if (!do_warning_dialog_with_checkf(WARN_MASK_FSIZE,
                                             _("\nLiVES cannot Instant Open this file, it may take some time to load.\n"
                                               "File size is %s\n"
                                               "Are you sure you wish to continue ?"), fsize_ds)) {
            lives_free(fsize_ds);
            close_current_file(old_file);
            if (mainw->multitrack) {
              mainw->multitrack->pb_start_event = mt_pb_start_event;
              mainw->multitrack->has_audio_file = mt_has_audio_file;
            }
            lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
            lives_widget_context_update();
            sensitize();
            return 0;
          }
          lives_free(fsize_ds);
          d_print(_(" - please be patient."));

        }
        d_print("\n");
#if defined DEBUG
        g_print("open_file: dpd in\n");
#endif
      }
    }

    // set undo_start and undo_end for preview
    cfile->undo_start = 1;
    cfile->undo_end = cfile->frames;

    if (cfile->achans > 0) cfile->opening_audio = TRUE;

    // these will get reset as we have no audio file yet, so preserve them
    achans = cfile->achans;
    arate = cfile->arate;
    arps = cfile->arps;
    asampsize = cfile->asampsize;
    cfile->old_frames = cfile->frames;
    cfile->frames = 0;

    // force a resize
    current_file = mainw->current_file;

    cfile->opening = TRUE;
    cfile->achans = achans;
    cfile->arate = arate;
    cfile->arps = arps;
    cfile->asampsize = asampsize;
    cfile->frames = cfile->old_frames;

    if (cfile->frames <= 0) {
      cfile->undo_end = cfile->frames = 123456789;
    }
    if (cfile->hsize * cfile->vsize == 0) {
      cfile->frames = 0;
    }

    if (!mainw->multitrack) get_play_times();

    add_to_clipmenu();
    set_main_title(cfile->file_name, 0);

    mainw->effects_paused = FALSE;

    if (!get_primary_src(mainw->current_file)) {
      migrate_from_staging(mainw->current_file);

      if (!mainw->file_open_params) mainw->file_open_params = lives_strdup("");

      tmp = lives_strconcat(mainw->file_open_params, get_deinterlace_string(), NULL);
      lives_free(mainw->file_open_params);
      mainw->file_open_params = tmp;

      if (cfile->achans > MAX_ACHANS) {
        cfile->achans = MAX_ACHANS;
        d_print(_("Forcing audio channels to %d\n"), MAX_ACHANS);
      }

      com = lives_strdup_printf("%s open \"%s\" \"%s\" %d %s:%s %.2f %d %d \"%s\"",
                                prefs->backend, cfile->handle,
                                (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)),
                                withsound, prefs->image_ext, get_image_ext_for_type(IMG_TYPE_BEST),
                                start, frames, cfile->achans, mainw->file_open_params);

      //g_print("cmd is %s\n", com);

      lives_free(tmp);
      lives_system(com, FALSE);
      lives_free(com);

      if (mainw->toy_type == LIVES_TOY_TV) {
        // for LiVES TV we do an auto-preview
        end_threaded_dialog();
        mainw->play_start = cfile->start = cfile->undo_start;
        mainw->play_end = cfile->end = cfile->undo_end;
        mainw->preview = TRUE;
        do {
          desensitize();
          procw_desensitize();
          on_playsel_activate(NULL, NULL);
        } while (mainw->cancelled == CANCEL_KEEP_LOOPING);
        mainw->preview = FALSE;
        on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
        lives_freep((void **)&mainw->file_open_params);
        mainw->cancelled = CANCEL_NONE;
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        lives_widget_context_update();
        return 0;
      }
    }

    //  loading:

    // 'entry point' when we switch back

    // spin until loading is complete
    // afterwards, mainw->msg will contain file details
    cfile->progress_start = cfile->progress_end = 0;

    // (also check for cancel)
    msgstr = lives_strdup_printf(_("Opening %s"), file_name);

    if (!get_primary_src(mainw->current_file) && mainw->toy_type != LIVES_TOY_TV) {
      end_threaded_dialog();
      mainw->disk_mon = MONITOR_QUOTA;
      if (!do_progress_dialog(TRUE, TRUE, msgstr)) {
        // user cancelled or switched to another clip
        mainw->disk_mon = 0;

        lives_free(msgstr);
        mainw->effects_paused = FALSE;

        if (mainw->cancelled == CANCEL_NO_PROPOGATE) {
          mainw->cancelled = CANCEL_NONE;
          lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
          lives_widget_context_update();
          return 0;
        }

        // cancelled
        // clean up our temp files
        if (IS_VALID_CLIP(current_file)) mainw->current_file = current_file;
        lives_kill_subprocesses(cfile->handle, TRUE);
        lives_freep((void **)&mainw->file_open_params);
        close_current_file(old_file);
        if (mainw->multitrack) {
          mainw->multitrack->pb_start_event = mt_pb_start_event;
          mainw->multitrack->has_audio_file = mt_has_audio_file;
        }
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        lives_widget_context_update();

        // mainw->error is TRUE if we could not open the file
        if (mainw->error) {
          d_print_failed();
          do_error_dialog(mainw->msg);
        }
        if (!mainw->multitrack)
          redraw_timeline(mainw->current_file);
        showclipimgs();
        sensitize();
        return 0;
      }
      mainw->disk_mon = 0;
    }
    lives_free(msgstr);
  }

  if (get_primary_src(mainw->current_file) && cfile->achans > 0) {
    char *afile = get_audio_file_name(mainw->current_file, TRUE);
    char *ofile = get_audio_file_name(mainw->current_file, FALSE);
    rename(afile, ofile);
    lives_free(afile);
    lives_free(ofile);
  }

  cfile->opening = cfile->opening_audio = cfile->opening_only_audio = FALSE;
  cfile->opening_frames = -1;
  mainw->effects_paused = FALSE;

#if defined DEBUG
  g_print("Out of dpd\n");
#endif

  if (mainw->multitrack) {
    mainw->multitrack->pb_start_event = mt_pb_start_event;
    mainw->multitrack->has_audio_file = mt_has_audio_file;
  }

  // mainw->error is TRUE if we could not open the file
  if (mainw->error) {
    end_threaded_dialog();
    do_error_dialog(mainw->msg);
    d_print_failed();
    close_current_file(old_file);
    lives_freep((void **)&mainw->file_open_params);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    lives_widget_context_update();
    sensitize();
    return 0;
  }

  if (cfile->opening_loc) {
    cfile->changed = TRUE;
    cfile->opening_loc = FALSE;
  } else {
    if (prefs->autoload_subs) {
      char filename[512];
      char *subfname;
      lives_subtitle_type_t subtype = SUBTITLE_TYPE_NONE;

      lives_snprintf(filename, 512, "%s", file_name);
      get_filename(filename, FALSE); // strip extension
      isubfname = lives_strdup_printf("%s.%s", filename, LIVES_FILE_EXT_SRT);
      if (lives_file_test(isubfname, LIVES_FILE_TEST_EXISTS)) {
        char *clipdir = get_clip_dir(mainw->current_file);
        subfname = lives_build_filename(clipdir, SUBS_FILENAME "." LIVES_FILE_EXT_SRT, NULL);
        lives_free(clipdir);
        subtype = SUBTITLE_TYPE_SRT;
      } else {
        lives_free(isubfname);
        isubfname = lives_strdup_printf("%s.%s", filename, LIVES_FILE_EXT_SUB);
        if (lives_file_test(isubfname, LIVES_FILE_TEST_EXISTS)) {
          char *clipdir = get_clip_dir(mainw->current_file);
          subfname = lives_build_filename(clipdir, SUBS_FILENAME "." LIVES_FILE_EXT_SUB, NULL);
          lives_free(clipdir);
          subtype = SUBTITLE_TYPE_SUB;
        }
      }
      if (subtype != SUBTITLE_TYPE_NONE) {
        lives_cp(isubfname, subfname);
        if (!THREADVAR(com_failed))
          subtitles_init(cfile, subfname, subtype);
        lives_free(subfname);
      } else {
        lives_freep((void **)&isubfname);
      }
    }
  }

  // now file should be loaded...get full details

  if (!get_primary_src(mainw->current_file)) add_file_info(cfile->handle, FALSE);

  cfile->is_loaded = TRUE;

  if (cfile->frames <= 0) {
    if (cfile->afilesize == 0l) {
      // we got neither video nor audio...
      lives_snprintf(msg, 256, "%s", _
                     ("\n\nLiVES was unable to extract either video or audio.\n"
                      "Please check the terminal window for more details.\n"));

      if (!capable->has_mplayer && !capable->has_mplayer2 && !capable->has_mpv) {
        lives_strappend(msg, 256, _("\n\nYou may need to install mplayer, "
                                    "mplayer2 or mpv to open this file.\n"));
      } else {
        if (capable->has_mplayer) {
          get_location(EXEC_MPLAYER, loc, PATH_MAX);
        } else if (capable->has_mplayer2) {
          get_location(EXEC_MPLAYER2, loc, PATH_MAX);
        } else if (capable->has_mpv) {
          get_location(EXEC_MPV, loc, PATH_MAX);
        }

        if (strcmp(prefs->video_open_command, loc) && strncmp(prefs->video_open_command + 1, loc, strlen(loc))) {
          lives_strappend(msg, 256, _("\n\nPlease check the setting of Video Open Command in\n"
                                      "Tools|Preferences|Decoding\n"));
        }
      }
      end_threaded_dialog();
      widget_opts.non_modal = TRUE;
      do_error_dialog(msg);
      widget_opts.non_modal = FALSE;
      d_print_failed();
      close_current_file(old_file);
      if (mainw->multitrack) {
        mainw->multitrack->pb_start_event = mt_pb_start_event;
        mainw->multitrack->has_audio_file = mt_has_audio_file;
      }
      lives_freep((void **)&mainw->file_open_params);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      lives_widget_context_update();
      sensitize();
      return 0;
    }
    cfile->frames = 0;
  }

  if (!get_primary_src(mainw->current_file)) {
    extra_frames = cfile->frames;
    add_file_info(cfile->handle, FALSE);
    extra_frames -= cfile->frames;
    cfile->end = cfile->frames;
    cfile->video_time = cfile->frames / cfile->fps;
  } else {
    add_file_info(NULL, FALSE);
    if (cfile->f_size == 0) {
      off_t fsize = sget_file_size((char *)file_name);
      if (fsize < 0) fsize = 0;
      cfile->f_size = (size_t)fsize;
    }
  }

  if (!get_primary_src(mainw->current_file)) {
    reget_afilesize(mainw->current_file);
    if (prefs->auto_trim_audio || prefs->keep_all_audio) {
      if (cfile->laudio_time > cfile->video_time && cfile->frames > 0) {
        if (!prefs->keep_all_audio || start != 0. || extra_frames <= 0) {
          d_print(_("Auto trimming %.2f seconds of audio at end..."),
                  cfile->laudio_time - cfile->video_time);
          cfile->undo1_dbl = cfile->video_time;
          cfile->undo2_dbl = cfile->laudio_time - cfile->video_time;
          cfile->opening_audio = TRUE;
          end_threaded_dialog();
          if (on_del_audio_activate(NULL, NULL)) d_print_done();
          else d_print("\n");
          cfile->changed = FALSE;
        } else {
          /// insert blank frames
          if (prefs->keep_all_audio && (cfile->laudio_time - cfile->video_time)
              * cfile->fps > extra_frames)
            extra_frames = (cfile->laudio_time - cfile->video_time) * cfile->fps;
          insert_blank_frames(mainw->current_file, extra_frames, cfile->frames, WEED_PALETTE_RGB24);
          cfile->video_time += extra_frames / cfile->fps;
          cfile->end = cfile->frames;
          end_threaded_dialog();
          showclipimgs();
          if (!mainw->multitrack)
            redraw_timeline(mainw->current_file);
        }
      }
      if (cfile->laudio_time < cfile->video_time && cfile->achans > 0) {
        pad_with_silence(mainw->current_file, FALSE, TRUE);
        cfile->changed = FALSE;
      }
    }
  }

  if (isubfname) {
    d_print(_("Loaded subtitle file: %s\n"), isubfname);
    lives_free(isubfname);
  }

  if (mainw->save_with_sound && !cfile->achans && probed_achans) {
    char *afname = get_audio_file_name(mainw->current_file, FALSE);
    if (lives_file_test(afname, LIVES_FILE_TEST_EXISTS)) {
      off_t fsize = sget_file_size(afname);
      if (fsize > 0) {
        LiVESResponseType resp;
        end_threaded_dialog();
        do {
          if (!do_yesno_dialog_with_check(_("This clip may have damaged audio.\n"
                                            "Should I attempt to load it anyway ?\n"),
                                          WARN_MASK_DMGD_AUDIO)) {
            d_print_cancelled();
            close_current_file(old_file);
            lives_freep((void **)&mainw->file_open_params);
            lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
            lives_widget_context_update();
            sensitize();
            return 0;
          }
          mainw->fx3_val = cfile->asampsize ? cfile->asampsize : DEFAULT_AUDIO_SAMPS;
          if (!cfile->achans && !cfile->signed_endian) {
            mainw->fx4_val = (mainw->fx3_val == 8) ? DEFAULT_AUDIO_SIGNED8 :
                             DEFAULT_AUDIO_SIGNED16 | ((capable->hw.byte_order == LIVES_BIG_ENDIAN)
                                 ? AFORM_BIG_ENDIAN : 0);

            mainw->fx2_val = cfile->achans ? cfile->achans : DEFAULT_AUDIO_CHANS;
            mainw->fx1_val = cfile->arate ? cfile->arate : DEFAULT_AUDIO_RATE;
            resaudw = create_resaudw(12, NULL, NULL);
            resp = lives_dialog_run(LIVES_DIALOG(resaudw->dialog));
            if (resp == LIVES_RESPONSE_OK) {
              audio_details_clicked(NULL, LIVES_INT_TO_POINTER(mainw->current_file));
              redraw_timeline(mainw->current_file);
            } else lives_widget_destroy(resaudw->dialog);
            lives_freep((void **)&resaudw);
	    // *INDENT-OFF*
          }} while (resp == LIVES_RESPONSE_CANCEL);
      }}}
  // *INDENT-ON*

img_load:
  current_file = mainw->current_file;
#define GET_MD5
#ifdef GET_MD5
  md5sum = get_md5sum(file_name);

  // TODO - this should be attached to a clip_src
  //lives_memcpy(cfile->ext_id.md5sum, md5sum, MD5_SIZE);
  g_print("md5sum is: ");
  md5_print(md5sum);
  g_print("\n");
  lives_free(md5sum);
#endif

  // TODO - prompt for copy to origs (unless it is already there)

  if (prefs->show_recent && !mainw->is_generating) {
    lives_proc_thread_create(0, (lives_funcptr_t)add_to_recent, 0, "sdis",
                             file_name, start, frames, mainw->file_open_params);
  }

  lives_freep((void **)&mainw->file_open_params);

  if (!strcmp(cfile->type, "Frames") || !strcmp(cfile->type, LIVES_IMAGE_TYPE_JPEG) ||
      !strcmp(cfile->type, LIVES_IMAGE_TYPE_PNG) ||
      !strcmp(cfile->type, "Audio")) {
    cfile->is_untitled = TRUE;
  }

  end_threaded_dialog();

  if ((!strcmp(cfile->type, LIVES_IMAGE_TYPE_JPEG) || !strcmp(cfile->type, LIVES_IMAGE_TYPE_PNG))) {
    migrate_from_staging(mainw->current_file);
    if (!prefs->concat_images || mainw->img_concat_clip == -1) {
      cfile->img_type = lives_image_type_to_img_type(cfile->type);
      mainw->img_concat_clip = mainw->current_file;
      add_to_clipmenu();
      set_main_title(cfile->file_name, 0);
      cfile->opening = cfile->opening_audio = cfile->opening_only_audio = FALSE;
      cfile->opening_frames = -1;
      mainw->effects_paused = FALSE;
      cfile->is_loaded = TRUE;
      add_primary_src(mainw->current_file, NULL, LIVES_SRC_TYPE_IMAGE);
    } else {
      // insert this image into our image clip, close this file
      com = lives_strdup_printf("%s insert \"%s\" \"%s\" %d 1 1 \"%s\" 0 %d %d %d", prefs->backend,
                                mainw->files[mainw->img_concat_clip]->handle,
                                get_image_ext_for_type(mainw->files[mainw->img_concat_clip]->img_type),
                                mainw->files[mainw->img_concat_clip]->frames,
                                cfile->handle, mainw->files[mainw->img_concat_clip]->frames,
                                mainw->files[mainw->img_concat_clip]->hsize,
                                mainw->files[mainw->img_concat_clip]->vsize);

      mainw->current_file = mainw->img_concat_clip;
      lives_system(com, FALSE);
      lives_free(com);

      do_auto_dialog(_("Adding image..."), 2);

      if (current_file != mainw->img_concat_clip) {
        mainw->current_file = current_file;
        close_current_file(mainw->img_concat_clip);
      }

      if (mainw->cancelled || mainw->error) {
        goto load_done;
      }

      cfile->frames++;
      cfile->end++;

      set_start_end_spins(mainw->current_file);

      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      lives_widget_context_update();
      sensitize();
      return 0;
    }
  }

  // set new style file details
  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    lives_widget_context_update();
    sensitize();
    return 0;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
  clip_uid = cfile->unique_id;

load_done:

  if (!mainw->multitrack) {
    // update widgets
    switch_clip(1, mainw->current_file, TRUE);
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
  } else {
    lives_mt *multi = mainw->multitrack;
    mainw->multitrack = NULL; // allow getting of afilesize
    current_file = mainw->current_file;
    mainw->current_file = -1; // stop framebars from being drawn
    reget_afilesize(current_file);
    mainw->current_file = current_file;
    mainw->multitrack = multi;
    get_total_time(cfile);
    if (!mainw->is_generating) mainw->current_file = mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack, current_file, TRUE);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    mt_clip_select(mainw->multitrack, TRUE);
  }

  if (*cfile->staging_dir) {
    lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)migrate_from_staging,
                             0, "i", mainw->current_file);
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_widget_context_update();
  check_storage_space(-1, FALSE);

  sensitize();
  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  return clip_uid;
}



