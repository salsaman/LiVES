// main.h
// LiVES
// (c) G. Finch (salsaman+lives@gmail.com) 2003 - 2020
// see file ../COPYING for full licensing details

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 or higher as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
*/

// begin legal warning
/*
  NO WARRANTY

  BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
  FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
  OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
  PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
  OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
  TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
  PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
  REPAIR OR CORRECTION.

  IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
  WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
  REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
  INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
  OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
  TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
  YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
  PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGES.
*/
// end legal warning

// Have fun, and let's fight for Free Speech, Open Media and True Creativity !
// - Salsaman

// note: preferred formatting style is: astyle --style=java -H -Y -s2 -U -k3 -W3 -xC128 -xL -p -o -O -Q -xp

#ifndef HAS_LIVES_MAIN_H
#define HAS_LIVES_MAIN_H 1

#include <sys/types.h>
#include <string.h>

#include <sys/file.h>
#include <unistd.h>

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#include <setjmp.h>

#define NEED_ENDIANTEST 0
#include "defs.h"

#define WEED_ADVANCED_PALETTES 1

#if NEED_LOCAL_WEED
#include "../libweed/weed-host.h"
#include "../libweed/weed.h"
#include "../libweed/weed-events.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host-utils.h"
#else

#include <weed/weed-host.h>
#include <weed/weed.h>
#include <weed/weed-events.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#if NEED_LOCAL_WEED_UTILS
#include "../libweed/weed-utils.h"
#else
#include <weed/weed-utils.h>
#endif
#if NEED_LOCAL_WEED_HOST_UTILS
#include "../libweed/weed-host-utils.h"
#else
#include <weed/weed-host-utils.h>
#endif
#endif

#ifdef USE_SWSCALE
// for weed-compat.h
#define HAVE_AVCODEC
#define HAVE_AVUTIL
#endif

#define NEED_FOURCC_COMPAT

#ifdef NEED_LOCAL_WEED_COMPAT
#include "../libweed/weed-compat.h"
#else
#include <weed/weed-compat.h>
#endif

weed_leaf_get_f _weed_leaf_get;
weed_leaf_set_f _weed_leaf_set;
#if WEED_ABI_CHECK_VERSION(202)
weed_ext_set_element_size_f _weed_ext_set_element_size;
weed_ext_append_elements_f _weed_ext_append_elements;
weed_ext_attach_leaf_f _weed_ext_attach_leaf;
weed_ext_detach_leaf_f _weed_ext_detach_leaf;
#endif
weed_plant_new_f _weed_plant_new;
weed_plant_free_f _weed_plant_free;
#if DEBUG_PLANTS
weed_plant_new_f Xweed_plant_new;
weed_plant_free_f Xweed_plant_free;
#endif
weed_plant_list_leaves_f _weed_plant_list_leaves;
weed_leaf_num_elements_f _weed_leaf_num_elements;
weed_leaf_element_size_f _weed_leaf_element_size;
weed_leaf_seed_type_f _weed_leaf_seed_type;
weed_leaf_get_flags_f _weed_leaf_get_flags;
weed_leaf_set_flags_f _weed_leaf_set_flags;
weed_leaf_delete_f _weed_leaf_delete;

#ifndef IGN_RET
#define IGN_RET(a) ((void)((a) + 1))
#endif

#define EXPECTED(x) __builtin_expect((x), 1)
#define UNEXPECTED(x) __builtin_expect((x), 0)

#define LIVES_UNLIKELY(a) UNEXPECTED(a)
#define LIVES_LIKELY(a) EXPECTED(a)

#define ADD_AUDIT(audt, ptr, vtype, val) do {				\
    char *pkey;								\
    if (!auditor_##audt) auditor_##audt = lives_plant_new(LIVES_WEED_SUBTYPE_AUDIT); \
    pkey = lives_strdup_printf("%p", (ptr));				\
    weed_set_##vtype##_value(auditor_##audt, pkey, (val));		\
    lives_free(pkey);} while (0);

