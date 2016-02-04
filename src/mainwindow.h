// mainwindow.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2003 - 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifndef HAS_LIVES_MAINWINDOW_H
#define HAS_LIVES_MAINWINDOW_H


#include <pthread.h>

#include "effects-data.h"

#include "audio.h"

#ifdef ALSA_MIDI
#include <alsa/asoundlib.h>
#endif

#ifdef HAVE_PULSE_AUDIO
#include "pulse.h"
#endif

// hardware related prefs

/// fraction of a second quantisation for event timing; must match U_SECL, and must be a multiple of 10>=1000000
///
/// >10**8 is not recommended, since we sometimes store seconds in a double
#define U_SEC 100000000.
#define U_SECL (int64_t)100000000
#define U_SEC_RATIO (U_SECL/1000000) ///< how many U_SECs (ticks) in a microsecond [default 100]


#define LIVES_SHORTEST_TIMEOUT  (2. * U_SEC) // 2 sec timeout
#define LIVES_SHORT_TIMEOUT  (5. * U_SEC) // 5 sec timeout
#define LIVES_DEFAULT_TIMEOUT  (10. * U_SEC) // 10 sec timeout
#define LIVES_LONGER_TIMEOUT  (20. * U_SEC) // 20 sec timeout
#define LIVES_LONGEST_TIMEOUT  (30. * U_SEC) // 30 sec timeout


/// rate to change pb fps when faster/slower pressed (TODO: make pref)
#define PB_CHANGE_RATE .005

/// forward/back scratch value (TODO: make pref)
#define PB_SCRATCH_VALUE 0.01

/////// GUI related constants /////////////////////////////////////////////////////////

// parameters for resizing the image frames, and for capture
#define V_RESIZE_ADJUST ((W_PACKING_WIDTH+2)*3)
#define H_RESIZE_ADJUST ((W_PACKING_HEIGHT+2)*2)

#if GTK_CHECK_VERSION(3,0,0)
#define CE_FRAME_HSPACE ((int)(320.*widget_opts.scale))
#else
#define CE_FRAME_HSPACE ((int)(420.*widget_opts.scale))
#endif

/// char width of start / end spinbuttons
#define SPBWIDTHCHARS 12

#define MIN_SEPWIN_WIDTH 600
#define MIN_SEPWIN_HEIGHT 36

/// sepwin/screen size safety margins in pixels
#define SCR_HEIGHT_SAFETY ((int)(100.*widget_opts.scale))
#define SCR_WIDTH_SAFETY ((int)(100.*widget_opts.scale))

/// default size for generators
#define DEF_GEN_WIDTH 640
#define DEF_GEN_HEIGHT 480

/// height of preview widgets in sepwin
#define PREVIEW_BOX_HT ((int)(100.*widget_opts.scale))

/// height of msg area
#define MSG_AREA_HEIGHT ((int)(50.*widget_opts.scale))

/// clip editor hrule height
#define CE_HRULE_HEIGHT ((int)(20.*widget_opts.scale))

/// clip edit vid/aud bar height
#define CE_VIDBAR_HEIGHT ((int)(10.*widget_opts.scale))

/// (unexpanded) height of rows in treeviews
#define TREE_ROW_HEIGHT ((int)(60.*widget_opts.scale))

// a few GUI specific settings
#define DEFAULT_FRAME_HSIZE ((int)(320.*widget_opts.scale))
#define DEFAULT_FRAME_VSIZE ((int)(200.*widget_opts.scale))

#define MAIN_SPIN_SPACER ((int)52.*widget_opts.scale) ///< pixel spacing for start/end spins for clip and multitrack editors

/// blank label to show so our message dialogs are not too small
#define PROCW_STRETCHER "                                                                                                                            "

#define ENC_DETAILS_WIN_H ((int)(640.*widget_opts.scale)) ///< horizontal size in pixels of the encoder output window
#define ENC_DETAILS_WIN_V ((int)(240.*widget_opts.scale)) ///< vertical size in pixels of the encoder output window

#define MIN_MSG_WIDTH_CHARS ((int)(40.*widget_opts.scale)) ///< min width of text on warning/error labels
#define MAX_MSG_WIDTH_CHARS ((int)(100.*widget_opts.scale)) ///< max width of text on warning/error labels

/// size of the fx dialog windows scrollwindow
#define RFX_WINSIZE_H ((int)(mainw->scr_width>=1024?(820.*widget_opts.scale):640))
#define RFX_WINSIZE_V ((int)(480.*widget_opts.scale))

#define RFX_TEXT_SCROLL_HEIGHT ((int)(80.*widget_opts.scale)) ///< height of textview scrolled window

#define DEF_BUTTON_WIDTH ((int)(80.*widget_opts.scale))

#define DEF_DIALOG_WIDTH RFX_WINSIZE_H
#define DEF_DIALOG_HEIGHT RFX_WINSIZE_V


////////////////////////////////////////////////////////////////////////////////////////////////////////

/// number of function keys
#define FN_KEYS 12

/// FX keys, 1 - 9 normally
#define FX_KEYS_PHYSICAL 9

/// must be >= FX_KEYS_PHYSICAL, and <=64 (number of bits in a 64bit int mask)
/// (max number of keys accesible through rte window or via OSC)
#define FX_KEYS_MAX_VIRTUAL 64

