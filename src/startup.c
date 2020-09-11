// startup.c
// LiVES
// (c) G. Finch 2010 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for first time startup

#include "main.h"
#include "interface.h"
#include "rte_window.h"
#include "startup.h"

static boolean allpassed;

LiVESWidget *assist;


boolean build_init_config(const char *config_datadir, boolean prompt) {
  /// startup phase 3
  boolean create = TRUE;
  if (prompt) {
    if (!do_yesno_dialogf(_("Should I create default items in\n%s ?"), config_datadir)) create = FALSE;
  }
  if (create) {
    LiVESResponseType retval;
    char *keymap_file, *stock_icons_dir, *devmapdir;

    if (!lives_file_test(config_datadir, LIVES_FILE_TEST_IS_DIR)) {
      if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
      while (1) {
        if (!lives_make_writeable_dir(config_datadir)) {
          do_dir_perm_error(config_datadir, FALSE);
          continue;
        }
        break;
	  // *INDENT-OFF*
      }}
    // *INDENT-ON*

    /// default keymap
    keymap_file = lives_build_filename(config_datadir, DEF_KEYMAP_FILE2, NULL);
    if (!lives_file_test(keymap_file, LIVES_FILE_TEST_EXISTS)) {
      char *tmp, *keymap_template = lives_build_filename(prefs->prefix_dir, DATA_DIR, DEF_KEYMAP_FILE2, NULL);
      if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
      do {
        retval = LIVES_RESPONSE_NONE;
        if (!lives_file_test(keymap_template, LIVES_FILE_TEST_EXISTS)) {
          retval = do_file_notfound_dialog(_("LiVES was unable to find the default keymap file"),
                                           keymap_template);
          if (retval == LIVES_RESPONSE_BROWSE) {
            char *dirx = lives_build_path(prefs->prefix_dir, DATA_DIR, NULL);
            char *xkeymap_template = choose_file(dirx, DEF_KEYMAP_FILE2, NULL,
                                                 LIVES_FILE_CHOOSER_ACTION_SELECT_FILE, NULL, NULL);
            if (xkeymap_template && *xkeymap_template) {
              lives_free(keymap_template);
              keymap_template = xkeymap_template;
            }
            continue;
          }
        }
      } while (retval == LIVES_RESPONSE_RETRY);

      if (retval != LIVES_RESPONSE_CANCEL) {
        do {
          retval = LIVES_RESPONSE_NONE;
          lives_cp(keymap_template, keymap_file);
          if (!lives_file_test(keymap_file, LIVES_FILE_TEST_EXISTS)) {
            // give up
            d_print((tmp = lives_strdup_printf
                           (_("Unable to create default keymap file: %s\nPlease make sure the directory\n%s\nis writable.\n"),
                            keymap_file, config_datadir)));

            retval = do_abort_cancel_retry_dialog(tmp);
          }
        } while (retval == LIVES_RESPONSE_RETRY);
        lives_free(keymap_template);
      }
    }
    lives_free(keymap_file);

    /// default keymap
    keymap_file = lives_build_filename(config_datadir, DEF_KEYMAP_FILE3, NULL);
    if (!lives_file_test(keymap_file, LIVES_FILE_TEST_EXISTS)) {
      char *keymap_template = lives_build_filename(prefs->prefix_dir, DATA_DIR, DEF_KEYMAP_FILE3, NULL);
      retval = LIVES_RESPONSE_NONE;
      if (lives_file_test(keymap_template, LIVES_FILE_TEST_EXISTS)) {
        lives_cp(keymap_template, keymap_file);
      }
    }
    lives_free(keymap_file);

    devmapdir = lives_build_path(config_datadir, LIVES_DEVICEMAP_DIR, NULL);
    if (!lives_file_test(devmapdir, LIVES_FILE_TEST_IS_DIR)) {
#ifdef ENABLE_OSC
      char *sys_devmap_dir = lives_build_path(prefs->prefix_dir, DATA_DIR, LIVES_DEVICEMAP_DIR, NULL);
      if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
      do {
        retval = LIVES_RESPONSE_NONE;
        if (!lives_file_test(sys_devmap_dir, LIVES_FILE_TEST_IS_DIR)) {
          retval = do_dir_notfound_dialog(_("LiVES was unable to find its default device maps in\n"),
                                          sys_devmap_dir);
          if (retval == LIVES_RESPONSE_BROWSE) {
            char *xsys_devmap_dir = choose_file(sys_devmap_dir, NULL, NULL,
                                                LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, NULL);
            if (xsys_devmap_dir && *xsys_devmap_dir) {
              lives_free(sys_devmap_dir);
              sys_devmap_dir = xsys_devmap_dir;
            }
            continue;
          }
        }
      } while (retval == LIVES_RESPONSE_RETRY);

      if (retval != LIVES_RESPONSE_CANCEL) {
        do {
          retval = LIVES_RESPONSE_NONE;
          if (!lives_make_writeable_dir(devmapdir))
            retval = do_dir_perm_error(devmapdir, TRUE);
        } while (retval == LIVES_RESPONSE_RETRY);

        if (retval != LIVES_RESPONSE_CANCEL) {
          lives_cp_recursive(sys_devmap_dir, config_datadir);
        }
      }
      lives_free(sys_devmap_dir);
#endif
    }
    lives_free(devmapdir);

#ifdef GUI_GTK
    /// stock_icons
    stock_icons_dir = lives_build_path(config_datadir, STOCK_ICONS_DIR, NULL);
    if (!lives_file_test(stock_icons_dir, LIVES_FILE_TEST_IS_DIR)) {
      char *sys_stock_icons_dir = lives_build_path(prefs->prefix_dir, DATA_DIR, STOCK_ICONS_DIR, NULL);
      if (mainw && mainw->splash_window) {
        lives_widget_hide(mainw->splash_window);
        lives_widget_context_update();
      }
      do {
        retval = LIVES_RESPONSE_NONE;
        if (!lives_file_test(sys_stock_icons_dir, LIVES_FILE_TEST_IS_DIR)) {
          retval = do_dir_notfound_dialog(_("LiVES was unable to find its default icons in\n"),
                                          sys_stock_icons_dir);
          if (retval == LIVES_RESPONSE_BROWSE) {
            char *xsys_stock_icons_dir = choose_file(sys_stock_icons_dir, NULL, NULL,
                                         LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, NULL);
            if (xsys_stock_icons_dir && *xsys_stock_icons_dir) {
              lives_free(sys_stock_icons_dir);
              sys_stock_icons_dir = xsys_stock_icons_dir;
            }
            continue;
          }
        }
      } while (retval == LIVES_RESPONSE_RETRY);

      if (retval != LIVES_RESPONSE_CANCEL) {
        do {
          retval = LIVES_RESPONSE_NONE;
          if (!lives_make_writeable_dir(stock_icons_dir))
            retval = do_dir_perm_error(stock_icons_dir, TRUE);
        } while (retval == LIVES_RESPONSE_RETRY);

        if (retval != LIVES_RESPONSE_CANCEL) {
          lives_cp_recursive(sys_stock_icons_dir, config_datadir);
        }
      }
      lives_free(sys_stock_icons_dir);
    }
    lives_free(stock_icons_dir);
#endif
    if (mainw && mainw->splash_window) lives_widget_show(mainw->splash_window);

    return TRUE;
  }
  return FALSE;
}


