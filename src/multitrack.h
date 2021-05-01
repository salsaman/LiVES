// multitrack.h
// LiVES
// (c) G. Finch 2005 - 2017 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// multitrack window

#ifndef HAS_LIVES_MULTITRACK_H
#define HAS_LIVES_MULTITRACK_H

////////////////////// GUI constants ///////////////////////

#define CLIP_THUMB_WIDTH ((int)(72.*widget_opts.scale))
#define CLIP_THUMB_HEIGHT ((int)(72.*widget_opts.scale))

#define CLIP_LABEL_LENGTH ((int)(20.*widget_opts.scale))

#define BLOCK_THUMB_WIDTH ((int)(40.*widget_opts.scale))

#define MT_PLAY_WIDTH_SMALL ((int)(320.*widget_opts.scale))
#define MT_PLAY_HEIGHT_SMALL ((int)(216.*widget_opts.scale))

#define MT_PLAY_WIDTH_EXP ((int)(432.*widget_opts.scale))
#define MT_PLAY_HEIGHT_EXP ((int)(324.*widget_opts.scale))

#define MT_CTX_WIDTH ((int)(320.*widget_opts.scale))
#define MT_CTX_HEIGHT ((int)(220.*widget_opts.scale))

#define FX_BLOCK_WIDTH ((int)(80.*widget_opts.scale))
#define FX_BLOCK_HEIGHT ((int)(20.*widget_opts.scale))

#define MT_TRACK_HEIGHT ((int)(35.*widget_opts.scale))

#define TIMECODE_LENGTH 14 ///< length of timecode text entry, must be > 12

#define TIMELINE_TABLE_COLUMNS 40

#define MENUBAR_MIN 1024

#define PEB_WRATIO 3. ///< preview eventbox width ratio (fraction of screen width)
#define PEB_HRATIO 3. ///< preview eventbox height ratio (fraction of screen height)

#define LIVES_AVOL_SCALE ((double)1000000.)

#define AMIXER_WRATIO 2. / 3. ///< audio mixer width ratio (fraction of screen width)
#define AMIXER_HRATIO 2. / 3. ///< audio mixer height ratio (fraction of screen height)

////////////////////////////////////////////////////////////////////////////////

#define MT_INOUT_TIME 10000 ///< min milliseconds to save autobackup when changing in / out spins

#define BLOCK_DRAW_SIMPLE 1
#define BLOCK_DRAW_THUMB 2
#define BLOCK_DRAW_TYPE BLOCK_DRAW_THUMB

#define SELBLOCK_ALPHA 0.6

#define DEF_TIME 120 ///< default seconds when there is no event_list

#define DEF_AUTOTRANS "simple_blend|chroma blend|salsaman"

typedef struct _mt lives_mt;

typedef struct _track_rect track_rect;

typedef struct _mt_opts mt_opts;

typedef struct _lives_amixer_t lives_amixer_t;

typedef enum {
  MOUSE_MODE_MOVE,
  MOUSE_MODE_SELECT,
  MOUSE_MODE_COPY
} lives_mt_mouse_mode_t;

typedef enum {
  INSERT_MODE_NORMAL,  ///< the default (only insert if it fits)

  // not implemented yet
  INSERT_MODE_OVERWRITE, ///< overwrite existing blocks
  INSERT_MODE_FLEX, ///< stretch first gap to fit block
  INSERT_MODE_FILL, ///< insert enough to fill gap (from selection start or end depending on gravity)
} lives_mt_insert_mode_t;

typedef enum {
  GRAV_MODE_NORMAL,
  GRAV_MODE_LEFT,
  GRAV_MODE_RIGHT
} lives_mt_grav_mode_t;

typedef enum {
  // undo actions
  /// no event_list
  MT_UNDO_NONE = 0,
  MT_UNDO_INSERT_BLOCK = 1,
  MT_UNDO_INSERT_AUDIO_BLOCK = 2,

  // minimal event_list
  MT_UNDO_APPLY_FILTER = 512,
  MT_UNDO_DELETE_FILTER = 513,
  MT_UNDO_SPLIT = 514,
  MT_UNDO_SPLIT_MULTI = 515,
  MT_UNDO_FILTER_MAP_CHANGE = 516,

  // full backups of event_list
  MT_UNDO_DELETE_BLOCK = 1024,
  MT_UNDO_MOVE_BLOCK = 1025,
  MT_UNDO_REMOVE_GAPS = 1026,
  MT_UNDO_DELETE_AUDIO_BLOCK = 1027,
  MT_UNDO_MOVE_AUDIO_BLOCK = 1028,
  MT_UNDO_INSERT_GAP = 1029
} lives_mt_undo_t;

typedef enum {
  NB_ERROR_SEL,
  NB_ERROR_NOEFFECT,
  NB_ERROR_NOTRANS,
  NB_ERROR_NOCOMP,
  NB_ERROR_NOCLIP
} lives_mt_nb_error_t;

typedef enum {
  POLY_NONE = 0,
  POLY_CLIPS,
  POLY_IN_OUT,
  POLY_FX_STACK,
  POLY_PARAMS,
  POLY_EFFECTS,
  POLY_TRANS,
  POLY_COMP
} lives_mt_poly_state_t;

