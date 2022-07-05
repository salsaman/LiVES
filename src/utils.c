// utils.c
// LiVES
// (c) G. Finch 2003 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <fcntl.h>
#include <dirent.h>
#include <sys/statvfs.h>
#ifdef HAVE_LIBEXPLAIN
#include <libexplain/system.h>
#endif
#include "main.h"
#include "interface.h"
#include "audio.h"
#include "resample.h"
#include "callbacks.h"
#include "cvirtual.h"


#ifdef ENABLE_OSC
boolean lives_osc_notify_failure(void) WARN_UNUSED;
#endif

#define ASPECT_ALLOWANCE 0.005


static boolean omute, osepwin, ofs, ofaded, odouble;


LIVES_GLOBAL_INLINE boolean lives_setenv(const char *name, const char *value) {
  // ret TRUE on success
#if IS_IRIX
  char *env = lives_strdup_printf("%s=%s", name, val);
  boolean ret = !putenv(env);
  lives_free(env);
  return ret;
#else
  return !setenv(name, value, 1);
#endif
}

LIVES_GLOBAL_INLINE boolean lives_unsetenv(const char *name) {
  // ret TRUE on success
#if IS_IRIX
  char *env = lives_strdup_printf("%s=", name);
  boolean ret = !putenv(env);
  lives_free(env);
  return ret;
#else
  return !unsetenv(name);
#endif
}


LIVES_GLOBAL_INLINE void lives_abort(const char *reason) {
  char *xreason = (char *)reason;
  if (!xreason) xreason = _("Aborting");
  lives_set_status(LIVES_STATUS_FATAL);
  break_me(xreason);
  if (mainw) lives_hooks_trigger(NULL, mainw->global_hook_closures, ABORT_HOOK);
  g_printerr("LIVES FATAL: %s\n", xreason);
  lives_notify(LIVES_OSC_NOTIFY_QUIT, xreason);
  if (xreason != reason) lives_free(xreason);
  abort();
}


void restart_me(LiVESList *extra_argv, const char *xreason) {
  int argc = orig_argc();
  char *new_argv[256], **argv = orig_argv();
  char more_argv[256][256];
  int i;
  for (i = 0; i < argc; i++) {
    new_argv[i] = argv[i];
  }
  if (extra_argv && argc < 255) {
    LiVESList *list = extra_argv;
    argv = new_argv;
    for (int j = 0; list; j++) {
      lives_snprintf(more_argv[j], 256, "%s", (char *)list->data);
      new_argv[i] = more_argv[j];
      if (++i == 255) break;
      list = list->next;
    }
    new_argv[i] = NULL;
  }
  if (mainw) lives_hooks_trigger(NULL, mainw->global_hook_closures, RESTART_HOOK);
  lives_notify(LIVES_OSC_NOTIFY_QUIT, xreason ? xreason : "");
  execve(orig_argv()[0], argv, environ);
#ifdef ENABLE_OSC
  IGN_RET(lives_osc_notify_failure());
#endif
  fprintf(stderr, "FAILED TO RESTART LiVES, aborting instead !");
  if (mainw) {
    mainw->error = TRUE;
    lives_hooks_trigger(NULL, mainw->global_hook_closures, ABORT_HOOK);
  }
  abort();
}


int lives_system(const char *com, boolean allow_error) {
  LiVESResponseType response;
  static boolean shortcut = FALSE;
  boolean cnorm = FALSE;
  int retval;

  //g_print("doing: %s\n",com);

  // otherwise we get infinite recursion removing cfile->info_file with lives_rm
  if (shortcut) return system(com);

  if (mainw && mainw->is_ready && !mainw->is_exiting &&
      ((!mainw->multitrack && mainw->cursor_style == LIVES_CURSOR_NORMAL) ||
       (mainw->multitrack && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL))) {
    cnorm = TRUE;
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  }

  if (CURRENT_CLIP_IS_VALID) {
    shortcut = TRUE;
    lives_rm(cfile->info_file);
    shortcut = FALSE;
  }
  mainw->error = FALSE;
  mainw->cancelled = CANCEL_NONE;

  do {
    THREADVAR(com_failed) = FALSE;
    response = LIVES_RESPONSE_NONE;
    retval = system(com);
    if (retval) {
      char *msg = NULL;
      THREADVAR(com_failed) = TRUE;
      if (!allow_error) {
        msg = lives_strdup_printf("lives_system failed with code %d: %s\n%s", retval, com,
#ifdef HAVE_LIBEXPLAIN
                                  explain_system(com)
#else
                                  ""
#endif
                                 );
        LIVES_ERROR(msg);
        response = do_system_failed_error(com, retval, NULL, TRUE, FALSE);
      }
#ifndef LIVES_NO_DEBUG
      else {
        msg = lives_strdup_printf("lives_system failed with code %d: %s (not an error)", retval, com);
        LIVES_DEBUG(msg);
      }
#endif
      if (msg) lives_free(msg);
    }
  } while (response == LIVES_RESPONSE_RETRY);

  if (cnorm) lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  return retval;
}


ssize_t lives_popen(const char *com, boolean allow_error, char *buff, ssize_t buflen) {
  // runs com, fills buff with a NUL terminated string (total length <= buflen)
  // returns number of bytes read. If an error occurs during popen or fread
  // then THREADVAR(com_failed) is set, and if allow_error is FALSE then an an error dialog is displayed to the user

  // on error we return err as a -ve number

  // id buflen is 0, then buff os cast from a textbuff, and the output will be appended to it

  FILE *fp;
  char *xbuff;
  LiVESResponseType response;
  ssize_t totlen = 0, xtotlen = 0;
  size_t slen;
  LiVESTextBuffer *tbuff = NULL;
  LiVESTextIter end_iter;
  boolean cnorm = FALSE;
  int err = 0;

  if (buflen <= 0) {
    tbuff = (LiVESTextBuffer *)buff;
    buflen = get_read_buff_size(BUFF_SIZE_READ_LARGE);
    xbuff = (char *)lives_calloc(buflen, 1);
  } else {
    xbuff = buff;
    lives_memset(xbuff, 0, 1);
  }
  //g_print("doing: %s\n",com);

  if (is_fg_thread()) {
    if (mainw && mainw->is_ready && !mainw->is_exiting &&
        ((!mainw->multitrack && mainw->cursor_style == LIVES_CURSOR_NORMAL) ||
         (mainw->multitrack && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL))) {
      cnorm = TRUE;
      lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    }
  }

  do {
    char *strg = NULL;
    response = LIVES_RESPONSE_NONE;
    THREADVAR(com_failed) = FALSE;
    fflush(NULL);
    fp = popen(com, "r");
    if (!fp) {
      err = errno;
    } else {
      while (1) {
        strg = fgets(xbuff + totlen, tbuff ? buflen : buflen - totlen, fp);
        err = ferror(fp);
        if (err || !strg || !*strg) break;
        slen = lives_strlen(xbuff);
        if (tbuff) {
          lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(tbuff), &end_iter);
          lives_text_buffer_insert(LIVES_TEXT_BUFFER(tbuff), &end_iter, xbuff, slen);
          xtotlen += slen;
        } else {
          //lives_snprintf(buff + totlen, buflen - totlen, "%s", xbuff);
          totlen = slen;
          if (totlen >= buflen - 1) break;
        }
      }
      pclose(fp);
    }

    if (tbuff) {
      lives_free(xbuff);
      totlen = xtotlen;
    }

    if (err) {
      char *msg = NULL;
      THREADVAR(com_failed) = TRUE;
      if (!allow_error) {
        msg = lives_strdup_printf("lives_popen failed p after %ld bytes with code %d: %s",
                                  !strg ? 0 : lives_strlen(strg), err, com);
        LIVES_ERROR(msg);
        response = do_system_failed_error(com, err, NULL, TRUE, FALSE);
      }
#ifndef LIVES_NO_DEBUG
      else {
        msg = lives_strdup_printf("lives_popen failed with code %d: %s (not an error)", err, com);
        LIVES_DEBUG(msg);
      }
#endif
      if (msg) lives_free(msg);
    }
  } while (response == LIVES_RESPONSE_RETRY);

  if (cnorm) lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  if (err != 0) return -ABS(err);
  return totlen;
}


lives_pid_t lives_fork(const char *com) {
  // returns a number which is the pid to use for lives_killpg

  // mingw - return PROCESS_INFORMATION * to use in GenerateConsoleCtrlEvent (?)

  // to signal to sub process and all children
  // TODO *** - error check

  pid_t ret;

  if (!(ret = fork())) {
    // this runs in the forked process
    setsid(); // create new session id, otherwise we inherit the parent's - THIS ALSO SETS US AS PGID leader of a new pgid
    // thus by calling killpg(ret, SIGTERM) for example, we ensure that any child processes are also terminated
    // anything which needs to survive after can be placed in a separate pgid or in a new session id
    // if we had simply used setpgid instead then that would not be possible
    // NOTE: this does not work for commands which run from the shell, as the shell itself will run the command in a
    // new sid / pgid, and even killing the parent process just orphans the child and sets the parent to init.
    IGN_RET(system(com));
    _exit(0);
  }

  // original (parent) process returns with child pid (which is now equal to child pgig)
  return ret;
}


