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

// note: preferred coding style is: astyle --style=java -H -Y -s2 -U -k3 -W3 -xC128 -xL -p -o -Q -xp

#ifndef HAS_LIVES_MAIN_H
#define HAS_LIVES_MAIN_H

#ifdef __cplusplus
#undef HAVE_UNICAP
#endif

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#  define GNU_PURE  __attribute__((pure))
#  define GNU_DEPRECATED(msg)  __attribute__((deprecated(msg)))
#  define GNU_CONST  __attribute__((const))
#  define GNU_MALLOC  __attribute__((malloc))
#  define GNU_ALIGN(x) __attribute__((alloc_align(x)))
#  define GNU_NORETURN __attribute__((noreturn))
#  define GNU_FLATTEN  __attribute__((flatten)) // inline all function calls
#  define GNU_HOT  __attribute__((hot))
#else
#  define WARN_UNUSED
#  define GNU_PURE
#  define GNU_CONST
#  define GNU_MALLOC
#  define GNU_DEPRECATED(msg)
#  define GNU_ALIGN(x)
#  define GNU_NORETURN
#  define GNU_FLATTEN
#  define GNU_HOT
#endif

#include <sys/types.h>
#include <inttypes.h>
#include <string.h>

typedef int64_t ticks_t;

typedef int frames_t; // nb. will chenge to int64_t at some future point
typedef int64_t frames64_t; // will become the new standard

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
typedef int lives_pgid_t;

#ifdef GUI_GTK
#ifndef GDK_WINDOWING_X11
#define GDK_WINDOWING_X11
#endif
#endif // GUI_GTK

#ifdef GUI_GTK

#define USE_GLIB

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

/// this must match AC_PREFIX_DEFAULT in configure.in
/// TODO - when lives-plugins is a separate package, use pkg-config to get PREFIX and remove PREFIX_DEFAULT
#ifndef PREFIX_DEFAULT
#define PREFIX_DEFAULT "/usr"
#endif

/// if --prefix= was not set, this is set to "NONE"
#ifndef PREFIX
#define PREFIX PREFIX_DEFAULT
#endif

#define LIVES_DIR_SEP "/"
#define LIVES_STATUS_FILE_NAME ".status"
#define LIVES_INFO_FILE_NAME ".info"
#define LIVES_BFILE_NAME ".smogrify"
#define LIVES_SMOGPLUGIN_FILE_NAME ".smogplugin"
#define LIVES_SMOGVAL_FILE_NAME ".smogval"
#define LIVES_ENC_DEBUG_FILE_NAME ".debug_out"
#define LIVES_DEVNULL "/dev/null"

#define DLL_NAME "so"

#define DOC_DIR "/share/doc/lives-"

#define THEME_DIR "/share/lives/themes/"
#define PLUGIN_SCRIPTS_DIR "/share/lives/plugins/"
#define PLUGIN_COMPOUND_DIR "/share/lives/plugins/"
#define PLUGIN_EXEC_DIR "/lives/plugins/"
#define ICON_DIR "/share/lives/icons/"
#define DESKTOP_ICON_DIR "/share/icons/hicolor/256x256/apps"
#define DATA_DIR "/share/lives/"
#define LIVES_RC_FILENAME ".lives"
#define LIVES_CONFIG_DIR ".lives-dir/"
#define LIVES_DEVICEMAP_DIR "devicemaps"
#define LIVES_DEF_WORK_NAME "livesprojects"
#define LIVES_RESOURCES_DIR "resources"
#define LIVES_TRASH_DIR "._Trash"

#define LIVES_DEVICE_DIR "/dev/"

#define LIVES_COPYRIGHT_YEARS "2002 - 2020"

#define LIVES_WEBSITE PACKAGE_URL
#define LIVES_MANUAL_URL LIVES_WEBSITE "/manual/"
#define LIVES_MANUAL_FILENAME "LiVES_manual.html"
#define LIVES_AUTHOR_EMAIL "salsaman+lives@gmail.com"
#define LIVES_DONATE_URL "https://sourceforge.net/p/lives/donate/"
#define LIVES_BUG_URL PACKAGE_BUGREPORT
#define LIVES_FEATURE_URL "https://sourceforge.net/p/lives/feature-requests/"
#define LIVES_TRANSLATE_URL "https://translations.launchpad.net/lives/trunk"

#if defined (IS_DARWIN) || defined (__FreeBSD__)
#ifndef off64_t
#define off64_t off_t
#endif
#ifndef lseek64
#define lseek64 lseek
#endif
#endif

#define DEF_FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) ///< non-executable, is modified by the umask

#define ALLOW_NONFREE_CODECS

/// LiVES will show a warning if this (MBytes) is exceeded on load
/// (can be overridden in prefs)
#define WARN_FILE_SIZE 500

/// maximum fps we will allow (double)
#define FPS_MAX 200.

#define MAX_FRAME_WIDTH 100000.
#define MAX_FRAME_HEIGHT 100000.

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

#define strip_ext(fname) lives_strdup((char *)(fname ? strrchr(fname, '.') ? lives_memset(strrchr(fname, '.'), 0, 1) \
					       ? fname : fname : fname : NULL))

// math macros / functions

#define squared(a) ((a) * (a))

#define sig(a) ((a) < 0. ? -1.0 : 1.0)

// round to nearest integer
#define ROUND_I(a) ((int)((double)(a) + .5))

// clamp a between 0 and b; both values rounded to nearest int
#define NORMAL_CLAMP(a, b) (ROUND_I((a))  < 0 ? 0 : ROUND_I((a)) > ROUND_I((b)) ? ROUND_I((b)) : ROUND_I((a)))

// clamp a between 1 and b; both values rounded to nearest int; if rounded value of a is <= 0, return rounded b
#define UTIL_CLAMP(a, b) (NORMAL_CLAMP((a), (b)) <= 0 ? ROUND_I((b)) : ROUND_I((a)))

// round a up double / float a to  next multiple of int b
#define CEIL(a, b) ((int)(((double)(a) + (double)(b) - .000000001) / ((double)(b))) * (b))

// round int a up to next multiple of int b, unless a is already a multiple of b
#define ALIGN_CEIL(a, b) (((int)(((a) + (b) - 1.) / (b))) * (b))

// round int a up to next multiple of int b, unless a is already a multiple of b
#define ALIGN_CEIL64(a, b) ((((int64_t)(a) + (int64_t)(b) - 1) / (int64_t)(b)) * (int64_t)(b))

// round float / double a down to nearest multiple of int b
#define FLOOR(a, b) ((int)(((double)(a) - .000000001) / ((double)(b))) * (b))

// floating point division, maintains the sign of the dividend, regardless of the sign of the divisor
#define SIGNED_DIVIDE(a, b) ((a) < 0. ? -fabs((a) / (b)) : fabs((a) / (b)))

// using signed ints, the first part will be 1 iff -a < b, the second iff a > b, equivalent to abs(a) > b
#define ABS_THRESH(a, b) (((a) + (b)) >> 31) | (((b) - (a)) >> 31)