typedef enum {
  MT_LAST_FX_NONE = 0,
  MT_LAST_FX_BLOCK,
  MT_LAST_FX_REGION
} lives_mt_last_fx_type_t;

typedef enum {
  FX_ORD_NONE = 0,
  FX_ORD_BEFORE,
  FX_ORD_AFTER
} lives_mt_fx_order_t;

typedef enum {
  // block state
  BLOCK_UNSELECTED = 0,
  BLOCK_SELECTED
} lives_mt_block_state_t;

struct _mt_opts {
  boolean set; ///< have we set opts (in mainw) ?
  boolean move_effects; ///< should we move effects attached to a block ?
  boolean fx_auto_preview;
  boolean snap_over; ///< snap to overlap
  lives_mt_grav_mode_t grav_mode;
  lives_mt_mouse_mode_t mouse_mode;
  lives_mt_insert_mode_t insert_mode;
  boolean show_audio;
  boolean ign_ins_sel;
  boolean follow_playback;
  boolean insert_audio;  ///< do we insert audio with video ?
  boolean pertrack_audio; ///< do we want pertrack audio ?
  boolean audio_bleedthru; ///< should we allow all audio to bleed thru ?
  boolean gang_audio; ///< gang layer audio volume levels
  boolean autocross_audio; ///< crossfade audio with autotransitions
  int back_audio_tracks; ///< number of backing audio tracks (currently 0 or 1)
  boolean render_vidp; ///< render video
  boolean render_audp; ///< render audio
  boolean normalise_audp; ///< normalise audio
  boolean overlay_timecode;
  boolean show_info;
  int hpaned_pos;
  int vpaned_pos;
  double ptr_time;
  LiVESList *aparam_view_list;
};