#define REMOVE_AUDIT(audt, ptr) do {					\
    if (auditor_##audt) {						\
      char *pkey = lives_strdup_printf("%p", ptr);			\
      weed_leaf_delete(auditor_##audt, pkey);				\
      lives_free(pkey);}} while (0);

#define AUDIT_REFC 1
#if AUDIT_REFC
extern weed_plant_t *auditor_refc;
#endif

typedef weed_plant_t weed_layer_t;
typedef weed_plant_t lives_layer_t;
typedef weed_plant_t weed_param_t;

typedef struct _capabilities capabilities;
extern capabilities *capable;

extern pthread_t main_thread;

#include "support.h"

#include "widget-helper.h"

#include "user-interface.h"

#include "filesystem.h"

/// install guidance flags
#define INSTALL_CANLOCAL (1ul << 0)
#define INSTALL_IMPORTANT (1ul << 1)

#define IS_ALTERNATIVE (1ul << 16)

typedef enum {
  MISSING = -1, ///< not yet implemented (TODO)
  UNCHECKED = 0,
  PRESENT,
  LOCAL,	// user compiled code
  INTERNAL,	// executable replaced by internal function
} lives_checkstatus_t;

#define XCAPABLE(foo, EXE_FOO) \
  (capable->has_##foo->present == INTERNAL ? PRESENT :  ((capable->has_##foo->present == UNCHECKED \
							  ? ((capable->has_##foo->present = \
							      (has_executable(EXE_FOO) ? PRESENT : MISSING))) : \
							  capable->has_##foo->present) == PRESENT))
#define GET_EXE(foo) QUOTEME(foo)
#define PRESENT(foo) (XCAPABLE(foo, GET_EXE(foo)) == PRESENT)
#define MISSING(foo) (XCAPABLE(foo, GET_EXE(foo)) == MISSING)

#define IS_PRESENT(item) (capable->has_##item == PRESENT)
#define IS_MISSING(item) (capable->has_##item == MISSING)
#define IS_UNCHECKED(item) (capable->has_##item == UNCHECKED)
#define IS_LOCAL(item) (capable->has_##item == LOCAL)
#define IS_INTERNAL(item) (capable->has_##item == INTERNAL)

#define ARE_PRESENT(item) IS_PRESENT(item)
#define ARE_MISSING(item) IS_MISSING(item)
#define ARE_UNCHECKED(item) IS_UNCHECKED(item)

#define IS_AVAILABLE(item) (IS_PRESENT(item) || IS_LOCAL(item) || IS_INTERNAL(item))

#define CHECK_AVAILABLE(item, EXEC) (IS_UNCHECKED(item) ? (((capable->has_##item = has_executable(EXEC)) \
							    == PRESENT || IS_LOCAL(item)) ? TRUE : FALSE) \
				     : IS_AVAILABLE(item))
