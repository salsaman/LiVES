// main.h
// LiVES
// (c) G. Finch (salsaman@xs4all.nl,salsaman@gmail.com) 2003 - 2012
// see file ../COPYING for full licensing details

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 or higher as 
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    agint64 with this program; if not, write to the Free Software
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


#ifndef HAS_LIVES_MAIN_H
#define HAS_LIVES_MAIN_H

#define GUI_GTK

#ifdef GUI_GTK

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef IS_LINUX_GNU
#define USE_X11
#endif

#ifdef IS_DARWIN
#define USE_X11
#endif

#ifdef USE_X11

// needed for GDK_WINDOW_XID - for fileselector preview

// needed for XMapWindow, GDK_WINDOW_XDISPLAY, GDK_WINDOW_XID, GTK_SOCKET
// for external window capture

// needed for gdk_x11_screen_get_window_manager_name()

#if GTK_CHECK_VERSION(3,0,0)
#include <gtk/gtkx.h>
#endif

#include <gdk/gdkx.h>
#endif

#endif // GUI_GTK

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#include "widget-helper.h"

#define QUOTEME(x) #x

/// don't change this unless the backend is changed as well
/// i.e. $GUI_BOOTSTRAP_FILE in smogrify
#define BOOTSTRAP_NAME "/tmp/.smogrify"

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

#define DOC_DIR "/share/doc/lives-"

#define THEME_DIR "/share/lives/themes/"
#define PLUGIN_SCRIPTS_DIR "/share/lives/plugins/"
#define PLUGIN_EXEC_DIR "/lives/plugins/"
#define ICON_DIR "/share/lives/icons/"
#define DATA_DIR "/share/lives/"
#define LIVES_CONFIG_DIR ".lives-dir/"

#define LIVES_DEVICE_DIR "/dev/"


#define LIVES_MANUAL_URL "http://lives.sourceforge.net/manual/"
#define LIVES_MANUAL_FILENAME "LiVES_manual.html"
#define LIVES_AUTHOR_EMAIL "mailto:salsaman@gmail.com"
#define LIVES_DONATE_URL "https://sourceforge.net/donate/index.php?group_id=64341"
#define LIVES_BUG_URL "http://sourceforge.net/tracker/?group_id=64341&atid=507139"
#define LIVES_FEATURE_URL "http://sourceforge.net/tracker/?group_id=64341&atid=507142"
#define LIVES_TRANSLATE_URL "https://translations.launchpad.net/lives/trunk"

#ifdef IS_DARWIN
#ifndef off64_t
#define off64_t off_t
#endif
#ifndef lseek64
#define lseek64 lseek
#endif
#endif

#define DEF_FILE_PERMS S_IRUSR|S_IWUSR
#define DEF_FILE_UMASK (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)^( DEF_FILE_PERMS )

/// LiVES will show a warning if this (MBytes) is exceeded on load
/// (can be overridden in prefs)
#define WARN_FILE_SIZE 500

/// maximum fps we will allow (gdouble)
#define FPS_MAX 200.

#define ENABLE_DVD_GRAB

#define FP_BITS 16 /// max fp bits [apparently 16 is faster]

#ifdef HAVE_MJPEGTOOLS
#define HAVE_YUV4MPEG
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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#else
#  define WARN_UNUSED
#endif

#ifdef PRODUCE_LOG
#define LIVES_LOG "lives.log"
#endif

typedef  void *(*fn_ptr) (void *ptr);


/// this struct is used only when physically resampling frames on the disk
/// we create an array of these and write them to the disk
typedef struct {
  gint value;
  gint64 reltime;
} event;

typedef struct {
  GtkWidget *processing;
  GtkWidget *progressbar;
  GtkWidget *label;
  GtkWidget *label2;
  GtkWidget *label3;
  GtkWidget *stop_button;
  GtkWidget *pause_button;
  GtkWidget *preview_button;
  GtkWidget *cancel_button;
  GtkWidget *scrolledwindow;
  guint frames_done;
} process;




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
  IMG_TYPE_UNKNOWN=(1<<0),
  IMG_TYPE_JPEG=(1<<1),
  IMG_TYPE_PNG=(1<<2)
} lives_image_type_t;



#define AFORM_SIGNED 0
#define AFORM_LITTLE_ENDIAN 0

#define AFORM_UNSIGNED 1
#define AFORM_BIG_ENDIAN (1<<1)
#define AFORM_UNKNOWN 65536


