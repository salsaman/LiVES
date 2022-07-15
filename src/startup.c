// startup.c
// LiVES
// (c) G. Finch 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#include "main.h"

_palette *palette;
ssize_t sizint, sizdbl, sizshrt;
mainwindow *mainw;

#include "setup.h"
#include "startup.h"
#include "omc-learn.h"
#include "interface.h"
#include "callbacks.h"
#include "diagnostics.h"
#include "effects.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"
#include "paramwindow.h"
#include "stream.h"
#include "cvirtual.h"
#include "ce_thumbs.h"
#include "rfx-builder.h"
#include "multitrack-gui.h"
#include "transcode.h"

#ifndef DISABLE_DIAGNOSTICS
#include "diagnostics.h"
#endif

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#ifdef HAVE_UNICAP
#include "videodev.h"
#endif

#include <getopt.h>
#include <wordexp.h>

#ifdef IS_DARWIN
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif
#ifdef USE_LIBPNG
#include <png.h>
#include <setjmp.h>
#endif

/* #ifdef HAVE_PRCTL */
/* #include <sys/prctl.h> */
/* #endif */

#ifdef LIVES_OS_UNIX
#include <glib-unix.h>
#endif

static boolean lives_init(_ign_opts *ign_opts);
static void do_start_messages(void);

#ifndef VALGRIND_ON
static void set_extra_colours(void);
#endif

capabilities *capable;
//////////////////////////////////////////

static char devmap[PATH_MAX];

static boolean no_recover = FALSE, auto_recover = FALSE;
static boolean info_only;

static char *newconfigfile = NULL;

#ifdef GUI_GTK
static LiVESTargetEntry target_table[]  = {
  { "STRING",                     GTK_TARGET_OTHER_APP, 0 },
  { "text/uri-list",              GTK_TARGET_OTHER_APP, 0 },
};
#endif

static char start_file[PATH_MAX];
static double start = 0.;
static int end = 0;

static boolean theme_error;

static _ign_opts ign_opts;

static int zargc;
static char **zargv;

static char *old_vhash = NULL;
static int initial_startup_phase = 0;
static boolean needs_workdir = FALSE;
static boolean ran_ds_dlg = FALSE;

static char buff[256];
static void print_notice(void) {
  fprintf(stderr, "\nLiVES %s\n", LiVES_VERSION);
  fprintf(stderr, "Copyright "LIVES_COPYRIGHT_YEARS" Gabriel Finch ("LIVES_AUTHOR_EMAIL") and others.\n");
  fprintf(stderr, "LiVES comes with ABSOLUTELY NO WARRANTY\nThis is free software, and you are welcome to redistribute it\n"
          "under certain conditions; "
          "see the file COPYING for details.\n\n");
}


capabilities *get_capabilities(void) {
  // get capabilities of backend system
  char **array;
  char *msg, *tmp;

  char buffer[PATH_MAX * 4];
  char command[PATH_MAX * 4];
  char dir[PATH_MAX];
  int numtok;
  size_t xs;

#ifdef IS_DARWIN
  processor_info_array_t processorInfo;
  mach_msg_type_number_t numProcessorInfo;
  natural_t numProcessors = 0U;
  kern_return_t kerr;
#endif

  buffer[0] = '\0';
  command[0] = '\0';

  if (!check_for_executable(&capable->has_perl, EXEC_PERL)) return capable;

  // this is _compile time_ bits, not runtime bits
  capable->hw.cpu_bits = (sizeof(void *)) * 8;

  capable->ds_used = capable->ds_free = capable->ds_tot = -1;

  capable->mainpid = lives_getpid();

  // cmds part 1
  get_location("cp", capable->cp_cmd, PATH_MAX);
  capable->sysbindir = get_dir(capable->cp_cmd);

  capable->wm_name = NULL;
  capable->wm_type = NULL;

  capable->python_version = 0;
  capable->xstdout = STDOUT_FILENO;

  lives_snprintf(capable->backend_path, PATH_MAX, "%s", (tmp = lives_find_program_in_path(BACKEND_NAME)));
  lives_free(tmp);
  if (!*capable->backend_path) return capable;
  capable->has_smogrify = PRESENT;

retry_configfile:

  if (!mainw->has_session_workdir) {
    lives_snprintf(prefs->backend, PATH_MAX * 4, "%s -s \"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL, capable->backend_path,
                   prefs->configfile);
    lives_snprintf(prefs->backend_sync, PATH_MAX * 4, "%s", prefs->backend);
  } else {
    // if the user passed a -workdir option, we will use that, and the backend won't attempt to find an existing value
    lives_snprintf(prefs->backend, PATH_MAX * 4, "%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL,
                   capable->backend_path, prefs->workdir, prefs->configfile);
    lives_snprintf(prefs->backend_sync, PATH_MAX * 4, "%s", prefs->backend);
  }

  if (!newconfigfile) {
    capable->has_smogrify = UNCHECKED;
    lives_snprintf(command, PATH_MAX * 4, "%s version", prefs->backend_sync);

    lives_popen(command, TRUE, buffer, PATH_MAX * 4);

    if (THREADVAR(com_failed)) {
      return capable;
    }

    xs = lives_strlen(buffer);
    if (xs < 5) return capable;

    lives_chomp(buffer, FALSE);
    numtok = get_token_count(buffer, ' ');
    if (numtok < 2) return capable;

    array = lives_strsplit(buffer, " ", numtok);
    if (strcmp(array[0], "smogrify")) {
      lives_strfreev(array);
      return capable;
    }

    capable->has_smogrify = PRESENT;
    capable->smog_version_correct = FALSE;

    if (strcmp(array[1], LiVES_VERSION)) {
      msg = lives_strdup_printf("Version mismatch: smogrify = %s, LiVES = %s\n", array[1], LiVES_VERSION);
      LIVES_ERROR(msg);
      lives_free(msg);
      lives_strfreev(array);
      return capable;
    }

    lives_strfreev(array);
    capable->smog_version_correct = TRUE;
  }

  if (!newconfigfile)
    lives_snprintf(command, PATH_MAX * 4, "%s report -", prefs->backend_sync);
  else
    lives_snprintf(command, PATH_MAX * 4, "%s report", prefs->backend_sync);

  // check_settings:

  capable->has_smogrify = UNCHECKED;
  lives_popen(command, TRUE, buffer, PATH_MAX * 4);
  if (THREADVAR(com_failed) || lives_strlen(buffer) < 6) return capable;
  capable->has_smogrify = PRESENT;

  numtok = get_token_count(buffer, '|');
  if (numtok < 2) {
    capable->smog_version_correct = FALSE;
    return capable;
  }

  array = lives_strsplit(buffer, "|", numtok);

  if (!newconfigfile) {
    if (!strcmp(array[0], "smogrify::error")) {
      LIVES_ERROR(buffer);
      if (!strcmp(array[1], "config_get")) {
        lives_strfreev(array);
        capable->can_read_from_config = FALSE;
        return capable;
      }
      if (!strcmp(array[1], "config_set_new")) {
        lives_strfreev(array);
        capable->can_write_to_config_new = FALSE;
        return capable;
      }
      if (!strcmp(array[1], "config_set_rec")) {
        lives_strfreev(array);
        capable->can_write_to_config_backup = FALSE;
        return capable;
      }
      if (!strcmp(array[1], "config_set")) {
        lives_strfreev(array);
        capable->can_write_to_config = FALSE;
        return capable;
      }
      // other unspecified error
      mainw->error = TRUE;
      lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%s", buff);
      return capable;
    }
  }

  // the startup phase
  // this is 0 for normal operation
  // -1 for a fresh install
  // after this the value goes to 1....n
  // then finally gets set to 100, which instructs the backend to remove this preference, and return 0
  initial_startup_phase = prefs->startup_phase = atoi(array[2]);

  if (!newconfigfile) {
    if (initial_startup_phase == -1 && !ign_opts.ign_configfile) {
      /// if no configfile:
      /// check for migration:
      /// if $HOME/.lives exists, get the verhash from it
      char *oldconfig  = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_FILE_OLD, NULL);
      if (lives_file_test(oldconfig, LIVES_FILE_TEST_EXISTS)) {
        lives_strfreev(array);
        newconfigfile = lives_strdup(prefs->configfile);
        lives_snprintf(prefs->configfile, PATH_MAX, "%s", oldconfig);
        lives_free(oldconfig);
        goto retry_configfile;
      }
      lives_free(oldconfig);
    }
  }

  // hash of last version used,
  // or 0 if rcfile existed, but we couldn't extract a version
  if (numtok > 3) {
    mainw->old_vhash = lives_strdup(array[3]);
  }

  if (!mainw->old_vhash) {
    old_vhash = lives_strdup("NULL");
  } else if (!*mainw->old_vhash) {
    old_vhash = lives_strdup("not present");
  } else if (!strcmp(mainw->old_vhash, "0")) {
    old_vhash = lives_strdup("unrecoverable");
  } else {
    old_vhash = lives_strdup(mainw->old_vhash);

    if (newconfigfile && *newconfigfile) {
      /// if < 3200000, migrate (copy) .lives and .lives-dir
      /// this should only happen once, since version will now have been updated in .lives
      /// after startup, we will offer to remove the old files
      migrate_config(old_vhash, newconfigfile);
    }
  }

  if (newconfigfile && *newconfigfile) {
    lives_strfreev(array);
    lives_snprintf(prefs->configfile, PATH_MAX, "%s", newconfigfile);
    lives_free(newconfigfile);
    newconfigfile = lives_strdup("");
    lives_free(old_vhash);
    lives_free(mainw->old_vhash);
    goto retry_configfile;
  }

  lives_snprintf(dir, PATH_MAX, "%s", array[1]);

  if (!mainw->has_session_workdir) {
    size_t dirlen = lives_strlen(dir);
    boolean dir_valid = TRUE;

    if (dirlen && strncmp(dir, "(null)", 6)) {
      if (!mainw->old_vhash || !*mainw->old_vhash || !strcmp(mainw->old_vhash, "0")) {
        msg = lives_strdup_printf("The backend found a workdir (%s), but claimed old version was %s !", dir, old_vhash);
        LIVES_WARN(msg);
        lives_free(msg);
      }

      if (dirlen < PATH_MAX - MAX_SET_NAME_LEN * 2) {
        ensure_isdir(dir);

        if (dirlen >= PATH_MAX - MAX_SET_NAME_LEN * 2) {
          dir_toolong_error(dir, (tmp = (_("working directory"))), PATH_MAX - MAX_SET_NAME_LEN * 2, TRUE);
          lives_free(tmp);
          dir_valid = FALSE;
        }

        if (!lives_make_writeable_dir(dir)) {
          do_dir_perm_error(dir, FALSE);
          if (!lives_make_writeable_dir(dir)) {
            dir_valid = FALSE;
          }
        }
      }

      if (dir_valid) {
        lives_snprintf(prefs->workdir, PATH_MAX, "%s", dir);
        lives_snprintf(prefs->backend, PATH_MAX * 4, "%s -s \"%s\" -WORKDIR=\"%s\" -CONFIGFILE=\"%s\" --", EXEC_PERL,
                       capable->backend_path, prefs->workdir, prefs->configfile);
        lives_snprintf(prefs->backend_sync, PATH_MAX * 4, "%s", prefs->backend);

        set_string_pref_priority(PREF_WORKING_DIR, prefs->workdir);

        // for backwards compatibility only
        set_string_pref(PREF_WORKING_DIR_OLD, prefs->workdir);
      } else {
        needs_workdir = TRUE;
      }
    } else {
      if (prefs->startup_phase != -1) {
        msg = lives_strdup_printf("The backend found no workdir, but set startup_phase to %d !\n%s",
                                  prefs->startup_phase, prefs->workdir);
        LIVES_ERROR(msg);
        lives_free(msg);
      }
      needs_workdir = TRUE;
      prefs->startup_phase = -1;
    }

    if (*mainw->old_vhash && strcmp(mainw->old_vhash, "0")) {
      if (atoi(mainw->old_vhash) < atoi(mainw->version_hash)) {
        if (prefs->startup_phase == 0) {
          msg = get_upd_msg();
          lives_snprintf(capable->startup_msg, 1024, "%s", msg);
          lives_free(msg);
          if (numtok > 4 && *array[4]) {
            lives_strappend(capable->startup_msg, 1024, array[4]);
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*

  if ((prefs->startup_phase == 1 || prefs->startup_phase == -1)) {
    needs_workdir = TRUE;
  }

  lives_strfreev(array);

  ///////////////////////////////////////////////////////

  get_location(EXEC_DF, capable->df_cmd, PATH_MAX);
  get_location(EXEC_WC, capable->wc_cmd, PATH_MAX);
  get_location(EXEC_SED, capable->sed_cmd, PATH_MAX);
  get_location(EXEC_GREP, capable->grep_cmd, PATH_MAX);
  get_location(EXEC_EJECT, capable->eject_cmd, PATH_MAX);

  check_for_executable(&capable->has_du, EXEC_DU);

#if USE_INTERNAL_MD5SUM
  capable->has_md5sum = INTERNAL;
#else
  check_for_executable(&capable->has_md5sum, EXEC_MD5SUM);
#endif

  check_for_executable(&capable->has_ffprobe, EXEC_FFPROBE);
  check_for_executable(&capable->has_sox_play, EXEC_PLAY);

  if (!check_for_executable(&capable->has_youtube_dl, EXEC_YOUTUBE_DL)) {
    check_for_executable(&capable->has_youtube_dlc, EXEC_YOUTUBE_DLC);
  }
  check_for_executable(&capable->has_sox_sox, EXEC_SOX);
  check_for_executable(&capable->has_dvgrab, EXEC_DVGRAB);

  if (!check_for_executable(&capable->has_cdda2wav, EXEC_CDDA2WAV)) {
    check_for_executable(&capable->has_icedax, EXEC_ICEDAX);
  }

  check_for_executable(&capable->has_jackd, EXEC_JACKD);
  check_for_executable(&capable->has_pulse_audio, EXEC_PULSEAUDIO);

  if (check_for_executable(&capable->has_python3, EXEC_PYTHON3) == PRESENT) {
    capable->python_version = get_version_hash(EXEC_PYTHON3 " -V 2>&1", " ", 1);
  } else {
    if (check_for_executable(&capable->has_python, EXEC_PYTHON) == PRESENT) {
      capable->python_version = get_version_hash(EXEC_PYTHON " -V 2>&1", " ", 1);
    }
  }

  check_for_executable(&capable->has_xwininfo, EXEC_XWININFO);
  check_for_executable(&capable->has_gconftool_2, EXEC_GCONFTOOL_2);
  check_for_executable(&capable->has_xdg_screensaver, EXEC_XDG_SCREENSAVER);

  if (check_for_executable(NULL, EXEC_MIDISTART)) {
    check_for_executable(&capable->has_midistartstop, EXEC_MIDISTOP);
  }

  capable->hw.ncpus = get_num_cpus();
  if (capable->hw.ncpus == 0) capable->hw.ncpus = 1;

  return capable;
}


static boolean pre_init(void) {
  // stuff which should be done *before* mainwindow is created
  // returns TRUE if we got an error loading the theme
#ifdef ENABLE_JACK
  char jbuff[JACK_PARAM_STRING_MAX];
#endif
#ifdef GUI_GTK
  LiVESPixbuf *iconpix;
#endif

  pthread_mutexattr_t mattr;

  char *msg, *tmp, *tmp2, *cfgdir, *old_libdir = NULL;

  boolean needs_update = FALSE;

  int i;

  /// create context data for main thread; must be called before get_capabilities()
  lives_thread_data_create(0);

#ifdef VALGRIND_ON
  prefs->nfx_threads = 8;
#else
  if (mainw->debug) prefs->nfx_threads = 2;
#endif

  lives_threadpool_init();

  // locate shell commands that may be used in processing
  //
  get_location("touch", capable->touch_cmd, PATH_MAX);
  get_location("rm", capable->rm_cmd, PATH_MAX);
  get_location("rmdir", capable->rmdir_cmd, PATH_MAX);
  get_location("mv", capable->mv_cmd, PATH_MAX);
  get_location("ln", capable->ln_cmd, PATH_MAX);
  get_location("chmod", capable->chmod_cmd, PATH_MAX);
  get_location("cat", capable->cat_cmd, PATH_MAX);
  get_location("echo", capable->echo_cmd, PATH_MAX);

  // need to create directory for configfile before calling get_capabilities()
  // NOTE: this is the one and only time we reference cfgdir, other than this it should be considered sacrosanct
  cfgdir = get_dir(prefs->configfile);
  lives_make_writeable_dir(cfgdir);
  lives_free(cfgdir);

  // pre-checked conditions. We will check for these again
  if (capable->has_perl && capable->can_write_to_workdir && capable->can_write_to_config &&
      capable->can_write_to_config_backup && capable->can_write_to_config_new && capable->can_read_from_config &&
      capable->has_smogrify && capable->smog_version_correct) {
    // check the backend is there, get some system details and prefs
    capable = get_capabilities();
  }

  //FATAL ERRORS

  if (!mainw->foreign) {
    if (!capable->has_perl) {
      startup_message_fatal(lives_strdup(
                              _("\nPerl must be installed and in your path.\n\n"
                                "Please review the README file which came with this package\nbefore running LiVES.\n\n"
                                "Thankyou.\n")));
    }
    if (!capable->has_smogrify) {
      msg = lives_strdup(
              _("\n`smogrify` must be in your path, and be executable\n\n"
                "Please review the README file which came with this package\nbefore running LiVES.\n"));
      startup_message_fatal(msg);
    }
    if (!capable->smog_version_correct) {
      startup_message_fatal
      (lives_strdup(_("\nAn incorrect version of smogrify was found in your path.\n\n"
                      "Please review the README file which came with this package\nbefore running LiVES."
                      "\n\nThankyou.\n")));
    }

    if (!capable->can_read_from_config) {
      msg = lives_strdup_printf(
              _("\nLiVES was unable to read from its configuration file\n%s\n\n"
                "Please check the file permissions for this file and try again.\n"),
              (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
      lives_free(tmp);
      startup_message_fatal(msg);
    }

    if (!capable->can_write_to_config_new || !capable->can_write_to_config_backup || !capable->can_write_to_config) {
      msg = lives_strdup_printf(
              _("\nAn error occurred when writing to the configuration files\n%s*\n\n"
                "Please check the file permissions for this file and directory\nand try again.\n"),
              (tmp2 = ensure_extension((tmp = lives_filename_to_utf8(prefs->configfile,
                                              -1, NULL, NULL, NULL)),
                                       LIVES_FILE_EXT_NEW)));
      lives_free(tmp);
      lives_free(tmp2);
      startup_message_fatal(msg);
    }

    if (!capable->can_write_to_workdir) {
      if (!mainw->has_session_workdir) {
        tmp2 = lives_strdup(_("Please try restarting lives with the -workdir <path_to_workdir> commandline option\n"
                              "Where <path_to_workdir> points to a writeable directory.\n"
                              "You can then change or set this value permanently from within Preferences / Directories"));
      } else tmp2 = lives_strdup("");

      msg = lives_strdup_printf(_("\nLiVES was unable to use the working directory\n%s\n\n%s"),
                                prefs->workdir, tmp2);
      lives_free(tmp2);
      startup_message_fatal(msg);
    }
    if (mainw->error) {
      msg = lives_strdup_printf(_("\nSomething went wrong during startup\n%s"), mainw->msg);
      startup_message_fatal(msg);
    }
  }

  sizint = sizeof(int);
  sizdbl = sizeof(double);
  sizshrt = sizeof(short);

  // TRANSLATORS: text saying "Any", for encoder and output format (as in "does not matter")
  mainw->string_constants[LIVES_STRING_CONSTANT_ANY] = (_("Any"));
  // TRANSLATORS: text saying "Default", (as in "*Default* value")
  mainw->string_constants[LIVES_STRING_CONSTANT_DEFAULT] = (_("Default"));
  // TRANSLATORS: text saying "None", for playback plugin name (as in "none specified")
  mainw->string_constants[LIVES_STRING_CONSTANT_NONE] = (_("None"));
  // TRANSLATORS: text saying "recommended", for plugin names, etc.
  mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED] = (_("recommended"));
  // TRANSLATORS: text saying "disabled", (as in "not enabled")
  mainw->string_constants[LIVES_STRING_CONSTANT_DISABLED] = (_("disabled !"));
  // TRANSLATORS: text saying "**The current layout**", to warn users that the current layout is affected
  mainw->string_constants[LIVES_STRING_CONSTANT_CL] = (_("**The current layout**"));
  // TRANSLATORS: adjective for "Built in" type effects
  mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN] = (_("Builtin"));
  // TRANSLATORS: adjective for "Custom" type effects
  mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM] = (_("Custom"));
  // TRANSLATORS: adjective for "Test" type effects
  mainw->string_constants[LIVES_STRING_CONSTANT_TEST] = (_("Test"));

  // now we can use PREFS properly
  mainw->prefs_cache = cache_file_contents(prefs->configfile);

  capable->uid = get_int64_prefd(PREF_UID, 0);

  if (!capable->uid) {
    capable->uid = gen_unique_id();
    set_int64_pref(PREF_UID, capable->uid);
  }

  prefs->show_dev_opts = get_boolean_prefd(PREF_SHOW_DEVOPTS, FALSE);
  if (mainw->debug) prefs->show_dev_opts = TRUE;

  prefs->back_compat = get_boolean_prefd(PREF_BACK_COMPAT, TRUE);

  future_prefs->vj_mode = get_boolean_prefd(PREF_VJMODE, FALSE);
  if (!ign_opts.ign_vjmode) prefs->vj_mode = future_prefs->vj_mode;

#ifdef GUI_GTK
  if (!prefs->show_dev_opts || prefs->vj_mode) {
    // don't crash on GTK+ fatals
    g_log_set_always_fatal((GLogLevelFlags)0);
  }
#endif

  if (!prefs->vj_mode) {
    check_for_executable(&capable->has_mplayer, EXEC_MPLAYER);
    check_for_executable(&capable->has_mplayer2, EXEC_MPLAYER2);
    check_for_executable(&capable->has_mpv, EXEC_MPV);

    check_for_executable(&capable->has_convert, EXEC_CONVERT);
    check_for_executable(&capable->has_composite, EXEC_COMPOSITE);
    check_for_executable(&capable->has_identify, EXEC_IDENTIFY);

    check_for_executable(&capable->has_gzip, EXEC_GZIP);
    check_for_executable(&capable->has_gdb, EXEC_GDB);
  }

  /// kick off the thread pool ////////////////////////////////
  /// this must be done before we can check the disk status
  future_prefs->nfx_threads = prefs->nfx_threads = get_int_pref(PREF_NFX_THREADS);
  if (future_prefs->nfx_threads <= 0) {
    prefs->nfx_threads = capable->hw.ncpus;
    // set this for the backend, but use a -ve value so we know it wasnt set by the user
    if (prefs->nfx_threads != -future_prefs->nfx_threads)
      set_int_pref(PREF_NFX_THREADS, -prefs->nfx_threads);
    future_prefs->nfx_threads = prefs->nfx_threads;
  }

  // initialise cpu load monitoring
  get_proc_loads(TRUE);
  get_proc_loads(FALSE);

  /// check disk storage status /////////////////////////////////////
  capable->ds_status = LIVES_STORAGE_STATUS_UNKNOWN;

  if (!ign_opts.ign_dscrit)
    prefs->ds_crit_level = (uint64_t)get_int64_prefd(PREF_DS_CRIT_LEVEL, DEF_DS_CRIT_LEVEL);
  if (prefs->ds_crit_level < 0) prefs->ds_crit_level = 0;

  prefs->ds_warn_level = (uint64_t)get_int64_prefd(PREF_DS_WARN_LEVEL, DEF_DS_WARN_LEVEL);
  if (prefs->ds_warn_level < prefs->ds_crit_level) prefs->ds_warn_level = prefs->ds_crit_level;
  mainw->next_ds_warn_level = prefs->ds_warn_level;
  prefs->show_disk_quota = get_boolean_prefd(PREF_SHOW_QUOTA, prefs->show_disk_quota);
  prefs->disk_quota = get_int64_prefd(PREF_DISK_QUOTA, 0);
  if (prefs->disk_quota < 0) prefs->disk_quota = 0;

  prefs->quota_limit = 90.0;

  if (mainw->has_session_workdir) {
    prefs->show_disk_quota = FALSE;
    prefs->disk_quota = 0;
  }

  future_prefs->disk_quota = prefs->disk_quota;

  if (!prefs->vj_mode) {
    /// start a bg thread to get diskspace used
    if (!needs_workdir && prefs->disk_quota && initial_startup_phase == 0)
      mainw->helper_procthreads[PT_LAZY_DSUSED] = disk_monitor_start(prefs->workdir);

    if (!needs_workdir && mainw->next_ds_warn_level > 0) {
      int64_t dsval = disk_monitor_check_result(prefs->workdir);
      if (dsval > 0) capable->ds_used = dsval;
      else dsval = capable->ds_used;
      capable->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval, 0);
      capable->ds_free = dsval;
      if (capable->ds_status == LIVES_STORAGE_STATUS_CRITICAL) {
        tmp = ds_critical_msg(prefs->workdir, &capable->mountpoint, dsval);
        msg = lives_strdup_printf("\n%s\n", tmp);
        lives_free(tmp);
        widget_opts.use_markup = TRUE;
        startup_message_nonfatal(msg);
        widget_opts.use_markup = FALSE;
        lives_free(msg);
      }
    }
  }

  mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;

  prefs->show_msg_area = future_prefs->show_msg_area = get_boolean_prefd(PREF_SHOW_MSGS, TRUE);

  // get some prefs we need to set menu options
  prefs->gui_monitor = -1;

  if (prefs->vj_mode) {
    check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL);
    check_for_executable(&capable->has_xwininfo, EXEC_XWININFO);
    check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL);
  }
  mainw->mgeom = NULL;

  prefs->force_single_monitor = get_boolean_pref(PREF_FORCE_SINGLE_MONITOR);
  mainw->ignore_screen_size = FALSE;

  capable->primary_monitor = 0;

  // sets prefs->screen_scale, capable->nmonitors, mainw->mgeom, prefs->play_monitor, prefs->gui_monitor
  // capable->can_show_msg_area, mainw->old_screen_height, mainw->old_screen_width
  // widget_opts.monitor, widget_opts.screen and various widget_opts sizes
  get_monitors(TRUE);

  // set to allow multiple locking by the same thread
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

  // recursive locks
  pthread_mutex_init(&mainw->abuf_mutex, &mattr);

  // non-recursive
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init(&mainw->abuf_aux_frame_mutex, &mattr);
  pthread_mutex_init(&mainw->fxd_active_mutex, &mattr);
  pthread_mutex_init(&mainw->event_list_mutex, &mattr);
  pthread_mutex_init(&mainw->clip_list_mutex, &mattr);
  pthread_mutex_init(&mainw->vpp_stream_mutex, &mattr);
  pthread_mutex_init(&mainw->cache_buffer_mutex, &mattr);
  pthread_mutex_init(&mainw->audio_filewriteend_mutex, &mattr);
  pthread_mutex_init(&mainw->exit_mutex, &mattr);
  pthread_mutex_init(&mainw->fbuffer_mutex, &mattr);
  pthread_mutex_init(&mainw->avseek_mutex, &mattr);
  pthread_mutex_init(&mainw->alarmlist_mutex, &mattr);
  pthread_mutex_init(&mainw->trcount_mutex, &mattr);
  pthread_mutex_init(&mainw->alock_mutex, &mattr);
  pthread_mutex_init(&mainw->tlthread_mutex, &mattr);

  // conds
  pthread_cond_init(&mainw->avseek_cond, NULL);

  if (prefs->vj_mode)
    prefs->load_rfx_builtin = FALSE;
  else
    prefs->load_rfx_builtin = get_boolean_prefd(PREF_LOAD_RFX_BUILTIN, TRUE);

  mainw->vrfx_update = NULL;

  mainw->kb_timer = -1;

  prefs->sleep_time = 1000;

  prefs->present = FALSE;

  get_string_prefd(PREF_DEFAULT_IMAGE_TYPE, prefs->image_type, 16, LIVES_IMAGE_TYPE_PNG);
  lives_snprintf(prefs->image_ext, 16, "%s",
                 get_image_ext_for_type(lives_image_type_to_img_type(prefs->image_type)));

  /// eye candy
  prefs->extra_colours = get_boolean_prefd(PREF_EXTRA_COLOURS, TRUE);
  prefs->show_button_images = widget_opts.show_button_images =
                                get_boolean_prefd(PREF_SHOW_BUTTON_ICONS, TRUE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  prefs->show_menu_images = get_boolean_prefd(PREF_SHOW_MENU_ICONS, FALSE);
#endif

  prefs->mt_show_ctx = get_boolean_prefd(PREF_MT_SHOW_CTX, TRUE);

  mainw->threaded_dialog = FALSE;
  clear_mainw_msg();

  prefs->autotrans_key = get_int_prefd(PREF_ATRANS_KEY, 8);
  prefs->autotrans_mode = -1;
  prefs->autotrans_amt = -1.;

  info_only = FALSE;
  palette = (_palette *)(lives_malloc(sizeof(_palette)));

  prefs->sepwin_type = future_prefs->sepwin_type = get_int_prefd(PREF_SEPWIN_TYPE, SEPWIN_TYPE_STICKY);

  if (!ign_opts.ign_aplayer) {
    prefs->audio_player = AUD_PLAYER_SOX;
    lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_SOX);
  }

  prefs->open_decorated = TRUE;

#ifdef ENABLE_GIW
  prefs->lamp_buttons = TRUE;
#else
  prefs->lamp_buttons = FALSE;
#endif

  prefs->autoload_subs = get_boolean_prefd(PREF_AUTOLOAD_SUBS, TRUE);
  prefs->show_subtitles = get_boolean_prefd(PREF_SHOW_SUBS, TRUE);

  prefs->pa_restart = get_boolean_prefd(PREF_PARESTART, FALSE);
  get_string_prefd(PREF_PASTARTOPTS, prefs->pa_start_opts, 255, "--high-priority");

  prefs->letterbox = get_boolean_prefd(PREF_LETTERBOX, TRUE);

  future_prefs->letterbox_mt = prefs->letterbox_mt = get_boolean_prefd(PREF_LETTERBOX_MT, TRUE);

  prefs->enc_letterbox = get_boolean_prefd(PREF_LETTERBOX_ENC, TRUE);

  prefs->no_lb_gens = get_boolean_prefd(PREF_NO_LB_GENS, TRUE);

  //////////////////////////
  load_prefs();
  //////////////////////////

  prefs->rte_keys_virtual = get_int_prefd(PREF_RTE_KEYS_VIRTUAL, FX_KEYS_PHYSICAL_EXTRA);
  if (prefs->rte_keys_virtual < 0) prefs->rte_keys_virtual = 0;
  if (prefs->rte_keys_virtual > FX_KEYS_MAX_VIRTUAL) prefs->rte_keys_virtual = FX_KEYS_MAX_VIRTUAL;

  prefs->bigendbug = 0;

#if GTK_CHECK_VERSION(3, 0, 0)
  prefs->funky_widgets = TRUE;
#else
  prefs->funky_widgets = FALSE;
#endif

  prefs->show_splash = TRUE;
  prefs->hide_framebar = FALSE;

  // get some prefs we need to set menu options
  future_prefs->show_recent = prefs->show_recent = get_boolean_prefd(PREF_SHOW_RECENT_FILES, TRUE);

  get_string_pref(PREF_PREFIX_DIR, prefs->prefix_dir, PATH_MAX);

  if (!(*prefs->prefix_dir)) {
    if (strcmp(PREFIX, "NONE")) {
      lives_snprintf(prefs->prefix_dir, PATH_MAX, "%s", PREFIX);
    } else {
      lives_snprintf(prefs->prefix_dir, PATH_MAX, "%s", PREFIX_DEFAULT);
    }
    needs_update = TRUE;
  }

  if (ensure_isdir(prefs->prefix_dir)) needs_update = TRUE;

  if (needs_update) set_string_pref(PREF_PREFIX_DIR, prefs->prefix_dir);

#ifdef GUI_GTK
  iconpix = get_desktop_icon(ICON_DIR(16));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  iconpix = get_desktop_icon(ICON_DIR(22));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  iconpix = get_desktop_icon(ICON_DIR(32));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  iconpix = get_desktop_icon(ICON_DIR(48));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  iconpix = get_desktop_icon(ICON_DIR(64));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  iconpix = get_desktop_icon(ICON_DIR(128));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  iconpix = get_desktop_icon(ICON_DIR(256));
  if (iconpix) capable->app_icons = lives_list_append(capable->app_icons, iconpix);
  gtk_window_set_default_icon_list(capable->app_icons);
#endif
  mainw->first_free_file = 1;

  needs_update = FALSE;

  if (ign_opts.ign_libdir) old_libdir = lives_strdup(prefs->lib_dir);
  get_string_pref(PREF_LIB_DIR, prefs->lib_dir, PATH_MAX);
  if (old_libdir) {
    if (lives_strcmp(old_libdir, prefs->lib_dir)) {
      lives_snprintf(prefs->lib_dir, PATH_MAX, "%s", old_libdir);
      needs_update = TRUE;
    }
    lives_free(old_libdir);
  }

  if (!(*prefs->lib_dir)) {
    lives_snprintf(prefs->lib_dir, PATH_MAX, "%s", LIVES_LIBDIR);
    needs_update = TRUE;
  }
  if (ensure_isdir(prefs->lib_dir)) needs_update = TRUE;
  if (needs_update) set_string_pref(PREF_LIB_DIR, prefs->lib_dir);

  lives_memset(mainw->sepimg_path, 0, 1);
  lives_memset(mainw->frameblank_path, 0, 1);

  mainw->imsep = mainw->imframe = NULL;

  prefs->max_messages = get_int_prefd(PREF_MAX_MSGS, DEF_MAX_MSGS);
  if (prefs->max_messages < mainw->n_messages + 1) {
    free_n_msgs(mainw->n_messages - prefs->max_messages
                + mainw->n_messages > prefs->max_messages ? 1 : 0);
  }

  future_prefs->msg_textsize = prefs->msg_textsize = get_int_prefd(PREF_MSG_TEXTSIZE, DEF_MSG_TEXTSIZE);

  add_messages_first(_("Starting...\n"));

  get_string_prefd(PREF_GUI_THEME, prefs->theme, 64, LIVES_DEF_THEME);

  if (!(*prefs->theme)) {
    lives_snprintf(prefs->theme, 64, LIVES_THEME_NONE);
  }

  lives_snprintf(future_prefs->theme, 64, "%s", prefs->theme);

  if (!set_palette_colours(initial_startup_phase == -1)) {
    lives_snprintf(prefs->theme, 64, LIVES_THEME_NONE);
    set_palette_colours(initial_startup_phase != -1);
  } else if (palette->style & STYLE_1) {
    widget_opts.apply_theme = 1;
  }
  if (!mainw->foreign) {
    if (prefs->startup_phase == 0 && prefs->show_splash) splash_init();
    print_notice();
  }

  capable->session_uid = gen_unique_id();

  if (prefs->show_dev_opts) {
    g_printerr("Today's lucky number is 0X%08lX\n", capable->session_uid);
  }

  g_printerr("Getting hardware details...\n");
  get_machine_dets();
  g_printerr("OK\n");

  g_printerr("Initializing memory block allocators...\n");
  init_memfuncs(1);
  g_printerr("OK\n");

  get_string_pref(PREF_CDPLAY_DEVICE, prefs->cdplay_device, PATH_MAX);

  prefs->warning_mask = (uint64_t)get_int64_prefd(PREF_LIVES_WARNING_MASK, DEF_WARNING_MASK);

  prefs->badfile_intent = get_int_prefd(PREF_BADFILE_INTENT, OBJ_INTENTION_UNKNOWN);

  get_utf8_pref(PREF_INTERFACE_FONT, buff, 256);

  if (*buff && (!*capable->def_fontstring || lives_strcmp(buff, capable->def_fontstring)))
    pref_factory_utf8(PREF_INTERFACE_FONT, buff, FALSE);
  else
    pref_factory_utf8(PREF_INTERFACE_FONT, capable->def_fontstring, FALSE);

#ifdef ENABLE_JACK
  if (!ign_opts.ign_jackopts) {
    prefs->jack_opts = get_int_prefd(PREF_JACK_OPTS, 16);
  }

  prefs->jack_srv_dup = TRUE;

  get_string_pref(PREF_JACK_ACSERVER, prefs->jack_aserver_cname, JACK_PARAM_STRING_MAX);
  get_string_pref(PREF_JACK_ASSERVER, prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX);

  get_string_prefd(PREF_JACK_INPORT_CLIENT, buff, jack_port_name_size(), JACK_SYSTEM_CLIENT);
  prefs->jack_inport_client = lives_strdup(buff);
  future_prefs->jack_inport_client = lives_strdup(buff);
  get_string_prefd(PREF_JACK_OUTPORT_CLIENT, buff, jack_port_name_size(), JACK_SYSTEM_CLIENT);
  prefs->jack_outport_client = lives_strdup(buff);
  future_prefs->jack_outport_client = lives_strdup(buff);
  get_string_prefd(PREF_JACK_AUXPORT_CLIENT, buff, jack_port_name_size(), JACK_SYSTEM_CLIENT);
  prefs->jack_auxport_client = lives_strdup(buff);
  future_prefs->jack_auxport_client = lives_strdup(buff);

  if (!ign_opts.ign_jackcfg) {
    get_string_pref(PREF_JACK_ACONFIG, prefs->jack_aserver_cfg, PATH_MAX);
    lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", prefs->jack_aserver_cfg);
  }

  if (!ign_opts.ign_jackserver) {
    lives_snprintf(future_prefs->jack_aserver_cname, PATH_MAX, "%s", prefs->jack_aserver_cname);
    lives_snprintf(future_prefs->jack_aserver_sname, PATH_MAX, "%s", prefs->jack_aserver_sname);
    if (has_pref(PREF_JACK_LAST_ASERVER)) delete_pref(PREF_JACK_LAST_ASERVER);
    if (has_pref(PREF_JACK_LAST_TSERVER)) delete_pref(PREF_JACK_LAST_TSERVER);
    if (has_pref(PREF_JACK_LAST_ADRIVER)) delete_pref(PREF_JACK_LAST_ADRIVER);
    if (has_pref(PREF_JACK_LAST_TDRIVER)) delete_pref(PREF_JACK_LAST_TDRIVER);
  }

  get_string_pref(PREF_JACK_ADRIVER, jbuff, JACK_PARAM_STRING_MAX);

  if (*jbuff) {
    prefs->jack_adriver = lives_strdup(jbuff);
    future_prefs->jack_adriver = lives_strdup(jbuff);
#ifdef ENABLE_JACK_TRANSPORT
    if (prefs->jack_srv_dup) {
      prefs->jack_tdriver = lives_strdup(jbuff);
      future_prefs->jack_tdriver = lives_strdup(jbuff);
    }
#endif
  } else {
    prefs->jack_adriver = NULL;
    future_prefs->jack_adriver = NULL;
#ifdef ENABLE_JACK_TRANSPORT
    if (prefs->jack_srv_dup) {
      prefs->jack_tdriver = NULL;
      future_prefs->jack_tdriver = NULL;
    }
  }

  if (prefs->jack_srv_dup) {
    lives_snprintf(prefs->jack_tserver_cname, PATH_MAX, "%s", prefs->jack_aserver_cname);
    lives_snprintf(prefs->jack_tserver_sname, PATH_MAX, "%s", prefs->jack_aserver_sname);
    lives_snprintf(prefs->jack_tserver_cfg, PATH_MAX, "%s", prefs->jack_aserver_cfg);
  } else {
    if (!ign_opts.ign_jackserver) {
      get_string_pref(PREF_JACK_TCSERVER, prefs->jack_tserver_cname, JACK_PARAM_STRING_MAX);
      get_string_pref(PREF_JACK_TSSERVER, prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX);
    }
    if (!ign_opts.ign_jackcfg) {
      get_string_pref(PREF_JACK_TCONFIG, prefs->jack_tserver_cfg, PATH_MAX);
    }
    get_string_pref(PREF_JACK_TDRIVER, jbuff, JACK_PARAM_STRING_MAX);
    if (*jbuff) {
      prefs->jack_tdriver = lives_strdup(jbuff);
      future_prefs->jack_tdriver = lives_strdup(jbuff);
    }
  }
  lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", prefs->jack_tserver_cfg);
  if (!ign_opts.ign_jackserver) {
    lives_snprintf(future_prefs->jack_tserver_cname, PATH_MAX, "%s", prefs->jack_tserver_cname);
    lives_snprintf(future_prefs->jack_tserver_sname, PATH_MAX, "%s", prefs->jack_tserver_sname);
  }
#else
    prefs->jack_opts &= ~(JACK_OPTS_TRANSPORT_CLIENT | JACK_OPTS_TRANSPORT_MASTER
                          | JACK_OPTS_START_TSERVER | JACK_OPTS_TIMEBASE_START
                          | JACK_OPTS_TIMEBASE_CLIENT | JACK_OPTS_TIMEBASE_MASTER
                          | JACK_OPTS_TIMEBASE_LSTART | JACK_OPTS_ENABLE_TCLIENT);
#endif
  if (ign_opts.ign_jackserver) {
    prefs->jack_opts |= JACK_INFO_TEMP_NAMES;
  }
  if (ign_opts.ign_jackopts) {
    prefs->jack_opts |= JACK_INFO_TEMP_OPTS;
  }

  future_prefs->jack_opts = prefs->jack_opts;

  if (lives_strcmp(future_prefs->jack_aserver_sname, prefs->jack_aserver_sname)) {
    // server name to start up changed, so we should invalidate the old driver in case this changed
    // but check first if it is equal to the last server set by cmdline opts and use its driver
    // otherwise we would end up constantly prompting for the driver for the same options
    get_string_pref(PREF_JACK_LAST_ASERVER, jbuff, JACK_PARAM_STRING_MAX);
    if (*jbuff && !lives_strcmp(future_prefs->jack_aserver_sname, jbuff)) {
      get_string_pref(PREF_JACK_LAST_ADRIVER, jbuff, JACK_PARAM_STRING_MAX);
      prefs->jack_adriver = lives_strdup_free(prefs->jack_adriver, jbuff);
      future_prefs->jack_adriver = lives_strdup_free(future_prefs->jack_adriver, jbuff);
    }
    // otherwise prompt for a new driver
    else lives_freep((void **)&future_prefs->jack_adriver);
  }
  if (lives_strcmp(future_prefs->jack_tserver_sname, prefs->jack_tserver_sname)) {
    // same for transport server
    get_string_pref(PREF_JACK_LAST_TSERVER, jbuff, JACK_PARAM_STRING_MAX);
    if (*jbuff && !lives_strcmp(future_prefs->jack_tserver_sname, jbuff)) {
      get_string_pref(PREF_JACK_LAST_ADRIVER, jbuff, JACK_PARAM_STRING_MAX);
      prefs->jack_tdriver = lives_strdup_free(prefs->jack_tdriver, jbuff);
      future_prefs->jack_tdriver = lives_strdup_free(future_prefs->jack_tdriver, jbuff);
    } else lives_freep((void **)&future_prefs->jack_tdriver);
  }

#endif

#ifdef GUI_GTK
  if (!has_pref(PREF_SHOW_TOOLTIPS)) {
    lives_widget_object_get(gtk_settings_get_default(), "gtk-enable-tooltips", &prefs->show_tooltips);
  } else
#endif
    prefs->show_tooltips = get_boolean_prefd(PREF_SHOW_TOOLTIPS, TRUE);

  prefs->show_urgency_msgs = get_boolean_prefd(PREF_SHOW_URGENCY, TRUE);
  prefs->show_overlay_msgs = get_boolean_prefd(PREF_SHOW_OVERLAY_MSGS, TRUE);

  prefs->allow_easing = get_boolean_prefd(PREF_ALLOW_EASING, TRUE);

  prefs->render_overlay = prefs->show_dev_opts;

  if (prefs->show_dev_opts) {
    prefs->btgamma = get_boolean_prefd(PREF_BTGAMMA, FALSE);
  }

  for (i = 0; i < MAX_FX_CANDIDATE_TYPES; i++) {
    mainw->fx_candidates[i].delegate = -1;
    mainw->fx_candidates[i].list = NULL;
    mainw->fx_candidates[i].func = 0l;
    mainw->fx_candidates[i].rfx = NULL;
  }

  prefs->volume = (float)get_double_prefd(PREF_MASTER_VOLUME, 0.72);
  future_prefs->volume = prefs->volume;
  mainw->uflow_count = 0;

  prefs->open_maximised = get_boolean_prefd(PREF_OPEN_MAXIMISED, TRUE);

  for (i = 0; i < MAX_EXT_CNTL; i++) mainw->ext_cntl[i] = FALSE;

  prefs->omc_dev_opts = get_int_prefd(PREF_OMC_DEV_OPTS, 3);

  get_utf8_pref(PREF_OMC_JS_FNAME, prefs->omc_js_fname, PATH_MAX);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  if (!*prefs->omc_js_fname) {
    const char *tmp = get_js_filename();
    if (tmp) {
      lives_snprintf(prefs->omc_js_fname, PATH_MAX, "%s", tmp);
    }
  }
#endif
#endif

  get_utf8_pref(PREF_OMC_MIDI_FNAME, prefs->omc_midi_fname, PATH_MAX);

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  if (!*prefs->omc_midi_fname) {
    const char *tmp = get_midi_filename();
    if (tmp) {
      lives_snprintf(prefs->omc_midi_fname, PATH_MAX, "%s", tmp);
    }
  }
#endif
#endif

#ifdef ALSA_MIDI
  prefs->use_alsa_midi = TRUE;
  prefs->alsa_midi_dummy = FALSE;
  mainw->seq_handle = NULL;

  if (prefs->omc_dev_opts & OMC_DEV_FORCE_RAW_MIDI) prefs->use_alsa_midi = FALSE;
  if (prefs->omc_dev_opts & OMC_DEV_MIDI_DUMMY) prefs->alsa_midi_dummy = TRUE;
#endif

  prefs->midi_rcv_channel = get_int_prefd(PREF_MIDI_RCV_CHANNEL, MIDI_OMNI);

  mainw->ccpd_with_sound = TRUE;
  mainw->loop = TRUE;

  if (prefs->vj_mode) {
    auto_recover = TRUE;
    mainw->loop_cont = TRUE;
    mainw->ccpd_with_sound = FALSE;
    prefs->sepwin_type = SEPWIN_TYPE_NON_STICKY;
    prefs->letterbox = FALSE;
    prefs->autoload_subs = FALSE;
    prefs->use_screen_gamma = TRUE;
    prefs->screen_gamma = 1.5;
  }

#ifdef GUI_GTK
  mainw->target_table = target_table;
#endif

  prefs->show_asrc = get_boolean_prefd(PREF_SHOW_ASRC, TRUE);
  prefs->hfbwnp = get_boolean_prefd(PREF_HFBWNP, FALSE);

  mainw->next_free_alarm = 0;

  for (i = 0; i < LIVES_MAX_ALARMS; i++) {
    mainw->alarms[i].lastcheck = 0;
  }

  if (lives_ascii_strcasecmp(prefs->theme, future_prefs->theme)) return TRUE;
  return FALSE;
}