struct _mt {
  // widgets
  LiVESWidget *menubar;
  LiVESWidget *top_vbox;
  LiVESWidget *top_vpaned;
  LiVESWidget *xtravbox;
  LiVESWidget *hbox;
  LiVESWidget *play_blank;
  LiVESWidget *poly_box;
  LiVESWidget *clip_scroll;
  LiVESWidget *clip_inner_box;
  LiVESWidget *in_out_box;
  LiVESWidget *in_hbox;
  LiVESWidget *out_hbox;
  LiVESWidget *in_frame;
  LiVESWidget *out_frame;
  LiVESWidget *in_image;
  LiVESWidget *out_image;
  LiVESWidget *scrolledwindow;
  LiVESWidget *context_box;
  LiVESWidget *context_scroll;
  LiVESWidget *sep_image;
  LiVESWidget *timeline_table_header;
  LiVESWidget *tl_eventbox;
  LiVESWidget *timeline_table;
  LiVESWidget *timeline;
  LiVESWidget *timeline_eb;
  LiVESWidget *timeline_reg;
  LiVESWidget *infobar;
  LiVESWidget *stop;
  LiVESWidget *rewind;
  LiVESWidget *sepwin;
  LiVESWidget *mute_audio;
  LiVESWidget *loop_continue;
  LiVESWidget *insert;
  LiVESWidget *audio_insert;
  LiVESWidget *delblock;
  LiVESWidget *clipedit;
  LiVESWidget *vpaned;
  LiVESWidget *hpaned;
  LiVESWidget *scrollbar;
  LiVESWidget *playall;
  LiVESWidget *playsel;
  LiVESWidget *jumpnext;
  LiVESWidget *jumpback;
  LiVESWidget *mark_jumpnext;
  LiVESWidget *mark_jumpback;
  LiVESWidget *render;
  LiVESWidget *prerender_aud;
  LiVESWidget *message_box;
  LiVESWidget *msg_area;
  LiVESWidget *msg_scrollbar;
  LiVESAdjustment *msg_adj;
  LiVESWidget *fx_block;
  LiVESWidget *fx_blockv;
  LiVESWidget *fx_blocka;
  LiVESWidget *fx_delete;
  LiVESWidget *fx_edit;
  LiVESWidget *fx_region;
  LiVESWidget *fx_region_v;
  LiVESWidget *fx_region_a;
  LiVESWidget *fx_region_2av;
  LiVESWidget *fx_region_2v;
  LiVESWidget *fx_region_2a;
  LiVESWidget *fx_region_3;
  LiVESWidget *atrans_menuitem;
  LiVESWidget *submenu_atransfx;
  LiVESWidget *move_fx;
  LiVESWidget *mm_menuitem;
  LiVESWidget *mm_move;
  LiVESWidget *mm_select;
  LiVESWidget *ins_menuitem;
  LiVESWidget *ins_normal;
  LiVESWidget *grav_menuitem;
  LiVESWidget *grav_label;
  LiVESWidget *grav_normal;
  LiVESWidget *grav_left;
  LiVESWidget *grav_right;
  LiVESWidget *select_track;
  LiVESWidget *seldesel_menuitem;
  LiVESWidget *view_events;
  LiVESWidget *view_clips;
  LiVESWidget *view_in_out;
  LiVESWidget *view_effects;
  LiVESWidget *show_quota;
  LiVESWidget *avel_box;
  LiVESWidget *checkbutton_avel_reverse;
  LiVESWidget *spinbutton_avel;
  LiVESWidget *avel_scale;
  LiVESWidget *spinbutton_in;
  LiVESWidget *spinbutton_out;
  LiVESWidget *checkbutton_start_anchored;
  LiVESWidget *checkbutton_end_anchored;
  LiVESWidget *timecode;
  LiVESWidget *spinbutton_start;
  LiVESWidget *spinbutton_end;
  LiVESWidget *tl_hbox;
  LiVESWidget *fx_base_box;
  LiVESWidget *fx_contents_box;
  LiVESWidget *fx_box;
  LiVESWidget *fx_label;
  LiVESWidget *param_inner_box;
  LiVESWidget *param_box;
  LiVESWidget *next_node_button;
  LiVESWidget *prev_node_button;
  LiVESWidget *del_node_button;
  LiVESWidget *resetp_button;
  LiVESWidget *node_spinbutton;
  LiVESWidget *node_scale;
  LiVESWidget *sel_label;
  LiVESWidget *l_sel_arrow;
  LiVESWidget *r_sel_arrow;
  LiVESWidget *save_event_list; ///< menuitem
  LiVESWidget *load_event_list; ///< menuitem
  LiVESWidget *clear_event_list; ///< menuitem
  LiVESWidget *tc_to_rs;
  LiVESWidget *tc_to_re;
  LiVESWidget *rs_to_tc;
  LiVESWidget *re_to_tc;
  LiVESWidget *undo;
  LiVESWidget *redo;
  LiVESWidget *ac_audio_check;
  LiVESWidget *remove_gaps;
  LiVESWidget *remove_first_gaps;
  LiVESWidget *split_sel;
  LiVESWidget *ins_gap_sel;
  LiVESWidget *ins_gap_cur;
  LiVESWidget *last_filter_map;
  LiVESWidget *next_filter_map;
  LiVESWidget *fx_list_box;
  LiVESWidget *fx_list_label;
  LiVESWidget *fx_list_scroll;
  LiVESWidget *fx_list_vbox;
  LiVESWidget *next_fm_button;
  LiVESWidget *prev_fm_button;
  LiVESWidget *fx_ibefore_button;
  LiVESWidget *fx_iafter_button;
  LiVESWidget *rename_track;
  LiVESWidget *cback_audio;
  LiVESWidget *delback_audio;
  LiVESWidget *aload_subs;
  LiVESWidget *load_vals;
  LiVESWidget *change_vals;
  LiVESWidget *aparam_separator;
  LiVESWidget *aparam_menuitem;
  LiVESWidget *aparam_submenu;
  LiVESWidget *render_sep;
  LiVESWidget *render_vid;
  LiVESWidget *render_aud;
  LiVESWidget *normalise_aud;
  LiVESWidget *view_audio;
  LiVESWidget *clear_marks;
  LiVESWidget *fd_frame;
  LiVESWidget *apply_fx_button;
  LiVESToolItem *eview_button;
  LiVESWidget *eview_label;
  LiVESWidget *follow_play;
  LiVESWidget *change_max_disp;
  LiVESWidget *add_vid_behind;
  LiVESWidget *add_vid_front;
  LiVESWidget *show_info;
  LiVESWidget *quit;
  LiVESWidget *troubleshoot;
  LiVESWidget *expl_missing;
  LiVESWidget *show_devopts;
  LiVESWidget *fx_params_label;
  LiVESWidget *solo_check;
  LiVESWidget *amixer_button;
  LiVESWidget *view_sel_events;
  LiVESWidget *adjust_start_end;
  LiVESWidget *context_frame;
  LiVESWidget *nb;
  LiVESWidget *nb_label;

  LiVESWidget *eventbox;
  LiVESWidget *scroll_label;
  LiVESWidget *preview_frame;
  LiVESWidget *preview_eventbox;
  LiVESWidget *btoolbarx;
  LiVESWidget *btoolbary;
  LiVESWidget *bleedthru;
  LiVESWidget *time_label;
  LiVESWidget *insa_label;
  LiVESWidget *overlap_label;
  LiVESWidget *amix_label;
  LiVESWidget *tl_label;
  LiVESWidget *dumlabel1;
  LiVESWidget *dumlabel2;
  LiVESWidget *top_eventbox;
  LiVESWidget *tlx_eventbox;
  LiVESWidget *tlx_vbox;

  LiVESWidget *grav_submenu;
  LiVESWidget *ins_submenu;
  LiVESWidget *mm_submenu;
  LiVESWidget *ins_label;
  LiVESWidget *mm_label;

  LiVESWidget *nb_label1;
  LiVESWidget *nb_label2;
  LiVESWidget *nb_label3;
  LiVESWidget *nb_label4;
  LiVESWidget *nb_label5;
  LiVESWidget *nb_label6;
  LiVESWidget *nb_label7;

  LiVESToolItem *sep1;
  LiVESToolItem *sep2;
  LiVESToolItem *sep3;
  LiVESWidget *sep4;

  LiVESWidget *btoolbar2;
  LiVESWidget *btoolbar3;

  LiVESWidget *menu_hbox;

