// main.h
// LiVES
// (c) G. Finch (salsaman@gmail.com) 2003 - 2015
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


// note: preferred coding style is: astyle --style=java -H -Y -s2 -U -k3 -W3 -xC140 -xL

#ifndef HAS_LIVES_MAIN_H
#define HAS_LIVES_MAIN_H

#ifdef __cplusplus
#undef HAVE_UNICAP
#endif

#ifndef GUI_QT
#define GUI_GTK
#define PAINTER_CAIRO
#else
#define PAINTER_QPAINTER
#define NO_PROG_LOAD
#undef ENABLE_GIW
#endif

#ifdef GUI_GTK

#define USE_GLIB

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if GTK_CHECK_VERSION(3,0,0)
#ifdef ENABLE_GIW
#define ENABLE_GIW_3
#endif
#endif

#endif

#ifdef IS_MINGW

#ifndef WINVER
#define WINVER 0x0500
#endif

#include <windows.h>
#include <winbase.h>
#include <tlhelp32.h>
#include <sddl.h>

#define O_SYNC (FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH)

typedef PROCESS_INFORMATION *lives_pid_t;
typedef PROCESS_INFORMATION *lives_pgid_t;

#ifdef GUI_GTK
#ifndef GDK_WINDOWING_WIN32
#define GDK_WINDOWING_WIN32
#endif
#endif

#else // IS_MINGW

#ifdef GUI_GTK
#ifndef GDK_WINDOWING_X11
#define GDK_WINDOWING_X11
#endif
#else
#include <sys/types.h>
#include <unistd.h>
#endif // GUI_GTK

typedef pid_t lives_pid_t;
typedef int lives_pgid_t;

#endif // IS_MINGW





#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
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
#ifndef IS_MINGW
#define PREFIX_DEFAULT "/usr"
#else
// TODO - get this from the installer
#define PREFIX_DEFAULT "C:\\Program Files\\LiVES"
#endif
#endif

/// if --prefix= was not set, this is set to "NONE"
#ifndef PREFIX
#define PREFIX PREFIX_DEFAULT
#endif



#ifndef IS_MINGW
#define DOC_DIR "/share/doc/lives-"

#define THEME_DIR "/share/lives/themes/"
#define PLUGIN_SCRIPTS_DIR "/share/lives/plugins/"
#define PLUGIN_COMPOUND_DIR "/share/lives/plugins/"
#define PLUGIN_EXEC_DIR "/lives/plugins/"
#define ICON_DIR "/share/lives/icons/"
#define DESKTOP_ICON_DIR "/share/app-install/icons/"
#define DATA_DIR "/share/lives/"
#define LIVES_CONFIG_DIR ".lives-dir/"
#define LIVES_TMP_NAME "livestmp"

#else // IS_MINGW
#define DOC_DIR "\\Documents/"

#define THEME_DIR "\\Themes/"
#define PLUGIN_SCRIPTS_DIR "\\Plugins/"
#define PLUGIN_COMPOUND_DIR "\\Plugins/"
#define PLUGIN_EXEC_DIR "\\Plugins/"
#define ICON_DIR "\\Icons/"
#define DATA_DIR "\\Data/"
#define LIVES_CONFIG_DIR "\\Config/"
#define LIVES_TMP_NAME "livescache"
#endif

#define LIVES_DEVICE_DIR "/dev/"


#define LIVES_MANUAL_URL "http://lives.sourceforge.net/manual/"
#define LIVES_MANUAL_FILENAME "LiVES_manual.html"
#define LIVES_AUTHOR_EMAIL "mailto:salsaman@gmail.com"
#define LIVES_DONATE_URL "https://sourceforge.net/p/lives/donate/"
#define LIVES_BUG_URL "https://sourceforge.net/p/lives/bugs/"
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

#define DEF_FILE_PERMS S_IRUSR|S_IWUSR // must be at least S_IRUSR|S_IWUSR
#define DEF_FILE_UMASK (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)^( DEF_FILE_PERMS )

/// LiVES will show a warning if this (MBytes) is exceeded on load
/// (can be overridden in prefs)
#define WARN_FILE_SIZE 500

/// maximum fps we will allow (double)
#define FPS_MAX 200.

#define MAX_FRAME_WIDTH 100000.
#define MAX_FRAME_HEIGHT 100000.

#define ENABLE_DVD_GRAB

#define FP_BITS 16 /// max fp bits [apparently 16 is faster]

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
#define LIVES_INLINE inline
#else
#define LIVES_INLINE
#endif

#include <limits.h>
#include <float.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#else
#  define WARN_UNUSED
#endif

