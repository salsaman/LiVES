// multitrack.h
// LiVES
// (c) G. Finch 2005 - 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// multitrack window

#ifndef __HAS_MULTITRACK_H__
#define __HAS_MULTITRACK_H__

#define CLIP_THUMB_WIDTH 72
#define CLIP_THUMB_HEIGHT 72

#define CLIP_LABEL_LENGTH 20

#define BLOCK_THUMB_WIDTH 40

#define BLOCK_DRAW_SIMPLE 1
#define BLOCK_DRAW_THUMB 2
#define BLOCK_DRAW_TYPE BLOCK_DRAW_THUMB

#define MT_PLAY_WIDTH_SMALL 320
#define MT_PLAY_HEIGHT_SMALL 216

#define MT_PLAY_WIDTH_EXP 480
#define MT_PLAY_HEIGHT_EXP 360

#define MT_CTX_WIDTH 320
#define MT_CTX_HEIGHT 220

#define FX_BLOCK_WIDTH 80
#define FX_BLOCK_HEIGHT 20


#define TIMECODE_LENGTH 14 ///< length of timecode text entry, must be >12

#define DEF_TIME 120 ///< default seconds when there is no event_list

#define MT_BORDER_WIDTH 20 ///< width border for window

#define IN_OUT_SEP 10 ///< min pixel separation for in/out images in poly_box (per image) 

typedef struct _mt lives_mt;

typedef struct _track_rect track_rect;

typedef struct _mt_opts mt_opts;

typedef struct _lives_amixer_t lives_amixer_t;

#define MAX_DISP_VTRACKS 5

typedef enum {
  MOUSE_MODE_MOVE,
  MOUSE_MODE_SELECT,
  MOUSE_MODE_COPY
} lives_mt_mouse_mode_t;


typedef enum {
  INSERT_MODE_NORMAL,  ///< the default (only insert if it fits)

  // not implemented yet
  INSERT_MODE_OVERWRITE, ///< overwite existing blocks
  INSERT_MODE_EXPAND, ///< repeat to fill gap
  INSERT_MODE_FILL_START, ///< insert enough to fill gap (from selection start)
  INSERT_MODE_FILL_END ///< insert enough to fill gap (to selection end)
} lives_mt_insert_mode_t;


typedef enum {
  GRAV_MODE_NORMAL,
  GRAV_MODE_LEFT,
  GRAV_MODE_RIGHT
} lives_mt_grav_mode_t;


typedef enum {
// undo actions
/// no event_list
  MT_UNDO_NONE=0,
  MT_UNDO_INSERT_BLOCK=1,
  MT_UNDO_INSERT_AUDIO_BLOCK=2,

// minimal event_list
  MT_UNDO_APPLY_FILTER=512,
  MT_UNDO_DELETE_FILTER=513,
  MT_UNDO_SPLIT=514,
  MT_UNDO_SPLIT_MULTI=515,
  MT_UNDO_FILTER_MAP_CHANGE=516,

// full backups of event_list
  MT_UNDO_DELETE_BLOCK=1024,
  MT_UNDO_MOVE_BLOCK=1025,
  MT_UNDO_REMOVE_GAPS=1026,
  MT_UNDO_DELETE_AUDIO_BLOCK=1027,
  MT_UNDO_MOVE_AUDIO_BLOCK=1028,
  MT_UNDO_INSERT_GAP=1029
} lives_mt_undo_t;



typedef enum {
  NB_ERROR_SEL,
  NB_ERROR_NOEFFECT,
  NB_ERROR_NOTRANS,
  NB_ERROR_NOCOMP,
  NB_ERROR_NOCLIP
} lives_mt_nb_error_t;
  


typedef enum {
  POLY_NONE=0,
  POLY_CLIPS,
  POLY_IN_OUT,
  POLY_FX_STACK,
  POLY_EFFECTS,
  POLY_TRANS,
  POLY_COMP,
  POLY_PARAMS
} lives_mt_poly_state_t;



typedef enum {
  DIRECTION_NEGATIVE,
  DIRECTION_POSITIVE
} lives_direction_t;


typedef enum {
  MT_LAST_FX_NONE=0,
  MT_LAST_FX_BLOCK,
  MT_LAST_FX_REGION
} lives_mt_last_fx_type_t;


typedef enum {
  FX_ORD_NONE=0,
  FX_ORD_BEFORE,
  FX_ORD_AFTER
} lives_mt_fx_order_t;


