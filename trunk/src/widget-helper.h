// widget-helper.h
// LiVES
// (c) G. Finch 2012 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H


#ifndef M_PI
#define M_PI 3.1415926536
#endif

#define LIVES_HAS_GRID_WIDGET 0
#define LIVES_HAS_IMAGE_MENU_ITEM 0
#define LIVES_HAS_DEVICE_MANAGER 0

typedef enum {
  LIVES_DISPLAY_TYPE_UNKNOWN=0,
  LIVES_DISPLAY_TYPE_X11,
  LIVES_DISPLAY_TYPE_WIN32
} lives_display_t;


#define W_PACKING_WIDTH  10 // packing width for widgets with labels
#define W_PACKING_HEIGHT 10 // packing height for widgets
#define W_BORDER_WIDTH   10 // default border width

#define ulong_random() lives_random()

#if defined (GUI_GTK) || defined (PAINTER_CAIRO)
#include "widget-helper-gtk.h"
#endif

// basic functions (wrappers for Toolkit functions)

// lives_painter_functions

lives_painter_t *lives_painter_create(lives_painter_surface_t *target);
lives_painter_t *lives_painter_create_from_widget(LiVESWidget *);
boolean lives_painter_set_source_pixbuf(lives_painter_t *, const LiVESPixbuf *, double pixbuf_x, double pixbuf_y);
boolean lives_painter_set_source_surface(lives_painter_t *, lives_painter_surface_t *, double x, double y);
lives_painter_surface_t *lives_painter_image_surface_create(lives_painter_format_t format, int width, int height);
lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data, lives_painter_format_t,
    int width, int height, int stride);
lives_painter_surface_t *lives_painter_surface_create_from_widget(LiVESWidget *, lives_painter_content_t,
    int width, int height);
boolean lives_painter_surface_flush(lives_painter_surface_t *);

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

boolean widget_helper_init(void);

boolean widget_color_to_lives_rgba(lives_colRGBA64_t *, LiVESWidgetColor *);


// object funcs.

livespointer lives_object_ref(livespointer); ///< increase refcount by one
boolean lives_object_unref(livespointer); ///< decrease refcount by one: if refcount==0, object is destroyed

// remove any "floating" reference and add a new ref
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
livespointer lives_object_ref_sink(livespointer);
#else
void lives_object_ref_sink(livespointer);
#endif
#else
livespointer lives_object_ref_sink(livespointer);
#endif


// lives_pixbuf functions

int lives_pixbuf_get_width(const LiVESPixbuf *);
int lives_pixbuf_get_height(const LiVESPixbuf *);
boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *);
int lives_pixbuf_get_rowstride(const LiVESPixbuf *);
int lives_pixbuf_get_n_channels(const LiVESPixbuf *);
unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *);
const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height);
LiVESPixbuf *lives_pixbuf_new_from_data(const unsigned char *buf, boolean has_alpha, int width, int height,
                                        int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn,
                                        livespointer destroy_fn_data);

LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error);
LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf *);
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, boolean preserve_aspect_ratio,
    LiVESError **error);


LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height,
                                       LiVESInterpType interp_type);

boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf *src, LiVESPixbuf *dest, float saturation, boolean pixilate);

// basic widget fns (TODO - amend all void to return boolean)


#ifdef GUI_GTK

#define lives_signal_connect(instance, detailed_signal, c_handler, data) g_signal_connect(instance, detailed_signal, c_handler, data)
#define lives_signal_connect_after(instance, detailed_signal, c_handler, data) g_signal_connect_after(instance, detailed_signal, c_handler, data)
#define lives_signal_handlers_block_by_func(instance, func, data) g_signal_handlers_block_by_func(instance, func, data)
#define lives_signal_handlers_unblock_by_func(instance, func, data) g_signal_handlers_unblock_by_func(instance, func, data)
#else
ulong lives_signal_connect(LiVESWidget *widget, const char *signal_name, ulong funcptr, livespointer data);
boolean lives_signal_handlers_block_by_func(livespointer instance, livespointer func, livespointer data);
boolean lives_signal_handlers_unblock_by_func(livespointer instance, livespointer func, livespointer data);
#endif

boolean lives_signal_handler_block(livespointer instance, unsigned long handler_id);
boolean lives_signal_handler_unblock(livespointer instance, unsigned long handler_id);