  LiVESWidget *hseparator;
  LiVESWidget *hseparator2;

  LiVESWidget *files_menu;
  LiVESWidget *edit_menu;
  LiVESWidget *play_menu;
  LiVESWidget *effects_menu;
  LiVESWidget *tracks_menu;
  LiVESWidget *selection_menu;
  LiVESWidget *tools_menu;
  LiVESWidget *render_menu;
  LiVESWidget *view_menu;
  LiVESWidget *help_menu;

  LiVESWidget *open_menu;
#ifdef HAVE_WEBM
  LiVESWidget *open_loc_menu;
#endif
#ifdef ENABLE_DVD_GRAB
  LiVESWidget *vcd_dvd_menu;
#endif
#ifdef HAVE_LDVGRAB
  LiVESWidget *device_menu;
#endif
  LiVESWidget *recent_menu;
  LiVESWidget *recent[N_RECENT_FILES];

  LiVESWidget *time_scrollbar;
  LiVESWidget *show_layout_errors;

  LiVESWidget *load_set;
  LiVESWidget *save_set;

  LiVESWidget *close;

  LiVESWidget *clear_ds;

  LiVESWidget *gens_submenu;
  LiVESWidget *capture;

  LiVESWidget *backup;

  LiVESWidget *insa_checkbutton;
  LiVESWidget *snapo_checkbutton;

  LiVESWidgetObject *spinbutton_in_adj;
  LiVESWidgetObject *spinbutton_out_adj;

  LiVESWidgetObject *hadjustment;
  LiVESWidgetObject *node_adj;

  LiVESList *audio_draws; ///< list of audio boxes, 0 == backing audio, 1 == track 0 audio, etc.

  LiVESList *audio_vols; ///< layer volume levels (coarse control) - set in mixer
  LiVESList *audio_vols_back; ///< layer volume levels (coarse control) - reset levels

  LiVESAccelGroup *accel_group;
  LiVESList *video_draws; ///< list of video timeline eventboxes, in layer order
  LiVESWidgetObject *vadjustment;

  LiVESXDisplay *display;

  LiVESPixbuf *frame_pixbuf;

  LiVESList *cb_list;

  lives_painter_surface_t *insurface, *outsurface;

  ulong spin_in_func;
  ulong spin_out_func;
  ulong spin_avel_func;
  ulong check_start_func;
  ulong check_end_func;
  ulong check_avel_rev_func;

  ulong mm_move_func;
  ulong mm_select_func;

  ulong ins_normal_func;

  ulong grav_normal_func;
  ulong grav_left_func;
  ulong grav_right_func;

  ulong sepwin_func;
  ulong mute_audio_func;
  ulong loop_cont_func;

  ulong seltrack_func;

  ulong tc_func;

  lives_painter_surface_t *pbox_surface;

  lives_painter_surface_t *tl_ev_surf;
  lives_painter_surface_t *tl_reg_surf;
  lives_painter_surface_t *tl_surf;

  weed_plant_t *event_list;

  weed_plant_t *init_event;   ///< current editable values
  weed_plant_t *selected_init_event;  ///< currently selected in list
  ///////////////////////
  int num_video_tracks;
  double end_secs;  ///< max display time of timeline in seconds

  // timeline min and max display values
  double tl_min, tl_max;

  int clip_selected; ///< clip in clip window
  int file_selected; ///< actual LiVES file struct number which clip_selected matches
  int current_track; ///< starts at 0

  LiVESList *selected_tracks;

  lives_mt_poly_state_t poly_state;  ///< state of polymorph window

  int render_file;

  lives_direction_t last_direction; ///< last direction timeline cursor was moved in

  track_rect *block_selected; ///< pointer to current selected block, or NULL
  track_rect *putative_block; ///< putative block to move or copy, or NULL

  double ptr_time; ///< stored timeline cursor position before playback

  double tl_fixed_length; ///< length of timeline can be fixed (secs) : TODO

  double fps; ///< fps of this timeline

  double region_start; ///< start of time region in seconds (or 0.)
  double region_end; ///< end of time region in seconds (or 0.)
  double region_init; ///< point where user pressed the mouse
  boolean region_updating;

  boolean is_rendering; ///< TRUE if we are in the process of rendering/pre-rendering to a clip, cf. mainw->is_rendering
  boolean pr_audio; ///< TRUE if we are in the process of prerendering audio to a clip

  int current_fx;

  boolean mt_frame_preview;

  boolean in_sensitise;

  boolean sel_locked;

  lives_rfx_t *current_rfx;

  char layout_name[PATH_MAX];

  // cursor warping for mouse move mode
  double hotspot_x;
  int hotspot_y;

  boolean moving_block; ///< moving block flag

  double pb_start_time; ///< playback start time in seconds. If play is stopped (not paused) we return to here.
  double pb_unpaused_start_time; ///< playback start time in seconds. If play is stopped (not paused) we return to here.

  //////////////////////////////

  int sel_x, sel_y;

  ulong mouse_mot1;
  ulong mouse_mot2;

  boolean tl_selecting; ///< for mouse select mode

  /* start and end offset overrides for inserting (used during block move) */
  ticks_t insert_start, insert_end;