typedef struct {
  char wm_name[64];
  uint64_t ver_major;
  uint64_t ver_minor;
  uint64_t ver_micro;

  LiVESXWindow *root_window;
  boolean is_composited;

  char *wm_focus;

#define ANNOY_DISPLAY      	(1ull << 0)
#define ANNOY_DISK		(1ull << 1)
#define ANNOY_PROC		(1ull << 2)
#define ANNOY_NETWORK		(1ull << 3)
#define ANNOY_SOUNDS		(1ull << 4)
#define ANNOY_DEV		(1ull << 5)
#define ANNOY_OTHER		(1ull << 6)

#define ANNOY_FS		(1ull << 32)
#define ANNOY_CONT		(1ull << 33)
#define ANNOY_PERIOD		(1ull << 34)
#define ANNOY_SPONT		(1ull << 35)
#define ANNOY_TIMED		(1ull << 36)
#define ANNOY_LOCK		(1ull << 37)

#define RES_HIDE		(1ull << 0)
#define RES_SUSPEND		(1ull << 1)
#define RES_STOP		(1ull << 2)
#define RES_BLOCK		(1ull << 3)
#define RES_MUTE		(1ull << 4)

#define RESTYPE_ACTION		(1ull << 16)
#define RESTYPE_CONFIG		(1ull << 17)
#define RESTYPE_SIGNAL		(1ull << 18)
#define RESTYPE_CMD		(1ull << 19)
#define RESTYPE_LOCKOUT		(1ull << 20)
#define RESTYPE_TIMED		(1ull << 21)
#define RESTYPE_MONITOR		(1ull << 22)

  char panel[64];
  uint64_t pan_annoy;
  uint64_t pan_res;
  char ssave[64];
  uint64_t ssave_annoy;
  uint64_t ssave_res;
  char other[64];
  uint64_t oth_annoy;
  uint64_t oth_res;

  char color_settings[64];
  char display_settings[64];
  char ssv_settings[64];
  char pow_settings[64];
  char settings[64];
  char term[64];
  char taskmgr[64];
  char sshot[64];
} wm_caps_t;

#ifdef FINALISE_MEMFUNCS
#undef FINALISE_MEMFUNCS
#endif

#include "memory.h"

#include "lsd.h"

#include "stringfuncs.h"

#define _BASE_DEFS_ONLY_
#include "intents.h"
#ifdef  _BASE_DEFS_ONLY_
#undef _BASE_DEFS_ONLY_
#endif

#include "functions.h"
#include "alarms.h"
#include "lists.h"
#include "intents.h"
#include "maths.h"
#include "colourspace.h"
#include "cliphandler.h"
#include "plugins.h"

#include "layers.h"

#include "frameloader.h"
#include "machinestate.h"
#include "lsd-tab.h"

boolean weed_threadsafe;
int weed_abi_version;

#ifdef IS_LIBLIVES
#include "liblives.hpp"
#include "lbindings.h"
#endif

#define N_RECENT_FILES 16

/// which stream end should cause playback to finish ?
typedef enum {
  NEVER_STOP = 0,
  STOP_ON_VID_END,
  STOP_ON_AUD_END
} lives_whentostop_t;

/// cancel reason
typedef enum {
  /// no cancel
  CANCEL_NONE,

  /// user pressed stop
  CANCEL_USER,

  /// cancel but keep opening
  CANCEL_NO_PROPOGATE,

  /// effect processing finished during preview
  CANCEL_PREVIEW_FINISHED,

  /// application quit
  CANCEL_APP_QUIT,

  /// ran out of preview frames
  CANCEL_NO_MORE_PREVIEW,

  /// image could not be captured
  CANCEL_CAPTURE_ERROR,

  /// event_list completed
  CANCEL_EVENT_LIST_END,

  /// video playback completed
  CANCEL_VID_END,

  /// generator was stopped
  CANCEL_GENERATOR_END,

  /// external process ended (e.g. autolives uncheck)
  CANCEL_EXTERNAL_ENDED,

  /// user pressed 'Keep'
  CANCEL_KEEP,

  /// video playback completed
  CANCEL_AUD_END,

  /// cancelled because of error
  CANCEL_ERROR,

  /// cancelled because of soundcard error
  CANCEL_AUDIO_ERROR,

  /// cancelled and paused
  CANCEL_USER_PAUSED,

  /// an error occurred, retry the operation
  CANCEL_RETRY,

  /// software error: e.g set mainw->current_file directly during pb instead of mainw->new_clip
  CANCEL_INTERNAL_ERROR,

  /// special cancel for TV toy
  CANCEL_KEEP_LOOPING,
} lives_cancel_t;

typedef enum {
  CANCEL_KILL = 0, ///< normal - kill background processes working on current clip
  CANCEL_INTERRUPT,     ///< midway between KILL and SOFT
  CANCEL_SOFT     ///< just cancel in GUI (for keep, etc)
} lives_cancel_type_t;

