// main.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 or higher as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
*/

// flow is: main() -> real_main() [entry point for liblives]
// real_main() -> pre_init() [early initialisation, initialise prefs system]
//   initialise prefs
//   initialise weed_memory
//   initialise message log
//   initialise widget_helper
// real_main() [parse startup opts]
// real_main() -> lives_startup() [added as idle function]
// lives_startup ->lives_init
// lives_startup2 [added as idle from lives_startup]
// real_main() -> gtk_main()

// idlefuncion:
// lives_startup() [handle any prefs touched by opts which are needed for the GUI]
//   on fresh install: do_workdir_query()
//   create_LiVES() [create the GUI interface]
//   splash_end -> create MT interface if wanted
//   show_lives()
//   check_for_recovery files, load them if wanted
//   do_layout_recovery
//   lives_init() [set remaining variables and preferences]

#ifdef USE_GLIB
#include <glib.h>
#endif

#define NEED_ENDIAN_TEST

#define _MAIN_C_
#include "main.h"
#include "interface.h"
#include "callbacks.h"

#include "effects.h"
#include "rte_window.h"
#include "resample.h"
#include "audio.h"
#include "paramwindow.h"
#include "stream.h"
#include "startup.h"
#include "cvirtual.h"
#include "ce_thumbs.h"
#include "rfx-builder.h"

#ifndef ENABLE_DIAGNOSTICS
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

#ifdef IS_DARWIN
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif
#ifdef USE_LIBPNG
#include <png.h>
#include <setjmp.h>
#endif

#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#endif

#ifdef LIVES_OS_UNIX
#include <glib-unix.h>
#endif


////////////////////////////////
_palette *palette;
ssize_t sizint, sizdbl, sizshrt;
mainwindow *mainw;

//////////////////////////////////////////
static char buff[256];

static char devmap[PATH_MAX];

static boolean no_recover = FALSE, auto_recover = FALSE;
static boolean upgrade_error = FALSE;
static boolean info_only;

static char *newconfigfile = NULL;

static char start_file[PATH_MAX];
static double start = 0.;
static int end = 0;

static boolean theme_error;

static _ign_opts ign_opts;

static int zargc;
static char **zargv;

#ifndef NO_PROG_LOAD
static int xxwidth = 0, xxheight = 0;
#endif

static char *old_vhash = NULL;
static int initial_startup_phase = 0;
static boolean needs_workdir = FALSE;
static boolean ran_ds_dlg = FALSE;

static void do_start_messages(void);

////////////////////

#ifdef GUI_GTK
LiVESTargetEntry target_table[]  = {
  { "STRING",                     GTK_TARGET_OTHER_APP, 0 },
  { "text/uri-list",              GTK_TARGET_OTHER_APP, 0 },
};
#endif

/////////////////////////////////
#ifdef NO_COMPILE // never compile this
void tr_msg(void) {
  // TRANSLATORS: do not translate this message
  char *msg =
    (_("Attention Translators !\nThis message is intended for you, so please do not translate it.\n\n"
       "All translators should read the LiVES translation notes at\n"
       "http://lives-video.com/TRANS-README.txt"));
}
#endif


void break_me(const char *brkstr) {
  if (prefs && prefs->show_dev_opts)
    g_print("BANG ! hit breakpoint %s\n", brkstr ? brkstr : "???");
  // breakpoint for gdb
}


// in library we run gtk in a thread so we can return to caller
void *gtk_thread_wrapper(void *data) {
  gtk_main();
  return NULL;
}


#ifdef USE_GLIB
static void lives_log_handler(const char *domain, LiVESLogLevelFlags level, const char *message,  livespointer data) {
  if (prefs && prefs->vj_mode) return;
  if (level & LIVES_LOG_FATAL_MASK) {
#ifndef IGNORE_FATAL_ERRORS
    raise(LIVES_SIGSEGV);
#endif
  } else {
    char *msg;
    LiVESLogLevelFlags xlevel = level & LIVES_LOG_LEVEL_MASK;

#ifdef LIVES_NO_DEBUG
    if (prefs && !prefs->show_dev_opts) return;
#endif

#ifndef SHOW_ALL_ERRORS
#ifdef LIVES_NO_DEBUG
    if (xlevel >= LIVES_LOG_LEVEL_DEBUG) return;
#endif
    //efine SHOW_INFO_ERRORS
#ifndef SHOW_INFO_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_INFO) return;
#endif
    //efine SHOW_MSG_ERRORS
#ifndef SHOW_MSG_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_MESSAGE) return;
#endif
#define NO_WARN_ERRORS
#ifdef NO_WARN_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_WARNING) return;
#endif
    //#define NO_CRITICAL_ERRORS
#ifdef NO_CRITICAL_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_CRITICAL) return;
#endif
#endif

    //efine TRAP_THEME_ERRORS
#define SHOW_THEME_ERRORS
#ifndef SHOW_THEME_ERRORS
    if (prefs->show_dev_opts)
      if (!strncmp(message, "Theme parsing", strlen("Theme parsing"))) {
#ifdef TRAP_THEME_ERRORS
        raise(LIVES_SIGTRAP);
#endif
        return;
      }
#endif

    //#define TRAP_ERRMSG ""
#ifdef TRAP_ERRMSG
    if (!strncmp(message, TRAP_ERRMSG, strlen(TRAP_ERRMSG))) {
      fprintf(stderr, "Trapped message %s\n", message);
      raise(LIVES_SIGTRAP);
    }
#endif
    if (xlevel == LIVES_LOG_LEVEL_FATAL)
      msg = lives_strdup_printf("%s Fatal error: %s\n", domain, message);
    else if (xlevel == LIVES_LOG_LEVEL_CRITICAL)
      msg = lives_strdup_printf("%s Critical error: %s\n", domain, message);
    else if (xlevel == LIVES_LOG_LEVEL_WARNING)
      msg = lives_strdup_printf("%s Warning: %s\n", domain, message);
    else if (xlevel == LIVES_LOG_LEVEL_MESSAGE)
      msg = lives_strdup_printf("%s Warning: %s\n", domain, message);
    else if (xlevel == LIVES_LOG_LEVEL_INFO)
      msg = lives_strdup_printf("%s Warning: %s\n", domain, message);
    else if (xlevel == LIVES_LOG_LEVEL_DEBUG)
      msg = lives_strdup_printf("%s Warning: %s\n", domain, message);
    else {
      msg = lives_strdup_printf("%s (Unknown level %u error: %s\n", domain, xlevel, message);
    }

    if (mainw->is_ready) d_print(msg);
    fprintf(stderr, "%s", msg);
    lives_free(msg);

#define BREAK_ON_CRIT
    if (xlevel <= LIVES_LOG_LEVEL_CRITICAL) {
#ifdef BREAK_ON_CRIT
      raise(LIVES_SIGTRAP);
#endif
    }

    //#define BREAK_ON_WARN
#ifdef BREAK_ON_WARN
    if (xlevel <= LIVES_LOG_LEVEL_WARNING) raise(LIVES_SIGTRAP);
#endif

    //#define BREAK_ON_ALL
#ifdef BREAK_ON_ALL
    raise(LIVES_SIGTRAP);
#endif
  }
}
#endif // USE_GLIB


#ifdef ENABLE_JACK
LIVES_LOCAL_INLINE void jack_warn() {
  do_jack_noopen_warn3();
  if (prefs->startup_phase == 4) {
    do_jack_noopen_warn2();
  } else do_jack_noopen_warn4();
}
#endif


void defer_sigint(int signum) {
  mainw->signal_caught = signum;
  switch (mainw->crash_possible) {
  case 1:
    // crash in jack_client_open()
    //jack_warn();
    break;
  default:
    break;
  }
  return;
}

//#define QUICK_EXIT
void catch_sigint(int signum) {
  // trap for ctrl-C and others
  //if (mainw->jackd) lives_jack_end();

  if (prefs->show_desktop_panel && (capable->wm_caps.pan_annoy & ANNOY_DISPLAY)
      && (capable->wm_caps.pan_annoy & ANNOY_FS) && (capable->wm_caps.pan_res & RES_HIDE) &&
      capable->wm_caps.pan_res & RESTYPE_ACTION) {
    mainw->ignore_screen_size = TRUE;
    show_desktop_panel();
  }

  if (capable && !pthread_equal(capable->main_thread, pthread_self())) {
    lives_proc_thread_t lpt = THREADVAR(tinfo);
    weed_set_int_value(lpt, WEED_LEAF_SIGNALLED, signum);
    weed_set_voidptr_value(lpt, WEED_LEAF_SIGNAL_DATA, THREADVAR(mydata));
    g_print("Thread got signal %d\n", signum);
    sleep(3600);
    pthread_exit(NULL);
  }
#ifdef QUICK_EXIT
  /* shoatend(); */
  /* fprintf(stderr, "shoatt end"); */
  /* fflush(stderr); */
  exit(signum);
#endif
  if (mainw) {
    if (LIVES_MAIN_WINDOW_WIDGET) {
      if (mainw->foreign) {
        exit(signum);
      }

      if (mainw->multitrack) mainw->multitrack->idlefunc = 0;
      mainw->fatal = TRUE;

      if (signum == LIVES_SIGABRT || signum == LIVES_SIGSEGV) {
        mainw->memok = FALSE;
        signal(LIVES_SIGSEGV, SIG_DFL);
        signal(LIVES_SIGABRT, SIG_DFL);
        fprintf(stderr, _("\nUnfortunately LiVES crashed.\nPlease report this bug at %s\n"
                          "Thanks. Recovery should be possible if you restart LiVES.\n"), LIVES_BUG_URL);
        fprintf(stderr, _("\n\nWhen reporting crashes, please include details of your operating system, "
                          "distribution, and the LiVES version (%s)\n"), LiVES_VERSION);

        if (capable->has_gdb) {
          if (mainw->debug) fprintf(stderr, "%s", _("and any information shown below:\n\n"));
          else fprintf(stderr, "%s", _("Please try running LiVES with the -debug option to collect more information.\n\n"));
        } else {
          fprintf(stderr, "%s", _("Please install gdb and then run LiVES with the -debug option "
                                  "to collect more information.\n\n"));
        }

#ifdef USE_GLIB
#ifdef LIVES_NO_DEBUG
        if (mainw->debug) {
#endif
#ifdef HAVE_PRCTL
          prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif
          g_on_error_query(capable->myname_full);
#ifdef LIVES_NO_DEBUG
        }
#endif
#endif
#ifndef GDB_ON
        _exit(signum);
#endif
      }

      if (mainw->was_set) {
        if (mainw->memok) fprintf(stderr, "%s", _("Preserving set.\n"));
      }

      mainw->leave_recovery = mainw->leave_files = TRUE;

      mainw->only_close = FALSE;
      lives_exit(signum);
    }
  }
  exit(signum);
}

#ifdef USE_GLIB
static boolean glib_sighandler(livespointer data) {
  int signum = LIVES_POINTER_TO_INT(data);
  catch_sigint(signum);
  return TRUE;
}
#endif


#ifdef GUI_GTK
static double get_screen_scale(GdkScreen *screen, double *pdpi) {
  double scale = 1.0;
  double dpi = gdk_screen_get_resolution(screen);
  if (dpi == 120.) scale = 1.25;
  else if (dpi == 144.) scale = 1.5;
  else if (dpi == 192.) scale = 2.0;
  if (pdpi) *pdpi = dpi;
  return scale;
}
#endif


void get_monitors(boolean reset) {
#ifdef GUI_GTK
  GdkDisplay *disp;
  GdkScreen *screen;
#if GTK_CHECK_VERSION(3, 22, 0)
  GdkMonitor *moni;
#endif
  GdkRectangle rect;
  GdkDevice *device;
  double scale, dpi;
  int play_moni = 1;
#if !GTK_CHECK_VERSION(3, 22, 0)
  GSList *dlist, *dislist;
  int nscreens, nmonitors;
#if LIVES_HAS_DEVICE_MANAGER
  GdkDeviceManager *devman;
  LiVESList *devlist;
  int k;
#endif
  int i, j;
#endif
  int idx = 0;

  if (mainw->ignore_screen_size) return;

  lives_freep((void **)&mainw->mgeom);
  capable->nmonitors = 0;

#if !GTK_CHECK_VERSION(3, 22, 0)
  dlist = dislist = gdk_display_manager_list_displays(gdk_display_manager_get());

  // for each display get list of screens

  while (dlist) {
    disp = (GdkDisplay *)dlist->data;

    // get screens
    nscreens = lives_display_get_n_screens(disp);
    for (i = 0; i < nscreens; i++) {
      screen = gdk_display_get_screen(disp, i);
      capable->nmonitors += gdk_screen_get_n_monitors(screen);
    }
    dlist = dlist->next;
  }
#else
  disp = gdk_display_get_default();
  capable->nmonitors += gdk_display_get_n_monitors(disp);
#endif

  mainw->mgeom = (lives_mgeometry_t *)lives_calloc(capable->nmonitors, sizeof(lives_mgeometry_t));


#if !GTK_CHECK_VERSION(3, 22, 0)
  dlist = dislist;

  while (dlist) {
    disp = (GdkDisplay *)dlist->data;

#if LIVES_HAS_DEVICE_MANAGER
    devman = gdk_display_get_device_manager(disp);
    devlist = gdk_device_manager_list_devices(devman, GDK_DEVICE_TYPE_MASTER);
#endif
    // get screens
    nscreens = lives_display_get_n_screens(disp);
    for (i = 0; i < nscreens; i++) {
      screen = gdk_display_get_screen(disp, i);
      scale = get_screen_scale(screen, &dpi);
      nmonitors = gdk_screen_get_n_monitors(screen);
      for (j = 0; j < nmonitors; j++) {
        gdk_screen_get_monitor_geometry(screen, j, &(rect));
        mainw->mgeom[idx].x = rect.x;
        mainw->mgeom[idx].y = rect.y;
        mainw->mgeom[idx].width = mainw->mgeom[idx].phys_width = rect.width;
        mainw->mgeom[idx].height = mainw->mgeom[idx].phys_height = rect.height;
        mainw->mgeom[idx].mouse_device = NULL;
        mainw->mgeom[idx].dpi = dpi;
        mainw->mgeom[idx].scale = scale;
#if GTK_CHECK_VERSION(3, 4, 0)
        gdk_screen_get_monitor_workarea(screen, j, &(rect));
        mainw->mgeom[idx].width = rect.width;
        mainw->mgeom[idx].height = rect.height;
#endif
#if LIVES_HAS_DEVICE_MANAGER
        // get (virtual) mouse device for this screen
        for (k = 0; k < lives_list_length(devlist); k++) {
          device = (GdkDevice *)lives_list_nth_data(devlist, k);
          if (gdk_device_get_display(device) == disp &&
              gdk_device_get_source(device) == GDK_SOURCE_MOUSE) {
            mainw->mgeom[idx].mouse_device = device;
            break;
          }
        }
#endif
        mainw->mgeom[idx].disp = disp;
        mainw->mgeom[idx].screen = screen;
        idx++;
        if (idx >= capable->nmonitors) break;
      }
    }
#if LIVES_HAS_DEVICE_MANAGER
    lives_list_free(devlist);
#endif
    dlist = dlist->next;
  }

  lives_slist_free(dislist);
#else
  screen = gdk_display_get_default_screen(disp);
  scale = get_screen_scale(screen, &dpi);
  device = gdk_seat_get_pointer(gdk_display_get_default_seat(disp));
  for (idx = 0; idx < capable->nmonitors; idx++) {
    mainw->mgeom[idx].disp = disp;
    mainw->mgeom[idx].monitor = moni = gdk_display_get_monitor(disp, idx);
    mainw->mgeom[idx].screen = screen;
    gdk_monitor_get_geometry(moni, (GdkRectangle *)&rect);
    mainw->mgeom[idx].x = rect.x;
    mainw->mgeom[idx].y = rect.y;
    mainw->mgeom[idx].phys_width = rect.width;
    mainw->mgeom[idx].phys_height = rect.height;
    mainw->mgeom[idx].mouse_device = device;
    mainw->mgeom[idx].dpi = dpi;
    mainw->mgeom[idx].scale = scale;
    gdk_monitor_get_workarea(moni, &(rect));
    mainw->mgeom[idx].width = rect.width;
    mainw->mgeom[idx].height = rect.height;
    if (gdk_monitor_is_primary(moni)) {
      capable->primary_monitor = idx;
      mainw->mgeom[idx].primary = TRUE;
    } else if (play_moni == 1) play_moni = idx + 1;
  }
#endif
#endif

  if (prefs->force_single_monitor) capable->nmonitors = 1; // force for clone mode

  if (!reset) return;

  prefs->gui_monitor = 0;
  prefs->play_monitor = play_moni;

  if (capable->nmonitors > 1) {
    get_string_pref(PREF_MONITORS, buff, 256);

    if (*buff && get_token_count(buff, ',') > 1) {
      char **array = lives_strsplit(buff, ",", 2);
      prefs->gui_monitor = atoi(array[0]);
      prefs->play_monitor = atoi(array[1]);
      lives_strfreev(array);
    }

    if (prefs->gui_monitor < 1) prefs->gui_monitor = 1;
    if (prefs->play_monitor < 0) prefs->play_monitor = 0;
    if (prefs->gui_monitor > capable->nmonitors) prefs->gui_monitor = capable->nmonitors;
    if (prefs->play_monitor > capable->nmonitors) prefs->play_monitor = capable->nmonitors;
  }

  widget_opts.monitor = prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : capable->primary_monitor;

  mainw->old_scr_width = GUI_SCREEN_WIDTH;
  mainw->old_scr_height = GUI_SCREEN_HEIGHT;

  prefs->screen_scale = get_double_prefd(PREF_SCREEN_SCALE, 0.);
  if (prefs->screen_scale == 0.) {
    prefs->screen_scale = (double)GUI_SCREEN_WIDTH / (double)SCREEN_SCALE_DEF_WIDTH;
    prefs->screen_scale = (prefs->screen_scale - 1.) * 1.5 + 1.;
  }

  if (!prefs->vj_mode && GUI_SCREEN_HEIGHT >= MIN_MSG_AREA_SCRNHEIGHT) prefs->show_msg_area = TRUE;
  else prefs->show_msg_area = FALSE;

  widget_opts_rescale(prefs->screen_scale);
}


static void print_notice(void) {
  fprintf(stderr, "\nLiVES %s\n", LiVES_VERSION);
  fprintf(stderr, "Copyright "LIVES_COPYRIGHT_YEARS" Gabriel Finch ("LIVES_AUTHOR_EMAIL") and others.\n");
  fprintf(stderr, "LiVES comes with ABSOLUTELY NO WARRANTY\nThis is free software, and you are welcome to redistribute it\n"
          "under certain conditions; "
          "see the file COPYING for details.\n\n");
}