  /// override for avel used during audio insert
  double insert_avel;

  LiVESList *undos;
  size_t undo_buffer_used;
  uint8_t *undo_mem;
  off_t undo_offset;
  boolean did_backup;

  char undo_text[32];
  char redo_text[32];

  boolean undoable;
  boolean redoable;

  boolean changed; ///< changed since last saved
  boolean auto_changed; ///< changed since last auto-saved

  ticks_t auto_back_time; ///< time when last backup was done (not to be confused with prefs->auto_back)

  lives_special_framedraw_rect_t *framedraw;
  int track_index;

  lives_mt_last_fx_type_t last_fx_type;

  lives_mt_fx_order_t fx_order;

  mt_opts opts;

  boolean auto_reloading;

  weed_plant_t *fm_edit_event;
  weed_plant_t *moving_fx;

  int avol_fx; ///< index of audio volume filter, delegated by user from audio volume candidates
  weed_plant_t *avol_init_event;

  ulong spin_start_func, spin_end_func;

  boolean layout_prompt; ///< on occasion, prompt user if they want to correct layout on disk or not
  boolean layout_set_properties;
  boolean ignore_load_vals;

  double user_fps;
  int user_width;
  int user_height;
  int user_arate;
  int user_achans;
  int user_asamps;
  int user_signed_endian;

  int exact_preview;

  int preview_layer;

  char timestring[TIMECODE_LENGTH];

  weed_plant_t *solo_inst; ///< instance to view solo in the frame preview

  LiVESList *tl_marks;

  weed_plant_t *pb_start_event; ///< FRAME event from which we start playback
  weed_plant_t *pb_loop_event; ///< FRAME event to loop back to (can be different from pb_start_event if we are paused)

  weed_plant_t *specific_event; ///< a pointer to some generally interesting event

  double context_time; ///< this is set when the user right clicks on a track, otherwise -1.
  boolean use_context;

  lives_cursor_t cursor_style;

  boolean is_paused;

  /* current size of frame inside playback/preview area */
  int play_width, play_height;

  /* /\* current size of playback/preview area *\/ */
  /* int play_window_width; */
  /* int play_window_height; */

  int selected_filter; ///< filter selected in poly window tab

  int top_track; ///< top (video) track in scrolled window

  boolean redraw_block; ///< block drawing of playback cursor during track redraws

  boolean was_undo_redo;

  boolean no_expose; ///< block timeline expose while we are exiting

  boolean is_ready;

  boolean aud_track_selected;

  boolean has_audio_file;

  boolean tl_mouse;

  boolean playing_sel; ///< are we playing just the time selection ?

  uint32_t idlefunc; ///< autobackup function

  LiVESList *clip_labels;

  lives_amixer_t *amixer;

  double prev_fx_time;

  boolean block_tl_move; ///< set to TRUE to block moving timeline (prevents loops with the node spinbutton)
  boolean block_node_spin; ///< set to TRUE to block moving node spinner (prevents loops with the timeline)

  boolean is_atrans; /// < force some visual changes when applying autotrans

  boolean no_frame_update;
  boolean no_expose_frame;

  char *force_load_name; ///< pointer to a string which contains a filename to be force loaded when load_event_list_activate() is called.
  ///< Normally NULL except when called from language bindings.
};  // lives_mt

typedef struct {
  lives_mt_undo_t action;
  ticks_t tc;
  void *extra;
  size_t data_len; ///< including this mt_undo
} mt_undo;

struct _lives_amixer_t {
  LiVESWidget *window;
  LiVESWidget *main_hbox;
  LiVESWidget **ch_sliders;
  LiVESWidget *gang_checkbutton;
  LiVESWidget *inv_checkbutton;
  ulong *ch_slider_fns;
  int nchans;
  lives_mt *mt;
};

// reasons for track invisibility (bitmap)
#define TRACK_I_HIDDEN_USER (1<<0)
#define TRACK_I_HIDDEN_SCROLLED (1<<1)

/// track rectangles (blocks), we translate our event_list FRAME events into these, then when exposed, the eventbox draws them
/// blocks MUST only contain frames from a single clip. They MAY NOT contain blank frames.
///
/// start and end events MUST be FRAME events

/// TODO: add scrap_file_offset so we can treat scrapfile like an ordinary clip
struct _track_rect {
  ulong uid;

  track_rect *next;
  track_rect *prev;
  weed_plant_t *start_event;
  weed_plant_t *end_event;

  ticks_t offset_start; ///< offset in sourcefile of first frame

  lives_mt_block_state_t state;
  boolean start_anchored;
  boolean end_anchored;
  boolean ordered; ///< are frames in sequential order ?

  LiVESWidget
  *eventbox; ///< pointer to eventbox widget which contains this block; we can use its "layer_number" to get the track/layer number
};

/* translation table for matching event_id to init_event */
typedef struct {
  uint64_t in;
  void *out;
} ttable;

/* clip->layout use mapping, from layout.map lives_clip_t */
typedef struct {
  char *handle;
  int64_t unique_id;
  char *name;
  LiVESList *list;
} layout_map;

//////////////////////////////////////////////////////////////////