#define IMG_TYPE_BEST IMG_TYPE_PNG

#include "pangotext.h"

extern const char *NO_COPY_LEAVES[];

#define WEED_LEAF_HOST_DEINTERLACE "host_deint" // frame needs deinterlacing
#define WEED_LEAF_HOST_TC "host_tc" // timecode for deinterlace

#define AV_TRACK_MIN_DIFF 0.001 ///< ignore track time differences < this (seconds)

#include "events.h"

/// some shared structures

#define USE_MPV (!capable->has_mplayer && !capable->has_mplayer2 && capable->has_mpv)
#define HAS_EXTERNAL_PLAYER (capable->has_mplayer || capable->has_mplayer2 || capable->has_mpv)

#include "threading.h"

#ifdef ENABLE_JACK
#include "jack.h"
#endif

#define USE_16BIT_PCONV

#include "paramspecial.h"
#include "mainwindow.h"
#include "keyboard.h"
#include "preferences.h"

extern mainwindow *mainw;

#define BACKEND_NAME EXEC_SMOGRIFY

#include "clip_load_save.h"

#include "player.h"
#include "sethandler.h"
#include "messaging.h"

/// type sizes
extern ssize_t sizint, sizdbl, sizshrt;

#include "setup.h"
#include "dialogs.h"
#include "player-control.h"
#include "gui.h"
#include "utils.h"

// capabilities (MUST come after plugins.h)
// TODO - move into capabilities.h

struct _capabilities {
  // the following can be assumed TRUE / PRESENT, they are checked on startup
  boolean smog_version_correct;
  boolean can_read_from_config;

  boolean can_write_to_config;
  boolean can_write_to_config_new;

  boolean can_write_to_config_backup;
  boolean can_write_to_workdir;

  lives_checkstatus_t has_smogrify;

  // MISSING if some subdirs not found
  lives_checkstatus_t has_plugins_libdir;

  // the following may need checking before use
  lives_checkstatus_t has_perl;
  lives_checkstatus_t has_file;
  lives_checkstatus_t has_dvgrab;
  lives_checkstatus_t has_sox_play;
  lives_checkstatus_t has_sox_sox;
  lives_checkstatus_t has_autolives;
  lives_checkstatus_t has_mplayer;
  lives_checkstatus_t has_mplayer2;
  lives_checkstatus_t has_mpv;
  lives_checkstatus_t has_convert;
  lives_checkstatus_t has_composite;
  lives_checkstatus_t has_identify;
  lives_checkstatus_t has_ffprobe;
  lives_checkstatus_t has_ffmpeg;
  lives_checkstatus_t has_cdda2wav;
  lives_checkstatus_t has_icedax;
  lives_checkstatus_t has_midistartstop;
  lives_checkstatus_t has_jackd;
  lives_checkstatus_t has_pulse_audio;
  lives_checkstatus_t has_xwininfo;
  lives_checkstatus_t has_gdb;
  lives_checkstatus_t has_gzip;
  lives_checkstatus_t has_rfx_builder;
  lives_checkstatus_t has_rfx_builder_multi;
  lives_checkstatus_t has_gconftool_2;
  lives_checkstatus_t has_gsettings;
  lives_checkstatus_t has_xdg_screensaver;
  lives_checkstatus_t has_xdg_open;
  lives_checkstatus_t has_xdg_mime;
  lives_checkstatus_t has_xdg_desktop_icon;
  lives_checkstatus_t has_xdg_desktop_menu;
  lives_checkstatus_t has_wmctrl;
  lives_checkstatus_t has_xdotool;
  lives_checkstatus_t has_youtube_dl;
  lives_checkstatus_t has_youtube_dlc;
  lives_checkstatus_t has_pip;
  lives_checkstatus_t has_du;
  lives_checkstatus_t has_md5sum;
  lives_checkstatus_t has_gio;
  lives_checkstatus_t has_wget;
  lives_checkstatus_t has_curl;
  lives_checkstatus_t has_mktemp;
  lives_checkstatus_t has_notify_send;
  lives_checkstatus_t has_snap;