LIVES_LOCAL_INLINE void outp_help(LiVESTextBuffer * textbuf, const char *fmt, ...) {
  va_list xargs;
  va_start(xargs, fmt);
  if (!textbuf) vfprintf(stderr, fmt, xargs);
  else {
    char *text = lives_strdup_vprintf(fmt, xargs);
    lives_text_buffer_insert_at_cursor(textbuf, text, -1);
    lives_free(text);
  }
  va_end(xargs);
}


void print_opthelp(LiVESTextBuffer * textbuf, const char *extracmds_file1, const char *extracmds_file2) {
  char *tmp;
  if (!textbuf) print_notice();
  outp_help(textbuf, _("\nStartup syntax is: %s [OPTS] [filename [start_time] [frames]]\n"), capable->myname);
  if (!textbuf) {
    outp_help(textbuf, _("\nIf the file %s exists, then the first line in it will be prepended to the commandline\n"
                         "exactly as if it had been typed in by the user. If that file does not exist, then the file %s\n"
                         "will be read in the same fashion\n\n"), extracmds_file1, extracmds_file2);
  }
  outp_help(textbuf, "%s", _("filename is the name of a media file or backup file to import at startup\n"));
  outp_help(textbuf, "%s", _("start_time : filename start time in seconds\n"));
  outp_help(textbuf, "%s", _("frames : maximum number of frames to open\n"));
  outp_help(textbuf, "%s", "\n");
  outp_help(textbuf, "%s", _("OPTS can be:\n"));
  outp_help(textbuf, "%s", _("-help | --help \t\t\t: print this help text on stderr and exit\n"));
  outp_help(textbuf, "%s", _("-version | --version\t\t: print the LiVES version on stderr and exit\n"));
  outp_help(textbuf, "%s", _("-workdir <workdir>\t\t: specify the working directory for the session, "
                             "overriding any value set in preferences\n"));
  outp_help(textbuf, "%s", _("\t\t\t\t\t(disables any disk quota checking)\n"));
  outp_help(textbuf, "%s", _("-configfile <path_to_file>\t: override the default configuration file for the session\n"));
  tmp = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_DIR, LIVES_DEF_CONFIG_FILE, NULL);
  outp_help(textbuf, _("\t\t\t\t\t(default is %s)\n"), tmp);
  lives_free(tmp);

  tmp = get_localsharedir(LIVES_DIR_LITERAL);
  outp_help(textbuf, "%s", _("-configdatadir <dir>\t\t: override the default configuration data directory for the session\n"));
  outp_help(textbuf, _("\t\t\t\t\t(default is %s\n"), tmp);
  lives_free(tmp);

  outp_help(textbuf, "%s",
            _("-dscrit <bytes>\t\t\t: temporarily sets the free disk space critical level for workdir to <bytes>\n"));
  outp_help(textbuf, "%s", _("\t\t\t\t\t(intended to allow correction of erroneous values within the app; "
                             "<= 0 disables checks)\n"));
  outp_help(textbuf, "%s", _("-set <setname>\t\t\t: autoload clip set <setname>\n"));
  outp_help(textbuf, "%s", _("-noset\t\t\t\t: do not reload any clip set on startup (overrides -set)\n"));
  outp_help(textbuf, "%s", _("-layout <layout_name>\t\t: autoload multitrack layout <layout_name> (if successful, "
                             "overrides -startup-ce)\n"));
  outp_help(textbuf, "%s", _("-nolayout\t\t\t: do not reload any multitrack layout on startup (overrides -layout)\n"));
  outp_help(textbuf, "%s",
            _("-norecover | -noautorecover\t: inhibits loading of crash recovery files (overrides -recover / -autorecover)\n"));
  outp_help(textbuf, "%s",
            _("-recover | -autorecover\t\t: force reloading of any crash recovery files (may override -noset and -nolayout)\n"));
  outp_help(textbuf, "%s", _("-nogui\t\t\t\t: do not show the gui (still shows the play window when active)\n"));
  outp_help(textbuf, "%s", _("-nosplash\t\t\t: do not show the splash window\n"));
  outp_help(textbuf, "%s",
            _("-noplaywin\t\t\t: do not show the play window (still shows the internal player; intended for remote streaming)\n"));
  outp_help(textbuf, "%s",
            _("-noninteractive\t\t\t: disable menu interactivity (intended for scripting applications, e.g liblives)\n"));
  outp_help(textbuf, "%s", _("-startup-ce\t\t\t: start in clip editor mode (overrides -startup-mt)\n"));
  outp_help(textbuf, "%s", _("-startup-mt\t\t\t: start in multitrack mode\n"));
  outp_help(textbuf, "%s", _("-vjmode\t\t\t\t: start in VJ mode (implicitly sets -startup-ce -autorecover "
                             "-nolayout -asource external)\n"));
  outp_help(textbuf,
            _("-fxmodesmax <n>\t\t\t: allow <n> modes per effect key (overrides any value set in preferences); range 1 - %x)\n"),
            FX_MODES_MAX);