/// the rest of the keys are accessible through the multitrack renderer (must, be > FX_KEYS_MAX_VIRTUAL)
#define FX_KEYS_MAX 65536

#define EFFECT_NONE 0
#define GU641 ((uint64_t)1)

#define MAX_FX_THREADS 65536

#define LIVES_DCLICK_TIME 400 ///< double click time (milliseconds)

/// max ext_cntl + 1
#define MAX_EXT_CNTL 2

/// external control types
typedef enum {
  EXT_CNTL_NONE=-1, ///< not used
  EXT_CNTL_JS=0,
  EXT_CNTL_MIDI=1
} lives_ext_cntl_t;


/// timebase sources
typedef enum {
  LIVES_TIME_SOURCE_NONE=0,
  LIVES_TIME_SOURCE_SYSTEM,
  LIVES_TIME_SOURCE_SOUNDCARD,
  LIVES_TIME_SOURCE_EXTERNAL
} lives_time_source_t;



typedef enum {
  LIVES_TOY_NONE=0,
  LIVES_TOY_MAD_FRAMES,
  LIVES_TOY_TV,
  LIVES_TOY_AUTOLIVES
} lives_toy_t;


typedef enum {
  LIVES_DIALOG_INFO,
  LIVES_DIALOG_ERROR,
  LIVES_DIALOG_WARN,
  LIVES_DIALOG_WARN_WITH_CANCEL,
  LIVES_DIALOG_YESNO,
  LIVES_DIALOG_ABORT_CANCEL_RETRY
} lives_dialog_t;


/// various return conditions from rendering (multitrack or after recording)
typedef enum {
  LIVES_RENDER_ERROR_NONE=0,
  LIVES_RENDER_READY,
  LIVES_RENDER_PROCESSING,
  LIVES_RENDER_EFFECTS_PAUSED,
  LIVES_RENDER_COMPLETE,
  LIVES_RENDER_WARNING,
  LIVES_RENDER_WARNING_READ_FRAME,
  LIVES_RENDER_ERROR,
  LIVES_RENDER_ERROR_READ_AUDIO,
  LIVES_RENDER_ERROR_WRITE_AUDIO,
  LIVES_RENDER_ERROR_WRITE_FRAME,
} lives_render_error_t;


/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN=0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;



/// set in set_palette_colours()
typedef struct {
  int style;
#define STYLE_PLAIN 0 ///< no theme (theme 'none')
#define STYLE_1 1<<0 ///< turn on theming if set
#define STYLE_2 1<<1 ///< colour the spinbuttons on the front page if set
#define STYLE_3 1<<2 ///< style is lightish - allow themeing of widgets with dark text, otherwise use menu bg
#define STYLE_4 1<<3 ///< coloured bg for poly window in mt
#define STYLE_5 1<<4 ///< separator col. in mt

  LiVESWidgetColor white;
  LiVESWidgetColor black;
  LiVESWidgetColor light_blue;
  LiVESWidgetColor light_yellow;
  LiVESWidgetColor pink;
  LiVESWidgetColor light_red;
  LiVESWidgetColor dark_red;
  LiVESWidgetColor light_green;
  LiVESWidgetColor grey20;
  LiVESWidgetColor grey25;
  LiVESWidgetColor grey45;
  LiVESWidgetColor grey60;
  LiVESWidgetColor dark_orange;
  LiVESWidgetColor fade_colour;
  LiVESWidgetColor normal_back;
  LiVESWidgetColor normal_fore;

  LiVESWidgetColor menu_and_bars;
  LiVESWidgetColor menu_and_bars_fore;
  LiVESWidgetColor banner_fade_text;
  LiVESWidgetColor info_text;
  LiVESWidgetColor info_base;

} _palette;

/// screen details
typedef struct {
  int x;
  int y;
  int width;
  int height;
  LiVESXDevice *mouse_device; ///< unused for gtk+ < 3.0.0
  LiVESXDisplay *disp;
  LiVESXScreen *screen;
} lives_mgeometry_t;

/// constant strings
enum {
  LIVES_STRING_CONSTANT_ANY=0,
  LIVES_STRING_CONSTANT_NONE,
  LIVES_STRING_CONSTANT_RECOMMENDED,
  LIVES_STRING_CONSTANT_DISABLED,
  LIVES_STRING_CONSTANT_CL,
  LIVES_STRING_CONSTANT_BUILTIN,
  LIVES_STRING_CONSTANT_CUSTOM,
  LIVES_STRING_CONSTANT_TEST,
  NUM_LIVES_STRING_CONSTANTS
};


// file extensions
#define LIVES_FILE_EXT_PNG "png"
#define LIVES_FILE_EXT_JPG "jpg"
#define LIVES_FILE_EXT_MGK "mgk"
#define LIVES_FILE_EXT_PRE "pre"
#define LIVES_FILE_EXT_SCRAP "scrap"


typedef struct {
  double top;
  double left;
  double width;
  double height;
} lives_rect_t;


typedef struct {
  char *name;
  lives_rect_t *rects; // for future use
  int z_index; // for future use
} lives_screen_area_t;


/// where do we add the builtin tools in the tools menu
#define RFX_TOOL_MENU_POSN 2

