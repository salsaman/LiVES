// widget-helper.h
// LiVES
// (c) G. Finch 2012 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H

#ifndef IS_SOLARIS
#define WIDGET_HELPER_GLOBAL_INLINE inline
#define WIDGET_HELPER_LOCAL_INLINE static inline
#else
#define WIDGET_HELPER_GLOBAL_INLINE
#define WIDGET_HELPER_LOCAL_INLINE
#endif

#ifndef M_PI
#define M_PI 3.1415926536
#endif

#define LIVES_HAS_GRID_WIDGET 0
#define LIVES_HAS_SWITCH_WIDGET 0
#define LIVES_HAS_IMAGE_MENU_ITEM 0
#define LIVES_HAS_DEVICE_MANAGER 0

#define MIN_SPINBUTTON_SIZE 6 ///< min digits for spinbuttons

#define BUTTON_RAD 5. ///< button border radius

#define LONGEST_ENTRY_WIDTH ((int)(150. * widget_opts.scaleW))
#define LONG_ENTRY_WIDTH ((int)(100. * widget_opts.scaleW))
#define MEDIUM_ENTRY_WIDTH ((int)(60. * widget_opts.scaleW))
#define SHORT_ENTRY_WIDTH ((int)(40. * widget_opts.scaleW))
#define SHORTER_ENTRY_WIDTH (MEDIUM_ENTRY_WIDTH >> 1)

typedef enum {
  LIVES_DISPLAY_TYPE_UNKNOWN = 0,
  LIVES_DISPLAY_TYPE_X11,
  LIVES_DISPLAY_TYPE_WIN32,
  LIVES_DISPLAY_TYPE_WAYLAND
} lives_display_t;

typedef struct {
  int x, y;
  int width, height;
} lives_rect_t;

// values below are multiplied by scale (unless value is -1)
#define W_CSS_MIN_WIDTH -1
#define W_CSS_MIN_HEIGHT 18
#define W_PACKING_WIDTH  10 // packing width for widgets with labels
#define W_PACKING_HEIGHT 6 // packing height for widgets
#define W_BORDER_WIDTH   10 // default border width
#define W_FILL_LENGTH    (W_PACKING_WIDTH * 4) // default extra fill size

#if defined (GUI_GTK) || defined (LIVES_PAINTER_IS_CAIRO)
#include "widget-helper-gtk.h"
#endif

/////////////// GUI threading parts ////////////////

typedef weed_plantptr_t lives_proc_thread_t;

void unlock_lpt(lives_proc_thread_t);

// fg service calls //

void fg_service_call(lives_proc_thread_t, void *retval);

boolean fg_service_fulfill(void);
boolean fg_service_fulfill_cb(void *dummy);
boolean fg_service_ready_cb(void *dummy);

void fg_service_wake(void);

void fg_stack_wait(void);

boolean set_gui_loop_tight(boolean val);

////////////////////////////////////////

// basic functions (wrappers for Toolkit functions)

#ifdef LIVES_LINGO_IS_PANGO
// pango stuff. I suppose it should be here on the offchance that it might one day be used with a non-gtk+ toolkit...
#define lives_text_strip_markup(text) pango_text_strip_markup(text)

typedef PangoLayout LingoLayout;
typedef PangoContext LingoContext;
typedef PangoWrapMode LingoWrapMode;
typedef PangoEllipsizeMode LingoEllipsizeMode;
typedef PangoFontDescription LingoFontDesc;
#define lingo_layout_set_alignment(a, b) pango_layout_set_alignment(a, b)

#define LINGO_ALIGN_LEFT PANGO_ALIGN_LEFT
#define LINGO_ALIGN_RIGHT PANGO_ALIGN_RIGHT
#define LINGO_ALIGN_CENTER PANGO_ALIGN_CENTER

#define LINGO_WRAP_WORD PANGO_WRAP_WORD
#define LINGO_WRAP_CHAR PANGO_WRAP_CHAR
#define LINGO_WRAP_WORD_CHAR PANGO_WRAP_WORD_CHAR

#define LINGO_ELLIPSIZE_NONE PANGO_ELLIPSIZE_NONE
#define LINGO_ELLIPSIZE_START PANGO_ELLIPSIZE_START
#define LINGO_ELLIPSIZE_END PANGO_ELLIPSIZE_END
#define LINGO_ELLIPSIZE_MIDDLE PANGO_ELLIPSIZE_MIDDLE

#define lingo_layout_set_text(a, b, c) pango_layout_set_text(a, b, c)
#define lingo_layout_set_markup_with_accel(a, b, c, d, e) \
  pango_layout_set_markup_with_accel(a, b, c, d, e)
#ifdef LIVES_PAINTER_IS_CAIRO
#define LIVES_PAINTER_COLOR_PALETTE(endian) (endian == LIVES_BIG_ENDIAN ? WEED_PALETTE_ARGB32 \
					     : WEED_PALETTE_BGRA32)
#define lingo_painter_show_layout(a, b) pango_cairo_show_layout(a, b)
#endif
#ifdef GUI_GTK
#define lives_widget_create_lingo_context(a) gtk_widget_create_pango_context(a)
#endif
#define lingo_layout_get_size(a, b, c) pango_layout_get_size(a, b, c)
#define lingo_layout_new(a) pango_layout_new(a)
#define lingo_layout_set_markup(a, b, c) pango_layout_set_markup(a, b, c)
#define lingo_layout_set_width(a, b) pango_layout_set_width(a, b)
#define lingo_layout_set_height(a, b) pango_layout_set_height(a, b)
#define lingo_layout_set_fontdesc(a, b) pango_layout_set_font_description(a, b)

#define lingo_fontdesc_new(a) pango_font_description_new()
#define lingo_fontdesc_get_size(a) pango_font_description_get_size(a)
#define lingo_fontdesc_size_scaled(a) pango_font_description_get_size_is_absolute(a)
#define lingo_fontdesc_set_size(a, b) pango_font_description_set_size(a, b)
#define lingo_fontdesc_set_absolute_size(a, b) pango_font_description_set_absolute_size(a, b)
#define lingo_fontdesc_free(a) pango_font_description_free(a)
#define lingo_fontdesc_get_fam(a) pango_font_description_get_family(a)
#define lingo_fontdesc_set_fam(a, b) pango_font_description_set_family(a, b)
#define lingo_fontdesc_get_stretch(a) pango_font_description_get_stretch(a)
#define lingo_fontdesc_set_stretch(a, b) pango_font_description_set_stretch(a, b)
#define lingo_fontdesc_get_style(a) pango_font_description_get_style(a)
#define lingo_fontdesc_set_style(a, b) pango_font_description_set_style(a, b)
#define lingo_fontdesc_get_weight(a) pango_font_description_get_weight(a)
#define lingo_fontdesc_set_weight(a, b) pango_font_description_set_weight(a, b)
#define lingo_fontdesc_from_string(a) pango_font_description_from_string(a)
#define lingo_fontdesc_to_string(a) pango_font_description_to_string(a)

#define LINGO_IS_LAYOUT(a) PANGO_IS_LAYOUT(a)
#define LINGO_IS_CONTEXT(a) PANGO_IS_CONTEXT(a)

#define LINGO_SCALE PANGO_SCALE
#endif

typedef LingoEllipsizeMode LiVESEllipsizeMode;
#define LIVES_ELLIPSIZE_NONE LINGO_ELLIPSIZE_NONE
#define LIVES_ELLIPSIZE_START LINGO_ELLIPSIZE_START
#define LIVES_ELLIPSIZE_MIDDLE LINGO_ELLIPSIZE_MIDDLE
#define LIVES_ELLIPSIZE_END LINGO_ELLIPSIZE_END

#ifdef LIVES_PAINTER_IS_CAIRO
// ...likewise with cairo
#ifndef GUI_GTK
#include <cairo/cairo.h>
#endif

typedef cairo_t lives_painter_t;
typedef cairo_surface_t lives_painter_surface_t;

struct pbs_struct {
  lives_painter_surface_t **surfp;
  LiVESWidget *widget;
  const char *key;
};

boolean lives_painter_surface_destroy(lives_painter_surface_t *);
lives_painter_surface_t *lives_painter_surface_reference(lives_painter_surface_t *);

typedef cairo_format_t lives_painter_format_t;

#define LIVES_PAINTER_FORMAT_A1   CAIRO_FORMAT_A1
#define LIVES_PAINTER_FORMAT_A8   CAIRO_FORMAT_A8
#define LIVES_PAINTER_FORMAT_RGB24 CAIRO_FORMAT_RGB24
#define LIVES_PAINTER_FORMAT_ARGB32 CAIRO_FORMAT_ARGB32

typedef cairo_content_t lives_painter_content_t; // eg. color, alpha, color+alpha

#define LIVES_PAINTER_CONTENT_COLOR CAIRO_CONTENT_COLOR

typedef cairo_operator_t lives_painter_operator_t;

#define LIVES_PAINTER_OPERATOR_UNKNOWN CAIRO_OPERATOR_OVER
#define LIVES_PAINTER_OPERATOR_DEFAULT CAIRO_OPERATOR_OVER

#define LIVES_PAINTER_OPERATOR_DEST_OUT CAIRO_OPERATOR_DEST_OUT
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 10, 0)
#define LIVES_PAINTER_OPERATOR_DIFFERENCE CAIRO_OPERATOR_OVER
#define LIVES_PAINTER_OPERATOR_OVERLAY CAIRO_OPERATOR_OVER
#else
#define LIVES_PAINTER_OPERATOR_DIFFERENCE CAIRO_OPERATOR_DIFFERENCE
#define LIVES_PAINTER_OPERATOR_OVERLAY CAIRO_OPERATOR_OVERLAY
#endif

typedef cairo_fill_rule_t lives_painter_fill_rule_t;

#define LIVES_PAINTER_FILL_RULE_WINDING  CAIRO_FILL_RULE_WINDING
#define LIVES_PAINTER_FILL_RULE_EVEN_ODD CAIRO_FILL_RULE_EVEN_ODD

#endif

// lives_painter_functions

lives_painter_t *lives_painter_create_from_surface(lives_painter_surface_t *target);

lives_painter_t *lives_painter_create_from_widget(LiVESWidget *);
boolean lives_painter_remerge(lives_painter_t *);

boolean lives_painter_set_source_pixbuf(lives_painter_t *, const LiVESPixbuf *, double pixbuf_x, double pixbuf_y);
boolean lives_painter_set_source_surface(lives_painter_t *, lives_painter_surface_t *, double x, double y);

lives_painter_surface_t *lives_xwindow_create_similar_surface(LiVESXWindow *window,
    lives_painter_content_t cont,
    int width, int height);
lives_painter_surface_t *lives_widget_create_painter_surface(LiVESWidget *);
lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data, lives_painter_format_t,
    int width, int height, int stride);

boolean lives_painter_surface_flush(lives_painter_surface_t *);
boolean lives_painter_surface_mark_dirty(lives_painter_surface_t *);

boolean lives_painter_destroy(lives_painter_t *);

boolean lives_painter_new_path(lives_painter_t *);

boolean lives_painter_paint(lives_painter_t *);

boolean lives_painter_fill(lives_painter_t *);
boolean lives_painter_stroke(lives_painter_t *);
boolean lives_painter_clip(lives_painter_t *);

boolean lives_painter_render_background(LiVESWidget *, lives_painter_t *, double x, double y, double width, double height);

boolean lives_painter_set_source_rgb(lives_painter_t *, double red, double green, double blue);
boolean lives_painter_set_source_rgba(lives_painter_t *, double red, double green, double blue, double alpha);

boolean lives_painter_set_line_width(lives_painter_t *, double width);

boolean lives_painter_translate(lives_painter_t *, double x, double y);

boolean lives_painter_rectangle(lives_painter_t *, double x, double y, double width, double height);
boolean lives_painter_arc(lives_painter_t *, double xc, double yc, double radius, double angle1, double angle2);
boolean lives_painter_line_to(lives_painter_t *, double x, double y);
boolean lives_painter_move_to(lives_painter_t *, double x, double y);
boolean lives_painter_close_path(lives_painter_t *);

boolean lives_painter_set_operator(lives_painter_t *, lives_painter_operator_t);