#define myround(n) ((n) >= 0. ? (int)((n) + 0.5) : (int)((n) - 0.5))

#ifdef NEED_ENDIAN_TEST
#undef NEED_ENDIAN_TEST
static int32_t testint = 0x12345678;
#define IS_BIG_ENDIAN (((char *)&testint)[0] == 0x12)  // runtime test only !
#endif

// utils.c math functions
float LEFloat_to_BEFloat(float f) GNU_CONST;
uint64_t lives_10pow(int pow) GNU_CONST;
double lives_fix(double val, int decimals) GNU_CONST;
uint32_t get_approx_ln(uint32_t val) GNU_CONST;
uint64_t get_approx_ln64(uint64_t x)GNU_CONST;
uint64_t get_near2pow(uint64_t val) GNU_CONST;

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

#if NEED_LOCAL_WEED
#include "../libweed/weed-host.h"
#include "../libweed/weed.h"
#include "../libweed/weed-events.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#else
#include <weed/weed-host.h>
#include <weed/weed.h>
#include <weed/weed-events.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#endif

#if NEED_LOCAL_WEED_UTILS
#include "../libweed/weed-utils.h"
#else
#include <weed/weed-utils.h>
#endif

#if NEED_LOCAL_WEED_COMPAT
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

#include "weed-effects-utils.h"

#include "widget-helper.h"
#include "machinestate.h"

boolean weed_threadsafe;
int weed_abi_version;

#define ALLOW_PNG24

/// this struct is used only when physically resampling frames on the disk
/// we create an array of these and write them to the disk
typedef struct {
  int value;
  int64_t reltime;
} resample_event;

typedef struct {
  // processing / busy dialog
  LiVESWidget *processing;
  LiVESWidget *progressbar;
  LiVESWidget *label;
  LiVESWidget *label2;
  LiVESWidget *label3;
  LiVESWidget *stop_button;
  LiVESWidget *pause_button;
  LiVESWidget *preview_button;
  LiVESWidget *cancel_button;
  LiVESWidget *scrolledwindow;
  frames_t frames_done;
  boolean is_ready;
} xprocess;

typedef struct {
  int afile;
  double seek;
  double vel;
} lives_audio_track_state_t;


#ifdef IS_LIBLIVES
#include "liblives.hpp"
#include "lbindings.h"
#endif

#define N_RECENT_FILES 4

typedef enum {
  UNDO_NONE = 0,
  UNDO_EFFECT,
  UNDO_RESIZABLE,
  UNDO_MERGE,
  UNDO_RESAMPLE,
  UNDO_TRIM_AUDIO,
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

  /// an error occured, retry the operation
  CANCEL_RETRY,

  /// special cancel for TV toy
  CANCEL_KEEP_LOOPING = CANCEL_NONE + 100
} lives_cancel_t;

typedef enum {
  CANCEL_KILL = 0, ///< normal - kill background processes working on current clip
  CANCEL_SOFT     ///< just cancel in GUI (for keep, etc)
} lives_cancel_type_t;

typedef enum {
  CLIP_TYPE_DISK, ///< imported video, broken into frames
  CLIP_TYPE_YUV4MPEG, ///< yuv4mpeg stream
  CLIP_TYPE_GENERATOR, ///< frames from generator plugin
  CLIP_TYPE_FILE, ///< unimported video, not or partially broken in frames
  CLIP_TYPE_LIVES2LIVES, ///< type for LiVES to LiVES streaming
  CLIP_TYPE_VIDEODEV,  ///< frames from video device
  CLIP_TYPE_TEMP ///< temp type, for internal use only
} lives_clip_type_t;

typedef enum {
  IMG_TYPE_UNKNOWN = 0,
  IMG_TYPE_JPEG,
  IMG_TYPE_PNG
} lives_image_type_t;

#define IMG_TYPE_BEST IMG_TYPE_PNG

#define AFORM_SIGNED 0
#define AFORM_LITTLE_ENDIAN 0

#define AFORM_UNSIGNED 1
#define AFORM_BIG_ENDIAN (1<<1)
#define AFORM_UNKNOWN 65536

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

#define CLIP_NAME_MAXLEN 256

#define IS_VALID_CLIP(clip) (clip >= 0 && clip <= MAX_FILES && mainw->files[clip] != NULL)
#define CURRENT_CLIP_IS_VALID IS_VALID_CLIP(mainw->current_file)

#define CLIP_HAS_VIDEO(clip) (IS_VALID_CLIP(clip) ? mainw->files[clip]->frames > 0 : FALSE)
#define CURRENT_CLIP_HAS_VIDEO CLIP_HAS_VIDEO(mainw->current_file)

#define CLIP_HAS_AUDIO(clip) (IS_VALID_CLIP(clip) ? mainw->files[clip]->achans > 0 && mainw->files[clip]->asampsize > 0 : FALSE)
#define CURRENT_CLIP_HAS_AUDIO CLIP_HAS_AUDIO(mainw->current_file)

#define CLIP_VIDEO_TIME(clip) ((double)(IS_VALID_CLIP(clip) ? mainw->files[clip]->video_time : 0.))

#define CLIP_LEFT_AUDIO_TIME(clip) ((double)(IS_VALID_CLIP(clip) ? mainw->files[clip]->laudio_time : 0.))

#define CLIP_RIGHT_AUDIO_TIME(clip) ((double)(IS_VALID_CLIP(clip) ? (mainw->files[clip]->achans > 1 ? \
								     mainw->files[clip]->raudio_time : 0.) : 0.))

#define CLIP_AUDIO_TIME(clip) ((double)(CLIP_LEFT_AUDIO_TIME(clip) >= CLIP_RIGHT_AUDIO_TIME(clip) \
					? CLIP_LEFT_AUDIO_TIME(clip) : CLIP_RIGHT_AUDIO_TIME(clip)))

#define CLIP_TOTAL_TIME(clip) ((double)(CLIP_VIDEO_TIME(clip) > CLIP_AUDIO_TIME(clip) ? CLIP_VIDEO_TIME(clip) : \
					CLIP_AUDIO_TIME(clip)))

#define IS_NORMAL_CLIP(clip) (IS_VALID_CLIP(clip) ? (mainw->files[clip]->clip_type == CLIP_TYPE_DISK \
						     || mainw->files[clip]->clip_type == CLIP_TYPE_FILE) : FALSE)

#define CURRENT_CLIP_IS_NORMAL IS_NORMAL_CLIP(mainw->current_file)

#define LIVES_IS_PLAYING (mainw != NULL && mainw->playing_file > -1)

#define LIVES_IS_RENDERING (mainw != NULL && ((mainw->multitrack == NULL && mainw->is_rendering) \
					      || (mainw->multitrack != NULL && mainw->multitrack->is_rendering)))

#define CURRENT_CLIP_TOTAL_TIME CLIP_TOTAL_TIME(mainw->current_file)

#define CURRENT_CLIP_IS_CLIPBOARD (mainw->current_file == 0)

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
} lives_direction_t;

