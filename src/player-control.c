// player-controller.c
// LiVES
// (c) G. Finch 2003 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

/*
  This file contains API functions for running and cleannig up beofre and after the
  actual playback  (accessed via calls to do_progress_dualog()
  Here we handle playback of various types - clip editor, multitrack,
  as well as showing previews ion the loader window.
  This code is pretty ancient and crufty - there is still support for non-realtime audio players
  (sox) which will be phased out in favour of the nullaudio player.
  The same functon will also play processing previews.
  All of this has to be run in a background thread so as not to block the gtk+ main loop, which adds complications
  some post rocessing has to be done via idlefuncs. */

#include "main.h"
#include "startup.h"
#include "functions.h"
#include "callbacks.h"
#include "effects-weed.h"
#include "resample.h"
#include "clip_load_save.h"

static boolean _start_playback(livespointer data) {
  int new_file, old_file;
  int play_type = LIVES_POINTER_TO_INT(data);
  if (play_type != 8 && mainw->noswitch) return TRUE;
  player_desensitize();

  switch (play_type) {
  case 8: case 6: case 0:
    /// normal play
    play_all(play_type == 8);
    if (play_type == 6) {
      /// triggered by generator
      // need to set this after playback ends; this stops the key from being activated (again) in effects.c
      // also stops the (now defunct instance being unreffed)
      mainw->gen_started_play = TRUE;
    }
    break;
  case 1:
    /// play selection
    if (!mainw->multitrack) play_sel();
    else multitrack_play_sel(NULL, mainw->multitrack);
    break;
  case 2:
    /// play stream
    mainw->play_start = 1;
    mainw->play_end = INT_MAX;
    play_file();
    break;
  case 3:
    /// osc playall
    mainw->osc_auto = 1; ///< request notifiction of success
    play_all(FALSE);
    mainw->osc_auto = 0;
    break;
  case 4:
    /// osc playsel
    mainw->osc_auto = 1; ///< request notifiction of success
    if (!mainw->multitrack) play_sel();
    else multitrack_play_sel(NULL, mainw->multitrack);
    mainw->osc_auto = 0;
    break;
  case 5:
    /// clipboard
    play_file();
    mainw->loop = mainw->oloop;
    mainw->loop_cont = mainw->oloop_cont;

    if (mainw->pre_play_file > 0) {
      switch_to_file(0, mainw->pre_play_file);
    } else {
      mainw->current_file = -1;
      close_current_file(0);
    }
    if (mainw->cancelled == CANCEL_AUDIO_ERROR) {
      handle_audio_timeout();
      mainw->cancelled = CANCEL_ERROR;
    }
    break;
  case 7:
    /// yuv4mpeg
    new_file = mainw->current_file;
    old_file = mainw->pre_play_file;
    play_file();
    if (mainw->current_file != old_file && mainw->current_file != new_file)
      old_file = mainw->current_file; // we could have rendered to a new file
    mainw->current_file = new_file;
    // close this temporary clip
    close_current_file(old_file);
    mainw->pre_play_file = -1;
    break;
  default:
    /// do nothing
    player_sensitize();
    break;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean start_playback(int type) {
  if (!is_fg_thread()) {
    mainw->player_proc = THREADVAR(proc_thread);
    break_me("PPROC1");
    return _start_playback(LIVES_INT_TO_POINTER(type));
  }
  start_playback_async(type);
  return FALSE;
}


void start_playback_async(int type) {
  lives_sigdata_t *sigdata;
  mainw->player_proc = lives_proc_thread_create(LIVES_THRDATTR_WAIT_SYNC,
						_start_playback, WEED_SEED_BOOLEAN,
						"v", LIVES_INT_TO_POINTER(type));
  lives_proc_thread_nullify_on_destruction(mainw->player_proc, (void **)&(mainw->player_proc));

  // ask the main thread to concentrate on service GUI requests from a specific proc_thread
  sigdata = lives_sigdata_new(mainw->player_proc, FALSE);
  governor_loop(sigdata);
}


static char *prep_audio_player(frames_t audio_end, int arate, int asigned, int aendian) {
  char *stfile = NULL;
  char *stopcom = NULL, *com;
  short audio_player = prefs->audio_player;
  int loop = 0;

  if (!mainw->preview && cfile->achans > 0) {
    cfile->aseek_pos = (off64_t)(cfile->real_pointer_time * (double)cfile->arate) * cfile->achans * (cfile->asampsize / 8);
    if (mainw->playing_sel) {
      off64_t apos = (off64_t)((double)(mainw->play_start - 1.) / cfile->fps * (double)cfile->arate) * cfile->achans *
                     (cfile->asampsize / 8);
      if (apos > cfile->aseek_pos) cfile->aseek_pos = apos;
    }
    if (cfile->aseek_pos > cfile->afilesize) cfile->aseek_pos = 0.;
    if (mainw->current_file == 0 && cfile->arate < 0) cfile->aseek_pos = cfile->afilesize;
  }

  if (CURRENT_CLIP_HAS_AUDIO) {
    // start up our audio player (jack or pulse)
    if (audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (mainw->jackd) jack_aud_pb_ready(mainw->jackd, mainw->current_file);
      return NULL;
#endif
    } else if (audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed) pulse_aud_pb_ready(mainw->pulsed, mainw->current_file);
      return NULL;
#endif
    }
  }

  // deprecated

  else if (audio_player != AUD_PLAYER_NONE) {
    // sox or mplayer audio - run as background process
    char *clipdir;
    char *com3 = NULL;

    if (mainw->loop_cont) {
      // tell audio to loop forever
      loop = -1;
    }

    clipdir = get_clip_dir(mainw->current_file);
    stfile = lives_build_filename(clipdir, ".stoploop", NULL);
    lives_free(clipdir);
    lives_rm(stfile);

    if (cfile->achans > 0 || (!cfile->is_loaded && !mainw->is_generating)) {
      if (loop) {
        com3 = lives_strdup_printf("%s \"%s\" 2>\"%s\" 1>&2", capable->touch_cmd,
                                   stfile, prefs->cmd_log);
      }

      if (cfile->achans > 0) {
        char *com2 = lives_strdup_printf("%s stop_audio %s", prefs->backend_sync, cfile->handle);
        if (com3) stopcom = lives_strconcat(com3, com2, NULL);
      } else stopcom = com3;
    }

    lives_freep((void **)&stfile);

    clipdir = get_clip_dir(mainw->current_file);
    stfile = lives_build_filename(clipdir, LIVES_STATUS_FILE_NAME".play", NULL);
    lives_free(clipdir);

    lives_snprintf(cfile->info_file, PATH_MAX, "%s", stfile);
    lives_free(stfile);
    if (cfile->clip_type == CLIP_TYPE_DISK) lives_rm(cfile->info_file);

    // PLAY

    if (cfile->clip_type == CLIP_TYPE_DISK && cfile->opening) {
      com = lives_strdup_printf("%s play_opening_preview \"%s\" %.3f %d %d %d %d %d %d %d %d",
                                prefs->backend,
                                cfile->handle, cfile->fps, mainw->audio_start, audio_end, 0,
                                arate, cfile->achans, cfile->asampsize, asigned, aendian);
    } else {
      // this is only used now for the sox (fallback) audio player  sox
      com = lives_strdup_printf("%s play %s %.3f %d %d %d %d %d %d %d %d",
                                prefs->backend, cfile->handle,
                                cfile->fps, mainw->audio_start, audio_end, loop,
                                arate, cfile->achans, cfile->asampsize, asigned, aendian);
    }
    if (!mainw->multitrack && com) lives_system(com, FALSE);
  }
  return stopcom;
}


static double fps_med = 0.;

static void post_playback(void) {
  if (prefs->show_player_stats && mainw->fps_measure > 0) {
    d_print(_("Average FPS was %.4f (%d frames in clock time of %f)\n"), fps_med, mainw->fps_measure,
            (double)lives_get_relative_ticks(mainw->origsecs, mainw->orignsecs)
            / TICKS_PER_SECOND_DBL);
  }

  if (mainw->new_vpp) {
    mainw->noswitch = FALSE;
    mainw->vpp = open_vid_playback_plugin(mainw->new_vpp, TRUE);
    mainw->new_vpp = NULL;
    mainw->noswitch = TRUE;
  }

  if (!mainw->multitrack && CURRENT_CLIP_HAS_VIDEO) {
    lives_widget_set_sensitive(mainw->spinbutton_start, TRUE);
    lives_widget_set_sensitive(mainw->spinbutton_end, TRUE);
  }

  if (!mainw->preview && CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_GENERATOR) {
    mainw->osc_block = TRUE;
    weed_generator_end((weed_plant_t *)cfile->ext_src);
    mainw->osc_block = FALSE;
  }

  if (prefs->open_maximised) {
    int bx, by;
    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    if (prefs->show_gui && (lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET)
                            > GUI_SCREEN_HEIGHT - abs(by) ||
                            lives_widget_get_allocation_width(LIVES_MAIN_WINDOW_WIDGET)
                            > GUI_SCREEN_WIDTH - abs(bx))) {
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    }
  }

  if (!mainw->preview && (mainw->current_file == -1 || (CURRENT_CLIP_IS_VALID && !cfile->opening))) {
    sensitize();
  }

  if (CURRENT_CLIP_IS_VALID && cfile->opening) {
    lives_widget_set_sensitive(mainw->mute_audio, cfile->achans > 0);
    lives_widget_set_sensitive(mainw->loop_continue, TRUE);
    lives_widget_set_sensitive(mainw->loop_video, cfile->achans > 0 && cfile->frames > 0);
  }

  if (mainw->cancelled != CANCEL_USER_PAUSED) {
    lives_widget_set_sensitive(mainw->stop, FALSE);
    lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);
  }

  lives_widget_set_sensitive(mainw->spinbutton_pb_fps, FALSE);

  if (!mainw->multitrack) {
    /// update screen for internal players
    if (prefs->hfbwnp) {
      lives_widget_hide(mainw->framebar);
    }
    set_drawing_area_from_pixbuf(mainw->play_image, NULL, mainw->play_surface);
    lives_widget_set_opacity(mainw->play_image, 0.);
  }

  if (!mainw->multitrack) mainw->osc_block = FALSE;

  reset_clipmenu();

  lives_menu_item_set_accel_path(LIVES_MENU_ITEM(mainw->quit), LIVES_ACCEL_PATH_QUIT);

  if (!mainw->multitrack && CURRENT_CLIP_IS_VALID)
    set_main_title(cfile->name, 0);

  if (!mainw->multitrack && !mainw->foreign && CURRENT_CLIP_IS_VALID && (!cfile->opening ||
      cfile->clip_type == CLIP_TYPE_FILE)) {
    showclipimgs();
  }

  player_sensitize();
}