static LiVESResponseType prompt_existing_dir(const char *dirname, uint64_t freespace, boolean wrtable) {
  // can return LIVES_RESPONSE_OK, LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY
  char *msg;
  if (wrtable) {
    if (dirs_equal(dirname, capable->home_dir)) {
      if (!do_yesno_dialog(
            _("You have chosen to use your home directory as the LiVES working directory.\n"
              "This is NOT recommended as it will possibly result in the loss of unrelated files.\n"
              "Click Yes if you REALLY want to continue, or No to create or select another directory.\n")))
        return LIVES_RESPONSE_CANCEL;
    } else {
      msg = lives_format_storage_space_string(freespace);
      boolean res = do_yesno_dialogf(
                      _("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n\n(Free space = %s)\n"),
                      dirname, msg);
      lives_free(msg);
      if (!res) return LIVES_RESPONSE_CANCEL;
      return LIVES_RESPONSE_OK;
    }
  } else {
    msg = lives_strdup_printf(_("A directory named\n%s\nalready exists.\nHowever, LiVES could not write to this directory "
                                "or read its free space.\nClick Abort to exit from LiVES, or Retry to select another "
                                "location.\n"), dirname);

    do_abort_retry_dialog(msg);
    lives_free(msg);
    return LIVES_RESPONSE_RETRY;
  }
  return LIVES_RESPONSE_OK;
}


static boolean prompt_new_dir(char *dirname, uint64_t freespace, boolean wrtable) {
  boolean res;
  if (wrtable) {
    char *fspstr = lives_format_storage_space_string(freespace);
    res = do_warning_dialogf(_("\nCreate the directory\n%s\n?\n\n(Free space = %s)"), dirname, fspstr);
    lives_free(fspstr);
  } else {
    res = do_error_dialogf(_("\nLiVES could not write to the directory\n%s\n"
                             "Please try again and choose a different location.\n"),
                           dirname);
  }
  return res;
}


void filename_toolong_error(const char *fname, const char *ftype, size_t max, boolean can_retry) {
  char *rstr, *msg;
  if (can_retry) rstr = _("\nPlease click Retry to select an alternative directory, or Abort to exit immediately"
                            "from LiVES\n");
  else rstr = lives_strdup("");

  msg = lives_strdup_printf(_("The name of the %s provided\n(%s)\nis too long (maximum is %d characters)\n%s"),
                            ftype, fname, max, rstr);
  if (can_retry) do_abort_retry_dialog(msg);
  else startup_message_fatal(msg);
  lives_free(msg); lives_free(rstr);
}

void dir_toolong_error(const char *dirname, const char *dirtype, size_t max, boolean can_retry) {
  char *rstr, *msg;
  if (can_retry) rstr = _("\nPlease click Retry to select an alternative directory, or Abort to exit immediately"
                            "from LiVES\n");
  else rstr = lives_strdup("");
  msg = lives_strdup_printf(_("The name of the %s provided\n(%s)\nis too long (maximum is %d characters)\n%s"),
                            dirtype, dirname, max, rstr);
  if (can_retry) do_abort_retry_dialog(msg);
  else startup_message_fatal(msg);
  lives_free(msg); lives_free(rstr);
}


void close_file(int current_file, boolean tshoot) {
  if (tshoot) close_current_file(current_file);
  else {
#ifdef IS_MINGW
    // kill any active processes: for other OSes the backend does this
    lives_kill_subprocesses(cfile->handle, TRUE);
#endif
    close_temp_handle(current_file);
  }
}