static boolean pre_init(void) {
  // stuff which should be done *before* mainwindow is created
  // returns TRUE if we got an error loading the theme

#ifdef GUI_GTK
  LiVESError *gerr = NULL;
  char *icon;
#endif

  pthread_mutexattr_t mattr;

  char *msg, *tmp, *tmp2, *cfgdir;

  boolean needs_update = FALSE;

  int i;

  /// create context data for main thread; must be called before get_capabilities()
  lives_thread_data_create(0);

  // need to create directory for configfile before calling get_capabilities()
  cfgdir = get_dir(prefs->configfile);
  lives_make_writeable_dir(cfgdir);
  lives_free(cfgdir);

  // pre-checked conditions. We will check for these agian
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
      startup_message_fatal(lives_strdup(
                              _("\nAn incorrect version of smogrify was found in your path.\n\n"
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
              (tmp2 = ensure_extension((tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)),
                                       LIVES_FILE_EXT_NEW)));
      lives_free(tmp);
      lives_free(tmp2);
      startup_message_fatal(msg);
    }

    if (!capable->can_write_to_workdir) {
      if (!mainw->has_session_workdir) {
        tmp2 = lives_strdup_printf(_("Please check the %s setting in \n%s\nand try again.\n"),
                                   (mainw->old_vhash && atoi(mainw->old_vhash) != 0 && atoi(mainw->old_vhash) < 3003003)
                                   ? "<tempdir>" : "<workdir>",
                                   (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
        lives_free(tmp);
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

  prefs->show_dev_opts = get_boolean_prefd(PREF_SHOW_DEVOPTS, FALSE);
  prefs->dev_show_dabg = prefs->dev_show_timing = FALSE;

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
    capable->has_mplayer = has_executable(EXEC_MPLAYER);
    capable->has_mplayer2 = has_executable(EXEC_MPLAYER2);
    capable->has_mpv = has_executable(EXEC_MPV);

    capable->has_convert = has_executable(EXEC_CONVERT);
    capable->has_composite = has_executable(EXEC_COMPOSITE);
    capable->has_identify = has_executable(EXEC_IDENTIFY);

    capable->has_gzip = has_executable(EXEC_GZIP);
    capable->has_gdb = has_executable(EXEC_GDB);
  }

  /// kick off the thread pool ////////////////////////////////
  /// this must be done before we can check the disk status
  future_prefs->nfx_threads = prefs->nfx_threads = get_int_prefd(PREF_NFX_THREADS, capable->ncpus);

#ifdef VALGRIND_ON
  prefs->nfx_threads = 2;
#endif

  lives_threadpool_init();

  capable->gui_thread = pthread_self();

  /// check disk storage status /////////////////////////////////////
  mainw->ds_status = LIVES_STORAGE_STATUS_UNKNOWN;

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
    if (!needs_workdir && prefs->disk_quota && !needs_workdir && initial_startup_phase == 0)
      mainw->helper_procthreads[PT_LAZY_DSUSED] = disk_monitor_start(prefs->workdir);

    if (!needs_workdir && mainw->next_ds_warn_level > 0) {
      int64_t dsval = disk_monitor_check_result(prefs->workdir);
      if (dsval > 0) capable->ds_used = dsval;
      else dsval = capable->ds_used;
      mainw->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval, 0);
      capable->ds_free = dsval;
      if (mainw->ds_status == LIVES_STORAGE_STATUS_CRITICAL) {
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

  // get some prefs we need to set menu options
  prefs->gui_monitor = -1;

  if (prefs->vj_mode) {
    capable->has_wmctrl = has_executable(EXEC_WMCTRL);
    capable->has_xwininfo = has_executable(EXEC_XWININFO);
    capable->has_xdotool = has_executable(EXEC_XDOTOOL);
  }
  mainw->mgeom = NULL;

  prefs->force_single_monitor = get_boolean_pref(PREF_FORCE_SINGLE_MONITOR);
  mainw->ignore_screen_size = FALSE;

  capable->primary_monitor = 0;

  // sets prefs->screen_scale, capable->nmonitors, mainw->mgeom, prefs->play_monitor, prefs->gui_monitor
  // prefs->show_msg_area, mainw->old_screen_height, mainw->old_screen_width
  // widget_opts.monitor, widget_opts.screen and various widget_opts sizes
  get_monitors(TRUE);

  // set to allow multiple locking by the same thread
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

  // recursive locks
  pthread_mutex_init(&mainw->instance_ref_mutex, &mattr);
  pthread_mutex_init(&mainw->abuf_mutex, &mattr);

  // non-recursive
  pthread_mutex_init(&mainw->abuf_frame_mutex, NULL);
  pthread_mutex_init(&mainw->fxd_active_mutex, NULL);
  pthread_mutex_init(&mainw->event_list_mutex, NULL);
  pthread_mutex_init(&mainw->clip_list_mutex, NULL);
  pthread_mutex_init(&mainw->vpp_stream_mutex, NULL);
  pthread_mutex_init(&mainw->cache_buffer_mutex, NULL);
  pthread_mutex_init(&mainw->audio_filewriteend_mutex, NULL);
  pthread_mutex_init(&mainw->exit_mutex, NULL);
  pthread_mutex_init(&mainw->fbuffer_mutex, NULL);
  pthread_mutex_init(&mainw->avseek_mutex, NULL);
  pthread_mutex_init(&mainw->alarmlist_mutex, NULL);

  // conds
  pthread_cond_init(&mainw->avseek_cond, NULL);

  if (prefs->vj_mode)
    prefs->load_rfx_builtin = FALSE;
  else
    prefs->load_rfx_builtin = get_boolean_prefd(PREF_LOAD_RFX_BUILTIN, TRUE);

  for (i = 0; i < FX_KEYS_MAX; i++) {
    pthread_mutex_init(&mainw->fx_mutex[i], NULL);
  }

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

  prefs->mt_show_ctx = get_boolean_prefd(PREF_MT_SHOW_CTX, TRUE);

  mainw->threaded_dialog = FALSE;
  clear_mainw_msg();

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
  prefs->letterbox_mt = get_boolean_prefd(PREF_LETTERBOXMT, TRUE);

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
  icon = lives_build_filename(prefs->prefix_dir, DESKTOP_ICON_DIR, "lives." LIVES_FILE_EXT_PNG, NULL);
  gtk_window_set_default_icon_from_file(icon, &gerr);
  lives_free(icon);

  if (gerr) lives_error_free(gerr);
#endif
  mainw->first_free_file = 1;

  needs_update = FALSE;

  get_string_pref(PREF_LIB_DIR, prefs->lib_dir, PATH_MAX);

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
  future_prefs->msg_textsize = prefs->msg_textsize = get_int_prefd(PREF_MSG_TEXTSIZE, DEF_MSG_TEXTSIZE);

  mainw->msg_list = NULL;
  mainw->n_messages = 0;
  mainw->ref_message = NULL;
  mainw->ref_message_n = 0;
  add_messages_to_list(_("Starting...\n"));

  get_string_pref(PREF_GUI_THEME, prefs->theme, 64);

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
  if (!mainw->foreign && prefs->startup_phase == -1) {
    if (prefs->show_splash) splash_init();
    print_notice();
  }

  get_string_pref(PREF_CDPLAY_DEVICE, prefs->cdplay_device, PATH_MAX);

  prefs->warning_mask = (uint64_t)get_int64_prefd(PREF_LIVES_WARNING_MASK, DEF_WARNING_MASK);

  if (!ign_opts.ign_jackopts) {
    prefs->jack_opts = future_prefs->jack_opts = get_int_prefd(PREF_JACK_OPTS, 16);
  }

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

  prefs->volume = (float)get_double_prefd(PREF_MASTER_VOLUME, 1.0);
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
  mainw->loop_cont = FALSE;
  mainw->fs = FALSE;

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
      mainw->resize_menuitem = lives_standard_menu_item_new_with_label(rfx->menu_text);
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

  if (mainw->resize_menuitem) lives_widget_set_sensitive(mainw->resize_menuitem, CURRENT_CLIP_HAS_VIDEO);

  deint_idx = weed_get_idx_for_hashname("deinterlacedeinterlace", FALSE);
  if (deint_idx > -1) {
    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list
      = lives_list_append(mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list,
                          LIVES_INT_TO_POINTER(deint_idx));
    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate = 0;
  }
}


static void lives_init(_ign_opts *ign_opts) {
  // init mainwindow data
  LiVESList *encoders = NULL;
  LiVESList *encoder_capabilities = NULL;

  char **array;
  char mppath[PATH_MAX];

  char *weed_plugin_path;
  char *frei0r_path;
  char *ladspa_path;
  char *libvis_path;
  char *msg;

  boolean needs_free;

  int naudp = 0;

  int i;

  for (i = 0; i <= MAX_FILES; mainw->files[i++] = NULL);
  mainw->prefs_changed = FALSE;
  mainw->insert_after = TRUE;
  mainw->mute = FALSE;
  mainw->faded = FALSE;
  if (!prefs->vj_mode)
    mainw->save_with_sound = TRUE;   // also affects loading
  else
    mainw->save_with_sound = FALSE;
  mainw->preview = FALSE;
  mainw->selwidth_locked = FALSE;
  mainw->untitled_number = mainw->cap_number = 1;
  mainw->sel_start = 0;
  mainw->sel_move = SEL_MOVE_AUTO;
  mainw->record_foreign = FALSE;
  mainw->play_window = NULL;
  mainw->opwx = mainw->opwy = -1;
  mainw->frame_layer = NULL;
  mainw->in_fs_preview = FALSE;
  mainw->effects_paused = FALSE;
  mainw->play_start = 0;
  mainw->opening_loc = FALSE;
  mainw->toy_type = LIVES_TOY_NONE;
  mainw->framedraw = mainw->framedraw_spinbutton = NULL;
  mainw->fd_layer = NULL;
  mainw->fd_layer_orig = NULL;
  mainw->is_processing = FALSE;
  mainw->is_rendering = FALSE;
  mainw->is_generating = FALSE;
  mainw->resizing = FALSE;
  mainw->switch_during_pb = FALSE;
  mainw->playing_sel = FALSE;
  mainw->aframeno = 0;
  if (capable->byte_order == LIVES_LITTLE_ENDIAN) {
    mainw->endian = 0;
  } else {
    mainw->endian = AFORM_BIG_ENDIAN;
  }

  mainw->leave_files = FALSE;
  mainw->was_set = FALSE;
  mainw->toy_go_wild = FALSE;

  for (i = 0; i < FN_KEYS - 1; i++) {
    mainw->clipstore[i][0] = -1;
  }

  mainw->ping_pong = FALSE;

  mainw->nervous = FALSE;
  fx_dialog[0] = fx_dialog[1] = NULL;

  mainw->rte_keys = -1;
  rte_window = NULL;

  mainw->rte = EFFECT_NONE;

  mainw->preview_box = NULL;
  mainw->prv_link = PRV_PTR;

  mainw->internal_messaging = FALSE;
  mainw->progress_fn = NULL;

  mainw->last_grabbable_effect = -1;
  mainw->blend_file = -1;

  mainw->pre_src_file = -2;
  mainw->pre_src_audio_file = -1;

  mainw->size_warn = 0;
  mainw->dvgrab_preview = FALSE;

  mainw->file_open_params = NULL;
  mainw->whentostop = NEVER_STOP;

  mainw->audio_start = mainw->audio_end = 0;
  mainw->cliplist = NULL;

  // rendered_fx number of last transition
  mainw->last_transition_idx = -1;
  mainw->last_transition_loops = 1;
  mainw->last_transition_align_start = TRUE;
  mainw->last_transition_loop_to_fit = mainw->last_transition_ins_frames = FALSE;
  mainw->num_tr_applied = 0;

  mainw->blend_factor = 0.;

  mainw->fixed_fps_numer = -1;
  mainw->fixed_fps_denom = 1;
  mainw->fixed_fpsd = -1.;
  mainw->noswitch = mainw->cs_permitted = mainw->cs_is_permitted = FALSE;
  mainw->osc_block = FALSE;

  mainw->cancelled = CANCEL_NONE;
  mainw->cancel_type = CANCEL_KILL;

  mainw->framedraw_reset = NULL;

  // setting this to TRUE can possibly increase smoothness for lower framerates
  // needs more testing and a preference in prefs window- TODO
  // can also be set through OSC: /output/nodrop/enable
  prefs->noframedrop = get_boolean_prefd(PREF_NOFRAMEDROP, FALSE);

  prefs->omc_noisy = FALSE;
  prefs->omc_events = TRUE;

  if (!ign_opts->ign_osc) {
    prefs->osc_udp_started = FALSE;
    prefs->osc_udp_port = 0;
#ifdef ENABLE_OSC
    if (!mainw->foreign) {
      prefs->osc_udp_port = get_int_prefd(PREF_OSC_PORT, DEF_OSC_LISTEN_PORT);
      future_prefs->osc_start = prefs->osc_start = get_boolean_prefd(PREF_OSC_START, FALSE);
    } else {
      future_prefs->osc_start = prefs->osc_start = FALSE;
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
  mainw->record = FALSE;
  mainw->event_list = NULL;
  mainw->clip_switched = FALSE;
  mainw->scrap_file = -1;
  mainw->ascrap_file = -1;

  mainw->jack_can_stop = FALSE;
  mainw->jack_can_start = TRUE;

  if (!mainw->foreign) mainw->video_seek_ready = mainw->audio_seek_ready = FALSE;
  else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;

  mainw->filter_map = NULL; // filter map for video rendering
  mainw->afilter_map = NULL; // filter map for audio rendering
  mainw->audio_event = NULL;

  mainw->did_rfx_preview = FALSE;

  prefsw = NULL;
  rdet = NULL;
  resaudw = NULL;

  mainw->actual_frame = 0;

  mainw->scratch = SCRATCH_NONE;

  mainw->clip_index = NULL;
  mainw->frame_index = NULL;

  mainw->affected_layouts_map = mainw->current_layouts_map = NULL;

  mainw->leave_recovery = TRUE;

  mainw->pchains = NULL;

  mainw->preview_frame = 0;

  mainw->unordered_blocks = FALSE;

  mainw->only_close = FALSE;

  mainw->no_exit = FALSE;

  mainw->multi_opts.set = FALSE;

  mainw->clip_header = NULL;

  mainw->new_blend_file = -1;

  mainw->jackd = mainw->jackd_read = NULL;

  mainw->pulsed = mainw->pulsed_read = NULL;

  mainw->show_procd = TRUE;

  mainw->framedraw_preview = mainw->framedraw_reset = NULL;

  mainw->block_param_updates = mainw->no_interp = FALSE;

  mainw->cevent_tc = 0;

  mainw->opening_multi = FALSE;

  mainw->img_concat_clip = -1;

  mainw->record_paused = mainw->record_starting = FALSE;

  mainw->gen_to_clipboard = FALSE;

  mainw->open_deint = FALSE;

  mainw->write_vpp_file = FALSE;

  mainw->stream_ticks = -1;

  mainw->keep_pre = FALSE;

  mainw->reverse_pb = FALSE;

  mainw->osc_auto = 0;
  mainw->osc_enc_width = mainw->osc_enc_height = 0;

  mainw->no_switch_dprint = FALSE;

  mainw->rte_textparm = NULL;

  mainw->abufs_to_fill = 0;

  mainw->recoverable_layout = mainw->recording_recovered = FALSE;

  mainw->iochan = NULL;

  mainw->stored_event_list = NULL;
  mainw->stored_event_list_changed = mainw->stored_event_list_auto_changed = FALSE;
  mainw->stored_layout_save_all_vals = TRUE;

  mainw->affected_layout_marks = NULL;

  mainw->stored_layout_undos = NULL;
  mainw->sl_undo_mem = NULL;
  mainw->sl_undo_buffer_used = 0;
  mainw->sl_undo_offset = 0;

  mainw->go_away = TRUE;

  mainw->aud_file_to_kill = -1;

  mainw->aud_rec_fd = -1;

  mainw->decoders_loaded = FALSE;
  mainw->decoder_list = NULL;

  mainw->subt_save_file = NULL;

  mainw->fonts_array = get_font_list();

  mainw->nfonts = 0;
  if (mainw->fonts_array)
    while (mainw->fonts_array[mainw->nfonts++]);

  mainw->videodevs = NULL;

  mainw->camframe = NULL;

  mainw->has_custom_effects = FALSE;
  mainw->has_custom_tools = FALSE;
  mainw->has_custom_gens = FALSE;
  mainw->has_custom_utilities = FALSE;

  mainw->log_fd = -2;

  mainw->last_display_ticks = 0;

  mainw->alives_pgid = 0;

  mainw->aplayer_broken = FALSE;

  mainw->render_error = LIVES_RENDER_ERROR_NONE;

  mainw->add_clear_ds_button = FALSE;
  mainw->add_clear_ds_adv = FALSE;
  mainw->tried_ds_recover = FALSE;

  mainw->foreign_visual = NULL;

  mainw->pconx = NULL;
  mainw->cconx = NULL;

  cached_key = cached_mod = 0;

  mainw->agen_key = 0;
  mainw->agen_needs_reinit = FALSE;
  mainw->agen_samps_count = 0;

  mainw->ce_frame_height = mainw->ce_frame_width = -1;

  mainw->cursor_style = LIVES_CURSOR_NORMAL;

  mainw->sepwin_minwidth = MIN_SEPWIN_WIDTH;
  mainw->sepwin_minheight = PREVIEW_BOX_HT;

  mainw->signal_caught = 0;
  mainw->signals_deferred = FALSE;

  mainw->n_screen_areas = SCREEN_AREA_USER_DEFINED1;
  mainw->screen_areas = (lives_screen_area_t *)lives_malloc(mainw->n_screen_areas * sizeof(lives_screen_area_t));
  mainw->screen_areas[SCREEN_AREA_FOREGROUND].name = (_("Foreground"));
  mainw->screen_areas[SCREEN_AREA_BACKGROUND].name = (_("Background"));

  mainw->active_sa_clips = mainw->active_sa_fx = SCREEN_AREA_FOREGROUND;

  mainw->file_buffers = NULL;

  mainw->blend_layer = NULL;

  mainw->ce_upd_clip = FALSE;

  mainw->clips_group = NULL;

  mainw->fx_is_auto = FALSE;
  mainw->gen_started_play = FALSE;

  mainw->audio_frame_buffer = NULL;
  mainw->afbuffer_clients = mainw->afbuffer_clients_read = 0;
  mainw->afb[0] = mainw->afb[1] = NULL;

  lives_memset(mainw->recent_file, 0, 1);

  mainw->aud_data_written = 0;

  mainw->rendered_fx = NULL;

  mainw->midi_channel_lock = FALSE;

  mainw->crash_possible = 0;

  mainw->scrap_pixbuf = NULL;

  mainw->close_keep_frames = FALSE;

#ifdef ENABLE_JACK
  mainw->jack_inited = FALSE;
  mainw->jack_trans_poll = FALSE;
#endif

  mainw->recovering_files = FALSE;

  mainw->num_rendered_effects_builtin = mainw->num_rendered_effects_custom = mainw->num_rendered_effects_test = 0;

  mainw->flush_audio_tc = 0;

  mainw->idlemax = 0;
  mainw->reconfig = FALSE;

  mainw->fsp_func = 0;

  mainw->swapped_clip = -1;

  mainw->urgency_msg = mainw->overlay_msg = NULL;

  mainw->xlays = NULL;

  mainw->preview_rendering = FALSE;

  mainw->new_lmap_errors = NULL;

  mainw->ncbstores = 0;

  mainw->loop_locked = FALSE;

  mainw->invalid_clips = FALSE;

  mainw->blend_palette = WEED_PALETTE_END;
  mainw->blend_width = mainw->blend_height = 0;

  mainw->force_show = FALSE;

  mainw->effort = 0;

  mainw->frame_layer_preload = NULL;
  mainw->pred_frame = 0;
  mainw->pred_clip = 0;

  mainw->lockstats = FALSE;

  mainw->audio_stretch = 1.0;

  mainw->play_sequence = 0;

  mainw->record_frame = -1;

  mainw->debug_ptr = NULL;

  mainw->inst_fps = 0.;

  mainw->pre_play_file = -1;

  mainw->st_fcache = mainw->en_fcache = mainw->pr_fcache = NULL;

  mainw->ptrtime = 0.;

  mainw->proc_ptr = NULL;

  mainw->permmgr = NULL;

  mainw->set_list = NULL;
  mainw->num_sets = -1;

  mainw->mt_needs_idlefunc = FALSE;

  mainw->suppress_layout_warnings = FALSE;

  mainw->add_trash_rb = FALSE;

  mainw->cs_manage = FALSE;

  mainw->dsu_valid = FALSE;
  mainw->dsu_widget = NULL;

  mainw->drawsrc = -1;

  mainw->lazy = 0;

  mainw->disk_mon = 0;

  mainw->wall_ticks = -1;

  /////////////////////////////////////////////////// add new stuff just above here ^^

  lives_memset(mainw->set_name, 0, 1);
  mainw->clips_available = 0;

  prefs->pause_effect_during_preview = FALSE;

  future_prefs->pb_quality = prefs->pb_quality = get_int_prefd(PREF_PB_QUALITY, PB_QUALITY_MED);
  if (prefs->pb_quality != PB_QUALITY_LOW && prefs->pb_quality != PB_QUALITY_HIGH &&
      prefs->pb_quality != PB_QUALITY_MED) prefs->pb_quality = PB_QUALITY_MED;

  prefs->pbq_adaptive = get_boolean_prefd(PREF_PBQ_ADAPTIVE, TRUE);

  mainw->ext_playback = mainw->ext_audio = FALSE;

  prefs->loop_recording = TRUE;
  prefs->no_bandwidth = FALSE;
  prefs->ocp = get_int_prefd(PREF_OPEN_COMPRESSION_PERCENT, 15);

  prefs->stop_screensaver = get_boolean_prefd(PREF_STOP_SCREENSAVER, TRUE);
  future_prefs->show_tool = prefs->show_tool = get_boolean_prefd(PREF_SHOW_TOOLBAR, TRUE);

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

  prefs->acodec_list = NULL;

  prefs->render_audio = TRUE;
  prefs->normalise_audio = TRUE;

  prefs->num_rtaudiobufs = 4;

  prefs->safe_symlinks = FALSE; // set to TRUE for dynebolic and other live CDs

  prefs->ce_maxspect = get_boolean_prefd(PREF_CE_MAXSPECT, TRUE);

  prefs->rec_stop_gb = get_double_prefd(PREF_REC_STOP_GB, DEF_REC_STOP_GB);

  if (prefs->max_modes_per_key == 0) prefs->max_modes_per_key = atoi(DEF_FX_KEYMODES);

  prefs->stream_audio_out = get_boolean_pref(PREF_STREAM_AUDIO_OUT);

  prefs->unstable_fx = get_boolean_prefd(PREF_UNSTABLE_FX, TRUE);

  prefs->disabled_decoders = get_list_pref(PREF_DISABLED_DECODERS);

  prefs->enc_letterbox = FALSE;

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

  prefs->perm_audio_reader = TRUE;

  prefs->max_disp_vtracks = get_int_prefd(PREF_MAX_DISP_VTRACKS, DEF_MT_DISP_TRACKS);

  prefs->mt_load_fuzzy = FALSE;

  prefs->ahold_threshold = get_double_pref(PREF_AHOLD_THRESHOLD);

  prefs->use_screen_gamma = get_boolean_prefd(PREF_USE_SCREEN_GAMMA, FALSE);
  prefs->screen_gamma = get_double_prefd(PREF_SCREEN_GAMMA, DEF_SCREEN_GAMMA);
  prefs->apply_gamma = get_boolean_prefd(PREF_APPLY_GAMMA, TRUE);
  prefs->btgamma = FALSE;

  prefs->msgs_pbdis = get_boolean_prefd(PREF_MSG_PBDIS, TRUE);

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

  prefs->show_desktop_panel = get_x11_visible(capable->wm_caps.panel);
  //prefs->show_desktop_panel = TRUE;

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

  get_string_prefd(PREF_DEF_AUTHOR, prefs->def_author, 1024, "");

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
    prefs->mt_def_signed_endian = get_int_prefd(PREF_MT_DEF_SIGNED_ENDIAN, (capable->byte_order == LIVES_BIG_ENDIAN)
                                  ? 2 : 0 + ((prefs->mt_def_asamps == 8) ? 1 : 0));

    prefs->mt_exit_render = get_boolean_prefd(PREF_MT_EXIT_RENDER, TRUE);
    prefs->render_prompt = get_boolean_prefd(PREF_RENDER_PROMPT, TRUE);

    prefs->mt_pertrack_audio = get_boolean_prefd(PREF_MT_PERTRACK_AUDIO, TRUE);
    prefs->mt_backaudio = get_int_prefd(PREF_MT_BACKAUDIO, 1);

    prefs->instant_open = get_boolean_prefd(PREF_INSTANT_OPEN, TRUE);
    prefs->auto_deint = get_boolean_prefd(PREF_AUTO_DEINTERLACE, TRUE);
    prefs->auto_nobord = get_boolean_prefd(PREF_AUTO_CUT_BORDERS, FALSE);

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

    // scan for encoder plugins
    if ((encoders = get_plugin_list(PLUGIN_ENCODERS, FALSE, NULL, NULL)) != NULL) {
      capable->has_encoder_plugins = TRUE;
      lives_list_free_all(&encoders);
    }

    lives_memset(prefs->encoder.of_name, 0, 1);
    lives_memset(prefs->encoder.of_desc, 0, 1);

    if ((prefs->startup_phase == 1 || prefs->startup_phase == -1) && capable->has_encoder_plugins && capable->has_python) {
      LiVESList *ofmt_all = NULL;
      char **array;
      if (has_executable(EXEC_FFMPEG)) {
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

    future_prefs->encoder.audio_codec = prefs->encoder.audio_codec = get_int_prefd(PREF_ENCODER_ACODEC, -1);
    prefs->encoder.capabilities = 0;
    prefs->encoder.of_allowed_acodecs = AUDIO_CODEC_UNKNOWN;

    lives_snprintf(future_prefs->encoder.name, 64, "%s", prefs->encoder.name);

    lives_memset(future_prefs->encoder.of_restrict, 0, 1);
    lives_memset(prefs->encoder.of_restrict, 0, 1);

    if (capable->has_encoder_plugins) {
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
    weed_plugin_path = getenv("WEED_PLUGIN_PATH");
    if (!weed_plugin_path) {
      get_string_pref(PREF_WEED_PLUGIN_PATH, prefs->weed_plugin_path, PATH_MAX);
      if (!*prefs->weed_plugin_path) weed_plugin_path = lives_build_path(prefs->lib_dir, PLUGIN_EXEC_DIR,
            PLUGIN_WEED_FX_BUILTIN, NULL);
      else weed_plugin_path = lives_strdup(prefs->weed_plugin_path);
      lives_setenv("WEED_PLUGIN_PATH", weed_plugin_path);
      needs_free = TRUE;
    }
    if (!weed_plugin_path) weed_plugin_path = "";
    lives_snprintf(prefs->weed_plugin_path, PATH_MAX, "%s", weed_plugin_path);
    if (needs_free) lives_free(weed_plugin_path);

    needs_free = FALSE;
    frei0r_path = getenv("FREI0R_PATH");
    if (!frei0r_path || !*frei0r_path) {
      get_string_pref(PREF_FREI0R_PATH, prefs->frei0r_path, PATH_MAX);
      if (!*prefs->frei0r_path) frei0r_path =
          lives_strdup_printf("/usr/lib/frei0r-1:/usr/local/lib/frei0r-1:%s/frei0r-1",
                              capable->home_dir);
      else frei0r_path = lives_strdup(prefs->frei0r_path);
      lives_setenv("FREI0R_PATH", frei0r_path);
      needs_free = TRUE;
    }
    if (!frei0r_path) frei0r_path = "";
    lives_snprintf(prefs->frei0r_path, PATH_MAX, "%s", frei0r_path);
    if (needs_free) lives_free(frei0r_path);

    needs_free = FALSE;
    ladspa_path = getenv("LADSPA_PATH");
    if (!ladspa_path || !*ladspa_path) {
      get_string_pref(PREF_LADSPA_PATH, prefs->ladspa_path, PATH_MAX);
      if (!*prefs->ladspa_path) ladspa_path = lives_build_filename(prefs->lib_dir, "ladspa", NULL);
      else ladspa_path = lives_strdup(prefs->ladspa_path);
      lives_setenv("LADSPA_PATH", ladspa_path);
      needs_free = TRUE;
    }
    if (!ladspa_path) ladspa_path = "";
    if (!*prefs->ladspa_path && *ladspa_path)
      lives_snprintf(prefs->ladspa_path, PATH_MAX, "%s", ladspa_path);
    if (needs_free) lives_free(ladspa_path);

    needs_free = FALSE;
    libvis_path = getenv("VISUAL_PLUGIN_PATH");
    if (!libvis_path || !*libvis_path) {
      get_string_pref(PREF_LIBVISUAL_PATH, prefs->libvis_path, PATH_MAX);
      if (*prefs->libvis_path) lives_setenv("VISUAL_PLUGIN_PATH", prefs->libvis_path);
    }
    if (!libvis_path) libvis_path = "";
    if (!*prefs->libvis_path && *libvis_path)
      lives_snprintf(prefs->libvis_path, PATH_MAX, "%s", libvis_path);

    splash_msg(_("Loading realtime effect plugins..."), SPLASH_LEVEL_LOAD_RTE);
    weed_load_all();

    // replace any multi choice effects with their delegates
    replace_with_delegates();

    threaded_dialog_spin(0.);
    load_default_keymap();
    threaded_dialog_spin(0.);

    future_prefs->audio_opts = prefs->audio_opts = get_int_prefd(PREF_AUDIO_OPTS, 3);

#ifdef ENABLE_JACK
    lives_snprintf(prefs->jack_aserver, PATH_MAX, "%s/.jackdrc", capable->home_dir);
    lives_snprintf(prefs->jack_tserver, PATH_MAX, "%s/.jackdrc", capable->home_dir);
#endif

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

    mainw->recovery_file = lives_strdup_printf("%s/recovery.%d.%d.%d", prefs->workdir, lives_getuid(),
                           lives_getgid(), capable->mainpid);

    if (capable->has_jackd) naudp++;
    if (capable->has_pulse_audio) naudp++;
    if (capable->has_sox_play) naudp++;

    if (naudp > 1) {
      if (prefs->startup_phase > 0 && prefs->startup_phase <= 4) {
        splash_end();
        if (!do_audio_choice_dialog(prefs->startup_phase)) {
          lives_exit(0);
        }
        if (prefs->audio_player == AUD_PLAYER_JACK) future_prefs->jack_opts = prefs->jack_opts = JACK_OPTS_START_ASERVER;
        else future_prefs->jack_opts = prefs->jack_opts = 0;
        set_int_pref(PREF_JACK_OPTS, prefs->jack_opts);

        prefs->startup_phase = 4;
        set_int_pref(PREF_STARTUP_PHASE, 4);
      }

      // audio startup
#ifdef ENABLE_JACK
      if (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER || prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT ||
          prefs->jack_opts & JACK_OPTS_START_ASERVER ||
          prefs->jack_opts & JACK_OPTS_START_TSERVER) {
        // start jack transport polling
        if (prefs->jack_opts & JACK_OPTS_START_ASERVER) splash_msg(_("Starting jack audio server..."),
              SPLASH_LEVEL_LOAD_APLAYER);
        else {
          if (prefs->jack_opts & JACK_OPTS_START_TSERVER) splash_msg(_("Starting jack transport server..."),
                SPLASH_LEVEL_LOAD_APLAYER);
          else splash_msg(_("Connecting to jack server..."), SPLASH_LEVEL_LOAD_APLAYER);
        }
        if (!lives_jack_init()) {
          if ((prefs->jack_opts & JACK_OPTS_START_ASERVER) || (prefs->jack_opts & JACK_OPTS_START_TSERVER))
            do_jack_noopen_warn();
          else do_jack_noopen_warn3();
          if (prefs->startup_phase == 4) {
            do_jack_noopen_warn2();
          }
          future_prefs->jack_opts = 0; // jack is causing hassle, get rid of it
          set_int_pref(PREF_JACK_OPTS, 0);
          lives_exit(0);
        }
      }

      if (prefs->audio_player == AUD_PLAYER_JACK) {
        jack_audio_init();
        jack_audio_read_init();
        mainw->jackd = jack_get_driver(0, TRUE);
        if (mainw->jackd) {
          if (!jack_create_client_writer(mainw->jackd)) mainw->jackd = NULL;

          if (!mainw->jackd && prefs->startup_phase == 0) {
#ifdef HAVE_PULSE_AUDIO
            char *otherbit = lives_strdup("\"lives -aplayer pulse\".");
#else
            char *otherbit = lives_strdup("\"lives -aplayer sox\".");
#endif
            char *tmp;

            char *msg = lives_strdup_printf(
                          _("\n\nManual start of jackd required. Please make sure jackd is running, \n"
                            "or else change the value of <jack_opts> in %s to 16\nand restart LiVES.\n\n"
                            "Alternatively, try to start lives with either \"lives -jackopts 16\", or "),
                          (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
            fprintf(stderr, "%s%s\n\n", msg, otherbit);
            lives_free(msg);
            lives_free(tmp);
            lives_free(otherbit);
          }

          if (!mainw->jackd) {
            do_jack_noopen_warn3();
            if (prefs->startup_phase == 4) {
              do_jack_noopen_warn2();
            } else do_jack_noopen_warn4();
            lives_exit(0);
          }

          mainw->jackd->whentostop = &mainw->whentostop;
          mainw->jackd->cancelled = &mainw->cancelled;
          mainw->jackd->in_use = FALSE;
          mainw->jackd->play_when_stopped = (prefs->jack_opts & JACK_OPTS_NOPLAY_WHEN_PAUSED)
                                            ? FALSE : TRUE;

          jack_write_driver_activate(mainw->jackd);

          if (prefs->perm_audio_reader) {
            // create reader connection now, if permanent
            jack_rec_audio_to_clip(-1, -1, RECA_EXTERNAL);
	    // *INDENT-OFF*
          }}}
      // *INDENT-ON*
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

        if (prefs->perm_audio_reader) {
          // create reader connection now, if permanent
          pulse_rec_audio_to_clip(-1, -1, RECA_EXTERNAL);
	  // *INDENT-OFF*
        }}}
    // *INDENT-ON*
#endif
  }

  if (prefs->startup_phase != 0) {
    splash_end();
    set_int_pref(PREF_STARTUP_PHASE, 5);
    prefs->startup_phase = 5;
    do_startup_interface_query();

    set_int_pref(PREF_STARTUP_PHASE, 6);
    prefs->startup_phase = 6;

    if (prefs->show_disk_quota && !prefs->vj_mode) {
      if (!disk_monitor_running(prefs->workdir))
        disk_monitor_start(prefs->workdir);

      capable->ds_used = disk_monitor_wait_result(prefs->workdir, LIVES_DEFAULT_TIMEOUT);
      if (capable->ds_used >= 0) {
        ran_ds_dlg = TRUE;
        run_diskspace_dialog();
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

    /* if (prefs->show_splash) { */
    /*   splash_init(); */
    /* } */
  }

  if (strcmp(future_prefs->theme, prefs->theme)) {
    // we set the theme here in case it got reset to 'none'
    set_string_pref(PREF_GUI_THEME, prefs->theme);
    lives_snprintf(future_prefs->theme, 64, "%s", prefs->theme);
  }

  if (mainw->vpp && mainw->vpp->get_audio_fmts) mainw->vpp->audio_codec = get_best_audio(mainw->vpp);
} // end of lives_init


static void show_detected_or_not(boolean cap, const char *pname) {
  if (cap) d_print(_("%s...detected... "), pname);
  else d_print(_("%s...NOT DETECTED... "), pname);
}

#define SHOWDETx(cap, exec) show_detected_or_not(capable->has_##cap, exec)
#define SHOWDET(cap) SHOWDETx(cap, QUOTEME(cap))
//   SHOWDET(, EXEC_);

static void do_start_messages(void) {
  int w, h;
  char *tmp, *endian, *fname, *phase = NULL;

  if (prefs->vj_mode) {
    d_print(_("Starting in VJ MODE: Skipping most startup checks\n"));
#ifndef IS_MINGW
    SHOWDET(wmctrl);
    SHOWDET(xdotool);
    SHOWDET(xwininfo);
#endif
    d_print("\n\n");
    return;
  }

  d_print(_("\nMachine details:\n"));

  get_machine_dets();
  d_print(_("OS is %s %s, running on %s\n"),
          capable->os_name ? capable->os_name : _("unknown"),
          capable->os_release ? capable->os_release : "?",
          capable->os_hardware ? capable->os_hardware : "????");

  d_print(_("CPU type is %s "), capable->cpu_name);
  d_print(P_("(%d core, ", "(%d cores, ", capable->ncpus), capable->ncpus);

  if (capable->byte_order == LIVES_LITTLE_ENDIAN) endian = (_("little endian"));
  else endian = (_("big endian"));
  d_print(_("%d bits, %s)\n"), capable->cpu_bits, endian);
  lives_free(endian);

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
      if (!strcmp(mainw->version_hash, mainw->old_vhash)) {
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
    fname = lives_strdup_printf("%s.recovery.tried.succeeded", prefs->configfile);
    if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
      phase = lives_strdup_printf(_("%s WAS POSSIBLY RECOVERED FROM %s.recovery\n"), prefs->configfile, prefs->configfile);
      d_print("%s", phase);
      lives_free(phase);
    }
    lives_free(fname);
  }


  d_print(_("\nChecking RECOMMENDED dependencies: "));

  SHOWDET(mplayer);
  if (!capable->has_mplayer) {
    SHOWDET(mplayer2);
    if (!capable->has_mplayer2) {
      SHOWDET(mpv);
    }
  }
  //SHOWDET(file);
  SHOWDET(identify);
  if (!capable->has_jackd)
    SHOWDETx(pulse_audio, EXEC_PULSEAUDIO);
  SHOWDETx(sox_sox, EXEC_SOX);
  SHOWDET(convert);
  SHOWDET(composite);
  SHOWDET(ffprobe);
  SHOWDET(gzip);
  SHOWDET(md5sum);
  SHOWDETx(youtube_dl, EXEC_YOUTUBE_DL);

  d_print(_("\n\nChecking OPTIONAL dependencies: "));
  SHOWDET(jackd);
  SHOWDET(python);
  SHOWDET(xwininfo);
  SHOWDETx(cdda2wav, "cdda2wav/icedax");
  SHOWDET(dvgrab);
  SHOWDET(gdb);
  SHOWDETx(gconftool_2, EXEC_GCONFTOOL_2);
  SHOWDETx(xdg_screensaver, EXEC_XDG_SCREENSAVER);

  d_print("\n\n");
}
#undef SHOWDETx
#undef SHOWDET

static void set_toolkit_theme(int prefer) {
  char *lname, *ic_dir;
  //  LiVESList *list;
  char *tmp;

  lives_widget_object_get(gtk_settings_get_default(), "gtk-double-click-time", &capable->dclick_time);
  if (capable->dclick_time <= 0) capable->dclick_time = LIVES_DEF_DCLICK_TIME;

  lives_widget_object_get(gtk_settings_get_default(), "gtk-double-click-distance", &capable->dclick_dist);
  if (capable->dclick_dist <= 0) capable->dclick_dist = LIVES_DEF_DCLICK_DIST;

  lives_widget_object_get(gtk_settings_get_default(), "gtk-font-name", &tmp);

  /// stretch, style, weight not used
  lives_parse_font_string(tmp, &widget_opts.font_name, &widget_opts.font_size, NULL, NULL, NULL);
  lives_free(tmp);

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

  capable->extra_icon_path = lives_build_path(prefs->config_datadir, STOCK_ICONS_DIR, NULL);

  widget_opts.icon_theme = gtk_icon_theme_new();

  gtk_icon_theme_set_custom_theme((LiVESIconTheme *)widget_opts.icon_theme, capable->icon_theme_name);
  gtk_icon_theme_prepend_search_path((LiVESIconTheme *)widget_opts.icon_theme, capable->extra_icon_path);

  ic_dir = lives_build_path(prefs->prefix_dir, ICON_DIR, NULL);
  gtk_icon_theme_prepend_search_path((LiVESIconTheme *)widget_opts.icon_theme, ic_dir);
  lives_free(ic_dir);

  capable->all_icons = gtk_icon_theme_list_icons((LiVESIconTheme *)widget_opts.icon_theme, NULL);
  if (0) {
    LiVESList *list = capable->all_icons;
    for (; list; list = list->next) if (!strncmp((char *)list->data, "gtk-", 4)) g_print("icon: %s\n", (char *)list->data);
  }

  widget_helper_set_stock_icon_alts((LiVESIconTheme *)widget_opts.icon_theme);
}


boolean set_palette_colours(boolean force_reload) {
  // force_reload should only be set when the theme changes in prefs.
  lives_colRGBA64_t lcol;
  LiVESList *cache_backup;

  char *themedir, *themefile, *othemefile, *fname, *tmp;
  char *pstyle, *colref;

  boolean is_OK = TRUE;
  boolean cached = FALSE;

#ifndef VALGRIND_ON
  double lmin, lmax;
  uint8_t ncr, ncg, ncb;
#endif

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
  } else if (get_colour_pref(THEME_DETAIL_STYLE, &lcol)) {
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
    set_palette_prefs();
  }

#ifndef VALGRIND_ON
  /// generate some complementary colours
  // still experimenting...some values may need tweaking
  // suggested uses for each colour in the process of being defined
  // TODO - run a bg thread until we create GUI
  if (!prefs->vj_mode) {
    if (!(palette->style & STYLE_LIGHT)) {
      lmin = .05; lmax = .4;
    } else {
      lmin = .6; lmax = .8;
    }
    ncr = palette->menu_and_bars.red * 255.;
    ncg = palette->menu_and_bars.green * 255.;
    ncb = palette->menu_and_bars.blue * 255.;
    prefs->pb_quality = PB_QUALITY_HIGH;
    if (pick_nice_colour(palette->normal_back.red * 255., palette->normal_back.green * 255.,
                         palette->normal_back.blue * 255., &ncr, &ncg, &ncb, 1.5, .25, .75)) {
      // nice1 - used for outlines
      palette->nice1.red = LIVES_WIDGET_COLOR_SCALE_255(ncr);
      palette->nice1.green = LIVES_WIDGET_COLOR_SCALE_255(ncg);
      palette->nice1.blue = LIVES_WIDGET_COLOR_SCALE_255(ncb);
      palette->nice1.alpha = 1.;

      ncr = palette->menu_and_bars.red * 255.;
      ncg = palette->menu_and_bars.green * 255.;
      ncb = palette->menu_and_bars.blue * 255.;

      if (pick_nice_colour(palette->nice1.red * 255., palette->nice1.green * 255.,
                           palette->nice1.blue * 255., &ncr, &ncg, &ncb, 1., lmin, lmax)) {
        // nice2 - alt for menu_and_bars
        // insensitive colour ?
        palette->nice2.red = LIVES_WIDGET_COLOR_SCALE_255(ncr);
        palette->nice2.green = LIVES_WIDGET_COLOR_SCALE_255(ncg);
        palette->nice2.blue = LIVES_WIDGET_COLOR_SCALE_255(ncb);
        palette->nice2.alpha = 1.;
        mainw->pretty_colours = TRUE;

        if (!(palette->style & STYLE_LIGHT)) {
          lmin = .6; lmax = .8;
        } else {
          lmin = .2; lmax = .4;
        }
        pick_nice_colour(palette->normal_fore.red * 255., palette->normal_fore.green * 255.,
                         palette->normal_fore.blue * 255., &ncr, &ncg, &ncb, 1., lmin, lmax);
        // nice3 - alt for menu_and_bars_fore
        palette->nice3.red = LIVES_WIDGET_COLOR_SCALE_255(ncr);
        palette->nice3.green = LIVES_WIDGET_COLOR_SCALE_255(ncg);
        palette->nice3.blue = LIVES_WIDGET_COLOR_SCALE_255(ncb);
        palette->nice3.alpha = 1.;
      }
    }
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

  if (prefs->extra_colours && mainw->pretty_colours) {
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "combobox window menu menuitem", "border-width", "2px");
    colref = gdk_rgba_to_string(&palette->nice1);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "combobox window menu menuitem", "border-color", colref);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_ACTIVE, "scrollbar slider", "background-color", colref);
    tmp = lives_strdup_printf("0 0 0 4px %s inset", colref);
    set_css_value_direct(NULL, LIVES_WIDGET_STATE_PRELIGHT, "combobox window menu menuitem", "box-shadow", tmp);
    lives_free(tmp);
    lives_free(colref);
  }

  tmp = lives_strdup_printf("%dpx", ((widget_opts.css_min_height * 3 + 3) >> 2) << 1);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "combobox window menu menuitem", "min-height", tmp);
  lives_free(tmp);
  colref = gdk_rgba_to_string(&palette->menu_and_bars_fore);
  set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "scrollbar", "color", colref);
  lives_free(colref);

  //set_css_value_direct(NULL, LIVES_WIDGET_STATE_INSENSITIVE, "spinbutton button", "opacity", "0.5");

  return TRUE;
}


capability *get_capabilities(void) {
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

  capable->has_perl = UNCHECKED;
  if (!has_executable(EXEC_PERL)) return capable;
  capable->has_perl = PRESENT;

  // this is _compile time_ bits, not runtime bits
  capable->cpu_bits = (sizeof(void *)) * 8;

  capable->ds_used = capable->ds_free = capable->ds_tot = -1;

  capable->mainpid = lives_getpid();

  get_location("cp", capable->cp_cmd, PATH_MAX);
  capable->sysbindir = get_dir(capable->cp_cmd);

  get_location("mv", capable->mv_cmd, PATH_MAX);
  get_location("ln", capable->ln_cmd, PATH_MAX);
  get_location("chmod", capable->chmod_cmd, PATH_MAX);
  get_location("cat", capable->cat_cmd, PATH_MAX);
  get_location("echo", capable->echo_cmd, PATH_MAX);
  get_location("eject", capable->eject_cmd, PATH_MAX);

  capable->wm_name = NULL;
  capable->wm_type = NULL;

  capable->python_version = 0;
  capable->xstdout = STDOUT_FILENO;

  // optional
  capable->has_mplayer = UNCHECKED;
  capable->has_mplayer2 = UNCHECKED;
  capable->has_mpv = UNCHECKED;
  capable->has_convert = UNCHECKED;
  capable->has_ffprobe = UNCHECKED;
  capable->has_ffmpeg = UNCHECKED;
  capable->has_composite = UNCHECKED;
  capable->has_identify = UNCHECKED;
  capable->has_sox_play = UNCHECKED;
  capable->has_sox_sox = UNCHECKED;
  capable->has_dvgrab = UNCHECKED;
  capable->has_cdda2wav = UNCHECKED;
  capable->has_autolives = UNCHECKED;
  capable->has_jackd = UNCHECKED;
  capable->has_gdb = UNCHECKED;
  capable->has_icedax = UNCHECKED;
  capable->has_du = UNCHECKED;
  capable->has_pulse_audio = UNCHECKED;
  capable->has_xwininfo = UNCHECKED;
  capable->has_midistartstop = UNCHECKED;
  capable->has_encoder_plugins = UNCHECKED;
  capable->has_python = UNCHECKED;
  capable->has_gconftool_2 = UNCHECKED;
  capable->has_xdg_screensaver = UNCHECKED;
  capable->has_file = UNCHECKED;
  capable->has_md5sum = UNCHECKED;
  capable->has_gio = UNCHECKED;
  capable->has_xdotool = UNCHECKED;
  capable->has_wmctrl = UNCHECKED;

  // not checked at startup
  capable->has_gzip = UNCHECKED;
  capable->has_wget = UNCHECKED;
  capable->has_curl = UNCHECKED;
  capable->has_mktemp = UNCHECKED;
  capable->has_snap = UNCHECKED;

  capable->has_smogrify = UNCHECKED;

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

    lives_chomp(buffer);
    numtok = get_token_count(buffer, ' ') ;
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
          dir_valid = FALSE;
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
        prefs->startup_phase = -1;
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

  capable->has_md5sum = has_executable(EXEC_MD5SUM);
  capable->has_du = has_executable(EXEC_DU);
  capable->has_ffprobe = has_executable(EXEC_FFPROBE); ///< backend only
  capable->has_sox_play = has_executable(EXEC_PLAY);
  //}
  capable->has_youtube_dl = has_executable(EXEC_YOUTUBE_DL);
  capable->has_sox_sox = has_executable(EXEC_SOX);
  capable->has_dvgrab = has_executable(EXEC_DVGRAB);
  if (!(capable->has_cdda2wav = has_executable(EXEC_CDDA2WAV))) {
    capable->has_cdda2wav = has_executable(EXEC_ICEDAX);
  }
  capable->has_jackd = has_executable(EXEC_JACKD);
  capable->has_pulse_audio = has_executable(EXEC_PULSEAUDIO);
  if ((capable->has_python = has_executable(EXEC_PYTHON))) {
    capable->python_version = get_version_hash(EXEC_PYTHON " -V 2>&1", " ", 1);
  }
  capable->has_xwininfo = has_executable(EXEC_XWININFO);
  capable->has_gconftool_2 = has_executable(EXEC_GCONFTOOL_2);
  capable->has_xdg_screensaver = has_executable(EXEC_XDG_SCREENSAVER);
  if (has_executable(EXEC_MIDISTART)) {
    capable->has_midistartstop = has_executable(EXEC_MIDISTOP);
  }

  capable->ncpus = get_num_cpus();
  if (capable->ncpus == 0) capable->ncpus = 1;

  return capable;
}


void print_opthelp(void) {
  char *tmp;
  print_notice();

  lives_printerr(_("\nStartup syntax is: %s [OPTS] [filename [start_time] [frames]]\n"), capable->myname);
  fprintf(stderr, "%s", _("Where: filename is the name of a media file or backup file to import\n"));
  fprintf(stderr, "%s", _("start_time : filename start time in seconds\n"));
  fprintf(stderr, "%s", _("frames : maximum number of frames to open\n"));
  fprintf(stderr, "%s", "\n");
  fprintf(stderr, "%s", _("OPTS can be:\n"));
  fprintf(stderr, "%s", _("-help | --help \t\t\t: print this help text on stderr and exit\n"));
  fprintf(stderr, "%s", _("-version | --version\t\t: print the LiVES version on stderr and exit\n"));
  fprintf(stderr, "%s", _("-workdir <workdir>\t\t: specify the working directory for the session, "
                          "overriding any value set in preferences\n"));
  fprintf(stderr, "%s", _("\t\t\t\t\t(disables any disk quota checking)"));
  fprintf(stderr, "%s", _("-configfile <path_to_file>\t: override the default configuration file for the session\n"));
  tmp = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_DIR, "lives", LIVES_DEF_CONFIG_FILE, NULL);
  fprintf(stderr, _("\t\t\t\t\t(default is %s)\n"), tmp);
  lives_free(tmp);

  tmp = lives_build_path(capable->home_dir, LOCAL_HOME_DIR, LIVES_DEF_CONFIG_DATADIR, NULL);
  fprintf(stderr, "%s", _("-configdatadir <dir>\t\t: override the default configuration data directory for the session\n"));
  fprintf(stderr, _("\t\t\t\t\t(default is %s\n"), tmp);
  lives_free(tmp);

  fprintf(stderr, "%s", _("-dscrit <bytes>\t\t\t: temporarily sets the free disk space critical level for workdir to <bytes>\n"));
  fprintf(stderr, "%s", _("\t\t\t\t\t(intended to allow correction of erroneous values within the app; "
                          "<= 0 disables checks)\n"));
  fprintf(stderr, "%s", _("-set <setname>\t\t\t: autoload clip set <setname>\n"));
  fprintf(stderr, "%s", _("-noset\t\t\t\t: do not reload any clip set on startup (overrides -set)\n"));
  fprintf(stderr, "%s", _("-layout <layout_name>\t\t: autoload multitrack layout <layout_name> (if successful, "
                          "overrides -startup-ce)\n"));
  fprintf(stderr, "%s", _("-nolayout\t\t\t: do not reload any multitrack layout on startup (overrides -layout)\n"));
  fprintf(stderr, "%s", _("-norecover\t\t\t: force non-loading of crash recovery files (overrides -recover / -autorecover)\n"));
  fprintf(stderr, "%s",
          _("-recover | -autorecover\t\t: force reloading of any crash recovery files (may override -noset and -nolayout)\n"));
  fprintf(stderr, "%s", _("-nogui\t\t\t\t: do not show the gui (still shows the play window when active)\n"));
  fprintf(stderr, "%s", _("-nosplash\t\t\t: do not show the splash window\n"));
  fprintf(stderr, "%s",
          _("-noplaywin\t\t\t: do not show the play window (still shows the internal player; intended for remote streaming)\n"));
  fprintf(stderr, "%s",
          _("-noninteractive\t\t\t: disable menu interactivity (intended for scripting applications, e.g liblives)\n"));
  fprintf(stderr, "%s", _("-startup-ce\t\t\t: start in clip editor mode (overrides -startup-mt)\n"));
  fprintf(stderr, "%s", _("-startup-mt\t\t\t: start in multitrack mode\n"));
  fprintf(stderr, "%s", _("-vjmode\t\t\t\t: start in VJ mode (implicitly sets -startup-ce -autorecover "
                          "-nolayout -asource external)\n"));
  fprintf(stderr, "%s",
          _("-fxmodesmax <n>\t\t\t: allow <n> modes per effect key (overrides any value set in preferences; minimum is 1)\n"));
#ifdef ENABLE_OSC
  fprintf(stderr,  _("-oscstart <port>\t\t: start OSC listener on UDP port <port> (default is %d)\n"), DEF_OSC_LISTEN_PORT);
  fprintf(stderr, "%s",
          _("-nooscstart\t\t\t: do not start the OSC listener (the default, unless set in preferences)\n"));
#endif
  fprintf(stderr, "%s",
          _("-asource <source>\t\t: set the initial audio source (<source> can be 'internal' or 'external')\n"));
  fprintf(stderr, _("\t\t\t\t\t(only valid for %s and %s players)\n"), AUDIO_PLAYER_JACK, AUDIO_PLAYER_PULSE_AUDIO);
  fprintf(stderr, "%s", _("-aplayer <ap>\t\t\t: start with the selected audio player (<ap> can be: "));
#ifdef HAVE_PULSE_AUDIO
  fprintf(stderr, "'%s'", AUDIO_PLAYER_PULSE);
#endif
#ifdef ENABLE_JACK
#ifdef HAVE_PULSE_AUDIO
  fprintf(stderr, ", "); // comma after pulse
#endif
  fprintf(stderr, "'%s'", AUDIO_PLAYER_JACK);
  if (capable->has_sox_play) lives_printerr(", '%s'", AUDIO_PLAYER_SOX); // comma after jack
  fprintf(stderr, " or '%s')\n", AUDIO_PLAYER_NONE);
  fprintf(stderr, "%s",
          _("-jackopts <opts>\t\t: opts is a bitmap of jackd startup / playback options (default is 16, "
            "unless set in Preferences)\n"
            "\t\t\t\t\t 1 = LiVES is a jack transport slave, \n"
            "\t\t\t\t\t 2 = LiVES is a jack transport master, \n"
            "\t\t\t\t\t 4 = start/stop jack transport server on LiVES playback start / stop\n"
            "\t\t\t\t\t\t(must be transport master), \n"
            "\t\t\t\t\t 8 = pause jack transport when video paused\n"
            "\t\t\t\t\t\t(must be transport master),\n"
            "\t\t\t\t\t16 = start/stop jack audio server on LiVES startup / shutdown\n"
            "\t\t\t\t\t\t(only if audio player is jack)) \n"));
#else // no jack
  if (capable->has_sox_play) {
#ifdef HAVE_PULSE_AUDIO
    fprintf(stderr, ", "); // comma after pulse
#endif
    fprintf(stderr, _("'%s' or "), AUDIO_PLAYER_SOX);
  }
#ifdef HAVE_PULSE_AUDIO
  else fprintf(stderr, "%s", _(" or ")); // no sox, 'or' after pulse
#endif
  fprintf(stderr, "'%s')\n", AUDIO_PLAYER_NONE);
#endif
  fprintf(stderr, "%s", _("-devicemap <mapname>\t\t: autoload devicemap <mapname> (for MIDI / joystick control)\n"));
  fprintf(stderr, "%s", _("-vppdefaults <file>\t\t: load defaults for video playback plugin from <file>\n"
                          "\t\t\t\t\t(Note: only affects the plugin settings, not the plugin type)\n"));
#ifdef HAVE_YUV4MPEG
  fprintf(stderr, "%s",  _("-yuvin <fifo>\t\t\t: autoplay yuv4mpeg from stream <fifo> on startup\n"));
  fprintf(stderr, "%s", _("\t\t\t\t\t(only valid in clip edit startup mode)\n"));
#endif
  fprintf(stderr, "%s", _("-debug\t\t\t\t: try to debug crashes (requires 'gdb' to be installed)\n"));
  fprintf(stderr, "%s", "\n");
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
  norecurse = TRUE;
  if (!is_recovery || mt_load_recovery_layout(NULL)) {
    if (mainw->event_list) {
      if (mainw->multitrack) {
        /// exit multitrack, backup mainw->event_as it will get set to NULL
        weed_plant_t *backup_elist = mainw->event_list;
        multitrack_delete(mainw->multitrack, FALSE);
        mainw->event_list = backup_elist;
      }

      deal_with_render_choice(is_recovery);
      if (is_recovery && mainw->multitrack) rec_recovered = TRUE;
    }
  }
  if (is_recovery) mainw->recording_recovered = rec_recovered;
  norecurse = FALSE;
  return FALSE;
}


boolean lazy_startup_checks(void *data) {
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
      int bx, by;
      get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
      if (by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
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
        mainw->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval, 0);
        capable->ds_free = dsval;
        if (capable->ds_used < 0)
          capable->ds_used = disk_monitor_check_result(prefs->workdir);
        if (!prefs->disk_quota && (mainw->ds_status == LIVES_STORAGE_STATUS_NORMAL
                                   || mainw->ds_status == LIVES_STORAGE_STATUS_UNKNOWN)) {
          if (capable->ds_used < 0) disk_monitor_forget();
        } else {
          if (capable->ds_used < 0) {
            capable->ds_used = disk_monitor_wait_result(prefs->workdir, LIVES_DEFAULT_TIMEOUT);
          }
        }
      }
      mainw->helper_procthreads[PT_LAZY_DSUSED] = NULL;
      if (capable->ds_used > prefs->disk_quota * .9 || (mainw->ds_status != LIVES_STORAGE_STATUS_NORMAL
          && mainw->ds_status != LIVES_STORAGE_STATUS_UNKNOWN)) {
        if (capable->ds_used > prefs->disk_quota * .9 && (mainw->ds_status == LIVES_STORAGE_STATUS_NORMAL
            || mainw->ds_status == LIVES_STORAGE_STATUS_UNKNOWN)) {
          mainw->ds_status = LIVES_STORAGE_STATUS_OVER_QUOTA;
        }
        do_show_quota = TRUE;
      }
    }
    if (do_show_quota) {
      run_diskspace_dialog();
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
    lives_widget_destroy(mainw->ldg_menuitem);
    mainw->ldg_menuitem = NULL;
    add_rfx_effects2(RFX_STATUS_ANY);
    if (LIVES_IS_SENSITIZED) sensitize(); // call fn again to sens. new menu entries
  }

  mainw->lazy = 0;
  return FALSE;
}


boolean resize_message_area(livespointer data) {
  // workaround because the window manager will resize the window asynchronously
  static boolean isfirst = TRUE;
  int bx, by;

  if (data) isfirst = TRUE;

  if (!prefs->show_gui || LIVES_IS_PLAYING || mainw->is_processing || mainw->is_rendering || !prefs->show_msg_area) {
    mainw->assumed_height = mainw->assumed_width = -1;
    mainw->idlemax = 0;
    return FALSE;
  }

  if (mainw->idlemax-- == DEF_IDLE_MAX) mainw->msg_area_configed = FALSE;

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);

  if (mainw->idlemax == DEF_IDLE_MAX / 2 && prefs->open_maximised && (by > 0 || bx > 0)) {
    if (by > MENU_HIDE_LIM)
      lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    mainw->assumed_height = mainw->assumed_width = -1;
    return TRUE;
  }

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
    lives_widget_set_vexpand(mainw->msg_area, TRUE);
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
  lives_widget_queue_draw(mainw->LiVES);
  return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
static boolean got_files = FALSE;
static boolean lives_startup2(livespointer data);
static boolean lives_startup(livespointer data) {
  // this is run in an idlefunc

  char *tmp, *tmp2, *msg;

  // check the working directory
  if (needs_workdir) {
    // get initial workdir
    if (!do_workdir_query()) {
      lives_exit(0);
    }
    prefs->startup_phase = 2;
    set_int_pref(PREF_STARTUP_PHASE, 2);
  }
  if (prefs->startup_phase > 0 && prefs->startup_phase < 3) {
    if (!do_startup_tests(FALSE)) {
      lives_exit(0);
    }
    prefs->startup_phase = 3;
    set_int_pref(PREF_STARTUP_PHASE, 3);

    // we can show this now
    if (prefs->show_splash) splash_init();
  }

  if (newconfigfile || prefs->startup_phase == 3) {
    /// CREATE prefs->config_datadir, and default items inside it
    build_init_config(prefs->config_datadir, ign_opts.ign_config_datadir);
  }

  get_string_pref(PREF_VID_PLAYBACK_PLUGIN, buff, 256);

  if (*buff && strcmp(buff, "(null)") && strcmp(buff, "none")) {
    mainw->vpp = open_vid_playback_plugin(buff, TRUE);
  } else if (prefs->startup_phase == 3) {
    mainw->vpp = open_vid_playback_plugin(DEFAULT_VPP, TRUE);
    if (mainw->vpp) {
      lives_snprintf(future_prefs->vpp_name, 64, "%s", mainw->vpp->name);
      set_string_pref(PREF_VID_PLAYBACK_PLUGIN, mainw->vpp->name);
    }
  }

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
    int bx, by;
    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    if (by > MENU_HIDE_LIM)
      lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  // needed to avoid priv->pulse2 > priv->pulse1 gtk error
  lives_widget_context_update();

  lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
  lives_widget_context_update();

  mainw->startup_error = FALSE;

  if (theme_error && !mainw->foreign) {
    // non-fatal errors
    msg = lives_strdup_printf(
            _("\n\nThe theme you requested (%s) could not be located.\n"
              "Please make sure you have the themes installed in\n%s/%s.\n"
              "(Maybe you need to change the value of <prefix_dir> in your %s file)\n"
              "or you may be missing a custom theme.\n"), future_prefs->theme,
            (tmp = lives_filename_to_utf8(prefs->prefix_dir, -1, NULL, NULL, NULL)), THEME_DIR,
            (tmp2 = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
    lives_free(tmp2);
    lives_free(tmp);
    startup_message_nonfatal(msg);
    lives_free(msg);
    lives_snprintf(prefs->theme, 64, LIVES_THEME_NONE);
    upgrade_error = TRUE;
  }

  lives_init(&ign_opts);

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
        if (!capable->has_encoder_plugins) {
          msg = lives_strdup_printf(
                  _("\nLiVES was unable to find any encoder plugins.\n"
                    "Please check that you have them installed correctly in\n%s%s%s/\n"
                    "You will not be able to 'Save' without them.\n"
                    "You may need to change the value of <lib_dir> in %s\n"),
                  prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_ENCODERS,
                  (tmp = lives_filename_to_utf8(prefs->configfile, -1, NULL, NULL, NULL)));
          lives_free(tmp);
          startup_message_nonfatal_dismissable(msg, WARN_MASK_NO_ENCODERS);
          lives_free(msg);
          upgrade_error = TRUE;
        }

        if (mainw->next_ds_warn_level > 0) {
          if (mainw->ds_status == LIVES_STORAGE_STATUS_WARNING) {
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
    splash_msg(_("Loading rendered effect plugins..."), SPLASH_LEVEL_LOAD_RFX);
    // must call this at least to set up rendered_fx[0]
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
    mainw->rec_fps = strtod(zargv[9], NULL);
    mainw->rec_arate = atoi(zargv[10]);
    mainw->rec_asamps = atoi(zargv[11]);
    mainw->rec_achans = atoi(zargv[12]);
    mainw->rec_signed_endian = atoi(zargv[13]);

    if (zargc > 14) {
      mainw->foreign_visual = lives_strdup(zargv[14]);
      if (!strcmp(mainw->foreign_visual, "(null)")) {
        lives_free(mainw->foreign_visual);
        mainw->foreign_visual = NULL;
      }
    }

#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd && mainw->rec_achans > 0) {
      lives_jack_init();
      jack_audio_read_init();
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

  if (prefs->startup_phase == 100) {
    if (upgrade_error) {
      do_upgrade_error_dialog();
    }
    prefs->startup_phase = 0;
  }

  // splash_end() will start up multitrack if in STARTUP_MT mode
  if (*start_file && strcmp(start_file, "-")) {
    splash_end();
    deduce_file(start_file, start, end);
    got_files = TRUE;
  } else {
    set_main_title(NULL, 0);
    splash_end();
  }

  if (prefs->startup_phase == 0) show_lives();
  mainw->is_ready = TRUE;

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


static boolean lives_startup2(livespointer data) {
  char *ustr;
  boolean layout_recovered = FALSE;

  if (prefs->crash_recovery && !no_recover) got_files = check_for_recovery_files(auto_recover);

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

  mainw->no_switch_dprint = TRUE;
  if (mainw->current_file > -1 && !mainw->multitrack) {
    switch_clip(1, mainw->current_file, TRUE);
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

  if (!mainw->multitrack) {
    if (mainw->current_file == -1) {
      resize(1.);
      if (prefs->show_msg_area) {
        // the message area must fit exactly to the screen size, so we update it in an idle function
        // due to the fact that the window manager may resize the window asynchronously
        if (mainw->idlemax == 0)
          lives_idle_add_simple(resize_message_area, NULL);
        mainw->idlemax = DEF_IDLE_MAX;
      }
    }
  } else {
    lives_idle_add_simple(mt_idle_show_current_frame, (livespointer)mainw->multitrack);
    if (mainw->multitrack->idlefunc == 0) {
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }
  }

  mainw->go_away = FALSE;
  if (!mainw->multitrack) sensitize();

  if (prefs->vj_mode) {
    char *wid = lives_strdup_printf("0x%08lx",
                                    (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET)));
    if (wid) activate_x11_window(wid);
  }
  if (mainw->recording_recovered) {
    lives_idle_add_simple(render_choice_idle, LIVES_INT_TO_POINTER(TRUE));
  }

  mainw->overlay_alarm = lives_alarm_set(0);

  if (!mainw->multitrack)
    lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_CE);
  else
    lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_MT);

  if (!prefs->vj_mode && !prefs->startup_phase) {
    mainw->helper_procthreads[PT_LAZY_RFX] =
      lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)add_rfx_effects, -1, "i", RFX_STATUS_ANY);
  }

  mainw->lazy = lives_idle_add_simple(lazy_startup_checks, NULL);

  // timer to poll for external commands: MIDI, joystick, jack transport, osc, etc.
  mainw->kb_timer = lives_timer_add_simple(EXT_TRIGGER_INTERVAL, &ext_triggers_poll, NULL);

  if (!CURRENT_CLIP_IS_VALID) lives_ce_update_timeline(0, 0.);

  if (newconfigfile) {
    cleanup_old_config();
    lives_free(newconfigfile);
  }

  if (prefs->interactive) set_interactive(TRUE);

  return FALSE;
} // end lives_startup2()


void set_signal_handlers(SignalHandlerPointer sigfunc) {
  sigset_t smask;
  struct sigaction sact;

  sigemptyset(&smask);

#define USE_GLIB_SIGHANDLER
#ifdef USE_GLIB_SIGHANDLER
  g_unix_signal_add(LIVES_SIGINT, glib_sighandler, LIVES_INT_TO_POINTER(LIVES_SIGINT));
  g_unix_signal_add(LIVES_SIGTERM, glib_sighandler, LIVES_INT_TO_POINTER(LIVES_SIGTERM));
#else
  sigaddset(&smask, LIVES_SIGINT);
  sigaddset(&smask, LIVES_SIGTERM);
#endif

  sigaddset(&smask, LIVES_SIGSEGV);
  sigaddset(&smask, LIVES_SIGABRT);

  sact.sa_handler = sigfunc;
  sact.sa_flags = 0;
  sact.sa_mask = smask;

  sigaction(LIVES_SIGINT, &sact, NULL);
  sigaction(LIVES_SIGTERM, &sact, NULL);
  sigaction(LIVES_SIGSEGV, &sact, NULL);
  sigaction(LIVES_SIGABRT, &sact, NULL);

  if (mainw) {
    if (sigfunc == defer_sigint) mainw->signals_deferred = TRUE;
    else mainw->signals_deferred = FALSE;
  }
}


int real_main(int argc, char *argv[], pthread_t *gtk_thread, ulong id) {
  weed_error_t werr;
  ssize_t mynsize;
  char cdir[PATH_MAX];
  boolean toolong = FALSE;
  char *tmp, *dir, *msg;
  pthread_mutexattr_t mattr;
#ifndef IS_LIBLIVES
  weed_plant_t *test_plant;
#endif

  mainw = NULL;
  prefs = NULL;
  capable = NULL;

  set_signal_handlers((SignalHandlerPointer)catch_sigint);

  lives_memset(&ign_opts, 0, sizeof(ign_opts));

#ifdef ENABLE_OIL
  oil_init();
#endif

  init_memfuncs();

#ifdef IS_LIBLIVES
#ifdef GUI_GTK
  if (gtk_thread) {
    pthread_create(gtk_thread, NULL, gtk_thread_wrapper, NULL);
  }
#endif
#endif

  capable = (capability *)lives_calloc(1, sizeof(capability));
  capable->cacheline_size = sizeof(void *) * 8;

  // _runtime_ byte order, needed for lives_strlen and other things
  if (IS_BIG_ENDIAN)
    capable->byte_order = LIVES_BIG_ENDIAN;
  else
    capable->byte_order = LIVES_LITTLE_ENDIAN;

  capable->main_thread = pthread_self();

  zargc = argc;
  zargv = argv;

  //setlocale(LC_ALL, "");

  // force decimal point to be a "."
  putenv("LC_NUMERIC=C");
  setlocale(LC_NUMERIC, "C");

#ifdef ENABLE_NLS
  textdomain(GETTEXT_PACKAGE);
  bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef UTF8_CHARSET
  bind_textdomain_codeset(GETTEXT_PACKAGE, nl_langinfo(CODESET));
#endif
#endif

#ifdef GDK_WINDOWING_X11
  XInitThreads();
#endif

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_init();
#else
  gtk_init(&argc, &argv);
#endif
#endif

  g_log_set_default_handler(lives_log_handler, NULL);

  //gtk_window_set_interactive_debugging(TRUE);
#ifndef LIVES_NO_DEBUG
  g_printerr("FULL DEBUGGING IS ON !!\n");
#endif

#ifndef IS_LIBLIVES
  // start up the Weed system
  weed_abi_version = weed_get_abi_version();
  if (weed_abi_version > WEED_ABI_VERSION) weed_abi_version = WEED_ABI_VERSION;
  //werr = weed_init(weed_abi_version, WEED_INIT_DEBUGMODE);
  werr = weed_init(weed_abi_version, 0);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }
#ifndef USE_STD_MEMFUNCS
  weed_utils_set_custom_memfuncs(lives_malloc, lives_calloc, lives_memcpy, NULL, lives_free);
#endif
#endif

  // backup the core functions so we can override them
  _weed_plant_new = weed_plant_new;
  _weed_plant_free = weed_plant_free;
  _weed_leaf_set = weed_leaf_set;
  _weed_leaf_get = weed_leaf_get;
  _weed_leaf_delete = weed_leaf_delete;
  _weed_plant_list_leaves = weed_plant_list_leaves;
  _weed_leaf_num_elements = weed_leaf_num_elements;
  _weed_leaf_element_size = weed_leaf_element_size;
  _weed_leaf_seed_type = weed_leaf_seed_type;
  _weed_leaf_get_flags = weed_leaf_get_flags;
  _weed_leaf_set_flags = weed_leaf_set_flags;

  mainw = (mainwindow *)(lives_calloc(1, sizeof(mainwindow)));
  init_random();

#ifdef ENABLE_DIAGNOSTICS
  run_weed_startup_tests();
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

  init_colour_engine();

  weed_threadsafe = FALSE;
  test_plant = weed_plant_new(0);
  if (weed_leaf_set_private_data(test_plant, WEED_LEAF_TYPE, NULL) == WEED_ERROR_CONCURRENCY)
    weed_threadsafe = TRUE;
  else weed_threadsafe = FALSE;
  weed_plant_free(test_plant);

  widget_helper_init();

  /* TRANSLATORS: localised name may be used here */
  lives_set_application_name(_("LiVES"));
  widget_opts.title_prefix = lives_strdup_printf("%s-%s: - ", lives_get_application_name(), LiVES_VERSION);

  // init prefs
  prefs = (_prefs *)lives_calloc(1, sizeof(_prefs));
  future_prefs = (_future_prefs *)lives_calloc(1, sizeof(_future_prefs));
  prefs->workdir[0] = '\0';
  future_prefs->workdir[0] = '\0';
  prefs->config_datadir[0] = '\0';
  prefs->configfile[0] = '\0';

  prefs->show_gui = TRUE;
  prefs->show_splash = FALSE;
  prefs->show_playwin = TRUE;
  prefs->show_dev_opts = FALSE;
  prefs->interactive = TRUE;
  prefs->show_disk_quota = FALSE;

  lives_snprintf(prefs->cmd_log, PATH_MAX, LIVES_DEVNULL);

#ifdef HAVE_YUV4MPEG
  prefs->yuvin[0] = '\0';
#endif

  mainw->version_hash = lives_strdup_printf("%d", verhash(LiVES_VERSION));
  mainw->mgeom = NULL;
  mainw->msg[0] = '\0';
  mainw->error = FALSE;
  mainw->is_exiting = FALSE;
  mainw->multitrack = NULL;
  mainw->splash_window = NULL;
  mainw->is_ready = mainw->fatal = FALSE;
  mainw->memok = TRUE;
  mainw->go_away = TRUE;
  mainw->last_dprint_file = mainw->current_file = mainw->playing_file = -1;
  mainw->no_context_update = FALSE;
  mainw->ce_thumbs = FALSE;
  mainw->LiVES = NULL;
  mainw->suppress_dprint = FALSE;
  mainw->clutch = TRUE;
  mainw->mbar_res = 0;
  mainw->max_textsize = N_TEXT_SIZES;
  mainw->no_configs = FALSE;

  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

  mainw->has_session_workdir = FALSE;
  mainw->old_vhash = NULL;

  mainw->prefs_cache = mainw->hdrs_cache = mainw->gen_cache = NULL;

  // mainw->foreign is set if we are grabbing an external window
  mainw->foreign = FALSE;

  mainw->debug = FALSE;

  mainw->sense_state = LIVES_SENSE_STATE_INSENSITIZED;

  prefs->max_modes_per_key = atoi(DEF_FX_KEYMODES);

  devmap[0] = '\0';
  capable->mountpoint = NULL;
  capable->startup_msg[0] = '\0';
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

  ensure_isdir(capable->home_dir);
  get_location("touch", capable->touch_cmd, PATH_MAX); // needed for make_writeable_dir()
  get_location("rm", capable->rm_cmd, PATH_MAX); // ditto
  get_location("rmdir", capable->rmdir_cmd, PATH_MAX); // ditto
  get_location("sed", capable->sed_cmd, PATH_MAX); // nice to know
  get_location("grep", capable->grep_cmd, PATH_MAX); // ditto
  get_location("wc", capable->wc_cmd, PATH_MAX); // ditto

  // get opts first
  if (argc > 1) {
    if (!strcmp(argv[1], "-capture")) {
      // special mode for grabbing external window
      mainw->foreign = TRUE;
    } else if (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help")) {
      char string[256];
      get_location(EXEC_PLAY, string, 256);
      if (*string) capable->has_sox_play = TRUE;

      capable->myname_full = lives_find_program_in_path(argv[0]);

      if ((mynsize = lives_readlink(capable->myname_full, cdir, PATH_MAX)) != -1) {
        lives_memset(cdir + mynsize, 0, 1);
        lives_free(capable->myname_full);
        capable->myname_full = lives_strdup(cdir);
      }

      lives_snprintf(cdir, PATH_MAX, "%s", capable->myname_full);
      get_basename(cdir);
      capable->myname = lives_strdup(cdir);

      print_opthelp();
      exit(0);
    } else if (!strcmp(argv[1], "-version") || !strcmp(argv[1], "--version")) {
      print_notice();
      exit(0);
    } else {
      struct option longopts[] = {
        {"aplayer", 1, 0, 0},
        {"asource", 1, 0, 0},
        {"workdir", 1, 0, 0},
        {"configfile", 1, 0, 0},
        {"configdatadir", 1, 0, 0},
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
        c = getopt_long_only(argc, argv, "", longopts, &option_index);
        if (c == -1) break; // end of options
        charopt = longopts[option_index].name;
        if (c == '?') {
          msg = lives_strdup_printf(_("Invalid option %s on commandline\n"), argv[count]);
          LIVES_FATAL(msg);
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

        if (!strcmp(charopt, "norecover")) {
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
          prefs->max_modes_per_key = atoi(optarg);
          if (prefs->max_modes_per_key < 1) prefs->max_modes_per_key = 1;
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
          future_prefs->jack_opts = prefs->jack_opts = atoi(optarg);
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

      if (optind < argc) {
        // remaining opts are filename [start_time] [end_frame]
        char *dir;
        lives_snprintf(start_file, PATH_MAX, "%s", argv[optind++]); // filename
        if (optind < argc) start = lives_strtod(argv[optind++], NULL); // start time (seconds)
        if (optind < argc) end = atoi(argv[optind++]); // number of frames

        if (lives_strrstr(start_file, "://") == NULL) {
          // prepend current directory if needed (unless file contains :// - eg. dvd:// or http://)
          dir = get_dir(start_file);
          get_basename(start_file);
          lives_snprintf(start_file, PATH_MAX, "%s", (tmp = lives_build_filename(dir, start_file, NULL)));
          lives_free(tmp);
          lives_free(dir);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (!ign_opts.ign_configfile) {
    tmp = lives_build_filename(capable->home_dir, LIVES_DEF_CONFIG_DIR, "lives", LIVES_DEF_CONFIG_FILE, NULL);
    lives_snprintf(prefs->configfile, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  if (!ign_opts.ign_config_datadir) {
    tmp = lives_build_path(capable->home_dir, LOCAL_HOME_DIR, LIVES_DEF_CONFIG_DATADIR, NULL);
    lives_snprintf(prefs->config_datadir, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  // get capabilities and if OK set some initial prefs
  theme_error = pre_init();

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

  // need to do this here, before lives_startup but afer setting ign_opts
  mainw->new_vpp = NULL;
  mainw->vpp = NULL;
  lives_memset(future_prefs->vpp_name, 0, 64);
  future_prefs->vpp_argv = NULL;

  if (!ign_opts.ign_vppdefs) {
    tmp = lives_build_filename(prefs->config_datadir, VPP_DEFS_FILE, NULL);
    lives_snprintf(mainw->vpp_defs_file, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }

  lives_idle_add_simple(lives_startup, NULL);

#ifdef GUI_GTK
  if (!gtk_thread) {
    gtk_main();
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
  // needs notify_socket and prefs->omc_events, so seems unlikely it will do anything, but anyway...
  lives_notify(LIVES_OSC_NOTIFY_QUIT, msg);
  lives_free(msg);
  _exit(1);
}


LIVES_GLOBAL_INLINE boolean startup_message_nonfatal(const char *msg) {
  if (mainw && mainw->ds_status == LIVES_STORAGE_STATUS_CRITICAL) do_abort_ok_dialog(msg);
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
  widget_opts.non_modal = TRUE;
  do_error_dialog_with_check(msg, warning_mask);
  widget_opts.non_modal = FALSE;
  return TRUE;
}


///////////////////////////////// GUI section - TODO: move into another file //////////////////////////////////////

void set_main_title(const char *file, int untitled) {
  char *title, *tmp;
  char short_file[256];

  if (file && CURRENT_CLIP_IS_VALID) {
    if (untitled) {
      title = lives_strdup_printf(_("<%s> %dx%d : %d frames %d bpp %.3f fps"), (tmp = get_untitled_name(untitled)),
                                  cfile->hsize, cfile->vsize, cfile->frames, cfile->bpp, cfile->fps);
      lives_free(tmp);
    } else {
      lives_snprintf(short_file, 256, "%s", file);
      if (cfile->restoring || (cfile->opening && cfile->frames == 123456789)) {
        title = lives_strdup_printf(_("<%s> %dx%d : ??? frames ??? bpp %.3f fps"),
                                    (tmp = lives_path_get_basename(file)), cfile->hsize, cfile->vsize, cfile->fps);
      } else {
        title = lives_strdup_printf(_("<%s> %dx%d : %d frames %d bpp %.3f fps"),
                                    cfile->clip_type != CLIP_TYPE_VIDEODEV ? (tmp = lives_path_get_basename(file))
                                    : (tmp = lives_strdup(file)), cfile->hsize, cfile->vsize, cfile->frames, cfile->bpp, cfile->fps);
      }
      lives_free(tmp);
    }
  } else {
    title = (_("<No File>"));
  }

  lives_window_set_title(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), title);
  lives_free(title);

  if (!LIVES_IS_PLAYING && mainw->play_window) play_window_set_title();
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
                             && capable->has_encoder_plugins);
#ifdef LIBAV_TRANSCODE
  lives_widget_set_sensitive(mainw->transcode, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
#endif
  if (!prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->backup, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
    lives_widget_set_sensitive(mainw->save_selection, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO &&
                               capable->has_encoder_plugins);
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
    lives_widget_set_sensitive(mainw->xdelete, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
  }
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
  lives_widget_set_sensitive(mainw->firewire, TRUE);
  lives_widget_set_sensitive(mainw->tvdev, TRUE);

  if (!prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->export_proj, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
    lives_widget_set_sensitive(mainw->import_proj, TRUE);
  }

  if (is_realtime_aplayer(prefs->audio_player) && prefs->audio_player != AUD_PLAYER_NONE) {
    lives_widget_set_sensitive(mainw->int_audio_checkbutton, prefs->audio_src != AUDIO_SRC_INT);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, prefs->audio_src != AUDIO_SRC_EXT);
  }

  if (!prefs->vj_mode) {
    if (RFX_LOADED) {
      if (!mainw->foreign) {
        if (mainw->rendered_fx) {
          for (i = 1; i <= mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom +
               mainw->num_rendered_effects_test; i++)
            if (mainw->rendered_fx[i].menuitem && mainw->rendered_fx[i].min_frames >= 0)
              lives_widget_set_sensitive(mainw->rendered_fx[i].menuitem, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
          if (mainw->rendered_fx[0].menuitem && LIVES_IS_WIDGET(mainw->rendered_fx[0].menuitem)) {
            if (!CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID
                && ((has_video_filters(FALSE) && !has_video_filters(TRUE)) ||
                    (cfile->achans > 0 && prefs->audio_src == AUDIO_SRC_INT
                     && has_audio_filters(AF_TYPE_ANY))
                    || mainw->agen_key != 0)) {
              lives_widget_set_sensitive(mainw->rendered_fx[0].menuitem, TRUE);
            } else lives_widget_set_sensitive(mainw->rendered_fx[0].menuitem, FALSE);
	    // *INDENT-OFF*
          }}}
      // *INDENT-ON*

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
    }

    if (mainw->resize_menuitem) {
      lives_widget_set_sensitive(mainw->resize_menuitem, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_HAS_VIDEO);
    }
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
  lives_widget_set_sensitive(mainw->load_audio, TRUE);
  lives_widget_set_sensitive(mainw->load_subs, CURRENT_CLIP_IS_VALID && !CURRENT_CLIP_IS_CLIPBOARD);
  lives_widget_set_sensitive(mainw->erase_subs, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID && cfile->subt != NULL);
  if (capable->has_cdda2wav && *prefs->cdplay_device) lives_widget_set_sensitive(mainw->load_cdtrack, TRUE);
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
  lives_widget_set_sensitive(mainw->vj_load_set, !mainw->was_set);
  lives_widget_set_sensitive(mainw->vj_reset, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID);
  lives_widget_set_sensitive(mainw->vj_realize, !CURRENT_CLIP_IS_CLIPBOARD && CURRENT_CLIP_IS_VALID &&
                             cfile->frame_index != NULL);
  lives_widget_set_sensitive(mainw->midi_learn, TRUE);
  lives_widget_set_sensitive(mainw->midi_save, has_devicemap(-1));
  lives_widget_set_sensitive(mainw->toy_tv, TRUE);
  lives_widget_set_sensitive(mainw->autolives, TRUE);
  lives_widget_set_sensitive(mainw->toy_random_frames, TRUE);
  //lives_widget_set_sensitive(mainw->open_lives2lives, TRUE);
  if (!prefs->vj_mode)
    lives_widget_set_sensitive(mainw->gens_submenu, TRUE);
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
    lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 1, cfile->frames);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end);
    lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);

    lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 1, cfile->frames);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start);
    lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);

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

  lives_widget_set_sensitive(mainw->firewire, FALSE);
  lives_widget_set_sensitive(mainw->tvdev, FALSE);

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
  if (!prefs->pause_during_pb) {
    lives_widget_set_sensitive(mainw->playall, FALSE);
  }
  lives_widget_set_sensitive(mainw->rewind, FALSE);
  if (!prefs->vj_mode) {
    if (RFX_LOADED) {
      if (!mainw->foreign) {
        for (i = 0; i <= mainw->num_rendered_effects_builtin + mainw->num_rendered_effects_custom +
             mainw->num_rendered_effects_test; i++)
          if (mainw->rendered_fx[i].menuitem && mainw->rendered_fx[i].menuitem &&
              mainw->rendered_fx[i].min_frames >= 0)
            lives_widget_set_sensitive(mainw->rendered_fx[i].menuitem, FALSE);
      }
    }
  }

  if (mainw->resize_menuitem) {
    lives_widget_set_sensitive(mainw->resize_menuitem, FALSE);
  }

  if (!prefs->vj_mode)
    if (mainw->num_rendered_effects_test > 0) {
      lives_widget_set_sensitive(mainw->run_test_rfx_submenu, FALSE);
    }


  if (mainw->has_custom_gens) {
    lives_widget_set_sensitive(mainw->custom_gens_submenu, FALSE);
  }

  if (mainw->has_custom_tools) {
    lives_widget_set_sensitive(mainw->custom_tools_submenu, FALSE);
  }

  if (!mainw->foreign)
    if (mainw->has_custom_effects) {
      lives_widget_set_sensitive(mainw->custom_effects_submenu, FALSE);
    }

  lives_widget_set_sensitive(mainw->export_submenu, FALSE);
  lives_widget_set_sensitive(mainw->recaudio_submenu, FALSE);
  lives_widget_set_sensitive(mainw->append_audio, FALSE);
  lives_widget_set_sensitive(mainw->trim_submenu, FALSE);
  lives_widget_set_sensitive(mainw->delaudio_submenu, FALSE);
  lives_widget_set_sensitive(mainw->gens_submenu, FALSE);
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
  lives_widget_set_sensitive(mainw->toy_tv, FALSE);
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
  lives_widget_set_sensitive(mainw->toy_tv, FALSE);
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
  LiVESXWindow *xwin;
  lives_rect_t update_rect;
  lives_painter_t *cr;
  int cx, cy;
  int rwidth, rheight, width, height, owidth, oheight;
  int border = 0;

  if (!surface || !widget) return;

  xwin = lives_widget_get_xwindow(widget);
  if (!LIVES_IS_XWINDOW(xwin)) return;

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

    if (widget == mainw->start_image || widget == mainw->end_image) {// || (widget == mainw->play_window && !mainw->fs)) {
      LiVESWidget *p = lives_widget_get_parent(widget);
      if (prefs->ce_maxspect) {
        calc_maxspect(rwidth, rheight, &width, &height);

        width = (width >> 1) << 1;
        height = (height >> 1) << 1;

        if (width > owidth && height > oheight) {
          width = owidth;
          height = oheight;
        }
      }
      rwidth = lives_widget_get_allocation_width(p);
      rheight = lives_widget_get_allocation_height(p);
      lives_painter_render_background(p, cr, 0., 0., rwidth, rheight);
    }

    lives_widget_set_opacity(widget, 1.);
    cx = (rwidth - width) >> 1;
    if (cx < 0) cx = 0;
    cy = (rheight - height) >> 1;

    if ((!mainw->multitrack || widget != mainw->play_image) && widget != mainw->preview_image) {
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
      }
    }

    /// x, y values are offset of top / left of image in drawing area
    lives_painter_set_source_pixbuf(cr, pixbuf, cx, cy);

    if (mainw->multitrack && widget != mainw->preview_image) border = 16;
    /// clipping area for image
    lives_painter_rectangle(cr, cx, cy, rwidth - border, rheight);
  } else {
    lives_widget_set_opacity(widget, 0.);
    clear_widget_bg(widget, surface);
  }
  lives_painter_fill(cr);
  lives_painter_destroy(cr);

  if (widget == mainw->play_window && rheight > mainw->pheight) rheight = mainw->pheight;

  update_rect.x = update_rect.y = 0;
  update_rect.width = rwidth;
  update_rect.height = rheight;

  if (!LIVES_IS_XWINDOW(xwin)) return;
  lives_xwindow_invalidate_rect(lives_widget_get_xwindow(widget), &update_rect, FALSE);
}


void showclipimgs(void) {
  if (CURRENT_CLIP_IS_VALID) {
    load_end_image(cfile->end);
    load_start_image(cfile->start);
  } else {
    load_end_image(0);
    load_start_image(0);
  }
  lives_widget_queue_draw(mainw->start_image);
  lives_widget_queue_draw(mainw->end_image);
  lives_widget_context_update();
}


void load_start_image(int frame) {
  LiVESPixbuf *start_pixbuf = NULL;
  LiVESPixbuf *orig_pixbuf = NULL;
  weed_layer_t *layer = NULL;
  weed_timecode_t tc;
  LiVESInterpType interp;
  char *xmd5sum = NULL;
  char *fname = NULL;
  boolean expose = FALSE;
  boolean cache_it = TRUE;
  int rwidth, rheight, width, height;
  int tries = 2;
  frames_t xpf;

  if (!prefs->show_gui) return;
  if (mainw->multitrack) return;

  if (LIVES_IS_PLAYING && mainw->fs && (!mainw->sep_win || ((prefs->gui_monitor == prefs->play_monitor ||
                                        capable->nmonitors == 1) &&
                                        (!mainw->ext_playback ||
                                         (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))))) return;
  if (frame < 0) {
    frame = -frame;
    expose = TRUE;
  }

  lives_widget_set_opacity(mainw->start_image, 1.);

  if (!CURRENT_CLIP_IS_NORMAL || frame < 1 || frame > cfile->frames) {
    int bx, by, hsize, vsize;
    int scr_width = GUI_SCREEN_WIDTH;
    int scr_height = GUI_SCREEN_HEIGHT;
    int vspace = get_vspace();
    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    if (by > MENU_HIDE_LIM)
      lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE + vspace + by + mainw->mbar_res);
    if (LIVES_IS_PLAYING && mainw->double_size) {
      hsize /= 2;
      vsize /= 2;
    }
    lives_widget_set_size_request(mainw->start_image, hsize, vsize);
    lives_widget_set_size_request(mainw->frame1, hsize, vsize);
    lives_widget_set_size_request(mainw->eventbox3, hsize, vsize);

    lives_widget_set_hexpand(mainw->frame1, FALSE);
    lives_widget_set_vexpand(mainw->frame1, FALSE);
  }

  if (CURRENT_CLIP_IS_VALID && (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV)) {
    if (!mainw->camframe) {
      LiVESError *error = NULL;
      char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
      char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
      mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
      if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
      lives_free(tmp); lives_free(fname);
    }
    set_drawing_area_from_pixbuf(mainw->start_image, mainw->camframe, mainw->si_surface);
    return;
  }

  if (!CURRENT_CLIP_IS_NORMAL || mainw->current_file == mainw->scrap_file || frame < 1 || frame > cfile->frames) {
    if (mainw->imframe) {
      set_drawing_area_from_pixbuf(mainw->start_image, mainw->imframe, mainw->si_surface);
    } else {
      set_drawing_area_from_pixbuf(mainw->start_image, NULL, mainw->si_surface);
    }
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->start_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->start_image, 0.4);
    }
    return;
  }

  xpf = ABS(get_indexed_frame(mainw->current_file, frame));

check_stcache:
  if (mainw->st_fcache) {
    if (lives_layer_get_clip(mainw->st_fcache) == mainw->current_file
        && lives_layer_get_frame(mainw->st_fcache) == xpf) {
      if (is_virtual_frame(mainw->current_file, frame)) layer = mainw->st_fcache;
      else {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          char *md5sum = weed_get_string_value(mainw->st_fcache, WEED_LEAF_MD5SUM, NULL);
          if (md5sum) {
            if (!fname) fname = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
            if (!xmd5sum) xmd5sum = get_md5sum(fname);
            if (!lives_strcmp(md5sum, xmd5sum)) layer = mainw->st_fcache;
            lives_free(md5sum);
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  if (!layer) {
    if (mainw->st_fcache) {
      if (mainw->st_fcache != mainw->en_fcache && mainw->st_fcache != mainw->pr_fcache)
        weed_layer_free(mainw->st_fcache);
      mainw->st_fcache = NULL;
    }
    if (tries--) {
      if (tries == 1) mainw->st_fcache = mainw->pr_fcache;
      else mainw->st_fcache = mainw->en_fcache;
      goto check_stcache;
    }
  }
  lives_freep((void **)&fname);

  tc = ((frame - 1.)) / cfile->fps * TICKS_PER_SECOND;

  if (!prefs->ce_maxspect) {
    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &start_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !start_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, cfile->hsize, cfile->vsize,
                             WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, cfile->hsize, cfile->vsize, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          if (xmd5sum) lives_free(xmd5sum);
          return;
        }
      }
    }

    if (!start_pixbuf) start_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

    if (LIVES_IS_PIXBUF(start_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->start_image, start_pixbuf, mainw->si_surface);
      if (!cache_it || weed_layer_get_pixel_data_packed(layer) || !(pixbuf_to_layer(layer, start_pixbuf)))
        lives_widget_object_unref(start_pixbuf);
    } else cache_it = FALSE;

    if (!cache_it) {
      if (layer) weed_layer_free(layer);
      mainw->st_fcache = NULL;
    } else {
      if (!mainw->st_fcache) {
        mainw->st_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            if (!xmd5sum) {
              char *fname = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
              xmd5sum = get_md5sum(fname);
              lives_free(fname);
            }
            weed_set_string_value(layer, WEED_LEAF_MD5SUM, xmd5sum);
	    // *INDENT-OFF*
	  }}
        lives_layer_set_frame(layer, xpf);
	lives_layer_set_clip(layer, mainw->current_file);
      }}
    // *INDENT-ON*
    if (xmd5sum) lives_free(xmd5sum);
    return;
  }

  do {
    width = cfile->hsize;
    height = cfile->vsize;

    // TODO *** - if width*height==0, show broken frame image

#if GTK_CHECK_VERSION(3, 0, 0)
    rwidth = mainw->ce_frame_width;
    rheight = mainw->ce_frame_height - V_RESIZE_ADJUST * 2;
#else
    rwidth = lives_widget_get_allocation_width(mainw->start_image);
    rheight = lives_widget_get_allocation_height(mainw->start_image);
#endif
    calc_maxspect(rwidth, rheight, &width, &height);
    width = (width >> 2) << 2;
    height = (height >> 2) << 2;

    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        // TODO: make background thread
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &start_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !start_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height, WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          weed_layer_free(layer);
          if (xmd5sum) lives_free(xmd5sum);
          return;
        }
      }
    }

    if (!start_pixbuf || lives_pixbuf_get_width(start_pixbuf) != width || lives_pixbuf_get_height(start_pixbuf) != height) {
      if (!orig_pixbuf) {
        if (layer) orig_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        else orig_pixbuf = start_pixbuf;
      }
      if (start_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(start_pixbuf)) {
        start_pixbuf = NULL;
      }
      if (LIVES_IS_PIXBUF(orig_pixbuf)) {
        if (lives_pixbuf_get_width(orig_pixbuf) == width && lives_pixbuf_get_height(orig_pixbuf) == height)
          start_pixbuf = orig_pixbuf;
        else {
          start_pixbuf = lives_pixbuf_scale_simple(orig_pixbuf, width, height, LIVES_INTERP_BEST);
        }
      }
    }

    if (LIVES_IS_PIXBUF(start_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->start_image, start_pixbuf, mainw->si_surface);
    }

#if !GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_queue_resize(mainw->start_image);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  } while (rwidth != lives_widget_get_allocation_width(mainw->start_image) ||
           rheight != lives_widget_get_allocation_height(mainw->start_image));
#else
  }
  while (FALSE);
#endif
  if (start_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(start_pixbuf)) {
    lives_widget_object_unref(start_pixbuf);
  }

  if (LIVES_IS_PIXBUF(orig_pixbuf)) {
    if (!cache_it || weed_layer_get_pixel_data_packed(layer) || !(pixbuf_to_layer(layer, orig_pixbuf)))
      lives_widget_object_unref(orig_pixbuf);
  } else cache_it = FALSE;

  if (!cache_it) {
    weed_layer_free(layer);
    mainw->st_fcache = NULL;
  } else {
    if (!mainw->st_fcache) {
      mainw->st_fcache = layer;
      if (!is_virtual_frame(mainw->current_file, frame)) {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          if (!xmd5sum) {
            char *fname = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
            xmd5sum = get_md5sum(fname);
            lives_free(fname);
          }
          weed_set_string_value(layer, WEED_LEAF_MD5SUM, xmd5sum);
	// *INDENT-OFF*
      }}
    lives_layer_set_frame(layer, xpf);
    lives_layer_set_clip(layer, mainw->current_file);
  }}