#define LIVES_DIRECTION_SIG(dir) ((lives_direction_t)sig(dir))  /// LIVES_DIRECTION_REVERSE or LIVES_DIRECTION_FORWARD
#define LIVES_DIRECTION_PAR(dir) ((lives_direction_t)((dir) & 1)) /// LIVES_DIRECTION_BACKWARD or LIVES_DIRECTION_FORWARD
#define LIVES_DIRECTION_OPPOSITE(dir1, dir2) (((dir1) == LIVES_DIR_BACKWARD || (dir1) == LIVES_DIR_REVERSED) \
					      ? (dir2) == LIVES_DIR_FORWARD : ((dir2) == LIVES_DIR_BACKWARD || (dir2) == LIVES_DIR_REVERSED) \
					      ? (dir1) == LIVES_DIR_FORWARD : sig(dir1) != sig(dir2))
/// corresponds to one clip in the GUI
typedef struct _lives_clip_t {
  // basic info (saved during backup)
  int bpp; ///< bits per pixel of the image frames, 24 or 32
  double fps;  /// framerate of the clip
  int hsize; ///< frame width (horizontal) in pixels (NOT macropixels !)
  int vsize; ///< frame height (vertical) in pixels
  int arps; ///< audio physical sample rate (i.e the "normal" sample rate of the clip when played at 1,0 X velocity)
  uint32_t signed_endian; ///< bitfield

  int arate; ///< current audio playback rate (varies if the clip rate is changed)
  uint64_t unique_id;    ///< this and the handle can be used to uniquely id a file
  int achans; ///< number of audio channels (0, 1 or 2)
  int asampsize; ///< audio sample size in bits (8 or 16)

  /////////////////
  frames_t frames;  ///< number of video frames

  int gamma_type;

  // TODO: make 1024 (to handle flv for example)
  char title[256];
  char author[256];
  char comment[256];

  char keywords[256];
  ////////////////

  lives_interlace_t interlace; ///< interlace type (if known - none, topfirst, bottomfirst or : see plugins.h)

  // extended info (not saved)
  int header_version;

#define LIVES_CLIP_HEADER_VERSION 101

  /// the processing window
  xprocess *proc_ptr;

  char handle[256];
  int ohsize;
  int ovsize;
  int64_t f_size;
  int64_t afilesize;
  frames_t old_frames; ///< for deordering, etc.
  char file_name[PATH_MAX]; ///< input file
  char info_file[PATH_MAX];
  char name[CLIP_NAME_MAXLEN];  ///< the display name
  char save_file_name[PATH_MAX];
  char type[64];
  frames_t start;
  frames_t end;
  frames_t insert_start;
  frames_t insert_end;
  frames_t progress_start;
  frames_t progress_end;
  boolean changed;
  LiVESWidget *menuentry;
  ulong menuentry_func;
  boolean orig_file_name;
  boolean was_renamed;
  boolean is_untitled;
  double pb_fps;  ///< current playback rate, may vary from fps, can be 0. or negative
  double freeze_fps; ///< pb_fps for paused / frozen clips
  boolean play_paused;
  boolean was_in_set;
  lives_direction_t
  adirection; ///< audio play directiion during playback. Normally eithr forwards or the same as pb_fps, but it may vary

  //opening/restoring status
  boolean opening;
  boolean opening_audio;
  boolean opening_only_audio;
  boolean opening_loc;
  boolean restoring;
  boolean is_loaded;  ///< should we continue loading if we come back to this clip
  frames_t opening_frames;

  /// don't show preview/pause buttons on processing
  boolean nopreview;

  /// don't show the 'keep' button - e.g. for operations which resize frames
  boolean nokeep;

  // various times; total time is calculated as the longest of video, laudio and raudio
  double video_time; // TODO: deprecate, calculate CLIP_VIDEO_TIME from frames and fps
  double laudio_time;
  double raudio_time;
  double pointer_time;  ///< pointer time in timeline, also the playback start position for clipeditor (unless playing the selection)
  double real_pointer_time;  ///< pointer time in timeline, can extend beyond video, for audio

  // used only for insert_silence, holds pre-padding length for undo
  double old_laudio_time;
  double old_raudio_time;

  // current and last played index frames for internal player
  frames_t frameno;
  frames_t last_frameno;
  frames_t saved_frameno;

  /////////////////////////////////////////////////////////////
  // see resample.c for new events system

  // events
  resample_event *resample_events;  ///<for block resampler

  weed_plant_t *event_list;
  weed_plant_t *event_list_back;
  weed_plant_t *next_event;

  LiVESList *layout_map;
  ////////////////////////////////////////////////////////////////////////////////////////

  ///undo
  lives_undo_t undo_action;

  frames_t undo_start;
  frames_t undo_end;
  char undo_text[32];
  char redo_text[32];
  boolean undoable;
  boolean redoable;

  // used for storing undo values
  int undo1_int;
  int undo2_int;
  int undo3_int;
  int undo4_int;
  uint32_t undo1_uint;
  double undo1_dbl;
  double undo2_dbl;
  boolean undo1_boolean;
  boolean undo2_boolean;
  boolean undo3_boolean;

  int undo_arate; ///< audio playback rate
  uint32_t undo_signed_endian;
  int undo_achans;
  int undo_asampsize;
  int undo_arps; ///< audio sample rate

  lives_clip_type_t clip_type;

  void *ext_src; ///< points to opaque source for non-disk types

  /// index of frames for CLIP_TYPE_FILE
  /// >0 means corresponding frame within original clip
  /// -1 means corresponding image file (equivalent to CLIP_TYPE_DISK)
  /// size must be >= frames, MUST be contiguous in memory
  frames_t *frame_index;

  frames_t *frame_index_back; ///< for undo

  frames_t fx_frame_pump; ///< rfx frame pump for virtual clips (CLIP_TYPE_FILE)

#define FX_FRAME_PUMP_VAL 50 ///< how many frames to prime the pump for realtime effects and resampler

#define IMG_BUFF_SIZE 262144  ///< 256 * 1024 < chunk size for reading images

  boolean ratio_fps; ///< if the fps was set by a ratio

  volatile off64_t aseek_pos; ///< audio seek posn. (bytes) for when we switch clips

  // decoder data

  char mime_type[256];

  boolean deinterlace; ///< auto deinterlace

  lives_image_type_t img_type;

  frames_t last_vframe_played;

  /// layout map for the current layout
  frames_t stored_layout_frame;
  int stored_layout_idx;
  double stored_layout_audio;
  double stored_layout_fps;

  lives_subtitles_t *subt;

  char *op_dir;
  uint64_t op_ds_warn_level; ///< current disk space warning level for any output direcditory

  boolean no_proc_sys_errors; ///< skip system error dialogs in processing
  boolean no_proc_read_errors; ///< skip read error dialogs in processing
  boolean no_proc_write_errors; ///< skip write error dialogs in processing

  boolean keep_without_preview; ///< allow keep, even when nopreview is set - TODO use only nopreview and nokeep

  lives_painter_surface_t *laudio_drawable;
  lives_painter_surface_t *raudio_drawable;

  int cb_src; ///< source clip for clipboard

  boolean needs_update; ///< loaded values were incorrect, update header
  boolean needs_silent_update; ///< needs internal update, we shouldn't concern the user

  boolean checked_for_old_header;
  boolean has_old_header;

  float **audio_waveform; ///< values for drawing the audio wave
  int *aw_sizes; ///< size of each audio_waveform in floats
  char md5sum[64];
} lives_clip_t;

