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
#define HAS_LIVES_MAIN_H

#ifdef __cplusplus
#undef HAVE_UNICAP
#endif

//#define WEED_STARTUP_TESTS
#define STD_STRINGFUNCS

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#  define GNU_PURE  __attribute__((pure))
#  define GNU_DEPRECATED(msg)  __attribute__((deprecated(msg)))
#  define GNU_CONST  __attribute__((const))
#  define GNU_MALLOC  __attribute__((malloc))
#  define GNU_MALLOC_SIZE(argx) __attribute__((alloc_size(argx)))
#  define GNU_MALLOC_SIZE2(argx, argy) __attribute__((alloc_size(argx, argy)))
#  define GNU_ALIGN(argx) __attribute__((alloc_align(argx)))
#  define GNU_ALIGNED(sizex) __attribute__((assume_aligned(sizex)))
#  define GNU_NORETURN __attribute__((noreturn))
#  define GNU_FLATTEN  __attribute__((flatten)) // inline all function calls
#  define GNU_HOT  __attribute__((hot))
#  define GNU_SENTINEL  __attribute__((sentinel))
#  define GNU_RETURNS_TWICE  __attribute__((returns_twice))
#  define LIVES_IGNORE_DEPRECATIONS G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#  define LIVES_IGNORE_DEPRECATIONS_END G_GNUC_END_IGNORE_DEPRECATIONS
#else
#  define WARN_UNUSED
#  define GNU_PURE
#  define GNU_CONST
#  define GNU_MALLOC
#  define GNU_MALLOC_SIZE(x)
#  define GNU_MALLOC_SIZE2(x, y)
#  define GNU_DEPRECATED(msg)
#  define GNU_ALIGN(x)
#  define GNU_ALIGNED(x)
#  define GNU_NORETURN
#  define GNU_FLATTEN
#  define GNU_HOT
#  define GNU_SENTINEL
#  define GNU_RETURNS_TWICE
#  define LIVES_IGNORE_DEPRECATIONS
#  define LIVES_IGNORE_DEPRECATIONS_END
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for "environ"
#endif

#define LIVES_RESULT_SUCCESS	1
#define LIVES_RESULT_FAIL	0
#define LIVES_RESULT_ERROR	-1

#include <sys/types.h>
#include <inttypes.h>
#include <string.h>

typedef int64_t ticks_t;

typedef int frames_t; // nb. will chenge to int64_t at some future point
typedef int64_t frames64_t; // will become the new standard

typedef void (*lives_funcptr_t)();

#define ENABLE_OSC2

#ifndef GUI_QT
#define GUI_GTK
#define LIVES_PAINTER_IS_CAIRO
#define LIVES_LINGO_IS_PANGO
#else
#define PAINTER_QPAINTER
#define NO_PROG_LOAD
#undef ENABLE_GIW
#endif

#include <sys/file.h>
#include <unistd.h>

typedef pid_t lives_pid_t;

#ifdef GUI_GTK
#ifndef GDK_WINDOWING_X11
#define GDK_WINDOWING_X11
#endif
#endif // GUI_GTK

#ifdef GUI_GTK

#define USE_GLIB

#define LIVES_OS_UNIX G_OS_UNIX

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if GTK_CHECK_VERSION(3, 0, 0)
#ifdef ENABLE_GIW
#define ENABLE_GIW_3
#endif
#else
#undef ENABLE_GIW_3
#endif

#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
// borked in < 3.0
#undef HAVE_WAYLAND
#endif

#ifdef HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#ifndef GDK_IS_WAYLAND_DISPLAY
#define GDK_IS_WAYLAND_DISPLAY(a) FALSE
#endif
#endif

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#endif

#include <stdint.h>
#include <stdarg.h>

#ifndef ulong
#define ulong unsigned long
#endif

#define QUOTEME(x) #x

/// max files is actually 1 more than this, since file 0 is the clipboard
#define MAX_FILES 65535

#ifndef PREFIX_DEFAULT
#define PREFIX_DEFAULT "/usr"
#endif

/// if --prefix= was not set, this is set to "NONE"
#ifndef PREFIX
#define PREFIX PREFIX_DEFAULT
#endif

#define LIVES_DIR_SEP "/"
#define LIVES_COPYRIGHT_YEARS "2002 - 2022"

#if defined (IS_DARWIN) || defined (IS_FREEBSD)
#ifndef off64_t
#define off64_t off_t
#endif
#ifndef lseek64
#define lseek64 lseek
#endif
#endif

#define ENABLE_DVD_GRAB

#ifdef HAVE_MJPEGTOOLS
#define HAVE_YUV4MPEG
#endif

#ifdef ENABLE_ORC
#include <orc/orc.h>
#endif

#ifdef ENABLE_OIL
#include <liboil/liboil.h>
#endif

