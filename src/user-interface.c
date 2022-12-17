// user-interface.c
// LiVES
// (c) G. Finch 2003 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "main.h"
#include "interface.h"
#include "effects.h"
#include "effects-weed.h"
#include "callbacks.h"
#include "omc-learn.h"
#include "startup.h"

static void _pop_to_front(LiVESWidget *dialog, LiVESWidget *extra) {
  if (prefs->startup_phase && !LIVES_IS_FILE_CHOOSER_DIALOG(dialog)) {
    if (!mainw->is_ready) {
      gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE); // dont know if this actually does anything...
      gtk_window_set_type_hint(LIVES_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
      gtk_window_set_focus_on_map(LIVES_WINDOW(dialog), TRUE);
    }
  }
  if (mainw->splash_window) {
    lives_widget_hide(mainw->splash_window);
  }

  if (extra) {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(extra), KEEPABOVE_KEY, dialog);
  }

  lives_widget_show_all(dialog);
  lives_window_present(LIVES_WINDOW(dialog));
  lives_grab_add(dialog);
}


void pop_to_front(LiVESWidget *dialog, LiVESWidget *extra) {
  if (!mainw->is_ready && !is_fg_thread()) {
    main_thread_execute(_pop_to_front, 0, NULL, "VV",
                        dialog, extra);
  } else _pop_to_front(dialog, extra);
}


void set_main_title(const char *file, int untitled) {
  char *title, *tmp, *tmp2;
  char short_file[256];

  if (file && CURRENT_CLIP_IS_VALID) {
    if (untitled) {
      title = lives_strdup_printf((tmp = _("<%s> %dx%d : %d frames %d bpp %.3f fps")), (tmp2 = get_untitled_name(untitled)),
                                  cfile->hsize, cfile->vsize, cfile->frames, cfile->bpp, cfile->fps);
    } else {
      lives_snprintf(short_file, 256, "%s", file);
      if (cfile->restoring || (cfile->opening && cfile->frames == 123456789)) {
        title = lives_strdup_printf((tmp = _("<%s> %dx%d : ??? frames ??? bpp %.3f fps")),
                                    (tmp2 = lives_path_get_basename(file)), cfile->hsize, cfile->vsize, cfile->fps);
      } else {
        title = lives_strdup_printf((tmp = _("<%s> %dx%d : %d frames %d bpp %.3f fps")),
                                    cfile->clip_type != CLIP_TYPE_VIDEODEV ? (tmp2 = lives_path_get_basename(file))
                                    : (tmp2 = lives_strdup(file)), cfile->hsize, cfile->vsize, cfile->frames, cfile->bpp, cfile->fps);
      }
    }
    lives_free(tmp); lives_free(tmp2);
  } else {
    title = (_("<No File>"));
  }

#if LIVES_HAS_HEADER_BAR_WIDGET
  lives_header_bar_set_title(LIVES_HEADER_BAR(mainw->hdrbar), title);
#else
  lives_window_set_title(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), title);
#endif
  lives_free(title);

  if (mainw->play_window) play_window_set_title();
}


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

  lives_widget_set_sensitive(mainw->rewind, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->real_pointer_time > 0.);
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
  if (!prefs->vj_mode) {
    if (mainw->rfx_loaded) {
      if (!mainw->foreign) {
        for (i = 0; i <= mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom +
             mainw->num_rendered_effects_test; i++) {
          if (i == mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate) continue;
          //if (mainw->rendered_fx[i]->props & RFX_PROPS_MAY_RESIZE) continue;
          if (mainw->rendered_fx[i]->num_in_channels == 2) continue;
          if (mainw->rendered_fx[i]->menuitem && mainw->rendered_fx[i]->min_frames >= 0)
            lives_widget_set_sensitive(mainw->rendered_fx[i]->menuitem, FALSE);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

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


void set_drawing_area_from_pixbuf(LiVESWidget * widget, LiVESPixbuf * pixbuf,
                                  lives_painter_surface_t *surface) {
  lives_painter_t *cr;
  int cx, cy;
  int rwidth, rheight, width, height, owidth, oheight;

  if (!surface || !widget) return;

  lives_painter_surface_flush(surface);

  cr = lives_painter_create_from_surface(surface);

  if (!cr) return;

  rwidth = lives_widget_get_allocation_width(widget);
  rheight = lives_widget_get_allocation_height(widget);

  if (!rwidth || !rheight) return;

  rwidth = (rwidth >> 1) << 1;
  rheight = (rheight >> 1) << 1;

  if (pixbuf) {
    owidth = width = lives_pixbuf_get_width(pixbuf);
    oheight = height = lives_pixbuf_get_height(pixbuf);

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
      lives_painter_render_background(p, cr, 0., 0., xrwidth, xrheight);

      if (mainw->multitrack) {
        rwidth = xrwidth;
        rheight = xrheight;
      }

      if ((mainw->multitrack && widget == mainw->preview_image && prefs->letterbox_mt)
          || (!mainw->multitrack && (prefs->ce_maxspect || prefs->letterbox))) {
        calc_maxspect(rwidth, rheight, &width, &height);

        width = (width >> 1) << 1;
        height = (height >> 1) << 1;

        if (width > owidth && height > oheight) {
          width = owidth;
          height = oheight;
        }
      }

      if (mainw->multitrack) {
        cx = (rwidth - width) >> 1;
        if (cx < 0) cx = 0;
        cy = (rheight - height) >> 1;
        if (cy < 0) cy = 0;
      }
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
        } else lives_painter_rectangle(cr, cx - 1, cy - 1, width + 2, height + 2);
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
  } else {
    lives_widget_set_opacity(widget, 0.);
    clear_widget_bg(widget, surface);
  }
  lives_painter_fill(cr);
  lives_painter_destroy(cr);

  cairo_surface_mark_dirty(surface);
}