LIVES_GLOBAL_INLINE void *lives_fork_cb(lives_object_t *dummy, void *com) {
  IGN_RET(lives_fork((const char *)com));
  return NULL;
}


int lives_chdir(const char *path, boolean no_error_dlg) {
  /// returns 0 on success
  /// on failure pops up an error dialog, unless no_error_dlg is TRUE
  int retval = chdir(path);

  if (retval) {
    char *msg = lives_strdup_printf("Chdir failed to: %s", path);
    THREADVAR(chdir_failed) = TRUE;
    if (!no_error_dlg) {
      LIVES_ERROR(msg);
      do_chdir_failed_error(path);
    } else LIVES_DEBUG(msg);
    lives_free(msg);
  }
  return retval;
}


int lives_rmdir(const char *dir, boolean force) {
  // if force is TRUE, removes non-empty dirs, otherwise leaves them
  // may fail
  if (!dir) return 1;
  else {
    size_t dirlen = lives_strlen(dir);
    // will abort if dir length < 7, if dir is $HOME, does not start with dir sep. or ends with a '*'
    if (dirlen < 7 || !lives_strcmp(dir, capable->home_dir)
        || lives_strncmp(dir, LIVES_DIR_SEP, lives_strlen(LIVES_DIR_SEP)) || dir[dirlen - 1] == '*') {
      char *msg = lives_strdup_printf("Refusing to lives_rmdir the following directory: %s", dir);
      LIVES_FATAL(msg);
      lives_free(msg);
    }
    if (lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
      char *com, *cmd;
      int retval;

      if (force) {
        cmd = lives_strdup_printf("%s -rf", capable->rm_cmd);
      } else {
        cmd = lives_strdup(capable->rmdir_cmd);
      }

      com = lives_strdup_printf("%s \"%s/\" >\"%s\" 2>&1", cmd, dir, prefs->cmd_log);
      retval = lives_system(com, TRUE);
      lives_free(com);
      lives_free(cmd);
      return retval;
    }
  }
  return 1;
}


int lives_rmdir_with_parents(const char *dir) {
  // may fail, will not remove non empty dirs
  if (!dir) return 1;
  else {
    size_t dirlen = lives_strlen(dir);
    // will abort if dir length < 7, if dir is $HOME, does not start with dir sep. or ends with a '*'
    if (dirlen < 7 || !lives_strcmp(dir, capable->home_dir)
        || lives_strncmp(dir, LIVES_DIR_SEP, lives_strlen(LIVES_DIR_SEP)) || dir[dirlen - 1] == '*') {
      char *msg = lives_strdup_printf("Refusing to lives_rmdir_with_parents the following directory: %s", dir);
      LIVES_FATAL(msg);
      lives_free(msg);
    }
  }
  if (lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
    char *com = lives_strdup_printf("%s -p \"%s\" >\"%s\" 2>&1", capable->rmdir_cmd, dir, prefs->cmd_log);
    int retval = lives_system(com, TRUE);
    lives_free(com);
    return retval;
  }
  return 1;
}



int lives_rm(const char *file) {
  // may fail - will not remove directories or symlinks
  if (!file) return 1;
  if (lives_file_test(file, LIVES_FILE_TEST_IS_REGULAR)) {
    char *com = lives_strdup_printf("%s -f \"%s\" >\"%s\" 2>&1", capable->rm_cmd, file, prefs->cmd_log);
    int retval = lives_system(com, TRUE);
    lives_free(com);
    return retval;
  }
  return 1;
}


int lives_rmglob(const char *files) {
  // delete files with name "files"*
  // may fail
  // should chdir first to target dir
  if (!files) return 1;
  else {
    // files may not be empty string, or start with a '.' or dir separator
    if (!*files || *files == '.' || !lives_strncmp(files, LIVES_DIR_SEP, lives_strlen(LIVES_DIR_SEP))) return 2;
    else {
      int retval;
      char *com;
      if (!lives_strcmp(files, "*"))
        com = lives_strdup_printf("%s * >\"%s\" 2>&1", capable->rm_cmd, prefs->cmd_log);
      else
        com = lives_strdup_printf("%s \"%s\"* >\"%s\" 2>&1", capable->rm_cmd, files, prefs->cmd_log);
      retval = lives_system(com, TRUE);
      lives_free(com);
      return retval;
    }
  }
}