#ifdef ENABLE_OSC
  outp_help(textbuf,  _("-oscstart <port>\t\t: start OSC listener on UDP port <port> (default is %d)\n"), DEF_OSC_LISTEN_PORT);
  outp_help(textbuf, "%s",
            _("-nooscstart\t\t\t: do not start the OSC listener (the default, unless set in preferences)\n"));
#endif
  outp_help(textbuf, "%s",
            _("-asource <source>\t\t: set the initial audio source (<source> can be 'internal' or 'external')\n"));
  outp_help(textbuf, _("\t\t\t\t\t(only valid for %s and %s players)\n"), AUDIO_PLAYER_JACK, AUDIO_PLAYER_PULSE_AUDIO);
  outp_help(textbuf, "%s", _("-aplayer <ap>\t\t\t: start with the selected audio player (<ap> can be: "));
#ifdef HAVE_PULSE_AUDIO
  outp_help(textbuf, "'%s'", AUDIO_PLAYER_PULSE);
#endif
#ifdef ENABLE_JACK
#ifdef HAVE_PULSE_AUDIO
  outp_help(textbuf, ", "); // comma after pulse
#endif
  outp_help(textbuf, "'%s'", AUDIO_PLAYER_JACK);
  if (capable->has_sox_play) lives_printerr(", '%s'", AUDIO_PLAYER_SOX); // comma after jack
  outp_help(textbuf, " or '%s')\n", AUDIO_PLAYER_NONE);
  outp_help(textbuf, "%s",
            _("\n-jackopts <opts>\t\t: opts is a bitmap of jackd startup / playback options\n"
              "\t\t\t\t\t\t(audio options are ignored if audio player is not jack)\n"
              "\tUseful combinations include:\t\t    0 - do not start any servers; only connect audio; no transport client\n"
              "\t\t\t\t\t   16 - start audio server if connection fails; no transport client\n"
              "\t\t\t\t\t 1024 - create audio and transport clients; only connect\n"
              "\t\t\t\t\t 1028 - create audio and transport clients; transport client may start a server\n"
              "\t\t\t\t\t\t\t\tif it fails to connect,\n"
             ));
  outp_help(textbuf, "%s",
            _("-jackserver <server_name>\t: temporarily sets the jackd server for all connection / startup attemps\n"));
  outp_help(textbuf, _("\t\t\t\t\tif <server_name> is ommitted then LiVES will use the default server name:-\n"
                       "\t\t\t\t\teither the value of $%s or '%s' if that enviromnent variable is unset\n\n"),
            JACK_DEFAULT_SERVER, JACK_DEFAULT_SERVER_NAME);
  outp_help(textbuf, "%s",
            _("-jackscript <script_file>\t: temporarily sets the path to the jack script file to run if a connection attempt fails\n"));
  jack_get_cfg_file(FALSE, &tmp);
  outp_help(textbuf, _("\t\t\t\t\tE.g: -jackscript %s\n\n"), tmp);
  lives_free(tmp);

#else // no jack
  if (capable->has_sox_play) {
#ifdef HAVE_PULSE_AUDIO
    outp_help(textbuf, ", "); // comma after pulse
#endif
    outp_help(textbuf, _("'%s' or "), AUDIO_PLAYER_SOX);
  }
#ifdef HAVE_PULSE_AUDIO
  else outp_help(textbuf, "%s", _(" or ")); // no sox, 'or' after pulse
#endif
  outp_help(textbuf, "'%s')\n", AUDIO_PLAYER_NONE);
#endif
  outp_help(textbuf, "%s", _("-devicemap <mapname>\t\t: autoload devicemap <mapname> (for MIDI / joystick control)\n"));
  outp_help(textbuf, "%s", _("-vppdefaults <file>\t\t: load defaults for video playback plugin from <file>\n"
                             "\t\t\t\t\t(Note: only affects the plugin settings, not the plugin type)\n"));
#ifdef HAVE_YUV4MPEG
  outp_help(textbuf, "%s",  _("-yuvin <fifo>\t\t\t: autoplay yuv4mpeg from stream <fifo> on startup\n"));
  outp_help(textbuf, "%s", _("\t\t\t\t\t(only valid in clip edit startup mode)\n"));
#endif
  outp_help(textbuf, "%s", _("-debug\t\t\t\t: try to debug crashes (requires 'gdb' to be installed)\n"));
  outp_help(textbuf, "%s", "\n");
}

//// things to do - on startup
#ifdef HAVE_YUV4MPEG
static boolean open_yuv4m_startup(livespointer data) {
  on_open_yuv4m_activate(NULL, data);
  return FALSE;
}
#endif


///////////////////////////////// TODO - move idle functions into another file //////////////////////////////////////

boolean render_choice_idle(livespointer data) {
  static boolean norecurse = FALSE;
  boolean rec_recovered = FALSE;
  boolean is_recovery = LIVES_POINTER_TO_INT(data);
  if (norecurse) return FALSE;
  if (mainw->noswitch) return TRUE;
  //if (mainw->pre_src_file >= 0) return TRUE;
  norecurse = TRUE;
  if (!is_recovery || mt_load_recovery_layout(NULL)) {
    if (mainw->event_list) {
      if (mainw->multitrack) {
        /// exit multitrack, backup mainw->event_as it will get set to NULL
        weed_plant_t *backup_elist = mainw->event_list;
        multitrack_delete(mainw->multitrack, FALSE);
        mainw->event_list = backup_elist;
      }

      main_thread_execute(deal_with_render_choice, 0, NULL, "b", is_recovery);
      //deal_with_render_choice(is_recovery);
      if (is_recovery && mainw->multitrack) rec_recovered = TRUE;
    }
  }
  if (is_recovery) mainw->recording_recovered = rec_recovered;
  norecurse = FALSE;
  return FALSE;
}


boolean lazy_startup_checks(void) {
  static boolean checked_trash = FALSE;
  static boolean mwshown = FALSE;
  static boolean dqshown = FALSE;
  static boolean tlshown = FALSE;
  static boolean extra_caps = FALSE;
  static boolean is_first = TRUE;

  if (LIVES_IS_PLAYING) {
    dqshown = mwshown = tlshown = TRUE;
    return FALSE;
  }

  if (is_first) {
    if (prefs->open_maximised && prefs->show_gui)
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    is_first = FALSE;
    return TRUE;
  }

  if (!tlshown) {
    //g_print("val is $d\n", check_snap("youtube-dl"));
    if (!mainw->multitrack) redraw_timeline(mainw->current_file);
    tlshown = TRUE;
    return TRUE;
  }

  if (prefs->vj_mode) {
    resize(1.);
    if (prefs->open_maximised) {
      if (!mainw->hdrbar) {
        int bx, by;
        get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
        if (abs(by) > MENU_HIDE_LIM)
          lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
      }
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    }
    return FALSE;
  }

  if (mainw->dsu_widget) return TRUE;

  if (!checked_trash) {
    if (prefs->autoclean) {
      char *com = lives_strdup_printf("%s empty_trash . general %s", prefs->backend, TRASH_NAME);
      lives_system(com, FALSE);
      lives_free(com);
    }
    checked_trash = TRUE;
  }
  if (!dqshown) {
    boolean do_show_quota = prefs->show_disk_quota;
    if (ran_ds_dlg) do_show_quota = FALSE;
    dqshown = TRUE;
    if (mainw->helper_procthreads[PT_LAZY_DSUSED]) {
      if (disk_monitor_running(prefs->workdir)) {
        int64_t dsval = capable->ds_used = disk_monitor_check_result(prefs->workdir);
        capable->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval, 0);
        capable->ds_free = dsval;
        if (capable->ds_used < 0)
          capable->ds_used = disk_monitor_check_result(prefs->workdir);
        if (!prefs->disk_quota && (capable->ds_status == LIVES_STORAGE_STATUS_NORMAL
                                   || capable->ds_status == LIVES_STORAGE_STATUS_UNKNOWN)) {
          if (capable->ds_used < 0) disk_monitor_forget();
        } else {
          if (capable->ds_used < 0) {
            capable->ds_used = disk_monitor_wait_result(prefs->workdir, LIVES_DEFAULT_TIMEOUT);
          }
        }
      }
      mainw->helper_procthreads[PT_LAZY_DSUSED] = NULL;
      if (capable->ds_used > prefs->disk_quota * .9 || (capable->ds_status != LIVES_STORAGE_STATUS_NORMAL
          && capable->ds_status != LIVES_STORAGE_STATUS_UNKNOWN)) {
        if (capable->ds_used > prefs->disk_quota * .9
            && (capable->ds_status == LIVES_STORAGE_STATUS_NORMAL
                || capable->ds_status == LIVES_STORAGE_STATUS_UNKNOWN)) {
          capable->ds_status = LIVES_STORAGE_STATUS_OVER_QUOTA;
        }
        do_show_quota = TRUE;
      }
    }
    if (do_show_quota) {
      run_diskspace_dialog(NULL);
      return TRUE;
    }
  }

  if (!mwshown) {
    mwshown = TRUE;
    if (prefs->show_msgs_on_startup) do_messages_window(TRUE);
  }

  if (!extra_caps) {
    extra_caps = TRUE;
    capable->boot_time = get_cpu_load(-1);
  }

  if (mainw->ldg_menuitem) {
    if (!RFX_LOADED) return TRUE;
    if (mainw->helper_procthreads[PT_LAZY_RFX]) {
      if (!lives_proc_thread_join_boolean(mainw->helper_procthreads[PT_LAZY_RFX])) {
        lives_proc_thread_free(mainw->helper_procthreads[PT_LAZY_RFX]);
        mainw->helper_procthreads[PT_LAZY_RFX] = NULL;
        if (capable->has_plugins_libdir == UNCHECKED) {
          if (check_for_plugins(prefs->lib_dir, FALSE)) {
            mainw->helper_procthreads[PT_LAZY_RFX] =
              lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                       (lives_funcptr_t)add_rfx_effects, WEED_SEED_BOOLEAN, "i", RFX_STATUS_ANY);
            return TRUE;
          }
        }
      } else {
        lives_proc_thread_free(mainw->helper_procthreads[PT_LAZY_RFX]);
        mainw->helper_procthreads[PT_LAZY_RFX] = NULL;
        lives_widget_destroy(mainw->ldg_menuitem);
        mainw->ldg_menuitem = NULL;
        add_rfx_effects2(RFX_STATUS_ANY);
        if (LIVES_IS_SENSITIZED) sensitize(); // call fn again to sens. new menu entries
      }
    }
  }

  return FALSE;
}


boolean resize_message_area(livespointer data) {
  // workaround because the window manager will resize the window asynchronously
  static boolean isfirst = TRUE;

  if (data) isfirst = TRUE;

  if (!prefs->show_gui || LIVES_IS_PLAYING || mainw->is_processing || mainw->is_rendering || !prefs->show_msg_area) {
    mainw->assumed_height = mainw->assumed_width = -1;
    mainw->idlemax = 0;
    return FALSE;
  }

  if (mainw->idlemax-- == DEF_IDLE_MAX) mainw->msg_area_configed = FALSE;

  /* if (mainw->idlemax == DEF_IDLE_MAX / 2 && prefs->open_maximised) { */
  /*   if (!mainw->hdrbar) { */
  /*     int bx, by; */
  /*     get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by); */
  /*     if (by > MENU_HIDE_LIM) */
  /* 	lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE); */
  /*   } */
  /*   lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET)); */
  /*   mainw->assumed_height = mainw->assumed_width = -1; */
  /*   return TRUE; */
  /* } */

  if (mainw->msg_area_configed) mainw->idlemax = 0;

  if (mainw->idlemax > 0 && mainw->assumed_height != -1 &&
      mainw->assumed_height != lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET)) return TRUE;
  if (mainw->idlemax > 0 && lives_widget_get_allocation_height(mainw->end_image) != mainw->ce_frame_height) return TRUE;

  mainw->idlemax = 0;
  mainw->assumed_height = mainw->assumed_width = -1;
  msg_area_scroll(LIVES_ADJUSTMENT(mainw->msg_adj), mainw->msg_area);
  //#if !GTK_CHECK_VERSION(3, 0, 0)
  msg_area_config(mainw->msg_area);
  //#endif
  if (isfirst) {
    //lives_widget_set_vexpand(mainw->msg_area, TRUE);
    if (prefs->open_maximised && prefs->show_gui) {
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    }
    if (!CURRENT_CLIP_IS_VALID) {
      d_print("");
    }
    msg_area_scroll_to_end(mainw->msg_area, mainw->msg_adj);
    lives_widget_queue_draw_if_visible(mainw->msg_area);
    isfirst = FALSE;
  }
  resize(1.);
  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
  return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
static boolean got_files = FALSE;
static boolean lives_startup2(livespointer data);
static boolean lives_startup(livespointer data) {
  // this is run in an idlefunc
  char *tmp, *msg;

  // check the working directory
  if (needs_workdir) {
    // get initial workdir
    if (!do_workdir_query()) {
      lives_exit(0);
    }
    if (prefs->startup_phase == 1) {
      prefs->startup_phase = 2;
      set_int_pref(PREF_STARTUP_PHASE, 2);
    }
  }

  // will advance startup phase to 3, unless skipped
  if (prefs->startup_phase > 0 && prefs->startup_phase < 3) {
    if (!do_setup_tests(FALSE)) {
      lives_exit(0);
    }

    if (prefs->startup_phase == 3) set_int_pref(PREF_STARTUP_PHASE, prefs->startup_phase);

    // we can show this now
    if (prefs->show_splash) splash_init();
  }

  if (newconfigfile || prefs->startup_phase == 3 || prefs->startup_phase == 2) {
    /// CREATE prefs->config_datadir, and default items inside it
    build_init_config(prefs->config_datadir, ign_opts.ign_config_datadir);
  }

  get_string_pref(PREF_VID_PLAYBACK_PLUGIN, buff, 256);

  if (*buff && lives_strcmp(buff, "(null)") && lives_strcmp(buff, "none")
      && lives_strcmp(buff, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    mainw->vpp = open_vid_playback_plugin(buff, TRUE);
  }
#ifdef DEFAULT_VPP_NAME
  else if (prefs->startup_phase && prefs->startup_phase <= 3) {
    mainw->vpp = open_vid_playback_plugin(DEFAULT_VPP_NAME, TRUE);
    if (mainw->vpp) {
      lives_snprintf(future_prefs->vpp_name, 64, "%s", mainw->vpp->soname);
      set_string_pref(PREF_VID_PLAYBACK_PLUGIN, mainw->vpp->soname);
    }
  }
#endif

  if (!ign_opts.ign_aplayer) {
    get_string_pref(PREF_AUDIO_PLAYER, buff, 256);
    if (!strcmp(buff, AUDIO_PLAYER_NONE)) {
      prefs->audio_player = AUD_PLAYER_NONE;  ///< experimental
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_NONE);
    } else if (!strcmp(buff, AUDIO_PLAYER_SOX)) {
      prefs->audio_player = AUD_PLAYER_SOX;
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_SOX);
    } else if (!strcmp(buff, AUDIO_PLAYER_JACK)) {
      prefs->audio_player = AUD_PLAYER_JACK;
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_JACK);
    } else if (!strcmp(buff, AUDIO_PLAYER_PULSE) || !strcmp(buff, AUDIO_PLAYER_PULSE_AUDIO)) {
      prefs->audio_player = AUD_PLAYER_PULSE;
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_PULSE);
    }
  } else {
    set_string_pref(PREF_AUDIO_PLAYER, prefs->aplayer);
  }

#ifdef HAVE_PULSE_AUDIO
  if ((prefs->startup_phase == 1 || prefs->startup_phase == -1) && capable->has_pulse_audio) {
    if (prefs->pa_restart) {
      char *com = lives_strdup_printf("%s -k %s", EXEC_PULSEAUDIO, prefs->pa_start_opts);
      lives_system(com, TRUE);
      lives_free(com);
    }
    prefs->audio_player = AUD_PLAYER_PULSE;
    lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_PULSE);
    set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
  } else {
#endif
#ifdef ENABLE_JACK
    if ((prefs->startup_phase == 1 || prefs->startup_phase == -1) && capable->has_jackd && prefs->audio_player == -1) {
      prefs->audio_player = AUD_PLAYER_JACK;
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_JACK);
      set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
    }
#endif
#ifdef HAVE_PULSE_AUDIO
  }