typedef enum {
// block state
  BLOCK_UNSELECTED=0,
  BLOCK_SELECTED
} lives_mt_block_state_t;



struct _mt_opts {
  gboolean set; ///< have we set opts (in mainw) ?
  gboolean move_effects; ///< should we move effects attached to a block ?
  gboolean fx_auto_preview;
  gboolean snap_over; ///< snap to overlap
  lives_mt_grav_mode_t grav_mode;
  lives_mt_mouse_mode_t mouse_mode;
  lives_mt_insert_mode_t insert_mode;
  gboolean show_audio;
  gboolean show_ctx;
  gboolean ign_ins_sel;
  gboolean follow_playback;
  gboolean insert_audio;  ///< do we insert audio with video ?
  gboolean pertrack_audio; ///< do we want pertrack audio ?
  gboolean audio_bleedthru; ///< should we allow all audio to bleed thru ?
  gboolean gang_audio; ///< gang layer audio volume levels
  gint back_audio_tracks; ///< number of backing audio tracks (currently 0 or 1)
};


struct _mt {
  // widgets
  GtkWidget *window;
  GtkWidget *top_vbox;
  GtkWidget *hbox;
  GtkWidget *play_blank;
  GtkWidget *play_box;
  GtkWidget *poly_box;
  GtkWidget *clip_scroll;
  GtkWidget *clip_inner_box;
  GtkWidget *in_out_box;
  GtkWidget *in_hbox;
  GtkWidget *out_hbox;
  GtkWidget *in_image;
  GtkWidget *out_image;
  GtkWidget *context_box;
  GtkWidget *sep_image;
  GtkWidget *timeline_table_header;
  GtkWidget *tl_eventbox;
  GtkWidget *timeline_table;
  GtkWidget *timeline;
  GtkWidget *timeline_eb;
  GtkWidget *timeline_reg;
  GtkWidget *infobar;
  GtkWidget *stop;
  GtkWidget *rewind;
  GtkWidget *sepwin;
  GtkWidget *mute_audio;
  GtkWidget *loop_continue;
  GtkWidget *insert;
  GtkWidget *audio_insert;
  GtkWidget *delblock;
  GtkWidget *clipedit;
  GtkWidget *vpaned;
  GtkWidget *hpaned;
  GtkWidget *scrollbar;
  GtkWidget *playall;
  GtkWidget *playsel;
  GtkWidget *jumpnext;
  GtkWidget *jumpback;
  GtkWidget *render;
  GtkWidget *prerender_aud;
  GtkWidget *fx_block;
  GtkWidget *fx_delete;
  GtkWidget *fx_edit;
  GtkWidget *fx_region;
  GtkWidget *fx_region_1;
  GtkWidget *fx_region_2;
  GtkWidget *fx_region_2av;
  GtkWidget *fx_region_2v;
  GtkWidget *fx_region_2a;
  GtkWidget *fx_region_3;
  GtkWidget *move_fx;
  GtkWidget *mm_menuitem;
  GtkWidget *mm_move;
  GtkWidget *mm_select;
  GtkWidget *ins_menuitem;
  GtkWidget *ins_normal;
  GtkWidget *grav_menuitem;
  GtkWidget *grav_normal;
  GtkWidget *grav_left;
  GtkWidget *grav_right;
  GtkWidget *select_track;
  GtkWidget *view_events;
  GtkWidget *view_clips;
  GtkWidget *view_in_out;
  GtkWidget *view_effects;
  GtkWidget *avel_box;
  GtkWidget *checkbutton_avel_reverse;
  GtkWidget *spinbutton_avel;
  GtkWidget *avel_scale;
  GtkWidget *spinbutton_in;
  GtkWidget *spinbutton_out;
  GtkWidget *checkbutton_start_anchored;
  GtkWidget *checkbutton_end_anchored;
  GtkWidget *timecode;
  GtkWidget *spinbutton_start;
  GtkWidget *spinbutton_end;
  GtkWidget *tl_hbox;
  GtkWidget *fx_box;
  GtkWidget *param_inner_box;
  GtkWidget *param_box;
  GtkWidget *next_node_button;
  GtkWidget *prev_node_button;
  GtkWidget *del_node_button;
  GtkWidget *node_spinbutton;
  GtkWidget *node_scale;
  GtkWidget *sel_label;
  GtkWidget *l_sel_arrow;
  GtkWidget *r_sel_arrow;
  GtkWidget *save_event_list; ///< menuitem
  GtkWidget *load_event_list; ///< menuitem
  GtkWidget *clear_event_list; ///< menuitem
  GtkWidget *tc_to_rs;
  GtkWidget *tc_to_re;
  GtkWidget *rs_to_tc;
  GtkWidget *re_to_tc;
  GtkWidget *undo;
  GtkWidget *redo;
  GtkWidget *remove_gaps;
  GtkWidget *remove_first_gaps;
  GtkWidget *split_sel;
  GtkWidget *ins_gap_sel;
  GtkWidget *ins_gap_cur;
  GtkWidget *last_filter_map;
  GtkWidget *next_filter_map;
  GtkWidget *fx_list_box;
  GtkWidget *fx_list_scroll;
  GtkWidget *fx_list_vbox;
  GtkWidget *next_fm_button;
  GtkWidget *prev_fm_button;
  GtkWidget *fx_ibefore_button;
  GtkWidget *fx_iafter_button;
  GtkWidget *cback_audio;
  GtkWidget *load_vals;
  GtkWidget *change_vals;
  GtkWidget *aparam_separator;
  GtkWidget *aparam_menuitem;
  GtkWidget *aparam_submenu;
  GtkWidget *render_sep;
  GtkWidget *render_vid;
  GtkWidget *render_aud;
  GtkWidget *view_audio;
  GtkWidget *clear_marks;
  GtkWidget *fd_frame;
  GtkWidget *apply_fx_button;
  GtkWidget *eview_button;
  GtkWidget *follow_play;
  GtkWidget *change_max_disp;
  GtkWidget *add_vid_behind;
  GtkWidget *add_vid_front;
  GtkWidget *quit;
  GtkWidget *troubleshoot;
  GtkWidget *fx_params_label;
  GtkWidget *amixer_button;
  GtkWidget *view_sel_events;
  GtkWidget *adjust_start_end;
  GtkWidget *in_eventbox;
  GtkWidget *out_eventbox;
  GtkWidget *start_in_label;
  GtkWidget *end_out_label;
  GtkWidget *context_frame;
  GtkWidget *nb;
  GtkWidget *nb_label;

