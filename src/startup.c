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
    if (1 || !lives_file_test(devmapdir, LIVES_FILE_TEST_IS_DIR)) {
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
          lives_cp_recursive(sys_devmap_dir, config_datadir, TRUE);
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
      if (!do_yesno_dialog(
            _("You have chosen to use your home directory as the LiVES working directory.\n"
              "This is NOT recommended as it will possibly result in the loss of unrelated files.\n"
              "Click Yes if you REALLY want to continue, or No to create or select another directory.\n")))
        return LIVES_RESPONSE_CANCEL;
    } else {
      msg = lives_format_storage_space_string(freespace);
      boolean res = do_yesno_dialogf(
                      _("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n\n(Free space in volume = %s)\n"),
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
        if (!prompt_existing_dir(*pdirname, freesp, TRUE)) {
          widget_opts.transient = NULL;
          return LIVES_RESPONSE_RETRY;
        }
        widget_opts.transient = NULL;
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
  if (!*mydir) return;

  parentdir = get_dir(mydir);
  if (!parentdir) return;
  if (!dirs_equal(last_parentdir, parentdir)) {
    if (!lives_file_test(parentdir, LIVES_FILE_TEST_IS_DIR)) {
      show_warn_image(LIVES_WIDGET(entry), _("WARNING: The parent directory does not exist !"));
    } else {
      hide_warn_image(LIVES_WIDGET(entry));
    }
  }
  last_parentdir = lives_strdup(parentdir);
}


boolean do_workdir_query(void) {
  LiVESWidget *dirbutton;
  _entryw *wizard = create_rename_dialog(6);
  char *dirname = NULL, *mp;
  LiVESResponseType response;
  boolean activated = FALSE;

  /// override FILESEL_TYPE_KEY, in case it was set to WORKDIR; we will do our own checking here
  dirbutton = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dirbutton), FILESEL_TYPE_KEY,
                               LIVES_INT_TO_POINTER(LIVES_DIR_SELECTION_WORKDIR_INIT));

  if (mainw->splash_window) {
    lives_widget_hide(mainw->splash_window);
    lives_widget_show_all(wizard->dialog);
    lives_widget_show_now(wizard->dialog);
  }
  lives_widget_context_update();
  if (!mainw->is_ready) {
    char *wid;
    gtk_window_set_urgency_hint(LIVES_WINDOW(wizard->dialog), TRUE); // dont know if this actually does anything...
    wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(wizard->dialog)));
    if (!wid || (activated = !activate_x11_window(wid)) || 1) {
      lives_window_set_keep_above(LIVES_WINDOW(wizard->dialog), TRUE);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dirbutton), KEEPABOVE_KEY, wizard->dialog);
    }
    if (activated)
      lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(dirbutton),
                                        ACTIVATE_KEY, lives_strdup(wid));
    if (wid) lives_free(wid);
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
      if (!mainw->is_ready) restore_wm_stackpos(LIVES_BUTTON(dirbutton));
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

void chk_jack_cfgx(LiVESWidget * e, livespointer data) {
  const char *server_cfgx = lives_entry_get_text(LIVES_ENTRY(e));
  if (lives_file_test(server_cfgx, LIVES_FILE_TEST_IS_EXECUTABLE)) {
    hide_warn_image(e);
    return;
  }
  if (!lives_file_test(server_cfgx, LIVES_FILE_TEST_EXISTS)) {
    show_warn_image(e, _("The specified file does not exist"));
    return;
  }
  show_warn_image(e, _("The specified file should be executable"));
}


