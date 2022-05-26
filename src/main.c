// main.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2021 <salsaman+lives@gmail.com>

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
#include "multitrack-gui.h"

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
static boolean info_only;

static char *newconfigfile = NULL;

static char start_file[PATH_MAX];
static double start = 0.;
static int end = 0;

static boolean theme_error;

static _ign_opts ign_opts;

static int zargc;
static char **zargv;

static int o_argc;
static char **o_argv;

#ifndef NO_PROG_LOAD
static int xxwidth = 0, xxheight = 0;
#endif

static char *old_vhash = NULL;
static int initial_startup_phase = 0;
static boolean needs_workdir = FALSE;
static boolean ran_ds_dlg = FALSE;

static void do_start_messages(void);

int orig_argc(void) {return o_argc;}
char **orig_argv(void) {return o_argv;}

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
  if (prefs && prefs->show_dev_opts) {
    g_printerr("BANG ! hit breakpoint %s\n", brkstr ? brkstr : "???");
  }
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
    //#define SHOW_INFO_ERRORS
#ifndef SHOW_INFO_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_INFO) return;
#endif
    //#define SHOW_MSG_ERRORS
#ifndef SHOW_MSG_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_MESSAGE) return;
#endif
#define NO_WARN_ERRORS
#ifdef NO_WARN_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_WARNING) {
      return;
    }
#endif
    //#define NO_CRITICAL_ERRORS
#ifdef NO_CRITICAL_ERRORS
    if (xlevel == LIVES_LOG_LEVEL_CRITICAL) {
      return;
    }
