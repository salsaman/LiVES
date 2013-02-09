// widget-helper.h
// LiVES
// (c) G. Finch 2012 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H

#ifndef M_PI
#define M_PI 3.1415926536
#endif

typedef enum {
  LIVES_CURSOR_NORMAL=0,  ///< must be zero
  LIVES_CURSOR_BLOCK,
  LIVES_CURSOR_AUDIO_BLOCK,
  LIVES_CURSOR_BUSY,
  LIVES_CURSOR_FX_BLOCK
} lives_cursor_t;



#define W_PACKING_WIDTH 10 // packing width for widgets with labels
#define W_BORDER_WIDTH 20 // dialog border width


#ifdef PAINTER_CAIRO
typedef cairo_t lives_painter_t;
typedef cairo_surface_t lives_painter_surface_t;

typedef cairo_format_t lives_painter_format_t;

#define LIVES_PAINTER_FORMAT_A1   CAIRO_FORMAT_A1
#define LIVES_PAINTER_FORMAT_A8   CAIRO_FORMAT_A8
#define LIVES_PAINTER_FORMAT_ARGB32 CAIRO_FORMAT_ARGB32


typedef cairo_content_t lives_painter_content_t; // eg. color, alpha, color+alpha

#define LIVES_PAINTER_CONTENT_COLOR CAIRO_CONTENT_COLOR


typedef cairo_operator_t lives_painter_operator_t;

#define LIVES_PAINTER_OPERATOR_DEST_OUT CAIRO_OPERATOR_DEST_OUT


typedef cairo_fill_rule_t lives_painter_fill_rule_t;

#define LIVES_PAINTER_FILL_RULE_WINDING  CAIRO_FILL_RULE_WINDING 
#define LIVES_PAINTER_FILL_RULE_EVEN_ODD CAIRO_FILL_RULE_EVEN_ODD 


#endif


#ifdef GUI_GTK

#define GTK_VERSION_3 GTK_CHECK_VERSION(3,0,0) // set to 2,0,0 for testing

typedef GtkJustification LiVESJustification;

#define LIVES_JUSTIFY_LEFT   GTK_JUSTIFY_LEFT
#define LIVES_JUSTIFY_RIGHT  GTK_JUSTIFY_RIGHT
#define LIVES_JUSTIFY_CENTER GTK_JUSTIFY_CENTER
#define LIVES_JUSTIFY_FILL   GTK_JUSTIFY_RIGHT

typedef GtkOrientation LiVESOrientation;
#define LIVES_ORIENTATION_HORIZONTAL GTK_ORIENTATION_HORIZONTAL
#define LIVES_ORIENTATION_VERTICAL   GTK_ORIENTATION_VERTICAL

typedef GdkEvent                          LiVESEvent;
#if GTK_CHECK_VERSION(3,0,0)
#define GTK_OBJECT(a)                     a
#else
typedef GtkObject                         LiVESObject;
#endif
typedef GtkWidget                         LiVESWidget;
typedef GtkContainer                      LiVESContainer;
typedef GtkBin                            LiVESBin;
typedef GtkDialog                         LiVESDialog;
typedef GtkBox                            LiVESBox;
typedef GtkEntry                          LiVESEntry;
typedef GtkComboBox                       LiVESCombo;
typedef GtkComboBox                       LiVESComboBox;
typedef GtkButton                         LiVESButton;
typedef GtkToggleButton                   LiVESToggleButton;
typedef GtkTextView                       LiVESTextView;
typedef GtkEntry                          LiVESEntry;
typedef GtkRadioButton                    LiVESRadioButton;
typedef GtkScaleButton                    LiVESScaleButton;
typedef GtkLabel                          LiVESLabel;

#if GTK_CHECK_VERSION(3,0,0)
#define LIVES_WIDGET_COLOR_SCALE (1./256.)
typedef GdkRGBA                           LiVESWidgetColor;
typedef GtkStateFlags LiVESWidgetState;
#else
#define LIVES_WIDGET_COLOR_SCALE 256.
typedef GdkColor                          LiVESWidgetColor;
typedef GtkStateType LiVESWidgetState;
#endif

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
#define LIVES_PIXBUF(widget) GDK_PIXBUF(widget)
#define LIVES_WINDOW(widget) GTK_WINDOW(widget)
#define LIVES_XWINDOW(widget) GDK_WINDOW(widget)
#define LIVES_BOX(widget) GTK_BOX(widget)
#define LIVES_ENTRY(widget) GTK_ENTRY(widget)
#define LIVES_CONTAINER(widget) GTK_CONTAINER(widget)
#define LIVES_BIN(widget) GTK_BIN(widget)
#define LIVES_ADJUSTMENT(widget) GTK_ADJUSTMENT(widget)
#define LIVES_DIALOG(widget) GTK_DIALOG(widget)
#define LIVES_COMBO(widget) GTK_COMBO_BOX(widget)
#define LIVES_COMBO_BOX(widget) GTK_COMBO_BOX(widget)
#define LIVES_BUTTON(widget) GTK_BUTTON(widget)
#define LIVES_LABEL(widget) GTK_LABEL(widget)
#define LIVES_RADIO_BUTTON(widget) GTK_RADIO_BUTTON(widget)
#define LIVES_SCALE_BUTTON(widget) GTK_SCALE_BUTTON(widget)
#define LIVES_TOGGLE_BUTTON(widget) GTK_TOGGLE_BUTTON(widget)