typedef struct {
  // the following can be assumed TRUE, they are checked on startup
  boolean has_smogrify;
  boolean smog_version_correct;
  boolean can_read_from_config;
  boolean can_write_to_config;
  boolean can_write_to_config_new;
  boolean can_write_to_config_backup;
  boolean can_write_to_workdir;

  // the following may need checking before use
  boolean has_perl;
  boolean has_dvgrab;
  boolean has_sox_play;
  boolean has_sox_sox;
  boolean has_autolives;
  boolean has_mplayer;
  boolean has_mplayer2;
  boolean has_mpv;
  boolean has_convert;
  boolean has_composite;
  boolean has_identify;
  boolean has_cdda2wav;
  boolean has_icedax;
  boolean has_midistartstop;
  boolean has_jackd;
  boolean has_pulse_audio;
  boolean has_xwininfo;
  boolean has_gdb;
  boolean has_gzip;
  boolean has_gconftool_2;
  boolean has_xdg_screensaver;
  boolean has_wmctrl;
  boolean has_ssh_askpass;
  boolean has_youtube_dl;

  /// home directory - default location for config file - locale encoding
  char home_dir[PATH_MAX];

  char backend_path[PATH_MAX];

  char touch_cmd[PATH_MAX];
  char rm_cmd[PATH_MAX];
  char mv_cmd[PATH_MAX];
  char cp_cmd[PATH_MAX];
  char ln_cmd[PATH_MAX];
  char chmod_cmd[PATH_MAX];
  char cat_cmd[PATH_MAX];
  char echo_cmd[PATH_MAX];
  char eject_cmd[PATH_MAX];
  char rmdir_cmd[PATH_MAX];

  char *rcfile;

  /// used for returning startup messages from the backend
  char startup_msg[1024];

  // plugins
  boolean has_encoder_plugins;

  boolean has_python;
  uint64_t python_version;

  short cpu_bits;

  char *myname_full;
  char *myname;

  int xstdout;
  int nmonitors;

  int ncpus;

  int byte_order;

  pid_t mainpid;

  mode_t umask;

  // machine load estimates
  double time_per_idle;
  double load_value;
  double avg_load;
} capability;

/// some shared structures
capability *capable;

#define USE_MPV (!capable->has_mplayer && !capable->has_mplayer2 && capable->has_mpv)
#define HAS_EXTERNAL_PLAYER (capable->has_mplayer || capable->has_mplayer2 || capable->has_mpv)

#ifdef ENABLE_JACK
#include "jack.h"
#endif

#define USE_16BIT_PCONV

// common defs for mainwindow (retain this order)
#include "plugins.h"
#include "paramspecial.h"
#include "multitrack.h"
#include "mainwindow.h"
#include "events.h"
#include "keyboard.h"
#include "preferences.h"

extern mainwindow *mainw;

#define BACKEND_NAME EXEC_SMOGRIFY

// internal player clock
#include <sys/time.h>
struct timeval tv;

/// type sizes
extern ssize_t sizint, sizdbl, sizshrt;

typedef enum {
  CLIP_DETAILS_BPP,
  CLIP_DETAILS_FPS,
  CLIP_DETAILS_PB_FPS,
  CLIP_DETAILS_WIDTH,
  CLIP_DETAILS_HEIGHT,
  CLIP_DETAILS_UNIQUE_ID,
  CLIP_DETAILS_ARATE,
  CLIP_DETAILS_PB_ARATE,
  CLIP_DETAILS_ACHANS,
  CLIP_DETAILS_ASIGNED,
  CLIP_DETAILS_AENDIAN,
  CLIP_DETAILS_ASAMPS,
  CLIP_DETAILS_FRAMES,
  CLIP_DETAILS_TITLE,
  CLIP_DETAILS_AUTHOR,
  CLIP_DETAILS_COMMENT,
  CLIP_DETAILS_PB_FRAMENO,
  CLIP_DETAILS_FILENAME,
  CLIP_DETAILS_CLIPNAME,
  CLIP_DETAILS_HEADER_VERSION,
  CLIP_DETAILS_KEYWORDS,
  CLIP_DETAILS_INTERLACE,
  CLIP_DETAILS_DECODER_NAME,
  CLIP_DETAILS_GAMMA_TYPE,
  CLIP_DETAILS_MD5SUM, // for future use
  CLIP_DETAILS_RESERVED1,
  CLIP_DETAILS_RESERVED2,
  CLIP_DETAILS_RESERVED3,
  CLIP_DETAILS_RESERVED4,
  CLIP_DETAILS_RESERVED5,
  CLIP_DETAILS_RESERVED6,
  CLIP_DETAILS_RESERVED7,
  CLIP_DETAILS_RESERVED8,
  CLIP_DETAILS_RESERVED9,
  CLIP_DETAILS_RESERVED10,
  CLIP_DETAILS_RESERVED11,
  CLIP_DETAILS_RESERVED12,
  CLIP_DETAILS_RESERVED13,
  CLIP_DETAILS_RESERVED14,
  CLIP_DETAILS_RESERVED15,
  CLIP_DETAILS_RESERVED16,
  CLIP_DETAILS_RESERVED17,
  CLIP_DETAILS_RESERVED18,
  CLIP_DETAILS_RESERVED19,
  CLIP_DETAILS_RESERVED20,
  CLIP_DETAILS_RESERVED21,
  CLIP_DETAILS_RESERVED22,
  CLIP_DETAILS_RESERVED23,
  CLIP_DETAILS_RESERVED24,
  CLIP_DETAILS_RESERVED25,
  CLIP_DETAILS_RESERVED26,
  CLIP_DETAILS_RESERVED27,
  CLIP_DETAILS_RESERVED28,
  CLIP_DETAILS_RESERVED29,
  CLIP_DETAILS_RESERVED30,
  CLIP_DETAILS_RESERVED31,
  CLIP_DETAILS_RESERVED32
} lives_clip_details_t;

// some useful functions