#endif
#endif

    //#define TRAP_THEME_ERRORS
    //#define SHOW_THEME_ERRORS
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
    if (*message && !strncmp(message, TRAP_ERRMSG, strlen(TRAP_ERRMSG))) {
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


#ifdef USE_GLIB

#define N_LOG_FIELDS 2
//static const GLogField log_fields[N_LOG_FIELDS] = {G_LOG_DOMAIN, GLIB_PRIORITY};
static const GLogField log_domain[1] = {G_LOG_DOMAIN};

static GLogWriterOutput lives_log_writer(GLogLevelFlags log_level, const GLogField *fields, gsize n_fields,
    gpointer user_data) {
  char *dom = g_log_writer_format_fields(log_level, log_domain, 1, FALSE);
  //char *msg = g_log_writer_format_fields(log_level, log_fields, N_LOG_FIELDS, TRUE);
  lives_log_handler(dom, log_level, "", user_data);
  lives_free(dom);// lives_free(log_fields);
  return G_LOG_WRITER_HANDLED;
}

#endif // USE_GLIB


#ifdef ENABLE_JACK
LIVES_LOCAL_INLINE boolean jack_warn(boolean is_trans, boolean is_con) {
  char *com = NULL;
  boolean ret = TRUE;

  mainw->fatal = TRUE;

  if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
  if (!is_con) {
    ret = do_jack_no_startup_warn(is_trans);
    //if (!prefs->startup_phase) do_jack_restart_warn(-1, NULL);
  } else {
    ret = do_jack_no_connect_warn(is_trans);
    //if (!prefs->startup_phase) do_jack_restart_warn(16, NULL);

    // as a last ditch attempt, try to launch the server
    if (is_trans) {
      if (prefs->jack_opts & JACK_OPTS_START_TSERVER) {
        if (*future_prefs->jack_tserver_cfg) com = lives_strdup_printf("source '%s'", future_prefs->jack_tserver_cfg);
      }
    } else {
      if (prefs->jack_opts & JACK_OPTS_START_ASERVER) {
        if (*future_prefs->jack_aserver_cfg) com = lives_strdup_printf("source '%s'", future_prefs->jack_aserver_cfg);
      }
    }
    if (com) {
      lives_hook_append(mainw->global_hook_closures, RESTART_HOOK, 0, lives_fork_cb, com);
    }
  }
  // TODO - if we have backup config, restore from it
  if (!ret) {
    maybe_abort(TRUE, mainw->restart_params);
  }
  if (com) {
    lives_hook_remove(mainw->global_hook_closures, RESTART_HOOK, lives_fork_cb, com);
  }
  return ret;
}
#endif


void defer_sigint(int signum) {
  // here we can prepare for and handle specific fn calls which we know may crash / abort
  // provided if we know ahead of time.
  // The main reason would be to show an error dialog and then exit, or transmit some error code first,
  // rather than simply doing nothing and aborting /exiting.
  // we should do the minimum necessary and exit, as the stack may be corrupted.
  char *logmsg = NULL;
#ifdef ENABLE_JACK
  boolean ret = TRUE;
#endif
  switch (mainw->crash_possible) {
#ifdef ENABLE_JACK
  case 1:
    if (mainw->jackd_trans) {
      logmsg = lives_strdup(_("Connection attempt timed out, aborting."));
      jack_log_errmsg(mainw->jackd_trans, logmsg);
    }
    while (ret) {
      // crash in jack_client_open() con - transport
      ret = jack_warn(TRUE, TRUE);
    }
    break;
  case 2:
    if (mainw->jackd) {
      logmsg = lives_strdup(_("Connection attempt timed out, aborting."));
      jack_log_errmsg(mainw->jackd, logmsg);
    }
    while (ret) {
      // crash in jack_client_open() con - audio
      ret = jack_warn(FALSE, TRUE);
    }
    break;
  case 3:
    while (ret) {
      // crash in jackctl_server_open() - transport
      ret = jack_warn(TRUE, FALSE);
    }
    break;
  case 4:
    while (ret) {
      // crash in jackctl_server_open() - audio
      ret = jack_warn(FALSE, FALSE);
    }
    break;
  case 5:
    while (ret) {
      // crash in jackctl_server_start() - transport
      ret = jack_warn(TRUE, FALSE);
    }
    break;
  case 6:
    while (ret) {
      // crash in jackctl_server_start() - audio
      ret = jack_warn(FALSE, FALSE);
    }
    break;
#endif
  default:
    break;
  }
  if (signum > 0) {
    if (logmsg) lives_free(logmsg);
    signal(signum, SIG_DFL);
    pthread_detach(pthread_self());
  } else lives_abort(logmsg);
}


void *defer_sigint_cb(lives_object_t *obj, void *pdtl) {
  int crpos = mainw->crash_possible;
  mainw->crash_possible = LIVES_POINTER_TO_INT(pdtl);
  defer_sigint(-1);
  mainw->crash_possible = crpos;
  return NULL;
}


//#define QUICK_EXIT
void catch_sigint(int signum) {
  // trap for ctrl-C and others
  //if (mainw->jackd) lives_jack_end();

  lives_hooks_trigger(NULL, THREADVAR(hook_closures), ABORT_HOOK);

  if (capable && !pthread_equal(capable->main_thread, pthread_self())) {
    // if we are not the main thread, just exit
    lives_proc_thread_t lpt = THREADVAR(tinfo);
    lives_thread_data_t *mydata = THREADVAR(mydata);
    //if (mydata) {
    lives_proc_thread_set_signalled(lpt, signum, mydata);
    g_print("Thread got signal %d\n", signum);
    pthread_detach(pthread_self());
  }

#ifdef QUICK_EXIT
  /* shoatend(); */
  /* fprintf(stderr, "shoatt end"); */
  /* fflush(stderr); */
  exit(signum);
#endif
  if (mainw) {
    lives_hooks_trigger(NULL, mainw->global_hook_closures, ABORT_HOOK);

    if (LIVES_MAIN_WINDOW_WIDGET) {
      if (mainw->foreign) {
        exit(signum);
      }

      if (mainw->record) backup_recording(NULL, NULL);

      if (mainw->multitrack) mainw->multitrack->idlefunc = 0;
      mainw->fatal = TRUE;

      if (signum == LIVES_SIGABRT || signum == LIVES_SIGSEGV || signum == LIVES_SIGFPE) {
        mainw->memok = FALSE;
        signal(LIVES_SIGSEGV, SIG_DFL);
        signal(LIVES_SIGABRT, SIG_DFL);
        signal(LIVES_SIGFPE, SIG_DFL);
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
  // gdk_display_manager_get_default_display(gdk_display_manager_get());
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

  widget_opts_rescale(prefs->screen_scale);

  if (!prefs->vj_mode && GUI_SCREEN_HEIGHT >= MIN_MSG_AREA_SCRNHEIGHT) {
    capable->can_show_msg_area = TRUE;
    if (future_prefs->show_msg_area) prefs->show_msg_area = TRUE;
  } else {
    prefs->show_msg_area = FALSE;
    capable->can_show_msg_area = FALSE;
  }
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

#ifdef VALGRIND_ON
  prefs->nfx_threads = 8;
#else
  if (mainw->debug) prefs->nfx_threads = 2;
#endif

  lives_threadpool_init();

  capable->gui_thread = pthread_self();

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
  pthread_mutex_init(&mainw->instance_ref_mutex, &mattr);

  // non-recursive
  pthread_mutex_init(&mainw->abuf_aux_frame_mutex, NULL);
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
  pthread_mutex_init(&mainw->trcount_mutex, NULL);
  pthread_mutex_init(&mainw->fx_mutex, NULL);
  pthread_mutex_init(&mainw->alock_mutex, NULL);
  pthread_mutex_init(&mainw->tlthread_mutex, NULL);

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

  if (prefs->show_dev_opts) {
    g_printerr("Today's lucky number is 0X%08lX\n", gen_unique_id());
  }

  get_string_pref(PREF_CDPLAY_DEVICE, prefs->cdplay_device, PATH_MAX);

  prefs->warning_mask = (uint64_t)get_int64_prefd(PREF_LIVES_WARNING_MASK, DEF_WARNING_MASK);

  prefs->badfile_intent = get_int_prefd(PREF_BADFILE_INTENT, LIVES_INTENTION_UNKNOWN);

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


static boolean lives_init(_ign_opts *ign_opts) {
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
  mainw->wall_ticks = -1;

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

    //load_decoders();
    if (ARE_UNCHECKED(decoder_plugins)) {
      prefs->disabled_decoders = locate_decoders(get_list_pref(PREF_DISABLED_DECODERS));
      capable->plugins_list[PLUGIN_TYPE_DECODER] = load_decoders();
      if (capable->plugins_list[PLUGIN_TYPE_DECODER]) {
        capable->has_decoder_plugins = PRESENT;
      } else capable->has_decoder_plugins = MISSING;
    }

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
      if (!(info = LPT_WITH_TIMEOUT(timeout, 0, (lives_funcptr_t)lives_jack_init,
                                    WEED_SEED_BOOLEAN, "iv",
                                    JACK_CLIENT_TYPE_TRANSPORT, NULL))) {
        if (mainw->cancelled) {
          lives_exit(0);
        }
        //lives_idle_priority(governor_loop, NULL);
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
          //lives_idle_priority(governor_loop, NULL);
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
                //lives_idle_priority(governor_loop, NULL);
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
    if (!do_startup_interface_query()) {
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

  get_machine_dets();
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
    if (capable->hw.cpu_features & CPU_HAS_SSE2) d_print(" SSE2");
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
      main_thread_execute((lives_funcptr_t)set_extra_colours, 0, NULL, "");
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
  else {
    palette->nice3.red = palette->nice2.red;
    palette->nice3.green = palette->nice2.green;
    palette->nice3.blue = palette->nice2.blue;
    palette->nice3.alpha = 1.;
    //main_thread_execute((lives_funcptr_t)set_extra_colours, 0, NULL, "");
  }
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
  // still experimenting...some values may need tweaking
  // suggested uses for each colour in the process of being defined
  // TODO - run a bg thread until we create GUI
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

  //set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "*", "border-width", "0");

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
  check_for_executable(&capable->has_md5sum, EXEC_MD5SUM);
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

  mainw->lazy = 0;
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
    if (!do_startup_tests(FALSE)) {
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
      lives_proc_thread_join(mainw->helper_procthreads[PT_CUSTOM_COLOURS]);
      // must take care to execute this here or in the function itself, otherwise
      // gtk+ may crash later
      main_thread_execute((lives_funcptr_t)set_extra_colours, 0, NULL, "");
    }
    mainw->helper_procthreads[PT_CUSTOM_COLOURS] = NULL;
  }
#endif

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

  mainw->lazy = lives_idle_add_simple(lazy_startup_checks, NULL);

  // timer to poll for external commands: MIDI, joystick, jack transport, osc, etc.
  mainw->kb_timer = lives_timer_add_simple(EXT_TRIGGER_INTERVAL, &ext_triggers_poll, NULL);

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

  if (!mainw->multitrack)
    lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_CE);
  else
    lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_MT);

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
  sigaddset(&smask, LIVES_SIGFPE);

  sact.sa_handler = sigfunc;
  sact.sa_flags = 0;
  sact.sa_mask = smask;

  sigaction(LIVES_SIGINT, &sact, NULL);
  sigaction(LIVES_SIGTERM, &sact, NULL);
  sigaction(LIVES_SIGSEGV, &sact, NULL);
  sigaction(LIVES_SIGABRT, &sact, NULL);
  sigaction(LIVES_SIGFPE, &sact, NULL);

  if (mainw) {
    if (sigfunc == defer_sigint) {
      mainw->signals_deferred = TRUE;
    } else {
      sigemptyset(&smask);
      sigaddset(&smask, LIVES_SIGHUP);
      sact.sa_handler = SIG_IGN;
      sact.sa_flags = 0;
      sact.sa_mask = smask;
      sigaction(LIVES_SIGHUP, &sact, NULL);
      mainw->signals_deferred = FALSE;
    }
  }
}


int real_main(int argc, char *argv[], pthread_t *gtk_thread, ulong id) {
  pthread_mutexattr_t mattr;
  wordexp_t extra_cmds;
  weed_error_t werr;
  char cdir[PATH_MAX];
  char **xargv = argv;
  char *tmp, *dir, *msg;
  char *extracmds_file;
  ssize_t mynsize;
  boolean toolong = FALSE;
  int winitopts = 0;
  int xargc = argc;

#ifndef IS_LIBLIVES
  weed_plant_t *test_plant;
#endif

  o_argc = argc;
  o_argv = argv;

  mainw = NULL;
  prefs = NULL;
  capable = NULL;

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
  capable->hw.cacheline_size = sizeof(void *) * 8;

  // _runtime_ byte order, needed for lives_strlen and other things
  if (IS_BIG_ENDIAN)
    capable->hw.byte_order = LIVES_BIG_ENDIAN;
  else
    capable->hw.byte_order = LIVES_LITTLE_ENDIAN;

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

#ifdef USE_GLIB
  g_log_set_default_handler(lives_log_handler, NULL);
  g_log_set_writer_func(lives_log_writer, NULL, NULL);
#endif

  //gtk_window_set_interactive_debugging(TRUE);
#ifndef LIVES_NO_DEBUG
  g_printerr("FULL DEBUGGING IS ON !!\n");
#endif
#endif

#ifndef IS_LIBLIVES
  // start up the Weed system
  weed_abi_version = weed_get_abi_version();
  if (weed_abi_version > WEED_ABI_VERSION) weed_abi_version = WEED_ABI_VERSION;
#ifdef WEED_STARTUP_TESTS
  winitopts |= WEED_INIT_DEBUGMODE;
#endif
  werr = weed_init(weed_abi_version, winitopts);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }

#ifndef USE_STD_MEMFUNCS
#ifdef USE_RPMALLOC
  weed_set_memory_funcs(rpmalloc, rpfree);
#else
#ifndef DISABLE_GSLICE
#if GLIB_CHECK_VERSION(2, 14, 0)
  weed_set_slab_funcs(lives_slice_alloc, lives_slice_unalloc, lives_slice_alloc_and_copy);
#else
  weed_set_slab_funcs(lives_slice_alloc, lives_slice_unalloc, NULL);
#endif
#else
  weed_set_memory_funcs(lives_malloc, lives_free);
#endif // DISABLE_GSLICE
#endif // USE_RPMALLOC
  weed_utils_set_custom_memfuncs(lives_malloc, lives_calloc, lives_memcpy, NULL, lives_free);
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
  _weed_leaf_seed_type = weed_leaf_seed_type;
  _weed_leaf_get_flags = weed_leaf_get_flags;
  _weed_leaf_set_flags = weed_leaf_set_flags;

  mainw = (mainwindow *)(lives_calloc(1, sizeof(mainwindow)));

#ifdef WEED_STARTUP_TESTS
  run_weed_startup_tests();
#if 0
  fprintf(stderr, "\n\nRetesting with API 200, bugfix mode\n");
  werr = weed_init(200, winitopts | WEED_INIT_ALLBUGFIXES);
  if (werr != WEED_SUCCESS) {
    lives_notify(LIVES_OSC_NOTIFY_QUIT, "Failed to init Weed");
    LIVES_FATAL("Failed to init Weed");
    _exit(1);
  }
  run_weed_startup_tests();
  fprintf(stderr, "\n\nRetesting with API 200, epecting problesm libweed-utils\n");
  werr = weed_init(200, winitopts);
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

  lives_snprintf(prefs->cmd_log, PATH_MAX, "%s", LIVES_DEVNULL);

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

  mainw->prefs_cache = mainw->hdrs_cache = mainw->gen_cache = mainw->meta_cache = NULL;

  // mainw->foreign is set if we are grabbing an external window
  mainw->foreign = FALSE;

  mainw->debug = FALSE;

  mainw->sense_state = LIVES_SENSE_STATE_INSENSITIZED;

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

  set_meta("status", "running");

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


///////////////////////////////// GUI section - TODO: move into another file //////////////////////////////////////

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
  LiVESXWindow *xwin;
  lives_rect_t update_rect;
  lives_painter_t *cr;
  int cx, cy;
  int rwidth, rheight, width, height, owidth, oheight;

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

  if (widget == mainw->play_window && rheight > mainw->pheight) rheight = mainw->pheight;

  update_rect.x = update_rect.y = 0;
  update_rect.width = rwidth;
  update_rect.height = rheight;

  if (!LIVES_IS_XWINDOW(xwin)) return;
  lives_xwindow_invalidate_rect(xwin, &update_rect, FALSE);
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


void load_start_image(frames_t frame) {
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
    bx = abs(bx);
    by = abs(by);
    if (!mainw->hdrbar) {
      if (by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }
    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE * 1.01 / sqrt(widget_opts.scale) + vspace + by
                          + (prefs->show_msg_area ? mainw->mbar_res : 0)
                          + widget_opts.border_width * 2);
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

  if (!prefs->ce_maxspect && !prefs->letterbox) {
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
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, start_pixbuf)))
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

    rwidth = lives_widget_get_allocation_width(mainw->start_image);
    rheight = lives_widget_get_allocation_height(mainw->start_image);

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
    if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, orig_pixbuf)))
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


