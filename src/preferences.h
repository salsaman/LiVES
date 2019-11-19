// preferences.h
// LiVES (lives-exe)
// (c) G. Finch (salsaman_lives@gmail.com) 2004 - 2019
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PREFS_H
#define HAS_LIVES_PREFS_H

#define PREFS_PANED_POS ((int)(300.*widget_opts.scale))

// for mainw->prefs_changed
#define PREFS_THEME_CHANGED (1<<0)
#define PREFS_JACK_CHANGED (1<<1)
#define PREFS_WORKDIR_CHANGED (1<<2)
#define PREFS_COLOURS_CHANGED (1<<3)
#define PREFS_XCOLOURS_CHANGED (1<<4)
#define PREFS_IMAGES_CHANGED (1<<5)
#define PREFS_MONITOR_CHANGED (1<<6)

#define PULSE_AUDIO_URL "http://www.pulseaudio.org"
#define JACK_URL "http://jackaudio.org"

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
#define AUD_PLAYER_NONE 0
#define AUD_PLAYER_SOX 1
#define AUD_PLAYER_JACK 2
#define AUD_PLAYER_PULSE 3

  // string forms
#define AUDIO_PLAYER_NONE "none"
#define AUDIO_PLAYER_SOX "sox"
#define AUDIO_PLAYER_JACK "jack"

#define AUDIO_PLAYER_PULSE "pulse" ///< used in pref and for external players (e.g -ao pulse, -aplayer pulse)
#define AUDIO_PLAYER_PULSE_AUDIO "pulseaudio" ///< used for display, alternate pref and alternate startup opt (-aplayer pulseaudio)

  char aplayer[512]; // name, eg. "jack","pulse","sox"

  /// frame quantisation type
  short q_type;
#define Q_FILL 1
#define Q_SMOOTH 1

  char workdir[PATH_MAX];  ///< kept in locale encoding
  char configdir[PATH_MAX];  ///< kept in locale encoding

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

#define WARN_MASK_LAYOUT_GAMMA (1<<27)

#define WARN_MASK_VJMODE_ENTER (1<<28)

  char cmd_log[PATH_MAX];
  char effect_command[PATH_MAX * 2];
  char video_open_command[PATH_MAX * 2];
  char audio_play_command[PATH_MAX * 2];
  char cdplay_device[PATH_MAX];  ///< locale encoding
  double default_fps;
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
  //  boolean show_framecount; - use hide_framebar
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

  short startup_phase; ///< -1 = fresh install, 1 = workdir set, 2, pre-audio start, 3, pre-tests, 100 = all tests passed
  char *wm; ///<window manager name
  int ocp; ///< open_compression_percent : get/set in prefs

  boolean antialias;

  double fps_tolerance;

  short rte_keys_virtual;

  boolean show_msg_area;

  double loadchecktime;

  // values for trickplay - TODO: add to prefs dialog
  double blendchange_amount;
  int scratchfwd_amount, scratchback_amount;
  double fpschange_amount;

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

  char jack_tserver[PATH_MAX];
  char jack_aserver[PATH_MAX];

  char *fxdefsfile;
  char *fxsizesfile;
  char *vppdefaultsfile;

  LiVESList *acodec_list;
  int acodec_list_to_format[AUDIO_CODEC_NONE];

  volatile uint32_t audio_opts;
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

  int mt_auto_back; ///< time diff to backup (-1 == never, 0 == after every change, > 0 == seconds)

  boolean ar_clipset;
  boolean ar_layout;

  char ar_clipset_name[128]; ///< locale
  char ar_layout_name[PATH_MAX];  ///< locale

  boolean rec_desktop_audio;

  boolean show_gui;
  boolean show_splash;
  boolean show_playwin;

  boolean osc_start;

  boolean concat_images;

  boolean render_audio;
  boolean normalise_audio;

  boolean instant_open;
  boolean auto_deint;
  boolean auto_nobord;

  int gui_monitor;
  int play_monitor;

  boolean force_single_monitor;

  boolean show_urgency_msgs;

  int midi_check_rate;
  int midi_rpt;