#endif

  if (!ign_opts.ign_asource) {
    if (prefs->vj_mode)
      prefs->audio_src = AUDIO_SRC_EXT;
    else
      prefs->audio_src = get_int_prefd(PREF_AUDIO_SRC, AUDIO_SRC_INT);
  }

  if (!((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) || (prefs->audio_player == AUD_PLAYER_PULSE &&
        capable->has_pulse_audio)) && prefs->audio_src == AUDIO_SRC_EXT) {
    prefs->audio_src = AUDIO_SRC_INT;
    set_int_pref(PREF_AUDIO_SRC, prefs->audio_src);
  }

  future_prefs->audio_src = prefs->audio_src;

  splash_msg(_("Starting GUI..."), SPLASH_LEVEL_BEGIN);
  LIVES_MAIN_WINDOW_WIDGET = NULL;

  create_LiVES();

  if (prefs->open_maximised && prefs->show_gui) {
    if (!mainw->hdrbar) {
      int bx, by;
      get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
      if (abs(by) > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  // needed to avoid priv->pulse2 > priv->pulse1 gtk error
  lives_widget_context_update();

  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_context_update();

  if (theme_error && !mainw->foreign) {
    // non-fatal errors
    char *old_prefix_dir = lives_strdup(prefs->prefix_dir);
    char *themesdir = lives_build_path((tmp = lives_filename_to_utf8(prefs->prefix_dir, -1, NULL, NULL, NULL)), THEME_DIR, NULL);
    msg = lives_strdup_printf(
            _("\n\nThe theme you requested (%s) could not be located.\n"
              "Please make sure you have the themes installed in\n%s.\n"),
            future_prefs->theme, themesdir);
    lives_free(themesdir);
    startup_message_nonfatal_dismissable(msg, WARN_MASK_CHECK_PREFIX);
    lives_free(msg);
    if (lives_strcmp(prefs->prefix_dir, old_prefix_dir)) {
      lives_free(old_prefix_dir);
      lives_snprintf(prefs->theme, 64, "%s", future_prefs->theme);
      set_palette_colours(TRUE);
    }
    lives_free(old_prefix_dir);
  }

  if (!lives_init(&ign_opts)) return FALSE;

  // non-fatal errors

  if (!mainw->foreign) {
    if (*capable->startup_msg) {
      if (info_only) startup_message_info(capable->startup_msg);
      else startup_message_nonfatal(capable->startup_msg);
    } else {
      if (!prefs->vj_mode) {
        if (!capable->has_mplayer && !capable->has_mplayer2 && !capable->has_mpv
            && !(prefs->warning_mask & WARN_MASK_NO_MPLAYER)) {
          startup_message_nonfatal_dismissable(
            _("\nLiVES was unable to locate 'mplayer','mplayer2' or 'mpv'. "
              "You may wish to install one of these to use LiVES more fully.\n"),
            WARN_MASK_NO_MPLAYER);
        }
        if (!capable->has_convert) {
          startup_message_nonfatal_dismissable(
            _("\nLiVES was unable to locate 'convert'. "
              "You should install convert and image-magick "
              "if you want to use rendered effects.\n"),
            WARN_MASK_NO_MPLAYER);
        }
        if (!capable->has_composite) {
          startup_message_nonfatal_dismissable(
            _("\nLiVES was unable to locate 'composite'. "
              "You should install composite and image-magick "
              "if you want to use the merge function.\n"),
            WARN_MASK_NO_MPLAYER);
        }
        if (!capable->has_sox_sox) {
          startup_message_nonfatal_dismissable(
            _("\nLiVES was unable to locate 'sox'. Some audio features may not work. "
              "You should install 'sox'.\n"),
            WARN_MASK_NO_MPLAYER);
        }

        if (prefs->startup_phase || prefs->startup_phase > 1)
          startup_message_nonfatal_dismissable(msg, WARN_MASK_CHECK_PLUGINS);

        if (mainw->next_ds_warn_level > 0) {
          if (capable->ds_status == LIVES_STORAGE_STATUS_WARNING) {
            uint64_t curr_ds_warn = mainw->next_ds_warn_level;
            mainw->next_ds_warn_level >>= 1;
            if (mainw->next_ds_warn_level > (capable->ds_free >> 1)) mainw->next_ds_warn_level = capable->ds_free >> 1;
            if (mainw->next_ds_warn_level < prefs->ds_crit_level) mainw->next_ds_warn_level = prefs->ds_crit_level;
            tmp = ds_warning_msg(prefs->workdir, &capable->mountpoint, capable->ds_free, curr_ds_warn, mainw->next_ds_warn_level);
            msg = lives_strdup_printf("\n%s\n", tmp);
            lives_free(tmp);
            startup_message_nonfatal(msg);
            lives_free(msg);
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*
  } else {
    // capture mode
    mainw->foreign_key = atoi(zargv[2]);

#if GTK_CHECK_VERSION(3, 0, 0) || defined GUI_QT
    mainw->foreign_id = (Window)atoi(zargv[3]);
#else
    mainw->foreign_id = (GdkNativeWindow)atoi(zargv[3]);
#endif

    mainw->foreign_width = atoi(zargv[4]);
    mainw->foreign_height = atoi(zargv[5]);
    lives_snprintf(prefs->image_ext, 16, "%s", zargv[6]);
    lives_snprintf(prefs->image_type, 16, "%s", image_ext_to_lives_image_type(prefs->image_ext));
    mainw->foreign_bpp = atoi(zargv[7]);
    mainw->rec_vid_frames = atoi(zargv[8]);
    mainw->rec_fps = lives_strtod(zargv[9]);
    mainw->rec_arate = atoi(zargv[10]);
    mainw->rec_asamps = atoi(zargv[11]);
    mainw->rec_achans = atoi(zargv[12]);
    mainw->rec_signed_endian = atoi(zargv[13]);

    if (zargc > 14) {
      mainw->foreign_visual = lives_strdup(zargv[14]);
      if (!strcmp(mainw->foreign_visual, " (null)")) {
        lives_free(mainw->foreign_visual);
        mainw->foreign_visual = NULL;
      }
    }

#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd && mainw->rec_achans > 0) {
      jack_audio_read_init();
      //jack_rec_audio_to_clip(-1, -1, RECA_EXTERNAL);
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && capable->has_pulse_audio && mainw->rec_achans > 0) {
      lives_pulse_init(0);
      pulse_audio_read_init();
    }
#endif

    lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
    lives_widget_context_update();
    mainw->go_away = FALSE;
    on_capture2_activate();  // exits
  }

  //#define NOTTY
#ifdef NOTTY
  if (!mainw->foreign) {
    capable->xstdout = dup(STDOUT_FILENO);
    close(STDOUT_FILENO);
  }
#endif

  if (mainw->prefs_cache) cached_list_free(&mainw->prefs_cache);

  if (!prefs->show_gui) lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);

  // splash_end() will start up multitrack if in STARTUP_MT mode
  if (*start_file && strcmp(start_file, "-")) {
    splash_end();
    deduce_file(start_file, start, end);
    got_files = TRUE;
  } else {
    set_main_title(NULL, 0);
    splash_end();
  }

  if (!mainw->lives_shown) show_lives();

  if (!strcmp(buff, AUDIO_PLAYER_SOX)) {
    switch_aud_to_sox(FALSE);
  }
  if (!strcmp(buff, AUDIO_PLAYER_NONE)) {
    // still experimental
    switch_aud_to_none(FALSE);
  }

  lives_idle_add_simple(lives_startup2, NULL);
  return FALSE;
}


static boolean lazy_start_fin(void *obj, void *data) {
  lives_proc_thread_t lpt = (lives_proc_thread_t)data;
  boolean bret = lives_proc_thread_join_boolean(lpt);
  if (!bret) {
    // it returned FALSE, we can free this. The destructor will
    // also set mainw->lazy_starter to NULL
    lives_proc_thread_free(lpt);
    return FALSE;
  }
  return TRUE;
}


static boolean lives_startup2(livespointer data) {
  char *ustr;
  boolean layout_recovered = FALSE;

#ifndef VALGRIND_ON
  if (mainw->helper_procthreads[PT_CUSTOM_COLOURS]) {
    if (lives_proc_thread_check_finished(mainw->helper_procthreads[PT_CUSTOM_COLOURS])) {
      double cpvar = lives_proc_thread_join_double(mainw->helper_procthreads[PT_CUSTOM_COLOURS]);
      lives_proc_thread_free(mainw->helper_procthreads[PT_CUSTOM_COLOURS]);
      if (prefs->cptime <= 0. && cpvar < MAX_CPICK_VAR) {
        prefs->cptime *= 1.1 + .2;
        set_double_pref(PREF_CPICK_TIME, prefs->cptime);
      }
      set_double_pref(PREF_CPICK_VAR, fabs(cpvar));
    } else {
      prefs->cptime =
        (double)(lives_get_current_ticks()
                 - lives_proc_thread_get_start_ticks(mainw->helper_procthreads[PT_CUSTOM_COLOURS]))
        / TICKS_PER_SECOND_DBL * .9;
      set_double_pref(PREF_CPICK_TIME, prefs->cptime);
      set_double_pref(PREF_CPICK_VAR, DEF_CPICK_VAR);
      lives_proc_thread_cancel(mainw->helper_procthreads[PT_CUSTOM_COLOURS], FALSE);
      //lives_proc_thread_join(mainw->helper_procthreads[PT_CUSTOM_COLOURS]);
      // must take care to execute this here or in the function itself, otherwise
      // gtk+ may crash later
      main_thread_execute_rvoid_pvoid(set_extra_colours);
    }
    mainw->helper_procthreads[PT_CUSTOM_COLOURS] = NULL;
  }
#endif

  /* mainw->helper_procthreads[PT_PERF_MANAGER] = */
  /*    lives_proc_thread_create(LIVES_THRDATTR_NONE, */
  /* 			     (lives_funcptr_t)perf_manager, -1, ""); */

  if (prefs->crash_recovery) got_files = check_for_recovery_files(auto_recover, no_recover);

  if (!mainw->foreign && !got_files && prefs->ar_clipset) {
    d_print(lives_strdup_printf(_("Autoloading set %s..."), prefs->ar_clipset_name));
    if (!reload_set(prefs->ar_clipset_name) || mainw->current_file == -1) {
      set_string_pref(PREF_AR_CLIPSET, "");
      prefs->ar_clipset = FALSE;
    }
    future_prefs->ar_clipset = FALSE;
  }

#ifdef ENABLE_OSC
  if (prefs->osc_start) prefs->osc_udp_started = lives_osc_init(prefs->osc_udp_port);
#endif

  if (mainw->recoverable_layout) {
    if (!prefs->vj_mode) layout_recovered = do_layout_recover_dialog();
    else mainw->recoverable_layout = FALSE;
  }

  if (!mainw->recording_recovered) {
    if (mainw->ascrap_file != -1) {
      if (!layout_recovered || !mainw->multitrack || !used_in_current_layout(mainw->multitrack, mainw->ascrap_file)) {
        close_ascrap_file(FALSE); // ignore but leave file on disk for recovery purposes
      }
    }
    if (mainw->scrap_file != -1) {
      if (!layout_recovered || mainw->multitrack || !used_in_current_layout(mainw->multitrack, mainw->scrap_file)) {
        close_scrap_file(FALSE); // ignore but leave file on disk for recovery purposes
      }
    }
  } else {
    if (mainw->multitrack) multitrack_delete(mainw->multitrack, FALSE);
  }

#ifdef HAVE_YUV4MPEG
  if (*prefs->yuvin) lives_idle_add_simple(open_yuv4m_startup, NULL);
#endif

  if (prefs->startup_phase == 100) prefs->startup_phase = 0;

  mainw->no_switch_dprint = TRUE;
  if (mainw->current_file > -1 && !mainw->multitrack) {
    switch_clip(1, mainw->current_file, TRUE);
#ifdef ENABLE_GIW
    giw_timeline_set_max_size(GIW_TIMELINE(mainw->hruler), CURRENT_CLIP_TOTAL_TIME);
#endif
    lives_ruler_set_upper(LIVES_RULER(mainw->hruler), CURRENT_CLIP_TOTAL_TIME);
    lives_widget_queue_draw(mainw->hruler);
  }

  if (!palette || !(palette->style & STYLE_LIGHT)) {
    lives_widget_set_opacity(mainw->sep_image, 0.4);
  } else {
    lives_widget_set_opacity(mainw->sep_image, 0.8);
  }
  lives_widget_queue_draw(mainw->sep_image);

  if (*devmap) on_devicemap_load_activate(NULL, devmap);

  if (capable->username)
    ustr = lives_strdup_printf(", %s", capable->username);
  else
    ustr = lives_strdup("");

  d_print(_("\nWelcome to LiVES version %s%s !\n"), LiVES_VERSION, ustr);
  lives_free(ustr);

  mainw->no_switch_dprint = FALSE;
  d_print("");

  if (mainw->debug_log) {
    close_logfile(mainw->debug_log);
    mainw->debug_log = NULL;
  }

  if (mainw->multitrack) {
    lives_idle_add_simple(mt_idle_show_current_frame, (livespointer)mainw->multitrack);
    if (mainw->multitrack->idlefunc == 0) {
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }
  }

  mainw->go_away = FALSE;
  if (!mainw->multitrack) sensitize();

  if (prefs->vj_mode) {
    lives_window_present(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    /* char *wid = */
    /*   lives_strdup_printf("0x%08lx", */
    /*                       (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET))); */
    /* if (wid) activate_x11_window(wid); */
  }
  if (mainw->recording_recovered) {
    lives_idle_add_simple(render_choice_idle, LIVES_INT_TO_POINTER(TRUE));
  }

  mainw->overlay_alarm = lives_alarm_set(0);

  if (!prefs->vj_mode && !prefs->startup_phase) {
    mainw->helper_procthreads[PT_LAZY_RFX] =
      lives_proc_thread_create(LIVES_THRDATTR_NONE,
                               (lives_funcptr_t)add_rfx_effects, WEED_SEED_BOOLEAN, "i", RFX_STATUS_ANY);
  }

  // TODO *** check if still working
  // timer to poll for external commands: MIDI, joystick, jack transport, osc, etc.
  mainw->kb_timer = lives_timer_add_simple(EXT_TRIGGER_INTERVAL, &ext_triggers_poll, NULL);

  mainw->lazy_starter =
    lives_proc_thread_create_pvoid(LIVES_THRDATTR_IDLEFUNC | LIVES_THRDATTR_WAIT_SYNC
                                   | LIVES_THRDATTR_NULLIFY_ON_DESTRUCTION,
                                   lazy_startup_checks, WEED_SEED_BOOLEAN);

  lives_proc_thread_hook_append(mainw->lazy_starter, COMPLETED_HOOK, 0,
                                lazy_start_fin, (void *)&mainw->lazy_starter);

  lives_proc_thread_set_pauseable(mainw->lazy_starter, TRUE);
  lives_proc_thread_sync_ready(mainw->lazy_starter);

  if (!CURRENT_CLIP_IS_VALID) lives_ce_update_timeline(0, 0.);

  if (newconfigfile) {
    cleanup_old_config(atoll(mainw->old_vhash));
    lives_free(newconfigfile);
  }

  if (!mainw->mute) {
    lives_widget_set_opacity(mainw->m_mutebutton, .75);
  }
  lives_widget_set_opacity(mainw->m_sepwinbutton, .75);
  lives_widget_set_opacity(mainw->m_loopbutton, .75);

  if (prefs->interactive) set_interactive(TRUE);

#ifdef ENABLE_JACK_TRANSPORT
  mainw->jack_can_start = TRUE;
#endif

  mainw->is_ready = TRUE;
  lives_window_set_auto_startup_notification(TRUE);

  if (prefs->show_gui) {
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    lives_widget_context_update();
  }

  //mainw->fg_service_handle = lives_idle_priority(fg_service_fulfill_cb, NULL);
  g_idle_add(fg_service_fulfill_cb, NULL);

  if (!mainw->multitrack)
    lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_CE);
  else
    lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_MT);

  return FALSE;
} // end lives_startup2()


int run_the_program(int argc, char *argv[], pthread_t *gtk_thread, ulong id) {
  // init prefs
  char **xargv = argv;
  pthread_mutexattr_t mattr;
  wordexp_t extra_cmds;
  char cdir[PATH_MAX];
  char *tmp, *dir, *msg;
  char *extracmds_file;
  ssize_t mynsize;
  boolean toolong = FALSE;
  int xargc = argc;
  weed_error_t werr;
  int winitopts = 0;

#ifndef IS_LIBLIVES
  weed_plant_t *test_plant;
#endif

#ifndef IS_LIBLIVES
  // start up the Weed system
  weed_abi_version = libweed_get_abi_version();
  if (weed_abi_version > WEED_ABI_VERSION) weed_abi_version = WEED_ABI_VERSION;
#ifdef WEED_STARTUP_TESTS
  winitopts |= WEED_INIT_DEBUGMODE;
#endif
  winitopts |= 8; // skip un-needed error checks
  werr = libweed_init(weed_abi_version, winitopts);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }

#if !USE_STD_MEMFUNCS
#if USE_RPMALLOC
  libweed_set_memory_funcs(rpmalloc, rpfree);
#else
#ifndef DISABLE_GSLICE
#if GLIB_CHECK_VERSION(2, 14, 0)
  libweed_set_slab_funcs(lives_slice_alloc, lives_slice_unalloc, lives_slice_alloc_and_copy);
#else
  libweed_set_slab_funcs(lives_slice_alloc, lives_slice_unalloc, NULL);
#endif
#else
  libweed_set_memory_funcs(_lives_malloc, _lives_free);
#endif // DISABLE_GSLICE
#endif // USE_RPMALLOC
  weed_utils_set_custom_memfuncs(_lives_malloc, _lives_calloc, _lives_memcpy, NULL, _lives_free);
#endif // USE_STD_MEMFUNCS
#endif //IS_LIBLIVES

  // backup the core functions so we can override them
  _weed_plant_new = weed_plant_new;
  _weed_plant_free = weed_plant_free;
  _weed_leaf_set = weed_leaf_set;
  _weed_leaf_get = weed_leaf_get;
  _weed_leaf_delete = weed_leaf_delete;
  _weed_plant_list_leaves = weed_plant_list_leaves;
  _weed_leaf_num_elements = weed_leaf_num_elements;
  _weed_leaf_element_size = weed_leaf_element_size;

#if WEED_ABI_CHECK_VERSION(202)
  _weed_leaf_set_element_size = weed_leaf_set_element_size;
#endif

  _weed_leaf_seed_type = weed_leaf_seed_type;
  _weed_leaf_get_flags = weed_leaf_get_flags;
  _weed_leaf_set_flags = weed_leaf_set_flags;

  mainw = (mainwindow *)(lives_calloc(1, sizeof(mainwindow)));

  mainw->wall_ticks = -1;
  mainw->initial_ticks = lives_get_current_ticks();

#ifdef WEED_STARTUP_TESTS
  run_weed_startup_tests();
  abort();
#if 0
  fprintf(stderr, "\n\nRetesting with API 200, bugfix mode\n");
  werr = libweed_init(200, winitopts | WEED_INIT_ALLBUGFIXES);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }
  run_weed_startup_tests();
  fprintf(stderr, "\n\nRetesting with API 200, epecting problems in libweed-utils\n");
  werr = libweed_init(200, winitopts);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }
  run_weed_startup_tests();
#endif
  abort();
#endif

#ifdef ENABLE_DIAGNOSTICS
  check_random();
  lives_struct_test();
  test_palette_conversions();
#endif

  // allow us to set immutable values (plugins can't)
  weed_leaf_set = weed_leaf_set_host;

  // allow us to delete undeletable leaves (plugins can't)
  weed_leaf_delete = weed_leaf_delete_host;

  // allow us to set immutable values (plugins can't)
  //weed_leaf_get = weed_leaf_get_monitor;

  // allow us to free undeletable plants (plugins cant')
  weed_plant_free = weed_plant_free_host;
  // weed_plant_new = weed_plant_new_host;

  init_random();

  init_colour_engine();

  make_std_icaps();

  weed_threadsafe = FALSE;
  test_plant = weed_plant_new(0);
  if (weed_leaf_set_private_data(test_plant, WEED_LEAF_TYPE, NULL) == WEED_ERROR_CONCURRENCY)
    weed_threadsafe = TRUE;
  else weed_threadsafe = FALSE;
  weed_plant_free(test_plant);

  widget_helper_init();

#ifdef WEED_WIDGETS
  widget_klasses_init(LIVES_TOOLKIT_GTK);
  //show_widgets_info();
#endif

  // non-localised name
  lives_set_prgname("LIVES");

  /* TRANSLATORS: localised name may be used here */
  lives_set_application_name(_("LiVES"));
  widget_opts.title_prefix = lives_strdup_printf("%s-%s: - ",
                             lives_get_application_name(), LiVES_VERSION);

  prefs = (_prefs *)lives_calloc(1, sizeof(_prefs));
  future_prefs = (_future_prefs *)lives_calloc(1, sizeof(_future_prefs));
  prefs->workdir[0] = '\0';
  future_prefs->workdir[0] = '\0';
  prefs->config_datadir[0] = '\0';
  prefs->configfile[0] = '\0';

  prefs->show_gui = TRUE;
  prefs->show_splash = FALSE;
  prefs->show_playwin = TRUE;
  prefs->interactive = TRUE;

  lives_snprintf(prefs->cmd_log, PATH_MAX, "%s", LIVES_DEVNULL);

#ifdef HAVE_YUV4MPEG
  prefs->yuvin[0] = '\0';
#endif

  mainw->version_hash = lives_strdup_printf("%d", verhash(LiVES_VERSION));
  mainw->memok = TRUE;
  mainw->go_away = TRUE;
  mainw->last_dprint_file = mainw->current_file = mainw->playing_file = -1;
  mainw->clutch = TRUE;
  mainw->max_textsize = N_TEXT_SIZES;

  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

  for (int i = 0; i < N_GLOBAL_HOOKS; i++) pthread_mutex_init(&mainw->global_hook_mutexes[i], NULL);

  mainw->prefs_cache = mainw->hdrs_cache = mainw->gen_cache = mainw->meta_cache = NULL;

  // mainw->foreign is set if we are grabbing an external window
  mainw->foreign = FALSE;

  mainw->sense_state = LIVES_SENSE_STATE_INSENSITIZED;

  capable->has_perl = TRUE;
  capable->has_smogrify = TRUE;
  capable->smog_version_correct = TRUE;
  capable->can_read_from_config = TRUE;
  capable->can_write_to_config = TRUE;
  capable->can_write_to_config_backup = TRUE;
  capable->can_write_to_config_new = TRUE;
  capable->can_write_to_workdir = TRUE;

  // this is the version we should pass into mkdir
  capable->umask = umask(0);
  umask(capable->umask);
  capable->umask = (0777 & ~capable->umask);

#ifdef GUI_GTK
  lives_snprintf(capable->home_dir, PATH_MAX, "%s", g_get_home_dir());
#else
  tmp = getenv("HOME");
  lives_snprintf(capable->home_dir, PATH_MAX, "%s", tmp);
  lives_free(tmp);
#endif

  // CMDS part 2
  ensure_isdir(capable->home_dir);

  // get opts first
  //
  // we can read some pre-commands from a file, there is something of a chicken / egg problem as we dont know
  // yet where config_datadir will be, so a fixed path is used: either
  // $HOME/.config/lives/cmdline or /etc/lives/cmdline

  capable->extracmds_file[0] = extracmds_file = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_DIR, EXTRA_CMD_FILE,
                               NULL);
  capable->extracmds_file[1] = lives_build_filename(LIVES_ETC_DIR, LIVES_DIR_LITERAL, EXTRA_CMD_FILE, NULL);
  capable->extracmds_idx = 0;

  // before parsing cmdline we may also parse cmds in a file
  if (!lives_file_test(capable->extracmds_file[0], LIVES_FILE_TEST_IS_REGULAR)) {
    if (!lives_file_test(capable->extracmds_file[1], LIVES_FILE_TEST_IS_REGULAR)) {
      extracmds_file = NULL;
      capable->extracmds_idx = -1;
    } else {
      extracmds_file = capable->extracmds_file[1];
      capable->extracmds_idx = 1;
    }
  }
  if (extracmds_file) {
    char extrabuff[2048];
    size_t buflen;
    if ((buflen = lives_fread_string(extrabuff, 2048, extracmds_file)) > 0) {
      int weret, i, j = 1;
      if (extrabuff[--buflen] == '\n') extrabuff[buflen] = 0;
      prefs->cmdline_args = lives_strdup(extrabuff);
      if (!(weret = wordexp(extrabuff, &extra_cmds, 0))) {
        if (extra_cmds.we_wordc) {
          // create a new array with extra followed by cmdline
          xargc += extra_cmds.we_wordc;
          xargv = lives_calloc(xargc, sizeof(char *));
          xargv[0] = argv[0];
          for (i = 0; i < extra_cmds.we_wordc; i++) {
            xargv[i + 1] = extra_cmds.we_wordv[i];
          }
          for (; i < xargc - 1; i++) {
            xargv[i + 1] = argv[j++];
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  if (xargc > 1) {
    if (!strcmp(xargv[1], "-capture")) {
      // special mode for grabbing external window
      mainw->foreign = TRUE;
      future_prefs->audio_src = prefs->audio_src = AUDIO_SRC_EXT;
      ign_opts.ign_asource = TRUE;
    } else if (!strcmp(xargv[1], "-help") || !strcmp(xargv[1], "--help")) {
      char string[256];
      get_location(EXEC_PLAY, string, 256);
      if (*string) capable->has_sox_play = TRUE;

      capable->myname_full = lives_find_program_in_path(xargv[0]);

      if ((mynsize = lives_readlink(capable->myname_full, cdir, PATH_MAX)) != -1) {
        lives_memset(cdir + mynsize, 0, 1);
        lives_free(capable->myname_full);
        capable->myname_full = lives_strdup(cdir);
      }

      lives_snprintf(cdir, PATH_MAX, "%s", capable->myname_full);
      get_basename(cdir);
      capable->myname = lives_strdup(cdir);

      print_opthelp(NULL, capable->extracmds_file[0], capable->extracmds_file[1]);
      exit(0);
    } else if (!strcmp(xargv[1], "-version") || !strcmp(xargv[1], "--version")) {
      print_notice();
      exit(0);
    } else {
      struct option longopts[] = {
        {"aplayer", 1, 0, 0},
        {"asource", 1, 0, 0},
        {"workdir", 1, 0, 0},
        {"configfile", 1, 0, 0},
        {"configdatadir", 1, 0, 0},
        {"plugins-libdir", 1, 0, 0},
        {"dscrit", 1, 0, 0},
        {"set", 1, 0, 0},
        {"noset", 0, 0, 0},
#ifdef ENABLE_OSC
        {"devicemap", 1, 0, 0},
#endif
        {"vppdefaults", 1, 0, 0},
        {"recover", 0, 0, 0},
        {"autorecover", 0, 0, 0},
        {"norecover", 0, 0, 0},
        {"noautorecover", 0, 0, 0},
        {"nogui", 0, 0, 0},
        {"nosplash", 0, 0, 0},
        {"noplaywin", 0, 0, 0},
        {"noninteractive", 0, 0, 0},
        {"startup-ce", 0, 0, 0},
        {"startup-mt", 0, 0, 0},
        {"vjmode", 0, 0, 0},
        {"fxmodesmax", 1, 0, 0},
        {"yuvin", 1, 0, 0},
        {"debug", 0, 0, 0},
#ifdef ENABLE_OSC
        {"oscstart", 1, 0, 0},
        {"nooscstart", 0, 0, 0},
#endif
#ifdef ENABLE_JACK
        {"jackopts", 1, 0, 0},
        {"jackserver", optional_argument, 0, 0},
        {"jackscript", 1, 0, 0},
#endif
        // deprecated
        {"nothreaddialog", 0, 0, 0},
        {"bigendbug", 1, 0, 0},
        {"tmpdir", 1, 0, 0},
        {0, 0, 0, 0}
      };

      int option_index = 0;
      const char *charopt;
      int c;
      int count = 0;

      while (1) {
        count++;
        c = getopt_long_only(xargc, xargv, "", longopts, &option_index);
        if (c == -1) break;

        charopt = longopts[option_index].name;
        if (c == '?') {
          msg = lives_strdup_printf(_("Invalid option %s on commandline\n"), xargv[count]);
          LIVES_FATAL(msg);
          lives_free(msg);
          msg = NULL;
        }
        if (!strcmp(charopt, "workdir") || !strcmp(charopt, "tmpdir")) {
          if (!*optarg) {
            do_abortblank_error(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_abortblank_error(charopt);
            optind--;
            continue;
          }
          if (lives_strlen(optarg) > PATH_MAX - MAX_SET_NAME_LEN * 2) {
            toolong = TRUE;
          } else {
            ensure_isdir(optarg);
            if (lives_strlen(optarg) > PATH_MAX - MAX_SET_NAME_LEN * 2) {
              toolong = TRUE;
            }
          }
          if (toolong) {
            dir_toolong_error(optarg, (tmp = (_("working directory"))), PATH_MAX - MAX_SET_NAME_LEN * 2, TRUE);
            lives_free(tmp);
            capable->can_write_to_workdir = FALSE;
            break;
          }

          mainw->has_session_workdir = TRUE;
          lives_snprintf(prefs->workdir, PATH_MAX, "%s", optarg);

          if (!lives_make_writeable_dir(prefs->workdir)) {
            // abort if we cannot write to the specified workdir
            capable->can_write_to_workdir = FALSE;
            break;
          }
          continue;
        }

        if (!strcmp(charopt, "plugins-libdir")) {
          if (!*optarg) {
            do_abortblank_error(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_abortblank_error(charopt);
            optind--;
            continue;
          }
          if (lives_strlen(optarg) > PATH_MAX - MAX_SET_NAME_LEN * 2) {
            toolong = TRUE;
          } else {
            ensure_isdir(optarg);
            if (lives_strlen(optarg) > PATH_MAX - MAX_SET_NAME_LEN * 2) {
              toolong = TRUE;
            }
          }
          if (toolong) {
            dir_toolong_error(optarg, (tmp = (_("plugins lib directory"))), PATH_MAX - MAX_SET_NAME_LEN * 2, TRUE);
            lives_free(tmp);
            capable->can_write_to_workdir = FALSE;
            break;
          }
          // check for subdirs decoders, effects/rendered, encoders, playback/video
          if (!check_for_plugins(optarg, FALSE)) lives_snprintf(prefs->lib_dir, PATH_MAX, "%s", optarg);
          ign_opts.ign_libdir = TRUE;
          continue;
        }

        if (!strcmp(charopt, "configdatadir")) {
          if (!*optarg) {
            do_abortblank_error(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_abortblank_error(charopt);
            optind--;
            continue;
          }
          if (lives_strlen(optarg) > PATH_MAX - 64) {
            toolong = TRUE;
          } else {
            ensure_isdir(optarg);
            if (lives_strlen(optarg) > PATH_MAX - 64) {
              toolong = TRUE;
            }
          }
          if (toolong) {
            /// FALSE => exit via startup_msg_fatal()
            dir_toolong_error(optarg, _("config data directory"), PATH_MAX - 64, FALSE);
          }

          lives_snprintf(prefs->config_datadir, PATH_MAX, "%s", optarg);
          ign_opts.ign_config_datadir = TRUE;
          continue;
        }

        if (!strcmp(charopt, "configfile")) {
          if (!*optarg) {
            do_abortblank_error(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_abortblank_error(charopt);
            optind--;
            continue;
          }
          if (lives_strlen(optarg) > PATH_MAX - 64) {
            toolong = TRUE;
          }
          if (toolong) {
            /// FALSE => exit via startup_msg_fatal()
            filename_toolong_error(optarg, _("configuration file"), PATH_MAX, FALSE);
          }

          lives_snprintf(prefs->configfile, PATH_MAX, "%s", optarg);
          ign_opts.ign_configfile = TRUE;
          continue;
        }

        if (!strcmp(charopt, "norecover") || !strcmp(charopt, "noautorecover")) {
          // auto no-recovery
          no_recover = TRUE;
          continue;
        }

        if (!strcmp(charopt, "recover") || !strcmp(charopt, "autorecover")) {
          // auto recovery
          auto_recover = TRUE;
          continue;
        }

        if (!strcmp(charopt, "debug")) {
          // debug crashes
          mainw->debug = TRUE;
          continue;
        }

        if (!strcmp(charopt, "yuvin")) {
#ifdef HAVE_YUV4MPEG
          char *dir;
          if (!*optarg) {
            continue;
          }
          if (optarg[0] == '-') {
            optind--;
            continue;
          }
          lives_snprintf(prefs->yuvin, PATH_MAX, "%s", optarg);
          prefs->startup_interface = STARTUP_CE;
          ign_opts.ign_stmode = TRUE;
          dir = get_dir(prefs->yuvin);
          get_basename(prefs->yuvin);
          lives_snprintf(prefs->yuvin, PATH_MAX, "%s", (tmp = lives_build_filename(dir, prefs->yuvin, NULL)));
          lives_free(tmp);
          lives_free(dir);
#else
          msg = (_("Must have mjpegtools installed for -yuvin to work"));
          do_abort_ok_dialog(msg);
          lives_free(msg);
#endif
          continue;
        }

        if (!strcmp(charopt, "dscrit") && optarg) {
          // force clipset loading
          if (!*optarg) {
            do_abortblank_error(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_abortblank_error(charopt);
            optind--;
            continue;
          }
          prefs->ds_crit_level = atoll(optarg);
          ign_opts.ign_dscrit = TRUE;
          continue;
        }

        if (!strcmp(charopt, "noset")) {
          // override clipset loading
          lives_memset(prefs->ar_clipset_name, 0, 1);
          prefs->ar_clipset = FALSE;
          ign_opts.ign_clipset = TRUE;
          continue;
        }

        if (!strcmp(charopt, "set") && optarg) {
          // force clipset loading
          if (!*optarg) {
            do_abortblank_error(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_abortblank_error(charopt);
            optind--;
            continue;
          }
          if (!is_legal_set_name(optarg, TRUE, TRUE)) {
            msg = (_("Abort and retry or continue ?"));
            do_abort_ok_dialog(msg);
            lives_free(msg);
          }
          lives_snprintf(prefs->ar_clipset_name, 128, "%s", optarg);
          prefs->ar_clipset = TRUE;
          ign_opts.ign_clipset = TRUE;
          continue;
        }

        if (!strcmp(charopt, "nolayout")) {
          // override layout loading
          lives_memset(prefs->ar_layout_name, 0, 1);
          prefs->ar_layout = FALSE;
          ign_opts.ign_layout = TRUE;
          continue;
        }

        if (!strcmp(charopt, "layout") && optarg) {
          // force layout loading
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          lives_snprintf(prefs->ar_layout_name, PATH_MAX, "%s", optarg);
          prefs->ar_layout = TRUE;
          ign_opts.ign_layout = TRUE;
          continue;
        }

#ifdef ENABLE_OSC
        if (!strcmp(charopt, "devicemap") && optarg) {
          // force devicemap loading
          char *devmap2;
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          lives_snprintf(devmap, PATH_MAX, "%s", optarg);
          devmap2 = lives_strdup(devmap);
          get_basename(devmap);
          if (!strcmp(devmap, devmap2)) {
            dir = lives_build_filename(prefs->config_datadir, LIVES_DEVICEMAP_DIR, NULL);
          } else dir = get_dir(devmap);
          lives_snprintf(devmap, PATH_MAX, "%s", (tmp = lives_build_filename(dir, devmap, NULL)));
          lives_free(tmp);
          lives_free(dir);
          lives_free(devmap2);
          continue;
        }
#endif

        if (!strcmp(charopt, "vppdefaults") && optarg) {
          // load alternate vpp file
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          lives_snprintf(mainw->vpp_defs_file, PATH_MAX, "%s", optarg);
          ign_opts.ign_vppdefs = TRUE;
          dir = get_dir(mainw->vpp_defs_file);
          get_basename(mainw->vpp_defs_file);
          lives_snprintf(mainw->vpp_defs_file, PATH_MAX, "%s", (tmp = lives_build_filename(dir, mainw->vpp_defs_file, NULL)));
          lives_free(tmp);
          lives_free(dir);
          continue;
        }

        if (!strcmp(charopt, "aplayer")) {
          boolean apl_valid = FALSE;
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          lives_snprintf(buff, 256, "%s", optarg);
          // override aplayer default
          if (!strcmp(buff, AUDIO_PLAYER_SOX)) {
            prefs->audio_player = AUD_PLAYER_SOX;
            lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_SOX);
            apl_valid = TRUE;
          }

          if (!strcmp(buff, AUDIO_PLAYER_NONE)) {
            // still experimental
            prefs->audio_player = AUD_PLAYER_NONE;
            lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_NONE);
            apl_valid = TRUE;
          }

          if (!strcmp(buff, AUDIO_PLAYER_JACK)) {
#ifdef ENABLE_JACK
            prefs->audio_player = AUD_PLAYER_JACK;
            lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_JACK);
            apl_valid = TRUE;
#endif
          }

          if (!strcmp(buff, AUDIO_PLAYER_PULSE) || !strcmp(buff, AUDIO_PLAYER_PULSE_AUDIO)) {
#ifdef HAVE_PULSE_AUDIO
            prefs->audio_player = AUD_PLAYER_PULSE;
            lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_PULSE);
            apl_valid = TRUE;
#endif
          }
          if (apl_valid) ign_opts.ign_aplayer = TRUE;
          else {
            msg = lives_strdup_printf(_("Invalid audio player %s"), buff);
            LIVES_ERROR(msg);
            lives_free(msg);
          }
          continue;
        }

        if (!strcmp(charopt, "asource")) {
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          lives_snprintf(buff, 256, "%s", optarg);
          // override audio source
          if (!strcmp(buff, _("external")) || !strcmp(buff, "external")) { // handle translated and original strings
            future_prefs->audio_src = prefs->audio_src = AUDIO_SRC_EXT;
            ign_opts.ign_asource = TRUE;
          } else if (strcmp(buff, _("internal")) && strcmp(buff, "internal")) { // handle translated and original strings
            fprintf(stderr, _("Invalid audio source %s\n"), buff);
          } else {
            future_prefs->audio_src = prefs->audio_src = AUDIO_SRC_INT;
            ign_opts.ign_asource = TRUE;
          }
          continue;
        }

        if (!strcmp(charopt, "nogui")) {
          // force headless mode
          prefs->show_gui = FALSE;
          continue;
        }

        if (!strcmp(charopt, "nosplash")) {
          // do not show splash
          prefs->show_splash = FALSE;
          continue;
        }

        if (!strcmp(charopt, "noplaywin")) {
          // do not show the play window
          prefs->show_playwin = FALSE;
          continue;
        }

        if (!strcmp(charopt, "noninteractive")) {
          // disable menu/toolbar interactivity
          prefs->interactive = FALSE;
          continue;
        }

        if (!strcmp(charopt, "nothreaddialog")) {
          continue;
        }

        if (!strcmp(charopt, "fxmodesmax") && optarg) {
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          // set number of fx modes
          prefs->rte_modes_per_key = atoi(optarg);
          if (prefs->rte_modes_per_key < 1) prefs->rte_modes_per_key = 1;
          if (prefs->rte_modes_per_key > FX_MODES_MAX) prefs->rte_modes_per_key = FX_MODES_MAX;
          ign_opts.ign_rte_keymodes = TRUE;
          continue;
        }

        if (!strcmp(charopt, "bigendbug")) {
          // only for backwards comptaibility
          if (optarg) {
            // set bigendbug
            prefs->bigendbug = atoi(optarg);
          } else prefs->bigendbug = 1;
          continue;
        }
#ifdef ENABLE_OSC

        if (!strcmp(charopt, "oscstart") && optarg) {
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          // force OSC start
          prefs->osc_udp_port = atoi(optarg);
          prefs->osc_start = TRUE;
          ign_opts.ign_osc = TRUE;
          continue;
        }

        if (!strcmp(charopt, "nooscstart")) {
          // force no OSC start
          prefs->osc_start = FALSE;
          ign_opts.ign_osc = TRUE;
          continue;
        }
#endif

#ifdef ENABLE_JACK
        if (!strcmp(charopt, "jackopts") && optarg) {
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          // override jackopts in config file
          ign_opts.ign_jackopts = TRUE;
          prefs->jack_opts = atoi(optarg) & JACK_OPTS_OPTS_MASK;
          continue;
        }

        if (!strcmp(charopt, "jackserver")) {
          char *srvname = xargv[optind++];
          ign_opts.ign_jackserver = TRUE;

          if (!srvname || !*srvname) {
            continue;
          }
          if (*srvname == '-') {
            optind--;
            continue;
          }

          lives_snprintf(future_prefs->jack_aserver_cname, PATH_MAX, "%s", srvname);
          lives_snprintf(future_prefs->jack_aserver_sname, PATH_MAX, "%s", srvname);
          lives_snprintf(future_prefs->jack_tserver_cname, PATH_MAX, "%s", srvname);
          lives_snprintf(future_prefs->jack_tserver_sname, PATH_MAX, "%s", srvname);
          continue;
        }

        if (!strcmp(charopt, "jackscript") && optarg) {
          if (!*optarg) {
            do_optarg_blank_err(charopt);
            continue;
          }
          if (optarg[0] == '-') {
            do_optarg_blank_err(charopt);
            optind--;
            continue;
          }
          // override jackopts in config file
          ign_opts.ign_jackcfg = TRUE;
          pref_factory_string(PREF_JACK_ACONFIG, optarg, FALSE);
          pref_factory_string(PREF_JACK_TCONFIG, optarg, FALSE);
          continue;
        }
#endif
        if (!strcmp(charopt, "startup-ce")) {
          // force start in clip editor mode
          if (!ign_opts.ign_stmode) {
            prefs->startup_interface = STARTUP_CE;
            ign_opts.ign_stmode = TRUE;
          }
          continue;
        }

        if (!strcmp(charopt, "startup-mt")) {
          // force start in multitrack mode
          if (!ign_opts.ign_stmode) {
            prefs->startup_interface = STARTUP_MT;
            ign_opts.ign_stmode = TRUE;
          }
          continue;
        }

        if (!strcmp(charopt, "vjmode")) {
          // force start in multitrack mode
          prefs->vj_mode = TRUE;
          ign_opts.ign_vjmode = TRUE;
          continue;
        }
      }

      if (optind < xargc) {
        // remaining opts are filename [start_time] [end_frame]
        char *dir;
        lives_snprintf(start_file, PATH_MAX, "%s", xargv[optind++]); // filename
        if (optind < xargc) start = lives_strtod(xargv[optind++]); // start time (seconds)
        if (optind < xargc) end = atoi(xargv[optind++]); // number of frames

        if (!lives_strrstr(start_file, "://")) {
          // prepend current directory if needed (unless file contains :// - eg. dvd:// or http://)
          dir = get_dir(start_file);
          get_basename(start_file);
          lives_snprintf(start_file, PATH_MAX, "%s", (tmp = lives_build_filename(dir, start_file, NULL)));
          lives_free(tmp);
          lives_free(dir);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (xargv != argv) {
    wordfree(&extra_cmds);
    lives_free(xargv);
  }

  if (!ign_opts.ign_configfile) {
    tmp = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_DIR, LIVES_DEF_CONFIG_FILE, NULL);
    lives_snprintf(prefs->configfile, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  if (!ign_opts.ign_config_datadir) {
    tmp = get_localsharedir(LIVES_DIR_LITERAL);
    lives_snprintf(prefs->config_datadir, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  if (mainw->debug) {
    mainw->debug_log = open_logfile(NULL);
  }

  // get capabilities and if OK set some initial prefs
  theme_error = pre_init();

  //set_meta("status", "running");

  /* widget_helper_suggest_icons("filter"); */
  /* abort(); */

  lives_memset(start_file, 0, 1);

  mainw->libthread = gtk_thread;

  // what's my name ?
  capable->myname_full = lives_find_program_in_path(argv[0]);

  if ((mynsize = lives_readlink(capable->myname_full, cdir, PATH_MAX)) != -1) {
    // no. i mean, what's my real name ?
    lives_memset(cdir + mynsize, 0, 1);
    lives_free(capable->myname_full);
    capable->myname_full = lives_strdup(cdir);
  }

  // what's my short name (without the path) ?
  lives_snprintf(cdir, PATH_MAX, "%s", capable->myname_full);
  get_basename(cdir);
  capable->myname = lives_strdup(cdir);

  // format is:
  // lives [opts] [filename [start_time] [frames]]

  // need to do this here, before lives_startup but after setting ign_opts
  mainw->new_vpp = NULL;
  mainw->vpp = NULL;
  lives_memset(future_prefs->vpp_name, 0, 64);
  future_prefs->vpp_argv = NULL;

  if (!ign_opts.ign_vppdefs) {
    tmp = lives_build_filename(prefs->config_datadir, VPP_DEFS_FILE, NULL);
    lives_snprintf(mainw->vpp_defs_file, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  if (prefs->startup_phase == -1) prefs->startup_phase = 1;
  lives_idle_add_simple(lives_startup, NULL);

#ifdef GUI_GTK
  if (!gtk_thread) {
    gtk_main();
    printf("LUVERLY !\n");
  }
#endif

#ifdef GUI_QT
  return qapp->exec();
#endif

  return 0;

}

void startup_message_fatal(char *msg) {
  if (mainw) {
    if (mainw->splash_window) splash_end();
    lives_freep((void **)&mainw->old_vhash);
    lives_freep((void **)&old_vhash);
  }

  do_error_dialog(msg);
  LIVES_FATAL(msg);
  _exit(1);
}


LIVES_GLOBAL_INLINE boolean startup_message_nonfatal(const char *msg) {
  if (capable && capable->ds_status == LIVES_STORAGE_STATUS_CRITICAL) do_abort_ok_dialog(msg);
  else do_error_dialog(msg);
  return TRUE;
}


boolean startup_message_info(const char *msg) {
  widget_opts.non_modal = TRUE;
  do_info_dialog(msg);
  widget_opts.non_modal = FALSE;
  return TRUE;
}


boolean startup_message_nonfatal_dismissable(const char *msg, uint64_t warning_mask) {
  if (warning_mask == WARN_MASK_CHECK_PLUGINS) {
    check_for_plugins(prefs->lib_dir, FALSE);
    return TRUE;
  }
  if (warning_mask == WARN_MASK_CHECK_PREFIX) {
    find_prefix_dir(prefs->prefix_dir, FALSE);
    return TRUE;
  }
  widget_opts.non_modal = TRUE;
  do_error_dialog_with_check(msg, warning_mask);
  widget_opts.non_modal = FALSE;
  return TRUE;
}


static boolean lives_init(_ign_opts * ign_opts) {
  // init mainwindow data
  LiVESList *encoder_capabilities = NULL;

  char **array;
  char mppath[PATH_MAX];

  char *weed_plugin_path;
#ifdef HAVE_FREI0R
  char *frei0r_path;
#endif
#ifdef HAVE_LADSPA
  char *ladspa_path;
#endif
#ifdef HAVE_LIBVISUAL
  char *libvis_path;
#endif
  char *msg;
  char *recfname;

  boolean needs_free;
#ifdef ENABLE_JACK
  boolean success;
  lives_proc_thread_t info;
  ticks_t timeout;
  int orig_err = 0;
  boolean jack_read_start = FALSE;
#endif
  int i;

  mainw->insert_after = TRUE;
  if (!prefs->vj_mode) mainw->save_with_sound = TRUE;   // also affects loading
  mainw->untitled_number = mainw->cap_number = 1;
  mainw->sel_move = SEL_MOVE_AUTO;
  mainw->opwx = mainw->opwy = -1;
  mainw->toy_type = LIVES_TOY_NONE;
  mainw->framedraw = mainw->framedraw_spinbutton = NULL;
  if (capable->hw.byte_order == LIVES_LITTLE_ENDIAN) {
    mainw->endian = 0;
  } else {
    mainw->endian = AFORM_BIG_ENDIAN;
  }

  for (i = 0; i < FN_KEYS - 1; i++) {
    mainw->clipstore[i][0] = -1;
  }

  fx_dialog[0] = fx_dialog[1] = NULL;

  mainw->rte_keys = -1;
  rte_window = NULL;

  mainw->rte = EFFECT_NONE;

  mainw->prv_link = PRV_PTR;

  mainw->last_grabbable_effect = -1;
  mainw->blend_file = -1;

  mainw->pre_src_file = -2;
  mainw->pre_src_audio_file = -1;

  mainw->whentostop = NEVER_STOP;

  // rendered_fx number of last transition
  mainw->last_transition_idx = -1;
  mainw->last_transition_loops = 1;
  mainw->last_transition_align_start = TRUE;

  mainw->fixed_fps_numer = -1;
  mainw->fixed_fps_denom = 1;
  mainw->fixed_fpsd = -1.;

  mainw->cancelled = CANCEL_NONE;
  mainw->cancel_type = CANCEL_KILL;

  // setting this to TRUE can possibly increase smoothness for lower framerates
  // needs more testing and a preference in prefs window- TODO
  // can also be set through OSC: /output/nodrop/enable
  prefs->noframedrop = get_boolean_prefd(PREF_NOFRAMEDROP, FALSE);

  prefs->omc_events = TRUE;

  if (!ign_opts->ign_osc) {
#ifdef ENABLE_OSC
    if (!mainw->foreign) {
      prefs->osc_udp_port = get_int_prefd(PREF_OSC_PORT, DEF_OSC_LISTEN_PORT);
      future_prefs->osc_start = prefs->osc_start = get_boolean_prefd(PREF_OSC_START, FALSE);
    }
#endif
  }

  prefs->fps_tolerance = .0005;
  prefs->rec_opts = get_int_prefd(PREF_RECORD_OPTS, -1);

  if (prefs->rec_opts == -1) {
    prefs->rec_opts = REC_FPS | REC_FRAMES | REC_EFFECTS | REC_CLIPS | REC_AUDIO;
    set_int_pref(PREF_RECORD_OPTS, prefs->rec_opts);
  }

  prefs->rec_opts |= (REC_FPS + REC_FRAMES);

  mainw->new_clip = -1;
  mainw->scrap_file = -1;
  mainw->ascrap_file = -1;
  mainw->scrap_file_size = -1;

  mainw->did_rfx_preview = FALSE;

  prefsw = NULL;
  rdet = NULL;
  resaudw = NULL;

  mainw->leave_recovery = TRUE;

  mainw->new_blend_file = -1;

  mainw->show_procd = TRUE;

  mainw->img_concat_clip = -1;

  mainw->record_paused = mainw->record_starting = FALSE;

  mainw->stream_ticks = -1;

  mainw->osc_auto = 0;
  mainw->osc_enc_width = mainw->osc_enc_height = 0;
  mainw->stored_layout_save_all_vals = TRUE;

  mainw->go_away = TRUE;
  mainw->status = LIVES_STATUS_NOTREADY;

  mainw->aud_file_to_kill = -1;

  mainw->aud_rec_fd = -1;

  mainw->log_fd = -2;

  mainw->render_error = LIVES_RENDER_ERROR_NONE;

  mainw->ce_frame_height = mainw->ce_frame_width = -1;

  mainw->cursor_style = LIVES_CURSOR_NORMAL;

  mainw->sepwin_minwidth = MIN_SEPWIN_WIDTH;
  mainw->sepwin_minheight = PREVIEW_BOX_HT;

  mainw->n_screen_areas = SCREEN_AREA_USER_DEFINED1;
  mainw->screen_areas = (lives_screen_area_t *)lives_malloc(mainw->n_screen_areas * sizeof(lives_screen_area_t));
  mainw->screen_areas[SCREEN_AREA_FOREGROUND].name = (_("Foreground"));
  mainw->screen_areas[SCREEN_AREA_BACKGROUND].name = (_("Background"));

  mainw->active_sa_clips = mainw->active_sa_fx = SCREEN_AREA_FOREGROUND;

  mainw->swapped_clip = -1;

  mainw->blend_palette = WEED_PALETTE_END;

  mainw->audio_stretch = 1.0;

  mainw->record_frame = -1;

  mainw->pre_play_file = -1;

  mainw->num_sets = -1;

  mainw->drawsrc = -1;

  /////////////////////////////////////////////////// add new stuff just above here ^^

  future_prefs->pb_quality = prefs->pb_quality = get_int_prefd(PREF_PB_QUALITY, PB_QUALITY_MED);
  if (prefs->pb_quality != PB_QUALITY_LOW && prefs->pb_quality != PB_QUALITY_HIGH &&
      prefs->pb_quality != PB_QUALITY_MED) prefs->pb_quality = PB_QUALITY_MED;

  prefs->pbq_adaptive = get_boolean_prefd(PREF_PBQ_ADAPTIVE, TRUE);

  prefs->loop_recording = TRUE;
  prefs->ocp = get_int_prefd(PREF_OPEN_COMPRESSION_PERCENT, 15);

  prefs->stop_screensaver = get_boolean_prefd(PREF_STOP_SCREENSAVER, TRUE);

  if (prefs->gui_monitor != 0) {
    lives_window_center(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  prefs->default_fps = get_double_prefd(PREF_DEFAULT_FPS, DEF_FPS);
  if (prefs->default_fps < 1.) prefs->default_fps = 1.;
  if (prefs->default_fps > FPS_MAX) prefs->default_fps = FPS_MAX;

  // values for trickplay
  prefs->blendchange_amount = get_double_prefd(PREF_BLEND_AMOUNT, DEF_BLENDCHANGE_AMOUNT);
  prefs->scratchfwd_amount  = get_int_prefd(PREF_SCFWD_AMOUNT, DEF_SCRATCHFWD_AMOUNT);
  prefs->scratchback_amount = get_int_prefd(PREF_SCBACK_AMOUNT, DEF_SCRATCHBACK_AMOUNT);
  prefs->fpschange_amount   = get_double_prefd(PREF_FPSCHANGE_AMOUNT, DEF_FPSCHANGE_AMOUNT);

  prefs->q_type = Q_SMOOTH;

  prefs->event_window_show_frame_events = FALSE;
  if (!mainw->foreign) prefs->crash_recovery = TRUE;
  else prefs->crash_recovery = FALSE;

  prefs->render_audio = TRUE;
  prefs->normalise_audio = TRUE;

  prefs->num_rtaudiobufs = 4;

  prefs->safe_symlinks = FALSE; // set to TRUE for dynebolic and other live CDs

  prefs->ce_maxspect = get_boolean_prefd(PREF_CE_MAXSPECT, TRUE);

  if (!ign_opts->ign_rte_keymodes) {
    prefs->rte_modes_per_key = get_int_prefd(PREF_RTE_MODES_PERKEY, DEF_FX_KEYMODES);
    if (prefs->rte_modes_per_key < 1) prefs->rte_modes_per_key = 1;
    if (prefs->rte_modes_per_key > FX_MODES_MAX) prefs->rte_modes_per_key = FX_MODES_MAX;
  }

  prefs->stream_audio_out = get_boolean_pref(PREF_STREAM_AUDIO_OUT);

  prefs->unstable_fx = get_boolean_prefd(PREF_UNSTABLE_FX, TRUE);

  prefs->clear_disk_opts = get_int_prefd(PREF_CLEAR_DISK_OPTS, 0);

  prefs->force_system_clock = TRUE;

  prefs->alpha_post = FALSE; ///< allow pre-multiplied alpha internally

  prefs->auto_trim_audio = get_boolean_prefd(PREF_AUTO_TRIM_PAD_AUDIO, TRUE);
  prefs->keep_all_audio = get_boolean_prefd(PREF_KEEP_ALL_AUDIO, FALSE);

  prefs->force64bit = FALSE;

#if LIVES_HAS_GRID_WIDGET
  prefs->ce_thumb_mode = get_boolean_prefd(PREF_CE_THUMB_MODE, FALSE);
#else
  prefs->ce_thumb_mode = FALSE;
#endif

  prefs->push_audio_to_gens = get_boolean_prefd(PREF_PUSH_AUDIO_TO_GENS, TRUE);

  prefs->max_disp_vtracks = get_int_prefd(PREF_MAX_DISP_VTRACKS, DEF_MT_DISP_TRACKS);

  prefs->mt_load_fuzzy = FALSE;

  prefs->ahold_threshold = get_double_pref(PREF_AHOLD_THRESHOLD);

  prefs->use_screen_gamma = get_boolean_prefd(PREF_USE_SCREEN_GAMMA, FALSE);
  prefs->screen_gamma = get_double_prefd(PREF_SCREEN_GAMMA, DEF_SCREEN_GAMMA);
  prefs->apply_gamma = get_boolean_prefd(PREF_APPLY_GAMMA, TRUE);
  prefs->btgamma = FALSE;

  prefs->msgs_nopbdis = get_boolean_prefd(PREF_MSG_NOPBDIS, TRUE);

  prefs->cb_is_switch = FALSE;

  prefs->autoclean = get_boolean_prefd(PREF_AUTOCLEAN_TRASH, TRUE);

  future_prefs->pref_trash = prefs->pref_trash = get_boolean_prefd(PREF_PREF_TRASH, FALSE);

  // get window manager capabilities
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY(mainw->mgeom[0].disp))
    capable->wm_name = lives_strdup("Wayland");
#endif
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY(mainw->mgeom[0].disp))
    capable->wm_name = lives_strdup(gdk_x11_screen_get_window_manager_name(gdk_screen_get_default()));
#endif

  *capable->wm_caps.wm_name = 0;
  capable->has_wm_caps = FALSE;
  get_wm_caps();

  if (*capable->wm_caps.panel)
    prefs->show_desktop_panel = get_x11_visible(capable->wm_caps.panel);
  if (prefs->show_dev_opts)
    prefs->show_desktop_panel = TRUE;

  prefs->show_msgs_on_startup = get_boolean_prefd(PREF_MSG_START, TRUE);

  /// record rendering options
  prefs->rr_crash = get_boolean_prefd(PREF_RRCRASH, TRUE);
  prefs->rr_super = get_boolean_prefd(PREF_RRSUPER, TRUE);
  prefs->rr_pre_smooth = get_boolean_prefd(PREF_RRPRESMOOTH, TRUE);
  prefs->rr_qsmooth = get_boolean_prefd(PREF_RRQSMOOTH, TRUE);
  prefs->rr_amicro = get_boolean_prefd(PREF_RRAMICRO, TRUE);
  prefs->rr_ramicro = get_boolean_prefd(PREF_RRRAMICRO, TRUE);

  prefs->rr_qmode = get_int_prefd(PREF_RRQMODE, 0);
  prefs->rr_qmode = INT_CLAMP(prefs->rr_qmode, 0, 1);
  prefs->rr_fstate = get_int_prefd(PREF_RRFSTATE, 0);
  prefs->rr_fstate = INT_CLAMP(prefs->rr_fstate, 0, 1);

  /// new prefs here:
  //////////////////////////////////////////////////////////////////

  //get_string_prefd(PREF_DEF_AUTHOR, prefs->def_author, 1024, "");

  if (!mainw->foreign) {
    prefs->midi_check_rate = get_int_pref(PREF_MIDI_CHECK_RATE);
    if (prefs->midi_check_rate == 0) prefs->midi_check_rate = DEF_MIDI_CHECK_RATE;

    if (prefs->midi_check_rate < 1) prefs->midi_check_rate = 1;

    prefs->midi_rpt = get_int_pref(PREF_MIDI_RPT);
    if (prefs->midi_rpt == 0) prefs->midi_rpt = DEF_MIDI_RPT;

    prefs->mouse_scroll_clips = get_boolean_prefd(PREF_MOUSE_SCROLL_CLIPS, TRUE);

    prefs->mt_auto_back = get_int_prefd(PREF_MT_AUTO_BACK, 120);

    get_string_pref(PREF_VIDEO_OPEN_COMMAND, prefs->video_open_command, PATH_MAX * 2);

    if (!*prefs->video_open_command) {
      lives_memset(mppath, 0, 1);

      if (!(*prefs->video_open_command) && capable->has_mplayer) {
        get_location(EXEC_MPLAYER, mppath, PATH_MAX);
      }

      if (!(*prefs->video_open_command) && capable->has_mplayer2) {
        get_location(EXEC_MPLAYER2, mppath, PATH_MAX);
      }

      if (!(*prefs->video_open_command) && capable->has_mpv) {
        get_location(EXEC_MPV, mppath, PATH_MAX);
      }

      if (*mppath) {
        lives_snprintf(prefs->video_open_command, PATH_MAX + 2, "\"%s\"", mppath);
        set_string_pref(PREF_VIDEO_OPEN_COMMAND, prefs->video_open_command);
      }
    }

    prefs->warn_file_size = get_int_prefd(PREF_WARN_FILE_SIZE, WARN_FILE_SIZE);

    prefs->show_rdet = TRUE;
    prefs->move_effects = TRUE;
    prefs->mt_undo_buf = get_int_prefd(PREF_MT_UNDO_BUF, DEF_MT_UNDO_SIZE);
    prefs->mt_enter_prompt = get_boolean_prefd(PREF_MT_ENTER_PROMPT, TRUE);

    // default frame sizes (TODO - allow pref)
    mainw->def_width = DEF_FRAME_HSIZE;
    mainw->def_height = DEF_FRAME_HSIZE;

    prefs->mt_def_width = get_int_prefd(PREF_MT_DEF_WIDTH, DEF_FRAME_HSIZE_UNSCALED);
    prefs->mt_def_height = get_int_prefd(PREF_MT_DEF_HEIGHT, DEF_FRAME_VSIZE_UNSCALED);
    prefs->mt_def_fps = get_double_prefd(PREF_MT_DEF_FPS, prefs->default_fps);
    prefs->mt_def_arate = get_int_prefd(PREF_MT_DEF_ARATE, DEFAULT_AUDIO_RATE);
    prefs->mt_def_achans = get_int_prefd(PREF_MT_DEF_ACHANS, DEFAULT_AUDIO_CHANS);
    prefs->mt_def_asamps = get_int_prefd(PREF_MT_DEF_ASAMPS, DEFAULT_AUDIO_SAMPS);
    prefs->mt_def_signed_endian = get_int_prefd(PREF_MT_DEF_SIGNED_ENDIAN, (capable->hw.byte_order == LIVES_BIG_ENDIAN)
                                  ? 2 : 0 + ((prefs->mt_def_asamps == 8) ? 1 : 0));

    prefs->mt_exit_render = get_boolean_prefd(PREF_MT_EXIT_RENDER, TRUE);
    prefs->render_prompt = get_boolean_prefd(PREF_RENDER_PROMPT, TRUE);

    prefs->mt_pertrack_audio = get_boolean_prefd(PREF_MT_PERTRACK_AUDIO, TRUE);
    //prefs->mt_backaudio = get_int_prefd(PREF_MT_BACKAUDIO, 1);
    prefs->mt_backaudio = get_int_prefd(PREF_MT_BACKAUDIO, 0);

    prefs->instant_open = get_boolean_prefd(PREF_INSTANT_OPEN, TRUE);
    prefs->auto_deint = get_boolean_prefd(PREF_AUTO_DEINTERLACE, TRUE);

    future_prefs->ar_clipset = FALSE;

    if (!ign_opts->ign_clipset) {
      get_string_prefd(PREF_AR_CLIPSET, prefs->ar_clipset_name, 128, "");
      if (*prefs->ar_clipset_name) future_prefs->ar_clipset = prefs->ar_clipset = TRUE;
      else prefs->ar_clipset = FALSE;
    } else set_string_pref(PREF_AR_CLIPSET, "");

    if (!ign_opts->ign_layout) {
      get_string_prefd(PREF_AR_LAYOUT, prefs->ar_layout_name, PATH_MAX, "");
      if (*prefs->ar_layout_name) prefs->ar_layout = TRUE;
      else prefs->ar_layout = FALSE;
    } else set_string_pref(PREF_AR_LAYOUT, "");

    prefs->rec_desktop_audio = get_boolean_prefd(PREF_REC_DESKTOP_AUDIO, FALSE);

    future_prefs->startup_interface = get_int_prefd(PREF_STARTUP_INTERFACE, STARTUP_CE);
    if (!ign_opts->ign_stmode) {
      prefs->startup_interface = future_prefs->startup_interface;
    }

    if (!prefs->vj_mode) {
      // scan for encoder plugins
#ifndef IS_MINGW
      capable->plugins_list[PLUGIN_TYPE_ENCODER] = get_plugin_list(PLUGIN_ENCODERS, FALSE, NULL, NULL);
#else
      capable->plugins_list[PLUGIN_TYPE_ENCODER] = get_plugin_list(PLUGIN_ENCODERS, TRUE, NULL, NULL);
#endif
      if (capable->plugins_list[PLUGIN_TYPE_ENCODER]) {
        LiVESList *list, *dummy_list, *listnext;
        for (list = capable->plugins_list[PLUGIN_TYPE_ENCODER]; list; list = listnext) {
          listnext = list->next;
          dummy_list = plugin_request("encoders", (const char *)list->data, "init");
          if (!dummy_list) {
            if (prefs->jokes)
              g_print("oh my gawwwd, what happened to %s !?\n", (const char *)list->data);
            capable->plugins_list[PLUGIN_TYPE_ENCODER]
              = lives_list_remove_node(capable->plugins_list[PLUGIN_TYPE_ENCODER], list, TRUE);
          }
        }
        if (capable->plugins_list[PLUGIN_TYPE_ENCODER])
          capable->has_encoder_plugins = PRESENT;
        else
          capable->has_encoder_plugins = MISSING;
      }
    }

    lives_memset(prefs->encoder.of_name, 0, 1);
    lives_memset(prefs->encoder.of_desc, 0, 1);

    if ((prefs->startup_phase == 1 || prefs->startup_phase == -1)
        && capable->has_encoder_plugins == PRESENT && capable->has_python) {
      LiVESList *ofmt_all = NULL;
      char **array;
      if (check_for_executable(&capable->has_ffmpeg, EXEC_FFMPEG)) {
        lives_snprintf(prefs->encoder.name, 64, "%s", FFMPEG_ENCODER_NAME);
      } else {
        if (capable->python_version >= 3000000)
          lives_snprintf(prefs->encoder.name, 64, "%s", MULTI_ENCODER3_NAME);
        else
          lives_snprintf(prefs->encoder.name, 64, "%s", MULTI_ENCODER_NAME);
      }
      // need to change the output format

      if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, prefs->encoder.name, "get_formats")) != NULL) {
        set_string_pref(PREF_ENCODER, prefs->encoder.name);

        for (i = 0; i < lives_list_length(ofmt_all); i++) {
          if (get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|') > 2) {
            array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);

            if (!strcmp(array[0], HI_THEORA_FORMAT)) {
              lives_snprintf(prefs->encoder.of_name, 64, "%s", array[0]);
              lives_strfreev(array);
              break;
            }
            if (!strcmp(array[0], HI_MPEG_FORMAT)) {
              lives_snprintf(prefs->encoder.of_name, 64, "%s", array[0]);
            } else if (!strcmp(array[0], HI_H_MKV_FORMAT) && strcmp(prefs->encoder.of_name, HI_MPEG_FORMAT)) {
              lives_snprintf(prefs->encoder.of_name, 64, "%s", array[0]);
            } else if (!strcmp(array[0], HI_H_AVI_FORMAT) && strcmp(prefs->encoder.of_name, HI_MPEG_FORMAT) &&
                       strcmp(prefs->encoder.of_name, HI_H_MKV_FORMAT)) {
              lives_snprintf(prefs->encoder.of_name, 64, "%s", array[0]);
            } else if (!(*prefs->encoder.of_name)) {
              lives_snprintf(prefs->encoder.of_name, 64, "%s", array[0]);
            }

            lives_strfreev(array);
          }
        }

        set_string_pref(PREF_OUTPUT_TYPE, prefs->encoder.of_name);

        lives_list_free_all(&ofmt_all);
      }
    }

    if (!(*prefs->encoder.of_name)) {
      get_string_pref(PREF_ENCODER, prefs->encoder.name, 64);
      get_string_pref(PREF_OUTPUT_TYPE, prefs->encoder.of_name, 64);
    }

    future_prefs->encoder.audio_codec = prefs->encoder.audio_codec = get_int64_prefd(PREF_ENCODER_ACODEC, -1);
    prefs->encoder.capabilities = 0;
    prefs->encoder.of_allowed_acodecs = AUDIO_CODEC_UNKNOWN;

    lives_snprintf(future_prefs->encoder.name, 64, "%s", prefs->encoder.name);

    lives_memset(future_prefs->encoder.of_restrict, 0, 1);
    lives_memset(prefs->encoder.of_restrict, 0, 1);

    if (capable->has_encoder_plugins == PRESENT) {
      char **array;
      int numtok;
      LiVESList *ofmt_all, *dummy_list;

      dummy_list = plugin_request("encoders", prefs->encoder.name, "init");
      lives_list_free_all(&dummy_list);

      if (((encoder_capabilities = plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "get_capabilities")) != NULL)) {
        prefs->encoder.capabilities = atoi((char *)lives_list_nth_data(encoder_capabilities, 0));
        lives_list_free_all(&encoder_capabilities);
        if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, prefs->encoder.name, "get_formats")) != NULL) {
          // get any restrictions for the current format
          LiVESList *list = ofmt_all;
          while (list) {
            if ((numtok = get_token_count((char *)list->data, '|')) > 2) {
              array = lives_strsplit((char *)list->data, "|", -1);
              if (!strcmp(array[0], prefs->encoder.of_name)) {
                if (numtok > 1) {
                  lives_snprintf(prefs->encoder.of_desc, 128, "%s", array[1]);
                }
                lives_strfreev(array);
                break;
              }
              lives_strfreev(array);
            }
            list = list->next;
          }
          lives_list_free_all(&ofmt_all);
        }
      }
    }

    get_utf8_pref(PREF_VID_LOAD_DIR, prefs->def_vid_load_dir, PATH_MAX);
    if (!(*prefs->def_vid_load_dir)) {
#ifdef USE_GLIB
#if GLIB_CHECK_VERSION(2, 14, 0)
      lives_snprintf(prefs->def_vid_load_dir, PATH_MAX, "%s", g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS));
#else
      lives_snprintf(prefs->def_vid_load_dir, PATH_MAX, "%s", capable->home_dir);
#endif
#endif
      set_utf8_pref(PREF_VID_LOAD_DIR, prefs->def_vid_load_dir);
    }
    lives_snprintf(mainw->vid_load_dir, PATH_MAX, "%s", prefs->def_vid_load_dir);
    ensure_isdir(mainw->vid_load_dir);

    get_utf8_pref(PREF_VID_SAVE_DIR, prefs->def_vid_save_dir, PATH_MAX);
    if (!(*prefs->def_vid_save_dir)) {
#ifdef USE_GLIB
#if GLIB_CHECK_VERSION(2, 14, 0)
      lives_snprintf(prefs->def_vid_save_dir, PATH_MAX, "%s", g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS));
#else
      lives_snprintf(prefs->def_vid_save_dir, PATH_MAX, "%s", capable->home_dir);
#endif
#endif
      set_utf8_pref(PREF_VID_SAVE_DIR, prefs->def_vid_save_dir);
    }
    lives_snprintf(mainw->vid_save_dir, PATH_MAX, "%s", prefs->def_vid_save_dir);
    ensure_isdir(mainw->vid_save_dir);

    lives_snprintf(mainw->vid_dl_dir, PATH_MAX, "%s", mainw->vid_save_dir);

    get_utf8_pref(PREF_AUDIO_DIR, prefs->def_audio_dir, PATH_MAX);
    if (!(*prefs->def_audio_dir)) {
#ifdef USE_GLIB
#if GLIB_CHECK_VERSION(2, 14, 0)
      lives_snprintf(prefs->def_audio_dir, PATH_MAX, "%s", g_get_user_special_dir(G_USER_DIRECTORY_MUSIC));
#else
      lives_snprintf(prefs->def_audio_dir, PATH_MAX, "%s", capable->home_dir);
#endif
#endif
      set_utf8_pref(PREF_AUDIO_DIR, prefs->def_audio_dir);
    }
    lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", prefs->def_audio_dir);
    ensure_isdir(mainw->audio_dir);

    get_utf8_pref(PREF_IMAGE_DIR, prefs->def_image_dir, PATH_MAX);
    if (!(*prefs->def_image_dir)) {
#ifdef USE_GLIB
#if GLIB_CHECK_VERSION(2, 14, 0)
      lives_snprintf(prefs->def_image_dir, PATH_MAX, "%s", g_get_user_special_dir(G_USER_DIRECTORY_PICTURES));
#else
      lives_snprintf(prefs->def_image_dir, PATH_MAX, "%s", capable->home_dir);
#endif
#endif
      set_utf8_pref(PREF_IMAGE_DIR, prefs->def_image_dir);
    }
    lives_snprintf(mainw->image_dir, PATH_MAX, "%s", prefs->def_image_dir);
    ensure_isdir(mainw->image_dir);

    get_utf8_pref(PREF_PROJ_DIR, prefs->def_proj_dir, PATH_MAX);
    if (!(*prefs->def_proj_dir)) {
      lives_snprintf(prefs->def_proj_dir, PATH_MAX, "%s", capable->home_dir);
      set_utf8_pref(PREF_PROJ_DIR, prefs->def_proj_dir);
    }
    lives_snprintf(mainw->proj_load_dir, PATH_MAX, "%s", prefs->def_proj_dir);
    ensure_isdir(mainw->proj_load_dir);
    lives_snprintf(mainw->proj_save_dir, PATH_MAX, "%s", mainw->proj_load_dir);

    prefs->show_player_stats = get_boolean_prefd(PREF_SHOW_PLAYER_STATS, FALSE);

    prefs->dl_bandwidth = get_int_prefd(PREF_DL_BANDWIDTH_K, DEF_DL_BANDWIDTH);
    prefs->fileselmax = get_boolean_prefd(PREF_FILESEL_MAXIMISED, TRUE);

    prefs->midisynch = get_boolean_prefd(PREF_MIDISYNCH, FALSE);
    if (prefs->midisynch && !capable->has_midistartstop) {
      set_boolean_pref(PREF_MIDISYNCH, FALSE);
      prefs->midisynch = FALSE;
    }

    prefs->discard_tv = FALSE;

    // conserve disk space ?
    prefs->conserve_space = get_boolean_prefd(PREF_CONSERVE_SPACE, FALSE);
    prefs->ins_resample = get_boolean_prefd(PREF_INSERT_RESAMPLE, TRUE);

    // need better control of audio channels first
    prefs->pause_during_pb = FALSE;

    // should we always use the last directory ?
    // TODO - add to GUI
    prefs->save_directories = get_boolean_prefd(PREF_SAVE_DIRECTORIES, FALSE);
    prefs->antialias = get_boolean_prefd(PREF_ANTIALIAS, TRUE);

    prefs->concat_images = get_boolean_prefd(PREF_CONCAT_IMAGES, TRUE);

    prefs->fxdefsfile = NULL;
    prefs->fxsizesfile = NULL;

    if (!needs_workdir && initial_startup_phase == 0) {
      disk_monitor_start(prefs->workdir);
    }

    // anything that d_prints messages should go here:
    do_start_messages();

    needs_free = FALSE;
    get_string_pref(PREF_WEED_PLUGIN_PATH, prefs->weed_plugin_path, PATH_MAX);
    if (!*prefs->weed_plugin_path) {
      weed_plugin_path = getenv("WEED_PLUGIN_PATH");
      if (!weed_plugin_path) {
        char *ppath1 = lives_build_path(prefs->lib_dir, PLUGIN_EXEC_DIR,
                                        PLUGIN_WEED_FX_BUILTIN, NULL);
        char *ppath2 = lives_build_path(capable->home_dir, LOCAL_HOME_DIR, LIVES_LIB_DIR, PLUGIN_EXEC_DIR,
                                        PLUGIN_WEED_FX_BUILTIN, NULL);
        weed_plugin_path = lives_strdup_printf("%s:%s", ppath1, ppath2);
        lives_free(ppath1); lives_free(ppath2);
        needs_free = TRUE;
      }
      lives_snprintf(prefs->weed_plugin_path, PATH_MAX, "%s", weed_plugin_path);
      if (needs_free) lives_free(weed_plugin_path);
      set_string_pref(PREF_WEED_PLUGIN_PATH, prefs->weed_plugin_path);
    }
    lives_setenv("WEED_PLUGIN_PATH", prefs->weed_plugin_path);

#ifdef HAVE_FREI0R
    needs_free = FALSE;
    get_string_pref(PREF_FREI0R_PATH, prefs->frei0r_path, PATH_MAX);
    if (!*prefs->frei0r_path) {
      frei0r_path = getenv("FREI0R_PATH");
      if (!frei0r_path) {
        char *fp0 = lives_build_path(LIVES_USR_DIR, LIVES_LIB_DIR, FREI0R1_LITERAL, NULL);
        char *fp1 = lives_build_path(LIVES_USR_DIR, LIVES_LOCAL_DIR, LIVES_LIB_DIR, FREI0R1_LITERAL, NULL);
        char *fp2 = lives_build_path(capable->home_dir, FREI0R1_LITERAL, NULL);
        frei0r_path =
          lives_strdup_printf("%s:%s:%s", fp0, fp1, fp2);
        lives_free(fp0); lives_free(fp1); lives_free(fp2);
        needs_free = TRUE;
      }
      lives_snprintf(prefs->frei0r_path, PATH_MAX, "%s", frei0r_path);
      if (needs_free) lives_free(frei0r_path);
      set_string_pref(PREF_FREI0R_PATH, prefs->frei0r_path);
    }
    lives_setenv("FREI0R_PATH", prefs->frei0r_path);
#endif

#if HAVE_LADSPA
    needs_free = FALSE;
    get_string_pref(PREF_LADSPA_PATH, prefs->ladspa_path, PATH_MAX);
    if (!*prefs->ladspa_path) {
      ladspa_path = getenv("LADSPA_PATH");
      if (!ladspa_path) {
        ladspa_path = lives_build_path(LIVES_USR_DIR, LIVES_LIB_DIR, LADSPA_LITERAL, NULL);
        needs_free = TRUE;
      }
      lives_snprintf(prefs->ladspa_path, PATH_MAX, "%s", ladspa_path);
      if (needs_free) lives_free(ladspa_path);
      set_string_pref(PREF_LADSPA_PATH, prefs->ladspa_path);
    }
    lives_setenv("LADSPA_PATH", prefs->ladspa_path);
#endif

#if HAVE_LIBVISUAL
    needs_free = FALSE;
    get_string_pref(PREF_LIBVISUAL_PATH, prefs->libvis_path, PATH_MAX);
    if (!*prefs->libvis_path) {
      libvis_path = getenv("VISUAL_PLUGIN_PATH");
      if (!libvis_path) {
        libvis_path = "";
      }
      lives_snprintf(prefs->libvis_path, PATH_MAX, "%s", libvis_path);
      set_string_pref(PREF_LIBVISUAL_PATH, prefs->libvis_path);
    }
    lives_setenv("VISUAL_PLUGIN_PATH", prefs->libvis_path);
#endif

    splash_msg(_("Loading realtime effect plugins..."), SPLASH_LEVEL_LOAD_RTE);
    weed_load_all();

    // replace any multi choice effects with their delegates
    replace_with_delegates();

    threaded_dialog_spin(0.);
    load_default_keymap();
    threaded_dialog_spin(0.);

    if (ARE_UNCHECKED(decoder_plugins)) load_decoders();

    future_prefs->audio_opts = prefs->audio_opts =
                                 get_int_prefd(PREF_AUDIO_OPTS,
                                     AUDIO_OPTS_EXT_FX | AUDIO_OPTS_FOLLOW_CLIPS | AUDIO_OPTS_FOLLOW_FPS
                                     | AUDIO_OPTS_UNLOCK_RESYNC | AUDIO_OPTS_LOCKED_RESET);

    array = lives_strsplit(DEF_AUTOTRANS, "|", 3);
    mainw->def_trans_idx = weed_filter_highest_version(array[0], array[1], array[2], NULL);
    if (mainw->def_trans_idx == - 1) {
      msg = lives_strdup_printf(_("System default transition (%s from package %s by %s) not found."),
                                array[1], array[0], array[2]);
      LIVES_WARN(msg);
      lives_free(msg);
    }
    lives_strfreev(array);

    get_string_prefd(PREF_ACTIVE_AUTOTRANS, buff, 256, DEF_AUTOTRANS);
    if (!strcmp(buff, "none")) prefs->atrans_fx = -1;
    else {
      if (!lives_utf8_strcasecmp(buff, DEF_AUTOTRANS) || get_token_count(buff, '|') < 3)
        prefs->atrans_fx = mainw->def_trans_idx;
      else {
        array = lives_strsplit(buff, "|", 3);
        prefs->atrans_fx = weed_filter_highest_version(array[0], array[1], array[2], NULL);
        if (prefs->atrans_fx == - 1) {
          msg = lives_strdup_printf(_("User default transition (%s from package %s by %s) not found."),
                                    array[1], array[0], array[2]);
          LIVES_WARN(msg);
          lives_free(msg);
        }
        lives_strfreev(array);
      }
    }

    if (prefs->startup_phase == 0) mainw->first_shown = TRUE;

    recfname = lives_strdup_printf("%s.%d.%d.%d", RECOVERY_LITERAL, lives_getuid(), lives_getgid(),
                                   capable->mainpid);
    mainw->recovery_file = lives_build_filename(prefs->workdir, recfname, NULL);
    lives_free(recfname);

#ifdef ENABLE_JACK
audio_choice:
    orig_err = 0;
#endif

    if (prefs->startup_phase > 0 && prefs->startup_phase <= 4) {
      splash_end();
      lives_widget_context_update();
      if (!do_audio_choice_dialog(prefs->startup_phase)) {
        prefs->startup_phase = 3;
        set_int_pref(PREF_STARTUP_PHASE, 3);
        lives_exit(0);
      }


post_audio_choice:
      prefs->startup_phase = 4;
      set_int_pref(PREF_STARTUP_PHASE, 4);
#ifdef ENABLE_JACK
      future_prefs->jack_opts |= JACK_INFO_TEST_SETUP;
#endif
    }

    // audio startup
#ifdef ENABLE_JACK

#ifdef ENABLE_JACK_TRANSPORT
    if (prefs->jack_srv_dup) {
      if (!ign_opts->ign_jackopts) {
        if (prefs->jack_opts & JACK_OPTS_PERM_ASERVER)
          prefs->jack_opts |= JACK_OPTS_PERM_TSERVER;
        else
          prefs->jack_opts &= ~JACK_OPTS_PERM_TSERVER;
      }
    }

jack_tcl_try:
    if (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) {
      // start jack transport polling
      splash_msg(_("Connecting to jack transport server..."),
                 SPLASH_LEVEL_LOAD_APLAYER);
      success = TRUE;
      timeout = LIVES_SHORT_TIMEOUT;
      if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) timeout <<= 2;
      if (!(info = LPT_WITH_TIMEOUT(timeout, 0, lives_jack_init, WEED_SEED_BOOLEAN, "iv",
                                    JACK_CLIENT_TYPE_TRANSPORT, NULL))) {
        if (mainw->cancelled) {
          lives_exit(0);
        }
        return FALSE;
      }
      success = lives_proc_thread_join_boolean(info);
      lives_proc_thread_free(info);

      if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) {
        if (prefs->startup_phase) {
          if (textwindow) {
            lives_widget_set_sensitive(textwindow->button, TRUE);
            lives_grab_add(textwindow->button);
            lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
            lives_widget_destroy(textwindow->dialog);
            lives_free(textwindow);
            lives_widget_context_update();
          }
        } else {
          if (textwindow) {
            lives_widget_set_sensitive(textwindow->button, TRUE);
            lives_grab_add(textwindow->button);
            lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
            lives_widget_destroy(textwindow->dialog);
            lives_free(textwindow);
            if (orig_err) success = FALSE;
	    // *INDENT-OFF*
          }}}
      // *INDENT-ON*
      if (!success) {
        if (mainw->cancelled == CANCEL_USER) {
          if (prefs->startup_phase) goto audio_choice;
          goto jack_tcl_try;
        }
        if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
        if (prefs->jack_opts & JACK_OPTS_START_TSERVER) {
rest2:
          orig_err = 0;
          if (do_jack_no_startup_warn(TRUE)) {
            orig_err = 2;
            goto jack_tcl_try;
          }
          if (prefs->startup_phase) goto audio_choice;
        } else {
rest1:
          orig_err = 0;
          if (do_jack_no_connect_warn(TRUE)) {
            orig_err = 1;
            goto jack_tcl_try;
          }
          if (prefs->startup_phase) goto audio_choice;
        }
        // disable transport for now
        future_prefs->jack_opts &= ~JACK_OPTS_ENABLE_TCLIENT;
        set_int_pref(PREF_JACK_OPTS, future_prefs->jack_opts);
        lives_exit(0);
      }
      if (ign_opts->ign_jackopts) set_int_pref(PREF_JACK_OPTS, prefs->jack_opts);

      if ((prefs->jack_opts & JACK_INFO_TEMP_NAMES)
          || lives_strcmp(future_prefs->jack_tdriver, prefs->jack_tdriver)) {
        if (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) {
          // if the driver has changed, update prefs
          if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) {
            pref_factory_string(PREF_JACK_LAST_TDRIVER, future_prefs->jack_tdriver, TRUE);
            pref_factory_string(PREF_JACK_LAST_TSERVER, future_prefs->jack_tserver_sname, TRUE);
          } else pref_factory_string(PREF_JACK_TDRIVER, future_prefs->jack_tdriver, TRUE);
        }
        if (prefs->jack_srv_dup) {
          if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) {
            // cross setting
            pref_factory_string(PREF_JACK_LAST_ADRIVER, future_prefs->jack_tdriver, TRUE);
            pref_factory_string(PREF_JACK_LAST_ASERVER, future_prefs->jack_tserver_sname, TRUE);
          } else pref_factory_string(PREF_JACK_ADRIVER, future_prefs->jack_tdriver, TRUE);
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*

#endif

    if (prefs->audio_player == AUD_PLAYER_JACK) {
      splash_msg(_("Connecting to jack audio server..."),
                 SPLASH_LEVEL_LOAD_APLAYER);

      // set config for (LiVES) clients
      jack_audio_init();

      // get first OUTPUT driver
      mainw->jackd = jack_get_driver(0, TRUE);

      if (mainw->jackd) {
        /// try to connect, and possibly start a server
        // activate the writer and connect ports
jack_acl_try:
        success = TRUE;
        timeout = LIVES_SHORTEST_TIMEOUT;
        if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) timeout <<= 2;
        if (!(info = LPT_WITH_TIMEOUT(timeout, 0,
                                      (lives_funcptr_t)jack_create_client_writer,
                                      WEED_SEED_BOOLEAN, "v", mainw->jackd))) {
          return FALSE;
        }
        success = lives_proc_thread_join_boolean(info);
        lives_proc_thread_free(info);

        if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) {
          // dont clear TEST till here
          if (prefs->startup_phase) {
            if (success) {
              jack_audio_read_init();
              jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);
            }
            if (textwindow) {
              lives_widget_set_sensitive(textwindow->button, TRUE);
              lives_grab_add(textwindow->button);
              lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
              lives_widget_destroy(textwindow->dialog);
              lives_free(textwindow);
              textwindow = NULL;
              lives_widget_context_update();
            }
            if (success) prompt_for_jack_ports(TRUE);
          } else {
            if (textwindow) {
              lives_widget_set_sensitive(textwindow->button, TRUE);
              lives_grab_add(textwindow->button);
              lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
              lives_widget_destroy(textwindow->dialog);
              lives_free(textwindow);
              textwindow = NULL;
              if (orig_err) success = FALSE;
            }
          }
        } else {
          if (mainw->jackd) {
            if (!mainw->jackd->sample_out_rate) success = FALSE;
            else {
              mainw->jackd->whentostop = &mainw->whentostop;
              mainw->jackd->cancelled = &mainw->cancelled;
              mainw->jackd->in_use = FALSE;
              if (!(info = LPT_WITH_TIMEOUT(timeout, 0,
                                            (lives_funcptr_t)jack_write_client_activate,
                                            WEED_SEED_BOOLEAN, "v", mainw->jackd))) {
                success = FALSE;
              } else {
                success = lives_proc_thread_join_boolean(info);
                lives_proc_thread_free(info);
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*

        if (!success || !mainw->jackd) {
          if (mainw->cancelled == CANCEL_USER) {
            if (prefs->startup_phase) {
              prefs->startup_phase = 4;
              goto audio_choice;
            }
            if (mainw->jackd) goto jack_acl_try;
          }
          // failed to connect
          if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
          if (prefs->jack_opts & JACK_OPTS_START_ASERVER) {
            // if we have backup config, allow restore
rest4:
            orig_err = 0;
            if (do_jack_no_startup_warn(FALSE)) {
              if (prefs->startup_phase) {
                prefs->startup_phase = 4;
                goto audio_choice;
              }
              orig_err = 4;
              if (mainw->jackd) goto jack_acl_try;
            }
            if (prefs->startup_phase) {
              prefs->startup_phase = 4;
              goto audio_choice;
            }
            future_prefs->jack_opts = 0; // jack is causing hassle, disable it for now
            set_int_pref(PREF_JACK_OPTS, 0);
          } else {
rest3:
            orig_err = 0;
            if (do_jack_no_connect_warn(FALSE)) {
              if (prefs->startup_phase) {
                prefs->startup_phase = 4;
                goto audio_choice;
              }
              orig_err = 3;
              if (mainw->jackd) goto jack_acl_try;
            }
            if (prefs->startup_phase) {
              prefs->startup_phase = 4;
              goto audio_choice;
            }
          }
          lives_exit(0);
        }

        if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) {
          future_prefs->jack_opts &= ~JACK_INFO_TEST_SETUP;
          lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", prefs->jack_aserver_cfg);
          lives_snprintf(future_prefs->jack_aserver_cname, PATH_MAX, "%s", prefs->jack_aserver_cname);
          lives_snprintf(future_prefs->jack_aserver_sname, PATH_MAX, "%s", prefs->jack_aserver_sname);
          future_prefs->jack_adriver = lives_strdup_free(future_prefs->jack_adriver, prefs->jack_adriver);

          lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", prefs->jack_tserver_cfg);
          lives_snprintf(future_prefs->jack_tserver_cname, PATH_MAX, "%s", prefs->jack_tserver_cname);
          lives_snprintf(future_prefs->jack_tserver_sname, PATH_MAX, "%s", prefs->jack_tserver_sname);
          future_prefs->jack_tdriver = lives_strdup_free(future_prefs->jack_tdriver, prefs->jack_tdriver);
          future_prefs->jack_opts = prefs->jack_opts;
          jack_read_start = TRUE;
          if (!orig_err) goto jack_acl_try;
          if (orig_err == 1) goto rest1;
          if (orig_err == 2) goto rest2;
          if (orig_err == 3) goto rest3;
          if (orig_err == 4) goto rest4;
        }

        if (ign_opts->ign_jackopts) set_int_pref(PREF_JACK_OPTS, prefs->jack_opts);

        if ((prefs->jack_opts & JACK_INFO_TEMP_NAMES)
            || lives_strcmp(future_prefs->jack_adriver, prefs->jack_adriver)) {
          // if the driver has changed, update prefs
          if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) {
            pref_factory_string(PREF_JACK_LAST_ADRIVER, future_prefs->jack_adriver, TRUE);
            pref_factory_string(PREF_JACK_LAST_ASERVER, future_prefs->jack_aserver_sname, TRUE);
          } else pref_factory_string(PREF_JACK_ADRIVER, future_prefs->jack_adriver, TRUE);

          if (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) {
            if (prefs->jack_srv_dup) {
              if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) {
                pref_factory_string(PREF_JACK_LAST_TDRIVER, future_prefs->jack_adriver, TRUE);
                pref_factory_string(PREF_JACK_LAST_TSERVER, future_prefs->jack_aserver_sname, TRUE);
              } else pref_factory_string(PREF_JACK_TDRIVER, future_prefs->jack_adriver, TRUE);
	      // *INDENT-OFF*
            }}}
	// *INDENT-ON*

        if (!mainw->jackd_read) {
          // connect the reader - will also attempt to connect, and possibly start a server
          // however the server should always be running since we already connected the writer client
          // also activates the client, but since the reader is not attatched to any clip
          // (first param is -1), we do not actually prepare for recording yet
          // however we do connect the ports (unless JACK_OPTS_NO_READ_AUTOCON is set)
          // so if needed we can still monitor incoming audio
          jack_audio_read_init();
          jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);
        }

        if (jack_read_start) {
          jack_create_client_reader(mainw->jackd_read);
          jack_read_client_activate(mainw->jackd_read, FALSE);
        }

        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_j, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(jack_interop_callback), (livespointer)mainw->jackd, NULL));

        if (prefs->startup_phase) {
          set_string_pref(PREF_JACK_INPORT_CLIENT, prefs->jack_inport_client);
          set_string_pref(PREF_JACK_OUTPORT_CLIENT, prefs->jack_outport_client);
          set_string_pref(PREF_JACK_AUXPORT_CLIENT, prefs->jack_auxport_client);
        }
      } // if (mainw->jackd)
    }
#endif
  }
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    splash_msg(_("Starting pulseaudio server..."), SPLASH_LEVEL_LOAD_APLAYER);

    if (!mainw->foreign) {
      if (prefs->pa_restart && !prefs->vj_mode) {
        char *com = lives_strdup_printf("%s -k %s", EXEC_PULSEAUDIO, prefs->pa_start_opts);
        lives_system(com, TRUE);
        lives_free(com);
      }
    }

    if (!lives_pulse_init(prefs->startup_phase)) {
      if (prefs->startup_phase == 4) {
        lives_exit(0);
      }
    } else {
      pulse_audio_init();
      pulse_audio_read_init();
      mainw->pulsed = pulse_get_driver(TRUE);
      mainw->pulsed->whentostop = &mainw->whentostop;
      mainw->pulsed->cancelled = &mainw->cancelled;
      mainw->pulsed->in_use = FALSE;

      pulse_driver_activate(mainw->pulsed);

      // create reader connection now, if permanent
      pulse_rec_audio_to_clip(-1, -1, RECA_MONITOR);
      // *INDENT-OFF*
    }}
  // *INDENT-ON*