  GtkWidget *open_menu;
  GtkWidget *recent_menu;
  GtkWidget *recent1;
  GtkWidget *recent2;
  GtkWidget *recent3;
  GtkWidget *recent4;

  GtkWidget *time_scrollbar;
  GtkWidget *show_layout_errors;

  GtkWidget *load_set;
  GtkWidget *save_set;

  GtkWidget *close;

  GtkWidget *clear_ds;

  GtkWidget *gens_submenu;
  GtkWidget *capture;

  GtkWidget *insa_eventbox;
  GtkWidget *insa_checkbutton;
  GtkWidget *snapo_checkbutton;

  GtkWidget *invis;

  GtkObject *spinbutton_in_adj;
  GtkObject *spinbutton_out_adj;

  GdkCursor *cursor;

  GtkObject *hadjustment;

  GList *audio_draws; ///< list of audio boxes, 0 == backing audio, 1 == track 0 audio, etc.

  GList *audio_vols; ///< layer volume levels (coarse control) - set in mixer
  GList *audio_vols_back; ///< layer volume levels (coarse control) - reset levels

  GtkAccelGroup *accel_group;
  GList *video_draws; ///< list of video timeline eventboxes, in layer order
  GtkObject *vadjustment;

  GdkDisplay *display;

  GList *aparam_view_list;

  GdkPixbuf *sepwin_pixbuf;

  gulong spin_in_func;
  gulong spin_out_func;
  gulong spin_avel_func;
  gulong check_start_func;
  gulong check_end_func;
  gulong check_avel_rev_func;

  gulong mm_move_func;
  gulong mm_select_func;

  gulong ins_normal_func;

  gulong grav_normal_func;
  gulong grav_left_func;
  gulong grav_right_func;
  
  gulong sepwin_func;
  gulong mute_audio_func;
  gulong loop_cont_func;

  gulong seltrack_func;

  gulong tc_func;

  weed_plant_t *event_list;

  weed_plant_t *init_event;   ///< current editable values
  weed_plant_t *selected_init_event;  ///< currently selected in list
  ///////////////////////
  gint num_video_tracks;
  gdouble end_secs;  ///< max display time of timeline in seconds

  // timeline min and max display values
  gdouble tl_min;
  gdouble tl_max;

  gint clip_selected; ///< clip in clip window
  gint file_selected; ///< actual LiVES file struct number which clip_selected matches
  gint current_track; ///< starts at 0