LiVESResponseType check_workdir_valid(char **pdirname, LiVESDialog * dialog, boolean fullcheck) {
  // returns LIVES_RESPONSE_RETRY or LIVES_RESPONSE_OK
  char cdir[PATH_MAX];
  uint64_t freesp;
  size_t chklen = strlen(LIVES_DEF_WORK_NAME) + strlen(LIVES_DIR_SEP) * 2;
  char *tmp;

  if (!pdirname || !*pdirname) return LIVES_RESPONSE_RETRY;

  if (lives_strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
    do_error_dialog(_("Directory name is too long !"));
    return LIVES_RESPONSE_RETRY;
  }

  // append a dirsep to the end if there isnt one
  lives_snprintf(cdir, PATH_MAX, "%s", *pdirname);
  ensure_isdir(cdir);

  *pdirname = lives_strdup(cdir);

  if (strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
    do_error_dialog(_("Directory name is too long !"));
    return LIVES_RESPONSE_RETRY;
  }

  if (fullcheck) {
    // if it's an existing dir, append "livesprojects" to the end unless it is already
    if (lives_file_test(*pdirname, LIVES_FILE_TEST_EXISTS) &&
        (strlen(*pdirname) < chklen || strncmp(*pdirname + strlen(*pdirname) - chklen,
            LIVES_DIR_SEP LIVES_DEF_WORK_NAME LIVES_DIR_SEP, chklen))) {
      tmp = lives_build_path(*pdirname, LIVES_DEF_WORK_NAME, NULL);
      lives_free(*pdirname);
      *pdirname = tmp;
    }

    if (lives_strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
      do_error_dialog(_("Directory name is too long !"));
      return LIVES_RESPONSE_RETRY;
    }
  }

  if (!check_dir_access(*pdirname, FALSE)) {
    do_dir_perm_error(*pdirname, FALSE);
    return LIVES_RESPONSE_RETRY;
  }

  if (fullcheck) {
    if (lives_file_test(*pdirname, LIVES_FILE_TEST_IS_DIR)) {
      if (is_writeable_dir(*pdirname)) {
        freesp = get_ds_free(*pdirname);
        widget_opts.transient = LIVES_WINDOW(dialog);
        if (!prompt_existing_dir(*pdirname, freesp, TRUE)) {
          widget_opts.transient = NULL;
          return LIVES_RESPONSE_RETRY;
        }
        widget_opts.transient = NULL;
      }
    } else {
      if (!prompt_existing_dir(*pdirname, 0, FALSE)) {
        return LIVES_RESPONSE_RETRY;
      }
    }
  } else {
    if (is_writeable_dir(*pdirname)) {
      freesp = get_ds_free(*pdirname);
      if (!prompt_new_dir(*pdirname, freesp, TRUE)) {
        lives_rmdir(*pdirname, FALSE);
        return LIVES_RESPONSE_RETRY;
      }
    } else {
      if (!prompt_new_dir(*pdirname, 0, FALSE)) {
        lives_rmdir(*pdirname, FALSE);
        return LIVES_RESPONSE_RETRY;
	// *INDENT-OFF*
        }}}
  // *INDENT-ON*

  if (!lives_make_writeable_dir(*pdirname)) {
    return do_dir_perm_error(*pdirname, FALSE);
  }

  return LIVES_RESPONSE_OK;
}


boolean do_workdir_query(void) {
  LiVESWidget *dirbutton;
  LiVESResponseType response;
  char *dirname = NULL, *mp;
  _entryw *wizard = create_rename_dialog(6);
  gtk_window_set_urgency_hint(LIVES_WINDOW(wizard->dialog), TRUE); // dont know if this actually does anything...

  /// override FILESEL_TYPE_KEY, in case it was set to WORKDIR; we will do our own checking here
  dirbutton = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dirbutton), FILESEL_TYPE_KEY,
                               LIVES_INT_TO_POINTER(LIVES_DIR_SELECTION_CREATE_FOLDER));

  if (mainw->splash_window) lives_widget_hide(mainw->splash_window);

  do {
    lives_freep((void **)&dirname);
    response = lives_dialog_run(LIVES_DIALOG(wizard->dialog));
    if (response == LIVES_RESPONSE_CANCEL) return FALSE;

    // TODO: should we convert to locale encoding ??
    dirname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(wizard->entry)));
  } while (lives_strcmp(dirname, prefs->workdir) &&
           check_workdir_valid(&dirname, LIVES_DIALOG(wizard->dialog), TRUE) == LIVES_RESPONSE_RETRY);

  mp = get_mountpoint_for(dirname);
  if (lives_strcmp(mp, capable->mountpoint) || !strcmp(mp, "??????")) {
    capable->ds_free = capable->ds_used = capable->ds_tot = -1;
    mainw->dsu_valid = FALSE;
    mainw->ds_status = LIVES_STORAGE_STATUS_UNKNOWN;
    if (capable->mountpoint) lives_free(capable->mountpoint);
    capable->mountpoint = mp;
  }

  lives_widget_destroy(wizard->dialog);
  lives_freep((void **)&wizard);

  if (mainw->splash_window) lives_widget_show(mainw->splash_window);

  if (mainw->is_ready) {
    lives_snprintf(future_prefs->workdir, PATH_MAX, "%s", dirname);
    return TRUE;
  }

  lives_snprintf(prefs->workdir, PATH_MAX, "%s", dirname);
  lives_snprintf(prefs->backend, PATH_MAX * 4, "%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL,
                 capable->backend_path, prefs->workdir, prefs->configfile);
  lives_snprintf(prefs->backend_sync, PATH_MAX * 4, "%s", prefs->backend);

  set_string_pref_priority(PREF_WORKING_DIR, prefs->workdir);
  set_string_pref(PREF_WORKING_DIR_OLD, prefs->workdir);

  mainw->has_session_workdir = FALSE;

  lives_free(dirname);

  return TRUE;
}