void load_end_image(frames_t frame) {
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
    bx = abs(bx);
    by = abs(by);
    if (!mainw->hdrbar) {
      if (by > MENU_HIDE_LIM)
        lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
    }
    hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
    vsize = scr_height - (CE_TIMELINE_VSPACE * 1.01 / sqrt(widget_opts.scale) + vspace + by
                          + (prefs->show_msg_area ? mainw->mbar_res : 0)
                          + widget_opts.border_width * 2);
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

  if (!prefs->ce_maxspect && !prefs->letterbox) {
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
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, end_pixbuf)))
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

    rwidth = lives_widget_get_allocation_width(mainw->end_image);
    rheight = lives_widget_get_allocation_height(mainw->end_image);

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
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type),
                             tc, width, height, WEED_PALETTE_RGB24)) {
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
      if (!cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, orig_pixbuf)))
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
#if 0
}
#endif

#ifdef USE_GAPPL
static int gappquick(GApplication * application, GVariantDict * options,
                     livespointer user_data) {
  if (g_variant_dict_contains(options, "zzz")) {
    print_notice();
    return 0;
  }
  return -1;
}


static int gappcmd(GApplication * application, GApplicationCommandLine * cmdline,
                   livespointer user_data) {
  char **st_argv;
  int st_argc;
  st_argv = g_application_command_line_get_arguments(cmdline, &st_argc);
  return real_main(st_argc, st_argv, NULL, 0l);
}
#endif