#ifdef PRODUCE_LOG
#define LIVES_LOG "lives.log"
#endif

uint64_t lives_random(void);

typedef struct {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
} lives_colRGB24_t;

typedef struct {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
  uint16_t alpha;
} lives_colRGBA32_t;


#include "widget-helper.h"

typedef  void *(*fn_ptr)(void *ptr);


/// this struct is used only when physically resampling frames on the disk
/// we create an array of these and write them to the disk
typedef struct {
  int value;
  int64_t reltime;
} event;

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
  uint32_t frames_done;
  boolean is_ready;
} xprocess;




typedef struct {
  int afile;
  double seek;
  double vel;
} lives_audio_track_state_t;


#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-events.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-events.h"
#endif

// see weed event spec. for more info

/// need this for event_list_t *
#include "events.h"

#ifdef IS_LIBLIVES
#include "liblives.hpp"
#include "lbindings.h"
#endif

typedef enum {
  UNDO_NONE=0,
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
  NEVER_STOP=0,
  STOP_ON_VID_END,
  STOP_ON_AUD_END
} lives_whentostop_t;


/// cancel reason
typedef enum {
  /// no cancel
  CANCEL_NONE=FALSE,

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

  /// user pressed 'Keep'
  CANCEL_KEEP,

  /// video playback completed
  CANCEL_AUD_END,

  /// cancelled because of error
  CANCEL_ERROR,

  /// cancelled and paused
  CANCEL_USER_PAUSED,

  /// special cancel for TV toy
  CANCEL_KEEP_LOOPING=CANCEL_NONE+100

} lives_cancel_t;


typedef enum {
  CANCEL_KILL=0,  ///< normal - kill background processes working on current clip
  CANCEL_SOFT     ///< just cancel in GUI (for keep, etc)
} lives_cancel_type_t;




typedef enum {
  CLIP_TYPE_DISK, ///< imported video, broken into frames
  CLIP_TYPE_YUV4MPEG, ///< yuv4mpeg stream
  CLIP_TYPE_GENERATOR, ///< frames from generator plugin
  CLIP_TYPE_FILE, ///< unimported video, not or partially broken in frames
  CLIP_TYPE_LIVES2LIVES, ///< type for LiVES to LiVES streaming
  CLIP_TYPE_VIDEODEV  ///< frames from video device
} lives_clip_type_t;


typedef enum {
  IMG_TYPE_UNKNOWN=0,
  IMG_TYPE_JPEG,
  IMG_TYPE_PNG
} lives_image_type_t;



#define AFORM_SIGNED 0
#define AFORM_LITTLE_ENDIAN 0

#define AFORM_UNSIGNED 1
#define AFORM_BIG_ENDIAN (1<<1)
#define AFORM_UNKNOWN 65536


typedef enum {
  LIVES_INTERLACE_NONE=0,
  LIVES_INTERLACE_BOTTOM_FIRST=1,
  LIVES_INTERLACE_TOP_FIRST=2
} lives_interlace_t;


#include "pangotext.h"