int lives_cp(const char *from, const char *to) {
  // may not fail - BUT seems to return -1 sometimes
  char *com = lives_strdup_printf("%s \"%s\" \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cp_noclobber(const char *from, const char *to) {
  char *com = lives_strdup_printf("%s -n \"%s\"/* \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_cp_recursive(const char *from, const char *to, boolean incl_dir) {
  // we do allow fail since the source may be empty dir
  int retval;
  char *com;
  boolean mayfail = FALSE;
  if (incl_dir) com = lives_strdup_printf("%s -r \"%s\" \"%s\" >\"%s\" 2>&1",
                                            capable->cp_cmd, from, to, prefs->cmd_log);
  else {
    // we do allow fail since the source dir may be empty dir
    com = lives_strdup_printf("%s -rf \"%s\"/* \"%s\" >\"%s\" 2>&1",
                              capable->cp_cmd, from, to, prefs->cmd_log);
    mayfail = TRUE;
  }
  if (!lives_file_test(to, LIVES_FILE_TEST_EXISTS))
    lives_mkdir_with_parents(to, capable->umask);

  retval = lives_system(com, mayfail);

  lives_free(com);
  return retval;
}


int lives_cp_keep_perms(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s -a \"%s\" \"%s/\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_mv(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" \"%s\"", capable->mv_cmd, from, to);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_touch(const char *tfile) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" >\"%s\" 2>&1", capable->touch_cmd, tfile, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_ln(const char *from, const char *to) {
  // may not fail
  char *com;
  int retval;
  com = lives_strdup_printf("%s -s \"%s\" \"%s\" >\"%s\" 2>&1", capable->ln_cmd, from, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_chmod(const char *target, const char *mode) {
  // may not fail
  char *com = lives_strdup_printf("%s %s \"%s\" >\"%s\" 2>&1", capable->chmod_cmd, mode, target, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cat(const char *from, const char *to, boolean append) {
  // may not fail
  char *com;
  char *op;
  int retval;

  if (append) op = ">>";
  else op = ">";

  com = lives_strdup_printf("%s \"%s\" %s \"%s\" >\"%s\" 2>&1", capable->cat_cmd, from, op, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_echo(const char *text, const char *to, boolean append) {
  // may not fail
  char *com;
  char *op;
  int retval;

  if (append) op = ">>";
  else op = ">";

  com = lives_strdup_printf("%s \"%s\" %s \"%s\" 2>\"%s\"", capable->echo_cmd, text, op, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


LIVES_GLOBAL_INLINE pid_t lives_getpid(void) {
#ifdef IS_MINGW
  return GetCurrentProcessId(),
#else
  return getpid();
#endif
}

LIVES_GLOBAL_INLINE int lives_getuid(void) {return geteuid();}

LIVES_GLOBAL_INLINE int lives_getgid(void) {return getegid();}


LIVES_GLOBAL_INLINE int lives_kill(lives_pid_t pid, int sig) {
  if (pid == 0) {
    LIVES_ERROR("Tried to kill pid 0");
    return -1;
  }
  return kill(pid, sig);
}


LIVES_GLOBAL_INLINE int lives_killpg(lives_pid_t pid, int sig) {return killpg(getpgid(pid), sig);}

void lives_kill_subprocesses(const char *dirname, boolean kill_parent) {
  char *com;
  if (kill_parent)
    com = lives_strdup_printf("%s stopsubsub \"%s\"", prefs->backend_sync, dirname);
  else
    com = lives_strdup_printf("%s stopsubsubs \"%s\"", prefs->backend_sync, dirname);
  lives_system(com, TRUE);
  lives_free(com);
}


void lives_suspend_resume_process(const char *dirname, boolean suspend) {
  char *com;
  if (!suspend)
    com = lives_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null", prefs->backend_sync, dirname);
  else
    com = lives_strdup_printf("%s stopsubsub \"%s\" SIGTSTP 2>/dev/null", prefs->backend_sync, dirname);
  lives_system(com, TRUE);
  lives_free(com);

  com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, dirname);
  lives_system(com, FALSE);
  lives_free(com);
}

///////////////////////////////////////////////////////////

LIVES_GLOBAL_INLINE double calc_time_from_frame(int clip, frames_t frame) {return ((double)frame - 1.) / mainw->files[clip]->fps;}


LIVES_GLOBAL_INLINE frames_t calc_frame_from_time(int filenum, double time) {
  // return the nearest frame (rounded) for a given time, max is cfile->frames
  frames_t frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (frames_t)(time * mainw->files[filenum]->fps + 1.49999);
  return (frame < mainw->files[filenum]->frames) ? frame : mainw->files[filenum]->frames;
}


LIVES_GLOBAL_INLINE frames_t calc_frame_from_time2(int filenum, double time) {
  // return the nearest frame (rounded) for a given time
  // allow max (frames+1)
  frames_t frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (frames_t)(time * mainw->files[filenum]->fps + 1.49999);
  return (frame < mainw->files[filenum]->frames + 1) ? frame : mainw->files[filenum]->frames + 1;
}


LIVES_GLOBAL_INLINE frames_t calc_frame_from_time3(int filenum, double time) {
  // return the nearest frame (floor) for a given time
  // allow max (frames+1)
  frames_t frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (frames_t)(time * mainw->files[filenum]->fps + 1.);
  return (frame < mainw->files[filenum]->frames + 1) ? frame : mainw->files[filenum]->frames + 1;
}


LIVES_GLOBAL_INLINE frames_t calc_frame_from_time4(int filenum, double time) {
  // return the nearest frame (rounded) for a given time, no maximum
  frames_t frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (frames_t)(time * mainw->files[filenum]->fps + 1.49999);
  return frame;
}


LIVES_GLOBAL_INLINE void calc_maxspect(int rwidth, int rheight, int *cwidth, int *cheight) {
  // calculate maxspect (maximum size which maintains aspect ratio)
  // of cwidth, cheight - given restrictions rwidth * rheight

  // i.e both dimensions will expand or shrink to fit in the bounding rectangle

  double aspect;
  if (*cwidth <= 0 || *cheight <= 0 || rwidth <= 0 || rheight <= 0) return;

  aspect = (double)(*cwidth) / (double)(*cheight);

  *cwidth = rwidth;
  *cheight = (double)(*cwidth) / aspect;
  if (*cheight > rheight) {
    // image too tall shrink it
    *cheight = rheight;
    *cwidth = (double)(*cheight) * aspect;
  }
  *cwidth = (*cwidth >> 2) << 2;
  *cheight = (*cheight >> 1) << 1;
}


LIVES_GLOBAL_INLINE void calc_midspect(int rwidth, int rheight, int *cwidth, int *cheight) {
  // calculate midspect (minimum size which conforms to aspect ratio of
  // of cwidth, cheight) - which contains rwidth, rheight

  // i.e both dimensions may grow, image size will never shrink

  double aspect;

  if (*cwidth <= 0 || *cheight <= 0 || rwidth <= 0 || rheight <= 0) return;
  if (*cwidth >= rwidth && *cheight >= rheight) return;

  aspect = (double)(*cwidth) / (double)(*cheight);

  calc_maxspect(rwidth, rheight, cwidth, cheight);

  if (rwidth > *cwidth) {
    *cwidth = rwidth;
    *cheight = *cwidth / aspect;
  }
  if (rheight > *cheight) {
    *cheight = rheight;
    *cwidth = *cheight * aspect;
  }

  *cwidth = ((*cwidth + 3) >> 2) << 2;
  *cheight = ((*cheight + 1) >> 1) << 1;
}


LIVES_GLOBAL_INLINE void calc_minspect(int *rwidth, int *rheight, int cwidth, int cheight) {
  // calculate minspect (minimum size which conforms to aspect ratio of
  // of rwidth, rheight) - which contains cwidth, cheight

  // rwidth, rheight keeps aspect ratio, but we fit it inside cwidth, cheight...
  calc_maxspect(cwidth, cheight, rwidth, rheight);

  // ...then expand it so it contains cwidth, cheight...
  calc_midspect(cwidth, cheight, rwidth, rheight);

  *rwidth = (*rwidth >> 2) << 2;
  *rheight = (*rheight  >> 1) << 1;
}


/////////////////////////////////////////////////////////////////////////////

void init_clipboard(void) {
  int current_file = mainw->current_file;
  char *com;

  if (!clipboard) {
    // here is where we create the clipboard
    // use get_new_handle(clipnumber,name);
    if (!get_new_handle(CLIPBOARD_FILE, "clipboard")) {
      mainw->error = TRUE;
      return;
    }
    migrate_from_staging(CLIPBOARD_FILE);
  } else {
    // experimental feature - we can have duplicate copies of the clipboard with different palettes / gamma
    for (int i = 0; i < mainw->ncbstores; i++) {
      if (mainw->cbstores[i] != clipboard) {
        char *clipd = lives_build_path(prefs->workdir, mainw->cbstores[i]->handle, NULL);
        if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
          char *com = lives_strdup_printf("%s close \"%s\"", prefs->backend, mainw->cbstores[i]->handle);
          char *permitname = lives_build_path(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
          lives_touch(permitname);
          lives_free(permitname);
          lives_system(com, TRUE);
          lives_free(com);
        }
        lives_free(clipd);
      }
    }
    mainw->ncbstores = 0;

    if (clipboard->frames > 0) {
      // clear old clipboard
      // need to set current file to 0 before monitoring progress
      mainw->current_file = CLIPBOARD_FILE;
      cfile->cb_src = current_file;

      if (cfile->clip_type == CLIP_TYPE_FILE) {
        lives_freep((void **)&cfile->frame_index);
        if (cfile->ext_src && cfile->ext_src_type == LIVES_EXT_SRC_DECODER) {
          close_clip_decoder(CLIPBOARD_FILE);
        }
        cfile->clip_type = CLIP_TYPE_DISK;
      }

      com = lives_strdup_printf("%s clear_clipboard \"%s\"", prefs->backend, clipboard->handle);
      lives_rm(clipboard->info_file);
      lives_system(com, FALSE);
      lives_free(com);

      if (THREADVAR(com_failed)) {
        mainw->current_file = current_file;
        return;
      }

      cfile->progress_start = cfile->start;
      cfile->progress_end = cfile->end;
      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE, FALSE, _("Clearing the clipboard"));
    }
  }
  mainw->current_file = current_file;
  *clipboard->file_name = 0;
  clipboard->img_type = IMG_TYPE_BEST; // override the pref
  clipboard->cb_src = current_file;
}



void buffer_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current) {
  lmap_error *err = (lmap_error *)lives_malloc(sizeof(lmap_error));
  if (!err) return;
  err->type = lerror;
  if (name) err->name = lives_strdup(name);
  else err->name = NULL;
  err->data = user_data;
  err->clipno = clipno;
  err->frameno = frameno;
  err->atime = atime;
  err->current = affects_current;
  mainw->new_lmap_errors = lives_list_prepend(mainw->new_lmap_errors, err);
}


void unbuffer_lmap_errors(boolean add) {
  LiVESList *list = mainw->new_lmap_errors;
  while (list) {
    lmap_error *err = (lmap_error *)list->data;
    if (add) add_lmap_error(err->type, err->name, err->data, err->clipno, err->frameno, err->atime, err->current);
    else mainw->files[err->clipno]->tcache_dubious_from = 0;
    if (err->name) lives_free(err->name);
    lives_free(err);
    list = list->next;
  }
  if (mainw->new_lmap_errors) {
    lives_list_free(mainw->new_lmap_errors);
    mainw->new_lmap_errors = NULL;
  }
}


boolean add_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current) {
  // potentially add a layout map error to the layout textbuffer
  LiVESTextIter end_iter;
  LiVESList *lmap;

  char *text, *name2;
  char **array;

  double orig_fps;
  double max_time;

  int resampled_frame;

  lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter);

  if (affects_current && !user_data) {
    mainw->affected_layout_marks =
      lives_list_append(mainw->affected_layout_marks, (livespointer)lives_text_buffer_create_mark
                        (LIVES_TEXT_BUFFER(mainw->layout_textbuffer), NULL, &end_iter, TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    if (!(*name)) name2 = (_("(blank)"));
    else name2 = lives_strdup(name);
    text = lives_strdup_printf
           (_("The set name has been changed from %s to %s. Affected layouts have been updated accordingly\n"),
            name2, (char *)user_data);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(name2);
    lives_free(text);
    break;
  case LMAP_ERROR_MISSING_CLIP:
    if (prefs->warning_mask & WARN_MASK_LAYOUT_MISSING_CLIPS) return FALSE;
    text = lives_strdup_printf(_("The clip %s is missing from this set.\nIt is required by the following layouts:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
  case LMAP_ERROR_CLOSE_FILE:
    text = lives_strdup_printf(_("The clip %s has been closed.\nIt is required by the following layouts:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
    text = lives_strdup_printf(_("Frames have been shifted in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_DELETE_FRAMES:
    text = lives_strdup_printf(_("Frames have been deleted from the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_DELETE_AUDIO:
    text = lives_strdup_printf(_("Audio has been deleted from the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
    text = lives_strdup_printf(_("Audio has been shifted in clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_ALTER_AUDIO:
    text = lives_strdup_printf(_("Audio has been altered in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_ALTER_FRAMES:
    text = lives_strdup_printf(_("Frames have been altered in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  }

  if (affects_current && user_data) {
    mainw->affected_layout_marks =
      lives_list_append(mainw->affected_layout_marks, (livespointer)lives_text_buffer_create_mark
                        (LIVES_TEXT_BUFFER(mainw->layout_textbuffer), NULL, &end_iter, TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    lmap = mainw->current_layouts_map;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      text = lives_strdup_printf("%s\n", array[0]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      // we could list all affected layouts, which could potentially be a lot !
      //mainw->affected_layouts_map=lives_list_append_unique_str(mainw->affected_layouts_map,array[0]);
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_MISSING_CLIP:
  case LMAP_ERROR_CLOSE_FILE:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique_str(mainw->affected_layouts_map,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      text = lives_strdup_printf("%s\n", array[0]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique_str(mainw->affected_layouts_map, array[0]);
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
  case LMAP_ERROR_DELETE_FRAMES:
  case LMAP_ERROR_ALTER_FRAMES:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique_str(mainw->affected_layouts_map,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      orig_fps = lives_strtod(array[3]);
      resampled_frame = count_resampled_frames(frameno, orig_fps, mainw->files[clipno]->fps);
      if (resampled_frame <= atoi(array[2])) {
        text = lives_strdup_printf("%s\n", array[0]);
        lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
        lives_free(text);
        mainw->affected_layouts_map = lives_list_append_unique_str(mainw->affected_layouts_map, array[0]);
      }
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
  case LMAP_ERROR_DELETE_AUDIO:
  case LMAP_ERROR_ALTER_AUDIO:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique_str(mainw->affected_layouts_map,
                                    mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      max_time = lives_strtod(array[4]);
      if (max_time > 0. && atime <= max_time) {
        text = lives_strdup_printf("%s\n", array[0]);
        lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
        lives_free(text);
        mainw->affected_layouts_map = lives_list_append_unique_str(mainw->affected_layouts_map, array[0]);
      }
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  }

  lives_widget_set_sensitive(mainw->show_layout_errors, TRUE);
  if (mainw->multitrack) lives_widget_set_sensitive(mainw->multitrack->show_layout_errors, TRUE);
  return TRUE;
}


void clear_lmap_errors(void) {
  LiVESTextIter start_iter, end_iter;
  LiVESList *lmap;

  lives_text_buffer_get_start_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &start_iter);
  lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter);
  lives_text_buffer_delete(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &start_iter, &end_iter);

  lmap = mainw->affected_layouts_map;

  while (lmap) {
    lives_free((livespointer)lmap->data);
    lmap = lmap->next;
  }
  lives_list_free(lmap);

  mainw->affected_layouts_map = NULL;
  lives_widget_set_sensitive(mainw->show_layout_errors, FALSE);
  if (mainw->multitrack) lives_widget_set_sensitive(mainw->multitrack->show_layout_errors, FALSE);

  if (mainw->affected_layout_marks) {
    remove_current_from_affected_layouts(mainw->multitrack);
  }
}


boolean do_std_checks(const char *type_name, const char *type, size_t maxlen, const char *nreject) {
  char *xtype = lives_strdup(type), *msg;
  const char *reject = " /\\*\"";
  size_t slen = strlen(type_name);

  if (nreject) reject = nreject;

  if (slen == 0) {
    msg = lives_strdup_printf(_("\n%s names may not be blank.\n"), xtype);
    if (!mainw->osc_auto) do_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  if (slen > MAX_SET_NAME_LEN) {
    msg = lives_strdup_printf(_("\n%s names may not be longer than %d characters.\n"), xtype, (int)maxlen);
    if (!mainw->osc_auto) do_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  if (strcspn(type_name, reject) != slen) {
    msg = lives_strdup_printf(_("\n%s names may not contain spaces or the characters%s.\n"), xtype, reject);
    if (!mainw->osc_auto) do_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  for (int i = 0; i < slen; i++) {
    if (type_name[i] == '.' && (i == 0 || type_name[i - 1] == '.')) {
      msg = lives_strdup_printf(_("\n%s names may not start with a '.' or contain '..'\n"), xtype);
      if (!mainw->osc_auto) do_error_dialog(msg);
      lives_free(msg);
      lives_free(xtype);
      return FALSE;
    }
  }

  lives_free(xtype);
  return TRUE;
}


LIVES_GLOBAL_INLINE const char *get_image_ext_for_type(lives_img_type_t imgtype) {
  switch (imgtype) {
  case IMG_TYPE_JPEG: return LIVES_FILE_EXT_JPG; // "jpg"
  case IMG_TYPE_PNG: return LIVES_FILE_EXT_PNG; // "png"
  default: return "";
  }
}


LIVES_GLOBAL_INLINE lives_img_type_t lives_image_ext_to_img_type(const char *img_ext) {
  return lives_image_type_to_img_type(image_ext_to_lives_image_type(img_ext));
}


LIVES_GLOBAL_INLINE const char *image_ext_to_lives_image_type(const char *img_ext) {
  if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) return LIVES_IMAGE_TYPE_PNG;
  if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) return LIVES_IMAGE_TYPE_JPEG;
  return LIVES_IMAGE_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE lives_img_type_t lives_image_type_to_img_type(const char *lives_img_type) {
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_PNG)) return IMG_TYPE_PNG;
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_JPEG)) return IMG_TYPE_JPEG;
  return IMG_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE char *make_image_file_name(lives_clip_t *sfile, frames_t frame,
    const char *img_ext) {
  char *fname, *ret;
  const char *ximg_ext = img_ext;
  if (!ximg_ext || !*ximg_ext) {
    sfile->img_type = resolve_img_type(sfile);
    ximg_ext = get_image_ext_for_type(sfile->img_type);
  }
  fname = lives_strdup_printf("%08d.%s", frame, ximg_ext);
  ret = lives_build_filename(prefs->workdir, sfile->handle, fname, NULL);
  lives_free(fname);
  return ret;
}


LIVES_GLOBAL_INLINE char *make_image_short_name(lives_clip_t *sfile, frames_t frame, const char *img_ext) {
  const char *ximg_ext = img_ext;
  if (!ximg_ext) ximg_ext = get_image_ext_for_type(sfile->img_type);
  return lives_strdup_printf("%08d.%s", frame, ximg_ext);
}


/** @brief check number of frames is correct
  for files of type CLIP_TYPE_DISK
  - check the image files (e.g. jpeg or png)

  use a "goldilocks" algorithm (just the right frames, not too few and not too many)

  ignores gaps */
boolean check_frame_count(int idx, boolean last_checked) {
  /// make sure nth frame is there...
  char *frame;
  if (mainw->files[idx]->frames > 0) {
    frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames,
                                 get_image_ext_for_type(mainw->files[idx]->img_type));
    if (!lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
      // not enough frames
      lives_free(frame);
      return FALSE;
    }
    lives_free(frame);
  }

  /// ...make sure n + 1 th frame is not
  frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames + 1,
                               get_image_ext_for_type(mainw->files[idx]->img_type));

  if (lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
    /// too many frames
    lives_free(frame);
    return FALSE;
  }
  lives_free(frame);

  /// just right
  return TRUE;
}


/** @brief sets mainw->files[idx]->frames with current framecount

   calls smogrify which physically finds the last frame using a (fast) O(log n) binary search method
   for CLIP_TYPE_DISK only
   (CLIP_TYPE_FILE should use the decoder plugin frame count) */
frames_t get_frame_count(int idx, int start) {
  ssize_t bytes;
  char *com = lives_strdup_printf("%s count_frames \"%s\" %s %d", prefs->backend_sync, mainw->files[idx]->handle,
                                  get_image_ext_for_type(mainw->files[idx]->img_type), start);

  bytes = lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);

  if (THREADVAR(com_failed)) return 0;

  if (bytes > 0) return atoi(mainw->msg);
  return 0;
}


boolean get_frames_sizes(int fileno, frames_t frame, int *hsize, int *vsize) {
  // get the actual physical frame size
  lives_clip_t *sfile = mainw->files[fileno];
  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
  char *fname = make_image_file_name(sfile, frame, get_image_ext_for_type(sfile->img_type));
  weed_set_int_value(layer, LIVES_LEAF_HOST_FLAGS, LIVES_LAYER_GET_SIZE_ONLY);
  if (!weed_layer_create_from_file_progressive(layer, fname, 0, 0, WEED_PALETTE_END,
      get_image_ext_for_type(sfile->img_type))) {
    lives_free(fname);
    return FALSE;
  }
  lives_free(fname);
  *hsize = weed_layer_get_width(layer);
  *vsize = weed_layer_get_height(layer);
  weed_layer_free(layer);
  return TRUE;
}


void get_location(const char *exe, char *val, int maxlen) {
  // find location of "exe" in path
  // sets it in val which is a char array of maxlen bytes

  char *loc;
  if ((loc = lives_find_program_in_path(exe)) != NULL) {
    lives_snprintf(val, maxlen, "%s", loc);
    lives_free(loc);
  } else {
    lives_memset(val, 0, 1);
  }
}


LIVES_LOCAL_INLINE lives_checkstatus_t has_executable(const char *exe) {
  char *loc;
  if ((loc = lives_find_program_in_path(exe)) != NULL) {
    lives_free(loc);
    return PRESENT;
  }
  // for now we don't return MISSING (requires code update to differentiate MISSING / UNCHECKED / PRESENT)
  return FALSE;
}


// check if executable is present, missing or unchecked
// if unchecked, check for it, and if not found ask the user politely to install it
boolean check_for_executable(lives_checkstatus_t *cap, const char *exec) {
#ifdef NEW_CHECKSTATUS
  if (!cap || (*cap)->present == UNCHECKED) {
    if (!cap || ((*cap)->flags & INSTALL_CANLOCAL)) {
      /// TODO (next version)
#else
  if (!cap || *cap == UNCHECKED) {
    if (!lives_strcmp(exec, EXEC_YOUTUBE_DL)) {
#endif
      char *localv = lives_build_filename(capable->home_dir, LOCAL_HOME_DIR, "bin", exec, NULL);
      if (lives_file_test(localv, LIVES_FILE_TEST_IS_EXECUTABLE)) {
        lives_free(localv);
        if (cap) *cap = LOCAL;
        return TRUE;
      }
      lives_free(localv);
    }
    if (has_executable(exec)) {
      if (cap) *cap = PRESENT;
      return TRUE;
    } else {
      if (!lives_strcmp(exec, EXEC_XDOTOOL) || !lives_strcmp(exec, EXEC_WMCTRL)) {
        if (cap) *cap = MISSING;
      }
      //if (importance == necessary)
      //do_please_install(exec);
#ifdef HAS_MISSING_PRESENCE
      if (cap) *cap = MISSING;
#endif
      //do_program_not_found_error(exec);
      return FALSE;
    }
  }
#if 0
}
}
#endif
return (*cap == PRESENT || *cap == LOCAL);
}


uint64_t get_version_hash(const char *exe, const char *sep, int piece) {
  /// get version hash output for an executable from the backend
  uint64_t val;
  char buff[128];
  char **array;
  int ntok;

  lives_popen(exe, TRUE, buff, 128);
  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return -2;
  }
  ntok = get_token_count(buff, sep[0]);
  if (ntok < piece) return -1;
  array = lives_strsplit(buff, sep, ntok);
  val = make_version_hash(array[piece]);
  lives_strfreev(array);
  return val;
}


#define VER_MAJOR_MULT 1000000
#define VER_MINOR_MULT 1000
#define VER_MICRO_MULT 1

uint64_t make_version_hash(const char *ver) {
  /// convert a version to uint64_t hash, for comparing
  char **array;
  uint64_t hash;
  int ntok;

  if (!ver) return 0;

  ntok = get_token_count((char *)ver, '.');
  array = lives_strsplit(ver, ".", ntok);

  hash = atoi(array[0]) * VER_MAJOR_MULT;
  if (ntok > 1) {
    hash += atoi(array[1]) * VER_MINOR_MULT;
    if (ntok > 2) hash += atoi(array[2]) * VER_MICRO_MULT;
  }

  lives_strfreev(array);
  return hash;
}


char *unhash_version(uint64_t version) {
  if (!version) return lives_strdup(_("'Unknown'"));
  else {
    uint64_t maj = version / VER_MAJOR_MULT, min;
    version -= maj * VER_MAJOR_MULT;
    min = version / VER_MINOR_MULT;
    version -= min * VER_MINOR_MULT;
    return lives_strdup_printf("%lu.%lu.%lu", maj, min, version);
  }
}


boolean check_snap(const char *prog) {
  // not working yet...
  /* if (!check_for_executable(&capable->has_snap, EXEC_SNAP)) return FALSE; */
  /* char *com = lives_strdup_printf("%s find %s", EXEC_SNAP, prog); */
  /* char *res = grep_in_cmd(com, 0, 1, prog, 0, 1, FALSE); */
  /* if (!res) return FALSE; */
  /* lives_free(res); */
  return TRUE;
}


#define SUDO_APT_INSTALL "sudo apt install %s"
#define SU_PKG_INSTALL "su pkg install %s"

char *get_install_cmd(const char *distro, const char *exe) {
  char *cmd = NULL;
  const char *pkgname = NULL;

  if (!distro) distro = capable->distro_name;

  if (!lives_strcmp(exe, EXEC_PIP)) {
    if (!lives_strcmp(distro, DISTRO_UBUNTU)) {
      if (capable->python_version >= 3000000) pkgname = "python3-pip";
      else if (capable->python_version >= 2000000) pkgname = "python-pip";
      else pkgname = "python3 python3-pip";
    }
    if (!lives_strcmp(distro, DISTRO_FREEBSD)) {
      if (capable->python_version >= 3000000) pkgname = "py3-pip";
      else if (capable->python_version >= 2000000) pkgname = "py2-pip";
      else pkgname = "python py3-pip";
    }
  }
  if (!strcmp(exe, EXEC_GZIP)) pkgname = EXEC_GZIP;
  if (!strcmp(exe, EXEC_YOUTUBE_DL)) pkgname = EXEC_YOUTUBE_DL;
  if (!strcmp(exe, EXEC_YOUTUBE_DLC)) pkgname = EXEC_YOUTUBE_DLC;

  if (!pkgname) pkgname = exe;

  // TODO - add more, eg. pacman, dpkg
  if (!lives_strcmp(distro, DISTRO_UBUNTU)) {
    cmd = lives_strdup_printf(SUDO_APT_INSTALL, pkgname);
  }
  if (!lives_strcmp(distro, DISTRO_FREEBSD)) {
    cmd = lives_strdup_printf(SU_PKG_INSTALL, pkgname);
  }
  return cmd;
}


char *get_install_lib_cmd(const char *distro, const char *libname) {
  char *libpkg = lives_strdup_printf("lib%s-dev", libname);
  char *cmd = get_install_cmd(NULL, libpkg);
  lives_free(libpkg);
  return cmd;
}


char *repl_workdir(const char *entry, boolean fwd) {
  // replace prefs->workdir with string workdir or vice-versa. This allows us to relocate workdir if necessary.
  // used for layout.map file
  // return value should be freed

  // fwd TRUE replaces "/tmp/foo" with "workdir"
  // fwd FALSE replaces "workdir" with "/tmp/foo"
  size_t wdl;
  char *string = lives_strdup(entry);

  if (fwd) {
    if (!lives_strncmp(entry, prefs->workdir, (wdl = lives_strlen(prefs->workdir)))) {
      lives_free(string);
      string = lives_strdup_printf("%s%s", WORKDIR_LITERAL, entry + wdl);
    }
  } else {
    if (!lives_strncmp(entry, WORKDIR_LITERAL, WORKDIR_LITERAL_LEN)) {
      lives_free(string);
      string = lives_build_filename(prefs->workdir, entry + WORKDIR_LITERAL_LEN, NULL);
    }
  }
  return string;
}


LIVES_GLOBAL_INLINE void get_play_times(void) {
  update_timer_bars(0, 0, 0, 0, 0);
}


void update_play_times(void) {
  // force a redraw, reread audio
  if (!CURRENT_CLIP_IS_PHYSICAL) return;
  if (cfile->audio_waveform) {
    lives_mutex_lock_carefully(&mainw->tlthread_mutex);
    if (mainw->drawtl_thread) {
      if (!lives_proc_thread_check_finished(mainw->drawtl_thread)) {
        lives_proc_thread_cancel(mainw->drawtl_thread, FALSE);
      }
      lives_proc_thread_join(mainw->drawtl_thread);
      mainw->drawtl_thread = NULL;
    }
    for (int i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
    lives_freep((void **)&cfile->audio_waveform);
    lives_freep((void **)&cfile->aw_sizes);
    pthread_mutex_unlock(&mainw->tlthread_mutex);
  }
  get_play_times();
}


void get_total_time(lives_clip_t *sfile) {
  // get times (video, left and right audio)

  sfile->laudio_time = sfile->raudio_time = sfile->video_time = 0.;

  if (sfile->opening) {
    int frames;
    if (sfile->frames != 123456789) frames = sfile->frames;
    else frames = sfile->opening_frames;
    if (frames * sfile->fps > 0) {
      sfile->video_time = sfile->frames / sfile->fps;
    }
    return;
  }

  if (sfile->fps > 0.) {
    sfile->video_time = sfile->frames / sfile->fps;
  }

  if (sfile->asampsize >= 8 && sfile->arate > 0 && sfile->achans > 0) {
    sfile->laudio_time = (double)(sfile->afilesize / (sfile->asampsize >> 3) / sfile->achans) / (double)sfile->arate;
    if (sfile->achans > 1) {
      sfile->raudio_time = sfile->laudio_time;
    }
  }

  if (sfile->laudio_time + sfile->raudio_time == 0. && !sfile->opening) {
    sfile->achans = sfile->afilesize = sfile->asampsize = sfile->arate = sfile->arps = 0;
  }
}


void find_when_to_stop(void) {
  // work out when to stop playing
  //
  // ---------------
  //        no loop              loop to fit                 loop cont
  //        -------              -----------                 ---------
  // a>v    stop on video end    stop on audio end           no stop
  // v>a    stop on video end    stop on video end           no stop
  // generator start - not playing : stop on vid_end, unless pure audio;
  if (mainw->alives_pid > 0) mainw->whentostop = NEVER_STOP;
  else if (mainw->aud_rec_fd != -1 &&
           mainw->ascrap_file == -1) mainw->whentostop = STOP_ON_VID_END;
  else if (mainw->multitrack && CURRENT_CLIP_HAS_VIDEO) mainw->whentostop = STOP_ON_VID_END;
  else if (!CURRENT_CLIP_IS_NORMAL) {
    if (mainw->loop_cont) mainw->whentostop = NEVER_STOP;
    else mainw->whentostop = STOP_ON_VID_END;
  } else if (cfile->opening_only_audio) mainw->whentostop = STOP_ON_AUD_END;
  else if (cfile->opening_audio) mainw->whentostop = STOP_ON_VID_END;
  else if (!mainw->preview && (mainw->loop_cont)) mainw->whentostop = NEVER_STOP;
  else if (!CURRENT_CLIP_HAS_VIDEO || (mainw->loop && cfile->achans > 0 && !mainw->is_rendering
                                       && (mainw->audio_end / cfile->fps)
                                       < MAX(cfile->laudio_time, cfile->raudio_time) &&
                                       calc_time_from_frame(mainw->current_file, mainw->play_start) < cfile->laudio_time))
    mainw->whentostop = STOP_ON_AUD_END;
  else mainw->whentostop = STOP_ON_VID_END; // tada...
}


void minimise_aspect_delta(double aspect, int hblock, int vblock, int hsize, int vsize, int *width, int *height) {
  // we will use trigonometry to calculate the smallest difference between a given
  // aspect ratio and the actual frame size. If the delta is smaller than current
  // we set the height and width
  int cw = width[0];
  int ch = height[0];

  int real_width, real_height;
  uint64_t delta, current_delta;

  // minimise d[(x-x1)^2 + (y-y1)^2]/d[x1], to get approximate values
  int calc_width = (int)((vsize + aspect * hsize) * aspect / (aspect * aspect + 1.));

  int i;

  current_delta = (hsize - cw) * (hsize - cw) + (vsize - ch) * (vsize - ch);

#ifdef DEBUG_ASPECT
  lives_printerr("aspect %.8f : width %d height %d is best fit\n", aspect, calc_width, (int)(calc_width / aspect));
#endif
  // use the block size to find the nearest allowed size
  for (i = -1; i < 2; i++) {
    real_width = (int)(calc_width / hblock + i) * hblock;
    real_height = (int)(real_width / aspect / vblock + .5) * vblock;
    delta = (hsize - real_width) * (hsize - real_width) + (vsize - real_height) * (vsize - real_height);

    if (real_width % hblock != 0 || real_height % vblock != 0 ||
        ABS((double)real_width / (double)real_height - aspect) > ASPECT_ALLOWANCE) {
      // encoders can be fussy, so we need to fit both aspect ratio and blocksize
      while (1) {
        real_width = ((int)(real_width / hblock) + 1) * hblock;
        real_height = (int)((double)real_width / aspect + .5);

        if (real_height % vblock == 0) break;

        real_height = ((int)(real_height / vblock) + 1) * vblock;
        real_width = (int)((double)real_height * aspect + .5);

        if (real_width % hblock == 0) break;
      }
    }

#ifdef DEBUG_ASPECT
    lives_printerr("block quantise to %d x %d\n", real_width, real_height);
#endif
    if (delta < current_delta) {
#ifdef DEBUG_ASPECT
      lives_printerr("is better fit\n");
#endif
      current_delta = delta;
      width[0] = real_width;
      height[0] = real_height;
    }
  }
}


void zero_spinbuttons(void) {
  lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 0., 0.);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 0.);
  lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
  lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 0., 0.);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 0.);
  lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);
  lives_widget_set_can_focus(mainw->spinbutton_start, FALSE);
  lives_widget_set_can_focus(mainw->spinbutton_end, FALSE);
}


void set_start_end_spins(int clipno) {
  // consider:
  //    showclipimgs();
  //    redraw_timeline(clipno);
  if (CLIP_HAS_VIDEO(clipno)) {
    lives_clip_t *sfile = mainw->files[clipno];
    lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end),
                                sfile->frames == 0 ? 0. : 1., (double)sfile->frames);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), (double)sfile->end);
    lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);

    lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start),
                                sfile->frames == 0 ? 0. : 1., (double)sfile->frames);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), (double)sfile->start);
    lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
  } else zero_spinbuttons();
  lives_widget_set_can_focus(mainw->spinbutton_start, TRUE);
  lives_widget_set_can_focus(mainw->spinbutton_end, TRUE);
  update_sel_menu();
}


boolean switch_aud_to_jack(boolean set_in_prefs) {
#ifdef ENABLE_JACK
  if (mainw->is_ready) {
    if (!mainw->jackd) {
      jack_audio_init();
      jack_audio_read_init();
      mainw->jackd = jack_get_driver(0, TRUE);
      if (!jack_create_client_writer(mainw->jackd)) {
        mainw->jackd = NULL;
        return FALSE;
      }
      mainw->jackd->whentostop = &mainw->whentostop;
      mainw->jackd->cancelled = &mainw->cancelled;
      mainw->jackd->in_use = FALSE;
      jack_write_client_activate(mainw->jackd);
    }

    lives_widget_set_sensitive(mainw->show_jackmsgs, TRUE);

    mainw->aplayer_broken = FALSE;
    lives_widget_show(mainw->vol_toolitem);
    if (mainw->vol_label) lives_widget_show(mainw->vol_label);
    lives_widget_show(mainw->recaudio_submenu);
    lives_widget_set_sensitive(mainw->vol_toolitem, TRUE);

    if (mainw->vpp && mainw->vpp->get_audio_fmts)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read) {
      pulse_close_client(mainw->pulsed_read);
      mainw->pulsed_read = NULL;
    }

    if (mainw->pulsed) {
      pulse_close_client(mainw->pulsed);
      mainw->pulsed = NULL;
      pulse_shutdown();
    }
#endif
  }
  prefs->audio_player = AUD_PLAYER_JACK;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_JACK);

  if (mainw->is_ready && mainw->vpp && mainw->vpp->get_audio_fmts)
    mainw->vpp->audio_codec = get_best_audio(mainw->vpp);
  jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);

  lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
  lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
  lives_widget_set_sensitive(mainw->mute_audio, TRUE);
  lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);

  if (mainw->p_mutebutton) lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);

  return TRUE;
#endif
  return FALSE;
}


boolean switch_aud_to_pulse(boolean set_in_prefs) {
#ifdef HAVE_PULSE_AUDIO
  boolean retval;

  if (mainw->is_ready) {
    if ((retval = lives_pulse_init(-1))) {
      if (!mainw->pulsed) {
        pulse_audio_init();
        pulse_audio_read_init();
        mainw->pulsed = pulse_get_driver(TRUE);
        mainw->pulsed->whentostop = &mainw->whentostop;
        mainw->pulsed->cancelled = &mainw->cancelled;
        mainw->pulsed->in_use = FALSE;
        pulse_driver_activate(mainw->pulsed);
      }
      mainw->aplayer_broken = FALSE;
      lives_widget_show(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_show(mainw->vol_label);
      lives_widget_show(mainw->recaudio_submenu);
      lives_widget_set_sensitive(mainw->vol_toolitem, TRUE);

      prefs->audio_player = AUD_PLAYER_PULSE;
      if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_PULSE);

      if (mainw->vpp && mainw->vpp->get_audio_fmts)
        mainw->vpp->audio_codec = get_best_audio(mainw->vpp);
    }

#ifdef ENABLE_JACK
    if (mainw->jackd_read) {
      jack_close_client(mainw->jackd_read);
      mainw->jackd_read = NULL;
    }

    if (mainw->jackd) {
      jack_close_client(mainw->jackd);
      mainw->jackd = NULL;
    }
    lives_widget_set_sensitive(mainw->show_jackmsgs, FALSE);
#endif

    pulse_rec_audio_to_clip(-1, -1, RECA_MONITOR);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
    lives_widget_set_sensitive(mainw->mute_audio, TRUE);
    lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
    if (mainw->play_window)
      lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);

    return retval;
  }
#endif
  return FALSE;
}


boolean switch_aud_to_sox(boolean set_in_prefs) {
  if (!capable->has_sox_play) return FALSE; // TODO - show error

  prefs->audio_player = AUD_PLAYER_SOX;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_SOX);
  //set_string_pref(PREF_AUDIO_PLAY_COMMAND, prefs->audio_play_command);

  if (mainw->is_ready) {
    /* //ubuntu / Unity has a hissy fit if you hide things in the menu !
      lives_widget_hide(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
    */
    lives_widget_set_sensitive(mainw->vol_toolitem, FALSE);
    lives_widget_hide(mainw->recaudio_submenu);

    if (mainw->vpp && mainw->vpp->get_audio_fmts)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->mute_audio, TRUE);
    lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
    lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read) {
    jack_close_client(mainw->jackd_read);
    mainw->jackd_read = NULL;
  }

  if (mainw->jackd) {
    jack_close_client(mainw->jackd);
    mainw->jackd = NULL;
  }
  lives_widget_set_sensitive(mainw->show_jackmsgs, FALSE);
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read = NULL;
  }

  if (mainw->pulsed) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed = NULL;
    pulse_shutdown();
  }
#endif
  return TRUE;
}


void switch_aud_to_none(boolean set_in_prefs) {
  prefs->audio_player = AUD_PLAYER_NONE;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_NONE);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_NONE);

  if (mainw->is_ready) {
    /* //ubuntu has a hissy fit if you hide things in the menu
      lives_widget_hide(mainw->vol_toolitem);
      if (mainw->vol_label) lives_widget_hide(mainw->vol_label);
    */
    lives_widget_set_sensitive(mainw->vol_toolitem, FALSE);
    // lives_widget_hide(mainw->recaudio_submenu);

    if (mainw->vpp && mainw->vpp->get_audio_fmts)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->mute_audio, FALSE);
    lives_widget_set_sensitive(mainw->m_mutebutton, FALSE);
    if (mainw->preview_box) {
      lives_widget_set_sensitive(mainw->p_mutebutton, FALSE);
    }
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read) {
    jack_close_client(mainw->jackd_read);
    lives_nanosleep(100000000);
    mainw->jackd_read = NULL;
  }

  if (mainw->jackd) {
    jack_close_client(mainw->jackd);
    mainw->jackd = NULL;
  }
  lives_widget_set_sensitive(mainw->show_jackmsgs, FALSE);
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read = NULL;
  }

  if (mainw->pulsed) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed = NULL;
    pulse_shutdown();
  }