static void on_init_aplayer_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  int audp = LIVES_POINTER_TO_INT(user_data);

  if (!lives_toggle_button_get_active(tbutton)) return;

  prefs->audio_player = audp;

  switch (audp) {
  case AUD_PLAYER_PULSE:
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
    break;
  case AUD_PLAYER_JACK:
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
    break;
  case AUD_PLAYER_SOX:
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
    break;
  }
}


boolean do_audio_choice_dialog(short startup_phase) {
  LiVESWidget *dialog, *dialog_vbox, *radiobutton2, *label;
  LiVESWidget *okbutton, *cancelbutton;
  LiVESWidget *hbox;

#ifdef HAVE_PULSE_AUDIO
  LiVESWidget *radiobutton0;
#endif

#ifdef ENABLE_JACK
  LiVESWidget *radiobutton1;
#endif

  LiVESAccelGroup *accel_group;

  LiVESSList *radiobutton_group = NULL;

  char *txt0, *txt1, *txt2, *txt3, *txt4, *txt5, *txt6, *msg, *wid;

  LiVESResponseType response;

  if (startup_phase == 2) {
    txt0 = (_("LiVES FAILED TO START YOUR SELECTED AUDIO PLAYER !\n\n"));
  } else {
    prefs->audio_player = -1;
    txt0 = lives_strdup("");
  }

  txt1 = lives_strdup(
           _("Before starting LiVES, you need to choose an audio player.\n\nPULSE AUDIO is recommended for most users"));

#ifndef HAVE_PULSE_AUDIO
  txt2 = (_(", but this version of LiVES was not compiled with pulse audio support.\n\n"));
#else
  if (!capable->has_pulse_audio) {
    txt2 = lives_strdup(
             _(", but you do not have pulseaudio installed on your system.\n "
               "You are advised to install pulseaudio first before running LiVES.\n\n"));
  } else txt2 = lives_strdup(".\n\n");
#endif

  txt3 = (_("JACK audio is recommended for pro users"));

#ifndef ENABLE_JACK
  txt4 = (_(", but this version of LiVES was not compiled with jack audio support.\n\n"));
#else
  if (!capable->has_jackd) {
    txt4 = (_(", but you do not have jackd installed. You may wish to install jackd first before running LiVES.\n\n"));
  } else {
    txt4 = lives_strdup(
             _(", but may prevent LiVES from starting on some systems.\nIf LiVES will not start with jack,"
               "you can restart and try with another audio player instead.\n\n"));
  }
#endif

  txt5 = (_("SOX may be used if neither of the preceding players work, "));

  if (capable->has_sox_play) {
    txt6 = (_("but many audio features will be disabled.\n\n"));
  } else {
    txt6 = (_("but you do not have sox installed.\nYou are advised to install it before running LiVES.\n\n"));
  }

  msg = lives_strdup_printf("%s%s%s%s%s%s%s", txt0, txt1, txt2, txt3, txt4, txt5, txt6);

  lives_free(txt0); lives_free(txt1); lives_free(txt2);
  lives_free(txt3); lives_free(txt4); lives_free(txt5);
  lives_free(txt6);

  dialog = lives_standard_dialog_new(_("Choose an audio player"), FALSE, -1, -1);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  // TODO: add_param_label_to_box()
  label = lives_standard_label_new(msg);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  lives_free(msg);

#ifdef HAVE_PULSE_AUDIO
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton0 = lives_standard_radio_button_new(_("Use _pulseaudio player"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  if (prefs->audio_player == -1) prefs->audio_player = AUD_PLAYER_PULSE;

  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton0), TRUE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton0), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                            LIVES_INT_TO_POINTER(AUD_PLAYER_PULSE));

#endif

#ifdef ENABLE_JACK
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton1 = lives_standard_radio_button_new(_("Use _jack audio player"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  if (prefs->audio_player == AUD_PLAYER_JACK || !capable->has_pulse_audio || prefs->audio_player == -1) {
    prefs->audio_player = AUD_PLAYER_JACK;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1), TRUE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton1), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                            LIVES_INT_TO_POINTER(AUD_PLAYER_JACK));

#endif

  if (capable->has_sox_play) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton2 = lives_standard_radio_button_new(_("Use _sox audio player"), &radiobutton_group, LIVES_BOX(hbox), NULL);

    if (prefs->audio_player == -1) prefs->audio_player = AUD_PLAYER_SOX;

    if (prefs->audio_player == AUD_PLAYER_SOX) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton2), TRUE);
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
    }

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                              LIVES_INT_TO_POINTER(AUD_PLAYER_SOX));
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD, _("_Next"),
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  if (prefs->audio_player == -1) {
    do_no_mplayer_sox_error();
    return LIVES_RESPONSE_CANCEL;
  }

  lives_widget_show_now(dialog);
  lives_widget_grab_focus(okbutton);

  wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
  if (!wid || !activate_x11_window(wid)) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
    lives_widget_hide(mainw->recaudio_submenu);
  }

  return (response == LIVES_RESPONSE_OK);
}


static void add_test(LiVESWidget * table, int row, char *ttext, boolean noskip) {
  LiVESWidget *label = lives_standard_label_new(ttext);

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  if (!noskip) {
    LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_REMOVE, LIVES_ICON_SIZE_LARGE_TOOLBAR);
    // TRANSLATORS - as in "skipped test"
    label = lives_standard_label_new(_("Skipped"));

    lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
    lives_widget_show(label);

    lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
    lives_widget_show(image);
  }

  lives_widget_context_update();
}