/* This is only used in the case that the audio player is sox,
   which is only there as a fallback option is pulse or jack cannot be installed fro whatever reason
   This is gradually being phased out in favour of the nullaudio playaer.
*/
static void wait_for_audio_stop(const char *stop_command) {
  FILE *infofile;

# define SECOND_STOP_TIME 0.1
# define STOP_GIVE_UP_TIME 1.0

  double time_waited = 0.;
  boolean sent_second_stop = FALSE;

  // send another stop if necessary
  while (!(infofile = fopen(cfile->info_file, "r"))) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    lives_usleep(prefs->sleep_time);
    time_waited += 1000000. / prefs->sleep_time;
    if (time_waited > SECOND_STOP_TIME && !sent_second_stop) {
      lives_system(stop_command, TRUE);
      sent_second_stop = TRUE;
    }

    if (time_waited > STOP_GIVE_UP_TIME) {
      // give up waiting, but send a last try...
      lives_system(stop_command, TRUE);
      break;
    }
  }
  if (infofile) fclose(infofile);
}


/// play the current clip from 'mainw->play_start' to 'mainw->play_end'
void play_file(void) {
  LiVESWidgetClosure *freeze_closure, *bg_freeze_closure;
  LiVESList *cliplist;
  weed_plant_t *pb_start_event = NULL;

  char *stopcom = NULL;
  char *stfile;

  double pointer_time = cfile->pointer_time;
  double real_pointer_time = cfile->real_pointer_time;

  short audio_player = prefs->audio_player;

  boolean mute;
  boolean needsadone = FALSE;

  boolean lazy_start = FALSE;

#ifdef RT_AUDIO
  boolean exact_preview = FALSE;
#endif
  boolean has_audio_buffers = FALSE;

  int arate;

  int asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
  int aendian = !(cfile->signed_endian & AFORM_BIG_ENDIAN);
  int current_file = mainw->current_file;
  int audio_end = 0;

  if (!mainw->preview || !cfile->opening) {
    enable_record();
    desensitize();
#ifdef ENABLE_JACK
    if (!(mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
          && (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE)))
      lives_widget_set_sensitive(mainw->spinbutton_pb_fps, TRUE);
#endif
  }

  /// from now on we can only switch at the designated SWITCH POINT
  mainw->noswitch = TRUE;
  mainw->cancelled = CANCEL_NONE;

  asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
  aendian = !(cfile->signed_endian & AFORM_BIG_ENDIAN);
  current_file = mainw->current_file;
  if (mainw->pre_play_file == -1) mainw->pre_play_file = current_file;

  if (!is_realtime_aplayer(audio_player)) mainw->aud_file_to_kill = mainw->current_file;
  else mainw->aud_file_to_kill = -1;

  mainw->ext_playback = FALSE;

  mainw->rec_aclip = -1;

  init_conversions(OBJ_INTENTION_PLAY);

  lives_hooks_trigger(THREADVAR(hook_stacks), PREPARING_HOOK);

  if (mainw->pre_src_file == -2) mainw->pre_src_file = mainw->current_file;
  mainw->pre_src_audio_file = mainw->current_file;

  /// enable the freeze button
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_BackSpace,
                            (LiVESXModifierType)LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            (freeze_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(freeze_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL)));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_BackSpace,
                            (LiVESXModifierType)(LIVES_SHIFT_MASK), (LiVESAccelFlags)0,
                            (bg_freeze_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(freeze_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND), NULL)));

  /// disable ctrl-q since it can be activated by user error
  lives_accel_path_disconnect(mainw->accel_group, LIVES_ACCEL_PATH_QUIT);

  if (mainw->multitrack) {
    mainw->event_list = mainw->multitrack->event_list;
    pb_start_event = mainw->multitrack->pb_start_event;
#ifdef RT_AUDIO
    exact_preview = mainw->multitrack->exact_preview;
#endif
  }

  mainw->clip_switched = FALSE;

  if (!mainw->foreign && mainw->lazy_starter) {
    // this is like a safe addrref
    lives_proc_thread_ref(mainw->lazy_starter);
    if (mainw->lazy_starter) {
      lives_proc_thread_request_pause(mainw->lazy_starter);
      lives_proc_thread_unref(mainw->lazy_starter);
      lazy_start = TRUE;
    }
  }

  // reinit all active effects
  if (!mainw->preview && !mainw->is_rendering && !mainw->foreign) weed_reinit_all();

  if (mainw->record) {
    if (mainw->preview) {
      mainw->record = FALSE;
      d_print(_("recording aborted by preview.\n"));
    } else if (mainw->current_file == 0) {
      mainw->record = FALSE;
      d_print(_("recording aborted by clipboard playback.\n"));
    } else {
      d_print(_("Recording performance..."));
      needsadone = TRUE;
      // TODO
      if (mainw->current_file > 0 && (cfile->undo_action == UNDO_RESAMPLE
                                      || cfile->undo_action == UNDO_RENDER)) {
        lives_widget_set_sensitive(mainw->undo, FALSE);
        lives_widget_set_sensitive(mainw->redo, FALSE);
        cfile->undoable = cfile->redoable = FALSE;
      }
    }
  }
  /// set performance at right place
  else if (mainw->event_list) cfile->next_event = get_first_event(mainw->event_list);

  if (!mainw->multitrack && CURRENT_CLIP_HAS_VIDEO) {
    lives_widget_set_frozen(mainw->spinbutton_start, TRUE, 0.);
    lives_widget_set_frozen(mainw->spinbutton_end, TRUE, 0.);
  }

  if (!mainw->multitrack) {
#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop && !mainw->event_list && !mainw->preview
        && (prefs->jack_opts & (JACK_OPTS_TIMEBASE_START | JACK_OPTS_TIMEBASE_SLAVE))) {
      // calculate the start position from jack transport
      double sttime = (double)jack_transport_get_current_ticks(mainw->jackd_trans) / TICKS_PER_SECOND_DBL;
      cfile->pointer_time = cfile->real_pointer_time = sttime;
      if (cfile->real_pointer_time > CLIP_TOTAL_TIME(mainw->current_file))
        cfile->real_pointer_time = CLIP_TOTAL_TIME(mainw->current_file);
      if (cfile->pointer_time > cfile->video_time) cfile->pointer_time = 0.;
      mainw->play_start = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
    }