#endif
}


boolean prepare_to_play_foreign(void) {
  // here we are going to 'play' a captured external window

#ifdef GUI_GTK

#if !GTK_CHECK_VERSION(3, 0, 0)
#ifdef GDK_WINDOWING_X11
  GdkVisual *vissi = NULL;
  int i;
#endif
#endif
#endif

  int new_file = mainw->first_free_file;

  mainw->foreign_window = NULL;

  // create a new 'file' to play into
  if (!get_new_handle(new_file, NULL)) {
    return FALSE;
  }

  mainw->current_file = new_file;
  migrate_from_staging(mainw->current_file);

  if (mainw->rec_achans > 0) {
    cfile->arate = cfile->arps = mainw->rec_arate;
    cfile->achans = mainw->rec_achans;
    cfile->asampsize = mainw->rec_asamps;
    cfile->signed_endian = mainw->rec_signed_endian;
#ifdef HAVE_PULSE_AUDIO
    if (mainw->rec_achans > 0 && prefs->audio_player == AUD_PLAYER_PULSE) {
      pulse_rec_audio_to_clip(mainw->current_file, -1, RECA_WINDOW_GRAB);
      mainw->pulsed_read->in_use = TRUE;
    }
#endif
#ifdef ENABLE_JACK
    if (mainw->rec_achans > 0 && prefs->audio_player == AUD_PLAYER_JACK) {
      jack_rec_audio_to_clip(mainw->current_file, -1, RECA_WINDOW_GRAB);
      mainw->jackd_read->in_use = TRUE;
    }
#endif
  }

  cfile->hsize = mainw->foreign_width / 2 + 1;
  cfile->vsize = mainw->foreign_height / 2 + 3;

  cfile->fps = cfile->pb_fps = mainw->rec_fps;

  resize(-2);

  lives_widget_show(mainw->playframe);
  lives_widget_show(mainw->playarea);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_set_opacity(mainw->playframe, 1.);

  // size must be exact, must not be larger than play window or we end up with nothing
  mainw->pwidth = lives_widget_get_allocation_width(mainw->playframe);// - H_RESIZE_ADJUST + 2;
  mainw->pheight = lives_widget_get_allocation_height(mainw->playframe);// - V_RESIZE_ADJUST + 2;

  cfile->hsize = mainw->pwidth;
  cfile->vsize = mainw->pheight;

  cfile->img_type = IMG_TYPE_BEST; // override the pref

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)