// dialogs.c
boolean do_progress_dialog(boolean visible, boolean cancellable, const char *text);
boolean do_warning_dialog(const char *text);
boolean do_warning_dialogf(const char *fmt, ...);
boolean do_warning_dialog_with_check(const char *text, uint64_t warn_mask_number);
boolean do_warning_dialog_with_check_transient(const char *text, uint64_t warn_mask_number, LiVESWindow *transient);
boolean do_yesno_dialog(const char *text);
boolean do_yesno_dialogf(const char *fmt, ...);
boolean do_yesno_dialog_with_check(const char *text, uint64_t warn_mask_number);
boolean do_yesno_dialog_with_check_transient(const char *text, uint64_t warn_mask_number, LiVESWindow *transient);
LiVESResponseType do_abort_ok_dialog(const char *text, LiVESWindow *transient);
LiVESResponseType do_abort_retry_dialog(const char *text, LiVESWindow *transient);
LiVESResponseType do_abort_cancel_retry_dialog(const char *text, LiVESWindow *transient) WARN_UNUSED;
LiVESResponseType do_error_dialog(const char *text);
LiVESResponseType do_error_dialogf(const char *fmt, ...);
LiVESResponseType do_info_dialog(const char *text);
LiVESResponseType do_info_dialogf(const char *fmt, ...);
LiVESResponseType do_error_dialog_with_check(const char *text, uint64_t warn_mask_number);
LiVESResponseType do_blocking_error_dialog(const char *text);
LiVESResponseType do_blocking_error_dialogf(const char *fmt, ...);
LiVESResponseType do_blocking_info_dialog(const char *text);
LiVESResponseType do_blocking_info_dialogf(const char *fmt, ...);
LiVESResponseType do_blocking_info_dialog_with_expander(const char *text, const char *exp_text, LiVESList *);
LiVESResponseType do_error_dialog_with_check_transient(const char *text, boolean is_blocking, uint64_t warn_mask_number,
    LiVESWindow *transient);
LiVESWidget *create_message_dialog(lives_dialog_t diat, const char *text, LiVESWindow *transient,
                                   int warn_mask_number, boolean is_blocking);
LiVESWidget *create_question_dialog(const char *title, const char *text, LiVESWindow *parent);
LiVESWindow *get_transient_full();
void do_abortblank_error(const char *what);
void do_optarg_blank_err(const char *what);
void do_clip_divergence_error(int fileno);
LiVESResponseType do_system_failed_error(const char *com, int retval, const char *addinfo, boolean can_retry,
    LiVESWindow *transient);
LiVESResponseType do_write_failed_error_s_with_retry(const char *fname, const char *errtext,
    LiVESWindow *transient) WARN_UNUSED;
void do_write_failed_error_s(const char *filename, const char *addinfo);
LiVESResponseType do_read_failed_error_s_with_retry(const char *fname, const char *errtext, LiVESWindow *transient) WARN_UNUSED;
void do_read_failed_error_s(const char *filename, const char *addinfo);
boolean do_header_write_error(int clip);
LiVESResponseType do_header_read_error_with_retry(int clip) WARN_UNUSED;
LiVESResponseType do_header_missing_detail_error(int clip, lives_clip_details_t detail) WARN_UNUSED;
void do_chdir_failed_error(const char *dir);
LiVESResponseType handle_backend_errors(boolean can_retry, LiVESWindow *transient);
boolean check_backend_return(lives_clip_t *sfile, LiVESWindow *transient);
const char *get_cache_stats(void);

/** warn about disk space */
char *ds_critical_msg(const char *dir, uint64_t dsval);
char *ds_warning_msg(const char *dir, uint64_t dsval, uint64_t cwarn, uint64_t nwarn);
boolean check_storage_space(int clipno, boolean is_processing);

char *get_upd_msg(void);

boolean ask_permission_dialog(int what);
boolean do_abort_check(void);
void add_warn_check(LiVESBox *box, int warn_mask_number);
LiVESResponseType do_memory_error_dialog(char *op, size_t bytes);
void too_many_files(void);
void workdir_warning(void);
void do_audio_import_error(void);
void do_mt_backup_space_error(lives_mt *, int memreq_mb);

boolean do_save_clipset_warn(void);
boolean do_clipboard_fps_warning(void);
void perf_mem_warning(void);
void do_dvgrab_error(void);
boolean do_comments_dialog(int fileno, char *filename);
boolean do_auto_dialog(const char *text, int type);
void do_encoder_acodec_error(void);
void do_encoder_sox_error(void);
boolean rdet_suggest_values(int width, int height, double fps, int fps_num, int fps_denom, int arate,
                            int asigned, boolean swap_endian, boolean anr, boolean ignore_fps);
boolean do_encoder_restrict_dialog(int width, int height, double fps, int fps_num, int fps_denom,
                                   int arate, int asigned, boolean swap_endian, boolean anr, boolean save_all);
void do_messages_window(void);
void do_firstever_dialog(void);
void do_upgrade_error_dialog(void);
void do_no_mplayer_sox_error(void);
void do_need_mplayer_dialog(void);
void do_need_mplayer_mpv_dialog(void);
void do_aud_during_play_error(void);
void do_rendered_fx_dialog(void);
void do_layout_scrap_file_error(void);
void do_layout_ascrap_file_error(void);
void do_program_not_found_error(const char *progname);
void do_lb_composite_error(void);
void do_lb_convert_error(void);
void do_ra_convert_error(void);
void do_set_load_lmap_error(void);
boolean do_set_duplicate_warning(const char *new_set);
boolean do_set_rename_old_layouts_warning(const char *new_set);
boolean do_layout_alter_frames_warning(void);
boolean do_layout_alter_audio_warning(void);
boolean do_reload_set_query(void);
boolean findex_bk_dialog(const char *fname_back);
boolean paste_enough_dlg(int lframe);
boolean do_yuv4m_open_warning(void);
void do_mt_undo_mem_error(void);
void do_mt_undo_buf_error(void);
void do_mt_set_mem_error(boolean has_mt, boolean trans);
void do_mt_audchan_error(int warn_mask);
void do_mt_no_audchan_error(void);
void do_mt_no_jack_error(int warn_mask);
boolean do_mt_rect_prompt(void);
void do_audrate_error_dialog(void);
boolean do_event_list_warning(void);
void do_nojack_rec_error(void);
void do_vpp_palette_error(void);
void do_vpp_fps_error(void);
void do_decoder_palette_error(void);
void do_rmem_max_error(int size);
LiVESResponseType do_original_lost_warning(const char *fname);
void do_no_decoder_error(const char *fname);
void do_no_loadfile_error(const char *fname);
void do_jack_noopen_warn(void);
void do_jack_noopen_warn2(void);
void do_jack_noopen_warn3(void);
void do_jack_noopen_warn4(void);
void do_file_perm_error(const char *file_name);
void do_dir_perm_error(const char *dir_name);
void do_dir_perm_access_error(const char *dir_name);
void do_encoder_img_fmt_error(render_details *rdet);
void do_after_crash_warning(void);
void do_after_invalid_warning(void);
void do_bad_layout_error(void);
void do_card_in_use_error(void);
void do_dev_busy_error(const char *devstr);
boolean do_existing_subs_warning(void);
void do_invalid_subs_error(void);
boolean do_erase_subs_warning(void);
boolean do_sub_type_warning(const char *ext, const char *type_ext);
boolean do_move_workdir_dialog(void);
boolean do_set_locked_warning(const char *setname);
void do_no_in_vdevs_error(void);
void do_locked_in_vdevs_error(void);
void do_do_not_close_d(void);
void do_set_noclips_error(const char *setname);
void do_no_autolives_error(void);
void do_autolives_needs_clips_error(void);
void do_pulse_lost_conn_error(void);
void do_jack_lost_conn_error(void);
void do_cd_error_dialog(void);
void do_bad_theme_error(const char *themefile);
void do_bad_theme_import_error(const char *theme_file);
boolean do_theme_exists_warn(const char *themename);
boolean do_layout_recover_dialog(void);
void add_resnn_label(LiVESDialog *dialog);