#define OMC_DEV_MIDI (1<<0)
#define OMC_DEV_JS (1<<1)
#define OMC_DEV_FORCE_RAW_MIDI (1<<2)
#define OMC_DEV_MIDI_DUMMY (1<<3)
  uint32_t omc_dev_opts;

  char omc_js_fname[PATH_MAX];  ///< utf8
  char omc_midi_fname[PATH_MAX]; ///< utf8

  boolean mouse_scroll_clips;

  int num_rtaudiobufs;

  boolean safe_symlinks;

#ifdef ALSA_MIDI
  boolean use_alsa_midi;
  boolean alsa_midi_dummy;
#endif

  int midi_rcv_channel;

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

  // these are default values; actual values can be adjusted in Preferences
#define DEF_DS_WARN_LEVEL 2500000000  // 2.5 GB
  uint64_t ds_warn_level; ///< diskspace warn level bytes
#define DEF_DS_CRIT_LEVEL 250000000 // 250MB
  uint64_t ds_crit_level; ///< diskspace critical level bytes

#define DEF_MSG_TEXTSIZE 4 // LIVES_FONTSIZE_LARGE (via lives_textsize_to_string())
#define DEF_MAX_MSGS 10000

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

  char backend_sync[PATH_MAX * 4];
  char backend[PATH_MAX * 4];

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

  boolean mt_load_fuzzy;

  boolean hide_framebar;

  boolean hfbwnp;

  boolean show_asrc;

  float ahold_threshold;

  double screen_gamma;

  int max_messages;

  int msg_textsize;

  double screen_scale;

  boolean load_rfx_builtin;

  boolean apply_gamma;

  boolean show_tooltips;

  double volume; ///< audio volume level (for jack and pulse)

  boolean vj_mode; // optimise for VJing (in progress, experimental)

  boolean allow_easing;

  boolean show_dev_opts;

  boolean btgamma;
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

#define PREFWIN_WIDTH (GUI_SCREEN_WIDTH * .9)
#define PREFWIN_HEIGHT (GUI_SCREEN_HEIGHT * .9)

#define DS_WARN_CRIT_MAX 1000000. ///< MB.