#ifdef GDK_WINDOWING_X11
  mainw->foreign_window = gdk_x11_window_foreign_new_for_display
                          (mainw->mgeom[widget_opts.monitor].disp,
                           mainw->foreign_id);
#else
#ifdef GDK_WINDOWING_WIN32
  if (!mainw->foreign_window)
    mainw->foreign_window = gdk_win32_window_foreign_new_for_display
                            (mainw->mgeom[widget_opts.monitor].disp,
                             mainw->foreign_id);
#endif

#endif // GDK_WINDOWING

  if (mainw->foreign_window) lives_xwindow_set_keep_above(mainw->foreign_window, TRUE);

#else // 3, 0, 0
  mainw->foreign_window = gdk_window_foreign_new(mainw->foreign_id);
#endif
#endif // GUI_GTK

#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
#if !GTK_CHECK_VERSION(3, 0, 0)

  if (mainw->foreign_visual) {
    for (i = 0; i < capable->nmonitors; i++) {
      vissi = gdk_x11_screen_lookup_visual(mainw->mgeom[i].screen, hextodec(mainw->foreign_visual));
      if (vissi) break;
    }
  }

  if (!vissi) vissi = gdk_visual_get_best_with_depth(mainw->foreign_bpp);
  if (!vissi) return FALSE;

  mainw->foreign_cmap = gdk_x11_colormap_foreign_new(vissi,
                        gdk_x11_colormap_get_xcolormap(gdk_colormap_new(vissi, TRUE)));

  if (!mainw->foreign_cmap) return FALSE;