boolean lives_painter_set_fill_rule(lives_painter_t *, lives_painter_fill_rule_t);

lives_painter_surface_t *lives_painter_get_target(lives_painter_t *);
int lives_painter_format_stride_for_width(lives_painter_format_t, int width);

uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *);
int lives_painter_image_surface_get_width(lives_painter_surface_t *);
int lives_painter_image_surface_get_height(lives_painter_surface_t *);
int lives_painter_image_surface_get_stride(lives_painter_surface_t *);
lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *);

// utils
LiVESAlign lives_justify_to_align(LiVESJustification justification);

LiVESScrollDirection lives_get_scroll_direction(LiVESXEventScroll *event);

boolean lives_has_icon(LiVESIconTheme *, const char *stock_id, LiVESIconSize size);

const char *lives_get_stock_icon_alt(int alt_stock_id);
void widget_helper_set_stock_icon_alts(LiVESIconTheme *);
const char *widget_helper_suggest_icons(const char *part, int idx);

boolean widget_helper_init(void);
boolean widget_opts_set_scale(double scale);

lives_colRGBA64_t lives_rgba_col_new(int red, int green, int blue, int alpha);
lives_colRGBA64_t *widget_color_to_lives_rgba(lives_colRGBA64_t *, LiVESWidgetColor *);
LiVESWidgetColor *lives_rgba_to_widget_color(LiVESWidgetColor *, lives_colRGBA64_t *);

lives_colRGBA64_t *lives_painter_set_source_rgb_from_lives_rgba(lives_painter_t *, lives_colRGBA64_t *);
lives_colRGB48_t *lives_painter_set_source_rgb_from_lives_rgb(lives_painter_t *, lives_colRGB48_t *);
LiVESWidgetColor *lives_painter_set_source_rgb_from_lives_widget_color(lives_painter_t *, LiVESWidgetColor *);

boolean clear_widget_bg(LiVESWidget *widget, lives_painter_surface_t *);
boolean clear_widget_bg_area(LiVESWidget *widget, lives_painter_surface_t *s,
                             double x, double y, double width, double height);

boolean lives_rgba_equal(lives_colRGBA64_t *col1, lives_colRGBA64_t *col2);
lives_colRGBA64_t *lives_rgba_copy(lives_colRGBA64_t *col1, lives_colRGBA64_t *col2);

// object funcs.

boolean lives_widget_object_ref(livespointer); ///< increase refcount by one
boolean lives_widget_object_unref(livespointer); ///< decrease refcount by one: if refcount==0, object is destroyed

// remove any "floating" reference and add a new ref
boolean lives_widget_object_ref_sink(livespointer);

// set string data and free it later
void lives_widget_object_set_data_auto(LiVESWidgetObject *, const char *key, livespointer data);

// set plantptr data and weed_plant_free it later
void lives_widget_object_set_data_plantptr(LiVESWidgetObject *obj, const char *key,
    weed_plantptr_t plant);

// set list and free it later (but not the list data)
void lives_widget_object_set_data_list(LiVESWidgetObject *, const char *key, LiVESList *list);

// set widget object and unref it later
void lives_widget_object_set_data_widget_object(LiVESWidgetObject *, const char *key, livespointer other);
// lives_pixbuf functions

// set widget object and nullify if target destroyed
void lives_widget_object_set_data_destroyable(LiVESWidgetObject *, const char *key,
    LiVESWidgetObject *widget);

#define GET_INT_DATA(widg, key) (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT((widg)), (key))))
#define SET_INT_DATA(widg, key, val) (lives_widget_object_set_data(LIVES_WIDGET_OBJECT((widg)), ((key)), LIVES_INT_TO_POINTER((val))))

#define GET_VOIDP_DATA(widg, key) lives_widget_object_get_data(LIVES_WIDGET_OBJECT((widg)), (key))
#define SET_VOIDP_DATA(widg, key, val) lives_widget_object_set_data(LIVES_WIDGET_OBJECT((widg)), ((key)), (val))

int lives_pixbuf_get_width(const LiVESPixbuf *);
int lives_pixbuf_get_height(const LiVESPixbuf *);
boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *);
int lives_pixbuf_get_rowstride(const LiVESPixbuf *);
int lives_pixbuf_get_n_channels(const LiVESPixbuf *);
unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *);
const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height);
LiVESPixbuf *lives_pixbuf_copy(LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new_from_data(const unsigned char *buf, boolean has_alpha, int width, int height,
                                        int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn,
                                        livespointer destroy_fn_data);

LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error);
LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, boolean preserve_aspect_ratio,
    LiVESError **error);

LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height,
                                       LiVESInterpType interp_type);

boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf *src, LiVESPixbuf *dest, float saturation, boolean pixelate);

// basic widget fns

// TODO consider combining  get/set value, get/set label, get/set label widget

#ifdef GUI_GTK

typedef GConnectFlags LiVESConnectFlags;

#define LIVES_CONNECT_AFTER G_CONNECT_AFTER
#define LIVES_CONNECT_SWAPPED G_CONNECT_SWAPPED

unsigned long lives_signal_connect_sync(livespointer instance, const char *detailed_signal, LiVESGuiCallback c_handler,
                                        livespointer data, LiVESConnectFlags flags);

unsigned long lives_signal_connect_async(livespointer instance, const char *detailed_signal, LiVESGuiCallback c_handler,
    livespointer data, LiVESConnectFlags flags);

#define lives_signal_connect(instance, detailed_signal, c_handler, data) \
  lives_signal_connect_async(instance, detailed_signal, c_handler, data, 0)
#define lives_signal_connect_after(instance, detailed_signal, c_handler, data) \
  lives_signal_connect_async(instance, detailed_signal, c_handler, data, LIVES_CONNECT_AFTER)
#define lives_signal_connect_swapped(instance, detailed_signal, c_handler, data) \
  lives_signal_connect_async(instance, detailed_signal, c_handler, data, LIVES_CONNECT_SWAPPED)

#define lives_signal_sync_connect(instance, detailed_signal, c_handler, data) \
  lives_signal_connect_sync(instance, detailed_signal, c_handler, data, 0)
#define lives_signal_sync_connect_after(instance, detailed_signal, c_handler, data) \
  lives_signal_connect_sync(instance, detailed_signal, c_handler, data, LIVES_CONNECT_AFTER)
#define lives_signal_sync_connect_swapped(instance, detailed_signal, c_handler, data) \
  lives_signal_connect_sync(instance, detailed_signal, c_handler, data, LIVES_CONNECT_SWAPPED)

boolean lives_signal_handlers_sync_disconnect_by_func(livespointer instance, LiVESGuiCallback func, livespointer data);
boolean lives_signal_handlers_sync_block_by_func(livespointer instance, LiVESGuiCallback func, livespointer data);
boolean lives_signal_handlers_sync_unblock_by_func(livespointer instance, LiVESGuiCallback func, livespointer data);

boolean lives_signal_handlers_disconnect_by_func(livespointer instance, LiVESGuiCallback func, livespointer data);
boolean lives_signal_handlers_block_by_func(livespointer instance, LiVESGuiCallback func, livespointer data);
boolean lives_signal_handlers_unblock_by_func(livespointer instance, LiVESGuiCallback func, livespointer data);
#else
ulong lives_signal_connect(LiVESWidget *, const char *signal_name, ulong funcptr, livespointer data);
#endif

boolean lives_signal_handler_block(livespointer instance, unsigned long handler_id);
boolean lives_signal_handler_unblock(livespointer instance, unsigned long handler_id);

boolean lives_signal_handler_disconnect(livespointer instance, unsigned long handler_id);
boolean lives_signal_stop_emission_by_name(livespointer instance, const char *detailed_signal);

boolean lives_grab_add(LiVESWidget *);
boolean lives_grab_remove(LiVESWidget *);

#ifdef GUI_GTK
boolean set_css_value_direct(LiVESWidget *, LiVESWidgetState state, const char *selector,
                             const char *detail, const char *value);
void set_css_min_size(LiVESWidget *, int mw, int mh);
#endif

boolean lives_widget_set_sensitive(LiVESWidget *, boolean state);
boolean lives_widget_get_sensitive(LiVESWidget *);

boolean lives_widget_show(LiVESWidget *);
boolean lives_widget_show_now(LiVESWidget *);
boolean lives_widget_show_all(LiVESWidget *);
boolean lives_widget_show_all_from_bg(LiVESWidget *);
boolean lives_widget_hide(LiVESWidget *);

boolean lives_widget_destroy(LiVESWidget *);
boolean lives_widget_realize(LiVESWidget *);

boolean lives_widget_queue_draw(LiVESWidget *);
boolean lives_widget_queue_draw_noblock(LiVESWidget *);
boolean lives_widget_queue_draw_update_noblock(LiVESWidget *);
boolean lives_widget_queue_draw_area(LiVESWidget *, int x, int y, int width, int height);
boolean lives_widget_queue_resize(LiVESWidget *);
boolean lives_widget_set_size_request(LiVESWidget *, int width, int height);
boolean lives_widget_set_minimum_size(LiVESWidget *, int width, int height);
boolean lives_widget_set_maximum_size(LiVESWidget *, int width, int height);
boolean lives_widget_reparent(LiVESWidget *, LiVESWidget *new_parent);

boolean lives_widget_is_ancestor(LiVESWidget *, LiVESWidget *ancestor);

boolean lives_widget_set_app_paintable(LiVESWidget *, boolean paintable);
boolean lives_widget_set_opacity(LiVESWidget *widget, double opacity);

boolean lives_widget_has_focus(LiVESWidget *);
boolean lives_widget_is_focus(LiVESWidget *);
boolean lives_widget_has_default(LiVESWidget *);

boolean lives_widget_set_halign(LiVESWidget *, LiVESAlign align);
boolean lives_widget_set_valign(LiVESWidget *, LiVESAlign align);

LiVESWidget *lives_event_box_new(void);
boolean lives_event_box_set_above_child(LiVESEventBox *, boolean set);

LiVESWidget *lives_label_new(const char *text);

const char *lives_label_get_text(LiVESLabel *);
boolean lives_label_set_text(LiVESLabel *, const char *text);

boolean lives_label_set_markup(LiVESLabel *, const char *markup);

boolean lives_label_set_mnemonic_widget(LiVESLabel *, LiVESWidget *widget);
LiVESWidget *lives_label_get_mnemonic_widget(LiVESLabel *);

boolean lives_label_set_selectable(LiVESLabel *, boolean setting);

boolean lives_label_set_line_wrap(LiVESLabel *, boolean set);
boolean lives_label_set_line_wrap_mode(LiVESLabel *, LingoWrapMode mode);
boolean lives_label_set_lines(LiVESLabel *, int nlines);
boolean lives_label_set_ellipsize(LiVESLabel *, LiVESEllipsizeMode mode);

//////////

LiVESWidget *lives_button_new(void);
LiVESWidget *lives_button_new_with_label(const char *label);

boolean lives_button_set_label(LiVESButton *, const char *label);
const char *lives_button_get_label(LiVESButton *);
boolean lives_button_clicked(LiVESButton *);

boolean lives_button_set_relief(LiVESButton *, LiVESReliefStyle);
boolean lives_button_set_image(LiVESButton *, LiVESWidget *image);
boolean lives_button_set_image_from_stock(LiVESButton *, const char *stock_id);
boolean lives_button_set_focus_on_click(LiVESButton *, boolean focus);
boolean lives_widget_set_focus_on_click(LiVESWidget *, boolean focus);

/////////////////////////////

LiVESWidget *lives_switch_new(void);
boolean lives_switch_set_active(LiVESSwitch *, boolean);
boolean lives_switch_get_active(LiVESSwitch *);

LiVESWidget *lives_spinner_new(void);
boolean lives_spinner_start(LiVESSpinner *);
boolean lives_spinner_stop(LiVESSpinner *);

LiVESWidget *lives_check_button_new(void);
LiVESWidget *lives_check_button_new_with_label(const char *label);

LiVESWidget *lives_radio_button_new(LiVESSList *group);

LiVESWidget *lives_spin_button_new(LiVESAdjustment *, double climb_rate, uint32_t digits);