#ifndef IS_SOLARIS
#define LIVES_INLINE static inline
#define LIVES_GLOBAL_INLINE inline
#else
#define LIVES_INLINE static
#define LIVES_GLOBAL_INLINE
#define LIVES_LOCAL_INLINE
#endif

#define LIVES_LOCAL_INLINE LIVES_INLINE

#include <limits.h>
#include <float.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

#define URL_MAX 2048

#ifdef NEED_ENDIAN_TEST
#undef NEED_ENDIAN_TEST
static const int32_t testint = 0x12345678;
#define IS_BIG_ENDIAN (((char *)&testint)[0] == 0x12)  // runtime test only !
#endif

typedef struct {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
} lives_colRGB48_t;

typedef struct {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
  uint16_t alpha;
} lives_colRGBA64_t;

#define WEED_ADVANCED_PALETTES

#if NEED_LOCAL_WEED
#include "../libweed/weed-host.h"
#include "../libweed/weed.h"
#include "../libweed/weed-events.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
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
weed_plant_new_f _weed_plant_new;
weed_plant_list_leaves_f _weed_plant_list_leaves;
weed_leaf_num_elements_f _weed_leaf_num_elements;
weed_leaf_element_size_f _weed_leaf_element_size;
weed_leaf_seed_type_f _weed_leaf_seed_type;
weed_leaf_get_flags_f _weed_leaf_get_flags;
weed_plant_free_f _weed_plant_free;
weed_leaf_set_flags_f _weed_leaf_set_flags;
weed_leaf_delete_f _weed_leaf_delete;

#ifndef IGN_RET
#define IGN_RET(a) ((void)((a) + 1))
#endif

#define EXPECTED(x) __builtin_expect((x), 1)
#define UNEXPECTED(x) __builtin_expect((x), 0)

#include "weed-effects-utils.h"
#include "support.h"

// directions
/// use REVERSE / FORWARD when a sign is used, BACKWARD / FORWARD when a parity is used
typedef enum {
  LIVES_DIRECTION_REVERSE = -1,
  LIVES_DIRECTION_BACKWARD,
  LIVES_DIRECTION_FORWARD,
  LIVES_DIRECTION_LEFT,
  LIVES_DIRECTION_RIGHT,
  LIVES_DIRECTION_UP,
  LIVES_DIRECTION_DOWN,
  LIVES_DIRECTION_IN,
  LIVES_DIRECTION_OUT,
  LIVES_DIRECTION_UNKNOWN,
  LIVES_DIRECTION_STOPPED,
  LIVES_DIRECTION_CYCLIC,
  LIVES_DIRECTION_RANDOM,
  LIVES_DIRECTION_OTHER,
} lives_direction_t;

#define LIVES_DIRECTION_NONE 0

#include "widget-helper.h"

#include "filesystem.h"

/// install guidance flags
#define INSTALL_CANLOCAL (1ul << 0)
#define INSTALL_IMPORTANT (1ul << 1)

#define IS_ALTERNATIVE (1ul << 16)

typedef enum {
  MISSING = -1, ///< not yet implemented (TODO)
  UNCHECKED = 0,
  PRESENT,
  LOCAL,
} lives_checkstatus_t;

#define XCAPABLE(foo, EXE_FOO) ((capable->has_##foo->present == UNCHECKED \
				 ? ((capable->has_##foo->present =	\
				     (has_executable(EXE_FOO) ? PRESENT : MISSING))) : \
				 capable->has_##foo->present) == PRESENT)
#define GET_EXE(foo) QUOTEME(foo)
#define PRESENT(foo) (XCAPABLE(foo, GET_EXE(foo)) == PRESENT)
#define MISSING(foo) (XCAPABLE(foo, GET_EXE(foo)) == MISSING)

#define IS_PRESENT(item) (capable->has_##item == PRESENT)
#define IS_MISSING(item) (capable->has_##item == MISSING)
#define IS_UNCHECKED(item) (capable->has_##item == UNCHECKED)
#define IS_LOCAL(item) (capable->has_##item == LOCAL)

#define ARE_PRESENT(item) IS_PRESENT(item)
#define ARE_MISSING(item) IS_MISSING(item)
#define ARE_UNCHECKED(item) IS_UNCHECKED(item)

#define IS_AVAILABLE(item) (IS_PRESENT(item) || IS_LOCAL(item))

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

typedef struct {
  int byte_order;
  int ncpus;
  short cpu_bits;
  char *cpu_name;
  char *cpu_vendor;
  uint64_t cpu_features;
  int cacheline_size;
  //
  int mem_status;
  int64_t memtotal;
  int64_t memfree;
  int64_t memavail;
  //
#define OOM_ADJ_RANGE 1000
  char *oom_adjust_file;
  int oom_adj_value;
} hw_caps_t;

#define CPU_HAS_SSE2			1