#endif
  }

  /// these values are only relevant for non-realtime audio players (e.g. sox)
  mainw->audio_start = mainw->audio_end = 0;

  if (cfile->achans > 0) {
    if (mainw->event_list &&
        !(mainw->preview && mainw->is_rendering) &&
        !(mainw->multitrack && mainw->preview && mainw->multitrack->is_rendering)) {
      /// play performance data
      if (event_list_get_end_secs(mainw->event_list) > cfile->frames
          / cfile->fps && !mainw->playing_sel) {
        mainw->audio_end = (event_list_get_end_secs(mainw->event_list) * cfile->fps + 1.)
                           * cfile->arate / cfile->arps;
      }
    }

    if (mainw->audio_end == 0) {
      // values in FRAMES
      mainw->audio_start = calc_time_from_frame(mainw->current_file,
                           mainw->play_start) * cfile->fps + 1.;
      mainw->audio_end = calc_time_from_frame(mainw->current_file, mainw->play_end) * cfile->fps + 1.;
      if (!mainw->playing_sel) {
        mainw->audio_end = 0;
      }
    }
    cfile->aseek_pos = (off_t)((double)(mainw->audio_start - 1.)
                               * cfile->fps * (double)cfile->arate)
                       * cfile->achans * (cfile->asampsize >> 3);
    cfile->async_delta = 0;
  }

  if (!cfile->opening_audio && !mainw->loop) {
    /** if we are opening audio or looping we just play to the end of audio,
      otherwise...*/
    audio_end = mainw->audio_end;
  }

  if (!mainw->multitrack) {
    if (!mainw->preview) {
      lives_frame_set_label(LIVES_FRAME(mainw->playframe), _("Play"));
    } else {
      lives_frame_set_label(LIVES_FRAME(mainw->playframe), _("Preview"));
    }

    if (palette->style & STYLE_1) {
      lives_widget_set_fg_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->playframe)),
                                LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    }

    if (mainw->foreign) {
      lives_widget_show_all(mainw->top_vbox);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    }

    /// blank the background if asked to
    if ((mainw->faded || (prefs->show_playwin && !prefs->show_gui)
         || (mainw->fs && (!mainw->sep_win))) && (cfile->frames > 0 ||
             mainw->foreign)) {
      main_thread_execute_rvoid_pvoid(fade_background);
      //fade_background();
    }

    if ((!mainw->sep_win || (!mainw->faded && (prefs->sepwin_type != SEPWIN_TYPE_STICKY)))
        && (cfile->frames > 0 || mainw->preview_rendering || mainw->foreign)) {
      /// show the frame in the main window
      lives_widget_set_opacity(mainw->playframe, 1.);
      lives_widget_show_all(mainw->playframe);
    }

    /// plug the plug into the playframe socket if we need to
    add_to_playframe();
  }

  arate = cfile->arate;

  mute = mainw->mute;

  if (!is_realtime_aplayer(audio_player)) {
    if (cfile->achans == 0 || mainw->is_rendering) mainw->mute = TRUE;
    if (mainw->mute && !cfile->opening_only_audio) arate = arate ? -arate : -1;
  }

  cfile->frameno = mainw->play_start;
  cfile->pb_fps = cfile->fps;

  if (mainw->reverse_pb) {
    cfile->pb_fps = -cfile->pb_fps;
    cfile->frameno = mainw->play_end;
  }
  cfile->last_frameno = cfile->frameno;
  mainw->reverse_pb = FALSE;

  mainw->swapped_clip = -1;
  mainw->blend_palette = WEED_PALETTE_END;

  cfile->play_paused = FALSE;
  mainw->period = TICKS_PER_SECOND_DBL / cfile->pb_fps;

  if ((audio_player == AUD_PLAYER_JACK && AUD_SRC_INTERNAL)
      || (mainw->event_list && (!mainw->is_rendering || !mainw->preview || mainw->preview_rendering)))
    audio_cache_init();

  if (mainw->blend_file != -1 && !IS_VALID_CLIP(mainw->blend_file)) {
    track_decoder_free(1, mainw->blend_file);
    mainw->blend_file = -1;
  }

  lives_widget_set_sensitive(mainw->m_stopbutton, TRUE);
  mainw->playing_file = mainw->current_file;

  if (mainw->record) {
    if (mainw->event_list) event_list_free(mainw->event_list);
    mainw->event_list = NULL;
    mainw->record_starting = TRUE;
  }

  if (!mainw->multitrack && (prefs->msgs_nopbdis || (prefs->show_msg_area && mainw->double_size))) {
    lives_widget_hide(mainw->message_box);
  }

#ifdef ENABLE_JACK
  if (!(mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
        && (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE)))