static boolean pass_test(LiVESWidget * table, int row) {
  // TRANSLATORS - as in "passed test"
  LiVESWidget *label = lives_standard_label_new(_("Passed"));

#if GTK_CHECK_VERSION(3, 10, 0)
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_ADD, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#else
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_APPLY, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#endif

  lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();
  return TRUE;
}


static boolean _fail_test(LiVESWidget * table, int row, char *ftext, const char *type) {
  LiVESWidget *label;
#if GTK_CHECK_VERSION(3, 10, 0)
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_REMOVE, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#else
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_CANCEL, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#endif

  label = lives_standard_label_new(ftext);

  lives_table_attach(LIVES_TABLE(table), label, 3, 4, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  // TRANSLATORS - as in "failed test"
  label = lives_standard_label_new(type);

  lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();
  return FALSE;
}

LIVES_LOCAL_INLINE boolean fail_test(LiVESWidget * table, int row, char *ftext) {
  allpassed = FALSE;
  return _fail_test(table, row, ftext, _("Failed"));
}

LIVES_LOCAL_INLINE boolean skip_test(LiVESWidget * table, int row, char *ftext) {
  return _fail_test(table, row, ftext, _("Skipped"));
}

LIVES_LOCAL_INLINE char *get_resource(char *fname) {
  return lives_build_filename(prefs->prefix_dir, DATA_DIR, LIVES_RESOURCES_DIR, fname, NULL);
}


boolean do_startup_tests(boolean tshoot) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;

  LiVESWidget *label;
  LiVESWidget *table;
  LiVESWidget *okbutton;
  LiVESWidget *cancelbutton;

  LiVESAccelGroup *accel_group;

  char mppath[PATH_MAX];

  char *com, *rname, *afile, *tmp;
  char *image_ext = lives_strdup(prefs->image_ext);
  char *title, *msg;

  const char *mp_cmd;
  const char *lookfor;

  uint8_t *abuff;

  off_t fsize;

  boolean success, success2, success3, success4;
  boolean imgext_switched = FALSE;

  LiVESResponseType response;
  int res;
  int current_file = mainw->current_file;

  int out_fd, info_fd, testcase = 0;

  allpassed = TRUE;

  mainw->suppress_dprint = TRUE;
  mainw->cancelled = CANCEL_NONE;

  if (mainw->multitrack) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!tshoot) {
    title = (_("Testing Configuration"));
  } else {
    title = (_("Troubleshoot"));
  }

  dialog = lives_standard_dialog_new(title, FALSE, -1, -1);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label = lives_standard_label_new(_("LiVES will now run some basic configuration tests\n"));
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  if (!tshoot) {
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD, _("_Next"),
               LIVES_RESPONSE_OK);
  } else okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                      LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_widget_set_sensitive(okbutton, FALSE);

  table = lives_table_new(10, 4, FALSE);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), table);

  gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE);
  lives_widget_show_all(dialog);
  lives_widget_context_update();

  // check for sox presence

  add_test(table, testcase, _("Checking for \"sox\" presence"), TRUE);

  if (!capable->has_sox_sox) {
    success = fail_test(table, testcase, _("You should install sox to be able to use all the audio features in LiVES"));
    lives_widget_grab_focus(cancelbutton);
  } else {
    success = pass_test(table, testcase);
  }

  // test if sox can convert raw 44100 -> wav 22050
  add_test(table, ++testcase, _("Checking if sox can convert audio"), success);

  if (!tshoot) set_string_pref(PREF_DEFAULT_IMAGE_TYPE, LIVES_IMAGE_TYPE_PNG);
  lives_snprintf(prefs->image_ext, 16, "%s", LIVES_FILE_EXT_PNG);
  lives_snprintf(prefs->image_type, 16, "%s", LIVES_IMAGE_TYPE_PNG);

  get_temp_handle(-1);

  if (success) {
    info_fd = -1;

    lives_rm(cfile->info_file);

    // write 1 second of silence
    afile = lives_get_audio_file_name(mainw->current_file);
    out_fd = lives_open3(afile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

    if (out_fd < 0) THREADVAR(write_failed) = TRUE;
    else THREADVAR(write_failed) = FALSE;

    if (!THREADVAR(write_failed)) {
      int bytes = 44100 * 4;
      abuff = (uint8_t *)lives_calloc(44100, 4);
      if (!abuff) {
        tmp = lives_strdup_printf(_("Unable to allocate %d bytes memory."), bytes);
        fail_test(table, testcase, tmp);
        lives_free(tmp);
      } else {
        lives_write(out_fd, abuff, bytes, TRUE);
        close(out_fd);
        lives_free(abuff);
      }
    }

    if (THREADVAR(write_failed)) {
      tmp = lives_strdup_printf(_("Unable to write to: %s"), afile);
      fail_test(table, testcase, tmp);
      lives_free(tmp);
    }

    lives_free(afile);

    if (!THREADVAR(write_failed)) {
      afile = lives_build_filename(prefs->workdir, cfile->handle, "testout.wav", NULL);

      com = lives_strdup_printf("%s export_audio \"%s\" 0. 0. 44100 2 16 1 22050 \"%s\"", prefs->backend_sync,
                                cfile->handle, afile);
      lives_system(com, TRUE);
      if (THREADVAR(com_failed)) {
        THREADVAR(com_failed) = FALSE;
        tmp = lives_strdup_printf(_("Command failed: %s"), com);
        fail_test(table, testcase, tmp);
        lives_free(tmp);
      }

      lives_free(com);

      while (mainw->cancelled == CANCEL_NONE && (info_fd = open(cfile->info_file, O_RDONLY)) == -1) {
        lives_usleep(prefs->sleep_time);
        lives_widget_context_update();
      }

      if (info_fd != -1) {
        close(info_fd);

        lives_sync(1);

        fsize = sget_file_size(afile);
        lives_rm(afile);
        lives_free(afile);

        if (fsize <= 0)
          fail_test(table, testcase, _("You should install sox_fmt_all or similar"));
        else pass_test(table, testcase);
      }
    }
  }

  if (tshoot) {
    lives_snprintf(prefs->image_ext, 16, "%s", image_ext);
    lives_snprintf(prefs->image_type, 16, "%s", image_ext_to_lives_image_type(prefs->image_ext));
  }

  if (mainw->cancelled != CANCEL_NONE) {
    mainw->cancelled = CANCEL_NONE;
    close_file(current_file, tshoot);
    lives_widget_destroy(dialog);
    mainw->suppress_dprint = FALSE;

    if (!mainw->multitrack) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }

    return FALSE;
  }

  // check for mplayer presence
  success2 = TRUE;

  add_test(table, ++testcase, _("Checking for \"mplayer\", \"mplayer2\" or \"mpv\" presence"), TRUE);

  if (!capable->has_mplayer && !capable->has_mplayer2 && !capable->has_mpv) {
    success2 = fail_test(table, testcase,
                         _("You should install mplayer, mplayer2 or mpv to be able to use all the decoding features in LiVES"));
  }

  if (!success && !capable->has_mplayer2 && !capable->has_mplayer) {
    success2 = FALSE;
  }

  if (!success2) {
    if (!success) {
      lives_widget_destroy(dialog);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

      do_no_mplayer_sox_error();
      close_file(current_file, tshoot);
      mainw->suppress_dprint = FALSE;

      if (mainw->multitrack) {
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
      }

      return FALSE;
    }
  } else {
    success2 = pass_test(table, testcase);
  }

  // if present

  // check if mplayer can decode audio

  if (capable->has_mplayer) mp_cmd = EXEC_MPLAYER;
  else if (capable->has_mplayer2) mp_cmd = EXEC_MPLAYER2;
  else mp_cmd = EXEC_MPV;

  get_location(mp_cmd, mppath, PATH_MAX);
  lives_snprintf(prefs->video_open_command, PATH_MAX + 2, "\"%s\"", mppath);
  set_string_pref(PREF_VIDEO_OPEN_COMMAND, prefs->video_open_command);

  msg = lives_strdup_printf(_("Checking if %s can convert audio"), mp_cmd);
  add_test(table, ++testcase, msg, success2);
  lives_free(msg);

  res = 1;

  if (success2) {
    // TODO - add a timeout
#ifndef IS_MINGW
    com = lives_strdup_printf("LANG=en LANGUAGE=en %s -ao help | %s pcm >/dev/null 2>&1", prefs->video_open_command,
                              capable->grep_cmd);
    res = lives_system(com, TRUE);
    lives_free(com);
#else
    com = lives_strdup_printf("%s -ao help | %s pcm >NUL 2>&1", prefs->video_open_command, capable->grep_cmd);
    res = lives_system(com, TRUE);
    lives_free(com);
#endif
  }

  if (res == 0) {
    pass_test(table, testcase);
  } else {
    fail_test(table, testcase, _("You should install mplayer,mplayer2 or mpv with pcm/wav support"));
  }

  // check if mplayer can decode to png/(alpha)

  rname = get_resource("");
  /// ensure that the resources dir is there
  if (!lives_file_test(rname, LIVES_FILE_TEST_IS_DIR)) {
    /// oops, no resources dir
    success4 = FALSE;
  } else success4 = TRUE;