#if GTK_VERSION_3
#define LIVES_RULER(widget) GTK_SCALE(widget)
#define LIVES_ORIENTABLE(widget) GTK_ORIENTABLE(widget)
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
#define LIVES_KEY_Home GDK_KEY_Home
#define LIVES_KEY_End GDK_KEY_End
#define LIVES_KEY_Slash GDK_KEY_slash
#define LIVES_KEY_Space GDK_KEY_space
#define LIVES_KEY_Plus GDK_KEY_plus
#define LIVES_KEY_Minus GDK_KEY_minus
#define LIVES_KEY_Equal GDK_KEY_equal

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

#define LIVES_KEY_a GDK_KEY_a
#define LIVES_KEY_b GDK_KEY_b
#define LIVES_KEY_c GDK_KEY_c
#define LIVES_KEY_d GDK_KEY_d
#define LIVES_KEY_e GDK_KEY_e
#define LIVES_KEY_f GDK_KEY_f
#define LIVES_KEY_g GDK_KEY_g
#define LIVES_KEY_h GDK_KEY_h
#define LIVES_KEY_i GDK_KEY_i
#define LIVES_KEY_j GDK_KEY_j
#define LIVES_KEY_k GDK_KEY_k
#define LIVES_KEY_l GDK_KEY_l
#define LIVES_KEY_m GDK_KEY_m
#define LIVES_KEY_n GDK_KEY_n
#define LIVES_KEY_o GDK_KEY_o
#define LIVES_KEY_p GDK_KEY_p
#define LIVES_KEY_q GDK_KEY_q
#define LIVES_KEY_r GDK_KEY_r
#define LIVES_KEY_s GDK_KEY_s
#define LIVES_KEY_t GDK_KEY_t
#define LIVES_KEY_u GDK_KEY_u
#define LIVES_KEY_v GDK_KEY_v
#define LIVES_KEY_w GDK_KEY_w
#define LIVES_KEY_x GDK_KEY_x
#define LIVES_KEY_y GDK_KEY_y
#define LIVES_KEY_z GDK_KEY_z

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

#define LIVES_KEY_Escape GDK_KEY_Escape

#else
#define LIVES_KEY_Left GDK_Left
#define LIVES_KEY_Right GDK_Right
#define LIVES_KEY_Up GDK_Up
#define LIVES_KEY_Down GDK_Down

#define LIVES_KEY_Space GDK_space
#define LIVES_KEY_BackSpace GDK_BackSpace
#define LIVES_KEY_Return GDK_Return
#define LIVES_KEY_Tab GDK_Tab
#define LIVES_KEY_Home GDK_Home
#define LIVES_KEY_End GDK_End
#define LIVES_KEY_Slash GDK_slash
#define LIVES_KEY_Space GDK_space
#define LIVES_KEY_Plus GDK_plus
#define LIVES_KEY_Minus GDK_minus
#define LIVES_KEY_Equal GDK_equal

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

#define LIVES_KEY_a GDK_a
#define LIVES_KEY_b GDK_b
#define LIVES_KEY_c GDK_c
#define LIVES_KEY_d GDK_d
#define LIVES_KEY_e GDK_e
#define LIVES_KEY_f GDK_f
#define LIVES_KEY_g GDK_g
#define LIVES_KEY_h GDK_h
#define LIVES_KEY_i GDK_i
#define LIVES_KEY_j GDK_j
#define LIVES_KEY_k GDK_k
#define LIVES_KEY_l GDK_l
#define LIVES_KEY_m GDK_m
#define LIVES_KEY_n GDK_n
#define LIVES_KEY_o GDK_o
#define LIVES_KEY_p GDK_p
#define LIVES_KEY_q GDK_q
#define LIVES_KEY_r GDK_r
#define LIVES_KEY_s GDK_s
#define LIVES_KEY_t GDK_t
#define LIVES_KEY_u GDK_u
#define LIVES_KEY_v GDK_v
#define LIVES_KEY_w GDK_w
#define LIVES_KEY_x GDK_x
#define LIVES_KEY_y GDK_y
#define LIVES_KEY_z GDK_z

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

#define LIVES_KEY_Escape GDK_Escape

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

// lives_painter_functions