typedef struct {
  gshort red;
  gshort green;
  gshort blue;
} lives_colRGB24_t;

typedef struct {
  gshort red;
  gshort green;
  gshort blue;
  gshort alpha;
} lives_colRGBA32_t;


typedef enum {
  LIVES_INTERLACE_NONE=0,
  LIVES_INTERLACE_BOTTOM_FIRST=1,
  LIVES_INTERLACE_TOP_FIRST=2
} lives_interlace_t;


#include "pangotext.h"

/// corresponds to one clip in the GUI
typedef struct {
  // basic info (saved during backup)
  gint bpp;
  gdouble fps;
  gint hsize; ///< in pixels (NOT macropixels !)
  gint vsize;
  gint arps; ///< audio sample rate
  guint signed_endian; ///< bitfield

  gint arate; ///< audio playback rate
  gint64 unique_id;    ///< this and the handle can be used to uniquely id a file
  gint achans;
  gint asampsize;

  /////////////////
  gint frames;
  gchar title[256];
  gchar author[256];
  gchar comment[256];
  gchar keywords[256];
  ////////////////

  lives_interlace_t interlace; ///< interlace type (if known - none, topfirst, bottomfirst or : see plugins.h)

  // extended info (not saved)
  gint header_version;

#define LIVES_CLIP_HEADER_VERSION 100

  gint rowstride;

  /// the processing window
  process *proc_ptr;

  gchar handle[256];
  gint ohsize;
  gint ovsize;
  glong f_size;
  glong afilesize;
  gint old_frames; ///< for deordering, etc.
  gchar file_name[PATH_MAX]; ///< input file
  gchar info_file[PATH_MAX];
  gchar name[256];  ///< the display name
  gchar save_file_name[PATH_MAX];
  gchar type[40];
  gint start;
  gint end;
  gint insert_start;
  gint insert_end;
  gint progress_start;
  gint progress_end;
  gboolean changed;
  GtkWidget *menuentry;
  gboolean orig_file_name;
  gboolean was_renamed;
  gboolean is_untitled;
  gdouble pb_fps;
  gdouble freeze_fps;
  gboolean play_paused;
  
  //opening/restoring status
  gboolean opening;
  gboolean opening_audio;
  gboolean opening_only_audio;
  gboolean opening_loc;
  gboolean restoring;
  gboolean is_loaded;  ///< should we continue loading if we come back to this clip

  /// don't show preview/pause buttons on processing
  gboolean nopreview;

  /// don't show the 'keep' button - e.g. for operations which resize frames
  gboolean nokeep;

  // various times; total time is calculated as the gint64est of video, laudio and raudio
  gdouble total_time;
  gdouble video_time;
  gdouble laudio_time;
  gdouble raudio_time;
  gdouble pointer_time;

  // current and last played index frames for internal player
  gint frameno;
  gint last_frameno;



  /////////////////////////////////////////////////////////////
  // see resample.c for new events system


  // events 
  event *events[1];  ///<for block resampler

  weed_plant_t *event_list;
  weed_plant_t *event_list_back;
  weed_plant_t *next_event;

  GList *layout_map;
  ////////////////////////////////////////////////////////////////////////////////////////

  ///undo
  lives_undo_t undo_action;

  gint undo_start;
  gint undo_end;
  gchar undo_text[32];
  gchar redo_text[32];
  gboolean undoable;
  gboolean redoable;

  // used for storing undo values
  gint undo1_int;
  gint undo2_int;
  gint undo3_int;
  gint undo4_int;
  guint undo1_uint;
  gdouble undo1_dbl;
  gdouble undo2_dbl;
  gboolean undo1_boolean;
  gboolean undo2_boolean;
  gboolean undo3_boolean;

  gint undo_arate; ///< audio playback rate
  guint undo_signed_endian;
  gint undo_achans;
  gint undo_asampsize;
  gint undo_arps; ///< audio sample rate

  lives_clip_type_t clip_type;

  void *ext_src; ///< points to opaque source for non-disk types

  /// index of frames for CLIP_TYPE_FILE
  /// >0 means corresponding frame within original clip
  /// -1 means corresponding image file (equivalent to CLIP_TYPE_DISK)
  /// size must be >= frames, MUST be contiguous in memory
  int *frame_index;

  int *frame_index_back; ///< for undo

  gint fx_frame_pump; ///< rfx frame pump for virtual clips (CLIP_TYPE_FILE)

#define FX_FRAME_PUMP_VAL 200 ///< how many frames to prime the pump for realtime effects and resampler

#define IMG_BUFF_SIZE 4096 ///< chunk size for reading images

  gboolean ratio_fps; ///< if the fps was set by a ratio

  glong aseek_pos; ///< audio seek posn. (bytes) for when we switch clips

  // decoder data

  gchar mime_type[256];


  gboolean deinterlace; ///< auto deinterlace

  lives_image_type_t img_type;

  /// layout map for the current layout
  gint stored_layout_frame;
  gint stored_layout_idx;
  gdouble stored_layout_audio;
  gdouble stored_layout_fps;

  lives_subtitles_t *subt;

  gchar *op_dir;
  guint64 op_ds_warn_level; ///< current disk space warning level for any output directory

  gboolean no_proc_sys_errors; ///< skip system error dialogs in processing
  gboolean no_proc_read_errors; ///< skip read error dialogs in processing
  gboolean no_proc_write_errors; ///< skip write error dialogs in processing

  gboolean keep_without_preview; ///< allow keep, even when nopreview is set - TODO use only nopreview and nokeep

  // TODO - change to lives_clip_t
} file;