#ifdef ALLOW_PNG24
  msg = lives_strdup_printf(_("Checking if %s can decode to png"), mp_cmd);
#else
  msg = lives_strdup_printf(_("Checking if %s can decode to png/alpha"), mp_cmd);
#endif
  add_test(table, ++testcase, msg, success2);
  lives_free(msg);

  success3 = FALSE;

  if (success2 && !success4) {
    tmp = lives_strdup_printf(_("Resource directory %s not found !"), rname);
    skip_test(table, testcase, tmp);
    lives_free(tmp);

    msg = lives_strdup_printf(_("Checking less rigorously"), mp_cmd);
    add_test(table, ++testcase, msg, TRUE);
    lives_free(msg);

    res = 1;

    if (!strcmp(mp_cmd, "mpv")) lookfor = "image";
    else lookfor = "png file";

#ifndef IS_MINGW
    com = lives_strdup_printf("LANG=en LANGUAGE=en %s -vo help | %s -i \"%s\" >/dev/null 2>&1",
                              prefs->video_open_command, capable->grep_cmd, lookfor);
#else
    com = lives_strdup_printf("%s -vo help | %s -i \"%s\" >NUL 2>&1", prefs->video_open_command,
                              capable->grep_cmd, lookfor);
#endif
    res = lives_system(com, TRUE);
    lives_free(com);

    if (!res) {
      pass_test(table, testcase);
      success3 = TRUE;
    }
  }

  lives_free(rname);

  // try to open resource vidtest.avi
  if (!success3 && success2 && success4) {
    info_fd = -1;

    lives_rm(cfile->info_file);

    rname = get_resource(LIVES_TEST_VIDEO_NAME);

    com = lives_strdup_printf("%s open_test \"%s\" %s \"%s\" 0 png", prefs->backend_sync, cfile->handle,
                              prefs->video_open_command,
                              (tmp = lives_filename_from_utf8(rname, -1, NULL, NULL, NULL)));
    lives_free(tmp);
    lives_free(rname);

    lives_system(com, TRUE);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      tmp = lives_strdup_printf(_("Command failed: %s"), com);
      fail_test(table, testcase, tmp);
      lives_free(tmp);
    }

    lives_free(com);

    while (mainw->cancelled == CANCEL_NONE && (info_fd = open(cfile->info_file, O_RDONLY)) == -1) {
      lives_usleep(prefs->sleep_time);
    }

    if (info_fd != -1) {
      close(info_fd);

      lives_sync(1);

      cfile->img_type = IMG_TYPE_PNG;
      cfile->frames = get_frame_count(mainw->current_file, 1);

      if (cfile->frames <= 0) {
        msg = lives_strdup_printf(_("You may wish to upgrade %s to a newer version"), mp_cmd);
        fail_test(table, testcase, msg);
        lives_free(msg);
      }

      else {
        pass_test(table, testcase);
        success3 = TRUE;
      }
    }
  }

  if (mainw->cancelled != CANCEL_NONE) {
    mainw->cancelled = CANCEL_NONE;
    close_file(current_file, tshoot);
    lives_widget_destroy(dialog);
    mainw->suppress_dprint = FALSE;

    if (mainw->multitrack) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }

    return FALSE;
  }

  // check if mplayer can decode to jpeg

  msg = lives_strdup_printf(_("Checking if %s can decode to jpeg"), mp_cmd);
  add_test(table, ++testcase, msg, success2);
  lives_free(msg);
  res = 1;

  if (!strcmp(mp_cmd, "mpv")) {
    if (success2 && success3 && !success4) {
      tmp = (_("Already checked"));
      skip_test(table, testcase - 1, tmp);
      lives_free(tmp);
      goto jpgdone;
    }
    lookfor = "image";
  } else lookfor = "jpeg file";

  if (success2) {
#ifndef IS_MINGW
    com = lives_strdup_printf("LANG=en LANGUAGE=en %s -vo help | %s -i \"%s\" >/dev/null 2>&1",
                              prefs->video_open_command, capable->grep_cmd, lookfor);
    res = lives_system(com, TRUE);
    lives_free(com);
#else
    com = lives_strdup_printf("%s -vo help | %s -i \"%s\" >NUL 2>&1", prefs->video_open_command,
                              capable->grep_cmd, lookfor);
    res = lives_system(com, TRUE);
    lives_free(com);
#endif
  }

  if (res == 0) {
    pass_test(table, testcase);
    if (!success3) {
      if (!strcmp(prefs->image_ext, LIVES_FILE_EXT_PNG)) imgext_switched = TRUE;
      set_string_pref(PREF_DEFAULT_IMAGE_TYPE, LIVES_IMAGE_TYPE_JPEG);
      lives_snprintf(prefs->image_ext, 16, "%s", LIVES_FILE_EXT_JPG);
      lives_snprintf(prefs->image_type, 16, "%s", LIVES_IMAGE_TYPE_JPEG);
    }
  } else {
    if (!success3) {
#ifdef ALLOW_PNG24
      msg = lives_strdup_printf(_("You should install %s with either png or jpeg support"), mp_cmd);
#else
      msg = lives_strdup_printf(_("You should install %s with either png/alpha or jpeg support"), mp_cmd);
#endif
      fail_test(table, testcase, msg);
      lives_free(msg);
    } else {
      msg = lives_strdup_printf(_("You may wish to add jpeg output support to %s"), mp_cmd);
      fail_test(table, testcase, msg);
      lives_free(msg);
    }
  }

  // TODO - check each enabled decoder plugin in turn