boolean do_jack_config(boolean is_setup, boolean is_trans) {
  LiVESSList *rb_group = NULL;
  LiVESWidget *dialog, *dialog_vbox, *layout, *hbox, *rb, *cb2 = NULL, *cb3;
  LiVESWidget *acdef, *acname, *astart;
  LiVESWidget *asdef, *asname;
  LiVESWidget *okbutton, *cancelbutton, *filebutton;
  LiVESWidget *scrpt_rb, *cfg_entry, *expander = NULL;
  LiVESResponseType response;
  char *show_hid[2] = {".", NULL};
  char *title, *text;
  char *wid;
  char *server_cfgx;
  int woph = widget_opts.packing_height;
  int old_fprefs = future_prefs->jack_opts;
  boolean cfg_exists = FALSE;

  cfg_exists = jack_get_cfg_file(is_trans, &server_cfgx);

  widget_opts.packing_height <<= 1;

set_config:
  if (is_setup) title = _("Initial configuration for jack audio");
  else title = lives_strdup_printf(_("Server and driver configuration for %s"),
                                     is_trans ? _("jack transport") : _("jack audio"));
  dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  if (is_setup) {
    widget_opts.use_markup = TRUE;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("</b></big>Please use the options below to define the "
                           "initial settings for jackd.\n"
                           "<big><b>Once LiVES starts up, you can adjust the settings "
                           "in Tools / Preferences / Jack Integration\n"), FALSE);
    widget_opts.use_markup = FALSE;


    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    acdef =
      lives_standard_check_button_new(_("Connect to the default server"),
                                      TRUE, LIVES_BOX(hbox),
                                      H_("The server name will be taken from the environment "
                                         "variable\n$JACK_DEFAULT_SERVER.\nIf that variable is not "
                                         "set, then the name 'default' will be used instead"));

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);

    acname = lives_standard_entry_new(_("Connect to a specific server"), *prefs->jack_aserver_cname
                                      ? prefs->jack_aserver_cname : JACK_DEFAULT_SERVER_NAME,
                                      -1, JACK_PARAM_STRING_MAX, LIVES_BOX(hbox), NULL);

    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(acdef), acname, TRUE);

    lives_layout_add_row(LIVES_LAYOUT(layout));

    lives_layout_add_label(LIVES_LAYOUT(layout), _("What to do if connection fails..."), TRUE);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    lives_standard_radio_button_new(_("...do nothing"), &rb_group, LIVES_BOX(hbox),
                                    H_("If this setting is active, LiVES will only ever attempt to connect a running jack server\n"
                                       "and will never try to start a server itself"));

    lives_layout_add_row(LIVES_LAYOUT(layout));
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    astart = lives_standard_radio_button_new(_("_...try to start the jack server"), &rb_group, LIVES_BOX(hbox),
             H_("Checking this will cause LiVES to try to start up a jackd server itself\n"
                "if it is unable to connect to a running instance.\n"));

    if (cfg_exists)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(astart), TRUE);
  } else {
    lives_layout_add_label(LIVES_LAYOUT(layout), _("NOTE: if server start up fails, "
                           "LiVES will ALWAYS produce an error."), FALSE);

    text = lives_strdup_printf(_("Consider: in the case that LiVES fails to connect to %s, it will try to start "
                                 "a new server itself using the settings below.\n"
                                 "<small>Should the new server fail to start, "
                                 "this will generate an error condition\nwhich will require manual "
                                 "intervention to resolve.\n</small>"), (is_trans && *prefs->jack_tserver_cname)
                               ? prefs->jack_tserver_cname : (!is_trans && *prefs->jack_aserver_cname)
                               ? prefs->jack_aserver_cname : _("the default server"));

    widget_opts.use_markup = TRUE;
    lives_layout_add_label(LIVES_LAYOUT(layout), text, FALSE);
    widget_opts.use_markup = FALSE;
    lives_free(text);
  }

  lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rb_group = NULL;

  scrpt_rb = lives_standard_radio_button_new(_("_Use settings in config file"),
             &rb_group, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  cfg_entry = lives_standard_fileentry_new(NULL, server_cfgx, capable->home_dir,
              MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);
  filebutton = lives_label_get_mnemonic_widget(LIVES_LABEL(widget_opts.last_label));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(filebutton), FILTER_KEY, show_hid);
  lives_entry_set_editable(LIVES_ENTRY(cfg_entry), TRUE);

  if (!cfg_exists) show_warn_image(cfg_entry, _("The specified file does not exist"));
  else {
    if (!lives_file_test(server_cfgx, LIVES_FILE_TEST_IS_EXECUTABLE)) {
      show_warn_image(cfg_entry, _("The specified file should be executable"));
    }
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cfg_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(chk_jack_cfgx), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(scrpt_rb), cfg_entry, FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rb = lives_standard_radio_button_new(_("Use settings from _LiVES"),
                                       &rb_group, LIVES_BOX(hbox), NULL);
  lives_widget_set_valign(hbox, LIVES_ALIGN_START);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  if (is_setup) {
    expander = lives_standard_expander_new(_("_LiVES Settings"), LIVES_BOX(hbox),
                                           (layout = lives_layout_new(NULL)));
  }

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  asdef = lives_standard_check_button_new(_("Use _default server name"),
                                          (is_setup
                                              || (!is_setup
                                                  && ((is_trans && !*future_prefs->jack_tserver_sname)
                                                      || (!is_trans
                                                          && !*future_prefs->jack_aserver_sname)))),
                                          LIVES_BOX(hbox),
                                          H_("The server name will be taken from the environment "
                                              "variable\n$JACK_DEFAULT_SERVER.\nIf that variable is not "
                                              "set, then 'default' will be used instead"));

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
  asname = lives_standard_entry_new(_("Use _custom server name"),
                                    (is_trans && *future_prefs->jack_tserver_sname)
                                    ? future_prefs->jack_tserver_sname :
                                    (!is_trans && *future_prefs->jack_aserver_sname)
                                    ? future_prefs->jack_aserver_sname :
                                    JACK_DEFAULT_SERVER_NAME, -1, JACK_PARAM_STRING_MAX,
                                    LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), asdef, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(asdef), asname, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), hbox, FALSE);

  if (!is_setup) {
    if ((!is_trans && !*future_prefs->jack_aserver_cfg)
        || (is_trans && !*future_prefs->jack_tserver_cfg)) {
      lives_toggle_button_set_active(rb, TRUE);
    }
  }

  if (!is_setup) {
    // warn if setting this for audio client and is already set for trans client, and trans
    // client is enabled, and !jack_srv_dup
    cb2 = lives_standard_check_button_new(_("Set as _user default"),
                                          (is_trans && (future_prefs->jack_opts
                                              & JACK_OPTS_SETENV_TSERVER))
                                          || (!is_trans && (future_prefs->jack_opts
                                              & JACK_OPTS_SETENV_ASERVER))
                                          ? TRUE : FALSE, LIVES_BOX(hbox),
                                          H_("If checked, the specified server name will be exported "
                                              "as\n$JACK_DEFAULT_SERVER, which will cause other jack "
                                              "clients in the same process environment\n"
                                              "to also use the exported value as their "
                                              "default server name"));
    text = lives_strdup_printf(_("WARNING: this option is already enabled for the %s client\n"
                                 "Enabling this value as well may cause undesired results\n"),
                               is_trans ? _("audio") : ("transport"));
    show_warn_image(cb2, text);
    lives_free(text);
    hide_warn_image(cb2);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cb2), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(chk_setenv_conf),
                                    LIVES_INT_TO_POINTER(is_trans));
    chk_setenv_conf(cb2, LIVES_INT_TO_POINTER(is_trans));
  }

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  cb3 = lives_standard_check_button_new(_("Create as temporary server"),
                                        is_trans ? !(future_prefs->jack_opts & JACK_OPTS_PERM_TSERVER) :
                                        !(future_prefs->jack_opts & JACK_OPTS_PERM_ASERVER),
                                        LIVES_BOX(hbox), H_("Checking this will "
                                            "make any jackd servers "
                                            "started by LiVES\n"
                                            "shut down when there are "
                                            "no more clients connected "
                                            "to them.\n(This option only"
                                            " applies to servers created"
                                            " by LiVES\nvia means other "
                                            "than using a config "
                                            "file)\n"));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), hbox, FALSE);
  if (expander) toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), expander, FALSE);

  if (!is_setup) {
    LiVESWidget *advbutton;

    lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), hbox, FALSE);

    advbutton =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
          _("Server Configuration (EXPERTS ONLY)"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

    if (is_trans) {
      if (prefs->jack_tsparams)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_server_config),
                                  (livespointer)prefs->jack_tsparams);
      else lives_widget_set_sensitive(advbutton, FALSE);
    } else {
      if (prefs->jack_asparams)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_server_config),
                                  (livespointer)prefs->jack_asparams);
      else lives_widget_set_sensitive(advbutton, FALSE);
    }

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), hbox, FALSE);

    advbutton =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
          _("Driver Configuration (EXPERTS ONLY)"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

    if (is_trans) {
      if (prefs->jack_tdrivers)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_drivers_config),
                                  (livespointer)prefs->jack_tdrivers);
      else lives_widget_set_sensitive(advbutton, FALSE);
    } else {
      if (prefs->jack_adrivers)
        lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(jack_drivers_config),
                                  (livespointer)prefs->jack_adrivers);
      else lives_widget_set_sensitive(advbutton, FALSE);
    }

    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  }

  widget_opts.packing_height = woph;

  if (is_setup)
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
                   _("Back"), LIVES_RESPONSE_CANCEL);
  else
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL,
                   NULL, LIVES_RESPONSE_CANCEL);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  if (is_setup)
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
               _("_Next"), LIVES_RESPONSE_OK);
  else
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK,
               NULL, LIVES_RESPONSE_OK);

  if (is_setup) {
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(astart), scrpt_rb, FALSE);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(astart), cfg_entry, FALSE);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(astart), rb, FALSE);

    lives_widget_show_all(dialog);
    lives_widget_grab_focus(okbutton);

    gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE); // dont know if this actually does anything...
    wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
    if (!wid || !activate_x11_window(wid) || 1) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);
    if (wid) lives_free(wid);
  }

  lives_button_grab_default_special(okbutton);

