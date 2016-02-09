// preferences.h
// LiVES (lives-exe)
// (c) G. Finch (salsaman@gmail.com) 2004 - 2013
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PREFS_H
#define HAS_LIVES_PREFS_H

#define PREFS_PANED_POS ((int)(200.*widget_opts.scale))

typedef struct {
  char bgcolour[256];
  boolean stop_screensaver;
  boolean open_maximised;
  char theme[64];  ///< the theme name

  short pb_quality;
#define PB_QUALITY_LOW 1
#define PB_QUALITY_MED 2  ///< default
#define PB_QUALITY_HIGH 3

  _encoder encoder; ///< from main.h

  short audio_player;
#define AUD_PLAYER_SOX 1
#define AUD_PLAYER_MPLAYER 2
#define AUD_PLAYER_JACK 3
#define AUD_PLAYER_PULSE 4
#define AUD_PLAYER_MPLAYER2 5

  char aplayer[512]; // name, eg. "jack","pulse","sox","mplayer","mplayer2"

  /// frame quantisation type
  short q_type;
#define Q_FILL 1
#define Q_SMOOTH 1

  char tmpdir[PATH_MAX];  ///< kept in locale encoding

  // utf8 encoding
  char def_vid_load_dir[PATH_MAX];
  char def_vid_save_dir[PATH_MAX];
  char def_audio_dir[PATH_MAX];
  char def_image_dir[PATH_MAX];
  char def_proj_dir[PATH_MAX];

  // locale encoding
  char prefix_dir[PATH_MAX];
  char lib_dir[PATH_MAX];


  char image_ext[16];

  uint32_t warning_mask;
  // if these bits are set, we do not show the warning
#define WARN_MASK_FPS (1<<0)
#define WARN_MASK_FSIZE (1<<1)

  /// no longer used
#define WARN_MASK_SAVE_QUALITY (1<<2)

#define WARN_MASK_SAVE_SET (1<<3)
#define WARN_MASK_NO_MPLAYER (1<<4)
#define WARN_MASK_RENDERED_FX (1<<5)
#define WARN_MASK_NO_ENCODERS (1<<6)
#define WARN_MASK_LAYOUT_MISSING_CLIPS (1<<7)
#define WARN_MASK_LAYOUT_CLOSE_FILE (1<<8)
#define WARN_MASK_LAYOUT_DELETE_FRAMES (1<<9)

  /* next two are off by default (on a fresh install) */
#define WARN_MASK_LAYOUT_SHIFT_FRAMES (1<<10)
#define WARN_MASK_LAYOUT_ALTER_FRAMES (1<<11)

#define WARN_MASK_DUPLICATE_SET (1<<12)

#define WARN_MASK_EXIT_MT (1<<13)
#define WARN_MASK_DISCARD_SET (1<<14)
#define WARN_MASK_AFTER_DVGRAB (1<<15)

#define WARN_MASK_MT_ACHANS (1<<16)

#define WARN_MASK_LAYOUT_DELETE_AUDIO (1<<17)

  /* next two are off by default (on a fresh install) */
#define WARN_MASK_LAYOUT_SHIFT_AUDIO (1<<18)
#define WARN_MASK_LAYOUT_ALTER_AUDIO (1<<19)

#define WARN_MASK_MT_NO_JACK (1<<20)

#define WARN_MASK_OPEN_YUV4M (1<<21)

#define WARN_MASK_MT_BACKUP_SPACE (1<<22)

#define WARN_MASK_LAYOUT_POPUP (1<<23)

#define WARN_MASK_CLEAN_AFTER_CRASH (1<<24)

#define WARN_MASK_NO_PULSE_CONNECT (1<<25)

#define WARN_MASK_LAYOUT_WIPE (1<<26)

  char effect_command[256];
  char video_open_command[256];
  char audio_play_command[256];
  char cdplay_device[PATH_MAX];  ///< locale encoding
  double default_fps;
  int bar_height;
  boolean pause_effect_during_preview;
  boolean open_decorated;
  int sleep_time;
  boolean pause_during_pb;
  boolean fileselmax;
  boolean show_recent;
  int warn_file_size;
  boolean midisynch;
  int dl_bandwidth;
  boolean conserve_space;
  boolean ins_resample;
  boolean show_tool;
  short sepwin_type;
#define SEPWIN_TYPE_NON_STICKY 0
#define SEPWIN_TYPE_STICKY 1

  boolean show_player_stats;
  boolean show_framecount;
  boolean show_subtitles;
  boolean loop_recording;
  boolean discard_tv;
  boolean save_directories;
  boolean safer_preview;
  int rec_opts;
#define REC_FRAMES (1<<0)
#define REC_FPS (1<<1)
#define REC_EFFECTS (1<<2)
#define REC_CLIPS (1<<3)
#define REC_AUDIO (1<<4)
#define REC_AFTER_PB (1<<5)


  int audio_src;
#define AUDIO_SRC_INT 0
#define AUDIO_SRC_EXT 1

  boolean no_bandwidth;
  boolean osc_udp_started;
  uint32_t osc_udp_port;

  boolean omc_noisy; ///< send success/fail
  boolean omc_events; ///< send other events

  short startup_phase; ///< -1 = fresh install, 1 = tmpdir set, 2, pre-audio start, 3, pre-tests, 100 = all tests passed
  char *wm; ///<window manager name
  int ocp; ///< open_compression_percent : get/set in prefs

  boolean antialias;

  boolean ignore_tiny_fps_diffs;

  short rte_keys_virtual;

  uint32_t jack_opts;
#define JACK_OPTS_TRANSPORT_CLIENT (1<<0)   ///< jack can start/stop
#define JACK_OPTS_TRANSPORT_MASTER (1<<1)  ///< transport master
#define JACK_OPTS_START_TSERVER (1<<2)     ///< start transport server
#define JACK_OPTS_NOPLAY_WHEN_PAUSED (1<<3) ///< play audio even when transport paused
#define JACK_OPTS_START_ASERVER (1<<4)     ///< start audio server
#define JACK_OPTS_TIMEBASE_START (1<<5)    ///< jack sets play start position
#define JACK_OPTS_TIMEBASE_CLIENT (1<<6)    ///< full timebase client
#define JACK_OPTS_TIMEBASE_MASTER (1<<7)   ///< timebase master (not implemented yet)
#define JACK_OPTS_NO_READ_AUTOCON (1<<8)   ///< do not auto connect read clients when playing ext audio

  char jack_tserver[256];
  char jack_aserver[256];

  char *fxdefsfile;
  char *fxsizesfile;
  char *vppdefaultsfile;

  LiVESList *acodec_list;
  int acodec_list_to_format[AUDIO_CODEC_NONE];

  uint32_t audio_opts;
#define AUDIO_OPTS_FOLLOW_CLIPS (1<<0)
#define AUDIO_OPTS_FOLLOW_FPS (1<<1)

  boolean event_window_show_frame_events;
  boolean crash_recovery; ///< TRUE==maintain mainw->recovery file

  boolean show_rdet; ///< show render details (frame size, encoder type) before saving to file

  boolean move_effects;

  int mt_undo_buf;
  boolean mt_enter_prompt;

  int mt_def_width;
  int mt_def_height;
  double mt_def_fps;

  int mt_def_arate;
  int mt_def_achans;
  int mt_def_asamps;
  int mt_def_signed_endian;

  boolean mt_exit_render;
  boolean render_prompt;

  boolean mt_pertrack_audio;
  int mt_backaudio;

  int mt_auto_back;

  boolean ar_clipset;
  boolean ar_layout;

  char ar_clipset_name[128]; ///< locale
  char ar_layout_name[PATH_MAX];  ///< locale

  boolean rec_desktop_audio;

  boolean show_gui;
  boolean show_splash;
  boolean show_playwin;

  boolean osc_start;

  int virt_height; ///< n screens vert.

  boolean concat_images;

  boolean render_audio;
  boolean normalise_audio;

  boolean instant_open;
  boolean auto_deint;
  boolean auto_nobord;

  int gui_monitor;
  int play_monitor;

  boolean force_single_monitor;

  int midi_check_rate;
  int midi_rpt;

#define OMC_DEV_MIDI 1<<0
#define OMC_DEV_JS 1<<1
#define OMC_DEV_FORCE_RAW_MIDI 1<<2
  uint32_t omc_dev_opts;

  char omc_js_fname[PATH_MAX];  ///< utf8
  char omc_midi_fname[PATH_MAX]; ///< utf8

  boolean mouse_scroll_clips;

  int num_rtaudiobufs;

  boolean safe_symlinks;

#ifdef ALSA_MIDI
  boolean use_alsa_midi;
#endif

  int startup_interface;

#define STARTUP_CE 0
#define STARTUP_MT 1

  boolean ce_maxspect;

  boolean show_button_icons;

  boolean lamp_buttons;

  boolean autoload_subs;

  int rec_stop_gb;

  int max_modes_per_key; ///< maximum effect modes per key

  // autotransitioning in mt
  int atrans_fx;
  char def_autotrans[256];

  int nfx_threads;

  boolean alpha_post; ///< set to TRUE to force use of post alpha internally

  boolean stream_audio_out;
  boolean unstable_fx;
  boolean letterbox; ///< playback with letterbox
  boolean enc_letterbox; ///< encode with letterbox

  boolean force_system_clock; /// < force system clock (rather than soundcard) for timing ( better for high framerates )

  boolean force64bit;

  boolean auto_trim_audio;

  /** default 0; 1==use old (bad) behaviour on bigendian machines (r/w bigend ints/doubles); 2==bad reads, good writes */
  int bigendbug;


  // these are defualt values; actual values can be adjusted in Preferences
#define DEF_DS_WARN_LEVEL 250000000  // 250MB
  uint64_t ds_warn_level; ///< diskspace warn level bytes
#define DEF_DS_CRIT_LEVEL 20000000 // 20MB
  uint64_t ds_crit_level; ///< diskspace critical level bytes


#define LIVES_CDISK_LEAVE_ORPHAN_SETS (1<<0)
#define LIVES_CDISK_LEAVE_BFILES (1<<1)
#define LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS (1<<2)
#define LIVES_CDISK_LEAVE_MARKER_FILES (1<<3)
#define LIVES_CDISK_LEAVE_MISC_FILES (1<<4)

#define LIVES_CDISK_REMOVE_LOCK_FILES (1<<5) ///< not yet implemented - TODO
#define LIVES_CDISK_REBUILD_ORDER_FILES (1<<6) ///< not yet implemented - TODO


  uint32_t clear_disk_opts;

#ifdef HAVE_YUV4MPEG
  char yuvin[PATH_MAX];
#endif

  LiVESList *disabled_decoders;

  char backend_sync[PATH_MAX];
  char backend[PATH_MAX];

  char weed_plugin_path[PATH_MAX];
  char frei0r_path[PATH_MAX];
  char ladspa_path[PATH_MAX];

  boolean present;

  boolean ce_thumb_mode;

  boolean show_button_images;

  boolean push_audio_to_gens;

  boolean perm_audio_reader;

  boolean funky_widgets;

  int max_disp_vtracks;

} _prefs;

