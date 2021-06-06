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
static boolean nowait;

LiVESWidget *assist;

void pop_to_front(LiVESWidget *dialog, LiVESWidget *extra) {
  char *wid = NULL;
  boolean activated = FALSE;
  if (prefs->startup_phase && !LIVES_IS_FILE_CHOOSER_DIALOG(dialog)) {
    if (!mainw->is_ready) {
      gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE); // dont know if this actually does anything...
      gtk_window_set_type_hint(LIVES_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
    }
  }
  if (mainw->splash_window) {
    lives_widget_hide(mainw->splash_window);
  }

  lives_widget_show_all(dialog);
  lives_widget_context_update();

  //if (!prefs->startup_phase || LIVES_IS_FILE_CHOOSER_DIALOG(dialog)) {
  if (capable->has_xdotool == MISSING && capable->has_wmctrl == MISSING) return;
  wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
  activated = activate_x11_window(wid);
  lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);

  if (extra) {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(extra), KEEPABOVE_KEY, dialog);
    if (activated)
      lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(extra),
                                        ACTIVATE_KEY, lives_strdup(wid));
  }
  if (wid) lives_free(wid);
  //}
}


boolean migrate_config(const char *old_vhash, const char *newconfigfile) {
  // on a fresh install, we check if there is an older config file, and if so, migrate it
  uint64_t oldver = atoll(old_vhash);
  /// $HOME/.lives.* files -> $HOME/.local/config/lives/settings.*
  /// then if $HOME/.lives-dir exists, move contents to $HOME/.local/share/lives
  if (oldver > 0 && oldver < 3200000) {
    char *ocfdir = lives_build_path(capable->home_dir, LIVES_DEF_CONFIG_DATADIR_OLD, NULL);
    lives_cp(prefs->configfile, newconfigfile);
    if (lives_file_test(ocfdir, LIVES_FILE_TEST_IS_DIR)) {
      char *fname, *fname2;
      lives_cp_recursive(ocfdir, prefs->config_datadir, FALSE);
      lives_free(ocfdir);
      fname = lives_build_filename(prefs->config_datadir, DEF_KEYMAP_FILE_OLD, NULL);
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        lives_rm(fname);
      }
      lives_free(fname);
      fname = lives_build_filename(prefs->config_datadir, DEF_KEYMAP_FILE2_OLD, NULL); // perkey defs
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        fname2 = lives_build_filename(prefs->config_datadir, DEF_KEYMAP_FILE2, NULL); // perkey defs
        lives_mv(fname, fname2);
        lives_free(fname2);
      }
      lives_free(fname);
      fname = lives_build_filename(prefs->config_datadir, DEF_KEYMAP_FILE3_OLD, NULL); // data connections
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        fname2 = lives_build_filename(prefs->config_datadir, DEF_KEYMAP_FILE3, NULL); // data connections
        lives_mv(fname, fname2);
        lives_free(fname2);
      }
      lives_free(fname);
    }
    return TRUE;
  }
  return FALSE;
}


void cleanup_old_config(uint64_t oldver) {
  if (oldver > 0 && oldver < 3200000) {
    char *cwd = lives_get_current_dir();
    char *oldconfig = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_FILE_OLD, NULL);
    char *oldconfigdir = lives_build_path(capable->home_dir, LIVES_DEF_CONFIG_DATADIR_OLD, NULL);
    if (do_yesno_dialogf(_("The locations of LiVES configuration files have changed.\n"
                           "%s is now %s\nand %s is now %s\nThe files have been copied to the new locations.\n"
                           "\nWould you like me to remove the old files ?\n"),
                         oldconfig, prefs->configfile, oldconfigdir, prefs->config_datadir)) {
      lives_rm(oldconfig);
      lives_free(oldconfig);
      if (!lives_chdir(capable->home_dir, TRUE)) {
        oldconfig = lives_strdup_printf("%s.", LIVES_DEF_CONFIG_FILE_OLD);
        lives_rmglob(oldconfig);
        lives_free(oldconfig);
        oldconfig = lives_strdup_printf("%s-", LIVES_DEF_CONFIG_FILE_OLD);
        lives_rm(oldconfig);
        lives_chdir(cwd, TRUE);
      }
      if (lives_file_test(oldconfigdir, LIVES_FILE_TEST_IS_DIR)) {
        lives_rmdir(oldconfigdir, TRUE);
      }
    }
    lives_free(oldconfig);
    lives_free(oldconfigdir);
  }
}


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
      char *tmp, *keymap_template = lives_build_filename(prefs->prefix_dir, LIVES_DATA_DIR, DEF_KEYMAP_FILE2, NULL);
      if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
      do {
        retval = LIVES_RESPONSE_NONE;
        if (!lives_file_test(keymap_template, LIVES_FILE_TEST_EXISTS)) {
          retval = do_file_notfound_dialog(_("LiVES was unable to find the default keymap file"),
                                           keymap_template);
          if (retval == LIVES_RESPONSE_BROWSE) {
            char *dirx = lives_build_path(prefs->prefix_dir, LIVES_DATA_DIR, NULL);
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

            retval = do_abort_retry_cancel_dialog(tmp);
          }
        } while (retval == LIVES_RESPONSE_RETRY);
        lives_free(keymap_template);
      }
    }
    lives_free(keymap_file);

    /// default keymap
    keymap_file = lives_build_filename(config_datadir, DEF_KEYMAP_FILE3, NULL);
    if (!lives_file_test(keymap_file, LIVES_FILE_TEST_EXISTS)) {
      char *keymap_template = lives_build_filename(prefs->prefix_dir, LIVES_DATA_DIR, DEF_KEYMAP_FILE3, NULL);
      retval = LIVES_RESPONSE_NONE;
      if (lives_file_test(keymap_template, LIVES_FILE_TEST_EXISTS)) {
        lives_cp(keymap_template, keymap_file);
      }
    }
    lives_free(keymap_file);

    devmapdir = lives_build_path(config_datadir, LIVES_DEVICEMAP_DIR, NULL);
    if (!lives_file_test(devmapdir, LIVES_FILE_TEST_IS_DIR)) {
#ifdef ENABLE_OSC
      char *sys_devmap_dir = lives_build_path(prefs->prefix_dir, LIVES_DATA_DIR, LIVES_DEVICEMAP_DIR, NULL);
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
          lives_cp_recursive(sys_devmap_dir, config_datadir, TRUE);
        }
      }
      lives_free(sys_devmap_dir);
#endif
    }
    lives_free(devmapdir);

#ifdef GUI_GTK
    /// stock_icons
    stock_icons_dir = lives_build_path(config_datadir, STOCK_ICON_DIR, NULL);
    if (!lives_file_test(stock_icons_dir, LIVES_FILE_TEST_IS_DIR)) {
      char *sys_stock_icons_dir = lives_build_path(prefs->prefix_dir, LIVES_DATA_DIR, STOCK_ICON_DIR, NULL);
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
          lives_cp_recursive(sys_stock_icons_dir, config_datadir, TRUE);
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
      if (!do_yesno_dialogf(
            _("You have chosen to use your home directory as the LiVES working directory.\n"
              "This is NOT recommended as it will possibly result in the loss of unrelated files.\n"
              "Click %s if you REALLY want to continue, or %s to create or select another directory.\n"),
            STOCK_LABEL_TEXT(YES), STOCK_LABEL_TEXT(NO)))
        return LIVES_RESPONSE_CANCEL;
    } else {
      boolean res;
      char *xdir = lives_markup_escape_text(dirname, -1);
      msg = lives_format_storage_space_string(freespace);
      widget_opts.use_markup = TRUE;
      res = do_yesno_dialogf(
              _("A directory named\n<b>%s</b>\nalready exists. "
                "Do you wish to use this directory ?\n\n(Free space in volume = %s)\n"),
              xdir, msg);
      widget_opts.use_markup = FALSE;
      lives_free(msg); lives_free(xdir);
      if (!res) return LIVES_RESPONSE_CANCEL;
      return LIVES_RESPONSE_OK;
    }
  } else {
    char *xdir = lives_markup_escape_text(dirname, -1);
    widget_opts.use_markup = TRUE;
    msg = lives_strdup_printf(_("A directory named\n<b>%s</b>\nalready exists.\nHowever, LiVES could not write to this directory "
                                "or read its free space.\nClick %s to exit from LiVES, or %s to select another "
                                "location.\n"), xdir, STOCK_LABEL_TEXT(ABORT), STOCK_LABEL_TEXT(RETRY));
    widget_opts.use_markup = FALSE;
    lives_free(xdir);
    do_abort_retry_dialog(msg);
    lives_free(msg);
    return LIVES_RESPONSE_RETRY;
  }
  return LIVES_RESPONSE_OK;
}


static boolean prompt_new_dir(char *dirname, uint64_t freespace, boolean wrtable) {
  boolean res;
  char *xdir = lives_markup_escape_text(dirname, -1);
  widget_opts.use_markup = TRUE;
  if (wrtable) {
    char *fspstr = lives_format_storage_space_string(freespace);
    res = do_yesno_dialogf(_("\nLiVES will create the directory\n<b>%s</b>\n"
                             "Is this OK ?\n\n(Free space in volume = %s)"), xdir, fspstr);
    lives_free(fspstr);
  } else {
    res = do_error_dialogf(_("\nLiVES could not write to the directory\n<b>%s</b>\n"
                             "Please try again and choose a different location.\n"),
                           xdir);
  }
  widget_opts.use_markup = FALSE;
  lives_free(xdir);
  return res;
}


void filename_toolong_error(const char *fname, const char *ftype, size_t max, boolean can_retry) {
  char *rstr, *msg;
  if (can_retry) rstr = lives_strdup_printf(_("\nPlease click %s to select an alternative directory, or Abort to exit immediately"
                          "from LiVES\n"), STOCK_LABEL_TEXT(RETRY), STOCK_LABEL_TEXT(ABORT));
  else rstr = lives_strdup("");

  msg = lives_strdup_printf(_("The name of the %s provided\n(%s)\nis too long (maximum is %d characters)\n%s"),
                            ftype, fname, max, rstr);
  if (can_retry) do_abort_retry_dialog(msg);
  else startup_message_fatal(msg);
  lives_free(msg); lives_free(rstr);
}

