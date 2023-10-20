// interface.h
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_INTERFACE_H
#define HAS_LIVES_INTERFACE_H

// info textbox sizes
#define _GUI_CONST_402 300.
#define _GUI_CONST_403 80.
#define _GUI_CONST_404 50.

#define _GUI_CONST_405 20.
#define _GUI_CONST_406 4.
#define _GUI_CONST_407 48.

// CE timeline bars
#define OVERDRAW_MARGIN 16

/// clip editor hrule height
#define CE_HRULE_HEIGHT ((int)(_GUI_CONST_405 * widget_opts.scaleH))

/// clip edit vid/aud bar height
#define CE_VIDBAR_HEIGHT ((int)(_GUI_CONST_406 * widget_opts.scaleH))

/// clip edit vid/aud bar height
#define CE_AUDBAR_HEIGHT ((int)(_GUI_CONST_407 * widget_opts.scaleH))

#define MSG_AREA_VMARGIN 0
#define LAYOUT_SIZE_MIN 0

int rbgroup_get_data(LiVESSList *rbgroup, const char *key, int def);

void restore_wm_stackpos(LiVESButton *button);

boolean update_dsu(void *lab);

void draw_little_bars(double ptrtime, int which);
double lives_ce_update_timeline(frames_t frame, double x);  ///< pointer position in timeline
boolean update_timer_bars(int clipno, int posx, int posy,
                          int width, int height, int which); ///< draw the timer bars
void redraw_timer_bars(double oldx, double newx, int which); ///< paint a damage region
void show_playbar_labels(int clipno);

void msg_area_scroll(LiVESAdjustment *, livespointer userdata);
void msg_area_scroll_to_end(LiVESWidget *, LiVESAdjustment *);
boolean on_msg_area_scroll(LiVESWidget *, LiVESXEventScroll *, livespointer user_data);
boolean expose_msg_area(LiVESWidget *, lives_painter_t *, livespointer user_data);
boolean msg_area_config(LiVESWidget *);
boolean reshow_msg_area(LiVESWidget *, lives_painter_t *, livespointer user_data);

boolean expose_vid_draw(LiVESWidget *, lives_painter_t *, livespointer psurf);
boolean expose_laud_draw(LiVESWidget *, lives_painter_t *, livespointer psurf);
boolean expose_raud_draw(LiVESWidget *, lives_painter_t *, livespointer psurf);

boolean config_vid_draw(LiVESWidget *, LiVESXEventConfigure *, livespointer user_data);
boolean config_laud_draw(LiVESWidget *, LiVESXEventConfigure *, livespointer user_data);
boolean config_raud_draw(LiVESWidget *, LiVESXEventConfigure *, livespointer user_data);

void clear_tbar_bgs(int posx, int posy, int width, int height, int which);

boolean redraw_tl_idle(void *data);

typedef struct {
  frames_t frames;
  double fps;
  LiVESWidget *dialog;
  LiVESWidget *sp_start;
  LiVESWidget *sp_frames;
  LiVESWidget *cb_allframes;
  LiVESWidget *maxflabel;
  LiVESWidget *preview_button;
} opensel_win;
opensel_win *create_opensel_window(int frames, double fps);

LiVESWidget *create_encoder_prep_dialog(const char *text1, const char *text2, boolean opt_resize);

LiVESWidget *widget_add_preview(LiVESWidget *, LiVESBox *for_preview, LiVESBox *for_button,
                                LiVESBox *for_deinterlace, int preview_type);  ///< for fileselector preview

boolean do_st_end_times_dlg(int clipno, double *start, double *end);

/// window change speed from Tools menu
void create_new_pb_speed(short type);

boolean do_audio_choice_dialog(short startup_phase);

void do_keys_window(void);

void do_mt_keys_window(void);

LiVESWidget *create_cleardisk_advanced_dialog(void);

boolean workdir_change_dialog(void);

LiVESWidget *make_autoreload_check(LiVESHBox *parent, boolean is_active);

LiVESWidget *add_list_expander(LiVESBox *, const char *title, int width, int height, LiVESList *xlist);

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *textview_type;
  LiVESWidget *textview_fps;
  LiVESWidget *textview_size;
  LiVESWidget *textview_frames;
  LiVESWidget *textview_vtime;
  LiVESWidget *textview_fsize;
  LiVESWidget *textview_ltime;
  LiVESWidget *textview_rtime;
  LiVESWidget *textview_lrate;
  LiVESWidget *textview_rrate;
} lives_clipinfo_t;

lives_clipinfo_t *create_clip_info_window(int audio_channels, boolean is_mt);

