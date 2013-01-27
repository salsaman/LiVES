// widget-helper.h
// LiVES
// (c) G. Finch 2012 - 2013 <salsaman@gmail.com>
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



#define W_PACKING_WIDTH 10 // packing width for widgets with labels
#define W_BORDER_WIDTH 20 // dialog border width


#ifdef GUI_GTK

#define GTK_VERSION_3 GTK_CHECK_VERSION(3,0,0) // set to 2,0,0 for testing

typedef GtkJustification LiVESJustification;

#define LIVES_JUSTIFY_LEFT   GTK_JUSTIFY_LEFT
#define LIVES_JUSTIFY_RIGHT  GTK_JUSTIFY_RIGHT
#define LIVES_JUSTIFY_CENTER GTK_JUSTIFY_CENTER
#define LIVES_JUSTIFY_FILL   GTK_JUSTIFY_RIGHT


typedef GdkEvent                          LiVESEvent;
typedef GtkObject                         LiVESObject;
typedef GtkWidget                         LiVESWidget;
typedef GtkContainer                      LiVESContainer;
typedef GtkBin                            LiVESBin;
typedef GtkDialog                         LiVESDialog;
typedef GtkBox                            LiVESBox;
typedef GtkComboBox                       LiVESCombo;
typedef GtkComboBox                       LiVESComboBox;
typedef GtkButton                         LiVESButton;
typedef GtkToggleButton                   LiVESToggleButton;
typedef GtkTextView                       LiVESTextView;
typedef GtkEntry                          LiVESEntry;
typedef GtkRadioButton                    LiVESRadioButton;

#if GTK_VERSION_3
typedef GtkScale                          LiVESRuler;
#else
typedef GtkRuler                          LiVESRuler;
#endif

typedef GtkRange                          LiVESRange;

typedef GtkAdjustment                     LiVESAdjustment;

typedef GdkPixbuf                         LiVESPixbuf;

typedef GdkWindow                         LiVESXWindow;

typedef GdkEventButton                    LiVESEventButton;

typedef GError                            LiVESError;

#ifndef IS_MINGW
typedef gboolean                          boolean;
#endif

typedef GList                             LiVESList;
typedef GSList                            LiVESSList;


typedef GdkPixbufDestroyNotify            LiVESPixbufDestroyNotify;

typedef GdkInterpType                     LiVESInterpType;

typedef gpointer                          LiVESObjectPtr;

#define LIVES_WIDGET(widget) GTK_WIDGET(widget)
#define LIVES_WINDOW(widget) GTK_WINDOW(widget)
#define LIVES_XWINDOW(widget) GDK_WINDOW(widget)
#define LIVES_BOX(widget) GTK_BOX(widget)
#define LIVES_CONTAINER(widget) GTK_CONTAINER(widget)
#define LIVES_BIN(widget) GTK_BIN(widget)
#define LIVES_DIALOG(widget) GTK_DIALOG(widget)
#define LIVES_COMBO(widget) GTK_COMBO_BOX(widget)
#define LIVES_COMBO_BOX(widget) GTK_COMBO_BOX(widget)
#define LIVES_BUTTON(widget) GTK_BUTTON(widget)
#define LIVES_RADIO_BUTTON(widget) GTK_RADIO_BUTTON(widget)
#define LIVES_TOGGLE_BUTTON(widget) GTK_TOGGLE_BUTTON(widget)

#if GTK_VERSION_3
#define LIVES_RULER(widget) GTK_SCALE(widget)
#else
#define LIVES_RULER(widget) GTK_RULER(widget)
#endif

#define LIVES_RANGE(widget) GTK_RANGE(widget)


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

boolean return_true (LiVESWidget *, LiVESEvent *, LiVESObjectPtr);

void lives_object_unref(LiVESObjectPtr);

int lives_pixbuf_get_width(const LiVESPixbuf *);
int lives_pixbuf_get_height(const LiVESPixbuf *);
boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *);
int lives_pixbuf_get_rowstride(const LiVESPixbuf *);
int lives_pixbuf_get_n_channels(const LiVESPixbuf *);
unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *);
const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height);
LiVESPixbuf *lives_pixbuf_new_from_data (const unsigned char *buf, boolean has_alpha, int width, int height, 
					 int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn, 
					 gpointer destroy_fn_data);

LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error);
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, boolean preserve_aspect_ratio,
						 LiVESError **error);


LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height, 
				       LiVESInterpType interp_type);


LiVESWidget *lives_dialog_get_content_area(LiVESDialog *);
LiVESWidget *lives_dialog_get_action_area(LiVESDialog *);

LiVESWidget *lives_combo_new(void);

void lives_combo_append_text(LiVESCombo *, const char *text);
void lives_combo_set_entry_text_column(LiVESCombo *, int column);

char *lives_combo_get_active_text(LiVESCombo *) WARN_UNUSED;
void lives_combo_set_active_text(LiVESCombo *, const char *text);

void lives_combo_set_active_string(LiVESCombo *, const char *active_str);

LiVESWidget *lives_combo_get_entry(LiVESCombo *);

void lives_combo_populate(LiVESCombo *, LiVESList *list);

boolean lives_toggle_button_get_active(LiVESToggleButton *);
void lives_toggle_button_set_active(LiVESToggleButton *, boolean active);

void lives_tooltips_set (LiVESWidget *, const char *tip_text);

LiVESSList *lives_radio_button_get_group(LiVESRadioButton *);

LiVESWidget *lives_widget_get_parent(LiVESWidget *);

LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *);

void lives_widget_set_can_focus(LiVESWidget *, boolean state);
void lives_widget_set_can_default(LiVESWidget *, boolean state);

void lives_container_remove(LiVESContainer *, LiVESWidget *);

double lives_ruler_get_value(LiVESRuler *);
double lives_ruler_set_value(LiVESRuler *, double value);

void lives_ruler_set_range(LiVESRuler *, double lower, double upper, double position, double max_size);
double lives_ruler_set_upper(LiVESRuler *, double value);
double lives_ruler_set_lower(LiVESRuler *, double value);

int lives_widget_get_allocation_x(LiVESWidget *);
int lives_widget_get_allocation_y(LiVESWidget *);
int lives_widget_get_allocation_width(LiVESWidget *);
int lives_widget_get_allocation_height(LiVESWidget *);

LiVESWidget *lives_bin_get_child(LiVESBin *);

boolean lives_widget_is_sensitive(LiVESWidget *);
boolean lives_widget_is_visible(LiVESWidget *);


// compound functions (composed of basic functions)

LiVESWidget *lives_standard_label_new(const char *text);
LiVESWidget *lives_standard_label_new_with_mnemonic(const char *text, LiVESWidget *mnemonic_widget);

LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, const char *tooltip);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *, const char *tooltip);
LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *, 
					    const char *tooltip);
LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *, 
				       const char *tooltip);

LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, char *txt, int dispwidth, int maxchars, LiVESBox *, 
				      const char *tooltip);

LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons);

LiVESWidget *lives_standard_hruler_new(void);




// util functions

LiVESWidget *lives_entry_new_with_max_length(int max);

void lives_widget_unparent(LiVESWidget *);

void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);


int get_box_child_index (LiVESBox *, LiVESWidget *child);

boolean label_act_toggle (LiVESWidget *, LiVESEventButton *, LiVESToggleButton *);
boolean widget_act_toggle (LiVESWidget *, LiVESToggleButton *);

void gtk_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);

void adjustment_configure(LiVESAdjustment *, double value, double lower, double upper, 
			  double step_increment, double page_increment, double page_size);

char *text_view_get_text(LiVESTextView *);
void text_view_set_text(LiVESTextView *, const char *text);

void lives_set_cursor_style(lives_cursor_t cstyle, LiVESXWindow *);

void toggle_button_toggle (LiVESToggleButton *);

void unhide_cursor(LiVESXWindow *);
void hide_cursor(LiVESXWindow *);

void get_border_size (LiVESWidget *win, int *bx, int *by);

void lives_widget_set_can_focus_and_default(LiVESWidget *);

void lives_general_button_clicked (LiVESButton *, LiVESObjectPtr data_to_free);

boolean lives_general_delete_event(LiVESWidget *, LiVESEvent *delevent, LiVESObjectPtr data_to_free);


#define LIVES_JUSTIFY_DEFAULT LIVES_JUSTIFY_LEFT

typedef struct {
  boolean swap_label; // swap label/widget position
  boolean pack_end;
  LiVESJustification justify;
} widget_opts_t;

widget_opts_t widget_opts;

#endif