void dir_toolong_error(const char *dirname, const char *dirtype, size_t max, boolean can_retry) {
  char *rstr, *msg;
  if (can_retry) rstr = lives_strdup_printf(_("\nPlease click %s to select an alternative directory, or Abort to exit immediately"
                          "from LiVES\n"), STOCK_LABEL_TEXT(RETRY), STOCK_LABEL_TEXT(ABORT));
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
  size_t chklen = strlen(LIVES_DEF_WORK_SUBDIR) + strlen(LIVES_DIR_SEP) * 2;
  char *tmp;

  if (!pdirname || !*pdirname) return LIVES_RESPONSE_RETRY;

  if (lives_strlen(*pdirname) > (PATH_MAX - MAX_SET_NAME_LEN * 2)) {
    do_error_dialog(_("Directory name is too long !"));
    return LIVES_RESPONSE_RETRY;
  }

  // append a dirsep to the end if there isn't one
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
            LIVES_DIR_SEP LIVES_DEF_WORK_SUBDIR LIVES_DIR_SEP, chklen))) {
      tmp = lives_build_path(*pdirname, LIVES_DEF_WORK_SUBDIR, NULL);
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
        if (prompt_existing_dir(*pdirname, freesp, TRUE) == LIVES_RESPONSE_CANCEL) {
          widget_opts.transient = NULL;
          return LIVES_RESPONSE_RETRY;
        }
        widget_opts.transient = NULL;
      } else {
        if (prompt_existing_dir(*pdirname, 0, FALSE) == LIVES_RESPONSE_CANCEL) {
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
        }}}}
  // *INDENT-ON*

  if (!lives_make_writeable_dir(*pdirname)) {
    return do_dir_perm_error(*pdirname, FALSE);
  }

  return LIVES_RESPONSE_OK;
}


LIVES_LOCAL_INLINE boolean confirm_exit(void) {
  return do_yesno_dialog(_("Are you sure you want to quit from LiVES setup ?"));
}

static void workdir_entry_check(LiVESEntry * entry, livespointer data) {
  const char *last_parentdir = NULL, *parentdir;
  const char *mydir = lives_entry_get_text(entry);
  char *mdir;

  if (!*mydir) return;
  mdir = lives_strdup(mydir);
  mdir[lives_strlen(mdir) - 1] = 0;

  parentdir = get_dir(mdir);
  if (!parentdir) {
    lives_free(mdir);
    return;
  }
  if (!dirs_equal(last_parentdir, parentdir)) {
    if (!lives_file_test(parentdir, LIVES_FILE_TEST_IS_DIR)) {
      show_warn_image(LIVES_WIDGET(entry), _("WARNING: The parent directory does not exist !"));
    } else {
      hide_warn_image(LIVES_WIDGET(entry));
    }
  }
  last_parentdir = lives_strdup(parentdir);
  lives_free(mdir);
}


boolean do_workdir_query(void) {
  _entryw *wizard = create_rename_dialog(6);
  char *dirname = NULL, *mp;
  LiVESResponseType response;

  /// override FILESEL_TYPE_KEY, in case it was set to WORKDIR; we will do our own checking here
  SET_INT_DATA(wizard->dirbutton, FILESEL_TYPE_KEY, LIVES_DIR_SELECTION_WORKDIR_INIT);

  pop_to_front(wizard->dialog, wizard->dirbutton);

  guess_font_size(wizard->dialog, LIVES_LABEL(wizard->xlabel), LIVES_LABEL(wizard->ylabel), .8);
  if (!mainw->first_shown) {
    guess_font_size(wizard->dialog, LIVES_LABEL(wizard->xlabel), LIVES_LABEL(wizard->ylabel), .22);
  }

  lives_entry_set_editable(LIVES_ENTRY(wizard->entry), TRUE);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(wizard->entry), LIVES_WIDGET_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(workdir_entry_check), NULL);

  do {
    lives_freep((void **)&dirname);
    response = lives_dialog_run(LIVES_DIALOG(wizard->dialog));
    if (response == LIVES_RESPONSE_CANCEL) {
      lives_widget_hide(wizard->dialog);
      if (confirm_exit()) {
        lives_widget_destroy(wizard->dialog);
        return FALSE;
      }

      lives_widget_show(wizard->dialog);

      // restore stack pos after showing file chooser
      if (!mainw->is_ready) restore_wm_stackpos(LIVES_BUTTON(wizard->dirbutton));
    }
    // TODO: should we convert to locale encoding ??
    dirname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(wizard->entry)));
  } while (response == LIVES_RESPONSE_CANCEL
           || (lives_strcmp(dirname, prefs->workdir) &&
               check_workdir_valid(&dirname, LIVES_DIALOG(wizard->dialog), TRUE)
               == LIVES_RESPONSE_RETRY));
  lives_widget_destroy(wizard->dialog);

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
  lives_snprintf(prefs->backend, PATH_MAX * 4,
                 "%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL,
                 capable->backend_path, prefs->workdir, prefs->configfile);
  lives_snprintf(prefs->backend_sync, PATH_MAX * 4, "%s", prefs->backend);

  set_string_pref_priority(PREF_WORKING_DIR, prefs->workdir);
  set_string_pref(PREF_WORKING_DIR_OLD, prefs->workdir);

  mainw->has_session_workdir = FALSE;

  lives_free(dirname);

  return TRUE;
}


#ifdef ENABLE_JACK
static void chk_setenv_conf(LiVESToggleButton * b, livespointer data) {
  // warn if setting this for audio client and is already set for trans client, and trans
  // client is enabled, and !jack_srv_dup
  if (prefs->jack_srv_dup) return;
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(b))) {
    boolean is_trans = LIVES_POINTER_TO_INT(data);
    if (!is_trans) {
      if (future_prefs->jack_opts & (JACK_OPTS_SETENV_TSERVER | JACK_OPTS_ENABLE_TCLIENT))
        show_warn_image(LIVES_WIDGET(b), NULL);
    } else {
      if (future_prefs->jack_opts & JACK_OPTS_SETENV_ASERVER) {
        show_warn_image(LIVES_WIDGET(b), NULL);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void chk_jack_cfgx(LiVESWidget * w, LiVESEntry * e) {
  const char *server_cfgx = lives_entry_get_text(LIVES_ENTRY(e));
  if (lives_file_test(server_cfgx, LIVES_FILE_TEST_EXISTS)) {
    char *srv_name = jack_parse_script(server_cfgx);
    hide_warn_image(w);
    if (srv_name) lives_entry_set_text(e, srv_name);
    lives_free(srv_name);
    return;
  } else {
    show_warn_image(w, _("The specified file does not exist"));
    return;
  }
}


boolean prompt_for_jack_ports(boolean is_setup) {
  LiVESWidget *dialog, *dialog_vbox, *layout, *incombo, *outcombo, *auxcombo;
  LiVESWidget *hbox, *cbr;
  LiVESList *inp_list, *outp_list;
  char *title = _("Port configuration for jack audio");
  dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  widget_opts.use_markup = TRUE;
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Please use the options below to define the "
                         "basic initial settings for jackd.\n"
                         "<b>After the setup process, you can review and update the full settings "
                         "from within Preferences / Jack Integration\n</b>"
                         "Most of the time the default values should be fine.\n"), TRUE);
  widget_opts.use_markup = FALSE;

  lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);
  lives_layout_add_row(LIVES_LAYOUT(layout));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  inp_list = jack_get_inport_clients();
  outp_list = jack_get_outport_clients();

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  incombo = lives_standard_combo_new(_("Input ports"), outp_list, LIVES_BOX(hbox),
                                     H_("This is the source that LiVES will use when 'playing' external audio."));
  lives_combo_set_active_string(LIVES_COMBO(incombo), jack_get_best_client(JACK_PORT_TYPE_DEF_IN, outp_list));

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  outcombo = lives_standard_combo_new(_("Output ports"), inp_list, LIVES_BOX(hbox),
                                      H_("Defines the (default) ports which LiVES will connect to when playing audio"));
  lives_combo_set_active_string(LIVES_COMBO(outcombo), jack_get_best_client(JACK_PORT_TYPE_DEF_OUT, inp_list));

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  auxcombo = lives_standard_combo_new(_("Aux ports"), outp_list, LIVES_BOX(hbox),
                                      H_("Defines the ports which LiVES will connect to as an auxiliary audio source\n"
                                         "The default is to connect to microphone for voiceovers"));
  lives_combo_set_active_string(LIVES_COMBO(auxcombo), jack_get_best_client(JACK_PORT_TYPE_AUX_IN, outp_list));

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  cbr = lives_standard_check_button_new(_("Exclusive routing during external playback"),
                                        future_prefs->jack_opts & JACK_OPTS_NO_READ_AUTOCON
                                        ? FALSE : TRUE, LIVES_BOX(hbox),
                                        H_("If set, during external playback LiVES will temporarily disconnect\nany direct "
                                            "connection between input ports and output ports which would otherwise bypass LiVES"));

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
                                     LIVES_STOCK_LABEL_NEXT, LIVES_RESPONSE_OK);

  if (is_setup) pop_to_front(dialog, NULL);

  lives_dialog_run(LIVES_DIALOG(dialog));

  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cbr)))
    future_prefs->jack_opts |= JACK_OPTS_NO_READ_AUTOCON;

  prefs->jack_inport_client = lives_strdup(lives_combo_get_active_text(LIVES_COMBO(incombo)));
  prefs->jack_outport_client = lives_strdup(lives_combo_get_active_text(LIVES_COMBO(outcombo)));
  prefs->jack_auxport_client = lives_strdup(lives_combo_get_active_text(LIVES_COMBO(auxcombo)));

  lives_widget_destroy(dialog);

  lives_list_free_all(&inp_list);
  lives_list_free_all(&outp_list);

  return TRUE;
}


