// preferences.h
// LiVES (lives-exe)
// (c) G. Finch (salsaman_lives@gmail.com) 2004 - 2019
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PREFS_H
#define HAS_LIVES_PREFS_H

#define PREFS_PANED_POS ((int)(300.*widget_opts.scale))

#ifdef HAVE_FREI0R
#define FREI0R1_LITERAL "frei0r-1"
#endif
#if HAVE_LADSPA
#define LADSPA_LITERAL "ladspa"
#endif

#define PREF_FLAG_EXPERIMENTAL		(1 << 0)

#define DEFINE_PREF_BOOL(IDX, PR, PDEF, FLAGS) {boolean a = (PDEF); define_pref(PREF_##IDX, &prefs->PR, WEED_SEED_BOOLEAN, &a, FLAGS);}
#define DEFINE_PREF_INT(IDX, PR, PDEF, FLAGS) {int a = (PDEF); define_pref(PREF_##IDX, &prefs->PR, WEED_SEED_INT, &a, FLAGS);}
#define DEFINE_PREF_STRING(IDX, PR, SLEN, PDEF, FLAGS) {char *a = (PDEF); weed_plant_t *prefplant = \
									    define_pref(PREF_##IDX, prefs->PR, WEED_SEED_STRING, \
											&a, FLAGS); \
    weed_set_int_value(prefplant, WEED_LEAF_MAXCHARS, (SLEN));}

#define SET_PREF_WIDGET(IDX, WIDGET) set_pref_widget(PREF_##IDX, (WIDGET))
#define GET_PREF_WIDGET(IDX, WIDGET) get_pref_widget(PREF_##IDX)

#define ACTIVE(widget, signal) lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->widget), LIVES_WIDGET_ ##signal## \
							 _SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL)
#define ACTIVE_W(signal) lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_ ##signal## \
							 _SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL)

#define LIVES_LEAF_PREF_IDX "pref_idx"
#define LIVES_LEAF_WIDGET "widget"
#define LIVES_LEAF_STATUS "status"
#define LIVES_LEAF_VARPTR "p_variable"

#define PREFSTATUS_UNKNOWN	0	///< unknown PREF_IDX (use old API)
#define PREFSTATUS_UNSET	1	///< esists, but value has not been set
#define PREFSTATUS_PERM		2	///< value consistent with config file
#define PREFSTATUS_TEMP	       	3	///< value and config inconsistent; load_pref() may change the value

// for the extended auto-colours
// variance is actually 1. / VAR
#define DEF_CPICK_VAR 0.4
#define MAX_CPICK_VAR 1.0
#define DEF_CPICK_TIME 2.0

// for mainw->prefs_changed
#define PREFS_THEME_CHANGED		(1 << 0)
#define PREFS_JACK_CHANGED		(1 << 1)
#define PREFS_WORKDIR_CHANGED		(1 << 2)
#define PREFS_COLOURS_CHANGED		(1 << 3)
#define PREFS_XCOLOURS_CHANGED		(1 << 4)
#define PREFS_IMAGES_CHANGED		(1 << 5)
#define PREFS_MONITOR_CHANGED		(1 << 6)
#define PREFS_RTE_KEYMODES_CHANGED	(1 << 7)
#define PREFS_THEME_MINOR_CHANGE       	(1 << 8)

#define PREFS_NEEDS_REVERT		(1 << 28)