#define DEF_ALIGN (sizeof(void *) * 8)

// avoiding using an enum allows the list to be extended in other headers
typedef int32_t lives_intention;
typedef weed_plant_t lives_capacity_t;

typedef struct {
  lives_intention intent;
  lives_capacity_t *capacities; ///< type specific capabilities
} lives_intentcap_t;

#include "lists.h"
#include "alarms.h"

/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN = 0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OVERFLOW,
  LIVES_STORAGE_STATUS_OVER_QUOTA,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;

#include "machinestate.h"
#include "lsd-tab.h"

boolean weed_threadsafe;
int weed_abi_version;

#define ALLOW_PNG24

#ifdef IS_LIBLIVES
#include "liblives.hpp"
#include "lbindings.h"
#endif

#define N_RECENT_FILES 16

typedef enum {
  UNDO_NONE = 0,
  UNDO_EFFECT,
  UNDO_RESIZABLE,
  UNDO_MERGE,
  UNDO_RESAMPLE,
  UNDO_TRIM_AUDIO,
  UNDO_TRIM_VIDEO,
  UNDO_CHANGE_SPEED,
  UNDO_AUDIO_RESAMPLE,
  UNDO_APPEND_AUDIO,
  UNDO_INSERT,
  UNDO_CUT,
  UNDO_DELETE,
  UNDO_DELETE_AUDIO,
  UNDO_INSERT_SILENCE,
  UNDO_NEW_AUDIO,

  /// resample/resize and resample audio for encoding
  UNDO_ATOMIC_RESAMPLE_RESIZE,

  /// resample/reorder/resize/apply effects
  UNDO_RENDER,

  UNDO_FADE_AUDIO,
  UNDO_AUDIO_VOL,

  /// record audio to selection
  UNDO_REC_AUDIO,

  UNDO_INSERT_WITH_AUDIO
} lives_undo_t;

/// which stream end should cause playback to finish ?
typedef enum {
  NEVER_STOP = 0,
  STOP_ON_VID_END,
  STOP_ON_AUD_END
} lives_whentostop_t;

/// cancel reason
typedef enum {
  /// no cancel
  CANCEL_NONE = FALSE,

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
  CANCEL_KEEP_LOOPING = CANCEL_NONE + 100
} lives_cancel_t;

typedef enum {
  CANCEL_KILL = 0, ///< normal - kill background processes working on current clip
  CANCEL_INTERRUPT,     ///< midway between KILL and SOFT
  CANCEL_SOFT     ///< just cancel in GUI (for keep, etc)
} lives_cancel_type_t;

typedef enum {
  IMG_TYPE_UNKNOWN = 0,
  IMG_TYPE_JPEG,
  IMG_TYPE_PNG,
  N_IMG_TYPES
} lives_img_type_t;

#define IMG_TYPE_BEST IMG_TYPE_PNG

typedef enum {
  LIVES_INTERLACE_NONE = 0,
  LIVES_INTERLACE_BOTTOM_FIRST = 1,
  LIVES_INTERLACE_TOP_FIRST = 2
} lives_interlace_t;

#include "colourspace.h"
#include "pangotext.h"

#define WEED_LEAF_HOST_DEINTERLACE "host_deint" // frame needs deinterlacing
#define WEED_LEAF_HOST_TC "host_tc" // timecode for deinterlace
#define WEED_LEAF_HOST_DECODER "host_decoder" // pointer to decoder for a layer
#define WEED_LEAF_HOST_PTHREAD "host_pthread" // thread for a layer

#define AV_TRACK_MIN_DIFF 0.001 ///< ignore track time differences < this (seconds)

#include "events.h"

/// some shared structures

#define USE_MPV (!capable->has_mplayer && !capable->has_mplayer2 && capable->has_mpv)
#define HAS_EXTERNAL_PLAYER (capable->has_mplayer || capable->has_mplayer2 || capable->has_mpv)

// common defs for mainwindow (retain this order)
#include "plugins.h"

#include "intents.h"
#include "maths.h"
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

#include "player.h"
#include "cliphandler.h"
#include "sethandler.h"
#include "messaging.h"

/// type sizes
extern ssize_t sizint, sizdbl, sizshrt;

#include "dialogs.h"
#include "saveplay.h"
#include "gui.h"
#include "utils.h"

// capabilities (MUST come after plugins.h)
// TODO - move into capabilities.h

typedef struct {
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

  uint64_t uid; // non-volatile unique id (for current execution)

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
} capability;

capability *capable;

int orig_argc(void);
char **orig_argv(void);

// main.c
typedef void (*SignalHandlerPointer)(int);

