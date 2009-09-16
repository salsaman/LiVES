// multitrack.h
// LiVES
// (c) G. Finch 2005 - 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// multitrack window

#ifndef __HAS_MULTITRACK_H__
#define __HAS_MULTITRACK_H__

#include "events.h"

#define CLIP_THUMB_WIDTH 80
#define CLIP_THUMB_HEIGHT 80

#define CLIP_LABEL_LENGTH 20

#define BLOCK_THUMB_WIDTH 40

#define BLOCK_DRAW_SIMPLE 1
#define BLOCK_DRAW_THUMB 2
#define BLOCK_DRAW_TYPE BLOCK_DRAW_THUMB

#define MT_PLAY_WIDTH_SMALL 320
#define MT_PLAY_HEIGHT_SMALL 240

#define MT_PLAY_WIDTH_EXP 480
#define MT_PLAY_HEIGHT_EXP 360

#define MT_CTX_WIDTH 320
#define MT_CTX_HEIGHT 240

#define TIMECODE_LENGTH 14 // length of timecode text entry, must be >12

#define DEF_TIME 120 // default seconds when there is no event_list

#define MT_BORDER_WIDTH 20 // width border for window

#define IN_OUT_SEP 10 // min pixel separation for in/out images in poly_box (per image) 

typedef struct _mt lives_mt;

typedef struct _track_rect track_rect;

typedef struct _mt_opts mt_opts;

struct _mt_opts {
  gboolean set; // have we set opts (in mainw) ?
  gboolean move_effects; // should we move effects attached to a block ?
  gboolean fx_auto_preview;
  gboolean snap_over; // snap to overlap
  gboolean snapt; // block snap
  gshort mouse_mode;
  gboolean show_audio;
  gboolean show_ctx;
  gboolean ign_ins_sel;
  gboolean follow_playback;
  gboolean insert_audio;  // do we insert audio with video ?
  gboolean pertrack_audio; // do we want pertrack audio ?
  gboolean audio_bleedthru; // should we allow all audio to bleed thru ?
  gboolean gang_audio; // gang layer audio volume levels
  gint back_audio_tracks; // number of backing audio tracks (currently 0 or 1)
};

#define MOUSE_MODE_MOVE 1
#define MOUSE_MODE_SELECT 2
#define MOUSE_MODE_COPY 3

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
  GtkWidget *fx_auto_prev;
  GtkWidget *fx_auto_prev_box;
  GtkWidget *mm_menuitem;
  GtkWidget *mm_move;
  GtkWidget *mm_select;
  GtkWidget *snapt_menuitem;
  GtkWidget *snapt_on;
  GtkWidget *snapt_off;
  GtkWidget *insa_menuitem;
  GtkWidget *insa_menuitemsep;
  GtkWidget *insa_on;
  GtkWidget *insa_off;
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
  GtkWidget *preview_button; // button
  GtkWidget *save_event_list; // menuitem
  GtkWidget *load_event_list; // menuitem
  GtkWidget *clear_event_list; // menuitem
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
  GtkWidget *save_vals;
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
  GtkWidget *cview_button;
  GtkWidget *eview_button;
  GtkWidget *follow_play;
  GtkWidget *change_max_disp;
  GtkWidget *add_vid_behind;
  GtkWidget *add_vid_front;
  GtkWidget *quit;
  GtkWidget *fx_params_label;
  GtkWidget *amixer_button;
  GtkWidget *view_sel_events;

  GdkCursor *cursor;

  GtkObject *hadjustment;
  GtkWidget *time_scrollbar;

  GList *audio_draws; // list of audio boxes, 0 == backing audio, 1 == track 0 audio, etc.

  GList *audio_vols; // layer volume levels (coarse control) - set in mixer

  GtkAccelGroup *accel_group;
  GList *video_draws; // list of video timeline eventboxes, in layer order
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
  
  gulong snapt_on_func;
  gulong snapt_off_func;

  gulong insa_on_func;
  gulong insa_off_func;

  gulong sepwin_func;
  gulong mute_audio_func;
  gulong loop_cont_func;

  gulong seltrack_func;

  weed_plant_t *event_list;

  weed_plant_t *init_event;   // current editable values
  weed_plant_t *selected_init_event;  // currently selected in list
  ///////////////////////
  gint num_video_tracks;
  gdouble end_secs;  // max display time of timeline in seconds

  // timeline min and max display values
  gdouble tl_min;
  gdouble tl_max;

  gint clip_selected; // clip in clip window
  gint file_selected; // actual LiVES file struct number which clip_selected matches
  gint current_track; // starts at 0

  GList *selected_tracks;

  gshort poly_state;  // state of polymorph window
#define POLY_NONE 0
#define POLY_CLIPS 1
#define POLY_IN_OUT 2
#define POLY_EFFECT 3
#define POLY_FX_LIST 4


  gshort insert_mode;