LiVESWidget *lives_dialog_new(void);
LiVESResponseType lives_dialog_run(LiVESDialog *);
boolean lives_dialog_response(LiVESDialog *, LiVESResponseType response);
LiVESResponseType lives_dialog_get_response_for_widget(LiVESDialog *, LiVESWidget *);
LiVESWidget *lives_dialog_get_widget_for_response(LiVESDialog *, LiVESResponseType response);

LiVESResponseType lives_dialog_get_response(LiVESDialog *);

boolean lives_widget_set_bg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_fg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_text_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_base_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);

boolean lives_widget_set_border_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_outline_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);

boolean lives_widget_set_text_size(LiVESWidget *, LiVESWidgetState state, const char *size);

boolean lives_widget_get_fg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);
boolean lives_widget_get_bg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);

boolean lives_color_parse(const char *spec, LiVESWidgetColor *);

LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1orNULL, const LiVESWidgetColor *c2);
boolean lives_widget_color_equal(LiVESWidgetColor *, const LiVESWidgetColor *);
boolean lives_widget_color_mix(LiVESWidgetColor *c1, const LiVESWidgetColor *c2, float mixval);

LiVESWidget *lives_image_new(void);
LiVESWidget *lives_image_new_from_file(const char *filename);
LiVESWidget *lives_image_new_from_stock(const char *stock_id, LiVESIconSize size);

LiVESWidget *lives_image_find_in_stock(LiVESIconSize size, ...) LIVES_SENTINEL;
LiVESWidget *lives_image_find_in_stock_at_size(int size, ...) LIVES_SENTINEL;

boolean lives_image_set_from_pixbuf(LiVESImage *, LiVESPixbuf *);
LiVESPixbuf *lives_image_get_pixbuf(LiVESImage *);

boolean lives_widget_set_margin_left(LiVESWidget *, int margin);
boolean lives_widget_set_margin_right(LiVESWidget *, int margin);
boolean lives_widget_set_margin_top(LiVESWidget *, int margin);
boolean lives_widget_set_margin_bottom(LiVESWidget *, int margin);

boolean lives_widget_set_margin(LiVESWidget *, int margin);
boolean lives_widget_set_padding(LiVESWidget *, int padding);

LiVESWidget *lives_dialog_get_content_area(LiVESDialog *);
LiVESWidget *lives_dialog_get_action_area(LiVESDialog *);
LiVESWidget *lives_standard_dialog_get_action_area(LiVESDialog *);

boolean lives_dialog_add_action_widget(LiVESDialog *, LiVESWidget *, LiVESResponseType response_id);

LiVESWidget *lives_window_new(LiVESWindowType wintype);
boolean lives_window_set_title(LiVESWindow *, const char *title);
const char *lives_window_get_title(LiVESWindow *);
boolean lives_window_set_transient_for(LiVESWindow *, LiVESWindow *parent);

boolean lives_window_set_modal(LiVESWindow *, boolean modal);
boolean lives_window_set_deletable(LiVESWindow *, boolean deletable);
boolean lives_window_set_resizable(LiVESWindow *, boolean resizable);
boolean lives_window_set_keep_below(LiVESWindow *, boolean keep_below);
boolean lives_window_set_keep_above(LiVESWindow *, boolean keep_below);
boolean lives_window_set_decorated(LiVESWindow *, boolean decorated);

boolean lives_window_set_default_size(LiVESWindow *, int width, int height);

boolean lives_window_set_monitor(LiVESWindow *window, int monnum);

boolean lives_widget_get_position(LiVESWidget *, int *x, int *y);

LiVESWidget *lives_window_get_focus(LiVESWindow *);

boolean lives_window_get_modal(LiVESWindow *);
boolean lives_window_set_modal(LiVESWindow *, boolean modal);

boolean lives_window_move(LiVESWindow *, int x, int y);
boolean lives_window_get_position(LiVESWindow *, int *x, int *y);
boolean lives_window_set_position(LiVESWindow *, LiVESWindowPosition pos);
boolean lives_window_resize(LiVESWindow *, int width, int height);
boolean lives_window_move_resize(LiVESWindow *, int x, int y, int w, int h);
boolean lives_window_present(LiVESWindow *);
boolean lives_window_fullscreen(LiVESWindow *);
boolean lives_window_unfullscreen(LiVESWindow *);
boolean lives_window_maximize(LiVESWindow *);
boolean lives_window_unmaximize(LiVESWindow *);
boolean lives_window_set_hide_titlebar_when_maximized(LiVESWindow *, boolean setting);

boolean lives_window_add_accel_group(LiVESWindow *, LiVESAccelGroup *group);
boolean lives_window_remove_accel_group(LiVESWindow *, LiVESAccelGroup *group);
boolean lives_menu_set_accel_group(LiVESMenu *, LiVESAccelGroup *group);

LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper,
                                      double step_increment, double page_increment, double page_size);

boolean lives_box_reorder_child(LiVESBox *, LiVESWidget *child, int pos);
boolean lives_box_set_homogeneous(LiVESBox *, boolean homogeneous);
boolean lives_box_set_spacing(LiVESBox *, int spacing);
boolean lives_box_set_child_packing(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill,
                                    uint32_t padding, LiVESPackType pack_type);

boolean lives_box_pack_start(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding);
boolean lives_box_pack_end(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding);

LiVESWidget *lives_hbox_new(boolean homogeneous, int spacing);
LiVESWidget *lives_vbox_new(boolean homogeneous, int spacing);

LiVESWidget *lives_hseparator_new(void);
LiVESWidget *lives_vseparator_new(void);

LiVESWidget *lives_hbutton_box_new(void);
LiVESWidget *lives_vbutton_box_new(void);

boolean lives_button_box_set_layout(LiVESButtonBox *, LiVESButtonBoxStyle bstyle);
boolean lives_button_box_set_button_width(LiVESButtonBox *, LiVESWidget *button, int min_width);

boolean lives_button_box_set_child_non_homogeneous(LiVESButtonBox *, LiVESWidget *child, boolean set);

boolean lives_button_set_border_colour(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);
boolean lives_button_center(LiVESWidget *);
boolean lives_button_uncenter(LiVESWidget *, int normal_width);
boolean lives_button_box_make_first(LiVESButtonBox *, LiVESWidget *);
boolean lives_dialog_make_widget_first(LiVESDialog *, LiVESWidget *);

LiVESWidget *lives_standard_spinner_new(boolean start);

LiVESWidget *lives_standard_toolbar_new(void);

LiVESWidget *lives_standard_hscale_new(LiVESAdjustment *);
LiVESWidget *lives_vscale_new(LiVESAdjustment *);

LiVESWidget *lives_standard_header_bar_new(LiVESWindow *toplevel);
boolean lives_header_bar_set_title(LiVESHeaderBar *, const char *title);
boolean lives_header_bar_pack_start(LiVESHeaderBar *, LiVESWidget *);

LiVESWidget *lives_hpaned_new(void);
LiVESWidget *lives_vpaned_new(void);

boolean lives_paned_set_position(LiVESPaned *, int pos);
int lives_paned_get_position(LiVESPaned *);
boolean lives_paned_pack(int where, LiVESPaned *, LiVESWidget *child, boolean resize, boolean shrink);

LiVESWidget *lives_hscrollbar_new(LiVESAdjustment *);
LiVESWidget *lives_vscrollbar_new(LiVESAdjustment *);

LiVESWidget *lives_arrow_new(LiVESArrowType, LiVESShadowType);

LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale);
boolean lives_alignment_set(LiVESWidget *, float xalign, float yalign, float xscale, float yscale);

LiVESWidget *lives_expander_new(const char *label);
LiVESWidget *lives_expander_get_label_widget(LiVESExpander *);
boolean lives_expander_set_use_markup(LiVESExpander *, boolean val);
boolean lives_expander_set_expanded(LiVESExpander *, boolean val);
boolean lives_expander_set_label(LiVESExpander *, const char *text);
boolean lives_expander_get_expanded(LiVESExpander *);
boolean lives_label_set_width_chars(LiVESLabel *, int nchars);
boolean lives_label_set_halignment(LiVESLabel *, float yalign);

LiVESWidget *lives_combo_new(void);
LiVESWidget *lives_combo_new_with_model(LiVESTreeModel *model);
LiVESTreeModel *lives_combo_get_model(LiVESCombo *);
boolean lives_combo_set_model(LiVESCombo *, LiVESTreeModel *);
boolean lives_combo_set_focus_on_click(LiVESCombo *, boolean state);
void lives_combo_popup(LiVESCombo *);
boolean lives_combo_remove_text(LiVESCombo *, const char *text);
boolean lives_combo_remove_all_text(LiVESCombo *);

int lives_combo_get_n_entries(LiVESCombo *);

boolean lives_combo_append_text(LiVESCombo *, const char *text);
boolean lives_combo_prepend_text(LiVESCombo *, const char *text);
boolean lives_combo_set_entry_text_column(LiVESCombo *, int column);

const char *lives_combo_get_active_text(LiVESCombo *) WARN_UNUSED;
boolean lives_combo_set_active_string(LiVESCombo *, const char *active_str);
boolean lives_combo_set_active_index(LiVESCombo *, int index);
int lives_combo_get_active_index(LiVESCombo *);
boolean lives_combo_get_active_iter(LiVESCombo *, LiVESTreeIter *);
boolean lives_combo_set_active_iter(LiVESCombo *, LiVESTreeIter *);

LiVESWidget *lives_combo_get_entry(LiVESCombo *);

boolean lives_combo_populate(LiVESCombo *, LiVESList *list);

LiVESWidget *lives_text_view_new(void);
LiVESWidget *lives_text_view_new_with_buffer(LiVESTextBuffer *);
LiVESTextBuffer *lives_text_view_get_buffer(LiVESTextView *);
boolean lives_text_view_set_editable(LiVESTextView *, boolean setting);
boolean lives_text_view_set_accepts_tab(LiVESTextView *, boolean setting);
boolean lives_text_view_set_cursor_visible(LiVESTextView *, boolean setting);
boolean lives_text_view_set_wrap_mode(LiVESTextView *, LiVESWrapMode wrapmode);
boolean lives_text_view_set_justification(LiVESTextView *, LiVESJustification justify);
boolean lives_text_view_set_top_margin(LiVESTextView *, int margin);
boolean lives_text_view_set_bottom_margin(LiVESTextView *, int margin);

LiVESTextBuffer *lives_text_buffer_new(void);
char *lives_text_buffer_get_text(LiVESTextBuffer *, LiVESTextIter *start,
                                 LiVESTextIter *end, boolean inc_hidden_chars);
char *lives_text_buffer_get_all_text(LiVESTextBuffer *);
boolean lives_text_buffer_set_text(LiVESTextBuffer *, const char *, int len);

boolean lives_text_buffer_insert(LiVESTextBuffer *, LiVESTextIter *, const char *, int len);
boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer *, const char *, int len);

boolean lives_text_buffer_insert_markup(LiVESTextBuffer *, LiVESTextIter *, const char *markup, int len);

boolean lives_text_buffer_get_start_iter(LiVESTextBuffer *, LiVESTextIter *);
boolean lives_text_buffer_get_end_iter(LiVESTextBuffer *, LiVESTextIter *);

boolean lives_text_buffer_place_cursor(LiVESTextBuffer *, LiVESTextIter *);

LiVESTextMark *lives_text_buffer_create_mark(LiVESTextBuffer *, const char *mark_name,
    const LiVESTextIter *where, boolean left_gravity);
boolean lives_text_buffer_delete_mark(LiVESTextBuffer *, LiVESTextMark *);

boolean lives_text_buffer_delete(LiVESTextBuffer *, LiVESTextIter *start, LiVESTextIter *end);

boolean lives_text_buffer_get_iter_at_mark(LiVESTextBuffer *, LiVESTextIter *, LiVESTextMark *);

boolean lives_tree_model_get(LiVESTreeModel *, LiVESTreeIter *, ...);
boolean lives_tree_model_get_iter(LiVESTreeModel *, LiVESTreeIter *, LiVESTreePath *);
boolean lives_tree_model_get_iter_first(LiVESTreeModel *, LiVESTreeIter *);
LiVESTreePath *lives_tree_model_get_path(LiVESTreeModel *, LiVESTreeIter *);
boolean lives_tree_model_iter_children(LiVESTreeModel *, LiVESTreeIter *, LiVESTreeIter *parent);
int lives_tree_model_iter_n_children(LiVESTreeModel *, LiVESTreeIter *);
boolean lives_tree_model_iter_next(LiVESTreeModel *, LiVESTreeIter *);

