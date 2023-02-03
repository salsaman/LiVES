// player-controller.c
// LiVES
// (c) G. Finch 2003 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

/*
  This file contains API functions for running and cleannig up before and after the
  actual playback  (accessed via calls to do_progress_dualog()
  Here we handle playback of various types - clip editor, multitrack,
  as well as showing previews ion the loader window.
  The same functon will also play processing previews.
*/

#include "main.h"
#include "startup.h"
#include "functions.h"
#include "callbacks.h"
#include "effects-weed.h"
#include "resample.h"
#include "clip_load_save.h"

static boolean _start_playback(int play_type) {
  // play types: (some are no longer valid)
  //  0 - normal
  //  1 - normal, selection only (inc. preview)
  // 3 - osc_playall
  // 4 osc_playsel
  //  6 - generator start
  // 8 playall from menu
  int new_file, old_file;
  boolean flag = FALSE;

  if (play_type != 6) {
    mainw->pre_play_file = mainw->current_file;
  }

  if (play_type != 8 && mainw->noswitch) return TRUE;
  player_desensitize();

  switch (play_type) {
  case 8: case 6: case 0:
    /// normal play
    if (play_type == 0) flag = TRUE;
    play_all(flag);
    break;
  case 1:
    /// play selection, including play preview
    if (!mainw->multitrack) play_sel();
    else {
      play_file();
      //multitrack_play_sel(NULL, mainw->multitrack);
    }
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
      // TODO - want to switch to clip -1,and avoid swithcing to clipboard
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

  if (mainw->player_proc) mainw->player_proc = NULL;

  return FALSE;
}


// These are now the entry points for clip playback
// start_playback (blocking) and start_playback_async (nonblocking)

// calling play_file() directly is no longer permitted

// only a single instance of player is allowed at any particular time
// during playback, mainw->current_clip must not be changed directly
// this should be done by setting mainw->new_clip, similarly we can set mainw->new_blen_file
// and mainw->close_this_clip
// (check for mainw->noswitch == TRUE)
// playback will only stop when mainw->cancelled
// is set to a value other than CANCEL_NONE.
//
// realtime effects, ie. mainw->rte may not be altered, instead rte_on_off_callback must be called
// player output cahnges - sepwin / fullsize / double etc. must be done by setting a hook
// callback for mainw->player_proc's SYNC_ANNOUNCE_HOOK (see keyboard.c for examples)
//
LIVES_GLOBAL_INLINE boolean start_playback(int type) {
  // BLOCKING playback
  // - block until playback stops
  lives_proc_thread_t lpt;
  /* if (!is_fg_thread()) { */
  /*   // if called by bg thread, thread itself becomes player, and will block until play finishes */
  /*   mainw->player_proc = THREADVAR(proc_thread); */
  /*   return _start_playback(type); */
  /* } */
  mainw->player_proc = lpt = lives_proc_thread_create(0, _start_playback, -1, "i", type);
  // the main thread will block here, waiting for playback to end.
  // during this time, it will service only bg requests set via fg_service_call
  lives_proc_thread_join(lpt);
  return FALSE;
}


void start_playback_async(int type) {
  // nonblocking playback, player is launcehd in a pool thread, and this thread returns
  // immediately
  mainw->player_proc = lives_proc_thread_create(0, _start_playback, 0, "i", type);
}


#if 0
static void prep_audio_player(frames_t audio_end, int arate, int asigned, int aendian) {
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
    IF_APLAYER_JACK(jack_aud_pb_ready(mainw->jackd, mainw->current_file));
    IF_APLAYER_PULSE(pulse_aud_pb_ready(mainw->pulsed, mainw->current_file));
  }
}
#endif

static double fps_med = 0.;