// *INDENT-ON*
  if (xmd5sum) lives_free(xmd5sum);
}


void load_end_image(int frame) {
  LiVESPixbuf *end_pixbuf = NULL;
  LiVESPixbuf *orig_pixbuf = NULL;
  weed_layer_t *layer = NULL;
  weed_timecode_t tc;
  LiVESInterpType interp;
  char *xmd5sum = NULL;
  char *fname = NULL;
  boolean expose = FALSE;
  boolean cache_it = TRUE;
  int rwidth, rheight, width, height;
  int tries = 2;
  frames_t xpf;

  if (!prefs->show_gui) return;
  if (mainw->multitrack) return;

  if (LIVES_IS_PLAYING && mainw->fs && (!mainw->sep_win || ((prefs->gui_monitor == prefs->play_monitor ||
                                        capable->nmonitors == 1) &&
                                        (!mainw->ext_playback ||
                                         (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))))) return;
  if (frame < 0) {
    frame = -frame;
    expose = TRUE;
  }

  lives_widget_set_opacity(mainw->end_image, 1.);

  if (!CURRENT_CLIP_IS_NORMAL || frame < 1 || frame > cfile->frames) {
    int bx, by, hsize, vsize;
    int scr_width = GUI_SCREEN_WIDTH;
    int scr_height = GUI_SCREEN_HEIGHT;
    int vspace = get_vspace();
    get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
    if (by > MENU_HIDE_LIM)
      lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE + vspace + by + mainw->mbar_res);
    if (LIVES_IS_PLAYING && mainw->double_size) {
      hsize /= 2;
      vsize /= 2;
    }
    lives_widget_set_size_request(mainw->end_image, hsize, vsize);
    lives_widget_set_size_request(mainw->frame2, hsize, vsize);
    lives_widget_set_size_request(mainw->eventbox4, hsize, vsize);

    lives_widget_set_hexpand(mainw->frame2, FALSE);
    lives_widget_set_vexpand(mainw->frame2, FALSE);
  }

  if (CURRENT_CLIP_IS_VALID && (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV)) {
    if (!mainw->camframe) {
      LiVESError *error = NULL;
      char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
      char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
      mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
      if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
      lives_free(tmp); lives_free(fname);
    }

    set_drawing_area_from_pixbuf(mainw->end_image, mainw->camframe, mainw->ei_surface);
    return;
  }

  if (!CURRENT_CLIP_IS_NORMAL || mainw->current_file == mainw->scrap_file || frame < 1 || frame > cfile->frames) {
    if (mainw->imframe) {
      set_drawing_area_from_pixbuf(mainw->end_image, mainw->imframe, mainw->ei_surface);
    } else {
      set_drawing_area_from_pixbuf(mainw->end_image, NULL, mainw->ei_surface);
    }
    if (!palette || !(palette->style & STYLE_LIGHT)) {
      lives_widget_set_opacity(mainw->end_image, 0.8);
    } else {
      lives_widget_set_opacity(mainw->end_image, 0.4);
    }
    return;
  }

  xpf = ABS(get_indexed_frame(mainw->current_file, frame));