#define INSERT_MODE_NORMAL 1  // the default
#define INSERT_MODE_OVERWRITE 2
#define INSERT_MODE_EXPAND 3
  //#define INSERT_MODE_FILL 4

  gint render_file;

  gshort last_direction; // last direction timeline cursor was moved in
#define DIRECTION_NEGATIVE 0
#define DIRECTION_POSITIVE 1

  track_rect *block_selected; // pointer to current selected block, or NULL
  track_rect *putative_block; // putative block to move or copy, or NULL

  gdouble ptr_time; // stored timeline cursor position before playback

  gdouble tl_fixed_length; // length of timeline can be fixed (secs) : TODO

  gdouble fps; // fps of this timeline

  gdouble region_start; // start of time region in seconds (or 0.)
  gdouble region_end; // end of time region in seconds (or 0.)
  gdouble region_init; // point where user pressed the mouse
  gboolean region_updating;

  gboolean is_rendering; // TRUE if we are in the process of rendering/pre-rendering to a clip, cf. mainw->is_rendering
  gboolean pr_audio; // TRUE if we are in the process of prerendering audio to a clip

#define MAX_DISP_VTRACKS 4
  gint max_disp_vtracks;
  gint current_fx;

  gboolean mt_frame_preview;

  lives_rfx_t *current_rfx;

  gchar layout_name[256];

  // cursor warping for mouse move mode
  gint hotspot_x;
  gint hotspot_y;

  gboolean moving_block; // moving block flag

  //////////////////////////////

  gint sel_x;
  gint sel_y;

  gulong mouse_mot1;
  gulong mouse_mot2;

  gboolean tl_selecting; // for mouse select mode

  /* start and end offset overrides for inserting (used during block move) */
  weed_timecode_t insert_start;
  weed_timecode_t insert_end;

  // override for avel used during audio insert
  gdouble insert_avel;

  GList *undos;
  size_t undo_buffer_used;
  unsigned char *undo_mem;
  gboolean did_backup;
  gint undo_offset;

  gchar undo_text[32];
  gchar redo_text[32];

  gboolean undoable;
  gboolean redoable;

  gboolean changed;

  // stuff to do with framedraw "special" widgets
  gint inwidth;
  gint inheight;
  gint outwidth;
  gint outheight;
  lives_special_framedraw_rect_t *framedraw;
  gint track_index;

  gint last_fx_type;

#define MT_LAST_FX_NONE 0
#define MT_LAST_FX_BLOCK 1
#define MT_LAST_FX_REGION 2

  gshort fx_order;

#define FX_ORD_NONE 0
#define FX_ORD_BEFORE 1
#define FX_ORD_AFTER 2

  mt_opts opts;

  gboolean auto_reloading;

  weed_plant_t *fm_edit_event;
  weed_plant_t *moving_fx;

  int avol_fx; // index of audio volume filter, delegated by user from audio volume candidates
  weed_plant_t *avol_init_event;

  gulong spin_start_func;
  gulong spin_end_func;

  gboolean layout_prompt; // on occasion, prompt user if they want to correct layout on disk or not
  gboolean layout_set_properties;
  gboolean save_all_vals;
  gboolean ignore_load_vals;

#define MT_PRESERVE_VALS 2

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

  weed_plant_t *pb_start_event; // FRAME event from which we start playback
  weed_plant_t *pb_loop_event; // FRAME event to loop back to (can be different from pb_start_event if we are paused)

  weed_plant_t *specific_event; // a pointer to some generally interesting event

  gdouble context_time; // this is set when the user right clicks on a track, otherwise -1.

  gint cursor_style;

  gboolean is_paused;

  /* current size of frame inside playback/preview area */
  gint play_width;
  gint play_height;

  /* current size of playback/preview area */
  gint play_window_width;
  gint play_window_height;

  gboolean redraw_block; // block drawing of playback cursor during track redraws

  gboolean was_undo_redo;

  gboolean no_expose; // block timeline expose while we are exiting

  gboolean is_ready;

  gboolean aud_track_selected;

  gboolean has_audio_file;

};  // lives_mt


// undo actions
// no event_list
#define MT_UNDO_NONE 0
#define MT_UNDO_INSERT_BLOCK 1
#define MT_UNDO_INSERT_AUDIO_BLOCK 2

// minimal event_list
#define MT_UNDO_APPLY_FILTER 512
#define MT_UNDO_DELETE_FILTER 513
#define MT_UNDO_SPLIT 514
#define MT_UNDO_SPLIT_MULTI 515
#define MT_UNDO_FILTER_MAP_CHANGE 516

