// user-interface.c
// LiVES
// (c) G. Finch 2003 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

/**
   This file is for UI updates at a higher level than create infividual windows / dialog boxes
   The ideal would be a hierarchical approach, in cases where this makes sense:
  - normal function, callback code - no gui code
  	-- user-interface.c - higher level UI functions
		-- other files (gui.c, interface.c, and all other files) : direct UI functions
			-- widget-helper.c, .h. -gtk.h etc : wrapped UI calls

   This is a general rule, in some cases one or more intermediate layers may be skipped for reasons of efficieny.
**/


#include "main.h"
#include "interface.h"
#include "effects.h"
#include "effects-weed.h"
#include "callbacks.h"
#include "omc-learn.h"
#include "startup.h"

static void _pop_to_front(LiVESWidget *dialog, LiVESWidget *extra) {
  // low level, pop dialog to front of dektop stack
  if (prefs->startup_phase && !LIVES_IS_FILE_CHOOSER_DIALOG(dialog)) {
    if (!mainw->is_ready) {
#ifdef GUI_GTK
      gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE); // dont know if this actually does anything...
      gtk_window_set_type_hint(LIVES_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
      gtk_window_set_focus_on_map(LIVES_WINDOW(dialog), TRUE);
#endif
    }
  }
  if (mainw->splash_window) lives_widget_hide(mainw->splash_window);

  if (extra) {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(extra), KEEPABOVE_KEY, dialog);
  }

  lives_widget_show_all(dialog);
  if ((widget_opts.non_modal && !(prefs->focus_steal & FOCUS_STEAL_MSG)) ||
      (!widget_opts.non_modal && !(prefs->focus_steal & (FOCUS_STEAL_BLOCKED
                                   | FOCUS_STEAL_MSG))))
    gtk_window_set_focus_on_map(LIVES_WINDOW(dialog), FALSE);
  lives_window_present(LIVES_WINDOW(dialog));
  lives_grab_add(dialog);
}


void pop_to_front(LiVESWidget *dialog, LiVESWidget *extra) {
  // high level, pop dialog to front of dektop stack
  //
  if (!mainw->is_ready && !is_fg_thread()) {
    main_thread_execute_rvoid(_pop_to_front, 0, "vv", dialog, extra);
  } else _pop_to_front(dialog, extra);
}


// desktop window title ////////

#define MAX_TITLE_LEN 1024
static char win_title[MAX_TITLE_LEN];

void disp_main_title(void) {
#if LIVES_HAS_HEADER_BAR_WIDGET
  if (!(LIVES_IS_PLAYING && mainw->fs && mainw->sepwin))
    lives_header_bar_set_title(LIVES_HEADER_BAR(mainw->hdrbar), win_title);
#else
  lives_window_set_title(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), win_title);
#endif
  if (mainw->play_window) play_window_set_title();
}


static pthread_mutex_t title_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_main_title(const char *file, int untitled) {
  char *tmp, *tmp2;
  char short_file[256];

  pthread_mutex_lock(&title_mutex);

  if (file && CURRENT_CLIP_IS_VALID) {
    if (untitled) {
      lives_snprintf(win_title, MAX_TITLE_LEN, (tmp = _("<%s> %dx%d : %d frames %d bpp %.3f fps")),
                     (tmp2 = get_untitled_name(untitled)),  cfile->hsize, cfile->vsize, cfile->frames, cfile->bpp, cfile->fps);
    } else {
      lives_snprintf(short_file, 256, "%s", file);
      if (cfile->restoring || (cfile->opening && cfile->frames == 123456789)) {
        lives_snprintf(win_title, MAX_TITLE_LEN, (tmp = _("<%s> %dx%d : ??? frames ??? bpp %.3f fps")),
                       (tmp2 = lives_path_get_basename(file)), cfile->hsize, cfile->vsize, cfile->fps);
      } else {
        lives_snprintf(win_title, MAX_TITLE_LEN, (tmp = _("<%s> %dx%d : %d frames %d bpp %.3f fps")),
                       cfile->clip_type != CLIP_TYPE_VIDEODEV ? (tmp2 = lives_path_get_basename(file))
                       : (tmp2 = lives_strdup(file)), cfile->hsize, cfile->vsize, cfile->frames, cfile->bpp, cfile->fps);
      }
    }

    lives_free(tmp); lives_free(tmp2);
  } else {
    lives_snprintf(win_title, MAX_TITLE_LEN, "%s", (tmp = (_("<No File>"))));
    lives_free(tmp);
  }

  if (!(LIVES_IS_PLAYING && mainw->fs && mainw->sep_win)) disp_main_title();

  pthread_mutex_unlock(&title_mutex);
}


/// sensitice / desensitize the interface ////