boolean do_jack_config(int type, boolean is_trans) {
  LiVESSList *rb_group;
  LiVESWidget *dialog, *dialog_vbox, *layout, *hbox, *cb2 = NULL, *cb3;
  LiVESWidget *acdef = NULL, *acname = NULL, *astart = NULL;
  LiVESWidget *asdef, *asname;
  LiVESWidget *dfa_button, *okbutton, *cancelbutton, *filebutton;
  LiVESWidget *scrpt_rb = NULL, *cfg_entry = NULL;
  LiVESResponseType response;
  char *title, *text;
  char *server_cfgx;
  int woph = widget_opts.packing_height;
  int old_fprefs = future_prefs->jack_opts;
  boolean cfg_exists = FALSE;
  boolean usedefsrv = FALSE;
  boolean is_setup = FALSE;
  boolean is_test = FALSE;
  boolean do_astart = FALSE;
  // type 0 = from prefs, 1 = setup, 2 = from error dialog

  if (type) is_setup = TRUE;

  cfg_exists = jack_get_cfg_file(is_trans, &server_cfgx);

set_config:
  rb_group = NULL;

  if (type == 1) title = _("Initial configuration for jack audio");
  title = lives_strdup_printf(_("Server and driver configuration for %s"),
                              is_trans ? _("jack transport") : _("jack audio"));

  dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  if (type == 1) {
    widget_opts.use_markup = TRUE;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Please use the options below to define the "
                           "basic initial settings for jackd.\n"
                           "<b>After the setup process, you can review and update the full settings "
                           "from within Preferences / Jack Integration\n</b>Most of the time the default values should be fine.\n"), TRUE);
    widget_opts.use_markup = FALSE;

    lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);
    lives_layout_add_row(LIVES_LAYOUT(layout));
  }

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  if (type) {
    widget_opts.use_markup = TRUE;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("<big><b>Connecting to a Server...</b></big>\n<small>(LiVES will always "
                           "try this first)</small>"), TRUE);
    widget_opts.use_markup = FALSE;


    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    acdef =
      lives_standard_check_button_new(_("Connect to the default server"),
                                      !*future_prefs->jack_aserver_cname, LIVES_BOX(hbox),
                                      H_("The server name will be taken from the environment "
                                         "variable\n$JACK_DEFAULT_SERVER.\nIf that variable is not "
                                         "set, then the name 'default' will be used instead"));

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);

    acname = lives_standard_entry_new(_("Connect to a specific server"), *prefs->jack_aserver_cname
                                      ? prefs->jack_aserver_cname : JACK_DEFAULT_SERVER_NAME,
                                      -1, JACK_PARAM_STRING_MAX, LIVES_BOX(hbox), NULL);

    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(acdef), acname, TRUE);

    if (type == 1 && cfg_exists && !*prefs->jack_aserver_cname) {
      char *srvname = jack_parse_script(server_cfgx);
      if (srvname) {
        lives_entry_set_text(LIVES_ENTRY(acname), srvname);
        lives_free(srvname);
        //lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(acdef), FALSE);
      }
    }

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

    lives_layout_add_label(LIVES_LAYOUT(layout), _("Action to take if the connection fails..."), FALSE);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    dfa_button = lives_standard_radio_button_new(_("...do nothing"), &rb_group, LIVES_BOX(hbox),
                 H_("With this setting active, LiVES will only ever attempt "
                    "to connect to an existing jack server, "
                    "and will never try to start one itself\n"
                    "If the connection attempt does fail, an error will be generated."));

    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  }

  if ((!is_trans && (prefs->jack_opts & JACK_OPTS_START_ASERVER))
      || (is_trans && (prefs->jack_opts & JACK_OPTS_START_TSERVER)))
    do_astart = TRUE;

  lives_layout_add_row(LIVES_LAYOUT(layout));

  if (!is_setup) {
    text = lives_strdup_printf(_("In the event that LiVES fails to connect to %s, an attempt will be made to start "
                                 "a new server using the settings below:"), (is_trans && *prefs->jack_tserver_cname)
                               ? prefs->jack_tserver_cname : (!is_trans && *prefs->jack_aserver_cname)
                               ? prefs->jack_aserver_cname : _("the default server"));
    lives_layout_add_label(LIVES_LAYOUT(layout), text, FALSE);
    lives_free(text);
    lives_layout_add_row(LIVES_LAYOUT(layout));

    lives_layout_add_label(LIVES_LAYOUT(layout), (_("(if the new server fails to start, "
                           "manual intervention will be required)")), TRUE);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  scrpt_rb = lives_standard_radio_button_new(is_setup ? _("..._run the following script (once) and try again:")
             : _("Run this script and retry connecting"),
             &rb_group, LIVES_BOX(hbox), H_("With this setting active, if the first connection "
                 "attempt fails,\nLiVES will run the specified "
                 "jack startup script and try again to connect.\n"
                 "Should the second attempt also fail, an error will be generated"));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  cfg_entry = lives_standard_fileentry_new(NULL, server_cfgx, capable->home_dir,
              MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);

  filebutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cfg_entry), BUTTON_KEY);

  SET_INT_DATA(filebutton, FILESEL_TYPE_KEY, LIVES_FILE_SELECTION_OPEN | LIVES_SELECTION_SHOW_HIDDEN);

  lives_entry_set_editable(LIVES_ENTRY(cfg_entry), TRUE);

  if (!cfg_exists) {
    show_warn_image(cfg_entry, _("The specified file does not exist"));
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cfg_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(chk_jack_cfgx), LIVES_ENTRY(acname));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(scrpt_rb), cfg_entry, FALSE);

  if (!is_setup) lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  astart = lives_standard_radio_button_new(is_setup ? _("..._try to start the jack server using the values below:")
           : _("Start a server using these values"), &rb_group,
           LIVES_BOX(hbox),
           H_("With this setting active, should the connection attempt fail,\nLiVES will try to start up a jackd server itself,\n"
              "using the values defined below."));

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  if (is_setup) lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  if (!is_setup) widget_opts.packing_height <<= 1;

  ////

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  if (!(prefs->jack_opts & JACK_INFO_TEMP_NAMES)
      && (is_setup || (!is_setup && ((is_trans && !*future_prefs->jack_tserver_sname)
                                     || (!is_trans && !*future_prefs->jack_aserver_sname)))))
    usedefsrv = TRUE;

  asdef = lives_standard_check_button_new(_("Use '_default' server name"), usedefsrv, LIVES_BOX(hbox),
                                          H_("The server name will be taken from the environment "
                                              "variable\n$JACK_DEFAULT_SERVER.\nIf that variable is not "
                                              "set, then 'default' will be used instead"));

  if (!is_setup) hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  else {
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    lives_layout_hbox_new(LIVES_LAYOUT(layout));
  }

  //lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
  asname = lives_standard_entry_new(_("Use _custom server name"),
                                    (is_trans && *future_prefs->jack_tserver_sname)
                                    ? future_prefs->jack_tserver_sname :
                                    (!is_trans && *future_prefs->jack_aserver_sname)
                                    ? future_prefs->jack_aserver_sname :
                                    JACK_DEFAULT_SERVER_NAME, -1, JACK_PARAM_STRING_MAX,
                                    LIVES_BOX(hbox), NULL);

  if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) show_warn_image(asname, _("Value was set from the commandline"));

  if (!is_trans) {
    if ((is_setup && cfg_exists) || (!is_setup && *future_prefs->jack_aserver_cfg))
      lives_toggle_button_set_active(scrpt_rb, TRUE);
    else if (type) {
      if (do_astart) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(astart), TRUE);
      else lives_toggle_button_set_active(dfa_button, TRUE);
      if (is_setup) {
        if (*prefs->jack_aserver_sname) lives_toggle_button_set_active(asdef, FALSE);
      } else {
        if (*future_prefs->jack_aserver_sname) lives_toggle_button_set_active(asdef, FALSE);
      }
    }
  } else {
    if ((is_setup && cfg_exists) || (!is_setup && *future_prefs->jack_tserver_cfg))
      lives_toggle_button_set_active(scrpt_rb, TRUE);
    else if (type) {
      if (do_astart) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(astart), TRUE);
      else lives_toggle_button_set_active(dfa_button, TRUE);
      if (is_setup) {
        if (*prefs->jack_tserver_sname) lives_toggle_button_set_active(asdef, FALSE);
      } else {
        if (*future_prefs->jack_tserver_sname) lives_toggle_button_set_active(asdef, FALSE);
      }
    }
  }

  if (!type) {
    // warn if setting this for audio client and is already set for trans client, and trans
    // client is enabled, and !jack_srv_dup
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    cb2 = lives_standard_check_button_new(_("Set as _user default"),
                                          (is_trans && (future_prefs->jack_opts
                                              & JACK_OPTS_SETENV_TSERVER))
                                          || (!is_trans && (future_prefs->jack_opts
                                              & JACK_OPTS_SETENV_ASERVER))
                                          ? TRUE : FALSE, LIVES_BOX(hbox),
                                          H_("If checked, the specified server name will be exported "
                                              "as\n$JACK_DEFAULT_SERVER,\nwhich will cause other jack "
                                              "clients in the same process environment\n"
                                              "to also use the exported value as their "
                                              "default server name"));
    text = lives_strdup_printf(_("WARNING: this option is already enabled for the %s client\n"
                                 "Enabling this option for multiple client types may produce undesired results"),
                               is_trans ? _("audio") : ("transport"));
    show_warn_image(cb2, text);
    lives_free(text);
    hide_warn_image(cb2);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cb2), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(chk_setenv_conf),
                                    LIVES_INT_TO_POINTER(is_trans));
    chk_setenv_conf(cb2, LIVES_INT_TO_POINTER(is_trans));
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(asdef), cb2, TRUE);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(scrpt_rb), cb2, TRUE);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  cb3 = lives_standard_check_button_new(_("Create as temporary server"),
                                        is_trans ? !(future_prefs->jack_opts & JACK_OPTS_PERM_TSERVER) :
                                        !(future_prefs->jack_opts & JACK_OPTS_PERM_ASERVER),
                                        LIVES_BOX(hbox), H_("Checking this will cause the server "
                                            "to automatically shut down\n"
                                            "when there are no longer any clients connected to it."));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(astart), layout, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(asdef), asname, TRUE);

  if (!is_setup) {
    LiVESWidget *advbutton;

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);

    advbutton =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
          _("Advanced Server Configuration"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

    if (is_trans) {
      if (prefs->jack_tsparams)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_server_config),
                                  (livespointer)prefs->jack_tsparams);
      else {
        lives_widget_set_sensitive(advbutton, FALSE);
        show_warn_image(advbutton, _("Server settings can only be adjusted for servers which were started by LiVES"));
      }
    } else {
      if (prefs->jack_asparams)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_server_config),
                                  (livespointer)prefs->jack_asparams);
      else {
        lives_widget_set_sensitive(advbutton, FALSE);
        show_warn_image(advbutton, _("Server settings can only be adjusted for servers which were started by LiVES"));
      }
    }

    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);

    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(astart), hbox, FALSE);

    advbutton =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
          _("Advanced Driver Configuration"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

    if (is_trans) {
      if (prefs->jack_tdrivers)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_drivers_config),
                                  LIVES_INT_TO_POINTER(2));
      else {
        lives_widget_set_sensitive(advbutton, FALSE);
        show_warn_image(advbutton, _("Driver configuration can only be set for servers which were started by LiVES"));
      }
    } else {
      if (prefs->jack_adrivers)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_drivers_config),
                                  LIVES_INT_TO_POINTER(3));
      else {
        lives_widget_set_sensitive(advbutton, FALSE);
        show_warn_image(advbutton, _("Driver configuration can only be set for servers which were started by LiVES"));
      }
    }
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  }

  widget_opts.packing_height = woph;

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  if (type == 1)
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
                   LIVES_STOCK_LABEL_BACK, LIVES_RESPONSE_CANCEL);
  else {
    if (type == 2)
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_QUIT,
                     _("Exit LiVES without updating"), LIVES_RESPONSE_CANCEL);
    else
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL,
                     NULL, LIVES_RESPONSE_CANCEL);
  }

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  if (type == 2) lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_DIALOG_QUESTION,
        _("_Test"), LIVES_RESPONSE_RETRY);

  if (type == 1)
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
               _("Test the Configuration"), LIVES_RESPONSE_RETRY);
  else {
    if (type == 2)
      okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
                 _("_Use these Settings"), LIVES_RESPONSE_OK);
    else
      okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK,
                 NULL, LIVES_RESPONSE_OK);
  }

  if (is_setup) lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_EDGE);

  lives_button_grab_default_special(okbutton);