/// mainw->
typedef struct {
  char msg[512];

  // files
  int current_file;
  int first_free_file;
  lives_clip_t *files[MAX_FILES+1]; ///< +1 for the clipboard
  char vid_load_dir[PATH_MAX];
  char vid_save_dir[PATH_MAX];
  char vid_dl_dir[PATH_MAX];
  char audio_dir[PATH_MAX];
  char image_dir[PATH_MAX];
  char proj_load_dir[PATH_MAX];
  char proj_save_dir[PATH_MAX];
  char recent_file[PATH_MAX];
  int untitled_number;
  int cap_number;
  int clips_available;

  /// hash table of clips in menu order
  LiVESList *cliplist;

  LiVESSList *clips_group;

  /// sets
  char set_name[256];   // actually 128 is the limit now, filesystem encoding

  // playback
  boolean faded;
  boolean double_size;
  boolean sep_win;
  boolean fs;
  boolean loop;
  boolean loop_cont;
  boolean ping_pong;
  boolean mute;
  boolean must_resize; ///< fixed playback size in gui; playback plugins have their own fwidth and fheight
  int audio_start;
  int audio_end;

  boolean ext_playback; ///< using external video playback plugin
  volatile boolean ext_keyboard; ///< keyboard codes must be polled from video playback plugin

  int ptr_x;
  int ptr_y;

  double fps_measure; ///< show fps stats after playback


  // flags
  boolean save_with_sound;
  boolean ccpd_with_sound;
  boolean selwidth_locked;
  boolean is_ready;
  boolean fatal; ///< got fatal signal
  boolean opening_loc;  ///< opening location (streaming)
  boolean dvgrab_preview;
  boolean switch_during_pb;
  boolean clip_switched; ///< for recording - did we switch clips ?
  boolean record;

  boolean in_fs_preview;
  volatile lives_cancel_t cancelled;

  boolean error;

  lives_cancel_type_t cancel_type;

  weed_plant_t *event_list; ///< current event_list, for recording
  weed_plant_t *stored_event_list; ///< stored mt -> clip editor
  boolean stored_event_list_changed;
  boolean stored_event_list_auto_changed;
  boolean stored_layout_save_all_vals;
  char stored_layout_name[PATH_MAX];

  LiVESList *stored_layout_undos;
  size_t sl_undo_buffer_used;
  unsigned char *sl_undo_mem;
  int sl_undo_offset;

  short endian;

  int pwidth; ///< playback width in RGB pixels
  int pheight; ///< playback height

  lives_whentostop_t whentostop;

  boolean noframedrop;

  int play_start;
  int play_end;
  boolean playing_sel;
  boolean preview;

  boolean is_processing;
  boolean is_rendering;
  boolean resizing;

  boolean foreign;  ///< for external window capture
  boolean record_foreign;
  boolean t_hidden;

  // recording from an external window
  uint32_t foreign_key;


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  Window foreign_id;
#else
  GdkNativeWindow foreign_id;
  GdkColormap *foreign_cmap;
  GdkPixmap *foreign_map;
#endif
#else
  Window foreign_id;
#endif

  LiVESXWindow *foreign_window;
  int foreign_width;
  int foreign_height;
  int foreign_bpp;
  char *foreign_visual;

  /// some VJ effects
  boolean nervous;

  lives_rfx_t *rendered_fx;
  int num_rendered_effects_builtin;
  int num_rendered_effects_custom;
  int num_rendered_effects_test;

  // for the merge dialog
  int last_transition_idx;
  int last_transition_loops;
  boolean last_transition_loop_to_fit;
  boolean last_transition_align_start;
  boolean last_transition_ins_frames;


  uint64_t rte; ///< current max for VJ mode == 64 effects on fg clip

  uint32_t last_grabbable_effect;
  int rte_keys; ///< which effect is bound to keyboard
  int num_tr_applied; ///< number of transitions active
  double blend_factor; ///< keyboard control parameter

  int blend_file;
  int last_blend_file;

  int scrap_file; ///< we throw odd sized frames here when recording in real time; used if a source is a generator or stream

  int ascrap_file; ///< scrap file for recording audio scraps

  /// which number file we are playing (or -1)
  volatile int playing_file;

  int pre_src_file; ///< video file we were playing before any ext input started
  int pre_src_audio_file; ///< audio file we were playing before any ext input started

  int scr_width;
  int scr_height;
  lives_toy_t toy_type;
  lives_pgid_t toy_alives_pgid; // 0, or thread for autolives toy
  boolean autolives_reset_fx;

  boolean toy_go_wild;

  // copy/paste
  boolean insert_after;
  boolean with_sound;

  // selection
  int sel_start;
  short sel_move;

  /// which bar should we move ?
#define SEL_MOVE_START 1
#define SEL_MOVE_END 2
#define SEL_MOVE_AUTO 3
#define SEL_MOVE_SINGLE 4

  // prefs (Save on exit)
  int prefs_changed;
#define PREFS_THEME_CHANGED (1<<0)
#define PREFS_JACK_CHANGED (1<<1)
#define PREFS_TEMPDIR_CHANGED (1<<2)
  boolean prefs_need_restart;

  /// default sizes for when no file is loaded
  int def_width;
  int def_height;

  /// for the framedraw preview - TODO use lives_framedraw_t array
  int framedraw_frame;


  /////////////////////////////////////////////////

  // end of static-ish info
  char first_info_file[PATH_MAX];
  boolean leave_files;
  boolean was_set;

  /// extra parameters for opening special files
  char *file_open_params;
  boolean open_deint;

  int last_dprint_file;
  boolean no_switch_dprint;

  /// actual frame being displayed
  int actual_frame;

  /// and the audio 'frame' for when we are looping
  double aframeno;

  // ticks are measured in 1/U_SEC of a second (by defalt a tick is 10 nano seconds)

  // for the internal player
  double period; ///< == 1./cfile->pb_fps (unless cfile->pb_fps is 0.)
  uint64_t startticks; ///< effective ticks when last frame was (should have been) displayed
  uint64_t timeout_ticks; ///< incremented if effect/rendering is paused/previewed
  uint64_t origsecs; ///< playback start seconds - subtracted from all other ticks to keep numbers smaller
  uint64_t origusecs; ///< usecs at start of playback - ditto
  uint64_t offsetticks; ///< offset for external transport
  uint64_t currticks; ///< current playback ticks (relative)
  uint64_t deltaticks; ///< deltaticks for scratching
  uint64_t firstticks; ///< ticks when audio started playing (for non-realtime audio plugins)
  uint64_t stream_ticks;  ///< ticks since first frame sent to playback plugin
  uint64_t last_display_ticks; /// currticks when last display was shown (used for fixed fps)

  boolean size_warn; ///< warn the user that incorrectly sized frames were found

  /// set to TRUE during frame load/display operation. If TRUE we should not switch clips,
  /// close the current clip, or call load_frame_image()
  boolean noswitch;
  int new_clip;

  int aud_file_to_kill; ///< # of audio file to kill on crash

  boolean reverse_pb; ///< used in osc.c

  /// TODO - make this a mutex and more finely grained : things we need to block are (clip switches, clip closure, effects on/off, etc)
  boolean osc_block;

  int osc_auto; ///< bypass user choices automatically

  /// encode width, height and fps set externally
  int osc_enc_width;
  int osc_enc_height;
  float osc_enc_fps;


  /// fixed fps playback; usually fixed_fpsd==0.
  int fixed_fps_numer;
  int fixed_fps_denom;
  double fixed_fpsd; ///< <=0. means free playback

  /// video playback plugin was updated; write settings to a file
  boolean write_vpp_file;

  volatile short scratch;
#define SCRATCH_NONE 0
#define SCRATCH_BACK -1
#define SCRATCH_FWD 1
#define SCRATCH_JUMP 2

  /// internal fx
  boolean internal_messaging;
  lives_render_error_t (*progress_fn)(boolean reset);

  volatile boolean threaded_dialog;

  // fx controls
  double fx1_val;
  double fx2_val;
  double fx3_val;
  double fx4_val;
  double fx5_val;
  double fx6_val;

  int fx1_start;
  int fx2_start;
  int fx3_start;
  int fx4_start;

  int fx1_step;
  int fx2_step;
  int fx3_step;
  int fx4_step;

  int fx1_end;
  int fx2_end;
  int fx3_end;
  int fx4_end;

  boolean fx1_bool;
  boolean fx2_bool;
  boolean fx3_bool;
  boolean fx4_bool;
  boolean fx5_bool;
  boolean fx6_bool;

  boolean effects_paused;
  boolean did_rfx_preview;

  uint32_t kb_timer;

  //function pointers
  ulong config_func;
  ulong pb_fps_func;
  ulong spin_start_func;
  ulong spin_end_func;
  ulong record_perf_func;
  ulong vidbar_func;
  ulong laudbar_func;
  ulong raudbar_func;
  ulong hrule_func;
  ulong toy_func_none;
  ulong toy_func_random_frames;
  ulong toy_func_lives_tv;
  ulong toy_func_autolives;
  ulong hnd_id;
  ulong loop_cont_func;
  ulong mute_audio_func;
  ulong fullscreen_cb_func;
  ulong sepwin_cb_func;

  // for jack transport
  boolean jack_can_stop;
  boolean jack_can_start;

  volatile boolean video_seek_ready;

  // selection pointers
  ulong mouse_fn1;
  boolean mouse_blocked;
  boolean hrule_blocked;

  /// stored clips
  int clipstore[FN_KEYS-1];

  /// key function for autorepeat ctrl-arrows
  uint32_t ksnoop;

  lives_mt *multitrack;

  int new_blend_file;

  LiVESWidget *frame1;
  LiVESWidget *frame2;
  LiVESWidget *freventbox0;
  LiVESWidget *freventbox1;
  LiVESWidget *playframe;
  LiVESWidget *pl_eventbox;
  LiVESPixbuf *imframe;
  LiVESPixbuf *camframe;
  LiVESPixbuf *imsep;
  LiVESWidget *LiVES;
  LiVESWidget *open;
  LiVESWidget *open_sel;
  LiVESWidget *open_vcd_menu;
  LiVESWidget *open_vcd_submenu;
  LiVESWidget *open_vcd;
  LiVESWidget *open_dvd;
  LiVESWidget *open_loc;
  LiVESWidget *open_utube;
  LiVESWidget *open_loc_menu;
  LiVESWidget *open_loc_submenu;
  LiVESWidget *open_yuv4m;
  LiVESWidget *open_lives2lives;
  LiVESWidget *send_lives2lives;
  LiVESWidget *open_device_menu;
  LiVESWidget *open_device_submenu;
  LiVESWidget *open_firewire;
  LiVESWidget *open_hfirewire;
  LiVESWidget *unicap;
  LiVESWidget *firewire;
  LiVESWidget *tvdev;
  LiVESWidget *recent_menu;
  LiVESWidget *recent_submenu;
  LiVESWidget *recent1;
  LiVESWidget *recent2;
  LiVESWidget *recent3;
  LiVESWidget *recent4;
  LiVESWidget *save_as;
  LiVESWidget *backup;
  LiVESWidget *restore;
  LiVESWidget *save_selection;
  LiVESWidget *close;
  LiVESWidget *import_proj;
  LiVESWidget *export_proj;
  LiVESWidget *sw_sound;
  LiVESWidget *clear_ds;
  LiVESWidget *ccpd_sound;
  LiVESWidget *quit;
  LiVESWidget *undo;
  LiVESWidget *redo;
  LiVESWidget *copy;
  LiVESWidget *cut;
  LiVESWidget *insert;
  LiVESWidget *paste_as_new;
  LiVESWidget *merge;
  LiVESWidget *xdelete;
  LiVESWidget *select_submenu;
  LiVESWidget *select_all;
  LiVESWidget *select_new;
  LiVESWidget *select_to_end;
  LiVESWidget *select_from_start;
  LiVESWidget *select_start_only;
  LiVESWidget *select_end_only;
  LiVESWidget *select_last;
  LiVESWidget *select_invert;
  LiVESWidget *lock_selwidth;
  LiVESWidget *record_perf;
  LiVESWidget *playall;
  LiVESWidget *playsel;
  LiVESWidget *playclip;
  LiVESWidget *rev_clipboard;
  LiVESWidget *stop;
  LiVESWidget *rewind;
  LiVESWidget *full_screen;
  LiVESWidget *loop_video;
  LiVESWidget *loop_continue;
  LiVESWidget *loop_ping_pong;
  LiVESWidget *sepwin;
  LiVESWidget *mute_audio;
  LiVESWidget *sticky;
  LiVESWidget *showfct;
  LiVESWidget *showsubs;
  LiVESWidget *letter;
  LiVESWidget *aload_subs;
  LiVESWidget *load_subs;
  LiVESWidget *erase_subs;
  LiVESWidget *fade;
  LiVESWidget *dsize;

  LiVESWidget *midi_learn;
  LiVESWidget *midi_save;
  LiVESWidget *change_speed;
  LiVESWidget *capture;
  LiVESWidget *load_audio;
  LiVESWidget *load_cdtrack;
  LiVESWidget *eject_cd;
  LiVESWidget *recaudio_submenu;
  LiVESWidget *recaudio_clip;
  LiVESWidget *recaudio_sel;
  LiVESWidget *export_submenu;
  LiVESWidget *export_allaudio;
  LiVESWidget *export_selaudio;
  LiVESWidget *append_audio;
  LiVESWidget *trim_submenu;
  LiVESWidget *trim_audio;
  LiVESWidget *trim_to_pstart;
  LiVESWidget *delaudio_submenu;
  LiVESWidget *delsel_audio;
  LiVESWidget *delall_audio;
  LiVESWidget *ins_silence;
  LiVESWidget *fade_aud_in;
  LiVESWidget *fade_aud_out;
  LiVESWidget *resample_audio;
  LiVESWidget *resample_video;
  LiVESWidget *preferences;
  LiVESWidget *rename;
  LiVESWidget *toys;
  LiVESWidget *toy_none;
  LiVESWidget *toy_random_frames;
  LiVESWidget *toy_tv;
  LiVESWidget *toy_autolives;
  LiVESWidget *show_file_info;
  LiVESWidget *show_file_comments;
  LiVESWidget *show_clipboard_info;
  LiVESWidget *show_messages;
  LiVESWidget *show_layout_errors;
  LiVESWidget *sel_label;
  LiVESAccelGroup *accel_group;
  LiVESWidget *sep_image;
  LiVESWidget *hruler;
  LiVESWidget *vj_menu;
  LiVESWidget *vj_save_set;
  LiVESWidget *vj_load_set;
  LiVESWidget *vj_show_keys;
  LiVESWidget *rte_defs_menu;
  LiVESWidget *rte_defs;
  LiVESWidget *save_rte_defs;
  LiVESWidget *vj_reset;
  LiVESWidget *mt_menu;
  LiVESWidget *troubleshoot;
  LiVESWidget *export_custom_rfx;
  LiVESWidget *delete_custom_rfx;
  LiVESWidget *edit_test_rfx;
  LiVESWidget *rename_test_rfx;
  LiVESWidget *delete_test_rfx;
  LiVESWidget *promote_test_rfx;

  /// for the fileselection preview
  LiVESWidget *fs_playarea;
  LiVESWidget *fs_playalign;
  LiVESWidget *fs_playframe;

  /// for the framedraw special widget - TODO - use a sub-struct
  LiVESWidget *framedraw; ///< the eventbox
  LiVESWidget *framedraw_reset; ///< the 'redraw' button
  LiVESWidget *framedraw_preview; ///< the 'redraw' button
  LiVESWidget *framedraw_spinbutton; ///< the frame number button
  LiVESWidget *framedraw_scale; ///< the slider
  LiVESWidget *fd_frame; ///< surrounding frame widget

  weed_plant_t *fd_layer_orig; ///< original layer uneffected
  weed_plant_t *fd_layer; ///< framedraw preview layer

  // bars here -> actually text above bars
  LiVESWidget *vidbar;
  LiVESWidget *laudbar;
  LiVESWidget *raudbar;

  LiVESWidget *spinbutton_end;
  LiVESWidget *spinbutton_start;

  LiVESWidget *arrow1;
  LiVESWidget *arrow2;

  lives_cursor_t cursor_style;

  weed_plant_t *filter_map; // the video filter map for rendering
  weed_plant_t *afilter_map; // the audio filter map for renering
  weed_plant_t *audio_event; // event for audio render tracking
  void ** *pchains; // parameter value chains for interpolation

  // for the internal player
  LiVESWidget *play_image;
  LiVESWidget *play_window;
  weed_plant_t *frame_layer;
  weed_plant_t *blend_layer;
  LiVESWidget *plug;

  // frame preview in the separate window
  LiVESWidget *preview_box;
  LiVESWidget *preview_image;
  LiVESWidget *preview_spinbutton;
  LiVESWidget *preview_scale;
  int preview_frame;
  ulong preview_spin_func;
  int prv_link;
#define PRV_FREE 0
#define PRV_START 1
#define PRV_END 2
#define PRV_PTR 3

  LiVESWidget *start_image;
  LiVESWidget *end_image;
  LiVESWidget *playarea;
  LiVESWidget *hseparator;
  LiVESWidget *scrolledwindow;
  LiVESWidget *message_box;

  LiVESWidget *textview1;
  LiVESWidget *clipsmenu;
  LiVESWidget *eventbox;
  LiVESWidget *eventbox2;
  LiVESWidget *eventbox3;
  LiVESWidget *eventbox4;
  LiVESWidget *eventbox5;

  // toolbar buttons
  LiVESWidget *t_stopbutton;
  LiVESWidget *t_bckground;
  LiVESWidget *t_fullscreen;
  LiVESWidget *t_sepwin;
  LiVESWidget *t_double;
  LiVESWidget *t_infobutton;

  LiVESWidget *t_slower;
  LiVESWidget *t_faster;
  LiVESWidget *t_forward;
  LiVESWidget *t_back;

  LiVESWidget *t_hide;

  LiVESWidget *toolbar;
  LiVESWidget *tb_hbox;
  LiVESWidget *fs1;
  LiVESWidget *vbox1;

  LiVESWidget *volume_scale;
  LiVESWidget *vol_toolitem;
  LiVESWidget *vol_label;

  // menubar buttons
  LiVESWidget *btoolbar; ///< button toolbar - clip editor
  LiVESWidget *m_sepwinbutton;
  LiVESWidget *m_playbutton;
  LiVESWidget *m_stopbutton;
  LiVESWidget *m_playselbutton;
  LiVESWidget *m_rewindbutton;
  LiVESWidget *m_loopbutton;
  LiVESWidget *m_mutebutton;
  LiVESWidget *menu_hbox;
  LiVESWidget *menubar;

  // separate window
  int opwx;
  int opwy;

  // sepwin buttons
  LiVESWidget *preview_controls;
  LiVESWidget *p_playbutton;
  LiVESWidget *p_playselbutton;
  LiVESWidget *p_rewindbutton;
  LiVESWidget *p_loopbutton;
  LiVESWidget *p_mutebutton;
  LiVESWidget *p_mute_img;

  // timer bars
  LiVESWidget *video_draw;
  LiVESWidget *laudio_draw;
  LiVESWidget *raudio_draw;

  lives_painter_surface_t *video_drawable;
  lives_painter_surface_t *laudio_drawable;
  lives_painter_surface_t *raudio_drawable;
  lives_painter_surface_t *blank_laudio_drawable;
  lives_painter_surface_t *blank_raudio_drawable;

  // framecounter
  LiVESWidget *framebar;
  LiVESWidget *framecounter;
  LiVESWidget *spinbutton_pb_fps;
  LiVESWidget *vps_label;
  LiVESWidget *curf_label;
  LiVESWidget *banner;

  // rendered effects
  LiVESWidget *effects_menu;
  LiVESWidget *tools_menu;
  LiVESWidget *utilities_menu;
  LiVESWidget *utilities_submenu;
  LiVESWidget *gens_menu;
  LiVESWidget *gens_submenu;
  LiVESWidget *run_test_rfx_submenu;
  LiVESWidget *run_test_rfx_menu;
  LiVESWidget *custom_effects_menu;
  LiVESWidget *custom_effects_submenu;
  LiVESWidget *custom_effects_separator;
  LiVESWidget *custom_tools_menu;
  LiVESWidget *custom_tools_submenu;
  LiVESWidget *custom_tools_separator;
  LiVESWidget *custom_gens_menu;
  LiVESWidget *custom_gens_submenu;
  LiVESWidget *custom_utilities_menu;
  LiVESWidget *custom_utilities_submenu;
  LiVESWidget *custom_utilities_separator;
  LiVESWidget *rte_separator;
  LiVESWidget *invis;

  int num_tracks;
  int *clip_index;
  int *frame_index;

  LiVESWidget *resize_menuitem;

  boolean only_close; ///< only close clips - do not exit
  volatile boolean is_exiting; ///< set during shutdown (inverse of only_close then)

  ulong pw_scroll_func;

#ifdef ENABLE_JACK
  jack_driver_t *jackd; ///< jack audio playback device
  jack_driver_t *jackd_read; ///< jack audio recorder device
#define RT_AUDIO
#else
  void *jackd;  ///< dummy
  void *jackd_read; ///< dummy
#endif

#ifdef HAVE_PULSE_AUDIO
  pulse_driver_t *pulsed; ///< pulse audio playback device
  pulse_driver_t *pulsed_read; ///< pulse audio recorder device
#define RT_AUDIO
#else
  void *pulsed;
  void *pulsed_read;
#endif

  // layouts
  LiVESTextBuffer *layout_textbuffer; ///< stores layout errors
  LiVESList *affected_layouts_map; ///< map of layouts with errors
  LiVESList *current_layouts_map; ///< map of all layouts for set

  /// list of pairs of marks in affected_layouts_map, text between them should be deleted when
  /// stored_layout is deleted
  LiVESList *affected_layout_marks;

  /// immediately (to be) affected layout maps
  LiVESList *xlays;

  char *recovery_file;  ///< the filename of our recover file
  boolean leave_recovery;

  boolean unordered_blocks; ///< are we recording unordered blocks ?

  boolean no_exit; ///< if TRUE, do not exit after saving set

  mt_opts multi_opts; ///< some multitrack options that survive between mt calls

  int rec_aclip;
  double rec_avel;
  double rec_aseek;

  LiVESMemVTable alt_vtable;

  pthread_mutex_t gtk_mutex;  ///< gtk drawing mutex - no longer used
  pthread_mutex_t interp_mutex;  ///< interpolation mutex - parameter interpolation must be single threaded

  pthread_mutex_t abuf_mutex;  ///< used to synch audio buffer request count - shared between audio and video threads
  pthread_mutex_t abuf_frame_mutex;  ///< used to synch audio buffer for generators
  pthread_mutex_t data_mutex[FX_KEYS_MAX];  ///< used to prevent data being connected while it is possibly being updated
  pthread_mutex_t fxd_active_mutex; ///< prevent simultaneous writing to active_dummy by audio and video threads
  pthread_mutex_t event_list_mutex; /// prevent simultaneous writing to event_list by audio and video threads
  pthread_mutex_t clip_list_mutex; /// prevent adding/removing to cliplist while another thread could be reading it

  volatile lives_rfx_t *vrfx_update;

  lives_fx_candidate_t
  fx_candidates[MAX_FX_CANDIDATE_TYPES]; ///< effects which can have candidates from which a delegate is selected (current examples are: audio_volume, resize)

  LiVESList *cached_list;  ///< cache of preferences or file header file (or NULL)
  FILE *clip_header;

  LiVESList *file_buffers;

  float volume; ///< audio volume level (for jack)

  int aud_rec_fd; ///< fd of file we are recording audio to
  double rec_end_time;
  int64_t rec_samples;
  double rec_fps;
  int rec_vid_frames;
  int rec_arate;
  int rec_achans;
  int rec_asamps;
  int rec_signed_endian;

  boolean suppress_dprint; ///< tidy up, e.g. by blocking "switched to file..." and "closed file..." messages

  boolean no_recurse; ///< flag to prevent recursive function calls

  char *string_constants[NUM_LIVES_STRING_CONSTANTS];
  char *any_string;  ///< localised text saying "Any", for encoder and output format
  char *none_string;  ///< localised text saying "None", for playback plugin name, etc.
  char *recommended_string;  ///< localised text saying "recommended", for encoder and output format
  char *disabled_string;  ///< localised text saying "disabled !", for playback plugin name, etc.
  char *cl_string; ///< localised text saying "*The current layout*", for layout warnings

  int opening_frames; ///< count of frames so far opened, updated after preview (currently)

  boolean show_procd; ///< override showing of "processing..." dialog

  boolean block_param_updates; ///< block visual param changes from updating real values
  boolean no_interp; ///< block interpolation (for single frame previews)

  weed_timecode_t cevent_tc; ///< timecode of currently processing event

  boolean opening_multi; ///< flag to indicate multiple file selection

  boolean record_paused; ///< pause during recording

  boolean record_starting; ///< start recording at next frame

  int img_concat_clip;  ///< when opening multiple, image files can get concatenated here (prefs->concat_images)

  /// rendered generators
  boolean gen_to_clipboard;
  boolean is_generating;

  boolean keep_pre;

  LiVESWidget *textwidget_focus;

  _vid_playback_plugin *vpp;

  /// multi-head support
  lives_mgeometry_t *mgeom;


  /// external control inputs
  boolean ext_cntl[MAX_EXT_CNTL];

#ifdef ALSA_MIDI
  snd_seq_t *seq_handle;
  int alsa_midi_port;
#endif

  weed_plant_t *rte_textparm; ///< send keyboard input to this paramter (usually NULL)

  int write_abuf; ///< audio buffer number to write to (for multitrack)
  volatile int abufs_to_fill;

  LiVESWidget *splash_window;
  LiVESWidget *splash_label;
  LiVESWidget *splash_progress;

  boolean recoverable_layout;

  boolean soft_debug; ///< for testing

  /// encoder text output
  LiVESIOChannel *iochan;
  LiVESTextView *optextview;

  boolean has_custom_tools;
  boolean has_custom_gens;
  boolean has_custom_utilities;

  /// decoders
  boolean decoders_loaded;
  LiVESList *decoder_list;

  boolean go_away;
  boolean debug; ///< debug crashes and asserts

  char *subt_save_file; ///< name of file to save subtitles to

  char **fonts_array;
  int nfonts;

  LiVESTargetEntry *target_table; ///< drag and drop target table

  LiVESList *videodevs;

  char vpp_defs_file[PATH_MAX];

  int log_fd;

  boolean jack_trans_poll;

#define LIVES_MAX_ALARMS 1024
#define LIVES_NO_ALARM_TICKS -1

  int64_t alarms[LIVES_MAX_ALARMS];
  int next_free_alarm;

  // stuff specific to audio gens (will be extended to all rt audio fx)
  volatile int agen_key; ///< which fx key is generating audio [1 based] (or 0 for none)
  volatile boolean agen_needs_reinit;
  uint64_t agen_samps_count; ///< count of samples since init

  boolean aplayer_broken;

  boolean com_failed;
  boolean write_failed;
  boolean read_failed;
  boolean chdir_failed;

  boolean add_clear_ds_button;
  boolean add_clear_ds_adv;
  boolean tried_ds_recover;

  boolean has_session_tmpdir;
  boolean startup_error;

  boolean kb_timer_end;

  boolean draw_blocked; // block drawing of timeline bars : prevents an infinite loop

  int ce_frame_height;
  int ce_frame_width;

  char *read_failed_file;
  char *write_failed_file;
  char *bad_aud_file;

  lives_render_error_t render_error;

  uint64_t next_ds_warn_level; ///< current disk space warning level for the tempdir

  float sepwin_scale;

  lives_pconnect_t *pconx; ///< list of out -> in param connections
  lives_cconnect_t *cconx; ///< list of out -> in alpha channel connections

  int overflow_height;

  int rowstride_alignment;
  int rowstride_alignment_hint;

  int sepwin_minwidth;
  int sepwin_minheight;

  uint32_t signal_caught;
  boolean signals_deferred;

  boolean ce_thumbs;
  boolean ce_upd_clip;

#define SCREEN_AREA_NONE -1
#define SCREEN_AREA_FOREGROUND 0
#define SCREEN_AREA_BACKGROUND 1
#define SCREEN_AREA_USER_DEFINED1 2

  int n_screen_areas; // number of screen areas
  int active_sa_fx; // active screen area for effects
  int active_sa_clips; // active screen area for clips
  lives_screen_area_t *screen_areas; // array of screen areas

  int active_track_list[MAX_TRACKS];
  boolean ext_src_used[MAX_FILES];
  lives_decoder_t *track_decoders[MAX_TRACKS];
  int old_active_track_list[MAX_TRACKS];

  boolean gen_started_play;
  boolean fx_is_auto;

  lives_audio_buf_t *audio_frame_buffer; ///< used for buffering / feeding audio to video generators
  int afbuffer_clients;

  pthread_t *libthread;
  ulong id;

  boolean interactive;

  int fc_buttonresponse;
  ////////////////////

} mainwindow;