void sensitize_rfx(void) {
  if (!mainw->foreign) {
    if (mainw->rendered_fx) {
      LiVESWidget *menuitem;
      for (int i = 1; i <= mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom +
           mainw->num_rendered_effects_test; i++) {
        if (i == mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate) continue;
        if (mainw->rendered_fx[i]->num_in_channels == 2) continue;
        menuitem = mainw->rendered_fx[i]->menuitem;
        if (menuitem && LIVES_IS_WIDGET(menuitem)) {
          if (mainw->rendered_fx[i]->num_in_channels == 0) {
            lives_widget_set_sensitive(menuitem, TRUE);
            continue;
          }
          if (mainw->rendered_fx[i]->min_frames >= 0)
            lives_widget_set_sensitive(menuitem, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
        }
      }
      menuitem = mainw->rendered_fx[0]->menuitem;
      if (menuitem && !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID
          && ((has_video_filters(FALSE) && !has_video_filters(TRUE)) ||
              (cfile->achans > 0 && prefs->audio_src == AUDIO_SRC_INT
               && has_audio_filters(AF_TYPE_ANY)) || mainw->agen_key != 0)) {
        lives_widget_set_sensitive(menuitem, TRUE);
      }

      if (mainw->num_rendered_effects_test > 0) {
        lives_widget_set_sensitive(mainw->run_test_rfx_submenu, TRUE);
      }

      if (mainw->has_custom_gens) {
        lives_widget_set_sensitive(mainw->custom_gens_submenu, TRUE);
      }

      if (mainw->has_custom_tools) {
        lives_widget_set_sensitive(mainw->custom_tools_submenu, TRUE);
      }

      if (mainw->has_custom_effects) {
        lives_widget_set_sensitive(mainw->custom_effects_submenu, TRUE);
      }

      if (mainw->resize_menuitem) {
        lives_widget_set_sensitive(mainw->resize_menuitem,
                                   !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
      }
      if (mainw->rfx_submenu) lives_widget_set_sensitive(mainw->rfx_submenu, TRUE);
    }
  }
}


void player_desensitize(void) {
  lives_widget_set_sensitive(mainw->rte_defs_menu, FALSE);
  lives_widget_set_sensitive(mainw->utilities_submenu, FALSE);
  if (!mainw->helper_procthreads[PT_LAZY_RFX] && !prefs->vj_mode)
    lives_widget_set_sensitive(mainw->rfx_submenu, FALSE);
  lives_widget_set_sensitive(mainw->import_theme, FALSE);
  lives_widget_set_sensitive(mainw->export_theme, FALSE);
}


void player_sensitize(void) {
  lives_widget_set_sensitive(mainw->rte_defs_menu, TRUE);
  lives_widget_set_sensitive(mainw->utilities_submenu, TRUE);
  if (!mainw->helper_procthreads[PT_LAZY_RFX] && !prefs->vj_mode)
    lives_widget_set_sensitive(mainw->rfx_submenu, TRUE);
  lives_widget_set_sensitive(mainw->import_theme, TRUE);
  lives_widget_set_sensitive(mainw->export_theme, TRUE);
}


void sensitize(void) {
  // sensitize main window controls
  // READY MODE
  int i;

  if (LIVES_IS_PLAYING || mainw->is_processing || mainw->go_away) return;

  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    return;
  }

  mainw->sense_state &= LIVES_SENSE_STATE_INTERACTIVE;
  mainw->sense_state |= LIVES_SENSE_STATE_SENSITIZED;

  lives_widget_set_sensitive(mainw->open, TRUE);
  lives_widget_set_sensitive(mainw->open_sel, TRUE);
  lives_widget_set_sensitive(mainw->open_vcd_menu, TRUE);
#ifdef HAVE_WEBM
  lives_widget_set_sensitive(mainw->open_loc_menu, TRUE);
#else
  lives_widget_set_sensitive(mainw->open_loc, TRUE);
#endif
  lives_widget_set_sensitive(mainw->open_device_menu, TRUE);
  lives_widget_set_sensitive(mainw->restore, TRUE);
  lives_widget_set_sensitive(mainw->recent_menu, TRUE);
  lives_widget_set_sensitive(mainw->save_as, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID
                             && capable->has_encoder_plugins == PRESENT);
#ifdef LIBAV_TRANSCODE
  lives_widget_set_sensitive(mainw->transcode, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
#endif
  if (!prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->backup, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
    lives_widget_set_sensitive(mainw->save_selection, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO &&
                               capable->has_encoder_plugins == PRESENT);
  }
  lives_widget_set_sensitive(mainw->clear_ds, TRUE);
  lives_widget_set_sensitive(mainw->load_cdtrack, TRUE);
  lives_widget_set_sensitive(mainw->playsel, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);

  if (!prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->copy, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
    lives_widget_set_sensitive(mainw->cut, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
    lives_widget_set_sensitive(mainw->rev_clipboard, !(clipboard == NULL));
    lives_widget_set_sensitive(mainw->playclip, !(clipboard == NULL));
    lives_widget_set_sensitive(mainw->paste_as_new, !(clipboard == NULL));
    lives_widget_set_sensitive(mainw->insert, !(clipboard == NULL));
    lives_widget_set_sensitive(mainw->merge, (clipboard != NULL && !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO));
  }
  lives_widget_set_sensitive(mainw->xdelete, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
  lives_widget_set_sensitive(mainw->trim_video, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO
                             && (cfile->start > 1 || cfile->end < cfile->frames));
  lives_widget_set_sensitive(mainw->playall, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->m_playbutton, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->m_playselbutton, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
  lives_widget_set_sensitive(mainw->m_rewindbutton, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID &&
                             cfile->real_pointer_time > 0.);
  lives_widget_set_sensitive(mainw->m_loopbutton, TRUE);
  lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
  if (mainw->preview_box) {
    lives_widget_set_sensitive(mainw->p_playbutton, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
    lives_widget_set_sensitive(mainw->p_playselbutton, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
    lives_widget_set_sensitive(mainw->p_rewindbutton, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID &&
                               cfile->real_pointer_time > 0.);
    lives_widget_set_sensitive(mainw->p_loopbutton, TRUE);
    lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);
  }

  lives_widget_set_sensitive(mainw->rewind, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID
                             && cfile->real_pointer_time > 0.);
  lives_widget_set_sensitive(mainw->show_file_info, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
  lives_widget_set_sensitive(mainw->show_file_comments, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
  lives_widget_set_sensitive(mainw->full_screen, TRUE);
  if (!prefs->vj_mode)
    lives_widget_set_sensitive(mainw->mt_menu, TRUE);
  lives_widget_set_sensitive(mainw->unicap, TRUE);
#ifdef HAVE_LDVGRAB
  lives_widget_set_sensitive(mainw->firewire, TRUE);
#endif
#ifdef HAVE_YUV4MPEG
  lives_widget_set_sensitive(mainw->tvdev, TRUE);
#endif
  if (!prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->export_proj, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
    lives_widget_set_sensitive(mainw->import_proj, TRUE);
  }

  if (is_realtime_aplayer(prefs->audio_player) && prefs->audio_player != AUD_PLAYER_NONE) {
    lives_widget_set_sensitive(mainw->int_audio_checkbutton, prefs->audio_src != AUDIO_SRC_INT);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, prefs->audio_src != AUDIO_SRC_EXT);
  }

  if (!prefs->vj_mode) {
    if (mainw->rfx_loaded) sensitize_rfx();
  }
  lives_widget_set_sensitive(mainw->record_perf, TRUE);
  if (!prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->export_submenu, !CURRENT_CLIP_IS_CLIPBOARD && (CURRENT_CLIP_HAS_AUDIO));
    lives_widget_set_sensitive(mainw->export_selaudio, (CURRENT_CLIP_HAS_VIDEO && CURRENT_CLIP_HAS_AUDIO));
    lives_widget_set_sensitive(mainw->export_allaudio, CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->normalize_audio, CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->recaudio_submenu, TRUE);
    lives_widget_set_sensitive(mainw->recaudio_sel, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
    lives_widget_set_sensitive(mainw->append_audio, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->trim_submenu, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->voladj, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->fade_aud_in, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->fade_aud_out, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->trim_audio, !CURRENT_CLIP_IS_CLIPBOARD
                               && CURRENT_CLIP_HAS_VIDEO && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->trim_to_pstart, !CURRENT_CLIP_IS_CLIPBOARD && (CURRENT_CLIP_HAS_AUDIO &&
                               cfile->real_pointer_time > 0.));
    lives_widget_set_sensitive(mainw->delaudio_submenu, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->delsel_audio, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO
                               && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->delall_audio, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO
                               && CURRENT_CLIP_HAS_AUDIO);
    lives_widget_set_sensitive(mainw->resample_audio, !CURRENT_CLIP_IS_CLIPBOARD && (CURRENT_CLIP_HAS_AUDIO &&
                               capable->has_sox_sox));
  }
  lives_widget_set_sensitive(mainw->dsize, TRUE);
  lives_widget_set_sensitive(mainw->fade, !(mainw->fs));
  lives_widget_set_sensitive(mainw->mute_audio, TRUE);
  lives_widget_set_sensitive(mainw->loop_video, !CURRENT_CLIP_IS_CLIPBOARD && (CURRENT_CLIP_TOTAL_TIME > 0.));
  lives_widget_set_sensitive(mainw->loop_continue, TRUE);
  lives_widget_set_sensitive(mainw->load_audio, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
  if (mainw->rendered_fx && mainw->rendered_fx[0] && mainw->rendered_fx[0]->menuitem)
    lives_widget_set_sensitive(mainw->rendered_fx[0]->menuitem, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->cg_managegroups, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->load_subs, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->erase_subs, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->subt != NULL);
  if ((check_for_executable(&capable->has_cdda2wav, EXEC_CDDA2WAV)
       || check_for_executable(&capable->has_icedax, EXEC_ICEDAX))
      && *prefs->cdplay_device) lives_widget_set_sensitive(mainw->load_cdtrack, TRUE);
  lives_widget_set_sensitive(mainw->rename, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && !cfile->opening);
  lives_widget_set_sensitive(mainw->change_speed, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  if (!prefs->vj_mode)
    lives_widget_set_sensitive(mainw->resample_video, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
  lives_widget_set_sensitive(mainw->preferences, TRUE);
  if (!prefs->vj_mode)
    lives_widget_set_sensitive(mainw->ins_silence, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
  lives_widget_set_sensitive(mainw->close, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->select_submenu, !CURRENT_CLIP_IS_CLIPBOARD && !mainw->selwidth_locked &&
                             CURRENT_CLIP_HAS_VIDEO);
  update_sel_menu();
#ifdef HAVE_YUV4MPEG
  lives_widget_set_sensitive(mainw->open_yuv4m, TRUE);
#endif

  lives_widget_set_sensitive(mainw->select_new, !CURRENT_CLIP_IS_CLIPBOARD
                             && CURRENT_CLIP_IS_VALID && (cfile->insert_start > 0));
  lives_widget_set_sensitive(mainw->select_last, !CURRENT_CLIP_IS_CLIPBOARD
                             && CURRENT_CLIP_IS_VALID && (cfile->undo_start > 0));
  lives_widget_set_sensitive(mainw->lock_selwidth, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);

  lives_widget_set_sensitive(mainw->undo, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->undoable);
  lives_widget_set_sensitive(mainw->redo, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->redoable);
  lives_widget_set_sensitive(mainw->show_clipboard_info, !(clipboard == NULL));
  lives_widget_set_sensitive(mainw->capture, TRUE);
  lives_widget_set_sensitive(mainw->vj_save_set, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
  lives_widget_set_sensitive(mainw->vj_load_set, !*mainw->set_name);
  lives_widget_set_sensitive(mainw->vj_reset, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
  lives_widget_set_sensitive(mainw->vj_realize, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID &&
                             cfile->frame_index != NULL);
  lives_widget_set_sensitive(mainw->midi_learn, TRUE);
  lives_widget_set_sensitive(mainw->midi_save, has_devicemap(-1));
  //lives_widget_set_sensitive(mainw->toy_tv, TRUE);
  lives_widget_set_sensitive(mainw->autolives, TRUE);
  lives_widget_set_sensitive(mainw->toy_random_frames, TRUE);
  //lives_widget_set_sensitive(mainw->open_lives2lives, TRUE);
  if (!prefs->vj_mode) lives_widget_set_sensitive(mainw->gens_submenu, TRUE);
  lives_widget_set_sensitive(mainw->troubleshoot, TRUE);
  lives_widget_set_sensitive(mainw->expl_missing, TRUE);

  lives_widget_set_sensitive(mainw->show_quota, TRUE);

  if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && (cfile->start == 1 || cfile->end == cfile->frames) &&
      !(cfile->start == 1 &&
        cfile->end == cfile->frames)) {
    lives_widget_set_sensitive(mainw->select_invert, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->select_invert, FALSE);
  }

  if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->menuentry) {
    set_start_end_spins(mainw->current_file);

    if (LIVES_IS_INTERACTIVE) {
      lives_widget_set_sensitive(mainw->spinbutton_start, TRUE);
      lives_widget_set_sensitive(mainw->spinbutton_end, TRUE);
    }

    if (mainw->play_window && (mainw->prv_link == PRV_START || mainw->prv_link == PRV_END)) {
      // unblock spinbutton in play window
      lives_widget_set_sensitive(mainw->preview_spinbutton, TRUE);
    }
  }

  // clips menu
  for (i = 1; i < MAX_FILES; i++) {
    if (mainw->files[i]) {
      if (mainw->files[i]->menuentry) {
        lives_widget_set_sensitive(mainw->files[i]->menuentry, TRUE);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
  if (prefs->vj_mode) {
#ifdef LIBAV_TRANSCODE
    lives_widget_set_sensitive(mainw->transcode, FALSE);
#endif
    lives_widget_set_sensitive(mainw->import_theme, FALSE);
    lives_widget_set_sensitive(mainw->export_theme, FALSE);
    lives_widget_set_sensitive(mainw->backup, FALSE);
    lives_widget_set_sensitive(mainw->capture, FALSE);
    lives_widget_set_sensitive(mainw->save_as, FALSE);
    lives_widget_set_sensitive(mainw->mt_menu, FALSE);
    lives_widget_set_sensitive(mainw->gens_submenu, FALSE);
    lives_widget_set_sensitive(mainw->utilities_submenu, FALSE);
  }
#ifdef ENABLE_JACK_TRANSPORT
  if (mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      && (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE))
    jack_transport_make_strict_slave(mainw->jackd_trans, TRUE);
#endif
}


void desensitize(void) {
  // desensitize the main window when we are playing/processing a clip
  int i;

  if (mainw->multitrack) {
    mt_desensitise(mainw->multitrack);
    return;
  }

  mainw->sense_state &= LIVES_SENSE_STATE_INTERACTIVE;
  mainw->sense_state |= LIVES_SENSE_STATE_INSENSITIZED;

  //lives_widget_set_sensitive (mainw->open, mainw->playing_file>-1);
  lives_widget_set_sensitive(mainw->open, FALSE);
  lives_widget_set_sensitive(mainw->open_sel, FALSE);
  lives_widget_set_sensitive(mainw->open_vcd_menu, FALSE);
#ifdef HAVE_WEBM
  lives_widget_set_sensitive(mainw->open_loc_menu, FALSE);
#else
  lives_widget_set_sensitive(mainw->open_loc, FALSE);
#endif
  lives_widget_set_sensitive(mainw->open_device_menu, FALSE);
#ifdef HAVE_YUV4MPEG
  lives_widget_set_sensitive(mainw->open_yuv4m, FALSE);
#endif

#ifdef HAVE_LDVGRAB
  lives_widget_set_sensitive(mainw->firewire, FALSE);
  lives_widget_set_sensitive(mainw->tvdev, FALSE);
#endif
  lives_widget_set_sensitive(mainw->recent_menu, FALSE);
  lives_widget_set_sensitive(mainw->restore, FALSE);
  lives_widget_set_sensitive(mainw->clear_ds, FALSE);
  lives_widget_set_sensitive(mainw->midi_learn, FALSE);
  lives_widget_set_sensitive(mainw->midi_save, FALSE);
  lives_widget_set_sensitive(mainw->load_cdtrack, FALSE);
  lives_widget_set_sensitive(mainw->save_as, FALSE);
#ifdef LIBAV_TRANSCODE
  lives_widget_set_sensitive(mainw->transcode, FALSE);
#endif
  lives_widget_set_sensitive(mainw->backup, FALSE);
  lives_widget_set_sensitive(mainw->playsel, FALSE);
  lives_widget_set_sensitive(mainw->playclip, FALSE);
  lives_widget_set_sensitive(mainw->copy, FALSE);
  lives_widget_set_sensitive(mainw->cut, FALSE);
  lives_widget_set_sensitive(mainw->preferences, FALSE);
  lives_widget_set_sensitive(mainw->rev_clipboard, FALSE);
  lives_widget_set_sensitive(mainw->insert, FALSE);
  lives_widget_set_sensitive(mainw->merge, FALSE);
  lives_widget_set_sensitive(mainw->xdelete, FALSE);
  lives_widget_set_sensitive(mainw->trim_video, FALSE);
  if (!prefs->pause_during_pb) {
    lives_widget_set_sensitive(mainw->playall, FALSE);
  }
  lives_widget_set_sensitive(mainw->rewind, FALSE);
  if (!prefs->vj_mode && mainw->rfx_loaded && !mainw->foreign) {
    for (i = 0; i <= mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom +
         mainw->num_rendered_effects_test; i++) {
      if (i == mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate) continue;
      //if (mainw->rendered_fx[i]->props & RFX_PROPS_MAY_RESIZE) continue;
      if (mainw->rendered_fx[i]->num_in_channels == 2) continue;
      if (mainw->rendered_fx[i]->menuitem && mainw->rendered_fx[i]->min_frames >= 0)
        lives_widget_set_sensitive(mainw->rendered_fx[i]->menuitem, FALSE);
    }
  }

  if (mainw->resize_menuitem) {
    lives_widget_set_sensitive(mainw->resize_menuitem, FALSE);
  }

  if (!prefs->vj_mode && !mainw->foreign) {
    if (mainw->num_rendered_effects_test > 0) {
      lives_widget_set_sensitive(mainw->run_test_rfx_submenu, FALSE);
    }
    if (mainw->has_custom_gens) {
      lives_widget_set_sensitive(mainw->custom_gens_submenu, FALSE);
    }

    if (mainw->has_custom_tools) {
      lives_widget_set_sensitive(mainw->custom_tools_submenu, FALSE);
    }

    if (mainw->has_custom_effects) {
      lives_widget_set_sensitive(mainw->custom_effects_submenu, FALSE);
    }
  }

  if (mainw->rendered_fx && mainw->rendered_fx[0] && mainw->rendered_fx[0]->menuitem)
    lives_widget_set_sensitive(mainw->rendered_fx[0]->menuitem, FALSE);
  lives_widget_set_sensitive(mainw->cg_managegroups, FALSE);
  lives_widget_set_sensitive(mainw->export_submenu, FALSE);
  lives_widget_set_sensitive(mainw->recaudio_submenu, FALSE);
  lives_widget_set_sensitive(mainw->append_audio, FALSE);
  lives_widget_set_sensitive(mainw->trim_submenu, FALSE);
  lives_widget_set_sensitive(mainw->delaudio_submenu, FALSE);
  lives_widget_set_sensitive(mainw->troubleshoot, FALSE);
  lives_widget_set_sensitive(mainw->expl_missing, FALSE);
  lives_widget_set_sensitive(mainw->resample_audio, FALSE);
  lives_widget_set_sensitive(mainw->voladj, FALSE);
  lives_widget_set_sensitive(mainw->fade_aud_in, FALSE);
  lives_widget_set_sensitive(mainw->fade_aud_out, FALSE);
  lives_widget_set_sensitive(mainw->normalize_audio, FALSE);
  lives_widget_set_sensitive(mainw->ins_silence, FALSE);
  lives_widget_set_sensitive(mainw->loop_video, is_realtime_aplayer(prefs->audio_player));
  if (!is_realtime_aplayer(prefs->audio_player)) lives_widget_set_sensitive(mainw->mute_audio, FALSE);
  lives_widget_set_sensitive(mainw->load_audio, FALSE);
  lives_widget_set_sensitive(mainw->load_subs, FALSE);
  lives_widget_set_sensitive(mainw->erase_subs, FALSE);
  lives_widget_set_sensitive(mainw->save_selection, FALSE);
  lives_widget_set_sensitive(mainw->close, FALSE);
  lives_widget_set_sensitive(mainw->change_speed, FALSE);
  lives_widget_set_sensitive(mainw->resample_video, FALSE);
  lives_widget_set_sensitive(mainw->undo, FALSE);
  lives_widget_set_sensitive(mainw->redo, FALSE);
  lives_widget_set_sensitive(mainw->paste_as_new, FALSE);
  lives_widget_set_sensitive(mainw->capture, FALSE);
  //lives_widget_set_sensitive(mainw->toy_tv, FALSE);
  lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
  lives_widget_set_sensitive(mainw->vj_load_set, FALSE);
  lives_widget_set_sensitive(mainw->vj_realize, FALSE);
  lives_widget_set_sensitive(mainw->vj_reset, FALSE);
  lives_widget_set_sensitive(mainw->show_quota, FALSE);
  lives_widget_set_sensitive(mainw->export_proj, FALSE);
  lives_widget_set_sensitive(mainw->import_proj, FALSE);
  lives_widget_set_sensitive(mainw->recaudio_sel, FALSE);
  lives_widget_set_sensitive(mainw->mt_menu, FALSE);

  lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
  lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);

  if (mainw->current_file >= 0 && (!LIVES_IS_PLAYING || mainw->foreign)) {
    //  if (!cfile->opening||mainw->dvgrab_preview||mainw->preview||cfile->opening_only_audio) {
    // disable the 'clips' menu entries
    for (i = 1; i < MAX_FILES; i++) {
      if (mainw->files[i]) {
        if (mainw->files[i]->menuentry) {
          if (i != mainw->current_file) {
            lives_widget_set_sensitive(mainw->files[i]->menuentry, FALSE);
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*
}


void procw_desensitize(void) {
  // switch on/off a few extra widgets in the processing dialog

  int current_file;

  if (mainw->multitrack) return;

  mainw->sense_state &= LIVES_SENSE_STATE_INTERACTIVE;
  mainw->sense_state |= LIVES_SENSE_STATE_PROC_INSENSITIZED | LIVES_SENSE_STATE_INSENSITIZED;

  if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID
      && (cfile->menuentry || cfile->opening) && !mainw->preview) {
    // an effect etc,
    lives_widget_set_sensitive(mainw->loop_video, CURRENT_CLIP_HAS_AUDIO && CURRENT_CLIP_HAS_VIDEO);
    lives_widget_set_sensitive(mainw->loop_continue, TRUE);

    if (CURRENT_CLIP_HAS_AUDIO && CURRENT_CLIP_HAS_VIDEO) {
      mainw->loop = lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video));
    }
    if (CURRENT_CLIP_HAS_AUDIO && CURRENT_CLIP_HAS_VIDEO) {
      mainw->mute = lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio));
    }
  }
  if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && !cfile->menuentry) {
    lives_widget_set_sensitive(mainw->rename, FALSE);
    if (cfile->opening || cfile->restoring) {
      // loading, restoring etc
      lives_widget_set_sensitive(mainw->lock_selwidth, FALSE);
      lives_widget_set_sensitive(mainw->show_file_comments, FALSE);
      if (!cfile->opening_only_audio) {
        lives_widget_set_sensitive(mainw->toy_random_frames, FALSE);
      }
    }
  }

  current_file = mainw->current_file;
  if (CURRENT_CLIP_IS_VALID && cfile->cb_src != -1) mainw->current_file = cfile->cb_src;

  if (CURRENT_CLIP_IS_VALID) {
    // stop the start and end from being changed
    // better to clamp the range than make insensitive, this way we stop
    // other widgets (like the video bar) updating it
    lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end, cfile->end);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end);
    lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);
    lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start, cfile->start);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start);
    lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
  }

  mainw->current_file = current_file;

  if (mainw->play_window && (mainw->prv_link == PRV_START || mainw->prv_link == PRV_END)) {
    // block spinbutton in play window
    lives_widget_set_sensitive(mainw->preview_spinbutton, FALSE);
  }

  lives_widget_set_sensitive(mainw->sa_button, FALSE);
  lives_widget_set_sensitive(mainw->select_submenu, FALSE);
  lives_widget_set_sensitive(mainw->gens_submenu, FALSE);
  lives_widget_set_sensitive(mainw->autolives, FALSE);
  lives_widget_set_sensitive(mainw->export_submenu, FALSE);
  lives_widget_set_sensitive(mainw->trim_submenu, FALSE);
  lives_widget_set_sensitive(mainw->delaudio_submenu, FALSE);
  lives_widget_set_sensitive(mainw->load_cdtrack, FALSE);
  lives_widget_set_sensitive(mainw->open_lives2lives, FALSE);
  lives_widget_set_sensitive(mainw->record_perf, FALSE);
  lives_widget_set_sensitive(mainw->unicap, FALSE);

  if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->nopreview) {
    lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
    if (mainw->preview_box) lives_widget_set_sensitive(mainw->p_playbutton, FALSE);
    lives_widget_set_sensitive(mainw->m_playselbutton, FALSE);
    if (mainw->preview_box) lives_widget_set_sensitive(mainw->p_playselbutton, FALSE);
  }
}


// drae directly in interface ////

void set_drawing_area_from_pixbuf(LiVESDrawingArea * da, LiVESPixbuf * pixbuf) {
  struct pbs_struct *pbs;
  pthread_mutex_t *mutex;
  LiVESWidget *widget;
  lives_painter_surface_t **psurface, *surface;
  lives_painter_t *cr;
  int rwidth, rheight, width, height, cx, cy;
  int owidth, oheight, oxrwidth, oxrheight;
  int dx = 0, dy = 0, dw = 0, dh = 0;

  if (!da) return;
  widget = LIVES_WIDGET(da);

  rwidth = lives_widget_get_allocation_width(widget);
  rheight = lives_widget_get_allocation_height(widget);

  if (!rwidth || !rheight) return;

  mutex = lives_widget_get_mutex(widget);
  pthread_mutex_lock(mutex);

  pbs = (struct pbs_struct *)GET_VOIDP_DATA(widget, PBS_KEY);
  if (pbs) {
    psurface = pbs->surfp;
    if (psurface) surface = *psurface;
    else surface = NULL;
  }
  if (!pbs || !psurface || !surface) {
    pthread_mutex_unlock(mutex);
    return;
  }

  cr = live_widget_begin_paint(widget);
  surface = *psurface = lives_painter_get_target(cr);
  lives_painter_surface_reference(surface);

  if (!cr) {
    pthread_mutex_unlock(mutex);
    return;
  }

  rwidth = (rwidth >> 1) << 1;
  rheight = (rheight >> 1) << 1;

  lives_painter_surface_flush(surface);

  if (pixbuf) {
    width = lives_pixbuf_get_width(pixbuf);
    height = lives_pixbuf_get_height(pixbuf);

    cx = (rwidth - width) >> 1;
    if (cx < 0) cx = 0;
    cy = (rheight - height) >> 1;
    if (cy < 0) cy = 0;

    if (widget == mainw->start_image || widget == mainw->end_image
        || (mainw->multitrack && widget == mainw->play_image)
        || (widget == mainw->play_window && !mainw->fs)
       ) {
      int xrwidth, xrheight;
      LiVESWidget *p = widget;

      if (mainw->multitrack || (!mainw->multitrack && widget != mainw->preview_image))
        p = lives_widget_get_parent(widget);

      xrwidth = lives_widget_get_allocation_width(p);
      xrheight = lives_widget_get_allocation_height(p);

      owidth = GET_INT_DATA(da, OWIDTH_KEY);
      oheight = GET_INT_DATA(da, OHEIGHT_KEY);
      oxrwidth = GET_INT_DATA(da, OXRWIDTH_KEY);
      oxrheight = GET_INT_DATA(da, OXRHEIGHT_KEY);

      if (width < owidth || height < oheight || xrwidth > oxrwidth || xrheight > oxrheight) {
        lives_painter_render_background(p, cr, 0., 0., xrwidth, xrheight);
        dw = xrwidth;
        dh = xrheight;
      }

      SET_INT_DATA(da, OWIDTH_KEY, owidth);
      SET_INT_DATA(da, OHEIGHT_KEY, oheight);
      SET_INT_DATA(da, OXRWIDTH_KEY, oxrwidth);
      SET_INT_DATA(da, OXRHEIGHT_KEY, oxrheight);

      if (mainw->multitrack) {
        rwidth = xrwidth;
        rheight = xrheight;
      }

      /*   if ((mainw->multitrack && widget == mainw->preview_image && prefs->letterbox_mt) */
      /*       || (!mainw->multitrack && (prefs->ce_maxspect || prefs->letterbox))) { */
      /*     calc_maxspect(rwidth, rheight, &width, &height); */

      /*     width = (width >> 1) << 1; */
      /*     height = (height >> 1) << 1; */

      /*     if (width > owidth && height > oheight) { */
      /*       width = owidth; */
      /*       height = oheight; */
      /*     } */
      /*   } */

      /*   if (mainw->multitrack) { */
      /*     cx = (rwidth - width) >> 1; */
      /*     if (cx < 0) cx = 0; */
      /*     cy = (rheight - height) >> 1; */
      /*     if (cy < 0) cy = 0; */
      /*   } */
      /* } */
    }
    lives_widget_set_opacity(widget, 1.);

    if ((!mainw->multitrack || widget != mainw->play_image) && widget != mainw->preview_image) {
      if (!LIVES_IS_PLAYING && mainw->multitrack && widget == mainw->play_image) clear_widget_bg(widget, surface);
      if (prefs->funky_widgets) {
        lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->frame_surround);
        if (widget == mainw->start_image || widget == mainw->end_image || widget == mainw->play_image) {
          lives_painter_move_to(cr, 0, 0);
          lives_painter_line_to(cr, rwidth, 0);
          lives_painter_move_to(cr, 0, rheight);
          lives_painter_line_to(cr, rwidth, rheight);
          dw = rwidth;
          dh = rheight;
        } else {
          lives_painter_rectangle(cr, cx - 1, cy - 1, width + 2, height + 2);
          if (!dw) {
            dw = width + 2;
            dh = height + 2;
            dx = cx - 1;
            dy = cy - 1;
          }
        }
        // frame
        lives_painter_stroke(cr);
        cx += 2;
        cy += 4;
      }
    }

    /// x, y values are offset of top / left of image in drawing area
    /* if (!mainw->multitrack && (widget == mainw->preview_image && LIVES_IS_PLAYING)) */
    /*   lives_painter_set_source_pixbuf(cr, pixbuf, 0, 0); */
    /* else */
    lives_painter_set_source_pixbuf(cr, pixbuf, cx, cy);
    lives_painter_rectangle(cr, cx, cy, rwidth - cx * 2, rheight + 2 - cy * 2);
    if (dw < rwidth - cx * 2) dw = rwidth - cx * 2;
    if (dh < rheight - cy * 2) dh = rheight - cy * 2;
    if (dx > cx) dx = cx;
    if (dy > cy) dy = cy;
  } else {
    lives_widget_set_opacity(widget, 0.);
    clear_widget_bg(widget, surface);
    dw = rwidth;
    dh = rheight;
  }

  lives_painter_surface_mark_dirty_rectangle(surface, dx, dy, dw, dh);

  lives_painter_paint(cr);

  lives_widget_end_paint(widget);

  pthread_mutex_unlock(mutex);
}


void lives_layer_draw(LiVESDrawingArea * darea, weed_layer_t *layer) {

  LiVESPixbuf *pixbuf;

  if (!LIVES_IS_DRAWING_AREA(darea)) return;

  //mainw->debug_ptr = NULL;
  if (!weed_layer_check_valid(layer)) return;

  pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

  if (pixbuf) {
    set_drawing_area_from_pixbuf(darea, pixbuf);
    lives_widget_object_unref(pixbuf);
  }

  /* g_print("unref %p, nrefs is %d\n", layer, weed_layer_count_refs(layer)); */
  /* g_print("if we free %p, here, it should remove proxy layer, unless it entered with copylist\n", layer); */
  weed_layer_unref(layer);
}


// UI sizing functions ////
LIVES_GLOBAL_INLINE boolean get_play_screen_size(int *opwidth, int *opheight) {
  // get the size of the screen / player in fullscreen / sepwin mode
  // returns TRUE if we span multiple monitors, FALSE for single monitor mode

  if (prefs->play_monitor == 0) {
    if (capable->nmonitors > 1) {
      // spread over all monitors
#if !GTK_CHECK_VERSION(3, 22, 0)
      *opwidth = lives_screen_get_width(mainw->mgeom[0].screen);
      *opheight = lives_screen_get_height(mainw->mgeom[0].screen);
#else
      /// TODO: no doubt this is wrong and should be done taking into account vertical monitor layouts as well
      *opheight = mainw->mgeom[0].height;
      *opwidth = 0;
      for (int i = 0; i < capable->nmonitors; i++) {
        *opwidth += mainw->mgeom[i].width;
      }
#endif
      return TRUE;
    } else {
      // but we only have one...
      *opwidth = mainw->mgeom[0].phys_width;
      *opheight = mainw->mgeom[0].phys_height;
    }
  } else {
    // single monitor
    *opwidth = mainw->mgeom[prefs->play_monitor - 1].phys_width;
    *opheight = mainw->mgeom[prefs->play_monitor - 1].phys_height;
  }
  return FALSE;
}


void get_player_size(int *opwidth, int *opheight) {
  // calc output size for display

  ///// external playback plugin
  if (LIVES_IS_PLAYING && mainw->fs && mainw->play_window) {
    // playback plugin (therefore fullscreen / separate window)
    if (mainw->vpp) {
      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
	if (mainw->vpp->capabilities & VPP_CAN_RESIZE) {
	  // plugin can resize, max is the screen size
	  get_play_screen_size(opwidth, opheight);
	} else {
	  // ext plugin can't resize, use its fixed size
	  *opwidth = mainw->vpp->fwidth;
	  *opheight = mainw->vpp->fheight;
	}
      } else {
	// remote display
	if (!(mainw->vpp->capabilities & VPP_CAN_RESIZE)) {
	  // cant resize, we use the width it gave us if it can't resize
	  *opwidth = mainw->vpp->fwidth;
	  *opheight = mainw->vpp->fheight;
	} else {
	  // else the clip size
	  *opwidth = cfile->hsize;
	  *opheight = cfile->vsize;
	}
      }
      goto align;
    }
  }

  if (lives_get_status() != LIVES_STATUS_RENDERING && mainw->play_window
      && LIVES_IS_WIDGET(mainw->preview_image)) {
    int rwidth, rheight;

    // playback in separate window
    // use values set in resize_play_window

    /* *opwidth = rwidth = lives_widget_get_allocation_width(mainw->preview_image);// - H_RESIZE_ADJUST; */
    /* *opheight = rheight = lives_widget_get_allocation_height(mainw->preview_image);// - V_RESIZE_ADJUST; */

    *opwidth = rwidth =  mainw->pwidth;
    *opheight = rheight = mainw->pheight;

    if (mainw->multitrack && prefs->letterbox_mt) {
      rwidth = *opwidth;
      rheight = *opheight;
      *opwidth = cfile->hsize;
      *opheight = cfile->vsize;
      calc_maxspect(rwidth, rheight, opwidth, opheight);
    }
    goto align;
  }

  /////////////////////////////////////////////////////////////////////////////////
  // multitrack: we ignore double size, and fullscreen unless playing in the separate window
  if (mainw->multitrack) {
    *opwidth = mainw->multitrack->play_width;
    *opheight = mainw->multitrack->play_height;
    goto align;
  }

  ////////////////////////////////////////////////////////////////////////////////////
  // clip edit mode
  if (lives_get_status() == LIVES_STATUS_RENDERING) {
    *opwidth = cfile->hsize;
    *opheight = cfile->vsize;
    *opwidth = (*opwidth >> 2) << 2;
    *opheight = (*opheight >> 1) << 1;
    mainw->pwidth = *opwidth;
    mainw->pheight = *opheight;
    return;
  }

  if (!mainw->fs) {
    // embedded player
    *opwidth = mainw->pwidth;
    *opheight = mainw->pheight;

    *opwidth = lives_widget_get_allocation_width(mainw->play_image);// - H_RESIZE_ADJUST;
    *opheight = lives_widget_get_allocation_height(mainw->play_image);// - V_RESIZE_ADJUST;
  }
  if (*opwidth * *opheight < 16) {
    *opwidth = mainw->ce_frame_width;
    *opheight = mainw->ce_frame_height;
  }

align:
  *opwidth = (*opwidth >> 3) << 3;
  *opheight = (*opheight >> 1) << 1;
  mainw->pwidth = *opwidth;
  mainw->pheight = *opheight;
}


void reset_mainwin_size(void) {
  int w, h, x, y, bx, by;
  int scr_width = GUI_SCREEN_WIDTH;
  int scr_height = GUI_SCREEN_HEIGHT;

  if (!prefs->show_gui) return;

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
  bx = 2 * abs(bx);
  by = abs(by);
  bx = by = 0;
  if (!mainw->hdrbar) {
    if (prefs->open_maximised && by > MENU_HIDE_LIM) {
      BREAK_ME("mabr hide 1");
      lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }
  }

  w = lives_widget_get_allocation_width(LIVES_MAIN_WINDOW_WIDGET);
  h = lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET);

  // resize the main window so it fits the gui monitor
  if (prefs->open_maximised) {
    lives_widget_set_maximum_size(LIVES_MAIN_WINDOW_WIDGET, scr_width - bx, scr_height - by);
    //lives_widget_set_minimum_size(LIVES_MAIN_WINDOW_WIDGET, scr_width - bx, scr_height - by);
    lives_window_set_default_size(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), scr_width - bx, scr_height - by);
    lives_window_unmaximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));

    lives_window_move(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), 0, by);
    lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), scr_width - bx, scr_height - by);
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  } else {
    if (w > scr_width - bx || h > scr_height - by) {
      w = scr_width - bx;
      h = scr_height - by;
    }
    lives_window_set_default_size(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);
    lives_widget_set_maximum_size(LIVES_MAIN_WINDOW_WIDGET, scr_width - bx, scr_height - by);
    lives_window_get_position(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), &x, &y);
    if (x + w > scr_width - bx) x = scr_width - bx - w;
    if (y + h > scr_height - by) y = scr_height - by - h;
    lives_window_unmaximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    lives_window_move(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), x, y);
    lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);
  }

  if (!LIVES_IS_PLAYING) {
    mainw->gui_much_events = TRUE;
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
  }
}


