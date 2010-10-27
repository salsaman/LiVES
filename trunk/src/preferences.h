// preferences.h
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2010
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _HAS_PREFS_H
#define _HAS_PREFS_H



typedef struct {
  gchar bgcolour[256];
  gboolean stop_screensaver;
  gboolean open_maximised;
  gchar theme[64];  ///< the theme name

  gshort pb_quality;
#define PB_QUALITY_LOW 1
#define PB_QUALITY_MED 2  ///< default
#define PB_QUALITY_HIGH 3

  _encoder encoder; ///< from main.h

  gshort audio_player;
#define AUD_PLAYER_SOX 1
#define AUD_PLAYER_MPLAYER 2
#define AUD_PLAYER_JACK 3
#define AUD_PLAYER_PULSE 4

  /// frame quantisation type
  gshort q_type;
#define Q_FILL 1
#define Q_SMOOTH 1

  gchar tmpdir[256];  ///< kept in locale encoding

  // TODO - also use cur_vid_load_dir, etc.
  // utf8 encoding
  gchar def_vid_load_dir[256];
  gchar def_vid_save_dir[256];
  gchar def_audio_dir[256];
  gchar def_image_dir[256];
  gchar def_proj_dir[256];

  // locale encoding
  gchar prefix_dir[256];
  gchar lib_dir[256];


  gchar image_ext[16];

  guint warning_mask;
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


  gchar effect_command[256];
  gchar video_open_command[256];
  gchar audio_play_command[256];
  gchar cdplay_device[256];  ///< locale encoding
  gdouble default_fps;
  gint bar_height;
  gboolean pause_effect_during_preview;
  gboolean open_decorated;
  int sleep_time;
  gboolean pause_during_pb;
  gboolean fileselmax;
  gboolean show_recent;
  gint warn_file_size;
  gboolean pause_xmms;
  gboolean midisynch;
  gint dl_bandwidth;
  gboolean conserve_space;
  gboolean ins_resample;
  gboolean show_tool;
  gshort sepwin_type;
  gboolean show_player_stats;
  gboolean show_framecount;
  gboolean show_subtitles;
  gboolean loop_recording;
  gboolean discard_tv;
  gboolean save_directories;
  gboolean safer_preview;
  guint rec_opts;
#define REC_FRAMES (1<<0)
#define REC_FPS (1<<1)
#define REC_EFFECTS (1<<2)
#define REC_CLIPS (1<<3)
#define REC_AUDIO (1<<4)
  
  gboolean no_bandwidth;
  gboolean osc_udp_started;
  guint osc_udp_port;

  gboolean omc_noisy; ///< send success/fail
  gboolean omc_events; ///< send other events

  gshort startup_phase; ///< -1 = fresh install, 1 = tmpdir set, 2, pre-audio start, 3, pre-tests, 100 = all tests passed
  const gchar *wm; ///<window manager name
  gint ocp; ///< open_compression_percent : get/set in prefs

  gboolean antialias;

  gboolean ignore_tiny_fps_diffs;

  gshort rte_keys_virtual;

  guint jack_opts;
#define JACK_OPTS_TRANSPORT_CLIENT (1<<0)   ///< jack can start/stop
#define JACK_OPTS_TRANSPORT_MASTER (1<<1)  ///< transport master
#define JACK_OPTS_START_TSERVER (1<<2)     ///< start transport server
#define JACK_OPTS_NOPLAY_WHEN_PAUSED (1<<3) ///< play audio even when transport paused
#define JACK_OPTS_START_ASERVER (1<<4)     ///< start audio server
#define JACK_OPTS_TIMEBASE_START (1<<5)    ///< jack sets play start position
#define JACK_OPTS_TIMEBASE_CLIENT (1<<6)    ///< full timebase client
#define JACK_OPTS_TIMEBASE_MASTER (1<<7)   ///< timebase master (not implemented yet)

  gchar jack_tserver[256];
  gchar jack_aserver[256];

  gchar *fxdefsfile;
  gchar *fxsizesfile;
  gchar *vppdefaultsfile;

  GList *acodec_list;
  gint acodec_list_to_format[AUDIO_CODEC_NONE];

  guint audio_opts;
#define AUDIO_OPTS_FOLLOW_CLIPS (1<<0)
#define AUDIO_OPTS_FOLLOW_FPS (1<<1)

  gboolean event_window_show_frame_events;
  gboolean crash_recovery; ///< TRUE==maintain mainw->recovery file

  gboolean show_rdet; ///< show render details (frame size, encoder type) before saving to file

  gboolean move_effects;

  gint mt_undo_buf;
  gboolean mt_enter_prompt;

  gint mt_def_width;
  gint mt_def_height;
  gdouble mt_def_fps;

  gint mt_def_arate;
  gint mt_def_achans;
  gint mt_def_asamps;
  gint mt_def_signed_endian;

  gboolean mt_exit_render;
  gboolean render_prompt;

  gboolean mt_pertrack_audio;
  gint mt_backaudio;

  gint mt_auto_back;

  gboolean ar_clipset;
  gboolean ar_layout;

  gchar ar_clipset_name[128]; ///< utf8 (not converted)
  gchar ar_layout_name[128];  ///< utf8 (not converted)

  gboolean rec_desktop_audio;

  gboolean show_gui;
  gboolean show_splash;
  gboolean show_playwin;

  gboolean osc_start;

  gboolean show_threaded_dialog;

  gboolean collate_images;

  gint virt_height; ///< n screens vert.

  gboolean concat_images;

  gboolean render_audio;

  gboolean instant_open;
  gboolean auto_deint;

  gint gui_monitor;
  gint play_monitor;

  gboolean force_single_monitor;

  gint midi_check_rate;
  gint midi_rpt;

#define OMC_DEV_MIDI 1<<0
#define OMC_DEV_JS 1<<1
#define OMC_DEV_FORCE_RAW_MIDI 1<<2
  guint omc_dev_opts;

  gchar omc_js_fname[256];  ///< utf8
  gchar omc_midi_fname[256]; ///< utf8
  
  gboolean mouse_scroll_clips;

  gint num_rtaudiobufs;

  gboolean safe_symlinks;

#ifdef ALSA_MIDI
  gboolean use_alsa_midi;
#endif

  gint startup_interface;

#define STARTUP_CE 0
#define STARTUP_MT 1

  gboolean ce_maxspect;

  gboolean lamp_buttons;

  gboolean autoload_subs;

  glong rec_stop_gb;

  guint max_modes_per_key; ///< maximum effect modes per key

  // autotransitioning in mt
  gint atrans_fx;

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


#define PREF_WIN_WIDTH 960
#define PREF_WIN_HEIGHT 640


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
  GtkWidget *vbox_right_warnings_outer;
  GtkWidget *vbox_right_misc;
  GtkWidget *vbox_right_themes;
  GtkWidget *vbox_right_net;
  GtkWidget *vbox_right_jack;
  GtkWidget *vbox_right_midi;
  GtkWidget *scrollw;
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
  GtkWidget *rdesk_audio;
  GtkWidget *encoder_combo;
  GtkWidget *checkbutton_antialias;
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
  GtkWidget *checkbutton_show_stats;
  GtkWidget *check_xmms_pause;
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
  GtkWidget *enable_OSC;
  GtkWidget *enable_OSC_start;
  GtkWidget *jack_tserver_entry;
  GtkWidget *jack_aserver_entry;
  GtkWidget *checkbutton_jack_master;
  GtkWidget *checkbutton_jack_client;
  GtkWidget *checkbutton_jack_tb_start;
  GtkWidget *checkbutton_jack_tb_client;
  GtkWidget *checkbutton_jack_pwp;
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
  GtkWidget *checkbutton_mt_exit_render;
  GtkWidget *pertrack_checkbutton;
  GtkWidget *backaudio_checkbutton;
  GtkWidget *checkbutton_render_prompt;
  GtkWidget *checkbutton_instant_open;
  GtkWidget *checkbutton_auto_deint;
  GtkWidget *checkbutton_concat_images;
  GtkWidget *forcesmon;
  GList *pbq_list;
  gchar *audp_name;
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
  gboolean needs_restart;
} _prefsw;

