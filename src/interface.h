// interface.h
// LiVES
// (c) G. Finch 2003 - 2012 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_INTERFACE_H
#define HAS_LIVES_INTERFACE_H

LiVESWidget* create_info_error_dialog (const gchar *text, boolean is_blocking, int mask, lives_info_t infotype);
LiVESWidget* create_opensel_dialog (void);
LiVESWidget* create_encoder_prep_dialog (const gchar *text1, const gchar *text2, boolean opt_resize);

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

lives_clipinfo_t * create_clip_info_window (int audio_channels, boolean is_mt);


typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *entry;
  LiVESWidget *dir_entry;
  LiVESWidget *name_entry;
  LiVESWidget *warn_checkbutton;
  LiVESList *setlist;
} _entryw;

_entryw* create_rename_dialog (int type);
_entryw* create_location_dialog (int type);
_entryw* create_cds_dialog (int type);

typedef struct __insertw {
  LiVESWidget *insert_dialog;
  LiVESWidget *with_sound;
  LiVESWidget *without_sound;
  LiVESWidget *spinbutton_times;
  LiVESWidget *fit_checkbutton;
} _insertw;

_insertw* create_insert_dialog (void);


typedef struct __commentsw {
  LiVESWidget *comments_dialog;
  LiVESWidget *title_entry;
  LiVESWidget *author_entry;
  LiVESWidget *comment_entry;
  LiVESWidget *subt_checkbutton;
  LiVESWidget *subt_entry;
} _commentsw;

_commentsw* create_comments_dialog (lives_clip_t *sfile, gchar *filename);


typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *clear_button;
  LiVESWidget *delete_button;
  LiVESWidget *textview;
} text_window;

text_window* create_text_window (const gchar *title_part, const gchar *text, LiVESTextBuffer *);


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



aud_dialog_t *create_audfade_dialog (int type);
LiVESWidget *create_combo_dialog (int type, livespointer user_data);

_commentsw *commentsw;
_entryw *renamew;
_entryw *locw;
_insertw *insertw;
text_window *textwindow;


#define DEF_AUD_FADE_SECS 10. ///< default time to offer fade audio in/out for



#endif