int process_one(boolean visible);
void update_progress(boolean visible);
void do_threaded_dialog(const char *translated_text, boolean has_cancel);
void end_threaded_dialog(void);
void threaded_dialog_spin(double fraction);
void response_ok(LiVESButton *button, livespointer user_data);
void pump_io_chan(LiVESIOChannel *iochan);

void do_splash_progress(void);

// message collection
void d_print(const char *fmt, ...);
char *dump_messages(int start, int end); // utils.c
weed_plant_t *get_nth_info_message(int n); // utils.c
int add_messages_to_list(const char *text);
int free_n_msgs(int frval);

// d_print shortcuts
void d_print_cancelled(void);
void d_print_failed(void);
void d_print_done(void);
void d_print_enough(int frames);
void d_print_file_error_failed(void);

boolean d_print_urgency(double timeout_seconds, const char *fmt, ...);

// general
void do_text_window(const char *title, const char *text);

// saveplay.c
boolean read_file_details(const char *file_name, boolean only_check_for_audio, boolean open_image);
boolean add_file_info(const char *check_handle, boolean aud_only);
boolean save_file_comments(int fileno);
boolean reload_clip(int fileno, int maxframe);
void wait_for_bg_audio_sync(int fileno);
ulong deduce_file(const char *filename, double start_time, int end);
ulong open_file(const char *filename);
ulong open_file_sel(const char *file_name, double start_time, int frames);
void open_fw_device(void);
char *get_untitled_name(int number);
boolean get_new_handle(int index, const char *name);
boolean get_temp_handle(int index);
int close_temp_handle(int new_clip);
boolean get_handle_from_info_file(int index);
lives_clip_t *create_cfile(int new_file, const char *handle, boolean is_loaded);
void save_file(int clip, int start, int end, const char *filename);
void play_file(void);
void save_frame(LiVESMenuItem *menuitem, livespointer user_data);
boolean save_frame_inner(int clip, int frame, const char *file_name, int width, int height, boolean from_osc);
void wait_for_stop(const char *stop_command);
boolean save_clip_values(int which_file);
void add_to_recovery_file(const char *handle);
boolean rewrite_recovery_file(void);
boolean check_for_recovery_files(boolean auto_recover);
boolean recover_files(char *recovery_file, boolean auto_recover);
void recover_layout_map(int numclips);
const char *get_deinterlace_string(void);
void reload_subs(int fileno);

// saveplay.c backup
void backup_file(int clip, int start, int end, const char *filename);
int save_event_frames(void);
boolean write_headers(lives_clip_t *file);

// saveplay.c restore
ulong restore_file(const char *filename);
boolean read_headers(const char *file_name);

// saveplay.c sets
void open_set_file(int clipnum);

// saveplay.c scrap file
boolean open_scrap_file(void);
boolean open_ascrap_file(void);
int save_to_scrap_file(weed_layer_t *layer);
boolean load_from_scrap_file(weed_layer_t *layer, int frame);
void close_ascrap_file(boolean remove);
void close_scrap_file(boolean remove);
void add_to_ascrap_mb(uint64_t bytes);

boolean check_for_disk_space(void);

// main.c
typedef void (*SignalHandlerPointer)(int);

void set_signal_handlers(SignalHandlerPointer sigfunc);
void catch_sigint(int signum);
void defer_sigint(int signum);
void startup_message_fatal(char *msg) GNU_NORETURN;
boolean startup_message_choice(const char *msg, int msgtype);
boolean startup_message_nonfatal(const char *msg);
boolean startup_message_info(const char *msg);
boolean startup_message_nonfatal_dismissable(const char *msg, uint64_t warning_mask);
capability *get_capabilities(void);
void get_monitors(boolean reset);
void replace_with_delegates(void);
void set_ce_frame_from_pixbuf(LiVESImage *image, LiVESPixbuf *pixbuf, lives_painter_t *);
void load_start_image(int frame);
void load_end_image(int frame);
void showclipimgs(void);
void load_preview_image(boolean update_always);
boolean resize_message_area(livespointer data);

#define is_layer_ready(layer) (weed_get_boolean_value((layer), "thread_processing", NULL) == WEED_FALSE)

boolean pull_frame(weed_layer_t *layer, const char *image_ext, ticks_t tc);
void pull_frame_threaded(weed_layer_t *layer, const char *img_ext, ticks_t tc, int width, int height);
boolean check_layer_ready(weed_layer_t *layer);
boolean pull_frame_at_size(weed_layer_t *layer, const char *image_ext, ticks_t tc,
                           int width, int height, int target_palette);
LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, ticks_t tc,
                                       int width, int height, LiVESInterpType interp, boolean fordisp);
LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, ticks_t tc);

boolean lives_pixbuf_save(LiVESPixbuf *pixbuf, char *fname, lives_image_type_t imgtype, int quality,
                          int width, int height, LiVESError **gerrorptr);

typedef struct {
  LiVESPixbuf *pixbuf;
  LiVESError *error;
  char *fname;
  int img_type;
  int compression;
  int width, height;
} savethread_priv_t;

void *lives_pixbuf_save_threaded(void *saveargs);

void init_track_decoders(void);
void free_track_decoders(void);

#ifdef USE_LIBPNG
boolean layer_from_png(int fd, weed_layer_t *layer, int width, int height, int tpalette, boolean prog);
//boolean save_to_png(FILE *fp, weed_layer_t *layer, int comp);
#endif

void wait_for_cleaner(void);
void load_frame_image(int frame);
void sensitize(void);
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

//gui.c
void  create_LiVES(void);
void show_lives(void);
void set_colours(LiVESWidgetColor *colf, LiVESWidgetColor *colb, LiVESWidgetColor *colf2,
                 LiVESWidgetColor *colb2, LiVESWidgetColor *coli, LiVESWidgetColor *colt);
void set_preview_box_colours(void);
void load_theme_images(void);
void set_interactive(boolean interactive);
char *get_menu_name(lives_clip_t *sfile, boolean add_set);
int get_hspace(void);
void enable_record(void);
void toggle_record(void);
void disable_record(void);
void make_custom_submenus(void);
void fade_background(void);
void unfade_background(void);
void fullscreen_internal(void);
void block_expose(void);
void unblock_expose(void);
void frame_size_update(void);
void splash_init(void);
void splash_end(void);
void splash_msg(const char *msg, double pct);
void resize_widgets_for_monitor(boolean get_play_times);
void reset_message_area(void);
void get_letterbox_sizes(int *pwidth, int *pheight, int *lb_width, int *lb_height, boolean player_can_upscale);