  GList *selected_tracks;

  lives_mt_poly_state_t poly_state;  ///< state of polymorph window

  gint render_file;

  lives_direction_t last_direction; ///< last direction timeline cursor was moved in

  track_rect *block_selected; ///< pointer to current selected block, or NULL
  track_rect *putative_block; ///< putative block to move or copy, or NULL

  gdouble ptr_time; ///< stored timeline cursor position before playback

  gdouble tl_fixed_length; ///< length of timeline can be fixed (secs) : TODO

  gdouble fps; ///< fps of this timeline

  gdouble region_start; ///< start of time region in seconds (or 0.)
  gdouble region_end; ///< end of time region in seconds (or 0.)
  gdouble region_init; ///< point where user pressed the mouse
  gboolean region_updating;

  gboolean is_rendering; ///< TRUE if we are in the process of rendering/pre-rendering to a clip, cf. mainw->is_rendering
  gboolean pr_audio; ///< TRUE if we are in the process of prerendering audio to a clip

  gint max_disp_vtracks;
  gint current_fx;

  gboolean mt_frame_preview;

  lives_rfx_t *current_rfx;

  gchar layout_name[256];

  // cursor warping for mouse move mode
  gdouble hotspot_x;
  gint hotspot_y;

  gboolean moving_block; ///< moving block flag

  //////////////////////////////

  gint sel_x;
  gint sel_y;

  gulong mouse_mot1;
  gulong mouse_mot2;

  gboolean tl_selecting; ///< for mouse select mode

  /* start and end offset overrides for inserting (used during block move) */
  weed_timecode_t insert_start;
  weed_timecode_t insert_end;

  /// override for avel used during audio insert
  gdouble insert_avel;

  GList *undos;
  size_t undo_buffer_used;
  unsigned char *undo_mem;
  gint undo_offset;
  gboolean did_backup;

  gchar undo_text[32];
  gchar redo_text[32];

  gboolean undoable;
  gboolean redoable;

  gboolean changed; ///< changed since last saved
  gboolean auto_changed; ///< changed since last auto-saved

  int64_t auto_back_time;

  // stuff to do with framedraw "special" widgets
  gint inwidth;
  gint inheight;
  gint outwidth;
  gint outheight;
  lives_special_framedraw_rect_t *framedraw;
  gint track_index;

  lives_mt_last_fx_type_t last_fx_type;

  lives_mt_fx_order_t fx_order;

  mt_opts opts;

  gboolean auto_reloading;

  weed_plant_t *fm_edit_event;
  weed_plant_t *moving_fx;

  int avol_fx; ///< index of audio volume filter, delegated by user from audio volume candidates
  weed_plant_t *avol_init_event;

  gulong spin_start_func;
  gulong spin_end_func;

  gboolean layout_prompt; ///< on occasion, prompt user if they want to correct layout on disk or not
  gboolean layout_set_properties;
  gboolean ignore_load_vals;

  gdouble user_fps;
  gint user_width;
  gint user_height;
  gint user_arate;
  gint user_achans;
  gint user_asamps;
  gint user_signed_endian;

  gboolean render_vidp;
  gboolean render_audp;

  gint exact_preview;


  GList *tl_marks;

  weed_plant_t *pb_start_event; ///< FRAME event from which we start playback
  weed_plant_t *pb_loop_event; ///< FRAME event to loop back to (can be different from pb_start_event if we are paused)

  weed_plant_t *specific_event; ///< a pointer to some generally interesting event

  gdouble context_time; ///< this is set when the user right clicks on a track, otherwise -1.
  gboolean use_context;

  gint cursor_style;

  gboolean is_paused;

  /* current size of frame inside playback/preview area */
  gint play_width;
  gint play_height;

  /* current size of playback/preview area */
  gint play_window_width;
  gint play_window_height;

  gint selected_filter; ///< filter selected in poly window tab

  gint top_track; ///< top (video) track in scrolled window

  gboolean redraw_block; ///< block drawing of playback cursor during track redraws

  gboolean was_undo_redo;

  gboolean no_expose; ///< block timeline expose while we are exiting

  gboolean is_ready;

  gboolean aud_track_selected;

  gboolean has_audio_file;

  gboolean tl_mouse;

  gboolean playing_sel; ///< are we playing just the time selection ?

  guint idlefunc; ///< autobackup function 

  GList *clip_labels;

  lives_amixer_t *amixer;

  gdouble prev_fx_time;