void set_signal_handlers(SignalHandlerPointer sigfunc);
void catch_sigint(int signum);
void defer_sigint(int signum);
void *defer_sigint_cb(lives_object_t *obj, void *pdtl);
void startup_message_fatal(char *msg) GNU_NORETURN;
boolean startup_message_choice(const char *msg, int msgtype);
boolean startup_message_nonfatal(const char *msg);
boolean startup_message_info(const char *msg);
boolean startup_message_nonfatal_dismissable(const char *msg, uint64_t warning_mask);
void print_opthelp(LiVESTextBuffer *, const char *extracmds_file1, const char *extracmds_file2);
capability *get_capabilities(void);
void get_monitors(boolean reset);
void replace_with_delegates(void);
void set_drawing_area_from_pixbuf(LiVESWidget *darea, LiVESPixbuf *, lives_painter_surface_t *);
void load_start_image(frames_t frame);
void load_end_image(frames_t frame);
void showclipimgs(void);
void load_preview_image(boolean update_always);
boolean resize_message_area(livespointer data);
boolean lazy_startup_checks(void *data);

#define is_layer_ready(layer) (weed_get_boolean_value((layer), LIVES_LEAF_THREAD_PROCESSING, NULL) == WEED_FALSE \
			       && weed_get_voidptr_value(layer, LIVES_LEAF_RESIZE_THREAD, NULL) == NULL)

boolean pull_frame(weed_layer_t *, const char *image_ext, ticks_t tc);
void pull_frame_threaded(weed_layer_t *, const char *img_ext, ticks_t tc, int width, int height);
boolean check_layer_ready(weed_layer_t *);
boolean pull_frame_at_size(weed_layer_t *, const char *image_ext, ticks_t tc,
                           int width, int height, int target_palette);
LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, ticks_t tc,
                                       int width, int height, LiVESInterpType interp, boolean fordisp);
LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, ticks_t tc);

boolean weed_layer_create_from_file_progressive(weed_layer_t *, const char *fname, int width,
    int height, int tpalette, const char *img_ext);

boolean lives_pixbuf_save(LiVESPixbuf *, char *fname, lives_img_type_t imgtype, int quality,
                          int width, int height, LiVESError **gerrorptr);

typedef struct {
  char *fname;

  // for pixbuf (e.g. jpeg)
  LiVESPixbuf *pixbuf;
  LiVESError *error;
  lives_img_type_t img_type;
  int width, height;

  // for layer (e.g. png) (may also set TGREADVAR(write_failed)
  weed_layer_t *layer;
  boolean success;

  int compression;
} savethread_priv_t;

void *lives_pixbuf_save_threaded(void *saveargs);

#ifdef USE_LIBPNG
boolean layer_from_png(int fd, weed_layer_t *layer, int width, int height, int tpalette, boolean prog);
boolean save_to_png(weed_layer_t *layer, const char *fname, int comp);
void *save_to_png_threaded(void *args);
#endif

void sensitize(void);
void sensitize_rfx(void);
void desensitize(void);
void procw_desensitize(void);
void close_current_file(int file_to_switch_to);   ///< close current file, and try to switch to file_to_switch_to
void switch_to_file(int old_file, int new_file);
void do_quick_switch(int new_file);
boolean switch_audio_clip(int new_file, boolean activate);
void resize(double scale);
boolean set_palette_colours(boolean force_reload);
void set_main_title(const char *filename, int or_untitled_number);
void set_record(void);

// multitrack-gui.c
void mt_desensitise(lives_mt *);
void mt_sensitise(lives_mt *);

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

#ifndef LIVES_CRITICAL
#ifndef LIVES_NO_CRITICAL
#define LIVES_CRITICAL(x)      {fprintf(stderr, "LiVES CRITICAL: %s\n", (x)); break_me((x)); raise (LIVES_SIGSEGV);}
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

#ifndef USE_STD_MEMFUNCS

#ifdef _lives_malloc
#undef  lives_malloc
#define lives_malloc _lives_malloc
#endif
#ifdef _lives_realloc
#undef  lives_realloc
#define lives_realloc _lives_realloc
#endif
#ifdef _lives_free
#undef  lives_free
#define lives_free _lives_free
#endif
#ifdef _lives_memcpy
#undef  lives_memcpy
#define lives_memcpy _lives_memcpy
#endif
#ifdef _lives_memcmp
#undef  lives_memcmp
#define lives_memcmp _lives_memcmp
#endif
#ifdef _lives_memset
#undef  lives_memset
#define lives_memset _lives_memset
#endif
#ifdef _lives_memmove
#undef  lives_memmove
#define lives_memmove _lives_memmove
#endif
#ifdef _lives_calloc
#undef  lives_calloc
#define lives_calloc _lives_calloc
#endif

#endif

#define VSLICES 1

#define VALGRIND_ON  ///< define this to ease debugging with valgrind
#ifdef VALGRIND_ON
#define QUICK_EXIT
#else
#define USE_REC_RS
#endif

#endif // #ifndef HAS_LIVES_MAIN_H