check_encache:
  if (mainw->en_fcache) {
    if (lives_layer_get_clip(mainw->en_fcache) == mainw->current_file
        && lives_layer_get_frame(mainw->en_fcache) == xpf) {
      if (is_virtual_frame(mainw->current_file, frame)) layer = mainw->en_fcache;
      else {
        if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
          char *md5sum = weed_get_string_value(mainw->en_fcache, WEED_LEAF_MD5SUM, NULL);
          if (md5sum) {
            if (!fname) fname = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
            if (!xmd5sum) xmd5sum = get_md5sum(fname);
            if (!lives_strcmp(md5sum, xmd5sum)) layer = mainw->en_fcache;
            lives_free(md5sum);
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  if (!layer) {
    if (mainw->en_fcache) {
      if (mainw->en_fcache != mainw->st_fcache && mainw->en_fcache != mainw->pr_fcache)
        weed_layer_free(mainw->en_fcache);
      mainw->en_fcache = NULL;
    }
    if (tries--) {
      if (tries == 1) mainw->en_fcache = mainw->pr_fcache;
      else mainw->en_fcache = mainw->st_fcache;
      goto check_encache;
    }
  }

  lives_freep((void **)&fname);

  tc = (frame - 1.) / cfile->fps * TICKS_PER_SECOND;

  if (!prefs->ce_maxspect) {
    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &end_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !end_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, cfile->hsize, cfile->vsize,
                             WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, cfile->hsize, cfile->vsize, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          if (xmd5sum) lives_free(xmd5sum);
          return;
        }
      }
    }

    if (!end_pixbuf) end_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);

    if (LIVES_IS_PIXBUF(end_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->end_image, end_pixbuf, mainw->ei_surface);
      if (!cache_it || weed_layer_get_pixel_data_packed(layer) || !(pixbuf_to_layer(layer, end_pixbuf)))
        lives_widget_object_unref(end_pixbuf);
    } else cache_it = FALSE;

    if (!cache_it) {
      if (layer) weed_layer_free(layer);
      mainw->en_fcache = NULL;
    } else {
      if (!mainw->en_fcache) {
        mainw->en_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            if (!xmd5sum) {
              char *fname = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
              xmd5sum = get_md5sum(fname);
              lives_free(fname);
            }
            weed_set_string_value(layer, WEED_LEAF_MD5SUM, xmd5sum);
	    // *INDENT-OFF*
	  }}
	lives_layer_set_frame(layer, xpf);
	lives_layer_set_clip(layer, mainw->current_file);
      }}
    // *INDENT-ON*
    return;
  }

  do {
    width = cfile->hsize;
    height = cfile->vsize;

#if GTK_CHECK_VERSION(3, 0, 0)
    rwidth = mainw->ce_frame_width;
    rheight = mainw->ce_frame_height - V_RESIZE_ADJUST * 2;
#else
    rwidth = lives_widget_get_allocation_width(mainw->end_image);
    rheight = lives_widget_get_allocation_height(mainw->end_image);
#endif
    calc_maxspect(rwidth, rheight, &width, &height);
    width = (width >> 2) << 2;
    height = (height >> 2) << 2;

    // if we are not playing, and it would be slow to seek to the frame, convert it to an image
    if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->current_file, frame) &&
        cfile->ext_src) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
      if (cdata && (expose || !(cdata->seek_flag & LIVES_SEEK_FAST))) {
        virtual_to_images(mainw->current_file, frame, frame, FALSE, &end_pixbuf);
        cache_it = FALSE;
      }
    }

    if (!layer && !end_pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, frame);
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height, WEED_PALETTE_RGB24)) {
        check_layer_ready(layer);
        interp = get_interp_value(prefs->pb_quality, TRUE);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
            !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
          weed_layer_free(layer);
          if (xmd5sum) lives_free(xmd5sum);
          return;
        }
      }
    }

    if (!end_pixbuf || lives_pixbuf_get_width(end_pixbuf) != width || lives_pixbuf_get_height(end_pixbuf) != height) {
      if (!orig_pixbuf) {
        if (layer) orig_pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        else orig_pixbuf = end_pixbuf;
      }
      if (end_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(end_pixbuf)) {
        lives_widget_object_unref(end_pixbuf);
        end_pixbuf = NULL;
      }
      if (LIVES_IS_PIXBUF(orig_pixbuf)) {
        if (lives_pixbuf_get_width(orig_pixbuf) == width && lives_pixbuf_get_height(orig_pixbuf) == height)
          end_pixbuf = orig_pixbuf;
        else {
          end_pixbuf = lives_pixbuf_scale_simple(orig_pixbuf, width, height, LIVES_INTERP_BEST);
        }
      }
    }

    if (LIVES_IS_PIXBUF(end_pixbuf)) {
      set_drawing_area_from_pixbuf(mainw->end_image, end_pixbuf, mainw->ei_surface);
    }

#if !GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_queue_resize(mainw->end_image);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  } while (rwidth != lives_widget_get_allocation_width(mainw->end_image) ||
           rheight != lives_widget_get_allocation_height(mainw->end_image));
#if 0
  {
#endif
#else
  } while (FALSE);
#endif

    if (end_pixbuf != orig_pixbuf && LIVES_IS_PIXBUF(end_pixbuf)) {
      lives_widget_object_unref(end_pixbuf);
    }

    if (LIVES_IS_PIXBUF(orig_pixbuf)) {
      if (!cache_it || weed_layer_get_pixel_data_packed(layer) || !(pixbuf_to_layer(layer, orig_pixbuf)))
        lives_widget_object_unref(orig_pixbuf);
    } else cache_it = FALSE;

    if (!cache_it) {
      weed_layer_free(layer);
      mainw->en_fcache = NULL;
    } else {
      if (!mainw->en_fcache) {
        mainw->en_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            if (!xmd5sum) {
              char *fname = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
              xmd5sum = get_md5sum(fname);
              lives_free(fname);
            }
            weed_set_string_value(layer, WEED_LEAF_MD5SUM, xmd5sum);
	  // *INDENT-OFF*
	}}
      lives_layer_set_frame(layer, xpf);
      lives_layer_set_clip(layer, mainw->current_file);
    }}
  // *INDENT-ON*
    if (xmd5sum) lives_free(xmd5sum);
  }


#ifndef IS_LIBLIVES
  int main(int argc, char *argv[]) {
    // call any hooks here
    return real_main(argc, argv, NULL, 0l);
  }
#endif


  void load_preview_image(boolean update_always) {
    // this is for the sepwin preview
    // update_always==TRUE = update widgets from mainw->preview_frame
    LiVESPixbuf *pixbuf = NULL;
    weed_layer_t *layer = NULL;
    boolean cache_it = TRUE;
    char *xmd5sum = NULL;
    char *fname = NULL;
    frames_t xpf = -1;
    int tries = 2;
    int preview_frame;

    if (!prefs->show_gui) return;
    if (LIVES_IS_PLAYING) return;

    lives_widget_set_opacity(mainw->preview_image, 1.);

    if (CURRENT_CLIP_IS_VALID && (cfile->clip_type == CLIP_TYPE_YUV4MPEG || cfile->clip_type == CLIP_TYPE_VIDEODEV)) {
      if (!mainw->camframe) {
        LiVESError *error = NULL;
        char *fname = lives_strdup_printf("%s.%s", THEME_FRAME_IMG_LITERAL, LIVES_FILE_EXT_JPG);
        char *tmp = lives_build_filename(prefs->prefix_dir, THEME_DIR, LIVES_THEME_CAMERA, fname, NULL);
        mainw->camframe = lives_pixbuf_new_from_file(tmp, &error);
        if (mainw->camframe) lives_pixbuf_saturate_and_pixelate(mainw->camframe, mainw->camframe, 0.0, FALSE);
        lives_free(tmp); lives_free(fname);
        fname = NULL;
      }
      pixbuf = lives_pixbuf_scale_simple(mainw->camframe, mainw->pwidth, mainw->pheight, LIVES_INTERP_BEST);
      set_drawing_area_from_pixbuf(mainw->preview_image, pixbuf, mainw->pi_surface);
      if (pixbuf) lives_widget_object_unref(pixbuf);
      mainw->preview_frame = 1;
      lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 1, 1);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 1);
      lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_widget_set_size_request(mainw->preview_image, mainw->pwidth, mainw->pheight);
      return;
    }

    if (!CURRENT_CLIP_IS_NORMAL || !CURRENT_CLIP_HAS_VIDEO) {
      mainw->preview_frame = 0;
      lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 0, 0);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 0);
      lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);
      if (mainw->imframe) {
        lives_widget_set_size_request(mainw->preview_image, lives_pixbuf_get_width(mainw->imframe),
                                      lives_pixbuf_get_height(mainw->imframe));
        set_drawing_area_from_pixbuf(mainw->preview_image, mainw->imframe, mainw->pi_surface);
      } else set_drawing_area_from_pixbuf(mainw->preview_image, NULL, mainw->pi_surface);
      if (!palette || !(palette->style & STYLE_LIGHT)) {
        lives_widget_set_opacity(mainw->preview_image, 0.8);
      } else {
        lives_widget_set_opacity(mainw->preview_image, 0.4);
      }
      return;
    }

    if (!update_always) {
      // set current frame from spins, set range
      // set mainw->preview_frame to 0 before calling to force an update (e.g after a clip switch)
      switch (mainw->prv_link) {
      case PRV_END:
        preview_frame = cfile->end;
        break;
      case PRV_PTR:
        preview_frame = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
        break;
      case PRV_START:
        preview_frame = cfile->start;
        break;
      default:
        preview_frame = mainw->preview_frame > 0 ? mainw->preview_frame : 1;
        if (preview_frame > cfile->frames) preview_frame = cfile->frames;
        break;
      }

      lives_signal_handler_block(mainw->preview_spinbutton, mainw->preview_spin_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), 1, cfile->frames);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->preview_spinbutton), preview_frame);
      lives_signal_handler_unblock(mainw->preview_spinbutton, mainw->preview_spin_func);

      mainw->preview_frame = preview_frame;
    }

    if (mainw->preview_frame < 1 || mainw->preview_frame > cfile->frames) {
      pixbuf = lives_pixbuf_scale_simple(mainw->imframe, cfile->hsize, cfile->vsize, LIVES_INTERP_BEST);
      if (!palette || !(palette->style & STYLE_LIGHT)) {
        lives_widget_set_opacity(mainw->preview_image, 0.4);
      } else {
        lives_widget_set_opacity(mainw->preview_image, 0.8);
      }
    } else {
      weed_timecode_t tc;
      xpf = ABS(get_indexed_frame(mainw->current_file, mainw->preview_frame));
check_prcache:
      if (mainw->pr_fcache) {
        if (lives_layer_get_clip(mainw->pr_fcache) == mainw->current_file
            && lives_layer_get_frame(mainw->pr_fcache) == xpf) {
          if (is_virtual_frame(mainw->current_file, mainw->preview_frame)) layer = mainw->pr_fcache;
          else {
            if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
              char *md5sum = weed_get_string_value(mainw->pr_fcache, WEED_LEAF_MD5SUM, NULL);
              if (md5sum) {
                if (!fname) fname = make_image_file_name(cfile, mainw->preview_frame, get_image_ext_for_type(cfile->img_type));
                if (!xmd5sum) xmd5sum = get_md5sum(fname);
                if (!lives_strcmp(md5sum, xmd5sum)) layer = mainw->pr_fcache;
                lives_free(md5sum);
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*
      if (!layer) {
        if (mainw->pr_fcache) {
          if (mainw->pr_fcache != mainw->st_fcache && mainw->pr_fcache != mainw->en_fcache)
            weed_layer_free(mainw->pr_fcache);
          mainw->pr_fcache = NULL;
        }
        if (tries--) {
          if (tries == 1) mainw->pr_fcache = mainw->st_fcache;
          else mainw->pr_fcache = mainw->en_fcache;
          goto check_prcache;
        }
      }
      lives_freep((void **)&fname);

      // if we are not playing, and it would be slow to seek to the frame, convert it to an image
      if (!LIVES_IS_PLAYING && !layer && cfile->clip_type == CLIP_TYPE_FILE &&
          is_virtual_frame(mainw->current_file, mainw->preview_frame) &&
          cfile->ext_src) {
        lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
        if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
          virtual_to_images(mainw->current_file, mainw->preview_frame, mainw->preview_frame, FALSE, &pixbuf);
          cache_it = FALSE;
        }
      }

      if (!layer && !pixbuf) {
        layer = lives_layer_new_for_frame(mainw->current_file, mainw->preview_frame);
        tc = ((mainw->preview_frame - 1.)) / cfile->fps * TICKS_PER_SECOND;
        if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, mainw->pwidth, mainw->pheight,
                               WEED_PALETTE_RGB24)) {
          LiVESInterpType interp = get_interp_value(prefs->pb_quality, TRUE);
          check_layer_ready(layer);
          if (!resize_layer(layer, mainw->pwidth, mainw->pheight, interp, WEED_PALETTE_RGB24, 0) ||
              !convert_layer_palette(layer, WEED_PALETTE_RGB24, 0)) {
            weed_layer_free(layer);
            if (xmd5sum) lives_free(xmd5sum);
            return;
          }
        }
      }

      if (!pixbuf || lives_pixbuf_get_width(pixbuf) != mainw->pwidth
          || lives_pixbuf_get_height(pixbuf) != mainw->pheight) {
        if (!pixbuf) {
          if (layer) pixbuf = layer_to_pixbuf(layer, TRUE, TRUE);
        }
      }

      if (LIVES_IS_PIXBUF(pixbuf)) {
        LiVESPixbuf *pr_pixbuf = NULL;
        if (lives_pixbuf_get_width(pixbuf) == mainw->pwidth && lives_pixbuf_get_height(pixbuf) == mainw->pheight)
          pr_pixbuf = pixbuf;
        else {
          pr_pixbuf = lives_pixbuf_scale_simple(pixbuf, mainw->pwidth, mainw->pheight, LIVES_INTERP_BEST);
        }
        if (LIVES_IS_PIXBUF(pr_pixbuf)) {
          lives_widget_set_size_request(mainw->preview_image, MAX(mainw->pwidth, mainw->sepwin_minwidth), mainw->pheight);
          set_drawing_area_from_pixbuf(mainw->preview_image, pr_pixbuf, mainw->pi_surface);
          if (pr_pixbuf != pixbuf) lives_widget_object_unref(pr_pixbuf);
          if (!layer || !cache_it || weed_layer_get_pixel_data_packed(layer) || !(pixbuf_to_layer(layer, pixbuf)))
            lives_widget_object_unref(pixbuf);
        } else cache_it = FALSE;
      } else cache_it = FALSE;
    }

    if (!cache_it) {
      weed_layer_free(layer);
      mainw->pr_fcache = NULL;
    } else {
      if (!mainw->pr_fcache) {
        mainw->pr_fcache = layer;
        if (!is_virtual_frame(mainw->current_file, mainw->preview_frame)) {
          if (cfile->clip_type == CLIP_TYPE_DISK && capable->has_md5sum) {
            if (!xmd5sum) {
              char *fname = make_image_file_name(cfile, mainw->preview_frame, get_image_ext_for_type(cfile->img_type));
              xmd5sum = get_md5sum(fname);
              lives_free(fname);
            }
            weed_set_string_value(layer, WEED_LEAF_MD5SUM, xmd5sum);
	  // *INDENT-OFF*
	}}
      lives_layer_set_frame(layer, xpf);
      lives_layer_set_clip(layer, mainw->current_file);
    }}
  // *INDENT-ON*

    if (update_always) {
      // set spins from current frame
      switch (mainw->prv_link) {
      case PRV_PTR:
        //cf. hrule_reset
        cfile->pointer_time = lives_ce_update_timeline(mainw->preview_frame, 0.);
        if (cfile->frames > 0) cfile->frameno = calc_frame_from_time(mainw->current_file,
                                                  cfile->pointer_time);
        if (cfile->pointer_time > 0.) {
          lives_widget_set_sensitive(mainw->rewind, TRUE);
          lives_widget_set_sensitive(mainw->trim_to_pstart, CURRENT_CLIP_HAS_AUDIO);
          lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
          if (mainw->preview_box) {
            lives_widget_set_sensitive(mainw->p_rewindbutton, TRUE);
          }
        }
        mainw->ptrtime = cfile->pointer_time;
        lives_widget_queue_draw(mainw->eventbox2);
        break;

      case PRV_START:
        if (mainw->st_fcache && mainw->st_fcache != mainw->pr_fcache && mainw->st_fcache != mainw->en_fcache) {
          weed_layer_free(mainw->st_fcache);
        }
        mainw->st_fcache = mainw->pr_fcache;
        if (cfile->start != mainw->preview_frame) {
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), mainw->preview_frame);
          lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_start));
          get_play_times();
        }
        break;

      case PRV_END:
        if (mainw->en_fcache && mainw->en_fcache != mainw->pr_fcache && mainw->en_fcache != mainw->st_fcache) {
          weed_layer_free(mainw->en_fcache);
        }
        mainw->en_fcache = mainw->pr_fcache;
        if (cfile->end != mainw->preview_frame) {
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), mainw->preview_frame);
          lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_end));
          get_play_times();
        }
        break;

      default:
        lives_widget_set_sensitive(mainw->rewind, FALSE);
        lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
        lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
        if (mainw->preview_box) {
          lives_widget_set_sensitive(mainw->p_rewindbutton, FALSE);
        }
        break;
      }
    }
    if (xmd5sum) lives_free(xmd5sum);
  }


#ifndef NO_PROG_LOAD

#ifdef GUI_GTK
  static void pbsize_set(GdkPixbufLoader * pbload, int xxwidth, int xxheight, livespointer ptr) {
    if (xxwidth * xxheight > 0) gdk_pixbuf_loader_set_size(pbload, xxwidth, xxheight);
  }
#endif

#endif


#ifdef USE_LIBPNG
#ifdef PNG_ASSEMBLER_CODE_SUPPORTED
  static png_uint_32 png_flags;
#endif
  static int png_flagstate = 0;

  static void png_init(png_structp png_ptr) {
    png_uint_32 mask = 0;
#if defined(PNG_LIBPNG_VER) && (PNG_LIBPNG_VER >= 10200)
#ifdef PNG_SELECT_READ
    int selection = PNG_SELECT_READ;// | PNG_SELECT_WRITE;
    int mmxsupport = png_mmx_support(); // -1 = not compiled, 0 = not on machine, 1 = OK
    mask = png_get_asm_flagmask(selection);

    if (mmxsupport < 1) {
      int compilerID;
      mask &= ~(png_get_mmx_flagmask(selection, &compilerID));
      /* if (prefs->show_dev_opts) { */
      /*   g_printerr(" without MMX features (%d)\n", mmxsupport); */
      /* } */
    } else {
      /* if (prefs->show_dev_opts) { */
      /*   g_printerr(" with MMX features\n"); */
      /* } */
    }
#endif
#endif

#if defined(PNG_USE_PNGGCCRD) && defined(PNG_ASSEMBLER_CODE_SUPPORTED)	\
  && defined(PNG_THREAD_UNSAFE_OK)
    /* Disable thread-unsafe features of pnggccrd */
    if (png_access_version() >= 10200) {
      mask &= ~(PNG_ASM_FLAG_MMX_READ_COMBINE_ROW
                | PNG_ASM_FLAG_MMX_READ_FILTER_SUB
                | PNG_ASM_FLAG_MMX_READ_FILTER_AVG
                | PNG_ASM_FLAG_MMX_READ_FILTER_PAETH);

      if (prefs->show_dev_opts) {
        g_printerr("Thread unsafe features of libpng disabled.\n");
      }
    }
#endif

    if (prefs->show_dev_opts && mask != 0) {
      uint64_t xmask = (uint64_t)mask;
      g_printerr("enabling png opts %lu\n", xmask);
    }

    if (!mask) png_flagstate = -1;
    else {
#ifdef PNG_ASSEMBLER_CODE_SUPPORTED
      png_flags = png_get_asm_flags(png_ptr);
      png_flags |= mask;
      png_flagstate = 1;
#endif
    }
  }

#define PNG_BIO
#ifdef PNG_BIO
  static void png_read_func(png_structp png_ptr, png_bytep data, png_size_t length) {
    int fd = LIVES_POINTER_TO_INT(png_get_io_ptr(png_ptr));
    //lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
    if (lives_read_buffered(fd, data, length, TRUE) < length) {
      png_error(png_ptr, "read_fn error");
    }
  }