typedef struct {
  char bgcolour[256];
  boolean stop_screensaver;
  boolean open_maximised;
  char theme[64];  ///< the theme name

  short pb_quality;
#define PB_QUALITY_LOW 1
#define PB_QUALITY_MED 2  ///< default
#define PB_QUALITY_HIGH 3

#define PB_QUALITY_BEST PB_QUALITY_HIGH

  boolean pbq_adaptive;

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

  char configfile[PATH_MAX];  ///< kept in locale encoding (config settings) [default ~/.local/config/lives/settings)
  char config_datadir[PATH_MAX];  ///< kept in locale encoding (general config files) (default ~/.local/share/lives)

  // utf8 encoding
  char def_vid_load_dir[PATH_MAX];
  char def_vid_save_dir[PATH_MAX];
  char def_audio_dir[PATH_MAX];
  char def_image_dir[PATH_MAX];
  char def_proj_dir[PATH_MAX];

  // locale encoding
  char prefix_dir[PATH_MAX];
  char lib_dir[PATH_MAX];

  char image_type[16];
  char image_ext[16];

  uint64_t warning_mask;

  /// bits 10, 11, 13, 18 and 19 set (off by default)
  // (should have been done by reversing the sense of these bits (so the default would be 0), but it is too late now
#define DEF_WARNING_MASK 0x000C2C04ul

  // if these bits are set, we do not show the warning
  // i.e default value of zero is ON
#define WARN_MASK_FPS 	       					(1ull << 0)
#define WARN_MASK_FSIZE 	       				(1ull << 1)
#define WARN_MASK_UNUSED 	       				(1ull << 2)  ///< was "save_quality"
#define WARN_MASK_SAVE_SET 		       			(1ull << 3)
#define WARN_MASK_NO_MPLAYER 			       		(1ull << 4)
#define WARN_MASK_CHECK_PREFIX	 	       			(1ull << 5)	// formerly "NO_RENDERED_FX"
#define WARN_MASK_CHECK_PLUGINS 	       			(1ull << 6)	// formerly "NO_ENCODERS"
#define WARN_MASK_LAYOUT_MISSING_CLIPS 				(1ull << 7)

#define WARN_MASK_LAYOUT_CLOSE_FILE 				(1ull << 8)
#define WARN_MASK_LAYOUT_DELETE_FRAMES 				(1ull << 9)

  /** off by default on a fresh install */
#define WARN_MASK_LAYOUT_SHIFT_FRAMES 				(1ull << 10) // off by default

  /** off by default on a fresh install */
#define WARN_MASK_LAYOUT_ALTER_FRAMES 				(1ull << 11) // off by default

#define WARN_MASK_DUPLICATE_SET        				(1ull << 12)

  /** off by default on a fresh install */
#define WARN_MASK_EXIT_MT 		       			(1ull << 13) // off by default
#define WARN_MASK_DISCARD_SET 					(1ull << 14)
#define WARN_MASK_AFTER_DVGRAB 					(1ull << 15)
#define WARN_MASK_MT_ACHANS 			       		(1ull << 16)
#define WARN_MASK_LAYOUT_DELETE_AUDIO 				(1ull << 17)

  /** off by default on a fresh install */
#define WARN_MASK_LAYOUT_SHIFT_AUDIO 				(1ull << 18) // off by default

  /** off by default on a fresh install */
#define WARN_MASK_LAYOUT_ALTER_AUDIO 				(1ull << 19) // off by default

#define WARN_MASK_MT_NO_JACK 			       		(1ull << 20)
#define WARN_MASK_OPEN_YUV4M 					(1ull << 21)
#define WARN_MASK_MT_BACKUP_SPACE 				(1ull << 22)
#define WARN_MASK_LAYOUT_POPUP 					(1ull << 23)
#define WARN_MASK_CLEAN_AFTER_CRASH 				(1ull << 24)
#define WARN_MASK_NO_PULSE_CONNECT 				(1ull << 25)
#define WARN_MASK_LAYOUT_WIPE 					(1ull << 26)
#define WARN_MASK_LAYOUT_GAMMA 					(1ull << 27)
#define WARN_MASK_VJMODE_ENTER 					(1ull << 28)
#define WARN_MASK_CLEAN_INVALID 		       		(1ull << 29)
#define WARN_MASK_LAYOUT_LB	 		       		(1ull << 30)
#define WARN_MASK_JACK_SCRPT		       			(1ull << 31)
#define WARN_MASK_DMGD_AUDIO					(1ull << 32)

  // reserved (on / unset by default) -> i.e sense is reversed so 0 is OFF (by default)
#define WARN_MASK_RSVD_14					(1ull << 33)
#define WARN_MASK_RSVD_13					(1ull << 34)
#define WARN_MASK_RSVD_12					(1ull << 35)
#define WARN_MASK_RSVD_11					(1ull << 36)
#define WARN_MASK_RSVD_10					(1ull << 37)
#define WARN_MASK_RSVD_9					(1ull << 38)
#define WARN_MASK_RSVD_8					(1ull << 39)
#define WARN_MASK_RSVD_7					(1ull << 40)
#define WARN_MASK_RSVD_6					(1ull << 41)
#define WARN_MASK_RSVD_5					(1ull << 42)
#define WARN_MASK_RSVD_4					(1ull << 43)
#define WARN_MASK_RSVD_3					(1ull << 44)
#define WARN_MASK_RSVD_2					(1ull << 45)
#define WARN_MASK_RSVD_1					(1ull << 46)
#define WARN_MASK_RSVD_0					(1ull << 47)

  // for bits 48 - 63, the sense will be reversed, in case we need anything else ON
  // by default (this only affects the checkbutton state in the warning dialog)
#define WARN_MASK_DEF_ON					(1ull << 48)
#define WARN_MASK_LAYOUT_RELOAD					(1ull << 48)

#define WARN_MASK_RSVD_OFF_14					(1ull << 49)
#define WARN_MASK_RSVD_OFF_13					(1ull << 50)
#define WARN_MASK_RSVD_OFF_12					(1ull << 51)
#define WARN_MASK_RSVD_OFF_11					(1ull << 52)
#define WARN_MASK_RSVD_OFF_10					(1ull << 53)
#define WARN_MASK_RSVD_OFF_9					(1ull << 54)
#define WARN_MASK_RSVD_OFF_8					(1ull << 55)
#define WARN_MASK_RSVD_OFF_7					(1ull << 56)
#define WARN_MASK_RSVD_OFF_6					(1ull << 57)
#define WARN_MASK_RSVD_OFF_5					(1ull << 58)
#define WARN_MASK_RSVD_OFF_4					(1ull << 59)
#define WARN_MASK_RSVD_OFF_3					(1ull << 60)
#define WARN_MASK_RSVD_OFF_2					(1ull << 61)
#define WARN_MASK_RSVD_OFF_1					(1ull << 62)
#define WARN_MASK_RSVD_OFF_0					(1ull << 63)

  char *cmdline_args; // defaults, prepended

  char cmd_log[PATH_MAX];
  char video_open_command[PATH_MAX * 2];
  char cdplay_device[PATH_MAX];  ///< locale encoding
  double default_fps;
  boolean open_decorated;
  boolean pb_hide_gui;
  int sleep_time;
  boolean pause_during_pb;
  boolean fileselmax;
  boolean show_recent;
  int warn_file_size;
  boolean midisynch;
  int dl_bandwidth;
  boolean conserve_space; // deprecated (always FALSE)
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
  int rec_opts;
#define REC_FRAMES		(1 << 0)
#define REC_FPS			(1 << 1)
#define REC_EFFECTS		(1 << 2)
#define REC_CLIPS		(1 << 3)
#define REC_AUDIO		(1 << 4)
#define REC_AFTER_PB		(1 << 5)
#define REC_AUDIO_AUTOLOCK	(1 << 6)

  int audio_src;
#define AUDIO_SRC_INT 0
#define AUDIO_SRC_EXT 1

  boolean no_bandwidth;
  boolean osc_udp_started;
  uint32_t osc_udp_port;

  boolean omc_noisy; ///< send success/fail
  boolean omc_events; ///< send other events

  lives_intention workdir_tx_intent;

  /// 0 = normal
  // -1 or 1: fresh install, = prompt for workir,
  // 2: workdir set, = run startup tests , if skipped, continue but do not advance the phase yet
  // 3: startup tests passed,
  // after 2 or 3, build init config
  // select audio player
  // 4: aud pl chosen, if we start in this phase we assume the audio player crashed
  // 5: audio player started, pre interface seln. = select start i/f etc.
  // , 100 setup complete
  short startup_phase;
  int ocp; ///< open_compression_percent : get/set in prefs

  boolean antialias;

  double fps_tolerance;

  int rte_keys_virtual;
  int rte_modes_per_key; ///< maximum effect modes per key

  boolean show_msg_area;

  // values for trickplay - TODO: add to prefs dialog
  double blendchange_amount;
  int scratchfwd_amount, scratchback_amount;
  double fpschange_amount;

  uint32_t jack_opts;

#ifdef ENABLE_JACK
  // config files to use instead of using internal settings
  char jack_tserver_cfg[PATH_MAX];
  char jack_aserver_cfg[PATH_MAX];

  // server names to connect to, if not set, use default
  char jack_aserver_cname[JACK_PARAM_STRING_MAX];
  char jack_tserver_cname[JACK_PARAM_STRING_MAX];

  // server names to start, if not set, use default
  char jack_aserver_sname[JACK_PARAM_STRING_MAX];

  // may cause problems if equal to aserver_cname and drivers differ
  char jack_tserver_sname[JACK_PARAM_STRING_MAX];

  // for info only
  char jack_def_server_name[JACK_PARAM_STRING_MAX];

  char *jack_tdriver, *jack_adriver;
  LiVESList *jack_tslaves, *jack_aslaves;

  // may be invalid after client open
  jackctl_server_t *jack_aserver, *jack_tserver;

  // only valid for startup
  lives_rfx_t *jack_asparams, *jack_tsparams;

  // always valid
  const JSList *jack_adrivers, *jack_tdrivers;

  boolean jack_srv_dup;

  char *jack_compat_client;
  char *jack_inport_client;
  char *jack_outport_client;
  char *jack_auxport_client;
#endif

  char *fxdefsfile;
  char *fxsizesfile;
  char *vppdefaultsfile;

  LiVESList *acodec_list;
  int acodec_list_to_format[AUDIO_CODEC_MAX];

  volatile uint32_t audio_opts;
#define AUDIO_OPTS_FOLLOW_CLIPS		(1 << 0)
#define AUDIO_OPTS_FOLLOW_FPS		(1 << 1)

  /// NOT resync when FPS is reset
#define AUDIO_OPTS_NO_RESYNC_FPS	(1 << 2) // on (0) by default

  /// NOT resync when video pos changes
#define AUDIO_OPTS_NO_RESYNC_VPOS	(1 << 3) // on (0) by default

  /// resync when audio directions inverts
#define AUDIO_OPTS_RESYNC_ADIR		(1 << 4) // off (0) by default

  /// resync when audio clip switches
#define AUDIO_OPTS_RESYNC_ACLIP		(1 << 5) // off (0) by default

  /// audio is locked (temporarily overrides FOLLOW_CLIPS | FOLLOW_FPS and all resyncs)
#define AUDIO_OPTS_IS_LOCKED		(1 << 16)

  /// video freeze / unfreeze still affects locked audio
#define AUDIO_OPTS_LOCKED_FREEZE	(1 << 17)

  /// locked audio should ping-pong loop
#define AUDIO_OPTS_LOCKED_PING_PONG	(1 << 18)

  /// resync audio when it becomes unlocked
#define AUDIO_OPTS_UNLOCK_RESYNC	(1 << 19)

  /// whether fps reset should resync locked audio
#define AUDIO_OPTS_LOCKED_RESET		(1 << 20)

  /// whether audio should auto unlock when playback ends
#define AUDIO_OPTS_AUTO_UNLOCK		(1 << 21)

  /// (see also REC_AUDIO_AUTOLOCK) ///
  
#define AUDIO_OPTS_EXT_FX		(1 << 25) // apply effects to external audio -> audio out
#define AUDIO_OPTS_AUX_RECORD		(1 << 26) // mix aux in when recording
#define AUDIO_OPTS_AUX_PLAY		(1 << 27) // mix aux in during pb

  boolean pogo_mode;   /// allow mixing of locked audio with current clip audio (experimental, in progress)

  boolean event_window_show_frame_events;
  boolean crash_recovery; ///< TRUE==maintain mainw->recovery file

  boolean show_rdet; ///< show render details (frame size, encoder type) before saving to file

  boolean move_effects; /// move fx with blocks in mt

#define DEF_MT_UNDO_SIZE 32 ///< MB

  int mt_undo_buf;
  boolean mt_enter_prompt;

  int mt_def_width, mt_def_height;
  double mt_def_fps;

  int mt_def_arate, mt_def_achans, mt_def_asamps, mt_def_signed_endian;

  boolean mt_exit_render;
  boolean render_prompt;
  boolean mt_show_ctx;
  boolean mt_pertrack_audio;
  int mt_backaudio;

  int mt_auto_back; ///< time diff to backup (-1 == never, 0 == after every change, > 0 == seconds)

  /// auto-reload
  boolean ar_clipset, ar_layout;
  char ar_clipset_name[128]; ///< locale
  char ar_layout_name[PATH_MAX];  ///< locale

  boolean rec_desktop_audio;

  boolean show_gui;
  boolean show_splash;
  boolean show_playwin;

  boolean osc_start;

  boolean concat_images;

  boolean render_audio;
  boolean normalise_audio;  ///< for future use

  boolean instant_open;
  boolean auto_deint;

  boolean tr_self;
  int autotrans_key;
  int autotrans_mode;
  double autotrans_amt;

  int gui_monitor;
  int play_monitor;

  boolean force_single_monitor;

  boolean show_urgency_msgs;
  boolean show_overlay_msgs;
  boolean render_overlay;

  int twater_type; // transcode watermark types
#define TWATER_TYPE_NONE		0
#define TWATER_TYPE_STATS		1
#define TWATER_TYPE_DIAGNOSTICS		2

  /// deprecated
  int midi_check_rate;
  int midi_rpt;

  uint32_t omc_dev_opts;

  char omc_js_fname[PATH_MAX];  ///< utf8
  char omc_midi_fname[PATH_MAX]; ///< utf8

  boolean mouse_scroll_clips;

  int num_rtaudiobufs;

  // if no more than this value end frames are blank, trim the blanks
  // value of 0 disables
  int max_blank_trim;

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

#define DEF_REC_STOP_GB 10.
  double rec_stop_gb;

  // autotransitioning in mt
  int atrans_fx;
  char def_autotrans[256];

  int nfx_threads;

  boolean alpha_post; ///< set to TRUE to force use of post alpha internally

  // frame size selection match methods
  lives_match_t  dload_matmet, webcam_matmet;

  boolean stream_audio_out;
  boolean unstable_fx;
  boolean letterbox; ///< playback with letterbox
  boolean letterbox_mt; ///< playback with letterbox (multitrack)
  boolean enc_letterbox; ///< encode with letterbox
  boolean no_lb_gens; // do not letterbox generators
  boolean genq_mode; // FALSE == speed, TRUE == quality

  boolean force_system_clock; /// < force system clock (rather than soundcard) for timing ( better for high framerates )

  boolean force64bit;

  boolean auto_trim_audio;
  boolean keep_all_audio;

  /** default 0; 1==use old (bad) behaviour on bigendian machines (r/w bigend ints/doubles); 2==bad reads, good writes */
  int bigendbug;

  // these are default values; actual values can be adjusted in Preferences
#define DEF_DS_WARN_LEVEL 2500000000  // 2.5 GB
  uint64_t ds_warn_level; ///< diskspace warn level bytes
#define DEF_DS_CRIT_LEVEL 250000000 // 250MB
  uint64_t ds_crit_level; ///< diskspace critical level bytes

#define DEF_DISK_QUOTA 50  /// def 50 GiB
  uint64_t disk_quota; /// (SOFT LIMIT) max space we can use for all our files (0 means unlimited (up to ds_crtical, HARD LIMIT))

#define DEF_MSG_TEXTSIZE 4 // LIVES_FONTSIZE_LARGE (via lives_textsize_to_string())
#define DEF_MAX_MSGS 10000

#define LIVES_CDISK_LEAVE_ORPHAN_SETS		(1 << 0)
#define LIVES_CDISK_LEAVE_BFILES		(1 << 1) /// leave clip .bak files
#define LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS	(1 << 2)
#define LIVES_CDISK_LEAVE_MARKER_FILES		(1 << 3)
#define LIVES_CDISK_LEAVE_MISC_FILES		(1 << 4)
#define LIVES_CDISK_REMOVE_ORPHAN_CLIPS		(1 << 5) /// ! == recover
#define LIVES_CDISK_REMOVE_STALE_RECOVERY	(1 << 6)
#define LIVES_CDISK_LEAVE_EMPTY_DIRS		(1 << 7)
#define LIVES_CDISK_LEAVE_STAGING_DIRS		(1 << 8)
#define LIVES_CDISK_CONSIDER_IGNORED		(1 << 9) // check files marked .ignore

#define LIVES_CDISK_REMOVE_LOCK_FILES		(1 << 16) ///< not yet implemented - TODO
#define LIVES_CDISK_REBUILD_ORDER_FILES		(1 << 17) ///< not yet implemented - TODO

#define LIVES_CDISK_LEAVE_LNUMBERING		(1 << 30) ///< internal flag - - preservere layout crash recovery
#define LIVES_CDISK_SEND_TO_TRASH		(1 << 31)

  uint32_t clear_disk_opts;

  lives_intention badfile_intent;

#ifdef HAVE_YUV4MPEG
  char yuvin[PATH_MAX];
#endif

  LiVESList *disabled_decoders;

  char backend_sync[PATH_MAX * 4];
  char backend[PATH_MAX * 4];

  char weed_plugin_path[PATH_MAX];
  char frei0r_path[PATH_MAX];
  char ladspa_path[PATH_MAX];
  char libvis_path[PATH_MAX];

  boolean present;

  boolean ce_thumb_mode;

  boolean show_button_images;
  boolean show_menu_images;

  boolean push_audio_to_gens;

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
  boolean use_screen_gamma;
  boolean btgamma; ///< allows clips to be *stored* with bt709 gamma - CAUTION not backwards compatible, untested

  boolean show_tooltips;

  float volume; ///< audio volume level (for jack and pulse)

  boolean vj_mode; // optimise for VJing (in progress, experimental)

  boolean allow_easing;
  boolean jokes;

  boolean show_dev_opts;
  boolean dev_show_dabg;
  boolean dev_show_timing;
  boolean dev_show_caching;

  boolean msgs_nopbdis;

  boolean noframedrop;

  boolean back_compat;

  char pa_start_opts[255];
  boolean pa_restart;

  boolean cb_is_switch;

  boolean interactive;
  boolean extra_colours;
  double cptime;
  boolean pref_trash; ///< user prefers trash to delete
  boolean autoclean; ///< remove temp files on shutdown / startup

  boolean show_desktop_panel;
  boolean show_msgs_on_startup;
  boolean show_disk_quota;
  double quota_limit;

  boolean rr_crash;
  int rr_qmode;
  boolean rr_super;
  boolean rr_pre_smooth;
  boolean rr_qsmooth;
  int rr_fstate;
  boolean rr_amicro;
  boolean rr_ramicro;
  boolean skip_lmap_afixes;
  char def_author[1024]; ///< TODO - add to prefs window
} _prefs;