retry:
  response = lives_dialog_run(LIVES_DIALOG(dialog));

  if (response == LIVES_RESPONSE_OK) {
    boolean ignore = FALSE;
    if (is_setup) {
      future_prefs->jack_opts = 0;
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(astart))) {
        future_prefs->jack_opts |= JACK_OPTS_START_ASERVER;
        if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(scrpt_rb))) {
          const char *server_cfg = lives_entry_get_text(LIVES_ENTRY(cfg_entry));
          if (!lives_file_test(server_cfg, LIVES_FILE_TEST_IS_EXECUTABLE)) {
            if (!do_jack_nonex_warn(server_cfg)) {
              goto retry;
            }
          }
          lives_snprintf(prefs->jack_aserver_cfg, PATH_MAX, "%s", server_cfg);
        }
      } else ignore = TRUE;
    } else {
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(scrpt_rb))) {
        boolean scrpt_conflict = FALSE;
        const char *server_cfg = lives_entry_get_text(LIVES_ENTRY(cfg_entry));
        if (is_trans) {
          if (!prefs->jack_srv_dup && !lives_strcmp(server_cfg, prefs->jack_aserver_cfg))
            scrpt_conflict = TRUE;
          else lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", server_cfg);
        } else {
          if (!prefs->jack_srv_dup && !lives_strcmp(server_cfg, prefs->jack_tserver_cfg))
            scrpt_conflict = TRUE;
          else lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", server_cfg);
        }
        if (scrpt_conflict) {
          if (do_jack_scripts_warn(server_cfg)) {
            goto retry;
          }
        }
      } else {
        if (!is_trans) {
          *future_prefs->jack_aserver_cfg = 0;
        } else {
          *future_prefs->jack_tserver_cfg = 0;
        }
      }
    }
    if (is_setup) {
      if (ignore || lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(acdef))) {
        *prefs->jack_aserver_cname = 0;
      } else {
        lives_snprintf(prefs->jack_aserver_cname, JACK_PARAM_STRING_MAX,
                       "%s", lives_entry_get_text(LIVES_ENTRY(acname)));
      }
    }
    if (ignore || lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(asdef))) {
      if (is_setup) *prefs->jack_aserver_sname = 0;
      else {
        if (!is_trans) *future_prefs->jack_tserver_sname = 0;
        else *future_prefs->jack_tserver_sname = 0;
      }
    } else {
      if (is_setup) lives_snprintf(prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX,
                                     "%s", lives_entry_get_text(LIVES_ENTRY(asname)));
      else {
        if (!is_trans)
          lives_snprintf(future_prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX,
                         "%s", lives_entry_get_text(LIVES_ENTRY(asname)));

        else
          lives_snprintf(future_prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX,
                         "%s", lives_entry_get_text(LIVES_ENTRY(asname)));
      }
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
        if (is_setup) {
          lives_snprintf(prefs->jack_tserver_cname, JACK_PARAM_STRING_MAX,
                         "%s", prefs->jack_aserver_cname);
          lives_snprintf(prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX,
                         "%s", prefs->jack_aserver_sname);
        } else {
          lives_snprintf(future_prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX,
                         "%s", future_prefs->jack_aserver_sname);
        }
        if (future_prefs->jack_opts & JACK_OPTS_START_ASERVER)
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
    lives_widget_destroy(dialog);
    if (is_setup) {
      prefs->jack_opts = future_prefs->jack_opts;
      if (!(prefs->jack_opts & JACK_OPTS_START_ASERVER)) {
        char *srvname;
        future_prefs->jack_opts = prefs->jack_opts = 0;
        if (*prefs->jack_aserver_cname)
          srvname = lives_strdup_printf(_("jack server '%s'"), prefs->jack_aserver_cname);
        else
          srvname = _("the default jack server");
        if (!do_warning_dialogf(_("Please ensure that %s "
                                  "is running before clicking OK\n"
                                  "Otherwise, click Cancel to select a different option\n"),
                                srvname)) {
          lives_free(srvname);
          rb_group = NULL;
          goto set_config;
        }
        lives_free(srvname);
        future_prefs->jack_opts = prefs->jack_opts = 0;
      }
      set_int_pref(PREF_JACK_OPTS, prefs->jack_opts);
      set_string_pref(PREF_JACK_ACSERVER, prefs->jack_aserver_cname);
      set_string_pref(PREF_JACK_ASSERVER, prefs->jack_aserver_sname);
      set_string_pref(PREF_JACK_ACONFIG, prefs->jack_aserver_cfg);
    } else {
      if (future_prefs->jack_opts != old_fprefs
          || (is_trans
              && (lives_strcmp(future_prefs->jack_tserver_sname, prefs->jack_tserver_sname)
                  || lives_strcmp(future_prefs->jack_tserver_cfg, prefs->jack_tserver_cfg)))
          || (!is_trans
              && (lives_strcmp(future_prefs->jack_aserver_sname, prefs->jack_aserver_sname)
                  || lives_strcmp(future_prefs->jack_aserver_sname, prefs->jack_aserver_sname))))
        apply_button_set_enabled(NULL, NULL);
    }
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
  default:
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_NONE);
    break;
  }
}