#endif
    lives_widget_set_sensitive(mainw->stop, TRUE);

  if (!mainw->multitrack) lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
  else if (!cfile->opening) {
    if (!mainw->is_processing) mt_swap_play_pause(mainw->multitrack, TRUE);
    else {
      lives_widget_set_sensitive(mainw->multitrack->playall, FALSE);
      lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
    }
  }

  lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), !mainw->double_size);

  lives_widget_set_sensitive(mainw->m_playselbutton, FALSE);
  lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  lives_widget_set_sensitive(mainw->m_mutebutton, is_realtime_aplayer(audio_player) || mainw->multitrack);

  lives_widget_set_sensitive(mainw->m_loopbutton, (!cfile->achans || mainw->mute || mainw->multitrack ||
                             mainw->loop_cont || is_realtime_aplayer(audio_player))
                             && mainw->current_file > 0);
  lives_widget_set_sensitive(mainw->loop_continue, (!cfile->achans || mainw->mute || mainw->loop_cont ||
                             is_realtime_aplayer(audio_player))
                             && mainw->current_file > 0);

  if (cfile->frames == 0 && !mainw->multitrack) {
    if (mainw->preview_box && lives_widget_get_parent(mainw->preview_box)) {

      lives_container_remove(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);

      mainw->pw_scroll_func = lives_signal_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_SCROLL_EVENT,
                              LIVES_GUI_CALLBACK(on_mouse_scroll), NULL);
    }
  } else {
    if (mainw->sep_win) {
      /// create a separate window for the internal player if requested
      if (prefs->sepwin_type == SEPWIN_TYPE_NON_STICKY) {
        make_play_window();
      } else {
        if (!mainw->multitrack || mainw->fs) {
          resize_play_window();
        }

        /// needed
        if (!mainw->multitrack) {
          lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
        } else {
          /// this doesn't get called if we don't call resize_play_window()
          if (mainw->play_window) {
            if (prefs->show_playwin) {
              lives_window_present(LIVES_WINDOW(mainw->play_window));
              lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*

    if (mainw->play_window) {
      hide_cursor(lives_widget_get_xwindow(mainw->play_window));
      lives_widget_set_app_paintable(mainw->play_window, TRUE);
      play_window_set_title();
    }

    if (!mainw->foreign && !mainw->sep_win) {
      hide_cursor(lives_widget_get_xwindow(mainw->playarea));
    }

    if (!mainw->sep_win && !mainw->foreign) {
      if (mainw->double_size) resize(2.);
      //else resize(1); // add_to_playframe does this
    }

    if (mainw->fs && !mainw->sep_win && cfile->frames > 0) {
      fullscreen_internal();
    }
  }

  if (prefs->stop_screensaver) {
    lives_disable_screensaver();
  }

  if (!mainw->multitrack) {
    lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->pb_fps);
    lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);

    mainw->last_blend_file = -1;

    // show the framebar
    if (!mainw->multitrack && !mainw->faded
        && (!prefs->hide_framebar &&
            (!mainw->fs || (widget_opts.monitor + 1 != prefs->play_monitor && prefs->play_monitor != 0
                            && capable->nmonitors > 1 && mainw->sep_win)
             || (mainw->vpp && mainw->sep_win && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)))
            && ((!mainw->preview && (cfile->frames > 0 || mainw->foreign)) || cfile->opening))) {
      lives_widget_show(mainw->framebar);
    }
  }

  cfile->play_paused = FALSE;
  mainw->actual_frame = 0;

  mainw->effort = -EFFORT_RANGE_MAX;

  find_when_to_stop();

  // reinit all active effects
  if (!mainw->preview && !mainw->is_rendering && !mainw->foreign) weed_reinit_all();

  if (!mainw->foreign && (!(AUD_SRC_EXTERNAL &&
                            (audio_player == AUD_PLAYER_JACK ||
                             audio_player == AUD_PLAYER_PULSE || audio_player == AUD_PLAYER_NONE)))) {
    stopcom = prep_audio_player(audio_end, arate, asigned, aendian);
  }

  if (!mainw->foreign && prefs->midisynch && !mainw->preview) {
    char *com3 = lives_strdup(EXEC_MIDISTART);
    lives_system(com3, TRUE);
    lives_free(com3);
  }

  if (mainw->play_window && !mainw->multitrack) {
    lives_widget_hide(mainw->preview_controls);
    if (prefs->pb_hide_gui) hide_main_gui();
  }
  // if recording, refrain from writing audio until we are ready
  if (mainw->record) mainw->record_paused = TRUE;

  // if recording, set up recorder (jack or pulse)
  if (!mainw->preview && (AUD_SRC_EXTERNAL
                          || (mainw->record && (mainw->agen_key != 0
                              || (prefs->audio_opts & AUDIO_OPTS_IS_LOCKED))))
      && (audio_player == AUD_PLAYER_JACK || audio_player == AUD_PLAYER_PULSE)) {
    mainw->rec_samples = -1; // record unlimited
    if (mainw->record) {
      // create temp clip
      open_ascrap_file(-1);
      if (mainw->ascrap_file != -1) {
        mainw->rec_aclip = mainw->ascrap_file;
        mainw->rec_avel = 1.;
        mainw->rec_aseek = 0;
      }
    }
    if (audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if ((AUD_SRC_EXTERNAL || mainw->agen_key != 0 || mainw->agen_needs_reinit
           || (prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) && mainw->jackd) {
        if (mainw->agen_key != 0 || mainw->agen_needs_reinit) {
          mainw->jackd->playing_file = mainw->current_file;
          if (mainw->ascrap_file != -1)
            jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
        } else {
          if (mainw->ascrap_file != -1) {
            if (AUD_SRC_EXTERNAL) jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
            else jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_MIXED);
          }
          //mainw->jackd->in_use = TRUE;
        }
      }
      if (AUD_SRC_EXTERNAL && mainw->jackd_read) {
        mainw->jackd_read->num_input_channels = mainw->jackd_read->num_output_channels = 2;
        mainw->jackd_read->sample_in_rate = mainw->jackd_read->sample_out_rate;
        mainw->jackd_read->is_paused = TRUE;
        mainw->jackd_read->in_use = TRUE;
      }
#endif
    }
    if (audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if ((AUD_SRC_EXTERNAL || mainw->agen_key != 0  || mainw->agen_needs_reinit
           || (prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) && mainw->pulsed) {
        if (mainw->agen_key != 0 || mainw->agen_needs_reinit) {
          mainw->pulsed->playing_file = mainw->current_file;
          if (mainw->ascrap_file != -1)
            pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
        } else {
          if (mainw->ascrap_file != -1) {
            if (AUD_SRC_EXTERNAL) pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
            else pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_MIXED);
          }
        }
        //mainw->pulsed->in_use = TRUE;
      }
      if (AUD_SRC_EXTERNAL && mainw->pulsed_read) {
        mainw->pulsed_read->in_achans = mainw->pulsed_read->out_achans = PA_ACHANS;
        mainw->pulsed_read->in_asamps = mainw->pulsed_read->out_asamps = PA_SAMPSIZE;
        mainw->pulsed_read->in_arate = mainw->pulsed_read->out_arate;
        mainw->pulsed_read->is_paused = TRUE;
        mainw->pulsed_read->in_use = TRUE;
      }
#endif
    }
  }

  // set in case audio lock gets actioned
  future_prefs->audio_opts = prefs->audio_opts;

  if (mainw->foreign || weed_playback_gen_start()) {
    if (mainw->osc_auto)
      lives_notify(LIVES_OSC_NOTIFY_SUCCESS, "");
    lives_notify(LIVES_OSC_NOTIFY_PLAYBACK_STARTED, "");

#ifdef ENABLE_JACK
    if (mainw->event_list && !mainw->record && audio_player == AUD_PLAYER_JACK && mainw->jackd &&
        !(mainw->preview && mainw->is_processing &&
          !(mainw->multitrack && mainw->preview && mainw->multitrack->is_rendering))) {
      // if playing an event list, we switch to audio memory buffer mode
      if (mainw->multitrack) init_jack_audio_buffers(cfile->achans, cfile->arate, exact_preview);
      else init_jack_audio_buffers(DEFAULT_AUDIO_CHANS, DEFAULT_AUDIO_RATE, FALSE);
      has_audio_buffers = TRUE;
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (mainw->event_list && !mainw->record && audio_player == AUD_PLAYER_PULSE && mainw->pulsed &&
        !(mainw->preview && mainw->is_processing &&
          !(mainw->multitrack && mainw->preview && mainw->multitrack->is_rendering))) {
      // if playing an event list, we switch to audio memory buffer mode
      if (mainw->multitrack) init_pulse_audio_buffers(cfile->achans, cfile->arate, exact_preview);
      else init_pulse_audio_buffers(DEFAULT_AUDIO_CHANS, DEFAULT_AUDIO_RATE, FALSE);
      has_audio_buffers = TRUE;
    }
#endif

    mainw->abufs_to_fill = 0;

    lives_hooks_trigger(THREADVAR(hook_stacks), TX_START_HOOK);

    //lives_widget_context_update();
    //play until stopped or a stream finishes
    do {
      mainw->cancelled = CANCEL_NONE;
      mainw->play_sequence++;
      mainw->fps_measure = 0;

      if (mainw->event_list && !mainw->record) {
        if (!pb_start_event) pb_start_event = get_first_event(mainw->event_list);

        if (!(mainw->preview && mainw->multitrack && mainw->multitrack->is_rendering))
          init_track_decoders();

        if (has_audio_buffers) {
#ifdef ENABLE_JACK
          if (audio_player == AUD_PLAYER_JACK) {
            int i;
            mainw->write_abuf = 0;

            // fill our audio buffers now
            // this will also get our effects state

            // reset because audio sync may have set it
            if (mainw->multitrack) mainw->jackd->abufs[0]->arate = cfile->arate;
            else mainw->jackd->abufs[0]->arate = mainw->jackd->sample_out_rate;
            fill_abuffer_from(mainw->jackd->abufs[0], mainw->event_list, pb_start_event, exact_preview);
            for (i = 1; i < prefs->num_rtaudiobufs; i++) {
              // reset because audio sync may have set it
              if (mainw->multitrack) mainw->jackd->abufs[i]->arate = cfile->arate;
              else mainw->jackd->abufs[i]->arate = mainw->jackd->sample_out_rate;
              fill_abuffer_from(mainw->jackd->abufs[i], mainw->event_list, NULL, FALSE);
            }

            pthread_mutex_lock(&mainw->abuf_mutex);
            mainw->jackd->read_abuf = 0;
            mainw->abufs_to_fill = 0;
            pthread_mutex_unlock(&mainw->abuf_mutex);
            if (mainw->event_list)
              mainw->jackd->in_use = TRUE;
          }
#endif
#ifdef HAVE_PULSE_AUDIO
          if (audio_player == AUD_PLAYER_PULSE) {
            int i;
            mainw->write_abuf = 0;
            /// fill our audio buffers now
            /// this will also get our effects state

            /// this is the IN rate, everything is resampled to this rate and then to output rate
            if (mainw->multitrack) mainw->pulsed->abufs[0]->arate = cfile->arate;
            else mainw->pulsed->abufs[0]->arate = mainw->pulsed->out_arate;

            /// need to set asamps, in case padding with silence is needed
            mainw->pulsed->abufs[0]->out_asamps = mainw->pulsed->out_asamps;

            fill_abuffer_from(mainw->pulsed->abufs[0], mainw->event_list, pb_start_event, exact_preview);

            for (i = 1; i < prefs->num_rtaudiobufs; i++) {
              if (mainw->multitrack) mainw->pulsed->abufs[i]->arate = cfile->arate;
              else mainw->pulsed->abufs[i]->arate = mainw->pulsed->out_arate;
              mainw->pulsed->abufs[i]->out_asamps = mainw->pulsed->out_asamps;
              fill_abuffer_from(mainw->pulsed->abufs[i], mainw->event_list, NULL, FALSE);
            }

            pthread_mutex_lock(&mainw->abuf_mutex);
            mainw->pulsed->read_abuf = 0;
            mainw->abufs_to_fill = 0;
            pthread_mutex_unlock(&mainw->abuf_mutex);
            /* if (mainw->event_list) { */
            /*   mainw->pulsed->in_use = TRUE; */
            /* } */
          }
#endif
        }
      }

      if (AUD_SRC_EXTERNAL) audio_analyser_start(AUDIO_SRC_EXT);
      if (!mainw->multitrack || !mainw->multitrack->pb_start_event) {
        do_progress_dialog(FALSE, FALSE, NULL);

        // reset audio buffers
#ifdef ENABLE_JACK
        if (audio_player == AUD_PLAYER_JACK && mainw->jackd) {
          // must do this before deinit fx
          pthread_mutex_lock(&mainw->abuf_mutex);
          mainw->jackd->read_abuf = -1;
          mainw->jackd->in_use = FALSE;
          pthread_mutex_unlock(&mainw->abuf_mutex);
        }
#endif
#ifdef HAVE_PULSE_AUDIO
        if (audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
          // must do this before deinit fx
          pthread_mutex_lock(&mainw->abuf_mutex);
          mainw->pulsed->read_abuf = -1;
          mainw->pulsed->in_use = FALSE;
          pthread_mutex_unlock(&mainw->abuf_mutex);
        }
#endif
        free_track_decoders();
      } else {
        // play from middle of mt timeline
        cfile->next_event = mainw->multitrack->pb_start_event;

        if (!has_audio_buffers) {
          // no audio buffering
          // get just effects state
          get_audio_and_effects_state_at(mainw->multitrack->event_list,
                                         mainw->multitrack->pb_start_event, 0,
                                         LIVES_PREVIEW_TYPE_VIDEO_ONLY,
                                         mainw->multitrack->exact_preview, NULL);
        }

        do_progress_dialog(FALSE, FALSE, NULL);

        // reset audio read buffers
#ifdef ENABLE_JACK
        if (audio_player == AUD_PLAYER_JACK && mainw->jackd) {
          // must do this before deinit fx
          pthread_mutex_lock(&mainw->abuf_mutex);
          mainw->jackd->read_abuf = -1;
          mainw->jackd->in_use = FALSE;
          pthread_mutex_unlock(&mainw->abuf_mutex);
        }
#endif
#ifdef HAVE_PULSE_AUDIO
        if (audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
          // must do this before deinit fx
          pthread_mutex_lock(&mainw->abuf_mutex);
          mainw->pulsed->read_abuf = -1;
          mainw->pulsed->in_use = FALSE;
          pthread_mutex_unlock(&mainw->abuf_mutex);
        }
#endif

        // realtime effects off (for multitrack and event_list preview)
        deinit_render_effects();

        cfile->next_event = NULL;

        if (!(mainw->preview && mainw->multitrack && mainw->multitrack->is_rendering))
          free_track_decoders();

        // multitrack loop - go back to loop start position unless external transport moved us
        if (mainw->scratch == SCRATCH_NONE) {
          mainw->multitrack->pb_start_event = mainw->multitrack->pb_loop_event;
        }

        mainw->effort = 0;
        if (mainw->multitrack) pb_start_event = mainw->multitrack->pb_start_event;
      }
    } while (mainw->multitrack && mainw->loop_cont &&
             (mainw->cancelled == CANCEL_NONE || mainw->cancelled == CANCEL_EVENT_LIST_END));
  }

  mainw->osc_block = TRUE;
  mainw->rte_textparm = NULL;
  mainw->playing_file = -1;
  mainw->abufs_to_fill = 0;

  if (AUD_SRC_EXTERNAL) audio_analyser_end(AUDIO_SRC_EXT);

  if (mainw->ext_playback) {
#ifndef IS_MINGW
    vid_playback_plugin_exit();
    if (mainw->play_window) lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
#else
    if (mainw->play_window) lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
    vid_playback_plugin_exit();
#endif
  }

  // play completed
  if (prefs->show_player_stats) {
    if (mainw->fps_measure > 0) {
      fps_med = (double)mainw->fps_measure
                / ((double)lives_get_relative_ticks(mainw->origsecs, mainw->orignsecs)
                   / TICKS_PER_SECOND_DBL);
    }
  }

  mainw->osc_auto = 0;

  // do this here before closing the audio tracks, easing_events, soft_deinits, etc
  if (mainw->record && !mainw->record_paused)
    event_list_add_end_events(mainw->event_list, TRUE);

  if (!mainw->foreign) {
    /// deinit any active real time effects
    really_deinit_effects();
    if (prefs->allow_easing && !mainw->multitrack) {
      // any effects which were "easing out" should be deinited now
      deinit_easing_effects();
    }
  }

  if (mainw->loop_locked) unlock_loop_lock();
  if (prefs->show_msg_area) {
    lives_widget_set_size_request(mainw->message_box, -1, MIN_MSGBAR_HEIGHT);
  }

  mainw->jack_can_stop = FALSE;
  if ((mainw->current_file == current_file) && CURRENT_CLIP_IS_VALID) {
    cfile->pointer_time = pointer_time;
    cfile->real_pointer_time = real_pointer_time;
  }

  // tell the audio cache thread to terminate, else we can get in a deadlock where the player is waiting for
  // more data, and we are waiting for the player to finish
  if (audio_player == AUD_PLAYER_JACK
      || (mainw->event_list && !mainw->record && (!mainw->is_rendering
          || !mainw->preview || mainw->preview_rendering)))
    audio_cache_finish();

#ifdef ENABLE_JACK
  if (audio_player == AUD_PLAYER_JACK && (mainw->jackd || mainw->jackd_read)) {
    if (prefs->audio_opts & AUDIO_OPTS_AUX_PLAY)
      unregister_aux_audio_channels(1);
    if (AUD_SRC_EXTERNAL) {
      if (prefs->audio_opts & AUDIO_OPTS_EXT_FX)
        unregister_audio_client(FALSE);
    }

    if (mainw->jackd_read || mainw->aud_rec_fd != -1)
      jack_rec_audio_end(TRUE);

    if (mainw->jackd_read) {
      mainw->jackd_read->in_use = FALSE;
    }

    if (mainw->jackd)
      jack_conx_exclude(mainw->jackd_read, mainw->jackd, FALSE);

    // send jack transport stop
    if (!mainw->preview && !mainw->foreign) {
      if (mainw->lives_can_stop) {
        jack_pb_stop(mainw->jackd_trans);
        if (!mainw->multitrack
            || (mainw->cancelled != CANCEL_USER_PAUSED
                && !((mainw->cancelled == CANCEL_NONE
                      || mainw->cancelled == CANCEL_NO_MORE_PREVIEW)
                     && mainw->multitrack->is_paused))) {
          jack_transport_update(mainw->jackd_trans, cfile->real_pointer_time);
        }
      }
    }

    if (mainw->alock_abuf) {
      if (mainw->alock_abuf->_fd != -1) {
        if (!lives_buffered_rdonly_is_slurping(mainw->alock_abuf->_fd)) {
          lives_close_buffered(mainw->alock_abuf->_fd);
        }
      }
      free_audio_frame_buffer(mainw->alock_abuf);
      mainw->alock_abuf = NULL;
    }

    // tell jack client to close audio file
    if (mainw->jackd && mainw->jackd->playing_file > 0) {
      ticks_t timeout = 0;
      if (mainw->cancelled != CANCEL_AUDIO_ERROR) {
        lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
        while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd)) {
          sched_yield(); // wait for seek
          lives_usleep(prefs->sleep_time);
        }
        lives_alarm_clear(alarm_handle);
      }
      if (mainw->cancelled == CANCEL_AUDIO_ERROR) mainw->cancelled = CANCEL_ERROR;
      jack_message.command = ASERVER_CMD_FILE_CLOSE;
      jack_message.data = NULL;
      jack_message.next = NULL;
      mainw->jackd->msgq = &jack_message;
      if (timeout == 0) handle_audio_timeout();
      else lives_nanosleep_while_true(mainw->jackd->playing_file > -1);
    }
  } else {
#endif
#ifdef HAVE_PULSE_AUDIO
    if (audio_player == AUD_PLAYER_PULSE && (mainw->pulsed || mainw->pulsed_read)) {
      if (mainw->pulsed_read || mainw->aud_rec_fd != -1)
        pulse_rec_audio_end(TRUE);

      if (mainw->pulsed_read) {
        mainw->pulsed_read->in_use = FALSE;
        pulse_driver_cork(mainw->pulsed_read);
      }

      // tell pulse client to close audio file
      if (mainw->pulsed) {
        if (mainw->pulsed->playing_file > 0 || mainw->pulsed->fd > 0) {
          ticks_t timeout = 0;
          if (mainw->cancelled != CANCEL_AUDIO_ERROR) {
            lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
            while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed)) {
              lives_usleep(prefs->sleep_time);
            }
            lives_alarm_clear(alarm_handle);
          }
          if (mainw->cancelled == CANCEL_AUDIO_ERROR) mainw->cancelled = CANCEL_ERROR;
          pulse_message.command = ASERVER_CMD_FILE_CLOSE;
          pulse_message.data = NULL;
          pulse_message.next = NULL;
          mainw->pulsed->msgq = &pulse_message;
          if (timeout == 0)  {
            handle_audio_timeout();
            mainw->pulsed->playing_file = -1;
            mainw->pulsed->fd = -1;
          } else {
            lives_nanosleep_while_true((mainw->pulsed->playing_file > -1 || mainw->pulsed->fd > 0));
          }
        }

        // MAKE SURE TO UNCORK THIS LATER
        pulse_driver_cork(mainw->pulsed);
      }
    } else {
#endif
#ifdef ENABLE_JACK
    }