typedef struct {
  // the following can be assumed TRUE, they are checked on startup
  gboolean has_smogrify;
  gboolean smog_version_correct;
  gboolean can_read_from_config;
  gboolean can_write_to_config;
  gboolean can_write_to_tmp;
  gboolean can_write_to_tempdir;

  // the following may need checking before use
  gboolean has_xmms;
  gboolean has_dvgrab;
  gboolean has_sox;
  gboolean has_autolives;
  gboolean has_mplayer;
  gboolean has_convert;
  gboolean has_composite;
  gboolean has_cdda2wav;
  gboolean has_midistartstop;
  gboolean has_jackd;
  gboolean has_pulse_audio;
  gboolean has_xwininfo;
  gboolean has_gdb;

  /// home directory - default location for config file - locale encoding
  gchar home_dir[PATH_MAX];

  /// used for returning startup messages from the backend
  gchar startup_msg[256];

  // plugins
  gboolean has_encoder_plugins;

  gboolean has_python;

  gshort cpu_bits;

  gchar *myname_full;
  gchar *myname;

  gboolean has_stderr;

  gint nmonitors;

  gint ncpus;


#define LIVES_LITTLE_ENDIAN G_LITTLE_ENDIAN
#define LIVES_BIG_ENDIAN G_BIG_ENDIAN

  int byte_order;

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


// common defs for mainwindow
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
extern size_t sizint, sizdbl, sizshrt;



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
  CLIP_DETAILS_INTERLACE
} lives_clip_details_t;


typedef enum {
  LIVES_CURSOR_NORMAL=0,  ///< must be zero
  LIVES_CURSOR_BLOCK,
  LIVES_CURSOR_AUDIO_BLOCK,
  LIVES_CURSOR_BUSY,
  LIVES_CURSOR_FX_BLOCK
} lives_cursor_t;




// some useful functions

// interface.c
process* create_processing (const gchar *text);
void add_to_winmenu(void);
void remove_from_winmenu(void);
void make_play_window(void);
void resize_play_window (void);
void kill_play_window (void);
void make_preview_box (void);
void add_to_playframe (void);
GtkWidget* create_cdtrack_dialog (gint type, gpointer user_data);
GtkTextView *create_output_textview(void);
gchar *choose_file(gchar *dir, gchar *fname, gchar **filt, GtkFileChooserAction act, const char *title, GtkWidget *extra);
void add_suffix_check(GtkBox *box, const gchar *ext);


// dialogs.c
gboolean do_progress_dialog(gboolean visible, gboolean cancellable, const gchar *text);
gboolean do_warning_dialog(const gchar *text);
gboolean do_warning_dialog_with_check(const gchar *text, gint warn_mask_number);
gboolean do_warning_dialog_with_check_transient(const gchar *text, gint warn_mask_number, GtkWindow *transient);
gboolean do_yesno_dialog(const gchar *text) WARN_UNUSED;
int do_abort_cancel_retry_dialog(const gchar *text, GtkWindow *transient) WARN_UNUSED;
void do_error_dialog(const gchar *text);
void do_error_dialog_with_check(const gchar *text, gint warn_mask_number);
void do_blocking_error_dialog(const gchar *text);
void do_error_dialog_with_check_transient(const gchar *text, gboolean is_blocking,gint warn_mask_number, 
					  GtkWindow *transient);