#endif

  typedef struct {
    weed_layer_t *layer;
    int width, height;
    LiVESInterpType interp;
    int pal, clamp;
  } resl_priv_data;


  static lives_proc_thread_t resthread = NULL;

  static void res_thrdfunc(void *arg) {
    resl_priv_data *priv = (resl_priv_data *)arg;
    resize_layer(priv->layer, priv->width, priv->height, priv->interp, priv->pal, priv->clamp);
    //weed_set_voidptr_value(priv->layer, WEED_LEAF_RESIZE_THREAD, NULL);
    lives_free(priv);
  }


  static void reslayer_thread(weed_layer_t *layer, int twidth, int theight, LiVESInterpType interp,
                              int tpalette, int clamp, double file_gamma) {
    resl_priv_data *priv = (resl_priv_data *)lives_malloc(sizeof(resl_priv_data));
    //lives_thread_t *res_thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
    weed_set_double_value(layer, "file_gamma", file_gamma);
    priv->layer = layer;
    priv->width = twidth;
    priv->height = theight;
    priv->interp = interp;
    priv->pal = tpalette;
    priv->clamp = clamp;
    resthread = lives_proc_thread_create(LIVES_THRDATTR_NO_GUI, (lives_funcptr_t)res_thrdfunc, -1, "v", priv);
    //weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, res_thread);
    // res_proc = lives_proc_thread_create(res_thread, LIVES_THRDATTR_NONE, res_thrdfunc, (void *)priv);
  }


  boolean layer_from_png(int fd, weed_layer_t *layer, int twidth, int theight, int tpalette, boolean prog) {
    png_structp png_ptr;
    png_infop info_ptr;
    double file_gamma;
    int gval;
#ifndef PNG_BIO
    FILE *fp = fdopen(fd, "rb");
    size_t bsize = fread(ibuff, 1, 8, fp);
    boolean is_png = TRUE;
    unsigned char ibuff[8];
#endif

    unsigned char **row_ptrs;
    unsigned char *ptr;

    boolean is16bit = FALSE;

    png_uint_32 xwidth, xheight;

    int width, height;
    int color_type, bit_depth;
    int rowstride;
    int flags, privflags;

    register int i = 0;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);

    if (!png_ptr) {
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

#if defined(PNG_LIBPNG_VER) && (PNG_LIBPNG_VER >= 10200)
    if (!png_flagstate) png_init(png_ptr);
#ifdef PNG_ASSEMBLER_CODE_SUPPORTED
    if (png_flagstate == 1) png_set_asm_flags(png_ptr, png_flags);
#endif
#endif

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
      // libpng will longjump to here on error
      weed_set_int_value(layer, WEED_LEAF_PROGSCAN, 0);
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#ifndef PNG_BIO
      fclose(fp);
#endif
      return FALSE;
    }

#ifdef PNG_BIO
    png_set_read_fn(png_ptr, LIVES_INT_TO_POINTER(fd), png_read_func);
#ifndef VALGRIND_ON
    png_set_sig_bytes(png_ptr, 8);
#else
    png_set_sig_bytes(png_ptr, 0);
#endif
#else
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, bsize);
#endif

    // read header info
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &xwidth, &xheight,
                 NULL, NULL, NULL,
                 NULL, NULL);
    if (xwidth > 0 && xheight > 0) {
      weed_set_int_value(layer, WEED_LEAF_WIDTH, xwidth);
      weed_set_int_value(layer, WEED_LEAF_HEIGHT, xheight);

      privflags = weed_get_int_value(layer, WEED_LEAF_HOST_FLAGS, NULL);
      weed_set_int_value(layer, WEED_LEAF_HOST_FLAGS, privflags | LIVES_LAYER_HAS_SIZE_NOW);
      if (privflags == LIVES_LAYER_GET_SIZE_ONLY
          || (privflags == LIVES_LAYER_LOAD_IF_NEEDS_RESIZE
              && (int)xwidth == twidth && (int)xheight == theight)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#ifndef PNG_BIO
        fclose(fp);
#endif
        return TRUE;
      }
    }

    flags = weed_layer_get_flags(layer);

#if PNG_LIBPNG_VER >= 10504
    if (prefs->alpha_post) {
      if (flags & WEED_LAYER_ALPHA_PREMULT) flags ^= WEED_LAYER_ALPHA_PREMULT;
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);
    } else {
      flags |= WEED_LAYER_ALPHA_PREMULT;
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PREMULTIPLIED, PNG_DEFAULT_sRGB);
    }
#endif

    weed_set_int_value(layer, WEED_LEAF_FLAGS, flags);

    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    gval = png_get_gAMA(png_ptr, info_ptr, &file_gamma);

    if (!gval) {
      // b > a, brighter
      //png_set_gamma(png_ptr, 1.0, .45455); /// default, seemingly
      //png_set_gamma(png_ptr, 1. / .45455, 1.0); /// too bright
      png_set_gamma(png_ptr, 1.0, 1.0);
    }

    if (gval == PNG_INFO_gAMA) {
      weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, WEED_GAMMA_LINEAR);
    }

    // want to convert everything (greyscale, RGB, RGBA64 etc.) to RGBA32 (or RGB24)
    if (color_type == PNG_COLOR_TYPE_PALETTE)
      png_set_palette_to_rgb(png_ptr);

    if (png_get_valid(png_ptr, info_ptr,
                      PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY &&
        bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      png_set_gray_to_rgb(png_ptr);

    if (bit_depth == 16) {
      // if tpalette is YUV, then recreate the pixel_data with double the width
      // and mark as 16bpc, then >> 8 when doing the conversion
#ifdef xUSE_16BIT_PCONV
      /// needs testing
      if (weed_palette_is_yuv(tpalette)) {
        width *= 2;
        is16bit = TRUE;
      } else {
#endif
#if PNG_LIBPNG_VER >= 10504
        png_set_scale_16(png_ptr);
#else
        png_set_strip_16(png_ptr);
#endif
#ifdef xUSE_16BIT_PCONV
      }
#endif
    }

    if (tpalette != WEED_PALETTE_END) {
      if (weed_palette_has_alpha(tpalette)) {
        // if target has alpha, add a channel
        if (color_type != PNG_COLOR_TYPE_RGB_ALPHA &&
            color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
          if (tpalette == WEED_PALETTE_ARGB32)
            png_set_add_alpha(png_ptr, 255, PNG_FILLER_BEFORE);
          else
            png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);
          color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        } else {
          if (tpalette == WEED_PALETTE_ARGB32) {
            png_set_swap_alpha(png_ptr);
          }
        }
      } else {
        // else remove it
        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
          png_set_strip_alpha(png_ptr);
          color_type = PNG_COLOR_TYPE_RGB;
        }
      }
      if (tpalette == WEED_PALETTE_BGR24 || tpalette == WEED_PALETTE_BGRA32) {
        png_set_bgr(png_ptr);
      }
    }

    // unnecessary for read_image or if we set npass
    //png_set_interlace_handling(png_ptr);

    // read updated info with the new palette
    png_read_update_info(png_ptr, info_ptr);

    if (i == 0) {
      width = png_get_image_width(png_ptr, info_ptr);
      height = png_get_image_height(png_ptr, info_ptr);

      weed_set_int_value(layer, WEED_LEAF_WIDTH, width);
      weed_set_int_value(layer, WEED_LEAF_HEIGHT, height);

      if (weed_palette_is_rgb(tpalette)) {
        weed_layer_set_palette(layer, tpalette);
      } else {
        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
          weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
        } else {
          weed_layer_set_palette(layer, WEED_PALETTE_RGB24);
        }
      }

      weed_layer_pixel_data_free(layer);

      if (!create_empty_pixel_data(layer, FALSE, TRUE)) {
        create_blank_layer(layer, LIVES_FILE_EXT_PNG, 4, 4, weed_layer_get_palette(layer));
#ifndef PNG_BIO
        fclose(fp);
#endif
        return FALSE;
      }

      // TODO: rowstride must be at least png_get_rowbytes(png_ptr, info_ptr)

      rowstride = weed_layer_get_rowstride(layer);
      ptr = weed_layer_get_pixel_data_packed(layer);

      // libpng needs pointers to each row
      row_ptrs = (unsigned char **)lives_malloc(height * sizeof(unsigned char *));
      for (int j = 0; j < height; j++) {
        row_ptrs[j] = ptr;
        ptr += rowstride;
      }
    }

    if (0 && (weed_threadsafe && twidth * theight != 0 && (twidth != width || theight != height) &&
              !png_get_interlace_type(png_ptr, info_ptr))) {
      weed_set_int_value(layer, WEED_LEAF_PROGSCAN, 1);
      reslayer_thread(layer, twidth, theight, get_interp_value(prefs->pb_quality, TRUE),
                      tpalette, weed_layer_get_yuv_clamping(layer),
                      gval == PNG_INFO_gAMA ? file_gamma : 1.);
      for (int j = 0; j < height; j++) {
        png_read_row(png_ptr, row_ptrs[j], NULL);
        weed_set_int_value(layer, WEED_LEAF_PROGSCAN, j + 1);
      }
      //weed_set_int_value(layer, WEED_LEAF_PROGSCAN, -1);
    } else {
      png_read_image(png_ptr, row_ptrs);
    }
    //png_read_end(png_ptr, NULL);

    // end read

    lives_free(row_ptrs);

    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

    /// it seems that if no gAMA is set then libpng (rather wastefully) always tries to convert to linear
    /// assuming a file gamma of approx. 1.2* (this is not accurate since sRGB -> linear doesnt use a power law)
    /// if gAMA is set, then we (correctly) need to convert to linear using the file gamma
    /// *I think this comes from 0.5 (approx linear to sRGB) * 2,4 (guessed display gamma)

    if (gval == PNG_INFO_gAMA) {
      /// img needs gamma converting
      /// TODO: can we do this using png_set_gamma ?
      weed_layer_set_gamma(layer, WEED_GAMMA_LINEAR);
    } else weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

    if (is16bit) {
      int clamping, sampling, subspace;
      weed_layer_get_palette_yuv(layer, &clamping, &sampling, &subspace);
      weed_set_int_value(layer, WEED_LEAF_PIXEL_BITS, 16);
      if (weed_palette_has_alpha(tpalette)) tpalette = WEED_PALETTE_YUVA4444P;
      else {
        if (tpalette != WEED_PALETTE_YUV420P) tpalette = WEED_PALETTE_YUV444P;
      }
      //if ((resl_thrd = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) {
      if (resthread) {
        lives_nanosleep_until_nonzero(lives_proc_thread_check(resthread));
        lives_proc_thread_join(resthread);
        resthread = NULL;
      }
      // convert RGBA -> YUVA4444P or RGB -> 444P or 420
      // 16 bit conversion
      convert_layer_palette_full(layer, tpalette, clamping, sampling, subspace, WEED_GAMMA_UNKNOWN);
    }
#ifndef PNG_BIO
    fclose(fp);
#endif
    return TRUE;
  }


  // unused
  boolean save_to_png(FILE * fp, weed_layer_t *layer, int comp) {
    // comp is 0 (none) - 9 (full)
    png_structp png_ptr;
    png_infop info_ptr;

    unsigned char *ptr;

    int width, height, palette;
#if PNG_LIBPNG_VER >= 10504
    int flags = 0;
#endif
    int rowstride;

    register int i;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);

    if (!png_ptr) {
      fclose(fp);
      return FALSE;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
      fclose(fp);
      return FALSE;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
      // libpng will longjump to here on error
      if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
      png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
      return FALSE;
    }

    png_init_io(png_ptr, fp);

    width = weed_layer_get_width(layer);
    height = weed_layer_get_height(layer);
    rowstride = weed_layer_get_rowstride(layer);
    palette = weed_layer_get_palette(layer);

    if (width <= 0 || height <= 0 || rowstride <= 0) {
      LIVES_WARN("Cannot make png with 0 width or height");
      return FALSE;
    }

    switch (palette) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_BGR24:
      png_set_IHDR(png_ptr, info_ptr, width, height,
                   8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      break;
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
      png_set_IHDR(png_ptr, info_ptr, width, height,
                   8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      break;
    default:
      LIVES_ERROR("Bad png palette !\n");
      break;
    }

    png_set_compression_level(png_ptr, comp);

    //png_set_write_status_fn(png_ptr, png_row_callback);

#if PNG_LIBPNG_VER >= 10504
    flags = weed_layer_get_flags(layer);
    if (flags & WEED_LAYER_ALPHA_PREMULT) {
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PREMULTIPLIED, PNG_DEFAULT_sRGB);
    } else {
      png_set_alpha_mode(png_ptr, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);
    }
#endif

    if (weed_layer_get_gamma(layer) == WEED_GAMMA_LINEAR)
      png_set_gAMA(png_ptr, info_ptr, 1.0);
    else
      png_set_gAMA(png_ptr, info_ptr, 0.45455);

    png_write_info(png_ptr, info_ptr);

    ptr = (unsigned char *)weed_layer_get_pixel_data_packed(layer);

    // Write image data
    for (i = 0 ; i < height ; i++) {
      png_write_row(png_ptr, ptr);
      ptr += rowstride;
    }

    // end write
    png_write_end(png_ptr, (png_infop)NULL);

    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

    fflush(fp);

    return TRUE;
  }
#endif


  boolean weed_layer_create_from_file_progressive(weed_layer_t *layer, const char *fname, int width,
      int height, int tpalette, const char *img_ext) {
    LiVESPixbuf *pixbuf = NULL;
    LiVESError *gerror = NULL;
    boolean ret = TRUE;
#ifndef VALGRIND_ON
    boolean is_png = FALSE;
#endif
    int fd = -1;

#ifndef NO_PROG_LOAD
#ifdef GUI_GTK
    GdkPixbufLoader *pbload;
#endif
    uint8_t ibuff[IMG_BUFF_SIZE];
    size_t bsize;

#ifndef VALGRIND_ON
    if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) is_png = TRUE;
#endif

#ifdef PNG_BIO
    fd = lives_open_buffered_rdonly(fname);
    if (fd < 0) break_me(fname);
    if (fd < 0) return FALSE;
#ifndef VALGRIND_ON
    if (is_png) lives_buffered_rdonly_slurp(fd, 8);
    else
      lives_buffered_rdonly_slurp(fd, 0);
#endif
#else
    fd = lives_open2(fname, O_RDONLY);
#endif
    if (fd < 0) return FALSE;
#ifndef PNG_BIO
#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#endif

    xxwidth = width;
    xxheight = height;

    if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) {
#ifdef USE_LIBPNG
      tpalette = weed_layer_get_palette(layer);
      ret = layer_from_png(fd, layer, width, height, tpalette, TRUE);
      goto fndone;
#endif

#ifdef GUI_GTK
      pbload = gdk_pixbuf_loader_new_with_type(LIVES_IMAGE_TYPE_PNG, &gerror);
#endif
    }
#ifdef GUI_GTK
    else if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) pbload = gdk_pixbuf_loader_new_with_type(LIVES_IMAGE_TYPE_JPEG, &gerror);
    else pbload = gdk_pixbuf_loader_new();

    lives_signal_connect(LIVES_WIDGET_OBJECT(pbload), LIVES_WIDGET_SIZE_PREPARED_SIGNAL,
                         LIVES_GUI_CALLBACK(pbsize_set), NULL);

    while (1) {
#ifndef PNG_BIO
      if ((bsize = read(fd, ibuff, IMG_BUFF_SIZE)) <= 0) break;
#else
      if ((bsize = lives_read_buffered(fd, ibuff, IMG_BUFF_SIZE, TRUE)) <= 0) break;
#endif
      if (!gdk_pixbuf_loader_write(pbload, ibuff, bsize, &gerror)) {
        ret = FALSE;
        goto fndone;
      }
    }

    if (!gdk_pixbuf_loader_close(pbload, &gerror)) {
      ret = FALSE;
      goto fndone;
    }
    pixbuf = gdk_pixbuf_loader_get_pixbuf(pbload);
    lives_widget_object_ref(pixbuf);
    if (pbload) lives_widget_object_unref(pbload);

#endif

# else //PROG_LOAD

#ifdef USE_LIBPNG
    {
#ifdef PNG_BIO
      fd = lives_open_buffered_rdonly(fname);
#else
      fd = lives_open2(fname, O_RDONLY);
#endif

      if (fd < 0) return FALSE;

#ifndef PNG_BIO
#ifdef HAVE_POSIX_FADVISE
      posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#endif
      tpalette = weed_layer_get_palette(layer);
      ret = layer_from_png(fd, layer, width, height, tpalette, FALSE);
      goto fndone;
    }
#endif

    pixbuf = lives_pixbuf_new_from_file_at_scale(fname, width > 0 ? width : -1, height > 0 ? height : -1, FALSE, gerror);
#endif

    if (gerror) {
      LIVES_ERROR(gerror->message);
      lives_error_free(gerror);
      pixbuf = NULL;
    }

    if (!pixbuf) {
      ret = FALSE;
      goto fndone;
    }

    if (lives_pixbuf_get_has_alpha(pixbuf)) {
      /* unfortunately gdk pixbuf loader does not preserve the original alpha channel, instead it adds its own.
         We need to hence reset it back to opaque */
      lives_pixbuf_set_opaque(pixbuf);
      weed_layer_set_palette(layer, WEED_PALETTE_RGBA32);
    } else weed_layer_set_palette(layer, WEED_PALETTE_RGB24);

    if (!pixbuf_to_layer(layer, pixbuf)) {
      lives_widget_object_unref(pixbuf);
    }

fndone:
#ifdef PNG_BIO
    if (fd >= 0) {
      lives_close_buffered(fd);
    }
#else
    if (fd >= 0) close(fd);
#endif

    return ret;
  }


  static weed_plant_t *render_subs_from_file(lives_clip_t *sfile, double xtime, weed_layer_t *layer) {
    // render subtitles from whatever (.srt or .sub) file
    // uses default values for colours, fonts, size, etc.

    // TODO - allow prefs settings for colours, fonts, size, alpha (use plugin for this)

    //char *sfont=mainw->font_list[prefs->sub_font];
    const char *sfont = "Sans";
    lives_colRGBA64_t col_white, col_black_a;

    int error, size;

    xtime -= (double)sfile->subt->offset / sfile->fps;

    // round to 2 dp
    xtime = (double)((int)(xtime * 100. + .5)) / 100.;

    if (xtime < 0.) return layer;

    get_subt_text(sfile, xtime);

    if (!sfile->subt->text) return layer;

    size = weed_get_int_value(layer, WEED_LEAF_WIDTH, &error) / 32;

    col_white = lives_rgba_col_new(65535, 65535, 65535, 65535);
    col_black_a = lives_rgba_col_new(0, 0, 0, SUB_OPACITY);

    if (prefs->apply_gamma && prefs->pb_quality == PB_QUALITY_HIGH) {
      // make it look nicer by dimming relative to luma
      gamma_convert_layer(WEED_GAMMA_LINEAR, layer);
    }

    layer = render_text_to_layer(layer, sfile->subt->text, sfont, size,
                                 LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND, &col_white, &col_black_a, TRUE, TRUE, 0.);
    return layer;
  }


  boolean pull_frame_at_size(weed_layer_t *layer, const char *image_ext, weed_timecode_t tc, int width, int height,
                             int target_palette) {
    // pull a frame from an external source into a layer
    // the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer
    // tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
    // image_ext is used if the source is an image file (eg. "jpg" or "png")
    // width and height are hints only, the caller should resize if necessary
    // target_palette is also a hint

    // if we pull from a decoder plugin, then we may also deinterlace
    weed_plant_t *vlayer;
    lives_clip_t *sfile = NULL;
    int clip = lives_layer_get_clip(layer);
    frames_t frame = lives_layer_get_frame(layer);
    int clip_type;

    boolean is_thread = FALSE;

    // the default unless overridden
    weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

    if (weed_plant_has_leaf(layer, WEED_LEAF_HOST_PTHREAD)) is_thread = TRUE;

    weed_layer_pixel_data_free(layer);
    weed_leaf_delete(layer, WEED_LEAF_NATURAL_SIZE);

    mainw->osc_block = TRUE; // block OSC until we are done

    if (clip < 0 && frame == 0) clip_type = CLIP_TYPE_DISK;
    else {
      sfile = mainw->files[clip];
      if (!sfile) {
        mainw->osc_block = FALSE;
        return FALSE;
      }
      clip_type = sfile->clip_type;
    }

    switch (clip_type) {
    case CLIP_TYPE_NULL_VIDEO:
      mainw->osc_block = FALSE;
      create_blank_layer(layer, image_ext, width, height, target_palette);
      return TRUE;
    case CLIP_TYPE_DISK:
    case CLIP_TYPE_FILE:
      // frame number can be 0 during rendering
      if (frame == 0) {
        mainw->osc_block = FALSE;
        create_blank_layer(layer, image_ext, width, height, target_palette);
        return TRUE;
      } else if (clip == mainw->scrap_file) {
        boolean res = load_from_scrap_file(layer, frame);
        if (!res) {
          create_blank_layer(layer, image_ext, width, height, target_palette);
          return FALSE;
        }
        weed_leaf_delete(layer, WEED_LEAF_HOST_PIXBUF_SRC);
        weed_leaf_delete(layer, WEED_LEAF_HOST_SURFACE_SRC);
        // clip width and height may vary dynamically
        if (LIVES_IS_PLAYING) {
          sfile->hsize = weed_layer_get_width(layer);
          sfile->vsize = weed_layer_get_height(layer);
        }
        // realign
        copy_pixel_data(layer, NULL, THREADVAR(rowstride_alignment));
        mainw->osc_block = FALSE;
        return TRUE;
      } else {
        if (sfile->clip_type == CLIP_TYPE_FILE && sfile->frame_index && frame > 0 &&
            frame <= sfile->frames && is_virtual_frame(clip, frame)) {
          // pull frame from video clip
          ///
#ifdef USE_REC_RS
          int nplanes;
#endif
          void **pixel_data;
          boolean res = TRUE;
          int *rowstrides;
          lives_decoder_t *dplug = NULL;
          /// HOST_DECODER is set in mulitrack, there is 1 decoder per track since multiple tracks can have the same clip
          if (weed_plant_has_leaf(layer, WEED_LEAF_HOST_DECODER)) {
            dplug = (lives_decoder_t *)weed_get_voidptr_value(layer, WEED_LEAF_HOST_DECODER, NULL);
          } else {
            /// experimental, multiple decoder plugins for each sfile,,,
            if (weed_plant_has_leaf(layer, "alt_src")) {
              int srcnum = weed_get_int_value(layer, "alt_src", NULL);
              dplug = sfile->alt_srcs[srcnum];
            }
            if (!dplug) dplug = (lives_decoder_t *)sfile->ext_src;
          }
          if (!dplug || !dplug->cdata) {
            create_blank_layer(layer, image_ext, width, height, target_palette);
            return FALSE;
          }
          if (target_palette != dplug->cdata->current_palette) {
            if (dplug->decoder->set_palette) {
              int opal = dplug->cdata->current_palette;
              int pal = best_palette_match(dplug->cdata->palettes, -1, target_palette);
              if (pal != opal) {
                dplug->cdata->current_palette = pal;
                if (!(*dplug->decoder->set_palette)(dplug->cdata)) {
                  dplug->cdata->current_palette = opal;
                  (*dplug->decoder->set_palette)(dplug->cdata);
                } else if (dplug->cdata->rec_rowstrides) {
                  lives_free(dplug->cdata->rec_rowstrides);
                  dplug->cdata->rec_rowstrides = NULL;
		// *INDENT-OFF*
	      }}}}
	// *INDENT-ON*

          // TODO *** - check for auto-border : we might use width,height instead of frame_width,frame_height,
          // and handle this in the plugin

          if (!prefs->auto_nobord) {
            width = dplug->cdata->frame_width / weed_palette_get_pixels_per_macropixel(dplug->cdata->current_palette);
            height = dplug->cdata->frame_height;
          } else {
            width = dplug->cdata->width / weed_palette_get_pixels_per_macropixel(dplug->cdata->current_palette);
            height = dplug->cdata->height;
          }

          weed_layer_set_size(layer, width, height);

          if (weed_palette_is_yuv(dplug->cdata->current_palette))
            weed_layer_set_palette_yuv(layer, dplug->cdata->current_palette,
                                       dplug->cdata->YUV_clamping,
                                       dplug->cdata->YUV_sampling,
                                       dplug->cdata->YUV_subspace);
          else weed_layer_set_palette(layer, dplug->cdata->current_palette);

#ifdef USE_REC_RS
          nplanes = weed_palette_get_nplanes(dplug->cdata->current_palette);
          if (!dplug->cdata->rec_rowstrides) {
            dplug->cdata->rec_rowstrides = lives_calloc(nplanes, sizint);
          } else {
            if (dplug->cdata->rec_rowstrides[0]) {
              weed_layer_set_rowstrides(layer, dplug->cdata->rec_rowstrides, nplanes);
              weed_leaf_set_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
              lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
            }
          }
#endif
          if (create_empty_pixel_data(layer, TRUE, TRUE)) {
#ifdef USE_REC_RS
            weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
#endif
            pixel_data = weed_layer_get_pixel_data(layer, NULL);
          } else {
#ifdef USE_REC_RS
            weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
#endif
            create_blank_layer(layer, image_ext, width, height, target_palette);
            return FALSE;
          }

          if (!pixel_data || !pixel_data[0]) {
            char *msg = lives_strdup_printf("NULL pixel data for layer size %d X %d, palette %s\n", width, height,
                                            weed_palette_get_name_full(dplug->cdata->current_palette,
                                                dplug->cdata->YUV_clamping, dplug->cdata->YUV_subspace));
            LIVES_WARN(msg);
            lives_free(msg);
            create_blank_layer(layer, image_ext, width, height, target_palette);
            return FALSE;
          }

          rowstrides = weed_layer_get_rowstrides(layer, NULL);

          //static int last = -1;
          // try to pull frame from decoder plugin
          //if (sfile->frame_index[frame - 1] == last) break_me();
          //last = sfile->frame_index[frame - 1];
          if (!(*dplug->decoder->get_frame)(dplug->cdata, (int64_t)(sfile->frame_index[frame - 1]),
                                            rowstrides, sfile->vsize, pixel_data)) {

#ifdef USE_REC_RS
            if (dplug->cdata->rec_rowstrides) lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
#endif
            if (prefs->show_dev_opts) g_print("Error loading frame %d (index value %d)\n", frame,
                                                sfile->frame_index[frame - 1]);
            // if get_frame fails, return a black frame
            if (!is_thread) {
              weed_layer_pixel_data_free(layer);
              if (!create_empty_pixel_data(layer, TRUE, TRUE)) {
                create_blank_layer(layer, image_ext, width, height, target_palette);
                return FALSE;
              }
            }
            res = FALSE;
          }

          lives_free(pixel_data);
          lives_free(rowstrides);
          if (res) {
            if (prefs->apply_gamma && prefs->pb_quality != PB_QUALITY_LOW) {
              if (dplug->cdata->frame_gamma != WEED_GAMMA_UNKNOWN) {
                weed_layer_set_gamma(layer, dplug->cdata->frame_gamma);
              } else if (dplug->cdata->YUV_subspace == WEED_YUV_SUBSPACE_BT709) {
                weed_layer_set_gamma(layer, WEED_GAMMA_BT709);
              }
            }

            // get_frame may now update YUV_clamping, YUV_sampling, YUV_subspace
            if (weed_palette_is_yuv(dplug->cdata->current_palette)) {
              weed_layer_set_palette_yuv(layer, dplug->cdata->current_palette,
                                         dplug->cdata->YUV_clamping,
                                         dplug->cdata->YUV_sampling,
                                         dplug->cdata->YUV_subspace);
              if (prefs->apply_gamma && prefs->pb_quality != PB_QUALITY_LOW) {
                if (weed_get_int_value(layer, WEED_LEAF_GAMMA_TYPE, NULL) == WEED_GAMMA_BT709) {
                  weed_set_int_value(layer, WEED_LEAF_YUV_SUBSPACE, WEED_YUV_SUBSPACE_BT709);
                }
                if (weed_get_int_value(layer, WEED_LEAF_YUV_SUBSPACE, NULL) == WEED_YUV_SUBSPACE_BT709) {
                  weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, WEED_GAMMA_BT709);
                }
              }
            }
            // deinterlace
            if (sfile->deinterlace || (prefs->auto_deint && dplug->cdata->interlace != LIVES_INTERLACE_NONE)) {
              if (!is_thread) {
                deinterlace_frame(layer, tc);
              } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
            }
          }
          mainw->osc_block = FALSE;
          return res;
        } else {
          // pull frame from decoded images
          boolean ret;
          char *fname = make_image_file_name(sfile, frame, image_ext);
          /* if (height * width == 0) { */
          /*   ret = weed_layer_create_from_file_progressive(layer, fname, 0, 0, target_palette, image_ext); */
          /* } else { */
          //lives_thread_t *resl_thrd;
          if (!*image_ext) image_ext = get_image_ext_for_type(sfile->img_type);
          ret = weed_layer_create_from_file_progressive(layer, fname, width, height, target_palette, image_ext);
          if (resthread) {
            lives_nanosleep_until_nonzero(lives_proc_thread_check(resthread));
            lives_proc_thread_join(resthread);
            resthread = NULL;
          }
          /* if ((resl_thrd = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) { */
          /*   lives_thread_join(*resl_thrd, NULL); */
          /*   weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL); */
          /*   lives_free(resl_thrd); */
          /* } */
          //}

          lives_free(fname);
          mainw->osc_block = FALSE;
          if (!ret) {
            break_me("bad frame load from file");
            create_blank_layer(layer, image_ext, width, height, target_palette);
            return FALSE;
          }
        }
      }
      break;

      // handle other types of sources

#ifdef HAVE_YUV4MPEG
    case CLIP_TYPE_YUV4MPEG:
      weed_layer_set_from_yuv4m(layer, sfile);
      if (sfile->deinterlace) {
        if (!is_thread) {
          deinterlace_frame(layer, tc);
        } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
      }
      mainw->osc_block = FALSE;
      return TRUE;
#endif
#ifdef HAVE_UNICAP
    case CLIP_TYPE_VIDEODEV:
      weed_layer_set_from_lvdev(layer, sfile, 4. / cfile->pb_fps);
      if (sfile->deinterlace) {
        if (!is_thread) {
          deinterlace_frame(layer, tc);
        } else weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_TRUE);
      }
      mainw->osc_block = FALSE;
      return TRUE;