retry:
  if (is_setup) pop_to_front(dialog, NULL);

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  if (response == LIVES_RESPONSE_RETRY) {
    is_test = TRUE;
    response = LIVES_RESPONSE_OK;
  }

  if (response == LIVES_RESPONSE_OK) {
    boolean ignore = FALSE;
    if (is_setup) {
      future_prefs->jack_opts = 0;
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(astart)) ||
          lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(scrpt_rb))) {
        future_prefs->jack_opts |= JACK_OPTS_START_ASERVER;
        if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(scrpt_rb))) {
          const char *server_cfg = lives_entry_get_text(LIVES_ENTRY(cfg_entry));

          if (!lives_file_test(server_cfg, LIVES_FILE_TEST_EXISTS)) {
            if (!do_jack_nonex_warn(server_cfg)) {
              goto retry;
            }
          }
          pref_factory_string(PREF_JACK_ACONFIG, server_cfg, TRUE);
          pref_factory_string(PREF_JACK_TCONFIG, server_cfg, TRUE);
        } else {
          pref_factory_string(PREF_JACK_ACONFIG, "", TRUE);
          pref_factory_string(PREF_JACK_TCONFIG, "", TRUE);
        }
      } else ignore = TRUE;
    } else {
      // !setup
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(scrpt_rb))) {
        const char *server_cfg = lives_entry_get_text(LIVES_ENTRY(cfg_entry));
        if (is_trans) {
          if (prefs->jack_srv_dup || lives_strcmp(server_cfg, prefs->jack_aserver_cfg))
            lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", server_cfg);
        } else {
          if (prefs->jack_srv_dup || lives_strcmp(server_cfg, prefs->jack_tserver_cfg))
            lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", server_cfg);
        }
      } else {
        if (!is_trans) *future_prefs->jack_aserver_cfg = 0;
        else *future_prefs->jack_tserver_cfg = 0;
      }
    }
    if (is_setup) {
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(acdef))) {
        pref_factory_string(PREF_JACK_ACSERVER, "", TRUE);
        pref_factory_string(PREF_JACK_TCSERVER, "", TRUE);
      } else {
        pref_factory_string(PREF_JACK_ACSERVER, lives_entry_get_text(LIVES_ENTRY(acname)), TRUE);
        pref_factory_string(PREF_JACK_TCSERVER, lives_entry_get_text(LIVES_ENTRY(acname)), TRUE);
      }
      if (ignore || lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(asdef))) {
        pref_factory_string(PREF_JACK_ASSERVER, "", TRUE);
        pref_factory_string(PREF_JACK_TSSERVER, "", TRUE);
      } else {
        pref_factory_string(PREF_JACK_ASSERVER, lives_entry_get_text(LIVES_ENTRY(asname)), TRUE);
        pref_factory_string(PREF_JACK_TSSERVER, lives_entry_get_text(LIVES_ENTRY(asname)), TRUE);
      }
    } else {
      if (type) {
        if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(acdef))) {
          if (!is_trans) *future_prefs->jack_aserver_cname = 0;
          else *future_prefs->jack_tserver_cname = 0;
        } else {
          if (!is_trans) lives_snprintf(future_prefs->jack_aserver_cname, JACK_PARAM_STRING_MAX,
                                          "%s", lives_entry_get_text(LIVES_ENTRY(acname)));
          else lives_snprintf(future_prefs->jack_tserver_cname, JACK_PARAM_STRING_MAX,
                                "%s", lives_entry_get_text(LIVES_ENTRY(acname)));
        }
      }
    }
    if (!is_setup && !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(asdef))) {
      if (!is_trans)
        lives_snprintf(future_prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX,
                       "%s", lives_entry_get_text(LIVES_ENTRY(asname)));
      else
        lives_snprintf(future_prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX,
                       "%s", lives_entry_get_text(LIVES_ENTRY(asname)));
    }
    if (!ignore) {
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb3))) {
        if (!is_trans) future_prefs->jack_opts &= ~JACK_OPTS_PERM_ASERVER;
        else future_prefs->jack_opts &= ~JACK_OPTS_PERM_TSERVER;
      } else {
        if (!is_trans) future_prefs->jack_opts |= JACK_OPTS_PERM_ASERVER;
        else future_prefs->jack_opts |= JACK_OPTS_PERM_TSERVER;
      }
      if (cb2) {
        if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb2))) {
          if (!is_trans) future_prefs->jack_opts |= JACK_OPTS_SETENV_ASERVER;
          else future_prefs->jack_opts |= JACK_OPTS_SETENV_TSERVER;
        } else {
          if (!is_trans) future_prefs->jack_opts &= ~JACK_OPTS_SETENV_ASERVER;
          else future_prefs->jack_opts &= ~JACK_OPTS_SETENV_TSERVER;
        }
      }
      if (!is_trans && prefs->jack_srv_dup) {
        if (!is_setup) {
          lives_snprintf(future_prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX,
                         "%s", future_prefs->jack_aserver_sname);
        }
        if ((future_prefs->jack_opts & JACK_OPTS_START_ASERVER)
            && (future_prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT))
          future_prefs->jack_opts |= JACK_OPTS_START_TSERVER;
        else
          future_prefs->jack_opts &= ~JACK_OPTS_START_TSERVER;
        if (future_prefs->jack_opts & JACK_OPTS_PERM_ASERVER)
          future_prefs->jack_opts |= JACK_OPTS_PERM_TSERVER;
        else
          future_prefs->jack_opts &= ~JACK_OPTS_PERM_TSERVER;
        if (future_prefs->jack_opts & JACK_OPTS_SETENV_ASERVER)
          future_prefs->jack_opts |= JACK_OPTS_SETENV_TSERVER;
        else
          future_prefs->jack_opts &= ~JACK_OPTS_SETENV_TSERVER;
      }
    }
    if (is_setup) {
      prefs->jack_opts = future_prefs->jack_opts;
      if (!(prefs->jack_opts & JACK_OPTS_START_ASERVER)) {
        char *srvname, *tmp;
        future_prefs->jack_opts = prefs->jack_opts = 0;
        if (*prefs->jack_aserver_cname) {
          srvname = lives_strdup_printf(_("jack server '%s'"), (tmp = lives_markup_escape_text(prefs->jack_aserver_cname, -1)));
          lives_free(tmp);
        } else
          srvname = _("the default jack server");
        widget_opts.use_markup = TRUE;
        if (!do_warning_dialogf(_("You have chosen <b>only</b> to connect to %s\nPlease ensure that the server "
                                  "is running before clicking %s\n"
                                  "Alternatively, click %s to change the jack client options\n"),
                                srvname, STOCK_LABEL_TEXT(OK), STOCK_LABEL_TEXT(CANCEL))) {
          widget_opts.use_markup = FALSE;
          lives_free(srvname);
          rb_group = NULL;
          lives_widget_destroy(dialog);
          goto set_config;
        }
        widget_opts.use_markup = FALSE;
        lives_free(srvname);
        future_prefs->jack_opts = prefs->jack_opts = 0;
      }
      pref_factory_int(PREF_JACK_OPTS, NULL, prefs->jack_opts, TRUE);
    } else {
      if (!type) {
        /* g_print("CEHCK %d %d %s and %s, %s and %s\n", future_prefs->jack_opts,  old_fprefs, */
        /* 	      future_prefs->jack_aserver_sname, prefs->jack_aserver_sname, */
        /* 	      future_prefs->jack_aserver_cfg, prefs->jack_aserver_cfg); */

        if (future_prefs->jack_opts != old_fprefs
            || (is_trans && (lives_strcmp(future_prefs->jack_tserver_sname, prefs->jack_tserver_sname)
                             || lives_strcmp(future_prefs->jack_tserver_cfg, prefs->jack_tserver_cfg)))
            || (!is_trans && (lives_strcmp(future_prefs->jack_aserver_sname, prefs->jack_aserver_sname)
                              || lives_strcmp(future_prefs->jack_aserver_cfg, prefs->jack_aserver_cfg))))
          apply_button_set_enabled(NULL, NULL);
        if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(asdef)) != usedefsrv) {
          apply_button_set_enabled(NULL, NULL);
        }
      }
    }
    if (is_test) future_prefs->jack_opts |= JACK_INFO_TEST_SETUP;
    lives_widget_destroy(dialog);
    lives_free(server_cfgx);
    return TRUE;
  }
  lives_free(server_cfgx);
  lives_widget_destroy(dialog);
  if (is_setup) prefs->jack_opts = 0;
  else future_prefs->jack_opts = old_fprefs;
  return FALSE;
}
#endif


static void on_init_aplayer_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  int audp = LIVES_POINTER_TO_INT(user_data);

  if (!lives_toggle_button_get_active(tbutton)) return;

  prefs->audio_player = audp;
}