/// prefs window
typedef struct {
  ulong encoder_ofmt_fn;
  ulong encoder_name_fn;
  ulong close_func;
  LiVESAccelGroup *accel_group;

  LiVESWidget *prefs_dialog;

  LiVESWidget *prefs_list;
  LiVESWidget *prefs_table;
  LiVESWidget *tlabel;
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
  LiVESWidget *revertbutton;
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
  LiVESWidget *workdir_entry;
  LiVESWidget *cdplay_entry;
  LiVESWidget *spinbutton_def_fps;
  LiVESWidget *pbq_combo;
  LiVESWidget *ofmt_combo;
  LiVESWidget *audp_combo;
  LiVESWidget *pa_gens;
  LiVESWidget *rframes;
  LiVESWidget *rfps;
  LiVESWidget *rclips;
  LiVESWidget *reffects;
  LiVESWidget *raudio;
  LiVESWidget *rextaudio;
  LiVESWidget *rintaudio;
  LiVESWidget *rdesk_audio;
  LiVESWidget *encoder_combo;
  LiVESWidget *checkbutton_load_rfx;
  LiVESWidget *checkbutton_apply_gamma;
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
  LiVESWidget *checkbutton_warn_layout_gamma;
  LiVESWidget *checkbutton_warn_vjmode_enter;
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
  LiVESWidget *fb_filebutton;
  LiVESWidget *se_filebutton;
  LiVESWidget *theme_style2;
  LiVESWidget *theme_style3;
  LiVESWidget *theme_style4;

  LiVESWidget *cbutton_fsur;
  LiVESWidget *cbutton_evbox;
  LiVESWidget *cbutton_mtmark;
  LiVESWidget *cbutton_tlreg;
  LiVESWidget *cbutton_tcfg;
  LiVESWidget *cbutton_tcbg;
  LiVESWidget *cbutton_vidcol;
  LiVESWidget *cbutton_audcol;
  LiVESWidget *cbutton_fxcol;
  LiVESWidget *cbutton_cesel;
  LiVESWidget *cbutton_ceunsel;

  LiVESWidget *check_midi;
  LiVESWidget *ins_speed;
  LiVESWidget *jpeg;
  LiVESWidget *mt_enter_prompt;
  LiVESWidget *spinbutton_ocp;
  LiVESWidget *nmessages_spin;
  LiVESWidget *msgs_unlimited;
  LiVESWidget *msg_textsize_combo;
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
  LiVESWidget *spinbutton_ext_aud_thresh;
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
  LiVESWidget *checkbutton_show_asrc;
  LiVESWidget *checkbutton_show_ttips;
  LiVESWidget *checkbutton_hfbwnp;
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
  LiVESWidget *midichan_combo;
  LiVESWidget *alsa_midi;
  LiVESWidget *alsa_midi_dummy;
  LiVESWidget *button_midid;
  LiVESWidget *rb_startup_ce;
  LiVESWidget *rb_startup_mt;
  LiVESWidget *jack_int_label;
  LiVESWidget *checkbutton_ce_maxspect;
  LiVESWidget *checkbutton_button_icons;
  LiVESWidget *workdir_label;
  LiVESWidget *checkbutton_stream_audio;
  LiVESWidget *checkbutton_rec_after_pb;
  LiVESWidget *wpp_entry;
  LiVESWidget *frei0r_entry;
  LiVESWidget *ladspa_entry;
  LiVESWidget *cdda_hbox;
  LiVESWidget *midi_hbox;
  LiVESWidget *frameblank_entry;
  LiVESWidget *sepimg_entry;
  LiVESWidget *dialog_hpaned;
  LiVESTreeSelection *selection;

  boolean ignore_apply; ///< dont light the apply button when thing changes (for external calls), normally FALSE
  boolean needs_restart;
} _prefsw;

/// startup overrides from commandline
typedef struct {
  boolean ign_clipset;
  boolean ign_layout;
  boolean ign_osc;
  boolean ign_jackopts;
  boolean ign_aplayer;
  boolean ign_asource;
  boolean ign_stmode;
  boolean ign_vppdefs;
} _ign_opts;

typedef struct {
  // if a pref also has an entry in future_prefs, be wary of changing its value
  // seek to understand why it has a variant value

  char workdir[PATH_MAX];
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

  int audio_src;

  int nfx_threads;

  LiVESList *disabled_decoders;
  LiVESList *disabled_decoders_new;

  short sepwin_type;
  volatile float volume; ///< audio volume level (for jack and pulse)
  boolean vj_mode;
} _future_prefs;

_prefs *prefs;
_future_prefs *future_prefs;
_prefsw *prefsw;

void set_acodec_list_from_allowed(_prefsw *, render_details *);
void  rdet_acodec_changed(LiVESCombo *acodec_combo, livespointer user_data);

void set_vpp(boolean set_in_prefs);

_prefsw *create_prefs_dialog(LiVESWidget *saved_dialog);
boolean on_prefs_delete_event(LiVESWidget *, LiVESXEvent *, livespointer prefsw);
void on_preferences_activate(LiVESMenuItem *, livespointer);
void on_prefs_close_clicked(LiVESButton *, livespointer);
void on_prefs_revert_clicked(LiVESButton *, livespointer);
void on_prefs_apply_clicked(LiVESButton *, livespointer user_data);
void on_prefDomainChanged(LiVESTreeSelection *, livespointer);
void populate_combo_box(LiVESCombo *, LiVESList *data);
void set_combo_box_active_string(LiVESCombo *, char *active_str);