LIVES_GLOBAL_INLINE int get_sepbar_height(void) {
  static int vspace = -1;
  if (vspace == -1) {
    LiVESPixbuf *sepbuf = lives_image_get_pixbuf(LIVES_IMAGE(mainw->sep_image));
    vspace = (sepbuf ? (((lives_pixbuf_get_height(sepbuf) + 1) >> 1) << 1) : 0);
  }
  return vspace;
}


void get_gui_framesize(int *hsize, int *vsize) {
  int bx, by;
  int scr_width = GUI_SCREEN_WIDTH;
  int scr_height = GUI_SCREEN_PHYS_HEIGHT;
  int vspace = get_sepbar_height();

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
  bx = abs(bx);
  by = abs(by);
  by = 0;

  if (!mainw->hdrbar && by > MENU_HIDE_LIM) {
    BREAK_ME("mabr hide 2");
    lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
  }
  if (hsize) *hsize = (((scr_width - H_RESIZE_ADJUST * 3 + bx) / 3) >> 1) << 1;
  if (vsize) *vsize = ((int)(scr_height - ((CE_TIMELINE_VSPACE * 1.01 + widget_opts.border_width * 2)
                               / sqrt(widget_opts.scaleH) + by
					   + (prefs->show_msg_area ? mainw->mbar_res : 0))) >> 1) << 1;
}


