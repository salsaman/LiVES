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

#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#endif

#ifdef LIVES_OS_UNIX
#include <glib-unix.h>
#endif

//#define WEED_STARTUP_TESTS
////////////////////////////////
//// externs - 'global' variables

void *(*lives_malloc)(size_t);
void (*lives_free)(void *);
void *(*lives_calloc)(size_t, size_t);

_palette *palette;
ssize_t sizint, sizdbl, sizshrt;
mainwindow *mainw;

pthread_t main_thread = 0;

capabilities *capable;
//////////////////////////////////////////
static _ign_opts ign_opts;

static int zargc;
static char **zargv;

static int o_argc;
static char **o_argv;

int orig_argc(void) {return o_argc;}
char **orig_argv(void) {return o_argv;}

#ifdef WEED_STARTUP_TESTS
extern int run_weed_startup_tests(void);
#endif

/////////////////////////////////
#ifdef NO_COMPILE // never compile this
void tr_msg(void) {
  // TARNSLATORS: do not translate this message
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
boolean ret = TRUE;
#endif
static char errdets[512], errmsg[512];

void defer_sigint(int signum) {
  // here we can prepare for and handle specific fn calls which we know may crash / abort
  // provided if we know ahead of time.
  // The main reason would be to show an error dialog and then exit, or transmit some error code first,
  // rather than simply doing nothing and aborting /exiting.
  // we should do the minimum necessary and exit, as the stack may be corrupted.

  if (mainw->err_funcdef) lives_snprintf(errdets, 512, " in function %s, %s line %d", mainw->err_funcdef->funcname,
                                           mainw->err_funcdef->file, mainw->err_funcdef->line);

  lives_snprintf(errmsg, 512, "received signal %d at tracepoint (%d), %s\n", signum, mainw->crash_possible,
                 *errdets ? errdets : NULL);

  LIVES_ERROR_NOBRK(errmsg);

  switch (mainw->crash_possible) {
#ifdef ENABLE_JACK
  case 1:
    if (mainw->jackd_trans) {
      lives_snprintf(errmsg, 512, "Connection attempt timed out, aborting.");
      jack_log_errmsg(mainw->jackd_trans, errmsg);
    }
    while (ret) {
      // crash in jack_client_open() con - transport
      ret = jack_warn(TRUE, TRUE);
    }
    break;
  case 2:
    if (mainw->jackd) {
      lives_snprintf(errmsg, 512, "Connection attempt timed out, aborting.");
      jack_log_errmsg(mainw->jackd, errmsg);
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
  case 16: // crash getting cdata from decoder plugin


    break;
#endif
  default:
    break;
  }
  if (signum > 0) {
    signal(signum, SIG_DFL);
    pthread_detach(pthread_self());
  } else lives_abort(errmsg);
}


boolean defer_sigint_cb(lives_obj_t *obj, void *pdtl) {
  int crpos = mainw->crash_possible;
  mainw->crash_possible = LIVES_POINTER_TO_INT(pdtl);
  defer_sigint(-1);
  mainw->crash_possible = crpos;
  return FALSE;
}


//#define QUICK_EXIT
void catch_sigint(int signum) {
  // trap for ctrl-C and others
  //if (mainw->jackd) lives_jack_end();

  if (!pthread_equal(main_thread, pthread_self())) {
    // if we are not the main thread, just exit
    GET_PROC_THREAD_SELF(self);
    g_print("Signal %d received by thread ", signum);
    lives_thread_data_t *mydata = get_thread_data();
    char *tnum = get_thread_id(mydata->vars.var_uid);
    g_print("%s\n", tnum);
    //if (mydata) {
    lives_proc_thread_set_signalled(self, signum, mydata);
    pthread_detach(pthread_self());
  }

  //#ifdef QUICK_EXIT
  /* shoatend(); */
  /* fprintf(stderr, "shoatt end"); */
  /* fflush(stderr); */
  if (!mainw) exit(signum);

  //#endif
  lives_hooks_trigger(mainw->global_hook_stacks, FATAL_HOOK);

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

  exit(signum);
}

#ifdef USE_GLIB
static boolean glib_sighandler(livespointer data) {
  int signum = LIVES_POINTER_TO_INT(data);
  catch_sigint(signum);
  return TRUE;
}
#endif

void set_signal_handlers(SignalHandlerPointer sigfunc) {
  sigset_t smask;
  struct sigaction sact;

  memset(errmsg, 0, 512);
  memset(errdets, 0, 512);

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
  o_argc = argc;
  o_argv = argv;

  mainw = NULL;
  prefs = NULL;
  capable = NULL;

#ifdef GDK_WINDOWING_X11
  XInitThreads();
#endif

  init_memfuncs(0);

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

#ifdef IS_LIBLIVES
#ifdef GUI_GTK
  if (gtk_thread) {
    pthread_create(gtk_thread, NULL, gtk_thread_wrapper, NULL);
  }
#endif
#endif

  capable = (capabilities *)lives_calloc(1, sizeof(capabilities));
  capable->hw.cacheline_size = sizeof(void *) * 8;

  // _runtime_ byte order, needed for lives_strlen and other things
  if (IS_BIG_ENDIAN)
    capable->hw.byte_order = LIVES_BIG_ENDIAN;
  else
    capable->hw.byte_order = LIVES_LITTLE_ENDIAN;

  main_thread = pthread_self();
  capable->main_thread = pthread_self();
  capable->gui_thread = pthread_self();

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

  run_the_program(argc, argv, gtk_thread, id);

  return 0;
}


#ifdef USE_GAPPL
static int gappquick(GApplication *application, GVariantDict *options,
                     livespointer user_data) {
  if (g_variant_dict_contains(options, "zzz")) {
    print_notice();
    return 0;
  }
  return -1;
}


static int gappcmd(GApplication *application, GApplicationCommandLine *cmdline,
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

