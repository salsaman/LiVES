// sethandler.c
// (c) G. Finch 2019 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

boolean is_legal_set_name(const char *set_name, boolean allow_dupes, boolean leeway) {
  // check (clip) set names for validity
  // - may not be of zero length
  // - may not contain spaces or characters / \ * "
  // - must NEVER be name of a set in use by another copy of LiVES (i.e. with a lock file)

  // - as of 1.6.0:
  // -  may not start with a .
  // -  may not contain ..

  // - as of 3.2.0
  //   - must start with a letter [a - z] or [A - Z]

  // should be in FILESYSTEM encoding

  // may not be longer than MAX_SET_NAME_LEN chars

  // iff allow_dupes is FALSE then we disallow the name of any existing set (has a subdirectory in the working directory)

  if (!do_std_checks(set_name, _("Set"), MAX_SET_NAME_LEN, NULL)) return FALSE;

  // check if this is a set in use by another copy of LiVES
  if (mainw && mainw->is_ready && !check_for_lock_file(set_name, 1)) return FALSE;

  if ((set_name[0] < 'a' || set_name[0] > 'z') && (set_name[0] < 'A' || set_name[0] > 'Z')) {
    if (leeway) {
      if (mainw->is_ready)
        do_warning_dialog(_("As of LiVES 3.2.0 all set names must begin with alphabetical character\n"
                            "(A - Z or a - z)\nYou will need to give a new name for the set when saving it.\n"));
    } else {
      do_error_dialog(_("All set names must begin with an alphabetical character\n(A - Z or a - z)\n"));
      return FALSE;
    }
  }

  if (!allow_dupes) {
    // check for duplicate set names
    char *set_dir = lives_build_filename(prefs->workdir, set_name, NULL);
    if (lives_file_test(set_dir, LIVES_FILE_TEST_IS_DIR)) {
      lives_free(set_dir);
      return do_yesno_dialogf(_("\nThe set %s already exists.\n"
                                "Do you want to add the current clips to the existing set ?.\n"), set_name);
    }
    lives_free(set_dir);
  }

  return TRUE;
}