/// corresponds to one clip in the GUI
typedef struct {
  // basic info (saved during backup)
  int bpp;
  double fps;
  int hsize; ///< in pixels (NOT macropixels !)
  int vsize;
  int arps; ///< audio sample rate
  uint32_t signed_endian; ///< bitfield

  int arate; ///< audio playback rate
  uint64_t unique_id;    ///< this and the handle can be used to uniquely id a file
  int achans;
  int asampsize;

  /////////////////
  int frames;
  char title[256];
  char author[256];
  char comment[256];
  char keywords[256];
  ////////////////

  lives_interlace_t interlace; ///< interlace type (if known - none, topfirst, bottomfirst or : see plugins.h)

  // extended info (not saved)
  int header_version;

#define LIVES_CLIP_HEADER_VERSION 100

  int rowstride;

  /// the processing window
  xprocess *proc_ptr;

  char handle[256];
  int ohsize;
  int ovsize;
  int64_t f_size;
  int64_t afilesize;
  int old_frames; ///< for deordering, etc.
  char file_name[PATH_MAX]; ///< input file
  char info_file[PATH_MAX];
  char name[256];  ///< the display name
  char save_file_name[PATH_MAX];
  char type[40];
  int start;
  int end;
  int insert_start;
  int insert_end;
  int progress_start;
  int progress_end;
  boolean changed;
  LiVESWidget *menuentry;
  ulong menuentry_func;
  boolean orig_file_name;
  boolean was_renamed;
  boolean is_untitled;
  double pb_fps;
  double freeze_fps;
  boolean play_paused;

  //opening/restoring status
  boolean opening;
  boolean opening_audio;
  boolean opening_only_audio;
  boolean opening_loc;
  boolean restoring;
  boolean is_loaded;  ///< should we continue loading if we come back to this clip

  /// don't show preview/pause buttons on processing
  boolean nopreview;

  /// don't show the 'keep' button - e.g. for operations which resize frames
  boolean nokeep;

  // various times; total time is calculated as the longest of video, laudio and raudio
  double total_time;
  double video_time;
  double laudio_time;
  double raudio_time;
  double pointer_time;

  // current and last played index frames for internal player
  int frameno;
  int last_frameno;



  /////////////////////////////////////////////////////////////
  // see resample.c for new events system


  // events
  event *events[1];  ///<for block resampler

  weed_plant_t *event_list;
  weed_plant_t *event_list_back;
  weed_plant_t *next_event;

  LiVESList *layout_map;
  ////////////////////////////////////////////////////////////////////////////////////////

  ///undo
  lives_undo_t undo_action;

  int undo_start;
  int undo_end;
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
  int *frame_index;

  int *frame_index_back; ///< for undo

  int fx_frame_pump; ///< rfx frame pump for virtual clips (CLIP_TYPE_FILE)

#define FX_FRAME_PUMP_VAL 200 ///< how many frames to prime the pump for realtime effects and resampler

#define IMG_BUFF_SIZE 4096 ///< chunk size for reading images

  boolean ratio_fps; ///< if the fps was set by a ratio

  int64_t aseek_pos; ///< audio seek posn. (bytes) for when we switch clips

  // decoder data

  char mime_type[256];


  boolean deinterlace; ///< auto deinterlace

  lives_image_type_t img_type;

  /// layout map for the current layout
  int stored_layout_frame;
  int stored_layout_idx;
  double stored_layout_audio;
  double stored_layout_fps;

  lives_subtitles_t *subt;

  char *op_dir;
  uint64_t op_ds_warn_level; ///< current disk space warning level for any output directory

  boolean no_proc_sys_errors; ///< skip system error dialogs in processing
  boolean no_proc_read_errors; ///< skip read error dialogs in processing
  boolean no_proc_write_errors; ///< skip write error dialogs in processing

  boolean keep_without_preview; ///< allow keep, even when nopreview is set - TODO use only nopreview and nokeep

  lives_painter_surface_t *laudio_drawable;
  lives_painter_surface_t *raudio_drawable;

  int cb_src; ///< source clip for clipboard
} lives_clip_t;



typedef struct {
  // the following can be assumed TRUE, they are checked on startup
  boolean has_smogrify;
  boolean smog_version_correct;
  boolean can_read_from_config;
  boolean can_write_to_config;
  boolean can_write_to_tmp;
  boolean can_write_to_tempdir;

  // the following may need checking before use
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
  boolean has_gconftool_2;
  boolean has_xdg_screensaver;

  /// home directory - default location for config file - locale encoding
  char home_dir[PATH_MAX];

  /// system tempdir (e.g /tmp for linux, C:\TEMP for win32)
  char system_tmpdir[PATH_MAX];  ///< kept in locale encoding

#ifndef IS_MINGW
  char touch_cmd[PATH_MAX];
  char rm_cmd[PATH_MAX];
  char mv_cmd[PATH_MAX];
  char cp_cmd[PATH_MAX];
  char ln_cmd[PATH_MAX];
  char chmod_cmd[PATH_MAX];
  char cat_cmd[PATH_MAX];
  char echo_cmd[PATH_MAX];
  char rmdir_cmd[PATH_MAX];
#endif

  char *rcfile;

  /// used for returning startup messages from the backend
  char startup_msg[256];

  // plugins
  boolean has_encoder_plugins;

  boolean has_python;
  uint64_t python_version;

  short cpu_bits;


  char *myname_full;
  char *myname;

  boolean has_stderr;

  int nmonitors;

  int ncpus;

  int byte_order;

  pid_t mainpid;

} capability;


/// some shared structures
extern capability *capable;

#ifdef HAVE_JACK_JACK_H
#include "jack.h"
#endif

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifndef PRId64

#ifndef __WORDSIZE
#if defined __x86_64__
# define __WORDSIZE	64
#ifndef __WORDSIZE_COMPAT32
# define __WORDSIZE_COMPAT32	1
#endif
#else
# define __WORDSIZE	32
#endif
#endif // __WORDSIZE

#ifndef __PRI64_PREFIX
# if __WORDSIZE == 64
#  define __PRI64_PREFIX	"l"
# else
#  define __PRI64_PREFIX	"ll"
# endif
#endif