#ifndef IS_LIBLIVES
int main(int argc, char *argv[]) {
  // call any hooks here
#ifdef USE_GAPPL
  GCancellable *canc = g_cancellable_new();
  GApplication *gapp = g_application_new("com.lives-video.LiVES",
                                         G_APPLICATION_HANDLES_COMMAND_LINE
                                         | G_APPLICATION_SEND_ENVIRONMENT);

  g_application_add_main_option(gapp, "version", 0, G_OPTION_ARG_NONE, G_OPTION_ARG_NONE,
                                "Show version and exit", NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(gapp), "handle-local-options",
                            LIVES_GUI_CALLBACK(gappquick), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(gapp), "command-line",
                            LIVES_GUI_CALLBACK(gappcmd), NULL);

  if (g_application_register(gapp, canc, NULL)) {
    return g_application_run(gapp, argc, argv);
  }
  return 0;
#endif
  real_main(argc, argv, NULL, 0l);
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
  int width, height;

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

    width = cfile->hsize;
    height = cfile->vsize;
    if (prefs->ce_maxspect && !prefs->letterbox) {
      width = mainw->pwidth;
      height = mainw->pheight;
    }
    if (prefs->letterbox) {
      calc_maxspect(mainw->pwidth, mainw->pheight, &width, &height);
    }

    if (!layer && !pixbuf) {
      layer = lives_layer_new_for_frame(mainw->current_file, mainw->preview_frame);
      tc = ((mainw->preview_frame - 1.)) / cfile->fps * TICKS_PER_SECOND;
      if (pull_frame_at_size(layer, get_image_ext_for_type(cfile->img_type), tc, width, height,
                             WEED_PALETTE_RGB24)) {
        LiVESInterpType interp = get_interp_value(prefs->pb_quality, TRUE);
        check_layer_ready(layer);
        if (!resize_layer(layer, width, height, interp, WEED_PALETTE_RGB24, 0) ||
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
      if (lives_pixbuf_get_width(pixbuf) == width && lives_pixbuf_get_height(pixbuf) == height)
        pr_pixbuf = pixbuf;
      else {
        pr_pixbuf = lives_pixbuf_scale_simple(pixbuf, width, height, LIVES_INTERP_BEST);
      }
      if (LIVES_IS_PIXBUF(pr_pixbuf)) {
        lives_widget_set_size_request(mainw->preview_image, MAX(width, mainw->sepwin_minwidth), height);
        set_drawing_area_from_pixbuf(mainw->preview_image, pr_pixbuf, mainw->pi_surface);
        if (pr_pixbuf != pixbuf) lives_widget_object_unref(pr_pixbuf);
        if (!layer || !cache_it || weed_layer_get_pixel_data(layer) || !(pixbuf_to_layer(layer, pixbuf)))
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
            char *fname = make_image_file_name(cfile, mainw->preview_frame,
                                               get_image_ext_for_type(cfile->img_type));
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
  weed_layer_t *layer = (weed_layer_t *)ptr;
  int nsize[2];
  nsize[0] = xxwidth;
  nsize[1] = xxheight;
  weed_set_int_array(layer, WEED_LEAF_NATURAL_SIZE, 2, nsize);
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
  ssize_t bread;
  //lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if ((bread = lives_read_buffered(fd, data, length, TRUE)) < length) {
    char *errmsg = lives_strdup_printf("png read_fn error, read only %ld bytes of %ld\n", bread, length);
    png_error(png_ptr, errmsg);
    lives_free(errmsg);
  }
}
#endif

typedef struct {
  weed_layer_t *layer;
  int width, height;
  LiVESInterpType interp;
  int pal, clamp;
} resl_priv_data;


#ifdef USE_RESTHREAD
static void res_thrdfunc(void *arg) {
  resl_priv_data *priv = (resl_priv_data *)arg;
  resize_layer(priv->layer, priv->width, priv->height, priv->interp, priv->pal, priv->clamp);
  weed_set_voidptr_value(priv->layer, WEED_LEAF_RESIZE_THREAD, NULL);
  lives_free(priv);
}


static void reslayer_thread(weed_layer_t *layer, int twidth, int theight, LiVESInterpType interp,
                            int tpalette, int clamp, double file_gamma) {
  resl_priv_data *priv = (resl_priv_data *)lives_malloc(sizeof(resl_priv_data));
  lives_proc_thread_t resthread;
  weed_set_double_value(layer, "file_gamma", file_gamma);
  priv->layer = layer;
  priv->width = twidth;
  priv->height = theight;
  priv->interp = interp;
  priv->pal = tpalette;
  priv->clamp = clamp;
  resthread = lives_proc_thread_create(LIVES_THRDATTR_NO_GUI, (lives_funcptr_t)res_thrdfunc, -1, "v", priv);
  weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, resthread);
}
#endif


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
#ifdef USE_RESTHREAD
    weed_set_int_value(layer, WEED_LEAF_PROGSCAN, 0);
#endif
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#ifndef PNG_BIO
    fclose(fp);
#endif
    return FALSE;
  }

#ifdef PNG_BIO
  png_set_read_fn(png_ptr, LIVES_INT_TO_POINTER(fd), png_read_func);
#ifndef VALGRIND_ON
  if (mainw->debug)
    png_set_sig_bytes(png_ptr, 0);
  else
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
               NULL, NULL, NULL, NULL, NULL);
  if (xwidth > 0 && xheight > 0) {
    weed_set_int_value(layer, WEED_LEAF_WIDTH, xwidth);
    weed_set_int_value(layer, WEED_LEAF_HEIGHT, xheight);

    if (!weed_plant_has_leaf(layer, WEED_LEAF_NATURAL_SIZE)) {
      int nsize[2];
      // set "natural_size" in case a filter needs it
      nsize[0] = xwidth;
      nsize[1] = xheight;
      weed_set_int_array(layer, WEED_LEAF_NATURAL_SIZE, 2, nsize);
    }

    privflags = weed_get_int_value(layer, LIVES_LEAF_HOST_FLAGS, NULL);

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
    //png_set_gamma(png_ptr, 1.0, .45455); /// default, seemingly: sRGB -> linear
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
    create_blank_layer(layer, LIVES_FILE_EXT_PNG, width, height, weed_layer_get_palette(layer));
#ifndef PNG_BIO
    fclose(fp);
#endif
    return FALSE;
  }

  // TODO: rowstride must be at least png_get_rowbytes(png_ptr, info_ptr)

  rowstride = weed_layer_get_rowstride(layer);
  ptr = weed_layer_get_pixel_data(layer);

  // libpng needs pointers to each row
  row_ptrs = (unsigned char **)lives_calloc(height,  sizeof(unsigned char *));
  for (int j = 0; j < height; j++) {
    row_ptrs[j] = &ptr[rowstride * j];
  }

#ifdef USE_RESTHREAD
  if (weed_threadsafe && twidth * theight != 0 && (twidth != width || theight != height) &&
      !png_get_interlace_type(png_ptr, info_ptr)) {
    weed_set_int_value(layer, WEED_LEAF_PROGSCAN, 1);
    reslayer_thread(layer, twidth, theight, get_interp_value(prefs->pb_quality, TRUE),
                    tpalette, weed_layer_get_yuv_clamping(layer),
                    gval == PNG_INFO_gAMA ? file_gamma : 1.);
    for (int j = 0; j < height; j++) {
      png_read_row(png_ptr, row_ptrs[j], NULL);
      weed_set_int_value(layer, WEED_LEAF_PROGSCAN, j + 1);
    }
    weed_set_int_value(layer, WEED_LEAF_PROGSCAN, -1);
  } else
#endif
    png_read_image(png_ptr, row_ptrs);

  //png_read_end(png_ptr, NULL);

  // end read

  lives_free(row_ptrs);

  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

  if (gval == PNG_INFO_gAMA) {
    /// if gAMA is set, then we need to convert to *linear* using the file gamma
    weed_layer_set_gamma(layer, WEED_GAMMA_LINEAR);
  } else weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

  if (is16bit) {
#ifdef USE_RESTHREAD
    lives_proc_thread_t resthread;
#endif
    int clamping, sampling, subspace;
    weed_layer_get_palette_yuv(layer, &clamping, &sampling, &subspace);
    weed_set_int_value(layer, LIVES_LEAF_PIXEL_BITS, 16);
    if (weed_palette_has_alpha(tpalette)) tpalette = WEED_PALETTE_YUVA4444P;
    else {
      if (tpalette != WEED_PALETTE_YUV420P) tpalette = WEED_PALETTE_YUV444P;
    }
#ifdef USE_RESTHREAD
    if ((resthread = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) {
      lives_proc_thread_join(resthread);
      weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL);
    }
#endif
    // convert RGBA -> YUVA4444P or RGB -> 444P or 420
    // 16 bit conversion
    convert_layer_palette_full(layer, tpalette, clamping, sampling, subspace, WEED_GAMMA_UNKNOWN);
  }
#ifndef PNG_BIO
  fclose(fp);
#endif
  return TRUE;
}


#if 0
static void png_write_func(png_structp png_ptr, png_bytep data, png_size_t length) {
  int fd = LIVES_POINTER_TO_INT(png_get_io_ptr(png_ptr));
  if (lives_write_buffered(fd, (const char *)data, length, TRUE) < length) {
    png_error(png_ptr, "write_fn error");
  }
}

static void png_flush_func(png_structp png_ptr) {
  int fd = LIVES_POINTER_TO_INT(png_get_io_ptr(png_ptr));
  if (lives_buffered_flush(fd) < 0) {
    png_error(png_ptr, "flush_fn error");
  }
}
#endif

static boolean save_to_png_inner(FILE * fp, weed_layer_t *layer, int comp) {
  // comp is 0 (none) - 9 (full)
  png_structp png_ptr;
  png_infop info_ptr;

  unsigned char **row_ptrs;
  unsigned char *ptr;

  int width, height, palette;
#if PNG_LIBPNG_VER >= 10504
  int flags = 0;
#endif
  int rowstride;

  int compr = (int)((100. - (double)comp + 5.) / 10.);

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);

  if (!png_ptr) {
    return FALSE;
  }

  info_ptr = png_create_info_struct(png_ptr);

  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    return FALSE;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    // libpng will longjump to here on error
    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    THREADVAR(write_failed) = fileno(fp) + 1;
    return FALSE;
  }

  //png_set_write_fn(png_ptr, LIVES_INT_TO_POINTER(fd), png_write_func, png_flush_func);

  //FILE *fp = fdopen(fd, "wb");

  png_init_io(png_ptr, fp);

  width = weed_layer_get_width(layer);
  height = weed_layer_get_height(layer);
  rowstride = weed_layer_get_rowstride(layer);
  palette = weed_layer_get_palette(layer);

  if (width <= 0 || height <= 0 || rowstride <= 0) {
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    LIVES_WARN("Cannot make png with 0 width or height");
    return FALSE;
  }

  if (palette == WEED_PALETTE_ARGB32) png_set_swap_alpha(png_ptr);

  switch (palette) {
  case WEED_PALETTE_BGR24:
    png_set_bgr(png_ptr);
  case WEED_PALETTE_RGB24:
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    break;
  case WEED_PALETTE_BGRA32:
    png_set_bgr(png_ptr);
  case WEED_PALETTE_RGBA32:
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    break;
  default:
    LIVES_ERROR("Bad png palette !\n");
    break;
  }

  png_set_compression_level(png_ptr, compr);

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

  png_write_info(png_ptr, info_ptr);

  ptr = (unsigned char *)weed_layer_get_pixel_data(layer);

  // Write image data

  /* for (i = 0 ; i < height ; i++) { */
  /*   png_write_row(png_ptr, &ptr[rowstride * i]); */
  /* } */

  row_ptrs = (unsigned char **)lives_calloc(height, sizeof(unsigned char *));
  for (int j = 0; j < height; j++) {
    row_ptrs[j] = &ptr[rowstride * j];
  }
  png_write_image(png_ptr, row_ptrs);
  lives_free(row_ptrs);

  // end write
  png_write_end(png_ptr, (png_infop)NULL);

  png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
  if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);

  return TRUE;
}