void prefsw_set_astream_settings(_vid_playback_plugin *, _prefsw *);
void prefsw_set_rec_after_settings(_vid_playback_plugin *, _prefsw *);

void pref_change_images(void);
void pref_change_xcolours(void);
void pref_change_colours(void);

void apply_button_set_enabled(LiVESWidget *widget, livespointer func_data);

// TODO:
/*typedef struct {
  const char *pref_name;
  int type;
} lives_preference;

const lives_preference [] = {
  {PREF_REC_EXT_AUDIO, WEED_SEED_BOOL},
};

then:

widget = lives_standard_widget_for_pref(const char *prefname, const char *label, val, min, max, step, page, dp, box, rb_group_or_combo_list, tooltip);
*/

// NOTE: the following definitions must match with equivalent keys in smogrify

#define PREF_REC_EXT_AUDIO "rec_ext_audio"
#define PREF_AUDIO_OPTS "audio_opts"
#define PREF_SEPWIN_TYPE "sepwin_type"
#define PREF_MT_EXIT_RENDER "mt_exit_render"

// factories non-cpp
#define PREF_SHOW_ASRC "show_audio_src"
#define PREF_HFBWNP "hide_framebar_when_not_playing"

// normal prefs

/////////////////// string values

#define PREF_WORKING_DIR "workdir"
#define PREF_WORKING_DIR_OLD "tempdir"
#define PREF_PREFIX_DIR "prefix_dir" // readonly
#define PREF_LIB_DIR "lib_dir" // readonly

#define PREF_AUDIO_PLAYER "audio_player"
#define PREF_AUDIO_PLAY_COMMAND "audio_play_command"

#define PREF_AUDIO_SRC "audio_src"

#define PREF_MONITORS "monitors"

#define PREF_LADSPA_PATH "ladspa_path"
#define PREF_WEED_PLUGIN_PATH "weed_plugin_path"
#define PREF_FREI0R_PATH "frei0r_path"

#define PREF_VID_PLAYBACK_PLUGIN "vid_playback_plugin"

#define PREF_DEFAULT_IMAGE_FORMAT "default_image_format"

#define PREF_VIDEO_OPEN_COMMAND "video_open_command"

#define PREF_GUI_THEME "gui_theme"

#define PREF_ENCODER "encoder"
#define PREF_OUTPUT_TYPE "output_type"

#define PREF_CDPLAY_DEVICE "cdplay_device"

#define PREF_AR_LAYOUT "ar_layout"
#define PREF_AR_CLIPSET "ar_clipset"

#define PREF_ACTIVE_AUTOTRANS "active_autotrans"

#define PREF_SCREEN_SCALE "screen_scale"

////////////////////// utf8 values

#define PREF_OMC_MIDI_FNAME "omc_midi_fname"
#define PREF_OMC_JS_FNAME "omc_js_fname"

#define PREF_IMAGE_DIR "image_dir"
#define PREF_AUDIO_DIR "audio_dir"

#define PREF_PROJ_DIR "proj_dir"

#define PREF_VID_SAVE_DIR "vid_save_dir"
#define PREF_VID_LOAD_DIR "vid_load_dir"

#define _PREF_RECENT(n) "recent" n
#define PREF_RECENT _PREF_RECENT()
#define PREF_RECENT1 _PREF_RECENT("1")
#define PREF_RECENT2 _PREF_RECENT("2")
#define PREF_RECENT3 _PREF_RECENT("3")
#define PREF_RECENT4 _PREF_RECENT("4")

/////////////////// integer64 values
#define PREF_DS_WARN_LEVEL "ds_warn_level"
#define PREF_DS_CRIT_LEVEL "ds_crit_level"

/////////////////// integer32 values
#define PREF_STARTUP_PHASE "startup_phase"

#define PREF_STARTUP_INTERFACE "startup_interface"

#define PREF_LIVES_WARNING_MASK "lives_warning_mask"
#define PREF_OPEN_COMPRESSION_PERCENT "open_compression_percent"