boolean do_audio_choice_dialog(short startup_phase) {
  LiVESWidget *dialog, *dialog_vbox, *radiobutton2 = NULL, *radiobutton3, *label;
  LiVESWidget *okbutton, *cancelbutton;
  LiVESWidget *hbox, *layout;

#ifdef HAVE_PULSE_AUDIO
  LiVESWidget *radiobutton0;
#endif

#ifdef ENABLE_JACK
  LiVESWidget *radiobutton1;
#endif

  LiVESSList *radiobutton_group = NULL;

  char *txt0, *txt1, *txt2, *txt3, *txt4, *txt5, *txt6, *txt7, *msg;
  char *recstr;

  LiVESResponseType response;

  if (startup_phase == 4) {
    txt0 = lives_big_and_bold(_("LiVES FAILED TO START YOUR SELECTED AUDIO PLAYER !\n\n"));
  } else {
    if (startup_phase != 5) prefs->audio_player = -1;
    txt0 = lives_strdup("");
  }

#ifdef ENABLE_JACK
reloop:
#endif

  txt1 = lives_strdup(
           _("Before starting LiVES, you need to choose an audio player.\n\n"
             "<big><b>PULSE AUDIO</b></big> is recommended for most users"));

#ifndef HAVE_PULSE_AUDIO
  txt2 = (_(", but this version of LiVES was not compiled with pulse audio support.\n\n"));
#else
  if (!capable->has_pulse_audio) {
    txt2 = lives_strdup(
             _(", but you do not have pulseaudio installed on your system.\n "
               "If you wish to use pulseaudio, you should Cancel and install that first\n"
               "before running LiVES.\n\n"));
  } else txt2 = lives_strdup(".\n\n");
#endif

  txt3 = (_("<big><b>JACK</b></big> audio is recommended for pro users"));

#ifndef ENABLE_JACK
  txt4 = (_(", but this version of LiVES was not compiled with jack audio support.\n\n"));
#else
  if (!capable->has_jackd) {
    txt4 = (_(", but you do not have jackd installed.\n"
              "If you wish to use jack you should Cancel and install jackd first before running LiVES.\n\n"));
  } else {
    txt4 = lives_strdup(
             _(", but may prevent LiVES from starting on some systems.\nIf LiVES will not start with jack,"
               "you can restart LiVES and try another audio player instead.\n\n"));
  }
#endif

  txt5 = (_("<big><b>SOX</b></big> may be used if neither of the preceding players work, "));

  if (capable->has_sox_play) {
    txt6 = (_("but many audio features will be disabled.\n\n"));
  } else {
    txt6 = (_("but you do not have sox installed.\n"
              "If you wish to use sox, you should Cancel and install it before running LiVES.\n\n"));
  }

  txt7 = (_("<big><b>NONE</b></big> If you are not intending to use LiVES at all for audio, you may select this option\n"
            "However, be aware this feature is somewhat experimental and should be used with caution\n"
            "as it may give rise to occasional buggy behaviour"));

  msg = lives_strdup_printf("%s%s%s%s%s%s%s%s", txt0, txt1, txt2, txt3, txt4, txt5, txt6, txt7);

  lives_free(txt0); lives_free(txt1); lives_free(txt2);
  lives_free(txt3); lives_free(txt4); lives_free(txt5);
  lives_free(txt6); lives_free(txt7);

  dialog = lives_standard_dialog_new(_("Choose the initial audio player"), FALSE, -1, -1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new(msg);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  widget_opts.use_markup = FALSE;
  lives_free(msg);

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  recstr = lives_strdup_printf("<b>(%s)</b>", mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);

  widget_opts.packing_height <<= 1;
  layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  widget_opts.packing_height >>= 1;

#ifdef HAVE_PULSE_AUDIO
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  widget_opts.use_markup = TRUE;
  radiobutton0 =
    lives_standard_radio_button_new(_("Use _<b>pulseaudio</b> player"),
                                    &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;
  if (!capable->has_pulse_audio) lives_widget_set_sensitive(radiobutton0, FALSE);
  else if (prefs->audio_player == -1) prefs->audio_player = AUD_PLAYER_PULSE;
  if (capable->has_pulse_audio) {
    widget_opts.use_markup = TRUE;
    lives_layout_add_label(LIVES_LAYOUT(layout), recstr, TRUE);
    widget_opts.use_markup = FALSE;
  }
#endif

#ifdef ENABLE_JACK
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  widget_opts.use_markup = TRUE;
  radiobutton1 =
    lives_standard_radio_button_new(_("Use _<b>jack</b> audio player"),
                                    &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;
  if (!capable->has_jackd) lives_widget_set_sensitive(radiobutton1, FALSE);
  else if (!capable->has_pulse_audio) {
    widget_opts.use_markup = TRUE;
    lives_layout_add_label(LIVES_LAYOUT(layout), recstr, TRUE);
    widget_opts.use_markup = FALSE;
  }
  msg = lives_strdup_printf(_("(click %s to view and adjust initial server settings)"),
                            STOCK_LABEL_TEXT(NEXT));
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);
#endif

  lives_free(recstr);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_opts.use_markup = TRUE;
  radiobutton2 = lives_standard_radio_button_new(_("Use _<b>sox</b> audio player"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;
  if (capable->has_sox_play) {
    if (prefs->audio_player == -1) prefs->audio_player = AUD_PLAYER_SOX;
  } else lives_widget_set_sensitive(radiobutton2, FALSE);

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE || (capable->has_pulse_audio && prefs->audio_player == -1)) {
    prefs->audio_player = AUD_PLAYER_PULSE;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton0), TRUE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
  }
#endif
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK || prefs->audio_player == -1) {
    prefs->audio_player = AUD_PLAYER_JACK;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1), TRUE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
  }
#endif
  if (capable->has_sox_play) {
    if (prefs->audio_player == AUD_PLAYER_SOX || prefs->audio_player == -1) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton2), TRUE);
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
    }
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_opts.use_markup = TRUE;
  radiobutton3 = lives_standard_radio_button_new(_("Use _<b>none</b> audio player"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;

  add_fill_to_box(LIVES_BOX(dialog_vbox));

#ifdef HAVE_PULSE_AUDIO
  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton0), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                            LIVES_INT_TO_POINTER(AUD_PLAYER_PULSE));
#endif

#ifdef ENABLE_JACK
  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton1), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                            LIVES_INT_TO_POINTER(AUD_PLAYER_JACK));
#endif

  if (capable->has_sox_play) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                              LIVES_INT_TO_POINTER(AUD_PLAYER_SOX));
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton3), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                            LIVES_INT_TO_POINTER(AUD_PLAYER_NONE));

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
                 _("Exit Setup"), LIVES_RESPONSE_CANCEL);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
                                     LIVES_STOCK_LABEL_BACK, LIVES_RESPONSE_RETRY);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
             LIVES_STOCK_GO_FORWARD, LIVES_STOCK_LABEL_NEXT, LIVES_RESPONSE_OK);

  if (prefs->audio_player == -1) {
    do_no_mplayer_sox_error();
    return FALSE;
  }

  lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_SPREAD);
  lives_button_grab_default_special(okbutton);
  //lives_widget_grab_focus(okbutton);

  pop_to_front(dialog, NULL);

  if (!mainw->first_shown) {
    if (prefs->startup_phase == 4)
      guess_font_size(dialog, LIVES_LABEL(label), NULL, 1.2);
    else
      guess_font_size(dialog, LIVES_LABEL(label), NULL, 1.12);
  }
  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(label), NULL, .22);
  }

  while (1) {
    response = lives_dialog_run(LIVES_DIALOG(dialog));
    if (response == LIVES_RESPONSE_CANCEL) {
      if (!confirm_exit()) continue;
    }
    break;
  }

  lives_widget_destroy(dialog);
  lives_widget_context_update();

#ifdef ENABLE_JACK
  if (response == LIVES_RESPONSE_OK) {
    switch (prefs->audio_player) {
    case AUD_PLAYER_PULSE:
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
      break;
    case AUD_PLAYER_JACK:
      if (!do_jack_config(1, FALSE)) {
        txt0 = lives_strdup("");
        radiobutton_group = NULL;
        goto reloop;
      }
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
      lives_widget_set_sensitive(mainw->show_jackmsgs, TRUE);
      break;
    case AUD_PLAYER_SOX:
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
      break;
    default:
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_NONE);
      break;
    }
  }
#endif

  if (response == LIVES_RESPONSE_RETRY) {
    if (!do_startup_tests(FALSE)) response = LIVES_RESPONSE_CANCEL;
    else {
      txt0 = lives_strdup("");
      radiobutton_group = NULL;
      goto reloop;
    }
  }

  if (mainw->splash_window) {
    lives_widget_show(mainw->splash_window);
  }

  if (response == LIVES_RESPONSE_OK) {
    if (!is_realtime_aplayer(prefs->audio_player)) {
      lives_widget_hide(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
      lives_widget_hide(mainw->recaudio_submenu);
    }
    return TRUE;
  }
  return FALSE;
}


#define MAX_TESTS 64
static LiVESWidget *test_labels[MAX_TESTS];
static LiVESWidget *test_reslabels[MAX_TESTS];
static LiVESWidget *test_spinners[MAX_TESTS];

// pauses here are just for dramatic effect...

static void prep_test(LiVESWidget * table, int row) {
  LiVESWidget *label = test_reslabels[row];
  if (label) lives_label_set_text(LIVES_LABEL(label), _("Checking..."));
#if LIVES_HAS_SPINNER_WIDGET
  if (test_spinners[row]) {
    lives_spinner_start(LIVES_SPINNER(test_spinners[row]));
  }
#endif
  if (!nowait) {
    for (int i = 0; i < 200; i++) {
      lives_widget_context_update();
      if (mainw->cancelled) break;
      lives_usleep(2000);
    }
  }
}

static void add_test(LiVESWidget * table, int row, const char *ttext) {
  LiVESWidget *label = test_labels[row], *image = NULL;
  boolean add_spinner = FALSE;

  if (!label) {
    label = lives_standard_label_new(ttext);
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
    //lives_widget_show(label);
    test_labels[row] = label;
    if (!test_spinners[row] || !test_reslabels[row]) add_spinner = TRUE;
  } else {
    lives_label_set_text(LIVES_LABEL(label), ttext);
  }

  if (!nowait) {
    label = test_reslabels[row];
    if (add_spinner) {
      if (!label) label = lives_standard_label_new(_("Waiting..."));
#if LIVES_HAS_SPINNER_WIDGET
      if (!test_spinners[row]) {
        image = test_spinners[row] = lives_standard_spinner_new(FALSE);
      }
#endif
    }
  }

  if (!test_reslabels[row]) {
    lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
    //lives_widget_show(label);
    test_reslabels[row] = label;
  }

  if (image) {
    lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  }
  lives_widget_show_all(table);

}