// setup functions

lives_mt *multitrack(weed_plant_t *, int orig_file, double fps);  ///< create and return lives_mt struct
void mt_init_tracks(lives_mt *, boolean set_min_max);   ///< add basic tracks, or set tracks from mt->event_list
boolean on_multitrack_activate(LiVESMenuItem *, weed_plant_t *event_list);  ///< menuitem callback

// theming
void set_mt_colours(lives_mt *);

// delete function
boolean multitrack_delete(lives_mt *, boolean save);

// morph the poly window
void polymorph(lives_mt *, lives_mt_poly_state_t poly);
void set_poly_tab(lives_mt *, uint32_t tab);

// gui related
void set_mt_play_sizes_cfg(lives_mt *);
boolean show_in_out_images(livespointer mt);

void add_aparam_menuitems(lives_mt *);

// external control callbacks
void insert_here_cb(LiVESMenuItem *, livespointer mt);
void insert_audio_here_cb(LiVESMenuItem *, livespointer mt);
void insert_at_ctx_cb(LiVESMenuItem *, livespointer mt);
void insert_audio_at_ctx_cb(LiVESMenuItem *, livespointer mt);
void multitrack_end_cb(LiVESMenuItem *, livespointer mt);
void delete_block_cb(LiVESMenuItem *, livespointer mt);
void selblock_cb(LiVESMenuItem *, livespointer mt);
void list_fx_here_cb(LiVESMenuItem *, livespointer mt);
void edit_start_end_cb(LiVESMenuItem *, livespointer mt);
void close_clip_cb(LiVESMenuItem *, livespointer mt);
void show_clipinfo_cb(LiVESMenuItem *, livespointer mt);

boolean multitrack_insert(LiVESMenuItem *, livespointer mt);
track_rect *move_block(lives_mt *, track_rect *, double timesecs, int old_track, int new_track);

void update_grav_mode(lives_mt *);
void update_insert_mode(lives_mt *);

boolean on_render_activate(LiVESMenuItem *, livespointer mt);

void mt_set_autotrans(int idx);

// event_list functions
weed_plant_t *add_blank_frames_up_to(weed_plant_t *event_list, weed_plant_t *start_event, ticks_t end_tc, double fps);

// track functions
char *get_track_name(lives_mt *, int track_num, boolean is_audio);
void on_cback_audio_activate(LiVESMenuItem *, livespointer mt);
void on_delback_audio_activate(LiVESMenuItem *, livespointer mt);
LiVESWidget *add_audio_track(lives_mt *, int trackno, boolean behind);
int add_video_track_behind(LiVESMenuItem *, livespointer mt);
int add_video_track_front(LiVESMenuItem *, livespointer mt);
void delete_video_track(lives_mt *, int layer, boolean full);
void delete_audio_track(lives_mt *, LiVESWidget *eventbox, boolean full);
void delete_audio_tracks(lives_mt *, LiVESList *list, boolean full);
void remove_gaps(LiVESMenuItem *, livespointer mt);
void remove_first_gaps(LiVESMenuItem *, livespointer mt);
void on_insgap_sel_activate(LiVESMenuItem *, livespointer mt);
void on_insgap_cur_activate(LiVESMenuItem *, livespointer mt);
void on_split_activate(LiVESMenuItem *, livespointer mt);
void scroll_tracks(lives_mt *, int top_track, boolean set_value);
boolean track_arrow_pressed(LiVESWidget *ahbox, LiVESXEventButton *, livespointer mt);
void track_select(lives_mt *);  ///< must call after setting mt->current_track
void mt_selection_lock(LiVESMenuItem *, livespointer user_data); ///< lock the time selection
boolean mt_track_is_audio(lives_mt *, int ntrack); ///< return TRUE if ntrack is a valid backing audio track
boolean mt_track_is_video(lives_mt *, int ntrack); ///< return TRUE if ntrack is a valid video track

void mt_do_autotransition(lives_mt *, track_rect *block); ///< call this on a block to apply autotransition on it

void set_track_label_string(lives_mt *, int track, const char *label);

LiVESWidget *get_eventbox_for_track(lives_mt *, int ntrack);
void on_rename_track_activate(LiVESMenuItem *, livespointer mt);

// track mouse movement
boolean on_track_click(LiVESWidget *eventbox, LiVESXEventButton *, livespointer mt);
boolean on_atrack_click(LiVESWidget *eventbox, LiVESXEventButton *, livespointer mt);
boolean on_track_header_click(LiVESWidget *eventbox, LiVESXEventButton *, livespointer mt);
boolean on_track_between_click(LiVESWidget *eventbox, LiVESXEventButton *, livespointer mt);
boolean on_track_release(LiVESWidget *eventbox, LiVESXEventButton *event, livespointer mt);
boolean on_atrack_release(LiVESWidget *eventbox, LiVESXEventButton *event, livespointer mt);
boolean on_track_header_release(LiVESWidget *eventbox, LiVESXEventButton *, livespointer mt);
boolean on_track_between_release(LiVESWidget *eventbox, LiVESXEventButton *, livespointer mt);
boolean on_track_move(LiVESWidget *widget, LiVESXEventMotion *event, livespointer mt);
boolean on_track_header_move(LiVESWidget *widget, LiVESXEventMotion *event, livespointer mt);