static void post_playback(void) {
  // gui thread only
  if (!mainw->multitrack) {
    if (mainw->faded || mainw->fs) {
      unfade_background();
    }

    if (mainw->sep_win) add_to_playframe();

    if (CURRENT_CLIP_HAS_VIDEO) {
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

  if (!mainw->foreign) {
    unhide_cursor(lives_widget_get_xwindow(mainw->playarea));
  }

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
        if (mainw->current_file != mainw->pre_play_file) {
          // now we have to guess how to center the play window
          mainw->opwx = mainw->opwy = -1;
          mainw->preview_frame = 0;
        }
      }

      if (!mainw->multitrack) {
        //
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
            lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
            play_window_set_title();
          }
        }

        if (mainw->play_window) {
          if (prefs->show_playwin) {
            unhide_cursor(lives_widget_get_xwindow(mainw->play_window));
            lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
            // need to recheck mainw->play_window after this
            lives_widget_show_all(mainw->preview_box);
            lives_widget_grab_focus(mainw->preview_spinbutton);
            lives_widget_set_no_show_all(mainw->preview_controls, TRUE);
            if (mainw->play_window) {
              if (mainw->play_window) {
                lives_window_center(LIVES_WINDOW(mainw->play_window));
                clear_widget_bg(mainw->play_image, mainw->play_surface);
                load_preview_image(FALSE);
              }
            }
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  if (prefs->show_player_stats && mainw->fps_measure > 0) {
    d_print(_("Average FPS was %.4f (%d frames in clock time of %f)\n"), fps_med, mainw->fps_measure,
            (double)lives_get_relative_ticks(mainw->origsecs, mainw->orignsecs)
            / TICKS_PER_SECOND_DBL);
  }

  if (mainw->new_vpp) {
    mainw->vpp = open_vid_playback_plugin(mainw->new_vpp, TRUE);
    mainw->new_vpp = NULL;
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

  if (!mainw->multitrack) redraw_timeline(mainw->current_file);

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
    if (prefs->hfbwnp) lives_widget_hide(mainw->framebar);
    set_drawing_area_from_pixbuf(mainw->play_image, NULL, mainw->play_surface);
    lives_widget_set_opacity(mainw->play_image, 0.);
    lives_widget_set_opacity(mainw->playframe, 0.);
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

  if (CURRENT_CLIP_IS_VALID) {
    if (!mainw->multitrack) {
      lives_ce_update_timeline(0, cfile->real_pointer_time);
      if (CURRENT_CLIP_IS_VALID) {
        mainw->ptrtime = cfile->real_pointer_time;
      }
      lives_widget_queue_draw(mainw->eventbox2);
    }
  }

  player_sensitize();
}


static void pre_playback(void) {
  // gui thread only
  short audio_player = prefs->audio_player;

  if (!mainw->preview || !cfile->opening) {
    enable_record();
    desensitize();
#ifdef ENABLE_JACK
    if (!(mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
          && (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE)))
      lives_widget_set_sensitive(mainw->spinbutton_pb_fps, TRUE);
#endif
  }

  if (!mainw->multitrack) {
    /// blank the background if asked to
    if ((mainw->faded || (prefs->show_playwin && !prefs->show_gui)
         || (mainw->fs && (!mainw->sep_win))) && (cfile->frames > 0 ||
             mainw->foreign)) {
      fade_background();
    }

    if ((!mainw->sep_win || (!mainw->faded && (prefs->sepwin_type != SEPWIN_TYPE_STICKY)))
        && (cfile->frames > 0 || mainw->preview_rendering || mainw->foreign)) {
      /// show the frame in the main window
      lives_widget_set_opacity(mainw->playframe, 1.);
      lives_widget_show_all(mainw->playframe);
    }

    // NB make sure playframe is mapped
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

    add_to_playframe();

    if (CURRENT_CLIP_HAS_VIDEO) {
      lives_widget_set_frozen(mainw->spinbutton_start, TRUE, 0.);
      lives_widget_set_frozen(mainw->spinbutton_end, TRUE, 0.);
    }
    if (mainw->play_window) {
      lives_widget_hide(mainw->preview_controls);
      if (prefs->pb_hide_gui) hide_main_gui();
    }

    if (prefs->msgs_nopbdis || (prefs->show_msg_area && mainw->double_size)) {
      lives_widget_hide(mainw->message_box);
    }
    lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), !mainw->double_size);
  }

#ifdef ENABLE_JACK
  if (!(mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
        && (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE)))
#endif
    lives_widget_set_sensitive(mainw->stop, TRUE);

  else if (!cfile->opening) {
    if (!mainw->is_processing) mt_swap_play_pause(mainw->multitrack, TRUE);
    else {
      lives_widget_set_sensitive(mainw->multitrack->playall, FALSE);
      lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
    }
  }

  lives_widget_set_sensitive(mainw->m_playselbutton, FALSE);
  lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  lives_widget_set_sensitive(mainw->m_mutebutton, is_realtime_aplayer(audio_player) || mainw->multitrack);
  lives_widget_set_sensitive(mainw->m_loopbutton, (!cfile->achans || mainw->mute || mainw->multitrack ||
                             mainw->loop_cont || is_realtime_aplayer(audio_player))
                             && mainw->current_file > 0);
  lives_widget_set_sensitive(mainw->loop_continue, (!cfile->achans || mainw->mute || mainw->loop_cont ||
                             is_realtime_aplayer(audio_player))
                             && mainw->current_file > 0);

  if (cfile->frames == 0) {
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
          if (mainw->play_window && prefs->show_playwin) {
            lives_window_present(LIVES_WINDOW(mainw->play_window));
            lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
	    // *INDENT-OFF*
	  }}}}
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
    }

    if (mainw->fs && !mainw->sep_win && cfile->frames > 0) {
      fullscreen_internal();
    }
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
  lives_widget_set_sensitive(mainw->m_stopbutton, TRUE);
}