boolean do_audio_choice_dialog(short startup_phase) {
  LiVESWidget *dialog, *dialog_vbox, *radiobutton2 = NULL, *radiobutton3, *label;
  LiVESWidget *okbutton, *cancelbutton;
  LiVESWidget *hbox;

#ifdef HAVE_PULSE_AUDIO
  LiVESWidget *radiobutton0;
#endif

#ifdef ENABLE_JACK
  LiVESWidget *radiobutton1;
#endif

  LiVESSList *radiobutton_group = NULL;

  char *txt0, *txt1, *txt2, *txt3, *txt4, *txt5, *txt6, *txt7, *msg, *wid;
  char *tmp, *recstr;

  LiVESResponseType response;

  if (startup_phase == 2) {
    txt0 = (_("LiVES FAILED TO START YOUR SELECTED AUDIO PLAYER !\n\n"));
  } else {
    prefs->audio_player = -1;
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
               "you can restart and try with another audio player instead.\n\n"));
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

  // TODO: add_param_label_to_box()
  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new(msg);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  widget_opts.use_markup = FALSE;
  lives_free(msg);

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  recstr = lives_strdup_printf(" (%s)", mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);

#ifdef HAVE_PULSE_AUDIO
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_opts.use_markup = TRUE;
  radiobutton0 =
    lives_standard_radio_button_new((tmp = lives_strdup_printf(_("Use _<b>pulseaudio</b> player%s"),
                                     capable->has_pulse_audio ?
                                     recstr : "")),
                                    &radiobutton_group, LIVES_BOX(hbox), NULL);
  lives_free(tmp);
  widget_opts.use_markup = FALSE;
  if (!capable->has_pulse_audio) lives_widget_set_sensitive(radiobutton0, FALSE);
  else if (prefs->audio_player == -1) prefs->audio_player = AUD_PLAYER_PULSE;