boolean check_can_show_msg_area(void) {
  int by, vspace = get_sepbar_height();

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, NULL, &by);
  by = abs(by);

  if (prefs->show_dev_opts) g_print("Can show msg_area if %d >= %d - calculated as %d + %d + %f; result is ",
                                      GUI_SCREEN_HEIGHT, (int)MIN_MSGAREA_SCRNHEIGHT,
                                      (int)(GUI_SCREEN_HEIGHT - ((CE_TIMELINE_VSPACE * 1.01 + widget_opts.border_width * 2)
                                            / sqrt(widget_opts.scaleH) + vspace + by)),
                                      CE_TIMELINE_VSPACE, MIN_MSGBAR_HEIGHT);

  if (!prefs->vj_mode && GUI_SCREEN_HEIGHT > (int)MIN_MSGAREA_SCRNHEIGHT) {
    capable->can_show_msg_area = TRUE;
    if (future_prefs->show_msg_area) prefs->show_msg_area = TRUE;
    if (prefs->show_dev_opts) g_print("YES\n");
  } else {
    prefs->show_msg_area = capable->can_show_msg_area = FALSE;
    if (prefs->show_dev_opts) g_print("NO\n");
  }
  return capable->can_show_msg_area;
}


static void _resize(double scale) {
  // resize the frame widgets
  // set scale < 0. to _force_ the playback frame to expand (for external capture)
  LiVESXWindow *xwin;
  double oscale = scale;
  int hsize, vsize, xsize;
  int bx, by;
  int scr_width = GUI_SCREEN_WIDTH;
  int scr_height = GUI_SCREEN_PHYS_HEIGHT;

  if (!prefs->show_gui || mainw->multitrack) return;

  //if (!mainw->go_away)
  //reset_mainwin_size();

  get_gui_framesize(&hsize, &vsize);

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
  bx = 2 * abs(bx);
  by = abs(by);
  bx = by = 0;

  if (scale < 0.) {
    // foreign capture
    scale = -scale;
    hsize = (scr_width - H_RESIZE_ADJUST - bx) / scale;
    vsize = (scr_height - V_RESIZE_ADJUST - by) / scale;
  }

  mainw->ce_frame_width = hsize;
  mainw->ce_frame_height = vsize;

  if (oscale > 0.) {
    if (hsize * scale > scr_width - SCR_WIDTH_SAFETY)
      scale = (scr_width - SCR_WIDTH_SAFETY) / hsize;

    if (vsize * scale > scr_height - SCR_HEIGHT_SAFETY)
      scale = (scr_height - SCR_HEIGHT_SAFETY) / vsize;

    if (scale > 1.) {
      // this is the size for the start and end frames
      // they shrink when scale is 2.0
      mainw->ce_frame_width = hsize / scale;
      mainw->ce_frame_height = vsize / scale + V_RESIZE_ADJUST;

      lives_widget_set_margin_left(mainw->playframe, widget_opts.packing_width);
      lives_widget_set_margin_right(mainw->playframe, widget_opts.packing_width);
    }

    if (CURRENT_CLIP_IS_VALID) {
      if (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV) {
        if (!mainw->camframe) {
          LiVESError *error = NULL;
          char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
          char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
          mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
          if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
          lives_free(tmp); lives_free(fname);
        }
      }
    }

    // THE SIZES OF THE FRAME CONTAINERS
    lives_widget_set_size_request(mainw->pf_grid, -1, mainw->ce_frame_height);
    lives_widget_set_size_request(mainw->frame1, mainw->ce_frame_width, vsize);
    lives_widget_set_size_request(mainw->eventbox3, mainw->ce_frame_width, mainw->ce_frame_height);
    lives_widget_set_size_request(mainw->frame2, mainw->ce_frame_width, vsize);
    lives_widget_set_size_request(mainw->eventbox4, mainw->ce_frame_width, vsize);

    lives_widget_set_size_request(mainw->start_image, mainw->ce_frame_width, vsize);
    lives_widget_set_size_request(mainw->end_image, mainw->ce_frame_width, vsize);

    // use unscaled size in dblsize
    if (scale > 1.) {
      hsize *= scale;
      vsize *= scale;
      /* lives_widget_set_size_request(mainw->playframe, hsize, vsize); */
      /* lives_widget_set_size_request(mainw->pl_eventbox, hsize, vsize); */
    }

    // IMPORTANT (or the entire image will not be shown)
    lives_widget_set_size_request(mainw->play_image, hsize, vsize);
    lives_widget_set_size_request(mainw->playarea, hsize, vsize);
    lives_widget_set_size_request(mainw->playframe, hsize, vsize);
    lives_widget_set_size_request(mainw->pl_eventbox, hsize, vsize);

    xwin = lives_widget_get_xwindow(mainw->play_image);
    if (LIVES_IS_XWINDOW(xwin)) {
      if (hsize != lives_painter_image_surface_get_width(mainw->play_surface)
          || vsize != lives_painter_image_surface_get_height(mainw->play_surface)) {
        pthread_mutex_lock(&mainw->play_surface_mutex);
        if (mainw->play_surface) lives_painter_surface_destroy(mainw->play_surface);
        mainw->play_surface =
          lives_xwindow_create_similar_surface(xwin, LIVES_PAINTER_CONTENT_COLOR,
                                               hsize, vsize);
        clear_widget_bg(mainw->play_image, mainw->play_surface);
        pthread_mutex_unlock(&mainw->play_surface_mutex);
      }
    }
  } else {
    // capture window size
    xsize = (scr_width - hsize * -oscale - H_RESIZE_ADJUST) / 2;
    if (xsize > 0) {
      lives_widget_set_size_request(mainw->frame1, xsize / scale, vsize + V_RESIZE_ADJUST);
      lives_widget_set_size_request(mainw->eventbox3, xsize / scale, vsize + V_RESIZE_ADJUST);
      lives_widget_set_size_request(mainw->frame2, xsize / scale, vsize + V_RESIZE_ADJUST);
      lives_widget_set_size_request(mainw->eventbox4, xsize / scale, vsize + V_RESIZE_ADJUST);
      mainw->ce_frame_width = xsize / scale;
      mainw->ce_frame_height = vsize + V_RESIZE_ADJUST;
    } else {
      lives_widget_hide(mainw->frame1);
      lives_widget_hide(mainw->frame2);
      lives_widget_hide(mainw->eventbox3);
      lives_widget_hide(mainw->eventbox4);
    }
  }

  if (!mainw->foreign && mainw->current_file == -1) {
    lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);
    load_start_image(0);
    load_end_image(0);
  }

  if (scale != oscale) reset_mainwin_size();
}


