// sethandler.c
// (c) G. Finch 2019 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "interface.h"
#include "callbacks.h"
#include "cvirtual.h"

// TODO - save clipgroups and bookmarks for the set
// and clip audio volumes
// allow reordering of clips in set

// general metadata

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
    char *set_dir = SET_DIR(set_name);
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

      if (!strcmp(subdirent->d_name, CLIP_ORDER_FILENAME)) {
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
      if (prefs->show_dev_opts) {
        g_printerr("clip name not found\n");
      }
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
    char *xsetfile = lives_strdup_printf("%s.%s", SET_LITERAL, mainw->set_name);
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
static LiVESResponseType rewrite_orderfile(boolean is_append, boolean add, boolean *got_new_handle) {
  char *ordfile = SET_ORDER_FILE(mainw->set_name);
  char *ordfile_new = lives_build_filename(CURRENT_SET_DIR, CLIP_ORDER_FILENAME "." LIVES_FILE_EXT_NEW, NULL);
  char *cwd = lives_get_current_dir();
  char *new_dir;
  char *dfile, *ord_entry, *xhand;
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
          lives_clip_t *sfile;
          if (ignore_clip(i)) continue;
          sfile = mainw->files[i];
          sfile->was_in_set = TRUE;
          dump_clip_binfmt(i);
          lives_snprintf(buff, PATH_MAX, "%s", sfile->handle);
          get_basename(buff);
          if (*buff) {
            xhand = CLIP_HANDLE(mainw->set_name, buff);
          } else {
            xhand = CLIP_HANDLE(mainw->set_name, sfile->handle);
          }
          lives_snprintf(new_handle, 256, "%s", xhand);
          lives_free(xhand);

          // compare the clip's handle with what it would be if it were already in the set directory
          // if the values differ then this is a candidate for moving
          if (lives_strcmp(new_handle, sfile->handle)) {
            if (!add) continue;

            new_dir = lives_build_path(prefs->workdir, new_handle, NULL);

            if (lives_file_test(new_dir, LIVES_FILE_TEST_IS_DIR)) {
              // get a new unique handle
              get_temp_handle(i);
              migrate_from_staging(i);
              xhand = CLIP_HANDLE(mainw->set_name, sfile->handle);
              lives_snprintf(new_handle, 256, "%s", xhand);
              lives_free(xhand);
            }
            lives_free(new_dir);

            // move the files
            oldval = lives_build_path(prefs->workdir, sfile->handle, NULL);
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

            lives_snprintf(sfile->handle, 256, "%s", new_handle);

            dfile = lives_build_filename(prefs->workdir, sfile->handle,
                                         LIVES_STATUS_FILE_NAME, NULL);

            lives_snprintf(sfile->info_file, PATH_MAX, "%s", dfile);
            lives_free(dfile);
          }

          ord_entry = lives_strdup_printf("%s\n", sfile->handle);
          lives_write(ord_fd, ord_entry, lives_strlen(ord_entry), FALSE);
          lives_free(ord_entry);
        }
      }

      if (THREADVAR(write_failed)) {
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


boolean on_save_set_activate(LiVESWidget *widget, livespointer user_data) {
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
    _entryw *entryw = create_entry_dialog(ENTRYW_SAVE_SET);
    do {
      // prompt for a set name, advise user to save set
      response = lives_dialog_run(LIVES_DIALOG(entryw->dialog));
      if (response == LIVES_RESPONSE_CANCEL) return FALSE;
      lives_snprintf(new_set_name, MAX_SET_NAME_LEN, "%s",
                     (tmp = U82F(lives_entry_get_text(LIVES_ENTRY(entryw->entry)))));
      lives_free(tmp);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    } while (!is_legal_set_name(new_set_name, TRUE, FALSE));
    lives_widget_destroy(entryw->dialog);
    lives_free(entryw);
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
    sensitize();
    return FALSE;
  }


  if (mainw->num_sets == -1) {
    mainw->set_list = get_set_list(prefs->workdir, TRUE);
    mainw->num_sets = lives_list_length(mainw->set_list);
    if (*old_set) mainw->num_sets--;
  }
  mainw->num_sets++;
  if (!*old_set) mainw->set_list = lives_list_prepend(mainw->set_list, lives_strdup(mainw->set_name));

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

  if (mainw->bad_lmaps) {
    LiVESList *lmap_node = mainw->bad_lmaps;
    while (lmap_node) {
      layout_map *lmap_entry = (layout_map *)lmap_node->data;
      if (lmap_entry->name) lives_free(lmap_entry->name);
      if (lmap_entry->handle) lives_free(lmap_entry->handle);
      lives_list_free_all(&lmap_entry->list);
      lmap_node = lmap_node->next;
    }
    lives_list_free(mainw->bad_lmaps);
    mainw->bad_lmaps = NULL;
  }

  if (!mainw->was_set && !lives_strcmp(old_set, mainw->set_name)) {
    // set name was set by export or save layout, now we need to update our layout map
    layout_map_dir = LAYOUTS_DIR(old_set);
    layout_map_file = LAYOUT_MAP_FILE(old_set);
    if (lives_file_test(layout_map_file, LIVES_FILE_TEST_EXISTS)) save_layout_map(NULL, NULL, NULL, layout_map_dir);
    mainw->was_set = TRUE;
    got_new_handle = FALSE;
    lives_free(layout_map_dir);
    lives_free(layout_map_file);
    if (mainw->multitrack && !mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  if (mainw->current_layouts_map && lives_strcmp(old_set, mainw->set_name) && !mainw->suppress_layout_warnings) {
    // warn the user about layouts if the set name changed
    // but, don't bother the user with errors if we are exiting
    add_lmap_error(LMAP_INFO_SETNAME_CHANGED, old_set, mainw->set_name, 0, 0, 0., FALSE);
    popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(LMAP_INFO_SETNAME_CHANGED));
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

  sensitize();
  return TRUE;
}


void recover_layout_map(void) {
  // load global layout map for a set and assign entries to clips [mainw->files[i]->layout_map]
  // each entry is a list of mappings for a particular clip, mapping e.g. clip index, as well as max frame, max aud time
  // here we match each bundle with its original clip when the layout was saved
  // we match using unique_id and handle, if we fail to find a match then we try to match EITHER uid or handle
  // (this shouldn't be necassary, but sometimes is needed due to buggy code or test scenarios)
  LiVESList *mlist, *lmap_node, *lmap_node_next, *lmap_entry_list, *lmap_entry_list_next;

  layout_map *lmap_entry;
  uint32_t mask = 0;

  char **array;
  char *check_handle;

  if (prefs->vj_mode) return;

  if ((mlist = load_layout_map())) {
    for (int pass = 0; pass < 2; pass++) {
      // assign layout map to clips
      for (int i = 1; i < MAX_FILES && mlist; i++) {
        if (!IS_NORMAL_CLIP(i) || (mainw->multitrack && i == mainw->multitrack->render_file)
            || i == mainw->scrap_file || i == mainw->ascrap_file) continue;
        lives_clip_t *sfile = mainw->files[i];
        lmap_node = mlist;
        while (lmap_node) {
          lmap_node_next = lmap_node->next;
          lmap_entry = (layout_map *)lmap_node->data;
          check_handle = lives_strdup(sfile->handle);

          if (!strstr(lmap_entry->handle, "/")) {
            lives_free(check_handle);
            check_handle = lives_path_get_basename(sfile->handle);
          }

          //g_print("CF %s and %s, %lu and %lu\n", check_handle, lmap_entry->handle, sfile->unique_id, lmap_entry->unique_id);

          if ((!lives_strcmp(check_handle, lmap_entry->handle) && (sfile->unique_id == lmap_entry->unique_id)) ||
              ((prefs->mt_load_fuzzy || pass == 1)
               && (!strcmp(check_handle, lmap_entry->handle) || (sfile->unique_id == lmap_entry->unique_id)))
             ) {
            // check handle and unique id match
            // got a match, assign list to clip layout_map and delete this node, then continue to next clip

            lmap_entry_list = lmap_entry->list;
            while (lmap_entry_list) {
              lmap_entry_list_next = lmap_entry_list->next;
              array = lives_strsplit((char *)lmap_entry_list->data, "|", -1);
              if (!lives_file_test(array[0], LIVES_FILE_TEST_EXISTS)) {
                g_print("removing layout because no file %s\n", array[0]);
                // layout file has been deleted, remove this entry
                lmap_entry->list = lives_list_remove_node(lmap_entry->list, lmap_entry_list, TRUE);
              }
              lives_strfreev(array);
              lmap_entry_list = lmap_entry_list_next;
            }
            sfile->layout_map = lmap_entry->list;

            lives_free(lmap_entry->handle);
            lives_free(lmap_entry->name);
            lives_free(lmap_entry);

            mlist = lives_list_remove_node(mlist, lmap_node, FALSE);

            if (sfile->layout_map) {
              /// check for missing frames and audio in layouts
              // TODO: -- needs checking ----
              mainw->xlays = layout_frame_is_affected(i, sfile->frames + 1, 0, mainw->xlays);
              if (mainw->xlays) {
                add_lmap_error(LMAP_ERROR_DELETE_FRAMES, sfile->name, (livespointer)sfile->layout_map, i,
                               sfile->frames, 0., FALSE);
                lives_list_free_all(&mainw->xlays);
                mask |= WARN_MASK_LAYOUT_DELETE_FRAMES;
                //g_print("FRMS %d\n", cfile->frames);
              }

              mainw->xlays = layout_audio_is_affected(i, sfile->laudio_time, 0., mainw->xlays);

              if (mainw->xlays) {
                add_lmap_error(LMAP_ERROR_DELETE_AUDIO, sfile->name, (livespointer)sfile->layout_map, i,
                               sfile->frames, sfile->laudio_time, FALSE);
                lives_list_free_all(&mainw->xlays);
                mask |= WARN_MASK_LAYOUT_DELETE_AUDIO;
                //g_print("AUD %f\n", cfile->laudio_time);
              }
            }
            lives_free(check_handle);
            break;
          }
          lives_free(check_handle);
          lmap_node = lmap_node_next;
        }
      }
      if (!mlist) break;
    }

    mainw->bad_lmaps = mlist;
    if (prefs->show_dev_opts) {
      for (; mlist; mlist = mlist->next) {
        lmap_entry = (layout_map *)mlist->data;
        g_print("Failed to find clip %s with uid 0X%016lX, needed in layouts\n", lmap_entry->handle,
                lmap_entry->unique_id);
      }
    }
  }
  if (mask) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(mask));
}


char *on_load_set_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // get set name (use a modified rename window)
  _entryw *entryw;
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

  entryw = create_entry_dialog(ENTRYW_RELOAD_SET);
  if (!entryw) return NULL; ///< no sets available

  resp = lives_dialog_run(LIVES_DIALOG(entryw->dialog));

  if (resp == LIVES_RESPONSE_OK) {
    set_name = U82F(lives_entry_get_text(LIVES_ENTRY(entryw->entry)));
  }

  // need to clean up entryw
  lives_widget_destroy(entryw->dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_freep((void **)&entryw);

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
  char *setdir = SET_DIR(set_name);
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
  boolean hadbad = FALSE;
  boolean use_dec;

  int last_file = -1, new_file = -1;
  int start_clip = mainw->current_file == -1 ? 1 : mainw->current_file + 1;
  int clipcount = start_clip, ignored = 0;
  frames_t maxframe;

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

  if (prefs->show_gui && !mainw->recovering_files) {
    char *tmp;
    msg = lives_strdup_printf(_("Loading clips from set %s"), (tmp = F2U8(set_name)));
    do_threaded_dialog(msg, FALSE);
    lives_free(msg);
    lives_free(tmp);
  }

  ordfile = SET_ORDER_FILE(set_name);

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
      if (orderfile) fclose(orderfile);

      if (!mainw->recovering_files) {
        mainw->suppress_dprint = FALSE;
        end_threaded_dialog();
      }

      if (!mainw->multitrack) {
        if (last_file > 0) switch_clip(1, last_file, TRUE);
      }

      if (clipcount == start_clip) {
        // no clips recovered from set
        char *dirname = SET_DIR(set_name);
        // if dir exists, and there are no subdirs, ask user if they want to delete
        if (lives_file_test(dirname, LIVES_FILE_TEST_IS_DIR)) {
          //// todo test if no subdirs
          if (ignored) {
            // if all files ignored, ignore the entire set
            char *ignore = lives_build_filename(CURRENT_SET_DIR, LIVES_FILENAME_IGNORE, NULL);
            lives_touch(ignore);
            lives_free(ignore);
          } else {
            char *cdirname = CLIPS_DIR(set_name);
            if (lives_file_test(cdirname, LIVES_FILE_TEST_IS_DIR)) {
              do_set_fail_load_error(set_name);
            } else {
              if (do_set_noclips_query(set_name)) {
                lives_rmdir(dirname, TRUE);
              }
            }
            lives_free(cdirname);
          }
        } else {
          // this is appropriate if the dir does not exist, and !recovery
          do_set_noclips_error(set_name);
        }
        lives_free(dirname);
      } else {
        char *tmp;
        reset_clipmenu();
        lives_widget_set_sensitive(mainw->vj_load_set, FALSE);

        if (hadbad) rewrite_orderfile(FALSE, FALSE, NULL);

        if (clipcount != start_clip && !mainw->recovering_files) {
          // if we are truly recovering files, we allow error correction later on
          // otherwise we might end up calling this twice
          mainw->recovering_files = TRUE; // allow error correction
          // MUST set set_name before calling this
          recover_layout_map();
          mainw->recovering_files = FALSE;
        }

        d_print(_("%d clips and %d layouts were recovered from set (%s).\n"),
                clipcount - start_clip, lives_list_length(mainw->current_layouts_map), (tmp = F2U8(set_name)));
        lives_free(tmp);
        lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", set_name);
        lives_notify(LIVES_OSC_NOTIFY_CLIPSET_OPENED, mainw->set_name);
      }

      if (!mainw->multitrack) {
        if (clipcount != start_clip && CURRENT_CLIP_IS_VALID) {
          showclipimgs();
        }
        if (mainw->is_ready) {
          // force a redraw
          get_play_times();
          redraw_timeline(mainw->current_file);
          //lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
        }
      } else {
        mainw->current_file = mainw->multitrack->render_file;
        polymorph(mainw->multitrack, POLY_NONE);
        polymorph(mainw->multitrack, POLY_CLIPS);
        mt_clip_select(mainw->multitrack, TRUE); // scroll clip on screen
      }
      lives_chdir(cwd, FALSE);
      lives_free(cwd);

      if (mainw->multitrack) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      } else sensitize();
      return TRUE;
    }

    clipdir = lives_build_filename(prefs->workdir, mainw->msg, NULL);
    if (!lives_file_test(clipdir, LIVES_FILE_TEST_IS_DIR)) {
      lives_free(clipdir);
      continue;
    }

    ignore = lives_build_filename(clipdir, LIVES_FILENAME_IGNORE, NULL);
    if (lives_file_test(ignore, LIVES_FILE_TEST_EXISTS)) {
      ignored++;
      lives_free(clipdir);
      lives_free(ignore);
      continue;
    }

    if (orderfile) {
      // newer style (0.9.6+)
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
      lives_free(ignore);
      mainw->suppress_dprint = FALSE;

      if (!mainw->recovering_files) end_threaded_dialog();

      if (mainw->multitrack) {
        mainw->current_file = mainw->multitrack->render_file;
        polymorph(mainw->multitrack, POLY_NONE);
        polymorph(mainw->multitrack, POLY_CLIPS);
      }

      lives_chdir(cwd, FALSE);
      lives_free(cwd);
      fclose(orderfile);

      if (mainw->multitrack) {
        mt_sensitise(mainw->multitrack);
        maybe_add_mt_idlefunc();
      } else sensitize();

      return FALSE;
    }

    // set this here, as some functions like to know
    cfile->was_in_set = TRUE;

    // get file details
    if (!read_headers(mainw->current_file, clipdir, NULL)) {
      /// set clip failed to load, when this happens
      // the user can choose to delete it or mark it as ignored
      /// else it will keep trying to reload each time.
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
    lives_free(ignore);

    threaded_dialog_spin(0.);

    /** read_headers() will have read the clip metadata file, so we 'know' how many frames there ought to be.
      however this could be wrong if the file was damaged for some reason. So the first step is to read the file_index if any.
      - if present we assume we are dealing with CLIP_TYPE_FILE (original clip + decoded frames).
      -- If it contains more frames then we will increase cfile->frames.
      - If it is not present then we assume are dealing with CLIP_TYPE_DISK (all frames decoded to images).

      we want to do as little checking here as possible, since it can slow down the startup, but if we detect a problem then we'll
      do increasingly more checking.
    */

    use_dec = should_use_decoder(mainw->current_file);
    maxframe = load_frame_index(mainw->current_file);

    if (use_dec || maxframe) {
      // CLIP_TYPE_FILE
      /** here we attempt to reload the clip. First we load the frame_index if any,
        If it contains more frames than the metadata says, then
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
        if (prefs->show_dev_opts) {
          g_printerr("invalid framecount detected\n");
        }
        cfile->needs_update = TRUE;
      }
    }

    if (prefs->crash_recovery && !added_recovery) {
      char *recovery_entry = lives_build_filename_relative(set_name, "*", NULL);
      add_to_recovery_file(recovery_entry);
      lives_free(recovery_entry);
      added_recovery = TRUE;
    }

    if (!prefs->vj_mode) {
      if (cfile->achans > 0 && cfile->afilesize == 0) {
        reget_afilesize_inner(mainw->current_file);
      }
    }

    last_file = new_file;

    if (clipcount++ == start_clip) {
      // we recovered our first clip from the set...
      /** we need to set the set_name before calling add_to_clipmenu(), so that the clip gets the name of the set in
        its menuentry, and also prior to loading any layouts since they specirfy the clipset they need */
      lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", set_name);
      mainw->was_set = TRUE;
    }

    /// read the playback fps, play frame, and name
    open_set_file(clipcount); ///< must do before calling save_clip_values()
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
      cfile->async_delta = 0;
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


void remove_layout_files(LiVESList *map) {
  // removes a LiVESList of layouts from the set layout map

  // removes from: - global layouts map
  //               - disk
  //               - clip layout maps

  // called after, for example: a clip is removed or altered and the user opts to remove all associated layouts

  LiVESList *lmap, *lmap_next, *cmap, *cmap_next, *map_next;
  size_t maplen;
  char **array;
  char *fname, *fdir;
  boolean is_current;

  while (map) {
    map_next = map->next;
    if (map->data) {
      if (!lives_utf8_strcasecmp((char *)map->data, mainw->string_constants[LIVES_STRING_CONSTANT_CL])) {
        is_current = TRUE;
        fname = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      } else {
        is_current = FALSE;
        maplen = lives_strlen((char *)map->data);

        // remove from mainw->current_layouts_map
        cmap = mainw->current_layouts_map;
        while (cmap) {
          cmap_next = cmap->next;
          if (!lives_utf8_strcasecmp((char *)cmap->data, (char *)map->data)) {
            lives_free((livespointer)cmap->data);
            mainw->current_layouts_map = lives_list_delete_link(mainw->current_layouts_map, cmap);
            break;
          }
          cmap = cmap_next;
        }

        array = lives_strsplit((char *)map->data, "|", -1);
        fname = repl_workdir(array[0], FALSE);
        lives_strfreev(array);
      }

      // fname should now hold the layout name on disk
      d_print(_("Removing layout %s\n"), fname);

      if (!is_current) {
        lives_rm(fname);

        // if no more layouts in parent dir, we can delete dir

        // ensure that parent dir is below our own working dir
        if (!lives_strncmp(fname, prefs->workdir, lives_strlen(prefs->workdir))) {
          // is in workdir, safe to remove parents

          char *protect_file = lives_build_filename(prefs->workdir, LIVES_FILENAME_NOREMOVE, NULL);

          // touch a file in tpmdir, so we cannot remove workdir itself
          lives_touch(protect_file);

          if (!THREADVAR(com_failed)) {
            // ok, the "touch" worked
            // now we call rmdir -p : remove directory + any empty parents
            if (lives_file_test(protect_file, LIVES_FILE_TEST_IS_REGULAR)) {
              fdir = lives_path_get_dirname(fname);
              lives_rmdir_with_parents(fdir);
              lives_free(fdir);
            }
          }

          // remove the file we touched to clean up
          lives_rm(protect_file);
          lives_free(protect_file);
        }

        // remove from mainw->files[]->layout_map
        for (int i = 1; i <= MAX_FILES; i++) {
          if (mainw->files[i]) {
            if (mainw->files[i]->layout_map) {
              lmap = mainw->files[i]->layout_map;
              while (lmap) {
                lmap_next = lmap->next;
                if (!lives_strncmp((char *)lmap->data, (char *)map->data, maplen)) {
                  lives_free((livespointer)lmap->data);
                  mainw->files[i]->layout_map = lives_list_delete_link(mainw->files[i]->layout_map, lmap);
                }
                lmap = lmap_next;
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*

      } else {
        // asked to remove the currently loaded layout

        if (mainw->stored_event_list || mainw->sl_undo_mem) {
          // we are in CE mode, so event_list is in storage
          stored_event_list_free_all(TRUE);
        }
        // in mt mode we need to do more
        else remove_current_from_affected_layouts(mainw->multitrack);

        // and we dont want to try reloading this next time
        prefs->ar_layout = FALSE;
        set_string_pref(PREF_AR_LAYOUT, "");
        lives_memset(prefs->ar_layout_name, 0, 1);
      }
      lives_free(fname);
    }
    map = map_next;
  }

  // save updated layout.map
  save_layout_map(NULL, NULL, NULL, NULL);
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
        if (!do_yesno_dialogf_with_countdown(3, TRUE, _("\n\n%s will be permanently deleted from the disk.\n"
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


void cleanup_set_dir(const char *set_name) {
  // this function is called:
  // - when a set is saved and merged with an existing one
  // - when a set is deleted
  // - when the last clip in a set is closed

  char *lfiles, *ofile, *sdir;
  char *cwd = lives_get_current_dir();

  sdir = LAYOUTS_DIR(set_name);
  if (lives_file_test(sdir, LIVES_FILE_TEST_IS_DIR))
    lives_rmdir(sdir, FALSE);
  lives_free(sdir);

  sdir = CLIPS_DIR(set_name);
  if (lives_file_test(sdir, LIVES_FILE_TEST_IS_DIR))
    lives_rmdir(sdir, FALSE);
  lives_free(sdir);

  // remove any stale lockfiles
  lfiles = SET_LOCK_FILES_PREFIX(set_name);

  if (!lives_chdir(SET_DIR(set_name), TRUE)) {
    lives_rmglob(lfiles);
    lives_chdir(cwd, TRUE);
  }
  lives_free(lfiles);

  ofile = SET_ORDER_FILE(set_name);
  lives_rm(ofile);
  lives_free(ofile);

  ofile = lives_build_filename(SET_DIR(set_name), CLIP_ORDER_FILENAME "." LIVES_FILE_EXT_NEW, NULL);
  lives_rm(ofile);
  lives_free(ofile);

  lives_sync(1);

  sdir = SET_DIR(set_name);
  lives_rmdir(sdir, FALSE); // set to FALSE in case the user placed extra files there
  lives_free(sdir);

  if (prefs->ar_clipset && !strcmp(prefs->ar_clipset_name, set_name)) {
    prefs->ar_clipset = FALSE;
    lives_memset(prefs->ar_clipset_name, 0, 1);
    set_string_pref(PREF_AR_CLIPSET, "");
  }
  mainw->set_list = lives_list_delete_string(mainw->set_list, set_name);
}


LIVES_GLOBAL_INLINE void do_set_noclips_error(const char *setname) {
  char *msg = lives_strdup_printf(
                _("No clips were recovered for set (%s).\n"
                  "Please check the spelling of the set name and try again.\n"),
                setname);
  d_print(msg);
  lives_free(msg);
}


LIVES_GLOBAL_INLINE void do_set_fail_load_error(const char *setname) {
  do_error_dialogf(_("Set %s\ncannot be opened, it may have been moved from its original location.\n"), setname);
}


LIVES_GLOBAL_INLINE boolean do_set_noclips_query(const char *set_name) {
  return do_yesno_dialogf_with_countdown(3, TRUE, _("LiVES was unable to find any clips in the set %s\n"
                                         "Would you like to delete this set ?\nIt is no longer usable"),
                                         set_name);
}


LIVES_GLOBAL_INLINE boolean do_set_locked_warning(const char *setname) {
  return do_yesno_dialogf(
           _("\nWarning - the set %s\nis in use by another copy of LiVES.\n"
             "You are strongly advised to close the other copy before clicking %s to continue\n.\n"
             "Click %s to cancel loading the set.\n"),
           setname, STOCK_LABEL_TEXT(YES), STOCK_LABEL_TEXT(NO));
}


LIVES_GLOBAL_INLINE boolean prompt_remove_layout_files(void) {
  return (do_yesno_dialogf_with_countdown(2, TRUE, "%s",
                                          _("\nDo you wish to remove the layout files associated with this set ?\n"
                                              "(They will not be usable without the set).\n")));
}


LIVES_GLOBAL_INLINE boolean do_reload_set_query(void) {
  return do_yesno_dialogf_with_countdown(2, TRUE, "%s",
                                         _("Current clips will be added to the clip set.\nIs that what you want ?\n"));
}

boolean do_set_duplicate_warning(const char *new_set) {
  char *msg = lives_strdup_printf(
                _("\nA set entitled %s already exists.\n"
                  "Click %s to add the current clips and layouts to the existing set.\n"
                  "Click %s to pick a new name.\n"), new_set,
                STOCK_LABEL_TEXT(OK), STOCK_LABEL_TEXT(CANCEL));
  boolean retcode = do_warning_dialog_with_check(msg, WARN_MASK_DUPLICATE_SET);
  lives_free(msg);
  return retcode;
}


void check_remove_layout_files(void) {
  if (prompt_remove_layout_files()) {
    // delete layout directory
    char *laydir = LAYOUTS_DIR(mainw->set_name);
    lives_rmdir(laydir, TRUE);
    lives_free(laydir);
    d_print(_("Layouts were removed for set %s.\n"), mainw->set_name);
  }
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
#define BW DEF_BUTTON_HEIGHT
#define BH BW

#define DEFNAME (mainw->string_constants[LIVES_STRING_CONSTANT_DEFAULT])

static lives_clipgrp_t *current_grp = NULL;
static LiVESTreeStore *xxtstore = NULL;
static LiVESTreeIter xxiter;

static boolean is_legal_clpgrp(const char *grpname) {
  char c = *grpname;
  for (int i = 0; c; c = grpname[++i]) {
    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '1' || c > '0')
        && c != '_') return FALSE;
  }
  return TRUE;
}


static lives_clipgrp_t *clpgrp_new(const char *name) {
  lives_clipgrp_t *clpgrp = (lives_clipgrp_t *)lives_calloc(1, sizeof(lives_clipgrp_t));
  clpgrp->name = lives_strdup(name);
  return clpgrp;
}


static boolean is_dupe_name(const char *name, LiVESList * list) {
  if (!lives_strcmp(name, DEFNAME)) return TRUE;
  for (; list; list = list->next) {
    lives_clipgrp_t *clpgrp = (lives_clipgrp_t *)list->data;
    if (!lives_strcmp(clpgrp->name, name)) return TRUE;
  }
  return FALSE;
}


static void chk_cgroup_name(LiVESWidget * e, LiVESWidget * okbutton) {
  const char *clipgrp_name = lives_entry_get_text(LIVES_ENTRY(e));
  boolean legal, duped, sens = FALSE;
  if (!*clipgrp_name || ((legal = is_legal_clpgrp(clipgrp_name))
                         && !(duped = is_dupe_name(clipgrp_name, mainw->clip_grps)))) {
    hide_warn_image(e);
    sens = TRUE;
  } else {
    if (!legal)
      show_warn_image(e, _("Clip group names must be composed only of '_' and alphanumeric characters."));
    else
      show_warn_image(e, _("Clip group name must be unique for the set"));
  }
  lives_widget_set_sensitive(okbutton, sens);
}


static void add_clipgrp(LiVESWidget * w, LiVESCombo * combo) {
  LiVESWidget *dialog_vbox, *hbox, *entry, *layout;
  LiVESWidget *okbutton, *cancelbutton;
  LiVESResponseType resp;
  LiVESWidget *dialog = lives_standard_dialog_new(_("Create a New Clip Group"),
                        FALSE, RFX_WINSIZE_H, RFX_WINSIZE_V);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Enter the name of the new group, and you can then add clips to it. "
                         "From the Clips menu, you can select which clip groups are visible.\n"
                         "The remaining clips in the set will be temporarily hidden."), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  entry = lives_standard_entry_new(_("Enter the name of the new group to create"), NULL, -1, 16, LIVES_BOX(hbox), NULL);

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(entry), "okbutton", (livespointer)okbutton);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(chk_cgroup_name), okbutton);

  chk_cgroup_name(entry, okbutton);

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  if (resp == LIVES_RESPONSE_OK) {
    const char *grpnamex = lives_entry_get_text(LIVES_ENTRY(entry));
    if (*grpnamex) {
      char *grpname = lives_strndup(grpnamex, 16);
      lives_clipgrp_t *clpgrp = clpgrp_new(grpname);
      int nentries;
      lives_combo_append_text(combo, grpname);
      nentries = lives_combo_get_n_entries(combo);
      mainw->clip_grps = lives_list_append(mainw->clip_grps, clpgrp);
      lives_combo_set_active_index(combo, nentries - 1);
      lives_free(grpname);
    }
  }
  lives_widget_destroy(dialog);
}


static void del_clipgrp(LiVESWidget * w, LiVESCombo * combo) {
  const char *grpname = lives_combo_get_active_text(combo);
  if (!do_yesno_dialogf(_("\nAre you sure you wish to delete the clip group '%s' ?\n"
                          "(This will not remove the clips themselves)"), grpname))
    return;
  for (LiVESList *list = mainw->clip_grps; list; list = list->next) {
    lives_clipgrp_t *clpgrp = (lives_clipgrp_t *)list->data;
    if (!lives_strcmp(clpgrp->name, grpname)) {
      lives_list_free_all(&clpgrp->list);
      lives_free(clpgrp->name);
      if (clpgrp->menuitem) lives_widget_destroy(clpgrp->menuitem);
      lives_free(clpgrp);
      mainw->clip_grps = lives_list_remove_node(mainw->clip_grps, list, TRUE);
      lives_combo_remove_text(combo, grpname);
      lives_combo_set_active_index(combo, 0);
      return;
    }
  }
}


static void grp_combo_populate(LiVESCombo * combo, LiVESTreeView * tview) {
  LiVESList *list;
  lives_clip_t *sfile;
  LiVESWidget *del_button = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "del_button");
  LiVESWidget *label = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "label");
  LiVESWidget *tree = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo), "tree");
  char *clipname;
  const char *grpname = lives_combo_get_active_text(combo);
  boolean sens = TRUE;
  int clipno;

  xxtstore = lives_tree_store_new(1, LIVES_COL_TYPE_STRING);
  lives_tree_store_append(xxtstore, &xxiter, NULL);

  if (!lives_strcmp(grpname, DEFNAME)) {
    // "default" group
    list = mainw->cliplist;
    for (; list; list = list->next) {
      clipno = LIVES_POINTER_TO_INT(list->data);
      if (!IS_VALID_CLIP(clipno)) continue;
      sfile = mainw->files[clipno];
      clipname = get_menu_name(sfile, FALSE);
      lives_tree_store_set(xxtstore, &xxiter, 0, clipname, -1);
      lives_tree_store_append(xxtstore, &xxiter, NULL);
    }
    sens = FALSE;
    current_grp = NULL;
  } else {
    for (list = mainw->clip_grps; list; list = list->next) {
      lives_clipgrp_t *clipgrp = (lives_clipgrp_t *)list->data;
      if (!lives_strcmp(clipgrp->name, grpname)) {
        list = clipgrp->list;
        current_grp = clipgrp;
        break;
      }
    }
    for (; list; list = list->next) {
      uint64_t uid = lives_strtoul((const char *)list->data);
      clipno = find_clip_by_uid(uid);
      if (IS_VALID_CLIP(clipno)) {
        sfile = mainw->files[clipno];
        clipname = get_menu_name(sfile, FALSE);
        lives_tree_store_set(xxtstore, &xxiter, 0, clipname, -1);
        lives_tree_store_append(xxtstore, &xxiter, NULL);
      }
    }
  }
  lives_tree_view_set_model(tview, LIVES_TREE_MODEL(xxtstore));
  lives_widget_set_sensitive(del_button, sens);
  lives_widget_set_sensitive(tree, sens);
  lives_widget_set_sensitive(label, sens);
}


static int drag_clipno = -1;

static boolean drag_start(LiVESWidget * widget, LiVESXEventButton * event, LiVESCellRenderer * rend) {
  LiVESWidget *topl = lives_widget_get_toplevel(widget);
  LiVESXDisplay *disp = lives_widget_get_display(topl);
  LiVESPixbuf *pixbuf;
  LiVESXCursor *cursor;
  lives_colRGBA64_t *col;
  LiVESTreePath *tpath;
  lives_clip_t *sfile;
  //lives_colRGBA64_t col_white = lives_rgba_col_new(65535, 65535, 65535, 65535);
  lives_colRGBA64_t col_black = lives_rgba_col_new(0, 0, 0, 65535);
  weed_layer_t *layer;
  uint32_t cwidth, cheight;
  uint8_t *cpixels;
  char *clipname;
  int trow;

  gtk_tree_view_get_path_at_pos(LIVES_TREE_VIEW(widget), event->x, event->y, &tpath, NULL, NULL, NULL);
  if (tpath) {
    void *listnode;
    drag_clipno = atoi(gtk_tree_path_to_string(tpath));
    listnode = lives_list_nth_data(mainw->cliplist, drag_clipno);
    if (!listnode) {
      drag_clipno = 1;
      return TRUE;
    }
    drag_clipno = LIVES_POINTER_TO_INT(listnode);
  }
  if (!IS_VALID_CLIP(drag_clipno)) {
    drag_clipno = -1;
    return TRUE;
  }

#ifdef GUI_GTK
  // see multitrack.c
  gdk_display_get_maximal_cursor_size(disp, &cwidth, &cheight);
#endif
  cheight = widget_opts.css_min_height << 1;
  pixbuf = lives_pixbuf_new(TRUE, cwidth, cheight);

  trow = lives_pixbuf_get_rowstride(pixbuf);
  cpixels = lives_pixbuf_get_pixels(pixbuf);

  col = &palette->fxcol;

  if (prefs->extra_colours && mainw->pretty_colours) {
    widget_color_to_lives_rgba(col, &palette->nice3);
  }

  for (int j = 0; j < cheight; j++) {
    for (int k = 0; k < cwidth; k++) {
      cpixels[0] = col->red >> 8;
      cpixels[1] = col->green >> 8;
      cpixels[2] = col->blue >> 8;
      cpixels[3] = 255;
      cpixels += 4;
    }
    cpixels += (trow - cwidth * 4);
  }

  layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  if (!pixbuf_to_layer(layer, pixbuf)) {
    lives_widget_object_unref(pixbuf);
  }

  sfile = mainw->files[drag_clipno];
  clipname = get_menu_name(sfile, FALSE);
  layer = render_text_to_layer(layer, clipname, capable->font_fam, -12.,
                               LIVES_TEXT_MODE_FOREGROUND_ONLY,
                               &col_black, &col_black, TRUE, FALSE, 0.1);

  pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
  weed_layer_free(layer);

  cursor = lives_cursor_new_from_pixbuf(disp, pixbuf, 0, cheight >> 1);
  lives_xwindow_set_cursor(lives_widget_get_xwindow(topl), cursor);

  return TRUE;
}

// need clipno, tree2
static boolean drag_end(LiVESWidget * widget, LiVESXEventButton * event, LiVESWidget * tree2) {
  LiVESWidget *topl = lives_widget_get_toplevel(widget);
  lives_xwindow_set_cursor(lives_widget_get_xwindow(topl), NULL);

  if (IS_VALID_CLIP(drag_clipno)) {
    LiVESXWindow *window;
    LiVESXDisplay *disp = lives_widget_get_display(topl);
    const char *clipname;
    int win_x, win_y;
    window = lives_display_get_window_at_pointer
             ((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
              disp, &win_x, &win_y);
    if (window == gtk_tree_view_get_bin_window(LIVES_TREE_VIEW(tree2))) {
      // need clipno of clip selected,
      lives_clip_t *sfile = mainw->files[drag_clipno];
      char *uidstr = lives_strdup_printf("%lu", sfile->unique_id);
      // TODO - no dupes !
      if (!current_grp->list) {
        current_grp->menuitem = lives_standard_check_menu_item_new_with_label(current_grp->name, TRUE);
        lives_container_add(LIVES_CONTAINER(mainw->cg_submenu), current_grp->menuitem);
        lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(current_grp->menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                          LIVES_GUI_CALLBACK(filter_clips), current_grp);
        lives_widget_show(current_grp->menuitem);
      }
      current_grp->list = lives_list_append(current_grp->list, uidstr);
      clipname = get_menu_name(sfile, FALSE);

      //g_print("ADD clip %s\n", clipname);

      lives_tree_store_set(xxtstore, &xxiter, 0, clipname, -1);
      lives_tree_store_append(xxtstore, &xxiter, NULL);
    }
  }
  drag_clipno = -1;
  return TRUE;
}


void manage_clipgroups(LiVESWidget * w, livespointer data) {
  lives_clip_t *sfile;
  LiVESCellRenderer *renderer;
  LiVESTreeStore *treestore;
  LiVESTreeIter iter;
  LiVESList *grpslist = NULL;
  LiVESTreeViewColumn *column;

  LiVESWidget *img_tips, *label;
  LiVESWidget *tree, *tree2, *combo;
  LiVESWidget *add_button, *del_button;
  LiVESWidget *layout, *okbutton;
  LiVESWidget *scrolledwindow;
  LiVESWidget *dialog_vbox, *hbox, *hbox1, *vbox;
  LiVESWidget *dialog = lives_standard_dialog_new(_("Create and Manage Clip Groups"),
                        FALSE, RFX_WINSIZE_H << 1, RFX_WINSIZE_V);
  char *clipname, *msg, *xdefname;

  xdefname = lives_strdup_printf("~%s", DEFNAME);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  msg = lives_strdup_printf(_("Clip groups can be used to filter the number of clips which are accessible "
                              "via the Clips menu at any one time. "
                              "This can be used to create smaller subsets within the larger set of available clips.\n"
                              "The *%s* group contains every clip in the set. Click the + button to add a new group.\n"), DEFNAME);

  lives_layout_add_label(LIVES_LAYOUT(layout), msg, FALSE);
  lives_free(msg);
  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  hbox1 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox1, FALSE, TRUE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), vbox, FALSE, TRUE, 0);

  layout = lives_layout_new(LIVES_BOX(vbox));
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  label = lives_layout_add_label(LIVES_LAYOUT(layout), _("Drag clips from here to the group"), TRUE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  renderer = lives_cell_renderer_text_new();
  column = lives_tree_view_column_new_with_attributes(NULL,
           renderer, LIVES_TREE_VIEW_COLUMN_TEXT, 0, NULL);

  lives_tree_view_column_set_sizing(column, LIVES_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_expand(column, TRUE);

  treestore = lives_tree_store_new(1, LIVES_COL_TYPE_STRING);
  lives_tree_store_append(treestore, &iter, NULL);
  lives_tree_store_set(treestore, &iter, 0, _("Clips in Set"), -1);

  tree = lives_tree_view_new_with_model(LIVES_TREE_MODEL(treestore));
  lives_tree_view_append_column(LIVES_TREE_VIEW(tree), column);
  lives_tree_view_set_headers_visible(LIVES_TREE_VIEW(tree), FALSE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(tree), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(drag_start), renderer);

  for (LiVESList *list = mainw->cliplist; list; list = list->next) {
    int clipno = LIVES_POINTER_TO_INT(list->data);
    if (!IS_VALID_CLIP(clipno)) continue;
    sfile = mainw->files[clipno];
    clipname = get_menu_name(sfile, FALSE);
    lives_tree_store_set(treestore, &iter, 0, clipname, -1);
    lives_tree_store_append(treestore, &iter, NULL);
  }

  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, tree);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_start(LIVES_BOX(hbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), vbox, FALSE, TRUE, 0);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  grpslist = lives_list_prepend(grpslist, (char *)DEFNAME);
  for (LiVESList *list = mainw->clip_grps; list; list = list->next) {
    char *grpname = ((lives_clipgrp_t *)list->data)->name;
    if (!lives_strcmp(grpname, DEFNAME)) grpname = xdefname;
    grpslist = lives_list_append(grpslist, grpname);
  }

  combo = lives_standard_combo_new(_("Group name:"), grpslist, LIVES_BOX(hbox), NULL);
  lives_list_free(grpslist);

  ////
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  add_button = lives_standard_button_new_from_stock(LIVES_STOCK_ADD, NULL, BW, BH);
  lives_layout_pack(LIVES_BOX(hbox), add_button);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(add_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(add_clipgrp), combo);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  del_button = lives_standard_button_new_from_stock(LIVES_STOCK_REMOVE, NULL, BW, BH);
  lives_layout_pack(LIVES_BOX(hbox), del_button);
  lives_widget_set_sensitive(del_button, FALSE);

  msg = lives_strdup_printf(H_("Use the '+' button to add a new group, or the '-' button to delete a group.\n"
                               "(The %s group can never be deleted)"), DEFNAME);
  img_tips = lives_widget_set_tooltip_text(del_button, H_(msg));
  lives_free(msg);

  lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, widget_opts.packing_width >> 1);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), "del_button", (livespointer)del_button);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), "tree", (livespointer)tree);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo), "label", (livespointer)label);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(del_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(del_clipgrp), combo);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  renderer = lives_cell_renderer_text_new();
  column = lives_tree_view_column_new_with_attributes(NULL,
           renderer, LIVES_TREE_VIEW_COLUMN_TEXT, 0, NULL);

  lives_tree_view_column_set_sizing(column, LIVES_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_expand(column, TRUE);

  treestore = lives_tree_store_new(1, LIVES_COL_TYPE_STRING);
  lives_tree_store_append(treestore, &iter, NULL);
  lives_tree_store_set(treestore, &iter, 0, _("Clips in Group %"), -1);

  tree2 = lives_tree_view_new_with_model(LIVES_TREE_MODEL(treestore));
  lives_tree_view_append_column(LIVES_TREE_VIEW(tree2), column);
  lives_tree_view_set_headers_visible(LIVES_TREE_VIEW(tree2), FALSE);

  grp_combo_populate(LIVES_COMBO(combo), LIVES_TREE_VIEW(tree2));

  lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(grp_combo_populate), tree2);

  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, tree2);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_start(LIVES_BOX(hbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_height);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(tree), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                            LIVES_GUI_CALLBACK(drag_end), tree2);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_dialog_run(LIVES_DIALOG(dialog));
  lives_widget_destroy(dialog);
  lives_free(xdefname);
}