#endif
#endif
#endif

  if (!mainw->foreign_window) return FALSE;

  mainw->play_start = 1;
  if (mainw->rec_vid_frames == -1) mainw->play_end = INT_MAX;
  else mainw->play_end = mainw->rec_vid_frames;

  mainw->rec_samples = -1;

  omute = mainw->mute;
  osepwin = mainw->sep_win;
  ofs = mainw->fs;
  ofaded = mainw->faded;
  odouble = mainw->double_size;

  mainw->mute = TRUE;
  mainw->sep_win = FALSE;
  mainw->fs = FALSE;
  mainw->faded = TRUE;
  mainw->double_size = FALSE;

  lives_widget_hide(mainw->t_sepwin);
  lives_widget_hide(mainw->t_infobutton);

  return TRUE;
}


boolean after_foreign_play(void) {
  // read details from capture file
  int capture_fd = -1;
  char *capfile = lives_strdup_printf("%s/.capture.%d", prefs->workdir, capable->mainpid);
  char capbuf[256];
  ssize_t length;
  int new_frames = 0;
  int old_file = mainw->current_file;

  char **array;

  // assume for now we only get one clip passed back
  if ((capture_fd = lives_open2(capfile, O_RDONLY)) > -1) {
    lives_memset(capbuf, 0, 256);
    if ((length = read(capture_fd, capbuf, 256))) {
      if (get_token_count(capbuf, '|') > 2) {
        array = lives_strsplit(capbuf, "|", 3);
        new_frames = atoi(array[1]);
        if (new_frames > 0) {
          create_cfile(-1, array[0], FALSE);
          lives_strfreev(array);
          lives_snprintf(cfile->file_name, 256, "Capture %d", mainw->cap_number);
          lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "Capture %d", mainw->cap_number++);
          lives_snprintf(cfile->type, 40, "Frames");

          cfile->progress_start = cfile->start = 1;
          cfile->progress_end = cfile->frames = cfile->end = new_frames;
          cfile->pb_fps = cfile->fps = mainw->rec_fps;

          cfile->hsize = CEIL(mainw->foreign_width, 4);
          cfile->vsize = CEIL(mainw->foreign_height, 4);

          cfile->img_type = IMG_TYPE_BEST;
          cfile->changed = TRUE;

          if (mainw->rec_achans > 0) {
            cfile->arate = cfile->arps = mainw->rec_arate;
            cfile->achans = mainw->rec_achans;
            cfile->asampsize = mainw->rec_asamps;
            cfile->signed_endian = mainw->rec_signed_endian;
          }

          save_clip_values(mainw->current_file);
          if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

          close(capture_fd);
          lives_rm(capfile);
          capture_fd = -1;
          do_threaded_dialog(_("Cleaning up clip"), FALSE);
          lives_widget_show_all(mainw->proc_ptr->processing);
          resize_all(mainw->current_file, cfile->hsize, cfile->vsize, cfile->img_type, FALSE, NULL, NULL);
          end_threaded_dialog();
          if (cfile->afilesize > 0 && cfile->achans > 0
              && CLIP_TOTAL_TIME(mainw->current_file) > cfile->laudio_time + AV_TRACK_MIN_DIFF) {
            pad_with_silence(mainw->current_file, FALSE, TRUE);
          }
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (capture_fd > -1) {
    close(capture_fd);
    lives_rm(capfile);
  }

  if (new_frames == 0) {
    // nothing captured; or cancelled
    lives_free(capfile);
    return FALSE;
  }

  cfile->nopreview = FALSE;
  lives_free(capfile);

  add_to_clipmenu();
  if (!mainw->multitrack) switch_to_file(old_file, mainw->current_file);

  else {
    int new_file = mainw->current_file;
    mainw->current_file = mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack, new_file, TRUE);
    mt_clip_select(mainw->multitrack, TRUE);
  }

  cfile->is_loaded = TRUE;
  cfile->changed = TRUE;
  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  return TRUE;
}


void reset_clipmenu(void) {
  // sometimes the clip menu gets messed up, e.g. after reloading a set.
  // This function will clean up the 'x's and so on.

  if (mainw->current_file > 0 && cfile && cfile->menuentry) {
#ifdef GTK_RADIO_MENU_BUG
    register int i;
    for (i = 1; i < MAX_FILES; i++) {
      if (i != mainw->current_file && mainw->files[i] && mainw->files[i]->menuentry) {
        lives_signal_handler_block(mainw->files[i]->menuentry, mainw->files[i]->menuentry_func);
        lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->files[i]->menuentry), FALSE);
        lives_signal_handler_unblock(mainw->files[i]->menuentry, mainw->files[i]->menuentry_func);
      }
    }
#endif
    lives_signal_handler_block(cfile->menuentry, cfile->menuentry_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(cfile->menuentry), TRUE);
    lives_signal_handler_unblock(cfile->menuentry, cfile->menuentry_func);
  }
}