static boolean pass_test(LiVESWidget * table, int row) {
  // TRANSLATORS - as in "passed test"
  LiVESWidget *label = test_reslabels[row];
  char *txt;
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_APPLY, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  set_css_min_size(image, widget_opts.css_min_width, widget_opts.css_min_height);
  if (test_spinners[row]) {
    lives_widget_unparent(test_spinners[row]);
    test_spinners[row] = NULL;
  }

  txt = _("Passed");
  if (!label) label = lives_standard_label_new(txt);
  else lives_label_set_text(LIVES_LABEL(label), txt);
  lives_free(txt);

  if (!test_reslabels[row]) {
    lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
    lives_widget_show(label);
    test_reslabels[row] = label;
  }

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);

  lives_widget_show_all(table);

  if (!nowait) {
    for (int i = 0; i < 100; i++) {
      lives_widget_context_update();
      if (mainw->cancelled) break;
      lives_usleep(2000);
    }
  }
  return TRUE;
}

static LiVESWidget *_fail_test(LiVESWidget * table, int row, char *ftext, const char *type) {
  LiVESWidget *label = test_reslabels[row];
#if GTK_CHECK_VERSION(3, 10, 0)
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_DIALOG_WARNING, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#else
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_CANCEL, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#endif
  LiVESWidget *hbox;
  char *msg;

  if (test_spinners[row]) {
    lives_widget_unparent(test_spinners[row]);
    test_spinners[row] = NULL;
  }

  if (!label) label = lives_standard_label_new(type);
  else lives_label_set_text(LIVES_LABEL(label), type);

  if (!test_reslabels[row]) {
    lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
    test_reslabels[row] = label;
  }

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);

  hbox = lives_hbox_new(FALSE, 0);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), MISC_KEY, (livespointer)image);

  label = lives_standard_label_new(_("checking"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  lives_table_attach(LIVES_TABLE(table), hbox, 3, 4, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show_all(table);

  if (!nowait) {
    for (int i = 0; i < 500; i++) {
      lives_widget_context_update();
      if (mainw->cancelled) break;
      lives_usleep(2000);
    }
  }

  msg = lives_strdup_printf("<b>%s</b>", ftext);
  widget_opts.use_markup = TRUE;
  lives_label_set_text(LIVES_LABEL(label), msg);
  widget_opts.use_markup = FALSE;
  lives_free(msg);

  if (!nowait) {
    for (int i = 0; i < 1500; i++) {
      lives_widget_context_update();
      if (mainw->cancelled) break;
      lives_usleep(2000);
    }
  }
  return hbox;
}

LIVES_LOCAL_INLINE LiVESWidget *fail_test(LiVESWidget * table, int row, char *ftext) {
  allpassed = FALSE;
  return _fail_test(table, row, ftext, _("Failed"));
}

LIVES_LOCAL_INLINE LiVESWidget *skip_test(LiVESWidget * table, int row, char *ftext) {
  return _fail_test(table, row, ftext, _("Skipped"));
}

LIVES_LOCAL_INLINE char *get_resource(char *fname) {
  return lives_build_filename(prefs->prefix_dir, LIVES_DATA_DIR, LIVES_RESOURCES_DIR, fname, NULL);
}

static void quit_from_tests(LiVESWidget * dialog, livespointer button) {
  lives_widget_hide(dialog);
  if (!prefs->startup_phase || confirm_exit()) {
    SET_INT_DATA(dialog, INTENTION_KEY, LIVES_INTENTION_DESTROY);
    mainw->cancelled = CANCEL_USER;
  } else {
    SET_INT_DATA(dialog, INTENTION_KEY, LIVES_INTENTION_UNKNOWN);
    lives_widget_show(dialog);
  }
}

static void back_from_tests(LiVESWidget * dialog, livespointer button) {
  SET_INT_DATA(dialog, INTENTION_KEY, LIVES_INTENTION_UNDO);
  mainw->cancelled = CANCEL_USER;
}

static void skip_tests(LiVESWidget * dialog, livespointer button) {
  nowait = TRUE;
  lives_widget_set_sensitive(LIVES_WIDGET(button), FALSE);
}


static void fix_plugins(LiVESWidget * b, LiVESWidget * table) {
  LiVESWidget *dialog = lives_widget_get_toplevel(b);
  lives_widget_hide(dialog);
  if (check_for_plugins(prefs->lib_dir, FALSE)) {
    LiVESWidget *w = lives_widget_get_parent(b), *image;
    while (!LIVES_IS_TABLE(lives_widget_get_parent(w))) w = lives_widget_get_parent(w);
    lives_widget_set_no_show_all(w, TRUE);
    lives_widget_hide(w);
    image = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(w), MISC_KEY);
    lives_widget_destroy(image);
    lives_widget_show(dialog);
    pass_test(table, 0);
  } else lives_widget_show(dialog);
}


static void fix_prefix(LiVESWidget * b, LiVESWidget * table) {
  LiVESWidget *dialog = lives_widget_get_toplevel(b);
  lives_widget_hide(dialog);
  if (find_prefix_dir(prefs->prefix_dir, FALSE)) {
    LiVESWidget *w = lives_widget_get_parent(b), *image;
    while (!LIVES_IS_TABLE(lives_widget_get_parent(w))) w = lives_widget_get_parent(w);
    lives_widget_set_no_show_all(w, TRUE);
    lives_widget_hide(w);
    image = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(w), MISC_KEY);
    lives_widget_destroy(image);
    lives_widget_show(dialog);
    pass_test(table, 1);
  } else lives_widget_show(dialog);
}


boolean do_startup_tests(boolean tshoot) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox, *hbox;

  LiVESWidget *label, *xlabel = NULL;
  LiVESWidget *table;
  LiVESWidget *resolveb;
  LiVESWidget *okbutton = NULL, *defbutton;
  LiVESWidget *cancelbutton = NULL, *backbutton = NULL;

  char mppath[PATH_MAX];

  char *com, *rname, *afile, *tmp;
  char *image_ext = lives_strdup(prefs->image_ext);
  char *title, *msg;
  char *temp_backend = NULL;

  const char *mp_cmd;
  const char *lookfor;

  uint8_t *abuff;
  ulong quitfunc = 0, backfunc = 0, skipfunc = 0;

  off_t fsize;

  boolean success, success2, success3, success4;
  boolean imgext_switched;
  boolean add_skip = FALSE;
  boolean onowait;

  LiVESResponseType response;
  int res;
  int current_file = mainw->current_file;

  int intent;
  int out_fd, info_fd, testcase = 0;

  nowait = FALSE;

  if (mainw->multitrack) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
    }
    mt_desensitise(mainw->multitrack);
  }

rerun:
  mainw->cancelled = CANCEL_NONE;

  for (int i = 0; i < MAX_TESTS; i++) {
    test_labels[i] = test_reslabels[i] = test_spinners[i] = NULL;
  }

  testcase = 0;
  imgext_switched = FALSE;
  allpassed = TRUE;
  mainw->suppress_dprint = TRUE;
  mainw->cancelled = CANCEL_NONE;

  if (!tshoot) {
    title = (_("Testing Configuration"));
  } else {
    title = (_("Troubleshoot"));
  }

  if (!tshoot)
    dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  else
    dialog = lives_standard_dialog_new(title, FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label = lives_standard_label_new(_("LiVES will now run some basic configuration tests\n"));
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);

  if (!tshoot) {
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, _("Exit Setup"),
                   LIVES_RESPONSE_CANCEL);

    backbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK, _("_Back to Directory Selection"),
                 LIVES_RESPONSE_RETRY);

    backfunc = lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(backbutton), LIVES_WIDGET_CLICKED_SIGNAL,
               LIVES_GUI_CALLBACK(back_from_tests), dialog);

    defbutton = okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD, _("_Skip"),
                           LIVES_RESPONSE_OK);

    skipfunc = lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
               LIVES_GUI_CALLBACK(skip_tests), dialog);

    lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_SPREAD);
  } else {
    defbutton = cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                               LIVES_RESPONSE_CANCEL);
  }
  quitfunc = lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
             LIVES_GUI_CALLBACK(quit_from_tests), dialog);

  lives_button_grab_default_special(defbutton);
  lives_widget_grab_focus(defbutton);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  table = lives_table_new(10, 4, FALSE);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), table);

  add_test(table, testcase, _("Checking for plugin presence"));

  widget_opts.mnemonic_label = FALSE;
  add_test(table, ++testcase, _("Checking for components under 'prefix_dir'"));
  widget_opts.mnemonic_label = TRUE;

  add_test(table, ++testcase, _("Checking for \"sox\" presence"));

  // test if sox can convert raw 44100 -> wav 22050
  add_test(table, ++testcase, _("Checking if sox can convert audio"));

  add_test(table, ++testcase, _("Checking for \"mplayer\", \"mplayer2\" or \"mpv\" presence"));

  add_test(table, ++testcase, _("Checking if ???? can convert audio"));

#ifdef ALLOW_PNG24
  msg = lives_strdup_printf(_("Checking if %s can decode to png"), "????");
#else
  msg = lives_strdup_printf(_("Checking if %s can decode to png/alpha"), "????");