/// play the current clip from 'mainw->play_start' to 'mainw->play_end'
void play_file(void) {
  LiVESWidgetClosure *freeze_closure, *bg_freeze_closure;
  LiVESList *cliplist;
  weed_plant_t *pb_start_event = NULL;
  weed_layer_t *old_flayer;

  double pointer_time = cfile->pointer_time;
  double real_pointer_time = cfile->real_pointer_time;

  short audio_player = prefs->audio_player;

  boolean mute;
  boolean needsadone = FALSE;

  boolean lazy_start = FALSE;
  boolean lazy_rfx = FALSE;

#ifdef RT_AUDIO
  boolean exact_preview = FALSE;
#endif
  boolean has_audio_buffers = FALSE;

  int arate;

  int current_file = mainw->current_file;

  ____FUNC_ENTRY____(play_file, "", "");

  /// from now on we can only switch at the designated SWITCH POINT
  // this applies to bg and fg frames
  // as well as - jumping frames, changing the fx map (mainw->rte). deiniting fx. changing the track count
  // closing current clip, (since this also involves switching clips)
  // changing player size, switching to / from sepwin
  mainw->noswitch = TRUE;
  mainw->cancelled = CANCEL_NONE;

  current_file = mainw->current_file;
  if (mainw->pre_play_file == -1) mainw->pre_play_file = current_file;

  if (!is_realtime_aplayer(audio_player)) mainw->aud_file_to_kill = mainw->current_file;
  else mainw->aud_file_to_kill = -1;

  mainw->ext_playback = FALSE;

  mainw->rec_aclip = -1;

  init_conversions(OBJ_INTENTION_PLAY);

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

  if (!mainw->foreign) {
    if (lives_proc_thread_ref(mainw->lazy_starter) > 1) {
      lives_proc_thread_request_pause(mainw->lazy_starter);
      lazy_start = TRUE;
    }

    if (lives_proc_thread_ref(mainw->helper_procthreads[PT_LAZY_RFX]) > 1) {
      lives_proc_thread_request_pause(mainw->helper_procthreads[PT_LAZY_RFX]);
      lazy_rfx = TRUE;
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

  if (cfile->achans > 0) {
    /* if (mainw->event_list && */
    /*     !(mainw->preview && mainw->is_rendering) && */
    /*     !(mainw->multitrack && mainw->preview && mainw->multitrack->is_rendering)) { */
    /// play performance data
    /* if (event_list_get_end_secs(mainw->event_list) > cfile->frames */
    /*     / cfile->fps && !mainw->playing_sel) { */
    /*   mainw->audio_end = (event_list_get_end_secs(mainw->event_list) * cfile->fps + 1.) */
    /*                      * cfile->arate / cfile->arps; */
    /* } */
    //}

    /* if (mainw->audio_end == 0) { */
    /*   // values in FRAMES */
    /*   mainw->audio_start = calc_time_from_frame(mainw->current_file, */
    /*                        mainw->play_start) * cfile->fps + 1.; */
    /*   mainw->audio_end = calc_time_from_frame(mainw->current_file, mainw->play_end) * cfile->fps + 1.; */
    /*   if (!mainw->playing_sel) { */
    /*     mainw->audio_end = 0; */
    /*   } */
    /* } */

    cfile->aseek_pos = (off_t)((double)(mainw->audio_start - 1.)
                               * cfile->fps * (double)cfile->arate)
                       * cfile->achans * (cfile->asampsize >> 3);
    cfile->async_delta = 0;
  }

  /* if (!cfile->opening_audio && !mainw->loop) { */
  /*   /\** if we are opening audio or looping we just play to the end of audio, */
  /*     otherwise...*\/ */
  /*   audio_end = mainw->audio_end; */
  /* } */

  if (prefs->stop_screensaver) {
    lives_disable_screensaver();
  }

  BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
  main_thread_execute_void(pre_playback, 0);
  BG_THREADVAR(hook_hints) = 0;

  // setting this forces the main thread to block in a loop and only update
  // ths gui context on request (via update_gui in player.c)
  set_gui_loop_tight(TRUE);

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

  mainw->playing_file = mainw->current_file;

  if (mainw->record) {
    if (mainw->event_list) event_list_free(mainw->event_list);
    mainw->event_list = NULL;
    mainw->record_starting = TRUE;
  }

  cfile->play_paused = FALSE;
  mainw->actual_frame = 0;

  mainw->effort = -EFFORT_RANGE_MAX;

  find_when_to_stop();

  // reinit all active effects
  if (!mainw->preview && !mainw->is_rendering && !mainw->foreign) weed_reinit_all();

  if (!mainw->foreign && prefs->midisynch && !mainw->preview) {
    char *com3 = lives_strdup(EXEC_MIDISTART);
    lives_system(com3, TRUE);
    lives_free(com3);
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

    lives_proc_thread_trigger_hooks(mainw->player_proc, SEGMENT_START_HOOK);

    //////////// PLAYBACK START ////////////////

    do {
      reset_old_frame_layer();

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

  // PLAYBACK END /////////////////////

  lives_proc_thread_trigger_hooks(mainw->player_proc, SEGMENT_END_HOOK);

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
        lives_nanosleep_while_true((timeout = lives_alarm_check(alarm_handle)) > 0
                                   && jack_get_msgq(mainw->jackd));
        lives_alarm_clear(alarm_handle);
      }
      if (mainw->cancelled == CANCEL_AUDIO_ERROR) mainw->cancelled = CANCEL_ERROR;
      jack_message.command = ASERVER_CMD_FILE_CLOSE;
      jack_message.data = NULL;
      jack_message.next = NULL;
      mainw->jackd->msgq = &jack_message;
      if (!timeout) handle_audio_timeout();
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

  if (IS_VALID_CLIP(mainw->close_this_clip)) {
    // need to keep blend_file around until we check if it is a generator to close
    int blend_file = mainw->blend_file;

    current_file = mainw->current_file;
    mainw->can_switch_clips = TRUE;
    mainw->current_file = mainw->close_this_clip;
    if (mainw->blend_file == mainw->current_file) blend_file = -1;
    mainw->new_clip = close_current_file(current_file);
    if (!IS_VALID_CLIP(current_file)) mainw->current_file = mainw->new_clip;
    mainw->can_switch_clips = FALSE;
    if (IS_VALID_CLIP(blend_file)) mainw->blend_file = blend_file;
    else mainw->blend_file = -1;
  }

  mainw->close_this_clip = mainw->new_clip = -1;

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

    ____FUNC_EXIT____;

    return;
  }

  disable_record();
  prefs->pb_quality = future_prefs->pb_quality;
  mainw->lockstats = FALSE;
  mainw->blend_palette = WEED_PALETTE_END;
  mainw->audio_stretch = 1.;

  lives_hooks_trigger(NULL, COMPLETED_HOOK);

  if (!is_realtime_aplayer(audio_player)) mainw->mute = mute;

  /// free the last frame image(s)
  old_flayer = get_old_frame_layer();
  if (old_flayer) {
    if (old_flayer == mainw->frame_layer) mainw->frame_layer = NULL;
    if (old_flayer == mainw->frame_layer_preload) mainw->frame_layer_preload = NULL;
    if (old_flayer == mainw->blend_layer) mainw->blend_layer = NULL;
    weed_layer_free(old_flayer);
    reset_old_frame_layer();
  }

  if (mainw->frame_layer) {
    check_layer_ready(mainw->frame_layer);
    weed_layer_free(mainw->frame_layer);
    if (mainw->frame_layer == mainw->frame_layer_preload)
      mainw->frame_layer_preload = NULL;
    if (mainw->frame_layer == mainw->blend_layer)
      mainw->blend_layer = NULL;
    mainw->frame_layer = NULL;
  }

  /// free any pre-cached frame
  if (mainw->frame_layer_preload && mainw->pred_clip != -1) {
    check_layer_ready(mainw->frame_layer_preload);
    weed_layer_free(mainw->frame_layer_preload);
    if (mainw->frame_layer_preload == mainw->blend_layer)
      mainw->blend_layer = NULL;
  }
  mainw->frame_layer_preload = NULL;

  if (mainw->blend_layer) {
    check_layer_ready(mainw->blend_layer);
    weed_layer_free(mainw->blend_layer);
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

  if (CURRENT_CLIP_IS_VALID) cfile->play_paused = FALSE;

  if (IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != mainw->current_file
      && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR) {
    current_file = mainw->current_file;
    mainw->can_switch_clips = TRUE;
    weed_bg_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
    mainw->can_switch_clips = FALSE;
    if (IS_VALID_CLIP(current_file)) mainw->current_file = current_file;
  }

  mainw->filter_map = mainw->afilter_map = mainw->audio_event = NULL;

  /// disable the freeze key
  lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), freeze_closure);
  lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), bg_freeze_closure);

  if (needsadone) d_print_done();

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

  if (CURRENT_CLIP_IS_VALID && cfile->clip_type == CLIP_TYPE_DISK
      && cfile->frames == 0 && mainw->record_perf) {
    // I beleive this is needed in the case where a video generator was recorded using audio
    // from a clip with no video frames
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
    // if we have open files which have been used to load in clip audio
    // we should close them. The audio ought to have been buffered by now
    if (IS_VALID_CLIP(i)) {
      lives_clip_t *sfile = mainw->files[i];
      if (sfile->aplay_fd > -1) {
        lives_close_buffered(sfile->aplay_fd);
        sfile->aplay_fd = -1;
      }
    }
  }

  // allow the main thread to exit from its blocking loop, it will resume normal operations
  lives_nanosleep_while_true(mainw->do_ctx_update);
  set_gui_loop_tight(FALSE);

  /// re-enable generic clip switching
  mainw->noswitch = FALSE;

  // clean up the interface, this has to be executed by the main (gui) thread
  // due to restrictions in gtk+
  BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
  main_thread_execute_void(post_playback, 0);
  BG_THREADVAR(hook_hints) = 0;

  // and startup helpers which were paused during playback may now resume
  if (lazy_start) {
    if (mainw->lazy_starter) {
      lives_proc_thread_request_resume(mainw->lazy_starter);
      lives_proc_thread_unref(mainw->lazy_starter);
    }
  }
  if (lazy_rfx) {
    if (mainw->helper_procthreads[PT_LAZY_RFX]) {
      lives_proc_thread_request_resume(mainw->helper_procthreads[PT_LAZY_RFX]);
      lives_proc_thread_unref(mainw->helper_procthreads[PT_LAZY_RFX]);
    }
  }

  if (prefs->show_msg_area) {
    if (mainw->idlemax == 0) {
      lives_idle_add(resize_message_area, NULL);
    }
    mainw->idlemax = DEF_IDLE_MAX;
  }

  /// need to do this here, in case we want to preview with only
  // a generator and no other clips (which will soon close to -1)
  if (mainw->record || mainw->record_paused) {
    mainw->record_paused = mainw->record_starting = mainw->record = FALSE;
    // if we recorded anything, show the dialog and lete the user select
    // what to do with the reording (render it, transcode it, discard it)
    deal_with_render_choice(FALSE);
  }

  mainw->record_paused = mainw->record_starting = mainw->record = FALSE;

  mainw->ignore_screen_size = FALSE;

  /* if (prefs->show_dev_opts) */
  /*   g_print("nrefs = %d\n", check_ninstrefs()); */

  ____FUNC_EXIT____;
}