#endif

#ifdef ENABLE_JACK
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_opts.use_markup = TRUE;
  radiobutton1 =
    lives_standard_radio_button_new((tmp = lives_strdup_printf(_("Use _<b>jack</b> audio player%s"),
                                     capable->has_jackd
                                     && !capable->has_pulse_audio
                                     ? recstr : "")),
                                    &radiobutton_group, LIVES_BOX(hbox), NULL);
  lives_free(tmp);
  widget_opts.use_markup = FALSE;
  if (!capable->has_jackd) lives_widget_set_sensitive(radiobutton1, FALSE);
#endif

  lives_free(recstr);
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_opts.use_markup = TRUE;
  radiobutton2 = lives_standard_radio_button_new(_("Use _<b>sox</b> audio player"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;
  if (capable->has_sox_play) {
    if (prefs->audio_player == -1) prefs->audio_player = AUD_PLAYER_SOX;
  } else lives_widget_set_sensitive(radiobutton2, FALSE);

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE || (!capable->has_pulse_audio && prefs->audio_player == -1)) {
    prefs->audio_player = AUD_PLAYER_PULSE;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton0), TRUE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
  }
#endif
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK || !capable->has_pulse_audio || prefs->audio_player == -1) {
    prefs->audio_player = AUD_PLAYER_JACK;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1), TRUE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
  }
#endif
  if (capable->has_sox_play) {
    if (prefs->audio_player == AUD_PLAYER_SOX) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton2), TRUE);
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
    }
  }

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
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
                 _("Quit from Setup"), LIVES_RESPONSE_CANCEL);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
                                     _("Back"), LIVES_RESPONSE_RETRY);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
             LIVES_STOCK_GO_FORWARD, _("_Next"),
             LIVES_RESPONSE_OK);

  lives_window_center(LIVES_WINDOW(dialog));

  lives_widget_show_all(dialog);
  lives_button_grab_default_special(okbutton);

  //lives_widget_show_noyw(dialog);
  lives_widget_grab_focus(okbutton);

  if (prefs->audio_player == -1) {
    do_no_mplayer_sox_error();
    return LIVES_RESPONSE_CANCEL;
  }

  if (mainw->splash_window) {
    lives_widget_hide(mainw->splash_window);
  }

  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(label), NULL, .22);
  }
  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(label), NULL, 1.22);
  }

  lives_widget_show_all(dialog);
  gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE);

  wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
  if (!wid || !activate_x11_window(wid) || 1) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);
  if (wid) lives_free(wid);

  while (1) {
    response = lives_dialog_run(LIVES_DIALOG(dialog));
    if (response == LIVES_RESPONSE_CANCEL) {
      if (!confirm_exit()) continue;
    }
    break;
  }

  lives_widget_destroy(dialog);