#endif
  add_test(table, ++testcase, msg);
  lives_free(msg);

  onowait = nowait;
  nowait = FALSE;
  add_test(table, ++testcase, NULL);
  nowait = onowait;

  add_test(table, ++testcase, _("Checking if ???? can decode to jpeg"));

  add_test(table, ++testcase, _("Checking for \"convert\" presence"));

  if (!tshoot) {
    msg = lives_strdup_printf(
            _("\n\n\tClick 'Exit Setup' to quit and install any missing components, %s to return to directory selection, "
              "or %s to continue with the setup\t\n"), STOCK_LABEL_TEXT(BACK), STOCK_LABEL_TEXT(NEXT));
    xlabel = lives_standard_label_new(msg);
    lives_container_add(LIVES_CONTAINER(dialog_vbox), xlabel);
    lives_free(msg);
    lives_widget_show(xlabel);
    lives_widget_set_opacity(xlabel, 0.);
  } else add_fill_to_box(LIVES_BOX(dialog_vbox));
  lives_widget_show_all(dialog);

  testcase = 0;

  if (!tshoot) {
    if (!mainw->first_shown) {
      guess_font_size(dialog, LIVES_LABEL(xlabel), NULL, .88);
    }
    if (!mainw->first_shown) {
      guess_font_size(dialog, LIVES_LABEL(xlabel), NULL, 0.18);
    }
    lives_widget_context_update();
  }
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (!tshoot) pop_to_front(dialog, NULL);

  // begin tests... //////

  testcase = 0;

  prep_test(table, testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (!check_for_plugins(prefs->lib_dir, TRUE)) {
    hbox = fail_test(table, testcase, _("Some plugin directories could not be located"));
    widget_opts.use_markup = TRUE;
    resolveb =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
          _("<b>Fix this !</b>"), DEF_BUTTON_WIDTH, -1, LIVES_BOX(hbox), TRUE, NULL);
    widget_opts.use_markup = FALSE;
    //lives_box_pack_start(LIVES_BOX(hbox), resolveb, FALSE, FALSE, widget_opts.packing_width);
    lives_widget_grab_focus(cancelbutton);
    success = FALSE;
    lives_signal_sync_connect(LIVES_GUI_OBJECT(resolveb), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(fix_plugins), table);
  } else {
    success = pass_test(table, testcase);
  }

  prep_test(table, ++testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (!find_prefix_dir(prefs->prefix_dir, TRUE)) {
    hbox = fail_test(table, testcase, _("Some default components such as themes and icons may be missing"));
    widget_opts.use_markup = TRUE;
    resolveb =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
          _("<b>Fix this !</b>"), DEF_BUTTON_WIDTH, -1, LIVES_BOX(hbox), TRUE, NULL);
    widget_opts.use_markup = FALSE;
    //lives_box_pack_start(LIVES_BOX(hbox), resolveb, FALSE, FALSE, widget_opts.packing_width);
    lives_widget_grab_focus(cancelbutton);
    lives_widget_show_all(resolveb);
    success = FALSE;
    lives_signal_sync_connect(LIVES_GUI_OBJECT(resolveb), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(fix_prefix), table);
  } else {
    success = pass_test(table, testcase);
  }

  // check for sox presence

  prep_test(table, ++testcase);

  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (!capable->has_sox_sox) {
    fail_test(table, testcase, _("sox is needed to resample audio and convert between some formats"));
    lives_widget_grab_focus(cancelbutton);
    success = FALSE;
  } else {
    success = pass_test(table, testcase);
  }

  prep_test(table, ++testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

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
      temp_backend = use_staging_dir_for(mainw->current_file);
      com = lives_strdup_printf("%s export_audio \"%s\" 0. 0. 44100 2 16 1 22050 \"%s\"",
                                temp_backend, cfile->handle, afile);
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

  prep_test(table, ++testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (tshoot) {
    lives_snprintf(prefs->image_ext, 16, "%s", image_ext);
    lives_snprintf(prefs->image_type, 16, "%s", image_ext_to_lives_image_type(prefs->image_ext));
  }

  // check for mplayer presence
  success2 = TRUE;

  if (!capable->has_mplayer && !capable->has_mplayer2 && !capable->has_mpv) {
    fail_test(table, testcase,
              _("You should install mplayer, mplayer2 or mpv to be able to use "
                "certain features related to decoding\n(such as loading the audio track "
                "and as a fallback for formats which the decoder plugins cannot handle)"));
    success2 = FALSE;
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

  if (mainw->cancelled != CANCEL_NONE) goto cancld;
  // if present

  // check if mplayer can decode audio

  if (capable->has_mplayer) mp_cmd = EXEC_MPLAYER;
  else if (capable->has_mplayer2) mp_cmd = EXEC_MPLAYER2;
  else mp_cmd = EXEC_MPV;

  get_location(mp_cmd, mppath, PATH_MAX);
  lives_snprintf(prefs->video_open_command, PATH_MAX + 2, "\"%s\"", mppath);
  set_string_pref(PREF_VIDEO_OPEN_COMMAND, prefs->video_open_command);

  onowait = nowait;
  nowait = success2;
  msg = lives_strdup_printf(_("Checking if %s can convert audio"), mp_cmd);
  add_test(table, ++testcase, msg);
  lives_free(msg);
  nowait = onowait;
  prep_test(table, testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  res = 1;

  if (success2) {
    // TODO - add a timeout
#ifndef IS_MINGW
    com = lives_strdup_printf("LANG=en LANGUAGE=en %s -ao help | %s pcm >/dev/null 2>&1",
                              prefs->video_open_command, capable->grep_cmd);
    res = lives_system(com, TRUE);
    lives_free(com);
#else
    com = lives_strdup_printf("%s -ao help | %s pcm >NUL 2>&1",
                              prefs->video_open_command, capable->grep_cmd);
    res = lives_system(com, TRUE);
    lives_free(com);
#endif
  }

  if (res == 0) {
    pass_test(table, testcase);
  } else {
    fail_test(table, testcase, _("You should install mplayer,mplayer2 or mpv with pcm/wav support"));
  }
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

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

  onowait = nowait;
  nowait = success2;
  add_test(table, ++testcase, msg);
  lives_free(msg);
  nowait = onowait;
  prep_test(table, testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  success3 = FALSE;

  if (success2 && !success4) {
    tmp = lives_strdup_printf(_("Resource directory %s not found !"), rname);
    skip_test(table, testcase, tmp);
    lives_free(tmp);

    msg = lives_strdup_printf(_("Checking less rigorously"), mp_cmd);
    onowait = nowait;
    nowait = TRUE;
    add_test(table, ++testcase, msg);
    lives_free(msg);
    nowait = onowait;

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
  } else add_skip = TRUE;

  lives_free(rname);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  // try to open resource vidtest.avi
  if (!success3 && success2 && success4) {
    info_fd = -1;

    lives_rm(cfile->info_file);

    rname = get_resource(LIVES_TEST_VIDEO_NAME);

    com = lives_strdup_printf("%s open_test \"%s\" %s \"%s\" 0 png", temp_backend, cfile->handle,
                              prefs->video_open_command,
                              (tmp = lives_filename_from_utf8(rname, -1, NULL, NULL, NULL)));
    lives_freep((void **)&temp_backend);
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
        msg = lives_strdup_printf(_("You may wish to upgrade %s to a newer version\n"
                                    "(optional; only files opened using the 'fallback' method are affected)"), mp_cmd);
        fail_test(table, testcase, msg);
        lives_free(msg);
      }

      else {
        pass_test(table, testcase);
        success3 = TRUE;
      }
    }
  } else lives_freep((void **)&temp_backend);

  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (add_skip) {
    testcase++;
    add_skip = FALSE;
  }

  // check if mplayer can decode to jpeg
  prep_test(table, ++testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  msg = lives_strdup_printf(_("Checking if %s can decode to jpeg"), mp_cmd);
  onowait = nowait;
  nowait = success2;
  add_test(table, testcase, msg);
  lives_free(msg);
  nowait = onowait;
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
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

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
      msg = lives_strdup_printf(_("jpeg support is not obligatory, since png decoding is a better choice"), mp_cmd);
      fail_test(table, testcase, msg);
      lives_free(msg);
    }
  }

  // TODO - check each enabled decoder plugin in turn

jpgdone:
  // check for convert

  prep_test(table, ++testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  nowait = TRUE;

  if (!capable->has_convert) {
    fail_test(table, testcase, _("Install imageMagick to be able to use all of the rendered effects"));
    success = FALSE;
  } else {
    success = pass_test(table, testcase);
  }

  close_file(current_file, tshoot);
  mainw->current_file = current_file;

  if (!tshoot) {
    lives_widget_set_sensitive(okbutton, TRUE);
    lives_widget_grab_focus(okbutton);
  }

  if (tshoot) {
    if (imgext_switched) {
      label = lives_standard_label_new(
                _("\n\n\tImage decoding type has been switched to jpeg. You can revert this in Preferences/Decoding.\t\n"
                  "Note: this only affects clips which are opened via the 'fallback' method"));
      lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    }
    lives_widget_show(label);
    lives_standard_button_set_label(LIVES_BUTTON(defbutton), LIVES_STOCK_LABEL_OK);
    lives_widget_set_sensitive(defbutton, TRUE);
  } else {
    if (xlabel) lives_widget_set_opacity(xlabel, 1.);

    lives_signal_handler_block(cancelbutton, quitfunc);
    if (backfunc) lives_signal_handler_block(backbutton, backfunc);
    if (skipfunc) lives_signal_handler_block(okbutton, skipfunc);

    if (mainw->cancelled != CANCEL_NONE) goto cancld;
    lives_standard_button_set_label(LIVES_BUTTON(okbutton), LIVES_STOCK_LABEL_NEXT);
  }

  while (1) {
    response = lives_dialog_run(LIVES_DIALOG(dialog));

    // returned if dialog is hidden
    if (response == LIVES_RESPONSE_NONE) continue;
    if (response == LIVES_RESPONSE_CANCEL && !tshoot) {
      lives_widget_hide(dialog);
      if (confirm_exit()) {
        goto cancld;
      }
      lives_widget_show(dialog);
      lives_widget_context_update();
      continue;
    }
    if (!check_for_plugins(prefs->lib_dir, TRUE) || !find_prefix_dir(prefs->prefix_dir, TRUE)) {
      lives_widget_hide(dialog);
      if (!do_yesno_dialog(_("It may be possible to correct the problems marked 'Fix This' after clicking on the relevant buttons.\n"
                             "Are you sure you wish to ignore those issues and continue anyway ?"))) {
        lives_widget_show(dialog);
        continue;
      }
    }
    lives_widget_show(dialog);
    break;
  }

  if (response == LIVES_RESPONSE_RETRY) {
    SET_INT_DATA(dialog, INTENTION_KEY, LIVES_INTENTION_UNDO);
    goto cancld;
  }

  lives_widget_destroy(dialog);
  mainw->suppress_dprint = FALSE;

  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
  }

  if (response == LIVES_RESPONSE_OK) {
    prefs->startup_phase = 3;
    return TRUE;
  }

  return FALSE;

cancld:
  mainw->cancelled = CANCEL_NONE;

  close_file(current_file, tshoot);
  intent = GET_INT_DATA(dialog, INTENTION_KEY);
  lives_widget_destroy(dialog);
  mainw->suppress_dprint = FALSE;

  lives_freep((void **)&temp_backend);

  if (intent == LIVES_INTENTION_UNDO) {
    if (do_workdir_query()) goto rerun;
  }

  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
  }

  return FALSE;
}