void do_system_failed_error(const char *com, int retval, const char *addinfo);
int do_write_failed_error_s_with_retry(const gchar *fname, const gchar *errtext, GtkWindow *transient) WARN_UNUSED;
void do_write_failed_error_s(const char *filename, const char *addinfo);
int do_read_failed_error_s_with_retry(const gchar *fname, const gchar *errtext, GtkWindow *transient) WARN_UNUSED;
void do_read_failed_error_s(const char *filename, const char *addinfo);
gboolean do_header_write_error(int clip);
int do_header_read_error_with_retry(int clip) WARN_UNUSED;
int do_header_missing_detail_error(int clip, lives_clip_details_t detail) WARN_UNUSED;
void do_chdir_failed_error(const char *dir);
void handle_backend_errors(void);
gboolean check_backend_return(file *sfile);

/** warn about disk space */
gchar *ds_critical_msg(const gchar *dir, guint64 dsval);
gchar *ds_warning_msg(const gchar *dir, guint64 dsval, guint64 cwarn, guint64 nwarn);
gboolean check_storage_space(file *sfile, gboolean is_processing);

gchar *get_upd_msg(void);
gchar *get_new_install_msg(void);

gboolean ask_permission_dialog(int what);
gboolean do_abort_check(void);
void add_warn_check (GtkBox *box, gint warn_mask_number);
void do_memory_error_dialog (void);
void too_many_files(void);
void tempdir_warning (void);
void do_audio_import_error(void);
void do_mt_backup_space_error(lives_mt *, gint memreq_mb);

gboolean do_clipboard_fps_warning(void);
void perf_mem_warning(void);
void do_dvgrab_error(void);
gboolean do_comments_dialog (file *sfile, gchar *filename);
gboolean do_auto_dialog(const gchar *text, gint type);
void do_encoder_acodec_error (void);
void do_encoder_sox_error(void);
gboolean rdet_suggest_values (gint width, gint height, gdouble fps, gint fps_num, gint fps_denom, gint arate, 
			      gint asigned, gboolean swap_endian, gboolean anr, gboolean ignore_fps);
gboolean do_encoder_restrict_dialog (gint width, gint height, gdouble fps, gint fps_num, gint fps_denom, 
				     gint arate, gint asigned, gboolean swap_endian, gboolean anr, gboolean save_all);
void do_keys_window (void);
void do_mt_keys_window (void);
void do_messages_window (void);
void do_firstever_dialog (void);
void do_upgrade_error_dialog(void);
void do_no_mplayer_sox_error(void);
void do_aud_during_play_error(void);
void do_rendered_fx_dialog(void);
void do_layout_scrap_file_error(void);
void do_layout_ascrap_file_error(void);
void do_set_load_lmap_error(void);
gboolean do_set_duplicate_warning (const gchar *new_set);
gboolean do_set_rename_old_layouts_warning(const gchar *new_set);
gboolean do_layout_alter_frames_warning(void);
gboolean do_layout_alter_audio_warning(void);
gboolean do_yuv4m_open_warning(void);
void do_mt_undo_mem_error(void);
void do_mt_undo_buf_error(void);
void do_mt_set_mem_error(gboolean has_mt, gboolean trans);
void do_mt_audchan_error(gint warn_mask);
void do_mt_no_audchan_error(void);
void do_mt_no_jack_error(gint warn_mask);
gboolean do_mt_rect_prompt(void);
void do_audrate_error_dialog(void);
gboolean do_event_list_warning(void);
void do_nojack_rec_error(void);
void do_vpp_palette_error (void);
void do_vpp_fps_error (void);
void do_decoder_palette_error (void);
void do_rmem_max_error (gint size);
gboolean do_original_lost_warning(const gchar *fname);
void do_no_decoder_error(const gchar *fname);
void do_jack_noopen_warn(void);
void do_jack_noopen_warn2(void);
void do_jack_noopen_warn3(void);
void do_jack_noopen_warn4(void);
void do_file_perm_error(const gchar *file_name);
void do_dir_perm_error(const gchar *dir_name);
void do_encoder_img_ftm_error(render_details *rdet);
void do_after_crash_warning (void);
void do_bad_layout_error(void);
void do_card_in_use_error(void);
void do_dev_busy_error(const gchar *devstr);
gboolean do_existing_subs_warning(void);
void do_invalid_subs_error(void);
gboolean do_erase_subs_warning(void);
gboolean do_sub_type_warning(const gchar *ext, const gchar *type_ext);
gboolean do_move_tmpdir_dialog(void);
void do_set_locked_warning (const gchar *setname);
void do_no_in_vdevs_error(void);
void do_locked_in_vdevs_error(void);
void do_do_not_close_d (void);
void do_set_noclips_error(const char *setname);
void do_no_autolives_error(void);
void do_autolives_needs_clips_error(void);
void do_pulse_lost_conn_error(void);
void do_jack_lost_conn_error(void);