#ifdef ENABLE_JACK
  if (response == LIVES_RESPONSE_OK) {
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      if (!do_jack_config(TRUE, FALSE)) {
        txt0 = lives_strdup("");
        radiobutton_group = NULL;
        goto reloop;
      }
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

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
    lives_widget_hide(mainw->recaudio_submenu);
  }

  return (response == LIVES_RESPONSE_OK);
}


#define MAX_TESTS 64
static LiVESWidget *test_labels[MAX_TESTS];
static LiVESWidget *test_reslabels[MAX_TESTS];
static LiVESWidget *test_spinners[MAX_TESTS];

static void prep_test(LiVESWidget * table, int row) {
  LiVESWidget *label = test_reslabels[row];
  if (label) lives_label_set_text(LIVES_LABEL(label), _("Checking..."));
#if LIVES_HAS_SPINNER_WIDGET
  if (test_spinners[row]) {
    lives_spinner_start(LIVES_SPINNER(test_spinners[row]));
  }
#endif
  for (int i = 0; i < 100; i++) {
    lives_widget_context_update();
    lives_usleep(4000);
  }
}

static void add_test(LiVESWidget * table, int row, const char *ttext, boolean noskip) {
  LiVESWidget *label = test_labels[row];
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

  if (!noskip) {
    LiVESWidget *image = NULL;
    label = test_reslabels[row];
    if (add_spinner) {
      if (!label) label = lives_standard_label_new(_("Waiting..."));
#if LIVES_HAS_SPINNER_WIDGET
      if (!test_spinners[row]) {
        image = test_spinners[row] = lives_standard_spinner_new(FALSE);
      }
#endif
    } else {
      char *txt;
      if (test_spinners[row]) {
        lives_widget_unparent(test_spinners[row]);
        test_spinners[row] = NULL;
      }

      image = lives_image_new_from_stock(LIVES_STOCK_REMOVE, LIVES_ICON_SIZE_LARGE_TOOLBAR);

      // TRANSLATORS - as in "skipped test"
      txt = _("Skipped");
      if (!label) label = lives_standard_label_new(txt);
      else lives_label_set_text(LIVES_LABEL(label), txt);
      lives_free(txt);
    }

    if (!test_reslabels[row]) {
      lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
      //lives_widget_show(label);
      test_reslabels[row] = label;
    }

    if (image) {
      lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
    }
  }
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
  lives_widget_show(image);

  lives_widget_show_all(table);
  for (int i = 0; i < 100; i++) {
    lives_widget_context_update();
    lives_usleep(2000);
  }
  return TRUE;
}