  gboolean block_tl_move; ///< set to TRUE to block moving timeline (prevents loops with the node spinbutton)
  gboolean block_node_spin; ///< set to TRUE to block moving node spinner (prevents loops with the timeline)

};  // lives_mt




typedef struct {
  lives_mt_undo_t action;
  void *extra;
  size_t data_len; ///< including this mt_undo
} mt_undo;


struct _lives_amixer_t {
  GtkWidget *main_hbox;
  GtkWidget **ch_sliders;
  GtkWidget *gang_checkbutton;
  GtkWidget *inv_checkbutton;
  gulong *ch_slider_fns;
  gint nchans;
  lives_mt *mt;
};


// reasons for track invisibility (bitmap)
#define TRACK_I_HIDDEN_USER (1<<0)
#define TRACK_I_HIDDEN_SCROLLED (1<<1)

/// track rectangles (blocks), we translate our event_list FRAME events into these, then when exposed, the eventbox draws them
/// blocks MUST only contain frames from a single clip. They MAY NOT contain blank frames.
///
/// start and end events MUST be FRAME events
struct _track_rect {
  track_rect *next;
  track_rect *prev;
  weed_plant_t *start_event;
  weed_plant_t *end_event;

  weed_timecode_t offset_start; ///< offset in sourcefile of first frame

  lives_mt_block_state_t state;
  gboolean start_anchored;
  gboolean end_anchored;
  gboolean ordered; ///< are frames in sequential order ?
  
  GtkWidget *eventbox; ///< pointer to eventbox widget which contains this block; we can use its "layer_number" to get the track/layer number

};


/* translation table for matching event_id to init_event */
typedef struct {
  void *in;
  void *out;
} ttable;


/* clip->layout use mapping, from layout.map file */
typedef struct {
  gchar *handle;
  gint64 unique_id;
  gchar *name;
  GList *list;
} layout_map;





//////////////////////////////////////////////////////////////////

// setup functions
gboolean on_multitrack_activate (GtkMenuItem *, weed_plant_t *); ///< widget callback to create the multitrack window
lives_mt *multitrack (weed_plant_t *, gint orig_file, gdouble fps); ///< create and return lives_mt struct
void mt_init_tracks (lives_mt *, gboolean set_min_max);  ///< add basic tracks, or set tracks from an event_list


// delete functions
gboolean on_mt_delete_event (GtkWidget *, GdkEvent *, gpointer mt);
gboolean multitrack_delete (lives_mt *, gboolean save); ///< free the lives_mt struct
gboolean multitrack_end (GtkMenuItem *, gpointer mt);

// morph the poly window
void polymorph (lives_mt *, lives_mt_poly_state_t poly);

// sens/desens
void mt_desensitise (lives_mt *);
void mt_sensitise (lives_mt *);

// external control callbacks
void insert_here_cb (GtkMenuItem *, gpointer mt);
void insert_audio_here_cb (GtkMenuItem *, gpointer mt);
void insert_at_ctx_cb (GtkMenuItem *, gpointer mt);
void insert_audio_at_ctx_cb (GtkMenuItem *, gpointer mt);
void multitrack_end_cb (GtkMenuItem *, gpointer mt);
void delete_block_cb (GtkMenuItem *, gpointer mt);
void selblock_cb (GtkMenuItem *, gpointer mt);
void list_fx_here_cb (GtkMenuItem *, gpointer mt);
void edit_start_end_cb (GtkMenuItem *, gpointer mt);
void close_clip_cb (GtkMenuItem *, gpointer mt);
void show_clipinfo_cb (GtkMenuItem *, gpointer mt);