jpgdone:
  // check for convert

  add_test(table, ++testcase, _("Checking for \"convert\" presence"), TRUE);

  if (!capable->has_convert) {
    success = fail_test(table, testcase, _("Install imageMagick to be able to use all of the rendered effects"));
  } else {
    success = pass_test(table, testcase);
  }

  close_file(current_file, tshoot);
  mainw->current_file = current_file;

  lives_widget_set_sensitive(okbutton, TRUE);
  lives_widget_grab_focus(okbutton);
  /* if (!tshoot) { */
  /*   if (allpassed) { */
  /*   } else { */
  /*     lives_widget_grab_focus(cancelbutton); */
  /*   } */
  /* } */

  if (tshoot) {
    lives_widget_hide(cancelbutton);
    if (imgext_switched) {
      label = lives_standard_label_new(
                _("\n\n\tImage decoding type has been switched to jpeg. You can revert this in Preferences/Decoding.\t\n"));
      lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    }
    lives_widget_show(label);
  } else {
    label = lives_standard_label_new(
              _("\n\n\tClick Cancel to exit and install any missing components, or Next to continue\t\n"));
    lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    lives_widget_show(label);
  }

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);
  mainw->suppress_dprint = FALSE;

  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
  }

  return (response == LIVES_RESPONSE_OK);
}