#if GTK_CHECK_VERSION(3, 0, 0)
void calibrate_sepwin_size(void);
boolean expose_pim(LiVESWidget *, lives_painter_t *, livespointer);
boolean expose_sim(LiVESWidget *, lives_painter_t *, livespointer);
boolean expose_eim(LiVESWidget *, lives_painter_t *, livespointer);
#endif

// system calls in utils.c
int lives_system(const char *com, boolean allow_error);
ssize_t lives_popen(const char *com, boolean allow_error, char *buff, size_t buflen);
lives_pid_t lives_fork(const char *com);
int lives_open3(const char *pathname, int flags, mode_t mode);
int lives_open2(const char *pathname, int flags);
ssize_t lives_write(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_write_le(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_read(int fd, void *buf, size_t count, boolean allow_less);
ssize_t lives_read_le(int fd, void *buf, size_t count, boolean allow_less);

// buffered io

/// fixed values only for write buffers (must be multiples of 16)
#define BUFFER_FILL_BYTES_SMALL 64   /// 1 -> 16 bytes
#define BUFFER_FILL_BYTES_SMALLMED 1024 /// 17 - 256 bytes
#define BUFFER_FILL_BYTES_MED 4096  /// 257 -> 2048 bytes
#define BUFFER_FILL_BYTES_BIGMED 16386  /// 2049 - 8192 bytes
#define BUFFER_FILL_BYTES_LARGE 65536

#define BUFF_SIZE_READ_SMALL 0
#define BUFF_SIZE_READ_SMALLMED 1
#define BUFF_SIZE_READ_MED 2
#define BUFF_SIZE_READ_LARGE 3

#define BUFF_SIZE_WRITE_SMALL 0
#define BUFF_SIZE_WRITE_SMALLMED 1
#define BUFF_SIZE_WRITE_MED 2
#define BUFF_SIZE_WRITE_BIGMED 3
#define BUFF_SIZE_WRITE_LARGE 4

typedef struct {
  ssize_t bytes;  /// buffer size for write, bytes left to read in case of read
  uint8_t *ptr;   /// read point in buffer
  uint8_t *buffer;   /// ptr to data  (ptr - buffer + bytes) gives the read size
  off_t offset; // file offs (of END of block)
  int fd;
  int bufsztype;
  boolean eof;
  boolean read;
  boolean reversed;
  boolean slurping;
  int nseqreads;
  int totops;
  int64_t totbytes;
  boolean allow_fail;
  volatile boolean invalid;
  size_t orig_size;
  char *pathname;
} lives_file_buffer_t;

lives_file_buffer_t *find_in_file_buffers(int fd);
lives_file_buffer_t *find_in_file_buffers_by_pathname(const char *pathname);

int lives_open_buffered_rdonly(const char *pathname);
int lives_open_buffered_writer(const char *pathname, int mode, boolean append);
int lives_create_buffered(const char *pathname, int mode);
int lives_close_buffered(int fd);
void lives_invalidate_all_file_buffers(void);
off_t lives_lseek_buffered_writer(int fd, off_t offset);
off_t lives_lseek_buffered_rdonly(int fd, off_t offset);
off_t lives_lseek_buffered_rdonly_absolute(int fd, off_t offset);
off_t lives_buffered_offset(int fd);
size_t lives_buffered_writer_orig_size(int fd);
boolean lives_buffered_rdonly_set_reversed(int fd, boolean val);
ssize_t lives_write_buffered(int fd, const char *buf, size_t count, boolean allow_fail);
ssize_t lives_write_le_buffered(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_read_buffered(int fd, void *buf, size_t count, boolean allow_less);
ssize_t lives_read_le_buffered(int fd, void *buf, size_t count, boolean allow_less);
boolean lives_read_buffered_eof(int fd);
lives_file_buffer_t *get_file_buffer(int fd);
void lives_buffered_rdonly_slurp(int fd, off_t skip);

int lives_chdir(const char *path, boolean allow_fail);
int lives_fputs(const char *s, FILE *stream);
char *lives_fgets(char *s, int size, FILE *stream);
size_t lives_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
pid_t lives_getpid(void);
int lives_getgid(void);
int lives_getuid(void);
boolean lives_freep(void **ptr);
void lives_kill_subprocesses(const char *dirname, boolean kill_parent);
void lives_suspend_resume_process(const char *dirname, boolean suspend);
int lives_kill(lives_pid_t pid, int sig);
int lives_killpg(lives_pgid_t pgrp, int sig);
ssize_t lives_readlink(const char *path, char *buf, size_t bufsiz);
boolean lives_setenv(const char *name, const char *value);
boolean lives_fsync(int fd);
void lives_sync(int times);
int lives_rmdir(const char *dir, boolean force);
int lives_rmdir_with_parents(const char *dir);
int lives_rm(const char *file);
int lives_rmglob(const char *files);
int lives_cp(const char *from, const char *to);
int lives_cp_recursive(const char *from, const char *to);
int lives_cp_keep_perms(const char *from, const char *to);
int lives_mv(const char *from, const char *to);
int lives_touch(const char *tfile);
int lives_chmod(const char *target, const char *mode);
int lives_cat(const char *from, const char *to, boolean append);
int lives_echo(const char *text, const char *to, boolean append);
int lives_ln(const char *from, const char *to);

int lives_utf8_strcasecmp(const char *s1, const char *s2);
int lives_utf8_strcmp(const char *s1, const char *s2);
LiVESList *lives_list_sort_alpha(LiVESList *list, boolean fwd);

boolean lives_string_ends_with(const char *string, const char *fmt, ...);

char *filename_from_fd(char *val, int fd);

ticks_t lives_get_current_playback_ticks(ticks_t origsecs, ticks_t origusecs, lives_time_source_t *time_source);

lives_alarm_t lives_alarm_set(ticks_t ticks);
ticks_t lives_alarm_check(lives_alarm_t alarm_handle);
boolean lives_alarm_clear(lives_alarm_t alarm_handle);

void get_dirname(char *filename);
char *get_dir(const char *filename);
void get_basename(char *filename);
void get_filename(char *filename, boolean strip_dir);
char *get_extension(const char *filename);
uint64_t get_version_hash(const char *exe, const char *sep, int piece);
uint64_t make_version_hash(const char *ver);
void init_clipboard(void);
boolean cache_file_contents(const char *filename);
char *get_val_from_cached_list(const char *key, size_t maxlen);

void get_location(const char *exe, char *val, int maxlen);
boolean has_executable(const char *exe);

char *make_image_file_name(lives_clip_t *clip, int frame, const char *img_ext);
const char *get_image_ext_for_type(lives_image_type_t imgtype);
lives_image_type_t lives_image_ext_to_type(const char *img_ext);
lives_image_type_t lives_image_type_to_image_type(const char *lives_img_type);

void reset_clipmenu(void);

void get_total_time(lives_clip_t
                    *file); ///< calculate laudio, raudio and video time (may be deprecated and replaced with macros)
void get_play_times(void); ///< recalculate video / audio lengths and draw the timer bars
void update_play_times(void); ///< like get_play_times, but will force redraw of audio waveforms

uint32_t get_signed_endian(boolean is_signed, boolean little_endian); ///< produce bitmapped value

void switch_aud_to_none(boolean set_pref);
boolean switch_aud_to_sox(boolean set_pref);
boolean switch_aud_to_jack(boolean set_pref);
boolean switch_aud_to_pulse(boolean set_pref);

boolean prepare_to_play_foreign(void);
boolean after_foreign_play(void);
boolean check_file(const char *file_name, boolean check_exists);  ///< check if file exists
boolean check_dir_access(const char *dir);
boolean lives_make_writeable_dir(const char *newdir);
boolean is_writeable_dir(const char *dir);
boolean ensure_isdir(char *fname);
boolean dirs_equal(const char *dira, const char *dirb);
char *ensure_extension(const char *fname, const char *ext) WARN_UNUSED;
void activate_url_inner(const char *link);
void activate_url(LiVESAboutDialog *about, const char *link, livespointer data);
void show_manual_section(const char *lang, const char *section);
void maybe_add_mt_idlefunc(void);

double calc_time_from_frame(int clip, int frame);
int calc_frame_from_time(int filenum, double time);   ///< nearest frame [1, frames]
int calc_frame_from_time2(int filenum, double time);  ///< nearest frame [1, frames+1]
int calc_frame_from_time3(int filenum, double time);  ///< nearest frame rounded down, [1, frames+1]
int calc_frame_from_time4(int filenum, double time);  ///<  nearest frame, no maximum

boolean check_for_ratio_fps(double fps);
double get_ratio_fps(const char *string);
void calc_maxspect(int rwidth, int rheight, int *cwidth, int *cheight);

char *remove_trailing_zeroes(double val);

void remove_layout_files(LiVESList *lmap);
boolean add_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data,
                       int clipno, int frameno, double atime, boolean affects_current);
void buffer_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current);
void unbuffer_lmap_errors(boolean add);

void clear_lmap_errors(void);
boolean prompt_remove_layout_files(void);
boolean do_std_checks(const char *type_name, const char *type, size_t maxlen, const char *nreject);
boolean is_legal_set_name(const char *set_name, boolean allow_dupes);
char *repl_workdir(const char *entry, boolean fwd);
char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp);
boolean get_clip_value(int which, lives_clip_details_t, void *retval, size_t maxlen);
boolean save_clip_value(int which, lives_clip_details_t, void *val);
boolean check_frame_count(int idx, boolean last_chkd);
int get_frame_count(int idx, int xsize);
boolean get_frames_sizes(int fileno, int frame_to_test);
int count_resampled_frames(int in_frames, double orig_fps, double resampled_fps);
boolean int_array_contains_value(int *array, int num_elems, int value);
boolean check_for_lock_file(const char *set_name, int type);
void lives_list_free_strings(LiVESList *);
void lives_list_free_all(LiVESList **);
void lives_slist_free_all(LiVESSList **);