void unselect_all(lives_mt *);  ///< unselect all blocks
void insert_frames(int filenum, ticks_t offset_start, ticks_t offset_end, ticks_t tc,
                   lives_direction_t direction, LiVESWidget *eventbox, lives_mt *, track_rect *in_block);
void insert_audio(int filenum, ticks_t offset_start, ticks_t offset_end, ticks_t tc,
                  double avel, lives_direction_t direction, LiVESWidget *eventbox, lives_mt *, track_rect *in_block);
void on_seltrack_toggled(LiVESWidget *, livespointer mt);
void scroll_track_by_scrollbar(LiVESScrollbar *, livespointer mt);

// block functions
void in_out_start_changed(LiVESWidget *, livespointer mt);
void in_out_end_changed(LiVESWidget *, livespointer mt);
void in_anchor_toggled(LiVESToggleButton *, livespointer mt);
void out_anchor_toggled(LiVESToggleButton *, livespointer mt);
void avel_reverse_toggled(LiVESToggleButton *, livespointer mt);
void avel_spin_changed(LiVESSpinButton *, livespointer mt);

// block API functions
track_rect *find_block_by_uid(lives_mt *, ulong uid);
ulong mt_get_last_block_uid(lives_mt *); ///< get index of last inserted (wallclock time) block for track
int mt_get_block_count(lives_mt *, int ntrack); ///< count blocks in track
double mt_get_block_sttime(lives_mt *, int ntrack, int iblock); /// get timeline start time of block
double mt_get_block_entime(lives_mt *, int ntrack, int iblock); /// get timeline end time of block

track_rect *get_block_from_track_and_time(lives_mt *, int track, double time);

int get_track_for_block(track_rect *block);
int get_clip_for_block(track_rect *block);

track_rect *mt_selblock(LiVESMenuItem *, livespointer user_data);

// timeline functions
boolean resize_timeline(lives_mt *);
void mt_tl_move(lives_mt *, double pos_abs);
void set_timeline_end_secs(lives_mt *, double secs);
boolean on_timeline_press(LiVESWidget *, LiVESXEventButton *, livespointer mt);
boolean on_timeline_release(LiVESWidget *, LiVESXEventButton *, livespointer mt);
boolean on_timeline_update(LiVESWidget *, LiVESXEventMotion *, livespointer mt);
void draw_region(lives_mt *);

boolean mt_mark_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                         livespointer user_data);

void mt_clear_timeline(lives_mt *);

void mt_zoom_in(LiVESMenuItem *, livespointer mt);
void mt_zoom_out(LiVESMenuItem *, livespointer mt);

boolean is_audio_eventbox(LiVESWidget *);

// context box text
void clear_context(lives_mt *);
void add_context_label(lives_mt *, const char *text);
void mouse_mode_context(lives_mt *);
void do_sel_context(lives_mt *);
void do_fx_list_context(lives_mt *, int fxcount);
void do_fx_move_context(lives_mt *);

// playback / animation
void multitrack_playall(lives_mt *);
void multitrack_play_sel(LiVESMenuItem *, livespointer mt);
void animate_multitrack(lives_mt *);

void mt_prepare_for_playback(lives_mt *);
void mt_post_playback(lives_mt *);

boolean mt_tcoverlay_callback(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod,
                              livespointer user_data);

// effect node controls
void on_next_node_clicked(LiVESWidget *, livespointer mt);
void on_prev_node_clicked(LiVESWidget *, livespointer mt);
void on_del_node_clicked(LiVESWidget *, livespointer mt);
void on_resetp_clicked(LiVESWidget *, livespointer mt);
void on_node_spin_value_changed(LiVESSpinButton *, livespointer mt);
double mt_get_effect_time(lives_mt *);

void on_frame_preview_clicked(LiVESButton *, livespointer mt);
void show_preview(lives_mt *, ticks_t tc);

boolean add_mt_param_box(lives_mt *);
void redraw_mt_param_box(lives_mt *);

// filter list controls
weed_plant_t *get_prev_fm(lives_mt *, int current_track, weed_plant_t *frame);
weed_plant_t *get_next_fm(lives_mt *, int current_track, weed_plant_t *frame);

void on_prev_fm_clicked(LiVESWidget *button, livespointer mt);
void on_next_fm_clicked(LiVESWidget *button, livespointer mt);
void on_fx_insb_clicked(LiVESWidget *button, livespointer mt);
void on_fx_insa_clicked(LiVESWidget *button, livespointer mt);

// utils
boolean event_list_rectify(lives_mt *, weed_plant_t *event_listy);
void *find_init_event_in_ttable(ttable *trans_table, uint64_t in, boolean normal);
void reset_renumbering(void);
boolean make_backup_space(lives_mt *, size_t space_needed);
void activate_mt_preview(lives_mt *); ///< sensitize Show Preview and Apply buttons
void **mt_get_pchain(void);
void event_list_free_undos(lives_mt *);
boolean used_in_current_layout(lives_mt *, int file);

boolean set_new_set_name(lives_mt *);