void resize(double scale) {
  if (is_fg_thread()) _resize(scale);
  else {
    BG_THREADVAR(hook_hints) = HOOK_CB_BLOCK | HOOK_CB_PRIORITY;
    main_thread_execute_rvoid(_resize, 0, "d", scale);
    BG_THREADVAR(hook_hints) = 0;
  }
}

/////////////// message area //

// when doing things which may chenge widget sizes withint the CE GUI,
// reset_message_area should be called first, this will cause the main window height to shrink
// and allows the other widgets to shrink or expand
// then resize_message_area should be added as an idlefunc. The size and position of the main window will be reset
// and the message area will resize itself to occupy the free vertical space. After doing so we check main window size,
// to see if has the requested size. If not then we repeat the process, set the height to 1, resize the main window
// add the message area with adjusted height, until the main window keeps its correct size.

// NOTE:
// This is the only way in GTK+. You cannot ask a window to shrink or grow to a pecific by auto adjusting the size of a child widget.
// All you can do is shrink or grow the child widget and reshow the parent. The parent will then resize itself to contain the child widgets.
// It is not always posible to determine in advance the size of the child widfet which will result in the parent ending up with the target size.
// This is partly due to the fact that other widgets inside the parent may also resize themselves according to the child widget being adjusted.
// Thus this has to be done iteratively, using heuristics to avoid falling into loops.
//
// It would be nice if gtk+ / gdk would provide some simple way to do this - provide a target size for a parent window, and a pointer to an "adjustable"
// widget, and have gtk+ automatically shrink or expand the adjustable widget so that the parent window ended up with the target size.
// This would be especially useful when you want the parent window to have the display size, no more and no less.
// In some cases you can maximize the window, but this only works if the window size is SMALLER than the screen size.