#endif

#ifdef ENABLE_JACK
  if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) {
    future_prefs->jack_opts &= ~JACK_INFO_TEST_SETUP;
  }
#endif

  reset_font_size();

  if (prefs->startup_phase != 0) {
    // splash_end() would normally kick us to MT mode, but we haven't queried for it yet
    // splash_end();
    set_int_pref(PREF_STARTUP_PHASE, 5);
    prefs->startup_phase = 5;
    if (!do_setup_interface_query()) {
      set_int_pref(PREF_STARTUP_PHASE, prefs->startup_phase);
      lives_exit(0);
    }
    if (prefs->startup_phase == 4) goto post_audio_choice;

    set_int_pref(PREF_STARTUP_PHASE, 6);
    prefs->startup_phase = 6;

    //reset_font_size();
    mainw->first_shown = TRUE;

    if (prefs->show_disk_quota && !prefs->vj_mode) {
      if (!disk_monitor_running(prefs->workdir))
        disk_monitor_start(prefs->workdir);

      capable->ds_used = disk_monitor_wait_result(prefs->workdir, LIVES_DEFAULT_TIMEOUT);
      if (capable->ds_used >= 0) {
        ran_ds_dlg = TRUE;
        run_diskspace_dialog(NULL);
      } else {
        disk_monitor_forget();
        if (prefs->show_disk_quota)
          mainw->helper_procthreads[PT_LAZY_DSUSED] = disk_monitor_start(prefs->workdir);
      }
    } else {
      disk_monitor_forget();
    }

    set_int_pref(PREF_STARTUP_PHASE, 100); // tell backend to delete this
    prefs->startup_phase = 100;
  }

  if (strcmp(future_prefs->theme, prefs->theme)) {
    // we set the theme here in case it got reset to 'none'
    set_string_pref(PREF_GUI_THEME, prefs->theme);
    lives_snprintf(future_prefs->theme, 64, "%s", prefs->theme);
  }

  if (mainw->vpp && mainw->vpp->get_audio_fmts) mainw->vpp->audio_codec = get_best_audio(mainw->vpp);
  return TRUE;
} // end of lives_init


