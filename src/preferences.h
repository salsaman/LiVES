// preferences.h
// LiVES (lives-exe)
// (c) G. Finch (salsaman@gmail.com) 2004 - 2013
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PREFS_H
#define HAS_LIVES_PREFS_H

#define LIVES_PREFS_TIMEOUT  (10 * U_SEC) // 10 sec timeout

#define PREFS_PANED_POS ((int)(200.*widget_opts.scale))

typedef struct {
  gchar bgcolour[256];
  boolean stop_screensaver;
  boolean open_maximised;
  gchar theme[64];  ///< the theme name

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

  gchar aplayer[512]; // name, eg. "jack","pulse","sox","mplayer","mplayer2"

  /// frame quantisation type
  short q_type;
#define Q_FILL 1
#define Q_SMOOTH 1

  gchar tmpdir[PATH_MAX];  ///< kept in locale encoding

  // TODO - also use cur_vid_load_dir, etc.
  // utf8 encoding
  gchar def_vid_load_dir[PATH_MAX];
  gchar def_vid_save_dir[PATH_MAX];
  gchar def_audio_dir[PATH_MAX];
  gchar def_image_dir[PATH_MAX];
  gchar def_proj_dir[PATH_MAX];

  // locale encoding
  gchar prefix_dir[PATH_MAX];
  gchar lib_dir[PATH_MAX];


  gchar image_ext[16];

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

  gchar effect_command[256];
  gchar video_open_command[256];
  gchar audio_play_command[256];
  gchar cdplay_device[PATH_MAX];  ///< locale encoding
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
  gchar *wm; ///<window manager name
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

  gchar jack_tserver[256];
  gchar jack_aserver[256];

  gchar *fxdefsfile;
  gchar *fxsizesfile;
  gchar *vppdefaultsfile;

  GList *acodec_list;
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

  gchar ar_clipset_name[128]; ///< utf8 (not converted (to locale?))
  gchar ar_layout_name[PATH_MAX];  ///< utf8 (not converted) 

  boolean rec_desktop_audio;

  boolean show_gui;
  boolean show_splash;
  boolean show_playwin;

  boolean osc_start;

  boolean collate_images;

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

  gchar omc_js_fname[PATH_MAX];  ///< utf8
  gchar omc_midi_fname[PATH_MAX]; ///< utf8
  
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

  boolean lamp_buttons;

  boolean autoload_subs;

  int rec_stop_gb;

  int max_modes_per_key; ///< maximum effect modes per key

  // autotransitioning in mt
  int atrans_fx;
  gchar def_autotrans[256];

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
  gchar yuvin[PATH_MAX];
#endif

  GList *disabled_decoders;

  char backend_sync[PATH_MAX];
  char backend[PATH_MAX];

  char weed_plugin_path[PATH_MAX];
  char frei0r_path[PATH_MAX];
  char ladspa_path[PATH_MAX];

  boolean present;

  boolean ce_thumb_mode;

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
  gulong encoder_ofmt_fn;
  gulong encoder_name_fn;
  GtkWidget *prefs_dialog;
  
  GtkWidget *prefs_list;
  GtkWidget *prefs_table;
  GtkWidget *vbox_right_gui;
  GtkWidget *vbox_right_multitrack;
  GtkWidget *vbox_right_decoding;
  GtkWidget *vbox_right_playback;
  GtkWidget *vbox_right_recording;
  GtkWidget *vbox_right_encoding;
  GtkWidget *vbox_right_effects;
  GtkWidget *table_right_directories;
  GtkWidget *vbox_right_warnings;
  //  GtkWidget *vbox_right_warnings_outer;
  GtkWidget *vbox_right_misc;
  GtkWidget *vbox_right_themes;
  GtkWidget *vbox_right_net;
  GtkWidget *vbox_right_jack;
  GtkWidget *vbox_right_midi;
  GtkWidget *scrollw_right_gui;
  GtkWidget *scrollw_right_multitrack;
  GtkWidget *scrollw_right_decoding;
  GtkWidget *scrollw_right_playback;
  GtkWidget *scrollw_right_recording;
  GtkWidget *scrollw_right_encoding;
  GtkWidget *scrollw_right_effects;
  GtkWidget *scrollw_right_directories;
  GtkWidget *scrollw_right_warnings;
  GtkWidget *scrollw_right_misc;
  GtkWidget *scrollw_right_themes;
  GtkWidget *scrollw_right_net;
  GtkWidget *scrollw_right_jack;
  GtkWidget *scrollw_right_midi;
  GtkWidget *right_shown;
  GtkWidget *cancelbutton;
  GtkWidget *applybutton;
  GtkWidget *closebutton;
  GtkWidget *stop_screensaver_check;
  GtkWidget *open_maximised_check;
  GtkWidget *show_tool;
  GtkWidget *mouse_scroll;
  GtkWidget *fs_max_check;
  GtkWidget *recent_check;
  GtkWidget *video_open_entry;
  GtkWidget *audio_command_entry;
  GtkWidget *vid_load_dir_entry;
  GtkWidget *vid_save_dir_entry;
  GtkWidget *audio_dir_entry;
  GtkWidget *image_dir_entry;
  GtkWidget *proj_dir_entry;
  GtkWidget *tmpdir_entry;
  GtkWidget *cdplay_entry;
  GtkWidget *spinbutton_def_fps;
  GtkWidget *pbq_combo;
  GtkWidget *ofmt_combo;
  GtkWidget *audp_combo;
  GtkWidget *rframes;
  GtkWidget *rfps;
  GtkWidget *rclips;
  GtkWidget *reffects;
  GtkWidget *raudio;
  GtkWidget *rextaudio;
  GtkWidget *rdesk_audio;
  GtkWidget *encoder_combo;
  GtkWidget *checkbutton_antialias;
  GtkWidget *checkbutton_threads;
  GtkWidget *spinbutton_warn_ds;
  GtkWidget *spinbutton_crit_ds;
  GtkWidget *checkbutton_warn_fps;
  GtkWidget *checkbutton_warn_mplayer;
  GtkWidget *checkbutton_warn_save_set;
  GtkWidget *checkbutton_warn_dup_set;
  GtkWidget *checkbutton_warn_rendered_fx;
  GtkWidget *checkbutton_warn_encoders;
  GtkWidget *checkbutton_warn_layout_clips;
  GtkWidget *checkbutton_warn_layout_close;
  GtkWidget *checkbutton_warn_layout_delete;
  GtkWidget *checkbutton_warn_layout_alter;
  GtkWidget *checkbutton_warn_layout_shift;
  GtkWidget *checkbutton_warn_layout_adel;
  GtkWidget *checkbutton_warn_layout_aalt;
  GtkWidget *checkbutton_warn_layout_ashift;
  GtkWidget *checkbutton_warn_layout_popup;
  GtkWidget *checkbutton_warn_discard_layout;
  GtkWidget *checkbutton_warn_after_dvgrab;
  GtkWidget *checkbutton_warn_no_pulse;
  GtkWidget *checkbutton_warn_layout_wipe;
  GtkWidget *checkbutton_show_stats;
  GtkWidget *checkbutton_warn_fsize;
  GtkWidget *checkbutton_warn_mt_achans;
  GtkWidget *checkbutton_warn_mt_no_jack;
  GtkWidget *checkbutton_warn_yuv4m_open;
  GtkWidget *checkbutton_warn_mt_backup_space;
  GtkWidget *checkbutton_warn_after_crash;
  GtkWidget *spinbutton_warn_fsize;
  GtkWidget *spinbutton_bwidth;
  GtkWidget *theme_combo;
  GtkWidget *check_midi;
  GtkWidget *ins_speed;
  GtkWidget *jpeg;
  GtkWidget *mt_enter_prompt;
  GtkWidget *spinbutton_ocp;
  GtkWidget *acodec_combo;
  GtkWidget *spinbutton_osc_udp;
  GtkWidget *spinbutton_rte_keys;
  GtkWidget *spinbutton_nfx_threads;
  GtkWidget *enable_OSC;
  GtkWidget *enable_OSC_start;
  GtkWidget *jack_tserver_entry;
  GtkWidget *jack_aserver_entry;
  GtkWidget *checkbutton_jack_master;
  GtkWidget *checkbutton_jack_client;
  GtkWidget *checkbutton_jack_tb_start;
  GtkWidget *checkbutton_jack_tb_client;
  GtkWidget *checkbutton_jack_pwp;
  GtkWidget *checkbutton_jack_read_autocon;
  GtkWidget *checkbutton_start_tjack;
  GtkWidget *checkbutton_start_ajack;
  GtkWidget *checkbutton_afollow;
  GtkWidget *checkbutton_aclips;
  GtkWidget *spinbutton_mt_def_width;
  GtkWidget *spinbutton_mt_def_height;
  GtkWidget *spinbutton_mt_def_fps;
  GtkWidget *spinbutton_mt_undo_buf;
  GtkWidget *spinbutton_mt_ab_time;
  GtkWidget *spinbutton_rec_gb;
  GtkWidget *mt_autoback_every;
  GtkWidget *mt_autoback_always;
  GtkWidget *mt_autoback_never;
  GtkWidget *spinbutton_gmoni;
  GtkWidget *spinbutton_pmoni;
  GtkWidget *ce_thumbs;
  GtkWidget *checkbutton_mt_exit_render;
  GtkWidget *pertrack_checkbutton;
  GtkWidget *backaudio_checkbutton;
  GtkWidget *checkbutton_render_prompt;
  GtkWidget *checkbutton_instant_open;
  GtkWidget *checkbutton_auto_deint;
  GtkWidget *checkbutton_auto_trim;
  GtkWidget *checkbutton_nobord;
  GtkWidget *checkbutton_concat_images;
  GtkWidget *forcesmon;
  GtkWidget *forcesmon_hbox;
  GList *pbq_list;
  gchar *audp_name;
  gchar *orig_audp_name;
  gulong audp_entry_func;
  GtkWidget *checkbutton_omc_js;
  GtkWidget *checkbutton_omc_midi;
  GtkWidget *omc_js_entry;
  GtkWidget *omc_midi_entry;
  GtkWidget *spinbutton_midicr;
  GtkWidget *spinbutton_midirpt;
  GtkWidget *alsa_midi;
  GtkWidget *button_midid;
  GtkWidget *rb_startup_ce;
  GtkWidget *rb_startup_mt;
  GtkWidget *jack_int_label;
  GtkWidget *checkbutton_ce_maxspect;
  GtkWidget *temp_label;
  GtkWidget *checkbutton_stream_audio;
  GtkWidget *checkbutton_rec_after_pb;
  GtkWidget *wpp_entry;
  GtkWidget *frei0r_entry;
  GtkWidget *ladspa_entry;
  GtkWidget *cdda_hbox;
  GtkWidget *midi_hbox;
  GtkTreeSelection *selection;
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
  gchar tmpdir[PATH_MAX];
  gchar theme[64];
  gchar vpp_name[64]; ///< new video playback plugin
  int vpp_fixed_fps_numer;
  int vpp_fixed_fps_denom;
  double vpp_fixed_fpsd;
  int vpp_palette;
  int vpp_YUV_clamping;
  int vpp_fwidth;
  int vpp_fheight;
  int vpp_argc;
  gchar **vpp_argv;

  _encoder encoder;
  boolean show_recent;
  boolean show_tool;
  boolean osc_start;
  int startup_interface;
  uint32_t jack_opts;

  int nfx_threads;


  GList *disabled_decoders;
  GList *disabled_decoders_new;

} _future_prefs;

_prefs *prefs;
_future_prefs *future_prefs;
_prefsw *prefsw;

void set_acodec_list_from_allowed (_prefsw *, render_details *);
void  rdet_acodec_changed (GtkComboBox *acodec_combo, gpointer user_data);

_prefsw* create_prefs_dialog (void);

boolean on_prefs_delete_event (GtkWidget *, GdkEvent *, gpointer prefsw);

void on_preferences_activate (GtkMenuItem *, gpointer);

void on_prefs_close_clicked (GtkButton *, gpointer);

void on_prefs_revert_clicked (GtkButton *, gpointer);

void set_vpp(boolean set_in_prefs);

void on_prefDomainChanged(GtkTreeSelection *, gpointer);

void populate_combo_box(GtkComboBox *combo, GList *data);

void set_combo_box_active_string(GtkComboBox *, gchar *active_str);

void prefsw_set_astream_settings(_vid_playback_plugin *);
void prefsw_set_rec_after_settings(_vid_playback_plugin *);

void apply_button_set_enabled(GtkWidget *widget, gpointer func_data);


// permissions

#define LIVES_PERM_OSC_PORTS 1

boolean lives_ask_permission(int what);


#endif