gboolean process_one (gboolean visible);
void do_threaded_dialog(gchar *translated_text, gboolean has_cancel);
void end_threaded_dialog(void);
void threaded_dialog_spin (void);
void response_ok (GtkButton *button, gpointer user_data);
void response_cancel (GtkButton *button, gpointer user_data);
void pump_io_chan(GIOChannel *iochan);

void do_splash_progress(void);


// d_print shortcuts
void d_print_cancelled(void);
void d_print_failed(void);
void d_print_done(void);
void d_print_file_error_failed(void);

// general
void do_text_window (const gchar *title, const gchar *text);

// saveplay.c
gboolean add_file_info(const gchar *check_handle, gboolean aud_only);
gboolean save_file_comments (int fileno);
void reget_afilesize (int fileno);
void deduce_file(const gchar *filename, gdouble start_time, gint end);
void open_file (const gchar *filename);
void open_file_sel(const gchar *file_name,gdouble start_time, gint frames);
void open_fw_device (void);
gboolean get_new_handle(gint index, const gchar *name);
gboolean get_temp_handle(gint index, gboolean create);
gboolean get_handle_from_info_file(gint index);
void create_cfile(void);
void save_file (int clip, int start, int end, const char *filename);
void play_file (void);
void save_frame (GtkMenuItem *menuitem, gpointer user_data);
gboolean save_frame_inner(gint clip, gint frame, const gchar *file_name, gint width, gint height, gboolean auto_overwrite);
void wait_for_stop (const gchar *stop_command);
gboolean save_clip_values(gint which_file);
void add_to_recovery_file (const gchar *handle);
void rewrite_recovery_file(void);
gboolean check_for_recovery_files (gboolean auto_recover);
void recover_layout_map(gint numclips);
const gchar *get_deinterlace_string(void);

// saveplay.c backup
void backup_file(int clip, int start, int end, const gchar *filename);
gint save_event_frames(void);
gboolean write_headers (file *file);

// saveplay.c restore
void restore_file(const gchar *filename);
gboolean read_headers(const gchar *file_name);

// saveplay.c sets
void open_set_file (const gchar *set_name, gint clipnum);


// saveplay.c scrap file
gboolean open_scrap_file (void);
gboolean open_ascrap_file (void);
gint save_to_scrap_file (weed_plant_t *layer);
gboolean load_from_scrap_file(weed_plant_t *layer, int frame);
void close_ascrap_file (void);
void close_scrap_file (void);


// main.c
void catch_sigint(int signum);
gboolean startup_message_fatal(const gchar *msg);
gboolean startup_message_nonfatal(const gchar *msg);
gboolean startup_message_nonfatal_dismissable(const gchar *msg, gint warning_mask);
capability *get_capabilities(void);
void get_monitors(void);
void load_start_image(gint frame);
void load_end_image(gint frame);
void load_preview_image(gboolean update_always);

gboolean pull_frame(weed_plant_t *layer, const gchar *image_ext, weed_timecode_t tc);
gboolean pull_frame_at_size (weed_plant_t *layer, const gchar *image_ext, weed_timecode_t tc, 
			     int width, int height, int target_palette);
GdkPixbuf *pull_gdk_pixbuf_at_size(gint clip, gint frame, const gchar *image_ext, weed_timecode_t tc, 
				   gint width, gint height, GdkInterpType interp);
GError * lives_pixbuf_save(GdkPixbuf *pixbuf, gchar *fname, lives_image_type_t imgtype, 
			   int quality, gboolean do_chmod, GError **gerrorptr);

void load_frame_image(gint frame);
void sensitize(void);
void desensitize(void);
void procw_desensitize(void);
void close_current_file (gint file_to_switch_to);  ///< close current file, and try to switch to file_to_switch_to
void get_next_free_file(void);
void switch_to_file(gint old_file, gint new_file);
void do_quick_switch (gint new_file);
void resize (gdouble scale);
gboolean read_file_details(const gchar *file_name, gboolean only_check_for_audio);
void do_start_messages(void);
void set_palette_colours (void);
void set_main_title(const gchar *filename, gint or_untitled_number);
void set_record (void);