#endif
    case CLIP_TYPE_LIVES2LIVES:
      weed_layer_set_from_lives2lives(layer, clip, (lives_vstream_t *)sfile->ext_src);
      mainw->osc_block = FALSE;
      return TRUE;
    case CLIP_TYPE_GENERATOR: {
      // special handling for clips where host controls size
      // Note: vlayer is actually the out channel of the generator, so we should
      // never free it !
      weed_plant_t *inst = (weed_plant_t *)sfile->ext_src;
      if (inst) {
        int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
        while (filter_mutex_trylock(key)) {
          sched_yield();
          lives_usleep(prefs->sleep_time);
        }
        vlayer = weed_layer_create_from_generator(inst, tc, clip);
        weed_layer_copy(layer, vlayer); // layer is non-NULL, so copy by reference
        weed_layer_nullify_pixel_data(vlayer);
        filter_mutex_unlock(key);
      } else {
        mainw->osc_block = FALSE;
        create_blank_layer(layer, image_ext, width, height, target_palette);
      }
      mainw->osc_block = FALSE;
    }
    return TRUE;
    default:
      mainw->osc_block = FALSE;
      create_blank_layer(layer, image_ext, width, height, target_palette);
      return FALSE;
    }
    mainw->osc_block = FALSE;

    if (!is_thread) {
      // render subtitles from file
      if (prefs->show_subtitles && sfile->subt && sfile->subt->tfile > 0) {
        double xtime = (double)(frame - 1) / sfile->fps;
        layer = render_subs_from_file(sfile, xtime, layer);
      }
    }

    return TRUE;
  }


  /**
     @brief pull a frame from an external source into a layer
     the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer
     tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
     image_ext is used if the source is an image file (eg. "jpg" or "png")
  */
  LIVES_GLOBAL_INLINE boolean pull_frame(weed_layer_t *layer, const char *image_ext, weed_timecode_t tc) {
    return pull_frame_at_size(layer, image_ext, tc, 0, 0, WEED_PALETTE_END);
  }


  /**
     @brief block until layer pixel_data is ready.
     This function should always be called for threaded layers, prior to freeing the layer, reading it's  properites, pixel data,
     resizing etc.

     We may also deinterlace and overlay subs here
     for the blend layer, we may also resize, convert palette, apply gamma in preparation for combining with the main layer

     if effects were applied then the frame_layer can depend on other layers, however
     these wil have been checked already when the effects were applied

     see also MACRO: is_layer_ready(layer) which can be called first to avoid the block, e.g.

     while (!is_layer_ready(layer)) {
     do_something_else();
     }
     check_layer_ready(layer); // won't block

     This function must be called at some point for every threaded frame, otherwise thread resources will be leaked.

     N.B. the name if this function is not the best, it will probably get renamed in th future to something like
     finish_layer.
  */
  boolean check_layer_ready(weed_layer_t *layer) {
    int clip;
    boolean ready = TRUE;
    lives_clip_t *sfile;
    lives_thread_t *thrd;

    if (!layer) return FALSE;

    thrd = (lives_thread_t *)weed_get_voidptr_value(layer, WEED_LEAF_HOST_PTHREAD, NULL);
    if (thrd) {
      ready = FALSE;
      lives_thread_join(*thrd, NULL);
      weed_leaf_delete(layer, WEED_LEAF_HOST_PTHREAD);
      lives_free(thrd);
    }
    if (resthread) {
      ready = FALSE;
      lives_nanosleep_until_nonzero(lives_proc_thread_check(resthread));
      lives_proc_thread_join(resthread);
      resthread = NULL;
    }

    if (ready) return TRUE;

    if (weed_get_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, NULL) == WEED_TRUE) {
      weed_timecode_t tc = weed_get_int64_value(layer, WEED_LEAF_HOST_TC, NULL);
      deinterlace_frame(layer, tc);
      weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_FALSE);
    }

    if (weed_plant_has_leaf(layer, WEED_LEAF_CLIP)) {
      // TODO: we should render subs before display, to avoid the text size changing
      clip = weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);
      if (IS_VALID_CLIP(clip)) {
        sfile = mainw->files[clip];
        // render subtitles from file
        if (sfile->subt && sfile->subt->tfile >= 0 && prefs->show_subtitles) {
          frames_t frame = weed_get_int_value(layer, WEED_LEAF_FRAME, NULL);
          double xtime = (double)(frame - 1) / sfile->fps;
          layer = render_subs_from_file(sfile, xtime, layer);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

    return TRUE;
  }


  typedef struct {
    weed_layer_t *layer;
    weed_timecode_t tc;
    const char *img_ext;
    int width, height;
  } pft_priv_data;


  static void *pft_thread(void *in) {
    pft_priv_data *data = (pft_priv_data *)in;
    weed_layer_t *layer = data->layer;
    weed_timecode_t tc = data->tc;
    const char *img_ext = data->img_ext;
    int width = data->width, height = data->height;
    lives_free(in);
    /// if loading the blend frame in clip editor, then we recall the palette details and size @ injection,
    //and prepare it in this thread
    if (mainw->blend_file != -1 && mainw->blend_palette != WEED_PALETTE_END
        && LIVES_IS_PLAYING && !mainw->multitrack && mainw->blend_file != mainw->current_file
        && weed_get_int_value(layer, WEED_LEAF_CLIP, NULL) == mainw->blend_file) {
      int tgamma = WEED_GAMMA_UNKNOWN;
      short interp = get_interp_value(prefs->pb_quality, TRUE);
      pull_frame_at_size(layer, img_ext, tc, mainw->blend_width, mainw->blend_height, mainw->blend_palette);
      if (is_layer_ready(layer)) {
        resize_layer(layer, mainw->blend_width,
                     mainw->blend_height, interp, mainw->blend_palette, mainw->blend_clamping);
      }
      if (mainw->blend_palette != WEED_PALETTE_END) {
        if (weed_palette_is_rgb(mainw->blend_palette))
          tgamma = mainw->blend_gamma;
      }
      if (mainw->blend_palette != WEED_PALETTE_END) {
        if (is_layer_ready(layer) && weed_layer_get_width(layer) == mainw->blend_width
            && weed_layer_get_height(layer) == mainw->blend_height) {
          convert_layer_palette_full(layer, mainw->blend_palette, mainw->blend_clamping, mainw->blend_sampling,
                                     mainw->blend_subspace, tgamma);
        }
      }
      if (tgamma != WEED_GAMMA_UNKNOWN && is_layer_ready(layer)
          && weed_layer_get_width(layer) == mainw->blend_width
          && weed_layer_get_height(layer) == mainw->blend_height
          && weed_layer_get_palette(layer) == mainw->blend_palette)
        gamma_convert_layer(mainw->blend_gamma, layer);
    } else {
      pull_frame_at_size(layer, img_ext, tc, width, height, WEED_PALETTE_END);
    }
    weed_set_boolean_value(layer, WEED_LEAF_THREAD_PROCESSING, WEED_FALSE);
    return NULL;
  }


  void pull_frame_threaded(weed_layer_t *layer, const char *img_ext, weed_timecode_t tc, int width, int height) {
    // pull a frame from an external source into a layer
    // the WEED_LEAF_CLIP and WEED_LEAF_FRAME leaves must be set in layer

    // done in a threaded fashion
    // call check_layer_ready() to block until the frame thread is completed
#ifdef NO_FRAME_THREAD
    pull_frame(layer, img_ext, tc);
    return;
#else
    //#define MULT_SRC_TEST
#ifdef MULT_SRC_TEST
    if (layer == mainw->frame_layer_preload) {
      int clip = mainw->pred_clip;
      if (IS_VALID_CLIP(clip)) {
        lives_clip_t *sfile = mainw->files[clip];
        if (sfile->clip_type == CLIP_TYPE_FILE) {
          if (!sfile->alt_srcs) {
            sfile->alt_srcs = lives_calloc(1, sizeof(void *));
            sfile->alt_srcs[0] = clone_decoder(clip);
            if (sfile->alt_srcs[0]) {
              weed_set_int_value(layer, "alt_src", 0);
              sfile->alt_src_types = lives_calloc(1, sizint);
              sfile->alt_src_types[0] = LIVES_EXT_SRC_DECODER;
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*
#endif
    weed_set_boolean_value(layer, WEED_LEAF_THREAD_PROCESSING, WEED_TRUE);
    if (1) {
      lives_thread_attr_t attr = LIVES_THRDATTR_PRIORITY;
      pft_priv_data *in = (pft_priv_data *)lives_calloc(1, sizeof(pft_priv_data));
      lives_thread_t *frame_thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
      weed_set_int64_value(layer, WEED_LEAF_HOST_TC, tc);
      weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_FALSE);
      weed_set_voidptr_value(layer, WEED_LEAF_HOST_PTHREAD, (void *)frame_thread);
      in->img_ext = img_ext;
      in->layer = layer;
      in->width = width;
      in->height = height;
      in->tc = tc;
      lives_thread_create(frame_thread, attr, pft_thread, (void *)in);
    }
#endif
}


LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, weed_timecode_t tc,
int width, int height, LiVESInterpType interp, boolean fordisp) {
// return a correctly sized (Gdk)Pixbuf (RGB24 for jpeg, RGB24 / RGBA32 for png) for the given clip and frame
// tc is used instead of WEED_LEAF_FRAME for some sources (e.g. generator plugins)
// image_ext is used if the source is an image file (eg. "jpg" or "png")
// pixbuf will be sized to width x height pixels using interp

LiVESPixbuf *pixbuf = NULL;
weed_layer_t *layer = lives_layer_new_for_frame(clip, frame);
int palette;

#ifndef ALLOW_PNG24
if (!strcmp(image_ext, LIVES_FILE_EXT_PNG)) palette = WEED_PALETTE_RGBA32;
else palette = WEED_PALETTE_RGB24;
#else
if (strcmp(image_ext, LIVES_FILE_EXT_PNG)) palette = WEED_PALETTE_RGB24;
else palette = WEED_PALETTE_END;
#endif

if (pull_frame_at_size(layer, image_ext, tc, width, height, palette)) {
check_layer_ready(layer);
pixbuf = layer_to_pixbuf(layer, TRUE, fordisp);
}
weed_layer_free(layer);
if (pixbuf && ((width != 0 && lives_pixbuf_get_width(pixbuf) != width)
|| (height != 0 && lives_pixbuf_get_height(pixbuf) != height))) {
LiVESPixbuf *pixbuf2;
// TODO - could use resize plugin here
pixbuf2 = lives_pixbuf_scale_simple(pixbuf, width, height, interp);
if (pixbuf) lives_widget_object_unref(pixbuf);
pixbuf = pixbuf2;
}

return pixbuf;
}


LIVES_GLOBAL_INLINE LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, weed_timecode_t tc) {
return pull_lives_pixbuf_at_size(clip, frame, image_ext, tc, 0, 0, LIVES_INTERP_NORMAL, FALSE);
}


void get_player_size(int *opwidth, int *opheight) {
// calc output size for display
int rwidth, rheight;

///// external playback plugin
if (mainw->ext_playback) {
// playback plugin (therefore fullscreen / separate window)
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

if (mainw->play_window) {
// playback in separate window
// use values set in resize_play_window
*opwidth = mainw->pwidth;
*opheight = mainw->pheight;
if (!mainw->multitrack && prefs->letterbox) {
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
// frame max sizes for multitrack
/* *opwidth = mainw->files[mainw->multitrack->render_file]->hsize; */
/* *opheight = mainw->files[mainw->multitrack->render_file]->vsize; */
/* if (!mainw->multitrack->is_rendering) { */
/*   calc_maxspect(mainw->multitrack->play_width, mainw->multitrack->play_height, opwidth, opheight); */
/* } */
*opwidth = mainw->multitrack->play_width;
*opheight = mainw->multitrack->play_height;
goto align;
}

////////////////////////////////////////////////////////////////////////////////////
// clip edit mode
if (mainw->is_rendering && !mainw->preview) {
*opwidth = cfile->hsize;
*opheight = cfile->vsize;
goto align;
}

if (!mainw->fs) {
// embedded player
rwidth = lives_widget_get_allocation_width(mainw->playframe);// - H_RESIZE_ADJUST;
rheight = lives_widget_get_allocation_height(mainw->play_image);// - V_RESIZE_ADJUST;

if (prefs->letterbox) {
*opwidth = cfile->hsize;
*opheight = cfile->vsize;
calc_maxspect(rwidth, rheight, opwidth, opheight);
} else {
*opwidth = rwidth;
*opheight = rheight;
}
} else {
// try to get exact inner size of the main window
lives_window_get_inner_size(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), opwidth, opheight);
//*opheight -= 2; // necessary, or screen expands too much (!?)
*opheight -= lives_widget_get_allocation_height(mainw->btoolbar);
}

align:
*opwidth = (*opwidth >> 2) << 2;
*opheight = (*opheight >> 2) << 2;
mainw->pwidth = *opwidth;
mainw->pheight = *opheight;
}


void init_track_decoders(void) {
register int i;
for (i = 0; i < MAX_TRACKS; i++) {
mainw->track_decoders[i] = NULL;
mainw->old_active_track_list[i] = mainw->active_track_list[i] = 0;
}
for (i = 0; i < MAX_FILES; i++) mainw->ext_src_used[i] = FALSE;
}


LIVES_GLOBAL_INLINE void free_track_decoders(void) {
for (register int i = 0; i < MAX_TRACKS; i++) {
if (mainw->track_decoders[i] &&
(mainw->active_track_list[i] <= 0 || mainw->track_decoders[i] != mainw->files[mainw->active_track_list[i]]->ext_src))
close_decoder_plugin(mainw->track_decoders[i]);
}
}


static boolean check_for_overlay_text(weed_layer_t *layer) {
if (mainw->urgency_msg && prefs->show_urgency_msgs) {
ticks_t timeout = lives_alarm_check(LIVES_URGENCY_ALARM);
if (!timeout) {
lives_freep((void **)&mainw->urgency_msg);
return FALSE;
}
render_text_overlay(layer, mainw->urgency_msg);
return TRUE;
}

if ((mainw->overlay_msg && prefs->show_overlay_msgs) || mainw->lockstats) {
if (mainw->lockstats) {
lives_freep((void **)&mainw->overlay_msg);
show_sync_callback(NULL, NULL, 0, 0, LIVES_INT_TO_POINTER(1));
if (mainw->overlay_msg) {
render_text_overlay(layer, mainw->overlay_msg);
if (prefs->render_overlay && mainw->record && !mainw->record_paused) {
weed_plant_t *event = get_last_frame_event(mainw->event_list);
if (event) weed_set_string_value(event, WEED_LEAF_OVERLAY_TEXT, mainw->overlay_msg);
}
}
return TRUE;
} else {
if (!mainw->preview_rendering) {
ticks_t timeout = lives_alarm_check(mainw->overlay_alarm);
if (timeout == 0) {
lives_freep((void **)&mainw->overlay_msg);
return FALSE;
}
}
render_text_overlay(layer, mainw->overlay_msg);
if (mainw->preview_rendering) lives_freep((void **)&mainw->overlay_msg);
return TRUE;
}
}
return FALSE;
}


static void do_cleanup(weed_layer_t *layer, int success) {
/// cleanup any resources after showing a frame. This run as a thread, so the main thread can return to the player
lives_clip_t *sfile = NULL;
int clip = -1;
double fps = DEF_FPS;
frames_t frame = 0;

if (layer) {
if (layer == mainw->frame_layer) mainw->frame_layer = NULL;
clip = lives_layer_get_clip(layer);
frame = lives_layer_get_frame(layer);
if (IS_VALID_CLIP(clip)) {
sfile = mainw->files[clip];
fps = sfile->pb_fps;
}
}

if (success) {
char *tmp;
// format is now msg|timecode|fgclip|fgframe|fgfps|
lives_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH, (const char *)
(tmp = lives_strdup_printf("%.8f|%d|%d|%.3f|", (double)mainw->currticks / TICKS_PER_SECOND_DBL,
clip, frame, fps)));
lives_free(tmp);
}

if (layer) {
check_layer_ready(layer);
///mainw->frame_layer_postload = mainw->frame_layer;
weed_layer_free(layer);
}
}


//static weed_plant_t *cleaner = NULL;

/* void wait_for_cleaner(void) { */
/*   if (cleaner) { */
/*     lives_nanosleep_until_nonzero(weed_get_boolean_value(cleaner, WEED_LEAF_DONE, NULL)); */
/*     weed_plant_free(cleaner); */
/*     cleaner = NULL; */
/*   } */
/* } */

#define USEC_WAIT_FOR_SYNC 500000

static boolean avsync_check(void) {
register int count = USEC_WAIT_FOR_SYNC / 10, rc = 0;
struct timespec ts;

#ifdef VALGRIND_ON
count *= 10;
#endif

if (mainw->foreign || !LIVES_IS_PLAYING || prefs->audio_src == AUDIO_SRC_EXT || prefs->force_system_clock
|| (mainw->event_list && !(mainw->record || mainw->record_paused)) || prefs->audio_player == AUD_PLAYER_NONE
|| !is_realtime_aplayer(prefs->audio_player)) {
mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
return TRUE;
}

if (!mainw->video_seek_ready) {
#ifdef ENABLE_JACK
if (!mainw->foreign && mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
/// try to improve sync by delaying audio pb start
if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
&& !mainw->record_paused && mainw->jackd->is_paused)) {
mainw->video_seek_ready = TRUE;
if (!mainw->switch_during_pb) mainw->force_show = TRUE;
return TRUE;
}
}
#endif
#ifdef HAVE_PULSE_AUDIO
if (!mainw->foreign && mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE) {
/// try to improve sync by delaying audio pb start
if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
&& !mainw->record_paused && mainw->pulsed->is_paused)) {
mainw->video_seek_ready = TRUE;
if (!mainw->switch_during_pb) mainw->force_show = TRUE;
return TRUE;
}
}
#endif
}

clock_gettime(CLOCK_REALTIME, &ts);
pthread_mutex_lock(&mainw->avseek_mutex);
mainw->video_seek_ready = TRUE;
while (!mainw->audio_seek_ready && --count) {
ts.tv_nsec += 10000;   // def. 10 usec (* 50000 = 0.5 sec)
if (ts.tv_nsec >= ONE_BILLION) {
ts.tv_sec++;
ts.tv_nsec -= ONE_BILLION;
}
sched_yield();
rc = pthread_cond_timedwait(&mainw->avseek_cond, &mainw->avseek_mutex, &ts);
mainw->video_seek_ready = TRUE;
}
if (!mainw->audio_seek_ready && rc == ETIMEDOUT) {
pthread_mutex_unlock(&mainw->avseek_mutex);
mainw->cancelled = handle_audio_timeout();
return FALSE;
}
pthread_mutex_unlock(&mainw->avseek_mutex);
return TRUE;
}


void load_frame_image(int frame) {
// this is where we do the actual load/record of a playback frame
// it is called every 1/fps from do_progress_dialog() via process_one() in dialogs.c

// for the multitrack window we set mainw->frame_image; this is used to display the
// preview image

// NOTE: we should be careful if load_frame_image() is called from anywhere inside load_frame_image()
// e.g. by calling g_main_context_iteration() --> user presses sepwin button --> load_frame_image() is called
// this is because mainw->frame_layer is global and gets freed() before exit from load_frame_image()

void **pd_array, **retdata = NULL;
LiVESPixbuf *pixbuf = NULL;

char *framecount = NULL, *tmp;
char *fname_next = NULL, *info_file = NULL;
const char *img_ext = NULL;

LiVESInterpType interp;
double scrap_file_size = -1;

ticks_t audio_timed_out = 1;

boolean was_preview = FALSE;
boolean rec_after_pb = FALSE;
boolean success = FALSE;
boolean size_ok = TRUE;
boolean player_v2 = FALSE;

int retval;
int layer_palette, cpal;

static int old_pwidth = 0, old_pheight = 0;
int opwidth = 0, opheight = 0;
int pwidth, pheight;
int lb_width = 0, lb_height = 0;
int bad_frame_count = 0;
int fg_file = mainw->current_file;
int tgamma = WEED_GAMMA_UNKNOWN;
boolean was_letterboxed = FALSE;

#if defined ENABLE_JACK || defined HAVE_PULSE_AUDIO
lives_alarm_t alarm_handle;
#endif

#define BFC_LIMIT 1000
if (LIVES_UNLIKELY(cfile->frames == 0 && !mainw->foreign && !mainw->is_rendering)) {
if (mainw->record && !mainw->record_paused) {
// add blank frame
weed_plant_t *event = get_last_event(mainw->event_list);
weed_plant_t *event_list = insert_blank_frame_event_at(mainw->event_list, lives_get_relative_ticks(mainw->origsecs,
mainw->orignsecs),
&event);
if (!mainw->event_list) mainw->event_list = event_list;
if (mainw->rec_aclip != -1 && (prefs->rec_opts & REC_AUDIO) && !mainw->record_starting) {
// we are recording, and the audio clip changed; add audio
if (mainw->rec_aclip == mainw->ascrap_file) {
mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
(double)(mainw->files[mainw->ascrap_file]->arps * mainw->files[mainw->ascrap_file]->achans *
mainw->files[mainw->ascrap_file]->asampsize >> 3);
mainw->rec_avel = 1.;
}
if (!mainw->mute) {
insert_audio_event_at(event, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel);
mainw->rec_aclip = -1;
}
}
}
if (!mainw->fs && !mainw->faded) get_play_times();

return;
}

if (!mainw->foreign) {
mainw->actual_frame = frame;
if (!mainw->preview_rendering && (!((was_preview = mainw->preview) || mainw->is_rendering))) {
/////////////////////////////////////////////////////////

// normal play

if (LIVES_UNLIKELY(mainw->nervous)) {
// nervous mode
if ((mainw->actual_frame += (-10 + (int)(21.*rand() / (RAND_MAX + 1.0)))) > cfile->frames ||
mainw->actual_frame < 1) mainw->actual_frame = frame;
else resync_audio(mainw->actual_frame);
}

if (mainw->opening_loc || !CURRENT_CLIP_IS_NORMAL) {
framecount = lives_strdup_printf("%9d", mainw->actual_frame);
} else {
framecount = lives_strdup_printf("%9d / %d", mainw->actual_frame, cfile->frames);
}

/////////////////////////////////////////////////

// record performance
if ((mainw->record && !mainw->record_paused) || mainw->record_starting) {
ticks_t actual_ticks;
int fg_frame = mainw->record_frame;
int bg_file = IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != mainw->current_file
? mainw->blend_file : -1;
int bg_frame = bg_file > 0 && bg_file != mainw->current_file ? mainw->files[bg_file]->frameno : 0;
int numframes;
int *clips;
int64_t *frames;
weed_plant_t *event_list;

// should we record the output from the playback plugin ?
if (mainw->record && (prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
(mainw->vpp->capabilities & VPP_CAN_RETURN)) {
rec_after_pb = TRUE;
}

if (rec_after_pb || !CURRENT_CLIP_IS_NORMAL ||
(prefs->rec_opts & REC_EFFECTS && bg_file != -1 && !IS_NORMAL_CLIP(bg_file))) {
// TODO - handle non-opening of scrap_file
if (mainw->scrap_file == -1) open_scrap_file();
fg_file = mainw->scrap_file;
fg_frame = mainw->files[mainw->scrap_file]->frames + 1;
scrap_file_size = mainw->files[mainw->scrap_file]->f_size;
bg_file = -1;
bg_frame = 0;
}

//actual_ticks = mainw->clock_ticks;//mainw->currticks;
actual_ticks = mainw->startticks; ///< use the "thoretical" time

if (mainw->record_starting) {
if (!mainw->event_list) {
mainw->event_list = lives_event_list_new(NULL, NULL);
}

// mark record start
mainw->event_list = append_marker_event(mainw->event_list, actual_ticks, EVENT_MARKER_RECORD_START);

if (prefs->rec_opts & REC_EFFECTS) {
// add init events and pchanges for all active fx
add_filter_init_events(mainw->event_list, actual_ticks);
}

#ifdef ENABLE_JACK
if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd &&
(prefs->rec_opts & REC_AUDIO) && prefs->audio_src == AUDIO_SRC_INT && mainw->rec_aclip != mainw->ascrap_file) {
// get current seek postion
alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);

while ((audio_timed_out = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd) != NULL) {
// wait for audio player message queue clearing
sched_yield();
lives_usleep(prefs->sleep_time);
}
lives_alarm_clear(alarm_handle);
if (audio_timed_out == 0) {
mainw->cancelled = handle_audio_timeout();
goto lfi_done;
}
jack_get_rec_avals(mainw->jackd);
}
#endif
#ifdef HAVE_PULSE_AUDIO
if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed &&
(prefs->rec_opts & REC_AUDIO) && prefs->audio_src == AUDIO_SRC_INT && mainw->rec_aclip != mainw->ascrap_file) {
// get current seek postion
alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
while ((audio_timed_out = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed) != NULL) {
// wait for audio player message queue clearing
sched_yield();
lives_usleep(prefs->sleep_time);
}
lives_alarm_clear(alarm_handle);
pulse_get_rec_avals(mainw->pulsed);
}
#endif
mainw->record_starting = FALSE;
if (audio_timed_out == 0) {
mainw->cancelled = handle_audio_timeout();
goto lfi_done;
}
mainw->record = TRUE;
mainw->record_paused = FALSE;
}

numframes = (bg_file == -1) ? 1 : 2;
clips = (int *)lives_malloc(numframes * sizint);
frames = (int64_t *)lives_malloc(numframes * 8);

clips[0] = fg_file;
frames[0] = (int64_t)fg_frame;
if (numframes == 2) {
clips[1] = bg_file;
frames[1] = (int64_t)bg_frame;
}
if (framecount) lives_free(framecount);
pthread_mutex_lock(&mainw->event_list_mutex);

/// usual function to record a frame event
if ((event_list = append_frame_event(mainw->event_list, actual_ticks, numframes, clips, frames)) != NULL) {
if (!mainw->event_list) mainw->event_list = event_list;

// TODO ***: do we need to perform more checks here ???
if (scrap_file_size != -1 || (mainw->rec_aclip != -1 && (prefs->rec_opts & REC_AUDIO))) {
weed_plant_t *event = get_last_frame_event(mainw->event_list);

if (scrap_file_size != -1) weed_set_int64_value(event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET, scrap_file_size);

if (mainw->rec_aclip != -1) {
if (mainw->rec_aclip == mainw->ascrap_file) {
mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
(double)(mainw->files[mainw->ascrap_file]->arps * mainw->files[mainw->ascrap_file]->achans *
mainw->files[mainw->ascrap_file]->asampsize >> 3);
mainw->rec_avel = 1.;

}
if (!mainw->mute) {
weed_event_t *xevent = get_prev_frame_event(event);
if (!xevent) xevent = event;
insert_audio_event_at(xevent, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel);
}
mainw->rec_aclip = -1;
}
}
pthread_mutex_unlock(&mainw->event_list_mutex);

/* TRANSLATORS: rec(ord) */
framecount = lives_strdup_printf(_("rec %9d / %d"), mainw->actual_frame,
cfile->frames > mainw->actual_frame ? cfile->frames : mainw->actual_frame);
} else {
pthread_mutex_unlock(&mainw->event_list_mutex);
/* TRANSLATORS: out of memory (rec(ord)) */
(framecount = lives_strdup_printf(_("!rec %9d / %d"), mainw->actual_frame, cfile->frames));
}
lives_free(clips);
lives_free(frames);
} else {
if (mainw->toy_type != LIVES_TOY_NONE) {
if (mainw->toy_type == LIVES_TOY_MAD_FRAMES && !mainw->fs && CURRENT_CLIP_IS_NORMAL) {
int current_file = mainw->current_file;
if (mainw->toy_go_wild) {
int i, other_file;
for (i = 0; i < 11; i++) {
other_file = (1 + (int)((double)(mainw->clips_available) * rand() / (RAND_MAX + 1.0)));
other_file = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->cliplist, other_file));
if (mainw->files[other_file]) {
// steal a frame from another clip
mainw->current_file = other_file;
}
}
}
load_end_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
load_start_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
mainw->current_file = current_file;
}
}
}

if ((!mainw->fs || (prefs->play_monitor != widget_opts.monitor && capable->nmonitors > 1) ||
(mainw->ext_playback && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)))
&& !prefs->hide_framebar) {
lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
//lives_widget_queue_draw(mainw->framecounter);
}
lives_freep((void **)&framecount);
}

if (was_preview) {
// preview
if (mainw->proc_ptr && mainw->proc_ptr->frames_done > 0 &&
frame >= (mainw->proc_ptr->frames_done - cfile->progress_start + cfile->start)) {
if (cfile->opening) {
mainw->proc_ptr->frames_done = cfile->opening_frames = get_frame_count(mainw->current_file, cfile->opening_frames);
}
}
if (mainw->proc_ptr && mainw->proc_ptr->frames_done > 0 &&
frame >= (mainw->proc_ptr->frames_done - cfile->progress_start + cfile->start)) {
mainw->cancelled = CANCEL_PREVIEW_FINISHED;
goto lfi_done;
}

// play preview
if (cfile->opening || (cfile->next_event && !mainw->proc_ptr)) {
fname_next = make_image_file_name(cfile, frame + 1, get_image_ext_for_type(cfile->img_type));
if (!mainw->fs && !prefs->hide_framebar && !mainw->is_rendering) {
lives_freep((void **)&framecount);
if (CURRENT_CLIP_HAS_VIDEO && cfile->frames != 123456789) {
framecount = lives_strdup_printf("%9d / %d", frame, cfile->frames);
} else {
framecount = lives_strdup_printf("%9d", frame);
}
lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
lives_widget_queue_draw(mainw->framecounter);
lives_freep((void **)&framecount);
}
if (mainw->toy_type != LIVES_TOY_NONE) {
// TODO - move into toys.c
if (mainw->toy_type == LIVES_TOY_MAD_FRAMES && !mainw->fs) {
if (cfile->opening_only_audio) {
load_end_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
load_start_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
} else {
load_end_image(1 + (int)((double)frame * rand() / (RAND_MAX + 1.0)));
load_start_image(1 + (int)((double)frame * rand() / (RAND_MAX + 1.0)));
}
}
}
} else {
if (mainw->is_rendering || mainw->is_generating) {
fname_next = make_image_file_name(cfile, frame + 1, get_image_ext_for_type(cfile->img_type));
} else {
if (!mainw->keep_pre) {
img_ext = LIVES_FILE_EXT_MGK;
} else {
img_ext = LIVES_FILE_EXT_PRE;
}
fname_next = make_image_file_name(cfile, frame + 1, img_ext);
}
}
mainw->actual_frame = frame;

// maybe the performance finished and we weren't looping
if ((mainw->actual_frame < 1 || mainw->actual_frame > cfile->frames) &&
CURRENT_CLIP_IS_NORMAL && (!mainw->is_rendering || mainw->preview)) {
goto lfi_done;
return;
}
}

// limit max frame size unless we are saving to disk or rendering
// frame_layer will in any case be equal to or smaller than this depending on maximum source frame size

if (!(mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_EFFECTS) &&
(!CURRENT_CLIP_IS_NORMAL || (IS_VALID_CLIP(mainw->blend_file) && !IS_NORMAL_CLIP(mainw->blend_file))))) {
get_player_size(&opwidth, &opheight);
}

////////////////////////////////////////////////////////////
// load a frame from disk buffer

if (mainw->preview && !mainw->frame_layer && (!mainw->event_list || cfile->opening)) {
info_file = lives_build_filename(prefs->workdir, cfile->handle, LIVES_STATUS_FILE_NAME, NULL);
}