enum {
  ENTRYW_INVALID,
  ENTRYW_CLIP_RENAME,
  ENTRYW_SAVE_SET,
  ENTRYW_RELOAD_SET,
  ENTRYW_SAVE_SET_MT,
  ENTRYW_SAVE_SET_PROJ_EXPORT,
  ENTRYW_INIT_WORKDIR,
  ENTRYW_TRACKNAME_MT,
  ENTRYW_EXPORT_THEME,
};

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *entry;
  LiVESWidget *dir_entry;
  LiVESWidget *dirbutton;
  LiVESWidget *name_entry;
  LiVESWidget *warn_checkbutton;
  LiVESWidget *okbutton;
  LiVESWidget *cancelbutton;
  LiVESWidget *expander;
  LiVESWidget *exp_label;
  LiVESWidget *exp_vbox;
  LiVESWidget *xlabel;
  LiVESWidget *ylabel;
  LiVESWidget *layouts_layout;
  LiVESWidget *clips_layout;
  LiVESWidget *parent;
} _entryw;

_entryw *create_entry_dialog(int type);
_entryw *create_location_dialog(void);
_entryw *create_cds_dialog(int type);

typedef struct __insertw {
  LiVESWidget *insert_dialog;
  LiVESWidget *with_sound;
  LiVESWidget *without_sound;
  LiVESWidget *spinbutton_times;
  LiVESWidget *fit_checkbutton;
} _insertw;

_insertw *create_insert_dialog(void);

typedef struct __commentsw {
  LiVESWidget *comments_dialog;
  LiVESWidget *title_entry;
  LiVESWidget *author_entry;
  LiVESWidget *comment_entry;
  LiVESWidget *subt_checkbutton;
  LiVESWidget *subt_entry;
} _commentsw;

_commentsw *create_comments_dialog(lives_clip_t *, char *filename);

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *clear_button;
  LiVESWidget *delete_button;
  LiVESWidget *textview;
  LiVESWidget *table;
  LiVESWidget *button;
  LiVESWidget *vbox;
  LiVESWidget *scrolledwindow;
} text_window;


void do_logger_dialog(const char *title, const char *text, const char *buff, boolean add_abort);

text_window *create_text_window(const char *title_part, const char *text, LiVESTextBuffer *,
                                boolean add_buttons);

LiVESWidget *scrolled_textview(const char *text, LiVESTextBuffer *, int window_width,
                               LiVESWidget **ptextview);
typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *time_spin;
  boolean is_sel;
} aud_dialog_t;

typedef struct {
  boolean use_advanced;
  LiVESWidget *advbutton;
  LiVESWidget *adv_vbox;
  LiVESWidget *combod;
  LiVESWidget *comboo;
  LiVESWidget *spinbuttoni;
  LiVESWidget *spinbuttonw;
  LiVESWidget *spinbuttonh;
  LiVESWidget *spinbuttonf;
  LiVESWidget *radiobuttond;
} lives_tvcardw_t;

#define LIVES_PREVIEW_TYPE_VIDEO_AUDIO 1
#define LIVES_PREVIEW_TYPE_VIDEO_ONLY 2
#define LIVES_PREVIEW_TYPE_AUDIO_ONLY 3
#define LIVES_PREVIEW_TYPE_RANGE 4
#define LIVES_PREVIEW_TYPE_IMAGE_ONLY 5

// not to be confused with FILE_CHOOSER_ACTION
//  **can be set in FILESEL_TYPE_KEY** for widget objects
#define LIVES_FILE_SELECTION_UNDEFINED 0
#define LIVES_FILE_SELECTION_VIDEO_AUDIO 1
#define LIVES_FILE_SELECTION_AUDIO_ONLY 2
#define LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI 3
#define LIVES_FILE_SELECTION_VIDEO_RANGE 4
#define LIVES_FILE_SELECTION_IMAGE_ONLY 5
#define LIVES_FILE_SELECTION_SAVE 6
#define LIVES_FILE_SELECTION_OPEN 7

#define LIVES_DIR_SELECTION_CREATE_FOLDER 1024
#define LIVES_DIR_SELECTION_SELECT_FOLDER 1025
#define LIVES_DIR_SELECTION_SELECT_CREATE_FOLDER 1026

#define LIVES_DIR_SELECTION_WORKDIR 2048
#define LIVES_DIR_SELECTION_WORKDIR_INIT 2049
#define LIVES_DIR_SELECTION_DEVICES 4096

// flags
#define LIVES_SELECTION_SHOW_HIDDEN 65536