// menuitem callbacks
void on_add_video_track_activate (GtkMenuItem *, gpointer mt);
void multitrack_insert (GtkMenuItem *, gpointer mt);
void multitrack_adj_start_end (GtkMenuItem *, gpointer mt);
void multitrack_audio_insert (GtkMenuItem *, gpointer mt);
void multitrack_view_events (GtkMenuItem *, gpointer mt);
void multitrack_view_sel_events (GtkMenuItem *, gpointer mt);
void on_render_activate (GtkMenuItem *, gpointer mt);
void on_prerender_aud_activate (GtkMenuItem *, gpointer mt);
void on_jumpnext_activate (GtkMenuItem *, gpointer mt);
void on_jumpback_activate (GtkMenuItem *, gpointer mt);
void on_delblock_activate (GtkMenuItem *, gpointer mt);
void on_seltrack_activate (GtkMenuItem *, gpointer mt);
void multitrack_view_details (GtkMenuItem *, gpointer mt);
void mt_add_region_effect (GtkMenuItem *, gpointer mt);
void mt_add_block_effect (GtkMenuItem *, gpointer mt);
void on_save_event_list_activate (GtkMenuItem *, gpointer mt);
void on_load_event_list_activate (GtkMenuItem *, gpointer mt);
void on_clear_event_list_activate (GtkMenuItem *, gpointer mt);
void show_frame_events_activate (GtkMenuItem *, gpointer);
void mt_save_vals_toggled (GtkMenuItem *, gpointer mt);
void mt_load_vals_toggled (GtkMenuItem *, gpointer mt);
void mt_load_vals_toggled (GtkMenuItem *, gpointer mt);
void mt_render_vid_toggled (GtkMenuItem *, gpointer mt);
void mt_render_aud_toggled (GtkMenuItem *, gpointer mt);
void mt_fplay_toggled (GtkMenuItem *, gpointer mt);
void mt_change_vals_activate (GtkMenuItem *, gpointer mt);
void on_set_pvals_clicked  (GtkWidget *button, gpointer mt);
void on_move_fx_changed (GtkMenuItem *, gpointer mt);
void select_all_time (GtkMenuItem *, gpointer mt);
void select_from_zero_time (GtkMenuItem *, gpointer mt);
void select_to_end_time (GtkMenuItem *, gpointer mt);
void select_all_vid (GtkMenuItem *, gpointer mt);
void select_no_vid (GtkMenuItem *, gpointer mt);
void on_split_sel_activate (GtkMenuItem *, gpointer mt);
void on_split_curr_activate (GtkMenuItem *, gpointer mt);
void multitrack_undo (GtkMenuItem *, gpointer mt);
void multitrack_redo (GtkMenuItem *, gpointer mt);
void on_mt_showkeys_activate (GtkMenuItem *, gpointer);
void on_mt_list_fx_activate (GtkMenuItem *, gpointer mt);
void on_mt_delfx_activate (GtkMenuItem *, gpointer mt);
void on_mt_fx_edit_activate (GtkMenuItem *, gpointer mt);
void mt_view_audio_toggled (GtkMenuItem *, gpointer mt);
void mt_view_ctx_toggled (GtkMenuItem *, gpointer mt);
void mt_ign_ins_sel_toggled (GtkMenuItem *, gpointer mt);
void mt_change_max_disp_tracks (GtkMenuItem *, gpointer mt);

// event_list functions
weed_plant_t *add_blank_frames_up_to (weed_plant_t *event_list, weed_plant_t *start_event, weed_timecode_t end_tc, gdouble fps);

// track functions
void on_cback_audio_activate (GtkMenuItem *, gpointer mt);
GtkWidget *add_audio_track (lives_mt *, gint trackno, gboolean behind);
void add_video_track_behind (GtkMenuItem *, gpointer mt);
void add_video_track_front (GtkMenuItem *, gpointer mt);
void delete_video_track(lives_mt *, gint layer, gboolean full);
void delete_audio_track(lives_mt *, GtkWidget *eventbox, gboolean full);
void delete_audio_tracks(lives_mt *, GList *list, gboolean full);
void remove_gaps (GtkMenuItem *, gpointer mt);
void remove_first_gaps (GtkMenuItem *, gpointer mt);
void on_insgap_sel_activate (GtkMenuItem *, gpointer mt);
void on_insgap_cur_activate (GtkMenuItem *, gpointer mt);
void on_split_activate (GtkMenuItem *, gpointer mt);
void scroll_tracks (lives_mt *, gint top_track);
gboolean track_arrow_pressed (GtkWidget *ahbox, GdkEventButton *, gpointer mt);
void track_select (lives_mt *); ///< must call after setting mt->current_track
gboolean mt_track_is_audio(lives_mt *, int ntrack); ///< return TRUE if ntrack is a valid backing audio track
gboolean mt_track_is_video(lives_mt *, int ntrack); ///< return TRUE if ntrack is a valid video track

void mt_do_autotransition(lives_mt *, int track, int iblock); ///< done in a hurry, FIXME