do {
//wait_for_cleaner();
if (mainw->frame_layer) {
// free the old mainw->frame_layer
check_layer_ready(mainw->frame_layer); // ensure all threads are complete
weed_layer_free(mainw->frame_layer);
mainw->frame_layer = NULL;
}

if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
// here if we are rendering from multitrack, previewing a recording, or applying realtime effects to a selection
weed_timecode_t tc = mainw->cevent_tc;
if (mainw->scrap_file != -1 && mainw->clip_index[0] == mainw->scrap_file && mainw->num_tracks == 1) {
// do not apply fx, just pull frame
mainw->frame_layer = lives_layer_new_for_frame(mainw->clip_index[0], mainw->frame_index[0]);
pull_frame_threaded(mainw->frame_layer, NULL, (weed_timecode_t)mainw->currticks, 0, 0);
/* if (!pull_frame(mainw->frame_layer, get_image_ext_for_type(cfile->img_type), tc)) { */
/*   weed_plant_free(mainw->frame_layer); */
/*   mainw->frame_layer = NULL; */
/*   if (mainw->preview_rendering) { */
/*     mainw->cancelled = CANCEL_NO_MORE_PREVIEW; */
/*     goto lfi_done; */
/*   } */
/* } */
} else {
int oclip, nclip;
register int i;
weed_plant_t **layers = (weed_plant_t **)lives_calloc((mainw->num_tracks + 1), sizeof(weed_plant_t *));
// get list of active tracks from mainw->filter map
get_active_track_list(mainw->clip_index, mainw->num_tracks, mainw->filter_map);
for (i = 0; i < mainw->num_tracks; i++) {
oclip = mainw->old_active_track_list[i];
mainw->ext_src_used[oclip] = FALSE;
if (oclip > 0 && oclip == (nclip = mainw->active_track_list[i])) {
// check if ext_src survives old->new
if (mainw->track_decoders[i] == mainw->files[oclip]->ext_src) mainw->ext_src_used[oclip] = TRUE;
}
}

for (i = 0; i < mainw->num_tracks; i++) {
layers[i] = lives_layer_new_for_frame(mainw->clip_index[i], mainw->frame_index[i]);
weed_set_int_value(layers[i], WEED_LEAF_CURRENT_PALETTE, (mainw->clip_index[i] == -1 ||
mainw->files[mainw->clip_index[i]]->img_type ==
IMG_TYPE_JPEG) ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32);
if ((oclip = mainw->old_active_track_list[i]) != (nclip = mainw->active_track_list[i])) {
// now using threading, we want to start pulling all pixel_data for all active layers here
// however, we may have more than one copy of the same clip -
// in this case we want to create clones of the decoder plugin
// this is to prevent constant seeking between different frames in the clip
if (oclip > 0) {
if (mainw->files[oclip]->clip_type == CLIP_TYPE_FILE) {
if (mainw->track_decoders[i] != (lives_decoder_t *)mainw->files[oclip]->ext_src) {
// remove the clone for oclip
close_decoder_plugin(mainw->track_decoders[i]);
}
mainw->track_decoders[i] = NULL;
}
}

if (nclip > 0) {
if (mainw->files[nclip]->clip_type == CLIP_TYPE_FILE) {
if (!mainw->ext_src_used[nclip]) {
mainw->track_decoders[i] = (lives_decoder_t *)mainw->files[nclip]->ext_src;
mainw->ext_src_used[nclip] = TRUE;
} else {
// add new clone for nclip
mainw->track_decoders[i] = clone_decoder(nclip);
		    // *INDENT-OFF*
		  }}}}
	    // *INDENT-ON*

              mainw->old_active_track_list[i] = mainw->active_track_list[i];

              if (nclip > 0) {
                img_ext = get_image_ext_for_type(mainw->files[nclip]->img_type);
                // set alt src in layer
                weed_set_voidptr_value(layers[i], WEED_LEAF_HOST_DECODER, (void *)mainw->track_decoders[i]);
                pull_frame_threaded(layers[i], img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
              } else {
                weed_layer_pixel_data_free(layers[i]);
              }
            }
            layers[i] = NULL;

            mainw->frame_layer = weed_apply_effects(layers, mainw->filter_map, tc, opwidth, opheight, mainw->pchains);

            for (i = 0; layers[i]; i++) {
              if (layers[i] != mainw->frame_layer) {
                check_layer_ready(layers[i]);
                weed_layer_free(layers[i]);
              }
            }
            lives_free(layers);

            if (mainw->internal_messaging) {
              // this happens if we are calling from multitrack, or apply rte.  We get our mainw->frame_layer and exit.
              // DO NOT goto lfi_done, as that will free mainw->frame_layer.
              lives_freep((void **)&framecount);
              lives_freep((void **)&info_file);
              return;
            }
          }
        } else {
          if (prefs->dev_show_timing)
            g_printerr("pull_frame @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
          // normal playback in the clip editor, or applying a non-realtime effect
          if (cfile->clip_type != CLIP_TYPE_DISK
              || !mainw->preview || lives_file_test(fname_next, LIVES_FILE_TEST_EXISTS)) {
            mainw->frame_layer = lives_layer_new_for_frame(mainw->current_file, mainw->actual_frame);
            if (!img_ext) img_ext = get_image_ext_for_type(cfile->img_type);
            if (mainw->preview && !mainw->frame_layer
                && (!mainw->event_list || cfile->opening)) {
              if (!pull_frame_at_size(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks,
                                      cfile->hsize, cfile->vsize, WEED_PALETTE_END)) {
                if (mainw->frame_layer) {
                  weed_layer_free(mainw->frame_layer);
                  mainw->frame_layer = NULL;
                }

                if (cfile->clip_type == CLIP_TYPE_DISK &&
                    cfile->opening && cfile->img_type == IMG_TYPE_PNG
                    && sget_file_size(fname_next) <= 0) {
                  if (++bad_frame_count > BFC_LIMIT) {
                    mainw->cancelled = check_for_bad_ffmpeg();
                    bad_frame_count = 0;
                  } else lives_usleep(prefs->sleep_time);
                }
              }
            } else {
              boolean got_preload = FALSE;
              if (mainw->frame_layer_preload && mainw->pred_clip == mainw->playing_file
                  && mainw->pred_frame != 0 && is_layer_ready(mainw->frame_layer_preload)) {
                frames_t delta = (labs(mainw->pred_frame) - mainw->actual_frame) * sig(cfile->pb_fps);
                /* g_print("THANKS for %p,! %d %ld should be %d, right  --  %d",
                  // mainw->frame_layer_preload, mainw->pred_clip, */
                /*         mainw->pred_frame, mainw->actual_frame, delta); */
                if (delta <= 0 || (mainw->pred_frame < 0 && delta > 0)) {
                  check_layer_ready(mainw->frame_layer_preload);
                  if (weed_layer_get_pixel_data_packed(mainw->frame_layer_preload) != NULL) {
                    //g_print("YAH !\n");
                    got_preload = TRUE;
                    if (mainw->pred_frame < 0) {
                      if (mainw->frame_layer) weed_layer_free(mainw->frame_layer);
                      mainw->frame_layer = weed_layer_copy(NULL, mainw->frame_layer_preload);
                    } else {
                      weed_layer_copy(mainw->frame_layer, mainw->frame_layer_preload);
                      weed_layer_nullify_pixel_data(mainw->frame_layer_preload);
                    }
                  }
                }
                if (prefs->show_dev_opts) {
                  if (delta < 0) g_printerr("cached frame    TOO LATE, got %ld, wanted %d !!!\n",
                                              labs(mainw->pred_frame), mainw->actual_frame);
                }
                //if (delta > 0) g_print("    waiting...\n");
              }
              if (!got_preload) {
                pull_frame_threaded(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
                //pull_frame(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks);
              }
            }
          }

          if (prefs->dev_show_timing)
            g_printerr("pull_frame done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
          if ((!cfile->next_event && mainw->is_rendering && !mainw->switch_during_pb &&
               (!mainw->multitrack || (!mainw->multitrack->is_rendering && !mainw->is_generating))) ||
              ((!mainw->multitrack || (mainw->multitrack && mainw->multitrack->is_rendering)) &&
               mainw->preview && !mainw->frame_layer)) {
            // preview ended
            if (!cfile->opening) mainw->cancelled = CANCEL_NO_MORE_PREVIEW;
            if (mainw->cancelled) {
              lives_free(fname_next);
              lives_freep((void **)&info_file);
              goto lfi_done;
            }
            // ???
            mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
          }

          img_ext = NULL;

          if (mainw->internal_messaging) {
            // here we are rendering to an effect or timeline, need to keep mainw->frame_layer and return
            lives_freep((void **)&framecount);
            lives_freep((void **)&info_file);
            check_layer_ready(mainw->frame_layer);
            return;
          }

          if (!mainw->frame_layer && (!mainw->preview || (mainw->multitrack && !cfile->opening))) {
            lives_freep((void **)&info_file);
            goto lfi_done;
          }

          if (mainw->preview && !mainw->frame_layer && (!mainw->event_list || cfile->opening)) {
            FILE *fp;
            // non-realtime effect preview
            // check effect to see if it finished yet
            if ((fp = fopen(info_file, "r"))) {
              clear_mainw_msg();
              do {
                retval = 0;
                lives_fgets(mainw->msg, MAINW_MSG_SIZE, fp);
                if (THREADVAR(read_failed) && THREADVAR(read_failed) == fileno(fp) + 1) {
                  THREADVAR(read_failed) = 0;
                  retval = do_read_failed_error_s_with_retry(info_file, NULL);
                }
              } while (retval == LIVES_RESPONSE_RETRY);
              fclose(fp);
              if (!lives_strncmp(mainw->msg, "completed", 9) || !strncmp(mainw->msg, "error", 5)) {
                // effect completed whilst we were busy playing a preview
                if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play"));
                lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play"));
                if (cfile->opening && !cfile->is_loaded) {
                  if (mainw->toy_type == LIVES_TOY_TV) {
                    on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
                  }
                }
                mainw->preview = FALSE;
              } else {
                lives_usleep(prefs->sleep_time);
              }
            } else {
              lives_usleep(prefs->sleep_time);
            }

            // or we reached the end of the preview
            if ((!cfile->opening && frame >= (mainw->proc_ptr->frames_done - cfile->progress_start + cfile->start)) ||
                (cfile->opening && (mainw->toy_type == LIVES_TOY_TV || !mainw->preview || mainw->effects_paused))) {
              if (mainw->toy_type == LIVES_TOY_TV) {
                // force a loop (set mainw->cancelled to CANCEL_KEEP_LOOPING to play selection again)
                mainw->cancelled = CANCEL_KEEP_LOOPING;
              } else mainw->cancelled = CANCEL_NO_MORE_PREVIEW;
              lives_free(fname_next);
              // end of playback, so this is no longer needed
              lives_freep((void **)&info_file);
              goto lfi_done;
            } else if (mainw->preview || cfile->opening) lives_widget_context_update();
          }
        }
      } while (!mainw->frame_layer && mainw->cancelled == CANCEL_NONE && cfile->clip_type == CLIP_TYPE_DISK);

      lives_freep((void **)&info_file);

      if (LIVES_UNLIKELY((!mainw->frame_layer) || mainw->cancelled > 0)) {
        // NULL frame or user cancelled
        if (mainw->frame_layer) {
          check_layer_ready(mainw->frame_layer);
          weed_layer_free(mainw->frame_layer);
          mainw->frame_layer = NULL;
        }
        goto lfi_done;
      }

      if (was_preview) {
        lives_free(fname_next);
      }

      // OK. Here is the deal now. We have a layer from the current file, current frame.
      // (or at least we sent out a thread to fetch it).
      // We will pass this into the effects, and we will get back a layer.
      // The palette of the effected layer could be any Weed palette.
      // We will pass the layer to all playback plugins.
      // Finally we may want to end up with a GkdPixbuf (unless the playback plugin is VPP_DISPLAY_LOCAL
      // and we are in full screen mode).

      if (!prefs->vj_mode && (mainw->current_file != mainw->scrap_file || mainw->multitrack)
          && mainw->pwidth > 0 && mainw->pheight > 0
          && !(mainw->is_rendering && !(mainw->proc_ptr && mainw->preview))
          && !cfile->opening && !mainw->resizing && CURRENT_CLIP_IS_NORMAL
          && !is_virtual_frame(mainw->current_file, mainw->actual_frame)
          && is_layer_ready(mainw->frame_layer)) {
        // if we are pulling the frame from an image and playing back normally, check the size is what it should be
        // this used to cause problems with some effects, but that may no longer be the case with the layers model
        int wl = weed_layer_get_width(mainw->frame_layer) *
                 weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(mainw->frame_layer));
        int hl = weed_layer_get_height(mainw->frame_layer);
        if ((wl != cfile->hsize && wl != mainw->pwidth)
            || (hl != cfile->vsize && hl != mainw->pheight)) {
          break_me("bad frame size");
          mainw->size_warn = mainw->current_file;
          size_ok = FALSE;
        }
      }

      if (size_ok) {
        // if frame size is OK we apply real time effects
        if ((mainw->rte != 0 || (mainw->is_rendering && !mainw->event_list))
            && (mainw->current_file != mainw->scrap_file || mainw->multitrack)) {
          int lb_width = opwidth;
          int lb_height = opheight;
          if (prefs->pb_quality == PB_QUALITY_HIGH
              && ((mainw->multitrack && !mainw->multitrack->is_rendering && prefs->letterbox_mt)
                  || (!mainw->multitrack && prefs->letterbox
                      && (!mainw->is_rendering || mainw->preview_rendering)))) {
            /// if letterboxing we adjust the player size to the inner rectangle.
            // This avoids problems with varying quality levels
            boolean can_resize = FALSE;
            int pwidth = opwidth;
            int pheight = opheight;
            lb_width = mainw->files[mainw->playing_file]->hsize;
            lb_height = mainw->files[mainw->playing_file]->vsize;
            if (mainw->ext_playback && (mainw->vpp->capabilities & VPP_CAN_RESIZE) != 0) can_resize = TRUE;
            get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, can_resize);
          }
          mainw->frame_layer = on_rte_apply(mainw->frame_layer, lb_width, lb_height, (weed_timecode_t)mainw->currticks);
        }
      }

      if (prefs->dev_show_timing)
        g_printerr("rte done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      ////////////////////////

      // save to scrap_file now if we have to
      if (mainw->record && !mainw->record_paused && mainw->scrap_file != -1 && fg_file == mainw->scrap_file) {
        if (!rec_after_pb) {
          check_layer_ready(mainw->frame_layer);
          save_to_scrap_file(mainw->frame_layer);
        }
        get_player_size(&opwidth, &opheight);
      }

      if (mainw->ext_playback && (mainw->vpp->capabilities & VPP_CAN_RESIZE)
          && ((((!prefs->letterbox && !mainw->multitrack) || (mainw->multitrack && !prefs->letterbox_mt)))
              || (mainw->vpp->capabilities & VPP_CAN_LETTERBOX))) {
        // here we are outputing video through a video playback plugin which can resize: thus we just send whatever we have
        // we need only to convert the palette to whatever was agreed with the plugin when we called set_palette()
        // in plugins.c
        //
        // if we want letterboxing we do this ourselves, later in the code

        weed_plant_t *frame_layer = NULL;
        weed_plant_t *return_layer = NULL;
        int lwidth, lheight;
        int ovpppalette = mainw->vpp->palette;

        /// check if function exists - it accepts rowstrides
        if (mainw->vpp->play_frame) player_v2 = TRUE;

        //g_print("clr1 start  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        check_layer_ready(mainw->frame_layer);
        mainw->video_seek_ready = TRUE;
        //g_print("clr1 done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        layer_palette = weed_layer_get_palette(mainw->frame_layer);
        if (!weed_palette_is_valid(layer_palette)) {
          goto lfi_done;
        }

        if ((mainw->vpp->capabilities & VPP_CAN_CHANGE_PALETTE)
            && mainw->vpp->palette != layer_palette) vpp_try_match_palette(mainw->vpp, mainw->frame_layer);

        if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
            ((weed_palette_is_rgb(layer_palette) &&
              !(weed_palette_is_rgb(mainw->vpp->palette))) ||
             (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
          // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
          // so copy layer and convert, retaining original
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        } else frame_layer = mainw->frame_layer;

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
          if (!check_for_overlay_text(frame_layer)) {
            if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
              frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring);
            }
          }
        }

        if (prefs->apply_gamma) {
          // gamma correction
          if (weed_palette_is_rgb(mainw->vpp->palette)) {
            if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
              tgamma = WEED_GAMMA_LINEAR;
            else {
              tgamma = WEED_GAMMA_SRGB;
            }
          }
        }

        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                        mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgamma)) {
          goto lfi_done;
        }
        if (prefs->dev_show_timing)
          g_print("cl palette done %d to %d @ %f\n", weed_layer_get_palette(frame_layer), mainw->vpp->palette,
                  lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        if (!player_v2) {
          // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
          if (!compact_rowstrides(frame_layer)) {
            goto lfi_done;
          }
        }
        if (prefs->dev_show_timing)
          g_print("comp rs done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        if (mainw->stream_ticks == -1) mainw->stream_ticks = mainw->currticks;

        if (rec_after_pb) {
          // record output from playback plugin

          int retwidth = mainw->pwidth / weed_palette_get_pixels_per_macropixel(mainw->vpp->palette);
          int retheight = mainw->pheight;

          return_layer = weed_layer_create(retwidth, retheight, NULL, ovpppalette);

          if (weed_palette_is_yuv(ovpppalette)) {
            weed_set_int_value(return_layer, WEED_LEAF_YUV_CLAMPING, mainw->vpp->YUV_clamping);
            weed_set_int_value(return_layer, WEED_LEAF_YUV_SUBSPACE, mainw->vpp->YUV_subspace);
            weed_set_int_value(return_layer, WEED_LEAF_YUV_SAMPLING, mainw->vpp->YUV_sampling);
          }

          // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          if (create_empty_pixel_data(return_layer, FALSE, TRUE))
            retdata = weed_layer_get_pixel_data(return_layer, NULL);
          else return_layer = NULL;
        }

        // chain any data to the playback plugin
        if (!(mainw->preview || mainw->is_rendering)) {
          // chain any data pipelines
          if (mainw->pconx) {
            pconx_chain_data(FX_DATA_KEY_PLAYBACK_PLUGIN, 0, FALSE);
          }
          if (mainw->cconx) cconx_chain_data(FX_DATA_KEY_PLAYBACK_PLUGIN, 0);
        }

        if (prefs->apply_gamma) {
          // gamma correction
          gamma_convert_layer(tgamma, frame_layer);
        }
        if (prefs->dev_show_timing)
          g_print("gamma conv done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        if (return_layer) weed_leaf_dup(return_layer, frame_layer, WEED_LEAF_GAMMA_TYPE);
        pd_array = weed_layer_get_pixel_data(frame_layer, NULL);
        if (prefs->pb_quality == PB_QUALITY_LOW) {
          lb_width = cfile->hsize;
          lb_height = cfile->vsize;
        } else {
          lb_width = lwidth = weed_layer_get_width(frame_layer);
          lb_height = lheight = weed_layer_get_height(frame_layer);
        }
        pwidth = mainw->pwidth;
        pheight = mainw->pheight;
        if (player_v2) {
          if ((!mainw->multitrack && prefs->letterbox) || (mainw->multitrack && prefs->letterbox_mt)) {
            // pretend it cant resize, just to get the offsets
            get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, FALSE);
            weed_set_double_value(frame_layer, "x_range", (double)lb_width / (double)pwidth);
            weed_set_double_value(frame_layer, "y_range", (double)lb_height / (double)pheight);
          } else {
            weed_set_double_value(frame_layer, "x_range", 1.0);
            weed_set_double_value(frame_layer, "y_range", 1.0);
          }
        }

        if (!avsync_check()) goto lfi_done;

        /* int rs = weed_layer_get_rowstride(frame_layer); */
        /* weed_layer_set_rowstride(frame_layer, rs - 2); */
        /* g_print("RS is %d\n", weed_layer_get_rowstride(frame_layer)); */

        lwidth = weed_layer_get_width(frame_layer);
        if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
          // TODO - do conversion before letterboxing
          gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
        }
        if ((player_v2 && !(*mainw->vpp->play_frame)(frame_layer, mainw->currticks - mainw->stream_ticks, return_layer))
            || (!player_v2 && !(*mainw->vpp->render_frame)(lwidth, weed_layer_get_height(frame_layer),
                mainw->currticks - mainw->stream_ticks, pd_array, retdata, mainw->vpp->play_params))) {
          //vid_playback_plugin_exit();
          if (return_layer) {
            weed_layer_free(return_layer);
            lives_free(retdata);
            return_layer = NULL;
          }
        } else success = TRUE;
        if (prefs->dev_show_timing)
          g_printerr("rend fr done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        lives_free(pd_array);
        if (frame_layer != mainw->frame_layer) {
          weed_layer_free(frame_layer);
        }

        if (return_layer) {
          int width = MIN(weed_layer_get_width(mainw->frame_layer)
                          / weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(mainw->frame_layer)),
                          weed_layer_get_width(return_layer)
                          / weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(return_layer)));
          int height = MIN(weed_layer_get_height(mainw->frame_layer), weed_layer_get_height(return_layer));
          if (!resize_layer(return_layer, width, height, LIVES_INTERP_FAST, WEED_PALETTE_END, 0)) {
            if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
              /// TODO - save w. screen_gamma
              gamma_convert_layer(WEED_GAMMA_SRGB, return_layer);
            }
            save_to_scrap_file(return_layer);
          }
          weed_layer_free(return_layer);
          lives_free(retdata);
          return_layer = NULL;
        }

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
      }

      get_player_size(&mainw->pwidth, &mainw->pheight);
      if (prefs->dev_show_timing)
        g_printerr("ext start  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (mainw->ext_playback && (!(mainw->vpp->capabilities & VPP_CAN_RESIZE)
                                  || (((!mainw->multitrack && prefs->letterbox) || (mainw->multitrack && prefs->letterbox_mt))
                                      & !(mainw->vpp->capabilities & VPP_CAN_LETTERBOX)))) {
        // here we are either: playing through an external video playback plugin which cannot resize
        // - we must resize to whatever width and height we set when we called init_screen() in the plugin
        // i.e. mainw->vpp->fwidth, mainw->vpp fheight

        // both dimensions are in RGB(A) pixels, so we must adjust here and send the correct
        // macropixel size in the plugin's render_frame() (in case of exotic palettes)

        // - this is also used if we are letterboxing with an external plugin

        weed_plant_t *frame_layer = NULL;
        weed_plant_t *return_layer = NULL;
        int ovpppalette = mainw->vpp->palette;
        boolean needs_lb = FALSE;

        /// check if function exists - it accepts rowstrides
        if (mainw->vpp->play_frame) player_v2 = TRUE;

        check_layer_ready(mainw->frame_layer);
        mainw->video_seek_ready = TRUE;
        if (prefs->dev_show_timing)
          g_printerr("clr2  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        layer_palette = weed_layer_get_palette(mainw->frame_layer);
        if (!weed_palette_is_valid(layer_palette)) goto lfi_done;

        if ((mainw->vpp->capabilities & VPP_CAN_CHANGE_PALETTE)
            && mainw->vpp->palette != layer_palette) vpp_try_match_palette(mainw->vpp, mainw->frame_layer);
        interp = get_interp_value(prefs->pb_quality, TRUE);

        if (mainw->fs && (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)) {
          mainw->vpp->fwidth = mainw->pwidth;
          mainw->vpp->fheight = mainw->pheight;
        }

        if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) && !(mainw->vpp->capabilities & VPP_CAN_RESIZE) &&
            ((mainw->vpp->fwidth  < mainw->pwidth || mainw->vpp->fheight < mainw->pheight))) {
          // mainw->frame_layer will be downsized for the plugin but upsized for screen
          // so copy layer and convert, retaining original
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        } else frame_layer = mainw->frame_layer;

        if (frame_layer == mainw->frame_layer && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
            ((weed_palette_is_rgb(layer_palette) &&
              !(weed_palette_is_rgb(mainw->vpp->palette))) ||
             (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
          // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
          // so copy layer and convert, retaining original
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        }
        if (prefs->dev_show_timing)
          g_printerr("copied  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        pwidth = mainw->vpp->fwidth;
        pheight = mainw->vpp->fheight;

        if ((mainw->multitrack && prefs->letterbox_mt) || (!mainw->multitrack && prefs->letterbox)) {
          //&&
          /// letterbox external
          lb_width = weed_layer_get_width(mainw->frame_layer) *
                     weed_palette_get_pixels_per_macropixel(layer_palette);
          lb_height = weed_layer_get_height(mainw->frame_layer);
          get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, (mainw->vpp->capabilities & VPP_CAN_RESIZE) != 0);
          if (pwidth != lb_width || pheight != lb_height) {
            needs_lb = TRUE;
            if (!(mainw->vpp->capabilities & VPP_CAN_LETTERBOX)) {
              if (frame_layer == mainw->frame_layer) {
                if (layer_palette != mainw->vpp->palette && (pwidth > lb_width || pheight > lb_height)) {
                  frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
                  if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
                  if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                                  mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgamma)) {
                    goto lfi_done;
                  }
                } else {
                  frame_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
                  weed_layer_copy(frame_layer, mainw->frame_layer);
                }
              }
              if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
              if (!letterbox_layer(frame_layer, pwidth, pheight, lb_width, lb_height, interp,
                                   mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
              was_letterboxed = TRUE;
	    // *INDENT-OFF*
	  } else {
	    weed_set_double_value(frame_layer, "x_range", (double)lb_width / (double)pwidth);
	    weed_set_double_value(frame_layer, "y_range", (double)lb_height / (double)pheight);
	  }
	}
      }
      // *INDENT-ON*

        if (prefs->dev_show_timing)
          g_printerr("lbb  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        layer_palette = weed_layer_get_palette(frame_layer);

        if ((((weed_layer_get_width(frame_layer) *
               weed_palette_get_pixels_per_macropixel(layer_palette)) ^ pwidth) >> 2) ||
            ((weed_layer_get_height(frame_layer) ^ pheight) >> 1)) {
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          if (!needs_lb  || was_letterboxed) {
            lb_width = pwidth;
            lb_height = pheight;
          }
          if (!resize_layer(frame_layer, lb_width, lb_height, interp,
                            mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
        }
        if (prefs->dev_show_timing)
          g_printerr("resize done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        // resize_layer can change palette

        if (frame_layer == mainw->frame_layer && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
            ((weed_palette_is_rgb(layer_palette) &&
              !(weed_palette_is_rgb(mainw->vpp->palette))) ||
             (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
          // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
          // so copy layer and convert, retaining original
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        }

        layer_palette = weed_layer_get_palette(frame_layer);

        pwidth = weed_layer_get_width(frame_layer) * weed_palette_get_pixels_per_macropixel(layer_palette);
        pheight = weed_layer_get_height(frame_layer);

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
          if (!check_for_overlay_text(frame_layer)) {
            if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
              frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring);
            }
          }
        }

        if (prefs->apply_gamma) {
          // gamma correction
          if (weed_palette_is_rgb(mainw->vpp->palette)) {
            if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
              tgamma = WEED_GAMMA_LINEAR;
            else {
              tgamma = WEED_GAMMA_SRGB;
            }
          }
        }
        //g_print("clp start %d %d   %d %d @\n", weed_layer_get_palette(frame_layer),
        //mainw->vpp->palette, weed_layer_get_gamma(frame_layer), tgamma);
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                        mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgamma)) {
          goto lfi_done;
        }

        if (prefs->dev_show_timing)
          g_printerr("clp done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        if (mainw->stream_ticks == -1) mainw->stream_ticks = mainw->currticks;

        if (!player_v2) {
          // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
          if (!compact_rowstrides(frame_layer)) goto lfi_done;
          if (prefs->dev_show_timing)
            g_printerr("c rows done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        }
        if (rec_after_pb) {
          // record output from playback plugin
          int retwidth = mainw->vpp->fwidth;
          int retheight = mainw->vpp->fheight;

          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
          return_layer = weed_layer_create(retwidth, retheight, NULL, ovpppalette);

          if (weed_palette_is_yuv(mainw->vpp->palette)) {
            weed_layer_set_yuv_clamping(return_layer, mainw->vpp->YUV_clamping);
            weed_layer_set_yuv_sampling(return_layer, mainw->vpp->YUV_sampling);
            weed_layer_set_yuv_subspace(return_layer, mainw->vpp->YUV_subspace);
          }

          if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1 ; /// special value to compact the rowstrides
          if (create_empty_pixel_data(return_layer, FALSE, TRUE)) {
            retdata = weed_layer_get_pixel_data(return_layer, NULL);
          } else return_layer = NULL;
        }

        // chain any data to the playback plugin
        if (!(mainw->preview || mainw->is_rendering)) {
          // chain any data pipelines
          if (mainw->pconx) {
            pconx_chain_data(-2, 0, FALSE);
          }
          if (mainw->cconx) cconx_chain_data(-2, 0);
        }

        if (tgamma != WEED_GAMMA_UNKNOWN) {
          if (!was_letterboxed) {
            gamma_convert_layer(tgamma, frame_layer);
          } else {
            gamma_convert_sub_layer(tgamma, 1.0, frame_layer, (pwidth - lb_width) / 2, (pheight - lb_height) / 2,
                                    lb_width, lb_height, TRUE);
          }
        }

        if (return_layer) weed_layer_set_gamma(return_layer, weed_layer_get_gamma(frame_layer));

        if (!avsync_check()) goto lfi_done;

        pd_array = weed_layer_get_pixel_data(frame_layer, NULL);

        if (player_v2) {
          weed_set_double_value(frame_layer, "x_range", 1.0);
          weed_set_double_value(frame_layer, "y_range", 1.0);
        }

        if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
          // TODO - do conversion before letterboxing
          gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
        }

        if ((player_v2 && !(*mainw->vpp->play_frame)(frame_layer,
             mainw->currticks - mainw->stream_ticks, return_layer))
            || (!player_v2 && !(*mainw->vpp->render_frame)(weed_layer_get_width(frame_layer),
                weed_layer_get_height(frame_layer),
                mainw->currticks - mainw->stream_ticks, pd_array, retdata,
                mainw->vpp->play_params))) {
          //vid_playback_plugin_exit();
          if (return_layer) {
            weed_layer_free(return_layer);
            lives_free(retdata);
            return_layer = NULL;
          }
          goto lfi_done;
        } else success = TRUE;
        lives_free(pd_array);
        if (prefs->dev_show_timing)
          g_printerr("rend done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        if (return_layer) {
          int width = MIN(weed_layer_get_width(mainw->frame_layer)
                          * weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(mainw->frame_layer)),
                          weed_layer_get_width(return_layer)
                          * weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(return_layer)));
          int height = MIN(weed_layer_get_height(mainw->frame_layer), weed_layer_get_height(return_layer));
          if (resize_layer(return_layer, width, height, LIVES_INTERP_FAST, WEED_PALETTE_END, 0)) {
            if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
              // TODO - save w. screen_gamma
              gamma_convert_layer(WEED_GAMMA_SRGB, frame_layer);
            }
            save_to_scrap_file(return_layer);
          }
          weed_layer_free(return_layer);
          lives_free(retdata);
          return_layer = NULL;
        }

        if (frame_layer != mainw->frame_layer) {
          weed_layer_free(frame_layer);
        }

        // frame display was handled by a playback plugin, skip the rest
        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
      }

      ////////////////////////////////////////////////////////
      // local display - either we are playing with no playback plugin, or else the playback plugin has no
      // local display of its own
      if (prefs->dev_show_timing)
        g_printerr("clr @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      check_layer_ready(mainw->frame_layer); // wait for all threads to complete
      mainw->video_seek_ready = TRUE;
      if (prefs->dev_show_timing)
        g_printerr("clr end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if (weed_layer_get_width(mainw->frame_layer) == 0) return;
      if ((mainw->sep_win && !prefs->show_playwin) || (!mainw->sep_win && !prefs->show_gui)) {
        // no display to output, skip the rest
        success = TRUE;
        goto lfi_done;
      }

      if (mainw->ext_playback && !player_v2) THREADVAR(rowstride_alignment_hint) = -1 ; /// special value to compact the rowstrides
      layer_palette = weed_layer_get_palette(mainw->frame_layer);
      if (!weed_palette_is_valid(layer_palette) || !CURRENT_CLIP_IS_VALID) goto lfi_done;

      if (cfile->img_type == IMG_TYPE_JPEG || !weed_palette_has_alpha(layer_palette)) cpal = WEED_PALETTE_RGB24;
      else {
        cpal = WEED_PALETTE_RGBA32;
      }
      if (mainw->fs && !mainw->ext_playback && (!mainw->multitrack || mainw->sep_win)) {
        // set again, in case vpp was turned off because of preview conditions
        get_player_size(&mainw->pwidth, &mainw->pheight);
      }

      interp = get_interp_value(prefs->pb_quality, TRUE);
      pwidth = opwidth;
      pheight = opheight;

      if (prefs->dev_show_timing)
        g_printerr("res start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if ((0 && mainw->multitrack && prefs->letterbox_mt) || (!mainw->multitrack && prefs->letterbox)) {
        /// letterbox internal
        lb_width = weed_layer_get_width(mainw->frame_layer) *
                   weed_palette_get_pixels_per_macropixel(layer_palette);
        lb_height = weed_layer_get_height(mainw->frame_layer);
        get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, FALSE);
        if (!letterbox_layer(mainw->frame_layer, pwidth, pheight, lb_width, lb_height, interp, cpal, 0)) goto lfi_done;
        was_letterboxed = TRUE;
        layer_palette = weed_layer_get_palette(mainw->frame_layer);
      }
      if (weed_layer_get_width(mainw->frame_layer) *
          weed_palette_get_pixels_per_macropixel(layer_palette) != pwidth ||
          weed_layer_get_height(mainw->frame_layer) != pheight) {
        if (!resize_layer(mainw->frame_layer, pwidth, pheight, interp, cpal, 0)) goto lfi_done;
      }
      if (prefs->dev_show_timing)
        g_printerr("res end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (!convert_layer_palette_full(mainw->frame_layer, cpal, 0, 0, 0, WEED_GAMMA_SRGB)) goto lfi_done;

      if (prefs->dev_show_timing)
        g_printerr("clp end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if (LIVES_IS_PLAYING) {
        if (!check_for_overlay_text(mainw->frame_layer)) {
          if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
            mainw->frame_layer = render_text_overlay(mainw->frame_layer, mainw->multitrack->timestring);
          }
        }
      }

      /* if (1 || !was_letterboxed) { */
      /*   if (!prefs->gamma_srgb) */
      /*     gamma_convert_layer(WEED_GAMMA_MONITOR, mainw->frame_layer); */
      /*   else */
      /*     gamma_convert_layer(WEED_GAMMA_SRGB, mainw->frame_layer); */
      /* } else { */
      /*   if (!prefs->gamma_srgb) */
      /*     gamma_convert_sub_layer(WEED_GAMMA_MONITOR, mainw->frame_layer, (pwidth - lb_width) / 2, (pheight - lb_height / 2), */
      /*                             lb_width, lb_height); */
      /*   else */
      /*     gamma_convert_sub_layer(WEED_GAMMA_SRGB, mainw->frame_layer, (pwidth - lb_width) / 2, (pheight - lb_height / 2), */
      /*                             lb_width, lb_height); */
      /* } */

      if (prefs->dev_show_timing)
        g_printerr("l2p start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      pixbuf = layer_to_pixbuf(mainw->frame_layer, TRUE, TRUE);

      if (prefs->dev_show_timing)
        g_printerr("l2p @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (!avsync_check()) goto lfi_done;

      // internal player, double size or fullscreen, or multitrack

      if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
        set_drawing_area_from_pixbuf(mainw->preview_image, pixbuf, mainw->pi_surface);
        lives_widget_queue_draw(mainw->preview_image);
      } else {
        pwidth = lives_widget_get_allocation_width(mainw->play_image);
        pheight = lives_widget_get_allocation_height(mainw->play_image);
        if (pwidth < old_pwidth || pheight < old_pheight)
          clear_widget_bg(mainw->play_image, mainw->play_surface);
        old_pwidth = pwidth;
        old_pheight = pheight;
        set_drawing_area_from_pixbuf(mainw->play_image, pixbuf, mainw->play_surface);
        lives_widget_queue_draw(mainw->play_image);
      }

      if (pixbuf) lives_widget_object_unref(pixbuf);
      success = TRUE;
      if (prefs->dev_show_timing)
        g_print("paint @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      goto lfi_done;
    }

    // record external window
    if (mainw->record_foreign) {
      char fname[PATH_MAX];
      int xwidth, xheight;
      LiVESError *gerror = NULL;
      lives_painter_t *cr = lives_painter_create_from_surface(mainw->play_surface);

      if (!cr) return;

      if (mainw->rec_vid_frames == -1) {
        lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), (tmp = lives_strdup_printf("%9d", frame)));
        lives_widget_queue_draw(mainw->framecounter);
      } else {
        if (frame > mainw->rec_vid_frames) {
          mainw->cancelled = CANCEL_KEEP;
          if (CURRENT_CLIP_HAS_VIDEO) cfile->frames = mainw->rec_vid_frames;
          return;
        }

        lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), (tmp = lives_strdup_printf("%9d / %9d",
                             frame, mainw->rec_vid_frames)));
        lives_widget_queue_draw(mainw->framecounter);
        lives_free(tmp);
      }

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
      xwidth = gdk_window_get_width(mainw->foreign_window);
      xheight = gdk_window_get_height(mainw->foreign_window);
      if ((pixbuf = gdk_pixbuf_get_from_window(mainw->foreign_window, 0, 0, xwidth, xheight))) {
#else
      gdk_window_get_size(mainw->foreign_window, &xwidth, &xheight);
      if ((pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(mainw->foreign_window),
                    mainw->foreign_cmap, 0, 0, 0, 0, xwidth, xheight))) {
#endif
#endif
#ifdef GUI_QT
        xwidth = mainw->foreign_window->size().width();
        xheight = mainw->foreign_window->size().height();
        QScreen *qscreen = mainw->foreign_window->screen();
        QPixmap qp = qscreen->grabWindow(mainw->foreign_id, 0, 0, xwidth, xheight);
        if (0) { // TODO
#endif
          tmp = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
          lives_snprintf(fname, PATH_MAX, "%s", tmp);
          lives_free(tmp);

          do {
            // TODO ***: add a timeout here
            if (gerror) lives_error_free(gerror);
            lives_pixbuf_save(pixbuf, fname, cfile->img_type, 100, cfile->hsize, cfile->vsize, &gerror);
          } while (gerror);

          lives_painter_set_source_pixbuf(cr, pixbuf, 0, 0);
          lives_painter_paint(cr);
          lives_painter_destroy(cr);

          if (pixbuf) lives_widget_object_unref(pixbuf);
          cfile->frames = frame;
        } else {
          widget_opts.non_modal = TRUE;
          do_error_dialog(_("LiVES was unable to capture this image\n\n"));
          widget_opts.non_modal = FALSE;
          mainw->cancelled = CANCEL_CAPTURE_ERROR;
        }
        if (frame == mainw->rec_vid_frames) mainw->cancelled = CANCEL_KEEP;
      }

      lives_freep((void **)&framecount);
      return;