lives_painter_t *lives_painter_create(lives_painter_surface_t *target);
lives_painter_t *lives_painter_create_from_widget(LiVESWidget *);
void lives_painter_set_source_pixbuf (lives_painter_t *, const LiVESPixbuf *, double pixbuf_x, double pixbuf_y);
lives_painter_surface_t *lives_painter_image_surface_create(lives_painter_format_t format, int width, int height);
lives_painter_surface_t *lives_painter_surface_create_similar (lives_painter_surface_t *, 
							       lives_painter_content_t, int width, int height);
lives_painter_surface_t *lives_painter_surface_create_similar_image(lives_painter_surface_t *other, 
								    lives_painter_format_t format, int width, int height);
lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data, lives_painter_format_t, 
								     int width, int height, int stride);

void lives_painter_surface_flush(lives_painter_surface_t *);

void lives_painter_destroy(lives_painter_t *);
void lives_painter_surface_destroy(lives_painter_surface_t *);

void lives_painter_new_path(lives_painter_t *);

void lives_painter_paint(lives_painter_t *);
void lives_painter_fill(lives_painter_t *);
void lives_painter_stroke(lives_painter_t *);
void lives_painter_clip(lives_painter_t *);

void lives_painter_set_source_rgb(lives_painter_t *, double red, double green, double blue);
void lives_painter_set_source_rgba(lives_painter_t *, double red, double green, double blue, double alpha);

void lives_painter_set_line_width(lives_painter_t *, double width);

void lives_painter_rectangle(lives_painter_t *, double x, double y, double width, double height);
void lives_painter_arc(lives_painter_t *, double xc, double yc, double radius, double angle1, double angle2);
void lives_painter_line_to(lives_painter_t *, double x, double y);
void lives_painter_move_to(lives_painter_t *, double x, double y);

void lives_painter_set_operator(lives_painter_t *, lives_painter_operator_t);

void lives_painter_set_fill_rule(lives_painter_t *, lives_painter_fill_rule_t);


lives_painter_surface_t *lives_painter_get_target(lives_painter_t *);
int lives_painter_format_stride_for_width(lives_painter_format_t, int width);

uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *);
int lives_painter_image_surface_get_width(lives_painter_surface_t *);
int lives_painter_image_surface_get_height(lives_painter_surface_t *);
int lives_painter_image_surface_get_stride(lives_painter_surface_t *);
lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *);




// utils

boolean return_true (LiVESWidget *, LiVESEvent *, LiVESObjectPtr);


// object funcs.

void lives_object_unref(LiVESObjectPtr);


// lives_pixbuf functions

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

// basic widget fns

void lives_widget_set_bg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
void lives_widget_set_fg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
void lives_widget_set_text_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
void lives_widget_set_base_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);

void lives_widget_get_fg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);
void lives_widget_get_bg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);

boolean lives_color_parse(const char *spec, LiVESWidgetColor *);

LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1orNULL, const LiVESWidgetColor *c2);


LiVESWidget *lives_dialog_get_content_area(LiVESDialog *);
LiVESWidget *lives_dialog_get_action_area(LiVESDialog *);

LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper, 
						   double step_increment, double page_increment, double page_size);
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
boolean lives_widget_is_realized(LiVESWidget *);


double lives_adjustment_get_upper(LiVESAdjustment *);
double lives_adjustment_get_lower(LiVESAdjustment *);
double lives_adjustment_get_page_size(LiVESAdjustment *);

void lives_adjustment_set_upper(LiVESAdjustment *, double upper);
void lives_adjustment_set_lower(LiVESAdjustment *, double lower);
void lives_adjustment_set_page_size(LiVESAdjustment *, double page_size);

const char *lives_label_get_text(LiVESLabel *);

void lives_entry_set_editable(LiVESEntry *, boolean editable);

void lives_scale_button_set_orientation(LiVESScaleButton *, LiVESOrientation orientation);

void lives_widget_clear_area(LiVESWidget *, int x, int y, int width, int height);

// optional

void lives_dialog_set_has_separator(LiVESDialog *dialog, boolean has);


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

LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, const char *txt, int dispwidth, int maxchars, LiVESBox *, 
				      const char *tooltip);

LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons);

LiVESWidget *lives_standard_hruler_new(void);




// util functions

void lives_widget_get_fg_color(LiVESWidget *, LiVESWidgetColor *);
void lives_widget_get_bg_color(LiVESWidget *, LiVESWidgetColor *);

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

void add_hsep_to_box (LiVESBox *, boolean expand);

void add_fill_to_box (LiVESBox *box);


#define LIVES_JUSTIFY_DEFAULT LIVES_JUSTIFY_LEFT

typedef struct {
  boolean no_gui; // show nothing !
  boolean swap_label; // swap label/widget position
  boolean pack_end;
  boolean line_wrap; // line wrapping for labels
  boolean non_modal; // non-modal for dialogs
  LiVESJustification justify; // justify for labels
} widget_opts_t;


widget_opts_t widget_opts;

#endif