// track mouse movement
gboolean on_track_click (GtkWidget *eventbox, GdkEventButton *, gpointer mt);
gboolean on_atrack_click (GtkWidget *eventbox, GdkEventButton *, gpointer mt);
gboolean on_track_header_click (GtkWidget *eventbox, GdkEventButton *, gpointer mt);
gboolean on_track_between_click (GtkWidget *eventbox, GdkEventButton *, gpointer mt);
gboolean on_track_release (GtkWidget *eventbox, GdkEventButton *event, gpointer mt);
gboolean on_atrack_release (GtkWidget *eventbox, GdkEventButton *event, gpointer mt);
gboolean on_track_header_release (GtkWidget *eventbox, GdkEventButton *, gpointer mt);
gboolean on_track_between_release (GtkWidget *eventbox, GdkEventButton *, gpointer mt);
gboolean on_track_move (GtkWidget *widget, GdkEventMotion *event, gpointer mt);
gboolean on_track_header_move (GtkWidget *widget, GdkEventMotion *event, gpointer mt);

void unselect_all (lives_mt *); ///< unselect all blocks
void insert_frames (gint filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc, lives_direction_t direction, GtkWidget *eventbox, lives_mt *, track_rect *in_block);
void insert_audio (gint filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc, gdouble avel, lives_direction_t direction, GtkWidget *eventbox, lives_mt *, track_rect *in_block);
void on_seltrack_toggled (GtkWidget *, gpointer mt);
void scroll_track_by_scrollbar (GtkVScrollbar *sbar, gpointer mt);

// block functions
void in_out_start_changed (GtkWidget *, gpointer mt);
void in_out_end_changed (GtkWidget *, gpointer mt);
void in_anchor_toggled (GtkToggleButton *, gpointer mt);
void out_anchor_toggled (GtkToggleButton *, gpointer mt);
void avel_reverse_toggled (GtkToggleButton *, gpointer mt);
void avel_spin_changed (GtkSpinButton *, gpointer mt);

// block API functions
gint mt_get_last_block_number(lives_mt *, int ntrack); ///< get index of last inserted (wallclock time) block for track
gint mt_get_block_count(lives_mt *, int ntrack); ///< count blocks in track
gdouble mt_get_block_sttime(lives_mt *, int ntrack, int iblock); /// get timeline start time of block
gdouble mt_get_block_entime(lives_mt *, int ntrack, int iblock); /// get timeline end time of block


// timeline functions
void mt_tl_move(lives_mt *, gdouble pos_rel);
void set_timeline_end_secs (lives_mt *, gdouble secs);
gboolean on_timeline_press (GtkWidget *, GdkEventButton *, gpointer mt);
gboolean on_timeline_release (GtkWidget *, GdkEventButton *, gpointer mt);
gboolean on_timeline_update (GtkWidget *, GdkEventMotion *, gpointer mt);
gint expose_timeline_reg_event (GtkWidget *, GdkEventExpose *, gpointer mt);
gint mt_expose_laudtrack_event (GtkWidget *ebox, GdkEventExpose *, gpointer mt);
gint mt_expose_raudtrack_event (GtkWidget *ebox, GdkEventExpose *, gpointer mt);
void draw_region (lives_mt *mt);
void tc_to_rs (GtkMenuItem *, gpointer mt);
void tc_to_re (GtkMenuItem *, gpointer mt);
void rs_to_tc (GtkMenuItem *, gpointer mt);
void re_to_tc (GtkMenuItem *, gpointer mt);
gboolean mt_mark_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data);
void multitrack_clear_marks (GtkMenuItem *, gpointer mt);
void mt_show_current_frame(lives_mt *);  ///< preview th current frame (non-effects mode)
void mt_clear_timeline(lives_mt *mt);


// context box text
void clear_context (lives_mt *);
void add_context_label (lives_mt *, const gchar *text);
void mouse_mode_context(lives_mt *);
void do_sel_context (lives_mt *);
void do_fx_list_context (lives_mt *, gint fxcount);
void do_fx_move_context(lives_mt *mt);

// playback / animation
void multitrack_playall (lives_mt *);
void multitrack_play_sel (GtkMenuItem *, gpointer mt);
void animate_multitrack (lives_mt *);
void unpaint_line(lives_mt *, GtkWidget *eventbox);
void unpaint_lines(lives_mt *);

void mt_prepare_for_playback(lives_mt *);
void mt_post_playback(lives_mt *);


// effect node controls
void on_next_node_clicked  (GtkWidget *, gpointer mt);
void on_prev_node_clicked  (GtkWidget *, gpointer mt);
void on_del_node_clicked  (GtkWidget *, gpointer mt);
void on_node_spin_value_changed (GtkSpinButton *, gpointer mt);
void redraw_mt_param_box(lives_mt *);
gdouble mt_get_effect_time(lives_mt *);