void activate_url_inner(const char *link) {
#if GTK_CHECK_VERSION(2, 14, 0)
  LiVESError *err = NULL;
#if GTK_CHECK_VERSION(3, 22, 0)
  gtk_show_uri_on_window(NULL, link, GDK_CURRENT_TIME, &err);
#else
  gtk_show_uri(NULL, link, GDK_CURRENT_TIME, &err);
#endif
#else
  char *com = getenv("BROWSER");
  com = lives_strdup_printf("\"%s\" '%s' &", com ? com : "gnome-open", link);
  lives_system(com, FALSE);
  lives_free(com);
#endif
}


void activate_url(LiVESAboutDialog * about, const char *link, livespointer data) {
  activate_url_inner(link);
}


void show_manual_section(const char *lang, const char *section) {
  char *tmp = NULL, *tmp2 = NULL;
  const char *link;

  link = lives_strdup_printf("%s%s%s%s", LIVES_MANUAL_URL, (lang == NULL ? "" : (tmp2 = lives_strdup_printf("//%s//", lang))),
                             LIVES_MANUAL_FILENAME, (section == NULL ? "" : (tmp = lives_strdup_printf("#%s", section))));

  activate_url_inner(link);

  if (tmp) lives_free(tmp);
  if (tmp2) lives_free(tmp2);
}