// full backups of event_list
#define MT_UNDO_DELETE_BLOCK 1024
#define MT_UNDO_MOVE_BLOCK 1025
#define MT_UNDO_REMOVE_GAPS 1026
#define MT_UNDO_DELETE_AUDIO_BLOCK 1027
#define MT_UNDO_MOVE_AUDIO_BLOCK 1028
#define MT_UNDO_INSERT_GAP 1029


typedef struct {
  gint action;
  void *extra;
  size_t data_len; // including this mt_undo
} mt_undo;


typedef struct {
  GtkWidget **ch_sliders;
  GtkWidget *gang_checkbutton;
  GtkWidget *inv_checkbutton;
  gulong *ch_slider_fns;
  gint nchans;
  lives_mt *mt;
} lives_amixer_t;


// reasons for track invisibility (bitmap)
#define TRACK_I_OUTSCROLLED_PRE (1<<0)
#define TRACK_I_OUTSCROLLED_POST (1<<1)
#define TRACK_I_HIDDEN_USER (1<<2)

struct _track_rect {
  // track rectangles (blocks), we translate our event_list FRAME events into these, then when exposed, the eventbox draws them
  // blocks MUST only contain frames from a single clip. They MAY NOT contain blank frames.
  // start and end events MUST be FRAME events
  track_rect *next;
  track_rect *prev;
  weed_plant_t *start_event;
  weed_plant_t *end_event;

  weed_timecode_t offset_start; // offset in sourcefile of first frame

  gshort state;
  gboolean start_anchored;
  gboolean end_anchored;
  gboolean ordered; // are frames in sequential order ?
  
  GtkWidget *eventbox; // pointer to eventbox widget which contains this block; we can use its "layer_number" to get the track/layer number

};

// block state
#define BLOCK_UNSELECTED 0
#define BLOCK_SELECTED 1


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
gboolean on_multitrack_activate (GtkMenuItem *, weed_plant_t *); // widget callback to create the multitrack window
lives_mt *multitrack (weed_plant_t *, gint orig_file, gdouble fps); // create and return lives_mt struct
void init_tracks (lives_mt *, gboolean set_min_max);  // add basic tracks, or set tracks from an event_list
void init_clips (lives_mt *, gint orig_file, gboolean add); // init clip window (or add a new clip)

// delete functions
gboolean on_mt_delete_event (GtkWidget *, GdkEvent *, gpointer mt);
gboolean multitrack_delete (lives_mt *); // free the lives_mt struct
gboolean multitrack_end (GtkMenuItem *, gpointer mt);

// morph the poly window
void polymorph (lives_mt *, gshort poly);

/// sens/desens
void mt_desensitise (lives_mt *);
void mt_sensitise (lives_mt *);

// external control callbacks
void insert_here_cb (GtkMenuItem *, gpointer mt);
void insert_audio_here_cb (GtkMenuItem *, gpointer mt);
void multitrack_end_cb (GtkMenuItem *, gpointer mt);
void delete_block_cb (GtkMenuItem *, gpointer mt);
void selblock_cb (GtkMenuItem *, gpointer mt);
void list_fx_here_cb (GtkMenuItem *, gpointer mt);

//// menuitem callbacks
void on_add_video_track_activate (GtkMenuItem *, gpointer mt);
void multitrack_insert (GtkMenuItem *, gpointer mt);
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
void on_snap_over_changed (GtkMenuItem *, gpointer mt);
void on_move_fx_changed (GtkMenuItem *, gpointer mt);
void on_fx_auto_prev_changed (GtkMenuItem *, gpointer mt);
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

/// track functions
void on_cback_audio_activate (GtkMenuItem *, gpointer mt);
GtkWidget *add_audio_track (lives_mt *, gint trackno, gboolean behind);
void add_video_track_behind (GtkMenuItem *, gpointer mt);
void add_video_track_front (GtkMenuItem *, gpointer mt);
void delete_video_track(lives_mt *, gint layer, gboolean full);
void delete_audio_track(lives_mt *mt, GtkWidget *eventbox, gboolean full);
void delete_audio_tracks(lives_mt *mt, GList *list, gboolean full);
void remove_gaps (GtkMenuItem *, gpointer mt);
void remove_first_gaps (GtkMenuItem *, gpointer mt);
void on_insgap_sel_activate (GtkMenuItem *, gpointer mt);
void on_insgap_cur_activate (GtkMenuItem *, gpointer mt);
void on_split_activate (GtkMenuItem *, gpointer mt);
void scroll_tracks (lives_mt *mt);
gboolean track_arrow_pressed (GtkWidget *ahbox, GdkEventButton *, gpointer mt);

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

void unselect_all (lives_mt *); // unselect all blocks
void insert_frames (gint filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc, gshort direction, GtkWidget *eventbox, lives_mt *, track_rect *in_block);
void insert_audio (gint filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc, gdouble avel, gshort direction, GtkWidget *eventbox, lives_mt *, track_rect *in_block);
void on_seltrack_toggled (GtkToggleButton *, gpointer mt);
void scroll_track_by_scrollbar (GtkVScrollbar *sbar, gpointer mt);