enum {
  LIST_ENTRY_GUI,
  LIST_ENTRY_DECODING,
  LIST_ENTRY_PLAYBACK,
  LIST_ENTRY_RECORDING,
  LIST_ENTRY_ENCODING,
  LIST_ENTRY_EFFECTS,
  LIST_ENTRY_DIRECTORIES,
  LIST_ENTRY_WARNINGS,
  LIST_ENTRY_MISC,
  LIST_ENTRY_THEMES,
  LIST_ENTRY_NET,
  LIST_ENTRY_JACK,
  LIST_ENTRY_MIDI,
  LIST_ENTRY_MULTITRACK
};

enum {
  LIST_ICON = 0,
  LIST_ITEM,
  LIST_NUM,
  N_COLUMNS
};


#define PREF_WIN_WIDTH (mainw->scr_width*.9)
#define PREF_WIN_HEIGHT (mainw->scr_height*.9)

#define DS_WARN_CRIT_MAX 1000000. ///< MB.

/// prefs window
typedef struct {
  ulong encoder_ofmt_fn;
  ulong encoder_name_fn;
  LiVESWidget *prefs_dialog;

  LiVESWidget *prefs_list;
  LiVESWidget *prefs_table;
  LiVESWidget *vbox_right_gui;
  LiVESWidget *vbox_right_multitrack;
  LiVESWidget *vbox_right_decoding;
  LiVESWidget *vbox_right_playback;
  LiVESWidget *vbox_right_recording;
  LiVESWidget *vbox_right_encoding;
  LiVESWidget *vbox_right_effects;
  LiVESWidget *table_right_directories;
  LiVESWidget *vbox_right_warnings;
  LiVESWidget *vbox_right_misc;
  LiVESWidget *vbox_right_themes;
  LiVESWidget *vbox_right_net;
  LiVESWidget *vbox_right_jack;
  LiVESWidget *vbox_right_midi;
  LiVESWidget *scrollw_right_gui;
  LiVESWidget *scrollw_right_multitrack;
  LiVESWidget *scrollw_right_decoding;
  LiVESWidget *scrollw_right_playback;
  LiVESWidget *scrollw_right_recording;
  LiVESWidget *scrollw_right_encoding;
  LiVESWidget *scrollw_right_effects;
  LiVESWidget *scrollw_right_directories;
  LiVESWidget *scrollw_right_warnings;
  LiVESWidget *scrollw_right_misc;
  LiVESWidget *scrollw_right_themes;
  LiVESWidget *scrollw_right_net;
  LiVESWidget *scrollw_right_jack;
  LiVESWidget *scrollw_right_midi;
  LiVESWidget *right_shown;
  LiVESWidget *cancelbutton;
  LiVESWidget *applybutton;
  LiVESWidget *closebutton;
  LiVESWidget *stop_screensaver_check;
  LiVESWidget *open_maximised_check;
  LiVESWidget *show_tool;
  LiVESWidget *mouse_scroll;
  LiVESWidget *fs_max_check;
  LiVESWidget *recent_check;
  LiVESWidget *video_open_entry;
  LiVESWidget *audio_command_entry;
  LiVESWidget *vid_load_dir_entry;
  LiVESWidget *vid_save_dir_entry;
  LiVESWidget *audio_dir_entry;
  LiVESWidget *image_dir_entry;
  LiVESWidget *proj_dir_entry;
  LiVESWidget *tmpdir_entry;
  LiVESWidget *cdplay_entry;
  LiVESWidget *spinbutton_def_fps;
  LiVESWidget *pbq_combo;
  LiVESWidget *ofmt_combo;
  LiVESWidget *audp_combo;
  LiVESWidget *rframes;
  LiVESWidget *rfps;
  LiVESWidget *rclips;
  LiVESWidget *reffects;
  LiVESWidget *raudio;
  LiVESWidget *rextaudio;
  LiVESWidget *rdesk_audio;
  LiVESWidget *encoder_combo;
  LiVESWidget *checkbutton_antialias;
  LiVESWidget *checkbutton_threads;
  LiVESWidget *spinbutton_warn_ds;
  LiVESWidget *spinbutton_crit_ds;
  LiVESWidget *checkbutton_warn_fps;
  LiVESWidget *checkbutton_warn_mplayer;
  LiVESWidget *checkbutton_warn_save_set;
  LiVESWidget *checkbutton_warn_dup_set;
  LiVESWidget *checkbutton_warn_rendered_fx;
  LiVESWidget *checkbutton_warn_encoders;
  LiVESWidget *checkbutton_warn_layout_clips;
  LiVESWidget *checkbutton_warn_layout_close;
  LiVESWidget *checkbutton_warn_layout_delete;
  LiVESWidget *checkbutton_warn_layout_alter;
  LiVESWidget *checkbutton_warn_layout_shift;
  LiVESWidget *checkbutton_warn_layout_adel;
  LiVESWidget *checkbutton_warn_layout_aalt;
  LiVESWidget *checkbutton_warn_layout_ashift;
  LiVESWidget *checkbutton_warn_layout_popup;
  LiVESWidget *checkbutton_warn_discard_layout;
  LiVESWidget *checkbutton_warn_after_dvgrab;
  LiVESWidget *checkbutton_warn_no_pulse;
  LiVESWidget *checkbutton_warn_layout_wipe;
  LiVESWidget *checkbutton_show_stats;
  LiVESWidget *checkbutton_warn_fsize;
  LiVESWidget *checkbutton_warn_mt_achans;
  LiVESWidget *checkbutton_warn_mt_no_jack;
  LiVESWidget *checkbutton_warn_yuv4m_open;
  LiVESWidget *checkbutton_warn_mt_backup_space;
  LiVESWidget *checkbutton_warn_after_crash;
  LiVESWidget *spinbutton_warn_fsize;
  LiVESWidget *spinbutton_bwidth;
  LiVESWidget *theme_combo;
  LiVESWidget *cbutton_fore;
  LiVESWidget *cbutton_back;
  LiVESWidget *cbutton_mabf;
  LiVESWidget *cbutton_mab;
  LiVESWidget *cbutton_infot;
  LiVESWidget *cbutton_infob;
  LiVESWidget *theme_style2;
  LiVESWidget *theme_style3;
  LiVESWidget *theme_style4;
  LiVESWidget *check_midi;
  LiVESWidget *ins_speed;
  LiVESWidget *jpeg;
  LiVESWidget *mt_enter_prompt;
  LiVESWidget *spinbutton_ocp;
  LiVESWidget *acodec_combo;
  LiVESWidget *spinbutton_osc_udp;
  LiVESWidget *spinbutton_rte_keys;
  LiVESWidget *spinbutton_nfx_threads;
  LiVESWidget *enable_OSC;
  LiVESWidget *enable_OSC_start;
  LiVESWidget *jack_tserver_entry;
  LiVESWidget *jack_aserver_entry;
  LiVESWidget *checkbutton_jack_master;
  LiVESWidget *checkbutton_jack_client;
  LiVESWidget *checkbutton_jack_tb_start;
  LiVESWidget *checkbutton_jack_tb_client;
  LiVESWidget *checkbutton_jack_pwp;
  LiVESWidget *checkbutton_jack_read_autocon;
  LiVESWidget *checkbutton_start_tjack;
  LiVESWidget *checkbutton_start_ajack;
  LiVESWidget *checkbutton_afollow;
  LiVESWidget *checkbutton_aclips;
  LiVESWidget *spinbutton_mt_def_width;
  LiVESWidget *spinbutton_mt_def_height;
  LiVESWidget *spinbutton_mt_def_fps;
  LiVESWidget *spinbutton_mt_undo_buf;
  LiVESWidget *spinbutton_mt_ab_time;
  LiVESWidget *spinbutton_max_disp_vtracks;
  LiVESWidget *spinbutton_rec_gb;
  LiVESWidget *mt_autoback_every;
  LiVESWidget *mt_autoback_always;
  LiVESWidget *mt_autoback_never;
  LiVESWidget *spinbutton_gmoni;
  LiVESWidget *spinbutton_pmoni;
  LiVESWidget *ce_thumbs;
  LiVESWidget *checkbutton_mt_exit_render;
  LiVESWidget *pertrack_checkbutton;
  LiVESWidget *backaudio_checkbutton;
  LiVESWidget *checkbutton_render_prompt;
  LiVESWidget *checkbutton_instant_open;
  LiVESWidget *checkbutton_auto_deint;
  LiVESWidget *checkbutton_auto_trim;
  LiVESWidget *checkbutton_nobord;
  LiVESWidget *checkbutton_concat_images;
  LiVESWidget *forcesmon;
  LiVESWidget *forcesmon_hbox;
  LiVESList *pbq_list;
  char *audp_name;
  char *orig_audp_name;
  ulong audp_entry_func;
  LiVESWidget *checkbutton_omc_js;
  LiVESWidget *checkbutton_omc_midi;
  LiVESWidget *omc_js_entry;
  LiVESWidget *omc_midi_entry;
  LiVESWidget *spinbutton_midicr;
  LiVESWidget *spinbutton_midirpt;
  LiVESWidget *alsa_midi;
  LiVESWidget *button_midid;
  LiVESWidget *rb_startup_ce;
  LiVESWidget *rb_startup_mt;
  LiVESWidget *jack_int_label;
  LiVESWidget *checkbutton_ce_maxspect;
  LiVESWidget *checkbutton_button_icons;
  LiVESWidget *temp_label;
  LiVESWidget *checkbutton_stream_audio;
  LiVESWidget *checkbutton_rec_after_pb;
  LiVESWidget *wpp_entry;
  LiVESWidget *frei0r_entry;
  LiVESWidget *ladspa_entry;
  LiVESWidget *cdda_hbox;
  LiVESWidget *midi_hbox;
  LiVESTreeSelection *selection;
  boolean needs_restart;
} _prefsw;