boolean lives_tree_path_free(LiVESTreePath *);
LiVESTreePath *lives_tree_path_new_from_string(const char *path);
int lives_tree_path_get_depth(LiVESTreePath *);
int *lives_tree_path_get_indices(LiVESTreePath *);

LiVESTreeStore *lives_tree_store_new(int ncols, ...);
boolean lives_tree_store_append(LiVESTreeStore *, LiVESTreeIter *, LiVESTreeIter *parent);
boolean lives_tree_store_prepend(LiVESTreeStore *, LiVESTreeIter *, LiVESTreeIter *parent);
boolean lives_tree_store_set(LiVESTreeStore *, LiVESTreeIter *, ...);

LiVESWidget *lives_tree_view_new(void);
LiVESWidget *lives_tree_view_new_with_model(LiVESTreeModel *);
boolean lives_tree_view_set_model(LiVESTreeView *, LiVESTreeModel *);
LiVESTreeModel *lives_tree_view_get_model(LiVESTreeView *);
int lives_tree_view_append_column(LiVESTreeView *, LiVESTreeViewColumn *);
boolean lives_tree_view_set_headers_visible(LiVESTreeView *, boolean vis);
LiVESAdjustment *lives_tree_view_get_hadjustment(LiVESTreeView *);
LiVESTreeSelection *lives_tree_view_get_selection(LiVESTreeView *);

LiVESTreeViewColumn *lives_tree_view_column_new_with_attributes(const char *title, LiVESCellRenderer *, ...);
boolean lives_tree_view_column_set_sizing(LiVESTreeViewColumn *, LiVESTreeViewColumnSizing type);
boolean lives_tree_view_column_set_fixed_width(LiVESTreeViewColumn *, int fwidth);

boolean lives_tree_selection_get_selected(LiVESTreeSelection *, LiVESTreeModel **, LiVESTreeIter *);
boolean lives_tree_selection_set_mode(LiVESTreeSelection *, LiVESSelectionMode);
boolean lives_tree_selection_select_iter(LiVESTreeSelection *, LiVESTreeIter *);

LiVESListStore *lives_list_store_new(int ncols, ...);
boolean lives_list_store_set(LiVESListStore *, LiVESTreeIter *, ...);
boolean lives_list_store_insert(LiVESListStore *, LiVESTreeIter *, int position);

boolean lives_list_store_append(LiVESListStore *lstore, LiVESTreeIter *liter);
boolean lives_list_store_prepend(LiVESListStore *lstore, LiVESTreeIter *liter);

LiVESCellRenderer *lives_cell_renderer_text_new(void);
LiVESCellRenderer *lives_cell_renderer_spin_new(void);
LiVESCellRenderer *lives_cell_renderer_toggle_new(void);
LiVESCellRenderer *lives_cell_renderer_pixbuf_new(void);

pthread_mutex_t *lives_widget_get_mutex(LiVESWidget *);

LiVESWidget *lives_drawing_area_new(void);

int lives_event_get_time(LiVESXEvent *);

boolean lives_toggle_button_get_active(LiVESToggleButton *);
boolean lives_toggle_button_get_inactive(LiVESToggleButton *);
boolean lives_toggle_button_set_active(LiVESToggleButton *, boolean active);
boolean lives_toggle_button_set_mode(LiVESToggleButton *, boolean drawind);
boolean lives_toggle_button_toggle(LiVESToggleButton *);

LiVESWidget *lives_toggle_tool_button_new(void);
boolean lives_toggle_tool_button_get_active(LiVESToggleToolButton *);
boolean lives_toggle_tool_button_set_active(LiVESToggleToolButton *, boolean active);
boolean lives_toggle_tool_button_toggle(LiVESToggleToolButton *);

void entry_text_copy(LiVESEntry *, LiVESEntry *);

int lives_utf8_strcmpfunc(livesconstpointer, livesconstpointer, livespointer fwd);

LiVESList *add_sorted_list_to_menu(LiVESMenu *, LiVESList *);

LiVESSList *lives_radio_button_get_group(LiVESRadioButton *);
LiVESSList *lives_radio_menu_item_get_group(LiVESRadioMenuItem *);

LiVESWidget *lives_widget_get_parent(LiVESWidget *);
LiVESWidget *lives_widget_get_toplevel(LiVESWidget *);

LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *);
boolean lives_xwindow_set_keep_above(LiVESXWindow *, boolean setting);

boolean lives_widget_set_can_focus(LiVESWidget *, boolean state);
boolean lives_widget_set_can_default(LiVESWidget *, boolean state);
boolean lives_widget_set_can_focus_and_default(LiVESWidget *);

boolean lives_widget_add_events(LiVESWidget *, int events);
boolean lives_widget_set_events(LiVESWidget *, int events);
boolean lives_widget_remove_accelerator(LiVESWidget *, LiVESAccelGroup *, uint32_t accel_key, LiVESXModifierType accel_mods);
boolean lives_widget_get_preferred_size(LiVESWidget *, LiVESRequisition *min_size, LiVESRequisition *nat_size);

boolean lives_widget_set_no_show_all(LiVESWidget *, boolean set);
boolean lives_widget_get_no_show_all(LiVESWidget *);

boolean lives_container_remove(LiVESContainer *, LiVESWidget *);
boolean lives_container_add(LiVESContainer *, LiVESWidget *);
boolean lives_container_set_border_width(LiVESContainer *, uint32_t width);

boolean lives_container_foreach(LiVESContainer *, LiVESWidgetCallback callback, livespointer cb_data);
boolean lives_container_foreach_int(LiVESContainer *, LiVESWidgetCallback callback, int cb_data);
LiVESList *lives_container_get_children(LiVESContainer *);
boolean lives_container_set_focus_child(LiVESContainer *, LiVESWidget *child);
LiVESWidget *lives_container_get_focus_child(LiVESContainer *);

LiVESWidget *lives_progress_bar_new(void);
boolean lives_progress_bar_set_fraction(LiVESProgressBar *, double fraction);
boolean lives_progress_bar_set_pulse_step(LiVESProgressBar *, double fraction);
boolean lives_progress_bar_pulse(LiVESProgressBar *);

double lives_spin_button_get_value(LiVESSpinButton *);
int lives_spin_button_get_value_as_int(LiVESSpinButton *);

LiVESAdjustment *lives_spin_button_get_adjustment(LiVESSpinButton *);
LiVESAdjustment *lives_spin_button_set_adjustment(LiVESSpinButton *, LiVESAdjustment *adj);

boolean lives_spin_button_set_value(LiVESSpinButton *, double value);
boolean lives_spin_button_set_range(LiVESSpinButton *, double min, double max);

boolean lives_spin_button_set_max(LiVESSpinButton *, double max);
boolean lives_spin_button_set_min(LiVESSpinButton *, double min);

boolean lives_spin_button_set_wrap(LiVESSpinButton *, boolean wrap);

boolean lives_spin_button_set_step_increment(LiVESSpinButton *button, double step_increment);
boolean lives_spin_button_set_snap_to_ticks(LiVESSpinButton *, boolean snap);
boolean lives_spin_button_set_snap_to_multiples(LiVESSpinButton *, double mult);

boolean lives_spin_button_set_digits(LiVESSpinButton *, uint32_t digits);
uint32_t lives_spin_button_get_digits(LiVESSpinButton *);

boolean lives_spin_button_update(LiVESSpinButton *);

LiVESWidget *lives_color_button_new_with_color(const LiVESWidgetColor *);
LiVESWidgetColor *lives_color_button_get_color(LiVESColorButton *, LiVESWidgetColor *);
boolean lives_color_button_set_color(LiVESColorButton *, const LiVESWidgetColor *);
boolean lives_color_button_set_alpha(LiVESColorButton *, int16_t alpha);
int16_t lives_color_button_get_alpha(LiVESColorButton *);
boolean lives_color_button_set_title(LiVESColorButton *, const char *title);
boolean lives_color_button_set_use_alpha(LiVESColorButton *, boolean use_alpha);

LiVESToolItem *lives_tool_button_new(LiVESWidget *icon_widget, const char *label);
LiVESToolItem *lives_tool_item_new(void);
LiVESToolItem *lives_separator_tool_item_new(void);
boolean lives_tool_button_set_icon_widget(LiVESToolButton *, LiVESWidget *icon);
boolean lives_tool_button_set_label_widget(LiVESToolButton *, LiVESWidget *label);
boolean lives_tool_button_set_use_underline(LiVESToolButton *, boolean use_underline);

LiVESWidget *lives_tool_button_get_icon_widget(LiVESToolButton *);
LiVESWidget *lives_tool_button_get_label_widget(LiVESToolButton *);

LiVESWidget *lives_message_dialog_new(LiVESWindow *parent, LiVESDialogFlags flags, LiVESMessageType type,
                                      LiVESButtonsType buttons,
                                      const char *msg_fmt, ...);

double lives_ruler_get_value(LiVESRuler *);
double lives_ruler_set_value(LiVESRuler *, double value);

boolean lives_ruler_set_range(LiVESRuler *, double lower, double upper, double position, double max_size);
double lives_ruler_set_upper(LiVESRuler *, double upper);
double lives_ruler_set_lower(LiVESRuler *, double lower);

LiVESWidget *lives_toolbar_new(void);
boolean lives_toolbar_insert(LiVESToolbar *, LiVESToolItem *, int pos);
boolean lives_toolbar_set_show_arrow(LiVESToolbar *, boolean show);
LiVESIconSize lives_toolbar_get_icon_size(LiVESToolbar *);
boolean lives_toolbar_set_icon_size(LiVESToolbar *, LiVESIconSize icon_size);
boolean lives_toolbar_set_style(LiVESToolbar *, LiVESToolbarStyle style);

int lives_widget_get_allocation_x(LiVESWidget *);
int lives_widget_get_allocation_y(LiVESWidget *);
int lives_widget_get_allocation_width(LiVESWidget *);
int lives_widget_get_allocation_height(LiVESWidget *);

boolean lives_widget_set_state(LiVESWidget *, LiVESWidgetState state);
LiVESWidgetState lives_widget_get_state(LiVESWidget *widget);

LiVESWidget *lives_bin_get_child(LiVESBin *);

boolean lives_widget_is_sensitive(LiVESWidget *);
boolean lives_widget_is_visible(LiVESWidget *);
boolean lives_widget_is_realized(LiVESWidget *);

double lives_adjustment_get_upper(LiVESAdjustment *);
double lives_adjustment_get_lower(LiVESAdjustment *);
double lives_adjustment_get_page_size(LiVESAdjustment *);
double lives_adjustment_get_step_increment(LiVESAdjustment *);
double lives_adjustment_get_page_increment(LiVESAdjustment *);
double lives_adjustment_get_value(LiVESAdjustment *);

boolean lives_adjustment_set_upper(LiVESAdjustment *, double upper);
boolean lives_adjustment_set_lower(LiVESAdjustment *, double lower);
boolean lives_adjustment_set_page_size(LiVESAdjustment *, double page_size);
boolean lives_adjustment_set_step_increment(LiVESAdjustment *, double step_increment);
boolean lives_adjustment_set_value(LiVESAdjustment *, double value);

boolean lives_adjustment_clamp_page(LiVESAdjustment *, double lower, double upper);

LiVESAdjustment *lives_range_get_adjustment(LiVESRange *);
boolean lives_range_set_adjustment(LiVESRange *, LiVESAdjustment *);
boolean lives_range_set_value(LiVESRange *, double value);
boolean lives_range_set_range(LiVESRange *, double min, double max);
boolean lives_range_set_increments(LiVESRange *, double step, double page);
boolean lives_range_set_inverted(LiVESRange *, boolean invert);

double lives_range_get_value(LiVESRange *);

boolean lives_editable_set_editable(LiVESEditable *, boolean editable);
boolean lives_editable_get_editable(LiVESEditable *);
boolean lives_editable_select_region(LiVESEditable *, int start_pos, int end_pos);