extern _palette *palette;

typedef struct {
  ulong ins_frame_function;

  LiVESWidget *merge_dialog;
  LiVESWidget *ins_frame_button;
  LiVESWidget *drop_frame_button;
  LiVESWidget *param_vbox;
  LiVESWidget *spinbutton_loops;

  boolean loop_to_fit;
  boolean align_start;
  boolean ins_frames;

  int *list_to_rfx_index;
  LiVESList *trans_list;

} _merge_opts;

extern _merge_opts *merge_opts;

/// note, we can only have two of these currently, one for rendered effects, one for real time effects
/// 0 for rfx, 1 for rte
extern LiVESWidget *fx_dialog[2];


#define LIVES_SIGKILL SIGKILL
#define LIVES_SIGINT  SIGINT
#define LIVES_SIGPIPE SIGPIPE
#define LIVES_SIGUSR1 SIGUSR1
#define LIVES_SIGABRT SIGABRT
#define LIVES_SIGSEGV SIGSEGV
#define LIVES_SIGHUP  SIGHUP
#define LIVES_SIGTERM SIGTERM
#define LIVES_SIGQUIT SIGQUIT


#ifdef ENABLE_JACK
volatile aserver_message_t jack_message;
volatile aserver_message_t jack_message2;
#endif

#ifdef HAVE_PULSE_AUDIO
volatile aserver_message_t pulse_message;
volatile aserver_message_t pulse_message2;
#endif

#endif // HAS_LIVES_MAINWINDOW_H