boolean do_startup_interface_query(void) {
  // prompt for startup ce or startup mt
  LiVESWidget *dialog, *dialog_vbox, *radiobutton, *label, *xlabel, *layout;
  LiVESWidget *okbutton;
  LiVESWidget *hbox, *cb_desk = NULL, *cb_menus = NULL, *cb_msgs, *cb_quota;
  LiVESSList *radiobutton_group = NULL;
  LiVESResponseType resp;
  char *txt1, *txt2, *txt3, *msg;
  char *dskfile, *com;

  boolean add_hsep = FALSE;

retry:
  cb_desk = cb_menus = NULL;
  radiobutton_group = NULL;

  dialog = lives_standard_dialog_new(_("Startup Options"), FALSE, -1, -1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  txt1 = (_("Finally, you can choose the <b>default startup interface</b> for LiVES.\n"));
  txt2 = (_("LiVES has two main interfaces and you can start up with either of them.\n"));
  txt3 = (_("The default can always be changed later from Preferences.\n"));

  msg = lives_strdup_printf("%s%s%s", txt1, txt2, txt3);

  lives_free(txt1); lives_free(txt2); lives_free(txt3);

  widget_opts.use_markup = TRUE;
  xlabel = lives_standard_label_new(msg);
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(dialog_vbox), xlabel, FALSE, FALSE, add_hsep ? 0 : widget_opts.packing_height);
  lives_free(msg);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  widget_opts.use_markup = TRUE;
  lives_standard_radio_button_new(_("Start in <b>_Clip Edit</b> mode"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;

  label = lives_standard_label_new(_("This is the best choice for simple editing tasks and for VJs\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  widget_opts.use_markup = TRUE;
  radiobutton = lives_standard_radio_button_new(_("Start in <b>_Multitrack</b> mode"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;

  label = lives_standard_label_new(_("This is a better choice for complex editing tasks involving multiple clips.\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  if (prefs->startup_interface == STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);
  }

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  dskfile = lives_build_filename(prefs->prefix_dir, APPLICATIONS_DIR, "LiVES.desktop", NULL);
  if (!lives_file_test(dskfile, LIVES_FILE_TEST_EXISTS)) {
    lives_free(dskfile);
    dskfile = NULL;
    if (!dirs_equal(prefs->prefix_dir, LIVES_USR_DIR)) {
      dskfile = lives_build_filename(LIVES_USR_DIR, APPLICATIONS_DIR, "LiVES.desktop", NULL);
      if (!lives_file_test(dskfile, LIVES_FILE_TEST_EXISTS)) {
        lives_free(dskfile);
        dskfile = NULL;
      }
    }
  }

  add_hsep = TRUE;

  if (dskfile) {
    if (check_for_executable(&capable->has_xdg_desktop_menu, EXEC_XDG_DESKTOP_MENU)) {
      widget_opts.expand = LIVES_EXPAND_NONE;
      add_hsep_to_box(LIVES_BOX(dialog_vbox));
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      layout = lives_layout_new(LIVES_BOX(dialog_vbox));
      lives_layout_add_label(LIVES_LAYOUT(layout), _("Options"), FALSE);
      lives_layout_add_row(LIVES_LAYOUT(layout));
      lives_layout_add_label(LIVES_LAYOUT(layout), _("Desktop options:"), TRUE);
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      cb_menus = lives_standard_check_button_new
                 (_("Add LiVES to window manager _menus"), TRUE, LIVES_BOX(hbox), NULL);
      add_hsep = FALSE;
    }
    if (check_for_executable(&capable->has_xdg_desktop_icon, EXEC_XDG_DESKTOP_ICON)) {
      if (add_hsep) {
        widget_opts.expand = LIVES_EXPAND_NONE;
        add_hsep_to_box(LIVES_BOX(dialog_vbox));
        widget_opts.expand = LIVES_EXPAND_DEFAULT;
        layout = lives_layout_new(LIVES_BOX(dialog_vbox));
        lives_layout_add_row(LIVES_LAYOUT(layout));
        lives_layout_add_label(LIVES_LAYOUT(layout), _("Desktop options:"), TRUE);
        lives_layout_add_row(LIVES_LAYOUT(layout));
      }
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      cb_desk = lives_standard_check_button_new
                (_("Add a desktop _icon for LiVES"), TRUE, LIVES_BOX(hbox), NULL);
      add_hsep = FALSE;
    }
  }

  if (add_hsep) {
    widget_opts.expand = LIVES_EXPAND_NONE;
    add_hsep_to_box(LIVES_BOX(dialog_vbox));
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  }

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Startup options:"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  cb_msgs = lives_standard_check_button_new
            (_("Show message window on startup"), prefs->show_msgs_on_startup, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  cb_quota = lives_standard_check_button_new
             (_("Set quota limits now"), prefs->show_disk_quota, LIVES_BOX(hbox),
              H_("You can set quota limits now if you want to manage how much disk space LiVES may use.\n"
                 "(You can also do this later from within the application)"));

  widget_opts.expand = LIVES_EXPAND_NONE;
  add_hsep_to_box(LIVES_BOX(dialog_vbox));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
                                     _("Back to Audio Selection"), LIVES_RESPONSE_NO);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
             _("_FINISH SETUP and START LiVES"), LIVES_RESPONSE_OK);

  lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_EDGE);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  pop_to_front(dialog, NULL);

  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(xlabel), NULL, 0.7);
  }
  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(xlabel), NULL, 0.32);
  }

  resp = lives_dialog_run(LIVES_DIALOG(dialog));

  if (resp == LIVES_RESPONSE_NO) {
    lives_widget_destroy(dialog);
    if (!do_audio_choice_dialog(5)) {
      prefs->startup_phase = 3;
      return FALSE;
    }
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      prefs->startup_phase = 4;
      return TRUE;
    }
    goto retry;
  }

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(radiobutton))) {
    pref_factory_int(PREF_STARTUP_INTERFACE, &prefs->startup_interface, STARTUP_MT, TRUE);
    future_prefs->startup_interface = prefs->startup_interface;
  }

  pref_factory_bool(PREF_MSG_START,
                    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_msgs)), TRUE);

  pref_factory_bool(PREF_SHOW_QUOTA,
                    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_quota)), TRUE);

  if (cb_desk && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_desk))) {
    com = lives_strdup_printf("%s install --novendor %s", EXEC_XDG_DESKTOP_ICON, dskfile);
    lives_system(com, TRUE);
    lives_free(com);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
  }
  if (cb_menus && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_menus))) {
    com = lives_strdup_printf("%s install --novendor %s", EXEC_XDG_DESKTOP_MENU, dskfile);
    lives_system(com, TRUE);
    lives_free(com);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
  }

  if (dskfile) lives_free(dskfile);

  lives_widget_destroy(dialog);
  if (mainw->splash_window) {
    lives_widget_show(mainw->splash_window);
  }
  return TRUE;
}


void on_troubleshoot_activate(LiVESMenuItem * menuitem, livespointer user_data) {do_startup_tests(TRUE);}


static char *explain_missing(const char *exe) {
  char *pt2, *pt1 = lives_strdup_printf(_("\t'%s' was not found on your system.\n"
                                          "Installation is recommended as it provides the following features\n\t- "), exe);
  if (!lives_strcmp(exe, EXEC_FILE)) pt2 = (_("Enables easier identification of file types,\n\n"));
  else if (!lives_strcmp(exe, EXEC_DU)) pt2 = (_("Enables measuring of disk space used,\n\n"));
  else if (!lives_strcmp(exe, EXEC_GZIP)) pt2 = (_("Enables reduction in file size for some files,\n\n"));
  else if (!lives_strcmp(exe, EXEC_DU)) pt2 = (_("Enables measuring of disk space used,\n\n"));
  else if (!lives_strcmp(exe, EXEC_FFPROBE)) pt2 = (_("Assists in the identification of video clips\n\n"));
  else if (!lives_strcmp(exe, EXEC_IDENTIFY)) pt2 = (_("Assists in the identification of image files\n\n"));
  else if (!lives_strcmp(exe, EXEC_CONVERT)) pt2 = (_("Required for many rendered effects in the clip editor.\n\n"));
  else if (!lives_strcmp(exe, EXEC_COMPOSITE)) pt2 = (_("Enables clip merging in the clip editor.\n\n"));
  else if (!lives_strcmp(exe, EXEC_PYTHON)) pt2 = (_("Allows use of some additional encoder plugins\n\n"));
  else if (!lives_strcmp(exe, EXEC_MD5SUM)) pt2 = (_("Allows checking for file changes, "
        "enabling additional files to be cached in memory.\n\n"));
  else if (!lives_strcmp(exe, EXEC_YOUTUBE_DL)) pt2 = (_("Enables download and import of files from "
        "Youtube and other sites.\n\n"));
  else if (!lives_strcmp(exe, EXEC_XWININFO)) pt2 = (_("Enables identification of external windows "
        "so that they can be recorded.\n\n"));
  else {
    lives_free(pt1);
    pt1 = lives_strdup_printf(_("\t'%s' was not found on your system.\n"
                                "Installation is optional, but may enable additional features\n\t- "), exe);
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

  check_for_executable(&capable->has_file, EXEC_FILE);

  ADD_TO_TEXT(sox_sox, EXEC_SOX);
  ADD_TO_TEXT(file, EXEC_FILE);
  ADD_TO_TEXT(du, EXEC_DU);
  ADD_TO_TEXT(identify, EXEC_IDENTIFY);
  ADD_TO_TEXT(md5sum, EXEC_MD5SUM);
  ADD_TO_TEXT(ffprobe, EXEC_FFPROBE);
  ADD_TO_TEXT(convert, EXEC_CONVERT);
  ADD_TO_TEXT(composite, EXEC_COMPOSITE);
  if (check_for_executable(&capable->has_python, EXEC_PYTHON) != PRESENT
      && check_for_executable(&capable->has_python3, EXEC_PYTHON3) != PRESENT) {
    ADD_TO_TEXT(python, EXEC_PYTHON);
  }
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