boolean save_to_png(weed_layer_t *layer, const char *fname, int comp) {
  //int fd = lives_create_buffered(fname, DEF_FILE_PERMS);
  FILE *fp = fopen(fname, "wb");
  boolean ret = save_to_png_inner(fp, layer, comp);
  //lives_close_buffered(fd);
  fclose(fp);
  return ret;
}

void *save_to_png_threaded(void *args) {
  savethread_priv_t *saveargs = (savethread_priv_t *)args;
  saveargs->success = save_to_png(saveargs->layer, saveargs->fname, saveargs->compression);
  return saveargs;
}

#endif // USE_LIBPNG


boolean weed_layer_create_from_file_progressive(weed_layer_t *layer, const char *fname, int width,
    int height, int tpalette, const char *img_ext) {
  LiVESPixbuf *pixbuf = NULL;
  LiVESError *gerror = NULL;
  char *shmpath = NULL;
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
  if (!mainw->debug)
    if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) is_png = TRUE;
#endif

#ifdef PNG_BIO
  fd = lives_open_buffered_rdonly(fname);
  if (fd < 0) break_me(fname);
  if (fd < 0) return FALSE;
#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
#ifndef VALGRIND_ON
  if (is_png) lives_buffered_rdonly_slurp(fd, 8);
  else lives_buffered_rdonly_slurp(fd, 0);
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
                       LIVES_GUI_CALLBACK(pbsize_set), layer);

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
  if (ret && fd >= 0) {
    lives_close_buffered(fd);
  }
#else
  if (fd >= 0) close(fd);