enum {
  LIST_ENTRY_RESET,
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

#define DS_WARN_CRIT_MAX 1000000. ///< MB. (default 1 TB)

/// prefs window
typedef struct {
  ulong encoder_ofmt_fn;
  ulong encoder_name_fn;
  ulong close_func;
  ulong font_size_func;
  LiVESAccelGroup *accel_group;

  LiVESWidget *prefs_dialog;

  LiVESWidget *list_scroll;
  LiVESWidget *prefs_list;
  LiVESWidget *prefs_table;
  LiVESWidget *tlabel;
  LiVESWidget *vbox_right_reset;
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
  LiVESWidget *the_button;
  LiVESWidget *cmdline_entry;
  LiVESWidget *stop_screensaver_check;
  LiVESWidget *open_maximised_check;
  LiVESWidget *mouse_scroll;
  LiVESWidget *fs_max_check;
  LiVESWidget *recent_check;
  LiVESWidget *checkbutton_lb; //< letterbox
  LiVESWidget *checkbutton_lbmt;
  LiVESWidget *checkbutton_lbenc;
  LiVESWidget *checkbutton_perm_workdir;
  LiVESWidget *no_lb_gens;
  LiVESWidget *spinbutton_gamma;
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
  LiVESWidget *pbq_adaptive;
  LiVESWidget *ofmt_combo;
  LiVESWidget *audp_combo;
  LiVESWidget *pa_gens;
  LiVESWidget *rframes;
  LiVESWidget *rfps;
  LiVESWidget *rclips;
  LiVESWidget *reffects;
  LiVESWidget *raudio;
  LiVESWidget *raudio_alock;
  LiVESWidget *rextaudio;
  LiVESWidget *rintaudio;
  LiVESWidget *rdesk_audio;
  LiVESWidget *rr_crash;
  LiVESWidget *rr_super;
  LiVESWidget *rr_combo;
  LiVESWidget *rr_pre_smooth;
  LiVESWidget *rr_qsmooth;
  LiVESWidget *rr_scombo;
  LiVESWidget *rr_amicro;
  LiVESWidget *rr_ramicro;
  LiVESWidget *encoder_combo;
  LiVESWidget *checkbutton_load_rfx;
  LiVESWidget *checkbutton_apply_gamma;
  LiVESWidget *checkbutton_screengamma;
  LiVESWidget *checkbutton_antialias;
  LiVESWidget *checkbutton_threads;
  LiVESWidget *spinbutton_warn_ds;
  LiVESWidget *spinbutton_crit_ds;
  LiVESWidget *dsl_label;
  LiVESWidget *dsc_label;
  LiVESWidget *tplay_butt;
  LiVESWidget *fontbutton;
  LiVESWidget *font_pre_button;
  LiVESWidget *font_size_spin;
  LiVESWidget *checkbutton_warn_fps;
  LiVESWidget *checkbutton_warn_mplayer;
  LiVESWidget *checkbutton_warn_save_set;
  LiVESWidget *checkbutton_warn_dup_set;
  LiVESWidget *checkbutton_warn_missplugs;
  LiVESWidget *checkbutton_warn_prefix;
  LiVESWidget *checkbutton_warn_layout_clips;
  LiVESWidget *checkbutton_warn_layout_close;
  LiVESWidget *checkbutton_warn_layout_delete;
  LiVESWidget *checkbutton_warn_layout_alter;
  LiVESWidget *checkbutton_warn_layout_shift;
  LiVESWidget *checkbutton_warn_layout_adel;
  LiVESWidget *checkbutton_warn_layout_aalt;
  LiVESWidget *checkbutton_warn_layout_ashift;
  LiVESWidget *checkbutton_warn_layout_popup;
  LiVESWidget *checkbutton_warn_layout_reload;
  LiVESWidget *checkbutton_warn_discard_layout;
  LiVESWidget *checkbutton_warn_after_dvgrab;
  LiVESWidget *checkbutton_warn_no_pulse;
  LiVESWidget *checkbutton_warn_layout_wipe;
  LiVESWidget *checkbutton_warn_layout_gamma;
  LiVESWidget *checkbutton_warn_layout_lb;
  LiVESWidget *checkbutton_warn_vjmode_enter;
  LiVESWidget *checkbutton_show_stats;
  LiVESWidget *checkbutton_warn_fsize;
  LiVESWidget *checkbutton_warn_dmgd_audio;
  LiVESWidget *checkbutton_warn_mt_achans;
  LiVESWidget *checkbutton_warn_mt_no_jack;
  LiVESWidget *checkbutton_warn_yuv4m_open;
  LiVESWidget *checkbutton_warn_mt_backup_space;
  LiVESWidget *checkbutton_warn_after_crash;
  LiVESWidget *checkbutton_warn_invalid_clip;
#ifdef ENABLE_JACK
  LiVESWidget *checkbutton_warn_jack_scrpts;
#endif
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
  LiVESWidget *msgs_nopbdis;
  LiVESWidget *msgs_mbdis;
  LiVESWidget *msg_textsize_combo;
  LiVESWidget *acodec_combo;
  LiVESWidget *spinbutton_osc_udp;
  LiVESWidget *spinbutton_rte_keys;
  LiVESWidget *spinbutton_rte_modes;
  LiVESWidget *spinbutton_nfx_threads;
  LiVESWidget *spinbutton_atrans_key;
  LiVESWidget *enable_OSC;
  LiVESWidget *enable_OSC_start;
  LiVESWidget *jack_tserver_entry;
  LiVESWidget *jack_tstart;
  // jack audio
  LiVESWidget *jack_acdef;
  LiVESWidget *jack_acname;
  LiVESWidget *jack_asname;
  LiVESWidget *jack_acerror;
  LiVESWidget *jack_apstats;
  LiVESWidget *jack_apperm;
  LiVESWidget *jack_aplabel;
  LiVESWidget *jack_aplayout;
  LiVESWidget *jack_aplayout2;
  LiVESWidget *checkbutton_jack_read_autocon;
  // jack transport
  LiVESWidget *jack_trans;
  LiVESWidget *jack_srv_dup;
  LiVESWidget *jack_tcdef;
  LiVESWidget *jack_tcname;
  LiVESWidget *jack_tcerror;
  LiVESWidget *checkbutton_jack_master;
  LiVESWidget *checkbutton_jack_client;
  LiVESWidget *checkbutton_jack_tb_start;
  LiVESWidget *checkbutton_jack_mtb_start;
  LiVESWidget *checkbutton_jack_mtb_update;
  LiVESWidget *checkbutton_jack_tb_client;
  LiVESWidget *checkbutton_jack_stricts;
  //
  LiVESWidget *checkbutton_parestart;
  LiVESWidget *checkbutton_afollow;
  LiVESWidget *checkbutton_aclips;
  LiVESWidget *resync_fps;
  LiVESWidget *resync_vpos;
  LiVESWidget *resync_adir;
  LiVESWidget *resync_aclip;
  LiVESWidget *afreeze_lock;
  LiVESWidget *afreeze_ping;
  LiVESWidget *afreeze_sync;
  LiVESWidget *alock_reset;
  LiVESWidget *auto_unlock;
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
  LiVESWidget *checkbutton_concat_images;
  LiVESWidget *checkbutton_show_asrc;
  LiVESWidget *checkbutton_show_ttips;
  LiVESWidget *checkbutton_hfbwnp;
  LiVESWidget *forcesmon;
  LiVESWidget *forcesmon_hbox;
  LiVESWidget *cb_show_msgstart;
  LiVESWidget *cb_show_quota;
  LiVESWidget *cb_autoclean;
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
  LiVESWidget *checkbutton_menu_icons;
  LiVESWidget *checkbutton_extra_colours;
  LiVESWidget *workdir_label;
  LiVESWidget *checkbutton_stream_audio;
  LiVESWidget *checkbutton_rec_after_pb;
  LiVESWidget *wpp_entry;
  LiVESWidget *frei0r_entry;
  LiVESWidget *ladspa_entry;
  LiVESWidget *libvis_entry;
  LiVESWidget *cdda_hbox;
  LiVESWidget *midi_hbox;
  LiVESWidget *frameblank_entry;
  LiVESWidget *sepimg_entry;
  LiVESWidget *def_author_entry;
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
  boolean ign_rte_keymodes;
  boolean ign_libdir;
  boolean ign_jackopts;
  boolean ign_jackserver;
  boolean ign_jackcfg;
  boolean ign_aplayer;
  boolean ign_asource;
  boolean ign_stmode;
  boolean ign_vppdefs;
  boolean ign_vjmode;
  boolean ign_dscrit;
  boolean ign_configfile;
  boolean ign_config_datadir;
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
  int nfx_threads;

  char **vpp_argv;
  uint64_t disk_quota;

  _encoder encoder;

  boolean show_recent;

  boolean osc_start;
  int startup_interface;

  uint32_t jack_opts;

#ifdef ENABLE_JACK
  // actual vals
  char jack_tserver_cfg[PATH_MAX];
  char jack_aserver_cfg[PATH_MAX];

  char jack_aserver_cname[JACK_PARAM_STRING_MAX];
  char jack_tserver_cname[JACK_PARAM_STRING_MAX];
  char jack_aserver_sname[JACK_PARAM_STRING_MAX];
  char jack_tserver_sname[JACK_PARAM_STRING_MAX];
  char *jack_tdriver, *jack_adriver;
  char *jack_inport_client;
  char *jack_outport_client;
  char *jack_auxport_client;
  LiVESList *jack_tslaves, *jack_aslaves;
#endif

  int audio_src;

  uint32_t audio_opts;
  short pb_quality;
  short sepwin_type;

  LiVESList *disabled_decoders;

  volatile float volume; ///< audio volume level (for jack and pulse)

  boolean vj_mode;
  boolean ar_clipset;

  boolean show_msg_area;
  int msg_textsize;
  boolean pref_trash; ///< user prefers trash to delete (future / present swapped)
  boolean letterbox_mt;
  char *def_fontstring; ///< preview of capable->def_fontstring
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
void on_prefs_page_changed(LiVESTreeSelection *, _prefsw *);
void populate_combo_box(LiVESCombo *, LiVESList *data);
void set_combo_box_active_string(LiVESCombo *, char *active_str);

void prefsw_set_astream_settings(_vid_playback_plugin *, _prefsw *);
void prefsw_set_rec_after_settings(_vid_playback_plugin *, _prefsw *);

void pref_change_images(void);
void pref_change_xcolours(void);
void pref_change_colours(void);

void apply_button_set_enabled(LiVESWidget *widget, livespointer func_data);

/** NEW PREF API ---
  -- adding a new pref is now as simple as:
  - add a pref_idx, e.g #define PREF_FOO "foo"
  - add a variable to the prefs struct, e.g. boolean foo;
  - add a corresponding definition in preferences.c, init_prefs(), e.g.
    DEFINE_PREF_BOOL(PREF_FOO, foo, FALSE)
    - here we also set the default value, in this case, FALSE
    - a widget can be attached for use in the prefs window, e.g
    -- set_pref_widget(PREF_FOO, lives_standard_check_button_new(...))

  -- initialising:
    after a call to init_prefs(), an individual pref can be loaded from config with:
    - load_pref(PREF_FOO)
    -- this will load any existing value, and if not present in config, the default value is set internally
    - load_prefs() - this will cause any remaining unloaded prefs to be loaded as if load_pref() were called

  -- reading the value:
    e.g: boolean bval = prefs->foo;
    -- underlying updates are reproduced in the variable

  -- updating the value:
    - recommended to use update_pref()

  -- getting widget
    get_pref_widget(PREF_FOO)
*/

// temp values (not saved)
#define PREF_REC_EXT_AUDIO "rec_ext_audio" // bool
#define PREF_SEPWIN "sepwin" // bool

/////////////////// string values
#define PREF_WORKING_DIR "workdir"
#define PREF_WORKING_DIR_OLD "tempdir"
#define PREF_PREFIX_DIR "prefix_dir" // readonly
#define PREF_LIB_DIR "lib_dir" // readonly

#define PREF_CMDLINE_ARGS "cmdline_args" // special location

#define PREF_AUDIO_PLAYER "audio_player"
#define PREF_AUDIO_SRC "audio_src"

#define PREF_MONITORS "monitors"

#define PREF_LADSPA_PATH "ladspa_path"
#define PREF_WEED_PLUGIN_PATH "weed_plugin_path"
#define PREF_FREI0R_PATH "frei0r_path"
#define PREF_LIBVISUAL_PATH "libvis_path"

#define PREF_VID_PLAYBACK_PLUGIN "vid_playback_plugin"

#define PREF_DEFAULT_IMAGE_TYPE "default_image_format"

#define PREF_VIDEO_OPEN_COMMAND "video_open_command"

#define PREF_GUI_THEME "gui_theme"

#define PREF_ENCODER "encoder"
#define PREF_OUTPUT_TYPE "output_type"

#define PREF_CDPLAY_DEVICE "cdplay_device"

#define PREF_AR_LAYOUT "ar_layout"
#define PREF_AR_CLIPSET "ar_clipset"

#define PREF_ACTIVE_AUTOTRANS "active_autotrans"

#define PREF_SCREEN_SCALE "screen_scale"
#define PREF_PASTARTOPTS "pa_start_opts"

#define PREF_DEF_AUTHOR "default_author_name"

#ifdef ENABLE_JACK
#define PREF_JACK_TDRIVER "jack_transport_driver"
#define PREF_JACK_ADRIVER "jack_audio_driver"

#define PREF_JACK_LAST_TDRIVER "last_jack_tdriver"
#define PREF_JACK_LAST_TSERVER "last_jack_tserver"

#define PREF_JACK_LAST_ADRIVER "last_jack_adriver"
#define PREF_JACK_LAST_ASERVER "last_jack_aserver"

#define PREF_JACK_ACSERVER "jack_audio_connect_server"
#define PREF_JACK_ASSERVER "jack_audio_startup_server"
#define PREF_JACK_ACONFIG "jack_audio_config_file"

#define PREF_JACK_TCSERVER "jack_trans_connect_server"
#define PREF_JACK_TSSERVER "jack_trans_startup_server"
#define PREF_JACK_TCONFIG "jack_trans_config_file"

#define PREF_JACK_INPORT_CLIENT "jack_inport_client"
#define PREF_JACK_OUTPORT_CLIENT "jack_outport_client"
#define PREF_JACK_AUXPORT_CLIENT "jack_auxport_client"
#endif

////////////////////// utf8 values
#define PREF_OMC_MIDI_FNAME "omc_midi_fname"
#define PREF_OMC_JS_FNAME "omc_js_fname"

#define PREF_IMAGE_DIR "image_dir"
#define PREF_AUDIO_DIR "audio_dir"

#define PREF_PROJ_DIR "proj_dir"

#define PREF_VID_SAVE_DIR "vid_save_dir"
#define PREF_VID_LOAD_DIR "vid_load_dir"

#define PREF_RECENT "recent"

#define PREF_INTERFACE_FONT "def_GUI_font"

/////////////////// integer64 values
#define PREF_UID "unique_id"
#define PREF_DS_WARN_LEVEL "ds_warn_level"
#define PREF_DS_CRIT_LEVEL "ds_crit_level"
#define PREF_DISK_QUOTA "disk_quota"

/////////////////// integer32 values
#define PREF_SEPWIN_TYPE "sepwin_type"

#define PREF_STARTUP_PHASE "startup_phase"

#define PREF_STARTUP_INTERFACE "startup_interface"

#define PREF_LIVES_WARNING_MASK "lives_warning_mask"
#define PREF_OPEN_COMPRESSION_PERCENT "open_compression_percent"

#define PREF_PB_QUALITY "pb_quality"

#define PREF_REC_STOP_GB "rec_stop-gb"

#define PREF_NFX_THREADS "nfx_threads"

#define PREF_BTGAMMA "experimental_bt709_gamma"
#define PREF_USE_SCREEN_GAMMA "use_screen_gamma"
#define PREF_SCREEN_GAMMA "screen_gamma"

#define PREF_CLEAR_DISK_OPTS "clear_disk_opts"

#define PREF_MAX_DISP_VTRACKS "max_disp_vtracks"

#define PREF_MAX_MSGS "max_text_messages"
#define PREF_MSG_TEXTSIZE "msg_textsize"
#define PREF_MSG_NOPBDIS "msg_disable_during_playback"

#define PREF_NOFRAMEDROP "no_framedrop"

#define PREF_RTE_KEYS_VIRTUAL "rte_keys_virtual"
#define PREF_RTE_MODES_PERKEY "rte_keymodes"

#define PREF_JACK_OPTS "jack_opts"

#define PREF_MIDI_CHECK_RATE "midi_check_rate"
#define PREF_MIDI_RPT "midi_rpt"

#define PREF_MIDI_RCV_CHANNEL "midi_rcv_channel"

#define PREF_ENCODER_ACODEC "encoder_acodec"

#define PREF_RECORD_OPTS "record_opts"

#define PREF_AUDIO_OPTS "audio_opts"

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
#define PREF_MT_SHOW_CTX "mt_show_ctx"
#define PREF_WARN_FILE_SIZE "warn_file_size"

#define PREF_DL_BANDWIDTH_K "dl_bandwidth_K"

#define PREF_SCFWD_AMOUNT "trickplay_scratch_fwd"
#define PREF_SCBACK_AMOUNT "trickplay_scratch_back"

#define PREF_RRQMODE "recrender_quant_mode"
#define PREF_RRFSTATE "recrender_fx_posn_state"

#define PREF_BADFILE_INTENT "badfile_intent"

#define PREF_ATRANS_KEY "autotrans_key"

#define PREF_DLOAD_MATMET "dload_match_method"
#define PREF_WEBCAM_MATMET "webcam_match_method"

////////// boolean values
#define PREF_MT_EXIT_RENDER "mt_exit_render"
#define PREF_HFBWNP "hide_framebar_when_not_playing"
#define PREF_SHOW_ASRC "show_audio_src"
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
#define PREF_KEEP_ALL_AUDIO "never_trim_audio"
#define PREF_MOUSE_SCROLL_CLIPS "mouse_scroll_clips"
#define PREF_SHOW_BUTTON_ICONS "show_button_icons"
#define PREF_SHOW_MENU_ICONS "show_menu_icons"
#define PREF_STREAM_AUDIO_OUT "stream_audio_out"
#define PREF_CE_THUMB_MODE "ce_thumb_mode"
#define PREF_LOAD_RFX_BUILTIN "load_rfx_builtin"
#define PREF_ANTIALIAS "antialias"
#define PREF_FILESEL_MAXIMISED "filesel_maximised"
#define PREF_SHOW_PLAYER_STATS "show_player_stats"
#define PREF_INSTANT_OPEN "instant_open"
#define PREF_MIDISYNCH "midisynch"
#define PREF_AUTO_DEINTERLACE "auto_deinterlace"
#define PREF_REC_DESKTOP_AUDIO "rec_desktop_audio"
#define PREF_INSERT_RESAMPLE "insert_resample"
#define PREF_CONCAT_IMAGES "concat_images"
#define PREF_SAVE_DIRECTORIES "save_directories"
#define PREF_CONSERVE_SPACE "conserve_space"
#define PREF_PUSH_AUDIO_TO_GENS "push_audio_to_gens"
#define PREF_APPLY_GAMMA "apply_gamma"
#define PREF_SHOW_TOOLTIPS "show_tooltips"
#define PREF_SHOW_URGENCY "show_urgency_messages"
#define PREF_SHOW_OVERLAY_MSGS "show_overlay_messages"
#define PREF_UNSTABLE_FX "allow_unstable_effects"
#define PREF_ALLOW_EASING "allow_easing"
#define PREF_SHOW_DEVOPTS "show_developer_options"
#define PREF_VJMODE "vj_mode_startup"
#define PREF_LETTERBOX "letterbox_ce"
#define PREF_LETTERBOX_MT "letterbox_mt"
#define PREF_LETTERBOX_ENC "letterbox_encoding"
#define PREF_NO_LB_GENS "no_letterbox_gens"
#define PREF_GENQ_MODE "genq_mode"
#define PREF_PARESTART "pa_restart"
#define PREF_PBQ_ADAPTIVE "pb_quality_adaptive"
#define PREF_EXTRA_COLOURS "extra_colours"
#define PREF_SHOW_MSGS "show_messages"
#define PREF_SHOW_SUBS "show_subtitles" /// add to prefs window
#define PREF_AUTOLOAD_SUBS "autoload_subtitles" /// add to prefs window
#define PREF_AUTOCLEAN_TRASH "autoclean_trash" ///< remove unneeded files on shutdown / startup
#define PREF_PREF_TRASH "prefer_trash" ///< prefer trash to delete
#define PREF_MSG_START "show_msgs_on_startup" /// pop up msgs box on startup
#define PREF_SHOW_QUOTA "show_quota_on_startup" /// pop up quota box on startup

#define PREF_RRCRASH "recrender_crash_protection" /// option for rendering recordings
#define PREF_RRSUPER "recrender_super" /// option for rendering recordings
#define PREF_RRPRESMOOTH "recrender_presmooth" /// option for rendering recordings
#define PREF_RRQSMOOTH "recrender_qsmooth" /// option for rendering recordings
#define PREF_RRAMICRO "recrender_amicro" /// option for rendering recordings
#define PREF_RRRAMICRO "recrender_rend_amicro" /// option for rendering recordings

#define PREF_BACK_COMPAT "backwards_compatibility" ///< forces backwards compatibility with earlier versions

#define PREF_PB_HIDE_GUI "hide_main_window_during_playback"

#define PREF_SELF_TRANS "self_transition"

#define PREF_POGO_MODE "pogo_mode"

////////// double values
#define PREF_MT_DEF_FPS "mt_def_fps"
#define PREF_DEFAULT_FPS "default_fps"

#define PREF_BLEND_AMOUNT "trickplay_blend_change"
#define PREF_FPSCHANGE_AMOUNT "trickplay_fpschange"

#define PREF_CPICK_VAR "colour_pick_variance"
#define PREF_CPICK_TIME "colour_pick_time"

///////// float values
#define PREF_AHOLD_THRESHOLD "ahold_threshold"
#define PREF_MASTER_VOLUME "master_volume"

////////// list values
#define PREF_DISABLED_DECODERS "disabled_decoders"

void load_prefs(void);
void load_pref(const char *pref_idx);
LiVESWidget *get_pref_widget(const char *pref_idx);
boolean update_pref(const char *pref_idx, void *newval, boolean permanent);

boolean update_boolean_pref(const char *pref_idx, boolean val, boolean permanent);
boolean update_int_pref(const char *pref_idx, int val, boolean permanent);

void invalidate_pref_widgets(LiVESWidget *top);

void free_prefs(void);

boolean pref_factory_bool(const char *prefidx, boolean newval, boolean permanent);
boolean pref_factory_string(const char *prefidx, const char *newval, boolean permanent);
boolean pref_factory_utf8(const char *prefidx, const char *newval, boolean permanent);
boolean pref_factory_int(const char *prefidx, int *pref, int newval, boolean permanent);
boolean pref_factory_int64(const char *prefidx, int64_t newval, boolean permanent);
boolean pref_factory_float(const char *prefidx, float newval, boolean permanent);
boolean pref_factory_bitmapped(const char *prefidx, uint32_t bitfield, boolean newval, boolean permanent);
boolean pref_factory_string_choice(const char *prefidx, LiVESList *list, const char *strval, boolean permanent);
LiVESList *pref_factory_list(const char *prefidx, LiVESList *);

boolean has_pref(const char *key);

LiVESResponseType get_pref_from_file(const char *filename, const char *key, char *val, int maxlen);

int get_utf8_pref(const char *key, char *val, int maxlen);
LiVESResponseType get_string_pref(const char *key, char *val, int maxlen);
LiVESResponseType get_string_prefd(const char *key, char *val, int maxlen, const char *def);
boolean get_boolean_pref(const char *key);
double get_double_pref(const char *key);
double get_double_prefd(const char *key, double defval);
int get_int_pref(const char *key);
LiVESList *get_list_pref(const char *key);
boolean get_colour_pref(const char *key, lives_colRGBA64_t *lcol);
boolean get_theme_colour_pref(const char *key, lives_colRGBA64_t *lcol);

boolean get_boolean_prefd(const char *key, boolean defval);
int get_int_prefd(const char *key, int defval);
int64_t get_int64_prefd(const char *key, int64_t defval);

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

void set_palette_prefs(boolean save);

void toggle_sets_pref(LiVESWidget *widget, livespointer prefidx);

#if 0
// TODO provide customised suggestions for best performance
void optimize(void);
#endif

// toggle for widget object data (widget is in preview state)
#define PREVIEW_KEY "_prefw_preview"

// permissions

#define LIVES_PERM_INVALID 0
#define LIVES_PERM_OSC_PORTS 1
#define LIVES_PERM_DOWNLOAD_LOCAL 2
#define LIVES_PERM_COPY_LOCAL 3

boolean lives_ask_permission(char **argv, int argc, int offs);
#endif