LiVESList *get_set_list(const char *dir, boolean utf8) {
  // get list of sets in top level dir
  // values will be in filename encoding

  LiVESList *setlist = NULL;
  DIR *tldir, *subdir;
  struct dirent *tdirent, *subdirent;
  char *subdirname;

  if (!dir) return NULL;

  tldir = opendir(dir);

  if (!tldir) return NULL;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  while (1) {
    tdirent = readdir(tldir);

    if (!tdirent) {
      closedir(tldir);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      return setlist;
    }

    if (tdirent->d_name[0] == '.'
        && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;

    subdirname = lives_build_filename(dir, tdirent->d_name, NULL);
    subdir = opendir(subdirname);

    if (!subdir) {
      lives_free(subdirname);
      continue;
    }

    while (1) {
      subdirent = readdir(subdir);
      if (!subdirent) break;

      if (!strcmp(subdirent->d_name, "order")) {
        if (!utf8)
          setlist = lives_list_append(setlist, lives_strdup(tdirent->d_name));
        else
          setlist = lives_list_append(setlist, F2U8(tdirent->d_name));
        break;
      }
    }
    lives_free(subdirname);
    closedir(subdir);
  }
}


/**
   @brief check for set lock file
   do this via the back-end (smogrify)
   this allows for the locking scheme to be more flexible

   smogrify indicates a lock very simply by by writing > 0 bytes to stdout
   we read this via popen

   type == 0 for load, type == 1 for save

*/
boolean check_for_lock_file(const char *set_name, int type) {
  char *com;

  if (type == 1 && !lives_strcmp(set_name, mainw->set_name)) return TRUE;

  com = lives_strdup_printf("%s check_for_lock \"%s\" \"%s\" %d", prefs->backend_sync, set_name, capable->myname,
                            capable->mainpid);

  clear_mainw_msg();

  threaded_dialog_spin(0.);
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  threaded_dialog_spin(0.);
  lives_free(com);

  if (THREADVAR(com_failed)) return FALSE;

  if (*(mainw->msg)) {
    if (type == 0) {
      if (mainw->recovering_files) return do_set_locked_warning(set_name);
      threaded_dialog_spin(0.);
      widget_opts.non_modal = TRUE;
      do_error_dialogf(_("Set %s\ncannot be opened, as it is in use\nby another copy of LiVES.\n"), set_name);
      widget_opts.non_modal = FALSE;
      threaded_dialog_spin(0.);
    } else if (type == 1) {
      if (!mainw->osc_auto) do_error_dialogf(_("\nThe set %s is currently in use by another copy of LiVES.\n"
                                               "Please choose another set name.\n"), set_name);
    }
    return FALSE;
  }
  return TRUE;
}


void open_set_file(int clipnum) {
  char name[CLIP_NAME_MAXLEN];
  lives_clip_t *sfile;

  if (!IS_VALID_CLIP(clipnum)) return;
  sfile = mainw->files[clipnum];

  if (!mainw->hdrs_cache && sfile->checked_for_old_header && !sfile->has_old_header)
    // probably restored from binfmt
    return;

  lives_memset(name, 0, CLIP_NAME_MAXLEN);

  if (mainw->hdrs_cache) {
    boolean retval;
    // LiVES 0.9.6+

    retval = get_clip_value(clipnum, CLIP_DETAILS_PB_FPS, &sfile->pb_fps, 0);
    if (!retval) {
      sfile->pb_fps = sfile->fps;
    }
    retval = get_clip_value(clipnum, CLIP_DETAILS_PB_FRAMENO, &sfile->frameno, 0);
    if (!retval) {
      sfile->frameno = 1;
    }

    retval = get_clip_value(clipnum, CLIP_DETAILS_CLIPNAME, name, CLIP_NAME_MAXLEN);
    if (!retval) {
      char *tmp;
      lives_snprintf(name, CLIP_NAME_MAXLEN, "%s", (tmp = get_untitled_name(mainw->untitled_number++)));
      lives_free(tmp);
      sfile->needs_update = TRUE;
    }
    retval = get_clip_value(clipnum, CLIP_DETAILS_UNIQUE_ID, &sfile->unique_id, 0);
    if (!retval) {
      sfile->unique_id = gen_unique_id();
      sfile->needs_silent_update = TRUE;
    }
    retval = get_clip_value(clipnum, CLIP_DETAILS_INTERLACE, &sfile->interlace, 0);
    if (!retval) {
      sfile->interlace = LIVES_INTERLACE_NONE;
      sfile->needs_silent_update = TRUE;
    }
    if (sfile->interlace != LIVES_INTERLACE_NONE) sfile->deinterlace = TRUE;
  } else {
    // pre 0.9.6 <- ancient code
    ssize_t nlen;
    int set_fd;
    int pb_fps;
    int retval;

    char *clipdir = get_clip_dir(clipnum);
    char *xsetfile = lives_strdup_printf("set.%s", mainw->set_name);
    char *setfile = lives_build_path(clipdir, xsetfile, NULL);

    lives_free(clipdir); lives_free(xsetfile);

    do {
      retval = 0;
      if ((set_fd = lives_open2(setfile, O_RDONLY)) > -1) {
        // get perf_start
        if ((nlen = lives_read_le(set_fd, &pb_fps, 4, TRUE)) > 0) {
          sfile->pb_fps = pb_fps / 1000.;
          lives_read_le(set_fd, &sfile->frameno, 4, TRUE);
          lives_read(set_fd, name, CLIP_NAME_MAXLEN, TRUE);
        }
        close(set_fd);
      } else retval = do_read_failed_error_s_with_retry(setfile, lives_strerror(errno));
    } while (retval == LIVES_RESPONSE_RETRY);

    lives_free(setfile);
    sfile->needs_silent_update = TRUE;
  }

  if (!*name) {
    lives_snprintf(name, CLIP_NAME_MAXLEN, "set_clip %.3d", clipnum);
  } else {
    // pre 3.x, files erroneously had the set name appended permanently, so here we undo that
    if (lives_string_ends_with(name, " (%s)", mainw->set_name)) {
      char *remove = lives_strdup_printf(" (%s)", mainw->set_name);
      if (strlen(name) > strlen(remove)) name[strlen(name) - strlen(remove)] = 0;
      lives_free(remove);
      sfile->needs_silent_update = TRUE;
    }
    lives_snprintf(sfile->name, CLIP_NAME_MAXLEN, "%s", name);
  }
}


// the name of this function is a bit deceptive,
// if "add" is TRUE, we dont just rewrite the orde file
// in fact we also move any floating clips into the set directory and include them in the order file
// thus making them official members of the set
// if add is FALSE then the function does what it says, recreates order file with only the clips which are already in the set
// for example after closing one of them
// if append is TRUE then the clips are appended, this is done for example when merging two sets together
// got_new_handle points to a boolean to receive a value, if TRUE then one or more clips were added to the set directory
// the return value is normally OK, but it can be CANCEL if writing failed and the user cancelled from error
//
// in any case, since this is an important file, updates are made to a new copy of the file, and only if everything succeeds
// will it be copied to the original (the backup is also retained, so it can be used for recovery purposes)
// it ought to be easy to recreate htis by parsing the subdirectories of /clips in the set but lack of time,,,
// - also reordering via interface would be nice...
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

      for (cliplist = mainw->cliplist; cliplist; cliplist = cliplist->next) {
        if (THREADVAR(write_failed)) break;
        threaded_dialog_spin(0.);
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

        i = LIVES_POINTER_TO_INT(cliplist->data);
        if (IS_NORMAL_CLIP(i) && i != mainw->scrap_file && i != mainw->ascrap_file) {
          if (ignore_clip(i)) continue;
          lives_snprintf(buff, PATH_MAX, "%s", mainw->files[i]->handle);
          get_basename(buff);
          if (*buff) {
            lives_snprintf(new_handle, 256, "%s/%s/%s", mainw->set_name, CLIPS_DIRNAME, buff);
          } else {
            lives_snprintf(new_handle, 256, "%s/%s/%s", mainw->set_name, CLIPS_DIRNAME, mainw->files[i]->handle);
          }

          // compare the clip's handle with what it would be if it were already in the set directory
          // if the values differ then this is a candidate for moving
          if (lives_strcmp(new_handle, mainw->files[i]->handle)) {
            if (!add) continue;

            new_dir = lives_build_path(prefs->workdir, new_handle, NULL);

            if (lives_file_test(new_dir, LIVES_FILE_TEST_IS_DIR)) {
              // get a new unique handle
              get_temp_handle(i);
              migrate_from_staging(mainw->current_file);
              lives_snprintf(new_handle, 256, "%s/%s/%s",
                             mainw->set_name, CLIPS_DIRNAME, mainw->files[i]->handle);
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

            dfile = lives_build_filename(prefs->workdir, mainw->files[i]->handle,
                                         LIVES_STATUS_FILE_NAME, NULL);

            lives_snprintf(mainw->files[i]->info_file, PATH_MAX, "%s", dfile);
            lives_free(dfile);
          }

          ord_entry = lives_strdup_printf("%s\n", mainw->files[i]->handle);
          lives_write(ord_fd, ord_entry, lives_strlen(ord_entry), FALSE);
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
  else lives_cp(ordfile_new, ordfile);

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
  // - called from prompt_for_set_save at lives_exit()
  // new_set_name can be passed in userdata, it should be in filename encoding
  // TODO - caller to do end_threaded_dialog()

  /////////////////
  /// IMPORTANT !!!  mainw->no_exit must be set. otherwise the app will exit
  ////////////

  char new_set_name[MAX_SET_NAME_LEN] = {0};

  char *old_set = lives_strdup(mainw->set_name);
  char *layout_map_file, *layout_map_dir, *new_clips_dir, *current_clips_dir;
  //char *tmp;
  char *text;

  char *tmp;
  char *omapf, *nmapf, *olayd, *nlayd;
  char *laydir, *clipdir = NULL;

  boolean is_append = FALSE; // we will overwrite the target layout.map file
  boolean response;
  boolean got_new_handle = FALSE;

  LiVESResponseType retval;

  if (!mainw->cliplist) return FALSE;

  // warn the user what will happen
  if (!user_data && !do_save_clipset_warn()) return FALSE;

  if (mainw->stored_event_list && mainw->stored_event_list_changed) {
    // if we have a current layout, give the user the chance to change their mind
    if (!check_for_layout_del(NULL, FALSE)) return FALSE;
  }

  if (!user_data) {
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
    } while (!is_legal_set_name(new_set_name, TRUE, FALSE));
  } else lives_snprintf(new_set_name, MAX_SET_NAME_LEN, "%s", (char *)user_data);

  lives_widget_queue_draw_and_update(LIVES_MAIN_WINDOW_WIDGET);

  lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", new_set_name);

  if (lives_strcmp(mainw->set_name, old_set)) {
    // The user CHANGED the set name or the working dir
    // we must migrate all physical files for the set, and possibly merge with another set

    new_clips_dir = CLIPS_DIR(mainw->set_name);
    // check if target clips dir exists, ask if user wants to append files

    // TODO - target may be in new workdir

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
      layout_map_file = LAYOUT_MAP_FILE(mainw->set_name);

      // if target has layouts dir but no clips, it means we have old layouts !
      if (lives_file_test(layout_map_file, LIVES_FILE_TEST_EXISTS)) {
        if (do_set_rename_old_layouts_warning(mainw->set_name)) {
          // user answered "yes" - delete
          // clear _old_layout maps
          laydir = LAYOUTS_DIR(mainw->set_name);
          lives_rmdir(laydir, TRUE);
          lives_free(laydir);
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

  current_clips_dir = CLIPS_DIR(old_set);

  if (*old_set && lives_strcmp(mainw->set_name, old_set)
      && lives_file_test(current_clips_dir, LIVES_FILE_TEST_IS_DIR)) {
    // set name was changed for an existing set
    if (!is_append) {
      // create new dir, in case it doesn't already exist
      clipdir = CLIPS_DIR(mainw->set_name);
    }
  } else {
    clipdir = CLIPS_DIR(mainw->set_name);
  }
  if (clipdir) {
    if (!lives_make_writeable_dir(clipdir)) {
      // abort if we cannot create the new subdir
      LIVES_ERROR("Could not create directory");
      LIVES_ERROR(clipdir);
      d_print_file_error_failed();
      lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", old_set);
      lives_free(clipdir);
      end_threaded_dialog();
      return FALSE;
    }
    lives_free(clipdir);
  }
  lives_free(current_clips_dir);

  if (mainw->scrap_file > -1) close_scrap_file(TRUE);
  if (mainw->ascrap_file > -1) close_ascrap_file(TRUE);

  // here we actually move the directories and update the order file for the set
  retval = rewrite_orderfile(is_append, TRUE, &got_new_handle);

  if (retval == LIVES_RESPONSE_CANCEL) {
    end_threaded_dialog();
    return FALSE;
  }

  if (mainw->num_sets > -1) {
    mainw->num_sets++;
    if (!*old_set) mainw->set_list = lives_list_prepend(mainw->set_list, lives_strdup(mainw->set_name));
  } else {
    mainw->num_sets = 1;
    mainw->set_list = lives_list_prepend(mainw->set_list, lives_strdup(mainw->set_name));
  }

  if (got_new_handle && !*old_set) migrate_layouts(NULL, mainw->set_name);

  if (*old_set && lives_strcmp(old_set, mainw->set_name)) {
    layout_map_dir = LAYOUTS_DIR(old_set);
    layout_map_file = LAYOUT_MAP_FILE(old_set);
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
      omapf = LAYOUT_MAP_FILE(old_set);
      nmapf = LAYOUT_MAP_FILE(mainw->set_name);

      if (lives_file_test(omapf, LIVES_FILE_TEST_EXISTS)) {
        //append current layout.map to target one
        lives_cat(omapf, nmapf, TRUE); /// command may not fail, so we check first
        lives_rm(omapf);
      }
      lives_free(omapf);
      lives_free(nmapf);
    }

    olayd = LAYOUTS_DIR(old_set);

    if (lives_file_test(olayd, LIVES_FILE_TEST_IS_DIR)) {
      nlayd = LAYOUTS_DIR(mainw->set_name);

      // move any layouts from old set to new (including layout.map)
      lives_cp_keep_perms(olayd, nlayd);

      lives_free(nlayd);
    }

    lives_free(olayd);

    // remove the old set (should be empty now)
    cleanup_set_dir(old_set);
  }

  if (!mainw->was_set && !lives_strcmp(old_set, mainw->set_name)) {
    // set name was set by export or save layout, now we need to update our layout map
    layout_map_dir = LAYOUTS_DIR(old_set);

    layout_map_file = lives_build_filename(layout_map_dir, LAYOUT_MAP_FILENAME, NULL);

    if (lives_file_test(layout_map_file, LIVES_FILE_TEST_EXISTS)) save_layout_map(NULL, NULL, NULL, layout_map_dir);
    mainw->was_set = TRUE;
    got_new_handle = FALSE;
    lives_free(layout_map_dir);
    lives_free(layout_map_file);
    if (mainw->multitrack && !mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  if (mainw->current_layouts_map && strcmp(old_set, mainw->set_name) && !mainw->suppress_layout_warnings) {
    // warn the user about layouts if the set name changed
    // but, don't bother the user with errors if we are exiting
    add_lmap_error(LMAP_INFO_SETNAME_CHANGED, old_set, mainw->set_name, 0, 0, 0., FALSE);
    popup_lmap_errors(NULL, NULL);
  }

  lives_notify(LIVES_OSC_NOTIFY_CLIPSET_SAVED, old_set);

  lives_free(old_set);
  mainw->leave_files = TRUE;
  if (mainw->multitrack && !mainw->only_close) mt_memory_free();
  else if (mainw->multitrack) wipe_layout(mainw->multitrack);

  // do a lot of cleanup here, but leave files
  lives_exit(0);
  mainw->leave_files = FALSE;
  //end_threaded_dialog();

  lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
  return TRUE;
}


char *on_load_set_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // get set name (use a modified rename window)
  char *set_name = NULL;
  LiVESResponseType resp;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack) {
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
    if (!is_legal_set_name(set_name, TRUE, TRUE)) {
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
  // function is called when a set is opened, to prevent multiple access to the same set
  char *setdir = lives_build_path(prefs->workdir, set_name, NULL);
  if (lives_file_test(setdir, LIVES_FILE_TEST_IS_DIR)) {
    char *set_lock_file = lives_strdup_printf("%s%d", SET_LOCK_FILENAME, capable->mainpid);
    char *set_locker = SET_LOCK_FILE(set_name, set_lock_file);
    lives_touch(set_locker);
    lives_free(set_locker);
    lives_free(set_lock_file);
  }
  lives_free(setdir);
}


void unlock_set_file(const char *set_name) {
  char *set_lock_file = lives_strdup_printf("%s%d", SET_LOCK_FILENAME, capable->mainpid);
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
  char *ignore;

  boolean added_recovery = FALSE;
  boolean keep_threaded_dialog = FALSE;
  boolean hadbad = FALSE;

  int last_file = -1, new_file = -1;
  int current_file = mainw->current_file;
  int clipnum = 0;
  int maxframe;
  int ignored = 0;
  mainw->mt_needs_idlefunc = FALSE;

  if (mainw->multitrack) {
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
      if (mainw->multitrack) {
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
      if (!lives_fgets(mainw->msg, MAINW_MSG_SIZE, orderfile)) clear_mainw_msg();
      else lives_memset(mainw->msg + lives_strlen(mainw->msg) - 1, 0, 1);
    }

    if (!(*mainw->msg) || (!strncmp(mainw->msg, "none", 4))) {
      if (!mainw->recovering_files) mainw->suppress_dprint = FALSE;
      if (!keep_threaded_dialog) end_threaded_dialog();

      if (orderfile) fclose(orderfile);

      //mainw->current_file = current_file;

      if (!mainw->multitrack) {
        if (last_file > 0) {
          threaded_dialog_spin(0.);
          switch_to_file(current_file, last_file);
          threaded_dialog_spin(0.);
        }
      }

      if (clipnum == 0) {
        char *dirname = lives_build_path(prefs->workdir, set_name, NULL);
        // if dir exists, and there are no subdirs, ask user if they want to delete
        if (lives_file_test(dirname, LIVES_FILE_TEST_IS_DIR)) {
          //// todo test if no subdirs
          if (ignored) {
            char *ignore = lives_build_filename(CURRENT_SET_DIR, LIVES_FILENAME_IGNORE, NULL);
            lives_touch(ignore);
            lives_free(ignore);
          } else {
            if (do_set_noclips_query(set_name)) {
              lives_rmdir(dirname, TRUE);
              lives_free(dirname);
            }
          }
        } else {
          // this is appropriate if the dir doens not exist, an !recovery
          do_set_noclips_error(set_name);
        }
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
      if (!mainw->multitrack) {
        if (mainw->is_ready) {
          if (clipnum > 0 && CURRENT_CLIP_IS_VALID) {
            showclipimgs();
          }
          // force a redraw
          update_play_times();
          redraw_timeline(mainw->current_file);
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
      if (mainw->multitrack)
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
    ignore = lives_build_filename(clipdir, LIVES_FILENAME_IGNORE, NULL);
    if (lives_file_test(ignore, LIVES_FILE_TEST_EXISTS)) {
      lives_free(clipdir);
      lives_free(ignore);
      ignored++;
      continue;
    }
    lives_free(ignore);

    if (orderfile) {
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

      if (mainw->multitrack) {
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

    // set this here, as some functions like to know
    cfile->was_in_set = TRUE;

    // get file details
    if (!read_headers(mainw->current_file, clipdir, NULL)) {
      /// set clip failed to load, when this happens
      // the user can choose to delete it or mark it as ignored
      /// else it will keep trying to reload each time.
      ignore = lives_build_filename(clipdir, LIVES_FILENAME_IGNORE, NULL);
      if (lives_file_test(ignore, LIVES_FILE_TEST_EXISTS)) {
        ignored++;
      }
      lives_free(ignore);
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

      /** if the image type is still unknown it means either there were no decoded frames, or the final decoded frame was absent
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
        - this can be pretty slow so we want to avoid it unless  we detected a problem. */
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

    if (update_clips_version(mainw->current_file)) cfile->needs_silent_update = TRUE;

    threaded_dialog_spin(0.);

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

    if (!mainw->multitrack) {
      resize(1);
    }

    if (mainw->multitrack && mainw->multitrack->is_ready) {
      new_file = mainw->current_file;
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


void del_current_set(boolean exit_after) {
  char *msg;
  boolean moc = mainw->only_close, crec;
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

  crec = prefs->crash_recovery;
  prefs->crash_recovery = FALSE;

  // do a lot of cleanup, delete files
  lives_exit(0);
  prefs->crash_recovery = crec;

  if (*mainw->set_name) {
    d_print(_("Set %s was permanently deleted from the disk.\n"), mainw->set_name);
    lives_memset(mainw->set_name, 0, 1);
  }
  mainw->only_close = moc;
  end_threaded_dialog();
}


LiVESResponseType prompt_for_set_save(void) {
  // function is called from lives_exit() to decide what to do with currently opened clips
  // user is offered the choice to save them as part of a clip set (including the possibility of merging with an existing set)
  // or to delete them (including the possibility of deleting the set from disk)
  // also a Cancel option is presented
  // if the working directory is being changed then there is a further option of saving the set in the new location
  // or in the old (discounting the possibilty of saving in the current to-be-deleted directory

  // when moving workdir: - save set as usual
  // then on return we either move everything, or just the saved set
  // the maybe delete old workdir

  _entryw *cdsw = create_cds_dialog(1);
  char *set_name, *tmp;
  LiVESResponseType resp;
  boolean legal_set_name;

  do {
    legal_set_name = TRUE;
    lives_widget_show_all(cdsw->dialog);

    // we have three additional options when changing workdir
    // - save set then update workdir, with no migration (save to current workdir - use new workdir)
    // - update workidr first, then save current clipset, no further migration (save to new workdr, use it)
    // - save in new work, then old is wiped (transfer current clips, erase)

    resp = lives_dialog_run(LIVES_DIALOG(cdsw->dialog));

    if (resp == LIVES_RESPONSE_RETRY) continue;

    if (resp == LIVES_RESPONSE_CANCEL) {
      lives_widget_destroy(cdsw->dialog);
      lives_free(cdsw);
      return resp;
    }

    if (resp == LIVES_RESPONSE_ACCEPT || resp == LIVES_RESPONSE_YES) {
      // save set
      if ((legal_set_name =
             is_legal_set_name((set_name = U82F(lives_entry_get_text(LIVES_ENTRY(cdsw->entry)))),
                               TRUE, FALSE))) {
        lives_widget_destroy(cdsw->dialog);
        lives_free(cdsw);

        if (prefs->ar_clipset) set_string_pref(PREF_AR_CLIPSET, set_name);
        else set_string_pref(PREF_AR_CLIPSET, "");

        mainw->leave_recovery = FALSE;

        // here we actually move any new directories into the set directory
        on_save_set_activate(NULL, (tmp = U82F(set_name)));

        lives_free(tmp);
        lives_free(set_name);
        return resp;
      }
      resp = LIVES_RESPONSE_RETRY;
      lives_widget_hide(cdsw->dialog);
      lives_free(set_name);
    }
    if (resp == LIVES_RESPONSE_RESET) {
      // wipe from disk
      if (prefs->workdir_tx_intent != LIVES_INTENTION_DELETE) {
        char *what, *expl;
        // TODO
        //if (check_for_executable(&capable->has_gio, EXEC_GIO)) mainw->add_trash_rb = TRUE;
        if (mainw->was_set) {
          what = lives_strdup_printf(_("Set '%s'"), mainw->set_name);
          expl = lives_strdup("");
        } else {
          what = (_("All currently open clips"));
          expl = (_("<b>(Note: original source material will NOT be affected !)</b>"));
          widget_opts.use_markup = TRUE;
        }
        if (!do_warning_dialogf(_("\n\n%s will be permanently deleted from the disk.\n"
                                  "Are you sure ?\n\n%s"), what, expl)) {
          resp = LIVES_RESPONSE_ABORT;
        }
        widget_opts.use_markup = FALSE;
        lives_free(what); lives_free(expl);
        //mainw->add_trash_rb = FALSE;
      }
    }
  } while (resp == LIVES_RESPONSE_ABORT || resp == LIVES_RESPONSE_RETRY);

  lives_widget_destroy(cdsw->dialog);
  lives_free(cdsw);

  // discard clipset
  del_current_set(!mainw->only_close);
  return resp;
}


boolean do_save_clipset_warn(void) {
  char *extra;
  char *msg;

  if (!mainw->no_exit && !mainw->only_close) extra = lives_strdup(", and LiVES will exit");
  else extra = lives_strdup("");

  msg = lives_strdup_printf(
          _("Saving the set will cause copies of all loaded clips to remain on the disk%s.\n\n"
            "Please press 'Cancel' if that is not what you want.\n"), extra);
  lives_free(extra);

  if (!do_warning_dialog_with_check(msg, WARN_MASK_SAVE_SET)) {
    lives_free(msg);
    return FALSE;
  }
  lives_free(msg);
  return TRUE;
}


//////////// clip groups ///

void new_clipgroup(LiVESWidget *w, livespointer data) {


}