#endif
  if (shmpath) {
    lives_rm(fname);
    lives_rmdir_with_parents(shmpath);
  }
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
  weed_layer_t *vlayer, *xlayer = NULL;
  lives_clip_t *sfile = NULL;
  int clip;
  frames_t frame;
  int clip_type;

  boolean is_thread = FALSE;
  static boolean norecurse = FALSE;

  weed_layer_pixel_data_free(layer);
  weed_layer_ref(layer);
  weed_layer_set_size(layer, width, height);

  clip = lives_layer_get_clip(layer);
  frame = lives_layer_get_frame(layer);

  // the default unless overridden
  weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);

  if (weed_plant_has_leaf(layer, WEED_LEAF_HOST_PTHREAD)) is_thread = TRUE;

  weed_leaf_delete(layer, WEED_LEAF_NATURAL_SIZE);

  sfile = RETURN_VALID_CLIP(clip);

  if (!sfile) {
    if (target_palette != WEED_PALETTE_END) weed_layer_set_palette(layer, target_palette);
    create_blank_layer(layer, NULL, width, height, target_palette);
    weed_layer_unref(layer);
    return FALSE;
  }

  clip_type = sfile->clip_type;

  switch (clip_type) {
  case CLIP_TYPE_NULL_VIDEO:
    create_blank_layer(layer, image_ext, width, height, target_palette);
    weed_layer_unref(layer);
    return TRUE;
  case CLIP_TYPE_DISK:
  case CLIP_TYPE_FILE:
    // frame number can be 0 during rendering
    if (frame == 0) {
      create_blank_layer(layer, image_ext, width, height, target_palette);
      weed_layer_unref(layer);
      return TRUE;
    } else if (clip == mainw->scrap_file) {
      boolean res = load_from_scrap_file(layer, frame);
      if (!res) {
        create_blank_layer(layer, image_ext, width, height, target_palette);
        weed_layer_unref(layer);
        return FALSE;
      }
      weed_leaf_delete(layer, LIVES_LEAF_PIXBUF_SRC);
      weed_leaf_delete(layer, LIVES_LEAF_SURFACE_SRC);
      // clip width and height may vary dynamically
      if (LIVES_IS_PLAYING) {
        sfile->hsize = weed_layer_get_width(layer);
        sfile->vsize = weed_layer_get_height(layer);
      }
      // realign
      copy_pixel_data(layer, NULL, THREADVAR(rowstride_alignment));
      weed_layer_unref(layer);
      return TRUE;
    } else {
      boolean need_unlock = TRUE;
      pthread_mutex_lock(&sfile->frame_index_mutex);
      if (clip_type == CLIP_TYPE_FILE && sfile->frame_index && frame > 0 &&
          frame <= sfile->frames && is_virtual_frame(clip, frame)) {
        if (prefs->vj_mode && mainw->loop_locked && mainw->ping_pong && sfile->pb_fps >= 0. && !norecurse) {
          lives_proc_thread_t tinfo = THREADVAR(tinfo);
          LiVESPixbuf *pixbuf = NULL;
          THREADVAR(tinfo) = NULL;
          norecurse = TRUE;
          virtual_to_images(clip, frame, frame, FALSE, &pixbuf);
          pthread_mutex_unlock(&sfile->frame_index_mutex);
          need_unlock = FALSE;
          norecurse = FALSE;
          THREADVAR(tinfo) = tinfo;
          if (pixbuf) {
            if (!pixbuf_to_layer(layer, pixbuf)) {
              lives_widget_object_unref(pixbuf);
            }
            break;
          }
        }
        if (1) {
          // pull frame from video clip
          ///
#ifdef USE_REC_RS
          int nplanes;
#endif
          void **pixel_data;
          frames_t iframe;
          boolean res = TRUE;
          int *rowstrides;
          lives_decoder_t *dplug = NULL;
          /// HOST_DECODER is set in mulitrack, there is 1 decoder per track since multiple tracks can have the same clip
          if (weed_plant_has_leaf(layer, WEED_LEAF_HOST_DECODER)) {
            dplug = (lives_decoder_t *)weed_get_voidptr_value(layer, WEED_LEAF_HOST_DECODER, NULL);
          } else {
            /// experimental, multiple decoder plugins for each sfile,,,
            if (weed_plant_has_leaf(layer, LIVES_LEAF_ALTSRC)) {
              int srcnum = weed_get_int_value(layer, LIVES_LEAF_ALTSRC, NULL);
              dplug = sfile->alt_srcs[srcnum];
            }
            if (!dplug) dplug = (lives_decoder_t *)sfile->ext_src;
          }
          iframe = get_indexed_frame(clip, frame);
          if (!dplug || !dplug->cdata || iframe >= dplug->cdata->nframes) {
            if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
            create_blank_layer(layer, image_ext, width, height, target_palette);
            weed_layer_unref(layer);
            return FALSE;
          }
          if (target_palette != dplug->cdata->current_palette) {
            if (dplug->dpsys->set_palette) {
              int opal = dplug->cdata->current_palette;
              int pal = best_palette_match(dplug->cdata->palettes, -1, target_palette);
              if (pal != opal) {
                dplug->cdata->current_palette = pal;
                if (!(*dplug->dpsys->set_palette)(dplug->cdata)) {
                  dplug->cdata->current_palette = opal;
                  (*dplug->dpsys->set_palette)(dplug->cdata);
                } else if (dplug->cdata->rec_rowstrides) {
                  lives_free(dplug->cdata->rec_rowstrides);
                  dplug->cdata->rec_rowstrides = NULL;
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*

          width = dplug->cdata->width / weed_palette_get_pixels_per_macropixel(dplug->cdata->current_palette);
          height = dplug->cdata->height;

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
            if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
#ifdef USE_REC_RS
            weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
#endif
            pixel_data = weed_layer_get_pixel_data_planar(layer, NULL);
          } else {
            if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
#ifdef USE_REC_RS
            weed_leaf_clear_flagbits(layer, WEED_LEAF_ROWSTRIDES, LIVES_FLAG_MAINTAIN_VALUE);
#endif
            create_blank_layer(layer, image_ext, width, height, target_palette);
            weed_layer_unref(layer);
            return FALSE;
          }

          if (!pixel_data || !pixel_data[0]) {
            char *msg = lives_strdup_printf("NULL pixel data for layer size %d X %d, palette %s\n", width, height,
                                            weed_palette_get_name_full(dplug->cdata->current_palette,
                                                dplug->cdata->YUV_clamping, dplug->cdata->YUV_subspace));
            LIVES_WARN(msg);
            lives_free(msg);
            if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
            create_blank_layer(layer, image_ext, width, height, target_palette);
            weed_layer_unref(layer);
            return FALSE;
          }

          rowstrides = weed_layer_get_rowstrides(layer, NULL);

          if (!(*dplug->dpsys->get_frame)(dplug->cdata, (int64_t)iframe, rowstrides, sfile->vsize, pixel_data)) {
            if (prefs->show_dev_opts) {
              g_print("Error loading frame %d (index value %d)\n", frame,
                      sfile->frame_index[frame - 1]);
              break_me("load error");
            }
            if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
            need_unlock = FALSE;
#ifdef USE_REC_RS
            if (dplug->cdata->rec_rowstrides) lives_memset(dplug->cdata->rec_rowstrides, 0, nplanes * sizint);
#endif
            // if get_frame fails, return a black frame
            if (!is_thread) {
              weed_layer_clear_pixel_data(layer);
              weed_layer_unref(layer);
              return FALSE;
            }
            res = FALSE;
          } else if (need_unlock) pthread_mutex_unlock(&sfile->frame_index_mutex);
          //g_print("ACT %f EST %f\n",  (double)(lives_get_current_ticks() - timex) / TICKS_PER_SECOND_DBL, est_time);

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
          weed_layer_unref(layer);
          return res;
        }
      } else {
        // pull frame from decoded images
        int64_t timex = lives_get_current_ticks();
        double img_decode_time;
        boolean ret;
        char *fname;
#ifdef USE_RESTHREAD
        lives_proc_thread_t resthread;
#endif
        pthread_mutex_unlock(&sfile->frame_index_mutex);
        if (!image_ext || !*image_ext) image_ext = get_image_ext_for_type(sfile->img_type);
        fname = make_image_file_name(sfile, frame, image_ext);
        ret = weed_layer_create_from_file_progressive(layer, fname, width, height, target_palette, image_ext);

#ifdef USE_RESTHREAD
        if ((resthread = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) {
          lives_proc_thread_join(resthread);
          weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL);
        }
#endif

        lives_free(fname);
        if (!ret) {
          weed_layer_clear_pixel_data(layer);
          weed_leaf_delete(layer, WEED_LEAF_CURRENT_PALETTE);
          create_blank_layer(layer, image_ext, width, height, target_palette);
          weed_layer_unref(layer);
          return FALSE;
        }
        img_decode_time = (double)(lives_get_current_ticks() - timex) / TICKS_PER_SECOND_DBL;
        if (!sfile->img_decode_time) sfile->img_decode_time = img_decode_time;
        else sfile->img_decode_time = (sfile->img_decode_time + img_decode_time) / 2.;
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
    weed_layer_unref(layer);
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
    weed_layer_unref(layer);
    return TRUE;
#endif
  case CLIP_TYPE_LIVES2LIVES:
    weed_layer_set_from_lives2lives(layer, clip, (lives_vstream_t *)sfile->ext_src);
    weed_layer_unref(layer);
    return TRUE;
  case CLIP_TYPE_GENERATOR: {
    // special handling for clips where host controls size
    // Note: vlayer is actually the out channel of the generator, so we should
    // never free it !
    weed_plant_t *inst;
    weed_instance_ref((inst = (weed_plant_t *)sfile->ext_src));
    if (inst) {
      int key = weed_get_int_value(inst, WEED_LEAF_HOST_KEY, NULL);
      filter_mutex_lock(key);
      if (!IS_VALID_CLIP(clip)) {
        // gen was switched off
        create_blank_layer(layer, image_ext, width, height, target_palette);
      } else {
        vlayer = weed_layer_create_from_generator(inst, tc, clip);
        weed_layer_copy(layer, vlayer); // layer is non-NULL, so copy by reference
        weed_layer_nullify_pixel_data(vlayer);
      }
      filter_mutex_unlock(key);
      weed_instance_unref(inst);
    } else {
      create_blank_layer(layer, image_ext, width, height, target_palette);
    }
  }
  weed_layer_unref(layer);
  return TRUE;
  default:
    create_blank_layer(layer, image_ext, width, height, target_palette);
    weed_layer_unref(layer);
    return FALSE;
  }

  if (!is_thread) {
    // render subtitles from file
    if (prefs->show_subtitles && sfile->subt && sfile->subt->tfile > 0) {
      double xtime = (double)(frame - 1) / sfile->fps;
      xlayer = render_subs_from_file(sfile, xtime, layer);
      if (xlayer == layer) xlayer = NULL;
    }
  }

  weed_layer_unref(layer);
  if (xlayer) layer = xlayer;
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
   This function should always be called for threaded layers, prior to freeing the layer, reading it's  properties, pixel data,
   resizing etc.

   We may also deinterlace and overlay subs here
   for the blend layer, we may also resize, convert palette, apply gamma in preparation for combining with the main layer

   if effects were applied then the frame_layer can depend on other layers, however
   these will have been checked already when the effects were applied

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
#ifdef USE_RESTHREAD
  lives_proc_thread_t resthread;
#endif
  if (!layer) return FALSE;

  thrd = (lives_thread_t *)weed_get_voidptr_value(layer, WEED_LEAF_HOST_PTHREAD, NULL);
  if (thrd) {
    ready = FALSE;
    lives_thread_join(*thrd, NULL);
    weed_leaf_delete(layer, WEED_LEAF_HOST_PTHREAD);
    lives_free(thrd);
  }

#ifdef USE_RESTHREAD
  if ((resthread = weed_get_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL))) {
    ready = FALSE;
    lives_proc_thread_join(resthread);
    weed_set_voidptr_value(layer, WEED_LEAF_RESIZE_THREAD, NULL);
  }
#endif

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
  weed_set_boolean_value(layer, LIVES_LEAF_THREAD_PROCESSING, WEED_FALSE);
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
        if (!sfile->n_altsrcs) {
          sfile->alt_srcs = lives_calloc(1, sizeof(void *));
          sfile->alt_srcs[0] = clone_decoder(clip);
          if (sfile->alt_srcs[0]) {
            weed_set_int_value(layer, LIVES_LEAF_ALTSRC, 0);
            sfile->n_altsrcs;
            sfile->alt_src_types = lives_calloc(1, sizint);
            sfile->alt_src_types[0] = LIVES_EXT_SRC_DECODER;
	    // *INDENT-OFF*
	  }}}}}
#endif
  // *INDENT-ON*
  weed_set_boolean_value(layer, LIVES_LEAF_THREAD_PROCESSING, WEED_TRUE);
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
// *INDENT-ON*
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
    lives_nanosleep(LIVES_QUICK_NAP);
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
void *lives_pixbuf_save_threaded(void *args) {
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
      if (cairo_surface_get_reference_count(cfile->laudio_drawable))
        lives_painter_surface_destroy(cfile->laudio_drawable);
      cfile->laudio_drawable = NULL;
    }
    if (cfile->raudio_drawable) {
      if (mainw->raudio_drawable == cfile->raudio_drawable
          || mainw->drawsrc == mainw->current_file) mainw->raudio_drawable = NULL;
      if (cairo_surface_get_reference_count(cfile->raudio_drawable))
        lives_painter_surface_destroy(cfile->raudio_drawable);
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

    if (mainw->current_file == mainw->blend_file) {
      need_new_blend_file = TRUE;
      // set blend_file to -1. This in case the file is a generator - we need to distinguish between the cases where
      // the generator is the blend file and we switch because it was deinited, and when we switch fg <-> bg
      // in the former case the generator is killed off, in the latter it survives
      track_decoder_free(1, mainw->blend_file);
      mainw->blend_file = -1;
      weed_layer_free(mainw->blend_layer);
      mainw->blend_layer = NULL;
    }

    if (CURRENT_CLIP_IS_PHYSICAL && (cfile->ext_src || cfile->old_ext_src)) {
      if (!cfile->ext_src && cfile->old_ext_src_type == LIVES_EXT_SRC_DECODER) {
        cfile->ext_src = cfile->old_ext_src;
        cfile->ext_src_type = cfile->old_ext_src_type;
      }
      if (cfile->ext_src_type == LIVES_EXT_SRC_DECODER) {
        close_clip_decoder(mainw->current_file);
      }
    }
    free_thumb_cache(mainw->current_file, 0);
    lives_freep((void **)&cfile->frame_index);
    lives_freep((void **)&cfile->frame_index_back);

    if (cfile->clip_type != CLIP_TYPE_GENERATOR && !mainw->close_keep_frames) {
      char *clipd = get_clip_dir(mainw->current_file);
      if (lives_file_test(clipd, LIVES_FILE_TEST_EXISTS)) {
        // as a safety feature we create a special file which allows the back end to delete the directory
        char *temp_backend;
        char *permitname = lives_build_filename(clipd, TEMPFILE_MARKER "." LIVES_FILE_EXT_TMP, NULL);
        lives_touch(permitname);
        lives_free(permitname);
        temp_backend = use_staging_dir_for(mainw->current_file);
        com = lives_strdup_printf("%s close \"%s\"", temp_backend, cfile->handle);
        lives_free(temp_backend);
        lives_system(com, TRUE);
        lives_free(com);
        temp_backend = lives_build_path(LIVES_DEVICE_DIR, LIVES_SHM_DIR, cfile->handle, NULL);
        if (!lives_file_test(temp_backend, LIVES_FILE_TEST_IS_DIR)) {
          lives_rmdir_with_parents(temp_backend);
        }
        lives_free(temp_backend);
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
      if (cfile->ext_src) lives_vdev_free((lives_vdev_t *)cfile->ext_src);
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

    if ((mainw->first_free_file == ALL_USED || mainw->first_free_file > mainw->current_file) && mainw->current_file > 0)
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

    mainw->preview_frame = 0;

    if (!mainw->only_close) {
      // find another clip to switch to
      if (index > -1) {
        if (!mainw->multitrack) {
          if (!LIVES_IS_PLAYING) {
            switch_clip(1, index, TRUE);
            d_print("");
          } else mainw->new_clip = index;
          if (need_new_blend_file) {
            mainw->blend_file = mainw->current_file;
          }
        } else {
          if (old_file != mainw->multitrack->render_file) {
            mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
            mt_clip_select(mainw->multitrack, TRUE);
          }
        }
        return;
      }
      if (mainw->clips_available > 0) {
        for (i = mainw->current_file; --i;) {
          if (mainw->files[i]) {
            if (!mainw->multitrack) {
              if (!LIVES_IS_PLAYING) {
                switch_clip(1, i, TRUE);
                d_print("");
              } else mainw->new_clip = index;
              if (need_new_blend_file) {
                mainw->blend_file = mainw->current_file;
              }
            } else {
              if (old_file != mainw->multitrack->render_file) {
                mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
                mt_clip_select(mainw->multitrack, TRUE);
              }
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
              if (need_new_blend_file) {
                track_decoder_free(1, mainw->blend_file);
                mainw->blend_file = mainw->current_file;
              }
            } else {
              if (old_file != mainw->multitrack->render_file) {
                mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
                mt_clip_select(mainw->multitrack, TRUE);
              }
            }
            return;
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*

  // no other clips
  mainw->current_file = mainw->blend_file = -1;
  set_main_title(NULL, 0);

  if (mainw->play_window) {
    lives_widget_hide(mainw->preview_controls);
    load_preview_image(FALSE);
    resize_play_window();
  }
  lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
  lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
  lives_widget_set_sensitive(mainw->export_proj, FALSE);
  lives_widget_set_sensitive(mainw->import_proj, FALSE);

  if (mainw->multitrack && old_file != mainw->multitrack->render_file)
    lives_widget_set_sensitive(mainw->multitrack->load_set, TRUE);

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
      if (old_file != mainw->multitrack->render_file) {
        mainw->multitrack->clip_selected = -mainw->multitrack->clip_selected;
        mt_clip_select(mainw->multitrack, TRUE);
      }
    }
  }
  if (!LIVES_IS_PLAYING && !mainw->is_processing && !mainw->preview) {
    if (mainw->multitrack) {
      if (old_file != mainw->multitrack->render_file) {
        mt_sensitise(mainw->multitrack);
      }
    } else sensitize();
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

  if (!LIVES_IS_PLAYING) {
    if (old_file != new_file) {
      if (CURRENT_CLIP_IS_VALID) {
        mainw->laudio_drawable = cfile->laudio_drawable;
        mainw->raudio_drawable = cfile->raudio_drawable;
        mainw->drawsrc = mainw->current_file;
      }
    }
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
          lives_freep((void **)&mainw->proc_ptr->text);
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

  if (!mainw->clip_switched && !cfile->opening) sensitize();

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

  set_start_end_spins(mainw->current_file);

  resize(1);
  if (!mainw->go_away) {
    get_play_times();
  }

  // if the file was opening, continue...
  if (cfile->opening) {
    open_file(cfile->file_name);
  } else {
    showclipimgs();
    //lives_widget_context_update();
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


boolean switch_audio_clip(int new_file, boolean activate) {
  //ticks_t cticks;
  lives_clip_t *sfile;
  int aplay_file;

  if (AUD_SRC_EXTERNAL) return FALSE;

  aplay_file = get_aplay_clipno();

  if ((aplay_file == new_file && (!(prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
                                  || !CLIP_HAS_AUDIO(aplay_file)))
      || (aplay_file != new_file && !(prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS))) {
    return FALSE;
  }

  sfile = RETURN_VALID_CLIP(new_file);
  if (!sfile) return FALSE;

  if (new_file == aplay_file) {
    if (sfile->pb_fps > 0. || (sfile->play_paused && sfile->freeze_fps > 0.))
      sfile->adirection = LIVES_DIRECTION_FORWARD;
    else sfile->adirection = LIVES_DIRECTION_REVERSE;
  } else if (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP) {
    mainw->scratch = SCRATCH_JUMP;
  }

  if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    if (mainw->jackd) {
      if (!activate) mainw->jackd->in_use = FALSE;

      if (new_file != aplay_file) {
        if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
          mainw->cancelled = handle_audio_timeout();
          return FALSE;
        }
        if (mainw->jackd->playing_file > 0) {
          if (!CLIP_HAS_AUDIO(new_file)) {
            jack_get_rec_avals(mainw->jackd);
            mainw->rec_avel = 0.;
          }
          jack_message.command = ASERVER_CMD_FILE_CLOSE;
          jack_message.data = NULL;
          jack_message.next = NULL;
          mainw->jackd->msgq = &jack_message;

          if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
            mainw->cancelled = handle_audio_timeout();
            return FALSE;
          }
	  // *INDENT-OFF*
	}}}
    // *INDENT-ON*

    if (!IS_VALID_CLIP(new_file)) {
      mainw->jackd->in_use = FALSE;
      return FALSE;
    }

    if (new_file == aplay_file) return TRUE;

    if (CLIP_HAS_AUDIO(new_file) && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
      // tell jack server to open audio file and start playing it

      jack_message.command = ASERVER_CMD_FILE_OPEN;

      jack_message.data = lives_strdup_printf("%d", new_file);

      jack_message2.command = ASERVER_CMD_FILE_SEEK;

      jack_message.next = &jack_message2;
      jack_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
      jack_message2.next = NULL;

      mainw->jackd->msgq = &jack_message;

      mainw->jackd->in_use = TRUE;

      if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
        mainw->cancelled = handle_audio_timeout();
        return FALSE;
      }

      //jack_time_reset(mainw->jackd, 0);

      mainw->jackd->is_paused = sfile->play_paused;
      mainw->jackd->is_silent = FALSE;
    } else {
      mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    }
#endif
  }

  if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed) {

      if (!activate) mainw->pulsed->in_use = FALSE;

      if (new_file != aplay_file) {
        if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
          mainw->cancelled = handle_audio_timeout();
          return FALSE;
        }

        if (IS_PHYSICAL_CLIP(aplay_file)) {
          lives_clip_t *afile = mainw->files[aplay_file];
          if (!CLIP_HAS_AUDIO(new_file)) {
            pulse_get_rec_avals(mainw->pulsed);
            mainw->rec_avel = 0.;
          }
          pulse_message.command = ASERVER_CMD_FILE_CLOSE;
          pulse_message.data = NULL;
          pulse_message.next = NULL;
          mainw->pulsed->msgq = &pulse_message;

          if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) {
            mainw->cancelled = handle_audio_timeout();
            return FALSE;
          }

          afile->sync_delta = lives_pulse_get_time(mainw->pulsed) - mainw->startticks;
          afile->aseek_pos = mainw->pulsed->seek_pos;
        }

        if (!IS_VALID_CLIP(new_file)) {
          mainw->pulsed->in_use = FALSE;
          return FALSE;
        }

        if (new_file == aplay_file) return TRUE;

        if (CLIP_HAS_AUDIO(new_file) && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
          // tell pulse server to open audio file and start playing it

          mainw->video_seek_ready = FALSE;

          pulse_message.command = ASERVER_CMD_FILE_OPEN;
          pulse_message.data = lives_strdup_printf("%d", new_file);

          pulse_message2.command = ASERVER_CMD_FILE_SEEK;
          /* if (LIVES_IS_PLAYING && !mainw->preview) { */
          /*   pulse_message2.tc = cticks; */
          /*   pulse_message2.command = ASERVER_CMD_FILE_SEEK_ADJUST; */
          /* } */
          pulse_message.next = &pulse_message2;
          pulse_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
          pulse_message2.next = NULL;

          mainw->pulsed->msgq = &pulse_message;

          mainw->pulsed->is_paused = sfile->play_paused;

          //pa_time_reset(mainw->pulsed, 0);
        } else {
          mainw->video_seek_ready = TRUE;
          video_sync_ready();
        }
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
      if (!sfile->play_paused)
        nullaudio_arate_set(sfile->arate * sfile->pb_fps / sfile->fps);
      else nullaudio_arate_set(sfile->arate * sfile->freeze_fps / sfile->fps);
    } else nullaudio_arate_set(sfile->arate);
    nullaudio_seek_set(sfile->aseek_pos);
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
  lives_clip_t *sfile = NULL;
  boolean osc_block;
  int old_file = mainw->playing_file;

  if (!LIVES_IS_PLAYING) {
    switch_to_file(mainw->current_file, new_file);
    return;
  }

  if (old_file == new_file || old_file < 1 || !IS_VALID_CLIP(new_file)) return;

  if (mainw->multitrack
      || (mainw->record && !mainw->record_paused && !(prefs->rec_opts & REC_CLIPS)) ||
      mainw->foreign || (mainw->preview && !mainw->is_rendering)) return;

  if (mainw->noswitch) {
    mainw->new_clip = new_file;
    return;
  }

  if (IS_VALID_CLIP(old_file)) sfile = mainw->files[old_file];

  mainw->whentostop = NEVER_STOP;
  mainw->blend_palette = WEED_PALETTE_END;

  if (old_file != mainw->blend_file && !mainw->is_rendering) {
    if (sfile && sfile->clip_type == CLIP_TYPE_GENERATOR && sfile->ext_src) {
      if (new_file != mainw->blend_file) {
        // switched from generator to another clip, end the generator
        weed_plant_t *inst = (weed_plant_t *)sfile->ext_src;
        mainw->gen_started_play = FALSE;
        weed_generator_end(inst);
      } else {
        // switch fg to bg
        rte_swap_fg_bg();
      }
    }

    if (new_file == mainw->blend_file && mainw->files[mainw->blend_file]->clip_type == CLIP_TYPE_GENERATOR
        && mainw->files[mainw->blend_file]->ext_src) {
      // switch bg to fg
      rte_swap_fg_bg();
    }
  }

  if (mainw->playing_file == mainw->blend_file)
    track_decoder_free(1, mainw->blend_file);

  osc_block = mainw->osc_block;
  mainw->osc_block = TRUE;

  if (mainw->loop_locked) {
    unlock_loop_lock();
  }

  mainw->clip_switched = TRUE;

  if (mainw->frame_layer) {
    check_layer_ready(mainw->frame_layer);
    weed_layer_free(mainw->frame_layer);
    mainw->frame_layer = NULL;
  }
  if (mainw->blend_layer) {
    check_layer_ready(mainw->blend_layer);
    weed_layer_free(mainw->blend_layer);
    mainw->blend_layer = NULL;
  }

  mainw->current_file = new_file;

  mainw->drawsrc = mainw->current_file;
  mainw->laudio_drawable = cfile->laudio_drawable;
  mainw->raudio_drawable = cfile->raudio_drawable;

  lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), mainw->files[new_file]->pb_fps);
  lives_spin_button_update(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps));
  lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);

  // switch audio clip
  if (AUD_SRC_INTERNAL && (!mainw->event_list || mainw->record)) {
    if (APLAYER_REALTIME && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
        && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS)
        && !mainw->is_rendering && (mainw->preview || !(mainw->agen_key != 0 || mainw->agen_needs_reinit))) {
      switch_audio_clip(new_file, TRUE);
    }
  }

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

  if (cfile->last_play_sequence != mainw->play_sequence) {
    cfile->frameno = calc_frame_from_time(mainw->current_file, cfile->pointer_time);
  }

  mainw->playing_file = new_file;

  changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), LIVES_INT_TO_POINTER(1));

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
      set_start_end_spins(mainw->current_file);

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
    if (mainw->blend_layer) {
      check_layer_ready(mainw->blend_layer);
      weed_layer_free(mainw->blend_layer);
      mainw->blend_layer = NULL;
    }
    track_decoder_free(1, mainw->blend_file);
    mainw->blend_file = old_file;
  }

  if (mainw->play_window && prefs->show_playwin) {
    lives_window_present(LIVES_WINDOW(mainw->play_window));
    lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
  }

  mainw->osc_block = osc_block;
  lives_ruler_set_upper(LIVES_RULER(mainw->hruler), CURRENT_CLIP_TOTAL_TIME);

  redraw_timeline(mainw->current_file);
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
  bx = 2 * abs(bx);
  by = abs(by);

  if (!mainw->hdrbar) {
    if (prefs->open_maximised && by > MENU_HIDE_LIM)
      lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);
  }

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

  hsize = (scr_width - (H_RESIZE_ADJUST * 3 + bx)) / 3;
  vsize = scr_height - (CE_TIMELINE_VSPACE * 1.01 / sqrt(widget_opts.scale) + vspace + by
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