void do_startup_interface_query(void) {
  // prompt for startup ce or startup mt

  LiVESWidget *dialog, *dialog_vbox, *radiobutton, *label;
  LiVESWidget *okbutton;
  LiVESWidget *hbox;
  LiVESSList *radiobutton_group = NULL;
  char *txt1, *txt2, *txt3, *msg, *wid;

  txt1 = (_("\n\nFinally, you can choose the default startup interface for LiVES.\n"));
  txt2 = (_("\n\nLiVES has two main interfaces and you can start up with either of them.\n"));
  txt3 = (_("\n\nThe default can always be changed later from Preferences.\n"));

  msg = lives_strdup_printf("%s%s%s", txt1, txt2, txt3);

  lives_free(txt1); lives_free(txt2); lives_free(txt3);

  dialog = lives_standard_dialog_new(_("Choose the Startup Interface"), FALSE, -1, -1);
  //if (transient) lives_window_set_transient_for(LIVES_WINDOW(dialog), NULL);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label = lives_standard_label_new(msg);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  lives_free(msg);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  lives_standard_radio_button_new(_("Start in _Clip Edit mode"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new(_("This is the best choice for simple editing tasks and for VJs\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  radiobutton = lives_standard_radio_button_new(_("Start in _Multitrack mode"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new(_("This is a better choice for complex editing tasks involving multiple clips.\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  if (prefs->startup_interface == STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);
  }

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
             _("Set Quota Limits (Optional)"), LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_show_now(dialog);

  wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
  if (!wid || !activate_x11_window(wid)) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);

  lives_dialog_run(LIVES_DIALOG(dialog));

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(radiobutton)))
    future_prefs->startup_interface = prefs->startup_interface = STARTUP_MT;

  set_int_pref(PREF_STARTUP_INTERFACE, prefs->startup_interface);

  lives_widget_destroy(dialog);
}


void on_troubleshoot_activate(LiVESMenuItem * menuitem, livespointer user_data) {do_startup_tests(TRUE);}


static char *explain_missing(const char *exe) {
  char *pt2, *pt1 = lives_strdup_printf(_("\t'%s' was not found on your system.\n"
                                          "Installation is recommended as it provides the following features\n\t- "), exe);
  if (!lives_strcmp(exe, EXEC_FILE)) pt2 = (_("Enables easier identification of file types,\n\n"));
  else if (!lives_strcmp(exe, EXEC_GZIP)) pt2 = (_("Enables reduction in file size for some files,\n\n"));
  else if (!lives_strcmp(exe, EXEC_DU)) pt2 = (_("Enables measuring of disk space used,\n\n"));
  else if (!lives_strcmp(exe, EXEC_FFPROBE)) pt2 = (_("Assists in the identification of video clips\n\n"));
  else if (!lives_strcmp(exe, EXEC_IDENTIFY)) pt2 = (_("Assists in the identification of image files\n\n"));
  else if (!lives_strcmp(exe, EXEC_CONVERT)) pt2 = (_("Required for many rendered effects in the clip editor.\n\n"));
  else if (!lives_strcmp(exe, EXEC_COMPOSITE)) pt2 = (_("Enables clip mergeing in the clip editor.\n\n"));
  else if (!lives_strcmp(exe, EXEC_PYTHON)) pt2 = (_("Allows use of some additional encoder plugins\n\n"));
  else if (!lives_strcmp(exe, EXEC_MD5SUM)) pt2 = (_("Allows checking for file changes, "
        "enabing additional files to be cached in memory.\n\n"));
  else if (!lives_strcmp(exe, EXEC_YOUTUBE_DL)) pt2 = (_("Enables download and import of files from "
        "Youtube and other sites.\n\n"));
  else if (!lives_strcmp(exe, EXEC_XWININFO)) pt2 = (_("Enables identification of external windows "
        "so that they can be recorded.\n\n"));
  else {
    lives_free(pt1);
    pt1 = lives_strdup_printf(_("\t'%s' was not found on your system.\n"
                                "Installation is optional, but may enable additionl features\n\t- "), exe);
    if (!lives_strcmp(exe, EXEC_XDOTOOL)) pt2 = (_("Enables adjustment of windows within the desktop,\n\n"));
    else return lives_strdup_free(pt1, "");
  }
  return lives_concat(pt1, pt2);
}


#define ADD_TO_TEXT(what, exec)   if (!capable->has_##what) {	\
    text = lives_concat(text, explain_missing(exec)) ;\
}

void explain_missing_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  char *title = (_("What is missing ?")), *text = lives_strdup("");

  if (capable->has_file == UNCHECKED) capable->has_file = has_executable(EXEC_FILE);

  ADD_TO_TEXT(file, EXEC_FILE);
  ADD_TO_TEXT(du,  EXEC_DU);
  ADD_TO_TEXT(identify, EXEC_IDENTIFY);
  ADD_TO_TEXT(md5sum, EXEC_MD5SUM);
  ADD_TO_TEXT(ffprobe, EXEC_FFPROBE);
  ADD_TO_TEXT(convert, EXEC_CONVERT);
  ADD_TO_TEXT(composite, EXEC_COMPOSITE);
  ADD_TO_TEXT(python, EXEC_PYTHON);
  ADD_TO_TEXT(gzip, EXEC_GZIP);
  ADD_TO_TEXT(youtube_dl, EXEC_YOUTUBE_DL);
  ADD_TO_TEXT(xwininfo, EXEC_XWININFO);
  if (!(*text)) {
    lives_free(title); lives_free(text);
    do_info_dialog(_("All optional components located\n"));
    return;
  }
  text = lives_concat(text, (_("\n\nIf you DO have any of these missing components, please ensure they are "
                               "located in your $PATH before restarting LiVES")));
  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  create_text_window(title, text, NULL, TRUE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;;
  lives_free(title);
  lives_free(text);
}
#undef ADD_TO_TEXT