boolean lives_signal_handler_disconnect(livespointer instance, unsigned long handler_id);
boolean lives_signal_stop_emission_by_name(livespointer instance, const char *detailed_signal);


boolean lives_widget_set_sensitive(LiVESWidget *, boolean state);
boolean lives_widget_get_sensitive(LiVESWidget *);

boolean lives_widget_show(LiVESWidget *);
boolean lives_widget_show_all(LiVESWidget *);
boolean lives_widget_hide(LiVESWidget *);
boolean lives_widget_destroy(LiVESWidget *);

boolean lives_widget_queue_draw(LiVESWidget *);
boolean lives_widget_queue_draw_area(LiVESWidget *, int x, int y, int width, int height);
boolean lives_widget_queue_resize(LiVESWidget *);
boolean lives_widget_set_size_request(LiVESWidget *, int width, int height);

boolean lives_widget_reparent(LiVESWidget *, LiVESWidget *new_parent);

boolean lives_widget_set_app_paintable(LiVESWidget *widget, boolean paintable);

LiVESWidget *lives_event_box_new(void);
boolean lives_event_box_set_above_child(LiVESEventBox *ebox, boolean set);

LiVESWidget *lives_label_new(const char *text);
LiVESWidget *lives_label_new_with_mnemonic(const char *text);

const char *lives_label_get_text(LiVESLabel *);
boolean lives_label_set_text(LiVESLabel *, const char *text);
boolean lives_label_set_text_with_mnemonic(LiVESLabel *, const char *text);

boolean lives_label_set_markup(LiVESLabel *, const char *markup);
boolean lives_label_set_markup_with_mnemonic(LiVESLabel *, const char *markup);

boolean lives_label_set_mnemonic_widget(LiVESLabel *, LiVESWidget *widget);
LiVESWidget *lives_label_get_mnemonic_widget(LiVESLabel *);

boolean lives_label_set_selectable(LiVESLabel *, boolean setting);


LiVESWidget *lives_button_new(void);
LiVESWidget *lives_button_new_from_stock(const char *stock_id, const char *label);
LiVESWidget *lives_button_new_with_label(const char *label);
LiVESWidget *lives_button_new_with_mnemonic(const char *label);

boolean lives_button_set_label(LiVESButton *, const char *label);

boolean lives_button_set_use_underline(LiVESButton *, boolean use);
boolean lives_button_set_relief(LiVESButton *, LiVESReliefStyle);
boolean lives_button_set_image(LiVESButton *, LiVESWidget *image);
boolean lives_button_set_focus_on_click(LiVESButton *, boolean focus);

LiVESWidget *lives_check_button_new(void);
LiVESWidget *lives_check_button_new_with_label(const char *label);

LiVESWidget *lives_radio_button_new(LiVESSList *group);

LiVESWidget *lives_spin_button_new(LiVESAdjustment *, double climb_rate, uint32_t digits);

LiVESResponseType lives_dialog_run(LiVESDialog *);
boolean lives_dialog_response(LiVESDialog *, int response);

boolean lives_widget_set_bg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_fg_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_text_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);
boolean lives_widget_set_base_color(LiVESWidget *, LiVESWidgetState state, const LiVESWidgetColor *);

boolean lives_widget_get_fg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);
boolean lives_widget_get_bg_state_color(LiVESWidget *, LiVESWidgetState state, LiVESWidgetColor *);

boolean lives_color_parse(const char *spec, LiVESWidgetColor *);

LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1orNULL, const LiVESWidgetColor *c2);
boolean lives_widget_color_equal(LiVESWidgetColor *, const LiVESWidgetColor *);

LiVESWidget *lives_event_box_new(void);

LiVESWidget *lives_image_new(void);
LiVESWidget *lives_image_new_from_file(const char *filename);
LiVESWidget *lives_image_new_from_stock(const char *stock_id, LiVESIconSize size);

boolean lives_image_set_from_pixbuf(LiVESImage *, LiVESPixbuf *);
LiVESPixbuf *lives_image_get_pixbuf(LiVESImage *);

LiVESWidget *lives_dialog_get_content_area(LiVESDialog *);
LiVESWidget *lives_dialog_get_action_area(LiVESDialog *);