LiVESWidget *lives_entry_new(void);
boolean lives_entry_set_editable(LiVESEntry *, boolean editable);
boolean lives_entry_get_editable(LiVESEntry *);
const char *lives_entry_get_text(LiVESEntry *);
boolean lives_entry_set_text(LiVESEntry *, const char *text);
boolean lives_entry_set_width_chars(LiVESEntry *, int nchars);
boolean lives_entry_set_max_length(LiVESEntry *, int len);
boolean lives_entry_set_activates_default(LiVESEntry *, boolean act);
boolean lives_entry_get_activates_default(LiVESEntry *);
boolean lives_entry_set_visibility(LiVESEntry *, boolean vis);
boolean lives_entry_set_has_frame(LiVESEntry *, boolean has);
boolean lives_entry_set_alignment(LiVESEntry *, float align);

void lives_entries_link(LiVESEntry *from, LiVESEntry *to);

double lives_scale_button_get_value(LiVESScaleButton *);
boolean lives_scale_button_set_value(LiVESScaleButton *, double value);

LiVESWidget *lives_table_new(uint32_t rows, uint32_t cols, boolean homogeneous);
boolean lives_table_set_row_spacings(LiVESTable *, uint32_t spacing);
boolean lives_table_set_col_spacings(LiVESTable *, uint32_t spacing);
boolean lives_table_resize(LiVESTable *, uint32_t rows, uint32_t cols);
boolean lives_table_attach(LiVESTable *, LiVESWidget *child, uint32_t left, uint32_t right,
                           uint32_t top, uint32_t bottom, LiVESAttachOptions xoptions, LiVESAttachOptions yoptions,
                           uint32_t xpad, uint32_t ypad);

boolean lives_table_set_column_homogeneous(LiVESTable *, boolean homogeneous);
boolean lives_table_set_row_homogeneous(LiVESTable *, boolean homogeneous);

#if LIVES_TABLE_IS_GRID
LiVESWidget *lives_grid_new(void);
boolean lives_grid_set_row_spacing(LiVESGrid *, uint32_t spacing);
boolean lives_grid_set_column_spacing(LiVESGrid *, uint32_t spacing);
boolean lives_grid_attach_next_to(LiVESGrid *, LiVESWidget *child, LiVESWidget *sibling,
                                  LiVESPositionType side, int width, int height);

boolean lives_grid_insert_row(LiVESGrid *, int posn);
boolean lives_grid_remove_row(LiVESGrid *, int posn);
#endif

#if GTK_CHECK_VERSION(3,2,0)
char *lives_font_chooser_get_font(LiVESFontChooser *);
boolean lives_font_chooser_set_font(LiVESFontChooser *, const char *fontname);
LingoFontDesc *lives_font_chooser_get_font_desc(LiVESFontChooser *);
boolean lives_font_chooser_set_font_desc(LiVESFontChooser *, LingoFontDesc *lfd);
#endif

LiVESWidget *lives_frame_new(const char *label);
boolean lives_frame_set_label(LiVESFrame *, const char *label);
boolean lives_frame_set_label_align(LiVESFrame *, float xalign, float yalign);
boolean lives_frame_set_label_widget(LiVESFrame *, LiVESWidget *);
LiVESWidget *lives_frame_get_label_widget(LiVESFrame *);
boolean lives_frame_set_shadow_type(LiVESFrame *, LiVESShadowType);

LiVESWidget *lives_notebook_new(void);
LiVESWidget *lives_notebook_get_nth_page(LiVESNotebook *, int pagenum);
int lives_notebook_get_current_page(LiVESNotebook *);
boolean lives_notebook_set_current_page(LiVESNotebook *, int pagenum);
boolean lives_notebook_set_tab_label(LiVESNotebook *, LiVESWidget *child, LiVESWidget *tablabel);

LiVESWidget *lives_menu_new(void);
LiVESWidget *lives_menu_bar_new(void);

boolean lives_menu_popup(LiVESMenu *, LiVESXEventButton *);

boolean lives_menu_reorder_child(LiVESMenu *, LiVESWidget *, int pos);
boolean lives_menu_detach(LiVESMenu *);

boolean lives_menu_shell_insert(LiVESMenuShell *, LiVESWidget *child, int pos);
boolean lives_menu_shell_prepend(LiVESMenuShell *, LiVESWidget *child);
boolean lives_menu_shell_append(LiVESMenuShell *, LiVESWidget *child);

LiVESWidget *lives_menu_item_new(void);
LiVESWidget *lives_menu_item_new_with_label(const char *label);

LiVESWidget *lives_separator_menu_item_new(void);

boolean lives_menu_item_set_accel_path(LiVESMenuItem *, const char *path);

LiVESWidget *lives_check_menu_item_new_with_label(const char *label);
boolean lives_check_menu_item_set_draw_as_radio(LiVESCheckMenuItem *, boolean setting);

LiVESWidget *lives_radio_menu_item_new_with_label(LiVESSList *group, const char *label);
LiVESWidget *lives_image_menu_item_new_with_label(const char *label);
LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group);

LiVESToolItem *lives_menu_tool_button_new(LiVESWidget *icon, const char *label);
boolean lives_menu_tool_button_set_menu(LiVESMenuToolButton *, LiVESWidget *menu);

#if LIVES_HAS_IMAGE_MENU_ITEM

boolean lives_image_menu_item_set_image(LiVESImageMenuItem *, LiVESWidget *image);

#endif

boolean lives_menu_item_set_submenu(LiVESMenuItem *, LiVESWidget *);
LiVESWidget *lives_menu_item_get_submenu(LiVESMenuItem *);

boolean lives_menu_item_activate(LiVESMenuItem *);

boolean lives_check_menu_item_set_active(LiVESCheckMenuItem *, boolean state);
boolean lives_check_menu_item_get_active(LiVESCheckMenuItem *);

boolean lives_menu_set_title(LiVESMenu *, const char *title);

int lives_display_get_n_screens(LiVESXDisplay *);

char *lives_file_chooser_get_filename(LiVESFileChooser *);
LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser *);

boolean lives_widget_grab_focus(LiVESWidget *);
boolean lives_widget_grab_default(LiVESWidget *);

LiVESWidget *lives_widget_set_tooltip_text(LiVESWidget *, const char *text);

boolean lives_widget_process_updates(LiVESWidget *);

boolean lives_xwindow_get_origin(LiVESXWindow *, int *posx, int *posy);
boolean lives_xwindow_get_frame_extents(LiVESXWindow *, lives_rect_t *);

LiVESAccelGroup *lives_accel_group_new(void);
boolean lives_accel_group_connect(LiVESAccelGroup *, uint32_t key, LiVESXModifierType mod, LiVESAccelFlags flags,
                                  LiVESWidgetClosure *closure);
boolean lives_accel_group_disconnect(LiVESAccelGroup *, LiVESWidgetClosure *closure);
boolean lives_accel_groups_activate(LiVESWidgetObject *object, uint32_t key, LiVESXModifierType mod);

boolean lives_widget_add_accelerator(LiVESWidget *, const char *accel_signal, LiVESAccelGroup *accel_group,
                                     uint32_t accel_key, LiVESXModifierType accel_mods, LiVESAccelFlags accel_flags);

boolean lives_widget_get_pointer(LiVESXDevice *, LiVESWidget *, int *x, int *y);
boolean lives_widget_get_modmask(LiVESXDevice *, LiVESWidget *, LiVESXModifierType *modmask);
LiVESXWindow *lives_display_get_window_at_pointer(LiVESXDevice *, LiVESXDisplay *, int *win_x, int *win_y);
boolean lives_display_get_pointer(LiVESXDevice *, LiVESXDisplay *, LiVESXScreen **, int *x, int *y, LiVESXModifierType *mask);
boolean lives_display_warp_pointer(LiVESXDevice *, LiVESXDisplay *, LiVESXScreen *, int x, int y);

LiVESXDisplay *lives_widget_get_display(LiVESWidget *);
lives_display_t lives_widget_get_display_type(LiVESWidget *);
lives_display_t lives_xwindow_get_display_type(LiVESXWindow *);

uint64_t lives_xwindow_get_xwinid(LiVESXWindow *, const char *failure_msg);
uint64_t lives_widget_get_xwinid(LiVESWidget *, const char *failure_msg);
LiVESWindow *lives_widget_get_window(LiVESWidget *);

LiVESWidget *lives_scrolled_window_new(void);
LiVESWidget *lives_scrolled_window_new_with_adj(LiVESAdjustment *hadj, LiVESAdjustment *vadj);
LiVESAdjustment *lives_scrolled_window_get_hadjustment(LiVESScrolledWindow *);
LiVESAdjustment *lives_scrolled_window_get_vadjustment(LiVESScrolledWindow *);

LiVESWidget *lives_scrolled_window_get_hscrollbar(LiVESScrolledWindow *);
LiVESWidget *lives_scrolled_window_get_vscrollbar(LiVESScrolledWindow *);

boolean lives_scrolled_window_set_policy(LiVESScrolledWindow *, LiVESPolicyType hpolicy, LiVESPolicyType vpolicy);
boolean lives_scrolled_window_add_with_viewport(LiVESScrolledWindow *, LiVESWidget *child);

boolean lives_scrolled_window_set_min_content_height(LiVESScrolledWindow *, int height);
boolean lives_scrolled_window_set_min_content_width(LiVESScrolledWindow *, int width);

boolean lives_xwindow_raise(LiVESXWindow *);
boolean lives_xwindow_set_cursor(LiVESXWindow *, LiVESXCursor *);

LiVESWidgetSource *lives_idle_priority(LiVESWidgetSourceFunc function, livespointer data);
uint32_t lives_timer_immediate(LiVESWidgetSourceFunc function, livespointer data);

LiVESWidgetSource *lives_thrd_idle_priority(LiVESWidgetSourceFunc function, livespointer data);

//uint32_t lives_idle_priority_longrun(LiVESWidgetSourceFunc function, livespointer data);

#ifdef GUI_GTK
#define IDLEFUNC_VALID (!g_source_is_destroyed (g_main_current_source ()))
#else
#define IFLEFUNC_VALID 1
#endif

boolean lives_source_remove(uint32_t handle);
boolean lives_source_attach(LiVESWidgetSource *, LiVESWidgetContext *);
boolean lives_source_set_priority(LiVESWidgetSource *, int32_t prio);
boolean lives_source_set_callback(LiVESWidgetSource *, LiVESWidgetSourceFunc func,
                                  livespointer data);

uint32_t lives_accelerator_get_default_mod_mask();

int lives_screen_get_width(LiVESXScreen *);
int lives_screen_get_height(LiVESXScreen *);

boolean lives_scale_set_draw_value(LiVESScale *, boolean draw_value);
boolean lives_scale_set_value_pos(LiVESScale *, LiVESPositionType ptype);
boolean lives_scale_set_digits(LiVESScale *, int digits);

boolean lives_has_toplevel_focus(LiVESWidget *window);

// optional (return TRUE if implemented)

boolean lives_dialog_set_has_separator(LiVESDialog *, boolean has);
boolean lives_widget_set_hexpand(LiVESWidget *, boolean state);
boolean lives_widget_set_vexpand(LiVESWidget *, boolean state);
boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem *, boolean show);
boolean lives_scale_button_set_orientation(LiVESScaleButton *, LiVESOrientation orientation);
boolean lives_window_set_auto_startup_notification(boolean set);

// compound functions (composed of basic functions)
boolean lives_widget_set_pack_type(LiVESBox *, LiVESWidget *, LiVESPackType);

void lives_label_set_hpadding(LiVESLabel *, int pad);

LiVESWidget *align_horizontal_with(LiVESWidget *thingtoadd, LiVESWidget *thingtoalignwith);

boolean lives_box_pack_first(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding);

// this is not the same as a GtkLayout !!

#define lives_layout_set_row_spacings(l, s) lives_table_set_row_spacings(l, s)