void wait_for_bg_audio_sync(int fileno) {
  char *afile = lives_get_audio_file_name(fileno);
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
  ticks_t timeout;

  lives_nanosleep_while_true(sget_file_size(afile) <= 0 && (timeout = lives_alarm_check(alarm_handle)) > 0);

  if (!timeout) break_me("no audio found");
  lives_alarm_clear(alarm_handle);

  lives_free(afile);
}


boolean create_event_space(int length) {
  // try to create desired events
  // if we run out of memory, all events requested are freed, and we return FALSE
  // otherwise we return TRUE

  // NOTE: this is the OLD event system, it's only used for reordering in the clip editor

  if (cfile->resample_events) {
    lives_free(cfile->resample_events);
  }
  if ((cfile->resample_events = (resample_event *)(lives_calloc(length, sizeof(resample_event)))) == NULL) {
    // memory overflow
    return FALSE;
  }
  return TRUE;
}


void add_to_recent(const char *filename, double start, frames_t frames, const char *extra_params) {
  const char *mtext;
  char buff[PATH_MAX * 2];
  char *file, *mfile, *prefname;
  boolean free_cache = FALSE;
  int i;

  if (frames > 0) {
    mfile = lives_strdup_printf("%s|%.2f|%d", filename, start, frames);
    if (!extra_params || (!(*extra_params))) file = lives_strdup(mfile);
    else file = lives_strdup_printf("%s\n%s", mfile, extra_params);
  } else {
    mfile = lives_strdup(filename);
    if (!extra_params || (!(*extra_params))) file = lives_strdup(mfile);
    else file = lives_strdup_printf("%s\n%s", mfile, extra_params);
  }

  for (i = 0; i < N_RECENT_FILES; i++) {
    mtext = lives_menu_item_get_text(mainw->recent[i]);
    if (!lives_strcmp(mfile, mtext)) break;
  }

  if (!i) return;

  if (i == N_RECENT_FILES) --i;

  if (!mainw->prefs_cache) {
    mainw->prefs_cache = cache_file_contents(prefs->configfile);
    free_cache = TRUE;
  }
  for (; i > 0; i--) {
    mtext = lives_menu_item_get_text(mainw->recent[i - 1]);
    lives_menu_item_set_text(mainw->recent[i], mtext, FALSE);
    if (mainw->multitrack) lives_menu_item_set_text(mainw->multitrack->recent[i], mtext, FALSE);

    prefname = lives_strdup_printf("%s%d", PREF_RECENT, i);
    get_utf8_pref(prefname, buff, PATH_MAX * 2);
    lives_free(prefname);

    prefname = lives_strdup_printf("%s%d", PREF_RECENT, i + 1);
    set_utf8_pref(prefname, buff);
    lives_free(prefname);
  }

  if (free_cache) cached_list_free(&mainw->prefs_cache);

  lives_menu_item_set_text(mainw->recent[0], mfile, FALSE);
  if (mainw->multitrack) lives_menu_item_set_text(mainw->multitrack->recent[0], mfile, FALSE);
  prefname = lives_strdup_printf("%s%d", PREF_RECENT, 1);
  set_utf8_pref(prefname, file);
  lives_free(prefname);

  for (; i < N_RECENT_FILES; i++) {
    mtext = lives_menu_item_get_text(mainw->recent[i]);
    if (*mtext) lives_widget_show(mainw->recent[i]);
  }

  lives_free(mfile); lives_free(file);
}


int verhash(char *xv) {
  char *version, *s;
  int major = 0, minor = 0, micro = 0;

  if (!xv) return 0;

  version = lives_strdup(xv);

  if (!(*version)) {
    lives_free(version);
    return 0;
  }

  s = strtok(version, ".");
  if (s) {
    major = atoi(s);
    s = strtok(NULL, ".");
    if (s) {
      minor = atoi(s);
      s = strtok(NULL, ".");
      if (s) micro = atoi(s);
    }
  }
  lives_free(version);
  return major * 1000000 + minor * 1000 + micro;
}


void set_sel_label(LiVESWidget * sel_label) {
  char *tstr, *frstr, *tmp;
  char *sy, *sz;

  if (mainw->current_file == -1 || !cfile->frames || mainw->multitrack) {
    lives_label_set_text(LIVES_LABEL(sel_label), (tmp = _("-------------Selection------------")));
    lives_free(tmp);
  } else {
    double xtime = calc_time_from_frame(mainw->current_file, cfile->end + 1) -
                   calc_time_from_frame(mainw->current_file, cfile->start);
    tstr = format_tstr(xtime, 0);

    frstr = lives_strdup_printf("%d", cfile->end - cfile->start + 1);

    // TRANSLATORS: - try to keep the text of the middle part the same length, by deleting "-" if necessary
    lives_label_set_text(LIVES_LABEL(sel_label),
                         (tmp = lives_strconcat("---------- [ ", tstr, (sy = ((_("] ----------Selection---------- [ ")))),
                                frstr, (sz = (_(" frames ] ----------"))), NULL)));
    lives_free(sy); lives_free(sz);
    lives_free(tmp); lives_free(frstr); lives_free(tstr);
  }
  lives_widget_queue_draw(sel_label);
}


uint32_t get_signed_endian(boolean is_signed, boolean little_endian) {
  // asigned TRUE == signed, FALSE == unsigned

  if (is_signed) {
    if (little_endian) {
      return 0;
    } else {
      return AFORM_BIG_ENDIAN;
    }
  } else {
    if (!is_signed) {
      if (little_endian) {
        return AFORM_UNSIGNED;
      } else {
        return AFORM_UNSIGNED | AFORM_BIG_ENDIAN;
      }
    }
  }
  return AFORM_UNKNOWN;
}


LIVES_GLOBAL_INLINE LiVESInterpType get_interp_value(short quality, boolean low_for_mt) {
  if ((mainw->is_rendering || (mainw->multitrack && mainw->multitrack->is_rendering))
      && !mainw->preview_rendering)
    return LIVES_INTERP_BEST;
  if (low_for_mt && mainw->multitrack) return LIVES_INTERP_FAST;
  if (quality <= PB_QUALITY_LOW) return LIVES_INTERP_FAST;
  else if (quality == PB_QUALITY_MED) return LIVES_INTERP_NORMAL;
  return LIVES_INTERP_BEST;
}


int check_for_bad_ffmpeg(void) {
  int i, fcount;
  char *fname_next;
  boolean maybeok = FALSE;

  fcount = get_frame_count(mainw->current_file, 1);

  for (i = 1; i <= fcount; i++) {
    fname_next = make_image_file_name(cfile, i, get_image_ext_for_type(cfile->img_type));
    if (sget_file_size(fname_next) > 0) {
      lives_free(fname_next);
      maybeok = TRUE;
      break;
    }
    lives_free(fname_next);
  }

  if (!maybeok) {
    widget_opts.non_modal = TRUE;
    do_error_dialog(
      _("Your version of mplayer/ffmpeg may be broken !\n"
        "See http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    widget_opts.non_modal = FALSE;
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}