boolean create_event_space(int length_in_eventsb);
void add_to_recent(const char *filename, double start, int frames, const char *file_open_params);
int verhash(char *version);
void set_undoable(const char *what, boolean sensitive);
void set_redoable(const char *what, boolean sensitive);
void zero_spinbuttons(void);
void set_sel_label(LiVESWidget *label);
void clear_mainw_msg(void);
size_t get_token_count(const char *string, int delim);
LiVESPixbuf *lives_pixbuf_new_blank(int width, int height, int palette);
const char *lives_strappend(const char *string, int len, const char *newbit);
const char *lives_strappendf(const char *string, int len, const char *fmt, ...);
void find_when_to_stop(void);
int64_t calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc);
void calc_aframeno(int fileno);
void minimise_aspect_delta(double allowed_aspect, int hblock, int vblock, int hsize, int vsize, int *width, int *height);
LiVESInterpType get_interp_value(short quality);

LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) WARN_UNUSED;
LiVESList *lives_list_delete_string(LiVESList *, const char *string) WARN_UNUSED;
LiVESList *lives_list_copy_strings(LiVESList *list);
boolean string_lists_differ(LiVESList *, LiVESList *);
LiVESList *lives_list_append_unique(LiVESList *xlist, const char *add);
LiVESList *buff_to_list(const char *buffer, const char *delim, boolean allow_blanks, boolean strip);
int lives_list_strcmp_index(LiVESList *list, livesconstpointer data, boolean case_sensitive);

LiVESList *get_set_list(const char *dir, boolean utf8);

char *subst(const char *string, const char *from, const char *to);
char *insert_newlines(const char *text, int maxwidth);

int hextodec(const char *string);

uint64_t fastrand(void) GNU_PURE;
void fastsrand(uint64_t seed);

#include "osc_notify.h"

// inlines
#define cfile mainw->files[mainw->current_file]
#define CLIPBOARD_FILE 0
#define clipboard mainw->files[CLIPBOARD_FILE]

#define LIVES_TV_CHANNEL1 "http://www.serverwillprovide.com/sorteal/livestvclips/livestv.ogm"

const char *dummychar;

void break_me(void);

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
#define LIVES_INFO(x)      fprintf(stderr, "LiVES info: %s\n", x)
#else // LIVES_NO_INFO
#define LIVES_INFO(x)      dummychar = x
#endif // LIVES_NO_INFO
#endif // LIVES_INFO

#ifndef LIVES_WARN
#ifndef LIVES_NO_WARN
#define LIVES_WARN(x)      fprintf(stderr, "LiVES warning: %s\n", x)
#else // LIVES_NO_WARN
#define LIVES_WARN(x)      dummychar = x
#endif // LIVES_NO_WARN
#endif // LIVES_WARN

#ifndef LIVES_ERROR
#ifndef LIVES_NO_ERROR
#define LIVES_ERROR(x)      {fprintf(stderr, "LiVES ERROR: %s\n", x); break_me();}
#else // LIVES_NO_ERROR
#define LIVES_ERROR(x)      dummychar = x
#endif // LIVES_NO_ERROR
#endif // LIVES_ERROR

#ifndef LIVES_CRITICAL
#ifndef LIVES_NO_CRITICAL
#define LIVES_CRITICAL(x)      {fprintf(stderr, "LiVES CRITICAL: %s\n", x); break_me(); raise (LIVES_SIGSEGV);}
#else // LIVES_NO_CRITICAL
#define LIVES_CRITICAL(x)      dummychar = x
#endif // LIVES_NO_CRITICAL
#endif // LIVES_CRITICAL

#ifndef LIVES_FATAL
#ifndef LIVES_NO_FATAL
#define LIVES_FATAL(x)      {fprintf(stderr, "LiVES FATAL: %s\n", x); lives_notify(LIVES_OSC_NOTIFY_QUIT, x), break_me(); _exit (1);}
#else // LIVES_NO_FATAL
#define LIVES_FATAL(x)      dummychar = x
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

//#define VALGRIND_ON
#ifdef VALGRIND_ON
#define QUICK_EXIT
#define STD_STRINGFUNCS
#endif

#endif // #ifndef HAS_LIVES_MAIN_H

