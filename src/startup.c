// startup.c
// LiVES
// (c) G. Finch 2010 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for first time startup

#include "main.h"
#include "interface.h"
#include "startup.h"

static boolean allpassed;

static LiVESResponseType prompt_existing_dir(const char *dirname, uint64_t freespace, boolean wrtable, LiVESWindow *transient) {
  // can return LIVES_RESPONSE_OK, LIVES_RESPONSE_CANCEL or LIVES_RESPONSE_RETRY
  char *msg;
  if (wrtable) {
    if (dirs_equal(dirname, capable->home_dir)) {
      if (!do_yesno_dialog(
            _("You have chosen to use your home directory as the LiVES working directory.\nThis is NOT recommended as it will possibly result in the loss of unrelated files.\nClick Yes if you REALLY want to continue, or No to create or select another directory.\n")))
        return LIVES_RESPONSE_CANCEL;
    } else {
      msg = lives_format_storage_space_string(freespace);
      boolean res = do_yesno_dialogf(_("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n\n(Free space = %s)\n"),
                                     dirname, msg);
      lives_free(msg);
      if (!res) return LIVES_RESPONSE_CANCEL;
      return LIVES_RESPONSE_OK;
    }
  } else {
    msg = lives_strdup_printf(_("A directory named\n%s\nalready exists.\nHowever, LiVES could not write to this directory "
                                "or read its free space.\nClick Abort to exit from LiVES, or Retry to select another "
                                "location.\n"), dirname);

    do_abort_retry_dialog(msg, transient);
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
    res = do_error_dialogf(_("\nLiVES could not write to the directory\n%s\nPlease try again and choose a different location.\n"), dirname);
  }
  return res;
}


void dir_toolong_error(char *dirname, const char *dirtype, size_t max, boolean retry) {
  char *msg = lives_strdup_printf(_("The name of the %s provided\n(%s)\nis too long (maximum is %d characters)\n"
                                    "Please click Retry to select an alternative directory, or Abort to exit immediately"
                                    "from LiVES"), dirtype, dirname, max);
  if (retry) do_abort_retry_dialog(msg, NULL);
  else startup_message_fatal(msg);
  lives_free(msg);
}