void reset_message_area(void) {
  if (!prefs->show_msg_area || mainw->multitrack) return;
  if (!mainw->is_ready || !prefs->show_gui) return;
  // need to shrink the message_box then re-expand it after redrawing the widgets
  // otherwise the main window can expand beyond the bottom of the screen
  lives_widget_set_size_request(mainw->message_box, 1, 1);
  lives_widget_set_size_request(mainw->msg_area, 1, 1);

  // hide this and show it last, because being narrow, it can "poke up" into the area above
  // and mess up the size calculations
  if (mainw->msg_scrollbar) lives_widget_hide(mainw->msg_scrollbar);
}


boolean resize_message_area(livespointer data) {
  static boolean isfirst = TRUE;
  RECURSE_GUARD_START;

  if (!prefs->show_gui || LIVES_IS_PLAYING || mainw->is_processing || mainw->is_rendering || !prefs->show_msg_area) {
    mainw->assumed_height = mainw->assumed_width = -1;
    mainw->idlemax = 0;
    return FALSE;
  }

  RECURSE_GUARD_ARM;

  if (mainw->idlemax-- == DEF_IDLE_MAX) mainw->msg_area_configed = FALSE;

  if (mainw->msg_area_configed) mainw->idlemax = 0;

  if (mainw->idlemax > 0 && mainw->assumed_height != -1 &&
      mainw->assumed_height != lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET)) return TRUE;
  if (mainw->idlemax > 0 && lives_widget_get_allocation_height(mainw->end_image) != mainw->ce_frame_height) return TRUE;

  mainw->idlemax = 0;

  mainw->assumed_height = mainw->assumed_width = -1;

  msg_area_scroll(LIVES_ADJUSTMENT(mainw->msg_adj), mainw->msg_area);
  msg_area_config(mainw->msg_area);

  if (isfirst) {
    resize(1.);
    if (!CURRENT_CLIP_IS_VALID) {
      d_print("");
    }
    if (mainw->msg_scrollbar) lives_widget_show(mainw->msg_scrollbar);
    msg_area_scroll_to_end(mainw->msg_area, mainw->msg_adj);
    lives_widget_queue_draw_if_visible(mainw->msg_area);
    isfirst = FALSE;
  }

  resize(1.);
  if (mainw->msg_scrollbar) lives_widget_show(mainw->msg_scrollbar);
  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);

  RECURSE_GUARD_END;

  return FALSE;
}