LiVESWidget *lives_layout_new(LiVESBox *);
LiVESWidget *lives_layout_hbox_new(LiVESLayout *);
LiVESWidget *lives_layout_row_new(LiVESLayout *);
int lives_layout_add_row(LiVESLayout *);
LiVESWidget *lives_layout_pack(LiVESHBox *, LiVESWidget *);
LiVESWidget *lives_layout_add_label(LiVESLayout *, const char *text, boolean horizontal);
void lives_layout_label_set_text(LiVESLabel *, const char *text);
LiVESWidget *lives_layout_add_fill(LiVESLayout *, boolean horizontal);
LiVESWidget *lives_layout_add_separator(LiVESLayout *, boolean horizontal);
LiVESWidget *lives_layout_expansion_row_new(LiVESLayout *, LiVESWidget *widget);

LiVESAdjustment *lives_adjustment_copy(LiVESAdjustment *adj);

boolean lives_button_grab_default_special(LiVESWidget *);
boolean lives_button_ungrab_default_special(LiVESWidget *);

#define BUTTON_DIM_VAL (0.4 * 65535.) // fg / bg ratio for dimmed buttons (BUTTON_DIM_VAL/65535) (lower is dimmer)

boolean show_warn_image(LiVESWidget *, const char *text);
boolean hide_warn_image(LiVESWidget *);

boolean is_standard_widget(LiVESWidget *);

boolean lives_widget_set_frozen(LiVESWidget *, boolean state, double opac);

#ifdef USE_SPECIAL_BUTTONS
void render_standard_button(LiVESButton *sbutton);

LiVESWidget *lives_standard_button_new(int width, int height);
LiVESWidget *lives_standard_button_new_with_label(const char *labeltext, int width, int height);
boolean lives_standard_button_set_label(LiVESButton *, const char *label);
const char *lives_standard_button_get_label(LiVESButton *);
boolean lives_standard_button_set_image(LiVESButton *, LiVESWidget *image, boolean force_show);

LiVESWidget *lives_standard_button_new_full(const char *label, int width, int height, LiVESBox *,
    boolean fake_default, const char *ttips);
LiVESWidget *lives_standard_button_new_from_stock_full(const char *stock_id, const char *label,
    int width, int height, LiVESBox *, boolean fake_default, const char *ttips);
#else
#define lives_standard_button_new(w, h) lives_button_new()
#define lives_standard_button_new_with_label(l, w, h) lives_button_new_with_label(l)
#define lives_standard_button_set_label(b, l); lives_button_set_label(b, l)
#define lives_standard_button_get_label(b) lives_button_get_label(b)
#define lives_standard_button_set_image(b, i) lives_button_set_image(b, i)
#endif

LiVESWidget *lives_standard_button_new_from_stock(const char *stock_id, const char *label,
    int width, int height);
LiVESWidget *lives_standard_menu_new(void);

LiVESWidget *lives_standard_menu_item_new(void);
LiVESWidget *lives_standard_menu_item_new_with_label(const char *labeltext);

LiVESWidget *lives_standard_image_menu_item_new_with_label(const char *labeltext);
LiVESWidget *lives_standard_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group);

LiVESWidget *lives_standard_radio_menu_item_new_with_label(LiVESSList *group, const char *labeltext);

LiVESWidget *lives_standard_check_menu_item_new_with_label(const char *labeltext, boolean active);

LiVESWidget *lives_standard_check_menu_item_new_for_var(const char *ltext, boolean *var, boolean invert);

LiVESWidget *lives_standard_switch_new(const char *labeltext, boolean active, LiVESBox *,
                                       const char *tooltip);

LiVESWidget *lives_standard_vpaned_new(void);
LiVESWidget *lives_standard_hpaned_new(void);

LiVESWidget *lives_standard_notebook_new(const LiVESWidgetColor *bg_color, const LiVESWidgetColor *act_color);

LiVESWidget *lives_standard_label_new(const char *labeltext);
LiVESWidget *lives_standard_label_new_with_mnemonic_widget(const char *text, LiVESWidget *mnemonic_widget);
LiVESWidget *lives_standard_label_new_with_tooltips(const char *text, LiVESBox *box,
    const char *tips);
LiVESWidget *lives_standard_formatted_label_new(const char *text);

char *lives_big_and_bold(const char *fmt, ...);

void lives_label_chomp(LiVESLabel *);

LiVESWidget *lives_standard_drawing_area_new(LiVESGuiCallback, lives_painter_surface_t **);

LiVESWidget *lives_standard_frame_new(const char *labeltext, float xalign, boolean invisible_outline);

LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean active, LiVESBox *, const char *tooltip);
LiVESWidget *lives_glowing_check_button_new(const char *labeltext, LiVESBox *, const char *tooltip, boolean *togglevalue);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, LiVESSList **rbgroup,
    LiVESBox *, const char *tooltip);


boolean spin_ranges_set_exclusive(LiVESSpinButton *, LiVESSpinButton *, int excl);
void spval_sets_start(LiVESSpinButton *, LiVESSpinButton *);
void spval_sets_end(LiVESSpinButton *, LiVESSpinButton *);

LiVESWidget *lives_standard_spin_button_new(const char *labeltext, double val, double min,
    double max, double step, double page, int dp, LiVESBox *,
    const char *tooltip);
LiVESWidget *lives_standard_combo_new(const char *labeltext, LiVESList *list, LiVESBox *, const char *tooltip);

LiVESWidget *lives_standard_combo_new_with_model(LiVESTreeModel *, LiVESBox *);

LiVESWidget *lives_standard_entry_new(const char *labeltext, const char *txt, int dispwidth, int maxchars, LiVESBox *,
                                      const char *tooltip);

LiVESWidget *lives_standard_direntry_new(const char *labeltext, const char *txt, int dispwidth, int maxchars, LiVESBox *,
    const char *tooltip);

LiVESWidget *lives_standard_fileentry_new(const char *labeltext, const char *txt, const char *defdir,
    int dispwidth, int maxchars, LiVESBox *, const char *tooltip);

LiVESWidget *lives_standard_progress_bar_new(void);
void set_progbar_colours(LiVESWidget *pbar, boolean new);

LiVESWidget *lives_standard_font_chooser_new(const char *fontname);
boolean lives_standard_font_chooser_set_size(LiVESFontChooser *, int fsize);

LiVESToolItem *lives_menu_tool_button_new(LiVESWidget *icon, const char *label);

LiVESWidget *lives_standard_lock_button_new(boolean is_locked, const char *label, const char *tooltip);

boolean lives_lock_button_get_locked(LiVESButton *);
boolean lives_lock_button_set_locked(LiVESButton *button, boolean state);
boolean lives_lock_button_toggle(LiVESButton *);

boolean lives_dialog_set_button_layout(LiVESDialog *, LiVESButtonBoxStyle bstyle);

LiVESAccelGroup *lives_window_add_escape(LiVESWindow *, LiVESWidget *button);
ulong lives_window_block_delete(LiVESWindow *);

LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons, int width, int height);

LiVESWidget *lives_dialog_add_button_from_stock(LiVESDialog *, const char *stock_id,
    const char *label, LiVESResponseType response_id);

LiVESWidget *lives_standard_hruler_new(void);

LiVESWidget *lives_standard_scrolled_window_new(int width, int height, LiVESWidget *child);

double lives_scrolled_window_scroll_to(LiVESScrolledWindow *, LiVESPositionType pos);

LiVESWidget *lives_standard_expander_new(const char *labeltext, const char *alt_text,
    LiVESBox *parent, LiVESWidget *child);

LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment *, double volume);

LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir);

LiVESWidget *lives_standard_color_button_new(LiVESBox *parent, const char *name, boolean use_alpha, lives_colRGBA64_t *rgba,
    LiVESWidget **sb_red, LiVESWidget **sb_green, LiVESWidget **sb_blue, LiVESWidget **sb_alpha);

LiVESWidget *lives_standard_text_view_new(const char *text, LiVESTextBuffer *tbuff);

LiVESWidget *lives_standard_table_new(uint32_t rows, uint32_t cols, boolean homogeneous);

LiVESToolItem *lives_standard_menu_tool_button_new(LiVESWidget *icon, const char *label);

LiVESXCursor *lives_cursor_new_from_pixbuf(LiVESXDisplay *, LiVESPixbuf *, int x, int y);

void set_button_image_border_colour(LiVESButton *, LiVESWidgetState state, LiVESWidgetColor *);

// util functions

// THEME COLOURS (will be done more logically in the future)
void lives_widget_apply_theme(LiVESWidget *, LiVESWidgetState state); // normal theme colours
void lives_widget_apply_theme_dimmed(LiVESWidget *, LiVESWidgetState state, int dimval); // dimmed normal theme
void set_child_dimmed_colour(LiVESWidget *, int dim); // dimmed normal theme for children (insensitive state only)

// if set_all, set the widget itself (labels always set_all; buttons are ignored if set_all is FALSE)
void *set_child_colour(LiVESWidget *, boolean set_all); // normal theme, sensitive and insensitive

void lives_widget_apply_theme2(LiVESWidget *, LiVESWidgetState state, boolean set_fg); // menu and bars colours
void lives_widget_apply_theme_dimmed2(LiVESWidget *, LiVESWidgetState state, int dimval);
void set_child_dimmed_colour2(LiVESWidget *, int dim); // dimmed m & b for children (insensitive state only)

// like set_child_colour, but with menu and bars colours
void set_child_alt_colour(LiVESWidget *, boolean set_all);
void set_child_alt_colour_prelight(LiVESWidget *);

void lives_widget_apply_theme3(LiVESWidget *, LiVESWidgetState state); // info base/text
void set_child_colour3(LiVESWidget *, boolean set_all);

boolean lives_widget_set_sensitive_with(LiVESWidget *, LiVESWidget *other);
boolean lives_widget_set_show_hide_with(LiVESWidget *, LiVESWidget *other);

boolean lives_image_scale(LiVESImage *, int width, int height, LiVESInterpType interp_type);

LiVESPixbuf *lives_pixbuf_new_from_stock_at_size(const char *stock_id, LiVESIconSize size, int custom_size);
LiVESWidget *lives_image_new_from_stock_at_size(const char *stock_id, LiVESIconSize size, int custom_size);

void lives_widget_queue_draw_if_visible(LiVESWidget *);
boolean lives_widget_queue_draw_and_update(LiVESWidget *);

boolean global_recent_manager_add(const char *file_name);

boolean lives_cursor_unref(LiVESXCursor *);

boolean lives_tree_store_find_iter(LiVESTreeStore *, int col, const char *val, LiVESTreeIter *existing, LiVESTreeIter *newiter);

boolean lives_widget_context_update(void);
boolean lives_widget_context_iteration(LiVESWidgetContext *, boolean may_block);

LiVESWidget *lives_menu_add_separator(LiVESMenu *);

void lives_menu_item_set_text(LiVESWidget *, const char *text, boolean use_mnemonic);
const char *lives_menu_item_get_text(LiVESWidget *);

boolean lives_widget_get_fg_color(LiVESWidget *, LiVESWidgetColor *);

boolean lives_widget_set_show_hide_parent(LiVESWidget *);

boolean lives_window_center(LiVESWindow *);
boolean lives_window_uncenter(LiVESWindow *);

boolean lives_entry_set_completion_from_list(LiVESEntry *, LiVESList *);

boolean lives_widget_unparent(LiVESWidget *);

void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);

char *lives_text_view_get_text(LiVESTextView *);
boolean lives_text_view_set_text(LiVESTextView *, const char *text, int len);
boolean lives_text_view_set_markup(LiVESTextView *, const char *markup);
boolean lives_text_view_strip_markup(LiVESTextView *);

boolean lives_text_buffer_insert_at_end(LiVESTextBuffer *, const char *text);
boolean lives_text_buffer_insert_markup_at_end(LiVESTextBuffer *, const char *markup);

void lives_general_button_clicked(LiVESButton *, livespointer data_to_free);
void lives_general_button_clickedp(LiVESButton *, livespointer *ptr_data_to_free);

boolean lives_spin_button_configure(LiVESSpinButton *, double value, double lower, double upper,
                                    double step_increment, double page_increment);

boolean lives_adjustment_configure(LiVESAdjustment *, double value, double lower, double upper,
                                   double step_increment, double page_increment);

boolean lives_adjustment_configure_the_good_bits(LiVESAdjustment *, double value,
    double lower, double upper);

size_t calc_spin_button_width(double min, double max, int dp);