void do_bad_dir_perms_error(const char *dirname) {
  char *msg = lives_strdup_printf(_("LiVES could not write to the directory\n%s\nPlease check permissions for the directory.\n"
                                    "Click Abort to exit immediately from LiVES, or Retry to select a different directory.\n"),
                                  *dirname);

  do_abort_retry_dialog(msg, NULL);
  lives_free(msg);
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


LiVESResponseType check_workdir_valid(char **pdirname, LiVESDialog *dialog, boolean fullcheck) {
  // returns LIVES_RESPONSE_RETRY or LIVES_RESPONSE_OK
  char cdir[PATH_MAX];
  uint64_t freesp;
  size_t chklen = strlen(LIVES_DEF_WORK_NAME) + strlen(LIVES_DIR_SEP) * 2;
  char *tmp;

  if (pdirname == NULL || *pdirname == NULL) return LIVES_RESPONSE_RETRY;

  if (strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
    do_blocking_error_dialog(_("Directory name is too long !"));
    return LIVES_RESPONSE_RETRY;
  }

  // append a dirsep to the end if there isnt one
  lives_snprintf(cdir, PATH_MAX, "%s", *pdirname);
  ensure_isdir(cdir);

  *pdirname = lives_strdup(cdir);

  if (strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
    do_blocking_error_dialog(_("Directory name is too long !"));
    return LIVES_RESPONSE_RETRY;
  }

  if (fullcheck) {
    // if it's an existing dir, append "livesprojects" to the end unless it is already
    if (lives_file_test(*pdirname, LIVES_FILE_TEST_EXISTS) &&
        (strlen(*pdirname) < chklen || strncmp(*pdirname + strlen(*pdirname) - chklen,
            LIVES_DIR_SEP LIVES_DEF_WORK_NAME LIVES_DIR_SEP, chklen))) {
      tmp = lives_strdup_printf("%s%s%s", *pdirname, LIVES_DEF_WORK_NAME, LIVES_DIR_SEP);
      lives_free(*pdirname);
      *pdirname = tmp;
    }

    if (strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
      do_blocking_error_dialog(_("Directory name is too long !"));
      return LIVES_RESPONSE_RETRY;
    }
  }

  if (!check_dir_access(*pdirname)) {
    do_dir_perm_error(*pdirname);
    return LIVES_RESPONSE_RETRY;
  }

  if (fullcheck) {
    if (lives_file_test(*pdirname, LIVES_FILE_TEST_IS_DIR)) {
      if (is_writeable_dir(*pdirname)) {
        freesp = get_fs_free(*pdirname);
        if (!prompt_existing_dir(*pdirname, freesp, TRUE, LIVES_WINDOW(dialog))) {
          return LIVES_RESPONSE_RETRY;
        }
      } else {
        if (!prompt_existing_dir(*pdirname, 0, FALSE, LIVES_WINDOW(dialog))) {
          return LIVES_RESPONSE_RETRY;
        }
      }
    } else {
      if (is_writeable_dir(*pdirname)) {
        freesp = get_fs_free(*pdirname);
        if (!prompt_new_dir(*pdirname, freesp, TRUE)) {
          lives_rmdir(*pdirname, FALSE);
          return LIVES_RESPONSE_RETRY;
        }
      } else {
        if (!prompt_new_dir(*pdirname, 0, FALSE)) {
          lives_rmdir(*pdirname, FALSE);
          return LIVES_RESPONSE_RETRY;
        }
      }
    }
  }

  if (!lives_make_writeable_dir(*pdirname)) {
    do_bad_dir_perms_error(*pdirname);
    return LIVES_RESPONSE_RETRY;
  }

  return LIVES_RESPONSE_OK;
}


boolean do_workdir_query(void) {
  int response;

  char *dirname = NULL;

  _entryw *wizard = create_rename_dialog(6);
  gtk_window_set_urgency_hint(LIVES_WINDOW(wizard->dialog), TRUE); // dont know if this actually does anything...

  do {
    lives_freep((void **)&dirname);
    response = lives_dialog_run(LIVES_DIALOG(wizard->dialog));
    if (response == LIVES_RESPONSE_CANCEL) return FALSE;

    // TODO: should we convert to locale encoding ??
    dirname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(wizard->entry)));
  } while (check_workdir_valid(&dirname, LIVES_DIALOG(wizard->dialog), TRUE) == LIVES_RESPONSE_RETRY);

  lives_widget_destroy(wizard->dialog);
  lives_freep((void **)&wizard);

  lives_snprintf(prefs->workdir, PATH_MAX, "%s", dirname);

  set_string_pref_priority(PREF_WORKING_DIR, prefs->workdir);
  set_string_pref(PREF_WORKING_DIR_OLD, prefs->workdir);

  mainw->has_session_workdir = FALSE;

  lives_snprintf(prefs->backend, PATH_MAX * 4, "%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGDIR=\"%s\" --", EXEC_PERL, capable->backend_path,
                 prefs->workdir, prefs->configdir);
  lives_snprintf(prefs->backend_sync, PATH_MAX * 4, "%s", prefs->backend);

  lives_free(dirname);

  return TRUE;
}


static void on_init_aplayer_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
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

  char *txt0, *txt1, *txt2, *txt3, *txt4, *txt5, *txt6, *msg;

  int response;

  if (startup_phase == 2) {
    txt0 = lives_strdup(_("LiVES FAILED TO START YOUR SELECTED AUDIO PLAYER !\n\n"));
  } else {
    prefs->audio_player = -1;
    txt0 = lives_strdup("");
  }

  txt1 = lives_strdup(_("Before starting LiVES, you need to choose an audio player.\n\nPULSE AUDIO is recommended for most users"));

#ifndef HAVE_PULSE_AUDIO
  txt2 = lives_strdup(_(", but this version of LiVES was not compiled with pulse audio support.\n\n"));
#else
  if (!capable->has_pulse_audio) {
    txt2 = lives_strdup(
             _(", but you do not have pulseaudio installed on your system.\n You are advised to install pulseaudio first before running LiVES.\n\n"));
  } else txt2 = lives_strdup(".\n\n");
#endif

  txt3 = lives_strdup(_("JACK audio is recommended for pro users"));

#ifndef ENABLE_JACK
  txt4 = lives_strdup(_(", but this version of LiVES was not compiled with jack audio support.\n\n"));
#else
  if (!capable->has_jackd) {
    txt4 = lives_strdup(_(", but you do not have jackd installed. You may wish to install jackd first before running LiVES.\n\n"));
  } else {
    txt4 = lives_strdup(
             _(", but may prevent LiVES from starting on some systems.\nIf LiVES will not start with jack,"
               "you can restart and try with another audio player instead.\n\n"));
  }