#define PREF_PB_QUALITY "pb_quality"

#define PREF_REC_STOP_GB "rec_stop-gb"

#define PREF_NFX_THREADS "nfx_threads"

#define PREF_CLEAR_DISK_OPTS "clear_disk_opts"

#define PREF_MAX_DISP_VTRACKS "max_disp_vtracks"

#define PREF_MAX_MSGS "max_text_messages"
#define PREF_MSG_TEXTSIZE "msg_textsize"

#define PREF_RTE_KEYS_VIRTUAL "rte_keys_virtual"

#define PREF_JACK_OPTS "jack_opts"

#define PREF_MIDI_CHECK_RATE "midi_check_rate"
#define PREF_MIDI_RPT "midi_rpt"

#define PREF_MIDI_RCV_CHANNEL "midi_rcv_channel"

#define PREF_ENCODER_ACODEC "encoder_acodec"

#define PREF_AUDIO_OPTS "audio_opts"

#define PREF_RECORD_OPTS "record_opts"

#define PREF_OMC_DEV_OPTS "omc_dev_opts"
#define PREF_OSC_PORT "osc_port"

#define PREF_MT_DEF_WIDTH "mt_def_width"
#define PREF_MT_DEF_HEIGHT "mt_def_height"
#define PREF_MT_DEF_ARATE "mt_def_arate"
#define PREF_MT_DEF_ACHANS "mt_def_achans"
#define PREF_MT_DEF_ASAMPS "mt_def_asamps"
#define PREF_MT_DEF_SIGNED_ENDIAN "mt_def_signed_endian"

#define PREF_MT_AUTO_BACK "mt_auto_back"
#define PREF_MT_UNDO_BUF "mt_undo_buf"

#define PREF_MT_BACKAUDIO "mt_backaudio"

#define PREF_WARN_FILE_SIZE "warn_file_size"

#define PREF_DL_BANDWIDTH_K "dl_bandwidth_K"

#define PREF_SCFWD_AMOUNT "trickplay_scratch_fwd"
#define PREF_SCBACK_AMOUNT "trickplay_scratch_back"
#define PREF_LOADCHECK_TIME "machine_load_check_secs"

////////// boolean values
#define PREF_SHOW_RECENT_FILES "show_recent_files"
#define PREF_FORCE_SINGLE_MONITOR "force_single_monitor"
#define PREF_STOP_SCREENSAVER "stop_screensaver"
#define PREF_MT_ENTER_PROMPT "mt_enter_prompt"
#define PREF_MT_EXIT_RENDER "mt_exit_render"
#define PREF_RENDER_PROMPT "render_prompt"
#define PREF_MT_PERTRACK_AUDIO "mt_pertrack_audio"
#define PREF_OSC_START "osc_start"
#define PREF_SHOW_TOOLBAR "show_toolbar"
#define PREF_CE_MAXSPECT "ce_maxspect"
#define PREF_OPEN_MAXIMISED "open_maximised"
#define PREF_AUTO_TRIM_PAD_AUDIO "auto_trim_pad_audio"
#define PREF_MOUSE_SCROLL_CLIPS "mouse_scroll_clips"
#define PREF_SHOW_BUTTON_ICONS "show_button_icons"
#define PREF_STREAM_AUDIO_OUT "stream_audio_out"
#define PREF_CE_THUMB_MODE "ce_thumb_mode"
#define PREF_LOAD_RFX_BUILTIN "load_rfx_builtin"
#define PREF_ANTIALIAS "antialias"
#define PREF_FILESEL_MAXIMISED "filesel_maximised"
#define PREF_SHOW_PLAYER_STATS "show_player_stats"
#define PREF_INSTANT_OPEN "instant_open"
#define PREF_MIDISYNCH "midisynch"
#define PREF_AUTO_DEINTERLACE "auto_deinterlace"
#define PREF_AUTO_CUT_BORDERS "auto_cut_borders"
#define PREF_REC_DESKTOP_AUDIO "rec_desktop_audio"
#define PREF_INSERT_RESAMPLE "insert_resample"
#define PREF_CONCAT_IMAGES "concat_images"
#define PREF_SAVE_DIRECTORIES "save_directories"
#define PREF_CONSERVE_SPACE "conserve_space"
#define PREF_PUSH_AUDIO_TO_GENS "push_audio_to_gens"
#define PREF_APPLY_GAMMA "apply_gamma"
#define PREF_SHOW_TOOLTIPS "show_tooltips"
#define PREF_SHOW_URGENCY "show_urgency_messages"
#define PREF_UNSTABLE_FX "allow_unstable_effects"
#define PREF_ALLOW_EASING "allow_easing"
#define PREF_SHOW_DEVOPTS "show_developer_options"
#define PREF_VJMODE "vj_mode_startup"