double lives_spin_button_get_snapval(LiVESSpinButton *, double val);

boolean lives_spin_button_clamp(LiVESSpinButton *);

int get_box_child_index(LiVESBox *, LiVESWidget *child);

boolean lives_box_pack_top(LiVESBox *, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding);
//lives_box_reorder_child(LIVES_BOX(mt->fx_list_vbox), xeventbox, 0);

boolean lives_container_child_set_shrinkable(LiVESContainer *, LiVESWidget *child, boolean val);

boolean set_submenu_colours(LiVESMenu *, LiVESWidgetColor *colf, LiVESWidgetColor *colb);

typedef boolean(*condfuncptr_t)(void *);

/// set callbacks
boolean toggle_sets_sensitive(LiVESToggleButton *, LiVESWidget *, boolean invert);
boolean toggle_toolbutton_sets_sensitive(LiVESToggleToolButton *, LiVESWidget *, boolean invert);
boolean menu_sets_sensitive(LiVESCheckMenuItem *, LiVESWidget *,  boolean invert);
boolean toggle_sets_active(LiVESToggleButton *, LiVESToggleButton *, boolean invert);
boolean toggle_sets_visible(LiVESToggleButton *, LiVESWidget *, boolean invert);
boolean toggle_toolbutton_sets_visible(LiVESToggleToolButton *, LiVESWidget *, boolean invert);
boolean menu_sets_visible(LiVESCheckMenuItem *, LiVESWidget *,  boolean invert);
boolean toggle_toggles_var(LiVESToggleButton *, boolean *var, boolean invert);
boolean toggle_shows_warn_img(LiVESToggleButton *tb, LiVESWidget *widget, boolean invert);

boolean toggle_sets_sensitive_cond(LiVESWidget *tb, LiVESWidget *widget,
                                   condfuncptr_t condsens_f, void *condsens_data,
                                   condfuncptr_t condinsens_f, void *condinsens_data,
                                   boolean invert);

boolean toggle_sets_visible_cond(LiVESWidget *tb, LiVESWidget *widget,
                                 condfuncptr_t condvisi_f, void *condvisi_data,
                                 condfuncptr_t condinvisi_f, void *condinvisi_data,
                                 boolean invert);

boolean toggle_sets_active_cond(LiVESWidget *tb, LiVESWidget *widget,
                                condfuncptr_t condact_f, void *condact_data,
                                condfuncptr_t condinact_f, void *condinact_data,
                                boolean invert);

// callbacks
boolean label_act_toggle(LiVESWidget *, LiVESXEventButton *, LiVESWidget *);
boolean widget_act_toggle(LiVESWidget *, LiVESWidget *);
boolean widget_inact_toggle(LiVESWidget *, LiVESWidget *);

boolean toggle_button_toggle(LiVESToggleButton *);

boolean label_act_lockbutton(LiVESWidget *, LiVESXEventButton *, LiVESButton *);

void funkify_dialog(LiVESWidget *dialog);
EXPOSE_FN_PROTOTYPE(draw_cool_toggle);
void lives_cool_toggled(LiVESWidget *tbutton, livespointer);

boolean unhide_cursor(LiVESXWindow *);
void hide_cursor(LiVESXWindow *);

boolean set_tooltips_state(LiVESWidget *, boolean state);

boolean get_border_size(LiVESWidget *win, int *bx, int *by);
boolean lives_window_get_inner_size(LiVESWindow *, int *x, int *y);

LiVESWidget *lives_standard_hseparator_new(void);
LiVESWidget *lives_standard_vseparator_new(void);

LiVESWidget *add_hsep_to_box(LiVESBox *);
LiVESWidget *add_vsep_to_box(LiVESBox *);

LiVESWidget *add_fill_to_box(LiVESBox *);
LiVESWidget *add_spring_to_box(LiVESBox *, int min);

LiVESWidget *lives_toolbar_insert_space(LiVESToolbar *);
LiVESWidget *lives_toolbar_insert_label(LiVESToolbar *, const char *labeltext, LiVESWidget *actwidg);
LiVESWidget *lives_standard_tool_button_new(LiVESToolbar *, GtkWidget *icon_widget, const char *label, const char *tooltips);
boolean lives_tool_button_set_border_color(LiVESWidget *button, LiVESWidgetState state, LiVESWidgetColor *);
LiVESWidget *lives_glowing_tool_button_new(const char *labeltext, LiVESToolbar *tbar, const char *tooltip,
    boolean *togglevalue);

boolean lives_accel_path_disconnect(LiVESAccelGroup *, const char *path);

boolean lives_widget_get_mod_mask(LiVESWidget *, LiVESXModifierType *modmask);

boolean lives_widget_nullify_with(LiVESWidget *, void **);
boolean lives_widget_destroy_with(LiVESWidget *, LiVESWidget *dieplease);

LiVESPixbuf *get_desktop_icon(const char *dir);

extern void pop_to_front(LiVESWidget *, LiVESWidget *extra);

#define LIVES_JUSTIFY_DEFAULT (def_widget_opts.justify)

typedef enum {
  LIVES_CURSOR_NORMAL = 0, ///< must be zero
  LIVES_CURSOR_BUSY,
  LIVES_CURSOR_CENTER_PTR,
  LIVES_CURSOR_HAND2,
  LIVES_CURSOR_SB_H_DOUBLE_ARROW,
  LIVES_CURSOR_CROSSHAIR,
  LIVES_CURSOR_TOP_LEFT_CORNER,
  LIVES_CURSOR_BOTTOM_RIGHT_CORNER,

  /// non-standard cursors
  LIVES_CURSOR_BLOCK,
  LIVES_CURSOR_AUDIO_BLOCK,
  LIVES_CURSOR_VIDEO_BLOCK,
  LIVES_CURSOR_FX_BLOCK
} lives_cursor_t;

void lives_set_cursor_style(lives_cursor_t cstyle, LiVESWidget *);

typedef int lives_expand_t;
#define LIVES_EXPAND_NONE 0
#define LIVES_EXPAND_DEFAULT_HEIGHT 1
#define LIVES_EXPAND_DEFAULT_WIDTH 2
#define LIVES_EXPAND_DEFAULT (LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_DEFAULT_WIDTH)
#define LIVES_EXPAND_EXTRA_HEIGHT 4
#define LIVES_EXPAND_EXTRA_WIDTH 8
#define LIVES_EXPAND_EXTRA (LIVES_EXPAND_EXTRA_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH)

#define LIVES_SHOULD_EXPAND (widget_opts.expand != LIVES_EXPAND_NONE)
#define LIVES_SHOULD_EXPAND_DEFAULT (widget_opts.expand == LIVES_EXPAND_DEFAULT)
#define LIVES_SHOULD_EXPAND_EXTRA (widget_opts.expand == LIVES_EXPAND_EXTRA)

#define LIVES_EXPAND_WIDTH(expand) (((expand) & (LIVES_EXPAND_DEFAULT_WIDTH | LIVES_EXPAND_EXTRA_WIDTH)) ? TRUE : FALSE)
#define LIVES_EXPAND_HEIGHT(expand) (((expand) & (LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_HEIGHT)) ? TRUE : FALSE)

#define LIVES_SHOULD_EXPAND_WIDTH LIVES_EXPAND_WIDTH(widget_opts.expand)
#define LIVES_SHOULD_EXPAND_HEIGHT LIVES_EXPAND_HEIGHT(widget_opts.expand)

#define LIVES_SHOULD_EXPAND_EXTRA_WIDTH ((widget_opts.expand & LIVES_EXPAND_EXTRA_WIDTH) ? TRUE : FALSE)
#define LIVES_SHOULD_EXPAND_EXTRA_HEIGHT ((widget_opts.expand & LIVES_EXPAND_EXTRA_HEIGHT) ? TRUE : FALSE)

#define LIVES_SHOULD_EXPAND_DEFAULT_WIDTH (LIVES_SHOULD_EXPAND_WIDTH && !LIVES_SHOULD_EXPAND_EXTRA_WIDTH)
#define LIVES_SHOULD_EXPAND_DEFAULT_HEIGHT (LIVES_SHOULD_EXPAND_HEIGHT && !LIVES_SHOULD_EXPAND_EXTRA_HEIGHT)

#define LIVES_SHOULD_EXPAND_DEFAULT_FOR(box) (((LIVES_IS_HBOX(box) && LIVES_SHOULD_EXPAND_DEFAULT_WIDTH) \
					       || (LIVES_IS_VBOX(box) && LIVES_EXPAND_DEFAULT_HEIGHT)) \
					      ? TRUE : FALSE)

#define LIVES_SHOULD_EXPAND_EXTRA_FOR(box) (((LIVES_IS_HBOX(box) && LIVES_SHOULD_EXPAND_EXTRA_WIDTH) \
					     || (LIVES_IS_VBOX(box) && LIVES_SHOULD_EXPAND_EXTRA_HEIGHT)) \
					    ? TRUE : FALSE)

#define LIVES_SHOULD_EXPAND_FOR(box) (LIVES_SHOULD_EXPAND_DEFAULT_FOR(box) || LIVES_SHOULD_EXPAND_EXTRA_FOR(box))

LiVESList *get_textsizes_list(void);
const char *lives_textsize_to_string(int val);

// custom stock images
#define LIVES_LIVES_STOCK_AUDIO "lives-audio"
#define LIVES_LIVES_STOCK_PLAY_SEL "lives-playsel"
#define LIVES_LIVES_STOCK_FULLSCREEN "lives-fullscreen"
#define LIVES_LIVES_STOCK_SEPWIN "lives-sepwin"
#define LIVES_LIVES_STOCK_VOLUME_MUTE "lives-volume_mute"
#define LIVES_LIVES_STOCK_LOOP "lives-loop"
#define LIVES_LIVES_STOCK_ZOOM_IN "lives-zoom-in"
#define LIVES_LIVES_STOCK_ZOOM_OUT "lives-zoom-out"
#define LIVES_LIVES_STOCK_PREF_GUI "lives-pref_gui"
#define LIVES_LIVES_STOCK_PREF_DECODING "lives-pref_decoding"
#define LIVES_LIVES_STOCK_PREF_DIRECTORY "lives-pref_directory"
#define LIVES_LIVES_STOCK_PREF_EFFECTS "lives-pref_effects"
#define LIVES_LIVES_STOCK_PREF_ENCODING "lives-pref_encoding"
#define LIVES_LIVES_STOCK_PREF_JACK "lives-pref_jack"
#define LIVES_LIVES_STOCK_PREF_MIDI "lives-pref_midi"
#define LIVES_LIVES_STOCK_PREF_MISC "lives-pref_misc"
#define LIVES_LIVES_STOCK_PREF_MULTITRACK "lives-pref_multitrack"
#define LIVES_LIVES_STOCK_PREF_NET "lives-pref_net"
#define LIVES_LIVES_STOCK_PREF_PLAYBACK "lives-pref_playback"
#define LIVES_LIVES_STOCK_PREF_RECORD "lives-pref_record"
#define LIVES_LIVES_STOCK_PREF_THEMES "lives-pref_themes"
#define LIVES_LIVES_STOCK_PREF_WARNING "lives-pref_warning"

// font sizes
#define LIVES_TEXT_SIZE_XX_SMALL "xx-small" // 0
#define LIVES_TEXT_SIZE_X_SMALL "x-small"  // 1
#define LIVES_TEXT_SIZE_SMALL "small"      // 2
#define LIVES_TEXT_SIZE_MEDIUM "medium"   // 3
#define LIVES_TEXT_SIZE_LARGE "large"      // 4
#define LIVES_TEXT_SIZE_X_LARGE "x-large"    // 5
#define LIVES_TEXT_SIZE_XX_LARGE "xx-large"   // 6
#define LIVES_TEXT_SIZE_NORMAL LIVES_TEXT_SIZE_MEDIUM
#define N_TEXT_SIZES 7

#define STOCK_LABEL_TEXT(text) (subst_quote(LIVES_STOCK_LABEL_##text, "'", "_", ""))