#endif

  txt5 = lives_strdup(_("SOX may be used if neither of the preceding players work, "));

  if (capable->has_sox_play) {
    txt6 = lives_strdup(_("but many audio features will be disabled.\n\n"));
  } else {
    txt6 = lives_strdup(_("but you do not have sox installed.\nYou are advised to install it before running LiVES.\n\n"));
  }

  msg = lives_strdup_printf("%s%s%s%s%s%s%s", txt0, txt1, txt2, txt3, txt4, txt5, txt6);

  lives_free(txt0);
  lives_free(txt1);
  lives_free(txt2);
  lives_free(txt3);
  lives_free(txt4);
  lives_free(txt5);
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

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton0), LIVES_WIDGET_TOGGLED_SIGNAL,
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

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton1), LIVES_WIDGET_TOGGLED_SIGNAL,
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

    lives_signal_connect(LIVES_GUI_OBJECT(radiobutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
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

  lives_widget_show_all(dialog);

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label != NULL) lives_widget_hide(mainw->vol_label);
    lives_widget_hide(mainw->recaudio_submenu);
  }

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  return (response == LIVES_RESPONSE_OK);
}


static void add_test(LiVESWidget *table, int row, char *ttext, boolean noskip) {
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


static boolean pass_test(LiVESWidget *table, int row) {
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


static boolean fail_test(LiVESWidget *table, int row, char *ftext) {
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
  label = lives_standard_label_new(_("Failed"));

  lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();
  allpassed = FALSE;
  return FALSE;
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

  size_t fsize;

  boolean success, success2, success3, success4;
  boolean imgext_switched = FALSE;

  int response, res;
  int current_file = mainw->current_file;

  int out_fd, info_fd;

  allpassed = TRUE;

  mainw->suppress_dprint = TRUE;
  mainw->cancelled = CANCEL_NONE;

  if (mainw->multitrack != NULL) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!tshoot) {
    title = lives_strdup(_("Testing Configuration"));
  } else {
    title = lives_strdup(_("Troubleshoot"));
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

  add_test(table, 0, _("Checking for \"sox\" presence"), TRUE);

  if (!capable->has_sox_sox) {
    success = fail_test(table, 0, _("You should install sox to be able to use all the audio features in LiVES"));
    lives_widget_grab_focus(cancelbutton);
  } else {
    success = pass_test(table, 0);
  }

  // test if sox can convert raw 44100 -> wav 22050
  add_test(table, 1, _("Checking if sox can convert audio"), success);

  if (!tshoot) set_string_pref(PREF_DEFAULT_IMAGE_FORMAT, LIVES_IMAGE_TYPE_PNG);
  lives_snprintf(prefs->image_ext, 16, "%s", LIVES_FILE_EXT_PNG);

  get_temp_handle(-1);

  if (success) {
    info_fd = -1;

    lives_rm(cfile->info_file);

    // write 1 second of silence
    afile = lives_get_audio_file_name(mainw->current_file);
    out_fd = lives_open3(afile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

    if (out_fd < 0) mainw->write_failed = TRUE;
    else mainw->write_failed = FALSE;

    if (!mainw->write_failed) {
      int bytes = 44100 * 4;
      abuff = (uint8_t *)lives_calloc(44100, 4);
      if (!abuff) {
        tmp = lives_strdup_printf(_("Unable to allocate %d bytes memory."), bytes);
        fail_test(table, 1, tmp);
        lives_free(tmp);
      } else {
        lives_write(out_fd, abuff, bytes, TRUE);
        close(out_fd);
        lives_free(abuff);
      }
    }

    if (mainw->write_failed) {
      tmp = lives_strdup_printf(_("Unable to write to: %s"), afile);
      fail_test(table, 1, tmp);
      lives_free(tmp);
    }

    lives_free(afile);

    if (!mainw->write_failed) {
      afile = lives_build_filename(prefs->workdir, cfile->handle, "testout.wav", NULL);

      mainw->com_failed = FALSE;
      com = lives_strdup_printf("%s export_audio \"%s\" 0. 0. 44100 2 16 1 22050 \"%s\"", prefs->backend_sync, cfile->handle, afile);
      lives_system(com, TRUE);
      if (mainw->com_failed) {
        tmp = lives_strdup_printf(_("Command failed: %s"), com);
        fail_test(table, 1, tmp);
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

        if (fsize == 0) {
          fail_test(table, 1, _("You should install sox_fmt_all or similar"));
        } else pass_test(table, 1);
      }
    }
  }

  if (tshoot) lives_snprintf(prefs->image_ext, 16, "%s", image_ext);

  if (mainw->cancelled != CANCEL_NONE) {
    mainw->cancelled = CANCEL_NONE;
    close_file(current_file, tshoot);
    lives_widget_destroy(dialog);
    mainw->suppress_dprint = FALSE;

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }

    return FALSE;
  }

  // check for mplayer presence
  success2 = TRUE;

  add_test(table, 2, _("Checking for \"mplayer\", \"mplayer2\" or \"mpv\" presence"), TRUE);

  if (!capable->has_mplayer && !capable->has_mplayer2 && !capable->has_mpv) {
    success2 = fail_test(table, 2, _("You should install mplayer, mplayer2 or mpv to be able to use all the decoding features in LiVES"));
  }

  if (!success && !capable->has_mplayer2 && !capable->has_mplayer) {
    success2 = FALSE;
  }

  if (!success2) {
    if (!success) {
      lives_widget_destroy(dialog);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

      do_no_mplayer_sox_error();
      close_file(current_file, tshoot);
      mainw->suppress_dprint = FALSE;

      if (mainw->multitrack != NULL) {
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
      }

      return FALSE;
    }
  } else {
    success2 = pass_test(table, 2);
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
  add_test(table, 3, msg, success2);
  lives_free(msg);

  res = 1;

  if (success2) {
    // TODO - add a timeout
#ifndef IS_MINGW
    com = lives_strdup_printf("LANG=en LANGUAGE=en %s -ao help | grep pcm >/dev/null 2>&1", prefs->video_open_command);
    res = lives_system(com, TRUE);
    lives_free(com);
#else
    com = lives_strdup_printf("%s -ao help | grep pcm >NUL 2>&1", prefs->video_open_command);
    res = lives_system(com, TRUE);
    lives_free(com);
#endif
  }

  if (res == 0) {
    pass_test(table, 3);
  } else {
    fail_test(table, 3, _("You should install mplayer,mplayer2 or mpv with pcm/wav support"));
  }

  // check if mplayer can decode to png/(alpha)

  rname = get_resource("");

  if (!lives_file_test(rname, LIVES_FILE_TEST_IS_DIR)) {
    /// oops, no resources dir
    success4 = FALSE;
  } else success4 = TRUE;

  lives_free(rname);

#ifdef ALLOW_PNG24
  msg = lives_strdup_printf(_("Checking if %s can decode to png"), mp_cmd);
#else
  msg = lives_strdup_printf(_("Checking if %s can decode to png/alpha"), mp_cmd);
#endif
  add_test(table, 4, msg, success2 && success4);
  lives_free(msg);

  success3 = FALSE;

  // try to open resource vidtest.avi

  if (success2 && success4) {
    info_fd = -1;

    lives_rm(cfile->info_file);

    rname = get_resource(LIVES_TEST_VIDEO_NAME);

    com = lives_strdup_printf("%s open_test \"%s\" %s \"%s\" 0 png", prefs->backend_sync, cfile->handle, prefs->video_open_command,
                              (tmp = lives_filename_from_utf8(rname, -1, NULL, NULL, NULL)));
    lives_free(tmp);
    lives_free(rname);

    mainw->com_failed = FALSE;
    lives_system(com, TRUE);
    if (mainw->com_failed) {
      tmp = lives_strdup_printf(_("Command failed: %s"), com);
      fail_test(table, 4, tmp);
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
      get_frame_count(mainw->current_file);

      if (cfile->frames == 0) {
        msg = lives_strdup_printf(_("You may wish to upgrade %s to a newer version"), mp_cmd);
        fail_test(table, 4, msg);
        lives_free(msg);
      }

      else {
        pass_test(table, 4);
        success3 = TRUE;
      }
    }
  }

  if (mainw->cancelled != CANCEL_NONE) {
    mainw->cancelled = CANCEL_NONE;
    close_file(current_file, tshoot);
    lives_widget_destroy(dialog);
    mainw->suppress_dprint = FALSE;

    if (mainw->multitrack != NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }

    return FALSE;
  }

  // check if mplayer can decode to jpeg

  msg = lives_strdup_printf(_("Checking if %s can decode to jpeg"), mp_cmd);
  add_test(table, 5, msg, success2);
  lives_free(msg);

  res = 1;

  if (!strcmp(mp_cmd, "mpv")) lookfor = "image";
  else lookfor = "jpeg file";

  if (success2) {
#ifndef IS_MINGW
    com = lives_strdup_printf("LANG=en LANGUAGE=en %s -vo help | grep -i \"%s\" >/dev/null 2>&1", prefs->video_open_command, lookfor);
    res = lives_system(com, TRUE);
    lives_free(com);
#else
    com = lives_strdup_printf("%s -vo help | grep -i \"%s\" >NUL 2>&1", prefs->video_open_command, lookfor);
    res = lives_system(com, TRUE);
    lives_free(com);
#endif
  }

  if (res == 0) {
    pass_test(table, 5);
    if (!success3) {
      if (!strcmp(prefs->image_ext, LIVES_FILE_EXT_PNG)) imgext_switched = TRUE;
      set_string_pref(PREF_DEFAULT_IMAGE_FORMAT, LIVES_IMAGE_TYPE_JPEG);
      lives_snprintf(prefs->image_ext, 16, "%s", LIVES_FILE_EXT_JPG);
    }
  } else {
    if (!success3) {
#ifdef ALLOW_PNG24
      msg = lives_strdup_printf(_("You should install %s with either png or jpeg support"), mp_cmd);
#else
      msg = lives_strdup_printf(_("You should install %s with either png/alpha or jpeg support"), mp_cmd);
#endif
      fail_test(table, 5, msg);
      lives_free(msg);
    } else {
      msg = lives_strdup_printf(_("You may wish to add jpeg output support to %s"), mp_cmd);
      fail_test(table, 5, msg);
      lives_free(msg);
    }
  }

  // TODO - check each enabled decoder plugin in turn

  // check for convert

  add_test(table, 8, _("Checking for \"convert\" presence"), TRUE);

  if (!capable->has_convert) {
    success = fail_test(table, 8, _("Install imageMagick to be able to use all of the rendered effects"));
  } else {
    success = pass_test(table, 8);
  }

  close_file(current_file, tshoot);
  mainw->current_file = current_file;

  lives_widget_set_sensitive(okbutton, TRUE);
  if (!tshoot) {
    if (allpassed) {
      gtk_widget_grab_focus(okbutton);
    } else {
      lives_widget_grab_focus(cancelbutton);
    }
  }

  if (!capable->has_mplayer && !capable->has_mplayer2 && capable->has_mpv) {
    label = lives_standard_label_new(
              _("\n\nLiVES has experimental support for 'mpv' but it is advisable to install\n"
                "'mplayer' or 'mplayer2' in order to use all the features of LiVES"));
    lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  }

  if (tshoot) {
    lives_widget_hide(cancelbutton);
    if (imgext_switched) {
      label = lives_standard_label_new(
                _("\n\n    Image decoding type has been switched to jpeg. You can revert this in Preferences/Decoding.    \n"));
      lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    }
    lives_widget_show(label);
  } else {
    label = lives_standard_label_new(_("\n\n    Click Cancel to exit and install any missing components, or Next to continue    \n"));
    lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    lives_widget_show(label);
  }

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);
  mainw->suppress_dprint = FALSE;

  if (mainw->multitrack != NULL) {
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
  char *txt1, *txt2, *txt3, *msg;

  txt1 = lives_strdup(_("\n\nFinally, you can choose the default startup interface for LiVES.\n"));
  txt2 = lives_strdup(_("\n\nLiVES has two main interfaces and you can start up with either of them.\n"));
  txt3 = lives_strdup(_("\n\nThe default can always be changed later from Preferences.\n"));

  msg = lives_strdup_printf("%s%s%s", txt1, txt2, txt3);

  lives_free(txt1);
  lives_free(txt2);
  lives_free(txt3);

  dialog = lives_standard_dialog_new(_("Choose the Startup Interface"), FALSE, -1, -1);

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

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD, _("_Finish"),
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_widget_show_all(dialog);

  lives_dialog_run(LIVES_DIALOG(dialog));

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(radiobutton)))
    future_prefs->startup_interface = prefs->startup_interface = STARTUP_MT;

  set_int_pref(PREF_STARTUP_INTERFACE, prefs->startup_interface);

  lives_widget_destroy(dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
}


void on_troubleshoot_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  do_startup_tests(TRUE);
}