//////////////////// TIMELINE /////////

static void redraw_timeline_inner(int clipno);


// mainw->drawsrc :: cureent clip being drawn / last drawn

static lives_proc_thread_t drawtl_thread = NULL;
static pthread_mutex_t tlthread_mutex = PTHREAD_MUTEX_INITIALIZER;


void drawtl_cancel(void) {
  // must call unlock_timline()
  pthread_mutex_lock(&tlthread_mutex);
  if (drawtl_thread) {
    lives_proc_thread_t lpt = drawtl_thread;
    lives_proc_thread_request_cancel(lpt, FALSE);
    lives_proc_thread_try_interrupt(lpt);
    pthread_mutex_unlock(&tlthread_mutex);
    lives_proc_thread_join(lpt);
    pthread_mutex_lock(&tlthread_mutex);
    lives_proc_thread_unref(STEAL_POINTER(drawtl_thread));
  }
  // exit with tlthread_mutex locked !!
}


void redraw_timeline(int clipno) {
  // function to redraw the timeline (video and audio bars)
  // this is run in a bg thread
  // at any time it can be called again and by any thread, - e.g when current clip change,
  // when audio length vchanges
  //
  // if the function is already active then the current thread is cancelled and the new one
  // activated
  // because thread does many

  //

  mainw->drawsrc = clipno;

  drawtl_thread = lives_proc_thread_create(LIVES_THRDATTR_SET_CANCELLABLE,
                  (lives_funcptr_t)redraw_timeline_inner, -1, "i", clipno);
  pthread_mutex_unlock(&tlthread_mutex);
}