aud_dialog_t *create_audfade_dialog(int type);
LiVESWidget *create_combo_dialog(int type, LiVESList *list);

void add_procdlg_opts(xprocess *, LiVESVBox *);

xprocess *create_processing(const char *text);
xprocess *create_threaded_dialog(char *text, boolean has_cancel, boolean *td_had_focus);

// actually in gui.c
void add_to_clipmenu(void);
void add_to_clipmenu_any(int clipno);
void remove_from_clipmenu(void);
void filter_clips(lives_clipgrp_t *clipgrp, livespointer);

boolean get_play_screen_size(int *opwidth, int *opheight); /// actually in gui.c
void make_play_window(void);
void resize_play_window(void);
void kill_play_window(void);
void make_preview_box(void);
void play_window_set_title(void);
void add_to_playframe(void);
LiVESWidget *create_cdtrack_dialog(int type, livespointer user_data);

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *atrigger_button;
  LiVESWidget *atrigger_spin;
  LiVESWidget *apb_button;
  LiVESWidget *mute_button;
  LiVESWidget *debug_button;
} autolives_window;

autolives_window *autolives_pre_dialog(void);

LiVESTextView *create_output_textview(void);

void on_filesel_button_clicked(LiVESButton *, livespointer);

char *choose_file(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act, const char *title,
                  LiVESWidget *extra);
/* char *choose_file_bg(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act, const char *title, */
/*                      LiVESWidget *extra); */
LiVESWidget *choose_file_with_preview(const char *dir, const char *title, char **const filt, int preview_type);

void add_suffix_check(LiVESBox *, const char *ext);

LiVESSList *add_match_methods(LiVESLayout *, char *mopts, int height_step, int width_step, boolean add_aspect);

int rbgroup_get_data(LiVESSList *rbgroup, const char *key, int def);

LiVESWidget *add_deinterlace_checkbox(LiVESBox *parent);

const lives_special_aspect_t *add_aspect_ratio_button(LiVESSpinButton *sp_width, LiVESSpinButton *sp_height,
    LiVESBox *container);

rec_args *do_rec_desk_dlg(void);

#define DEF_AUD_FADE_SECS 10. ///< default time to offer fade audio in/out for

// textboxes for clip info
#define TB_WIDTH ((int)(_GUI_CONST_402 * widget_opts.scaleW))
#define TB_HEIGHT_VID ((int)(_GUI_CONST_403 * widget_opts.scaleH))
#define TB_HEIGHT_AUD ((int)(_GUI_CONST_404 * widget_opts.scaleH))

boolean do_utube_stream_warn(void);
lives_remote_clip_request_t *run_youtube_dialog(lives_remote_clip_request_t *);
boolean youtube_select_format(lives_remote_clip_request_t *);

#define N_LEGENDS 6

typedef struct {
  boolean scanning;
  LiVESWidget *top_label;
  LiVESWidget *dsu_label;
  LiVESWidget *used_label;
  LiVESWidget *inst_label;
  LiVESWidget *note_label;
  LiVESWidget *checkbutton;
  LiVESWidget *vlabel;
  LiVESWidget *noqlabel;
  LiVESWidget *vvlabel;
  LiVESWidget *pculabel;
  LiVESWidget *slider;
  LiVESWidget *button;
  LiVESWidget *abort_button;
  LiVESWidget *resbutton;
  LiVESWidget *expander;
  LiVESWidget *exp_vbox;
  LiVESWidget *exp_layout;
  LiVESWidget *legend_labels[N_LEGENDS];
  boolean setting, visible;
  uint64_t sliderfunc, checkfunc;
  lives_painter_surface_t *dsu_surface;
  boolean crit_dism;
  char *ext;
} _dsquotaw;

void run_diskspace_dialog(const char *target);
void run_diskspace_dialog_cb(LiVESWidget *, livespointer data);
boolean run_diskspace_dialog_idle(livespointer data);

LiVESResponseType filter_cleanup(const char *trashdir, LiVESList **rec_list, LiVESList **rem_list,
                                 LiVESList **left_list);

LiVESWidget *trash_rb(LiVESButtonBox *parent);

void draw_dsu_widget(LiVESWidget *dsu_widget);

text_window *textwindow;

#define DEF_FILE_KEY "_fc_def_file"
#define FC_ACTION_KEY "_fc_action"

#define STRUCT_KEY "_struct"

#define MATCHTYPE_KEY "_matchtype"

#define PIXBUF_KEY "_pixbuf"
#define PRV_TYPE_KEY "_preview_type"
#endif