static void show_detected_or_not(boolean cap, const char *pname) {
  if (cap) d_print(_("%s...detected... "), pname);
  else d_print(_("%s...NOT DETECTED... "), pname);

}

#define SHOWDETx(cap, exec) show_detected_or_not(capable->has_##cap, exec)
#define SHOWDET_EXEC(cap, exec) SHOWDETx(cap, EXEC_##exec)
#define SHOWDET_ALTS(check1, exec1, check2, exec2) if (!check_for_executable(&capable->has_##check1, EXEC_##exec1) \
						       && check_for_executable(&capable->has_##check2, EXEC_##exec2)) { \
    SHOWDET_EXEC(check2, exec2);} else SHOWDET_EXEC(check1, exec1)

static void do_start_messages(void) {
  int w, h;
  char *tmp, *endian, *fname, *phase = NULL, *vendorstr = NULL;

  if (prefs->vj_mode) {
    d_print(_("Starting in VJ MODE: Skipping most startup checks\n"));
#ifndef IS_MINGW
    SHOWDET_EXEC(wmctrl, WMCTRL);
    SHOWDET_EXEC(xdotool, XDOTOOL);
    SHOWDET_EXEC(xwininfo, XWININFO);
#endif
    d_print("\n\n");
    return;
  }

  d_print(_("\nMachine details:\n"));

  d_print(_("OS is %s %s, running on %s\n"),
          capable->os_name ? capable->os_name : _("unknown"),
          capable->os_release ? capable->os_release : "?",
          capable->os_hardware ? capable->os_hardware : "????");

  if (capable->hw.cpu_vendor) {
    vendorstr = lives_strdup_printf(_(" (VendorID: %s)"), capable->hw.cpu_vendor);
  }

  d_print(_("CPU type is %s%s"), capable->hw.cpu_name, vendorstr ? vendorstr : "");
  if (vendorstr) lives_free(vendorstr);
  d_print(P_(", (%d core, ", ", (%d cores, ", capable->hw.ncpus), capable->hw.ncpus);

  if (capable->hw.byte_order == LIVES_LITTLE_ENDIAN) endian = (_("little endian"));
  else endian = (_("big endian"));
  d_print(_("%d bits, %s)\n"), capable->hw.cpu_bits, endian);
  lives_free(endian);

  if (capable->hw.cpu_features) {
    d_print(_("CPU features detected:"));
    if (capable->hw.cpu_features & CPU_FEATURE_HAS_SSE2) d_print(" SSE2");
    d_print("\n");
  }

  if (capable->hw.cacheline_size > 0) {
    d_print(_("Cacheline size is %d bytes.\n"), capable->hw.cacheline_size);
  }

  d_print(_("Machine name is '%s'\n"), capable->mach_name);

  d_print(_("Number of monitors detected: %d: "), capable->nmonitors);

  d_print(_("GUI screen size is %d X %d (usable: %d X %d); %d dpi.\nWidget scaling has been set to %.3f.\n"),
          mainw->mgeom[widget_opts.monitor].phys_width, mainw->mgeom[widget_opts.monitor].phys_height,
          GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT,
          (int)mainw->mgeom[widget_opts.monitor].dpi,
          widget_opts.scale);

  if (get_screen_usable_size(&w, &h)) {
    d_print(_("Actual usable size appears to be %d X %d\n"), w, h);
  }

  get_wm_caps();

  d_print(_("Window manager reports as \"%s\" (%s)"),
          capable->wm_name ? capable->wm_name : _("UNKNOWN - please patch me !"),
          capable->wm_caps.wm_name ? capable->wm_caps.wm_name : "unknown");

  if (capable->wm_type && *capable->wm_type)
    d_print(_(", running on %s"), capable->wm_type);

  d_print(_("; compositing is %s.\n"), capable->wm_caps.is_composited ? _("supported") : _("not supported"));

  get_distro_dets();

  if (capable->distro_codename) tmp = lives_strdup_printf(" (%s)", capable->distro_codename);
  else tmp = lives_strdup("");

  d_print(_("Distro is %s %s %s\n"), capable->distro_name ? capable->distro_name : "?????",
          capable->distro_ver ? capable->distro_ver : "?????", tmp);
  lives_free(tmp);

  d_print("%s", _("GUI type is: "));

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  d_print(_("GTK+ version %d.%d.%d (compiled with %d.%d.%d)"),
          gtk_get_major_version(), gtk_get_minor_version(),
          gtk_get_micro_version(),
          GTK_MAJOR_VERSION, GTK_MINOR_VERSION,
          GTK_MICRO_VERSION
         );
#else
  d_print(_("GTK+ (compiled with %d.%d.%d)"),
          GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
#endif
#endif

#ifdef LIVES_PAINTER_IS_CAIRO
  d_print(_(", with cairo support\n"));
#else
  d_print(_("\n"));
#endif

  if (*capable->gui_theme_name) tmp = lives_strdup(capable->gui_theme_name);
  else tmp = lives_strdup_printf("lives-%s-dynamic", prefs->theme);

  d_print("GUI theme set to %s, icon theme set to %s\n", tmp,
          capable->icon_theme_name);

  lives_free(tmp);


#ifndef RT_AUDIO
  d_print(_("WARNING - this version of LiVES was compiled without either\njack or pulseaudio support.\n"
            "Many audio features will be unavailable.\n"));
# else
#ifdef ENABLE_JACK
  d_print(_("Compiled with jack support, good !\n"));
#endif
#ifdef HAVE_PULSE_AUDIO
  d_print(_("Compiled with pulseaudio support, wonderful !\n"));
#endif
#endif

  if (ign_opts.ign_configfile) {
    tmp = (_("set via -configfile commandline option"));
  } else {
    tmp = (_("default value"));
  }
  d_print(_("\nConfig file is %s (%s)\n"), prefs->configfile, tmp);
  lives_free(tmp);

  if (!capable->mountpoint || !*capable->mountpoint)
    capable->mountpoint = get_mountpoint_for(prefs->workdir);
  if (capable->mountpoint && *capable->mountpoint) tmp = lives_strdup_printf(_(", contained in volume %s"), capable->mountpoint);
  else tmp = lives_strdup("");

  d_print(_("\nWorking directory is %s%s\n"), prefs->workdir, tmp);
  lives_free(tmp);

  if (mainw->has_session_workdir) {
    d_print(_("(Set by -workdir commandline option)\n"));
  } else {
    if (initial_startup_phase != -1) {
      if (!lives_strcmp(mainw->version_hash, mainw->old_vhash)) {
        lives_free(old_vhash);
        old_vhash = lives_strdup(LiVES_VERSION);
      }
      d_print(_("(Retrieved from %s, version %s)\n"), prefs->configfile, old_vhash);
    } else {
      d_print(_("(Set by user during setup phase)\n"));
    }
  }

  if (initial_startup_phase == 0) {
    if (!*mainw->old_vhash || !strcmp(mainw->old_vhash, "0")) {
      phase = (_("STARTUP ERROR OCCURRED - FORCED REINSTALL"));
    } else {
      if (atoi(mainw->old_vhash) < atoi(mainw->version_hash)) {
        phase = lives_strdup_printf(_("upgrade from version %s. Welcome !"), mainw->old_vhash);
      } else if (atoi(mainw->old_vhash) > atoi(mainw->version_hash)) {
        phase = lives_strdup_printf(_("downgrade from version %s !"), mainw->old_vhash);
      }
    }
  } else if (initial_startup_phase == -1) {
    if (!strcmp(mainw->old_vhash, "0")) {
      phase = (_("REINSTALL AFTER FAILED RECOVERY"));
      fname = lives_strdup_printf("%s.damaged", prefs->configfile);
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        tmp = lives_strdup_printf(_("%s; check %s for possible errors before re-running LiVES"), phase, fname);
        lives_free(phase);
        phase = tmp;
      }
      lives_free(fname);
      d_print("\n");
    } else {
      phase = (_("fresh install. Welcome !"));
    }
  } else {
    phase = lives_strdup_printf(_("continue with installation"), initial_startup_phase);
  }
  if (!phase) phase = (_("normal startup"));
  d_print(_("Initial startup phase was %d: (%s)\n"), initial_startup_phase, phase);
  lives_free(phase);
  lives_free(old_vhash);

  if (initial_startup_phase == 0) {
    char *fname = lives_strdup_printf("%s.%s.tried.succeeded", prefs->configfile, RECOVERY_LITERAL);
    if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
      phase = lives_strdup_printf(_("%s WAS POSSIBLY RECOVERED FROM %s.\n"),
                                  prefs->configfile, prefs->configfile, RECOVERY_LITERAL);
      d_print("%s", phase);
      lives_free(phase);
    }
    lives_free(fname);
  }

  d_print(_("\nChecking RECOMMENDED dependencies: "));

  SHOWDET_ALTS(mplayer, MPLAYER, mplayer2, MPLAYER2);
  if (!capable->has_mplayer && !capable->has_mplayer2) {
    SHOWDET_EXEC(mpv, MPV);
  }
  //SHOWDET(file);
  SHOWDET_EXEC(identify, IDENTIFY);
  if (!capable->has_jackd)
    SHOWDET_EXEC(pulse_audio, PULSEAUDIO);
  SHOWDET_EXEC(sox_sox, SOX);
  SHOWDET_EXEC(convert, CONVERT);
  SHOWDET_EXEC(composite, COMPOSITE);
  SHOWDET_EXEC(ffprobe, FFPROBE);
  SHOWDET_EXEC(gzip, GZIP);
  SHOWDET_EXEC(md5sum, MD5SUM);
  SHOWDET_ALTS(youtube_dl, YOUTUBE_DL, youtube_dlc, YOUTUBE_DLC);

  d_print(_("\n\nChecking OPTIONAL dependencies: "));
  SHOWDET_EXEC(jackd, JACKD);
  SHOWDET_ALTS(python, PYTHON, python3, PYTHON3);
  SHOWDET_EXEC(xwininfo, XWININFO);
  SHOWDET_ALTS(icedax, ICEDAX, cdda2wav, CDDA2WAV);
  SHOWDET_EXEC(dvgrab, DVGRAB);
  SHOWDET_EXEC(gdb, GDB);
  SHOWDET_EXEC(gconftool_2, GCONFTOOL_2);
  SHOWDET_EXEC(xdg_screensaver, XDG_SCREENSAVER);

  d_print("\n\n");
}
#undef SHOWDETx
#undef SHOWDET_EXEC
#undef SHOWDET_ALTS