/// startup overrides from commandline
typedef struct {
  gboolean ign_clipset;
  gboolean ign_osc;
  gboolean ign_jackopts;
  gboolean ign_aplayer;
  gboolean ign_stmode;
} _ign_opts;

typedef struct {
  gchar tmpdir[256];
  gchar theme[64];
  gchar vpp_name[64]; ///< new video playback plugin
  gint vpp_fixed_fps_numer;
  gint vpp_fixed_fps_denom;
  gdouble vpp_fixed_fpsd;
  int vpp_palette;
  int vpp_YUV_clamping;
  gint vpp_fwidth;
  gint vpp_fheight;
  gint vpp_argc;
  gchar **vpp_argv;

  _encoder encoder;
  gboolean show_recent;
  gboolean show_tool;
  gboolean osc_start;
  gint startup_interface;
  guint jack_opts;
} _future_prefs;

_prefs *prefs;
_future_prefs *future_prefs;
_prefsw *prefsw;

void set_acodec_list_from_allowed (_prefsw *, render_details *);
void  rdet_acodec_changed (GtkComboBox *acodec_combo, gpointer user_data);

_prefsw* create_prefs_dialog (void);

gboolean on_prefs_delete_event (GtkWidget *, GdkEvent *, gpointer prefsw);

void on_preferences_activate (GtkMenuItem *, gpointer);

void on_prefs_close_clicked (GtkButton *, gpointer);

void on_prefs_revert_clicked (GtkButton *, gpointer);

void set_vpp(gboolean set_in_prefs);

void on_prefDomainChanged(GtkTreeSelection *, gpointer);

void select_pref_list_row(guint);

void populate_combo_box(GtkComboBox *combo, GList *data);

void set_combo_box_active_string(GtkComboBox *combo, gchar *active_str);

#endif