# define PRId64		__PRI64_PREFIX "d"
# define PRIu64		__PRI64_PREFIX "u"
#endif // ifndef PRI64d


// common defs for mainwindow (retain this order)
#include "plugins.h"
#include "colourspace.h"
#include "paramspecial.h"
#include "multitrack.h"
#include "mainwindow.h"
#include "keyboard.h"
#include "preferences.h"

extern mainwindow *mainw;


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
  CLIP_DETAILS_DECODER_NAME
} lives_clip_details_t;


// some useful functions


// dialogs.c
boolean do_progress_dialog(boolean visible, boolean cancellable, const char *text);
boolean do_warning_dialog(const char *text);
boolean do_warning_dialog_with_check(const char *text, int warn_mask_number);
boolean do_warning_dialog_with_check_transient(const char *text, int warn_mask_number, LiVESWindow *transient);
boolean do_yesno_dialog(const char *text);
boolean do_yesno_dialog_with_check(const char *text, int warn_mask_number);
boolean do_yesno_dialog_with_check_transient(const char *text, int warn_mask_number, LiVESWindow *transient);
boolean do_yesno_dialog_with_check(const char *text, int warn_mask_number);
boolean do_yesno_dialog_with_check_transient(const char *text, int warn_mask_number, LiVESWindow *transient);
int do_abort_cancel_retry_dialog(const char *text, LiVESWindow *transient) WARN_UNUSED;
int do_error_dialog(const char *text);
int do_info_dialog(const char *text);
int do_error_dialog_with_check(const char *text, int warn_mask_number);
int do_blocking_error_dialog(const char *text);
int do_blocking_info_dialog(const char *text);
int do_error_dialog_with_check_transient(const char *text, boolean is_blocking, int warn_mask_number,
    LiVESWindow *transient);
int do_info_dialog_with_transient(const char *text, boolean is_blocking, LiVESWindow *transient);
LiVESWidget *create_message_dialog(lives_dialog_t diat, const char *text, LiVESWindow *transient,
                                   int warn_mask_number, boolean is_blocking);


void do_system_failed_error(const char *com, int retval, const char *addinfo);
int do_write_failed_error_s_with_retry(const char *fname, const char *errtext, LiVESWindow *transient) WARN_UNUSED;
void do_write_failed_error_s(const char *filename, const char *addinfo);
int do_read_failed_error_s_with_retry(const char *fname, const char *errtext, LiVESWindow *transient) WARN_UNUSED;
void do_read_failed_error_s(const char *filename, const char *addinfo);
boolean do_header_write_error(int clip);
int do_header_read_error_with_retry(int clip) WARN_UNUSED;
int do_header_missing_detail_error(int clip, lives_clip_details_t detail) WARN_UNUSED;
void do_chdir_failed_error(const char *dir);
void handle_backend_errors(void);
boolean check_backend_return(lives_clip_t *sfile);

/** warn about disk space */
char *ds_critical_msg(const char *dir, uint64_t dsval);
char *ds_warning_msg(const char *dir, uint64_t dsval, uint64_t cwarn, uint64_t nwarn);
boolean check_storage_space(lives_clip_t *sfile, boolean is_processing);

char *get_upd_msg(void);
char *get_new_install_msg(void);

boolean ask_permission_dialog(int what);
boolean do_abort_check(void);
void add_warn_check(LiVESBox *box, int warn_mask_number);
void do_memory_error_dialog(void);
void too_many_files(void);
void tempdir_warning(void);
void do_audio_import_error(void);
void do_mt_backup_space_error(lives_mt *, int memreq_mb);

boolean do_clipboard_fps_warning(void);
void perf_mem_warning(void);
void do_dvgrab_error(void);
boolean do_comments_dialog(lives_clip_t *sfile, char *filename);
boolean do_auto_dialog(const char *text, int type);
void do_encoder_acodec_error(void);
void do_encoder_sox_error(void);
boolean rdet_suggest_values(int width, int height, double fps, int fps_num, int fps_denom, int arate,
                            int asigned, boolean swap_endian, boolean anr, boolean ignore_fps);
boolean do_encoder_restrict_dialog(int width, int height, double fps, int fps_num, int fps_denom,
                                   int arate, int asigned, boolean swap_endian, boolean anr, boolean save_all);