void free_thumb_cache(int fnum, frames_t fromframe);

// event_list utilities
boolean compare_filter_maps(weed_plant_t *fm1, weed_plant_t *fm2,
                            int ctrack); ///< ctrack can be -1 to compare all events, else we cf for ctrack
void move_init_in_filter_map(lives_mt *, weed_plant_t *event_list, weed_plant_t *fmap, weed_plant_t *ifrom,
                             weed_plant_t *ito, int track, boolean after);
void update_filter_events(lives_mt *, weed_plant_t *first_event, ticks_t start_tc, ticks_t end_tc,
                          int track, ticks_t new_start_tc, int new_track);
void mt_fixup_events(lives_mt *, weed_plant_t *old_event, weed_plant_t *new_event);

// event_list load/save
char *get_eload_filename(lives_mt *, boolean allow_auto_reload);
weed_plant_t *load_event_list(lives_mt *, char *eload_file);
boolean on_save_event_list_activate(LiVESMenuItem *, livespointer mt);
boolean save_event_list_inner(lives_mt *, int fd, weed_plant_t *event_list, unsigned char **mem);
boolean mt_load_recovery_layout(lives_mt *);

// layouts and layout maps
LiVESList *load_layout_map(void);
void add_markers(lives_mt *, weed_plant_t *event_list, boolean add_block_ids);
void remove_markers(weed_plant_t *event_list);
void save_layout_map(int *lmap, double *lmap_audio, const char *file, const char *dir);

void wipe_layout(lives_mt *);

void migrate_layouts(const char *old_set_name, const char *new_set_name);

LiVESList *layout_frame_is_affected(int clipno, int start, int end, LiVESList *xlays);
LiVESList *layout_audio_is_affected(int clipno, double stime, double etime, LiVESList *xlays);

boolean check_for_layout_del(lives_mt *, boolean exiting);

boolean on_load_event_list_activate(LiVESMenuItem *, livespointer mt);

void stored_event_list_free_all(boolean wiped);
void stored_event_list_free_undos(void);
void remove_current_from_affected_layouts(lives_mt *);

// auto backup
void maybe_add_mt_idlefunc(void);
uint32_t mt_idle_add(lives_mt *);
boolean recover_layout(void);
void recover_layout_cancelled(boolean is_startup);
boolean write_backup_layout_numbering(lives_mt *);
boolean mt_auto_backup(livespointer mt);

// amixer funcs
void amixer_show(LiVESButton *, livespointer mt);
void on_amixer_close_clicked(LiVESButton *, lives_mt *mt);
LiVESWidget *amixer_add_channel_slider(lives_mt *, int i);

// misc
void mt_change_disp_tracks_ok(LiVESButton *, livespointer mt);
void mt_swap_play_pause(lives_mt *, boolean put_pause);
char *set_values_from_defs(lives_mt *, boolean from_prefs);

// clip boxes
void mt_clip_select(lives_mt *, boolean scroll);
void mt_delete_clips(lives_mt *, int file);
void mt_init_clips(lives_mt *, int orig_file, boolean add);

// key shortcuts
boolean mt_prevclip(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);
boolean mt_nextclip(LiVESAccelGroup *, LiVESWidgetObject *, uint32_t keyval, LiVESXModifierType mod, livespointer);

typedef enum {
  /* default to warn about */
  LMAP_ERROR_MISSING_CLIP = 1,
  LMAP_ERROR_CLOSE_FILE = 2,
  LMAP_ERROR_DELETE_FRAMES = 3,
  LMAP_ERROR_DELETE_AUDIO = 4,

  /*non-default*/
  LMAP_ERROR_SHIFT_FRAMES = 65,
  LMAP_ERROR_ALTER_FRAMES = 66,
  LMAP_ERROR_SHIFT_AUDIO = 67,
  LMAP_ERROR_ALTER_AUDIO = 68,

  /* info */
  LMAP_INFO_SETNAME_CHANGED = 1024
} lives_lmap_error_t;

typedef struct {
  lives_lmap_error_t type;
  char *name;
  livespointer data;
  int clipno, frameno;
  double atime;
  boolean current;
} lmap_error;

#define DEF_MT_DISP_TRACKS 5

// array max: TODO - use dynamic arrays to bypass track limits
#define MAX_TRACKS 65536
#define MAX_VIDEO_TRACKS 65536
#define MAX_AUDIO_TRACKS 65536

// double -> quantised double
#define QUANT_TIME(dbltime) ((q_gint64(dbltime * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL))

// ticks -> quantised double
#define QUANT_TICKS(ticks) ((q_gint64(ticks, mt->fps) / TICKS_PER_SECOND_DBL))

// called from multitrack-gui.c

lives_mt_poly_state_t get_poly_state_from_page(lives_mt *);
boolean get_track_index(lives_mt *, weed_timecode_t tc);
track_rect *get_block_from_time(LiVESWidget *eventbox, double time, lives_mt *);
int mt_file_from_clip(lives_mt *, int clip);
weed_timecode_t mt_set_play_position(lives_mt *);
char *mt_time_to_string(double secs);
void set_mixer_track_vol(lives_mt *, int trackno, double vol);

#endif