lfi_done:
      // here is where we free the mainw->frame_layer (the output video "frame" we just worked with)
      // we also animate the timeline and frame counters
      // if success is TRUE we may send an OSC FRAME_SYNCH notification

      /* if (cleaner) { */
      /* 	lives_nanosleep_until_nonzero(weed_get_boolean_value(cleaner, WEED_LEAF_DONE, NULL)); */
      /* 	weed_plant_free(cleaner); */
      /* 	cleaner = NULL; */
      /* } */

      do_cleanup(mainw->frame_layer, success);
      mainw->frame_layer = NULL;
      THREADVAR(rowstride_alignment_hint) = 0;
      lives_freep((void **)&framecount);
      if (success) {
        if (!mainw->multitrack &&
            !mainw->faded && (!mainw->fs || (prefs->gui_monitor != prefs->play_monitor
                                             && prefs->play_monitor != 0 && capable->nmonitors > 1))
            && mainw->current_file != mainw->scrap_file) {
          double ptrtime = ((double)mainw->actual_frame - 1.) / cfile->fps;
          mainw->ptrtime = ptrtime;
          lives_widget_queue_draw(mainw->eventbox2);
        }
        if (mainw->multitrack && !cfile->opening) animate_multitrack(mainw->multitrack);
      }
    }


    /**
       @brief Save a pixbuf to a file using the specified imgtype and the specified quality/compression value
    */
    boolean lives_pixbuf_save(LiVESPixbuf * pixbuf, char *fname, lives_img_type_t imgtype, int quality, int width, int height,
                              LiVESError **gerrorptr) {
      ticks_t timeout;
      lives_alarm_t alarm_handle;
      boolean retval = TRUE;
      int fd;

      // CALLER should check for errors
      // fname should be in local charset

      if (!LIVES_IS_PIXBUF(pixbuf)) {
        /// invalid pixbuf, we will save a blank image
        const char *img_ext = get_image_ext_for_type(imgtype);
        weed_layer_t  *layer = create_blank_layer(NULL, img_ext, width, height, WEED_PALETTE_END);
        pixbuf = layer_to_pixbuf(layer, TRUE, FALSE);
        weed_layer_free(layer);
        retval = FALSE;
      }

      fd = lives_open3(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
      alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
      while (flock(fd, LOCK_EX) && (timeout = lives_alarm_check(alarm_handle)) > 0) {
        lives_nanosleep(1000);
      }
      lives_alarm_clear(alarm_handle);
      if (timeout == 0) return FALSE;

      if (imgtype == IMG_TYPE_JPEG) {
        char *qstr = lives_strdup_printf("%d", quality);
#ifdef GUI_GTK
        gdk_pixbuf_save(pixbuf, fname, LIVES_IMAGE_TYPE_JPEG, gerrorptr, "quality", qstr, NULL);
#endif
#ifdef GUI_QT
        qt_jpeg_save(pixbuf, fname, gerrorptr, quality);
#endif
        lives_free(qstr);
      } else if (imgtype == IMG_TYPE_PNG) {
        char *cstr = lives_strdup_printf("%d", (int)((100. - (double)quality + 5.) / 10.));
        if (LIVES_IS_PIXBUF(pixbuf)) {
#ifdef GUI_GTK
          gdk_pixbuf_save(pixbuf, fname, LIVES_IMAGE_TYPE_PNG, gerrorptr, "compression", cstr, NULL);
#endif
#ifdef GUI_QT
          qt_png_save(pixbuf, fname, gerrorptr, (int)((100. - (double)quality + 5.) / 10.));
#endif
        } else retval = FALSE;
        lives_free(cstr);
      } else {
        //gdk_pixbuf_save_to_callback(...);
      }

      close(fd);
      if (*gerrorptr) return FALSE;
      return retval;
    }

    /**
       @brief save frame to pixbuf in a thread.
       The renderer uses this now so that it can be saving the current output frame
       at the same time as it prepares the following frame
    */
    void  *lives_pixbuf_save_threaded(void *args) {
      savethread_priv_t *saveargs = (savethread_priv_t *)args;
      lives_pixbuf_save(saveargs->pixbuf, saveargs->fname, saveargs->img_type, saveargs->compression, saveargs->width,
                        saveargs->height, &saveargs->error);
      return saveargs;
    }


    void close_current_file(int file_to_switch_to) {
      // close the current file, and free the file struct and all sub storage
      LiVESList *list_index;
      char *com;
      boolean need_new_blend_file = FALSE;
      int index = -1;
      int old_file = mainw->current_file;

      //update the bar text
      if (CURRENT_CLIP_IS_VALID) {
        int i;
        if (cfile->clip_type == CLIP_TYPE_TEMP) {
          close_temp_handle(file_to_switch_to);
          return;
        }
        if (cfile->clip_type != CLIP_TYPE_GENERATOR && mainw->current_file != mainw->scrap_file &&
            mainw->current_file != mainw->ascrap_file && mainw->current_file != 0 &&
            (!mainw->multitrack || mainw->current_file != mainw->multitrack->render_file)) {
          d_print(_("Closed clip %s\n"), cfile->file_name);
          lives_notify(LIVES_OSC_NOTIFY_CLIP_CLOSED, "");
        }

        cfile->hsize = mainw->def_width;
        cfile->vsize = mainw->def_height;

        if (cfile->laudio_drawable) {
          if (mainw->laudio_drawable == cfile->laudio_drawable
              || mainw->drawsrc == mainw->current_file) mainw->laudio_drawable = NULL;
          if (!mainw->multitrack) {
            if (cairo_surface_get_reference_count(cfile->laudio_drawable))
              lives_painter_surface_destroy(cfile->laudio_drawable);
          }
          cfile->laudio_drawable = NULL;
        }
        if (cfile->raudio_drawable) {
          if (mainw->raudio_drawable == cfile->raudio_drawable
              || mainw->drawsrc == mainw->current_file) mainw->raudio_drawable = NULL;
          if (!mainw->multitrack) {
            if (cairo_surface_get_reference_count(cfile->raudio_drawable))
              lives_painter_surface_destroy(cfile->raudio_drawable);
          }
          cfile->raudio_drawable = NULL;
        }
        if (mainw->drawsrc == mainw->current_file) mainw->drawsrc = -1;

        if (mainw->st_fcache) {
          if (mainw->en_fcache == mainw->st_fcache) mainw->en_fcache = NULL;
          if (mainw->pr_fcache == mainw->st_fcache) mainw->pr_fcache = NULL;
          weed_layer_free(mainw->st_fcache);
          mainw->st_fcache = NULL;
        }
        if (mainw->en_fcache) {
          if (mainw->pr_fcache == mainw->en_fcache) mainw->pr_fcache = NULL;
          weed_layer_free(mainw->en_fcache);
          mainw->en_fcache = NULL;
        }
        if (mainw->pr_fcache) {
          weed_layer_free(mainw->pr_fcache);
          mainw->pr_fcache = NULL;
        }

        for (i = 0; i < FN_KEYS - 1; i++) {
          if (mainw->clipstore[i][0] == mainw->current_file) mainw->clipstore[i][0] = -1;
        }

        // this must all be done last...
        if (cfile->menuentry) {
          // c.f. on_prevclip_activate
          list_index = lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
          do {
            if (!(list_index = lives_list_previous(list_index))) list_index = lives_list_last(mainw->cliplist);
            index = LIVES_POINTER_TO_INT(lives_list_nth_data(list_index, 0));
          } while ((mainw->files[index] || mainw->files[index]->opening || mainw->files[index]->restoring ||
                    (index == mainw->scrap_file && index > -1) || (index == mainw->ascrap_file && index > -1)
                    || (mainw->files[index]->frames == 0 &&
                        LIVES_IS_PLAYING)) &&
                   index != mainw->current_file);
          if (index == mainw->current_file) index = -1;
          if (mainw->current_file != mainw->scrap_file && mainw->current_file != mainw->ascrap_file) remove_from_clipmenu();
        }

        if (CURRENT_CLIP_IS_NORMAL && cfile->ext_src) {
          if (cfile->ext_src_type == LIVES_EXT_SRC_DECODER) {
            close_clip_decoder(mainw->current_file);
          }
        }
        free_thumb_cache(mainw->current_file, 0);
        lives_freep((void **)&cfile->frame_index);
        lives_freep((void **)&cfile->frame_index_back);

        if (cfile->clip_type != CLIP_TYPE_GENERATOR && !mainw->close_keep_frames) {
          char *clipd = lives_build_path(prefs->workdir, cfile->handle, NULL);
          if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
            // as a safety feature we create a special file which allows the back end to delete the directory
            char *permitname = lives_build_filename(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
            lives_touch(permitname);
            lives_free(permitname);

            com = lives_strdup_printf("%s close \"%s\"", prefs->backend_sync, cfile->handle);
            lives_system(com, TRUE);
            lives_free(com);
          }
          lives_free(clipd);
          if (cfile->event_list_back) event_list_free(cfile->event_list_back);
          if (cfile->event_list) event_list_free(cfile->event_list);

          lives_list_free_all(&cfile->layout_map);
        }

        if (cfile->subt) subtitles_free(cfile);

        if (cfile->clip_type == CLIP_TYPE_YUV4MPEG) {
#ifdef HAVE_YUV4MPEG
          lives_yuv_stream_stop_read((lives_yuv4m_t *)cfile->ext_src);
          lives_free(cfile->ext_src);
#endif
        }

        if (cfile->clip_type == CLIP_TYPE_VIDEODEV) {
#ifdef HAVE_UNICAP
          lives_vdev_free((lives_vdev_t *)cfile->ext_src);
          lives_free(cfile->ext_src);
#endif
        }

        if (cfile->audio_waveform) {
          for (i = 0; i < cfile->achans; i++) lives_freep((void **)&cfile->audio_waveform[i]);
          lives_freep((void **)&cfile->audio_waveform);
          lives_free(cfile->aw_sizes);
        }

        lives_freep((void **)&cfile);

        if (mainw->multitrack && mainw->current_file != mainw->multitrack->render_file) {
          mt_delete_clips(mainw->multitrack, mainw->current_file);
        }

        if (mainw->first_free_file == ALL_USED || mainw->first_free_file > mainw->current_file)
          mainw->first_free_file = mainw->current_file;

        if (!mainw->only_close) {
          if (IS_VALID_CLIP(file_to_switch_to) && file_to_switch_to > 0) {
            if (!mainw->multitrack) {
              if (!LIVES_IS_PLAYING) {
                mainw->current_file = file_to_switch_to;
                switch_clip(1, file_to_switch_to, TRUE);
                d_print("");
              } else {
                if (file_to_switch_to != mainw->playing_file) mainw->new_clip = file_to_switch_to;
              }
            } else if (old_file != mainw->multitrack->render_file) {
              mt_clip_select(mainw->multitrack, TRUE);
            }
            return;
          }
        }
        // file we were asked to switch to is invalid, thus we must find one

        if (mainw->current_file == mainw->blend_file) {
          need_new_blend_file = TRUE;
          // set blend_file to -1. This in case the file is a generator - we need to distinguish between the cases where
          // the generator is the blend file and we switch because it was deinited, and when we switch fg <-> bg
          // in the former case the generator is killed off, in the latter it survives
          mainw->blend_file = -1;
        }

        mainw->preview_frame = 0;

        if (!mainw->only_close) {
          // find another clip to switch to
          if (index > -1) {
            if (!mainw->multitrack) {
              if (!LIVES_IS_PLAYING) {
                switch_clip(1, index, TRUE);
                d_print("");
              } else mainw->new_clip = index;
              if (need_new_blend_file) mainw->blend_file = mainw->current_file;
            } else {
              mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
              mt_clip_select(mainw->multitrack, TRUE);
            }
            return;
          }
          if (mainw->clips_available > 0) {
            for (i = mainw->current_file - 1; i > 0; i--) {
              if (mainw->files[i]) {
                if (!mainw->multitrack) {
                  if (!LIVES_IS_PLAYING) {
                    switch_clip(1, i, TRUE);
                    d_print("");
                  } else mainw->new_clip = index;
                  if (need_new_blend_file) mainw->blend_file = mainw->current_file;
                } else {
                  mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
                  mt_clip_select(mainw->multitrack, TRUE);
                }
                return;
              }
            }
            for (i = 1; i < MAX_FILES; i++) {
              if (mainw->files[i]) {
                if (!mainw->multitrack) {
                  if (!LIVES_IS_PLAYING) {
                    switch_clip(1, i, TRUE);
                    d_print("");
                  } else mainw->new_clip = index;
                  if (need_new_blend_file) mainw->blend_file = mainw->current_file;
                } else {
                  mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
                  mt_clip_select(mainw->multitrack, TRUE);
                }
                return;
		// *INDENT-OFF*
	      }}}}}
      // *INDENT-ON*

      // no other clips
      mainw->current_file = mainw->blend_file = -1;
      set_main_title(NULL, 0);

      lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
      lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
      lives_widget_set_sensitive(mainw->export_proj, FALSE);
      lives_widget_set_sensitive(mainw->import_proj, FALSE);

      if (mainw->multitrack) lives_widget_set_sensitive(mainw->multitrack->load_set, TRUE);

      // can't use set_undoable, as we don't have a cfile
      lives_menu_item_set_text(mainw->undo, _("_Undo"), TRUE);
      lives_menu_item_set_text(mainw->redo, _("_Redo"), TRUE);
      lives_widget_hide(mainw->redo);
      lives_widget_show(mainw->undo);
      lives_widget_set_sensitive(mainw->undo, FALSE);

      if (!mainw->is_ready || mainw->recovering_files) return;

      if (LIVES_IS_PLAYING) mainw->cancelled = CANCEL_GENERATOR_END;

      if (!mainw->multitrack) {
        //resize(1);
        lives_widget_set_opacity(mainw->playframe, 0.);
        //lives_widget_hide(mainw->playframe);
        load_start_image(0);
        load_end_image(0);
        if (prefs->show_msg_area && !mainw->only_close) {
          if (mainw->idlemax == 0) {
            lives_idle_add_simple(resize_message_area, NULL);
          }
          mainw->idlemax = DEF_IDLE_MAX;
        }
      }

      set_sel_label(mainw->sel_label);

      zero_spinbuttons();
      show_playbar_labels(-1);

      if (!mainw->only_close) {
        lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
        if (!LIVES_IS_PLAYING) d_print("");

        if (mainw->multitrack) {
          mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
          mt_clip_select(mainw->multitrack, TRUE);
        }
      }
      if (!LIVES_IS_PLAYING && !mainw->is_processing && !mainw->preview) {
        if (mainw->multitrack) mt_sensitise(mainw->multitrack);
        else sensitize();
      }
    }


    void switch_to_file(int old_file, int new_file) {
      // this function is used for full clip switching (during non-playback or non fs)

      // calling this function directly is now deprecated in favour of switch_clip()

      int orig_file = mainw->current_file;

      // should use close_current_file
      if (!IS_VALID_CLIP(new_file)) {
        char *msg = lives_strdup_printf("attempt to switch to invalid clip %d", new_file);
        LIVES_WARN(msg);
        lives_free(msg);
        return;
      }

      if (mainw->multitrack) return;

      if (LIVES_IS_PLAYING) {
        mainw->new_clip = new_file;
        return;
      }

      mainw->current_file = new_file;

      if (old_file != new_file) {
        if (CURRENT_CLIP_IS_VALID) {
          mainw->laudio_drawable = cfile->laudio_drawable;
          mainw->raudio_drawable = cfile->raudio_drawable;
          mainw->drawsrc = mainw->current_file;
        }
        if (old_file != 0 && new_file != 0) mainw->preview_frame = 0;
        if (1) {
          // TODO - indicate "opening" in clipmenu

          //      if (old_file>0&&mainw->files[old_file]!=NULL&&mainw->files[old_file]->menuentry!=NULL&&
          //  (mainw->files[old_file]->clip_type==CLIP_TYPE_DISK||mainw->files[old_file]->clip_type==CLIP_TYPE_FILE)) {
          //char menutext[32768];
          //get_menu_text_long(mainw->files[old_file]->menuentry,menutext);

          //lives_menu_item_set_text(mainw->files[old_file]->menuentry,menutext,FALSE);
          //}
          lives_widget_set_sensitive(mainw->select_new, (cfile->insert_start > 0));
          lives_widget_set_sensitive(mainw->select_last, (cfile->undo_start > 0));
          if ((cfile->start == 1 || cfile->end == cfile->frames) && !(cfile->start == 1 && cfile->end == cfile->frames)) {
            lives_widget_set_sensitive(mainw->select_invert, TRUE);
          } else {
            lives_widget_set_sensitive(mainw->select_invert, FALSE);
          }
          if (IS_VALID_CLIP(old_file) && mainw->files[old_file]->opening) {
            // switch while opening - come out of processing dialog
            if (mainw->proc_ptr) {
              lives_widget_destroy(mainw->proc_ptr->processing);
              lives_freep((void **)&mainw->proc_ptr);
	      // *INDENT-OFF*
	    }}}}
      // *INDENT-ON*

      if (mainw->play_window && cfile->is_loaded && orig_file != new_file) {
        resize_play_window();

        // if the clip is loaded
        if (!mainw->preview_box) {
          // create the preview box that shows frames...
          make_preview_box();
        }
        // add it the play window...
        if (!lives_widget_get_parent(mainw->preview_box)) {
          lives_widget_queue_draw(mainw->play_window);
          lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
        }

        lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
        lives_widget_show_all(mainw->preview_box);
        lives_widget_show_now(mainw->preview_box);
        lives_widget_set_no_show_all(mainw->preview_controls, TRUE);

        // and resize it
        lives_widget_grab_focus(mainw->preview_spinbutton);
        load_preview_image(FALSE);
      }

      if (!mainw->go_away && !LIVES_IS_PLAYING && CURRENT_CLIP_IS_NORMAL) {
        mainw->no_context_update = TRUE;
        reget_afilesize(mainw->current_file);
        mainw->no_context_update = FALSE;
      }

      if (!CURRENT_CLIP_IS_VALID) return;
      //chill_decoder_plugin(mainw->current_file);

      if (!CURRENT_CLIP_IS_NORMAL || cfile->opening) {
        lives_widget_set_sensitive(mainw->rename, FALSE);
      }

      if (cfile->menuentry) {
        reset_clipmenu();
      }

      if (!mainw->switch_during_pb && !cfile->opening) sensitize();

      lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
      lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);

      set_sel_label(mainw->sel_label);

      if (mainw->eventbox5) lives_widget_show(mainw->eventbox5);
      lives_widget_show(mainw->hruler);
      lives_widget_show(mainw->vidbar);
      lives_widget_show(mainw->laudbar);

      if (cfile->achans < 2) {
        lives_widget_hide(mainw->raudbar);
      } else {
        lives_widget_show(mainw->raudbar);
      }

      if (cfile->redoable) {
        lives_widget_show(mainw->redo);
        lives_widget_hide(mainw->undo);
      } else {
        lives_widget_hide(mainw->redo);
        lives_widget_show(mainw->undo);
      }

      if (new_file > 0) {
        if (cfile->menuentry) {
          set_main_title(cfile->name, 0);
        } else set_main_title(cfile->file_name, 0);
      }

      if (cfile->frames == 0) {
        zero_spinbuttons();
      }

      resize(1);
      if (!mainw->go_away) {
        get_play_times();
      }

      // if the file was opening, continue...
      if (cfile->opening) {
        open_file(cfile->file_name);
      } else {
        showclipimgs();
        redraw_timeline(mainw->current_file);
        lives_ce_update_timeline(0, cfile->pointer_time);
        mainw->ptrtime = cfile->pointer_time;
        lives_widget_queue_draw(mainw->eventbox2);
      }

      if (!mainw->multitrack && !mainw->reconfig) {
        if (prefs->show_msg_area && !mainw->only_close) {
          reset_message_area(); // necessary
          if (mainw->idlemax == 0) {
            lives_idle_add_simple(resize_message_area, NULL);
          }
          mainw->idlemax = DEF_IDLE_MAX;
        }
      }
    }


    boolean  switch_audio_clip(int new_file, boolean activate) {
      ticks_t timeout;
      lives_alarm_t alarm_handle;

      if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
        if (mainw->jackd) {
          if (mainw->jackd->playing_file == new_file ||
              (IS_VALID_CLIP(mainw->playing_file) && mainw->files[mainw->playing_file]->achans > 0
               && mainw->jackd->playing_file != mainw->playing_file)) return FALSE;

          if (mainw->scratch == SCRATCH_JUMP) {
            mainw->files[new_file]->aseek_pos =
              (off_t)((double)mainw->files[new_file]->frameno / mainw->files[new_file]->fps * mainw->files[new_file]->arate)
              * mainw->files[new_file]->achans * mainw->files[new_file]->asampsize / 8;
          }

          if (!activate) mainw->jackd->in_use = FALSE;

          alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
          while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd) != NULL) {
            // wait for seek
            lives_nanosleep(1000);
          }
          lives_alarm_clear(alarm_handle);
          if (timeout == 0) {
            mainw->cancelled = handle_audio_timeout();
            return FALSE;
          }

          if (mainw->jackd->playing_file > 0) {
            jack_message.command = ASERVER_CMD_FILE_CLOSE;
            jack_message.data = NULL;
            jack_message.next = NULL;
            mainw->jackd->msgq = &jack_message;

            alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
            while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd) != NULL) {
              // wait for seek
              lives_nanosleep(1000);
            }
            lives_alarm_clear(alarm_handle);
            if (timeout == 0)  {
              mainw->cancelled = handle_audio_timeout();
              return FALSE;
            }
          }
        }
        if (!IS_VALID_CLIP(new_file)) {
          mainw->jackd->in_use = FALSE;
          return FALSE;
        }

        if (CLIP_HAS_AUDIO(new_file)) {
          int asigned = !(mainw->files[new_file]->signed_endian & AFORM_UNSIGNED);
          int aendian = !(mainw->files[new_file]->signed_endian & AFORM_BIG_ENDIAN);
          mainw->jackd->num_input_channels = mainw->files[new_file]->achans;
          mainw->jackd->bytes_per_channel = mainw->files[new_file]->asampsize / 8;
          if (activate && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
            if (!mainw->files[new_file]->play_paused)
              mainw->jackd->sample_in_rate = mainw->files[new_file]->arate * mainw->files[new_file]->pb_fps /
                                             mainw->files[new_file]->fps;
            else mainw->jackd->sample_in_rate = mainw->files[new_file]->arate * mainw->files[new_file]->freeze_fps /
                                                  mainw->files[new_file]->fps;
          } else mainw->jackd->sample_in_rate = mainw->files[new_file]->arate;
          if (mainw->files[new_file]->adirection == LIVES_DIRECTION_REVERSE)
            mainw->jackd->sample_in_rate = -abs(mainw->jackd->sample_in_rate);
          else
            mainw->jackd->sample_in_rate = abs(mainw->jackd->sample_in_rate);
          mainw->jackd->usigned = !asigned;
          mainw->jackd->seek_end = mainw->files[new_file]->afilesize;

          if ((aendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
              (!aendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
            mainw->jackd->reverse_endian = TRUE;
          else mainw->jackd->reverse_endian = FALSE;

          if (mainw->ping_pong) mainw->jackd->loop = AUDIO_LOOP_PINGPONG;
          else mainw->jackd->loop = AUDIO_LOOP_FORWARD;

          avsync_force();

          // tell jack server to open audio file and start playing it

          jack_message.command = ASERVER_CMD_FILE_OPEN;

          jack_message.data = lives_strdup_printf("%d", new_file);

          jack_message2.command = ASERVER_CMD_FILE_SEEK;
          jack_message.next = &jack_message2;
          jack_message2.data = lives_strdup_printf("%"PRId64, mainw->files[new_file]->aseek_pos);
          if (LIVES_IS_PLAYING && !mainw->preview) jack_message2.tc = lives_get_current_ticks();
          jack_message2.next = NULL;

          mainw->jackd->msgq = &jack_message;
          mainw->jackd->in_use = TRUE;

          mainw->jackd->is_paused = mainw->files[new_file]->play_paused;
          mainw->jackd->is_silent = FALSE;
          mainw->rec_aclip = new_file;
          mainw->rec_avel = (double)mainw->jackd->sample_in_rate / (double)mainw->files[new_file]->arps;
          mainw->rec_aseek = fabs((double)(mainw->files[new_file]->aseek_pos
                                           / (mainw->files[new_file]->achans * mainw->files[new_file]->asampsize / 8))
                                  / (double)mainw->files[new_file]->arps);
        } else {
          jack_get_rec_avals(mainw->jackd);
          mainw->rec_avel = 0.;
          mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
        }
        /* event = get_last_frame_event(mainw->event_list); */
        /* insert_audio_event_at(event, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel); */
        /* mainw->rec_aclip = -1; */
#endif
      }

      if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
        if (mainw->pulsed) {
          if (mainw->pulsed->playing_file == new_file ||
              (IS_VALID_CLIP(mainw->playing_file) && mainw->files[mainw->playing_file]->achans > 0
               && mainw->pulsed->playing_file != mainw->playing_file)) return FALSE;

          if (mainw->scratch == SCRATCH_JUMP) {
            mainw->files[new_file]->aseek_pos =
              (off_t)((double)mainw->files[new_file]->frameno / mainw->files[new_file]->fps * mainw->files[new_file]->arate)
              * mainw->files[new_file]->achans * mainw->files[new_file]->asampsize / 8;
          }

          if (!activate) mainw->pulsed->in_use = FALSE;

          alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
          while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed)) {
            // wait for seek
            lives_nanosleep(1000);
          }
          lives_alarm_clear(alarm_handle);
          if (timeout == 0)  {
            mainw->cancelled = handle_audio_timeout();
            return FALSE;
          }

          if (mainw->pulsed->fd > 0) {
            pulse_message.command = ASERVER_CMD_FILE_CLOSE;
            pulse_message.data = NULL;
            pulse_message.next = NULL;
            mainw->pulsed->msgq = &pulse_message;

            alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
            while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed) != NULL) {
              // wait for seek
              lives_nanosleep(1000);
            }
            lives_alarm_clear(alarm_handle);
            if (timeout == 0)  {
              mainw->cancelled = handle_audio_timeout();
              return FALSE;
            }
          }

          if (!IS_VALID_CLIP(new_file)) {
            mainw->pulsed->in_use = FALSE;
            return FALSE;
          }

          mainw->pulsed->in_use = TRUE;

          if (CLIP_HAS_AUDIO(new_file)) {
            int asigned = !(mainw->files[new_file]->signed_endian & AFORM_UNSIGNED);
            int aendian = !(mainw->files[new_file]->signed_endian & AFORM_BIG_ENDIAN);
            mainw->pulsed->in_achans = mainw->files[new_file]->achans;
            mainw->pulsed->in_asamps = mainw->files[new_file]->asampsize;
            if (activate && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
              if (!mainw->files[new_file]->play_paused)
                mainw->pulsed->in_arate = mainw->files[new_file]->arate
                                          * mainw->files[new_file]->pb_fps /
                                          mainw->files[new_file]->fps;
              else mainw->pulsed->in_arate = mainw->files[new_file]->arate
                                               * mainw->files[new_file]->freeze_fps /
                                               mainw->files[new_file]->fps;
            } else mainw->pulsed->in_arate = mainw->files[new_file]->arate;
            if (mainw->files[new_file]->adirection == LIVES_DIRECTION_REVERSE)
              mainw->pulsed->in_arate = -abs(mainw->pulsed->in_arate);
            else
              mainw->pulsed->in_arate = abs(mainw->pulsed->in_arate);
            mainw->pulsed->usigned = !asigned;
            mainw->pulsed->seek_end = mainw->files[new_file]->afilesize;

            if ((aendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
                (!aendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
              mainw->pulsed->reverse_endian = TRUE;
            else mainw->pulsed->reverse_endian = FALSE;

            if (mainw->ping_pong) mainw->pulsed->loop = AUDIO_LOOP_PINGPONG;
            else mainw->pulsed->loop = AUDIO_LOOP_FORWARD;

            avsync_force();

            // tell pulse server to open audio file and start playing it

            pulse_message.command = ASERVER_CMD_FILE_OPEN;
            pulse_message.data = lives_strdup_printf("%d", new_file);

            pulse_message2.command = ASERVER_CMD_FILE_SEEK;
            if (LIVES_IS_PLAYING && !mainw->preview) pulse_message2.tc = lives_get_current_ticks();
            pulse_message.next = &pulse_message2;
            pulse_message2.data = lives_strdup_printf("%"PRId64, mainw->files[new_file]->aseek_pos);
            pulse_message2.next = NULL;
            mainw->pulsed->msgq = &pulse_message;
            mainw->pulsed->in_use = TRUE;

            mainw->pulsed->is_paused = mainw->files[new_file]->play_paused;
            mainw->rec_aclip = new_file;
            mainw->rec_avel = (double)mainw->pulsed->in_arate / (double)mainw->files[new_file]->arps;
            mainw->rec_aseek = fabs((double)(mainw->files[new_file]->aseek_pos
                                             / (mainw->files[new_file]->achans * mainw->files[new_file]->asampsize / 8))
                                    / (double)mainw->files[new_file]->arps);
          } else {
            pulse_get_rec_avals(mainw->pulsed);
            mainw->rec_avel = 0.;
            mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
          }
        }
#endif
      }

#if 0
      if (prefs->audio_player == AUD_PLAYER_NONE) {
        if (!IS_VALID_CLIP(new_file)) {
          mainw->nullaudio_playing_file = -1;
          return FALSE;
        }
        if (mainw->nullaudio->playing_file == new_file) return FALSE;
        nullaudio_clip_set(new_file);
        if (activate && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
          if (!mainw->files[new_file]->play_paused)
            nullaudio_arate_set(mainw->files[new_file]->arate * mainw->files[new_file]->pb_fps /
                                mainw->files[new_file]->fps);
          else nullaudio_arate_set(mainw->files[new_file]->arate
                                     * mainw->files[new_file]->freeze_fps /
                                     mainw->files[new_file]->fps);
        } else nullaudio_arate_set(mainw->files[new_file]->arate);
        nullaudio_seek_set(mainw->files[new_file]->aseek_pos);
        if (CLIP_HAS_AUDIO(new_file)) {
          nullaudio_get_rec_avals();
        } else {
          nullaudio_get_rec_avals();
          mainw->rec_avel = 0.;
        }
      }
#endif
      return TRUE;
    }


    void do_quick_switch(int new_file) {
      // handle clip switching during playback
      // calling this function directly is now deprecated in favour of switch_clip()
      boolean osc_block;
      int old_file = mainw->current_file;// area = 0;

      if (mainw->current_file < 1 || !mainw->files[new_file]) return;

      if (mainw->multitrack
          || (mainw->record && !mainw->record_paused && !(prefs->rec_opts & REC_CLIPS)) ||
          mainw->foreign || (mainw->preview && !mainw->is_rendering)) return;

      if (!LIVES_IS_PLAYING) {
        switch_to_file(mainw->current_file, new_file);
        return;
      }

      if (mainw->noswitch) {
        mainw->new_clip = new_file;
        return;
      }

      mainw->blend_palette = WEED_PALETTE_END;

      if (CURRENT_CLIP_IS_VALID && new_file != mainw->current_file && mainw->current_file != mainw->blend_file
          && !mainw->is_rendering) {
        if (cfile->clip_type == CLIP_TYPE_GENERATOR && cfile->ext_src) {
          if (new_file != mainw->blend_file) {
            // switched from generator to another clip, end the generator
            weed_plant_t *inst = (weed_plant_t *)cfile->ext_src;
            mainw->gen_started_play = FALSE;
            mainw->whentostop = NEVER_STOP;
            weed_generator_end(inst);
          } else {
            rte_swap_fg_bg();
          }
        }
        if (new_file == mainw->blend_file && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR
            && mainw->files[mainw->blend_file]->ext_src) {
          rte_swap_fg_bg();
        }
      }

      osc_block = mainw->osc_block;
      mainw->osc_block = TRUE;

      if (mainw->loop_locked) {
        unlock_loop_lock();
      }

      mainw->whentostop = NEVER_STOP;

      // TODO - can these be combined ?
      mainw->switch_during_pb = TRUE;
      mainw->clip_switched = TRUE;

      if (CURRENT_CLIP_IS_NORMAL) cfile->last_play_sequence = mainw->play_sequence;

      mainw->current_file = new_file;

      mainw->drawsrc = mainw->current_file;
      mainw->laudio_drawable = cfile->laudio_drawable;
      mainw->raudio_drawable = cfile->raudio_drawable;

      if (!mainw->fs && !mainw->faded) {
        redraw_timeline(mainw->current_file);
        show_playbar_labels(mainw->current_file);
      }

      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), mainw->files[new_file]->pb_fps);
      changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), LIVES_INT_TO_POINTER(1));

      // switch audio clip
      if (is_realtime_aplayer(prefs->audio_player) && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS)
          && !mainw->is_rendering && (mainw->preview || !(mainw->agen_key != 0 || mainw->agen_needs_reinit
                                      || prefs->audio_src == AUDIO_SRC_EXT))) {
        switch_audio_clip(new_file, TRUE);
      }

      mainw->deltaticks = 0;

      set_main_title(cfile->name, 0);

      if (mainw->ce_thumbs && mainw->active_sa_clips == SCREEN_AREA_FOREGROUND) ce_thumbs_highlight_current_clip();

      if (CURRENT_CLIP_IS_NORMAL) {
        char *tmp;
        tmp = lives_build_filename(prefs->workdir, cfile->handle, LIVES_STATUS_FILE_NAME, NULL);
        lives_snprintf(cfile->info_file, PATH_MAX, "%s", tmp);
        lives_free(tmp);
      }

      if (!CURRENT_CLIP_IS_NORMAL || (mainw->event_list && !mainw->record))
        mainw->play_end = INT_MAX;

      // act like we are not playing a selection (but we will try to keep to
      // selection bounds)
      mainw->playing_sel = FALSE;

      if (!cfile->frameno && cfile->frames) cfile->frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
      cfile->last_frameno = cfile->frameno;

      mainw->playing_file = new_file;

      cfile->next_event = NULL;

#if GTK_CHECK_VERSION(3, 0, 0)
      if (LIVES_IS_PLAYING && !mainw->play_window && (!IS_VALID_CLIP(old_file)
          || !CURRENT_CLIP_IS_VALID || cfile->hsize != mainw->files[old_file]->hsize
          || cfile->vsize != mainw->files[old_file]->vsize)) {
        clear_widget_bg(mainw->play_image, mainw->play_surface);
      }
#endif

      if (CURRENT_CLIP_HAS_VIDEO) {
        if (!mainw->fs && !mainw->faded) {
          lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
          lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 1, cfile->frames);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), cfile->end);
          lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);

          lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
          lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 1, cfile->frames);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), cfile->start);
          lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);

          if (!mainw->play_window && mainw->double_size) {
            //frame_size_update();
            resize(2.);
          } else resize(1);
        }
      } else resize(1);

      if (!mainw->fs && !mainw->faded) {
        showclipimgs();
      }

      if (new_file == mainw->blend_file) {
        check_layer_ready(mainw->blend_layer);
        mainw->blend_file = old_file;
      }

      // force loading of a frame from the new clip
      mainw->force_show = TRUE;

      mainw->fps_mini_measure = 1.;
      mainw->fps_mini_ticks = mainw->currticks;

      if (mainw->play_window && prefs->show_playwin) {
        lives_window_present(LIVES_WINDOW(mainw->play_window));
        lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
      }

      //mainw->switch_during_pb = FALSE;
      mainw->osc_block = osc_block;
      lives_ruler_set_upper(LIVES_RULER(mainw->hruler), CURRENT_CLIP_TOTAL_TIME);

      if (!mainw->fs && !mainw->faded) {
        redraw_timeline(mainw->current_file);
        set_sel_label(mainw->sel_label);
      }
    }


    void resize(double scale) {
      // resize the frame widgets
      // set scale < 0. to _force_ the playback frame to expand (for external capture)
      LiVESXWindow *xwin;
      double oscale = scale;

      int xsize, vspace;
      int bx, by;

      // height of the separator imeage

      // maximum values
      int hsize, vsize;
      int w, h;
      int scr_width = GUI_SCREEN_WIDTH;
      int scr_height = GUI_SCREEN_HEIGHT;

      if (!prefs->show_gui || mainw->multitrack) return;
      vspace = get_vspace();

      get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);
      bx *= 2;

      if (prefs->open_maximised && by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);

      w = lives_widget_get_allocation_width(LIVES_MAIN_WINDOW_WIDGET);
      h = lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET);

      // resize the main window so it fits the gui monitor
      if (prefs->open_maximised)
        lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      else if (w > scr_width - bx || h > scr_height - by) {
        w = scr_width - bx;
        h = scr_height - by - mainw->mbar_res;
        lives_window_unmaximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
        lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);
      }

      //if (mainw->play_window) return;

      hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
      vsize = scr_height - (CE_TIMELINE_VSPACE / sqrt(widget_opts.scale) + vspace + by
                            + (prefs->show_msg_area ? mainw->mbar_res : 0)
                            + widget_opts.border_width * 2);

      if (scale < 0.) {
        // foreign capture
        scale = -scale;
        hsize = (scr_width - H_RESIZE_ADJUST - bx) / scale;
        vsize = (scr_height - V_RESIZE_ADJUST - by) / scale;
      }

      mainw->ce_frame_width = hsize;
      mainw->ce_frame_height = vsize;

      if (oscale == 2.) {
        if (hsize * 2 > scr_width - SCR_WIDTH_SAFETY) {
          scale = 1.;
        }
      }

      if (oscale > 0.) {
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
        lives_widget_set_size_request(mainw->frame1, mainw->ce_frame_width, mainw->ce_frame_height);
        lives_widget_set_size_request(mainw->eventbox3, mainw->ce_frame_width, mainw->ce_frame_height);
        lives_widget_set_size_request(mainw->frame2, mainw->ce_frame_width, mainw->ce_frame_height);
        lives_widget_set_size_request(mainw->eventbox4, mainw->ce_frame_width, mainw->ce_frame_height);

        lives_widget_set_size_request(mainw->start_image, mainw->ce_frame_width, mainw->ce_frame_height);
        lives_widget_set_size_request(mainw->end_image, mainw->ce_frame_width, mainw->ce_frame_height);

        // use unscaled size in dblsize
        if (scale > 1.) {
          hsize *= scale;
          vsize *= scale;
        }
        lives_widget_set_size_request(mainw->playframe, hsize, vsize);
        lives_widget_set_size_request(mainw->pl_eventbox, hsize, vsize);
        lives_widget_set_size_request(mainw->playarea, hsize, vsize);

        // IMPORTANT (or the entire image will not be shown)
        lives_widget_set_size_request(mainw->play_image, hsize, vsize);
        xwin = lives_widget_get_xwindow(mainw->play_image);
        if (LIVES_IS_XWINDOW(xwin)) {
          if (mainw->play_surface) lives_painter_surface_destroy(mainw->play_surface);
          mainw->play_surface =
            lives_xwindow_create_similar_surface(xwin, LIVES_PAINTER_CONTENT_COLOR,
                                                 hsize, vsize);
          clear_widget_bg(mainw->play_image, mainw->play_surface);
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

      update_sel_menu();

      if (scale != oscale) {
        lives_widget_context_update();
        if (prefs->open_maximised)
          lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
        else if (w > scr_width - bx || h > scr_height - by) {
          w = scr_width - bx;
          h = scr_height - by;
          lives_window_unmaximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
          lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);
        }
      }
    }