void do_keys_window(void);
void do_mt_keys_window(void);
void do_messages_window(void);
void do_firstever_dialog(void);
void do_upgrade_error_dialog(void);
void do_no_mplayer_sox_error(void);
void do_aud_during_play_error(void);
void do_rendered_fx_dialog(void);
void do_layout_scrap_file_error(void);
void do_layout_ascrap_file_error(void);
void do_lb_composite_error(void);
void do_lb_convert_error(void);
void do_ra_convert_error(void);
void do_set_load_lmap_error(void);
boolean do_set_duplicate_warning(const char *new_set);
boolean do_set_rename_old_layouts_warning(const char *new_set);
boolean do_layout_alter_frames_warning(void);
boolean do_layout_alter_audio_warning(void);
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
boolean do_original_lost_warning(const char *fname);
void do_no_decoder_error(const char *fname);
void do_jack_noopen_warn(void);
void do_jack_noopen_warn2(void);
void do_jack_noopen_warn3(void);
void do_jack_noopen_warn4(void);
void do_file_perm_error(const char *file_name);
void do_dir_perm_error(const char *dir_name);
void do_dir_perm_access_error(const char *dir_name);
void do_encoder_img_ftm_error(render_details *rdet);
void do_after_crash_warning(void);
void do_bad_layout_error(void);
void do_card_in_use_error(void);
void do_dev_busy_error(const char *devstr);
boolean do_existing_subs_warning(void);
void do_invalid_subs_error(void);
boolean do_erase_subs_warning(void);
boolean do_sub_type_warning(const char *ext, const char *type_ext);
boolean do_move_tmpdir_dialog(void);
void do_set_locked_warning(const char *setname);
void do_no_in_vdevs_error(void);
void do_locked_in_vdevs_error(void);
void do_do_not_close_d(void);
void do_set_noclips_error(const char *setname);
void do_no_autolives_error(void);
void do_autolives_needs_clips_error(void);
void do_pulse_lost_conn_error(void);
void do_jack_lost_conn_error(void);
void do_cd_error_dialog(void);

boolean process_one(boolean visible);
void do_threaded_dialog(char *translated_text, boolean has_cancel);
void end_threaded_dialog(void);
void threaded_dialog_spin(void);
void response_ok(LiVESButton *button, livespointer user_data);
void pump_io_chan(LiVESIOChannel *iochan);

void do_splash_progress(void);


// d_print shortcuts
void d_print_cancelled(void);
void d_print_failed(void);
void d_print_done(void);
void d_print_file_error_failed(void);

// general
void do_text_window(const char *title, const char *text);

// saveplay.c
boolean read_file_details(const char *file_name, boolean only_check_for_audio);
boolean add_file_info(const char *check_handle, boolean aud_only);
boolean save_file_comments(int fileno);
boolean reload_clip(int fileno);
void reget_afilesize(int fileno);
ulong deduce_file(const char *filename, double start_time, int end);
ulong open_file(const char *filename);
ulong open_file_sel(const char *file_name, double start_time, int frames);
void open_fw_device(void);
boolean get_new_handle(int index, const char *name);
boolean get_temp_handle(int index, boolean create);
boolean get_handle_from_info_file(int index);
void create_cfile(void);
void save_file(int clip, int start, int end, const char *filename);
void play_file(void);
void save_frame(LiVESMenuItem *menuitem, livespointer user_data);
boolean save_frame_inner(int clip, int frame, const char *file_name, int width, int height, boolean from_osc);
void wait_for_stop(const char *stop_command);
boolean save_clip_values(int which_file);
void add_to_recovery_file(const char *handle);
void rewrite_recovery_file(void);
boolean check_for_recovery_files(boolean auto_recover);
void recover_layout_map(int numclips);
const char *get_deinterlace_string(void);

// saveplay.c backup
void backup_file(int clip, int start, int end, const char *filename);
int save_event_frames(void);
boolean write_headers(lives_clip_t *file);

// saveplay.c restore
ulong restore_file(const char *filename);
boolean read_headers(const char *file_name);

// saveplay.c sets
void open_set_file(const char *set_name, int clipnum);


// saveplay.c scrap file
boolean open_scrap_file(void);
boolean open_ascrap_file(void);
int save_to_scrap_file(weed_plant_t *layer);
boolean load_from_scrap_file(weed_plant_t *layer, int frame);
void close_ascrap_file(void);
void close_scrap_file(void);


// main.c
typedef void (*SignalHandlerPointer)(int);

void set_signal_handlers(SignalHandlerPointer sigfunc);
void catch_sigint(int signum);
void defer_sigint(int signum);
boolean startup_message_fatal(const char *msg);
boolean startup_message_nonfatal(const char *msg);
boolean startup_message_info(const char *msg);
boolean startup_message_nonfatal_dismissable(const char *msg, int warning_mask);
capability *get_capabilities(void);
void get_monitors(void);
void set_ce_frame_from_pixbuf(LiVESImage *image, LiVESPixbuf *pixbuf, lives_painter_t *);
void load_start_image(int frame);
void load_end_image(int frame);
void load_preview_image(boolean update_always);