boolean get_timeline_lock(void) {
  if (!pthread_mutex_trylock(&tlthread_mutex)) {
    if (drawtl_thread) {
      lives_proc_thread_t lpt = drawtl_thread;
      if (lives_proc_thread_check_finished(lpt)
          && !lives_proc_thread_should_cancel(lpt)) {
        pthread_mutex_unlock(&tlthread_mutex);
        lives_proc_thread_join(lpt);
        pthread_mutex_lock(&tlthread_mutex);
        lives_proc_thread_unref(lpt);
        drawtl_thread = NULL;
        return TRUE;
      }
      pthread_mutex_unlock(&tlthread_mutex);
      return FALSE;
    }
    return TRUE;
  }
  return FALSE;
}


void unlock_timeline(void) {
  pthread_mutex_trylock(&tlthread_mutex);
  pthread_mutex_unlock(&tlthread_mutex);
}


static void redraw_timeline_inner(int clipno) {
  // Video bar
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);

  if (!sfile) return;

  mainw->drawsrc = clipno;

  if (!mainw->video_drawable) {
    mainw->video_drawable = lives_widget_create_painter_surface(mainw->video_draw);
  }
  // returns FALSE if cancel requested
  if (!update_timer_bars(clipno, 0, 0, 0, 0, 1)) return;

  // Left / mono audio

  if (!sfile->laudio_drawable) {
    sfile->laudio_drawable = lives_widget_create_painter_surface(mainw->laudio_draw);
    mainw->laudio_drawable = sfile->laudio_drawable;
    clear_tbar_bgs(0, 0, 0, 0, 2);
  } else {
    mainw->laudio_drawable = sfile->laudio_drawable;
  }
  if (!update_timer_bars(clipno, 0, 0, 0, 0, 2)) return;

  // right audio

  if (!sfile->raudio_drawable) {
    sfile->raudio_drawable = lives_widget_create_painter_surface(mainw->raudio_draw);
    mainw->raudio_drawable = sfile->raudio_drawable;
    clear_tbar_bgs(0, 0, 0, 0, 3);
  } else {
    mainw->raudio_drawable = sfile->raudio_drawable;
  }
  if (!update_timer_bars(clipno, 0, 0, 0, 0, 3)) return;

  lives_widget_queue_draw(mainw->video_draw);
  lives_widget_queue_draw(mainw->laudio_draw);
  lives_widget_queue_draw(mainw->raudio_draw);
  lives_widget_queue_draw(mainw->eventbox2);
}