/// startup overrides from commandline
typedef struct {
  boolean ign_clipset;
  boolean ign_osc;
  boolean ign_aplayer;
  boolean ign_stmode;
  boolean ign_vppdefs;
} _ign_opts;

typedef struct {
  char tmpdir[PATH_MAX];
  char theme[64];
  char vpp_name[64]; ///< new video playback plugin
  int vpp_fixed_fps_numer;
  int vpp_fixed_fps_denom;
  double vpp_fixed_fpsd;
  int vpp_palette;
  int vpp_YUV_clamping;
  int vpp_fwidth;
  int vpp_fheight;
  int vpp_argc;
  char **vpp_argv;

  _encoder encoder;
  boolean show_recent;
  boolean show_tool;
  boolean osc_start;
  int startup_interface;
  uint32_t jack_opts;

  int nfx_threads;


  LiVESList *disabled_decoders;
  LiVESList *disabled_decoders_new;

} _future_prefs;

_prefs *prefs;
_future_prefs *future_prefs;
_prefsw *prefsw;

void set_acodec_list_from_allowed(_prefsw *, render_details *);
void  rdet_acodec_changed(LiVESCombo *acodec_combo, livespointer user_data);

_prefsw *create_prefs_dialog(void);

boolean on_prefs_delete_event(LiVESWidget *, LiVESXEvent *, livespointer prefsw);

