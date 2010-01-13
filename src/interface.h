// interface.h
// LiVES
// (c) G. Finch 2003 - 2009 <salsaman@xs4all.nl>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


#define DEFAULT_FRAME_HSIZE 320
#define DEFAULT_FRAME_VSIZE 240

#define PROG_LABEL_WIDTH 540

void load_theme (void);

GtkWidget* create_fileselection (const gchar *title, gint preview_type, gpointer free_on_cancel);
GtkWidget* create_window4 (void);
GtkWidget* create_dialog2 (gint warning_mask);
GtkWidget* create_dialog3 (const gchar *text, gboolean is_blocking, gint warning_mask);
GtkWidget* create_opensel_dialog (void);
GtkWidget* create_cdtrack_dialog (gint type, gpointer user_data);
GtkWidget* create_encoder_prep_dialog (const gchar *text1, const gchar *text2, gboolean opt_resize);

void widget_add_preview(GtkBox *for_preview, GtkBox *for_button, GtkBox *for_deinterlace, gint preview_type);  // for fileselector preview

gboolean do_audio_choice_dialog(short startup_phase);

void do_layout_recover_dialog(void);

typedef struct _fileinfo {
  GtkWidget *info_window;
  GtkWidget *textview24;
  GtkWidget *textview25;
  GtkWidget *textview26;
  GtkWidget *textview27;
  GtkWidget *textview28;
  GtkWidget *textview29;
  GtkWidget *textview_ltime;
  GtkWidget *textview_rtime;
  GtkWidget *textview_lrate;
  GtkWidget *textview_rrate;
} fileinfo;

fileinfo* create_info_window (gint audio_channels, gboolean is_mt);


typedef struct {
  GtkWidget *dialog;
  GtkWidget *entry;
  GtkWidget *warn_checkbutton;
  GList *setlist;
} _entryw;

_entryw* create_rename_dialog (gint type);
_entryw* create_location_dialog (void);
_entryw* create_cds_dialog (gint type);

typedef struct __insertw {
  GtkWidget *insert_dialog;
  GtkWidget *with_sound;
  GtkWidget *without_sound;
  GtkWidget *spinbutton_times;
  GtkWidget *fit_checkbutton;
} _insertw;

_insertw* create_insert_dialog (void);

typedef struct __xranw {
  // xmms random play
  GtkWidget *rp_dialog;
  GtkWidget *numtracks;
  GtkWidget *dir;
  GtkWidget *subdir_check;
  GtkWidget *minsize;
  GtkWidget *maxsize;
} _xranw;

_xranw* create_rp_dialog (void);

typedef struct __commentsw {
  GtkWidget *comments_dialog;
  GtkWidget *title_entry;
  GtkWidget *author_entry;
  GtkWidget *comment_entry;
} _commentsw;

_commentsw* create_comments_dialog (void);


typedef struct {
  GtkWidget *dialog;
  GtkWidget *clear_button;
  GtkWidget *delete_button;
  GtkWidget *textview;
} text_window;

text_window* create_text_window (const gchar *title_part, const gchar *text, GtkTextBuffer *);


typedef struct {
  GtkWidget *dialog;
  GtkWidget *time_spin;
  gboolean is_sel;
} aud_dialog_t;

aud_dialog_t *create_audfade_dialog (gint type);

_commentsw *commentsw;
_xranw *xranw;
_entryw *renamew;
_entryw *locw;
_insertw *insertw;
text_window *textwindow;

gchar *choose_file(gchar *dir, gchar *fname, gchar **filt, GtkFileChooserAction act, GtkWidget *extra);


#define MAX_FADE_SECS 30.

