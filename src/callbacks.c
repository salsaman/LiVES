// callbacks.c
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "effects.h"
#include "resample.h"
#include "rte_window.h"
#include "events.h"
#include "audio.h"
#include "cvirtual.h"
#include "paramwindow.h"
#include "ce_thumbs.h"
#include "startup.h"
#include "diagnostics.h"

#ifdef LIBAV_TRANSCODE
#include "transcode.h"
#endif

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#ifdef HAVE_UNICAP
#include "videodev.h"
#endif

#ifdef ENABLE_OSC
#include "osc.h"
#endif

static char file_name[PATH_MAX];


void lives_notify(int msgnumber, const char *msgstring) {
#ifdef IS_LIBLIVES
  binding_cb(msgnumber, msgstring, mainw->id);
#endif
#ifdef ENABLE_OSC
  lives_osc_notify(msgnumber, msgstring);
#endif

#ifdef TEST_NOTIFY
  if (msgnumber == LIVES_OSC_NOTIFY_CLIPSET_OPENED) {
    char *details = lives_strdup_printf(_("'LiVES opened the clip set' '%s'"), msgstring);
    char *tmp = lives_strdup_printf("notify-send %s", details);
    lives_system(tmp, TRUE);
    lives_free(tmp);
    lives_free(details);
  }

  if (msgnumber == LIVES_OSC_NOTIFY_CLIPSET_SAVED) {
    char *details = lives_strdup_printf(_("'LiVES saved the clip set' '%s'"), msgstring);
    char *tmp = lives_strdup_printf("notify-send %s", details);
    lives_system(tmp, TRUE);
    lives_free(tmp);
    lives_free(details);
  }
#endif
}


LIVES_GLOBAL_INLINE void lives_notify_int(int msgnumber, int msgint) {
  char *tmp = lives_strdup_printf("%d", msgint);
  lives_notify(msgnumber, tmp);
  lives_free(tmp);
}


boolean on_LiVES_delete_event(LiVESWidget *widget, LiVESXEventDelete *event, livespointer user_data) {
  if (!LIVES_IS_INTERACTIVE) return TRUE;
  on_quit_activate(NULL, NULL);
  return TRUE;
}


static void cleanup_set_dir(const char *set_name) {
  // this function is called:
  // - when a set is saved and merged with an existing one
  // - when a set is deleted
  // - when the last clip in a set is closed

  char *lfiles, *ofile, *sdir;
  char *cdir = lives_build_filename(prefs->workdir, set_name, CLIPS_DIRNAME, NULL);

  do {
    // keep trying until backend has deleted the clip
    lives_rmdir(cdir, TRUE);
    if (lives_file_test(cdir, LIVES_FILE_TEST_IS_DIR)) {
      if (mainw->threaded_dialog) threaded_dialog_spin(0.);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      lives_usleep(prefs->sleep_time);
    }
  } while (lives_file_test(cdir, LIVES_FILE_TEST_IS_DIR));

  THREADVAR(com_failed) = FALSE;

  lives_free(cdir);

  // remove any stale lockfiles
  lfiles = SET_LOCK_FILES(set_name);
  lives_rmglob(lfiles);
  lives_free(lfiles);

  ofile = lives_build_filename(prefs->workdir, set_name, CLIP_ORDER_FILENAME, NULL);
  lives_rm(ofile);
  lives_free(ofile);

  lives_sync(1);

  sdir = lives_build_filename(prefs->workdir, set_name, NULL);
  lives_rmdir(sdir, FALSE); // set to FALSE in case the user placed extra files there
  lives_free(sdir);

  if (prefs->ar_clipset && !strcmp(prefs->ar_clipset_name, set_name)) {
    prefs->ar_clipset = FALSE;
    lives_memset(prefs->ar_clipset_name, 0, 1);
    set_string_pref(PREF_AR_CLIPSET, "");
  }
}

#ifndef VALGRIND_ON
#ifdef _lives_free
#undef  lives_free
#define lives_free(a) (mainw->is_exiting ? a : _lives_free(a))
#endif
#endif

void lives_exit(int signum) {
  char *cwd, *tmp, *com;
  int i;

  if (!mainw) _exit(0);

  if (!mainw->only_close) {
    mainw->is_exiting = TRUE;

    // unlock all mutexes to prevent deadlocks
#ifdef HAVE_PULSE_AUDIO
    /* if (mainw->pulsed != NULL || mainw->pulsed_read != NULL) */
    /*   pa_mloop_unlock(); */
#endif

    // recursive
    while (!pthread_mutex_unlock(&mainw->instance_ref_mutex));
    while (!pthread_mutex_unlock(&mainw->abuf_mutex));

    // non-recursive
    pthread_mutex_trylock(&mainw->abuf_frame_mutex);
    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
    pthread_mutex_trylock(&mainw->fxd_active_mutex);
    pthread_mutex_unlock(&mainw->fxd_active_mutex);
    pthread_mutex_trylock(&mainw->event_list_mutex);
    pthread_mutex_unlock(&mainw->event_list_mutex);
    pthread_mutex_trylock(&mainw->clip_list_mutex);
    pthread_mutex_unlock(&mainw->clip_list_mutex);
    pthread_mutex_trylock(&mainw->vpp_stream_mutex);
    pthread_mutex_unlock(&mainw->vpp_stream_mutex);
    pthread_mutex_trylock(&mainw->cache_buffer_mutex);
    pthread_mutex_unlock(&mainw->cache_buffer_mutex);
    pthread_mutex_trylock(&mainw->audio_filewriteend_mutex);
    pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
    pthread_mutex_trylock(&mainw->fbuffer_mutex);
    pthread_mutex_unlock(&mainw->fbuffer_mutex);
    pthread_mutex_trylock(&mainw->alarmlist_mutex);
    pthread_mutex_unlock(&mainw->alarmlist_mutex);
    // filter mutexes are unlocked in weed_unload_all

    if (pthread_mutex_trylock(&mainw->exit_mutex)) pthread_exit(NULL);

    if (mainw->memok && prefs->crash_recovery && mainw->record) {
      backup_recording(NULL, NULL);
    }

    //lives_threadpool_finish();
    //show_weed_stats();
  }

  if (mainw->is_ready) {
    if (mainw->multitrack != NULL && mainw->multitrack->idlefunc > 0) {
      //lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
    }

    threaded_dialog_spin(0.);

    if (mainw->toy_type != LIVES_TOY_NONE) {
      on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
    }

    if (mainw->alives_pgid > 0) {
      autolives_toggle(NULL, NULL);
    }

#ifdef VALGRIND_ON
    if (mainw->stored_event_list != NULL || mainw->sl_undo_mem != NULL) {
      stored_event_list_free_all(FALSE);
    }

    if (mainw->multitrack != NULL && !mainw->only_close) {
      lives_freep((void **)&mainw->multitrack->undo_mem);
    }

    if (mainw->multi_opts.set && !mainw->only_close && mainw->multi_opts.aparam_view_list != NULL) {
      lives_list_free(mainw->multi_opts.aparam_view_list);
    }
#endif

#ifdef VALGRIND_ON
    if (LIVES_IS_PLAYING) {
      lives_grab_remove(LIVES_MAIN_WINDOW_WIDGET);
      if (mainw->ext_playback) {
        pthread_mutex_lock(&mainw->vpp_stream_mutex);
        mainw->ext_audio = FALSE;
        pthread_mutex_unlock(&mainw->vpp_stream_mutex);
        if (mainw->vpp->exit_screen != NULL)(*mainw->vpp->exit_screen)(mainw->ptr_x, mainw->ptr_y);
        stop_audio_stream();
        mainw->stream_ticks = -1;
      }

      // tell non-realtime audio players (sox or mplayer) to stop
      if (!is_realtime_aplayer(prefs->audio_player) && mainw->aud_file_to_kill > -1 &&
          mainw->files[mainw->aud_file_to_kill] != NULL) {
        char *lsname = lives_build_filename(prefs->workdir, mainw->files[mainw->aud_file_to_kill]->handle, NULL);
        lives_touch(lsname);
        lives_free(lsname);
        com = lives_strdup_printf("%s stop_audio \"%s\"", prefs->backend, mainw->files[mainw->aud_file_to_kill]->handle);
        lives_system(com, TRUE);
        lives_free(com);
      }
    }
#endif
    // stop any background processing for the current clip
    if (CURRENT_CLIP_IS_VALID) {
      if (cfile->handle != NULL && CURRENT_CLIP_IS_NORMAL) {
        lives_kill_subprocesses(cfile->handle, TRUE);
      }
    }

    // prevent crash in "threaded" dialog
    mainw->current_file = -1;

    if (!mainw->only_close) {
      // shut down audio players
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed != NULL) pulse_close_client(mainw->pulsed);
      if (mainw->pulsed_read != NULL) pulse_close_client(mainw->pulsed_read);
      pulse_shutdown();
#endif
#ifdef ENABLE_JACK
      lives_jack_end();
      if (mainw->jackd != NULL) {
        jack_close_device(mainw->jackd);
      }
      if (mainw->jackd_read != NULL) {
        jack_close_device(mainw->jackd_read);
      }
#endif
    }

    if (mainw->vpp != NULL && !mainw->only_close) {
      if (mainw->memok) {
        if (mainw->write_vpp_file) {
          // save video playback plugin parameters
          char *vpp_file = lives_build_filename(prefs->configdir, LIVES_CONFIG_DIR, "vpp_defaults", NULL);
          save_vpp_defaults(mainw->vpp, vpp_file);
        }
      }
      close_vid_playback_plugin(mainw->vpp);
    }

    if (mainw->memok) {
      if (!mainw->leave_recovery) {
        lives_rm(mainw->recovery_file);
        // hide the main window
        threaded_dialog_spin(0.);
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
        threaded_dialog_spin(0.);
      }

      if (*(future_prefs->workdir) && lives_strcmp(future_prefs->workdir, prefs->workdir)) {
        // if we changed the workdir, remove everything but sets from the old dir
        // create the new directory, and then move any sets over
        end_threaded_dialog();
        if (do_move_workdir_dialog()) {
          do_do_not_close_d();
          lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

          // TODO *** - check for namespace collisions between sets in old dir and sets in new dir

          // use backend to move the sets
          com = lives_strdup_printf("%s move_workdir \"%s\"", prefs->backend_sync,
                                    future_prefs->workdir);
          lives_system(com, FALSE);
          lives_free(com);
        }
        lives_snprintf(prefs->workdir, PATH_MAX, "%s", future_prefs->workdir);
      }

      if (mainw->leave_files && !mainw->fatal) {
        d_print(_("Saving as set %s..."), mainw->set_name);
        mainw->suppress_dprint = TRUE;
      }

      cwd = lives_get_current_dir();

      if (mainw->memok) {
        for (i = 1; i <= MAX_FILES; i++) {
          if (mainw->files[i]) {
            mainw->current_file = i;
            threaded_dialog_spin(0.);
            if (cfile->event_list_back) event_list_free(cfile->event_list_back);
            if (cfile->event_list) event_list_free(cfile->event_list);

            cfile->event_list = cfile->event_list_back = NULL;

            lives_list_free_all(&cfile->layout_map);

            if (cfile->laudio_drawable) {
              if (mainw->laudio_drawable == cfile->laudio_drawable) mainw->laudio_drawable = NULL;
              lives_painter_surface_destroy(cfile->laudio_drawable);
            }

            if (cfile->raudio_drawable) {
              if (mainw->raudio_drawable == cfile->raudio_drawable) mainw->raudio_drawable = NULL;
              lives_painter_surface_destroy(cfile->raudio_drawable);
            }

            if (IS_NORMAL_CLIP(i) && mainw->files[i]->ext_src && mainw->files[i]->ext_src_type == LIVES_EXT_SRC_DECODER) {
              // must do this before we move it
              close_clip_decoder(i);
              threaded_dialog_spin(0.);
            }
            lives_freep((void **)&mainw->files[i]->frame_index);
            cfile->layout_map = NULL;
          }

          if (mainw->files[i]) {
            if ((!mainw->leave_files && !prefs->crash_recovery && !(*mainw->set_name)) ||
                (!mainw->only_close && (i == 0 || !IS_NORMAL_CLIP(i))) ||
                (i == mainw->scrap_file && (!mainw->leave_recovery || !prefs->rr_crash)) ||
                (i == mainw->ascrap_file && (!mainw->leave_recovery || !prefs->rr_crash)) ||
                (mainw->multitrack && i == mainw->multitrack->render_file)) {
              char *permitname;
              // close all open clips, except for ones we want to retain
#ifdef HAVE_YUV4MPEG
              if (mainw->files[i]->clip_type == CLIP_TYPE_YUV4MPEG) {
                lives_yuv_stream_stop_read((lives_yuv4m_t *)mainw->files[i]->ext_src);
                lives_free(mainw->files[i]->ext_src);
              }
#endif
#ifdef HAVE_UNICAP
              if (mainw->files[i]->clip_type == CLIP_TYPE_VIDEODEV) {
                lives_vdev_free((lives_vdev_t *)mainw->files[i]->ext_src);
                lives_free(mainw->files[i]->ext_src);
              }
#endif
              threaded_dialog_spin(0.);
              lives_kill_subprocesses(mainw->files[i]->handle, TRUE);
              permitname = lives_build_filename(prefs->workdir, mainw->files[i]->handle,
                                                TEMPFILE_MARKER "," LIVES_FILE_EXT_TMP, NULL);
              lives_touch(permitname);
              lives_free(permitname);
              com = lives_strdup_printf("%s close \"%s\"", prefs->backend, mainw->files[i]->handle);
              lives_system(com, FALSE);
              lives_free(com);
              threaded_dialog_spin(0.);
            } else {
              threaded_dialog_spin(0.);
              // or just clean them up -
              // remove the following: "*.mgk *.bak *.pre *.tmp pause audio.* audiodump* audioclip";
              if (!prefs->vj_mode) {
                if (prefs->autoclean) {
                  com = lives_strdup_printf("%s clear_tmp_files \"%s\"",
                                            prefs->backend_sync, mainw->files[i]->handle);
                  lives_system(com, FALSE);
                  threaded_dialog_spin(0.);
                  lives_free(com);
                }
                if (IS_NORMAL_CLIP(i)) {
                  char *fname = lives_build_filename(prefs->workdir, mainw->files[i]->handle,
                                                     TOTALSAVE_NAME, NULL);
                  int fd = lives_create_buffered(fname, DEF_FILE_PERMS);
                  lives_write_buffered(fd, (const char *)mainw->files[i], sizeof(lives_clip_t), TRUE);
                  lives_close_buffered(fd);
                }
              }
              if (mainw->files[i]->frameno != mainw->files[i]->saved_frameno) {
                save_clip_value(i, CLIP_DETAILS_PB_FRAMENO, &mainw->files[i]->frameno);
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*
        if (prefs->autoclean) {
          com = lives_strdup_printf("%s empty_trash . general", prefs->backend);
          lives_system(com, FALSE);
          lives_free(com);
        }

        lives_free(cwd);
      }

      if (!mainw->leave_files && (*mainw->set_name) && !mainw->leave_recovery) {
        char *set_layout_dir = lives_build_filename(prefs->workdir, mainw->set_name,
                               LAYOUTS_DIRNAME, NULL);
        if (!lives_file_test(set_layout_dir, LIVES_FILE_TEST_IS_DIR)) {
          char *sdname = lives_build_filename(prefs->workdir, mainw->set_name, NULL);

          // note, FORCE is FALSE
          lives_rmdir(sdname, FALSE);
          lives_free(sdname);
          threaded_dialog_spin(0.);
        } else {
          char *dname = lives_build_filename(prefs->workdir, mainw->set_name, CLIPS_DIRNAME, NULL);

          // note, FORCE is FALSE
          lives_rmdir(dname, FALSE);
          lives_free(dname);
          threaded_dialog_spin(0.);

          dname = lives_build_filename(prefs->workdir, mainw->set_name, CLIP_ORDER_FILENAME, NULL);
          lives_rm(dname);
          lives_free(dname);
          threaded_dialog_spin(0.);
        }
        lives_free(set_layout_dir);
      }

      if (*mainw->set_name) {
        threaded_dialog_spin(0.);
      }

      // closes in memory clips
      if (mainw->only_close) {
        mainw->suppress_dprint = TRUE;
        mainw->close_keep_frames = TRUE;
        mainw->is_processing = TRUE; ///< stop multitrack from sensitizing too soon
        for (i = 1; i <= MAX_FILES; i++) {
          if (IS_NORMAL_CLIP(i) && (mainw->multitrack == NULL || i != mainw->multitrack->render_file)) {
            mainw->current_file = i;
            close_current_file(0);
            threaded_dialog_spin(0.);
          }
        }
        mainw->close_keep_frames = FALSE;

        if (!mainw->leave_files) {
          // delete the current set (this is for DELETE_SET)
          cleanup_set_dir(mainw->set_name);
          lives_memset(mainw->set_name, 0, 1);
          mainw->was_set = FALSE;
          lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
        } else {
          unlock_set_file(mainw->set_name);
        }

        mainw->suppress_dprint = FALSE;
        if (mainw->multitrack == NULL) resize(1);
        mainw->was_set = FALSE;
        lives_memset(mainw->set_name, 0, 1);
        mainw->only_close = FALSE;
        prefs->crash_recovery = TRUE;

        threaded_dialog_spin(0.);
        if (mainw->current_file > -1) sensitize();
        lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
        d_print_done();
        end_threaded_dialog();

        if (mainw->multitrack != NULL) {
          mainw->current_file = mainw->multitrack->render_file;
          mainw->multitrack->file_selected = -1;
          polymorph(mainw->multitrack, POLY_NONE);
          polymorph(mainw->multitrack, POLY_CLIPS);
          mt_sensitise(mainw->multitrack);
        } else {
          if (prefs->show_msg_area) {
            reset_message_area();
            lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
            if (mainw->idlemax == 0) {
              lives_idle_add_simple(resize_message_area, NULL);
            }
            mainw->idlemax = DEF_IDLE_MAX;
          }
        }
        mainw->is_processing = FALSE; ///< mt may now sensitize...
        return;
      }

      save_future_prefs();
    }

    unlock_set_file(mainw->set_name);

    // stop valgrind from complaining
#ifdef VALGRIND_ON
    if (mainw->frame_layer != NULL) {
      check_layer_ready(mainw->frame_layer);
      weed_layer_free(mainw->frame_layer);
      mainw->frame_layer = NULL;
    }
#endif

    if (mainw->sep_win && (LIVES_IS_PLAYING || prefs->sepwin_type == SEPWIN_TYPE_STICKY)) {
      threaded_dialog_spin(0.);
      kill_play_window();
      threaded_dialog_spin(0.);
    }
  }

  weed_unload_all();

#ifdef VALGRIND_ON
  lives_list_free_all(&mainw->current_layouts_map);

  if (capable->has_encoder_plugins) {
    LiVESList *dummy_list = plugin_request("encoders", prefs->encoder.name, "finalise");
    lives_list_free_all(&dummy_list);
  }
  threaded_dialog_spin(0.);
  rfx_free_all();
  threaded_dialog_spin(0.);
#endif

#ifdef ENABLE_OSC
  if (prefs->osc_udp_started) lives_osc_end();
#endif
  mainw->is_ready = FALSE;

#ifdef VALGRIND_ON
  pconx_delete_all();
  cconx_delete_all();

  mainw->msg_adj = NULL;
  free_n_msgs(mainw->n_messages);

  if (mainw->multitrack != NULL) {
    event_list_free_undos(mainw->multitrack);

    if (mainw->multitrack->event_list != NULL) {
      event_list_free(mainw->multitrack->event_list);
      mainw->multitrack->event_list = NULL;
    }
  }

  lives_freep((void **)&prefs->fxdefsfile);
  lives_freep((void **)&prefs->fxsizesfile);
  lives_freep((void **)&capable->wm);
  lives_freep((void **)&mainw->recovery_file);

  for (i = 0; i < NUM_LIVES_STRING_CONSTANTS; i++) lives_freep((void **)&mainw->string_constants[i]);
  for (i = 0; i < mainw->n_screen_areas; i++) lives_freep((void **)&mainw->screen_areas[i].name);

  lives_freep((void **)&mainw->foreign_visual);
  lives_freep((void **)&THREADVAR(read_failed_file));
  lives_freep((void **)&THREADVAR(write_failed_file));
  lives_freep((void **)&THREADVAR(bad_aud_file));

  lives_freep((void **)&mainw->old_vhash);
  lives_freep((void **)&mainw->version_hash);
  lives_freep((void **)&mainw->multitrack);
  lives_freep((void **)&mainw->mgeom);
  lives_list_free_all(&prefs->disabled_decoders);
  if (mainw->fonts_array != NULL) lives_strfreev(mainw->fonts_array);
#ifdef ENABLE_NLS
  //lives_freep((void **)&trString);
#endif
#endif
  unload_decoder_plugins();

  tmp = lives_strdup_printf("signal: %d", signum);
  lives_notify(LIVES_OSC_NOTIFY_QUIT, tmp);
  lives_free(tmp);

  exit(0);
}

#ifndef VALGRIND_ON
#ifdef _lives_free
#undef  lives_free
#define lives_free _lives_free
#endif
#endif

void on_filesel_button_clicked(LiVESButton * button, livespointer user_data) {
  LiVESWidget *tentry = LIVES_WIDGET(user_data);

  lives_rfx_t *rfx;

  char **filt = NULL;

  char *dirname = NULL;
  char *fname;
  char *tmp;

  char *def_dir = NULL;

  boolean is_dir = TRUE, free_def_dir = FALSE;

  int filesel_type = LIVES_FILE_SELECTION_UNDEFINED;

  if (button) {
    def_dir = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), DEFDIR_KEY);
    is_dir = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), ISDIR_KEY));
    filt = (char **)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), FILTER_KEY);
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), FILESEL_TYPE_KEY)) {
      filesel_type = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), FILESEL_TYPE_KEY));
    }
  }

  if (LIVES_IS_TEXT_VIEW(tentry)) fname = lives_text_view_get_text(LIVES_TEXT_VIEW(tentry));
  else fname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(tentry)));

  if (!(*fname)) {
    lives_free(fname);
    fname = def_dir;
  }

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  switch (filesel_type) {
  case LIVES_FILE_SELECTION_UNDEFINED:
    if (!is_dir && *fname && (def_dir == NULL || !(*def_dir))) {
      def_dir = get_dir(fname);
      free_def_dir = TRUE;
    }
  case LIVES_DIR_SELECTION_WORKDIR:
    dirname = choose_file(is_dir ? fname : def_dir, is_dir ? NULL : fname, filt,
                          is_dir ? LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER :
                          (fname == def_dir && def_dir != NULL && !strcmp(def_dir, LIVES_DEVICE_DIR))
                          ? LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE :
                          LIVES_FILE_CHOOSER_ACTION_OPEN,
                          NULL, NULL);

    if (filesel_type == LIVES_DIR_SELECTION_WORKDIR) {
      if (strcmp(dirname, fname)) {
        if (check_workdir_valid(&dirname, LIVES_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))),
                                FALSE) == LIVES_RESPONSE_RETRY) {
          lives_free(dirname);
          dirname = lives_strdup(fname);
        }
      }
    }
    if (free_def_dir) {
      lives_free(def_dir);
      def_dir = NULL;
    }
    break;
  case LIVES_FILE_SELECTION_SAVE: {
    char fnamex[PATH_MAX], dirnamex[PATH_MAX];
    boolean free_filt = FALSE;

    lives_snprintf(dirnamex, PATH_MAX, "%s", fname);
    lives_snprintf(fnamex, PATH_MAX, "%s", fname);

    get_dirname(dirnamex);
    get_basename(fnamex);

    if (!is_dir && filt == NULL && strlen(fnamex)) {
      char *tmp;
      filt = (char **)lives_malloc(2 * sizeof(char *));
      filt[0] = lives_strdup_printf("*.%s", (tmp = get_extension(fnamex)));
      filt[1] = NULL;
      free_filt = TRUE;
      lives_free(tmp);
    }

    dirname = choose_file(def_dir != NULL ? def_dir : dirnamex, fnamex, filt, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);

    if (free_filt) {
      lives_free(filt[0]);
      lives_free(filt);
    }
  }
  break;
  default: {
    LiVESWidget *chooser = choose_file_with_preview(def_dir, fname, filt, filesel_type);
    int resp = lives_dialog_run(LIVES_DIALOG(chooser));

    end_fs_preview();

    if (resp == LIVES_RESPONSE_ACCEPT) {
      dirname = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));
    }
    lives_widget_destroy(LIVES_WIDGET(chooser));
  }
  }

  if (fname != NULL && fname != def_dir) lives_free(fname);

  if (dirname == NULL) return;

  lives_snprintf(file_name, PATH_MAX, "%s", dirname);
  lives_free(dirname);

  if (button != NULL) {
    if (LIVES_IS_ENTRY(tentry)) lives_entry_set_text(LIVES_ENTRY(tentry), (tmp = lives_filename_to_utf8(file_name, -1, NULL, NULL,
          NULL)));
    else lives_text_view_set_text(LIVES_TEXT_VIEW(tentry), (tmp = lives_filename_to_utf8(file_name, -1, NULL, NULL, NULL)), -1);
    lives_free(tmp);
  }

  // force update to be recognized
  if ((rfx = (lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tentry), "rfx")) != NULL) {
    int param_number = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tentry), "param_number"));
    after_param_text_changed(tentry, rfx);
    rfx->params[param_number].edited = FALSE;
  }
}


static void open_sel_range_activate(int frames, double fps) {
  // open selection range dialog
  LiVESWidget *opensel_dialog;
  mainw->fc_buttonresponse = LIVES_RESPONSE_NONE; // reset button state
  mainw->fx1_val = 0.;
  mainw->fx2_val = frames > 1000. ? 1000. : (double)frames;
  opensel_dialog = create_opensel_dialog(frames, fps);
  lives_widget_show_all(opensel_dialog);
}


static boolean read_file_details_generic(const char *fname) {
  /// make a tmpdir in case we need to open images for example
  char *tmpdir, *dirname, *com;
  const char *prefix = "fsp";
  tmpdir = get_worktmp(prefix);
  if (tmpdir) dirname = lives_path_get_basename(tmpdir);
  else {
    dirname = lives_strdup_printf("%s%lu", prefix, gen_unique_id());
    tmpdir = lives_build_path(prefs->workdir, dirname, NULL);
    if (!lives_make_writeable_dir(tmpdir)) {
      workdir_warning();
      lives_free(tmpdir); lives_free(dirname);
      end_fs_preview();
      return FALSE;
    }
  }

  // check details
  com = lives_strdup_printf("%s get_details %s \"%s\" \"%s\" %d", prefs->backend_sync,
                            dirname, fname, prefs->image_ext, 0);
  lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    end_fs_preview();
    return FALSE;
  }
  return TRUE;
}


void on_open_sel_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // OPEN A FILE
  LiVESWidget *chooser;
  char **array;
  char *fname, *tmp;
  double fps;
  int resp, npieces, frames;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  while (1) {
    chooser = choose_file_with_preview((*mainw->vid_load_dir) ? mainw->vid_load_dir : NULL, NULL, NULL,
                                       LIVES_FILE_SELECTION_VIDEO_AUDIO);
    resp = lives_dialog_run(LIVES_DIALOG(chooser));

    end_fs_preview();

    if (resp != LIVES_RESPONSE_ACCEPT) {
      on_filechooser_cancel_clicked(chooser);
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      return;
    }

    fname = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));

    if (fname == NULL) {
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      return;
    }

    lives_snprintf(file_name, PATH_MAX, "%s", (tmp = lives_filename_to_utf8(fname, -1, NULL, NULL, NULL)));
    lives_free(tmp);

    lives_widget_destroy(LIVES_WIDGET(chooser));

    lives_snprintf(mainw->vid_load_dir, PATH_MAX, "%s", file_name);
    get_dirname(mainw->vid_load_dir);

    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    if (prefs->save_directories) {
      set_utf8_pref(PREF_VID_LOAD_DIR, mainw->vid_load_dir);
    }

    if (!read_file_details_generic(fname)) {
      lives_free(fname);
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      return;
    }
    lives_free(fname);

    npieces = get_token_count(mainw->msg, '|');
    if (npieces < 8) {
      end_fs_preview();
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      return;
    }

    array = lives_strsplit(mainw->msg, "|", npieces);
    frames = atoi(array[2]);
    fps = lives_strtod(array[7], NULL);
    lives_strfreev(array);

    if (frames == 0) {
      do_error_dialog("LiVES could not extract any video frames from this file.\nSorry.\n");
      end_fs_preview();
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      continue;
    }
    break;
  }

  open_sel_range_activate(frames, fps);
}


void on_open_vcd_activate(LiVESMenuItem * menuitem, livespointer device_type) {
  LiVESWidget *vcdtrack_dialog;
  int type = LIVES_POINTER_TO_INT(device_type);
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  mainw->fx1_val = 1;
  mainw->fx2_val = 1;
  mainw->fx3_val = DVD_AUDIO_CHAN_DEFAULT;

  vcdtrack_dialog = create_cdtrack_dialog(type, NULL);
  lives_widget_show_all(vcdtrack_dialog);
}


void on_open_loc_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // need non-instant opening (for now)
  mainw->mt_needs_idlefunc = FALSE;

  if (!HAS_EXTERNAL_PLAYER) {
    do_need_mplayer_mpv_dialog();
    return;
  }

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  locw = create_location_dialog();
  lives_widget_show_all(locw->dialog);
}


void on_open_utube_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  /// get a system tmpdir
  char *tmpdir = get_systmp("ytdl", TRUE);
  lives_remote_clip_request_t *req = NULL, *req2;

  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  do {
    mainw->cancelled = CANCEL_NONE;
    req = run_youtube_dialog(req);
    if (!req) {
      if (mainw->multitrack) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      goto final123;
    }
    req2 = on_utube_select(req, tmpdir);
#ifndef ALLOW_NONFREE_CODECS
    req2 = NULL;
#endif
    if (req2 && mainw->cancelled == CANCEL_RETRY) req = req2;
    else {
      lives_free(req);
      req = NULL;
    }
  } while (mainw->cancelled == CANCEL_RETRY);

final123:
  if (mainw->permmgr) {
    lives_freep((void **)&mainw->permmgr->key);
    lives_free(mainw->permmgr);
    mainw->permmgr = NULL;
  }
  if (tmpdir) {
    if (lives_file_test(tmpdir, LIVES_FILE_TEST_EXISTS)) {
      lives_rmdir(tmpdir, TRUE);
    }
    lives_free(tmpdir);
  }

  if (!mainw->multitrack) sensitize();
}


void on_recent_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char file[PATH_MAX];
  double start = 0.;
  int end = 0, pno;
  char *pref;
  mainw->mt_needs_idlefunc = FALSE;

  pno = LIVES_POINTER_TO_INT(user_data);

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  //lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  pref = lives_strdup_printf("%s%d", PREF_RECENT, pno);

  get_utf8_pref(pref, file, PATH_MAX);

  lives_free(pref);

  if (get_token_count(file, '\n') > 1) {
    char **array = lives_strsplit(file, "\n", 2);
    lives_snprintf(file, PATH_MAX, "%s", array[0]);
    lives_freep((void **)&mainw->file_open_params);
    mainw->file_open_params = lives_strdup(array[1]);
    lives_strfreev(array);
  }

  if (get_token_count(file, '|') > 2) {
    char **array = lives_strsplit(file, "|", 3);
    lives_snprintf(file, PATH_MAX, "%s", array[0]);
    start = lives_strtod(array[1], NULL);
    end = atoi(array[2]);
    lives_strfreev(array);
  }
  deduce_file(file, start, end);

  if (mainw->multitrack != NULL) {
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
}


void on_location_select(LiVESButton * button, livespointer user_data) {
  lives_snprintf(file_name, PATH_MAX, "%s", lives_entry_get_text(LIVES_ENTRY(locw->entry)));
  lives_widget_destroy(locw->dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_free(locw);

  mainw->opening_loc = TRUE;
  lives_freep((void **)&mainw->file_open_params);
  if (prefs->no_bandwidth) {
    mainw->file_open_params = lives_strdup("nobandwidth");
  } else mainw->file_open_params = lives_strdup("sendbandwidth");
  mainw->img_concat_clip = -1;
  open_file(file_name);

  if (mainw->multitrack != NULL) {
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
}


//ret updated req if fmt sel. needs change
lives_remote_clip_request_t *on_utube_select(lives_remote_clip_request_t *req, const char *tmpdir) {
  char *com, *full_dfile = NULL, *tmp, *ddir, *dest = NULL;
  char *overrdkey = NULL;
  char *mpf = NULL, *mpt = NULL;
  lives_remote_clip_request_t *reqout = NULL;
  boolean hasnone = FALSE, hasalts = FALSE;
  boolean keep_old_dir = FALSE;
  boolean forcecheck = FALSE;
  boolean bres;
  boolean badfile = FALSE;
  int keep_frags = 0;
  int manage_ds = 0;
  int current_file = mainw->current_file;

  mainw->no_switch_dprint = TRUE;

  // if possible, download to a temp dir first, that way we can cleanly delete leftover fragments, etc.
  if (tmpdir) {
    ddir = lives_build_path(tmpdir, req->fname, NULL);
    keep_frags = 1;
    if (lives_file_test(ddir, LIVES_FILE_TEST_IS_DIR)) {
      if (check_dir_access(ddir, FALSE)) {
        keep_old_dir = TRUE;
      }
    }
    if (!keep_old_dir) {
      char *xdir = lives_build_path(tmpdir, "*", NULL);
      lives_rmdir(xdir, TRUE);
      lives_free(xdir);
      if (!lives_make_writeable_dir(ddir)) {
        do_dir_perm_error(ddir);
        goto cleanup_ut;
      }
    }
  } else ddir = req->save_dir;

  if (capable->mountpoint && *capable->mountpoint) {
    /// if tempdir is on same volume as workdir, or if not using tmpdir and final is on same
    // then we monitor continuously
    mpf = get_mountpoint_for(ddir);
    if (mpf && *mpf && !lives_strcmp(mpf, capable->mountpoint)) {
      manage_ds = 1;
    }
  }
  if (!manage_ds && ddir != req->save_dir) {
    mpt = get_mountpoint_for(req->save_dir);
    if (mpf && *mpf && mpt && *mpt) {
      if (lives_strcmp(mpf, mpt)) {
        /// in this case tmpdir is on another volume, but final dest. is on our volume
        /// or a different one
        /// we will only check before moving the file
        if (capable->mountpoint && *capable->mountpoint && !lives_strcmp(mpt, capable->mountpoint)) {
          // final dest is ours, we will check warn, crit and overflow
          manage_ds = 2;
        } else {
          // final dest is not ours, we will only check overflow
          manage_ds = 3;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  lives_freep((void **)&mpf);

  if (prefs->ds_warn_level && !prefs->ds_crit_level) {
    /// if the user disabled disk_quota, disk_warn, AND disk_crit, then we just let the disk fill up
    /// the only thing we will care about is if there is insufficient space to move the file
    if (manage_ds == 2) manage_ds = 3;
    else manage_ds = 0;
  }

  mainw->error = FALSE;

  // do minimal ds checking until we hit full download
  forcecheck = FALSE;

  while (1) {
    if (!get_temp_handle(-1)) {
      // we failed because we ran out of file handles; this hsould almost never happen
      // we wil do only minimal ds checks
      d_print_failed();
      goto cleanup_ut;
    }

retry:

    if (mainw->permmgr && mainw->permmgr->key) {
      overrdkey = mainw->permmgr->key;
    }

    // get format list

    // for now, we dont't pass req->desired_fps or req->audchoice
    // also we could send req->sub_lang...

    com = lives_strdup_printf("%s download_clip \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" %d %d %d \"%s\" %d "
                              "%d %d%s", prefs->backend, cfile->handle, req->URI, ddir,
                              req->fname, req->format, req->desired_width, req->desired_height,
                              req->matchsize, req->vidchoice, keep_frags,
                              req->do_update, prefs->show_dev_opts,
                              overrdkey ? (tmp = lives_strdup_printf(" %s", overrdkey))
                              : (tmp = lives_strdup("")));
    lives_free(tmp);

    if (mainw->permmgr) {
      lives_freep((void **)&mainw->permmgr->key);
      lives_free(mainw->permmgr);
      overrdkey = NULL;
      mainw->permmgr = NULL;
    }

    mainw->error = FALSE;
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      d_print_failed();
      goto cleanup_ut;
    }

    // we expect to get back a list of available formats
    // or the selected format
    if (*(req->vidchoice)) break;

    if (!do_auto_dialog(_("Getting format list"), 2)) {
      if (mainw->cancelled) d_print_cancelled();
      else if (mainw->error) {
        d_print_failed();
        /* /// need to check here if backend returned a specific reason */
        /* LiVESResponseType resp = handle_backend_errors(FALSE, NULL); */
        if (mainw->permmgr)  {
          if (mainw->permmgr->key && *mainw->permmgr->key) {
            req->do_update = TRUE;
            mainw->error = FALSE;
            mainw->cancelled = CANCEL_NONE;
            goto retry;
          } else {
            lives_free(mainw->permmgr);
            mainw->permmgr = NULL;
          }
        } else {
          do_error_dialogf(_("Unable to download media from the requested URL:\n%s\n\n"
                             "NB: Obtaining the address by right clicking on the target itself "
                             "can sometimes work better\n\n\nAlso, please note that downloading of 'Private' videos "
                             "from Youtube is not possible,\nthey need to be changed to'Unlisted' "
                             "in order for the download to succeed.\n"), req->URI);
        }
      }
      mainw->error = FALSE;
      mainw->cancelled = CANCEL_RETRY;
      goto cleanup_ut;
    }

    if (!(*mainw->msg)) {
      hasnone = TRUE;
    }
#ifdef ALLOW_NONFREE_CODECS
    else if (!lives_strncmp(mainw->msg, "completed|altfmts", 17)) {
      hasalts = TRUE;
    }
#endif

    if (hasnone || hasalts) {
      d_print_failed();
      if (hasnone) {
        do_error_dialog(
          _("\nLiVES was unable to download the clip.\nPlease check the clip URL and make sure you have \n"
            "the latest youtube-dl installed.\n"));
      } else {
#ifdef ALLOW_NONFREE_CODECS
        if (do_yesno_dialog(
              _("\nLiVES was unable to download the clip in the desired format\nWould you like to try using an alternate "
                "format selection ?"))) {
          mainw->error = FALSE;
          mainw->cancelled = CANCEL_RETRY;
          req->allownf = TRUE;
          reqout = req;
#endif
        }
      }
      goto cleanup_ut;
    }

    if (req->matchsize == LIVES_MATCH_CHOICE && strlen(req->vidchoice) == 0)  {
      // show a list of the video formats and let the user pick one
      if (!youtube_select_format(req)) {
        goto cleanup_ut;
      }
      // we try again, this time with req->vidchoice set
      req->matchsize = LIVES_MATCH_SPECIFIED;
    } else {
      // returned completed|vidchoice|audchoice
      char **array = lives_strsplit(mainw->msg, "|", 3);
      lives_snprintf(req->vidchoice, 512, "%s", array[1]);
      lives_snprintf(req->audchoice, 512, "%s", array[2]);
      lives_strfreev(array);
    }
  }

  // backend should now be downloading the clip

  // do more intensive diskspace checking
  forcecheck = TRUE;

  if (manage_ds == 1) {
    /// type 1, we do continuous monitoring
    // we will only monitor for CRIT, other statuses will be checked at the end
    mainw->ds_mon = CHECK_CRIT;
  }

  cfile->nopreview = TRUE;
  cfile->no_proc_sys_errors = TRUE; ///< do not show processing error dialogs, we will show our own msg

  //// TODO - allow downloading in bg while user does something else
  /// **ADD TASK CALLBACK

  lives_snprintf(req->ext, 16, "%s", req->format);

  full_dfile = lives_strdup_printf("%s.%s", req->fname, req->ext);

  bres = do_progress_dialog(TRUE, TRUE, _("Downloading clip"));
  cfile->no_proc_sys_errors = FALSE;
  if (!bres || mainw->error) badfile = TRUE;
  else {
    dest = lives_build_filename(req->save_dir, full_dfile, NULL);
    if (ddir != req->save_dir) {
      lives_storage_status_t dstate;
      char *from = lives_build_filename(ddir, full_dfile, NULL);
      off_t clipsize;
      if (!lives_file_test(from, LIVES_FILE_TEST_EXISTS) || (clipsize = sget_file_size(from)) <= 0) {
        badfile = TRUE;
      } else {
        if (manage_ds == 2 || manage_ds == 3) {
          LiVESResponseType resp;
          do {
            char *msg = NULL;
            int64_t dsu = -1;
            resp = LIVES_RESPONSE_OK;
            dstate = get_storage_status(req->save_dir, mainw->next_ds_warn_level, &dsu, clipsize);
            if (dstate == LIVES_STORAGE_STATUS_OVERFLOW) {
              msg =
                lives_strdup_printf(_("There is insufficient disk space in %s to move the downloaded clip.\n"), mpt);
            } else if (manage_ds != 3) {
              if (dstate == LIVES_STORAGE_STATUS_CRITICAL || dstate == LIVES_STORAGE_STATUS_WARNING) {
                msg = lives_strdup_printf(_("Moving the downloaded clip to\n%s\nwill bring free disk space in %s\n"
                                            "below the %s level of %s\n"), mpt);
              }
            }

            if (msg) {
              char *cs = lives_format_storage_space_string(clipsize);
              char *vs = lives_format_storage_space_string(dsu);
              char *xmsg = lives_strdup_printf("%s\n(Free space in volume = %s, clip size = %s)\n"
                                               "You can either try deleting some files from %s and clicking on  Retry\n"
                                               "or else click Cancel to cancel the download.\n", msg, vs, cs, mpt);
              resp = do_retry_cancel_dialog(xmsg);
              lives_free(xmsg); lives_free(msg);
              lives_free(cs); lives_free(vs);
            }
          } while (resp == LIVES_RESPONSE_RETRY);
          if (resp == LIVES_RESPONSE_CANCEL) {
            badfile = TRUE;
          }
        }
      }
      if (!badfile) {
        /// move file to its final destination
        LiVESResponseType resp;
        req->do_update = FALSE;
        if (tmpdir) {
          if (lives_mv(from, dest)) {
            lives_free(from);
            lives_free(dest);
            badfile = TRUE;
            goto cleanup_ut;
          }
        }
        do {
          resp = LIVES_RESPONSE_NONE;
          if (!lives_file_test(dest, LIVES_FILE_TEST_EXISTS)) {
            char *errtxt = lives_strdup_printf(_("Failed to move %s to $\n"), from, dest);
            resp = do_write_failed_error_s_with_retry(dest, errtxt);
            lives_free(errtxt);
          }
        } while (resp == LIVES_RESPONSE_RETRY);
        if (resp == LIVES_RESPONSE_CANCEL) {
          badfile = TRUE;
        }
      }
      lives_free(from);
    }
  }

  if (badfile) {
    lives_kill_subprocesses(cfile->handle, TRUE);
    if (mainw->error) {
      d_print_failed();
      do_error_dialog(
        _("\nLiVES was unable to download the clip.\nPlease check the clip URL and make sure you have \n"
          "the latest youtube-dl installed.\n(Note: try right-clicking on the clip itself to copy its address)\n"));
      mainw->error = FALSE;
    }
  }

cleanup_ut:
  close_temp_handle(current_file);
  lives_freep((void **)&mpt);

  if (manage_ds) {
    lives_storage_status_t dstat;
    boolean recheck = FALSE;
    boolean tempdir_removed = FALSE;
    int64_t dsu = -1;
    if (prefs->disk_quota) {
      size_t wdlen = lives_strlen(prefs->workdir);
      size_t tlen = lives_strlen(req->save_dir);
      if (tlen >= wdlen && lives_strncmp(req->save_dir, prefs->workdir, wdlen)) {
        dsu = capable->ds_used = get_dir_size(prefs->workdir);
      }
    }
    dstat = mainw->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsu, 0);
    capable->ds_free = dsu;
    THREADVAR(com_failed) = FALSE;
    if (dstat != LIVES_STORAGE_STATUS_NORMAL) {
      // Houston, we hava problem !
      if (!forcecheck) recheck = TRUE;
      if (tmpdir) {
        /// remove tmpdir and re-check
        lives_freep((void **)&mpt);
        mpt = get_mountpoint_for(tmpdir);
        if (!lives_strcmp(mpt, capable->mountpoint)) {
          tempdir_removed = TRUE;
          lives_rmdir(tmpdir, TRUE);
          if (!THREADVAR(com_failed)) {
            recheck = TRUE;
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    if (recheck) {
      if (prefs->disk_quota) {
        mainw->dsu_valid = TRUE;
        dsu = capable->ds_used = get_dir_size(prefs->workdir);
      }
      dstat = mainw->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsu, 0);
      capable->ds_free = dsu;
    }
    if (dstat != LIVES_STORAGE_STATUS_NORMAL) {
      if (!tempdir_removed) {
        if (tmpdir) {
          /// remove tmpdir and re-check
          lives_freep((void **)&mpt);
          mpt = get_mountpoint_for(tmpdir);
          if (!lives_strcmp(mpt, capable->mountpoint)) {
            lives_rmdir(tmpdir, TRUE);
            if (!THREADVAR(com_failed)) {
              if (prefs->disk_quota) {
                dsu = capable->ds_used = get_dir_size(prefs->workdir);
              }
              dstat = mainw->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsu, 0);
              capable->ds_free = dsu;
	      // *INDENT-OFF*
            }}}}}
    // *INDENT-ON*
    if (dstat != LIVES_STORAGE_STATUS_NORMAL) {
      /// iff critical, delete file
      // we should probably offer if warn or quota too
      if (mainw->ds_status == LIVES_STORAGE_STATUS_CRITICAL && dest) {
        lives_rm(dest);
        badfile = TRUE;
      }
      if (!check_storage_space(-1, FALSE)) {
        badfile = TRUE;
      }
    }
  }

  mainw->img_concat_clip = -1;
  mainw->no_switch_dprint = FALSE;

  if (dest) {
    if (!badfile) {
      open_file(dest);
      if (mainw->multitrack) {
        polymorph(mainw->multitrack, POLY_NONE);
        polymorph(mainw->multitrack, POLY_CLIPS);
        mt_sensitise(mainw->multitrack);
      }
    }
    lives_free(dest);
  }
  if (mainw->multitrack) {
    maybe_add_mt_idlefunc();
  }
  return reqout;
}


void on_stop_clicked(LiVESMenuItem * menuitem, livespointer user_data) {
  // 'enough' button for open, open location, and record audio

#ifdef ENABLE_JACK
  if (mainw->jackd != NULL && mainw->jackd_read != NULL && mainw->jackd_read->in_use) {
    mainw->cancelled = CANCEL_KEEP;
    return;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed != NULL && mainw->pulsed_read != NULL && mainw->pulsed_read->in_use) {
    mainw->cancelled = CANCEL_KEEP;
    return;
  }
#endif

  if (CURRENT_CLIP_IS_VALID) {
    lives_kill_subprocesses(cfile->handle, FALSE);
    if (mainw->proc_ptr != NULL) {
      if (mainw->proc_ptr->stop_button != NULL)
        lives_widget_set_sensitive(mainw->proc_ptr->stop_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->pause_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->preview_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, FALSE);
    }
  }

  // resume to allow return
  if (mainw->effects_paused && CURRENT_CLIP_IS_VALID) {
    lives_suspend_resume_process(cfile->handle, FALSE);
  }
}


void on_save_as_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (cfile->frames == 0) {
    on_export_audio_activate(NULL, NULL);
    return;
  }
  save_file(mainw->current_file, cfile->start, cfile->end, NULL);
}


#ifdef LIBAV_TRANSCODE
void on_transcode_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (mainw->current_file < 1 || mainw->files[mainw->current_file] == NULL) return;
  transcode(cfile->start, cfile->end);
}
#endif


void on_save_selection_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  save_file(mainw->current_file, cfile->start, cfile->end, NULL);
}


static void check_remove_layout_files(void) {
  if (prompt_remove_layout_files()) {
    // delete layout directory
    char *laydir = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, NULL);
    lives_rmdir(laydir, TRUE);
    lives_free(laydir);
    d_print(_("Layouts were removed for set %s.\n"), mainw->set_name);
  }
}


void on_close_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *warn, *extra;
  boolean lmap_errors = FALSE, acurrent = FALSE, only_current = FALSE;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    mainw->current_file = mainw->multitrack->file_selected;
  } else desensitize();

  if (!(prefs->warning_mask & WARN_MASK_LAYOUT_CLOSE_FILE)) {
    mainw->xlays = layout_frame_is_affected(mainw->current_file, 1, 0, mainw->xlays);
    mainw->xlays = layout_audio_is_affected(mainw->current_file, 0., 0., mainw->xlays);

    acurrent = used_in_current_layout(mainw->multitrack, mainw->current_file);
    if (acurrent) {
      if (mainw->xlays == NULL) only_current = TRUE;
      mainw->xlays = lives_list_append_unique(mainw->xlays, mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
    }

    if (mainw->xlays != NULL) {
      char *title = get_menu_name(cfile, FALSE);
      if (strlen(title) > 128) {
        lives_free(title);
        title = (_("This file"));
      }
      if (acurrent) extra = (_(",\n - including the current layout - "));
      else extra = lives_strdup("");
      if (!only_current) warn = lives_strdup_printf(_("\n%s\nis used in some multitrack layouts%s.\n\nReally close it ?"),
                                  title, extra);
      else warn = lives_strdup_printf(_("\n%s\nis used in the current layout.\n\nReally close it ?"), title);
      lives_free(title);
      lives_free(extra);
      if (!do_warning_dialog(warn)) {
        lives_free(warn);
        lives_list_free_all(&mainw->xlays);
        goto close_done;
      }
      lives_free(warn);
      add_lmap_error(LMAP_ERROR_CLOSE_FILE, cfile->name, cfile->layout_map, 0, 1, 0., acurrent);
      lmap_errors = TRUE;
      lives_list_free_all(&mainw->xlays);
    }
  }
  if (!lmap_errors) {
    if (cfile->changed) {
      if (!do_close_changed_warn()) {
        goto close_done;
      }
    }
  }

  if (mainw->sl_undo_mem != NULL && (cfile->stored_layout_frame != 0 || cfile->stored_layout_audio != 0.)) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (mainw->multitrack != NULL) {
    event_list_free_undos(mainw->multitrack);
  }

  close_current_file(0);

  if (mainw->multitrack != NULL) {
    mainw->current_file = mainw->multitrack->render_file;
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    if (mainw->multitrack->event_list != NULL) only_current = FALSE;
  }

  if (lmap_errors && !only_current && mainw->cliplist != NULL) popup_lmap_errors(NULL, NULL);

  if (mainw->cliplist == NULL && (*mainw->set_name)) {
    boolean has_layout_map = FALSE;

    // check for layout maps
    if (mainw->current_layouts_map != NULL) {
      has_layout_map = TRUE;
    }

    if (has_layout_map) {
      check_remove_layout_files();
      recover_layout_cancelled(FALSE);
    }
    // the user closed the last clip in the set, we should remove the set
    d_print(_("Removing set %s since it is now empty..."));
    cleanup_set_dir(mainw->set_name);
    lives_memset(mainw->set_name, 0, 1);
    mainw->was_set = FALSE;
    lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
    d_print_done();
  }

close_done:
  if (mainw->multitrack != NULL) {
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  } else sensitize();
}


void on_import_proj_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *com;
  char *filt[] = {"*."LIVES_FILE_EXT_PROJECT, NULL};
  char *proj_file = choose_file(NULL, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
  char *new_set;
  char *set_dir;
  char *msg;

  int current_file = mainw->current_file;

  if (proj_file == NULL) return;
  com = lives_strdup_printf("%s get_proj_set \"%s\"", prefs->backend_sync, proj_file);
  lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    lives_free(proj_file);
    return;
  }

  if (!(*mainw->msg)) {
    lives_free(proj_file);
    widget_opts.non_modal = TRUE;
    do_error_dialog(_("\nInvalid project file.\n"));
    widget_opts.non_modal = FALSE;
    return;
  }

  if (!is_legal_set_name(mainw->msg, TRUE)) return;

  new_set = lives_strdup(mainw->msg);
  set_dir = lives_build_filename(prefs->workdir, new_set, NULL);

  if (lives_file_test(set_dir, LIVES_FILE_TEST_IS_DIR)) {
    msg = lives_strdup_printf(
            _("\nA set called %s already exists.\nIn order to import this project, you must rename or delete the existing set.\n"
              "You can do this by File|Reload Set, and giving the set name\n%s\n"
              "then File|Close/Save all Clips and provide a new set name or discard it.\n"
              "Once you have done this, you will be able to import the new project.\n"),
            new_set, new_set);
    do_error_dialog(msg);
    lives_free(msg);
    lives_free(proj_file);
    lives_free(set_dir);
    lives_free(new_set);
    return;
  }

  lives_free(set_dir);

  d_print(_("Importing the project %s as set %s..."), proj_file, new_set);

  if (!get_temp_handle(-1)) {
    lives_free(proj_file);
    lives_free(new_set);
    d_print_failed();
    return;
  }

  com = lives_strdup_printf("%s import_project \"%s\" \"%s\"", prefs->backend, cfile->handle, proj_file);
  lives_system(com, FALSE);
  lives_free(com);
  lives_free(proj_file);

  if (THREADVAR(com_failed)) {
    mainw->current_file = close_temp_handle(current_file);
    lives_free(new_set);
    d_print_failed();
    return;
  }

  do_progress_dialog(TRUE, FALSE, _("Importing project"));

  mainw->current_file = close_temp_handle(current_file);
  sensitize();

  if (mainw->error) {
    lives_free(new_set);
    d_print_failed();
    return;
  }

  d_print_done();

  reload_set(new_set);
  lives_free(new_set);
}


void on_export_proj_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *filt[] = {"*."LIVES_FILE_EXT_PROJECT, NULL};
  char *def_file;
  char *proj_file;
  char *com, *tmp;

  if (!(*mainw->set_name)) {
    int response;
    char new_set_name[MAX_SET_NAME_LEN];
    do {
      // prompt for a set name, advise user to save set
      renamew = create_rename_dialog(5);
      lives_widget_show_all(renamew->dialog);
      response = lives_dialog_run(LIVES_DIALOG(renamew->dialog));
      if (response == LIVES_RESPONSE_CANCEL) {
        mainw->cancelled = CANCEL_USER;
        return;
      }
      lives_snprintf(new_set_name, MAX_SET_NAME_LEN, "%s", (tmp = U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
      lives_widget_destroy(renamew->dialog);
      lives_freep((void **)&renamew);
      lives_free(tmp);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

    } while (!is_legal_set_name(new_set_name, FALSE));
    lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", new_set_name);
  }

  if (mainw->stored_event_list != NULL && mainw->stored_event_list_changed) {
    if (!check_for_layout_del(NULL, FALSE)) return;
  }

  if (mainw->sl_undo_mem != NULL) stored_event_list_free_undos();

  if (!mainw->was_set) {
    mainw->no_exit = TRUE;
    on_save_set_activate(NULL, mainw->set_name);
    mainw->no_exit = FALSE;
    mainw->was_set = TRUE;
    if (mainw->multitrack != NULL && !mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  def_file = lives_strdup_printf("%s.%s", mainw->set_name, LIVES_FILE_EXT_PROJECT);
  proj_file = choose_file(NULL, def_file, filt, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
  lives_free(def_file);

  if (proj_file == NULL) return;

  lives_rm((tmp = lives_filename_from_utf8(proj_file, -1, NULL, NULL, NULL)));
  lives_free(tmp);

  d_print(_("Exporting project %s..."), proj_file);

  com = lives_strdup_printf("%s export_project \"%s\" \"%s\" \"%s\"", prefs->backend, cfile->handle, mainw->set_name, proj_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    lives_free(proj_file);
    d_print_failed();
    return;
  }

  do_progress_dialog(TRUE, FALSE, _("Exporting project"));

  if (mainw->error) d_print_failed();
  else d_print_done();

  lives_free(proj_file);
}


void on_export_theme_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_colRGBA64_t lcol;

  char *filt[] = {"*."LIVES_FILE_EXT_TAR_GZ, NULL};

  char theme_name[128];

  char *file_name, *tmp, *tmp2, *com, *fname;
  char *sepimg_ext, *frameimg_ext, *sepimg, *frameimg;
  char *themedir, *thfile, *themefile;
  char *pstyle;

  int response;

  desensitize();

  do {
    // prompt for a set name, advise user to save set
    renamew = create_rename_dialog(8);
    lives_widget_show_all(renamew->dialog);
    response = lives_dialog_run(LIVES_DIALOG(renamew->dialog));
    if (response == LIVES_RESPONSE_CANCEL) return;
    lives_snprintf(theme_name, 128, "%s", (tmp = U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
    lives_widget_destroy(renamew->dialog);
    lives_freep((void **)&renamew);
    lives_free(tmp);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  } while (!do_std_checks(U82F(theme_name), _("Theme"), 64, NULL));

  fname = lives_strdup_printf("%s.%s", theme_name, LIVES_FILE_EXT_TAR_GZ);

  file_name = choose_file(capable->home_dir, fname, filt,
                          LIVES_FILE_CHOOSER_ACTION_SAVE, _("Choose a directory to export to"), NULL);

  lives_free(fname);

  if (file_name == NULL) {
    return;
  }

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  // create a header.theme file in tmp, then zip it up with the images

  sepimg_ext = get_extension(mainw->sepimg_path);
  frameimg_ext = get_extension(mainw->frameblank_path);

  thfile = lives_strdup_printf("%s%d", THEME_LITERAL, capable->mainpid);
  themedir = lives_build_filename(prefs->workdir, thfile, NULL);
  themefile = lives_build_filename(themedir, THEME_HEADER, NULL);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_free(themefile);
  themefile = lives_build_filename(themedir, THEME_HEADER_2, NULL);
#endif
#endif
  lives_free(thfile);

  thfile = lives_strdup_printf("%s.%s", THEME_SEP_IMG_LITERAL, sepimg_ext);
  sepimg = lives_build_filename(themedir, thfile, NULL);
  lives_free(thfile);

  thfile = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, frameimg_ext);
  frameimg = lives_build_filename(themedir, thfile, NULL);

  lives_free(sepimg_ext);
  lives_free(frameimg_ext);

  lives_mkdir_with_parents(themedir, capable->umask);

  set_theme_pref(themefile, THEME_DETAIL_NAME, theme_name);

  pstyle = lives_strdup_printf("%d", palette->style);
  set_theme_pref(themefile, THEME_DETAIL_STYLE, pstyle);
  lives_free(pstyle);

  widget_color_to_lives_rgba(&lcol, &palette->normal_fore);
  set_theme_colour_pref(themefile, THEME_DETAIL_NORMAL_FORE, &lcol);

  widget_color_to_lives_rgba(&lcol, &palette->normal_back);
  set_theme_colour_pref(themefile, THEME_DETAIL_NORMAL_BACK, &lcol);

  widget_color_to_lives_rgba(&lcol, &palette->menu_and_bars_fore);
  set_theme_colour_pref(themefile, THEME_DETAIL_ALT_FORE, &lcol);

  widget_color_to_lives_rgba(&lcol, &palette->menu_and_bars);
  set_theme_colour_pref(themefile, THEME_DETAIL_ALT_BACK, &lcol);

  widget_color_to_lives_rgba(&lcol, &palette->info_text);
  set_theme_colour_pref(themefile, THEME_DETAIL_INFO_TEXT, &lcol);

  widget_color_to_lives_rgba(&lcol, &palette->info_base);
  set_theme_colour_pref(themefile, THEME_DETAIL_INFO_BASE, &lcol);

  if (mainw->fx1_bool) {
    widget_color_to_lives_rgba(&lcol, &palette->mt_timecode_fg);
    set_theme_colour_pref(themefile, THEME_DETAIL_MT_TCFG, &lcol);

    widget_color_to_lives_rgba(&lcol, &palette->mt_timecode_bg);
    set_theme_colour_pref(themefile, THEME_DETAIL_MT_TCBG, &lcol);

    set_theme_colour_pref(themefile, THEME_DETAIL_AUDCOL, &palette->audcol);
    set_theme_colour_pref(themefile, THEME_DETAIL_VIDCOL, &palette->vidcol);
    set_theme_colour_pref(themefile, THEME_DETAIL_FXCOL, &palette->fxcol);

    set_theme_colour_pref(themefile, THEME_DETAIL_MT_TLREG, &palette->mt_timeline_reg);
    set_theme_colour_pref(themefile, THEME_DETAIL_MT_MARK, &palette->mt_mark);
    set_theme_colour_pref(themefile, THEME_DETAIL_MT_EVBOX, &palette->mt_evbox);

    set_theme_colour_pref(themefile, THEME_DETAIL_FRAME_SURROUND, &palette->frame_surround);

    set_theme_colour_pref(themefile, THEME_DETAIL_CE_SEL, &palette->ce_sel);
    set_theme_colour_pref(themefile, THEME_DETAIL_CE_UNSEL, &palette->ce_unsel);
  }

  lives_free(themefile);

  d_print(_("Exporting theme as %s..."), file_name);

  // copy images for packaging
  lives_cp(mainw->sepimg_path, sepimg);
  lives_free(sepimg);

  if (THREADVAR(com_failed)) {
    lives_rmdir(themedir, TRUE);
    lives_free(frameimg);
    lives_free(file_name);
    lives_free(themedir);
    d_print_failed();
    sensitize();
    return;
  }

  lives_cp(mainw->frameblank_path, frameimg);
  lives_free(frameimg);

  if (THREADVAR(com_failed)) {
    lives_rmdir(themedir, TRUE);
    lives_free(file_name);
    lives_free(themedir);
    d_print_failed();
    sensitize();
    return;
  }

  com = lives_strdup_printf("%s create_package \"%s\" \"%s\"", prefs->backend_sync,
                            (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)),
                            (tmp2 = lives_filename_from_utf8(themedir, -1, NULL, NULL, NULL)));

  lives_free(tmp);
  lives_free(tmp2);
  lives_free(file_name);

  lives_system(com, TRUE);
  lives_free(com);

  lives_rmdir(themedir, TRUE);
  lives_free(themedir);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    sensitize();
    return;
  }

  d_print_done();
  sensitize();
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
}


void on_import_theme_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *filt[] = {"*."LIVES_FILE_EXT_TAR_GZ, NULL};
  char tname[128];

  char *importcheckdir, *themeheader, *themedir;
  char *com;
  char *theme_file;

  desensitize();

  theme_file = choose_file(NULL, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);

  if (theme_file == NULL) {
    sensitize();
    return;
  }
  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);


  importcheckdir = lives_build_filename(prefs->workdir, IMPORTS_DIRNAME, NULL);
  lives_rmdir(importcheckdir, TRUE);

  // unpackage file to get the theme name
  com = lives_strdup_printf("%s import_package \"%s\" \"%s\"", prefs->backend_sync, U82F(theme_file), importcheckdir);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    lives_rmdir(importcheckdir, TRUE);
    lives_free(importcheckdir);
    lives_free(theme_file);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    sensitize();
    return;
  }

  themeheader = lives_build_filename(prefs->workdir, IMPORTS_DIRNAME, THEME_HEADER, NULL);

  if (get_pref_from_file(themeheader, THEME_DETAIL_NAME, tname, 128) == LIVES_RESPONSE_NO) {
    // failed to get name
    lives_rmdir(importcheckdir, TRUE);
    lives_free(importcheckdir);
    lives_free(themeheader);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    do_bad_theme_import_error(theme_file);
    lives_free(theme_file);
    sensitize();
    return;
  }

  lives_rmdir(importcheckdir, TRUE);
  lives_free(importcheckdir);
  lives_free(themeheader);

  d_print(_("Importing theme \"%s\" from %s..."), tname, theme_file);

  if (!do_std_checks(U82F(tname), _("Theme"), 64, NULL)) {
    lives_free(theme_file);
    d_print_failed();
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    return;
  }

  // check for existing dupes

  themedir = lives_build_filename(prefs->configdir, LIVES_CONFIG_DIR, PLUGIN_THEMES, tname, NULL);

  if (lives_file_test(themedir, LIVES_FILE_TEST_IS_DIR)) {
    if (!do_theme_exists_warn(tname)) {
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      lives_free(themedir);
      lives_free(theme_file);
      d_print_failed();
      sensitize();
      return;
    }
    lives_rmdir(themedir, TRUE);
  }

  // name was OK, unpack into custom dir
  com = lives_strdup_printf("%s import_package \"%s\" \"%s\"", prefs->backend_sync, U82F(theme_file), themedir);
  lives_system(com, FALSE);
  lives_free(com);

  lives_free(theme_file);

  if (THREADVAR(com_failed)) {
    lives_rmdir(themedir, TRUE);
    lives_free(themedir);
    d_print_failed();
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    sensitize();
    return;
  }

  lives_free(themedir);

  lives_snprintf(prefs->theme, 64, "%s", tname);

  // try to set theme colours
  if (!set_palette_colours(TRUE)) {
    lives_snprintf(prefs->theme, 64, "%s", future_prefs->theme);
    d_print_failed();
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    sensitize();
    return;
  }

  lives_snprintf(future_prefs->theme, 64, "%s", prefs->theme);
  set_string_pref(PREF_GUI_THEME, prefs->theme);

  load_theme_images();
  pref_change_images();
  pref_change_colours();
  pref_change_xcolours();

  d_print_done();
  sensitize();
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
}


void on_backup_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *filt[] = {"*."LIVES_FILE_EXT_BACKUP, NULL};
  char *file_name;
  char *defname, *text;

  defname = lives_strdup_printf("%s.%s", cfile->name, LIVES_FILE_EXT_BACKUP);

  text = lives_strdup_printf(_("Backup as %s File"), LIVES_FILE_EXT_BACKUP);

  file_name = choose_file((*mainw->proj_save_dir) ? mainw->proj_save_dir : NULL, defname, filt,
                          LIVES_FILE_CHOOSER_ACTION_SAVE, text, NULL);

  lives_free(text);
  lives_free(defname);

  if (file_name == NULL) return;

  backup_file(mainw->current_file, 1, cfile->frames, file_name);

  lives_snprintf(mainw->proj_save_dir, PATH_MAX, "%s", file_name);
  get_dirname(mainw->proj_save_dir);
  lives_free(file_name);
}


void on_restore_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *filt[] = {"*."LIVES_FILE_EXT_BACKUP, NULL};
  char *file_name, *text;

  text = lives_strdup_printf(_("Restore %s File"), LIVES_FILE_EXT_BACKUP);

  file_name = choose_file((*mainw->proj_load_dir) ? mainw->proj_load_dir : NULL, text, filt,
                          LIVES_FILE_CHOOSER_ACTION_OPEN, text, NULL);

  lives_free(text);

  if (file_name == NULL) return;

  restore_file(file_name);

  lives_snprintf(mainw->proj_load_dir, PATH_MAX, "%s", file_name);
  get_dirname(mainw->proj_load_dir);
  lives_free(file_name);
}


void mt_memory_free(void) {
  int i;

  threaded_dialog_spin(0.);

  mainw->multitrack->no_expose = TRUE;

  if (CURRENT_CLIP_HAS_AUDIO) {
    delete_audio_tracks(mainw->multitrack, mainw->multitrack->audio_draws, FALSE);
    if (mainw->multitrack->audio_vols != NULL) lives_list_free(mainw->multitrack->audio_vols);
  }

  if (mainw->multitrack->video_draws != NULL) {
    for (i = 0; i < mainw->multitrack->num_video_tracks; i++) {
      delete_video_track(mainw->multitrack, i, FALSE);
    }
    lives_list_free(mainw->multitrack->video_draws);
  }

  lives_widget_object_unref(mainw->multitrack->clip_scroll);
  lives_widget_object_unref(mainw->multitrack->in_out_box);

  lives_list_free(mainw->multitrack->tl_marks);

  if (mainw->multitrack->event_list != NULL) event_list_free(mainw->multitrack->event_list);
  mainw->multitrack->event_list = NULL;

  if (mainw->multitrack->undo_mem != NULL) event_list_free_undos(mainw->multitrack);

  recover_layout_cancelled(FALSE);

  threaded_dialog_spin(0.);
}


void del_current_set(boolean exit_after) {
  char *msg;
  boolean moc = mainw->only_close;
  mainw->only_close = !exit_after;
  set_string_pref(PREF_AR_CLIPSET, "");
  prefs->ar_clipset = FALSE;

  if (mainw->multitrack) {
    event_list_free_undos(mainw->multitrack);

    if (mainw->multitrack->event_list) {
      event_list_free(mainw->multitrack->event_list);
      mainw->multitrack->event_list = NULL;
    }
  }

  // check for layout maps
  if (mainw->current_layouts_map) {
    check_remove_layout_files();
  }

  if (mainw->multitrack && !mainw->only_close) mt_memory_free();
  else if (mainw->multitrack) wipe_layout(mainw->multitrack);

  mainw->was_set = mainw->leave_files = mainw->leave_recovery = FALSE;

  recover_layout_cancelled(FALSE);

  if (mainw->clips_available) {
    if (*mainw->set_name)
      msg = lives_strdup_printf(_("Deleting set %s..."), mainw->set_name);
    else
      msg = lives_strdup(_("Deleting set..."));
    d_print(msg);
    lives_free(msg);

    do_threaded_dialog(_("Deleting set"), FALSE);
  }

  prefs->crash_recovery = FALSE;

  // do a lot of cleanup, delete files
  lives_exit(0);
  prefs->crash_recovery = TRUE;

  if (*mainw->set_name) {
    d_print(_("Set %s was permanently deleted from the disk.\n"), mainw->set_name);
    lives_memset(mainw->set_name, 0, 1);
  }
  mainw->only_close = moc;
}


void on_quit_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *tmp;
  boolean legal_set_name;

  mainw->mt_needs_idlefunc = FALSE;

  if (user_data != NULL && LIVES_POINTER_TO_INT(user_data) == 1) {
    mainw->no_exit = TRUE;
    mainw->only_close = TRUE;
  } else {
    mainw->no_exit = FALSE;
    mainw->only_close = FALSE;
  }

  // stop if playing
  if (LIVES_IS_PLAYING) {
    mainw->cancelled = CANCEL_APP_QUIT;
    mainw->only_close = mainw->is_exiting = FALSE;
    return;
  }

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (mainw->multitrack != NULL && mainw->multitrack->event_list != NULL) {
    if (mainw->only_close) {
      if (!check_for_layout_del(mainw->multitrack, FALSE)) {
        if (mainw->multitrack != NULL) {
          mt_sensitise(mainw->multitrack);
          maybe_add_mt_idlefunc();
        }
        return;
      }
    }
  }

  if (mainw->stored_event_list != NULL && mainw->stored_event_list_changed) {
    if (!check_for_layout_del(NULL, FALSE)) {
      mainw->only_close = mainw->is_exiting = FALSE;
      return;
    }
  } else if (mainw->stored_layout_undos != NULL) {
    stored_event_list_free_undos();
  }

  /// do not popup warning if set name is changed
  mainw->suppress_layout_warnings = TRUE;

  if (mainw->clips_available > 0) {
    char *set_name;
    _entryw *cdsw = create_cds_dialog(1);
    LiVESResponseType resp;
    do {
      legal_set_name = TRUE;
      lives_widget_show_all(cdsw->dialog);
      resp = lives_dialog_run(LIVES_DIALOG(cdsw->dialog));

      if (resp == LIVES_RESPONSE_CANCEL) {
        lives_widget_destroy(cdsw->dialog);
        lives_free(cdsw);
        mainw->only_close = mainw->is_exiting = FALSE;
        if (mainw->multitrack != NULL) {
          mt_sensitise(mainw->multitrack);
          maybe_add_mt_idlefunc();
        }
        mainw->only_close = mainw->is_exiting = FALSE;
        return;
      }
      if (resp == LIVES_RESPONSE_ABORT) { /// TODO
        // save set
        if ((legal_set_name = is_legal_set_name((set_name = U82F(lives_entry_get_text(LIVES_ENTRY(cdsw->entry)))), TRUE))) {
          lives_widget_destroy(cdsw->dialog);
          lives_free(cdsw);

          if (prefs->ar_clipset) set_string_pref(PREF_AR_CLIPSET, set_name);
          else set_string_pref(PREF_AR_CLIPSET, "");
          mainw->no_exit = FALSE;
          mainw->leave_recovery = FALSE;

          on_save_set_activate(NULL, (tmp = U82F(set_name)));
          lives_free(tmp);

          if (mainw->multitrack != NULL) {
            mt_sensitise(mainw->multitrack);
            maybe_add_mt_idlefunc();
          }
          lives_free(set_name);
          mainw->only_close = mainw->is_exiting = FALSE;
          return;
        }
        legal_set_name = FALSE;
        lives_widget_hide(cdsw->dialog);
        lives_free(set_name);
      }
      if (mainw->was_set && legal_set_name) {
        // TODO
        //if (check_for_executable(&capable->has_gio, EXEC_GIO)) mainw->add_trash_rb = TRUE;
        if (!do_warning_dialog(_("\n\nSet will be deleted from the disk.\nAre you sure ?\n"))) {
          resp = LIVES_RESPONSE_ABORT;
        }
        //mainw->add_trash_rb = FALSE;
      }
    } while (resp == LIVES_RESPONSE_ABORT);


    lives_widget_destroy(cdsw->dialog);
    lives_free(cdsw);

    // discard clipset
    del_current_set(!mainw->only_close);
  }

  mainw->leave_recovery = TRUE;
}


// TODO - split into undo.c
void on_undo_activate(LiVESWidget * menuitem, livespointer user_data) {
  char *com, *tmp;

  boolean bad_header = FALSE;
  boolean retvalb;

  int ostart = cfile->start;
  int oend = cfile->end;
  int current_file = mainw->current_file;
  int switch_file = current_file;
  int asigned, aendian;
  int i;

  if (mainw->multitrack != NULL) return;

  lives_widget_set_sensitive(mainw->undo, FALSE);
  lives_widget_set_sensitive(mainw->redo, TRUE);
  cfile->undoable = FALSE;
  cfile->redoable = TRUE;
  lives_widget_hide(mainw->undo);
  lives_widget_show(mainw->redo);

  mainw->osc_block = TRUE;

  d_print("");

  if (menuitem != NULL) {
    mainw->no_switch_dprint = TRUE;
    d_print("%s...", lives_menu_item_get_text(LIVES_WIDGET(menuitem)));
    mainw->no_switch_dprint = FALSE;
  }

  if (cfile->undo_action == UNDO_INSERT_SILENCE) {
    double start = cfile->undo1_dbl;
    // if old audio end < start then we want to delete from oae to end
    if (cfile->old_laudio_time < start) cfile->undo1_dbl = cfile->old_laudio_time;
    on_del_audio_activate(NULL, NULL);
    cfile->undo_action = UNDO_INSERT_SILENCE;
    set_redoable(_("Insert Silence"), TRUE);
    cfile->undo1_dbl = start;
  }

  if (cfile->undo_action == UNDO_CUT || cfile->undo_action == UNDO_DELETE || cfile->undo_action == UNDO_DELETE_AUDIO) {
    int reset_achans = 0;
    lives_rm(cfile->info_file);
    if (cfile->achans != cfile->undo_achans) {
      if (cfile->audio_waveform != NULL) {
        for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
        lives_freep((void **)&cfile->audio_waveform);
        lives_freep((void **)&cfile->aw_sizes);
      }
    }

    cfile->arate = cfile->undo_arate;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->arps = cfile->undo_arps;

    if (cfile->frames == 0) {
      cfile->hsize = cfile->ohsize;
      cfile->vsize = cfile->ovsize;
    }

    if (cfile->undo_action == UNDO_DELETE_AUDIO) {
      if (cfile->undo1_dbl == cfile->undo2_dbl && cfile->undo1_dbl == 0.) {
        // undo delete_all_audio
        reset_achans = cfile->undo_achans;
        com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, cfile->handle);
      }
      // undo delete selected audio
      // (set with with_audio==2 [audio only],therfore start,end,where are in secs.; times==-1)
      else com = lives_strdup_printf("%s insert \"%s\" \"%s\" %.8f 0. %.8f \"%s\" 2 0 0 0 0 %d %d %d %d %d -1",
                                       prefs->backend,
                                       cfile->handle, get_image_ext_for_type(cfile->img_type), cfile->undo1_dbl,
                                       cfile->undo2_dbl - cfile->undo1_dbl, cfile->handle, cfile->arps, cfile->achans,
                                       cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                                       !(cfile->signed_endian & AFORM_BIG_ENDIAN));
    } else {
      // undo cut or delete (times to insert is -1)
      // start,end, where are in frames
      cfile->undo1_boolean &= mainw->ccpd_with_sound;
      com = lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d 0 0 %.3f %d %d %d %d %d -1",
                                prefs->backend, cfile->handle,
                                get_image_ext_for_type(cfile->img_type), cfile->undo_start - 1, cfile->undo_start,
                                cfile->undo_end, cfile->handle, cfile->undo1_boolean, cfile->frames, cfile->fps,
                                cfile->arps, cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                                !(cfile->signed_endian & AFORM_BIG_ENDIAN));

    }

    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) return;

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE, FALSE, _("Undoing"));

    if (mainw->error) {
      d_print_failed();
      //cfile->may_be_damaged=TRUE;
      return;
    }

    if (cfile->undo_action != UNDO_DELETE_AUDIO) {
      cfile->insert_start = cfile->undo_start;
      cfile->insert_end = cfile->undo_end;

      if (cfile->start >= cfile->undo_start) {
        cfile->start += cfile->undo_end - cfile->undo_start + 1;
      }
      if (cfile->end >= cfile->undo_start) {
        cfile->end += cfile->undo_end - cfile->undo_start + 1;
      }

      cfile->frames += cfile->undo_end - cfile->undo_start + 1;
      if (cfile->frames > 0) {
        if (cfile->start == 0) {
          cfile->start = 1;
        }
        if (cfile->end == 0) {
          cfile->end = cfile->frames;
        }
      }
      if (cfile->frame_index_back != NULL) {
        restore_frame_index_back(mainw->current_file);
      }
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames)) bad_header = TRUE;
      showclipimgs();
    }
    if (reset_achans > 0) {
      if (cfile->audio_waveform != NULL) {
        for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
        lives_freep((void **)&cfile->audio_waveform);
        lives_freep((void **)&cfile->aw_sizes);
      }
      asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
      aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;
      cfile->achans = reset_achans;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_AENDIAN, &aendian)) bad_header = TRUE;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASIGNED, &asigned)) bad_header = TRUE;
    }

    reget_afilesize(mainw->current_file);
    showclipimgs();

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_RESIZABLE || cfile->undo_action == UNDO_RENDER || cfile->undo_action == UNDO_EFFECT ||
      cfile->undo_action == UNDO_MERGE || (cfile->undo_action == UNDO_ATOMIC_RESAMPLE_RESIZE &&
          (cfile->frames != cfile->old_frames || cfile->hsize != cfile->ohsize ||
           cfile->vsize != cfile->ovsize || cfile->fps != cfile->undo1_dbl))) {
    char *audfile;

    com = lives_strdup_printf("%s undo \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle, cfile->undo_start, cfile->undo_end,
                              get_image_ext_for_type(cfile->img_type));
    lives_rm(cfile->info_file);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) return;

    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;

    // show a progress dialog, not cancellable
    cfile->progress_start = cfile->undo_start;
    cfile->progress_end = cfile->undo_end;
    do_progress_dialog(TRUE, FALSE, _("Undoing"));

    if (cfile->undo_action == UNDO_RENDER || cfile->undo_action == UNDO_MERGE) {
      audfile = lives_get_audio_file_name(mainw->current_file);
      tmp = lives_strdup_printf("%s.%s", audfile, LIVES_FILE_EXT_BAK);
      lives_free(audfile);
      audfile = tmp;
      if (lives_file_test(audfile, LIVES_FILE_TEST_EXISTS)) {
        // restore overwritten audio
        com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, cfile->handle);
        lives_rm(cfile->info_file);
        lives_system(com, FALSE);
        lives_free(com);
        if (THREADVAR(com_failed)) {
          lives_free(audfile);
          return;
        }
        retvalb = do_auto_dialog(_("Restoring audio"), 0);
        if (!retvalb) {
          d_print_failed();
          //cfile->may_be_damaged=TRUE;
          return;
        }
      }
      lives_free(audfile);
    }

    if (cfile->frame_index_back != NULL) {
      int *tmpindex = cfile->frame_index;
      cfile->clip_type = CLIP_TYPE_FILE;
      cfile->frame_index = cfile->frame_index_back;
      if (cfile->undo_action == UNDO_RENDER) {
        do_threaded_dialog(_("Clearing frame images"), FALSE);
        clean_images_from_virtual(cfile, cfile->undo_start, cfile->undo_end);
        save_frame_index(mainw->current_file);
        cfile->frame_index_back = NULL;
        end_threaded_dialog();
      } else {
        save_frame_index(mainw->current_file);
        cfile->frame_index_back = tmpindex;
      }
    }
  }

  if (cfile->undo_action == UNDO_ATOMIC_RESAMPLE_RESIZE && (cfile->frames != cfile->old_frames ||
      cfile->hsize != cfile->ohsize || cfile->vsize != cfile->ovsize)) {

    if (cfile->frames > cfile->old_frames) {
      com = lives_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
                                prefs->backend, cfile->handle, cfile->old_frames + 1,
                                cfile->frames, FALSE, cfile->frames, get_image_ext_for_type(cfile->img_type),
                                cfile->fps, cfile->arate, cfile->achans, cfile->asampsize);

      cfile->progress_start = cfile->old_frames + 1;
      cfile->progress_end = cfile->frames;

      lives_rm(cfile->info_file);
      lives_system(com, FALSE);
      lives_free(com);

      if (THREADVAR(com_failed)) return;

      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE, FALSE, _("Deleting excess frames"));

      if (cfile->clip_type == CLIP_TYPE_FILE) {
        delete_frames_from_virtual(mainw->current_file, cfile->old_frames + 1, cfile->frames);
      }
    }

    cfile->frames = cfile->old_frames;
    cfile->hsize = cfile->ohsize;
    cfile->vsize = cfile->ovsize;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;
    cfile->fps = cfile->undo1_dbl;
    if (cfile->clip_type == CLIP_TYPE_FILE && cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      double dfps = (double)cdata->fps;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &dfps)) bad_header = TRUE;
    } else {
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps)) bad_header = TRUE;
    }
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->fps)) bad_header = TRUE;
    cfile->redoable = FALSE;
    // force a resize in switch_to_file
    switch_file = 0;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_RENDER) {
    cfile->frames = cfile->old_frames;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames)) bad_header = TRUE;
    showclipimgs();
    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_INSERT || cfile->undo_action == UNDO_MERGE
      || cfile->undo_action == UNDO_INSERT_WITH_AUDIO) {
    boolean ccpd_with_sound = mainw->ccpd_with_sound;
    if (!(cfile->undo_action == UNDO_MERGE && cfile->insert_start == cfile->undo_start && cfile->insert_end == cfile->undo_end)) {
      if (cfile->undo_action == UNDO_MERGE) {
        if (cfile->insert_start == cfile->undo_start) {
          cfile->insert_start = cfile->undo_end + 1;
        }
        if (cfile->insert_end == cfile->undo_end) {
          cfile->insert_end = cfile->undo_start - 1;
        }
      }
      cfile->start = cfile->insert_start;
      cfile->end = cfile->insert_end;

      if (cfile->undo_action == UNDO_INSERT_WITH_AUDIO) mainw->ccpd_with_sound = TRUE;
      else mainw->ccpd_with_sound = FALSE;
      on_delete_activate(NULL, NULL);

      cfile->start = ostart;
      if (ostart >= cfile->insert_start) {
        cfile->start -= cfile->insert_end - cfile->insert_start + 1;
        if (cfile->start < cfile->insert_start - 1) {
          cfile->start = cfile->insert_start - 1;
        }
      }
      cfile->end = oend;
      if (oend >= cfile->insert_start) {
        cfile->end -= cfile->insert_end - cfile->insert_start + 1;
        if (cfile->end < cfile->insert_start - 1) {
          cfile->end = cfile->insert_start - 1;
        }
      }
      // TODO - use lives_clip_start macro
      if (cfile->start < 1) cfile->start = cfile->frames > 0 ? 1 : 0;
      if (cfile->end < 1) cfile->end = cfile->frames > 0 ? 1 : 0;

      cfile->insert_start = cfile->insert_end = 0;
    }
    mainw->ccpd_with_sound = ccpd_with_sound;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames)) bad_header = TRUE;
    showclipimgs();
    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_REC_AUDIO) {
    mainw->fx1_val = cfile->arate;
    mainw->fx2_val = cfile->achans;
    mainw->fx3_val = cfile->asampsize;
    mainw->fx4_val = cfile->signed_endian;
    mainw->fx5_val = cfile->arps;
  }

  if (cfile->undo_action == UNDO_AUDIO_RESAMPLE || cfile->undo_action == UNDO_REC_AUDIO ||
      cfile->undo_action == UNDO_FADE_AUDIO || cfile->undo_action == UNDO_AUDIO_VOL ||
      cfile->undo_action == UNDO_TRIM_AUDIO || cfile->undo_action == UNDO_APPEND_AUDIO ||
      (cfile->undo_action == UNDO_ATOMIC_RESAMPLE_RESIZE && cfile->arate != cfile->undo1_int)) {
    lives_rm(cfile->info_file);
    com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, cfile->handle);
    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      reget_afilesize(mainw->current_file);
      d_print_failed();
      return;
    }
    if (!do_auto_dialog(_("Undoing"), 0)) {
      reget_afilesize(mainw->current_file);
      d_print_failed();
      return;
    }
  }

  if ((cfile->undo_action == UNDO_AUDIO_RESAMPLE) || (cfile->undo_action == UNDO_ATOMIC_RESAMPLE_RESIZE &&
      cfile->arate != cfile->undo1_int)) {
    cfile->arate += cfile->undo1_int;
    cfile->undo1_int = cfile->arate - cfile->undo1_int;
    cfile->arate -= cfile->undo1_int;

    cfile->achans += cfile->undo2_int;
    cfile->undo2_int = cfile->achans - cfile->undo2_int;
    cfile->achans -= cfile->undo2_int;

    cfile->asampsize += cfile->undo3_int;
    cfile->undo3_int = cfile->asampsize - cfile->undo3_int;
    cfile->asampsize -= cfile->undo3_int;

    cfile->arps += cfile->undo4_int;
    cfile->undo4_int = cfile->arps - cfile->undo4_int;
    cfile->arps -= cfile->undo4_int;

    cfile->signed_endian += cfile->undo1_uint;
    cfile->undo1_uint = cfile->signed_endian - cfile->undo1_uint;
    cfile->signed_endian -= cfile->undo1_uint;

    asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
    aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;

    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_AENDIAN, &aendian)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASIGNED, &asigned)) bad_header = TRUE;

    reget_afilesize(mainw->current_file);

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_NEW_AUDIO) {
    lives_rm(cfile->info_file);
    com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) return;

    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;

    if (!do_auto_dialog(_("Restoring audio"), 0)) {
      d_print_failed();
      return;
    }

    if (cfile->achans != cfile->undo_achans) {
      if (cfile->audio_waveform != NULL) {
        for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
        lives_freep((void **)&cfile->audio_waveform);
        lives_freep((void **)&cfile->aw_sizes);
      }
    }

    cfile->achans = cfile->undo_achans;
    cfile->arate = cfile->undo_arate;
    cfile->arps = cfile->undo_arps;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;

    reget_afilesize(mainw->current_file);

    asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
    aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;

    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_AENDIAN, &aendian)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASIGNED, &asigned)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_CHANGE_SPEED) {
    cfile->fps += cfile->undo1_dbl;
    cfile->undo1_dbl = cfile->fps - cfile->undo1_dbl;
    cfile->fps -= cfile->undo1_dbl;

    cfile->arate += cfile->undo1_int;
    cfile->undo1_int = cfile->arate - cfile->undo1_int;
    cfile->arate -= cfile->undo1_int;
    /// DON'T ! we only save the pb_fps now, since otherwise we may lose the orig. clip fps
    /* save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps); */
    if (cfile->clip_type == CLIP_TYPE_FILE && cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      double dfps = (double)cdata->fps;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &dfps)) bad_header = TRUE;
    } else {
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps)) bad_header = TRUE;
    }
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->fps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_INSERT || cfile->undo_action == UNDO_INSERT_WITH_AUDIO || cfile->undo_action == UNDO_MERGE ||
      cfile->undo_action == UNDO_NEW_AUDIO) {
    cfile->redoable = FALSE;
  }

  if (menuitem != NULL) {
    mainw->no_switch_dprint = TRUE;
    d_print_done();
    mainw->no_switch_dprint = FALSE;
  }

  if (cfile->undo_action == UNDO_RESAMPLE) {
    cfile->start = (int)((cfile->start - 1) / cfile->fps * cfile->undo1_dbl + 1.);
    if ((cfile->end = (int)(cfile->end / cfile->fps * cfile->undo1_dbl + .49999)) < 1) cfile->end = 1;
    cfile->fps += cfile->undo1_dbl;
    cfile->undo1_dbl = cfile->fps - cfile->undo1_dbl;
    cfile->fps -= cfile->undo1_dbl;

    // deorder the frames
    cfile->frames = deorder_frames(cfile->old_frames, mainw->current_file == 0 && !prefs->conserve_space);

    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames)) bad_header = TRUE;
    if (cfile->clip_type == CLIP_TYPE_FILE && cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      double dfps = (double)cdata->fps;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &dfps)) bad_header = TRUE;
    } else {
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps)) bad_header = TRUE;
    }
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->fps)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);

    if (mainw->current_file > 0) {
      com = lives_strdup_printf(_("Length of video is now %d frames at %.3f frames per second.\n"), cfile->frames, cfile->fps);
    } else {
      mainw->no_switch_dprint = TRUE;
      com = lives_strdup_printf(_("Clipboard was resampled to %d frames.\n"), cfile->frames);
    }
    d_print(com);
    lives_free(com);
    mainw->no_switch_dprint = FALSE;
    showclipimgs();
  }

  if (cfile->end > cfile->frames) {
    cfile->end = cfile->frames;
  }

  if (cfile->undo_action == UNDO_RESIZABLE) {
    cfile->vsize += cfile->ovsize;
    cfile->ovsize = cfile->vsize - cfile->ovsize;
    cfile->vsize -= cfile->ovsize;
    cfile->hsize += cfile->ohsize;
    cfile->ohsize = cfile->hsize - cfile->ohsize;
    cfile->hsize -= cfile->ohsize;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;

    // force a resize in switch_to_file
    switch_file = 0;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (current_file > 0) {
    switch_to_file((mainw->current_file = switch_file), current_file);
  }

  if (cfile->undo_action == UNDO_RENDER) {
    if (mainw->event_list != NULL) event_list_free(mainw->event_list);
    mainw->event_list = cfile->event_list_back;
    cfile->event_list_back = NULL;
    deal_with_render_choice(FALSE);
  }
  mainw->osc_block = FALSE;
}


void on_redo_activate(LiVESWidget * menuitem, livespointer user_data) {
  char *com;

  int ostart = cfile->start;
  int oend = cfile->end;
  int current_file = mainw->current_file;
  int i;

  mainw->osc_block = TRUE;

  cfile->undoable = TRUE;
  cfile->redoable = FALSE;
  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo, TRUE);
  lives_widget_set_sensitive(mainw->redo, FALSE);

  d_print("");

  if (menuitem != NULL) {
    mainw->no_switch_dprint = TRUE;
    d_print("%s...", lives_menu_item_get_text(menuitem));
    mainw->no_switch_dprint = FALSE;
  }

  if (cfile->undo_action == UNDO_INSERT_SILENCE) {
    on_ins_silence_activate(NULL, NULL);
    mainw->osc_block = FALSE;
    mainw->no_switch_dprint = TRUE;
    d_print_done();
    mainw->no_switch_dprint = FALSE;
    sensitize();
    return;
  }
  if (cfile->undo_action == UNDO_CHANGE_SPEED) {
    on_change_speed_ok_clicked(NULL, NULL);
    mainw->osc_block = FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action == UNDO_RESAMPLE) {
    on_resample_vid_ok(NULL, NULL);
    mainw->osc_block = FALSE;
    return;
  }
  if (cfile->undo_action == UNDO_AUDIO_RESAMPLE) {
    on_resaudio_ok_clicked(NULL, NULL);
    mainw->osc_block = FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action == UNDO_CUT || cfile->undo_action == UNDO_DELETE) {
    cfile->start = cfile->undo_start;
    cfile->end = cfile->undo_end;
    mainw->osc_block = FALSE;
  }
  if (cfile->undo_action == UNDO_CUT) {
    on_cut_activate(NULL, NULL);
    mainw->osc_block = FALSE;
  }
  if (cfile->undo_action == UNDO_DELETE) {
    on_delete_activate(NULL, NULL);
    mainw->osc_block = FALSE;
  }
  if (cfile->undo_action == UNDO_DELETE_AUDIO) {
    on_del_audio_activate(NULL, NULL);
    mainw->osc_block = FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action == UNDO_CUT || cfile->undo_action == UNDO_DELETE) {
    cfile->start = ostart;
    cfile->end = oend;
    if (mainw->current_file == current_file) {
      if (cfile->start >= cfile->undo_start) {
        cfile->start -= cfile->undo_end - cfile->undo_start + 1;
        if (cfile->start < cfile->undo_start - 1) {
          cfile->start = cfile->undo_start - 1;
        }
      }
      if (cfile->end >= cfile->undo_start) {
        cfile->end -= cfile->undo_end - cfile->undo_start + 1;
        if (cfile->end < cfile->undo_start - 1) {
          cfile->end = cfile->undo_start - 1;
        }
      }
      switch_to_file(mainw->current_file, mainw->current_file);
    }
    mainw->osc_block = FALSE;
    return;
  }

  if (cfile->undo_action == UNDO_REC_AUDIO) {
    if (cfile->audio_waveform != NULL) {
      for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
      lives_freep((void **)&cfile->audio_waveform);
      lives_freep((void **)&cfile->aw_sizes);
    }
    cfile->arate = mainw->fx1_val;
    cfile->achans = mainw->fx2_val;
    cfile->asampsize = mainw->fx3_val;
    cfile->signed_endian = mainw->fx4_val;
    cfile->arps = mainw->fx5_val;
    save_clip_values(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_REC_AUDIO || cfile->undo_action == UNDO_FADE_AUDIO
      || cfile->undo_action == UNDO_TRIM_AUDIO || cfile->undo_action == UNDO_AUDIO_VOL ||
      cfile->undo_action == UNDO_APPEND_AUDIO) {
    com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, cfile->handle);
    lives_rm(cfile->info_file);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      reget_afilesize(mainw->current_file);
      d_print_failed();
      return;
    }

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE, FALSE, _("Redoing"));

    if (mainw->error) {
      reget_afilesize(mainw->current_file);
      d_print_failed();
      return;
    }

    d_print_done();
    switch_to_file(mainw->current_file, mainw->current_file);
    mainw->osc_block = FALSE;
    return;
  }

  com = lives_strdup_printf("%s redo \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle, cfile->undo_start, cfile->undo_end,
                            get_image_ext_for_type(cfile->img_type));
  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    return;
  }

  cfile->progress_start = cfile->undo_start;
  cfile->progress_end = cfile->undo_end;

  // show a progress dialog, not cancellable
  do_progress_dialog(TRUE, FALSE, _("Redoing"));
  reget_afilesize(mainw->current_file);

  if (mainw->error) {
    d_print_failed();
    return;
  }

  if (cfile->clip_type == CLIP_TYPE_FILE && (cfile->undo_action == UNDO_EFFECT || cfile->undo_action == UNDO_RESIZABLE)) {
    int *tmpindex = cfile->frame_index;
    cfile->frame_index = cfile->frame_index_back;
    cfile->frame_index_back = tmpindex;
    cfile->clip_type = CLIP_TYPE_FILE;
    if (!check_if_non_virtual(mainw->current_file, 1, cfile->frames)) save_frame_index(mainw->current_file);
  }

  if (cfile->undo_action == UNDO_RESIZABLE) {
    cfile->vsize += cfile->ovsize;
    cfile->ovsize = cfile->vsize - cfile->ovsize;
    cfile->vsize -= cfile->ovsize;
    cfile->hsize += cfile->ohsize;
    cfile->ohsize = cfile->hsize - cfile->ohsize;
    cfile->hsize -= cfile->ohsize;
    switch_to_file((mainw->current_file = 0), current_file);
  } else {
    if (cfile->end <= cfile->undo_end) load_end_image(cfile->end);
    if (cfile->start >= cfile->undo_start) load_start_image(cfile->start);
  }

  d_print_done();
  mainw->osc_block = FALSE;
}


//////////////////////////////////////////////////

void on_copy_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *com;

  int current_file = mainw->current_file;
  int start, end;
  int i;

  desensitize();

  d_print(""); // force switchtext

  if (mainw->ccpd_with_sound && cfile->achans > 0)
    d_print(_("Copying frames %d to %d (with sound) to the clipboard..."), cfile->start, cfile->end);
  else
    d_print(lives_strdup_printf(_("Copying frames %d to %d to the clipboard..."), cfile->start, cfile->end));

  init_clipboard();

  lives_rm(cfile->info_file);
  mainw->last_transition_loops = 1;

  start = cfile->start;
  end = cfile->end;

  if (cfile->clip_type == CLIP_TYPE_FILE) {
    // for virtual frames, we copy only the frame_index
    clipboard->clip_type = CLIP_TYPE_FILE;
    clipboard->interlace = cfile->interlace;
    clipboard->deinterlace = cfile->deinterlace;
    clipboard->frame_index = frame_index_copy(cfile->frame_index, end - start + 1, start - 1);
    clipboard->frames = end - start + 1;
    check_if_non_virtual(0, 1, clipboard->frames);
    if (clipboard->clip_type == CLIP_TYPE_FILE) {
      clipboard->ext_src = clone_decoder(mainw->current_file);
      clipboard->ext_src_type = LIVES_EXT_SRC_DECODER;
      end = -end; // allow missing frames
      lives_snprintf(clipboard->file_name, PATH_MAX, "%s", cfile->file_name);
    }
  }

  mainw->fx1_val = 1;
  mainw->fx1_bool = FALSE;

  clipboard->img_type = cfile->img_type;

  // copy audio and frames
  com = lives_strdup_printf("%s insert \"%s\" \"%s\" 0 %d %d \"%s\" %d 0 0 0 %.3f %d %d %d %d %d", prefs->backend,
                            clipboard->handle, get_image_ext_for_type(clipboard->img_type),
                            start, end, cfile->handle, mainw->ccpd_with_sound, cfile->fps, cfile->arate,
                            cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                            !(cfile->signed_endian & AFORM_BIG_ENDIAN));

  if (clipboard->clip_type == CLIP_TYPE_FILE) end = -end;

  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    sensitize();
    return;
  }

  // we need to set this to look at the right info_file
  mainw->current_file = 0;
  cfile->progress_start = clipboard->start = 1;
  cfile->progress_end = clipboard->end = end - start + 1;

  // stop the 'preview' and 'pause' buttons from appearing
  cfile->nopreview = TRUE;
  if (!do_progress_dialog(TRUE, TRUE, _("Copying to the clipboard"))) {
#ifdef IS_MINGW
    // kill any active processes: for other OSes the backend does this
    lives_kill_subprocesses(cfile->handle, TRUE);
#endif

    // close clipboard, it is invalid
    mainw->current_file = CLIPBOARD_FILE;
    close_temp_handle(current_file);

    sensitize();
    mainw->cancelled = CANCEL_USER;
    return;
  }

  cfile->nopreview = FALSE;
  mainw->current_file = current_file;

  //set all clipboard details
  clipboard->frames = clipboard->old_frames = clipboard->end;
  clipboard->hsize = cfile->hsize;
  clipboard->vsize = cfile->vsize;
  clipboard->bpp = cfile->bpp;
  clipboard->gamma_type = cfile->gamma_type;
  clipboard->undo1_dbl = clipboard->fps = cfile->fps;
  clipboard->ratio_fps = cfile->ratio_fps;
  clipboard->is_loaded = TRUE;
  lives_snprintf(clipboard->type, 40, "Frames");

  clipboard->asampsize = clipboard->arate = clipboard->achans = 0;
  clipboard->afilesize = 0l;

  if (mainw->ccpd_with_sound) {
    if (clipboard->audio_waveform != NULL) {
      for (i = 0; i < clipboard->achans; lives_freep((void **)&clipboard->audio_waveform[i++]));
      lives_freep((void **)&clipboard->audio_waveform);
      lives_freep((void **)&clipboard->aw_sizes);
    }
    clipboard->achans = cfile->achans;
    clipboard->asampsize = cfile->asampsize;

    clipboard->arate = cfile->arate;
    clipboard->arps = cfile->arps;
    clipboard->signed_endian = cfile->signed_endian;

    reget_afilesize(0);
  }

  clipboard->start = 1;
  clipboard->end = clipboard->frames;

  get_total_time(clipboard);

  sensitize();
  d_print_done();
}


void on_cut_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  uint32_t chk_mask = 0;
  int current_file = mainw->current_file;

  if (menuitem != NULL) {
    char *tmp = (_("Cutting"));
    chk_mask = WARN_MASK_LAYOUT_DELETE_FRAMES | WARN_MASK_LAYOUT_SHIFT_FRAMES | WARN_MASK_LAYOUT_ALTER_FRAMES;
    if (mainw->ccpd_with_sound) chk_mask |= WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_SHIFT_AUDIO |
                                              WARN_MASK_LAYOUT_ALTER_AUDIO;
    if (!check_for_layout_errors(tmp, mainw->current_file, cfile->start, cfile->end, &chk_mask)) {
      lives_free(tmp);
      return;
    }
    lives_free(tmp);
  }

  on_copy_activate(menuitem, user_data);
  if (mainw->cancelled) {
    unbuffer_lmap_errors(FALSE);
    return;
  }

  on_delete_activate(NULL, user_data);
  if (mainw->current_file == current_file) {
    set_undoable(_("Cut"), TRUE);
    cfile->undo_action = UNDO_CUT;
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}


void on_paste_as_new_activate(LiVESMenuItem * menuitem, livespointer user_data) {
#define VIRT_PASTE
#ifndef VIRT_PASTE
  char *msg;
#endif
  char *com;
  int old_file = mainw->current_file;
  frames_t cbframes, lframe;

  if (!clipboard) return;

  mainw->current_file = mainw->first_free_file;

  if (!get_new_handle(mainw->current_file, NULL)) {
    mainw->current_file = old_file;
    return;
  }

  lframe = cbframes = clipboard->frames;

  //set file details
  cfile->hsize = clipboard->hsize;
  cfile->vsize = clipboard->vsize;
  cfile->pb_fps = cfile->fps = clipboard->fps;
  cfile->ratio_fps = clipboard->ratio_fps;
  cfile->changed = TRUE;
  cfile->is_loaded = TRUE;
  cfile->img_type = clipboard->img_type;
  cfile->gamma_type = clipboard->gamma_type;

  set_default_comment(cfile, NULL);

#ifndef VIRT_PASTE
  msg = (_("Pulling frames from clipboard..."));

  if ((lframe = realize_all_frames(0, msg, TRUE)) < cbframes) {
    if (!paste_enough_dlg(lframe - 1)) {
      lives_free(msg);
      close_current_file(old_file);
      sensitize();
      return;
    }
    lframe--;
  }
  lives_free(msg);
#else
  /// copy the frame_index, and do an insert, skipping missing frames
  lframe = -lframe;
#endif

  cfile->progress_start = cfile->start = cbframes > 0 ? 1 : 0;
  cfile->progress_end = cfile->end = cfile->frames = cbframes;

  mainw->no_switch_dprint = TRUE;
  d_print(_("Pasting %d frames to new clip %s..."), lframe, cfile->name);
  mainw->no_switch_dprint = FALSE;

  if (clipboard->achans > 0 && clipboard->arate > 0) {
    com = lives_strdup_printf("%s insert \"%s\" \"%s\" 0 1 %d \"%s\" %d 0 0 0 %.3f %d %d %d %d %d",
                              prefs->backend, cfile->handle,
                              get_image_ext_for_type(cfile->img_type), lframe, clipboard->handle,
                              mainw->ccpd_with_sound, clipboard->fps, clipboard->arate, clipboard->achans,
                              clipboard->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                              !(cfile->signed_endian & AFORM_BIG_ENDIAN));
  } else {
    com = lives_strdup_printf("%s insert \"%s\" \"%s\" 0 1 %d \"%s\" %d 0 0 0 %.3f 0 0 0 0 0",
                              prefs->backend, cfile->handle,
                              get_image_ext_for_type(cfile->img_type), lframe, clipboard->handle,
                              FALSE, clipboard->fps);

    if (clipboard->achans > 0 && clipboard->arate < 0) {
      int zero = 0;
      double chvols = 1.;
      double avels = -1.;
      double aseeks = (double)clipboard->afilesize / (double)(-clipboard->arate * clipboard->asampsize / 8 * clipboard->achans);
      ticks_t tc = (ticks_t)(aseeks * TICKS_PER_SECOND_DBL);
      cfile->arate = clipboard->arate = -clipboard->arate;
      cfile->arps = clipboard->arps;
      cfile->achans = clipboard->achans;
      cfile->asampsize = clipboard->asampsize;
      cfile->afilesize = clipboard->afilesize;
      cfile->signed_endian = clipboard->signed_endian;
      render_audio_segment(1, &zero, mainw->current_file, &avels, &aseeks, 0, tc, &chvols, 1., 1., NULL);
    }
  }

  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    close_current_file(old_file);
    return;
  }

  cfile->nopreview = TRUE;

  // show a progress dialog, not cancellable
  if (!do_progress_dialog(TRUE, TRUE, _("Pasting"))) {
    if (mainw->error) d_print_failed();
    close_current_file(old_file);
    return;
  }
  cfile->nopreview = FALSE;

  if (mainw->ccpd_with_sound) {
    if (cfile->audio_waveform != NULL) {
      for (int i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
      lives_freep((void **)&cfile->audio_waveform);
      lives_freep((void **)&cfile->aw_sizes);
    }
    cfile->arate = clipboard->arate;
    cfile->arps = clipboard->arps;
    cfile->achans = clipboard->achans;
    cfile->asampsize = clipboard->asampsize;
    cfile->afilesize = clipboard->afilesize;
    cfile->signed_endian = clipboard->signed_endian;
    if (cfile->afilesize > 0) d_print(_("...added audio..."));
  }

#ifdef VIRT_PASTE
  if (clipboard->frame_index) {
    cfile->frame_index = frame_index_copy(clipboard->frame_index, cbframes, 0);
  }

  cfile->clip_type = clipboard->clip_type;

  if (cfile->clip_type == CLIP_TYPE_FILE) {
    cfile->ext_src = clone_decoder(CLIPBOARD_FILE);
    lives_snprintf(cfile->file_name, PATH_MAX, "%s", clipboard->file_name);
  }
#endif

  if (cfile->frame_index) save_frame_index(mainw->current_file);

  // add entry to window menu
  add_to_clipmenu();
  if (!save_clip_values(mainw->current_file)) {
    close_current_file(old_file);
    return;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

  switch_clip(1, mainw->current_file, TRUE);
  d_print_done();

  mainw->last_dprint_file = old_file;
  d_print(""); // force switchtext

  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
}


void on_insert_pre_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  insertw = create_insert_dialog();

  lives_widget_show_all(insertw->insert_dialog);
  mainw->fx1_bool = FALSE;
  mainw->fx1_val = 1;

  mainw->fx2_bool = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(insertw->with_sound));
}


void on_insert_activate(LiVESButton * button, livespointer user_data) {
  double times_to_insert;
  double audio_stretch;

  char *com;

  boolean with_sound = mainw->fx2_bool;
  boolean bad_header = FALSE;
  boolean insert_silence = FALSE;

  // have we resampled ?
  boolean cb_audio_change = FALSE;
  boolean cb_video_change = FALSE;

  boolean virtual_ins = FALSE;
  boolean all_virtual = FALSE;

  uint32_t chk_mask = 0;

  int where = cfile->start - 1;
  int start = cfile->start, ostart = start;
  int end = cfile->end, oend = end;

  int hsize = cfile->hsize;
  int vsize = cfile->vsize;

  int cfile_signed = 0, cfile_endian = 0, clipboard_signed = 0, clipboard_endian = 0;
  int current_file = mainw->current_file;

  int orig_frames = cfile->frames;
  int ocarps = clipboard->arps;
  int leave_backup = 1;
  int remainder_frames;
  int insert_start;
  int cb_start = 1, cb_end = clipboard->frames;
  int i;

  // if it is an insert into the original file, and we can do fast seek, we can insert virtual frames
  if (button != NULL && mainw->current_file == clipboard->cb_src && !check_if_non_virtual(0, 1, clipboard->frames)) {
    lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
    if (cdata->seek_flag & LIVES_SEEK_FAST) {
      virtual_ins = TRUE;
      if (count_virtual_frames(clipboard->frame_index, 1, clipboard->frames) == clipboard->frames) all_virtual = TRUE;
    }
  }

  // don't ask smogrify to resize if frames are the same size and type
  if (all_virtual || (((cfile->hsize == clipboard->hsize && cfile->vsize == clipboard->vsize) || orig_frames == 0) &&
                      (cfile->img_type == clipboard->img_type))) hsize = vsize = 0;
  else {
    if (!capable->has_convert) {
      widget_opts.non_modal = TRUE;
      do_error_dialog(
        _("This operation requires resizing or converting of frames.\n"
          "Please install 'convert' from the Image-magick package, and then restart LiVES.\n"));
      widget_opts.non_modal = FALSE;
      mainw->error = TRUE;
      if (button != NULL) {
        lives_widget_destroy(insertw->insert_dialog);
        lives_free(insertw);
      }
      return;
    }
  }

  if (button != NULL) {
    lives_widget_destroy(insertw->insert_dialog);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    // call to update fx1_val, in case activates_default was called from spin entry
    lives_free(insertw);
  }

  times_to_insert = mainw->fx1_val;

  // fit video to audio if requested
  if (mainw->fx1_bool && (cfile->asampsize * cfile->arate * cfile->achans != 0)) {
    // "insert to fit audio" : number of inserts is (audio_time - sel_end_time) / clipboard_time
    times_to_insert = (cfile->laudio_time - (cfile->frames > 0 ? (double)cfile->end / cfile->fps : 0.)) / ((
                        double)clipboard->frames / clipboard->fps);
  }

  if (times_to_insert < 0. && (mainw->fx1_bool)) {
    widget_opts.non_modal = TRUE;
    do_error_dialog(
      _("\n\nVideo is longer than audio.\nTry selecting all frames, and then using \n"
        "the 'Trim Audio' function from the Audio menu."));
    mainw->error = TRUE;
    widget_opts.non_modal = FALSE;
    return;
  }

  if (with_sound) {
    cfile_signed = !(cfile->signed_endian & AFORM_UNSIGNED);
    cfile_endian = !(cfile->signed_endian & AFORM_BIG_ENDIAN);

    clipboard_signed = !(clipboard->signed_endian & AFORM_UNSIGNED);
    clipboard_endian = !(clipboard->signed_endian & AFORM_BIG_ENDIAN);

    if ((cfile->achans * cfile->arps * cfile->asampsize > 0) && (cfile->achans != clipboard->achans ||
        (cfile->arps != clipboard->arps && clipboard->achans > 0) ||
        cfile->asampsize != clipboard->asampsize ||
        cfile_signed != clipboard_signed || cfile_endian != clipboard_endian ||
        cfile->arate != clipboard->arate)) {
      if (!(capable->has_sox_sox)) {
        if (cfile->arps != clipboard->arps) {
          widget_opts.non_modal = TRUE;
          do_error_dialog(_("LiVES cannot insert because the audio rates do not match.\n"
                            "Please install 'sox', and try again."));
          mainw->error = TRUE;
          widget_opts.non_modal = FALSE;
          return;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (mainw->insert_after) insert_start = cfile->end + 1;
  else insert_start = cfile->start;

  if (button != NULL) {
    char *tmp = (_("Insertion"));
    chk_mask = WARN_MASK_LAYOUT_SHIFT_FRAMES | WARN_MASK_LAYOUT_ALTER_FRAMES;
    if (with_sound) chk_mask |= WARN_MASK_LAYOUT_SHIFT_AUDIO | WARN_MASK_LAYOUT_ALTER_AUDIO;
    if (!check_for_layout_errors(tmp, mainw->current_file, insert_start, 0, &chk_mask)) {
      lives_free(tmp);
      return;
    }
    lives_free(tmp);
  }

  if (button != NULL) {
    if ((cfile->fps != clipboard->fps && orig_frames > 0) || (cfile->arps != clipboard->arps && clipboard->achans > 0 &&
        with_sound)) {
      if (!do_clipboard_fps_warning()) {
        unbuffer_lmap_errors(FALSE);
        mainw->error = TRUE;
        return;
      }
    }
    if (prefs->ins_resample && clipboard->fps != cfile->fps && orig_frames != 0) {
      cb_end = count_resampled_frames(clipboard->frames, clipboard->fps, cfile->fps);
    }
  } else {
    // called from on_merge_activate()
    cb_start = mainw->fx1_start;
    cb_end = mainw->fx2_start;

    // we will use leave_backup as this will leave our
    // merge backup in place
    leave_backup = -1;
  }

  cfile->insert_start = insert_start;
  cfile->insert_end = cfile->insert_start - 1;

  if (mainw->insert_after) where = cfile->end;

  // at least we should try to convert the audio to match...
  // if with_sound is TRUE, and clipboard has no audio, we will insert silence (unless target
  // also has no audio
  if (with_sound) {
    if (clipboard->achans == 0) {
      if (cfile->achans > 0) insert_silence = TRUE;
      with_sound = FALSE;
    } else {
      if ((cfile->achans * cfile->arps * cfile->asampsize > 0)
          && clipboard->achans > 0 && (cfile->achans != clipboard->achans ||
                                       cfile->arps != clipboard->arps || clipboard->vol != 1. || cfile->vol != 1. ||
                                       cfile->asampsize != clipboard->asampsize ||
                                       cfile_signed != clipboard_signed ||
                                       cfile_endian != clipboard_endian || cfile->arate != clipboard->arate)) {

        cb_audio_change = TRUE;

        if (clipboard->arps != clipboard->arps || cfile->arate != clipboard->arate) {
          // pb rate != real rate - stretch to pb rate and resample
          if ((audio_stretch = (double)clipboard->arps / (double)clipboard->arate *
                               (double)cfile->arate / (double)cfile->arps) != 1.) {
            if (audio_stretch < 0.) {
              // clipboard audio should be reversed
              // we will create a temp handle, copy the audio, and then render it back reversed
              if (!get_temp_handle(-1)) {
                d_print_failed();
                return;
              } else {
                char *fnameto = lives_get_audio_file_name(mainw->current_file);
                char *fnamefrom = lives_get_audio_file_name(0);
                int zero = 0;
                float volx = 1.;
                double chvols = 1.;
                double avels = -1.;
                double aseeks = (double)clipboard->afilesize / (double)(-clipboard->arate
                                * clipboard->asampsize / 8 * clipboard->achans);
                ticks_t tc = (ticks_t)(aseeks * TICKS_PER_SECOND_DBL);
                if (cfile->vol > 0.001) volx = clipboard->vol / cfile->vol;
                render_audio_segment(1, &zero, mainw->current_file, &avels, &aseeks, 0, tc, &chvols, volx, volx, NULL);
                reget_afilesize(0);
                reget_afilesize(mainw->current_file);
                if (cfile->afilesize == clipboard->afilesize) {
                  lives_mv(fnameto, fnamefrom);
                }
                close_temp_handle(current_file);
                clipboard->arate = -clipboard->arate;
                lives_free(fnamefrom);
                lives_free(fnameto);
              }
            } else {
              lives_rm(clipboard->info_file);
              com = lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d %.4f",
                                        prefs->backend,
                                        clipboard->handle, clipboard->arps, clipboard->achans, clipboard->asampsize,
                                        clipboard_signed, clipboard_endian, cfile->arps, clipboard->achans,
                                        clipboard->asampsize, clipboard_signed, clipboard_endian, audio_stretch);
              lives_system(com, FALSE);
              lives_free(com);

              if (THREADVAR(com_failed)) {
                unbuffer_lmap_errors(FALSE);
                return;
              }

              mainw->current_file = 0;
              mainw->error = FALSE;
              do_progress_dialog(TRUE, FALSE, _("Resampling clipboard audio"));
              mainw->current_file = current_file;
              if (mainw->error) {
                d_print_failed();
                unbuffer_lmap_errors(FALSE);
                return;
              }

              // not really, but we pretend...
              clipboard->arps = cfile->arps;
            }
          }
        }

        if (clipboard->achans > 0 && (cfile->achans != clipboard->achans || cfile->arps != clipboard->arps ||
                                      cfile->asampsize != clipboard->asampsize || cfile_signed != clipboard_signed ||
                                      cfile_endian != clipboard_endian)) {
          lives_rm(clipboard->info_file);
          com = lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d",
                                    prefs->backend, clipboard->handle,
                                    clipboard->arps, clipboard->achans, clipboard->asampsize, clipboard_signed,
                                    clipboard_endian, cfile->arps, cfile->achans, cfile->asampsize, cfile_signed, cfile_endian);
          lives_system(com, FALSE);
          lives_free(com);

          if (THREADVAR(com_failed)) {
            unbuffer_lmap_errors(FALSE);
            return;
          }

          mainw->current_file = 0;
          do_progress_dialog(TRUE, FALSE, _("Resampling clipboard audio"));
          mainw->current_file = current_file;

          if (mainw->error) {
            d_print_failed();
            unbuffer_lmap_errors(FALSE);
            return;
          }
        }

        if (clipboard->achans > 0 && clipboard->afilesize == 0l) {
          if (prefs->conserve_space) {
            // oops...
            if (clipboard->audio_waveform != NULL) {
              for (i = 0; i < clipboard->achans; lives_freep((void **)&clipboard->audio_waveform[i++]));
              lives_freep((void **)&clipboard->audio_waveform);
              lives_freep((void **)&clipboard->aw_sizes);
            }
            clipboard->achans = clipboard->arate = clipboard->asampsize = 0;
            with_sound = FALSE;
            widget_opts.non_modal = TRUE;
            do_error_dialog
            (_("\n\nLiVES was unable to resample the clipboard audio. \nClipboard audio has been erased.\n"));
            widget_opts.non_modal = FALSE;
          } else {
            lives_rm(clipboard->info_file);
            mainw->current_file = 0;
            com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, clipboard->handle);
            lives_system(com, FALSE);
            lives_free(com);
            mainw->current_file = current_file;

            clipboard->arps = ocarps;
            reget_afilesize(0);

            if (!do_yesno_dialog
                (_("\n\nLiVES was unable to resample the clipboard audio.\n"
                   "Do you wish to continue with the insert \nusing unchanged audio ?\n"))) {
              mainw->error = TRUE;
              unbuffer_lmap_errors(FALSE);
              return;
		// *INDENT-OFF*
            }}}}}}
	  // *INDENT-ON*

  if (!virtual_ins) {
    char *msg = (_("Pulling frames from clipboard..."));
    if (realize_all_frames(0, msg, FALSE) <= 0) {
      lives_free(msg);
      sensitize();
      unbuffer_lmap_errors(FALSE);
      return;
    }
    lives_free(msg);
  }

  d_print(""); // force switchtext

  // if pref is set, resample clipboard video
  if (prefs->ins_resample && cfile->fps != clipboard->fps && orig_frames > 0) {
    if (!resample_clipboard(cfile->fps)) {
      unbuffer_lmap_errors(FALSE);
      return;
    }
    cb_video_change = TRUE;
  }

  if (mainw->fx1_bool && (cfile->asampsize * cfile->arate * cfile->achans != 0)) {
    // in theory this should not change after resampling, but we will recalculate anyway

    // "insert to fit audio" : number of inserts is (audio_time - video_time) / clipboard_time
    times_to_insert = (cfile->laudio_time - cfile->frames > 0 ? (double)cfile->frames / cfile->fps : 0.) / ((
                        double)clipboard->frames / clipboard->fps);
  }

  switch_clip(1, current_file, TRUE);

  if (cb_end > clipboard->frames) {
    cb_end = clipboard->frames;
  }

  if (with_sound && cfile->achans == 0) {
    int asigned = !(clipboard->signed_endian & AFORM_UNSIGNED);
    int endian = clipboard->signed_endian & AFORM_BIG_ENDIAN;

    cfile->achans = clipboard->achans;
    cfile->asampsize = clipboard->asampsize;
    cfile->arps = cfile->arate = clipboard->arate;
    cfile->signed_endian = clipboard->signed_endian;

    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASIGNED, &asigned)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_AENDIAN, &endian)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  // first remainder frames
  remainder_frames = (int)(times_to_insert - (double)(int)times_to_insert) * clipboard->frames;

  end = clipboard->frames;
  if (virtual_ins) end = -end;

  if (!mainw->insert_after && remainder_frames > 0) {
    d_print(_("Inserting %d%s frames from the clipboard..."), remainder_frames,
            times_to_insert > 1. ? " remainder" : "");

    com = lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %.3f %d %d %d %d %d",
                              prefs->backend, cfile->handle,
                              get_image_ext_for_type(cfile->img_type), where, clipboard->frames - remainder_frames + 1,
                              end, clipboard->handle, with_sound, cfile->frames, hsize, vsize, cfile->fps,
                              cfile->arate, cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                              !(cfile->signed_endian & AFORM_BIG_ENDIAN));

    lives_rm(cfile->info_file);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      d_print_failed();
      unbuffer_lmap_errors(FALSE);
      return;
    }

    cfile->progress_start = 1;
    cfile->progress_end = remainder_frames;

    do_progress_dialog(TRUE, FALSE, _("Inserting"));

    if (mainw->error) {
      d_print_failed();
      unbuffer_lmap_errors(FALSE);
      return;
    }

    if (cfile->clip_type == CLIP_TYPE_FILE || virtual_ins) {
      insert_images_in_virtual(mainw->current_file, where, remainder_frames, clipboard->frame_index,
                               clipboard->frames - remainder_frames + 1);
    }

    cfile->frames += remainder_frames;
    where += remainder_frames;

    cfile->insert_end += remainder_frames;

    if (!mainw->insert_after) {
      cfile->start += remainder_frames;
      cfile->end += remainder_frames;
    }

    if (with_sound) {
      reget_afilesize(mainw->current_file);
    } else get_play_times();
    d_print_done();
  }

  // inserts of whole clipboard
  if ((int)times_to_insert > 1) {
    d_print("");
    d_print(_("Inserting %d times from the clipboard%s..."), (int)times_to_insert, with_sound ?
            " (with sound)" : "");
  } else if ((int)times_to_insert > 0) {
    d_print("");
    d_print(_("Inserting %d frames from the clipboard%s..."), cb_end - cb_start + 1, with_sound ?
            " (with sound)" : "");
  }

  if (virtual_ins) cb_end = -cb_end;

  // for an insert after a merge we set our start posn. -ve
  // this should indicate to the back end to leave our
  // backup frames alone

  com = lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %.3f %d %d %d %d %d %d",
                            prefs->backend, cfile->handle,
                            get_image_ext_for_type(cfile->img_type), where, cb_start * leave_backup, cb_end,
                            clipboard->handle, with_sound, cfile->frames, hsize, vsize, cfile->fps, cfile->arate,
                            cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                            !(cfile->signed_endian & AFORM_BIG_ENDIAN), (int)times_to_insert);

  if (virtual_ins) cb_end = -cb_end;

  cfile->progress_start = 1;
  cfile->progress_end = (cb_end - cb_start + 1) * (int)times_to_insert + cfile->frames - where;
  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    unbuffer_lmap_errors(FALSE);
    return;
  }

  // show a progress dialog
  cfile->nopreview = TRUE;
  if (!do_progress_dialog(TRUE, TRUE, _("Inserting"))) {
    // cancelled

    cfile->nopreview = FALSE;

    if (mainw->error) {
      d_print_failed();
      unbuffer_lmap_errors(FALSE);
      return;
    }

    // clean up moved/inserted frames
    com = lives_strdup_printf("%s undo_insert \"%s\" %d %d %d \"%s\"",
                              prefs->backend, cfile->handle, where + 1,
                              where + (cb_end - cb_start + 1) * (int)times_to_insert, cfile->frames,
                              get_image_ext_for_type(cfile->img_type));
    lives_system(com, FALSE);
    lives_free(com);

    do_progress_dialog(TRUE, FALSE, _("Cancelling"));

    cfile->start = ostart;
    cfile->end = oend;

    if (with_sound) {
      // desample clipboard audio
      if (cb_audio_change && !prefs->conserve_space) {
        lives_rm(clipboard->info_file);
        com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, clipboard->handle);
        mainw->current_file = 0;
        lives_system(com, FALSE);
        lives_free(com);
        mainw->current_file = current_file;
        clipboard->arps = ocarps;
        reget_afilesize(0);
      }
    }

    if (cb_video_change) {
      // desample clipboard video
      mainw->current_file = 0;
      mainw->no_switch_dprint = TRUE;
      on_undo_activate(NULL, NULL);
      mainw->no_switch_dprint = FALSE;
      mainw->current_file = current_file;
    }

    switch_clip(1, current_file, TRUE);
    set_undoable(NULL, FALSE);
    mainw->cancelled = CANCEL_USER;
    unbuffer_lmap_errors(FALSE);
    return;
  }

  mainw->cancelled = CANCEL_NONE;
  cfile->nopreview = FALSE;

  if (cfile->clip_type == CLIP_TYPE_FILE || virtual_ins) {
    insert_images_in_virtual(mainw->current_file, where, (cb_end - cb_start + 1) * (int)times_to_insert, clipboard->frame_index,
                             cb_start * leave_backup);
  }

  cfile->frames += (cb_end - cb_start + 1) * (int)times_to_insert;
  where += (cb_end - cb_start + 1) * (int)times_to_insert;
  cfile->insert_end += (cb_end - cb_start + 1) * (int)times_to_insert;

  if (!mainw->insert_after) {
    cfile->start += (cb_end - cb_start + 1) * (int)times_to_insert;
    cfile->end += (cb_end - cb_start + 1) * (int)times_to_insert;
  }

  if (with_sound == 1) {
    reget_afilesize(mainw->current_file);
  } else get_play_times();
  d_print_done();

  // last remainder frames

  if (mainw->insert_after && remainder_frames > 0) {
    d_print(_("Inserting %d%s frames from the clipboard..."), remainder_frames,
            times_to_insert > 1. ? " remainder" : "");

    if (virtual_ins) remainder_frames = -remainder_frames;

    com = lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %3f %d %d %d %d %d",
                              prefs->backend, cfile->handle,
                              get_image_ext_for_type(cfile->img_type), where, 1, remainder_frames, clipboard->handle,
                              with_sound, cfile->frames, hsize, vsize, cfile->fps, cfile->arate, cfile->achans,
                              cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                              !(cfile->signed_endian & AFORM_BIG_ENDIAN));

    lives_rm(cfile->info_file);
    lives_system(com, FALSE);

    if (THREADVAR(com_failed)) {
      unbuffer_lmap_errors(TRUE);
      d_print_failed();
      return;
    }

    if (virtual_ins) remainder_frames = -remainder_frames;

    cfile->progress_start = 1;
    cfile->progress_end = remainder_frames;

    do_progress_dialog(TRUE, FALSE, _("Inserting"));

    if (mainw->error) {
      d_print_failed();
      unbuffer_lmap_errors(TRUE);
      return;
    }

    if (cfile->clip_type == CLIP_TYPE_FILE || virtual_ins) {
      insert_images_in_virtual(mainw->current_file, where, remainder_frames, clipboard->frame_index, 1);
    }

    cfile->frames += remainder_frames;
    cfile->insert_end += remainder_frames;
    lives_free(com);

    if (!mainw->insert_after) {
      cfile->start += remainder_frames;
      cfile->end += remainder_frames;
    }
    get_play_times();

    d_print_done();
  }

  // if we had deferred audio, we insert silence in selection
  if (insert_silence) {
    cfile->undo1_dbl = calc_time_from_frame(mainw->current_file, cfile->insert_start);
    cfile->undo2_dbl = calc_time_from_frame(mainw->current_file, cfile->insert_end + 1);
    cfile->undo_arate = cfile->arate;
    cfile->undo_signed_endian = cfile->signed_endian;
    cfile->undo_achans = cfile->achans;
    cfile->undo_asampsize = cfile->asampsize;
    cfile->undo_arps = cfile->arps;

    on_ins_silence_activate(NULL, NULL);

    with_sound = TRUE;
  }

  // insert done

  // start or end can be zero if we inserted into pure audio
  if (cfile->start == 0 && cfile->frames > 0) cfile->start = 1;
  if (cfile->end == 0) cfile->end = cfile->frames;

  if (cfile->frames > 0 && orig_frames == 0) {
    lives_snprintf(cfile->type, 40, "Frames");
    cfile->orig_file_name = FALSE;
    cfile->hsize = clipboard->hsize;
    cfile->vsize = clipboard->vsize;
    cfile->bpp = clipboard->bpp;
    cfile->fps = cfile->pb_fps = clipboard->fps;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_BPP, &cfile->bpp)) bad_header = TRUE;
    if (cfile->clip_type == CLIP_TYPE_FILE && cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      double dfps = (double)cdata->fps;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &dfps)) bad_header = TRUE;
    } else {
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps)) bad_header = TRUE;
    }
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->fps)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->frames == 0 ? 0 : 1, cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end);
  lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);

  lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->frames == 0 ? 0 : 1, cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start);
  lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);

  set_undoable(_("Insert"), TRUE);
  cfile->undo1_boolean = with_sound;
  lives_widget_set_sensitive(mainw->select_new, TRUE);

  // mark new file size as 'Unknown'
  cfile->f_size = 0l;
  cfile->changed = TRUE;

  if (with_sound) {
    cfile->undo_action = UNDO_INSERT_WITH_AUDIO;
    if (cb_audio_change && !prefs->conserve_space && clipboard->achans > 0) {
      lives_rm(clipboard->info_file);
      mainw->current_file = 0;
      com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, clipboard->handle);
      lives_system(com, FALSE);
      lives_free(com);
      mainw->current_file = current_file;
      clipboard->arps = ocarps;
      reget_afilesize(0);
    }
  } else cfile->undo_action = UNDO_INSERT;

  if (cb_video_change) {
    mainw->current_file = 0;
    mainw->no_switch_dprint = TRUE;
    on_undo_activate(NULL, NULL);
    mainw->no_switch_dprint = FALSE;
    mainw->current_file = current_file;
  }

  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames)) bad_header = TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);

  switch_clip(1, current_file, TRUE);
  mainw->error = FALSE;

  if (mainw->sl_undo_mem != NULL && (cfile->stored_layout_frame != 0 || (with_sound && cfile->stored_layout_audio != 0.))) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}

/**
   @brief check for layout errors, using in_mask as a guide
   (mask values are taken from prefs->warn_mask, but with opposite sense)

   This function should ALWAYS be called before any operations are performed which may do any of the following:
   - delete frames, shift frames, alter frames (e.g. insert, delete, resample, apply rendered effects).
   - delete audio, shift audio, alter audio (e.g. insert / delete with audio, resample, adjust the volume)
   - (deletion includes closing the clip)

   - some operations are exempt (i.e resizing frames, converting the frame / sample format, appending after the end, temporary
   changes which affect only playback such as applying real time effects, letterboxing, altering the playback rate)

   -- changing clip audio volume is somewhat undefined as this is a new feature and can be both a playback change and / or
   permanent change. However fade in / out and insert silence are more permanent and should call this function.

   - the order of priority for both frames and audio is always: delete > shift > alter
     the default settings are to warn on delete / shift and not to warn on alter; however the user preferences may override this

   start and end represent frame values for the affected region.
   For audio, the values should approximate the start and end points,
   (though normally they would be the correct points) and end may extend beyond the actual frame count.

   After checking, in_mask is set (reduced) to to any 'trangressions' found - this will be the intersection (AND) of the check_mask,
   AND detected transgressions AND the conjunction of non-disabled warnings.

   if the resulting set is non-empty, then prior to returning, a warning dialog is displayed.
   'operation' is a descriptive word / phrase for what the operation intends to be doing, e.g "deletion", "cutting", "pasting" which
   used in the dialog message.

   if the user cancels, FALSE is returned. and the caller should abort whatever operation.

   if the user chooses to continue, warnings are added to the layout errors buffer am TRUE is returned.
   (within the buffer, errors of the same type are collated and organised by priority,
   so there is no harm in calling this function multiple times).

   If no warnings are shown (either because there were no conflicts, or because the user disabled the warnings) then TRUE is also
   returned, and in this case chk_mask will be 0.

   After return from this function, if the return value is TRUE the operation may proceed, but the following must be observed:

   - If the operation fails or is cancelled. any buffered warnings should be cleared by calling:  unbuffer_lmap_errors(FALSE);
   (this function may always be called, even if chk_mask was returned as 0)

   Otherwise, after the operation completes something like the following must be done:

   if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

   this latter function must be called if and only if chk_mask was returned with a non-zero value.
*/
boolean check_for_layout_errors(const char *operation, int fileno, int start, int end, uint32_t *in_mask) {
  lives_clip_t *sfile;
  LiVESList *xlays = NULL;
  uint32_t ret_mask = 0, mask = *in_mask;
  boolean cancelled = FALSE;

  if (!IS_VALID_CLIP(fileno)) return 0;
  sfile = mainw->files[fileno];
  if (start < 1) start = 1;

  if (mask & WARN_MASK_LAYOUT_DELETE_FRAMES) {
    if ((xlays = layout_frame_is_affected(fileno, start, end, NULL)) != NULL) {
      if (sfile->tcache_dubious_from > 0) free_thumb_cache(fileno, sfile->tcache_dubious_from);
      sfile->tcache_dubious_from = start;
      ret_mask |= WARN_MASK_LAYOUT_DELETE_FRAMES | (mask & WARN_MASK_LAYOUT_SHIFT_FRAMES) | (mask & WARN_MASK_LAYOUT_ALTER_FRAMES);
      if ((prefs->warning_mask & WARN_MASK_LAYOUT_DELETE_FRAMES) == 0) {
        mainw->xlays = xlays;
        if (!do_warning_dialogf
            (_("%s will cause missing frames in some multitrack layouts.\nAre you sure you wish to continue ?\n"), operation)) {
          cancelled = TRUE;
        }
      }

      if (!cancelled) {
        buffer_lmap_error(LMAP_ERROR_DELETE_FRAMES, sfile->name, (livespointer)sfile->layout_map, fileno,
                          start, 0., count_resampled_frames(sfile->stored_layout_frame,
                              sfile->stored_layout_fps, sfile->fps) >= start);
      }
      lives_list_free_all(&xlays);
    }
  }

  if (mask & WARN_MASK_LAYOUT_DELETE_AUDIO) {
    if ((xlays = layout_audio_is_affected(fileno, (start - 1.) / sfile->fps, (end - 1.) / sfile->fps, NULL)) != NULL) {
      ret_mask |= WARN_MASK_LAYOUT_DELETE_AUDIO | (mask & WARN_MASK_LAYOUT_SHIFT_AUDIO)
                  | (mask & WARN_MASK_LAYOUT_ALTER_AUDIO);
      if (!cancelled) {
        if ((prefs->warning_mask & WARN_MASK_LAYOUT_DELETE_AUDIO) == 0) {
          mainw->xlays = xlays;
          if (!do_warning_dialogf
              (_("%s will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"), operation)) {
            cancelled = TRUE;
          }
        }
        if (!cancelled) {
          buffer_lmap_error(LMAP_ERROR_DELETE_AUDIO, sfile->name, (livespointer)sfile->layout_map, fileno, 0,
                            (start - 1.) / sfile->fps, (start - 1.) /
                            sfile->fps < sfile->stored_layout_audio);
        }
      }
    }
    lives_list_free_all(&xlays);
  }

  if ((ret_mask & WARN_MASK_LAYOUT_DELETE_FRAMES) == 0) {
    if (mask & WARN_MASK_LAYOUT_SHIFT_FRAMES) {
      if ((xlays = layout_frame_is_affected(fileno, start, 0, NULL)) != NULL) {
        if (sfile->tcache_dubious_from > 0) free_thumb_cache(fileno, sfile->tcache_dubious_from);
        sfile->tcache_dubious_from = start;
        ret_mask |= WARN_MASK_LAYOUT_SHIFT_FRAMES | (mask & WARN_MASK_LAYOUT_ALTER_FRAMES);
        if ((prefs->warning_mask & WARN_MASK_LAYOUT_SHIFT_FRAMES) == 0) {
          mainw->xlays = xlays;
          if (!do_warning_dialogf
              (_("%s will cause frames to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"), operation)) {
            cancelled = TRUE;
          }
        }
        if (!cancelled) {
          buffer_lmap_error(LMAP_ERROR_SHIFT_FRAMES, sfile->name, (livespointer)sfile->layout_map, fileno,
                            start, 0., start <= count_resampled_frames(sfile->stored_layout_frame,
                                sfile->stored_layout_fps, sfile->fps));
        }
      }
      lives_list_free_all(&xlays);
    }
  }

  if ((ret_mask & WARN_MASK_LAYOUT_DELETE_AUDIO) == 0) {
    if (mask & WARN_MASK_LAYOUT_SHIFT_AUDIO) {
      if ((xlays = layout_audio_is_affected(fileno, (start - 1.) / sfile->fps, 0., NULL)) != NULL) {
        ret_mask |= WARN_MASK_LAYOUT_SHIFT_AUDIO | (mask & WARN_MASK_LAYOUT_ALTER_AUDIO);
        if (!cancelled) {
          if ((prefs->warning_mask & WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
            mainw->xlays = xlays;
            if (!do_warning_dialogf
                (_("%s will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"), operation)) {
              cancelled = TRUE;
            }
          }
          if (!cancelled) {
            buffer_lmap_error(LMAP_ERROR_SHIFT_AUDIO, sfile->name, (livespointer)sfile->layout_map, fileno, 0,
                              (start - 1.) / sfile->fps, (start - 1.) / sfile->fps <= sfile->stored_layout_audio);
          }
        }
        lives_list_free_all(&xlays);
      }
    }
  }

  if ((ret_mask & (WARN_MASK_LAYOUT_DELETE_FRAMES | WARN_MASK_LAYOUT_SHIFT_FRAMES)) == 0) {
    if (mask & WARN_MASK_LAYOUT_ALTER_FRAMES) {
      if ((xlays = layout_frame_is_affected(fileno, start, end, NULL)) != NULL) {
        if (sfile->tcache_dubious_from > 0) free_thumb_cache(fileno, sfile->tcache_dubious_from);
        sfile->tcache_dubious_from = start;
        ret_mask |= WARN_MASK_LAYOUT_ALTER_FRAMES;
        if ((prefs->warning_mask & WARN_MASK_LAYOUT_ALTER_FRAMES) == 0) {
          mainw->xlays = xlays;
          if (!do_layout_alter_frames_warning()) {
            cancelled = TRUE;
          }
        }
        if (!cancelled) {
          buffer_lmap_error(LMAP_ERROR_ALTER_FRAMES, sfile->name, (livespointer)sfile->layout_map, fileno, 0, 0.,
                            sfile->stored_layout_frame > 0);
        }
        lives_list_free_all(&xlays);
      }
    }
  }

  if ((ret_mask & (WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_SHIFT_AUDIO)) == 0) {
    if (mask & WARN_MASK_LAYOUT_ALTER_AUDIO) {
      if ((xlays = layout_audio_is_affected(fileno, 0., 0., NULL)) != NULL) {
        ret_mask |= WARN_MASK_LAYOUT_ALTER_AUDIO;
        if (!cancelled) {
          if ((prefs->warning_mask & WARN_MASK_LAYOUT_ALTER_AUDIO) == 0) {
            mainw->xlays = xlays;
            if (!do_layout_alter_audio_warning()) {
              cancelled = TRUE;
            }
          }
        }
        if (!cancelled) {
          buffer_lmap_error(LMAP_ERROR_ALTER_AUDIO, sfile->name, (livespointer)sfile->layout_map, fileno, 0, 0.,
                            sfile->stored_layout_audio > 0.);
        }
        lives_list_free_all(&xlays);
      }
    }
  }

  mainw->xlays = NULL;
  *in_mask = ret_mask;
  return !cancelled;
}


void on_delete_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *com;

  boolean bad_header = FALSE;

  uint32_t chk_mask = 0;

  int frames_cut = cfile->end - cfile->start + 1;
  int start = cfile->start;
  int end = cfile->end;

  // occasionally we get a keyboard misread, so this should prevent that
  if (LIVES_IS_PLAYING) return;

  if (cfile->start <= 1 && cfile->end == cfile->frames) {
    if (!mainw->osc_auto && menuitem != LIVES_MENU_ITEM(mainw->cut) && (cfile->achans == 0 ||
        ((cfile->end - 1.) / cfile->fps >= cfile->laudio_time &&
         mainw->ccpd_with_sound))) {
      if (do_warning_dialog
          (_("\nDeleting all frames will close this file.\nAre you sure ?"))) close_current_file(0);
      return;
    }
  }

  if (menuitem != NULL) {
    char *tmp = (_("Deletion"));
    chk_mask = WARN_MASK_LAYOUT_DELETE_FRAMES | WARN_MASK_LAYOUT_SHIFT_FRAMES | WARN_MASK_LAYOUT_ALTER_FRAMES;
    if (mainw->ccpd_with_sound) chk_mask |= WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_SHIFT_AUDIO |
                                              WARN_MASK_LAYOUT_ALTER_AUDIO;
    if (!check_for_layout_errors(tmp, mainw->current_file, cfile->start, cfile->end, &chk_mask)) {
      lives_free(tmp);
      return;
    }
    lives_free(tmp);
  }

  if (cfile->start <= 1 && cfile->end == cfile->frames) {
    cfile->ohsize = cfile->hsize;
    cfile->ovsize = cfile->vsize;
  }

  cfile->undo_start = cfile->start;
  cfile->undo_end = cfile->end;
  cfile->undo1_boolean = mainw->ccpd_with_sound;

  if (menuitem != NULL || mainw->osc_auto) {
    d_print(""); // force switchtext
    d_print(_("Deleting frames %d to %d%s..."), cfile->start, cfile->end,
            mainw->ccpd_with_sound && cfile->achans > 0 ? " (with sound)" : "");
  }

  com = lives_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
                            prefs->backend, cfile->handle, cfile->start, cfile->end,
                            mainw->ccpd_with_sound, cfile->frames, get_image_ext_for_type(cfile->img_type),
                            cfile->fps, cfile->arate, cfile->achans, cfile->asampsize);
  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    unbuffer_lmap_errors(FALSE);
    d_print_failed();
    return;
  }

  cfile->progress_start = cfile->start;
  cfile->progress_end = cfile->frames;

  // show a progress dialog, not cancellable
  do_progress_dialog(TRUE, FALSE, _("Deleting"));

  if (cfile->clip_type == CLIP_TYPE_FILE) {
    delete_frames_from_virtual(mainw->current_file, cfile->start, cfile->end);
  }

  cfile->frames -= frames_cut;

  cfile->undo_arate = cfile->arate;
  cfile->undo_signed_endian = cfile->signed_endian;
  cfile->undo_achans = cfile->achans;
  cfile->undo_asampsize = cfile->asampsize;
  cfile->undo_arps = cfile->arps;

  if (mainw->ccpd_with_sound) {
    reget_afilesize(mainw->current_file);
  } else get_play_times();

  if (cfile->frames == 0) {
    if (cfile->afilesize == 0l) {
      close_current_file(0);
      return;
    }
    lives_snprintf(cfile->type, 40, "Audio");
    cfile->hsize = cfile->vsize = 0;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
    cfile->orig_file_name = FALSE;
    desensitize();
    sensitize();
  }

  if (!mainw->selwidth_locked || cfile->start > cfile->frames) {
    if (--start == 0 && cfile->frames > 0) {
      start = 1;
    }
  }

  cfile->start = start;

  if (!mainw->selwidth_locked) {
    cfile->end = start;
  } else {
    cfile->end = end;
    if (cfile->end > cfile->frames) {
      cfile->end = cfile->frames;
    }
  }

  lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->frames == 0 ? 0 : 1, cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end);
  lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);

  lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->frames == 0 ? 0 : 1, cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start);
  lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);

  // menuitem is NULL if we came here from undo_insert
  if (menuitem == NULL && !mainw->osc_auto) return;

  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames)) bad_header = TRUE;

  showclipimgs();

  get_play_times();

  if (bad_header) do_header_write_error(mainw->current_file);

  // mark new file size as 'Unknown'
  cfile->f_size = 0l;
  cfile->changed = TRUE;

  set_undoable(_("Delete"), TRUE);
  cfile->undo_action = UNDO_DELETE;
  d_print_done();

  if (mainw->sl_undo_mem != NULL && (cfile->stored_layout_frame != 0 || (mainw->ccpd_with_sound &&
                                     cfile->stored_layout_audio != 0.))) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}


void on_select_all_activate(LiVESWidget * widget, livespointer user_data) {
  if (!CURRENT_CLIP_IS_VALID) return;

  if (mainw->selwidth_locked) {
    widget_opts.non_modal = TRUE;
    if (widget != NULL) do_error_dialog(_("\n\nSelection is locked.\n"));
    widget_opts.non_modal = FALSE;
    return;
  }

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 1);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->frames);

  cfile->start = cfile->frames > 0 ? 1 : 0;
  cfile->end = cfile->frames;

  get_play_times();

  showclipimgs();
}


void on_select_start_only_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (mainw->current_file == -1) return;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->start);
}


void on_select_end_only_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (mainw->current_file == -1) return;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->end);
}


void on_select_invert_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (cfile->start == 1) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->end + 1);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->frames);
  } else {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->start - 1);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 1);
  }

  get_play_times();

  showclipimgs();
}


void on_select_last_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (cfile->undo_start > cfile->frames) cfile->undo_start = cfile->frames;
  if (cfile->undo_end > cfile->frames) cfile->undo_end = cfile->frames;

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->undo_start);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->undo_end);

  cfile->start = cfile->undo_start;
  cfile->end = cfile->undo_end;

  get_play_times();

  showclipimgs();
}


void on_select_new_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (cfile->insert_start > cfile->frames) cfile->insert_start = cfile->frames;
  if (cfile->insert_end > cfile->frames) cfile->insert_end = cfile->frames;

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->insert_start);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->insert_end);

  cfile->start = cfile->insert_start;
  cfile->end = cfile->insert_end;

  get_play_times();

  showclipimgs();
}


void on_select_to_end_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->frames);
  cfile->end = cfile->frames;
  get_play_times();
  load_end_image(cfile->end);
}


void on_select_to_aend_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  int end = calc_frame_from_time4(mainw->current_file, cfile->laudio_time);
  if (end > cfile->frames) end = cfile->frames;
  if (end < cfile->start) end = cfile->start;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), end);
  cfile->end = end;
  get_play_times();
  load_end_image(cfile->end);
}


void on_select_from_start_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 1);
  cfile->start = cfile->frames > 0 ? 1 : 0;
  get_play_times();
  load_start_image(cfile->start);
}


void on_lock_selwidth_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  mainw->selwidth_locked = !mainw->selwidth_locked;
  lives_widget_set_sensitive(mainw->select_submenu, !mainw->selwidth_locked);
  update_sel_menu();
}


void play_all(boolean from_menu) {
  if (!CURRENT_CLIP_IS_VALID || CURRENT_CLIP_IS_CLIPBOARD) return;

  if (mainw->multitrack != NULL) {
    if (!LIVES_IS_PLAYING) {
      if (!mainw->multitrack->playing_sel) multitrack_playall(mainw->multitrack);
      else multitrack_play_sel(NULL, mainw->multitrack);
    } else on_pause_clicked();
    return;
  }

  if (!LIVES_IS_PLAYING) {
    if (mainw->proc_ptr != NULL && from_menu) {
      on_preview_clicked(LIVES_BUTTON(mainw->proc_ptr->preview_button), NULL);
      return;
    }

    if (!mainw->osc_auto) {
      if (cfile->frames > 0) {
        mainw->play_start = calc_frame_from_time(mainw->current_file,
                            cfile->pointer_time);
      } else {
        mainw->play_start = calc_frame_from_time4(mainw->current_file,
                            cfile->pointer_time);  // real_pointer_time ???
      }
      mainw->play_end = cfile->frames;
    }

    mainw->playing_sel = FALSE;
    if (CURRENT_CLIP_IS_NORMAL) lives_rm(cfile->info_file);

    play_file();

    /* if (CURRENT_CLIP_IS_VALID) { */
    /*   if (1 || !cfile->play_paused) { */
    /*     //cfile->pointer_time = (cfile->last_frameno - 1.) / cfile->fps; */
    /*     lives_ce_update_timeline(0, cfile->real_pointer_time); */
    /*   } else { */
    /* 	lives_ce_update_timeline(cfile->frameno, 0); */
    /*     //cfile->pointer_time = (cfile->last_frameno - 1.) / cfile->fps; */
    /*     //cfile->play_paused = TRUE; */
    /*     mainw->cancelled = CANCEL_USER; */
    /*   } */
    //}
  }
}


void on_playall_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (menuitem != NULL && mainw->go_away) return;
  start_playback(menuitem ? 8 : 0);
}


void play_sel(void) {
  if (!mainw->is_rendering) {
    mainw->play_start = cfile->start;
    mainw->play_end = cfile->end;
    mainw->clip_switched = FALSE;
  }

  if (!mainw->preview) {
    int orig_play_frame = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
    if (orig_play_frame > mainw->play_start && orig_play_frame < mainw->play_end) {
      mainw->play_start = orig_play_frame;
    }
  }

  mainw->playing_sel = TRUE;

  play_file();

  mainw->playing_sel = FALSE;
  lives_ce_update_timeline(0, cfile->real_pointer_time);

  // in case we are rendering and previewing, in case we now have audio
  if (mainw->preview && mainw->is_rendering && mainw->is_processing) reget_afilesize(mainw->current_file);
  if (mainw->cancelled == CANCEL_AUDIO_ERROR) {
    handle_audio_timeout();
    mainw->cancelled = CANCEL_ERROR;
  }
}


void on_playsel_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // play part of a clip (in clip editor)
  if (!CURRENT_CLIP_IS_VALID || CURRENT_CLIP_IS_CLIPBOARD) return;

  if (mainw->proc_ptr && menuitem) {
    on_preview_clicked(LIVES_BUTTON(mainw->proc_ptr->preview_button), NULL);
    return;
  }
  if (LIVES_POINTER_TO_INT(user_data)) play_file();
  else start_playback(1);
}


void on_playclip_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // play the clipboard
  int current_file;
  if (mainw->multitrack != NULL) return;

  current_file = mainw->pre_play_file = mainw->current_file;
  mainw-> oloop = mainw->loop;
  mainw->oloop_cont = mainw->loop_cont;

  // switch to the clipboard
  switch_to_file(current_file, 0);
  lives_widget_set_sensitive(mainw->loop_video, FALSE);
  lives_widget_set_sensitive(mainw->loop_continue, FALSE);
  mainw->loop = mainw->loop_cont = FALSE;

  mainw->play_start = 1;
  mainw->play_end = clipboard->frames;
  mainw->playing_sel = FALSE;
  mainw->loop = FALSE;

  lives_rm(cfile->info_file);

  start_playback(5);
}


void on_record_perf_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // real time recording

  if (mainw->multitrack) return;

  if (LIVES_IS_PLAYING) {
    // we are playing a clip
    if (!mainw->record || mainw->record_paused) {
      // recording is starting
      mainw->record_starting = TRUE;

      toggle_record();

      if ((prefs->rec_opts & REC_AUDIO) && (mainw->agen_key != 0 || mainw->agen_needs_reinit
                                            || prefs->audio_src == AUDIO_SRC_EXT) &&
          ((prefs->audio_player == AUD_PLAYER_JACK) ||
           (prefs->audio_player == AUD_PLAYER_PULSE))) {
        if (mainw->ascrap_file == -1) {
          open_ascrap_file();
        }
        if (mainw->ascrap_file != -1) {
          mainw->rec_samples = -1; // record unlimited
          mainw->rec_aclip = mainw->ascrap_file;
          mainw->rec_avel = 1.;
          mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
                             (double)(mainw->files[mainw->ascrap_file]->arps * mainw->files[mainw->ascrap_file]->achans *
                                      mainw->files[mainw->ascrap_file]->asampsize >> 3);

#ifdef ENABLE_JACK
          if (prefs->audio_player == AUD_PLAYER_JACK) {
            char *lives_header = lives_build_filename(prefs->workdir, mainw->files[mainw->ascrap_file]->handle,
                                 LIVES_CLIP_HEADER, NULL);
            mainw->clip_header = fopen(lives_header, "w"); // speed up clip header writes
            lives_free(lives_header);

            if (mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
              jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
              mainw->jackd_read->is_paused = FALSE;
              mainw->jackd_read->in_use = TRUE;
            } else {
              if (mainw->jackd) {
                jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
              }
            }
            if (mainw->clip_header) fclose(mainw->clip_header);
            mainw->clip_header = NULL;
          }

#endif
#ifdef HAVE_PULSE_AUDIO
          if (prefs->audio_player == AUD_PLAYER_PULSE) {
            char *lives_header = lives_build_filename(prefs->workdir, mainw->files[mainw->ascrap_file]->handle,
                                 LIVES_CLIP_HEADER, NULL);
            mainw->clip_header = fopen(lives_header, "w"); // speed up clip header writes
            lives_free(lives_header);

            if (mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
              pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
              mainw->pulsed_read->is_paused = FALSE;
              mainw->pulsed_read->in_use = TRUE;
            } else {
              if (mainw->pulsed) {
                pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
              }
            }
            if (mainw->clip_header) fclose(mainw->clip_header);
            mainw->clip_header = NULL;
          }
#endif
        }
        return;
      }

      if (prefs->rec_opts & REC_AUDIO) {
        // recording INTERNAL audio
#ifdef ENABLE_JACK
        if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) {
          jack_get_rec_avals(mainw->jackd);
        }
#endif
#ifdef HAVE_PULSE_AUDIO
        if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
          pulse_get_rec_avals(mainw->pulsed);
        }
#endif
      }
      return;
    }

    // end record during playback
    event_list_add_end_events(mainw->event_list, FALSE);
    mainw->record_paused = TRUE; // pause recording of further events
    enable_record();
    return;
  }

  // out of playback

  // record performance
  if (!mainw->record) {
    // TODO - change message depending on rec_opts
    d_print(_("Ready to record. Use 'control' and cursor keys during playback to record your performance.\n"
              "(To cancel, press 'r' or click on Play|Record Performance again before you play.)\n"));
    mainw->record = TRUE;
    toggle_record();
    get_play_times();
  } else {
    d_print(_("Record cancelled.\n"));
    enable_record();
    mainw->record = FALSE;
  }
}


boolean record_toggle_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                               livespointer user_data) {
  // from osc
  boolean start = (boolean)LIVES_POINTER_TO_INT(user_data);

  if ((start && (!mainw->record || mainw->record_paused)) || (!start && (mainw->record && !mainw->record_paused)))
    on_record_perf_activate(NULL, NULL);

  return TRUE;
}


void on_rewind_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (LIVES_IS_PLAYING) return;

  if (mainw->multitrack != NULL) {
    mt_tl_move(mainw->multitrack, 0.);
    return;
  }

  cfile->pointer_time = lives_ce_update_timeline(0, 0.);
  lives_widget_queue_draw_if_visible(mainw->hruler);
  lives_widget_set_sensitive(mainw->rewind, FALSE);
  lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
}


void on_stop_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // stop during playback

  if (mainw->multitrack && mainw->multitrack->is_paused && !LIVES_IS_PLAYING) {
    mainw->multitrack->is_paused = FALSE;
    mainw->multitrack->playing_sel = FALSE;
    mt_tl_move(mainw->multitrack, mainw->multitrack->pb_unpaused_start_time);
    lives_widget_set_sensitive(mainw->stop, FALSE);
    lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);
    return;
  }

  mainw->cancelled = CANCEL_USER;
}


boolean on_stop_activate_by_del(LiVESWidget * widget, LiVESXEventDelete * event, livespointer user_data) {
  // called if the user closes the separate play window
  if (prefs->sepwin_type == SEPWIN_TYPE_STICKY) {
    on_sepwin_pressed(NULL, NULL);
  }
  return TRUE;
}


void on_pause_clicked(void) {
  mainw->jack_can_stop = FALSE;
  mainw->cancelled = CANCEL_USER_PAUSED;
}


void on_encoder_entry_changed(LiVESCombo * combo, livespointer ptr) {
  LiVESList *encoder_capabilities = NULL;
  LiVESList *ofmt_all = NULL;
  LiVESList *ofmt = NULL;

  char *new_encoder_name = lives_combo_get_active_text(combo);
  char *msg;
  char **array;
  int i;
  render_details *rdet = (render_details *)ptr;
  LiVESList *dummy_list;

  if (!strlen(new_encoder_name)) {
    lives_free(new_encoder_name);
    return;
  }

  if (!strcmp(new_encoder_name, mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) {
    LiVESList *ofmt = NULL;
    ofmt = lives_list_append(ofmt, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

    lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
    // ---
    lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    // ---
    lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);

    lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);
    lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo), mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);

    lives_list_free(ofmt);
    if (prefs->acodec_list != NULL) {
      lives_list_free_all(&prefs->acodec_list);
    }
    prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);

    lives_combo_set_active_string(LIVES_COMBO(rdet->acodec_combo), mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);

    lives_free(new_encoder_name);

    rdet->enc_changed = FALSE;

    return;
  }

  // finalise old plugin
  plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "finalise");

  clear_mainw_msg();
  // initialise new plugin
  if ((dummy_list = plugin_request(PLUGIN_ENCODERS, new_encoder_name, "init")) == NULL) {
    if (*mainw->msg) {
      msg = lives_strdup_printf(_("\n\nThe '%s' plugin reports:\n%s\n"), new_encoder_name, mainw->msg);
    } else {
      msg = lives_strdup_printf
            (_("\n\nUnable to find the 'init' method in the %s plugin.\nThe plugin may be broken or not installed correctly."),
             new_encoder_name);
    }
    lives_free(new_encoder_name);

    if (mainw->is_ready) {
      do_error_dialog_with_check(msg, 0);
    }

    lives_free(msg);

    if (prefsw != NULL) {
      lives_signal_handler_block(prefsw->encoder_combo, prefsw->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      lives_signal_handler_unblock(prefsw->encoder_combo, prefsw->encoder_name_fn);
    }

    if (rdet != NULL) {
      lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
      // ---
      lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);
    }

    dummy_list = plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "init");
    lives_list_free_all(&dummy_list);
    return;
  }
  lives_list_free_all(&dummy_list);

  lives_snprintf(future_prefs->encoder.name, 64, "%s", new_encoder_name);
  lives_free(new_encoder_name);

  if ((encoder_capabilities = plugin_request(PLUGIN_ENCODERS, future_prefs->encoder.name, "get_capabilities")) == NULL) {
    do_plugin_encoder_error(future_prefs->encoder.name);

    if (prefsw != NULL) {
      lives_signal_handler_block(prefsw->encoder_combo, prefsw->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      lives_signal_handler_unblock(prefsw->encoder_combo, prefsw->encoder_name_fn);
    }

    if (rdet != NULL) {
      lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
      // ---
      lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);
    }

    plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "init");
    lives_snprintf(future_prefs->encoder.name, 64, "%s", prefs->encoder.name);
    return;
  }
  prefs->encoder.capabilities = atoi((char *)lives_list_nth_data(encoder_capabilities, 0));
  lives_list_free_all(&encoder_capabilities);

  // fill list with new formats
  if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, future_prefs->encoder.name, "get_formats")) != NULL) {
    for (i = 0; i < lives_list_length(ofmt_all); i++) {
      if (get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|') > 2) {
        array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);
        ofmt = lives_list_append(ofmt, lives_strdup(array[1]));
        lives_strfreev(array);
      }
    }

    if (prefsw != NULL) {
      // we have to block here, otherwise on_ofmt_changed gets called for every added entry !
      lives_signal_handler_block(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);

      lives_combo_populate(LIVES_COMBO(prefsw->ofmt_combo), ofmt);

      lives_signal_handler_unblock(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
    }

    if (rdet != NULL) {
      // we have to block here, otherwise on_ofmt_changed gets called for every added entry !
      lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);

      lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);

      lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    }

    lives_list_free(ofmt);

    // set default (first) output type
    array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, 0), "|", -1);

    if (rdet != NULL) {
      lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo), array[1]);

      if (prefsw == NULL && strcmp(prefs->encoder.name, future_prefs->encoder.name)) {
        lives_snprintf(prefs->encoder.name, 64, "%s", future_prefs->encoder.name);
        set_string_pref(PREF_ENCODER, prefs->encoder.name);
        lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", future_prefs->encoder.of_restrict);
        prefs->encoder.of_allowed_acodecs = future_prefs->encoder.of_allowed_acodecs;
      }
      rdet->enc_changed = TRUE;
      rdet->encoder_name = lives_strdup(prefs->encoder.name);
      lives_widget_set_sensitive(rdet->okbutton, TRUE);
    }

    if (prefsw != NULL) {
      lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), array[1]);
    }
    on_encoder_ofmt_changed(NULL, rdet);
    lives_strfreev(array);
    if (ofmt_all != NULL) {
      lives_list_free_all(&ofmt_all);
    }
  }
}


void on_insertwsound_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(togglebutton))) {
    lives_widget_set_sensitive(insertw->fit_checkbutton, FALSE);
  } else {
    lives_widget_set_sensitive(insertw->fit_checkbutton, CURRENT_CLIP_HAS_AUDIO);
  }
  mainw->fx2_bool = !mainw->fx2_bool;
}


/// stored values for loop locking
static int loop_lock_frame = -1;
static lives_direction_t ofwd;

void unlock_loop_lock(void) {
  mainw->loop = mainw->oloop;
  mainw->loop_cont = mainw->oloop_cont;
  mainw->ping_pong = mainw->oping_pong;
  mainw->loop_locked = FALSE;
  if (CURRENT_CLIP_IS_NORMAL) {
    mainw->play_start = cfile->start;
    mainw->play_end = cfile->end;
  }
  loop_lock_frame = -1;
  mainw->clip_switched = TRUE;
}


boolean clip_can_reverse(int clipno) {
  if (!LIVES_IS_PLAYING || mainw->internal_messaging || mainw->is_rendering || mainw->is_processing
      || !IS_VALID_CLIP(clipno) || mainw->preview) return FALSE;
  else {
    lives_clip_t *sfile = mainw->files[clipno];
    if (sfile->clip_type == CLIP_TYPE_DISK) return TRUE;
    if (sfile->next_event != NULL) return FALSE;
    if (sfile->clip_type == CLIP_TYPE_FILE) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
      if (cdata == NULL || !(cdata->seek_flag & LIVES_SEEK_FAST)) return FALSE;
    }
  }
  return TRUE;
}


boolean dirchange_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer area_enum) {
  int area = LIVES_POINTER_TO_INT(area_enum);

  if (!(mod & LIVES_ALT_MASK) && (mod & LIVES_CONTROL_MASK) && mainw->loop_locked) {
    boolean do_ret = FALSE;
    if (!clip_can_reverse(mainw->current_file) || !mainw->ping_pong || ((cfile->pb_fps >= 0. && ofwd == LIVES_DIRECTION_FORWARD)
        || (cfile->pb_fps < 0. && ofwd == LIVES_DIRECTION_BACKWARD))) do_ret = TRUE;
    unlock_loop_lock();
    if (do_ret) return TRUE;
  }

  if (area == SCREEN_AREA_FOREGROUND) {
    if (!CURRENT_CLIP_IS_NORMAL
        || (!clip_can_reverse(mainw->current_file) && cfile->pb_fps > 0.)) return TRUE;
    // change play direction
    if (cfile->play_paused) {
      if (!clip_can_reverse(mainw->current_file) && cfile->freeze_fps > 0.) return TRUE;
      cfile->freeze_fps = -cfile->freeze_fps;
      return TRUE;
    }

    /// set this so we invalid preload cache
    if (mainw->scratch == SCRATCH_NONE) mainw->scratch = SCRATCH_REV;

    lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), -cfile->pb_fps);
    lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);

    // make sure this is called, sometimes we switch clips too soon...
    changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);
  } else if (area == SCREEN_AREA_BACKGROUND) {
    if (!IS_NORMAL_CLIP(mainw->blend_file)
        || (!clip_can_reverse(mainw->blend_file) && mainw->files[mainw->blend_file]->pb_fps >= 0.)) return TRUE;
    mainw->files[mainw->blend_file]->pb_fps = -mainw->files[mainw->blend_file]->pb_fps;
  }
  return TRUE;
}


boolean dirchange_lock_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                                livespointer area_enum) {

  //if (!clip_can_reverse(mainw->current_file)) return TRUE;

  if (!mainw->loop_locked && loop_lock_frame == -1) loop_lock_frame = mainw->actual_frame;
  else {
    // temporary loop_cont / ping-pong
    mainw->clip_switched = FALSE;
    if ((!mainw->loop_locked && mainw->actual_frame < loop_lock_frame) || (mainw->loop_locked && cfile->pb_fps < 0)) {
      if (!mainw->loop_locked || mainw->play_end - mainw->actual_frame > LOOP_LOCK_MIN_FRAMES)
        mainw->play_start = mainw->actual_frame;
      if (!mainw->loop_locked) mainw->play_end = loop_lock_frame;
    } else {
      if (!mainw->loop_locked) mainw->play_start = loop_lock_frame;
      if (!mainw->loop_locked || mainw->actual_frame - mainw->play_start > LOOP_LOCK_MIN_FRAMES)
        mainw->play_end = mainw->actual_frame;
    }
    if (!mainw->loop_locked) {
      mainw->oloop = mainw->loop;
      mainw->oloop_cont = mainw->loop_cont;
      mainw->oping_pong = mainw->ping_pong;
      /// store original direction so when we unlock loop lock we come out with original
      /// this is reversed because we already had one reversal
      ofwd = cfile->pb_fps < 0. ? LIVES_DIRECTION_FORWARD : LIVES_DIRECTION_BACKWARD;
    }
    mainw->loop_cont = TRUE;
    if (clip_can_reverse(mainw->current_file))
      mainw->ping_pong = TRUE;
    mainw->loop = FALSE;
    mainw->loop_locked = TRUE;
    loop_lock_frame = -1;
  }
  if (clip_can_reverse(mainw->current_file))
    return dirchange_callback(group, obj, keyval, mod, area_enum);
  return TRUE;
}


void on_volch_pressed(LiVESButton * button, livespointer user_data) {
  lives_direction_t dirn = LIVES_POINTER_TO_INT(user_data);
  if (!CURRENT_CLIP_IS_VALID || mainw->preview || (mainw->is_processing && cfile->is_loaded) ||
      mainw->cliplist == NULL) return;
  if (dirn == LIVES_DIRECTION_UP) cfile->vol += .01;
  else cfile->vol -= .01;
  if (cfile->vol > 2.) cfile->vol = 2.;
  if (cfile->vol < 0.) cfile->vol = 0.;
  if (prefs->show_overlay_msgs && !(mainw->urgency_msg && prefs->show_urgency_msgs))
    d_print_overlay(.5, _("Clip volume: %.2f"), cfile->vol);
}


boolean fps_reset_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer area_enum) {
  // reset playback fps (cfile->pb_fps) to normal fps (cfile->fps)
  // also resync the audio
  int area;
  if (!LIVES_IS_PLAYING || mainw->multitrack) return TRUE;

  area = LIVES_POINTER_TO_INT(area_enum);
  if (area == SCREEN_AREA_BACKGROUND) {
    if (!IS_NORMAL_CLIP(mainw->blend_file)) return TRUE;
    mainw->files[mainw->blend_file]->pb_fps = mainw->files[mainw->blend_file]->fps;
    return TRUE;
  }

  mainw->scratch = SCRATCH_JUMP_NORESYNC;

  if (mainw->loop_locked) {
    dirchange_callback(group, obj, keyval, LIVES_CONTROL_MASK, SCREEN_AREA_FOREGROUND);
  }

  if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(((double)cfile->frameno));
    /* + (double)(lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL) */
    /* - mainw->startticks) / TICKS_PER_SECOND_DBL * cfile->pb_fps); */
  }

  // change play direction
  if (cfile->play_paused) {
    if (cfile->freeze_fps < 0.) cfile->freeze_fps = -cfile->fps;
    else cfile->freeze_fps = cfile->fps;
    return TRUE;
  }

  lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
  if (cfile->pb_fps > 0.) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->fps);
  else lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), -cfile->fps);
  lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);

  // make sure this is called, sometimes we switch clips too soon...
  changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);

  return TRUE;
}


boolean prevclip_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                          livespointer user_data) {
  LiVESList *list_index;
  int i = 0;
  int num_tried = 0, num_clips;
  int type = 0;

  // prev clip
  // type = 0 : if the effect is a transition, this will change the background clip
  // type = 1 fg only
  // type = 2 bg only

  if (!LIVES_IS_INTERACTIVE) return TRUE;
  if (mainw->go_away) return TRUE;

  if (!CURRENT_CLIP_IS_VALID || mainw->preview || (mainw->is_processing && cfile->is_loaded) ||
      mainw->cliplist == NULL) return TRUE;

  if (user_data != NULL) type = LIVES_POINTER_TO_INT(user_data);

  if (type == 1 && mainw->new_clip != -1) return TRUE;

  if (type == 2 || (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND && mainw->playing_file > 0 && type != 1
                    && !(type == 0 && !IS_NORMAL_CLIP(mainw->blend_file)))) {
    if (!IS_VALID_CLIP(mainw->blend_file)) return TRUE;
    list_index = lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->blend_file));
  } else {
    list_index = lives_list_find(mainw->cliplist,
                                 LIVES_INT_TO_POINTER(mainw->swapped_clip == -1 ? mainw->current_file : mainw->swapped_clip));
  }
  mainw->swapped_clip = -1;

  num_clips = lives_list_length(mainw->cliplist);

  do {
    if (num_tried++ == num_clips) return TRUE; // we might have only audio clips, and then we will block here
    if (list_index == NULL || ((list_index = list_index->prev) == NULL)) list_index = lives_list_last(mainw->cliplist);
    i = LIVES_POINTER_TO_INT(list_index->data);
  } while ((mainw->files[i] == NULL || mainw->files[i]->opening || mainw->files[i]->restoring || i == mainw->scrap_file ||
            i == mainw->ascrap_file || (!mainw->files[i]->frames && LIVES_IS_PLAYING)) &&
           i != ((type == 2 || (mainw->playing_file > 0 && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND && type != 1)) ?
                 mainw->blend_file : mainw->current_file));

  switch_clip(type, i, FALSE);

  return TRUE;
}


boolean nextclip_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                          livespointer user_data) {
  LiVESList *list_index;
  int i;
  int num_tried = 0, num_clips;

  int type = 0; ///< auto (switch bg if a transition is active, otherwise foreground)

  if (!LIVES_IS_INTERACTIVE) return TRUE;
  if (mainw->go_away) return TRUE;
  // next clip
  // if the effect is a transition, this will change the background clip
  if (!CURRENT_CLIP_IS_VALID || mainw->preview || (mainw->is_processing && cfile->is_loaded) ||
      mainw->cliplist == NULL) return TRUE;

  if (user_data != NULL) type = LIVES_POINTER_TO_INT(user_data);

  if (type == 1 && mainw->new_clip != -1) return TRUE;

  if (type == 2 || (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND && mainw->playing_file > 0 && type != 1
                    && !(type == 0 && !IS_NORMAL_CLIP(mainw->blend_file)))) {
    if (!IS_VALID_CLIP(mainw->blend_file)) return TRUE;
    list_index = lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->blend_file));
  } else {
    list_index = lives_list_find(mainw->cliplist,
                                 LIVES_INT_TO_POINTER(mainw->swapped_clip == -1 ? mainw->current_file : mainw->swapped_clip));
  }
  mainw->swapped_clip = -1;

  num_clips = lives_list_length(mainw->cliplist);

  do {
    if (num_tried++ == num_clips) return TRUE; // we might have only audio clips, and then we will block here
    if (list_index == NULL || ((list_index = list_index->next) == NULL)) list_index = mainw->cliplist;
    i = LIVES_POINTER_TO_INT(list_index->data);
  } while ((mainw->files[i] == NULL || mainw->files[i]->opening || mainw->files[i]->restoring || i == mainw->scrap_file ||
            i == mainw->ascrap_file || (!mainw->files[i]->frames && LIVES_IS_PLAYING)) &&
           i != ((type == 2 || (mainw->playing_file > 0 && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND && type != 1)) ?
                 mainw->blend_file : mainw->current_file));

  switch_clip(type, i, FALSE);

  return TRUE;
}


static LiVESResponseType rewrite_orderfile(boolean is_append, boolean add, boolean * got_new_handle) {
  char *ordfile = lives_build_filename(prefs->workdir, mainw->set_name, CLIP_ORDER_FILENAME, NULL);
  char *ordfile_new = lives_build_filename(prefs->workdir, mainw->set_name, CLIP_ORDER_FILENAME "." LIVES_FILE_EXT_NEW, NULL);
  char *cwd = lives_get_current_dir();
  char *new_dir;
  char *dfile, *ord_entry;
  char buff[PATH_MAX] = {0};
  char new_handle[256] = {0};
  LiVESResponseType retval;
  LiVESList *cliplist;
  int ord_fd, i;

  do {
    // create the orderfile which lists all the clips in order
    retval = LIVES_RESPONSE_NONE;
    if (!is_append) ord_fd = creat(ordfile_new, DEF_FILE_PERMS);
    else {
      lives_cp(ordfile, ordfile_new);
      ord_fd = open(ordfile_new, O_CREAT | O_WRONLY | O_APPEND, DEF_FILE_PERMS);
    }

    if (ord_fd < 0) {
      retval = do_write_failed_error_s_with_retry(ordfile, lives_strerror(errno));
      if (retval == LIVES_RESPONSE_CANCEL) {
        lives_free(ordfile);
        lives_free(ordfile_new);
        lives_chdir(cwd, FALSE);
        lives_free(cwd);
        return retval;
      }
    }

    else {
      char *oldval, *newval;

      for (cliplist = mainw->cliplist; cliplist != NULL; cliplist = cliplist->next) {
        if (THREADVAR(write_failed)) break;
        threaded_dialog_spin(0.);
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

        i = LIVES_POINTER_TO_INT(cliplist->data);
        if (IS_NORMAL_CLIP(i) && i != mainw->scrap_file && i != mainw->ascrap_file) {
          lives_snprintf(buff, PATH_MAX, "%s", mainw->files[i]->handle);
          get_basename(buff);
          if (*buff) {
            lives_snprintf(new_handle, 256, "%s/%s/%s", mainw->set_name, CLIPS_DIRNAME, buff);
          } else {
            lives_snprintf(new_handle, 256, "%s/%s/%s", mainw->set_name, CLIPS_DIRNAME, mainw->files[i]->handle);
          }

          if (strcmp(new_handle, mainw->files[i]->handle)) {
            if (!add) continue;

            new_dir = lives_build_filename(prefs->workdir, new_handle, NULL);
            if (lives_file_test(new_dir, LIVES_FILE_TEST_IS_DIR)) {
              // get a new unique handle
              get_temp_handle(i);
              lives_snprintf(new_handle, 256, "%s/%s/%s", mainw->set_name, CLIPS_DIRNAME, mainw->files[i]->handle);
            }
            lives_free(new_dir);

            // move the files
            oldval = lives_build_path(prefs->workdir, mainw->files[i]->handle, NULL);
            newval = lives_build_path(prefs->workdir, new_handle, NULL);

            lives_mv(oldval, newval);
            lives_free(oldval);
            lives_free(newval);

            if (THREADVAR(com_failed)) {
              close(ord_fd);
              end_threaded_dialog();
              lives_free(ordfile);
              lives_free(ordfile_new);
              lives_chdir(cwd, FALSE);
              lives_free(cwd);
              return FALSE;
            }

            *got_new_handle = TRUE;

            lives_snprintf(mainw->files[i]->handle, 256, "%s", new_handle);
            dfile = lives_build_filename(prefs->workdir, mainw->files[i]->handle, LIVES_STATUS_FILE_NAME, NULL);
            lives_snprintf(mainw->files[i]->info_file, PATH_MAX, "%s", dfile);
            lives_free(dfile);
          }

          ord_entry = lives_strdup_printf("%s\n", mainw->files[i]->handle);
          lives_write(ord_fd, ord_entry, strlen(ord_entry), FALSE);
          lives_free(ord_entry);
        }
      }

      if (THREADVAR(write_failed) == ord_fd) {
        THREADVAR(write_failed) = 0;
        retval = do_write_failed_error_s_with_retry(ordfile, NULL);
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  close(ord_fd);

  if (retval == LIVES_RESPONSE_CANCEL) lives_rm(ordfile_new);
  else  lives_cp(ordfile_new, ordfile);

  lives_free(ordfile);
  lives_free(ordfile_new);

  lives_chdir(cwd, FALSE);
  lives_free(cwd);
  return retval;
}


boolean on_save_set_activate(LiVESWidget * widget, livespointer user_data) {
  // here is where we save clipsets
  // SAVE CLIPSET FUNCTION
  // also handles migration and merging of sets
  // new_set_name can be passed in userdata, it should be in filename encoding
  // TODO - caller to do end_threaded_dialog()

  /////////////////
  /// IMPORTANT !!!  mainw->no_wxit must be set. otherwise the app will exit
  ////////////

  char new_set_name[MAX_SET_NAME_LEN] = {0};

  char *old_set = lives_strdup(mainw->set_name);
  char *layout_map_file, *layout_map_dir, *new_clips_dir, *current_clips_dir;
  //char *tmp;
  char *text;
  char *tmp;
  char *osetn, *nsetn, *dfile;

  boolean is_append = FALSE; // we will overwrite the target layout.map file
  boolean response;
  boolean got_new_handle = FALSE;

  int retval;

  if (!mainw->cliplist) return FALSE;

  // warn the user what will happen
  if (widget && !do_save_clipset_warn()) return FALSE;

  if (mainw->stored_event_list && mainw->stored_event_list_changed) {
    // if we have a current layout, give the user the chance to change their mind
    if (!check_for_layout_del(NULL, FALSE)) return FALSE;
  }

  if (widget) {
    // this was called from the GUI
    do {
      // prompt for a set name, advise user to save set
      renamew = create_rename_dialog(2);
      response = lives_dialog_run(LIVES_DIALOG(renamew->dialog));
      if (response == LIVES_RESPONSE_CANCEL) return FALSE;
      lives_snprintf(new_set_name, MAX_SET_NAME_LEN, "%s",
                     (tmp = U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
      lives_free(tmp);
      lives_widget_destroy(renamew->dialog);
      lives_free(renamew);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    } while (!is_legal_set_name(new_set_name, TRUE));
  } else lives_snprintf(new_set_name, MAX_SET_NAME_LEN, "%s", (char *)user_data);

  lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

  lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", new_set_name);

  if (lives_strcmp(mainw->set_name, old_set)) {
    // The user CHANGED the set name
    // we must migrate all physical files for the set, and possibly merge with another set

    new_clips_dir = CLIPS_DIR(mainw->set_name);
    // check if target clips dir exists, ask if user wants to append files
    if (lives_file_test(new_clips_dir, LIVES_FILE_TEST_IS_DIR)) {
      lives_free(new_clips_dir);
      if (mainw->osc_auto == 0) {
        if (!do_set_duplicate_warning(mainw->set_name)) {
          lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", old_set);
          return FALSE;
        }
      } else if (mainw->osc_auto == 1) return FALSE;

      is_append = TRUE;
    } else {
      lives_free(new_clips_dir);
      layout_map_file = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME,
                                             LAYOUT_MAP_FILENAME, NULL);
      // if target has layouts dir but no clips, it means we have old layouts !
      if (lives_file_test(layout_map_file, LIVES_FILE_TEST_EXISTS)) {
        if (do_set_rename_old_layouts_warning(mainw->set_name)) {
          // user answered "yes" - delete
          // clear _old_layout maps
          char *dfile = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, NULL);
          lives_rm(dfile);
          //lives_free(dfile);
        }
      }
      lives_free(layout_map_file);
    }
  }

  text = lives_strdup_printf(_("Saving set %s"), mainw->set_name);
  do_threaded_dialog(text, FALSE);
  lives_free(text);

  /////////////////////////////////////////////////////////////

  THREADVAR(com_failed) = FALSE;

  current_clips_dir = lives_build_filename(prefs->workdir, old_set, CLIPS_DIRNAME "/", NULL);
  if (*old_set && strcmp(mainw->set_name, old_set)
      && lives_file_test(current_clips_dir, LIVES_FILE_TEST_IS_DIR)) {
    // set name was changed for an existing set
    if (!is_append) {
      // create new dir, in case it doesn't already exist
      dfile = lives_build_filename(prefs->workdir, mainw->set_name, CLIPS_DIRNAME, NULL);
      if (!lives_make_writeable_dir(dfile)) {
        // abort if we cannot create the new subdir
        LIVES_ERROR("Could not create directory");
        LIVES_ERROR(dfile);
        d_print_file_error_failed();
        lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", old_set);
        lives_free(dfile);
        end_threaded_dialog();
        return FALSE;
      }
      lives_free(dfile);
    }
  } else {
    // saving as same name (or as new set)
    dfile = lives_build_filename(prefs->workdir, mainw->set_name, CLIPS_DIRNAME, NULL);
    if (!lives_make_writeable_dir(dfile)) {
      // abort if we cannot create the new subdir
      LIVES_ERROR("Could not create directory");
      LIVES_ERROR(dfile);
      d_print_file_error_failed();
      lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", old_set);
      lives_free(dfile);
      end_threaded_dialog();
      return FALSE;
    }
    lives_free(dfile);
  }
  lives_free(current_clips_dir);

  if (mainw->scrap_file > -1) close_scrap_file(TRUE);
  if (mainw->ascrap_file > -1) close_ascrap_file(TRUE);

  retval = rewrite_orderfile(is_append, TRUE, &got_new_handle);

  if (retval == LIVES_RESPONSE_CANCEL) {
    end_threaded_dialog();
    return FALSE;
  }

  if (mainw->num_sets > -1) mainw->num_sets++;

  if (got_new_handle && !strlen(old_set)) migrate_layouts(NULL, mainw->set_name);

  if (*old_set && strcmp(old_set, mainw->set_name)) {
    layout_map_dir = lives_build_filename(prefs->workdir, old_set, LAYOUTS_DIRNAME, LIVES_DIR_SEP, NULL);
    layout_map_file = lives_build_filename(layout_map_dir, LAYOUT_MAP_FILENAME, NULL);
    // update details for layouts - needs_set, current_layout_map and affected_layout_map
    if (lives_file_test(layout_map_file, LIVES_FILE_TEST_EXISTS)) {
      migrate_layouts(old_set, mainw->set_name);
      // save updated layout.map (with new handles), we will move it below

      save_layout_map(NULL, NULL, NULL, layout_map_dir);

      got_new_handle = FALSE;
    }
    lives_free(layout_map_file);
    lives_free(layout_map_dir);

    if (is_append) {
      osetn = lives_build_filename(prefs->workdir, old_set, LAYOUTS_DIRNAME, LAYOUT_MAP_FILENAME, NULL);
      nsetn = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, LAYOUT_MAP_FILENAME, NULL);

      if (lives_file_test(osetn, LIVES_FILE_TEST_EXISTS)) {
        //append current layout.map to target one
        lives_cat(osetn, nsetn, TRUE); /// command may not fail, so we check first
        lives_rm(osetn);
      }
      lives_free(osetn);
      lives_free(nsetn);
    }

    osetn = lives_build_path(prefs->workdir, old_set, LAYOUTS_DIRNAME, NULL);

    if (lives_file_test(osetn, LIVES_FILE_TEST_IS_DIR)) {
      nsetn = lives_build_filename(prefs->workdir, mainw->set_name, NULL);

      // move any layouts from old set to new (including layout.map)
      lives_cp_keep_perms(osetn, nsetn);

      lives_free(nsetn);
    }

    lives_free(osetn);

    // remove the old set (should be empty now)
    cleanup_set_dir(old_set);
  }

  if (!mainw->was_set && !strcmp(old_set, mainw->set_name)) {
    // set name was set by export or save layout, now we need to update our layout map
    layout_map_dir = lives_build_filename(prefs->workdir, old_set, LAYOUTS_DIRNAME, NULL);
    layout_map_file = lives_build_filename(layout_map_dir, LAYOUT_MAP_FILENAME, NULL);
    if (lives_file_test(layout_map_file, LIVES_FILE_TEST_EXISTS)) save_layout_map(NULL, NULL, NULL, layout_map_dir);
    mainw->was_set = TRUE;
    got_new_handle = FALSE;
    lives_free(layout_map_dir);
    lives_free(layout_map_file);
    if (mainw->multitrack != NULL && !mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  if (mainw->current_layouts_map && strcmp(old_set, mainw->set_name) && !mainw->suppress_layout_warnings) {
    // warn the user about layouts if the set name changed
    // but, don't bother the user with errors if we are exiting
    add_lmap_error(LMAP_INFO_SETNAME_CHANGED, old_set, mainw->set_name, 0, 0, 0., FALSE);
    popup_lmap_errors(NULL, NULL);
  }

  lives_notify(LIVES_OSC_NOTIFY_CLIPSET_SAVED, old_set);

  lives_free(old_set);
  if (!mainw->no_exit) {
    mainw->leave_files = TRUE;
    if (mainw->multitrack != NULL && !mainw->only_close) mt_memory_free();
    else if (mainw->multitrack != NULL) wipe_layout(mainw->multitrack);

    // do a lot of cleanup here, but leave files
    lives_exit(0);
    mainw->leave_files = FALSE;
  } else {
    unlock_set_file(old_set);
    end_threaded_dialog();
  }

  lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
  return TRUE;
}


char *on_load_set_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // get set name (use a modified rename window)
  char *set_name = NULL;
  LiVESResponseType resp;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  renamew = create_rename_dialog(3);
  if (!renamew) return NULL; ///< no sets available

  resp = lives_dialog_run(LIVES_DIALOG(renamew->dialog));

  if (resp == LIVES_RESPONSE_OK) {
    set_name = U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)));
  }

  // need to clean up renamew
  lives_widget_destroy(renamew->dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_freep((void **)&renamew);

  if (resp == LIVES_RESPONSE_OK) {
    if (!is_legal_set_name(set_name, TRUE)) {
      lives_freep((void **)&set_name);
    } else {
      if (!user_data) {
        if (mainw->cliplist)
          if (!do_reload_set_query()) return NULL;
        reload_set(set_name);
        lives_free(set_name);
        if (mainw->num_sets > -1) mainw->num_sets--;
        return NULL;
      }
    }
  }

  return set_name;
}


void lock_set_file(const char *set_name) {
  // function is called when a set is opened, to prevent multiple acces to the same set
  char *set_lock_file = lives_strdup_printf("%s.%d", SET_LOCK_FILENAME, capable->mainpid);
  char *set_locker = SET_LOCK_FILE(set_name, set_lock_file);
  lives_touch(set_locker);
  lives_free(set_locker);
  lives_free(set_lock_file);
}


void unlock_set_file(const char *set_name) {
  char *set_lock_file = lives_strdup_printf("%s.%d", SET_LOCK_FILENAME, capable->mainpid);
  char *set_locker = SET_LOCK_FILE(set_name, set_lock_file);
  lives_rm(set_locker);
  lives_free(set_lock_file);
  lives_free(set_locker);
}


boolean reload_set(const char *set_name) {
  // this is the main clip set loader

  // CLIP SET LOADER

  // setname should be in filesystem encoding

  FILE *orderfile;
  lives_clip_t *sfile;

  char *msg, *com, *ordfile, *cwd, *clipdir, *handle = NULL;

  boolean added_recovery = FALSE;
  boolean keep_threaded_dialog = FALSE;
  boolean hadbad = FALSE;

  int last_file = -1, new_file = -1;
  int current_file = mainw->current_file;
  int clipnum = 0;
  int maxframe;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
  }
  lives_memset(mainw->set_name, 0, 1);

  // check if set is locked
  if (!check_for_lock_file(set_name, 0)) {
    if (!mainw->recovering_files) {
      d_print_cancelled();
      if (mainw->multitrack != NULL) {
        mainw->current_file = mainw->multitrack->render_file;
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
    }
    return FALSE;
  }

  lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "none");

  // check if we already have a threaded dialog running (i.e. we are called from startup)
  if (mainw->threaded_dialog) keep_threaded_dialog = TRUE;

  if (prefs->show_gui && !keep_threaded_dialog) {
    char *tmp;
    msg = lives_strdup_printf(_("Loading clips from set %s"), (tmp = F2U8(set_name)));
    do_threaded_dialog(msg, FALSE);
    lives_free(msg);
    lives_free(tmp);
  }

  ordfile = lives_build_filename(prefs->workdir, set_name, CLIP_ORDER_FILENAME, NULL);

  orderfile = fopen(ordfile, "r"); // no we can't assert this, because older sets did not have this file
  lives_free(ordfile);

  mainw->suppress_dprint = TRUE;
  THREADVAR(read_failed) = FALSE;

  // lock the set
  lock_set_file(set_name);

  cwd = lives_get_current_dir();

  while (1) {
    if (prefs->show_gui) threaded_dialog_spin(0.);

    if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

    if (!orderfile) {
      // old style (pre 0.9.6)
      com = lives_strdup_printf("%s get_next_in_set \"%s\" \"%s\" %d", prefs->backend_sync, mainw->msg,
                                set_name, capable->mainpid);
      lives_system(com, FALSE);
      lives_free(com);
    } else {
      if (lives_fgets(mainw->msg, MAINW_MSG_SIZE, orderfile) == NULL) clear_mainw_msg();
      else lives_memset(mainw->msg + lives_strlen(mainw->msg) - 1, 0, 1);
    }

    if (!(*mainw->msg) || (!strncmp(mainw->msg, "none", 4))) {
      if (!mainw->recovering_files) mainw->suppress_dprint = FALSE;
      if (!keep_threaded_dialog) end_threaded_dialog();

      if (orderfile != NULL) fclose(orderfile);

      //mainw->current_file = current_file;

      if (mainw->multitrack == NULL) {
        if (last_file > 0) {
          threaded_dialog_spin(0.);
          switch_to_file(current_file, last_file);
          threaded_dialog_spin(0.);
        }
      }

      if (clipnum == 0) {
        do_set_noclips_error(set_name);
      } else {
        char *tmp;
        reset_clipmenu();
        lives_widget_set_sensitive(mainw->vj_load_set, FALSE);

        // MUST set set_name before calling this
        recover_layout_map(MAX_FILES);

        // TODO - check for missing frames and audio in layouts

        if (hadbad) rewrite_orderfile(FALSE, FALSE, NULL);

        d_print(_("%d clips and %d layouts were recovered from set (%s).\n"),
                clipnum, lives_list_length(mainw->current_layouts_map), (tmp = F2U8(set_name)));
        lives_free(tmp);
        lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", set_name);
        lives_notify(LIVES_OSC_NOTIFY_CLIPSET_OPENED, mainw->set_name);
      }

      threaded_dialog_spin(0.);
      if (mainw->multitrack == NULL) {
        if (mainw->is_ready) {
          if (clipnum > 0 && CURRENT_CLIP_IS_VALID) {
            showclipimgs();
          }
          // force a redraw
          update_play_times();
          lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
        }
      } else {
        mainw->current_file = mainw->multitrack->render_file;
        polymorph(mainw->multitrack, POLY_NONE);
        polymorph(mainw->multitrack, POLY_CLIPS);
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      if (!keep_threaded_dialog) end_threaded_dialog();
      lives_chdir(cwd, FALSE);
      lives_free(cwd);
      if (mainw->multitrack != NULL)
        mt_clip_select(mainw->multitrack, TRUE); // scroll clip on screen
      sensitize();
      return TRUE;
    }

    if (clipnum > 0)
      mainw->was_set = TRUE;

    if (prefs->crash_recovery && !added_recovery) {
      char *recovery_entry = lives_build_filename(set_name, "*", NULL);
      add_to_recovery_file(recovery_entry);
      lives_free(recovery_entry);
      added_recovery = TRUE;
    }

    clipdir = lives_build_filename(prefs->workdir, mainw->msg, NULL);
    if (orderfile != NULL) {
      // newer style (0.9.6+)

      if (!lives_file_test(clipdir, LIVES_FILE_TEST_IS_DIR)) {
        lives_free(clipdir);
        continue;
      }
      threaded_dialog_spin(0.);

      //create a new cfile and fill in the details
      handle = lives_strndup(mainw->msg, 256);
    }

    // changes mainw->current_file on success
    sfile = create_cfile(-1, handle, FALSE);
    if (handle) lives_free(handle);
    handle = NULL;
    threaded_dialog_spin(0.);

    if (!sfile) {
      lives_free(clipdir);
      mainw->suppress_dprint = FALSE;

      if (!keep_threaded_dialog) end_threaded_dialog();

      if (mainw->multitrack != NULL) {
        mainw->current_file = mainw->multitrack->render_file;
        polymorph(mainw->multitrack, POLY_NONE);
        polymorph(mainw->multitrack, POLY_CLIPS);
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }

      recover_layout_map(MAX_FILES);
      lives_chdir(cwd, FALSE);
      lives_free(cwd);
      fclose(orderfile);
      return FALSE;
    }

    // get file details
    if (!read_headers(mainw->current_file, clipdir, NULL)) {
      /// set clip failed to load, we need to do something with it
      /// else it will keep trying to reload each time.
      /// for now we shall just move it out of the set
      /// this is fine as the user can try to recover it at a later time
      lives_free(clipdir);
      lives_free(mainw->files[mainw->current_file]);
      mainw->files[mainw->current_file] = NULL;
      if (mainw->first_free_file > mainw->current_file) mainw->first_free_file = mainw->current_file;
      if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);
      hadbad = TRUE;
      continue;
    }
    lives_free(clipdir);

    threaded_dialog_spin(0.);

    /** read_headers() will have read the clip metadata file, so we 'know' how many frames there ought to be.
      however this could be wrong if the file was damaged for some reason. So the first step is to read the file_index if any.
      - if present we assume we are dealing with CLIP_TYPE_FILE (original clip + decoded frames).
      -- If it contains more frames then we will increase cfile->frames.
      - If it is not present then we assume are dealing with CLIP_TYPE_DISK (all frames decoded to images).

      we want to do as little checking here as possible, since it can slow down the startup, but if we detect a problem then we'll
      do increasingly more checking.
    */

    if ((maxframe = load_frame_index(mainw->current_file)) > 0) {
      // CLIP_TYPE_FILE
      /** here we attempt to reload the clip. First we load the frame_index if any, If it contains more frames than the metadata says, then
        we trust the frame_index for now, since the metadata may have been corrupted.
        If it contains fewer frames, then we will warn the user, but we cannot know whether the metadata is correct or not,
        and in any case we cannot reconstruct the frame_index, so we have to use what it tells us.
        If file_index is absent then we assume we are dealing with CLIP_TYPE_DISK (below).

        Next we attempt to reload the original clip using the same decoder plugin as last time if possible.
        The decoder may return fewer frames from the original clip than the size of frame_index,
        This is OK provided the final frames are decoded frames.
        We then check backwards from the end of file_index to find the final decoded frame.
        If this is the frame we expected then we assume all is OK */

      if (!reload_clip(mainw->current_file, maxframe)) {
        if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);
        continue;
      }
      if (cfile->clip_type == CLIP_TYPE_FILE && cfile->header_version >= 102) cfile->fps = cfile->pb_fps;

      /** if the image type is still unkown it means either there were no decoded frames, or the final decoded frame was absent
        so we count the virtual frames. If all are virtual then we set img_type to prefs->img_type and assume all is OK
        (or at least we recovered as many frames as we could using frame_index),
        and we'll accept whatever the decoder returns if there is a divergence with the clip metadata */

      if (!cfile->checked && cfile->img_type == IMG_TYPE_UNKNOWN) {
        lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
        int fvirt = count_virtual_frames(cfile->frame_index, 1, cfile->frames);
        /** if there are some decoded frames then we have a problem.
          Since the img type was not found it means that the final decoded
            frame was missing. So we check backwards to find where the last actual decoded frame is and the frame count is set to
            final decoded frame + any virtual frames immediately following, and warn the user.
            If other frames are missing then the clip is corrupt, but we'll continue as best we can. */
        if (fvirt < cfile->frames) check_clip_integrity(mainw->current_file, cdata, cfile->frames);
      }
      cfile->checked = TRUE;
    } else {
      /// CLIP_TYPE_DISK
      /** in this case we find the last decoded frame and check the frame size and get the image_type.
        If there is a discrepancy with the metadata then we trust the empirical evidence.
        If the final frame is absent then we find the real final frame, warn the user, and adjust frame count.
      */
      boolean isok = TRUE;
      if (!cfile->checked) isok = check_clip_integrity(mainw->current_file, NULL, cfile->frames);
      cfile->checked = TRUE;
      /** here we do a simple check: make sure the final frame is present and frame + 1 isn't
        if either check fails then we count all the frames (since we don't have a frame_index to guide us),
        - this can be pretty slow so we wan't to avoid it unless  we detected a problem. */
      if (!check_frame_count(mainw->current_file, isok)) {
        cfile->frames = get_frame_count(mainw->current_file, 1);
        if (cfile->frames == -1) {
          close_current_file(0);
          if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);
          continue;
        }
        cfile->needs_update = TRUE;
      }
    }

    if (!prefs->vj_mode) {
      if (cfile->achans > 0 && cfile->afilesize == 0) {
        reget_afilesize_inner(mainw->current_file);
      }
    }

    last_file = new_file;

    if (++clipnum == 1) {
      /** we need to set the set_name before calling add_to_clipmenu(), so that the clip gets the name of the set in
        its menuentry, and also prior to loading any layouts since they specirfy the clipset they need */
      lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", set_name);
    }

    /// read the playback fps, play frame, and name
    open_set_file(clipnum); ///< must do before calling save_clip_values()
    if (cfile->clip_type == CLIP_TYPE_FILE && cfile->header_version >= 102) cfile->fps = cfile->pb_fps;

    /// if this is set then it means we are auto reloading the clipset from the previous session, so restore full details
    if (future_prefs->ar_clipset) restore_clip_binfmt(mainw->current_file);

    threaded_dialog_spin(0.);
    cfile->was_in_set = TRUE;

    if (cfile->frameno > cfile->frames) cfile->frameno = cfile->last_frameno = 1;

    if (cfile->needs_update || cfile->needs_silent_update) {
      if (cfile->needs_update) do_clip_divergence_error(mainw->current_file);
      save_clip_values(mainw->current_file);
      cfile->needs_silent_update = cfile->needs_update = FALSE;
    }

    if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);

    if (prefs->autoload_subs) {
      reload_subs(mainw->current_file);
      threaded_dialog_spin(0.);
    }

    get_total_time(cfile);

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
      if (cfile->aseek_pos > cfile->afilesize) cfile->aseek_pos = 0.;
    }

    // add to clip menu
    threaded_dialog_spin(0.);
    add_to_clipmenu();
    cfile->start = cfile->frames > 0 ? 1 : 0;
    cfile->end = cfile->frames;
    cfile->is_loaded = TRUE;
    cfile->changed = TRUE;
    lives_rm(cfile->info_file);
    set_main_title(cfile->name, 0);
    restore_clip_binfmt(mainw->current_file);

    if (mainw->multitrack == NULL) {
      resize(1);
    }

    if (mainw->multitrack != NULL && mainw->multitrack->is_ready) {
      mainw->current_file = mainw->multitrack->render_file;
      mt_init_clips(mainw->multitrack, new_file, TRUE);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      mt_clip_select(mainw->multitrack, TRUE);
    }

    lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  }

  // should never reach here
  lives_chdir(cwd, FALSE);
  lives_free(cwd);
  return TRUE;
}


static void recover_lost_clips(LiVESList * reclist) {
  if (!do_foundclips_query()) return;

  //d_print_cancelled();

  // save set
  if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID) {
    on_quit_activate(NULL, LIVES_INT_TO_POINTER(1));
    if (mainw->clips_available) return;
  }

  // recover files
  mainw->recovery_list = reclist;
  recover_files(NULL, TRUE);

  if (prefs->crash_recovery) rewrite_recovery_file();

  if (!CURRENT_CLIP_IS_VALID) {
    int start_file;
    for (start_file = MAX_FILES; start_file > 0; start_file--) {
      if (IS_VALID_CLIP(start_file)
          && (mainw->files[start_file]->frames > 0 || mainw->files[start_file]->afilesize > 0)) break;
    }
    switch_to_file(-1, start_file);
  }
  do_info_dialogf(P_("$d clip was recovered", "%d clips were recovered\n",
                     mainw->clips_available), mainw->clips_available);
}


static boolean handle_remnants(LiVESList * recnlist, const char *trashremdir, LiVESList **rem_list) {
  LiVESResponseType resp;
  LiVESList *list = recnlist;
  char *unrecdir = lives_build_path(prefs->workdir, UNREC_CLIPS_DIR, NULL);
  char *text = lives_strdup_printf(_("Some clips could not be recovered.\n"
                                     "These items can be deleted or moved to the directory\n%s\n"
                                     "What would you like to do with them ?"), unrecdir);
  LiVESWidget *dialog = create_question_dialog(_("Unrecoverable Clips"), text), *bbox, *cancelbutton;

  lives_free(text);

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, _("Ignore"),
                 LIVES_RESPONSE_CANCEL);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_DELETE, _("Delete them"),
                                     LIVES_RESPONSE_NO);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_SAVE, _("Move them"),
                                     LIVES_RESPONSE_OK);

  bbox = lives_dialog_get_action_area(LIVES_DIALOG(dialog));
  trash_rb(LIVES_BUTTON_BOX(bbox));
  lives_dialog_add_escape(LIVES_DIALOG(dialog), cancelbutton);

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  lives_widget_destroy(dialog);
  lives_widget_context_update();
  THREADVAR(com_failed) = FALSE;

  if (resp == LIVES_RESPONSE_CANCEL) return FALSE;

  if (resp == LIVES_RESPONSE_OK) {
    // move from workdir to workdir/unrec
    char *from, *to, *norem;
    char *ucdir = lives_build_path(prefs->workdir, UNREC_CLIPS_DIR, NULL);
    lives_mkdir_with_parents(ucdir, capable->umask);
    if (!lives_file_test(ucdir, LIVES_FILE_TEST_IS_DIR)) return FALSE;
    norem = lives_build_filename(ucdir, LIVES_FILENAME_NOREMOVE, NULL);
    lives_touch(norem);
    lives_free(norem);
    for (; list; list = list->next) {
      from = lives_build_path(prefs->workdir, (char *)list->data, NULL);
      to = lives_build_path(ucdir, (char *)list->data, NULL);
      lives_mv(from, to);
      lives_free(from); lives_free(to);
      if (THREADVAR(com_failed)) {
        THREADVAR(com_failed) = FALSE;
        return FALSE;
      }
    }
    lives_free(ucdir);
  } else {
    char *tfile;
    lives_file_dets_t *fdets;
    // touch files in remdir, also prepend to remdir so we dont skip removal
    for (; list; list = list->next) {
      tfile = lives_build_filename(trashremdir, (char *)list->data, NULL);
      lives_touch(tfile);
      lives_free(tfile);
      if (THREADVAR(com_failed)) {
        THREADVAR(com_failed) = FALSE;
        return FALSE;
      }
      fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);
      fdets->name = lives_strdup((char *)list->data);
      *rem_list = lives_list_prepend(*rem_list, fdets);
    }
  }
  return TRUE;
}


void on_cleardisk_activate(LiVESWidget * widget, livespointer user_data) {
  // recover disk space
  lives_file_dets_t *filedets;
  lives_proc_thread_t tinfo;
  LiVESTextBuffer *tbuff;
  LiVESWidget *top_vbox;

  LiVESList *lists[3];
  LiVESList **left_list, **rec_list, **rem_list;
  LiVESList *list;

  int64_t bytes = 0, fspace = -1;
  int64_t ds_warn_level = mainw->next_ds_warn_level;

  uint64_t ukey;

  char *uidgid;
  char *trashdir = NULL, *full_trashdir = NULL;

  char *markerfile, *filedir;
  char *com, *msg, *tmp;
  char *extra = lives_strdup("");

  LiVESResponseType retval = LIVES_RESPONSE_NONE;

  boolean gotsize = FALSE;

  int current_file = mainw->current_file;
  int marker_fd;
  int i, ntok, nitems = 0;

  mainw->mt_needs_idlefunc = FALSE;

  mainw->next_ds_warn_level = 0; /// < avoid nested warnings

  if (user_data) lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(user_data)));

  rec_list = &lists[0];
  rem_list = &lists[1];
  left_list = &lists[2];

  *rec_list = *rem_list = *left_list = NULL;

  mainw->tried_ds_recover = TRUE; ///< indicates we tried ds recovery already
  mainw->add_clear_ds_adv = TRUE; ///< auto reset by do_warning_dialog()
  if (!(prefs->clear_disk_opts & LIVES_CDISK_REMOVE_ORPHAN_CLIPS)) {
    lives_free(extra);
    extra = (_("\n\nIf potential missing clips are detected, you will be provided "
               "the option to try to recover them\n"
               "before they are removed permanently from the disk.\n\n"
               "<b>You will also have an opportunity to view and revise the list of items to be removed "
               "before continuing.</b>\n"));
  }
  widget_opts.use_markup = TRUE;
  if (!do_warning_dialogf(
        _("LiVES will attempt to recover some disk space.\n"
          "Unnecessary files will be removed from %s\n"
          "You should <b>ONLY</b> run this if you have no other copies of LiVES running on this machine.\n"
          "%s\nClick OK to proceed.\n"), tmp = lives_markup_escape_text(prefs->workdir, -1), extra)) {
    widget_opts.use_markup = FALSE;
    lives_free(tmp);
    lives_free(extra);
    mainw->next_ds_warn_level = ds_warn_level;
    return;
  }
  widget_opts.use_markup = FALSE;
  lives_free(extra);

  if (CURRENT_CLIP_IS_VALID) cfile->cb_src = current_file;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  d_print(_("Cleaning up disk space..."));
  // get a temporary clip for receiving data from backend
  if (!get_temp_handle(-1)) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    d_print_failed();
    mainw->next_ds_warn_level = ds_warn_level;
    return;
  }

  if (mainw->multitrack) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  ukey = gen_unique_id();
  uidgid = lives_strdup_printf("-%d-%d", lives_getuid(), lives_getgid());
  trashdir = lives_strdup_printf("%s%lu%s", TRASH_NAME, ukey, uidgid);

  for (i = 0; i < MAX_FILES; i++) {
    // mark all free-floating files (directories) which we do not want to remove
    // we do error checking here
    if (mainw->files[i] && *mainw->files[i]->handle) {
      filedir = lives_build_path(prefs->workdir, mainw->files[i]->handle, NULL);
      if (lives_file_test(filedir, LIVES_FILE_TEST_IS_DIR)) {
        markerfile = lives_build_filename(filedir, LIVES_FILENAME_INUSE, NULL);
        lives_echo(trashdir, markerfile, FALSE);
        lives_free(markerfile);
        if (mainw->files[i]->undo_action != UNDO_NONE) {
          markerfile = lives_build_filename(filedir, LIVES_FILENAME_NOCLEAN, NULL);
          do {
            retval = LIVES_RESPONSE_NONE;
            marker_fd = creat(markerfile, S_IRUSR | S_IWUSR);
            if (marker_fd < 0) {
              retval = do_write_failed_error_s_with_retry(markerfile, lives_strerror(errno));
            }
          } while (retval == LIVES_RESPONSE_RETRY);
          if (marker_fd >= 0) close(marker_fd);
          lives_free(markerfile);
          if (retval == LIVES_RESPONSE_CANCEL) goto cleanup;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  full_trashdir = lives_build_path(prefs->workdir, trashdir, NULL);

  if (!check_dir_access(full_trashdir, TRUE)) {
    do_dir_perm_error(full_trashdir);
    goto cleanup;
  }

  // get space before
  fspace = get_ds_free(prefs->workdir);

  //if (prefs->autoclean) {
  // clearup old
  mainw->cancelled = CANCEL_NONE;

  com = lives_strdup_printf("%s empty_trash %s general %s", prefs->backend, cfile->handle,
                            TRASH_NAME);
  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);
  do_auto_dialog(_("Removing general trash"), 0);

  THREADVAR(com_failed) = FALSE;

  if (CURRENT_CLIP_IS_VALID) lives_rm(cfile->info_file);

  tinfo = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)do_auto_dialog, -1,
                                   "si", _("Analysing Disk"), 0);
  tbuff = lives_text_buffer_new();
  com = lives_strdup_printf("%s disk_check %s %u %s", prefs->backend, cfile->handle,
                            prefs->clear_disk_opts, trashdir);
  lives_free(uidgid);
  lives_popen(com, TRUE, (char *)tbuff, 0);
  lives_free(com);

  lives_proc_thread_join(tinfo);

  if (*mainw->msg && (ntok = get_token_count(mainw->msg, '|')) > 1) {
    char **array = lives_strsplit(mainw->msg, "|", 2);
    if (!strcmp(array[0], "completed")) {
      nitems = atoi(array[1]);
    }
    lives_strfreev(array);
  }

  // remove the protective markers
  for (i = 0; i < MAX_FILES; i++) {
    if (mainw->files[i] && *mainw->files[i]->handle) {
      filedir = lives_build_path(prefs->workdir, mainw->files[i]->handle, NULL);
      if (lives_file_test(filedir, LIVES_FILE_TEST_IS_DIR)) {
        markerfile = lives_build_filename(prefs->workdir, mainw->files[i]->handle,
                                          LIVES_FILENAME_INUSE, NULL);
        lives_rm(markerfile);
        lives_free(markerfile);
        if (mainw->files[i]->undo_action != UNDO_NONE) {
          markerfile = lives_build_filename(prefs->workdir, mainw->files[i]->handle,
                                            LIVES_FILENAME_NOCLEAN, NULL);
          lives_rm(markerfile);
          lives_free(markerfile);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
  } else {
    LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    LiVESWidget *button, *accb, *hbox, *label;
    lives_proc_thread_t recinfo, reminfo, leaveinfo;
    char *remtrashdir, *op, *from, *to;
    int orig;

    if (nitems) {
      char *dirname = lives_build_path(full_trashdir, TRASH_RECOVER, NULL);
      recinfo =
        dir_to_file_details(rec_list, dirname, prefs->workdir,
                            EXTRA_DETAILS_CLIPHDR);
      lives_free(dirname);

      dirname = lives_build_path(full_trashdir, TRASH_REMOVE, NULL);
      reminfo =
        dir_to_file_details(rem_list, dirname, prefs->workdir,
                            EXTRA_DETAILS_EMPTY_DIRS | EXTRA_DETAILS_DIRSIZE);
      lives_free(dirname);

      dirname = lives_build_path(full_trashdir, TRASH_LEAVE, NULL);
      leaveinfo =
        dir_to_file_details(left_list, dirname, prefs->workdir,
                            EXTRA_DETAILS_EMPTY_DIRS | EXTRA_DETAILS_DIRSIZE);
      lives_free(dirname);
    } else {
      *rec_list = lives_list_append(*rec_list, NULL);
      *rem_list = lives_list_append(*rem_list, NULL);
      *left_list = lives_list_append(*left_list, NULL);
    }

    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH;
    textwindow = create_text_window(_("Disk Analysis Log"), NULL, tbuff, FALSE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;;

    lives_window_add_accel_group(LIVES_WINDOW(textwindow->dialog), accel_group);

    top_vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    msg = lives_strdup_printf(_("\nAnalysis of directory %s complete.\n\n"), prefs->workdir);
    label = lives_standard_label_new(msg);
    lives_free(msg);

    lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
    lives_widget_set_halign(label, LIVES_ALIGN_CENTER);

    if (!nitems) {
      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
      msg = _("No items to be removed or recovered were detected.\n");
      label = lives_standard_label_new(msg);
      lives_free(msg);

      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
      lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
    }

    lives_widget_object_ref(textwindow->vbox);
    lives_widget_unparent(textwindow->vbox);

    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    lives_standard_expander_new(_("Show _Log"), LIVES_BOX(top_vbox),
                                textwindow->vbox);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_widget_object_unref(textwindow->vbox);

    add_fill_to_box(LIVES_BOX(top_vbox));

    button =
      lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
                                         LIVES_STOCK_CANCEL, nitems ? NULL : mainw->string_constants
                                         [LIVES_STRING_CONSTANT_CLOSE_WINDOW], LIVES_RESPONSE_CANCEL);

    lives_widget_add_accelerator(button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

    widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
    accb = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
           LIVES_STOCK_EDIT,
           nitems ? _("_Check and Filter Results")
           : _("Show Results"),
           LIVES_RESPONSE_BROWSE);

    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_button_grab_default_special(accb);

    if (nitems) {
      LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(textwindow->dialog));
      lives_button_box_set_child_non_homogeneous(LIVES_BUTTON_BOX(bbox), button, TRUE);
      lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), LIVES_BUTTONBOX_START);
    }

    retval = lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
    lives_widget_destroy(textwindow->dialog);
    lives_free(textwindow);

    if (retval != LIVES_RESPONSE_CANCEL) {
      retval = filter_cleanup(full_trashdir, rec_list, rem_list, left_list);
    }

    if (!nitems) {
      gotsize = TRUE;
      goto cleanup;
    }

    lives_proc_thread_cancel(recinfo);
    lives_proc_thread_cancel(reminfo);
    lives_proc_thread_cancel(leaveinfo);

    if (retval == LIVES_RESPONSE_CANCEL) {
      com = lives_strdup_printf("%s restore_trash %s", prefs->backend, trashdir);
      lives_system(com, FALSE);
      lives_free(com);
      goto cleanup;
    }

    /// user accepted

    // first we need to move some entries at the list starts
    // type now indicates the origin list, since moved entries were prepended
    // we can stop when orig == current list

    THREADVAR(com_failed) = FALSE;

    for (list = *rem_list; list && list->data; list = list->next) {
      filedets = (lives_file_dets_t *)list->data;
      orig = filedets->type & ~LIVES_FILE_TYPE_FLAG_SPECIAL;
      if (orig == 1) break;
      to = lives_build_path(full_trashdir, TRASH_REMOVE, filedets->name, NULL);
      if (!orig) {
        // moved from rec_list to rem_list
        from = lives_build_path(full_trashdir, TRASH_RECOVER, filedets->name, NULL);
      } else {
        // moved from left_list to rem_list
        from = lives_build_path(full_trashdir, TRASH_LEAVE, filedets->name, NULL);
      }
      lives_mv(from, to);
      lives_free(from);
      lives_free(to);
      if (THREADVAR(com_failed)) {
        THREADVAR(com_failed) = FALSE;
        goto cleanup;
      }
    }

    for (list = *left_list; list && list->data; list = list->next) {
      filedets = (lives_file_dets_t *)list->data;
      orig = filedets->type;
      if (orig == 2) break;
      to = lives_build_path(full_trashdir, TRASH_LEAVE, filedets->name, NULL);
      if (!orig) {
        // moved from rec_list to left_list
        from = lives_build_path(full_trashdir, TRASH_RECOVER, filedets->name, NULL);
      } else {
        // moved from rem_list to left_list
        from = lives_build_path(full_trashdir, TRASH_REMOVE, filedets->name, NULL);
      }
      lives_mv(from, to);
      lives_free(from);
      lives_free(to);
      if (THREADVAR(com_failed)) {
        THREADVAR(com_failed) = FALSE;
        goto cleanup;
      }
    }

    list = *rec_list;

    if (list && list->data) {
      /// try to recover lost files first
      // create a list with just the names
      LiVESList *recnlist = NULL;

      for (; list && list->data; list = list->next) {
        filedets = (lives_file_dets_t *)list->data;
        if (*filedets->name)
          recnlist = lives_list_prepend(recnlist, lives_strdup(filedets->name));
      }
      recnlist = lives_list_reverse(recnlist);

      // close the temporary clip
      close_temp_handle(current_file);

      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      recover_lost_clips(recnlist);
      lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

      /// handle any remnants. Remove any entries from recnlist which are now loaded
      /// the remainder will be deleted or moved to "unrecoverable_clips"

      for (list = mainw->cliplist; list; list = list->next) {
        int clipno = LIVES_POINTER_TO_INT(list->data);
        LiVESList *list2 = recnlist;
        for (; list2; list2 = list2->next) {
          if (!strcmp(mainw->files[clipno]->handle, (char *)list2->data)) {
            if (list2->prev) list2->prev->next = list2->next;
            if (list2->next) list2->next->prev = list2->prev;
            if (recnlist == list2) recnlist = list2->next;
            list2->next = list2->prev = NULL;
            lives_list_free(list2);
            break;
          }
        }
      }

      current_file = mainw->current_file;

      // get a temporary clip for receiving data from backend
      if (!get_temp_handle(-1)) {
        lives_list_free_all(&recnlist);
        mainw->next_ds_warn_level = ds_warn_level;
        goto cleanup;
      }

      if (recnlist) {
        boolean bresp;
        remtrashdir = lives_build_path(full_trashdir, TRASH_REMOVE, NULL);

        /// handle unrecovered items
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        bresp = handle_remnants(recnlist, remtrashdir, rem_list);
        lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
        if (!bresp) {
          // if failed / cancelled, add to left_list
          lives_file_dets_t *fdets;
          for (list = recnlist; list; list = list->next) {
            fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);
            fdets->name = lives_strdup((char *)list->data);
            *left_list = lives_list_prepend(*left_list, fdets);
          }
        }
        lives_list_free_all(&recnlist);
        lives_free(remtrashdir);
      }
    }
    // now finally we remove all in rem_list, this is done in the backend
    list = *rem_list;
    if (list && list->data) {
      if (prefs->pref_trash) op = lives_strdup("giotrash");
      else op = lives_strdup("delete");

      remtrashdir = lives_build_path(trashdir, TRASH_REMOVE, NULL);

      if (CURRENT_CLIP_IS_VALID) lives_rm(cfile->info_file);

      tinfo = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)do_auto_dialog, -1,
                                       "si", _("Clearing Disk"), 0);
      tbuff = lives_text_buffer_new();

      com = lives_strdup_printf("%s empty_trash \"%s\" %s \"%s\"",
                                prefs->backend, cfile->handle, op, remtrashdir);
      lives_free(op);
      lives_free(remtrashdir);

      lives_popen(com, TRUE, (char *)tbuff, 0);
      lives_free(com);
      lives_proc_thread_join(tinfo);

      lives_rm(cfile->info_file);
    }

    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      goto cleanup;
    }

    bytes = get_ds_free(prefs->workdir) - fspace;
    gotsize = TRUE;
  }

cleanup:

  if (trashdir) lives_free(trashdir);

  if (full_trashdir) {
    lives_rmdir(full_trashdir, TRUE);
    lives_free(full_trashdir);
  }

  if (*rec_list) free_fdets_list(rec_list);

  // close the temporary clip
  close_temp_handle(current_file);

  if (bytes < 0) bytes = 0;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  if (gotsize && retval != LIVES_RESPONSE_CANCEL && !THREADVAR(com_failed) && fspace > -1) {
    LiVESWidget *dialog, *tview;

    d_print_done();

    msg = lives_strdup_printf(_("%s of disk space was recovered.\n"),
                              lives_format_storage_space_string((uint64_t)bytes));

    dialog = create_message_dialog(LIVES_DIALOG_INFO, msg, 0);
    lives_free(msg);

    list = *rem_list;
    top_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

    if (list && list->data) {
      lives_label_chomp(LIVES_LABEL(widget_opts.last_label));
      widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
      tview = scrolled_textview(NULL, tbuff, RFX_WINSIZE_H * 2, NULL);
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      lives_standard_expander_new(_("Show _Log"), LIVES_BOX(top_vbox), tview);
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    }

    list = *left_list;

    if (list && list->data) {
      char *text = NULL, *item;
      LiVESWidget *label = lives_standard_label_new(_("Some files and directories may be removed manually "
                           "if desired.\nClick for details:\n"));
      LiVESWidget *hbox = lives_hbox_new(FALSE, 0);

      lives_box_pack_start(LIVES_BOX(top_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
      lives_widget_set_halign(label, LIVES_ALIGN_CENTER);

      for (; list && list->data; list = list->next) {
        filedets = (lives_file_dets_t *)list->data;
        item = lives_build_path(prefs->workdir, filedets->name, NULL);
        text = lives_concat_sep(text, "\n", item);
      }

      widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
      tview = scrolled_textview(text, NULL, RFX_WINSIZE_H * 2, NULL);
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      lives_free(text);

      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      lives_standard_expander_new(_("Show _Remaining Items"),
                                  LIVES_BOX(top_vbox), tview);
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    }

    lives_dialog_run(LIVES_DIALOG(dialog));
  } else {
    if (retval != LIVES_RESPONSE_CANCEL) d_print_failed();
    else d_print_cancelled();
  }

  if (*rem_list) free_fdets_list(rem_list);
  if (*left_list) free_fdets_list(left_list);

  mainw->next_ds_warn_level = ds_warn_level;

  if (user_data) {
    mainw->dsu_valid = FALSE;
    if (!disk_monitor_running()) {
      mainw->dsu_valid = TRUE;
      lives_idle_add_simple(update_dsu, NULL);
    }
    lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(user_data)));
  } else {
    if (!mainw->multitrack && !mainw->is_processing && !LIVES_IS_PLAYING) {
      sensitize();
    }

    if (mainw->multitrack) {
      if (!mainw->is_processing && !LIVES_IS_PLAYING) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void on_cleardisk_advanced_clicked(LiVESWidget * widget, livespointer user_data) {
  // make cleardisk adv window

  // show various options and OK/Cancel button

  // on OK set clear_disk opts
  int response;
  LiVESWidget *dialog;
  do {
    dialog = create_cleardisk_advanced_dialog();
    lives_widget_show_all(dialog);
    response = lives_dialog_run(LIVES_DIALOG(dialog));
    lives_widget_destroy(dialog);
    if (response == LIVES_RESPONSE_RETRY) prefs->clear_disk_opts = 0;
  } while (response == LIVES_RESPONSE_RETRY);

  set_int_pref(PREF_CLEAR_DISK_OPTS, prefs->clear_disk_opts);
}


void on_show_keys_activate(LiVESMenuItem * menuitem, livespointer user_data) {do_keys_window();}


void on_vj_realize_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  frames_t ret;
  char *msg = (_("Pre-decoding all frames in this clip..."));
  d_print(msg);

  desensitize();

  ret = realize_all_frames(mainw->current_file, msg, TRUE);
  lives_free(msg);
  if (ret <= 0) d_print_failed();
  else if (ret < cfile->frames) d_print_enough(ret);
  else d_print_done();

  sensitize();
}


void on_vj_reset_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESList *clip_list = mainw->cliplist;

  boolean bad_header = FALSE;

  int i;

  //mainw->soft_debug=TRUE;

  do_threaded_dialog(_("Resetting frame rates and frame values..."), FALSE);

  while (clip_list != NULL) {
    i = LIVES_POINTER_TO_INT(clip_list->data);
    mainw->files[i]->pb_fps = mainw->files[i]->fps;
    mainw->files[i]->frameno = 1;
    mainw->files[i]->aseek_pos = 0;

    if (!save_clip_value(i, CLIP_DETAILS_PB_FPS, &mainw->files[i]->fps)) bad_header = TRUE;
    if (!save_clip_value(i, CLIP_DETAILS_PB_FRAMENO, &mainw->files[i]->frameno)) bad_header = TRUE;

    threaded_dialog_spin((double)i / (double)mainw->clips_available);

    if (bad_header) {
      if (!do_header_write_error(i)) break;
    } else clip_list = clip_list->next;
  }

  end_threaded_dialog();
}


void on_show_messages_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  do_messages_window(FALSE);
}


void on_show_file_info_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char buff[512];
  lives_clipinfo_t *filew;

  char *sigs, *ends, *tmp;

  if (!CURRENT_CLIP_IS_VALID) return;

  filew = create_clip_info_window(cfile->achans, FALSE);

  if (cfile->frames > 0) {
    // type
    lives_snprintf(buff, 512, _("\n\nExternal: %s\nInternal: %s (%d bpp) / %s"), cfile->type,
                   (tmp = lives_strdup((cfile->clip_type == CLIP_TYPE_YUV4MPEG ||
                                        cfile->clip_type == CLIP_TYPE_VIDEODEV) ? (_("buffered")) :
                                       (cfile->img_type == IMG_TYPE_JPEG ? LIVES_IMAGE_TYPE_JPEG : LIVES_IMAGE_TYPE_PNG))),
                   cfile->bpp, LIVES_AUDIO_TYPE_PCM);
    lives_free(tmp);

    if (cfile->clip_type == CLIP_TYPE_FILE) {
      lives_decoder_t *dplug = (lives_decoder_t *)cfile->ext_src;
      lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)dplug->decoder;
      const char *decname = dpsys->name;
      lives_strappendf(buff, 512, _("\ndecoder: %s"), decname);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_type), buff, -1);
    // fps
    lives_snprintf(buff, 512, "\n\n  %.3f%s", cfile->fps, cfile->ratio_fps ? "..." : "");

    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_fps), buff, -1);
    // image size
    lives_snprintf(buff, 512, "\n\n  %dx%d", cfile->hsize, cfile->vsize);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_size), buff, -1);
    // frames
    if ((cfile->opening && !cfile->opening_audio && cfile->frames == 0) || cfile->frames == 123456789) {
      lives_snprintf(buff, 512, "%s", _("\n\n  Opening..."));
    } else {
      lives_snprintf(buff, 512, "\n  %d", cfile->frames);

      if (cfile->frame_index != NULL) {
        int fvirt = count_virtual_frames(cfile->frame_index, 1, cfile->frames);
        char *tmp = lives_strdup_printf(_("\n(%d virtual)"), fvirt);
        lives_strappend(buff, 512, tmp);
        lives_free(tmp);
        tmp = lives_strdup_printf(_("\n(%d decoded)"), cfile->frames - fvirt);
        lives_strappend(buff, 512, tmp);
        lives_free(tmp);
      }

    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_frames), buff, -1);
    // video time
    if ((cfile->opening && !cfile->opening_audio && cfile->frames == 0) || cfile->frames == 123456789) {
      lives_snprintf(buff, 512, "%s", _("\n\n  Opening..."));
    } else {
      lives_snprintf(buff, 512, _("\n\n  %.2f sec."), cfile->video_time);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_vtime), buff, -1);
    // file size
    if (cfile->f_size > 0l) {
      char *file_ds = lives_format_storage_space_string((uint64_t)cfile->f_size);
      lives_snprintf(buff, 512, "\n\n  %s", file_ds);
      lives_free(file_ds);
    } else lives_snprintf(buff, 512, "%s", _("\n\n  Unknown"));
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_fsize), buff, -1);
  }

  if (cfile->achans > 0) {
    if (cfile->opening) {
      lives_snprintf(buff, 512, "%s", _("\n\n  Opening..."));
    } else {
      lives_snprintf(buff, 512, _("\n  %.2f sec."), cfile->laudio_time);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_ltime), buff, -1);

    if (cfile->signed_endian & AFORM_UNSIGNED) sigs = (_("unsigned"));
    else sigs = (_("signed"));

    if (cfile->signed_endian & AFORM_BIG_ENDIAN) ends = (_("big-endian"));
    else ends = (_("little-endian"));

    lives_snprintf(buff, 512, _("\n  %d Hz %d bit\n%s %s"), cfile->arate, cfile->asampsize, sigs, ends);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_lrate), buff, -1);

    lives_free(sigs);
    lives_free(ends);
  }

  if (cfile->achans > 1) {
    if (cfile->signed_endian & AFORM_UNSIGNED) sigs = (_("unsigned"));
    else sigs = (_("signed"));

    if (cfile->signed_endian & AFORM_BIG_ENDIAN) ends = (_("big-endian"));
    else ends = (_("little-endian"));

    lives_snprintf(buff, 512, _("\n  %d Hz %d bit\n%s %s"), cfile->arate, cfile->asampsize, sigs, ends);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_rrate), buff, -1);

    lives_free(sigs);
    lives_free(ends);

    if (cfile->opening) {
      lives_snprintf(buff, 512, "%s", _("\n  Opening..."));
    } else {
      lives_snprintf(buff, 512, _("\n  %.2f sec."), cfile->raudio_time);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_rtime), buff, -1);
  }
}


void on_show_file_comments_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  do_comments_dialog(mainw->current_file, NULL);
}


void on_show_clipboard_info_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  int current_file = mainw->current_file;
  mainw->current_file = 0;
  on_show_file_info_activate(menuitem, user_data);
  mainw->current_file = current_file;
}


void switch_clip(int type, int newclip, boolean force) {
  // generic switch clip callback

  // This is the new single entry function for switching clips.
  // It should eventually replace switch_to_file() and do_quick_switch()

  // type = 0 : if we are playing and a transition is active, this will change the background clip
  // type = 1 fg only
  // type = 2 bg only

  if (mainw->current_file < 1 || mainw->multitrack != NULL || mainw->preview || mainw->internal_messaging ||
      (mainw->is_processing && cfile != NULL && cfile->is_loaded) || mainw->cliplist == NULL) return;

  mainw->blend_palette = WEED_PALETTE_END;

  if (type == 2 || (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND && mainw->playing_file > 0 && type != 1
                    && !(!IS_NORMAL_CLIP(mainw->blend_file) && mainw->blend_file != mainw->playing_file))) {
    if (mainw->num_tr_applied < 1 || newclip == mainw->blend_file) return;

    // switch bg clip
    if (IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != mainw->playing_file
        && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
      if (mainw->blend_layer != NULL) check_layer_ready(mainw->blend_layer);
      weed_plant_t *inst = mainw->files[mainw->blend_file]->ext_src;
      if (inst != NULL) {
        mainw->osc_block = TRUE;
        if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_KEY)) {
          int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
          rte_key_on_off(key + 1, FALSE);
        }
        mainw->osc_block = FALSE;
      }
    }

    //chill_decoder_plugin(mainw->blend_file);
    mainw->blend_file = newclip;
    mainw->whentostop = NEVER_STOP;
    if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_BACKGROUND) {
      ce_thumbs_highlight_current_clip();
    }
    mainw->blend_palette = WEED_PALETTE_END;
    return;
  }

  // switch fg clip

  if (!force && (newclip == mainw->current_file && (!LIVES_IS_PLAYING || mainw->playing_file == newclip))) return;
  if (cfile && !cfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;

  if (LIVES_IS_PLAYING) {
    mainw->new_clip = newclip;
    mainw->blend_palette = WEED_PALETTE_END;
  } else {
    if (!cfile || (force && newclip == mainw->current_file)) mainw->current_file = -1;
    switch_to_file(mainw->current_file, newclip);
  }
}


void switch_clip_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // switch clips from the clips menu

  register int i;
  if (mainw->current_file < 1 || mainw->preview || (mainw->is_processing && cfile->is_loaded) || mainw->cliplist == NULL) return;

  for (i = 1; i < MAX_FILES; i++) {
    if (mainw->files[i] != NULL) {
      if (LIVES_MENU_ITEM(menuitem) == LIVES_MENU_ITEM(mainw->files[i]->menuentry) &&
          lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->files[i]->menuentry))) {
        switch_clip(0, i, FALSE);
        return;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void on_about_activate(LiVESMenuItem * menuitem, livespointer user_data) {

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  char *license = (_(
                     "This program is free software; you can redistribute it and/or modify\n"
                     "it under the terms of the GNU General Public License as published by\n"
                     "the Free Software Foundation; either version 3 of the License, or\n"
                     "(at your option) any later version.\n"
                     "\n"
                     "This program is distributed in the hope that it will be useful,\n"
                     "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                     "GNU General Public License for more details.\n"
                     "\n"
                     "You should have received a copy of the GNU General Public License\n"
                     "along with this program; if not, write to the Free Software\n"
                     "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA.\n"));

  char *comments = (_("A video editor and VJ program."));
  char *title = (_("About LiVES"));

  char *translator_credits = (_("translator_credits"));

#if GTK_CHECK_VERSION(3, 0, 0)
  char *authors[2] = {LIVES_AUTHOR_EMAIL, NULL};
#else
  gtk_about_dialog_set_url_hook(activate_url, NULL, NULL);
  gtk_about_dialog_set_email_hook(activate_url, NULL, NULL);
#endif

  gtk_show_about_dialog(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET),
                        "logo", NULL,
                        "name", PACKAGE_NAME,
                        "version", LiVES_VERSION,
                        "comments", comments,
                        "copyright", "(C) "LIVES_COPYRIGHT_YEARS" salsaman <"LIVES_AUTHOR_EMAIL"> and others",
                        "website", LIVES_WEBSITE,
                        "license", license,
                        "title", title,
                        "translator_credits", translator_credits,
#if GTK_CHECK_VERSION(3, 0, 0)
                        "authors", authors,
                        "license-type", GTK_LICENSE_GPL_3_0,
#endif
                        NULL);

  lives_free(translator_credits);
  lives_free(comments);
  lives_free(title);
  lives_free(license);
  return;
#endif
#endif
  widget_opts.non_modal = TRUE;
  do_error_dialogf(_("LiVES Version %s\n"
                     "(c) G. Finch (salsaman) %s\n\n"
                     "Released under the GPL 3 or later (http://www.gnu.org/licenses/gpl.txt)\n"
                     "LiVES is distributed WITHOUT WARRANTY\n\n"
                     "Contact the author at:\n%s\n"
                     "Homepage: %s"),
                   LiVES_VERSION, LIVES_COPYRIGHT_YEARS, LIVES_AUTHOR_EMAIL, LIVES_WEBSITE);
  widget_opts.non_modal = FALSE;
}


void show_manual_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  show_manual_section(NULL, NULL);
}


void email_author_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  activate_url_inner("mailto:"LIVES_AUTHOR_EMAIL);
}


void report_bug_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  activate_url_inner(LIVES_BUG_URL);
}


void suggest_feature_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  activate_url_inner(LIVES_FEATURE_URL);
}


void help_translate_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  activate_url_inner(LIVES_TRANSLATE_URL);
}


void donate_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  const char *link = lives_strdup_printf("%s%s", LIVES_DONATE_URL, user_data == NULL ? "" : (char *)user_data);
  activate_url_inner(link);
}


static char *fsp_ltext;
static char *fsp_info_file;
static char *file_open_params;

static boolean fs_preview_idle(void *data) {
  LiVESWidget *widget = (LiVESWidget *)data;
  FILE *ifile;

  if ((!(ifile = fopen(fsp_info_file, "r"))) && mainw->in_fs_preview && mainw->fc_buttonresponse == LIVES_RESPONSE_NONE) {
    return TRUE;
  }

  widget_opts.expand = LIVES_EXPAND_NONE;

  if (LIVES_IS_BUTTON(widget))
    lives_button_set_label(LIVES_BUTTON(widget), fsp_ltext);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_free(fsp_ltext);

  if (ifile != NULL) {
    fclose(ifile);
  }

  end_fs_preview();
  lives_free(fsp_info_file);

  lives_freep((void **)&file_open_params);
  if (LIVES_IS_WIDGET(widget))
    lives_widget_set_sensitive(widget, TRUE);
  return FALSE;
}



void on_fs_preview_clicked(LiVESWidget * widget, livespointer user_data) {
  // file selector preview
  double start_time = 0.;

  uint64_t xwin = 0;

  char **array;

  int preview_frames = 0;
  int preview_type = LIVES_POINTER_TO_INT(user_data);
  int height = 0, width = 0;
  int fwidth = -1, fheight = -1;
  int owidth, oheight, npieces, border = 0;

  pid_t pid = capable->mainpid;

  char *thm_dir;
  char *tmp, *tmp2;
  char *com;
  char *dfile;
  char *type;

  file_open_params = NULL;

  if (mainw->in_fs_preview) {
    end_fs_preview();
    return;
  }
  fsp_ltext = NULL;

  /* if (mainw->in_fs_preview) { */
  /*   dfile = lives_strdup_printf("thm%d", pid); */
  /*   lives_kill_subprocesses(dfile, TRUE); */
  /*   lives_free(dfile); */
  /* } */

  if (preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    // open selection
    start_time = mainw->fx1_val;
    preview_frames = (int)mainw->fx2_val;
  } else {
    // open file
    lives_snprintf(file_name, PATH_MAX, "%s",
                   (tmp = lives_filename_to_utf8((tmp2
                          = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(lives_widget_get_toplevel(widget)))),
                          -1, NULL, NULL, NULL)));
    lives_free(tmp);
    lives_free(tmp2);
  }

  // get file detaisl
  if (!read_file_details_generic(file_name)) return;

  npieces = get_token_count(mainw->msg, '|');
  if (npieces < 4) {
    end_fs_preview();
    return;
  }
  array = lives_strsplit(mainw->msg, "|", npieces);
  type = lives_strdup(array[3]);

  if (npieces > 5) {
    width = atoi(array[4]);
    height = atoi(array[5]);
  }
  lives_strfreev(array);

  if (!strcmp(type, "image") || !strcmp(type, prefs->image_type)) {
    /* info_file = lives_strdup_printf("%s/thm%d/%s", prefs->workdir, capable->mainpid, LIVES_STATUS_FILE_NAME); */
    /* lives_rm(info_file); */

    if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_IMAGE_ONLY) {
      clear_mainw_msg();

      if (capable->has_identify) {
        mainw->error = FALSE;

        fwidth = lives_widget_get_allocation_width(mainw->fs_playalign) - 20;
        fheight = lives_widget_get_allocation_height(mainw->fs_playalign) - 20;

        // make thumb from any image file
        com = lives_strdup_printf("%s make_thumb thm%d %d %d \"%s\" \"%s\"", prefs->backend_sync, pid, fwidth,
                                  fheight, prefs->image_ext, (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));
        lives_free(tmp);

        lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
        lives_free(com);

        npieces = get_token_count(mainw->msg, '|');
        if (npieces < 3) {
          THREADVAR(com_failed) = FALSE;
          end_fs_preview();
          return;
        }
        if (!THREADVAR(com_failed)) {
          array = lives_strsplit(mainw->msg, "|", 3);
          width = atoi(array[1]);
          height = atoi(array[2]);
          lives_strfreev(array);
        } else height = width = 0;
        THREADVAR(com_failed) = FALSE;
      } else {
        height = width = 0;
      }

      if (height > 0 && width > 0) {
        // draw image
        LiVESXWindow *xwin;
        LiVESError *error = NULL;
        char *thumb = lives_strdup_printf("%s/thm%d/%08d.%s", prefs->workdir, pid, 1, prefs->image_ext);
        LiVESPixbuf *pixbuf = lives_pixbuf_new_from_file((tmp = lives_filename_from_utf8(thumb, -1, NULL, NULL, NULL)), &error);
        lives_free(thumb);
        lives_free(tmp);
        if (error == NULL) {
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->fs_playimg), "pixbuf", pixbuf);
          owidth = width;
          oheight = height;

          calc_maxspect(fwidth, fheight, &width, &height);

          width = (width >> 1) << 1;
          height = (height >> 1) << 1;

          if (width > owidth || height > oheight) {
            width = owidth;
            height = oheight;
          }

          if (width < 4 || height < 4) {
            end_fs_preview();
            lives_widget_set_sensitive(widget, TRUE);
            return;
          }
          lives_widget_set_size_request(mainw->fs_playarea, width, height);
          lives_alignment_set(mainw->fs_playalign, 0.5, 0.5, (float)width / (float)fwidth,
                              (float)height / (float)fheight);
          border = MIN(fwidth - width, fheight - height);
          lives_container_set_border_width(LIVES_CONTAINER(mainw->fs_playarea), border >> 1);
          lives_widget_show(mainw->fs_playimg);
          lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
          xwin = lives_widget_get_xwindow(mainw->fs_playimg);
          if (LIVES_IS_XWINDOW(xwin)) {
            if (mainw->fsp_surface) lives_painter_surface_destroy(mainw->fsp_surface);
            mainw->fsp_surface =
              lives_xwindow_create_similar_surface(xwin,
                                                   LIVES_PAINTER_CONTENT_COLOR,
                                                   width, height);
            if (mainw->fsp_func == 0)
              mainw->fsp_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->fs_playimg), LIVES_WIDGET_EXPOSE_EVENT,
                                LIVES_GUI_CALLBACK(all_expose),
                                &mainw->fsp_surface);
          }
          set_drawing_area_from_pixbuf(mainw->fs_playimg, pixbuf, mainw->fsp_surface);
        } else {
          lives_error_free(error);
        }
      }

      thm_dir = lives_strdup_printf("%s/thm%d", prefs->workdir, capable->mainpid);
      lives_rmdir(thm_dir, TRUE);
      lives_free(thm_dir);
      if (height > 0 || width > 0 || preview_type == LIVES_PREVIEW_TYPE_IMAGE_ONLY) {
        lives_widget_set_sensitive(widget, TRUE);
        return;
      }
    }
    return;
  }

  if (!HAS_EXTERNAL_PLAYER) {
    char *msg;
    if (capable->has_identify) {
      msg = (_("\n\nYou need to install mplayer, mplayer2 or mpv to be able to preview this file.\n"));
    } else {
      msg = (_("\n\nYou need to install mplayer, mplayer2, mpv or imageMagick to be able to preview this file.\n"));
    }
    do_error_dialog(msg);
    lives_free(msg);
    lives_widget_set_sensitive(widget, TRUE);
    return;
  }

  dfile = lives_strdup_printf("%s" LIVES_DIR_SEP "fsp%d" LIVES_DIR_SEP, prefs->workdir, capable->mainpid);
  if (!lives_make_writeable_dir(dfile)) {
    workdir_warning();
    lives_free(dfile);
    lives_widget_set_sensitive(widget, TRUE);
    return;
  }

  fsp_info_file = lives_strdup_printf("%s%s", dfile, LIVES_STATUS_FILE_NAME);
  lives_free(dfile);

  mainw->in_fs_preview = TRUE;

  if (preview_type != LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
    if (!strcmp(type, "Audio")) {
      preview_frames = -1;
    } else {
      if (height == 0 || width == 0) {
        width = DEF_FRAME_HSIZE / 2;
        height = DEF_FRAME_VSIZE / 2;
      }

      owidth = width;
      oheight = height;

      fwidth = lives_widget_get_allocation_width(mainw->fs_playframe) - 20;
      fheight = lives_widget_get_allocation_height(mainw->fs_playframe) - 20;

      calc_maxspect(fwidth, fheight, &width, &height);

      width = (width >> 1) << 1;
      height = (height >> 1) << 1;

      if (width > owidth || height > oheight) {
        width = owidth;
        height = oheight;
      }

      if (width < 4 || height < 4) {
        end_fs_preview();
        lives_widget_set_sensitive(widget, TRUE);
        return;
      }
      lives_widget_set_bg_color(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, &palette->black);
      lives_widget_set_size_request(mainw->fs_playarea, width, height);
      lives_alignment_set(mainw->fs_playalign, 0.5, 0.5, (float)width / (float)fwidth,
                          (float)height / (float)fheight);
      border = MIN(fwidth - width, fheight - height);
      lives_container_set_border_width(LIVES_CONTAINER(mainw->fs_playarea), border >> 1);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    }
  }  else preview_frames = -1;

  if (!USE_MPV) {
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      file_open_params = lives_strdup_printf("%s %s -ao jack", mainw->file_open_params != NULL ?
                                             mainw->file_open_params : "", get_deinterlace_string());
    } else if (prefs->audio_player == AUD_PLAYER_PULSE) {
      file_open_params = lives_strdup_printf("%s %s -ao pulse", mainw->file_open_params != NULL ?
                                             mainw->file_open_params : "", get_deinterlace_string());
    } else {
      file_open_params = lives_strdup_printf("%s %s -ao null", mainw->file_open_params != NULL ?
                                             mainw->file_open_params : "", get_deinterlace_string());
    }
  } else {
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      file_open_params = lives_strdup_printf("%s %s --ao=jack", mainw->file_open_params != NULL ?
                                             mainw->file_open_params : "", get_deinterlace_string());
    } else if (prefs->audio_player == AUD_PLAYER_PULSE) {
      file_open_params = lives_strdup_printf("%s %s --ao=pulse", mainw->file_open_params != NULL ?
                                             mainw->file_open_params : "", get_deinterlace_string());
    } else {
      file_open_params = lives_strdup_printf("%s %s --ao=null", mainw->file_open_params != NULL ?
                                             mainw->file_open_params : "", get_deinterlace_string());
    }
  }

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    xwin = lives_widget_get_xwinid(mainw->fs_playarea, "Unsupported display type for preview.");
    if (xwin == -1) {
      end_fs_preview();
      lives_free(fsp_info_file);
      lives_widget_set_sensitive(widget, TRUE);
      return;
    }
  }

  if (file_open_params != NULL) {
    com = lives_strdup_printf("%s fs_preview fsp%d %"PRIu64" %d %d %.2f %d %d \"%s\" \"%s\"", prefs->backend, capable->mainpid,
                              xwin, width, height, start_time, preview_frames, (int)(prefs->volume * 100.),
                              (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)), file_open_params);

  } else {
    com = lives_strdup_printf("%s fs_preview fsp%d %"PRIu64" %d %d %.2f %d %d \"%s\"", prefs->backend, capable->mainpid,
                              xwin, width, height, start_time, preview_frames, (int)(prefs->volume * 100.),
                              (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));
  }

  lives_free(tmp);
  mainw->in_fs_preview = TRUE;
  lives_rm(fsp_info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    end_fs_preview();
    lives_free(fsp_info_file);
    lives_widget_set_sensitive(widget, TRUE);
    return;
  }

  tmp = (_("\nStop Preview\n"));
  fsp_ltext = lives_strdup(lives_button_get_label(LIVES_BUTTON(widget)));
  widget_opts.expand = LIVES_EXPAND_NONE;
  lives_button_set_label(LIVES_BUTTON(widget), tmp);
  if (preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
  }
  lives_free(tmp);

  // loop here until preview has finished, or the user presses OK or Cancel
  lives_idle_add_simple(fs_preview_idle, (livespointer)widget);
}


void on_open_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // OPEN A FILE (single or multiple)
  LiVESWidget *chooser;
  LiVESResponseType resp;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  chooser = choose_file_with_preview((*mainw->vid_load_dir) ? mainw->vid_load_dir : NULL, NULL, NULL,
                                     LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI);

  resp = lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();

  if (resp == LIVES_RESPONSE_ACCEPT) on_ok_file_open_clicked(LIVES_FILE_CHOOSER(chooser), NULL);
  else on_filechooser_cancel_clicked(chooser);
}


void on_ok_file_open_clicked(LiVESFileChooser * chooser, LiVESSList * fnames) {
  // this is also called from drag target

  LiVESSList *ofnames;

  if (chooser) {
    fnames = lives_file_chooser_get_filenames(chooser);

    lives_widget_destroy(LIVES_WIDGET(chooser));

    if (!fnames) return;

    if (!fnames->data) {
      lives_list_free_all((LiVESList **)&fnames);
      return;
    }

    lives_snprintf(mainw->vid_load_dir, PATH_MAX, "%s", (char *)fnames->data);
    get_dirname(mainw->vid_load_dir);

    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    if (prefs->save_directories) {
      set_utf8_pref(PREF_VID_LOAD_DIR, mainw->vid_load_dir);
    }

    mainw->cancelled = CANCEL_NONE;
  }

  ofnames = fnames;
  mainw->img_concat_clip = -1;

  while (fnames != NULL && mainw->cancelled == CANCEL_NONE) {
    lives_snprintf(file_name, PATH_MAX, "%s", (char *)fnames->data);
    lives_free((livespointer)fnames->data);
    open_file(file_name);
    fnames = fnames->next;
  }

  lives_slist_free(ofnames);

  mainw->opening_multi = FALSE;
  mainw->img_concat_clip = -1;

  if (mainw->multitrack) {
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
}


#ifdef GUI_GTK
// TODO
// files dragged onto target from outside - try to open them
void drag_from_outside(LiVESWidget * widget, GdkDragContext * dcon, int x, int y,
                       GtkSelectionData * data, uint32_t info, uint32_t time, livespointer user_data) {
  GSList *fnames = NULL;
#if GTK_CHECK_VERSION(3, 0, 0)
  char *filelist = (char *)gtk_selection_data_get_data(data);
#else
  char *filelist = (char *)data->data;
#endif
  char *nfilelist, **array;
  int nfiles, i;

  if (filelist == NULL) {
    gtk_drag_finish(dcon, FALSE, FALSE, time);
    return;
  }

  if ((mainw->multitrack && !lives_widget_is_sensitive(mainw->multitrack->open_menu)) ||
      (mainw->multitrack == NULL && !lives_widget_is_sensitive(mainw->open))) {
    gtk_drag_finish(dcon, FALSE, FALSE, time);
    return;
  }

  nfilelist = subst(filelist, "file://", "");

  nfiles = get_token_count(nfilelist, '\n');
  array = lives_strsplit(nfilelist, "\n", nfiles);
  lives_free(nfilelist);

  for (i = 0; i < nfiles; i++) {
    fnames = lives_slist_append(fnames, array[i]);
  }

  on_ok_file_open_clicked(NULL, fnames);

  // fn will free array elements and fnames

  lives_free(array);

  gtk_drag_finish(dcon, TRUE, FALSE, time);
}
#endif


void on_opensel_range_ok_clicked(LiVESButton * button, livespointer user_data) {
  // open file selection
  boolean needs_idlefunc;

  end_fs_preview();

  needs_idlefunc = mainw->mt_needs_idlefunc;
  mainw->mt_needs_idlefunc = FALSE;
  lives_general_button_clicked(button, NULL);
  mainw->mt_needs_idlefunc = needs_idlefunc;

  mainw->img_concat_clip = -1;
  open_file_sel(file_name, mainw->fx1_val, (int)mainw->fx2_val);

  if (mainw->multitrack != NULL) {
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
}


void end_fs_preview(void) {
  // clean up if we were playing a preview - should be called from all callbacks
  // where there is a possibility of fs preview still playing
  char *com;

  lives_widget_set_bg_color(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  if (mainw->fs_playarea != NULL) {
    LiVESPixbuf *pixbuf = NULL;
    pixbuf = (LiVESPixbuf *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea), "pixbuf");
    if (pixbuf != NULL) lives_widget_object_unref(pixbuf);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea), "pixbuf", NULL);
    if (mainw->fsp_func != 0) {
      lives_signal_handler_disconnect(mainw->fs_playimg, mainw->fsp_func);
      mainw->fsp_func = 0;
    }
    if (mainw->fsp_surface) {
      lives_painter_surface_destroy(mainw->fsp_surface);
      mainw->fsp_surface = NULL;
    }
    lives_widget_hide(mainw->fs_playimg);
  }

  if (mainw->in_fs_preview) {
    char *tmp = lives_strdup_printf("fsp%d", capable->mainpid);
    char *permitname = lives_build_filename(prefs->workdir, tmp, TEMPFILE_MARKER "," LIVES_FILE_EXT_TMP, NULL);
    lives_kill_subprocesses(tmp, TRUE);
    lives_touch(permitname);
    lives_free(permitname);
    com = lives_strdup_printf("%s close \"%s\"", prefs->backend, tmp);
    lives_free(tmp);
    lives_system(com, TRUE);
    lives_free(com);
    mainw->in_fs_preview = FALSE;
  }
  if (mainw->fs_playframe) lives_widget_queue_draw(mainw->fs_playframe);
}


void on_save_textview_clicked(LiVESButton * button, livespointer user_data) {
  LiVESTextView *textview = (LiVESTextView *)user_data;
  char *filt[] = {"*." LIVES_FILE_EXT_TEXT, NULL};
  int fd;
  char *btext;
  char *save_file;
  boolean needs_idlefunc;

  lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(button)));
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  save_file = choose_file(NULL, NULL, filt, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);

  if (!save_file) {
    lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    return;
  }

#ifndef IS_MINGW
  if ((fd = creat(save_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1) {
#else
  if ((fd = creat(save_file, S_IRUSR | S_IWUSR)) == -1) {
#endif
    lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    do_write_failed_error_s(save_file, lives_strerror(errno));
    lives_free(save_file);
    return;
  }

  btext = lives_text_view_get_text(textview);

  needs_idlefunc = mainw->mt_needs_idlefunc;
  mainw->mt_needs_idlefunc = FALSE;
  lives_general_button_clicked(button, NULL);
  mainw->mt_needs_idlefunc = needs_idlefunc;

  THREADVAR(write_failed) = FALSE;
  lives_write(fd, btext, strlen(btext), FALSE);
  lives_free(btext);

  close(fd);

  if (THREADVAR(write_failed)) {
    do_write_failed_error_s(save_file, lives_strerror(errno));
  } else {
    char *msg = lives_strdup_printf(_("Text was saved as\n%s\n"), save_file);
    do_error_dialog(msg);
    lives_free(msg);
  }

  lives_free(save_file);
#if 0
}
#endif
}


void on_filechooser_cancel_clicked(LiVESWidget * widget) {
  lives_widget_destroy(widget);

  if (mainw->multitrack != NULL) {
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  } else if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && !cfile->opening) {
    get_play_times();
  }
}


void on_cancel_opensel_clicked(LiVESButton * button, livespointer user_data) {
  boolean needs_idlefunc;

  end_fs_preview();
  needs_idlefunc = mainw->mt_needs_idlefunc;
  mainw->mt_needs_idlefunc = FALSE;
  lives_general_button_clicked(button, NULL);
  mainw->mt_needs_idlefunc = needs_idlefunc;

  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
  lives_menu_item_activate(LIVES_MENU_ITEM(mainw->open_sel)); // returm to the fileselector
}


void on_cancel_keep_button_clicked(LiVESButton * button, livespointer user_data) {
  // Cancel/Keep from progress dialog
  char *com = NULL;

  uint32_t keep_frames = 0;

  boolean killprocs = FALSE;

  if (CURRENT_CLIP_IS_VALID && cfile->opening && mainw->effects_paused) {
    on_stop_clicked(NULL, NULL);
    return;
  }

  clear_mainw_msg();

  if (CURRENT_CLIP_IS_VALID && mainw->proc_ptr) {
    lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, FALSE);
    lives_widget_set_sensitive(mainw->proc_ptr->pause_button, FALSE);
    if (mainw->proc_ptr->stop_button)
      lives_widget_set_sensitive(mainw->proc_ptr->stop_button, FALSE);
    lives_widget_set_sensitive(mainw->proc_ptr->preview_button, FALSE);
  }
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  if ((!mainw->effects_paused || cfile->nokeep) && (!mainw->multitrack ||
      (mainw->multitrack && (!mainw->multitrack->is_rendering ||
                             !mainw->preview)))) {
    // Cancel
    if (mainw->cancel_type == CANCEL_SOFT) {
      // cancel in record audio
      mainw->cancelled = CANCEL_USER;
      d_print_cancelled();
      return;
    } else if (mainw->cancel_type == CANCEL_KILL) {
      // kill processes and subprocesses working on cfile
      killprocs = TRUE;
    }

    if (CURRENT_CLIP_IS_VALID && !cfile->opening && !mainw->internal_messaging) {
      // if we are opening, this is 'stop' in the preview, so don't cancel
      // otherwise, come here

      // kill off the background process
      if (killprocs) {
        lives_kill_subprocesses(cfile->handle, TRUE);
      }

      // resume for next time
      if (mainw->effects_paused) {
        lives_freep((void **)&com);
        com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, cfile->handle);
        lives_system(com, FALSE);
      }
    }

    mainw->cancelled = CANCEL_USER;

    if (mainw->is_rendering) {
      if (CURRENT_CLIP_IS_VALID) cfile->frames = 0;
      d_print_cancelled();
    } else {
      // see if there was a message from backend

      if (mainw->cancel_type != CANCEL_SOFT) {
        lives_fread_string(mainw->msg, MAINW_MSG_SIZE, cfile->info_file);
        if (lives_strncmp(mainw->msg, "completed", 9)) {
          d_print_cancelled();
        } else {
          // processing finished before we could cancel
          mainw->cancelled = CANCEL_NONE;
        }
      } else d_print_cancelled();
    }
  } else {
    // Keep
    if (mainw->cancel_type == CANCEL_SOFT) {
      mainw->cancelled = CANCEL_KEEP;
      return;
    }
    if (!mainw->is_rendering) {
      keep_frames = mainw->proc_ptr->frames_done - cfile->progress_start
                    + cfile->start - 1 + mainw->internal_messaging * 2;
      if (mainw->internal_messaging && atoi(mainw->msg) > mainw->proc_ptr->frames_done)
        keep_frames = atoi(mainw->msg) - cfile->progress_start + cfile->start - 1 + 2;
    } else keep_frames = cfile->frames + 1;
    if (keep_frames > mainw->internal_messaging) {
      d_print_enough(keep_frames - cfile->start);
      lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
      if (!mainw->internal_messaging) {
        lives_kill_subprocesses(cfile->handle, TRUE);
        com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, cfile->handle);
        lives_system(com, FALSE);
        lives_free(com);
        if (!mainw->keep_pre) com = lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle,
                                      cfile->start, keep_frames - 1, get_image_ext_for_type(cfile->img_type));
        else {
          com = lives_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\" &", prefs->backend_sync, cfile->handle,
                                    cfile->start, keep_frames - 1, get_image_ext_for_type(cfile->img_type));
          mainw->keep_pre = FALSE;
        }
      } else {
        mainw->internal_messaging = FALSE;
        if (!mainw->keep_pre) com = lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle,
                                      cfile->start, keep_frames - 1, get_image_ext_for_type(cfile->img_type));
        else {
          com = lives_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\" &", prefs->backend_sync, cfile->handle,
                                    cfile->start, keep_frames - 1, get_image_ext_for_type(cfile->img_type));
          mainw->keep_pre = FALSE;
        }
      }
      if (!mainw->is_rendering || mainw->multitrack) {
        lives_rm(cfile->info_file);
        lives_system(com, FALSE);
        cfile->undo_end = keep_frames - 1;
      } else mainw->cancelled = CANCEL_KEEP;
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    } else {
      // no frames there, nothing to keep
      d_print_cancelled();

      if (!mainw->internal_messaging && !mainw->is_rendering) {
        lives_kill_subprocesses(cfile->handle, TRUE);
        com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, cfile->handle);
        lives_system(com, FALSE);
      }
      mainw->cancelled = CANCEL_USER;
    }
  }

  lives_freep((void **)&com);
}


void on_details_button_clicked(void) {
  text_window *textwindow;
  widget_opts.expand = LIVES_EXPAND_EXTRA;
  textwindow = create_text_window(_("Encoder Debug Output"),
                                  lives_text_view_get_text(mainw->optextview), NULL, TRUE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_widget_show_all(textwindow->dialog);
}


void on_full_screen_pressed(LiVESButton * button, livespointer user_data) {
  // toolbar button (full screen)
  // ignore if audio only clip
  if (CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_HAS_VIDEO && !mainw->multitrack) return;
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->full_screen), !mainw->fs);
}


static void _on_full_screen_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *fs_img;

  // ignore if audio only clip
  if (CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_HAS_VIDEO && LIVES_IS_PLAYING && !mainw->multitrack) return;

  if (!user_data) {
    // toggle can be overridden by setting user_data non-NULL
    mainw->fs = !mainw->fs;
  }
  mainw->blend_palette = WEED_PALETTE_END;

  // update the button icon
  fs_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_FULLSCREEN,
                                      lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  lives_widget_show(fs_img);
  if (!mainw->fs) {
    if (LIVES_IS_IMAGE(fs_img)) {
      LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(fs_img));
      lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
    }
    lives_widget_set_tooltip_text(mainw->t_fullscreen, _("Fullscreen playback (f)"));
  } else lives_widget_set_tooltip_text(mainw->t_fullscreen, _("Fullscreen playback off (f)"));

  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_fullscreen), fs_img);

  if (LIVES_IS_PLAYING) {
    if (mainw->fs) {
      // switch TO full screen during pb
      if (!mainw->multitrack && !mainw->sep_win) {
        fade_background();
        fullscreen_internal();
      }
      if (mainw->sep_win) {
        resize_play_window();
        lives_window_set_decorated(LIVES_WINDOW(mainw->play_window), FALSE);
      }
      if (cfile->frames == 1 || cfile->play_paused) {
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      }

      if (mainw->ext_playback && mainw->vpp->fheight > -1 && mainw->vpp->fwidth > -1) {
        // fixed o/p size for stream
        if (mainw->vpp->fwidth == 0 || mainw->vpp->fheight == 0) {
          mainw->vpp->fwidth = cfile->hsize;
          mainw->vpp->fheight = cfile->vsize;
        }
        mainw->pwidth = mainw->vpp->fwidth;
        mainw->pheight = mainw->vpp->fheight;
      }
    } else {
      // switch from fullscreen during pb
      if (mainw->sep_win) {
        // separate window
        if (prefs->show_desktop_panel && (capable->wm_caps.pan_annoy & ANNOY_DISPLAY)
            && (capable->wm_caps.pan_annoy & ANNOY_FS) && (capable->wm_caps.pan_res & RES_HIDE) &&
            capable->wm_caps.pan_res & RESTYPE_ACTION) {
          show_desktop_panel();
        }
        if (mainw->ext_playback) {
#ifndef IS_MINGW
          vid_playback_plugin_exit();
          lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
#else
          lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
          vid_playback_plugin_exit();
#endif
        } else {
          // multi monitors don't like this it seems, breaks the window
          lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
        }

        if (!mainw->faded) unfade_background();
        resize_play_window();

        if (!mainw->multitrack && mainw->opwx > -1) {
          //opwx and opwy were stored when we first switched to full screen
          lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->opwx, mainw->opwy);
          mainw->opwx = mainw->opwy = -1;
        } else {
          if (mainw->play_window) {
            lives_window_center(LIVES_WINDOW(mainw->play_window));
            hide_cursor(lives_widget_get_xwindow(mainw->play_window));
            lives_widget_set_app_paintable(mainw->play_window, TRUE);
          }
        }
        if (!mainw->multitrack) {
          lives_window_set_decorated(LIVES_WINDOW(mainw->play_window), TRUE);
          if (cfile->frames == 1 || cfile->play_paused) {
            lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
          }
        }
      } else {
        // switch FROM fullscreen during pb
        // in frame window
        if (!mainw->multitrack) {
          if (!mainw->faded) {
            lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), FALSE);
            if (mainw->double_size) {
              lives_widget_hide(mainw->sep_image);
              lives_widget_hide(mainw->message_box);
            }
            unfade_background();
          } else {
            lives_widget_hide(mainw->frame1);
            lives_widget_hide(mainw->frame2);
            lives_widget_show(mainw->eventbox3);
            lives_widget_show(mainw->eventbox4);
            fade_background();
            if (mainw->double_size) {
              lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), FALSE);
              resize(2.);
            } else {
              lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
              resize(1.);
            }
          }

          lives_widget_set_sensitive(mainw->fade, TRUE);
          lives_widget_set_sensitive(mainw->dsize, TRUE);

          lives_widget_show(mainw->t_bckground);
          lives_widget_show(mainw->t_double);
	    // *INDENT-OFF*
	}}
      if (!mainw->multitrack && !mainw->faded) {
	if (CURRENT_CLIP_IS_VALID) {
	  redraw_timeline(mainw->current_file);
	  show_playbar_labels(mainw->current_file);
	}}}
    // *INDENT-ON*
    mainw->force_show = TRUE;
  } else {
    // not playing
    if (!mainw->multitrack) {
      if (mainw->fs) {
        lives_widget_set_sensitive(mainw->fade, FALSE);
        lives_widget_set_sensitive(mainw->dsize, FALSE);
      } else {
        lives_widget_set_sensitive(mainw->fade, TRUE);
        lives_widget_set_sensitive(mainw->dsize, TRUE);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void on_full_screen_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  main_thread_execute((lives_funcptr_t)_on_full_screen_activate, 0, NULL, "vv", menuitem, user_data);
}

void on_double_size_pressed(LiVESButton * button, livespointer user_data) {
  // toolbar button (separate window)
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->dsize), !mainw->double_size);
}


void on_double_size_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *sngl_img;

  if (mainw->multitrack || (CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_HAS_VIDEO && !user_data)) return;

  if (!user_data) {
    mainw->double_size = !mainw->double_size;
  }

  if (!CURRENT_CLIP_IS_VALID) return;
  mainw->blend_palette = WEED_PALETTE_END;

  if (user_data) {
    // change the blank window icons
    if (!mainw->double_size) {
      sngl_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_ZOOM_IN,
                                            lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
      lives_widget_set_tooltip_text(mainw->t_double, _("Double size (d)"));
    } else {
      sngl_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_ZOOM_OUT,
                                            lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
      lives_widget_set_tooltip_text(mainw->t_double, _("Single size (d)"));
    }

    if (LIVES_IS_IMAGE(sngl_img)) {
      LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(sngl_img));
      if (pixbuf) lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
    }

    lives_widget_show(sngl_img);
    lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_double), sngl_img);
  }

  mainw->opwx = mainw->opwy = -1;

  if ((LIVES_IS_PLAYING && !mainw->fs) || (!LIVES_IS_PLAYING && mainw->play_window)) {
    mainw->pwidth = DEF_FRAME_HSIZE - H_RESIZE_ADJUST;
    mainw->pheight = DEF_FRAME_VSIZE - V_RESIZE_ADJUST;

    if (mainw->play_window) {
      resize_play_window();
      sched_yield();
      if (!mainw->double_size) lives_window_center(LIVES_WINDOW(mainw->play_window));
    } else {
      // in-frame
      if (mainw->double_size) {
        if (!mainw->faded) {
          if (palette->style & STYLE_1) {
            lives_widget_hide(mainw->sep_image);
          }
          lives_widget_hide(mainw->message_box);
        }
        lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), FALSE);
        resize(2.);
      } else {
        lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
        resize(1.);
        if (!mainw->faded) {
          if (palette->style & STYLE_1) {
            lives_widget_show_all(mainw->sep_image);
          }
          lives_widget_show_all(mainw->message_box);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
  if (LIVES_IS_PLAYING && !mainw->fs) mainw->force_show = TRUE;
}


void on_sepwin_pressed(LiVESButton * button, livespointer user_data) {
  if (mainw->go_away) return;

  // toolbar button (separate window)
  if (mainw->multitrack) {
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->sepwin), !mainw->sep_win);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin), mainw->sep_win);
  } else lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin), !mainw->sep_win);
}


void on_sepwin_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *sep_img;
  LiVESWidget *sep_img2;

  if (mainw->go_away) return;

  mainw->sep_win = !mainw->sep_win;
  mainw->blend_palette = WEED_PALETTE_END;

  if (mainw->multitrack) {
    if (!LIVES_IS_PLAYING) return;
    unpaint_lines(mainw->multitrack);
    mainw->multitrack->redraw_block = TRUE; // stop pb cursor from updating
    mt_show_current_frame(mainw->multitrack, FALSE);
    mainw->multitrack->redraw_block = FALSE;
  }

  sep_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_SEPWIN,
                                       lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  sep_img2 = lives_image_new_from_stock(LIVES_LIVES_STOCK_SEPWIN,
                                        lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  if (mainw->sep_win) {
    lives_widget_set_tooltip_text(mainw->m_sepwinbutton, _("Hide the play window (s)"));
    lives_widget_set_tooltip_text(mainw->t_sepwin, _("Hide the play window (s)"));
  } else {
    if (LIVES_IS_IMAGE(sep_img)) {
      LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(sep_img));
      if (pixbuf) lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
      pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(sep_img2));
      if (pixbuf) lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
    }
    lives_widget_set_tooltip_text(mainw->m_sepwinbutton, _("Show the play window (s)"));
    lives_widget_set_tooltip_text(mainw->t_sepwin, _("Play in separate window (s)"));
  }

  lives_widget_show(sep_img);
  lives_widget_show(sep_img2);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_sepwinbutton), sep_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_sepwin), sep_img2);

  if (prefs->sepwin_type == SEPWIN_TYPE_STICKY && !LIVES_IS_PLAYING) {
    if (mainw->sep_win) make_play_window();
    else kill_play_window();
    lives_widget_context_update();
    /* if (mainw->multitrack != NULL && !LIVES_IS_PLAYING) { */
    /*   activate_mt_preview(mainw->multitrack); // show frame preview */
    /* } */
  } else {
    if (LIVES_IS_PLAYING) {
      if (mainw->sep_win) {
        // switch to separate window during pb
        if (!mainw->multitrack) {
          lives_widget_hide(mainw->playframe);
          if (!prefs->hide_framebar && !mainw->faded && ((!mainw->preview && (CURRENT_CLIP_HAS_VIDEO || mainw->foreign)) ||
              (CURRENT_CLIP_IS_VALID &&
               cfile->opening))) {
            lives_widget_show(mainw->framebar);
          }
          if ((!mainw->faded && mainw->fs && ((prefs->play_monitor != widget_opts.monitor && prefs->play_monitor > 0 &&
                                               capable->nmonitors > 1))) ||
              (mainw->fs && mainw->vpp &&
               !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))) {
            unfade_background();
            showclipimgs();
          }
          if (mainw->fs && !mainw->faded) {
            lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
            resize(1.);
          } else {
            if (mainw->faded) {
              lives_widget_hide(mainw->playframe);
              lives_widget_hide(mainw->frame1);
              lives_widget_hide(mainw->frame2);
              lives_widget_show(mainw->eventbox3);
              lives_widget_show(mainw->eventbox4);
            } else {
              if (mainw->double_size) {
                // switch back to single size as we are scooping the player out
                lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
                resize(1.);
                if (!mainw->faded) {
                  if (palette->style & STYLE_1) {
                    lives_widget_show(mainw->sep_image);
                  }
                  lives_widget_show_all(mainw->message_box);
		// *INDENT-OFF*
                }}}}}
	// *INDENT-ON*

        make_play_window();

        mainw->pw_scroll_func = lives_signal_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_SCROLL_EVENT,
                                LIVES_GUI_CALLBACK(on_mouse_scroll), NULL);

        if (mainw->ext_playback && mainw->vpp->fheight > -1 && mainw->vpp->fwidth > -1) {
          // fixed o/p size for stream
          if ((mainw->vpp->fwidth == 0 || mainw->vpp->fheight == 0) && CURRENT_CLIP_IS_VALID) {
            mainw->vpp->fwidth = cfile->hsize;
            mainw->vpp->fheight = cfile->vsize;
          }
          mainw->pwidth = mainw->vpp->fwidth;
          mainw->pheight = mainw->vpp->fheight;

          if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)) {
            unfade_background();
          }
          resize(1.);
          resize_play_window();
        }

        if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
          hide_cursor(lives_widget_get_xwindow(mainw->play_window));
          lives_widget_set_app_paintable(mainw->play_window, TRUE);
        }
      } else {
        // switch from separate window during playback
        if (mainw->ext_playback) {
#ifndef IS_MINGW
          vid_playback_plugin_exit();
          lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
#else
          lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
          vid_playback_plugin_exit();
#endif
        }
        if (mainw->fs) {
          if (prefs->show_desktop_panel) {
            show_desktop_panel();
          }
        }

        kill_play_window();

        if (!mainw->fs && !mainw->multitrack) {
          lives_widget_show_all(mainw->playframe);

          lives_widget_show(mainw->t_bckground);
          lives_widget_show(mainw->t_double);

          if (!mainw->double_size) {
            lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
            resize(1.);
          } else {
            if (palette->style & STYLE_1) {
              lives_widget_hide(mainw->sep_image);
            }
            lives_widget_hide(mainw->message_box);
            lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), FALSE);
            resize(2.);
          }
        } else {
          // fullscreen
          if (mainw->multitrack && CURRENT_CLIP_HAS_VIDEO) {
            fade_background();
            fullscreen_internal();
	    // *INDENT-OFF*
          }}

        hide_cursor(lives_widget_get_xwindow(mainw->playarea));
      }}}
  // *INDENT-ON*
  if (LIVES_IS_PLAYING) mainw->force_show = TRUE;
}


void on_showfct_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  prefs->hide_framebar = !prefs->hide_framebar;
  if (!mainw->fs || (prefs->play_monitor != widget_opts.monitor && mainw->play_window && capable->nmonitors > 1)) {
    if (!prefs->hide_framebar) {
      lives_widget_show(mainw->framebar);
    } else {
      if (prefs->hide_framebar) {
        lives_widget_hide(mainw->framebar);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void on_sticky_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // type is SEPWIN_TYPE_STICKY (shown even when not playing)
  // or SEPWIN_TYPE_NON_STICKY (shown only when playing)
  boolean make_perm = (prefs->sepwin_type == future_prefs->sepwin_type);
  if (prefs->sepwin_type == SEPWIN_TYPE_NON_STICKY) {
    pref_factory_int(PREF_SEPWIN_TYPE, SEPWIN_TYPE_STICKY, make_perm);
  } else {
    pref_factory_int(PREF_SEPWIN_TYPE, SEPWIN_TYPE_NON_STICKY, make_perm);
  }
}


void on_fade_pressed(LiVESButton * button, livespointer user_data) {
  // toolbar button (unblank background)
  if (mainw->fs && (!mainw->play_window || (prefs->play_monitor == widget_opts.monitor || capable->nmonitors == 1)))
    return;
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->fade), !mainw->faded);
}


void on_fade_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  mainw->faded = !mainw->faded;
  if (LIVES_IS_PLAYING && (!mainw->fs || (prefs->play_monitor != widget_opts.monitor && mainw->play_window &&
                                          capable->nmonitors > 1))) {
    if (mainw->faded) {
      lives_widget_hide(mainw->framebar);
      fade_background();
    } else {
      unfade_background();
      lives_widget_show(mainw->frame1);
      lives_widget_show(mainw->frame2);
      lives_widget_show(mainw->eventbox3);
      lives_widget_show(mainw->eventbox4);
      if (!prefs->hide_framebar && !(prefs->hfbwnp && !LIVES_IS_PLAYING)) {
        lives_widget_show(mainw->framebar);
      }
      if (!mainw->multitrack && CURRENT_CLIP_IS_VALID) {
        redraw_timeline(mainw->current_file);
        show_playbar_labels(mainw->current_file);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void on_showsubs_toggled(LiVESWidgetObject * obj, livespointer user_data) {
  prefs->show_subtitles = !prefs->show_subtitles;
  if (mainw->current_file > 0 && !mainw->multitrack) {
    if (mainw->play_window) {
      load_preview_image(FALSE);
    }
    showclipimgs();
  }
}


void on_boolean_toggled(LiVESWidgetObject * obj, livespointer user_data) {
  boolean *ppref = (boolean *)user_data;
  *ppref = !(*ppref);
}


void on_audio_toggled(LiVESWidget * tbutton, LiVESWidget * label) {
  boolean state;
  if (!LIVES_IS_INTERACTIVE) return;

  state = lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbutton));
  lives_widget_set_sensitive(tbutton, !state);

  if (tbutton == mainw->ext_audio_checkbutton) {
    pref_factory_bool(PREF_REC_EXT_AUDIO, state, TRUE);
  } else {
    pref_factory_bool(PREF_REC_EXT_AUDIO, !state, TRUE);
  }
}

void on_loop_video_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (mainw->current_file == 0) return;
  mainw->loop = !mainw->loop;
  lives_widget_set_sensitive(mainw->playclip, !LIVES_IS_PLAYING && clipboard);
  if (mainw->current_file > -1) find_when_to_stop();
}


void on_loop_button_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (mainw->multitrack) {
    lives_signal_handler_block(mainw->multitrack->loop_continue, mainw->multitrack->loop_cont_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->loop_continue), !mainw->loop_cont);
    lives_signal_handler_unblock(mainw->multitrack->loop_continue, mainw->multitrack->loop_cont_func);
  }
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_continue), !mainw->loop_cont);
}


void on_loop_cont_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *loop_img;

  mainw->loop_cont = !mainw->loop_cont;

  loop_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_LOOP, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  if (mainw->loop_cont) {
    lives_widget_set_tooltip_text(mainw->m_loopbutton, _("Switch continuous looping off (o)"));
  } else {
    LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(loop_img));
    if (pixbuf) lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
    lives_widget_set_tooltip_text(mainw->m_loopbutton, _("Switch continuous looping on (o)"));
  }

  lives_widget_show(loop_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_loopbutton), loop_img);

  lives_widget_set_sensitive(mainw->playclip, clipboard != NULL);
  if (mainw->current_file > -1) find_when_to_stop();
  else mainw->whentostop = NEVER_STOP;

  if (mainw->preview_box) {
    loop_img = lives_image_new_from_stock(LIVES_STOCK_LOOP, LIVES_ICON_SIZE_LARGE_TOOLBAR);
    if (!loop_img) loop_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_LOOP,
                                LIVES_ICON_SIZE_LARGE_TOOLBAR);
    if (LIVES_IS_IMAGE(loop_img)) {
      LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(loop_img));
      if (pixbuf) {
        LiVESPixbuf *pixbuf2 = lives_pixbuf_copy(pixbuf);
        if (!mainw->loop_cont) {
          lives_pixbuf_saturate_and_pixelate(pixbuf2, pixbuf2, 0.2, FALSE);
        }
        lives_image_set_from_pixbuf(LIVES_IMAGE(loop_img), pixbuf2);
        lives_standard_button_set_image(LIVES_BUTTON(mainw->p_loopbutton), loop_img);
        //lives_widget_object_unref(pixbuf);
      }
    }
  }

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    if (mainw->jackd && (mainw->loop_cont || mainw->whentostop == NEVER_STOP)) {
      if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
        mainw->jackd->loop = AUDIO_LOOP_PINGPONG;
      else mainw->jackd->loop = AUDIO_LOOP_FORWARD;
    } else if (mainw->jackd) mainw->jackd->loop = AUDIO_LOOP_NONE;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
  }
#endif
}


void on_ping_pong_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  mainw->ping_pong = !mainw->ping_pong;
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd && mainw->jackd->loop != AUDIO_LOOP_NONE) {
    if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) mainw->jackd->loop = AUDIO_LOOP_PINGPONG;
    else mainw->jackd->loop = AUDIO_LOOP_FORWARD;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed && mainw->pulsed->loop != AUDIO_LOOP_NONE) {
    if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) mainw->pulsed->loop = AUDIO_LOOP_PINGPONG;
    else mainw->pulsed->loop = AUDIO_LOOP_FORWARD;
  }
#endif
}


void on_volume_slider_value_changed(LiVESScaleButton * sbutton, livespointer user_data) {
  pref_factory_float(PREF_MASTER_VOLUME, lives_scale_button_get_value(sbutton), TRUE);
}


void on_mute_button_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (mainw->multitrack != NULL) {
    lives_signal_handler_block(mainw->multitrack->mute_audio, mainw->multitrack->mute_audio_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->mute_audio), !mainw->mute);
    lives_signal_handler_unblock(mainw->multitrack->mute_audio, mainw->multitrack->mute_audio_func);
  }
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);
}


boolean mute_audio_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                            livespointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);
  return TRUE;
}


void on_mute_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *mute_img;
  LiVESWidget *mute_img2 = NULL;

  mainw->mute = !mainw->mute;

  // change the mute icon
  mute_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_VOLUME_MUTE,
                                        lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  if (mainw->preview_box) mute_img2 = lives_image_new_from_stock_at_size(LIVES_LIVES_STOCK_VOLUME_MUTE,
                                        LIVES_ICON_SIZE_CUSTOM, 24, 24);

  if (mainw->mute) {
#ifdef ENABLE_JACK
    if (mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
      if (LIVES_IS_PLAYING) {
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)) {
          weed_plant_t *event = get_last_frame_event(mainw->event_list);
          insert_audio_event_at(event, -1, mainw->jackd->playing_file, 0., 0.); // audio switch off
        }
        mainw->jackd->mute = TRUE;
        mainw->jackd->in_use = TRUE;
      }
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE) {
      if (LIVES_IS_PLAYING) {
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)) {
          weed_plant_t *event = get_last_frame_event(mainw->event_list);
          insert_audio_event_at(event, -1, mainw->pulsed->playing_file, 0., 0.); // audio switch off
        }
        mainw->pulsed->mute = TRUE;
        mainw->pulsed->in_use = TRUE;
      }
    }
#endif
    lives_widget_set_tooltip_text(mainw->m_mutebutton, _("Unmute the audio (z)"));
    if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_mutebutton, _("Unmute the audio (z)"));
  } else {
#ifdef ENABLE_JACK
    if (mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
      if (LIVES_IS_PLAYING) {
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)) {
          jack_get_rec_avals(mainw->jackd);
        }
        mainw->jackd->mute = FALSE;
        mainw->jackd->in_use = TRUE;
      }
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE) {
      if (LIVES_IS_PLAYING) {
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)) {
          pulse_get_rec_avals(mainw->pulsed);
        }
        mainw->pulsed->mute = FALSE;
        mainw->pulsed->in_use = TRUE;
      }
    }
#endif
    if (LIVES_IS_IMAGE(mute_img)) {
      LiVESPixbuf *pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(mute_img));
      if (pixbuf) lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
      if (mainw->preview_box) {
        pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(mute_img2));
        if (pixbuf) lives_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.2, FALSE);
      }
    }
    lives_widget_set_tooltip_text(mainw->m_mutebutton, _("Mute the audio (z)"));
    if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_mutebutton, _("Mute the audio (z)"));
  }

  lives_widget_show(mute_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_mutebutton), mute_img);

  if (mainw->preview_box) {
    lives_widget_show(mute_img2);
    lives_button_set_image(LIVES_BUTTON(mainw->p_mutebutton), mute_img2); // doesn't work (gtk+ bug ?)
    lives_widget_queue_draw(mainw->p_mutebutton);
    lives_widget_queue_draw(mute_img2);
  }

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && LIVES_IS_PLAYING && mainw->jackd) {
    mainw->jackd->mute = mainw->mute;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && LIVES_IS_PLAYING && mainw->pulsed) {
    mainw->pulsed->mute = mainw->mute;
  }
#endif
}


#define GEN_SPB_LINK(n, bit) case n: mainw->fx##n##_##bit = \
    lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton)); break
#define GEN_SPB_LINK_I(n, bit) case n: mainw->fx##n##_##bit = \
    lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton)); break

void on_spin_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
    GEN_SPB_LINK(1, val); GEN_SPB_LINK(2, val);
    GEN_SPB_LINK(3, val); GEN_SPB_LINK(4, val);
  default: break;
  }
}


void on_spin_start_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  // generic
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
    GEN_SPB_LINK_I(1, start); GEN_SPB_LINK_I(2, start);
    GEN_SPB_LINK_I(3, start); GEN_SPB_LINK_I(4, start);
  default: break;
  }
}


void on_spin_step_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  // generic
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
    GEN_SPB_LINK_I(1, step); GEN_SPB_LINK_I(2, step);
    GEN_SPB_LINK_I(3, step); GEN_SPB_LINK_I(4, step);
  default: break;
  }
}


void on_spin_end_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  // generic
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
    GEN_SPB_LINK_I(1, end); GEN_SPB_LINK_I(2, end);
    GEN_SPB_LINK_I(3, end); GEN_SPB_LINK_I(4, end);
  default: break;
  }
}


void on_rev_clipboard_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // reverse the clipboard
  char *com;
  int current_file = mainw->current_file;
  mainw->current_file = 0;

  if (!check_if_non_virtual(0, 1, cfile->frames)) {
    lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
    char *msg = (_("Pulling frames from clipboard..."));
    if (!(cdata->seek_flag & LIVES_SEEK_FAST)) {
      if (realize_all_frames(0, msg, FALSE) <= 0) {
        mainw->current_file = current_file;
        lives_free(msg);
        sensitize();
        return;
      }
      lives_free(msg);
    }
  }

  d_print(_("Reversing clipboard..."));
  com = lives_strdup_printf("%s reverse \"%s\" %d %d \"%s\"", prefs->backend, clipboard->handle, 1, clipboard->frames,
                            get_image_ext_for_type(cfile->img_type));

  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (!THREADVAR(com_failed)) {
    cfile->progress_start = 1;
    cfile->progress_end = cfile->frames;

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE, FALSE, _("Reversing clipboard"));
  }

  if (THREADVAR(com_failed) || mainw->error) d_print_failed();
  else {
    if (clipboard->frame_index) reverse_frame_index(0);
    d_print_done();
  }
  clipboard->arate = -clipboard->arate;
  clipboard->aseek_pos = clipboard->afilesize;
  mainw->current_file = current_file;
  sensitize();
}


void on_load_subs_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *subfile;
  char *filt[] = LIVES_SUBS_FILTER;
  char filename[512];
  char *subfname, *isubfname;
  lives_subtitle_type_t subtype = SUBTITLE_TYPE_NONE;
  char *lfile_name;
  char *ttl;

  if (!CURRENT_CLIP_IS_VALID) return;

  if (cfile->subt != NULL) if (!do_existing_subs_warning()) return;

  // try to repaint the screen, as it may take a few seconds to get a directory listing
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  ttl = (_("Load Subtitles"));

  if (*mainw->vid_load_dir) {
    subfile = choose_file(mainw->vid_load_dir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, ttl, NULL);
  } else subfile = choose_file(NULL, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, ttl, NULL);
  lives_free(ttl);

  if (subfile == NULL) return; // cancelled

  lives_snprintf(filename, 512, "%s", subfile);
  lives_free(subfile);

  get_filename(filename, FALSE); // strip extension
  isubfname = lives_strdup_printf("%s.%s", filename, LIVES_FILE_EXT_SRT);
  lfile_name = lives_filename_from_utf8(isubfname, -1, NULL, NULL, NULL);

  if (lives_file_test(lfile_name, LIVES_FILE_TEST_EXISTS)) {
    subfname = lives_build_filename(prefs->workdir, cfile->handle, SUBS_FILENAME "." LIVES_FILE_EXT_SRT, NULL);
    subtype = SUBTITLE_TYPE_SRT;
  } else {
    lives_free(isubfname);
    lives_free(lfile_name);
    isubfname = lives_strdup_printf("%s.%s", filename, LIVES_FILE_EXT_SUB);
    lfile_name = lives_filename_from_utf8(isubfname, -1, NULL, NULL, NULL);

    if (lives_file_test(isubfname, LIVES_FILE_TEST_EXISTS)) {
      subfname = lives_build_filename(prefs->workdir, cfile->handle, SUBS_FILENAME "." LIVES_FILE_EXT_SUB, NULL);
      subtype = SUBTITLE_TYPE_SUB;
    } else {
      lives_free(isubfname);
      do_invalid_subs_error();
      lives_free(lfile_name);
      return;
    }
  }

  if (cfile->subt != NULL) {
    // erase any existing subs
    on_erase_subs_activate(NULL, NULL);
    subtitles_free(cfile);
  }

  lives_cp(lfile_name, subfname);

  if (THREADVAR(com_failed)) {
    lives_free(subfname);
    lives_free(isubfname);
    lives_free(lfile_name);
    return;
  }

  subtitles_init(cfile, subfname, subtype);
  lives_free(subfname);

  if (mainw->multitrack == NULL) {
    // force update
    switch_to_file(0, mainw->current_file);
  }

  d_print(_("Loaded subtitle file: %s\n"), isubfname);

  lives_free(isubfname);
  lives_free(lfile_name);
}


void on_save_subs_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *subfile;
  char xfname[512];
  char xfname2[512];

  // try to repaint the screen, as it may take a few seconds to get a directory listing
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  lives_snprintf(xfname, 512, "%s", mainw->subt_save_file);
  get_dirname(xfname);

  lives_snprintf(xfname2, 512, "%s", mainw->subt_save_file);
  get_basename(xfname2);

  subfile = choose_file(xfname, xfname2, NULL, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);

  if (subfile == NULL) return; // cancelled

  lives_free(subfile);
}


void on_erase_subs_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *sfname;

  if (!CURRENT_CLIP_IS_VALID || cfile->subt == NULL) return;

  if (menuitem != NULL)
    if (!do_erase_subs_warning()) return;

  switch (cfile->subt->type) {
  case SUBTITLE_TYPE_SRT:
    sfname = lives_build_filename(prefs->workdir, cfile->handle, SUBS_FILENAME "." LIVES_FILE_EXT_SRT, NULL);
    break;

  case SUBTITLE_TYPE_SUB:
    sfname = lives_build_filename(prefs->workdir, cfile->handle, SUBS_FILENAME "." LIVES_FILE_EXT_SUB, NULL);
    break;

  default:
    return;
  }

  subtitles_free(cfile);

  lives_rm(sfname);
  lives_free(sfname);

  if (menuitem != NULL) {
    // force update
    if (mainw->multitrack == NULL) {
      switch_to_file(0, mainw->current_file);
    }
    d_print(_("Subtitles were erased.\n"));
  }
}


void on_load_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *chooser;
  char *filt[] = LIVES_AUDIO_LOAD_FILTER;
  LiVESResponseType resp;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  chooser = choose_file_with_preview((*mainw->audio_dir) ? mainw->audio_dir : NULL, _("Select Audio File"), filt,
                                     LIVES_FILE_SELECTION_AUDIO_ONLY);

  resp = lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();

  if (resp != LIVES_RESPONSE_ACCEPT) on_filechooser_cancel_clicked(chooser);
  else on_open_new_audio_clicked(LIVES_FILE_CHOOSER(chooser), NULL);
}


void on_open_new_audio_clicked(LiVESFileChooser * chooser, livespointer user_data) {
  // open audio file
  // also called from osc.c

  char *a_type;
  char *com, *tmp;
  char **array;

  uint32_t chk_mask = 0;

  int oundo_start;
  int oundo_end;
  int israw = 1;
  int asigned, aendian;

  boolean bad_header = FALSE;
  boolean preparse = FALSE;
  boolean gotit = FALSE;

  register int i;

  if (!CURRENT_CLIP_IS_VALID) return;

  tmp = (_("Loading new audio"));
  chk_mask = WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_ALTER_AUDIO;
  if (!check_for_layout_errors(tmp, mainw->current_file, 1, 0, &chk_mask)) {
    lives_free(tmp);
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  cfile->undo_arate = cfile->arate;
  cfile->undo_achans = cfile->achans;
  cfile->undo_asampsize = cfile->asampsize;
  cfile->undo_signed_endian = cfile->signed_endian;
  cfile->undo_arps = cfile->arps;

  oundo_start = cfile->undo_start;
  oundo_end = cfile->undo_end;

  if (user_data == NULL) {
    char *filename = lives_file_chooser_get_filename(chooser);
    lives_snprintf(file_name, PATH_MAX, "%s", (tmp = lives_filename_to_utf8(filename, -1, NULL, NULL, NULL)));
    lives_free(filename);
    lives_free(tmp);
  } else lives_snprintf(file_name, PATH_MAX, "%s", (char *)user_data);

  lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", file_name);
  get_dirname(mainw->audio_dir);
  end_fs_preview();
  lives_widget_destroy(LIVES_WIDGET(chooser));
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  a_type = get_extension(file_name);

  if (strlen(a_type)) {
    char *filt[] = LIVES_AUDIO_LOAD_FILTER;
    for (i = 0; filt[i] != NULL; i++) {
      if (!lives_ascii_strcasecmp(a_type, filt[i] + 2)) gotit = TRUE; // skip past "*." in filt
    }
  }

  if (gotit) {
    com = lives_strdup_printf("%s audioopen \"%s\" \"%s\"", prefs->backend, cfile->handle,
                              (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));
    lives_free(tmp);
  } else {
    lives_free(a_type);
    do_audio_import_error();

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (!lives_ascii_strncasecmp(a_type, LIVES_FILE_EXT_WAV, 3)) israw = 0;

  if (HAS_EXTERNAL_PLAYER) {
    if (read_file_details(file_name, TRUE, FALSE)) {
      if (get_token_count(mainw->msg, '|') >= 14) {
        array = lives_strsplit(mainw->msg, "|", -1);
        cfile->arate = atoi(array[9]);
        cfile->achans = atoi(array[10]);
        cfile->asampsize = atoi(array[11]);
        cfile->signed_endian = get_signed_endian(atoi(array[12]), atoi(array[13]));
        lives_strfreev(array);
        preparse = TRUE;
      }
    }
  }

  if (!preparse) {
    // TODO !!! - need some way to identify audio without invoking mplayer
    cfile->arate = cfile->arps = DEFAULT_AUDIO_RATE;
    cfile->achans = DEFAULT_AUDIO_CHANS;
    cfile->asampsize = DEFAULT_AUDIO_SAMPS;
    cfile->signed_endian = mainw->endian;
  }

  if (cfile->undo_arate > 0) cfile->arps = cfile->undo_arps / cfile->undo_arate * cfile->arate;
  else cfile->arps = cfile->arate;

  d_print(""); // force switchtext
  d_print(_("Opening audio %s, type %s..."), file_name, a_type);
  lives_free(a_type);

  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    cfile->arate = cfile->undo_arate;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->arps = cfile->undo_arps;
    cfile->undo_start = oundo_start;
    cfile->undo_end = oundo_end;
    sensitize();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    reget_afilesize(mainw->current_file);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  cfile->opening = cfile->opening_audio = cfile->opening_only_audio = TRUE;

  cfile->undo_start = 1;
  cfile->undo_end = cfile->frames;

  // show audio [opening...] in main window
  get_play_times();

  if (!(do_progress_dialog(TRUE, TRUE, _("Opening audio")))) {
    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;
    lives_rm(cfile->info_file);
    com = lives_strdup_printf("%s cancel_audio \"%s\"", prefs->backend, cfile->handle);
    lives_system(com, FALSE);
    do_auto_dialog(_("Cancelling"), 0);
    lives_free(com);
    cfile->opening_audio = cfile->opening = cfile->opening_only_audio = FALSE;
    cfile->arate = cfile->undo_arate;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->arps = cfile->undo_arps;
    cfile->undo_start = oundo_start;
    cfile->undo_end = oundo_end;
    sensitize();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    reget_afilesize(mainw->current_file);
    if (mainw->error) d_print_failed();
    unbuffer_lmap_errors(FALSE);
    return;
  }

  cfile->opening_audio = cfile->opening = cfile->opening_only_audio = FALSE;

  cfile->afilesize = 0;

  if (get_token_count(mainw->msg, '|') > 6) {
    array = lives_strsplit(mainw->msg, "|", 7);
    cfile->arate = atoi(array[1]);
    cfile->achans = atoi(array[2]);
    cfile->asampsize = atoi(array[3]);
    cfile->signed_endian = get_signed_endian(atoi(array[4]), atoi(array[5]));
    cfile->afilesize = atol(array[6]);
    lives_strfreev(array);

    if (cfile->undo_arate > 0) cfile->arps = cfile->undo_arps / cfile->undo_arate * cfile->arate;
    else cfile->arps = cfile->arate;
  }

  /// not sure why, but this messes up mainw->msg...
  lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

  if (cfile->afilesize == 0) {
    d_print_failed();

    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;
    lives_rm(cfile->info_file);

    com = lives_strdup_printf("%s cancel_audio \"%s\"", prefs->backend, cfile->handle);

    lives_system(com, FALSE);
    lives_free(com);

    if (!THREADVAR(com_failed)) do_auto_dialog(_("Cancelling"), 0);

    cfile->arate = cfile->undo_arate;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->arps = cfile->undo_arps;
    cfile->undo_start = oundo_start;
    cfile->undo_end = oundo_end;
    sensitize();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    reget_afilesize(mainw->current_file);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  cfile->changed = TRUE;
  d_print_done();

  d_print(P_("New audio: %d Hz %d channel %d bps\n", "New audio: %d Hz %d channels %d bps\n",
             cfile->achans),
          cfile->arate, cfile->achans, cfile->asampsize);

  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;
  lives_rm(cfile->info_file);

  com = lives_strdup_printf("%s commit_audio \"%s\" %d", prefs->backend, cfile->handle, israw);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    cfile->arate = cfile->undo_arate;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->arps = cfile->undo_arps;
    cfile->undo_start = oundo_start;
    cfile->undo_end = oundo_end;
    sensitize();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    reget_afilesize(mainw->current_file);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (!do_auto_dialog(_("Committing audio"), 0)) {
    //cfile->may_be_damaged=TRUE;
    d_print_failed();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (prefs->save_directories) {
    set_utf8_pref(PREF_AUDIO_DIR, mainw->audio_dir);
  }
  if (!prefs->conserve_space) {
    cfile->undo_action = UNDO_NEW_AUDIO;
    set_undoable(_("New Audio"), TRUE);
  }

  asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
  aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;

  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_AENDIAN, &aendian)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASIGNED, &asigned)) bad_header = TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);

  if (mainw->multitrack == NULL) {
    switch_to_file(mainw->current_file, mainw->current_file);
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

  if (mainw->multitrack != NULL) {
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
}


void on_load_cdtrack_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *cdtrack_dialog;

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  if (!strlen(prefs->cdplay_device)) {
    do_cd_error_dialog();
    return;
  }

  mainw->fx1_val = 1;
  cdtrack_dialog = create_cdtrack_dialog(LIVES_DEVICE_CD, NULL);
  lives_widget_show_all(cdtrack_dialog);
}


void on_eject_cd_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *com;

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  if (!strlen(prefs->cdplay_device)) {
    do_cd_error_dialog();
    return;
  }

  if (strlen(capable->eject_cmd)) {
    com = lives_strdup_printf("%s \"%s\"", capable->eject_cmd, prefs->cdplay_device);

    lives_system(com, TRUE);
    lives_free(com);
  }
}


void on_load_cdtrack_ok_clicked(LiVESButton * button, livespointer user_data) {
  char *com;
  char **array;

  boolean was_new = FALSE;

  uint32_t chk_mask = 0;

  int new_file = mainw->first_free_file;
  int asigned, endian;

  boolean bad_header = FALSE;
  boolean needs_idlefunc = mainw->mt_needs_idlefunc;
  mainw->mt_needs_idlefunc = FALSE;
  lives_general_button_clicked(button, NULL);
  mainw->mt_needs_idlefunc = needs_idlefunc;

  if (CURRENT_CLIP_IS_VALID) {
    char *tmp = (_("Loading new audio"));
    chk_mask = WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_ALTER_AUDIO;
    if (!check_for_layout_errors(tmp, mainw->current_file, 1, 0, &chk_mask)) {
      lives_free(tmp);
      return;
    }
    lives_free(tmp);
  }

  d_print(_("Opening CD track %d from %s..."), (int)mainw->fx1_val, prefs->cdplay_device);

  if (!CURRENT_CLIP_IS_VALID) {
    if (!get_new_handle(new_file, lives_strdup_printf(_("CD track %d"), (int)mainw->fx1_val))) {
      unbuffer_lmap_errors(FALSE);
      return;
    }

    mainw->current_file = new_file;
    lives_snprintf(cfile->type, 40, "CD track %d on %s", (int)mainw->fx1_val, prefs->cdplay_device);
    update_play_times();
    add_to_clipmenu();
    was_new = TRUE;
    cfile->opening = cfile->opening_audio = cfile->opening_only_audio = TRUE;
    cfile->hsize = DEF_FRAME_HSIZE;
    cfile->vsize = DEF_FRAME_VSIZE;
  } else {
    cfile->undo_arate = cfile->arate;
    cfile->undo_achans = cfile->achans;
    cfile->undo_asampsize = cfile->asampsize;
    cfile->undo_signed_endian = cfile->signed_endian;
    cfile->undo_arps = cfile->arps;
  }

  com = lives_strdup_printf("%s cdopen \"%s\" %d", prefs->backend, cfile->handle, (int)mainw->fx1_val);

  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    cfile->arate = cfile->undo_arate;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->arps = cfile->undo_arps;

    sensitize();
    reget_afilesize(mainw->current_file);
    if (was_new) close_current_file(0);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (!(do_progress_dialog(TRUE, TRUE, _("Opening CD track...")))) {
    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    if (!was_new) {
      mainw->cancelled = CANCEL_NONE;
      mainw->error = FALSE;
      lives_rm(cfile->info_file);

      com = lives_strdup_printf("%s cancel_audio \"%s\"", prefs->backend, cfile->handle);
      lives_system(com, FALSE);
      lives_free(com);

      if (!THREADVAR(com_failed)) do_auto_dialog(_("Cancelling"), 0);

      cfile->arate = cfile->undo_arate;
      cfile->achans = cfile->undo_achans;
      cfile->asampsize = cfile->undo_asampsize;
      cfile->signed_endian = cfile->undo_signed_endian;
      cfile->arps = cfile->undo_arps;

      sensitize();
      reget_afilesize(mainw->current_file);
    }

    if (was_new) close_current_file(0);

    if (mainw->error) {
      d_print_failed();
    }

    unbuffer_lmap_errors(FALSE);
    return;
  }

  lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

  if (mainw->error) {
    d_print(_("Error loading CD track\n"));

    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    if (!was_new) {
      com = lives_strdup_printf("%s cancel_audio \"%s\"", prefs->backend, cfile->handle);
      mainw->cancelled = CANCEL_NONE;
      mainw->error = FALSE;
      lives_rm(cfile->info_file);
      lives_system(com, FALSE);
      lives_free(com);

      if (!THREADVAR(com_failed)) do_auto_dialog(_("Cancelling"), 0);

      cfile->arate = cfile->undo_arate;
      cfile->achans = cfile->undo_achans;
      cfile->asampsize = cfile->undo_asampsize;
      cfile->signed_endian = cfile->undo_signed_endian;
      cfile->arps = cfile->undo_arps;

      sensitize();
      reget_afilesize(mainw->current_file);
    }

    if (was_new) close_current_file(0);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  array = lives_strsplit(mainw->msg, "|", 5);
  cfile->arate = atoi(array[1]);
  cfile->achans = atoi(array[2]);
  cfile->asampsize = atoi(array[3]);
  cfile->afilesize = strtol(array[4], NULL, 10);
  lives_strfreev(array);

  if (!was_new && cfile->undo_arate > 0) cfile->arps = cfile->undo_arps / cfile->undo_arate * cfile->arate;
  else cfile->arps = cfile->arate;

  asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
  endian = cfile->signed_endian & AFORM_BIG_ENDIAN;

  if (cfile->afilesize == 0l) {
    d_print(_("Error loading CD track\n"));

    if (!was_new) {
      com = lives_strdup_printf("%s cancel_audio \"%s\"", prefs->backend, cfile->handle);
      mainw->cancelled = CANCEL_NONE;
      mainw->error = FALSE;
      lives_rm(cfile->info_file);
      lives_system(com, FALSE);
      lives_free(com);

      if (!THREADVAR(com_failed)) do_auto_dialog(_("Cancelling"), 0);

      cfile->achans = cfile->undo_achans;
      cfile->arate = cfile->undo_arate;
      cfile->arps = cfile->undo_arps;
      cfile->asampsize = cfile->undo_asampsize;
      cfile->signed_endian = cfile->undo_signed_endian;

      reget_afilesize(mainw->current_file);
    }

    if (was_new) close_current_file(0);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  cfile->opening = cfile->opening_audio = cfile->opening_only_audio = FALSE;

  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;
  lives_rm(cfile->info_file);

  com = lives_strdup_printf("%s commit_audio \"%s\"", prefs->backend, cfile->handle);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    cfile->achans = cfile->undo_achans;
    cfile->arate = cfile->undo_arate;
    cfile->arps = cfile->undo_arps;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->signed_endian = cfile->undo_signed_endian;

    reget_afilesize(mainw->current_file);

    if (was_new) close_current_file(0);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (!do_auto_dialog(_("Committing audio"), 0)) {
    d_print_failed();
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASIGNED, &asigned)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_AENDIAN, &endian)) bad_header = TRUE;
  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);

  reget_afilesize(mainw->current_file);
  cfile->changed = TRUE;
  d_print_done();
  d_print(P_("New audio: %d Hz %d channel %d bps\n", "New audio: %d Hz %d channels %d bps\n", cfile->achans),
          cfile->arate, cfile->achans, cfile->asampsize);

  if (!was_new) {
    if (!prefs->conserve_space) {
      cfile->undo_action = UNDO_NEW_AUDIO;
      set_undoable(_("New Audio"), TRUE);
    }
  }

  lives_widget_set_sensitive(mainw->loop_video, TRUE);

  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}


void on_load_vcd_ok_clicked(LiVESButton * button, livespointer user_data) {
  boolean needs_idlefunc = mainw->mt_needs_idlefunc;
  mainw->mt_needs_idlefunc = FALSE;
  lives_general_button_clicked(button, NULL);
  mainw->mt_needs_idlefunc = needs_idlefunc;

  if (LIVES_POINTER_TO_INT(user_data) == LIVES_DEVICE_DVD) {
    lives_snprintf(file_name, PATH_MAX, "dvd://%d", (int)mainw->fx1_val);
    lives_freep((void **)&mainw->file_open_params);
    if (USE_MPV) mainw->file_open_params = lives_strdup_printf("--chapter=%d --aid=%d", (int)mainw->fx2_val, (int)mainw->fx3_val);
    else mainw->file_open_params = lives_strdup_printf("-chapter %d -aid %d", (int)mainw->fx2_val, (int)mainw->fx3_val);
  } else {
    lives_snprintf(file_name, PATH_MAX, "vcd://%d", (int)mainw->fx1_val);
  }
  open_sel_range_activate(0, 0.);
}


void popup_lmap_errors(LiVESMenuItem * menuitem, livespointer user_data) {
  // popup layout map errors dialog
  LiVESWidget *vbox;
  LiVESWidget *button;
  text_window *textwindow;

  uint32_t chk_mask = 0;

  unbuffer_lmap_errors(TRUE);

  if (menuitem == NULL && user_data != NULL) {
    if (prefs->warning_mask & WARN_MASK_LAYOUT_POPUP) return;
    chk_mask = (uint32_t)LIVES_POINTER_TO_INT(user_data);
    if (((chk_mask ^ prefs->warning_mask) & chk_mask) == 0) return;
  }

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  textwindow = create_text_window(_("Layout Errors"), NULL, mainw->layout_textbuffer, FALSE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

  add_warn_check(LIVES_BOX(vbox), WARN_MASK_LAYOUT_POPUP);

  button = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog), LIVES_STOCK_CLOSE, _("_Close Window"),
           LIVES_RESPONSE_OK);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked),
                            textwindow);

  lives_container_set_border_width(LIVES_CONTAINER(button), widget_opts.border_width);

  textwindow->clear_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog), LIVES_STOCK_CLEAR,
                             _("Clear _Errors"),
                             LIVES_RESPONSE_CANCEL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(textwindow->clear_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_lerrors_clear_clicked),
                            LIVES_INT_TO_POINTER(FALSE));

  lives_container_set_border_width(LIVES_CONTAINER(textwindow->clear_button), widget_opts.border_width);

  textwindow->delete_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog), LIVES_STOCK_DELETE,
                              _("_Delete affected layouts"), LIVES_RESPONSE_CANCEL);

#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_button_box_set_layout(LIVES_BUTTON_BOX(lives_widget_get_parent(textwindow->delete_button)), LIVES_BUTTONBOX_SPREAD);
#else
  lives_widget_set_halign(lives_widget_get_parent(textwindow->delete_button), LIVES_ALIGN_FILL);
#endif

  lives_container_set_border_width(LIVES_CONTAINER(textwindow->delete_button), widget_opts.border_width);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(textwindow->delete_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_lerrors_delete_clicked),
                            NULL);

  lives_widget_show_all(textwindow->dialog);
}


void on_rename_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  renamew = create_rename_dialog(1);
  lives_widget_show_all(renamew->dialog);
}


void autolives_toggle(LiVESMenuItem * menuitem, livespointer user_data) {
  // TODO: allow mapping of change types to random ranges in the backend
  // TODO: allow user selection of all ports
#ifdef ENABLE_OSC
  autolives_window *alwindow = NULL;
  int trigtime;
  char *apb;
  char *mute;
  char *trigopt;
  char *debug;
  char string[PATH_MAX];
  char *com = NULL;
  boolean cancelled = FALSE;

  if (mainw->alives_pgid > 0) {
    // already running, kill the old process
    lives_killpg(mainw->alives_pgid, LIVES_SIGHUP);
    mainw->alives_pgid = 0;

    // restore pre-playback rte state
    rte_keymodes_restore(prefs->rte_keys_virtual);
    goto autolives_fail;
  }

  if (!lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->autolives))) return;

  if (!CURRENT_CLIP_IS_VALID) {
    do_autolives_needs_clips_error();
    goto autolives_fail;
  }

  if (cfile->event_list != NULL || cfile->opening || mainw->multitrack != NULL || mainw->is_processing || mainw->preview) {
    // ignore if doing something more important
    goto autolives_fail;
  }

  // search for autolives.pl
  if (!capable->has_autolives) {
    get_location(EXEC_AUTOLIVES_PL, string, PATH_MAX);
    if (strlen(string)) capable->has_autolives = TRUE;
    else {
      do_no_autolives_error();
      goto autolives_fail;
    }
  }

  alwindow = autolives_pre_dialog();
  if (alwindow == NULL) {
    goto autolives_fail;
  }
  if (lives_dialog_run(LIVES_DIALOG(alwindow->dialog)) == LIVES_RESPONSE_CANCEL) {
    // user cancelled
    cancelled = TRUE;
    goto autolives_fail;
  }

  // check if osc is started; if not ask permission
  if (!prefs->osc_udp_started) {
    char typep[32];
    char *inst = (char *)typep;
    lives_snprintf(typep, 32, "%d", LIVES_PERM_OSC_PORTS);
    if (!lives_ask_permission((char **)&inst, 1, 0)) {
      // permission not given
      goto autolives_fail;
    }

    // try: start up osc
    prefs->osc_udp_started = lives_osc_init(prefs->osc_udp_port);
    if (!prefs->osc_udp_started) {
      goto autolives_fail;
    }
  }

  // build the command to run
  trigtime = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(alwindow->atrigger_spin));
  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(alwindow->apb_button))) apb = lives_strdup(" -waitforplay");
  else apb = lives_strdup("");
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(alwindow->mute_button))) mute = lives_strdup(" -mute");
  else mute = lives_strdup("");
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(alwindow->atrigger_button))) {
    // timed
    trigopt = lives_strdup_printf(" -time %d", trigtime);
  } else {
    // omc
    trigopt = lives_strdup_printf(" -omc %d", prefs->osc_udp_port - 2);
  }
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(alwindow->debug_button))) {
    debug = lives_strdup(" -debug");
  } else {
    debug = lives_strdup("");
  }
  lives_widget_destroy(alwindow->dialog);
  lives_free(alwindow);

  // store the current key/mode state
  rte_keymodes_backup(prefs->rte_keys_virtual);

  com = lives_strdup_printf("/usr/bin/%s localhost %d %d%s%s%s%s", EXEC_AUTOLIVES_PL, prefs->osc_udp_port,
                            prefs->osc_udp_port - 1, apb,
                            trigopt, mute,
                            debug);

  mainw->alives_pgid = lives_fork(com);

  lives_free(debug);
  lives_free(trigopt);
  lives_free(apb);
  lives_free(mute);
  if (com != NULL) lives_free(com);
  return;

autolives_fail:
  if (alwindow != NULL) {
    if (!cancelled) lives_widget_destroy(alwindow->dialog);
    lives_free(alwindow);
  }

  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->autolives), FALSE);

#endif
}


void on_rename_clip_name(LiVESButton * button, livespointer user_data) {
  char title[256];
  boolean bad_header = FALSE;

  if (user_data == NULL) {
    lives_snprintf(title, 256, "%s", lives_entry_get_text(LIVES_ENTRY(renamew->entry)));
    lives_widget_destroy(renamew->dialog);
    lives_free(renamew);
  } else lives_snprintf(title, 256, "%s", (char *)user_data);

  if (!(strlen(title))) return;

  if (user_data == NULL) {
    set_main_title(title, 0);
  }

  if (CURRENT_CLIP_IS_VALID) {
    lives_menu_item_set_text(cfile->menuentry, title, FALSE);

    lives_snprintf(cfile->name, 256, "%s", title);

    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_CLIPNAME, cfile->name)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
    cfile->was_renamed = TRUE;
  }
}


void on_toy_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (menuitem != NULL && mainw->toy_type == LIVES_POINTER_TO_INT(user_data)) {
    // switch is off
    user_data = LIVES_INT_TO_POINTER(LIVES_TOY_NONE);
  }

  switch (mainw->toy_type) {
  // old status
  case LIVES_TOY_MAD_FRAMES:
    lives_signal_handler_block(mainw->toy_random_frames, mainw->toy_func_random_frames);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_random_frames), FALSE);
    lives_signal_handler_unblock(mainw->toy_random_frames, mainw->toy_func_random_frames);
    if (LIVES_IS_PLAYING) {
      if (mainw->faded) {
        lives_widget_hide(mainw->start_image);
        lives_widget_hide(mainw->end_image);
      }
      if (CURRENT_CLIP_IS_VALID) {
        showclipimgs();
      }
    }
    break;
  case LIVES_TOY_TV:
    lives_signal_handler_block(mainw->toy_tv, mainw->toy_func_lives_tv);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_tv), FALSE);
    lives_signal_handler_unblock(mainw->toy_tv, mainw->toy_func_lives_tv);
    break;
  default:
    lives_signal_handler_block(mainw->toy_none, mainw->toy_func_none);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none), FALSE);
    lives_signal_handler_unblock(mainw->toy_none, mainw->toy_func_none);
    break;
  }

  mainw->toy_type = (lives_toy_t)LIVES_POINTER_TO_INT(user_data);

  switch (mainw->toy_type) {
  case LIVES_TOY_NONE:
    lives_signal_handler_block(mainw->toy_none, mainw->toy_func_none);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none), TRUE);
    lives_signal_handler_unblock(mainw->toy_none, mainw->toy_func_none);
    return;
  case LIVES_TOY_MAD_FRAMES:
    break;
  case LIVES_TOY_TV:
    // load in the lives TV clip
    deduce_file(LIVES_TV_CHANNEL1, 0., 0);

    // if we choose to discard it, discard it....otherwise keep it
    if (prefs->discard_tv) {
      close_current_file(0);
    } else {
      // keep it
      int current_file = mainw->current_file;
      char *com = lives_strdup_printf("%s commit_audio \"%s\"", prefs->backend, cfile->handle);
      cfile->start = 1;
      cfile->frames = get_frame_count(mainw->current_file, 1);
      cfile->end = cfile->frames;
      cfile->opening = cfile->opening_loc = cfile->opening_audio = cfile->opening_only_audio = FALSE;
      cfile->is_loaded = TRUE;
      lives_system(com, FALSE);
      save_clip_values(current_file);
      if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
      if (mainw->multitrack == NULL) {
        switch_to_file((mainw->current_file = 0), current_file);
        sensitize();
      }
    }
    break;
  default:
    if (mainw->faded && !mainw->foreign) {
      lives_widget_show(mainw->start_image);
      lives_widget_show(mainw->end_image);
    }
  }
}


void on_preview_spinbutton_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  // update the play window preview
  int preview_frame;
  static volatile boolean updated = FALSE;

  if (updated) return;

  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) {
    return;
  }
  if ((preview_frame = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton))) == mainw->preview_frame) {
    return;
  }
  // prevent multiple updates from interfering
  updated = TRUE;
  lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
  //lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  mainw->preview_frame = preview_frame;
  load_preview_image(TRUE);
  lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);
  updated = FALSE;
}


void on_prv_link_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(togglebutton)) return;
  mainw->prv_link = LIVES_POINTER_TO_INT(user_data);
  if (mainw->is_processing && (mainw->prv_link == PRV_START || mainw->prv_link == PRV_END)) {
    // block spinbutton in play window
    lives_widget_set_sensitive(mainw->preview_spinbutton, FALSE);
  } else {
    lives_widget_set_sensitive(mainw->preview_spinbutton, TRUE);
  }
  load_preview_image(FALSE);
  lives_widget_grab_focus(mainw->preview_spinbutton);
}


void update_sel_menu(void) {
  if (mainw->multitrack != NULL || mainw->selwidth_locked) return;

  if (!CURRENT_CLIP_HAS_VIDEO || LIVES_IS_PLAYING || CURRENT_CLIP_IS_CLIPBOARD || mainw->is_processing) {
    lives_widget_set_sensitive(mainw->select_invert, FALSE);
    lives_widget_set_sensitive(mainw->select_all, FALSE);
    lives_widget_set_sensitive(mainw->sa_button, FALSE);
    lives_widget_set_sensitive(mainw->select_start_only, FALSE);
    lives_widget_set_sensitive(mainw->select_end_only, FALSE);
    lives_widget_set_sensitive(mainw->select_from_start, FALSE);
    lives_widget_set_sensitive(mainw->select_to_end, FALSE);
    lives_widget_set_sensitive(mainw->select_to_aend, FALSE);
    return;
  }

  lives_widget_set_sensitive(mainw->select_new, cfile->insert_start > 0);
  lives_widget_set_sensitive(mainw->select_last, cfile->undo_start > 0);

  if (cfile->end > cfile->start) {
    lives_widget_set_sensitive(mainw->select_start_only, TRUE);
    lives_widget_set_sensitive(mainw->select_end_only, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->select_start_only, FALSE);
    lives_widget_set_sensitive(mainw->select_end_only, FALSE);
  }
  if (cfile->start == 1 && cfile->end == cfile->frames) {
    lives_widget_set_sensitive(mainw->select_invert, FALSE);
    lives_widget_set_sensitive(mainw->select_all, FALSE);
    lives_widget_set_sensitive(mainw->sa_button, FALSE);
  } else {
    if (cfile->start == 1 || cfile->end == cfile->frames)
      lives_widget_set_sensitive(mainw->select_invert, TRUE);
    else
      lives_widget_set_sensitive(mainw->select_invert, FALSE);

    lives_widget_set_sensitive(mainw->select_all, TRUE);
    lives_widget_set_sensitive(mainw->sa_button, TRUE);
  }

  if (cfile->start == 1) lives_widget_set_sensitive(mainw->select_from_start, FALSE);
  else lives_widget_set_sensitive(mainw->select_from_start, TRUE);

  if (cfile->end < cfile->frames) {
    lives_widget_set_sensitive(mainw->select_to_end, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->select_to_end, FALSE);
    lives_widget_set_sensitive(mainw->select_to_aend, FALSE);
  }
  if (cfile->achans > 0) {
    int audframe = calc_frame_from_time4(mainw->current_file, cfile->laudio_time);
    if (audframe <= cfile->frames && audframe >= cfile->start && audframe != cfile->end)
      lives_widget_set_sensitive(mainw->select_to_aend, TRUE);
    else lives_widget_set_sensitive(mainw->select_to_aend, FALSE);
  } else lives_widget_set_sensitive(mainw->select_to_aend, FALSE);
}


void on_spinbutton_start_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  int start, ostart;
  boolean rdrw_bars = TRUE;
  static volatile boolean updated = FALSE;

  if (updated) return;

  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) {
    return;
  }
  if ((start = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton))) == cfile->start) {
    return;
  }

  // prevent multiple updates from interfering
  updated = TRUE;
  lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  ostart = cfile->start;
  cfile->start = start;

  if (mainw->selwidth_locked) {
    /// must not update cfile->end directly, otherwise the selection colour won'r be updates
    int new_end = cfile->end + cfile->start - ostart;
    mainw->selwidth_locked = FALSE;
    if (new_end > cfile->frames) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start - cfile->end + cfile->frames);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->frames);
      lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_end));
    } else {
      if (cfile->start < ostart && cfile->fps > 0.) {
        redraw_timer_bars((double)(cfile->start - 1.) / cfile->fps, (double)ostart / cfile->fps, 0);
        rdrw_bars = FALSE;
      }
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), new_end);
      lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_end));
    }
    mainw->selwidth_locked = TRUE;
  }
  update_sel_menu();

  if (!LIVES_IS_PLAYING && mainw->play_window != NULL && cfile->is_loaded) {
    /// load this first in case of caching - it is likely to be larger and higher quality
    if (mainw->prv_link == PRV_START && mainw->preview_frame != cfile->start)
      load_preview_image(FALSE);
  }

  load_start_image(cfile->start);

  if (cfile->start > cfile->end) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->start);
  }
  set_sel_label(mainw->sel_label);
  if (rdrw_bars && cfile->fps > 0.) {
    if (cfile->start < ostart)
      redraw_timer_bars((double)(cfile->start - 1.) / cfile->fps, (double)ostart / cfile->fps, 0);
    else
      redraw_timer_bars((double)(ostart - 1.) / cfile->fps, (double)cfile->start / cfile->fps, 0);
  }

  lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
  updated = FALSE;
}


void on_spinbutton_end_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  int end, oend;
  boolean rdrw_bars = TRUE;
  static volatile boolean updated = FALSE;

  if (updated) return;

  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) {
    return;
  }
  if ((end = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton))) == cfile->end) {
    return;
  }

  // prevent multiple updates from interfering
  updated = TRUE;

  lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  oend = cfile->end;
  cfile->end = end;

  if (mainw->selwidth_locked) {
    int new_start = cfile->start + cfile->end - oend;
    mainw->selwidth_locked = FALSE;
    if (new_start < 1) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end - cfile->start + 1);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 1);
      lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_start));
    } else {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), new_start);
      if (cfile->end > oend && cfile->fps > 0.) {
        redraw_timer_bars((double)oend / cfile->fps, (double)(cfile->end + 1) / cfile->fps, 0);
        rdrw_bars = FALSE;
      }
      lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_start));
    }
    mainw->selwidth_locked = TRUE;
  }
  update_sel_menu();

  if (!LIVES_IS_PLAYING && mainw->play_window != NULL && cfile->is_loaded) {
    /// load this first in case of caching - it is likely to be larger and higher quality
    if (mainw->prv_link == PRV_END && mainw->preview_frame != cfile->end)
      load_preview_image(FALSE);
  }

  load_end_image(cfile->end);

  if (cfile->end < cfile->start) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->end);
  }

  set_sel_label(mainw->sel_label);
  if (rdrw_bars && cfile->fps > 0.) {
    if (cfile->end > oend)
      redraw_timer_bars((double)oend / cfile->fps, (double)(cfile->end + 1) / cfile->fps, 0);
    else
      redraw_timer_bars((double)cfile->end / cfile->fps, (double)(oend + 1) / cfile->fps, 0);
  }

  lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);
  updated = FALSE;
}


boolean all_expose(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  lives_painter_surface_t **surf = (lives_painter_surface_t **)psurf;
  if (surf) {
    if (*surf) {
      lives_painter_set_source_surface(cr, *surf, 0., 0.);
      lives_painter_paint(cr);
    } else {
      return TRUE;
    }
  }
  return TRUE;
}


boolean all_expose_overlay(LiVESWidget * widget, lives_painter_t *creb, livespointer psurf) {
  /// quick and dirty copy / paste
  if (mainw->go_away) return FALSE;
  if (LIVES_IS_PLAYING && mainw->faded) return FALSE;
  if (!CURRENT_CLIP_IS_VALID) return FALSE;
  else {
    int bar_height;
    int allocy;
    double allocwidth = (double)lives_widget_get_allocation_width(mainw->video_draw), allocheight;
    double offset;
    double ptrtime = mainw->ptrtime;
    frames_t frame;
    int which = 0;

    offset = ptrtime / CURRENT_CLIP_TOTAL_TIME * allocwidth;

    lives_painter_set_line_width(creb, 1.);

    if (palette->style & STYLE_LIGHT) {
      lives_painter_set_source_rgb_from_lives_widget_color(creb, &palette->black);
    } else {
      lives_painter_set_source_rgb_from_lives_widget_color(creb, &palette->white);
    }

    if (!(frame = calc_frame_from_time(mainw->current_file, ptrtime)))
      frame = cfile->frames;

    if (cfile->frames > 0 && (which == 0 || which == 1)) {
      if (mainw->video_drawable) {
        bar_height = CE_VIDBAR_HEIGHT;

        allocheight = (double)lives_widget_get_allocation_height(mainw->vidbar) + bar_height + widget_opts.packing_height * 2.5;
        allocy = lives_widget_get_allocation_y(mainw->vidbar) - widget_opts.packing_height;
        lives_painter_move_to(creb, offset, allocy);
        lives_painter_line_to(creb, offset, allocy + allocheight);
        lives_painter_stroke(creb);
      }
    }

    if (LIVES_IS_PLAYING) {
      if (which == 0) lives_ruler_set_value(LIVES_RULER(mainw->hruler), ptrtime);
      if (cfile->achans > 0 && cfile->is_loaded && prefs->audio_src != AUDIO_SRC_EXT) {
        if (is_realtime_aplayer(prefs->audio_player) && (mainw->event_list == NULL || !mainw->preview)) {
#ifdef ENABLE_JACK
          if (mainw->jackd != NULL && prefs->audio_player == AUD_PLAYER_JACK) {
            offset = allocwidth * ((double)mainw->jackd->seek_pos / cfile->arate / cfile->achans /
                                   cfile->asampsize * 8) / CURRENT_CLIP_TOTAL_TIME;
          }
#endif
#ifdef HAVE_PULSE_AUDIO
          if (mainw->pulsed != NULL && prefs->audio_player == AUD_PLAYER_PULSE) {
            offset = allocwidth * ((double)mainw->pulsed->seek_pos / cfile->arate / cfile->achans /
                                   cfile->asampsize * 8) / CURRENT_CLIP_TOTAL_TIME;
          }
#endif
        } else offset = allocwidth * (mainw->aframeno - .5) / cfile->fps / CURRENT_CLIP_TOTAL_TIME;
      }
    }

    if (cfile->achans > 0) {
      bar_height = CE_AUDBAR_HEIGHT;
      if (mainw->laudio_drawable && (which == 0 || which == 2)) {
        allocheight = (double)lives_widget_get_allocation_height(mainw->laudbar) + bar_height
                      + widget_opts.packing_height * 2.5;
        allocy = lives_widget_get_allocation_y(mainw->laudbar) - widget_opts.packing_height;
        lives_painter_move_to(creb, offset, allocy);
        lives_painter_line_to(creb, offset, allocy + allocheight);
      }

      if (cfile->achans > 1 && (which == 0 || which == 3)) {
        if (mainw->raudio_drawable) {
          allocheight = (double)lives_widget_get_allocation_height(mainw->raudbar)
                        + bar_height + widget_opts.packing_height * 2.5;
          allocy = lives_widget_get_allocation_y(mainw->raudbar) - widget_opts.packing_height;
          lives_painter_move_to(creb, offset, allocy);
          lives_painter_line_to(creb, offset, allocy + allocheight);
        }
      }
    }

    lives_painter_stroke(creb);
    return TRUE;
  }
}


boolean all_expose_pb(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  if (LIVES_IS_PLAYING) all_expose(widget, cr, psurf);
  return TRUE;
}

boolean all_expose_nopb(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  if (!LIVES_IS_PLAYING) all_expose(widget, cr, psurf);
  return TRUE;
}

boolean expose_vid_draw(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  if (mainw->video_drawable) {
    lives_painter_set_source_surface(cr, mainw->video_drawable, 0., 0.);
    lives_painter_paint(cr);
  }
  return TRUE;
}

boolean config_vid_draw(LiVESWidget * widget, LiVESXEventConfigure * event, livespointer user_data) {
  if (mainw->video_drawable) lives_painter_surface_destroy(mainw->video_drawable);
  mainw->video_drawable = lives_widget_create_painter_surface(widget);
  clear_tbar_bgs(0, 0, 0, 0, 1);
  update_timer_bars(0, 0, 0, 0, 1);
  return TRUE;
}

boolean expose_laud_draw(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  if (mainw->laudio_drawable) {
    lives_painter_set_source_surface(cr, mainw->laudio_drawable, 0., 0.);
    lives_painter_paint(cr);
  }
  return TRUE;
}

boolean config_laud_draw(LiVESWidget * widget, LiVESXEventConfigure * event, livespointer user_data) {
  lives_painter_surface_t *surf = lives_widget_create_painter_surface(widget);
  if (mainw->laudio_drawable) lives_painter_surface_destroy(mainw->laudio_drawable);
  mainw->laudio_drawable = surf;
  if (IS_VALID_CLIP(mainw->drawsrc)) {
    mainw->files[mainw->drawsrc]->laudio_drawable = mainw->laudio_drawable;
  }
  return TRUE;
}


boolean expose_raud_draw(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  if (mainw->raudio_drawable) {
    lives_painter_set_source_surface(cr, mainw->raudio_drawable, 0., 0.);
    lives_painter_paint(cr);
  }
  return TRUE;
}

boolean config_raud_draw(LiVESWidget * widget, LiVESXEventConfigure * event, livespointer user_data) {
  lives_painter_surface_t *surf = lives_widget_create_painter_surface(widget);
  if (mainw->raudio_drawable) lives_painter_surface_destroy(mainw->raudio_drawable);
  mainw->raudio_drawable = surf;
  if (IS_VALID_CLIP(mainw->drawsrc)) {
    mainw->files[mainw->drawsrc]->raudio_drawable = mainw->raudio_drawable;
  }
  return TRUE;
}

boolean config_event2(LiVESWidget * widget, LiVESXEventConfigure * event, livespointer user_data) {
  mainw->msg_area_configed = TRUE;
  return TRUE;
}


/// genric func. to create surfaces
boolean all_config(LiVESWidget * widget, LiVESXEventConfigure * event, livespointer ppsurf) {
  lives_painter_t *cr;
  lives_painter_surface_t **psurf = (lives_painter_surface_t **)ppsurf;
  int rwidth, rheight;
  if (!psurf) return FALSE;
  if (*psurf) lives_painter_surface_destroy(*psurf);
  *psurf = lives_widget_create_painter_surface(widget);

#ifdef USE_SPECIAL_BUTTONS
  if (LIVES_IS_DRAWING_AREA(widget)) {
    LiVESWidget *parent = lives_widget_get_parent(widget);
    if (parent && is_standard_widget(parent)) {
      sbutt_render(parent, 0, NULL);
      return FALSE;
    }
  }
#endif
  cr = lives_painter_create_from_surface(*psurf);
  rwidth = lives_widget_get_allocation_width(widget);
  rheight = lives_widget_get_allocation_height(widget);
  lives_painter_render_background(widget, cr, 0., 0., rwidth, rheight);
  lives_painter_destroy(cr);
  lives_widget_queue_draw(widget);

  if (widget == mainw->start_image)
    load_start_image(CURRENT_CLIP_IS_VALID ? cfile->start : 0);
  else if (widget == mainw->end_image)
    load_end_image(CURRENT_CLIP_IS_VALID ? cfile->end : 0);
  else if (widget == mainw->preview_image)
    load_preview_image(FALSE);
  else if (widget == mainw->msg_area && !mainw->multitrack)
    msg_area_config(widget);
  else if (widget == mainw->dsu_widget)
    draw_dsu_widget(widget);
  else if (mainw->multitrack) {
    if (widget == mainw->multitrack->timeline_reg)
      draw_region(mainw->multitrack);
    else if (widget == mainw->play_image)
      mt_show_current_frame(mainw->multitrack, FALSE);
    else if (widget == mainw->multitrack->msg_area) {
      msg_area_config(widget);
    }
  }
  return FALSE;
}


boolean config_event(LiVESWidget * widget, LiVESXEventConfigure * event, livespointer user_data) {
  if (!mainw->configured) {
    mainw->configured = TRUE;
    return FALSE;
  }
  if (widget == LIVES_MAIN_WINDOW_WIDGET && !mainw->ignore_screen_size) {
    int scr_width, scr_height;
    scr_width = GUI_SCREEN_WIDTH;
    scr_height = GUI_SCREEN_HEIGHT;
    get_monitors(FALSE);
    if (scr_width != GUI_SCREEN_WIDTH || scr_height != GUI_SCREEN_HEIGHT) {
      g_print("RESIZE %d %d -> %d %d\n", scr_width, scr_height, GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT);
      resize_widgets_for_monitor(FALSE);
    }
    if (!CURRENT_CLIP_IS_VALID) {
      lives_ce_update_timeline(0, 0.);
    }
    return FALSE;
  }
  return FALSE;
}


// these two really belong with the processing widget

void on_effects_paused(LiVESButton * button, livespointer user_data) {
  char *com = NULL;
  ticks_t xticks;

  if (mainw->iochan != NULL || cfile->opening) {
    // pause during encoding (if we start using mainw->iochan for other things, this will
    // need changing...)
    if (!mainw->effects_paused) {
      lives_suspend_resume_process(cfile->handle, TRUE);

      if (!cfile->opening) {
        lives_button_set_label(LIVES_BUTTON(button), _("Resume"));
        lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label2), _("\nPaused\n(click Resume to continue processing)"));
        d_print(_("paused..."));
      }
    } else {
      lives_suspend_resume_process(cfile->handle, FALSE);

      if (!cfile->opening) {
        lives_button_set_label(LIVES_BUTTON(button), _("Paus_e"));
        lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label2), _("\nPlease Wait"));
        d_print(_("resumed..."));
      }
    }
  }

  if (mainw->iochan == NULL) {
    // pause during effects processing or opening
    xticks = lives_get_relative_ticks(mainw->origsecs, mainw->orignsecs);
    if (!mainw->effects_paused) {
      mainw->timeout_ticks -= xticks;
      com = lives_strdup_printf("%s pause \"%s\"", prefs->backend_sync, cfile->handle);
      if (!mainw->preview) {
        lives_button_set_label(LIVES_BUTTON(button), _("Resume"));
        if (!cfile->nokeep) {
          char *tmp, *ltext;

          if (!cfile->opening) {
            ltext = (_("Keep"));
          } else {
            ltext = (_("Enough"));
          }
          lives_button_set_label(LIVES_BUTTON(mainw->proc_ptr->cancel_button), ltext);
          lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label2),
                               (tmp = lives_strdup_printf
                                      (_("\nPaused\n(click %s to keep what you have and stop)\n(click Resume to continue processing)"),
                                       ltext)));
          lives_free(tmp);
          lives_free(ltext);
        }
        if ((mainw->multitrack == NULL && mainw->is_rendering && prefs->render_audio) || (mainw->multitrack != NULL &&
            mainw->multitrack->opts.render_audp)) {
          // render audio up to current tc
          mainw->flush_audio_tc = q_gint64((double)cfile->undo_end / cfile->fps * TICKS_PER_SECOND_DBL, cfile->fps);
          render_events(FALSE);
          mainw->flush_audio_tc = 0;
          cfile->afilesize = reget_afilesize_inner(mainw->current_file);
          cfile->laudio_time = (double)(cfile->afilesize / (cfile->asampsize >> 3) / cfile->achans) / (double)cfile->arate;
          if (cfile->achans > 1) {
            cfile->raudio_time = cfile->laudio_time;
          }
        }
        d_print(_("paused..."));
      }
#ifdef ENABLE_JACK
      if (mainw->jackd != NULL && mainw->jackd_read != NULL && mainw->jackd_read->in_use)
        if (mainw->proc_ptr->stop_button != NULL)
          lives_widget_hide(mainw->proc_ptr->stop_button);
#endif
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed != NULL && mainw->pulsed_read != NULL && mainw->pulsed_read->in_use)
        if (mainw->proc_ptr->stop_button != NULL)
          lives_widget_hide(mainw->proc_ptr->stop_button);
#endif
    } else {
      mainw->timeout_ticks += xticks;
      com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, cfile->handle);
      if (!mainw->preview) {
        if (cfile->opening || !cfile->nokeep) lives_button_set_label(LIVES_BUTTON(button), _("Pause/_Enough"));
        else lives_button_set_label(LIVES_BUTTON(button), _("Paus_e"));
        lives_button_set_label(LIVES_BUTTON(mainw->proc_ptr->cancel_button), _("Cancel"));
        lives_label_set_text(LIVES_LABEL(mainw->proc_ptr->label2), _("\nPlease Wait"));
        d_print(_("resumed..."));
      }
#ifdef ENABLE_JACK
      if (mainw->jackd != NULL && mainw->jackd_read != NULL && mainw->jackd_read->in_use)
        if (mainw->proc_ptr->stop_button != NULL)
          lives_widget_show(mainw->proc_ptr->stop_button);
#endif
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed != NULL && mainw->pulsed_read != NULL && mainw->pulsed_read->in_use)
        if (mainw->proc_ptr->stop_button != NULL)
          lives_widget_show(mainw->proc_ptr->stop_button);
#endif
    }

    if (!cfile->opening && !mainw->internal_messaging
        && !(
#ifdef ENABLE_JACK
          (mainw->jackd != NULL && mainw->jackd_read != NULL && mainw->jackd_read->in_use)
#else
          0
#endif
          ||
#ifdef HAVE_PULSE_AUDIO
          (mainw->pulsed != NULL && mainw->pulsed_read != NULL && mainw->pulsed->in_use)
#else
          0
#endif
        )) {
      lives_system(com, FALSE);
    }
  }
  lives_freep((void **)&com);
  mainw->effects_paused = !mainw->effects_paused;
}


void on_preview_clicked(LiVESButton * button, livespointer user_data) {
  // play an effect/tool preview
  // IMPORTANT: cfile->undo_start and cfile->undo_end determine which frames
  // should be played

  weed_plant_t *filter_map = mainw->filter_map; // back this up in case we are rendering
  weed_plant_t *afilter_map = mainw->afilter_map; // back this up in case we are rendering
  weed_plant_t *audio_event = mainw->audio_event;

  uint64_t old_rte; //TODO - block better
  ticks_t xticks;

  static volatile boolean in_preview_func = FALSE;

  boolean resume_after;
  boolean ointernal_messaging = mainw->internal_messaging;

  int ostart = cfile->start;
  int oend = cfile->end;

  int toy_type = mainw->toy_type;

  int current_file = mainw->current_file;

  if (in_preview_func) {
    // called a second time from playback loop
    // this is a special value of cancel - don't propogate it to "open"
    mainw->cancelled = CANCEL_NO_PROPOGATE;
    return;
  }

  in_preview_func = TRUE;

  old_rte = mainw->rte;
  xticks = lives_get_current_ticks();
  mainw->timeout_ticks -= xticks;

  if (mainw->internal_messaging) {
    mainw->internal_messaging = FALSE;
    // for realtime fx previews, we will switch all effects off and restore old
    // value after
    mainw->rte = EFFECT_NONE;
  }

  if (!LIVES_IS_PLAYING) {
    if (cfile->opening) {
      if (!cfile->opening_only_audio) {
        mainw->toy_type = LIVES_TOY_NONE;
        lives_widget_set_sensitive(mainw->toys_menu, FALSE);
      }
      if (mainw->multitrack == NULL && prefs->show_gui) lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);

      if (mainw->multitrack == NULL && !cfile->is_loaded) {
        if (mainw->play_window != NULL) {
          cfile->is_loaded = TRUE;
          resize_play_window();
          cfile->is_loaded = FALSE;
        }
      }
    }

    resume_after = FALSE;

    if (mainw->multitrack != NULL) {
      mt_prepare_for_playback(mainw->multitrack);
      if (cfile->opening) {
        lives_widget_set_sensitive(mainw->multitrack->playall, FALSE);
        lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
      }
    }

    if (user_data != NULL) {
      // called from multitrack
      /* if (mainw->play_window != NULL) { */
      /*   resize_play_window(); */
      /* } */
      if (mainw->multitrack != NULL && mainw->multitrack->is_rendering) {
        mainw->play_start = 1;
        mainw->play_end = cfile->frames;
      } else {
        mainw->play_start = 1;
        mainw->play_end = INT_MAX;
      }
    } else {
      mainw->preview = TRUE;
      if (!mainw->is_processing && !mainw->is_rendering) {
        mainw->play_start = cfile->start = cfile->undo_start;
        mainw->play_end = cfile->end = cfile->undo_end;
      } else {
        if (mainw->is_processing && mainw->is_rendering && prefs->render_audio) {
          // render audio up to current tc
          mainw->flush_audio_tc = q_gint64((double)cfile->undo_end / cfile->fps * TICKS_PER_SECOND_DBL, cfile->fps);
          render_events(FALSE);
          mainw->flush_audio_tc = 0;
          cfile->afilesize = reget_afilesize_inner(mainw->current_file);
          cfile->laudio_time = (double)(cfile->afilesize / (cfile->asampsize >> 3) / cfile->achans) / (double)cfile->arate;
          if (cfile->achans > 1) {
            cfile->raudio_time = cfile->laudio_time;
          }
        }
        mainw->play_start = calc_frame_from_time(mainw->current_file, event_list_get_start_secs(cfile->event_list));
        mainw->play_end = INT_MAX;
      }
    }

    if (cfile->opening) mainw->effects_paused = TRUE;
    else {
      // stop effects processing (if preferred)
      if (prefs->pause_effect_during_preview) {
        if (!(mainw->effects_paused)) {
          on_effects_paused(LIVES_BUTTON(mainw->proc_ptr->pause_button), NULL);
          resume_after = TRUE;
        }
      }
    }

    if (button != NULL) lives_button_set_label(LIVES_BUTTON(button), _("Stop"));
    if (mainw->proc_ptr != NULL) {
      lives_widget_set_sensitive(mainw->proc_ptr->pause_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, FALSE);
    }
    if (!cfile->opening) {
      lives_widget_set_sensitive(mainw->showfct, FALSE);
    }

    desensitize();

    if (cfile->opening || cfile->opening_only_audio) {
      lives_widget_hide(mainw->proc_ptr->processing);
      if (mainw->multitrack == NULL && !cfile->opening_audio) {
        showclipimgs();
      }
      resize(1);
    }

    if (ointernal_messaging) {
      lives_sync(3);
    }
    current_file = mainw->current_file;
    resize(1);

    // play the clip
    on_playsel_activate(NULL, LIVES_INT_TO_POINTER(TRUE));

    if (current_file != mainw->current_file) {
      if (mainw->is_rendering) {
        mainw->files[current_file]->next_event = cfile->next_event;
        cfile->next_event = NULL;
        mainw->current_file = current_file;
        init_conversions(LIVES_INTENTION_RENDER);
        mainw->effort = -EFFORT_RANGE_MAX;
      } else if (!mainw->multitrack) {
        switch_to_file((mainw->current_file = 0), current_file);
      }
    }

    if (cfile->opening) mainw->effects_paused = FALSE;
    else {
      // restart effects processing (if necessary)
      if (resume_after) on_effects_paused(LIVES_BUTTON(mainw->proc_ptr->pause_button), NULL);
    }
    // user_data is non-NULL if called from multitrack. We want to preserve the value of cancelled.
    if (user_data == NULL) mainw->cancelled = CANCEL_NONE;

    cfile->start = ostart;
    cfile->end = oend;

    mainw->toy_type = (lives_toy_t)toy_type;
    lives_widget_set_sensitive(mainw->toys_menu, TRUE);

    if (mainw->proc_ptr != NULL) {
      // proc_ptr can be NULL if we finished loading with a bg generator running
      lives_widget_show(mainw->proc_ptr->processing);
      lives_button_set_label(LIVES_BUTTON(button), _("Preview"));
      lives_widget_set_sensitive(mainw->proc_ptr->pause_button, TRUE);
      lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, TRUE);
    }
    mainw->preview = FALSE;
    desensitize();
    procw_desensitize();

    if (!cfile->opening) {
      lives_widget_set_sensitive(mainw->showfct, TRUE);
    } else {
      /*      for (i=1;i<MAX_FILES;i++) {
        if (mainw->files[i]!=NULL) {
        if (mainw->files[i]->menuentry!=NULL) {
        lives_widget_set_sensitive (mainw->files[i]->menuentry, TRUE);
        }}}*/
      if (mainw->multitrack == NULL && mainw->play_window != NULL) {
        resize_play_window();
      }
    }
  }

  if (mainw->preview_box != NULL) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
  lives_widget_set_tooltip_text(mainw->m_playbutton, _("Preview"));

  // redraw our bars for the clip
  if (!mainw->merge && mainw->multitrack == NULL) {
    get_play_times();
  }
  if (ointernal_messaging) {
    mainw->internal_messaging = TRUE;

    // switch realtime fx back on
    mainw->rte = old_rte;
  }

  if (mainw->play_window != NULL && mainw->fs && mainw->multitrack == NULL) {
    // this prevents a hang when the separate window is visible
    // it may be the first time we have shown it
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  }
  xticks = lives_get_current_ticks();
  mainw->timeout_ticks += xticks;
  mainw->filter_map = filter_map;
  mainw->afilter_map = afilter_map;
  mainw->audio_event = audio_event;

  if (mainw->multitrack != NULL) {
    current_file = mainw->current_file;
    mainw->current_file = mainw->multitrack->render_file;
    mt_post_playback(mainw->multitrack);
    mainw->current_file = current_file;
  }

  in_preview_func = FALSE;
}


void vj_mode_toggled(LiVESCheckMenuItem * menuitem, livespointer user_data) {
  if (lives_check_menu_item_get_active(menuitem) && (prefs->warning_mask & WARN_MASK_VJMODE_ENTER) == 0) {
    if (!(do_yesno_dialog_with_check(_("VJ Mode is specifically designed to make LiVES ready for realtime presentation.\n"
                                       "Enabling VJ restart will have the following effects:\n"
                                       "\n\n - On startup, audio source will be set to external.\n"
                                       "Clips willl reload without audio (although the audio files will remain on the disk).\n"
                                       "Additionally, when playing external audio, iVES to use the system clock for frame timings\n"
                                       "(rather than the soundcard) which may allow for slighly smoother playback.\n"
                                       "\n - only the lightest of checks will be done when reloading clips (unless a problem is detected "
                                       "during the reload.)\n\n"
                                       "Startup  will be almost instantaneous, however in the rare occurance of corruption to\n"
                                       "           a clip audio file, this will not be detected, as the file will not be loaded.\n"
                                       "\nOn startup, LiVES will grab the keyboard and screen focus if it can,\n"
                                       "\n - Shutdown will be slightly more rapid as no cleanup of the working directory will be attempted\n"
                                       "\n - Rendered effects will not be loaded, which willl further reduce the startup time.\n"
                                       "(Realtime effects will still be loaded as usual)\n"
                                       "\n - Any crash recovery files will be auto reloaded\n"
                                       "      making it convenient to  terminate  LiVES using ctrl-c or simply shutting down the machine\n"
                                       "\n - Continuous looping of video will be enabled automatically on startup\n"),
                                     WARN_MASK_VJMODE_ENTER))) {
      lives_signal_handler_block(mainw->vj_mode, mainw->vj_mode_func);
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->vj_mode), FALSE);
      lives_signal_handler_unblock(mainw->vj_mode, mainw->vj_mode_func);
      return;
    }
  }
  pref_factory_bool(PREF_VJMODE, !future_prefs->vj_mode, TRUE);
}


/**
   This is a super important function : almost everything related to velocity direction
   changes during playback should ensurre this function is called.
   For example it user_data is non-NULL, will also set the audio player rate / direction.
   Even if the clip is frozen, we still update the freeze fps so that when it is unfrozen it starts with the correct velocity.
*/
void changed_fps_during_pb(LiVESSpinButton * spinbutton, livespointer user_data) {
  /// user_data non-NULL to force audio rate update
  double new_fps;

  if (!LIVES_IS_PLAYING) return;
  if (!CURRENT_CLIP_IS_VALID) return;

  new_fps = lives_fix(lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton)), 3);

  if (user_data == NULL && ((!cfile->play_paused && cfile->pb_fps == new_fps) || (cfile->play_paused && new_fps == 0.))) {
    mainw->period = TICKS_PER_SECOND_DBL / cfile->pb_fps;
    return;
  }

  mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);

  if (new_fps != cfile->pb_fps && fabs(new_fps) > .001 && !cfile->play_paused) {
    /// we must scale the frame delta, since e.g if we were a halfway through the frame and the fps increased,
    /// we could end up jumping several frames
    ticks_t delta_ticks;
    delta_ticks = (mainw->currticks - mainw->startticks);
    delta_ticks = (ticks_t)((double)delta_ticks + fabs(cfile->pb_fps / new_fps));
    /// the time we would shown the last frame at using the new fps
    mainw->startticks = mainw->currticks - delta_ticks;
  }

  cfile->pb_fps = new_fps;
  mainw->period = TICKS_PER_SECOND_DBL / cfile->pb_fps;

  if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
    if (new_fps >= 0.) cfile->adirection = LIVES_DIRECTION_FORWARD;
    else cfile->adirection = LIVES_DIRECTION_REVERSE;
    // update our audio player
#ifdef ENABLE_JACK
    if (prefs->audio_src == AUDIO_SRC_INT && prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) {
      if (mainw->jackd->playing_file == mainw->current_file) {
        mainw->jackd->sample_in_rate = cfile->arate * cfile->pb_fps / cfile->fps;
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)
            && mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
          jack_get_rec_avals(mainw->jackd);
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*
#endif

#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_src == AUDIO_SRC_INT && prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL) {
      if (mainw->pulsed->playing_file == mainw->current_file) {
        mainw->pulsed->in_arate = cfile->arate * cfile->pb_fps / cfile->fps;
        if (mainw->pulsed->fd >= 0) {
          if (mainw->pulsed->in_arate > 0.) {
            lives_buffered_rdonly_set_reversed(mainw->pulsed->fd, FALSE);
          } else {
            lives_buffered_rdonly_set_reversed(mainw->pulsed->fd, TRUE);
          }
        }
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO)
            && mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
          pulse_get_rec_avals(mainw->pulsed);
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*
#endif
  }

  if (cfile->play_paused && new_fps != 0.) {
    cfile->freeze_fps = new_fps;
    // unfreeze the clip at the new (non-zero) fps rate
    freeze_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
    return;
  }

  if (cfile->pb_fps == 0. && !cfile->play_paused) {
    // freeze the clip
    freeze_callback(NULL, NULL, 0, (LiVESXModifierType)0, NULL);
    return;
  }
}


boolean on_mouse_scroll(LiVESWidget * widget, LiVESXEventScroll * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESXModifierType kstate = (LiVESXModifierType)event->state;
  uint32_t type = 1;

  if (!LIVES_IS_INTERACTIVE) return FALSE;
  if (mt != NULL) {
    // multitrack mode
    if ((kstate & LIVES_DEFAULT_MOD_MASK) == LIVES_CONTROL_MASK) {
      if (lives_get_scroll_direction(event) == LIVES_SCROLL_UP) mt_zoom_in(NULL, mt);
      else if (lives_get_scroll_direction(event) == LIVES_SCROLL_DOWN) mt_zoom_out(NULL, mt);
      return FALSE;
    }

    if (!prefs->mouse_scroll_clips) return FALSE;

    if (mt->poly_state == POLY_CLIPS) {
      // check if mouse pointer is over clip scroll tab
      LiVESXWindow *window = lives_display_get_window_at_pointer
                             ((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                              mt->display, NULL, NULL);

      if (widget == mt->clip_scroll || window == lives_widget_get_xwindow(mt->poly_box)) {
        // scroll fwd / back in clips
        if (lives_get_scroll_direction(event) == LIVES_SCROLL_UP) mt_prevclip(NULL, NULL, 0, (LiVESXModifierType)0, user_data);
        else if (lives_get_scroll_direction(event) == LIVES_SCROLL_DOWN) mt_nextclip(NULL, NULL, 0, (LiVESXModifierType)0, user_data);
      }
    }
    return FALSE;
  }

  // clip editor mode

  if (!prefs->mouse_scroll_clips) return FALSE;

  if ((kstate & LIVES_DEFAULT_MOD_MASK) == (LIVES_CONTROL_MASK | LIVES_SHIFT_MASK)) type = 2; // bg
  else if ((kstate & LIVES_DEFAULT_MOD_MASK) == LIVES_CONTROL_MASK) type = 0; // fg or bg

  if (lives_get_scroll_direction(event) == LIVES_SCROLL_UP) prevclip_callback(NULL, NULL, 0, (LiVESXModifierType)0,
        LIVES_INT_TO_POINTER(type));
  else if (lives_get_scroll_direction(event) == LIVES_SCROLL_DOWN) nextclip_callback(NULL, NULL, 0, (LiVESXModifierType)0,
        LIVES_INT_TO_POINTER(type));

  return TRUE;
}


// next few functions are for the timer bars
boolean on_mouse_sel_update(LiVESWidget * widget, LiVESXEventMotion * event, livespointer user_data) {
  if (!LIVES_IS_INTERACTIVE) return FALSE;

  if (CURRENT_CLIP_IS_VALID && mainw->sel_start > 0) {
    int x, sel_current;
    double tpos;

    lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                             LIVES_MAIN_WINDOW_WIDGET, &x, NULL);
    tpos = (double)x / (double)(lives_widget_get_allocation_width(mainw->video_draw) - 1) * CLIP_TOTAL_TIME(mainw->current_file);
    if (mainw->sel_move == SEL_MOVE_AUTO)
      sel_current = calc_frame_from_time3(mainw->current_file, tpos);
    else
      sel_current = calc_frame_from_time(mainw->current_file, tpos);

    if (mainw->sel_move == SEL_MOVE_SINGLE) {
      sel_current = calc_frame_from_time3(mainw->current_file, tpos);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), sel_current);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), sel_current);
    }

    if (mainw->sel_move == SEL_MOVE_START || (mainw->sel_move == SEL_MOVE_AUTO && sel_current < mainw->sel_start)) {
      sel_current = calc_frame_from_time(mainw->current_file, tpos);
      if (LIVES_IS_PLAYING && sel_current > cfile->end) sel_current = cfile->end;
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), sel_current);
    } else if (mainw->sel_move == SEL_MOVE_END || (mainw->sel_move == SEL_MOVE_AUTO && sel_current > mainw->sel_start)) {
      sel_current = calc_frame_from_time2(mainw->current_file, tpos);
      if (LIVES_IS_PLAYING && sel_current <= cfile->start) sel_current = cfile->start + 1;
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), sel_current - 1);
    }
  }
  return FALSE;
}


boolean on_mouse_sel_reset(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  if (!LIVES_IS_INTERACTIVE) return FALSE;

  if (mainw->current_file <= 0) return FALSE;
  mainw->sel_start = 0;
  if (!mainw->mouse_blocked) {
    lives_signal_handler_block(mainw->eventbox2, mainw->mouse_fn1);
    mainw->mouse_blocked = TRUE;
  }
  return FALSE;
}


boolean on_mouse_sel_start(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  int x;

  if (!LIVES_IS_INTERACTIVE) return FALSE;

  if (mainw->current_file <= 0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           LIVES_MAIN_WINDOW_WIDGET, &x, NULL);

  mainw->sel_start = calc_frame_from_time(mainw->current_file,
                                          (double)x / (double)(lives_widget_get_allocation_width(mainw->video_draw) - 1)
                                          * CLIP_TOTAL_TIME(mainw->current_file));

  if (event->button == 3 && !mainw->selwidth_locked) {
    mainw->sel_start = calc_frame_from_time3(mainw->current_file,
                       (double)x / (double)lives_widget_get_allocation_width(mainw->video_draw)
                       * CLIP_TOTAL_TIME(mainw->current_file));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->sel_start);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), mainw->sel_start);
    mainw->sel_move = SEL_MOVE_AUTO;
  }

  else {
    if (event->button == 2 && !mainw->selwidth_locked) {
      mainw->sel_start = calc_frame_from_time3(mainw->current_file,
                         (double)x / (double)lives_widget_get_allocation_width(mainw->video_draw)
                         * CLIP_TOTAL_TIME(mainw->current_file));
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->sel_start);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), (int)mainw->sel_start);
      mainw->sel_move = SEL_MOVE_SINGLE;
    }

    else {
      if (!mainw->selwidth_locked) {
        if ((mainw->sel_start < cfile->end && ((mainw->sel_start - cfile->start) <= (cfile->end - mainw->sel_start))) ||
            mainw->sel_start < cfile->start) {
          if (LIVES_IS_PLAYING && mainw->sel_start >= cfile->end) {
            mainw->sel_start = cfile->end;
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->sel_start);
          }
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->sel_start);
          mainw->sel_move = SEL_MOVE_START;
        } else {
          mainw->sel_start = calc_frame_from_time2(mainw->current_file,
                             (double)x / (double)lives_widget_get_allocation_width(mainw->video_draw)
                             * CLIP_TOTAL_TIME(mainw->current_file));
          if (LIVES_IS_PLAYING && mainw->sel_start <= cfile->start) {
            mainw->sel_start = cfile->start;
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), mainw->sel_start);
          } else
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), mainw->sel_start - 1);
          mainw->sel_move = SEL_MOVE_END;
        }
      } else {
        // locked selection
        if (mainw->sel_start > cfile->end) {
          // past end
          if (cfile->end + cfile->end - cfile->start + 1 <= cfile->frames) {
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end + cfile->end - cfile->start + 1);
            mainw->sel_move = SEL_MOVE_START;
          }
        } else {
          if (mainw->sel_start >= cfile->start) {
            if (mainw->sel_start > cfile->start + (cfile->end - cfile->start + 1) / 2) {
              // nearer to end
              lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), mainw->sel_start);
              mainw->sel_move = SEL_MOVE_END;
            } else {
              // nearer to start
              lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->sel_start);
              mainw->sel_move = SEL_MOVE_START;
            }
          } else {
            // before start
            if (cfile->start - cfile->end + cfile->start - 1 >= 1) {
              lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->start - 1);
              mainw->sel_move = SEL_MOVE_END;
	      // *INDENT-OFF*
	    }}}}}}
	  // *INDENT-ON*

  if (mainw->mouse_blocked) {// stops a warning if the user clicks around a lot...
    lives_signal_handler_unblock(mainw->eventbox2, mainw->mouse_fn1);
    mainw->mouse_blocked = FALSE;
  }
  return FALSE;
}


#ifdef ENABLE_GIW_3
void on_hrule_value_changed(LiVESWidget * widget, livespointer user_data) {

  if (!LIVES_IS_INTERACTIVE) return;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return;

  if (LIVES_IS_PLAYING) {
    lives_clip_t *pfile = mainw->files[mainw->playing_file];
    if (pfile->frames > 0) {
      pfile->frameno = pfile->last_frameno = calc_frame_from_time(mainw->playing_file,
                                             giw_timeline_get_value(GIW_TIMELINE(widget)));
      mainw->scratch = SCRATCH_JUMP;
    }
    return;
  }

  cfile->pointer_time = lives_ce_update_timeline(0, giw_timeline_get_value(GIW_TIMELINE(widget)));
  if (cfile->frames > 0) cfile->frameno = cfile->last_frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);

  if (cfile->pointer_time > 0.) {
    lives_widget_set_sensitive(mainw->rewind, TRUE);
    lives_widget_set_sensitive(mainw->trim_to_pstart, (cfile->achans * cfile->frames > 0));
    lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
    if (mainw->preview_box != NULL) {
      lives_widget_set_sensitive(mainw->p_rewindbutton, TRUE);
    }
  } else {
    lives_widget_set_sensitive(mainw->rewind, FALSE);
    lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
    lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
    if (mainw->preview_box != NULL) {
      lives_widget_set_sensitive(mainw->p_rewindbutton, FALSE);
    }
  }
  mainw->ptrtime = cfile->pointer_time;
  lives_widget_queue_draw(mainw->eventbox2);
}

#else

boolean on_hrule_update(LiVESWidget * widget, LiVESXEventMotion * event, livespointer user_data) {
  LiVESXModifierType modmask;
  LiVESXDevice *device;
  int x;
  if (LIVES_IS_PLAYING) return TRUE;
  if (!LIVES_IS_INTERACTIVE) return TRUE;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return TRUE;

  device = (LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device;
  lives_widget_get_modmask(device, widget, &modmask);

  if (!(modmask & LIVES_BUTTON1_MASK)) return TRUE;

  lives_widget_get_pointer(device, widget, &x, NULL);
  cfile->pointer_time = lives_ce_update_timeline(0,
                        (double)x / (double()lives_widget_get_allocation_width(widget) - 1)
                        * CLIP_TOTAL_TIME(mainw->current_file));
  if (cfile->frames > 0) cfile->frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
  return TRUE;
}


boolean on_hrule_reset(LiVESWidget * widget, LiVESXEventButton  * event, livespointer user_data) {
  //button release
  int x;

  if (LIVES_IS_PLAYING) return FALSE;
  if (!LIVES_IS_INTERACTIVE) return FALSE;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           widget, &x, NULL);
  cfile->pointer_time = lives_ce_update_timeline(0,
                        (double)x / (double)(lives_widget_get_allocation_width(widget) - 1)
                        * CLIP_TOTAL_TIME(mainw->current_file));
  if (cfile->frames > 0) {
    cfile->last_frameno = cfile->frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
  }
  if (cfile->pointer_time > 0.) {
    lives_widget_set_sensitive(mainw->rewind, TRUE);
    lives_widget_set_sensitive(mainw->trim_to_pstart, (cfile->achans * cfile->frames > 0));
    lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
    if (mainw->preview_box != NULL) {
      lives_widget_set_sensitive(mainw->p_rewindbutton, TRUE);
    }
  } else {
    lives_widget_set_sensitive(mainw->rewind, FALSE);
    lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
    lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
    if (mainw->preview_box != NULL) {
      lives_widget_set_sensitive(mainw->p_rewindbutton, FALSE);
    }
  }
  return FALSE;
}


boolean on_hrule_set(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  // button press
  int x;

  if (!LIVES_IS_INTERACTIVE) return FALSE;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return TRUE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           widget, &x, NULL);

  cfile->pointer_time = lives_ce_update_timeline(0,
                        (double)x / (double)(lives_widget_get_allocation_width(widget) - 1)
                        * CLIP_TOTAL_TIME(mainw->current_file));
  if (cfile->frames > 0) cfile->frameno = cfile->last_frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
  if (LIVES_IS_PLAYING) mainw->scratch = SCRATCH_JUMP;

  mainw->ptrtime = cfile->pointer_time;
  lives_widget_queue_draw(mainw->eventbox2);
  g_print("HRSET\n");
  return TRUE;
}

#endif


boolean frame_context(LiVESWidget * widget, LiVESXEventButton * event, livespointer which) {
  //popup a context menu when we right click on a frame

  LiVESWidget *save_frame_as;
  LiVESWidget *menu;

  int frame = 0;

  if (!LIVES_IS_INTERACTIVE) return FALSE;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return FALSE;

  if (mainw->multitrack != NULL && mainw->multitrack->event_list == NULL) return FALSE;

  // only accept right mouse clicks

  if (event->button != 3) return FALSE;

  if (mainw->multitrack == NULL) {
    switch (LIVES_POINTER_TO_INT(which)) {
    case 1:
      // start frame
      frame = cfile->start;
      break;
    case 2:
      // end frame
      frame = cfile->end;
      break;
    default:
      // preview frame
      frame = mainw->preview_frame;
      break;
    }
  }

  menu = lives_menu_new();
  lives_menu_set_title(LIVES_MENU(menu), _("Selected Frame"));

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  if (cfile->frames > 0 || mainw->multitrack != NULL) {
    save_frame_as = lives_standard_menu_item_new_with_label(_("_Save Frame as..."));
    lives_signal_sync_connect(LIVES_GUI_OBJECT(save_frame_as), LIVES_WIDGET_ACTIVATE_SIGNAL,
                              LIVES_GUI_CALLBACK(save_frame),
                              LIVES_INT_TO_POINTER(frame));

    if (capable->has_convert && capable->has_composite)
      lives_container_add(LIVES_CONTAINER(menu), save_frame_as);
  }

  lives_widget_show_all(menu);
  lives_menu_popup(LIVES_MENU(menu), event);

  return FALSE;
}


void on_less_pressed(LiVESButton * button, livespointer user_data) {
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return;

  if (!LIVES_IS_PLAYING || mainw->internal_messaging || (mainw->is_processing && cfile->is_loaded)) return;
  if (cfile->next_event != NULL) return;

  if (mainw->rte_keys != -1) {
    mainw->blend_factor -= prefs->blendchange_amount * (double)KEY_RPT_INTERVAL / 1000.;
    weed_set_blend_factor(mainw->rte_keys);
  }
}


void on_slower_pressed(LiVESButton * button, livespointer user_data) {
  double change = 1., new_fps;

  int type = 0;

  lives_clip_t *sfile = cfile;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return;

  if (!LIVES_IS_PLAYING || mainw->internal_messaging || (mainw->is_processing && cfile->is_loaded)) return;
  if (mainw->record && !(prefs->rec_opts & REC_FRAMES)) return;
  if (cfile->next_event != NULL) return;

  if (mainw->record && !mainw->record_paused && !(prefs->rec_opts & REC_FPS)) return;

  if (user_data != NULL) {
    type = LIVES_POINTER_TO_INT(user_data);
    if (type == SCREEN_AREA_BACKGROUND) {
      if (!IS_NORMAL_CLIP(mainw->blend_file)) return;
      sfile = mainw->files[mainw->blend_file];
    } else if (!IS_NORMAL_CLIP(mainw->current_file)) return;
  }

  if (sfile->next_event != NULL) return;

  change *= prefs->fpschange_amount / TICKS_PER_SECOND_DBL * (double)KEY_RPT_INTERVAL * sfile->pb_fps;

  if (sfile->pb_fps == 0.) return;
  if (sfile->pb_fps > 0.) {
    if (sfile->pb_fps < 0.1 || sfile->pb_fps < change) sfile->pb_fps = change;
    new_fps = sfile->pb_fps - change;
    if (sfile == cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), new_fps);
    else sfile->pb_fps = new_fps;
  } else {
    if (sfile->pb_fps > change) sfile->pb_fps = change;
    if (sfile == cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), (sfile->pb_fps - change));
    else sfile->pb_fps -= change;
  }
}


void on_more_pressed(LiVESButton * button, livespointer user_data) {
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return;

  if (!LIVES_IS_PLAYING || mainw->internal_messaging || (mainw->is_processing && cfile->is_loaded)) return;
  if (cfile->next_event != NULL) return;

  if (mainw->rte_keys != -1) {
    mainw->blend_factor += prefs->blendchange_amount * (double)KEY_RPT_INTERVAL / 1000.;
    weed_set_blend_factor(mainw->rte_keys);
  }
}


void on_faster_pressed(LiVESButton * button, livespointer user_data) {
  double change = 1.;
  int type = 0;

  lives_clip_t *sfile = cfile;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return;

  if (!LIVES_IS_PLAYING || mainw->internal_messaging || (mainw->is_processing && cfile->is_loaded)) return;
  if (mainw->record && !(prefs->rec_opts & REC_FRAMES)) return;
  if (cfile->next_event != NULL) return;

  if (mainw->record && !mainw->record_paused && !(prefs->rec_opts & REC_FPS)) return;

  if (user_data != NULL) {
    type = LIVES_POINTER_TO_INT(user_data);
    if (type == SCREEN_AREA_BACKGROUND) {
      if (!IS_NORMAL_CLIP(mainw->blend_file)) return;
      sfile = mainw->files[mainw->blend_file];
    } else if (!IS_NORMAL_CLIP(mainw->current_file)) return;
  }

  if (sfile->play_paused && sfile->freeze_fps < 0.) {
    sfile->pb_fps = -.00000001; // want to keep this as a negative value so when we unfreeze we still play in reverse
  }

  if (sfile->next_event != NULL) return;

  change *= prefs->fpschange_amount / TICKS_PER_SECOND_DBL * (double)KEY_RPT_INTERVAL
            * (sfile->pb_fps == 0. ? 1. : sfile->pb_fps);

  if (sfile->pb_fps >= 0.) {
    if (sfile->pb_fps == FPS_MAX) return;
    if (sfile->pb_fps < 0.5) sfile->pb_fps = .5;
    if (sfile->pb_fps > FPS_MAX - change) sfile->pb_fps = FPS_MAX - change;
    if (sfile == cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), (sfile->pb_fps + change));
    else sfile->pb_fps = sfile->pb_fps + change;
  } else {
    if (sfile->pb_fps == -FPS_MAX) return;
    if (sfile->pb_fps < -FPS_MAX - change) sfile->pb_fps = -FPS_MAX - change;
    if (sfile == cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), (sfile->pb_fps + change));
    else sfile->pb_fps = sfile->pb_fps + change;
  }
}


void on_back_pressed(LiVESButton * button, livespointer user_data) {
  int type = 0;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_NORMAL) return;
  if (mainw->internal_messaging || (mainw->is_processing && cfile->is_loaded)) return;
  if (LIVES_IS_PLAYING && !clip_can_reverse(mainw->current_file)) return;

  if (mainw->record && !(prefs->rec_opts & REC_FRAMES)) return;
  if (cfile->next_event != NULL) return;

  if (!LIVES_IS_PLAYING) {
    if (cfile->real_pointer_time > 0.) {
      cfile->real_pointer_time--;
      if (cfile->real_pointer_time < 0.) cfile->real_pointer_time = 0.;
      lives_ce_update_timeline(0, cfile->real_pointer_time);
    }
    return;
  }

  if (user_data != NULL) {
    type = LIVES_POINTER_TO_INT(user_data);
    if (type == SCREEN_AREA_BACKGROUND) return; // TODO: implement scratch play for the blend file
  }

  mainw->scratch = SCRATCH_BACK;
}


void on_forward_pressed(LiVESButton * button, livespointer user_data) {
  int type = 0;
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_NORMAL) return;
  if (mainw->internal_messaging || (mainw->is_processing && cfile->is_loaded)) return;

  if (mainw->record && !(prefs->rec_opts & REC_FRAMES)) return;
  if (cfile->next_event != NULL) return;

  if (!LIVES_IS_PLAYING) {
    if (cfile->real_pointer_time < CURRENT_CLIP_TOTAL_TIME) {
      cfile->real_pointer_time++;
      if (cfile->real_pointer_time > CURRENT_CLIP_TOTAL_TIME) cfile->real_pointer_time = CURRENT_CLIP_TOTAL_TIME;
      lives_ce_update_timeline(0, cfile->real_pointer_time);
    }
    return;
  }

  if (user_data != NULL) {
    type = LIVES_POINTER_TO_INT(user_data);
    if (type == SCREEN_AREA_BACKGROUND) return; // TODO: implement scratch play for the blend file
  }

  mainw->scratch = SCRATCH_FWD;
}


boolean freeze_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                        livespointer user_data) {
  weed_timecode_t tc;
  static boolean norecurse = FALSE;

  if (norecurse) return TRUE;

  if (mainw->multitrack != NULL && (LIVES_IS_PLAYING || mainw->multitrack->is_paused)) {
    on_playall_activate(NULL, NULL);
    return TRUE;
  }
  if (CURRENT_CLIP_IS_CLIPBOARD || !CURRENT_CLIP_IS_VALID) return TRUE;
  if (!LIVES_IS_PLAYING || (mainw->is_processing && cfile->is_loaded)) return TRUE;
  if (mainw->record && !(prefs->rec_opts & REC_FRAMES)) return TRUE;

  // TODO: make pref (reset keymode grab on freeze)
  //if (group != NULL) mainw->rte_keys = -1;

  if (cfile->play_paused) {
    cfile->pb_fps = cfile->freeze_fps;
    if (cfile->pb_fps != 0.) mainw->period = TICKS_PER_SECOND_DBL / cfile->pb_fps;
    else mainw->period = INT_MAX;
    cfile->play_paused = FALSE;
    if (mainw->record && !mainw->record_paused) {
      pthread_mutex_lock(&mainw->event_list_mutex);
      // write a RECORD_START marker
      tc = get_event_timecode(get_last_event(mainw->event_list));
      mainw->event_list = append_marker_event(mainw->event_list, tc, EVENT_MARKER_RECORD_START); // mark record end
      pthread_mutex_unlock(&mainw->event_list_mutex);
    }
  } else {
    if (mainw->record) {
      pthread_mutex_lock(&mainw->event_list_mutex);
      // write a RECORD_END marker
      tc = get_event_timecode(get_last_event(mainw->event_list));
      mainw->event_list = append_marker_event(mainw->event_list, tc, EVENT_MARKER_RECORD_END); // mark record end
      pthread_mutex_unlock(&mainw->event_list_mutex);
    }

    cfile->freeze_fps = cfile->pb_fps;
    cfile->play_paused = TRUE;
    cfile->pb_fps = 0.;
    mainw->timeout_ticks = mainw->currticks;
  }

  if (group != NULL) {
    norecurse = TRUE;
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->pb_fps);
    norecurse = FALSE;
  }

  if (prefs->audio_src == AUDIO_SRC_INT) {
#ifdef ENABLE_JACK
    if (mainw->jackd != NULL && prefs->audio_player == AUD_PLAYER_JACK
        && mainw->jackd->playing_file == mainw->playing_file && (prefs->jack_opts & JACK_OPTS_NOPLAY_WHEN_PAUSED ||
            prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
      mainw->jackd->is_paused = cfile->play_paused;
      if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO) && mainw->agen_key == 0 &&
          !mainw->agen_needs_reinit) {
        if (cfile->play_paused) {
          weed_plant_t *event = get_last_frame_event(mainw->event_list);
          insert_audio_event_at(event, -1, mainw->jackd->playing_file, 0., 0.); // audio switch off
        } else {
          jack_get_rec_avals(mainw->jackd);
        }
      }
      if (cfile->play_paused) jack_pb_stop();
      else jack_pb_start();
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed != NULL && prefs->audio_player == AUD_PLAYER_PULSE
        && mainw->pulsed->playing_file == mainw->playing_file && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
      if (!cfile->play_paused) mainw->pulsed->in_arate = cfile->arate * cfile->pb_fps / cfile->fps;
      mainw->pulsed->is_paused = cfile->play_paused;
      if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO) && mainw->agen_key == 0 &&
          !mainw->agen_needs_reinit) {
        if (cfile->play_paused) {
          if (!mainw->mute) {
            weed_plant_t *event = get_last_frame_event(mainw->event_list);
            insert_audio_event_at(event, -1, mainw->pulsed->playing_file, 0., 0.); // audio switch off
          }
        } else {
          pulse_get_rec_avals(mainw->pulsed);
        }
      }
    }
#endif
  }
  if (LIVES_IS_PLAYING) mainw->force_show = TRUE;
  return TRUE;
}


boolean nervous_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                         livespointer clip_number) {
  if (mainw->multitrack != NULL) return FALSE;
  mainw->nervous = !mainw->nervous;
  return TRUE;
}


boolean aud_lock_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                          livespointer statep) {
  boolean state = LIVES_POINTER_TO_INT(statep);
  if (!LIVES_IS_PLAYING || !is_realtime_aplayer(prefs->audio_player) || mainw->multitrack != NULL
      || mainw->is_rendering || mainw->preview || mainw->agen_key != 0 || mainw->agen_needs_reinit
      || prefs->audio_src == AUDIO_SRC_EXT) return TRUE;

  if (!state) {
    // lock OFF
    prefs->audio_opts |= (AUDIO_OPTS_FOLLOW_CLIPS & future_prefs->audio_opts);
    return TRUE;
  }
  prefs->audio_opts = ((prefs->audio_opts | AUDIO_OPTS_FOLLOW_CLIPS) ^ AUDIO_OPTS_FOLLOW_CLIPS);
  if (switch_audio_clip(mainw->current_file, TRUE)) {
    if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
      mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
      resync_audio(cfile->frameno);
      changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), LIVES_INT_TO_POINTER(TRUE));
    }
  }
  return TRUE;
}


char *get_palette_name_for_clip(int clipno) {
  lives_clip_t *sfile;
  char *palname = NULL;
  if (!IS_VALID_CLIP(clipno)) return NULL;
  sfile = mainw->files[clipno];
  if (IS_NORMAL_CLIP(clipno)) {
    if (is_virtual_frame(clipno, sfile->frameno)) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
      palname = lives_strdup(weed_palette_get_name_full(cdata->current_palette, cdata->YUV_clamping, cdata->YUV_subspace));
    } else {
      palname = lives_strdup(weed_palette_get_name((sfile->bpp == 24 ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32)));
    }
  } else switch (sfile->clip_type) {
    case CLIP_TYPE_GENERATOR: {
      weed_plant_t *inst = (weed_plant_t *)sfile->ext_src;
      if (inst) {
        weed_plant_t *channel = get_enabled_channel(inst, 0, FALSE);
        if (channel) {
          int clamping, subspace, pal;
          pal = weed_channel_get_palette_yuv(channel, &clamping, NULL, &subspace);
          palname = lives_strdup(weed_palette_get_name_full(pal, clamping, subspace));
        }
      }
    }
    break;
    case CLIP_TYPE_VIDEODEV: {
#ifdef HAVE_UNICAP
      lives_vdev_t *ldev = (lives_vdev_t *)sfile->ext_src;
      palname = lives_strdup(weed_palette_get_name_full(ldev->current_palette, ldev->YUV_clamping, 0));
#endif
    }
    break;
    default: break;
    }
  if (!palname) palname = lives_strdup("??????");
  return palname;
}


boolean show_sync_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer keybd) {
  if (!LIVES_IS_PLAYING) return FALSE;
  if (!CURRENT_CLIP_HAS_VIDEO || CURRENT_CLIP_IS_CLIPBOARD) return FALSE;

  if (!prefs->show_dev_opts) {
    int last_dprint_file = mainw->last_dprint_file;
    mainw->no_switch_dprint = TRUE;
    d_print_overlay(2.0, _("Playing frame %d / %d, at fps %.3f\n"),
                    mainw->actual_frame, cfile->frames, cfile->pb_fps);
    mainw->no_switch_dprint = FALSE;
    mainw->last_dprint_file = last_dprint_file;
    return FALSE;
  }

#ifdef USE_LIVES_MFUNCS
  show_memstats();
#endif

  lives_freep((void **)&mainw->overlay_msg);

  if (!keybd) mainw->lockstats = !mainw->lockstats;
  if (!mainw->lockstats) return FALSE;

  mainw->overlay_msg = get_stats_msg(FALSE);
  return FALSE;
}


boolean storeclip_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                           livespointer clip_number) {
  // ctrl-fn key will store a clip for higher switching
  int clip = LIVES_POINTER_TO_INT(clip_number) - 1;
  register int i;

  if (!LIVES_IS_INTERACTIVE) return TRUE;

  if (!CURRENT_CLIP_IS_VALID || mainw->preview || (LIVES_IS_PLAYING && mainw->event_list != NULL && !mainw->record)
      || (mainw->is_processing && cfile->is_loaded) ||
      mainw->cliplist == NULL) return TRUE;

  if (clip >= FN_KEYS - 1) {
    // last fn key will clear all
    for (i = 0; i < FN_KEYS - 1; i++) {
      mainw->clipstore[i][0] = -1;
    }
    return TRUE;
  }

  if (!IS_VALID_CLIP(mainw->clipstore[clip][0])) {
    mainw->clipstore[clip][0] = mainw->current_file;
    if (LIVES_IS_PLAYING) {
      mainw->clipstore[clip][1] = mainw->actual_frame;
    } else {
      int frame = calc_frame_from_time4(mainw->current_file, cfile->pointer_time);
      if (frame <= cfile->frames) mainw->clipstore[clip][1] = frame;
      else mainw->clipstore[clip][1] = 1;
    }
  } else {
    lives_clip_t *sfile = mainw->files[mainw->clipstore[clip][0]];
    if (LIVES_IS_PLAYING) {
      sfile->frameno = sfile->last_frameno = mainw->clipstore[clip][1];
      mainw->scratch = SCRATCH_JUMP;
    }
    if ((LIVES_IS_PLAYING && mainw->clipstore[clip][0] != mainw->playing_file)
        || (!LIVES_IS_PLAYING && mainw->clipstore[clip][0] != mainw->current_file)) {
      switch_clip(0, mainw->clipstore[clip][0], TRUE);
    }
    if (!LIVES_IS_PLAYING) {
      cfile->real_pointer_time = (mainw->clipstore[clip][1] - 1.) / cfile->fps;
      lives_ce_update_timeline(0, cfile->real_pointer_time);
    }
  }
  return TRUE;
}


void on_toolbar_hide(LiVESButton * button, livespointer user_data) {
  lives_widget_hide(mainw->tb_hbox);
  fullscreen_internal();
  future_prefs->show_tool = FALSE;
}


void on_capture_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char **array;
  double rec_end_time = -1.;
  char *com;
  boolean sgui;
  int curr_file = mainw->current_file;
  LiVESResponseType response;
  mainw->mt_needs_idlefunc = FALSE;

#if !GTK_CHECK_VERSION(3, 0, 0)
#ifndef GDK_WINDOWING_X11
  do_error_dialog(
    _("\n\nThis function will only work with X11.\nPlease send a patch to get it working on other platforms.\n\n"));
  return;
#endif
#endif

  if (!capable->has_xwininfo) {
    do_error_dialog(_("\n\nYou must install \"xwininfo\" before you can use this feature\n\n"));
    return;
  }

  if (mainw->first_free_file == ALL_USED) {
    too_many_files();
    return;
  }

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (prefs->rec_desktop_audio && ((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) ||
                                   (prefs->audio_player == AUD_PLAYER_PULSE && capable->has_pulse_audio))) {
    resaudw = create_resaudw(8, NULL, NULL);
  } else {
    resaudw = create_resaudw(9, NULL, NULL);
  }
  response = lives_dialog_run(LIVES_DIALOG(resaudw->dialog));

  if (response != LIVES_RESPONSE_OK) {
    lives_widget_destroy(resaudw->dialog);

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  if (prefs->rec_desktop_audio && ((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) ||
                                   (prefs->audio_player == AUD_PLAYER_PULSE && capable->has_pulse_audio))) {
    mainw->rec_arate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
    mainw->rec_achans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
    mainw->rec_asamps = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      mainw->rec_signed_endian = AFORM_UNSIGNED;
    }
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      mainw->rec_signed_endian |= AFORM_BIG_ENDIAN;
    }
  } else {
    mainw->rec_arate = mainw->rec_achans = mainw->rec_asamps = mainw->rec_signed_endian = 0;
  }

  mainw->rec_fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->fps_spinbutton));

  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton))) {
    rec_end_time = (lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->hour_spinbutton)) * 60.
                    + lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->minute_spinbutton))) * 60.
                   + lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->second_spinbutton));
    mainw->rec_vid_frames = (rec_end_time * mainw->rec_fps + .5);
  } else mainw->rec_vid_frames = -1;

  lives_widget_destroy(resaudw->dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  lives_freep((void **)&resaudw);

  if (prefs->rec_desktop_audio && mainw->rec_arate <= 0 && ((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) ||
      (prefs->audio_player == AUD_PLAYER_PULSE && capable->has_pulse_audio))) {
    do_audrate_error_dialog();
    return;
  }

  if (rec_end_time == 0.) {
    widget_opts.non_modal = TRUE;
    do_error_dialog(_("\nRecord time must be greater than 0.\n"));
    widget_opts.non_modal = FALSE;
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  sgui = prefs->show_gui;
  prefs->show_gui = FALSE;

  if (!(do_warning_dialog(
          _("Capture an External Window:\n\nClick on 'OK', then click on any window to capture it\n"
            "Click 'Cancel' to cancel\n\n")))) {
    if (sgui) {
      prefs->show_gui = TRUE;
      lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
    }
    d_print(_("External window was released.\n"));
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  prefs->show_gui = sgui;

  // an example of using 'get_temp_handle()' ////////
  if (!get_temp_handle(-1)) {
    if (prefs->show_gui) {
      lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    }

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  com = lives_strdup_printf("%s get_window_id \"%s\"", prefs->backend, cfile->handle);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    close_temp_handle(curr_file);
    if (prefs->show_gui) {
      lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    }

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  do_progress_dialog(TRUE, FALSE, _("Click on a Window to Capture it\nPress 'q' to stop recording"));

  if (get_token_count(mainw->msg, '|') < 6) {
    close_temp_handle(curr_file);
    if (prefs->show_gui) {
      lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    }

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  array = lives_strsplit(mainw->msg, "|", -1);
#if IS_MINGW
  mainw->foreign_id = (HWND)atoi(array[1]);
#else
#if GTK_CHECK_VERSION(3, 0, 0) || defined GUI_QT
  mainw->foreign_id = (Window)atoi(array[1]);
#else
  mainw->foreign_id = (GdkNativeWindow)atoi(array[1]);
#endif
#endif
  mainw->foreign_width = atoi(array[2]);
  mainw->foreign_height = atoi(array[3]);
  mainw->foreign_bpp = atoi(array[4]);
  mainw->foreign_visual = lives_strdup(array[5]);
  lives_strfreev(array);

  close_temp_handle(curr_file);

  ////////////////////////////////////////

  d_print(_("\nExternal window captured. Width=%d, height=%d, bpp=%d. *Do not resize*\n\n"
            "Stop or 'q' to finish.\n(Default of %.3f frames per second will be used.)\n"),
          mainw->foreign_width, mainw->foreign_height, mainw->foreign_bpp, mainw->rec_fps);

  // start another copy of LiVES and wait for it to return values
  com = lives_strdup_printf("%s -capture %d %u %d %d %s %d %d %.4f %d %d %d %d \"%s\"",
                            capable->myname_full, capable->mainpid,
                            (unsigned int)mainw->foreign_id, mainw->foreign_width, mainw->foreign_height,
                            get_image_ext_for_type(IMG_TYPE_BEST),
                            mainw->foreign_bpp, mainw->rec_vid_frames, mainw->rec_fps, mainw->rec_arate,
                            mainw->rec_asamps, mainw->rec_achans, mainw->rec_signed_endian, mainw->foreign_visual);

  // force the dialog to disappear
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  lives_system(com, FALSE);

  if (prefs->show_gui) {
    lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
  }

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  if (!after_foreign_play() && mainw->cancelled == CANCEL_NONE) {
    widget_opts.non_modal = TRUE;
    do_error_dialog(_("LiVES was unable to capture this window. Sorry.\n"));
    widget_opts.non_modal = FALSE;
    sensitize();
  }

  if (mainw->multitrack != NULL) {
    polymorph(mainw->multitrack, POLY_NONE);
    polymorph(mainw->multitrack, POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
}


void on_capture2_activate(void) {
  // this is in the second copy of lives, we are now going to grab frames from the X window
  char *capfilename = lives_strdup_printf(".capture.%d", mainw->foreign_key);
  char *capfile = lives_build_filename(prefs->workdir, capfilename, NULL);

  char buf[32];

  boolean retval;
  int capture_fd;
  register int i;

  retval = prepare_to_play_foreign();

  lives_freep((void **)&mainw->foreign_visual);

  if (!retval) exit(2);

  mainw->record_foreign = TRUE; // for now...

  play_file();

  // pass the handle and frames back to the caller
  capture_fd = creat(capfile, S_IRUSR | S_IWUSR);
  if (capture_fd < 0) {
    lives_free(capfile);
    exit(1);
  }

  for (i = 1; i < MAX_FILES; i++) {
    if (mainw->files[i] == NULL) break;
    lives_write(capture_fd, mainw->files[i]->handle, lives_strlen(mainw->files[i]->handle), TRUE);
    lives_write(capture_fd, "|", 1, TRUE);
    lives_snprintf(buf, 32, "%d", cfile->frames);
    lives_write(capture_fd, buf, strlen(buf), TRUE);
    lives_write(capture_fd, "|", 1, TRUE);
  }

  close(capture_fd);
  lives_free(capfilename);
  lives_free(capfile);
  exit(0);
}


// TODO - move all encoder related stuff from here and plugins.c into encoders.c
void on_encoder_ofmt_changed(LiVESCombo * combo, livespointer user_data) {
  // change encoder format in the encoder plugin
  render_details *rdet = (render_details *)user_data;

  LiVESList *ofmt_all = NULL;

  char **array;
  char *new_fmt;

  int counter;
  register int i;

  if (rdet == NULL) {
    new_fmt = lives_combo_get_active_text(LIVES_COMBO(prefsw->ofmt_combo));
  } else {
    new_fmt = lives_combo_get_active_text(LIVES_COMBO(rdet->ofmt_combo));
  }

  if (!strlen(new_fmt) || !strcmp(new_fmt, mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) {
    lives_free(new_fmt);
    return;
  }

  if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, future_prefs->encoder.name, "get_formats")) != NULL) {
    // get details for the current format
    counter = 0;
    for (i = 0; i < lives_list_length(ofmt_all); i++) {
      if (get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|') > 2) {
        array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);

        if (!strcmp(array[1], new_fmt)) {
          if (prefsw != NULL) {
            lives_signal_handler_block(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
            lives_combo_set_active_index(LIVES_COMBO(prefsw->ofmt_combo), counter);
            lives_signal_handler_unblock(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
          }
          if (rdet != NULL) {
            lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
            lives_combo_set_active_index(LIVES_COMBO(rdet->ofmt_combo), counter);
            lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
          }
          lives_snprintf(future_prefs->encoder.of_name, 64, "%s", array[0]);
          lives_snprintf(future_prefs->encoder.of_desc, 128, "%s", array[1]);

          future_prefs->encoder.of_allowed_acodecs = atoi(array[2]);
          lives_snprintf(future_prefs->encoder.of_restrict, 1024, "%s", array[3]);
          lives_strfreev(array);
          break;
        }
        lives_strfreev(array);
        counter++;
      }
    }
    lives_list_free_all(&ofmt_all);
    lives_free(new_fmt);

    if (rdet != NULL && prefsw == NULL) {
      if (strcmp(prefs->encoder.of_name, future_prefs->encoder.of_name)) {
        rdet->enc_changed = TRUE;
        lives_snprintf(prefs->encoder.of_name, 64, "%s", future_prefs->encoder.of_name);
        lives_snprintf(prefs->encoder.of_desc, 128, "%s", future_prefs->encoder.of_desc);
        lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", future_prefs->encoder.of_restrict);
        prefs->encoder.of_allowed_acodecs = future_prefs->encoder.of_allowed_acodecs;
        set_string_pref(PREF_OUTPUT_TYPE, prefs->encoder.of_name);
      }
    }
    set_acodec_list_from_allowed(prefsw, rdet);
  } else {
    do_plugin_encoder_error(future_prefs->encoder.name);
  }
}

// TODO - move all this to audio.c

void on_export_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {

  char *filt[] = {"*."LIVES_FILE_EXT_WAV, NULL};
  char *filename, *file_name;
  char *com, *tmp;

  double start, end;

  int nrate = cfile->arps;
  int asigned = !(cfile->signed_endian & AFORM_UNSIGNED);

  if (cfile->end > 0 && !LIVES_POINTER_TO_INT(user_data)) {
    filename = choose_file((*mainw->audio_dir) ? mainw->audio_dir : NULL, NULL,
                           filt, LIVES_FILE_CHOOSER_ACTION_SAVE, _("Export Selected Audio as..."), NULL);
  } else {
    filename = choose_file((*mainw->audio_dir) ? mainw->audio_dir : NULL, NULL,
                           filt, LIVES_FILE_CHOOSER_ACTION_SAVE, _("Export Audio as..."), NULL);
  }

  if (filename == NULL) return;
  file_name = ensure_extension(filename, LIVES_FILE_EXT_WAV);
  lives_free(filename);

  // warn if arps!=arate
  if (cfile->arate != cfile->arps) {
    if (do_warning_dialog(
          _("\n\nThe audio playback speed has been altered for this clip.\nClick 'OK' to export at the new speed, "
            "or 'Cancel' to export at the original rate.\n"))) {
      nrate = cfile->arate;
    }
  }

  if (cfile->start * cfile->end > 0 && !LIVES_POINTER_TO_INT(user_data)) {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, _("Exporting audio frames %d to %d as %s..."), cfile->start, cfile->end, file_name);
    start = calc_time_from_frame(mainw->current_file, cfile->start);
    end = calc_time_from_frame(mainw->current_file, cfile->end);
  } else {
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, _("Exporting audio as %s..."), file_name);
    start = 0.;
    end = 0.;
  }

  d_print(mainw->msg);

  com = lives_strdup_printf("%s export_audio \"%s\" %.8f %.8f %d %d %d %d %d \"%s\"", prefs->backend, cfile->handle,
                            start, end, cfile->arps, cfile->achans, cfile->asampsize, asigned, nrate,
                            (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));
  lives_free(tmp);

  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    lives_free(file_name);
    d_print_failed();
    return;
  }

  do_progress_dialog(TRUE, FALSE, _("Exporting audio"));

  if (mainw->error) {
    d_print_failed();
    widget_opts.non_modal = TRUE;
    do_error_dialog(mainw->msg);
    widget_opts.non_modal = FALSE;
  } else {
    d_print_done();
    lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", file_name);
    get_dirname(mainw->audio_dir);
  }
  lives_free(file_name);
}


void on_append_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *chooser;

  char *filt[] = LIVES_AUDIO_LOAD_FILTER;

  char *com, *tmp, *tmp2;
  char *a_type;

  uint32_t chk_mask = WARN_MASK_LAYOUT_ALTER_AUDIO;

  boolean gotit = FALSE;

  int asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
  int aendian = !(cfile->signed_endian & AFORM_BIG_ENDIAN);

  int resp;

  register int i;

  if (!CURRENT_CLIP_IS_VALID) return;

  if (!check_for_layout_errors(NULL, mainw->current_file, 1, 0, &chk_mask)) {
    return;
  }

  chooser = choose_file_with_preview((*mainw->audio_dir) ? mainw->audio_dir : NULL, _("Append Audio File"), filt,
                                     LIVES_FILE_SELECTION_AUDIO_ONLY);

  resp = lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();

  if (resp != LIVES_RESPONSE_ACCEPT) {
    on_filechooser_cancel_clicked(chooser);
    unbuffer_lmap_errors(FALSE);
    return;
  }

  lives_snprintf(file_name, PATH_MAX, "%s",
                 (tmp = lives_filename_to_utf8((tmp2 = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(
                          chooser))),
                        -1, NULL, NULL, NULL)));
  lives_free(tmp);
  lives_free(tmp2);

  lives_widget_destroy(LIVES_WIDGET(chooser));

  lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

  lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", file_name);
  get_dirname(mainw->audio_dir);

  a_type = get_extension(file_name);

  if (strlen(a_type)) {
    char *filt[] = LIVES_AUDIO_LOAD_FILTER;
    for (i = 0; filt[i] != NULL; i++) {
      if (!lives_ascii_strcasecmp(a_type, filt[i] + 2)) gotit = TRUE; // skip past "*." in filt
    }
  }

  if (gotit) {
    com = lives_strdup_printf("%s append_audio \"%s\" \"%s\" %d %d %d %d %d \"%s\"", prefs->backend, cfile->handle,
                              a_type, cfile->arate,
                              cfile->achans, cfile->asampsize, asigned, aendian,
                              (tmp = lives_filename_from_utf8(file_name, -1, NULL, NULL, NULL)));
    lives_free(tmp);
  } else {
    lives_free(a_type);
    do_audio_import_error();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    unbuffer_lmap_errors(FALSE);
    return;
  }

  lives_free(a_type);

  lives_snprintf(mainw->msg, MAINW_MSG_SIZE, _("Appending audio file %s..."), file_name);
  d_print(""); // force switchtext
  d_print(mainw->msg);

  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (!do_progress_dialog(TRUE, TRUE, _("Appending audio"))) {
    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;
    lives_rm(cfile->info_file);
    com = lives_strdup_printf("%s cancel_audio \"%s\"", prefs->backend, cfile->handle);
    lives_system(com, FALSE);
    if (!THREADVAR(com_failed)) {
      do_auto_dialog(_("Cancelling"), 0);
      check_backend_return(cfile);
    }
    lives_free(com);
    reget_afilesize(mainw->current_file);
    if (mainw->error) d_print_failed();
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (mainw->error) {
    d_print_failed();
    widget_opts.non_modal = TRUE;
    do_error_dialog(mainw->msg);
    widget_opts.non_modal = FALSE;
  } else {
    lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

    com = lives_strdup_printf("%s commit_audio \"%s\"", prefs->backend, cfile->handle);
    mainw->cancelled = CANCEL_NONE;
    mainw->error = FALSE;
    lives_rm(cfile->info_file);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      d_print_failed();
      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      }
      unbuffer_lmap_errors(FALSE);
      return;
    }

    do_auto_dialog(_("Committing audio"), 0);
    check_backend_return(cfile);
    if (mainw->error) {
      d_print_failed();
      widget_opts.non_modal = TRUE;
      if (mainw->cancelled != CANCEL_ERROR) do_error_dialog(mainw->msg);
      widget_opts.non_modal = FALSE;
    } else {
      get_dirname(file_name);
      lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", file_name);
      reget_afilesize(mainw->current_file);
      cfile->changed = TRUE;
      d_print_done();
    }
  }
  cfile->undo_action = UNDO_APPEND_AUDIO;
  set_undoable(_("Append Audio"), !prefs->conserve_space);

  if (mainw->multitrack != NULL) {
    mt_sensitise(mainw->multitrack);
    maybe_add_mt_idlefunc();
  }
  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}


boolean on_trim_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // type 0 == trim selected
  // type 1 == trim to play pointer

  char *com, *msg, *tmp;

  double start, end;

  uint32_t chk_mask = WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_ALTER_AUDIO;

  int type = LIVES_POINTER_TO_INT(user_data);

  if (!CURRENT_CLIP_IS_VALID) return FALSE;

  if (type == 0) {
    start = calc_time_from_frame(mainw->current_file, cfile->start);
    end = calc_time_from_frame(mainw->current_file, cfile->end + 1);
  } else {
    start = 0.;
    end = cfile->pointer_time;
  }

  tmp = (_("Deletion"));
  if (!check_for_layout_errors(tmp, mainw->current_file, calc_frame_from_time(mainw->current_file, start),
                               calc_frame_from_time(mainw->current_file, end), &chk_mask)) {
    lives_free(tmp);
    return FALSE;
  }
  lives_free(tmp);

  if (end > cfile->laudio_time && end > cfile->raudio_time)
    msg = lives_strdup_printf(_("Padding audio to %.2f seconds..."), end);
  else
    msg = lives_strdup_printf(_("Trimming audio from %.2f to %.2f seconds..."), start, end);

  d_print(msg);
  lives_free(msg);

  com = lives_strdup_printf("%s trim_audio \"%s\" %.8f %.8f %d %d %d %d %d", prefs->backend, cfile->handle,
                            start, end, cfile->arate,
                            cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                            !(cfile->signed_endian & AFORM_BIG_ENDIAN));
  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    unbuffer_lmap_errors(FALSE);
    d_print_failed();
    return FALSE;
  }

  do_progress_dialog(TRUE, FALSE, _("Trimming/Padding audio"));

  if (mainw->error) {
    d_print_failed();
    unbuffer_lmap_errors(FALSE);
    return FALSE;
  }

  if (!prefs->conserve_space) {
    set_undoable(_("Trim/Pad Audio"), !prefs->conserve_space);
    cfile->undo_action = UNDO_TRIM_AUDIO;
  }

  reget_afilesize(mainw->current_file);
  cfile->changed = TRUE;
  d_print_done();

  if (mainw->sl_undo_mem != NULL && cfile->stored_layout_audio != 0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
  return TRUE;
}

void on_voladj_activate(LiVESMenuItem * menuitem, livespointer user_data) {create_new_pb_speed(3);}

void on_fade_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // type == 0 fade in
  // type == 1 fade out

  double startt, endt, startv, endv, time = 0.;
  char *msg, *msg2, *utxt, *com;

  uint32_t chk_mask = 0;

  lives_alarm_t alarm_handle;
  int type;

  aud_dialog_t *aud_d = NULL;

  if (!CURRENT_CLIP_IS_VALID) return;

  if (menuitem != NULL) {
    cfile->undo1_int = type = LIVES_POINTER_TO_INT(user_data);
    aud_d = create_audfade_dialog(type);
    if (lives_dialog_run(LIVES_DIALOG(aud_d->dialog)) == LIVES_RESPONSE_CANCEL) {
      lives_free(aud_d);
      return;
    }

    time = lives_spin_button_get_value(LIVES_SPIN_BUTTON(aud_d->time_spin));

    lives_widget_destroy(aud_d->dialog);
  } else {
    type = cfile->undo1_int;
  }

  if (menuitem == NULL || !aud_d->is_sel) {
    if (menuitem == NULL) {
      endt = cfile->undo1_dbl;
      startt = cfile->undo2_dbl;
    } else {
      if (type == 0) {
        cfile->undo2_dbl = startt = 0.;
        cfile->undo1_dbl = endt = time;
      } else {
        cfile->undo1_dbl = endt = cfile->laudio_time;
        cfile->undo2_dbl = startt = cfile->laudio_time - time;
      }
    }
  } else {
    cfile->undo2_dbl = startt = ((double)cfile->start - 1.) / cfile->fps;
    cfile->undo1_dbl = endt = (double)cfile->end / cfile->fps;
  }

  if (type == 0) {
    startv = 0.;
    endv = 1.;
    msg2 = (_("Fading audio in"));
    utxt = (_("Fade audio in"));
  } else {
    startv = 1.;
    endv = 0.;
    msg2 = (_("Fading audio out"));
    utxt = (_("Fade audio out"));
  }

  if (menuitem != NULL) {
    chk_mask = WARN_MASK_LAYOUT_ALTER_AUDIO;
    if (!check_for_layout_errors(NULL, mainw->current_file, 1, 0, &chk_mask)) {
      return;
    }

    if (!aud_d->is_sel)
      msg = lives_strdup_printf(_("%s over %.1f seconds..."), msg2, time);
    else
      msg = lives_strdup_printf(_("%s from time %.2f seconds to %.2f seconds..."), msg2, startt, endt);
    d_print(msg);
    lives_free(msg);
    lives_free(msg2);
  }

  desensitize();
  do_threaded_dialog(_("Fading audio..."), FALSE);
  alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);

  threaded_dialog_spin(0.);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  if (!prefs->conserve_space) {
    com = lives_strdup_printf("%s backup_audio \"%s\"", prefs->backend_sync, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      lives_alarm_clear(alarm_handle);
      end_threaded_dialog();
      d_print_failed();
      sensitize();
      unbuffer_lmap_errors(FALSE);
      return;
    }
  }

  aud_fade(mainw->current_file, startt, endt, startv, endv);
  audio_free_fnames();

  while (lives_alarm_check(alarm_handle) > 0) {
    lives_usleep(prefs->sleep_time);
  }

  lives_alarm_clear(alarm_handle);

  end_threaded_dialog();
  d_print_done();

  cfile->changed = TRUE;
  reget_afilesize(mainw->current_file);

  if (!prefs->conserve_space) {
    set_undoable(utxt, TRUE);
    cfile->undo_action = UNDO_FADE_AUDIO;
  }
  lives_free(utxt);
  sensitize();

  lives_freep((void **)&aud_d);
  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
}


boolean on_del_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  double start, end;
  char *com, *tmp, *msg = NULL;

  uint32_t chk_mask = 0;

  boolean bad_header = FALSE;

  int i;

  if (!CURRENT_CLIP_IS_VALID) return FALSE;

  if (menuitem == NULL) {
    // undo/redo
    start = cfile->undo1_dbl;
    end = cfile->undo2_dbl;
  } else {
    if (LIVES_POINTER_TO_INT(user_data)) {
      tmp = (_("Deleting all audio"));
      chk_mask = WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_ALTER_AUDIO;
      if (!check_for_layout_errors(tmp, mainw->current_file, 1, 0, &chk_mask)) {
        lives_free(tmp);
        return FALSE;
      }
      lives_free(tmp);

      if (!CURRENT_CLIP_HAS_VIDEO) {
        if (do_warning_dialog(_("\nDeleting all audio will close this file.\nAre you sure ?"))) close_current_file(0);
        unbuffer_lmap_errors(FALSE);
        return FALSE;
      }
      msg = (_("Deleting all audio..."));
      start = end = 0.;
    } else {
      start = calc_time_from_frame(mainw->current_file, cfile->start);
      end = calc_time_from_frame(mainw->current_file, cfile->end + 1);
      msg = lives_strdup_printf(_("Deleting audio from %.2f to %.2f seconds..."), start, end);
      start *= (double)cfile->arate / (double)cfile->arps;
      end *= (double)cfile->arate / (double)cfile->arps;

      tmp = (_("Deleting audio"));
      chk_mask = WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_ALTER_AUDIO;
      if (!check_for_layout_errors(tmp, mainw->current_file, 1, 0, &chk_mask)) {
        lives_free(tmp);
        return FALSE;
      }
    }

    cfile->undo1_dbl = start;
    cfile->undo2_dbl = end;
  }

  cfile->undo_arate = cfile->arate;
  cfile->undo_signed_endian = cfile->signed_endian;
  cfile->undo_achans = cfile->achans;
  cfile->undo_asampsize = cfile->asampsize;
  cfile->undo_arps = cfile->arps;

  if (msg != NULL) {
    d_print("");
    d_print(msg);
    lives_free(msg);
  }

  com = lives_strdup_printf("%s delete_audio \"%s\" %.8f %.8f %d %d %d", prefs->backend,
                            cfile->handle, start, end, cfile->arps,
                            cfile->achans, cfile->asampsize);
  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    if (menuitem != NULL) d_print_failed();
    unbuffer_lmap_errors(FALSE);
    return FALSE;
  }

  do_progress_dialog(TRUE, FALSE, _("Deleting Audio"));

  if (mainw->error) {
    if (menuitem != NULL) d_print_failed();
    unbuffer_lmap_errors(FALSE);
    return FALSE;
  }

  set_undoable(_("Delete Audio"), TRUE);
  cfile->undo_action = UNDO_DELETE_AUDIO;

  reget_afilesize(mainw->current_file);
  cfile->changed = TRUE;
  sensitize();

  if (cfile->laudio_time == 0. || cfile->raudio_time == 0.) {
    if (cfile->audio_waveform != NULL) {
      for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
      lives_freep((void **)&cfile->audio_waveform);
      lives_freep((void **)&cfile->aw_sizes);
    }
    if (cfile->laudio_time == cfile->raudio_time) cfile->achans = 0;
    else cfile->achans = 1;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (menuitem != NULL) {
    d_print_done();
  }

  if (mainw->sl_undo_mem != NULL && cfile->stored_layout_audio != 0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

  return TRUE;
}


void on_rb_audrec_time_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  _resaudw *resaudw = (_resaudw *)user_data;
  if (resaudw == NULL) return;
  if (lives_toggle_button_get_active(togglebutton)) {
    lives_widget_set_sensitive(resaudw->hour_spinbutton, TRUE);
    lives_widget_set_sensitive(resaudw->minute_spinbutton, TRUE);
    lives_widget_set_sensitive(resaudw->second_spinbutton, TRUE);
  } else {
    lives_widget_set_sensitive(resaudw->hour_spinbutton, FALSE);
    lives_widget_set_sensitive(resaudw->minute_spinbutton, FALSE);
    lives_widget_set_sensitive(resaudw->second_spinbutton, FALSE);
  }
}


void on_recaudclip_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  if (!is_realtime_aplayer(prefs->audio_player)) {
    do_nojack_rec_error();
    return;
  }

  mainw->fx1_val = DEFAULT_AUDIO_RATE;
  mainw->fx2_val = DEFAULT_AUDIO_CHANS;
  mainw->fx3_val = DEFAULT_AUDIO_SAMPS;
  mainw->fx4_val = mainw->endian;
  mainw->rec_end_time = -1.;
  resaudw = create_resaudw(5, NULL, NULL);
  lives_widget_show(resaudw->dialog);
}

static uint32_t lmap_error_recsel;

void on_recaudsel_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  uint32_t chk_mask = WARN_MASK_LAYOUT_ALTER_AUDIO;

  lmap_error_recsel = 0;

  if (!CURRENT_CLIP_IS_VALID) return;
  if (!is_realtime_aplayer(prefs->audio_player)) {
    do_nojack_rec_error();
    return;
  }

  if (!check_for_layout_errors(NULL, mainw->current_file, 1, 0, &chk_mask)) {
    return;
  }

  lmap_error_recsel = chk_mask;

  mainw->rec_end_time = (cfile->end - cfile->start + 1.) / cfile->fps;

  if (cfile->achans > 0) {
    mainw->fx1_val = cfile->arate;
    mainw->fx2_val = cfile->achans;
    mainw->fx3_val = cfile->asampsize;
    mainw->fx4_val = cfile->signed_endian;
    resaudw = create_resaudw(7, NULL, NULL);
  } else {
    mainw->fx1_val = DEFAULT_AUDIO_RATE;
    mainw->fx2_val = DEFAULT_AUDIO_CHANS;
    mainw->fx3_val = DEFAULT_AUDIO_SAMPS;
    mainw->fx4_val = mainw->endian;
    resaudw = create_resaudw(6, NULL, NULL);
  }
  lives_widget_show_all(resaudw->dialog);
}


void on_recaudclip_ok_clicked(LiVESButton * button, livespointer user_data) {
#ifdef RT_AUDIO
  ticks_t ins_pt;
  double aud_start, aud_end, vel = 1., vol = 1.;

  uint32_t chk_mask;

  boolean backr = FALSE;

  int asigned = 1, aendian = 1;
  int old_file = mainw->current_file, new_file;
  int type = LIVES_POINTER_TO_INT(user_data);
  int oachans = 0, oarate = 0, oarps = 0, ose = 0, oasamps = 0;

  char *com;

  // type == 0 - new clip
  // type == 1 - existing clip

  if (type == 1) d_print(""); // show switch message, if appropriate

  mainw->current_file = mainw->first_free_file;
  if (!get_new_handle(mainw->current_file, NULL)) {
    mainw->current_file = old_file;
    return;
  }

  cfile->is_loaded = TRUE;
  cfile->img_type = IMG_TYPE_BEST; // override the pref

  cfile->arps = cfile->arate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  cfile->achans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  cfile->asampsize = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

  mainw->rec_samples = -1;
  mainw->rec_end_time = -1.;

  if (type == 0) {
    if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton))) {
      mainw->rec_end_time = (lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->hour_spinbutton)) * 60.
                             + lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->minute_spinbutton))) * 60.
                            + lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->second_spinbutton));
      mainw->rec_samples = mainw->rec_end_time * cfile->arate;
    }
  }

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
    asigned = 0;
  }
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
    aendian = 0;
  }

  mainw->is_processing = TRUE;

  cfile->signed_endian = get_signed_endian(asigned, aendian);
  lives_widget_destroy(resaudw->dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  lives_freep((void **)&resaudw);

  if (cfile->arate <= 0) {
    do_audrate_error_dialog();
    mainw->is_processing = FALSE;
    close_temp_handle(old_file);
    return;
  }

  if (mainw->rec_end_time == 0.) {
    widget_opts.non_modal = TRUE;
    do_error_dialog(_("\nRecord time must be greater than 0.\n"));
    widget_opts.non_modal = FALSE;
    mainw->is_processing = FALSE;
    close_temp_handle(old_file);
    return;
  }

  asigned = !asigned;

  if (type == 0) {
    lives_snprintf(cfile->type, 40, "Audio");
    add_to_clipmenu();

    lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  }

  mainw->effects_paused = FALSE;

  if (type == 1) {
    oachans = mainw->files[old_file]->achans;
    oarate = mainw->files[old_file]->arate;
    oarps = mainw->files[old_file]->arps;
    oasamps = mainw->files[old_file]->asampsize;
    ose = mainw->files[old_file]->signed_endian;

    mainw->files[old_file]->arate = mainw->files[old_file]->arps = cfile->arate;
    mainw->files[old_file]->asampsize = cfile->asampsize;
    mainw->files[old_file]->achans = cfile->achans;
    mainw->files[old_file]->signed_endian = cfile->signed_endian;
  }

  mainw->suppress_dprint = TRUE;
  mainw->no_switch_dprint = TRUE;

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    jack_rec_audio_to_clip(mainw->current_file, old_file, type == 0 ? RECA_NEW_CLIP : RECA_EXISTING);
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    pulse_rec_audio_to_clip(mainw->current_file, old_file, type == 0 ? RECA_NEW_CLIP : RECA_EXISTING);
  }
#endif

  if (type == 1) {
    // set these again, as playsel may have reset them
    mainw->files[old_file]->arate = mainw->files[old_file]->arps = cfile->arate;
    mainw->files[old_file]->asampsize = cfile->asampsize;
    mainw->files[old_file]->achans = cfile->achans;
    mainw->files[old_file]->signed_endian = cfile->signed_endian;
  }

  if (type != 1 && mainw->cancelled == CANCEL_USER) {
    mainw->cancelled = CANCEL_NONE;
    if (type == 1) {
      mainw->files[old_file]->arps = oarps;
      mainw->files[old_file]->arate = oarate;
      mainw->files[old_file]->achans = oachans;
      mainw->files[old_file]->asampsize = oasamps;
      mainw->files[old_file]->signed_endian = ose;
    }
    mainw->is_processing = FALSE;
    close_temp_handle(old_file);
    mainw->suppress_dprint = FALSE;
    d_print_cancelled();
    mainw->no_switch_dprint = FALSE;
    return;
  }

  mainw->cancelled = CANCEL_NONE;
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  if (type == 1) {
    // set these again in case reget_afilesize() reset them
    cfile->arate = cfile->arps = mainw->files[old_file]->arate;
    cfile->asampsize = mainw->files[old_file]->asampsize;
    cfile->achans = mainw->files[old_file]->achans;
    cfile->signed_endian = mainw->files[old_file]->signed_endian;

    do_threaded_dialog(_("Committing audio"), FALSE);
    aud_start = 0.;
    reget_afilesize(mainw->current_file);
    aud_end = cfile->laudio_time;

    if (aud_end == 0.) {
      end_threaded_dialog();
      close_temp_handle(old_file);
      mainw->suppress_dprint = FALSE;
      d_print("nothing recorded...");
      d_print_failed();
      return;
    }

    ins_pt = (mainw->files[old_file]->start - 1.) / mainw->files[old_file]->fps * TICKS_PER_SECOND_DBL;

    if (!prefs->conserve_space && oachans > 0) {
      com = lives_strdup_printf("%s backup_audio \"%s\"", prefs->backend_sync, mainw->files[old_file]->handle);
      lives_system(com, FALSE);
      lives_free(com);

      if (THREADVAR(com_failed)) {
        end_threaded_dialog();
        close_temp_handle(old_file);
        mainw->suppress_dprint = FALSE;
        d_print_failed();
        return;
      }
    }

    THREADVAR(read_failed) = THREADVAR(write_failed) = FALSE;
    lives_freep((void **)&THREADVAR(read_failed_file));

    // insert audio from old (new) clip to current
    render_audio_segment(1, &(mainw->current_file), old_file, &vel, &aud_start, ins_pt,
                         ins_pt + (ticks_t)((aud_end - aud_start)*TICKS_PER_SECOND_DBL), &vol, vol, vol, NULL);

    end_threaded_dialog();
    close_current_file(old_file);

    if (THREADVAR(write_failed)) {
      // on failure
      int outfile = (mainw->multitrack != NULL ? mainw->multitrack->render_file : mainw->current_file);
      char *outfilename = lives_get_audio_file_name(outfile);
      do_write_failed_error_s(outfilename, NULL);
      lives_free(outfilename);

      if (!prefs->conserve_space && type == 1) {
        // try to recover backup
        com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, mainw->files[old_file]->handle);
        lives_system(com, FALSE);
        lives_free(com);
        backr = TRUE;
      }
    }

    if (THREADVAR(read_failed)) {
      do_read_failed_error_s(THREADVAR(read_failed_file), NULL);
      lives_freep((void **)&THREADVAR(read_failed_file));
      if (!prefs->conserve_space && type == 1 && !backr) {
        // try to recover backup
        com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, mainw->files[old_file]->handle);
        lives_system(com, FALSE);
        lives_free(com);
      }
    }
  }

  mainw->suppress_dprint = FALSE;
  cfile->changed = TRUE;
  save_clip_values(mainw->current_file);

  mainw->cancelled = CANCEL_NONE;

  new_file = mainw->current_file;
  if (type == 0) {
    if (mainw->multitrack == NULL) {
      switch_to_file((mainw->current_file = 0), new_file);
    }
  } else {
    if (!prefs->conserve_space) {
      set_undoable(_("Record new audio"), TRUE);
      cfile->undo_action = UNDO_REC_AUDIO;
    }
  }

  d_print_done();
  mainw->no_switch_dprint = FALSE;

  chk_mask = lmap_error_recsel;
  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));
  lmap_error_recsel = 0;
  mainw->is_processing = FALSE;

#endif
}


boolean on_ins_silence_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  double start = 0, end = 0;
  char *com;

  uint32_t chk_mask = 0;

  boolean has_new_audio = FALSE;

  int i;

  if (!CURRENT_CLIP_IS_VALID) return FALSE;

  if (!CURRENT_CLIP_HAS_AUDIO) {
    has_new_audio = TRUE;
  }

  if (menuitem == NULL) {
    // redo
    if (cfile->achans != cfile->undo_achans) {
      if (cfile->audio_waveform != NULL) {
        for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
        lives_freep((void **)&cfile->audio_waveform);
        lives_freep((void **)&cfile->aw_sizes);
      }
    }
    start = cfile->undo1_dbl;
    end = cfile->undo2_dbl;
    cfile->arate = cfile->undo_arate;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
    cfile->arps = cfile->undo_arps;
  }

  if (!cfile->achans) {
    mainw->fx1_val = DEFAULT_AUDIO_RATE;
    mainw->fx2_val = DEFAULT_AUDIO_CHANS;
    mainw->fx3_val = DEFAULT_AUDIO_SAMPS;
    mainw->fx4_val = mainw->endian;
    resaudw = create_resaudw(2, NULL, NULL);
    if (lives_dialog_run(LIVES_DIALOG(resaudw->dialog)) != LIVES_RESPONSE_OK) return FALSE;
    if (mainw->error) {
      mainw->error = FALSE;
      return FALSE;
    }

    cfile->undo_arate = cfile->arate;
    cfile->undo_signed_endian = cfile->signed_endian;
    cfile->undo_achans = cfile->achans;
    cfile->undo_asampsize = cfile->asampsize;
  }

  if (menuitem != NULL) {
    char *tmp = (_("Inserting silence"));
    chk_mask = WARN_MASK_LAYOUT_SHIFT_AUDIO |  WARN_MASK_LAYOUT_ALTER_AUDIO;

    start = calc_time_from_frame(mainw->current_file, cfile->start);
    end = calc_time_from_frame(mainw->current_file, cfile->end + 1);

    if (!check_for_layout_errors(tmp, mainw->current_file, cfile->start, cfile->end, &chk_mask)) {
      lives_free(tmp);
      return FALSE;
    }
    lives_free(tmp);

    d_print(""); // force switchtext
    d_print(_("Inserting silence from %.2f to %.2f seconds..."), start, end);
  }

  cfile->undo1_dbl = start;
  start *= (double)cfile->arate / (double)cfile->arps;
  cfile->undo2_dbl = end;
  end *= (double)cfile->arate / (double)cfile->arps;

  // store values for undo
  cfile->old_laudio_time = cfile->laudio_time;
  cfile->old_raudio_time = cfile->raudio_time;

  // with_sound is 2 (audio only), therefore start, end, where, are in seconds. rate is -ve to indicate silence
  com = lives_strdup_printf("%s insert \"%s\" \"%s\" %.8f 0. %.8f \"%s\" 2 0 0 0 0 %d %d %d %d %d 1",
                            prefs->backend, cfile->handle,
                            get_image_ext_for_type(cfile->img_type), start, end - start, cfile->handle, -cfile->arps,
                            cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                            !(cfile->signed_endian & AFORM_BIG_ENDIAN));

  lives_rm(cfile->info_file);
  lives_system(com, FALSE);
  lives_free(com);

  if (THREADVAR(com_failed)) {
    d_print_failed();
    if (has_new_audio) cfile->achans = cfile->arate = cfile->asampsize = cfile->arps = 0;
    unbuffer_lmap_errors(FALSE);
    return FALSE;
  }

  do_progress_dialog(TRUE, FALSE, _("Inserting Silence"));

  if (mainw->error) {
    d_print_failed();
    if (has_new_audio) cfile->achans = cfile->arate = cfile->asampsize = cfile->arps = 0;
    unbuffer_lmap_errors(FALSE);
    return FALSE;
  }

  if (has_new_audio) {
    cfile->arate = cfile->arps = cfile->undo_arate;
    cfile->signed_endian = cfile->undo_signed_endian;
    cfile->achans = cfile->undo_achans;
    cfile->asampsize = cfile->undo_asampsize;
  }

  set_undoable(_("Insert Silence"), TRUE);
  cfile->undo_action = UNDO_INSERT_SILENCE;

  reget_afilesize(mainw->current_file);
  cfile->changed = TRUE;

  save_clip_values(mainw->current_file);

  if (menuitem != NULL) {
    sensitize();
    d_print_done();
  }

  if (mainw->sl_undo_mem != NULL && cfile->stored_layout_audio != 0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

  return TRUE;
}


void on_ins_silence_details_clicked(LiVESButton * button, livespointer user_data) {
  int asigned = 1, aendian = 1;
  boolean bad_header = FALSE;

  cfile->arps = cfile->arate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  cfile->achans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  cfile->asampsize = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
    asigned = 0;
  }
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
    aendian = 0;
  }
  cfile->signed_endian = get_signed_endian(asigned, aendian);
  lives_widget_destroy(resaudw->dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  lives_freep((void **)&resaudw);
  if (cfile->arate <= 0) {
    do_audrate_error_dialog();
    cfile->achans = cfile->arate = cfile->arps = cfile->asampsize = 0;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ARATE, &cfile->arps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ACHANS, &cfile->achans)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_ASAMPS, &cfile->asampsize)) bad_header = TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
    mainw->error = TRUE;
    return;
  }
  mainw->error = FALSE;
}


void on_lerrors_clear_clicked(LiVESButton * button, livespointer user_data) {
  boolean close = LIVES_POINTER_TO_INT(user_data);
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  clear_lmap_errors();
  save_layout_map(NULL, NULL, NULL, NULL);
  if (close) {
    boolean needs_idlefunc = mainw->mt_needs_idlefunc;
    mainw->mt_needs_idlefunc = FALSE;
    lives_general_button_clicked(button, textwindow);
    mainw->mt_needs_idlefunc = needs_idlefunc;
  } else {
    lives_widget_queue_draw(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    lives_widget_set_sensitive(textwindow->clear_button, FALSE);
    lives_widget_set_sensitive(textwindow->delete_button, FALSE);

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
  }
}


void on_lerrors_delete_clicked(LiVESButton * button, livespointer user_data) {
  int num_maps = lives_list_length(mainw->affected_layouts_map);
  char *msg = lives_strdup_printf(P_("\nDelete %d layout...are you sure ?\n", "\nDelete %d layouts...are you sure ?\n", num_maps),
                                  num_maps);
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mainw->mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!do_warning_dialog(msg)) {
    lives_free(msg);
    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      maybe_add_mt_idlefunc();
    }
    return;
  }

  lives_free(msg);
  remove_layout_files(mainw->affected_layouts_map);
  on_lerrors_clear_clicked(button, LIVES_INT_TO_POINTER(TRUE));
}