//gui.c
//gui.c
void  create_LiVES (void);
void enable_record (void);
void toggle_record (void);
void disable_record (void);
void make_custom_submenus(void);
void fade_background(void);
void unfade_background(void);
void block_expose (void);
void unblock_expose (void);
void frame_size_update(void);
void splash_init(void);
void splash_end(void);
void splash_msg(const gchar *msg, gdouble pct);
void add_message_scroller(GtkWidget *conter);

// utils.c
#ifdef IS_IRIX
void setenv(const char *name, const char *val, int _xx);
#endif

// system calls in utils.c
int lives_system(const char *com, gboolean allow_error);
pid_t lives_fork(const char *com);
ssize_t lives_write(int fd, const void *buf, size_t count, gboolean allow_fail);
ssize_t lives_write_le(int fd, const void *buf, size_t count, gboolean allow_fail);
ssize_t lives_read(int fd, void *buf, size_t count, gboolean allow_less);
ssize_t lives_read_le(int fd, void *buf, size_t count, gboolean allow_less);
int lives_chdir(const char *path, gboolean allow_fail);
int lives_fputs(const char *s, FILE *stream);
char *lives_fgets(char *s, int size, FILE *stream);

char *filename_from_fd(char *val, int fd);


float LEFloat_to_BEFloat(float f);
uint64_t lives_10pow(int pow);
int get_approx_ln(guint val);
void lives_freep(void **ptr);
void lives_free(gpointer ptr);
void lives_free_with_check(gpointer ptr);
int lives_kill(pid_t pid, int sig);
int lives_killpg(int pgrp, int sig);
int64_t lives_get_current_ticks(void);
gboolean lives_alarm_get(int alarm_handle);
int lives_alarm_set(int64_t ticks);
void lives_alarm_clear(int alarm_handle);
lives_storage_status_t get_storage_status(const char *dir, guint64 warn_level, guint64 *dsval);
gchar *lives_format_storage_space_string(guint64 space);


gint myround(gdouble n);
void get_dirname(gchar *filename);
gchar *get_dir(const gchar *filename);
void get_basename(gchar *filename);
void get_filename(gchar *filename, gboolean strip_dir);
gchar *get_extension(const gchar *filename);
void d_print(const gchar *text);
void init_clipboard(void);
gboolean cache_file_contents(const gchar *filename);
gchar *get_val_from_cached_list(const gchar *key, size_t maxlen);
void get_pref(const gchar *key, gchar *val, gint maxlen);
void get_pref_utf8(const gchar *key, gchar *val, gint maxlen);
void get_pref_default(const gchar *key, gchar *val, gint maxlen);
gboolean get_boolean_pref(const gchar *key);
gdouble get_double_pref(const gchar *key);
gint get_int_pref(const gchar *key);
GList *get_list_pref(const gchar *key);
void get_location(const gchar *exe, gchar *val, gint maxlen);
void set_pref (const gchar *key, const gchar *value);
void delete_pref (const gchar *key);
void set_boolean_pref(const gchar *key, gboolean value);
void set_double_pref(const gchar *key, gdouble value);
void set_int_pref(const gchar *key, gint value);
void set_int64_pref(const gchar *key, gint64 value);
void set_list_pref(const char *key, GList *values);
gboolean apply_prefs(gboolean skip_warnings);
void save_future_prefs(void);
void set_menu_text(GtkWidget *menu, const gchar *text, gboolean use_mnemonic);
void get_menu_text(GtkWidget *menu, gchar *text);
void get_menu_text_long(GtkWidget *menuitem, gchar *text);
gint get_box_child_index (GtkBox *box, GtkWidget *tchild);
void reset_clip_menu (void);
void get_play_times(void);
void get_total_time (file *file);
guint get_signed_endian (gboolean is_signed, gboolean little_endian);
void fullscreen_internal(void);
void unhide_cursor(GdkWindow *window);
void hide_cursor(GdkWindow *window);
void set_alwaysontop(GtkWidget *window, gboolean ontop); ///< TODO - use for playwin
void colour_equal(GdkColor *c1, const GdkColor *c2);
void switch_to_int_player(void);
void switch_to_mplayer(void);
void switch_aud_to_sox(gboolean set_pref);
gboolean switch_aud_to_jack(void);
gboolean switch_aud_to_pulse(void);
void switch_aud_to_mplayer(gboolean set_pref);
void prepare_to_play_foreign(void);
gboolean after_foreign_play(void);
gboolean check_file(const gchar *file_name, gboolean check_exists);  ///< check if file exists
gboolean check_dir_access (const gchar *dir);
gulong get_file_size(int fd);
gulong sget_file_size(const gchar *name);
gulong get_fs_free(const char *dir);
gboolean is_writeable_dir(const char *dir);
gboolean ensure_isdir(gchar *fname);
gchar *ensure_extension(const gchar *fname, const gchar *ext) WARN_UNUSED;
gboolean check_dev_busy(gchar *devstr);
void activate_url_inner(const gchar *link);
void activate_url (GtkAboutDialog *about, const gchar *link, gpointer data);
void show_manual_section (const gchar *lang, const gchar *section);