boolean lives_dialog_add_action_widget(LiVESDialog *, LiVESWidget *, int response_id);

LiVESWidget *lives_window_new(LiVESWindowType wintype);
boolean lives_window_set_title(LiVESWindow *, const char *title);
const char *lives_window_get_title(LiVESWindow *);
boolean lives_window_set_transient_for(LiVESWindow *, LiVESWindow *parent);

boolean lives_window_set_modal(LiVESWindow *, boolean modal);
boolean lives_window_set_deletable(LiVESWindow *, boolean deletable);
boolean lives_window_set_resizable(LiVESWindow *, boolean resizable);
boolean lives_window_set_keep_below(LiVESWindow *, boolean keep_below);
boolean lives_window_set_decorated(LiVESWindow *, boolean decorated);

boolean lives_window_set_default_size(LiVESWindow *, int width, int height);

boolean lives_window_set_screen(LiVESWindow *, LiVESXScreen *);

boolean lives_widget_get_position(LiVESWidget *, int *x, int *y);

boolean lives_window_move(LiVESWindow *, int x, int y);
boolean lives_window_get_position(LiVESWindow *, int *x, int *y);
boolean lives_window_set_position(LiVESWindow *, LiVESWindowPosition pos);
boolean lives_window_resize(LiVESWindow *, int width, int height);
boolean lives_window_present(LiVESWindow *);
boolean lives_window_fullscreen(LiVESWindow *);
boolean lives_window_unfullscreen(LiVESWindow *);
boolean lives_window_maximize(LiVESWindow *);
boolean lives_window_unmaximize(LiVESWindow *);
boolean lives_window_set_hide_titlebar_when_maximized(LiVESWindow *, boolean setting);

boolean lives_window_add_accel_group(LiVESWindow *, LiVESAccelGroup *group);
boolean lives_window_remove_accel_group(LiVESWindow *, LiVESAccelGroup *group);
boolean lives_menu_set_accel_group(LiVESMenu *, LiVESAccelGroup *group);

boolean lives_window_has_toplevel_focus(LiVESWindow *);

LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper,
                                      double step_increment, double page_increment, double page_size);

boolean lives_box_reorder_child(LiVESBox *, LiVESWidget *child, int pos);
boolean lives_box_set_homogeneous(LiVESBox *, boolean homogeneous);
boolean lives_box_set_spacing(LiVESBox *, int spacing);

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

LiVESWidget *lives_hscale_new(LiVESAdjustment *);
LiVESWidget *lives_vscale_new(LiVESAdjustment *);

LiVESWidget *lives_hpaned_new(void);
LiVESWidget *lives_vpaned_new(void);

boolean lives_paned_set_position(LiVESPaned *, int pos);
boolean lives_paned_pack(int where, LiVESPaned *, LiVESWidget *child, boolean resize, boolean shrink);

LiVESWidget *lives_hscrollbar_new(LiVESAdjustment *);
LiVESWidget *lives_vscrollbar_new(LiVESAdjustment *);

LiVESWidget *lives_arrow_new(LiVESArrowType, LiVESShadowType);

LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale);
boolean lives_alignment_set(LiVESAlignment *, float xalign, float yalign, float xscale, float yscale);

LiVESWidget *lives_expander_new_with_mnemonic(const char *label);
LiVESWidget *lives_expander_new(const char *label);
LiVESWidget *lives_expander_get_label_widget(LiVESExpander *expander);

boolean lives_label_set_halignment(LiVESLabel *, float yalign);

LiVESWidget *lives_combo_new(void);
LiVESWidget *lives_combo_new_with_model(LiVESTreeModel *);
LiVESTreeModel *lives_combo_get_model(LiVESCombo *);

boolean lives_combo_append_text(LiVESCombo *, const char *text);
boolean lives_combo_set_entry_text_column(LiVESCombo *, int column);

char *lives_combo_get_active_text(LiVESCombo *) WARN_UNUSED;
boolean lives_combo_set_active_text(LiVESCombo *, const char *text);
boolean lives_combo_set_active_index(LiVESCombo *, int index);
int lives_combo_get_active(LiVESCombo *);
boolean lives_combo_get_active_iter(LiVESCombo *, LiVESTreeIter *);
boolean lives_combo_set_active_iter(LiVESCombo *, LiVESTreeIter *);
boolean lives_combo_set_active_string(LiVESCombo *, const char *active_str);

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