static void set_toolkit_theme(int prefer) {
  char *lname, *ic_dir;

  lives_widget_object_get(gtk_settings_get_default(), "gtk-double-click-time", &capable->dclick_time);
  if (capable->dclick_time <= 0) capable->dclick_time = LIVES_DEF_DCLICK_TIME;

  lives_widget_object_get(gtk_settings_get_default(), "gtk-double-click-distance", &capable->dclick_dist);
  if (capable->dclick_dist <= 0) capable->dclick_dist = LIVES_DEF_DCLICK_DIST;

  // default unless overwritten by pref
  lives_widget_object_get(gtk_settings_get_default(), "gtk-font-name", &capable->def_fontstring);

  lives_widget_object_get(gtk_settings_get_default(), "gtk-alternative-button-order", &widget_opts.alt_button_order);

  lives_widget_object_get(gtk_settings_get_default(), "gtk-icon-theme-name", &capable->icon_theme_name);
  lives_widget_object_get(gtk_settings_get_default(), "gtk-theme-name", &capable->gui_theme_name);

  if (prefer & LIVES_THEME_DARK) {
    lives_widget_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE);
  }

  if (prefer & USE_LIVES_THEMEING) {
    lives_widget_object_set(gtk_settings_get_default(), "gtk-theme-name", "");

    lname = lives_strdup("-lives-hybrid");
    capable->icon_theme_name = lives_concat(capable->icon_theme_name, lname);

    lname = lives_strdup("-lives-hybrid-dynamic");
    capable->gui_theme_name = lives_concat(capable->gui_theme_name, lname);
  }

  capable->extra_icon_path = lives_build_path(prefs->config_datadir, STOCK_ICON_DIR, NULL);
  capable->app_icon_path = lives_build_path(prefs->prefix_dir, APP_ICON_DIR, NULL);

  widget_opts.icon_theme = gtk_icon_theme_new();

  gtk_icon_theme_set_custom_theme((LiVESIconTheme *)widget_opts.icon_theme, capable->icon_theme_name);
  gtk_icon_theme_prepend_search_path((LiVESIconTheme *)widget_opts.icon_theme, capable->extra_icon_path);
  gtk_icon_theme_prepend_search_path((LiVESIconTheme *)widget_opts.icon_theme, capable->app_icon_path);

  ic_dir = lives_build_path(prefs->prefix_dir, DESKTOP_ICON_DIR, NULL);
  gtk_icon_theme_prepend_search_path((LiVESIconTheme *)widget_opts.icon_theme, ic_dir);
  lives_free(ic_dir);

  capable->all_icons = gtk_icon_theme_list_icons((LiVESIconTheme *)widget_opts.icon_theme, NULL);
  if (0) {
    LiVESList *list = capable->all_icons;
    for (; list; list = list->next) if (1 || !strncmp((const char *)list->data, "gtk-", 4))
        g_print("icon: %s\n", (const char *)list->data);
  }

  widget_helper_set_stock_icon_alts((LiVESIconTheme *)widget_opts.icon_theme);
}