#endif
#ifdef HAVE_PULSE_AUDIO
  }
#endif

  lives_freep((void **)&mainw->urgency_msg);
  mainw->actual_frame = 0;

  lives_notify(LIVES_OSC_NOTIFY_PLAYBACK_STOPPED, "");

  if (mainw->new_clip != -1) {
    mainw->current_file = mainw->new_clip;
    mainw->new_clip = -1;
  }

  // stop the audio players
#ifdef ENABLE_JACK
  if (audio_player == AUD_PLAYER_JACK && mainw->jackd) {
    mainw->jackd->in_use = FALSE;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
    mainw->pulsed->in_use = FALSE;
  }
#endif

  /// stop the players before the cache thread, else the players may try to play from a non-existent file
  if (audio_player == AUD_PLAYER_JACK
      || (mainw->event_list && !mainw->record && (!mainw->is_rendering
          || !mainw->preview || mainw->preview_rendering)))
    audio_cache_end();

  // terminate autolives if running
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->autolives), FALSE);

  // PLAY FINISHED...

  if (prefs->stop_screensaver) lives_reenable_screensaver();

  if (prefs->show_gui) {
    if (mainw->gui_hidden) unhide_main_gui();
    else lives_widget_show_now(LIVES_MAIN_WINDOW_WIDGET);
  }

  if (!mainw->multitrack && mainw->ext_audio_mon)
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_mon), FALSE);

  if ((prefs->audio_opts & AUDIO_OPTS_IS_LOCKED
       && (prefs->audio_opts & AUDIO_OPTS_AUTO_UNLOCK))) {
    aud_lock_act(NULL, LIVES_INT_TO_POINTER(FALSE));
  }

  // TODO ***: use MIDI output port for this
  if (!mainw->foreign && prefs->midisynch) {
    lives_cancel_t cancelled = mainw->cancelled;
    lives_system(EXEC_MIDISTOP, TRUE);
    mainw->cancelled = cancelled;
  }
  // we could have started by playing a generator, which could've been closed
  if (!mainw->files[current_file]) current_file = mainw->current_file;

  if (!APLAYER_REALTIME && stopcom) {
    lives_cancel_t cancelled = mainw->cancelled;
    // kill sound (if still playing)
    wait_for_audio_stop(stopcom);
    mainw->aud_file_to_kill = -1;
    lives_free(stopcom);
    mainw->cancelled = cancelled;
  }

  if (CURRENT_CLIP_IS_NORMAL) {
    char *clipdir = get_clip_dir(mainw->current_file);
    stfile = lives_build_filename(clipdir, LIVES_STATUS_FILE_NAME, NULL);
    lives_free(clipdir);
    lives_snprintf(cfile->info_file, PATH_MAX, "%s", stfile);
    lives_free(stfile);
    cfile->last_play_sequence = mainw->play_sequence;
  }

  if (IS_VALID_CLIP(mainw->scrap_file) && mainw->files[mainw->scrap_file]->ext_src) {
    lives_close_buffered(LIVES_POINTER_TO_INT(mainw->files[mainw->scrap_file]->ext_src));
    mainw->files[mainw->scrap_file]->ext_src = NULL;
    mainw->files[mainw->scrap_file]->ext_src_type = LIVES_EXT_SRC_NONE;
  }

  if (mainw->foreign) {
    // recording from external window capture
    mainw->pwidth = lives_widget_get_allocation_width(mainw->playframe) - H_RESIZE_ADJUST;
    mainw->pheight = lives_widget_get_allocation_height(mainw->playframe) - V_RESIZE_ADJUST;

    cfile->hsize = mainw->pwidth;
    cfile->vsize = mainw->pheight;

    lives_xwindow_set_keep_above(mainw->foreign_window, FALSE);

    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

    return;
  }

  disable_record();
  prefs->pb_quality = future_prefs->pb_quality;
  mainw->lockstats = FALSE;
  mainw->blend_palette = WEED_PALETTE_END;
  mainw->audio_stretch = 1.;

  lives_hooks_trigger(THREADVAR(hook_stacks), COMPLETED_HOOK);

  if (!mainw->multitrack) {
    if (mainw->faded || mainw->fs) {
      main_thread_execute_rvoid_pvoid(unfade_background);
      //unfade_background();
    }

    if (mainw->sep_win) add_to_playframe();

    if (CURRENT_CLIP_HAS_VIDEO) {
      //resize(1.);
      lives_widget_show_all(mainw->playframe);
      lives_frame_set_label(LIVES_FRAME(mainw->playframe), NULL);
    }

    if (palette->style & STYLE_1) {
      lives_widget_show(mainw->sep_image);
    }

    if (prefs->show_msg_area && !mainw->multitrack) {
      lives_widget_show_all(mainw->message_box);
      reset_message_area(); ///< necessary
    }

    lives_widget_show(mainw->frame1);
    lives_widget_show(mainw->frame2);
    lives_widget_show(mainw->eventbox3);
    lives_widget_show(mainw->eventbox4);
    lives_widget_show(mainw->sep_image);

    if (!prefs->hide_framebar && !prefs->hfbwnp) {
      lives_widget_show(mainw->framebar);
    }
  }

  if (!is_realtime_aplayer(audio_player)) mainw->mute = mute;

  /// kill the separate play window
  if (mainw->play_window) {
    if (mainw->fs) {
      mainw->ignore_screen_size = TRUE;
      if (!mainw->multitrack || !mainw->fs
          || (mainw->cancelled != CANCEL_USER_PAUSED
              && !((mainw->cancelled == CANCEL_NONE
                    || mainw->cancelled == CANCEL_NO_MORE_PREVIEW)))) {
      }
      if (prefs->show_desktop_panel && (capable->wm_caps.pan_annoy & ANNOY_DISPLAY)
          && (capable->wm_caps.pan_annoy & ANNOY_FS) && (capable->wm_caps.pan_res & RES_HIDE)
          && capable->wm_caps.pan_res & RESTYPE_ACTION) {
        show_desktop_panel();
      }
      lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
    }
    if (prefs->sepwin_type == SEPWIN_TYPE_NON_STICKY) {
      kill_play_window();
    } else {
      /// or resize it back to single size
      if (CURRENT_CLIP_IS_VALID && cfile->is_loaded && cfile->frames > 0 && !mainw->is_rendering
          && (cfile->clip_type != CLIP_TYPE_GENERATOR)) {
        if (mainw->preview_controls) {
          /// create the preview in the sepwin
          if (prefs->show_gui) {
            lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
            lives_widget_show_all(mainw->preview_box);
            lives_widget_set_no_show_all(mainw->preview_controls, TRUE);
          }
        }
        if (mainw->current_file != current_file) {
          // now we have to guess how to center the play window
          mainw->opwx = mainw->opwy = -1;
          mainw->preview_frame = 0;
        }
      }
      if (!mainw->multitrack) {
        mainw->playing_file = -2;
        if (mainw->fs) mainw->ignore_screen_size = TRUE;
        resize_play_window();
        mainw->playing_file = -1;
        lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);

        if (!mainw->preview_box) {
          // create the preview box that shows frames
          make_preview_box();
        }
        // and add it to the play window
        if (!lives_widget_get_parent(mainw->preview_box)
            && CURRENT_CLIP_IS_NORMAL && !mainw->is_rendering) {
          if (mainw->play_window) {
            lives_widget_queue_draw(mainw->play_window);
            lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
            play_window_set_title();
          }
        }

        if (mainw->play_window) {
          if (prefs->show_playwin) {
            lives_window_present(LIVES_WINDOW(mainw->play_window));
            lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
            unhide_cursor(lives_widget_get_xwindow(mainw->play_window));
            lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
            // need to recheck mainw->play_window after this
            lives_widget_show_all(mainw->preview_box);
            lives_widget_grab_focus(mainw->preview_spinbutton);
            lives_widget_set_no_show_all(mainw->preview_controls, TRUE);
            if (mainw->play_window) {
              lives_widget_process_updates(mainw->play_window);
              // need to recheck after calling process_updates
              if (mainw->play_window) {
                lives_window_center(LIVES_WINDOW(mainw->play_window));
                clear_widget_bg(mainw->play_image, mainw->play_surface);
                load_preview_image(FALSE);
              }
            }
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  /// free the last frame image(s)
  if (mainw->frame_layer) {
    check_layer_ready(mainw->frame_layer);
    weed_layer_unref(mainw->frame_layer);
    mainw->frame_layer = NULL;
  }

  if (mainw->blend_layer) {
    check_layer_ready(mainw->blend_layer);
    weed_layer_unref(mainw->blend_layer);
    mainw->blend_layer = NULL;
  }

  cliplist = mainw->cliplist;
  while (cliplist) {
    int i = LIVES_POINTER_TO_INT(cliplist->data);
    if (IS_NORMAL_CLIP(i) && mainw->files[i]->clip_type == CLIP_TYPE_FILE)
      chill_decoder_plugin(i);
    mainw->files[i]->adirection = LIVES_DIRECTION_FORWARD;
    cliplist = cliplist->next;
  }

  // join any threads created for this
  if (mainw->scrap_file > -1) flush_scrap_file();

  if (!mainw->foreign) {
    unhide_cursor(lives_widget_get_xwindow(mainw->playarea));
  }

  if (CURRENT_CLIP_IS_VALID) cfile->play_paused = FALSE;

  if (mainw->blend_file != -1 && mainw->blend_file != mainw->current_file
      && mainw->files[mainw->blend_file] &&
      mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
    int xcurrent_file = mainw->current_file;
    weed_bg_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
    mainw->current_file = xcurrent_file;
  }

  mainw->filter_map = mainw->afilter_map = mainw->audio_event = NULL;

  /// disable the freeze key
  lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), freeze_closure);
  lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), bg_freeze_closure);

  if (needsadone) d_print_done();

  /// free any pre-cached frame
  if (mainw->frame_layer_preload && mainw->pred_clip != -1) {
    check_layer_ready(mainw->frame_layer_preload);
    weed_layer_free(mainw->frame_layer_preload);
  }
  mainw->frame_layer_preload = NULL;

  if (!prefs->vj_mode) {
    /// pop up error dialog if badly sized frames were detected
    if (mainw->size_warn) {
      if (mainw->size_warn > 0 && mainw->files[mainw->size_warn]) {
        char *smsg = lives_strdup_printf(
                       _("\n\nSome frames in the clip\n%s\nare wrongly sized.\nYou should "
                         "click on Tools--->Resize All\n"
                         "and resize all frames to the current size.\n"),
                       mainw->files[mainw->size_warn]->name);
        widget_opts.non_modal = TRUE;
        do_error_dialog(smsg);
        widget_opts.non_modal = FALSE;
        lives_free(smsg);
      }
    }
  }
  mainw->size_warn = 0;

  // set processing state again if a previewe finished
  // CAUTION !!
  mainw->is_processing = mainw->preview;
  /////////////////

  if (prefs->volume != (double)future_prefs->volume)
    pref_factory_float(PREF_MASTER_VOLUME, future_prefs->volume, TRUE);

  // TODO - ????
  if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_DISK
      && cfile->frames == 0 && mainw->record_perf) {
    lives_signal_handler_block(mainw->record_perf, mainw->record_perf_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->record_perf), FALSE);
    lives_signal_handler_unblock(mainw->record_perf, mainw->record_perf_func);
  }

  // TODO - can this be done earlier ?
  if (mainw->cancelled == CANCEL_APP_QUIT) on_quit_activate(NULL, NULL);

  /// end record performance

  // TODO - use awai_audio_queue