boolean pull_frame(weed_plant_t *layer, const char *image_ext, weed_timecode_t tc);
void pull_frame_threaded(weed_plant_t *layer, const char *img_ext, weed_timecode_t tc);
void check_layer_ready(weed_plant_t *layer);
boolean pull_frame_at_size(weed_plant_t *layer, const char *image_ext, weed_timecode_t tc,
                           int width, int height, int target_palette);
LiVESPixbuf *pull_lives_pixbuf_at_size(int clip, int frame, const char *image_ext, weed_timecode_t tc,
                                       int width, int height, LiVESInterpType interp);
LiVESPixbuf *pull_lives_pixbuf(int clip, int frame, const char *image_ext, weed_timecode_t tc);

LiVESError *lives_pixbuf_save(LiVESPixbuf *pixbuf, char *fname, lives_image_type_t imgtype,
                              int quality, boolean do_chmod, LiVESError **gerrorptr);

void init_track_decoders(void);
void free_track_decoders(void);


void load_frame_image(int frame);
void sensitize(void);
void desensitize(void);
void procw_desensitize(void);
void close_current_file(int file_to_switch_to);   ///< close current file, and try to switch to file_to_switch_to
void get_next_free_file(void);
void switch_to_file(int old_file, int new_file);
void do_quick_switch(int new_file);
void switch_audio_clip(int new_file, boolean activate);
void resize(double scale);
void do_start_messages(void);
void set_palette_colours(void);
void set_main_title(const char *filename, int or_untitled_number);
void set_record(void);

//gui.c
void  create_LiVES(void);
void set_interactive(boolean interactive);
char *get_menu_name(lives_clip_t *sfile);
void enable_record(void);
void toggle_record(void);
void disable_record(void);
void make_custom_submenus(void);
void fade_background(void);
void unfade_background(void);
void block_expose(void);
void unblock_expose(void);
void frame_size_update(void);
void splash_init(void);
void splash_end(void);
void splash_msg(const char *msg, double pct);
void add_message_scroller(LiVESWidget *conter);
void resize_widgets_for_monitor(boolean get_play_times);
#if GTK_CHECK_VERSION(3,0,0)
void calibrate_sepwin_size(void);
boolean expose_pim(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data);
boolean expose_sim(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data);
boolean expose_eim(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data);
#endif