static boolean _fail_test(LiVESWidget * table, int row, char *ftext, const char *type) {
  LiVESWidget *label = test_reslabels[row];
#if GTK_CHECK_VERSION(3, 10, 0)
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_DIALOG_WARNING, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#else
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_CANCEL, LIVES_ICON_SIZE_LARGE_TOOLBAR);
#endif

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

  label = lives_standard_label_new(ftext);

  lives_table_attach(LIVES_TABLE(table), label, 3, 4, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row + 1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_show_all(table);

  for (int i = 0; i < 100; i++) {
    lives_widget_context_update();
    lives_usleep(2000);
  }
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

static void quit_from_tests(LiVESWidget * dialog, livespointer button) {
  lives_widget_hide(dialog);
  if (confirm_exit()) {
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


boolean do_startup_tests(boolean tshoot) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;

  LiVESWidget *label, *xlabel = NULL;
  LiVESWidget *table;
  LiVESWidget *okbutton;
  LiVESWidget *cancelbutton = NULL, *backbutton = NULL;

  char mppath[PATH_MAX];

  char *com, *rname, *afile, *tmp;
  char *image_ext = lives_strdup(prefs->image_ext);
  char *title, *msg;
  char *temp_backend = NULL;

  const char *mp_cmd;
  const char *lookfor;

  uint8_t *abuff;
  ulong quitfunc = 0, backfunc = 0;

  off_t fsize;

  boolean success, success2, success3, success4;
  boolean imgext_switched;
  boolean add_skip = FALSE;

  LiVESResponseType response;
  int res;
  int current_file = mainw->current_file;

  int intent;
  int out_fd, info_fd, testcase = 0;

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
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, _("Quit from Setup"),
                   LIVES_RESPONSE_CANCEL);

    quitfunc = lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
               LIVES_GUI_CALLBACK(quit_from_tests), dialog);

    lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

    backbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK, _("_Back to directory selection"),
                 LIVES_RESPONSE_RETRY);

    backfunc = lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(backbutton), LIVES_WIDGET_CLICKED_SIGNAL,
               LIVES_GUI_CALLBACK(back_from_tests), dialog);

    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD, _("_Next"),
               LIVES_RESPONSE_OK);
  } else okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                      LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_widget_set_sensitive(okbutton, FALSE);

  table = lives_table_new(10, 4, FALSE);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), table);

  if (!tshoot) {
    if (mainw->splash_window) {
      lives_widget_hide(mainw->splash_window);
    }
  }

  add_test(table, testcase, _("Checking for \"sox\" presence"), FALSE);

  // test if sox can convert raw 44100 -> wav 22050
  add_test(table, ++testcase, _("Checking if sox can convert audio"), FALSE);

  add_test(table, ++testcase, _("Checking for \"mplayer\", \"mplayer2\" or \"mpv\" presence"), FALSE);

  add_test(table, ++testcase, _("Checking if ???? can convert audio"), FALSE);

#ifdef ALLOW_PNG24
  msg = lives_strdup_printf(_("Checking if %s can decode to png"), "????");
#else
  msg = lives_strdup_printf(_("Checking if %s can decode to png/alpha"), "????");
#endif
  add_test(table, ++testcase, msg, FALSE);
  lives_free(msg);

  add_test(table, ++testcase, NULL, TRUE);

  add_test(table, ++testcase, _("Checking if ???? can decode to jpeg"), FALSE);

  add_test(table, ++testcase, _("Checking for \"convert\" presence"), FALSE);

  if (!tshoot) {
    xlabel = lives_standard_label_new(
               _("\n\n\tClick 'Quit from Setup' to exit and install any missing components, or Next to continue\t\n"));
    lives_container_add(LIVES_CONTAINER(dialog_vbox), xlabel);
    lives_widget_show(xlabel);
    lives_widget_set_opacity(xlabel, 0.);
  } else add_fill_to_box(LIVES_BOX(dialog_vbox));
  lives_widget_show_all(dialog);

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

  if (!tshoot) {
    char *wid;
    gtk_window_set_urgency_hint(LIVES_WINDOW(dialog), TRUE); // dont know if this actually does anything...
    wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
    if (!wid || !activate_x11_window(wid) || 1) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);
    if (wid) lives_free(wid);
  }

  // begin tests... //////

  testcase = 0;

  // check for sox presence

  prep_test(table, testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

  if (!capable->has_sox_sox) {
    success = fail_test(table, testcase, _("You should install sox to be able to use all the audio features in LiVES"));
    lives_widget_grab_focus(cancelbutton);
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
    success2 = fail_test(table, testcase,
                         _("You should install mplayer, mplayer2 or mpv to be able to use "
                           "all the decoding features in LiVES"));
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

  msg = lives_strdup_printf(_("Checking if %s can convert audio"), mp_cmd);
  add_test(table, ++testcase, msg, success2);
  lives_free(msg);
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

  add_test(table, ++testcase, msg, success2);
  lives_free(msg);
  prep_test(table, testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

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
        msg = lives_strdup_printf(_("You may wish to upgrade %s to a newer version"), mp_cmd);
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
  add_test(table, testcase, msg, success2);
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
      msg = lives_strdup_printf(_("You may wish to add jpeg output support to %s"), mp_cmd);
      fail_test(table, testcase, msg);
      lives_free(msg);
    }
  }

  // TODO - check each enabled decoder plugin in turn

jpgdone:
  // check for convert

  prep_test(table, ++testcase);
  if (mainw->cancelled != CANCEL_NONE) goto cancld;

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
    if (imgext_switched) {
      label = lives_standard_label_new(
                _("\n\n\tImage decoding type has been switched to jpeg. You can revert this in Preferences/Decoding.\t\n"));
      lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    }
    lives_widget_show(label);
  } else {
    if (xlabel) lives_widget_set_opacity(xlabel, 1.);

    lives_signal_handler_block(cancelbutton, quitfunc);
    if (backfunc) lives_signal_handler_block(backbutton, backfunc);

    if (mainw->cancelled != CANCEL_NONE) goto cancld;
  }

  while (1) {
    response = lives_dialog_run(LIVES_DIALOG(dialog));

    if (response == LIVES_RESPONSE_CANCEL) {
      lives_widget_hide(dialog);
      if (confirm_exit()) {
        goto cancld;
      }
      lives_widget_show(dialog);
      lives_widget_context_update();
      continue;
    }
    break;
  }

  if (response == LIVES_RESPONSE_RETRY) {
    SET_INT_DATA(dialog, INTENTION_KEY, LIVES_INTENTION_UNDO);
    goto cancld;
  }

  lives_widget_destroy(dialog);
  mainw->suppress_dprint = FALSE;

  if (mainw->splash_window) {
    lives_widget_show(mainw->splash_window);
  }

  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
  }

  return (response == LIVES_RESPONSE_OK);

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