boolean lives_text_view_scroll_mark_onscreen(LiVESTextView *, LiVESTextMark *mark);


LiVESTextBuffer *lives_text_buffer_new(void);
char *lives_text_buffer_get_text(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end, boolean inc_hidden_chars);
boolean lives_text_buffer_set_text(LiVESTextBuffer *, const char *, int len);

boolean lives_text_buffer_insert(LiVESTextBuffer *, LiVESTextIter *, const char *, int len);
boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer *, const char *, int len);

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

LiVESCellRenderer *lives_cell_renderer_text_new(void);
LiVESCellRenderer *lives_cell_renderer_spin_new(void);
LiVESCellRenderer *lives_cell_renderer_toggle_new(void);
LiVESCellRenderer *lives_cell_renderer_pixbuf_new(void);

LiVESWidget *lives_drawing_area_new(void);

int lives_event_get_time(LiVESXEvent *);

boolean lives_toggle_button_get_active(LiVESToggleButton *);
boolean lives_toggle_button_set_active(LiVESToggleButton *, boolean active);
boolean lives_toggle_button_set_mode(LiVESToggleButton *, boolean drawind);

boolean lives_has_icon(const char *stock_id, LiVESIconSize size);

void lives_tooltips_set(LiVESWidget *, const char *tip_text);

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

boolean lives_container_remove(LiVESContainer *, LiVESWidget *);
boolean lives_container_add(LiVESContainer *, LiVESWidget *);
boolean lives_container_set_border_width(LiVESContainer *, uint32_t width);

boolean lives_container_foreach(LiVESContainer *, LiVESWidgetCallback callback, livespointer cb_data);
LiVESList *lives_container_get_children(LiVESContainer *);
boolean lives_container_set_focus_child(LiVESContainer *, LiVESWidget *child);

LiVESWidget *lives_progress_bar_new(void);
boolean lives_progress_bar_set_fraction(LiVESProgressBar *, double fraction);
boolean lives_progress_bar_set_pulse_step(LiVESProgressBar *, double fraction);
boolean lives_progress_bar_pulse(LiVESProgressBar *);

double lives_spin_button_get_value(LiVESSpinButton *);
int lives_spin_button_get_value_as_int(LiVESSpinButton *);

LiVESAdjustment *lives_spin_button_get_adjustment(LiVESSpinButton *);

boolean lives_spin_button_set_value(LiVESSpinButton *, double value);
boolean lives_spin_button_set_range(LiVESSpinButton *, double min, double max);

boolean lives_spin_button_set_wrap(LiVESSpinButton *, boolean wrap);

boolean lives_spin_button_set_digits(LiVESSpinButton *, uint32_t digits);

boolean lives_spin_button_update(LiVESSpinButton *);

LiVESWidget *lives_color_button_new_with_color(const LiVESWidgetColor *);
boolean lives_color_button_get_color(LiVESColorButton *, LiVESWidgetColor *);
boolean lives_color_button_set_color(LiVESColorButton *, const LiVESWidgetColor *);
boolean lives_color_button_set_title(LiVESColorButton *, const char *title);
boolean lives_color_button_set_use_alpha(LiVESColorButton *, boolean use_alpha);

LiVESToolItem *lives_tool_button_new(LiVESWidget *icon_widget, const char *label);
LiVESToolItem *lives_tool_item_new(void);
boolean lives_tool_button_set_icon_widget(LiVESToolButton *, LiVESWidget *icon);
boolean lives_tool_button_set_label_widget(LiVESToolButton *, LiVESWidget *label);
boolean lives_tool_button_set_use_underline(LiVESToolButton *, boolean use_underline);

LiVESWidget *lives_message_dialog_new(LiVESWindow *parent, LiVESDialogFlags flags, LiVESMessageType type, LiVESButtonsType buttons,
                                      const char *msg_fmt, ...);

double lives_ruler_get_value(LiVESRuler *);
double lives_ruler_set_value(LiVESRuler *, double value);

void lives_ruler_set_range(LiVESRuler *, double lower, double upper, double position, double max_size);
double lives_ruler_set_upper(LiVESRuler *, double value);
double lives_ruler_set_lower(LiVESRuler *, double value);

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
double lives_adjustment_get_value(LiVESAdjustment *);