gdouble calc_time_from_frame (gint clip, gint frame);
gint calc_frame_from_time (gint filenum, gdouble time);  ///< nearest frame start
gint calc_frame_from_time2 (gint filenum, gdouble time); ///< nearest frame end
gint calc_frame_from_time3 (gint filenum, gdouble time); ///< nearest frame mid

gboolean check_for_ratio_fps (gdouble fps);
gdouble get_ratio_fps(const gchar *string);
void calc_maxspect(gint rwidth, gint rheight, gint *cwidth, gint *cheight);

gchar *remove_trailing_zeroes(gdouble val);
void toggle_button_toggle (GtkToggleButton *tbutton);
void remove_layout_files(GList *lmap);
gboolean add_lmap_error(lives_lmap_error_t lerror, const gchar *name, gpointer user_data, 
			gint clipno, gint frameno, gdouble atime, gboolean affects_current);
void clear_lmap_errors(void);
gboolean prompt_remove_layout_files(void);
gboolean is_legal_set_name(const gchar *set_name, gboolean allow_dupes);
gchar *repl_tmpdir(const gchar *entry, gboolean fwd);
gchar *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp);
gboolean get_clip_value(int which, lives_clip_details_t, void *retval, size_t maxlen);
void save_clip_value(int which, lives_clip_details_t, void *val);
gboolean check_frame_count(gint idx);
void get_frame_count(gint idx);
gint count_resampled_frames (gint in_frames, gdouble orig_fps, gdouble resampled_fps);
gboolean int_array_contains_value(int *array, int num_elems, int value);
gboolean check_for_lock_file(const gchar *set_name, gint type);
void g_list_free_strings(GList *list);
void gtk_tooltips_copy(GtkWidget *dest, GtkWidget *source);
void adjustment_configure(GtkAdjustment *adjustment, gdouble value, gdouble lower, gdouble upper, 
			  gdouble step_increment, gdouble page_increment, gdouble page_size);
gboolean create_event_space(gint length_in_eventsb);
void add_to_recent(const gchar *filename, gdouble start, gint frames, const gchar *file_open_params);
gint verhash (gchar *version);
void set_undoable (const gchar *what, gboolean sensitive);
void set_redoable (const gchar *what, gboolean sensitive);
void zero_spinbuttons (void);
void draw_little_bars (gdouble ptrtime);
void set_sel_label (GtkWidget *label);
void clear_mainw_msg (void);
gint get_token_count (const gchar *string, int delim);
GdkPixmap* gdk_pixmap_copy (GdkPixmap *pixmap);
GdkPixbuf *gdk_pixbuf_new_blank(gint width, gint height, int palette);
void get_border_size (GtkWidget *win, gint *bx, gint *by);
gchar *g_strappend (gchar *string, gint len, const gchar *newbit);
GList *g_list_append_unique(GList *xlist, const gchar *add);
void find_when_to_stop (void);
gint calc_new_playback_position(gint fileno, gint64 otc, gint64 *ntc);
void calc_aframeno(gint fileno);
void minimise_aspect_delta (gdouble allowed_aspect,gint hblock,gint vblock,gint hsize,gint vsize,gint *width,gint *height);
GdkInterpType get_interp_value(gshort quality);
GList *g_list_move_to_first(GList *list, GList *item) WARN_UNUSED;
GList *g_list_delete_string(GList *, char *string) WARN_UNUSED;
GList *g_list_copy_strings(GList *list);
gboolean string_lists_differ(GList *, GList *);


GList *get_set_list(const gchar *dir);
void combo_set_popdown_strings (GtkCombo *combo, GList *list);

