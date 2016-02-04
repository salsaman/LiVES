// interface.h
// LiVES
// (c) G. Finch 2003 - 2012 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_INTERFACE_H
#define HAS_LIVES_INTERFACE_H

LiVESWidget *create_info_error_dialog(lives_dialog_t info_type, const char *text, LiVESWindow *transient, int mask, boolean is_blocking);
LiVESWidget *create_opensel_dialog(void);
LiVESWidget *create_encoder_prep_dialog(const char *text1, const char *text2, boolean opt_resize);

void widget_add_preview(LiVESWidget *widget, LiVESBox *for_preview, LiVESBox *for_button,
                        LiVESBox *for_deinterlace, int preview_type);  ///< for fileselector preview

boolean do_audio_choice_dialog(short startup_phase);

void do_layout_recover_dialog(void);

LiVESWidget *create_cleardisk_advanced_dialog(void);

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *textview24;
  LiVESWidget *textview25;
  LiVESWidget *textview26;
  LiVESWidget *textview27;
  LiVESWidget *textview28;
  LiVESWidget *textview29;
  LiVESWidget *textview_ltime;
  LiVESWidget *textview_rtime;
  LiVESWidget *textview_lrate;
  LiVESWidget *textview_rrate;
} lives_clipinfo_t;

lives_clipinfo_t *create_clip_info_window(int audio_channels, boolean is_mt);


typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *entry;
  LiVESWidget *dir_entry;
  LiVESWidget *name_entry;
  LiVESWidget *warn_checkbutton;
  LiVESList *setlist;
} _entryw;

_entryw *create_rename_dialog(int type);
_entryw *create_location_dialog(int type);
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

_commentsw *create_comments_dialog(lives_clip_t *sfile, char *filename);


typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *clear_button;
  LiVESWidget *delete_button;
  LiVESWidget *textview;
} text_window;

text_window *create_text_window(const char *title_part, const char *text, LiVESTextBuffer *);


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
#define LIVES_PREVIEW_TYPE_AUDIO_ONLY 2
#define LIVES_PREVIEW_TYPE_RANGE 3

#define LIVES_FILE_SELECTION_VIDEO_AUDIO 1
#define LIVES_FILE_SELECTION_AUDIO_ONLY 2
#define LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI 3
#define LIVES_FILE_SELECTION_VIDEO_RANGE 4



aud_dialog_t *create_audfade_dialog(int type);
LiVESWidget *create_combo_dialog(int type, livespointer user_data);

xprocess *create_processing(const char *text);
void add_to_clipmenu(void);
void remove_from_clipmenu(void);
void make_play_window(void);
void resize_play_window(void);
void kill_play_window(void);
void make_preview_box(void);
void play_window_set_title(void);
void add_to_playframe(void);
LiVESWidget *create_cdtrack_dialog(int type, livespointer user_data);
LiVESTextView *create_output_textview(void);
char *choose_file(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act, const char *title, LiVESWidget *extra);
LiVESWidget *choose_file_with_preview(const char *dir, const char *title, int preview_type);
void add_suffix_check(LiVESBox *box, const char *ext);


_commentsw *commentsw;
_entryw *renamew;
_entryw *locw;
_insertw *insertw;
text_window *textwindow;


#define DEF_AUD_FADE_SECS 10. ///< default time to offer fade audio in/out for


#define MIN_MSGBOX_WIDTH ((int)(mainw->scr_width>1024?(820.*widget_opts.scale):600))

#define TB_WIDTH ((int)(200.*widget_opts.scale))
#define TB_HEIGHT_VID ((int)(80.*widget_opts.scale))
#define TB_HEIGHT_AUD ((int)(50.*widget_opts.scale))

#define RW_ENTRY_DISPWIDTH 40

#endif