////////// double values
#define PREF_MT_DEF_FPS "mt_def_fps"
#define PREF_DEFAULT_FPS "default_fps"

#define PREF_BLEND_AMOUNT "trickplay_blend_change"
#define PREF_FPSCHANGE_AMOUNT "trickplay_fpschange"

///////// float values
#define PREF_AHOLD_THRESHOLD "ahold_threshold"
#define PREF_MASTER_VOLUME "master_volume"

////////// list values
#define PREF_DISABLED_DECODERS "disabled_decoders"

// defaults (readonly)
/////////////// string values
#define PREF_SOX_COMMAND "sox_command"

boolean pref_factory_bool(const char *prefidx, boolean newval, boolean permanent);
boolean pref_factory_string(const char *prefidx, const char *newval, boolean permanent);
boolean pref_factory_int(const char *prefidx, int newval, boolean permanent);
boolean pref_factory_float(const char *prefidx, float newval, boolean permanent);
boolean pref_factory_bitmapped(const char *prefidx, int bitfield, boolean newval, boolean permanent);
boolean pref_factory_string_choice(const char *prefidx, LiVESList *list, const char *strval, boolean permanent);

boolean has_pref(const char *key);

LiVESResponseType get_pref_from_file(const char *filename, const char *key, char *val, int maxlen);
void get_pref_default(const char *key, char *val, int maxlen); // valid only for select prefs (PREF_SOX_COMMAND)

int get_utf8_pref(const char *key, char *val, int maxlen);
LiVESResponseType get_string_pref(const char *key, char *val, int maxlen);
LiVESResponseType get_string_prefd(const char *key, char *val, int maxlen, const char *def);
boolean get_boolean_pref(const char *key);
double get_double_pref(const char *key);
double get_double_prefd(const char *key, double defval);
int get_int_pref(const char *key);
LiVESList *get_list_pref(const char *key);
boolean get_colour_pref(const char *key, lives_colRGBA64_t *lcol);
boolean get_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol);

boolean get_boolean_prefd(const char *key, boolean defval);
int get_int_prefd(const char *key, int defval);

int delete_pref(const char *key);

int set_string_pref(const char *key, const char *value);
int set_string_pref_priority(const char *key, const char *value);
int set_utf8_pref(const char *key, const char *value);
int set_boolean_pref(const char *key, boolean value);
int set_double_pref(const char *key, double value);
int set_int_pref(const char *key, int value);
int set_int64_pref(const char *key, int64_t value);
int set_list_pref(const char *key, LiVESList *values);
int set_colour_pref(const char *key, lives_colRGBA64_t *lcol);
void set_theme_pref(const char *themefile, const char *key, const char *value);
void set_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol);

boolean apply_prefs(boolean skip_warnings);
void save_future_prefs(void);

void set_palette_prefs(void);

void toggle_sets_pref(LiVESWidget *widget, livespointer prefidx);

// permissions

#define LIVES_PERM_OSC_PORTS 1

boolean lives_ask_permission(int what);

#endif