boolean lives_adjustment_set_upper(LiVESAdjustment *, double upper);
boolean lives_adjustment_set_lower(LiVESAdjustment *, double lower);
boolean lives_adjustment_set_page_size(LiVESAdjustment *, double page_size);
boolean lives_adjustment_set_value(LiVESAdjustment *, double value);

boolean lives_adjustment_clamp_page(LiVESAdjustment *, double lower, double upper);

LiVESAdjustment *lives_range_get_adjustment(LiVESRange *);
boolean lives_range_set_value(LiVESRange *, double value);
boolean lives_range_set_range(LiVESRange *, double min, double max);
boolean lives_range_set_increments(LiVESRange *, double step, double page);
boolean lives_range_set_inverted(LiVESRange *, boolean invert);

double lives_range_get_value(LiVESRange *);

boolean lives_editable_set_editable(LiVESEditable *, boolean editable);
boolean lives_editable_select_region(LiVESEditable *, int start_pos, int end_pos);


LiVESWidget *lives_entry_new(void);
boolean lives_entry_set_editable(LiVESEntry *, boolean editable);
const char *lives_entry_get_text(LiVESEntry *);
boolean lives_entry_set_text(LiVESEntry *, const char *text);
boolean lives_entry_set_width_chars(LiVESEntry *, int nchars);
boolean lives_entry_set_max_length(LiVESEntry *, int len);
boolean lives_entry_set_activates_default(LiVESEntry *, boolean act);
boolean lives_entry_set_visibility(LiVESEntry *, boolean vis);
boolean lives_entry_set_has_frame(LiVESEntry *, boolean has);

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

LiVESWidget *lives_frame_new(const char *label);
boolean lives_frame_set_label(LiVESFrame *, const char *label);
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
LiVESWidget *lives_menu_item_new_with_mnemonic(const char *label);
LiVESWidget *lives_menu_item_new_with_label(const char *label);

LiVESWidget *lives_check_menu_item_new_with_mnemonic(const char *label);
LiVESWidget *lives_check_menu_item_new_with_label(const char *label);
boolean lives_check_menu_item_set_draw_as_radio(LiVESCheckMenuItem *, boolean setting);

LiVESWidget *lives_radio_menu_item_new_with_label(LiVESSList *group, const char *label);
LiVESWidget *lives_image_menu_item_new_with_label(const char *label);
LiVESWidget *lives_image_menu_item_new_with_mnemonic(const char *label);
LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group);

LiVESToolItem *lives_menu_tool_button_new(LiVESWidget *icon, const char *label);
boolean lives_menu_tool_button_set_menu(LiVESMenuToolButton *, LiVESWidget *menu);


#if !GTK_CHECK_VERSION(3,10,0)

boolean lives_image_menu_item_set_image(LiVESImageMenuItem *, LiVESWidget *image);

#endif

boolean lives_menu_item_set_submenu(LiVESMenuItem *, LiVESWidget *);

boolean lives_menu_item_activate(LiVESMenuItem *);

boolean lives_check_menu_item_set_active(LiVESCheckMenuItem *, boolean state);
boolean lives_check_menu_item_get_active(LiVESCheckMenuItem *);

boolean lives_menu_set_title(LiVESMenu *, const char *title);


int lives_display_get_n_screens(LiVESXDisplay *);


char *lives_file_chooser_get_filename(LiVESFileChooser *);
LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser *);

boolean lives_widget_grab_focus(LiVESWidget *);
boolean lives_widget_grab_default(LiVESWidget *);

boolean lives_widget_set_tooltip_text(LiVESWidget *, const char *text);

boolean lives_widget_process_updates(LiVESWidget *, boolean upd_children);
boolean lives_xwindow_process_all_updates(void);

LiVESAccelGroup *lives_accel_group_new(void);
boolean lives_accel_group_connect(LiVESAccelGroup *, uint32_t key, LiVESXModifierType mod, LiVESAccelFlags flags,
                                  LiVESWidgetClosure *closure);
boolean lives_accel_group_disconnect(LiVESAccelGroup *, LiVESWidgetClosure *closure);
boolean lives_accel_groups_activate(LiVESObject *object, uint32_t key, LiVESXModifierType mod);