  lives_checkstatus_t writeable_shmdir;

  /// home directory - default location for config file - locale encoding
  char home_dir[PATH_MAX];

  char backend_path[PATH_MAX];

  char shmdir_path[PATH_MAX];

  char *xdg_data_home; // e.g $HOME/.local/share
  char *xdg_current_desktop; // e.g XFCE
  char *xdg_runtime_dir; // e.g /run/user/$uid

  char touch_cmd[PATH_MAX];
  char rm_cmd[PATH_MAX];
  char mv_cmd[PATH_MAX];
  char cp_cmd[PATH_MAX];
  char df_cmd[PATH_MAX];
  char ln_cmd[PATH_MAX];
  char chmod_cmd[PATH_MAX];
  char cat_cmd[PATH_MAX];
  char grep_cmd[PATH_MAX];
  char sed_cmd[PATH_MAX];
  char wc_cmd[PATH_MAX];
  char echo_cmd[PATH_MAX];
  char eject_cmd[PATH_MAX];
  char rmdir_cmd[PATH_MAX];

  /// used for returning startup messages from the backend
  char startup_msg[1024];

  uint64_t uid; // non-volatile unique id (set in prefs)
  uint64_t session_uid; // uid for the session, changed each time we run

  lives_checkstatus_t has_python;
  lives_checkstatus_t has_python3;
  uint64_t python_version;

  char *myname_full;
  char *myname;

  pid_t mainpid;
  pthread_t main_thread;
  pthread_t gui_thread;

  char *username;

  mode_t umask;

  // TODO - move to hwcaps
  char *mach_name;
  int64_t boot_time;
  int xstdout;
  int nmonitors;
  int primary_monitor;
  boolean can_show_msg_area;

  int64_t ds_used, ds_free, ds_tot;
  lives_storage_status_t ds_status;

  hw_caps_t hw;

  // todo - move to gui_caps
  char *gui_theme_name;
  char *icon_theme_name;
  char *app_icon_path;
  char *extra_icon_path;
  LiVESList *all_icons;

  LiVESList *app_icons; // desktop icons for app

  char *def_fontstring;
  char *font_name;
  char *font_fam;
  int font_size;
  char *font_stretch;
  char *font_style;
  char *font_weight;

  char *wm_type; ///< window manager type, e.g. x11
  char *wm_name; ///< window manager name, may be different from wm_caps.wwm_name
  boolean has_wm_caps;
  wm_caps_t wm_caps;

  char *mountpoint;  ///< utf-8

  // move to os_caps_t
  char *os_name;
  char *os_release;
  char *os_hardware;

#define DISTRO_UBUNTU "Ubuntu"
#define DISTRO_FREEBSD "FreeBSD"

  char *distro_name;
  char *distro_ver;
  char *distro_codename;

  int dclick_time;
  int dclick_dist;
  char *sysbindir;

  // cmdline defaults
  int orig_argc;
  char **orig_argv;

  char *extracmds_file[2];
  int extracmds_idx;

  // plugins
  lives_checkstatus_t has_encoder_plugins;
  lives_checkstatus_t has_decoder_plugins;
  lives_checkstatus_t has_vid_playback_plugins;

  LiVESList *plugins_list[PLUGIN_TYPE_FIRST_CUSTOM];

  // devices
  LiVESList *videodevs;
};

int orig_argc(void);
char **orig_argv(void);

// main.c

void set_signal_handlers(lives_sigfunc_t sigfunc);
void catch_sigint(int signum, siginfo_t *si, void *uc);
void defer_sigint(int signum, siginfo_t *si, void *uc);
boolean defer_sigint_cb(lives_obj_t *obj, void *pdtl);
void ign_signal_handlers(void);