// system calls in utils.c
int lives_system(const char *com, boolean allow_error);
lives_pid_t lives_fork(const char *com);
int lives_open_buffered_rdonly(const char *pathname);
int lives_creat_buffered(const char *pathname, int mode);
int lives_close_buffered(int fd);
void lives_close_all_file_buffers(void);
off_t lives_lseek_buffered_rdonly(int fd, off_t offset);
ssize_t lives_write(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_write_buffered(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_write_le(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_write_le_buffered(int fd, livesconstpointer buf, size_t count, boolean allow_fail);
ssize_t lives_read(int fd, void *buf, size_t count, boolean allow_less);
ssize_t lives_read_buffered(int fd, void *buf, size_t count, boolean allow_less);
ssize_t lives_read_le(int fd, void *buf, size_t count, boolean allow_less);
ssize_t lives_read_le_buffered(int fd, void *buf, size_t count, boolean allow_less);
int lives_chdir(const char *path, boolean allow_fail);
int lives_fputs(const char *s, FILE *stream);
char *lives_fgets(char *s, int size, FILE *stream);
pid_t lives_getpid(void);
int lives_getgid(void);
int lives_getuid(void);
void lives_freep(void **ptr);
#ifdef IS_MINGW
boolean lives_win32_suspend_resume_process(DWORD pid, boolean suspend);
boolean lives_win32_kill_subprocesses(DWORD pid, boolean kill_parent);
int lives_win32_get_num_logical_cpus(void);
#endif
int lives_kill(lives_pid_t pid, int sig);
int lives_killpg(lives_pgid_t pgrp, int sig);
void lives_srandom(unsigned int seed);
ssize_t lives_readlink(const char *path, char *buf, size_t bufsiz);
boolean lives_setenv(const char *name, const char *value);
boolean lives_fsync(int fd);
void lives_sync(void);

int lives_utf8_strcasecmp(const char *s1, const char *s2);

char *filename_from_fd(char *val, int fd);

float LEFloat_to_BEFloat(float f);
uint64_t lives_10pow(int pow);
int get_approx_ln(uint32_t val);

int64_t lives_get_current_ticks(void);
boolean lives_alarm_get(int alarm_handle);
int lives_alarm_set(int64_t ticks);
void lives_alarm_clear(int alarm_handle);
lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, uint64_t *dsval);
char *lives_format_storage_space_string(uint64_t space);


int myround(double n);
void get_dirname(char *filename);
char *get_dir(const char *filename);
void get_basename(char *filename);
void get_filename(char *filename, boolean strip_dir);
char *get_extension(const char *filename);
uint64_t get_version_hash(const char *exe, const char *sep, int piece);
uint64_t make_version_hash(const char *ver);
void d_print(const char *fmt, ...);
void init_clipboard(void);
boolean cache_file_contents(const char *filename);
char *get_val_from_cached_list(const char *key, size_t maxlen);

void get_location(const char *exe, char *val, int maxlen);

char *make_image_file_name(lives_clip_t *clip, int frame, const char *img_ext);

void set_menu_text(LiVESWidget *menu, const char *text, boolean use_mnemonic);
void get_menu_text(LiVESWidget *menu, char *text);
void get_menu_text_long(LiVESWidget *menuitem, char *text);
void reset_clipmenu(void);
void get_play_times(void);
void get_total_time(lives_clip_t *file);
uint32_t get_signed_endian(boolean is_signed, boolean little_endian);
void fullscreen_internal(void);
void switch_to_int_player(void);
void switch_to_mplayer(void);
void switch_aud_to_sox(boolean set_pref);
boolean switch_aud_to_jack(void);
boolean switch_aud_to_pulse(void);
void switch_aud_to_mplayer(boolean set_pref);
void switch_aud_to_mplayer2(boolean set_pref);
boolean prepare_to_play_foreign(void);
boolean after_foreign_play(void);
boolean check_file(const char *file_name, boolean check_exists);  ///< check if file exists
boolean check_dir_access(const char *dir);
uint64_t get_file_size(int fd);
uint64_t sget_file_size(const char *name);
uint64_t get_fs_free(const char *dir);
boolean is_writeable_dir(const char *dir);
boolean ensure_isdir(char *fname);
char *ensure_extension(const char *fname, const char *ext) WARN_UNUSED;
boolean check_dev_busy(char *devstr);
void activate_url_inner(const char *link);
void activate_url(LiVESAboutDialog *about, const char *link, livespointer data);
void show_manual_section(const char *lang, const char *section);

double calc_time_from_frame(int clip, int frame);
int calc_frame_from_time(int filenum, double time);   ///< nearest frame start
int calc_frame_from_time2(int filenum, double time);  ///< nearest frame end
int calc_frame_from_time3(int filenum, double time);  ///< nearest frame mid

boolean check_for_ratio_fps(double fps);
double get_ratio_fps(const char *string);
void calc_maxspect(int rwidth, int rheight, int *cwidth, int *cheight);

char *remove_trailing_zeroes(double val);

void remove_layout_files(LiVESList *lmap);
boolean add_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data,
                       int clipno, int frameno, double atime, boolean affects_current);
void clear_lmap_errors(void);
boolean prompt_remove_layout_files(void);
boolean is_legal_set_name(const char *set_name, boolean allow_dupes);
char *repl_tmpdir(const char *entry, boolean fwd);
char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp);
boolean get_clip_value(int which, lives_clip_details_t, void *retval, size_t maxlen);
void save_clip_value(int which, lives_clip_details_t, void *val);
boolean check_frame_count(int idx);
void get_frame_count(int idx);
void get_frames_sizes(int fileno, int frame_to_test);
int count_resampled_frames(int in_frames, double orig_fps, double resampled_fps);
boolean int_array_contains_value(int *array, int num_elems, int value);
boolean check_for_lock_file(const char *set_name, int type);
void lives_list_free_strings(LiVESList *list);

boolean create_event_space(int length_in_eventsb);
void add_to_recent(const char *filename, double start, int frames, const char *file_open_params);
int verhash(char *version);
void set_undoable(const char *what, boolean sensitive);
void set_redoable(const char *what, boolean sensitive);
void zero_spinbuttons(void);
void draw_little_bars(double ptrtime);
void set_sel_label(LiVESWidget *label);
void clear_mainw_msg(void);
int get_token_count(const char *string, int delim);
LiVESPixbuf *lives_pixbuf_new_blank(int width, int height, int palette);
char *lives_strappend(char *string, int len, const char *newbit);
LiVESList *lives_list_append_unique(LiVESList *xlist, const char *add);
void find_when_to_stop(void);
int calc_new_playback_position(int fileno, uint64_t otc, uint64_t *ntc);
void calc_aframeno(int fileno);
void minimise_aspect_delta(double allowed_aspect,int hblock,int vblock,int hsize,int vsize,int *width,int *height);
LiVESInterpType get_interp_value(short quality);

LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) WARN_UNUSED;
LiVESList *lives_list_delete_string(LiVESList *, char *string) WARN_UNUSED;
LiVESList *lives_list_copy_strings(LiVESList *list);
boolean string_lists_differ(LiVESList *, LiVESList *);

boolean is_realtime_aplayer(int ptype);

LiVESList *get_set_list(const char *dir, boolean utf8);

char *subst(const char *string, const char *from, const char *to);
char *insert_newlines(const char *text, int maxwidth);

int hextodec(char *string);
int get_hex_digit(const char *c);

const char *get_image_ext_for_type(lives_image_type_t imgtype);

uint32_t fastrand(void);
void fastsrand(uint32_t seed);

int lives_list_strcmp_index(LiVESList *list, livesconstpointer data);

lives_cancel_t check_for_bad_ffmpeg(void);

//callbacks.c
void lives_exit(int signum);
void lives_notify(int msgnumber,const char *msgstring);
const char *get_set_name(void);
void count_opening_frames(void);
void on_fileread_clicked(LiVESFileChooser *, livespointer widget);
boolean dirchange_callback(LiVESAccelGroup *, LiVESObject *, uint32_t, LiVESXModifierType, livespointer user_data);
void on_effects_paused(LiVESButton *, livespointer user_data);
void on_cancel_keep_button_clicked(LiVESButton *, livespointer user_data);
void on_cleardisk_activate(LiVESWidget *, livespointer user_data);
void on_cleardisk_advanced_clicked(LiVESWidget *, livespointer user_data);
void popup_lmap_errors(LiVESMenuItem *, livespointer);
void on_filesel_button_clicked(LiVESButton *, livespointer user_data);
void switch_clip(int type, int newclip, boolean force);
void on_details_button_clicked(void);



// paramspecial.c
LiVESPixbuf *mt_framedraw(lives_mt *, LiVESPixbuf *);


// effects-weed.c
livespointer _lives_malloc(size_t size);
livespointer lives_memcpy(livespointer dest, livesconstpointer src, size_t n);
livespointer lives_memset(livespointer s, int c, size_t n);
void _lives_free(livespointer ptr); ///< calls mainw->free_fn
livespointer lives_calloc(size_t n_blocks, size_t n_block_bytes);
livespointer _lives_realloc(livespointer ptr, size_t new_size);


// pangotext.c
boolean subtitles_init(lives_clip_t *sfile, char *fname, lives_subtitle_type_t);
void subtitles_free(lives_clip_t *sfile);
boolean get_srt_text(lives_clip_t *sfile, double xtime);
boolean get_sub_text(lives_clip_t *sfile, double xtime);
boolean save_sub_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename);
boolean save_srt_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename);

#include "osc_notify.h"

// inlines
#define cfile mainw->files[mainw->current_file]
#define clipboard mainw->files[0]

#define PREFS_TIMEOUT 10000000 ///< 10 seconds

#define LIVES_TV_CHANNEL1 "http://www.serverwillprovide.com/sorteal/livestvclips/livestv.ogm"


// round (double) a up to next (integer) multiple of (double) b
#define CEIL(a,b) ((int)(((double)a+(double)b-.000000001)/((double)b))*b)

#ifdef NEED_ENDIAN_TEST
#undef NEED_ENDIAN_TEST
static int32_t testint = 0x12345678;
#define IS_BIG_ENDIAN (((char *)&testint)[0] == 0x12)
#endif


char *dummychar;

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
#define LIVES_ERROR(x)      {fprintf(stderr, "LiVES error: %s\n", x); break_me();}
#else // LIVES_NO_ERROR
#define LIVES_ERROR(x)      dummychar = x
#endif // LIVES_NO_ERROR
#endif // LIVES_ERROR

#ifndef LIVES_FATAL
#ifndef LIVES_NO_FATAL
#define LIVES_FATAL(x)      {fprintf(stderr, "LiVES fatal: %s\n", x); raise (LIVES_SIGSEGV);}
#else // LIVES_NO_FATAL
#define LIVES_FATAL(x)      dummychar = x
#endif // LIVES_NO_FATAL
#endif // LIVES_FATAL


#endif // #ifndef HAS_LIVES_MAIN_H

