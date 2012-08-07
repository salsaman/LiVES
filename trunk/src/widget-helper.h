// widget-helper.h
// LiVES
// (c) G. Finch 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H

typedef enum {
  LIVES_CURSOR_NORMAL=0,  ///< must be zero
  LIVES_CURSOR_BLOCK,
  LIVES_CURSOR_AUDIO_BLOCK,
  LIVES_CURSOR_BUSY,
  LIVES_CURSOR_FX_BLOCK
} lives_cursor_t;


#ifdef GUI_GTK
typedef GtkObject                         LiVESObject;
typedef GtkWidget                         LiVESWidget;
typedef GtkDialog                         LiVESDialog;
typedef GtkBox                            LiVESBox;
typedef GtkComboBox                       LiVESCombo;
typedef GtkComboBox                       LiVESComboBox;
typedef GtkComboBoxText                   LiVESComboBoxText;
typedef GtkToggleButton                   LiVESToggleButton;
typedef GtkTextView                       LiVESTextView;
typedef GtkEntry                          LiVESEntry;

typedef GtkAdjustment                     LiVESAdjustment;

typedef GdkPixbuf                         LiVESPixbuf;

typedef GdkWindow                         LiVESXWindow;

typedef GdkEventButton                    LiVESEventButton;

typedef GError                          LiVESError;

#ifndef IS_MINGW
typedef gboolean                          boolean;
#endif

typedef GList                             LiVESList;
typedef GSList                            LiVESSList;


typedef GdkPixbufDestroyNotify            LiVESPixbufDestroyNotify;

typedef GdkInterpType                     LiVESInterpType;

typedef gpointer                          LiVESObjectPtr;

#define LIVES_BOX(widget) GTK_BOX(widget)
#define LIVES_COMBO(widget) GTK_COMBO_BOX(widget)
#define LIVES_COMBO_BOX(widget) GTK_COMBO_BOX(widget)
#define LIVES_COMBO_BOX_TEXT(widget) GTK_COMBO_BOX_TEXT(widget)

#define LIVES_WIDGET_IS_SENSITIVE(widget) GTK_WIDGET_IS_SENSITIVE(widget)
#define LIVES_IS_COMBO(widget) GTK_IS_COMBO_BOX(widget)

#define LIVES_INTERP_BEST   GDK_INTERP_HYPER
#define LIVES_INTERP_NORMAL GDK_INTERP_BILINEAR
#define LIVES_INTERP_FAST   GDK_INTERP_NEAREST

typedef GLogLevelFlags LiVESLogLevelFlags;

#define LIVES_LOG_LEVEL_WARNING G_LOG_LEVEL_WARNING
#define LIVES_LOG_LEVEL_MASK G_LOG_LEVEL_MASK
#define LIVES_LOG_LEVEL_CRITICAL G_LOG_LEVEL_CRITICAL
#define LIVES_LOG_FATAL_MASK G_LOG_FATAL_MASK

#define LIVES_CONTROL_MASK GDK_CONTROL_MASK
#define LIVES_ALT_MASK     GDK_MOD1_MASK
#define LIVES_SHIFT_MASK   GDK_SHIFT_MASK
#define LIVES_LOCK_MASK    GDK_LOCK_MASK

#ifdef GDK_KEY_a
#define LIVES_KEY_Left GDK_KEY_Left
#define LIVES_KEY_Right GDK_KEY_Right
#define LIVES_KEY_Up GDK_KEY_Up
#define LIVES_KEY_Down GDK_KEY_Down

#define LIVES_KEY_Space GDK_KEY_space
#define LIVES_KEY_BackSpace GDK_KEY_BackSpace
#define LIVES_KEY_Return GDK_KEY_Return
#define LIVES_KEY_Tab GDK_KEY_Tab

#define LIVES_KEY_q GDK_KEY_q

#define LIVES_KEY_1 GDK_KEY_1
#define LIVES_KEY_2 GDK_KEY_2
#define LIVES_KEY_3 GDK_KEY_3
#define LIVES_KEY_4 GDK_KEY_4
#define LIVES_KEY_5 GDK_KEY_5
#define LIVES_KEY_6 GDK_KEY_6
#define LIVES_KEY_7 GDK_KEY_7
#define LIVES_KEY_8 GDK_KEY_8
#define LIVES_KEY_9 GDK_KEY_9
#define LIVES_KEY_0 GDK_KEY_0

#define LIVES_KEY_q GDK_KEY_q

#define LIVES_KEY_F1 GDK_KEY_F1
#define LIVES_KEY_F2 GDK_KEY_F2
#define LIVES_KEY_F3 GDK_KEY_F3
#define LIVES_KEY_F4 GDK_KEY_F4
#define LIVES_KEY_F5 GDK_KEY_F5
#define LIVES_KEY_F6 GDK_KEY_F6
#define LIVES_KEY_F7 GDK_KEY_F7
#define LIVES_KEY_F8 GDK_KEY_F8
#define LIVES_KEY_F9 GDK_KEY_F9
#define LIVES_KEY_F10 GDK_KEY_F10
#define LIVES_KEY_F11 GDK_KEY_F11
#define LIVES_KEY_F12 GDK_KEY_F12