void on_preferences_activate(LiVESMenuItem *, livespointer);

void on_prefs_close_clicked(LiVESButton *, livespointer);

void on_prefs_revert_clicked(LiVESButton *, livespointer);

void set_vpp(boolean set_in_prefs);

void on_prefDomainChanged(LiVESTreeSelection *, livespointer);

void populate_combo_box(LiVESCombo *, LiVESList *data);

void set_combo_box_active_string(LiVESCombo *, char *active_str);

void prefsw_set_astream_settings(_vid_playback_plugin *);
void prefsw_set_rec_after_settings(_vid_playback_plugin *);

void apply_button_set_enabled(LiVESWidget *widget, livespointer func_data);


// factories
enum {
  PREF_REC_EXT_AUDIO,
  PREF_AUDIO_OPTS,
  PREF_SEPWIN_STICKY,
  PREF_MT_EXIT_RENDER
};

void pref_factory_bool(int prefidx, boolean newval);
void pref_factory_int(int prefidx, int newval);
void pref_factory_bitmapped(int prefidx, int bitfield, boolean newval);


void get_pref(const char *key, char *val, int maxlen);
void get_pref_utf8(const char *key, char *val, int maxlen);
void get_pref_default(const char *key, char *val, int maxlen);
boolean get_boolean_pref(const char *key);
double get_double_pref(const char *key);
int get_int_pref(const char *key);
LiVESList *get_list_pref(const char *key);
void set_pref(const char *key, const char *value);
void delete_pref(const char *key);
void set_boolean_pref(const char *key, boolean value);
void set_double_pref(const char *key, double value);
void set_int_pref(const char *key, int value);
void set_int64_pref(const char *key, int64_t value);
void set_list_pref(const char *key, LiVESList *values);
boolean apply_prefs(boolean skip_warnings);
void save_future_prefs(void);

// permissions

#define LIVES_PERM_OSC_PORTS 1

boolean lives_ask_permission(int what);


#endif