// block functions
void in_out_start_changed (GtkWidget *, gpointer mt);
void in_out_end_changed (GtkWidget *, gpointer mt);
void in_anchor_toggled (GtkToggleButton *, gpointer mt);
void out_anchor_toggled (GtkToggleButton *, gpointer mt);
void avel_reverse_toggled (GtkToggleButton *, gpointer mt);
void avel_spin_changed (GtkSpinButton *, gpointer mt);

// timeline functions
void mt_tl_move(lives_mt *mt, gdouble pos_rel);
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
void mt_show_current_frame(lives_mt *);  // preview th current frame (non-effects mode)

// context box text
void clear_context (lives_mt *);
void add_context_label (lives_mt *, gchar *text);
void mouse_mode_context(lives_mt *);
void do_sel_context (lives_mt *);
void do_fx_list_context (lives_mt *mt, gint fxcount);

// playback / animation
void multitrack_playall (lives_mt *mt);
void animate_multitrack (lives_mt *);
void unpaint_line(lives_mt *, GtkWidget *eventbox);
void unpaint_lines(lives_mt *);

// effect node controls
void on_next_node_clicked  (GtkWidget *, gpointer mt);
void on_prev_node_clicked  (GtkWidget *, gpointer mt);
void on_del_node_clicked  (GtkWidget *, gpointer mt);
void on_node_spin_value_changed (GtkSpinButton *, gpointer mt);

void on_frame_preview_clicked (GtkButton *, gpointer mt);
void show_preview (lives_mt *, weed_timecode_t tc);

// filter list controls
weed_plant_t *get_prev_fm (lives_mt *mt, gint current_track, weed_plant_t *frame);
weed_plant_t *get_next_fm (lives_mt *mt, gint current_track, weed_plant_t *frame);

void on_prev_fm_clicked  (GtkWidget *button, gpointer user_data);
void on_next_fm_clicked  (GtkWidget *button, gpointer user_data);
void on_fx_insb_clicked  (GtkWidget *button, gpointer user_data);
void on_fx_insa_clicked  (GtkWidget *button, gpointer user_data);

// utils
guint event_list_get_byte_size(weed_plant_t *event_list, int *num_events);  // returns bytes and sets num_events
gboolean event_list_rectify(lives_mt *, weed_plant_t *event_list, gboolean check_clips);
gboolean make_backup_space (lives_mt *, size_t space_needed);
void activate_mt_preview(lives_mt *mt); // sensitize Show Preview and Apply buttons
void **mt_get_pchain(void);

// event_list utilities
gboolean compare_filter_maps(weed_plant_t *fm1, weed_plant_t *fm2, gint ctrack); // ctrack can be -1 to compare all events, else we cf for ctrack
void move_init_in_filter_map(weed_plant_t *event_list, weed_plant_t *fmap, weed_plant_t *ifrom, weed_plant_t *ito, gint track, gboolean after);
void update_filter_events(lives_mt *, weed_plant_t *first_event, weed_timecode_t start_tc, weed_timecode_t end_tc, int track, weed_timecode_t new_start_tc, int new_track);
void mt_fixup_events(lives_mt *mt, weed_plant_t *old_event, weed_plant_t *new_event);

// event_list load/save
weed_plant_t *load_event_list(lives_mt *, gchar *eload_file);


// layout maps
GList *load_layout_map(void);
void save_layout_map (int *lmap, double *lmap_audio, gchar *file, gchar *dir);
void migrate_layouts (gchar *old_set_name, gchar *new_set_name);
gboolean layout_frame_is_affected(gint clipno, gint frame);
gboolean layout_audio_is_affected(gint clipno, gdouble time);
void add_markers(lives_mt *, weed_plant_t *event_list);
void remove_markers(weed_plant_t *event_list);


// internal functions
void mouse_select_end(GtkWidget *widget, lives_mt *mt);


// misc
void mt_change_disp_tracks_ok (GtkButton *, gpointer user_data);
void mt_swap_play_pause (lives_mt *mt, gboolean put_pause);
void amixer_show (GtkButton *button, gpointer user_data);


/* default to warn about */
#define LMAP_ERROR_MISSING_CLIP 1
#define LMAP_ERROR_CLOSE_FILE 2
#define LMAP_ERROR_DELETE_FRAMES 3
#define LMAP_ERROR_DELETE_AUDIO 6

/*non-default*/
#define LMAP_ERROR_SHIFT_FRAMES 4
#define LMAP_ERROR_ALTER_FRAMES 5
#define LMAP_ERROR_SHIFT_AUDIO 7
#define LMAP_ERROR_ALTER_AUDIO 8

/* info */
#define LMAP_INFO_SETNAME_CHANGED 16


#endif