boolean lives_widget_add_accelerator(LiVESWidget *, const char *accel_signal, LiVESAccelGroup *accel_group,
                                     uint32_t accel_key, LiVESXModifierType accel_mods, LiVESAccelFlags accel_flags);

boolean lives_widget_get_pointer(LiVESXDevice *, LiVESWidget *, int *x, int *y);
LiVESXWindow *lives_display_get_window_at_pointer(LiVESXDevice *, LiVESXDisplay *, int *win_x, int *win_y);
boolean lives_display_get_pointer(LiVESXDevice *, LiVESXDisplay *, LiVESXScreen **, int *x, int *y, LiVESXModifierType *mask);
boolean lives_display_warp_pointer(LiVESXDevice *, LiVESXDisplay *, LiVESXScreen *, int x, int y);

LiVESXDisplay *lives_widget_get_display(LiVESWidget *);
lives_display_t lives_widget_get_display_type(LiVESWidget *);

uint64_t lives_widget_get_xwinid(LiVESWidget *, const char *failure_msg);


LiVESWidget *lives_scrolled_window_new(LiVESAdjustment *hadj, LiVESAdjustment *vadj);
LiVESAdjustment *lives_scrolled_window_get_hadjustment(LiVESScrolledWindow *);
LiVESAdjustment *lives_scrolled_window_get_vadjustment(LiVESScrolledWindow *);

boolean lives_scrolled_window_set_policy(LiVESScrolledWindow *, LiVESPolicyType hpolicy, LiVESPolicyType vpolicy);
boolean lives_scrolled_window_add_with_viewport(LiVESScrolledWindow *, LiVESWidget *child);

boolean lives_xwindow_raise(LiVESXWindow *);
boolean lives_xwindow_set_cursor(LiVESXWindow *, LiVESXCursor *);

uint32_t lives_timer_add(uint32_t interval, LiVESWidgetSourceFunc function, livespointer data);
boolean lives_timer_remove(uint32_t timer);

boolean lives_source_remove(ulong handle);

uint32_t lives_accelerator_get_default_mod_mask();

int lives_screen_get_width(LiVESXScreen *);
int lives_screen_get_height(LiVESXScreen *);

boolean lives_scale_set_draw_value(LiVESScale *, boolean draw_value);
boolean lives_scale_set_value_pos(LiVESScale *, LiVESPositionType ptype);
boolean lives_scale_set_digits(LiVESScale *, int digits);

// optional (return TRUE if implemented)

boolean lives_dialog_set_has_separator(LiVESDialog *, boolean has);
boolean lives_widget_set_hexpand(LiVESWidget *, boolean state);
boolean lives_widget_set_vexpand(LiVESWidget *, boolean state);
boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem *, boolean show);
boolean lives_scale_button_set_orientation(LiVESScaleButton *, LiVESOrientation orientation);
boolean lives_window_set_auto_startup_notification(boolean set);




// compound functions (composed of basic functions)

LiVESWidget *lives_standard_label_new(const char *text);
LiVESWidget *lives_standard_label_new_with_mnemonic(const char *text, LiVESWidget *mnemonic_widget);

LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, const char *tooltip);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup,
    LiVESBox *, const char *tooltip);
LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min,
    double max, double step, double page, int dp, LiVESBox *,
    const char *tooltip);
LiVESWidget *lives_standard_combo_new(const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *,
                                      const char *tooltip);

LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, const char *txt, int dispwidth, int maxchars, LiVESBox *,
                                      const char *tooltip);

LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons, int width, int height);

LiVESWidget *lives_standard_hruler_new(void);

LiVESWidget *lives_standard_scrolled_window_new(int width, int height, LiVESWidget *child);

LiVESWidget *lives_standard_expander_new(const char *label, boolean use_mnemonic, LiVESBox *parent, LiVESWidget *child);

LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment *, double volume);

LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir);

LiVESWidget *lives_standard_color_button_new(LiVESBox *parent, char *name, boolean use_mnemonic, boolean use_alpha, lives_colRGBA64_t *rgba,
    \
    LiVESWidget **sb_red, LiVESWidget **sb_green, LiVESWidget **sb_blue, LiVESWidget **sb_alpha);


LiVESXCursor *lives_cursor_new_from_pixbuf(LiVESXDisplay *, LiVESPixbuf *, int x, int y);