/// stock labels, these are set up in widget_helper_init()
char LIVES_STOCK_LABEL_CANCEL[32];
char LIVES_STOCK_LABEL_OK[32];
char LIVES_STOCK_LABEL_YES[32];
char LIVES_STOCK_LABEL_NO[32];
char LIVES_STOCK_LABEL_SAVE[32];
char LIVES_STOCK_LABEL_SAVE_AS[32];
char LIVES_STOCK_LABEL_OPEN[32];
char LIVES_STOCK_LABEL_QUIT[32];
char LIVES_STOCK_LABEL_APPLY[32];
char LIVES_STOCK_LABEL_CLOSE[32];
char LIVES_STOCK_LABEL_REVERT[32];
char LIVES_STOCK_LABEL_REFRESH[32];
char LIVES_STOCK_LABEL_DELETE[32];
char LIVES_STOCK_LABEL_SELECT_ALL[32];
char LIVES_STOCK_LABEL_GO_FORWARD[32];

char LIVES_STOCK_LABEL_MEDIA_FORWARD[32];
char LIVES_STOCK_LABEL_MEDIA_REWIND[32];
char LIVES_STOCK_LABEL_MEDIA_STOP[32];
char LIVES_STOCK_LABEL_MEDIA_PLAY[32];
char LIVES_STOCK_LABEL_MEDIA_PAUSE[32];
char LIVES_STOCK_LABEL_MEDIA_RECORD[32];

char LIVES_STOCK_LABEL_CLOSE_WINDOW[32];
char LIVES_STOCK_LABEL_SKIP[32];
char LIVES_STOCK_LABEL_ABORT[32];
char LIVES_STOCK_LABEL_BROWSE[32];
char LIVES_STOCK_LABEL_SELECT[32];
char LIVES_STOCK_LABEL_BACK[32];
char LIVES_STOCK_LABEL_NEXT[32];
char LIVES_STOCK_LABEL_RETRY[32];
char LIVES_STOCK_LABEL_RESET[32];

typedef struct {
  /// commonly adjusted values //////
  LiVESWidget *last_widget; ///< last standard widget created (spin,radio,check,entry,combo) (READONLY)
  LiVESWidget *last_label; ///< label widget of last standard widget (spin,radio,check,entry,combo) (READONLY)
  LiVESWidget *last_container; ///< container which wraps last widget created + subwidgets (READONLY)
  lives_expand_t expand; ///< how much space to apply between widgets
  int apply_theme; ///< theming variation for widget (0 -> no theme, 1 -> normal colours, 2+ -> theme variants)
  int packing_width; ///< horizontal pixels between widgets
  int packing_height; ///< vertical pixels between widgets
  LiVESJustification justify; ///< justify for labels

  /// specialised values /////
  const char *text_size; ///< size of text for label widgets etc (e.g. LIVES_TEXT_SIZE_MEDIUM)
  int border_width; ///< border width in pixels
  boolean swap_label; ///< swap label/widget position
  boolean pack_end; ///< pack widget at end or start
  boolean line_wrap; ///< line wrapping for labels
  boolean mnemonic_label; ///< if underscore in label text should be mnemonic accelerator
  boolean use_markup; ///< whether markup should be used in labels
  boolean non_modal; ///< non-modal for dialogs
  LiVESWindow *transient; ///< transient window for dialogs, if NULL then use the default (READ / WRITE)
  int filler_len; ///< length of extra "fill" between widgets

  /// rarely changed values /////
  int css_min_width;
  int css_min_height;
  int icon_size; ///< icon size for tooltips image, warn image, toolbar img, etc.
  boolean no_gui; ///< show nothing !
  double scaleW; ///< width scale factor for all sizes
  double scaleH; ///< height scale factor for all sizes
  boolean alt_button_order; ///< unused for now
  char **image_filter; ///</ NULL or NULL terminated list of image extensions which can be loaded
  char *title_prefix; ///< Text which is prepended to window titles, etc.
  int monitor; ///< monitor we are displaying on
  boolean show_button_images; ///< whether to show small images in buttons or not
  void *icon_theme; /// pointer to opaque "icon_theme"
} widget_opts_t;

widget_opts_t widget_opts;
widget_opts_t def_widget_opts;

#ifdef NEED_DEF_WIDGET_OPTS

const widget_opts_t _def_widget_opts = {
  NULL, ///< last_widget
  NULL, ///< last_label
  NULL, ///< last_container
  LIVES_EXPAND_DEFAULT, ///< default expand
  0, ///< no themeing
  W_PACKING_WIDTH, ///< def packing width
  W_PACKING_HEIGHT, ///< def packing height
  LIVES_JUSTIFY_START, ///< justify
  LIVES_TEXT_SIZE_MEDIUM, ///< default font size
  W_BORDER_WIDTH, ///< def border width

  FALSE, ///< swap_label (TRUE for RTL ?)
  FALSE, ///<pack_end
  FALSE, ///< line_wrap
  TRUE, ///< mnemonic_label
  FALSE, ///< use markup
  FALSE, ///< non_modal
  NULL, ///< transient window
  W_FILL_LENGTH, ///< def fill width (in pixels)

  W_CSS_MIN_WIDTH, ///< css_min_width
  W_CSS_MIN_HEIGHT, ///< css_min_height
  LIVES_ICON_SIZE_LARGE_TOOLBAR, ///< icon_size
  FALSE, ///< no_gui
  1.0, ///< default scaleW
  1.0, ///< default scaleH
  FALSE, ///< alt button order
  NULL, ///< image_filter
  "", ///< title_prefix
  0, ///< monitor
  FALSE, ///< show button images
  NULL ///< icon theme
};

#endif

// object data keys
#define HIDDEN_KEY "hidden"
#define SECLIST_KEY "secondary_list"
#define SECLIST_VAL_KEY "secondary_list_value"
#define ISDIR_KEY "is_dir"
#define FILTER_KEY "filter"
#define DEFDIR_KEY "def_dir"
#define BUTTON_KEY "link_button"
#define RFX_KEY "rfx"
#define REFCOUNT_KEY "refcountr"
#define LAYER_PROXY_KEY "proxy_layer"
#define DISP_LABEL_KEY "disp_label"
#define KEEPABOVE_KEY "keep_above"
#define ACTIVATE_KEY "activate"
#define TEXTWIDGET_KEY "def_dir"
#define FILESEL_TYPE_KEY "filesel_type"
#define PARAM_NUMBER_KEY "param_number"
#define WH_LAYOUT_KEY "_wh_layout"
#define WIDTH_KEY "_wh_width"
#define HEIGHT_KEY "_wh_height"
#define DEFER_KEY "_wh_defer"
#define DEFERRED_KEY "_wh_deferred"

#define MUTEX_KEY "_wh_mutex"
#define PBS_KEY "_wh_pbs"

#define SBUTT_MARKUP_KEY "_sbutt_markup"

// will be moved to separate header after testing
//#define WEED_WIDGETS
#ifdef WEED_WIDGETS

#include "main.h"
#include "threading.h"

#define KLASSES_PER_TOOLKIT 1024

#define N_TOOLKITS 1

typedef enum {
  LIVES_TOOLKIT_ANY,
  LIVES_TOOLKIT_GTK,
} lives_toolkit_t;

// must be called once only, before any other functions for the toolkit
// analogoue to lives_object_template_init(type)
boolean widget_klasses_init(lives_toolkit_t);

// return list of initialised toolkits (data is LIVES_IN_TO_POINTER(tk_idx))
// list should not be freed
// lives_object_types_available
const LiVESList *widget_toolkits_available(void);

// lives_object_type_get_name
const char *widget_toolkit_get_name(lives_toolkit_t);

typedef weed_plant_t lives_widget_klass_t;
typedef weed_plant_t lives_widget_instance_t;

// object_type_templates
// return read-only list of lives_widget_klass_t * : list must not be freed
//lives_object_type_get_subtypes
const LiVESList *widget_toolkit_klasses_list(lives_toolkit_t);

// returns LIVES_PARAM_WIDGET_* value
// can be done with IMkTypes
int widget_klass_get_idx(const lives_widget_klass_t *);

// returns the human readable name for a klass_idx, e.g "spin button", "scale"
// object_subtype_get_name
const char *klass_idx_get_name(int klass_idx);

enum {
  KLASS_ROLE_UNKNOWN,
  KLASS_ROLE_WIDGET,
  KLASS_ROLE_INTERFACE, // funcs only, non-instantiable
  KLASS_ROLE_TK0, // toolkit specific roles (GtkAdjustment for gtk+)
  KLASS_ROLE_TK1, //
  KLASS_ROLE_TK2, //
  KLASS_ROLE_TK3, //
  KLASS_ROLE_TK4, //
  KLASS_ROLE_TK5, //
  KLASS_ROLE_TK6, //
  KLASS_ROLE_TK7, //
  KLASS_ROLE_OTHER //
};

// wtype is a LIVES_PARAM_TYPE_* from paramwindow.h
// subtype_for_tag ?
const lives_widget_klass_t *widget_klass_for_type(lives_toolkit_t, int wtype);

// subtype_get_type ?
lives_toolkit_t widget_klass_get_toolkit(const lives_widget_klass_t *); // TODO
int widget_klass_get_role(const lives_widget_klass_t *);

// returns the human readable name, e.g "widget", "interface" or a toolkit specific value
const char *klass_role_get_name(lives_toolkit_t, int role);

// list should be freed after use
// subtype_inherits_from ??
LiVESList *widget_klass_inherits_from(const lives_widget_klass_t *); // TODO

// returns list of LIVES_INENTION_* using LIVES_INT_TO_POINTER
// list all is TRUE, also lists inherited functions
// free after use
LiVESList *widget_klass_list_intentions(const lives_widget_klass_t *, boolean list_all);

// returns human readable name of function, e.g "create", "destroy", "get value", "set value"
const char *widget_intention_get_name(lives_intention intent);

// defn. in threading.h, "data" points to function source, either self or an inherited klass
// functions are searched for in order: klass, inherited klasses
// may return multiple values with differing return types, however there can only be one per type
// nfuncs tells how many were found, if nfuncs is zero, the function was not found and NULL is returned
// the returned array should be freed if non-NULL, but not the actual array elements
// intents_for_type
lives_func_info_t **get_widget_funcs_for_klass(const lives_widget_klass_t *, lives_intention intent, int *nfuncs);

// calls LIVES_WIDGET_CREATE_FUNC
lives_widget_instance_t *widget_instance_from_klass(const lives_widget_klass_t *, ...);
const lives_widget_klass_t *widget_instance_get_klass(lives_widget_instance_t *);

// lives_object_instance_get_fundamental
void *widget_instance_get_widget(lives_widget_instance_t *);

// returns list of LIVES_WIDGET_FUNC_* using LIVES_INT_TO_POINTER
// if inherited is TRUE, only list functions inherited from the klass
// if inherited is FALSE, only list functions overriden for instance
// free after use
// object_instance_list_intents
LiVESList *widget_instance_list_funcs(lives_widget_instance_t *, boolean inherited); // TODO

// defn. in threading.h, "data" points to the function source, either a klass or the instance itself
// functions are searched for in order: instance, parent klass, inherited klasses
// may return multiple values with differing return types, however there can only be one per type
// nfuncs tells how many were found, if nfuncs is zero, the function was not found and NULL is returned
// the returned array should be freed if non-NULL, but not the actual array elements
// c.f transforms for intent
const lives_func_info_t **get_widget_funcs_for_instance(lives_widget_instance_t *,
    lives_intention intent, int *nfuncs); // TODO

void widget_func_void(lives_widget_instance_t *, lives_intention intent);
int widget_func_int(lives_widget_instance_t *, lives_intention intent);
double widget_func_double(lives_widget_instance_t *, lives_intention intent, ...);
int widget_func_boolean(lives_widget_instance_t *, lives_intention intent, ...);
int64_t widget_func_int64(lives_widget_instance_t *, lives_intention intent);
char *widget_func_string(lives_widget_instance_t *, lives_intention intent);
lives_funcptr_t widget_func_funcptr(lives_widget_instance_t *, lives_intention intent);
void *widget_func_voidptr(lives_widget_instance_t *, lives_intention intent);
weed_plantptr_t widget_func_plantptr(lives_widget_instance_t *, lives_intention intent);

boolean lives_widget_klass_show_info(const lives_widget_klass_t *);

#endif

#endif