gchar *subst (const gchar *string, const gchar *from, const gchar *to);
gchar *insert_newlines(const gchar *text, int maxwidth);

gint hextodec (const gchar *string);
gint get_hex_digit (const gchar *c);

guint32 fastrand(void);
void fastsrand(guint32 seed);

gint lives_list_index (GList *list, const gchar *data);

void set_fg_colour(gint red, gint green, gint blue);
gboolean label_act_toggle (GtkWidget *, GdkEventButton *, GtkToggleButton *);
gboolean widget_act_toggle (GtkWidget *, GtkToggleButton *);

void lives_set_cursor_style(lives_cursor_t cstyle, GdkWindow *window);


gchar *text_view_get_text(GtkTextView *textview);
void text_view_set_text(GtkTextView *textview, const gchar *text);


// plugins.c
GList *get_external_window_hints(lives_rfx_t *rfx);
gboolean check_encoder_restrictions (gboolean get_extension, gboolean user_audio, gboolean save_all);

//callbacks.c
void lives_exit (void);
void count_opening_frames(void);
void on_fileread_clicked (GtkFileChooser *, gpointer widget);
gboolean dirchange_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer user_data);
void on_effects_paused (GtkButton *, gpointer user_data);
void on_cancel_keep_button_clicked (GtkButton *, gpointer user_data);
void on_cleardisk_activate (GtkWidget *, gpointer user_data);
void on_cleardisk_advanced_clicked (GtkWidget *, gpointer user_data);

// paramspecial.c
gboolean mt_framedraw(lives_mt *, GdkPixbuf *);

// paramwindow.c
void add_fill_to_box (GtkBox *);

// rte_window.c
void refresh_rte_window (void);

// effects-weed.c
void *w_memcpy  (void *dest, const void *src, size_t n);

// pangotext.c
gboolean subtitles_init(file *sfile, char * fname, lives_subtitle_type_t);
void subtitles_free(file *sfile);
gboolean get_srt_text(file *sfile, double xtime);
gboolean get_sub_text(file *sfile, double xtime);
gboolean save_sub_subtitles(file *sfile, double start_time, double end_time, double offset_time, const char *filename);
gboolean save_srt_subtitles(file *sfile, double start_time, double end_time, double offset_time, const char *filename);

// osc.c
#ifdef ENABLE_OSC
gboolean lives_osc_init(guint osc_udp_port);
gint lives_osc_poll(gpointer data);
void lives_osc_end(void);
gboolean lives_osc_notify(int msgtype, const char *msgstring);
void lives_osc_notify_success (const gchar *msg);
void lives_osc_notify_failure (void);
void lives_osc_notify_cancel (void);
#include "osc_notify.h"
#endif

// ldvgrab.c
#ifdef HAVE_LDVGRAB
void on_open_fw_activate (GtkMenuItem *menuitem, gpointer format);

#define CAM_FORMAT_DV 0
#define CAM_FORMAT_HDV 1

#endif

// inlines
#define cfile mainw->files[mainw->current_file]
#define clipboard mainw->files[0]

#define U82L(String) ( g_locale_from_utf8 (String,-1,NULL,NULL,NULL) ) 
#define L2U8(String) ( g_locale_to_utf8 (String,-1,NULL,NULL,NULL) ) 


#define PREFS_TIMEOUT 10000000 ///< 10 seconds // TODO !

#define LIVES_TV_CHANNEL1 "http://www.serverwillprovide.com/sorteal/livestvclips/livestv.ogm"


// round (double) a up to next (integer) multiple of (double) b
// haha
#define CEIL(a,b) ((int)(((double)a+(double)b-.000000001)/((double)b))*b)


// should sprinkle some of these around

gchar *dummychar;

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
#define LIVES_ERROR(x)      fprintf(stderr, "LiVES error: %s\n", x)
#else // LIVES_NO_ERROR
#define LIVES_ERROR(x)      dummychar = x
#endif // LIVES_NO_ERROR
#endif // LIVES_ERROR

#ifndef LIVES_FATAL
#ifndef LIVES_NO_FATAL
#define LIVES_FATAL(x)      fprintf(stderr, "LiVES fatal: %s\n", x)
#else // LIVES_NO_FATAL
#define LIVES_FATAL(x)      dummychar = x
#endif // LIVES_NO_FATAL
#endif // LIVES_FATAL


#endif // #ifndef HAS_LIVES_MAIN_H