// multitrack-gui.c
void mt_desensitise(lives_mt *);
void mt_sensitise(lives_mt *);

// effects-weed.c
weed_error_t weed_leaf_set_autofree(weed_plant_t *, const char *, boolean state);
weed_plant_t *lives_plant_copy(weed_plant_t *orig); // weed_plant_copy_clean
void weed_plant_duplicate_clean(weed_plant_t *dst, weed_plant_t *src);
void weed_plant_dup_add_clean(weed_plant_t *dst, weed_plant_t *src);
void weed_plant_sanitize(weed_plant_t *, boolean sterilize);
boolean weed_leaf_autofree(weed_plant_t *, const char *key);

int32_t weed_plant_mutate(weed_plantptr_t, int32_t newtype);

#include "osc_notify.h"

// inlines
#define cfile mainw->files[mainw->current_file]

#define LIVES_TV_CHANNEL1 "http://www.serverwillprovide.com/sorteal/livestvclips/livestv.ogm"

const char *dummychar;

void break_me(const char *dtl);

#define LIVES_NO_DEBUG
#ifndef LIVES_DEBUG
#ifndef LIVES_NO_DEBUG
#define LIVES_DEBUG(x)      fprintf(stderr, "LiVES debug: %s\n", x)
#else // LIVES_NO_DEBUG
#define LIVES_DEBUG(x)      dummychar = x
#endif // LIVES_NO_DEBUG
#endif // LIVES_DEBUG

#ifndef LIVES_INFO
#ifndef LIVES_NO_INFO
#define LIVES_INFO(x)      fprintf(stderr, "LiVES info: %s\n", (x))
#else // LIVES_NO_INFO
#define LIVES_INFO(x)      dummychar = (x)
#endif // LIVES_NO_INFO
#endif // LIVES_INFO

#ifndef LIVES_WARN
#ifndef LIVES_NO_WARN
#define LIVES_WARN(x)      fprintf(stderr, "LiVES warning: %s\n", (x))
#else // LIVES_NO_WARN
#define LIVES_WARN(x)      dummychar = (x)
#endif // LIVES_NO_WARN
#endif // LIVES_WARN

#ifndef LIVES_ERROR
#ifndef LIVES_NO_ERROR
#define LIVES_ERROR(x)      {fprintf(stderr, "LiVES ERROR: %s\n", (x)); break_me((x));}
#else // LIVES_NO_ERROR
#define LIVES_ERROR(x)      dummychar = (x)
#endif // LIVES_NO_ERROR
#endif // LIVES_ERROR

#ifndef LIVES_ERROR_NOBRK
#ifndef LIVES_NO_ERROR
#define LIVES_ERROR_NOBRK(x)      {fprintf(stderr, "LiVES ERROR: %s\n", (x));}
#else // LIVES_NO_ERROR
#define LIVES_ERROR_NOBRK(x)      dummychar = (x)
#endif // LIVES_NO_ERROR
#endif // LIVES_ERROR_NOBRK

#ifndef LIVES_CRITICAL
#ifndef LIVES_NO_CRITICAL
#define LIVES_CRITICAL(x)      {fprintf(stderr, "LiVES CRITICAL: %s\n", (x)); break_me((x)); lives_abort(x);}
#else // LIVES_NO_CRITICAL
#define LIVES_CRITICAL(x)      dummychar = (x)
#endif // LIVES_NO_CRITICAL
#endif // LIVES_CRITICAL

#ifndef LIVES_FATAL
#ifndef LIVES_NO_FATAL // WARNING - defining LIVES_NO_FATAL may result in DANGEROUS behaviour !!
#define LIVES_FATAL(x)      lives_abort((x))
#else // LIVES_NO_FATAL
#define LIVES_FATAL(x)      dummychar = (x)
#endif // LIVES_NO_FATAL
#endif // LIVES_FATAL

#define FINALISE_MEMFUNCS
#include "memory.h"

#endif // #ifndef HAS_LIVES_MAIN_H