#ifndef VALGRIND_ON
static void set_extra_colours(void) {
  if (!mainw->debug && prefs->extra_colours && mainw->pretty_colours) {
    char *colref, *tmp;
    colref = gdk_rgba_to_string(&palette->nice1);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "combobox window menu menuitem", "border-color", colref);

    tmp = lives_strdup_printf("0 -3px %s inset", colref);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_CHECKED, "notebook header tabs *", "box-shadow", tmp);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "menuitem", "box-shadow", tmp);
    lives_free(tmp);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "menu menuitem", "box-shadow", "none");

    set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "menu separator", "background-color", colref);

    set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "paned separator", "background-color", colref);

    set_css_value_direct(NULL, LIVES_WIDGET_STATE_ACTIVE, "scrollbar slider", "background-color", colref);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "scrollbar slider", "background-color", colref);

    tmp = lives_strdup_printf("0 0 0 4px %s inset", colref);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "combobox window menu menuitem", "box-shadow", tmp);
    lives_free(tmp);
    lives_free(colref);
  }
}


static double pick_custom_colours(double var, double timer) {
  ticks_t xtimerinfo, timerinfo, timeout;
  double lmin, lmax;
  uint8_t ncr, ncg, ncb;
  boolean retried = FALSE, fixed = FALSE;
  int ncols = 0;

  if (timer > 0.) fixed = TRUE;
  else timer = -timer;

  if (!(palette->style & STYLE_LIGHT)) {
    lmin = .05; lmax = .25;
  } else {
    lmin = .45; lmax = .6;
  }
retry:
  ncr = palette->menu_and_bars.red * 255.;
  ncg = palette->menu_and_bars.green * 255.;
  ncb = palette->menu_and_bars.blue * 255.;
  prefs->pb_quality = PB_QUALITY_HIGH;

  if (mainw->helper_procthreads[PT_CUSTOM_COLOURS])
    lives_proc_thread_set_cancellable(mainw->helper_procthreads[PT_CUSTOM_COLOURS]);

  timeout = (ticks_t)(timer * TICKS_PER_SECOND_DBL);
  xtimerinfo = lives_get_current_ticks();
  if (pick_nice_colour(timeout, palette->normal_back.red * 255., palette->normal_back.green * 255.,
                       palette->normal_back.blue * 255., &ncr, &ncg, &ncb, .15 * var, .25, .75)) {
    mainw->pretty_colours = TRUE;
    if ((timerinfo = lives_get_current_ticks()) - xtimerinfo < timeout / 5) var *= 1.02;
    //g_print("c1 %ld %ld %f %f\n", timerinfo - xtimerinfo, timeout, (double)(timerinfo - xtimerinfo) / (double)timeout, var);
    if (var > MAX_CPICK_VAR) var = MAX_CPICK_VAR;
    timeout -= (timerinfo - xtimerinfo);
    xtimerinfo = timerinfo;

    // nice1 - used for outlines
    palette->nice1.red = LIVES_WIDGET_COLOR_SCALE_255(ncr);
    palette->nice1.green = LIVES_WIDGET_COLOR_SCALE_255(ncg);
    palette->nice1.blue = LIVES_WIDGET_COLOR_SCALE_255(ncb);
    palette->nice1.alpha = 1.;

    ncr = palette->menu_and_bars.red * 255.;
    ncg = palette->menu_and_bars.green * 255.;
    ncb = palette->menu_and_bars.blue * 255.;

    ncols++;

    if (mainw->helper_procthreads[PT_CUSTOM_COLOURS] &&
        lives_proc_thread_get_cancelled(mainw->helper_procthreads[PT_CUSTOM_COLOURS])) goto windup;

    if (pick_nice_colour(timeout, palette->nice1.red * 255., palette->nice1.green * 255.,
                         palette->nice1.blue * 255., &ncr, &ncg, &ncb, .1 * var, lmin, lmax)) {
      if ((timerinfo = lives_get_current_ticks()) - xtimerinfo < timeout / 2) var *= 1.02;
      //g_print("c2 %ld %ld %f %f\n", timerinfo - xtimerinfo, timeout, (double)(timerinfo - xtimerinfo) / (double)timeout, var);
      timeout -= (timerinfo - xtimerinfo);
      xtimerinfo = timerinfo;
      if (var > MAX_CPICK_VAR) var = MAX_CPICK_VAR;
      // nice2 - alt for menu_and_bars
      // insensitive colour ?
      palette->nice2.red = LIVES_WIDGET_COLOR_SCALE_255(ncr);
      palette->nice2.green = LIVES_WIDGET_COLOR_SCALE_255(ncg);
      palette->nice2.blue = LIVES_WIDGET_COLOR_SCALE_255(ncb);
      palette->nice2.alpha = 1.;
      mainw->pretty_colours = TRUE;
      ncols++;

      if (mainw->helper_procthreads[PT_CUSTOM_COLOURS] &&
          lives_proc_thread_get_cancelled(mainw->helper_procthreads[PT_CUSTOM_COLOURS])) goto windup;

      if (!(palette->style & STYLE_LIGHT)) {
        lmin = .6; lmax = .8;
      } else {
        lmin = .2; lmax = .4;
      }
      if (pick_nice_colour(timeout, palette->normal_fore.red * 255., palette->normal_fore.green * 255.,
                           palette->normal_fore.blue * 255., &ncr, &ncg, &ncb, .1 * var, lmin, lmax)) {
        if ((timerinfo = lives_get_current_ticks()) - xtimerinfo < timeout / 4 * 3) var *= 1.02;
        // nice3 - alt for menu_and_bars_fore
        if (var > MAX_CPICK_VAR) var = MAX_CPICK_VAR;
        //g_print("c3 %ld %ld %f %f\n", timerinfo - xtimerinfo, timeout, (double)(timerinfo - xtimerinfo) / (double)timeout, var);
        palette->nice3.red = LIVES_WIDGET_COLOR_SCALE_255(ncr);
        palette->nice3.green = LIVES_WIDGET_COLOR_SCALE_255(ncg);
        palette->nice3.blue = LIVES_WIDGET_COLOR_SCALE_255(ncb);
        palette->nice3.alpha = 1.;
        ncols++;
      } else if (!fixed) var = -var;
#ifndef VALGRIND_ON
      main_thread_execute_rvoid_pvoid(set_extra_colours);
#endif
    }
  } else {
    if (!retried) {
      retried = TRUE;
      var *= .98;
      goto retry;
    } else lives_widget_color_copy(&palette->nice2, &palette->menu_and_bars);
  }
  return var;

windup:
  if (ncols < 2) lives_widget_color_copy(&palette->nice2, &palette->menu_and_bars);
  palette->nice3.red = palette->nice2.red;
  palette->nice3.green = palette->nice2.green;
  palette->nice3.blue = palette->nice2.blue;
  palette->nice3.alpha = 1.;
  //main_thread_execute(set_extra_colours, 0, NULL, "");
  return var;
}
#endif


boolean set_palette_colours(boolean force_reload) {
  // force_reload should only be set when the theme changes in prefs.
  lives_colRGBA64_t lcol;
  LiVESList *cache_backup;

  char *themedir, *themefile, *othemefile, *fname, *tmp;
  char *pstyle, *colref;

  boolean is_OK = TRUE;
  boolean cached = FALSE;

  lcol.alpha = 65535;

  // set configurable colours and theme colours for the app
  lcol.red = lcol.green = lcol.blue = 0;
  lives_rgba_to_widget_color(&palette->black, &lcol);

  lcol.red = lcol.green = lcol.blue = 65535;
  lives_rgba_to_widget_color(&palette->white, &lcol);

  // salmon
  lcol.red = 63750;
  lcol.green = 32767;
  lcol.blue = 29070;
  lives_rgba_to_widget_color(&palette->light_red, &lcol);

  // SeaGreen3
  lcol.red = 17219;
  lcol.green = 52685;
  lcol.blue = 32896;
  lives_rgba_to_widget_color(&palette->light_green, &lcol);

  // dark red
  lcol.red = 30723;
  lcol.green = 0;
  lcol.blue = 0;
  lives_rgba_to_widget_color(&palette->dark_red, &lcol);

  // darkorange4
  lcol.red = 35723;
  lcol.green = 17733;
  lcol.blue = 0;
  lives_rgba_to_widget_color(&palette->dark_orange, &lcol);

  lives_widget_color_copy(&palette->fade_colour, &palette->black);
  lives_widget_color_copy(&palette->banner_fade_text, &palette->white);

  palette->style = STYLE_PLAIN;

  // defaults
  palette->frame_surround.red = palette->frame_surround.green
                                = palette->frame_surround.blue = palette->frame_surround.alpha = 65535;

  palette->audcol.blue = palette->audcol.red = 16384;
  palette->audcol.green = palette->audcol.alpha = 65535;

  palette->vidcol.red = 0;
  palette->vidcol.green = 16384;
  palette->vidcol.blue = palette->vidcol.alpha = 65535;

  palette->fxcol.red = palette->fxcol.alpha = 65535;
  palette->fxcol.green = palette->fxcol.blue = 0;

  palette->mt_mark.red = palette->mt_mark.green = 0;
  palette->mt_mark.blue = palette->mt_mark.alpha = 65535;

  palette->mt_timeline_reg.red = palette->mt_timeline_reg.green = palette->mt_timeline_reg.blue = 0;
  palette->mt_timeline_reg.alpha = 65535;

  palette->mt_evbox.red = palette->mt_evbox.green = palette->mt_evbox.blue = palette->mt_evbox.alpha = 65535;

  palette->ce_unsel.red = palette->ce_unsel.green = palette->ce_unsel.blue = 0;
  palette->ce_unsel.alpha = 65535;

  palette->ce_sel.red = palette->ce_sel.green = palette->ce_sel.blue = palette->ce_sel.alpha = 65535;

  lives_widget_color_copy(&palette->mt_timecode_bg, &palette->black);
  lives_widget_color_copy(&palette->mt_timecode_fg, &palette->light_green);

  lcol.red = 0;

  // if theme is not "none" and we dont find stuff in prefs then we must reload
  if (!lives_ascii_strcasecmp(future_prefs->theme, LIVES_THEME_NONE)) {
    set_toolkit_theme(0);
    return TRUE;
  } else if (!get_colour_pref(THEME_DETAIL_STYLE, &lcol)) {
    force_reload = TRUE;
  } else {
    // pull our colours from normal prefs
    palette->style = lcol.red;
    if (!(palette->style & STYLE_LIGHT)) {
      if (mainw->sep_image) lives_widget_set_opacity(mainw->sep_image, 0.8);
      if (mainw->multitrack && mainw->multitrack->sep_image)
        lives_widget_set_opacity(mainw->multitrack->sep_image, 0.8);
      palette->ce_unsel.red = palette->ce_unsel.green = palette->ce_unsel.blue = 6554;
      set_toolkit_theme(USE_LIVES_THEMEING | LIVES_THEME_DARK | LIVES_THEME_COMPACT);
    } else {
      set_toolkit_theme(USE_LIVES_THEMEING | LIVES_THEME_COMPACT);
      palette->ce_unsel.red = palette->ce_unsel.green = palette->ce_unsel.blue = 0;
      if (mainw->sep_image) lives_widget_set_opacity(mainw->sep_image, 0.4);
      if (mainw->multitrack && mainw->multitrack->sep_image)
        lives_widget_set_opacity(mainw->multitrack->sep_image, 0.4);
    }
    get_string_pref(THEME_DETAIL_SEPWIN_IMAGE, mainw->sepimg_path, PATH_MAX);
    get_string_pref(THEME_DETAIL_FRAMEBLANK_IMAGE, mainw->frameblank_path, PATH_MAX);

    get_colour_pref(THEME_DETAIL_NORMAL_FORE, &lcol);
    lives_rgba_to_widget_color(&palette->normal_fore, &lcol);

    get_colour_pref(THEME_DETAIL_NORMAL_BACK, &lcol);
    lives_rgba_to_widget_color(&palette->normal_back, &lcol);

    get_colour_pref(THEME_DETAIL_ALT_FORE, &lcol);
    lives_rgba_to_widget_color(&palette->menu_and_bars_fore, &lcol);

    get_colour_pref(THEME_DETAIL_ALT_BACK, &lcol);
    lives_rgba_to_widget_color(&palette->menu_and_bars, &lcol);

    get_colour_pref(THEME_DETAIL_INFO_TEXT, &lcol);
    lives_rgba_to_widget_color(&palette->info_text, &lcol);

    get_colour_pref(THEME_DETAIL_INFO_BASE, &lcol);
    lives_rgba_to_widget_color(&palette->info_base, &lcol);

    // extended colours

    get_colour_pref(THEME_DETAIL_MT_TCFG, &lcol);
    lives_rgba_to_widget_color(&palette->mt_timecode_fg, &lcol);

    get_colour_pref(THEME_DETAIL_MT_TCBG, &lcol);
    lives_rgba_to_widget_color(&palette->mt_timecode_bg, &lcol);

    get_colour_pref(THEME_DETAIL_AUDCOL, &palette->audcol);
    get_colour_pref(THEME_DETAIL_VIDCOL, &palette->vidcol);
    get_colour_pref(THEME_DETAIL_FXCOL, &palette->fxcol);

    get_colour_pref(THEME_DETAIL_MT_TLREG, &palette->mt_timeline_reg);
    get_colour_pref(THEME_DETAIL_MT_MARK, &palette->mt_mark);
    get_colour_pref(THEME_DETAIL_MT_EVBOX, &palette->mt_evbox);

    get_colour_pref(THEME_DETAIL_FRAME_SURROUND, &palette->frame_surround);

    get_colour_pref(THEME_DETAIL_CE_SEL, &palette->ce_sel);
    get_colour_pref(THEME_DETAIL_CE_UNSEL, &palette->ce_unsel);
  }

  if (force_reload) {
    // check if theme is custom:
    themedir = lives_build_path(prefs->config_datadir, PLUGIN_THEMES, prefs->theme, NULL);
    if (!lives_file_test(themedir, LIVES_FILE_TEST_IS_DIR)) {
      lives_free(themedir);
      // if not custom, check if builtin
      themedir = lives_build_path(prefs->prefix_dir, THEME_DIR, prefs->theme, NULL);
      if (!lives_file_test(themedir, LIVES_FILE_TEST_IS_DIR)) {
        if (!mainw->is_ready) {
          lives_free(themedir);
          set_toolkit_theme(0);
          return FALSE;
        }
        is_OK = FALSE;
      }
    }

    fname = lives_strdup_printf("%s.%s", THEME_SEP_IMG_LITERAL, LIVES_FILE_EXT_JPG);
    tmp = lives_build_filename(themedir, fname, NULL);
    lives_free(fname);
    if (lives_file_test(tmp, LIVES_FILE_TEST_EXISTS)) {
      lives_snprintf(mainw->sepimg_path, PATH_MAX, "%s", tmp);
      lives_free(tmp);
    } else {
      fname = lives_strdup_printf("%s.%s", THEME_SEP_IMG_LITERAL, LIVES_FILE_EXT_PNG);
      lives_free(tmp);
      tmp = lives_build_filename(themedir, fname, NULL);
      lives_free(fname);
      lives_snprintf(mainw->sepimg_path, PATH_MAX, "%s", tmp);
      lives_free(tmp);
    }

    fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
    tmp = lives_build_filename(themedir, fname, NULL);
    lives_free(fname);
    if (lives_file_test(tmp, LIVES_FILE_TEST_EXISTS)) {
      lives_snprintf(mainw->frameblank_path, PATH_MAX, "%s", tmp);
      lives_free(tmp);
    } else {
      fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_PNG);
      tmp = lives_build_filename(themedir, fname, NULL);
      lives_free(fname);
      lives_snprintf(mainw->frameblank_path, PATH_MAX, "%s", tmp);
      lives_free(tmp);
    }

    // load from file
    themefile = lives_build_filename(themedir, THEME_HEADER, NULL);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
    lives_free(themefile);
    themefile = lives_build_filename(themedir, THEME_HEADER_2, NULL);
#endif
#endif

    if (!lives_file_test(themefile, LIVES_FILE_TEST_EXISTS)) {
      lives_free(themefile);
      themefile = lives_build_filename(themedir, THEME_HEADER_2, NULL);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
      lives_free(themefile);
      themefile = lives_build_filename(themedir, THEME_HEADER, NULL);
#endif
#endif
      if (!lives_file_test(themefile, LIVES_FILE_TEST_EXISTS)) {
        is_OK = FALSE;
      }
    }

    lives_free(themedir);

    // cache the themefile
    othemefile = themefile;
    cache_backup = mainw->gen_cache;
    if (!(mainw->gen_cache = cache_file_contents(themefile))) themefile = NULL;
    else cached = TRUE;

    /// get mandatory details

    if (!is_OK || !(pstyle = get_val_from_cached_list(THEME_DETAIL_STYLE, 8, mainw->gen_cache))) {
      if (pstyle) lives_free(pstyle);
      is_OK = FALSE;
      set_toolkit_theme(0);
    } else {
      palette->style = atoi(pstyle);
      lives_free(pstyle);
      if (!(palette->style & STYLE_LIGHT)) {
        palette->ce_unsel.red = palette->ce_unsel.green = palette->ce_unsel.blue = 6554;
        if (mainw->sep_image) lives_widget_set_opacity(mainw->sep_image, 0.8);
        if (mainw->multitrack && mainw->multitrack->sep_image)
          lives_widget_set_opacity(mainw->multitrack->sep_image, 0.8);
        set_toolkit_theme(USE_LIVES_THEMEING | LIVES_THEME_DARK | LIVES_THEME_COMPACT);
      } else {
        if (mainw->sep_image) lives_widget_set_opacity(mainw->sep_image, 0.4);
        if (mainw->multitrack && mainw->multitrack->sep_image)
          lives_widget_set_opacity(mainw->multitrack->sep_image, 0.4);
        set_toolkit_theme(USE_LIVES_THEMEING | LIVES_THEME_COMPACT);
        palette->ce_unsel.red = palette->ce_unsel.green = palette->ce_unsel.blue = 0;
      }
    }

    if (!is_OK || !get_theme_colour_pref(THEME_DETAIL_NORMAL_FORE, &lcol)) {
      is_OK = FALSE;
    } else lives_rgba_to_widget_color(&palette->normal_fore, &lcol);

    if (!is_OK || !get_theme_colour_pref(THEME_DETAIL_NORMAL_BACK, &lcol)) {
      is_OK = FALSE;
    } else lives_rgba_to_widget_color(&palette->normal_back, &lcol);

    if (!is_OK || !get_theme_colour_pref(THEME_DETAIL_ALT_FORE, &lcol)) {
      is_OK = FALSE;
    } else lives_rgba_to_widget_color(&palette->menu_and_bars_fore, &lcol);

    if (!is_OK || !get_theme_colour_pref(THEME_DETAIL_ALT_BACK, &lcol)) {
      is_OK = FALSE;
    } else lives_rgba_to_widget_color(&palette->menu_and_bars, &lcol);

    if (!is_OK || !get_theme_colour_pref(THEME_DETAIL_INFO_TEXT, &lcol)) {
      is_OK = FALSE;
    } else lives_rgba_to_widget_color(&palette->info_text, &lcol);

    if (!is_OK || !get_theme_colour_pref(THEME_DETAIL_INFO_BASE, &lcol)) {
      is_OK = FALSE;
    } else lives_rgba_to_widget_color(&palette->info_base, &lcol);

    if (!is_OK) {
      if (cached) {
        lives_list_free_all(&mainw->gen_cache);
        mainw->gen_cache = cache_backup;
        themefile = othemefile;
      }
      if (mainw->is_ready) do_bad_theme_error(themefile);
      lives_free(themefile);
      return FALSE;
    }

    // get optional elements
    if (get_theme_colour_pref(THEME_DETAIL_MT_TCFG, &lcol)) {
      lives_rgba_to_widget_color(&palette->mt_timecode_fg, &lcol);
    }

    if (get_theme_colour_pref(THEME_DETAIL_MT_TCBG, &lcol)) {
      lives_rgba_to_widget_color(&palette->mt_timecode_bg, &lcol);
    }

    get_theme_colour_pref(THEME_DETAIL_AUDCOL, &palette->audcol);
    get_theme_colour_pref(THEME_DETAIL_VIDCOL, &palette->vidcol);
    get_theme_colour_pref(THEME_DETAIL_FXCOL, &palette->fxcol);

    get_theme_colour_pref(THEME_DETAIL_MT_TLREG, &palette->mt_timeline_reg);
    get_theme_colour_pref(THEME_DETAIL_MT_MARK, &palette->mt_mark);
    get_theme_colour_pref(THEME_DETAIL_MT_EVBOX, &palette->mt_evbox);

    get_theme_colour_pref(THEME_DETAIL_FRAME_SURROUND, &palette->frame_surround);

    get_theme_colour_pref(THEME_DETAIL_CE_SEL, &palette->ce_sel);
    get_theme_colour_pref(THEME_DETAIL_CE_UNSEL, &palette->ce_unsel);

    if (cached) {
      lives_list_free_all(&mainw->gen_cache);
      mainw->gen_cache = cache_backup;
      themefile = othemefile;
    }

    lives_free(themefile);

    // set details in prefs
    set_palette_prefs(force_reload);
  }

#ifndef VALGRIND_ON
  /// generate some complementary colours
  if (!prefs->vj_mode && !prefs->startup_phase && !mainw->debug) {
    /// create thread to pick custom colours
    double cpvar = get_double_prefd(PREF_CPICK_VAR, DEF_CPICK_VAR);
    prefs->cptime = get_double_prefd(PREF_CPICK_TIME, -DEF_CPICK_TIME);
    if (fabs(prefs->cptime) < .5) prefs->cptime = -1.;
    mainw->helper_procthreads[PT_CUSTOM_COLOURS]
      = lives_proc_thread_create(LIVES_THRDATTR_NOTE_STTIME, (lives_funcptr_t)pick_custom_colours,
                                 WEED_SEED_DOUBLE, "dd", cpvar, prefs->cptime);
  }
#endif
  /// set global values
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "toolbutton *", "background-image", "none");

  colref = gdk_rgba_to_string(&palette->normal_back);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "combobox window menu", "background-color", colref);
  lives_free(colref);
  colref = gdk_rgba_to_string(&palette->normal_fore);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "combobox window menu", "color", colref);
  lives_free(colref);

  colref = gdk_rgba_to_string(&palette->menu_and_bars);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "scrollbar", "background-color", colref);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "combobox window menu menuitem", "background-color", colref);
  lives_free(colref);
  colref = gdk_rgba_to_string(&palette->menu_and_bars_fore);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "combobox window menu menuitem", "color", colref);
  lives_free(colref);

  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "csombobox window menu menuitem", "border-width", "2px");

  tmp = lives_strdup_printf("%dpx", ((widget_opts.css_min_height * 3 + 3) >> 2) << 1);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "combobox window menu menuitem", "min-height", tmp);
  lives_free(tmp);
  colref = gdk_rgba_to_string(&palette->menu_and_bars_fore);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "scrollbar", "color", colref);
  lives_free(colref);

  return TRUE;
}


void replace_with_delegates(void) {
  weed_plant_t *filter;

  lives_rfx_t *rfx;

  int resize_fx;
  int deint_idx;

  if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate != -1) {
    resize_fx = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_RESIZER].list,
                                     mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate));
    filter = get_weed_filter(resize_fx);
    rfx = weed_to_rfx(filter, TRUE);

    rfx->is_template = FALSE;
    rfx->props |= RFX_PROPS_MAY_RESIZE;

    lives_free(rfx->action_desc);
    rfx->action_desc = (_("Resizing"));

    rfx->min_frames = 1;

    lives_free(rfx->menu_text);

    if (!mainw->resize_menuitem) {
      rfx->menu_text = (_("_Resize All Frames..."));
      mainw->resize_menuitem = rfx->menuitem = lives_standard_menu_item_new_with_label(rfx->menu_text);
      lives_widget_show(mainw->resize_menuitem);
      lives_menu_shell_insert(LIVES_MENU_SHELL(mainw->tools_menu), mainw->resize_menuitem, RFX_TOOL_MENU_POSN);
    } else {
      // disconnect old menu entry
      lives_signal_handler_disconnect(mainw->resize_menuitem, mainw->fx_candidates[FX_CANDIDATE_RESIZER].func);
    }
    // connect new menu entry
    mainw->fx_candidates[FX_CANDIDATE_RESIZER].func = lives_signal_connect(LIVES_GUI_OBJECT(mainw->resize_menuitem),
        LIVES_WIDGET_ACTIVATE_SIGNAL,
        LIVES_GUI_CALLBACK(on_render_fx_pre_activate),
        (livespointer)rfx);
    mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx = rfx;
  }

  if (mainw->resize_menuitem) {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->resize_menuitem), LINKED_RFX_KEY,
                                 (livespointer)mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx);
    lives_widget_set_sensitive(mainw->resize_menuitem, CURRENT_CLIP_HAS_VIDEO);
  }

  deint_idx = weed_get_idx_for_hashname("deinterlacedeinterlace", FALSE);
  if (deint_idx > -1) {
    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list
      = lives_list_append(mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list,
                          LIVES_INT_TO_POINTER(deint_idx));
    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate = 0;
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