#ifdef ENABLE_JACK
  if (audio_player == AUD_PLAYER_JACK && mainw->jackd) {
    ticks_t timeout;
    lives_alarm_t alarm_handle;
    alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
    lives_nanosleep_until_zero((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd));
    lives_alarm_clear(alarm_handle);
    if (timeout == 0) handle_audio_timeout();
    if (has_audio_buffers) {
      free_jack_audio_buffers();
      audio_free_fnames();
    }
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
    ticks_t timeout;
    lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
    lives_nanosleep_until_zero((timeout = lives_alarm_check(alarm_handle)) > 0
                               && pulse_get_msgq(mainw->pulsed));
    lives_alarm_clear(alarm_handle);
    if (timeout == 0) handle_audio_timeout();
    if (has_audio_buffers) {
      free_pulse_audio_buffers();
      audio_free_fnames();
    }
  }
#endif

  if (THREADVAR(bad_aud_file)) {
    /// we got an error recording audio
    do_write_failed_error_s(THREADVAR(bad_aud_file), NULL);
    lives_freep((void **)&THREADVAR(bad_aud_file));
  }

  for (int i = 0; i < MAX_FILES; i++) {
    if (IS_VALID_CLIP(i)) {
      lives_clip_t *sfile = mainw->files[i];
      if (sfile->aplay_fd > -1) {
        lives_close_buffered(sfile->aplay_fd);
        sfile->aplay_fd = -1;
      }
    }
  }

  //////
  main_thread_execute_pvoid(post_playback, -1, NULL);
  if (!mainw->multitrack) redraw_timeline(mainw->current_file);

  if (CURRENT_CLIP_IS_VALID) {
    if (!mainw->multitrack) {
      lives_ce_update_timeline(0, cfile->real_pointer_time);
      mainw->ptrtime = cfile->real_pointer_time;
      lives_widget_queue_draw(mainw->eventbox2);
    }
  }

  if (lazy_start) {
    lives_proc_thread_ref(mainw->lazy_starter);
    if (mainw->lazy_starter) {
      lives_proc_thread_request_resume(mainw->lazy_starter);
      lives_proc_thread_unref(mainw->lazy_starter);
    }
  }

  if (prefs->show_msg_area) {
    if (mainw->idlemax == 0) {
      lives_idle_add_simple(resize_message_area, NULL);
    }
    mainw->idlemax = DEF_IDLE_MAX;
  }

  /// need to do this here, in case we want to preview with only a generator and no other clips (which will close to -1)
  if (mainw->record) {
    lives_idle_add_simple(render_choice_idle, LIVES_INT_TO_POINTER(FALSE));
  }

  mainw->record_paused = mainw->record_starting = mainw->record = FALSE;

  mainw->ignore_screen_size = FALSE;

  /// re-enable generic clip switching
  mainw->noswitch = FALSE;

  /* if (prefs->show_dev_opts) */
  /*   g_print("nrefs = %d\n", check_ninstrefs()); */
}