// util functions

void lives_widget_apply_theme(LiVESWidget *, LiVESWidgetState state); // normal theme colours
void lives_widget_apply_theme2(LiVESWidget *, LiVESWidgetState state); // menu and bars colours (bg only...)

boolean global_recent_manager_add(const char *file_name);

void lives_cursor_unref(LiVESXCursor *cursor);

boolean lives_widget_context_update(void);

LiVESWidget *lives_menu_add_separator(LiVESMenu *menu);

boolean lives_widget_get_fg_color(LiVESWidget *, LiVESWidgetColor *);

void lives_window_center(LiVESWindow *);

boolean lives_entry_set_completion_from_list(LiVESEntry *, LiVESList *);

void lives_widget_unparent(LiVESWidget *);

void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source);

char *lives_text_view_get_text(LiVESTextView *);
boolean lives_text_view_set_text(LiVESTextView *, const char *text, int len);


boolean lives_text_buffer_insert_at_end(LiVESTextBuffer *, const char *text);
boolean lives_text_view_scroll_onscreen(LiVESTextView *);




void lives_general_button_clicked(LiVESButton *, livespointer data_to_free);

void lives_spin_button_configure(LiVESSpinButton *, double value, double lower, double upper,
                                 double step_increment, double page_increment);



size_t calc_spin_button_width(double min, double max, int dp);

int get_box_child_index(LiVESBox *, LiVESWidget *child);

boolean label_act_toggle(LiVESWidget *, LiVESXEventButton *, LiVESToggleButton *);
boolean widget_act_toggle(LiVESWidget *, LiVESToggleButton *);

void toggle_button_toggle(LiVESToggleButton *);

// must retain this fn prototype as a callback
void set_child_colour(LiVESWidget *widget, livespointer set_all);

void funkify_dialog(LiVESWidget *dialog);

void unhide_cursor(LiVESXWindow *);
void hide_cursor(LiVESXWindow *);

void get_border_size(LiVESWidget *win, int *bx, int *by);

LiVESWidget *add_hsep_to_box(LiVESBox *);
LiVESWidget *add_vsep_to_box(LiVESBox *);

LiVESWidget *add_fill_to_box(LiVESBox *);

#endif // cplusplus

#define LIVES_JUSTIFY_DEFAULT (widget_opts.default_justify)

#define W_MAX_FILLER_LEN 65535


typedef enum {
  LIVES_CURSOR_NORMAL=0,  ///< must be zero
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

typedef enum {
  LIVES_EXPAND_DEFAULT,
  LIVES_EXPAND_NONE,
  LIVES_EXPAND_EXTRA
} lives_expand_t;


typedef struct {
  boolean no_gui; // show nothing !
  boolean swap_label; // swap label/widget position
  boolean pack_end;
  boolean line_wrap; // line wrapping for labels
  boolean non_modal; // non-modal for dialogs
  boolean expand; // whether spin,check,radio buttons should expand
  boolean apply_theme; // whether to apply theming to widget
  double scale; // scale factor for all sizes
  int packing_width; // default should be W_PACKING_WIDTH
  int packing_height; // default should be W_PACKING_HEIGHT
  int border_width; // default should be W_BORDER_WIDTH
  int filler_len; // length of extra "fill" between widgets
  LiVESWidget *last_label; // label widget of last standard widget (spin,radio,check,entry,combo) [readonly]
  LiVESJustification justify; // justify for labels
  LiVESJustification default_justify;
} widget_opts_t;


widget_opts_t widget_opts;

#ifdef NEED_DEF_WIDGET_OPTS

const widget_opts_t def_widget_opts = {
  FALSE, // no_gui
  FALSE, // swap_label
  FALSE, //pack_end
  FALSE, // line_wrap
  FALSE, // non_modal
  LIVES_EXPAND_DEFAULT, // default expand
  FALSE, // no themeing
  1.0, // default scale
  W_PACKING_WIDTH, // def packing width
  W_PACKING_HEIGHT, // def packing height
  W_BORDER_WIDTH, // def border width
  8, // def fill width (in chars)
  NULL, // last_label
  LIVES_JUSTIFY_LEFT, // justify
  LIVES_JUSTIFY_LEFT // default justify
};

#else

extern const widget_opts_t def_widget_opts;

//#endif

#endif