void on_frame_preview_clicked (GtkButton *, gpointer mt);
void show_preview (lives_mt *, weed_timecode_t tc);

// filter list controls
weed_plant_t *get_prev_fm (lives_mt *, gint current_track, weed_plant_t *frame);
weed_plant_t *get_next_fm (lives_mt *, gint current_track, weed_plant_t *frame);

void on_prev_fm_clicked  (GtkWidget *button, gpointer user_data);
void on_next_fm_clicked  (GtkWidget *button, gpointer user_data);
void on_fx_insb_clicked  (GtkWidget *button, gpointer user_data);
void on_fx_insa_clicked  (GtkWidget *button, gpointer user_data);

// utils
guint event_list_get_byte_size(lives_mt *mt, weed_plant_t *event_list, int *num_events);  ///< returns bytes and sets num_events
gboolean event_list_rectify(lives_mt *, weed_plant_t *event_listy);
gboolean make_backup_space (lives_mt *, size_t space_needed);
void activate_mt_preview(lives_mt *); ///< sensitize Show Preview and Apply buttons
void **mt_get_pchain(void);
void event_list_free_undos(lives_mt *);
gboolean used_in_current_layout(lives_mt *, gint file);

// event_list utilities
gboolean compare_filter_maps(weed_plant_t *fm1, weed_plant_t *fm2, gint ctrack); ///< ctrack can be -1 to compare all events, else we cf for ctrack
void move_init_in_filter_map(lives_mt *mt, weed_plant_t *event_list, weed_plant_t *fmap, weed_plant_t *ifrom, weed_plant_t *ito, gint track, gboolean after);
void update_filter_events(lives_mt *, weed_plant_t *first_event, weed_timecode_t start_tc, weed_timecode_t end_tc, int track, weed_timecode_t new_start_tc, int new_track);
void mt_fixup_events(lives_mt *, weed_plant_t *old_event, weed_plant_t *new_event);

// event_list load/save
weed_plant_t *load_event_list(lives_mt *, gchar *eload_file);


// layouts and layout maps
GList *load_layout_map(void);
void add_markers(lives_mt *, weed_plant_t *event_list);
void remove_markers(weed_plant_t *event_list);
void save_layout_map (int *lmap, double *lmap_audio, const gchar *file, const gchar *dir);

void wipe_layout(lives_mt *);

void migrate_layouts (const gchar *old_set_name, const gchar *new_set_name);

GList *layout_frame_is_affected(gint clipno, gint frame);
GList *layout_audio_is_affected(gint clipno, gdouble time);

gboolean check_for_layout_del (lives_mt *, gboolean exiting);

void stored_event_list_free_all(gboolean wiped);
void stored_event_list_free_undos(void);
void remove_current_from_affected_layouts(lives_mt *);


// auto backup
guint mt_idle_add(lives_mt *);
void recover_layout(GtkButton *, gpointer);
void recover_layout_cancelled(GtkButton *, gpointer user_data);
void write_backup_layout_numbering(lives_mt *);


// internal functions
void mouse_select_end(GtkWidget *, lives_mt *);


// amixer funcs
void amixer_show (GtkButton *, gpointer mt);
void on_amixer_close_clicked (GtkButton *, lives_mt *mt);
GtkWidget * amixer_add_channel_slider (lives_mt *, gint i);


// misc
void mt_change_disp_tracks_ok (GtkButton *, gpointer user_data);
void mt_swap_play_pause (lives_mt *, gboolean put_pause);
gchar *set_values_from_defs(lives_mt *, gboolean from_prefs);


// clip boxes
void mt_clip_select (lives_mt *, gboolean scroll);
void mt_delete_clips(lives_mt *, gint file);
void mt_init_clips (lives_mt *, gint orig_file, gboolean add);


typedef enum {
  /* default to warn about */
  LMAP_ERROR_MISSING_CLIP=1,
  LMAP_ERROR_CLOSE_FILE=2,
  LMAP_ERROR_DELETE_FRAMES=3,
  LMAP_ERROR_DELETE_AUDIO=4,

  /*non-default*/
  LMAP_ERROR_SHIFT_FRAMES=65,
  LMAP_ERROR_ALTER_FRAMES=66,
  LMAP_ERROR_SHIFT_AUDIO=67,
  LMAP_ERROR_ALTER_AUDIO=68,

  /* info */
  LMAP_INFO_SETNAME_CHANGED=1024
} lives_lmap_error_t;

#endif