#define LIVES_KEY_Page_Up GDK_KEY_Page_Up
#define LIVES_KEY_Page_Down GDK_KEY_Page_Down

#else
#define LIVES_KEY_Left GDK_Left
#define LIVES_KEY_Right GDK_Right
#define LIVES_KEY_Up GDK_Up
#define LIVES_KEY_Down GDK_Down

#define LIVES_KEY_Space GDK_space
#define LIVES_KEY_BackSpace GDK_BackSpace
#define LIVES_KEY_Return GDK_Return
#define LIVES_KEY_Tab GDK_Tab

#define LIVES_KEY_q GDK_q

#define LIVES_KEY_1 GDK_1
#define LIVES_KEY_2 GDK_2
#define LIVES_KEY_3 GDK_3
#define LIVES_KEY_4 GDK_4
#define LIVES_KEY_5 GDK_5
#define LIVES_KEY_6 GDK_6
#define LIVES_KEY_7 GDK_7
#define LIVES_KEY_8 GDK_8
#define LIVES_KEY_9 GDK_9
#define LIVES_KEY_0 GDK_0

#define LIVES_KEY_q GDK_q

#define LIVES_KEY_F1 GDK_F1
#define LIVES_KEY_F2 GDK_F2
#define LIVES_KEY_F3 GDK_F3
#define LIVES_KEY_F4 GDK_F4
#define LIVES_KEY_F5 GDK_F5
#define LIVES_KEY_F6 GDK_F6
#define LIVES_KEY_F7 GDK_F7
#define LIVES_KEY_F8 GDK_F8
#define LIVES_KEY_F9 GDK_F9
#define LIVES_KEY_F10 GDK_F10
#define LIVES_KEY_F11 GDK_F11
#define LIVES_KEY_F12 GDK_F12

#define LIVES_KEY_Page_Up GDK_Page_Up
#define LIVES_KEY_Page_Down GDK_Page_Down


#endif

// TO BE REMOVED:
void combo_set_popdown_strings (GtkCombo *combo, LiVESList *list);


#endif


#ifdef GUI_QT
typedef QImage                            LiVESPixbuf;
typedef bool                              boolean;
typedef int                               gint;
typedef uchar                             guchar;
typedef (void *)                          gpointer;  
typedef (void *)(LiVESPixbufDestroyNotify(uchar *, gpointer));

// etc.



#define LIVES_INTERP_BEST   Qt::SmoothTransformation
#define LIVES_INTERP_NORMAL Qt::SmoothTransformation
#define LIVES_INTERP_BEST   Qt::FastTransformation


#endif














// basic functions (wrappers for Toolkit functions)

void lives_object_unref(LiVESObjectPtr object);

int lives_pixbuf_get_width(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_height(const LiVESPixbuf *pixbuf);
boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_rowstride(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_n_channels(const LiVESPixbuf *pixbuf);
unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *pixbuf);
const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *pixbuf);
LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height);
LiVESPixbuf *lives_pixbuf_new_from_data (const unsigned char *buf, boolean has_alpha, int width, int height, 
					 int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn, 
					 gpointer destroy_fn_data);

LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error);
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, boolean preserve_aspect_ratio,
						 LiVESError **error);


LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height, 
				       LiVESInterpType interp_type);


LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog);

LiVESWidget *lives_combo_new(void);

void lives_combo_append_text(LiVESCombo *combo, const char *text);
void lives_combo_set_entry_text_column(LiVESCombo *combo, int column);

char *lives_combo_get_active_text(LiVESCombo *combo) WARN_UNUSED;
void lives_combo_set_active_text(LiVESCombo *combo, const char *text);

LiVESWidget *lives_combo_get_entry(LiVESCombo *combo);

void lives_combo_populate(LiVESCombo *combo, LiVESList *list);

boolean lives_toggle_button_get_active(LiVESToggleButton *button);
void lives_toggle_button_set_active(LiVESToggleButton *button, boolean active);

void lives_tooltips_set (LiVESWidget *widget, const char *tip_text);


// compound functions (composed of basic functions)

void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);

LiVESWidget *lives_standard_check_button_new(const char *label, boolean use_mnemonic, LiVESBox *box, const char *tooltip);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *box, const char *tooltips);
LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *box, 
					    const char *tooltip);
LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip);


// util functions



void lives_combo_set_active_string(LiVESCombo *combo, const char *active_str);

int get_box_child_index (LiVESBox *box, LiVESWidget *tchild);

void set_fg_colour(gint red, gint green, gint blue);

boolean label_act_toggle (LiVESWidget *, LiVESEventButton *, LiVESToggleButton *);
boolean widget_act_toggle (LiVESWidget *, LiVESToggleButton *);

void gtk_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);

void adjustment_configure(LiVESAdjustment *adjustment, double value, double lower, double upper, 
			  double step_increment, double page_increment, double page_size);

char *text_view_get_text(LiVESTextView *textview);
void text_view_set_text(LiVESTextView *textview, const char *text);

void lives_set_cursor_style(lives_cursor_t cstyle, LiVESXWindow *window);

void toggle_button_toggle (LiVESToggleButton *tbutton);

void unhide_cursor(LiVESXWindow *window);
void hide_cursor(LiVESXWindow *window);

void get_border_size (LiVESWidget *win, int *bx, int *by);

#endif