void do_startup_interface_query(void) {
  // prompt for startup ce or startup mt
  LiVESWidget *dialog, *dialog_vbox, *radiobutton, *label, *xlabel;
  LiVESWidget *okbutton;
  LiVESWidget *hbox;
  LiVESSList *radiobutton_group = NULL;
  LiVESResponseType resp;
  char *txt1, *txt2, *txt3, *msg, *wid;
  boolean add_hsep = FALSE;

  dialog = lives_standard_dialog_new(_("Choose the Startup Interface"), FALSE, -1, -1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

#ifdef ENABLE_JACK
  if (mainw->jackd && *mainw->jackd->status_msg) {
    label = lives_standard_label_new("JACK status:");
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    label = lives_standard_label_new(mainw->jackd->status_msg);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    add_hsep = TRUE;
  }
  if (mainw->jackd_read && *mainw->jackd_read->status_msg) {
    if (!add_hsep) {
      label = lives_standard_label_new("JACK status:");
      lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    }
    label = lives_standard_label_new(mainw->jackd_read->status_msg);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    add_hsep = TRUE;
  }
#endif

  if (add_hsep) {
    widget_opts.expand = LIVES_EXPAND_NONE;
    add_hsep_to_box(LIVES_BOX(dialog_vbox));
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
  }

  txt1 = (_("\n\nFinally, you can choose the default startup interface for LiVES.\n"));
  txt2 = (_("\n\nLiVES has two main interfaces and you can start up with either of them.\n"));
  txt3 = (_("\n\nThe default can always be changed later from Preferences.\n"));

  msg = lives_strdup_printf("%s%s%s", txt1, txt2, txt3);

  lives_free(txt1); lives_free(txt2); lives_free(txt3);

  xlabel = lives_standard_label_new(msg);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), xlabel, FALSE, FALSE, add_hsep ? 0 : widget_opts.packing_height);
  lives_free(msg);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  widget_opts.use_markup = TRUE;
  lives_standard_radio_button_new(_("Start in _<b>Clip Edit</b> mode"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;

  label = lives_standard_label_new(_("This is the best choice for simple editing tasks and for VJs\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  widget_opts.use_markup = TRUE;
  radiobutton = lives_standard_radio_button_new(_("Start in _<b>Multitrack</b> mode"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;

  label = lives_standard_label_new(_("This is a better choice for complex editing tasks involving multiple clips.\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  if (prefs->startup_interface == STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);
  }

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new(
            _("You may optionally set quota limits here if you want to manage how much disk space LiVES may use.\n"
              "<small>(You can also do this later from within the application, should you wish)</small>"));
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), NULL,
                                     _("Set Quota Limits (Optional)"), LIVES_RESPONSE_SHOW_DETAILS);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
             _("Finish"), LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);
  //  lives_widget_show_now(dialog);
  lives_widget_show_all(dialog);

  wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
  if (!wid || !activate_x11_window(wid) || 1) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);
  if (wid) lives_free(wid);

  if (mainw->splash_window) {
    lives_widget_hide(mainw->splash_window);
  }

  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(xlabel), NULL, 0.7);
  }
  if (!mainw->first_shown) {
    guess_font_size(dialog, LIVES_LABEL(xlabel), NULL, 0.32);
  }

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  if (resp == LIVES_RESPONSE_SHOW_DETAILS) pref_factory_bool(PREF_SHOW_QUOTA, TRUE, TRUE);
  else pref_factory_bool(PREF_SHOW_QUOTA, FALSE, TRUE);

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(radiobutton)))
    future_prefs->startup_interface = prefs->startup_interface = STARTUP_MT;

  set_int_pref(PREF_STARTUP_INTERFACE, prefs->startup_interface);

  lives_widget_destroy(dialog);
  if (mainw->splash_window) {
    lives_widget_show(mainw->splash_window);
  }
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

